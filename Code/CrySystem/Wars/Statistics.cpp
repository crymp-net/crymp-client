#include <StdAfx.h>

#ifdef WIN32

#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/Cry3DEngine/I3DEngine.h"
#include "CryCommon/CryRenderer/IRenderer.h"
#include "CryCommon/CrySystem/IConsole.h"
#include "CryCommon/CryAnimation/ICryAnimation.h"
#include "CryCommon/CrySystem/CryPath.h"
#include "CryCommon/CryCore/CrySizer.h"
#include "CrySizerImpl.h"
#include "CrySizerStats.h"
#include "System.h"
#include "../CryMemoryManager.h"
#include "CryCommon/CrySystem/ITestSystem.h"
#include "CryCommon/CryScriptSystem/IScriptSystem.h"
#include "CryCommon/CrySoundSystem/ISound.h"
#include "CrySoundSystem/IAudioDevice.h"
#include <ResourceCompilerHelper.h>			// CResourceCompilerHelper

// Access to some game info.
#include "CryCommon/CryGame/IGame.h"							// IGame
#include "CryCommon/CryAction/IGameFramework.h"		// IGameFramework
#include "CryCommon/CryAction/ILevelSystem.h"		// IGameFramework

#include "Psapi.h"
typedef BOOL (WINAPI *GetProcessMemoryInfoProc)( HANDLE,PPROCESS_MEMORY_COUNTERS,DWORD );

#if defined(WIN32) || defined(XENON)
#pragma pack(push,1)
const struct PEHeader_DLL
{
	DWORD signature;
	IMAGE_FILE_HEADER _head;
	IMAGE_OPTIONAL_HEADER opt_head;
	IMAGE_SECTION_HEADER *section_header;  // actual number in NumberOfSections
};
#pragma pack(pop)
#endif 


extern int CryMemoryGetAllocatedSize();

const char *g_szTestResults="TestResults";

class CResourceCollector :public IResourceCollector
{	
	struct SInstanceEntry
	{
//		AABB			m_AABB;
		uint32		m_dwFileNameId;				// use with m_FilenameToId,  m_IdToFilename
	};

	struct SAssetEntry
	{
		SAssetEntry() :m_dwInstanceCnt(0), m_dwDependencyCnt(0), m_dwMemSize(0xffffffff), m_dwFileSize(0xffffffff)
		{
		}

		string		m_sFileName;
		uint32		m_dwInstanceCnt;			// 1=this asset is used only once in the level, 2, 3, ...
		uint32		m_dwDependencyCnt;		// 1=this asset is only used by one asset, 2, 3, ...
		uint32		m_dwMemSize;					// 0xffffffff if unknown (only needed to verify disk file size)
		uint32		m_dwFileSize;					// 0xffffffff if unknown
	};

public:	// -----------------------------------------------------------------------------

	CResourceCollector()
	{
	}

	// compute m_dwDependencyCnt
	void ComputeDependencyCnt()
	{
		std::set<SDependencyPair>::const_iterator it, end=m_Dependencies.end();

		for(it=m_Dependencies.begin();it!=end;++it)
		{
			const SDependencyPair &rRef = *it;

			++m_Assets[rRef.m_idDependsOnAsset].m_dwDependencyCnt;
		}
	}

	// watch out: this function modifies internal data
	void LogData( ILog &rLog )
	{
		rLog.Log(" ");

		{
			rLog.Log("Assets:");

			std::vector<SAssetEntry>::const_iterator it, end=m_Assets.end();
			uint32 dwAssetID=0;

			for(it=m_Assets.begin();it!=end;++it,++dwAssetID)
			{
				const SAssetEntry &rRef = *it;

				rLog.Log(" A%d: inst:%5d dep:%d mem:%9d file:%9d name:%s",dwAssetID,rRef.m_dwInstanceCnt,rRef.m_dwDependencyCnt,rRef.m_dwMemSize,rRef.m_dwFileSize,rRef.m_sFileName.c_str());
			}
		}

		rLog.Log(" ");

		{
			rLog.Log("Dependencies:");

			std::set<SDependencyPair>::const_iterator it, end=m_Dependencies.end();

			uint32 dwCurrentAssetID=0xffffffff;
			uint32 dwSumFile=0;

			for(it=m_Dependencies.begin();it!=end;++it)
			{
				const SDependencyPair &rRef = *it;		

				if(rRef.m_idAsset!=dwCurrentAssetID)
				{
					if(dwSumFile!=0 && dwSumFile!=0xffffffff)
						rLog.Log("                                                ---> sum file: %d KB",(dwSumFile+1023)/1024);

					dwSumFile=0;

					rLog.Log(" ");
					rLog.Log(" A%d '%s' depends on", rRef.m_idAsset,m_Assets[rRef.m_idAsset].m_sFileName.c_str());
				}

				uint32 dwFileSize = m_Assets[rRef.m_idDependsOnAsset].m_dwFileSize;

				rLog.Log("        A%d file:%9d dep:%d '%s'",rRef.m_idDependsOnAsset,dwFileSize,m_Assets[rRef.m_idDependsOnAsset].m_dwDependencyCnt,m_Assets[rRef.m_idDependsOnAsset].m_sFileName.c_str());

				if(dwFileSize!=0xffffffff)
					dwSumFile += dwFileSize;

				dwCurrentAssetID=rRef.m_idAsset;
			}

			if(dwSumFile!=0 && dwSumFile!=0xffffffff)
				rLog.Log("                                                ---> sum file: %d KB",(dwSumFile+1023)/1024);
		}

		rLog.Log(" ");

		{
			rLog.Log("SourceAtoms:");

			std::set<SDependencyPair>::const_iterator it;

			while(!m_Dependencies.empty())
			{
				for(it=m_Dependencies.begin();it!=m_Dependencies.end();++it)
				{
					const SDependencyPair &rRef1 = *it;

					rLog.Log(" ");
					std::set<uint32> localDependencies;

					localDependencies.insert(rRef1.m_idAsset);

					RecursiveMove(rRef1.m_idAsset,localDependencies);

					PrintDependencySet(rLog,localDependencies);
					break;
				}
			}
		}
	}

	// interface IResourceCollector -------------------------------------------------

	virtual bool AddResource( const char *szFileName, const uint32 dwSize=0xffffffff )
	{
		uint32 dwNewAssetIdOrInvalid = _AddResource(szFileName,dwSize);

		return dwNewAssetIdOrInvalid!=0xffffffff;
	}

	virtual void AddInstance( const char *_szFileName, void *pInstance )
	{
		assert(pInstance);

		{
			std::set<void *>::const_iterator itInstance = m_ReportedInstances.find(pInstance);

			if(itInstance!=m_ReportedInstances.end())
				return;
		}

		string sOutputFileName = UnifyFilename(_szFileName);

		std::map<string,uint32>::const_iterator it = m_FilenameToId.find(sOutputFileName);

		if(it==m_FilenameToId.end())
		{
			OutputDebugString("ERROR: file wasn't registered with AddResource(): '");
			OutputDebugString(sOutputFileName.c_str());
			OutputDebugString("'\n");
			assert(0);		// asset wasn't registered yet AddResource() missing - unpredictable result might happen
			return;
		}

		uint32 dwAssetId = it->second;
/*
		// debug
		char str[256];
		sprintf(str,"AddInstance: %p '",pInstance);
		OutputDebugString(str);
		OutputDebugString(sOutputFileName.c_str());
		OutputDebugString("'\n");
*/
		++m_Assets[dwAssetId].m_dwInstanceCnt;
		m_ReportedInstances.insert(pInstance);
	}

	virtual void OpenDependencies( const char *_szFileName )
	{
		string sOutputFileName = UnifyFilename(_szFileName);

		std::map<string,uint32>::const_iterator it = m_FilenameToId.find(sOutputFileName);


		if(it==m_FilenameToId.end())
		{
			m_OpenedAssetId.push_back(0xffffffff);			// CloseDependencies() relies on that

			OutputDebugString("ERROR: file wasn't registered with AddResource(): '");
			OutputDebugString(sOutputFileName.c_str());
			OutputDebugString("'\n");
			assert(0);		// asset wasn't registered yet AddResource() missing - unpredictable result might happen
			return;
		}

		uint32 dwAssetId = it->second;

		m_OpenedAssetId.push_back(dwAssetId);
	}

	virtual void CloseDependencies()
	{
		assert(!m_OpenedAssetId.empty());		// internal error - OpenDependencies() should match CloseDependencies()

		m_OpenedAssetId.pop_back();
	}

private: // -----------------------------------------------------------------------

	struct SDependencyPair
	{
		SDependencyPair( const uint32 idAsset, const uint32 idDependsOnAsset ) :m_idAsset(idAsset), m_idDependsOnAsset(idDependsOnAsset)
		{
		}

		uint32			m_idAsset;							// AssetID
		uint32			m_idDependsOnAsset;			// AssetID

		bool operator<( const SDependencyPair &rhs ) const
		{
			if(m_idAsset<rhs.m_idAsset) return true;
			if(m_idAsset>rhs.m_idAsset) return false;

			return m_idDependsOnAsset<rhs.m_idDependsOnAsset;
		}
	};

	std::vector<uint32>							m_OpenedAssetId;			// to track for dependencies
	std::map<string,uint32>					m_FilenameToId;				// could be done more efficiently
	std::vector<SAssetEntry>				m_Assets;							// could be done more efficiently
	std::vector<SInstanceEntry>			m_ResourceEntries;		//
	std::set<SDependencyPair>				m_Dependencies;				//
	std::set<void *>								m_ReportedInstances;	// to avoid counting them twice

	// ---------------------------------------------------------------------

	string UnifyFilename( const char *_szFileName ) const
	{
		char *szFileName=(char *)_szFileName;

		// as bump and normal maps become combined during loading e.g.  blah.tif+blah_ddn.dds
		// the filename needs to be adjusted
		{
			char *pSearchForPlus = szFileName;

			while(*pSearchForPlus!=0 && *pSearchForPlus!='+')
				++pSearchForPlus;

			if(*pSearchForPlus=='+')
				szFileName=pSearchForPlus+1;
		}

		string sOutputFileName = CResourceCompilerHelper::GetOutputFilename(szFileName);

		sOutputFileName = PathUtil::ToUnixPath(sOutputFileName);
		sOutputFileName.MakeLower();

		return sOutputFileName;
	}

	// Returns:
	//   0xffffffff if asset was already known (m_dwInstanceCnt will be increased), AssetId otherwise
	uint32 _AddResource( const char *_szFileName, const uint32 dwSize=0xffffffff )
	{
		assert(_szFileName);

		if(_szFileName[0]==0)
			return 0xffffffff;								// no name provided - ignore this case - this often means the feature is not used

		uint32 dwNewAssetIdOrInvalid=0xffffffff;

		string sOutputFileName = UnifyFilename(_szFileName);

		std::map<string,uint32>::const_iterator it = m_FilenameToId.find(sOutputFileName);
		uint32 dwAssetId;

		if(it!=m_FilenameToId.end())
			dwAssetId = it->second;
		else
		{
			dwAssetId = m_FilenameToId.size();
			m_FilenameToId[sOutputFileName] = dwAssetId;

			SAssetEntry NewAsset;

			NewAsset.m_sFileName=sOutputFileName;

			//			if(dwSize==0xffffffff)
			{
				CCryFile file;

				if(file.Open(sOutputFileName.c_str(),"rb"))
					NewAsset.m_dwFileSize = file.GetLength();
			}

			dwNewAssetIdOrInvalid = dwAssetId;
			m_Assets.push_back(NewAsset);
		}

		SAssetEntry &rAsset = m_Assets[dwAssetId];

		if(dwSize!=0xffffffff)										// if size was specified
		{
			if(rAsset.m_dwMemSize==0xffffffff)
			{
				rAsset.m_dwMemSize=dwSize;						// store size
			}
			else
				assert(rAsset.m_dwMemSize==dwSize);		// size should always be the same
		}

//		rAsset.m_dwInstanceCnt+=dwInstanceCount;

		// debugging
//		char str[1204];
//		sprintf(str,"_AddResource %s(size=%d cnt=%d)\n",_szFileName,rAsset.m_dwInstanceCnt,rAsset.m_dwMemSize);
//		OutputDebugString(str);


		SInstanceEntry instance;

		instance.m_dwFileNameId=dwAssetId;

		AddDependencies(dwAssetId);

		m_ResourceEntries.push_back(instance);
		return dwNewAssetIdOrInvalid;
	}

	void AddDependencies( const uint32 dwPushedAssetId )
	{
		std::vector<uint32>::const_iterator it, end=m_OpenedAssetId.end();

		for(it=m_OpenedAssetId.begin();it!=end;++it)
		{
			uint32 dwOpendedAssetId = *it;

			if(dwOpendedAssetId==0xffffffff)
				continue;		//  asset wasn't registered yet AddResource() missing

			m_Dependencies.insert(SDependencyPair(dwOpendedAssetId,dwPushedAssetId));
		}
	}

	void PrintDependencySet( ILog &rLog, const std::set<uint32> &Dep )
	{
		rLog.Log("      {");
		std::set<uint32>::const_iterator it, end=Dep.end();
		uint32 dwSumFile=0;

		// iteration could be optimized
		for(it=Dep.begin();it!=end;++it)
		{
			uint32 idAsset = *it;

			uint32 dwFileSize = m_Assets[idAsset].m_dwFileSize;

			if(dwFileSize!=0xffffffff)
				dwSumFile += dwFileSize;

			rLog.Log("        A%d file:%9d dep:%d '%s'",idAsset,m_Assets[idAsset].m_dwFileSize,m_Assets[idAsset].m_dwDependencyCnt,m_Assets[idAsset].m_sFileName.c_str());
		}
		rLog.Log("      }                                           ---> sum file: %d KB",(dwSumFile+1023)/1024);
	}

	// find all dependencies to it and move to localDependencies
	void RecursiveMove( const uint32 dwCurrentAssetID, std::set<uint32> &localDependencies )
	{
		bool bProcess=true;

		// iteration could be optimized
		while(bProcess)
		{
			bProcess=false;

			std::set<SDependencyPair>::iterator it;

			for(it=m_Dependencies.begin();it!=m_Dependencies.end();++it)
			{
				SDependencyPair Pair = *it;

				if(Pair.m_idAsset==dwCurrentAssetID || Pair.m_idDependsOnAsset==dwCurrentAssetID)
				{
					uint32 idAsset = (Pair.m_idAsset==dwCurrentAssetID) ? Pair.m_idDependsOnAsset : Pair.m_idAsset;

					localDependencies.insert(idAsset);

					m_Dependencies.erase(it);

					RecursiveMove(idAsset,localDependencies);
					bProcess=true;
					break;
				}
			}
		}
	}

	friend class CStatsToExcelExporter;
};


#define MAX_LODS 6

//////////////////////////////////////////////////////////////////////////
// Statistics about currently loaded level.
//////////////////////////////////////////////////////////////////////////
struct SCryEngineStats
{
	struct StatObjInfo
	{
		int nVertices;
		int nIndices;
		int nIndicesPerLod[MAX_LODS];
		int nMeshSize;
		int nTextureSize;
		int nPhysProxySize;
		int nPhysPrimitives;
		int nLods;
		int nSubMeshCount;
		IStatObj *pStatObj;
	};
	struct CharacterInfo
	{
		CharacterInfo() :nVertices(0), nIndices(0), nMeshSize(0), nTextureSize(0), nLods(0), nInstances(0), pModel(0)
		{
		}
		
		int nVertices;
		int nIndices;
		int nIndicesPerLod[MAX_LODS];
		int nMeshSize;
		int nTextureSize;
		int nLods;
		int nInstances;
		ICharacterModel *pModel;
	};
	struct MeshInfo
	{
		int nVerticesSum;
		int nIndicesSum;
		int nCount;
		int nMeshSize;
		int nTextureSize;
		const char *name;
	};
	struct ModuleInfo
	{
		string name;
		CryModuleMemoryInfo memInfo;
		uint32 moduleStaticSize;
		uint32 usedInModule;
		uint32 SizeOfCode;
		uint32 SizeOfInitializedData;
		uint32 SizeOfUninitializedData;
	};
	struct MemInfo
	{
		MemInfo() : m_pSizer(0), m_pStats(0) {}
		~MemInfo() {SAFE_DELETE(m_pSizer); SAFE_DELETE(m_pStats); }

		int totalStatic;
		int totalUsedInModules;
		int totalUsedInModulesStatic;
		int countedMemoryModules;
		uint64 totalAllocatedInModules;
		int totalNumAllocsInModules;
		CrySizerImpl* m_pSizer;
		CrySizerStats* m_pStats;

		std::vector<ModuleInfo> modules;
	};

	struct ProfilerInfo
	{
		string m_name;
		string m_module;

		//! Total time spent in this counter including time of child profilers in current frame.
		float m_totalTime;
		//! Self frame time spent only in this counter (But includes recursive calls to same counter) in current frame.
		int64 m_selfTime;
		//! How many times this profiler counter was executed.
		int m_count;
		//! Total time spent in this counter during all profiling period.
		int64 m_sumTotalTime;
		//! Total self time spent in this counter during all profiling period.
		int64 m_sumSelfTime;
		//! Displayed quantity (interpolated or avarage).
		float m_displayedValue;
		//! Displayed quantity (current frame value).
		float m_displayedCurrentValue;
		//! How variant this value.
		float m_variance;
		// min value
		float m_min;
		// max value
		float m_max;

		int m_mincount;

		int m_maxcount;

	};

	struct SPeakProfilerInfo
	{
		ProfilerInfo profiler;
		float peakValue;
		float avarageValue;
		float variance;
		int pageFaults; // Number of page faults at this frame.
		int count;  // Number of times called for peak.
		float when; // when it added.
	};

	//int nMemMeshTotalSize;
	//int nMemCharactersTotalSize;
	//int nMemTexturesTotalSize;

	uint64 nWin32_WorkingSet;
	uint64 nWin32_PeakWorkingSet;
	uint64 nWin32_PagefileUsage;
	uint64 nWin32_PeakPagefileUsage;
	uint64 nWin32_PageFaultCount;

	uint32 nTotalAllocatedMemory;

	uint32 nSummaryScriptSize;
	uint32 nSummaryCharactersSize;
	uint32 nSummaryMeshCount;
	uint32 nSummaryMeshSize;
	
	uint32 nAPI_MeshSize;    // Allocated by DirectX
	
	uint32 nSummary_TextureSize;       // Total size of all textures
	uint32 nSummary_UserTextureSize;   // Size of eser textures, (from files...)
	uint32 nSummary_EngineTextureSize; // Dynamic Textures

	uint32 nStatObj_SummaryTextureSize;
	uint32 nStatObj_SummaryMeshSize;
	uint32 nStatObj_TotalCount; // Including sub-objects.

	uint32 nChar_SummaryMeshSize;
	uint32 nChar_SummaryTextureSize;
	uint32 nChar_NumInstances;

	float fLevelLoadTime;
	SDebugFPSInfo infoFPS;


	std::vector<StatObjInfo> objects;
	std::vector<CharacterInfo> characters;
	std::vector<ITexture*> textues;
	std::vector<MeshInfo> meshes;
  std::vector<IMaterial*> materials;
	std::vector<IWavebank*> wavebanks;
	std::vector<ISoundProfileInfo*> soundinfos;
	std::vector<ProfilerInfo> profilers;
	std::vector<SPeakProfilerInfo> peaks;
	std::vector<SAnimationStatistics> animations;

	MemInfo memInfo;
};

inline bool CompareFrameProfilersValue( const SCryEngineStats::ProfilerInfo &p1,const SCryEngineStats::ProfilerInfo &p2 )
{
	return p1.m_displayedValue > p2.m_displayedValue;
}


enum EEngineStatsFlags
{
};

class CEngineStats
{
public:
	// Collect all stats.
	// @see EEngineStatsFlags
	void Collect( int nFlags );

private: // ----------------------------------------------------------------------------

	void CollectGeometry();
	void CollectGeometryForObject( IStatObj *pObj,SCryEngineStats::StatObjInfo &si,CrySizerImpl &statObjTextureSizer,CrySizerImpl &textureSizer );
	void CollectCharacters();
	void CollectMaterialDependencies();
	void CollectTextures();
  void CollectMaterials();
	void CollectVoxels();
	void CollectRenderMeshes();
	void CollectWavebanks();
	void CollectSoundInfos();
	void CollectEntityDependencies();
	void CollectMemInfo();
	void CollectProfileStatistics();
	void CollectAnimations();

	// Arguments:
	//   pObj - 0 is ignored
	//   pMat - 0 if IStatObjet Material should be used
	void AddResource_StatObjWithLODs( IStatObj *pObj, CrySizerImpl &statObjTextureSizer, IMaterial *pMat=0 );
	//
	void AddResource_SingleStatObj( IStatObj &rData );
	//
	void AddResource_CharInstance( ICharacterInstance &rData );
	//
	void AddResource_Material( IMaterial &rData, const bool bSubMaterial=false );


	CResourceCollector		m_ResourceCollector;		// dependencies between assets 
	SCryEngineStats				m_stats;								//
	int										m_nFlags;								//

	friend static void SaveLevelStats( IConsoleCmdArgs *pArgs );
};

//////////////////////////////////////////////////////////////////////////
void CEngineStats::Collect( int nFlags )
{
	m_nFlags = nFlags;
	ISystem *pSystem = GetISystem();
	I3DEngine *p3DEngine = pSystem->GetI3DEngine();
	IRenderer *pRenderer = pSystem->GetIRenderer();

	memset(&m_stats,0,sizeof(m_stats));

	m_stats.nTotalAllocatedMemory = CryMemoryGetAllocatedSize();
	m_stats.nSummaryMeshSize =  *((int*)pRenderer->EF_Query(EFQ_Alloc_Mesh_SysMem));
	m_stats.nSummaryMeshCount =  *((int*)pRenderer->EF_Query(EFQ_Mesh_Count));
	m_stats.nAPI_MeshSize =  *((int*)pRenderer->EF_Query(EFQ_Alloc_APIMesh));
	m_stats.nSummaryScriptSize = pSystem->GetIScriptSystem()->GetScriptAllocSize();

	m_stats.nWin32_WorkingSet = 0;
	m_stats.nWin32_PeakWorkingSet = 0;
	m_stats.nWin32_PagefileUsage = 0;
	m_stats.nWin32_PeakPagefileUsage = 0;
	m_stats.nWin32_PageFaultCount = 0;
	{
		HMODULE hPSAPI = LoadLibrary("psapi.dll");
		if (hPSAPI)
		{
			GetProcessMemoryInfoProc pGetProcessMemoryInfo = (GetProcessMemoryInfoProc)GetProcAddress(hPSAPI, "GetProcessMemoryInfo");
			if (pGetProcessMemoryInfo)
			{
				PROCESS_MEMORY_COUNTERS pc;
				HANDLE hProcess = GetCurrentProcess();
				pc.cb = sizeof(pc);
				pGetProcessMemoryInfo( hProcess, &pc, sizeof(pc) );

				m_stats.nWin32_WorkingSet = pc.WorkingSetSize;
				m_stats.nWin32_PeakWorkingSet = pc.PeakWorkingSetSize;
				m_stats.nWin32_PagefileUsage = pc.PagefileUsage;
				m_stats.nWin32_PeakPagefileUsage = pc.PeakPagefileUsage;
				m_stats.nWin32_PageFaultCount = pc.PageFaultCount;
			}
		}
	}

	m_stats.fLevelLoadTime = 0;

	if (gEnv->pGame)
		m_stats.fLevelLoadTime = gEnv->pGame->GetIGameFramework()->GetILevelSystem()->GetLastLevelLoadTime();

//	I3DEngine *p3DEngine = pSystem->GetI3DEngine();
//	SDebugFPSInfo infoFPS;
	m_stats.infoFPS.fAverageFPS = 0.0f;
	m_stats.infoFPS.fMaxFPS = 0.0f;
	m_stats.infoFPS.fMinFPS = 0.0f;
	m_stats.infoFPS.averageDrawCalls = 0;
	m_stats.infoFPS.maxDrawCalls = 0;
	m_stats.infoFPS.minDrawCalls = 0;
	m_stats.infoFPS.averagePolys = 0;
	m_stats.infoFPS.maxPolys = 0;
	m_stats.infoFPS.minPolys = 0;

	if (p3DEngine) {
		p3DEngine->FillDebugFPSInfo(m_stats.infoFPS);
	}

	//////////////////////////////////////////////////////////////////////////
	// Collect CGFs
	//////////////////////////////////////////////////////////////////////////
	CollectMemInfo(); // First of all collect memory info for modules (must be first).
	CollectGeometry();
	CollectCharacters();
	CollectTextures();
  CollectMaterials();
	CollectRenderMeshes();
	CollectWavebanks();
	CollectSoundInfos();
	CollectMaterialDependencies();
	CollectEntityDependencies();
	CollectProfileStatistics();
	CollectAnimations();
}

inline bool CompareMaterialsByName( IMaterial *pMat1,IMaterial *pMat2 )
{
  return pMat1->GetName() > pMat2->GetName();
}

inline bool CompareTexturesBySizeFunc( ITexture *pTex1,ITexture *pTex2 )
{
	return pTex1->GetDeviceDataSize() > pTex2->GetDeviceDataSize();
}
inline bool CompareStatObjBySizeFunc( const SCryEngineStats::StatObjInfo &s1,const SCryEngineStats::StatObjInfo &s2 )
{
	return (s1.nMeshSize+s1.nTextureSize) > (s2.nMeshSize+s2.nTextureSize);
}
inline bool CompareCharactersBySizeFunc( const SCryEngineStats::CharacterInfo &s1,const SCryEngineStats::CharacterInfo &s2 )
{
	return (s1.nMeshSize+s1.nTextureSize) > (s2.nMeshSize+s2.nTextureSize);
}
inline bool CompareWavebanksBySizeFunc( IWavebank *pWB1,IWavebank *pWB2 )
{
	return pWB1->GetInfo()->nFileSize > pWB2->GetInfo()->nFileSize;
}
inline bool CompareSoundInfosByTimesPlayedFunc( ISoundProfileInfo *pSI1, ISoundProfileInfo *pSI2 )
{
	return pSI1->GetInfo()->nTimesPlayed > pSI2->GetInfo()->nTimesPlayed;
}
inline bool CompareRenderMeshByTypeName( IRenderMesh *pRM1,IRenderMesh *pRM2 )
{
	return strcmp( pRM1->GetTypeName(),pRM2->GetTypeName() ) < 0;
}


void CEngineStats::AddResource_SingleStatObj( IStatObj &rData )
{
	if(!m_ResourceCollector.AddResource(rData.GetFilePath()))
		return;		// was already registered

	// dependencies

	if(rData.GetMaterial())
	{
		m_ResourceCollector.OpenDependencies(rData.GetFilePath());

		AddResource_Material(*rData.GetMaterial());

		m_ResourceCollector.CloseDependencies();
	}
}

void CEngineStats::AddResource_CharInstance( ICharacterInstance &rData )
{
	IMaterial *pMat = rData.GetMaterial();

	if(!m_ResourceCollector.AddResource(rData.GetFilePath()))
		return;		// was already registered

	// dependencies

	if(rData.GetMaterial())
	{
		m_ResourceCollector.OpenDependencies(rData.GetFilePath());

		AddResource_Material(*rData.GetMaterial());

		m_ResourceCollector.CloseDependencies();
	}
}


void CEngineStats::AddResource_Material( IMaterial &rData, const bool bSubMaterial )
{
	if(!bSubMaterial)
	{
		string sName = string(rData.GetName())+".mtl";

		if(!m_ResourceCollector.AddResource(sName))
			return; // was already registered

		// dependencies

		m_ResourceCollector.OpenDependencies(sName);
	}

	{
		SShaderItem &rItem = rData.GetShaderItem();

		uint32 dwSubMatCount = rData.GetSubMtlCount();

		for(uint32 dwSubMat=0;dwSubMat<dwSubMatCount;++dwSubMat)
		{
			IMaterial *pSub = rData.GetSubMtl(dwSubMat);

			if(pSub)
				AddResource_Material(*pSub,true);		
		}

		// this material
		if(rItem.m_pShaderResources)
			for(uint32 dwI=0;dwI<EFTT_MAX;++dwI)
			{
				SEfResTexture *pTex = rItem.m_pShaderResources->GetTexture(dwI);

				if(!pTex)
					continue;

				uint32 dwSize=0xffffffff;
				/*
				if(pTex->m_Sampler.m_pITex)
				{
				dwSize = pTex->m_Sampler.m_pITex->GetDeviceDataSize();

				assert(pTex->m_Name);
				assert(pTex->m_Sampler.m_pITex->GetName());

				string sTex = PathUtil::ToUnixPath(PathUtil::ReplaceExtension(pTex->m_Name,""));
				string sSampler = PathUtil::ToUnixPath(PathUtil::ReplaceExtension(pTex->m_Sampler.m_pITex->GetName(),""));

				if(stricmp(sTex.c_str(),sSampler.c_str())!=0)
				{
				char str[1024];

				sprintf(str,"IGNORE  '%s' '%s'\n",sTex.c_str(),sSampler.c_str());
				OutputDebugString(str);
				dwSize=0;
				//				IGNORE  'Textures/gradf' 'Editor/Objects/gradf'
				//				IGNORE  'textures/cubemaps/auto_cubemap' '$RT_CM'
				//				IGNORE  '' 'Textures/Defaults/White_ddn'
				//				IGNORE  '' 'textures/defaults/oceanwaves_ddn'
				//				IGNORE  '' 'textures/sprites/fire_blur1_ddn'
				//				IGNORE  '' 'textures/sprites/fire_blur1_ddn'
				//				IGNORE  '' 'Game/Objects/Library/Barriers/Sandbags/sandbags_ddn'
				//				IGNORE  '' 'objects/characters/human/us/nanosuit/nanosuit_ddn'
				//				IGNORE  '' 'objects/characters/human/us/nanosuit/nanosuit_ddndif'
				}

				//		if(dwI==EFTT_NORMALMAP && rItem.m_pShaderResources->m_Textures[EFTT_BUMP])
				//			dwSize=0;			// if there is a _bump texture, we don't make use of the ddn any more, so not space is wasted for this but we still need to report the file

				m_ResourceCollector.AddResource(pTex->m_Sampler.m_pITex->GetName(),dwSize);		// used texture
				}
				else
				*/			
				//		CryLog("AddResource ITex (%d): '%s' '%s'",dwI,pTex->m_Name.c_str(),pTex->m_Sampler.m_pITex->GetName());

				if(pTex->m_Sampler.m_pITex)
					m_ResourceCollector.AddResource(pTex->m_Sampler.m_pITex->GetName(),dwSize);
				//		m_ResourceCollector.AddResource(pTex->m_Name,dwSize);
			}
	}


	if(!bSubMaterial)
		m_ResourceCollector.CloseDependencies();
}



//////////////////////////////////////////////////////////////////////////
void CEngineStats::CollectGeometryForObject( IStatObj *pStatObj,SCryEngineStats::StatObjInfo &si,CrySizerImpl &statObjTextureSizer,CrySizerImpl &textureSizer )
{
	IStatObj *pLod0 = 0;
	for (int lod = 0; lod < MAX_LODS; lod++)
	{
		IStatObj *pLod = pStatObj->GetLodObject(lod);
		if (pLod)
		{
			if (!pLod0 && pLod->GetRenderMesh())
				pLod0 = pLod;

			if (lod > 0 && lod+1 > si.nLods) // Assign last existing lod.
				si.nLods = lod+1;
			if (pLod->GetRenderMesh())
			{
				si.nIndicesPerLod[lod] += pLod->GetRenderMesh()->GetSysIndicesCount();
				si.nMeshSize += pLod->GetRenderMesh()->GetMemoryUsage(0,IRenderMesh::MEM_USAGE_COMBINED);
			}
		}
	}

	if (pLod0 && pLod0->GetRenderMesh())
	{
		IRenderMesh *pRenderMesh = pLod0->GetRenderMesh();
		
		pRenderMesh->GetTextureMemoryUsage(0,&statObjTextureSizer);
		pRenderMesh->GetTextureMemoryUsage(0,&textureSizer);
		
		si.nVertices += pRenderMesh->GetVertCount();
		si.nIndices += pRenderMesh->GetSysIndicesCount();
	}

	for(int j = 0; j < 4; j++)
	{
		if (pStatObj->GetPhysGeom(j))
		{
			CrySizerImpl physMeshSizer;
			pStatObj->GetPhysGeom(j)->pGeom->GetMemoryStatistics(&physMeshSizer);
			si.nPhysProxySize += physMeshSizer.GetTotalSize(); 
			si.nPhysPrimitives += pStatObj->GetPhysGeom(j)->pGeom->GetPrimitiveCount();
		}
	}
}


void CEngineStats::AddResource_StatObjWithLODs( IStatObj *pObj, CrySizerImpl &statObjTextureSizer, IMaterial *pMat )
{
	if(!pObj)
		return;

	SCryEngineStats::StatObjInfo si;

	si.pStatObj = pObj;

	memset(si.nIndicesPerLod,0,sizeof(si.nIndicesPerLod));

	// dependencies
	AddResource_SingleStatObj(*si.pStatObj);

	CrySizerImpl textureSizer;

	si.nLods = 0;
	si.nSubMeshCount = 0;
	// Analyze geom object.

	bool bMultiSubObj = (si.pStatObj->GetFlags() & STATIC_OBJECT_COMPOUND) != 0;

	si.nMeshSize = 0;
	si.nTextureSize = 0;
	si.nIndices = 0;
	si.nVertices = 0;
	si.nPhysProxySize = 0;
	si.nPhysPrimitives = 0;

	m_stats.nStatObj_TotalCount++;
	std::set<void*> addedObjects;

	//////////////////////////////////////////////////////////////////////////
	// Iterate LODs.
	//////////////////////////////////////////////////////////////////////////
	if (!bMultiSubObj)
	{
		CollectGeometryForObject( si.pStatObj,si,statObjTextureSizer,textureSizer );
	}
	else
	{
		for (int k = 0; k < si.pStatObj->GetSubObjectCount(); k++)
		{
			//if (si.pStatObj->GetSubObject(k)->nType == STATIC_SUB_OBJECT_MESH)
			{
				if (!si.pStatObj->GetSubObject(k))
					continue;
				IStatObj *pObj = si.pStatObj->GetSubObject(k)->pStatObj;

				if (addedObjects.find(pObj) != addedObjects.end())
					continue;
				addedObjects.insert(pObj);

				// dependencies
				if(pObj)
				{
					AddResource_SingleStatObj(*pObj);

					CollectGeometryForObject( pObj,si,statObjTextureSizer,textureSizer );
				}
			}
		}
	}

	si.nTextureSize = textureSizer.GetTotalSize();

	m_stats.nStatObj_SummaryMeshSize += si.nMeshSize;
	m_stats.objects.push_back(si); 
}


inline bool CompareAnimations( const SAnimationStatistics &p1,const SAnimationStatistics &p2 )
{
	return p1.count > p2.count;
}

//////////////////////////////////////////////////////////////////////////
void CEngineStats::CollectAnimations()
{
	ISystem *pSystem = GetISystem();
	I3DEngine *p3DEngine = pSystem->GetI3DEngine();

	m_stats.animations.clear();

	size_t count = pSystem->GetIAnimationSystem()->GetIAnimEvents()->GetGlobalAnimCount();

	//m_stats.animations.reserve(count);
	for (size_t i = 0; i < count; ++i) {
		SAnimationStatistics stat;
		pSystem->GetIAnimationSystem()->GetIAnimEvents()->GetGlobalAnimStatistics(i, stat);		
		if (stat.count)
			m_stats.animations.push_back(stat);
	}

	std::sort( m_stats.animations.begin(),m_stats.animations.end(),CompareAnimations );
}

//////////////////////////////////////////////////////////////////////////
void CEngineStats::CollectGeometry()
{
	ISystem *pSystem = GetISystem();
	I3DEngine *p3DEngine = pSystem->GetI3DEngine();

	m_stats.nStatObj_SummaryTextureSize = 0;
	m_stats.nStatObj_SummaryMeshSize = 0;
	m_stats.nStatObj_TotalCount = 0;


	CrySizerImpl statObjTextureSizer;


	// iterate through all IStatObj
	{
		int nObjCount = 0;

		p3DEngine->GetLoadedStatObjArray(0,nObjCount);
		if (nObjCount > 0)
		{
			m_stats.objects.reserve(nObjCount);
			IStatObj **pObjects = new IStatObj*[nObjCount];
			p3DEngine->GetLoadedStatObjArray(pObjects,nObjCount);
			
			for (int nCurObj = 0; nCurObj < nObjCount; nCurObj++)
				AddResource_StatObjWithLODs(pObjects[nCurObj],statObjTextureSizer,0);

			delete []pObjects;
		}
	}


	// iterate through all instances
	{
		std::vector<IRenderNode*>	lstInstances;

		uint32 dwCount=0;

		dwCount+=p3DEngine->GetObjectsByType(eERType_Brush);
		dwCount+=p3DEngine->GetObjectsByType(eERType_Vegetation);
		dwCount+=p3DEngine->GetObjectsByType(eERType_Light);
		dwCount+=p3DEngine->GetObjectsByType(eERType_Decal);

		if(dwCount)
		{
			lstInstances.resize(dwCount);dwCount=0;

			dwCount+=p3DEngine->GetObjectsByType(eERType_Brush,&lstInstances[dwCount]);
			dwCount+=p3DEngine->GetObjectsByType(eERType_Vegetation,&lstInstances[dwCount]);
			dwCount+=p3DEngine->GetObjectsByType(eERType_Light,&lstInstances[dwCount]);
			dwCount+=p3DEngine->GetObjectsByType(eERType_Decal,&lstInstances[dwCount]);

			IRenderNode **pNode = &lstInstances[0];

			for(uint32 dwI=0;dwI<dwCount;++dwI,++pNode)
			{
				IRenderNode &rNode = **pNode;

				for(uint32 dwSlot=0;dwSlot<100;++dwSlot)			// 100 should be enough (according to Timur)
				{
					if(IStatObj *pEntObject = rNode.GetEntityStatObj(dwSlot))
					{
						m_ResourceCollector.AddInstance(pEntObject->GetFilePath(),&rNode);

						if(IMaterial *pMat = rNode.GetMaterial())		// if this rendernode overwrites the IStatObj material
						{
							m_ResourceCollector.OpenDependencies(pEntObject->GetFilePath());
							AddResource_Material(*pMat);		// to report the dependencies of this instance to the IStatObj
							m_ResourceCollector.CloseDependencies();
						}
					}

					if(ICharacterInstance *pCharInst = rNode.GetEntityCharacter(dwSlot))
						m_ResourceCollector.AddInstance(pCharInst->GetFilePath(),&rNode);
				}
			}
		}
	}

	m_stats.nStatObj_SummaryTextureSize += statObjTextureSizer.GetTotalSize();
	std::sort( m_stats.objects.begin(),m_stats.objects.end(),CompareStatObjBySizeFunc );
}

//////////////////////////////////////////////////////////////////////////
void CEngineStats::CollectCharacters()
{
	ISystem *pSystem = GetISystem();
	I3DEngine *p3DEngine = pSystem->GetI3DEngine();

	m_stats.nChar_SummaryTextureSize = 0;
	m_stats.nChar_SummaryMeshSize = 0;
	m_stats.nChar_NumInstances = 0;

	CrySizerImpl totalCharactersTextureSizer;

	int nObjCount = 0;
	pSystem->GetIAnimationSystem()->GetLoadedModels(0,nObjCount);
	if (nObjCount > 0)
	{
		m_stats.characters.reserve(nObjCount);
		ICharacterModel **pObjects = new ICharacterModel*[nObjCount];
		pSystem->GetIAnimationSystem()->GetLoadedModels(pObjects,nObjCount);
		for (int nCurObj = 0; nCurObj < nObjCount; nCurObj++)
		{
			if (!pObjects[nCurObj])
				continue;

			// Do not consider cga files characters (they are already considered to be static geometries)
			if (stricmp(PathUtil::GetExt(pObjects[nCurObj]->GetModelFilePath()),"cga") == 0)
				continue;

			SCryEngineStats::CharacterInfo si;
			si.pModel = pObjects[nCurObj];
			if (!si.pModel)
				continue;

			// dependencies
//			AddResource(*si.pModel);

			CrySizerImpl textureSizer;

			si.nInstances = si.pModel->GetNumInstances();
			si.nLods = si.pModel->GetNumLods();
			si.nIndices = 0;
			si.nVertices = 0;
			memset(si.nIndicesPerLod,0,sizeof(si.nIndicesPerLod));

			CrySizerImpl meshSizer;

			si.nMeshSize = si.pModel->GetMeshMemoryUsage(&meshSizer);
			si.pModel->GetTextureMemoryUsage( &textureSizer );
			si.pModel->GetTextureMemoryUsage( &totalCharactersTextureSizer );
			bool bLod0_Found = false;
			for (int nlod = 0; nlod < MAX_LODS; nlod++)
			{
				IRenderMesh *pRenderMesh = si.pModel->GetRenderMesh(nlod);
				if (pRenderMesh)
				{
					if (!bLod0_Found)
					{
						bLod0_Found = true;
						si.nVertices = pRenderMesh->GetVertCount();
						si.nIndices = pRenderMesh->GetSysIndicesCount();
					}
					si.nIndicesPerLod[nlod] = pRenderMesh->GetSysIndicesCount();
				}
			}
		
			si.nTextureSize = textureSizer.GetTotalSize();

			m_stats.nChar_SummaryMeshSize += si.nMeshSize;
			m_stats.characters.push_back(si);

			m_stats.nChar_NumInstances += si.nInstances;
		}
		delete []pObjects;
	}
	m_stats.nChar_SummaryTextureSize = totalCharactersTextureSizer.GetTotalSize();
	std::sort( m_stats.characters.begin(),m_stats.characters.end(),CompareCharactersBySizeFunc );
}

//////////////////////////////////////////////////////////////////////////
void CEngineStats::CollectEntityDependencies()
{
	ISystem *pSystem = GetISystem();

	IEntitySystem *pEntitySystem = pSystem->GetIEntitySystem();

	IEntityItPtr it = pEntitySystem->GetEntityIterator();
	while ( !it->IsEnd() )
	{
		IEntity *pEntity = it->Next();

		IMaterial *pMat=pEntity->GetMaterial();

		if(pMat)
			AddResource_Material(*pMat);

		uint32 dwSlotCount = pEntity->GetSlotCount();

		for(uint32 dwI=0;dwI<dwSlotCount;++dwI)
		{
			SEntitySlotInfo slotInfo;

			if(pEntity->GetSlotInfo(dwI,slotInfo))
			{
				if(slotInfo.pMaterial)
					AddResource_Material(*slotInfo.pMaterial);

				if(slotInfo.pCharacter)
					AddResource_CharInstance(*slotInfo.pCharacter);
			}
		}
	}	
}


//////////////////////////////////////////////////////////////////////////
void CEngineStats::CollectMaterialDependencies()
{
	ISystem *pSystem = GetISystem();
	I3DEngine *p3DEngine = pSystem->GetI3DEngine();

	IMaterialManager *pManager = p3DEngine->GetMaterialManager();

	uint32 nObjCount=0;
	pManager->GetLoadedMaterials(0,nObjCount);

	if(nObjCount>0)
	{
		std::vector<IMaterial *> Materials;

		Materials.resize(nObjCount);

		pManager->GetLoadedMaterials(&Materials[0],nObjCount);
	
		std::vector<IMaterial *>::const_iterator it, end=Materials.end();

		for(it=Materials.begin();it!=end;++it)
		{
			IMaterial *pMat = *it;

			AddResource_Material(*pMat);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEngineStats::CollectTextures()
{
	ISystem *pSystem = GetISystem();
	IRenderer *pRenderer = pSystem->GetIRenderer();

	m_stats.nSummary_TextureSize = 0;
	m_stats.nSummary_UserTextureSize = 0;
	m_stats.nSummary_EngineTextureSize = 0;

	m_stats.textues.clear();
	int nTexCount = *(int*)pRenderer->EF_Query(EFQ_GetAllTextures,0);
	if (nTexCount > 0)
	{
		m_stats.textues.reserve(nTexCount);
		ITexture **pTextures = new ITexture*[nTexCount];
		pRenderer->EF_Query( EFQ_GetAllTextures,(INT_PTR)pTextures );
		for (int i = 0; i < nTexCount; i++)
		{
			ITexture *pTexture = pTextures[i];
			int nTexSize = pTexture->GetDeviceDataSize();
			if (nTexSize > 0)
			{
				m_stats.textues.push_back( pTexture );

				m_stats.nSummary_TextureSize += nTexSize;

				// Dynamic texture.
				if (pTexture->GetName()[0] == '$')
				{
					m_stats.nSummary_EngineTextureSize += nTexSize;
				}
				else
				{
					m_stats.nSummary_UserTextureSize += nTexSize;
				}
			}
		}
		delete []pTextures;

		std::sort( m_stats.textues.begin(),m_stats.textues.end(),CompareTexturesBySizeFunc );
	}
}

//////////////////////////////////////////////////////////////////////////
void CEngineStats::CollectMaterials()
{
  ISystem *pSystem = GetISystem();
  I3DEngine *p3DEngine = pSystem->GetI3DEngine();

  IMaterialManager *pManager = p3DEngine->GetMaterialManager();

  uint32 nObjCount=0;
  pManager->GetLoadedMaterials(0,nObjCount);

  m_stats.materials.resize(nObjCount);

  if(nObjCount>0)
  {
    pManager->GetLoadedMaterials(&m_stats.materials[0],nObjCount);

    std::sort( m_stats.materials.begin(),m_stats.materials.end(),CompareMaterialsByName );
  }
}

//////////////////////////////////////////////////////////////////////////
void CEngineStats::CollectRenderMeshes()
{
	ISystem *pSystem = GetISystem();
	IRenderer *pRenderer = pSystem->GetIRenderer();

	m_stats.meshes.clear();
	m_stats.meshes.reserve(100);

	int nVertices = 0;
	int nIndices = 0;

	int nCount = *(int*)pRenderer->EF_Query(EFQ_GetAllMeshes,0);
	if (nCount > 0)
	{
		IRenderMesh **pMeshes = new IRenderMesh*[nCount];
		pRenderer->EF_Query( EFQ_GetAllMeshes,(INT_PTR)pMeshes );
		
		// Sort meshes by name.
		std::sort( pMeshes,pMeshes+nCount,CompareRenderMeshByTypeName );

		CrySizerImpl* pTextureSizer = new CrySizerImpl();

		int nInstances = 0;
		int nMeshSize = 0;
		const char *sMeshName = 0;
		const char *sLastMeshName = "";
		for (int i = 0; i < nCount; i++)
		{
			IRenderMesh *pMesh = pMeshes[i];
			sMeshName = pMesh->GetTypeName();

			if ((strcmp(sMeshName,sLastMeshName) != 0 && i != 0))
			{
				SCryEngineStats::MeshInfo mi;
				mi.nCount = nInstances;
				mi.nVerticesSum = nVertices;
				mi.nIndicesSum = nIndices;
				mi.nMeshSize = nMeshSize;
				mi.nTextureSize = (int)pTextureSizer->GetTotalSize();
				mi.name = sLastMeshName;
				m_stats.meshes.push_back(mi);

				delete pTextureSizer;
				pTextureSizer = new CrySizerImpl();
				nInstances = 0;
				nMeshSize = 0;
				nVertices = 0;
				nIndices = 0;
			}
			sLastMeshName = sMeshName;
			nMeshSize += pMesh->GetMemoryUsage(0,IRenderMesh::MEM_USAGE_COMBINED); // Collect System+Video memory usage.
			pMesh->GetTextureMemoryUsage(pMesh->GetMaterial(),pTextureSizer);

			nVertices += pMesh->GetVertCount();
			nIndices += pMesh->GetSysIndicesCount();

			nInstances++;
		}
		if (nCount > 0 && sMeshName)
		{
			SCryEngineStats::MeshInfo mi;
			mi.nCount = nInstances;
			mi.nVerticesSum = nVertices;
			mi.nIndicesSum = nIndices;
			mi.nMeshSize = nMeshSize;
			mi.nTextureSize = (int)pTextureSizer->GetTotalSize();
			mi.name = sMeshName;
			m_stats.meshes.push_back(mi);
		}

		delete pTextureSizer;

		delete []pMeshes;
	}
}

//////////////////////////////////////////////////////////////////////////
void CEngineStats::CollectVoxels()
{
	//int nCount = 10000;
	//IRenderNode *pRenderNodes = NULL;
	
	//virtual void GetVoxelRenderNodes(struct IRenderNode**pRenderNodes, int & nCount) = 0;
}

//////////////////////////////////////////////////////////////////////////
void CEngineStats::CollectWavebanks()
{
	ISystem *pSystem = GetISystem();
	ISoundSystem *pSoundSystem = pSystem->GetISoundSystem();

	if (!pSoundSystem)
		return;

	if(!pSoundSystem->GetIAudioDevice())
		return;															// rare condition (one device exists but cannot be initialized)

	m_stats.wavebanks.clear();

	int nWavebankCount = pSoundSystem->GetIAudioDevice() ? pSoundSystem->GetIAudioDevice()->GetWavebankCount() : 0;
	if (nWavebankCount > 0)
	{
		m_stats.wavebanks.reserve(nWavebankCount);

		for (int i=0; i<nWavebankCount; ++i)
		{
			IWavebank *pWavebank = pSoundSystem->GetIAudioDevice()->GetWavebank(i);
			m_stats.wavebanks.push_back(pWavebank);
		}

		std::sort( m_stats.wavebanks.begin(),m_stats.wavebanks.end(),CompareWavebanksBySizeFunc );
	}
	
}

//////////////////////////////////////////////////////////////////////////
void CEngineStats::CollectSoundInfos()
{
	ISystem *pSystem = GetISystem();
	ISoundSystem *pSoundSystem = pSystem->GetISoundSystem();

	if (!pSoundSystem)
		return;

	m_stats.soundinfos.clear();

	int nSoundCount = pSoundSystem->GetSoundInfoCount();
	if (nSoundCount > 0)
	{
		m_stats.soundinfos.reserve(nSoundCount);

		for (int i=0; i<nSoundCount; ++i)
		{
			ISoundProfileInfo *pSoundInfo = pSoundSystem->GetSoundInfo(i);
			m_stats.soundinfos.push_back(pSoundInfo);
		}

		std::sort( m_stats.soundinfos.begin(), m_stats.soundinfos.end(),CompareSoundInfosByTimesPlayedFunc );
	}

}

//////////////////////////////////////////////////////////////////////////
void CEngineStats::CollectProfileStatistics()
{
	ISystem *pSystem = GetISystem();
	IFrameProfileSystem* pProfiler = pSystem->GetIProfileSystem();
	
	m_stats.profilers.clear();

	int num = pProfiler->GetProfilerCount();//min(20, pProfiler->GetProfilerCount());

	int need = 0;

	for (uint32 i = 0; i < num; ++i)
	{
		CFrameProfiler * pFrameInfo = pProfiler->GetProfiler(i);
		if (pFrameInfo->m_countHistory.GetAverage() > 0 && pFrameInfo->m_totalTimeHistory.GetAverage() > 0.0f)
			++need;
	}
	

	m_stats.profilers.resize(need);
	for (uint32 j = 0, i = 0; j < num; ++j)
	{
		CFrameProfiler * pFrameInfo = pProfiler->GetProfiler(j);	

		if (pFrameInfo->m_countHistory.GetAverage() > 0 && pFrameInfo->m_totalTimeHistory.GetAverage() > 0.0f)
		{

			m_stats.profilers[i].m_count =  pFrameInfo->m_countHistory.GetAverage();//pFrameInfo->m_count; 
			m_stats.profilers[i].m_displayedCurrentValue = pFrameInfo->m_displayedCurrentValue;
			m_stats.profilers[i].m_displayedValue = pFrameInfo->m_selfTimeHistory.GetAverage();
			m_stats.profilers[i].m_name = pFrameInfo->m_name;
			m_stats.profilers[i].m_module = ((CFrameProfileSystem*)pProfiler)->GetModuleName(pFrameInfo);
			m_stats.profilers[i].m_selfTime = pFrameInfo->m_selfTime;
			m_stats.profilers[i].m_sumSelfTime = pFrameInfo->m_sumSelfTime;
			m_stats.profilers[i].m_sumTotalTime = pFrameInfo->m_sumTotalTime;
			m_stats.profilers[i].m_totalTime = pFrameInfo->m_totalTimeHistory.GetAverage();
			m_stats.profilers[i].m_variance = pFrameInfo->m_variance;
			m_stats.profilers[i].m_min = (float)pFrameInfo->m_selfTimeHistory.GetMin();
			m_stats.profilers[i].m_max = (float)pFrameInfo->m_selfTimeHistory.GetMax();
			m_stats.profilers[i].m_mincount = pFrameInfo->m_countHistory.GetMin();
			m_stats.profilers[i].m_maxcount = pFrameInfo->m_countHistory.GetMax();
			i++;
		}
	}

	std::sort( m_stats.profilers.begin(),m_stats.profilers.end(),CompareFrameProfilersValue );


	// fill peaks
	num= pProfiler->GetPeaksCount();

	m_stats.peaks.resize(num);
	for (uint32 i = 0; i < num; ++i)
	{

		const SPeakRecord * pPeak = pProfiler->GetPeak(i);
		CFrameProfiler * pFrameInfo = pPeak->pProfiler;	

		m_stats.peaks[i].peakValue = pPeak->peakValue;
		m_stats.peaks[i].avarageValue = pPeak->avarageValue;
		m_stats.peaks[i].variance = pPeak->variance;
		m_stats.peaks[i].pageFaults = pPeak->pageFaults; // Number of page faults at this frame.
		m_stats.peaks[i].count = pPeak->count;  // Number of times called for peak.
		m_stats.peaks[i].when = pPeak->when; // when it added.

		m_stats.peaks[i].profiler.m_count =  pFrameInfo->m_countHistory.GetAverage();//pFrameInfo->m_count; 
		m_stats.peaks[i].profiler.m_displayedCurrentValue = pFrameInfo->m_displayedCurrentValue;
		m_stats.peaks[i].profiler.m_displayedValue = pFrameInfo->m_selfTimeHistory.GetAverage();
		m_stats.peaks[i].profiler.m_name = pFrameInfo->m_name;
		m_stats.peaks[i].profiler.m_module = ((CFrameProfileSystem*)pProfiler)->GetModuleName(pFrameInfo);
		m_stats.peaks[i].profiler.m_selfTime = pFrameInfo->m_selfTime;
		m_stats.peaks[i].profiler.m_sumSelfTime = pFrameInfo->m_sumSelfTime;
		m_stats.peaks[i].profiler.m_sumTotalTime = pFrameInfo->m_sumTotalTime;
		m_stats.peaks[i].profiler.m_totalTime = pFrameInfo->m_totalTimeHistory.GetAverage();
		m_stats.peaks[i].profiler.m_variance = pFrameInfo->m_variance;
		m_stats.peaks[i].profiler.m_min = (float)pFrameInfo->m_selfTimeHistory.GetMin();
		m_stats.peaks[i].profiler.m_max = (float)pFrameInfo->m_selfTimeHistory.GetMax();
		m_stats.peaks[i].profiler.m_mincount = pFrameInfo->m_countHistory.GetMin();
		m_stats.peaks[i].profiler.m_maxcount = pFrameInfo->m_countHistory.GetMax();
	}


}


//////////////////////////////////////////////////////////////////////////
void CEngineStats::CollectMemInfo()
{
	//////////////////////////////////////////////////////////////////////////
	m_stats.memInfo.totalStatic = 0;
	m_stats.memInfo.totalUsedInModules = 0;
	m_stats.memInfo.totalUsedInModulesStatic = 0;
	m_stats.memInfo.countedMemoryModules = 0;
	m_stats.memInfo.totalAllocatedInModules = 0;
	m_stats.memInfo.totalNumAllocsInModules = 0;

	static const char* szModules[] = {
		"Editor.exe",
		"CrySystem.dll",
		"CryScriptSystem.dll",
		"CryNetwork.dll",
		"CryPhysics.dll",
		"CryMovie.dll",
		"CryInput.dll",
		"CrySoundSystem.dll",
		"CryFont.dll",
		"CryAISystem.dll",
		"CryEntitySystem.dll",
		"Cry3DEngine.dll",
		"CryAnimation.dll",
		"CryGame.dll",
		"CryAction.dll",
		"CryRenderD3D9.dll",
		"CryRenderD3D10.dll",
		"CryRenderNULL.dll",
	};

	int nRows = 0;

	for (int i = 0; i < sizeof(szModules)/sizeof(szModules[0]); i++)
	{
		const char *szModule = szModules[i];
		HMODULE hModule = GetModuleHandle( szModule );
		if (!hModule)
			continue;

		//totalStatic += me.modBaseSize;
		typedef void (*PFN_MODULEMEMORY)( CryModuleMemoryInfo* );
		PFN_MODULEMEMORY fpCryModuleGetAllocatedMemory = (PFN_MODULEMEMORY)::GetProcAddress( hModule,"CryModuleGetMemoryInfo" );
		if (!fpCryModuleGetAllocatedMemory)
			continue;

		PEHeader_DLL pe_header;
		PEHeader_DLL *header = &pe_header;

		/*
		#ifdef XENON
		FILE *file = fopen( string("d:\\")+szModule,"rb" );
		if (file)
		{
		IMAGE_DOS_HEADER dos_hdr;
		fread( &dos_hdr,sizeof(dos_hdr),1,file );
		SwapEndian( dos_hdr.e_lfanew );
		fseek( file,dos_hdr.e_lfanew,SEEK_SET );
		fread( &pe_header,sizeof(pe_header),1,file );
		fclose(file);
		SwapEndian(header->opt_head.SizeOfInitializedData);
		SwapEndian(header->opt_head.SizeOfUninitializedData);
		SwapEndian(header->opt_head.SizeOfCode);
		SwapEndian(header->opt_head.SizeOfHeaders);
		}
		#elif defined(WIN32)
		*/
		const IMAGE_DOS_HEADER *dos_head = (IMAGE_DOS_HEADER*)hModule;
		if (dos_head->e_magic != IMAGE_DOS_SIGNATURE)
		{
			// Wrong pointer, not to PE header.
			continue;
		}
		header = (PEHeader_DLL*)(const void *)((char *)dos_head + dos_head->e_lfanew);
		//#endif

		SCryEngineStats::ModuleInfo moduleInfo;
		moduleInfo.name = szModule;
		moduleInfo.moduleStaticSize = header->opt_head.SizeOfInitializedData + header->opt_head.SizeOfUninitializedData + header->opt_head.SizeOfCode + header->opt_head.SizeOfHeaders;
		memset( &moduleInfo.memInfo,0,sizeof(moduleInfo.memInfo) );
		fpCryModuleGetAllocatedMemory( &moduleInfo.memInfo );

		moduleInfo.usedInModule = (int)(moduleInfo.memInfo.allocated - moduleInfo.memInfo.freed);

		moduleInfo.SizeOfCode = header->opt_head.SizeOfCode;
		moduleInfo.SizeOfInitializedData = header->opt_head.SizeOfInitializedData;
		moduleInfo.SizeOfUninitializedData = header->opt_head.SizeOfUninitializedData;
		
		m_stats.memInfo.totalNumAllocsInModules += moduleInfo.memInfo.num_allocations;
		m_stats.memInfo.totalAllocatedInModules += moduleInfo.memInfo.allocated;
		m_stats.memInfo.totalUsedInModules += moduleInfo.usedInModule;
		m_stats.memInfo.countedMemoryModules++;
		m_stats.memInfo.totalUsedInModulesStatic += moduleInfo.moduleStaticSize;

		m_stats.memInfo.modules.push_back(moduleInfo);
	}

	m_stats.memInfo.m_pSizer = new CrySizerImpl();
	((CSystem*)GetISystem())->CollectMemStats( m_stats.memInfo.m_pSizer,CSystem::nMSP_ForDump );
	m_stats.memInfo.m_pStats = new CrySizerStats(m_stats.memInfo.m_pSizer);

}

// Exports engine stats to the Excel.
class CStatsToExcelExporter
{
public:
	enum CellFlags
	{
		CELL_BOLD = 0x0001,
		CELL_CENTERED = 0x0002,
	};
	void ExportToFile( SCryEngineStats &stats,const char *filename );
	void ExportDependenciesToFile( CResourceCollector &stats,const char *filename );
	void Export( XmlNodeRef Workbook,SCryEngineStats &stats );

private:
	void ExportSummary( SCryEngineStats &stats );
	void ExportStatObjects( SCryEngineStats &stats );
	void ExportCharacters( SCryEngineStats &stats );
	void ExportRenderMeshes( SCryEngineStats &stats );
	void ExportTextures( SCryEngineStats &stats );
  void ExportMaterials( SCryEngineStats &stats );
	void ExportWavebanks( SCryEngineStats &stats );
	void ExportSoundInfos( SCryEngineStats &stats );
	void ExportMemStats(SCryEngineStats &stats );
	void ExportMemInfo( SCryEngineStats &stats );
	void ExportTimeDemoInfo(SCryEngineStats &stats);
	void ExportDependencies( CResourceCollector &stats );
	void ExportProfilerStatistics( SCryEngineStats &stats );
	void ExportAnimationStatistics( SCryEngineStats &stats );

	void InitExcelWorkbook( XmlNodeRef Workbook );

	XmlNodeRef NewWorksheet( const char *name );
	void AddCell( float number );
	void AddCell( int number );
	void AddCell( uint32 number );
	void AddCell( uint64 number ) { AddCell( (uint32)number ); };
	void AddCell( int64 number ) { AddCell( (int)number ); };
	void AddCell( const char *str,int flags=0 );
	void AddCellAtIndex( int nIndex,const char *str,int flags=0 );
	void SetCellFlags( XmlNodeRef cell,int flags );
	void AddRow();
	void AddCell_SumOfRows( int nRows );
	string GetXmlHeader();

private:
	XmlNodeRef m_Workbook;
	XmlNodeRef m_CurrTable;
	XmlNodeRef m_CurrWorksheet;
	XmlNodeRef m_CurrRow;
	XmlNodeRef m_CurrCell;
};

//////////////////////////////////////////////////////////////////////////
string CStatsToExcelExporter::GetXmlHeader()
{
	return "<?xml version=\"1.0\"?>\n<?mso-application progid=\"Excel.Sheet\"?>\n";
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::InitExcelWorkbook( XmlNodeRef Workbook )
{
	m_Workbook = Workbook;
	m_Workbook->setTag( "Workbook" );
	m_Workbook->setAttr( "xmlns","urn:schemas-microsoft-com:office:spreadsheet" );
	XmlNodeRef ExcelWorkbook = Workbook->newChild( "ExcelWorkbook" );
	ExcelWorkbook->setAttr( "xmlns","urn:schemas-microsoft-com:office:excel" );

	XmlNodeRef Styles = m_Workbook->newChild("Styles");
	{
		// Style s25
		// Bold header, With Background Color.
		XmlNodeRef Style = Styles->newChild("Style");
		Style->setAttr("ss:ID","s25");
		XmlNodeRef StyleFont = Style->newChild("Font");
		StyleFont->setAttr("x:CharSet","204");
		StyleFont->setAttr("x:Family","Swiss");
		StyleFont->setAttr("ss:Bold","1");
		XmlNodeRef StyleInterior = Style->newChild("Interior");
		StyleInterior->setAttr("ss:Color","#00FF00");
		StyleInterior->setAttr("ss:Pattern","Solid");
		XmlNodeRef NumberFormat = Style->newChild( "NumberFormat" );
		NumberFormat->setAttr( "ss:Format","#,##0" );
	}
	{
		// Style s26
		// Bold/Centered header.
		XmlNodeRef Style = Styles->newChild("Style");
		Style->setAttr("ss:ID","s26");
		XmlNodeRef StyleFont = Style->newChild("Font");
		StyleFont->setAttr("x:CharSet","204");
		StyleFont->setAttr("x:Family","Swiss");
		StyleFont->setAttr("ss:Bold","1");
		XmlNodeRef StyleInterior = Style->newChild("Interior");
		StyleInterior->setAttr("ss:Color","#FFFF99");
		StyleInterior->setAttr("ss:Pattern","Solid");
		XmlNodeRef Alignment = Style->newChild( "Alignment" );
		Alignment->setAttr( "ss:Horizontal","Center" );
		Alignment->setAttr( "ss:Vertical","Bottom" );
	}
	{
		// Style s20
		// Centered
		XmlNodeRef Style = Styles->newChild("Style");
		Style->setAttr("ss:ID","s20");
		XmlNodeRef Alignment = Style->newChild( "Alignment" );
		Alignment->setAttr( "ss:Horizontal","Center" );
		Alignment->setAttr( "ss:Vertical","Bottom" );
	}
	{
		// Style s21
		// Bold
		XmlNodeRef Style = Styles->newChild("Style");
		Style->setAttr("ss:ID","s21");
		XmlNodeRef StyleFont = Style->newChild("Font");
		StyleFont->setAttr("x:CharSet","204");
		StyleFont->setAttr("x:Family","Swiss");
		StyleFont->setAttr("ss:Bold","1");
	}
	{
		// Style s22
		// Centered, Integer Number format
		XmlNodeRef Style = Styles->newChild("Style");
		Style->setAttr("ss:ID","s22");
		XmlNodeRef Alignment = Style->newChild( "Alignment" );
		Alignment->setAttr( "ss:Horizontal","Center" );
		Alignment->setAttr( "ss:Vertical","Bottom" );
		XmlNodeRef NumberFormat = Style->newChild( "NumberFormat" );
		NumberFormat->setAttr( "ss:Format","#,##0" );
	}
	{
		// Style s23
		// Centered, Float Number format
		XmlNodeRef Style = Styles->newChild("Style");
		Style->setAttr("ss:ID","s23");
		XmlNodeRef Alignment = Style->newChild( "Alignment" );
		Alignment->setAttr( "ss:Horizontal","Center" );
		Alignment->setAttr( "ss:Vertical","Bottom" );
		//XmlNodeRef NumberFormat = Style->newChild( "NumberFormat" );
		//NumberFormat->setAttr( "ss:Format","#,##0" );
	}

/*
		<Style ss:ID="s25">
		<Font x:CharSet="204" x:Family="Swiss" ss:Bold="1"/>
		<Interior ss:Color="#FFFF99" ss:Pattern="Solid"/>
		</Style>
		*/
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CStatsToExcelExporter::NewWorksheet( const char *name )
{
	m_CurrWorksheet = m_Workbook->newChild( "Worksheet" );
	m_CurrWorksheet->setAttr( "ss:Name",name );
	m_CurrTable = m_CurrWorksheet->newChild( "Table" );
	return m_CurrWorksheet;
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::AddRow()
{
	m_CurrRow = m_CurrTable->newChild( "Row" );
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::AddCell_SumOfRows( int nRows )
{
	XmlNodeRef cell = m_CurrRow->newChild("Cell");
	XmlNodeRef data = cell->newChild( "Data" );
	data->setAttr( "ss:Type","Number" );
	data->setContent( "0" );
	m_CurrCell = cell;

	if (nRows > 0)
	{
		char buf[32];
		sprintf( buf,"=SUM(R[-%d]C:R[-1]C)",nRows );
		m_CurrCell->setAttr( "ss:Formula",buf );
	}
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::AddCell( float number )
{
	XmlNodeRef cell = m_CurrRow->newChild("Cell");
	cell->setAttr("ss:StyleID","s23"); // Centered
	XmlNodeRef data = cell->newChild( "Data" );
	data->setAttr( "ss:Type","Number" );
	char str[32];
	sprintf( str,"%.3f",number );
	data->setContent( str );
	m_CurrCell = cell;
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::AddCell( int number )
{
	XmlNodeRef cell = m_CurrRow->newChild("Cell");
	cell->setAttr("ss:StyleID","s22"); // Centered
	XmlNodeRef data = cell->newChild( "Data" );
	data->setAttr( "ss:Type","Number" );
	char str[32];
	sprintf( str,"%d",number );
	data->setContent( str );
	m_CurrCell = cell;
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::AddCell( uint32 number )
{
	XmlNodeRef cell = m_CurrRow->newChild("Cell");
	cell->setAttr("ss:StyleID","s22"); // Centered
	XmlNodeRef data = cell->newChild( "Data" );
	data->setAttr( "ss:Type","Number" );
	char str[32];
	sprintf( str,"%u",number );
	data->setContent( str );
	m_CurrCell = cell;
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::AddCell( const char *str,int nFlags )
{
	XmlNodeRef cell = m_CurrRow->newChild("Cell");
	XmlNodeRef data = cell->newChild( "Data" );
	data->setAttr( "ss:Type","String" );
	data->setContent( str );
	SetCellFlags( cell,nFlags );
	m_CurrCell = cell;
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::AddCellAtIndex( int nIndex,const char *str,int nFlags )
{
	XmlNodeRef cell = m_CurrRow->newChild("Cell");
	cell->setAttr( "ss:Index",nIndex );
	XmlNodeRef data = cell->newChild( "Data" );
	data->setAttr( "ss:Type","String" );
	data->setContent( str );
	SetCellFlags( cell,nFlags );
	m_CurrCell = cell;
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::SetCellFlags( XmlNodeRef cell,int flags )
{
	if (flags & CELL_BOLD)
	{
		if (flags & CELL_CENTERED)
			cell->setAttr("ss:StyleID","s26");
		else
			cell->setAttr("ss:StyleID","s21");
	}
	else
	{
		if (flags & CELL_CENTERED)
			cell->setAttr("ss:StyleID","s20");
	}
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::ExportToFile( SCryEngineStats &stats,const char *filename )
{
	XmlNodeRef Workbook = GetISystem()->CreateXmlNode("Workbook");
	Export( Workbook,stats );
	string xml = GetXmlHeader();
	xml = xml + Workbook->getXML();

	CryCreateDirectory( g_szTestResults,NULL );
	FILE *file = fopen( PathUtil::Make(g_szTestResults,filename),"wt" );
	if (file)
	{
		fprintf( file,"%s",xml.c_str() );
		fclose(file);
	}
}


//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::ExportDependenciesToFile( CResourceCollector &stats,const char *filename )
{
	XmlNodeRef Workbook = GetISystem()->CreateXmlNode("Workbook");

	{
		InitExcelWorkbook(Workbook);
		ExportDependencies(stats);
	}

	string xml = GetXmlHeader();
	xml = xml + Workbook->getXML();

	CryCreateDirectory( g_szTestResults,NULL );
	FILE *file = fopen( PathUtil::Make(g_szTestResults,filename),"wt" );
	if (file)
	{
		fprintf( file,"%s",xml.c_str() );
		fclose(file);
	}
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::Export( XmlNodeRef Workbook,SCryEngineStats &stats )
{
	InitExcelWorkbook(Workbook);
	ExportSummary(stats);
	ExportStatObjects(stats);
	ExportCharacters(stats);
	ExportRenderMeshes(stats);
	ExportTextures(stats);
  ExportMaterials( stats );
	ExportWavebanks(stats);
	ExportSoundInfos(stats);
	ExportMemInfo(stats);
	ExportMemStats(stats);
	ExportTimeDemoInfo(stats);
	ExportProfilerStatistics(stats);
	ExportAnimationStatistics(stats);
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::ExportSummary( SCryEngineStats &stats )
{
	// Make summary sheet.en
	NewWorksheet( "Summary" );

	XmlNodeRef Column;
	Column = m_CurrTable->newChild("Column"); Column->setAttr( "ss:Width",200 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",100 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",100 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",100 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",100 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",100 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",100 );

	// get version
	const SFileVersion & ver = GetISystem()->GetFileVersion();
	char sVersion[128];
	ver.ToString(sVersion);
	string levelName = "no_level";
	ICVar *sv_map = gEnv->pConsole->GetCVar("sv_map");
	if (sv_map)
		levelName = sv_map->GetString();

	AddRow();
	AddCell( string("CryEngine2 Version ")+sVersion );
	AddRow();
	AddCell( string("Level ")+levelName );
	AddRow();
#ifdef WIN64
	AddCell( "Running in 64bit version" );
#else
	AddCell( "Running in 32bit version" );
#endif
	AddRow();
	AddCell( "Level Load Time (sec):" );
	AddCell( (int)stats.fLevelLoadTime );
	AddRow();
	AddRow();
	AddCell( "Average\\Min\\Max (fps):" );
	AddCell( (int)stats.infoFPS.fAverageFPS );
	AddCell( (int)stats.infoFPS.fMinFPS );
	AddCell( (int)stats.infoFPS.fMaxFPS );
	AddRow();

	AddRow();
	m_CurrRow->setAttr( "ss:StyleID","s25" );
	AddCell( "Resource Type (MB)" );
	AddCell( "Count" );
	AddCell( "Memory Size" );
	AddCell( "Only Mesh Size" );
	AddCell( "Only Texture Size" );

	AddRow();
	AddCell( "CGF Objects",CELL_BOLD );
	AddCell( stats.objects.size() );
	AddCell( (stats.nStatObj_SummaryTextureSize+stats.nStatObj_SummaryMeshSize)/(1024*1024) );
	AddCell( (stats.nStatObj_SummaryMeshSize)/(1024*1024) );
	AddCell( (stats.nStatObj_SummaryTextureSize)/(1024*1024) );

	AddRow();
	AddCell( "Character Models",CELL_BOLD );
	AddCell( stats.characters.size() );
	AddCell( (stats.nChar_SummaryTextureSize+stats.nChar_SummaryMeshSize)/(1024*1024) );
	AddCell( (stats.nChar_SummaryMeshSize)/(1024*1024) );
	AddCell( (stats.nChar_SummaryTextureSize)/(1024*1024) );


	AddRow();
	AddCell( "Character Instances",CELL_BOLD );
	AddCell( stats.nChar_NumInstances );

	//AddCell( stats.nMemCharactersTotalSize );
	AddRow();
	
	AddRow();
	m_CurrRow->setAttr( "ss:StyleID","s25" );
	AddCell( "Resource Type (MB)" );
	AddCell( "Count" );
	AddCell( "Total Size" );
	AddCell( "System Memory" );
	AddCell( "Video Memory" );
	AddCell( "Engine Textures" );
	AddCell( "User Textures" );

	AddRow();
	AddCell( "Textures",CELL_BOLD );
	AddCell( stats.textues.size() );
	AddCell( (stats.nSummary_TextureSize)/(1024*1024) );
	AddCell( (stats.nSummary_TextureSize)/(1024*1024) );
	AddCell( (stats.nSummary_TextureSize)/(1024*1024) );
	AddCell( (stats.nSummary_EngineTextureSize)/(1024*1024) );
	AddCell( (stats.nSummary_UserTextureSize)/(1024*1024) );
	
	AddRow();
	AddCell( "Meshes",CELL_BOLD );
	AddCell( stats.nSummaryMeshCount );
	AddCell( (stats.nAPI_MeshSize+stats.nSummaryMeshSize)/(1024*1024) );
	AddCell( (stats.nSummaryMeshSize)/(1024*1024) );
	AddCell( (stats.nAPI_MeshSize)/(1024*1024) );

	AddRow();
	AddRow();
	m_CurrRow->setAttr( "ss:StyleID","s25" );
	AddRow();

	AddRow();
	AddCell( "Lua Memory Usage (MB)" );
	AddCell( stats.nSummaryScriptSize/(1024*1024) );
	AddRow();
	AddCell( "Game Memory Usage (MB)" );
	AddCell( stats.nTotalAllocatedMemory/(1024*1024) );

	uint64 totalAll = stats.nTotalAllocatedMemory;
	totalAll += stats.nAPI_MeshSize;
	totalAll += stats.nSummary_TextureSize;
	AddRow();
	AddCell( "Total Allocated (With API Textures/Mesh) (MB)" );
	AddCell( totalAll/(1024*1024) );
	AddRow();
	AddRow();
	AddCell( "Virtual Memory Usage (MB)" );
	AddCell( stats.nWin32_PagefileUsage/(1024*1024) );
	AddRow();
	AddCell( "Peak Virtual Memory Usage (MB)" );
	AddCell( stats.nWin32_PeakPagefileUsage/(1024*1024) );
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::ExportStatObjects( SCryEngineStats &stats )
{
	NewWorksheet( "Static Geometry" );

	XmlNodeRef Column;
	Column = m_CurrTable->newChild("Column"); Column->setAttr( "ss:Width",300 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",90 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",90 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",40 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",70 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",60 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",60 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",60 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",80 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",120 );

	AddRow();
	m_CurrRow->setAttr( "ss:StyleID","s25" );
	AddCell( "Filename" );
	AddCell( "Mesh Size (KB)" );
	AddCell( "Texture Size (KB)" );
	AddCell( "LODs" );
	AddCell( "Sub Meshes" );
	AddCell( "Vertices" );
	AddCell( "Tris" );
	AddCell( "Physics Tris" );
	AddCell( "Physics Size (KB)" );
	AddCell( "LODs Tris" );

	int nRows = (int)stats.objects.size();
	for (int i = 0; i < nRows; i++)
	{
		SCryEngineStats::StatObjInfo &si = stats.objects[i];
		AddRow();
		AddCell( si.pStatObj->GetFilePath() );
		AddCell( si.nMeshSize/1024 );
		AddCell( si.nTextureSize/1024 );
		AddCell( si.nLods );
		AddCell( si.nSubMeshCount );
		AddCell( si.nVertices );
		AddCell( si.nIndices/3 );
		AddCell( si.nPhysPrimitives );
		AddCell( si.nPhysProxySize/1024 );

		if (si.nLods > 1)
		{
			// Print lod1/lod2/lod3 ...
			char tempstr[256];
			char numstr[32];
			tempstr[0] = 0;
			int numlods = 0;
			for (int lod = 0; lod < MAX_LODS; lod++)
			{
				if (si.nIndicesPerLod[lod] != 0)
				{
					sprintf_s(numstr,sizeof(numstr),"%d",(si.nIndicesPerLod[lod]/3) );
					if (numlods > 0)
						strcat_s(tempstr,sizeof(tempstr)," / ");
					strcat_s(tempstr,sizeof(tempstr),numstr);
					numlods++;
				}
			}
			if (numlods > 1)
				AddCell(tempstr);
		}
		
	}
	AddRow(); m_CurrRow->setAttr( "ss:StyleID","s25" );
	AddCell( "" );
	AddCell_SumOfRows( nRows );
	AddCell_SumOfRows( nRows );
	AddCell( "" );
	AddCell( "" );
	AddCell_SumOfRows( nRows );
	AddCell_SumOfRows( nRows );
	AddCell_SumOfRows( nRows );
}
//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::ExportAnimationStatistics( SCryEngineStats &stats )
{
	NewWorksheet( "Animations" );

	XmlNodeRef Column;
	Column = m_CurrTable->newChild("Column"); Column->setAttr( "ss:Width",300 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",50 );

	AddRow();
	m_CurrRow->setAttr( "ss:StyleID","s25" );
	AddCell( "Name" );
	AddCell( "Count" );

	int nRows = (int)stats.animations.size();
	for (int i = 0; i < nRows; i++)
	{
		SAnimationStatistics &an = stats.animations[i];
		AddRow();
		AddCell( an.name );
		AddCell( an.count );
	}
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::ExportProfilerStatistics( SCryEngineStats &stats )
{
	{

	
	NewWorksheet( "Profiler" );

	XmlNodeRef Column;
	Column = m_CurrTable->newChild("Column"); Column->setAttr( "ss:Width",100 );
	Column = m_CurrTable->newChild("Column"); Column->setAttr( "ss:Width",300 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",50 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",50 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",50 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",50 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",50 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",50 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",50 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",50 );

//	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",40 );

	AddRow();
	m_CurrRow->setAttr( "ss:StyleID","s25" );
	AddCell( "Module" );
	AddCell( "Name" );
	AddCell( "Self time, ms" );
	AddCell( "Total time, ms" );
	AddCell( "Count" );
	AddCell( "" );
	AddCell( "Min time, ms" );
	AddCell( "Max time, ms" );
	AddCell( "Min count" );
	AddCell( "Max count" );

	int nRows = (int)stats.profilers.size();
	for (int i = 0; i < nRows; i++)
	{
		SCryEngineStats::ProfilerInfo &pi = stats.profilers[i];
		AddRow();
		AddCell( pi.m_module );
		AddCell( pi.m_name );
		AddCell( pi.m_displayedValue );
		AddCell( pi.m_totalTime );
		AddCell( pi.m_count );
		AddCell( "");
		AddCell( pi.m_min );
		AddCell( pi.m_max );
		AddCell( pi.m_mincount );
		AddCell( pi.m_maxcount );

	}

	AddRow(); m_CurrRow->setAttr( "ss:StyleID","s25" );
	AddCell("");
	AddCell("");
	AddCell_SumOfRows( nRows );
	AddCell_SumOfRows( nRows );
	AddCell_SumOfRows( nRows );
//	AddCell("");
	AddCell("");
	AddCell("");
	}

	// Peaks

	NewWorksheet( "Peaks" );

	XmlNodeRef Column;
	Column = m_CurrTable->newChild("Column"); Column->setAttr( "ss:Width",100 );
	Column = m_CurrTable->newChild("Column"); Column->setAttr( "ss:Width",300 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",50 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",50 );
	//Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",50 );
	//Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",50 );
	//Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",50 );
	//Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",50 );
	//Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",50 );
	//Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",50 );
	//Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",50 );

	//	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",40 );

	AddRow();
	m_CurrRow->setAttr( "ss:StyleID","s25" );
	AddCell( "Module" );
	AddCell( "Name" );
	AddCell( "Peak, ms" );
	//AddCell( "Self time, ms" );
	//AddCell( "Total time, ms" );
	AddCell( "Count" );
	//AddCell( "" );
	//AddCell( "Min time, ms" );
	//AddCell( "Max time, ms" );
	//AddCell( "Min count" );
	//AddCell( "Max count" );

	int nRows = (int)stats.peaks.size();
	for (int i = 0; i < nRows; i++)
	{
		SCryEngineStats::SPeakProfilerInfo &peak = stats.peaks[i];
		SCryEngineStats::ProfilerInfo &pi = stats.peaks[i].profiler;
		AddRow();
		AddCell( pi.m_module );
		AddCell( pi.m_name );
		AddCell( peak.peakValue );
		//AddCell( pi.m_displayedValue );
		//AddCell( pi.m_totalTime );
		AddCell( peak.count );
		//AddCell( "");
		//AddCell( pi.m_min );
		//AddCell( pi.m_max );
		//AddCell( pi.m_mincount );
		//AddCell( pi.m_maxcount );
	}

	//AddRow(); m_CurrRow->setAttr( "ss:StyleID","s25" );
	//AddCell("");
	//AddCell("");
	//AddCell("");
	//AddCell_SumOfRows( nRows );
	//AddCell_SumOfRows( nRows );
	//AddCell_SumOfRows( nRows );
	////	AddCell("");
	//AddCell("");
	//AddCell("");

}


//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::ExportCharacters( SCryEngineStats &stats )
{
	NewWorksheet( "Characters" );

	XmlNodeRef Column;
	Column = m_CurrTable->newChild("Column"); Column->setAttr( "ss:Width",300 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",40 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",90 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",90 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",40 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",90 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",90 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",150 );

	AddRow();
	m_CurrRow->setAttr( "ss:StyleID","s25" );
	AddCell( "Filename" );
	AddCell( "Num Instances" );
	AddCell( "Mesh Size (KB)" );
	AddCell( "Texture Size (KB)" );
	AddCell( "LODs" );
	AddCell( "Vertices" );
	AddCell( "Tris" );
	AddCell( "LOD Tris" );
	int nRows = (int)stats.characters.size();
	for (int i = 0; i < nRows; i++)
	{
		SCryEngineStats::CharacterInfo &si = stats.characters[i];
		AddRow();
		AddCell( si.pModel->GetModelFilePath() );
		AddCell( si.nInstances );
		AddCell( si.nMeshSize/1024 );
		AddCell( si.nTextureSize/1024 );
		AddCell( si.nLods );
		AddCell( si.nVertices );
		AddCell( si.nIndices/3 );

		if (si.nLods > 1)
		{
			// Print lod1/lod2/lod3 ...
			char tempstr[256];
			char numstr[32];
			tempstr[0] = 0;
			int numlods = 0;
			for (int lod = 0; lod < MAX_LODS; lod++)
			{
				if (si.nIndicesPerLod[lod] != 0)
				{
					sprintf_s(numstr,sizeof(numstr),"%d",(si.nIndicesPerLod[lod]/3) );
					if (numlods > 0)
						strcat_s(tempstr,sizeof(tempstr)," / ");
					strcat_s(tempstr,sizeof(tempstr),numstr);
					numlods++;
				}
			}
			if (numlods > 1)
				AddCell(tempstr);
		}
	}
	AddRow(); m_CurrRow->setAttr( "ss:StyleID","s25" );
	AddCell("");
	AddCell_SumOfRows( nRows );
	AddCell_SumOfRows( nRows );
	AddCell_SumOfRows( nRows );
	AddCell("");
	AddCell_SumOfRows( nRows );
	AddCell_SumOfRows( nRows );
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::ExportDependencies( CResourceCollector &stats )
{
	{
		NewWorksheet( "Assets" );

		XmlNodeRef Column;
		Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",72 );
		Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",108 );
		Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",73 );
		Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",60 );
		Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",1000 );

		AddRow();
		m_CurrRow->setAttr( "ss:StyleID","s25" );
		AddCell( "Instances" );
		AddCell( "Dependencies" );
		AddCell( "FileMem (KB)" );
		AddCell( "Extension" );
		AddCell( "FileName" );

		std::vector<CResourceCollector::SAssetEntry>::const_iterator it, end=stats.m_Assets.end();
		uint32 dwAssetID=0;

		for(it=stats.m_Assets.begin();it!=end;++it,++dwAssetID)
		{
			const CResourceCollector::SAssetEntry &rRef = *it;

			AddRow();

			char szAName1[1024];		sprintf(szAName1,"A%d %s",dwAssetID,rRef.m_sFileName);

			AddCell( rRef.m_dwInstanceCnt );
			AddCell( rRef.m_dwDependencyCnt );
			AddCell( rRef.m_dwFileSize!=0xffffffff ? rRef.m_dwFileSize/1024 : 0 );
			AddCell( PathUtil::GetExt(rRef.m_sFileName) );
			AddCell( szAName1 );
		}

		AddRow(); m_CurrRow->setAttr( "ss:StyleID","s25" );
		AddCell_SumOfRows( dwAssetID );
		AddCell("");
		AddCell_SumOfRows( dwAssetID );
	}

	{
		NewWorksheet( "Dependencies" );
		
		XmlNodeRef Column;
		Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",600 );
		Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",90 );
		Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",60 );

		AddRow();
		m_CurrRow->setAttr( "ss:StyleID","s25" );
		AddCell( "Asset Filename" );
		AddCell( "Requires Sum (KB)" );
		AddCell( "Requires Count" );

		std::set<CResourceCollector::SDependencyPair>::const_iterator it, end=stats.m_Dependencies.end();

		uint32 dwCurrentAssetID=0xffffffff;
		uint32 dwSumFile=0,dwSum=0;

		for(it=stats.m_Dependencies.begin();;++it)
		{
			uint32 dwAssetsSize = stats.m_Assets.size();

			if(it==end || (*it).m_idAsset!=dwCurrentAssetID)
			{
				if(dwSum!=0 && dwSumFile!=0xffffffff)
				{
					assert(dwCurrentAssetID<dwAssetsSize);

					char szAName0[1024];		sprintf(szAName0,"A%d %s",dwCurrentAssetID,stats.m_Assets[dwCurrentAssetID].m_sFileName.c_str());

					AddRow();
					AddCell( szAName0 );
					AddCell( dwSumFile/1024 );
					AddCell( dwSum );
				}

				dwSumFile=0;dwSum=0;

				if(it==end)
					break;
			}

			const CResourceCollector::SDependencyPair &rRef = *it;		

			assert(rRef.m_idDependsOnAsset<dwAssetsSize);

			CResourceCollector::SAssetEntry &rDepAsset = stats.m_Assets[rRef.m_idDependsOnAsset];

			if(rDepAsset.m_dwFileSize!=0xffffffff)
				dwSumFile += rDepAsset.m_dwFileSize;

			++dwSum;

			dwCurrentAssetID=rRef.m_idAsset;
		}
	}


	{
		NewWorksheet( "Detailed Dependencies" );

		XmlNodeRef Column;
		Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",600 );
		Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",1000 );
		Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",70 );

		AddRow();
		m_CurrRow->setAttr( "ss:StyleID","s25" );
		AddCell( "Asset Filename" );
		AddCell( "Requires Filename" );
		AddCell( "Requires (KB)" );

		std::set<CResourceCollector::SDependencyPair>::const_iterator it, end=stats.m_Dependencies.end();

		uint32 dwCurrentAssetID=0xffffffff;
		uint32 dwSumFile=0,dwSum=0;

		for(it=stats.m_Dependencies.begin();;++it)
		{
			if(it==end || (*it).m_idAsset!=dwCurrentAssetID)
			{
				if(dwSum!=0 && dwSumFile!=0xffffffff)
				{
					//					AddRow(); m_CurrRow->setAttr( "ss:StyleID","s21" );
					//					AddCell("");
					//					AddCell_SumOfRows( dwSum );
					AddRow();
					AddCell( "" );
				}

				dwSumFile=0;dwSum=0;

				if(it==end)
					break;
				/*
				const CResourceCollector::SDependencyPair &rRef = *it;		

				char szAName0[20];		sprintf(szAName0,"A%d",rRef.m_idAsset);

				AddRow(); m_CurrRow->setAttr( "ss:StyleID","s21" );
				AddCell( szAName0 );
				AddCell( stats.m_Assets[rRef.m_idAsset].m_sFileName.c_str() );
				*/
			}

			const CResourceCollector::SDependencyPair &rRef = *it;		

			CResourceCollector::SAssetEntry &rDepAsset = stats.m_Assets[rRef.m_idDependsOnAsset];

			AddRow();

			char szAName0[1024];		sprintf(szAName0,"A%d %s",rRef.m_idAsset,stats.m_Assets[rRef.m_idAsset].m_sFileName.c_str());
			char szAName1[1024];		sprintf(szAName1,"A%d %s",rRef.m_idDependsOnAsset,rDepAsset.m_sFileName.c_str());

			AddCell( szAName0 );
			AddCell( szAName1 );
			AddCell( rDepAsset.m_dwFileSize!=0xffffffff ? rDepAsset.m_dwFileSize/1024 : 0 );

			if(rDepAsset.m_dwFileSize!=0xffffffff)
				dwSumFile += rDepAsset.m_dwFileSize;

			++dwSum;

			dwCurrentAssetID=rRef.m_idAsset;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::ExportRenderMeshes( SCryEngineStats &stats )
{
	NewWorksheet( "Meshes" );

	XmlNodeRef Column;
	Column = m_CurrTable->newChild("Column"); Column->setAttr( "ss:Width",300 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",40 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",90 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",90 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",90 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",90 );

	AddRow();
	m_CurrRow->setAttr( "ss:StyleID","s25" );
	AddCell( "Mesh Type" );
	AddCell( "Num Instances" );
	AddCell( "Mesh Size (KB)" );
	AddCell( "Texture Size (KB)" );
	AddCell( "Total Vertices" );
	AddCell( "Total Tris" );
	int nRows = (int)stats.meshes.size();
	for (int i = 0; i < nRows; i++)
	{
		SCryEngineStats::MeshInfo &mi = stats.meshes[i];
		AddRow();
		AddCell( mi.name );
		AddCell( mi.nCount );
		AddCell( mi.nMeshSize/1024 );
		AddCell( mi.nTextureSize/1024 );
		AddCell( mi.nVerticesSum );
		AddCell( mi.nIndicesSum/3 );
	}
	AddRow(); m_CurrRow->setAttr( "ss:StyleID","s25" );
	AddCell("");
	AddCell_SumOfRows( nRows );
	AddCell_SumOfRows( nRows );
	AddCell_SumOfRows( nRows );
	AddCell_SumOfRows( nRows );
	AddCell_SumOfRows( nRows );
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::ExportMaterials( SCryEngineStats &stats )
{
  NewWorksheet( "Materials" );

  XmlNodeRef Column;
  Column = m_CurrTable->newChild("Column"); Column->setAttr( "ss:Width",400 );

  AddRow();
  m_CurrRow->setAttr( "ss:StyleID","s25" );

  AddCell( "Name" );
  AddCell( "System?" );
  AddCell( "Ref counter" );
  AddCell( "Childs num" );

  int nRows = (int)stats.materials.size();
  //char texres[32];
  for (int i = 0; i < nRows; i++)
  {
    IMaterial *pMat = stats.materials[i];
    AddRow();
    AddCell( pMat->GetName() );

    if(pMat->GetShaderItem().m_pShader && pMat->GetShaderItem().m_pShader->GetFlags()&EF_SYSTEM)
      AddCell( "YES" );
    else
      AddCell( "NO" );

    AddCell( pMat->GetNumRefs() );

    AddCell( pMat->GetSubMtlCount() );
  }

//  AddRow(); m_CurrRow->setAttr( "ss:StyleID","s25" );
  //AddCell("");
  //AddCell_SumOfRows( nRows );
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::ExportTextures( SCryEngineStats &stats )
{
	NewWorksheet( "Textures" );

	XmlNodeRef Column;
	Column = m_CurrTable->newChild("Column"); Column->setAttr( "ss:Width",400 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",80 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",80 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",40 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",40 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",80 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",80 );

	AddRow();
	m_CurrRow->setAttr( "ss:StyleID","s25" );
	
	AddCell( "Filename" );
	AddCell( "Texture Size (KB)" );
	AddCell( "Resolution" );
	AddCell( "Mip Levels" );
	AddCell( "Type" );
	AddCell( "Format" );

	int nRows = (int)stats.textues.size();
	char texres[32];
	for (int i = 0; i < nRows; i++)
	{
		ITexture *pTexture = stats.textues[i];
		AddRow();
		AddCell( pTexture->GetName() );
		AddCell( pTexture->GetDeviceDataSize()/1024 );

		sprintf( texres,"%d x %d",pTexture->GetWidth(),pTexture->GetHeight() );
		AddCell( texres,CELL_CENTERED );

		AddCell( pTexture->GetNumMips() );
		AddCell( pTexture->GetTypeName(),CELL_CENTERED );
		AddCell( pTexture->GetFormatName(),CELL_CENTERED );
	}

	AddRow(); m_CurrRow->setAttr( "ss:StyleID","s25" );
	AddCell("");
	AddCell_SumOfRows( nRows );
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::ExportWavebanks( SCryEngineStats &stats )
{
	NewWorksheet( "Wavebanks" );

	XmlNodeRef Column;
	Column = m_CurrTable->newChild("Column"); Column->setAttr( "ss:Width",400 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",90 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",40 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",90 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",80 );

	AddRow();
	m_CurrRow->setAttr( "ss:StyleID","s25" );

	AddCell( "Filename" );
	AddCell( "Wavebank Size (KB)" );
	AddCell( "Times Used" );
	AddCell( "Current Memory (KB)" );
	AddCell( "Peak Memory (KB)" );

	int nRows = (int)stats.wavebanks.size();
	for (int i = 0; i < nRows; i++)
	{
		IWavebank *pWavebank = stats.wavebanks[i];
		AddRow();
		
		string sFullName = pWavebank->GetPath();
		sFullName += pWavebank->GetName();
		sFullName += ".fsb";

		AddCell( sFullName );
		AddCell( pWavebank->GetInfo()->nFileSize/1024 );
		AddCell (pWavebank->GetInfo()->nTimesAccessed );
		AddCell (pWavebank->GetInfo()->nMemCurrentlyInByte/1024 );
		AddCell (pWavebank->GetInfo()->nMemPeakInByte/1024 );
	}

	AddRow(); m_CurrRow->setAttr( "ss:StyleID","s25" );
	AddCell("");
	AddCell_SumOfRows( nRows );
	AddCell("");
	AddCell_SumOfRows( nRows );
	AddCell_SumOfRows( nRows );
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::ExportSoundInfos( SCryEngineStats &stats )
{
	NewWorksheet( "SoundInfos" );

	XmlNodeRef Column;
	Column = m_CurrTable->newChild("Column"); Column->setAttr( "ss:Width",400 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",70 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",120 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",90 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",80 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",80 );

	AddRow();
	m_CurrRow->setAttr( "ss:StyleID","s25" );

	AddCell( "Soundname" );
	AddCell( "Times Played" );
	AddCell( "Times Played on a Channel" );
	AddCell( "Memory Size (KB)" );
	AddCell( "Peak Spawns" );
	AddCell( "Channels Used" );

	int nRows = (int)stats.soundinfos.size();
	for (int i = 0; i < nRows; i++)
	{
		ISoundProfileInfo *pSoundInfo = stats.soundinfos[i];
		AddRow();
		AddCell( pSoundInfo->GetName() );
		AddCell (pSoundInfo->GetInfo()->nTimesPlayed );
		AddCell (pSoundInfo->GetInfo()->nTimesPlayedOnChannel );
		AddCell( pSoundInfo->GetInfo()->nMemorySize/1024 );
		AddCell (pSoundInfo->GetInfo()->nPeakSpawn );
		AddCell (pSoundInfo->GetInfo()->nChannelsUsed );
	}

	AddRow(); m_CurrRow->setAttr( "ss:StyleID","s25" );
	AddCell("");
	AddCell_SumOfRows( nRows );
	AddCell("");
	AddCell_SumOfRows( nRows );
	AddCell_SumOfRows( nRows );
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::ExportMemStats(SCryEngineStats &stats )
{
	NewWorksheet( "Memory Stats" );
	XmlNodeRef Column;
	Column = m_CurrTable->newChild("Column"); Column->setAttr( "ss:Width",300 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",90 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",90 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",90 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",90 );

	AddRow();
	m_CurrRow->setAttr( "ss:StyleID","s25" );
	
	AddCell( "Section" );
	AddCell( "Size (KB)" );
	AddCell( "Total Size (KB)" );
	AddCell( "Object Count" );

	AddRow();


	for (unsigned i = 0; i < stats.memInfo.m_pStats->size(); ++i)
	{
		AddRow();
		const CrySizerStats::Component& rComp = (*stats.memInfo.m_pStats)[i];

		if (rComp.nDepth < 2)
		{
			AddRow(); // Skip one row if primary component.
			m_CurrRow->setAttr( "ss:StyleID","s25" );
		}

		char szDepth[64] = "                                                              ";
		if (rComp.nDepth < sizeof(szDepth))
			szDepth[rComp.nDepth*2] = '\0';

		string sCompDisplayName = szDepth;
		sCompDisplayName += rComp.strName;
		
		AddCell( sCompDisplayName.c_str() );
		//char szSize[32];
		//sprintf(szSize, "%s%7.3f", szDepth,rComp.getSizeMBytes() );
		//AddCell( szSize );
		//sprintf(szSize, "%s%7.3f", szDepth,rComp.getTotalSizeMBytes() );
		//AddCell( szSize );

		if (rComp.sizeBytes > 0)
			AddCell( (unsigned int)(rComp.sizeBytes/1024) );
		else
			AddCell( "" );
		AddCell( (unsigned int)(rComp.sizeBytesTotal/1024) );

		if (rComp.numObjects > 0)
		{
			AddCell( (unsigned int)(rComp.numObjects) );
		}

		//if (rComp.sizeBytesTotal <= m_nMinSubcomponentBytes || rComp.nDepth > m_nMaxSubcomponentDepth)
			//continue;



		/*
		char szDepth[32] = " ..............................";
		if (rComp.nDepth < sizeof(szDepth))
			szDepth[rComp.nDepth] = '\0';

		char szSize[32];
		if (rComp.sizeBytes > 0)
		{
			if (rComp.sizeBytesTotal > rComp.sizeBytes)
				sprintf (szSize, "%7.3f  %7.3f", rComp.getTotalSizeMBytes(), rComp.getSizeMBytes());
			else
				sprintf (szSize, "         %7.3f", rComp.getSizeMBytes());
		}
		else
		{
			assert (rComp.sizeBytesTotal > 0);
			sprintf (szSize, "%7.3f         ", rComp.getTotalSizeMBytes());
		}
		char szCount[16];

		if (rComp.numObjects)
			sprintf (szCount, "%8u", rComp.numObjects);
		else
			szCount[0] = '\0';

		m_pLog->LogToFile ("%s%-*s:%s%s",szDepth, nNameWidth-rComp.nDepth,rComp.strName.c_str(), szSize, szCount);
		*/
	}
	
	//delete m_pStats;
	//delete m_pSizer;
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::ExportMemInfo( SCryEngineStats &stats )
{
	NewWorksheet( "Modules Memory Info" );
	XmlNodeRef Column;
	Column = m_CurrTable->newChild("Column"); Column->setAttr( "ss:Width",300 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",90 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",90 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",90 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",20 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",90 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",90 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",90 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",90 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",90 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",90 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",90 );

	AddRow();
	m_CurrRow->setAttr( "ss:StyleID","s25" );
	AddCell( "Module" );
	AddCell( "Dynamic(KB)" );
	AddCell( "Num Allocs" );
	AddCell( "Sum Of Allocs (KB)" );
	AddCell( "" );
	AddCell( "Static Total (KB)" );
	AddCell( "Static Code (KB)" );
	AddCell( "Static Init. Data (KB)" );
	AddCell( "Static Uninit. Data (KB)" );

	AddCell( "Strings (KB)" );
	AddCell( "STL (KB)" );
	AddCell( "STL Wasted (KB)" );

	AddCell( "Dynamic - Wasted (KB)" );

	AddRow();

	//////////////////////////////////////////////////////////////////////////
	int nRows = 0;

	for (int i = 0; i < stats.memInfo.modules.size(); i++)
	{
		SCryEngineStats::ModuleInfo &moduleInfo = stats.memInfo.modules[i];
		const char *szModule = moduleInfo.name;

		AddRow();
		nRows++;
		AddCell( szModule,CELL_BOLD );
		
		AddCell( moduleInfo.usedInModule/1024 );
		AddCell( moduleInfo.memInfo.num_allocations );
		AddCell( moduleInfo.memInfo.allocated/1024 );
		AddCell( "" );
		AddCell( moduleInfo.moduleStaticSize/1024 );
		AddCell( (uint32)moduleInfo.SizeOfCode/1024 );
		AddCell( (uint32)moduleInfo.SizeOfInitializedData/1024 );
		AddCell( (uint32)moduleInfo.SizeOfUninitializedData/1024 );
		AddCell( (uint32)(moduleInfo.memInfo.CryString_allocated/1024) );
		AddCell( (uint32)(moduleInfo.memInfo.STL_allocated/1024) );
		AddCell( (uint32)(moduleInfo.memInfo.STL_wasted/1024) );

		AddCell( (uint32)((moduleInfo.memInfo.allocated - moduleInfo.memInfo.requested)/1024) );
	}

	AddRow();
	AddCell("");
	m_CurrRow->setAttr( "ss:StyleID","s25" );
	AddCell_SumOfRows( nRows );
	AddCell_SumOfRows( nRows );
	AddCell_SumOfRows( nRows );
	AddCell("");
	AddCell_SumOfRows( nRows );
	AddCell_SumOfRows( nRows );
	AddCell_SumOfRows( nRows );
	AddRow();
	//AddCell( "" );
	
	//AddRow();
	//AddCell( "Dynamic Textures (KB)",CELL_BOLD );
	//AddCell( m_stats.nAPI_DynTextureSize/(1024) );

	AddRow();
	AddCell( "Lua Memory Usage (KB)",CELL_BOLD );
	AddCell( stats.nSummaryScriptSize/(1024) );
	AddRow();
	AddCell( "Total Num Allocs",CELL_BOLD );
	AddCell( stats.memInfo.totalNumAllocsInModules );
	AddRow();
	AddCell( "Total Allocated (KB)",CELL_BOLD );
	AddCell( stats.memInfo.totalUsedInModules/1024 );
	AddRow();
	AddCell( "Total Static (KB)",CELL_BOLD );
	AddCell( stats.memInfo.totalUsedInModulesStatic/1024 );
	AddRow();
	AddCell( "Sum of All Allocs (KB)",CELL_BOLD );
	AddCell( stats.memInfo.totalAllocatedInModules/1024 );

	AddRow();
	AddRow();
	AddCell( "API Textures (KB)",CELL_BOLD );
	AddCell( stats.nSummary_TextureSize/1024 );
	AddRow();
	AddCell( "API Meshes (KB)",CELL_BOLD );
	AddCell( stats.nAPI_MeshSize/1024 );
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::ExportTimeDemoInfo(SCryEngineStats &stats)
{
	if (!GetISystem()->GetITestSystem())
		return;

	if(!IsDemoPlayback())
		return;
	
	NewWorksheet( "Demo" );

	XmlNodeRef Column;
	Column = m_CurrTable->newChild("Column"); Column->setAttr( "ss:Width",400 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",80 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",80 );

	AddRow(); AddCell( "Average FPS:",CELL_BOLD ); AddCell( stats.infoFPS.fAverageFPS );
	AddRow(); AddCell( "Min FPS:",CELL_BOLD ); AddCell( stats.infoFPS.fMinFPS ); //AddCell( "At Frame:" ); AddCell( pTD->minFPS_Frame );
	AddRow(); AddCell( "Max FPS:",CELL_BOLD ); AddCell( stats.infoFPS.fMaxFPS ); //AddCell( "At Frame:" ); AddCell( pTD->maxFPS_Frame );
	
	AddRow(); AddCell( "Average Polys/frame:",CELL_BOLD ); AddCell(stats.infoFPS.averagePolys );
	AddRow(); AddCell( "Min Polys/frame:", CELL_BOLD); AddCell(stats.infoFPS.minPolys);
	AddRow(); AddCell( "Max Polys/frame:", CELL_BOLD); AddCell(stats.infoFPS.maxPolys);

	AddRow(); AddCell( "Average drawcalls/frame:",CELL_BOLD ); AddCell(stats.infoFPS.averageDrawCalls );
	AddRow(); AddCell( "Min drawcalls/frame:", CELL_BOLD); AddCell(stats.infoFPS.minDrawCalls);
	AddRow(); AddCell( "Max drawcalls/frame:", CELL_BOLD); AddCell(stats.infoFPS.maxDrawCalls);

	//////////////////////////////////////////////////////////////////////////
	NewWorksheet( "DemoFrames" );
	Column = m_CurrTable->newChild("Column"); Column->setAttr( "ss:Width",80 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",80 );
	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",80 );
	AddRow();
	m_CurrRow->setAttr( "ss:StyleID","s25" );
	AddCell( "Frame Number" );
	AddCell( "Frame Rate" );
	AddCell( "Rendered Polygons" );
	AddCell( "Draw Calls" );

	AddRow();

	for (int i = 0; i < stats.infoFPS.pFrameRecords->size(); i++)
	{
		AddRow();
		AddCell( (*stats.infoFPS.pFrameRecords)[i].frameNumber);
		AddCell( (*stats.infoFPS.pFrameRecords)[i].fps );
		AddCell( (*stats.infoFPS.pFrameRecords)[i].polys );
		AddCell( (*stats.infoFPS.pFrameRecords)[i].drawCalls );
	}
}

//////////////////////////////////////////////////////////////////////////
static void SaveLevelStats( IConsoleCmdArgs *pArgs )
{
	string levelName = "no_level";

	ICVar *sv_map = gEnv->pConsole->GetCVar("sv_map");
	if (sv_map)
		levelName = sv_map->GetString();

	levelName = PathUtil::GetFileName(levelName);

	if(pArgs->GetArgCount() > 1)
		levelName += pArgs->GetArg(1);

	CEngineStats engineStats;
	engineStats.Collect(0);

	// level.xml
	{
		CStatsToExcelExporter excelExporter;

		string filename = PathUtil::ReplaceExtension(levelName,"xml");

		excelExporter.ExportToFile( engineStats.m_stats,filename );

		CryLog("SaveLevelStats exported '%s'",filename.c_str());
	}

	// level_dependencies.xml
	{
		CStatsToExcelExporter excelExporter;

		engineStats.m_ResourceCollector.ComputeDependencyCnt();

		string filename = PathUtil::ReplaceExtension(string("depends_")+levelName,"xml");

		excelExporter.ExportDependenciesToFile( engineStats.m_ResourceCollector,filename );	

		// log to log file - modifies engineStats.m_ResourceCollector data
//		engineStats.m_ResourceCollector.LogData(*GetISystem()->GetILog());
		CryLog("SaveLevelStats exported '%s'",filename.c_str());
	}

	// Clean STL after heavy operation
	STLALLOCATOR_CLEANUP
}

void RegisterEngineStatistics()
{
	gEnv->pConsole->AddCommand("SaveLevelStats",SaveLevelStats,0,
		"Calling this command creates multiple XML files with level statistics.\n"
		"The data includes file usage, dependencies, size in more/disk.\n"
		"The files can be loaded in Excel.");
}

#else //WIN32

void RegisterEngineStatistics()
{
}

#endif //WIN32
