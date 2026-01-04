/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 09:01:2006   14:00 : Created by Michael Rauh

*************************************************************************/
#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CrySystem/IConsole.h"
#include "CryCommon/CryRenderer/IRenderer.h"
#include "CryCommon/Cry3DEngine/IFoliage.h"
#include "DebugGun.h"

#include "CryCommon/CryAction/IActorSystem.h"
#include "CryCommon/CryAction/IVehicleSystem.h"
#include "CryCommon/CryAction/IMovementController.h"
#include "CryGame/Actors/Actor.h"
#include "CryGame/Game.h"
#include "CryGame/GameCVars.h"
#include "CryGame/Actors/Player/Player.h"
#include "CryGame/GameRules.h"
#include "CryGame/HUD/HUD.h"

#include "CryMP/Client/Client.h"
#include "CryMP/Client/HandGripRegistry.h"

#define HIT_RANGE (2000.0f)
#define LINE_HEIGHT (8)

//------------------------------------------------------------------------
CDebugGun::CDebugGun()
{
	m_pAIDebugDraw = gEnv->pConsole->GetCVar("ai_DebugDraw");
	m_aiDebugDrawPrev = m_pAIDebugDraw->GetIVal();
	m_fireMode = 0;

	for (int i = 15; i >= 0; --i)
	{
		m_fireModes.push_back(TFmPair("pierceability", (float)i));
	}
}

//------------------------------------------------------------------------
void CDebugGun::OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if (actionId == "attack1")
	{
		if (activationMode == eAAM_OnPress) 
			Shoot(true);
	}
	else if (actionId == "zoom")
	{
		if (activationMode == eAAM_OnPress) 
			Shoot(false);
	}
	else if (actionId == "tweak_left")
	{
		if (activationMode == eAAM_OnPress) 
			ArmLeftCapture();
	}
	else if (actionId == "tweak_right")
	{
		if (activationMode == eAAM_OnPress) 
			ArmRightCapture();
	}
	else if (actionId == "tweak_up")
	{
		if (activationMode == eAAM_OnPress)
		{
			m_removeGripForEntity = true;
		}
	}
	else if (actionId == "tweak_down")
	{
		if (activationMode == eAAM_OnPress) 
			DumpGripListOnly(); // just the array lines
	}
	else if (actionId == "firemode")
	{
		++m_fireMode; 
		if (m_fireMode == m_fireModes.size())
			m_fireMode = 0;
	}
	else
	{
		CWeapon::OnAction(actorId, actionId, activationMode, value);
	}
}

//------------------------------------------------------------------------
void CDebugGun::LogMaterial(IMaterial* pMat, IRenderNode* pNode, int& x, int& y, float font, float* color, const char* label)
{
	if (!pMat)
		return;

	DrawLog(x, y, font, color, "[%s]", label);
	DrawLog(x, y, font, color, "Name: %s", pMat->GetName());

	if (ISurfaceType* pSurf = pMat->GetSurfaceType())
	{
		float surfaceColor[4] = { color[0], color[1], color[2], color[3] };
		if (pSurf->GetId() == m_selectedSurfaceIdx)
		{
			surfaceColor[0] = 1.0f;
			surfaceColor[1] = 0.5f;
			surfaceColor[2] = 0.0f;
			surfaceColor[3] = 1.0f;
		}

		const ISurfaceType::SPhysicalParams& phys = pSurf->GetPhyscalParams();
		DrawLog(x, y, font, surfaceColor, "Surface: %s", pSurf->GetName());
		DrawLog(x, y, font, surfaceColor, "Pierceability: %d", phys.pierceability);
	}

	y += LINE_HEIGHT;

	const int subCount = pMat->GetSubMtlCount();
	for (int i = 0; i < subCount; ++i)
	{
		if (IMaterial* pSub = pMat->GetSubMtl(i))
		{
			DrawLog(x, y, font, color, "Sub[%d]: %s", i, pSub->GetName());

			if (ISurfaceType* pSurf = pSub->GetSurfaceType())
			{
				float subColor[4] = { color[0], color[1], color[2], color[3] };
				if (pSurf->GetId() == m_selectedSurfaceIdx)
				{
					subColor[0] = 1.0f;
					subColor[1] = 0.5f;
					subColor[2] = 0.0f;
					subColor[3] = 1.0f;
				}

				const ISurfaceType::SPhysicalParams& phys = pSurf->GetPhyscalParams();
				DrawLog(x, y, font, subColor, "Surface: %s (Pcb:%d)", pSurf->GetName(), phys.pierceability);
			}
		}
	}

	if (pMat != m_pLastHighlightedMat)
	{
		DisableHighLighting();
		EnableHighlighting(pMat, pNode);
		m_pLastHighlightedMat = pMat;
	}
}

//------------------------------------------------------------------------
void CDebugGun::EnableHighlighting(IMaterial* pMat, IRenderNode* pNode)
{
	if (!pNode)
		return;

	IMaterial* pMaterial = pMat;
	if (!pMaterial)
		return;

	m_pLastHighlightedMat = pMaterial;
	m_lastHighlightedType = pNode->GetRenderNodeType();

	if (pNode->GetRenderNodeType() == eERType_Vegetation)
	{
		float opacity = 0.8f;
		pMaterial->SetGetMaterialParamFloat("opacity", opacity, false);

		const int subCount = pMaterial->GetSubMtlCount();
		for (int i = 0; i < subCount; ++i)
		{
			if (IMaterial* pSub = pMaterial->GetSubMtl(i))
			{
				pSub->SetGetMaterialParamFloat("opacity", opacity, false);
			}
		}
	}
	else
	{
		float glow = 0.6f;
		pMaterial->SetGetMaterialParamFloat("glow", glow, false);

		const int subCount = pMaterial->GetSubMtlCount();
		for (int i = 0; i < subCount; ++i)
		{
			if (IMaterial* pSub = pMaterial->GetSubMtl(i))
			{
				pSub->SetGetMaterialParamFloat("glow", glow, false);
			}
		}
	}
}

//------------------------------------------------------------------------
void CDebugGun::DisableHighLighting()
{
	if (!m_pLastHighlightedMat)
		return;

	if (m_lastHighlightedType == eERType_Vegetation)
	{
		float opacity = 1.0f;
		m_pLastHighlightedMat->SetGetMaterialParamFloat("opacity", opacity, false);

		const int subCount = m_pLastHighlightedMat->GetSubMtlCount();
		for (int i = 0; i < subCount; ++i)
		{
			if (IMaterial* pSub = m_pLastHighlightedMat->GetSubMtl(i))
			{
				pSub->SetGetMaterialParamFloat("opacity", opacity, false);
			}
		}
	}
	else
	{
		float glow = 0.f;
		m_pLastHighlightedMat->SetGetMaterialParamFloat("glow", glow, false);

		const int subCount = m_pLastHighlightedMat->GetSubMtlCount();
		for (int i = 0; i < subCount; ++i)
		{
			if (IMaterial* pSub = m_pLastHighlightedMat->GetSubMtl(i))
			{
				pSub->SetGetMaterialParamFloat("glow", glow, false);
			}
		}
	}
	m_pLastHighlightedMat = nullptr;
	m_lastHighlightedType = -1;
}

//------------------------------------------------------------------------
void CDebugGun::DrawBackgroundBox2D(float x, float y, float width, float height, float alpha)
{
	if (alpha <= 0.0f)
		return;

	IRenderer* pRenderer = gEnv->pRenderer;

	// Enable alpha blending
	pRenderer->SetState(GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA | GS_NODEPTHTEST);

	// Black color with specified alpha
	const float r = 0.f, g = 0.f, b = 0.f, a = alpha;

	// Use a white 1x1 texture or a dummy black texture (tex ID 0 is usually safe)
	int dummyTexId = 0;
	pRenderer->Draw2dImage(x, y, width, height, dummyTexId, 0, 0, 0, 0, 0, r, g, b, a);
}

//------------------------------------------------------------------------
void CDebugGun::DrawLog(int& x, int& y, float font, float* color, const char* fmt, ...)
{
	y += LINE_HEIGHT;

	va_list args;
	va_start(args, fmt);

	va_list argsCopy;
	va_copy(argsCopy, args);
	int len = std::vsnprintf(nullptr, 0, fmt, argsCopy);
	va_end(argsCopy);

	if (len <= 0)
	{
		va_end(args);
		return;
	}

	std::vector<char> buffer(len + 1);
	std::vsnprintf(buffer.data(), buffer.size(), fmt, args);
	va_end(args);

	SDrawTextInfo ti;
	ti.color[0] = color[0];
	ti.color[1] = color[1];
	ti.color[2] = color[2];
	ti.color[3] = color[3];
	ti.xscale = font;
	ti.yscale = font;

	const char* text = buffer.data();

	// If there's more, draw the rest on the next line
	const int maxLineLength = 40;
	// Draw first part
	std::string firstLine(text, MIN(maxLineLength, len));
	gEnv->pRenderer->Draw2dText((float)x, (float)y, firstLine.c_str(), ti);

	if (len > maxLineLength)
	{
		y += LINE_HEIGHT;
		const char* rest = text + maxLineLength;
		gEnv->pRenderer->Draw2dText((float)x, (float)y, rest, ti);
	}
}

//------------------------------------------------------------------------
void CDebugGun::Update(SEntityUpdateContext& ctx, int update)
{
}

//------------------------------------------------------------------------
void CDebugGun::Shoot(bool bPrimary)
{
	CWeapon::StartFire();

	ResetAnimation();

	// console cmd      
	string cmd;

	cmd = (bPrimary) ? g_pGameCVars->i_debuggun_1->GetString() : g_pGameCVars->i_debuggun_2->GetString();
	cmd += " ";

	unsigned int flags = rwi_stop_at_pierceable | rwi_colltype_any;

	if (m_fireModes[m_fireMode].first == "pierceability")
	{
		flags = (unsigned int)m_fireModes[m_fireMode].second & rwi_pierceability_mask;
	}

	IPhysicalWorld* pWorld = gEnv->pPhysicalWorld;
	IPhysicalEntity* pSkip = GetOwnerActor()->GetEntity()->GetPhysics();
	ray_hit rayhit;
	int hits = 0;

	CCamera& cam = GetISystem()->GetViewCamera();
	Vec3 pos = cam.GetPosition() + cam.GetViewdir();
	Vec3 dir = cam.GetViewdir() * HIT_RANGE;

	IEntity* pEntity = 0;

	hits = pWorld->RayWorldIntersection(pos, dir, ent_all, flags, &rayhit, 1, &pSkip, 1);
	if (hits)
	{
		gEnv->p3DEngine->RefineRayHit(&rayhit, cam.GetViewdir() * 50.f);

		pEntity = (IEntity*)rayhit.pCollider->GetForeignData(PHYS_FOREIGN_ID_ENTITY);

		if (pEntity)
		{
			// capture if armed (shows sphere)
			CaptureIfArmed(pEntity, rayhit);
		}

		IRenderNode* pNode = GetRenderNodeFromCollider(rayhit.pCollider);
		if (pNode)
		{
			const char* pClassName = pNode->GetEntityClassName();
			CryLogAlways("$3RenderNode: %s (%s)",
				pNode->GetName() ? pNode->GetName() : "<no name>",
				pClassName ? pClassName : "<unknown class>");

			IMaterial* pMat = pNode->GetMaterial();
			if (pMat)
			{
				const char* mtlName = pMat->GetName();
				const char* shaderName = pMat->GetShaderItem().m_pShader ? pMat->GetShaderItem().m_pShader->GetName() : "<no shader>";

				CryLogAlways("$3Material: %s (Shader: %s)", mtlName ? mtlName : "<null>", shaderName);

				const int subCount = pMat->GetSubMtlCount();
				for (int i = 0; i < subCount; ++i)
				{
					IMaterial* pSub = pMat->GetSubMtl(i);
					if (pSub)
					{
						const char* subName = pSub->GetName();
						const char* subShader = pSub->GetShaderItem().m_pShader ? pSub->GetShaderItem().m_pShader->GetName() : "<no shader>";
						CryLogAlways("	$3Submaterial %d: %s (Shader: %s)", i, subName ? subName : "<null>", subShader);
					}
				}
			}

		}
	}

	cmd.append(pEntity ? pEntity->GetName() : "0");

	// if we execute an AI command take care of ai_debugdraw
	if (cmd.substr(0, 3) == "ai_")
	{
		if (pEntity && m_pAIDebugDraw->GetIVal() == 0)
			m_pAIDebugDraw->Set(1);
		else if (!pEntity && m_aiDebugDrawPrev == 0 && m_pAIDebugDraw->GetIVal() == 1)
			m_pAIDebugDraw->Set(0);
	}

	gEnv->pConsole->ExecuteString(cmd.c_str());

	// if 2nd button hits a vehicle, enable movement profiling  
	if (!bPrimary)
	{
		static IVehicleSystem* pVehicleSystem = g_pGame->GetIGameFramework()->GetIVehicleSystem();

		string vehicleCmd = "v_debugVehicle ";
		vehicleCmd.append((pEntity && pVehicleSystem->GetVehicle(pEntity->GetId())) ? pEntity->GetName() : "0");

		gEnv->pConsole->ExecuteString(vehicleCmd.c_str());
	}

	OnShoot(GetOwnerId(), 0, 0, pos, dir, Vec3(ZERO));
}

//------------------------------------------------------------------------
void CDebugGun::Select(bool select)
{
	CWeapon::Select(select);

	// save ai_debugDraw val
	if (select)
	{
		m_aiDebugDrawPrev = m_pAIDebugDraw->GetIVal();
	}

	CActor* pOwner = GetOwnerActor();
	if (pOwner && pOwner->IsClient())
	{
		if (select)
		{
			GetGameObject()->EnablePostUpdates(this);
		}
		else
		{
			DisableHighLighting();
			GetGameObject()->DisablePostUpdates(this);
		}
	}
}

//------------------------------------------------------------------------
IRenderNode* CDebugGun::GetRenderNodeFromCollider(IPhysicalEntity* pCollider)
{
	if (!pCollider)
		return nullptr;

	pe_params_foreign_data fd;
	if (!pCollider->GetParams(&fd))
		return nullptr;

	switch (fd.iForeignData)
	{
	case PHYS_FOREIGN_ID_ENTITY:
	{
		IEntity* pEntity = static_cast<IEntity*>(pCollider->GetForeignData(PHYS_FOREIGN_ID_ENTITY));
		if (pEntity)
		{
			IEntityRenderProxy* pRenderProxy = static_cast<IEntityRenderProxy*>(pEntity->GetProxy(ENTITY_PROXY_RENDER));
			if (pRenderProxy)
			{
				return pRenderProxy->GetRenderNode();
			}
		}
		break;
	}
	case PHYS_FOREIGN_ID_STATIC:
	case PHYS_FOREIGN_ID_TERRAIN:
	{
		return static_cast<IRenderNode*>(pCollider->GetForeignData(fd.iForeignData));
	}
	case PHYS_FOREIGN_ID_FOLIAGE:
	{
		IFoliage* pFoliage = static_cast<IFoliage*>(fd.pForeignData);
		if (pFoliage)
		{
			return pFoliage->GetIRenderNode();  // Safely get parent render node
		}
		break;
	}
	default:
		// Unknown or unsupported foreign type
		break;
	}

	return nullptr;
}

//------------------------------------------------------------------------
void CDebugGun::PostUpdate(float frameTime)
{
	if (!IsSelected() || !IsClient())
		return;

	IRenderer* pRenderer = gEnv->pRenderer;

	const int screenWidth = gEnv->pRenderer->GetWidth();
	const int screenHeight = gEnv->pRenderer->GetHeight();

	float font = 0.6f;
	if (screenWidth < 1280 || screenHeight < 720)
	{
		font = 0.7f; // Slightly larger font for low resolution
	}

	const int space = 145;
	const int height = 50;
	int xColumn1 = 10;
	int yColumn1 = height;

	int xColumn2 = xColumn1 + space;
	int yColumn2 = height;

	int xColumn3 = xColumn2 + space;
	int yColumn3 = height;

	int xColumn4 = xColumn3 + space;
	int yColumn4 = height;

	int xColumn5 = xColumn4 + space;
	int yColumn5 = height;

	// Networking placed below Entity
	int xNet = 40;
	int yNet = yColumn1 + 200;

	float colorYellow[4] = { 1, 1, 0, 1 };     // yellow
	float colorCyan[4] = { 0, 1, 1, 1 };    // cyan
	float colorOrange[4] = { 1, 0.5f, 0, 1 };// orange
	float colorWhite[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	float colorLightRed[4] = { 1.0f, 0.85f, 0.75f, 1.0f };
	float colorLightBlue[4] = { 0.6f, 0.8f, 1, 1 }; // light blue
	float colorGreen[4] = { 0.4f, 1, 0.4f, 1 };  // greenish

	CCamera& cam = gEnv->pSystem->GetViewCamera();
	ray_hit rayhit;
	//const int rwiFlags = (geom_colltype_ray << rwi_colltype_bit) | rwi_colltype_any | (10 & rwi_pierceability_mask) | (geom_colltype14 << rwi_colltype_bit);
	const int rwiFlags = rwi_stop_at_pierceable | rwi_colltype_any;

	const int hits = gEnv->pPhysicalWorld->RayWorldIntersection(
		cam.GetPosition() + cam.GetViewdir(), cam.GetViewdir() * 500.f, ent_all,
		rwiFlags, &rayhit, 1);

	gEnv->p3DEngine->RefineRayHit(&rayhit, cam.GetViewdir() * 50.f);

	if (!hits) 
	{
		DisableHighLighting();
		return;
	}

	IPhysicalEntity* pPhysicalEnt = rayhit.pCollider;
	IEntity *pEntity = pPhysicalEnt ? m_pEntitySystem->GetEntityFromPhysics(pPhysicalEnt) : nullptr;

	if (g_pGameCVars->i_debug_entity > 0)
	{
		if (IEntity* pSelectEntity = m_pEntitySystem->GetEntity(g_pGameCVars->i_debug_entity))
		{
			pEntity = pSelectEntity;
			pPhysicalEnt = pSelectEntity->GetPhysics();
		}
		else
		{
			g_pGameCVars->i_debug_entity = 0; // Reset if entity not found
			CryLogAlways("$3DebugGun: Entity with ID %d not found. Resetting i_debug_entity.");
		}
	}

	const EntityId entityId = pEntity ? pEntity->GetId() : 0;

	IRenderNode* pNode = GetRenderNodeFromCollider(pPhysicalEnt);

	IMaterial *pMaterialToLog = nullptr;
	const char* materialDescription = "";

	// === IEntity Info ===
	DrawLog(xColumn1, yColumn1, font, colorYellow, "[Entity]");
	if (pEntity)
	{
		DebugDrawHandGripsForEntity(pEntity, 0.2f);

		if (m_armLeft || m_armRight)
		{
			PlaceTempSphere(rayhit.pt, m_armRight, 0.2f); // debug sphere at hit point
		}

		if (m_removeGripForEntity)
		{
			if (gClient->GetHandGripRegistry()->RemoveGripForEntity(pEntity))
			{
				if (CHUD* pHUD = g_pGame->GetHUD())
					pHUD->DisplayBigOverlayFlashMessage("Cleared Hand Grip data for selected entity", 2.0f, 400, 400, Col_Goldenrod);
			}
			else
			{
				if (CHUD* pHUD = g_pGame->GetHUD())
					pHUD->DisplayBigOverlayFlashMessage("No Hand Grip data found for selected entity!", 2.0f, 400, 400, Col_Goldenrod);
			}
			m_removeGripForEntity = false;
		}

		DrawLog(xColumn1, yColumn1, font, colorYellow, "Name: %s", pEntity->GetName());
		DrawLog(xColumn1, yColumn1, font, colorYellow, "Class: %s", pEntity->GetClass()->GetName());
		DrawLog(xColumn1, yColumn1, font, colorYellow, "Id: %u", entityId);

		if (IVehicle* pVehicle = gEnv->pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(entityId))
		{
			DrawLog(xColumn1, yColumn1, font, colorYellow, "[Vehicle]");
			DrawLog(xColumn1, yColumn1, font, colorYellow, "Passengers: %d", pVehicle->GetStatus().passengerCount);
			int compCount = pVehicle->GetComponentCount();
			for (int i = 0; i < compCount; ++i)
			{
				IVehicleComponent* pComp = pVehicle->GetComponent(i);
				if (pComp)
				{
					DrawLog(xColumn1, yColumn1, font, colorYellow, "Component[%d]: %s (%.2f)",
						i, pComp->GetComponentName(), pComp->GetDamageRatio());
				}
			}
		}
		if (CActor* pActor = static_cast<CActor*>(gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(entityId)))
		{
			DrawLog(xColumn1, yColumn1, font, colorYellow, "[Actor]");
			DrawLog(xColumn1, yColumn1, font, colorYellow, "Health: %d", pActor->GetHealth());

			const EStance stance = pActor->GetStance();

			const char* stanceStr = "STANCE_UNKNOWN";
			switch (stance)
			{
			case STANCE_NULL:    stanceStr = "STANCE_NULL";    break;
			case STANCE_STAND:   stanceStr = "STANCE_STAND";   break;
			case STANCE_CROUCH:  stanceStr = "STANCE_CROUCH";  break;
			case STANCE_PRONE:   stanceStr = "STANCE_PRONE";   break;
			case STANCE_RELAXED: stanceStr = "STANCE_RELAXED"; break;
			case STANCE_STEALTH: stanceStr = "STANCE_STEALTH"; break;
			case STANCE_SWIM:    stanceStr = "STANCE_SWIM";    break;
			case STANCE_ZEROG:   stanceStr = "STANCE_ZEROG";   break;
			default: break;
			}

			DrawLog(
				xColumn1,
				yColumn1,
				font,
				colorYellow,
				"Stance: %s (%d)",
				stanceStr,
				(int)stance
			);
		}

		if (IMaterial* pMat = pEntity->GetMaterial())
		{
			pMaterialToLog = pMat;
			materialDescription = "Entity Material";
		}

		// --- Children ---
		const int childCount = pEntity->GetChildCount();
		if (childCount)
		{
			DrawLog(xColumn1, yColumn1, font, colorYellow, "Children: %d", childCount);

			for (int i = 0; i < childCount; ++i)
			{
				if (IEntity* pChild = pEntity->GetChild(i))
				{
					const char* childName = pChild->GetName();
					const char* childClass = pChild->GetClass() ? pChild->GetClass()->GetName() : "<no class>";
					EntityId    childId = pChild->GetId();

					DrawLog(xColumn1, yColumn1, font, colorYellow,
						"  [%d] %s  id=%u  class=%s",
						i, childName, childId, childClass);
				}
			}
		}

		if (IEntity* pParent = pEntity->GetParent())
		{
			const char* parentClass = pParent->GetClass() ? pParent->GetClass()->GetName() : "<no class>";
			DrawLog(xColumn1, yColumn1, font, colorYellow,
				"Parent: %s  id=%u  class=%s",
				pParent->GetName(), pParent->GetId(), parentClass);
		}

		{
			int linkCount = 0;
			for (IEntityLink* pLink = pEntity->GetEntityLinks(); pLink; pLink = pLink->next)
				++linkCount;

			if (linkCount)
			{
				DrawLog(xColumn1, yColumn1, font, colorYellow, "Links (out): %d", linkCount);

				for (IEntityLink* pLink = pEntity->GetEntityLinks(); pLink; pLink = pLink->next)
				{
					const EntityId targetId = pLink->entityId;
					const char* linkName = pLink->name ? pLink->name : "<no name>";
					const IEntity* pTarget = gEnv->pEntitySystem->GetEntity(targetId);
					const char* targetName = pTarget ? pTarget->GetName() : "<missing>";
					const char* targetClass = (pTarget && pTarget->GetClass()) ? pTarget->GetClass()->GetName() : "<no class>";

					DrawLog(xColumn1, yColumn1, font, colorYellow,
						"  %s -> %s id=%u class=%s", linkName, targetName, targetId, targetClass);
				}
			}
		}

		const int slotCount = pEntity->GetSlotCount();
		int totalSubs = 0;
		for (int s = 0; s < slotCount; ++s)
		{
			if (IStatObj* so = pEntity->GetStatObj(s))
			{
				const int nSubs = so->GetSubObjectCount();
				if (nSubs > 0)
				{
					DrawLog(xColumn1, yColumn1, font, colorYellow, "Slot %d SubObjects: %d", s, nSubs);
					for (int i = 0; i < nSubs; ++i)
					{
						if (IStatObj::SSubObject* sub = so->GetSubObject(i))
						{
							const char* n = sub->name.c_str();
							DrawLog(xColumn1, yColumn1, font, colorYellow, "  [%d] %s", i, n);
							++totalSubs;
						}
					}
				}
			}
		}
	}

	// === Physics Info ===
	DrawLog(xColumn2, yColumn2, font, colorCyan, "[Physics]");
	if (IPhysicalEntity* pPhys = pPhysicalEnt)
	{
		DrawLog(xColumn2, yColumn2, font, colorCyan, "PhysHandle: %p", pPhys);

		DrawLog(xColumn2, yColumn2, font, colorCyan, "PartId: %d", rayhit.partid);

		const float distance = (rayhit.pt - cam.GetPosition()).GetLength();
		DrawLog(xColumn2, yColumn2, font, colorCyan, "Distance: %.2f", distance);

		const int peType = pPhys->GetType();
		//pe_type
		{
			const auto PeTypeToString = [](int t) -> const char*
				{
					switch (t)
					{
					case PE_NONE:           return "PE_NONE";
					case PE_STATIC:         return "PE_STATIC";
					case PE_RIGID:          return "PE_RIGID";
					case PE_WHEELEDVEHICLE: return "PE_WHEELEDVEHICLE";
					case PE_LIVING:         return "PE_LIVING";
					case PE_PARTICLE:       return "PE_PARTICLE";
					case PE_ARTICULATED:    return "PE_ARTICULATED";
					case PE_ROPE:           return "PE_ROPE";
					case PE_SOFT:           return "PE_SOFT";
					case PE_AREA:           return "PE_AREA";
					default:                return "UNKNOWN";
					}
				};

			DrawLog(xColumn2, yColumn2, font, colorCyan,
				"Type: %s (%d)", PeTypeToString(peType), peType);
		}

		pe_status_dynamics dyn;
		if (pPhys->GetStatus(&dyn))
		{
			DrawLog(xColumn2, yColumn2, font, colorCyan, "Mass: %.1f", dyn.mass);
			DrawLog(xColumn2, yColumn2, font, colorCyan, "Velocity: %.2f", dyn.v.len());
			DrawLog(xColumn2, yColumn2, font, colorCyan, "Submerged: %.2f", dyn.submergedFraction);
		}

		pe_status_pos status;
		if (pPhys->GetStatus(&status))
		{
			int simClass = status.iSimClass;
			const char* simClassStr = "Unknown";

			switch (simClass)
			{
			case SC_STATIC:          simClassStr = "SC_STATIC"; break;
			case SC_SLEEPING_RIGID:  simClassStr = "SC_SLEEPING_RIGID"; break;
			case SC_ACTIVE_RIGID:    simClassStr = "SC_ACTIVE_RIGID"; break;
			case SC_LIVING:          simClassStr = "SC_LIVING"; break;
			case SC_TRIGGER:         simClassStr = "SC_TRIGGER"; break;
			case SC_INDEPENDENT:     simClassStr = "SC_INDEPENDENT"; break;
			case SC_DELETED:         simClassStr = "SC_DELETED"; break;
			}
			DrawLog(xColumn2, yColumn2, font, colorCyan, "Sim Class: %s", simClassStr);

			//Check for discrepancy between physics and entity position
			if (pEntity)
			{
				const Vec3 physPos = status.pos;
				const Vec3 entPos = pEntity->GetWorldPos();
				const float delta = (physPos - entPos).len();

				const float eps = 1.0f;

				if (delta > eps)
				{
					DrawLog(xColumn2, yColumn2, font, colorYellow,
						"Phys!=Entity: %.3f | Phys(%.2f, %.2f, %.2f) vs Ent(%.2f, %.2f, %.2f)",
						delta, physPos.x, physPos.y, physPos.z, entPos.x, entPos.y, entPos.z);
				}
			}
		}

		pe_params_timeout timeoutParams;
		if (pPhys->GetParams(&timeoutParams))
		{
			if (timeoutParams.timeIdle > 0.0f)
				DrawLog(xColumn2, yColumn2, font, colorCyan, "TimeIdle = %.3f", timeoutParams.timeIdle);

			if (timeoutParams.maxTimeIdle > 0.0f)
				DrawLog(xColumn2, yColumn2, font, colorCyan, "MaxTimeIdle = %.3f", timeoutParams.maxTimeIdle);
		}

		// --- Physics Part Info ---
		pe_status_nparts snp;
		const int nParts = pPhys->GetStatus(&snp); 
		DrawLog(xColumn2, yColumn2, font, colorCyan, "Parts: %d", nParts);

		for (int i = 0; i < nParts; ++i)
		{
			pe_params_part pp;
			pp.ipart = i;
			if (!pPhys->GetParams(&pp))
				continue;

			const phys_geometry* pPG = pp.pPhysGeomProxy ? pp.pPhysGeomProxy : pp.pPhysGeom;
			const char* label = pp.pPhysGeomProxy ? "proxy" : "geom";

			const int gtype = (pPG && pPG->pGeom) ? pPG->pGeom->GetType() : -1;
			const char* typeStr = "unknown";

			switch (gtype)
			{
			case GEOM_TRIMESH:     typeStr = "GEOM_TRIMESH";     break;
			case GEOM_HEIGHTFIELD: typeStr = "GEOM_HEIGHTFIELD"; break;
			case GEOM_CYLINDER:    typeStr = "GEOM_CYLINDER";    break;
			case GEOM_CAPSULE:     typeStr = "GEOM_CAPSULE";     break;
			case GEOM_RAY:         typeStr = "GEOM_RAY";         break;
			case GEOM_SPHERE:      typeStr = "GEOM_SPHERE";      break;
			case GEOM_BOX:         typeStr = "GEOM_BOX";         break;
			case GEOM_VOXELGRID:   typeStr = "GEOM_VOXELGRID";   break;
			default:               typeStr = "unknown";     break;
			}

			DrawLog(xColumn2, yColumn2, font, i == rayhit.partid ? colorYellow : colorCyan, "  Part %d: %s (%s)", i, label, typeStr);

			// --- Structural Joints
			if (i == rayhit.partid)
			{
				pe_params_structural_joint pj;
				pj.idx = rayhit.partid;
				if (pPhys->GetParams(&pj))
				{
					if (pj.bBreakable || pj.bBroken)
					{
						DrawLog(xColumn2, yColumn2, font, colorYellow, "	%s%s", pj.bBreakable ? "Breakable " : "", pj.bBroken ? "-> Broken" : "");
					}
				}
			}
		}
	}

	// === Material & Surface Info ===
	DrawLog(xColumn3, yColumn3, font, colorOrange, "[SurfaceType]");
	ISurfaceType* pSurface = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceType(rayhit.surface_idx);

	m_selectedSurfaceIdx = rayhit.surface_idx;

	if (pSurface)
	{
		DrawLog(xColumn3, yColumn3, font, colorOrange, "Name: %s", pSurface->GetName());
		if (pSurface->GetBreakability())
		{
			DrawLog(xColumn3, yColumn3, font, colorOrange, "Break Energy: %.2f", pSurface->GetBreakEnergy());
		}
		const ISurfaceType::SPhysicalParams& phys = pSurface->GetPhyscalParams();
		DrawLog(xColumn3, yColumn3, font, colorOrange, "Hit Points: %.2f", phys.hit_points);
		DrawLog(xColumn3, yColumn3, font, colorOrange, "Pierceability: %d", phys.pierceability);
		DrawLog(xColumn3, yColumn3, font, colorOrange, "Hit Radius: %.2f", phys.hit_radius);
	}

	// === Render Object Info (StatObj) ===
	yColumn2 += LINE_HEIGHT;

	DrawLog(xColumn2, yColumn2, font, colorLightRed, "[RenderNode]");

	if (pNode)
	{
		DrawLog(xColumn2, yColumn2, font, colorLightRed, "Class: %s", pNode->GetEntityClassName());
		DrawLog(xColumn2, yColumn2, font, colorLightRed, "Type: %d", pNode->GetRenderNodeType());
		DrawLog(xColumn2, yColumn2, font, colorLightRed, "ViewDistance: %d", pNode->GetViewDistRatio());
		if (pNode->GetFoliage())
		{
			DrawLog(xColumn2, yColumn2, font, colorLightRed, "Foliage");
		}

		if (IStatObj* pObj = pNode->GetEntityStatObj(0))
		{
			DrawLog(xColumn2, yColumn2, font, colorLightRed, "File: %s", pObj->GetFilePath());
			if (IRenderMesh* pMesh = pObj->GetRenderMesh())
			{
				DrawLog(xColumn2, yColumn2, font, colorLightRed, "Tris: %d", pMesh->GetSysIndicesCount() / 3);
			}

			if (IMaterial* pMat = pNode->GetMaterial())
			{
				if (!pMaterialToLog)
				{
					pMaterialToLog = pMat;
					materialDescription = "StatObj Material";
				}
			}
		}
	}

	if (pEntity)
	{
		if (ICharacterInstance* pChar = pEntity->GetCharacter(0))
		{
			DrawLog(xColumn5, yColumn5, font, colorLightBlue, "[Character]");
			DrawLog(xColumn5, yColumn5, font, colorLightBlue, "Skeleton: %s", pChar->GetFilePath());

			yColumn5 += LINE_HEIGHT;

			// Attachments
			IAttachmentManager* pAttachMgr = pChar->GetIAttachmentManager();
			int attachCount = pAttachMgr ? pAttachMgr->GetAttachmentCount() : 0;
			DrawLog(xColumn5, yColumn5, font, colorLightBlue, "Attachments: %d", attachCount);

			for (int i = 0; i < attachCount; ++i)
			{
				if (IAttachment* pAttach = pAttachMgr->GetInterfaceByIndex(i))
				{
					const char* typeStr = "Unknown";
					switch (pAttach->GetType())
					{
					case CA_BONE: typeStr = "Bone"; break;
					case CA_FACE: typeStr = "Face"; break;
					case CA_SKIN: typeStr = "Skin"; break;
					}

					DrawLog(xColumn5, yColumn5, font, colorLightBlue, "Attach[%d]: %s:%s", i, pAttach->GetName(), typeStr);
				}
			}

			// Skeleton info
			ISkeletonPose* pPose = pChar->GetISkeletonPose();
			if (pPose)
			{
				yColumn5 += LINE_HEIGHT;

				const int jointCount = pPose->GetJointCount();
				DrawLog(xColumn5, yColumn5, font, colorLightBlue, "Joints: %d", jointCount);
			}

			yColumn5 += LINE_HEIGHT;

			ISkeletonAnim* pSkelAnim = pChar->GetISkeletonAnim();
			IAnimationSet* pAnimSet = pChar->GetIAnimationSet();

			if (pSkelAnim && pAnimSet)
			{
				const int maxLayers = 4;
				for (int layer = 0; layer < maxLayers; ++layer)
				{
					const int animCount = pSkelAnim->GetNumAnimsInFIFO(layer);
					if (animCount == 0)
						continue;

					DrawLog(xColumn5, yColumn5, font, colorLightBlue, "Layer %d: %d anim(s)", layer, animCount);

					for (int i = 0; i < animCount; ++i)
					{
						const CAnimation& anim = pSkelAnim->GetAnimFromFIFO(layer, i);
						if (!anim.m_bActivated)
							continue;

						DrawLog(xColumn5, yColumn5, font, colorLightBlue, "Anim[%d]:", i);
						DrawLog(xColumn5, yColumn5, font, colorLightBlue, "Playback Time: %.2f", anim.m_fAnimTime);
						DrawLog(xColumn5, yColumn5, font, colorLightBlue, "Weight: %.2f", anim.m_fIWeight);
						DrawLog(xColumn5, yColumn5, font, colorLightBlue, "Speed: %.2f", anim.m_fCurrentPlaybackSpeed);
						DrawLog(xColumn5, yColumn5, font, colorLightBlue, "Duration: %.2f", anim.m_fCurrentDuration);
						DrawLog(xColumn5, yColumn5, font, colorLightBlue, "Transition Weight: %.2f", anim.m_fTransitionWeight);
						DrawLog(xColumn5, yColumn5, font, colorLightBlue, "Priority: %.2f", anim.m_fTransitionPriority);
						DrawLog(xColumn5, yColumn5, font, colorLightBlue, "Loops: %d", anim.m_nLoopCount);

						// Dump names from locomotion group if available
						const SLocoGroup& group = anim.m_LMG0;
						if (group.m_numAnims > 0)
						{
							for (int j = 0; j < group.m_numAnims; ++j)
							{
								const int animID = group.m_nAnimID[j];
								const char* animName = animID >= 0 ? pAnimSet->GetNameByAnimID(animID) : nullptr;
								if (!animName) animName = "<unknown>";

								DrawLog(xColumn5, yColumn5, font, colorLightBlue, "[%d] %s", j, animName);
								DrawLog(xColumn5, yColumn5, font, colorLightBlue, "Duration: %.2f", group.m_fDurationQQQ[j]);
							}
						}

						// Aim poses
						if (anim.m_strAimPosName0 || anim.m_strAimPosName1)
						{
							DrawLog(xColumn5, yColumn5, font, colorLightBlue, "Aim0: %s", anim.m_strAimPosName0 ? anim.m_strAimPosName0 : "-");
							DrawLog(xColumn5, yColumn5, font, colorLightBlue, "Aim1: %s", anim.m_strAimPosName1 ? anim.m_strAimPosName1 : "-");
						}

						// Flags
						if (anim.m_bEndOfCycle)
							DrawLog(xColumn5, yColumn5, font, colorLightRed, "[EndOfCycle]");
						if (anim.m_bTWFlag)
							DrawLog(xColumn5, yColumn5, font, colorLightRed, "[TWFlag]");

						yColumn5 += LINE_HEIGHT;
					}
				}
			}


			// Character material
			if (IMaterial* pCharMat = pChar->GetMaterial())
			{
				if (!pMaterialToLog)
				{
					pMaterialToLog = pCharMat;
					materialDescription = "Character Material";
				}
			}
		}

		else if (IStatObj* pStatObj = pEntity->GetStatObj(0))
		{
			if (IMaterial* pMat = pStatObj->GetMaterial())
			{
				if (!pMaterialToLog)
				{
					pMaterialToLog = pMat;
					materialDescription = "Entity StatObj Material";
				}
			}
		}
	}

	// === Networking / MP ===
	if (gEnv->bMultiplayer && g_pGame->GetGameRules())
	{
		yColumn1 += LINE_HEIGHT;
		DrawLog(xColumn1, yColumn1, font, colorGreen, "[Networking]");
		if (pEntity)
		{
			INetContext* pNetCtx = gEnv->pGame->GetIGameFramework()->GetNetContext();
			CGameRules* pRules = g_pGame->GetGameRules();
			if (pNetCtx)
				DrawLog(xColumn1, yColumn1, font, colorGreen, "NetBound: %s", pNetCtx->IsBound(entityId) ? "Yes" : "No");
			if (pRules)
				DrawLog(xColumn1, yColumn1, font, colorGreen, "Team: %d", pRules->GetTeam(entityId));
		}
	}
	/*
	float offset = -70.f;
	DrawBackgroundBox2D((float)xColumn1, 90.0f, 120.0f, yColumn1 + offset, 0.4f);
	DrawBackgroundBox2D((float)xColumn2, 90.0f, 120.0f, yColumn2 + offset, 0.4f);
	DrawBackgroundBox2D((float)xColumn3, 90.0f, 120.0f, yColumn3 + offset, 0.4f);
	DrawBackgroundBox2D((float)xColumn4, 90.0f, 120.0f, yColumn4 + offset, 0.4f);
	DrawBackgroundBox2D((float)xColumn5, 90.0f, 120.0f, yColumn5 + offset, 0.4f);
	*/
	//Turn off material glow if no materials logged
	if (!pMaterialToLog)
	{
		DisableHighLighting();
	}
	else
	{
		LogMaterial(pMaterialToLog, pNode, xColumn4, yColumn4, font, colorWhite, materialDescription);
	}

	CWeapon::PostUpdate(frameTime);
}

// ==================== CDebugGun capture implementation ====================

void CDebugGun::ArmLeftCapture() 
{ 
	m_armLeft = true;  
	m_armRight = false;
	CryLogAlways("$3[DebugGun] Armed LEFT"); 

	if (CHUD *pHUD = g_pGame->GetHUD())
		pHUD->DisplayBigOverlayFlashMessage("[LEFT - HAND GRIP] Shoot an object to save hand grip position", 2.0f, 400, 400, Col_Goldenrod);
}

void CDebugGun::ArmRightCapture() 
{ 
	m_armRight = true;
	m_armLeft = false; 
	CryLogAlways("$3[DebugGun] Armed RIGHT");

	if (CHUD* pHUD = g_pGame->GetHUD())
		pHUD->DisplayBigOverlayFlashMessage("[RIGHT - HAND GRIP] Shoot an object to save hand grip position", 2.0f, 400, 400, Col_Goldenrod);
}

// Robust sphere (PersistantDebug + AuxGeom fallback)
void CDebugGun::PlaceTempSphere(const Vec3& worldPos, bool isRight, float lifeSec) const
{
	if (IPersistantDebug* pd = g_pGame->GetIGameFramework()->GetIPersistantDebug())
	{
		static const char* kTag = "GripCapture";
		// DO NOT clear (false) so other systems don't wipe our spheres immediately
		pd->Begin(kTag, /*clear=*/false);
		const ColorF colL(1.f, 0.9f, 0.0f, 1.f); // yellow
		const ColorF colR(0.2f, 0.6f, 1.0f, 1.f); // blue
		pd->AddSphere(worldPos, 0.03f, isRight ? colR : colL, lifeSec <= 0.f ? 15.0f : lifeSec);
	}
}

// ---------- capture on shot ----------
void CDebugGun::CaptureIfArmed(IEntity* pEntity, const ray_hit& rayhit)
{
	if (!pEntity)
		return;

	if (!m_armLeft && !m_armRight)
		return;

	// Build key: prefer CGF path from slot 0. If missing, use entity class name.
	const char* keyCStr = nullptr;

	IStatObj* so = pEntity->GetStatObj(0);
	if (so)
	{
		const char* p = so->GetFilePath();
		if (p && *p)
		{
			keyCStr = p;
		}
	}

	if (!keyCStr)
	{
		const IEntityClass* c = pEntity->GetClass();
		if (c)
		{
			const char* n = c->GetName();
			if (n && *n)
			{
				keyCStr = n;
			}
		}
	}

	if (!keyCStr)
	{
		CryLogAlways("$4[DebugGun] No model/class; capture ignored.");
		return;
	}

	const Matrix34 inv = pEntity->GetWorldTM().GetInverted();
	const Vec3     hitEL = inv.TransformPoint(rayhit.pt);

	if (m_armLeft)
	{
		gClient->GetHandGripRegistry()->SetGripLeft(keyCStr, hitEL);
		m_armLeft = false;

		PlaceTempSphere(rayhit.pt, /*isRight=*/false, 18.0f);
		CryLogAlways("$3[DebugGun] LEFT saved '%s'  EL=(%.3f, %.3f, %.3f)", keyCStr, hitEL.x, hitEL.y, hitEL.z);

		if (CHUD* pHUD = g_pGame->GetHUD())
			pHUD->DisplayBigOverlayFlashMessage("[LEFT - HAND GRIP] Position saved", 2.0f, 400, 400, Col_Goldenrod);
	}
	else if (m_armRight)
	{
		gClient->GetHandGripRegistry()->SetGripRight(keyCStr, hitEL);
		m_armRight = false;

		PlaceTempSphere(rayhit.pt, /*isRight=*/true, 18.0f);

		if (CHUD* pHUD = g_pGame->GetHUD())
			pHUD->DisplayBigOverlayFlashMessage("[RIGHT - HAND GRIP] Position saved", 2.0f, 400, 400, Col_Goldenrod);
	}
}

void CDebugGun::DebugDrawHandGripsForEntity(IEntity* pEntity, float lifeSec)
{
	if (!pEntity)
		return;

	// Lookup grip info.
	const HandGripInfo* info = gClient->GetHandGripRegistry()->GetGripByEntity(pEntity);
	if (!info)
		return;

	IPersistantDebug* pd = g_pGame->GetIGameFramework()->GetIPersistantDebug();
	if (!pd)
		return;

	const Matrix34 entW = pEntity->GetWorldTM();

	const ColorF colL(1.0f, 0.9f, 0.0f, 1.0f);  // yellow (left)
	const ColorF colR(0.2f, 0.6f, 1.0f, 1.0f);  // blue   (right)
	const float  radius = 0.08f;

	// Use a stable tag; we clear each frame so the spheres don't accumulate.
	const char* kTag = "GripPreview";
	pd->Begin(kTag, /*clear=*/true);

	if (info->hasLeft)
	{
		const Vec3 ws = entW.TransformPoint(info->leftEL);
		pd->AddSphere(ws, radius, colL, lifeSec);
	}

	if (info->hasRight)
	{
		const Vec3 ws = entW.TransformPoint(info->rightEL);
		pd->AddSphere(ws, radius, colR, lifeSec);
	}
}

void CDebugGun::DumpGripListOnly() const
{
	std::string script = gClient->GetHandGripRegistry()->SerializeToLuaScript();

	CryLogAlways("%s", script.c_str());

	if (CHUD* pHUD = g_pGame->GetHUD())
	{
		pHUD->DisplayBigOverlayFlashMessage("Dumped HandGrip data into console", 2.0f, 400, 400, Col_Goldenrod);
	}
}

