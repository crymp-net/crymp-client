#pragma once

#include "CryCommon/CrySystem/ITestSystem.h"				// ITestSystem
#include "Log.h"								// CLog

// needed for external test application
class CTestSystem :public ITestSystem
{
public:
	// constructor
	CTestSystem();
	// destructor
	virtual ~CTestSystem();

	// interface ITestSystem -----------------------------------------------
	
	virtual void ApplicationTest( const char *szParam );
	virtual void Update();
	virtual void BeforeRender();
	virtual void AfterRender() {}
	virtual ILog *GetILog() { return m_pLog; }
	virtual void Release() { delete this; }
	virtual void SetTimeDemoInfo( STimeDemoInfo *pTimeDemoInfo );
	virtual STimeDemoInfo* GetTimeDemoInfo();
	virtual void QuitInNSeconds( const float fInNSeconds );

private: // --------------------------------------------------------------

	// TGA screenshot
	void ScreenShot( const char *szDirectory, const char *szFilename );
	//
	void LogLevelStats();
	// useful when running through a lot of tests
	void DeactivateCrashDialog();

private: // --------------------------------------------------------------

	string						m_sParameter;							// "" if not in test mode
	CLog *						m_pLog;										//
	int								m_iRenderPause;						// counts down every render to delay some processing
	float							m_fQuitInNSeconds;				// <=0 means it's deactivated
				
	int								m_bFirstUpdate : 1;
	int								m_bApplicationTest : 1;	

	STimeDemoInfo*		m_pTimeDemoInfo;

	friend class CLevelListener;
};
