#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>

#include "CryCommon/CryCore/CryMalloc.h"

extern std::uintptr_t CRYACTION_BASE;

template<class T>
class NodeAlloc_CryAction
{
public:
	T* allocate(std::size_t n)
	{
		using AllocFunc = void* (*)(std::size_t bytes);

		const std::size_t bytes = n * sizeof(T);

#ifdef BUILD_64BIT
		AllocFunc alloc = reinterpret_cast<AllocFunc>(CRYACTION_BASE + 0x34F0);

		void *p = alloc(bytes);
#else
		// _M_allocate instead of allocate
		AllocFunc alloc = reinterpret_cast<AllocFunc>(CRYACTION_BASE + 0x73A0);

		void *p = nullptr;
		if (bytes > 0x200)
		{
			std::size_t allocatedSize = 0;
			p = CryMalloc(bytes, allocatedSize);
		}
		else
		{
			p = alloc(bytes);
		}
#endif

		if (!p)
		{
			std::abort();
		}

		return static_cast<T*>(p);
	}

	void deallocate(T* p, std::size_t n)
	{
		using DeallocFunc = void (*)(void* p, std::size_t bytes);

		const std::size_t bytes = n * sizeof(T);

#ifdef BUILD_64BIT
		DeallocFunc dealloc = reinterpret_cast<DeallocFunc>(CRYACTION_BASE + 0x1A90);

		dealloc(p, n * sizeof(T));
#else
		// _M_deallocate instead of deallocate
		DeallocFunc dealloc = reinterpret_cast<DeallocFunc>(CRYACTION_BASE + 0x6C30);

		if (bytes > 0x200)
		{
			CryFree(p);
		}
		else
		{
			dealloc(p, bytes);
		}
#endif
	}
};

template<class T, class Allocator>
class StlportVector : private Allocator
{
	T* m_begin = nullptr;
	T* m_end = nullptr;
	T* m_end_of_storage = nullptr;

public:
	StlportVector() = default;

	StlportVector(const StlportVector& other)
	{
		this->reserve(other.capacity());

		for (const T& element : other)
		{
			this->push_back(element);
		}
	}

	StlportVector(StlportVector&& other) noexcept
	{
		m_begin = other.m_begin;
		m_end = other.m_end;
		m_end_of_storage = other.m_end_of_storage;

		other.m_begin = nullptr;
		other.m_end = nullptr;
		other.m_end_of_storage = nullptr;
	}

	const StlportVector& operator=(const StlportVector& other)
	{
		// self-assignment is ok
		this->clear();
		this->reserve(other.capacity());

		for (const T& element : other)
		{
			this->push_back(element);
		}

		return *this;
	}

	const StlportVector& operator=(StlportVector&& other) noexcept
	{
		this->release();

		m_begin = other.m_begin;
		m_end = other.m_end;
		m_end_of_storage = other.m_end_of_storage;

		other.m_begin = nullptr;
		other.m_end = nullptr;
		other.m_end_of_storage = nullptr;

		return *this;
	}

	~StlportVector()
	{
		this->release();
	}

	bool empty() const { return m_begin == m_end; }

	std::size_t size() const { return m_end - m_begin; }

	std::size_t capacity() const { return m_end_of_storage - m_begin; }

	      T& operator[](std::size_t index)       { return m_begin[index]; }
	const T& operator[](std::size_t index) const { return m_begin[index]; }

	      T*  begin()       { return m_begin; }
	const T*  begin() const { return m_begin; }
	const T* cbegin() const { return m_begin; }

	      T*  end()       { return m_end; }
	const T*  end() const { return m_end; }
	const T* cend() const { return m_end; }

	void reserve(std::size_t new_capacity)
	{
		if (new_capacity > this->capacity())
		{
			this->reallocate(new_capacity);
		}
	}

	T* insert(T* pos, const T& element)
	{
		const std::size_t index = pos - m_begin;
		this->grow_if_full();
		pos = m_begin + index;

		std::construct_at(m_end);

		for (T* it = m_end; it != pos; --it)
		{
			*it = *(it - 1);
		}

		++m_end;

		*pos = element;

		return pos;
	}

	T* erase(T* pos)
	{
		--m_end;

		for (T* it = pos; it != m_end; ++it)
		{
			*it = *(it + 1);
		}

		std::destroy_at(m_end);

		return pos;
	}

	void push_back(const T& element)
	{
		this->grow_if_full();

		std::construct_at(m_end, element);
		++m_end;
	}

	void clear()
	{
		std::destroy(m_begin, m_end);
		m_end = m_begin;
	}

private:
	void grow_if_full()
	{
		if (m_end == m_end_of_storage)
		{
			this->reallocate((this->capacity() == 0) ? 8 : this->capacity() * 2);
		}
	}

	void reallocate(std::size_t new_capacity)
	{
		const std::size_t count = this->size();

		T* new_buffer = this->allocate(new_capacity);
		std::uninitialized_move(m_begin, m_end, new_buffer);
		this->release();

		m_end_of_storage = new_buffer + new_capacity;
		m_end = new_buffer + count;
		m_begin = new_buffer;
	}

	void release()
	{
		std::destroy(m_begin, m_end);

		if (m_begin)
		{
			this->deallocate(m_begin, this->capacity());
		}
	}
};

template<class T>
using StlportVector_CryAction = StlportVector<T, NodeAlloc_CryAction<T>>;

static_assert(sizeof(StlportVector_CryAction<void*>) == (sizeof(void*) * 3));
