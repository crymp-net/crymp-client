#pragma once

#include <atomic>
#include <thread>

#include "CryCommon/CryPhysics/IPhysics.h"

#include "GeomManager.h"
#include "IntersectData.h"
#include "PhysicalPlaceholder.h"
#include "Utils.h"

constexpr int NSURFACETYPES = 512;
constexpr int PLACEHOLDER_CHUNK_SZLG2 = 8;
constexpr int PLACEHOLDER_CHUNK_SZ = 1 << PLACEHOLDER_CHUNK_SZLG2;
constexpr int QUEUE_SLOT_SZ = 8192;

constexpr int PENT_SETPOSED = 1 << 16;
constexpr int PENT_QUEUED_BIT = 17;
constexpr int PENT_QUEUED = 1 << PENT_QUEUED_BIT;

class CPhysArea;
struct entity_contact;
struct SExplosionShape;
struct SRwiRequest;
struct SPwiRequest;
struct pe_gridthunk;

struct EventClient
{
	int (*OnEvent)(const EventPhys*);
	EventClient* next;
	float priority;
};

struct EventChunk
{
	EventChunk* next;
};

struct pe_PODcell
{
	int inextActive;
	float lifeTime;
	Vec2 zlim;
};

class CPhysicalWorld final : public IPhysicalWorld, public IPhysUtils, public CGeomManager
{
public:
	CPhysicalWorld();
	~CPhysicalWorld();

	void Init();
	void Shutdown(int bDeleteEntities = 1);
	void Release() { delete this; }
	IGeomManager* GetGeomManager() { return this; }
	IPhysUtils* GetPhysUtils() { return this; }

	void SetupEntityGrid(int axisz, Vec3 org, int nx, int ny, float stepx, float stepy);
	void DeactivateOnDemandGrid();
	void RegisterBBoxInPODGrid(const Vec3* BBox);
	int AddRefEntInPODGrid(IPhysicalEntity* pent, const Vec3* BBox = 0);
	IPhysicalEntity* SetHeightfieldData(const primitives::heightfield* phf, int* pMatMapping = 0, int nMats = 0);
	IPhysicalEntity* GetHeightfieldData(primitives::heightfield* phf);
	int SetSurfaceParameters(int surface_idx, float bounciness, float friction, unsigned int flags = 0);
	int GetSurfaceParameters(int surface_idx, float& bounciness, float& friction, unsigned int& flags);
	PhysicsVars* GetPhysVars() { return &m_vars; }

	void GetPODGridCellBBox(int ix, int iy, Vec3& center, Vec3& size);
	pe_PODcell* getPODcell(int ix, int iy);

	IPhysicalEntity* CreatePhysicalEntity(pe_type type, pe_params* params = 0, void* pForeignData = 0,
	                                      int iForeignData = 0, int id = -1);
	IPhysicalEntity* CreatePhysicalEntity(pe_type type, float lifeTime, pe_params* params = 0,
	                                      void* pForeignData = 0, int iForeignData = 0, int id = -1,
	                                      IPhysicalEntity* pHostPlaceholder = 0);
	IPhysicalEntity* CreatePhysicalPlaceholder(pe_type type, pe_params* params = 0, void* pForeignData = 0,
	                                           int iForeignData = 0, int id = -1);
	int DestroyPhysicalEntity(IPhysicalEntity* pent, int mode = 0, int bThreadSafe = 0);
	int SetPhysicalEntityId(IPhysicalEntity* pent, int id, int bReplace = 1, int bThreadSafe = 0);
	int GetPhysicalEntityId(IPhysicalEntity* pent);
	int GetFreeEntId();
	IPhysicalEntity* GetPhysicalEntityById(int id);
	int IsPlaceholder(CPhysicalPlaceholder* pent);

	void TimeStep(float time_interval, int flags = ent_all | ent_deleted);
	int ResolveGroupContacts(int i, int nAnimatedObjects);
	float GetPhysicsTime() { return m_timePhysics; }
	int GetiPhysicsTime() { return m_iTimePhysics; }
	void SetPhysicsTime(float time)
	{
		m_timePhysics = time;
		if (m_vars.timeGranularity > 0)
		{
			m_iTimePhysics = (int)((m_timePhysics / m_vars.timeGranularity) + 0.5f);
		}
	}
	void SetiPhysicsTime(int itime) { m_timePhysics = (m_iTimePhysics = itime) * m_vars.timeGranularity; }
	void SetSnapshotTime(float time_snapshot, int iType = 0)
	{
		m_timeSnapshot[iType] = time_snapshot;
		if (m_vars.timeGranularity > 0)
		{
			m_iTimeSnapshot[iType] = (int)((time_snapshot / m_vars.timeGranularity) + 0.5f);
		}
	}
	void SetiSnapshotTime(int itime_snapshot, int iType = 0)
	{
		m_iTimeSnapshot[iType] = itime_snapshot;
		m_timeSnapshot[iType] = itime_snapshot * m_vars.timeGranularity;
	}

	// *important* if request RWIs queued iForeignData should be a EPhysicsForeignIds
	int RayWorldIntersection(Vec3 org, Vec3 dir, int objtypes, unsigned int flags, ray_hit* hits, int nmaxhits,
	                         IPhysicalEntity** pSkipEnts = 0, int nSkipEnts = 0, void* pForeignData = 0,
	                         int iForeignData = 0, const char* pNameTag = "RayWorldIntersection(Physics)",
	                         ray_hit_cached* phitLast = 0, int iCaller = 0);
	int TracePendingRays(int bDoTracing = 1);

	void SimulateExplosion(pe_explosion* pexpl, IPhysicalEntity** pSkipEnts = 0, int nSkipEnts = 0,
	                       int iTypes = ent_rigid | ent_sleeping_rigid | ent_living | ent_independent,
	                       int iCaller = 0);
	float IsAffectedByExplosion(IPhysicalEntity* pent, Vec3* impulse = 0);
	float CalculateExplosionExposure(pe_explosion* pexpl, IPhysicalEntity* pient);
	void ResetDynamicEntities();
	void DestroyDynamicEntities();
	void PurgeDeletedEntities();
	int DeformPhysicalEntity(IPhysicalEntity* pent, const Vec3& ptHit, const Vec3& dirHit, float r, int flags = 0);
	void UpdateDeformingEntities(float time_interval);
	int GetEntityCount(int iEntType) { return m_nTypeEnts[iEntType]; }
	int ReserveEntityCount(int nNewEnts);

	void DrawPhysicsHelperInformation(IPhysRenderer* pRenderer, int iCaller = 0);

	void GetMemoryStatistics(ICrySizer*) {}

	int CollideEntityWithBeam(IPhysicalEntity* _pent, Vec3 org, Vec3 dir, float r, ray_hit* phit);
	int RayTraceEntity(IPhysicalEntity* pient, Vec3 origin, Vec3 dir, ray_hit* pHit, pe_params_pos* pp = 0);
	int CollideEntityWithPrimitive(IPhysicalEntity* _pent, int itype, primitives::primitive* pprim, Vec3 dir,
	                               ray_hit* phit);

	float PrimitiveWorldIntersection(int itype, primitives::primitive* pprim, const Vec3& sweepDir = {},
	                                 int entTypes = ent_all, geom_contact** ppcontact = 0, int geomFlagsAll = 0,
	                                 int geomFlagsAny = geom_colltype0 | geom_colltype_player,
	                                 intersection_params* pip = 0, void* pForeignData = 0, int iForeignData = 0,
	                                 IPhysicalEntity** pSkipEnts = 0, int nSkipEnts = 0,
	                                 const char* pNameTag = "PrimitiveWorldIntersection");

	int GetEntitiesInBox(Vec3 ptmin, Vec3 ptmax, IPhysicalEntity**& pList, int objtypes, int szListPrealloc)
	{
		WriteLock lock(m_lockCaller[1]);
		return GetEntitiesAround(ptmin, ptmax, (CPhysicalEntity**&)pList, objtypes, 0, szListPrealloc, 1);
	}
	int GetEntitiesAround(const Vec3& ptmin, const Vec3& ptmax, CPhysicalEntity**& pList, int objtypes,
	                      CPhysicalEntity* pPetitioner = 0, int szListPrealoc = 0, int iCaller = 0);
	void ChangeEntitySimClass(CPhysicalEntity* pent);
	int RepositionEntity(CPhysicalPlaceholder* pobj, int flags = 3, Vec3* BBox = 0, int bQueued = 0);
	void DetachEntityGridThunks(CPhysicalPlaceholder* pobj);
	void ScheduleForStep(CPhysicalEntity* pent);
	CPhysicalEntity* CheckColliderListsIntegrity();

	int CoverPolygonWithCircles(strided_pointer<vector2df> pt, int npt, bool bConsecutive, const vector2df& center,
	                            vector2df*& centers, float*& radii, float minCircleRadius);

	void DeletePointer(void* pdata);

	int qhull(strided_pointer<Vec3> pts, int npts, index_t*& pTris);

	void SetPhysicsStreamer(IPhysicsStreamer* pStreamer) { m_pPhysicsStreamer = pStreamer; }
	void SetPhysicsEventClient(IPhysicsEventClient* pEventClient) { m_pEventClient = pEventClient; }
	float GetLastEntityUpdateTime(IPhysicalEntity* pent)
	{
		return m_updateTimes[((CPhysicalPlaceholder*)pent)->m_iSimClass & 7];
	}

	volatile int* GetInternalLock(int idx)
	{
		switch (idx)
		{
			case PLOCK_WORLD_STEP:
				return &m_lockStep;
			case PLOCK_CALLER0:
				return m_lockCaller + 0;
			case PLOCK_CALLER1:
				return m_lockCaller + 1;
			case PLOCK_QUEUE:
				return &m_lockQueue;
			case PLOCK_AREAS:
				return &m_lockAreas;
		}
		return 0;
	}

	void AddEntityProfileInfo(CPhysicalEntity* pent, int nTicks);
	int GetEntityProfileInfo(phys_profile_info*& pList)
	{
		pList = m_pEntProfileData;
		return m_nProfiledEnts;
	}
	void AddFuncProfileInfo(const char* name, int nTicks);
	int GetFuncProfileInfo(phys_profile_info*& pList)
	{
		pList = m_pFuncProfileData;
		return m_nProfileFunx;
	}

	// *important* if provided callback function return 0, other registered listeners are not called anymore.
	void AddEventClient(int type, int (*func)(const EventPhys*), int bLogged, float priority = 1.0f);
	int RemoveEventClient(int type, int (*func)(const EventPhys*), int bLogged);
	void PumpLoggedEvents();
	void ClearLoggedEvents();
	void CleanseEventsQueue();
	EventPhys* AllocEvent(int id, int sz);

	template<class Etype>
	int OnEvent(unsigned int flags, Etype* pEvent, Etype** pEventLogged = 0)
	{
		int res = 0;
		if ((flags & Etype::flagsCall) == Etype::flagsCall)
		{
			res = SignalEvent(pEvent, 0);
		}
		if ((flags & Etype::flagsLog) == Etype::flagsLog)
		{
			WriteLock lock(m_lockEventsQueue);
			Etype* pDst = (Etype*)AllocEvent(Etype::id, sizeof(Etype));
			*pDst = *pEvent;
			pDst->next = 0;
			(m_pEventLast ? m_pEventLast->next : m_pEventFirst) = pDst;
			m_pEventLast = pDst;
			if (pEventLogged)
			{
				*pEventLogged = pDst;
			}
			if (Etype::id == (const int)EventPhysPostStep::id)
			{
				CPhysicalPlaceholder* ppc =
				    (CPhysicalPlaceholder*)((EventPhysPostStep*)pEvent)->pEntity;
				if (ppc->m_bProcessed & PENT_SETPOSED)
				{
					AtomicAdd(&ppc->m_bProcessed, -PENT_SETPOSED);
				}
			}
		}
		return res;
	}

	template<class Etype>
	int SignalEvent(Etype* pEvent, int bLogged)
	{
		int nres = 0;
		EventClient* pClient;
		for (pClient = m_pEventClients[Etype::id][bLogged]; pClient; pClient = pClient->next)
		{
			nres += pClient->OnEvent(pEvent);
		}
		return nres;
	}

	int SerializeWorld(const char* fname, int bSave);
	int SerializeGeometries(const char* fname, int bSave);

	IPhysicalEntity* AddGlobalArea();
	IPhysicalEntity* AddArea(Vec3* pt, int npt, float zmin, float zmax, const Vec3& pos = Vec3(0, 0, 0),
	                         const quaternionf& q = quaternionf(), float scale = 1.0f, const Vec3& normal = {},
	                         int* pTessIdx = 0, int nTessTris = 0, Vec3* pFlows = 0);
	IPhysicalEntity* AddArea(IGeometry* pGeom, const Vec3& pos, const quaternionf& q, float scale);
	IPhysicalEntity* AddArea(Vec3* pt, int npt, float r, const Vec3& pos, const quaternionf& q, float scale);
	void RemoveArea(IPhysicalEntity* pArea);
	int CheckAreas(const Vec3& ptc, Vec3& gravity, pe_params_buoyancy* pb, int nMaxBuoys = 1, const Vec3& vel = {},
	               IPhysicalEntity* pent = 0, int iCaller = 0);
	int CheckAreas(CPhysicalEntity* pent, Vec3& gravity, pe_params_buoyancy* pb, int nMaxBuoys = 1,
	               const Vec3& vel = {}, int iCaller = 0);
	void RepositionArea(CPhysArea* pArea);
	IPhysicalEntity* GetNextArea(IPhysicalEntity* pPrevArea = 0);

	void SetWaterMat(int imat);
	int GetWaterMat() { return m_matWater; }
	int SetWaterManagerParams(pe_params* params);
	int GetWaterManagerParams(pe_params* params);
	int GetWatermanStatus(pe_status* status);
	void DestroyWaterManager();

	int AddExplosionShape(IGeometry* pGeom, float size, int idmat, float probability = 1.0f);
	void RemoveExplosionShape(int id);
	IGeometry* GetExplosionShape(float size, int idmat, float& scale, int& bCreateConstraint);
	int DeformEntityPart(CPhysicalEntity* pent, int i, pe_explosion* pexpl, geom_world_data* gwd,
	                     geom_world_data* gwd1, int iSource = 0);
	void MarkEntityAsDeforming(CPhysicalEntity* pent);
	void ClonePhysGeomInEntity(CPhysicalEntity* pent, int i, IGeometry* pNewGeom);

	void AllocRequestsQueue(int sz);
	void* QueueData(void* ptr, int sz);

	template<class T>
	T* QueueData(const T& data)
	{
		*(T*)(m_pQueueSlots[m_nQueueSlots - 1] + m_nQueueSlotSize) = data;
		m_nQueueSlotSize += sizeof(data);
		*(int*)(m_pQueueSlots[m_nQueueSlots - 1] + m_nQueueSlotSize) = -1;
		return (T*)(m_pQueueSlots[m_nQueueSlots - 1] + m_nQueueSlotSize - sizeof(data));
	}

	entity_contact* AllocContact();
	void FreeContact(entity_contact* pContact);

	float GetFriction(int imat0, int imat1, int bDynamic = 0);
	float GetBounciness(int imat0, int imat1);

	void SavePhysicalEntityPtr(TSerialize ser, CPhysicalEntity* pent);
	CPhysicalEntity* LoadPhysicalEntityPtr(TSerialize ser);

	IPhysicalWorld* GetIWorld() { return this; }

	bool IsPhysicsThread();
	bool IsPodThread();
	void SetPhysicsThreadId();
	void SetPodThreadId();
	void UnsetPodThreadId();

	void SerializeGarbageTypedSnapshot(TSerialize ser, int iSnapshotType, int flags);

	PhysicsVars m_vars;
	IPhysicsStreamer* m_pPhysicsStreamer;
	IPhysicsEventClient* m_pEventClient;
	IPhysRenderer* m_pRenderer;

	CPhysicalEntity *m_pTypedEnts[8], *m_pTypedEntsPerm[8];
	CPhysicalEntity **m_pTmpEntList, **m_pTmpEntList1, **m_pTmpEntList2;
	float *m_pGroupMass, *m_pMassList;
	int *m_pGroupIds, *m_pGroupNums;
	primitives::grid m_entgrid;
	int m_iEntAxisz;
	int* m_pEntGrid;
	pe_gridthunk* m_gthunks;
	int m_thunkPoolSz, m_iFreeGThunk0;
	pe_PODcell **m_pPODCells, m_dummyPODcell, *m_pDummyPODcell;
	vector2di m_PODstride;
	volatile int m_lockPODGrid;
	int m_bHasPODGrid, m_iActivePODCell0;
	int m_nEnts, m_nEntsAlloc;
	int m_nDynamicEntitiesDeleted;
	CPhysicalPlaceholder** m_pEntsById;
	int m_nIdsAlloc, m_iNextId;
	int m_iNextIdDown, m_lastExtId, m_nExtIds;
	int m_bGridThunksChanged;
	int m_bUpdateOnlyFlagged;
	int m_nTypeEnts[10];
	int m_bEntityCountReserved;
	volatile int m_lockEntIdList;
	volatile int m_nGEA[2];
	volatile int m_nEntListAllocs;
	int m_nOnDemandListFailures;
	int m_iLastPODUpdate;
	Vec3 m_prevGEABBox[2];
	int m_prevGEAobjtypes;
	int m_nprevGEAEnts;

	int m_nPlaceholders, m_nPlaceholderChunks, m_iLastPlaceholder;
	CPhysicalPlaceholder** m_pPlaceholders;
	int* m_pPlaceholderMap;
	CPhysicalEntity* m_pEntBeingDeleted;

	int *m_pGridStat[6], *m_pGridDyn[6];
	int m_nOccRes;
	Vec3 m_lastEpicenter, m_lastEpicenterImp, m_lastExplDir;
	float m_lastRmax;
	CPhysicalEntity** m_pExplVictims;
	float* m_pExplVictimsFrac;
	Vec3* m_pExplVictimsImp;
	int m_nExplVictims, m_nExplVictimsAlloc;

	CPhysicalEntity* m_pHeightfield[2];
	Matrix33 m_HeightfieldBasis;
	Vec3 m_HeightfieldOrigin;

	float m_timePhysics, m_timeSurplus, m_timeSnapshot[4];
	int m_iTimePhysics, m_iTimeSnapshot[4];
	float m_updateTimes[8];
	int m_iSubstep, m_bWorldStep, m_iCurGroup;
	int m_bCurGroupInvisible;
	float m_curGroupMass;
	CPhysicalEntity* m_pAuxStepEnt;
	phys_profile_info m_pEntProfileData[16];
	int m_nProfiledEnts;
	phys_profile_info* m_pFuncProfileData;
	int m_nProfileFunx, m_nProfileFunxAlloc;
	volatile int m_lockFuncProfiler;
	float m_groupTimeStep;
	float m_lastTimeInterval;
	std::atomic<std::thread::id> m_threadId{};
	std::atomic<std::thread::id> m_podThreadId{};

	float m_BouncinessTable[NSURFACETYPES];
	float m_FrictionTable[NSURFACETYPES];
	float m_DynFrictionTable[NSURFACETYPES];
	unsigned int m_SurfaceFlagsTable[NSURFACETYPES];
	int m_matWater, m_bCheckWaterHits;
	class CWaterMan* m_pWaterMan;
	Vec3 m_posViewer;

	volatile int m_lockStep, m_lockGrid, m_lockCaller[2], m_lockQueue, m_lockList;
	char** m_pQueueSlots;
	int m_nQueueSlots, m_nQueueSlotsAlloc;
	int m_nQueueSlotSize;

	CPhysArea* m_pGlobalArea;
	int m_nAreas, m_nBigAreas;
	volatile int m_lockAreas;
	CPhysArea* m_pDeletedAreas;

	volatile int m_lockEventsQueue, m_iLastLogPump;
	EventPhys *m_pEventFirst, *m_pEventLast, *m_pFreeEvents[EVENT_TYPES_NUM];
	EventClient* m_pEventClients[EVENT_TYPES_NUM][2];
	EventChunk *m_pFirstEventChunk, *m_pCurEventChunk;
	int m_szCurEventChunk;
	int m_nEvents[EVENT_TYPES_NUM];
	volatile int m_idStep;

	entity_contact *m_pFreeContact, *m_pLastFreeContact;
	int m_nFreeContacts;
	int m_nContactsAlloc;

	SExplosionShape* m_pExpl;
	int m_nExpl, m_nExplAlloc, m_idExpl;
	CPhysicalEntity** m_pDeformingEnts;
	int m_nDeformingEnts, m_nDeformingEntsAlloc;
	volatile int m_lockDeformingEntsList;

	SRwiRequest* m_rwiQueue;
	int m_rwiQueueHead, m_rwiQueueTail;
	int m_rwiQueueSz, m_rwiQueueAlloc;
	volatile int m_lockRwiQueue;
	ray_hit *m_pRwiHitsHead, *m_pRwiHitsTail;
	int m_rwiHitsPoolSize;
	int m_rwiPoolEmpty;
	volatile int m_lockRwiHitsPool;
	CPhysicalEntity* m_pentLastHit[2];
	int m_ipartLastHit[2], m_inodeLastHit[2];
	volatile int m_lockTPR;

	SPwiRequest* m_pwiQueue;
	int m_pwiQueueHead, m_pwiQueueTail;
	int m_pwiQueueSz, m_pwiQueueAlloc;
	volatile int m_lockPwiQueue;
};

extern int g_nPhysWorlds;
extern CPhysicalWorld* g_pPhysWorlds[];
