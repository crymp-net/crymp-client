/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Implements the command line interface ICmdLine.
  
 -------------------------------------------------------------------------
  History:
  - 2:8:2004   15:19 : Created by Mrcio Martins

*************************************************************************/
#ifndef __CMDLINE_H__
#define __CMDLINE_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include "CryCommon/CrySystem/ICmdLine.h"
#include "CmdLineArg.h"


class CCmdLine :
	public ICmdLine
{
public:
	CCmdLine(const char *commandLine);
	virtual ~CCmdLine();

	virtual const ICmdLineArg *GetArg(int n) const;
	virtual int GetArgCount() const;
	virtual const ICmdLineArg *FindArg( const ECmdLineArgType ArgType, const char *name, bool caseSensitive = false ) const;
	virtual const char* GetCommandLine() const { return m_sCmdLine.c_str(); };

private:
	void PushCommand( const string &sCommand, const string &sParameter );
	string Next(char *&str);

	string m_sCmdLine;
	std::vector<CCmdLineArg>	m_args;

};


#endif //__CMDLINE_H__