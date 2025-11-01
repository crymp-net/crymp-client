/*************************************************************************
Crytek Source File
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 12:04:2006   17:22 : Created by Márcio Martins
- 18:02:2007	 13:30 : Refactored Offhand by Benito G.R.

*************************************************************************/

#include "CryCommon/CrySystem/ISystem.h"
#include "OffHand.h"
#include "CryGame/Actors/Actor.h"
#include "FireModes/Throw.h"
#include "CryGame/GameRules.h"
#include "CryCommon/CryAction/IWorldQuery.h"
#include "Fists.h"
#include "CryGame/GameActions.h"
#include "FireModes/Melee.h"

#include "CryGame/HUD/HUD.h"
#include "CryGame/HUD/HUDCrosshair.h"
#include "WeaponSystem.h"
#include "Projectile.h"
#include "CryGame/GameCVars.h"

#include "CryGame/HUD/HUDSilhouettes.h"
#include <CryCommon/Cry3DEngine/IIndexedMesh.h>

//Sounds tables
namespace
{
	const char gChokeSoundsTable[MAX_CHOKE_SOUNDS][64] =
	{
		"Languages/dialog/ai_korean_soldier_1/choke_01.mp3",
		"Languages/dialog/ai_korean_soldier_2/choke_02.mp3",
		"Languages/dialog/ai_korean_soldier_1/choke_03.mp3",
		"Languages/dialog/ai_korean_soldier_3/choke_04.mp3",
		"Languages/dialog/ai_korean_soldier_1/choke_05.mp3"
	};
	const char gDeathSoundsTable[MAX_CHOKE_SOUNDS][64] =
	{
		"Languages/dialog/ai_korean_soldier_1/choke_grab_00.mp3",
		"Languages/dialog/ai_korean_soldier_1/choke_grab_01.mp3",
		"Languages/dialog/ai_korean_soldier_1/choke_grab_02.mp3",
		"Languages/dialog/ai_korean_soldier_1/choke_grab_03.mp3",
		"Languages/dialog/ai_korean_soldier_1/choke_grab_04.mp3"
	};
}

static void DumpOffhandHelper_CE2(IEntity* pOwnerEnt, int fpSlot, const char* helperName)
{
	if (!pOwnerEnt) return;

	const Matrix34 ownerW = pOwnerEnt->GetWorldTM();

	CryLogAlways("[$3Helper Dump Starting$1]");

	SEntitySlotInfo info;
	if (!pOwnerEnt->GetSlotInfo(fpSlot, info))
	{
		CryLogAlways("[FPHelper] slot %d not found", fpSlot);
		return;
	}

	Matrix34 helperW = Matrix34(IDENTITY);
	bool ok = false;

	if (info.pCharacter)
	{
		ICharacterInstance* pChar = info.pCharacter;
		ISkeletonPose* pose = pChar->GetISkeletonPose();
		if (!pose) { CryLogAlways("[FPHelper] no pose"); return; }

		const int16 jid = pose->GetJointIDByName(helperName);
		if (jid < 0) { CryLogAlways("[FPHelper] joint '%s' not found", helperName); return; }

		const QuatT rel = pose->GetRelJointByID(jid);  // local to parent bone
		const QuatT abs = pose->GetAbsJointByID(jid);  // character-local

		const Matrix34 helperChar(abs);                // character-local
		helperW = ownerW * helperChar;                 // world

		const Ang3 relA(rel.q), absA(abs.q);
		CryLogAlways("[FPHelper/JNT '%s'] "
			"REL.t=(%.3f,%.3f,%.3f) REL.r=(%.1f,%.1f,%.1f)  "
			"ABS.t=(%.3f,%.3f,%.3f) ABS.r=(%.1f,%.1f,%.1f)  "
			"WORLD.t=(%.3f,%.3f,%.3f)",
			helperName,
			rel.t.x, rel.t.y, rel.t.z, RAD2DEG(relA.x), RAD2DEG(relA.y), RAD2DEG(relA.z),
			abs.t.x, abs.t.y, abs.t.z, RAD2DEG(absA.x), RAD2DEG(absA.y), RAD2DEG(absA.z),
			helperW.GetTranslation().x, helperW.GetTranslation().y, helperW.GetTranslation().z);

		ok = true;
	}
	else if (info.pStatObj)
	{
		IStatObj* so = info.pStatObj;
		const Matrix34 slotLocal = pOwnerEnt->GetSlotLocalTM(fpSlot, false);
		const Matrix34 hLocal = so->GetHelperTM(helperName);     // mesh local
		const Matrix34 helperEntLocal = slotLocal * hLocal;         // entity-local
		helperW = ownerW * helperEntLocal;                          // world

		CryLogAlways("[FPHelper/CGF '%s'] WORLD.t=(%.3f,%.3f,%.3f)",
			helperName,
			helperW.GetTranslation().x, helperW.GetTranslation().y, helperW.GetTranslation().z);

		ok = true;
	}

	if (!ok) { CryLogAlways("[FPHelper] no character/statobj in slot %d", fpSlot); return; }

	// Basis sanity + draw axes
	const Matrix33 R(helperW);
	const float c0 = R.GetColumn0().GetLength();
	const float c1 = R.GetColumn1().GetLength();
	const float c2 = R.GetColumn2().GetLength();
	CryLogAlways("[FPHelper '%s'] col lens: X=%.3f Y=%.3f Z=%.3f (should be ~1)", helperName, c0, c1, c2);

	if (IPersistantDebug* pd = g_pGame->GetIGameFramework()->GetIPersistantDebug())
	{
		const Vec3 p0 = helperW.GetTranslation();
		const float L = 0.3f; // axis length
		pd->Begin("FPHelperAxes", true);
		pd->AddLine(p0, p0 + R.GetColumn0() * L, ColorF(1, 0, 0, 1), 3.0f); // X red
		pd->AddLine(p0, p0 + R.GetColumn1() * L, ColorF(0, 1, 0, 1), 3.0f); // Y green
		pd->AddLine(p0, p0 + R.GetColumn2() * L, ColorF(0, 0, 1, 1), 3.0f); // Z blue
	}
}

// Call this once while IN FIRST-PERSON (e.g., right after pickup/equip)
// pPlayerEnt = owner/character entity (slot 0), this = offhand item entity
void COffHand::DumpFallbackConstants(IEntity* pPlayerEnt, const char* tpParentJoint /* e.g. "Bip01 L Hand" */)
{
	IEntity* pItemEnt = GetEntity();
	if (!pItemEnt || !pPlayerEnt) return;

	ICharacterInstance* fpChar = pItemEnt->GetCharacter(eIGS_FirstPerson);
	ICharacterInstance* tpChar = pPlayerEnt->GetCharacter(0);
	if (!fpChar || !tpChar) return;

	ISkeletonPose* fpPose = fpChar->GetISkeletonPose();
	ISkeletonPose* tpPose = tpChar->GetISkeletonPose();
	if (!fpPose || !tpPose) return;

	const int jidHelper = fpPose->GetJointIDByName("item_attachment");
	const int jidTP = tpPose->GetJointIDByName(tpParentJoint);
	if (jidHelper < 0 || jidTP < 0) return;

	// --- ABS helper (character-local) ---
	const QuatT absH = fpPose->GetAbsJointByID(jidHelper);
	const Ang3  absEuler = Ang3(absH.q);
	CryLogAlways("[ABS helper] t=(%.6f, %.6f, %.6f)  rXYZdeg=(%.6f, %.6f, %.6f)",
		absH.t.x, absH.t.y, absH.t.z,
		RAD2DEG(absEuler.x), RAD2DEG(absEuler.y), RAD2DEG(absEuler.z));

	// --- slotLocal (FP slot local TM, applyParentTM=false) ---
	const Matrix34 slotLocal = pItemEnt->GetSlotLocalTM(eIGS_FirstPerson, false);
	const Quat     slotQ = Quat(Matrix33(slotLocal));
	const Ang3     slotE = Ang3(slotQ);
	const Vec3     slotT = slotLocal.GetTranslation();
	CryLogAlways("[slotLocal FP] t=(%.6f, %.6f, %.6f)  rXYZdeg=(%.6f, %.6f, %.6f)",
		slotT.x, slotT.y, slotT.z,
		RAD2DEG(slotE.x), RAD2DEG(slotE.y), RAD2DEG(slotE.z));

	// --- Build helperW exactly like FP path (includes m_holdScale on helper) ---
	const Matrix34 itemWorld = pItemEnt->GetWorldTM();
	Matrix34       helperEL = Matrix34(absH);         // entity-local
	Matrix34       helperW = itemWorld * (slotLocal * helperEL); // world
	helperW.Scale(m_holdScale);

	// --- TP parent joint in WORLD ---
	const Matrix34 playerWorld = pPlayerEnt->GetWorldTM();
	const Matrix34 jointW = playerWorld * Matrix34(tpPose->GetAbsJointByID(jidTP));

	// --- HelperFromJoint (this is what you should hardcode for fallback) ---
	const Matrix34 helperFromJoint = jointW.GetInverted() * helperW;
	const Quat     hfQ = Quat(Matrix33(helperFromJoint));
	const Ang3     hfE = Ang3(hfQ);
	const Vec3     hfT = helperFromJoint.GetTranslation();
	CryLogAlways("[HelperFromJoint] t=(%.6f, %.6f, %.6f)  rXYZdeg=(%.6f, %.6f, %.6f)",
		hfT.x, hfT.y, hfT.z,
		RAD2DEG(hfE.x), RAD2DEG(hfE.y), RAD2DEG(hfE.z));

	// (Optional) sanity: column lengths of helperW basis
	const Matrix33 Rw(helperW);
	CryLogAlways("[helperW basis lens] X=%.3f Y=%.3f Z=%.3f",
		Rw.GetColumn0().GetLength(), Rw.GetColumn1().GetLength(), Rw.GetColumn2().GetLength());
}


static const char* SOTypeToStr(EStaticSubObjectType t)
{
	switch (t)
	{
	case STATIC_SUB_OBJECT_MESH:        return "MESH";
	case STATIC_SUB_OBJECT_HELPER_MESH: return "HELPER_MESH";
	case STATIC_SUB_OBJECT_POINT:       return "POINT";
	case STATIC_SUB_OBJECT_DUMMY:       return "DUMMY";
	case STATIC_SUB_OBJECT_XREF:        return "XREF";
	case STATIC_SUB_OBJECT_CAMERA:      return "CAMERA";
	case STATIC_SUB_OBJECT_LIGHT:       return "LIGHT";
	default:                            return "UNKNOWN";
	}
}

static void LogMatrix(const Matrix34& m, const char* label, int indent)
{
	QuatT qt(m);
	Ang3  euler = Ang3::GetAnglesXYZ(Matrix33(qt.q)); // radians
	const float r2d = 180.0f / gf_PI;
	CryLogAlways("%*s%s: pos(%.4f, %.4f, %.4f) rotXYZ(%.2f, %.2f, %.2f) scale(%.4f, %.4f, %.4f)",
		indent, "", label,
		qt.t.x, qt.t.y, qt.t.z,
		euler.x * r2d, euler.y * r2d, euler.z * r2d,
		m.GetColumn0().GetLength(), m.GetColumn1().GetLength(), m.GetColumn2().GetLength());
}

static void LogRenderMeshInfo(IRenderMesh* rm, int indent)
{
	if (!rm)
	{
		CryLogAlways("%*sRenderMesh: <none>", indent, "");
		return;
	}

#if 1 // keep simple; some CE2 builds don’t expose all stats
	// The following are common in CE2; if unavailable, comment them out.
	const int vtxCount = rm->GetVertCount();
	const int idxCount = rm->GetSysIndicesCount();
	CryLogAlways("%*sRenderMesh: vtx=%d idx=%d", indent, "", vtxCount, idxCount);
#else
	CryLogAlways("%*sRenderMesh: <present>", indent, "");
#endif
}

static void LogStatObjBasics(IStatObj* so, int indent)
{
	if (!so) { CryLogAlways("%*s<null IStatObj>", indent, ""); return; }

#if 1
	AABB aabb = so->GetAABB();
	CryLogAlways("%*sAABB: min(%.3f, %.3f, %.3f) max(%.3f, %.3f, %.3f)",
		indent, "", aabb.min.x, aabb.min.y, aabb.min.z, aabb.max.x, aabb.max.y, aabb.max.z);
#endif

#if 1 // Optional: print source path/geom name if available in your SDK
	if (const char* fp = so->GetFilePath())
		CryLogAlways("%*sFile: %s", indent, "", fp);
	if (const char* gn = so->GetGeoName())
		CryLogAlways("%*sGeoName: %s", indent, "", gn);
#endif

	LogRenderMeshInfo(so->GetRenderMesh(), indent);
}

void DumpStatObj(IStatObj* root, const char* tag = "StatObj")
{
	if (!root) { CryLogAlways("[DumpStatObj] %s: <null>", tag); return; }

	CryLogAlways("====== DumpStatObj: %s ======", tag);
	LogStatObjBasics(root, 0);

	// Top-level user properties on the mesh (key=value;key=value)
	if (const char* props = root->GetProperties())
		CryLogAlways("Properties: %s", props);

	const int n = root->GetSubObjectCount();
	CryLogAlways("SubObjects: %d", n);

	// Build children array based on nParent to print a proper tree.
	std::vector<std::vector<int>> children(n);
	std::vector<int>              roots;
	roots.reserve(n);

	for (int i = 0; i < n; ++i)
	{
		IStatObj::SSubObject* so = root->GetSubObject(i);
		if (!so) continue;
		if (so->nParent >= 0 && so->nParent < n)
			children[so->nParent].push_back(i);
		else
			roots.push_back(i);
	}

	// We’ll also log flat for completeness.
	CryLogAlways("-- Flat list --");
	for (int i = 0; i < n; ++i)
	{
		IStatObj::SSubObject* so = root->GetSubObject(i);
		if (!so) continue;

		CryLogAlways("[%02d] type=%s name=\"%s\" parent=%d hidden=%d identity=%d",
			i, SOTypeToStr(so->nType), so->name.c_str(), so->nParent, so->bHidden, so->bIdentityMatrix);

		if (!so->properties.empty())
			CryLogAlways("      properties: %s", so->properties.c_str());

		LogMatrix(so->tm, "tm (world-ish in statobj space)", 6);
		LogMatrix(so->localTM, "localTM (relative to parent)", 6);

		if (so->nType == STATIC_SUB_OBJECT_POINT || so->nType == STATIC_SUB_OBJECT_DUMMY || so->nType == STATIC_SUB_OBJECT_HELPER_MESH)
			CryLogAlways("      helperSize: (%.3f, %.3f, %.3f)", so->helperSize.x, so->helperSize.y, so->helperSize.z);

		// If this subobject has its own statobj (e.g., piece), show its basics too.
		if (so->pStatObj)
		{
			CryLogAlways("      pStatObj: %p", (void*)so->pStatObj);
			LogStatObjBasics(so->pStatObj, 6);
		}
		else
		{
			CryLogAlways("      pStatObj: <null>");
		}
	}

	// Pretty tree print (depth-first)
	std::function<void(int, int)> dfs = [&](int idx, int depth)
		{
			IStatObj::SSubObject* so = root->GetSubObject(idx);
			if (!so) return;

			CryLogAlways("%*s- [%02d] %s \"%s\" hidden=%d",
				depth * 2, "", idx, SOTypeToStr(so->nType), so->name.c_str(), so->bHidden);

			if (!so->properties.empty())
				CryLogAlways("%*s  props: %s", depth * 2, "", so->properties.c_str());

			LogMatrix(so->localTM, "localTM", depth * 2 + 2);

			if (so->nType == STATIC_SUB_OBJECT_POINT || so->nType == STATIC_SUB_OBJECT_DUMMY || so->nType == STATIC_SUB_OBJECT_HELPER_MESH)
				CryLogAlways("%*s  helperSize: (%.3f, %.3f, %.3f)", depth * 2, "", so->helperSize.x, so->helperSize.y, so->helperSize.z);

			for (int c : children[idx])
				dfs(c, depth + 1);
		};

	CryLogAlways("-- Hierarchy --");
	for (int r : roots) dfs(r, 0);

	CryLogAlways("====== End DumpStatObj: %s ======", tag);
}

//========================Scheduled offhand actions =======================//

//This class help us to select the correct action
struct COffHand::Timer_FinishOffHandAction
{
public:
	Timer_FinishOffHandAction(EOffHandActions _eOHA, COffHand* _pOffHand)
	{
		eOHA = _eOHA;
		pOffHand = _pOffHand;
	}
	void execute(CItem* cItem)
	{
		pOffHand->FinishAction(eOHA);
	}

private:

	EOffHandActions eOHA;
	COffHand* pOffHand;
};

//End finish grenade action (switch/throw)
struct COffHand::Timer_FinishGrenadeAction
{
public:
	Timer_FinishGrenadeAction(COffHand* _pOffHand, CItem* _pMainHand)
	{
		pOffHand = _pOffHand;
		pMainHand = _pMainHand;
	}
	void execute(CItem* cItem)
	{
		//pOffHand->HideItem(true);
		float timeDelay = 0.1f;	//ms

		if (pMainHand && !pMainHand->IsDualWield())
		{
			pMainHand->ResetDualWield();		//I can reset, because if DualWield it's not possible to switch grenades (see PreExecuteAction())
			pMainHand->PlayAction(g_pItemStrings->offhand_off, 0, false, CItem::eIPAF_Default | CItem::eIPAF_NoBlend);
			timeDelay = (pMainHand->GetCurrentAnimationTime(CItem::eIGS_FirstPerson) + 50) * 0.001f;
		}
		else if (!pOffHand->GetOwnerActor()->ShouldSwim())
		{
			if (pMainHand && pMainHand->IsDualWield())
				pMainHand->Select(true);
			else
				pOffHand->GetOwnerActor()->HolsterItem(false);
		}

		if (pOffHand->GetOffHandState() == eOHS_SWITCHING_GRENADE)
		{
			int grenadeType = pOffHand->GetCurrentFireMode();
			pOffHand->AttachGrenadeToHand(grenadeType);
		}

		pOffHand->SetOffHandState(eOHS_TRANSITIONING);

		//Offhand goes back to initial state
		pOffHand->SetResetTimer(timeDelay);
		pOffHand->RequireUpdate(eIUS_General);
	}

private:
	COffHand* pOffHand;
	CItem* pMainHand;
};

struct COffHand::Timer_EnableCollisionsWithOwner
{
public:
	Timer_EnableCollisionsWithOwner(COffHand* _pOffHand, EntityId _entityId)
	{
		pOffHand = _pOffHand;
		entityId = _entityId;
	}
	void execute(CItem* cItem)
	{
		pOffHand->SetIgnoreCollisionsWithOwner(false, entityId);
	}

private:

	COffHand* pOffHand;
	EntityId entityId;
};

//=====================~Scheduled offhand actions======================//

TActionHandler<COffHand> COffHand::s_actionHandler;

COffHand::COffHand()
{
	m_useFPCamSpacePP = false;

	m_holdOffset = Matrix34::CreateIdentity();
	m_lastNPCMatrix = Matrix34::CreateIdentity();
	m_intialBoidLocalMatrix = Matrix34::CreateIdentity();

	// Sound array
	for (int i = 0; i < eOHSound_LastSound; ++i)
		m_sounds[i] = INVALID_SOUNDID;

	RegisterActions();
}

//=============================================================
COffHand::~COffHand()
{
	if (m_heldEntityId)
	{
		RemoveHeldEntityId(m_heldEntityId); //restore collisions immediately
	}
}

//============================================================
void COffHand::RegisterActions()
{
	if (s_actionHandler.GetNumHandlers() == 0)
	{
#define ADD_HANDLER(action, func) s_actionHandler.AddHandler(actions.action, &COffHand::func)
		const CGameActions& actions = g_pGame->Actions();

		ADD_HANDLER(use, OnActionUse);
		ADD_HANDLER(xi_use, OnActionUse);
		ADD_HANDLER(attack1, OnActionAttack);
		ADD_HANDLER(zoom, OnActionDrop);
		ADD_HANDLER(xi_zoom, OnActionDrop);
		ADD_HANDLER(grenade, OnActionThrowGrenade);
		ADD_HANDLER(xi_grenade, OnActionXIThrowGrenade);
		ADD_HANDLER(handgrenade, OnActionSwitchGrenade);
		ADD_HANDLER(xi_handgrenade, OnActionXISwitchGrenade);
		ADD_HANDLER(special, OnActionSpecial);
#undef ADD_HANDLER
	}
}
//=============================================================
void COffHand::Reset()
{
	CWeapon::Reset();

	if (m_heldEntityId)
	{
		//Prevent editor-reset issues
		if (m_currentState & (eOHS_GRABBING_NPC | eOHS_HOLDING_NPC | eOHS_THROWING_NPC))
		{
			ThrowNPC(m_heldEntityId, false);
		}
	}

	m_nextThrowTimer = -1.0f;
	m_lastFireModeId = 0;
	m_pickingTimer = -1.0f;
	m_preHeldEntityId = 0;
	m_constraintId = 0;
	m_resetTimer = -1.0f;
	m_killTimeOut = -1.0f;
	m_killNPC = false;
	m_effectRunning = false;
	m_npcWasDead = false;
	m_grabbedNPCSpecies = eGCT_UNKNOWN;
	m_lastCHUpdate = 0.0f;
	m_heldEntityMass = 0.0f;
	m_prevMainHandId = 0;
	m_constraintStatus = ConstraintStatus::Inactive;
	m_pRockRN = nullptr;
	m_bCutscenePlaying = false;
	m_forceThrow = false;
	m_restoreStateAfterLoading = false;

	DrawSlot(eIGS_Aux0, false); //frag_grenade_tp.cgf	
	DrawSlot(eIGS_Owner, false); //emp_grenade.cgf
	DrawSlot(eIGS_OwnerLooped, false); //flashbang.cgf
	DrawSlot(eIGS_Aux1, false); //smoke_grenade_tp.cgf

	SetOffHandState(eOHS_INIT_STATE);

	for (int i = 0; i < eOHSound_LastSound; i++)
	{
		m_sounds[i] = INVALID_SOUNDID;
	}
}

//=============================================================
void COffHand::PostInit(IGameObject* pGameObject)
{
	CWeapon::PostInit(pGameObject);

	m_lastFireModeId = 0;
	SetCurrentFireMode(0);
	HideItem(true);
}

//============================================================
bool COffHand::ReadItemParams(const IItemParamsNode* root)
{
	if (!CWeapon::ReadItemParams(root))
		return false;

	m_grabTypes.clear();

	//Read offHand grab types
	if (const IItemParamsNode* pickabletypes = root->GetChild("pickabletypes"))
	{
		int n = pickabletypes->GetChildCount();
		for (int i = 0; i < n; ++i)
		{
			const IItemParamsNode* pt = pickabletypes->GetChild(i);

			SGrabType grabType;
			grabType.helper = pt->GetAttribute("helper");
			grabType.pickup = pt->GetAttribute("pickup");
			grabType.idle = pt->GetAttribute("idle");
			grabType.throwFM = pt->GetAttribute("throwFM");

			if (strcmp(pt->GetName(), "onehanded") == 0)
			{
				grabType.twoHanded = false;
				m_grabTypes.push_back(grabType);
			}
			else if (strcmp(pt->GetName(), "twohanded") == 0)
			{
				grabType.twoHanded = true;
				m_grabTypes.push_back(grabType);
			}
		}
	}

	return true;
}
//============================================================
void COffHand::FullSerialize(TSerialize ser)
{
	CWeapon::FullSerialize(ser);

	EntityId oldHeldId = m_heldEntityId;

	ser.Value("m_lastFireModeId", m_lastFireModeId);
	ser.Value("m_usable", m_usable);
	ser.Value("m_currentState", m_currentState);
	ser.Value("m_preHeldEntityId", m_preHeldEntityId);
	ser.Value("m_startPickUp", m_startPickUp);
	ser.Value("m_heldEntityId", m_heldEntityId);
	ser.Value("m_constraintId", m_constraintId);
	ser.Value("m_grabType", m_grabType);
	ser.Value("m_grabbedNPCSpecies", m_grabbedNPCSpecies);
	ser.Value("m_killTimeOut", m_killTimeOut);
	ser.Value("m_effectRunning", m_effectRunning);
	ser.Value("m_grabbedNPCInitialHealth", m_grabbedNPCInitialHealth);
	ser.Value("m_prevMainHandId", m_prevMainHandId);
	ser.Value("m_holdScale", m_holdScale);

	//PATCH ====================
	if (ser.IsReading())
	{
		m_mainHandIsDualWield = false;
	}

	ser.Value("m_mainHandIsDualWield", m_mainHandIsDualWield);

	//============================

	if (ser.IsReading() && m_heldEntityId != oldHeldId)
	{
		IActor* pActor = m_pActorSystem->GetActor(oldHeldId);
		if (pActor)
		{
			if (pActor->GetEntity()->GetCharacter(0))
				pActor->GetEntity()->GetCharacter(0)->SetFlags(pActor->GetEntity()->GetCharacter(0)->GetFlags() & (~ENTITY_SLOT_RENDER_NEAREST));
			if (IEntityRenderProxy* pProxy = (IEntityRenderProxy*)pActor->GetEntity()->GetProxy(ENTITY_PROXY_RENDER))
			{
				if (IRenderNode* pRenderNode = pProxy->GetRenderNode())
					pRenderNode->SetRndFlags(ERF_RENDER_ALWAYS, false);
			}
		}
		else
			DrawNear(false, oldHeldId);
	}
}

//============================================================
bool COffHand::NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags)
{
	return true;
}

//============================================================
void COffHand::PostSerialize()
{
	m_restoreStateAfterLoading = true; //Will be cleared on first update
}

//==========================================================
void COffHand::PostPostSerialize()
{
	RemoveAllAccessories();

	SetCurrentFireMode(m_lastFireModeId);

	bool needsReset = false;

	if (m_currentState & (eOHS_THROWING_OBJECT | eOHS_HOLDING_GRENADE | eOHS_THROWING_GRENADE | eOHS_TRANSITIONING))
	{
		needsReset = true;
	}
	else if (m_currentState == eOHS_SWITCHING_GRENADE)
	{
		m_currentState = eOHS_INIT_STATE;
		ProcessOffHandActions(eOHA_SWITCH_GRENADE, INPUT_DEF, eAAM_OnPress, 1.0f);
	}
	else if (m_currentState == eOHS_PICKING_ITEM)
	{
		//If picking an item...
		if (m_heldEntityId || (m_preHeldEntityId && m_startPickUp))
		{
			if (!m_heldEntityId)
			{
				SetHeldEntityId(m_preHeldEntityId);
			}

			SelectGrabType(m_pEntitySystem->GetEntity(m_heldEntityId));

			CActor* pPlayer = GetOwnerActor();
			if (pPlayer)
			{
				m_currentState = eOHS_PICKING_ITEM2;
				pPlayer->PickUpItem(m_heldEntityId, true);
				SetIgnoreCollisionsWithOwner(false, m_heldEntityId);
			}
		}
		SetOffHandState(eOHS_INIT_STATE);
	}
	else if ((m_heldEntityId || m_preHeldEntityId) && m_currentState != eOHS_TRANSITIONING)
	{
		if (m_preHeldEntityId && !m_heldEntityId && m_startPickUp)
		{
			SetOffHandState(eOHS_PICKING);
			SetHeldEntityId(m_preHeldEntityId);
		}

		if (m_heldEntityId)
		{
			IEntity* pEntity = m_pEntitySystem->GetEntity(m_heldEntityId);
			if (!pEntity)
			{
				CryLogWarning("Offhand held entity did not exist anymore! Perhaps it was a boid ... ");
				needsReset = true;
			}
			else
			{
				SelectGrabType(m_pEntitySystem->GetEntity(m_heldEntityId));

				//If holding an object or NPC
				if (m_currentState & (eOHS_HOLDING_OBJECT | eOHS_PICKING | eOHS_THROWING_OBJECT | eOHS_MELEE))
				{
					//Do grabbing again
					SetOffHandState(eOHS_INIT_STATE);
					m_preHeldEntityId = m_heldEntityId;
					PreExecuteAction(eOHA_USE, eAAM_OnPress, true);

					StartPickUpObject(m_heldEntityId, false);
				}
				else if (m_currentState & (eOHS_HOLDING_NPC | eOHS_GRABBING_NPC | eOHS_THROWING_NPC))
				{
					//Do grabbing again
					SetOffHandState(eOHS_INIT_STATE);
					CActor* pActor = static_cast<CActor*>(m_pActorSystem->GetActor(m_heldEntityId));
					bool isDead = false;
					if (pActor && ((pActor->GetActorStats() && pActor->GetActorStats()->isRagDoll) || pActor->GetHealth() <= 0))
						isDead = true;
					if (isDead)//don't pickup ragdolls
					{
						pActor->Revive(); //will be ragdollized after serialization...
						pActor->SetAnimationInput("Action", "idle");
						pActor->CreateScriptEvent("kill", 0);
						needsReset = true;
					}
					else
					{
						m_preHeldEntityId = m_heldEntityId;
						PreExecuteAction(eOHA_USE, eAAM_OnPress, true);

						StartPickUpObject(m_heldEntityId, true);
						if (pActor)
						{
							pActor->GetAnimatedCharacter()->ForceRefreshPhysicalColliderMode();
						}
					}
				}
			}
		}

	}
	else if (m_currentState != eOHS_INIT_STATE)
	{
		needsReset = true;
	}

	if (needsReset)
	{
		SetMainHand(static_cast<CItem*>(GetOwnerActor()->GetCurrentItem()));
		SetMainHandWeapon(m_mainHand ? static_cast<CWeapon*>(m_mainHand->GetIWeapon()) : nullptr);

		m_mainHandIsDualWield = m_mainHand ? m_mainHand->IsDualWield() : false;
		
		SetOffHandState(eOHS_TRANSITIONING);
		FinishAction(eOHA_RESET);
	}

	if (m_ownerId == LOCAL_PLAYER_ENTITY_ID)
	{
		if (!m_stats.selected)
		{
			SetHand(eIH_Left);					//This will be only done once after loading
			Select(true);	//this can select the wrong crosshair, should be removed if save
			Select(false);
		}
	}

	m_restoreStateAfterLoading = false;
}

//=======================================
void COffHand::OnEnterFirstPerson()
{
	//CryMP: Check 1st/3rd person transition
	CWeapon::OnEnterFirstPerson();

	if (m_heldEntityId)
	{
		AttachObjectToHand(false, m_heldEntityId, false);
		UpdateEntityRenderFlags(m_heldEntityId, EntityFpViewMode::ForceActive);

		//if (m_currentState == eOHS_HOLDING_OBJECT)  //Is this needed?
		//{
		//	GetOwnerActor()->HolsterItem(true);
		//}
	}
}

//=======================================
void COffHand::OnEnterThirdPerson()
{
	//CryMP: Check 1st/3rd person transition
	CWeapon::OnEnterThirdPerson();

	if (m_heldEntityId)
	{
		if (m_stats.fp) //Only if we were in FP	(This function is also called from Select(true) in TP
		{
			AttachObjectToHand(true, m_heldEntityId, false);
			UpdateEntityRenderFlags(m_heldEntityId, EntityFpViewMode::ForceDisable);
		}
	}
}

void COffHand::EnableUpdate(bool enable, int slot)
{
	CWeapon::EnableUpdate(enable, slot);
}

//=============================================================
bool COffHand::CanSelect() const
{
	return false;
}

//=============================================================
void COffHand::Select(bool select)
{
	if (g_pGameCVars->mp_pickupDebug > 2)
		CryLogAlways("COffHand::Select($3%d$1)", select);

	CWeapon::Select(select);
}

//=============================================================
void COffHand::Update(SEntityUpdateContext& ctx, int slot)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	CWeapon::Update(ctx, slot);

	//CryMP
	//This has to be called for remote players when they're in thirdperson,
	//if we are spectating the remote player in firstperson, UpdateFPView will be called from CPlayer::UpdateFpSpectatorView
	//For local client, UpdateFPView is always called, even in thirdperson (from ItemSystem::Update)
	if (gEnv->bClient && !m_stats.fp)
	{
		CActor* pActor = GetOwnerActor();
		if (pActor && pActor->IsRemote())
		{
			SharedUpdate(ctx.fFrameTime);

			if (m_heldEntityId && m_currentState & (eOHS_HOLDING_OBJECT | eOHS_PICKING | eOHS_THROWING_OBJECT | eOHS_PICKING_ITEM | eOHS_MELEE))
			{
				UpdateHeldObject();
			}
		}
	}

	CActor* pOwnerActor = GetOwnerActor();
	if (!pOwnerActor)
		return;

	if (g_pGameCVars->mp_pickupDebug)
	{
		DrawLog("%s m_mainHandWeapon: %s - m_mainHand: %s (%s, %s, %s)", IsSelected() ? "OffHand Selected" : "OffHand Not Selected",
			m_mainHandWeapon ? m_mainHandWeapon->GetEntity()->GetName() : "NULL",
			m_mainHand ? m_mainHand->GetEntity()->GetName() : "NULL",
			IsFirstPersonCharacterMasterHidden() ? "FP Master Hidden" : "", IsArmsHidden() ? "Arms Hidden" : "", GetEntity()->IsHidden() ? "Entity Hidden" : ""
		);
	}
}

//=============================================================
void COffHand::CheckTimers(float frameTime)
{
	if (m_resetTimer >= 0.0f)
	{
		m_resetTimer -= frameTime;
		if (m_resetTimer < 0.0f)
		{
			SetOffHandState(eOHS_INIT_STATE);
			m_resetTimer = -1.0f;
		}
	}
	if (m_nextThrowTimer >= 0.0f)
	{
		//Throw fire rate (grenade, object, NPC)
		m_nextThrowTimer -= frameTime;
	}
	if (m_fGrenadeToggleTimer >= 0.0f)
	{
		m_fGrenadeToggleTimer += frameTime;
		if (m_fGrenadeToggleTimer > 1.0f)
		{
			StartSwitchGrenade(true);
			m_fGrenadeToggleTimer = 0.0f;
		}
	}

	if (m_fGrenadeThrowTimer >= 0.0f)
	{
		m_fGrenadeThrowTimer += frameTime;
		if (m_fGrenadeThrowTimer > 0.5f)
		{
			m_fGrenadeThrowTimer = -1.0f;
		}
	}

	if (m_pickingTimer >= 0.0f)
	{
		m_pickingTimer -= frameTime;

		if (m_pickingTimer < 0.0f)
		{
			PerformPickUp();
		}

	}
	if (m_killTimeOut >= 0.0f)
	{
		m_killTimeOut -= frameTime;
		if (m_killTimeOut < 0.0f)
		{
			m_killTimeOut = -1.0f;
			m_killNPC = true;
		}
	}
}

//=============================================================
void COffHand::SharedUpdate(float frameTime)
{
	CheckTimers(frameTime);

	//CryMP: Update held items in TP mode as well
	//CryMP: Note: this check needs to be here, otherwise no grab anims in FP 
	if (!m_stats.fp && m_heldEntityId)
	{
		UpdateFPPosition(frameTime);
		UpdateFPCharacter(frameTime);
	}
}

//=============================================================
//CryMP: Called always on the client, even in ThirdPerson
//Called on other clients, if spectating them in FirstPerson

void COffHand::UpdateFPView(float frameTime)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	SharedUpdate(frameTime);

	if (m_stats.selected)
	{
		CItem::UpdateFPView(frameTime);

		if (m_fm)
			m_fm->UpdateFPView(frameTime);

	}
	else
	{
		//CryMP need to do this here, not checked in CItem::UpdateFPView if deselected
		CheckViewChange();
	}

	if (m_restoreStateAfterLoading)
		PostPostSerialize();

	m_lastCHUpdate += frameTime;

	if (m_currentState == eOHS_INIT_STATE)
	{
		if (!gEnv->bMultiplayer || (g_pGameCVars->mp_pickupObjects && m_pGameFramework->IsImmersiveMPEnabled()))
			UpdateCrosshairUsabilitySP();
		else
			UpdateCrosshairUsabilityMP();

		m_weaponLowered = false;

		//Fix offhand floating on spawn (not really nice fix...)
		if (m_stats.hand == 0)
		{
			SetHand(eIH_Left); //This will be only done once after loading

			Select(true); Select(false);
		}
		//=========================================================
	}
	else if (m_heldEntityId && m_currentState & (eOHS_HOLDING_OBJECT | eOHS_PICKING | eOHS_THROWING_OBJECT | eOHS_PICKING_ITEM | eOHS_MELEE))
	{

		if (m_usable)
		{
			if (g_pGame->GetHUD())
				g_pGame->GetHUD()->GetCrosshair()->SetUsability(0, "");
			m_usable = false;
		}

		UpdateHeldObject();

		if (m_grabType == GRAB_TYPE_TWO_HANDED)
			UpdateWeaponLowering(frameTime);
		else if (CActor* pActor = GetOwnerActor())
		{
			if (CWeapon* pWeapon = pActor->GetCurrentWeapon(false))
			{
				LowerWeapon(pWeapon->IsWeaponLowered());
			}
		}
	}
}
//============================================================

void COffHand::UpdateCrosshairUsabilitySP()
{

	//Only update a few times per second
	if (m_lastCHUpdate > TIME_TO_UPDATE_CH)
		m_lastCHUpdate = 0.0f;
	else
		return;

	CActor* pActor = GetOwnerActor();
	if (pActor)
	{
		CPlayer* pPlayer = CPlayer::FromActor(pActor);
		bool isLadder = pPlayer->IsLadderUsable();

		const bool onLadder = pPlayer->GetPlayerStats()->isOnLadder.Value();

		const int canGrab = CanPerformPickUp(pActor, NULL);

		if (canGrab || (isLadder && !onLadder))
		{
			if (CHUD* pHUD = g_pGame->GetHUD())
			{
				IItem* pItem = m_pItemSystem->GetItem(m_crosshairId);
				CHUDCrosshair *pHUDCrosshair = pHUD->GetCrosshair();
				if (IActor* pActor = m_pActorSystem->GetActor(m_crosshairId))
					pHUDCrosshair->SetUsability(1, "@grab_enemy");
				else if (isLadder)
					pHUDCrosshair->SetUsability(1, "@use_ladder");
				else if (!pItem)
				{
					pHUDCrosshair->SetUsability(1, "@grab_object");
				}
				else if (pItem)
				{
					CryFixedStringT<128> itemName("@");
					itemName.append(pItem->GetEntity()->GetClass()->GetName());
					if (!strcmp(pItem->GetEntity()->GetClass()->GetName(), "CustomAmmoPickup"))
					{
						SmartScriptTable props;
						if (pItem->GetEntity()->GetScriptTable() && pItem->GetEntity()->GetScriptTable()->GetValue("Properties", props))
						{
							const char* name = NULL;
							props->GetValue("AmmoName", name);
							itemName.assign("@");
							itemName.append(name);
						}
					}

					if (pItem->GetIWeapon())
					{
						IEntityClass* pItemClass = pItem->GetEntity()->GetClass();
						bool isSocom = strcmp(pItemClass->GetName(), "SOCOM") ? false : true;
						IItem* pCurrentItem = m_pItemSystem->GetItem(pPlayer->GetInventory()->GetItemByClass(pItemClass));
						if ((!isSocom && pCurrentItem) ||
							(isSocom && pCurrentItem && pCurrentItem->IsDualWield()))
						{
							if (pItem->CheckAmmoRestrictions(pPlayer->GetEntityId()))
								pHUDCrosshair->SetUsability(1, "@game_take_ammo_from", itemName.c_str());
							else
								pHUDCrosshair->SetUsability(2, "@weapon_ammo_full", itemName.c_str());
						}
						else
						{
							int typ = CanPerformPickUp(GetOwnerActor(), NULL, true);
							if (m_preHeldEntityId && !pPlayer->CheckInventoryRestrictions(m_pEntitySystem->GetEntity(m_preHeldEntityId)->GetClass()->GetName()))
							{
								IItem* pExchangedItem = GetExchangeItem(pPlayer);
								if (pExchangedItem)
								{
									pHUDCrosshair->SetUsability(1, "@game_exchange_weapon",
										pExchangedItem->GetEntity()->GetClass()->GetName(), itemName.c_str());
								}
								else
									pHUDCrosshair->SetUsability(2, "@inventory_full");
							}
							else
							{
								pHUDCrosshair->SetUsability(1, "@pick_weapon", itemName.c_str());
							}
						}
					}
					else
					{
						if (pItem->CheckAmmoRestrictions(pPlayer->GetEntityId()))
							pHUDCrosshair->SetUsability(1, "@pick_item", itemName.c_str());
						else
							pHUDCrosshair->SetUsability(2, "@weapon_ammo_full", itemName.c_str());
					}
				}
			}
			m_usable = true;
		}
		else if (m_usable)
		{
			if (g_pGame->GetHUD())
				g_pGame->GetHUD()->GetCrosshair()->SetUsability(false, "");
			m_usable = false;
		}
	}
}

//==========================================================
void COffHand::UpdateCrosshairUsabilityMP()
{
	//Only update a few times per second
	if (m_lastCHUpdate > TIME_TO_UPDATE_CH)
		m_lastCHUpdate = 0.0f;
	else
		return;

	CPlayer* pPlayer = CPlayer::FromActor(GetOwnerActor());
	if (pPlayer)
	{
		bool isLadder = pPlayer->IsLadderUsable();

		const bool onLadder = pPlayer->GetPlayerStats()->isOnLadder.Value();

		IMovementController* pMC = pPlayer->GetMovementController();
		if (!pMC)
			return;

		SMovementState info;
		pMC->GetMovementState(info);
		if (CheckItemsInProximity(info.eyePosition, info.eyeDirection, false) || (isLadder && !onLadder))
		{
			//Offhand pick ups are disabled in MP (check only for items)
			if (CHUD* pHUD = g_pGame->GetHUD())
			{
				if (isLadder)
				{
					pHUD->GetCrosshair()->SetUsability(1, "@use_ladder");
					m_usable = true;
					return;
				}

				if (CItem* pItem = static_cast<CItem*>(m_pItemSystem->GetItem(m_crosshairId)))
				{
					CryFixedStringT<128> itemName(pItem->GetEntity()->GetClass()->GetName());
					if (!strcmp(itemName.c_str(), "CustomAmmoPickup"))
					{
						SmartScriptTable props;
						if (pItem->GetEntity()->GetScriptTable() && pItem->GetEntity()->GetScriptTable()->GetValue("Properties", props))
						{
							const char* name = NULL;
							props->GetValue("AmmoName", name);
							itemName.assign(name);
						}
					}

					if (pItem->GetIWeapon())
					{
						IEntityClass* pItemClass = pItem->GetEntity()->GetClass();
						const bool isSocom = pItemClass == CItem::sSOCOMClass;
						IItem* pCurrentItem = m_pItemSystem->GetItem(pPlayer->GetInventory()->GetItemByClass(pItemClass));
						if ((!isSocom && pCurrentItem) ||
							(isSocom && pCurrentItem && pCurrentItem->IsDualWield()))
						{
							if (pItem->CheckAmmoRestrictions(pPlayer->GetEntityId()))
								pHUD->GetCrosshair()->SetUsability(1, "@game_take_ammo_from", itemName.c_str());
							else
								pHUD->GetCrosshair()->SetUsability(2, "@weapon_ammo_full", itemName.c_str());
						}
						else
						{
							int typ = CanPerformPickUp(pPlayer, NULL, true);
							if (m_preHeldEntityId && !pPlayer->CheckInventoryRestrictions(m_pEntitySystem->GetEntity(m_preHeldEntityId)->GetClass()->GetName()))
							{
								IItem* pExchangedItem = GetExchangeItem(pPlayer);
								if (pExchangedItem)
								{
									pHUD->GetCrosshair()->SetUsability(1, "@game_exchange_weapon",
										pExchangedItem->GetEntity()->GetClass()->GetName(), itemName.c_str());
								}
								else
									pHUD->GetCrosshair()->SetUsability(2, "@inventory_full");
							}
							else
							{
								pHUD->GetCrosshair()->SetUsability(true, "@pick_weapon", itemName.c_str());
							}
						}
					}
					else
					{
						if (pItem->CheckAmmoRestrictions(pPlayer->GetEntityId()))
							pHUD->GetCrosshair()->SetUsability(1, "@pick_item", itemName.c_str());
						else
							pHUD->GetCrosshair()->SetUsability(2, "@weapon_ammo_full", itemName.c_str());
					}
				}
			}
			m_usable = true;
		}
		else if (m_usable)
		{
			if (g_pGame->GetHUD())
				g_pGame->GetHUD()->GetCrosshair()->SetUsability(0, "");
			m_usable = false;
		}
	}
}

// Members somewhere:
bool     m_hasTPCal = false;
Matrix34 m_HelperFromJoint = Matrix34(IDENTITY);
const char* m_TPParentJoint = "Bip01 Head"; // or your left-hand joint

// Hardcoded ABS (character-local) values you measured:
static inline Vec3  kABS_t() { return Vec3(-0.032f, 0.458f, -0.610f); }
static inline Ang3  kABS_deg() { return Ang3(5.3f, -2.4f, 10.8f); } // X,Y,Z in degrees

//=============================================================
void COffHand::UpdateHeldObject()
{
	if (!gEnv->bClient)
		return;

	CActor* pPlayer = GetOwnerActor();
	if (!pPlayer)
		return;


	EnablePhysPostStep(g_pGameCVars->mp_pickupApplyPhysicsVelocity == 2);

	//TEST
	if (g_pGameCVars->mp_pickupDebug)
	{
		DrawLog("														UpdateHeldObject: m_heldEntityId=%d, m_constraintId=%d, m_constraintStatus=%d", m_heldEntityId, m_constraintId, static_cast<int>(m_constraintStatus));
	}

	const EntityId entityId = m_heldEntityId;

	IEntity* pEntity = m_pEntitySystem->GetEntity(entityId);
	if (!pEntity)
	{
		FinishAction(eOHA_RESET);

		return;
	}

	AwakeEntityPhysics(pEntity);

	//DEBUG CODE

	if (CHUD* pHUD = g_pGame->GetHUD())
	{
		if (pHUD->GetSilhouettes() && g_pGameCVars->mp_pickupDebug > 1)
		{
			bool broken = false;
			IPhysicalEntity* pPE = pEntity->GetPhysics();
			if ((pPE && m_constraintId) ||
				(pPE && !m_constraintId && pPE->GetType() == PE_ARTICULATED))
			{
				pe_status_constraint state;
				state.id = m_constraintId;

				//The constraint was removed (the object was broken/destroyed)
				//m_constraintId == 0 means a boid that died in player hands (and now it's a ragdoll,which could cause collision issues)
				if (!m_constraintId || !pPE->GetStatus(&state))
				{
					broken = true;
				}
			}

			float r = broken ? 1.0f : 0.0f; // Red if broken, otherwise 0
			float g = broken ? 0.0f : 1.0f; // Green if not broken, otherwise 0
			float b = 0.0f; // No blue component
			float a = 1.0f; // Fully opaque
			float fDuration = 1.0f; // Duration of the silhouette effect

			pHUD->GetSilhouettes()->SetSilhouette(pEntity, r, g, b, a, fDuration);
		}
	}

	if (m_constraintStatus == ConstraintStatus::WaitForPhysicsUpdate || m_constraintStatus == ConstraintStatus::Active)
	{
		IPhysicalEntity* pPE = pEntity->GetPhysics();
		if ((pPE && m_constraintId) ||
			(pPE && !m_constraintId && pPE->GetType() == PE_ARTICULATED))
		{
			pe_status_constraint state;
			state.id = m_constraintId;
			if (m_constraintStatus == ConstraintStatus::WaitForPhysicsUpdate)
			{
				if (pPE->GetStatus(&state))
				{
					m_constraintStatus = ConstraintStatus::Active;
				}
				else
					return;
			}
			else if (m_constraintStatus == ConstraintStatus::Active)
			{
				//The constraint was removed (the object was broken/destroyed)
				//means a boid that died in player hands (and now it's a ragdoll,which could cause collision issues)
				if (!pPE->GetStatus(&state))
				{
					m_constraintStatus = ConstraintStatus::Broken;
				}
			}
		}
	}

	if (m_constraintStatus == ConstraintStatus::Broken)
	{
		if (m_currentState & (eOHS_HOLDING_OBJECT | eOHS_THROWING_OBJECT | eOHS_HOLDING_NPC | eOHS_THROWING_NPC)) //CryMP: don't release untill we're actually holding it
		{
			if (pPlayer->IsClient())
			{
				if (m_mainHand && m_mainHand->IsBusy())
				{
					m_mainHand->SetBusy(false);
				}

				if (m_currentState != eOHS_THROWING_OBJECT)
				{
					if (m_currentState == eOHS_MELEE)
					{
						GetScheduler()->Reset();
					}

					SetOffHandState(eOHS_HOLDING_OBJECT);

					OnAction(GetOwnerId(), ActionId("use"), eAAM_OnPress, 0.0f);
					OnAction(GetOwnerId(), ActionId("use"), eAAM_OnRelease, 0.0f);
				}
				else
				{
					OnAction(GetOwnerId(), ActionId("use"), eAAM_OnRelease, 0.0f);
				}
				m_constraintId = 0;
			}
			return;
		}

		if (pPlayer->IsRemote())
		{
			FinishAction(eOHA_RESET);

			return;
		}
	}

	if (!m_stats.fp)
	{
		if (g_pGameCVars->mp_pickupApplyPhysicsVelocity == 1)
		{
			TickCarryPhysics(pEntity);
		}
		else if (g_pGameCVars->mp_pickupApplyPhysicsVelocity == 2)
		{
			ICharacterInstance* pOwnerCharacter = pPlayer->GetEntity()->GetCharacter(0);
			IAttachmentManager* pAttachmentManager = pOwnerCharacter ? pOwnerCharacter->GetIAttachmentManager() : nullptr;
			if (!pAttachmentManager)
				return;

			const char* kAttachmentName = "held_object_attachment";
			IAttachment* pAttachment = pAttachmentManager->GetInterfaceByName(kAttachmentName);
			if (pAttachment)
			{
				CacheHandPose_GameThread(pAttachment);
			}
		}

		return;
	}

	const int id = eIGS_FirstPerson;

	Matrix34 baseRot = Matrix34(GetSlotHelperRotation(id, "item_attachment", true));

	Matrix34 finalMatrix = baseRot;
	finalMatrix.Scale(m_holdScale);

	Vec3 pos = GetSlotHelperPos(id, "item_attachment", true);

	// Predefined offsets
	Vec3 fpPosOffset(ZERO), tpPosOffset(ZERO);
	GetPredefinedPosOffset(pEntity, fpPosOffset, tpPosOffset);

	// Apply fpPosOffset in *camera space* (x→right, y→forward, z→up), using unscaled axes
	if (!fpPosOffset.IsZero(0.0001f))
	{
		const Vec3 viewRight = baseRot.GetColumn0().GetNormalizedSafe(Vec3(1, 0, 0));
		const Vec3 viewFwd = baseRot.GetColumn1().GetNormalizedSafe(Vec3(0, 1, 0));
		const Vec3 viewUp = baseRot.GetColumn2().GetNormalizedSafe(Vec3(0, 0, 1));

		pos += viewRight * fpPosOffset.x
			+ viewFwd * fpPosOffset.y
			+ viewUp * fpPosOffset.z;
	}

	finalMatrix.SetTranslation(pos);

	finalMatrix = finalMatrix * m_holdOffset;

	//This is need it for breakable/joint-constraints stuff
	if (IPhysicalEntity* pPhys = pEntity->GetPhysics())
	{
		if (!pPlayer->IsRemote())
		{
			pe_action_set_velocity v;
			v.v = Vec3(0.01f, 0.01f, 0.01f);
			pPhys->Action(&v);

			//For boids
			if (pPhys->GetType() == PE_PARTICLE)
			{
				pEntity->SetSlotLocalTM(0, IDENTITY);
			}
		}
	}
	//====================================
	bool hasAuthority(gEnv->bServer);
	if (gEnv->bMultiplayer && !gEnv->bServer)
	{
		/*INetChannel* pClientChannel = m_pGameFramework->GetClientChannel();
		if (pClientChannel && m_pGameFramework->GetNetContext()->RemoteContextHasAuthority(pClientChannel, entityId))
		{
			hasAuthority = true;
		}*/
		if (pPlayer->GetHeldObjectId() == entityId)
		{
			hasAuthority = true;
		}

		//We haven't received authorization from server, check if it's a client entity e.g. chickens etc
		if (!hasAuthority)
		{
			//CryMP: Let us have fun with chickens in MP...
			const bool bClientEntity = (pEntity->GetFlags() & ENTITY_FLAG_CLIENT_ONLY);
			if (bClientEntity)
			{
				hasAuthority = true;
			}
		}
	}
	if (hasAuthority || pPlayer->IsRemote())
	{
		pEntity->SetWorldTM(finalMatrix);
	}
}

//===========================================================
bool COffHand::IsHeldObjectBroken(EntityId entityId) const
{
	IEntity* pEntity = m_pEntitySystem->GetEntity(entityId);
	if (!pEntity)
	{
		return false;
	}

	IPhysicalEntity* pPE = pEntity->GetPhysics();

	CryLogAlways("IsHeldObjectBroken %s (Type: %d)", pEntity->GetName(), pPE ? pPE->GetType() : -1);

	for (int idx = 0; ; ++idx)
	{
		pe_params_structural_joint pj;
		pj.idx = idx;
		if (!pPE->GetParams(&pj))
			break;                  // no more joints

		if (pj.bBroken)
		{
			CryLogAlways("Structural joint %d is broken! (Entity: %s, Type: %d)", idx, pEntity->GetName(), pPE ? pPE->GetType() : -1);
		}
	}

	return false;
}

//===========================================================
void COffHand::UpdateGrabbedNPCState()
{
	//Get actor
	CActor* pActor = static_cast<CActor*>(m_pActorSystem->GetActor(m_heldEntityId));
	CActor* pPlayer = GetOwnerActor();
	SActorStats* pStats = pPlayer ? pPlayer->GetActorStats() : NULL;

	if (pActor && pStats)
	{
		RunEffectOnGrabbedNPC(pActor);

		// workaround for playing a different facial animation after 5 seconds
		pStats->grabbedTimer += gEnv->pTimer->GetFrameTime();
		if (m_grabbedNPCSpecies == eGCT_HUMAN && pStats->grabbedTimer > 5.2f)
			pActor->SetAnimationInput("Action", "grabStruggleFP2");
		else
			pActor->SetAnimationInput("Action", "grabStruggleFP");

		//Actor died while grabbed
		if ((pActor->GetHealth() <= 0 && !m_npcWasDead) || pActor->IsFallen() || (pStats->isRagDoll))
		{
			if (m_mainHand && m_mainHand->IsBusy())
				m_mainHand->SetBusy(false);
			//Already throwing (do nothing)
			if (m_currentState & (eOHS_GRABBING_NPC | eOHS_HOLDING_NPC))
			{
				//Drop NPC
				SetOffHandState(eOHS_HOLDING_NPC);

				OnAction(GetOwnerId(), ActionId("use"), eAAM_OnPress, 0.0f);
				OnAction(GetOwnerId(), ActionId("use"), eAAM_OnRelease, 0.0f);
			}
			else if (m_currentState & eOHS_THROWING_NPC)
			{
				OnAction(GetOwnerId(), ActionId("use"), eAAM_OnRelease, 0.0f);
			}

			if (!pStats->isRagDoll)
				m_npcWasDead = true;

		}
	}

}

//============================================================
void COffHand::PostFilterView(struct SViewParams& viewParams)
{
	//This should be only be called when grabbing/holding/throwing an NPC from CPlayer::PostUpdateView()
	IEntity* pEntity = m_pEntitySystem->GetEntity(m_heldEntityId);

	UpdateGrabbedNPCState();
	UpdateGrabbedNPCWorldPos(pEntity, &viewParams);
}

//===============================================
void COffHand::PostUpdate(float frameTime)
{
	//Update character position here when the game is paused, if I don't do so, character goes invisible!
	//IGameObject::PostUpdate() is updated when the game is paused.
	//PostUpdated are enabled/disabled when grabbing/throwing a character
	if (m_pGameFramework->IsGamePaused() && m_currentState & (eOHS_GRABBING_NPC | eOHS_HOLDING_NPC | eOHS_THROWING_NPC))
	{
		//This should be only be called when grabbing/holding/throwing an NPC from CPlayer::PostUpdateView()
		IEntity* pEntity = m_pEntitySystem->GetEntity(m_heldEntityId);

		//UpdateGrabbedNPCState();
		UpdateGrabbedNPCWorldPos(pEntity, NULL);
	}

}

//============================================================
void COffHand::UpdateGrabbedNPCWorldPos(IEntity* pEntity, struct SViewParams* viewParams)
{
	if (!pEntity || !m_stats.fp)
		return;

	Matrix34 neckFinal = Matrix34::CreateIdentity();

	if (viewParams)
	{
		CPlayer* pPlayer = CPlayer::FromActor(GetOwnerActor());
		if (!pPlayer)
			return;

		const SPlayerStats* stats = pPlayer->GetPlayerStats();
		Quat wQuat = (viewParams->rotation * Quat::CreateRotationXYZ(stats->FPWeaponAnglesOffset * gf_PI / 180.0f));
		wQuat *= Quat::CreateSlerp(viewParams->currentShakeQuat, IDENTITY, 0.5f);
		wQuat.Normalize();

		Vec3 itemAttachmentPos = GetSlotHelperPos(0, "item_attachment", false);
		itemAttachmentPos = stats->FPWeaponPos + wQuat * itemAttachmentPos;

		neckFinal.SetRotation33(Matrix33(viewParams->rotation * Quat::CreateRotationZ(gf_PI)));
		neckFinal.SetTranslation(itemAttachmentPos);

		ICharacterInstance* pCharacter = pEntity->GetCharacter(0);
		if (!pCharacter)
			return;

		ISkeletonPose* pSkeletonPose = pCharacter->GetISkeletonPose();
		if (!pSkeletonPose)
			return;

		int neckId = 0;
		Vec3 specialOffset(0.0f, -0.07f, -0.09f);

		switch (m_grabbedNPCSpecies)
		{
		case eGCT_HUMAN:  neckId = pSkeletonPose->GetJointIDByName("Bip01 Neck");
			specialOffset.Set(0.0f, 0.0f, 0.0f);
			break;

		case eGCT_ALIEN:  neckId = pSkeletonPose->GetJointIDByName("Bip01 Neck");
			specialOffset.Set(0.0f, 0.0f, -0.09f);
			break;

		case eGCT_TROOPER: neckId = pSkeletonPose->GetJointIDByName("Bip01 Head");
			break;
		}

		Vec3 neckLOffset(pSkeletonPose->GetAbsJointByID(neckId).t);
		//Vec3 charOffset(pEntity->GetSlotLocalTM(0,false).GetTranslation());
		//if(m_grabbedNPCSpecies==eGCT_TROOPER)
		//charOffset.Set(0.0f,0.0f,0.0f);
		//Vec3 charOffset(0.0f,0.0f,0.0f);		//For some reason the above line didn't work with the trooper...

		//float white[4] = {1,1,1,1};
		//gEnv->pRenderer->Draw2dLabel( 100, 50, 2, white, false, "neck: %f %f %f", neckLOffset.x,neckLOffset.y,neckLOffset.z );
		//gEnv->pRenderer->Draw2dLabel( 100, 70, 2, white, false, "char: %f %f %f", charOffset.x,charOffset.y,charOffset.z );

		//gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(neckFinal.GetTranslation(),0.08f,ColorB(255,0,0));

		neckFinal.AddTranslation(Quat(neckFinal) * -(neckLOffset + specialOffset));
		m_lastNPCMatrix = neckFinal;
	}
	else
	{
		Vec3 itemAttachmentPos = GetSlotHelperPos(0, "item_attachment", true);
		neckFinal = m_lastNPCMatrix;
		neckFinal.SetTranslation(itemAttachmentPos);

		if (ICharacterInstance* pCharacter = pEntity->GetCharacter(0))
		{
			ISkeletonPose* pSkeletonPose = pCharacter->GetISkeletonPose();
			assert(pSkeletonPose && "COffHand::UpdateGrabbedNPCWorldPos --> Actor entity has no skeleton!!");
			if (!pSkeletonPose)
				return;

			int neckId = 0;
			Vec3 specialOffset(0.0f, -0.07f, -0.09f);

			switch (m_grabbedNPCSpecies)
			{
			case eGCT_HUMAN:  neckId = pSkeletonPose->GetJointIDByName("Bip01 Neck");
				specialOffset.Set(0.0f, 0.0f, 0.0f);
				break;

			case eGCT_ALIEN:  neckId = pSkeletonPose->GetJointIDByName("Bip01 Neck");
				specialOffset.Set(0.0f, 0.0f, -0.09f);
				break;

			case eGCT_TROOPER: neckId = pSkeletonPose->GetJointIDByName("Bip01 Head");
				break;
			}

			Vec3 neckLOffset(pSkeletonPose->GetAbsJointByID(neckId).t);
			neckFinal.AddTranslation(Quat(neckFinal) * -(neckLOffset + specialOffset));
		}

	}

	float EntRotZ = RAD2DEG(Quat(neckFinal).GetRotZ());

	pEntity->SetWorldTM(neckFinal);

	//gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(neckFinal.GetTranslation(),0.08f,ColorB(0,255,0));
}

//=============================================================
bool COffHand::GetGrabbedActorNeckWorldPos(IEntity* pEntity, Vec3& outNeckPos) const
{
	if (!pEntity)
		return false;

	ICharacterInstance* pCharacter = pEntity->GetCharacter(0);
	if (!pCharacter)
		return false;

	ISkeletonPose* pSkeletonPose = pCharacter->GetISkeletonPose();
	if (!pSkeletonPose)
		return false;

	int neckId = 0;
	Vec3 specialOffset(0.0f, -0.07f, -0.09f);

	switch (m_grabbedNPCSpecies)
	{
	case eGCT_HUMAN:
		neckId = pSkeletonPose->GetJointIDByName("Bip01 Neck");
		specialOffset.Set(0.0f, 0.0f, 0.0f);
		break;
	case eGCT_ALIEN:
		neckId = pSkeletonPose->GetJointIDByName("Bip01 Neck");
		specialOffset.Set(0.0f, 0.0f, -0.09f);
		break;
	case eGCT_TROOPER:
		neckId = pSkeletonPose->GetJointIDByName("Bip01 Head");
		break;
	default:
		return false;
	}

	if (neckId < 0)
		return false;

	const QuatT neckJoint = pSkeletonPose->GetAbsJointByID(neckId);
	outNeckPos = pEntity->GetWorldTM().TransformPoint(neckJoint.t + specialOffset);
	return true;
}

//=============================================================
void COffHand::OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	s_actionHandler.Dispatch(this, actorId, actionId, activationMode, value);
}

//-------------
bool COffHand::OnActionUse(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	return ProcessOffHandActions(eOHA_USE, INPUT_USE, activationMode);
}

//------------
bool COffHand::OnActionAttack(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	return ProcessOffHandActions(eOHA_USE, INPUT_LBM, activationMode);
}

//------------
bool COffHand::OnActionDrop(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	return ProcessOffHandActions(eOHA_USE, INPUT_RBM, activationMode);
}

//------------
bool COffHand::OnActionThrowGrenade(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	return ProcessOffHandActions(eOHA_THROW_GRENADE, INPUT_DEF, activationMode);
}

//------------
bool COffHand::OnActionXIThrowGrenade(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if (activationMode == eAAM_OnPress)
	{
		m_fGrenadeThrowTimer = 0.0f;
		RequireUpdate(eIUS_General);
	}
	else if (activationMode == eAAM_OnRelease)
	{
		if (m_fGrenadeThrowTimer >= 0.0f)
		{
			ProcessOffHandActions(eOHA_THROW_GRENADE, INPUT_DEF, eAAM_OnPress);
			ProcessOffHandActions(eOHA_THROW_GRENADE, INPUT_DEF, eAAM_OnRelease);
			m_fGrenadeThrowTimer = -1.0f;
		}
	}
	return true;
}

//-----------
bool COffHand::OnActionSwitchGrenade(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	return ProcessOffHandActions(eOHA_SWITCH_GRENADE, INPUT_DEF, activationMode);
}

//-----------
bool COffHand::OnActionXISwitchGrenade(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if (activationMode == eAAM_OnPress)
	{
		m_fGrenadeToggleTimer = 0.0f;
		RequireUpdate(eIUS_General);
	}
	else if (activationMode == eAAM_OnRelease)
	{
		m_fGrenadeToggleTimer = -1.0f;
	}
	return true;
}

//-----------
bool COffHand::OnActionSpecial(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	return ProcessOffHandActions(eOHA_MELEE_ATTACK, INPUT_DEF, activationMode);
}

//----------
bool COffHand::ProcessOffHandActions(EOffHandActions eOHA, int input, int activationMode, float value /*= 0.0f*/)
{
	//Test if action is possible
	if (!EvaluateStateTransition(eOHA, activationMode, input))
		return false;

	if (!PreExecuteAction(eOHA, activationMode))
		return false;

	if (eOHA == eOHA_SWITCH_GRENADE)
	{
		StartSwitchGrenade(false, (value != 0.0f) ? true : false);
	}
	else if (eOHA == eOHA_THROW_GRENADE)
	{
		if (!m_fm->OutOfAmmo() && GetFireModeIdx(m_fm->GetName()) < MAX_GRENADE_TYPES)
		{
			PerformThrow(activationMode, 0, GetCurrentFireMode());
		}
	}
	else if (eOHA == eOHA_MELEE_ATTACK)
	{
		MeleeAttack();
	}
	else if (eOHA == eOHA_USE)
	{
		if (m_currentState & eOHS_INIT_STATE)
		{
			const int typ = CanPerformPickUp(GetOwnerActor(), nullptr, true);
			if (typ == OH_GRAB_ITEM)
			{
				StartPickUpItem();
			}
			else if (typ == OH_GRAB_OBJECT)
			{
				if (gEnv->bMultiplayer && g_pGameCVars->mp_pickupObjects && m_pGameFramework->IsImmersiveMPEnabled())
				{
					if (Request_PickUpObject_MP())
					{
						return true;
					}
				}

				StartPickUpObject(m_preHeldEntityId, false);
			}
			else if (typ == OH_GRAB_NPC)
			{
				StartPickUpObject(m_preHeldEntityId, true);
			}
			else if (typ == OH_NO_GRAB)
			{
				CancelAction();
				return false;
			}

		}
		else if (m_currentState & (eOHS_HOLDING_OBJECT | eOHS_THROWING_OBJECT))
		{
			//CryMP throwing object called here
			StartThrowObject(m_heldEntityId, activationMode, false);
		}
		else if (m_currentState & (eOHS_HOLDING_NPC | eOHS_THROWING_NPC))
		{
			StartThrowObject(m_heldEntityId, activationMode, true);
		}
	}

	return true;
}

//==================================================================
bool COffHand::EvaluateStateTransition(int requestedAction, int activationMode, int	inputMethod)
{
	switch (requestedAction)
	{
	case eOHA_SWITCH_GRENADE:
		if (activationMode == eAAM_OnPress && m_currentState == eOHS_INIT_STATE)
		{
			return true;
		}
		break;

	case eOHA_THROW_GRENADE:
		if (activationMode == eAAM_OnPress && m_currentState == eOHS_INIT_STATE)
		{
			//Don't allow throwing grenades under water.
			if (CPlayer* pPlayer = GetOwnerPlayer())
			{
				const SPlayerStats* stats = pPlayer->GetPlayerStats();
				if ((stats->worldWaterLevel + 0.1f) > stats->FPWeaponPos.z)
					return false;
			}

			//Don't throw if there's no ammo (or not fm)
			if (m_fm && !m_fm->OutOfAmmo() && m_nextThrowTimer <= 0.0f)
				return true;
		}
		else if (activationMode == eAAM_OnRelease && m_currentState == eOHS_HOLDING_GRENADE)
		{
			return true;
		}
		break;

	case eOHA_USE:
		//Evaluate mouse inputs first
		if (inputMethod == INPUT_LBM)
		{
			//Two handed object throw/drop is handled with the mouse now...
			if (activationMode == eAAM_OnPress && m_currentState & eOHS_HOLDING_OBJECT && m_grabType == GRAB_TYPE_TWO_HANDED)
			{
				m_forceThrow = true;
				return true;
			}
			else if (activationMode == eAAM_OnRelease && m_currentState & eOHS_THROWING_OBJECT && m_grabType == GRAB_TYPE_TWO_HANDED)
			{
				return true;
			}
		}
		else if (inputMethod == INPUT_RBM)
		{
			//Two handed object throw/drop is handled with the mouse now...
			if (activationMode == eAAM_OnPress && m_currentState & eOHS_HOLDING_OBJECT)
			{
				m_forceThrow = false;
				return true;
			}
			else if (activationMode == eAAM_OnRelease && m_currentState & eOHS_THROWING_OBJECT)
			{
				return true;
			}
		}
		else if (activationMode == eAAM_OnPress && (m_currentState & (eOHS_INIT_STATE | eOHS_HOLDING_OBJECT | eOHS_HOLDING_NPC)))
		{
			m_forceThrow = true;
			return true;
		}
		else if (activationMode == eAAM_OnRelease && (m_currentState & (eOHS_THROWING_OBJECT | eOHS_THROWING_NPC)))
		{
			return true;
		}
		break;

	case eOHA_MELEE_ATTACK:
		if (activationMode == eAAM_OnPress && m_currentState == eOHS_HOLDING_OBJECT && m_grabType == GRAB_TYPE_ONE_HANDED)
		{
			return true;
		}
		break;
	}

	return false;
}

//==================================================================
bool COffHand::PreExecuteAction(int requestedAction, int activationMode, bool forceSelect)
{
	if (m_currentState != eOHS_INIT_STATE && requestedAction != eOHA_REINIT_WEAPON)
		return true;

	//We have to test the main weapon/item state, in some cases the offhand could not perform the action
	CActor* pActor = GetOwnerActor();
	if (!pActor || pActor->GetHealth() <= 0 || (pActor->IsSwimming() && requestedAction != eOHA_USE))
		return false;

	CItem* pMain = GetActorItem(pActor);
	CWeapon* pMainWeapon = static_cast<CWeapon*>(pMain ? pMain->GetIWeapon() : NULL);

	bool exec = true;

	if (pMain && pMain->IsMounted())
		return false;

	if (pMainWeapon)
		exec &= !pMainWeapon->IsModifying() && !pMainWeapon->IsReloading() && !pMainWeapon->IsSwitchingFireMode() && !pMainWeapon->IsFiring();


	if (pMainWeapon && (pMainWeapon->IsZoomed() || pMainWeapon->IsZooming()) && (requestedAction == eOHA_THROW_GRENADE))
	{
		pMainWeapon->ExitZoom();
		pMainWeapon->ExitViewmodes();
	}

	if (exec)
	{
		if ((!gEnv->bMultiplayer || g_pGameCVars->mp_animationGrenadeSwitch) || (requestedAction != eOHA_SWITCH_GRENADE))
		{
			SetHand(eIH_Left);		//Here??

			if ((GetEntity()->IsHidden() || forceSelect) && activationMode == eAAM_OnPress)
			{
				m_stats.fp = !m_stats.fp;

				GetScheduler()->Lock(true);
				Select(true);
				GetScheduler()->Lock(false);
				SetBusy(false);
			}
		}
		SetMainHand(pMain);
		SetMainHandWeapon(pMainWeapon);

		m_mainHandIsDualWield = false;

		if (requestedAction == eOHA_THROW_GRENADE)
		{
			if (m_mainHand && m_mainHand->TwoHandMode() == 1)
			{
				GetOwnerActor()->HolsterItem(true);

				SetMainHand(nullptr);
				SetMainHandWeapon(nullptr);
			}
			else if (m_mainHand && m_mainHand->IsDualWield() && m_mainHand->GetDualWieldSlave())
			{
				SetMainHand(static_cast<CItem*>(m_mainHand->GetDualWieldSlave()));

				m_mainHandIsDualWield = true;
				m_mainHand->Select(false);
			}
		}
	}
	else if (requestedAction == eOHA_REINIT_WEAPON)
	{
		SetMainHand(pMain);
		SetMainHandWeapon(pMainWeapon);
	}

	return exec;
}

//==================================================================
void COffHand::StartFire()
{
	CWeapon::StartFire();

	if (m_heldEntityId)
	{
		CPlayer* pPlayer = CPlayer::FromActor(GetOwnerActor());
		if (pPlayer && pPlayer->GetAnimatedCharacter())
		{
			pPlayer->GetAnimatedCharacter()->TriggerRecoil(1.0, 0.2f);
		}
	}
}

//==================================================================
void COffHand::StopFire()
{
	CWeapon::StopFire();
}

//==================================================================
void COffHand::NetStartFire()
{
	if (GetEntity()->IsHidden()) // this is need for network triggered grenade throws to trigger updates and what not..
	{
		m_stats.fp = !m_stats.fp;

		GetScheduler()->Lock(true);
		Select(true);
		GetScheduler()->Lock(false);
		SetBusy(false);
	}

	CWeapon::NetStartFire();

	if (!m_heldEntityId)
	{
		AttachGrenadeToHand(GetCurrentFireMode(), m_stats.fp, true);
	}

	//Handle FP Spectator
	if (m_stats.fp)
	{
		SetMainHandWeapon(GetOwnerActor()->GetCurrentWeapon(false));
		SetMainHand(static_cast<CItem*>(GetOwnerActor()->GetCurrentItem()));
		if (m_mainHand)
		{
			//if (!(m_currentState & (eOHS_THROWING_NPC | eOHS_THROWING_OBJECT)))
			{
				if (m_mainHandWeapon && m_mainHandWeapon->IsWeaponRaised())
				{
					m_mainHandWeapon->RaiseWeapon(false, true);
					m_mainHandWeapon->SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, g_pItemStrings->idle);
				}
				m_mainHand->PlayAction(g_pItemStrings->offhand_on);
				m_mainHand->SetActionSuffix("akimbo_");
			}
		}
		if (m_mainHandWeapon && m_mainHandWeapon->GetEntity()->GetClass() == CItem::sFistsClass)
		{
			CFists* pFists = static_cast<CFists*>(m_mainHandWeapon);
			pFists->RequestAnimState(CFists::eFAS_FIGHT);
		}
	}
}

//=============================================================================
void COffHand::NetStopFire()
{
	CWeapon::NetStopFire();

	AttachGrenadeToHand(GetCurrentFireMode(), m_stats.fp, false);

	if (m_heldEntityId)
	{
		CPlayer* pPlayer = CPlayer::FromActor(GetOwnerActor());
		if (pPlayer)
		{
			pPlayer->OnObjectEvent(CActor::ObjectEvent::THROW);
		}

		if (m_mainHand && !m_mainHand->IsDualWield() && g_pGameCVars->mp_buyPageKeepTime != 170)
		{
			m_mainHand->PlayAction(g_pItemStrings->offhand_off, 0, false, CItem::eIPAF_Default | CItem::eIPAF_NoBlend);
		}
	}
	else
	{
		GetScheduler()->TimerAction(GetCurrentAnimationTime(CItem::eIGS_FirstPerson),
			CSchedulerAction<Timer_FinishGrenadeAction>::Create(Timer_FinishGrenadeAction(this, m_mainHand)), false);
	}
}


//=============================================================================
//This function seems redundant...
void COffHand::CancelAction()
{
	SetOffHandState(eOHS_INIT_STATE);
}

//=============================================================================
void COffHand::FinishAction(EOffHandActions eOHA)
{
	if (g_pGameCVars->mp_pickupDebug > 2)
	{
		const char* actionName = [eOHA]()
			{
				switch (eOHA)
				{
				case eOHA_SWITCH_GRENADE:           return "eOHA_SWITCH_GRENADE";
				case eOHA_PICK_ITEM:                return "eOHA_PICK_ITEM";
				case eOHA_GRAB_NPC:                 return "eOHA_GRAB_NPC";
				case eOHA_THROW_NPC:                return "eOHA_THROW_NPC";
				case eOHA_PICK_OBJECT:              return "eOHA_PICK_OBJECT";
				case eOHA_THROW_OBJECT:             return "eOHA_THROW_OBJECT";
				case eOHA_RESET:                    return "eOHA_RESET";
				case eOHA_FINISH_MELEE:             return "eOHA_FINISH_MELEE";
				case eOHA_FINISH_AI_THROW_GRENADE:  return "eOHA_FINISH_AI_THROW_GRENADE";
				default:                            return "eOHA_UNKNOWN";
				}
			}();

		CryLogAlways("[OffHand] FinishAction: $8%s (%d)", actionName, (int)eOHA);
	}

	switch (eOHA)
	{
	case eOHA_SWITCH_GRENADE:
		EndSwitchGrenade();
		break;

	case eOHA_PICK_ITEM:
		EndPickUpItem();
		break;

	case eOHA_GRAB_NPC:
		SetOffHandState(eOHS_HOLDING_NPC);

		break;

	case eOHA_THROW_NPC:
		GetScheduler()->TimerAction(300, 
			CSchedulerAction<Timer_FinishOffHandAction>::Create(Timer_FinishOffHandAction(eOHA_RESET, this)),
			true);
		ThrowNPC(m_heldEntityId);

		RemoveHeldEntityId(m_heldEntityId, ConstraintReset::Delayed);

		SetOffHandState(eOHS_TRANSITIONING);
		break;

	case eOHA_PICK_OBJECT:
		SetOffHandState(eOHS_HOLDING_OBJECT);
		break;

	case eOHA_THROW_OBJECT:
	{
		GetScheduler()->TimerAction(500,
			CSchedulerAction<Timer_FinishOffHandAction>::Create(Timer_FinishOffHandAction(eOHA_RESET, this)), //CryMP: Timer needed for FP animations
			true);

		CActor* pActor = GetOwnerActor();
		const EntityId playerId = pActor ? pActor->GetEntityId() : 0;
		const bool isTwoHand = IsTwoHandMode();

		GetScheduler()->TimerAction(
			g_pGameCVars->mp_pa10,
			MakeAction([this, playerId, isTwoHand](CItem* /*unused*/) {
				CPlayer* pPlayer = CPlayer::FromActorId(playerId);
				if (pPlayer)
				{
					if (pPlayer->IsThirdPerson())
					{
						//pPlayer->PlayAnimation("combat_plantUB_c4_01", 1.0f, false, true, 1);

						if (isTwoHand)
						{

							pPlayer->SetExtension("c4");
							pPlayer->SetInput("plant", false);
							//pPlayer->PlayAction("plant", "c4");
						}
						else
						{
							pPlayer->SetExtension("nw");
							pPlayer->SetInput("holding_grenade");
							pPlayer->SetInput("throw_grenade");
							//pPlayer->PlayAction("throw_grenade", "ignore");
						}
					}
				}
				}),
			/*persistent=*/true
		);

		// after it's thrown, wait 500ms to enable collisions again
		RemoveHeldEntityId(m_heldEntityId, ConstraintReset::Delayed);

		IEntity* pEntity = m_pEntitySystem->GetEntity(m_heldEntityId);
		if (pEntity)
		{
			IPhysicalEntity* pPhys = pEntity->GetPhysics();
			if (pPhys && pPhys->GetType() == PE_PARTICLE)
				pEntity->SetSlotLocalTM(0, m_intialBoidLocalMatrix);

		}

		SetOffHandState(eOHS_TRANSITIONING);
	}
	break;

	case eOHA_RESET:
		//Reset main weapon status and reset offhand
	{
		CActor* pActor = GetOwnerActor();
		if (pActor)
		{
			const EntityId playerId = pActor->GetEntityId();
			if (m_prevMainHandId && !pActor->IsSwimming())
			{
				pActor->SelectItem(m_prevMainHandId, false);
				SetMainHand(static_cast<CItem*>(pActor->GetCurrentItem()));
				SetMainHandWeapon(static_cast<CWeapon*>(m_mainHand ? m_mainHand->GetIWeapon() : nullptr));
			}

			if (pActor->IsClient())
			{
				RequestFireMode(m_lastFireModeId); //Works for MP as well
			}

			float timeDelay = 0.1f;
			if (!m_mainHand)
			{
				SActorStats* pStats = pActor->GetActorStats();
				if (!pActor->ShouldSwim() && !m_bCutscenePlaying && (pStats && !pStats->inFreefall.Value()))
				{
					pActor->HolsterItem(false);
				}
			}
			else if (!m_mainHandIsDualWield && !m_prevMainHandId)
			{
				m_mainHand->ResetDualWield();
				m_mainHand->PlayAction(g_pItemStrings->offhand_off, 0, false, CItem::eIPAF_Default | CItem::eIPAF_NoBlend);
				timeDelay = (m_mainHand->GetCurrentAnimationTime(CItem::eIGS_FirstPerson) + 50) * 0.001f;
			}
			else if (m_mainHandIsDualWield)
			{
				m_mainHand->Select(true);
			}
			if (pActor->IsRemote())
			{
				SetOffHandState(eOHS_INIT_STATE);
			}
			else
			{
				SetResetTimer(timeDelay);
			}

			RequireUpdate(eIUS_General);
			m_prevMainHandId = 0;

			const EntityId entityId = m_heldEntityId;

			RemoveHeldEntityId(m_heldEntityId, static_cast<ConstraintReset>(SkipIfDelayTimerActive | Immediate));

			if (CPlayer* pPlayer = CPlayer::FromActor(pActor))
			{
				pPlayer->SetArmIKLocalInvalid();
			}

			GetScheduler()->TimerAction(
				1000,
				MakeAction([this, playerId, entityId](CItem* /*unused*/) {
					CPlayer* pPlayer = CPlayer::FromActorId(playerId);
					if (pPlayer)
					{
						CItem* pItem = static_cast<CItem*>(pPlayer->GetCurrentItem());
						if (pItem)
						{
							pItem->PlaySelectAnimation(pPlayer);
						}
					}
					IEntity* pObject = m_pEntitySystem->GetEntity(entityId);
					if (pObject)
					{
						AwakeEntityPhysics(pObject);
					}
					}),
				/*persistent=*/true
			);
		}
	}

	break;

	case	eOHA_FINISH_MELEE:	
	{
		if (m_heldEntityId)
		{
			SetOffHandState(eOHS_HOLDING_OBJECT);
		}
	}
	break;

	case eOHA_FINISH_AI_THROW_GRENADE:
	{
		// Reset the main weapon after a grenade throw.
		CActor* pActor = GetOwnerActor();
		if (pActor)
		{
			CItem* pMain = GetActorItem(pActor);
			if (pMain)
				pMain->PlayAction(g_pItemStrings->idle, 0, false, CItem::eIPAF_Default | CItem::eIPAF_NoBlend);
		}
	}
	break;
	}
}

//==============================================================================
void COffHand::Freeze(bool freeze)
{
	CWeapon::Freeze(freeze);

	if (!freeze && m_currentState == eOHS_HOLDING_GRENADE)
	{
		FinishAction(eOHA_RESET);
		CancelAction();
	}
}

//==============================================================================
void COffHand::SetOffHandState(EOffHandStates eOHS)
{
	if (g_pGameCVars->mp_pickupDebug > 2)
	{
		// -- Current State Bitmask --
		string stateBits;
		struct StateFlagInfo { int flag; const char* name; };
		const StateFlagInfo flags[] = {
			{ eOHS_INIT_STATE,        "Init" },
			{ eOHS_SWITCHING_GRENADE, "SwitchingGrenade" },
			{ eOHS_HOLDING_GRENADE,   "HoldingGrenade" },
			{ eOHS_THROWING_GRENADE,  "ThrowingGrenade" },
			{ eOHS_PICKING,           "Picking" },
			{ eOHS_PICKING_ITEM,      "PickingItem" },
			{ eOHS_PICKING_ITEM2,     "PickingItem2" },
			{ eOHS_HOLDING_OBJECT,    "HoldingObject" },
			{ eOHS_THROWING_OBJECT,   "ThrowingObject" },
			{ eOHS_GRABBING_NPC,      "GrabbingNPC" },
			{ eOHS_HOLDING_NPC,       "HoldingNPC" },
			{ eOHS_THROWING_NPC,      "ThrowingNPC" },
			{ eOHS_TRANSITIONING,     "Transitioning" },
			{ eOHS_MELEE,             "Melee" }
		};

		for (const auto& f : flags)
		{
			if (eOHS & f.flag)
			{
				if (!stateBits.empty()) stateBits += "|";
				stateBits += f.name;
			}
		}
		CryLogAlways("SetOffHandState: $6%s", stateBits.c_str());
	}

	m_currentState = eOHS;

	if (eOHS & eOHS_INIT_STATE)
	{
		SetMainHand(nullptr);
		SetMainHandWeapon(nullptr);

		m_preHeldEntityId = 0;
		
		RemoveHeldEntityId(m_heldEntityId, static_cast<ConstraintReset>(SkipIfDelayTimerActive | Immediate));

		m_mainHandIsDualWield = false;
		Select(false);

		if (gEnv->bClient && GetOwnerActor() && GetOwnerActor()->IsRemote())
		{
			//EnableUpdate(false, eIUS_General);
		}
	}
}

//==============================================================================
void COffHand::SetMainHand(CItem *pItem)
{
	m_mainHand = pItem;
}

//==============================================================================
void COffHand::SetMainHandWeapon(CWeapon* pWeapon)
{
	m_mainHandWeapon = pWeapon;
}

//==============================================================================
void COffHand::StartSwitchGrenade(bool xi_switch, bool fakeSwitch)
{
	//Iterate different firemodes 
	int firstMode = GetCurrentFireMode();
	int newMode = GetNextFireMode(GetCurrentFireMode());

	while (newMode != firstMode)
	{
		//Fire mode idx>2 means its a throw object/npc firemode
		if (GetFireMode(newMode)->OutOfAmmo() || newMode >= MAX_GRENADE_TYPES)
			newMode = GetNextFireMode(newMode);
		else
		{
			m_lastFireModeId = newMode;
			break;
		}
	}

	//We didn't find a fire mode with ammo
	if (newMode == firstMode)
	{
		CancelAction();

		if (m_ownerId == LOCAL_PLAYER_ENTITY_ID && g_pGame->GetHUD())
			g_pGame->GetHUD()->FireModeSwitch(true);

		return;
	}
	else if (!fakeSwitch)
		RequestFireMode(newMode);

	//No animation in multiplayer or when using the gamepad
	//CryMP: Optional animations
	if ((gEnv->bMultiplayer && (!g_pGameCVars->mp_animationGrenadeSwitch || !m_stats.fp)) || xi_switch)
	{
		SetOffHandState(eOHS_SWITCHING_GRENADE);

		SetResetTimer(0.3f); //Avoid spamming keyboard issues
		RequireUpdate(eIUS_General);
		return;
	}

	//Unzoom weapon if neccesary
	if (m_mainHandWeapon && (m_mainHandWeapon->IsZoomed() || m_mainHandWeapon->IsZooming()))
	{
		m_mainHandWeapon->ExitZoom();
		m_mainHandWeapon->ExitViewmodes();
	}

	m_mainHandIsDualWield = false;

	//A new grenade type/fire mode was chosen
	if (m_mainHand && (m_mainHand->TwoHandMode() != 1) && !m_mainHand->IsDualWield())
	{

		if (m_mainHandWeapon && m_mainHandWeapon->GetEntity()->GetClass() == CItem::sFistsClass)
		{
			CFists* pFists = static_cast<CFists*>(m_mainHandWeapon);
			pFists->RequestAnimState(CFists::eFAS_FIGHT);
		}

		if (m_mainHandWeapon && m_mainHandWeapon->IsWeaponRaised())
		{
			m_mainHandWeapon->RaiseWeapon(false, true);
			m_mainHandWeapon->SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, g_pItemStrings->idle);
		}

		//Play deselect left hand on main item
		m_mainHand->PlayAction(g_pItemStrings->offhand_on);
		m_mainHand->SetActionSuffix("akimbo_");

		GetScheduler()->TimerAction(m_mainHand->GetCurrentAnimationTime(eIGS_FirstPerson),
			CSchedulerAction<Timer_FinishOffHandAction>::Create(Timer_FinishOffHandAction(eOHA_SWITCH_GRENADE, this)), false);

	}
	else
	{
		//No main item or holstered (wait 100ms)
		if (m_mainHand && m_mainHand->IsDualWield() && m_mainHand->GetDualWieldSlave())
		{
			SetMainHand(static_cast<CItem*>(m_mainHand->GetDualWieldSlave()));

			m_mainHand->Select(false);
			m_mainHandIsDualWield = true;
		}
		else
		{
			GetOwnerActor()->HolsterItem(true);

			SetMainHand(nullptr);
			SetMainHandWeapon(nullptr);
		}
		GetScheduler()->TimerAction(100, CSchedulerAction<Timer_FinishOffHandAction>::Create(Timer_FinishOffHandAction(eOHA_SWITCH_GRENADE, this)), false);
	}

	//Change offhand state
	SetOffHandState(eOHS_SWITCHING_GRENADE);

	if (fakeSwitch)
		AttachGrenadeToHand(firstMode);
	else
		AttachGrenadeToHand(newMode);
}

//==============================================================================
void COffHand::EndSwitchGrenade()
{
	//Play select grenade animation (and un-hide grenade geometry)
	PlayAction(g_pItemStrings->select_grenade);
	//HideItem(false);
	GetScheduler()->TimerAction(GetCurrentAnimationTime(CItem::eIGS_FirstPerson),
		CSchedulerAction<Timer_FinishGrenadeAction>::Create(Timer_FinishGrenadeAction(this, m_mainHand)), false);
}

//==============================================================================
void COffHand::PerformThrow(float speedScale)
{
	if (!m_fm)
		return;

	m_fm->StartFire();

	CThrow* pThrow = static_cast<CThrow*>(m_fm);
	pThrow->SetSpeedScale(speedScale);

	m_fm->StopFire();
	pThrow->ThrowingGrenade(true);

	// Schedule to revert back to main weapon.
	GetScheduler()->TimerAction(2000,
		CSchedulerAction<Timer_FinishOffHandAction>::Create(Timer_FinishOffHandAction(eOHA_FINISH_AI_THROW_GRENADE, this)), false);
}

//==============================================================================
void COffHand::PerformThrow(int activationMode, EntityId throwableId, int oldFMId /* = 0 */, bool isLivingEnt /*=false*/)
{
	if (!m_fm)
		return;

	if (!throwableId and activationMode == eAAM_OnPress)
	{
		SetOffHandState(eOHS_HOLDING_GRENADE);
	}

	//Throw objects...
	if (throwableId && activationMode == eAAM_OnPress)
	{
		if (!isLivingEnt)
		{
			CPlayer* pPlayer = CPlayer::FromActor(GetOwnerActor());
			if (pPlayer)
			{
				pPlayer->NotifyObjectGrabbed(false, throwableId, false);
			}

			SetOffHandState(eOHS_THROWING_OBJECT);

			CThrow* pThrow = static_cast<CThrow*>(m_fm);
			pThrow->SetThrowable(throwableId, m_forceThrow,
				CSchedulerAction<Timer_FinishOffHandAction>::Create(Timer_FinishOffHandAction(eOHA_THROW_OBJECT, this))
			);
		}
		else
		{
			SetOffHandState(eOHS_THROWING_NPC);

			CThrow* pThrow = static_cast<CThrow*>(m_fm);
			pThrow->SetThrowable(throwableId, true,
				CSchedulerAction<Timer_FinishOffHandAction>::Create(Timer_FinishOffHandAction(eOHA_THROW_NPC, this))
			);
		}
		m_forceThrow = false;

		// enable leg IK again
		EnableFootGroundAlignment(true);
	}
	//--------------------------

	if (activationMode == eAAM_OnPress)
	{
		if (!m_fm->IsFiring() && m_nextThrowTimer < 0.0f)
		{
			if (m_currentState == eOHS_HOLDING_GRENADE)
			{
				AttachGrenadeToHand(GetCurrentFireMode(), m_stats.fp);
			}

			StartFire(); //m_fm->StartFire();

			SetBusy(false);

			if (m_mainHand && m_fm->IsFiring())
			{
				if (!(m_currentState & (eOHS_THROWING_NPC | eOHS_THROWING_OBJECT)))
				{
					if (m_mainHandWeapon && m_mainHandWeapon->IsWeaponRaised())
					{
						m_mainHandWeapon->RaiseWeapon(false, true);
						m_mainHandWeapon->SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, g_pItemStrings->idle);
					}

					m_mainHand->PlayAction(g_pItemStrings->offhand_on);
					m_mainHand->SetActionSuffix("akimbo_");
				}
			}
			if (!throwableId)
			{
				if (m_mainHandWeapon && m_mainHandWeapon->GetEntity()->GetClass() == CItem::sFistsClass)
				{
					CFists* pFists = static_cast<CFists*>(m_mainHandWeapon);
					pFists->RequestAnimState(CFists::eFAS_FIGHT);
				}
			}
		}

	}
	else if (activationMode == eAAM_OnRelease && m_nextThrowTimer <= 0.0f)
	{
		CThrow* pThrow = static_cast<CThrow*>(m_fm);
		if (m_currentState != eOHS_HOLDING_GRENADE)
		{
			if (gEnv->bMultiplayer)
			{
				IEntity* pEntity = m_pEntitySystem->GetEntity(m_heldEntityId);
				if (pEntity && pEntity->GetPhysics())
				{
					CThrow* pThrow = static_cast<CThrow*>(m_fm);
					Vec3 hit = pThrow->GetProbableHit(WEAPON_HIT_RANGE);
					Vec3 pos = pThrow->GetFiringPos(hit);
					Vec3 dir = pThrow->GetFiringDir(hit, pos);
					if (pThrow->CheckForIntersections(pEntity->GetPhysics(), dir))
					{
						if (isLivingEnt)
						{
							SetOffHandState(eOHS_HOLDING_NPC);
						}
						else
						{
							SetOffHandState(eOHS_HOLDING_OBJECT);
						}

						EnableFootGroundAlignment(false);

						//Should clear throwable in CThrow maybe

						g_pGame->GetHUD()->DisplayBigOverlayFlashMessage("Cant throw object here!", 2.0f, 400, 400, Col_Goldenrod);
						return;
					}
				}
			}

			if (CPlayer* pPlayer = CPlayer::FromActor(GetOwnerActor()))
			{
				ThrowObject_MP(pPlayer, m_heldEntityId, isLivingEnt);
			}

			pThrow->ThrowingGrenade(false);
		}
		else if (m_currentState == eOHS_HOLDING_GRENADE)
		{
			SetOffHandState(eOHS_THROWING_GRENADE);
		}
		else
		{
			CancelAction();
			return;
		}

		m_nextThrowTimer = 60.0f / m_fm->GetFireRate();

		m_fm->StopFire();
		pThrow->ThrowingGrenade(true);

		if (m_fm->IsFiring() && m_currentState == eOHS_THROWING_GRENADE)
		{
			GetScheduler()->TimerAction(GetCurrentAnimationTime(CItem::eIGS_FirstPerson), 
				CSchedulerAction<Timer_FinishGrenadeAction>::Create(Timer_FinishGrenadeAction(this, m_mainHand)), false);
		}
	}
}

//===============================================================================
int COffHand::CanPerformPickUp(CActor* pActor, IPhysicalEntity* pPhysicalEntity /*=NULL*/, bool getEntityInfo /*= false*/)
{
	if (!pActor || (!pActor->IsClient() && !pActor->IsFpSpectatorTarget())) //CryMP Fp Spec enable pickup HUD 
		return OH_NO_GRAB;

	IMovementController* pMC = pActor->GetMovementController();
	if (!pMC)
		return OH_NO_GRAB;

	SMovementState info;
	pMC->GetMovementState(info);

	if (gEnv->bMultiplayer && !g_pGameCVars->mp_pickupObjects)
	{
		return CheckItemsInProximity(info.eyePosition, info.eyeDirection, getEntityInfo);
	}

	EStance playerStance = pActor->GetStance();
	const bool isRemote = pActor->IsRemote();

	if (!getEntityInfo)
	{
		//Prevent pick up message while can not pick up
		IItem* pItem = pActor->GetCurrentItem(false);
		CWeapon* pMainWeapon = pItem ? static_cast<CWeapon*>(pItem->GetIWeapon()) : NULL;
		if (pMainWeapon)
		{
			if (pMainWeapon->IsBusy() || pMainWeapon->IsModifying() || pMainWeapon->IsReloading())
				return OH_NO_GRAB;
		}
	}

	if (!pPhysicalEntity)
	{
		const ray_hit* pRay = pActor->GetGameObject()->GetWorldQuery()->GetLookAtPoint(m_range);
		if (pRay)
		{
			pPhysicalEntity = pRay->pCollider;
		}
		else
			return CheckItemsInProximity(info.eyePosition, info.eyeDirection, getEntityInfo);
	}

	IEntity* pEntity = m_pEntitySystem->GetEntityFromPhysics(pPhysicalEntity);

	m_crosshairId = 0;

	Vec3 pos = info.eyePosition;
	float lenSqr = 0.0f;
	bool  breakable = false;
	pe_params_part pPart;
		
	//Check if entity is in range
	if (pEntity)
	{
		const bool bDefaultClass = pEntity->GetClass() == gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();
			
		lenSqr = (pos - pEntity->GetWorldPos()).len2();
		if (pPhysicalEntity->GetType() == PE_RIGID && !bDefaultClass)
		{
			//Procedurally breakable object (most likely...)
			//I need to adjust the distance, since the pivot of the entity could be anywhere
			pPart.ipart = 0;
			if (pPhysicalEntity->GetParams(&pPart) && pPart.pPhysGeom)
			{
				lenSqr -= pPart.pPhysGeom->origin.len2();
				breakable = true;
			}
		}

		const EntityId lookAtId = pEntity->GetId();
		if (lookAtId != m_lastLookAtEntityId)
		{
			OnLookAtEntityChanged(pEntity);

			m_lastLookAtEntityId = lookAtId;
		}
	}

	if (lenSqr < m_range * m_range)
	{
		if (pEntity)
		{
			const EntityId entityId = pEntity->GetId();
			// check if we have to pickup with two hands or just on hand
			SelectGrabType(pEntity);
			m_crosshairId = pEntity->GetId();

			IVehicle* pVehicle = m_pVehicleSystem->GetVehicle(entityId);

			if (getEntityInfo && !isRemote)
			{
				m_preHeldEntityId = pEntity->GetId();
			}

			//1.- Player can grab some NPCs
			//Let the actor decide if it can be grabbed
			if (CActor* pActorAI = static_cast<CActor*>(m_pActorSystem->GetActor(entityId)))
			{
				if (((playerStance != STANCE_STAND) && (playerStance != STANCE_ZEROG)) || pActor->IsSwimming())
					return OH_NO_GRAB;

				//Check Player position vs AI position
				if (pActorAI->GetActorSpecies() == eGCT_HUMAN)
				{
					float playerZ = pActor->GetEntity()->GetWorldPos().z;
					Vec3 aiPos = pActorAI->GetEntity()->GetWorldPos();
					if (aiPos.z - playerZ > 1.0f)
						return OH_NO_GRAB;

					Line aim = Line(info.eyePosition, info.eyeDirection);

					float dst = LinePointDistanceSqr(aim, aiPos, 0.75f);
					if (dst < 0.6f)
						return OH_NO_GRAB;

					CPlayer* pPlayerAI = CPlayer::FromActor(pActorAI);
					if (pPlayerAI && pPlayerAI->GetPlayerStats()->isStandingUp)
						return OH_NO_GRAB;
				}

				if (pEntity->GetAI() && pActor->GetEntity() && !pActorAI->GetLinkedVehicle())
				{
					//Check script table (maybe is not possible to grab)
					SmartScriptTable props;
					SmartScriptTable propsDamage;
					IScriptTable* pScriptTable = pEntity->GetScriptTable();
					if (pScriptTable && pScriptTable->GetValue("Properties", props))
					{
						if (props->GetValue("Damage", propsDamage))
						{
							int noGrab = 0;
							if (propsDamage->GetValue("bNoGrab", noGrab) && noGrab != 0)
								return OH_NO_GRAB;

							float customGrabDistance;
							if (propsDamage->GetValue("customGrabDistance", customGrabDistance))
							{
								if (lenSqr > customGrabDistance * customGrabDistance)
									return OH_NO_GRAB;
							}
						}
					}

					if (pActorAI->GetActorSpecies() != eGCT_UNKNOWN && pActorAI->GetHealth() > 0 && !pActorAI->IsFallen() && pEntity->GetAI()->IsHostile(pActor->GetEntity()->GetAI(), false))
						return OH_GRAB_NPC;
					else
						return OH_NO_GRAB;
				}
				return OH_NO_GRAB;
			}

			//2. -if it's an item, let the item decide if it can be picked up or not
			if (CItem* pItem = static_cast<CItem*>(m_pItemSystem->GetItem(pEntity->GetId())))
			{
				if (pItem->CanPickUp(pActor->GetEntityId()))
					return OH_GRAB_ITEM;
				else
					return OH_NO_GRAB;
			}

			//Items have priority over the rest of pickables
			if (CheckItemsInProximity(info.eyePosition, info.eyeDirection, getEntityInfo) == OH_GRAB_ITEM)
				return OH_GRAB_ITEM;

			if (pActor->IsSwimming() || playerStance == STANCE_PRONE)
				return OH_NO_GRAB;

			//2.5. -CryMP Custom pickups 
			if (g_pGameCVars->mp_pickupVehicles || !gEnv->bMultiplayer)
			{
				//CryMP: Crouch to pickup vehicles :D
				if (playerStance == STANCE_CROUCH && pVehicle)
				{
					// CryMP: Mass limit for pickups
					if (g_pGameCVars->mp_pickupMassLimit > 0)
					{
						pe_status_dynamics dyn;
						if (pPhysicalEntity->GetStatus(&dyn))
						{
							if (dyn.mass > g_pGameCVars->mp_pickupMassLimit)
							{
								if (g_pGame->GetHUD() && gEnv->pTimer->GetCurrTime() - m_lastTooHeavyMessage > 2.0f)
								{
									g_pGame->GetHUD()->DisplayBigOverlayFlashMessage("@object_too_heavy", 2.0f, 400, 400, Col_Goldenrod);

									m_lastTooHeavyMessage = gEnv->pTimer->GetCurrTime();
								}

								return OH_NO_GRAB;
							}
						}
					}

					//Don't allow to pickup while in vehicle, or in air
					if (!pActor->GetLinkedVehicle() && pActor->GetActorStats() && pActor->GetActorStats()->onGround > 0.0f)
					{
						return OH_GRAB_OBJECT;
					}
				}
			}

			const bool bPICK_UP_OBJECTS_MP = gEnv->bMultiplayer && g_pGameCVars->mp_pickupObjects;
			if (bPICK_UP_OBJECTS_MP)
			{
				//CryMP: Objects that are not bound to network, cannot be picked up in MP (except chickens etc)
				const bool bClientEntity = (pEntity->GetFlags() & ENTITY_FLAG_CLIENT_ONLY);
				const bool bIsBound = m_pGameFramework->GetNetContext()->IsBound(entityId);
				const IEntityClass* pClass = pEntity->GetClass();
				if (!bIsBound && !bClientEntity)
				{
					return OH_NO_GRAB;
				}

				if (pClass == CItem::sDoorClass ||
					pClass == CItem::sElevatorSwitchClass ||
					pClass == CItem::sFlagClass ||
					pClass == CItem::sGeomEntityClass)
				{
					return OH_NO_GRAB;
				}
			}

			if (bPICK_UP_OBJECTS_MP || !gEnv->bMultiplayer)
			{
				//CryMP: Allow picking up projectiles 
				if (g_pGame->GetWeaponSystem()->GetProjectile(entityId))
				{
					const pe_type physicsType = pEntity->GetPhysics() ? pEntity->GetPhysics()->GetType() : PE_NONE;
					//Only allow PE_RIGID
					if (physicsType != PE_RIGID && physicsType != PE_PARTICLE)
					{
						return OH_NO_GRAB;
					}
					m_grabType = GRAB_TYPE_ONE_HANDED;
					return OH_GRAB_OBJECT;
				}
			}

			//3. -If we found a helper, it has to be pickable
			if (m_hasHelper && pPhysicalEntity->GetType() == PE_RIGID)
			{
				SmartScriptTable props;
				IScriptTable* pEntityScript = pEntity->GetScriptTable();
				if (pEntityScript && pEntityScript->GetValue("Properties", props))
				{
					//If it's not pickable, ignore helper
					int pickable = 0;
					if (props->GetValue("bPickable", pickable) && !pickable)
						return OH_NO_GRAB;
				}
				return OH_GRAB_OBJECT;
			}

			//4. Pick boid object
			if ((pPhysicalEntity->GetType() == PE_PARTICLE || (pPhysicalEntity->GetType() == PE_ARTICULATED && m_grabType == GRAB_TYPE_TWO_HANDED)) && m_hasHelper)
				return OH_GRAB_OBJECT;

			//5. -Procedurally breakable object (most likely...)
			if (breakable)
			{
				//Set "hold" matrix
				if (pPart.pPhysGeom->V < 0.35f && pPart.pPhysGeom->Ibody.len() < 0.1)
				{
					m_holdOffset.SetTranslation(pPart.pPhysGeom->origin + Vec3(0.0f, -0.15f, 0.0f));
					m_holdOffset.InvertFast();
					return OH_GRAB_OBJECT;
				}

			}

			//6.- Temp? solution for spawned rocks (while they don't have helpers)
			if (pPhysicalEntity->GetType() == PE_RIGID && !strcmp(pEntity->GetClass()->GetName(), "rock"))
			{
				m_grabType = GRAB_TYPE_ONE_HANDED;
				return OH_GRAB_OBJECT;
			}

			//7.- Legacy system...
			SmartScriptTable props;
			IScriptTable* pEntityScript = pEntity->GetScriptTable();
			if (pPhysicalEntity->GetType() == PE_RIGID && pEntityScript && pEntityScript->GetValue("Properties", props))
			{
				int pickable = 0;
				int usable = 0;
				if (props->GetValue("bPickable", pickable) && !pickable) 
					return false;
				else if (pickable)
					if (props->GetValue("bUsable", usable) && !usable)
						return OH_GRAB_OBJECT;

				return false;
			}

			if (getEntityInfo && !isRemote)
			{
				m_preHeldEntityId = 0;
			}

			return OH_NO_GRAB;//CheckItemsInProximity(info.eyePosition, info.eyeDirection, getEntityInfo);
		}
		else if (pPhysicalEntity->GetType() == PE_STATIC)
		{
			//Rocks and small static vegetation marked as pickable
			IRenderNode* pRenderNode = 0;
			pe_params_foreign_data pfd;
			if (pPhysicalEntity->GetParams(&pfd) && pfd.iForeignData == PHYS_FOREIGN_ID_STATIC)
				pRenderNode = static_cast<IRenderNode*>(pfd.pForeignData);

			if (pRenderNode && pRenderNode->GetRndFlags() & ERF_PICKABLE)
			{
				if (getEntityInfo)
				{
					m_grabType = GRAB_TYPE_ONE_HANDED;
					m_pRockRN = pRenderNode;
					if (!isRemote)
					{
						m_preHeldEntityId = 0;
					}
				}
				return OH_GRAB_OBJECT;
			}
		}
	}
	return CheckItemsInProximity(info.eyePosition, info.eyeDirection, getEntityInfo);
}

//========================================================================================
void COffHand::OnLookAtEntityChanged(IEntity* pEntity)
{
	if (pEntity && m_pVehicleSystem->GetVehicle(pEntity->GetId()))
	{
		//CryMP: Fix missing Enter vehicle HUD display
		//Usually a punch solves it, the following awake code fixes it
		//The bugged vehicles have submergedFraction 1.0
		AwakeEntityPhysics(pEntity);
	}
}

//========================================================================================
int COffHand::CheckItemsInProximity(Vec3 pos, Vec3 dir, bool getEntityInfo)
{
	float sizeX = 1.2f;
	if (gEnv->bMultiplayer)
		sizeX = 1.75;
	float sizeY = sizeX;
	float sizeZup = 0.5f;
	float sizeZdown = 1.75f;

	SEntityProximityQuery query;
	query.box = AABB(Vec3(pos.x - sizeX, pos.y - sizeY, pos.z - sizeZdown),
		Vec3(pos.x + sizeX, pos.y + sizeY, pos.z + sizeZup));
	query.nEntityFlags = ~0; // Filter by entity flag.

	float minDstSqr = 0.2f;
	EntityId nearItemId = 0;
	int count = m_pEntitySystem->QueryProximity(query);
	for (int i = 0; i < query.nCount; i++)
	{
		EntityId id = query.pEntities[i]->GetId();

		if (CItem* pItem = static_cast<CItem*>(m_pItemSystem->GetItem(id)))
		{
			if (pItem->GetOwnerId() != GetOwnerId())
			{
				Line line(pos, dir);
				AABB bbox;
				pItem->GetEntity()->GetWorldBounds(bbox);
				Vec3 itemPos = bbox.GetCenter();
				if (dir.Dot(itemPos - pos) > 0.0f)
				{
					float dstSqr = LinePointDistanceSqr(line, itemPos);
					if (dstSqr < 0.2f)
					{
						if ((dstSqr < minDstSqr) && pItem->CanPickUp(GetOwnerId()))
						{
							minDstSqr = dstSqr;
							nearItemId = id;
						}
					}
				}
			}
		}
	}

	if (IEntity* pEntity = m_pEntitySystem->GetEntity(nearItemId))
	{
		AABB bbox;
		pEntity->GetWorldBounds(bbox);
		Vec3 itemPos = bbox.GetCenter();
		Vec3 newDir = (pos - itemPos);

		IPhysicalEntity* phys = pEntity->GetPhysics();

		ray_hit hit;
		if (!gEnv->pPhysicalWorld->RayWorldIntersection(itemPos, newDir, ent_static | ent_rigid | ent_sleeping_rigid,
			rwi_stop_at_pierceable | rwi_ignore_back_faces, &hit, 1, phys ? &phys : NULL, phys ? 1 : 0))
		{
			//If nothing in between...
			m_crosshairId = nearItemId;

			SelectGrabType(pEntity);
			if (getEntityInfo)
				m_preHeldEntityId = m_crosshairId;

			return OH_GRAB_ITEM;
		}
	}

	return OH_NO_GRAB;
}

//==========================================================================================
bool COffHand::PerformPickUp()
{
	bool setHeld = false;

	//If we are here, we must have the entity ID
	if (m_preHeldEntityId)
	{
		SetHeldEntityId(m_preHeldEntityId);
		m_preHeldEntityId = 0;
		setHeld = true;
	}
	m_startPickUp = false;
	IEntity* pEntity = NULL;

	if (m_heldEntityId)
	{
		pEntity = m_pEntitySystem->GetEntity(m_heldEntityId);
	}
	else if (m_pRockRN)
	{
		SetHeldEntityId(SpawnRockProjectile(m_pRockRN));
		setHeld = true;

		m_pRockRN = NULL;
		if (!m_heldEntityId)
			return false;

		pEntity = m_pEntitySystem->GetEntity(m_heldEntityId);
		SelectGrabType(pEntity);
		m_grabType = GRAB_TYPE_ONE_HANDED; //Force for now
	}

	if (pEntity)
	{
		if (!setHeld)
		{
			m_heldEntityId = 0;
			SetHeldEntityId(pEntity->GetId());
		}

		CActor* pActor = GetOwnerActor();
		// Send event to entity.
		SEntityEvent entityEvent;
		entityEvent.event = ENTITY_EVENT_PICKUP;
		entityEvent.nParam[0] = 1;
		if (pActor)
			entityEvent.nParam[1] = pActor->GetEntityId();
		pEntity->SendEvent(entityEvent);

		m_holdScale = pEntity->GetScale();

		//if (m_pVehicleSystem->GetVehicle(pEntity->GetId()))
		//{
		//	m_holdScale *= 0.3f;
		//}

		if (pActor && pActor->IsPlayer())
		{
			m_heldEntityMass = 1.0f;
			if (IPhysicalEntity* pPhy = pEntity->GetPhysics())
			{
				pe_status_dynamics dynStat;
				if (pPhy->GetStatus(&dynStat))
				{
					m_heldEntityMass = dynStat.mass;
				}
				if (pPhy->GetType() == PE_PARTICLE)
				{
					m_intialBoidLocalMatrix = pEntity->GetSlotLocalTM(0, false);
				}
				else if (pPhy->GetType() == PE_RIGID)
				{
					//// Make it temporarily unbreakable
					//pe_params_structural_joint lock;
					//lock.idx = pj.idx;
					//lock.bBreakable = 0;
					//pPhys->SetParams(&lock);

					pe_params_structural_joint pj;
					pj.idx = 0;
					if (pPhy->GetParams(&pj))
					{
						if (pj.bBreakable)
						{
							CryLogAlways("$3This object is breakable");
						}
					}
				}
			}
			// only if we're picking up a normal (non Item) object
			if (m_currentState & eOHS_PICKING)
			{
				if (CPlayer* pPlayer = CPlayer::FromActor(pActor))
				{
					pPlayer->NotifyObjectGrabbed(true, m_heldEntityId, false, (m_grabType == GRAB_TYPE_TWO_HANDED));
				}
			}

			//CryMP
			if (pActor->IsRemote())
			{
				if (!IsSelected())
				{
					Select(true);
				}
			}
		}

		SetDefaultIdleAnimation(eIGS_FirstPerson, m_grabTypes[m_grabType].idle);

		return true;
	}

	return false;
}

//===========================================================================================
void COffHand::SetIgnoreCollisionsWithOwner(bool activate, EntityId entityId /*=0*/)
{
	if (!m_heldEntityId && !entityId)
		return;

	IEntity* pEntity = m_pEntitySystem->GetEntity(m_heldEntityId ? m_heldEntityId : entityId);
	if (!pEntity)
		return;

	IPhysicalEntity* pPE = pEntity->GetPhysics();
	if (!pPE)
		return;

	if (pPE->GetType() == PE_PARTICLE)
	{
		m_constraintId = 0;
		m_constraintStatus = ConstraintStatus::Inactive;
		return;
	}

	if (activate)
	{
		if (pEntity->IsHidden())
			return;

		//The constraint doesn't work with Items physicalized as static
		if (pPE->GetType() == PE_STATIC)
		{
			IItem* pItem = m_pItemSystem->GetItem(pEntity->GetId());
			if (pItem)
			{
				pItem->Physicalize(true, true);
				pPE = pEntity->GetPhysics();
				assert(pPE);
			}
		}

		CActor* pActor = GetOwnerActor();
		if (!pActor)
			return;

		pe_action_add_constraint ic;
		ic.flags = constraint_inactive | constraint_ignore_buddy;
		ic.pBuddy = pActor->GetEntity()->GetPhysics();

		if (g_pGameCVars->mp_buyPageKeepTime == 80)
		{
			ICharacterInstance* pCharacter = pActor->GetEntity()->GetCharacter(0);
			IPhysicalEntity* pPhysEnt = pCharacter ? pCharacter->GetISkeletonPose()->GetCharacterPhysics(-1) : NULL;

			if (pPhysEnt)
			{
				pe_simulation_params sp;
				pPhysEnt->GetParams(&sp);
				//if (sp.iSimClass <= 2)
				{
					ic.pBuddy = pPhysEnt;

					CryLogAlways("$3Adding character constraint instead");
				}
			}

		}

		ic.pt[0].Set(0, 0, 0);
		m_constraintId = pPE->Action(&ic);
		m_constraintStatus = ConstraintStatus::WaitForPhysicsUpdate;
	}
	else
	{
		pe_action_update_constraint up;
		up.bRemove = true;
		up.idConstraint = m_constraintId;
		m_constraintId = 0;
		m_constraintStatus = ConstraintStatus::Inactive;
		pPE->Action(&up);
	}
}

//==========================================================================================
void COffHand::DrawNear(bool drawNear, EntityId entityId /*=0*/)
{
	IEntity* pEntity = m_pEntitySystem->GetEntity(m_heldEntityId ? m_heldEntityId : entityId);
	if (!pEntity)
		return;

	int nslots = pEntity->GetSlotCount();
	for (int i = 0;i < nslots;i++)
	{
		if (pEntity->GetSlotFlags(i) & ENTITY_SLOT_RENDER)
		{
			if (drawNear)
			{
				pEntity->SetSlotFlags(i, pEntity->GetSlotFlags(i) | ENTITY_SLOT_RENDER_NEAREST);
				if (IEntityRenderProxy* pProxy = (IEntityRenderProxy*)pEntity->GetProxy(ENTITY_PROXY_RENDER))
				{
					if (IRenderNode* pRenderNode = pProxy->GetRenderNode())
						pRenderNode->SetRndFlags(ERF_REGISTER_BY_POSITION, true);
				}
				if (IEntityPhysicalProxy* pPhysicsProxy = static_cast<IEntityPhysicalProxy*>(pEntity->GetProxy(ENTITY_PROXY_PHYSICS)))
					pPhysicsProxy->DephysicalizeFoliage(i);

			}
			else
			{
				pEntity->SetSlotFlags(i, pEntity->GetSlotFlags(i) & (~ENTITY_SLOT_RENDER_NEAREST));
				if (IEntityRenderProxy* pProxy = (IEntityRenderProxy*)pEntity->GetProxy(ENTITY_PROXY_RENDER))
				{
					if (IRenderNode* pRenderNode = pProxy->GetRenderNode())
						pRenderNode->SetRndFlags(ERF_REGISTER_BY_POSITION, false);
				}
				if (IEntityPhysicalProxy* pPhysicsProxy = static_cast<IEntityPhysicalProxy*>(pEntity->GetProxy(ENTITY_PROXY_PHYSICS)))
					pPhysicsProxy->PhysicalizeFoliage(i);

			}
		}
	}
}

//=========================================================================================
void COffHand::SelectGrabType(IEntity* pEntity)
{
	if (!pEntity)
		return;

	CActor* pActor = GetOwnerActor();
	if (!pActor)
		return;

	if (g_pGame->GetWeaponSystem()->GetProjectile(pEntity->GetId()))
	{
		//If it's a projectile, we can only grab it with one hand
		m_grabType = GRAB_TYPE_ONE_HANDED;
		m_holdOffset.SetIdentity();
		m_hasHelper = false;
		return;
	}

	// iterate over the grab types and see if this object supports one
	m_grabType = GRAB_TYPE_ONE_HANDED;
	const TGrabTypes::const_iterator end = m_grabTypes.end();
	for (TGrabTypes::const_iterator i = m_grabTypes.begin(); i != end; ++i, ++m_grabType)
	{
		SEntitySlotInfo slotInfo;
		for (int n = 0; n < pEntity->GetSlotCount(); n++)
		{
			if (!pEntity->IsSlotValid(n))
				continue;

			bool ok = pEntity->GetSlotInfo(n, slotInfo) && (pEntity->GetSlotFlags(n) & ENTITY_SLOT_RENDER);
			if (ok && slotInfo.pStatObj)
			{
				//Iterate two times (normal helper name, and composed one)
				for (int j = 0; j < 2; j++)
				{
					string helper;
					helper.clear();
					if (j == 0)
					{
						helper.append(slotInfo.pStatObj->GetGeoName()); helper.append("_"); helper.append((*i).helper.c_str());
					}
					else
						helper.append((*i).helper.c_str());

					//It is already a subobject, we have to search in the parent
					if (slotInfo.pStatObj->GetParentObject())
					{
						IStatObj::SSubObject* pSubObj = slotInfo.pStatObj->GetParentObject()->FindSubObject(helper.c_str());
						if (pSubObj)
						{
							m_holdOffset = pSubObj->tm;
							m_holdOffset.OrthonormalizeFast();
							m_holdOffset.InvertFast();
							m_hasHelper = true;
							return;
						}
					}
					else {
						IStatObj::SSubObject* pSubObj = slotInfo.pStatObj->FindSubObject(helper.c_str());
						if (pSubObj)
						{
							//DumpStatObj(slotInfo.pStatObj);

							m_holdOffset = pSubObj->tm;
							m_holdOffset.OrthonormalizeFast();
							m_holdOffset.InvertFast();
							m_hasHelper = true;
							return;
						}
					}
				}
			}
			else if (ok && slotInfo.pCharacter)
			{
				//Grabbing helpers for boids/animals
				IAttachmentManager* pAM = slotInfo.pCharacter->GetIAttachmentManager();
				if (pAM)
				{
					IAttachment* pAttachment = pAM->GetInterfaceByName((*i).helper.c_str());
					if (pAttachment)
					{
						m_holdOffset = Matrix34(pAttachment->GetAttAbsoluteDefault().q);
						m_holdOffset.SetTranslation(pAttachment->GetAttAbsoluteDefault().t);
						m_holdOffset.OrthonormalizeFast();
						m_holdOffset.InvertFast();
						if (m_grabType == GRAB_TYPE_TWO_HANDED)
							m_holdOffset.AddTranslation(Vec3(0.1f, 0.0f, -0.12f));
						m_hasHelper = true;
						return;
					}
				}
			}
		}
	}

	// when we come here, we haven't matched any of the predefined helpers ... so try to make a 
	// smart decision based on how large the object is
	//float volume(0),heavyness(0);
	//pActor->CanPickUpObject(pEntity, heavyness, volume);

	// grabtype 0 is onehanded and 1 is twohanded
	//m_grabType = (volume>0.08f) ? 1 : 0;
	m_holdOffset.SetIdentity();
	m_hasHelper = false;
	m_grabType = GRAB_TYPE_TWO_HANDED;
}

//=========================================================================================
bool COffHand::IsGrabTypeTwoHanded(const EntityId entityId) const noexcept
{
	IEntity *pEntity = m_pEntitySystem->GetEntity(entityId);
	if (!pEntity)
		return false;

	// Emulate original index-based mapping:
	// start at ONE_HANDED and ++ per entry in m_grabTypes
	uint32 grabType = GRAB_TYPE_ONE_HANDED;

	for (const auto& gt : m_grabTypes)
	{
		SEntitySlotInfo slotInfo{};
		const int slotCount = pEntity->GetSlotCount();

		for (int n = 0; n < slotCount; ++n)
		{
			if (!pEntity->IsSlotValid(n))
				continue;

			const bool ok = pEntity->GetSlotInfo(n, slotInfo) && (pEntity->GetSlotFlags(n) & ENTITY_SLOT_RENDER);
			if (!ok)
				continue;

			if (slotInfo.pStatObj)
			{
				const char* geoName = slotInfo.pStatObj->GetGeoName();

				for (int j = 0; j < 2; ++j)
				{
					string helper;
					if (j == 0)
					{
						helper.reserve((geoName ? strlen(geoName) : 0) + 1 + gt.helper.length());
						if (geoName) helper.append(geoName);
						helper.push_back('_');
						helper.append(gt.helper.c_str());
					}
					else
					{
						helper = gt.helper.c_str();
					}

					if (slotInfo.pStatObj->GetParentObject())
					{
						if (IStatObj::SSubObject* pSubObj =
							slotInfo.pStatObj->GetParentObject()->FindSubObject(helper.c_str()))
						{
							return (grabType == GRAB_TYPE_TWO_HANDED);
						}
					}
					else
					{
						if (IStatObj::SSubObject* pSubObj =
							slotInfo.pStatObj->FindSubObject(helper.c_str()))
						{
							return (grabType == GRAB_TYPE_TWO_HANDED);
						}
					}
				}
			}
			else if (slotInfo.pCharacter)
			{
				if (IAttachmentManager* pAM = slotInfo.pCharacter->GetIAttachmentManager())
				{
					if (IAttachment* pAttachment = pAM->GetInterfaceByName(gt.helper.c_str()))
					{
						return (grabType == GRAB_TYPE_TWO_HANDED);
					}
				}
			}
		}

		++grabType;
	}

	// If no predefined helper matched, mirror original fallback: default to TWO_HANDED
	return true;
}

//=========================================================================================
// Returns the same hold offset SelectGrabType would produce, without touching member state.
Matrix34 COffHand::GetHoldOffset(IEntity* pEntity)
{
	Matrix34 holdOffset(IDENTITY);

	CActor* pActor = GetOwnerActor();
	if (!pActor || !pEntity)
		return holdOffset;

	// Projectile special case => identity (SelectGrabType sets one-handed, but offset is identity)
	if (g_pGame->GetWeaponSystem()->GetProjectile(pEntity->GetId()))
		return holdOffset;

	// Iterate grab types exactly like SelectGrabType (incrementing the grab type as we go)
	int trialGrabType = GRAB_TYPE_ONE_HANDED;
	const TGrabTypes::const_iterator end = m_grabTypes.end();
	for (TGrabTypes::const_iterator i = m_grabTypes.begin(); i != end; ++i, ++trialGrabType)
	{
		SEntitySlotInfo slotInfo;
		for (int n = 0; n < pEntity->GetSlotCount(); ++n)
		{
			if (!pEntity->IsSlotValid(n))
				continue;

			bool ok = pEntity->GetSlotInfo(n, slotInfo) && (pEntity->GetSlotFlags(n) & ENTITY_SLOT_RENDER);
			if (!ok)
				continue;

			// --- Static object path ---
			if (slotInfo.pStatObj)
			{
				for (int j = 0; j < 2; ++j)
				{
					string helper;
					helper.clear();
					if (j == 0)
					{
						helper.append(slotInfo.pStatObj->GetGeoName());
						helper.append("_");
						helper.append((*i).helper.c_str());
					}
					else
					{
						helper.append((*i).helper.c_str());
					}

					if (slotInfo.pStatObj->GetParentObject())
					{
						IStatObj::SSubObject* pSubObj =
							slotInfo.pStatObj->GetParentObject()->FindSubObject(helper.c_str());
						if (pSubObj)
						{
							holdOffset = pSubObj->tm;
							holdOffset.OrthonormalizeFast();
							holdOffset.InvertFast();
							return holdOffset;
						}
					}
					else
					{
						IStatObj::SSubObject* pSubObj =
							slotInfo.pStatObj->FindSubObject(helper.c_str());
						if (pSubObj)
						{
							holdOffset = pSubObj->tm;
							holdOffset.OrthonormalizeFast();
							holdOffset.InvertFast();
							return holdOffset;
						}
					}
				}
			}
			// --- Character path (boids/animals) ---
			else if (slotInfo.pCharacter)
			{
				IAttachmentManager* pAM = slotInfo.pCharacter->GetIAttachmentManager();
				if (pAM)
				{
					IAttachment* pAttachment = pAM->GetInterfaceByName((*i).helper.c_str());
					if (pAttachment)
					{
						holdOffset = Matrix34(pAttachment->GetAttAbsoluteDefault().q);
						holdOffset.SetTranslation(pAttachment->GetAttAbsoluteDefault().t);
						holdOffset.OrthonormalizeFast();
						holdOffset.InvertFast();

						// Apply the exact two-handed tweak from SelectGrabType
						if (trialGrabType == GRAB_TYPE_TWO_HANDED)
							holdOffset.AddTranslation(Vec3(0.1f, 0.0f, -0.12f));

						return holdOffset;
					}
				}
			}
		}
	}

	// No helper found => identity (SelectGrabType also leaves offset identity in this case)
	holdOffset.SetIdentity();
	return holdOffset;
}

//========================================================================================================
void COffHand::StartPickUpItem()
{
	CPlayer* pPlayer = static_cast<CPlayer*>(GetOwnerActor());
	assert(pPlayer && "COffHand::StartPickUpItem -> No player found!!");

	bool drop_success = false;

	IEntity* pPreHeldEntity = m_pEntitySystem->GetEntity(m_preHeldEntityId);

	// determine position of entity which will be picked up
	if (!pPreHeldEntity)
		return;

	if (!pPlayer->CheckInventoryRestrictions(pPreHeldEntity->GetClass()->GetName()))
	{
		//Can not carry more heavy/medium weapons
		//Drop existing weapon and pick up the other

		IItem* pItem = GetExchangeItem(pPlayer);
		if (pItem)
		{
			if (pPlayer->DropItem(pItem->GetEntityId(), 8.0f, true))
				drop_success = true;
		}

		if (!drop_success)
		{
			g_pGame->GetGameRules()->OnTextMessage(eTextMessageError, "@mp_CannotCarryMore");
			CancelAction();
			return;
		}
	}
	else if (IItem* pItem = m_pItemSystem->GetItem(m_preHeldEntityId))
	{
		if (!pItem->CheckAmmoRestrictions(pPlayer->GetEntityId()))
		{
			CryFixedStringT<128> itemName(pItem->GetEntity()->GetClass()->GetName());
			if (!_stricmp(itemName.c_str(), "CustomAmmoPickup"))
			{
				SmartScriptTable props;
				if (pItem->GetEntity()->GetScriptTable() && pItem->GetEntity()->GetScriptTable()->GetValue("Properties", props))
				{
					const char* name = NULL;
					if (props->GetValue("AmmoName", name))
						itemName.assign(name);
				}
			}
			if (g_pGame->GetHUD())
			{
				CryFixedStringT<128> temp("@");
				temp.append(itemName.c_str());
				g_pGame->GetHUD()->DisplayFlashMessage("@ammo_maxed_out", 2, ColorF(1.0f, 0, 0), true, temp.c_str());
			}
			CancelAction();
			return;
		}
	}

	//No animation in MP
	if (gEnv->bMultiplayer)
	{
		SetOffHandState(eOHS_PICKING_ITEM2);

		pPlayer->PickUpItem(m_preHeldEntityId, true);
		CancelAction();
		return;
	}

	//Unzoom weapon if neccesary
	if (m_mainHandWeapon && (m_mainHandWeapon->IsZoomed() || m_mainHandWeapon->IsZooming()))
	{
		m_mainHandWeapon->ExitZoom();
		m_mainHandWeapon->ExitViewmodes();
	}

	if (m_mainHandWeapon && m_mainHandWeapon->IsWeaponRaised())
	{
		m_mainHandWeapon->RaiseWeapon(false, true);
		m_mainHandWeapon->SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, g_pItemStrings->idle);
	}

	//Everything seems ok, start the action...
	SetOffHandState(eOHS_PICKING_ITEM);

	m_pickingTimer = 0.3f;
	m_mainHandIsDualWield = false;
	pPlayer->NeedToCrouch(pPreHeldEntity->GetWorldPos());

	if (m_mainHand && (m_mainHand->TwoHandMode() == 1 || drop_success))
	{
		GetOwnerActor()->HolsterItem(true);

		SetMainHand(nullptr);
		SetMainHandWeapon(nullptr);
	}
	else
	{
		if (m_mainHand)
		{
			if (m_mainHand->IsDualWield() && m_mainHand->GetDualWieldSlave())
			{
				SetMainHand(static_cast<CItem*>(m_mainHand->GetDualWieldSlave()));

				m_mainHand->Select(false);
				m_mainHandIsDualWield = true;
			}
			else
			{
				m_mainHand->PlayAction(g_pItemStrings->offhand_on);
				m_mainHand->SetActionSuffix("akimbo_");
			}
		}
	}

	PlayAction(g_pItemStrings->pickup_weapon_left, 0, false, eIPAF_Default | eIPAF_RepeatLastFrame);
	GetScheduler()->TimerAction(GetCurrentAnimationTime(eIGS_FirstPerson) + 100, 
		CSchedulerAction<Timer_FinishOffHandAction>::Create(Timer_FinishOffHandAction(eOHA_PICK_ITEM, this)), false);
	RequireUpdate(eIUS_General);
	m_startPickUp = true;
}

//=========================================================================================================
void COffHand::EndPickUpItem()
{
	IFireMode* pReloadFM = 0;
	EntityId prevWeaponId = 0;
	if (m_mainHandWeapon && IsServer() && m_mainHand)
	{
		prevWeaponId = m_mainHand->GetEntityId();
		pReloadFM = m_mainHandWeapon->GetFireMode(m_mainHandWeapon->GetCurrentFireMode());
		if (pReloadFM)
		{
			int fmAmmo = pReloadFM->GetAmmoCount();
			int invAmmo = GetInventoryAmmoCount(pReloadFM->GetAmmoType());
			if (pReloadFM && (pReloadFM->GetAmmoCount() != 0 || GetInventoryAmmoCount(pReloadFM->GetAmmoType()) != 0))
			{
				pReloadFM = 0;
			}
		}
	}

	//Restore main weapon
	if (m_mainHand)
	{
		if (m_mainHandIsDualWield)
		{
			m_mainHand->Select(true);
		}
		else
		{
			m_mainHand->ResetDualWield();
			m_mainHand->PlayAction(g_pItemStrings->offhand_off, 0, false, eIPAF_Default | eIPAF_NoBlend);
		}
	}
	else
	{
		GetOwnerActor()->HolsterItem(false);
	}

	//Pick-up the item, and reset offhand
	SetOffHandState(eOHS_PICKING_ITEM2);

	GetOwnerActor()->PickUpItem(m_heldEntityId, true);
	SetIgnoreCollisionsWithOwner(false, m_heldEntityId);

	SetOffHandState(eOHS_INIT_STATE);

	// if we were in a reload position and weapon hasn't changed, try to reload it
	// this will fail automatically, if we didn't get any new ammo
	// it will only occur on the server due to checks at the start of this function
	if (pReloadFM)
	{
		IItem* pItem = GetOwnerActor()->GetCurrentItem();
		if (pItem && pItem->GetEntityId() == prevWeaponId)
		{
			if (IWeapon* pWeapon = pItem->GetIWeapon())
			{
				pWeapon->Reload(false);
			}
		}
	}
}

//=======================================================================================
void COffHand::StartPickUpObject(const EntityId entityId, bool isLivingEnt /* = false */)
{
	//Grab NPCs-----------------
	if (isLivingEnt)
	{
		if (!GrabNPC())
		{
			CancelAction();
		}
	}
	//-----------------------

	//Don't pick up in prone
	CPlayer* pPlayer = static_cast<CPlayer*>(GetOwnerActor());
	if (!pPlayer)
		return;

	if (!gEnv->bMultiplayer)
	{
		if (pPlayer->IsClient() && pPlayer->GetStance() == STANCE_PRONE)
		{
			CancelAction();
			return;
		}

		const CActor::ObjectHoldType armHoldType = DetermineObjectHoldType(entityId);

		pPlayer->SetHeldObjectId(entityId, armHoldType);

	}

	if (pPlayer->IsRemote()) //TEST
	{
		SetMainHandWeapon(GetOwnerActor()->GetCurrentWeapon(false));
		SetMainHand(static_cast<CItem*>(GetOwnerActor()->GetCurrentItem()));
	}

	//Unzoom weapon if neccesary
	if (m_mainHandWeapon && (m_mainHandWeapon->IsZoomed() || m_mainHandWeapon->IsZooming()))
	{
		m_mainHandWeapon->ExitZoom();
		m_mainHandWeapon->ExitViewmodes();
	}

	// if two handed or dual wield we use the fists as the mainhand weapon
	if (m_mainHand && (m_grabTypes[m_grabType].twoHanded || m_mainHand->TwoHandMode() >= 1))
	{
		if (m_grabTypes[m_grabType].twoHanded)
		{
			GetOwnerActor()->HolsterItem(true);
			
			SetMainHand(nullptr);
			SetMainHandWeapon(nullptr);
		}
		else
		{
			m_prevMainHandId = m_mainHand->GetEntityId();
			GetOwnerActor()->SelectItemByName("Fists", false);

			SetMainHand(GetActorItem(GetOwnerActor()));
			SetMainHandWeapon(static_cast<CWeapon*>(m_mainHand ? m_mainHand->GetIWeapon() : nullptr));
		}
	}

	if (m_mainHand)
	{

		if (m_mainHandWeapon && m_mainHandWeapon->IsWeaponRaised())
		{
			m_mainHandWeapon->RaiseWeapon(false, true);
			m_mainHandWeapon->SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, g_pItemStrings->idle);
		}

		if (m_mainHand->IsDualWield() && m_mainHand->GetDualWieldSlave())
		{
			SetMainHand(static_cast<CItem*>(m_mainHand->GetDualWieldSlave()));

			m_mainHand->Select(false);
			m_mainHandIsDualWield = true;
		}
		else
		{
			m_mainHand->PlayAction(g_pItemStrings->offhand_on);
			m_mainHand->SetActionSuffix("akimbo_");
		}


		if (m_mainHandWeapon && pPlayer->IsClient())
		{
			IFireMode* pFireMode = m_mainHandWeapon->GetFireMode(m_mainHandWeapon->GetCurrentFireMode());
			if (pFireMode)
			{
				pFireMode->SetRecoilMultiplier(1.5f);		//Increase recoil for the weapon
			}
		}
	}
	if (!isLivingEnt)
	{
		SetOffHandState(eOHS_PICKING);

		if (!m_preHeldEntityId)
		{
			m_preHeldEntityId = entityId;
		}

		if (pPlayer->IsRemote()) //TEST
		{
			PerformPickUp();
		}
		else
		{
			m_pickingTimer = 0.3f;

			//PerformPickUp();
		}

		if (g_pGameCVars->mp_pickupDebug)
			CryLogAlways("COffHand::StartPickUpObject: Picking up object with fireMode $7%d", GetCurrentFireMode());

		SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, m_grabTypes[m_grabType].idle);
		PlayAction(m_grabTypes[m_grabType].pickup);

		if (pPlayer->IsRemote())
		{
			FinishAction(eOHA_PICK_OBJECT);
		}
		else
		{
			GetScheduler()->TimerAction(GetCurrentAnimationTime(eIGS_FirstPerson),
				CSchedulerAction<Timer_FinishOffHandAction>::Create(Timer_FinishOffHandAction(eOHA_PICK_OBJECT, this)), false);
		}

		m_startPickUp = true;
	}
	else
	{
		SetOffHandState(eOHS_GRABBING_NPC);

		m_grabType = GRAB_TYPE_NPC;

		SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, m_grabTypes[m_grabType].idle);
			PlayAction(m_grabTypes[m_grabType].pickup);

		GetScheduler()->TimerAction(GetCurrentAnimationTime(eIGS_FirstPerson),
			CSchedulerAction<Timer_FinishOffHandAction>::Create(Timer_FinishOffHandAction(eOHA_GRAB_NPC, this)), false);
	}
	RequireUpdate(eIUS_General);
}

//=========================================================================================
void COffHand::StartThrowObject(const EntityId entityId, int activationMode, bool isLivingEnt /*= false*/)
{
	if (!entityId)
		return;

	CActor* pActor = GetOwnerActor();
	if (!pActor)
		return;

	if (!gEnv->bMultiplayer && activationMode == eAAM_OnPress) //CryMP: moved this to request pickup action for MultiPlayer
	{
		if (GetCurrentFireMode() < MAX_GRENADE_TYPES)
		{
			m_lastFireModeId = GetCurrentFireMode();
		}
		if (entityId)
		{
			RequestFireMode(GetFireModeIdx(m_grabTypes[m_grabType].throwFM.c_str()));
		}
	}

	PerformThrow(activationMode, entityId, m_lastFireModeId, isLivingEnt);

	if (m_mainHandWeapon)
	{
		IFireMode* pFireMode = m_mainHandWeapon->GetFireMode(m_mainHandWeapon->GetCurrentFireMode());
		if (pFireMode)
		{
			pFireMode->SetRecoilMultiplier(1.0f);		//Restore normal recoil for the weapon
		}
	}
}

//==========================================================================================
bool COffHand::GrabNPC()
{
	CActor* pActor = GetOwnerActor();

	if (!pActor)
		return false;

	//Do not grab in prone
	if (pActor->GetStance() == STANCE_PRONE)
		return false;

	//Get actor
	CActor* pHeldActor = static_cast<CActor*>(m_pActorSystem->GetActor(m_preHeldEntityId));

	if (!pHeldActor)
		return false;

	IEntity* pEntity = pHeldActor->GetEntity();
	if (!pEntity->GetCharacter(0))
		return false;

	//The NPC holster his weapon
	bool mounted = false;
	CItem* currentItem = static_cast<CItem*>(pHeldActor->GetCurrentItem());
	if (currentItem)
	{
		if (currentItem->IsMounted() && currentItem->IsUsed())
		{
			currentItem->StopUse(pHeldActor->GetEntityId());
			mounted = true;
		}
	}

	if (!mounted)
	{
		pHeldActor->HolsterItem(false); //AI sometimes has holstered a weapon and selected a different one...
		pHeldActor->HolsterItem(true);
	}

	if (IAnimationGraphState* pAGState = pHeldActor->GetAnimationGraphState())
	{
		char value[256];
		IAnimationGraphState::InputID actionInputID = pAGState->GetInputId("Action");
		pAGState->GetInput(actionInputID, value);
		if (strcmp(value, "grabStruggleFP") != 0)
		{
			// this is needed to make sure the transition is super fast
			if (ICharacterInstance* pCharacter = pEntity->GetCharacter(0))
				if (ISkeletonAnim* pSkeletonAnim = pCharacter->GetISkeletonAnim())
					pSkeletonAnim->StopAnimationsAllLayers();
		}

		// just in case clear the Signal
		pAGState->SetInput("Signal", "none");
		pAGState->SetInput(actionInputID, "grabStruggleFP");
	}
	if (SActorStats* pStats = pActor->GetActorStats())
		pStats->grabbedTimer = 0.0f;

	// this needs to be done before sending signal "OnFallAndPlay" to make sure
	// in case AG state was FallAndPlay we leave it before AI is disabled
	if (IAnimationGraphState* pAGState = pHeldActor->GetAnimationGraphState())
	{
		pAGState->ForceTeleportToQueriedState();
		pAGState->Update();
	}

	if (IAISystem* pAISystem = gEnv->pAISystem)
	{
		IAIActor* pAIActor = CastToIAIActorSafe(pEntity->GetAI());
		if (pAIActor)
		{
			IAISignalExtraData* pEData = pAISystem->CreateSignalExtraData();
			pEData->point = Vec3(0, 0, 0);
			pAIActor->SetSignal(1, "OnFallAndPlay", 0, pEData);
		}
	}

	//Disable IK
	SActorStats* pStats = pHeldActor->GetActorStats();
	if (pStats)
	{
		pStats->isGrabbed = true;
	}

	SetHeldEntityId(pHeldActor->GetEntityId());

	m_preHeldEntityId = 0;
	m_grabbedNPCSpecies = pHeldActor->GetActorSpecies();
	m_grabType = GRAB_TYPE_NPC;
	m_killTimeOut = KILL_NPC_TIMEOUT;
	m_killNPC = m_effectRunning = m_npcWasDead = false;
	m_grabbedNPCInitialHealth = pHeldActor->GetHealth();

	if (CPlayer* pHeldPlayer = CPlayer::FromActor(pHeldActor))
	{
		pHeldPlayer->NotifyObjectGrabbed(true, m_heldEntityId, true);
	}

	//Hide attachments on the back
	if (CWeaponAttachmentManager* pWAM = pHeldActor->GetWeaponAttachmentManager())
	{
		pWAM->HideAllAttachments(true);
	}

	if (m_grabbedNPCSpecies == eGCT_HUMAN)
	{
		PlaySound(eOHSound_Choking_Human, true);
	}

	RequireUpdate(eIUS_General);
	GetGameObject()->EnablePostUpdates(this); //needed, if I pause the game before throwing the NPC

	return true;
}

//=============================================================================================
void COffHand::ThrowNPC(const EntityId entityId, bool kill /*= true*/)
{
	//Get actor
	CActor* pHeldActor = static_cast<CActor*>(m_pActorSystem->GetActor(entityId));
	if (!pHeldActor)
		return;

	IEntity* pEntity = pHeldActor->GetEntity();

	SActorStats* pStats = pHeldActor->GetActorStats();
	if (pStats)
	{
		pStats->isGrabbed = false;
	}

	//Un-Hide attachments on the back
	if (CWeaponAttachmentManager* pWAM = pHeldActor->GetWeaponAttachmentManager())
	{
		pWAM->HideAllAttachments(false);
	}

	CPlayer* pPlayer = static_cast<CPlayer*>(GetOwnerActor());

	if (kill)
	{
		UpdateGrabbedNPCWorldPos(pEntity, NULL);
		pHeldActor->HolsterItem(false);
		IItem* currentItem = pHeldActor->GetCurrentItem();

		{
			int prevHealth = pHeldActor->GetHealth();
			int health = prevHealth - 100;

			//In strenght mode, always kill
			if (pPlayer && pPlayer->GetNanoSuit() && pPlayer->GetNanoSuit()->GetMode() == NANOMODE_STRENGTH)
			{
				health = 0;
			}
			if (health <= 0 || (m_grabbedNPCSpecies != eGCT_HUMAN) || m_bCutscenePlaying)
			{
				pHeldActor->SetHealth(0);
				if (currentItem && m_grabbedNPCSpecies == eGCT_HUMAN)
					pHeldActor->DropItem(currentItem->GetEntityId(), 0.5f, false, true);

				//Don't kill if it was already dead
				if ((pStats && !pStats->isRagDoll) || prevHealth > 0 || m_grabbedNPCSpecies == eGCT_HUMAN)
				{
					pHeldActor->SetAnimationInput("Action", "idle");
					pHeldActor->CreateScriptEvent("kill", 0);
				}
			}
			else
			{
				if (pEntity->GetAI())
				{
					pHeldActor->SetHealth(health);
					pHeldActor->SetAnimationInput("Action", "idle");
					//pEntity->GetAI()->Event(AIEVENT_ENABLE,0);
					pHeldActor->Fall();
					PlaySound(eOHSound_Kill_Human, true);
				}
			}
		}
	}
	else
	{
		pHeldActor->SetAnimationInput("Action", "idle");
	}

	/*if(m_grabbedNPCSpecies==eGCT_TROOPER)
	{
		PlaySound(eOHSound_Choking_Trooper,false);
	}
	else*/
	if (m_grabbedNPCSpecies == eGCT_HUMAN)
	{
		PlaySound(eOHSound_Choking_Human, false);
	}

	m_killTimeOut = -1.0f;
	m_killNPC = m_effectRunning = m_npcWasDead = false;
	m_grabbedNPCInitialHealth = 0;

	if (CPlayer* pHeldPlayer = CPlayer::FromActor(pHeldActor))
	{
		pHeldPlayer->NotifyObjectGrabbed(false, pEntity->GetId(), true);
	}

	GetGameObject()->DisablePostUpdates(this); //Disable again
}


//==============================================================================
void COffHand::RunEffectOnGrabbedNPC(CActor* pNPC)
{
	//Under certain conditions, different things could happen to the grabbed NPC (die, auto-destruct...)
	if (m_grabbedNPCSpecies == eGCT_TROOPER)
	{
		if (m_killNPC && m_effectRunning)
		{
			pNPC->SetHealth(0);
			//PlaySound(eOHSound_Choking_Trooper,false);
			m_killNPC = false;
		}
		else if (m_killNPC || (pNPC->GetHealth() < m_grabbedNPCInitialHealth && !m_effectRunning))
		{
			SGameObjectEvent event(eCGE_InitiateAutoDestruction, eGOEF_ToScriptSystem);
			pNPC->GetGameObject()->SendEvent(event);
			m_effectRunning = true;
			m_killTimeOut = KILL_NPC_TIMEOUT - 0.75f;
			m_killNPC = false;
		}
	}
	else if (m_grabbedNPCSpecies == eGCT_HUMAN && (pNPC->GetHealth() < m_grabbedNPCInitialHealth))
	{
		//Release the guy at first hit/damage... 8/
		if (m_currentState & (eOHS_HOLDING_NPC | eOHS_THROWING_NPC))
		{
			pNPC->SetHealth(0);
		}
	}
}

//========================================================================================
void COffHand::PlaySound(EOffHandSounds sound, bool play)
{
	if (!gEnv->pSoundSystem)
		return;

	bool repeating = false;
	unsigned int idx = 0;
	const char* soundName = NULL;

	switch (sound)
	{
	case eOHSound_Choking_Trooper:
		soundName = "Sounds/alien:trooper:choke";
		repeating = true;
		break;

	case eOHSound_Choking_Human:
		idx = Random(MAX_CHOKE_SOUNDS);
		idx = CLAMP(idx, 0, MAX_CHOKE_SOUNDS - 1);
		if (idx >= 0 && idx < MAX_CHOKE_SOUNDS)
			soundName = gChokeSoundsTable[idx];
		//repeating = true;
		break;

	case eOHSound_Kill_Human:
		idx = Random(MAX_CHOKE_SOUNDS);
		idx = CLAMP(idx, 0, MAX_CHOKE_SOUNDS - 1);
		if (idx >= 0 && idx < MAX_CHOKE_SOUNDS)
			soundName = gDeathSoundsTable[idx];
		break;

	default:
		break;
	}

	if (!soundName)
		return;

	if (play)
	{
		ISound* pSound = NULL;
		if (repeating && m_sounds[sound])
		{
			pSound = gEnv->pSoundSystem->GetSound(m_sounds[sound]);
			if (pSound && pSound->IsPlaying())
			{
				return;
			}
		}

		if (!pSound)
		{
			pSound = gEnv->pSoundSystem->CreateSound(soundName, 0);
		}

		if (pSound)
		{
			pSound->SetSemantic(eSoundSemantic_Player_Foley);
			if (repeating)
			{
				m_sounds[sound] = pSound->GetId();
			}
			pSound->Play();
		}
	}
	else if (repeating && m_sounds[sound])
	{
		ISound* pSound = gEnv->pSoundSystem->GetSound(m_sounds[sound]);
		if (pSound)
		{
			pSound->Stop();
		}
		m_sounds[sound] = 0;
	}


}
//==========================================================================================
void COffHand::MeleeAttack()
{
	if (m_melee)
	{
		if (CPlayer* pOwner = GetOwnerPlayer())
		{
			if (pOwner->GetPlayerStats()->bLookingAtFriendlyAI)
				return;
		}

		CMelee* melee = static_cast<CMelee*>(m_melee);

		SetOffHandState(eOHS_MELEE);

		m_melee->Activate(true);
		if (melee)
		{
			melee->IgnoreEntity(m_heldEntityId);
			//Most 1-handed objects have a mass between 1.0f and 10.0f
			//Scale melee damage/impulse based on mass of the held object
			float massScale = m_heldEntityMass * 0.1f;
			if (massScale < 0.2f)
				massScale *= 0.1f; //Scale down even more for small object...
			melee->MeleeScale(min(massScale, 1.0f));
		}
		m_melee->StartFire();
		m_melee->StopFire();

		GetScheduler()->TimerAction(GetCurrentAnimationTime(eIGS_FirstPerson) + 100, CSchedulerAction<Timer_FinishOffHandAction>::Create(Timer_FinishOffHandAction(eOHA_FINISH_MELEE, this)), false);
	}
}

//=======================================================================================
float COffHand::GetObjectMassScale()
{
	if (m_currentState & eOHS_HOLDING_NPC)
		return 0.65f;
	if (m_currentState & eOHS_HOLDING_OBJECT)
	{
		if (m_grabType == GRAB_TYPE_TWO_HANDED)
			return 0.65f;
		else
		{
			float mass = CLAMP(m_heldEntityMass, 1.0f, 10.0f);
			return (1.0f - (0.025f * mass));
		}
	}

	return 1.0f;
}

//=========================================================================================
bool COffHand::IsHoldingEntity()
{
	bool ret = false;
	if (m_currentState & (eOHS_GRABBING_NPC | eOHS_HOLDING_NPC | eOHS_THROWING_NPC | eOHS_PICKING | eOHS_HOLDING_OBJECT | eOHS_THROWING_OBJECT | eOHS_MELEE))
		ret = true;

	return ret;
}

//=========================================================================================
void COffHand::SetResetTimer(float t)
{
	m_resetTimer = t;
}

//==============================================================
void COffHand::GetAvailableGrenades(std::vector<string>& grenades)
{
	if (!GetFireMode(0)->OutOfAmmo())
	{
		grenades.push_back(GetFireMode(0)->GetName());
	}

	if (!GetFireMode(1)->OutOfAmmo())
	{
		grenades.push_back(GetFireMode(1)->GetName());
	}

	if (!GetFireMode(2)->OutOfAmmo())
	{
		grenades.push_back(GetFireMode(2)->GetName());
	}

	if (!GetFireMode(3)->OutOfAmmo())
	{
		grenades.push_back(GetFireMode(3)->GetName());
	}
}

//==============================================================
int COffHand::CanExchangeWeapons(IItem* pItem, IItem** pExchangeItem)
{
	CPlayer* pPlayer = static_cast<CPlayer*>(GetOwnerActor());

	if (!pPlayer)
		return ITEM_NO_EXCHANGE;

	if (!pPlayer->CheckInventoryRestrictions(pItem->GetEntity()->GetClass()->GetName()))
	{
		//Can not carry more heavy/medium weapons
		IItem* pNewItem = GetExchangeItem(pPlayer);
		if (pNewItem)
		{
			*pExchangeItem = pNewItem;
			//can replace medium or heavy weapon
			return ITEM_CAN_EXCHANGE;
		}
		else
			return ITEM_NO_EXCHANGE;
	}

	return ITEM_CAN_PICKUP;
}

//==========================================================================
IItem* COffHand::GetExchangeItem(CPlayer* pPlayer)
{
	if (!pPlayer)
		return NULL;

	IItem* pItem = pPlayer->GetCurrentItem();
	const char* itemCategory = NULL;
	if (pItem)
	{
		itemCategory = m_pItemSystem->GetItemCategory(pItem->GetEntity()->GetClass()->GetName());
		if (!strcmp(itemCategory, "medium") || !strcmp(itemCategory, "heavy"))
			return pItem;
		else
		{
			int i = pPlayer->GetInventory()->GetCount();
			for (int w = i - 1; w > -1; --w)
			{
				pItem = m_pItemSystem->GetItem(pPlayer->GetInventory()->GetItem(w));
				itemCategory = m_pItemSystem->GetItemCategory(pItem->GetEntity()->GetClass()->GetName());
				if (!strcmp(itemCategory, "medium") || !strcmp(itemCategory, "heavy"))
					return pItem;
			}
		}
	}
	return NULL;
}

//========================================================
EntityId COffHand::SpawnRockProjectile(IRenderNode* pRenderNode)
{
	Matrix34 statObjMtx;
	IStatObj* pStatObj = pRenderNode->GetEntityStatObj(0, 0, &statObjMtx);
	assert(pStatObj);
	if (!pStatObj)
		return 0;

	pRenderNode->SetRndFlags(ERF_HIDDEN, true);
	pRenderNode->Dephysicalize();

	float scale = statObjMtx.GetColumn(0).GetLength();

	IEntityClass* pClass = m_pEntitySystem->GetClassRegistry()->FindClass("rock");
	if (!pClass)
		return 0;
	CProjectile* pRock = g_pGame->GetWeaponSystem()->SpawnAmmo(pClass);
	assert(pRock);
	if (!pRock)
		return 0;
	IEntity* pEntity = pRock->GetEntity();
	assert(pEntity);
	if (!pEntity)
		return 0;

	pEntity->SetStatObj(pStatObj, 0, true);
	pEntity->SetSlotFlags(0, pEntity->GetSlotFlags(0) | ENTITY_SLOT_RENDER);

	IEntityRenderProxy* pRenderProxy = static_cast<IEntityRenderProxy*>(pEntity->GetProxy(ENTITY_PROXY_RENDER));
	if (pRenderProxy)
	{
		pRenderProxy->GetRenderNode()->SetLodRatio(255);
		pRenderProxy->GetRenderNode()->SetViewDistRatio(255);
		pRenderProxy->GetRenderNode()->SetRndFlags(pRenderProxy->GetRenderNode()->GetRndFlags() | ERF_PICKABLE);
	}

	pRock->SetParams(GetOwnerId(), GetEntityId(), GetEntityId(), GetCurrentFireMode(), 75, 0);
	pRock->SetSequence(GenerateShootSeqN());

	pRock->Launch(statObjMtx.GetTranslation(), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), 1.0f);

	SEntityPhysicalizeParams params;
	params.type = PE_RIGID;
	params.nSlot = 0;
	params.mass = 2.0f;

	pe_params_buoyancy buoyancy;
	buoyancy.waterDamping = 1.5;
	buoyancy.waterResistance = 10;
	buoyancy.waterDensity = 0;
	params.pBuoyancy = &buoyancy;

	pEntity->Physicalize(params);

	return pEntity->GetId();
}

//==============================================================
//Handle entering cinematic (called from HUD)

void COffHand::OnBeginCutScene()
{
	if (IsHoldingEntity())
	{
		if (m_currentState & (eOHS_THROWING_NPC | eOHS_THROWING_OBJECT))
		{
			OnAction(GetOwnerId(), ActionId("use"), eAAM_OnRelease, 0.0f);
		}
		else
		{
			OnAction(GetOwnerId(), ActionId("use"), eAAM_OnPress, 0.0f);
			OnAction(GetOwnerId(), ActionId("use"), eAAM_OnRelease, 0.0f);
		}
	}

	m_bCutscenePlaying = true;
}

void COffHand::OnEndCutScene()
{
	m_bCutscenePlaying = false;
}

//==============================================================
void COffHand::AttachGrenadeToHand(int grenade, bool fp /*=true*/, bool attach /*=true*/)
{
	//Attach grenade to hand
	if (fp || (GetOwnerActor() && GetOwnerActor()->IsClient()))
	{
		if (grenade == 0)
			DoSwitchAccessory(ItemString("OffhandGrenade"));
		else if (grenade == 1)
			DoSwitchAccessory(ItemString("OffhandSmoke"));
		else if (grenade == 2)
			DoSwitchAccessory(ItemString("OffhandFlashbang"));
		else if (grenade == 3)
			DoSwitchAccessory(ItemString("OffhandNanoDisruptor"));
	}
	if (!fp)
	{
		ICharacterInstance* pOwnerCharacter = GetOwnerActor() ? GetOwnerActor()->GetEntity()->GetCharacter(0) : NULL;
		if (!pOwnerCharacter)
			return;

		IAttachmentManager* pAttachmentManager = pOwnerCharacter->GetIAttachmentManager();
		IAttachment* pAttachment = pAttachmentManager ? pAttachmentManager->GetInterfaceByName(m_params.attachment[eIH_Left].c_str()) : NULL;

		if (pAttachment)
		{
			//If there's an attachment, clear it
			if (!attach)
			{
				pAttachment->ClearBinding();
			}
			else
			{
				//If not it means we need to attach
				int slot = eIGS_Aux0;
				if (grenade == 1)
					slot = eIGS_Aux1;
				else if (grenade == 2)
					slot = eIGS_OwnerLooped;
				else if (grenade == 3)
					slot = eIGS_Owner;

				if (IStatObj* pStatObj = GetEntity()->GetStatObj(slot))
				{
					CCGFAttachment* pCGFAttachment = new CCGFAttachment();
					pCGFAttachment->pObj = pStatObj;

					pAttachment->AddBinding(pCGFAttachment);
				}
			}

		}
	}
}

//==============================================================
EntityId	COffHand::GetHeldEntityId() const
{
	if (m_currentState & (eOHS_HOLDING_NPC | eOHS_HOLDING_OBJECT))
		return m_heldEntityId;

	return 0;
}

//==============================================================
bool COffHand::Request_PickUpObject_MP()
{
	const EntityId objectId = m_crosshairId;
	IEntity* pObject = m_pEntitySystem->GetEntity(objectId);
	if (!pObject)
		return false;

	//CryMP notify server
	CActor* pOwner = GetOwnerActor();
	if (pOwner && pOwner->IsClient())
	{
		//Don't pick up in prone
		if (pOwner->GetStance() == STANCE_PRONE)
		{
			return false;
		}

		//Check if someone else is carrying the object already
		const int count = m_pActorSystem->GetActorCount();
		if (count > 1)
		{
			IActorIteratorPtr pIter = m_pActorSystem->CreateActorIterator();
			while (IActor* pActor = pIter->Next())
			{
				if (pActor == pOwner)
					continue;

				CPlayer* pOwnerPlayer = CPlayer::FromIActor(pActor);

				if (pOwnerPlayer && pOwnerPlayer->GetHeldObjectId() == objectId)
				{
					COffHand* pOffHand = static_cast<COffHand*>(pOwnerPlayer->GetItemByClass(CItem::sOffHandClass));
					if (pOffHand)
					{
						//stolen object event
						pOffHand->ThrowObject_MP(pOwnerPlayer, objectId, true);
					}
					break;
				}
			}
		}

		const bool bClientEntity = (pObject->GetFlags() & ENTITY_FLAG_CLIENT_ONLY);
		if (!bClientEntity)
		{
			//Start throw animation for all clients
			/*if (m_fm)
			{
				m_fm->StartFire();
			}
			else
			{
				CryLogWarningAlways("COffHand: owner %s has no firemode", pOwner->GetEntity()->GetName());
			}*/

			RequestFireMode(GetFireModeIdx(m_grabTypes[m_grabType].throwFM.c_str())); //CryMP: added this here, early enough 

			//CryMP: Might need a small timer to guarantee firemode is accepted before pickupitem rmi
			pOwner->GetGameObject()->InvokeRMI(CPlayer::SvRequestPickUpItem(), CPlayer::ItemIdParam(objectId), eRMI_ToServer);
			return true;
		}
	}
	return false;
}

//==============================================================
bool COffHand::PickUpObject_MP(CPlayer* pPlayer, const EntityId synchedObjectId) //Called from CPlayer.cpp
{
	IEntity* pObject = m_pEntitySystem->GetEntity(synchedObjectId);
	if (!pObject)
		return false;

	if (pPlayer->IsRemote() && m_resetTimer > 0.0f)
	{
		m_resetTimer = -1.0f;
		CryLogAlways("$8COffHand::PickUpObject_MP: Reset timer cleared for player %s", pPlayer->GetEntity()->GetName());
	}

	//CryMP: Remote player stole our object
	CPlayer* pClientPlayer = static_cast<CPlayer*>(m_pGameFramework->GetClientActor());
	if (pClientPlayer && pClientPlayer != pPlayer && pClientPlayer->GetHeldObjectId() == synchedObjectId)
	{
		COffHand* pClientOffHand = static_cast<COffHand*>(pClientPlayer->GetWeaponByClass(CItem::sOffHandClass));
		if (pClientOffHand)
		{
			//Will take care of resets 
			pClientOffHand->ThrowObject_MP(pClientPlayer, synchedObjectId, true);
		}
	}

	StartPickUpObject(synchedObjectId, false); 

	if (gEnv->bClient && pPlayer->IsRemote())
	{
		EnableUpdate(true, eIUS_General);
	}

	return true;
}

//==============================================================
bool COffHand::ThrowObject_MP(CPlayer* pPlayer, const EntityId synchedObjectId, bool stealingObject) //Called from CPlayer.cpp
{
	if (!pPlayer)
		return false;

	IEntity* pObject = m_pEntitySystem->GetEntity(synchedObjectId);
	if (!pObject)
		return false;

	const EntityId playerId = pPlayer->GetEntityId();
	//bool isTwoHand = IsTwoHandMode();

	//pPlayer->ClearIKPosBlending("leftArm");

	//if (isTwoHand)
	//{
	//	pPlayer->ClearIKPosBlending("rightArm");
	//}

	if (stealingObject)
	{
		FinishAction(EOffHandActions::eOHA_RESET);
	}
	else
	{
		if (pPlayer->IsRemote())
		{
			FinishAction(EOffHandActions::eOHA_THROW_OBJECT);
		}
	}

	return true;
}

//==============================================================
void COffHand::AwakeEntityPhysics(IEntity* pEntity)
{
	if (!pEntity)
		return;

	IPhysicalEntity* pPhysics = pEntity->GetPhysics();
	if (pPhysics)
	{
		pe_action_awake actionAwake;
		actionAwake.bAwake = 1;
		pPhysics->Action(&actionAwake);
	}
}

//// Primary = RayTraceEntity (thin ray), Fallback = CollideEntityWithBeam (fat ray)
//// Returns 2 local-space points on the entity surface good for left/right hands.
//struct STwoHandLocalGrips
//{
//	Vec3 leftLocal = ZERO;
//	Vec3 rightLocal = ZERO;
//	bool ok = false;
//};
//
//// Compute two local-space grip points on the held entity (CryEngine 2).
//// Primary probe = RayTraceEntity (thin ray from sides toward center).
//// Fallback      = CollideEntityWithBeam (fat ray) from sides, then from underneath upward.
//static STwoHandLocalGrips ComputeTwoHandGripLocal(IEntity* pObj)
//{
//	STwoHandLocalGrips out{};
//	if (!pObj || !pObj->GetPhysics())
//		return out;
//
//	// --- 1) Object-space sampling layout ---
//	AABB lb; pObj->GetLocalBounds(lb);
//	const Vec3 lc = (lb.min + lb.max) * 0.5f;
//	const float halfY = 0.5f * fabsf(lb.max.y - lb.min.y); // width (left/right) on local Y
//	const float halfZ = 0.5f * fabsf(lb.max.z - lb.min.z); // height on local Z
//
//	const float side = std::clamp(halfY * 0.8f, 0.15f, 0.45f);          // lateral offset from center
//	const float height = std::clamp(halfZ * 0.10f + 0.05f, 0.05f, 0.20f); // a bit above bottom
//	const float fwdBias = 0.05f;                                           // slight +X bias
//	const float rayLen = std::max(0.25f, halfY * 2.0f);
//
//	// Start points (local) and directions (local)
//	Vec3 Lloc = Vec3(lc.x + fwdBias, lc.y + side, lc.z + height);
//	Vec3 Rloc = Vec3(lc.x + fwdBias, lc.y - side, lc.z + height);
//	Vec3 LdirL = Vec3(0, -1, 0) * rayLen; // toward Y=0 from +Y
//	Vec3 RdirL = Vec3(0, 1, 0) * rayLen; // toward Y=0 from -Y
//
//	// --- 2) Transform to world and cast only against THIS entity ---
//	Matrix34 objW = pObj->GetWorldTM();
//	auto L2W = [&](const Vec3& p) { return objW.TransformPoint(p); };
//	auto V2W = [&](const Vec3& v) { return objW.TransformVector(v); };
//
//	IPhysicalEntity* pe = pObj->GetPhysics();
//	ray_hit hL{}, hR{};
//	bool leftHit = gEnv->pPhysicalWorld->RayTraceEntity(pe, L2W(Lloc), V2W(LdirL), &hL, nullptr) > 0;
//	bool rightHit = gEnv->pPhysicalWorld->RayTraceEntity(pe, L2W(Rloc), V2W(RdirL), &hR, nullptr) > 0;
//
//	// --- 3) Fallback: “fat ray” (beam) to avoid slipping through small gaps ---
//	if (!leftHit)
//		leftHit = gEnv->pPhysicalWorld->CollideEntityWithBeam(pe, L2W(Lloc), V2W(LdirL), 0.03f, &hL) > 0; // ~3 cm radius
//	if (!rightHit)
//		rightHit = gEnv->pPhysicalWorld->CollideEntityWithBeam(pe, L2W(Rloc), V2W(RdirL), 0.03f, &hR) > 0;
//
//	// --- 4) Second fallback: probe from underneath upward (good for lips/undersides) ---
//	if (!(leftHit && rightHit))
//	{
//		const float upLen = std::max(0.20f, halfZ * 1.6f);
//		Vec3 Lunder = Vec3(lc.x, lc.y + side, lb.min.z - 0.04f);
//		Vec3 Runder = Vec3(lc.x, lc.y - side, lb.min.z - 0.04f);
//		Vec3 UpL = Vec3(0, 0, 1) * upLen;
//
//		if (!leftHit)
//			leftHit = gEnv->pPhysicalWorld->CollideEntityWithBeam(pe, L2W(Lunder), V2W(UpL), 0.03f, &hL) > 0;
//		if (!rightHit)
//			rightHit = gEnv->pPhysicalWorld->CollideEntityWithBeam(pe, L2W(Runder), V2W(UpL), 0.03f, &hR) > 0;
//	}
//
//	if (!(leftHit && rightHit))
//		return out;
//
//	// --- 5) Convert hits back to LOCAL, nudge slightly into surface to avoid float ---
//	Matrix34 w2o = objW.GetInverted();
//	Vec3 lL = w2o.TransformPoint(hL.pt);
//	Vec3 lR = w2o.TransformPoint(hR.pt);
//	Vec3 nL = w2o.TransformVector(hL.n).GetNormalizedSafe();
//	Vec3 nR = w2o.TransformVector(hR.n).GetNormalizedSafe();
//	const float eps = 0.01f; // 1 cm push-inwards
//	lL -= nL * eps;
//	lR -= nR * eps;
//
//	// --- 6) Ergonomic clamp on horizontal separation (local Y) ---
//	float sep = fabsf(lL.y - lR.y);
//	const float minSep = 0.25f, maxSep = 0.8f;
//	if (sep < minSep || sep > maxSep)
//	{
//		const float midY = 0.5f * (lL.y + lR.y);
//		const float half = std::clamp(sep * 0.5f, minSep * 0.5f, maxSep * 0.5f);
//		lL.y = midY + half;
//		lR.y = midY - half;
//	}
//
//	out.leftLocal = lL;
//	out.rightLocal = lR;
//	out.ok = true;
//	return out;
//}
//
//// Vehicles or anything mounted above the player:
//// probe from the top-front half downward to find natural hand holds.
//// Returns LOCAL-SPACE points on the entity (no rotations needed).
//static STwoHandLocalGrips ComputeVehicleGripLocal(IEntity* pObj)
//{
//	STwoHandLocalGrips out{};
//	if (!pObj || !pObj->GetPhysics())
//		return out;
//
//	// --- Object-space layout ---
//	AABB lb; pObj->GetLocalBounds(lb);
//	const Vec3 lc = (lb.min + lb.max) * 0.5f;
//	const float halfY = 0.5f * fabsf(lb.max.y - lb.min.y);
//	const float halfZ = 0.5f * fabsf(lb.max.z - lb.min.z);
//
//	// Bias to the FRONT HALF (+X by convention; swap if your forward differs)
//	const float frontBiasX = std::clamp(0.10f, 0.08f, 0.18f);   // ~10cm forward; clamped for safety
//	const float side = std::clamp(halfY * 0.75f, 0.18f, 0.50f);
//	const float downLen = std::max(0.30f, halfZ * 2.0f);
//
//	// Start slightly above the top, cast downward
//	Vec3 LstartL = Vec3(lc.x + frontBiasX, lc.y + side, lb.max.z + 0.06f);
//	Vec3 RstartL = Vec3(lc.x + frontBiasX, lc.y - side, lb.max.z + 0.06f);
//	Vec3 downL = Vec3(0, 0, -1) * downLen;
//
//	// --- Cast only against THIS entity ---
//	Matrix34 objW = pObj->GetWorldTM();
//	auto L2W = [&](const Vec3& p) { return objW.TransformPoint(p); };
//	auto V2W = [&](const Vec3& v) { return objW.TransformVector(v); };
//
//	IPhysicalEntity* pe = pObj->GetPhysics();
//	ray_hit hL{}, hR{};
//	bool leftHit = gEnv->pPhysicalWorld->RayTraceEntity(pe, L2W(LstartL), V2W(downL), &hL, nullptr) > 0;
//	bool rightHit = gEnv->pPhysicalWorld->RayTraceEntity(pe, L2W(RstartL), V2W(downL), &hR, nullptr) > 0;
//
//	// Fat fallback to handle perforated/top meshes
//	if (!leftHit)  leftHit = gEnv->pPhysicalWorld->CollideEntityWithBeam(pe, L2W(LstartL), V2W(downL), 0.03f, &hL) > 0;
//	if (!rightHit) rightHit = gEnv->pPhysicalWorld->CollideEntityWithBeam(pe, L2W(RstartL), V2W(downL), 0.03f, &hR) > 0;
//
//	if (!(leftHit && rightHit))
//		return out;
//
//	// --- Back to local; slight push-in; keep on the front half ---
//	Matrix34 w2o = objW.GetInverted();
//	Vec3 lL = w2o.TransformPoint(hL.pt);
//	Vec3 lR = w2o.TransformPoint(hR.pt);
//	Vec3 nL = w2o.TransformVector(hL.n).GetNormalizedSafe();
//	Vec3 nR = w2o.TransformVector(hR.n).GetNormalizedSafe();
//
//	const float eps = 0.01f; // 1 cm
//	lL -= nL * eps; lR -= nR * eps;
//
//	// Force front-half grips (avoid behind-player selections on large roofs)
//	lL.x = std::max(lL.x, lc.x + 0.01f);
//	lR.x = std::max(lR.x, lc.x + 0.01f);
//
//	// Ergonomic lateral separation
//	float sep = fabsf(lL.y - lR.y);
//	const float minSep = 0.28f, maxSep = 0.90f;
//	if (sep < minSep || sep > maxSep)
//	{
//		const float midY = 0.5f * (lL.y + lR.y);
//		const float half = std::clamp(sep * 0.5f, minSep * 0.5f, maxSep * 0.5f);
//		lL.y = midY + half;
//		lR.y = midY - half;
//	}
//
//	out.leftLocal = lL;
//	out.rightLocal = lR;
//	out.ok = true;
//	return out;
//}
//
//// Clamp a world-space target to a cone in front of the player, at least minFront meters away.
//// minCos = cos(halfAngle). Example: halfAngle≈70° -> minCos≈0.34.
//// If requireAbove=true, force Z at least slightly above player origin.
//static Vec3 ClampToFrontHemisphere(const Vec3& worldPoint, const Matrix34& playerW,
//	float minFront, float minCos, bool requireAbove)
//{
//	const Vec3 origin = playerW.GetTranslation();
//	const Vec3 fwd = playerW.GetColumn1().GetNormalizedSafe(); // Cry forward = +Y
//	const Vec3 up = playerW.GetColumn2().GetNormalizedSafe(); // +Z
//
//	Vec3 v = worldPoint - origin;
//	float d = v.GetLength();
//	if (d < 1e-4f)
//		return origin + fwd * minFront;
//
//	Vec3 vn = v / d;
//	float dotF = vn.Dot(fwd);
//
//	if (dotF < minCos)
//	{
//		Vec3 lateral = (vn - fwd * dotF).GetNormalizedSafe();
//		float s = sqrtf(std::max(0.f, 1.f - minCos * minCos));
//		vn = (fwd * minCos + lateral * s).GetNormalizedSafe();
//		d = std::max(d, minFront);
//	}
//
//	Vec3 clamped = origin + vn * std::max(d, minFront);
//
//	if (requireAbove && clamped.z < origin.z + 0.15f)
//		clamped.z = origin.z + 0.15f;
//
//	return clamped;
//}

//struct SGripHitLocal
//{
//	bool ok = false;
//	Vec3 leftLocal = ZERO;
//	Vec3 rightLocal = ZERO;
//};
//
//static SGripHitLocal ComputeGripHitsLocal_PhysOnly(
//	CActor* pOwner, IEntity* pObject, float extraMargin /*e.g. 0.25f*/, float ttlSeconds /*debug*/)
//{
//	SGripHitLocal out;
//	if (!pOwner || !pObject) return out;
//
//	IPhysicalEntity* pPE = pObject->GetPhysics();
//	if (!pPE) { CryLogAlways("[GripFromRays] ERROR: object has no physics"); return out; }
//
//	// Use render AABB for center/radius (fast + stable), then place anchors outside it
//	AABB wbox; pObject->GetWorldBounds(wbox);
//	Vec3 center = (wbox.min + wbox.max) * 0.5f;
//
//	pe_status_dynamics sd;
//	if (pPE->GetStatus(&sd))
//	{
//		center = sd.centerOfMass;
//	}
//	
//	const Vec3 ext = (wbox.max - wbox.min) * 0.5f;
//	const float radius = max(ext.len(), 0.05f);
//	const float d = radius + extraMargin;
//
//	// Player-right defines left/right
//	const Matrix34 playerW = pOwner->GetEntity()->GetWorldTM();
//	Vec3 playerRight = playerW.GetColumn0(); playerRight.NormalizeSafe(Vec3(1, 0, 0));
//
//	const Vec3 leftWS = center - playerRight * d; // yellow
//	const Vec3 rightWS = center + playerRight * d; // blue
//
//	if (g_pGameCVars->mp_pickupDebug)
//	{
//		if (IPersistantDebug* pPD = g_pGame->GetIGameFramework()->GetIPersistantDebug())
//		{
//			pPD->Begin("OffHand_GripAnchors", false);
//			pPD->AddSphere(leftWS, 0.06f, ColorF(1, 1, 0, 1), ttlSeconds);
//			pPD->AddSphere(rightWS, 0.06f, ColorF(0.2f, 0.5f, 1, 1), ttlSeconds);
//			pPD->AddLine(leftWS, rightWS, ColorF(1, 1, 1, 0.75f), ttlSeconds);
//		}
//	}
//
//	auto rayOne = [&](const Vec3& from, const Vec3& to, ray_hit& hit)->bool
//		{
//			const Vec3 rayVec = to - from;
//			if (rayVec.GetLengthSquared() < 1e-6f)
//				return false;
//
//			// CE2 signature: (pient, origin, dirVec, pHit, pe_params_pos*)
//			const int result = gEnv->pPhysicalWorld->RayTraceEntity(pPE, from, rayVec, &hit, nullptr);
//			return (result > 0);
//		};
//
//	ray_hit hitLR = {}, hitRL = {};
//	const bool okLR = rayOne(leftWS, rightWS, hitLR);
//	const bool okRL = rayOne(rightWS, leftWS, hitRL);
//
//	if (!(okLR && okRL))
//	{
//		CryLogAlways("[GripFromRays] Miss (L->R=%d, R->L=%d). Check margins or physics proxy.", okLR ? 1 : 0, okRL ? 1 : 0);
//		return out;
//	}
//
//	// Nudge slightly inward to avoid IK hovering
//	const float inset = 0.01f;
//	const Vec3 leftInsetWS = hitLR.pt + hitLR.n * (-inset);
//	const Vec3 rightInsetWS = hitRL.pt + hitRL.n * (-inset);
//
//	const Matrix34 objW = pObject->GetWorldTM();
//	const Matrix34 objInv = objW.GetInvertedFast();
//	out.leftLocal = objInv.TransformPoint(leftInsetWS);
//	out.rightLocal = objInv.TransformPoint(rightInsetWS);
//	out.ok = true;
//
//	CryLogAlways("[GripFromRays] OK  L_local=(%.3f, %.3f, %.3f)  R_local=(%.3f, %.3f, %.3f)",
//		out.leftLocal.x, out.leftLocal.y, out.leftLocal.z,
//		out.rightLocal.x, out.rightLocal.y, out.rightLocal.z);
//
//	return out;
//}


// ------------------------------
// TWO-HAND: anchors + bottom-edge fallback
// ------------------------------
COffHand::SGripHitLocal COffHand::ComputeGripHitsLocal(
	CActor* pOwner, IEntity* pObject, bool isTwoHand)
{
	SGripHitLocal out;
	if (!pOwner || !pObject) return out;

	const float extraMargin = 0.25f;
	const float ttlSeconds = 2.0f;

	IPhysicalEntity* pPE = pObject->GetPhysics();
	if (!pPE) { CryLogAlways("[GripFromRays] ERROR: object has no physics"); return out; }

	AABB wbox; pObject->GetWorldBounds(wbox);
	Vec3 center = (wbox.min + wbox.max) * 0.5f;

	pe_status_dynamics sd;
	if (pPE->GetStatus(&sd))
		center = sd.centerOfMass;

	const Vec3 ext = (wbox.max - wbox.min) * 0.5f;
	const float radius = max(ext.len(), 0.05f);
	const float d = radius + extraMargin;

	const Matrix34 playerW = pOwner->GetEntity()->GetWorldTM();
	Vec3 playerRight = playerW.GetColumn0(); playerRight.NormalizeSafe(Vec3(1, 0, 0));

	const Vec3 leftWS = center - playerRight * d;
	const Vec3 rightWS = center + playerRight * d;

	if (g_pGameCVars->mp_pickupDebug)
	{
		if (IPersistantDebug* pPD = g_pGame->GetIGameFramework()->GetIPersistantDebug())
		{
			pPD->Begin("OffHand_GripAnchors", false);
			pPD->AddSphere(leftWS, 0.06f, ColorF(1, 1, 0, 1), ttlSeconds);
			pPD->AddSphere(rightWS, 0.06f, ColorF(0.2f, 0.5f, 1, 1), ttlSeconds);
			pPD->AddLine(leftWS, rightWS, ColorF(1, 1, 1, 0.75f), ttlSeconds);
		}
	}

	auto rayOne = [&](const Vec3& from, const Vec3& to, ray_hit& hit)->bool
		{
			const Vec3 rayVec = to - from;
			if (rayVec.GetLengthSquared() < 1e-6f)
				return false;

			const int result = gEnv->pPhysicalWorld->RayTraceEntity(pPE, from, rayVec, &hit, nullptr);
			return (result > 0);
		};

	ray_hit hitLR = {}, hitRL = {};
	const bool okLR = rayOne(leftWS, rightWS, hitLR);
	const bool okRL = isTwoHand ? rayOne(rightWS, leftWS, hitRL) : false;

	if (isTwoHand)
	{
		if (!(okLR && okRL))
		{
			if (g_pGameCVars->mp_pickupDebug)
				CryLogAlways("[GripFromRays] Miss (L->R=%d, R->L=%d). Check margins or physics proxy.",
					okLR ? 1 : 0, okRL ? 1 : 0);
			return out;
		}
	}
	else
	{
		if (!okLR)
		{
			if (g_pGameCVars->mp_pickupDebug)
				CryLogAlways("[GripFromRays] Miss (single-hand: L->R=0). Check margins or physics proxy.");
			return out;
		}
	}

	const float inset = 0.01f;
	const Vec3 leftInsetWS = okLR ? (hitLR.pt + hitLR.n * (-inset)) : leftWS;
	const Vec3 rightInsetWS = (isTwoHand && okRL) ? (hitRL.pt + hitRL.n * (-inset)) : rightWS;

	const Matrix34 objW = pObject->GetWorldTM();
	const Matrix34 objInv = objW.GetInvertedFast();

	out.leftLocal = objInv.TransformPoint(leftInsetWS);
	if (isTwoHand)
		out.rightLocal = objInv.TransformPoint(rightInsetWS);

	out.ok = true;

	if (g_pGameCVars->mp_pickupDebug)
	{
		if (isTwoHand)
		{
			CryLogAlways("[GripFromRays] OK  L_local=(%.3f, %.3f, %.3f)  R_local=(%.3f, %.3f, %.3f)",
				out.leftLocal.x, out.leftLocal.y, out.leftLocal.z,
				out.rightLocal.x, out.rightLocal.y, out.rightLocal.z);
		}
		else
		{
			CryLogAlways("[GripFromRays] OK (single-hand)  L_local=(%.3f, %.3f, %.3f)",
				out.leftLocal.x, out.leftLocal.y, out.leftLocal.z);
		}
	}

	return out;
}

//COffHand::SGripHitLocal COffHand::ComputeGripHitsLocal_PhysOnly(
//	CActor* pOwner, IEntity* pObject,
//	float extraMargin /*e.g. 0.25f*/,
//	float ttlSeconds  /*debug*/
//)
//{
//	SGripHitLocal out;
//	if (!pOwner || !pObject)
//		return out;
//
//	IPhysicalEntity* pPE = pObject->GetPhysics();
//	if (!pPE)
//	{
//		CryLogAlways("[GripFromRays] ERROR: object has no physics");
//		return out;
//	}
//
//	// --- Constants / Tunables ---
//	const float maxLocalSpan = 0.45f;   // Maximum allowed grip width in local space
//	const float handHalfSpan = 0.25f;   // Half distance between left/right hands for wide objects
//	const float inset = 0.01f;          // Nudge inward to avoid hovering
//	const float underLift = 0.02f;      // Small lift for bottom contact
//	const float debugLift = 0.06f;      // For debug spheres
//
//	// --- Get bounding and center ---
//	AABB wbox;
//	pObject->GetWorldBounds(wbox);
//	Vec3 center = (wbox.min + wbox.max) * 0.5f;
//
//	pe_status_dynamics sd;
//	if (pPE->GetStatus(&sd))
//		center = sd.centerOfMass;
//
//	const Vec3 ext = (wbox.max - wbox.min) * 0.5f;
//	const float radius = max(ext.len(), 0.05f);
//	const float d = radius + extraMargin;
//
//	const Matrix34 playerW = pOwner->GetEntity()->GetWorldTM();
//	const Vec3 playerPos = playerW.GetTranslation();
//	Vec3 playerRight = playerW.GetColumn0();
//	playerRight.NormalizeSafe(Vec3(1, 0, 0));
//	const Vec3 worldUp(0, 0, 1);
//
//	const Vec3 leftWS = center - playerRight * d;
//	const Vec3 rightWS = center + playerRight * d;
//
//	if (g_pGameCVars->mp_pickupDebug)
//	{
//		if (IPersistantDebug* pPD = g_pGame->GetIGameFramework()->GetIPersistantDebug())
//		{
//			pPD->Begin("OffHand_GripAnchors", false);
//			pPD->AddSphere(leftWS, debugLift, ColorF(1, 1, 0, 1), ttlSeconds);
//			pPD->AddSphere(rightWS, debugLift, ColorF(0.2f, 0.5f, 1, 1), ttlSeconds);
//			pPD->AddLine(leftWS, rightWS, ColorF(1, 1, 1, 0.75f), ttlSeconds);
//		}
//	}
//
//	auto RayOne = [&](const Vec3& from, const Vec3& to, ray_hit& hit) -> bool
//		{
//			const Vec3 rayVec = to - from;
//			if (rayVec.GetLengthSquared() < 1e-6f)
//				return false;
//			const int result = gEnv->pPhysicalWorld->RayTraceEntity(pPE, from, rayVec, &hit, nullptr);
//			return (result > 0);
//		};
//
//	auto InsetPoint = [&](const ray_hit& h) -> Vec3
//		{
//			return h.pt + h.n * (-inset);
//		};
//
//	auto ToLocal = [&](const Vec3& leftWS, const Vec3& rightWS, SGripHitLocal& outLocal)
//		{
//			const Matrix34 objInv = pObject->GetWorldTM().GetInvertedFast();
//			outLocal.leftLocal = objInv.TransformPoint(leftWS);
//			outLocal.rightLocal = objInv.TransformPoint(rightWS);
//			outLocal.ok = true;
//		};
//
//	// ---------- PASS 1: side rays ----------
//	ray_hit hitLR = {}, hitRL = {};
//	const bool okLR = RayOne(leftWS, rightWS, hitLR);
//	const bool okRL = RayOne(rightWS, leftWS, hitRL);
//
//	if (!(okLR && okRL))
//	{
//		if (g_pGameCVars->mp_pickupDebug)
//			CryLogAlways("[GripFromRays] Miss (L->R=%d, R->L=%d). Check margins or physics proxy.", okLR ? 1 : 0, okRL ? 1 : 0);
//		return out;
//	}
//
//	const Vec3 leftInsetWS_pass1 = InsetPoint(hitLR);
//	const Vec3 rightInsetWS_pass1 = InsetPoint(hitRL);
//
//	SGripHitLocal pass1;
//	ToLocal(leftInsetWS_pass1, rightInsetWS_pass1, pass1);
//
//	const float spanPass1 = (pass1.leftLocal - pass1.rightLocal).GetLength();
//
//	if (g_pGameCVars->mp_pickupDebug)
//		CryLogAlways("[GripFromRays] Pass1 local span = %.3f (max=%.3f)", spanPass1, maxLocalSpan);
//
//	if (spanPass1 <= maxLocalSpan)
//		return pass1;
//
//	// ---------- PASS 2: wide-object fallback ----------
//	const float below = radius + extraMargin + 0.12f;
//	const Vec3 startBelow = center - worldUp * below;
//
//	ray_hit hitBottom = {};
//	const bool okBottom = RayOne(startBelow, center, hitBottom);
//
//	ray_hit hitFront = {};
//	const bool okFront = RayOne(playerPos, center, hitFront);
//
//	if (okBottom && okFront)
//	{
//		Vec3 base = hitFront.pt;
//		base.z = hitBottom.pt.z + underLift;
//
//		Vec3 leftBase = base - playerRight * handHalfSpan;
//		Vec3 rightBase = base + playerRight * handHalfSpan;
//
//		ray_hit hitL = {}, hitR = {};
//		const bool okL = RayOne(leftBase, center, hitL);
//		const bool okR = RayOne(rightBase, center, hitR);
//
//		if (okL && okR)
//		{
//			const Vec3 leftInsetWS_pass2 = InsetPoint(hitL);
//			const Vec3 rightInsetWS_pass2 = InsetPoint(hitR);
//
//			SGripHitLocal pass2;
//			ToLocal(leftInsetWS_pass2, rightInsetWS_pass2, pass2);
//
//			const float spanPass2 = (pass2.leftLocal - pass2.rightLocal).GetLength();
//
//			if (g_pGameCVars->mp_pickupDebug)
//			{
//				CryLogAlways("[GripFromRays] Pass2-wide: span=%.3f (max=%.3f)", spanPass2, maxLocalSpan);
//				if (IPersistantDebug* pPD = g_pGame->GetIGameFramework()->GetIPersistantDebug())
//				{
//					pPD->AddSphere(hitBottom.pt, debugLift, ColorF(0.9f, 0.3f, 0.1f, 1), ttlSeconds);
//					pPD->AddSphere(hitFront.pt, debugLift, ColorF(0.3f, 0.9f, 0.1f, 1), ttlSeconds);
//					pPD->AddSphere(base, debugLift, ColorF(1, 1, 1, 0.9f), ttlSeconds);
//					pPD->AddSphere(leftBase, debugLift, ColorF(1, 0.6f, 0.1f, 1), ttlSeconds);
//					pPD->AddSphere(rightBase, debugLift, ColorF(0.1f, 0.6f, 1, 1), ttlSeconds);
//				}
//			}
//
//			if (spanPass2 <= maxLocalSpan)
//				return pass2;
//		}
//	}
//
//	// Fallback
//	if (g_pGameCVars->mp_pickupDebug)
//		CryLogAlways("[GripFromRays] Using pass1 (wide fallback unsuitable)");
//	return pass1;
//}

bool COffHand::GetPredefinedGripHandPos(IEntity* pEnt, Vec3& outLeftEL, Vec3& outRightEL)
{
	if (!pEnt)
		return false;

	const CGame::HandGripInfo* info = g_pGame->GetGripByEntity(pEnt);
	if (!info)
		return false;

	bool any = false;

	if (info->hasLeft)
	{
		outLeftEL = info->leftEL;
		any = true;
	}

	if (info->hasRight)
	{
		outRightEL = info->rightEL;
		any = true;
	}

	return any; // true if at least one hand is defined
}

void COffHand::GetPredefinedPosOffset(IEntity* pEnt, Vec3& fpPosOffset, Vec3& tpPosOffset)
{
	if (!pEnt)
		return;

	const CGame::HandGripInfo* info = g_pGame->GetGripByEntity(pEnt);
	if (!info)
		return;

	fpPosOffset = info->posOffset_FP;
	tpPosOffset = info->posOffset_TP;
}

void COffHand::AttachObjectToHand(bool attach, EntityId objectId, bool throwObject /*throwObject*/)
{
	if (!g_pGameCVars->mp_pickupApplyPhysicsVelocity)
	{
		AttachObjectToHand_original(attach, objectId, throwObject);
		return;
	}

	if (!gEnv->bClient)
		return;

	CActor* pOwner = GetOwnerActor();
	if (!pOwner)
		return;

	IEntity* pObject = gEnv->pEntitySystem->GetEntity(objectId);
	if (!pObject)
		return;

	ICharacterInstance* pOwnerCharacter = pOwner->GetEntity()->GetCharacter(0);
	IAttachmentManager* pAttachmentManager = pOwnerCharacter ? pOwnerCharacter->GetIAttachmentManager() : nullptr;
	if (!pAttachmentManager)
		return;

	const char* kAttachmentName = "held_object_attachment";
	IAttachment* pAttachment = pAttachmentManager->GetInterfaceByName(kAttachmentName);
	if (!pAttachment)
		return;

	if (attach)
	{
		// ---------- Determine two-hand mode & special cases ----------
		const bool isVehicle = g_pGame->GetIGameFramework()->GetIVehicleSystem()->IsVehicleClass(pObject->GetClass()->GetName());
		const bool isActor = !isVehicle && (g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(objectId) != nullptr);

		bool isTwoHand = IsTwoHandMode();
		if (isActor)
			isTwoHand = false;

		// ---------- Rotation offset (as in your code) ----------
		Matrix34 holdMatrix = GetHoldOffset(pObject);
		Quat rotationOffset = Quat(Matrix33(holdMatrix));

		if (ISkeletonPose* pSkeletonPose = pOwnerCharacter ? pOwnerCharacter->GetISkeletonPose() : nullptr)
		{
			Quat rollQuaternion = Quat::CreateRotationXYZ(Ang3(DEG2RAD(60), DEG2RAD(90), DEG2RAD(50)));
			rotationOffset = rollQuaternion * rotationOffset;

			if (isActor)
				rotationOffset = Quat::CreateRotationX(DEG2RAD(180)) * rotationOffset;

			rotationOffset.Normalize();
		}

		// ---------- Position offset (your logic, trimmed) ----------
		Vec3 positionOffset = Vec3(ZERO);
		IPhysicalEntity* pPhysEnt = pObject->GetPhysics();

		if (isTwoHand)
		{
			AABB bbox; pObject->GetLocalBounds(bbox);

			const float lengthX = fabsf(bbox.max.x - bbox.min.x);
			const float lengthY = fabsf(bbox.max.y - bbox.min.y);

			Vec3 vOffset(ZERO);
			vOffset.y = 0.5f + (std::max(lengthY, lengthX) * 0.2f);

			// pick COM if available, else visual center
			Vec3 P_local = 0.5f * (bbox.min + bbox.max);

			if (pPhysEnt)
			{
				pe_status_dynamics sd;
				if (pPhysEnt->GetStatus(&sd))
				{
					const Matrix34 invTM = pObject->GetWorldTM().GetInverted();
					P_local = invTM.TransformPoint(sd.centerOfMass);
				}
			}

			const Quat R = rotationOffset;
			const float kComHeightX = 0.0f;
			const float kCenterSideZ = 0.0f;
			const Vec3  desiredLocalPoint(kComHeightX, vOffset.y, kCenterSideZ);

			positionOffset = desiredLocalPoint - (R * P_local);

			if (g_pGameCVars->mp_pickupDebug)
			{
				Ang3 angXYZ = Ang3::GetAnglesXYZ(Matrix33(rotationOffset));
				Vec3 eulerDeg(RAD2DEG(angXYZ.x), RAD2DEG(angXYZ.y), RAD2DEG(angXYZ.z));
				CryLogAlways("[AttachDbg] Rot(deg)=(%.1f,%.1f,%.1f) P_local=(%.3f,%.3f,%.3f) target=(%.3f,%.3f,%.3f)",
					eulerDeg.x, eulerDeg.y, eulerDeg.z, P_local.x, P_local.y, P_local.z,
					desiredLocalPoint.x, desiredLocalPoint.y, desiredLocalPoint.z);
			}
		}
		else
		{
			// One-hand default
			positionOffset = Vec3(-0.4f, 0.7f, 0.0f);
		}

		// Predefined offsets (unchanged)
		Vec3 fpPosOffset = Vec3(ZERO), tpPosOffset = Vec3(ZERO);
		GetPredefinedPosOffset(pObject, fpPosOffset, tpPosOffset);
		positionOffset += tpPosOffset;

		// ---------- STORE data; do NOT bind the entity to the attachment ----------
		// (Ensure you have SHoldData m_hold as a member; or replace fields with your own.)
		m_hold.ownerId = pOwner->GetEntityId();
		m_hold.entityId = objectId;
		m_hold.attachmentName = kAttachmentName;
		m_hold.rotOffset = rotationOffset;
		m_hold.posOffset = positionOffset;
		m_hold.twoHand = isTwoHand;
		m_hold.active = true;
		m_hold.pPhys = pPhysEnt;

		// Store object-local COM once (for physics COM targeting)
		if (IPhysicalEntity* pe = pObject->GetPhysics())
		{
			pe_status_dynamics sd;
			if (pe->GetStatus(&sd))
			{
				const Matrix34 invTM = pObject->GetWorldTM().GetInverted();
				m_hold.comLocal = invTM.TransformPoint(sd.centerOfMass);
			}
		}

		Vec3 left = Vec3(ZERO);
		Vec3 right = Vec3(ZERO);

		if (!GetPredefinedGripHandPos(pObject, left, right))
		{
			GetScheduler()->TimerAction(
				200, //needs to be called with a delay
				MakeAction([this, objectId, isTwoHand](CItem* /*unused*/) {

					CActor* pOwner = this->GetOwnerActor();
					IEntity* pObject = gEnv->pEntitySystem->GetEntity(objectId);
					if (!pOwner || !pObject)
						return;

					SGripHitLocal rayGrips =
						ComputeGripHitsLocal(pOwner, pObject, isTwoHand);

					if (!rayGrips.ok)
					{
						CryLogAlways("[AttachGrip] Ray grips failed; keeping previous grips.");
						return;
					}

					Vec3 left = rayGrips.leftLocal;
					Vec3 right = rayGrips.rightLocal;

					if (CPlayer* pPlayer = CPlayer::FromActor(pOwner))
					{
						pPlayer->SetArmIKLocal(left, right);

						if (g_pGameCVars->mp_pickupDebug)
						{
							CryLogAlways("$3[AttachGrip] Stored ray-hit grips  L=(%.3f, %.3f, %.3f)  R=(%.3f, %.3f, %.3f)",
								left.x, left.y, left.z, right.x, right.y, right.z);
						}
					}
					}),
				false
			);
		}
		else
		{
			if (CPlayer* pPlayer = CPlayer::FromActor(pOwner))
			{
				pPlayer->SetArmIKLocal(left, right);

				if (g_pGameCVars->mp_pickupDebug)
				{
					CryLogAlways("$3[AttachGrip] Stored ray-hit grips  L=(%.3f, %.3f, %.3f)  R=(%.3f, %.3f, %.3f)",
						left.x, left.y, left.z, right.x, right.y, right.z);
				}
			}
		}
	}
	else
	{
		// Detach: stop driving (no ClearBinding needed since we never bound)
		m_hold = {};
	}
}

void COffHand::AttachObjectToHand_original(bool attach, EntityId objectId, bool throwObject)
{
	if (!gEnv->bClient)
		return;

	CActor* pOwner = GetOwnerActor();
	if (!pOwner)
		return;

	if (pOwner->IsRemote()) //Test early out
		return;

	const EntityId ownerId = pOwner->GetEntityId();
	IEntity* pObject = m_pEntitySystem->GetEntity(objectId);

	ICharacterInstance* pOwnerCharacter = pOwner->GetEntity()->GetCharacter(0);
	IAttachmentManager* pAttachmentManager = pOwnerCharacter ? pOwnerCharacter->GetIAttachmentManager() : nullptr;
	if (!pAttachmentManager)
	{
		return;
	}

	const char* attachmentName = "held_object_attachment";
	IAttachment* pAttachment = pAttachmentManager->GetInterfaceByName(attachmentName);

	if (attach && pObject)
	{
		if (pAttachment)
		{
			if (pAttachment->GetIAttachmentObject() && pAttachment->GetIAttachmentObject()->GetAttachmentType() == IAttachmentObject::eAttachment_Entity)
			{
				CEntityAttachment* pCurrent = static_cast<CEntityAttachment*>(pAttachment->GetIAttachmentObject());
				if (pCurrent && pCurrent->GetEntityId() == objectId)
				{
					return;
				}
			}

			IPhysicalEntity* pPhysEnt = pObject->GetPhysics();

			// Attach the entity to the left hand
			CEntityAttachment* pEntityAttachment = new CEntityAttachment();
			pEntityAttachment->SetEntityId(objectId);

			pAttachment->ClearBinding();
			pAttachment->AddBinding(pEntityAttachment); //Disabled for test

			bool isTwoHand = IsTwoHandMode();

			QuatT offset;
			offset.t = Vec3(ZERO);

			const bool isVehicle = g_pGame->GetIGameFramework()->GetIVehicleSystem()->IsVehicleClass(pObject->GetClass()->GetName());
			const bool isActor = !isVehicle && g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(objectId) != nullptr;
			if (isActor)
			{
				isTwoHand = false;
			}

			Matrix34 holdMatrix = GetHoldOffset(pObject);

			// ----------------------------
			// Rotation setup 
			// ----------------------------
			Quat rotationOffset = Quat(Matrix33(holdMatrix));
			if (ISkeletonPose* pSkeletonPose = pOwnerCharacter ? pOwnerCharacter->GetISkeletonPose() : nullptr)
			{
				Quat rollQuaternion = Quat::CreateRotationXYZ(Ang3(DEG2RAD(60), DEG2RAD(90), DEG2RAD(50)));
				rotationOffset = rollQuaternion * rotationOffset;

				if (isActor)
				{
					rotationOffset = Quat::CreateRotationX(DEG2RAD(180)) * rotationOffset;
				}

				rotationOffset.Normalize();
			}
			
			// ---------------------------------------------------------
			// POSITION OFFSET
			// ---------------------------------------------------------
			Vec3 positionOffset = Vec3(ZERO);

			if (isTwoHand)
			{
				// --- Desired placement: keep COM at a fixed height in attachment space ---

				Vec3 vOffset = Vec3(ZERO);

				AABB bbox;
				pObject->GetLocalBounds(bbox);

				const float lengthX = fabsf(bbox.max.x - bbox.min.x);
				const float lengthY = fabsf(bbox.max.y - bbox.min.y);
				// const float lengthZ = fabsf(bbox.max.z - bbox.min.z); // no longer used for height

				// Legacy forward-depth heuristic (attachment Y)
				vOffset.y = 0.5f + (std::max(lengthY, lengthX) * 0.2f);

				// --- Choose the object-local point to center: physics COM if possible, else AABB center ---
				Vec3 P_local = 0.5f * (bbox.min + bbox.max); // fallback: visual center

				if (pPhysEnt)
				{
					pe_status_dynamics sd;
					if (pPhysEnt->GetStatus(&sd))
					{
						const Matrix34 objWMInv = pObject->GetWorldTM().GetInverted();
						P_local = objWMInv.TransformPoint(sd.centerOfMass);	
					}
				}

				// --- Lock COM to a fixed height (attachment X) and current forward target (attachment Y) ---
				const Quat R = rotationOffset;

				// Tunables: attachment-local targets
				const float kComHeightX = 0.0f;   // meters up from the attachment (X = up)
				const float kCenterSideZ = 0.0f;    // keep centered left/right (Z)

				// We want: R * P_local + t = desiredLocalPoint  =>  t = desiredLocalPoint - R * P_local
				const Vec3 desiredLocalPoint(kComHeightX, vOffset.y, kCenterSideZ);
				Vec3 newPosOffset = desiredLocalPoint - (R * P_local);

				positionOffset = newPosOffset;

				// --- Debug ---
				if (g_pGameCVars->mp_pickupDebug)
				{
					Ang3 angXYZ = Ang3::GetAnglesXYZ(Matrix33(rotationOffset));
					Vec3 eulerDeg(RAD2DEG(angXYZ.x), RAD2DEG(angXYZ.y), RAD2DEG(angXYZ.z));

					CryLogAlways("[AttachDbg] Rot(deg)=(%.1f,%.1f,%.1f)  P_local=(%.3f,%.3f,%.3f)  targetX=%.3f Y=%.3f Z=%.3f",
						eulerDeg.x, eulerDeg.y, eulerDeg.z, P_local.x, P_local.y, P_local.z,
						desiredLocalPoint.x, desiredLocalPoint.y, desiredLocalPoint.z);

					CryLogAlways("[AttachDbg] posOffset=(%.3f, %.3f, %.3f)  (mp_pa8=%d)",
						positionOffset.x, positionOffset.y, positionOffset.z, g_pGameCVars->mp_pa8);
				}
			}
			else
			{
				// One-hand default
				positionOffset = Vec3(-0.4f, 0.7f, 0.0f);
			}
			
			Vec3 fpPosOffset = Vec3(ZERO);
			Vec3 tpPosOffset = Vec3(ZERO);
			GetPredefinedPosOffset(pObject, fpPosOffset, tpPosOffset);

			positionOffset += tpPosOffset;

			// Apply final (rotation + position)
			pAttachment->SetAttRelativeDefault(QuatT(rotationOffset, positionOffset));

			// Optional persistent debug sphere at COM (from your earlier code)
			if (g_pGameCVars->mp_pickupDebug)
			{
				Vec3 t = holdMatrix.GetTranslation();
				CryLogAlways("offset: %f %f %f m_holdOffset pos: %f %f %f", positionOffset.x, positionOffset.y, positionOffset.z, t.x, t.y, t.z);

				GetScheduler()->TimerAction(
					1000,
					MakeAction([this, pObject](CItem* /*unused*/) {

						const float life = 20.0f; // seconds
						if (IPersistantDebug* pd = g_pGame->GetIGameFramework()->GetIPersistantDebug())
						{
							static const char* kTag = "FPDebug";
							pd->Begin(kTag, /*clear*/ true);

							pe_status_dynamics dyn;
							Vec3 objectPos = Vec3(ZERO);
							if (pObject->GetPhysics() && pObject->GetPhysics()->GetStatus(&dyn))
							{
								objectPos = dyn.centerOfMass;
								pd->AddSphere(objectPos, 0.3f, ColorF(1, 0, 0, 1), life);
							}
						}
						}),
					/*persistent=*/false
				);
			}

			// ----------------
			// IK scheduling
			// ----------------

			if (CPlayer* pPlayer = CPlayer::FromActor(pOwner))
			{
				pPlayer->SetArmIKLocalInvalid();
			}

			Vec3 left = Vec3(ZERO);
			Vec3 right = Vec3(ZERO);

			if (!GetPredefinedGripHandPos(pObject, left, right))
			{
				GetScheduler()->TimerAction(
					200, //needs to be called with a delay
					MakeAction([this, ownerId, objectId, isTwoHand](CItem* /*unused*/) {

						CActor* pOwner = this->GetOwnerActor();
						IEntity* pObject = gEnv->pEntitySystem->GetEntity(objectId);
						if (!pOwner || !pObject)
							return;

						SGripHitLocal rayGrips =
							ComputeGripHitsLocal(pOwner, pObject, isTwoHand);

						if (!rayGrips.ok)
						{
							CryLogAlways("[AttachGrip] Ray grips failed; keeping previous grips.");
							return;
						}

						Vec3 left = rayGrips.leftLocal;
						Vec3 right = rayGrips.rightLocal;

						if (CPlayer* pPlayer = CPlayer::FromActor(pOwner))
						{
							pPlayer->SetArmIKLocal(left, right);

							if (g_pGameCVars->mp_pickupDebug)
							{
								CryLogAlways("$3[AttachGrip] Stored ray-hit grips  L=(%.3f, %.3f, %.3f)  R=(%.3f, %.3f, %.3f)",
									left.x, left.y, left.z, right.x, right.y, right.z);
							}
						}
						}),
					false
				);
			}
			else
			{
				if (CPlayer* pPlayer = CPlayer::FromActor(pOwner))
				{
					pPlayer->SetArmIKLocal(left, right);

					if (g_pGameCVars->mp_pickupDebug)
					{
						CryLogAlways("$3[AttachGrip] Stored ray-hit grips  L=(%.3f, %.3f, %.3f)  R=(%.3f, %.3f, %.3f)",
							left.x, left.y, left.z, right.x, right.y, right.z);
					}
				}
			}

			if (IPhysicalEntity* pe = pObject->GetPhysics()) {
				pe_status_dynamics sd;
				if (pe->GetStatus(&sd)) {
					const Matrix34 invTM = pObject->GetWorldTM().GetInverted();
					m_hold.comLocal = invTM.TransformPoint(sd.centerOfMass);
				}
			}

			m_hold.ownerId = ownerId;
			m_hold.rotOffset = rotationOffset;
			m_hold.posOffset = positionOffset;
			m_hold.twoHand = isTwoHand;
			m_hold.active = true;
		}
	}
	else
	{
		if (pAttachment)
		{
			pAttachment->ClearBinding();
		}

		m_hold.active = false;
	}
}

//==============================================================
void COffHand::UpdateEntityRenderFlags(const EntityId entityId, EntityFpViewMode mode)
{
	if (!gEnv->bClient)
		return;

	IEntity* pEntity = m_pEntitySystem->GetEntity(entityId);
	if (!pEntity)
		return;

	CActor* pOwner = GetOwnerActor();

	bool enable = pOwner && !pOwner->IsThirdPerson();

	if (mode == EntityFpViewMode::ForceActive)
		enable = true;
	else if (mode == EntityFpViewMode::ForceDisable)
		enable = false;

	if (mode == EntityFpViewMode::Default && enable == m_objectFpMode)
		return;

	m_objectFpMode = enable;

	ICharacterInstance* pCharacter = pEntity->GetCharacter(0);

	if (enable)
	{
		//FP Mode
		if (pCharacter)
		{
			pCharacter->SetFlags(pCharacter->GetFlags() | ENTITY_SLOT_RENDER_NEAREST);
			if (IEntityRenderProxy* pProxy = static_cast<IEntityRenderProxy*>(pEntity->GetProxy(ENTITY_PROXY_RENDER)))
			{
				if (IRenderNode* pRenderNode = pProxy->GetRenderNode())
					pRenderNode->SetRndFlags(ERF_RENDER_ALWAYS, true);
			}
		}
		else
		{
			DrawNear(true, entityId);
		}
	}
	else
	{
		if (pCharacter)
		{
			pCharacter->SetFlags(pCharacter->GetFlags() & (~ENTITY_SLOT_RENDER_NEAREST));
			if (IEntityRenderProxy* pProxy = static_cast<IEntityRenderProxy*>(pEntity->GetProxy(ENTITY_PROXY_RENDER)))
			{
				if (IRenderNode* pRenderNode = pProxy->GetRenderNode())
					pRenderNode->SetRndFlags(ERF_RENDER_ALWAYS, false);
			}
		}
		else
		{
			DrawNear(false, entityId);
		}
	}
}

//==============================================================
void COffHand::EnableFootGroundAlignment(bool enable)
{
	CActor* pActor = GetOwnerActor();
	if (pActor && (enable || m_grabType == GRAB_TYPE_TWO_HANDED))
	{
		if (ICharacterInstance* pCharacter = pActor->GetEntity()->GetCharacter(0))
		{
			if (ISkeletonPose* pSkeletonPose = pCharacter->GetISkeletonPose())
			{
				pSkeletonPose->EnableFootGroundAlignment(enable);
				m_footAlignmentEnabled = enable;
			}
		}
	}
}

//==============================================================
bool COffHand::IsEnableCollisionsTimerActive()
{
	return m_timerEnableCollisions && GetScheduler()->IsTimerActive(m_timerEnableCollisions);
}

//==============================================================
CActor::ObjectHoldType COffHand::DetermineObjectHoldType(const EntityId entityId) const
{
	if (m_pActorSystem->GetActor(entityId))
	{
		return CActor::ObjectHoldType::Actor;
	}
	else if (m_pGameFramework->GetIVehicleSystem()->GetVehicle(entityId))
	{
		return CActor::ObjectHoldType::Vehicle;
	}
	else if (g_pGame->GetWeaponSystem()->GetProjectile(entityId))
	{
		return CActor::ObjectHoldType::Projectile;
	}
	else
	{
		if (IsGrabTypeTwoHanded(entityId))
		{
			return CActor::ObjectHoldType::TwoHanded;
		}
		else
		{
			return CActor::ObjectHoldType::OneHanded;
		}
	}
	return CActor::ObjectHoldType::None;
}

//==============================================================
bool COffHand::SetHeldEntityId(const EntityId entityId)
{
	m_heldVehicleCollisions = 0;

	if (m_heldEntityId == entityId)
	{
		CryLogWarningAlways("Failed to set held entity ID %u: already holding that entity");
		return false;
	}

	// If swapping directly between two entities, drop current first
	if (m_heldEntityId && entityId)
	{
		RemoveHeldEntityId(m_heldEntityId, ConstraintReset::Immediate);
	}

	CActor* pActor = GetOwnerActor();
	if (!pActor)
		return false;

	m_heldEntityId = entityId;

	const bool isNewItem = (m_pItemSystem->GetItem(entityId) != nullptr);

	// If clearing or switching to a non-item/new target, ensure actor state mirrors that
	if (!isNewItem || !entityId)
	{
		const CActor::ObjectHoldType armHoldType = DetermineObjectHoldType(entityId);

		pActor->SetHeldObjectId(entityId, armHoldType);

		if (CPlayer *pPlayer = CPlayer::FromActor(pActor))
		{
			if (armHoldType == CActor::ObjectHoldType::TwoHanded || armHoldType == CActor::ObjectHoldType::Vehicle)
			{
				pPlayer->SetExtension("c4");
			}
			else
			{
				pPlayer->SetExtension("nw");
				pPlayer->SetInput("holding_grenade", true);
			}
		}
	}

	// Server: skip client-side acquisition logic below
	if (!gEnv->bClient)
		return true;

	// If no new entity or it's an item, we're done (original behavior)
	if (!entityId || isNewItem)
		return true;

	// Acquire/setup the new held entity
	HandleNewHeldEntity(entityId, isNewItem, pActor);

	if (g_pGameCVars->mp_pickupDebug)
		CryLogAlways("Successfully set held entity ID %u", entityId);

	return true;
}

//==============================================================
bool COffHand::RemoveHeldEntityId(const EntityId oldId /* = 0*/, ConstraintReset constraintReset /* = ConstraintReset::Immediate*/)
{
	const EntityId oldHeldEntityId = oldId ? oldId : m_heldEntityId;
	if (!oldHeldEntityId)
	{
		if (g_pGameCVars->mp_pickupDebug)
			CryLogWarningAlways("COffHand::RemoveHeldEntityId: no held entity to remove");
		return false;
	}

	// Update local state to "no entity" (matches original recursion path)
	m_heldEntityId = 0;

	CActor* pActor = GetOwnerActor();
	if (pActor)
	{
		// Mirror actor's held object state (in the original call this happened
		// because entityId == 0 && !isNewItem)
		pActor->SetHeldObjectId(0);
	}

	// Server: skip the client-side visuals/physics logic entirely
	if (!gEnv->bClient)
		return true;

	// Compute once here so you can pass the right flag to your handler
	const bool isOldItem = (m_pItemSystem->GetItem(oldHeldEntityId) != nullptr);

	// Release/cleanup phase for the old entity
	HandleOldHeldEntity(oldHeldEntityId, isOldItem, constraintReset, pActor);

	if (g_pGameCVars->mp_pickupDebug)
		CryLogAlways("Successfully removed held entity ID %u", oldHeldEntityId);

	return true;
}

//==============================================================
void COffHand::HandleNewHeldEntity(const EntityId entityId, const bool isNewItem, CActor* pActor)
{
	// If there's no entity or it's an item, the caller will early-return before calling this.
	IEntity* pEntity = m_pEntitySystem->GetEntity(entityId);
	if (!pEntity || !pActor)
		return;

	SelectGrabType(pEntity);

	EnableFootGroundAlignment(false);

	if (!pActor->IsThirdPerson())
	{
		UpdateEntityRenderFlags(entityId, EntityFpViewMode::ForceActive);
	}

	if (!g_pGameCVars->mp_pickupApplyPhysicsVelocity)
	{
		// Remote client in MP: mark client-only and zero mass while storing original mass on actor
		if (gEnv->bClient && gEnv->bMultiplayer && pActor->IsRemote())
		{

			pEntity->SetFlags(pEntity->GetFlags() | ENTITY_FLAG_CLIENT_ONLY);

			if (IPhysicalEntity* pObjectPhys = pEntity->GetPhysics())
			{
				pe_status_dynamics dyn;
				if (pObjectPhys->GetStatus(&dyn))
				{
					pe_simulation_params simParams;
					simParams.mass = 0.0f;

					if (pObjectPhys->SetParams(&simParams))
					{
						pActor->SetHeldObjectMass(dyn.mass);
					}
				}
			}
		}
	}

	SetIgnoreCollisionsWithOwner(true, entityId);

	if (pActor->IsThirdPerson())
	{
		if (CPlayer *pPlayer = CPlayer::FromActor(pActor))
		{
			const bool ok = pPlayer->SetGrabTarget(entityId);
		}

		GetScheduler()->TimerAction(
			500,
			MakeAction([this](CItem* /*cItem*/) {
				this->OnReachReady();
				}),
			/*persistent=*/false
		);

		//AttachObjectToHand(true, entityId, false);
	}
}

//==============================================================
void COffHand::HandleOldHeldEntity(const EntityId oldHeldEntityId, const bool isOldItem, ConstraintReset constraintReset, CActor* pActor)
{
	// If we previously held a non-item, clean up visuals & stance first.
	if (!isOldItem)
	{
		if (oldHeldEntityId)
		{
			UpdateEntityRenderFlags(oldHeldEntityId, EntityFpViewMode::ForceDisable);
			AttachObjectToHand(false, oldHeldEntityId, false);
		}
		EnableFootGroundAlignment(true);
	}

	if (!oldHeldEntityId || isOldItem)
		return;

	if (IEntity* pOldEntity = m_pEntitySystem->GetEntity(oldHeldEntityId))
	{
		// Collisions reset (immediate / delayed / skip if timer active)
		const bool skip = (constraintReset & ConstraintReset::SkipIfDelayTimerActive) && IsEnableCollisionsTimerActive();
		if (!skip)
		{
			if (constraintReset & ConstraintReset::Immediate)
			{
				SetIgnoreCollisionsWithOwner(false, oldHeldEntityId);
			}
			else if (constraintReset & ConstraintReset::Delayed)
			{
				/*
				m_timerEnableCollisions = GetScheduler()->TimerAction(
					500,
					CSchedulerAction<Timer_EnableCollisionsWithOwner>::Create(
						Timer_EnableCollisionsWithOwner(this, oldHeldEntityId)),
					false);
					*/

				m_timerEnableCollisions = GetScheduler()->TimerAction(
					500,
					MakeAction([this, id = oldHeldEntityId](CItem* /*cItem*/) {
						SetIgnoreCollisionsWithOwner(false, id);
						}),
					/*persistent=*/false
				);
			}
		}

		// Remote client in MP: restore mass, clear client-only, rephys vehicles, clear cached mass
		if (gEnv->bClient && gEnv->bMultiplayer && pActor && pActor->IsRemote())
		{
			
			if (!g_pGameCVars->mp_pickupApplyPhysicsVelocity)
			{
				if (IPhysicalEntity* pObjectPhys = pOldEntity->GetPhysics())
				{
					pe_status_dynamics dyn;
					pObjectPhys->GetStatus(&dyn);

					const float originalMass = pActor->GetHeldObjectMass();
					if (originalMass > 0.0f && dyn.mass != originalMass)
					{
						pe_simulation_params simParams;
						simParams.mass = originalMass;
						if (pObjectPhys->SetParams(&simParams))
						{
							pe_status_dynamics tmp;
							pObjectPhys->GetStatus(&tmp);
						}
					}
				}
			}

			//pOldEntity->SetFlags(pOldEntity->GetFlags() & ~ENTITY_FLAG_CLIENT_ONLY);
			

			if (IVehicle* pVehicle = m_pGameFramework->GetIVehicleSystem()->GetVehicle(pOldEntity->GetId()))
			{
				// CryMP: trigger rephysicalization to avoid bugs caused by 0 mass
				reinterpret_cast<IGameObjectProfileManager*>(pVehicle + 1)->SetAspectProfile(eEA_Physics, 1);
			}

			pActor->SetHeldObjectMass(0.0f);
		}
	}
}

void COffHand::OnReachReady()
{
	AttachObjectToHand(true, m_heldEntityId, false);
}

void COffHand::OnHeldObjectCollision(CPlayer* pClientActor, const EventPhysCollision* pCollision, IEntity* pTargetEnt)
{
	if (!pClientActor || !m_heldEntityId)
		return;

	const int maxCollisions = g_pGameCVars->mp_pickupMaxVehicleCollisions;
	if (!maxCollisions)
		return;

	IVehicle* pVehicle = m_pVehicleSystem->GetVehicle(m_heldEntityId);
	if (pVehicle)
	{
		++m_heldVehicleCollisions;
		if (m_heldVehicleCollisions > maxCollisions)
		{
			g_pGame->GetHUD()->DisplayBigOverlayFlashMessage("Woops, you lost your vehicle! Be careful...", 2.0f, 400, 400, Col_Goldenrod); //PO:TODO translation
			CancelAction();
		}
	}
}

void COffHand::OnPlayerRevive(CPlayer* pPlayer)
{
	FinishAction(eOHA_RESET);
}

void COffHand::OnPlayerDied(CPlayer* pPlayer)
{
	FinishAction(eOHA_RESET);
}

void COffHand::ReAttachObjectToHand()
{
	if (m_heldEntityId && !m_stats.fp)
	{
		AttachObjectToHand(false, m_heldEntityId, false);
		AttachObjectToHand(true, m_heldEntityId, false);
	}
}

static const char* GeomTypeToString(int t)
{
	// eGeomTypes from CryPhysics
	switch (t)
	{
	case GEOM_TRIMESH:  return "trimesh";
	case GEOM_BOX:      return "box";
	case GEOM_SPHERE:   return "sphere";
	case GEOM_CYLINDER: return "cylinder";
	case GEOM_CAPSULE:  return "capsule";
	case GEOM_RAY:      return "ray";
		// (Older builds may also have GEOM_OBB, etc.)
	default:            return "unknown";
	}
}

void DumpPhysicsGeomTypes(IEntity* pEnt)
{
	if (!pEnt) return;
	IPhysicalEntity* pPhys = pEnt->GetPhysics();
	if (!pPhys) { CryLog("No physics"); return; }

	// 1) How many parts?
	pe_status_nparts snp;
	int nparts = pPhys->GetStatus(&snp);

	for (int i = 0; i < nparts; ++i)
	{
		// 2) Query that part’s params to get the phys geometry pointer(s)
		pe_params_part pp;
		pp.ipart = i;
		if (!pPhys->GetParams(&pp))
			continue;

		// Prefer the “game” geometry; proxy is the simplified collision if present
		for (phys_geometry* pPG : { pp.pPhysGeom, pp.pPhysGeomProxy })
		{
			if (pPG && pPG->pGeom)
			{
				int gtype = pPG->pGeom->GetType();   // ← this is the key call
				CryLog("Part %d: %s (%s)",
					i,
					pPG == pp.pPhysGeom ? "geom" : "proxy",
					GeomTypeToString(gtype));
			}
		}
	}
}

void COffHand::ToggleHeldEntityClientFlag()
{
	IEntity* pEntity = GetEntity();
	CActor* pOwnerActor = GetOwnerActor();
	if (!pOwnerActor)
		return;

	IEntity* pHeld = gEnv->pEntitySystem->GetEntity(m_heldEntityId);
	if (!pHeld)
	{
		CryLogAlways("[OffHand] ToggleHeldEntityClientFlag: no held entity (id=%u).", (uint32)m_heldEntityId);
		return;
	}

	const char* name = pHeld->GetName();
	uint32 flags = pHeld->GetFlags();

	if (flags & ENTITY_FLAG_CLIENT_ONLY)
	{
		pHeld->ClearFlags(ENTITY_FLAG_CLIENT_ONLY);
		CryLogAlways("[OffHand] %s (%u): ENTITY_FLAG_CLIENT_ONLY -> OFF", name, (uint32)m_heldEntityId);
	}
	else
	{
		pHeld->AddFlags(ENTITY_FLAG_CLIENT_ONLY);
		CryLogAlways("[OffHand] %s (%u): ENTITY_FLAG_CLIENT_ONLY -> ON", name, (uint32)m_heldEntityId);
	}

	CryLogAlways("[OffHand] %s: new flags=0x%08X", name, pHeld->GetFlags());
}

void COffHand::DebugLogInfo()
{
	IEntity* pEntity = GetEntity();
	CActor* pOwnerActor = GetOwnerActor();
	if (!pOwnerActor)
		return;

	const char* itemName = pEntity ? pEntity->GetName() : "<no entity>";

	// -- Owner --
	const char* ownerName = "<no owner>";
	if (pOwnerActor)
	{
		if (IEntity* pOwnerEntity = pOwnerActor->GetEntity())
			ownerName = pOwnerEntity->GetName();
	}

	// -- Grab Type --
	const char* grabStr = "Unknown";
	switch (m_grabType)
	{
	case GRAB_TYPE_ONE_HANDED: grabStr = "1-Hand"; break;
	case GRAB_TYPE_TWO_HANDED: grabStr = "2-Hand"; break;
	case GRAB_TYPE_NPC:        grabStr = "NPC"; break;
	default: break;
	}

	// -- Held Entity --
	EntityId heldId = m_heldEntityId;
	if (!heldId)
	{
		if (CPlayer* pPlayer = CPlayer::FromActor(GetOwnerActor()))
		{
			heldId = pPlayer->GetHeldObjectId();
		}
	}

	IEntity* pHeld = gEnv->pEntitySystem->GetEntity(heldId);

	float mass = -1.f;
	if (pHeld)
	{
		if (IPhysicalEntity* pPhys = pHeld->GetPhysics())
		{
			pe_status_dynamics dyn;
			if (pPhys->GetStatus(&dyn))
			{
				mass = dyn.mass;
			}
		}
	}

	// -- Current State Bitmask --
	string stateBits;
	struct StateFlagInfo { int flag; const char* name; };
	const StateFlagInfo flags[] = {
		{ eOHS_INIT_STATE,        "Init" },
		{ eOHS_SWITCHING_GRENADE, "SwitchingGrenade" },
		{ eOHS_HOLDING_GRENADE,   "HoldingGrenade" },
		{ eOHS_THROWING_GRENADE,  "ThrowingGrenade" },
		{ eOHS_PICKING,           "Picking" },
		{ eOHS_PICKING_ITEM,      "PickingItem" },
		{ eOHS_PICKING_ITEM2,     "PickingItem2" },
		{ eOHS_HOLDING_OBJECT,    "HoldingObject" },
		{ eOHS_THROWING_OBJECT,   "ThrowingObject" },
		{ eOHS_GRABBING_NPC,      "GrabbingNPC" },
		{ eOHS_HOLDING_NPC,       "HoldingNPC" },
		{ eOHS_THROWING_NPC,      "ThrowingNPC" },
		{ eOHS_TRANSITIONING,     "Transitioning" },
		{ eOHS_MELEE,             "Melee" }
	};

	for (const auto& f : flags)
	{
		if (m_currentState & f.flag)
		{
			if (!stateBits.empty()) stateBits += "|";
			stateBits += f.name;
		}
	}

	// -- Game Object Update Status --
	IGameObject* pGameObject = GetGameObject();
	uint8_t slotEnables = pGameObject->GetUpdateSlotEnables(this, eIUS_General);
	uint8_t slotEnables2 = pGameObject->GetUpdateSlotEnables(this, eIUS_Scheduler);

	// -- Fire Mode Index --
	int fireMode = GetCurrentFireMode();

	IGameObject* pActorObject = pOwnerActor->GetGameObject();
	uint8_t playerUpdating = pActorObject->GetUpdateSlotEnables(this, eIUS_General);

	// -- Final Output --
	CryLogAlways("$9[OffHand] Owner=$5%s$9 (Updating:%s%s$9), Item=%s, Grab=%s, Mode=%d, Timer=%s%.2f$9, Update=%s%s",
		ownerName, playerUpdating ? "$3" : "$9", playerUpdating ? "Yes" : "No",  itemName, grabStr, fireMode,
		m_nextThrowTimer > 0.0f ? "$3" : "$9", m_nextThrowTimer,
		slotEnables ? "$3" : "$9", slotEnables ? "Yes" : "No");

	CryLogAlways("$9           StateFlags: $6%s", stateBits.c_str());

	CryLogAlways("$9           FireMode Handle: $3%p", GetActiveFireMode());

	CryLogAlways("$9           ResetTimer: %f", m_resetTimer);

	CryLogAlways("$9           Scheduler: Actives %d - Timers %d", GetScheduler()->GetActivesCount(), GetScheduler()->GetTimersCount());

	CryLogAlways("$9           Scheduler Update: $1%d", slotEnables2);

	if (pHeld)
	{
		CryLogAlways("$9           Holding: %s (%d), Mass=$1%.2f", pHeld->GetName(), heldId, mass);

		if (IPhysicalEntity* pPhys = pHeld->GetPhysics())
		{
			pe_status_nparts snp;
			const int nParts = pPhys->GetStatus(&snp); // CE2: returns number of parts directly
			CryLogAlways("$9           Physics parts: $6%d", nParts);

			for (int i = 0; i < nParts; ++i)
			{
				pe_params_part pp;
				pp.ipart = i;
				if (!pPhys->GetParams(&pp))
				{
					CryLogAlways("$9           Part %d: $4<GetParams failed>", i);
					continue;
				}

				const phys_geometry* pPG = pp.pPhysGeomProxy ? pp.pPhysGeomProxy : pp.pPhysGeom;
				const char* label = pp.pPhysGeomProxy ? "proxy" : "geom";

				const int gtype = (pPG && pPG->pGeom) ? pPG->pGeom->GetType() : -1;
				const char* typeStr = "unknown";

				switch (gtype)
				{
				case GEOM_TRIMESH:    typeStr = "trimesh";    break;
				case GEOM_HEIGHTFIELD:typeStr = "heightfield"; break;
				case GEOM_CYLINDER:   typeStr = "cylinder";   break;
				case GEOM_CAPSULE:    typeStr = "capsule";    break;
				case GEOM_RAY:        typeStr = "ray";        break;
				case GEOM_SPHERE:     typeStr = "sphere";     break;
				case GEOM_BOX:        typeStr = "box";        break;
				case GEOM_VOXELGRID:  typeStr = "voxelgrid";  break;
				default:              typeStr = "unknown";    break;
				}

				CryLogAlways("$9           Part %d: %s: $6%s", i, label, typeStr);
			}
		}
		else
		{
			CryLogAlways("$9           Physics: None");
		}
	}
	else
	{
		CryLogAlways("$9           Holding: None");
	}

	auto* pHolster = pOwnerActor->GetHolsteredItem();
	if (pHolster)
	{
		CryLogAlways("$9           Holstered: $5%s$9",
			pHolster->GetEntity()->GetName());
	}
	auto* pCurr = pOwnerActor->GetCurrentItem();
	if (pCurr)
	{
		CryLogAlways("$9           Item: $5%s$9",
			pCurr->GetEntity()->GetName());
	}
	CryLogAlways("$9           %s", GetEntity()->IsHidden() ? "OffHand Entity Hidden" : "OffHand Entity $3Visible");
	CryLogAlways("$9--------------------------------------------");
	CryLogAlways("$9           Character Master: %s", IsFirstPersonCharacterMasterHidden() ? "Hidden" : "$3Visible$9");
	CryLogAlways("$9           Hands: %d", m_stats.hand);
	const ItemString &attachment = GetParams().attachment[m_stats.hand];
	CryLogAlways("$9           Attachment Name: %s (%s)", attachment.c_str(), IsCharacterAttachmentHidden(eIGS_FirstPerson, attachment.c_str()) ? "Hidden" : "$3Visible$9");
	CryLogAlways("$9           %s", IsArmsHidden() ? "FP Arms Hidden" : "FP Arms $3Visible");

	// -- Constraint Status --
	const char* constraintStr = "Inactive";
	const char* constraintClr = "$9"; // gray

	switch (m_constraintStatus)
	{
	case ConstraintStatus::Inactive:
		constraintStr = "Inactive";
		constraintClr = "$9"; // gray
		break;
	case ConstraintStatus::WaitForPhysicsUpdate:
		constraintStr = "WaitingForPhys";
		constraintClr = "$3"; // yellow/orange
		break;
	case ConstraintStatus::Active:
		constraintStr = "Active";
		constraintClr = "$5"; // green
		break;
	case ConstraintStatus::Broken:
		constraintStr = "Broken";
		constraintClr = "$4"; // red
		break;
	default:
		break;
	}

	CryLogAlways("$9           Constraint: %s%s$9", constraintClr, constraintStr);

	CryLogAlways("$9           Foot Alignment: %s", m_footAlignmentEnabled ? "$1Enabled" : "Disabled");
}


void COffHand::TickCarryPhysics(IEntity* pObject)
{
	if (!m_hold.active || !pObject)
		return;

	// ---------- Authority only (local client owns the item) ----------
	CActor* pOwner = GetOwnerActor();
	if (!pOwner)
		return;

	// keep your original remote guard
	if (pOwner->IsRemote())
		return;

	ICharacterInstance* chr = pOwner->GetEntity()->GetCharacter(0);
	IAttachmentManager* am = chr ? chr->GetIAttachmentManager() : nullptr;
	IAttachment* att = am ? am->GetInterfaceByName(m_hold.attachmentName.c_str()) : nullptr;
	if (!att) return;

	IPhysicalEntity* pe = pObject->GetPhysics();
	if (!pe) return;

	pe_status_pos sp;      if (!pe->GetStatus(&sp)) return; // sp.pos (COM), sp.q
	pe_status_dynamics sd; pe->GetStatus(&sd);              // sd.v, sd.w

	// ---------- Helpers ----------
	struct H {
		static inline Vec3 RotVecFromQuatDelta(const Quat& qTarget, const Quat& qCurrent) {
			Quat dq = qTarget * !qCurrent; dq.Normalize();
			// Shortest arc
			if (dq.w < 0.f) { dq.w = -dq.w; dq.v = -dq.v; }
			const float vmag = dq.v.GetLength();
			if (vmag < 1e-8f) return dq.v * 2.f; // small-angle
			const float angle = 2.f * atan2f(vmag, dq.w);
			return dq.v * (angle / vmag);        // axis * angle
		}
	};

	const float dtRaw = gEnv->pPhysicalWorld->GetPhysVars()->timeGranularity;
	if (dtRaw <= 0.f)
		return;

	// MOD: prevent derivative explosion at very small timesteps
	const float safeDt = max(dtRaw, 1.0f / 120.0f);

	// ---------- Target pivot pose from hand + stored offset ----------
	const QuatT handWT = att->GetAttWorldAbsolute();
	const Quat  q_off = m_hold.rotOffset.GetNormalized();
	const Quat  q_off_inv = !q_off;
	const Vec3  t_off = m_hold.posOffset;
	const Vec3  t_off_inv = -(q_off_inv * t_off);
	const QuatT offset_AttToItem(q_off_inv, t_off_inv);

	const QuatT itemPivot = handWT * offset_AttToItem;
	const Quat q_pivot = itemPivot.q.GetNormalized();
	const Vec3 p_pivot = itemPivot.t;

	// ---------- Target COM pose ----------
	const Vec3 r_local = m_hold.comLocal;      // pivot->COM in item local (legacy meaning)
	const Vec3 r_world = q_pivot * r_local;    // rotate into world

	// MOD: many setups expect COM to sit *at* the attachment center.
	//      Lock COM to pivot by ignoring the COM offset for positional target/kinematics.
	static const bool kCOMAtAttachment = true;

	const Vec3 p_t = kCOMAtAttachment ? p_pivot : (p_pivot + r_world); // desired COM (wish position)
	const Quat q_t = q_pivot; // rotation unchanged

	// ---------- Hand kinematics (v_pivot, a_pivot, w_hand, alpha_hand) ----------
	static bool  s_prevValid = false;
	static Vec3  s_prev_p_pivot = ZERO;
	static Vec3  s_prev_v_pivot = ZERO;
	static Quat  s_prev_q_pivot = IDENTITY;
	static Vec3  s_prev_w_hand = ZERO;
	static EntityId s_prevObjId = 0;

	if (s_prevObjId != pObject->GetId())
	{
		s_prevValid = false;
		s_prev_p_pivot = p_pivot;
		s_prev_q_pivot = q_pivot;
		s_prev_v_pivot = ZERO;
		s_prev_w_hand = ZERO;
		s_prevObjId = pObject->GetId();
	}

	Vec3 v_pivot = ZERO, a_pivot = ZERO;
	if (s_prevValid) {
		v_pivot = (p_pivot - s_prev_p_pivot) / safeDt;
		a_pivot = (v_pivot - s_prev_v_pivot) / safeDt;
	}

	Vec3 w_hand = ZERO, alpha_hand = ZERO;
	if (s_prevValid) {
		const Vec3 rotVec = H::RotVecFromQuatDelta(q_pivot, s_prev_q_pivot);
		w_hand = rotVec * (1.0f / safeDt);
		alpha_hand = (w_hand - s_prev_w_hand) / safeDt;
	}

	// MOD: if the attachment pose is effectively unchanged, zero out rates (kill jitter)
	{
		const float dp = (p_pivot - s_prev_p_pivot).GetLength();
		const float dtheta = H::RotVecFromQuatDelta(q_pivot, s_prev_q_pivot).GetLength(); // radians
		if (!s_prevValid || dp < 1e-4f || dtheta < DEG2RAD(0.1f))
		{
			v_pivot = ZERO; a_pivot = ZERO; w_hand = ZERO; alpha_hand = ZERO;
		}
	}

	// MOD: reset history on teleport/large delta to avoid spikes
	{
		const float jumpDist = (p_pivot - s_prev_p_pivot).GetLength();
		if (!s_prevValid || jumpDist > 1.0f) {
			v_pivot = ZERO; a_pivot = ZERO; w_hand = ZERO; alpha_hand = ZERO;
		}
	}

	// persist for next tick
	s_prev_p_pivot = p_pivot;
	s_prev_v_pivot = v_pivot;
	s_prev_q_pivot = q_pivot;
	s_prev_w_hand = w_hand;
	s_prevValid = true;

	// ---------- Rigid offset kinematics ----------
	// MOD: when COM is locked to pivot, the effective r is ZERO so no w×r terms.
	const Vec3 r_eff = kCOMAtAttachment ? ZERO : r_world;

	const Vec3 v_t = v_pivot + w_hand.Cross(r_eff);
	const Vec3 a_t = a_pivot + alpha_hand.Cross(r_eff) + w_hand.Cross(w_hand.Cross(r_eff));

	// ---------- Errors relative to moving target ----------
	const Vec3 e_p = p_t - sp.pos;                      // position error (COM)
	const Vec3 e_v = v_t - sd.v;                        // linear velocity error
	const Vec3 e_r = H::RotVecFromQuatDelta(q_t, sp.q); // orientation error (axis*angle)
	const Vec3 e_w = w_hand - sd.w;                     // angular velocity error

	// ---------- Acceleration-form critically damped PD ----------
	const float w_lin = 20.0f;  // rad/s, linear natural frequency
	const float w_ang = 22.0f;  // rad/s, angular natural frequency

	const float KpL = w_lin * w_lin;
	const float KdL = 2.0f * w_lin;
	const float KpA = w_ang * w_ang;
	const float KdA = 2.0f * w_ang;

	// Linear: aim for target acceleration + PD around moving target
	const Vec3 a_cmd = a_t + KpL * e_p + KdL * e_v;
	Vec3 v_des = sd.v + a_cmd * safeDt;

	// Angular: same idea
	const Vec3 alpha_cmd = alpha_hand + KpA * e_r + KdA * e_w;
	Vec3 w_des = sd.w + alpha_cmd * safeDt;

	// ---------- No-overshoot caps ----------
	{
		const float dist = e_p.GetLength();
		if (dist > 1e-6f)
		{
			const Vec3 e_hat = e_p / dist;
			const Vec3 v_err = v_des - v_t;
			const float v_tow = v_err.Dot(e_hat);
			const float v_maxTow = dist / safeDt;
			if (v_tow > v_maxTow)
				v_des -= e_hat * (v_tow - v_maxTow);
		}
	}
	{
		const float theta = e_r.GetLength();  // radians
		if (theta > 1e-6f)
		{
			const Vec3 er_hat = e_r / theta;
			const Vec3 w_err = w_des - w_hand;
			const float w_tow = w_err.Dot(er_hat);
			const float w_maxTow = theta / safeDt;
			if (w_tow > w_maxTow)
				w_des -= er_hat * (w_tow - w_maxTow);
		}
	}

	// ---------- Clamp + tiny deadzones ----------
	const float vMax = 25.0f;
	const float wMax = 50.0f;

	if (e_p.GetLength() < 0.002f) v_des = v_t;    // avoid micro-chatter
	if (e_r.GetLength() < 0.001f) w_des = w_hand;

	v_des.ClampLength(vMax);
	w_des.ClampLength(wMax);

	// ---------- Apply to physics ----------
	pe_action_awake aw; aw.bAwake = 1; pe->Action(&aw);

	pe_action_set_velocity av;
	av.v = v_des;
	av.w = w_des;
	pe->Action(&av);

	// ---------- Debug logging ----------
	if (g_pGameCVars && g_pGameCVars->mp_pickupDebug)
	{
		const float dist = e_p.GetLength();
		const float theta = e_r.GetLength();
		const float vlen = v_des.GetLength();
		const float wlen = w_des.GetLength();
		const float v_tlen = v_t.GetLength();

		float vTow = 0.f, wTow = 0.f;
		if (dist > 1e-6f) { Vec3 e_hat = e_p / dist;  vTow = (v_des - v_t).Dot(e_hat); }
		if (theta > 1e-6f) { Vec3 er_hat = e_r / theta; wTow = (w_des - w_hand).Dot(er_hat); }

		// MOD: pivot distance reporting respects COM lock mode
		const float pivotDist = kCOMAtAttachment
			? (p_pivot - sp.pos).GetLength()
			: (p_pivot - (sp.pos - r_world)).GetLength();

		CryLogAlways("$6[CarryPD] distCOM=%.4f pivotDist=%.4f theta(deg)=%.2f  |v_des|=%.3f (|v_t|=%.3f, vTow=%.3f)  |w_des|=%.3f (wTow=%.3f)  dt=%.4f",
			dist, pivotDist, RAD2DEG(theta), vlen, v_tlen, vTow, wlen, wTow, safeDt);

		if (IPersistantDebug* pd = g_pGame->GetIGameFramework()->GetIPersistantDebug())
		{
			static const char* kTag = "CarryDebugWish";
			pd->Begin(kTag, /*clear*/ true);
			const float life = 0.15f;

			pd->AddSphere(p_t, 0.06f, ColorF(0, 1, 0, 1), life); // desired COM
			pd->AddSphere(sp.pos, 0.06f, ColorF(1, 0, 0, 1), life); // actual COM
			pd->AddLine(sp.pos, p_t, ColorF(1, 1, 0, 1), life);
		}
	}
}

// ---- COffHand.cpp (top of file or in anonymous namespace) ----
static COffHand* g_pOffHandPhysInst = nullptr;

// Plain C callback thunk (matches AddEventClient signature)
static int OffHand_OnPhysPostStep(const EventPhys* pBase)
{
	if (g_pOffHandPhysInst)
		return g_pOffHandPhysInst->OnPhysPostStep_Instance(pBase);
	return 1;
}

void COffHand::CacheHandPose_GameThread(IAttachment* att)
{
	std::lock_guard<std::mutex> lock(m_handMtx);

	if (!att)
	{
		m_handPoseValid = false;
		return;
	}

	m_handWT_cached = att->GetAttWorldAbsolute();
	m_handPoseValid = true;
}
int COffHand::OnPhysPostStep_Instance(const EventPhys* pBase)
{
	const EventPhysPostStep* ev = static_cast<const EventPhysPostStep*>(pBase);
	if (!ev || !ev->pEntity)
		return 1;

	// Only operate in physics-thread mode
	if (!g_pGameCVars || g_pGameCVars->mp_pickupApplyPhysicsVelocity != 2)
		return 1;

	// Must be holding something and it must match this stepped entity
	if (!m_hold.active || !m_hold.pPhys)
		return 1;

	// Read cached hand transform (thread-safe)
	QuatT handWT;
	{
		std::lock_guard<std::mutex> lock(m_handMtx);
		if (!m_handPoseValid)
			return 1;
		handWT = m_handWT_cached;
	}

	// Use event dt; fallback to phys vars if necessary
	float dtPhys = ev->dt;
	if (dtPhys <= 0.f)
		if (IPhysicalWorld* pw = gEnv->pPhysicalWorld)
			if (PhysicsVars* pv = pw->GetPhysVars())
				dtPhys = pv->timeGranularity;
	if (dtPhys <= 0.f)
		return 1;

	TickCarryPhysics_Thread(m_hold.pPhys, handWT, dtPhys);
	return 1;
}

void COffHand::EnablePhysPostStep(bool enable)
{
	if (enable)
	{
		if (!m_postStepRegistered)
		{
			g_pOffHandPhysInst = this; // single-instance bridge
			gEnv->pPhysicalWorld->AddEventClient(
				EventPhysPostStep::id,
				OffHand_OnPhysPostStep,   // <-- free function thunk
				/*bLogged=*/0,
				/*priority=*/1.0f);
			m_postStepRegistered = true;

			CryLogAlways("$3Enabled OffHand phys post-step callback.");
		}
	}
	else
	{
		if (m_postStepRegistered)
		{
			gEnv->pPhysicalWorld->RemoveEventClient(
				EventPhysPostStep::id,
				OffHand_OnPhysPostStep,   // <-- must match AddEventClient
				/*bLogged=*/0);
			m_postStepRegistered = false;
			if (g_pOffHandPhysInst == this)
				g_pOffHandPhysInst = nullptr;

			CryLogAlways("$3Disabled OffHand phys post-step callback.");
		}
	}
}

void COffHand::TickCarryPhysics_Thread(IPhysicalEntity* pe, const QuatT& handWT_cached, float dtPhys)
{
	if (!pe || dtPhys <= 0.f) return;

	// Safe dt (avoid derivative blow-ups on tiny substeps)
	const float safeDt = max(dtPhys, 1.0f / 120.0f);

	pe_status_pos sp;      if (!pe->GetStatus(&sp)) return; // sp.pos (COM), sp.q
	pe_status_dynamics sd; pe->GetStatus(&sd);              // sd.v, sd.w

	// ---------- Helpers ----------
	struct H {
		static inline Vec3 RotVecFromQuatDelta(const Quat& qTarget, const Quat& qCurrent) {
			Quat dq = qTarget * !qCurrent; dq.Normalize();
			if (dq.w < 0.f) { dq.w = -dq.w; dq.v = -dq.v; }
			const float vmag = dq.v.GetLength();
			if (vmag < 1e-8f) return dq.v * 2.f;
			const float angle = 2.f * atan2f(vmag, dq.w);
			return dq.v * (angle / vmag); // axis * angle
		}
	};

	// ---------- Target pivot pose from cached hand + stored offset ----------
	const QuatT offsetWT(m_hold.rotOffset, m_hold.posOffset);

	// "attachment -> item" convention
	const QuatT itemPivot = handWT_cached * offsetWT;

	Quat q_pivot = itemPivot.q.GetNormalized();
	Vec3 p_pivot = itemPivot.t;

	// --- If you want COM exactly at attachment pivot (like the original AttachObjectToHand felt):
	if (m_kCOMAtAttachment)
	{
		p_pivot = handWT_cached.t;     // pivot = attachment position
		q_pivot = handWT_cached.q;     // orientation unchanged (you said rotation feels good)
	}

	// ---------- Target COM pose ----------
	const Vec3 r_local = m_hold.comLocal;           // pivot->COM in item local
	const Vec3 r_world = q_pivot * r_local;         // rotate into world
	const Vec3 p_t = m_kCOMAtAttachment ? p_pivot : (p_pivot + r_world);
	const Quat q_t = q_pivot;

	// ---------- Hand kinematics (computed in physics thread) ----------
	// Per-thread history (single carried object at a time)
	if (m_threadPrevObjId != m_hold.entityId) // ensure you set this when you start holding
	{
		m_threadPrevValid = false;
		m_threadPrevObjId = m_hold.entityId;
	}

	Vec3 v_pivot = ZERO, a_pivot = ZERO;
	Vec3 w_hand = ZERO, alpha_hand = ZERO;

	if (m_threadPrevValid)
	{
		v_pivot = (p_pivot - m_threadPrev_p_pivot) / safeDt;
		a_pivot = (v_pivot - m_threadPrev_v_pivot) / safeDt;

		const Vec3 rotVec = H::RotVecFromQuatDelta(q_pivot, m_threadPrev_q_pivot);
		w_hand = rotVec * (1.0f / safeDt);
		alpha_hand = (w_hand - m_threadPrev_w_hand) / safeDt;

		// Kill jitter if pivot barely moved/rotated
		const float dp = (p_pivot - m_threadPrev_p_pivot).GetLength();
		const float dtheta = H::RotVecFromQuatDelta(q_pivot, m_threadPrev_q_pivot).GetLength();
		if (dp < 1e-4f || dtheta < DEG2RAD(0.1f))
		{
			v_pivot = ZERO; a_pivot = ZERO; w_hand = ZERO; alpha_hand = ZERO;
		}
	}

	// Reset on big jumps
	{
		const float jumpDist = m_threadPrevValid ? (p_pivot - m_threadPrev_p_pivot).GetLength() : 0.f;
		if (!m_threadPrevValid || jumpDist > 1.0f) {
			v_pivot = ZERO; a_pivot = ZERO; w_hand = ZERO; alpha_hand = ZERO;
		}
	}

	// persist history
	m_threadPrevValid = true;
	m_threadPrev_p_pivot = p_pivot;
	m_threadPrev_v_pivot = v_pivot;
	m_threadPrev_q_pivot = q_pivot;
	m_threadPrev_w_hand = w_hand;

	// ---------- Rigid offset kinematics at r_world ----------
	const Vec3 v_t = m_kCOMAtAttachment ? v_pivot : (v_pivot + w_hand.Cross(r_world));
	const Vec3 a_t = m_kCOMAtAttachment
		? a_pivot
		: (a_pivot + alpha_hand.Cross(r_world) + w_hand.Cross(w_hand.Cross(r_world)));

	// ---------- Errors relative to moving target ----------
	const Vec3 e_p = p_t - sp.pos;                      // position error (COM)
	const Vec3 e_v = v_t - sd.v;                        // linear velocity error
	const Vec3 e_r = H::RotVecFromQuatDelta(q_t, sp.q); // orientation error
	const Vec3 e_w = w_hand - sd.w;                     // angular velocity error

	// ---------- Acceleration-form critically damped PD ----------
	const float w_lin = 20.0f;  // linear natural frequency
	const float w_ang = 22.0f;  // angular natural frequency
	const float KpL = w_lin * w_lin;
	const float KdL = 2.0f * w_lin;
	const float KpA = w_ang * w_ang;
	const float KdA = 2.0f * w_ang;

	const Vec3 a_cmd = a_t + KpL * e_p + KdL * e_v;
	Vec3 v_des = sd.v + a_cmd * safeDt;

	const Vec3 alpha_cmd = alpha_hand + KpA * e_r + KdA * e_w;
	Vec3 w_des = sd.w + alpha_cmd * safeDt;

	// ---------- No-overshoot caps ----------
	{
		const float dist = e_p.GetLength();
		if (dist > 1e-6f)
		{
			const Vec3 e_hat = e_p / dist;
			const Vec3 v_err = v_des - v_t;
			const float v_tow = v_err.Dot(e_hat);
			const float v_maxTow = dist / safeDt;
			if (v_tow > v_maxTow)
				v_des -= e_hat * (v_tow - v_maxTow);
		}
	}
	{
		const float theta = e_r.GetLength();
		if (theta > 1e-6f)
		{
			const Vec3 er_hat = e_r / theta;
			const Vec3 w_err = w_des - w_hand;
			const float w_tow = w_err.Dot(er_hat);
			const float w_maxTow = theta / safeDt;
			if (w_tow > w_maxTow)
				w_des -= er_hat * (w_tow - w_maxTow);
		}
	}

	// ---------- Clamp + small deadzones ----------
	const float vMax = 25.0f;
	const float wMax = 50.0f;

	if (e_p.GetLength() < 0.002f) v_des = v_t;
	if (e_r.GetLength() < 0.001f) w_des = w_hand;

	v_des.ClampLength(vMax);
	w_des.ClampLength(wMax);

	// ---------- Apply to physics (we're already in physics thread) ----------
	pe_action_awake aw; aw.bAwake = 1; pe->Action(&aw);
	pe_action_set_velocity av;  av.v = v_des; av.w = w_des; pe->Action(&av);

	// ---------- Debug (optional) ----------
	if (g_pGameCVars && g_pGameCVars->mp_pickupDebug)
	{
		const float dist = e_p.GetLength();
		const float theta = e_r.GetLength();
		const float vlen = v_des.GetLength();
		const float wlen = w_des.GetLength();
		const float v_tlen = v_t.GetLength();

		float vTow = 0.f, wTow = 0.f;
		if (dist > 1e-6f) { Vec3 e_hat = e_p / dist;  vTow = (v_des - v_t).Dot(e_hat); }
		if (theta > 1e-6f) { Vec3 er_hat = e_r / theta; wTow = (w_des - w_hand).Dot(er_hat); }

		// Compare pivot distance (for sanity) using current COM and r_world
		const float pivotDist = (p_pivot - (sp.pos - r_world)).GetLength();

		CryLogAlways("$6[CarryPD/Phys] distCOM=%.4f pivotDist=%.4f theta(deg)=%.2f  |v_des|=%.3f (|v_t|=%.3f, vTow=%.3f)  |w_des|=%.3f (wTow=%.3f)  dt=%.4f",
			dist, pivotDist, RAD2DEG(theta), vlen, v_tlen, vTow, wlen, wTow, safeDt);

		if (IPersistantDebug* pd = g_pGame->GetIGameFramework()->GetIPersistantDebug())
		{
			static const char* kTag = "CarryDebugWish";
			pd->Begin(kTag, /*clear*/ true);
			const float life = 0.15f;
			pd->AddSphere(p_t, 0.06f, ColorF(0, 1, 0, 1), life);
			pd->AddSphere(sp.pos, 0.06f, ColorF(1, 0, 0, 1), life);
			pd->AddLine(sp.pos, p_t, ColorF(1, 1, 0, 1), life);
		}
	}
}
