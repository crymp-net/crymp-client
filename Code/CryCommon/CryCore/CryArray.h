// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#ifndef _CRY_ARRAY_H_
#define _CRY_ARRAY_H_
#pragma once

#include "CryMalloc.h"
#include "CryPodArray.h"

//---------------------------------------------------------------------------
// Allocation functors.
struct FModuleAlloc
{
	static void* Alloc( void* oldptr, size_t oldsize, size_t newsize )
	{
		std::size_t allocatedSize = 0;
		return CryRealloc(oldptr, newsize, allocatedSize);
	}
};

//---------------------------------------------------------------------------
// Dynamically growable array container.
// Size and capacity are stored at front of allocated memory, 
// so size of empty array is just a pointer.
// Assumes T is movable.

template<class T, class A = FModuleAlloc>
struct DynArray
{
	// Base storage.
	typedef DynArray<T,A> self_type;
	typedef T						value_type;
	typedef T*					iterator;
	typedef const T*		const_iterator;
	typedef int					size_type;
	typedef T&					reference_type;
	typedef const T&		const_reference_type;

	// Construction.
	inline DynArray()
		: m_aElems(null())
	{}
	inline DynArray(self_type const& a)
		: m_aElems(null())
	{
		resize_raw(a.size());
		init(begin(), end(), a.begin());
	}
	
	self_type& operator=(self_type const& a)
	{
		if (&a != this)
		{
			if (size() != a.size())
			{
				destroy(begin(), end());
				resize_raw(a.size());
				init(begin(), end(), a.begin());
			}
			else
			{
				copy(begin(), end(), a.begin());
			}
		}
		return *this;
	}
	inline ~DynArray()
	{
		clear();
	}

	// Accessors.
	inline bool empty() const								{ return size() == 0; }
	inline size_type size() const						{ return header()->nSize; }
	inline size_type capacity() const				{ return header()->nCapacity; }

	inline T* begin()												{ return m_aElems; }
	inline const T* begin() const						{ return m_aElems; }

	inline value_type front()								{ return *m_aElems; }
	inline const value_type front() const		{ return *m_aElems; }

	inline T* end()													{ return begin()+size(); }
	inline const T* end() const							{ return begin()+size(); }

	inline reference_type back()							{ return *(begin()+(size()-1)); }
	inline const_reference_type back() const	{ return *(begin()+(size()-1)); }

	inline T const& operator [] (int i) const
	{
		assert(i >= 0 && i < (int)size());
		return begin()[i];
	}
	inline T& operator [] (int i)
	{
		assert(i >= 0 && i < (int) size());
		return begin()[i];
	}

	// Mutators.

	void resize(size_type nCount)
	{
		int nOld = size();
		destroy(begin()+nCount, end());
		resize_raw(nCount);
		init(begin()+nOld, end());
	}

	void resize(size_type nCount, const T& value)
	{
		int nOld = size();
		destroy(begin()+nCount, end());
		resize_raw(nCount);
		init(begin()+nOld, end(), value);
	}

	void reserve(size_type nCount)
	{
		if (nCount > capacity())
			resize_raw(size(), nCount);
	}

	iterator insert(iterator at)
	{
		iterator it = insert_raw(at);
		return new(it) T;
	}
	iterator insert(iterator at, T const& val)
	{
		iterator it = insert_raw(at);
		return new(it) T(val);
	}

	inline iterator push_back()									
	{ 
		resize_raw(size()+1);
		return new(end()-1) T;
	}
	inline iterator push_back(T const& val)
	{ 
		resize_raw(size()+1);
		return new(end()-1) T(val);
	}
	inline void pop_back()
	{
		if (!empty())
		{
			erase( size()-1 );
		}
	}

	iterator erase(iterator first, iterator final)
	{
		int pos = first-begin();
		int count = final-first;

		// Destroy, then delete elems.
		destroy(first, final);
		move(first, end() - count, final);

		// Resize mem.
		resize_raw(size() - count);
		return begin()+pos;
	}

	iterator erase(iterator it)
	{
		return erase(it, it+1);
	}

	int erase(int i)
	{
		assert(i >= 0 && i < size());
		erase(begin() + i);
		return i-1;
	}

	int erase( T const& val )
	{
		int count = 0;
		for (iterator it = end()-1; it >= begin(); --it)
		{
			if (*it == val)
			{
				it = erase(it, it+1);
				count++;
			}
		}
		return count;
	}

	void clear()
	{
		if (capacity())
		{
			destroy(begin(), end());
			A::Alloc(header(), get_alloc_size(), 0);
			m_aElems = null();
		}
	}

	void swap( DynArray& other )
	{
		std::swap(m_aElems, other.m_aElems);
	}

  size_t get_alloc_size()
  {
    return capacity() ? (sizeof(Header) + capacity()*sizeof(T)) : 0;
  }

protected:

	T*				m_aElems;

  struct Header
  {
    size_type	nCapacity, nSize;
  };

	static T* null()
	{
		static Header empty = {0,0};
		return (T*)(&empty+1);
	}

	inline Header* header()
	{
		assert(m_aElems);
		return ((Header*)m_aElems)-1;
	}
	inline Header const* header() const
	{
		assert(m_aElems);
		return ((Header*)m_aElems)-1;
	}

	inline void* mem()
	{
		return capacity() ? header() : 0;
	}

	void resize_raw(size_type nCount)
	{
		// Set new size, and automatically determine allocation count.
		if (capacity() == 0)
			// If first allocation, allocate exact size.
			resize_raw(nCount, nCount);
		else if (nCount > capacity())
		{
//PS3workaround -> Remove/ allowed to further load level





			// Grow if needed, to next power-of-two.
			size_type nCap = 4;
			nCap = 4;
			while (nCap < nCount)
				nCap <<= 1;
			resize_raw(nCount, nCap);

		}
		else
		{
			header()->nSize = nCount;
		}
	}

	void resize_raw(size_type nCount, size_type nAlloc)
	{
		// Set new size and allocation count.
		assert(nCount <= nAlloc);
		if (nAlloc != capacity())
		{
			if (nAlloc == 0)
			{
				A::Alloc(mem(), get_alloc_size(), 0);
				m_aElems = null();
			}
			else
			{
				size_t nSize = sizeof(Header) + nAlloc * sizeof(T);
				void* newMem = A::Alloc(mem(), get_alloc_size(), nSize);
				Header* head = (Header*)newMem;
				head->nCapacity = nAlloc;
				m_aElems = (T*)(head+1);
			}
		}
		if (nAlloc > 0)
			header()->nSize = nCount;
	}

	iterator insert_raw(iterator at)
	{
		int pos = at-begin();

		// Realloc, then move elements.
		resize_raw(size()+1);
		move(begin()+pos+1, end(), begin()+pos);
		return begin()+pos;
	}

	void set_size(size_type nCount)
	{
		assert(nCount <= capacity());
		header()->nSize = nCount;
	}

	// Init a range of raw elements to default or copied values.
	static inline void init(T* first, T* final)
	{
		for (; first < final; first++)
			new(first) T;
	}

	static inline void init(T* first, T* final, T const& val)
	{
		for (; first < final; first++)
			new(first) T(val);
	}

	static inline void init(T* first, T* final, T const* src)
	{
		for (; first < final; first++)
			new(first) T(*src++);
	}

	// Copy values to a range of existing elements.
	static inline void copy(T* first, T* final, T const& val)
	{
		for (; first < final; first++)
			*first = val;
	}

	static inline void copy(T* first, T* final, T const* src)
	{
		for (; first < final; first++)
			*first = *src++;
	}

	// Move a range of move-invariant elements.
	static inline void move(T* first, T* final, T const* src)
	{
		memmove(first, src, (size_t)final-(size_t)first);
	}

	// Destroy a range of existing elements.
	static inline void destroy(T* first, T* final)
	{
		for (; first < final; first++)
			first->~T();
	}
  friend struct FModuleAlloc16;
};

// 16 byte alligned allocator
struct FModuleAlloc16
{
  static void* Alloc( void* oldptr, size_t oldsize, size_t newsize )
  {
    unsigned char *bActualPtr = (unsigned char *)oldptr;
    if (bActualPtr)
    {
      bActualPtr -= sizeof(DynArray<char>::Header);
      bActualPtr = ((unsigned char **)bActualPtr)[-1];
    }
    if (newsize)
    {
      std::size_t allocSize = 0;
      unsigned char *pData = (unsigned char *)CryMalloc(newsize+16+sizeof(char *)+sizeof(DynArray<char>::Header), allocSize);
      unsigned char *bPtrRes = (unsigned char *)((ptrdiff_t)(pData+16+sizeof(char *)) & ~0xf);
      ((unsigned char**)bPtrRes)[-1] = pData;
      if (oldptr)
      {
        memcpy(&bPtrRes[sizeof(DynArray<char>::Header)], oldptr, oldsize);
        CryFree(bActualPtr);
      }
      return &bPtrRes[sizeof(DynArray<char>::Header)];
    }
    else if (oldptr)
    {
      CryFree(bActualPtr);
    }
    return NULL;
  }
};


template<class T>
struct DynArray16 : public DynArray<T, FModuleAlloc16>
{
};

#endif
