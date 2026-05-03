#include "CryCommon/CryCore/MTPseudoRandom.h"
#include "CryCommon/CryNetwork/ISerialize.h"
#include "CryCommon/CrySystem/ISystem.h"

#include "ArticulatedEntity.h"
#include "BoxGeom.h"
#include "CapsuleGeom.h"
#include "ContactSolver.h"
#include "CylinderGeom.h"
#include "Geometry.h"
#include "LivingEntity.h"
#include "ParticleEntity.h"
#include "PhysArea.h"
#include "PhysicalEntity.h"
#include "PhysicalWorld.h"
#include "Qhull.h"
#include "RayGeom.h"
#include "RigidEntity.h"
#include "RopeEntity.h"
#include "SoftEntity.h"
#include "TetrLattice.h"
#include "TriMesh.h"
#include "WaterMan.h"
#include "WheeledVehicleEntity.h"

constexpr int EVENT_CHUNK_SZ = 8192 - sizeof(void*);
constexpr int pef_step_requested = 0x40000000;

int g_nPhysWorlds;
CPhysicalWorld* g_pPhysWorlds[64];

struct SExplosionShape
{
	int id;
	IGeometry* pGeom;
	float size, rsize;
	int idmat;
	float probability;
	int iFirstByMat, nSameMat;
	int bCreateConstraint;
};

struct SRwiRequest
{
	void* pForeignData;
	int iForeignData;
	Vec3 org, dir;
	int objtypes;
	unsigned int flags;
	ray_hit* hits;
	int nMaxHits;
	int idSkipEnts[5];
	ray_hit_cached* phitLast;
	int nSkipEnts;
	int iCaller;
};

struct SPwiRequest
{
	void* pForeignData;
	int iForeignData;
	int itype;
	char pprim[std::max({sizeof(primitives::box), sizeof(primitives::cylinder), sizeof(primitives::capsule),
	                     sizeof(primitives::sphere)})];
	Vec3 sweepDir;
	int entTypes;
	int geomFlagsAll;
	int geomFlagsAny;
	int idSkipEnts[4];
	int nSkipEnts;
};

struct pe_gridthunk
{
	uint64 inext : 20; // int64 for tighter packing
	uint64 iprev : 20;
	uint64 inextOwned : 20;
	uint64 iSimClass : 3;
	uint64 bFirstInCell : 1;
	unsigned char BBox[4];
	CPhysicalPlaceholder* pent;
};

CPhysicalWorld::CPhysicalWorld()
{
	Init();
	g_pPhysWorlds[g_nPhysWorlds] = this;
	g_nPhysWorlds = min(g_nPhysWorlds + 1, sizeof(g_pPhysWorlds) / sizeof(g_pPhysWorlds[0]));
	m_pEntBeingDeleted = 0;
	m_bGridThunksChanged = 0;
	m_bUpdateOnlyFlagged = 0;
	m_lockStep = 0;
	m_lockQueue = 0;
	m_lockGrid = 0;
	m_lockList = 0;
	m_lockEventsQueue = 0;
	m_lockEntIdList = 0;
	m_lockCaller[0] = m_lockCaller[1] = 0;
	m_lockPODGrid = 0;
	m_lockFuncProfiler = 0;
	m_nOnDemandListFailures = 0;
	m_iLastPODUpdate = 1;
	m_dummyPODcell.zlim[0] = 1E10f;
	m_dummyPODcell.zlim[1] = 1E10f;
	m_gthunks = 0;
	m_iFreeGThunk0 = 0;
	m_thunkPoolSz = 0;
}

CPhysicalWorld::~CPhysicalWorld()
{
	Shutdown();
	int i;
	for (i = 0; i < g_nPhysWorlds && g_pPhysWorlds[i] != this; i++)
		;
	if (i < g_nPhysWorlds)
	{
		g_nPhysWorlds--;
	}
	for (; i < g_nPhysWorlds; i++)
	{
		g_pPhysWorlds[i] = g_pPhysWorlds[i + 1];
	}
}

void CPhysicalWorld::Init()
{
	InitGeoman();
	m_pTmpEntList = 0;
	m_pTmpEntList1 = 0;
	m_pTmpEntList2 = 0;
	m_pGroupMass = 0;
	m_pMassList = 0;
	m_pGroupIds = 0;
	m_pGroupNums = 0;
	m_nEnts = 0;
	m_nEntsAlloc = 0;
	m_bEntityCountReserved = 0;
	m_pEntGrid = 0;
	m_timePhysics = m_timeSurplus = 0;
	m_timeSnapshot[0] = m_timeSnapshot[1] = m_timeSnapshot[2] = m_timeSnapshot[3] = 0;
	m_iTimeSnapshot[0] = m_iTimeSnapshot[1] = m_iTimeSnapshot[2] = m_iTimeSnapshot[3] = 0;
	m_iTimePhysics = 0;
	m_pHeightfield[0] = m_pHeightfield[1] = 0;
	int i;
	for (i = 0; i < 8; i++)
	{
		m_pTypedEnts[i] = m_pTypedEntsPerm[i] = 0;
		m_updateTimes[i] = 0;
	}
	for (i = 0; i < 10; i++)
	{
		m_nTypeEnts[i] = 0;
	}
	m_vars.nMaxSubsteps = 10;
	for (i = 0; i < NSURFACETYPES; i++)
	{
		m_BouncinessTable[i] = 0;
		m_FrictionTable[i] = 1.2f;
		m_DynFrictionTable[i] = 1.2f / 1.5f;
		m_SurfaceFlagsTable[i] = 0;
	}
	m_vars.nMaxStackSizeMC = 8;
	m_vars.maxMassRatioMC = 50.0f;
	m_vars.nMaxMCiters = 6000;
	m_vars.nMaxMCitersHopeless = 6000;
	m_vars.accuracyMC = 0.005f;
	m_vars.accuracyLCPCG = 0.005f;
	m_vars.nMaxContacts = 150;
	m_vars.nMaxPlaneContacts = 8;
	m_vars.nMaxPlaneContactsDistress = 4;
	m_vars.nMaxLCPCGsubiters = 120;
	m_vars.nMaxLCPCGsubitersFinal = 250;
	m_vars.nMaxLCPCGmicroiters = 12000;
	m_vars.nMaxLCPCGmicroitersFinal = 25000;
	m_vars.nMaxLCPCGiters = 5;
	m_vars.minLCPCGimprovement = 0.05f;
	m_vars.nMaxLCPCGFruitlessIters = 4;
	m_vars.accuracyLCPCGnoimprovement = 0.05f;
	m_vars.minSeparationSpeed = 0.02f;
	m_vars.maxwCG = 500.0f;
	m_vars.maxvCG = 500.0f;
	m_vars.maxvUnproj = 10.0f;
	m_vars.maxMCMassRatio = 100.0f;
	m_vars.maxMCVel = 15.0f;
	m_vars.maxLCPCGContacts = 100;
	m_vars.bFlyMode = 0;
	m_vars.iCollisionMode = 0;
	m_vars.bSingleStepMode = 0;
	m_vars.bDoStep = 0;
	m_vars.fixedTimestep = 0;
	m_vars.timeGranularity = 0.0001f;
	m_vars.maxWorldStep = 0.2f;
	m_vars.iDrawHelpers = 0;
	m_vars.nMaxSubsteps = 5;
	m_vars.nMaxSurfaces = NSURFACETYPES;
	m_vars.maxContactGap = 0.01f;
	m_vars.maxContactGapPlayer = 0.01f;
	m_vars.bProhibitUnprojection = 1; // 2;
	m_vars.bUseDistanceContacts = 0;
	m_vars.unprojVelScale = 10.0f;
	m_vars.maxUnprojVel = 2.5f;
	m_vars.gravity.Set(0, 0, -9.8f);
	m_vars.nGroupDamping = 8;
	m_vars.groupDamping = 0.5f;
	m_vars.nMaxSubstepsLargeGroup = 5;
	m_vars.nBodiesLargeGroup = 30;
	m_vars.bEnforceContacts = 1; // 1;
	m_vars.bBreakOnValidation = 0;
	m_vars.bLogActiveObjects = 0;
	m_vars.bMultiplayer = 0;
	m_vars.bProfileEntities = 0;
	m_vars.bProfileFunx = 0;
	m_vars.minBounceSpeed = 6;
	m_vars.nGEBMaxCells = 800;
	m_vars.maxVel = 100.0f;
	m_vars.maxVelPlayers = 150.0f;
	m_vars.bSkipRedundantColldet = 1;
	m_vars.penaltyScale = 0.3f;
	m_vars.maxContactGapSimple = 0.03f;
	m_vars.bLimitSimpleSolverEnergy = 1;
	m_vars.nMaxEntityCells = 300000;
	m_vars.nMaxAreaCells = 128;
	m_vars.nMaxEntityContacts = 256;
	m_vars.tickBreakable = 0.1f;
	m_vars.approxCapsLen = 1.2f;
	m_vars.nMaxApproxCaps = 7;
	m_vars.bCGUnprojVel = 0;
	m_vars.bLogLatticeTension = 0;
	m_vars.nMaxLatticeIters = 100000;
	m_vars.bLogStructureChanges = 1;
	m_vars.bPlayersCanBreak = 0;
	m_vars.bMultithreaded = 0;
	m_vars.breakImpulseScale = 1.0f;
	m_vars.massLimitDebris = 1E10f;
	m_vars.maxSplashesPerObj = 0;
	m_vars.splashDist0 = 7.0f;
	m_vars.minSplashForce0 = 15000.0f;
	m_vars.minSplashVel0 = 4.5f;
	m_vars.splashDist1 = 30.0f;
	m_vars.minSplashForce1 = 150000.0f;
	m_vars.minSplashVel1 = 10.0f;
	m_vars.lastTimeStep = 0;
	MARK_UNUSED m_vars.flagsColliderDebris;
	m_vars.flagsANDDebris = -1;
	m_vars.bDebugExplosions = 0;
	m_vars.netMinSnapDist = 0.1f;
	m_vars.netVelSnapMul = 0.1f;
	m_vars.netMinSnapDot = 0.99f;
	m_vars.netAngSnapMul = 0.01f;
	m_vars.netSmoothTime = 5.0f;
	m_iNextId = 1;
	m_iNextIdDown = m_lastExtId = m_nExtIds = 0;
	m_pEntsById = 0;
	m_nIdsAlloc = 0;
	m_nOccRes = 0;
	m_nExplVictims = m_nExplVictimsAlloc = 0;
	m_pPlaceholders = 0;
	m_pPlaceholderMap = 0;
	m_nPlaceholders = m_nPlaceholderChunks = 0;
	m_iLastPlaceholder = -1;
	m_pPhysicsStreamer = 0;
	m_pEventClient = 0;
	m_nProfiledEnts = 0;
	m_iSubstep = 0;
	m_bWorldStep = 0;
	m_nDynamicEntitiesDeleted = 0;
	m_nQueueSlots = m_nQueueSlotsAlloc = 0;
	m_nQueueSlotSize = QUEUE_SLOT_SZ;
	m_pQueueSlots = 0;
	for (i = 0; i < EVENT_TYPES_NUM; i++)
	{
		m_pFreeEvents[i] = 0;
		m_pEventClients[i][0] = m_pEventClients[i][1] = 0;
		m_nEvents[i] = 0;
	}
	m_pEventFirst = m_pEventLast = 0;
	m_pFirstEventChunk = m_pCurEventChunk = (EventChunk*)(new char[sizeof(EventChunk) + EVENT_CHUNK_SZ]);
	m_szCurEventChunk = 0;
	m_pFirstEventChunk->next = 0;
	m_pGlobalArea = 0;
	m_nAreas = m_nBigAreas = 0;
	m_pDeletedAreas = 0;
	m_iLastLogPump = 0;
	m_pFreeContact = m_pLastFreeContact = CONTACT_END(m_pFreeContact);
	m_nFreeContacts = 0;
	m_pExpl = 0;
	m_nExpl = m_nExplAlloc = 0;
	m_idExpl = 0;
	m_pDeformingEnts = 0;
	m_nDeformingEnts = m_nDeformingEntsAlloc = 0;
	m_pRenderer = 0;
	m_lockDeformingEntsList = 0;
	m_lockAreas = 0;
	m_matWater = -1;
	m_bCheckWaterHits = 0;
	g_StaticPhysicalEntity.m_pWorld = this;
	g_StaticPhysicalEntity.m_id = -2;
	g_StaticPhysicalEntity.m_parts[0].pos.zero();
	g_StaticPhysicalEntity.m_parts[0].q.SetIdentity();
	g_StaticPhysicalEntity.m_parts[0].scale = 1.0f;
	m_rwiQueueHead = -1;
	m_rwiQueueTail = -64;
	m_rwiQueueSz = m_rwiQueueAlloc = 0;
	m_rwiQueue = 0;
	m_lockRwiQueue = 0;
	m_pRwiHitsTail = (m_pRwiHitsHead = new ray_hit[256]) + 255;
	for (i = 0; i < 255; i++)
	{
		m_pRwiHitsHead[i].next = m_pRwiHitsHead + i + 1;
	}
	m_pRwiHitsHead[i].next = m_pRwiHitsHead;
	m_rwiPoolEmpty = 1;
	m_rwiHitsPoolSize = 256;
	m_lockRwiHitsPool = 0;
	m_lockTPR = 0;
	m_pwiQueueHead = -1;
	m_pwiQueueTail = 0;
	m_pwiQueueSz = m_pwiQueueAlloc = 0;
	m_pwiQueue = 0;
	m_lockPwiQueue = 0;
	m_pWaterMan = 0;
	m_idStep = 0;
	m_nGEA[0] = m_nGEA[1] = 0;
	m_nEntListAllocs = 0;
	m_pentLastHit[0] = m_pentLastHit[1] = 0;
	m_curGroupMass = 0;
	m_pPODCells = &(m_pDummyPODcell = &m_dummyPODcell);
	m_dummyPODcell.lifeTime = 1E10f;
	m_dummyPODcell.zlim.set(1E10f, -1E10f);
	m_iActivePODCell0 = -1;
	m_bHasPODGrid = 0;
	m_iLastPODUpdate = 1;
	m_nProfileFunx = m_nProfileFunxAlloc = 0;
	m_pFuncProfileData = 0;
	m_posViewer.zero();
	memset(m_gthunks = new pe_gridthunk[m_thunkPoolSz = 16384], 0, 16384 * sizeof(pe_gridthunk));
	for (i = 1; i < 1024 - 1; i++)
	{
		m_gthunks[i].inextOwned = i + 1;
	}
	m_gthunks[i].inextOwned = 0;
	m_gthunks[0].inext = m_gthunks[0].iprev = 0;
	m_iFreeGThunk0 = 1;
	m_bCurGroupInvisible = 0;
	m_nContactsAlloc = 0;
	m_prevGEABBox[0].Set(1E10f, 1E10f, 1E10f);
	m_prevGEABBox[1].zero();
	m_prevGEAobjtypes = m_nprevGEAEnts = 0;
}

void CPhysicalWorld::Shutdown(int bDeleteGeometries)
{
	int i;
	CPhysicalEntity *pent, *pent_next;
	for (i = 0; i < 8; i++)
	{
		for (pent = m_pTypedEnts[i]; pent; pent = pent_next)
		{
			pent_next = pent->m_next;
			delete pent;
		}
		m_pTypedEnts[i] = 0;
		m_pTypedEntsPerm[i] = 0;
	}
	m_nEnts = m_nEntsAlloc = 0;
	m_bEntityCountReserved = 0;
	for (i = 0; i < m_nPlaceholderChunks; i++)
	{
		if (m_pPlaceholders[i])
		{
			delete[] m_pPlaceholders[i];
		}
	}
	if (m_pPlaceholders)
	{
		delete[] m_pPlaceholders;
	}
	if (m_pPlaceholderMap)
	{
		delete[] m_pPlaceholderMap;
	}
	m_nPlaceholderChunks = m_nPlaceholders = 0;
	m_iLastPlaceholder = -1;
	if (m_pEntGrid)
	{
		delete[] m_pEntGrid;
	}
	m_pEntGrid = 0;
	if (m_gthunks)
	{
		delete[] m_gthunks;
	}
	m_gthunks = 0;
	m_thunkPoolSz = m_iFreeGThunk0 = 0;
	if (m_pTmpEntList)
	{
		delete[] m_pTmpEntList;
	}
	m_pTmpEntList = 0;
	if (m_pTmpEntList1)
	{
		delete[] m_pTmpEntList1;
	}
	m_pTmpEntList1 = 0;
	if (m_pTmpEntList2)
	{
		delete[] m_pTmpEntList2;
	}
	m_pTmpEntList2 = 0;
	if (m_pGroupMass)
	{
		delete[] m_pGroupMass;
	}
	m_pGroupMass = 0;
	if (m_pMassList)
	{
		delete[] m_pMassList;
	}
	m_pMassList = 0;
	if (m_pGroupIds)
	{
		delete[] m_pGroupIds;
	}
	m_pGroupIds = 0;
	if (m_pGroupNums)
	{
		delete[] m_pGroupNums;
	}
	m_pGroupNums = 0;
	if (m_pEntsById)
	{
		delete[] m_pEntsById;
	}
	m_pEntsById = 0;
	m_nIdsAlloc = 0;
	if (m_nOccRes)
	{
		for (i = 0; i < 6; i++)
		{
			delete[] m_pGridStat[i];
			delete[] m_pGridDyn[i];
		}
	}
	if (m_nExplVictimsAlloc)
	{
		delete[] m_pExplVictims;
		delete[] m_pExplVictimsFrac;
		delete[] m_pExplVictimsImp;
	}

	if (bDeleteGeometries)
	{
		for (i = 0; i < m_nQueueSlots; i++)
		{
			delete[] m_pQueueSlots[i];
		}
		if (m_nQueueSlots)
		{
			m_pQueueSlots;
		}
		m_nQueueSlots = m_nQueueSlotsAlloc = 0;
		m_nQueueSlotSize = QUEUE_SLOT_SZ;
		m_pQueueSlots = 0;
		for (i = 0; i < EVENT_TYPES_NUM; i++)
		{
			EventClient *pClient, *pNextClient;
			for (pClient = m_pEventClients[i][0]; pClient; pClient = pNextClient)
			{
				pNextClient = pClient->next;
				delete pClient;
			}
			for (pClient = m_pEventClients[i][1]; pClient; pClient = pNextClient)
			{
				pNextClient = pClient->next;
				delete pClient;
			}
			m_pEventClients[i][0] = m_pEventClients[i][1] = 0;
		}
		m_pEventFirst = m_pEventLast = 0;
		EventChunk *pChunk, *pChunkNext;
		for (pChunk = m_pFirstEventChunk; pChunk; pChunk = pChunkNext)
		{
			pChunkNext = pChunk->next;
			delete[] pChunk;
		}
		m_szCurEventChunk = 0;
		m_pFirstEventChunk = m_pCurEventChunk = 0;
		entity_contact *pContact, *pContactNext;
		for (pContact = m_pFreeContact; pContact != CONTACT_END(m_pFreeContact); pContact = pContact->next)
		{
			if (!pContact->bChunkStart)
			{
				pContact->prev->next = pContact->next;
				pContact->next->prev = pContact->prev;
			}
		}
		for (pContact = m_pFreeContact; pContact != CONTACT_END(m_pFreeContact); pContact = pContactNext)
		{
			pContactNext = pContact->next;
			delete[] pContact;
		}
		m_pFreeContact = m_pLastFreeContact = CONTACT_END(m_pFreeContact);
		m_nContactsAlloc = 0;
		if (m_pExpl)
		{
			for (i = 0; i < m_nExpl; i++)
			{
				m_pExpl[i].pGeom->Release();
			}
			delete[] m_pExpl;
			m_pExpl = 0;
		}
		m_nExpl = m_nExplAlloc = 0;
		m_idExpl = 0;
		if (m_rwiQueue)
		{
			delete[] m_rwiQueue;
		}
		m_rwiQueueHead = -1;
		m_rwiQueueTail = -64;
		m_rwiQueueSz = m_rwiQueueAlloc = 0;
		m_rwiQueue = 0;
		m_lockRwiQueue = 0;
		if (m_pRwiHitsHead)
		{
			ray_hit *phit = m_pRwiHitsHead, *pchunk = 0, *phit_next;
			do
			{
				if ((phit_next = phit->next) != phit + 1)
				{
					if (pchunk)
					{
						delete[] pchunk;
					}
					pchunk = phit_next;
				}
			}
			while ((phit = phit_next) != m_pRwiHitsHead);
			delete[] pchunk;
			m_pRwiHitsHead = m_pRwiHitsTail = 0;
			m_rwiHitsPoolSize = 0;
		}
		if (m_pwiQueue)
		{
			delete[] m_pwiQueue;
		}
		m_pwiQueueHead = -1;
		m_pwiQueueTail = 0;
		m_pwiQueueSz = m_pwiQueueAlloc = 0;
		m_pwiQueue = 0;
		m_lockPwiQueue = 0;
		DestroyWaterManager();

		SetHeightfieldData(0);
		ShutDownGeoman();
	}
}

IPhysicalEntity* CPhysicalWorld::SetHeightfieldData(const primitives::heightfield* phf, int* pMatMapping, int nMats)
{
	int iCaller;
	if (!phf)
	{
		for (iCaller = 0; iCaller < 2; iCaller++)
		{
			if (m_pHeightfield[iCaller])
			{
				m_pHeightfield[iCaller]->m_parts[0].pPhysGeom->pGeom->Release();
				delete m_pHeightfield[iCaller]->m_parts[0].pPhysGeom;
				m_pHeightfield[iCaller]->m_parts[0].pPhysGeom = 0;
				if (m_pHeightfield[iCaller]->m_parts[0].pMatMapping)
				{
					delete[] m_pHeightfield[iCaller]->m_parts[0].pMatMapping;
				}
				delete m_pHeightfield[iCaller];
			}
			m_pHeightfield[iCaller] = 0;
		}
		return 0;
	}
	for (iCaller = 0; iCaller < 2; iCaller++)
	{
		CGeometry* pGeom = (CGeometry*)CreatePrimitive(primitives::heightfield::type, phf);
		if (m_pHeightfield[iCaller])
		{
			m_pHeightfield[iCaller]->m_parts[0].pPhysGeom->pGeom->Release();
			if (m_pHeightfield[iCaller]->m_parts[0].pMatMapping)
			{
				delete[] m_pHeightfield[iCaller]->m_parts[0].pMatMapping;
			}
		}
		else
		{
			m_pHeightfield[iCaller] = new CPhysicalEntity(this);
			m_pHeightfield[iCaller]->m_parts[0].pPhysGeom =
			    m_pHeightfield[iCaller]->m_parts[0].pPhysGeomProxy = new phys_geometry;
			m_pHeightfield[iCaller]->m_parts[0].id = 0;
			m_pHeightfield[iCaller]->m_parts[0].scale = 1.0;
			m_pHeightfield[iCaller]->m_parts[0].mass = 0;
			m_pHeightfield[iCaller]->m_parts[0].flags = geom_collides;
			m_pHeightfield[iCaller]->m_parts[0].minContactDist = phf->step.x;
			m_pHeightfield[iCaller]->m_parts[0].idmatBreakable = -1;
			m_pHeightfield[iCaller]->m_parts[0].pLattice = 0;
			m_pHeightfield[iCaller]->m_parts[0].nMats = 0;
			m_pHeightfield[iCaller]->m_nParts = 1;
			m_pHeightfield[iCaller]->m_id = -1;
		}
		if (pMatMapping)
		{
			memcpy(m_pHeightfield[iCaller]->m_parts[0].pMatMapping = new int[nMats], pMatMapping,
			       (m_pHeightfield[iCaller]->m_parts[0].nMats = nMats) * sizeof(int));
		}
		else
		{
			m_pHeightfield[iCaller]->m_parts[0].pMatMapping = 0;
		}
		m_HeightfieldBasis = phf->Basis;
		m_HeightfieldOrigin = phf->origin;
		m_pHeightfield[iCaller]->m_parts[0].pPhysGeom->pGeom = pGeom;
		m_pHeightfield[iCaller]->m_parts[0].pos = phf->origin;
		m_pHeightfield[iCaller]->m_parts[0].q = !quaternionf(phf->Basis);
		m_pHeightfield[iCaller]->m_parts[0].BBox[0].zero();
		m_pHeightfield[iCaller]->m_parts[0].BBox[1].zero();
	}
	return m_pHeightfield[0];
}

IPhysicalEntity* CPhysicalWorld::GetHeightfieldData(primitives::heightfield* phf)
{
	if (m_pHeightfield[0])
	{
		m_pHeightfield[0]->m_parts[0].pPhysGeom->pGeom->GetPrimitive(0, phf);
		phf->Basis = m_HeightfieldBasis;
		phf->origin = m_HeightfieldOrigin;
	}
	return m_pHeightfield[0];
}

void CPhysicalWorld::SetupEntityGrid(int axisz, Vec3 org, int nx, int ny, float stepx, float stepy)
{
	int i;
	if (m_pEntGrid)
	{
		for (i = m_entgrid.size.x * m_entgrid.size.y; i >= 0; i--)
		{
			if (m_pEntGrid[i])
			{
				m_gthunks[m_pEntGrid[i]].iprev = 0;
			}
		}
		delete[] m_pEntGrid;
	}
	m_iEntAxisz = axisz;
	m_entgrid.size.set(nx, ny);
	m_entgrid.stride.set(1, nx);
	m_entgrid.step.set(stepx, stepy);
	m_entgrid.stepr.set(1.0f / stepx, 1.0f / stepy);
	m_entgrid.origin = org;
	m_pEntGrid = new int[(nx * ny) + 1];
	for (i = nx * ny; i >= 0; i--)
	{
		m_pEntGrid[i] = 0;
	}
	m_PODstride.set(1, ny >> 3);
}

void CPhysicalWorld::DeactivateOnDemandGrid()
{
	if (m_bHasPODGrid)
	{
		int i;
		for (i = m_entgrid.size.x * m_entgrid.size.y >> 6; i >= 0; i--)
		{
			if (m_pPODCells[i])
			{
				delete[] m_pPODCells[i];
			}
		}
		delete[] m_pPODCells;
		m_pPODCells = &m_pDummyPODcell;
		m_dummyPODcell.lifeTime = 1E10f;
		m_iActivePODCell0 = -1;
		m_bHasPODGrid = 0;
		for (i = 0; i < 8; i++)
		{
			for (CPhysicalEntity* pent = m_pTypedEnts[i]; pent; pent = pent->m_next)
			{
				pent->m_nRefCount -= pent->m_nRefCountPOD;
				pent->m_nRefCountPOD = 0;
			}
		}
	}
}

void CPhysicalWorld::RegisterBBoxInPODGrid(const Vec3* BBox)
{
	int i, ix, iy, igx[2], igy[2], imask;
	pe_PODcell* pPODcell;
	if (!m_bHasPODGrid)
	{
		if ((m_entgrid.size.x | m_entgrid.size.y) & 7)
		{
			return;
		}
		memset(m_pPODCells = new pe_PODcell*[(m_entgrid.size.x * m_entgrid.size.y >> 6) + 1], 0,
		       ((m_entgrid.size.x * m_entgrid.size.y >> 6) + 1) * sizeof(m_pPODCells[0]));
		m_iActivePODCell0 = -1;
		m_bHasPODGrid = 1;
	}

	for (i = 0; i < 2; i++)
	{
		igx[i] = max(-1, min(m_entgrid.size.x, physics_float2int(((BBox[i][inc_mod3[m_iEntAxisz]] -
		                                                           m_entgrid.origin[inc_mod3[m_iEntAxisz]]) *
		                                                          m_entgrid.stepr.x) -
		                                                         0.5f)));
		igy[i] = max(-1, min(m_entgrid.size.y, physics_float2int(((BBox[i][dec_mod3[m_iEntAxisz]] -
		                                                           m_entgrid.origin[dec_mod3[m_iEntAxisz]]) *
		                                                          m_entgrid.stepr.y) -
		                                                         0.5f)));
	}
	for (ix = igx[0]; ix <= igx[1]; ix++)
	{
		for (iy = igy[0]; iy <= igy[1]; iy++)
		{
			imask = -(inrange(ix, -1, m_entgrid.size.x) & inrange(iy, -1, m_entgrid.size.y));
			i = ((ix >> 3) * m_PODstride.x) + ((iy >> 3) * m_PODstride.y);
			i = i + ((m_entgrid.size.x * m_entgrid.size.y >> 6) - i & ~imask);
			if (!m_pPODCells[i])
			{
				memset(m_pPODCells[i] = new pe_PODcell[64], 0, sizeof(pe_PODcell) * 64);
				for (int j = 0; j < 64; j++)
				{
					m_pPODCells[i][j].zlim.set(1000.0f, -1000.0f);
				}
			}
			pPODcell = m_pPODCells[i] + ((ix & 7) + (iy & 7) * 8 & imask);
			pPODcell->zlim[0] = min(pPODcell->zlim[0], BBox[0][m_iEntAxisz]);
			pPODcell->zlim[1] = max(pPODcell->zlim[1], BBox[1][m_iEntAxisz]);
		}
	}
}

int CPhysicalWorld::AddRefEntInPODGrid(IPhysicalEntity* _pent, const Vec3* BBox)
{
	CPhysicalEntity* pent = (CPhysicalEntity*)_pent;
	WriteLock lockPOD(m_lockPODGrid);
	int i, ix, iy, nCells = 0;
	Vec2i ig[2];
	const Vec3* pBBox = BBox ? BBox : pent->m_BBox;
	for (i = 0; i < 2; i++)
	{
		ig[i].x = max(-1, min(m_entgrid.size.x, physics_float2int(((pBBox[i][inc_mod3[m_iEntAxisz]] -
		                                                            m_entgrid.origin[inc_mod3[m_iEntAxisz]]) *
		                                                           m_entgrid.stepr.x) -
		                                                          0.5f)));
		ig[i].y = max(-1, min(m_entgrid.size.y, physics_float2int(((pBBox[i][dec_mod3[m_iEntAxisz]] -
		                                                            m_entgrid.origin[dec_mod3[m_iEntAxisz]]) *
		                                                           m_entgrid.stepr.y) -
		                                                          0.5f)));
	}
	for (ix = ig[0].x; ix <= ig[1].x; ix++)
	{
		for (iy = ig[0].y; iy <= ig[1].y; iy++)
		{
			if (getPODcell(ix, iy)->lifeTime > 0)
			{
				++pent->m_nRefCount, ++pent->m_nRefCountPOD, ++nCells;
			}
		}
	}
	return nCells;
}

void CPhysicalWorld::GetPODGridCellBBox(int ix, int iy, Vec3& center, Vec3& size)
{
	pe_PODcell* pPODcell = getPODcell(ix, iy);
	center = m_entgrid.origin;
	center[inc_mod3[m_iEntAxisz]] += (ix + 0.5f) * m_entgrid.step.x;
	center[dec_mod3[m_iEntAxisz]] += (iy + 0.5f) * m_entgrid.step.y;
	center[m_iEntAxisz] = (pPODcell->zlim[1] + pPODcell->zlim[0]) * 0.5f;
	size[inc_mod3[m_iEntAxisz]] = m_entgrid.step.x * 0.5f;
	size[dec_mod3[m_iEntAxisz]] = m_entgrid.step.x * 0.5f;
	size[m_iEntAxisz] = (pPODcell->zlim[1] - pPODcell->zlim[0]) * 0.5f;
}

pe_PODcell* CPhysicalWorld::getPODcell(int ix, int iy)
{
	int i, imask = -(inrange(ix, -1, m_entgrid.size.x) & inrange(iy, -1, m_entgrid.size.y));
	i = ((ix >> 3) * m_PODstride.x) + ((iy >> 3) * m_PODstride.y);
	i = (i + ((m_entgrid.size.x * m_entgrid.size.y >> 6) - i & ~imask)) & -m_bHasPODGrid;
	INT_PTR pmask = -iszero((INT_PTR)m_pPODCells[i]);
	pe_PODcell* pcell0 = (pe_PODcell*)(((INT_PTR)m_pPODCells[i] & ~pmask) + ((INT_PTR)m_pDummyPODcell & pmask));
	imask &= -m_bHasPODGrid & ~pmask;
	return pcell0 + ((ix & 7) + (iy & 7) * 8 & imask);
}

int CPhysicalWorld::SetSurfaceParameters(int surface_idx, float bounciness, float friction, unsigned int flags)
{
	if ((unsigned int)surface_idx >= (unsigned int)NSURFACETYPES)
	{
		return 0;
	}
	m_BouncinessTable[surface_idx] = bounciness;
	m_FrictionTable[surface_idx] = friction;
	m_DynFrictionTable[surface_idx] = friction * (1.0 / 1.5);
	m_SurfaceFlagsTable[surface_idx] = flags;
	return 1;
}

int CPhysicalWorld::GetSurfaceParameters(int surface_idx, float& bounciness, float& friction, unsigned int& flags)
{
	if ((unsigned int)surface_idx >= (unsigned int)NSURFACETYPES)
	{
		return 0;
	}
	bounciness = m_BouncinessTable[surface_idx];
	friction = m_FrictionTable[surface_idx];
	flags = m_SurfaceFlagsTable[surface_idx];
	return 1;
}

IPhysicalEntity* CPhysicalWorld::CreatePhysicalEntity(pe_type type, pe_params* params, void* pForeignData,
                                                      int iForeignData, int id)
{
	return CreatePhysicalEntity(type, 0.0f, params, pForeignData, iForeignData, id);
}

IPhysicalEntity* CPhysicalWorld::CreatePhysicalEntity(pe_type type, float lifeTime, pe_params* params,
                                                      void* pForeignData, int iForeignData, int id,
                                                      IPhysicalEntity* pHostPlaceholder)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_PHYSICS);

	CPhysicalEntity* res = 0;
	CPhysicalPlaceholder* pEntityHost = (CPhysicalPlaceholder*)pHostPlaceholder;

	switch (type)
	{
		case PE_STATIC:
			res = new CPhysicalEntity(this);
			break;
		case PE_RIGID:
			res = new CRigidEntity(this);
			break;
		case PE_LIVING:
			res = new CLivingEntity(this);
			break;
		case PE_WHEELEDVEHICLE:
			res = new CWheeledVehicleEntity(this);
			break;
		case PE_PARTICLE:
			res = new CParticleEntity(this);
			break;
		case PE_ARTICULATED:
			res = new CArticulatedEntity(this);
			break;
		case PE_ROPE:
			res = new CRopeEntity(this);
			break;
		case PE_SOFT:
			res = new CSoftEntity(this);
			break;
	}
	m_nTypeEnts[type]++;
	if (!res)
	{
		return 0;
	}

	if (type != PE_STATIC)
	{
		m_nDynamicEntitiesDeleted = 0;
	}
	if (pEntityHost && lifeTime > 0)
	{
		res->m_pForeignData = pEntityHost->m_pForeignData;
		res->m_iForeignData = pEntityHost->m_iForeignData;
		res->m_iForeignFlags = pEntityHost->m_iForeignFlags;
		res->m_id = pEntityHost->m_id;
		res->m_pEntBuddy = pEntityHost;
		pEntityHost->m_pEntBuddy = res;
		res->m_maxTimeIdle = lifeTime;
		res->m_bPermanent = 0;
		res->m_iGThunk0 = pEntityHost->m_iGThunk0;
		res->m_ig[0].x = pEntityHost->m_ig[0].x;
		res->m_ig[1].x = pEntityHost->m_ig[1].x;
		res->m_ig[0].y = pEntityHost->m_ig[0].y;
		res->m_ig[1].y = pEntityHost->m_ig[1].y;
	}
	else
	{
		res->m_bPermanent = 1;
		res->m_pForeignData = pForeignData;
		res->m_iForeignData = iForeignData;
		m_lastExtId = max(m_lastExtId, id);
		m_nExtIds += 1 + (id >> 31);
		SetPhysicalEntityId(res, id >= 0 ? id : GetFreeEntId());
	}
	res->m_flags |= 0x80000000u;
	if (params)
	{
		res->SetParams(params, iForeignData == 0x5AFE || IsPhysicsThread());
	}

	if (!m_lockStep && lifeTime == 0)
	{
		WriteLockCond lock1(m_lockCaller[1], !IsPodThread() && m_nEnts + 1 > m_nEntsAlloc - 1);
		WriteLock lock(m_lockStep);
		res->m_flags &= ~0x80000000u;
		RepositionEntity(res, 2);
		if (++m_nEnts > m_nEntsAlloc - 1)
		{
			m_nEntsAlloc += 4096;
			m_nEntListAllocs++;
			m_bEntityCountReserved = 0;
			ReallocateList(m_pTmpEntList, m_nEnts - 1, m_nEntsAlloc);
			ReallocateList(m_pTmpEntList1, m_nEnts - 1, m_nEntsAlloc);
			ReallocateList(m_pTmpEntList2, m_nEnts - 1, m_nEntsAlloc);
			ReallocateList(m_pGroupMass, m_nEnts - 1, m_nEntsAlloc);
			ReallocateList(m_pMassList, m_nEnts - 1, m_nEntsAlloc);
			ReallocateList(m_pGroupIds, m_nEnts - 1, m_nEntsAlloc);
			ReallocateList(m_pGroupNums, m_nEnts - 1, m_nEntsAlloc);
		}
	}
	else if (!m_lockQueue || !IsPhysicsThread())
	{
		WriteLock lock(m_lockQueue);
		AllocRequestsQueue((sizeof(int) * 3) + sizeof(void*));
		QueueData(4);                                        // RepositionEntity opcode
		QueueData((int)((sizeof(int) * 3) + sizeof(void*))); // size
		QueueData(res);
		QueueData(2); // flags
	}
	else
	{
		res->m_timeIdle = -10.0f;
		m_nOnDemandListFailures++;
	}

	return res;
}

IPhysicalEntity* CPhysicalWorld::CreatePhysicalPlaceholder(pe_type type, pe_params* params, void* pForeignData,
                                                           int iForeignData, int id)
{
	int i, j, iChunk;
	if (m_nPlaceholders * 10 < m_iLastPlaceholder * 7)
	{
		for (i = m_iLastPlaceholder >> 5; i >= 0 && m_pPlaceholderMap[i] == -1; i--)
			;
		if (i >= 0)
		{
			for (j = 0; j < 32 && m_pPlaceholderMap[i] & 1 << j; j++)
				;
			i = i << 5 | j;
		}
		i = (i - (i >> 31)) | ((m_iLastPlaceholder + 1) & i >> 31);
	}
	else
	{
		i = m_iLastPlaceholder + 1;
	}

	iChunk = i >> PLACEHOLDER_CHUNK_SZLG2;
	if (iChunk == m_nPlaceholderChunks)
	{
		m_nPlaceholderChunks++;
		ReallocateList(m_pPlaceholders, m_nPlaceholderChunks - 1, m_nPlaceholderChunks, true);
		ReallocateList(m_pPlaceholderMap, (m_iLastPlaceholder >> 5) + 1,
		               m_nPlaceholderChunks << (PLACEHOLDER_CHUNK_SZLG2 - 5), true);
	}
	if (!m_pPlaceholders[iChunk])
	{
		m_pPlaceholders[iChunk] = new CPhysicalPlaceholder[PLACEHOLDER_CHUNK_SZ];
	}
	CPhysicalPlaceholder* res = m_pPlaceholders[iChunk] + (i & PLACEHOLDER_CHUNK_SZ - 1);

	res->m_pForeignData = pForeignData;
	res->m_iForeignData = iForeignData;
	res->m_iForeignFlags = 0;
	res->m_iGThunk0 = 0;
	res->m_ig[0].x = res->m_ig[0].y = res->m_ig[1].x = res->m_ig[1].y = -2;
	res->m_pEntBuddy = 0;
	res->m_id = -1;
	res->m_bProcessed = 0;
	switch (type)
	{
		case PE_STATIC:
			res->m_iSimClass = 0;
			break;
		case PE_RIGID:
		case PE_WHEELEDVEHICLE:
			res->m_iSimClass = 1;
			break;
		case PE_LIVING:
			res->m_iSimClass = 3;
			break;
		case PE_PARTICLE:
		case PE_ROPE:
		case PE_ARTICULATED:
		case PE_SOFT:
			res->m_iSimClass = 4;
	}
	m_pPlaceholderMap[i >> 5] |= 1 << (i & 31);

	SetPhysicalEntityId(res, id >= 0 ? id : GetFreeEntId());
	if (params)
	{
		res->SetParams(params);
	}
	m_nPlaceholders++;
	m_iLastPlaceholder = max(m_iLastPlaceholder, i);

	return res;
}

int CPhysicalWorld::DestroyPhysicalEntity(IPhysicalEntity* _pent, int mode, int bThreadSafe)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_PHYSICS);

	int idx;
	CPhysicalPlaceholder* ppc = (CPhysicalPlaceholder*)_pent;
	if (ppc->m_pEntBuddy && IsPlaceholder(ppc->m_pEntBuddy) && mode != 0 ||
	    m_nDynamicEntitiesDeleted && ppc->m_iSimClass > 0)
	{
		return 0;
	}
	if (!(idx = IsPlaceholder(ppc)))
	{
		if (ppc->m_iSimClass != 5)
		{
			if (mode & 4 && ((CPhysicalEntity*)ppc)->Release() > 0)
			{
				return 0;
			}
			((CPhysicalEntity*)ppc)->m_iDeletionTime = 1;
			if (mode == 0)
			{
				((CPhysicalEntity*)ppc)->m_pForeignData = 0;
				((CPhysicalEntity*)ppc)->m_iForeignData = -1;
			}
		}
		else
		{
			((CPhysArea*)ppc)->m_bDeleted = 1;
		}
	}
	mode &= 3;

	// if (m_lockStep & (bThreadSafe^1))
	if (m_vars.bMultithreaded & (bThreadSafe ^ 1) && (m_lockStep || m_lockTPR || m_vars.lastTimeStep > 0))
	{
		WriteLock lock(m_lockQueue);
		AtomicAdd(&ppc->m_bProcessed, PENT_QUEUED);
		AllocRequestsQueue((sizeof(int) * 3) + sizeof(void*));
		QueueData(5);                                        // DestroyPhysicalEntity opcode
		QueueData((int)((sizeof(int) * 3) + sizeof(void*))); // size
		QueueData(_pent);
		QueueData(mode);
		return 1;
	}
	WriteLockCond lock(m_lockStep, !bThreadSafe && !IsPodThread());

	if (ppc->m_iSimClass == 5)
	{
		if (mode == 0)
		{
			RemoveArea(_pent);
		}
		return 1;
	}

	if (idx)
	{
		if (mode != 0)
		{
			return 0;
		}
		if (ppc->m_pEntBuddy)
		{
			DestroyPhysicalEntity(ppc->m_pEntBuddy, mode, 1);
		}
		SetPhysicalEntityId(ppc, -1);
		{
			WriteLock lockGrid(m_lockGrid);
			DetachEntityGridThunks(ppc);
		}
		--idx;
		m_pPlaceholderMap[idx >> 5] &= ~(1 << (idx & 31));
		m_nPlaceholders--;

		int i, j, iChunk = idx >> PLACEHOLDER_CHUNK_SZLG2;
		// if entire iChunk is empty, deallocate it
		for (i = j = 0; i < PLACEHOLDER_CHUNK_SZ >> 5; i++)
		{
			j |= m_pPlaceholderMap[(iChunk << (PLACEHOLDER_CHUNK_SZLG2 - 5)) + i];
		}
		if (!j)
		{
			delete[] m_pPlaceholders[iChunk];
			m_pPlaceholders[iChunk] = 0;
		}
		j = m_nPlaceholderChunks;
		// make sure that m_iLastPlaceholder points to the last used placeholder slot
		for (; m_iLastPlaceholder >= 0 &&
		       !(m_pPlaceholderMap[m_iLastPlaceholder >> 5] & 1 << (m_iLastPlaceholder & 31));
		     m_iLastPlaceholder--)
		{
			if (((m_iLastPlaceholder ^ m_iLastPlaceholder - 1) + 1) >> 1 == PLACEHOLDER_CHUNK_SZ)
			{
				// if m_iLastPlaceholder points to the 1st chunk element, entire chunk is free and can
				// be deallocated
				iChunk = m_iLastPlaceholder >> PLACEHOLDER_CHUNK_SZLG2;
				if (m_pPlaceholders[iChunk])
				{
					delete[] m_pPlaceholders[iChunk];
					m_pPlaceholders[iChunk] = 0;
				}
				m_nPlaceholderChunks = iChunk;
			}
		}
		if (m_nPlaceholderChunks < j)
		{
			ReallocateList(m_pPlaceholderMap, j << (PLACEHOLDER_CHUNK_SZLG2 - 5),
			               m_nPlaceholderChunks << (PLACEHOLDER_CHUNK_SZLG2 - 5), true);
		}

		return 1;
	}

	CPhysicalEntity* pent = (CPhysicalEntity*)_pent;
	if (pent->m_iSimClass == 7)
	{
		return 0;
	}
	pent->m_iDeletionTime = m_iLastLogPump + 2;
	for (idx = m_nProfiledEnts - 1; idx >= 0; idx--)
	{
		if (m_pEntProfileData[idx].pEntity == pent)
		{
			memmove(m_pEntProfileData + idx, m_pEntProfileData + idx + 1,
			        (--m_nProfiledEnts - idx) * sizeof(m_pEntProfileData[0]));
		}
	}
	m_prevGEAobjtypes = -1;

	if (mode == 2)
	{
		if (pent->m_iSimClass == -1 && pent->m_iPrevSimClass >= 0)
		{
			pent->m_ig[0].x = pent->m_ig[1].x = pent->m_ig[0].y = pent->m_ig[1].y = -2;
			pent->m_iSimClass = pent->m_iPrevSimClass & 0x0F;
			pent->m_iPrevSimClass = -1;
			AtomicAdd(&m_lockGrid, -RepositionEntity(pent));
		}
		pent->m_iDeletionTime = 0;
		return 1;
	}

	if (m_pEntBeingDeleted == pent)
	{
		return 1;
	}
	m_pEntBeingDeleted = pent;
	if (mode == 0 && !pent->m_bPermanent && m_pPhysicsStreamer)
	{
		m_pPhysicsStreamer->DestroyPhysicalEntity(pent);
	}
	m_pEntBeingDeleted = 0;

	pent->AlertNeighbourhoodND();
	if ((unsigned int)pent->m_iPrevSimClass < 8u && pent->m_iSimClass >= 0)
	{
		if (pent->m_next)
		{
			pent->m_next->m_prev = pent->m_prev;
		}
		(pent->m_prev ? pent->m_prev->m_next : m_pTypedEnts[pent->m_iPrevSimClass]) = pent->m_next;
		if (pent == m_pTypedEntsPerm[pent->m_iPrevSimClass])
		{
			m_pTypedEntsPerm[pent->m_iPrevSimClass] = pent->m_next;
		}
	}
	pent->m_next = pent->m_prev = 0;

	if (!pent->m_pEntBuddy)
	{
		WriteLock lockGrid(m_lockGrid);
		DetachEntityGridThunks(pent);
	}
	pent->m_iGThunk0 = 0;

	if (mode == 0)
	{
		int bWasRegistered = !(pent->m_flags & 0x80000000u);
		pent->m_iPrevSimClass = -1;
		pent->m_iSimClass = 7;
		pent->m_pForeignData = 0;
		pent->m_iForeignData = -1;
		pent->m_next = m_pTypedEnts[7];
		if (pent->m_next)
		{
			pent->m_next->m_prev = pent;
		}
		if (pent->m_pEntBuddy)
		{
			pent->m_pEntBuddy->m_pEntBuddy = 0;
		}
		else
		{
			if (pent->m_id <= m_lastExtId)
			{
				--m_nExtIds;
			}
			SetPhysicalEntityId(pent, -1);
		}
		m_pTypedEnts[7] = pent;
		if (bWasRegistered)
		{
			m_nTypeEnts[pent->GetType()]--;
		}
		if (bWasRegistered && --m_nEnts < m_nEntsAlloc - 8192 && !m_bEntityCountReserved)
		{
			m_nEntsAlloc -= 8192;
			m_nEntListAllocs++;
			ReallocateList(m_pTmpEntList, m_nEntsAlloc + 8192, m_nEntsAlloc);
			ReallocateList(m_pTmpEntList1, m_nEntsAlloc + 8192, m_nEntsAlloc);
			ReallocateList(m_pTmpEntList2, m_nEntsAlloc + 8192, m_nEntsAlloc);
			ReallocateList(m_pGroupMass, 0, m_nEntsAlloc);
			ReallocateList(m_pMassList, 0, m_nEntsAlloc);
			ReallocateList(m_pGroupIds, 0, m_nEntsAlloc);
			ReallocateList(m_pGroupNums, 0, m_nEntsAlloc);
		}
	}
	else if (pent->m_iSimClass >= 0)
	{
		pe_action_reset reset;
		pent->Action(&reset);
		pent->m_iPrevSimClass = pent->m_iSimClass | 0x100;
		pent->m_iSimClass = -1;
	}

	return 1;
}

int CPhysicalWorld::ReserveEntityCount(int nNewEnts)
{
	if (m_nEnts + nNewEnts > m_nEntsAlloc - 1)
	{
		m_nEntsAlloc = (m_nEnts + nNewEnts & ~4095) + 4096;
		m_nEntListAllocs++;
		m_bEntityCountReserved = 1;
		ReallocateList(m_pTmpEntList, m_nEnts - 1, m_nEntsAlloc);
		ReallocateList(m_pTmpEntList1, m_nEnts - 1, m_nEntsAlloc);
		ReallocateList(m_pTmpEntList2, m_nEnts - 1, m_nEntsAlloc);
		ReallocateList(m_pGroupMass, m_nEnts - 1, m_nEntsAlloc);
		ReallocateList(m_pMassList, m_nEnts - 1, m_nEntsAlloc);
		ReallocateList(m_pGroupIds, m_nEnts - 1, m_nEntsAlloc);
		ReallocateList(m_pGroupNums, m_nEnts - 1, m_nEntsAlloc);
	}
	return m_nEntsAlloc;
}

void CPhysicalWorld::CleanseEventsQueue()
{
	WriteLock lock(m_lockEventsQueue);
	EventPhys *pEvent, **ppPrevNext;

	for (pEvent = m_pEventFirst, ppPrevNext = &m_pEventFirst, m_pEventLast = 0; pEvent; pEvent = *ppPrevNext)
	{
		if (pEvent->idval <= EventPhysCollision::id &&
		        (((CPhysicalEntity*)((EventPhysStereo*)pEvent)->pEntity[0])->m_iDeletionTime ||
		         ((CPhysicalEntity*)((EventPhysStereo*)pEvent)->pEntity[1])->m_iDeletionTime) ||
		    pEvent->idval > EventPhysCollision::id &&
		        ((CPhysicalEntity*)((EventPhysMono*)pEvent)->pEntity)->m_iDeletionTime)
		{
			*ppPrevNext = pEvent->next;
			pEvent->next = m_pFreeEvents[pEvent->idval];
			m_pFreeEvents[pEvent->idval] = pEvent;
		}
		else
		{
			ppPrevNext = &pEvent->next;
			m_pEventLast = pEvent;
		}
	}
}

int CPhysicalWorld::GetFreeEntId()
{
	int nPhysEnts = m_nEnts - m_nExtIds, nPhysSlots = m_iNextId - m_lastExtId;
	if (nPhysEnts * 2 > nPhysSlots)
	{
		return m_iNextId++;
	}
	int nTries;
	for (nTries = 100; nTries > 0 && m_iNextIdDown > m_lastExtId && m_pEntsById[m_iNextIdDown];
	     m_iNextIdDown--, nTries--)
		;
	if (nTries <= 0)
	{
		return m_iNextId++;
	}
	if (m_iNextIdDown <= m_lastExtId)
	{
		for (m_iNextIdDown = m_iNextId - 2, nTries = 100;
		     m_iNextIdDown > m_lastExtId && m_pEntsById[m_iNextIdDown]; m_iNextIdDown--)
			;
	}
	if (nTries <= 0 || m_iNextIdDown <= m_lastExtId)
	{
		return m_iNextId++;
	}
	return m_iNextIdDown--;
}

int CPhysicalWorld::SetPhysicalEntityId(IPhysicalEntity* _pent, int id, int bReplace, int bThreadSafe)
{
	WriteLockCond lock(m_lockEntIdList, bThreadSafe ^ 1);
	CPhysicalPlaceholder* pent = (CPhysicalPlaceholder*)_pent;
	unsigned int previd = (unsigned int)pent->m_id;
	if (previd < (unsigned int)m_nIdsAlloc)
	{
		m_pEntsById[previd] = 0;
		if (previd == static_cast<unsigned int>(m_iNextId - 1))
		{
			for (; m_iNextId > 0 && m_pEntsById[m_iNextId - 1] == 0; m_iNextId--)
				;
		}
		if (previd == static_cast<unsigned int>(m_lastExtId))
		{
			for (--m_lastExtId; m_lastExtId > 0 && m_pEntsById[m_lastExtId]; m_lastExtId--)
				;
		}
	}
	m_iNextId = max(m_iNextId, id + 1);

	if (id >= 0)
	{
		if (id >= m_nIdsAlloc)
		{
			int nAllocPrev = m_nIdsAlloc;
			ReallocateList(m_pEntsById, nAllocPrev, m_nIdsAlloc = (id & ~255) + 256, true);
		}
		if (m_pEntsById[id])
		{
			if (bReplace)
			{
				SetPhysicalEntityId(m_pEntsById[id], GetFreeEntId(), 1, 1);
			}
			else
			{
				return 0;
			}
		}
		if (IsPlaceholder(pent->m_pEntBuddy))
		{
			pent = pent->m_pEntBuddy;
		}
		(m_pEntsById[id] = pent)->m_id = id;
		if (pent->m_pEntBuddy)
		{
			pent->m_pEntBuddy->m_id = id;
		}
		return 1;
	}
	return 0;
}

int CPhysicalWorld::GetPhysicalEntityId(IPhysicalEntity* pent)
{
	return pent ? ((CPhysicalEntity*)pent)->m_id : -1;
}

IPhysicalEntity* CPhysicalWorld::GetPhysicalEntityById(int id)
{
	ReadLock lock(m_lockEntIdList);
	int bNoExpand = id >> 30;
	id &= ~(1 << 30);
	if ((unsigned int)id < (unsigned int)m_nIdsAlloc)
	{
		return m_pEntsById[id] ? (!bNoExpand ? m_pEntsById[id]->GetEntity() : m_pEntsById[id]->GetEntityFast())
		                       : 0;
	}
	else if (id == -1)
	{
		return m_pHeightfield[0];
	}
	else if (id == -2)
	{
		return &g_StaticPhysicalEntity;
	}
	return 0;
}

int CPhysicalWorld::IsPlaceholder(CPhysicalPlaceholder* pent)
{
	if (!pent)
	{
		return 0;
	}

	int iChunk;
	for (iChunk = 0; iChunk < m_nPlaceholderChunks &&
	                 (unsigned int)(pent - m_pPlaceholders[iChunk]) >= (unsigned int)PLACEHOLDER_CHUNK_SZ;
	     iChunk++)
		;

	if (iChunk < m_nPlaceholderChunks)
	{
		return (iChunk << PLACEHOLDER_CHUNK_SZLG2 | pent - m_pPlaceholders[iChunk]) + 1;
	}

	return 0;
}

namespace {

void swap(CPhysicalEntity** pentlist, float* pmass, int* pids, int i1, int i2)
{
	CPhysicalEntity* pent = pentlist[i1];
	pentlist[i1] = pentlist[i2];
	pentlist[i2] = pent;
	float m = pmass[i1];
	pmass[i1] = pmass[i2];
	pmass[i2] = m;
	if (pids)
	{
		int id = pids[i1];
		pids[i1] = pids[i2];
		pids[i2] = id;
	}
}

void qsort(CPhysicalEntity** pentlist, float* pmass, int* pids, int ileft, int iright)
{
	if (ileft >= iright)
	{
		return;
	}
	int i, ilast;
	float diff = 0.0f;
	swap(pentlist, pmass, pids, ileft, (ileft + iright) >> 1);
	for (ilast = ileft, i = ileft + 1; i <= iright; i++)
	{
		diff += fabs_tpl(pmass[i] - pmass[ileft]);
		if (pmass[i] > pmass[ileft])
		{
			swap(pentlist, pmass, pids, ++ilast, i);
		}
	}
	swap(pentlist, pmass, pids, ileft, ilast);

	if (diff > 0)
	{
		qsort(pentlist, pmass, pids, ileft, ilast - 1);
		qsort(pentlist, pmass, pids, ilast + 1, iright);
	}
}

void swap(float* pval, int* pidx, int i1, int i2)
{
	float x = pval[i1];
	pval[i1] = pval[i2];
	pval[i2] = x;
	int i = pidx[i1];
	pidx[i1] = pidx[i2];
	pidx[i2] = i;
}

void qsort(float* pval, int* pidx, int ileft, int iright)
{
	if (ileft >= iright)
	{
		return;
	}
	int i, ilast;
	swap(pval, pidx, ileft, (ileft + iright) >> 1);
	for (ilast = ileft, i = ileft + 1; i <= iright; i++)
	{
		if (pval[i] < pval[ileft])
		{
			swap(pval, pidx, ++ilast, i);
		}
	}
	swap(pval, pidx, ileft, ilast);

	qsort(pval, pidx, ileft, ilast - 1);
	qsort(pval, pidx, ilast + 1, iright);
}

bool AABB_overlap2d(const Vec2& min0, const Vec2& max0, const Vec2& min1, const Vec2& max1)
{
	return max(fabs_tpl(min0.x + max0.x - min1.x - max1.x) - (max0.x - min0.x) - (max1.x - min1.x),
	           fabs_tpl(min0.y + max0.y - min1.y - max1.y) - (max0.y - min0.y) - (max1.y - min1.y)) < 0;
}

} // unnamed namespace

int CPhysicalWorld::GetEntitiesAround(const Vec3& ptmin, const Vec3& ptmax, CPhysicalEntity**& pList, int objtypes,
                                      CPhysicalEntity* pPetitioner, int szListPrealloc, int iCaller)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_PHYSICS);
	INT_PTR mask = (INT_PTR)pPetitioner;
	mask = mask >> ((sizeof(mask) * 8) - 1) ^ (mask - 1) >> ((sizeof(mask) * 8) - 1);

	if (!m_pEntGrid || !m_pTmpEntList)
	{
		return 0;
	}
	// WriteLock lock(m_lockCaller[iCaller]);
	CPhysicalEntity **pTmpEntList, **pTmpEntLists[2] = {m_pTmpEntList, m_pTmpEntList2};
	int i, j, igx[2], igy[2], ix, iy, nout = 0, itype, bSortRequired = 0, bContact, nGridEnts = 0, nEntsChecked = 0,
					  bProcessed, bAreasOnly;
	float zrange, gx[2], gy[2];
	Vec3 bbox[2];
	int ithunk, ithunk_next;
	pe_PODcell* pPODcell;
	EventPhysBBoxOverlap event;
	pTmpEntList = pTmpEntLists[iCaller];
	if (!szListPrealloc)
	{
		pList = 0;
	}
	if (pPetitioner)
	{
		itype = 1 << pPetitioner->m_iSimClass & -iszero((int)pPetitioner->m_flags & pef_never_affect_triggers);
		event.pEntity[0] = pPetitioner;
		event.pForeignData[0] = pPetitioner->m_pForeignData;
		event.iForeignData[0] = pPetitioner->m_iForeignData;
	}
	else
	{
		itype = 0;
	}
	bAreasOnly = iszero(objtypes - ent_areas);
	m_nGEA[iCaller]++;
	if ((ptmin - m_prevGEABBox[0]).len2() + (ptmax - m_prevGEABBox[1]).len2() + sqr(objtypes - m_prevGEAobjtypes) +
	        iCaller ==
	    0)
	{
		pList = m_pTmpEntList;
		return m_nprevGEAEnts;
	}

	bbox[0] = ptmin;
	bbox[1] = ptmax;
	for (i = 0; i < 2; i++)
	{
		gx[i] = (bbox[i][inc_mod3[m_iEntAxisz]] - m_entgrid.origin[inc_mod3[m_iEntAxisz]]) * m_entgrid.stepr.x;
		igx[i] = max(-1, min(m_entgrid.size.x, physics_float2int(gx[i] - 0.5f)));
		gy[i] = (bbox[i][dec_mod3[m_iEntAxisz]] - m_entgrid.origin[dec_mod3[m_iEntAxisz]]) * m_entgrid.stepr.y;
		igy[i] = max(-1, min(m_entgrid.size.y, physics_float2int(gy[i] - 0.5f)));
	}

	if ((igx[1] - igx[0] + 1) * (igy[1] - igy[0] + 1) > m_vars.nGEBMaxCells)
	{
		CryLog("GetEntitiesInBox: too many cells requested by %s (%d, (%.1f,%.1f,%.1f)-(%.1f,%.1f,%.1f))",
		       pPetitioner && m_pRenderer
		           ? m_pRenderer->GetForeignName(pPetitioner->m_pForeignData, pPetitioner->m_iForeignData,
		                                         pPetitioner->m_iForeignFlags)
		           : "Game",
		       (igx[1] - igx[0] + 1) * (igy[1] - igy[0] + 1), bbox[0].x, bbox[0].y, bbox[0].z, bbox[1].x,
		       bbox[1].y, bbox[1].z);
	}

	{
		ReadLock lock0(m_lockGrid);
		for (ix = igx[0]; ix <= igx[1]; ix++)
		{
			for (iy = igy[0]; iy <= igy[1]; iy++)
			{
				if ((objtypes & (ent_static | ent_no_ondemand_activation)) == ent_static)
				{
					pPODcell = getPODcell(ix, iy);
					zrange = pPODcell->zlim[1] - pPODcell->zlim[0];
					if (fabs_tpl((ptmax[m_iEntAxisz] + ptmin[m_iEntAxisz] - pPODcell->zlim[1] -
					              pPODcell->zlim[0]) *
					             zrange) <
					    (ptmax[m_iEntAxisz] - ptmin[m_iEntAxisz] + zrange) * zrange)
					{
						CryInterlockedAdd(&m_lockGrid, -1);
						ReadLock lockPOD(m_lockPODGrid);
						if (pPODcell->lifeTime <= 0)
						{
							CryInterlockedAdd(&m_lockPODGrid, -1);
							{
								WriteLock lockPODw(m_lockPODGrid);
								if (pPODcell->lifeTime <= 0)
								{
									SetPodThreadId();
									Vec3 center, size;
									GetPODGridCellBBox(ix, iy, center, size);
									m_nOnDemandListFailures = 0;
									++m_iLastPODUpdate;
									if (m_pPhysicsStreamer
									        ->CreatePhysicalEntitiesInBox(
										    center - size, center + size))
									{
										pPODcell->lifeTime =
										    m_nOnDemandListFailures ? 1E10f
													    : 8.0f;
										pPODcell->inextActive =
										    m_iActivePODCell0;
										m_iActivePODCell0 = iy << 16 | ix;
									}
								}
							}
							ReadLockCond lockPODr(m_lockPODGrid, 1);
							lockPODr.SetActive(0);
							m_nOnDemandListFailures = 0;
						}
						else
						{
							pPODcell->lifeTime = max(8.0f, pPODcell->lifeTime);
						}
						UnsetPodThreadId();
						ReadLockCond relock(m_lockGrid, 1);
						relock.SetActive(0);
					}
				}

				for (ithunk = m_pEntGrid[m_entgrid.getcell_safe(ix, iy)];
				     ithunk && ((objtypes >> m_gthunks[ithunk].iSimClass) & 1) | bAreasOnly ^ 1;
				     ithunk = ithunk_next, nGridEnts++)
				{
					ithunk_next = m_gthunks[ithunk].inext;
					if ((objtypes >> m_gthunks[ithunk].iSimClass) & 1 &&
					    (!m_entgrid.inrange(ix, iy) ||
					     AABB_overlap2d(
						 Vec2(gx[0], gy[0]), Vec2(gx[1], gy[1]),
						 Vec2(ix + (m_gthunks[ithunk].BBox[0] * (1.0f / 256)),
					              iy + (m_gthunks[ithunk].BBox[1] * (1.0f / 256))),
						 Vec2(ix + ((m_gthunks[ithunk].BBox[2] + 1) * (1.0f / 256)),
					              iy + ((m_gthunks[ithunk].BBox[3] + 1) * (1.0f / 256))))) &&
					    !((m_gthunks[ithunk].pent->m_bProcessed >> iCaller & 1) |
					      -((CPhysicalEntity*)m_gthunks[ithunk].pent)->m_iDeletionTime >> 31))
					{
						CPhysicalPlaceholder* pGridEnt = m_gthunks[ithunk].pent;
						{
							ReadLock lock1(pGridEnt->m_lockUpdate);
							bContact = AABB_overlap(bbox, pGridEnt->m_BBox);
						}

						if (bContact)
						{
							if ((unsigned int)(pGridEnt->m_iSimClass - 5) > 1u)
							{
								m_bGridThunksChanged = 0;
								CPhysicalEntity* pent = pGridEnt->GetEntity();
								if (m_bGridThunksChanged)
								{
									ithunk_next =
									    m_pEntGrid[m_entgrid.getcell_safe(ix, iy)];
								}
								m_bGridThunksChanged = 0;
								if (objtypes & ent_ignore_noncolliding)
								{
									for (i = 0; i < pent->m_nParts &&
									            !(pent->m_parts[i].flags &
									              geom_colltype_solid);
									     i++)
										;
									if (i == pent->m_nParts)
									{
										continue;
									}
								}
								pTmpEntList[nout] = pent;
								bProcessed = iszero(m_bUpdateOnlyFlagged &
								                    ((int)pent->m_flags ^ pef_update)) |
								             iszero(pent->m_iSimClass);
								nout += bProcessed;
								AtomicAdd(&pGridEnt->m_bProcessed,
								          bProcessed << iCaller);
								bSortRequired += pent->m_pOuterEntity != 0;
							}
							else if (pGridEnt->m_iSimClass == 5)
							{
								if (!((CPhysArea*)pGridEnt)->m_bDeleted)
								{
									pTmpEntList[nout++] =
									    (CPhysicalEntity*)pGridEnt;
									AtomicAdd(&pGridEnt->m_bProcessed,
									          1 << iCaller);
								}
							}
							else if (pGridEnt->m_iForeignFlags & itype)
							{
								event.pEntity[1] = pGridEnt;
								event.pForeignData[1] = pGridEnt->m_pForeignData;
								event.iForeignData[1] = pGridEnt->m_iForeignData;
								OnEvent(pPetitioner->m_flags, &event);
								// m_pEventClient->OnBBoxOverlap(pGridEnt,pGridEnt->m_pForeignData,pGridEnt->m_iForeignData,
								//	pPetitioner,pPetitioner->m_pForeignData,pPetitioner->m_iForeignData);
							}
							if (nout >= m_nEnts)
							{
								goto listfull;
							}
						}
						nEntsChecked++;
					}
				}
			}
		}
	listfull:;
	}
	for (i = 0; i < nout; i++)
	{
		AtomicAdd(&(pTmpEntList[i]->m_pEntBuddy ? pTmpEntList[i]->m_pEntBuddy : pTmpEntList[i])->m_bProcessed,
		          -(1 << iCaller));
	}

	if (bSortRequired)
	{
		CPhysicalEntity *pent, *pents, *pstart;
		for (i = 0; i < nout; i++)
		{
			pTmpEntList[i]->m_bProcessed_aux = 1;
		}
		for (i = 0, pent = 0; i < nout - 1; i++)
		{
			pTmpEntList[i]->m_prev_aux = pent;
			pTmpEntList[i]->m_next_aux = pTmpEntList[i + 1];
			pent = pTmpEntList[i];
		}
		pstart = pTmpEntList[0];
		pTmpEntList[nout - 1]->m_prev_aux = pent;
		pTmpEntList[nout - 1]->m_next_aux = 0;
		for (i = 0; i < nout; i++)
		{
			if ((pent = pTmpEntList[i])->m_pOuterEntity && pent->m_pOuterEntity->m_bProcessed_aux > 0)
			{
				// if entity has an outer entity, move it together with its children right before this
				// outer entity
				for (pents = pent, j = pent->m_bProcessed_aux - 1; j > 0; pents = pents->m_prev_aux)
					; // count back the number of pent children
				(pents->m_prev_aux ? pent->m_prev_aux->m_next_aux : pstart) =
				    pent->m_next_aux; // cut pents-pent stripe from list ...
				if (pent->m_next_aux)
				{
					pent->m_next_aux->m_prev_aux = pents->m_prev_aux;
				}
				pent->m_next_aux = pent->m_pOuterEntity; // ... and insert if before pent
				pents->m_prev_aux = pent->m_pOuterEntity->m_prev_aux;
				(pent->m_pOuterEntity->m_prev_aux ? pent->m_pOuterEntity->m_prev_aux->m_next_aux
				                                  : pstart) = pents;
				pent->m_pOuterEntity->m_prev_aux = pent;
				pent->m_pOuterEntity->m_bProcessed_aux += pent->m_bProcessed_aux;
			}
		}
		Vec3 ptc = (ptmin + ptmax) * 0.5f;
		for (i = 0; i < nout; i++)
		{
			pTmpEntList[i]->m_bProcessed_aux = 0;
		}
		for (pent = pstart, nout = 0; pent; pent = pent->m_next_aux)
		{
			if (!pent->m_bProcessed_aux)
			{
				pTmpEntList[nout] = pent;
				if (pent->m_pOuterEntity && pent->IsPointInside(ptc))
				{
					for (pent = pent->m_pOuterEntity; pent; pent = pent->m_pOuterEntity)
					{
						pent->m_bProcessed_aux = -1;
					}
				}
				pent = pTmpEntList[nout++];
			}
		}
	}

	if (m_pHeightfield[iCaller] && objtypes & ent_terrain)
	{
		pTmpEntList[nout++] = m_pHeightfield[iCaller];
	}

	if (objtypes & ent_sort_by_mass)
	{
		for (i = 0; i < nout; i++)
		{
			m_pMassList[i] = pTmpEntList[i]->GetMassInv();
		}
		// manually put all static (0-massinv) object to the end of the list, since qsort doesn't
		// perform very well on lists of same numbers
		int ilast;
		for (i = ilast = nout - 1; i > 0; i--)
		{
			if (m_pMassList[i] == 0)
			{
				if (i != ilast)
				{
					swap(pTmpEntList, m_pMassList, 0, i, ilast);
				}
				--ilast;
			}
		}
		qsort(pTmpEntList, m_pMassList, 0, 0, ilast);
	}

	if (objtypes & 1 << 5)
	{
		ReadLock lock1(m_lockAreas);
		if (m_pGlobalArea)
		{
			for (CPhysArea* pArea = m_pGlobalArea->m_nextBig; pArea; pArea = pArea->m_nextBig)
			{
				if (!pArea->m_bDeleted && AABB_overlap(bbox, pArea->m_BBox) && nout < m_nEnts)
				{
					pTmpEntList[nout++] = (CPhysicalEntity*)pArea;
				}
			}
		}
	}

	if (szListPrealloc < nout)
	{
		if (!(objtypes & ent_allocate_list))
		{
			pList = pTmpEntList;
		}
		else if (nout > 0)
		{ //  don't allocate 0-elements arrays
			pList = new CPhysicalEntity*[nout];
			for (i = 0; i < nout; i++)
			{
				pList[i] = pTmpEntList[i];
			}
		}
	}
	else
	{
		for (i = 0; i < nout; i++)
		{
			pList[i] = pTmpEntList[i];
		}
	}

	if (iCaller == 0)
	{
		m_prevGEABBox[0] = ptmin;
		m_prevGEABBox[1] = ptmax;
		m_prevGEAobjtypes = objtypes;
		m_nprevGEAEnts = nout;
	}
	return nout;
}

void CPhysicalWorld::ScheduleForStep(CPhysicalEntity* pent)
{
	if (!(pent->m_flags & pef_step_requested))
	{
		pent->m_flags |= pef_step_requested;
		pent->m_next_coll2 = m_pAuxStepEnt;
		pent->m_timeIdle = m_groupTimeStep;
		m_pAuxStepEnt = pent;
	}
}

int CPhysicalWorld::ResolveGroupContacts(int i, int nAnimatedObjects)
{
	int j, bGroupFinished = 1, nEnts, n;
	float Ebefore = 0.0f, Eafter = 0.0f, damping;
	CPhysicalEntity* pent;

	if (m_groupTimeStep > 0)
	{
		InitContactSolver(m_groupTimeStep);

		if (m_vars.nMaxPlaneContactsDistress != m_vars.nMaxPlaneContacts)
		{
			for (pent = m_pTmpEntList1[i], j = nEnts = 0; pent; pent = pent->m_next_coll, nEnts++)
			{
				j += pent->GetContactCount(m_vars.nMaxPlaneContacts);
				Ebefore += pent->CalcEnergy(m_groupTimeStep);
			}
			n = j > m_vars.nMaxContacts ? m_vars.nMaxPlaneContactsDistress : m_vars.nMaxPlaneContacts;
			for (pent = m_pTmpEntList1[i]; pent; pent = pent->m_next_coll)
			{
				pent->RegisterContacts(m_groupTimeStep, n);
			}
		}
		else
		{
			for (pent = m_pTmpEntList1[i], nEnts = 0; pent; pent = pent->m_next_coll, nEnts++)
			{
				pent->RegisterContacts(m_groupTimeStep, m_vars.nMaxPlaneContacts);
				Ebefore += pent->CalcEnergy(m_groupTimeStep);
			}
		}

		Ebefore = max(m_pGroupMass[i] * sqr(0.005f), Ebefore);

		InvokeContactSolver(m_groupTimeStep, &m_vars, Ebefore);

		// if (nAnimatedObjects==0)
		damping = 1.0f - (m_groupTimeStep * m_vars.groupDamping *
		                  isneg(m_vars.nGroupDamping - 1 - max(nEnts, g_nBodies)));
		for (pent = m_pTmpEntList1[i], bGroupFinished = 1; pent; pent = pent->m_next_coll)
		{
			Eafter += pent->CalcEnergy(0);
			if (!(pent->m_flags & pef_fixed_damping))
			{
				damping = min(damping, pent->GetDamping(m_groupTimeStep));
			}
			else
			{
				damping = pent->GetDamping(m_groupTimeStep);
				break;
			}
		}
		Ebefore *=
		    isneg(-nAnimatedObjects) + 1; // increase energy growth limit if we have animated bodies involved
		if (Eafter > Ebefore * (1.0f + 0.1f * isneg(g_nBodies - 15)))
		{
			damping = min(damping, sqrt_tpl(Ebefore / Eafter));
		}
		for (pent = m_pTmpEntList1[i], bGroupFinished = 1; pent; pent = pent->m_next_coll)
		{
			bGroupFinished &= pent->Update(m_groupTimeStep, damping);
		}
		for (pent = m_pTmpEntList1[i]; pent; pent = pent->m_next_coll)
		{
			pent->m_bMoved = bGroupFinished;
		}
	}

	return bGroupFinished;
}

void CPhysicalWorld::UpdateDeformingEntities(float time_interval)
{
	WriteLock lock3(m_lockDeformingEntsList);
	int i, j;
	if (time_interval >= 0)
	{
		for (i = j = 0; i < m_nDeformingEnts; i++)
		{
			if (m_pDeformingEnts[i]->m_iSimClass != 7 &&
			    m_pDeformingEnts[i]->UpdateStructure(time_interval, 0))
			{
				m_pDeformingEnts[j++] = m_pDeformingEnts[i];
			}
			else
			{
				m_pDeformingEnts[i]->m_flags &= ~pef_deforming;
			}
		}
		m_nDeformingEnts = j;
	}
	else
	{
		for (i = 0; i < m_nDeformingEnts; i++)
		{
			m_pDeformingEnts[i]->m_flags &= ~pef_deforming;
		}
		m_nDeformingEnts = 0;
	}
}

void CPhysicalWorld::TimeStep(float time_interval, int flags)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_PHYSICS);

	float m, m_groupTimeStep{}, time_interval_org = time_interval, Ebefore, Eafter, damping, fixedDamping,
				    minGroupTimeStep;
	CPhysicalEntity *pent, *phead, *ptail, **pentlist, *pent_next, *pent1, *pentmax;
	int i, i1, j, n, iter, ipass, nGroups, bHeadAdded, bGroupFinished, bAllGroupsFinished, bStepValid,
	    nAnimatedObjects, nEnts, bSkipFlagged, nParts0, nBrokenParts;

	if (time_interval < 0)
	{
		return;
	}

	SetPhysicsThreadId();

	{
		WriteLock lock1(m_lockCaller[1]), lock2(m_lockQueue), lock3(m_lockStep);
		phys_geometry* pgeom;
		for (i = 0; i < m_nQueueSlots; i++)
		{
			for (j = 0; *(int*)(m_pQueueSlots[i] + j) != -1;
			     j += *(int*)(m_pQueueSlots[i] + j + sizeof(int)))
			{
				pent = *(CPhysicalEntity**)(m_pQueueSlots[i] + j + (sizeof(int) * 2));
				if (!(pent->m_iSimClass == 7 || pent->m_iSimClass == 5 && pent->m_iDeletionTime == 2))
				{
					switch (*(int*)(m_pQueueSlots[i] + j))
					{
						case 0:
							pent->SetParams((pe_params*)(m_pQueueSlots[i] + j +
							                             (sizeof(int) * 2) + sizeof(void*)),
							                1);
							break;
						case 1:
							pent->Action((pe_action*)(m_pQueueSlots[i] + j +
							                          (sizeof(int) * 2) + sizeof(void*)),
							             1);
							break;
						case 2:
							pent->AddGeometry(
							    pgeom =
								*(phys_geometry**)(m_pQueueSlots[i] + j +
							                           (sizeof(int) * 2) + sizeof(void*)),
							    (pe_geomparams*)(m_pQueueSlots[i] + j + (sizeof(int) * 3) +
							                     (sizeof(void*) * 2)),
							    *(int*)(m_pQueueSlots[i] + j + (sizeof(int) * 2) +
							            (sizeof(void*) * 2)),
							    1);
							AtomicAdd(&pgeom->nRefCount, -1);
							break;
						case 3:
							pent->RemoveGeometry(*(int*)(m_pQueueSlots[i] + j +
							                             (sizeof(int) * 2) +
							                             (sizeof(void*) * 2)),
							                     1);
							break;
						case 4:
							pent->m_flags &= ~0x80000000u;
							AtomicAdd(&m_lockGrid,
							          -RepositionEntity(pent,
							                            *(int*)(m_pQueueSlots[i] + j +
							                                    (sizeof(int) * 2) +
							                                    sizeof(void*)),
							                            0, 1));
							if (++m_nEnts > m_nEntsAlloc - 1)
							{
								m_nEntsAlloc += 4096;
								m_nEntListAllocs++;
								m_bEntityCountReserved = 0;
								ReallocateList(m_pTmpEntList, m_nEnts - 1,
								               m_nEntsAlloc);
								ReallocateList(m_pTmpEntList1, m_nEnts - 1,
								               m_nEntsAlloc);
								ReallocateList(m_pTmpEntList2, m_nEnts - 1,
								               m_nEntsAlloc);
								ReallocateList(m_pGroupMass, m_nEnts - 1, m_nEntsAlloc);
								ReallocateList(m_pMassList, m_nEnts - 1, m_nEntsAlloc);
								ReallocateList(m_pGroupIds, m_nEnts - 1, m_nEntsAlloc);
								ReallocateList(m_pGroupNums, m_nEnts - 1, m_nEntsAlloc);
							}
							break;
						case 5:
							DestroyPhysicalEntity((IPhysicalEntity*)pent,
							                      *(int*)(m_pQueueSlots[i] + j +
							                              (sizeof(int) * 2) +
							                              sizeof(void*)),
							                      1);
					}
				}
				if (*(int*)(m_pQueueSlots[i] + j) != 4)
				{
					AtomicAdd(&pent->m_bProcessed, -PENT_QUEUED);
				}
			}
		}
		if (m_nQueueSlots)
		{
			m_nQueueSlots = 1;
			m_nQueueSlotSize = 0;
			*(int*)m_pQueueSlots[0] = -1;
		}
	}
	WriteLock lock(m_lockStep);

	if (time_interval > m_vars.maxWorldStep)
	{
		time_interval = time_interval_org = m_vars.maxWorldStep;
	}

	if (m_vars.timeGranularity > 0)
	{
		i = physics_float2int(time_interval_org * (m_vars.rtimeGranularity = 1.0f / m_vars.timeGranularity));
		time_interval_org = time_interval = i * m_vars.timeGranularity;
		m_iTimePhysics += i;
		m_timePhysics = m_iTimePhysics * m_vars.timeGranularity;
	}
	else
	{
		m_timePhysics += time_interval;
	}
	if (m_vars.fixedTimestep > 0 && time_interval > 0)
	{
		time_interval = m_vars.fixedTimestep;
	}
	m_bUpdateOnlyFlagged = flags & ent_flagged_only;
	bSkipFlagged = flags >> 1 & pef_update;
	m_bWorldStep = 1;
	m_vars.bMultiplayer = gEnv->bMultiplayer;
	m_vars.bUseDistanceContacts &= m_vars.bMultiplayer ^ 1;
	m_vars.lastTimeStep = m_lastTimeInterval = time_interval;
	if (m_pGlobalArea && !is_unused(m_pGlobalArea->m_gravity))
	{
		m_pGlobalArea->m_gravity = m_vars.gravity;
	}

	if (flags & ent_living)
	{
		for (pent = m_pTypedEnts[3]; pent; pent = pent->m_next)
		{
			if (!((m_bUpdateOnlyFlagged & (pent->m_flags ^ pef_update)) | (bSkipFlagged & pent->m_flags)))
			{
				pent->StartStep(time_interval_org *
				                m_vars.timeScalePlayers); // prepare to advance living entities
			}
		}
	}

	if (!m_vars.bSingleStepMode || m_vars.bDoStep)
	{
		iter = 0;
		if (flags & ent_independent)
		{
			for (pent = m_pTypedEnts[4]; pent; pent = pent->m_next)
			{
				if (!((m_bUpdateOnlyFlagged & (pent->m_flags ^ pef_update)) |
				      (bSkipFlagged & pent->m_flags)))
				{
					pent->StartStep(time_interval);
				}
			}
		}

		if (flags & ent_rigid)
		{
			if (m_pTypedEnts[2])
			{
				do
				{ // make as many substeps as required
					bAllGroupsFinished = 1;
					m_pAuxStepEnt = 0;
					m_pGroupNums[m_nEntsAlloc - 1] =
					    -1; // special group for rigid bodies w/ infinite mass
					m_iSubstep++;

					for (ipass = 0; ipass < 2; ipass++)
					{
						// build lists of intercolliding groups of entities
						for (pent = m_pTypedEnts[2], nGroups = 0; pent; pent = pent_next)
						{
							pent_next = pent->m_next;
							if (!(pent->m_bMoved |
							      (m_bUpdateOnlyFlagged & (pent->m_flags ^ pef_update)) |
							      (bSkipFlagged & pent->m_flags)))
							{
								if (pent->GetMassInv() <= 0)
								{
									if ((iter | ipass) == 0)
									{ // just make isolated step for rigids with
									  // infinite mass
										pent->StartStep(time_interval);
										pent->m_iGroup = m_nEntsAlloc - 1;
									}
									if (ipass == 0)
									{
										pent->Step(m_groupTimeStep =
										               pent->GetMaxTimeStep(
												   time_interval));
										bAllGroupsFinished &=
										    pent->Update(time_interval, 1);
									}
								}
								else
								{
									pent->m_iGroup = nGroups;
									pent->m_bMoved = 1;
									m_pGroupIds[nGroups] = 0;
									m_pGroupMass[nGroups] =
									    1.0f / pent->GetMassInv();
									if ((iter | ipass) == 0)
									{
										pent->StartStep(time_interval);
									}
									pent->m_next_coll1 = pent->m_next_coll = 0;
									m_pTmpEntList1[nGroups] = 0;
									// initially m_pTmpEntList1 points to group
									// entities that collide with statics (sorted by
									// mass) - linked via m_next_coll m_next_coll1
									// maintains a queue of current intercolliding
									// objects

									for (phead = ptail = pentmax = pent; phead;
									     phead = phead->m_next_coll1)
									{
										for (i = bHeadAdded = 0,
										    n = phead->GetColliders(pentlist);
										     i < n; i++)
										{
											if (pentlist[i]->GetMassInv() <=
											    0)
											{
												if (!bHeadAdded)
												{
													for (
													    pent1 = m_pTmpEntList1
														[nGroups];
													    pent1 &&
													    pent1
														->m_next_coll &&
													    pent1->m_next_coll
														    ->GetMassInv() <=
														phead
														    ->GetMassInv();
													    pent1 =
														pent1
														    ->m_next_coll)
														;
													if (!pent1 ||
													    pent1->GetMassInv() >
													        phead
													            ->GetMassInv())
													{
														phead
														    ->m_next_coll =
														    pent1;
														m_pTmpEntList1
														    [nGroups] =
															phead;
													}
													else
													{
														phead
														    ->m_next_coll =
														    pent1
															->m_next_coll;
														pent1
														    ->m_next_coll =
														    phead;
													}
													bHeadAdded = 1;
												}
												m_pGroupIds[nGroups] =
												    1; // tells that
												       // group has
												       // static
												       // entities
											}
											else if (
											    !(pentlist[i]->m_bMoved |
											      (m_bUpdateOnlyFlagged &
											       (pentlist[i]->m_flags ^
											        pef_update))))
											{
												pentlist[i]->m_flags &=
												    ~bSkipFlagged;
												ptail->m_next_coll1 =
												    pentlist[i];
												ptail = pentlist[i];
												ptail->m_next_coll1 = 0;
												ptail->m_next_coll = 0;
												ptail->m_iGroup =
												    nGroups;
												ptail->m_bMoved = 1;
												if ((iter | ipass) == 0)
												{
													ptail->StartStep(
													    time_interval);
												}
												m_pGroupMass[nGroups] +=
												    1.0f /
												    (m = ptail
												             ->GetMassInv());
												if (pentmax
												        ->GetMassInv() >
												    m)
												{
													pentmax = ptail;
												}
											}
										}
									}
									if (!m_pTmpEntList1[nGroups])
									{
										m_pTmpEntList1[nGroups] = pentmax;
									}
									nGroups++;
								}
							}
						}

						// add maximum group mass to all groups that contain static entities
						for (i = 1, m = m_pGroupMass[0]; i < nGroups; i++)
						{
							m = max(m, m_pGroupMass[i]);
						}
						for (m *= 1.01f, i = 0; i < nGroups; i++)
						{
							m_pGroupMass[i] += m * m_pGroupIds[i];
						}
						for (i = 0; i < nGroups; i++)
						{
							m_pGroupIds[i] = i;
						}

						// sort groups by decsending group mass
						qsort(m_pTmpEntList1, m_pGroupMass, m_pGroupIds, 0, nGroups - 1);
						for (i = 0; i < nGroups; i++)
						{
							m_pGroupNums[m_pGroupIds[i]] = i;
						}
						minGroupTimeStep = time_interval;

						for (i = 0; i < nGroups; i++)
						{
							m_iCurGroup = m_pGroupIds[i];
							m_curGroupMass =
							    m_pGroupMass[i] - (m * isneg(m - m_pGroupMass[i]));
							m_groupTimeStep = time_interval * (ipass ^ 1);
							nAnimatedObjects = 0;
							for (ptail = m_pTmpEntList1[i]; ptail->m_next_coll;
							     ptail = ptail->m_next_coll)
							{
								ptail->m_bMoved = 0;
							}
							ptail->m_bMoved = 0;
							m_bCurGroupInvisible =
							    m_pTmpEntList1[i]->m_flags & pef_invisible;
							fixedDamping = -1.0f;
							for (phead = m_pTmpEntList1[i]; phead;
							     phead = phead->m_next_coll)
							{
								for (j = 0, n = phead->GetColliders(pentlist); j < n;
								     j++)
								{
									if (pentlist[j]->GetMassInv() > 0)
									{
										if (!((pentlist[j]->m_bMoved ^ 1) |
										      (m_bUpdateOnlyFlagged &
										       (pentlist[j]->m_flags ^
										        pef_update))))
										{
											ptail->m_next_coll =
											    pentlist[j];
											ptail = pentlist[j];
											ptail->m_next_coll = 0;
											ptail->m_bMoved = 0;
										}
										m_bCurGroupInvisible &=
										    pentlist[j]->m_flags;
									}
									else if (pentlist[j]->m_iSimClass > 1)
									{
										if (ipass)
										{
											if (pentlist[j]->m_flags &
											    pef_fixed_damping)
											{
												fixedDamping = max(
												    fixedDamping,
												    pentlist[j]->GetDamping(
													pentlist[j]->GetMaxTimeStep(
													    time_interval)));
											}
											m_groupTimeStep =
											    max(m_groupTimeStep,
											        pentlist[j]
											            ->GetLastTimeStep(
													time_interval));
										}
										else
										{
											m_groupTimeStep = min(
											    m_groupTimeStep,
											    pentlist[j]->GetMaxTimeStep(
												time_interval));
										}
										nAnimatedObjects++;
										m_bCurGroupInvisible &=
										    pentlist[j]->m_flags |
										    ~-iszero(pentlist[j]->m_iSimClass -
										             2);
									}
								}
							}
							m_bCurGroupInvisible = -(-m_bCurGroupInvisible >> 31);

							if (ipass == 0)
							{
								for (pent = m_pTmpEntList1[i]; pent;
								     pent = pent->m_next_coll)
								{
									m_groupTimeStep =
									    min(m_groupTimeStep,
									        pent->GetMaxTimeStep(time_interval));
								}
								for (pent = m_pTmpEntList1[i], bStepValid = 1,
								    phead = 0;
								     pent; pent = pent_next)
								{
									pent_next = pent->m_next_coll;
									if (pent->m_iSimClass < 3)
									{
										bStepValid &=
										    (phead = pent)
											->Step(m_groupTimeStep);
									}
									pent->m_bMoved = 1;
								}
								if (!bStepValid)
								{
									for (pent = m_pTmpEntList1[i]; pent;
									     pent = pent->m_next_coll)
									{
										pent->StepBack(m_groupTimeStep);
									}
									// for(pent=m_pAuxStepEnt1; pent;
									// pent=pent->m_next_coll) 	pent->m_flags &=
									//~pef_step_requested;
								}
								for (pent = m_pTmpEntList1[i]; pent;
								     pent = pent->m_next_coll)
								{
									pent->m_bMoved = 0;
								}
							}
							else if (time_interval > 0)
							{
								for (pent = m_pTmpEntList1[i], m_groupTimeStep = 0;
								     pent; pent = pent->m_next_coll)
								{
									if (pent->m_iSimClass > 1)
									{
										m_groupTimeStep =
										    max(m_groupTimeStep,
										        pent->GetLastTimeStep(
											    time_interval));
									}
								}
								if (m_groupTimeStep == 0)
								{
									m_groupTimeStep = time_interval;
								}
								InitContactSolver(m_groupTimeStep);
								Ebefore = Eafter = 0.0f;

								if (m_vars.nMaxPlaneContactsDistress !=
								    m_vars.nMaxPlaneContacts)
								{
									for (pent = m_pTmpEntList1[i], j = nEnts = 0;
									     pent; pent = pent->m_next_coll, nEnts++)
									{
										j += pent->GetContactCount(
										    m_vars.nMaxPlaneContacts);
										Ebefore +=
										    pent->CalcEnergy(m_groupTimeStep);
									}
									n = j > m_vars.nMaxContacts
									        ? m_vars.nMaxPlaneContactsDistress
									        : m_vars.nMaxPlaneContacts;
									for (pent = m_pTmpEntList1[i]; pent;
									     pent = pent->m_next_coll)
									{
										pent->RegisterContacts(m_groupTimeStep,
										                       n);
									}
								}
								else
								{
									for (pent = m_pTmpEntList1[i], nEnts = 0; pent;
									     pent = pent->m_next_coll, nEnts++)
									{
										pent->RegisterContacts(
										    m_groupTimeStep,
										    m_vars.nMaxPlaneContacts);
										Ebefore +=
										    pent->CalcEnergy(m_groupTimeStep);
									}
								}

								Ebefore = max(m_pGroupMass[i] * sqr(0.005f), Ebefore);

								InvokeContactSolver(m_groupTimeStep, &m_vars, Ebefore);
								for (j = 0; j < g_nContacts; j++)
								{
									if ((g_pContacts[j]->ipart[0] |
									     g_pContacts[j]->ipart[1]) >= 0)
									{
										if (g_pContacts[j]
										        ->pent[0]
										        ->m_parts[g_pContacts[j]
										                      ->ipart[0]]
										        .flags &
										    geom_monitor_contacts)
										{
											g_pContacts[j]
											    ->pent[0]
											    ->OnContactResolved(
												g_pContacts[j], 0,
												m_iCurGroup);
										}
										if (g_pContacts[j]
										        ->pent[1]
										        ->m_parts[g_pContacts[j]
										                      ->ipart[1]]
										        .flags &
										    geom_monitor_contacts)
										{
											g_pContacts[j]
											    ->pent[1]
											    ->OnContactResolved(
												g_pContacts[j], 1,
												m_iCurGroup);
										}
									}
								}

								// if (nAnimatedObjects==0)
								damping =
								    1.0f - (m_groupTimeStep * m_vars.groupDamping *
								            isneg(m_vars.nGroupDamping - 1 -
								                  nEnts)); // max(nEnts,g_nBodies));
								for (pent = m_pTmpEntList1[i], bGroupFinished = 1; pent;
								     pent = pent->m_next_coll)
								{
									Eafter += pent->CalcEnergy(0);
									if (!(pent->m_flags & pef_fixed_damping))
									{
										damping = min(
										    damping,
										    pent->GetDamping(m_groupTimeStep));
									}
									else
									{
										damping =
										    pent->GetDamping(m_groupTimeStep);
										break;
									}
								}
								Ebefore *= isneg(-nAnimatedObjects) +
								           1; // increase energy growth limit if we have
								              // animated bodies involved
								if (Eafter >
								    Ebefore * (1.0f + 0.1f * isneg(g_nBodies - 15)))
								{
									damping =
									    min(damping, sqrt_tpl(Ebefore / Eafter));
								}
								if (fixedDamping > -0.5f)
								{
									damping = fixedDamping;
								}
								for (pent = m_pTmpEntList1[i], bGroupFinished = 1; pent;
								     pent = pent->m_next_coll)
								{
									bGroupFinished &=
									    pent->Update(m_groupTimeStep, damping);
								}
								bGroupFinished |=
								    isneg((m_vars.nMaxSubstepsLargeGroup - iter - 2) &
								          (m_vars.nBodiesLargeGroup - g_nBodies - 1));
								for (pent = m_pTmpEntList1[i]; pent;
								     pent = pent->m_next_coll)
								{
									pent->m_bMoved = bGroupFinished;
								}
								bAllGroupsFinished &= bGroupFinished;

								// process deforming (breaking) enities of this group
								{
									WriteLock lock3(m_lockDeformingEntsList);
									for (i1 = j = nBrokenParts = 0;
									     i1 < m_nDeformingEnts; i1++)
									{
										if (m_pDeformingEnts[i1]->m_iGroup ==
										    m_iCurGroup)
										{
											if ((nParts0 =
											         m_pDeformingEnts[i1]
											             ->m_nParts) &&
											    m_pDeformingEnts[i1]
											            ->m_iSimClass != 7)
											{
												if (m_pDeformingEnts[i1]
												        ->UpdateStructure(
													    max(m_groupTimeStep,
												                0.01f),
													    0))
												{
													m_pDeformingEnts
													    [j++] =
														m_pDeformingEnts
														    [i1];
												}
												else
												{
													m_pDeformingEnts
													    [i1]
														->m_flags &=
													    ~pef_deforming;
												}
												nBrokenParts +=
												    -iszero(
													(int)m_pDeformingEnts
													    [i1]
														->m_flags &
													aef_recorded_physics) &
												    (nParts0 -
												     m_pDeformingEnts[i1]
												         ->m_nParts);
											}
											else
											{
												m_pDeformingEnts[i1]
												    ->m_flags &=
												    ~pef_deforming;
											}
											m_pDeformingEnts[i1]->m_iGroup =
											    -1;
										}
										else
										{
											m_pDeformingEnts[j++] =
											    m_pDeformingEnts[i1];
										}
									}
									m_nDeformingEnts = j;
								}
								// if some entities broke, step back the velocities, but
								// don't re-execute the step immediately
								if (nBrokenParts)
								{
									for (pent = m_pTmpEntList1[i]; pent;
									     pent = pent->m_next_coll)
									{
										pent->StepBack(0);
									}
								}
							}
							minGroupTimeStep = min(minGroupTimeStep, m_groupTimeStep);
						}

						if (ipass == 0)
						{
							m_bWorldStep = 2;
							for (pent = m_pAuxStepEnt; pent; pent = pent_next)
							{
								pent_next = pent->m_next_coll2;
								pent->m_flags &= ~pef_step_requested;
								pent->Step(pent->GetMaxTimeStep(m_groupTimeStep));
								pent->m_bMoved = 0;
							}
							m_bWorldStep = 1;
							m_pAuxStepEnt = 0;
						}
					}
				}
				while (!bAllGroupsFinished && ++iter < m_vars.nMaxSubsteps);
			}

			for (pent = m_pTypedEnts[1]; pent; pent = pent->m_next)
			{
				pent->m_bMoved = 0, pent->m_iGroup = -1;
			}
			for (pent = m_pTypedEnts[2]; pent; pent = pent->m_next)
			{
				pent->m_bMoved = 0;
			}
			for (pent = m_pTypedEnts[4]; pent; pent = pent->m_next)
			{
				pent->m_bMoved = 0, pent->m_iGroup = -1;
			}
			m_updateTimes[1] = m_updateTimes[2] = m_timePhysics;
		}

		if (m_pWaterMan)
		{
			m_pWaterMan->TimeStep(time_interval);
		}
		for (i = 0; i < m_nProfiledEnts; i++)
		{
			m_pEntProfileData[i].nTicksStep &= -m_pEntProfileData[i].nTicks >> 31;
		}
	}
	m_iSubstep++;

	if (flags & ent_living)
	{
		// for(pent=m_pTypedEnts[3]; pent; pent=pent->m_next) if (!(m_bUpdateOnlyFlagged &
		// (pent->m_flags^pef_update))) 	pent->StartStep(time_interval_org);	// prepare to advance
		// living
		// entities
		for (pent = m_pTypedEnts[3]; pent; pent = pent_next)
		{
			pent_next = pent->m_next;
			if (!((m_bUpdateOnlyFlagged & (pent->m_flags ^ pef_update)) | (bSkipFlagged & pent->m_flags)))
			{
				pent->Step(pent->GetMaxTimeStep(time_interval_org *
				                                m_vars.timeScalePlayers)); // advance living entities
			}
		}
		m_updateTimes[3] = m_timePhysics;
	}

	if (!m_vars.bSingleStepMode || m_vars.bDoStep)
	{
		if (flags & ent_independent)
		{
			for (pent = m_pTypedEnts[4]; pent; pent = pent->m_next)
			{
				if (!((m_bUpdateOnlyFlagged & (pent->m_flags ^ pef_update)) |
				      (bSkipFlagged & pent->m_flags)))
				{
					for (iter = 0; !pent->Step(pent->GetMaxTimeStep(time_interval)) &&
					               ++iter < m_vars.nMaxSubsteps;)
						;
				}
			}
			m_updateTimes[4] = m_timePhysics;
		}
	}

	if (flags & ent_deleted)
	{
		if (!m_vars.bSingleStepMode || m_vars.bDoStep)
		{
			// process deforming (breaking) enities
			{
				WriteLock lock3(m_lockDeformingEntsList);
				for (i = j = 0; i < m_nDeformingEnts; i++)
				{
					if (m_pDeformingEnts[i]->m_iSimClass != 7 &&
					    m_pDeformingEnts[i]->UpdateStructure(time_interval, 0))
					{
						m_pDeformingEnts[j++] = m_pDeformingEnts[i];
					}
					else
					{
						m_pDeformingEnts[i]->m_flags &= ~pef_deforming;
					}
				}
				m_nDeformingEnts = j;
				m_updateTimes[0] = m_timePhysics;
			}

			CleanseEventsQueue(); // remove events that reference deleted entities

			for (pent = m_pTypedEnts[7]; pent; pent = pent_next)
			{ // purge deletion requests
				pent_next = pent->m_next;
				if (m_iLastLogPump >= pent->m_iDeletionTime && pent->m_nRefCount <= 0)
				{
					if (pent->m_next)
					{
						pent->m_next->m_prev = pent->m_prev;
					}
					(pent->m_prev ? pent->m_prev->m_next : m_pTypedEnts[7]) = pent->m_next;
					delete pent;
				}
			}
			// m_pTypedEnts[7] = 0;
		}

		// flush timeouted sectors for cell-based physics-on-demand
		{
			WriteLock lockPOD(m_lockPODGrid);
			SetPodThreadId();
			pe_PODcell* pPODcell;
			int* picellNext;
			for (i = m_iActivePODCell0, picellNext = &m_iActivePODCell0; i >= 0; i = pPODcell->inextActive)
			{
				if (((pPODcell = getPODcell(i & 0xFFFF, i >> 16))->lifeTime -= time_interval_org) <=
				        0 ||
				    pPODcell->lifeTime > 1E9f)
				{
					Vec3 center, sz;
					++m_iLastPODUpdate;
					GetPODGridCellBBox(i & 0xFFFF, i >> 16, center, sz);
					m_pPhysicsStreamer->DestroyPhysicalEntitiesInBox(center - sz, center + sz);
					*picellNext = pPODcell->inextActive;
				}
				else
				{
					picellNext = &pPODcell->inextActive;
				}
			}
			UnsetPodThreadId();
		}

		// flush static and sleeping physical objects that have timeouted
		for (i = 0; i < 2; i++)
		{
			for (pent = m_pTypedEnts[i]; pent != m_pTypedEntsPerm[i]; pent = pent_next)
			{
				pent_next = pent->m_next;
				{
					WriteLock lockEnt(pent->m_lockUpdate);
					for (j = 0; j < pent->m_nParts && !(pent->m_parts[j].flags & geom_can_modify);
					     j++)
						;
					if (j < pent->m_nParts || pent->m_pStructure && pent->m_pStructure->bModified)
					{
						j = -1;
					}
				}
				if (j == -1)
				{
					j -= pent_next == m_pTypedEntsPerm[i];
					pent->m_bPermanent = 1;
					ChangeEntitySimClass(pent);
					if (pent->m_pEntBuddy)
					{
						CPhysicalPlaceholder* ppc = pent->m_pEntBuddy;
						ppc->m_pEntBuddy = 0;
						ppc->m_iGThunk0 = 0;
						SetPhysicalEntityId(ppc, -1, 1, 1);
						ppc->m_id = -1;
						pent->m_pEntBuddy = 0;
						SetPhysicalEntityId(pent, pent->m_id, 1, 1);
						for (i1 = pent->m_iGThunk0; i1; i1 = m_gthunks[i1].inextOwned)
						{
							m_gthunks[i1].pent = pent;
						}
						DestroyPhysicalEntity(ppc, 0, 1);
					}
					if (j == -2)
					{
						break;
					}
				}
				else if (pent->m_nRefCount == 0 &&
				         ((pent->m_timeIdle += time_interval_org) > pent->m_maxTimeIdle ||
				          pent->m_timeIdle < 0))
				{
					DestroyPhysicalEntity(pent, 0, 1);
				}
			}
		}

		for (pent = m_pTypedEnts[2]; pent != m_pTypedEntsPerm[2]; pent = pent->m_next)
		{
			pent->m_timeIdle = 0; // reset idle count for active physical entities
		}

		for (pent = m_pTypedEnts[4]; pent != m_pTypedEntsPerm[4]; pent = pent_next)
		{
			pent_next = pent->m_next;
			if (pent->IsAwake())
			{
				pent->m_timeIdle = 0; // reset idle count for active detached entities
			}
			else if (pent->m_nRefCount == 0 &&
			         (pent->m_timeIdle += time_interval_org) > pent->m_maxTimeIdle)
			{
				DestroyPhysicalEntity(pent);
			}
		}

		// flush deleted areas
		{
			WriteLock lockAreas(m_lockAreas);
			CPhysArea *pArea, **ppNextArea = &m_pDeletedAreas;
			for (pArea = m_pDeletedAreas; pArea; pArea = *ppNextArea)
			{
				if (pArea->m_lockRef == 0)
				{
					*ppNextArea = pArea->m_next;
					delete pArea;
				}
				else
				{
					ppNextArea = &pArea->m_next;
				}
			}
		}

		m_updateTimes[7] = m_timePhysics;
		if (m_vars.bDoStep == 2)
		{
			m_vars.bDoStep = 0;
			SerializeWorld("D:\\worldents.txt", 1);
			SerializeGeometries("D:\\worldgeoms.txt", 1);
		}
		m_vars.bDoStep = 0;
	}
	m_bUpdateOnlyFlagged = 0;
	m_bWorldStep = 0;
	if (time_interval > 0)
	{
		++m_idStep;
	}
}

void CPhysicalWorld::DetachEntityGridThunks(CPhysicalPlaceholder* pobj)
{
	if (pobj->m_iGThunk0)
	{
		int ithunk, ithunk_next, ithunk_last = 0, icell;
		for (ithunk = pobj->m_iGThunk0; ithunk; ithunk = ithunk_next)
		{
			ithunk_next = m_gthunks[ithunk].inextOwned;
			m_gthunks[m_gthunks[ithunk].inext].iprev =
			    m_gthunks[ithunk].iprev & -(int)m_gthunks[ithunk].inext >> 31;
			m_gthunks[m_gthunks[ithunk].inext].bFirstInCell = m_gthunks[ithunk].bFirstInCell;
			if (m_gthunks[ithunk].bFirstInCell)
			{
				icell = m_entgrid.size.x * m_entgrid.size.y;
				if (m_pEntGrid[icell] != ithunk)
				{
					icell = Vec2i(m_gthunks[ithunk].iprev & 1023,
					              m_gthunks[ithunk].iprev >> 10 & 1023) *
					        m_entgrid.stride;
				}
				m_pEntGrid[icell] = m_gthunks[ithunk].inext;
			}
			else
			{
				m_gthunks[m_gthunks[ithunk].iprev].inext = m_gthunks[ithunk].inext;
			}
			m_gthunks[ithunk].pent = 0;
			ithunk_last = ithunk;
		}
		m_gthunks[ithunk_last].inextOwned = m_iFreeGThunk0;
		m_iFreeGThunk0 = pobj->m_iGThunk0;
		pobj->m_iGThunk0 = 0;
	}
}

void CPhysicalWorld::ChangeEntitySimClass(CPhysicalEntity* pent)
{
	WriteLock lock(m_lockList);
	if ((unsigned int)pent->m_iPrevSimClass < 8u)
	{
		if (pent->m_next)
		{
			pent->m_next->m_prev = pent->m_prev;
		}
		(pent->m_prev ? pent->m_prev->m_next : m_pTypedEnts[pent->m_iPrevSimClass]) = pent->m_next;
		if (pent == m_pTypedEntsPerm[pent->m_iPrevSimClass])
		{
			m_pTypedEntsPerm[pent->m_iPrevSimClass] = pent->m_next;
		}
	}

	if (!pent->m_bPermanent)
	{
		pent->m_next = m_pTypedEnts[pent->m_iSimClass];
		pent->m_prev = 0;
		if (pent->m_next)
		{
			pent->m_next->m_prev = pent;
		}
		m_pTypedEnts[pent->m_iSimClass] = pent;
	}
	else
	{
		pent->m_next = m_pTypedEntsPerm[pent->m_iSimClass];
		if (m_pTypedEntsPerm[pent->m_iSimClass])
		{
			pent->m_prev = m_pTypedEntsPerm[pent->m_iSimClass]->m_prev;
			if (pent->m_prev)
			{
				pent->m_prev->m_next = pent;
			}
			pent->m_next->m_prev = pent;
		}
		else if (m_pTypedEnts[pent->m_iSimClass])
		{
			for (pent->m_prev = m_pTypedEnts[pent->m_iSimClass]; pent->m_prev && pent->m_prev->m_next;
			     pent->m_prev = pent->m_prev->m_next)
				;
			pent->m_prev->m_next = pent;
		}
		else
		{
			pent->m_prev = 0;
		}
		if (m_pTypedEntsPerm[pent->m_iSimClass] == m_pTypedEnts[pent->m_iSimClass])
		{
			m_pTypedEnts[pent->m_iSimClass] = pent;
		}
		m_pTypedEntsPerm[pent->m_iSimClass] = pent;
	}

	for (int ithunk = pent->m_iGThunk0; ithunk; ithunk = m_gthunks[ithunk].inextOwned)
	{
		m_gthunks[ithunk].iSimClass = pent->m_iSimClass;
	}
}

int CPhysicalWorld::RepositionEntity(CPhysicalPlaceholder* pobj, int flags, Vec3* BBox, int bQueued)
{
	int i, j, igx[2], igy[2], igxInner[2], igyInner[2], ix, iy, ithunk, ithunk0;
	unsigned int n;
	if ((unsigned int)pobj->m_iSimClass >= 7u)
	{
		return 0; // entity is frozen
	}
	int bGridLocked = 0;

	if (flags & 1 && m_pEntGrid)
	{
		i = -iszero((INT_PTR)BBox);
		Vec3* pBBox = (Vec3*)(((INT_PTR)pobj->m_BBox & (INT_PTR)i) | ((INT_PTR)BBox & ~(INT_PTR)i));
		for (i = 0; i < 2; i++)
		{
			float x = (pBBox[i][inc_mod3[m_iEntAxisz]] - m_entgrid.origin[inc_mod3[m_iEntAxisz]]) *
			          m_entgrid.stepr.x;
			igx[i] = max(-1, min(m_entgrid.size.x, physics_float2int(x - 0.5f)));
			igxInner[i] = max(0, min(255, physics_float2int(((x - igx[i]) * 256.0f) - 0.5f)));
			x = (pBBox[i][dec_mod3[m_iEntAxisz]] - m_entgrid.origin[dec_mod3[m_iEntAxisz]]) *
			    m_entgrid.stepr.y;
			igy[i] = max(-1, min(m_entgrid.size.y, physics_float2int(x - 0.5f)));
			igyInner[i] = max(0, min(255, physics_float2int(((x - igy[i]) * 256.0f) - 0.5f)));
		}
		if (pobj->m_ig[0].x != -3) // if m_igx[0] is -3, the entity should not be registered in grid at all
		{
			if ((igx[0] - pobj->m_ig[0].x) | (igy[0] - pobj->m_ig[0].y) | (igx[1] - pobj->m_ig[1].x) |
			    (igy[1] - pobj->m_ig[1].y))
			{
				CPhysicalPlaceholder* pcurobj = pobj;
				if (IsPlaceholder(pobj->m_pEntBuddy))
				{
					goto skiprepos; // pcurobj = pobj->m_pEntBuddy;
				}
				SpinLock(&m_lockGrid, 0, bGridLocked = WRITE_LOCK_VAL);
				m_bGridThunksChanged = 1;
				DetachEntityGridThunks(pobj);
				n = (igx[1] - igx[0] + 1) * (igy[1] - igy[0] + 1);
				if (pobj->m_iSimClass != 5)
				{
					if (n <= 0 || n > m_vars.nMaxEntityCells)
					{
						Vec3 pos = (pcurobj->m_BBox[0] + pcurobj->m_BBox[1]) * 0.5f;
						CryLogError("%s @ %.1f,%.1f,%.1f is too large or invalid",
						            !m_pRenderer
						                ? "entity"
						                : m_pRenderer->GetForeignName(pcurobj->m_pForeignData,
						                                              pcurobj->m_iForeignData,
						                                              pcurobj->m_iForeignFlags),
						            pos.x, pos.y, pos.z);
						goto skiprepos;
					}
				}
				else if (n > m_vars.nMaxAreaCells)
				{
					return -1;
				}
				for (ix = igx[0]; ix <= igx[1]; ix++)
				{
					for (iy = igy[0]; iy <= igy[1]; iy++)
					{
						j = m_entgrid.getcell_safe(ix, iy);
						if (!m_iFreeGThunk0)
						{
							pe_gridthunk* prevthunks = m_gthunks;
							memcpy(m_gthunks = new pe_gridthunk[m_thunkPoolSz * 2],
							       prevthunks, m_thunkPoolSz * sizeof(pe_gridthunk));
							memset(m_gthunks + m_thunkPoolSz, 0,
							       m_thunkPoolSz * sizeof(pe_gridthunk));
							for (ithunk = m_thunkPoolSz; ithunk < (m_thunkPoolSz * 2) - 1;
							     ithunk++)
							{
								m_gthunks[ithunk].inextOwned = ithunk + 1;
							}
							m_gthunks[ithunk].inextOwned = 0;
							delete[] prevthunks;
							m_iFreeGThunk0 = m_thunkPoolSz;
							m_thunkPoolSz *= 2;
						}
						ithunk = m_iFreeGThunk0;
						m_iFreeGThunk0 = m_gthunks[m_iFreeGThunk0].inextOwned;
						m_gthunks[ithunk].inextOwned = pcurobj->m_iGThunk0;
						pcurobj->m_iGThunk0 = ithunk;

						if (!m_pEntGrid[j] || m_gthunks[m_pEntGrid[j]].iSimClass != 5)
						{
							m_gthunks[ithunk].bFirstInCell = 1;
							m_gthunks[ithunk].iprev = iy << 10 | ix;
							m_gthunks[ithunk].inext = m_pEntGrid[j];
							m_gthunks[m_pEntGrid[j]].iprev = ithunk & -m_pEntGrid[j] >> 31;
							m_gthunks[m_pEntGrid[j]].bFirstInCell = 0;
							m_pEntGrid[j] = ithunk;
						}
						else
						{
							for (ithunk0 = m_pEntGrid[j];
							     m_gthunks[m_gthunks[ithunk0].inext].iSimClass == 5;
							     ithunk0 = m_gthunks[ithunk0].inext)
								;
							m_gthunks[ithunk].bFirstInCell = 0;
							m_gthunks[ithunk].inext = m_gthunks[ithunk0].inext;
							m_gthunks[ithunk].iprev = ithunk0;
							m_gthunks[m_gthunks[ithunk0].inext].iprev =
							    ithunk & -(int)m_gthunks[ithunk0].inext >> 31;
							m_gthunks[ithunk0].inext = ithunk;
						}

						m_gthunks[ithunk].iSimClass = pcurobj->m_iSimClass;
						m_gthunks[ithunk].BBox[0] = igxInner[0] & ~((igx[0] - ix) >> 31);
						m_gthunks[ithunk].BBox[1] = igyInner[0] & ~((igy[0] - iy) >> 31);
						m_gthunks[ithunk].BBox[2] =
						    igxInner[1] + ((255 - igxInner[1]) & (ix - igx[1]) >> 31);
						m_gthunks[ithunk].BBox[3] =
						    igyInner[1] + ((255 - igyInner[1]) & (iy - igy[1]) >> 31);
						m_gthunks[ithunk].pent = pcurobj;
					}
				}
				pcurobj->m_ig[0].x = igx[0];
				pcurobj->m_ig[1].x = igx[1];
				pcurobj->m_ig[0].y = igy[0];
				pcurobj->m_ig[1].y = igy[1];
				if (pcurobj->m_pEntBuddy)
				{
					pcurobj->m_pEntBuddy->m_iGThunk0 = pcurobj->m_iGThunk0;
					pcurobj->m_pEntBuddy->m_ig[0].x = igx[0];
					pcurobj->m_pEntBuddy->m_ig[1].x = igx[1];
					pcurobj->m_pEntBuddy->m_ig[0].y = igy[0];
					pcurobj->m_pEntBuddy->m_ig[1].y = igy[1];
				}
			skiprepos:;
			}
			else
			{
				for (ix = igx[1], ithunk = pobj->m_iGThunk0; ix >= igx[0]; ix--)
				{
					for (iy = igy[1]; iy >= igy[0]; iy--, ithunk = m_gthunks[ithunk].inextOwned)
					{
						m_gthunks[ithunk].BBox[0] = igxInner[0] & ~((igx[0] - ix) >> 31);
						m_gthunks[ithunk].BBox[1] = igyInner[0] & ~((igy[0] - iy) >> 31);
						m_gthunks[ithunk].BBox[2] =
						    igxInner[1] + ((255 - igxInner[1]) & (ix - igx[1]) >> 31);
						m_gthunks[ithunk].BBox[3] =
						    igyInner[1] + ((255 - igyInner[1]) & (iy - igy[1]) >> 31);
					}
				}
			}
		}
	}

	if (flags & 2)
	{
		CPhysicalEntity* pent = (CPhysicalEntity*)pobj;
		if (pent->m_iPrevSimClass != pent->m_iSimClass && pent->m_bPermanent + bQueued)
		{
			ChangeEntitySimClass(pent);
			i = pent->m_iPrevSimClass;
			pent->m_iPrevSimClass = pent->m_iSimClass;

			if (pent->m_flags & (pef_monitor_state_changes | pef_log_state_changes))
			{
				EventPhysStateChange event;
				event.pEntity = pent;
				event.pForeignData = pent->m_pForeignData;
				event.iForeignData = pent->m_iForeignData;
				event.iSimClass[0] = i;
				event.iSimClass[1] = pent->m_iSimClass;
				OnEvent(pent->m_flags, &event);
			}
		}
	}

	return bGridLocked;
}

namespace {

bool ray_box_overlap2d(const Vec2& org, const Vec2& dir, const Vec2& boxmin, const Vec2& boxmax)
{
	Vec2 n(dir.y, -dir.x), center = (boxmin + boxmax) * 0.5f, size = (boxmax - boxmin) * 0.5f;
	return max(max(fabs_tpl(center.x - org.x - (dir.x * 0.5f)) - (size.x + fabs_tpl(dir.x) * 0.5f),
	               fabs_tpl(center.y - org.y - (dir.y * 0.5f)) - (size.y + fabs_tpl(dir.y) * 0.5f)),
	           fabs_tpl(n * (center - org)) - (fabs_tpl(n.x) * size.x) - (fabs_tpl(n.y) * size.y)) <= 0;
}

struct entity_grid_checker
{
	geom_world_data gwd;
	intersection_params ip;
	int nMaxHits, nThroughHits, nThroughHitsAux, objtypes, nEnts, bCallbackUsed, nParts;
	unsigned int flags, flagsColliderAll, flagsColliderAny;
	vector2df org2d, dir2d;
	float dir2d_len, maxt;
	ray_hit* phits;
	CPhysicalWorld* pWorld;
	CPhysicalPlaceholder* pGridEnt;
	CPhysicalEntity** pTmpEntList;
	void* pSkipForeignData;
	int iSkipForeignData;
	int iCaller;
	int bUsePhysOnDemand;
	CRayGeom aray;
	pe_gridthunk *pThunkSubst, thunkSubst;
	int ipartSubst, ipartMask;
	int iSolidNode;
	entity_grid_checker()
	{
		pThunkSubst = 0;
		ipartSubst = ipartMask = 0;
	}

	int check_cell(const vector2di& icell, int& ilastcell)
	{
		quotientf t((org2d + icell) * dir2d, dir2d_len * dir2d_len);
		if (t.x > maxt && (icell.x & icell.y) != -1)
		{
			return 1;
		}

		primitives::box bbox;
		bbox.Basis.SetIdentity();
		bbox.bOriented = 0;
		geom_contact* pcontacts;
		pe_gridthunk* thunk;
		int ithunk, ithunk_next;
		pe_PODcell* pPODcell;
		int i, j, ihit, imat, pierceability, nCellEnts = 0, nEntsChecked = 0, bRecheckOtherParts, bNoThunkSubst;
		pWorld->GetPODGridCellBBox(icell.x, icell.y, bbox.center, bbox.size);
		if (bbox.size.z * bUsePhysOnDemand > 0 && box_ray_overlap_check(&bbox, &aray.m_ray, nullptr))
		{
			CryInterlockedAdd(&pWorld->m_lockGrid, -1);
			ReadLock lockPOD(pWorld->m_lockPODGrid);
			if ((pPODcell = pWorld->getPODcell(icell.x, icell.y))->lifeTime <= 0)
			{
				CryInterlockedAdd(&pWorld->m_lockPODGrid, -1);
				{
					WriteLock lockPODw(pWorld->m_lockPODGrid);
					if (pPODcell->lifeTime <= 0)
					{
						pWorld->SetPodThreadId();
						pWorld->m_nOnDemandListFailures = 0;
						++pWorld->m_iLastPODUpdate;
						if (pWorld->m_pPhysicsStreamer->CreatePhysicalEntitiesInBox(
							bbox.center - bbox.size, bbox.center + bbox.size))
						{
							pPODcell->lifeTime =
							    pWorld->m_nOnDemandListFailures ? 1E10f : 8.0f;
							pPODcell->inextActive = pWorld->m_iActivePODCell0;
							pWorld->m_iActivePODCell0 = icell.y << 16 | icell.x;
						}
						pWorld->m_nOnDemandListFailures = 0;
					}
				}
				ReadLockCond lockPODr(pWorld->m_lockPODGrid, 1);
				lockPODr.SetActive(0);
			}
			else
			{
				pPODcell->lifeTime = max(8.0f, pPODcell->lifeTime);
			}
			pWorld->UnsetPodThreadId();
			ReadLockCond relock(pWorld->m_lockGrid, 1);
			relock.SetActive(0);
		}
		thunk =
		    pWorld->m_gthunks + (ithunk = pWorld->m_pEntGrid[pWorld->m_entgrid.getcell_safe(icell.x, icell.y)]);
		bNoThunkSubst = iszero_mask(pThunkSubst);
		thunk = (pe_gridthunk*)((intptr_t)thunk + ((intptr_t)pThunkSubst - (intptr_t)thunk & ~bNoThunkSubst));

		for (; ithunk; thunk = pWorld->m_gthunks + (ithunk = ithunk_next))
		{
			ithunk_next = thunk->inext;
			if (objtypes & 1u << thunk->iSimClass &&
			    (!(pWorld->m_entgrid.inrange(icell.x, icell.y) & -bNoThunkSubst) ||
			     ray_box_overlap2d(Vec2(0.5f - org2d.x, 0.5f - org2d.y), dir2d,
			                       Vec2(icell.x + (thunk->BBox[0] * (1.0f / 256)),
			                            icell.y + (thunk->BBox[1] * (1.0f / 256))),
			                       Vec2(icell.x + ((thunk->BBox[2] + 1) * (1.0f / 256)),
			                            icell.y + ((thunk->BBox[3] + 1) * (1.0f / 256))))) &&
			    !((thunk->pent->m_bProcessed >> iCaller & 1) |
			      -((CPhysicalEntity*)thunk->pent)->m_iDeletionTime >> 31) &&
			    (!pSkipForeignData || thunk->pent->GetForeignData(iSkipForeignData) != pSkipForeignData))
			{
				ReadLock lock(thunk->pent->m_lockUpdate);
				bbox.center = (thunk->pent->m_BBox[0] + thunk->pent->m_BBox[1]) * 0.5f;
				bbox.size = (thunk->pent->m_BBox[1] - thunk->pent->m_BBox[0]) * 0.5f;
				nCellEnts++;

				if ((bbox.center - aray.m_ray.origin -
				     aray.m_dirn * ((bbox.center - aray.m_ray.origin) * aray.m_dirn))
				        .len2() > bbox.size.len2())
				{
					continue; // skip objects that lie to far from the ray
				}
				if ((box_ray_overlap_check(&bbox, &aray.m_ray, nullptr) &
				     (nEnts - pWorld->m_nEnts) >> 31) == 0)
				{
					continue;
				}
				if (nEnts >= pWorld->m_nEnts)
				{
					continue;
				}
				nEntsChecked++;
				CPhysicalEntity *pent, *pentLog, *pentFlags;
				bCallbackUsed = bRecheckOtherParts = 0;

				if (thunk->pent->m_iSimClass == 5)
				{
					if (((CPhysArea*)thunk->pent)->m_pb.iMedium != 0)
					{
						continue;
					}
					ray_hit ahit;
					if (((CPhysArea*)thunk->pent)
					        ->RayTrace(aray.m_ray.origin, aray.m_ray.dir, &ahit))
					{
						WriteLockCond lock(g_idata[iCaller].lockIntersect, 1);
						lock.SetActive(0);
						ip.plock = &g_idata[iCaller].lockIntersect;
						pcontacts = g_idata[iCaller].Contacts;
						pentFlags = &g_StaticPhysicalEntity;
						pentFlags->m_parts[0].flags =
						    geom_colltype_ray &
						    -iszero((int)flags & rwi_force_pierceable_noncoll);
						pcontacts->t = ahit.dist;
						pcontacts->pt = ahit.pt;
						pcontacts->n = ahit.n;
						pcontacts->id[0] = pWorld->m_matWater;
						pcontacts->iNode[0] = -1;
						pent = (CPhysicalEntity*)(pGridEnt = thunk->pent);
						i = 0;
						j = 1;
						nParts = 0;
						pentLog = 0;
						goto gotcontacts;
					}
					continue;
				}

				pWorld->m_bGridThunksChanged = 0;
				pentLog = pentFlags = pent = (pGridEnt = thunk->pent)->GetEntity();
				if (pWorld->m_bGridThunksChanged)
				{
					ithunk_next =
					    pWorld->m_pEntGrid[pWorld->m_entgrid.getcell_safe(icell.x, icell.y)];
				}
				pWorld->m_bGridThunksChanged = 0;

				if ((nParts = pent->m_nParts) == 0 || pent->m_flags & pef_use_geom_callbacks)
				{
					j = pent->RayTrace(&aray, pcontacts, ip.plock);
					i = 0;
					bCallbackUsed = 1;
					goto gotcontacts;
				}

				for (i = ipartSubst; i < nParts + (ipartSubst + 1 - nParts & ipartMask); i++)
				{
					if ((pent->m_parts[i].flags & flagsColliderAll) == flagsColliderAll &&
					    (pent->m_parts[i].flags & flagsColliderAny))
					{
						if (nParts > 1)
						{
							bbox.center =
							    (pent->m_parts[i].BBox[0] + pent->m_parts[i].BBox[1]) *
							    0.5f;
							bbox.size =
							    (pent->m_parts[i].BBox[1] - pent->m_parts[i].BBox[0]) *
							    0.5f;
							if (!box_ray_overlap_check(&bbox, &aray.m_ray, nullptr))
							{
								continue;
							}
						}
						gwd.offset = pent->m_pos + pent->m_qrot * pent->m_parts[i].pos;
						//(pent->m_qrot*pent->m_parts[i].q).getmatrix(gwd.R);	//Q2M_IVO
						gwd.R = Matrix33(pent->m_qrot * pent->m_parts[i].q);
						gwd.scale = pent->m_parts[i].scale;
						j = pent->m_parts[i].pPhysGeom->pGeom->Intersect(&aray, &gwd, 0, &ip,
						                                                 pcontacts);
					gotcontacts:
						bRecheckOtherParts = (j - 1 & 1 - nParts) >> 31 & ipartMask;

						{
							WriteLockCond lockColl(*ip.plock, 0);
							lockColl.SetActive(isneg(-j));
							float facing;
							for (j--; j >= 0; j--)
							{
								if (pcontacts[j].t < phits[0].dist &&
								    (flags & rwi_ignore_back_faces) *
								            (facing = pcontacts[j].n * aray.m_dirn) <=
								        0)
								{
									imat =
									    pentFlags->GetMatId(pcontacts[j].id[0], i);
									pierceability =
									    pWorld->m_SurfaceFlagsTable[imat &
									                                (NSURFACETYPES -
									                                 1)] &
									    sf_pierceable_mask;
									ihit =
									    -(int)(flags &
									           rwi_force_pierceable_noncoll) >>
										31 &
									    -iszero((int)pentFlags->m_parts[i].flags &
									            (geom_colltype_solid |
									             geom_colltype_ray));
									pierceability +=
									    (sf_max_pierceable + 1 - pierceability) &
									    ihit;
									ihit = 0;
									if ((flags & rwi_pierceability_mask) <
									    pierceability)
									{
										if ((pWorld->m_SurfaceFlagsTable
										         [imat & (NSURFACETYPES - 1)] |
										     flags) &
										    sf_important)
										{
											for (ihit = 1;
											     ihit <= nThroughHits &&
											     phits[ihit].dist <
											         pcontacts[j].t;
											     ihit++)
												;
											if (ihit <= nThroughHits)
											{
												memmove(
												    phits + ihit + 1,
												    phits + ihit,
												    (min(nThroughHits +
												             1,
												         nMaxHits - 1) -
												     ihit) *
													sizeof(
													    ray_hit));
											}
											else if (nThroughHits + 1 ==
											         nMaxHits)
											{
												continue;
											}
											nThroughHits =
											    min(nThroughHits + 1,
											        nMaxHits - 1);
											nThroughHitsAux =
											    min(nThroughHitsAux,
											        nMaxHits - 1 -
											            nThroughHits);
										}
										else
										{
											for (ihit = nMaxHits - 1;
											     ihit >=
											         nMaxHits -
											             nThroughHitsAux &&
											     phits[ihit].dist <
											         pcontacts[j].t;
											     ihit--)
												;
											if (ihit >=
											    nMaxHits - nThroughHitsAux)
											{
												int istart = max(
												    nMaxHits -
													nThroughHitsAux -
													1,
												    nThroughHits + 1);
												memmove(
												    phits + istart,
												    phits + istart + 1,
												    (ihit - istart) *
													sizeof(
													    ray_hit));
											}
											else if (nThroughHits +
											             nThroughHitsAux >=
											         nMaxHits - 1)
											{
												continue;
											}
											nThroughHitsAux =
											    min(nThroughHitsAux + 1,
											        nMaxHits - 1 -
											            nThroughHits);
										}
									}
									else
									{
										if ((flags &
										     rwi_ignore_solid_back_faces) *
										        facing >
										    0)
										{
											continue;
										}
										ilastcell =
										    physics_float2int(
											((pcontacts[j].pt
										              [inc_mod3
										                   [pWorld
										                        ->m_iEntAxisz]] -
										          pWorld->m_entgrid.origin
										              [inc_mod3
										                   [pWorld
										                        ->m_iEntAxisz]]) *
										         pWorld->m_entgrid.stepr.x) -
											0.5f) |
										    physics_float2int(
											((pcontacts[j].pt
										              [dec_mod3
										                   [pWorld
										                        ->m_iEntAxisz]] -
										          pWorld->m_entgrid.origin
										              [dec_mod3
										                   [pWorld
										                        ->m_iEntAxisz]]) *
										         pWorld->m_entgrid.stepr.y) -
											0.5f)
											<< 16;
										aray.m_ray.dir =
										    pcontacts[j].pt - aray.m_ray.origin;
										iSolidNode = pcontacts[j].iNode[0];
									}
									phits[ihit].dist = pcontacts[j].t;
									phits[ihit].pCollider = pentLog;
									phits[ihit].ipart =
									    i + (pcontacts[j].iNode[0] - i &
									         -bCallbackUsed);
									phits[ihit].partid = pcontacts[j].iPrim[0];
									phits[ihit].surface_idx = imat;
									phits[ihit].idmatOrg =
									    pcontacts[j].id[0] +
									    (pentFlags->m_parts[i].surface_idx + 1 &
									     pcontacts[j].id[0] >> 31);
									phits[ihit].pt = pcontacts[j].pt;
									phits[ihit].n = pcontacts[j].n;
									phits[ihit].iNode = pcontacts[j].iNode[0];
									phits[ihit].bTerrain = 0;
								}
							}
						}
					}
				}

				pTmpEntList[nEnts] = pent;
				nEnts += 1 + bRecheckOtherParts;
				AtomicAdd(&pGridEnt->m_bProcessed, (1 + bRecheckOtherParts) << iCaller);
			}
		}
		return (sgn((icell.y << 16 | icell.x) - ilastcell) & 1) ^ 1;
	}
};

} // unnamed namespace

int CPhysicalWorld::RayWorldIntersection(Vec3 org, Vec3 dir, int objtypes, unsigned int flags, ray_hit* hits,
                                         int nMaxHits, IPhysicalEntity** pSkipEnts, int nSkipEnts, void* pForeignData,
                                         int iForeignData, const char* pNameTag, ray_hit_cached* phitLast, int iCaller)
{
	if (!(dir.len2() < 1E20f && org.len2() < 1E20f))
	{
		return 0;
	}
	if (dir.len2() == 0)
	{
		return 0;
	}

	if (flags & rwi_queue)
	{
		WriteLock lockQ(m_lockRwiQueue);
		int i;
		if (m_rwiQueueSz == m_rwiQueueAlloc)
		{
			SRwiRequest* pqueue = m_rwiQueue;
			m_rwiQueue = new SRwiRequest[m_rwiQueueAlloc + 64];
			memcpy(m_rwiQueue, pqueue, (m_rwiQueueHead + 1) * sizeof(SRwiRequest));
			memcpy(m_rwiQueue + m_rwiQueueHead + 65, pqueue + m_rwiQueueHead + 1,
			       (m_rwiQueueSz - m_rwiQueueHead - 1) * sizeof(SRwiRequest));
			if (m_rwiQueueTail)
			{
				m_rwiQueueTail += 64;
			}
			m_rwiQueueAlloc += 64;
			if (pqueue)
			{
				delete[] pqueue;
			}
		}
		m_rwiQueueHead = m_rwiQueueHead + 1 - (m_rwiQueueAlloc & (m_rwiQueueAlloc - 2 - m_rwiQueueHead) >> 31);
		m_rwiQueue[m_rwiQueueHead].pForeignData = pForeignData;
		m_rwiQueue[m_rwiQueueHead].iForeignData = iForeignData;
		m_rwiQueue[m_rwiQueueHead].org = org;
		m_rwiQueue[m_rwiQueueHead].dir = dir;
		m_rwiQueue[m_rwiQueueHead].objtypes = objtypes;
		m_rwiQueue[m_rwiQueueHead].flags = flags & ~rwi_queue;
		m_rwiQueue[m_rwiQueueHead].phitLast = phitLast;
		m_rwiQueue[m_rwiQueueHead].iCaller = iCaller;
		if (!(m_rwiQueue[m_rwiQueueHead].hits = hits))
		{
			WriteLock lockH(m_lockRwiHitsPool);
			int nhits = 0;
			ray_hit *phit = m_pRwiHitsTail->next, *pchunk = 0;
			if (m_rwiPoolEmpty || phit->next != m_pRwiHitsHead)
			{
				for (nhits = 1; nhits < nMaxHits && (m_rwiPoolEmpty || phit->next != m_pRwiHitsHead);
				     nhits++, phit = phit->next, m_rwiPoolEmpty = 0)
				{
					if (phit->next != phit + 1)
					{
						pchunk = phit, nhits = 0;
					}
				}
			}
			if (nhits < nMaxHits)
			{
				if (!pchunk)
				{
					for (pchunk = m_pRwiHitsHead; pchunk->next == pchunk + 1; pchunk++)
						;
				}
				phit = new ray_hit[nhits = max(nMaxHits, 512)];
				for (i = 0; i < nhits - 1; i++)
				{
					phit[i].next = phit + i + 1;
				}
				phit[nhits - 1].next = pchunk->next;
				pchunk->next = phit;
				m_rwiHitsPoolSize += nhits;
			}
			else
			{
				phit = (pchunk ? pchunk : m_pRwiHitsTail)->next;
			}
			m_pRwiHitsTail = phit + nMaxHits - 1;
			m_rwiPoolEmpty = 0;
			m_rwiQueue[m_rwiQueueHead].hits = phit;
			m_rwiQueue[m_rwiQueueHead].iCaller |= 1 << 16;
		}
		m_rwiQueue[m_rwiQueueHead].nMaxHits = nMaxHits;
		m_rwiQueue[m_rwiQueueHead].nSkipEnts =
		    min(sizeof(m_rwiQueue[0].idSkipEnts) / sizeof(m_rwiQueue[0].idSkipEnts[0]), nSkipEnts);
		for (i = 0; i < m_rwiQueue[m_rwiQueueHead].nSkipEnts; i++)
		{
			m_rwiQueue[m_rwiQueueHead].idSkipEnts[i] =
			    pSkipEnts[i] ? GetPhysicalEntityId(pSkipEnts[i]) : -3;
		}
		m_rwiQueueSz++;
		return 1;
	}

	FUNCTION_PROFILER(GetISystem(), PROFILE_PHYSICS);

	int i, nHits;
	for (i = 0; i < nMaxHits; i++)
	{
		hits[i].dist = 1E10;
		hits[i].bTerrain = 0;
		hits[i].pCollider = 0;
	}
	ray_hit* ptrailer = hits[nMaxHits - 1].next;
	entity_grid_checker egc;
	egc.nThroughHits = egc.nThroughHitsAux = 0;
	if (phitLast)
	{
		m_pentLastHit[iCaller] = (CPhysicalEntity*)phitLast->pCollider;
		m_ipartLastHit[iCaller] = phitLast->ipart;
		m_inodeLastHit[iCaller] = phitLast->iNode;
	}

	if ((objtypes & ent_terrain) && m_pHeightfield[iCaller])
	{
		geom_world_data gwd;
		intersection_params ip;
		geom_contact* pcontacts;
		CRayGeom aray(org, dir);
		gwd.R = m_HeightfieldBasis.T();
		gwd.offset = m_HeightfieldOrigin;
		if (m_pHeightfield[iCaller]->m_parts[0].pPhysGeom->pGeom->Intersect(&aray, &gwd, 0, &ip, pcontacts))
		{
			WriteLockCond lockColl(*ip.plock, 0);
			lockColl.SetActive();
			if (pcontacts->id[0] >= 0 || flags & rwi_ignore_terrain_holes)
			{
				dir = pcontacts->pt - org;
				hits[0].dist = pcontacts->t;
				hits[0].pCollider = m_pHeightfield[iCaller];
				hits[0].partid = hits[0].ipart = 0;
				hits[0].surface_idx = m_pHeightfield[iCaller]->GetMatId(pcontacts->id[0], 0);
				hits[0].idmatOrg = pcontacts->id[0];
				hits[0].pt = pcontacts->pt;
				hits[0].n = pcontacts->n;
				hits[0].bTerrain = 1;
			}
		}
	}
	egc.aray.CreateRay(org, dir);

	if (objtypes & m_bCheckWaterHits & ent_water)
	{
		ReadLock lockAr(m_lockAreas);
		quotientf t(1, 0);
		CPhysArea *pArea, *pHitArea{};
		ray_hit whit;
		Vec3 n = m_pGlobalArea->m_pb.waterPlane.n;
		primitives::box bbox;
		float l = egc.aray.m_ray.dir * egc.aray.m_dirn;
		bbox.Basis.SetIdentity();
		bbox.bOriented = 0;
		if (m_pGlobalArea->RayTrace(org, dir, &whit))
		{
			t.set(whit.dist, l), n = whit.n, pHitArea = m_pGlobalArea;
		}

		for (pArea = m_pGlobalArea->m_nextBig; pArea; pArea = pArea->m_nextBig)
		{
			if (pArea->m_pb.iMedium == 0)
			{
				bbox.center = (pArea->m_BBox[0] + pArea->m_BBox[1]) * 0.5f;
				bbox.size = (pArea->m_BBox[1] - pArea->m_BBox[0]) * 0.5f;
				if (box_ray_overlap_check(&bbox, &egc.aray.m_ray, nullptr) &&
				    pArea->RayTrace(org, dir, &whit) && whit.dist * sqr(t.y) <= t.x * t.y * l)
				{
					t.set(whit.dist, l);
					n = whit.n, pHitArea = pArea;
				}
			}
		}

		if (inrange(t.x, 0.0f, t.y))
		{
			if ((flags & rwi_pierceability_mask) < (m_SurfaceFlagsTable[m_matWater] & sf_pierceable_mask) +
			                                           ((int)flags & rwi_force_pierceable_noncoll))
			{
				if (nMaxHits <= 1)
				{
					goto nowater;
				}
				if ((m_SurfaceFlagsTable[m_matWater] | (flags ^ rwi_separate_important_hits)) &
				    sf_important)
				{
					i = egc.nThroughHits = 1;
				}
				else
				{
					i = nMaxHits - (egc.nThroughHitsAux = 1);
				}
			}
			else
			{
				i = 0;
			}
			hits[i].dist = (t.x = t.val()) * dir.len();
			hits[i].pCollider = pHitArea;
			hits[i].partid = hits[0].ipart = 0;
			hits[i].surface_idx = m_matWater;
			hits[i].idmatOrg = 0;
			hits[i].pt = org + dir * t.x;
			hits[i].n = n;
			hits[i].bTerrain = 0;
		nowater:;
		}
		objtypes |= ent_areas;
	}

	if (objtypes & ~(ent_terrain | ent_water))
	{
		WriteLock lock(m_lockCaller[iCaller]);

		for (i = 0; i < nSkipEnts; i++)
		{
			if (pSkipEnts[i])
			{
				if (!(((CPhysicalPlaceholder**)pSkipEnts)[i]->m_bProcessed >> iCaller & 1))
				{
					AtomicAdd(&((CPhysicalPlaceholder**)pSkipEnts)[i]->m_bProcessed, 1 << iCaller);
				}
				if (((CPhysicalPlaceholder**)pSkipEnts)[i]->m_pEntBuddy &&
				    !(((CPhysicalPlaceholder**)pSkipEnts)[i]->m_pEntBuddy->m_bProcessed >> iCaller & 1))
				{
					AtomicAdd(&((CPhysicalPlaceholder**)pSkipEnts)[i]->m_pEntBuddy->m_bProcessed,
					          1 << iCaller);
				}
			}
		}

		egc.phits = hits;
		egc.pWorld = this;
		egc.objtypes = objtypes;
		egc.flags = flags ^ rwi_separate_important_hits;
		if (!(egc.flagsColliderAll = flags >> rwi_colltype_bit))
		{
			egc.flagsColliderAll = geom_colltype_ray;
		}
		if (flags & rwi_ignore_noncolliding)
		{
			egc.flagsColliderAll |= geom_colltype0;
		}
		if (flags & rwi_colltype_any)
		{
			egc.flagsColliderAny = egc.flagsColliderAll;
			egc.flagsColliderAll = 0;
		}
		else
		{
			egc.flagsColliderAny = geom_collides;
		}
		egc.bUsePhysOnDemand = iszero((objtypes & (ent_static | ent_no_ondemand_activation)) - ent_static);
		egc.nMaxHits = nMaxHits;
		egc.pSkipForeignData =
		    (nSkipEnts > 0 && pSkipEnts[0])
			? pSkipEnts[0]->GetForeignData(egc.iSkipForeignData = pSkipEnts[0]->GetiForeignData())
			: 0;
		egc.ip.bStopAtFirstTri = (flags & rwi_any_hit) != 0;
		CPhysicalEntity** pTmpEntLists[2] = {m_pTmpEntList, m_pTmpEntList2};
		egc.pTmpEntList = pTmpEntLists[iCaller];
		egc.iCaller = iCaller;
		m_prevGEAobjtypes |= (iCaller - 1) >> 31;

		Vec3 origin_grid = (org - m_entgrid.origin).GetPermutated(m_iEntAxisz),
		     dir_grid = dir.GetPermutated(m_iEntAxisz);
		egc.org2d.set(0.5f - (origin_grid.x * m_entgrid.stepr.x), 0.5f - (origin_grid.y * m_entgrid.stepr.y));
		egc.dir2d.set(dir_grid.x * m_entgrid.stepr.x, dir_grid.y * m_entgrid.stepr.y);
		egc.dir2d_len = len(egc.dir2d);
		egc.nEnts = 0;
		if (flags & rwi_reuse_last_hit && m_pentLastHit[iCaller])
		{
			egc.maxt = 1E20f;
			egc.thunkSubst.inext = 0;
			egc.thunkSubst.pent = m_pentLastHit[iCaller];
			egc.thunkSubst.iSimClass = m_pentLastHit[iCaller]->m_iSimClass;
			egc.pThunkSubst = &egc.thunkSubst;
			egc.ipartSubst = m_ipartLastHit[iCaller];
			egc.ipartMask = -1;
			egc.gwd.iStartNode = m_inodeLastHit[iCaller];
			egc.check_cell(vector2di(0, 0), i);
			if (hits[0].dist > 0)
			{
				dir = egc.aray.m_ray.dir = hits[0].pt - egc.aray.m_ray.origin;
				dir_grid = dir.GetPermutated(m_iEntAxisz);
				egc.dir2d.set(dir_grid.x * m_entgrid.stepr.x, dir_grid.y * m_entgrid.stepr.y);
				egc.dir2d_len = len(egc.dir2d);
			}
			egc.pThunkSubst = 0;
			egc.ipartSubst = egc.ipartMask = 0;
			egc.gwd.iStartNode = 0;
		}
		egc.maxt = (egc.dir2d_len * (egc.dir2d_len + sqrt2)) + 0.0001f;

		{
			ReadLock lock(m_lockGrid);
			if (fabsf((origin_grid.x * m_entgrid.stepr.x * 2) - m_entgrid.size.x) > m_entgrid.size.x ||
			    fabsf((origin_grid.y * m_entgrid.stepr.y * 2) - m_entgrid.size.y) > m_entgrid.size.y ||
			    fabsf(((origin_grid.x + dir_grid.x) * m_entgrid.stepr.x * 2) - m_entgrid.size.x) >
			        m_entgrid.size.x ||
			    fabsf(((origin_grid.y + dir_grid.y) * m_entgrid.stepr.y * 2) - m_entgrid.size.y) >
			        m_entgrid.size.y)
			{
				egc.check_cell(vector2di(-1, -1), i);
			}

			DrawRayOnGrid(&m_entgrid, origin_grid, dir_grid, egc);
		}
		for (i = 0; i < egc.nEnts; i++)
		{
			AtomicAdd(
			    &(egc.pTmpEntList[i]->m_pEntBuddy ? egc.pTmpEntList[i]->m_pEntBuddy : egc.pTmpEntList[i])
				 ->m_bProcessed,
			    -(1 << iCaller));
		}

		for (i = 0; i < nSkipEnts; i++)
		{
			if (pSkipEnts[i])
			{
				if (((CPhysicalPlaceholder**)pSkipEnts)[i]->m_bProcessed >> iCaller & 1)
				{
					AtomicAdd(&((CPhysicalPlaceholder**)pSkipEnts)[i]->m_bProcessed,
					          -(1 << iCaller));
				}
				if (((CPhysicalPlaceholder**)pSkipEnts)[i]->m_pEntBuddy &&
				    (((CPhysicalPlaceholder**)pSkipEnts)[i]->m_pEntBuddy->m_bProcessed >> iCaller & 1))
				{
					AtomicAdd(&((CPhysicalPlaceholder**)pSkipEnts)[i]->m_pEntBuddy->m_bProcessed,
					          -(1 << iCaller));
				}
			}
		}

		if (flags & rwi_separate_important_hits)
		{
			int j, idx[2];
			ray_hit thit;
			for (idx[0] = 1, idx[1] = nMaxHits - 1, i = 1;
			     idx[0] + nMaxHits - idx[1] - 2 < egc.nThroughHits + egc.nThroughHitsAux; i++)
			{
				j = isneg(hits[idx[1]].dist -
				          hits[idx[0]].dist);          // j = hits[1].dist<hits[0].dist ? 1:0;
				j |= isneg(egc.nThroughHits - idx[0]); // if (idx[0]>nThroughHits) j = 1;
				j &= (nMaxHits - egc.nThroughHitsAux - 1 - idx[1]) >> 31;
				hits[idx[j]].bTerrain = i;
				idx[j] += 1 - (j * 2);
			}
			for (i = egc.nThroughHits + 1; i < nMaxHits - egc.nThroughHitsAux; i++)
			{
				hits[i].bTerrain = nMaxHits + 1;
			}
			for (i = 1; i < nMaxHits;)
			{
				if (hits[i].bTerrain != i && hits[i].bTerrain < nMaxHits)
				{
					thit = hits[hits[i].bTerrain];
					hits[hits[i].bTerrain] = hits[i];
					hits[i] = thit;
				}
				else
				{
					i++;
				}
			}
		}
	}

	nHits = 0;
	if (hits[0].dist > 1E9f)
	{
		hits[0].dist = -1;
		hits[0].pt = org + dir;
	}
	else
	{
		CPhysicalEntity* pent = (CPhysicalEntity*)hits[0].pCollider;
		if (pent && pent->m_iSimClass != 5 && hits[0].ipart < pent->m_nParts)
		{
			hits[0].foreignIdx =
			    pent->m_parts[hits[0].ipart].pPhysGeom->pGeom->GetForeignIdx(hits[0].partid);
			hits[0].partid = pent->m_parts[hits[0].ipart].id;
			m_pentLastHit[iCaller] = pent;
			m_ipartLastHit[iCaller] = hits[0].ipart;
			m_inodeLastHit[iCaller] = egc.iSolidNode;
		}
		else
		{
			hits[0].foreignIdx = hits[0].partid = -1;
		}
		nHits++;
	}
	if (m_vars.iDrawHelpers & 64 && m_pRenderer)
	{
		int nTicks = 0;
		m_pRenderer->DrawLine(org, hits[0].pt, 7, 1 | (nTicks * 2));
	}
	for (i = 1; i < nMaxHits; i++)
	{
		if (hits[i].dist > 1E9f || hits[0].dist > 0 && hits[i].dist > hits[0].dist)
		{
			hits[i].dist = -1;
		}
		else
		{
			CPhysicalEntity* pent = (CPhysicalEntity*)hits[i].pCollider;
			if (pent && pent->m_iSimClass != 5 && hits[i].ipart < pent->m_nParts)
			{
				hits[i].foreignIdx =
				    pent->m_parts[hits[i].ipart].pPhysGeom->pGeom->GetForeignIdx(hits[i].partid);
				hits[i].partid = pent->m_parts[hits[i].ipart].id;
			}
			else
			{
				hits[i].foreignIdx = hits[i].partid = -1;
			}
			hits[i].bTerrain = 0;
			nHits++;
		}
		hits[i].next = hits + i + 1;
	}
	hits[0].next = hits + 1;
	hits[nMaxHits - 1].next = ptrailer;

	if (phitLast && flags & rwi_update_last_hit)
	{
		phitLast->pCollider = m_pentLastHit[iCaller];
		phitLast->ipart = m_ipartLastHit[iCaller];
		phitLast->iNode = m_inodeLastHit[iCaller];
	}

	return nHits;
}

int CPhysicalWorld::TracePendingRays(int bDoTracing)
{
	int i, nChex = 0;
	IPhysicalEntity* pSkipEnts[8];
	int iCaller = IsPhysicsThread() ? 0 : 1;
	WriteLock lockg(m_lockTPR);

	{
		SRwiRequest curreq;
		EventPhysRWIResult eprr;
		eprr.pEntity = &g_StaticPhysicalEntity;

		do
		{
			{
				ReadLock lock(m_lockRwiQueue);
				if (m_rwiQueueSz == 0)
				{
					break;
				}
				curreq = m_rwiQueue[m_rwiQueueTail];
				m_rwiQueueTail = m_rwiQueueTail + 1 -
				                 (m_rwiQueueAlloc & (m_rwiQueueAlloc - 2 - m_rwiQueueTail) >> 31);
				m_rwiQueueSz--;
				nChex++;
			}
			eprr.pForeignData = curreq.pForeignData;
			eprr.iForeignData = curreq.iForeignData;
			for (i = 0; i < curreq.nSkipEnts; i++)
			{
				pSkipEnts[i] = GetPhysicalEntityById(curreq.idSkipEnts[i]);
			}
			eprr.nHits =
			    bDoTracing ? RayWorldIntersection(curreq.org, curreq.dir, curreq.objtypes, curreq.flags,
			                                      curreq.hits, curreq.nMaxHits, pSkipEnts, i, 0, 0,
			                                      "RayWorldIntersection(queued)", curreq.phitLast, iCaller)
				       : 0;
			eprr.bHitsFromPool = curreq.iCaller >> 16;
			eprr.nMaxHits = curreq.nMaxHits;
			eprr.pHits = curreq.hits;
			OnEvent(0, &eprr);
		}
		while (true);
	}

	{
		SPwiRequest curreq;
		EventPhysPWIResult eppr;
		eppr.pEntity = &g_StaticPhysicalEntity;
		geom_contact* pcontact = 0;

		do
		{
			{
				ReadLock lock(m_lockPwiQueue);
				if (m_pwiQueueSz == 0)
				{
					break;
				}
				curreq = m_pwiQueue[m_pwiQueueTail];
				m_pwiQueueTail = m_pwiQueueTail + 1 -
				                 (m_pwiQueueAlloc & (m_pwiQueueAlloc - 2 - m_pwiQueueTail) >> 31);
				m_pwiQueueSz--;
				nChex++;
			}
			eppr.pForeignData = curreq.pForeignData;
			eppr.iForeignData = curreq.iForeignData;
			for (i = 0; i < curreq.nSkipEnts; i++)
			{
				pSkipEnts[i] = GetPhysicalEntityById(curreq.idSkipEnts[i]);
			}
			if ((eppr.dist = bDoTracing
			                     ? PrimitiveWorldIntersection(
						   curreq.itype, (primitives::primitive*)curreq.pprim, curreq.sweepDir,
						   curreq.entTypes, &pcontact, curreq.geomFlagsAll, curreq.geomFlagsAny,
						   0, 0, 0, pSkipEnts, i)
			                     : 0) &&
			    pcontact)
			{
				eppr.pt = pcontact->pt;
				eppr.n = pcontact->n;
				eppr.idxMat = pcontact->id[1];
			}
			OnEvent(0, &eppr);
		}
		while (true);
	}

	return nChex;
}

float CPhysicalWorld::IsAffectedByExplosion(IPhysicalEntity* pobj, Vec3* impulse)
{
	int i;
	CPhysicalEntity* pent = ((CPhysicalPlaceholder*)pobj)->GetEntityFast();
	for (i = 0; i < m_nExplVictims && m_pExplVictims[i] != pent; i++)
		;
	if (i < m_nExplVictims)
	{
		if (impulse)
		{
			*impulse = m_pExplVictimsImp[i];
		}
		return m_pExplVictimsFrac[i];
	}
	if (impulse)
	{
		impulse->zero();
	}
	return 0.0f;
}

int CPhysicalWorld::DeformPhysicalEntity(IPhysicalEntity* pient, const Vec3& ptHit, const Vec3& dirHit, float r,
                                         int flags)
{
	// craig - experimental fix: i think the random number in GetExplosionShape() is upsetting things in MP
	if (m_vars.bMultiplayer)
	{
		g_random_generator.seed(1234567);
	}

	int i, bEntChanged, bPartChanged;
	CPhysicalEntity* pent = (CPhysicalEntity*)pient;
	pe_explosion expl;
	geom_world_data gwd, gwd1;
	primitives::box bbox;
	Vec3 zaxWorld(0, 0, 1), zaxObj, zax;
	gwd1.offset = ptHit;
	(zaxWorld -= dirHit * (zaxWorld * dirHit)).normalize();
	expl.epicenter = expl.epicenterImp = ptHit;
	expl.impulsivePressureAtR = 0;
	expl.r = expl.rmin = expl.holeSize = r;
	if (flags & 2)
	{ // special values for explosion
		expl.explDir = dirHit;
		expl.impulsivePressureAtR = -1;
	}
	expl.iholeType = 0;

	{
		WriteLock lock(pent->m_lockUpdate);
		for (i = bEntChanged = 0; i < pent->m_nParts; i++)
		{
			if (pent->m_parts[i].flags & geom_colltype_explosion && pent->m_parts[i].idmatBreakable >= 0)
			{
				pent->m_parts[i].pPhysGeomProxy->pGeom->GetBBox(&bbox);
				zaxObj = pent->m_qrot *
				         (pent->m_parts[i].q * bbox.Basis.GetRow(idxmax3((float*)&bbox.size)));
				if (pent->m_iSimClass > 0 || fabs_tpl(zaxObj * zaxWorld) < 0.7f)
				{
					(zax = zaxObj - dirHit * (zaxObj * dirHit)).normalize();
				}
				else
				{
					zax = zaxWorld;
				}
				gwd1.R.SetColumn(0, dirHit ^ zax);
				gwd1.R.SetColumn(1, dirHit);
				gwd1.R.SetColumn(2, zax);
				gwd.R = Matrix33(pent->m_qrot * pent->m_parts[i].q);
				gwd.offset = pent->m_pos + pent->m_qrot * pent->m_parts[i].pos;
				gwd.scale = pent->m_parts[i].scale;
				bEntChanged += (bPartChanged = DeformEntityPart(pent, i, &expl, &gwd, &gwd1, 1));
				if (bPartChanged)
				{
					pent->m_parts[i].flags &= ~(flags >> 16 & 0xFFFF);
				}
			}
		}
	}
	if (bEntChanged && pent->UpdateStructure(0.01f, &expl, 1))
	{
		MarkEntityAsDeforming(pent);
	}

	return bEntChanged;
}

void CPhysicalWorld::ClonePhysGeomInEntity(CPhysicalEntity* pent, int i, IGeometry* pNewGeom)
{
	phys_geometry* pgeom;
	if (pNewGeom->GetType() == GEOM_TRIMESH && pent->m_parts[i].pLattice)
	{
		(pent->m_parts[i].pLattice = new CTetrLattice(pent->m_parts[i].pLattice, 1))
		    ->SetMesh((CTriMesh*)pNewGeom);
	}
	{
		WriteLock lock(m_lockGeoman);
		*(pgeom = GetFreeGeomSlot()) = *pent->m_parts[i].pPhysGeomProxy;
		pgeom->pGeom = pNewGeom;
		pgeom->nRefCount = 1;
		pgeom->surface_idx = 0;
		if (pgeom->pMatMapping)
		{
			memcpy(pgeom->pMatMapping = new int[pgeom->nMats], pent->m_parts[i].pPhysGeomProxy->pMatMapping,
			       pgeom->nMats * sizeof(int));
		}
	}
	if (pent->m_parts[i].pPhysGeom->pMatMapping == pent->m_parts[i].pMatMapping)
	{
		pent->m_parts[i].pMatMapping = pgeom->pMatMapping;
	}
	UnregisterGeometry(pent->m_parts[i].pPhysGeom);
	if (pent->m_parts[i].pPhysGeomProxy != pent->m_parts[i].pPhysGeom)
	{
		UnregisterGeometry(pent->m_parts[i].pPhysGeomProxy);
	}
	pent->m_parts[i].pPhysGeomProxy = pent->m_parts[i].pPhysGeom = pgeom;
	pent->m_parts[i].flags |= geom_can_modify;
}

void CPhysicalWorld::AllocRequestsQueue(int sz)
{
	if (m_nQueueSlotSize + sz + 1 > QUEUE_SLOT_SZ)
	{
		if (m_nQueueSlots == m_nQueueSlotsAlloc)
		{
			ReallocateList(m_pQueueSlots, m_nQueueSlots, m_nQueueSlotsAlloc += 8, true);
		}
		if (!m_pQueueSlots[m_nQueueSlots])
		{
			m_pQueueSlots[m_nQueueSlots] = new char[max(sz + 1, QUEUE_SLOT_SZ)];
		}
		++m_nQueueSlots;
		m_nQueueSlotSize = 0;
	}
}

void* CPhysicalWorld::QueueData(void* ptr, int sz)
{
	memcpy(m_pQueueSlots[m_nQueueSlots - 1] + m_nQueueSlotSize, ptr, sz);
	m_nQueueSlotSize += sz;
	*(int*)(m_pQueueSlots[m_nQueueSlots - 1] + m_nQueueSlotSize) = -1;
	return m_pQueueSlots[m_nQueueSlots - 1] + m_nQueueSlotSize - sz;
}

int CPhysicalWorld::DeformEntityPart(CPhysicalEntity* pent, int i, pe_explosion* pexpl, geom_world_data* gwd,
                                     geom_world_data* gwd1, int iSource)
{
	IGeometry *pGeom, *pHole;
	CTriMesh* pNewGeom = 0;
	EventPhysUpdateMesh epum;
	int bCreateConstraint = 0;
	epum.pEntity = pent;
	epum.pForeignData = pent->m_pForeignData;
	epum.iForeignData = pent->m_iForeignData;
	epum.partid = pent->m_parts[i].id;
	epum.iReason = iSource ? EventPhysUpdateMesh::ReasonRequest : EventPhysUpdateMesh::ReasonExplosion;
	epum.bInvalid = 0;
	if ((pent->m_parts[i].idmatBreakable >> 7 | pexpl->iholeType) != 0 &&
	    !(pent->m_parts[i].idmatBreakable & 128 << pexpl->iholeType))
	{
		return 0;
	}
	for (int j = 0; j < min(pent->m_nParts, 5); j++)
	{
		if (i != j && pent->m_parts[j].flags & geom_log_interactions &&
		    pent->m_parts[j].pPhysGeom->pGeom->PointInsideStatus(
			((pexpl->epicenter - pent->m_pos) * pent->m_qrot - pent->m_parts[j].pos) * pent->m_parts[j].q *
			(pent->m_parts[j].scale == 1.0f ? 1.0f : 1.0f / pent->m_parts[j].scale)))
		{
			return 0;
		}
	}

	pHole = GetExplosionShape(pexpl->holeSize, pent->m_parts[i].idmatBreakable, gwd1->scale, bCreateConstraint);
	if (pHole)
	{
		pGeom = pent->m_parts[i].pPhysGeomProxy->pGeom;
		if (!(pent->m_parts[i].flags & geom_can_modify))
		{
			if (pGeom->GetType() != GEOM_TRIMESH)
			{
				return 0;
			}
			(pNewGeom = new CTriMesh())->Clone((CTriMesh*)pGeom, 0);
			// pNewGeom->RebuildBVTree(((CTriMesh*)pGeom)->m_pTree);
			pGeom = pNewGeom;
		}
		else
		{
			primitives::box bbox, bbox1;
			pGeom->GetBBox(&bbox);
			pHole->GetBBox(&bbox1);
			float szmin0 = min(min(bbox.size.x, bbox.size.y), bbox.size.z);
			if (szmin0 > max(max(bbox.size.x, bbox.size.y), bbox.size.z) * 0.4f &&
			    szmin0 < max(max(bbox1.size.x, bbox1.size.y), bbox1.size.z) * 1.5f &&
			    ++((CTriMesh*)pGeom)->m_nMessyCutCount >= 5)
			{
				return 0;
			}
		}

		if (pGeom->Subtract(pHole, gwd, gwd1))
		{
			if (pNewGeom)
			{
				ClonePhysGeomInEntity(pent, i, pNewGeom);
			}
			if (pent->m_parts[i].pLattice)
			{
				pent->m_parts[i].pLattice->Subtract(pHole, gwd, gwd1);
			}
			(epum.pMesh = pGeom)->Lock();
			epum.pLastUpdate = (bop_meshupdate*)epum.pMesh->GetForeignData(DATA_MESHUPDATE);
			for (; epum.pLastUpdate && epum.pLastUpdate->next; epum.pLastUpdate = epum.pLastUpdate->next)
				;
			epum.pMesh->Unlock();
			OnEvent(m_vars.bLogStructureChanges + 1, &epum);
			pent->m_parts[i].flags |=
			    geom_structure_changes | (geom_constraint_on_break & -bCreateConstraint);
			return 1;
		}
		else if (pNewGeom)
		{
			pNewGeom->Release();
		}
	}
	return 0;
}

void CPhysicalWorld::MarkEntityAsDeforming(CPhysicalEntity* pent)
{
	if (!(pent->m_flags & pef_deforming))
	{
		WriteLock lock(m_lockDeformingEntsList);
		pent->m_flags |= pef_deforming;
		if (m_nDeformingEnts == m_nDeformingEntsAlloc)
		{
			ReallocateList(m_pDeformingEnts, m_nDeformingEnts, m_nDeformingEntsAlloc += 16);
		}
		m_pDeformingEnts[m_nDeformingEnts++] = pent;
	}
}

void CPhysicalWorld::SimulateExplosion(pe_explosion* pexpl, IPhysicalEntity** pSkipEnts, int nSkipEnts, int iTypes,
                                       int iCaller)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_PHYSICS);

	CPhysicalEntity** pents;
	int nents, nents1, i, j, bBreak, bEntChanged;
	RigidBody* pbody;
	float kr = pexpl->impulsivePressureAtR * sqr(pexpl->r), maxspeed = 15, E, frac = 1.0f, sumFrac, sumV, Minv;
	Vec3 gravity;
	pe_params_buoyancy pb;
	geom_world_data gwd, gwd1;
	primitives::box bboxPart, bbox;
	primitives::sphere sphExpl;
	CPhysicalPlaceholder** pSkipPcs = (CPhysicalPlaceholder**)pSkipEnts;
	pe_action_impulse shockwave;
	shockwave.iApplyTime = 2;
	shockwave.iSource = 2;
	EventPhysCollision epc;
	epc.pEntity[0] = &g_StaticPhysicalEntity;
	epc.pForeignData[0] = 0;
	epc.iForeignData[0] = 0;
	epc.vloc[1].zero();
	epc.mass[0] = 1E10f;
	epc.partid[0] = 0;
	epc.idmat[0] = 0;
	epc.penetration = epc.radius = 0;
	if (!CheckAreas(pexpl->epicenter, gravity, &pb, 1, Vec3(ZERO), 0, iCaller) || is_unused(gravity))
	{
		gravity = m_vars.gravity;
	}
	WriteLock lock(m_lockCaller[iCaller]);
	bboxPart.bOriented = 0;
	bboxPart.Basis.SetIdentity();
	sphExpl.center = pexpl->epicenter;
	sphExpl.r = pexpl->rmax;
	if (m_vars.bDebugExplosions)
	{
		m_vars.bSingleStepMode = 1;
	}

	for (i = 0; i < nSkipEnts; i++)
	{
		AtomicAdd(&((!pSkipPcs[i]->m_pEntBuddy || IsPlaceholder(pSkipPcs[i])) ? pSkipPcs[i]
		                                                                      : pSkipPcs[i]->m_pEntBuddy)
		               ->m_bProcessed,
		          1 << iCaller);
	}

	if (pexpl->holeSize > 0)
	{
		gwd1.R.SetRotationV0V1(Vec3(0, 1, 0), pexpl->explDir);
		gwd1.offset = pexpl->epicenter;
	}

	if (pexpl->nOccRes > 0)
	{
		if (pexpl->nOccRes > m_nOccRes)
		{
			for (i = 0; i < 6; i++)
			{
				if (m_nOccRes)
				{
					delete[] m_pGridStat[i];
					delete[] m_pGridDyn[i];
				}
				m_pGridStat[i] = new int[sqr(pexpl->nOccRes)];
				m_pGridDyn[i] = new int[sqr(pexpl->nOccRes)];
			}
		}
		for (i = 0; i < 6; i++)
		{
			for (j = sqr(pexpl->nOccRes) - 1; j >= 0; j--)
			{
				m_pGridStat[i][j] = (1u << 31) - 1;
			}
		}
		m_lastEpicenter = pexpl->epicenter;
		m_lastEpicenterImp = pexpl->epicenterImp;
		m_lastRmax = pexpl->rmax;
		m_lastExplDir = pexpl->explDir;
	}

	if (pexpl->nOccRes > 0 || pexpl->holeSize > 0)
	{
		for (nents =
		         GetEntitiesAround(pexpl->epicenter - Vec3(1, 1, 1) * pexpl->rmax,
		                           pexpl->epicenter + Vec3(1, 1, 1) * pexpl->rmax, pents,
		                           ent_terrain | ent_static | ent_sleeping_rigid | ent_rigid, 0, 0, iCaller) -
		         1;
		     nents >= 0; nents--)
		{
			if (pents[nents]->m_iSimClass < 1 || pents[nents]->GetMassInv() <= 0)
			{
				{
					WriteLock lock0(pents[nents]->m_lockUpdate);
					for (i = bEntChanged = 0; i < pents[nents]->m_nParts; i++)
					{
						if (pents[nents]->m_parts[i].flags & geom_colltype_explosion &&
						    (pents[nents]->m_nParts <= 1 ||
						     (bboxPart.center = (pents[nents]->m_parts[i].BBox[1] +
						                         pents[nents]->m_parts[i].BBox[0]) *
						                        0.5f,
						      bboxPart.size = (pents[nents]->m_parts[i].BBox[1] -
						                       pents[nents]->m_parts[i].BBox[0]) *
						                      0.5f,
						      box_sphere_overlap_check(&bboxPart, &sphExpl, nullptr))))
						{
							bBreak =
							    (m_vars.breakImpulseScale ||
							     pents[nents]->m_flags & pef_override_impulse_scale &&
							         !m_vars.bMultiplayer ||
							     pexpl->forceDeformEntities) &&
							    iTypes & 1 << pents[nents]->m_iSimClass &&
							    pexpl->holeSize > 0 &&
							    pents[nents]->m_parts[i].idmatBreakable >= 0 &&
							    !(pents[nents]->m_parts[i].flags & geom_manually_breakable);
							if (pexpl->nOccRes <= 0 && !bBreak)
							{
								continue;
							}
							gwd.R =
							    Matrix33(pents[nents]->m_qrot * pents[nents]->m_parts[i].q);
							gwd.offset =
							    pents[nents]->m_pos +
							    pents[nents]->m_qrot * pents[nents]->m_parts[i].pos -
							    pexpl->epicenter;
							gwd.scale = pents[nents]->m_parts[i].scale;
							if (pexpl->nOccRes > 0 && !(pents[nents]->m_parts[i].flags &
							                            geom_manually_breakable) ||
							    (pents[nents]->m_parts[i].pPhysGeomProxy->pGeom->GetBBox(
								 &bbox),
							     min(min(bbox.size.x, bbox.size.y), bbox.size.z) >
							         pexpl->rminOcc))
							{
								pents[nents]
								    ->m_parts[i]
								    .pPhysGeomProxy->pGeom->BuildOcclusionCubemap(
									&gwd, 0, m_pGridStat, m_pGridDyn,
									pexpl->nOccRes, pexpl->rminOcc, pexpl->rmax,
									pexpl->nGrow);
							}
							if (bBreak)
							{
								gwd.offset += pexpl->epicenter;
								bEntChanged += DeformEntityPart(pents[nents], i, pexpl,
								                                &gwd, &gwd1);
							}
						}
					}
				}
				if (bEntChanged && pents[nents]->UpdateStructure(0.01f, pexpl, -1, gravity))
				{
					MarkEntityAsDeforming(pents[nents]);
				}
			}
		}
	}
	if (pexpl->nOccRes >= 0)
	{
		m_nOccRes = pexpl->nOccRes;
	}

	nents = GetEntitiesAround(pexpl->epicenter - Vec3(1, 1, 1) * pexpl->rmax,
	                          pexpl->epicenter + Vec3(1, 1, 1) * pexpl->rmax, pents, iTypes, 0, 0, iCaller);
	if (pexpl->nOccRes < 0 && m_nOccRes >= 0)
	{
		// special case: reuse the previous m_pGridStat and process only entities that were not affected by the
		// previous call
		for (i = nents1 = 0; i < nents; i++)
		{
			for (j = 0; j < m_nExplVictims && m_pExplVictims[j] != pents[i]; j++)
				;
			if (j == m_nExplVictims)
			{
				pents[nents1++] = pents[i];
			}
		}
		pexpl->nOccRes = m_nOccRes;
		nents = nents1;
	}
	if (m_nExplVictimsAlloc < nents)
	{
		if (m_nExplVictimsAlloc)
		{
			delete[] m_pExplVictims;
			delete[] m_pExplVictimsFrac;
			delete[] m_pExplVictimsImp;
		}
		m_pExplVictims = new CPhysicalEntity*[m_nExplVictimsAlloc = nents];
		m_pExplVictimsFrac = new float[m_nExplVictimsAlloc];
		m_pExplVictimsImp = new Vec3[m_nExplVictimsAlloc];
	}

	for (nents--, m_nExplVictims = 0; nents >= 0; nents--)
	{
		{
			ReadLock lock0(pents[nents]->m_lockUpdate);
			m_pExplVictimsImp[m_nExplVictims].zero();
			for (i = bEntChanged = 0, sumFrac = sumV = 0.0f; i < pents[nents]->m_nParts; i++)
			{
				if ((pents[nents]->m_parts[i].flags & geom_colltype_explosion ||
				     pents[nents]->m_flags & pef_use_geom_callbacks) &&
				    (pents[nents]->m_nParts <= 1 ||
				     (bboxPart.center =
				          (pents[nents]->m_parts[i].BBox[1] + pents[nents]->m_parts[i].BBox[0]) * 0.5f,
				      bboxPart.size =
				          (pents[nents]->m_parts[i].BBox[1] - pents[nents]->m_parts[i].BBox[0]) * 0.5f,
				      box_sphere_overlap_check(&bboxPart, &sphExpl, nullptr))))
				{
					bBreak = (pents[nents]->m_iSimClass > 0 ||
					          pents[nents]->m_parts[i].flags & geom_monitor_contacts) &&
					         !(pents[nents]->m_parts[i].flags & geom_manually_breakable);

					if (bBreak || pents[nents]->m_parts[i].flags & geom_manually_breakable)
					{
						gwd.R = Matrix33(pents[nents]->m_qrot * pents[nents]->m_parts[i].q);
						gwd.offset = pents[nents]->m_pos +
						             pents[nents]->m_qrot * pents[nents]->m_parts[i].pos;
						gwd.scale = pents[nents]->m_parts[i].scale;

						if (pexpl->nOccRes > 0)
						{
							gwd.offset -= pexpl->epicenter;
							frac = pents[nents]
							           ->m_parts[i]
							           .pPhysGeomProxy->pGeom->BuildOcclusionCubemap(
								       &gwd, 1, m_pGridStat, m_pGridDyn, pexpl->nOccRes,
								       pexpl->rminOcc, m_lastRmax, pexpl->nGrow);
							gwd.offset += pexpl->epicenter;
							sumFrac += pents[nents]->m_parts[i].pPhysGeomProxy->V * frac;
							sumV += pents[nents]->m_parts[i].pPhysGeomProxy->V;
						}

						if (bBreak)
						{
							if (pexpl->holeSize > 0 &&
							    pents[nents]->m_parts[i].idmatBreakable >= 0)
							{
								bEntChanged += DeformEntityPart(pents[nents], i, pexpl,
								                                &gwd, &gwd1);
							}

							if (kr > 0)
							{
								if (!(pents[nents]->m_flags & pef_use_geom_callbacks))
								{
									shockwave.impulse.zero();
									shockwave.angImpulse.zero();
									pbody = pents[nents]->GetRigidBody(i);
									Minv = pents[nents]->GetMassInv();
									pents[nents]
									    ->m_parts[i]
									    .pPhysGeomProxy->pGeom
									    ->CalcVolumetricPressure(
										&gwd, pexpl->epicenterImp, kr,
										pexpl->rmin, pbody->pos,
										shockwave.impulse,
										shockwave.angImpulse);
									shockwave.impulse *= frac;
									shockwave.angImpulse *= frac;
									shockwave.ipart = i;
									if ((E = shockwave.impulse.len2() * sqr(Minv)) >
									    sqr(maxspeed))
									{
										shockwave.impulse *=
										    sqrt_tpl(sqr(maxspeed) / E);
									}
									if ((E = shockwave.angImpulse *
									         (pbody->Iinv * shockwave.angImpulse) *
									         Minv) > sqr(maxspeed))
									{
										shockwave.angImpulse *=
										    sqrt_tpl(sqr(maxspeed) / E);
									}
									pents[nents]->Action(&shockwave);
									m_pExplVictimsImp[m_nExplVictims] +=
									    shockwave.impulse;
								}
								else
								{
									pents[nents]->ApplyVolumetricPressure(
									    pexpl->epicenterImp, kr * frac,
									    pexpl->rmin);
								}
							}
						}
					}

					if ((pents[nents]->m_parts[i].flags &
					     (geom_manually_breakable | geom_structure_changes)) ==
					        geom_manually_breakable &&
					    (pexpl->nOccRes == 0 || sumFrac > 0))
					{
						int iprim, ifeat, ncont;
						primitives::box bbox;
						Vec3 ptdst[2];
						CBoxGeom boxGeom;
						intersection_params ip;
						geom_contact* pcontacts;
						float rscale;

						pents[nents]->m_parts[i].pPhysGeomProxy->pGeom->GetBBox(&bbox);
						boxGeom.CreateBox(&bbox);
						epc.n.zero();
						if (boxGeom.FindClosestPoint(&gwd, iprim, ifeat, pexpl->epicenter,
						                             pexpl->epicenter, ptdst, 1) < 0 ||
						    (pexpl->epicenter - ptdst[0]) *
						            (ptdst[0] - gwd.offset - gwd.R * bbox.center * gwd.scale) <
						        0)
						{
							ptdst[0] = pexpl->epicenter;
						}
						epc.mass[0] = (bbox.size.x * bbox.size.y) +
						              (bbox.size.x * bbox.size.z) + (bbox.size.y * bbox.size.z);
						if (pents[nents]->m_parts[i].idmatBreakable >= 0)
						{
							if ((ptdst[0] - ptdst[1]).len2() >
							    sqr((pexpl->rmin * 0.7f) + (pexpl->rmax * 0.3f)))
							{
								continue;
							}
							j = idxmax3((float*)&bbox.size);
							rscale = gwd.scale == 1.0f ? 1.0f : 1.0f / gwd.scale;
							ptdst[1].z = (bbox.Basis.GetRow(j) *
							              ((ptdst[0] - gwd.offset) * gwd.R - bbox.center)) *
							             rscale;
							ptdst[1].z = max(-bbox.size[j] * 0.8f,
							                 min(bbox.size[j] * 0.8f, ptdst[1].z));
							bbox.center += bbox.Basis.GetRow(j) * ptdst[1].z;
							bbox.size[inc_mod3[j]] *= 1.01f;
							bbox.size[dec_mod3[j]] *= 1.01f;
							bbox.size[j] *= 0.002f;
							boxGeom.CreateBox(&bbox);
							ncont =
							    pents[nents]->m_parts[i].pPhysGeomProxy->pGeom->Intersect(
								&boxGeom, 0, 0, &ip, pcontacts);
							if (ncont)
							{
								WriteLockCond lockColl(*ip.plock, 0);
								lockColl.SetActive();
								ptdst[0].Set(1E10f, 1E10f, 1E10f);
								ptdst[1] =
								    ((pexpl->epicenter - gwd.offset) * gwd.R) * rscale;
								for (ncont--; ncont >= 0; ncont--)
								{
									for (j = 0; j < pcontacts[ncont].nborderpt; j++)
									{
										if ((pcontacts[ncont].ptborder[j] -
										     ptdst[1])
										        .len2() <
										    (ptdst[0] - ptdst[1]).len2())
										{
											epc.n =
											    pents[nents]
												->m_parts[i]
												.pPhysGeomProxy->pGeom
												->GetNormal(
												    pcontacts[ncont]
													    .idxborder
														[j][0] &
													IDXMASK,
												    ptdst[0] =
													pcontacts[ncont]
													    .ptborder
														[j]);
										}
									}
								}
								ptdst[0] = gwd.R * ptdst[0] * gwd.scale + gwd.offset;
								epc.vloc[0] = -(epc.n = gwd.R * epc.n);
							}
						}
						epc.pt = ptdst[0];
						epc.pEntity[1] = pents[nents];
						epc.pForeignData[1] = pents[nents]->m_pForeignData;
						epc.iForeignData[1] = pents[nents]->m_iForeignData;
						if ((epc.pt - pexpl->epicenter).len2() < sqr(pexpl->holeSize * 0.2f))
						{
							epc.pt = pexpl->epicenter;
							if (!epc.n.len2())
							{
								epc.n = -(epc.vloc[0] = pexpl->explDir);
							}
						}
						else if (!epc.n.len2())
						{
							epc.vloc[0] =
							    -(epc.n = (pexpl->epicenter - epc.pt).normalized());
						}
						epc.vloc[0] *=
						    kr / max(sqr(pexpl->rmin), (pexpl->epicenter - epc.pt).len2());
						epc.vloc[0] *= epc.mass[0] * 100.0f;
						epc.mass[0] = 0.01f;
						epc.mass[1] = pents[nents]->GetMass(i);
						epc.partid[1] = pents[nents]->m_parts[i].id;
						epc.idmat[0] = -1;
						epc.normImpulse = epc.penetration = 0;
						epc.radius = pexpl->rmax;
						pents[nents]->m_parts[i].flags |= geom_will_be_destroyed;

						for (j = pents[nents]
						             ->m_parts[i]
						             .pPhysGeom->pGeom->GetPrimitiveCount() -
						         1;
						     j >= 0; j--)
						{
							epc.idmat[1] = pents[nents]->GetMatId(
							    pents[nents]->m_parts[i].pPhysGeom->pGeom->GetPrimitiveId(
								j, 0x40),
							    i);
							if (m_SurfaceFlagsTable[epc.idmat[1]] & sf_manually_breakable)
							{
								break;
							}
						}
						OnEvent(pef_log_collisions, &epc);
					}
				}
			}
		}
		if (pents[nents]->m_nParts == 0)
		{
			pents[nents]->ApplyVolumetricPressure(pexpl->epicenterImp, kr, pexpl->rmin);
		}

		m_pExplVictims[m_nExplVictims] = pents[nents];
		m_pExplVictimsFrac[m_nExplVictims++] = sumV > 0 ? sumFrac / sumV : 1.0f;
		if (bEntChanged && pents[nents]->UpdateStructure(0.01f, pexpl, -1, gravity))
		{
			MarkEntityAsDeforming(pents[nents]);
		}
	}
	pexpl->pAffectedEnts = (IPhysicalEntity**)m_pExplVictims;
	pexpl->pAffectedEntsExposure = m_pExplVictimsFrac;
	pexpl->nAffectedEnts = m_nExplVictims;

	for (i = 0; i < nSkipEnts; i++)
	{
		AtomicAdd(&((!pSkipPcs[i]->m_pEntBuddy || IsPlaceholder(pSkipPcs[i])) ? pSkipPcs[i]
		                                                                      : pSkipPcs[i]->m_pEntBuddy)
		               ->m_bProcessed,
		          -(1 << iCaller));
	}
}

float CPhysicalWorld::CalculateExplosionExposure(pe_explosion* pexpl, IPhysicalEntity* pient)
{
	if (pexpl->nOccRes <= 0)
	{
		return 1.0f;
	}
	if (pient->GetType() == PE_AREA)
	{
		return 0.0f;
	}

	CPhysicalEntity* pent = (CPhysicalEntity*)pient;
	ReadLock lock(pent->m_lockUpdate);
	int i;
	float sumV, sumFrac, frac;
	geom_world_data gwd;

	for (i = 0, sumFrac = sumV = 0.0f; i < pent->m_nParts; i++)
	{
		if (pent->m_parts[i].flags & geom_colltype_explosion)
		{
			gwd.R = Matrix33(pent->m_qrot * pent->m_parts[i].q);
			gwd.offset = pent->m_pos + pent->m_qrot * pent->m_parts[i].pos - pexpl->epicenter;
			gwd.scale = pent->m_parts[i].scale;
			frac = pent->m_parts[i].pPhysGeomProxy->pGeom->BuildOcclusionCubemap(
			    &gwd, 1, m_pGridStat, m_pGridDyn, pexpl->nOccRes, pexpl->rminOcc, m_lastRmax, pexpl->nGrow);
			sumFrac += pent->m_parts[i].pPhysGeomProxy->V * frac;
			sumV += pent->m_parts[i].pPhysGeomProxy->V;
		}
	}

	return sumV > 0 ? sumFrac / sumV : 1.0f;
}

void CPhysicalWorld::ResetDynamicEntities()
{
	int i;
	CPhysicalEntity* pent;
	WriteLock lock(m_lockStep);
	pe_action_reset reset;
	for (i = 1; i <= 4; i++)
	{
		for (pent = m_pTypedEnts[i]; pent; pent = pent->m_next)
		{
			pent->Action(&reset);
		}
	}
}

void CPhysicalWorld::DestroyDynamicEntities()
{
	int i;
	CPhysicalEntity *pent, *pent_next;

	m_nDynamicEntitiesDeleted = 0;
	for (i = 1; i <= 4; i++)
	{
		for (pent = m_pTypedEnts[i]; pent; pent = pent_next)
		{
			pent_next = pent->m_next;
			if (pent->m_pEntBuddy)
			{
				pent->m_pEntBuddy->m_pEntBuddy = 0;
				DestroyPhysicalEntity(pent->m_pEntBuddy);
			}
			else
			{
				SetPhysicalEntityId(pent, -1);
			}
			DetachEntityGridThunks(pent);
			pent->m_next = m_pTypedEnts[7];
			if (pent->m_next)
			{
				pent->m_next->m_prev = pent;
			}
			m_pTypedEnts[7] = pent;
			pent->m_iPrevSimClass = -1;
			pent->m_iSimClass = 7;
			m_nDynamicEntitiesDeleted++;
		}
		m_pTypedEnts[i] = m_pTypedEntsPerm[i] = 0;
	}

	m_nEnts -= m_nDynamicEntitiesDeleted;
	if (m_nEnts < m_nEntsAlloc - 8192 && !m_bEntityCountReserved)
	{
		int nEntsAlloc = m_nEntsAlloc;
		m_nEntsAlloc = (m_nEnts - 1 & ~8191) + 8192;
		m_nEntListAllocs++;
		ReallocateList(m_pTmpEntList, nEntsAlloc, m_nEntsAlloc);
		ReallocateList(m_pTmpEntList1, nEntsAlloc, m_nEntsAlloc);
		ReallocateList(m_pTmpEntList2, nEntsAlloc, m_nEntsAlloc);
		ReallocateList(m_pGroupMass, 0, m_nEntsAlloc);
		ReallocateList(m_pMassList, 0, m_nEntsAlloc);
		ReallocateList(m_pGroupIds, 0, m_nEntsAlloc);
		ReallocateList(m_pGroupNums, 0, m_nEntsAlloc);
	}
}

void CPhysicalWorld::PurgeDeletedEntities()
{
	int i, j;
	{
		WriteLock lock1(m_lockQueue);
		for (i = 0; i < m_nQueueSlots; i++)
		{
			for (j = 0; *(int*)(m_pQueueSlots[i] + j) != -1;
			     j += *(int*)(m_pQueueSlots[i] + j + sizeof(int)))
			{
				if ((*(CPhysicalEntity**)(m_pQueueSlots[i] + j + (sizeof(int) * 2)))->m_iSimClass == 7)
				{
					*(int*)(m_pQueueSlots[i] + j) = -1;
				}
			}
		}
	}

	{
		WriteLock lock3(m_lockDeformingEntsList);
		for (i = j = 0; i < m_nDeformingEnts; i++)
		{
			if (m_pDeformingEnts[i]->m_iSimClass != 7)
			{
				m_pDeformingEnts[j++] = m_pDeformingEnts[i];
			}
			else
			{
				m_pDeformingEnts[i]->m_flags &= ~pef_deforming;
			}
		}
		m_nDeformingEnts = j;
	}

	TracePendingRays(0);

	WriteLock lock(m_lockStep);
	CleanseEventsQueue();
	CPhysicalEntity *pent, *pent_next;
	for (pent = m_pTypedEnts[7]; pent; pent = pent_next)
	{
		pent_next = pent->m_next;
		if (pent->m_nRefCount <= 0)
		{
			if (pent->m_next)
			{
				pent->m_next->m_prev = pent->m_prev;
			}
			(pent->m_prev ? pent->m_prev->m_next : m_pTypedEnts[7]) = pent->m_next;
			delete pent;
		}
	}
}

void CPhysicalWorld::DrawPhysicsHelperInformation(IPhysRenderer* pRenderer, int iCaller)
{
	int entype;
	CPhysicalEntity* pent = 0;
	m_pRenderer = pRenderer;

	if (m_vars.iDrawHelpers)
	{
		int i, n = 0, nEntListAllocs, nGEA;
		CPhysicalEntity** pEntList;
		{
			WriteLock lock0(m_lockCaller[iCaller]);
			if (m_pHeightfield[iCaller] && m_vars.iDrawHelpers & 128)
			{
				pRenderer->DrawGeometry(m_pHeightfield[iCaller]->m_parts[0].pPhysGeom->pGeom, 0, 0);
			}
			nEntListAllocs = m_nEntListAllocs;
			nGEA = m_nGEA[iCaller];

			pEntList = iCaller ? m_pTmpEntList2 : m_pTmpEntList, *pent;
			{
				ReadLock lock(m_lockList);
				for (entype = 0; entype <= 6; entype++)
				{
					if (m_vars.iDrawHelpers & 0x100 << entype)
					{
						for (pent = m_pTypedEnts[entype];
						     pent && nEntListAllocs == m_nEntListAllocs &&
						     nGEA == m_nGEA[iCaller];
						     pent = pent->m_next)
						{
							pEntList[n++] = reinterpret_cast<CPhysicalEntity*>(
							    static_cast<size_t>(pent->m_id));
						}
					}
				}
				if (pent)
				{
					return;
				}
			}
		}
		for (i = 0; i < n; i++)
		{
			int id = *(int*)(pEntList + i);
			if (nEntListAllocs != m_nEntListAllocs || nGEA != m_nGEA[iCaller])
			{
				break;
			}
			if ((pent = (CPhysicalEntity*)GetPhysicalEntityById(id | 1 << 30)) &&
			    m_vars.iDrawHelpers & 0x80 << (pent->m_iSimClass + 1))
			{
				pent->DrawHelperInformation(pRenderer, m_vars.iDrawHelpers);
			}
		}
	}

	if (m_vars.iDrawHelpers & 8192 && m_nOccRes)
	{
		float zscale, xscale, xoffs, z;
		int i, ix, iy, cx, cy, cz;
		Vec3 pt0, pt1;
		zscale = m_lastRmax * (1.0 / 65535.0f);
		xscale = 2.0f / m_nOccRes;
		xoffs = 1.0f - xscale;
		for (i = 0; i < 6; i++)
		{
			cz = i >> 1;
			cx = inc_mod3[cz];
			cy = dec_mod3[cz];
			for (iy = 0; iy < m_nOccRes; iy++)
			{
				for (ix = 0; ix < m_nOccRes; ix++)
				{
					if (m_pGridStat[i][(iy * m_nOccRes) + ix] < (1u << 31) - 1)
					{
						pt0[cz] = (z = m_pGridStat[i][(iy * m_nOccRes) + ix] * zscale) *
						          ((i & 1) * 2 - 1);
						pt0[cx] = ((ix + 0.5f) * xscale - 1.0f) * z;
						pt0[cy] = ((iy + 0.5f) * xscale - 1.0f) * z;
						pt0 += m_lastEpicenter;
						pRenderer->DrawLine(m_lastEpicenter, pt0, 7);
						pt0[cx] -= z * xscale * 0.5f;
						pt0[cy] -= z * xscale * 0.5f;
						pt1 = pt0;
						pt1[cx] += z * xscale;
						pt1[cy] += z * xscale;
						pRenderer->DrawLine(pt0, pt1, 7);
						pt0[cy] += z * xscale;
						pt1[cy] -= z * xscale;
						pRenderer->DrawLine(pt0, pt1, 7);
					}
				}
			}
		}
	}

	if (m_vars.iDrawHelpers & 32)
	{
		ReadLock lock(m_lockAreas);
		for (CPhysArea* pArea = m_pGlobalArea; pArea; pArea = pArea->m_next)
		{
			pArea->DrawHelperInformation(pRenderer, m_vars.iDrawHelpers);
		}
	}

	if (m_vars.iDrawHelpers & 32768 && m_pWaterMan)
	{
		m_pWaterMan->DrawHelpers(pRenderer);
	}

	if (m_vars.bLogActiveObjects)
	{
		ReadLock lock(m_lockList);
		m_vars.bLogActiveObjects = 0;
		int i, nPrims, nCount = 0;
		RigidBody* pbody;
		for (pent = m_pTypedEnts[2]; pent; pent = pent->m_next)
		{
			if (pent->GetMassInv() > 0)
			{
				for (i = nPrims = 0; i < pent->m_nParts; i++)
				{
					if (pent->m_parts[i].flags & geom_colltype0)
					{
						nPrims += ((CGeometry*)pent->m_parts[i].pPhysGeomProxy->pGeom)
						              ->GetPrimitiveCount();
					}
				}
				pbody = pent->GetRigidBody();
				++nCount;
				CryLogAlways("%s @ %7.2f,%7.2f,%7.2f, mass %.2f, v %.1f, w %.1f, #polies %d, id %d",
				             m_pRenderer ? m_pRenderer->GetForeignName(pent->m_pForeignData,
				                                                       pent->m_iForeignData,
				                                                       pent->m_iForeignFlags)
				                         : "",
				             pent->m_pos.x, pent->m_pos.y, pent->m_pos.z, pbody->M, pbody->v.len(),
				             pbody->w.len(), nPrims, pent->m_id);
			}
		}
		CryLogAlways("%d active object(s)", nCount);
	}
}

int CPhysicalWorld::CollideEntityWithBeam(IPhysicalEntity* _pent, Vec3 org, Vec3 dir, float r, ray_hit* phit)
{
	if (!_pent)
	{
		return 0;
	}
	FUNCTION_PROFILER(GetISystem(), PROFILE_PHYSICS);

	CPhysicalEntity* pent = (CPhysicalEntity*)_pent;
	CSphereGeom SweptSph;
	geom_contact* pcontacts;
	geom_world_data gwd[2];
	primitives::sphere asph;
	asph.r = r;
	asph.center.zero();
	SweptSph.CreateSphere(&asph);
	intersection_params ip;
	ip.bSweepTest = dir.len2() > 0;
	gwd[0].R.SetIdentity();
	gwd[0].offset = org;
	gwd[0].v = dir;
	ip.time_interval = 1.0f;
	phit->dist = 1E10;

	for (int i = 0; i < pent->m_nParts; i++)
	{
		if (pent->m_parts[i].flags & geom_collides)
		{
			gwd[1].offset = pent->m_pos + pent->m_qrot * pent->m_parts[i].pos;
			gwd[1].R = Matrix33(pent->m_qrot * pent->m_parts[i].q);
			gwd[1].scale = pent->m_parts[i].scale;
			if (SweptSph.Intersect(pent->m_parts[i].pPhysGeom->pGeom, gwd, gwd + 1, &ip, pcontacts))
			{
				WriteLockCond lockColl(*ip.plock, 0);
				lockColl.SetActive();
				if (pcontacts->t < phit->dist)
				{
					phit->dist = pcontacts->t;
					phit->pCollider = pent;
					phit->partid = pent->m_parts[phit->ipart = i].id;
					phit->surface_idx = pent->GetMatId(pcontacts->id[1], i);
					phit->idmatOrg = pcontacts->id[1] +
					                 (pent->m_parts[i].surface_idx + 1 & pcontacts->id[1] >> 31);
					phit->foreignIdx =
					    pent->m_parts[i].pPhysGeom->pGeom->GetForeignIdx(pcontacts->iPrim[1]);
					phit->pt = pcontacts->pt;
					phit->n = -pcontacts->n;
				}
			}
		}
	}

	return isneg(phit->dist - 1E9f);
}

int CPhysicalWorld::CollideEntityWithPrimitive(IPhysicalEntity* _pent, int itype, primitives::primitive* pprim,
                                               Vec3 dir, ray_hit* phit)
{
	if (!_pent || ((CPhysicalPlaceholder*)_pent)->m_iSimClass == 5)
	{
		return 0;
	}
	FUNCTION_PROFILER(GetISystem(), PROFILE_PHYSICS);

	CPhysicalEntity* pent = (CPhysicalEntity*)_pent;
	ReadLock lock(pent->m_lockUpdate);
	geom_contact* pcontacts;
	geom_world_data gwd[2];
	CBoxGeom gbox;
	CCylinderGeom gcyl;
	CCapsuleGeom gcaps;
	CSphereGeom gsph;
	CGeometry* pgeom;
	intersection_params ip;
	ip.bSweepTest = dir.len2() > 0;
	gwd[0].R.SetIdentity();
	gwd[0].offset.zero();
	gwd[0].v = dir;
	ip.time_interval = 1.0f;
	phit->dist = 1E10;

	switch (itype)
	{
		case primitives::box::type:
			gwd[0].offset = ((primitives::box*)pprim)->center;
			((primitives::box*)pprim)->center.zero();
			gbox.CreateBox((primitives::box*)pprim);
			pgeom = &gbox;
			((primitives::box*)pprim)->center = gwd[0].offset;
			break;
		case primitives::cylinder::type:
			gwd[0].offset = ((primitives::cylinder*)pprim)->center;
			((primitives::cylinder*)pprim)->center.zero();
			gcyl.CreateCylinder((primitives::cylinder*)pprim);
			pgeom = &gcyl;
			((primitives::cylinder*)pprim)->center = gwd[0].offset;
			break;
		case primitives::capsule::type:
			gwd[0].offset = ((primitives::capsule*)pprim)->center;
			((primitives::capsule*)pprim)->center.zero();
			gcaps.CreateCapsule((primitives::capsule*)pprim);
			pgeom = &gcaps;
			((primitives::capsule*)pprim)->center = gwd[0].offset;
			break;
		case primitives::sphere::type:
			gwd[0].offset = ((primitives::sphere*)pprim)->center;
			((primitives::sphere*)pprim)->center.zero();
			gsph.CreateSphere((primitives::sphere*)pprim);
			pgeom = &gsph;
			((primitives::sphere*)pprim)->center = gwd[0].offset;
			break;
		default:
			return 0;
	}

	for (int i = 0; i < pent->m_nParts; i++)
	{
		if (pent->m_parts[i].flags & geom_collides)
		{
			gwd[1].offset = pent->m_pos + pent->m_qrot * pent->m_parts[i].pos;
			gwd[1].R = Matrix33(pent->m_qrot * pent->m_parts[i].q);
			gwd[1].scale = pent->m_parts[i].scale;
			if (pgeom->Intersect(pent->m_parts[i].pPhysGeom->pGeom, gwd, gwd + 1, &ip, pcontacts))
			{
				WriteLockCond lockColl(*ip.plock, 0);
				lockColl.SetActive();
				if (pcontacts->t < phit->dist)
				{
					phit->dist = pcontacts->t;
					phit->pCollider = pent;
					phit->partid = pent->m_parts[phit->ipart = i].id;
					phit->surface_idx = pent->GetMatId(pcontacts->id[1], i);
					phit->idmatOrg = pcontacts->id[1] +
					                 (pent->m_parts[i].surface_idx + 1 & pcontacts->id[1] >> 31);
					phit->foreignIdx =
					    pent->m_parts[i].pPhysGeom->pGeom->GetForeignIdx(pcontacts->iPrim[1]);
					phit->pt = pcontacts->pt;
					phit->n = -pcontacts->n;
				}
			}
		}
	}

	return isneg(phit->dist - 1E9f);
}

float CPhysicalWorld::PrimitiveWorldIntersection(int itype, primitives::primitive* pprim, const Vec3& sweepDir,
                                                 int entTypes, geom_contact** ppcontact, int geomFlagsAll,
                                                 int geomFlagsAny, intersection_params* pip, void* pForeignData,
                                                 int iForeignData, IPhysicalEntity** pSkipEnts, int nSkipEnts,
                                                 const char* pNameTag)
{
	int i, j, ncont, nents, iActive = 0;
	const int iCaller = 1;
	Vec3 BBox[2], sz;
	primitives::box bbox;
	CPhysicalEntity** pents;
	CBoxGeom gbox;
	CCylinderGeom gcyl;
	CCapsuleGeom gcaps;
	CSphereGeom gsph;
	CGeometry* pgeom;
	intersection_params ip;
	geom_world_data gwd[2];
	geom_contact* pcontacts;
	static geom_contact contactBest;
	contactBest.t = 0;

	if (entTypes & rwi_queue)
	{
		WriteLock lockQ(m_lockPwiQueue);
		if (ppcontact || pip)
		{
			return 0;
		}
		if (m_pwiQueueSz == m_pwiQueueAlloc)
		{
			SPwiRequest* pqueue = m_pwiQueue;
			m_pwiQueue = new SPwiRequest[m_pwiQueueAlloc + 64];
			memcpy(m_pwiQueue, pqueue, (m_pwiQueueHead + 1) * sizeof(SPwiRequest));
			memcpy(m_pwiQueue + m_pwiQueueTail + 64, pqueue + m_pwiQueueTail,
			       (m_pwiQueueSz - m_pwiQueueTail) * sizeof(SPwiRequest));
			m_pwiQueueAlloc += 64;
			m_pwiQueueTail += 64;
			if (pqueue)
			{
				delete[] pqueue;
			}
		}
		m_pwiQueueHead = m_pwiQueueHead + 1 - (m_pwiQueueAlloc & (m_pwiQueueAlloc - 2 - m_pwiQueueHead) >> 31);
		switch (m_pwiQueue[m_pwiQueueHead].itype = itype)
		{
			case primitives::box::type:
				*(primitives::box*)m_pwiQueue[m_pwiQueueHead].pprim = *(primitives::box*)pprim;
				break;
			case primitives::cylinder::type:
				*(primitives::cylinder*)m_pwiQueue[m_pwiQueueHead].pprim =
				    *(primitives::cylinder*)pprim;
				break;
			case primitives::capsule::type:
				*(primitives::capsule*)m_pwiQueue[m_pwiQueueHead].pprim = *(primitives::capsule*)pprim;
				break;
			case primitives::sphere::type:
				*(primitives::sphere*)m_pwiQueue[m_pwiQueueHead].pprim = *(primitives::sphere*)pprim;
				break;
			default:
				return 0;
		}
		m_pwiQueue[m_pwiQueueHead].sweepDir = sweepDir;
		m_pwiQueue[m_pwiQueueHead].entTypes = entTypes & ~rwi_queue;
		m_pwiQueue[m_pwiQueueHead].geomFlagsAll = geomFlagsAll;
		m_pwiQueue[m_pwiQueueHead].geomFlagsAny = geomFlagsAny;
		m_pwiQueue[m_pwiQueueHead].pForeignData = pForeignData;
		m_pwiQueue[m_pwiQueueHead].iForeignData = iForeignData;
		m_pwiQueue[m_pwiQueueHead].nSkipEnts =
		    min(sizeof(m_pwiQueue[0].idSkipEnts) / sizeof(m_pwiQueue[0].idSkipEnts[0]), nSkipEnts);
		for (i = 0; i < m_pwiQueue[m_pwiQueueHead].nSkipEnts; i++)
		{
			m_pwiQueue[m_pwiQueueHead].idSkipEnts[i] =
			    pSkipEnts[i] ? GetPhysicalEntityId(pSkipEnts[i]) : -3;
		}
		m_pwiQueueSz++;
		return 1;
	}

	FUNCTION_PROFILER(GetISystem(), PROFILE_PHYSICS);

	if (pip)
	{
		ip = *pip;
		if (!ip.bThreadSafe)
		{
			CrySpinLock(pip->plock = ip.plock = &g_idata[iCaller].lockIntersect, 0,
			            iActive = WRITE_LOCK_VAL);
		}
		ip.bThreadSafe = true;
	}
	else if (sweepDir.len2() > 0)
	{
		ip.bSweepTest = true;
		gwd[0].v = sweepDir;
		ip.time_interval = 1.0f;
		contactBest.t = 1E10f;
	}
	else
	{
		ip.bStopAtFirstTri = true;
		ip.bNoBorder = true;
		ip.bNoAreaContacts = true;
	}
	if (ppcontact)
	{
		*ppcontact = 0;
	}

	switch (itype)
	{
		case primitives::box::type:
			gwd[0].offset = ((primitives::box*)pprim)->center;
			((primitives::box*)pprim)->center.zero();
			gbox.CreateBox((primitives::box*)pprim);
			pgeom = &gbox;
			((primitives::box*)pprim)->center = gwd[0].offset;
			break;
		case primitives::cylinder::type:
			gwd[0].offset = ((primitives::cylinder*)pprim)->center;
			((primitives::cylinder*)pprim)->center.zero();
			gcyl.CreateCylinder((primitives::cylinder*)pprim);
			pgeom = &gcyl;
			((primitives::cylinder*)pprim)->center = gwd[0].offset;
			break;
		case primitives::capsule::type:
			gwd[0].offset = ((primitives::capsule*)pprim)->center;
			((primitives::capsule*)pprim)->center.zero();
			gcaps.CreateCapsule((primitives::capsule*)pprim);
			pgeom = &gcaps;
			((primitives::capsule*)pprim)->center = gwd[0].offset;
			break;
		case primitives::sphere::type:
			gwd[0].offset = ((primitives::sphere*)pprim)->center;
			((primitives::sphere*)pprim)->center.zero();
			gsph.CreateSphere((primitives::sphere*)pprim);
			pgeom = &gsph;
			((primitives::sphere*)pprim)->center = gwd[0].offset;
			break;
		default:
			return 0;
	}
	pgeom->GetBBox(&bbox);
	sz = bbox.size * bbox.Basis.Fabs();
	BBox[0] = gwd[0].offset + bbox.center - sz;
	BBox[1] = gwd[0].offset + bbox.center + sz;
	for (i = 0; i < 3; i++)
	{
		BBox[0][i] += min(0.0f, sweepDir[i]), BBox[1][i] += max(0.0f, sweepDir[i]);
	}
	WriteLock lock(m_lockCaller[iCaller]);

	for (i = 0; i < nSkipEnts; i++)
	{
		if (pSkipEnts[i])
		{
			if (!(((CPhysicalPlaceholder**)pSkipEnts)[i]->m_bProcessed >> iCaller & 1))
			{
				AtomicAdd(&((CPhysicalPlaceholder**)pSkipEnts)[i]->m_bProcessed, 1 << iCaller);
			}
			if (((CPhysicalPlaceholder**)pSkipEnts)[i]->m_pEntBuddy &&
			    !(((CPhysicalPlaceholder**)pSkipEnts)[i]->m_pEntBuddy->m_bProcessed >> iCaller & 1))
			{
				AtomicAdd(&((CPhysicalPlaceholder**)pSkipEnts)[i]->m_pEntBuddy->m_bProcessed,
				          1 << iCaller);
			}
		}
	}

	nents = GetEntitiesAround(BBox[0], BBox[1], pents, entTypes, 0, 0, iCaller);
	for (i = 0; i < nents; i++)
	{
		ReadLock lockEnt(pents[i]->m_lockUpdate);
		for (j = 0; j < pents[i]->m_nParts; j++)
		{
			if ((pents[i]->m_parts[j].flags & geomFlagsAll) == static_cast<unsigned int>(geomFlagsAll) &&
			    (pents[i]->m_parts[j].flags & geomFlagsAny) &&
			    ((pents[i]->m_parts[j].BBox[1] - pents[i]->m_parts[j].BBox[0]).len2() == 0 ||
			     AABB_overlap(pents[i]->m_parts[j].BBox, BBox)))
			{
				gwd[1].offset = pents[i]->m_pos + pents[i]->m_qrot * pents[i]->m_parts[j].pos;
				gwd[1].R = Matrix33(pents[i]->m_qrot * pents[i]->m_parts[j].q);
				gwd[1].scale = pents[i]->m_parts[j].scale;
				ncont = pgeom->Intersect(pents[i]->m_parts[j].pPhysGeom->pGeom, gwd, gwd + 1, &ip,
				                         pcontacts);
				if (ncont)
				{
					WriteLockCond lockColl(*ip.plock, 0);
					lockColl.SetActive(ip.bThreadSafe ^ 1);
					for (int ic = 0; ic < ncont; ic++)
					{
						pcontacts[ic].iPrim[0] = pents[i]->m_id;
						pcontacts[ic].iPrim[1] = pents[i]->m_parts[j].id;
						pcontacts[ic].id[1] = pents[i]->GetMatId(pcontacts[ic].id[1], j);
					}
					if (ip.bStopAtFirstTri)
					{
						if (ppcontact)
						{
							*ppcontact = ip.pGlobalContacts;
						}
						return 1;
					}
					if (ip.bSweepTest)
					{
						if (pcontacts[0].t < contactBest.t)
						{
							contactBest = pcontacts[0];
						}
					}
					else
					{
						ip.bKeepPrevContacts = true, contactBest.t += ncont;
					}
				}
			}
		}
	}
	if (ppcontact)
	{
		*ppcontact = ip.bSweepTest ? &contactBest : ip.pGlobalContacts;
	}
	if (pip && contactBest.t == 0)
	{
		CryInterlockedAdd(&g_idata[iCaller].lockIntersect, -iActive);
	}

	if (m_vars.iDrawHelpers & 64 && m_pRenderer)
	{
		if (!ip.bSweepTest)
		{
			m_pRenderer->DrawGeometry(pgeom, gwd, 7, 1);
		}
		else if (contactBest.t < 1E10f)
		{
			m_pRenderer->DrawGeometry(pgeom, gwd, 7, 1, sz = gwd[0].v.normalized() * contactBest.t);
			gwd[0].offset += sz;
			m_pRenderer->DrawGeometry(pgeom, gwd, 7, 1, gwd[0].v - sz);
		}
		else
		{
			m_pRenderer->DrawGeometry(pgeom, gwd, 7, 1, gwd[0].v);
		}
	}

	for (i = 0; i < nSkipEnts; i++)
	{
		if (pSkipEnts[i])
		{
			if (((CPhysicalPlaceholder**)pSkipEnts)[i]->m_bProcessed >> iCaller & 1)
			{
				AtomicAdd(&((CPhysicalPlaceholder**)pSkipEnts)[i]->m_bProcessed, -(1 << iCaller));
			}
			if (((CPhysicalPlaceholder**)pSkipEnts)[i]->m_pEntBuddy &&
			    (((CPhysicalPlaceholder**)pSkipEnts)[i]->m_pEntBuddy->m_bProcessed >> iCaller & 1))
			{
				AtomicAdd(&((CPhysicalPlaceholder**)pSkipEnts)[i]->m_pEntBuddy->m_bProcessed,
				          -(1 << iCaller));
			}
		}
	}

	return contactBest.t < 1E10f ? contactBest.t : 0;
}

int CPhysicalWorld::RayTraceEntity(IPhysicalEntity* pient, Vec3 origin, Vec3 dir, ray_hit* pHit, pe_params_pos* pp)
{
	if (!(dir.len2() > 0 && origin.len2() >= 0))
	{
		return 0;
	}

	int i, ncont;
	Vec3 pos;
	quaternionf qrot;
	float scale = 1.0f;
	CRayGeom aray(origin, dir);
	geom_world_data gwd;
	geom_contact* pcontacts;
	intersection_params ip;
	pHit->dist = 1E10;

	if (((CPhysicalPlaceholder*)pient)->m_iSimClass != 5)
	{
		CPhysicalEntity* pent = ((CPhysicalPlaceholder*)pient)->GetEntity();
		if (pp)
		{
			pos = pp->pos;
			qrot = pp->q;
			if (!is_unused(pp->scale))
			{
				scale = pp->scale;
			}
			get_xqs_from_matrices(pp->pMtx3x4, pp->pMtx3x3, pos, qrot, scale);
		}
		else
		{
			pos = pent->m_pos;
			qrot = pent->m_qrot;
		}
		for (i = 0; i < pent->m_nParts; i++)
		{
			//(pent->m_qrot*pent->m_parts[i].q).getmatrix(gwd.R);	//Q2M_IVO
			gwd.R = Matrix33(qrot * pent->m_parts[i].q);
			gwd.offset = pos + qrot * pent->m_parts[i].pos;
			gwd.scale = scale * pent->m_parts[i].scale;
			ncont = pent->m_parts[i].pPhysGeom->pGeom->Intersect(&aray, &gwd, 0, &ip, pcontacts);
			WriteLockCond lockColl(*ip.plock, 0);
			lockColl.SetActive(isneg(-ncont));
			for (; ncont > 0 && pcontacts[ncont - 1].t < pHit->dist && pcontacts[ncont - 1].n * dir > 0;
			     ncont--)
				;
			if (ncont > 0)
			{
				pHit->dist = pcontacts[ncont - 1].t;
				pHit->pCollider = pent;
				pHit->partid = pent->m_parts[pHit->ipart = i].id;
				pHit->surface_idx = pent->GetMatId(pcontacts[ncont - 1].id[0], i);
				pHit->idmatOrg = pcontacts[ncont - 1].id[0] +
				                 (pent->m_parts[i].surface_idx + 1 & pcontacts[ncont - 1].id[0] >> 31);
				pHit->foreignIdx =
				    pent->m_parts[i].pPhysGeom->pGeom->GetForeignIdx(pcontacts[ncont - 1].iPrim[0]);
				pHit->pt = pcontacts[ncont - 1].pt;
				pHit->n = pcontacts[ncont - 1].n;
			}
		}
	}
	else
	{
		return ((CPhysArea*)pient)->RayTrace(origin, dir, pHit, pp);
	}

	return isneg(pHit->dist - 1E9);
}

CPhysicalEntity* CPhysicalWorld::CheckColliderListsIntegrity()
{
	int i, j, k;
	CPhysicalEntity* pent;
	for (i = 1; i <= 2; i++)
	{
		for (pent = m_pTypedEnts[i]; pent; pent = pent->m_next)
		{
			for (j = 0; j < pent->m_nColliders; j++)
			{
				if (pent->m_pColliders[j]->m_iSimClass > 0)
				{
					for (k = 0; k < pent->m_pColliders[j]->m_nColliders &&
					            pent->m_pColliders[j]->m_pColliders[k] != pent;
					     k++)
						;
					if (k == pent->m_pColliders[j]->m_nColliders)
					{
						return pent;
					}
				}
			}
		}
	}
	return 0;
}

int CPhysicalWorld::CoverPolygonWithCircles(strided_pointer<vector2df> pt, int npt, bool bConsecutive,
                                            const vector2df& center, vector2df*& centers, float*& radii,
                                            float minCircleRadius)
{
	return ::CoverPolygonWithCircles(pt, npt, bConsecutive, center, centers, radii, minCircleRadius);
}

void CPhysicalWorld::DeletePointer(void* pdata)
{
	if (pdata)
	{
		delete[] (char*)pdata;
	}
}

int CPhysicalWorld::qhull(strided_pointer<Vec3> pts, int npts, index_t*& pTris)
{
	return ::qhull(pts, npts, pTris);
}

void CPhysicalWorld::AddEntityProfileInfo(CPhysicalEntity* pent, int nTicks)
{
	if (m_nProfiledEnts == sizeof(m_pEntProfileData) / sizeof(m_pEntProfileData[0]) &&
	        nTicks <= m_pEntProfileData[m_nProfiledEnts - 1].nTicksStep ||
	    m_vars.bSingleStepMode)
	{
		return;
	}

	int i;
	phys_profile_info ppi;
	for (i = 0; i < m_nProfiledEnts && m_pEntProfileData[i].pEntity != pent; i++)
		;
	if (i == m_nProfiledEnts)
	{
		ppi.pEntity = pent;
		ppi.nTicks = ppi.nTicksStep = nTicks;
		ppi.nCalls = 1;
		ppi.nTicksPeak = ppi.nCallsPeak = ppi.nTicksAvg = ppi.peakAge = 0;
		ppi.nCallsAvg = 0;
		ppi.id = pent->m_id;
		ppi.pName = m_pRenderer ? m_pRenderer->GetForeignName(pent->m_pForeignData, pent->m_iForeignData,
		                                                      pent->m_iForeignFlags)
		                        : "noname";
	}
	else
	{
		ppi = m_pEntProfileData[i];
		ppi.nTicksStep &= -ppi.nTicks >> 31;
		ppi.nTicks += nTicks;
		ppi.nCalls++;
		nTicks = (ppi.nTicksStep = max(ppi.nTicksStep, nTicks));
		memmove(m_pEntProfileData + i, m_pEntProfileData + i + 1,
		        (--m_nProfiledEnts - i) * sizeof(m_pEntProfileData[0]));
	}

	int iBound[2] = {-1, m_nProfiledEnts};
	do
	{
		i = (iBound[0] + iBound[1]) >> 1;
		iBound[isneg(m_pEntProfileData[i].nTicksStep - nTicks)] = i;
	}
	while (iBound[1] > iBound[0] + 1);
	m_nProfiledEnts = min(m_nProfiledEnts + 1, sizeof(m_pEntProfileData) / sizeof(m_pEntProfileData[0]));
	if ((i = iBound[0] + 1) < m_nProfiledEnts)
	{
		memmove(m_pEntProfileData + i + 1, m_pEntProfileData + i,
		        (m_nProfiledEnts - 1 - i) * sizeof(m_pEntProfileData[0]));
		m_pEntProfileData[i] = ppi;
	}
}

void CPhysicalWorld::AddFuncProfileInfo(const char* name, int nTicks)
{
	WriteLock lock(m_lockFuncProfiler);
	int i, iBound[2] = {-1, m_nProfileFunx};
	if (m_nProfileFunx)
	{
		do
		{
			i = (iBound[0] + iBound[1]) >> 1;
			iBound[isneg((int)(name - m_pFuncProfileData[i].pName))] = i;
		}
		while (iBound[1] > iBound[0] + 1);
	}
	if ((i = iBound[0]) < 0 || m_pFuncProfileData[i].pName != name)
	{
		++i;
		if (m_nProfileFunx == m_nProfileFunxAlloc)
		{
			if (m_nProfileFunx >= 64)
			{
				return;
			}
			ReallocateList(m_pFuncProfileData, m_nProfileFunx, m_nProfileFunxAlloc += 16);
		}
		memmove(m_pFuncProfileData + i + 1, m_pFuncProfileData + i,
		        sizeof(phys_profile_info) * (m_nProfileFunx - i));
		m_pFuncProfileData[i].nTicks = m_pFuncProfileData[i].nTicksPeak = m_pFuncProfileData[i].peakAge = 0;
		m_pFuncProfileData[i].nCalls = m_pFuncProfileData[i].nCallsPeak = 0;
		m_pFuncProfileData[i].pName = name;
		m_nProfileFunx++;
	}
	m_pFuncProfileData[i].nTicks += nTicks;
	m_pFuncProfileData[i].nCalls++;
	m_pFuncProfileData[i].id = 0;
}

void CPhysicalWorld::AddEventClient(int type, int (*func)(const EventPhys*), int bLogged, float priority)
{
	EventClient *pSlot = new EventClient, *pCurSlot, *pSlot0 = m_pEventClients[type][bLogged];
	pSlot->priority = priority;
	pSlot->OnEvent = func;
	RemoveEventClient(type, func, bLogged);
	if (pSlot0 && pSlot0->priority > priority)
	{
		for (pCurSlot = m_pEventClients[type][bLogged]; pCurSlot->next && pCurSlot->next->priority > priority;
		     pCurSlot = pCurSlot->next)
			;
		pSlot->next = pCurSlot->next;
		pCurSlot->next = pSlot;
	}
	else
	{
		(m_pEventClients[type][bLogged] = pSlot)->next = pSlot0;
	}
}

int CPhysicalWorld::RemoveEventClient(int type, int (*func)(const EventPhys*), int bLogged)
{
	EventClient* pSlot = m_pEventClients[type][bLogged];
	if (!pSlot)
	{
		return 0;
	}
	if (pSlot->OnEvent == func)
	{
		m_pEventClients[type][bLogged] = pSlot->next;
		delete pSlot;
		return 1;
	}
	for (; pSlot->next && pSlot->next->OnEvent != func; pSlot = pSlot->next)
		;
	if (pSlot->next)
	{
		EventClient* pDelSlot = pSlot->next;
		pSlot->next = pSlot->next->next;
		delete pDelSlot;
		return 1;
	}
	return 0;
}

EventPhys* CPhysicalWorld::AllocEvent(int id, int sz)
{
	if (m_pFreeEvents[id] == 0)
	{
		if (m_szCurEventChunk + sz > EVENT_CHUNK_SZ)
		{
			EventChunk* pNewChunk = (EventChunk*)(new char[sizeof(EventChunk) + max(sz, EVENT_CHUNK_SZ)]);
			pNewChunk->next = 0;
			m_pCurEventChunk->next = pNewChunk;
			m_pCurEventChunk = pNewChunk;
			m_szCurEventChunk = 0;
		}
		m_pFreeEvents[id] = (EventPhys*)((char*)(m_pCurEventChunk + 1) + m_szCurEventChunk);
		m_szCurEventChunk += sz;
		m_pFreeEvents[id]->idval = id;
		m_pFreeEvents[id]->next = 0;
	}
	EventPhys* pSlot = m_pFreeEvents[id];
	m_pFreeEvents[id] = m_pFreeEvents[id]->next;
	m_nEvents[id]++;
	return pSlot;
}

void CPhysicalWorld::PumpLoggedEvents()
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_PHYSICS);

	EventPhys *pEventFirst, *pEvent, *pEventLast, *pEvent_next;
	ray_hit* pLastPoolHit = nullptr;
	int lastPoolHitBlockSize = 0;
	{
		WriteLock lock(m_lockEventsQueue);
		pEventFirst = m_pEventFirst;
		m_pEventFirst = m_pEventLast = 0;
		for (int i = 0; i < EVENT_TYPES_NUM; i++)
		{
			m_nEvents[i] = 0;
		}
	}
	m_iLastLogPump++;

	EventClient* pClient;
	for (pEvent = pEventFirst; pEvent; pEvent = pEvent->next)
	{
		if (!(pEvent->idval <= EventPhysCollision::id &&
		          (((CPhysicalEntity*)((EventPhysStereo*)pEvent)->pEntity[0])->m_iDeletionTime |
		           ((CPhysicalEntity*)((EventPhysStereo*)pEvent)->pEntity[1])->m_iDeletionTime) ||
		      pEvent->idval > EventPhysCollision::id &&
		          ((CPhysicalEntity*)((EventPhysMono*)pEvent)->pEntity)->m_iDeletionTime))
		{
			if (pEvent->idval == EventPhysPostStep::id)
			{
				EventPhysPostStep* pepps = (EventPhysPostStep*)pEvent;
				if (pepps->idStep == m_idStep)
				{
					break;
				}
				else if (pepps->idStep < m_idStep - 1 &&
				             ((CPhysicalPlaceholder*)pepps->pEntity)->m_iSimClass > 1 ||
				         ((CPhysicalPlaceholder*)pepps->pEntity)->m_bProcessed & PENT_SETPOSED ||
				         ((CPhysicalEntity*)pepps->pEntity)->m_iDeletionTime)
				{
					continue;
				}
			}
			for (pClient = m_pEventClients[pEvent->idval][1];
			     pClient && pClient->OnEvent(pEvent) + iszero(pEvent->idval - EventPhysRWIResult::id) +
			                    +iszero(pEvent->idval - EventPhysPWIResult::id);
			     pClient = pClient->next)
				;
			if (pEvent->idval == EventPhysRWIResult::id && ((EventPhysRWIResult*)pEvent)->bHitsFromPool)
			{
				pLastPoolHit = ((EventPhysRWIResult*)pEvent)->pHits +
				               (lastPoolHitBlockSize = ((EventPhysRWIResult*)pEvent)->nMaxHits) - 1;
			}
		}
	}
	pEventLast = pEvent;

	{
		WriteLock lock(m_lockEventsQueue);
		for (pEvent = pEventFirst; pEvent != pEventLast; pEvent = pEvent_next)
		{
			pEvent_next = pEvent->next;
			pEvent->next = m_pFreeEvents[pEvent->idval];
			m_pFreeEvents[pEvent->idval] = pEvent;
		}
		if (pEventLast)
		{
			CPhysicalEntity* pent = nullptr;
			for (pEvent = pEventLast; pEvent; pEvent = pEvent->next)
			{
				if (pEvent->idval > EventPhysCollision::id)
				{
					pent = (CPhysicalEntity*)((EventPhysMono*)pEvent)->pEntity;
					pent->m_iDeletionTime += min(1, pent->m_iDeletionTime);
				}
				else
				{
					(CPhysicalEntity*)((EventPhysStereo*)pEvent)->pEntity[0];
					pent->m_iDeletionTime += min(1, pent->m_iDeletionTime);
					(CPhysicalEntity*)((EventPhysStereo*)pEvent)->pEntity[1];
					pent->m_iDeletionTime += min(1, pent->m_iDeletionTime);
				}
			}
			for (pEvent = pEventLast; pEvent->next; pEvent = pEvent->next)
				;
			pEvent->next = m_pEventFirst;
			m_pEventFirst = pEventLast;
			if (!m_pEventLast)
			{
				m_pEventLast = pEvent;
			}
			//(m_pEventLast ? m_pEventLast->next : m_pEventFirst) = pEventLast;
			// for(m_pEventLast=pEventLast; m_pEventLast->next; m_pEventLast=m_pEventLast->next);
		}
	}
	if (pLastPoolHit)
	{
		WriteLock lockH(m_lockRwiHitsPool);
		int i;
		for (i = 1; i < lastPoolHitBlockSize && pLastPoolHit[-i].next == pLastPoolHit - i + 1; i++)
			;
		if (i < lastPoolHitBlockSize) // || pLastPoolHit->next->next!=pLastPoolHit->next+1)
		{
			CryLog("Error: queued RWI hits pool corrupted");
		}
		else
		{
			m_pRwiHitsHead = pLastPoolHit->next;
			m_rwiPoolEmpty = m_pRwiHitsTail->next == m_pRwiHitsHead;
		}
	}

	{
		WriteLock lockfp(m_lockFuncProfiler);
		int i, j;
		for (i = j = 0; i < m_nProfileFunx; i++)
		{
			if (++m_pFuncProfileData[i].id < 20)
			{
				if (i != j)
				{
					m_pFuncProfileData[j] = m_pFuncProfileData[i];
				}
				++j;
			}
		}
		m_nProfileFunx = j;
	}
}

void CPhysicalWorld::ClearLoggedEvents()
{
	EventPhys *pEvent, *pEvent_next;
	WriteLock lock(m_lockEventsQueue);
	for (pEvent = m_pEventFirst; pEvent; pEvent = pEvent_next)
	{
		pEvent_next = pEvent->next;
		pEvent->next = m_pFreeEvents[pEvent->idval];
		m_pFreeEvents[pEvent->idval] = pEvent;
	}
	m_pEventFirst = m_pEventLast = 0;
	m_iLastLogPump = -1;
}

entity_contact* CPhysicalWorld::AllocContact()
{
	if (m_pFreeContact->next == m_pFreeContact)
	{
		entity_contact* pChunk = new entity_contact[64];
		for (int i = 0; i < 64; i++)
		{
			pChunk[i].next = pChunk + i + 1;
			pChunk[i].prev = pChunk + i - 1;
			pChunk[i].bChunkStart = 0;
		}
		pChunk[0].prev = pChunk[63].next = CONTACT_END(m_pFreeContact);
		(m_pFreeContact = pChunk)->bChunkStart = 1;
		m_nFreeContacts += 64;
		m_nContactsAlloc += 64;
	}
	entity_contact* pContact = m_pFreeContact;
	m_pFreeContact->next->prev = m_pFreeContact->prev;
	m_pFreeContact->prev->next = m_pFreeContact->next;
	pContact->next = pContact->prev = pContact;
	m_nFreeContacts--;
	return pContact;
}

void CPhysicalWorld::FreeContact(entity_contact* pContact)
{
	pContact->prev = m_pFreeContact->prev;
	pContact->next = m_pFreeContact;
	m_pFreeContact->prev = pContact;
	m_pFreeContact = pContact;
	m_nFreeContacts++;
}

float CPhysicalWorld::GetFriction(int imat0, int imat1, int bDynamic)
{
	float* pTable = (float*)(((intptr_t)m_FrictionTable & ~(intptr_t)-bDynamic) |
	                         ((intptr_t)m_DynFrictionTable & (intptr_t)-bDynamic));
	return max(0.0f, pTable[imat0 & (NSURFACETYPES - 1)] + pTable[imat1 & (NSURFACETYPES - 1)]) * 0.5f;
}

float CPhysicalWorld::GetBounciness(int imat0, int imat1)
{
	return (m_BouncinessTable[imat0 & (NSURFACETYPES - 1)] + m_BouncinessTable[imat1 & (NSURFACETYPES - 1)]) * 0.5f;
}

int CPhysicalWorld::SerializeWorld(const char* fname, int bSave)
{
	return 0;
}

int CPhysicalWorld::SerializeGeometries(const char* fname, int bSave)
{
	return 0;
}

int CPhysicalWorld::AddExplosionShape(IGeometry* pGeom, float size, int idmat, float probability)
{
	int i, j, bCreateConstraint = idmat >> 16 & 1;
	idmat &= 0xFFFF;
	for (i = 0; i < m_nExpl; i++)
	{
		if (m_pExpl[i].pGeom == pGeom && m_pExpl[i].idmat == idmat)
		{
			return -1;
		}
	}
	if (m_nExpl == m_nExplAlloc)
	{
		ReallocateList(m_pExpl, m_nExpl, m_nExplAlloc += 16);
	}

	for (i = 0; i < m_nExpl && m_pExpl[i].idmat <= idmat; i++)
		;
	memmove(m_pExpl + i + 1, m_pExpl + i, (m_nExpl - i) * sizeof(m_pExpl[0]));
	if (i > 0 && m_pExpl[i - 1].idmat == idmat)
	{
		m_pExpl[i].iFirstByMat = m_pExpl[i - 1].iFirstByMat;
		m_pExpl[i].nSameMat = m_pExpl[i - 1].nSameMat + 1;
		for (j = m_pExpl[i].iFirstByMat; j < m_pExpl[i].iFirstByMat + m_pExpl[i - 1].nSameMat; j++)
		{
			m_pExpl[j].nSameMat++;
		}
	}
	else
	{
		m_pExpl[i].iFirstByMat = i;
		m_pExpl[i].nSameMat = 1;
	}
	if (pGeom->GetType() == GEOM_TRIMESH)
	{
		mesh_data* pmd = (mesh_data*)pGeom->GetData();
		memset(pmd->pMats, 100, pmd->nTris);
	}

	m_pExpl[i].id = m_idExpl++;
	(m_pExpl[i].pGeom = pGeom)->AddRef();
	m_pExpl[i].size = size;
	m_pExpl[i].rsize = 1 / size;
	m_pExpl[i].idmat = idmat;
	m_pExpl[i].probability = probability;
	m_pExpl[i].bCreateConstraint = bCreateConstraint;
	m_nExpl++;

	return m_pExpl[i].id;
}

void CPhysicalWorld::RemoveExplosionShape(int id)
{
	int i, j;
	for (i = 0; i < m_nExpl && m_pExpl[i].id != id; i++)
		;
	if (i == m_nExpl)
	{
		return;
	}
	m_pExpl[i].pGeom->Release();
	for (j = m_pExpl[i].iFirstByMat; j < m_pExpl[i].iFirstByMat + m_pExpl[i].nSameMat; j++)
	{
		m_pExpl[j].nSameMat--;
	}
	for (; j < m_nExpl; j++)
	{
		m_pExpl[j].iFirstByMat--;
	}
	memmove(m_pExpl + i, m_pExpl + i + 1, (m_nExpl - 1 - i) * sizeof(m_pExpl[0]));
	m_nExpl--;
}

IGeometry* CPhysicalWorld::GetExplosionShape(float size, int idmat, float& scale, int& bCreateConstraint)
{
	int i, j, mask, ibound[2] = {-1, m_nExpl};
	float sum, probabilitySum, f;

	if (!m_nExpl || size <= 0)
	{
		return 0;
	}
	idmat &= 127;
	do
	{
		i = (ibound[0] + ibound[1]) >> 1;
		ibound[isneg(idmat - m_pExpl[i].idmat)] = i;
	}
	while (ibound[1] > ibound[0] + 1);
	if (ibound[0] < 0 || m_pExpl[ibound[0]].idmat != idmat)
	{
		return 0;
	}

	ibound[1] = m_pExpl[ibound[0]].iFirstByMat + m_pExpl[ibound[0]].nSameMat;
	j = ibound[0] = m_pExpl[ibound[0]].iFirstByMat;
	for (i = j + 1; i < ibound[1]; i++)
	{
		mask = -isneg(fabs_tpl(m_pExpl[i].size - size) - fabs_tpl(m_pExpl[j].size - size));
		j = (i & mask) | (j & ~mask);
	}

	for (i = ibound[0], probabilitySum = 0; i < ibound[1]; i++)
	{
		probabilitySum += m_pExpl[i].probability * iszero(m_pExpl[i].size - m_pExpl[j].size);
	}
	f = (physics_rand() + 1) * probabilitySum * (1.0f / (RAND_MAX + 1));
	for (i = ibound[0], sum = 0; i < ibound[1] && sum < f; i++)
	{
		sum += m_pExpl[i].probability * iszero(m_pExpl[i].size - m_pExpl[j].size);
	}

	scale = size * m_pExpl[i - 1].rsize;
	bCreateConstraint = m_pExpl[i - 1].bCreateConstraint;
	return m_pExpl[i - 1].pGeom;
}

int CPhysicalWorld::SetWaterManagerParams(pe_params* params)
{
	if (params->type == pe_params_waterman::type_id && !is_unused(((pe_params_waterman*)params)->posViewer))
	{
		m_posViewer = ((pe_params_waterman*)params)->posViewer;
	}
	if (!m_pWaterMan)
	{
		if ((params->type != pe_params_waterman::type_id) ||
		    is_unused(((pe_params_waterman*)params)->nExtraTiles))
		{
			return 0;
		}
		m_pWaterMan = new CWaterMan(this);
	}
	return m_pWaterMan->SetParams(params);
}

int CPhysicalWorld::GetWaterManagerParams(pe_params* params)
{
	return m_pWaterMan ? m_pWaterMan->GetParams(params) : 0;
}

int CPhysicalWorld::GetWatermanStatus(pe_status* status)
{
	return m_pWaterMan ? m_pWaterMan->GetStatus(status) : 0;
}

void CPhysicalWorld::DestroyWaterManager()
{
	if (m_pWaterMan)
	{
		delete m_pWaterMan;
		m_pWaterMan = 0;
	}
}

namespace {

void getEntityMassAndCom(CPhysicalEntity* pent, float& mass, Vec3& com)
{
	int i, j;
	for (j = i = 0, mass = 0; i < pent->m_nParts; i++)
	{
		j += (i - j) & -isneg(pent->m_parts[i].mass - pent->m_parts[j].mass);
		mass += pent->m_parts[i].mass;
	}
	if (pent->m_nParts)
	{
		com = pent->m_pos + pent->m_qrot * (pent->m_parts[j].pos + pent->m_parts[j].q *
		                                                               pent->m_parts[j].pPhysGeomProxy->origin *
		                                                               pent->m_parts[j].scale);
	}
	else
	{
		com.zero();
	}
}

} // unnamed namespace

void CPhysicalWorld::SavePhysicalEntityPtr(TSerialize ser, CPhysicalEntity* pent)
{
	ser.BeginGroup("entity_ptr");
	int i;
	float mass;
	Vec3 com;
	ser.Value("id", i = pent->m_id);
	ser.Value("simclass", i = pent->m_iSimClass);
	getEntityMassAndCom(pent, mass, com);
	ser.Value("com", com);
	ser.Value("mass", mass);
	ser.EndGroup();
}

CPhysicalEntity* CPhysicalWorld::LoadPhysicalEntityPtr(TSerialize ser)
{
	int i, iSimClass, iSimClass1;
	CPhysicalEntity *pent, **pents;
	float mass, mass1;
	Vec3 com, com1;
	ser.BeginGroup("entity_ptr");
	ser.Value("id", i);
	ser.Value("simclass", iSimClass);
	ser.Value("com", com);
	ser.Value("mass", mass);
	ser.EndGroup();
	iSimClass1 = (unsigned int)(iSimClass - 1) < 2u ? (iSimClass ^ 3) : iSimClass;

	pent = (CPhysicalEntity*)GetPhysicalEntityById(i);
	if (pent)
	{
		getEntityMassAndCom(pent, mass1, com1);
		if ((pent->m_iSimClass == iSimClass || pent->m_iSimClass == iSimClass1) &&
		    fabs_tpl(mass - mass1) <= fabs_tpl(min(mass, mass1)) * 0.01f && (com - com1).len2() < sqr(0.01f))
		{
			return pent;
		}
	}

	for (i = GetEntitiesAround(com - Vec3(0.1f), com + Vec3(0.1f), pents, 1 << iSimClass | 1 << iSimClass1) - 1;
	     i >= 0; i--)
	{
		getEntityMassAndCom(pents[i], mass1, com1);
		if (fabs_tpl(mass - mass1) <= fabs_tpl(min(mass, mass1)) * 0.01f && (com - com1).len2() < sqr(0.01f))
		{
			return pents[i];
		}
	}
	return 0;
}

bool CPhysicalWorld::IsPhysicsThread()
{
	return std::this_thread::get_id() == m_threadId.load(std::memory_order_relaxed);
}

bool CPhysicalWorld::IsPodThread()
{
	return std::this_thread::get_id() == m_podThreadId.load(std::memory_order_relaxed);
}

void CPhysicalWorld::SetPhysicsThreadId()
{
	const auto id = std::this_thread::get_id();
	m_threadId.store(id, std::memory_order_relaxed);
	g_physicsThreadId.store(id, std::memory_order_relaxed);
}

void CPhysicalWorld::SetPodThreadId()
{
	m_podThreadId.store(std::this_thread::get_id(), std::memory_order_relaxed);
}

void CPhysicalWorld::UnsetPodThreadId()
{
	m_podThreadId.store(std::thread::id{}, std::memory_order_relaxed);
}

void CPhysicalWorld::SerializeGarbageTypedSnapshot(TSerialize ser, int iSnapshotType, int flags)
{
	static CPhysicalEntity g_entStatic(0);
	static CRigidEntity g_entRigid(0);
	static CWheeledVehicleEntity g_entWheeled(0);
	static CLivingEntity g_entLiving(0);
	static CParticleEntity g_entParticle(0);
	static CArticulatedEntity g_entArticulated(0);
	static CRopeEntity g_entRope(0);
	static CSoftEntity g_entSoft(0);
	static CPhysicalEntity* g_pTypedEnts[] = {&g_entStatic,      &g_entStatic, &g_entRigid,
	                                          &g_entWheeled,     &g_entLiving, &g_entParticle,
	                                          &g_entArticulated, &g_entRope,   &g_entSoft};

	g_pTypedEnts[min(sizeof(g_pTypedEnts) / sizeof(g_pTypedEnts[0]), max(0, iSnapshotType))]->GetStateSnapshot(
	    ser, flags);
}

IPhysicalEntity* CPhysicalWorld::AddArea(Vec3* pt, int npt, float zmin, float zmax, const Vec3& pos,
                                         const quaternionf& q, float scale, const Vec3& normal, int* pTessIdx,
                                         int nTessTris, Vec3* pFlows)
{
	if (npt <= 0)
	{
		return 0;
	}
	WriteLock lock(m_lockAreas);
	if (!m_pGlobalArea)
	{
		m_pGlobalArea = new CPhysArea(this);
		m_pGlobalArea->m_gravity = m_vars.gravity;
	}

	CPhysArea* pArea = new CPhysArea(this);
	int i, j, k, iend;
	float *xstart, seglen, len, maxdist;
	Vec3 n, p0, p1, BBox[2], sz, center, nscrew, axisx;
	Matrix33 C;
	m_nAreas++;
	m_nTypeEnts[PE_AREA]++;

	len = seglen = (pt[0] - pt[npt - 1]).len();
	pArea->m_offset0 = (pt[0] + pt[npt - 1]) * seglen;
	for (i = 0; i < npt - 1; i++)
	{
		len += seglen = (pt[i + 1] - pt[i]).len();
		pArea->m_offset0 += (pt[i + 1] + pt[i]) * seglen;
	}
	pArea->m_offset0 /= len * 2;
	nscrew = pt[npt - 1] - pArea->m_offset0 ^ pt[0] - pArea->m_offset0;
	for (i = 0, C.SetZero(); i < npt - 1; i++)
	{
		p0 = pt[i] - pArea->m_offset0;
		p1 = pt[i + 1] - pArea->m_offset0;
		seglen = (p1 - p0).len();
		nscrew += p0 ^ p1;
		for (j = 0; j < 3; j++)
		{
			for (k = 0; k < 3; k++)
			{
				C(j, k) +=
				    seglen * (2 * (p0[j] * p0[k] + p1[j] * p1[k]) + p0[j] * p1[k] + p0[k] * p1[j]);
			}
		}
	}

	real eval[3];
	Vec3r Cbuf[3], eigenAxes[3];
	for (i = 0; i < 3; i++)
	{
		Cbuf[i] = C.GetRow(i);
	}
	matrix Cmtx(3, 3, mtx_symmetric, static_cast<double*>(Cbuf[0]));
	matrix eigenBasis(3, 3, 0, static_cast<double*>(eigenAxes[0]));
	Cmtx.jacobi_transformation(eigenBasis, eval, 0);
	if (normal.len2() == 0)
	{
		n = eigenAxes[idxmin3(eval)];
		n *= (i = sgnnz(n * nscrew));
		axisx = eigenAxes[idxmax3(eval)] * i;
	}
	else
	{
		n = normal;
		axisx = eigenAxes[idxmax3(eval)];
		(axisx -= n * (axisx * n)).normalize();
	}

	pArea->m_R0.SetColumn(2, n);
	pArea->m_R0.SetColumn(0, axisx);
	pArea->m_R0.SetColumn(1, n ^ axisx);
	pArea->m_pt = new vector2df[(pArea->m_npt = npt) + 1];
	pArea->m_rscale = 1 / (pArea->m_rscale = scale);
	BBox[0] = Vec3(VMAX);
	BBox[1] = Vec3(VMIN);
	for (i = 0, maxdist = 0; i < npt; i++)
	{
		p0 = (pt[i] - pArea->m_offset0) * pArea->m_R0;
		pArea->m_pt[i] = vector2df(p0);
		maxdist = max(maxdist, fabs_tpl(p0.z));
		BBox[0].x = min_safe(BBox[0].x, pArea->m_pt[i].x);
		BBox[0].y = min_safe(BBox[0].y, pArea->m_pt[i].y);
		BBox[1].x = max_safe(BBox[1].x, pArea->m_pt[i].x);
		BBox[1].y = max_safe(BBox[1].y, pArea->m_pt[i].y);
	}
	BBox[0].z = pArea->m_zlim[0] = zmin;
	BBox[1].z = pArea->m_zlim[1] = zmax;
	pArea->m_size0 = (BBox[1] - BBox[0]) * 0.5f;
	pArea->m_idxSort[0] = new int[npt];
	pArea->m_idxSort[1] = new int[npt];
	xstart = new float[npt];

	for (iend = 0; iend < 2; iend++)
	{
		for (i = 0; i < npt; i++)
		{
			pArea->m_idxSort[iend][i] = i;
			j = (i + 1) & (i - npt + 1) >> 31;
			k = isneg(pArea->m_pt[i].x - pArea->m_pt[j].x) ^ iend;
			xstart[i] = pArea->m_pt[(i & -k) | (j & ~-k)].x;
		}
		qsort(xstart, pArea->m_idxSort[iend], 0, npt - 1);
	}
	delete[] xstart;
	pArea->m_offset = pArea->m_offset0 + pos;
	pArea->m_R = Matrix33(q) * pArea->m_R0;
	memset(pArea->m_pMask = new unsigned int[((npt - 1) >> 5) + 1], 0, (((npt - 1) >> 5) + 1) * sizeof(int));

	sz = Matrix33(pArea->m_R).Fabs() * pArea->m_size0 * scale;
	center = pArea->m_offset + pArea->m_R * (BBox[0] + BBox[1]) * (pArea->m_scale * 0.5f);
	pArea->m_BBox[0] = center - sz;
	pArea->m_BBox[1] = center + sz;

	if (maxdist > 0.05f || pFlows)
	{
		int nTris, *pidx = new int[npt * 3];
		Vec3* pvtx = new Vec3[npt];
		MARK_UNUSED pArea->m_pt[npt].x;
		if (pTessIdx && nTessTris)
		{
			memcpy(pidx, pTessIdx, (nTris = nTessTris) * 3 * sizeof(int));
		}
		else
		{
			nTris = TriangulatePoly(pArea->m_pt, npt, pidx, npt * 3);
		}
		if (pFlows)
		{
			pArea->m_pFlows = new Vec3[npt];
		}
		for (i = 0; i < npt; i++)
		{
			pvtx[i] = (pt[i] - pArea->m_offset0) * pArea->m_R0;
			if (pFlows)
			{
				pArea->m_pFlows[i] = pFlows[i] * pArea->m_R0;
			}
		}
		pArea->m_pGeom = CreateMesh(pvtx, strided_pointer<unsigned short>((unsigned short*)pidx, sizeof(int)),
		                            0, 0, nTris, mesh_SingleBB | mesh_shared_vtx | mesh_shared_idx);
		if (pArea->m_pGeom)
		{
			((CTriMesh*)pArea->m_pGeom)->m_flags &= ~(mesh_shared_vtx | mesh_shared_idx);
		}
	}

	pArea->m_next = m_pGlobalArea->m_next;
	m_pGlobalArea->m_next = pArea;
	RepositionArea(pArea);

	return pArea;
}

IPhysicalEntity* CPhysicalWorld::AddArea(IGeometry* pGeom, const Vec3& pos, const quaternionf& q, float scale)
{
	WriteLock lock(m_lockAreas);
	if (!m_pGlobalArea)
	{
		m_pGlobalArea = new CPhysArea(this);
		m_pGlobalArea->m_gravity = m_vars.gravity;
	}

	CPhysArea* pArea = new CPhysArea(this);
	m_nAreas++;
	m_nTypeEnts[PE_AREA]++;
	pArea->m_pGeom = pGeom;
	pArea->m_offset = pos;
	pArea->m_R = Matrix33(q);
	pArea->m_rscale = 1.0f / (pArea->m_scale = scale);

	pArea->m_offset0.zero();
	pArea->m_R0.SetIdentity();

	primitives::box abox;
	Vec3 sz, center;
	pGeom->GetBBox(&abox);
	abox.Basis *= pArea->m_R.T();
	sz = (abox.size * abox.Basis.Fabs()) * scale;
	center = pos + q * abox.center * scale;
	pArea->m_BBox[0] = center - sz;
	pArea->m_BBox[1] = center + sz;

	pArea->m_next = m_pGlobalArea->m_next;
	m_pGlobalArea->m_next = pArea;
	RepositionArea(pArea);

	return pArea;
}

IPhysicalEntity* CPhysicalWorld::AddArea(Vec3* pt, int npt, float r, const Vec3& pos, const quaternionf& q, float scale)
{
	WriteLock lock(m_lockAreas);
	if (!m_pGlobalArea)
	{
		m_pGlobalArea = new CPhysArea(this);
		m_pGlobalArea->m_gravity = m_vars.gravity;
	}

	CPhysArea* pArea = new CPhysArea(this);
	m_nAreas++;
	m_nTypeEnts[PE_AREA]++;
	pArea->m_offset = pos;
	pArea->m_R = Matrix33(q);
	pArea->m_rscale = 1.0f / (pArea->m_scale = scale);
	pArea->m_offset0.zero();
	pArea->m_R0.SetIdentity();
	pArea->m_zlim[0] = r;

	pArea->m_ptSpline = new Vec3[pArea->m_npt = npt];
	pArea->m_BBox[0] = pArea->m_BBox[1] = q * pt[0] * scale + pos;
	for (int i = 0; i < npt; i++)
	{
		pArea->m_ptSpline[i] = pt[i];
		Vec3 ptw = q * pt[i] * scale + pos;
		pArea->m_BBox[0] = min(pArea->m_BBox[0], ptw);
		pArea->m_BBox[1] = max(pArea->m_BBox[1], ptw);
	}
	pArea->m_BBox[0] -= Vec3(r, r, r);
	pArea->m_BBox[1] += Vec3(r, r, r);
	pArea->m_damping = 1.0f;
	pArea->m_falloff0 = 0.8f;

	pArea->m_next = m_pGlobalArea->m_next;
	m_pGlobalArea->m_next = pArea;
	RepositionArea(pArea);

	return pArea;
}

IPhysicalEntity* CPhysicalWorld::AddGlobalArea()
{
	WriteLock lock(m_lockAreas);
	if (!m_pGlobalArea)
	{
		m_pGlobalArea = new CPhysArea(this);
		m_pGlobalArea->m_gravity = m_vars.gravity;

		EventPhysAreaChange epac;
		epac.pEntity = m_pGlobalArea;
		epac.boxAffected[0] = Vec3(-FLT_MAX);
		epac.boxAffected[1] = Vec3(FLT_MAX);
		SignalEvent(&epac, 0);
	}
	return m_pGlobalArea;
}

void CPhysicalWorld::RepositionArea(CPhysArea* pArea)
{
	int res;
	CPhysArea* pPrevArea;
	for (pPrevArea = m_pGlobalArea; pPrevArea && pPrevArea->m_nextBig != pArea; pPrevArea = pPrevArea->m_nextBig)
		;

	if ((res = RepositionEntity(pArea, 1)) != 0)
	{
		AtomicAdd(&m_lockGrid, -WRITE_LOCK_VAL);
		if (res != -1)
		{
			if (pPrevArea)
			{
				pPrevArea->m_nextBig = pArea->m_nextBig;
				m_nBigAreas--;
			}
			pArea->m_nextBig = 0;
		}
		else if (!pPrevArea)
		{
			pArea->m_nextBig = m_pGlobalArea->m_nextBig;
			m_pGlobalArea->m_nextBig = pArea;
			m_nBigAreas++;
		}
	}

	pArea->SignalEventAreaChange(this);
}

void CPhysicalWorld::RemoveArea(IPhysicalEntity* _pArea)
{
	WriteLock lock1(m_lockAreas), lock(m_lockGrid);
	CPhysArea *pPrevArea, *pArea = (CPhysArea*)_pArea;
	pArea->m_bDeleted = 2;
	for (pPrevArea = m_pGlobalArea; pPrevArea && pPrevArea->m_next != pArea; pPrevArea = pPrevArea->m_next)
		;
	if (pPrevArea)
	{
		pPrevArea->m_next = pArea->m_next;
	}
	for (pPrevArea = m_pGlobalArea; pPrevArea && pPrevArea->m_nextBig != pArea; pPrevArea = pPrevArea->m_nextBig)
		;
	if (pPrevArea)
	{
		pPrevArea->m_nextBig = pArea->m_nextBig;
		m_nBigAreas--;
	}

	pArea->SignalEventAreaChange(this);
	DetachEntityGridThunks(pArea);
	m_nAreas--;
	m_nTypeEnts[PE_AREA]--;
	pArea->m_next = m_pDeletedAreas;
	m_pDeletedAreas = pArea;
	m_prevGEAobjtypes = -1;
}

int CPhysicalWorld::CheckAreas(const Vec3& ptc, Vec3& gravity, pe_params_buoyancy* pb, int nMaxBuoys, const Vec3& vel,
                               IPhysicalEntity* pent, int iCaller)
{
	if (!m_pGlobalArea)
	{
		return 0;
	}
	if (m_vars.bMultithreaded)
	{
		iCaller = IsPhysicsThread() ? 0 : 1;
	}
	WriteLock lock0(m_lockCaller[iCaller]);
	ReadLock lock1(m_lockAreas);
	CPhysArea* pArea;
	CPhysicalEntity** pEnts;
	Vec3 gravityGlobal;
	MARK_UNUSED gravity, gravityGlobal;
	int iMedium0 = -1;
	int nBuoys = m_pGlobalArea->ApplyParams(ptc, gravityGlobal, vel, pb, 0, nMaxBuoys, iMedium0, pent);
	if (nBuoys)
	{
		iMedium0 = pb->iMedium;
	}

	if (m_nBigAreas == m_nAreas)
	{
		for (pArea = m_pGlobalArea->m_nextBig; pArea; pArea = pArea->m_nextBig)
		{
			if (!pArea->m_bDeleted && PtInAABB(pArea->m_BBox, ptc) && pArea->CheckPoint(ptc))
			{
				nBuoys += pArea->ApplyParams(ptc, gravity, vel, pb, nBuoys, nMaxBuoys, iMedium0, pent);
			}
		}
	}
	else
	{
		for (int i = GetEntitiesAround(ptc, ptc, pEnts, ent_areas, 0, 0, iCaller) - 1; i >= 0; i--)
		{
			if ((pArea = (CPhysArea*)pEnts[i])->CheckPoint(ptc))
			{
				nBuoys += pArea->ApplyParams(ptc, gravity, vel, pb, nBuoys, nMaxBuoys, iMedium0, pent);
			}
		}
	}
	if (is_unused(gravity))
	{
		gravity = gravityGlobal;
	}

	return nBuoys | (nBuoys - 1) >> 31;
}

int CPhysicalWorld::CheckAreas(CPhysicalEntity* pent, Vec3& gravity, pe_params_buoyancy* pb, int nMaxBuoys,
                               const Vec3& vel, int iCaller)
{
	if (!m_pGlobalArea || pent->m_flags & pef_ignore_areas)
	{
		return 0;
	}

	return CheckAreas((pent->m_BBox[0] + pent->m_BBox[1]) * 0.5f, gravity, pb, nMaxBuoys, vel, pent, iCaller);
}

void CPhysicalWorld::SetWaterMat(int imat)
{
	m_matWater = imat;
	m_bCheckWaterHits =
	    m_matWater >= 0 && m_pGlobalArea && !is_unused(m_pGlobalArea->m_pb.waterPlane.origin) ? ent_water : 0;
}

IPhysicalEntity* CPhysicalWorld::GetNextArea(IPhysicalEntity* pPrevArea)
{
	if (!pPrevArea)
	{
		if (!m_pGlobalArea)
		{
			return 0;
		}
		ReadLockCond lock(m_lockAreas, 1);
		lock.SetActive(0);
		return m_pGlobalArea;
	}
	else
	{
		CPhysArea* pNextArea;
		for (pNextArea = ((CPhysArea*)pPrevArea)->m_next; pNextArea && pNextArea->m_bDeleted;
		     pNextArea = (CPhysArea*)pNextArea->m_next)
			;
		if (!pNextArea)
		{
			CryInterlockedAdd(&m_lockAreas, -1);
			return 0;
		}
		else
		{
			return (IPhysicalEntity*)pNextArea;
		}
	}
}
