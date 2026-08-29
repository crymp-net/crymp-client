#ifndef _CRY_SYSTEM_MT_SAFE_ALLOCATOR_HDR_
#define _CRY_SYSTEM_MT_SAFE_ALLOCATOR_HDR_

#include <stdexcept>
#if defined( LINUX )
#	include "Linux_Win32Wrapper.h"
#endif
#include "CryCommon/CrySystem/ISystem.h"

#define NUM_POOLS 6

class CMTSafeHeap
{
public:
	CMTSafeHeap();

	~CMTSafeHeap();

	void* Alloc (size_t nSize, const char* szDbgSource,bool bUsePools=true);
	void Free (void*p);

	// the number of allocations this heap holds right now
	unsigned NumAllocations()const {return m_numAllocations;}
	// the total size of all allocations
	//size_t GetAllocatedSize()const {return m_nTotalAllocations;}

	void GetMemoryUsage( ICrySizer *pSizer );

	// zlib-compatible stubs
	static void* StaticAlloc (void* pOpaque, unsigned nItems, unsigned nSize);
	static void StaticFree (void* pOpaque, void* pAddress);

	static int m_numBigAllocationSize;
	static int m_numBigAllocationStartSize;
protected:
	size_t m_nTotalAllocations;
	int m_numAllocations;
	long m_iBigPoolUsed[NUM_POOLS];
	long m_iBigPoolSize[NUM_POOLS];
	const char* m_sBigPoolDescription[NUM_POOLS];
	void * m_pBigPool[NUM_POOLS];
};

#endif