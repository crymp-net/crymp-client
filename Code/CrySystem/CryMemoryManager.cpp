#include <atomic>
#include <cinttypes>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <malloc.h>  // _msize
#include <memory_resource>
#include <mutex>
#include <unordered_map>

#include "config.h"

#include <tracy/Tracy.hpp>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>

#include "CryCommon/CryCore/CryMalloc.h"
#include "CryCommon/CrySystem/ISystem.h"

#include "CryMemoryManager.h"

static char g_fault_message[256];

[[noreturn]] [[maybe_unused]] static void Die(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	std::vsnprintf(g_fault_message, sizeof(g_fault_message), format, args);
	va_end(args);

#ifdef CRYMP_CONSOLE_APP
	std::printf("%s\n", g_fault_message);
	std::fflush(stdout);
#endif

	// summon crash logger to pickup our message and log the callstack etc.
	__debugbreak();
}

////////////////////////////////////////////////////////////////////////////////
// Statistics
////////////////////////////////////////////////////////////////////////////////

struct Stats
{
	std::atomic_uint64_t mallocCalls{};
	std::atomic_uint64_t reallocCalls{};
	std::atomic_uint64_t freeCalls{};
	std::atomic_uint64_t sizeCalls{};
	std::atomic_uint64_t crtMallocCalls{};
	std::atomic_uint64_t crtFreeCalls{};

	std::atomic_uint64_t safePoolBlocks{};
	std::atomic_uint64_t safePoolFreeBlocks{};
	std::atomic_uint64_t safePoolAllocs{};
	std::atomic_uint64_t safePoolFailedAllocs{};
	std::atomic_uint64_t safePoolDeallocs{};

	void Log()
	{
		CryLogAlways("------------------------------- CryMemoryManager -------------------------------");

		CryLogAlways("Calls:");
		CryLogAlways("- CryMalloc: $5%" PRIu64 "$o + $5%" PRIu64 "$o",
			mallocCalls.load(std::memory_order_relaxed),
			crtMallocCalls.load(std::memory_order_relaxed));
		CryLogAlways("- CryRealloc: $5%" PRIu64 "$o",
			reallocCalls.load(std::memory_order_relaxed));
		CryLogAlways("- CryFree: $5%" PRIu64 "$o + $5%" PRIu64 "$o",
			freeCalls.load(std::memory_order_relaxed),
			crtFreeCalls.load(std::memory_order_relaxed));
		CryLogAlways("- CryGetMemSize: $5%" PRIu64 "$o",
			sizeCalls.load(std::memory_order_relaxed));

		CryLogAlways("SafePool:");
		CryLogAlways("- Blocks: $5%" PRIu64 "$o ($5%" PRIu64 "$o free)",
			safePoolBlocks.load(std::memory_order_relaxed),
			safePoolFreeBlocks.load(std::memory_order_relaxed));
		CryLogAlways("- Allocs: $5%" PRIu64 "$o ($5%" PRIu64 "$o failed)",
			safePoolAllocs.load(std::memory_order_relaxed),
			safePoolFailedAllocs.load(std::memory_order_relaxed));
		CryLogAlways("- Deallocs: $5%" PRIu64 "$o",
			safePoolDeallocs.load(std::memory_order_relaxed));

		CryLogAlways("--------------------------------------------------------------------------------");
	}
};

static Stats g_stats;

////////////////////////////////////////////////////////////////////////////////
// Debug allocator
////////////////////////////////////////////////////////////////////////////////

#ifdef CRYMP_DEBUG_ALLOCATOR_ENABLED

static void Log(const char* format, ...)
{
#ifdef CRYMP_CONSOLE_APP
	va_list args;
	va_start(args, format);
	std::vprintf(format, args);
	va_end(args);

	std::fflush(stdout);
#endif
}

static std::pmr::string CaptureCallstack(std::pmr::memory_resource* memory)
{
	constexpr unsigned int MAX_DEPTH = 32;

	void* callstack[MAX_DEPTH];
	const unsigned int count = RtlCaptureStackBackTrace(0, MAX_DEPTH, callstack, nullptr);

	const unsigned int resultLength = count * ((sizeof(void*) * 2) + 1);

	std::pmr::string result(memory);
	result.resize(resultLength);
	auto it = result.begin();

	for (unsigned int i = 0; i < count; i++)
	{
		for (int j = (sizeof(void*) * 8) - 4; j >= 0; j -= 4)
		{
			*it++ = "0123456789ABCDEF"[(reinterpret_cast<std::uintptr_t>(callstack[i]) >> j) & 0xf];
		}

		*it++ = '\n';
	}

	return result;
}

struct DebugAllocatorMetadataHeap final : public std::pmr::memory_resource
{
	HANDLE heap = {};

	DebugAllocatorMetadataHeap()
	{
		this->heap = HeapCreate(0, 0, 0);
		if (!this->heap)
		{
			Die("%s: HeapCreate failed with error code %u", __FUNCTION__, GetLastError());
		}
	}

	void* do_allocate(std::size_t bytes, std::size_t alignment) override
	{
		void *p = HeapAlloc(this->heap, 0, bytes);
		if (!p)
		{
			Die("%s: HeapAlloc failed", __FUNCTION__);
		}

		TracyAllocN(p, bytes, "DebugAllocatorMetadata");

		return p;
	}

	void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override
	{
		TracyFreeN(p, "DebugAllocatorMetadata");

		if (!HeapFree(this->heap, 0, p))
		{
			Die("%s: HeapFree failed with error code %u", __FUNCTION__, GetLastError());
		}
	}

	bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override
	{
		auto otherHeap = dynamic_cast<const DebugAllocatorMetadataHeap*>(&other);
		if (!otherHeap)
		{
			return false;
		}

		return this->heap == otherHeap->heap;
	}
};

struct DebugAllocator
{
	struct Block
	{
		void* begin = nullptr;
		bool is_allocated = false;
		std::size_t requested_size = 0;
		std::size_t total_size = 0;
		std::pmr::string callstack_allocate;
		std::pmr::string callstack_deallocate;

		explicit Block(std::pmr::memory_resource* memory)
		: callstack_allocate(memory), callstack_deallocate(memory) {}
	};

	DebugAllocatorMetadataHeap metadata_heap;
	std::pmr::unordered_map<void*, Block> blocks{&metadata_heap};
	std::recursive_mutex mutex;
	std::size_t page_size = 0;
	std::size_t alloc_granularity = 0;
	std::size_t next_address = 0x20000000000;

	DebugAllocator()
	{
		SYSTEM_INFO info = {};
		GetSystemInfo(&info);

		this->page_size = info.dwPageSize;
		this->alloc_granularity = info.dwAllocationGranularity;
	}

	void* Allocate(std::size_t size)
	{
		std::pmr::string callstack = CaptureCallstack(&this->metadata_heap);

		std::lock_guard lock(this->mutex);

		void* address = reinterpret_cast<void*>(next_address);

		const std::size_t page_count = (size + this->page_size - 1) / this->page_size;
		const std::size_t total_size = (2 + page_count) * this->page_size;

		const DWORD allocation_type = MEM_COMMIT | MEM_RESERVE;

#ifdef CRYMP_DEBUG_ALLOCATOR_CHECK_READS
		const DWORD protect = PAGE_NOACCESS;
#else
		const DWORD protect = PAGE_READONLY;
#endif

		void* block_begin = VirtualAlloc(address, total_size, allocation_type, protect);
		if (!block_begin)
		{
			Die("%s: VirtualAlloc failed with error code %u", __FUNCTION__, GetLastError());
		}

		this->next_address += (total_size + this->alloc_granularity - 1) & (~(this->alloc_granularity - 1));

		void* ptr = static_cast<unsigned char*>(block_begin) + this->page_size;

		DWORD old_protect;
		if (!VirtualProtect(ptr, size, PAGE_READWRITE, &old_protect))
		{
			Die("%s: VirtualProtect failed with error code %u", __FUNCTION__, GetLastError());
		}

		std::memset(ptr, 0, page_count * this->page_size);

#ifdef CRYMP_DEBUG_ALLOCATOR_OVERFLOW_INSTEAD_OF_UNDERFLOW
		const std::size_t remaining_size = size % this->page_size;
		const std::size_t alignment = (remaining_size > 0) ? this->page_size - remaining_size : 0;

		ptr = static_cast<unsigned char*>(ptr) + alignment;
#endif

		auto [it, added] = this->blocks.emplace(ptr, &this->metadata_heap);
		it->second.begin = block_begin;
		it->second.is_allocated = true;
		it->second.callstack_allocate = std::move(callstack);
		it->second.requested_size = size;
		it->second.total_size = total_size;

		TracyAllocN(block_begin, total_size, "DebugAllocator");

#ifdef CRYMP_DEBUG_ALLOCATOR_VERBOSE
		Log("%04x: Allocate(%zu) -> %p\n", GetCurrentThreadId(), size, ptr);
#endif

		return ptr;
	}

	std::size_t Deallocate(void* ptr)
	{
		std::pmr::string callstack = CaptureCallstack(&this->metadata_heap);

		std::lock_guard lock(this->mutex);

		const auto it = this->blocks.find(ptr);
		if (it == this->blocks.end())
		{
			Die("%s: Invalid pointer (%p)", __FUNCTION__, ptr);
		}

		if (!it->second.is_allocated)
		{
			Die("%s: Detected double-free (%p)", __FUNCTION__, ptr);
		}

		const std::size_t size = it->second.requested_size;

		TracyFreeN(it->second.begin, "DebugAllocator");

		if (!VirtualFree(it->second.begin, 0, MEM_RELEASE))
		{
			Die("%s: VirtualFree failed with error code %u", __FUNCTION__, GetLastError());
		}

		it->second.is_allocated = false;
		it->second.callstack_deallocate = std::move(callstack);

#ifdef CRYMP_DEBUG_ALLOCATOR_VERBOSE
		Log("%04x: Deallocate(%p) -> %zu\n", GetCurrentThreadId(), ptr, size);
#endif

		return size;
	}

	std::size_t GetSize(void* ptr)
	{
		std::lock_guard lock(this->mutex);

		const auto it = this->blocks.find(ptr);
		if (it == this->blocks.end())
		{
			Die("%s: Invalid pointer (%p)", __FUNCTION__, ptr);
		}

		if (!it->second.is_allocated)
		{
			Die("%s: Detected use-after-free (%p)", __FUNCTION__, ptr);
		}

		const std::size_t size = it->second.requested_size;

#ifdef CRYMP_DEBUG_ALLOCATOR_VERBOSE
		Log("%04x: GetSize(%p) -> %zu\n", GetCurrentThreadId(), ptr, size);
#endif

		return size;
	}

	void ProvideHeapInfo(std::FILE* file, void* address)
	{
		if (g_fault_message[0])
		{
			std::fprintf(file, "%s\n", g_fault_message);
		}

		if (!address)
		{
			return;
		}

		std::lock_guard lock(this->mutex);

		const auto to_32bit = [](const void* address) -> const void*
		{
			return reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(address) & 0xFFFFFFFFU);
		};

		const void* address_32bit = (address == to_32bit(address)) ? to_32bit(address) : nullptr;

		for (const auto& [ptr, block] : this->blocks)
		{
			const void* end = static_cast<const uint8_t*>(block.begin) + block.total_size;
			const bool match_32bit = address_32bit
				&& address_32bit >= to_32bit(block.begin) && address_32bit < to_32bit(end);

			if ((address < block.begin || address >= end) && !match_32bit)
			{
				continue;
			}

			std::fprintf(file,
				"%smatch: begin=%p, end=%p, is_allocated=%d, requested_size=%zu, total_size=%zu\n"
				"allocation:\n%s"
				"deallocation:\n%s",
				match_32bit ? "possible " : "",
				block.begin,
				end,
				static_cast<int>(block.is_allocated),
				block.requested_size,
				block.total_size,
				block.callstack_allocate.c_str(),
				block.callstack_deallocate.c_str()
			);
		}
	}
};

static DebugAllocator& GetDebugAlloc()
{
	static DebugAllocator instance;
	return instance;
}

#endif

////////////////////////////////////////////////////////////////////////////////
// Workaround for pointer truncation in 64-bit CryMemoryAllocator allocator
////////////////////////////////////////////////////////////////////////////////

#ifdef BUILD_64BIT

class SafePool
{
public:
	static constexpr std::size_t BLOCK_SIZE = 0x80000;
	static constexpr std::size_t BLOCK_COUNT = 2048;  // 0x80000 * 2048 = 1 GiB should be enough for anyone

	static_assert(BLOCK_SIZE >= sizeof(void*));
	static_assert(BLOCK_COUNT > 0);

private:
	void* m_pool = nullptr;
	std::size_t m_blockCount = 0;
	void* m_freeList = nullptr;
	void* m_freeListLast = nullptr;
	std::mutex m_mutex;

public:
	SafePool()
	{
		// use 0x80000000 .. 0xc0000000 for the pool to avoid interfering with DLL placement
		void* hint = reinterpret_cast<void*>(0x80000000ULL);

		void* pool = VirtualAlloc(hint, BLOCK_SIZE * BLOCK_COUNT, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (!pool)
		{
			return;
		}

		if (pool != hint)
		{
			VirtualFree(pool, 0, MEM_RELEASE);
			return;
		}

		m_pool = pool;

		std::size_t i = 0;
		for (; i < BLOCK_COUNT; i++)
		{
			const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(pool) + (i * BLOCK_SIZE);

			// make sure the whole block is below 4 GB
			if ((address + BLOCK_SIZE) > 0x100000000ULL)
			{
				break;
			}

			void* block = reinterpret_cast<void*>(address);

			if (!m_freeList)
			{
				m_freeList = block;
				m_freeListLast = block;
			}
			else
			{
				*static_cast<void**>(m_freeListLast) = block;
				m_freeListLast = block;
			}
		}

		m_blockCount = i;
		g_stats.safePoolBlocks.store(i, std::memory_order_relaxed);
		g_stats.safePoolFreeBlocks.store(i, std::memory_order_relaxed);
	}

	void* Allocate()
	{
		g_stats.safePoolAllocs.fetch_add(1, std::memory_order_relaxed);

		std::lock_guard lock(m_mutex);

		if (!m_freeList)
		{
			g_stats.safePoolFailedAllocs.fetch_add(1, std::memory_order_relaxed);
			return nullptr;
		}

		void* block = m_freeList;

		if (m_freeList == m_freeListLast)
		{
			m_freeList = nullptr;
			m_freeListLast = nullptr;
		}
		else
		{
			m_freeList = *static_cast<void**>(m_freeList);
		}

		g_stats.safePoolFreeBlocks.fetch_sub(1, std::memory_order_relaxed);

		return block;
	}

	void Deallocate(void* block)
	{
		g_stats.safePoolDeallocs.fetch_add(1, std::memory_order_relaxed);

		std::lock_guard lock(m_mutex);

		if (!m_freeList)
		{
			m_freeList = block;
			m_freeListLast = block;
		}
		else
		{
			*static_cast<void**>(m_freeListLast) = block;
			m_freeListLast = block;
		}

		g_stats.safePoolFreeBlocks.fetch_add(1, std::memory_order_relaxed);
	}

	bool Contains(void* ptr) const
	{
		const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(ptr);
		const std::uintptr_t poolBegin = reinterpret_cast<std::uintptr_t>(m_pool);
		const std::uintptr_t poolEnd = poolBegin + (m_blockCount * BLOCK_SIZE);

		return address >= poolBegin && address < poolEnd && !(address % BLOCK_SIZE);
	}
};

static SafePool* g_safePool;

#endif

////////////////////////////////////////////////////////////////////////////////
// Hooked functions
////////////////////////////////////////////////////////////////////////////////

static void* CryMalloc_internal(std::size_t size, std::size_t& allocatedSize)
{
	allocatedSize = 0;

	if (!size)
	{
		return nullptr;
	}

#ifdef BUILD_64BIT
	if (g_safePool && size == SafePool::BLOCK_SIZE)
	{
		void* ptr = g_safePool->Allocate();
		if (ptr)
		{
			TracyAllocN(ptr, size, "SafePool");
			allocatedSize = size;
		}

		return ptr;
	}
#endif

#if defined(CRYMP_DEBUG_ALLOCATOR_ENABLED)
	void* ptr = GetDebugAlloc().Allocate(size);
#else
	void* ptr = std::calloc(1, size);
#endif

	if (ptr)
	{
		TracyAllocN(ptr, size, "CryMalloc");
		allocatedSize = size;
	}

	return ptr;
}

#ifdef CRYMALLOC_API
#undef CRYMALLOC_API
#define CRYMALLOC_API extern "C"
#endif

CRYMALLOC_API void* CryMalloc(std::size_t size, std::size_t& allocatedSize)
{
	g_stats.mallocCalls.fetch_add(1, std::memory_order_relaxed);

	if (gEnv)
	{
		FUNCTION_PROFILER(gEnv->pSystem, PROFILE_SYSTEM);

		return CryMalloc_internal(size, allocatedSize);
	}
	else
	{
		return CryMalloc_internal(size, allocatedSize);
	}
}

static void* CryRealloc_internal(void* oldPtr, std::size_t newSize, std::size_t& allocatedSize)
{
	if (!oldPtr)
	{
		return CryMalloc_internal(newSize, allocatedSize);
	}

	std::size_t oldSize = 0;

#ifdef BUILD_64BIT
	const bool isSafePool = g_safePool && g_safePool->Contains(oldPtr);

	if (isSafePool)
	{
		oldSize = SafePool::BLOCK_SIZE;
	}
	else
#endif
	{
#if defined(CRYMP_DEBUG_ALLOCATOR_ENABLED)
		oldSize = GetDebugAlloc().GetSize(oldPtr);
#else
		oldSize = _msize(oldPtr);
#endif
	}

	void* newPtr = CryMalloc_internal(newSize, allocatedSize);

	if (newPtr)
	{
		std::memcpy(newPtr, oldPtr, (oldSize <= newSize) ? oldSize : newSize);
	}

#ifdef BUILD_64BIT
	if (isSafePool)
	{
		TracyFreeN(oldPtr, "SafePool");
		g_safePool->Deallocate(oldPtr);
	}
	else
#endif
	{
		TracyFreeN(oldPtr, "CryMalloc");
#if defined(CRYMP_DEBUG_ALLOCATOR_ENABLED)
		GetDebugAlloc().Deallocate(oldPtr);
#else
		std::free(oldPtr);
#endif
	}

	return newPtr;
}

CRYMALLOC_API void* CryRealloc(void* oldPtr, std::size_t newSize, std::size_t& allocatedSize)
{
	g_stats.reallocCalls.fetch_add(1, std::memory_order_relaxed);

	if (gEnv)
	{
		FUNCTION_PROFILER(gEnv->pSystem, PROFILE_SYSTEM);

		return CryRealloc_internal(oldPtr, newSize, allocatedSize);
	}
	else
	{
		return CryRealloc_internal(oldPtr, newSize, allocatedSize);
	}
}

static std::size_t CryGetMemSize_internal(void* ptr)
{
	if (!ptr)
	{
		return 0;
	}

#ifdef BUILD_64BIT
	if (g_safePool && g_safePool->Contains(ptr))
	{
		return SafePool::BLOCK_SIZE;
	}
#endif

#if defined(CRYMP_DEBUG_ALLOCATOR_ENABLED)
	return GetDebugAlloc().GetSize(ptr);
#else
	return _msize(ptr);
#endif
}

CRYMALLOC_API std::size_t CryGetMemSize(void* ptr, std::size_t)
{
	g_stats.sizeCalls.fetch_add(1, std::memory_order_relaxed);

	if (gEnv)
	{
		FUNCTION_PROFILER(gEnv->pSystem, PROFILE_SYSTEM);

		return CryGetMemSize_internal(ptr);
	}
	else
	{
		return CryGetMemSize_internal(ptr);
	}
}

static std::size_t CryFree_internal(void* ptr)
{
	if (!ptr)
	{
		return 0;
	}

#ifdef BUILD_64BIT
	if (g_safePool && g_safePool->Contains(ptr))
	{
		TracyFreeN(ptr, "SafePool");
		g_safePool->Deallocate(ptr);

		return SafePool::BLOCK_SIZE;
	}
#endif

	std::size_t size = 0;

	TracyFreeN(ptr, "CryMalloc");
#if defined(CRYMP_DEBUG_ALLOCATOR_ENABLED)
	size = GetDebugAlloc().Deallocate(ptr);
#else
	size = _msize(ptr);
	std::free(ptr);
#endif

	return size;
}

CRYMALLOC_API std::size_t CryFree(void* ptr)
{
	g_stats.freeCalls.fetch_add(1, std::memory_order_relaxed);

	if (gEnv)
	{
		FUNCTION_PROFILER(gEnv->pSystem, PROFILE_SYSTEM);

		return CryFree_internal(ptr);
	}
	else
	{
		return CryFree_internal(ptr);
	}
}

CRYMALLOC_API void* CrySystemCrtMalloc(std::size_t size)
{
	g_stats.crtMallocCalls.fetch_add(1, std::memory_order_relaxed);

	std::size_t allocatedSize = 0;

	if (gEnv)
	{
		FUNCTION_PROFILER(gEnv->pSystem, PROFILE_SYSTEM);

		return CryMalloc_internal(size, allocatedSize);
	}
	else
	{
		return CryMalloc_internal(size, allocatedSize);
	}
}

CRYMALLOC_API void CrySystemCrtFree(void* ptr)
{
	g_stats.crtFreeCalls.fetch_add(1, std::memory_order_relaxed);

	if (gEnv)
	{
		FUNCTION_PROFILER(gEnv->pSystem, PROFILE_SYSTEM);

		CryFree_internal(ptr);
	}
	else
	{
		CryFree_internal(ptr);
	}
}

////////////////////////////////////////////////////////////////////////////////
// CryMemoryManager
////////////////////////////////////////////////////////////////////////////////

void CryMemoryManager::Init()
{
#ifdef BUILD_64BIT
	g_safePool = new SafePool;
#endif
}

void CryMemoryManager::ProvideHeapInfo(std::FILE* file, void* address)
{
#ifdef CRYMP_DEBUG_ALLOCATOR_ENABLED
	GetDebugAlloc().ProvideHeapInfo(file, address);
#endif
}

void CryMemoryManager::LogInfo()
{
	g_stats.Log();
}
