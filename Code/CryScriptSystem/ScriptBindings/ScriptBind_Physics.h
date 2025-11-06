#pragma once

#include "CryCommon/CryScriptSystem/IScriptSystem.h"

class ScriptBind_Physics : public CScriptableBase
{
public:
	ScriptBind_Physics(IScriptSystem *pSS);

	int SimulateExplosion(IFunctionHandler *pH, SmartScriptTable explosionParams);
	int RegisterExplosionShape(IFunctionHandler *pH, const char *sGeometryFile, float fSize, int nIdMaterial, float fProbability, const char *sSplintersFile, float fSplintersOffset, const char *sSplintersCloudEffect);
	int RegisterExplosionCrack(IFunctionHandler *pH, const char *sGeometryFile, int nIdMaterial);
	int RayWorldIntersection(IFunctionHandler *pH);
	int RayTraceCheck(IFunctionHandler *pH, Vec3 src, Vec3 dst, ScriptHandle skipEntityId1, ScriptHandle skipEntityId2);
	int SamplePhysEnvironment(IFunctionHandler *pH);
	int GetPhysPosAng(IFunctionHandler* pH, ScriptHandle entityId);
	int GetEntityFromPhysicsId(IFunctionHandler* pH);
	int GetPhysicsIdFromEntity(IFunctionHandler* pH);
};
