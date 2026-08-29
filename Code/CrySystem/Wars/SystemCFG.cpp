
//////////////////////////////////////////////////////////////////////
//
//	Crytek CryENGINE Source code
// 
//	File: SystemCFG.cpp
//  Description: handles system cfg
// 
//	History:
//	-Jan 21,2004: created
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h" 
#include "System.h"
#include <time.h>
#include "XConsole.h"
#include "CryCommon/CrySystem/CryFile.h"

#include "CryCommon/CryScriptSystem/IScriptSystem.h"
#include "SystemCFG.h" 
#if defined(LINUX)
#include "CryCommon/CrySystem/ILog.h"
#endif

#ifndef VERSION_INFO
#define VERSION_INFO 1
#endif 

//////////////////////////////////////////////////////////////////////////
const SFileVersion& CSystem::GetFileVersion()
{
	return m_fileVersion;
}

//////////////////////////////////////////////////////////////////////////
const SFileVersion& CSystem::GetProductVersion()
{
	return m_productVersion;
}

//////////////////////////////////////////////////////////////////////////
void CSystem::QueryVersionInfo()
{
	// CryMP: CrySystem is embedded in CryMP-Client.exe, so querying
	// CrySystem.dll is no longer reliable (and fails under Wine/Linux).
	// Keep the engine ABI/version exposed to the rest of Crysis fixed to
	// the supported Crysis 1.1.1.6156 version.
	m_fileVersion.v[3] = 1;
	m_fileVersion.v[2] = 1;
	m_fileVersion.v[1] = 1;
	m_fileVersion.v[0] = 6156;

	m_productVersion = m_fileVersion;
}

//////////////////////////////////////////////////////////////////////////
void CSystem::LogVersion()
{
#if defined(WIN32) || defined(LINUX)
	// Get time.
	time_t ltime;
	time( &ltime );
	tm *today = localtime( &ltime );

	char s[1024];

	strftime( s,128,"Date(%d %b %Y) Time(%H %M %S)",today);		

	const SFileVersion &ver = GetFileVersion();

	CryLogAlways("BackupNameAttachment=\" Build(%d) %s\"  -- used by backup system",ver.v[0],s);			// read by CreateBackupFile()
	CryLogAlways(" ");		// empty line

	// Use strftime to build a customized time string.
	strftime( s,128,"Log Started at %#c", today );
	CryLogAlways( s );

#if defined(WIN64) || defined(LINUX64)
	CryLogAlways( "Running 64 bit version" );
#else
	CryLogAlways( "Running 32 bit version" );
#endif
#ifdef WIN32
	GetModuleFileName(NULL,s,sizeof(s));
	CryLogAlways( "Executable: %s",s );
#endif
#endif

	CryLogAlways( "FileVersion: %d.%d.%d.%d",m_fileVersion.v[3],m_fileVersion.v[2],m_fileVersion.v[1],m_fileVersion.v[0] );
	CryLogAlways( "ProductVersion: %d.%d.%d.%d",m_productVersion.v[3],m_productVersion.v[2],m_productVersion.v[1],m_productVersion.v[0] );

#ifdef WIN32
#ifdef STLPORT
	CryLogAlways( "Using STLport C++ Standard Library implementation" );
#else //STLPORT
	CryLogAlways( "Using Microsoft (tm) C++ Standard Library implementation" );
#endif //STLPORT
#endif //WIN32

	CryLogAlways( " " );
}



//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
class CCVarSaveDump : public ICVarDumpSink
{
public:

	CCVarSaveDump(FILE *pFile)
	{
		m_pFile=pFile;
	}

	virtual void OnElementFound(ICVar *pCVar)
	{
		if (!pCVar)
			return;
		int nFlags = pCVar->GetFlags();
		if (((nFlags & VF_DUMPTODISK) && (nFlags & VF_MODIFIED)) || (nFlags & VF_WASINCONFIG))
		{
			string szValue = pCVar->GetString();
			int pos;

			// replace \ with \\ 
			pos = 1;
			for(;;)
			{
				pos = szValue.find_first_of("\\", pos);

				if (pos == string::npos)
				{
					break;
				}

				szValue.replace(pos, 1, "\\\\", 2);
				pos+=2;
			}

			// replace " with \" 
			pos = 1;
			for(;;)
			{
				pos = szValue.find_first_of("\"", pos);

				if (pos == string::npos)
				{
					break;
				}

				szValue.replace(pos, 1, "\\\"", 2);
				pos+=2;
			}

			string szLine = pCVar->GetName();

			if(pCVar->GetType()==CVAR_STRING)
				szLine += " = \"" + szValue + "\"\r\n";
			 else
				szLine += " = " + szValue + "\r\n";

			if(pCVar->GetFlags()&VF_WARNING_NOTUSED)
				fputs("-- REMARK: the following was not assigned to a console variable\r\n",m_pFile);

			fputs(szLine.c_str(), m_pFile);
		}
	}

private: // --------------------------------------------------------

	FILE *				m_pFile;					//
};

//////////////////////////////////////////////////////////////////////////
void CSystem::SaveConfiguration()
{
/*	// save config before we quit
	if (!m_env.pGame)
		return;

	// get player's profile
	ICVar *pProfile=m_env.pConsole->GetCVar("g_playerprofile");
	if (pProfile)
	{	
		const char *sProfileName=pProfile->GetString();
		//m_env.pGame->SaveConfiguration( "system.cfg","game.cfg",sProfileName);
	}
	// always save the current profile in the root, otherwise, nexttime, the game will have the default one
	// wich is annoying
	//m_env.pGame->SaveConfiguration( "system.cfg","game.cfg",NULL);

	m_rDriver->Set(sSave.c_str());
*/

	if(m_rDriver && m_sSavedRDriver!="")
		m_rDriver->Set(m_sSavedRDriver.c_str());

	// the following code is simple replacement for the old code
	// without profiles and without saving of the actionmap
	// todo: improve and remove this comment

	if(m_sys_SaveCVars->GetIVal())
	{
		FILE *pFile=fxopen("system.cfg","wb");
		if(pFile)
		{
			fputs("-- [System-Configuration]\r\n",pFile);
			fputs("-- Attention: This file is generated by the system! Editing is not recommended! \r\n\r\n",pFile);

			CCVarSaveDump SaveDump(pFile);
			
			GetIConsole()->DumpCVars(&SaveDump);
			
			fclose(pFile); 

			GetILog()->Log("SaveConfiguration() succeeded");
		}
		else GetILog()->LogError("SaveConfiguration() failed");
	}

}

//////////////////////////////////////////////////////////////////////////
// system cfg
//////////////////////////////////////////////////////////////////////////
CSystemConfiguration::CSystemConfiguration(const string& strSysConfigFilePath,CSystem *pSystem, ILoadConfigurationEntrySink *pSink )
: m_strSysConfigFilePath( strSysConfigFilePath ), m_bError(false), m_pSink(pSink)
{
	assert(pSink);

	m_pSystem = pSystem;
	m_bError = !ParseSystemConfig();
}

//////////////////////////////////////////////////////////////////////////
CSystemConfiguration::~CSystemConfiguration()
{
}

//////////////////////////////////////////////////////////////////////////
bool CSystemConfiguration::ParseSystemConfig()
{
	string filename = m_strSysConfigFilePath;
	if (strlen(PathUtil::GetExt(filename.c_str())) == 0)
	{
		filename = PathUtil::ReplaceExtension(filename,"cfg");
	}

	CCryFile file;
	if (!file.Open(filename.c_str(), "rb", ICryPak::FOPEN_HINT_QUIET))
	{
		if (!file.Open((string("config/") + filename).c_str(), "rb", ICryPak::FOPEN_HINT_QUIET))
		{
			if (!file.Open((string("./") + filename).c_str(), "rb", ICryPak::FOPEN_HINT_QUIET))
				return false;
		}
	}
	
	int nLen = file.GetLength();
	char *sAllText = new char [nLen + 16];
	file.ReadRaw( sAllText,nLen );
	sAllText[nLen] = '\0';
	sAllText[nLen+1] = '\0';
	
	string strGroup;			// current group e.g. "[General]"

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


		string strLine = s;

		// detect groups e.g. "[General]"   should set strGroup="General"
		{
			string strTrimmedLine( RemoveWhiteSpaces(strLine) );
			size_t size = strTrimmedLine.size();

			if(size>=3)
			if(strTrimmedLine[0]=='[' && strTrimmedLine[size-1]==']')		// currently no comments are allowed to be behind groups
			{
				strGroup = &strTrimmedLine[1];strGroup.resize(size-2);		// remove [ and ]
				continue;																									// next line
			}
		}

		// skip comments
		if (0<strLine.find( "--" ))
		{
			// extract key
			string::size_type posEq( strLine.find( "=", 0 ) );
			if (string::npos!=posEq)
			{
				string stemp( strLine, 0, posEq );
				string strKey( RemoveWhiteSpaces(stemp) );

//				if (!strKey.empty())
				{
					// extract value
					string::size_type posValueStart( strLine.find( "\"", posEq + 1 ) + 1 );
					// string::size_type posValueEnd( strLine.find( "\"", posValueStart ) );
					string::size_type posValueEnd( strLine.rfind( '\"' ) );

					string strValue;

					if( string::npos != posValueStart && string::npos != posValueEnd )
						strValue=string( strLine, posValueStart, posValueEnd - posValueStart );
					else
					{
						string strTmp( strLine, posEq + 1, strLine.size()-(posEq + 1) );
						strValue = RemoveWhiteSpaces(strTmp);
					}

					{
						string strTemp;
						strTemp.reserve(strValue.length()+1);
						// replace '\\\\' with '\\' and '\\\"' with '\"'
						for (string::const_iterator iter = strValue.begin(); iter != strValue.end(); ++iter)
						{
							if (*iter == '\\')
							{
								++iter;
								if (iter == strValue.end())
									;
								else if (*iter == '\\')
									strTemp	+= '\\';
								else if (*iter == '\"')
									strTemp += '\"';
							}
							else
								strTemp += *iter;
						}
						strValue.swap( strTemp );

//						m_pSystem->GetILog()->Log("Setting %s to %s",strKey.c_str(),strValue.c_str());
						m_pSink->OnLoadConfigurationEntry(strKey.c_str(), strValue.c_str(), strGroup.c_str());
					}
				}					
			}
		} //--
	}

	delete []sAllText;

	m_pSink->OnLoadConfigurationEntry_End();

	return true;
}


//////////////////////////////////////////////////////////////////////////
void CSystem::OnLoadConfigurationEntry( const char *szKey, const char *szValue, const char *szGroup )
{
	if(*szKey!=0)
		gEnv->pConsole->LoadConfigVar( szKey,szValue );
}

//////////////////////////////////////////////////////////////////////////
void CSystem::LoadConfiguration( const char *sFilename, ILoadConfigurationEntrySink *pSink )
{
	if (sFilename && strlen(sFilename) > 0)
	{	
		if(!pSink)
		{
			// log only if no user sink is used
			CryLog("Loading configuration file '%s' ... ",sFilename );
		
			pSink = this;
		}

		CSystemConfiguration tempConfig(sFilename,this,pSink);

		if (tempConfig.IsError()) 
			CryLog("Configuration file '%s' not loaded",sFilename);
	}
}

