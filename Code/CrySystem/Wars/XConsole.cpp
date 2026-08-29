// XConsole.cpp: implementation of the CXConsole class.
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "XConsole.h"
#include "XConsoleVariable.h"
#include "System.h"
#include "ConsoleBatchFile.h"

#include "CryCommon/CryInput/IInput.h"
#include "CryCommon/CrySystem/ITimer.h"
#include "CryCommon/CryScriptSystem/IScriptSystem.h"
#include "CryCommon/CryInput/IInput.h"
#include "CryCommon/CryRenderer/IRenderer.h"
#include "CryCommon/CryNetwork/INetwork.h"     // EvenBalance - M.Quinn
#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CrySystem/ILog.h"
#include "CryCommon/CrySystem/IProcess.h"
#include "CryCommon/CrySystem/IHardwareMouse.h"

// EvenBalance - M. Quinn
#if !defined(CRYMP_EMBEDDED_CRYSYSTEM) && !defined(PS3) && !defined(XENON)
	#include <PunkBuster/pbcommon.h>
#endif

//#define CRYSIS_BETA

#ifdef CRYSIS_BETA
static inline bool AllowedForBeta(const string& cmd)
{
	if (gEnv->pSystem->IsDedicated())
		return true;
	static const char* commands[] = {"name", "connect", "quit"}; // a list of allowed beta commands; hacky ... but for beta test only
	static const size_t ncommands = sizeof(commands) / sizeof(char*);
	string temp = cmd; temp.MakeLower();
	for (size_t i = 0; i < ncommands; ++i) // slow ... but for beta test only
		if (temp == commands[i])
			return true;
	return false;
}
#endif

static inline void AssertName( const char *szName )
{
#ifdef _DEBUG
	assert(szName);

	// test for good console variable / command name
	const char *p = szName;
	bool bFirstChar=true;

	while(*p)
	{
		assert((*p>='a' && *p<='z') 
				|| (*p>='A' && *p<='Z') 
				|| (*p>='0' && *p<='9' && !bFirstChar) 
				|| *p=='_');

		++p;bFirstChar=false;
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
// user defined comparison - for nicer printout
inline int GetCharPrio( char x ) 
{
	if(x>='a' && x<='z')
		x+='A'-'a';					// make upper case

	if(x=='_')return 300;
		else return x;
}
// case sensitive
inline bool less_CVar( const char* left,const char* right )
{
	for(;;)
	{
		uint32 l=GetCharPrio(*left), r=GetCharPrio(*right);

		if(l<r)
			return true;
		if(l>r)
			return false;

		if(*left==0 || *right==0)
			break;
	
		++left;++right;
	}

	return false;
}

inline bool less_CVar(const string& left, const string& right)
{
	return less_CVar(left.c_str(), right.c_str());
}

#ifdef _XBOX

#include <Xbdm.h>

//////////////////////////////////////////////////////////////////////
// Xenon debug command input.
//////////////////////////////////////////////////////////////////////

class CXDebugInput
{
public:
	CXDebugInput();
	string GetInput();
};


class CXenonOutputPrintSink : public IOutputPrintSink
{
public:
	void Print( const char *inszText );
};


class CXenonVarDumpSink : public ICVarDumpSink
{
public:
	CXenonVarDumpSink();
	void OnElementFound(ICVar *pCVar);
	void SetBufferPointer(char * pBuf);
	void SetCurCommand(const char * pCommand);
private:
	char * m_pCh;
	char * m_pBegin;
	char m_CurCommand[128];
	int m_State;
};

CXenonVarDumpSink::CXenonVarDumpSink()
{
	m_pCh = 0;
	m_pBegin = 0;
	m_State = 1;
}

void CXenonVarDumpSink::OnElementFound(ICVar *pCVar)
{
	if(!m_pCh || !m_pBegin)
		return;

	if(m_pCh - m_pBegin > 200)
		return;

	if(m_State==1 && m_pCh == m_pBegin)
	{
		if(strlen(m_CurCommand) > 0)
		{
			if(strcmp(m_CurCommand, pCVar->GetName()))
				return;
			m_State = 0;
		}
	}

	if(m_State)
	{
		strcpy(m_pCh, pCVar->GetName());
		m_pCh += strlen(pCVar->GetName());
		*m_pCh++ = ';';
		*m_pCh = 0;
	}

	m_State = 2;
}

void CXenonVarDumpSink::SetBufferPointer(char * pBuf)
{
	m_pCh = pBuf;
	m_pBegin = pBuf;
}

void CXenonVarDumpSink::SetCurCommand(const char * pCommand)
{
	strncpy(m_CurCommand, pCommand, 127);
}



	// Global buffer to receive remote commands from the debug console. Note that
	// since this data is accessed by the app's main thread, and the debug monitor
	// thread, we need to protect access with a critical section
	static char sInputBuf[1024];

	// The critical section used to protect data that is shared between threads
	static CRITICAL_SECTION CriticalSection;


	static HRESULT __stdcall XDebugInputProcessor(	const char* strCommand,
																									char* strResponse, DWORD ResponseLen,
																									DM_CMDCONT* pdmcc )
	{
		OutputDebugString("Command received.\n");

		// Skip over dumb prefix and ! mark.
		while (*strCommand && *strCommand != '!')
			strCommand++;
		if (!*strCommand)
			return XBDM_NOERR;
		strCommand++;

		// Check if this is the initial connect signal
		if (strnicmp( strCommand, "__connect__", 11 ) == 0)
		{
			// If so, respond that we're connected
			lstrcpynA( strResponse, "Connected.", ResponseLen );
			return XBDM_NOERR;
		}

		// Get variable names
		if (strnicmp( strCommand, "__getvars__", 11 ) == 0)
		{
			// If so, collect variable names
			CXenonVarDumpSink s_XenonVarDumpSink;
			s_XenonVarDumpSink.SetBufferPointer(strResponse);
			s_XenonVarDumpSink.SetCurCommand(&strCommand[11]);
			gEnv->pConsole->DumpCVars(&s_XenonVarDumpSink);
			s_XenonVarDumpSink.SetBufferPointer(0);

			return XBDM_NOERR;
		}



		/*
		// Check if this is the binary receive command
		// Note: The binary receive command can be any string - you just have to agree on 
		// some distinct string between your dev kit app and your debug console app running 
		// on the PC.  Here, we use "__recvbinary__".
		if( dbgstrnicmp( strCommand, "__recvbinary__", 14 ) == 0 )
		{
			// The binary receive signal must be followed by a data size.
			// Read the data size and make sure we have enough buffer space here to hold
			// the data.
			DWORD dwDataSize = atoi( strCommand + 14 );
			if( dwDataSize > g_dwReceiveBufferCapacity )
					return XBDM_INVALIDARG;
			// Set data size to 0, since we're going to overwrite the contents of the buffer.
			g_dwReceiveBufferDataSize = 0;
			// Set in-progress flag
			g_bBinaryTransferInProgress = TRUE;

			// Respond using the PDM_CMDCONT command continuation struct.
			// This result tells the app running on the PC that it is OK to send binary data now.
			// Note that we are pointing the structure directly at our binary receive buffer.
			dbgstrcpy( strResponse, "Ready for binary." );
			pdmcc->HandlingFunction = DebugConsoleBinaryReceiveHandler;
			pdmcc->DataSize = 0;
			pdmcc->Buffer = g_pBinaryReceiveBuffer;
			pdmcc->BufferSize = g_dwReceiveBufferCapacity;
			pdmcc->BytesRemaining = dwDataSize;
			pdmcc->CustomData = NULL;
			// XBDM_READYFORBIN allows the debug console on the PC to call DmSendBinary().
			return XBDM_READYFORBIN;
		}

		// Check if this is the binary send command
		// Note: As with the binary receive command, the binary send command can be any 
		// string, as long as your app and your debug console on the PC agree on the same 
		// string.  You'll probably have many commands that trigger binary send, like 
		// "send backbuffer" or "dump AI state", etc.
		if( dbgstrnicmp( strCommand, "__sendbinary__", 14 ) == 0 )
		{
			// Fill buffer with a letter of the alphabet (will be displayed on the debug console)
			static BYTE CharIndex = 0;
			memset( g_pBinarySendBuffer + sizeof( DWORD ), (BYTE)'A' + CharIndex, g_dwSendBufferCapacity );
			CharIndex = ( CharIndex + 1 ) % 26;

			// Set the first 4 bytes to the size of the buffer minus 4 bytes.
			*(DWORD*)g_pBinarySendBuffer = ( g_dwSendBufferCapacity - sizeof( DWORD ) );

			// Prepare the continuation structure to send the buffer we just created
			pdmcc->HandlingFunction = DebugConsoleBinarySendHandler;
			pdmcc->Buffer = g_pBinarySendBuffer;
			pdmcc->BufferSize = g_dwSendBufferCapacity;
			pdmcc->BytesRemaining = g_dwSendBufferCapacity;
			pdmcc->CustomData = NULL;
			g_bBinaryTransferInProgress = TRUE;

			return XBDM_BINRESPONSE;
		}
		*/

		// sInputBuf needs to be protected by the critical section
		EnterCriticalSection( &CriticalSection );
		if( sInputBuf[0] )
		{
			// This means the application has probably stopped polling for debug commands
			strcpy( strResponse, "Cannot execute - previous command still pending" );
		}
		else
		{
			strcpy( sInputBuf, strCommand );
		}
		LeaveCriticalSection( &CriticalSection );

		return XBDM_NOERR;
	}

	CXDebugInput::CXDebugInput()
	{
		// Singleton enforcement.
		static bool bInstance = false;
		assert(!bInstance);
		bInstance = true;

		// Register our command handler with the debug monitor
		// The prefix is what uniquely identifies our command handler. Any
		// commands that are sent to the console with that prefix and a '!'
		// character (i.e.; XCMD!data data data) will be sent to
		// our callback.
		static char const* s_DebugCmdPrefix = "XCON";
		HRESULT hr = DmRegisterCommandProcessor( s_DebugCmdPrefix, XDebugInputProcessor );
		if( FAILED(hr) )
				return;

		// We'll also need a critical section to protect access to sInputBuf
		InitializeCriticalSection( &CriticalSection );

		/*
		g_pBinaryReceiveBuffer = new BYTE[ g_dwReceiveBufferCapacity ];
		g_pBinarySendBuffer = new BYTE[ g_dwSendBufferCapacity ];
		*/
	}

	string CXDebugInput::GetInput()
	{
    // If there's nothing waiting, return.
    if( !sInputBuf[0] )
        return "";

    // Grab a local copy of the command received in the remote buffer
    EnterCriticalSection( &CriticalSection );

		string Result = sInputBuf;
    sInputBuf[0] = 0;

    LeaveCriticalSection( &CriticalSection );

		return Result;
	}

//////////////////////////////////////////////////////////////////////
// CXenonOutputPrintSink implementation
void CXenonOutputPrintSink::Print( const char *inszText )
{
	char strBuffer[1024];
	strcpy(strBuffer, "FXEN!");
	strncat(strBuffer, inszText, 1000);
//  assert(0);
	DmSendNotificationString( strBuffer );
}


static CXDebugInput s_XsDebugInput; 
static CXenonOutputPrintSink s_XenonOutputPrintSink;

#endif

void ConsoleShow( IConsoleCmdArgs* )
{
  gEnv->pConsole->ShowConsole(true);
}
void ConsoleHide( IConsoleCmdArgs* )
{
  gEnv->pConsole->ShowConsole(false);
}

void Bind( IConsoleCmdArgs* cmdArgs)
{
	int count = cmdArgs->GetArgCount();

	if (cmdArgs->GetArgCount() >= 3)
	{
		string arg;
		for (int i = 2; i < cmdArgs->GetArgCount(); ++i)
		{
			arg += cmdArgs->GetArg(i);
			arg += " ";
		}
		gEnv->pConsole->CreateKeyBind(cmdArgs->GetArg(1), arg.c_str());
	}
}

//////////////////////////////////////////////////////////////////////////
int CXConsole::con_display_last_messages = 0;
int CXConsole::con_line_buffer_size = 1000;
int CXConsole::con_showonload = 0;
int CXConsole::con_debug = 0;
int CXConsole::con_restricted = 1;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CXConsole::CXConsole()
{
	m_pSysDeactivateConsole=0;
	m_pFont=NULL;
	m_pRenderer=NULL;
	m_pNetwork=NULL; // EvenBalance - M. Quinn
	m_pInput=NULL;
	m_pImage=NULL;
	m_nCursorPos=0;
	m_nScrollPos=0;
	m_nScrollMax=300;
	m_nTempScrollMax=m_nScrollMax;
	m_nScrollLine=0;
	m_nHistoryPos=-1;
	m_bRepeat=false;
	m_nTabCount=0;
	m_bConsoleActive=false;
	m_bActivationKeyEnable=true;
	m_sdScrollDir = sdNONE;
	m_pSystem=NULL;
	m_bDrawCursor = true;

	m_bStaticBackground=false;
	m_nProgress = 0;
	m_nProgressRange = 0;
//	m_nLoadingBarTexID = 0;
	m_nLoadingBackTexID = 0;
	m_nWhiteTexID = 0;
}


//////////////////////////////////////////////////////////////////////////
CXConsole::~CXConsole()
{
	if(!m_mapVariables.empty())
	{
		while (!m_mapVariables.empty())
			m_mapVariables.begin()->second->Release();

		m_mapVariables.clear();
	}
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::FreeRenderResources()
{
  if (m_pRenderer)
  {
/*    if (m_nLoadingBarTexID)
		{
      m_pRenderer->RemoveTexture(m_nLoadingBarTexID);
			m_nLoadingBarTexID = -1;
		}
*/
		if (m_nLoadingBackTexID)
		{
      m_pRenderer->RemoveTexture(m_nLoadingBackTexID);
			m_nLoadingBackTexID = -1;
		}
		if (m_nWhiteTexID)
		{
      m_pRenderer->RemoveTexture(m_nWhiteTexID);
			m_nWhiteTexID = -1;
		}
    if (m_pImage)
      m_pImage->Release();
  }
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::Release()
{
	delete this;
}





static void Command_DumpCommandsVars(IConsoleCmdArgs* Cmd)
{
	const char *arg = "";

	if(Cmd->GetArgCount()>1) arg = Cmd->GetArg(1);

	((CXConsole *)(gEnv->pConsole))->DumpCommandsVars(arg);
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::Init(CSystem *pSystem)
{
	m_pSystem=pSystem;
	if (pSystem->GetICryFont())
		m_pFont=pSystem->GetICryFont()->GetFont("console");
	m_pRenderer=pSystem->GetIRenderer();
	m_pNetwork=pSystem->GetINetwork();  // EvenBalance - M. Quinn
	m_pInput=pSystem->GetIInput();   
	m_pTimer=pSystem->GetITimer();
	
	if (m_pInput)
	{
		// Assign this class as input listener.
		m_pInput->AddConsoleEventListener( this );
	}
	
	m_pSysDeactivateConsole = RegisterInt("sys_DeactivateConsole",0,0,
		"0: normal console behavior\n"
		"1: hide the console");

	Register("con_display_last_messages",&con_display_last_messages,0);  // keep default at 1, needed for gameplay
	Register("con_line_buffer_size",&con_line_buffer_size,1000,0);
	Register("con_showonload",&con_showonload,0,0,"Show console on level loading");
	Register("con_debug",&con_debug,0,VF_CHEAT,"Log call stack on every GetCVar call");
	Register("con_restricted",&con_restricted,con_restricted,VF_RESTRICTEDMODE,"0=normal mode / 1=restricted access to the console");				// later on VF_RESTRICTEDMODE should be removed (to 0)

	if(m_pSystem->IsDevMode()					// unrestricted console for -DEVMODE
	|| m_pSystem->IsDedicated())			// unrestricted console for dedicated server
		con_restricted=0;

	// test cases -----------------------------------------------
#ifdef CRYSIS_BETA
	// no test for BETA test
#else
	assert(GetCVar("con_debug")!=0);		// should be registered a few lines above
	assert(GetCVar("Con_Debug")==GetCVar("con_debug"));		// different case

	// editor
	assert(strcmp(AutoComplete("con_"),"con_debug")==0);
	assert(strcmp(AutoComplete("CON_"),"con_debug")==0);
	assert(strcmp(AutoComplete("con_debug"),"con_display_last_messages")==0);		// actually we should reconsider this behavior
	assert(strcmp(AutoComplete("Con_Debug"),"con_display_last_messages")==0);		// actually we should reconsider this behavior

	// game
	assert(strcmp(ProcessCompletion("con_"),"con_debug ")==0);
	ResetAutoCompletion();
	assert(strcmp(ProcessCompletion("CON_"),"con_debug ")==0);
	ResetAutoCompletion();
	assert(strcmp(ProcessCompletion("con_debug"),"con_debug ")==0);
	ResetAutoCompletion();
	assert(strcmp(ProcessCompletion("Con_Debug"),"con_debug ")==0);
	ResetAutoCompletion();
#endif

	// ----------------------------------------------------------

//	m_nLoadingBarTexID = -1;
	if (m_pRenderer)
	{
		m_nWhiteTexID = -1;

		ITexture *pTex = 0;		
/*	
		pTex = pSystem->GetIRenderer()->EF_LoadTexture("Textures/console/loadingbar.dds", FT_DONT_RESIZE | FT_NOMIPS, eTT_2D);
		if (pTex)
      m_nLoadingBarTexID = pTex->GetTextureID();
*/
		// This texture is already loaded by the renderer. It's ref counted so there is no wasted space.
		pTex = pSystem->GetIRenderer()->EF_LoadTexture("Textures/Defaults/White.dds", FT_DONT_ANISO | FT_DONT_RELEASE, eTF_Unknown);
		if (pTex)
      m_nWhiteTexID = pTex->GetTextureID();
	}
	else
	{
		m_nLoadingBackTexID = -1;
		m_nWhiteTexID = -1;
	} 
	if(pSystem && pSystem->IsDedicated())
			m_bConsoleActive = true;

#ifdef _XBOX
	pSystem->GetIConsole()->AddOutputPrintSink(&s_XenonOutputPrintSink);
#endif

  AddCommand( "ConsoleShow",&ConsoleShow );
  AddCommand( "ConsoleHide",&ConsoleHide );
	AddCommand( "DumpCommandsVars",&Command_DumpCommandsVars,0,
		"This console command dumps all console variables and commands to disk\n"
		"DumpCommandsVars [prefix]" );
	AddCommand( "Bind", &Bind);

	CConsoleBatchFile::Init();
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::RegisterVar( ICVar *pCVar,ConsoleVarFunc pChangeFunc )
{
	// first callback so setting the value from m_configVars
	// is calling pChangeFunc         (that would be more correct but to not introduce new problems this code was not changed)
//	if (pChangeFunc)
//		pCVar->SetOnChangeCallback(pChangeFunc);

	ConfigVars::iterator it = m_configVars.find( CONST_TEMP_STRING(pCVar->GetName()) );
	if (it != m_configVars.end())
	{
		pCVar->Set(it->second.c_str());
		pCVar->SetFlags( pCVar->GetFlags()|VF_WASINCONFIG );
		// Remove value.
		m_configVars.erase(it);
	}
	else
	{
		// Variable is not modified when just registered.
		pCVar->ClearFlags(VF_MODIFIED);
	}

	if (pChangeFunc)
		pCVar->SetOnChangeCallback(pChangeFunc);

	m_mapVariables.insert(ConsoleVariablesMapItor::value_type(pCVar->GetName(),pCVar));
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::LoadConfigVar( const char *sVariable,const char *sValue )
{
	ICVar *pCVar = GetCVar(sVariable);
	if (pCVar)
	{
		pCVar->Set( sValue );
		pCVar->SetFlags( pCVar->GetFlags()|VF_WASINCONFIG );
		return;
	}

	m_configVars[sVariable] = sValue;
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::EnableActivationKey(bool bEnable)
{
	m_bActivationKeyEnable = bEnable;
}

//////////////////////////////////////////////////////////////////////////
ICVar *CXConsole::Register(const char *sName, int *src, int iValue, int nFlags, const char *help,ConsoleVarFunc pChangeFunc )
{
	AssertName(sName);

	ICVar *pCVar = stl::find_in_map( m_mapVariables,sName,NULL );
	if (pCVar)
		return pCVar;

	pCVar=new CXConsoleVariableIntRef(this,sName,src,nFlags,help);
	*src = iValue;
	RegisterVar( pCVar,pChangeFunc );
	return pCVar;
}


//////////////////////////////////////////////////////////////////////////
ICVar *CXConsole::RegisterCVarGroup( const char *szName, const char *szFileName )
{
	AssertName(szName);
	assert(szFileName);

	// suppress cvars not starting with sys_spec_ as
	// cheaters might create cvars before we created ours
	if(strnicmp(szName,"sys_spec_",9)!=0)
		return 0;

	ICVar *pCVar = stl::find_in_map( m_mapVariables,szName,NULL );
	if (pCVar)
	{
		assert(0);			// groups should be only registered once
		return pCVar;
	}

	CXConsoleVariableCVarGroup *pCVarGroup=new CXConsoleVariableCVarGroup(this,szName,szFileName,VF_COPYNAME);

	pCVar = pCVarGroup;

	RegisterVar( pCVar,CXConsoleVariableCVarGroup::OnCVarChangeFunc);
/*
	// log to file - useful during development - might not be required for final shipping
	{
		string sInfo = pCVarGroup->GetDetailedInfo();

		gEnv->pLog->LogToFile("CVarGroup %s",sInfo.c_str());
		gEnv->pLog->LogToFile(" ");
	}
*/
	return pCVar;
}

//////////////////////////////////////////////////////////////////////////
ICVar *CXConsole::Register(const char *sName, float *src, float fValue, int nFlags, const char *help,ConsoleVarFunc pChangeFunc )
{
	AssertName(sName);

	ICVar *pCVar = stl::find_in_map( m_mapVariables,sName,NULL );
	if (pCVar)
		return pCVar;

	pCVar = new CXConsoleVariableFloatRef(this,sName,src,nFlags,help);
	*src = fValue;
	RegisterVar( pCVar,pChangeFunc );
	return pCVar;
}


//////////////////////////////////////////////////////////////////////////
ICVar *CXConsole::RegisterString(const char *sName,const char *sValue,int nFlags, const char *help,ConsoleVarFunc pChangeFunc )
{
	AssertName(sName);

	ICVar *pCVar = stl::find_in_map( m_mapVariables,sName,NULL );
	if (pCVar)
		return pCVar;

	pCVar = new CXConsoleVariableString(this,sName,sValue,nFlags,help);
	RegisterVar( pCVar,pChangeFunc );
	return pCVar;
}

//////////////////////////////////////////////////////////////////////////
ICVar *CXConsole::RegisterFloat(const char *sName,float fValue,int nFlags, const char *help,ConsoleVarFunc pChangeFunc )
{
	AssertName(sName);

	ICVar *pCVar = stl::find_in_map( m_mapVariables,sName,NULL );
	if (pCVar)
		return pCVar;

	pCVar=new CXConsoleVariableFloat(this,sName,fValue,nFlags,help);
	RegisterVar( pCVar,pChangeFunc );
	return pCVar;
}

//////////////////////////////////////////////////////////////////////////
ICVar *CXConsole::RegisterInt(const char *sName,int iValue,int nFlags, const char *help,ConsoleVarFunc pChangeFunc )
{
	AssertName(sName);

	ICVar *pCVar = stl::find_in_map( m_mapVariables,sName,NULL );
	if (pCVar)
		return pCVar;
	
	pCVar=new CXConsoleVariableInt(this,sName,iValue,nFlags,help);
	RegisterVar( pCVar,pChangeFunc );
	return pCVar;
}



//////////////////////////////////////////////////////////////////////////
void CXConsole::UnregisterVariable(const char *sVarName,bool bDelete)
{
	ConsoleVariablesMapItor itor;
	itor=m_mapVariables.find(sVarName);

	if(itor==m_mapVariables.end())
		return;

	ICVar *pCVar=itor->second;

	m_mapVariables.erase(sVarName);

	if(bDelete)
		pCVar->Release();
}


//////////////////////////////////////////////////////////////////////////
void CXConsole::SetScrollMax(int value)
{
	m_nScrollMax=value;
	m_nTempScrollMax=m_nScrollMax;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CXConsole::SetImage(ITexture *pImage,bool bDeleteCurrent)
{
	if (bDeleteCurrent)	
	{
    pImage->Release();
	}
		
	m_pImage=pImage;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void	CXConsole::ShowConsole(bool show, const int iRequestScrollMax )
{
	if(m_pSysDeactivateConsole->GetIVal())
		show=false;

	if(show && !m_bConsoleActive)
	{
		if(gEnv->pHardwareMouse)
		{
			gEnv->pHardwareMouse->IncrementCounter();
		}
	}
	else if(!show && m_bConsoleActive)
	{
		if(gEnv->pHardwareMouse)
		{
			gEnv->pHardwareMouse->DecrementCounter();	
		}
	}

	SetStatus(show);

	if(iRequestScrollMax>0)
		m_nTempScrollMax=iRequestScrollMax;			// temporary user request
	 else
		m_nTempScrollMax=m_nScrollMax;					// reset

	if(m_bConsoleActive) 
    m_sdScrollDir=sdDOWN;		
	 else 
    m_sdScrollDir=sdUP;	
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::CreateKeyBind(const char *sCmd,const char *sRes)
{
	m_mapBinds.insert(ConsoleBindsMapItor::value_type(sCmd, sRes));
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::DumpKeyBinds( IKeyBindDumpSink *pCallback )
{
	for (ConsoleBindsMap::iterator it = m_mapBinds.begin(); it != m_mapBinds.end(); ++it)
	{
		pCallback->OnKeyBindFound( it->first.c_str(),it->second.c_str() );
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* CXConsole::FindKeyBind( const char *sCmd ) const
{
	ConsoleBindsMap::const_iterator it = m_mapBinds.find(CONST_TEMP_STRING(sCmd));

	if (it != m_mapBinds.end())
		return it->second.c_str();

	return 0;
}


//////////////////////////////////////////////////////////////////////////
void CXConsole::DumpCVars(ICVarDumpSink *pCallback,unsigned int nFlagsFilter)
{
	ConsoleVariablesMapItor It=m_mapVariables.begin();

	while (It!=m_mapVariables.end())
	{
		if((nFlagsFilter==0) || ((nFlagsFilter!=0) && (It->second->GetFlags()&nFlagsFilter) ))
			pCallback->OnElementFound(It->second);
		++It;
	}
}

//////////////////////////////////////////////////////////////////////////
ICVar *CXConsole::GetCVar( const char *sName )
{
	assert(this);
	assert(sName);

	if (con_debug)
	{
		// Log call stack on get cvar.
		CryLog( "GetCVar(\"%s\") called",sName );
		m_pSystem->debug_LogCallStack();
	}

	// Fast map lookup for case-sensitive match.
	ConsoleVariablesMapItor it;
	
	it=m_mapVariables.find(sName);
	if(it!=m_mapVariables.end())
		return it->second;

/*
	if(!bCaseSensitive)
	{
		// Much slower but allows names with wrong case (use only where performance doesn't matter).
		for(it=m_mapVariables.begin(); it!=m_mapVariables.end(); ++it)
		{
			if(stricmp(it->first,sName)==0)
				return it->second;
		}
	}
	test else
	{
		for(it=m_mapVariables.begin(); it!=m_mapVariables.end(); ++it)
		{
			if(stricmp(it->first,sName)==0)
			{
				CryError("Error: Wrong case for '%s','%s'",it->first,sName);
			}
		}
	}
*/

	return NULL;		// haven't found this name
}

//////////////////////////////////////////////////////////////////////////
char *CXConsole::GetVariable( const char * szVarName, const char * szFileName, const char * def_val )
{
	assert( m_pSystem );
	return 0;
}

//////////////////////////////////////////////////////////////////////////
float CXConsole::GetVariable( const char * szVarName, const char * szFileName, float def_val )
{
	assert( m_pSystem );
	return 0;
}


//////////////////////////////////////////////////////////////////////////
bool CXConsole::GetStatus()
{
	return m_bConsoleActive;
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::Clear()
{
	m_dqConsoleBuffer.clear();
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::Update()
{
	// Repeat GetIRenderer (For Editor).
	if (!m_pSystem)
		return;

	m_pRenderer = m_pSystem->GetIRenderer();

	// Process Key press repeat.
	if (m_bConsoleActive)
	{
		if (m_nRepeatEvent.keyId != eKI_Unknown)
		{
			float fRepeatSpeed = 40.0;

			static ICVar *pRepeatSpeed = GetCVar("ui_RepeatSpeed");
			if(pRepeatSpeed)
				fRepeatSpeed = (float)pRepeatSpeed->GetIVal();

			float fTime = m_pSystem->GetITimer()->GetAsyncCurTime() * 1000.0f;
			float fNextTimer = (1000.0f / fRepeatSpeed); // repeat speed

			if (fTime - m_nRepeatTimer > fNextTimer)
			{
				ProcessInput(m_nRepeatEvent);
				m_nRepeatTimer = fTime + fNextTimer;
			}
		}
	}

#ifdef _XBOX
	string externalInput = s_XsDebugInput.GetInput();
	if (!externalInput.empty())
	{
		ExecuteStringInternal(externalInput,true);			// external input treated as console input
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
bool CXConsole::OnInputEvent( const SInputEvent &event )
{
	// Process input event.
	ConsoleBindsMapItor itorBind;

	if (event.state == eIS_Released && m_bConsoleActive)
	{
		m_nRepeatEvent.keyId = eKI_Unknown;
	}

	if (event.state != eIS_Pressed)
		return false;

	m_nRepeatEvent = event;

	float fRepeatDelay = 200.0f;
	
	static ICVar *pRepeatDelay = GetCVar("ui_RepeatDelay");
	if(pRepeatDelay)
		fRepeatDelay = (float)pRepeatDelay->GetIVal();

	m_nRepeatTimer = m_pTimer->GetAsyncCurTime() * 1000.0f + fRepeatDelay;

	//execute Binds
	if(!m_bConsoleActive)
	{
		const char* cmd=0;

		if(event.modifiers==0)
		{
			// fast
			cmd = FindKeyBind(event.keyName);
		}
		else
		{
			// slower
			char szCombinedName[40];
			int iLen=0;

			if(event.modifiers&eMM_Ctrl)
				{ strcpy(szCombinedName,				"ctrl ");	iLen+=5; }
			if(event.modifiers&eMM_Shift)
				{ strcpy(&szCombinedName[iLen],	"shift ");iLen+=6; }
			if(event.modifiers&eMM_Alt)
				{ strcpy(&szCombinedName[iLen],	"alt ");	iLen+=4; }
			if(event.modifiers&eMM_Win)
				{ strcpy(&szCombinedName[iLen],	"win ");	iLen+=4; }

			strcpy(&szCombinedName[iLen],event.keyName);

			cmd = FindKeyBind(szCombinedName);
		}

		if(cmd)
		{
			SetInputLine("");
			ExecuteStringInternal(cmd,false);		// keybinds should not suffer from restricted mod (e.g. screenshot)
		}
	}
	else
	{
		if (event.keyId != eKI_Tab)
			ResetAutoCompletion();

		if (event.keyId == eKI_V && (event.modifiers & eMM_Ctrl) != 0) 
		{
			Paste();
			return false;
		}

		if (event.keyId == eKI_C && (event.modifiers & eMM_Ctrl) != 0) 
		{
			Copy();
			return false;
		}
	}

	if (event.keyId == eKI_Tilde)
	{
		if(m_bActivationKeyEnable)
		{
			ShowConsole(!GetStatus());
			m_sInputBuffer="";
			m_nCursorPos=0;
			m_pInput->ClearKeyState();
			return true;
		}
	}
	if (event.keyId == eKI_Escape)
	{
		//switch process or page or other things
		m_sInputBuffer="";
		m_nCursorPos=0;
		if (m_pSystem)
		{
			if (!m_bConsoleActive)
			{
				//m_pSystem->GetIGame()->SendMessage("Switch");
			}
			ShowConsole(false);

			ISystemUserCallback *pCallback = ((CSystem*)m_pSystem)->GetUserCallback();
			if (pCallback)
				pCallback->OnProcessSwitch();
		}
		return false;
	}

	return ProcessInput(event);
}

//////////////////////////////////////////////////////////////////////////
bool CXConsole::OnInputEventUI( const SInputEvent &event )
{
	if (!m_bConsoleActive)
		return false;

	AddInputChar( event.keyName[0] );

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CXConsole::ProcessInput(const SInputEvent& event)
{
	if (!m_bConsoleActive)
		return false;

	// this is not so super-nice as the XKEY's ... but a small price to pay
	// if speed is a problem (which would be laughable for this) the CCryName
	// can be cached in a static var
	if (event.keyId == eKI_Enter || event.keyId == eKI_NP_Enter)
	{
		ExecuteInputBuffer();
		m_nScrollLine=0; 
		return true;
	}
	else if (event.keyId == eKI_Backspace)
	{
		RemoveInputChar(true);
		return true;
	}
	else if (event.keyId == eKI_Left)
	{
		if(m_nCursorPos)
			m_nCursorPos--;
		return true;
	}
	else if (event.keyId == eKI_Right)
	{
		if(m_nCursorPos<(int)(m_sInputBuffer.length()))
			m_nCursorPos++;
		return true;
	}
	else if (event.keyId == eKI_Up)
	{
		const char *szHistoryLine=GetHistoryElement(true);		// true=UP

		if(szHistoryLine)
		{
			m_sInputBuffer=szHistoryLine;
			m_nCursorPos=(int)m_sInputBuffer.size();
		}
		return true;
	}
	else if (event.keyId == eKI_Down)
	{
		const char *szHistoryLine=GetHistoryElement(false);		// false=DOWN

		if(szHistoryLine)
		{
			m_sInputBuffer=szHistoryLine;
			m_nCursorPos=(int)m_sInputBuffer.size();
		}
		return true;
	}
	else if (event.keyId == eKI_Tab)
	{
		if (!(event.modifiers & eMM_Alt))
		{
			m_sInputBuffer = ProcessCompletion(m_sInputBuffer.c_str());
			m_nCursorPos = m_sInputBuffer.size();
		}
		return true;
	}
	else if (event.keyId == eKI_PgUp)
	{
		if(m_nScrollLine<(int)(m_dqConsoleBuffer.size()-2))
			m_nScrollLine++;
		return true;
	}
	else if (event.keyId == eKI_PgDn)
	{
		if(m_nScrollLine)
			m_nScrollLine--;
		return true;
	}
	else if (event.keyId == eKI_Home)
	{
		m_nCursorPos=0;
		return true;
	}
	else if (event.keyId == eKI_End)
	{
		m_nCursorPos=(int)m_sInputBuffer.length();
		return true;
	}
	else if (event.keyId == eKI_Delete)
	{
		RemoveInputChar(false);
		return true;
	}
	else 
	{
	#if !defined(PS3)
		if (gEnv->pSystem->IsEditor())
	#endif
		{
			const char*sKeyName = m_pInput->GetKeyName(event, 1);

			if (sKeyName)
			{
				if(strlen(sKeyName)!=1)
					return true;
				AddInputChar( sKeyName[0] );
				return true;
			}
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* CXConsole::GetHistoryElement( const bool bUpOrDown )
{
	static string sReturnString;			// is that save enough?

	if(bUpOrDown)
	{
		if(!m_dqHistory.empty())
		{
			if(m_nHistoryPos<(int)(m_dqHistory.size()-1))
			{
				m_nHistoryPos++;
				sReturnString=m_dqHistory[m_nHistoryPos];
				return sReturnString.c_str();
			}
		}
	}
	else
	{		
		if(m_nHistoryPos>0)
		{
			m_nHistoryPos--;
			sReturnString=m_dqHistory[m_nHistoryPos];
			return sReturnString.c_str();
		}
	}

	return 0;
}



//////////////////////////////////////////////////////////////////////////
void CXConsole::Draw()
{
  //ShowConsole(true);
	if(!m_pSystem || !m_nTempScrollMax)
		return;

	if(!m_pRenderer)
	{
		// For Editor.
		m_pRenderer = m_pSystem->GetIRenderer();
	}

	if(!m_pRenderer)
		return;

	if (!m_pFont)
	{
		// For Editor.
		ICryFont *pICryFont=m_pSystem->GetICryFont();

		if(pICryFont)
			m_pFont= m_pSystem->GetICryFont()->GetFont("default");
	}

	ScrollConsole();

	if (m_nScrollPos<=0)
	{
//#ifdef _DEBUG
//		DrawBuffer(70, "buttonfocus");
//#endif
		return;
	} 

	float fCurrTime = m_pTimer->GetFrameStartTime(ITimer::ETIMER_UI).GetSeconds();

	const double fBlinkTime=CURSOR_TIME*2.0;
	m_bDrawCursor = fmod((double)fCurrTime,fBlinkTime)<0.5;
	 
	if (!m_nProgressRange)
	{
		if (m_bStaticBackground)
		{
      m_pRenderer->SetState(GS_NODEPTHTEST);
			m_pRenderer->Draw2dImage(0,0,800,600,m_pImage?m_pImage->GetTextureID():m_nWhiteTexID, 0.0f, 1.0f, 1.0f, 0.0f);
		}
		else
		{
			m_pRenderer->Set2DMode(true,m_pRenderer->GetWidth(),m_pRenderer->GetHeight());

			float fReferenceSize = 600.0f;

			float fSizeX = m_pRenderer->GetWidth();
			float fSizeY = m_nTempScrollMax * m_pRenderer->GetHeight() / fReferenceSize;

			m_pRenderer->SetState(GS_NODEPTHTEST|GS_BLSRC_SRCALPHA|GS_BLDST_ONEMINUSSRCALPHA);
			m_pRenderer->DrawImage(0,0,fSizeX,fSizeY,m_nWhiteTexID,0.0f,0.0f,1.0f,1.0f,0.0f,0.0f,0.0f,0.7f);
			m_pRenderer->DrawImage(0,fSizeY,fSizeX,2.0f*m_pRenderer->GetHeight()/fReferenceSize,m_nWhiteTexID,0,0,0,0,0.0f,0.0f,0.0f,1.0f);

			m_pRenderer->Set2DMode(false,0,0);
		}
	}
/*#else
		if(m_bStaticBackground)
			m_pRenderer->Draw2dImage(0,(float)(m_nScrollPos-m_nTempScrollMax),800,(float)m_nTempScrollMax,m_pImage->GetTextureID(),4.0f,2.0f);	
		else
			m_pRenderer->Draw2dImage(0,0,800,600,m_pImage->GetTextureID());
#endif		*/


  // draw progress bar
  if(m_nProgressRange)
  {
//		float fRcp1024 = 1.0f / 1024.0f;
//		float fProgress = min(1.0f, m_nProgress / (float)m_nProgressRange);

//		float fTexProgress0 = 51.0f * fRcp1024;
//		float fTexProgress1 = (51.0f + fProgress * (1024.0f - 51.0f*2.0f)) * fRcp1024;

    m_pRenderer->SetState(GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA | GS_NODEPTHTEST);
		m_pRenderer->Draw2dImage(0.0, 0.0, 800.0f, 600.0f, m_nLoadingBackTexID, 0.0f, 1.0f, 1.0f, 0.0f);
//	  m_pRenderer->Draw2dImage(40, 480, fProgress * (800.0f - 40.0f*2.0f), 13, m_nLoadingBarTexID, fTexProgress0, 1.0f, fTexProgress1, 0.0f);
  }

  int nPrevMode = m_pRenderer->SetPolygonMode(0);
  //if (!m_bStaticBackground)
	DrawBuffer(m_nScrollPos, "console");
  m_pRenderer->SetPolygonMode(nPrevMode);
}

void CXConsole::DrawBuffer(int nScrollPos, const char *szEffect)
{
	if(!m_bConsoleActive && con_display_last_messages == 0)
		return;
	
	if (m_pFont && m_pRenderer)
  {
	  m_pFont->Reset();
		m_pFont->UseRealPixels(false);
	  m_pFont->SetEffect(szEffect);
	  m_pFont->SetSameSize(true);
		m_pFont->SetCharWidthScale(0.5f);
	  m_pFont->SetSize(vector2f(14, 14));
	  m_pFont->SetColor(ColorF(1,1,1,1));

		m_pFont->UseRealPixels(true);
		float csize = 0.8f * m_pFont->GetCharHeight();
		m_pFont->UseRealPixels(false);

		float ypos = nScrollPos-csize-3.0f;

		float fCharWidth=(m_pFont->GetCharWidth() * m_pFont->GetCharWidthScale());
		
		//int ypos=nScrollPos-csize-3;	

		//Draw the input line
		if(m_bConsoleActive && !m_nProgressRange)
    {
      /*m_pRenderer->DrawString(LINE_BORDER-nCharWidth, ypos, false, ">");
      m_pRenderer->DrawString(LINE_BORDER, ypos, false, m_sInputBuffer.c_str());
		  if(m_bDrawCursor)
			  m_pRenderer->DrawString(LINE_BORDER+nCharWidth*m_nCursorPos, ypos, false, "_");*/

      m_pFont->DrawString((float)(LINE_BORDER-fCharWidth), (float)ypos, ">");
			m_pFont->DrawString((float)LINE_BORDER, (float)ypos, m_sInputBuffer.c_str(),false);
		  
			if(m_bDrawCursor)
			{
				string szCursorLeft(m_sInputBuffer.c_str(), m_sInputBuffer.c_str() + m_nCursorPos);
				int n = m_pFont->GetTextLength(szCursorLeft.c_str(), false);

			  m_pFont->DrawString((float)(LINE_BORDER+(fCharWidth * n)), (float)ypos, "_");
			}
		}
		
		ypos-=csize;
		
		ConsoleBufferRItor ritor;
		ritor=m_dqConsoleBuffer.rbegin();
		int nScroll=0;
		while(ritor!=m_dqConsoleBuffer.rend() && ypos>=0)  
		{
			if(nScroll>=m_nScrollLine)
			{
				const char *buf=ritor->c_str();// GetBuf(k);
				
				if(*buf>0 && *buf<32) buf++;		// to jump over verbosity level character

				if (ypos+csize>0) 
  			  m_pFont->DrawString((float)LINE_BORDER, (float)ypos, buf,false);
					//m_pRenderer->DrawString(LINE_BORDER, ypos, false, buf);
				//CSystem::GetRenderer()->WriteXY(m_font,0,ypos,0.5f,1,1,1,1,1,buf);			
				ypos-=csize;
			}
			nScroll++;
			
			++ritor;
		} //k		
  }
}


//////////////////////////////////////////////////////////////////////////
bool CXConsole::GetLineNo( const int indwLineNo, char *outszBuffer, const int indwBufferSize ) const
{
	assert(outszBuffer);
	assert(indwBufferSize>0);

	outszBuffer[0]=0;

	ConsoleBuffer::const_reverse_iterator ritor = m_dqConsoleBuffer.rbegin();
  
	ritor+=indwLineNo;

	if(indwLineNo>=(int)m_dqConsoleBuffer.size())
		return false;

	const char *buf=ritor->c_str();// GetBuf(k);
	
	if(*buf>0 && *buf<32) buf++;		// to jump over verbosity level character

	strncpy(outszBuffer,buf,indwBufferSize-1);
	outszBuffer[indwBufferSize-1]=0;
	
	return true;
}

//////////////////////////////////////////////////////////////////////////
int CXConsole::GetLineCount() const
{
	return m_dqConsoleBuffer.size();
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::ScrollConsole()
{
	if(!m_pRenderer)
		return;
  
  int nCurrHeight=m_pRenderer->GetHeight();
  
  switch (m_sdScrollDir)
  {
/////////////////////////////////
		case sdDOWN: // The console is scrolling down
      
      // Vlads note: console should go down immediately, otherwise it can look very bad on startup
      //m_nScrollPos+=nCurrHeight/2;			
      m_nScrollPos = m_nTempScrollMax;
      
      if (m_nScrollPos>m_nTempScrollMax)
      {
        m_nScrollPos = m_nTempScrollMax;
        m_sdScrollDir = sdNONE;
      }
      break;
/////////////////////////////////      
    case sdUP: // The console is scrolling up
      
      m_nScrollPos-=nCurrHeight;//2;			
      
      if (m_nScrollPos<0)
      {
        m_nScrollPos = 0;
        m_sdScrollDir = sdNONE;				
      }
      break;
/////////////////////////////////      
    case sdNONE: 
			break;
/////////////////////////////////
  }
}


//////////////////////////////////////////////////////////////////////////
void CXConsole::AddCommand( const char *sCommand, ConsoleCommandFunc func,int nFlags, const char *sHelp )
{
	AssertName(sCommand);

	CConsoleCommand cmd;
	cmd.m_sName = sCommand;
	cmd.m_func = func;
	if (sHelp)
		cmd.m_sHelp = sHelp;
	cmd.m_nFlags = nFlags;
	m_mapCommands[sCommand] = cmd;
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::AddCommand( const char *sCommand, const char *sScriptFunc,int nFlags, const char *sHelp )
{
	AssertName(sCommand);

	CConsoleCommand cmd;
	cmd.m_sName = sCommand;
	cmd.m_sCommand = sScriptFunc;
	if (sHelp)
		cmd.m_sHelp = sHelp;
	cmd.m_nFlags = nFlags;
	m_mapCommands[sCommand] = cmd;
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::RemoveCommand(const char *sName)
{
	ConsoleCommandsMap::iterator ite = m_mapCommands.find(sName);
	if (ite != m_mapCommands.end())
		m_mapCommands.erase(ite);
}

//////////////////////////////////////////////////////////////////////////
inline bool hasprefix(const char *s, const char *prefix)
{
	while(*prefix) if(*prefix++!=*s++) return false;
	return true;
}

//////////////////////////////////////////////////////////////////////////
// remove bad characters, not very fast
static string FixAnchorName( const char *szName )
{
	string ret;

	const char *p=szName;
	bool bForceUpper=true;		// dorce uppercase in beginning and after each _

	while(*p)
	{
		if((*p>='a' && *p<='z')
		|| (*p>='A' && *p<='Z')
		|| (*p>='0' && *p<='9'))
		{
			if(bForceUpper && *p>='a' && *p<='z')
				ret += *p-'a'+'A';
			 else
				ret += *p;
		}

		bForceUpper = (*p=='_');

		++p;

	}

	return ret;
}

//////////////////////////////////////////////////////////////////////////
static const char* GetFlagsString( const uint32 dwFlags )
{
	static char sFlags[256];

// hiding this makes it a bit more difficult for cheaters
//	if(dwFlags&VF_CHEAT)									sFlags+="CHEAT, ";

	strcpy( sFlags,"" );

//	if(0 == (dwFlags&VF_NOT_NET_SYNCED))	sFlags+="REQUIRE_NET_SYNC, ";
	if(dwFlags&VF_SAVEGAME)								strcat( sFlags,"SAVEGAME, ");
	if(dwFlags&VF_READONLY)								strcat( sFlags,"READONLY, ");
	if(dwFlags&VF_DUMPTODISK)							strcat( sFlags,"DUMPTODISK, ");
	if(dwFlags&VF_REQUIRE_LEVEL_RELOAD)		strcat( sFlags,"REQUIRE_LEVEL_RELOAD, ");
	if(dwFlags&VF_REQUIRE_APP_RESTART)		strcat( sFlags,"REQUIRE_APP_RESTART, ");
	if(dwFlags&VF_RESTRICTEDMODE)					strcat( sFlags,"RESTRICTEDMODE, ");

	if(sFlags[0] != 0)
		sFlags[strlen(sFlags)-2] = 0; // remove ending chars

	return sFlags;
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::DumpCommandsVars( const char *prefix )
{
	DumpCommandsVarsTxt(prefix);
	DumpCommandsVarsWiki();
}


//////////////////////////////////////////////////////////////////////////
void CXConsole::DumpCommandsVarsTxt( const char *prefix )
{
	FILE *f0 = fopen("consolecommandsandvars.txt", "w");

	if(!f0) 
		return;

	ConsoleCommandsMapItor itrCmd, itrCmdEnd=m_mapCommands.end();
	ConsoleVariablesMapItor itrVar, itrVarEnd=m_mapVariables.end();

	fprintf(f0," CHEAT: stays in the default value if cheats are not disabled\n");
	fprintf(f0," REQUIRE_NET_SYNC: cannot be changed on client and when connecting its sent to the client\n");
	fprintf(f0," SAVEGAME: stored when saving a savegame\n");
	fprintf(f0," READONLY: can not be changed by the user\n");
	fprintf(f0,"-------------------------\n");
	fprintf(f0,"\n");

	for(itrCmd=m_mapCommands.begin(); itrCmd!=itrCmdEnd; ++itrCmd)
	{
		CConsoleCommand &cmd = itrCmd->second;

		if(hasprefix(cmd.m_sName.c_str(), prefix))
		{
			const char* sFlags = GetFlagsString(cmd.m_nFlags);

			fprintf(f0, "Command: %s %s\nscript: %s\nhelp: %s\n\n", cmd.m_sName.c_str(), sFlags, cmd.m_sCommand.c_str(), cmd.m_sHelp.c_str());
		}
	}

	for(itrVar=m_mapVariables.begin(); itrVar!=itrVarEnd; ++itrVar)
	{
		ICVar *var = itrVar->second;
		const char *types[] = { "?", "int", "float", "string", "?" };

		var->GetRealIVal();			// assert inside checks consistency for all cvars

		if(hasprefix(var->GetName(), prefix)) 
		{
			const char *sFlags = GetFlagsString(var->GetFlags());

			fprintf(f0, "variable: %s %s\ntype: %s\ncurrent: %s\nhelp: %s\n\n", var->GetName(), sFlags, types[var->GetType()], var->GetString(), var->GetHelp());
		}
	}

	fclose(f0);

	ConsoleLogInputResponse( "successfully wrote consolecommandsandvars.txt");	
}


//////////////////////////////////////////////////////////////////////////
static string GetCleanPrefix( const char *p )
{
	string sRet;

	while(*p!='_' && *p!=0)
		sRet += *p++;

	return sRet;
}


//////////////////////////////////////////////////////////////////////////
static string GetNameTillRet( const char *p )
{
	string sRet;

	while(*p!=10 && *p!=13 && *p!=0)
		sRet += *p++;

	return sRet;
}

//////////////////////////////////////////////////////////////////////////
// output data should go to: \\server41\http\twiki\data\CryAutoGen
void CXConsole::DumpCommandsVarsWiki()
{
	const char *szFolderName = "CryAutoGen";

	CryCreateDirectory(szFolderName,0);

	FILE *f1 = fopen((string(szFolderName)+"/WebHome.txt").c_str(), "w");
	if(!f1)
	{
		fclose(f1);
		return;
	}


	ConsoleCommandsMapItor itrCmd, itrCmdEnd=m_mapCommands.end();
	ConsoleVariablesMapItor itrVar, itrVarEnd=m_mapVariables.end();
	bool bFirst;
	std::map<string,const char *> mapPrefix;

	// order here doesn't matter
	mapPrefix["AI_"]	="Artificial Intelligence";
	mapPrefix["NET_"]	="Network";
	mapPrefix["ES_"]	="Entity System";
	mapPrefix["CON_"]	="Console";
	mapPrefix["AG_"]	="Animation Graph\nHigh level animation logic, describes animation selection and flow, matches animation state to current game logical state.";
	mapPrefix["AC_"]	="Animated Character\nBetter name would be 'Character Movement'.\nBridges game controlled movement and animation controlled movement.";
	mapPrefix["CA_"]	="Character Animation\nMotion synthesize and playback, parameterization through blending and inversed kinematics.";
	mapPrefix["E_"]		="3DEngine";
	mapPrefix["I_"]		="Input";
	mapPrefix["FG_"]	="Flow Graph\nhyper graph: game logic";
	mapPrefix["P_"]		="Physics";
	mapPrefix["R_"]		="Renderer";
	mapPrefix["S_"]		="Sound";
	mapPrefix["G_"]		="Game\nnot part of !CryEngine";
	mapPrefix["SYS_"]	="System";
	mapPrefix["V_"]		="Vehicle";
	mapPrefix["LUA_"]	="LUA\nscripting system";
	mapPrefix["SV_"]	="Server";
	mapPrefix["MFX_"]	="Material Effects";
	mapPrefix["CL_"]	="Client";
	mapPrefix["Q_"]		="Quality\nusually shader quality";
	mapPrefix["___"]	="Remaining";			// key defined to get it sorted in the end

	fprintf(f1,"%%META:TOPICINFO{author=\"CryEngineConsole\" format=\"1.1\" version=\"1.20\"}%%\n\n");

	fprintf(f1,"---+ !CryEngine2 Console Commands and Variables\n");
	fprintf(f1,"This list was exported from the engine by using the *DumpCommandsVars* console command.<br>\n");
	fprintf(f1,"The resulting text file can be inserted into TWiki with Copy&Paste.\n");
	fprintf(f1,"\n");

	{
		std::map<string,const char *>::const_iterator it, it2, end=mapPrefix.end();

		fprintf(f1,"---++ Registered prefixes\n<br>\n");

		for(it=mapPrefix.begin();it!=end;++it)
		{
			const char *szLocalPrefix = it->first.c_str();		// can be 0 for remaining ones
			string sCleanPrefix = GetCleanPrefix(szLocalPrefix);
			string sPrefixName = GetNameTillRet(it->second);

			fprintf(f1,"   * [[ConsolePrefix%s][%s_ %s]]\n",sCleanPrefix.c_str(),sCleanPrefix.c_str(),sPrefixName.c_str());
		}


		{
			// console commands
			for(itrCmd=m_mapCommands.begin(); itrCmd!=itrCmdEnd; ++itrCmd)
			{
				CConsoleCommand &cmd = itrCmd->second;

				FILE *f3 = fopen((string(szFolderName)+"/"+FixAnchorName(cmd.m_sName.c_str())+".txt").c_str(), "w");
				if(!f3)
				{
					fclose(f1);
					return;
				}
				const char* sFlags = GetFlagsString(cmd.m_nFlags);

				fprintf(f3,"---+++ <noautolink>%s</noautolink>\n", cmd.m_sName.c_str());
				
				if(*sFlags)
					fprintf(f3,"%%GRAY%% %s %%ENDCOLOR%%<br>\n",sFlags);

				fprintf(f3,"<noautolink><blockquote>");

				// in our wiki %TODO% is defined as <img src=\"%%ICONURL{todo}%%\" width=\"37\" height=\"16\" alt=\"TODO\" border=\"0\" />

				if(cmd.m_sHelp=="")
					fprintf(f3,"%%TODO%%\n");
				 else 
					fprintf(f3,"<verbatim>%s</verbatim>\n", cmd.m_sHelp.c_str());

				fprintf(f3,"</blockquote></noautolink>\n");

				fclose(f3);
			}
			// console variables
			for(itrVar=m_mapVariables.begin(); itrVar!=itrVarEnd; ++itrVar)
			{
				ICVar *var = itrVar->second;

				FILE *f3 = fopen((string(szFolderName)+"/"+FixAnchorName(var->GetName())+".txt").c_str(), "w");
				if(!f3)
				{
					fclose(f1);
					return;
				}

				const char* sFlags = GetFlagsString(var->GetFlags());

				fprintf(f3,"---+++ <noautolink>%s</noautolink>\n",var->GetName());

				if(*sFlags)
					fprintf(f3,"%%GRAY%% %s %%ENDCOLOR%%<br>\n",sFlags);

				fprintf(f3,"<noautolink><blockquote>");

				// in our wiki %TODO% is defined as <img src=\"%%ICONURL{todo}%%\" width=\"37\" height=\"16\" alt=\"TODO\" border=\"0\" />

				const char *szHelp = var->GetHelp();

				if(szHelp==0 || *szHelp==0)
					fprintf(f3,"%%TODO%%\n");
				 else
					fprintf(f3,"<verbatim>%s</verbatim>",var->GetHelp());

				fprintf(f3,"</blockquote></noautolink>\n");

				fclose(f3);
			}
		}


		fprintf(f1,"---++ Console commands and variables sorted by prefix\n");

		for(it=mapPrefix.begin();it!=end;++it)
		{
			const char *szLocalPrefix = it->first.c_str();		// can be 0 for remaining ones
			string sCleanPrefix = GetCleanPrefix(szLocalPrefix);
			string sPrefixName = GetNameTillRet(it->second);

			fprintf(f1,"---+++ [[ConsolePrefix%s][%s_ %s]]\n",sCleanPrefix.c_str(),sCleanPrefix.c_str(),sPrefixName.c_str());

			std::set<const char *> setCmdAndVars;		// to get console variables and commands sorted together

			// -------------------------------

			// console commands
			for(itrCmd=m_mapCommands.begin(); itrCmd!=itrCmdEnd; ++itrCmd)
			{
				CConsoleCommand &cmd = itrCmd->second;
				bool bInsert=true;

				if(it->first!="___")
				{
					if(strnicmp(cmd.m_sName.c_str(),szLocalPrefix,strlen(szLocalPrefix))!=0) 
						bInsert=false;
				}
				else
				{
					for(it2=mapPrefix.begin();it2!=end;++it2)
						if(it2->first!="___" && strnicmp(cmd.m_sName.c_str(),it2->first.c_str(),it2->first.size())==0) 
							{ bInsert=false;break; }
				}

				if(bInsert)
					setCmdAndVars.insert(cmd.m_sName.c_str());
			}

			// -------------------------------

			// console variables
			for(itrVar=m_mapVariables.begin(); itrVar!=itrVarEnd; ++itrVar)
			{
				ICVar *var = itrVar->second;
				bool bInsert=true;

				if(it->first!="___")
				{
					if(strnicmp(var->GetName(),szLocalPrefix,strlen(szLocalPrefix))!=0) 
						bInsert=false;
				}
				else
				{
					for(it2=mapPrefix.begin();it2!=end;++it2)
						if(it2->first!="___" && strnicmp(var->GetName(),it2->first.c_str(),it2->first.size())==0) 
							{ bInsert=false;break; }
				}

				if(bInsert)
					setCmdAndVars.insert(var->GetName());
			}

			// -------------------------------


			string sSubName = string("ConsolePrefix") + sCleanPrefix;

			FILE *f2 = fopen((string(szFolderName)+"/"+sSubName+".txt").c_str(), "w");
			if(!f2)
			{
				fclose(f1);
				return;
			}

			if(sCleanPrefix.empty())
				fprintf(f2,"---+ Console Commands and Variables without special prefix\n");
			 else
				fprintf(f2,"---+ Console Commands and Variables with prefix %s_\n",sCleanPrefix.c_str());

			fprintf(f2,"%s<br>\n<br>\n",it->second);	// prefix explanation and indention for following text

			fprintf(f2,"<b>Possible Flags:</b><br>\n");
			fprintf(f2,"     %%GRAY%% %s %%ENDCOLOR%% <br>",GetFlagsString(0xffffffff));
			fprintf(f2,"<br>\n");
			fprintf(f2,"<b>Alphabetically sorted:</b><br>\n");

			// log console variables and commands
			{
				std::set<const char *>::const_iterator itI, endI=setCmdAndVars.end();

				for(itI=setCmdAndVars.begin(); itI!=endI; ++itI)
				{
					fprintf(f1,"   * [[%s#Anchor%s][%s]]\n",sSubName.c_str(),FixAnchorName(*itI).c_str(), *itI);
					fprintf(f2,"   * [[#Anchor%s][%s]]\n",FixAnchorName(*itI).c_str(), *itI);
				}
			}

			fprintf(f2,"<br>\n");

			{
				fprintf(f2,"\n");
				fprintf(f2,"---++ Console Variables and Commands:\n");
				fprintf(f2,"\n");

				bFirst=true;
				{
					std::set<const char *>::const_iterator itI, endI=setCmdAndVars.end();

					for(itI=setCmdAndVars.begin(); itI!=endI; ++itI)
					{
						if(!bFirst)
							fprintf(f2,"----\n");		// separator
						bFirst=false;

						fprintf(f2,"#Anchor%s\n",FixAnchorName(*itI).c_str());		// anchor

						fprintf(f2,"%%INCLUDE{\"%s\"}%%",(FixAnchorName(*itI)+".txt").c_str());
					}
				}
			}





			fclose(f2);f2=0;

			fprintf(f1,"<br>\n");
		}
	}
	
	fclose(f1);

	ConsoleLogInputResponse( "successfully wrote directory %s",szFolderName);	

}

//////////////////////////////////////////////////////////////////////////
void CXConsole::DisplayHelp( const char *help, const char *name)
{
	if(help==0 || *help==0)
	{
		ConsoleLogInputResponse("No help available for $3%s", name);
	}
	else
	{
		char *start, *pos;
		for(pos = (char *)help, start = (char *)help; pos = strstr(pos, "\n"); start = ++pos)
		{
			string s = start;
			s.resize(pos-start);
			ConsoleLogInputResponse("    $3%s", s.c_str());
		}
		ConsoleLogInputResponse("    $3%s", start);
	}
}


void CXConsole::ExecuteString( const char *command )
{
	ExecuteStringInternal(command,false);		// not from console
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::ExecuteStringInternal(const char *command, const bool bFromConsole )
{
	assert(command);
	assert(command[0]!='\\');			// caller should remove leading "\\"

	string sTemp=command;
	string sCommand;
	ConsoleCommandsMapItor itrCmd;
	ConsoleVariablesMapItor itrVar;

	if(sTemp.empty())
		return;

#ifdef __WITH_PB__
	// If this is a PB command, PbConsoleCommand will return true
	if ( m_pNetwork )
		if ( m_pNetwork->PbConsoleCommand ( command, sTemp.length() ) )
			return ;
#endif
		
///////////////////////////
//Execute as string
	if(command[0]=='#')
	if(!con_restricted || !bFromConsole)			// in restricted mode we allow only VF_RESTRICTEDMODE CVars&CCmd
	{
		AddLine(sTemp);

		if (m_pSystem->IsDevMode())
		{
			sTemp=&command[1];
			if (m_pSystem->GetIScriptSystem())
				m_pSystem->GetIScriptSystem()->ExecuteBuffer(sTemp.c_str(),sTemp.length());
			m_bDrawCursor = 0;
		}
		else
		{
			// Warning.
			// No Cheat warnings. ConsoleWarning("Console execution is cheat protected");
		}
		return;
	}

	string::size_type nPos;

	if (GetStatus())
		AddLine(sTemp);

//		sTemp=&command[bNeedSlash?1:0];

	nPos = sTemp.find_first_of('=');

	if (nPos != string::npos)
		sCommand=sTemp.substr(0,nPos);
	 else if((nPos=sTemp.find_first_of(' ')) != string::npos)
		sCommand=sTemp.substr(0,nPos);
	 else
		sCommand=sTemp;

	sCommand.Trim();

	//////////////////////////////////////////
	//Check if is a command
	itrCmd=m_mapCommands.find(sCommand);
	if(itrCmd!=m_mapCommands.end())
	{
		if(((itrCmd->second).m_nFlags&VF_RESTRICTEDMODE) || !con_restricted || !bFromConsole)			// in restricted mode we allow only VF_RESTRICTEDMODE CVars&CCmd
		{
			sTemp=command;
			ExecuteCommand((itrCmd->second), sTemp);
			return;
		}
	}

	//////////////////////////////////////////
	//Check  if is a variable
	itrVar=m_mapVariables.find(sCommand.c_str());
	if(itrVar!=m_mapVariables.end())
	{
		ICVar *pCVar = itrVar->second;

		if((pCVar->GetFlags()&VF_RESTRICTEDMODE) || !con_restricted || !bFromConsole)			// in restricted mode we allow only VF_RESTRICTEDMODE CVars&CCmd
		{
			if(nPos!=string::npos)
			{
				sTemp=sTemp.substr(nPos+1);		// remove the command from sTemp
				sTemp.Trim(" \t\r\n\"");

				if(sTemp=="?" && ((pCVar->GetFlags() & VF_NOHELP) == 0))
				{
					ICVar *v = itrVar->second;
					DisplayHelp( v->GetHelp(), sCommand.c_str());
					return;
				}

				if(sTemp=="~" && pCVar->GetType() == CVAR_STRING)
				{
					pCVar->Set("");
					sTemp.resize(0);
				}

				if(!sTemp.empty())
					pCVar->Set(sTemp.c_str());
			}

			// the following line calls AddLine() indirectly
			DisplayVarValue( pCVar );
			//ConsoleLogInputResponse("%s=%s",pCVar->GetName(),pCVar->GetString());
			return;
		}
	}
 
	ConsoleWarning("Unknown command: %s", command);	
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::ExecuteCommand(CConsoleCommand &cmd,string &str,bool bIgnoreDevMode)
{
    std::vector<string> args;

		size_t t=1;

    for(char *p = (char *)str.c_str(); *p;)
    {
        while(*p==' ') p++;
        char *s = p;
        while(*p && *p!=' ') p++;
        if(p!=s) args.push_back(string(s, p));
    }
    
    if(args.size()>=2 && args[1]=="?" && ((cmd.m_nFlags & VF_NOHELP) == 0))
    {
			DisplayHelp(cmd.m_sHelp.c_str(), cmd.m_sName.c_str());
			return;
    }

		if((cmd.m_nFlags&VF_CHEAT)!=0 && !m_pSystem->IsDevMode() && !bIgnoreDevMode)
		{
			// No Log. ConsoleWarning("Command %s is cheat protected.", cmd.m_sName.c_str());
			return;
		}

		if (cmd.m_func)
		{
			// This is function command, execute it with a list of parameters.
			CConsoleCommandArgs cmdArgs(str, args);
			cmd.m_func( &cmdArgs );
			return;
		}

		// only do this for commands with script implementation
		for(;;) 
		{
			t = str.find_first_of("\\",t);
			if (t==string::npos)break;
			str.replace(t, 1, "\\\\", 2);
			t+=2;
		} 

		for(t=1;;) 
		{
			t = str.find_first_of("\"",t);
			if (t==string::npos)break;
			str.replace(t, 1, "\\\"", 2);
			t+=2;
		} 
    
    string buf = cmd.m_sCommand;

    size_t pp = buf.find("%%");
    if(pp!=string::npos)
    {
      string list = "";
      for(unsigned int i = 1; i<args.size(); i++)
      {
          list += string("\"") + args[i] + "\"";
          if(i<args.size()-1) list += ",";
      }
			buf.replace(pp, 2, list.c_str());          
		}
		else if((pp = buf.find("%line"))!=string::npos)
		{
			string tmp = string("\"") + str.substr(str.find(" ")+1) + "\"";
			if(args.size()>1)
			{
				buf.replace(pp, 5, tmp.c_str());
			}
			else
			{
				buf.replace(pp, 5, "");
			}
		}
    else
    {
      for(unsigned int i = 1; i<=args.size(); i++)
      {
        char pat[10];
        sprintf(pat, "%%%d", i); 
        size_t pos = buf.find(pat);
        if(pos==string::npos)
        {
          if(i!=args.size())
          {
            ConsoleWarning("Too many arguments for: %s", cmd.m_sName.c_str());
            return;
          }
        }
        else
        {
          if(i==args.size())
          {
              ConsoleWarning("Not enough arguments for: %s", cmd.m_sName.c_str());
              return;
          }
          string arg = string("\"") + args[i] + "\"";
					buf.replace(pos, strlen(pat), arg.c_str());          
        }
			}
    }

	if (m_pSystem->GetIScriptSystem())
		m_pSystem->GetIScriptSystem()->ExecuteBuffer(buf.c_str(), buf.length());
	m_bDrawCursor = 0;
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::Exit(const char * szExitComments, ...)
{
  char sResultMessageText[4096]="";
  
	if(szExitComments) 
  { 
		// make result string
    va_list		arglist;    
    va_start(arglist, szExitComments);
    vsprintf_s(sResultMessageText, sizeof(sResultMessageText), szExitComments, arglist);
    va_end(arglist);
  }
  else strcpy(sResultMessageText, "No comments from application");

	CryError( sResultMessageText );
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::RegisterAutoComplete( const char *sVarOrCommand,IConsoleArgumentAutoComplete *pArgAutoComplete )
{
	m_mapArgumentAutoComplete[sVarOrCommand] = pArgAutoComplete;
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::ResetAutoCompletion()
{
	m_nTabCount = 0;
	m_sPrevTab = "";
}

//////////////////////////////////////////////////////////////////////////
char *CXConsole::ProcessCompletion(const char *szInputBuffer)
{
	static string szInput;
	szInput = szInputBuffer;

	int offset = (szInputBuffer[0] == '\\' ? 1 : 0);		// legacy support

	if ((m_sPrevTab.size() > strlen(szInputBuffer + offset)) || strnicmp(m_sPrevTab.c_str(), (szInputBuffer + offset), m_sPrevTab.size()))
	{
		m_nTabCount = 0;
		m_sPrevTab = "";
	}

	if(szInput.empty())
		return (char *)szInput.c_str();

	int nMatch = 0;
	ConsoleCommandsMapItor itrCmds;
	ConsoleVariablesMapItor itrVars;
	bool showlist = !m_nTabCount && m_sPrevTab=="";

	if (m_nTabCount==0)
	{
		if(szInput.size()>0)
			if(szInput[0]=='\\')
				m_sPrevTab=&szInput.c_str()[1];		// legacy support
			else
			{
				m_sPrevTab=szInput;
			}

		else
			m_sPrevTab="";
	}
	//try to search in command list

	bool bArgumentAutoComplete = false;
	std::vector<string> matches;

	if (m_sPrevTab.find(' ') != string::npos)
	{
		bool bProcessAutoCompl=true;

		// Find command.
		string sVar = m_sPrevTab.substr(0,m_sPrevTab.find(' '));
		ICVar *pCVar = GetCVar(sVar.c_str());
		if (pCVar)
		{
			if(!(pCVar->GetFlags()&VF_RESTRICTEDMODE) && con_restricted)			// in restricted mode we allow only VF_RESTRICTEDMODE CVars&CCmd
				bProcessAutoCompl=false;
		}
		
		ConsoleCommandsMap::iterator it = m_mapCommands.find(sVar);
		if (it != m_mapCommands.end())
		{
			CConsoleCommand &ccmd = it->second;

			if(!(ccmd.m_nFlags&VF_RESTRICTEDMODE) && con_restricted)			// in restricted mode we allow only VF_RESTRICTEDMODE CVars&CCmd
				bProcessAutoCompl=false;
		}

		if(bProcessAutoCompl)
		{
			IConsoleArgumentAutoComplete *pArgumentAutoComplete = stl::find_in_map(m_mapArgumentAutoComplete,sVar,0);
			if (pArgumentAutoComplete)
			{
				int nMatches = pArgumentAutoComplete->GetCount();
				for (int i = 0; i < nMatches; i++)
				{
					string cmd = string(sVar) + " " + pArgumentAutoComplete->GetValue(i);
					if(strnicmp(m_sPrevTab.c_str(),cmd.c_str(),m_sPrevTab.length())==0)
					{
						bArgumentAutoComplete = true;
						matches.push_back( cmd );
					}
				}
			}
		}
	}

	if (!bArgumentAutoComplete)
	{
		itrCmds=m_mapCommands.begin();
		while(itrCmds!=m_mapCommands.end())
		{
			CConsoleCommand &cmd = itrCmds->second;

			if((cmd.m_nFlags&VF_RESTRICTEDMODE) || !con_restricted)			// in restricted mode we allow only VF_RESTRICTEDMODE CVars&CCmd
#ifdef SP_DEMO
			if((cmd.m_nFlags&(VF_CHEAT|VF_READONLY))==0 || (m_pSystem && m_pSystem->IsDevMode()))
			{
#endif
			if(strnicmp(m_sPrevTab.c_str(),itrCmds->first.c_str(),m_sPrevTab.length())==0)
#ifdef CRYSIS_BETA
				if ( AllowedForBeta(itrCmds->first) )
#endif
				matches.push_back((char *const)itrCmds->first.c_str());
#ifdef SP_DEMO
			}
#endif
			++itrCmds;
		}

		// try to search in console variables

		itrVars=m_mapVariables.begin();
		while(itrVars!=m_mapVariables.end()) 
		{
			ICVar *pVar = itrVars->second;

			if((pVar->GetFlags()&VF_RESTRICTEDMODE) || !con_restricted)			// in restricted mode we allow only VF_RESTRICTEDMODE CVars&CCmd
#ifdef SP_DEMO
			if((pVar->GetFlags()&(VF_CHEAT|VF_READONLY))==0 || (m_pSystem && m_pSystem->IsDevMode()))
			{
#endif
			//if(itrVars->first.compare(0,m_sPrevTab.length(),m_sPrevTab)==0)
			if(strnicmp(m_sPrevTab.c_str(),itrVars->first,m_sPrevTab.length())==0)
#ifdef CRYSIS_BETA
				if ( AllowedForBeta(itrVars->first) )
#endif
				matches.push_back((char *const)itrVars->first);
#ifdef SP_DEMO
			}
#endif
			++itrVars;
		}
	}

#ifdef __WITH_PB__
	// Check to see if this is a PB command
	const char pbCompleteBuf[PB_Q_MAXRESULTLEN] = "";
	strncpy ( (char *)pbCompleteBuf, szInputBuffer, PB_Q_MAXRESULTLEN ) ;
	if ( !strncmp ( szInputBuffer, "pb_", 3 ) ) {
		if ( !strncmp ( szInputBuffer, "pb_sv", 5 ) ) {
			if ( m_pNetwork )
				m_pNetwork->PbServerAutoComplete( pbCompleteBuf, PB_Q_MAXRESULTLEN ) ;
		}
		else {
			if ( m_pNetwork )
				m_pNetwork->PbClientAutoComplete( pbCompleteBuf, PB_Q_MAXRESULTLEN ) ;
		}

		if (0 != strcmp(szInputBuffer, pbCompleteBuf))
#ifdef CRYSIS_BETA
			if ( AllowedForBeta(pbCompleteBuf) )
#endif
			matches.push_back((char *const)pbCompleteBuf);
	}
#endif

	if(!matches.empty())
		std::sort(matches.begin(), matches.end(), [](const string& left, const string& right)
		{
			return less_CVar(left.c_str(), right.c_str());
		});		// to sort commands with variables

	if (showlist && !matches.empty()) 
	{
		ConsoleLogInput(" ");		// empty line before auto completion

		for(std::vector<string>::iterator i = matches.begin(); i!=matches.end(); ++i)
		{
			// List matching variables
			const char *sVar = i->c_str();
			ICVar *pVar = GetCVar( sVar );

			if (pVar)
				DisplayVarValue( pVar );
			 else
				ConsoleLogInputResponse( "    $3%s $6(Command)", sVar );
		}
	}

	for(std::vector<string>::iterator i = matches.begin(); i!=matches.end(); ++i)
	{
		if(m_nTabCount<=nMatch)
		{
			szInput = *i;
			szInput += " ";
			m_nTabCount=nMatch+1;
			return (char *)szInput.c_str();
		}
		nMatch++;
	}

	if (m_nTabCount>0)
	{
		m_nTabCount=0;
		szInput = m_sPrevTab;
		szInput = ProcessCompletion(szInput.c_str());
	}

	return (char *)szInput.c_str();
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::DisplayVarValue( ICVar *pVar  )
{
	if (!pVar)
		return;

	const char * sFlagsString=GetFlagsString(pVar->GetFlags());

	string sValue = pVar->GetString();
	string sVar = pVar->GetName();

	char szRealState[20]="";

	if(pVar->GetType()==CVAR_INT)
	{
		int iRealState = pVar->GetRealIVal();

		if(iRealState!=pVar->GetIVal())
		{
			if(iRealState==-1) strcpy(szRealState," RealState=Custom");
			else
				sprintf(szRealState," RealState=%d",iRealState);
		}
	}

	if (m_pSystem && m_pSystem->IsEditor())
		ConsoleLogInputResponse( "%s=%s [ %s ]%s",sVar.c_str(),sValue.c_str(),sFlagsString,szRealState);
	 else
		ConsoleLogInputResponse( "    $3%s = $6%s $5[%s]$4%s", sVar.c_str(),sValue.c_str(),sFlagsString,szRealState);
}

//////////////////////////////////////////////////////////////////////////
bool CXConsole::IsOpened()
{
	return m_nScrollPos == m_nTempScrollMax;
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::PrintLine(const char *s)
{
	AddLine(s);
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::PrintLinePlus(const char *s)
{
	AddLinePlus(s);
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::AddLine(string str)
{
	// strip trailing \n or \r.
	if (!str.empty() && (str[str.size()-1]=='\n' || str[str.size()-1]=='\r'))
		str.resize(str.size()-1);

	string::size_type nPos;
	while ((nPos=str.find('\n'))!=string::npos)
	{
		str.replace(nPos,1,1,' ');
	}

	while ((nPos=str.find('\r'))!=string::npos)
	{
		str.replace(nPos,1,1,' ');
	}

	m_dqConsoleBuffer.push_back(str);

	int nBufferSize = con_line_buffer_size;

	while(((int)(m_dqConsoleBuffer.size()))>nBufferSize)
	{
		m_dqConsoleBuffer.pop_front();
	}

	// tell everyone who is interested (e.g. dedicated server printout)
	{
		std::vector<IOutputPrintSink *>::iterator it;

		for(it=m_OutputSinks.begin();it!=m_OutputSinks.end();++it)
			(*it)->Print(str.c_str());
	}
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::ResetProgressBar(int nProgressBarRange)
{
	m_nProgressRange = nProgressBarRange;
	m_nProgress = 0;

	if (nProgressBarRange < 0)
		nProgressBarRange = 0;

	if (!m_nProgressRange)
	{
		if (m_nLoadingBackTexID)
		{
			if (m_pRenderer)
				m_pRenderer->RemoveTexture(m_nLoadingBackTexID);
			m_nLoadingBackTexID = -1;
		}
	}

	static ICVar *log_Verbosity = GetCVar("log_Verbosity");

	if (log_Verbosity && (!log_Verbosity->GetIVal()))
		Clear();
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::TickProgressBar()
{	
	if (m_nProgressRange != 0 && m_nProgressRange > m_nProgress)
	{
		m_nProgress++;
		m_pSystem->UpdateLoadingScreen();
	}

	//if(m_pSystem->GetIGame())
		//m_pSystem->GetIGame()->UpdateDuringLoading();		// network update
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::SetLoadingImage(const char *szFilename )
{
	ITexture *pTex = 0;		

	pTex = m_pSystem->GetIRenderer()->EF_LoadTexture(szFilename, FT_DONT_RESIZE | FT_NOMIPS, eTT_2D);

	if (!pTex || (pTex->GetFlags() & FT_FAILED))
	{
    SAFE_RELEASE(pTex);
		pTex = m_pSystem->GetIRenderer()->EF_LoadTexture("Textures/Console/loadscreen_default.dds", FT_DONT_RESIZE | FT_NOMIPS, eTT_2D);
	}

	if (pTex)
	{
		m_nLoadingBackTexID = pTex->GetTextureID();
	}
	else
	{
		m_nLoadingBackTexID = -1;
	}
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::AddOutputPrintSink( IOutputPrintSink *inpSink )
{
	assert(inpSink);

	m_OutputSinks.push_back(inpSink);
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::RemoveOutputPrintSink( IOutputPrintSink *inpSink )
{
	assert(inpSink);

	int nCount=m_OutputSinks.size();

	for(int i=0;i<nCount;i++)
	{
		if(m_OutputSinks[i]==inpSink)
		{
			if(nCount<=1)	m_OutputSinks.clear();
			else
			{
				m_OutputSinks[i]=m_OutputSinks.back();
				m_OutputSinks.pop_back();
			}
			return;
		}
	}

	assert(0);
}


//////////////////////////////////////////////////////////////////////////
void CXConsole::AddLinePlus(string str)
{
	if(!m_dqConsoleBuffer.size())
   return;

	// strip trailing \n or \r.
	if (!str.empty() && (str[str.size()-1]=='\n' || str[str.size()-1]=='\r'))
		str.resize(str.size()-1);

	string::size_type nPos;
	while ((nPos=str.find('\n'))!=string::npos)
		str.replace(nPos,1,1,' ');

	while ((nPos=str.find('\r'))!=string::npos)
		str.replace(nPos,1,1,' ');

	string tmpStr = m_dqConsoleBuffer.back();// += str;

	m_dqConsoleBuffer.pop_back();

  if(tmpStr.size()<256)
    m_dqConsoleBuffer.push_back(tmpStr + str);
  else 
    m_dqConsoleBuffer.push_back(tmpStr);

	// tell everyone who is interested (e.g. dedicated server printout)
	{
		std::vector<IOutputPrintSink *>::iterator it;

		for(it=m_OutputSinks.begin();it!=m_OutputSinks.end();++it)
			(*it)->Print((tmpStr + str).c_str());
	}
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::AddInputChar(const char c)
{
	if(m_nCursorPos<(int)(m_sInputBuffer.length()))
		m_sInputBuffer.insert(m_nCursorPos,1,c);
	else
		m_sInputBuffer=m_sInputBuffer+c;
	m_nCursorPos++;
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::ExecuteInputBuffer()
{
	string sTemp=m_sInputBuffer;
	if(m_sInputBuffer.empty())
		return;
	m_sInputBuffer="";
#ifdef CRYSIS_BETA
	int i = 0;
	string cmd = sTemp.Tokenize("# =", i);
	if ( AllowedForBeta(cmd) )
		goto L_execute;
	CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "You are not allowed to execute '%s' during BETA test!", sTemp.c_str());
	m_nCursorPos = 0;
	return;
L_execute:
#endif
	AddCommandToHistory(sTemp.c_str());

	ExecuteStringInternal(sTemp.c_str(),true);			// from console
	
	m_nCursorPos=0;
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::RemoveInputChar(bool bBackSpace)
{
	if(m_sInputBuffer.empty())
		return;

	if(bBackSpace)
	{
		if(m_nCursorPos>0)
		{
			m_sInputBuffer.erase(m_nCursorPos-1,1);
			m_nCursorPos--;
		}
	}
	else
	{
		if(m_nCursorPos<(int)(m_sInputBuffer.length()))
			m_sInputBuffer.erase(m_nCursorPos,1);
	}
}


//////////////////////////////////////////////////////////////////////////
void CXConsole::AddCommandToHistory( const char *szCommand )
{
	assert(szCommand);

	m_nHistoryPos=-1;

	if(!m_dqHistory.empty())
	{
		// add only if the command is != than the last
		if(m_dqHistory.front()!=szCommand)
			m_dqHistory.push_front(szCommand);
	}
	else
		m_dqHistory.push_front(szCommand);

	while(m_dqHistory.size()>MAX_HISTORY_ENTRIES)
		m_dqHistory.pop_back();
}


//////////////////////////////////////////////////////////////////////////
void CXConsole::Copy()
{
#ifdef WIN32
	if (m_sInputBuffer.empty())
		return;

	if (!OpenClipboard(NULL))
		return;

	size_t cbLength = m_sInputBuffer.length();

	HGLOBAL hGlobal;
	LPVOID  pGlobal;

	hGlobal = GlobalAlloc(GHND, cbLength + 1);
	pGlobal = GlobalLock (hGlobal);

	strcpy((char *)pGlobal, m_sInputBuffer.c_str());

	GlobalUnlock(hGlobal);

	EmptyClipboard();
	SetClipboardData(CF_TEXT, hGlobal);
	CloseClipboard();

	return;
#endif //WIN32
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::Paste()
{
#ifdef WIN32
	//TRACE("Paste\n");
	
	static char sTemp[256];
	HGLOBAL hGlobal;
  void*  pGlobal;
	
	
	if (!IsClipboardFormatAvailable(CF_TEXT)) 
		return; 
	if (!OpenClipboard(NULL))
		return;
	
	hGlobal = GetClipboardData(CF_TEXT);
	if(!hGlobal)
		return;

	pGlobal = GlobalLock(hGlobal);
	size_t cbLength = min(sizeof(sTemp)-1, strlen((char *)pGlobal));
	
	strncpy(sTemp, (char *)pGlobal, cbLength);
	sTemp[cbLength] = '\0';
	GlobalUnlock(hGlobal);
	CloseClipboard();
	
	m_sInputBuffer.insert(m_nCursorPos,sTemp,strlen(sTemp));
	m_nCursorPos+=(int)strlen(sTemp);
#endif //WIN32
}


//////////////////////////////////////////////////////////////////////////
int CXConsole::GetNumVars()
{
	return (int)m_mapVariables.size();
}

//////////////////////////////////////////////////////////////////////////
size_t CXConsole::GetSortedVars( const char **pszArray, size_t numItems, const char *szPrefix )
{
	size_t i = 0;
	size_t iPrefixLen = szPrefix ? strlen(szPrefix) : 0;

	// variables
	{
		ConsoleVariablesMap::const_iterator it, end=m_mapVariables.end();
		for(it=m_mapVariables.begin(); it!=end; ++it)
		{
			if(pszArray && i>=numItems)
				break;

			if(szPrefix)
			if(strnicmp(it->first,szPrefix,iPrefixLen)!=0)
				continue;

			if(pszArray)
				pszArray[i] = it->first;

			i++;
		}
	}

	// commands
	{
		ConsoleCommandsMap::iterator it, end=m_mapCommands.end();
		for(it=m_mapCommands.begin(); it!=end; ++it)
		{
			if(pszArray && i>=numItems)
				break;

			if(szPrefix)
				if(strnicmp(it->first.c_str(),szPrefix,iPrefixLen)!=0)
					continue;

			if(pszArray)
				pszArray[i] = it->first.c_str();

			i++;
		}
	}

	if(i!=0 && pszArray)
		std::sort(pszArray, pszArray + i, [](const char* left, const char* right)
		{
			return less_CVar(left, right);
		});

	return i;
}

//////////////////////////////////////////////////////////////////////////
const char* CXConsole::AutoComplete( const char* substr )
{
	// following code can be optimized

	std::vector<const char*> cmds;
	cmds.resize( GetNumVars()+m_mapCommands.size() );
	size_t cmdCount = GetSortedVars( &cmds[0],cmds.size() );

	size_t substrLen = strlen(substr);
	
	// If substring is empty return first command.
	if (substrLen==0 && cmdCount>0)
		return cmds[0];

	// find next
	for (size_t i = 0; i < cmdCount; i++)
	{
		const char *szCmd = cmds[i];
		size_t cmdlen = strlen(szCmd);
		if (cmdlen >= substrLen && memcmp(szCmd,substr,substrLen) == 0)
		{
			if (substrLen == cmdlen)
			{
				i++;
				if (i < cmdCount)
					return cmds[i];
				return cmds[i-1];
			}
			return cmds[i];
		}
	}

	// then first matching case insensitive
	for (size_t i = 0; i < cmdCount; i++)
	{
		const char *szCmd = cmds[i];

		size_t cmdlen = strlen(szCmd);
		if (cmdlen >= substrLen && memicmp(szCmd,substr,substrLen) == 0)
		{
			if (substrLen == cmdlen)
			{
				i++;
				if (i < cmdCount)
					return cmds[i];
				return cmds[i-1];
			}
			return cmds[i];
		}
	}

	// Not found.
	return "";
}
	
void CXConsole::SetInputLine( const char *szLine )
{
	assert(szLine);

	m_sInputBuffer = szLine;
	m_nCursorPos=m_sInputBuffer.size();
}



//////////////////////////////////////////////////////////////////////////
const char* CXConsole::AutoCompletePrev( const char* substr )
{
	std::vector<const char*> cmds;
	cmds.resize( GetNumVars()+m_mapCommands.size() );
	size_t cmdCount = GetSortedVars( &cmds[0],cmds.size() );

	// If substring is empty return last command.
	if (strlen(substr)==0 && cmds.size()>0)
		return cmds[cmdCount-1];

	for (unsigned int i = 0; i < cmdCount; i++)
	{
		if (stricmp(substr,cmds[i])==0)
		{
			if (i > 0) 
				return cmds[i-1];
			else
				return cmds[0];
		}
	}
	return AutoComplete( substr );
}

//////////////////////////////////////////////////////////////////////////
inline size_t sizeOf (const string& str)
{
	return str.capacity()+1;
}

//////////////////////////////////////////////////////////////////////////
inline size_t sizeOf (const char* sz)
{
	return sz? strlen(sz)+1:0;
}

//////////////////////////////////////////////////////////////////////////
inline size_t sizeOf (const CXConsole::ConsoleBuffer& Buffer)
{
	size_t nSize = 0;
	CXConsole::ConsoleBuffer::const_iterator it = Buffer.begin(), itEnd = Buffer.end();
	for (; it != itEnd; ++it)
	{
		nSize += sizeof(*it) + sizeOf (*it);
	}
	return nSize;
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::GetMemoryUsage (class ICrySizer* pSizer)
{
	pSizer->AddObject (this, sizeof(*this)
		+ sizeOf (m_dqConsoleBuffer)
		+ sizeOf (m_dqHistory)
		+ sizeOf (m_sInputBuffer)
		+ sizeOf (m_sPrevTab)
		);

	{
		ConsoleCommandsMap::const_iterator it = m_mapCommands.begin(), itEnd = m_mapCommands.end();
		for (;it != itEnd; ++it)
			pSizer->AddObject (&(*it), sizeof(*it) + sizeOf(it->first) + it->second.sizeofThis());
	}

	{
		ConsoleBindsMap::const_iterator it = m_mapBinds.begin(), itEnd = m_mapBinds.end();
		for (; it != itEnd; ++it)
			pSizer->AddObject (&(*it), sizeof(*it) + sizeOf (it->first) + sizeOf (it->second));
	}

	{
		ConsoleVariablesMap::const_iterator it = m_mapVariables.begin(), itEnd = m_mapVariables.end();
		for (; it != itEnd; ++it)
		{
			pSizer->AddObject (&*it, sizeof(*it) + sizeOf(it->first));
			it->second->GetMemoryUsage (pSizer);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::ConsoleLogInputResponse( const char *format,... )
{
	va_list args;
	va_start(args,format);
	gEnv->pLog->LogV( ILog::eInputResponse,format,args );
	va_end(args);
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::ConsoleLogInput( const char *format,... )
{
	va_list args;
	va_start(args,format);
	gEnv->pLog->LogV( ILog::eInput,format,args );
	va_end(args);
}


//////////////////////////////////////////////////////////////////////////
void CXConsole::ConsoleWarning( const char *format,... )
{
	va_list args;
	va_start(args,format);
	gEnv->pLog->LogV( ILog::eWarningAlways,format,args );
	va_end(args);
}

//////////////////////////////////////////////////////////////////////////
bool CXConsole::OnBeforeVarChange( ICVar *pVar,const char *sNewValue )
{
	if (((pVar->GetFlags()&VF_CHEAT)!=0) && !gEnv->pSystem->IsDevMode())
		return (false);

	if (!m_consoleVarSinks.empty())
	{
		ConsoleVarSinks::iterator it,next;
		for (it = m_consoleVarSinks.begin(); it != m_consoleVarSinks.end(); it = next)
		{
			next = it; next++;
			if (!(*it)->OnBeforeVarChange(pVar,sNewValue))
				return false;
		}
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::OnAfterVarChange( ICVar *pVar )
{
	if (!m_consoleVarSinks.empty())
	{
		ConsoleVarSinks::iterator it,next;
		for (it = m_consoleVarSinks.begin(); it != m_consoleVarSinks.end(); it = next)
		{
			next = it; next++;
			(*it)->OnAfterVarChange(pVar);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::AddConsoleVarSink( IConsoleVarSink *pSink )
{
	m_consoleVarSinks.push_back(pSink);
}

//////////////////////////////////////////////////////////////////////////
void CXConsole::RemoveConsoleVarSink( IConsoleVarSink *pSink )
{
	m_consoleVarSinks.remove(pSink);
}
