/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Executes an ASCII batch file of console commands...

-------------------------------------------------------------------------
History:
- 19:04:2006   10:38 : Created by Jan Mller
- 26:06:2006           Modified by Timur.

*************************************************************************/

#include "StdAfx.h"
#include "ConsoleBatchFile.h"
#include "CryCommon/CrySystem/IConsole.h"
#include "CryCommon/CrySystem/ISystem.h"
#include "XConsole.h"
#include "CryCommon/CrySystem/CryPath.h"
#include <stdio.h>
#include "System.h"

IConsole* CConsoleBatchFile::m_pConsole = NULL;

void CConsoleBatchFile::Init()
{
	m_pConsole = gEnv->pConsole;
	m_pConsole->AddCommand("exec", (ConsoleCommandFunc)ExecuteFileCmdFunc, 0, "executes a batch file of console commands");
}

//////////////////////////////////////////////////////////////////////////
void CConsoleBatchFile::ExecuteFileCmdFunc( IConsoleCmdArgs* args)
{
	if(!m_pConsole)
		Init();

	if (!args->GetArg(1))
		return;

	ExecuteConfigFile( args->GetArg(1) );
}

//////////////////////////////////////////////////////////////////////////
bool CConsoleBatchFile::ExecuteConfigFile( const char *sFilename )
{
	if(!m_pConsole)
		Init();

	string filename;
	string root = gEnv->pSystem->GetRootFolder();
	if (!root.empty())
		filename = PathUtil::Make( root, PathUtil::GetFile(sFilename) );
	else
		filename = sFilename;
	if (strlen(PathUtil::GetExt(filename.c_str())) == 0)
	{
		filename = PathUtil::ReplaceExtension(filename,"cfg");
	}

	//////////////////////////////////////////////////////////////////////////
	CCryFile file;
	if ( !file.Open(filename.c_str(), "rb") )
	{
		filename = string("config/")+PathUtil::GetFile(filename);
		if ( !file.Open(filename.c_str(), "rb") )
		{
			filename = string("./") + sFilename;
			if ( !file.Open(filename.c_str(), "rb") )
				return false;
		}
	}
	CryLog("Executing %s ...", file.GetFilename());

	int nLen = file.GetLength();
	char *sAllText = new char [nLen + 16];
	file.ReadRaw( sAllText,nLen );
	sAllText[nLen] = '\0';
	sAllText[nLen+1] = '\0';

/*
	This can't work properly as ShowConsole() can be called during the execution of the scripts,
	which means bConsoleStatus is outdated and must not be set at the end of the function

	bool bConsoleStatus = ((CXConsole*)m_pConsole)->GetStatus();
	((CXConsole*)m_pConsole)->SetStatus(false);
*/

	char *strLast = sAllText+nLen;
	char *str = sAllText;
	while (str < strLast)
	{
		char *s = str;
		while (str < strLast && *str != '\n' && *str != '\r')
			str++;
		*str = '\0';
		str++;
		while (str < strLast && (*str == '\n' || *str == '\r'))
			str++;

		const char *strLine = s;

		// skip comments and empty lines.
		if (*strLine != 0 && *strLine != '-')
		{
			m_pConsole->ExecuteString(strLine);
		}
	}
	// See above
//	((CXConsole*)m_pConsole)->SetStatus(bConsoleStatus);

	delete []sAllText;
	return true;
}
