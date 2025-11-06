#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CryPhysics/IPhysics.h"
#include "CryCommon/Cry3DEngine/I3DEngine.h"
#include "CryCommon/Cry3DEngine/IFoliage.h"
#include "CryCommon/CryEntitySystem/IEntitySystem.h"

#include "ScriptBind_Physics.h"

ScriptBind_Physics::ScriptBind_Physics(IScriptSystem *pSS)
{
	CScriptableBase::Init(pSS, gEnv->pSystem);
	SetGlobalName("Physics");

#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &ScriptBind_Physics::

	SCRIPT_REG_TEMPLFUNC(SimulateExplosion, "tableExplosion");
	SCRIPT_REG_TEMPLFUNC(RegisterExplosionShape, "sGeometryFile, fSize, nIdMaterial, fProbability, sSplintersFile, fSplintersOffset, sSplintersCloudEffect");
	SCRIPT_REG_TEMPLFUNC(RegisterExplosionCrack, "sGeometryFile, nIdMaterial");
	SCRIPT_REG_FUNC(RayWorldIntersection);
	SCRIPT_REG_TEMPLFUNC(RayTraceCheck, "vSrc, vTrg, hSkipEntityId1, hSkipEntityId2");
	SCRIPT_REG_TEMPLFUNC(SamplePhysEnvironment, "vPoint, fRadius");
	SCRIPT_REG_TEMPLFUNC(GetPhysPosAng, "entityId");
	SCRIPT_REG_FUNC(GetEntityFromPhysicsId);
	SCRIPT_REG_FUNC(GetPhysicsIdFromEntity);
}

int ScriptBind_Physics::SimulateExplosion(IFunctionHandler *pH, SmartScriptTable explisionTable)
{
	{
		// Must be in local scope.
		CScriptSetGetChain params(explisionTable);

		pe_explosion expl;
		params.GetValue("pos", expl.epicenter);
		params.GetValue("radius", expl.r);
		params.GetValue("direction", expl.explDir);
		params.GetValue("impulse_pos", expl.epicenterImp);
		params.GetValue("impulse_pressure", expl.impulsivePressureAtR);
		params.GetValue("rmin", expl.rmin);
		params.GetValue("rmax", expl.rmax);
		params.GetValue("hole_size", expl.holeSize);

		gEnv->pPhysicalWorld->SimulateExplosion(&expl);
	}

	return pH->EndFunction();
}

int ScriptBind_Physics::RegisterExplosionShape(IFunctionHandler *pH, const char *sGeometryFile, float fSize, int nIdMaterial, float fProbability, const char *sSplintersFile, float fSplintersOffset, const char *sSplintersCloudEffect)
{
	// remove all this
	IStatObj *pObj = gEnv->p3DEngine->LoadStatObj(sGeometryFile, "#ForceBreakable", nullptr);
	if (!pObj || pObj->IsDefaultObject())
	{
		CryLogWarning("Physics.RegisterExplosionShape: Object file %s not found", sGeometryFile);
		return pH->EndFunction();
	}

	pObj->AddRef();
	pObj->GetIndexedMesh(true);  // prepare idxMesh now

	if (sSplintersFile && *sSplintersFile)  // if sSplintersFile was specified
	{
		IStatObj *pSplinters = gEnv->p3DEngine->LoadStatObj(sSplintersFile, nullptr, nullptr);
		if (pSplinters)
		{
			pObj->SetSubObjectCount(pObj->GetSubObjectCount() + 1);
			IStatObj::SSubObject *pSubObj = pObj->GetSubObject(pObj->GetSubObjectCount() - 1);
			pSubObj->nType = STATIC_SUB_OBJECT_MESH;
			pSubObj->bHidden = true;
			pSubObj->name = "splinters";
			(pSubObj->pStatObj = pSplinters)->AddRef();
			pSubObj->helperSize.x = fSplintersOffset;
			nIdMaterial |= 1 << 16;

			if (sSplintersCloudEffect && *sSplintersCloudEffect)
			{
				pSplinters->SetSubObjectCount(pSplinters->GetSubObjectCount() + 1);
				pSplinters->SetFlags(pSplinters->GetFlags() & ~STATIC_OBJECT_COMPOUND);
				pSubObj = pSplinters->GetSubObject(pSplinters->GetSubObjectCount() - 1);
				pSubObj->nType = STATIC_SUB_OBJECT_DUMMY;
				pSubObj->bHidden = true;
				pSubObj->name = "splinters_cloud";
				pSubObj->properties = sSplintersCloudEffect;
			}
		}
	}

	phys_geometry *pPhysGeom = pObj->GetPhysGeom();
	if (pPhysGeom)
	{
		gEnv->pPhysicalWorld->AddExplosionShape(pPhysGeom->pGeom, fSize, nIdMaterial, fProbability);
	}

	return pH->EndFunction();
}

int ScriptBind_Physics::RegisterExplosionCrack(IFunctionHandler *pH, const char *sGeometryFile, int nIdMaterial)
{
	IStatObj *pObj = gEnv->p3DEngine->LoadStatObj(sGeometryFile, "#ForceBreakable", nullptr);
	if (!pObj || pObj->IsDefaultObject())
	{
		CryLogWarning("<RegisterExplosionCrack> Object file %s not found", sGeometryFile);
		return pH->EndFunction();
	}

	pObj->AddRef();

	Vec3 vtx[3] = {
		pObj->GetHelperPos("1"),
		pObj->GetHelperPos("2"),
		pObj->GetHelperPos("3")
	};

	// TODO: restore it
	gEnv->pPhysicalWorld->GetGeomManager()->RegisterCrack(pObj->GetPhysGeom()->pGeom, vtx, 0);

	return pH->EndFunction();
}

int ScriptBind_Physics::RayWorldIntersection(IFunctionHandler *pH)
{
	assert(pH->GetParamCount() >= 3 && pH->GetParamCount() <= 7);

	Vec3 vPos(0, 0, 0);
	Vec3 vDir(0, 0, 0);
	ScriptHandle skipId1;
	ScriptHandle skipId2;
	IEntity *skipEnt1 = nullptr;
	IEntity *skipEnt2 = nullptr;
	int nMaxHits = 0;
	int iEntTypes = ent_all;

	SmartScriptTable hitTable(m_pSS);

	if (!pH->GetParams(vPos, vDir, nMaxHits, iEntTypes))
		return pH->EndFunction();

	int nParams = pH->GetParamCount();

	if (nParams >= 5 && pH->GetParamType(5) != svtNull && pH->GetParam(5, skipId1))
		skipEnt1 = gEnv->pEntitySystem->GetEntity(static_cast<EntityId>(skipId1.n));

	if (nParams >= 6 && pH->GetParamType(6) != svtNull && pH->GetParam(6, skipId2))
		skipEnt2 = gEnv->pEntitySystem->GetEntity(static_cast<EntityId>(skipId2.n));

	bool bHitTablePassed = (nParams >= 7);

	if (bHitTablePassed)
		pH->GetParam(7, hitTable);

	if (nMaxHits > 10)
		nMaxHits = 10;

	ray_hit RayHit[10];

	int skipIdCount = 0;
	IPhysicalEntity *skipPhys[2] = { nullptr, nullptr };

	if (skipEnt1)
	{
		++skipIdCount;
		skipPhys[0] = skipEnt1->GetPhysics();
	}

	if (skipEnt2)
	{
		++skipIdCount;
		skipPhys[1] = skipEnt2->GetPhysics();
	}

	const int flags = geom_colltype0 << rwi_colltype_bit | rwi_stop_at_pierceable;

	int nHits = gEnv->pPhysicalWorld->RayWorldIntersection(vPos, vDir, iEntTypes, flags, RayHit, nMaxHits, skipPhys, skipIdCount);

	for (int i = 0; i < nHits; i++)
	{
		SmartScriptTable pHitObj(m_pSS);
		ray_hit & Hit = RayHit[i];
		pHitObj->SetValue("pos", Hit.pt);
		pHitObj->SetValue("normal", Hit.n);
		pHitObj->SetValue("dist", Hit.dist);
		pHitObj->SetValue("surface", Hit.surface_idx);

		IEntity *pEntity = (IEntity*) Hit.pCollider->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
		if (pEntity)
		{
			pHitObj->SetValue("entity", pEntity->GetScriptTable());
		}
		else
		{
			if (Hit.pCollider->GetiForeignData() == PHYS_FOREIGN_ID_STATIC)
			{
				IRenderNode *pRN = (IRenderNode*) Hit.pCollider->GetForeignData(PHYS_FOREIGN_ID_STATIC);
				if (pRN)
					pHitObj->SetValue("renderNode", ScriptHandle(pRN));
			}
			else if (Hit.pCollider->GetiForeignData() == PHYS_FOREIGN_ID_FOLIAGE)
			{
				IRenderNode *pRN = ((IFoliage*) Hit.pCollider->GetForeignData(PHYS_FOREIGN_ID_FOLIAGE))->GetIRenderNode();
				if (pRN)
					pHitObj->SetValue("renderNode", ScriptHandle(pRN));
			}
		}

		hitTable->SetAt(i + 1, pHitObj);
	}

	if (bHitTablePassed)
		return pH->EndFunction(nHits);

	return pH->EndFunction(*hitTable);
}

int ScriptBind_Physics::RayTraceCheck(IFunctionHandler *pH, Vec3 src, Vec3 dst, ScriptHandle skipEntityId1, ScriptHandle skipEntityId2)
{
	SCRIPT_CHECK_PARAMETERS(4);

	IEntity *skipEnt1 = gEnv->pEntitySystem->GetEntity((EntityId)skipEntityId1.n);
	IEntity *skipEnt2 = gEnv->pEntitySystem->GetEntity((EntityId)skipEntityId2.n);
	IPhysicalEntity *skipPhys[2] = { nullptr, nullptr };

	if (skipEnt1)
		skipPhys[0] = skipEnt1->GetPhysics();

	if (skipEnt2)
		skipPhys[1] = skipEnt2->GetPhysics();

	ray_hit RayHit;

	const int entityTypes = ent_static | ent_terrain;
	const int flags = rwi_ignore_noncolliding | rwi_stop_at_pierceable;

	//TODO: add an extraparam to specify what kind of objects to check? now its world and static
	int nHits = gEnv->pPhysicalWorld->RayWorldIntersection(src, dst - src, entityTypes, flags, &RayHit, 1, skipPhys, 2);

	return pH->EndFunction(static_cast<bool>(nHits == 0));
}

int ScriptBind_Physics::SamplePhysEnvironment(IFunctionHandler *pH)
{
	int i = 0;
	int nHits = 0;
	int objtypes = ent_static | ent_rigid | ent_sleeping_rigid | ent_sort_by_mass;
	pe_status_nparts snp;
	pe_status_pos sp;
	IPhysicalEntity **pEnts;
	geom_world_data gwd;
	IGeometry *pSphere;
	primitives::sphere sph;
	intersection_params ip;
	geom_contact *pcontacts;
	SmartScriptTable pObj(m_pSS);
	ip.bStopAtFirstTri = true;
	ip.bNoBorder = true;
	ip.bNoAreaContacts = true;
	ip.bThreadSafe = true;

	if (!pH->GetParams(sph.center, sph.r))
		return pH->EndFunction();

	if (pH->GetParamCount() > 2)
		pH->GetParam(3, objtypes);

	pSphere = gEnv->pPhysicalWorld->GetGeomManager()->CreatePrimitive(primitives::sphere::type, &sph);

	int nEnts = gEnv->pPhysicalWorld->GetEntitiesInBox(sph.center - Vec3(sph.r), sph.center + Vec3(sph.r), pEnts, objtypes);
	for (i = 0; i < nEnts; i++)
	{
		for (sp.ipart = pEnts[i]->GetStatus(&snp) - 1; sp.ipart >= 0; sp.ipart--)
		{
			sp.partid = -1;
			pEnts[i]->GetStatus(&sp);
			gwd.offset = sp.pos;
			gwd.R = Matrix33(sp.q);
			gwd.scale = sp.scale;
			if ((sp.flagsOR & (geom_colltype0 | geom_no_coll_response)) == geom_colltype0
			 && sp.pGeomProxy->Intersect(pSphere, &gwd, 0, &ip, pcontacts))
			{
				++nHits;

				IEntity* pEntity = (IEntity*)pEnts[i]->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
				if (pEntity)
					pObj->SetAt(nHits  *3 - 2, pEntity->GetScriptTable());
				else
					pObj->SetNullAt(nHits  *3 - 2);

				pObj->SetAt(nHits  *3 - 1, sp.partid);
				pObj->SetAt(nHits  *3, gEnv->pPhysicalWorld->GetPhysicalEntityId(pEnts[i]));

				if (pEnts[i]->GetType() != PE_ARTICULATED)
					break;
			}
		}
	}

	pSphere->Release();

	return pH->EndFunction(*pObj);
}

int ScriptBind_Physics::GetPhysPosAng(IFunctionHandler* pH, ScriptHandle entityId)
{
	const EntityId entId = (EntityId)entityId.n;
	IEntity* pEnt = gEnv->pEntitySystem->GetEntity(entId);
	if (!pEnt)
		return pH->EndFunction();

	// Prefer physics pose
	if (IPhysicalEntity* pe = pEnt->GetPhysics())
	{
		pe_status_pos sp; memset(&sp, 0, sizeof(sp));
		if (pe->GetStatus(&sp))
		{
			// #2: physics angles (RADIANS) via Quat -> Matrix33 -> Euler XYZ
			const Matrix33 m33(sp.q);
			const Ang3 aRad = Ang3::GetAnglesXYZ(m33);

			return pH->EndFunction(Script::SetCachedVector(sp.pos, pH, 1), Script::SetCachedVector((Vec3)aRad, pH, 2));
		}
	}

	const Vec3 pos = pEnt->GetWorldPos();
	const Ang3 ang = pEnt->GetWorldAngles();
	return pH->EndFunction(Script::SetCachedVector(pos, pH, 1), Script::SetCachedVector((Vec3)ang, pH, 2));
}

int ScriptBind_Physics::GetEntityFromPhysicsId(IFunctionHandler* pH)
{
	if (pH->GetParamCount() < 1 || pH->GetParamType(1) != svtNumber)
	{
		CryLogAlways("[GetEntityFromPhysicsId] invalid arg: need number at param 1");
		return pH->EndFunction();
	}

	int physId = 0;
	pH->GetParam(1, physId);

	IPhysicalEntity* pPE = gEnv->pPhysicalWorld->GetPhysicalEntityById(physId);
	if (!pPE)
	{
		CryLogAlways("[GetEntityFromPhysicsId] no physical entity for physId=%d", physId);
		return pH->EndFunction();
	}

	IEntity* pEnt = gEnv->pEntitySystem->GetEntityFromPhysics(pPE);
	if (!pEnt)
	{
		CryLogAlways("[GetEntityFromPhysicsId] no IEntity for physId=%d (pPE=%p)", physId, pPE);
		return pH->EndFunction();
	}

	if (IScriptTable* pObj = pEnt->GetScriptTable())
	{
		return pH->EndFunction(pObj);
	}

	CryLogAlways("[GetEntityFromPhysicsId] entity has no ScriptTable: %s[%u] (physId=%d)",
		pEnt->GetName(), pEnt->GetId(), physId);
	return pH->EndFunction();
}

int ScriptBind_Physics::GetPhysicsIdFromEntity(IFunctionHandler* pH)
{
	if (pH->GetParamCount() < 1)
		return pH->EndFunction();

	IEntity* pEnt = nullptr;

	// entity table?
	if (pH->GetParamType(1) == svtObject)
	{
		IScriptTable* pObj = nullptr;
		if (pH->GetParam(1, pObj) && pObj)
		{
			ScriptHandle sh;
			if (pObj->GetValue("id", sh))
				pEnt = gEnv->pEntitySystem->GetEntity((EntityId)sh.n);
		}
	}
	// number?
	else if (pH->GetParamType(1) == svtNumber)
	{
		EntityId eID = 0; pH->GetParam(1, eID);
		pEnt = gEnv->pEntitySystem->GetEntity(eID);
	}
	// ScriptHandle?
	else
	{
		ScriptHandle sh;
		if (pH->GetParam(1, sh))
			pEnt = gEnv->pEntitySystem->GetEntity((EntityId)sh.n);
	}

	if (!pEnt)
		return pH->EndFunction();

	IPhysicalEntity* pPE = pEnt->GetPhysics();
	if (!pPE)
		pH->EndFunction();

	const int physId = gEnv->pPhysicalWorld->GetPhysicalEntityId(pPE);
	return pH->EndFunction(physId);
}


