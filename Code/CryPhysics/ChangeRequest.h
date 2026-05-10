#pragma once

#include "PhysicalWorld.h"
#include "Utils.h"

struct subref
{
	int iPtrOffs;
	int iSzFixed;
	int iSzOffs;
	int iSzUnit;
	subref* next;

	void set(int _iPtrOffs, int _iSzFixed, int _iSzOffs, int _iSzUnit, subref* _next)
	{
		iPtrOffs = _iPtrOffs;
		iSzFixed = _iSzFixed;
		iSzOffs = _iSzOffs;
		iSzUnit = _iSzUnit;
		next = _next;
	}
};

int GetStructSize(pe_params* params);
int GetStructSize(pe_action* action);
int GetStructSize(pe_geomparams* geomparams);
int GetStructSize(void*);

subref* GetSubref(pe_params* params);
subref* GetSubref(pe_action* action);
subref* GetSubref(pe_geomparams* geomparams);
subref* GetSubref(void*);

inline int GetStructId(pe_params*)
{
	return 0;
}

inline int GetStructId(pe_action*)
{
	return 1;
}

inline int GetStructId(pe_geomparams*)
{
	return 2;
}

inline int GetStructId(void*)
{
	return 3;
}

inline bool StructUsesAuxData(pe_params*)
{
	return false;
}

inline bool StructUsesAuxData(pe_action*)
{
	return false;
}

inline bool StructUsesAuxData(pe_geomparams*)
{
	return true;
}

inline bool StructUsesAuxData(void*)
{
	return true;
}

template<class T>
inline bool StructChangesPos(T*)
{
	return false;
}

inline bool StructChangesPos(pe_params* params)
{
	pe_params_pos* pp = (pe_params_pos*)params;
	return params->type == pe_params_pos::type_id &&
	       (!is_unused(pp->pos) || !is_unused(pp->pMtx3x4) && pp->pMtx3x4) && !(pp->bRecalcBounds & 16);
}

template<class T>
inline void OnStructQueued(T* params, CPhysicalWorld* pWorld, void* ptrAux, int iAux)
{
}

inline void OnStructQueued(pe_geomparams* params, CPhysicalWorld* pWorld, void* ptrAux, int iAux)
{
	pWorld->AddRefGeometry((phys_geometry*)ptrAux);
}

template<class T>
struct ChangeRequest
{
	CPhysicalWorld* m_pWorld = nullptr;
	int m_bQueued = 0;
	int m_bLocked = 0;
	T* m_pQueued = nullptr;

	ChangeRequest(CPhysicalPlaceholder* pent, CPhysicalWorld* pWorld, T* params, int bInactive,
	              void* ptrAux = nullptr, int iAux = 0)
	    : m_pWorld(pWorld)
	{
		if (bInactive <= 0 && pent->m_iSimClass != 7)
		{
			if (m_pWorld->m_lockStep || m_pWorld->m_lockTPR || pent->m_bProcessed >= PENT_QUEUED ||
			    bInactive < 0)
			{
				subref* psubref;
				int szSubref, szTot;
				WriteLock lock(m_pWorld->m_lockQueue);
				AtomicAdd(&pent->m_bProcessed, PENT_QUEUED);
				for (psubref = GetSubref(params), szSubref = 0; psubref; psubref = psubref->next)
				{
					if (*(char**)((char*)params + psubref->iPtrOffs) &&
					    !is_unused(*(char**)((char*)params + psubref->iPtrOffs)))
					{
						szSubref += ((*(int*)((char*)params + max(0, psubref->iSzOffs)) &
						              -psubref->iSzOffs >> 31) +
						             psubref->iSzFixed) *
						            psubref->iSzUnit;
					}
				}
				szTot = (sizeof(int) * 2) + sizeof(void*) + GetStructSize(params) + szSubref;
				if (StructUsesAuxData(params))
				{
					szTot += sizeof(void*) + sizeof(int);
				}
				m_pWorld->AllocRequestsQueue(szTot);
				m_pWorld->QueueData(GetStructId(params));
				m_pWorld->QueueData(szTot);
				m_pWorld->QueueData(pent);
				if (StructUsesAuxData(params))
				{
					m_pWorld->QueueData(ptrAux);
					m_pWorld->QueueData(iAux);
				}
				m_pQueued = (T*)m_pWorld->QueueData(params, GetStructSize(params));
				for (psubref = GetSubref(params); psubref; psubref = psubref->next)
				{
					szSubref = ((*(int*)((char*)params + max(0, psubref->iSzOffs)) &
					             -psubref->iSzOffs >> 31) +
					            psubref->iSzFixed) *
					           psubref->iSzUnit;
					if (*(char**)((char*)params + psubref->iPtrOffs) &&
					    !is_unused(*(char**)((char*)params + psubref->iPtrOffs)))
					{
						*(void**)((char*)m_pQueued + psubref->iPtrOffs) = m_pWorld->QueueData(
						    *(char**)((char*)params + psubref->iPtrOffs), szSubref);
					}
				}
				OnStructQueued(params, pWorld, ptrAux, iAux);
				m_bQueued = 1;
			}
			else
			{
				SpinLock(&m_pWorld->m_lockStep, 0, 1);
				m_bLocked = 1;
			}
		}

		if (StructChangesPos(params) && !(pent->m_bProcessed & PENT_SETPOSED))
		{
			AtomicAdd(&pent->m_bProcessed, PENT_SETPOSED);
		}
	}

	~ChangeRequest() { AtomicAdd(&m_pWorld->m_lockStep, -m_bLocked); }

	int IsQueued() { return m_bQueued; }

	T* GetQueuedStruct() { return m_pQueued; }
};
