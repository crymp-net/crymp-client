
//////////////////////////////////////////////////////////////////////
//
//	Crytek CryENGINE Source code
//	
//	File:Log.h
//
//	History:
//	-Feb 2,2001:Created by Marco Corbetta
//
//////////////////////////////////////////////////////////////////////

#ifndef LOG_H
#define LOG_H

#if _MSC_VER > 1000
# pragma once
#endif

#include "CryCommon/CrySystem/ILog.h"

//////////////////////////////////////////////////////////////////////
#define MAX_TEMP_LENGTH_SIZE	2048
#define MAX_FILENAME_SIZE			256
 



//////////////////////////////////////////////////////////////////////
class CLog :public ILog
{
public:
	typedef std::list<ILogCallback*> Callbacks;	
	typedef CryFixedStringT<MAX_TEMP_LENGTH_SIZE> LogStringType;

	// constructor
	CLog( ISystem *pSystem );
	// destructor
	~CLog();

	// interface ILog, IMiniLog -------------------------------------------------

	virtual void Release() { delete this; };
	virtual bool SetFileName(const char *filename);		
	virtual const char*	GetFileName();
	virtual void Log(const char *command,...) PRINTF_PARAMS(2, 3);
	virtual void LogWarning(const char *command,...) PRINTF_PARAMS(2, 3);
	virtual void LogError(const char *command,...) PRINTF_PARAMS(2, 3);
	virtual void LogPlus(const char *command,...) PRINTF_PARAMS(2, 3);
	virtual void LogToFile	(const char *command,...) PRINTF_PARAMS(2, 3);
  virtual void LogToFilePlus(const char *command,...) PRINTF_PARAMS(2, 3);
	virtual void LogToConsole(const char *command,...) PRINTF_PARAMS(2, 3);
	virtual void LogToConsolePlus(const char *command,...) PRINTF_PARAMS(2, 3);
	virtual void UpdateLoadingScreen(const char *command,...) PRINTF_PARAMS(2, 3);
	virtual void SetVerbosity( int verbosity );
	virtual	int	 GetVerbosityLevel();
	virtual void RegisterConsoleVariables();
	virtual void UnregisterConsoleVariables();
	virtual void AddCallback( ILogCallback *pCallback );
	virtual void RemoveCallback( ILogCallback *pCallback );
	virtual void LogV( const ELogType ineType, const char* szFormat, va_list args );

private: // -------------------------------------------------------------------

	void LogStringToFile( const char* szString,bool bAdd,bool bError=false );
	void LogStringToConsole( const char* szString,bool bAdd=false );

	FILE* OpenLogFile( const char *filename,const char *mode );

	// will format the message into m_szTemp
	void FormatMessage(const char *szCommand, ... ) PRINTF_PARAMS(2, 3);


	ISystem	*				m_pSystem;														//
	char						m_szTemp[MAX_TEMP_LENGTH_SIZE];				//
	char						m_szFilename[MAX_FILENAME_SIZE];			// can be with path
	FILE *					m_pLogFile;
	FILE *					m_pErrFile;
	int							m_nErrCount;
		
	ICVar *					m_pLogIncludeTime;										//
	
	IConsole *			m_pConsole;														//

public: // -------------------------------------------------------------------

	// checks the verbosity of the message and returns NULL if the message must NOT be
	// logged, or the pointer to the part of the message that should be logged
	const char* CheckAgainstVerbosity(const char * pText, bool &logtofile, bool &logtoconsole, const uint8 DefaultVerbosity=2 );

	// create backup of log file, useful behavior - only on development platform
	void CreateBackupFile() const;

	ICVar *					m_pLogVerbosity;											//
	ICVar *					m_pLogFileVerbosity;									//
	Callbacks				m_callbacks;													//

	uint32 m_nMainThreadId;
};


#endif

