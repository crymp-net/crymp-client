////////////////////////////////////////////////////////////////////////////
//
//  CryEngine Source File.
//  Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
//  File name:   SimpleStringPool.h
//  Created:     21/04/2006 by Timur.
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __SimpleStringPool_h__
#define __SimpleStringPool_h__
#pragma once

#include "CryCommon/CrySystem/ISystem.h"

/////////////////////////////////////////////////////////////////////
// String pool implementation.
// Inspired by expat implementation.
/////////////////////////////////////////////////////////////////////
class CSimpleStringPool
{
public:
	enum { STD_BLOCK_SIZE = 4096 };
	struct BLOCK {
		BLOCK *next;
		int size;
		char s[1];
	};
	unsigned int m_blockSize;
	BLOCK *m_blocks;
	BLOCK *m_free_blocks;
	const char *m_end;
	char *m_ptr;
	char *m_start;
	int nUsedSpace;
	int nUsedBlocks;

	static size_t g_nTotalAllocInXmlStringPools;

	CSimpleStringPool()
	{
		m_blockSize = STD_BLOCK_SIZE;
		m_blocks = 0;
		m_start = 0;
		m_ptr = 0;
		m_end = 0;
		nUsedSpace = 0;
		nUsedBlocks = 0;
		m_free_blocks = 0;
	}
	~CSimpleStringPool()
	{
		BLOCK *pBlock = m_blocks;
		while (pBlock) {
			BLOCK *temp = pBlock->next;
			g_nTotalAllocInXmlStringPools -= (offsetof(BLOCK, s) + pBlock->size*sizeof(char));
			//nFree++;
			free(pBlock);
			pBlock = temp;
		}
		pBlock = m_free_blocks;
		while (pBlock) {
			BLOCK *temp = pBlock->next;
			g_nTotalAllocInXmlStringPools -= (offsetof(BLOCK, s) + pBlock->size*sizeof(char));
			//nFree++;
			free(pBlock);
			pBlock = temp;
		}
		m_blocks = 0;
		m_ptr = 0;
		m_start = 0;
		m_end = 0;
	}
	void SetBlockSize( unsigned int nBlockSize )
	{
		if (nBlockSize > 1024*1024)
			nBlockSize = 1024*1024;
		unsigned int size = 512;
		while (size < nBlockSize)
			size *= 2;

		m_blockSize = size - offsetof(BLOCK, s);
	}
	void Clear()
	{
		if (m_free_blocks)
		{
			BLOCK *pLast = m_blocks;
			while (pLast) {
				BLOCK *temp = pLast->next;
				if (!temp)
					break;
				pLast = temp;
			}
			if (pLast)
				pLast->next = m_free_blocks;
		}
		m_free_blocks = m_blocks;
		m_blocks = 0;
		m_start = 0;
		m_ptr = 0;
		m_end = 0;
		nUsedSpace = 0;
	}
	char *Append( const char *ptr,int nStrLen )
	{
		char *ret = m_ptr;
		if (m_ptr && nStrLen+1 < (m_end - m_ptr))
		{
			memcpy( m_ptr,ptr,nStrLen );
			m_ptr = m_ptr + nStrLen;
			*m_ptr++ = 0; // add null termination.
		}
		else
		{
			int nNewBlockSize = std::max(nStrLen+1,(int)m_blockSize);
			AllocBlock(nNewBlockSize,nStrLen+1);
			memcpy( m_ptr,ptr,nStrLen );
			m_ptr = m_ptr + nStrLen;
			*m_ptr++ = 0; // add null termination.
			ret = m_start;
		}
		nUsedSpace += nStrLen;
		return ret;
	}
	char *ReplaceString( const char *str1,const char *str2 )
	{
		int nStrLen1 = strlen(str1);
		int nStrLen2 = strlen(str2);

		// undo ptr1 add.
		if (m_ptr != m_start)
			m_ptr = m_ptr - nStrLen1 - 1;

		assert( m_ptr == str1 );

		int nStrLen = nStrLen1 + nStrLen2;

		char *ret = m_ptr;
		if (m_ptr && nStrLen+1 < (m_end - m_ptr))
		{
			if(m_ptr != str1)
				memcpy( m_ptr,str1,nStrLen1 );
			memcpy( m_ptr+nStrLen1,str2,nStrLen2 );
			m_ptr = m_ptr + nStrLen;
			*m_ptr++ = 0; // add null termination.
		}
		else
		{
			int nNewBlockSize = std::max(nStrLen+1,(int)m_blockSize);
			if (m_ptr == m_start)
			{
				ReallocBlock(nNewBlockSize*2); // Reallocate current block.
				memcpy( m_ptr+nStrLen1,str2,nStrLen2 );
			}
			else
			{
				AllocBlock(nNewBlockSize,nStrLen+1);
				memcpy( m_ptr,str1,nStrLen1 );
				memcpy( m_ptr+nStrLen1,str2,nStrLen2 );
			}

			m_ptr = m_ptr + nStrLen;
			*m_ptr++ = 0; // add null termination.
			ret = m_start;
		}
		nUsedSpace += nStrLen;
		return ret;
	}
private:
	void AllocBlock( int blockSize,int nMinBlockSize )
	{
		if (m_free_blocks)
		{
			BLOCK *pBlock = m_free_blocks;
			BLOCK *pPrev = 0;
			while (pBlock)
			{
				if (pBlock->size >= nMinBlockSize)
				{
					// Reuse free block
					if (pPrev)
						pPrev->next = pBlock->next;
					else
						m_free_blocks = pBlock->next;

					pBlock->next = m_blocks;
					m_blocks = pBlock;
					m_ptr = pBlock->s;
					m_start = pBlock->s;
					m_end = pBlock->s + pBlock->size;
					return;
				}
				pPrev = pBlock;
				pBlock = pBlock->next;
			}

		}
		size_t nMallocSize = offsetof(BLOCK, s) + blockSize * sizeof(char);
		g_nTotalAllocInXmlStringPools += nMallocSize;
		//nMallocs++;
		BLOCK *pBlock = (BLOCK*)malloc(nMallocSize);
		pBlock->size = blockSize;
		pBlock->next = m_blocks;
		m_blocks = pBlock;
		m_ptr = pBlock->s;
		m_start = pBlock->s;
		m_end = pBlock->s + blockSize;
		nUsedBlocks++;
	}
	void ReallocBlock( int blockSize )
	{
		BLOCK *pThisBlock = m_blocks;
		BLOCK *pPrevBlock = m_blocks->next;
		m_blocks = pPrevBlock;

		size_t nMallocSize = offsetof(BLOCK, s) + blockSize * sizeof(char);
		if (pThisBlock)
		{
			g_nTotalAllocInXmlStringPools -= (offsetof(BLOCK, s) + pThisBlock->size*sizeof(char));
		}
		g_nTotalAllocInXmlStringPools += nMallocSize;

		//nMallocs++;
		BLOCK *pBlock = (BLOCK*)realloc(pThisBlock,nMallocSize);
		pBlock->size = blockSize;
		pBlock->next = m_blocks;
		m_blocks = pBlock;
		m_ptr = pBlock->s;
		m_start = pBlock->s;
		m_end = pBlock->s + blockSize;
	}
};


#endif // __SimpleStringPool_h__