
//////////////////////////////////////////////////////////////////////
//
//	Crytek CryENGINE Source code
//
//	File:CryPak.cpp
//  Description: Implementation of the Crytek package files management
//
//	History:
//	-Jan 31,2001:Created by Honich Andrey
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CryPak.h"
#include "CryCommon/CrySystem/ILog.h"
#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CrySystem/ITimer.h"
#include "zlib/zlib.h"				// crc32()
#include "md5.h"
#if defined(PS3) || defined(LINUX)
	#include "System.h"
	#include <sys/stat.h>
	#include <unistd.h>
//#define fileno(X) ((X)->_Handle)
#endif

/////////////////////////////////////////////////////

#define EDITOR_DATA_FOLDER "editor"

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#endif //WIN32

#if defined(PS3) && !defined(__SPU__)
#include <cell/cell_fs.h>
#endif

extern CMTSafeHeap* g_pPakHeap;

//default values for pak vars
static const PakVars g_PakVars;

#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES ((unsigned int)-1)
#endif


//////////////////////////////////////////////////////////////////////////
// IResourceList implementation class.
//////////////////////////////////////////////////////////////////////////
class CResourceList : public IResourceList
{
public:
	CResourceList() { m_iter = m_set.end(); };
	~CResourceList() {};

	string UnifyFilename( const char *sResourceFile )
	{
		string filename = sResourceFile;
		filename.replace( '\\','/' );
		filename.MakeLower();
		return filename;
	}

	virtual void Add( const char *sResourceFile )
	{
		string filename = UnifyFilename(sResourceFile);
		m_set.insert(filename);
	}
	virtual void Clear()
	{
		m_set.clear();
	}
	virtual bool IsExist( const char *sResourceFile )
	{
		string filename = UnifyFilename(sResourceFile);
		if (m_set.find(filename) != m_set.end())
			return true;
		return false;
	}
	virtual void Load( const char *sResourceListFilename )
	{
		CCryFile file;
		if (file.Open( sResourceListFilename,"rb" ))
		{
			char *buf = new char[file.GetLength()];
			file.ReadRaw( buf,file.GetLength() );

			// Parse file, every line in a file represents a resource filename.
			char seps[] = "\r\n";
			char *token = strtok( buf, seps );
			while (token != NULL)
			{
				Add(token);
				token = strtok( NULL, seps );
			}
			delete []buf;
		}
	}
	virtual const char* GetFirst()
	{
		m_iter = m_set.begin();
		if (m_iter != m_set.end())
			return *m_iter;
		return NULL;
	}
	virtual const char* GetNext()
	{
		if (m_iter != m_set.end())
		{
			m_iter++;
			if (m_iter != m_set.end())
				return *m_iter;
		}
		return NULL;
	}

private:
	typedef std::set<string> ResourceSet;
	ResourceSet m_set;
	ResourceSet::iterator m_iter;
};

//////////////////////////////////////////////////////////////////////////
class CNextLevelResourceList : public IResourceList
{
public:
	CNextLevelResourceList() {};
	~CNextLevelResourceList() {};

	const char* UnifyFilename( const char *sResourceFile )
	{
		static char sFile[256];
		int len = min( (int)strlen(sResourceFile),(int)sizeof(sFile)-1 );
		int i;
		for (i = 0; i < len; i++)
		{
			if (sResourceFile[i] != '\\')
				sFile[i] = sResourceFile[i];
			else
				sFile[i] = '/';
		}
		sFile[i] = 0;
		strlwr(sFile);
		return sFile;
	}

	uint32 GetFilenameHash( const char *sResourceFile )
	{
		uint32 code = (uint32)crc32( 0L,(unsigned char*)sResourceFile,strlen(sResourceFile) );
		return code;
	}

	virtual void Add( const char *sResourceFile )
	{
		assert(0); // Not implemented
	}
	virtual void Clear()
	{
		m_resources_crc32.clear();
	}
	virtual bool IsExist( const char *sResourceFile )
	{
		uint32 nHash = GetFilenameHash(UnifyFilename(sResourceFile));
		if (stl::binary_find( m_resources_crc32.begin(),m_resources_crc32.end(),nHash ) != m_resources_crc32.end())
			return true;
		return false;
	}
	virtual void Load( const char *sResourceListFilename )
	{
		m_resources_crc32.reserve(1000);
		CCryFile file;
		if (file.Open( sResourceListFilename,"rb" ))
		{
			int nFileLen = file.GetLength();
			char *buf = new char[nFileLen+16];
			file.ReadRaw( buf,nFileLen );
			buf[nFileLen] = '\0';

			// Parse file, every line in a file represents a resource filename.
			char seps[] = "\r\n";
			char *token = strtok( buf, seps );
			while (token != NULL)
			{
				uint32 nHash = GetFilenameHash(token);
				m_resources_crc32.push_back(nHash);
				token = strtok( NULL, seps );
			}
			delete []buf;
		}
		std::sort( m_resources_crc32.begin(),m_resources_crc32.end() );
	}
	virtual const char* GetFirst()
	{
		assert(0); // Not implemented
		return NULL;
	}
	virtual const char* GetNext()
	{
		assert(0); // Not implemented
		return NULL;
	}

private:
	std::vector<uint32> m_resources_crc32;
};

//#define COLLECT_TIME_STATISTICS
//////////////////////////////////////////////////////////////////////////
// Automatically calculate time taken by file operations.
//////////////////////////////////////////////////////////////////////////
struct SAutoCollectFileAcessTime
{
	SAutoCollectFileAcessTime( CCryPak *pPak )
	{		
#ifdef COLLECT_TIME_STATISTICS
		m_pPak = pPak;
		m_fTime = m_pPak->m_pITimer->GetAsyncCurTime();
#endif
	}
	~SAutoCollectFileAcessTime()
	{
#ifdef COLLECT_TIME_STATISTICS
		m_fTime = m_pPak->m_pITimer->GetAsyncCurTime() - m_fTime;
		m_pPak->m_fFileAcessTime += m_fTime;
#endif
	}
private:
	CCryPak *m_pPak;
	float m_fTime;
};




/////////////////////////////////////////////////////
// Initializes the crypak system;
//   pVarPakPriority points to the variable, which is, when set to 1,
//   signals that the files from pak should have higher priority than filesystem files
CCryPak::CCryPak(IMiniLog* pLog, PakVars* pPakVars, const bool bLvlRes ):
m_pLog (pLog),
m_eRecordFileOpenList(RFOM_Disabled),
m_pPakVars (pPakVars?pPakVars:&g_PakVars),
m_fFileAcessTime(0.f), m_bLvlRes(bLvlRes),
m_HandleSource(0)
{
	m_pITimer = gEnv->pTimer;

	// Create data root folder.
	m_strDataRoot = DATA_FOLDER;
	m_strDataRoot.append( 1,g_cNativeSlash );
	m_strDataRoot.MakeLower();

	m_pEngineStartupResourceList = new CResourceList;
	m_pLevelResourceList = new CResourceList;
	m_pNextLevelResourceList = new CNextLevelResourceList;
}

//////////////////////////////////////////////////////////////////////////
void CCryPak::SetGameFolderWritable( bool bWritable )
{
	m_bGameFolderWritable = bWritable;
}

//////////////////////////////////////////////////////////////////////////
void CCryPak::AddMod(const char* szMod)
{
	assert(m_strModPath.size() == 0);

	m_strModPath = string(szMod);
}

//////////////////////////////////////////////////////////////////////////
void CCryPak::RemoveMod(const char* szMod)
{
	if(!m_strModPath.size() || szMod==0)
		return;

	if(!stricmp(szMod, m_strModPath.c_str()))
		m_strModPath.clear();
	else
		assert(szMod == m_strModPath.c_str());
}

//////////////////////////////////////////////////////////////////////////
void CCryPak::ParseAliases(const char* szCommandLine)
{
	const char *szVal=szCommandLine;
	while (1)
	{			
		// this is a list of pairs separated by commas, i.e. Folder1,FolderNew,Textures,TestBuildTextures etc.
		const char *szSep=strstr(szVal,","); 
		if (!szSep)
			break; // bogus string passed.
		const char *szSepNext=strstr(szSep+1,","); // find next pair (skip comma)
		char szName[256],szAlias[256];
		strncpy(szName,szVal,szSep-szVal); // old folder name
		szName[szSep-szVal]=0; // fix the string.
		if (!szSepNext)
		{
			// the system is returning the whole command line instead of only the next token
			// check if there are other commands on the command line and skip them.
			szSepNext=strstr(szSep+1," "); 
			if (szSepNext)
			{			
				strncpy(szAlias,szSep+1,(szSepNext-1)-(szSep)); // skip 2 commas - copy alias name till the next trail
				szAlias[(szSepNext-1)-(szSep)]=0; // fix the string.
				szSepNext=NULL; // terminate the loop.
			}
			else
				strcpy(szAlias,szSep+1); // skip comma - copy alias name 
		}
		else
		{
			strncpy(szAlias,szSep+1,(szSepNext-1)-(szSep)); // skip 2 commas - copy alias name till the next trail
			szAlias[(szSepNext-1)-(szSep)]=0; // fix the string.
		}
		
		SetAlias(szName,szAlias,true); // inform the pak system.

		CryLogAlways("PAK ALIAS:%s,%s\n",szName,szAlias);
		if (!szSepNext)
			break; // no more aliases
		szVal=szSepNext+1; // move over to the next pair (skip comma)
	}
}

//////////////////////////////////////////////////////////////////////////
//! if bReturnSame==true, it will return the input name if an alias doesn't exist. Otherwise returns NULL
const char *CCryPak::GetAlias(const char* szName,bool bReturnSame)
{	
	std::list<tNameAlias*>::iterator it;	
	for (it=m_arrAliases.begin();it!=m_arrAliases.end();it++)
	{
		tNameAlias *tTemp=(*it);
		if (stricmp(tTemp->szName,szName)==0)		
			return (tTemp->szAlias);		
	} //it
	if (bReturnSame)
		return (szName);
	return (NULL);
}

//////////////////////////////////////////////////////////////////////////
void CCryPak::SetAlias(const char* szName,const char* szAlias,bool bAdd)
{
	// find out if it is already there
	std::list<tNameAlias*>::iterator it;
	tNameAlias *tPrev=NULL;
	for (it=m_arrAliases.begin();it!=m_arrAliases.end();it++)
	{
		tNameAlias *tTemp=(*it);
		if (stricmp(tTemp->szName,szName)==0)
		{
			tPrev=tTemp;
			if (!bAdd)
			{
				//remove it
				SAFE_DELETE(tPrev->szName);
				SAFE_DELETE(tPrev->szAlias);
				delete tPrev;
				m_arrAliases.erase(it);				
			}
			break;
		}
	} //it

	if (!bAdd)
		return;

	if (tPrev)
	{
		// replace existing alias
		if (stricmp(tPrev->szAlias,szAlias)!=0)
		{		
			SAFE_DELETE(tPrev->szAlias);
			tPrev->nLen2=strlen(szAlias);
			tPrev->szAlias=new char [tPrev->nLen2+1]; // includes /0
			strcpy(tPrev->szAlias,szAlias);
			// make it lowercase
			strlwr(tPrev->szAlias);
		}
	}
	else
	{
		// add a new one
		tNameAlias *tNew=new tNameAlias;

		tNew->nLen1=strlen(szName);
		tNew->szName=new char [tNew->nLen1+1]; // includes /0
		strcpy(tNew->szName,szName);
		// make it lowercase
		strlwr(tNew->szName);

		tNew->nLen2=strlen(szAlias);
		tNew->szAlias=new char [tNew->nLen2+1]; // includes /0
		strcpy(tNew->szAlias,szAlias);
		// make it lowercase
		strlwr(tNew->szAlias);

		m_arrAliases.push_back(tNew);
	}
}

//////////////////////////////////////////////////////////////////////////
CCryPak::~CCryPak()
{
	unsigned numFilesForcedToClose = 0;
	// scan through all open files and close them
	for (ZipPseudoFileArray::iterator itFile = m_arrOpenFiles.begin(); itFile != m_arrOpenFiles.end(); ++itFile)
		if (itFile->GetFile())
		{
			itFile->Destruct();
			++numFilesForcedToClose;
		}

		if (numFilesForcedToClose)
			m_pLog->LogWarning ("%u files were forced to close", numFilesForcedToClose);

		size_t numDatasForcedToDestruct = m_setCachedFiles.size();
		for (size_t i = 0; i < numDatasForcedToDestruct; ++i)
			if (m_setCachedFiles.empty())
				assert (0);
			else
				delete *m_setCachedFiles.begin();

		if (numDatasForcedToDestruct)
			m_pLog->LogWarning ("%u cached file data blocks were forced to destruct; they still have references on them, crash possible", numDatasForcedToDestruct);

		if (!m_arrArchives.empty())
			m_pLog->LogError("There are %d external references to archive objects: they have dangling pointers and will either lead to memory leaks or crashes", m_arrArchives.size());

		if (!m_mapMissingFiles.empty())
		{
			FILE* f = fopen ("Missing Files Report.txt", "wt");
			if (f)
			{
				for (MissingFileMap::iterator it = m_mapMissingFiles.begin(); it != m_mapMissingFiles.end(); ++it)
					fprintf (f, "%d\t%s\n", it->second,it->first.c_str());
				fclose(f);
			}
		}

	std::list<tNameAlias*>::iterator it;
	for (it=m_arrAliases.begin();it!=m_arrAliases.end();it++)
	{
		tNameAlias *tTemp=(*it);
		SAFE_DELETE(tTemp->szName);
		SAFE_DELETE(tTemp->szAlias);
		delete tTemp;
	}
}


// makes the path lower-case and removes the duplicate and non native slashes
// may make some other fool-proof stuff
// may NOT write beyond the string buffer (may not make it longer)
// returns: the pointer to the ending terminator \0
char* CCryPak::BeautifyPath(char* dst)
{
	// make the path lower-letters and with native slashes
	char*p,*q;
	// there's a special case: two slashes at the beginning mean UNC filepath
	p = q = dst;
	if (*p == g_cNonNativeSlash || *p == g_cNativeSlash)
		++p,++q; // start normalization/beautifications from the second symbol; if it's a slash, we'll add it, too

	while (*p)
	{
		if (*p == g_cNonNativeSlash || *p == g_cNativeSlash)
		{
			*q = g_cNativeSlash;
			++p,++q;
			while(*p == g_cNonNativeSlash || *p == g_cNativeSlash)
				++p; // skip the extra slashes
		}
		else
		{
			#if defined(LINUX)
				*q = *p;
			#else
				*q = tolower (*p);
			#endif
			++q,++p;
		}
	}
	*q = '\0';
	return q;
}

char* CCryPak::BeautifyPathForWrite(char* dst)
{
	// make the path lower-letters and with native slashes
	char*p,*q;
	// there's a special case: two slashes at the beginning mean UNC filepath
	p = q = dst;
	if (*p == g_cNonNativeSlash || *p == g_cNativeSlash)
		++p,++q; // start normalization/beautifications from the second symbol; if it's a slash, we'll add it, too

	bool bMakeLower = false;

	while (*p)
	{
		if (*p == g_cNonNativeSlash || *p == g_cNativeSlash)
		{
			*q = g_cNativeSlash;
			++p,++q;
			while(*p == g_cNonNativeSlash || *p == g_cNativeSlash)
				++p; // skip the extra slashes
		}
		else
		{
			#if defined(LINUX)
				*q = *p;
			#else
				if (*p == '%')
					bMakeLower = !bMakeLower;

				if (bMakeLower)
					*q = tolower (*p);
				else
					*q = *p;
			#endif
			++q,++p;
		}
	}
	*q = '\0';
	return q;
}

#define xFULLPATH

inline bool CheckFilenamePrefix( const char *str,const char *prefix )
{
	//this should rather be a case insensitive check here, so strnicmp is used instead of strncmp
	return (strnicmp(str,prefix,strlen(prefix)) == 0);
}

//////////////////////////////////////////////////////////////////////////
char *CCryPak::AdjustAliases(char *dst) 
{
	for (std::list<tNameAlias*>::iterator it=m_arrAliases.begin();it!=m_arrAliases.end();++it)
	{
		tNameAlias *tTemp=(*it);
		// find out if the folder is used
		char *szSrc=dst;		
		bool bFound=false;
		do 
		{
			//if (*szSrc==g_cNativeSlash)
			//	break; // didnt find any

			const char *szComp=tTemp->szName;
			if (*szSrc==*szComp)
			{
				char *szSrc2=szSrc;
				int k;
				for (k=0;k<tTemp->nLen1;k++)
				{					
					if ( (!(*szSrc2)) || (!(*szComp)) || (*szSrc2!=*szComp))
						break;					
					*szSrc2++; *szComp++;
				}
			
				if (k<tTemp->nLen1)
					break; // comparison failed, stop

				// we must verify that the next character is a slash, to be sure 
				// this is the whole folder and we aren't erroneously replacing partial folders (ex. Game04 with Game1) 
				if (*szSrc2!=g_cNativeSlash)
					break; // comparison failed, stop

				// replace name
				char szDest[256];
				int nLenDiff=szSrc-dst;
				memcpy(szDest,dst,nLenDiff); // copy till the name to be replaced
				memcpy(szDest+nLenDiff,tTemp->szAlias,tTemp->nLen2); // add the new name
				// add the rest
				//strcat(szDest+nLenDiff+tTemp->nLen2,dst+nLenDiff+tTemp->nLen1); 
				szSrc2=dst+nLenDiff+tTemp->nLen1;
				szSrc=szDest+nLenDiff+tTemp->nLen2;
				while (*szSrc2)
				{
					*szSrc++=*szSrc2++;
				} 				
				*szSrc=0;
				memcpy(dst,szDest,256);
				break; // done
			}

			break; // check only the first folder name, skip the rest.

		} while (*szSrc++);
	} //it

	return (dst);
}

#if defined PS3
#define fopenwrapper_basedir ((char *)gPS3Env->pFopenWrapperBasedir + 0)
#endif
#if defined LINUX
extern const char *fopenwrapper_basedir;
#endif

//////////////////////////////////////////////////////////////////////////
// given the source relative path, constructs the full path to the file according to the flags
const char* CCryPak::AdjustFileName(const char *src, char *dst, unsigned nFlags,bool *bFoundInPak)
{
	// in many cases, the path will not be long, so there's no need to allocate so much..
	// I'd use _alloca, but I don't like non-portable solutions. besides, it tends to confuse new developers. So I'm just using a big enough array
	char szNewSrc[g_nMaxPath];
#if defined(PS3) && defined(USE_HDD0)
	bool foundUserDir = false;
#endif
#if (defined(PS3) && !defined(__SPU__)) || defined(LINUX)
	int srcOff = 0, dstOff = 0;
	if(*src == '%')
	{
		const CSystem *const cpSystem		= (CSystem*)gEnv->pSystem;
		const string& crEngineUserPath	= cpSystem ->GetEngineUserPath();
		//adjust user path
		const char* cpUserAlias = "%USER%";
		if(strnicmp(cpUserAlias, src, strlen(cpUserAlias)) == 0)
		{
			//replace by global user path settings, cannot handle aliases as windows does
			const uint32 cUserAliasLen = crEngineUserPath.size();
			strcpy_s(szNewSrc, cUserAliasLen+1, crEngineUserPath.c_str());
			dstOff	= cUserAliasLen;
			srcOff	= strlen(cpUserAlias);
#if defined(PS3) && defined(USE_HDD0)
			foundUserDir = true;
#endif
		}
		else
			CRY_ASSERT_MESSAGE(0, "Unknown alias in CryPak::AdjustFileName");
	}
	strcpy(szNewSrc + dstOff, src + srcOff); 
#else
	strcpy_s(szNewSrc, src); 
#endif

	bool bNeedBeautifyPath = true;
	if (nFlags & FLAGS_FOR_WRITING)
	{
#if defined(WIN32) && !defined(XENON)
		// Path is adjusted for writing file.
		if (!m_bGameFolderWritable)
		{
			// If game folder is not writable on Windows, we must adjust the path to go into the user folder if it is not already starts from alias.
			if (*src != '%')
			{
				// If game folder is not writable on Windows, we must adjust the path to go into the user folder if not already.
				strcpy_s(szNewSrc,"%user%\\");
				strcat_s(szNewSrc,src);
			}
		}
		BeautifyPathForWrite(szNewSrc);
#endif
	}
	else
	{
#if !defined(PS3) || !defined(USE_HDD0)
		if (bNeedBeautifyPath)
			BeautifyPath(szNewSrc);
#endif
	}
	
	AdjustAliases(szNewSrc);

	if (nFlags & FLAGS_NO_FULL_PATH)
	{
		strcpy(dst,szNewSrc);
		return (dst);
	}
	
	bool isAbs = false;
#if !defined(USE_HDD0)//for PS3 HDD the dir cannot be absolute
	if (szNewSrc[0] == g_cNativeSlash || szNewSrc[0] == g_cNonNativeSlash
		|| szNewSrc[1] == ':')
	{
		isAbs = true;
	}
#endif
#if !defined(PS3)
	//for PS3 it cannot be a full file name yet
	if (!isAbs && !(nFlags&FLAGS_NO_MASTER_FOLDER_MAPPING))
	{
#endif
		// This is a relative filename.
#if defined(PS3)
		if (!CheckFilenamePrefix( szNewSrc,m_strDataRoot.c_str() )/* &&	!CheckFilenamePrefix( szNewSrc,"mods\\" )*/)
#else
		if (!CheckFilenamePrefix( szNewSrc,m_strDataRoot.c_str() ) &&
				!CheckFilenamePrefix( szNewSrc,".\\" ) && 
				!CheckFilenamePrefix( szNewSrc,"editor\\" ) && 
				!CheckFilenamePrefix( szNewSrc,"bin32\\" ) && 
				//!CheckFilenamePrefix( szNewSrc,"fcdata\\" ) && 
				!CheckFilenamePrefix( szNewSrc,"mods\\" ))
#endif
		{
			// Add data folder prefix.
			memmove( szNewSrc+m_strDataRoot.length(),szNewSrc,strlen(szNewSrc)+1 );
			memcpy( szNewSrc,m_strDataRoot.c_str(),m_strDataRoot.length() );			
		}
		else if (CheckFilenamePrefix( szNewSrc,".\\" ))
		{
			int nLen = strlen(szNewSrc)-2;
			memmove( szNewSrc,szNewSrc+2,nLen );
			szNewSrc[nLen] = 0;
		}

#if defined(PS3)
		//copy string and lower the casing
		char *__restrict pCurSrc = szNewSrc;
		char *__restrict pCurDst = dst;
		int dstLen = 0;
		while(*pCurSrc)
		{
			*pCurDst++ = tolower(*pCurSrc++);
			++dstLen;
		}
		*pCurDst = '\0';
#elif defined(LINUX)
		strcpy(dst, fopenwrapper_basedir);
		strcat(dst, "/");
		if(*szNewSrc == '/')
			strcat(dst, szNewSrc+1);//'/' already ends fopenwrapper_basedir
		else
			strcat(dst, szNewSrc);
#elif defined(_XBOX) || defined(XENON)
		strcpy(dst, "d:\\");
		strcat(dst, szNewSrc);
#else
		//PC code
		if (nFlags & FLAGS_ONLY_MOD_DIRS)
		{
			if (m_strModPath.empty())
			{
				return NULL;
			}

			if(!CheckFilenamePrefix( szNewSrc,"mods\\" ))
				strcpy(dst, m_strModPath.c_str());
			else
				dst[0] = '\0';
			strcat(dst, szNewSrc);	
		}
		else
		{
			strcpy(dst, szNewSrc );
		}
#endif
#if !defined(PS3)
	}
	else
	{
		// This is a full filename.
		strcpy(dst, szNewSrc);
	}
	const int dstLen = strlen(dst);
#endif
	char* pEnd = dst + dstLen;

#if defined(LINUX) || defined(PS3)
	//we got to adjust the filename and fetch the case sensitive one
	#if !defined(USE_HDD0)
		char sourceName[MAX_PATH];
		strcpy(sourceName, dst);
		// Note: we'll copy the adjustedFilename even if the filename can not be
		// matched against an existing file. This is because getFilenameNoCase() will
		// adjust leading path components (e.g. ".../game/..." => ".../Game/...").
		GetFilenameNoCase(sourceName, dst, false);
/*	#else
		//if hard disk is used, no need for a string creation here
		assert(strstr(dst, gPS3Env->pCurDirHDD0));
		const int cLen = dstLen;
		for(int i=gPS3Env->nCurDirHDD0Len; i<cLen; ++i)//leave chars for harddisk dir
			dst[i] = tolower(dst[i]);
*/	
	#endif
	if ((nFlags & FLAGS_ADD_TRAILING_SLASH) && pEnd > dst && (pEnd[-1]!=g_cNativeSlash && pEnd[-1]!=g_cNonNativeSlash))
#else
	// p now points to the end of string
	if ((nFlags & FLAGS_ADD_TRAILING_SLASH) && pEnd > dst && pEnd[-1]!=g_cNativeSlash)
#endif
	{
		*pEnd = g_cNativeSlash;
		*++pEnd = '\0';
	}

	return dst; // the last MOD scanned, or the absolute path outside MasterCD
}
/*
FILETIME CCryPak::GetFileTime(const char * szFileName)
{
FILETIME writetime;
memset(&writetime, 0, sizeof(writetime));
#ifdef WIN32
HANDLE hFile = CreateFile(szFileName, GENERIC_READ, FILE_SHARE_READ,
NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
if (hFile != INVALID_HANDLE_VALUE)
{
::GetFileTime(hFile, NULL, NULL, &writetime);
CloseHandle(hFile);
}
#endif
return writetime;
}
*/
/*bool CCryPak::IsOutOfDate(const char * szCompiledName, const char * szMasterFile)
{
FILE * f = FOpen(szMasterFile,"rb");
if (f)
FClose(f);
else
return (false);

assert(f > m_OpenFiles.Num());

f = FOpen(szCompiledName,"rb");
if (f)
FClose(f);
else
return (true);

assert(f > m_OpenFiles.Num());

#ifdef WIN32

HANDLE status1 = CreateFile(szCompiledName,GENERIC_READ,FILE_SHARE_READ,
NULL,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,NULL);

HANDLE status2 = CreateFile(szMasterFile,GENERIC_READ,FILE_SHARE_READ,
NULL,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,NULL);

FILETIME writetime1,writetime2;

GetFileTime(status1,NULL,NULL,&writetime1);
GetFileTime(status2,NULL,NULL,&writetime2);

CloseHandle(status1);
CloseHandle(status2);

if (CompareFileTime(&writetime1,&writetime2)==-1)
return(true);

return (false);
#else

return (false);

#endif
}*/

//////////////////////////////////////////////////////////////////////////
bool CCryPak::IsFileExist( const char *sFilename )
{
	// Try top open file to check if it exist or not.
	FILE *f = FOpen( sFilename,"rb",FOPEN_HINT_QUIET );
	if (f)
	{
		FClose(f);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
FILE *CCryPak::FOpen(const char *pName, const char *mode,char *szFileGamePath,int nLen)
{
	FILE* pFile = FOpen_Internal(pName, mode, szFileGamePath, nLen);

	if(!pFile)
	{
		CryFixedStringT<128> fileName = pName;
		fileName.replace("\\", "/");
		fileName.MakeLower();
		if(fileName.find("game/levels/") != -1)
		{
			string newFileName = pName;
			newFileName = "%USER%/Downloads/Levels/" + newFileName.substr(12);
			pFile = FOpen_Internal(newFileName.c_str(), mode, szFileGamePath, nLen);
		}
		else if(fileName.find("levels/") != -1)
		{
			string newFileName = pName;
			newFileName = "%USER%/Downloads/Levels/" + newFileName.substr(7);
			pFile = FOpen_Internal(newFileName.c_str(), mode, szFileGamePath, nLen);
		}
	}

	return pFile;
}

//////////////////////////////////////////////////////////////////////////
FILE *CCryPak::FOpen(const char *pName, const char *mode, unsigned nPathFlags/* =0 */)
{
	FILE* pFile = FOpen_Internal(pName, mode, nPathFlags);

	if(!pFile)
	{
 		CryFixedStringT<128> fileName = pName;
		fileName.replace("\\", "/");
		fileName.MakeLower();
		if(fileName.find("game/levels/") != -1)
		{
			string newFileName = pName;
			newFileName = "%USER%/Downloads/Levels/" + newFileName.substr(12);
			pFile = FOpen_Internal(newFileName.c_str(), mode, nPathFlags);
		}
		else if(fileName.find("levels/") != -1)
		{
			string newFileName = pName;
			newFileName = "%USER%/Downloads/Levels/" + newFileName.substr(7);
			pFile = FOpen_Internal(newFileName.c_str(), mode, nPathFlags);
		}
	}

	return pFile;
}

//////////////////////////////////////////////////////////////////////////
FILE *CCryPak::FOpen_Internal(const char *pName, const char *szMode,char *szFileGamePath,int nLen)
{
	AUTO_LOCK(m_csMain);

	SAutoCollectFileAcessTime accessTime(this);

	FILE *fp = NULL;
#if !defined(PS3) || !defined(USE_HDD0)
	char szFullPathBuf[g_nMaxPath];
	const char* szFullPath = AdjustFileName(pName, szFullPathBuf, 0);
	if (nLen>g_nMaxPath)
		nLen=g_nMaxPath;
	strncpy(szFileGamePath,szFullPath,nLen);
	fp = fopen (szFullPath, szMode);
#else
	fp = fopen (pName, szMode);//wrapper function, does all inside
#endif

	if (fp)
		RecordFile( fp, pName );
	else
		OnMissingFile(pName);
	return (fp);
}

//////////////////////////////////////////////////////////////////////////
FILE *CCryPak::FOpen_Internal(const char *pName, const char *szMode,unsigned nFlags2)
{
	AUTO_LOCK(m_csMain);
	
	SAutoCollectFileAcessTime accessTime(this);
	
	FILE *fp = NULL;
	char szFullPathBuf[g_nMaxPath];

	bool bWriteAccess = false;
	for (const char *s = szMode; *s; s++) { if (*s == 'w' || *s == 'W' || *s == 'a' || *s == 'A' || *s == '+') { bWriteAccess = true; break; }; }

	int nAdjustFlags = 0;
	if (bWriteAccess)
		nAdjustFlags |= FLAGS_FOR_WRITING;

	// get the priority into local variable to avoid it changing in the course of
	// this function execution (?)
	int nVarPakPriority = m_pPakVars->nPriority;

	// Remove unknown to CRT 'x' parameter.
	bool bDirectAccess = false;
	char smode[16];
	strncpy( smode,szMode,sizeof(smode) );
	smode[sizeof(smode) - 1] = 0;
	for (char *s = smode; *s; s++) { if (*s == 'x' || *s == 'X') { *s = ' '; bDirectAccess = true; }; }

	// maybe this file is in MOD dir - then it'll have the preference anyway
	if (!m_strModPath.empty() && !bWriteAccess)
	{
		bool bFoundInPak = false;
		const char* szModPath = AdjustFileName (pName, szFullPathBuf, FLAGS_ONLY_MOD_DIRS,&bFoundInPak);

		// [marco] will have the preference, when outside the pak, ONLY if indevmode
		// in non-devmode it always has preference inside the pak, so that we can use
		// security checks - if the file is not found in the pak and we are not in
		// devmode, do not open it

		if (szModPath)
		{
			FILE* pTest = fopen(szModPath, "r");

			if (pTest != NULL)
			{
				fclose(pTest);

				fp = fopen (szModPath, smode);
				if (fp)
				{
					RecordFile(fp, pName);
					return fp;
				}
				else
				{
					//mp, temp comment: assert (0);
					// this is strange - AdjustFileName() found a file, but fopen() didn't find one.. this should be impossible with FLAGS_ONLY_MOD_DIRS
				}
			}
		}
	}

#if defined(LINUX)
	const int _fmode = 0;
#else
	const int _fmode = _O_BINARY;
#endif
	unsigned nFlags = _fmode|_O_RDONLY;

	//if (bDirectAccess)
		//nFlags |= FOPEN_HINT_DIRECT_OPERATION;
	
	//Timur, Try direct zip operation always.
	nFlags |= FOPEN_HINT_DIRECT_OPERATION;

	// check the szMode
	for (const char* pModeChar = szMode; *pModeChar; ++pModeChar)
		switch (*pModeChar)
	{
		case 'r':
			nFlags &= ~(_O_WRONLY|_O_RDWR);
			// read mode is the only mode we can open the file in
			break;
		case 'w':
			nFlags |= _O_WRONLY;
			break;
		case 'a':
			nFlags |= _O_RDWR;
			break;
		case '+':
			nFlags |= _O_RDWR;
			break;

		case 'b':
			nFlags &= ~_O_TEXT;
			nFlags |= _O_BINARY;
			break;
		case 't':
			nFlags &= ~_O_BINARY;
			nFlags |= _O_TEXT;
			break;

		case 'c':
		case 'C':
			nFlags |= CZipPseudoFile::_O_COMMIT_FLUSH_MODE;
			break;
		case 'n':
		case 'N':
			nFlags &= ~CZipPseudoFile::_O_COMMIT_FLUSH_MODE;
			break;

		case 'S':
			nFlags |= _O_SEQUENTIAL;
			break;

		case 'R':
			nFlags |= _O_RANDOM;
			break;

		case 'T':
			nFlags |= _O_SHORT_LIVED;
			break;

		case 'D':
			nFlags |= _O_TEMPORARY;
			break;

		case 'x':
		case 'X':
			nFlags2 |= FOPEN_HINT_DIRECT_OPERATION;
			break;
	}

	const char *szFullPath = AdjustFileName(pName, szFullPathBuf, nAdjustFlags);

	if (nFlags & (_O_WRONLY|_O_RDWR))
	{
		// we need to open the file for writing, but we failed to do so.
		// the only reason that can be is that there are no directories for that file.
		// now create those dirs
#if !defined(PS3)
		if (!MakeDir (PathUtil::GetParentDirectory(string(szFullPath)).c_str()))
		{
			return NULL;
		}
#endif
		FILE *file = fopen (szFullPath, smode);
		return file;
	}

	if (!nVarPakPriority) // if the file system files have priority now..
	{
		fp = fopen (szFullPath, smode);
		if (fp)
		{
			RecordFile( fp, pName );
			return fp;
		}
	}

	// try to open the pseudofile from one of the zips
	CCachedFileData_AutoPtr pFileData = GetFileData (szFullPath);
	if (!pFileData)
	{
		if (nVarPakPriority) // if the pak files had more priority, we didn't attempt fopen before- try it now
		{
			fp = fopen (szFullPath, smode);
			if (fp)
			{
				RecordFile( fp, pName );
				return fp;
			}
		}
		if (!(nFlags2&FOPEN_HINT_QUIET))
			OnMissingFile (pName);
		return NULL; // we can't find such file in the pack files
	}

	size_t nFile;
	// find the empty slot and open the file there; return the handle
	for (nFile = 0; nFile < m_arrOpenFiles.size() && m_arrOpenFiles[nFile].GetFile(); ++nFile)
		continue;

	if (nFile == m_arrOpenFiles.size())
	{
		m_arrOpenFiles.resize (nFile+1);
	}

	if (pFileData != NULL && (nFlags2 & FOPEN_HINT_DIRECT_OPERATION))
		nFlags |= CZipPseudoFile::_O_DIRECT_OPERATION;
	m_arrOpenFiles[nFile].Construct (pFileData, nFlags);

	FILE *ret = (FILE*)(nFile + g_nPseudoFileIdxOffset);

	RecordFile( ret,pName );

	return ret; // the handle to the file
}

//////////////////////////////////////////////////////////////////////////
// given the file name, searches for the file entry among the zip files.
// if there's such file in one of the zips, then creates (or used cached)
// CCachedFileData instance and returns it.
// The file data object may be created in this function,
// and it's important that the autoptr is returned: another thread may release the existing
// cached data before the function returns
// the path must be absolute normalized lower-case with forward-slashes
CCachedFileDataPtr CCryPak::GetFileData(const char* szName)
{
#if defined(LINUX)
	replaceDoublePathFilename((char*)szName);
#endif
	unsigned nNameLen = (unsigned)strlen(szName);
	AUTO_LOCK(m_csZips);

	// scan through registered pak files and try to find this file
	for (ZipArray::reverse_iterator itZip = m_arrZips.rbegin(); itZip != m_arrZips.rend(); ++itZip)
	{
		size_t nBindRootLen = itZip->strBindRoot.length();
#if defined(LINUX) || defined(PS3)
		if (nNameLen > nBindRootLen	&&!comparePathNames(itZip->strBindRoot.c_str(), szName, nBindRootLen))
#else
		if (nNameLen > nBindRootLen	&&!memcmp(itZip->strBindRoot.c_str(), szName, nBindRootLen))
#endif
		{
			const char	*szDebug1=itZip->strBindRoot.c_str();
			const char	*szDebug2=itZip->pZip->GetFilePath();

			ZipDir::FileEntry* pFileEntry = itZip->pZip->FindFile (szName+nBindRootLen);
			if (pFileEntry)
			{
				CCachedFileData Result(NULL, itZip->pZip, pFileEntry,szName);
				AUTO_LOCK(m_csCachedFiles);

				CachedFileDataSet::iterator it = m_setCachedFiles.find(&Result);
				if (it != m_setCachedFiles.end())
				{
					assert((*it)->GetFileEntry() == pFileEntry); // cached data
					return *it;
				}
				else
					return new CCachedFileData (this, itZip->pZip, pFileEntry,szName);
			}
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
// tests if the given file path refers to an existing file inside registered (opened) packs
// the path must be absolute normalized lower-case with forward-slashes
bool CCryPak::HasFileEntry (const char* szPath)
{
	unsigned nNameLen = (unsigned)strlen(szPath);
	AUTO_LOCK(m_csZips);
	// scan through registered pak files and try to find this file
	for (ZipArray::reverse_iterator itZip = m_arrZips.rbegin(); itZip != m_arrZips.rend(); ++itZip)
	{
		size_t nBindRootLen = itZip->strBindRoot.length();
		//const char	*szDebug1=itZip->strBindRoot.c_str();
		//const char	*szDebug2=itZip->pZip->GetFilePath();
#if defined(LINUX) || defined(PS3)
		if (nNameLen > nBindRootLen	&&!comparePathNames(itZip->strBindRoot.c_str(), szPath, nBindRootLen))
#else
		if (nNameLen > nBindRootLen	&&!memcmp(itZip->strBindRoot.c_str(), szPath, nBindRootLen))
#endif
		{
			ZipDir::FileEntry* pFileEntry = itZip->pZip->FindFile (szPath+nBindRootLen);
			if (pFileEntry)
			{
				return true;
			}
		}
	}
	return false;
}


long CCryPak::FTell(FILE *hFile)
{
	AUTO_LOCK(m_csMain);
	INT_PTR nPseudoFile = ((INT_PTR)hFile) - g_nPseudoFileIdxOffset;
	if ((UINT_PTR)nPseudoFile < m_arrOpenFiles.size())
		return m_arrOpenFiles[nPseudoFile].FTell();
	else
		return ftell(hFile);
}

// returns the path to the archive in which the file was opened
const char* CCryPak::GetFileArchivePath (FILE* hFile)
{
	INT_PTR nPseudoFile = ((INT_PTR)hFile) - g_nPseudoFileIdxOffset;
	if ((UINT_PTR)nPseudoFile < m_arrOpenFiles.size())
		return m_arrOpenFiles[nPseudoFile].GetArchivePath();
	else
		return NULL;
}

#ifndef Int32x32To64
#define Int32x32To64(a, b) ((uint64)((uint64)(a)) * (uint64)((uint64)(b)))
#endif

// returns the file modification time
uint64 CCryPak::GetModificationTime(FILE* hFile)
{
	AUTO_LOCK(m_csMain);
	INT_PTR nPseudoFile = ((INT_PTR)hFile) - g_nPseudoFileIdxOffset;
	if ((UINT_PTR)nPseudoFile < m_arrOpenFiles.size())
		return m_arrOpenFiles[nPseudoFile].GetModificationTime();
	else
	{
#if defined(LINUX) || defined(PS3)
		return GetFileModifTime(hFile);
#elif defined(WIN32) || defined(XENON)
		// Win 32
		HANDLE hOsFile = (void*)_get_osfhandle( fileno(hFile) );
		FILETIME CreationTime,LastAccessTime,LastWriteTime;

		GetFileTime( hOsFile,&CreationTime,&LastAccessTime,&LastWriteTime );
		LARGE_INTEGER lt;
		lt.HighPart = LastWriteTime.dwHighDateTime;
		lt.LowPart = LastWriteTime.dwLowDateTime;
		return lt.QuadPart;
#else
		Undefined!
#endif
	}
}

size_t CCryPak::FGetSize(FILE* hFile)
{
	AUTO_LOCK(m_csMain);
	INT_PTR nPseudoFile = ((INT_PTR)hFile) - g_nPseudoFileIdxOffset;
	if ((UINT_PTR)nPseudoFile < m_arrOpenFiles.size())
		return m_arrOpenFiles[nPseudoFile].GetFileSize();
	else
	{
		long nSave = ftell (hFile);
		fseek (hFile, 0, SEEK_END);
		long nSize = ftell(hFile);
		fseek (hFile, nSave, SEEK_SET);
		return nSize;
	}
}

int CCryPak::FFlush(FILE *hFile)
{
	AUTO_LOCK(m_csMain);
	SAutoCollectFileAcessTime accessTime(this);
	INT_PTR nPseudoFile = ((INT_PTR)hFile) - g_nPseudoFileIdxOffset;
	if ((UINT_PTR)nPseudoFile < m_arrOpenFiles.size())
		return 0;
	else
		return fflush(hFile);
}

size_t CCryPak::FSeek(FILE *hFile, long seek, int mode)
{
	AUTO_LOCK(m_csMain);
	SAutoCollectFileAcessTime accessTime(this);
	INT_PTR nPseudoFile = ((INT_PTR)hFile) - g_nPseudoFileIdxOffset;
	if ((UINT_PTR)nPseudoFile < m_arrOpenFiles.size())
		return m_arrOpenFiles[nPseudoFile].FSeek(seek, mode);
	else
		return fseek(hFile, seek, mode);
}

size_t CCryPak::FWrite(const void *data, size_t length, size_t elems, FILE *hFile)
{
	AUTO_LOCK(m_csMain);
	SAutoCollectFileAcessTime accessTime(this);
	INT_PTR nPseudoFile = ((INT_PTR)hFile) - g_nPseudoFileIdxOffset;
	if ((UINT_PTR)nPseudoFile < m_arrOpenFiles.size())
		return 0;
	else
	{
		CRY_ASSERT(hFile);
		return fwrite(data, length, elems, hFile);
	}
}

//////////////////////////////////////////////////////////////////////////
size_t CCryPak::FReadRaw(void *pData, size_t nSize, size_t nCount, FILE *hFile)
{
	AUTO_LOCK(m_csMain);
	SAutoCollectFileAcessTime accessTime(this);
	INT_PTR nPseudoFile = ((INT_PTR)hFile) - g_nPseudoFileIdxOffset;
	if ((UINT_PTR)nPseudoFile < m_arrOpenFiles.size())
		return m_arrOpenFiles[nPseudoFile].FRead(pData, nSize, nCount, hFile);
	else
	{
		return fread(pData, nSize, nCount, hFile);
	}
}

//////////////////////////////////////////////////////////////////////////
size_t CCryPak::FReadRawAll(void *pData, size_t nFileSize, FILE *hFile)
{
	AUTO_LOCK(m_csMain);
	SAutoCollectFileAcessTime accessTime(this);
	INT_PTR nPseudoFile = ((INT_PTR)hFile) - g_nPseudoFileIdxOffset;
	if ((UINT_PTR)nPseudoFile < m_arrOpenFiles.size())
		return m_arrOpenFiles[nPseudoFile].FReadAll(pData, nFileSize, hFile);
	else
	{
		fseek( hFile,0,SEEK_SET );
		return fread(pData, 1, nFileSize, hFile);
	}
}

//////////////////////////////////////////////////////////////////////////
void* CCryPak::FGetCachedFileData( FILE *hFile,size_t &nFileSize )
{
	AUTO_LOCK(m_csMain);
	SAutoCollectFileAcessTime accessTime(this);
	INT_PTR nPseudoFile = ((INT_PTR)hFile) - g_nPseudoFileIdxOffset;
	if ((UINT_PTR)nPseudoFile < m_arrOpenFiles.size())
		return m_arrOpenFiles[nPseudoFile].GetFileData( nFileSize,hFile );
	else
	{
		if (m_pCachedFileData)
		{
			assert(0 && "Cannot have more then 1 FGetCachedFileData at the same time");
			CryWarning( VALIDATOR_MODULE_SYSTEM,VALIDATOR_ERROR,"!Cannot have more then 1 FGetCachedFileData at the same time" );
			return 0;
		}
		fseek( hFile,0,SEEK_END );
		nFileSize = ftell(hFile);

		m_pCachedFileData = new CCachedFileRawData(nFileSize);
		m_pCachedFileData->m_hFile = hFile;

		fseek( hFile,0,SEEK_SET );
		if (fread(m_pCachedFileData->m_pCachedData, 1, nFileSize, hFile) != nFileSize)
		{
			m_pCachedFileData = 0;
			return 0;
		}
		return m_pCachedFileData->m_pCachedData;
	}
}

//////////////////////////////////////////////////////////////////////////
int CCryPak::FClose(FILE *hFile)
{
	if (m_pCachedFileData && m_pCachedFileData->m_hFile == hFile) // Free cached data.
		m_pCachedFileData = 0;

	AUTO_LOCK(m_csMain);
	SAutoCollectFileAcessTime accessTime(this);
	INT_PTR nPseudoFile = ((INT_PTR)hFile) - g_nPseudoFileIdxOffset;
	if ((UINT_PTR)nPseudoFile < m_arrOpenFiles.size())
	{
		m_arrOpenFiles[nPseudoFile].Destruct();
		return 0;
	}
	else
		return fclose(hFile);
}


bool CCryPak::IsInPak(FILE *hFile)
{
	AUTO_LOCK(m_csMain);
	INT_PTR nPseudoFile = ((INT_PTR)hFile) - g_nPseudoFileIdxOffset;

	return (UINT_PTR)nPseudoFile < m_arrOpenFiles.size();
}


int CCryPak::FEof(FILE *hFile)
{
	AUTO_LOCK(m_csMain);
	SAutoCollectFileAcessTime accessTime(this);
	INT_PTR nPseudoFile = ((INT_PTR)hFile) - g_nPseudoFileIdxOffset;
	if ((UINT_PTR)nPseudoFile < m_arrOpenFiles.size())
		return m_arrOpenFiles[nPseudoFile].FEof();
	else
		return feof(hFile);
}

int CCryPak::FError(FILE *hFile)
{
	AUTO_LOCK(m_csMain);
	INT_PTR nPseudoFile = ((INT_PTR)hFile) - g_nPseudoFileIdxOffset;
	if ((UINT_PTR)nPseudoFile < m_arrOpenFiles.size())
		return 0;
	else
		return ferror(hFile);
}

int CCryPak::FGetErrno()
{
	return errno;
}

int CCryPak::FScanf(FILE *hFile, const char *format, ...)
{
	AUTO_LOCK(m_csMain);
	SAutoCollectFileAcessTime accessTime(this);
	va_list arglist;
	int count = 0;
	va_start(arglist, format);
	INT_PTR nPseudoFile = ((INT_PTR)hFile) - g_nPseudoFileIdxOffset;
	if ((UINT_PTR)nPseudoFile < m_arrOpenFiles.size())
		count = m_arrOpenFiles[nPseudoFile].FScanfv(format, arglist);
	else
		count = 0;//vfscanf(handle, format, arglist);
	va_end(arglist);
	return count;
}

int CCryPak::FPrintf(FILE *hFile, const char *szFormat, ...)
{
	AUTO_LOCK(m_csMain);
	SAutoCollectFileAcessTime accessTime(this);
	INT_PTR nPseudoFile = ((INT_PTR)hFile) - g_nPseudoFileIdxOffset;
	if ((UINT_PTR)nPseudoFile < m_arrOpenFiles.size())
		return 0; // we don't support it now

	va_list arglist;
	int rv;
	va_start(arglist, szFormat);

	rv = vfprintf(hFile, szFormat, arglist);
	va_end(arglist);
	return rv;
}

char *CCryPak::FGets(char *str, int n, FILE *hFile)
{
	AUTO_LOCK(m_csMain);
	SAutoCollectFileAcessTime accessTime(this);
	INT_PTR nPseudoFile = ((INT_PTR)hFile) - g_nPseudoFileIdxOffset;
	if ((UINT_PTR)nPseudoFile < m_arrOpenFiles.size())
		return m_arrOpenFiles[nPseudoFile].FGets(str, n);
	else
		return fgets(str, n, hFile);
}

int CCryPak::Getc(FILE *hFile)
{
	AUTO_LOCK(m_csMain);
	SAutoCollectFileAcessTime accessTime(this);
	INT_PTR nPseudoFile = ((INT_PTR)hFile) - g_nPseudoFileIdxOffset;
	if ((UINT_PTR)nPseudoFile < m_arrOpenFiles.size())
		return m_arrOpenFiles[nPseudoFile].Getc();
	else
		return getc(hFile);
}

int CCryPak::Ungetc(int c, FILE *hFile)
{
	AUTO_LOCK(m_csMain);
	SAutoCollectFileAcessTime accessTime(this);
	INT_PTR nPseudoFile = ((INT_PTR)hFile) - g_nPseudoFileIdxOffset;
	if ((UINT_PTR)nPseudoFile < m_arrOpenFiles.size())
		return m_arrOpenFiles[nPseudoFile].Ungetc(c);
	else
		return ungetc(c, hFile);
}

#ifndef _XBOX
const char *GetExtension (const char *in)
{
	while (*in)
	{
		if (*in=='.')
			return in;
		in++;
	}
	return NULL;
}
#else
const char *GetExtension (const char *in);
#endif //_XBOX

//////////////////////////////////////////////////////////////////////////
intptr_t CCryPak::FindFirst(const char *pDir, struct _finddata_t *fd,unsigned int nPathFlags)
{
	AUTO_LOCK(m_csMain);
	char szFullPathBuf[g_nMaxPath];

	//m_pLog->Log("Scanning %s",pDir);
	//const char *szFullPath = AdjustFileName(pDir, szFullPathBuf, 0);
	const char *szFullPath = AdjustFileName(pDir, szFullPathBuf,nPathFlags);
	CCryPakFindData_AutoPtr pFindData = new CCryPakFindData(this, szFullPath);
	if (pFindData->empty())
	{
		if (m_strModPath.empty())
			return (-1); // no mods and no files found
	}

	if(m_strModPath.size())
	{
		string sModFolder=string(pDir);
		const char *szFullPath = AdjustFileName(sModFolder.c_str(), szFullPathBuf,nPathFlags | FLAGS_ONLY_MOD_DIRS);
		//m_pLog->Log("Scanning (2) %s",szFullPath);
		if(szFullPath)
			pFindData->Scan(this,szFullPath);
	}

	if (pFindData->empty())
		return (-1);

	m_setFindData.insert (pFindData);
	pFindData->Fetch(fd);

	// you can't just cast a pointer...if it's negative, we'll think it failed!
	//return (intptr_t)(CCryPakFindData*)pFindData;
	
	// !!! FIXME: maybe just change m_setFindData to a std::map instead.
	m_HandleSource++;
	if (m_HandleSource < 0)
		m_HandleSource = 1;
	assert(m_Handles.find(m_HandleSource) == m_Handles.end());
	m_Handles.insert(std::pair<intptr_t, CCryPakFindData*>(m_HandleSource, pFindData));
	return m_HandleSource;
}

//////////////////////////////////////////////////////////////////////////
int CCryPak::FindNext(intptr_t handle, struct _finddata_t *fd)
{
	AUTO_LOCK(m_csMain);
	//if (m_setFindData.find ((CCryPakFindData*)handle) == m_setFindData.end())
	//	return -1; // invalid handle

	std::map<intptr_t, CCryPakFindData*>::iterator lookup = m_Handles.find(handle);
	if (lookup == m_Handles.end())
		return -1;
	
	CCryPakFindData *CryFinder = lookup->second;
	if (CryFinder->Fetch(fd))
		return 0;
	else
		return -1;
}

int CCryPak::FindClose(intptr_t handle)
{
	AUTO_LOCK(m_csMain);

	std::map<intptr_t, CCryPakFindData*>::iterator lookup = m_Handles.find(handle);
	if (lookup == m_Handles.end())
		return -1;
	CCryPakFindData *CryFinder = lookup->second;
	m_Handles.erase(lookup);
	
	m_setFindData.erase (CryFinder);
	return 0;
}

//======================================================================

bool CCryPak::OpenPack(const char* szBindRootIn, const char *szPath, unsigned nFlags)
{
	assert(szBindRootIn);
	char szFullPathBuf[g_nMaxPath];

	const char *szFullPath = AdjustFileName(szPath, szFullPathBuf, nFlags|FLAGS_IGNORE_MOD_DIRS);

	char szBindRootBuf[g_nMaxPath];
	const char* szBindRoot = AdjustFileName(szBindRootIn, szBindRootBuf, FLAGS_ADD_TRAILING_SLASH|FLAGS_IGNORE_MOD_DIRS);

	return OpenPackCommon(szBindRoot, szFullPath, nFlags);
}

bool CCryPak::OpenPack(const char *szPath, unsigned nFlags)
{
	char szFullPathBuf[g_nMaxPath];

	const char *szFullPath = AdjustFileName(szPath, szFullPathBuf, nFlags|FLAGS_IGNORE_MOD_DIRS);
	string strBindRoot;
	const char *pLastSlash = strrchr(szFullPath, g_cNativeSlash);
#if defined(LINUX) || defined(PS3)
  if (!pLastSlash) pLastSlash = strrchr(szFullPath, g_cNonNativeSlash);
#endif
	if (pLastSlash)
		strBindRoot.assign (szFullPath, pLastSlash - szFullPath +  1);
	else
	{
		m_pLog->LogError("Pak file %s has absolute path %s, which is strange", szPath, szFullPath);
//		desc.strFileName = szZipPath;
	}

	return OpenPackCommon(strBindRoot.c_str(), szFullPath, nFlags);
}

bool CCryPak::OpenPackCommon(const char* szBindRoot, const char *szFullPath, unsigned nFlags)
{
	AUTO_LOCK(m_csZips);
	{
		// try to find this - maybe the pack has already been opened
		for (ZipArray::iterator it = m_arrZips.begin(); it != m_arrZips.end(); ++it)
			if (!stricmp(it->pZip->GetFilePath(), szFullPath)
				&&!stricmp(it->strBindRoot.c_str(), szBindRoot))
				return true; // already opened
	}
	PackDesc desc;
#if defined(PS3) && defined(USE_HDD0)
	//the system user path not be included for the path comparison
	if(strstr(szBindRoot, gPS3Env->pFopenWrapperBasedir))
		desc.strBindRoot = &szBindRoot[gPS3Env->nCurDirHDD0Len];
	else
		desc.strBindRoot = szBindRoot;
	//slash is expected last (rBegin sadly not available)
	if(desc.strBindRoot.empty() || desc.strBindRoot.c_str()[desc.strBindRoot.size()-1] != '/')
		desc.strBindRoot += '/';
#else
	desc.strBindRoot = szBindRoot;
#endif
	desc.strFileName = szFullPath;

	desc.pArchive = OpenArchive (szFullPath, ICryArchive::FLAGS_OPTIMIZED_READ_ONLY|ICryArchive::FLAGS_ABSOLUTE_PATHS);
	if (!desc.pArchive)
		return false; // couldn't open the archive

	CryComment("Opening pak file %s",szFullPath);	

	if (desc.pArchive->GetClassId() == CryArchive::gClassId)
	{
		m_pLog->LogWithType(IMiniLog::eComment,"Opening pak file %s to %s",szFullPath,szBindRoot?szBindRoot:"<NIL>");
		desc.pZip = static_cast<CryArchive*>((ICryArchive*)desc.pArchive)->GetCache();
		m_arrZips.push_back(desc);
		return true;
	}
	else
		return false; // don't support such objects yet
}

//int gg=1;
// after this call, the file will be unlocked and closed, and its contents won't be used to search for files
bool CCryPak::ClosePack(const char* pName, unsigned nFlags)
{
	char szZipPathBuf[g_nMaxPath];
	const char* szZipPath = AdjustFileName(pName, szZipPathBuf, nFlags);
	AUTO_LOCK(m_csZips);

	//if (strstr(szZipPath,"huggy_tweak_scripts"))
	//	gg=0;

	for (ZipArray::iterator it = m_arrZips.begin(); it != m_arrZips.end(); ++it)
		if (!stricmp(szZipPath, it->GetFullPath()))
		{
			// this is the pack with the given name - remove it, and if possible it will be deleted
			// the zip is referenced from the archive and *it; the archive is referenced only from *it
			//
			// the pZip (cache) can be referenced from stream engine and pseudo-files.
			// the archive can be referenced from outside
			bool bResult = (it->pZip->NumRefs() == 2) && it->pArchive->NumRefs() == 1;
			if (bResult)
			{
				m_arrZips.erase (it);
			}
			return bResult;
		}
		return true;
}


bool CCryPak::OpenPacks(const char *pWildcardIn, unsigned nFlags)
{
	char cWorkBuf[g_nMaxPath];
	AdjustFileName(pWildcardIn, cWorkBuf, nFlags|FLAGS_COPY_DEST_ALWAYS);
	string strBindRoot = PathUtil::GetParentDirectory(cWorkBuf);
	strBindRoot += g_cNativeSlash;
	return OpenPacksCommon(strBindRoot.c_str(), pWildcardIn, cWorkBuf, nFlags);
}

bool CCryPak::OpenPacks(const char* szBindRoot, const char *pWildcardIn, unsigned nFlags)
{
	char cWorkBuf[g_nMaxPath];
	AdjustFileName(pWildcardIn, cWorkBuf, nFlags|FLAGS_COPY_DEST_ALWAYS);
	char cBindRootBuf[g_nMaxPath];
#if defined(PS3)
	const char* pBindRoot = szBindRoot;
#else
	const char* pBindRoot = AdjustFileName(szBindRoot, cBindRootBuf, nFlags|FLAGS_ADD_TRAILING_SLASH);
#endif
	return OpenPacksCommon(pBindRoot, pWildcardIn, cWorkBuf, nFlags);
}

bool CCryPak::OpenPacksCommon(const char* szDir, const char *pWildcardIn, char *cWork, unsigned nFlags)
{
	// Note this code suffers from a common FindFirstFile problem - a search
	// for *.pak will also find *.pak? (since the short filename version of *.pakx,
	// for example, is *.pak). For more information, see
	// http://blogs.msdn.com/oldnewthing/archive/2005/07/20/440918.aspx
	// Therefore this code performs an additional check to make sure that the
	// found filenames match the spec.
	__finddata64_t fd;
	intptr_t h = _findfirst64 (cWork, &fd);

	char cWildcardFullPath[MAX_PATH];
	sprintf(cWildcardFullPath, "*.%s", PathUtil::GetExt(pWildcardIn) );

	// where to copy the filenames to form the path in cWork
	char* pDestName = strrchr (cWork, g_cNativeSlash);
#if defined(LINUX) || defined(PS3)
	if (!pDestName) pDestName = strrchr (cWork, g_cNonNativeSlash);
#endif
	if (!pDestName)
		pDestName = cWork;
	else
		++pDestName;
	if (h != -1)
	{
		std::vector<string> files;
		do 
		{
			strcpy (pDestName, fd.name);
			if (PathUtil::MatchWildcard(cWork, cWildcardFullPath))
				files.push_back(strlwr(cWork));
		}
		while(0 == _findnext64 (h, &fd));

		// Open files in alphabet order.
		std::sort( files.begin(),files.end() );
		for (int i = 0; i < (int)files.size(); i++)
		{
			OpenPackCommon(szDir, files[i].c_str(), nFlags);
		}

		_findclose (h);
		return true;
	}

	return false;
}


bool CCryPak::ClosePacks(const char *pWildcardIn, unsigned nFlags)
{
	bool bOk = true;
	__finddata64_t fd;
	char cWorkBuf[g_nMaxPath];
	const char* cWork = AdjustFileName(pWildcardIn, cWorkBuf, nFlags);
	intptr_t h = _findfirst64 (cWork, &fd);
	string strDir = PathUtil::GetParentDirectory(pWildcardIn);
	if (h != -1)
	{
		do {
			if (!ClosePack((strDir + "\\" + fd.name).c_str(), nFlags))
				bOk = false;
		} while(0 == _findnext64 (h, &fd));
		_findclose (h);
		return true;
	}
	return false;
}


bool CCryPak::InitPack(const char *szBasePath, unsigned nFlags)
{
	return true;
}

/////////////////////////////////////////////////////
bool CCryPak::Init(const char *szBasePath)
{
	return InitPack(szBasePath);
}

void CCryPak::Release()
{
}





//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
CCachedFileRawData::CCachedFileRawData( int nAlloc )
{
	m_hFile = 0;
	m_pCachedData = 0;

	m_pCachedData = g_pPakHeap->Alloc (nAlloc,"CCachedFileRawData::CCachedFileRawData");
}

//////////////////////////////////////////////////////////////////////////
CCachedFileRawData::~CCachedFileRawData()
{
	if (m_pCachedData)
		g_pPakHeap->Free(m_pCachedData);
	m_pCachedData = 0;
}


//////////////////////////////////////////////////////////////////////////
// this object must be constructed before usage
// nFlags is a combination of _O_... flags
void CZipPseudoFile::Construct(CCachedFileData* pFileData, unsigned nFlags)
{
	m_pFileData = pFileData;
	m_nFlags = nFlags;
	m_nCurSeek = 0;
	m_pCachedData = 0;

	if ((nFlags & _O_DIRECT_OPERATION)&&pFileData&&pFileData->GetFileEntry()->nMethod == ZipFile::METHOD_STORE)
	{
		m_fDirect = fopen (pFileData->GetZip()->GetFilePath(), "rb"); // "rb" is the only supported mode
		FSeek(0,SEEK_SET); // the user assumes offset 0 right after opening the file
	}
	else
		m_fDirect = NULL;
}

//////////////////////////////////////////////////////////////////////////
// this object needs to be freed manually when the CryPak shuts down..
void CZipPseudoFile::Destruct()
{
	assert ((bool)m_pFileData);
	// mark it free, and deallocate the pseudo file memory
	m_pFileData = NULL;
	if (m_fDirect)
		fclose (m_fDirect);
	m_fDirect = 0;
	if (m_pCachedData)
		g_pPakHeap->Free(m_pCachedData);
	m_pCachedData = 0;
}

//////////////////////////////////////////////////////////////////////////
int CZipPseudoFile::FSeek (long nOffset, int nMode)
{
	if (!m_pFileData)
		return -1;

	switch (nMode)
	{
	case SEEK_SET:
		m_nCurSeek = nOffset;
		break;
	case SEEK_CUR:
		m_nCurSeek += nOffset;
		break;
	case SEEK_END:
		m_nCurSeek = GetFileSize() - nOffset;
		break;
	default:
		assert(0);
		return -1;
	}
	if (m_fDirect)
		fseek (m_fDirect, m_nCurSeek + GetFile()->GetFileDataOffset(), SEEK_SET);
	return 0;
}

//////////////////////////////////////////////////////////////////////////
size_t CZipPseudoFile::FRead (void* pDest, size_t nSize, size_t nCount, FILE* hFile)
{
	if (!GetFile())
		return 0;

	size_t nTotal = nSize * nCount;
	if (!nTotal || (unsigned)m_nCurSeek >= GetFileSize())
		return 0;

	if (nTotal > GetFileSize() - m_nCurSeek)
	{
		nTotal = GetFileSize() - m_nCurSeek;
		if (nTotal < nSize)
			return 0;
		nTotal -= nTotal % nSize;
	}

	if (m_fDirect)
	{
		m_nCurSeek += nTotal;
		//return fread (pDest, nSize, nCount, m_fDirect);

		// FIX [Carsten]: nTotal holds the amount of bytes we need to read!
		// Since we're in a zip archive it's very likely that we read more than actually belongs to hFile.
		// Therefore nTotal was correct in the previous "if" branch in case of we're reading over the end of the file.
		if (fread (pDest, nTotal, 1, m_fDirect) == 1)
			return nCount;
		else
			return 0;
	}
	else
	{
		unsigned char * pSrc = (unsigned char *)GetFile()->GetData();
		if (!pSrc)
			return 0;
		pSrc += m_nCurSeek;

		if (!(m_nFlags & _O_TEXT))
			memcpy(pDest, pSrc, nTotal);
		else
		{
			unsigned char *itDest = (unsigned char *)pDest;
			unsigned char *itSrc = pSrc, *itSrcEnd = pSrc + nTotal;
			for (; itSrc != itSrcEnd; ++itSrc)
			{
				if (*pSrc != 0xd)
					*(itDest++) = *itSrc;
			}
			m_nCurSeek += nTotal;
			return itDest - (unsigned char*) pDest;
		}
		m_nCurSeek += nTotal;
	}
	return nTotal / nSize;
}

//////////////////////////////////////////////////////////////////////////
size_t CZipPseudoFile::FReadAll (void* pDest, size_t nFileSize, FILE* hFile)
{
	if (!GetFile())
		return 0;

	if (nFileSize != GetFileSize())
	{
		assert(0); // Bad call
		return 0;
	}

	if (m_fDirect)
	{
		FSeek(0,SEEK_SET);
		m_nCurSeek = nFileSize;
		if (fread (pDest, nFileSize, 1, m_fDirect) == 1)
			return nFileSize;
		else
			return 0;
	}
	else
	{
		if (!GetFile()->GetDataTo( pDest,nFileSize ))
			return 0;

		m_nCurSeek = nFileSize;
	}
	return nFileSize;
}

//////////////////////////////////////////////////////////////////////////
void* CZipPseudoFile::GetFileData ( size_t &nFileSize, FILE* hFile )
{
	if (!GetFile())
		return 0;

	nFileSize = GetFileSize();

	if (m_fDirect)
	{
		if (!m_pCachedData)
			m_pCachedData = g_pPakHeap->Alloc (nFileSize, "CZipPseudoFile::GetFileData");
		
		fseek( m_fDirect,0,SEEK_SET );
		if (fread (m_pCachedData, nFileSize, 1, m_fDirect) == 1)
		{
			m_nCurSeek = nFileSize;
			return m_pCachedData;
		}
		else
		{
			g_pPakHeap->Free(m_pCachedData);
			m_pCachedData = NULL;
		}
	}
	else
	{
		void *pData = GetFile()->GetData();
		m_nCurSeek = nFileSize;
		return pData;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
int CZipPseudoFile::FEof()
{
	return (unsigned)m_nCurSeek >= GetFileSize();
}

int CZipPseudoFile::FScanfv (const char* szFormat, va_list args)
{
	if (!GetFile())
		return 0;
	char *pSrc = (char *)GetFile()->GetData();
	if (!pSrc)
		return 0;
	// now scan the pSrc+m_nCurSeek
	return 0;
}

char* CZipPseudoFile::FGets(char* pBuf, int n)
{
	if (!GetFile())
		return NULL;
	
	if (m_fDirect)
	{
		long n0 = ftell(m_fDirect);
		char *s = fgets( pBuf,n,m_fDirect );
		long n1 = ftell(m_fDirect);
		m_nCurSeek += (n1-n0);
		return s;
	}

	char *pData = (char *)GetFile()->GetData();
	if (!pData)
		return NULL;
	int nn = 0;
	int i;
	for (i=0; i<n; i++)
	{
		if (i+m_nCurSeek == GetFileSize())
			break;
		char c = pData[i+m_nCurSeek];
		if (c == 0xa || c == 0)
		{
			pBuf[nn++] = c;
			i++;
			break;
		}
		else
			if (c == 0xd)
				continue;
		pBuf[nn++] = c;
	}
	pBuf[nn] = 0;
	m_nCurSeek += i;
	if (m_nCurSeek == GetFileSize())
		return NULL;
	return pBuf;
}

int CZipPseudoFile::Getc()
{
	if (!GetFile())
		return EOF;
	char *pData = (char *)GetFile()->GetData();
	if (!pData)
		return EOF;
	int nn = 0;
	int c = EOF;
	int i;
	for (i=0; i<1; i++)
	{
		if (i+m_nCurSeek == GetFileSize())
			return c;
		c = pData[i+m_nCurSeek];
		break;
	}
	m_nCurSeek += i+1;
	return c;
}

int CZipPseudoFile::Ungetc(int c)
{
	if (m_nCurSeek <= 0)
		return EOF;
	m_nCurSeek--;
	return c;
}

CCachedFileData::CCachedFileData(class CCryPak* pPak, ZipDir::Cache*pZip, ZipDir::FileEntry* pFileEntry,const char *szFilename)
{
	m_pPak = pPak;
	m_pFileData = NULL;
	m_nRefCounter = 0;
	m_pZip = pZip;
	m_pFileEntry = pFileEntry;
	m_nRefCount = 0;
	//m_filename = szFilename;

	if (pPak)
		pPak->Register (this);

}

CCachedFileData::~CCachedFileData()
{
	if (m_pPak)
		m_pPak->Unregister (this);

	// forced destruction
	if (m_pFileData)
	{
		m_pZip->Free (m_pFileData);
		m_pFileData = NULL;
	}

	m_pZip = NULL;
	m_pFileEntry = NULL;
}

//////////////////////////////////////////////////////////////////////////
void CCachedFileData::AddRef()
{
	CryInterlockedIncrement(&m_nRefCount);
}

//////////////////////////////////////////////////////////////////////////
void CCachedFileData::Release()
{
	if (CryInterlockedDecrement(&m_nRefCount) <= 0)
		delete this;
}

//////////////////////////////////////////////////////////////////////////
bool CCachedFileData::GetDataTo( void *pFileData,int nDataSize )
{
	assert ((bool)m_pZip);
	assert (m_pFileEntry && m_pZip->IsOwnerOf(m_pFileEntry));

	CCritSection& csCachedFileDataLock = m_pPak->GetCachedFileLock();
	AUTO_LOCK(csCachedFileDataLock);

	if (nDataSize != m_pFileEntry->desc.lSizeUncompressed)
		return false;
	
	if (!m_pFileData)
	{
		if (ZipDir::ZD_ERROR_SUCCESS != m_pZip->ReadFile (m_pFileEntry, NULL, pFileData))
		{
			return false;
		}
	}
	else
	{
		memcpy( pFileData,m_pFileData,nDataSize );
	}
	return true;
}

// return the data in the file, or NULL if error
void* CCachedFileData::GetData(bool bRefreshCache)
{
	// first, do a "dirty" fast check without locking the critical section
	// in most cases, the data's going to be already there, and if it's there,
	// nobody's going to release it until this object is destructed.
	if (bRefreshCache && !m_pFileData)
	{
		assert ((bool)m_pZip);
		assert (m_pFileEntry && m_pZip->IsOwnerOf(m_pFileEntry));
		// Then, lock it and check whether the data is still not there.
		// if it's not, allocate memory and unpack the file
		CCritSection& csCachedFileDataLock = m_pPak->GetCachedFileLock();
		AUTO_LOCK(csCachedFileDataLock);
		if (!m_pFileData)
		{
			m_pFileData = g_pPakHeap->Alloc (m_pFileEntry->desc.lSizeUncompressed, "CCachedFileData::GetData");
			if (ZipDir::ZD_ERROR_SUCCESS != m_pZip->ReadFile (m_pFileEntry, NULL, m_pFileData))
			{
				g_pPakHeap->Free(m_pFileData);
				m_pFileData = NULL;
			}
		}
	}
	return m_pFileData;
}

//////////////////////////////////////////////////////////////////////////
void CCryPakFindData::Scan(class CCryPak*pPak, const char* szDir)
{
	// get the priority into local variable to avoid it changing in the course of
	// this function execution
	int nVarPakPriority = pPak->m_pPakVars->nPriority;

	if (!nVarPakPriority)
	{
		// first, find the file system files
		ScanFS(pPak,szDir);
		ScanZips(pPak, szDir);
	}
	else
	{
		// first, find the zip files
		ScanZips(pPak, szDir);
		ScanFS(pPak,szDir);
	}
}

//////////////////////////////////////////////////////////////////////////
CCryPakFindData::CCryPakFindData (class CCryPak*pPak, const char* szDir)
{
	Scan(pPak,szDir);
}

//////////////////////////////////////////////////////////////////////////
void CCryPakFindData::ScanFS(CCryPak*pPak, const char *szDirIn)
{
	//char cWork[CCryPak::g_nMaxPath];
	//pPak->AdjustFileName(szDirIn, cWork);

	__finddata64_t fd;
#ifdef WIN64
	memset (&fd, 0, sizeof(fd));
#endif
	intptr_t nFS = _findfirst64 (szDirIn, &fd);
	if (nFS == -1)
		return;

	do
	{
		m_mapFiles.insert (FileMap::value_type(fd.name, FileDesc(&fd)));
	}
	while(0 == _findnext64(nFS, &fd));

	_findclose (nFS);
}

//////////////////////////////////////////////////////////////////////////
void CCryPakFindData::ScanZips (CCryPak* pPak, const char* szDir)
{
	size_t nLen = strlen(szDir);
	CAutoLock<CCritSection> __AL_Zips(pPak->m_csZips);
	for (CCryPak::ZipArray::iterator it = pPak->m_arrZips.begin(); it != pPak->m_arrZips.end(); ++it)
	{
		size_t nBindRootLen = it->strBindRoot.length();

#if defined(LINUX) || defined(PS3)
		if (nLen > nBindRootLen && !comparePathNames(szDir, it->strBindRoot.c_str(), nBindRootLen))
#else
		if (nLen > nBindRootLen && !memcmp(szDir, it->strBindRoot.c_str(), nBindRootLen))
#endif
		{
			// first, find the files
			{
				ZipDir::FindFile fd (it->pZip);
				for(fd.FindFirst (szDir + nBindRootLen); fd.GetFileEntry(); fd.FindNext())
					m_mapFiles.insert (FileMap::value_type(fd.GetFileName(), FileDesc(fd.GetFileEntry())));
			}

			{
				ZipDir::FindDir fd (it->pZip);
				for (fd.FindFirst(szDir + nBindRootLen); fd.GetDirEntry(); fd.FindNext())
					m_mapFiles.insert (FileMap::value_type(fd.GetDirName(), FileDesc ()));
			}
		}
	}
}

bool CCryPakFindData::empty() const
{
	return m_mapFiles.empty();
}

bool CCryPakFindData::Fetch(_finddata_t* pfd)
{
	if (m_mapFiles.empty())
		return false;

	FileMap::iterator it = m_mapFiles.begin();
	memcpy(pfd->name, it->first.c_str(), min((uint32)sizeof(pfd->name), (uint32)(it->first.length()+1)));
	pfd->attrib = it->second.nAttrib;
	pfd->size   = it->second.nSize;
	pfd->time_access = it->second.tAccess;
	pfd->time_create = it->second.tCreate;
	pfd->time_write  = it->second.tWrite;

	m_mapFiles.erase (it);
	return true;
}

CCryPakFindData::FileDesc::FileDesc (struct _finddata_t* fd)
{
	nSize   = fd->size;
	nAttrib = fd->attrib;
	tAccess = fd->time_access;
	tCreate = fd->time_create;
	tWrite  = fd->time_write;
}

// the conversions in this function imply that we don't support
// 64-bit file sizes or 64-bit time values
CCryPakFindData::FileDesc::FileDesc (struct __finddata64_t* fd)
{
	nSize   = (unsigned)fd->size;
	nAttrib = fd->attrib;
	tAccess = (time_t)fd->time_access;
	tCreate = (time_t)fd->time_create;
	tWrite  = (time_t)fd->time_write;
}


CCryPakFindData::FileDesc::FileDesc (ZipDir::FileEntry* fe)
{
	nSize = fe->desc.lSizeUncompressed;
#if defined(LINUX) || defined(PS3)
	nAttrib = _A_IN_CRYPAK; // files in zip are read-only, and
#else
	nAttrib = _A_RDONLY|_A_IN_CRYPAK; // files in zip are read-only, and
#endif
	tAccess = -1;
	tCreate = -1;
	tWrite  = -1; // we don't need it
}

CCryPakFindData::FileDesc::FileDesc ()
{
	nSize = 0;
#if defined(LINUX) || defined(PS3)
	nAttrib = _A_SUBDIR;
#else
	nAttrib = _A_SUBDIR | _A_RDONLY;
#endif
	tAccess = -1;
	tCreate = -1;
	tWrite  = -1;
}

//! Puts the memory statistics into the given sizer object
//! According to the specifications in interface ICrySizer
void CCryPak::GetMemoryStatistics(ICrySizer *pSizer)
{
	AUTO_LOCK(m_csZips);

	pSizer->Add(this);

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "Zips");
		size_t nSize = 0;
	
		for (ZipArray::iterator itZip = m_arrZips.begin(); itZip != m_arrZips.end(); ++itZip)
			nSize += itZip->sizeofThis();

		pSizer->AddObject((void *)&m_arrZips,nSize);
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "FindData");
		size_t nSize = 0;
	
		for (CryPakFindDataSet::iterator itFindData = m_setFindData.begin(); itFindData != m_setFindData.end(); ++itFindData)
			nSize += sizeof(CryPakFindDataSet::value_type) + (*itFindData)->sizeofThis();

		pSizer->AddObject((void *)&m_setFindData,nSize);
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CachedFiles");
		size_t nSize = 0;
	
		for (CachedFileDataSet::iterator itCachedData = m_setCachedFiles.begin(); itCachedData != m_setCachedFiles.end(); ++itCachedData)
		{
			nSize += sizeof(CachedFileDataSet::value_type) + (*itCachedData)->sizeofThis();
	
//			char str[256];

//			sprintf(str,"%d ",(*itCachedData)->sizeofThis());
//			OutputDebugString(str);
		}
//		OutputDebugString("\n");

		pSizer->AddObject((void *)&m_setCachedFiles,nSize);
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "OpenFiles");
		size_t nSize = m_arrOpenFiles.capacity() * sizeof(CZipPseudoFile);
		pSizer->AddObject((void *)&m_arrOpenFiles,nSize);
	}
}

size_t CCryPakFindData::sizeofThis()const
{
	size_t nSize = sizeof(*this);
	for (FileMap::const_iterator it = m_mapFiles.begin(); it != m_mapFiles.end(); ++it)
	{
		nSize += sizeof(FileMap::value_type);
		nSize += it->first.capacity();
	}
	return nSize;
}


bool CCryPak::MakeDir(const char* szPath)
{
#if !defined(PS3)
	char newPath[MAX_PATH];
	char *q = newPath;

	memset(newPath,0,sizeof(newPath));

	const char* p = szPath;
	bool bUNCPath = false;
	// Check for UNC path.
	if (strlen(szPath) > 2 && szPath[0] == g_cNativeSlash && szPath[1] == g_cNativeSlash)
	{
		// UNC path given, Skip first 2 slashes.
		*q++ = *p++;
		*q++ = *p++;
	}

	for (; *p; )
	{
		while (*p != g_cNonNativeSlash && *p != g_cNativeSlash && *p)
		{
			*q++ = *p++;
		}
		CryCreateDirectory( newPath,NULL );
		if (*p)
		{
			*q++ = *p++;
		}
	}
#endif
	int res = CryCreateDirectory( szPath,NULL );
	if (res != 0)
	{
		// !!! FIXME:
		// CryCreateDirectory() sets an error if the path already exists, which is probably wrong behaviour.
		// CryGetFileAttributes() and WinBase.cpp's GetFileAttributes() both have incorrect behaviour.
		// None of these are worth the risk of fixing, in case there is code relying on the existing behaviour.
		#if 0
		const DWORD attrs = GetFileAttributes(szPath);
		if ((attrs == INVALID_FILE_ATTRIBUTES) || ((attrs & FILE_ATTRIBUTE_DIRECTORY) == 0))
			return false; // couldn't create such directory
		#elif defined(LINUX)
			struct stat statbuf;
			if ((stat(szPath, &statbuf) == -1) || (!S_ISDIR(statbuf.st_mode)))
				return false; // couldn't create such directory
		#else
		if (GetLastError() == ERROR_PATH_NOT_FOUND)
				return false; // couldn't create such directory
		#endif
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
// open the physical archive file - creates if it doesn't exist
// returns NULL if it's invalid or can't open the file
ICryArchive* CCryPak::OpenArchive (const char* szPath, unsigned nFlags)
{
	char szFullPathBuf[CCryPak::g_nMaxPath];
	const char* szFullPath = AdjustFileName(szPath, szFullPathBuf, nFlags);

	// if it's simple and read-only, it's assumed it's read-only
	if (nFlags & ICryArchive::FLAGS_OPTIMIZED_READ_ONLY)
		nFlags |= ICryArchive::FLAGS_READ_ONLY;

	unsigned nFactoryFlags = 0;
	if (nFlags & ICryArchive::FLAGS_DONT_COMPACT)
		nFactoryFlags |= ZipDir::CacheFactory::FLAGS_DONT_COMPACT;

	if (nFlags & ICryArchive::FLAGS_READ_ONLY)
		nFactoryFlags |= ZipDir::CacheFactory::FLAGS_READ_ONLY;

	if (nFlags & ICryArchive::FLAGS_CREATE_NEW)
		nFactoryFlags |= ZipDir::CacheFactory::FLAGS_CREATE_NEW;

	ICryArchive* pArchive = FindArchive(szFullPath);
	const unsigned nFlagsMustMatch = ICryArchive::FLAGS_RELATIVE_PATHS_ONLY | ICryArchive::FLAGS_ABSOLUTE_PATHS | ICryArchive::FLAGS_READ_ONLY;
	if (pArchive)
	{
		// check for compatibility

		if (!(nFlags & ICryArchive::FLAGS_RELATIVE_PATHS_ONLY) && (pArchive->GetFlags() & ICryArchive::FLAGS_RELATIVE_PATHS_ONLY))
			pArchive->ResetFlags(ICryArchive::FLAGS_RELATIVE_PATHS_ONLY);

		// we found one
		if (nFlags & ICryArchive::FLAGS_OPTIMIZED_READ_ONLY)
		{
			if (pArchive->GetClassId() == CryArchive::gClassId)
				return pArchive; // we can return an optimized archive

			//if (!(pArchive->GetFlags() & ICryArchive::FLAGS_READ_ONLY))
			return NULL; // we can't let it open read-only optimized while it's open for RW access
		}
		else
		{
			if (!(nFlags & ICryArchive::FLAGS_READ_ONLY) && (pArchive->GetFlags() & ICryArchive::FLAGS_READ_ONLY))
			{
				// we don't support upgrading from ReadOnly to ReadWrite
				return NULL;
			}

			return pArchive;
		}

		return NULL;
	}

	string strBindRoot;

	//if (!(nFlags & ICryArchive::FLAGS_RELATIVE_PATHS_ONLY))
	strBindRoot = PathUtil::GetParentDirectory(szFullPath);

	// Check if file on disk exist.
	FILE *fp = fopen (szFullPath, "rb");
	if (fp)
	{
		fclose(fp);
	}
	else if (nFactoryFlags & ZipDir::CacheFactory::FLAGS_READ_ONLY)
	{
		// Pak file not found.
		CryWarning( VALIDATOR_MODULE_SYSTEM,VALIDATOR_WARNING,"Cannot open Pak file %s",szFullPath );
		return NULL;
	}

	//try
	{
		ZipDir::CacheFactory factory (g_pPakHeap, ZipDir::ZD_INIT_FAST, nFactoryFlags);
		if (nFlags & ICryArchive::FLAGS_OPTIMIZED_READ_ONLY)
			return new CryArchive (this, strBindRoot, factory.New (szFullPath), nFlags);
		else
			return new CryArchiveRW(this, strBindRoot, factory.NewRW(szFullPath), nFlags);
	}
	/*
	catch(ZipDir::Error e)
	{
		m_pLog->LogError("can't create the archive \"%s\"==\"%s\": error %s (code %d) in %s at %s:%d. %s.", szPath, szFullPath, e.getError(), e.nError, e.szFunction, e.szFile, e.nLine, e.getDescription());
		return NULL;
	}
	*/
}


//////////////////////////////////////////////////////////////////////////
// Adds a new file to the zip or update an existing one
// adds a directory (creates several nested directories if needed)
// compression methods supported are 0 (store) and 8 (deflate) , compression level is 0..9 or -1 for default (like in zlib)
int CryArchiveRW::UpdateFile (const char* szRelativePath, void* pUncompressed, unsigned nSize, unsigned nCompressionMethod, int nCompressionLevel)
{
	if (m_nFlags & FLAGS_READ_ONLY)
		return ZipDir::ZD_ERROR_INVALID_CALL;

	char szFullPath[CCryPak::g_nMaxPath];
	const char*pPath = AdjustPath (szRelativePath, szFullPath);
	if (!pPath)
		return ZipDir::ZD_ERROR_INVALID_PATH;
	return m_pCache->UpdateFile(pPath, pUncompressed, nSize, nCompressionMethod, nCompressionLevel);
}

//////////////////////////////////////////////////////////////////////////
//   Adds a new file to the zip or update an existing one if it is not compressed - just stored  - start a big file
int	CryArchiveRW::StartContinuousFileUpdate( const char* szRelativePath, unsigned nSize )
{
	if (m_nFlags & FLAGS_READ_ONLY)
		return ZipDir::ZD_ERROR_INVALID_CALL;

	char szFullPath[CCryPak::g_nMaxPath];
	const char*pPath = AdjustPath (szRelativePath, szFullPath);
	if (!pPath)
		return ZipDir::ZD_ERROR_INVALID_PATH;
	return m_pCache->StartContinuousFileUpdate(pPath, nSize );
}


//////////////////////////////////////////////////////////////////////////
// Adds a new file to the zip or update an existing's segment if it is not compressed - just stored 
// adds a directory (creates several nested directories if needed)
int CryArchiveRW::UpdateFileContinuousSegment (const char* szRelativePath, unsigned nSize, void* pUncompressed, unsigned nSegmentSize, unsigned nOverwriteSeekPos )
{
	if (m_nFlags & FLAGS_READ_ONLY)
		return ZipDir::ZD_ERROR_INVALID_CALL;

	char szFullPath[CCryPak::g_nMaxPath];
	const char*pPath = AdjustPath (szRelativePath, szFullPath);
	if (!pPath)
		return ZipDir::ZD_ERROR_INVALID_PATH;
	return m_pCache->UpdateFileContinuousSegment(pPath, nSize, pUncompressed, nSegmentSize,nOverwriteSeekPos );
}



uint32 CCryPak::ComputeCRC( const char* szPath )
{
	assert(szPath);

	uint32 dwCRC=0;

	// generate crc32
	{
		const uint32 dwChunkSize=1024*1024;		// 1MB chunks
		uint8 *pMem = new uint8[dwChunkSize];

		if(!pMem)
			return ZipDir::ZD_ERROR_NO_MEMORY;

		uint32 dwSize = 0;

		{
			FILE *fp = 	gEnv->pCryPak->FOpen(szPath, "rb");

			if(fp)
			{
				gEnv->pCryPak->FSeek(fp, 0, SEEK_END);
				dwSize = gEnv->pCryPak->FGetSize(fp);
				gEnv->pCryPak->FClose(fp);
			}
		}

		// rbx open flags, x is a hint to not cache whole file in memory.
		FILE *fp = 	gEnv->pCryPak->FOpen(szPath, "rbx");

		if(!fp)
		{
			delete pMem;
			return ZipDir::ZD_ERROR_INVALID_PATH;
		}

		// load whole file in chunks and compute CRC
		while(dwSize>0)
		{
			uint32 dwLocalSize = min(dwSize,dwChunkSize);

			INT_PTR read = gEnv->pCryPak->FReadRaw(pMem,1,dwLocalSize,fp);		

			assert(read==dwLocalSize);

			dwCRC = crc32(dwCRC,pMem,dwLocalSize);
			dwSize-=dwLocalSize;
		}

		delete pMem;

		gEnv->pCryPak->FClose(fp);
	}
	
	return dwCRC;
}

bool CCryPak::ComputeMD5(const char* szPath, unsigned char* md5)
{
	if(!szPath || !md5)
		return false;

	MD5Context context;
	MD5Init(&context);

	// generate checksum
	{
		const uint32 dwChunkSize=1024*1024;		// 1MB chunks
		uint8 *pMem = new uint8[dwChunkSize];

		if(!pMem)
			return false;

		uint32 dwSize = 0;

		{
			FILE *fp = 	gEnv->pCryPak->FOpen(szPath, "rb");

			if(fp)
			{
				gEnv->pCryPak->FSeek(fp, 0, SEEK_END);
				dwSize = gEnv->pCryPak->FGetSize(fp);
				gEnv->pCryPak->FClose(fp);
			}
		}

		// rbx open flags, x is a hint to not cache whole file in memory.
		FILE *fp = 	gEnv->pCryPak->FOpen(szPath, "rbx");

		if(!fp)
		{
			delete pMem;
			return false;
		}

		// load whole file in chunks and compute Md5
		while(dwSize>0)
		{
			uint32 dwLocalSize = min(dwSize,dwChunkSize);

			INT_PTR read = gEnv->pCryPak->FReadRaw(pMem,1,dwLocalSize,fp);		
			assert(read==dwLocalSize);

			MD5Update(&context, pMem, dwLocalSize);
			dwSize-=dwLocalSize;
		}

		delete pMem;

		gEnv->pCryPak->FClose(fp);
	}

	MD5Final(md5, &context);
	return true;
}

//////////////////////////////////////////////////////////////////////////
// attempt to extract the contents of the pak into it's folder
int CCryPak::ExtractPack(const char* fileName)
{
	ICryArchive* pArchive = OpenArchive(fileName, ICryArchive::FLAGS_RELATIVE_PATHS_ONLY | ICryArchive::FLAGS_OPTIMIZED_READ_ONLY | ICryArchive::FLAGS_IGNORE_MODS);
	if(!pArchive)
		return 0;

	ZipDir::Cache* pCache = static_cast<CryArchive*>(pArchive)->GetCache();
	if(!pCache)
		return 0;
	
	ZipDir::DirHeader* pHeader = pCache->GetRoot();
	if(!pHeader)
		return 0;

	CryLog("Extracting from archive %s", fileName);

	string path = fileName;
	const char* slash = strrchr(fileName, '/');
	int newEnd = (slash - fileName);
	path.erase(newEnd, path.length());

	int userPos = path.find("%USER%");
	if(userPos >= 0)
	{
		path.erase(userPos, userPos+6);
		path.insert(userPos, gEnv->pCryPak->GetAlias("%USER%"));
	}				
	int ret = ExtractFilesInFolder(pHeader, path, pCache);

	Unregister(pArchive);
	ClosePack(fileName);

	return ret;
}


int CCryPak::ExtractFilesInFolder(ZipDir::DirHeader* pHeader, string path, ZipDir::Cache* pCache)
{
	if(!pHeader || !pCache)
		return 0;

	int ret = 1;

	// iterate through each set of files for this dir\, extract data
	//	and save it to a file of the correct name/path.
	for(int iFile = 0; iFile < pHeader->numFiles; ++iFile)
	{
		ZipDir::FileEntry* pFile = pHeader->GetFileEntry(iFile);
		if(!pFile)
			return 0;

		CryLog("Extracting file %s", pFile->GetName(pHeader->GetNamePool()));

		// NB: must call free on this data after use
		void* pData = pCache->AllocAndReadFile(pFile);
		if(!pData)
			return 0;

		string fullName = path + "/" + pFile->GetName(pHeader->GetNamePool());
		FILE* file = fopen(fullName.c_str(), "wb");
		if(!file)
		{
			free(pData);
			return 0;
		}

		if(fwrite(pData, pFile->desc.lSizeUncompressed, 1, file) == 0)
		{
			int err = 0;
			_get_errno(&err);
			free(pData);
			return err;
		}

		fclose(file);

		// get the crc of the new file and compare to what the ZIP has recorded
		int crc = ComputeCRC(fullName.c_str());
		if(crc != pFile->desc.lCRC32)
		{
			free(pData);
			return 0;
		}		
		free(pData);
	}

	// also iterate through all subdirs and recursively call this function
	for(int iDir = 0; iDir < pHeader->numDirs; ++iDir)
	{
		ZipDir::DirEntry* pDir = pHeader->GetSubdirEntry(iDir);
		if(pDir)
		{
			CryLog("Found subdir %s", pDir->GetName(pHeader->GetNamePool()));

			string newPath = path + "/" + pDir->GetName(pHeader->GetNamePool());
			int ok = CryCreateDirectory( newPath, NULL );
			ret &= ExtractFilesInFolder(pDir->GetDirectory(), newPath, pCache);
		}
	}

	return ret;
}

int CryArchiveRW::UpdateFileCRC( const char* szRelativePath, const uint32 dwCRC )
{
	if (m_nFlags & FLAGS_READ_ONLY)
		return ZipDir::ZD_ERROR_INVALID_CALL;

	char szFullPath[CCryPak::g_nMaxPath];
	const char*pPath = AdjustPath (szRelativePath, szFullPath);
	if (!pPath)
		return ZipDir::ZD_ERROR_INVALID_PATH;

	m_pCache->UpdateFileCRC(pPath,dwCRC);

	return ZipDir::ZD_ERROR_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////
// deletes the file from the archive
int CryArchiveRW::RemoveFile (const char* szRelativePath)
{
	if (m_nFlags & FLAGS_READ_ONLY)
		return ZipDir::ZD_ERROR_INVALID_CALL;

	char szFullPath[CCryPak::g_nMaxPath];
	const char*pPath = AdjustPath (szRelativePath, szFullPath);
	if (!pPath)
		return ZipDir::ZD_ERROR_INVALID_PATH;
	return m_pCache->RemoveFile (pPath);
}


//////////////////////////////////////////////////////////////////////////
// deletes the directory, with all its descendants (files and subdirs)
int CryArchiveRW::RemoveDir (const char* szRelativePath)
{
	if (m_nFlags & FLAGS_READ_ONLY)
		return ZipDir::ZD_ERROR_INVALID_CALL;

	char szFullPath[CCryPak::g_nMaxPath];
	const char*pPath = AdjustPath (szRelativePath, szFullPath);
	if (!pPath)
		return ZipDir::ZD_ERROR_INVALID_PATH;
	return m_pCache->RemoveDir(pPath);
}

int CryArchiveRW::RemoveAll()
{
	return m_pCache->RemoveAll();

}
int CryArchive::RemoveAll()
{
	return ZipDir::ZD_ERROR_INVALID_CALL;

}

void CCryPak::Register (ICryArchive* pArchive)
{
	ArchiveArray::iterator it = std::lower_bound (m_arrArchives.begin(), m_arrArchives.end(), pArchive, CryArchiveSortByName());
	m_arrArchives.insert (it, pArchive);
}

void CCryPak::Unregister (ICryArchive* pArchive)
{
	if (pArchive)
		CryComment( "Closing PAK file: %s",pArchive->GetFullPath() );
	ArchiveArray::iterator it;
	if (m_arrArchives.size() < 16)
	{
		// for small array sizes, we'll use linear search
		it = std::find (m_arrArchives.begin(), m_arrArchives.end(), pArchive);
	}
	else
		it = std::lower_bound (m_arrArchives.begin(), m_arrArchives.end(), pArchive, CryArchiveSortByName());

	if (it != m_arrArchives.end() && *it == pArchive)
		m_arrArchives.erase (it);
	else
		assert (0); // unregistration of unregistered archive
}

ICryArchive* CCryPak::FindArchive (const char* szFullPath)
{
	ArchiveArray::iterator it = std::lower_bound (m_arrArchives.begin(), m_arrArchives.end(), szFullPath, CryArchiveSortByName());
	if (it != m_arrArchives.end() && !stricmp(szFullPath, (*it)->GetFullPath()))
		return *it;
	else
		return NULL;
}


// compresses the raw data into raw data. The buffer for compressed data itself with the heap passed. Uses method 8 (deflate)
// returns one of the Z_* errors (Z_OK upon success)
// MT-safe
int CCryPak::RawCompress (const void* pUncompressed, unsigned long* pDestSize, void* pCompressed, unsigned long nSrcSize, int nLevel)
{
	return ZipDir::ZipRawCompress(g_pPakHeap, pUncompressed, pDestSize, pCompressed, nSrcSize, nLevel);
}

// Uncompresses raw (without wrapping) data that is compressed with method 8 (deflated) in the Zip file
// returns one of the Z_* errors (Z_OK upon success)
// This function just mimics the standard uncompress (with modification taken from unzReadCurrentFile)
// with 2 differences: there are no 16-bit checks, and
// it initializes the inflation to start without waiting for compression method byte, as this is the
// way it's stored into zip file
int CCryPak::RawUncompress (void* pUncompressed, unsigned long* pDestSize, const void* pCompressed, unsigned long nSrcSize)
{
	return ZipDir::ZipRawUncompress(g_pPakHeap, pUncompressed, pDestSize, pCompressed, nSrcSize);
}

//////////////////////////////////////////////////////////////////////////
void CCryPak::RecordFileOpen( const ERecordFileOpenList eList )
{
	m_eRecordFileOpenList = eList;

	//CryLog("RecordFileOpen(%d)",(int)eList);

	switch(m_eRecordFileOpenList)
	{
		case RFOM_Disabled:
		case RFOM_EngineStartup:
		case RFOM_Level:
			break;

		case RFOM_NextLevel:
		default:
			assert(0);
	}
}

//////////////////////////////////////////////////////////////////////////
IResourceList* CCryPak::GetRecorderdResourceList( const ERecordFileOpenList eList )
{
	switch(eList)
	{
		case RFOM_EngineStartup: return m_pEngineStartupResourceList;
		case RFOM_Level: return m_pLevelResourceList;
		case RFOM_NextLevel: return m_pNextLevelResourceList;

		case RFOM_Disabled:
		default:
			assert(0);
	}
	return 0; 
}


//////////////////////////////////////////////////////////////////////////
void CCryPak::RecordFile( FILE *in, const char *szFilename )
{
	if (m_pLog)
		CryComment( "File open: %s",szFilename );
	
	if (m_eRecordFileOpenList!=ICryPak::RFOM_Disabled)
	{
		if(strnicmp("%USER%",szFilename,6)!=0)		// ignore path to OS settings
		{
			IResourceList *pList = GetRecorderdResourceList(m_eRecordFileOpenList);
			
			if(pList)
				pList->Add( szFilename );
		}
	}
	
	std::vector<ICryPakFileAcesssSink *>::iterator it, end=m_FileAccessSinks.end();

	for(it=m_FileAccessSinks.begin();it!=end;++it)
	{
		ICryPakFileAcesssSink *pSink = *it;

		pSink->ReportFileOpen(in,szFilename);
	}
}


void CCryPak::OnMissingFile (const char* szPath)
{
#if defined(PS3)
	#if defined(_DEBUG)
		//for development we need this to be messaged to the HOST-PC
		printf("CryPak: cannot open file \"%s\"\n",szPath);
	#endif
#else
	AUTO_LOCK(m_csMain);
	if (m_pPakVars->nLogMissingFiles)
	{
		std::pair<MissingFileMap::iterator, bool> insertion = m_mapMissingFiles.insert (MissingFileMap::value_type(szPath,1));
		if (m_pPakVars->nLogMissingFiles >= 2 && (insertion.second || m_pPakVars->nLogMissingFiles >= 3))
		{
			// insertion occured
			char szFileName[64];
			sprintf (szFileName, "MissingFiles%d.log", m_pPakVars->nLogMissingFiles);
			FILE* f = fopen (szFileName, "at");
			if (f)
			{
				fprintf (f, "%s\n", szPath);
				fclose(f);
			}
		}
		else
			++insertion.first->second;  // increase the count of missing files
	}
#endif
}

static char* cry_strdup(const char* szSource)
{
	size_t len = strlen(szSource);
	char* szResult = (char*)malloc(len+1);
	memcpy (szResult, szSource, len+1);
	return szResult;
}


ICryPak::PakInfo* CCryPak::GetPakInfo()
{
	AUTO_LOCK(m_csZips);
	PakInfo* pResult = (PakInfo*)malloc (sizeof(PakInfo) + sizeof(PakInfo::Pak)*m_arrZips.size());
	pResult->numOpenPaks = m_arrZips.size();
	for (unsigned i = 0; i < m_arrZips.size(); ++i)
	{
		pResult->arrPaks[i].szBindRoot = cry_strdup(m_arrZips[i].strBindRoot.c_str());
		pResult->arrPaks[i].szFilePath = cry_strdup(m_arrZips[i].GetFullPath());
		pResult->arrPaks[i].nUsedMem = m_arrZips[i].sizeofThis();
	}
	return pResult;
}

//////////////////////////////////////////////////////////////////////////
void CCryPak::FreePakInfo (PakInfo* pPakInfo)
{
	for (unsigned i = 0; i < pPakInfo->numOpenPaks; ++i)
	{
		free((void*)pPakInfo->arrPaks[i].szBindRoot);
		free((void*)pPakInfo->arrPaks[i].szFilePath);
	}
	free(pPakInfo);
}

//////////////////////////////////////////////////////////////////////////
void CCryPak::Notify( ENotifyEvent event )
{
	switch (event)
	{
	case EVENT_BEGIN_LOADLEVEL:
		m_fFileAcessTime = 0;
		break;
	case EVENT_END_LOADLEVEL:
		{
			// Log used time.
			CryLog( "File access time during level loading: %.2f seconds",m_fFileAcessTime );
			m_fFileAcessTime = 0;
		}
		break;
	}
}


//////////////////////////////////////////////////////////////////////////
void CCryPak::RegisterFileAccessSink( ICryPakFileAcesssSink *pSink )
{
	assert(pSink);

	if(std::find(m_FileAccessSinks.begin(),m_FileAccessSinks.end(),pSink)!=m_FileAccessSinks.end())
	{
		// was already registered
		assert(0);
		return;
	}

	m_FileAccessSinks.push_back(pSink);
}


//////////////////////////////////////////////////////////////////////////
void CCryPak::UnregisterFileAccessSink( ICryPakFileAcesssSink *pSink )
{
	assert(pSink);

	std::vector<ICryPakFileAcesssSink *>::iterator it = std::find(m_FileAccessSinks.begin(),m_FileAccessSinks.end(),pSink);

	if(it==m_FileAccessSinks.end())
	{
		// was not there
		assert(0);
		return;
	}

	m_FileAccessSinks.erase(it);
}

//////////////////////////////////////////////////////////////////////////
bool CCryPak::RemoveFile(const char* pName)
{
	char szFullPathBuf[g_nMaxPath];
	const char *szFullPath = AdjustFileName(pName, szFullPathBuf, 0);
	uint32 dwAttr = CryGetFileAttributes(szFullPath);
	bool ok = false;
	if (dwAttr != INVALID_FILE_ATTRIBUTES && dwAttr != FILE_ATTRIBUTE_DIRECTORY)
	{
#ifdef WIN32
		SetFileAttributes( szFullPath,FILE_ATTRIBUTE_NORMAL );
#endif
		ok = (remove(szFullPath) == 0);
	}
	return ok;
}

//////////////////////////////////////////////////////////////////////////
static void Deltree( const char *szFolder, bool bRecurse )
{
	__finddata64_t fd;
	string filespec = szFolder;
	filespec += "*.*";

	intptr_t hfil = 0;
	if ((hfil = _findfirst64(filespec.c_str(), &fd)) == -1)
	{
		return;
	}

	do
	{
		if (fd.attrib & _A_SUBDIR)
		{
			string name = fd.name;

			if ((name != ".") && (name != ".."))
			{
				if (bRecurse)
				{
					name = szFolder;
					name += fd.name;
					name += "/";

					Deltree(name.c_str(), bRecurse);
				}
			}
		}
		else
		{
			string name = szFolder;

			name += fd.name;

			DeleteFile(name.c_str());
		}

	} while(!_findnext64(hfil, &fd));

	_findclose(hfil);

	RemoveDirectory(szFolder);
}

//////////////////////////////////////////////////////////////////////////
bool CCryPak::RemoveDir(const char* pName, bool bRecurse)
{
	char szFullPathBuf[g_nMaxPath];
	const char *szFullPath = AdjustFileName(pName, szFullPathBuf, 0);
	
	bool ok = false;
	uint32 dwAttr = CryGetFileAttributes(szFullPath);
	if (dwAttr == FILE_ATTRIBUTE_DIRECTORY)
	{
		Deltree(szFullPath, bRecurse);
		ok = true;
	}
	return ok;
}

ILINE bool IsDirSep(const char c)
{
	return (c == CCryPak::g_cNativeSlash || c == CCryPak::g_cNonNativeSlash);
}

//////////////////////////////////////////////////////////////////////////
bool CCryPak::IsAbsPath(const char* pPath)
{
	return (pPath && ((pPath[0] && pPath[1] == ':' && IsDirSep(pPath[2]))
										|| IsDirSep(pPath[0])
									 )
				 );
}

//////////////////////////////////////////////////////////////////////////
void CCryPak::SetLog( IMiniLog *pLog )
{
	m_pLog = pLog;

	if(m_strModPath.size())
	{
		m_pLog->Log("Added MOD directory <%s> to CryPak", m_strModPath.c_str());
	}
}

void * CCryPak::PoolMalloc(size_t size)
{
	return g_pPakHeap->Alloc(size, "CCryPak::GetPoolRealloc", true);
}

void CCryPak::PoolFree(void * p)
{
	g_pPakHeap->Free(p);
}
