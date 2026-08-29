// XConsole.h: interface for the CXConsole class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_XCONSOLE_H__BA902011_5C47_4954_8E09_68598456912D__INCLUDED_)
#define AFX_XCONSOLE_H__BA902011_5C47_4954_8E09_68598456912D__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "CryCommon/CrySystem/IConsole.h"
#include "CryCommon/CryInput/IInput.h"

//forward declaration
struct IIpnut;
class CSystem;

#define	CURSOR_TIME		1.0f/2.0f // half second

#define MAX_HISTORY_ENTRIES 50
#define LINE_BORDER 10

enum ScrollDir
{
	sdDOWN,
	sdUP,
	sdNONE
};

//////////////////////////////////////////////////////////////////////////
// Console command holds information about commands registered to console.
//////////////////////////////////////////////////////////////////////////
struct CConsoleCommand
{
	string m_sName;            // Console command name
	string m_sCommand;         // lua code that is executed when this command is invoked
	string m_sHelp;            // optional help string - can be shown in the console with "<commandname> ?"
	int    m_nFlags;           // bitmask consist of flag starting with VF_ e.g. VF_CHEAT
	ConsoleCommandFunc m_func; // Pointer to console command.

	//////////////////////////////////////////////////////////////////////////
	CConsoleCommand() : m_func(0), m_nFlags(0) {}
	size_t sizeofThis ()const {return sizeof(*this) + m_sName.capacity()+1 + m_sCommand.capacity()+1;}
};

//////////////////////////////////////////////////////////////////////////
// Implements IConsoleCmdArgs.
//////////////////////////////////////////////////////////////////////////
struct CConsoleCommandArgs : public IConsoleCmdArgs
{
	CConsoleCommandArgs( string &line, std::vector<string> &args ) : m_line(line), m_args(args) {};
	virtual int GetArgCount() const { return m_args.size(); };
	// Get argument by index, nIndex must be in 0 <= nIndex < GetArgCount()
	virtual const char* GetArg( int nIndex ) const
	{
		assert( nIndex >= 0 && nIndex < GetArgCount() );
		if( !(nIndex >= 0 && nIndex < GetArgCount()) )
			return NULL;
		return m_args[nIndex].c_str();
	}
	virtual const char *GetCommandLine() const
	{
		return m_line.c_str();
	}

private:
	std::vector<string> &m_args;
	string							&m_line;
};


struct string_nocase_lt 
{
	bool operator()( const char *s1,const char *s2 ) const 
	{
		return stricmp(s1,s2) < 0;
	}
	bool operator()( const string &s1,const string &s2 ) const 
	{
		return stricmp(s1.c_str(),s2.c_str()) < 0;
	}
};

/* - very dangerous to use with STL containers
struct string_nocase_lt 
{
	bool operator()( const char *s1,const char *s2 ) const 
	{
		return stricmp(s1,s2) < 0;
	}
	bool operator()( const string &s1,const string &s2 ) const 
	{
		return stricmp(s1.c_str(),s2.c_str()) < 0;
	}
};
*/

//forward declarations
class ITexture;
struct IRenderer;


/*! engine console implementation
	@see IConsole
*/
class CXConsole : public IConsole,public IInputEventListener
{
public:
	typedef std::deque<string> ConsoleBuffer;
	typedef ConsoleBuffer::iterator ConsoleBufferItor;
	typedef ConsoleBuffer::reverse_iterator ConsoleBufferRItor;

	//! constructor
	CXConsole();
	//! destructor
	virtual ~CXConsole();

	//!
	void Init(CSystem *pSystem);
	//!
	void SetStatus(bool bActive){ m_bConsoleActive=bActive;}
	bool GetStatus() const { return m_bConsoleActive; }
	//!
	void DrawBuffer(int nScrollPos, const char *szEffect);
	//!
//	void RefreshVariable(string sVarName);
	//!
  void FreeRenderResources();
	//!
	void Copy();
	//!
	void Paste();


	// interface IConsole ---------------------------------------------------------

	virtual void Release();
	virtual ICVar *RegisterString(const char *sName,const char *sValue,int nFlags, const char *help = "",ConsoleVarFunc pChangeFunc=0 );
	virtual ICVar *RegisterInt(const char *sName,int iValue,int nFlags, const char *help = "",ConsoleVarFunc pChangeFunc=0 );
	virtual ICVar *RegisterFloat(const char *sName,float fValue,int nFlags, const char *help = "",ConsoleVarFunc pChangeFunc=0 );
	virtual ICVar *Register(const char *name, float *src, float defaultvalue, int flags=0, const char *help = "",ConsoleVarFunc pChangeFunc=0 );
	virtual ICVar *Register(const char *name, int *src, int defaultvalue, int flags=0, const char *help = "",ConsoleVarFunc pChangeFunc=0 );

	virtual void UnregisterVariable(const char *sVarName,bool bDelete=false);
	virtual void SetScrollMax(int value);
	virtual void AddOutputPrintSink( IOutputPrintSink *inpSink );
	virtual void RemoveOutputPrintSink( IOutputPrintSink *inpSink );
	virtual void ShowConsole( bool show, const int iRequestScrollMax=-1 );
	virtual void DumpCVars(ICVarDumpSink *pCallback,unsigned int nFlagsFilter=0);
	virtual void DumpKeyBinds(IKeyBindDumpSink *pCallback );
	virtual void CreateKeyBind(const char *sCmd,const char *sRes);
	virtual const char* FindKeyBind( const char *sCmd ) const;
	virtual void	SetImage(ITexture *pImage,bool bDeleteCurrent); 
	virtual inline ITexture *GetImage() { return m_pImage; }
	virtual void StaticBackground(bool bStatic) { m_bStaticBackground=bStatic; }
	virtual bool GetLineNo( const int indwLineNo, char *outszBuffer, const int indwBufferSize ) const;
	virtual int GetLineCount() const;
	virtual ICVar *GetCVar( const char *name );
	virtual char *GetVariable( const char * szVarName, const char * szFileName, const char * def_val );
  virtual float GetVariable( const char * szVarName, const char * szFileName, float def_val );
	virtual void PrintLine(const char *s);
  virtual void PrintLinePlus(const char *s);
	virtual bool GetStatus();
	virtual void Clear();
	virtual void Update();
	virtual void Draw();
	virtual void AddCommand( const char *sCommand, ConsoleCommandFunc func,int nFlags=0, const char *sHelp=NULL );
	virtual void AddCommand(const char *sName, const char *sScriptFunc, int nFlags=0, const char *sHelp=NULL );
	virtual void RemoveCommand(const char *sName);
	virtual void ExecuteString(const char *command);
	virtual void Exit(const char *command,...) PRINTF_PARAMS(2, 3);
	virtual bool IsOpened();
	virtual int GetNumVars();
	virtual size_t GetSortedVars( const char **pszArray, size_t numItems, const char *szPrefix=0 );
	virtual const char*	AutoComplete( const char* substr );
	virtual const char*	AutoCompletePrev( const char* substr );
	virtual char *ProcessCompletion( const char *szInputBuffer);
	virtual void RegisterAutoComplete( const char *sVarOrCommand,IConsoleArgumentAutoComplete *pArgAutoComplete );
	virtual void ResetAutoCompletion();
	virtual void GetMemoryUsage (class ICrySizer* pSizer);
	virtual void ResetProgressBar(int nProgressRange);
	virtual void TickProgressBar();
	virtual void SetLoadingImage(const char *szFilename);
	virtual void AddConsoleVarSink( IConsoleVarSink *pSink );
	virtual void RemoveConsoleVarSink( IConsoleVarSink *pSink );
	virtual const char* GetHistoryElement( const bool bUpOrDown );
	virtual void AddCommandToHistory( const char *szCommand );
	virtual void SetInputLine( const char *szLine );
	virtual void LoadConfigVar( const char *sVariable, const char *sValue );
	virtual void EnableActivationKey(bool bEnable);

	// interface IInputEventListener ------------------------------------------------------------------

	virtual bool OnInputEvent( const SInputEvent &event );
	virtual bool OnInputEventUI( const SInputEvent &event );

	// interface IConsoleVarSink ----------------------------------------------------------------------

	virtual bool OnBeforeVarChange( ICVar *pVar,const char *sNewValue );
	virtual void OnAfterVarChange( ICVar *pVar );

	//////////////////////////////////////////////////////////////////////////
	
	void DumpCommandsVars( const char *prefix );

	// Returns
	//   0 if the operation failed
	ICVar *RegisterCVarGroup( const char *sName, const char *szFileName );

protected: // ----------------------------------------------------------------------------------------

	void RegisterVar( ICVar *pCVar,ConsoleVarFunc pChangeFunc=0 );

	bool ProcessInput(const SInputEvent& event);
	void AddLine(string str);
  void AddLinePlus(string str);
	void AddInputChar(const char c);
	void RemoveInputChar(bool bBackSpace);
	void ExecuteInputBuffer();
	void ExecuteCommand( CConsoleCommand &cmd,string &params,bool bIgnoreDevMode = false );
 
	void ScrollConsole();

	void DumpCommandsVarsTxt( const char *prefix );
	void DumpCommandsVarsWiki();

	void ConsoleLogInputResponse( const char *szFormat,... ) PRINTF_PARAMS(2, 3);
	void ConsoleLogInput( const char *szFormat,... ) PRINTF_PARAMS(2, 3);
	void ConsoleWarning( const char *szFormat,... ) PRINTF_PARAMS(2, 3);

	void DisplayHelp( const char *help, const char *name );
	void DisplayVarValue( ICVar *pVar  );

	// Arguments:
	//   bFromConsole - true=from console, false=from outside
	void ExecuteStringInternal(const char *command, const bool bFromConsole );

private: // ----------------------------------------------------------
	ConsoleBuffer										m_dqConsoleBuffer;
	ConsoleBuffer										m_dqHistory;

	bool														m_bStaticBackground;
	int															m_nLoadingBackTexID;
//	int															m_nLoadingBarTexID;		depricated feature, scaleform is used instead
	int															m_nWhiteTexID;
	int															m_nProgress;
	int															m_nProgressRange;

	string													m_sInputBuffer;

	string													m_sPrevTab;
	int															m_nTabCount;

	//class CXConsoleVariable;
	typedef std::map<const char *, ICVar *,string_nocase_lt> ConsoleVariablesMap;		// key points into string stored in ICVar or in .exe/.dll
	typedef ConsoleVariablesMap::iterator ConsoleVariablesMapItor;

	typedef std::map<string,CConsoleCommand,string_nocase_lt> ConsoleCommandsMap;
	typedef ConsoleCommandsMap::iterator ConsoleCommandsMapItor;

	typedef std::map<string,string> ConsoleBindsMap;
	typedef ConsoleBindsMap::iterator ConsoleBindsMapItor;

	typedef std::map<string,IConsoleArgumentAutoComplete*,string_nocase_lt> ArgumentAutoCompleteMap;

	typedef std::map<string,string,string_nocase_lt> ConfigVars;

	ConsoleCommandsMap							m_mapCommands;						//!<
	ConsoleBindsMap									m_mapBinds;								//!<
	ConsoleVariablesMap							m_mapVariables;						//!<
	std::vector<IOutputPrintSink *>	m_OutputSinks;						//!< objects in this vector are not released

	ArgumentAutoCompleteMap         m_mapArgumentAutoComplete;

	typedef std::list<IConsoleVarSink*>	ConsoleVarSinks;
	ConsoleVarSinks									m_consoleVarSinks;

	ConfigVars											m_configVars;

	int															m_nScrollPos;
	int															m_nTempScrollMax;					// for currently opened console, reset to m_nScrollMax
	int															m_nScrollMax;							//
	int															m_nScrollLine;
	int															m_nHistoryPos;
	int															m_nCursorPos;
	ITexture*												m_pImage;

	bool														m_bRepeat;
	float														m_nRepeatTimer;
	SInputEvent											m_nRepeatEvent;

	ScrollDir												m_sdScrollDir;

	bool														m_bDrawCursor;

	bool														m_bConsoleActive;
	bool														m_bActivationKeyEnable;

	CSystem	*												m_pSystem;
	IFFont *												m_pFont;
	IRenderer *											m_pRenderer;
	IInput *												m_pInput;
	ITimer *												m_pTimer;
	INetwork *											m_pNetwork;   // EvenBalance - M. Quinn

	ICVar *													m_pSysDeactivateConsole;

	static int con_display_last_messages;
	static int con_line_buffer_size;
	static int con_showonload;
	static int con_debug;
	static int con_restricted;

//	friend void Command_DumpCommandsVars(IConsoleCmdArgs* Cmd);
};

#endif // !defined(AFX_XCONSOLE_H__BA902011_5C47_4954_8E09_68598456912D__INCLUDED_)
