#include "StdAfx.h"
#include "MTSafeAllocator.h"
#include "CryCommon/CrySystem/IConsole.h"

int CMTSafeHeap::m_numBigAllocationSize = 24*1024*1024;
int CMTSafeHeap::m_numBigAllocationStartSize = 1*1024;

extern CMTSafeHeap* g_pPakHeap;

CMTSafeHeap::CMTSafeHeap()  //m_pBigPool(0), m_iBigPoolUsed(0)
{
	//IConsole* pConsole( gEnv->pConsole );
	//if( 0 != pConsole )
	//{
	//	pConsole->Register( "sys_mtsafeheap_size", &m_numBigAllocationSize, m_numBigAllocationSize, VF_DUMPTODISK, "Sets the CMTSafeHeap big allocation size" );
	//	pConsole->Register( "sys_mtsafeheap_startsize", &m_numBigAllocationStartSize, m_numBigAllocationStartSize, VF_DUMPTODISK, "Sets the CMTSafeHeap start size for big allocation usage" );
	//}

#if !defined(XENON) && !defined(PS3)
	m_iBigPoolSize[0] = 100 * 1024;
	m_iBigPoolSize[1] = 512 * 1024;
	m_iBigPoolSize[2] = 1024 * 1024;
	m_iBigPoolSize[3] = 2048 * 1024;
	m_iBigPoolSize[4] = 5 * 1024 * 1024;
	m_iBigPoolSize[5] = 14 * 1024 * 1024;
#else
  m_iBigPoolSize[0] = 0 * 1024;
  m_iBigPoolSize[1] = 0 * 1024;
  m_iBigPoolSize[2] = 0 * 1024;
  m_iBigPoolSize[3] = 0 * 1024;
  m_iBigPoolSize[4] = 0 * 1024 * 1024;
  m_iBigPoolSize[5] = 0 * 1024 * 1024;
#endif
//	m_numBigAllocationSize >> (NUM_POOLS - i - 1);

	for (int i = 0; i < NUM_POOLS; ++i)
	{
		
		m_pBigPool[i] = static_cast<void*>(new char[m_iBigPoolSize[i]]);
		m_iBigPoolUsed[i] = 0;
		m_sBigPoolDescription[i] = "";
		if (!m_pBigPool[i])
		{
	//		CryError("Failed CMTSafeHeap::m_pBigPool allocation");
#if defined(PS3) || defined(LINUX)
			CRY_ASSERT_MESSAGE(0, "Failed CMTSafeHeap::m_pBigPool allocation");
#else
			throw;
#endif
		}
	}

	m_numAllocations = 0;
};

CMTSafeHeap::~CMTSafeHeap() 
{
	for (int i = 0; i < NUM_POOLS; ++i)
		delete [] (uint8*)m_pBigPool[i];
};

void* CMTSafeHeap::Alloc (size_t nSize, const char* szDbgSource,bool bUsePools)
{
	m_numAllocations++;
#if !defined(XENON) && !defined(PS3)
	if (bUsePools && nSize >= m_numBigAllocationStartSize && nSize <= m_numBigAllocationSize)
	{
		for (int i = 0; i < NUM_POOLS; ++i)
		{
			if (!m_iBigPoolUsed[i] && nSize < m_iBigPoolSize[i])
			{
				m_sBigPoolDescription[i] = szDbgSource;
				if (CryInterlockedCompareExchange(&m_iBigPoolUsed[i], 1, 0) == 0)
					return m_pBigPool[i];
			}
		}

		//CryWarning(VALIDATOR_MODULE_SYSTEM,VALIDATOR_WARNING,"CMTSafeHeap::m_pBigPool is already used. %s uses malloc instead", szDbgSource );
	}
#endif
	return malloc(nSize);
};


void CMTSafeHeap::Free (void*p)
{
	m_numAllocations--;

	for (int i = 0; i < NUM_POOLS; ++i)
	{
		if (p == m_pBigPool[i])
		{
			m_sBigPoolDescription[i] = "";
			m_iBigPoolUsed[i] = 0;
			return;
		}
	}

	free(p);
};

//////////////////////////////////////////////////////////////////////////
void* CMTSafeHeap::StaticAlloc (void* pOpaque, unsigned nItems, unsigned nSize)
{
	return g_pPakHeap->Alloc(nItems*nSize,"StaticAlloc");
}

//////////////////////////////////////////////////////////////////////////
void CMTSafeHeap::StaticFree (void* pOpaque, void* pAddress)
{
	g_pPakHeap->Free(pAddress);
}

//////////////////////////////////////////////////////////////////////////
void CMTSafeHeap::GetMemoryUsage( ICrySizer *pSizer )
{
	SIZER_COMPONENT_NAME(pSizer, "FileSystem Pools");
	for (int i = 0; i < NUM_POOLS; ++i)
	{
		if (m_pBigPool[i])
		{
			pSizer->AddObject( m_pBigPool[i],m_iBigPoolSize[i] );
		}
	}
}
