#include "StdAfx.h"
#include "CryCommon/CryCore/smartptr.h"
#include "MTSafeAllocator.h"
#include "ZipFileFormat.h"
#include "ZipDirStructures.h"
#include "ZipDirTree.h"
#include "ZipDirList.h"
#include "ZipDirCache.h"
#include "ZipDirCacheRW.h"
#include "ZipDirCacheFactory.h"
#include "ZipDirFindRW.h"
#include "CryCommon/CrySystem/ICryPak.h"

// declaration of Z_OK for ZipRawDecompress
#include "zlib/zlib.h"

using namespace ZipFile;

//////////////////////////////////////////////////////////////////////////
void ZipDir::CacheRW::AddRef()
{
	CryInterlockedIncrement(&m_nRefCount);
}

//////////////////////////////////////////////////////////////////////////
void ZipDir::CacheRW::Release()
{
	if (CryInterlockedDecrement(&m_nRefCount) <= 0)
		delete this;
}

void ZipDir::CacheRW::Close()
{
	if (m_pFile)
	{
		if (!(m_nFlags & FLAGS_READ_ONLY))
		{
			if ((m_nFlags & FLAGS_UNCOMPACTED) && !(m_nFlags&FLAGS_DONT_COMPACT))
			{
				if (!RelinkZip())
					WriteCDR();
			}
			else
			if (m_nFlags & FLAGS_CDR_DIRTY)
				WriteCDR();
		}

		if(m_pFile)						// RelinkZip() might have closed the file
			fclose (m_pFile);

		m_pFile = NULL;
	}
	m_pHeap = NULL;
	m_treeDir.Clear();
}

//////////////////////////////////////////////////////////////////////////
char* ZipDir::CacheRW::UnifyPath( const char *pPath )
{
	static char str[_MAX_PATH];
	const char *src = pPath;
	char *trg = str;
	while (*src)
	{
		if (*src != '/')
		{
			*trg++ = ::tolower(*src++);
		}
		else
		{
			*trg++ = '\\';
			src++;
		}
	}
	*trg = 0;
	return str;
}

//////////////////////////////////////////////////////////////////////////
char* ZipDir::CacheRW::AllocPath( const char *pPath )
{
	char *temp = UnifyPath(pPath);
	temp = m_tempStringPool.Append( temp,strlen(temp) );
	return temp;
}

// Adds a new file to the zip or update an existing one
// adds a directory (creates several nested directories if needed)
ZipDir::ErrorEnum ZipDir::CacheRW::UpdateFile (const char* szRelativePathSrc, void* pUncompressed, unsigned nSize, unsigned nCompressionMethod, int nCompressionLevel)
{
	char *szRelativePath = UnifyPath(szRelativePathSrc);

	SmartPtr pBufferDestroyer(m_pHeap);

	// we'll need the compressed data
	void* pCompressed;
	unsigned long nSizeCompressed;
	int nError;

	switch (nCompressionMethod)
	{
	case METHOD_DEFLATE:
		// allocate memory for compression. Min is nSize * 1.001 + 12
		nSizeCompressed = nSize + (nSize >> 3) + 32;
		pCompressed = m_pHeap->Alloc(nSizeCompressed, "ZipDir::CacheRW::UpdateFile");
		pBufferDestroyer.Attach(pCompressed);
		nError = ZipRawCompress(m_pHeap, pUncompressed, &nSizeCompressed, pCompressed, nSize, nCompressionLevel);
		if (Z_OK != nError)
			return ZD_ERROR_ZLIB_FAILED;
		break;
		
	case METHOD_STORE:
		pCompressed = pUncompressed;
		nSizeCompressed = nSize;
		break;

	default:
		return ZD_ERROR_UNSUPPORTED;
	}

	// create or find the file entry.. this object will rollback (delete the object
	// if the operation fails) if needed.
	FileEntryTransactionAdd pFileEntry(this, AllocPath(szRelativePathSrc));

	if (!pFileEntry)
		return ZD_ERROR_INVALID_PATH;

	pFileEntry->OnNewFileData (pUncompressed, nSize, nSizeCompressed, nCompressionMethod, false);
	// since we changed the time, we'll have to update CDR
	m_nFlags |= FLAGS_CDR_DIRTY;

	// the new CDR position, if the operation completes successfully
	unsigned lNewCDROffset = m_lCDROffset;

	if (pFileEntry->IsInitialized())
	{
		// this file entry is already allocated in CDR

		// check if the new compressed data fits into the old place
		unsigned nFreeSpace = pFileEntry->nEOFOffset - pFileEntry->nFileHeaderOffset - (unsigned)sizeof(ZipFile::LocalFileHeader) - (unsigned)strlen(szRelativePath);

		if (nFreeSpace != nSizeCompressed)
			m_nFlags |= FLAGS_UNCOMPACTED;

		if (nFreeSpace >= nSizeCompressed)
		{
			// and we can just override the compressed data in the file
			ErrorEnum e = WriteLocalHeader(m_pFile, pFileEntry, szRelativePath);
			if (e != ZD_ERROR_SUCCESS)
				return e;
		}
		else
		{
			// we need to write the file anew - in place of current CDR
			pFileEntry->nFileHeaderOffset = m_lCDROffset;
			ErrorEnum e = WriteLocalHeader(m_pFile, pFileEntry, szRelativePath);
			lNewCDROffset = pFileEntry->nEOFOffset;
			if (e != ZD_ERROR_SUCCESS)
				return e;
		}
	}
	else
	{
		pFileEntry->nFileHeaderOffset = m_lCDROffset;
		ErrorEnum e = WriteLocalHeader(m_pFile, pFileEntry, szRelativePath);
		if (e != ZD_ERROR_SUCCESS)
			return e;

		lNewCDROffset = pFileEntry->nFileDataOffset + nSizeCompressed;

		m_nFlags |= FLAGS_CDR_DIRTY;
	}

	// now we have the fresh local header and data offset

#ifdef WIN32
	if (_fseeki64 (m_pFile, (__int64)pFileEntry->nFileDataOffset, SEEK_SET)!=0)
#else
  if (fseek (m_pFile, pFileEntry->nFileDataOffset, SEEK_SET)!=0)
#endif
	{
		return ZD_ERROR_IO_FAILED;
	}
	// Danny - changed this because writing a single large chunk (more than 6MB?) causes
	// Windows fwrite to (silently?!) fail. If it's big then break up the write into
	// chunks
	const unsigned long maxChunk = 1 << 20;
	unsigned long sizeLeft = nSizeCompressed;
	unsigned long sizeToWrite;
	char * ptr = (char *) pCompressed;
	while ( (sizeToWrite = min(sizeLeft, maxChunk)) != 0 )
	{
		if (fwrite (ptr, sizeToWrite, 1, m_pFile) != 1)
		{
			CryWarning( VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR,
				"Cannot write to zip file!! error = (%d): %s", errno, strerror(errno));
			return ZD_ERROR_IO_FAILED;
		}
		ptr += sizeToWrite;
		sizeLeft -= sizeToWrite;
	}


//	if (fseek (m_pFile, pFileEntry->nFileDataOffset, SEEK_SET)!=0
//		||fwrite (pCompressed, nSizeCompressed, 1, m_pFile) != 1)
//	if (fseek (m_pFile, pFileEntry->nFileDataOffset, SEEK_SET)!=0
//		||fwrite (pCompressed, 1, nSizeCompressed, m_pFile) != nSizeCompressed)
//	{
		// we need to rollback the transaction: file write failed in the middle, the old
		// data is unrecoverably damaged, and we need to destroy this file entry
		// the CDR is already marked dirty
//		return ZD_ERROR_IO_FAILED;
//	}

	// since we wrote the file successfully, update the new CDR position
	m_lCDROffset = lNewCDROffset;
	pFileEntry.Commit();

	return ZD_ERROR_SUCCESS;
}

//   Adds a new file to the zip or update an existing one if it is not compressed - just stored  - start a big file
ZipDir::ErrorEnum ZipDir::CacheRW::StartContinuousFileUpdate( const char* szRelativePathSrc, unsigned nSize )
{
	char *szRelativePath = UnifyPath(szRelativePathSrc);

	SmartPtr pBufferDestroyer(m_pHeap);

	// create or find the file entry.. this object will rollback (delete the object
	// if the operation fails) if needed.
	FileEntryTransactionAdd pFileEntry(this, AllocPath(szRelativePathSrc));

	if (!pFileEntry)
		return ZD_ERROR_INVALID_PATH;

	pFileEntry->OnNewFileData (NULL, nSize, nSize, METHOD_STORE, false );
	// since we changed the time, we'll have to update CDR
	m_nFlags |= FLAGS_CDR_DIRTY;

	// the new CDR position, if the operation completes successfully
	unsigned lNewCDROffset = m_lCDROffset;
	if (pFileEntry->IsInitialized())
	{
		// check if the new compressed data fits into the old place
		unsigned nFreeSpace = pFileEntry->nEOFOffset - pFileEntry->nFileHeaderOffset - (unsigned)sizeof(ZipFile::LocalFileHeader) - (unsigned)strlen(szRelativePath);

		if (nFreeSpace != nSize)
			m_nFlags |= FLAGS_UNCOMPACTED;

		if (nFreeSpace >= nSize)
		{
			// and we can just override the compressed data in the file
			ErrorEnum e = WriteLocalHeader(m_pFile, pFileEntry, szRelativePath);
			if (e != ZD_ERROR_SUCCESS)
				return e;
		}
		else
		{
			// we need to write the file anew - in place of current CDR
			pFileEntry->nFileHeaderOffset = m_lCDROffset;
			ErrorEnum e = WriteLocalHeader(m_pFile, pFileEntry, szRelativePath);
			lNewCDROffset = pFileEntry->nEOFOffset;
			if (e != ZD_ERROR_SUCCESS)
				return e;
		}
	}
	else
	{
		pFileEntry->nFileHeaderOffset = m_lCDROffset;
		ErrorEnum e = WriteLocalHeader(m_pFile, pFileEntry, szRelativePath);
		if (e != ZD_ERROR_SUCCESS)
			return e;

		lNewCDROffset = pFileEntry->nFileDataOffset + nSize;

		m_nFlags |= FLAGS_CDR_DIRTY;
	}

#ifdef WIN32
	if (_fseeki64 (m_pFile, (__int64)pFileEntry->nFileDataOffset, SEEK_SET)!=0)
#else
  if (fseek (m_pFile, pFileEntry->nFileDataOffset, SEEK_SET)!=0)
#endif
	{
		return ZD_ERROR_IO_FAILED;
	}
	// Danny - changed this because writing a single large chunk (more than 6MB?) causes
	// Windows fwrite to (silently?!) fail. If it's big then break up the write into
	// chunks
	const unsigned long maxChunk = 1 << 20;
	unsigned long sizeLeft = nSize;
	unsigned long sizeToWrite;
	char * ptr = new char[maxChunk];
	if( NULL == ptr )
		return ZD_ERROR_IO_FAILED;

	memset(ptr,0,sizeof(char)*maxChunk);

	while ( (sizeToWrite = min(sizeLeft, maxChunk)) != 0 )
	{
		if (fwrite (ptr, sizeToWrite, 1, m_pFile) != 1)
		{
			CryWarning( VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR,
				"Cannot write to zip file!! error = (%d): %s", errno, strerror(errno));
			return ZD_ERROR_IO_FAILED;
		}
		sizeLeft -= sizeToWrite;
	}
	
	delete [] ptr;


	pFileEntry->nEOFOffset = pFileEntry->nFileDataOffset;

	// since we wrote the file successfully, update the new CDR position
	m_lCDROffset = lNewCDROffset;
	pFileEntry.Commit();

	return ZD_ERROR_SUCCESS;
}

// Adds a new file to the zip or update an existing's segment if it is not compressed - just stored 
// adds a directory (creates several nested directories if needed)
ZipDir::ErrorEnum ZipDir::CacheRW::UpdateFileContinuousSegment (const char* szRelativePathSrc, unsigned nSize, void* pUncompressed, unsigned nSegmentSize, unsigned nOverwriteSeekPos )
{
	char *szRelativePath = UnifyPath(szRelativePathSrc);

	SmartPtr pBufferDestroyer(m_pHeap);

	// create or find the file entry.. this object will rollback (delete the object
	// if the operation fails) if needed.
	FileEntryTransactionAdd pFileEntry(this, AllocPath(szRelativePathSrc));

	if (!pFileEntry)
		return ZD_ERROR_INVALID_PATH;

	pFileEntry->OnNewFileData (pUncompressed, nSegmentSize, nSegmentSize, METHOD_STORE, true);
	// since we changed the time, we'll have to update CDR
	m_nFlags |= FLAGS_CDR_DIRTY;

	// this file entry is already allocated in CDR
	unsigned lSegmentOffset = pFileEntry->nEOFOffset;

#ifdef WIN32
	if (_fseeki64 (m_pFile, (__int64)pFileEntry->nFileHeaderOffset, SEEK_SET)!=0)
#else
  if (fseek (m_pFile, pFileEntry->nFileHeaderOffset, SEEK_SET)!=0)
#endif
		return ZD_ERROR_IO_FAILED;

	// and we can just override the compressed data in the file
	ErrorEnum e = WriteLocalHeader(m_pFile, pFileEntry, szRelativePath);
	if (e != ZD_ERROR_SUCCESS)
		return e;

	if(nOverwriteSeekPos!=0xffffffff)
		lSegmentOffset = pFileEntry->nFileDataOffset + nOverwriteSeekPos;

	// now we have the fresh local header and data offset
#ifdef WIN32
	if (_fseeki64 (m_pFile, (__int64)lSegmentOffset, SEEK_SET)!=0)
#else
  if (fseek (m_pFile, lSegmentOffset, SEEK_SET)!=0)
#endif
		return ZD_ERROR_IO_FAILED;

	// Danny - changed this because writing a single large chunk (more than 6MB?) causes
	// Windows fwrite to (silently?!) fail. If it's big then break up the write into
	// chunks
	const unsigned long maxChunk = 1 << 20;
	unsigned long sizeLeft = nSegmentSize;
	unsigned long sizeToWrite;
	char * ptr = (char *) pUncompressed;
	while ( (sizeToWrite = min(sizeLeft, maxChunk)) != 0 )
	{
		if (fwrite (ptr, sizeToWrite, 1, m_pFile) != 1)
		{
			CryWarning( VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR,
				"Cannot write to zip file!! error = (%d): %s", errno, strerror(errno));
			return ZD_ERROR_IO_FAILED;
		}
		ptr += sizeToWrite;
		sizeLeft -= sizeToWrite;
	}

	if(nOverwriteSeekPos==0xffffffff)
		pFileEntry->nEOFOffset = lSegmentOffset + nSegmentSize;

	// since we wrote the file successfully, update CDR
	pFileEntry.Commit();
	return ZD_ERROR_SUCCESS;
}


ZipDir::ErrorEnum ZipDir::CacheRW::UpdateFileCRC (const char* szRelativePathSrc, unsigned dwCRC32 )
{
	char *szRelativePath = UnifyPath(szRelativePathSrc);

	SmartPtr pBufferDestroyer(m_pHeap);

	// create or find the file entry.. this object will rollback (delete the object
	// if the operation fails) if needed.
	FileEntryTransactionAdd pFileEntry(this, AllocPath(szRelativePathSrc));

	if (!pFileEntry)
		return ZD_ERROR_INVALID_PATH;

	// since we changed the time, we'll have to update CDR
	m_nFlags |= FLAGS_CDR_DIRTY;

	pFileEntry->desc.lCRC32=dwCRC32;

#ifdef WIN32
	if (_fseeki64 (m_pFile, (__int64)pFileEntry->nFileHeaderOffset, SEEK_SET)!=0)
#else
  if (fseek (m_pFile, pFileEntry->nFileHeaderOffset, SEEK_SET)!=0)
#endif
		return ZD_ERROR_IO_FAILED;

	// and we can just override the compressed data in the file
	ErrorEnum e = WriteLocalHeader(m_pFile, pFileEntry, szRelativePath);
	if (e != ZD_ERROR_SUCCESS)
		return e;

	// since we wrote the file successfully, update
	pFileEntry.Commit();
	return ZD_ERROR_SUCCESS;
}


// deletes the file from the archive
ZipDir::ErrorEnum ZipDir::CacheRW::RemoveFile (const char* szRelativePathSrc)
{
	char *szRelativePath = UnifyPath(szRelativePathSrc);

	// find the last slash in the path
	const char* pSlash = max(strrchr(szRelativePath, '/'), strrchr(szRelativePath, '\\'));

	const char* pFileName; // the name of the file to delete

	FileEntryTree* pDir; // the dir from which the subdir will be deleted

	if (pSlash)
	{
		FindDirRW fd (GetRoot());
		// the directory to remove
		pDir = fd.FindExact(string (szRelativePath, pSlash-szRelativePath).c_str());
		if (!pDir)
			return ZD_ERROR_DIR_NOT_FOUND;// there is no such directory
		pFileName = pSlash+1;
	}
	else
	{
		pDir = GetRoot();
		pFileName = szRelativePath;
	}

	ErrorEnum e = pDir->RemoveFile (pFileName);
	if (e == ZD_ERROR_SUCCESS)
		m_nFlags |= FLAGS_UNCOMPACTED|FLAGS_CDR_DIRTY;
	return e;
}


// deletes the directory, with all its descendants (files and subdirs)
ZipDir::ErrorEnum ZipDir::CacheRW::RemoveDir (const char* szRelativePathSrc)
{
	char *szRelativePath = UnifyPath(szRelativePathSrc);

	// find the last slash in the path
	const char* pSlash = max(strrchr(szRelativePath, '/'), strrchr(szRelativePath, '\\'));
	
	const char* pDirName; // the name of the dir to delete
	
	FileEntryTree* pDir; // the dir from which the subdir will be deleted

	if (pSlash)
	{
		FindDirRW fd (GetRoot());
		// the directory to remove
		pDir = fd.FindExact(string (szRelativePath, pSlash-szRelativePath).c_str());
		if (!pDir)
			return ZD_ERROR_DIR_NOT_FOUND;// there is no such directory
		pDirName = pSlash+1;
	}
	else
	{
		pDir = GetRoot();
		pDirName = szRelativePath;
	}

	ErrorEnum e = pDir->RemoveDir (pDirName);
	if (e == ZD_ERROR_SUCCESS)
		m_nFlags |= FLAGS_UNCOMPACTED|FLAGS_CDR_DIRTY;
	return e;
}

// deletes all files and directories in this archive
ZipDir::ErrorEnum ZipDir::CacheRW::RemoveAll()
{
	ErrorEnum e = m_treeDir.RemoveAll();
	if (e == ZD_ERROR_SUCCESS)
		m_nFlags |= FLAGS_UNCOMPACTED|FLAGS_CDR_DIRTY;
	return e;
}

ZipDir::ErrorEnum ZipDir::CacheRW::ReadFile (FileEntry* pFileEntry, void* pCompressed, void* pUncompressed)
{
	if (!pFileEntry)
		return ZD_ERROR_INVALID_CALL;

	if (pFileEntry->desc.lSizeUncompressed == 0)
	{
		assert (pFileEntry->desc.lSizeCompressed == 0);
		return ZD_ERROR_SUCCESS;
	}

	assert (pFileEntry->desc.lSizeCompressed > 0);

	ErrorEnum nError = Refresh(pFileEntry);
	if (nError != ZD_ERROR_SUCCESS)
		return nError;

#ifdef WIN32
	if (_fseeki64 (m_pFile, (__int64)pFileEntry->nFileDataOffset, SEEK_SET))
#else
  if (fseek (m_pFile, pFileEntry->nFileDataOffset, SEEK_SET))
#endif
		return ZD_ERROR_IO_FAILED;

	SmartPtr pBufferDestroyer(m_pHeap);

	void* pBuffer = pCompressed; // the buffer where the compressed data will go

	if (pFileEntry->nMethod == 0 && pUncompressed)
	{
		// we can directly read into the uncompress buffer
		pBuffer = pUncompressed;
	}

	if (!pBuffer)
	{
		if (!pUncompressed)
			// what's the sense of it - no buffers at all?
			return ZD_ERROR_INVALID_CALL;

		pBuffer = m_pHeap->Alloc(pFileEntry->desc.lSizeCompressed, "ZipDir::Cache::ReadFile");
		pBufferDestroyer.Attach(pBuffer); // we want it auto-freed once we return
	}

	if (fread (pBuffer, pFileEntry->desc.lSizeCompressed, 1, m_pFile) != 1)
		return ZD_ERROR_IO_FAILED;

	// if there's a buffer for uncompressed data, uncompress it to that buffer
	if (pUncompressed)
	{
		if (pFileEntry->nMethod == 0)
		{
			assert (pBuffer == pUncompressed);
			//assert (pFileEntry->nSizeCompressed == pFileEntry->nSizeUncompressed);
			//memcpy (pUncompressed, pBuffer, pFileEntry->nSizeCompressed);
		}
		else
		{
			unsigned long nSizeUncompressed = pFileEntry->desc.lSizeUncompressed;
			if (Z_OK != ZipRawUncompress(m_pHeap, pUncompressed, &nSizeUncompressed, pBuffer, pFileEntry->desc.lSizeCompressed))
				return ZD_ERROR_CORRUPTED_DATA;
		}
	}

	return ZD_ERROR_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
// finds the file by exact path
ZipDir::FileEntry* ZipDir::CacheRW::FindFile (const char* szPathSrc, bool bFullInfo)
{
	char *szPath = UnifyPath(szPathSrc);

	if (!this)
		return NULL;
	ZipDir::FindFileRW fd (GetRoot());
	if (!fd.FindExact(szPath))
	{
		assert (!fd.GetFileEntry());
		return NULL;
	}
	assert (fd.GetFileEntry());
	return fd.GetFileEntry();
}

// returns the size of memory occupied by the instance referred to by this cache
size_t ZipDir::CacheRW::GetSize()const
{
	return sizeof(*this) + m_strFilePath.capacity() + m_treeDir.GetSize() - sizeof(m_treeDir);
}

// refreshes information about the given file entry into this file entry
ZipDir::ErrorEnum ZipDir::CacheRW::Refresh (FileEntry* pFileEntry)
{
	if (!pFileEntry)
		return ZD_ERROR_INVALID_CALL;

	if (pFileEntry->nFileDataOffset != pFileEntry->INVALID_DATA_OFFSET)
		return ZD_ERROR_SUCCESS; // the data offset has been successfully read..

	if (!this)
		return ZD_ERROR_INVALID_CALL; // from which cache is this file entry???

	return ZipDir::Refresh(m_pFile, pFileEntry);
}


// writes the CDR to the disk
bool ZipDir::CacheRW::WriteCDR(FILE* fTarget)
{
	if (!fTarget)
		return false;

#ifdef WIN32
	if (_fseeki64(fTarget, (__int64)m_lCDROffset, SEEK_SET))
#else
  if (fseek(fTarget, m_lCDROffset, SEEK_SET))
#endif
		return false;

	FileRecordList arrFiles(GetRoot());
	//arrFiles.SortByFileOffset();
	size_t nSizeCDR = arrFiles.GetStats().nSizeCDR;
	void* pCDR = m_pHeap->Alloc(nSizeCDR, "ZipDir::CacheRW::WriteCDR");
  size_t nSizeCDRSerialized = arrFiles.MakeZipCDR(m_lCDROffset,pCDR);
	assert (nSizeCDRSerialized == nSizeCDR);
	size_t nWriteRes = fwrite (pCDR, nSizeCDR, 1, fTarget);
	m_pHeap->Free(pCDR);
	return nWriteRes == 1;
}

// generates random file name
string ZipDir::CacheRW::GetRandomName(int nAttempt)
{
	if (nAttempt)
	{
		char szBuf[8];
		int i;
		for (i = 0; i < sizeof(szBuf)-1;++i)
		{
			int r = rand()%(10 + 'z' - 'a' + 1);
			szBuf[i] = r > 9 ? (r-10)+'a' : '0' + r;
		}
		szBuf[i] = '\0';
		return szBuf;
	}
	else
		return string();
}

bool ZipDir::CacheRW::RelinkZip()
{
	for (int nAttempt = 0; nAttempt < 32; ++nAttempt)
	{
		string strNewFilePath = m_strFilePath + "$" + GetRandomName(nAttempt);
		if (CryGetFileAttributes (strNewFilePath.c_str()) != -1)
			continue; //  we don't want to overwrite the old temp files for safety reasons

		FILE* f = fopen (strNewFilePath.c_str(), "wb");
		if (f)
		{
			bool bOk = RelinkZip(f);
			fclose (f); // we don't need the temporary file handle anyway

			if (!bOk)
			{
				// we don't need the temporary file
				DeleteFile(strNewFilePath.c_str());
				return false;
			}

			// we successfully relinked, now copy the temporary file to the original file
			fclose (m_pFile);
			m_pFile = NULL;

			DeleteFile(m_strFilePath.c_str());
			if (MoveFile(strNewFilePath.c_str(), m_strFilePath.c_str()) == 0)
			{
				// successfully renamed - reopen
				m_pFile = fopen (m_strFilePath.c_str(), "r+b");
				return m_pFile == NULL;
			}
			else
			{
				// could not rename
				
				//m_pFile = fopen (strNewFilePath.c_str(), "r+b");
				return false;
			}
		}
	}

	// couldn't open temp file
	return false; 
}

bool ZipDir::CacheRW::RelinkZip(FILE* fTmp)
{
	FileRecordList arrFiles(GetRoot());
	arrFiles.SortByFileOffset();
	FileRecordList::ZipStats Stats = arrFiles.GetStats();

	// we back up our file entries, because we'll need to restore them
	// in case the operation fails
	std::vector<FileEntry> arrFileEntryBackup;
	arrFiles.Backup (arrFileEntryBackup);

	// this is the set of files that are to be written out - compressed data and the file record iterator
	std::vector<FileDataRecordPtr> queFiles;
	queFiles.reserve (g_nMaxItemsRelinkBuffer);

	// the total size of data in the queue
	unsigned nQueueSize = 0;

	for (FileRecordList::iterator it = arrFiles.begin(); it != arrFiles.end(); ++it)
	{
		// find the file data offset
		if (ZD_ERROR_SUCCESS != Refresh (it->pFileEntry))
			return false;

		// go to the file data
#ifdef WIN32
		if (_fseeki64 (m_pFile, (__int64)it->pFileEntry->nFileDataOffset, SEEK_SET) != 0)
#else
    if (fseek (m_pFile, it->pFileEntry->nFileDataOffset, SEEK_SET) != 0)
#endif
			return false;

		// allocate memory for the file compressed data
		FileDataRecordPtr pFile = FileDataRecord::New (*it, m_pHeap);

		if(!pFile)
			return false;

		// read the compressed data
		if (it->pFileEntry->desc.lSizeCompressed && fread (pFile->GetData(), it->pFileEntry->desc.lSizeCompressed, 1, m_pFile) != 1)
			return false;

		// put the file into the queue for copying (writing)
		queFiles.push_back(pFile);
		nQueueSize += it->pFileEntry->desc.lSizeCompressed;

		// if the queue is big enough, write it out
		if(nQueueSize > g_nSizeRelinkBuffer || queFiles.size() >= g_nMaxItemsRelinkBuffer)
		{
			nQueueSize = 0;
			if (!WriteZipFiles(queFiles, fTmp))
				return false;
		}
	}

	if (!WriteZipFiles(queFiles, fTmp))
		return false;

	ZipFile::ulong lOldCDROffset = m_lCDROffset;
	// the file data has now been written out. Now write the CDR
#ifdef WIN32
  m_lCDROffset = (ZipFile::ulong)_ftelli64(fTmp);
#else
  m_lCDROffset = ftell(fTmp);
#endif
	if (m_lCDROffset >= 0 && WriteCDR(fTmp) && 0 == fflush (fTmp))
		// the new file positions are already there - just discard the backup and return
		return true;
	// recover from backup
	arrFiles.Restore (arrFileEntryBackup);
	m_lCDROffset = lOldCDROffset;
	return false;
}

// writes out the file data in the queue into the given file. Empties the queue
bool ZipDir::CacheRW::WriteZipFiles(std::vector<FileDataRecordPtr>& queFiles, FILE* fTmp)
{
	for (std::vector<FileDataRecordPtr>::iterator it = queFiles.begin(); it != queFiles.end(); ++it)
	{
		// set the new header offset to the file entry - we won't need it
#ifdef WIN32
		(*it)->pFileEntry->nFileHeaderOffset = (unsigned long)_ftelli64 (fTmp);
#else
    (*it)->pFileEntry->nFileHeaderOffset = ftell (fTmp);
#endif

		// while writing the local header, the data offset will also be calculated
		if (ZD_ERROR_SUCCESS != WriteLocalHeader(fTmp, (*it)->pFileEntry, (*it)->strPath.c_str()))
			return false;;

		// write the compressed file data
		if ((*it)->pFileEntry->desc.lSizeCompressed && fwrite ((*it)->GetData(), (*it)->pFileEntry->desc.lSizeCompressed, 1, fTmp) != 1)
			return false;

#ifdef WIN32
    assert ((*it)->pFileEntry->nEOFOffset == (unsigned long)_ftelli64 (fTmp));
#else
    assert ((*it)->pFileEntry->nEOFOffset == ftell (fTmp));
#endif
	}
	queFiles.clear();
	queFiles.reserve (g_nMaxItemsRelinkBuffer);
	return true;
}