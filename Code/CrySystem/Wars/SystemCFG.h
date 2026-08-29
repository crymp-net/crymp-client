//////////////////////////////////////////////////////////////////////
//
//	Crytek CryENGINE Source code (c) 2002-2004
// 
//	File: SystemCfg.h	
// 
//	History:
//	-Jan 22,2004:Created 
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cmath>
#include <map>

typedef string SysConfigKey;
typedef string SysConfigValue;

//////////////////////////////////////////////////////////////////////////
class CSystemConfiguration
{
public:
	CSystemConfiguration( const string& strSysConfigFilePath,CSystem *pSystem, ILoadConfigurationEntrySink *pSink );
	~CSystemConfiguration();

	string RemoveWhiteSpaces( string& s )
	{
		s.Trim();
		return s;
	}

	bool IsError() const { return m_bError; }

private: // ----------------------------------------

	// Returns:
	//   success
	bool ParseSystemConfig();

	CSystem	*												m_pSystem;
	string													m_strSysConfigFilePath;	
	bool														m_bError;
	ILoadConfigurationEntrySink *		m_pSink;										// never 0
};

