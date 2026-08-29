////////////////////////////////////////////////////////////////////////////
//
//  CryEngine Source File.
//  Copyright (C), Crytek, 2001-2007.
// -------------------------------------------------------------------------
//  File name:   WindowsErrorReporting.cpp
//  Created:     16/11/2006 by Timur.
//  Description: Support for Windows Error Reporting (WER)
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#ifdef WIN32

#include "System.h"
#include <windows.h>
#include <tchar.h>
#include "errorrep.h"
#include "CryCommon/CrySystem/ISystem.h"
#include "dbghelp.h"


static WCHAR szPath[MAX_PATH+1];
static WCHAR szFR[] = L"\\System32\\FaultRep.dll";

WCHAR * GetFullPathToFaultrepDll(void)
{
	CHAR *lpRet = NULL;
	UINT rc;

	rc = GetSystemWindowsDirectoryW(szPath, ARRAYSIZE(szPath));
	if (rc == 0 || rc > ARRAYSIZE(szPath) - ARRAYSIZE(szFR) - 1)
		return NULL;

	wcscat(szPath, szFR);
	return szPath;
}


typedef BOOL (WINAPI *MINIDUMPWRITEDUMP)(HANDLE hProcess, DWORD dwPid, HANDLE hFile, MINIDUMP_TYPE DumpType,
																				 CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
																				 CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
																				 CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam
																				 );

//////////////////////////////////////////////////////////////////////////
LONG WINAPI CryEngineExceptionFilterMiniDump( struct _EXCEPTION_POINTERS * pExceptionPointers )
{
	LONG lRet = EXCEPTION_CONTINUE_SEARCH;
	HWND hParent = NULL;	
	HMODULE hDll = NULL;
//	char szDbgHelpPath[_MAX_PATH];

	if (hDll==NULL)
	{
		// load any version we can
		hDll = ::LoadLibrary( "DBGHELP.DLL" );
	}

	const TCHAR * szResult = NULL;

	const TCHAR * m_szAppName = _T("CE2Dump");
	if (hDll)
	{
		MINIDUMPWRITEDUMP pDump = (MINIDUMPWRITEDUMP)::GetProcAddress( hDll, "MiniDumpWriteDump" );
		if (pDump)
		{
//			char szDumpPath[_MAX_PATH];
			char szScratch [_MAX_PATH];


			const char *szDumpPath = gEnv->pCryPak->AdjustFileName( "%USER%/CE2Dump.dmp",szScratch,0 );


			// ask the user if they want to save a dump file
			if (true/*::MessageBox( NULL, "Something bad happened in your program, would you like to save a diagnostic file?", m_szAppName, MB_YESNO )==IDYES*/)
			{
				// create the file
				HANDLE hFile = ::CreateFile( szDumpPath, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
					FILE_ATTRIBUTE_NORMAL, NULL );

				if (hFile!=INVALID_HANDLE_VALUE)
				{
					_MINIDUMP_EXCEPTION_INFORMATION ExInfo;

					ExInfo.ThreadId = ::GetCurrentThreadId();
					ExInfo.ExceptionPointers = pExceptionPointers;
					ExInfo.ClientPointers = NULL;

					// write the dump
					MINIDUMP_TYPE mdumpValue= (MINIDUMP_TYPE)(MiniDumpNormal);
					if (g_cvars.sys_WER > 1)
						mdumpValue= (MINIDUMP_TYPE)(g_cvars.sys_WER - 2);
						

					BOOL bOK = pDump( GetCurrentProcess(), GetCurrentProcessId(), hFile, mdumpValue , &ExInfo, NULL, NULL );
					if (bOK)
					{
						sprintf( szScratch, "Saved dump file to '%s'", szDumpPath );
						szResult = szScratch;
						lRet = EXCEPTION_EXECUTE_HANDLER;
					}
					else
					{
						sprintf( szScratch, "Failed to save dump file to '%s' (error %d)", szDumpPath, GetLastError() );
						szResult = szScratch;
					}
					::CloseHandle(hFile);
				}
				else
				{
					sprintf( szScratch, "Failed to create dump file '%s' (error %d)", szDumpPath, GetLastError() );
					szResult = szScratch;
				}
			}
		}
		else
		{
			szResult = "DBGHELP.DLL too old";
		}
	}
	else
	{
		szResult = "DBGHELP.DLL not found";
	}
	CryLogAlways(szResult);

	return lRet ;
}

/*
struct AutoSetCryEngineExceptionFilter
{
	AutoSetCryEngineExceptionFilter()
	{
		WCHAR * psz = GetFullPathToFaultrepDll();
		SetUnhandledExceptionFilter(CryEngineExceptionFilterWER);
	}
};
AutoSetCryEngineExceptionFilter g_AutoSetCryEngineExceptionFilter;
*/

//////////////////////////////////////////////////////////////////////////
LONG WINAPI CryEngineExceptionFilterWER( struct _EXCEPTION_POINTERS * pExceptionPointers )
{

	if (g_cvars.sys_WER > 1)
		return CryEngineExceptionFilterMiniDump(pExceptionPointers);

	LONG lRet = EXCEPTION_CONTINUE_SEARCH;
	WCHAR * psz = GetFullPathToFaultrepDll();
	if ( psz )
	{
		HMODULE hFaultRepDll = LoadLibraryW( psz ) ;
		if ( hFaultRepDll )
		{
			pfn_REPORTFAULT pfn = (pfn_REPORTFAULT)GetProcAddress( hFaultRepDll,"ReportFault" ) ;
			if ( pfn )
			{
				EFaultRepRetVal rc = pfn( pExceptionPointers, 0) ;
				lRet = EXCEPTION_EXECUTE_HANDLER;
			}
			FreeLibrary(hFaultRepDll );
		}
	}
	return lRet ;
}


#endif // WIN32

