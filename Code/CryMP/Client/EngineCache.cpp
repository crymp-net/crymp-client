#include <string>
#include <vector>

#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CrySystem/CryFind.h"
#include "CryCommon/CrySystem/CryPath.h"
#include "CryCommon/CrySystem/IConsole.h"
#include "CryCommon/Cry3DEngine/I3DEngine.h"
#include "CryCommon/CryAnimation/ICryAnimation.h"

#include "EngineCache.h"

static void CacheObjectsInfo(IConsoleCmdArgs* pArgs)
{
	int vCount = -1;
	gEnv->pCharacterManager->GetLoadedModels(nullptr, vCount);
	CryLogAlways("$3Loaded Models: %d", vCount);

	int sCount = -1;
	gEnv->p3DEngine->GetLoadedStatObjArray(nullptr, sCount);
	CryLogAlways("$3Loaded StatObj: %d", sCount);

	int rCount = -1;
	gEnv->p3DEngine->GetVoxelRenderNodes(nullptr, rCount);
	CryLogAlways("$3Loaded VoxelRenderNodes: %d", rCount);

	uint32 mCount = -1;
	gEnv->p3DEngine->GetMaterialManager()->GetLoadedMaterials(nullptr, mCount);
	CryLogAlways("$3Loaded Materials: %d", mCount);
}

EngineCache::EngineCache()
{
	IConsole* pConsole = gEnv->pConsole;
	pConsole->Register("cl_engineCacheLevel", &cl_engineCacheLevel, 1, VF_NOT_NET_SYNCED, "0 - off, 4 - maximum level");

	pConsole->AddCommand("cacheInfo", CacheObjectsInfo, 0, "Get cached object info");
}

EngineCache::~EngineCache()
{
	IConsole* pConsole = gEnv->pConsole;
	pConsole->UnregisterVariable("cl_engineCacheLevel", true);
}

int EngineCache::ScanFolder(const char* folderName)
{
	int counter = 0;
	std::string wildcard = folderName;
	wildcard += "/*.*";

	for (auto& entry : CryFind(wildcard.c_str()))
	{
		std::string entryPath = folderName;
		entryPath += '/';
		entryPath += entry.name;

		if (entry.IsDirectory())
		{
			counter += ScanFolder(entryPath.c_str());
		}
		else
		{
			counter += Cache(entryPath.c_str());
		}
	}

	return counter;
}

bool EngineCache::Cache(const char* file)
{
	const char* ext = CryPath::GetExt(file);

	if (!_stricmp(ext, "cdf") || !_stricmp(ext, "chr") || !_stricmp(ext, "cga"))
	{
		ICharacterInstance* pChar = gEnv->pCharacterManager->CreateInstance(file);
		if (pChar && pChar->GetFilePath())
		{
			pChar->AddRef();
			m_cachedCharacterInstances.emplace_back(pChar);
			return true;
		}
	}
	else if (!_stricmp(ext, "cgf"))
	{
		IStatObj* pStatObj = gEnv->p3DEngine->LoadStatObj(file);
		if (pStatObj && pStatObj->GetFilePath())
		{
			pStatObj->AddRef();
			m_cachedStatObjs.emplace_back(pStatObj);
			return true;
		}
	}
	else if (!_stricmp(ext, "mtl"))
	{
		IMaterial* pMat = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(file);
		if (pMat)
		{
			pMat->AddRef();
			m_cachedMaterials.emplace_back(pMat);
		}
	}

	return false;
}

void EngineCache::OnLoadingStart(ILevelInfo* pLevel)
{
	m_isCached = false;
}

void EngineCache::OnLoadingProgress(ILevelInfo* pLevel, int progressAmount)
{
	if (m_isCached)
	{
		return;
	}

	m_isCached = true;

	int counter = 0;
	std::vector<std::string> folders = {};

	if (cl_engineCacheLevel == MINIMUM)
	{
		folders = {
			//"Objects/Characters/Human/story",
			"Objects/Characters/Human/Asian/Nanosuit",
			"Objects/Characters/Human/US/NanoSuit",
			"Objects/Vehicles/US_Vtol"
		};
	}
	else if (cl_engineCacheLevel == RECOMMENDED)
	{
		folders = {
			"Objects/Characters/Human/story",
			"Objects/Vehicles",
			"Materials"
		};
	}
	else if (cl_engineCacheLevel == HIGH)
	{
		folders = {
			"Objects/Characters/Human",
			"Objects/Weapons",
			"Objects/Vehicles",
			"Materials"
		};
	}
	else if (cl_engineCacheLevel >= LUDICROUS)
	{
		folders = {
			"Objects",
			"Materials"
		};
	}

	for (const std::string& folder : folders)
	{
		counter += ScanFolder(folder.c_str());
	}

	if (counter > 0)
	{
		CryLogAlways("$3[CryMP] Successfully cached %d objects", counter);
	}
}

void EngineCache::OnDisconnect()
{
	// flush the cache
	m_cachedCharacterInstances.clear();
	m_cachedMaterials.clear();
	m_cachedStatObjs.clear();
}
