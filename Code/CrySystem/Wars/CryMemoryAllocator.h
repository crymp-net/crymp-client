#pragma once

#include <cstddef>
#include <cstdlib>

// C++20 compatibility subset of the old Wars CryMemoryAllocator.
// The embedded CrySystem only needs node_alloc for its XML node pool.
enum EAllocFreeType
{
    eCryDefaultMalloc,
    eCryMallocCryFreeAll,
    eCryMallocCryFreeCRTCleanup
};

template <EAllocFreeType AllocType, bool Threads, int PoolSize>
class node_alloc
{
public:
    using value_type = char;

    static void* allocate(std::size_t size)
    {
        (void)AllocType; (void)Threads; (void)PoolSize;
        return std::malloc(size);
    }

    static void* alloc(std::size_t size)
    {
        return allocate(size);
    }

    static void deallocate(void* ptr, std::size_t)
    {
        std::free(ptr);
    }

    static void dealloc(void* ptr, std::size_t size)
    {
        deallocate(ptr, size);
    }

    static std::size_t get_heap_size() { return 0; }
    static std::size_t get_wasted_in_allocation() { return 0; }
    static std::size_t get_wasted_in_blocks() { return 0; }
    static void cleanup() {}
};
