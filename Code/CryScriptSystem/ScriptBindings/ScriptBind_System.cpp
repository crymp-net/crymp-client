#include <time.h>
#include <string.h>
#include <string>

#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CryFont/IFont.h"
#include "CryCommon/CrySystem/ILog.h"
#include "CryCommon/Cry3DEngine/I3DEngine.h"
#include "CryCommon/CryRenderer/IRenderer.h"
#include "CryCommon/CryInput/IInput.h"
#include "CryCommon/CryEntitySystem/IEntitySystem.h"
#include "CryCommon/CrySystem/ITimer.h"
#include "CryCommon/CrySystem/IConsole.h"
#include "CryCommon/CrySystem/CryFind.h"
#include "CryCommon/CryAction/IGameFramework.h"
#include "CryCommon/CryMath/Cry_Camera.h"
#include "CryCommon/CryMath/Cry_Geo.h"
#include "CryCommon/CryRenderer/IRenderAuxGeom.h"
#include "CryCommon/CryAction/IVehicleSystem.h"
#include "CryCommon/CryAction/IActorSystem.h"
#include "CryCommon/CryAction/IItemSystem.h" // <== GetItemClasses

#include "ScriptBind_System.h"

// modes of ScanDirectory function
enum
{
	SCANDIR_ALL     = 0,
	SCANDIR_FILES   = 1,
	SCANDIR_SUBDIRS = 2
};

ScriptBind_System::ScriptBind_System(IScriptSystem *pSS)
{
	CScriptableBase::Init(pSS, gEnv->pSystem);
	SetGlobalName("System");

	m_pScriptTimeTable.Create(pSS);

	pSS->SetGlobalValue("SCANDIR_ALL", SCANDIR_ALL);
	pSS->SetGlobalValue("SCANDIR_FILES", SCANDIR_FILES);
	pSS->SetGlobalValue("SCANDIR_SUBDIRS", SCANDIR_SUBDIRS);

#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &ScriptBind_System::

	SCRIPT_REG_FUNC(LoadFont);
	SCRIPT_REG_FUNC(ExecuteCommand);
	SCRIPT_REG_FUNC(LogToConsole);
	SCRIPT_REG_FUNC(LogAlways);
	SCRIPT_REG_FUNC(ClearConsole);
	SCRIPT_REG_FUNC(Log);
	SCRIPT_REG_FUNC(Warning);
	SCRIPT_REG_FUNC(Error);
	SCRIPT_REG_TEMPLFUNC(IsEditor, "");
	SCRIPT_REG_TEMPLFUNC(IsEditing, "");
	SCRIPT_REG_FUNC(GetCurrTime);
	SCRIPT_REG_FUNC(GetCurrAsyncTime);
	SCRIPT_REG_FUNC(GetFrameTime);
	SCRIPT_REG_FUNC(GetLocalOSTime);
	SCRIPT_REG_FUNC(GetUserName);
	SCRIPT_REG_FUNC(DrawLabel);
	SCRIPT_REG_FUNC(GetEntity);
	SCRIPT_REG_FUNC(GetEntityClass);
	SCRIPT_REG_FUNC(GetEntities);
	SCRIPT_REG_TEMPLFUNC(GetEntitiesInSphere, "center, radius");
	SCRIPT_REG_TEMPLFUNC(GetEntitiesInSphereByClass, "center, radius, className");
	SCRIPT_REG_TEMPLFUNC(GetPhysicalEntitiesInBox, "center, radius");
	SCRIPT_REG_TEMPLFUNC(GetPhysicalEntitiesInBoxByClass, "center, radius, className");
	SCRIPT_REG_TEMPLFUNC(GetEntitiesByClass, "EntityClass");
	SCRIPT_REG_TEMPLFUNC(GetNearestEntityByClass, "center, radius, className");
	SCRIPT_REG_TEMPLFUNC(GetEntityByName, "sEntityName");
	SCRIPT_REG_TEMPLFUNC(GetEntityIdByName, "sEntityName");
	SCRIPT_REG_FUNC(DeformTerrain);
	SCRIPT_REG_FUNC(DeformTerrainUsingMat);
	SCRIPT_REG_FUNC(ScreenToTexture);
	SCRIPT_REG_FUNC(DrawLine);
	SCRIPT_REG_FUNC(Draw2DLine);
	SCRIPT_REG_FUNC(DrawText);
	SCRIPT_REG_TEMPLFUNC(DrawSphere, "x, y, z, radius, r, g, b, a");
	SCRIPT_REG_TEMPLFUNC(DrawAABB, "x, y, z, x2, y2, z2, r, g, b, a");
	SCRIPT_REG_TEMPLFUNC(DrawOBB, "x, y, z, w, h, d, rx, ry, rz");
	SCRIPT_REG_FUNC(SetGammaDelta);
	SCRIPT_REG_FUNC(ShowConsole);
	SCRIPT_REG_FUNC(IsMultiplayer);
	SCRIPT_REG_FUNC(SetPostProcessFxParam);
	SCRIPT_REG_FUNC(GetPostProcessFxParam);
	SCRIPT_REG_FUNC(SetScreenFx);
	SCRIPT_REG_FUNC(GetScreenFx);
	SCRIPT_REG_FUNC(SetCVar);
	SCRIPT_REG_FUNC(GetCVar);
	SCRIPT_REG_FUNC(AddCCommand);
	SCRIPT_REG_FUNC(SetScissor);
	SCRIPT_REG_FUNC(IsHDRSupported);
	SCRIPT_REG_FUNC(SetWind);
	SCRIPT_REG_FUNC(GetWind);
	SCRIPT_REG_TEMPLFUNC(GetSurfaceTypeIdByName, "surfaceName");
	SCRIPT_REG_TEMPLFUNC(GetSurfaceTypeNameById, "surfaceId");
	SCRIPT_REG_TEMPLFUNC(RemoveEntity, "entityId");
	SCRIPT_REG_TEMPLFUNC(SpawnEntity, "params");
	SCRIPT_REG_FUNC(IsValidMapPos);
	SCRIPT_REG_FUNC(EnableOceanRendering);
	SCRIPT_REG_FUNC(ScanDirectory);
	SCRIPT_REG_FUNC(ViewDistanceGet);
	SCRIPT_REG_FUNC(ApplyForceToEnvironment);
	SCRIPT_REG_FUNC(GetOutdoorAmbientColor);
	SCRIPT_REG_FUNC(GetTerrainElevation);
	SCRIPT_REG_FUNC(ActivatePortal);
	SCRIPT_REG_FUNC(IsPointIndoors);
	SCRIPT_REG_TEMPLFUNC(ProjectToScreen, "point");
	SCRIPT_REG_FUNC(Break);
	SCRIPT_REG_TEMPLFUNC(SetViewCameraFov, "fov");
	SCRIPT_REG_TEMPLFUNC(GetViewCameraFov, "");
	SCRIPT_REG_TEMPLFUNC(IsPointVisible, "point");
	SCRIPT_REG_FUNC(GetViewCameraPos);
	SCRIPT_REG_FUNC(GetViewCameraDir);
	SCRIPT_REG_FUNC(GetViewCameraUpDir);
	SCRIPT_REG_FUNC(GetViewCameraAngles);
	SCRIPT_REG_FUNC(IsDevModeEnable);
	SCRIPT_REG_FUNC(SaveConfiguration);
	SCRIPT_REG_FUNC(Quit);
	SCRIPT_REG_FUNC(ClearKeyState);
	SCRIPT_REG_TEMPLFUNC(SetSunColor, "vSunColor");
	SCRIPT_REG_TEMPLFUNC(GetSunColor, "");
	SCRIPT_REG_TEMPLFUNC(SetSkyColor, "vSkyColor");
	SCRIPT_REG_TEMPLFUNC(GetSkyColor, "");
	SCRIPT_REG_TEMPLFUNC(SetSkyHighlight, "tableSkyHighlightParams");
	SCRIPT_REG_TEMPLFUNC(GetSkyHighlight, "");
	SCRIPT_REG_TEMPLFUNC(LoadLocalizationXml, "filename");
	SCRIPT_REG_FUNC(GetFrameID);
	SCRIPT_REG_FUNC(GetActors);
	SCRIPT_REG_TEMPLFUNC(GetActorsByClass, "class");
	SCRIPT_REG_FUNC(GetPlayers);
	SCRIPT_REG_FUNC(GetVehicles);
	SCRIPT_REG_TEMPLFUNC(GetVehiclesByClass, "class");
	SCRIPT_REG_TEMPLFUNC(IsClassValid, "class");
	SCRIPT_REG_TEMPLFUNC(GetEntityScriptFilePath, "class");
	SCRIPT_REG_FUNC(GetVehicleClasses);
	SCRIPT_REG_FUNC(GetItemClasses);
	SCRIPT_REG_FUNC(AwakeDefaultObjects);
	SCRIPT_REG_TEMPLFUNC(GetCountOfClass, "class");
	SCRIPT_REG_TEMPLFUNC(GetEntityIdByNumId, "numId");
	SCRIPT_REG_TEMPLFUNC(RemoveEntitiesByClass, "entclass");
}

int ScriptBind_System::LoadFont(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	const char *fontName = nullptr;
	pH->GetParam(1, fontName);

	ICryFont *pCryFont = gEnv->pCryFont;
	if (pCryFont && fontName)
	{
		std::string fontPath = "fonts/";
		fontPath += fontName;
		fontPath += ".xml";

		IFFont *pFont = pCryFont->NewFont(fontName);

		if (!pFont->Load(fontPath.c_str()))
		{
			CryLogError("System.LoadFont: Error loading digital font from %s", fontPath.c_str());
		}
	}

	return pH->EndFunction();
}

int ScriptBind_System::ExecuteCommand(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	const char *cmd = nullptr;
	pH->GetParam(1, cmd);

	if (cmd)
		gEnv->pConsole->ExecuteString(cmd);

	return pH->EndFunction();
}

int ScriptBind_System::Log(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	const char *message = "";
	pH->GetParam(1, message);

	gEnv->pLog->Log("<Lua> %s", message);

	return pH->EndFunction();
}

int ScriptBind_System::LogToConsole(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	const char *message = "";
	pH->GetParam(1, message);

	gEnv->pLog->LogToConsole("<Lua> %s", message);

	return pH->EndFunction();
}

int ScriptBind_System::LogAlways(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	const char *message = "";
	pH->GetParam(1, message);

	CryLogAlways("%s", message);

	return pH->EndFunction();
}

int ScriptBind_System::Warning(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	const char *message = "";
	pH->GetParam(1, message);

	gEnv->pSystem->Warning(VALIDATOR_MODULE_SCRIPTSYSTEM, VALIDATOR_WARNING, 0, nullptr, "%s", message);

	return pH->EndFunction();
}

int ScriptBind_System::Error(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	const char *message = "";
	pH->GetParam(1, message);

	gEnv->pSystem->Warning(VALIDATOR_MODULE_SCRIPTSYSTEM, VALIDATOR_ERROR, 0, nullptr, "%s", message);

	return pH->EndFunction();
}

int ScriptBind_System::IsEditor(IFunctionHandler *pH)
{
	return pH->EndFunction(gEnv->bEditor);
}

int ScriptBind_System::IsEditing(IFunctionHandler *pH)
{
	return pH->EndFunction(!gEnv->bEditorGameMode);
}

int ScriptBind_System::ClearConsole(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(0);

	gEnv->pConsole->Clear();

	return pH->EndFunction();
}

int ScriptBind_System::GetCurrTime(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(0);

	float fTime = gEnv->pTimer->GetCurrTime();

	return pH->EndFunction(fTime);
}

int ScriptBind_System::GetCurrAsyncTime(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(0);

	float fTime = gEnv->pTimer->GetAsyncCurTime();

	return pH->EndFunction(fTime);
}

int ScriptBind_System::GetFrameTime(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(0);

	float fTime = gEnv->pTimer->GetFrameTime();

	return pH->EndFunction(fTime);
}

int ScriptBind_System::GetLocalOSTime(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(0);

	__time64_t now = _time64(nullptr);
	struct tm *newtime = _localtime64(&now);

	if (newtime)
	{
		m_pScriptTimeTable->BeginSetGetChain();
		m_pScriptTimeTable->SetValueChain("sec", newtime->tm_sec);
		m_pScriptTimeTable->SetValueChain("min", newtime->tm_min);
		m_pScriptTimeTable->SetValueChain("hour", newtime->tm_hour);
		m_pScriptTimeTable->SetValueChain("isdst", newtime->tm_isdst);
		m_pScriptTimeTable->SetValueChain("mday", newtime->tm_mday);
		m_pScriptTimeTable->SetValueChain("wday", newtime->tm_wday);
		m_pScriptTimeTable->SetValueChain("mon", newtime->tm_mon);
		m_pScriptTimeTable->SetValueChain("yday", newtime->tm_yday);
		m_pScriptTimeTable->SetValueChain("year", newtime->tm_year);
		m_pScriptTimeTable->EndSetGetChain();
	}

	return pH->EndFunction(m_pScriptTimeTable);
}

int ScriptBind_System::GetUserName(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(0);

	return pH->EndFunction(gEnv->pSystem->GetUserName());
}

int ScriptBind_System::ShowConsole(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	int show = 0;
	pH->GetParam(1, show);

	gEnv->pConsole->ShowConsole(show != 0);

	return pH->EndFunction();
}

int ScriptBind_System::IsMultiplayer(IFunctionHandler *pH)
{
	return pH->EndFunction(gEnv->bMultiplayer);
}

int ScriptBind_System::GetEntity(IFunctionHandler *pH)
{
	EntityId eID = 0;

	if (pH->GetParamType(1) == svtNumber)
	{
		pH->GetParam(1, eID);
	}
	else
	{
		ScriptHandle sh;
		pH->GetParam(1, sh);
		eID = static_cast<EntityId>(sh.n);
	}

	IEntity *pEntity = gEnv->pEntitySystem->GetEntity(eID);
	if (pEntity)
	{
		IScriptTable *pObject = pEntity->GetScriptTable();
		if (pObject)
		{
			return pH->EndFunction(pObject);
		}
	}

	return pH->EndFunction();
}

int ScriptBind_System::GetEntityClass(IFunctionHandler *pH)
{
	EntityId eID = 0;

	if (pH->GetParamType(1) == svtNumber)
	{
		pH->GetParam(1, eID);
	}
	else
	{
		ScriptHandle sh;
		pH->GetParam(1, sh);
		eID = (EntityId)sh.n;
	}

	IEntity *pEntity = gEnv->pEntitySystem->GetEntity(eID);
	if (pEntity)
	{
		return pH->EndFunction(pEntity->GetClass()->GetName());
	}

	return pH->EndFunction();
}

int ScriptBind_System::GetEntities(IFunctionHandler *pH)
{
	Vec3 center(0, 0, 0);
	float radius = 0;

	if (pH->GetParamCount() > 1)
	{
		pH->GetParam(1, center);
		pH->GetParam(2, radius);
	}

	SmartScriptTable pObj(m_pSS);
	int k = 0;

	IEntityItPtr pIIt = gEnv->pEntitySystem->GetEntityIterator();
	IEntity *pEntity = nullptr;

	while ((pEntity = pIIt->Next()) != nullptr)
	{
		if (radius)
		{
			if ((pEntity->GetWorldPos() - center).len2() > radius*radius)
			{
				continue;
			}
		}

		if (pEntity->GetScriptTable())
		{
			pObj->SetAt(k, pEntity->GetScriptTable());
			k++;
		}
	}

	return pH->EndFunction(*pObj);
}

int ScriptBind_System::GetEntitiesByClass(IFunctionHandler *pH, const char *entityClass)
{
	if (!entityClass || !*entityClass)
		return pH->EndFunction();

	IEntityClass *pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(entityClass);
	if (!pClass)
	{
		return pH->EndFunction();
	}

	SmartScriptTable pObj(m_pSS);
	IEntityItPtr pIIt = gEnv->pEntitySystem->GetEntityIterator();
	IEntity *pEntity = nullptr;
	int k = 1;

	pIIt->MoveFirst();

	while ((pEntity = pIIt->Next()) != nullptr)
	{
		if (pEntity->GetClass() == pClass)
		{
			if (pEntity->GetScriptTable())
			{
				pObj->SetAt(k++, pEntity->GetScriptTable());
			}
		}
	}

	return pH->EndFunction(*pObj);
}

int ScriptBind_System::GetEntitiesInSphere(IFunctionHandler *pH, Vec3 center, float radius)
{
	SmartScriptTable pObj(m_pSS);
	IEntityItPtr pIIt = gEnv->pEntitySystem->GetEntityIterator();
	IEntity *pEntity = nullptr;
	int k = 1;

	pIIt->MoveFirst();

	while ((pEntity = pIIt->Next()) != nullptr)
	{
		if ((pEntity->GetWorldPos() - center).len2() <= radius  *radius)
		{
			if (pEntity->GetScriptTable())
			{
				pObj->SetAt(k++, pEntity->GetScriptTable());
			}
		}
	}

	return pH->EndFunction(*pObj);
}

int ScriptBind_System::GetEntitiesInSphereByClass(IFunctionHandler *pH, Vec3 center, float radius, const char *entityClass)
{
	if (!entityClass || !*entityClass)
		return pH->EndFunction();

	IEntityClass *pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(entityClass);
	if (!pClass)
	{
		return pH->EndFunction();
	}

	SmartScriptTable pObj(m_pSS);
	IEntityItPtr pIIt = gEnv->pEntitySystem->GetEntityIterator();
	IEntity *pEntity = 0;
	int k = 1;

	pIIt->MoveFirst();

	while ((pEntity = pIIt->Next()) != nullptr)
	{
		if ((pEntity->GetClass() == pClass) && ((pEntity->GetWorldPos() - center).len2() <= radius  *radius))
		{
			if (pEntity->GetScriptTable())
			{
				pObj->SetAt(k++, pEntity->GetScriptTable());
			}
		}
	}

	return pH->EndFunction(*pObj);
}

static bool Filter(const CryFindEntry& entry, int scanMode)
{
	switch (scanMode)
	{
		case SCANDIR_ALL:     return true;
		case SCANDIR_SUBDIRS: return entry.IsDirectory();
		case SCANDIR_FILES:   return !entry.IsDirectory();
	}

	return false;
}

int ScriptBind_System::ScanDirectory(IFunctionHandler *pH)
{
	if (pH->GetParamCount() < 1)
	{
		return pH->EndFunction();
	}

	SmartScriptTable pObj(m_pSS);
	int k = 1;

	const char *folderName = nullptr;
	if (!pH->GetParam(1, folderName))
	{
		return pH->EndFunction(*pObj);
	}

	int scanMode = SCANDIR_SUBDIRS;
	if (pH->GetParamCount() > 1)
	{
		pH->GetParam(2, scanMode);
	}

	std::string wildcard(folderName);
	wildcard += "/*";

	for (auto& entry : CryFind(wildcard.c_str()))
	{
		if (Filter(entry, scanMode))
		{
			pObj->SetAt(k, entry.name);
			k++;
		}
	}

	return pH->EndFunction(*pObj);
}

int ScriptBind_System::DrawLabel(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(7);

	Vec3 vPos(0, 0, 0);
	float fSize = 0;
	const char *text = "";
	float r = 1;
	float g = 1;
	float b = 1;
	float alpha = 1;

	if (!pH->GetParams(vPos, fSize, text, r, g, b, alpha))
		return pH->EndFunction();

	if (gEnv->pRenderer)
	{
		float color[] = { r, g, b, alpha };

		gEnv->pRenderer->DrawLabelEx(vPos, fSize, color, true, true, "%s", text);
	}

	return pH->EndFunction();
}

int ScriptBind_System::GetPhysicalEntitiesInBox(IFunctionHandler *pH, Vec3 center, float radius)
{
	SEntityProximityQuery query;
	query.box.min = center - Vec3(radius, radius, radius);
	query.box.max = center + Vec3(radius, radius, radius);
	gEnv->pEntitySystem->QueryProximity(query);

	int n = query.nCount;
	if (n > 0)
	{
		SmartScriptTable tbl(m_pSS);

		int k = 0;
		for (int i = 0; i < n; i++)
		{
			IEntity *pEntity = query.pEntities[i];
			if (pEntity)
			{
				// The physics can return multiple records per one entity, filter out entities of same id.
				if (!pEntity->GetPhysics())
					continue;

				IScriptTable *pEntityTable = pEntity->GetScriptTable();
				if (pEntityTable)
				{
					tbl->SetAt(++k, pEntityTable);
				}
			}
		}

		if (k)
			return pH->EndFunction(tbl);
	}

	return pH->EndFunction();
}

int ScriptBind_System::GetPhysicalEntitiesInBoxByClass(IFunctionHandler *pH, Vec3 center, float radius, const char *className)
{
	IEntityClass *pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(className);
	if (!pClass)
	{
		return pH->EndFunction();
	}

	SEntityProximityQuery query;
	query.box.min = center - Vec3(radius, radius, radius);
	query.box.max = center + Vec3(radius, radius, radius);
	query.pEntityClass = pClass;
	gEnv->pEntitySystem->QueryProximity(query);

	int n = query.nCount;
	if (n > 0)
	{
		SmartScriptTable tbl(m_pSS);

		int k = 0;
		for (int i = 0; i < n; i++)
		{
			IEntity *pEntity = query.pEntities[i];
			if (pEntity)
			{
				// The physics can return multiple records per one entity, filter out entities of same id.
				if (!pEntity->GetPhysics())
					continue;

				IScriptTable *pEntityTable = pEntity->GetScriptTable();
				if (pEntityTable)
				{
					tbl->SetAt(++k, pEntityTable);
				}
			}
		}

		if (k)
			return pH->EndFunction(tbl);
	}

	return pH->EndFunction();
}

int ScriptBind_System::GetNearestEntityByClass(IFunctionHandler *pH, Vec3 center, float radius, const char *className)
{
	IEntityClass *pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(className);
	if (!pClass)
		return pH->EndFunction();

	ScriptHandle ignore[2];
	ignore[0].n = 0;
	ignore[1].n = 0;

	if (pH->GetParamCount() > 3)
		pH->GetParam(4, ignore[0]);
	if (pH->GetParamCount() > 4)
		pH->GetParam(5, ignore[1]);

	SEntityProximityQuery query;
	query.box.min = center - Vec3(radius, radius, radius);
	query.box.max = center + Vec3(radius, radius, radius);
	query.pEntityClass = pClass;
	gEnv->pEntitySystem->QueryProximity(query);

	int closest = -1;
	float closestdist2 = 1e+8f;

	int n = query.nCount;
	if (n > 0)
	{
		for (int i = 0; i < n; i++)
		{
			IEntity *pEntity = query.pEntities[i];
			if (pEntity && pEntity->GetId() != ignore[0].n && pEntity->GetId() != ignore[1].n)
			{
				float dist2 = (pEntity->GetWorldPos() - center).len2();
				if (dist2 < closestdist2)
				{
					closest = i;
					closestdist2 = dist2;
				}
			}
		}

		if (closest > -1)
			return pH->EndFunction(query.pEntities[closest]->GetScriptTable());
	}

	return pH->EndFunction();
}

int ScriptBind_System::GetEntityByName(IFunctionHandler *pH, const char *sEntityName)
{
	IEntity *pEntity = gEnv->pEntitySystem->FindEntityByName(sEntityName);
	if (pEntity)
	{
		IScriptTable *pObject = pEntity->GetScriptTable();
		return pH->EndFunction(pObject);
	}

	return pH->EndFunction();
}

int ScriptBind_System::GetEntityIdByName(IFunctionHandler *pH, const char *sEntityName)
{
	IEntity *pEntity = gEnv->pEntitySystem->FindEntityByName(sEntityName);
	if (pEntity)
	{
		ScriptHandle handle;
		handle.n = pEntity->GetId();
		return pH->EndFunction(handle);
	}

	return pH->EndFunction();
}

static void DeformTerrainInternal(IFunctionHandler *pH, I3DEngine *p3DEngine)
{
	if (pH->GetParamCount() >= 3)
		return;

	Vec3 vPos;
	float fSize;
	const char *name = nullptr;

	pH->GetParams(vPos, fSize, name);

	bool bDeform = true;

	if (pH->GetParamCount() > 3)
		pH->GetParam(4, bDeform);

	p3DEngine->OnExplosion(vPos, fSize, bDeform);
}

int ScriptBind_System::DeformTerrain(IFunctionHandler *pH)
{
	DeformTerrainInternal(pH, gEnv->p3DEngine);

	return pH->EndFunction();
}

int ScriptBind_System::DeformTerrainUsingMat(IFunctionHandler *pH)
{
	DeformTerrainInternal(pH, gEnv->p3DEngine);

	return pH->EndFunction();
}

int ScriptBind_System::ScreenToTexture(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(0);

	gEnv->pRenderer->ScreenToTexture();

	return pH->EndFunction();
}

int ScriptBind_System::DrawLine(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(6);

	IRenderAuxGeom *pRenderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();

	Vec3 p1(0, 0, 0);
	Vec3 p2(0, 0, 0);
	pH->GetParam(1, p1);
	pH->GetParam(2, p2);

	float r, g, b, a;
	pH->GetParam(3, r);
	pH->GetParam(4, g);
	pH->GetParam(5, b);
	pH->GetParam(6, a);
	ColorB col((unsigned char)(r  *255.0f), (unsigned char)(g  *255.0f),
	           (unsigned char)(b  *255.0f), (unsigned char)(a  *255.0f));

	pRenderAuxGeom->SetRenderFlags(e_Def3DPublicRenderflags);
	pRenderAuxGeom->DrawLine(p1, col, p2, col);

	return pH->EndFunction();
}

int ScriptBind_System::Draw2DLine(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(8);

	IRenderAuxGeom *pRenderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();

	Vec3 p1(0, 0, 0);
	Vec3 p2(0, 0, 0);
	pH->GetParam(1, p1.x);
	pH->GetParam(2, p1.y);
	pH->GetParam(3, p2.x);
	pH->GetParam(4, p2.y);

	// do normalization as exiting scripts assume a virtual window size of 800x600
	// however the auxiliary geometry rendering system uses normalized device coords for 2d primitive rendering
	const float c_Normalize2Dx(1.0f / 800.0f);
	const float c_Normalize2Dy(1.0f / 600.0f);
	p1.x *= c_Normalize2Dx;
	p1.y *= c_Normalize2Dy;
	p2.x *= c_Normalize2Dx;
	p2.y *= c_Normalize2Dy;

	float r, g, b, a;
	pH->GetParam(5, r);
	pH->GetParam(6, g);
	pH->GetParam(7, b);
	pH->GetParam(8, a);
	ColorB col((unsigned char)(r  *255.0f), (unsigned char)(g  *255.0f),
	           (unsigned char)(b  *255.0f), (unsigned char)(a  *255.0f));

	SAuxGeomRenderFlags renderFlags(e_Def2DPublicRenderflags);

	if (a < 1.0f)
	{
		renderFlags.SetAlphaBlendMode(e_AlphaBlended);
	}

	pRenderAuxGeom->SetRenderFlags(renderFlags);
	pRenderAuxGeom->DrawLine(p1, col, p2, col);

	return pH->EndFunction();
}

int ScriptBind_System::DrawText(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(9);

	ICryFont *pCryFont = gEnv->pCryFont;
	if (!pCryFont)
	{
		return pH->EndFunction();
	}

	float x = 0;
	float y = 0;
	const char *text = "";
	const char *fontName = "default";
	float size = 16;
	float r = 1;
	float g = 1;
	float b = 1;
	float a = 1;

	pH->GetParam(1, x);
	pH->GetParam(2, y);
	pH->GetParam(3, text);
	pH->GetParam(4, fontName);
	pH->GetParam(5, size);
	pH->GetParam(6, r);
	pH->GetParam(7, g);
	pH->GetParam(8, b);
	pH->GetParam(9, a);

	IFFont *pFont = pCryFont->GetFont(fontName);

	if (!pFont)
	{
		return pH->EndFunction();
	}

	pFont->SetColor(ColorF(r, g, b, a));
	pFont->SetSize(Vec2(size, size));
	pFont->DrawString(x, y, text, true);

	return pH->EndFunction();
}

int ScriptBind_System::DrawSphere(IFunctionHandler *pH, float x, float y, float z, float radius, int r, int g, int b, int a)
{
	IRenderAuxGeom *pRenderAuxGeom =gEnv->pRenderer->GetIRenderAuxGeom();
	if (pRenderAuxGeom)
	{
		SAuxGeomRenderFlags oldFlags = pRenderAuxGeom->GetRenderFlags();
		SAuxGeomRenderFlags newFlags = oldFlags;

		newFlags.SetCullMode(e_CullModeNone);
		newFlags.SetFillMode(e_FillModeWireframe);
		newFlags.SetAlphaBlendMode(e_AlphaBlended);
		pRenderAuxGeom->SetRenderFlags(newFlags);

		pRenderAuxGeom->DrawSphere(Vec3(x, y, z), radius, ColorB(r, g, b, a), false);

		pRenderAuxGeom->SetRenderFlags(oldFlags);
	}

	return pH->EndFunction();
}

int ScriptBind_System::DrawAABB(IFunctionHandler *pH, float x, float y, float z, float x2, float y2, float z2, int r, int g, int b, int a)
{
	IRenderAuxGeom *pRenderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
	if (pRenderAuxGeom)
	{
		SAuxGeomRenderFlags oldFlags = pRenderAuxGeom->GetRenderFlags();
		SAuxGeomRenderFlags newFlags = oldFlags;

		newFlags.SetCullMode(e_CullModeNone);
		newFlags.SetAlphaBlendMode(e_AlphaBlended);
		pRenderAuxGeom->SetRenderFlags(newFlags);

		AABB bbox(Vec3(x, y, z), Vec3(x2, y2, z2));
		pRenderAuxGeom->DrawAABB(bbox, true, ColorB(r, g, b, a), eBBD_Faceted);

		pRenderAuxGeom->SetRenderFlags(oldFlags);
	}

	return pH->EndFunction();
}

int ScriptBind_System::DrawOBB(IFunctionHandler *pH, float x, float y, float z, float w, float h, float d, float rx, float ry, float rz)
{
	IRenderAuxGeom *pRenderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
	if (pRenderAuxGeom)
	{
		SAuxGeomRenderFlags oldFlags = pRenderAuxGeom->GetRenderFlags();
		SAuxGeomRenderFlags newFlags = oldFlags;

		newFlags.SetCullMode(e_CullModeNone);
		newFlags.SetAlphaBlendMode(e_AlphaBlended);
		pRenderAuxGeom->SetRenderFlags(newFlags);

		AABB bbox(Vec3(-w  *0.5f, -h  *0.5f, -d  *0.5f), Vec3(w  *0.5f, h  *0.5f, d  *0.5f));
		OBB obb = OBB::CreateOBBfromAABB(Matrix33::CreateRotationXYZ(Ang3(rx, ry, rz)), bbox);
		pRenderAuxGeom->DrawOBB(obb, Vec3(x, y, z), true, ColorB(255, 128, 128, 128), eBBD_Faceted);

		pRenderAuxGeom->SetRenderFlags(oldFlags);
	}

	return pH->EndFunction();
}

int ScriptBind_System::SetPostProcessFxParam(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);

	const char *effectName = "";
	pH->GetParam(1, effectName);

	switch (pH->GetParamType(2))
	{
		case svtNumber:
		{
			float fValue = -1;
			pH->GetParam(2, fValue);

			gEnv->p3DEngine->SetPostEffectParam(effectName, fValue);

			break;
		}
		case svtObject:
		{
			Vec3 pValue = Vec3(0, 0, 0);
			pH->GetParam(2, pValue);

			gEnv->p3DEngine->SetPostEffectParamVec4(effectName, Vec4(pValue, 1));

			break;
		}
		case svtString:
		{
			const char *value = "";
			pH->GetParam(2, value);

			gEnv->p3DEngine->SetPostEffectParamString(effectName, value);

			break;
		}
	}

	return pH->EndFunction();
}

int ScriptBind_System::GetPostProcessFxParam(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);

	const char *effectName = nullptr;
	pH->GetParam(1, effectName);

	switch (pH->GetParamType(2))
	{
		case svtNumber:
		{
			float fValue = 0;
			pH->GetParam(2, fValue);

			gEnv->p3DEngine->GetPostEffectParam(effectName, fValue);

			return pH->EndFunction(fValue);
		}
		case svtString:
		{
			const char *value = "";
			pH->GetParam(2, value);

			gEnv->p3DEngine->GetPostEffectParamString(effectName, value);

			return pH->EndFunction(value);
		}
	}

	return pH->EndFunction();
}

int ScriptBind_System::SetScreenFx(IFunctionHandler *pH)
{
	return SetPostProcessFxParam(pH);
}

int ScriptBind_System::GetScreenFx(IFunctionHandler *pH)
{
	return GetPostProcessFxParam(pH);
}

int ScriptBind_System::SetCVar(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);

	const char *name = "";
	pH->GetParam(1, name);

	ICVar *pCVar = gEnv->pConsole->GetCVar(name);
	if (pCVar)
	{
		switch (pH->GetParamType(2))
		{
			case svtNumber:
			{
				float value = 0;
				pH->GetParam(2, value);

				pCVar->Set(value);

				break;
			}
			case svtString:
			{
				const char *value = "";
				pH->GetParam(2, value);

				pCVar->Set(value);

				break;
			}
		}
	}
	else
	{
		CryLogWarning("Script.SetCVar: Console variable '%s' not found", name);
	}

	return pH->EndFunction();
}

int ScriptBind_System::GetCVar(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	const char *name = "";
	pH->GetParam(1, name);

	ICVar *pCVar = gEnv->pConsole->GetCVar(name);
	if (pCVar)
	{
		if (pCVar->GetType() == CVAR_FLOAT || pCVar->GetType() == CVAR_INT)
		{
			return pH->EndFunction(pCVar->GetFVal());
		}
		else if (pCVar->GetType() == CVAR_STRING)
		{
			return pH->EndFunction(pCVar->GetString());
		}
	}
	else
	{
		CryLogWarning("Script.GetCVar: Console variable '%s' not found", name);
	}

	return pH->EndFunction();
}

int ScriptBind_System::AddCCommand(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(3);

	const char *commandName = "";
	pH->GetParam(1, commandName);

	const char *command = "";
	pH->GetParam(2, command);

	const char *help = "";
	pH->GetParam(3, help);

	gEnv->pConsole->AddCommand(commandName, command, 0, help);

	return pH->EndFunction();
}

int ScriptBind_System::SetScissor(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(4);

	float x;
	float y;
	float w;
	float h;

	pH->GetParam(1, x);
	pH->GetParam(2, y);
	pH->GetParam(3, w);
	pH->GetParam(4, h);

	gEnv->pRenderer->SetScissor(
	  (int) gEnv->pRenderer->ScaleCoordX(x),
	  (int) gEnv->pRenderer->ScaleCoordY(y),
	  (int) gEnv->pRenderer->ScaleCoordX(w),
	  (int) gEnv->pRenderer->ScaleCoordY(h)
	);

	return pH->EndFunction();
}

int ScriptBind_System::IsValidMapPos(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	bool isValid = false;

	Vec3 v(0, 0, 0);
	if (pH->GetParam(1, v))
	{
		int nTerrainSize = gEnv->p3DEngine->GetTerrainSize();
		float fOut = static_cast<float>(nTerrainSize + 500);

		if (v.x < -500 || v.y < -500 || v.z > 500 || v.x > fOut || v.y > fOut)
			isValid = false;
		else
			isValid = true;
	}

	return pH->EndFunction(isValid);
}

int ScriptBind_System::ViewDistanceGet(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(0);

	float fDist = gEnv->p3DEngine->GetMaxViewDistance();

	return pH->EndFunction(fDist);
}

int ScriptBind_System::SetSunColor(IFunctionHandler *pH, Vec3 vColor)
{
	gEnv->p3DEngine->SetGlobalParameter(E3DPARAM_SUN_COLOR, vColor);

	return pH->EndFunction();
}

int ScriptBind_System::GetSunColor(IFunctionHandler *pH)
{
	Vec3 vColor;
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SUN_COLOR, vColor);

	return pH->EndFunction(vColor);
}

int ScriptBind_System::SetSkyColor(IFunctionHandler *pH, Vec3 vColor)
{
	gEnv->p3DEngine->SetGlobalParameter(E3DPARAM_SKY_COLOR, vColor);

	return pH->EndFunction();
}

int ScriptBind_System::GetSkyColor(IFunctionHandler *pH)
{
	Vec3 vColor;
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SKY_COLOR, vColor);

	return pH->EndFunction(vColor);
}

int ScriptBind_System::SetSkyHighlight(IFunctionHandler *pH, SmartScriptTable tbl)
{
	float fSize = 0;
	Vec3 vColor(0, 0, 0);
	Vec3 vPos(0, 0, 0);

	tbl->GetValue("size", fSize);
	tbl->GetValue("color", vColor);
	tbl->GetValue("position", vPos);

	gEnv->p3DEngine->SetGlobalParameter(E3DPARAM_SKY_HIGHLIGHT_SIZE, fSize);
	gEnv->p3DEngine->SetGlobalParameter(E3DPARAM_SKY_HIGHLIGHT_COLOR, vColor);
	gEnv->p3DEngine->SetGlobalParameter(E3DPARAM_SKY_HIGHLIGHT_POS, vPos);

	return pH->EndFunction();
}

int ScriptBind_System::GetSkyHighlight(IFunctionHandler *pH, SmartScriptTable params)
{
	float fSize = 0;
	Vec3 vColor(0, 0, 0);
	Vec3 vPos(0, 0, 0);

	fSize = gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SKY_HIGHLIGHT_SIZE);
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SKY_HIGHLIGHT_COLOR, vColor);
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SKY_HIGHLIGHT_POS, vPos);

	params->SetValue("size", fSize);
	params->SetValue("color", vColor);
	params->SetValue("position", vPos);

	return pH->EndFunction();
}

int ScriptBind_System::ApplyForceToEnvironment(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(3);

	float force;
	float radius;

	Vec3 pos(0, 0, 0);
	pH->GetParam(1, pos);
	pH->GetParam(2, radius);
	pH->GetParam(3, force);

	gEnv->p3DEngine->ApplyForceToEnvironment(pos, radius, force);

	return pH->EndFunction();
}

int ScriptBind_System::GetOutdoorAmbientColor(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(0);

	Vec3 v3Color = gEnv->p3DEngine->GetSkyColor();

	return pH->EndFunction(v3Color);
}

int ScriptBind_System::GetTerrainElevation(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	Vec3 v3Pos;
	pH->GetParam(1, v3Pos);

	float elevation = gEnv->p3DEngine->GetTerrainElevation(v3Pos.x, v3Pos.y);

	return pH->EndFunction(elevation);
}

int ScriptBind_System::ActivatePortal(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(3);

	Vec3 vPos;
	bool bActivate;
	ScriptHandle nID;

	pH->GetParam(1, vPos);
	pH->GetParam(2, bActivate);
	pH->GetParam(3, nID);

	IEntity *pEnt = gEnv->pEntitySystem->GetEntity(static_cast<EntityId>(nID.n));

	gEnv->p3DEngine->ActivatePortal(vPos, bActivate, pEnt ? pEnt->GetName() : "[Not specified by script]");

	//gEnv->pAudioSystem->GetInterfaceExtended()->RecomputeSoundOcclusion(false,true);

	return pH->EndFunction();
}

int ScriptBind_System::IsPointIndoors(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	bool bInside = false;

	Vec3 vPos(0, 0, 0);
	if (pH->GetParam(1, vPos))
	{
		bInside = gEnv->p3DEngine->GetVisAreaFromPos(vPos) != 0;
	}

	return pH->EndFunction(bInside);
}

int ScriptBind_System::SetGammaDelta(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	float fDelta = 0;
	pH->GetParam(1, fDelta);

	gEnv->pRenderer->SetGammaDelta(fDelta);

	return pH->EndFunction();
}

int ScriptBind_System::ProjectToScreen(IFunctionHandler *pH, Vec3 vec)
{
	Vec3 res(0, 0, 0);
	gEnv->pRenderer->ProjectToScreen(vec.x, vec.y, vec.z, &res.x, &res.y, &res.z);

	res.x *= 8.0f;
	res.y *= 6.0f;

	return pH->EndFunction(Script::SetCachedVector(res, pH, 2));
}

int ScriptBind_System::EnableOceanRendering(IFunctionHandler *pH)
{
	bool bOcean = true;
	if (pH->GetParam(1, bOcean))
		if (gEnv->p3DEngine)
			gEnv->p3DEngine->EnableOceanRendering(bOcean);

	return pH->EndFunction();
}

int ScriptBind_System::Break(IFunctionHandler *pH)
{
	gEnv->pSystem->Error("ScriptBind_System:Break");

	return pH->EndFunction();
}

int ScriptBind_System::SetViewCameraFov(IFunctionHandler *pH, float fov)
{
	CCamera& Camera = gEnv->pSystem->GetViewCamera();
	Camera.SetFrustum(Camera.GetViewSurfaceX(), Camera.GetViewSurfaceZ(), fov, DEFAULT_NEAR, DEFAULT_FAR);

	return pH->EndFunction();
}

int ScriptBind_System::GetViewCameraFov(IFunctionHandler *pH)
{
	const CCamera& Camera = gEnv->pSystem->GetViewCamera();

	return pH->EndFunction(Camera.GetFov());
}

int ScriptBind_System::IsPointVisible(IFunctionHandler *pH, Vec3 point)
{
	const CCamera& Camera = gEnv->pSystem->GetViewCamera();

	return pH->EndFunction(Camera.IsPointVisible(point));
}

int ScriptBind_System::GetViewCameraPos(IFunctionHandler *pH)
{
	const CCamera& Camera = gEnv->pSystem->GetViewCamera();

	return pH->EndFunction(Script::SetCachedVector(Camera.GetPosition(), pH, 1));
}

int ScriptBind_System::GetViewCameraDir(IFunctionHandler *pH)
{
	const CCamera& Camera = gEnv->pSystem->GetViewCamera();
	Matrix34 cameraMatrix = Camera.GetMatrix();

	return pH->EndFunction(Script::SetCachedVector(cameraMatrix.GetColumn(1), pH, 1));
}

int ScriptBind_System::GetViewCameraUpDir(IFunctionHandler *pH)
{
	const CCamera& Camera = gEnv->pSystem->GetViewCamera();
	Matrix34 cameraMatrix = Camera.GetMatrix();

	return pH->EndFunction(Script::SetCachedVector(cameraMatrix.GetColumn(2), pH, 1));
}

int ScriptBind_System::GetViewCameraAngles(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(0);

	const CCamera& Camera = gEnv->pSystem->GetViewCamera();

	return pH->EndFunction(Vec3(RAD2DEG(Ang3::GetAnglesXYZ(Matrix33(Camera.GetMatrix())))));
}

int ScriptBind_System::IsHDRSupported(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(0);

	return pH->EndFunction(gEnv->pRenderer->GetFeatures() & RFT_HW_HDR);
}

int ScriptBind_System::SetWind(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	Vec3 vWind(0, 0, 0);
	pH->GetParam(1, vWind);

	gEnv->p3DEngine->SetWind(vWind);

	return pH->EndFunction();
}

int ScriptBind_System::GetWind(IFunctionHandler *pH)
{
	return pH->EndFunction(gEnv->p3DEngine->GetWind(AABB(ZERO), false));
}

int ScriptBind_System::GetSurfaceTypeIdByName(IFunctionHandler *pH, const char *surfaceName)
{
	ISurfaceType *pSurface = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeByName(surfaceName);
	if (pSurface)
		return pH->EndFunction(pSurface->GetId());

	return pH->EndFunction();
}

int ScriptBind_System::GetSurfaceTypeNameById(IFunctionHandler *pH, int surfaceId)
{
	ISurfaceType *pSurface = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceType(surfaceId);

	if (pSurface)
		return pH->EndFunction(pSurface->GetName());

	return pH->EndFunction();
}

int ScriptBind_System::RemoveEntity(IFunctionHandler *pH, ScriptHandle entityId)
{
	gEnv->pEntitySystem->RemoveEntity(static_cast<EntityId>(entityId.n));

	return pH->EndFunction();
}

static void MergeTable(IScriptSystem *pSS, IScriptTable *pDest, IScriptTable *pSrc)
{
	IScriptTable::Iterator it = pSrc->BeginIteration();

	while (pSrc->MoveNext(it))
	{
		if (pSrc->GetAtType(it.nKey) != svtNull)
		{
			if (pSrc->GetAtType(it.nKey) == svtObject)
			{
				SmartScriptTable tbl;

				if (pDest->GetAtType(it.nKey) != svtObject)
				{
					tbl = SmartScriptTable(pSS->CreateTable());
					pDest->SetAtAny(it.nKey, tbl);
				}
				else
				{
					tbl = SmartScriptTable(pSS, true);
					pDest->GetAt(it.nKey, tbl);
				}

				SmartScriptTable srcTbl;
				it.value.CopyTo(srcTbl);
				MergeTable(pSS, tbl, srcTbl);
			}
			else
			{
				pDest->SetAtAny(it.nKey, it.value);
			}
		}
		else if (pSrc->GetValueType(it.sKey) != svtNull)
		{
			if (pSrc->GetValueType(it.sKey) == svtObject)
			{
				SmartScriptTable tbl;

				if (pDest->GetValueType(it.sKey) != svtObject)
				{
					tbl = SmartScriptTable(pSS->CreateTable());
					pDest->SetValue(it.sKey, tbl);
				}
				else
				{
					tbl = SmartScriptTable(pSS, true);
					pDest->GetValue(it.sKey, tbl);
				}

				SmartScriptTable srcTbl;
				it.value.CopyTo(srcTbl);
				MergeTable(pSS, tbl, srcTbl);
			}
			else
			{
				pDest->SetValueAny(it.sKey, it.value);
			}
		}
	}

	pSrc->EndIteration(it);
}

int ScriptBind_System::SpawnEntity(IFunctionHandler *pH, SmartScriptTable params)
{
	const char *entityClass = nullptr;
	const char *entityName = "";
	const char *archetypeName = nullptr;
	ScriptHandle entityId;
	SmartScriptTable propsTable(m_pSS, true);
	SmartScriptTable propsInstanceTable(m_pSS, true);

	Vec3 pos(0.0f, 0.0f, 0.0f);
	Vec3 dir(1.0f, 0.0f, 0.0f);
	Vec3 scale(1.0f, 1.0f, 1.0f);
	int flags = 0;
	bool props = false;
	bool propsInstance = false;

	{
		CScriptSetGetChain chain(params);

		chain.GetValue("id", entityId);
		chain.GetValue("class", entityClass);
		chain.GetValue("name", entityName);
		chain.GetValue("position", pos);
		chain.GetValue("orientation", dir);   //orientation unit vector
		chain.GetValue("scale", scale);
		chain.GetValue("flags", flags);
		chain.GetValue("archetype", archetypeName);

		if (params.GetPtr())
		{
			props = params.GetPtr()->GetValue("properties", propsTable);
			propsInstance = params.GetPtr()->GetValue("propertiesInstance", propsInstanceTable);
		}
	}

	ScriptHandle hdl;
	IEntity *pProtoEntity = nullptr;

	if (pH->GetParamCount() > 1 && pH->GetParam(2, hdl))
	{
		pProtoEntity = gEnv->pEntitySystem->GetEntity((EntityId)hdl.n);
	}

	if (!entityClass)
	{
		return pH->EndFunction();
	}

	if (dir.IsZero(.1f))
	{
		dir = Vec3(1.0f, 0.0f, 0.0f);
		CryLog("System.SpawnEntity: Zero orientation of entity %s", entityName);
	}
	else
	{
		dir.NormalizeSafe();
	}

	SEntitySpawnParams spawnParams;
	spawnParams.id = (EntityId)entityId.n;
	spawnParams.qRotation = Quat(Matrix33::CreateRotationVDir(dir));
	spawnParams.vPosition = pos;
	spawnParams.vScale = scale;
	spawnParams.sName = entityName;
	spawnParams.pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(entityClass);

	if (archetypeName)
	{
		spawnParams.pArchetype = gEnv->pEntitySystem->LoadEntityArchetype(archetypeName);
	}

	if (!spawnParams.pClass)
	{
		gEnv->pLog->Log("Error: no such entity class %s (entity name: %s)", entityClass, entityName);
		return pH->EndFunction();
	}

	spawnParams.nFlags = flags;

	if (pProtoEntity)
	{
		// if there is a prototype - use some flags of prototype entity
		spawnParams.nFlags |= pProtoEntity->GetFlags() & (ENTITY_FLAG_CASTSHADOW
		                                                | ENTITY_FLAG_GOOD_OCCLUDER
		                                                | ENTITY_FLAG_RECVWIND
		                                                | ENTITY_FLAG_OUTDOORONLY);
	}

	IEntity *pEntity = gEnv->pEntitySystem->SpawnEntity(spawnParams, !props);
	if (!pEntity)
		return pH->EndFunction();

	IScriptTable *pEntityTable = pEntity->GetScriptTable();

	if (props)
	{
		if (pEntityTable)
		{
			SmartScriptTable entityProps(m_pSS, false);

			if (pEntityTable->GetValue("Properties", entityProps))
			{
				MergeTable(m_pSS, entityProps, propsTable);
			}

			if (propsInstance && pEntityTable->GetValue("PropertiesInstance", entityProps))
			{
				MergeTable(m_pSS, entityProps, propsInstanceTable);
			}
		}

		gEnv->pEntitySystem->InitEntity(pEntity, spawnParams);
	}

	if (pEntity && pEntityTable)
	{
		return pH->EndFunction(pEntityTable);
	}

	return pH->EndFunction();
}

int ScriptBind_System::IsDevModeEnable(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(0);

	return pH->EndFunction(gEnv->pSystem->IsDevMode());
}

int ScriptBind_System::SaveConfiguration(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(0);

	gEnv->pSystem->SaveConfiguration();

	return pH->EndFunction();
}

int ScriptBind_System::Quit(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(0);

	gEnv->pSystem->Quit();

	return pH->EndFunction();
}

int ScriptBind_System::ClearKeyState(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(0);

	gEnv->pSystem->GetIInput()->ClearKeyState();

	return pH->EndFunction();
}

int ScriptBind_System::LoadLocalizationXml(IFunctionHandler *pH, const char *filename)
{
	gEnv->pSystem->GetLocalizationManager()->LoadExcelXmlSpreadsheet(filename);

	return pH->EndFunction();
}

int ScriptBind_System::GetFrameID(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(0);

	return pH->EndFunction(gEnv->pRenderer->GetFrameID());
}

int ScriptBind_System::GetActors(IFunctionHandler *pH)
{
	SmartScriptTable pObj(m_pSS);
	IActorSystem* pActorSystem = gEnv->pGame->GetIGameFramework()->GetIActorSystem();
	IActorIteratorPtr pActorIterator = pActorSystem->CreateActorIterator();
	IActor* pActor = nullptr;
	int k = 1;

	while ((pActor = pActorIterator->Next()) != nullptr)
	{
		pObj->SetAt(k++, pActor->GetEntity()->GetScriptTable());
	}
	return pH->EndFunction(*pObj);
}

int ScriptBind_System::GetActorsByClass(IFunctionHandler* pH, const char *entityClass)
{
	IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(entityClass);
	if (!pClass)
	{
		return pH->EndFunction();
	}
	SmartScriptTable pObj(m_pSS);
	IActorSystem* pActorSystem = gEnv->pGame->GetIGameFramework()->GetIActorSystem();
	IActorIteratorPtr pActorIterator = pActorSystem->CreateActorIterator();
	IActor* pActor = nullptr;
	int k = 1;

	while ((pActor = pActorIterator->Next()) != nullptr)
	{
		if (pActor->GetEntity()->GetClass() == pClass)
		{
			pObj->SetAt(k++, pActor->GetEntity()->GetScriptTable());
		}
	}
	return pH->EndFunction(*pObj);
}

int ScriptBind_System::GetPlayers(IFunctionHandler* pH)
{
	SmartScriptTable pObj(m_pSS);
	IActorSystem* pActorSystem = gEnv->pGame->GetIGameFramework()->GetIActorSystem();
	IActorIteratorPtr pActorIterator = pActorSystem->CreateActorIterator();
	IActor* pActor = nullptr;
	int k = 1;

	while ((pActor = pActorIterator->Next()) != nullptr)
	{
		if (!pActor->IsPlayer())
		{
			continue;
		}
		pObj->SetAt(k++, pActor->GetEntity()->GetScriptTable());
	}
	return pH->EndFunction(*pObj);
}

int ScriptBind_System::GetVehicles(IFunctionHandler * pH)
{
	SmartScriptTable pObj(m_pSS);
	IVehicleSystem* pVehicleSystem = gEnv->pGame->GetIGameFramework()->GetIVehicleSystem();
	IVehicleIteratorPtr pVehicleIterator = pVehicleSystem->CreateVehicleIterator();
	IVehicle* pVehicle = nullptr;
	int k = 1;

	while ((pVehicle = pVehicleIterator->Next()) != nullptr)
	{
		if (pVehicle->GetEntity()->GetScriptTable())
		{
			pObj->SetAt(k++, pVehicle->GetEntity()->GetScriptTable());
		}
	}
	return pH->EndFunction(*pObj);
}

int ScriptBind_System::GetVehiclesByClass(IFunctionHandler* pH, const char* entityClass)
{
	IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(entityClass);
	if (!pClass)
	{
		return pH->EndFunction();
	}

	SmartScriptTable pObj(m_pSS);
	IVehicleSystem* pVehicleSystem = gEnv->pGame->GetIGameFramework()->GetIVehicleSystem();
	IVehicleIteratorPtr pVehicleIterator = pVehicleSystem->CreateVehicleIterator();
	IVehicle* pVehicle = nullptr;
	int k = 1;

	while ((pVehicle = pVehicleIterator->Next()) != nullptr)
	{
		if (pVehicle->GetEntity()->GetClass() == pClass)
		{
			if (pVehicle->GetEntity()->GetScriptTable())
			{
				pObj->SetAt(k++, pVehicle->GetEntity()->GetScriptTable());
			}
		}
	}
	return pH->EndFunction(*pObj);
}

int ScriptBind_System::IsClassValid(IFunctionHandler* pH, const char* entityClass)
{
	IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(entityClass);
	if (pClass)
	{
		return pH->EndFunction(true);
	}
	return pH->EndFunction(false);
}

int ScriptBind_System::GetEntityScriptFilePath(IFunctionHandler* pH, const char* entityClass)
{
	IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(entityClass);
	if (!pClass)
	{
		return pH->EndFunction();
	}
	return pH->EndFunction(pClass->GetScriptFile());
}

int ScriptBind_System::GetItemClasses(IFunctionHandler * pH)
{
	SmartScriptTable pObj(m_pSS);
	int k = 1;

	IItemSystem* pItemSystem = gEnv->pGame->GetIGameFramework()->GetIItemSystem();
	IEntityClassRegistry* pEntityRegistry = gEnv->pEntitySystem->GetClassRegistry();
	IEntityClass* pClass;

	for (pEntityRegistry->IteratorMoveFirst(); (pClass = pEntityRegistry->IteratorNext()) != nullptr;)
	{
		if (pItemSystem->IsItemClass(pClass->GetName()))
		{
			pObj->SetAt(k++, pClass->GetName());
		}

	}

	return pH->EndFunction(*pObj);
}

int ScriptBind_System::GetVehicleClasses(IFunctionHandler * pH)
{
	SmartScriptTable pObj(m_pSS);
	int k = 1;

	IVehicleSystem* pVehicleSystem = gEnv->pGame->GetIGameFramework()->GetIVehicleSystem();
	IEntityClassRegistry* pEntityRegistry = gEnv->pEntitySystem->GetClassRegistry();
	IEntityClass* pClass;

	for (pEntityRegistry->IteratorMoveFirst(); (pClass = pEntityRegistry->IteratorNext()) != nullptr;)
	{
		if (pVehicleSystem->IsVehicleClass(pClass->GetName())) 
		{
			pObj->SetAt(k++, pClass->GetName());
		}
	}

	return pH->EndFunction(*pObj);
}

int ScriptBind_System::AwakeDefaultObjects(IFunctionHandler* pH)
{
	IEntityItPtr pIIt = gEnv->pEntitySystem->GetEntityIterator();
	IEntity* pEntity = nullptr;

	int counter = 0;

	IEntityClass* pDefaultClass = gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();

	while ((pEntity = pIIt->Next()) != nullptr)
	{
		if (pEntity->GetClass() == pDefaultClass)
		{
			if (IPhysicalEntity* pPhysics = pEntity->GetPhysics())
			{
				pe_action_awake aa;
				aa.bAwake = 1;
				pPhysics->Action(&aa);
				++counter;
			}
		}
	}

	return pH->EndFunction(counter);
}

int ScriptBind_System::GetCountOfClass(IFunctionHandler* pH, const char* EntityClass)
{
	IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(EntityClass);
	if (!pClass)
	{
		return pH->EndFunction();
	}

	IEntityItPtr pIIt = gEnv->pEntitySystem->GetEntityIterator();
	IEntity* pEntity = nullptr;
	int k = 1;

	pIIt->MoveFirst();

	while ((pEntity = pIIt->Next()))
	{
		IEntityClass* entClass = pEntity->GetClass();
		if (entClass == pClass)
		{
			++k;
		}
	}

	return pH->EndFunction(k);
}

int ScriptBind_System::GetEntityIdByNumId(IFunctionHandler* pH, int entityId)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId);
	if (pEntity)
		return pH->EndFunction(ScriptHandle(pEntity->GetId()));

	return pH->EndFunction();
}

int ScriptBind_System::RemoveEntitiesByClass(IFunctionHandler* pH, const char* EntityClass)
{
	IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(EntityClass);
	if (!pClass)
	{
		return pH->EndFunction();
	}

	IEntityItPtr pIIt = gEnv->pEntitySystem->GetEntityIterator();
	IEntity* pEntity = nullptr;
	int k = 1;

	pIIt->MoveFirst();

	while ((pEntity = pIIt->Next()))
	{
		auto* entClass = pEntity->GetClass();
		if (entClass == pClass)
		{
			pEntity->ClearFlags(ENTITY_FLAG_UNREMOVABLE);
			gEnv->pEntitySystem->RemoveEntity(pEntity->GetId(), true);
			++k;
		}
	}

	return pH->EndFunction(k);
}