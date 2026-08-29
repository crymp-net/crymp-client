	
//////////////////////////////////////////////////////////////////////
//
//	Crytek CryENGINE Source code
//	
//	File:Log.cpp
//  Description:Log related functions
//
//	History:
//	-Feb 2,2001:Created by Marco Corbetta
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "Log.h"

//this should not be included here
#include "CryCommon/CrySystem/IConsole.h"
#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CrySystem/IStreamEngine.h"
#include "CryCommon/CryNetwork/INetwork.h"  // EvenBalance - M. Quinn
#include "System.h"
#include "CryCommon/CrySystem/CryPath.h"					// PathUtil::ReplaceExtension()

#if !defined(CRYMP_EMBEDDED_CRYSYSTEM) && !defined(PS3) && !defined(XENON)
	#include <PunkBuster/pbcommon.h>  // EvenBalance - M. Quinn
#endif

#ifdef WIN32
#include <time.h>
#endif

#if defined(PS3)
	#include <sys/memory.h>
	#include <IJobManSPU.h>
	#if defined(SNTUNER)
		#include <Lib/libsntuner.h>
	#endif
	__attribute__((always_inline))
	inline const unsigned int GetStackAddress()
	{
		unsigned int __sp __asm__ ("1");
		return __sp;
	}

	//need these 2 globals to log current stack usage and size, defined in PS3Launcher
	#if defined(_LIB)
		#define g_StackSize ((uint32)(gPS3Env->nMainStackSize))
		#define g_StackPtr ((uint32)gPS3Env->pMainStack)
	#else
		#define g_StackSize (4 * 1024 * 1024)
		#define g_StackPtr	(0)
	#endif
#endif

//#define RETURN return
#define RETURN

// Only accept logging from the main thread.
#ifdef WIN32
#define THREAD_RETURN                           \
	{                                             \
		uint32 nCurrThread = GetCurrentThreadId();  \
		if (nCurrThread != m_nMainThreadId)         \
			return;                                   \
	}

#else
#define THREAD_RETURN
#endif //WIN32

//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
CLog::CLog( ISystem *pSystem )
{
	memset(m_szFilename,0,MAX_FILENAME_SIZE);	
	memset(m_szTemp,0,MAX_TEMP_LENGTH_SIZE);
	m_pSystem=pSystem;
	m_pLogVerbosity = 0;
	m_pLogFileVerbosity = 0;
	m_pLogIncludeTime = 0;

#ifdef WIN32
	m_nMainThreadId = GetCurrentThreadId();
#endif //WIN32

#if defined(PS3)
	assert(GetIJobManSPU());
	GetIJobManSPU()->SetLog((ILog*)this); 
#endif
}

void CLog::RegisterConsoleVariables()
{
	IConsole *console = m_pSystem->GetIConsole();			assert(console);

	#define DEFAULT_VERBOSITY 3

	if(console)
	{
		m_pLogVerbosity = console->RegisterInt("log_Verbosity",DEFAULT_VERBOSITY,VF_DUMPTODISK,
			"defines the verbosity level for console log messages (use log_FileVerbosity for file logging)\n"
			"0=suppress all logs(except eAlways)\n"
			"1=additional errors\n"
			"2=additional warnings\n"
			"3=additional messages\n"
			"4=additional comments");

		m_pLogFileVerbosity = console->RegisterInt("log_FileVerbosity",DEFAULT_VERBOSITY,VF_DUMPTODISK,
			"defines the verbosity level for file log messages (if log_Verbosity defines higher value this one is used)\n"
			"0=suppress all logs(except eAlways)\n"
			"1=additional errors\n"
			"2=additional warnings\n"
			"3=additional messages\n"
			"4=additional comments");

		// put time into begin of the string if requested by cvar
		m_pLogIncludeTime = console->RegisterInt("log_IncludeTime",0,0,
			"Toggles time stamping of log entries.\n"
			"Usage: log_IncludeTime [0/1/2/3/4]\n"
			"  0=off (default)\n"
			"  1=current time\n"
			"  2=relative time\n"
			"  3=current+relative time\n"
			"  4=absolute time in seconds since this mode was started");
	}
/*
	//testbed
	{
		int iSave0 = m_pLogVerbosity->GetIVal();
		int iSave1 = m_pLogFileVerbosity->GetIVal();

		for(int i=0;i<=4;++i)
		{
			m_pLogVerbosity->Set(i);
			m_pLogFileVerbosity->Set(i);

			LogWithType(eAlways,"CLog selftest: Verbosity=%d FileVerbosity=%d",m_pLogVerbosity->GetIVal(),m_pLogFileVerbosity->GetIVal());
			LogWithType(eAlways,"--------------");

			LogWithType(eError,"eError");
			LogWithType(eWarning,"eWarning");
			LogWithType(eMessage,"eMessage");
			LogWithType(eInput,"eInput");
			LogWithType(eInputResponse,"eInputResponse");

			LogWarning("LogWarning()");
			LogError("LogError()");
			LogWithType(eAlways,"--------------");
		}

		m_pLogVerbosity->Set(iSave0);
		m_pLogFileVerbosity->Set(iSave1);
	}
*/
	#undef DEFAULT_VERBOSITY
}

//////////////////////////////////////////////////////////////////////
CLog::~CLog()
{
	CreateBackupFile();

	UnregisterConsoleVariables();
}

void CLog::UnregisterConsoleVariables()
{
	m_pLogVerbosity = 0;
	m_pLogFileVerbosity = 0;
	m_pLogIncludeTime = 0;
}

//////////////////////////////////////////////////////////////////////////
FILE* CLog::OpenLogFile( const char *filename, const char *mode )
{
#if defined(PS3)
	//the compiler did not treat ::fopen properly
	#undef fopen
	return ::fopen(filename, mode);
	#define fopen WrappedFopen
#elif defined(XENON)
	return 0;
#else
	return fxopen( filename,mode );
#endif
}


//////////////////////////////////////////////////////////////////////////
void CLog::SetVerbosity( int verbosity )
{
	RETURN;

	if(m_pLogVerbosity)
		m_pLogVerbosity->Set(verbosity);
}

//////////////////////////////////////////////////////////////////////////
void CLog::LogWarning(const char *szFormat,...)
{
	THREAD_RETURN;

	va_list	ArgList;
	char		szBuffer[MAX_WARNING_LENGTH];
	va_start(ArgList, szFormat);
	vsprintf_s(szBuffer, sizeof(szBuffer), szFormat, ArgList);
	LogV( eWarning, szFormat, ArgList);
	va_end(ArgList);

	IValidator *pValidator = m_pSystem->GetIValidator();
	if (pValidator)
	{
		SValidatorRecord record;
		record.text = szBuffer;
		record.module = VALIDATOR_MODULE_SYSTEM;
		record.severity = VALIDATOR_WARNING;
		pValidator->Report( record );
	}
}

//////////////////////////////////////////////////////////////////////////
void CLog::LogError(const char *szFormat,...)
{
	THREAD_RETURN;

	va_list	ArgList;
	char		szBuffer[MAX_WARNING_LENGTH];
	va_start(ArgList, szFormat);
	vsprintf_s(szBuffer, sizeof(szBuffer), szFormat, ArgList);
	LogV( eError, szFormat, ArgList);
	va_end(ArgList);

	IValidator *pValidator = m_pSystem->GetIValidator();
	if (pValidator)
	{
		SValidatorRecord record;
		record.text = szBuffer;
		record.module = VALIDATOR_MODULE_SYSTEM;
		record.severity = VALIDATOR_ERROR;
		pValidator->Report( record );
	}
}

//////////////////////////////////////////////////////////////////////////
void CLog::Log(const char *szFormat,...)
{
	RETURN;

	if (m_pLogVerbosity && !m_pLogVerbosity->GetIVal())
	{
		if (m_pLogFileVerbosity && !m_pLogFileVerbosity->GetIVal())
		{
			return;
		}
	}
	va_list arg;
	va_start(arg, szFormat);
	LogV (eMessage, szFormat, arg);
	va_end(arg);
}

//will log the text both to file and console
//////////////////////////////////////////////////////////////////////
void CLog::LogV( const ELogType type, const char* szFormat, va_list args )
{
#if defined(PS3)
	if(gPS3Env->bDisableLog)
		return;//runs through network to Host-PC
#endif

	RETURN;
	if (!szFormat)
		return;

	THREAD_RETURN;

	static ISystem * pSys = 0;
	if (!pSys)
		pSys = GetISystem();

	FUNCTION_PROFILER(pSys, PROFILE_SYSTEM);

	bool bfile = false, bconsole = false;
	const char* szCommand = szFormat;
	if (type == eMessage || type == eWarning || type == eError || type == eComment)
	{
		uint8 DefaultVerbosity=0;

		switch(type)
		{
			case eError: DefaultVerbosity=1;break;
			case eWarning: DefaultVerbosity=2;break;
			case eMessage: DefaultVerbosity=3;break;
			case eComment: DefaultVerbosity=4;break;
			default: assert(0);
		}

		szCommand = CheckAgainstVerbosity(szFormat, bfile, bconsole,DefaultVerbosity);
		if (!bfile && !bconsole)
			return;
	}
	else
	{
		bfile = true;
		if (type == eInput || type == eInputResponse)
		{
			const unsigned char nMaxVerbosity = 8;
			int nLogFileVerbosity = m_pLogFileVerbosity ? m_pLogFileVerbosity->GetIVal() : nMaxVerbosity;
			if (nLogFileVerbosity == 0)
				bfile = 0;
		}
		bconsole = true; // console always true.
	}

	bool bError = false;
	
	char szBuffer[MAX_WARNING_LENGTH+32];
	char *szString = szBuffer;
	char *szAfterColour = szBuffer;

	switch(type)
	{
		case eWarning:
		case eWarningAlways:
			bError = true;
			strcpy( szString,"$6[Warning] " );
			szString += 12;	// strlen("$6[Warning] ");
			szAfterColour += 2;
			break;

		case eError:
		case eErrorAlways:
			bError = true;
			strcpy( szString,"$4[Error] " );
			szString += 10;	// strlen("$4[Error] ");
			szAfterColour += 2;
			break;

//		default:
	//		assert(0);
	}
	
	vsprintf_s( szString, sizeof(szBuffer)-32, szCommand, args );
	szBuffer[sizeof(szBuffer)-8]=0;

#if defined(PS3) && defined(SNTUNER)
	static int counter = 0;
	if(counter > 0)
		snStopMarker(counter-1);
	snStartMarker(counter++, szBuffer);
#endif

	if (bfile)
		LogStringToFile( szAfterColour,false,bError );
	if (bconsole) {
#ifdef __WITH_PB__
		// Send the console output to PB for audit purposes
		if ( gEnv->pNetwork )
			gEnv->pNetwork->PbCaptureConsoleLog (szBuffer, strlen(szBuffer)) ;
#endif
		LogStringToConsole( szBuffer );
	}
}

//will log the text both to the end of file and console
//////////////////////////////////////////////////////////////////////
void CLog::LogPlus(const char *szFormat,...)
{
	if (m_pLogVerbosity && !m_pLogVerbosity->GetIVal())
	{
		if (m_pLogFileVerbosity && !m_pLogFileVerbosity->GetIVal())
		{
			return;
		}
	}

	THREAD_RETURN;

	RETURN;
	if (!szFormat)
		return;

	bool bfile = false, bconsole = false;
	const char* szCommand = CheckAgainstVerbosity(szFormat, bfile, bconsole);
	if (!bfile && !bconsole)
		return;

	va_list		arglist;	

	va_start(arglist, szFormat);
	vsprintf_s(m_szTemp, sizeof(m_szTemp), szCommand, arglist);
	m_szTemp[sizeof(m_szTemp)-8]=0;
	va_end(arglist);	

	if (bfile)
		LogToFilePlus(m_szTemp);		
	if (bconsole)
		LogToConsolePlus(m_szTemp);	
}

//log to console only
//////////////////////////////////////////////////////////////////////
void CLog::LogStringToConsole( const char *szString,bool bAdd )
{
	if (!szString || !szString[0])
		return;

	if (!m_pSystem)
		return;
	IConsole *console = m_pSystem->GetIConsole();
	if (!console)
		return;

	LogStringType tempString;
	tempString = szString;
	if (tempString.length() > MAX_TEMP_LENGTH_SIZE)
		tempString.erase( MAX_TEMP_LENGTH_SIZE );
	// add \n at end.
	if (tempString.length() > 0 && tempString[tempString.length()-1] != '\n')
	{
		tempString += '\n';
	}

	if (bAdd)
		console->PrintLinePlus(tempString.c_str());	
	else
		console->PrintLine(tempString.c_str());

	// Call callback function.
	if (!m_callbacks.empty())
	{
		for (Callbacks::iterator it = m_callbacks.begin(); it != m_callbacks.end(); ++it)
		{
			(*it)->OnWriteToConsole(tempString.c_str(),!bAdd);
		}
	}
}

//log to console only
//////////////////////////////////////////////////////////////////////
void CLog::LogToConsole(const char *szFormat,...)
{
	if (m_pLogVerbosity && !m_pLogVerbosity->GetIVal())
	{
		if (m_pLogFileVerbosity && !m_pLogFileVerbosity->GetIVal())
		{
			return;
		}
	}

	RETURN;
	THREAD_RETURN;

	if (!szFormat)
		return;

	bool bfile = false, bconsole = false;
	const char* szCommand = CheckAgainstVerbosity(szFormat, bfile, bconsole);
	if (!bconsole)
		return;

	va_list		arglist;	

	char szBuffer[MAX_WARNING_LENGTH];
	va_start(arglist, szFormat);
	vsprintf_s(szBuffer, sizeof(szBuffer), szCommand, arglist);
	szBuffer[sizeof(szBuffer)-1]=0;
	va_end(arglist);

	LogStringToConsole( szBuffer );
}

//////////////////////////////////////////////////////////////////////
void CLog::LogToConsolePlus(const char *szFormat,...)
{
	if (m_pLogVerbosity && !m_pLogVerbosity->GetIVal())
	{
		if (m_pLogFileVerbosity && !m_pLogFileVerbosity->GetIVal())
		{
			return;
		}
	}

	RETURN;
	THREAD_RETURN;

	if (!szFormat)
		return;

	bool bfile = false, bconsole = false;
	const char* szCommand = CheckAgainstVerbosity(szFormat, bfile, bconsole);
	if (!bconsole)
		return;

	va_list		arglist;
	
	va_start(arglist, szFormat);
	vsprintf_s(m_szTemp, sizeof(m_szTemp), szCommand, arglist);
	m_szTemp[sizeof(m_szTemp)-1]=0;
	va_end(arglist);	

	if (!m_pSystem)
		return;

	LogStringToConsole( m_szTemp,true );
}

//////////////////////////////////////////////////////////////////////
void CLog::LogStringToFile( const char *szString,bool bAdd,bool bError )
{
#if defined(PS3)
	if(gPS3Env->bDisableLog)
		return;//runs through network to Host-PC
#endif

	if (!szString || !szString[0])
		return;

	if (!m_pSystem)
		return;
	IConsole * console = m_pSystem->GetIConsole();

	LogStringType tempString;
	tempString = szString;

	if (tempString.length() > 0 && tempString.at(0) < 32)
	{
		tempString.erase(0,1);
	}
	if (tempString.length() > 1 && tempString.at(0) == '$') // skip color code.
	{
		tempString.erase(0,2);
	}
	if (tempString.length() == 0)
		return;

	if(m_pLogIncludeTime && gEnv->pTimer)
	{
		uint32 dwCVarState=m_pLogIncludeTime->GetIVal();

		if(dwCVarState==1)				// Log_IncludeTime
		{
			char sTime[128];
			time_t ltime;
			time( &ltime );
			struct tm *today = localtime( &ltime );
			strftime( sTime, 20, "<%H:%M:%S> ", today );

			tempString = LogStringType(sTime) + tempString;
		}
		else if(dwCVarState==2)		// Log_IncludeTime
		{
			static CTimeValue lasttime;
			CTimeValue currenttime = gEnv->pTimer->GetAsyncTime();
			if(lasttime!=CTimeValue())
			{
				LogStringType timeStr;
				uint32 dwMs=(uint32)((currenttime-lasttime).GetMilliSeconds());
				timeStr.Format( "<%3d.%.3d>: ", dwMs/1000,dwMs%1000 );
				tempString = timeStr + tempString;
			}
			lasttime = currenttime;
		}
		else if(dwCVarState==3)		// Log_IncludeTime
		{
			char sTime[128];
			time_t ltime;
			time( &ltime );
			struct tm *today = localtime( &ltime );
			strftime( sTime, 20, "<%H:%M:%S> ", today );
			tempString = LogStringType(sTime) + tempString;

			static CTimeValue lasttime;
			CTimeValue currenttime = gEnv->pTimer->GetAsyncTime();
			if(lasttime!=CTimeValue())
			{
				LogStringType timeStr;
				uint32 dwMs=(uint32)((currenttime-lasttime).GetMilliSeconds());
				timeStr.Format( "<%3d.%.3d>: ", dwMs/1000,dwMs%1000 );
				tempString = timeStr + tempString;
			}
			lasttime = currenttime;
		}
		else if(dwCVarState==4)				// Log_IncludeTime
		{
			static bool bFirst = true;
			
			if(gEnv->pTimer)
			{
				static CTimeValue lasttime;
				CTimeValue currenttime = gEnv->pTimer->GetAsyncTime();
				if(lasttime!=CTimeValue())
				{
					LogStringType timeStr;
					uint32 dwMs=(uint32)((currenttime-lasttime).GetMilliSeconds());
					timeStr.Format( "<%3d.%.3d>: ", dwMs/1000,dwMs%1000 );
					tempString = timeStr + tempString;
				}
				if(bFirst)
				{
					lasttime = currenttime;
					bFirst=false;
				}
			}
		}
	}

	// add \n at end.
	if (tempString.length() > 0 && tempString[tempString.length()-1] != '\n')
	{
		tempString += '\n';
	}

	//////////////////////////////////////////////////////////////////////////
	// Call callback function.
	if (!m_callbacks.empty())
	{
		for (Callbacks::iterator it = m_callbacks.begin(); it != m_callbacks.end(); ++it)
		{
			(*it)->OnWriteToFile(tempString.c_str(),!bAdd);
		}
	}
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Write to file.
	//////////////////////////////////////////////////////////////////////////
	if (bAdd)
	{
		FILE *fp=OpenLogFile(m_szFilename,"r+t");
		if (fp)
		{
			int p1 = ftell(fp);
			fseek(fp,0,SEEK_END);
			p1 = ftell(fp);
			fseek(fp,-2,SEEK_CUR);
			p1 = ftell(fp);

			fputs(tempString.c_str(),fp);		
			fclose(fp);
		}
	}
	else
	{
		// comment on bug by TN: Log file misses the last lines of output
		// Temporally forcing the Log to flush before closing the file, so all lines will show up
		if(FILE * fp = OpenLogFile(m_szFilename,"at")) // change to option "atc"
		{
			fputs(tempString.c_str(),fp);
			// fflush(fp);  // enable to flush the file
			fclose(fp);
		}
	}

	if (bError)
	{
		static int errcount = 0;
#if !defined(PS3)	//for PS3 we do not want too much log traffic over network (file io is slow)

#if defined(XENON)
		OutputDebugString( tempString.c_str() );
#endif
/*
		char errfile[MAX_PATH];
		strcpy( errfile,m_szFilename );
		strcat( errfile,".err" );
		if(FILE * fp = OpenLogFile(errfile,"at"))
		{
			fprintf( fp,"[%d] %s",errcount,szTemp );
			fclose(fp);
		}
		*/
#endif
		errcount++;
	}
}

//same as above but to a file
//////////////////////////////////////////////////////////////////////
void CLog::LogToFilePlus(const char *szFormat,...)
{
	if (m_pLogVerbosity && !m_pLogVerbosity->GetIVal())
	{
		if (m_pLogFileVerbosity && !m_pLogFileVerbosity->GetIVal())
		{
			return;
		}
	}

	RETURN;
	THREAD_RETURN;

	if (!m_szFilename[0] || !szFormat) 
		return;

	bool bfile = false, bconsole = false;
	const char* szCommand = CheckAgainstVerbosity(szFormat, bfile, bconsole);
	if (!bfile)
		return;

	va_list		arglist;	
	va_start(arglist, szFormat);
	vsprintf_s(m_szTemp, sizeof(m_szTemp), szCommand, arglist);
	m_szTemp[sizeof(m_szTemp)-8]=0;
	va_end(arglist);	

	LogStringToFile( m_szTemp,true );
}

//log to the file specified in setfilename
//////////////////////////////////////////////////////////////////////
void CLog::LogToFile(const char *szFormat,...)
{
	if (m_pLogVerbosity && !m_pLogVerbosity->GetIVal())
	{
		if (m_pLogFileVerbosity && !m_pLogFileVerbosity->GetIVal())
		{
			return;
		}
	}

	RETURN;
	THREAD_RETURN;

	if (!m_szFilename[0] || !szFormat) 
		return;	 

	bool bfile = false, bconsole = false;
	const char* szCommand = CheckAgainstVerbosity(szFormat, bfile, bconsole);
	if (!bfile)
		return;

	va_list		arglist;  	
	va_start(arglist, szFormat);
	vsprintf_s(m_szTemp, sizeof(m_szTemp), szCommand, arglist);
	m_szTemp[sizeof(m_szTemp)-16]=0;
	va_end(arglist);	

	LogStringToFile( m_szTemp,false );
}


//////////////////////////////////////////////////////////////////////
void CLog::CreateBackupFile() const
{
#if defined(WIN32) || defined(LINUX)
	// simple:
	//		string bakpath = PathUtil::ReplaceExtension(m_szFilename,"bak");
	//		CopyFile(m_szFilename,bakpath.c_str(),false);

	// advanced: to backup directory

	char temppath[_MAX_PATH];

	string sExt = PathUtil::GetExt(m_szFilename);
	string sFileWithoutExt = PathUtil::GetFileName(m_szFilename);

	{
		assert(::strstr(sFileWithoutExt,":")==0);
		assert(::strstr(sFileWithoutExt,"\\")==0);
	}
	
	PathUtil::RemoveExtension(sFileWithoutExt);

	const char *path = "LogBackups";
	char szPath[MAX_FILENAME_SIZE];
	string temp = m_pSystem->GetRootFolder();
	temp += path;
	if (temp.size() < MAX_FILENAME_SIZE)
		strcpy(szPath, temp.data());
	else
		strcpy(szPath, path);

	if(!gEnv->pCryPak)
	{
		return;
	}

	string szBackupPath = gEnv->pCryPak->AdjustFileName(szPath,temppath,ICryPak::FLAGS_FOR_WRITING|ICryPak::FLAGS_NO_MASTER_FOLDER_MAPPING);
	gEnv->pCryPak->MakeDir(szBackupPath.c_str());

	string sBackupNameAttachment;

	string sLogFilename = gEnv->pCryPak->AdjustFileName(m_szFilename,temppath,ICryPak::FLAGS_FOR_WRITING|ICryPak::FLAGS_NO_MASTER_FOLDER_MAPPING);
	FILE *in = fopen(sLogFilename.c_str(), "rb");

	// parse backup name attachment
	// e.g. BackupNameAttachment="attachment name"
	if(in)
	{
		bool bKeyFound=false;
		string sName;

		while(!feof(in))
		{
			uint8 c=fgetc(in);

			if(c=='\"')
			{
				if(!bKeyFound)
				{
					bKeyFound=true;

					if(sName!="BackupNameAttachment=")
					{
						OutputDebugString("Log::CreateBackupFile ERROR '");
						OutputDebugString(sName.c_str());
						OutputDebugString("' not recognized \n");

						assert(0);		// broken log file? - first line should include this name - written by LogVersion()
						return;
					}

					sName.clear();
				}
				else
				{
					sBackupNameAttachment=sName;
					break;
				}

				continue;
			}

			if(c>=' ')
				sName += c;
			else
				break;
		}

		fclose(in);
	}

	string bakdest = PathUtil::Make(szBackupPath,sFileWithoutExt+sBackupNameAttachment+"."+sExt);

#ifdef WIN32
	CopyFile(sLogFilename.c_str(), bakdest.c_str(), false);
#else
	in = fopen(sLogFilename.c_str(), "rb");
	if(in != NULL)
	{
		FILE* out = fopen(bakdest.c_str(), "wb");
		if(out != NULL)
		{
			char cpybuf[1024 * 32];
			size_t br = 0;
			while((br = fread(cpybuf, 1, sizeof (cpybuf), in)) > 0)
			{
				if(fwrite(cpybuf, 1, br, out) != br)
					break;
			}
			fclose(out);
		}
		fclose(in);
	}
#endif

#endif
}

//set the file used to log to disk
//////////////////////////////////////////////////////////////////////
bool CLog::SetFileName(const char *filename)
{
	RETURN;
	if (!filename) 
    return false;

#if defined(PS3)
	//for developing reason, the log file is placed on the local machine not in the hard disk mastercd folder
	strcpy(m_szFilename, filename);
#else
	string temp = PathUtil::Make(m_pSystem->GetRootFolder(), PathUtil::GetFile(filename));
	if ( temp.empty() || temp.size() >= MAX_FILENAME_SIZE )
		return false;
	strcpy(m_szFilename, temp.data());
	CreateBackupFile();
#endif	

	FILE *fp=OpenLogFile(m_szFilename,"wt");
	if (fp)
	{
		fclose(fp);
		return true;
	}

	return false;

	/*
#if !defined(PS3)	//for PS3 we do not want too much log traffic over network (file io is slow)
	// Clear error file.
	char errfile[MAX_PATH];
	strcpy( errfile,m_szFilename );
	strcat( errfile,".err" );
	fp=OpenLogFile(errfile,"wt");
	if (fp)
		fclose(fp);
#endif
	*/
}

//////////////////////////////////////////////////////////////////////////
const char* CLog::GetFileName()
{
	return m_szFilename;
}


//////////////////////////////////////////////////////////////////////
void CLog::UpdateLoadingScreen(const char *szFormat,...)
{
	THREAD_RETURN;
	{
		va_list args;
		va_start(args, szFormat);

		LogV(ILog::eMessage,szFormat,args);

		va_end(args);
	}

	((CSystem*)m_pSystem)->UpdateLoadingScreen();

#ifndef LINUX
	// Take this opportunity to update streaming engine.
	//IStreamEngine *pStreamEngine = GetISystem()->GetStreamEngine();
	//if(pStreamEngine)
		//pStreamEngine->Update();
#endif
}



//////////////////////////////////////////////////////////////////////////
int	CLog::GetVerbosityLevel()
{
	if (m_pLogVerbosity)
		return (m_pLogVerbosity->GetIVal());

	return (0);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Checks the verbosity of the message and returns NULL if the message must NOT be
// logged, or the pointer to the part of the message that should be logged
// NOTE:
//    Normally, this is either the pText pointer itself, or the pText+1, meaning
//    the first verbosity character may be cut off)
//    This is done in order to avoid modification of const char*, which may cause GPF
//    sometimes, or kill the verbosity qualifier in the text that's gonna be passed next time.
const char* CLog::CheckAgainstVerbosity(const char * pText, bool &logtofile, bool &logtoconsole, const uint8 DefaultVerbosity )
{
	RETURN NULL;
	// the max verbosity (most detailed level)
	const unsigned char nMaxVerbosity = 8;
	
	// the current verbosity of the log
	int nLogVerbosity = m_pLogVerbosity ? m_pLogVerbosity->GetIVal() : nMaxVerbosity;
	int nLogFileVerbosity = m_pLogFileVerbosity ? m_pLogFileVerbosity->GetIVal() : nMaxVerbosity;

	nLogFileVerbosity=max(nLogFileVerbosity,nLogVerbosity);		// file verbosity depends on usual log_verbosity as well

	// Empty string
	if (pText[0] == '\0')
	{
		logtoconsole = false;
		logtofile = false;
		return 0;
	}

	logtoconsole = (nLogVerbosity >= DefaultVerbosity);
	logtofile = (nLogFileVerbosity >= DefaultVerbosity);

	return pText;
}


//////////////////////////////////////////////////////////////////////////
void CLog::AddCallback( ILogCallback *pCallback )
{
	stl::push_back_unique(m_callbacks,pCallback);
}

//////////////////////////////////////////////////////////////////////////
void CLog::RemoveCallback( ILogCallback *pCallback )
{
	m_callbacks.remove(pCallback);
}
