/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:
Console implementation for UNIX systems, based on curses/ncurses.
-------------------------------------------------------------------------
History:
- Jul 04,2006:	Created by Sascha Demetrio

*************************************************************************/

#if !defined(_UNIXCONSOLE_H_)
#define _UNIXCONSOLE_H_ 1

#if defined(USE_UNIXCONSOLE)

#if defined(WIN32)
// Avoid naming conflict with wincon.h.
#undef MOUSE_MOVED
#endif

#include "CryCommon/CrySystem/IConsole.h"
#include "CryThread.h"
#include <curses.h>
#include <deque>
#include "CryCommon/CryNetwork/INetwork.h"
#include "CryCommon/CrySystem/ITextModeConsole.h"

// Avoid naming conflicts with pdcurses.
// Use werase(stdscr) instead of erase().
#undef erase
// Use wclear(stdscr) instead of clear().
#undef clear

// Define if you wish to enable the player count feature.
// Note: The player count feature can not be used when building
// Windows-style DLLs!
#if defined(LINUX)
#define UC_ENABLE_PLAYER_COUNT 1
#else
#undef UC_ENABLE_PLAYER_COUNT
#endif

// Define if you wish to enable magic console commands.
// These are commands starting with an '@' character, which are intercepted by
// the CUNIXConsole class and not passed to the system.
//#undef UC_ENABLE_MAGIC_COMMANDS
#define UC_ENABLE_MAGIC_COMMANDS 1

class CUNIXConsoleInputThread;
class CUNIXConsoleSignalHandler;

class CUNIXConsole
  : public IOutputPrintSink,
		public ISystemUserCallback,
		public ITextModeConsole
{
	friend class CUNIXConsoleInputThread;
	friend class CUNIXConsoleSignalHandler;

	static const int DEFAULT_COLOR = -1;

	typedef CryCondLock<CRYLOCK_FAST> ConsoleLock;
	ConsoleLock m_Lock;

	enum EConDrawOp
	{
		eCDO_PutText,
	};
	struct SConDrawCmd
	{
		EConDrawOp op;
		int x, y;
		char text[256];
	};
	DynArray<SConDrawCmd> m_drawCmds;
	DynArray<SConDrawCmd> m_newCmds;
	bool m_fsMode;

	INetNub::SStatistics m_nubStats;

#if !defined(NDEBUG)
	bool IsLocked() { return m_Lock.IsLocked(); }
#endif

	// The header string.
	//
	// Should be set by the launcher through SetHeader().
	string m_HeaderString;

	// The line buffer.
	//
	// We'll use the escape sequence "\1" followed by a digit to encode color
	// changes.
	typedef std::deque<string> TLineBuffer;
	TLineBuffer m_LineBuffer;

	// The command queue.
	//
	// Commands typed on the console are added to this command queue.  It is
	// processed by the OnUpdate() callback.
	typedef std::deque<string> TCommandQueue;
	TCommandQueue m_CommandQueue;

	// The command history.
	//
	// The UNIX console is decoupled from the system console object through a
	// command queue, so we can't use the history buffer from the system
	// console.  This is our own command history.
	//
	// The history index indicates the reverse index (counting from the end)
	// into our command history.  The special value -1 indicates that we're not
	// currently showing a command from the history.
	typedef std::deque<string> TCommandHistory;
	TCommandHistory m_CommandHistory;
	int m_HistoryIndex;

	// Interactive prompt.
	//
	// If this is not empty, then this prompt is shown in the command area.  The
	// input thread will wait for one if the response characters.  The response is
	// stored to m_PromptResponse and m_PromptCond is notified.
	string m_Prompt;
	char m_PromptResponseChars[16]; // Null-terminated.
	char m_PromptResponse;
	CryCond<ConsoleLock> m_PromptCond;

	ISystem *m_pSystem;
	IConsole *m_pConsole;
	ITimer *m_pTimer; // Initialized on the first call to OnUpdate().

	// Flag indicating if OnUpdate() has been called.
	//
	// The initialization of the console variable pointers for 'sv_map' and
	// 'sv_gamerules' is deferred until the first iteration of the update loop,
	// because OnInit() is called too early for that.
	bool m_OnUpdateCalled;
	CTimeValue m_LastUpdateTime;

	ICVar *m_svMap;
	ICVar *m_svGameRules;

	// Terminal window layout.
	//
	// The terminal window is split into 4 logical windows.  Top to bottom,
	// these windows are:
	// - Header window.  May be empty (heigth 0).
	// - Log window.  This is the area in the middle of the terminal showing the
	//   log messages.
	// - Status window.  This is a window below the log window showing things
	//   like current FPS or other status information.
	// - Command window.  This is a few lines (typically 1 or 2) at the
	//   bottom of the terminal window.  The command prompt and command line
	//   editor is shown in the command window.
	//
	// The layout is implementated as a single curses window - the standard
	// screen (stdscr).

	// The width and height of the terminal window.
	unsigned m_Width, m_Height;

	// The height of the header window.
	// The header window is displayed at the top of the terminal window.
	// Typically 0 (no header) or 1 (single header line).
	unsigned m_HeaderHeight;

	// The height of the status window.
	// This is typically a single line between the log window and the command
	// window, displayed in inverse video.
	unsigned m_StatusHeight;

	// The heigth of the command window.
	// This is typically a single line at the bottom of the screen.
	unsigned m_CmdHeight;

	// The current text color.
	int m_Color;

	// The default text color pair (read from curses when the app starts).
	int m_DefaultColorPair;

	// Flag indicating if color output is enabled.
	bool m_EnableColor;

	// Flag indicating that the window has been resized.
	// Set by the SIGWINCH signal handler.
	bool m_WindowResized;

	// Flag indicating that OnShutdown() has been called.
	bool m_OnShutdownCalled;

	// Flag indicating if the console has been initialized (i.e. Init() has been
	// called).
	bool m_Initialized;

	// Flag indicating if the implied console initialization (performed by the
	// OnInit() callback) requires a dedicated server.
	// This flag is set through the public SetRequireDedicatedServer() method.
	bool m_RequireDedicatedServer;

	// The number of (logical) lines scrolled up.
	// 0 indicates that we're at the bottom of the log.
	int m_ScrollUp;

	// Array of color pair handles.
	// 0: default terminal color
	// 1: default terminal color, reverse video
	// 2: blue
	// 3: green
	// 4: red, bold font
	// 5: cyan
	// 6: yellow on black, bold font
	// 7: magenta
	// 8: red, normal text
	// 9: black on white
	short m_ColorPair[10];

	// The keyboard input thread.
	CUNIXConsoleInputThread *m_InputThread;

	// The current input line, cursor position, and horizontal scroll position.
	string m_InputLine;
	string m_SavedInputLine;
	int m_CursorPosition;
	int m_ScrollPosition;

	// The current progress status string.
	//
	// Set by the OnInitProgress() method and cleared by OnUpdate().  If this is
	// not empty, then this is shown in the status line.
	string m_ProgressStatus;

	// Set the size of the terminal window.
	//
	// This method is called when the UNIX console is created and whenever the
	// size of the terminal window changes (i.e. SIGWINCH received).
	//
	// We're relying on the ncurses handler for SIGWINCH, so we'll call this
	// method when getch() returns KEY_RESIZE.
	void SetSize(unsigned width, unsigned heigth);

	// Check if the terminal window is too small for drawing.
	bool IsTooSmall();

	// Check if the window size has changed.
	void CheckResize();

	// Get the heigth of the log window.
	unsigned GetLogHeight()
	{
		return m_Height - m_HeaderHeight - m_StatusHeight - m_CmdHeight;
	}

	// Scroll the log window and start a new log line.
	//
	// Move the cursor position to the beginning of the new log line.
	void NewLine();

	// Continue the last log line.
	//
	// Move the cursor position to the first character following the last
	// character logged and update the current color.
	void ContinueLine();

	// Clear the current output line.
	//
	// Move the cursor to the beginning of the current output line.
	//
	// Note: This is a bit fuzzy to implement because long wrapped lines are not
	// easy to deal with.  Instead I'll simply call NewLine() and maybe
	// implement this later.
	void ClearLine() { NewLine(); }

	// Set the output color.
	//
	// The color is one of the 10 color codes (0-9) used by the graphical
	// console.  If color output is enabled, then the corresponding terminal
	// color is set.  If color output is not enabled, then only the text
	// attributes are changed.
	//
	// In addition to setting the color (if enabled), the method will set the
	// following terminal attributes:
	//   0, 1: Normal text (black, white)
	//   4, 6: Bold text (red, yellow, typically indicates an error or warning)
	//   other: Underlined text
	void SetColor(int color = DEFAULT_COLOR);
	void SetColorRaw(int color);

	// Write a single character or a sequence of characters to the console,
	// using the currently specified color.  The specified character must be a
	// printable character.
	//
	// Note:
	// - The Put(const char *) method will interpret '\n' as a line separator an
	//   call NewLine() when encountered.  All other characters must be
	//   printable characters.
	// - Both Put() methods will _not_ update the cursor position before writing
	//   the character to the screen.  It is up to the caller to update the
	//   cursor position (either by calling NewLine() or ContinueLine()).
	void Put(int c);
	void Put(const char *s);

	// Get the length of the line (number of displayed characters), not counting
	// color change escapes.
	static unsigned GetLineLength(const string &line);

	// Get the last printable character from the specified line.  It is an error
	// if the specified line contains no printable characters.  If color is not
	// NULL, then the selected color for the last character is stored to *color.
	static char GetLastCharacter(const string &line, int *color);

	// Get the heigth of a line of text.
	//
	// The method returns the number of terminal lines to display the specified
	// line (wrapped).  If column is not NULL, then *column is set to the column
	// indicating the end of the last wrapped terminal line.
	unsigned GetLineHeight(const string &line, unsigned *column = NULL);

	// Scroll the log window one line.
	void ScrollLog();

	// Flush/repaint the screen.
	void Flush();

	// Called by the input thread when idle.
	void InputIdle();

	// Lock/unlock the UNIX console.
	void Lock() { m_Lock.Lock(); }
	void Unlock() { m_Lock.Unlock(); }

	// Fix the cursor position and scroll position after updating the command
	// input line.  Returns true if the command window must be repainted.
	bool FixCursorPosition();

#if defined(UC_ENABLE_PLAYER_COUNT)
	// Get the number of players connected to the server.
	int GetNumPlayers();
#endif

	// Called when the command line has been edited.
	void OnEdit();

	// Keyboard input.
	void KeyEnter();
	void KeyUp();
	void KeyDown();
	void KeyLeft();
	void KeyRight();
	void KeyHome();
	void KeyEnd();
	void KeyBackspace();
	void KeyDelete();
	void KeyDeleteWord();
	void KeyKill();
	void KeyRepaint();
	void KeyTab();
	void KeyPgUp();
	void KeyPgDown();
	void Key(int c);

	// Drawing.
	void Repaint();
	void DrawHeader();
	unsigned DrawLogLine(const string&, bool noOutput = false);
	void DrawLog();
	void DrawFullscreen();
	void DrawStatus(int maxLines = -1);
	void DrawCmd(bool cursorOnly = false);
	void DrawCmdPrompt();

	CUNIXConsole(const CUNIXConsole&);
	void operator = (const CUNIXConsole&);

public:
	CUNIXConsole();
	~CUNIXConsole();

	// Set or clear the RequireDedicatedServer flag.
	// The implied initialization call performed by the
	// ISystemUserCallback::OnInit() depends on this flag.
	// Note: This method _must_ be called before Init() or OnInit() is called.
	void SetRequireDedicatedServer(bool);

	// Initialize the console for use.
	//
	// This method must be called before any other method of the console is
	// called.
	// It is perfectly valid to instanciate a console and not use it (i.e. skip
	// the Init() call).
	//
	// Note: If the ISystemUserCallback interface is used, then the call to
	// Init() is optional.  OnInit() will call Init() if it has not been called
	// already.
	void Init(const char *headerString = NULL);

	// Check if the console is initialized.
	bool IsInitialized() { return m_Initialized; }

	// Cleanup function.
	// This method is called by the destructor.
	// If the instance has not been initialized (via Init() and/or OnInit()),
	// then this method has no effect.
	void Cleanup();

	// Set the header string.
	// Note:
	// - Setting the header string does _not_ trigger a redraw.
	// - This method may be called before Init() has been called.
	void SetHeader(const char *headerString)
	{
		Lock();
		m_HeaderString = headerString;
		Unlock();
	}

	// Issue a query-response prompt.
	//
	// promptString is the string to be shown as the query prompt.
	// responseChars is a null-terminated string of valid response characters.
	// Add '@' to the response characters if the user may type any character.
	//
	// The method blocks the caller until the user has typed a response.  The
	// return value is the response character typed by the user.
	DLL_EXPORT char Prompt(const char *promptString, const char *responseChars);

	// Check if the calling thread is the input thread.
	//
	// This may be used to make sure that you're not calling Prompt() from the
	// input thread - which will deadlock.
	DLL_EXPORT bool IsInputThread();

	// Print formatted.  Calls Print().
	DLL_EXPORT void PrintF(const char *format, ...) PRINTF_PARAMS(2, 3);

	// Interface IOutputPrintSink /////////////////////////////////////////////
	DLL_EXPORT virtual void Print(const char *line);

	// Interface ISystemUserCallback //////////////////////////////////////////
	virtual bool OnError(const char *errorString);
	virtual void OnSaveDocument() { }
	virtual void OnProcessSwitch() { }
	virtual void OnInitProgress(const char *sProgressMsg);
	virtual void OnInit(ISystem *);
	virtual void OnShutdown();
	virtual void OnUpdate();
	virtual void GetMemoryUsage(ICrySizer *pSizer);

	// Interface ITextModeConsole /////////////////////////////////////////////
	virtual Vec2_tpl<int> BeginDraw();
	virtual void PutText( int x, int y, const char * msg );
	virtual void EndDraw();
};

#endif // USE_UNIXCONSOLE

#endif // _UNIXCONSOLE_H_

// vim:ts=2

