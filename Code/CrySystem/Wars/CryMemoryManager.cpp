// CryMemoryManager.cpp : Defines the entry point for the DLL application.

#include "StdAfx.h"

#include <stdio.h>
#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CryCore/platform.h"

#ifdef WIN32
#include "DebugCallStack.h"
#endif
//#define GARBAGEMEMORY

#ifdef MM_TRACE_ADDRS
// If MM_TRACE_ADDRS is defined, then code will be added to trace allocations
// of specific addresses. The list of addresses is kept in global pointer
// array CryMM_CheckAddr[]. The number of addresses in that array is kept in
// the global CryMM_nCheckAddr. The address checker is typically used by
// setting up the address array at runtime using a debugger.
void *CryMM_CheckAddr[16];
int CryMM_nCheckAddr = 0;

void CryMM_CheckAddrBreak(int index, void *ptr)
{
	// Set your breakpoint here!
}
#endif



//#include "CryMemoryAllocator.h"

//#define NOT_STANDARD_CRT
//#define _BRUTAL_FORCE

//////////////////////////////////////////////////////////////////////////
// Some globals for fast profiling.
//////////////////////////////////////////////////////////////////////////
long g_TotalAllocatedMemory = 0;

#if !defined(LINUX) || 1

#ifndef CRYMEMORYMANAGER_API
#define CRYMEMORYMANAGER_API
#endif CRYMEMORYMANAGER_API

//////////////////////////////////////////////////////////////////////////
CRYMEMORYMANAGER_API size_t CryFree(void *p);
CRYMEMORYMANAGER_API void CryFreeSize(void *p, size_t size);

CRYMEMORYMANAGER_API void CryCleanup();

// Undefine malloc for memory manager itself..
#undef malloc
#undef realloc
#undef free


//malloc and free resides in std on PS3
#if defined(PS3)
	#define _MSTD std
#else
	#define _MSTD 
#endif

/*
#ifndef _XBOX

#define GLOBALPOOLSIZE (32*1025*1025+16)
#define EXTRAPOOLSIZE (32*1025*1025+16)    // how much to allocate if we allow the pool to be exceeded
// currently biggest allocation happends during hires screenshot creation

#else // _XBOX

#ifndef _DEBUG
#define GLOBALPOOLSIZE (8*1025*1025+16)
#define EXTRAPOOLSIZE (8*1025*1025+16)
#else // _DEBUG
#define GLOBALPOOLSIZE (0)
#define EXTRAPOOLSIZE (0)
#endif // _DEBUG

#endif // _XBOX

#define MAXPOOLS 128
void *poolbufs[MAXPOOLS];
int poolsizes[MAXPOOLS];
int numpools = 0;
*/


volatile int g_lockMemMan = 0;


/*
PoolContext *AllocPool(PoolContext *pCtx, int size)
{
// TODO: replace "malloc" with however we obtain memory on other platforms
void *buf = VirtualAlloc(NULL,size,MEM_COMMIT,PAGE_READWRITE);
if(!buf)
{
CryError( "<CrySystem> (AllocPool) malloc() Failed" );
};
bpool(pCtx, buf, size);
if (numpools==MAXPOOLS)
{
CryError( "<CrySystem> (AllocPool) Maximum number of memory pools reached." );
}
poolsizes[numpools] = size;
poolbufs[numpools++] = buf;
return pCtx; 
};
*/

//static PoolContext globalpool;
//static PoolContext *g_pool = AllocPool(InitPoolContext(&globalpool), GLOBALPOOLSIZE);
static unsigned int biggestalloc = 0;
/*
#define MAXSTAT 1000
static int stats[MAXSTAT];
void addstat(int size) { if(size<0 || size>=MAXSTAT) size = MAXSTAT-1; stats[size]++; };
void printstats() { for(int i = 0; i<MAXSTAT; i++) if(stats[i]) { char buf[100]; sprintf(buf, "bucket %d -> %d\n", i, stats[i]); ::OutputDebugString(buf); }; };
int clearstats() { for(int i = 0; i<MAXSTAT; i++) stats[i] = 0; return 0; };
static int foo = clearstats();
*/

#ifdef OLD_BUCKET_ALLOCATOR

#ifdef WIN32
#ifdef WIN64
// non-512 doesn't work for some reason: an infinite loop (recursion with stack overflow) occurs
#define BUCKETQUANT 512    // wouter: affects performance of bucket allocator, modify with care!
#else
#define BUCKETQUANT 512*2
#endif
#else
#define BUCKETQUANT 512		// save some memory overhead with tiny speed cost on consoles
#endif

class PageBucketAllocator
{
	/*
	Generic allocator that combines bucket allocation with reference counted 1 size object pages.
	manages to perform well along each axis:
	- very fast for small objects: only a few instructions in the general case for alloc/dealloc,
	up to several orders of magnitude faster than traditional best fit allocators
	- low per object memory overhead: 0 bytes overhead on small objects, small overhead for
	pages that are partially in use (still significantly lower than other allocators).
	- almost no fragmentation, reuse of pages is close to optimal
	- very good cache locality (page aligned, same size objects)
	*/
	enum { PAGESIZE = 4096 };
	enum { PAGEMASK = (~(PAGESIZE-1)) };
	//enum { PAGESATONCE = 64 };
	enum { PAGESATONCE = 32 };
	enum { PAGEBLOCKSIZE = PAGESIZE*PAGESATONCE };
	enum { PTRSIZE = sizeof(char *) };
	enum { MAXBUCKETS = BUCKETQUANT/4+1 }; // meaning up to size 512 on 32bit pointer systems
	enum { MAXREUSESIZE = MAXBUCKETS*PTRSIZE-PTRSIZE };
	int bucket(int s) { return (s+PTRSIZE-1)>>PTRBITS; };
	int *ppage(void *p) { return (int *)(((INT_PTR)p)&PAGEMASK); };
	enum { PTRBITS = PTRSIZE==2 ? 1 : PTRSIZE==4 ? 2 : 3 };

	void *reuse[MAXBUCKETS];
	void **pages;
	//! Total allocated size.

	void putinbuckets(char *start, char *end, int bsize)
	{
		int size = bsize*PTRSIZE;        
		for(end -= size; start<=end; start += size)
		{
			*((void **)start) = reuse[bsize];
			reuse[bsize] = start;
		};
	};

	void newpageblocks()
	{
		char *b = 0;
#ifndef _BRUTAL_FORCE
		b = (char *)_MSTD::malloc(PAGEBLOCKSIZE); // if we could get page aligned memory here, that would be even better
#else 
		b = (char *)VirtualAlloc(NULL,PAGEBLOCKSIZE,MEM_COMMIT,PAGE_READWRITE);
#endif
		if (!b)
		{
			g_lockMemMan = 0;
			CryError( "*** Memory allocation for %u bytes failed ****",PAGEBLOCKSIZE );
		}
		char *first = ((char *)ppage(b))+PAGESIZE;
		for(int i = 0; i<PAGESATONCE-1; i++)
		{
			void **p = (void **)(first+i*PAGESIZE);
			*p = pages;
			pages = p;
		};
		//if(b-first+PAGESIZE>BUCKETQUANT) bpool(g_pool, first+PAGEBLOCKSIZE-PAGESIZE, b-first+PAGESIZE);
	};

	void *newpage(unsigned int bsize)
	{
		if(!pages) newpageblocks();
		void **page = pages;
		pages = (void **)*pages;
		*page = 0;
		putinbuckets((char *)(page+1), ((char *)page)+PAGESIZE, bsize);
		return alloc(bsize*PTRSIZE);
	};

	void freepage(int *page, int bsize) // worst case if very large amounts of objects get deallocated in random order from when they were allocated
	{
		for(void **r = &reuse[bsize]; *r; )
		{
			if(page == ppage(*r)) *r = *((void **)*r);
			else r = (void **)*r;
		};
		void **p = (void **)page;
		*p = pages;
		pages = p;
	};

public:

	PageBucketAllocator()
	{
		pages = NULL;
		for(int i = 0; i<MAXBUCKETS; i++) reuse[i] = NULL;
	};

	void *alloc(unsigned int size)
	{
#ifdef XENON
		//return malloc(size);
#endif
		if (size == 0)
			return 0;

		if(size>biggestalloc)
		{
			biggestalloc = size;
		};
		if(size>MAXREUSESIZE)
		{
			void *p(0);
#ifndef _BRUTAL_FORCE
			p = _MSTD::malloc(size);
#else
			p = VirtualAlloc(NULL,size,MEM_COMMIT,PAGE_READWRITE);
#endif
			if (!p)
			{
				g_lockMemMan = 0;
				CryError( "*** Memory allocation for %u bytes failed ****",size );
			}
			return p;
		}
		size = bucket(size);
		void **r = (void **)reuse[size];
		if(!r) return newpage(size);
		reuse[size] = *r;
		int *page = ppage(r);
		(*page)++;
#ifdef MM_TRACE_ADDRS
		for (int i = 0; i < CryMM_nCheckAddr; ++i)
			if (r == CryMM_CheckAddr[i]) CryMM_CheckAddrBreak(i, r);
#endif
		return (void *)r;
	};

	void dealloc(void *p, unsigned int size)
	{
#ifdef MM_TRACE_ADDRS
		for (int i = 0; i < CryMM_nCheckAddr; ++i)
			if (p == CryMM_CheckAddr[i]) CryMM_CheckAddrBreak(i, p);
#endif
#ifdef XENON
		//free(p);
		//return;
#endif

		if(size>MAXREUSESIZE)
		{
#ifndef _BRUTAL_FORCE
			_MSTD::free(p);
#else
			VirtualFree( p,0,MEM_RELEASE );
#endif
			//brel(g_pool, p);
		}
		else
		{
			size = bucket(size);
			*((void **)p) = reuse[size];
			reuse[size] = p;
			int *page = ppage(p);
			if(!--(*page)) freepage(page, size);
		};
	};

	void stats()
	{
		int totalwaste = 0;
		for(int i = 0; i<MAXBUCKETS; i++)
		{
			int n = 0;
			for(void **r = (void **)reuse[i]; r; r = (void **)*r) n++;
			if(n)
			{
				int waste = i*4*n/1024;
				totalwaste += waste;
				CryLogAlways("bucket %d -> %d (%d k)\n", i*4, n, waste);
			};
		};
		CryLogAlways("totalwaste %d k\n", totalwaste);
	};
};


PageBucketAllocator g_GlobPageBucketAllocator;
#else /*OLD_BUCKET_ALLOCATOR*/


#define VIRTUAL_ALLOC_SIZE 524288

#include "CryMemoryAllocator.h"
node_alloc<eCryMallocCryFreeCRTCleanup, true, 524288> g_GlobPageBucketAllocator;
#undef CRY_MEMORY_ALLOCATOR

#endif /*OLD_BUCKET_ALLOCATOR*/

static volatile bool bProcessed = false;

#ifndef __MEMORY_VALIDATOR_HACK
CRYMEMORYMANAGER_API void *CryMalloc(size_t size, size_t& allocated)
#else
CRYMEMORYMANAGER_API void *CryMalloc(size_t size)
#endif
{
/*
	static DWORD mainThreadId = GetCurrentThreadId();
	if (mainThreadId != GetCurrentThreadId())
	{
		static int numAlloc = 0;
		numAlloc++;
		char str[1024];
		sprintf( str,"[%6d]malloc: %d\n",(int)numAlloc,(int)size );
		OutputDebugString( str );
	}
*/

#ifdef WIN32
	if (g_bTraceAllocations && !bProcessed) 
	{
		bProcessed = true;
		CryLogAlways( "*** Memory allocation for %u bytes ****",size );
		DebugCallStack::instance()->LogCallstack();
		bProcessed = false;
	}
#endif

//	WriteLock lock(g_lockMemMan);
#ifdef __MEMORY_VALIDATOR_HACK
	size_t allocated;
#endif

#ifdef NOT_STANDARD_CRT
	int *p;
	if (g_bProfilerEnabled)
	{
		FUNCTION_PROFILER_FAST( GetISystem(),PROFILE_SYSTEM,g_bProfilerEnabled );


		size_t sizePlus = size + sizeof(int);
#ifdef WIN32
		//if (sizePlus > VIRTUAL_ALLOC_SIZE)
		//	p = (int*)VirtualAlloc(NULL,sizePlus,MEM_COMMIT,PAGE_READWRITE);
		//else
		p = (int *)g_GlobPageBucketAllocator.alloc(sizePlus);
#else
		p = (int *)g_GlobPageBucketAllocator.alloc(sizePlus);
#endif

		if(!p)
		{
			allocated = g_lockMemMan = 0;
			CryError( "*** Memory allocation for %u bytes failed ****",sizePlus );
			return 0;		// don't crash - allow caller to react
		}

		CryInterlockedExchangeAdd(&g_TotalAllocatedMemory, +sizePlus);
		//	g_TotalAllocatedMemory += sizePlus;

		*p++ = size;  // stores 2 sizes for big objects!

		allocated = sizePlus;
#ifdef MM_TRACE_ADDRS
		for (int i = 0; i < CryMM_nCheckAddr; ++i)
			if (p == CryMM_CheckAddr[i]) CryMM_CheckAddrBreak(i, p);
#endif
	}
	else
	{
		size_t sizePlus = size + sizeof(int);
#ifdef WIN32
		//if (sizePlus > VIRTUAL_ALLOC_SIZE)
		//	p = (int*)VirtualAlloc(NULL,sizePlus,MEM_COMMIT,PAGE_READWRITE);
		//else
		p = (int *)g_GlobPageBucketAllocator.alloc(sizePlus);
#else
		p = (int *)g_GlobPageBucketAllocator.alloc(sizePlus);
#endif

		if(!p)
		{
			allocated = g_lockMemMan = 0;
			CryError( "*** Memory allocation for %u bytes failed ****",sizePlus );
			return 0;		// don't crash - allow caller to react
		}

		CryInterlockedExchangeAdd(&g_TotalAllocatedMemory, +sizePlus);
		//	g_TotalAllocatedMemory += sizePlus;

		*p++ = size;  // stores 2 sizes for big objects!

		allocated = sizePlus;
#ifdef MM_TRACE_ADDRS
		for (int i = 0; i < CryMM_nCheckAddr; ++i)
			if (p == CryMM_CheckAddr[i]) CryMM_CheckAddrBreak(i, p);
#endif
	}

	return p;
#else /*NOT_STANDARD_CRT*/
	void * p;

	if (!g_bProfilerEnabled)
	{
		p = _MSTD::malloc(size);
	}
	else
	{
		FUNCTION_PROFILER_FAST( GetISystem(),PROFILE_SYSTEM,g_bProfilerEnabled );
		p = _MSTD::malloc(size);
	}
	// UNCOMMMENT IF SOMETHING WRONG WITH ALLOCATED SIZE

	//size_t r = (((size + 7) >> 3) << 3);

	//if (r != _msize(p))
	//	g_TotalAllocatedMemory = 0;
	//else
	allocated = _msize(p);
	g_TotalAllocatedMemory += allocated;


	//g_TotalAllocatedMemory += (((size + 7) >> 3) << 3);

#ifdef MM_TRACE_ADDRS
	for (int i = 0; i < CryMM_nCheckAddr; ++i)
		if (p == CryMM_CheckAddr[i]) CryMM_CheckAddrBreak(i, p);
#endif
	return p;

#endif /*NOT_STANDARD_CRT*/


}

CRYMEMORYMANAGER_API size_t CryGetMemSize(void *memblock, size_t sourceSize)
{
	//	ReadLock lock(g_lockMemMan);
#ifdef NOT_STANDARD_CRT
	size_t oldsize = ((int *)memblock)[-1];
	return oldsize +sizeof(int);;
#else
	//	if (sourceSize == 0)
	return _msize(memblock);
	//	else
	//		return (((sourceSize + 7) >> 3) << 3);
#endif
}

#ifndef __MEMORY_VALIDATOR_HACK
CRYMEMORYMANAGER_API void *CryRealloc(void *memblock, size_t size, size_t& allocated)
#else
CRYMEMORYMANAGER_API void *CryRealloc(void *memblock, size_t size)
#endif
{
	//WriteLock lock(g_lockMemMan);
#ifdef __MEMORY_VALIDATOR_HACK
	size_t allocated;
#endif

#ifdef NOT_STANDARD_CRT

#ifdef MM_TRACE_ADDRS
	for (int i = 0; i < CryMM_nCheckAddr; ++i)
		if (memblock == CryMM_CheckAddr[i]) CryMM_CheckAddrBreak(i, memblock);
#endif

	if (!g_bProfilerEnabled)
	{
		// Without profiler.
		if(memblock==NULL)
			return CryMalloc(size, allocated);
		else
		{
			void *np = CryMalloc(size, allocated);
			size_t oldsize = ((int *)memblock)[-1];
			memcpy(np, memblock, size>oldsize ? oldsize : size);
			CryFree(memblock);
			if (!np)
			{
				g_lockMemMan = 0;
				CryError( "*** Memory allocation for %u bytes failed ****",size );
				return 0;		// don't crash - allow caller to react
			}

#ifdef MM_TRACE_ADDRS
			for (int i = 0; i < CryMM_nCheckAddr; ++i)
				if (np == CryMM_CheckAddr[i]) CryMM_CheckAddrBreak(i, np);
#endif
			//CryInterlockedExchangeAdd(&g_TotalAllocatedMemory, size - oldsize);
			//g_TotalAllocatedMemory = g_TotalAllocatedMemory + size - oldsize;
			return np;
		}
	}
	else
	{
		// With Profiler.
		FUNCTION_PROFILER_FAST( GetISystem(),PROFILE_SYSTEM,g_bProfilerEnabled );
		if(memblock==NULL)
		{
			return CryMalloc(size, allocated);
			//CryInterlockedExchangeAdd(&g_TotalAllocatedMemory, g_TotalAllocatedMemory + size);
			//g_TotalAllocatedMemory = g_TotalAllocatedMemory + size;
		}
		else
		{
			void *np = CryMalloc(size, allocated);
			size_t oldsize = ((int *)memblock)[-1];
			memcpy(np, memblock, size>oldsize ? oldsize : size);
			CryFree(memblock);
#ifdef MM_TRACE_ADDRS
			for (int i = 0; i < CryMM_nCheckAddr; ++i)
				if (np == CryMM_CheckAddr[i]) CryMM_CheckAddrBreak(i, np);
#endif
			//g_TotalAllocatedMemory = g_TotalAllocatedMemory + size - oldsize;
			return np;
		}
	}
#else /*NOT_STANDARD_CRT*/

#ifdef MM_TRACE_ADDRS
	for (int i = 0; i < CryMM_nCheckAddr; ++i)
		if (memblock == CryMM_CheckAddr[i]) CryMM_CheckAddrBreak(i, memblock);
#endif
	if (!g_bProfilerEnabled)
	{
		// Without profiler.
		if(memblock==NULL)
		{
			void * p = _MSTD::malloc(size);
			g_TotalAllocatedMemory = g_TotalAllocatedMemory + _msize(p);

			return p;
		}
		else
		{
			void *np = _MSTD::malloc(size);
			size_t oldsize = _msize(memblock);
			memcpy(np, memblock, size>oldsize ? oldsize : size);
			_MSTD::free(memblock);
			allocated = _msize(np);

			g_TotalAllocatedMemory = g_TotalAllocatedMemory + allocated - oldsize;
#ifdef MM_TRACE_ADDRS
			for (int i = 0; i < CryMM_nCheckAddr; ++i)
				if (memblock == CryMM_CheckAddr[i]) CryMM_CheckAddrBreak(i, memblock);
#endif
			return np;
		}
	}
	else
	{
		// With Profiler.
		FUNCTION_PROFILER_FAST( GetISystem(),PROFILE_SYSTEM,g_bProfilerEnabled );
		if(memblock==NULL)
		{
			void * p = _MSTD::malloc(size);
			g_TotalAllocatedMemory = g_TotalAllocatedMemory + _msize(p);

			return p;
		}
		else
		{
			void *np = _MSTD::malloc(size);
			size_t oldsize = _msize(memblock);
			memcpy(np, memblock, size>oldsize ? oldsize : size);
			_MSTD::free(memblock);
			allocated = _msize(np);

			g_TotalAllocatedMemory = g_TotalAllocatedMemory + allocated - oldsize;

#ifdef MM_TRACE_ADDRS
			for (int i = 0; i < CryMM_nCheckAddr; ++i)
				if (memblock == CryMM_CheckAddr[i]) CryMM_CheckAddrBreak(i, memblock);
#endif
			return np;
		}
	}

#endif /*NOT_STANDARD_CRT*/
}

CRYMEMORYMANAGER_API size_t CryFree(void *p) 
{
//	WriteLock lock(g_lockMemMan);

#ifdef NOT_STANDARD_CRT
#ifdef MM_TRACE_ADDRS
	for (int i = 0; i < CryMM_nCheckAddr; ++i)
		if (p == CryMM_CheckAddr[i]) CryMM_CheckAddrBreak(i, p);
#endif

	if (g_bProfilerEnabled)
	{
		FUNCTION_PROFILER_FAST( GetISystem(),PROFILE_SYSTEM,g_bProfilerEnabled );
	}

	if (p != NULL)
	{
		unsigned int *t = (unsigned int *)p;
		size_t size = *--t;	

#ifdef MINIMALDEBUG
		if (size>=100000000)
		{
			g_lockMemMan = 0;
			CryError("[CRYMANAGER ERROR](CryFree): illegal size 0x%X - block header was corrupted",size);
		}
#endif

		size += sizeof(int);
#ifdef GARBAGEMEMORY     //FIXME: *disabling* memset caused random crash???
		memset(t, 0xBA, size); 
#endif

		long lsize = size;
		CryInterlockedExchangeAdd(&g_TotalAllocatedMemory, -lsize);
		//g_TotalAllocatedMemory -= size;

#ifdef WIN32
		//if (size > VIRTUAL_ALLOC_SIZE)
		//	VirtualFree( t,0,MEM_RELEASE );
		//else
		g_GlobPageBucketAllocator.dealloc(t, size);
#else
		g_GlobPageBucketAllocator.dealloc(t, size);
#endif

		return size;
	}

	return 0;
#else /*NOT_STANDARD_CRT*/

#ifdef MM_TRACE_ADDRS
	for (int i = 0; i < CryMM_nCheckAddr; ++i)
		if (p == CryMM_CheckAddr[i]) CryMM_CheckAddrBreak(i, p);
#endif

#ifdef GARBAGEMEMORY     //FIXME: *disabling* memset caused random crash???
	memset(p, 0xBA, size); 
#endif

	if (!g_bProfilerEnabled)
	{
		size_t size = _msize(p);
		if (size == -1)
		{
			// wrong deallocation
			g_lockMemMan = 0;
			CryError( "[CRYMANAGER ERROR](CryFree): illegal size 0x%X - block header was corrupted",size);
		}
		g_TotalAllocatedMemory -= size;
		_MSTD::free(p);
		return size;
	}
	else
	{
		FUNCTION_PROFILER_FAST( GetISystem(),PROFILE_SYSTEM,g_bProfilerEnabled );
		size_t size = _msize(p);
		if (size == -1)
		{
			// wrong deallocation
			g_lockMemMan = 0;
			CryError( "[CRYMANAGER ERROR](CryFree): illegal size 0x%X - block header was corrupted",size);
		}
		g_TotalAllocatedMemory -= size;
		_MSTD::free(p);
		return size;
	}

	return 0;
#endif /*NOT_STANDARD_CRT*/
}

CRYMEMORYMANAGER_API void CryFlushAll()  // releases/resets ALL memory... this is useful for restarting the game
{
//	WriteLock lock(g_lockMemMan);
	/*
	InitPoolContext(g_pool);
	for(int i = 0; i<numpools; i++) 
	bpool(g_pool, poolbufs[i], poolsizes[i]);
	*/
#ifdef  OLD_BUCKET_ALLOCATOR
	new (&g_GlobPageBucketAllocator) PageBucketAllocator();
#endif /*OLD_BUCKET_ALLOCATOR*/

	g_TotalAllocatedMemory = 0;
};

/* MarcoK: This is never used anywhere ... commented out (LINUX port)
extern "C" CRYMEMORYMANAGER_API void CryFreeMemoryPools()  // releases/resets ALL memory... this is useful for restarting the game
{
for(int i = 0; i<numpools; i++) 
{
void *pBuf = poolbufs[i];
VirtualFree( pBuf,0,MEM_RELEASE );
}
numpools = 0;
g_TotalAllocatedMemory = 0;
};
*/

//////////////////////////////////////////////////////////////////////////
// Returns ammount of memory allocated with CryMalloc/CryFree functions.
//////////////////////////////////////////////////////////////////////////
CRYMEMORYMANAGER_API int CryMemoryGetAllocatedSize()
{
	return g_TotalAllocatedMemory;
}

//////////////////////////////////////////////////////////////////////////
CRYMEMORYMANAGER_API int CryMemoryGetPoolSize()
{
	ReadLock lock(g_lockMemMan);

	int totalsize = 0;
/*
	for(int i = 0; i<numpools; i++) 
		totalsize += poolsizes[i];
*/
	return totalsize;
}

//////////////////////////////////////////////////////////////////////////
CRYMEMORYMANAGER_API int CryStats(char *buf)
{
	ReadLock lock(g_lockMemMan);

	int curalloc=0, totfree=0, maxfree=0, nget=0, nrel=0;
	//bstats(g_pool, &curalloc, &totfree, &maxfree, &nget, &nrel);
	if(buf)
	{
		int poolsize = CryMemoryGetPoolSize();
		sprintf(buf, "Memory Allocated = %d K, totfree = %d K , maxfree = %d K, nmalloc = %d, nfree = %d, biggestalloc = %d, Pool Size = %d K",
			curalloc/1024, totfree/1024, maxfree/1024, nget, nrel, biggestalloc,poolsize/1024);
		//printstats();
#ifdef OLD_BUCKET_ALLOCATOR
		g_GlobPageBucketAllocator.stats();
#endif /*OLD_BUCKET_ALLOCATOR*/
	};
	return curalloc/1024;
}

CRYMEMORYMANAGER_API int CryGetUsedHeapSize()
{
#ifdef OLD_BUCKET_ALLOCATOR
	CRY_ASSERT_MESSAGE(0, "CryGetUsedHeapSize not implemented for old bucket allocator");
	return 0;
#else
	return g_GlobPageBucketAllocator.get_heap_size();
#endif
}

CRYMEMORYMANAGER_API int CryGetWastedHeapSize()
{
#ifdef OLD_BUCKET_ALLOCATOR
	CRY_ASSERT_MESSAGE(0, "CryGetWastedHeapSize not implemented for old bucket allocator");
	return 0;
#else
	return g_GlobPageBucketAllocator.get_wasted_in_allocation();
#endif
}

CRYMEMORYMANAGER_API void CryCleanup()
{
#ifdef NOT_STANDARD_CRT
	WriteLock lock(g_lockMemMan);
	g_GlobPageBucketAllocator.cleanup();
#endif
}

CRYMEMORYMANAGER_API void *CrySystemCrtMalloc(size_t size)
{
	return _MSTD::malloc(size);
}

CRYMEMORYMANAGER_API void CrySystemCrtFree(void *p)
{
	return _MSTD::free(p);
}

CRYMEMORYMANAGER_API size_t CrySystemCrtSize(void *p)
{
#if defined(MEM_MAN_ADD_SIZE_BLOCK)
	const uint32* pMem = (uint32*)((uint8*)p - 16);
	if(pMem[1] != 0x8F8F8F8F)//comes from somewhere else (possible SPU allocation)
	{
		return 4;//unknown size
	}
	else
	{
		size_t n = *pMem;
		pMem = (uint32*)((uint8*)pMem - pMem[3]);//additional offset stored in 3rd word
		return n;
	}

#elif LINUX
	assert(false && "need _msize");
	return 0;
#else
	return _msize(p);
#endif
}

/*
extern "C" void debug(int n)
{
char buf[100];
sprintf(buf, "BESTFIT: %d\n", n);  
::OutputDebugString(buf); 
};
*/// CryMemoryManager.cpp : Defines the entry point for the DLL application.
#endif //LINUX


/*
//////////////////////////////////////////////////////////////////////////
class CCryMemoryManager : public IMemoryManager
{
	virtual bool GetProcessMemInfo( SProcessMemInfo &minfo ) = 0;
};
*/