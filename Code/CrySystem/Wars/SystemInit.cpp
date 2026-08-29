//////////////////////////////////////////////////////////////////////
//
//	Crytek Source code (c) Crytek 2001-2004
//
//	File: SystemInit.cpp
//  Description: CryENGINE system core-handle all subsystems
//
//	History:
//	-Feb 09,2004: split from system.cpp
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "System.h"
#include "CryLibrary.h"
#include "CryCommon/CryCore/StringUtils.h"

// CryMP: CopyProtection is not required for the embedded CrySystem build

#if defined(LINUX) && !defined(DEDICATED_SERVER)
#include <dlfcn.h>
#endif

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#include <float.h>
#include <ShlObj.h>
#undef GetUserName

#define  PROFILE_WITH_VTUNE

#endif //WIN32

#include "CryCommon/CryNetwork/INetwork.h"
#include "CryCommon/Cry3DEngine/I3DEngine.h"
#include "CryCommon/CryAISystem/IAISystem.h"
#include "CryCommon/CryRenderer/IRenderer.h"
#include "CryCommon/CrySystem/ICryPak.h"
#include "CryCommon/CryMovie/IMovieSystem.h"
#include "CryCommon/CryEntitySystem/IEntitySystem.h"
#include "CryCommon/CryInput/IInput.h"
#include "CryCommon/CrySystem/ILog.h"
#include "CryCommon/CrySoundSystem/ISound.h"
#include "CryCommon/CryAnimation/ICryAnimation.h"
#include "CryCommon/CryScriptSystem/IScriptSystem.h"
#include "CryCommon/CrySystem/ICmdLine.h"
#include "CryCommon/CrySystem/IProcess.h"

// CryMP: legacy factory typedefs removed from modern CryCommon.
using PFNCREATESOUNDSYSTEM = ISoundSystem* (*)(ISystem*, void*);
using CREATESCRIPTSYSTEM_FNCPTR = IScriptSystem* (*)(ISystem*, bool);

#include "CrySystem/CryPak.h"
#include "XConsole.h"
#include "Log.h"
#include "XML/xml.h"
#include "DataProbe.h"
#include "CrySystem/StreamEngine.h"
#include "BudgetingSystem.h"
#include "PhysRenderer.h"
#include "LocalizedStringManager.h"
#include "../LocalizationManager.h"
#include "SystemEventDispatcher.h"
#include "Statistics.h"
#include "ThreadProfiler.h"
#include "CrySystem/HardwareMouse.h"
#include "CryPhysics/CryPhysics.h"
#include "CryScriptSystem/ScriptSystem.h"
#include "CrySoundSystem/CrySoundSystem.h"
// CryMP: private Scaleform implementation is not part of embedded CrySystem
#include "Validator.h"
#include "ServerThrottle.h"
#include "SystemCFG.h"
#include "AutoDetectSpec.h"
#include "MemoryManager.h"

#include "CryCommon/CryGame/IGame.h"
#include "CryCommon/CryAction/IGameFramework.h"

#include "Library/WinAPI.h"
#include "Launcher/Resources.h"

#if defined(USE_UNIXCONSOLE)
#include "UnixConsole.h"
DLL_EXPORT CUNIXConsole g_UnixConsole;
#if defined(_MSC_VER)
#pragma comment(lib, "pdcurses.lib")
#endif // _MSC_VER
#endif // USE_UNIXCONSOLE

#ifdef WIN32
extern LONG WINAPI CryEngineExceptionFilterWER( struct _EXCEPTION_POINTERS * pExceptionPointers );
#endif


//////////////////////////////////////////////////////////////////////////
#define DEFAULT_LOG_FILENAME "Log.txt"


//////////////////////////////////////////////////////////////////////////
#define CRYENGINE_DEFAULT_LANGUAGE_CONFIG_FILE "Game/Localized/Default.lng"

//////////////////////////////////////////////////////////////////////////
#define CRYENGINE_FOLDERS_CONFIG_FILE "Game/Config/Folders.ini"
 
#if defined (SP_DEMO)
#define CRYENGINE_FOLDER_NAME_USER "UserFolderSPDEMO"
#elif defined (CRYSIS_BETA) 
#define CRYENGINE_FOLDER_NAME_USER "UserFolderMPDEMO"
#else
#define CRYENGINE_FOLDER_NAME_USER "UserFolder"
#endif

//////////////////////////////////////////////////////////////////////////
#if defined(LINUX)
#		define DLL_SOUND				"CrySoundSystem.so"
#		define DLL_NETWORK			"CryNetwork.so"
#		define DLL_ENTITYSYSTEM	"CryEntitySystem.so"
#		define DLL_INPUT				"CryInput.so"
#		define DLL_PHYSICS			"CryPhysics.so"
#		define DLL_MOVIE				"CryMovie.so"
#		define DLL_AI						"CryAISystem.so"
#		define DLL_FONT					"CryFont.so"
#		define DLL_3DENGINE			"Cry3DEngine.so"
#		define DLL_NULLRENDERER	"CryRenderNULL.so"
#else
#	define DLL_SOUND				"CrySoundSystem.dll"
#	define DLL_NETWORK			"CryNetwork.dll"
#	define DLL_ENTITYSYSTEM	"CryEntitySystem.dll"
#	define DLL_INPUT				"CryInput.dll"
#	define DLL_PHYSICS			"CryPhysics.dll"
#	define DLL_MOVIE				"CryMovie.dll"
#	define DLL_AI						"CryAISystem.dll"
#	define DLL_FONT					"CryFont.dll"
#	define DLL_3DENGINE			"Cry3DEngine.dll"
#	define DLL_NULLRENDERER	"CryRenderNULL.dll"
#	ifndef EXCLUDE_GPU_PARTICLE_PHYSICS
#  define DLL_GPU_PHYSICS		"CryPhysicsGPU.dll"
#	endif
#endif

//////////////////////////////////////////////////////////////////////////
#ifdef WIN32
#	define DLL_INITFUNC_RENDERER "PackageRenderConstructor"
#	define DLL_INITFUNC_NETWORK "CreateNetwork"
#	define DLL_INITFUNC_ENTITY "CreateEntitySystem"
#	define DLL_INITFUNC_INPUT "CreateInput"
#	define DLL_INITFUNC_SOUND "CreateSoundSystem"
#	define DLL_INITFUNC_PHYSIC "CreatePhysicalWorld"
#	define DLL_INITFUNC_MOVIE "CreateMovieSystem"
#	define DLL_INITFUNC_AI "CreateAISystem"
#	define DLL_INITFUNC_SCRIPT "CreateScriptSystem"
#	define DLL_INITFUNC_FONT "CreateCryFontInterface"
#	define DLL_INITFUNC_3DENGINE "CreateCry3DEngine"
#	define DLL_INITFUNC_ANIMATION "CreateCharManager"
#	ifndef EXCLUDE_GPU_PARTICLE_PHYSICS
#		define DLL_INITFUNC_GPU_PHYSICS	"CreateGPUPhysics"
#	endif
#else
#	define DLL_INITFUNC_RENDERER  (LPCSTR)1
#	define DLL_INITFUNC_RENDERER  (LPCSTR)1
#	define DLL_INITFUNC_NETWORK   (LPCSTR)1
#	define DLL_INITFUNC_ENTITY    (LPCSTR)1
#	define DLL_INITFUNC_INPUT     (LPCSTR)1
#	define DLL_INITFUNC_SOUND     (LPCSTR)1
#	define DLL_INITFUNC_PHYSIC    (LPCSTR)1
#	define DLL_INITFUNC_MOVIE     (LPCSTR)1
#	define DLL_INITFUNC_AI        (LPCSTR)1
#	define DLL_INITFUNC_SCRIPT    (LPCSTR)1
#	define DLL_INITFUNC_FONT      (LPCSTR)1
#	define DLL_INITFUNC_3DENGINE  (LPCSTR)1
#	define DLL_INITFUNC_ANIMATION (LPCSTR)1
#	ifndef EXCLUDE_GPU_PARTICLE_PHYSICS
#		define DLL_INITFUNC_GPU_PHYSICS (LPCSTR)1
#	endif
#endif

//////////////////////////////////////////////////////////////////////////
// Extern declarations for static libraries.
//////////////////////////////////////////////////////////////////////////
#if defined(_LIB) || defined(LINUX) || defined(PS3)
extern "C"
{
	IRenderer* PackageRenderConstructor(int argc, char* argv[], SCryRenderInterface *sp);
	/*IRenderer* PackageRenderConstructor(int argc, char* argv[], SCryRenderInterface *sp)
	{
		// test
		return 0;
	}*/

	IAISystem* CreateAISystem( ISystem *pSystem );
	IMovieSystem* CreateMovieSystem( ISystem *pSystem );
}
#endif //_LIB
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// PS3 PRX initialization code.
//////////////////////////////////////////////////////////////////////////
#if defined PS3 && !defined _LIB
namespace PS3Init
{
  IRenderer *Renderer(int argc, char **argv, SCryRenderInterface *pIf)
	{
#if defined PS3_PRX_Renderer
		return PackageRenderConstructor(argc, argv, pIf);
#else
		return gPS3Env->pInitFnTable->pInitRenderer(argc, argv, pIf);
#endif
	}

	INetwork *CryNetwork(ISystem *pSystem, int ncpu)
	{
#if defined PS3_PRX_CryNetwork
		return CreateNetwork(pSystem, ncpu);
#else
		return gPS3Env->pInitFnTable->pInitNetwork(pSystem, ncpu);
#endif
	}

	IEntitySystem *CryEntitySystem(ISystem *pSystem)
	{
#if defined PS3_PRX_CryEntitySystem
		return CreateEntitySystem(pSystem);
#else
		return gPS3Env->pInitFnTable->pInitEntitySystem(pSystem);
#endif
	}

	IInput *CryInput(ISystem *pSystem, void *hwnd)
	{
#if defined PS3_PRX_CryInput
		return CreateInput(pSystem, hwnd);
#else
		return gPS3Env->pInitFnTable->pInitInput(pSystem, hwnd);
#endif
	}

	ISoundSystem *CrySoundSystem(ISystem *pSystem, void *pInitData)
	{
#if defined PS3_PRX_CrySoundSystem
		return CreateSoundSystem(pSystem, pInitData);
#else
		return gPS3Env->pInitFnTable->pInitSoundSystem(pSystem, pInitData);
#endif
	}

	IPhysicalWorld *CryPhysics(ISystem *pSystem)
	{
#if defined PS3_PRX_CryPhysics
		return CreatePhysicalWorld(pSystem);
#else
		return gPS3Env->pInitFnTable->pInitPhysics(pSystem);
#endif
	}

	IMovieSystem *CryMovieSystem(ISystem *pSystem)
	{
#if defined PS3_PRX_CryMovie
		return CreateMovieSystem(pSystem);
#else
		return gPS3Env->pInitFnTable->pInitMovieSystem(pSystem);
#endif
	}

	IAISystem *CryAISystem(ISystem *pSystem)
	{
#if defined PS3_PRX_CryAISystem
		return CreateAISystem(pSystem);
#else
		return gPS3Env->pInitFnTable->pInitAISystem(pSystem);
#endif
	}

	IScriptSystem *CryScriptSystem(ISystem *pSystem, bool bStdLibs)
	{
#if defined PS3_PRX_CryScriptSystem
		return CreateScriptSystem(pSystem, bStdLibs);
#else
		return gPS3Env->pInitFnTable->pInitScriptSystem(pSystem, bStdLibs);
#endif
	}

	ICryFont *CryFont(ISystem *pSystem)
	{
#if defined PS3_PRX_CryFont
		return CreateCryFontInterface(pSystem);
#else
		return gPS3Env->pInitFnTable->pInitFont(pSystem);
#endif
	}

	I3DEngine *Cry3DEngine(ISystem *pSystem, const char *szInterfaceVersion)
	{
#if defined PS3_PRX_Cry3DEngine
		return CreateCry3DEngine(pSystem, szInterfaceVersion);
#else
		return gPS3Env->pInitFnTable->pInit3DEngine(pSystem, szInterfaceVersion);
#endif
	}

	ICharacterManager *CryAnimation(
			ISystem *pSystem,
			const char *szInterfaceVersion
			)
	{
#if defined PS3_PRX_CryAnimation
		return CreateCharManager(pSystem, szInterfaceVersion);
#else
		return gPS3Env->pInitFnTable->pInitAnimation(pSystem, szInterfaceVersion);
#endif
	}
}
#endif // PS3 && !_LIB

//static int g_sysSpecChanged = false;

const char *g_szLvlResExt="_LvlRes.txt";

//////////////////////////////////////////////////////////////////////////
static void OnSysSpecChange( ICVar *pVar )
{
//	g_sysSpecChanged = true;
	static int no_recursive = false;
	if (no_recursive)
		return;
	no_recursive = true;
	// Called when sys_spec (client config spec) variable changes.
	int spec = pVar->GetIVal();

	if (spec > ((CSystem*)gEnv->pSystem)->GetMaxConfigSpec())
	{
		spec = ((CSystem*)gEnv->pSystem)->GetMaxConfigSpec();
		pVar->Set(spec);
	}

	CryLog("OnSysSpecChange(%d)",spec);

	switch (spec)
	{
	case CONFIG_LOW_SPEC:
		GetISystem()->LoadConfiguration( "LowSpec.cfg" );
		break;
	case CONFIG_MEDIUM_SPEC:
		GetISystem()->LoadConfiguration( "MedSpec.cfg" );
		break;
	case CONFIG_HIGH_SPEC:
		GetISystem()->LoadConfiguration( "HighSpec.cfg" );
		break;
	case CONFIG_VERYHIGH_SPEC:
		GetISystem()->LoadConfiguration( "VeryHighSpec.cfg" );
		break;
	default:
		// Do nothing.
		break;
	}
	bool bChangeServerSpec = true;
	if (gEnv->pGame && gEnv->bMultiplayer)
		bChangeServerSpec = false;
	if (bChangeServerSpec)
		GetISystem()->SetConfigSpec( (ESystemConfigSpec)spec,false );

	no_recursive = false;
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct SCryEngineFoldersLoader : public ILoadConfigurationEntrySink
{
	CSystem *m_pSystem;
	SCryEngineFoldersLoader( CSystem *pSystem ) { m_pSystem = pSystem; }
	void Load( const char *sCfgFilename )
	{
		CSystemConfiguration cfg(sCfgFilename,m_pSystem,this); // Parse folders config file.
	}
	virtual void OnLoadConfigurationEntry( const char *szKey, const char *szValue, const char *szGroup )
	{
		string path = PathUtil::ToUnixPath(szValue);
		path = PathUtil::RemoveSlash(path);
		if (stricmp(szKey,CRYENGINE_FOLDER_NAME_USER) == 0)
		{
			m_pSystem->m_engineFolders[ENGINE_FOLDER_USER] = path;
		}
	}
	virtual void OnLoadConfigurationEntry_End() {}
};

//////////////////////////////////////////////////////////////////////////
struct SCryEngineLanguageConfigLoader : public ILoadConfigurationEntrySink
{
	CSystem *m_pSystem;
	string m_language;
	string m_pakFile;

	SCryEngineLanguageConfigLoader( CSystem *pSystem ) { m_pSystem = pSystem; }
	void Load( const char *sCfgFilename )
	{
		CSystemConfiguration cfg(sCfgFilename,m_pSystem,this); // Parse folders config file.
	}
	virtual void OnLoadConfigurationEntry( const char *szKey, const char *szValue, const char *szGroup )
	{
		if (stricmp(szKey,"Language") == 0)
		{
			m_language = szValue;
		} else if (stricmp(szKey,"PAK") == 0)
		{
			m_pakFile = szValue;
		}
	}
	virtual void OnLoadConfigurationEntry_End() {}
};

//////////////////////////////////////////////////////////////////////////
bool CSystem::OpenRenderLibrary(const char *t_rend)
{
	#ifdef _XBOX
		return OpenRenderLibrary(R_DX9_RENDERER);
	#endif

  int nRenderer = R_DX9_RENDERER;
	if (IsDedicated())
		return OpenRenderLibrary(R_NULL_RENDERER);

	if (stricmp(t_rend, "OpenGL") == 0)
    return OpenRenderLibrary(R_GL_RENDERER);
  else
  if (stricmp(t_rend, "DX9") == 0)
	{
		/*	reenable these two lines (and ones below - search for CONFIG_HIGH_SPEC)
				to disable very high spec for dx9
		if (GetConfigSpec() > CONFIG_HIGH_SPEC)
			SetConfigSpec(CONFIG_HIGH_SPEC, true);
		*/
    return OpenRenderLibrary(R_DX9_RENDERER);
	}
	else
//	if (stricmp(t_rend, "DX9+") == 0)							// required to be deactivated for final release
//		return OpenRenderLibrary(R_DX9_RENDERER);
//  else
  if (stricmp(t_rend, "DX10") == 0)
    return OpenRenderLibrary(R_DX10_RENDERER);
  else
  if (stricmp(t_rend, "NULL") == 0)
    return OpenRenderLibrary(R_NULL_RENDERER);

	Error ("Unknown renderer type: %s", t_rend);
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

#if defined(WIN32) || defined(WIN64)
wstring CSystem::GetErrorStringUnsupportedGPU(const char* gpuName, unsigned int gpuVendorId, unsigned int gpuDeviceId)
{
	ICVar* pCVarGLang(m_env.pConsole->GetCVar("g_language"));

	string lang;
	if (pCVarGLang)
		lang = pCVarGLang->GetString();

	const wchar_t* unsupportedGPU = nullptr;
	const wchar_t* vendor = nullptr;
	const wchar_t* device = nullptr;
	const wchar_t* hint = nullptr;

	wchar_t* pFmtMsgBuf(0);

	if (stricmp(lang.c_str(), "english") == 0)
	{
		unsupportedGPU = L"Unsupported video card!\n\n";
		vendor = L"vendor = ";
		device = L"device = ";
		hint = L"Continuing to run might lead to unexpected results or crashes.";
	}
	else
	{		
		FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, 0, E_NOINTERFACE, 0, (wchar_t*) &pFmtMsgBuf, 0, 0);
		unsupportedGPU = L"";
		vendor = L"";
		device = L"";
		hint = pFmtMsgBuf;
	}

	wchar_t msg[1024]; msg[0] = L'\0'; msg[sizeof(msg) / sizeof(msg[0]) - 1] = L'\0';
	_snwprintf(msg, sizeof(msg) / sizeof(msg[0]) - 1, L"%s\"%S\" [%s0x%.4x, %s0x%.4x]\n\n%s", unsupportedGPU, gpuName, vendor, gpuVendorId, device, gpuDeviceId, hint);

	if (pFmtMsgBuf)
		LocalFree(pFmtMsgBuf);

	return msg;
}
#endif

bool CSystem::OpenRenderLibrary(int type)
{
  SCryRenderInterface sp;

#if defined(WIN32) || defined(WIN64)
	if (type == R_DX9_RENDERER && !m_env.bEditor)
	{
		unsigned int gpuVendorId(0), gpuDeviceId(0), totVidMem(0);
		bool supportsSM20orAbove(false);
		char gpuName[256];
		Win32SysInspect::GetGPUInfo(gpuName, sizeof(gpuName), gpuVendorId, gpuDeviceId, totVidMem, supportsSM20orAbove);
		int gpuRating(Win32SysInspect::GetGPURating(gpuVendorId, gpuDeviceId));
		if (gpuRating < 0 || !supportsSM20orAbove)
		{
			int mbRes(MessageBoxW(0, GetErrorStringUnsupportedGPU(gpuName, gpuVendorId, gpuDeviceId).c_str(), L"CryEngine2", MB_ICONWARNING | MB_OKCANCEL | MB_DEFBUTTON2));
			const char logMsgFmt[]("Unsupported video card!\n- %s (vendor = 0x%.4x, device = 0x%.4x)\n- Video memory: %d MB\n- Minimum SM 2.0 support: %s\n- Rating: %d\n%s");
			CryLogAlways(logMsgFmt, gpuName, gpuVendorId, gpuDeviceId, totVidMem >> 20, (supportsSM20orAbove == true) ? "yes" : "no", gpuRating, mbRes == IDCANCEL ? "User chose to quit!" : "User chose to continue!");
			if (mbRes == IDCANCEL)
				return false;
		}
	}
#endif

#if defined(_XBOX) || (defined(LINUX) && !defined(DEDICATED_SERVER))
  type = R_DX9_RENDERER;
#endif

#if defined(DEDICATED_SERVER)
	type = R_NULL_RENDERER;
#else
	if (IsDedicated())
		type = R_NULL_RENDERER;
#endif

  sp.ipConsole = GetIConsole();
  sp.ipLog = GetILog();
  sp.ipSystem = this;
  sp.ipTimer = GetITimer();

#if !defined(_LIB) && !defined(LINUX) && !defined(PS3)
	char libname[128];
	if (type == R_GL_RENDERER)
    strcpy(libname, "CryRenderOGL.dll");
	else
  if (type == R_DX9_RENDERER)
    strcpy(libname, "CryRenderD3D9.dll");
  else
  if (type == R_DX10_RENDERER)
    strcpy(libname, "CryRenderD3D10.dll");
  else
  if (type == R_NULL_RENDERER)
    strcpy(libname, DLL_NULLRENDERER);
	else
	{
		Error("No renderer specified");
		return (false);
	}
	m_dll.hRenderer = LoadDLL(libname);
	if (!m_dll.hRenderer)
		return false;

	typedef IRenderer *(PROCREND)(int argc, char* argv[], SCryRenderInterface *sp);
  PROCREND *Proc = (PROCREND *) CryGetProcAddress(m_dll.hRenderer, DLL_INITFUNC_RENDERER);
	if (!Proc)
	{
		Error( "Error: Library '%s' isn't Crytek render library", libname);
		FreeLib(m_dll.hRenderer);
		return false;
	}
 
	m_env.pRenderer = Proc(0, NULL, &sp);
	if (!m_env.pRenderer)
	{
		Error( "Error: Couldn't construct render driver '%s'", libname);
		FreeLib(m_dll.hRenderer);
		return false;
	}
#elif defined(LINUX) && !defined(DEDICATED_SERVER)
	// TODO: Maybe all the Linux specific library loading code should be moved
	// to a platform specific place. (Where?)
	char libname[128];
	if (type == R_DX9_RENDERER)
		strcpy(libname, "cryrenderd3d9");
	else
	if (type == R_NULL_RENDERER)
		strcpy(libname, "cryrendernull");
	else
	{
		Error("Unknown renderer type %i", type);
		return false;
	}
#if defined(_DEBUG)
	strcat(libname, "_debug");
#endif
	strcat(libname, ".so");
	string libnameAbs;
	bool renderLibFound = false;
	// We'll have to look around a bit to find the renderer library.  Then we'll
	// try to locate it in the directory containing the executable.
	// (Note: "filename" is a magic automatic parameter holding the name of the
	// executable.)
	const ICmdLineArg *const argExec = m_pCmdLine->FindArg(eCLAT_Executable,"filename");
	assert(argExec != NULL);
	const char *execName = argExec->GetValue();
	assert(execName != NULL);
	char execNameBuf[strlen(execName) + 1];
	strcpy(execNameBuf, execName);
	char *p = execNameBuf;
	for (; *p; ++p)
		if (*p == '\\') *p = '/';
	while (p > execNameBuf && p[-1] == '/') --p;
	*p = 0;
	p = strrchr(execNameBuf, '/');
	if (p) *p = 0;
	const char *execDirName = p ? execNameBuf : NULL;
	// We'll look in the executable directory only if we were called with a path
	// prefix. If the program was found in the path, then we don't go through
	// the pains of scanning the path for the program.
	if (execDirName)
	{
		libnameAbs.Format("%s/%s", execDirName, libname);
		if (access(libnameAbs.c_str(), F_OK) == 0)
			renderLibFound = true;
		else
			libnameAbs.clear();
	}
	if (!renderLibFound)
	{
		// Add more checks here.
	}
	if (!renderLibFound)
	{
		// If we can't find the renderlib, then our last bet is that the lib is
		// somewhere in the library path.
		libnameAbs = libname;
	}
	void *handle = dlopen(libnameAbs.c_str(), RTLD_LAZY | RTLD_LOCAL);
	if (handle == NULL)
	{
		Error("Failed to load renderer library '%s': %s", libname, dlerror());
		return false;
	}
	typedef IRenderer *fnType(int, char **, SCryRenderInterface *);
	fnType *constructor = reinterpret_cast<fnType *>(dlsym(
				handle,
				"PackageRenderConstructor"));
	if (constructor == NULL)
	{
		Error("Error: Library '%s' isn't Crytek render library", libnameAbs.c_str());
		dlclose(handle);
		return false;
	}
	m_env.pRenderer = (*constructor)(0, NULL, &sp);
	if (m_env.pRenderer == NULL)
	{
		Error("Error: Couldn't construct render driver '%s'", libname);
		dlclose(handle);
		return false;
	}
#elif defined PS3 && !defined _LIB
	m_env.pRenderer = PS3Init::Renderer(0, NULL, &sp);
#else
  m_env.pRenderer = (IRenderer*)PackageRenderConstructor(0, NULL, &sp);
#endif

	return true;
}


/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitNetwork()
{
	if (m_pUserCallback)
		m_pUserCallback->OnInitProgress( "Initializing Network System..." );

#if !defined(_LIB) && !defined(LINUX) && !defined(PS3)
	PFNCREATENETWORK pfnCreateNetwork;
	m_dll.hNetwork = LoadDLL( DLL_NETWORK );
	if (!m_dll.hNetwork)
		return false;

	pfnCreateNetwork=(PFNCREATENETWORK) CryGetProcAddress(m_dll.hNetwork,DLL_INITFUNC_NETWORK);
	m_env.pNetwork=pfnCreateNetwork(this, m_pCpu->GetCPUCount());
#elif defined PS3 && !defined _LIB
  m_env.pNetwork=PS3Init::CryNetwork(this, m_pCpu->GetCPUCount());
#else //_LIB
	m_env.pNetwork=CreateNetwork(this, m_pCpu->GetCPUCount());
#endif //_LIB
	if(m_env.pNetwork==NULL)
	{
		Error( "Error creating Network System (CreateNetwork) !" );
		return false;
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitEntitySystem(WIN_HINSTANCE hInstance, WIN_HWND hWnd)
{
	if (m_pUserCallback)
		m_pUserCallback->OnInitProgress( "Initializing Entity System..." );

	/////////////////////////////////////////////////////////////////////////////////
	// Load and initialize the entity system
	/////////////////////////////////////////////////////////////////////////////////
#if !defined(_LIB) && !defined(LINUX) && !defined(PS3)
	PFNCREATEENTITYSYSTEM pfnCreateEntitySystem;

	// Load the DLL
	m_dll.hEntitySystem = LoadDLL( DLL_ENTITYSYSTEM );
	if (!m_dll.hEntitySystem)
		return false;

	// Obtain factory pointer
	pfnCreateEntitySystem = (PFNCREATEENTITYSYSTEM) CryGetProcAddress( m_dll.hEntitySystem, DLL_INITFUNC_ENTITY);

	if (!pfnCreateEntitySystem)
	{
		Error( "Error querying entry point of Entity System Module (CryEntitySystem.dll) !");
		return false;
	}
	// Create the object
	m_env.pEntitySystem = pfnCreateEntitySystem(this);
#elif defined PS3 && !defined _LIB
	m_env.pEntitySystem = PS3Init::CryEntitySystem(this);
#else //_LIB
	// Create the object
	m_env.pEntitySystem = CreateEntitySystem(this);
#endif //_LIB
	if (!m_env.pEntitySystem)
	{
		Error( "Error creating Entity System");
		return false;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitInput(WIN_HWND hwnd)
{
	if (m_pUserCallback)
		m_pUserCallback->OnInitProgress( "Initializing Input System..." );

#if !defined(_LIB) && !defined(LINUX) && !defined(PS3)
	m_dll.hInput = LoadDLL(DLL_INPUT, false);
	if (!m_dll.hInput)
	{
#ifdef WIN32
		MessageBox( NULL,"CryInput.dll could not be loaded. This is likely due to not having XInput support installed.\nPlease install the most recent version of the DirectX runtime." ,"ERROR: CryInput.dll could not be loaded!",MB_OK|MB_ICONERROR );
#endif
		return false;
	}

	CRY_PTRCREATEINPUTFNC *pfnCreateInput = (CRY_PTRCREATEINPUTFNC *) CryGetProcAddress(m_dll.hInput, DLL_INITFUNC_INPUT);
	if (pfnCreateInput)
		m_env.pInput = pfnCreateInput(this, m_hWnd);
#elif defined PS3 && !defined _LIB
	m_env.pInput = PS3Init::CryInput(this, NULL);
#else //_LIB
	m_env.pInput = CreateInput(this, NULL);
#endif //_LIB
	if (!m_env.pInput)
	{
		Error( "Error creating Input system" );
		return false;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitConsole()
{
//	m_Console->Init(this);
	// Ignore when run in Editor.
	if (m_bEditor && !m_env.pRenderer)
		return true;

	// Ignore for dedicated server.
	if (IsDedicated())
		return true;

/*	char *filename = "Textures/Defaults/DefaultConsole.dds"; // this path is set by Harry
	if (filename)
	{
		ITexture * conimage = m_env.pRenderer->EF_LoadTexture(filename, FT_DONT_RESIZE, eTT_2D);
		if (conimage)
  		m_env.pConsole->SetImage(conimage,false);
	}
	else
	{
		Error("Error: Cannot open %s", filename);
	}*/

	return (true);
}

//////////////////////////////////////////////////////////////////////////
// attaches the given variable to the given container;
// recreates the variable if necessary
ICVar* CSystem::attachVariable (const char* szVarName, int* pContainer, const char*szComment,int dwFlags)
{
	IConsole* pConsole = GetIConsole();

	ICVar* pOldVar = pConsole->GetCVar (szVarName);
	int nDefault;
	if (pOldVar)
	{
		nDefault = pOldVar->GetIVal();
		pConsole->UnregisterVariable(szVarName, true);
	}

	// NOTE: maybe we should preserve the actual value of the variable across the registration,
	// because of the strange architecture of IConsole that converts int->float->int

	pConsole->Register(szVarName, pContainer, *pContainer, dwFlags, szComment);

	ICVar* pVar = pConsole->GetCVar(szVarName);

#ifdef _DEBUG
	// test if the variable really has this container
	assert (*pContainer == pVar->GetIVal());
	++*pContainer;
	assert (*pContainer == pVar->GetIVal());
	--*pContainer;
#endif

	if (pOldVar)
	{
		// carry on the default value from the old variable anyway
		pVar->Set(nDefault);
	}
	return pVar;
}

/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitRenderer(WIN_HINSTANCE hinst, WIN_HWND hwnd,const char *szCmdLine)
{
	if (m_pUserCallback)
		m_pUserCallback->OnInitProgress( "Initializing Renderer..." );

	if (m_bEditor)
	{
		// save current screen width/height/bpp, so they can be restored on shutdown
		m_iWidth = m_env.pConsole->GetCVar("r_Width")->GetIVal();
		m_iHeight = m_env.pConsole->GetCVar("r_Height")->GetIVal();
		m_iColorBits = m_env.pConsole->GetCVar("r_ColorBits")->GetIVal();
	}

	if (!OpenRenderLibrary(m_rDriver->GetString()))
		return false;

#ifdef WIN32

	if(!m_bDedicatedServer)
	{
		// [marco] If a previous instance is running, activate
		// the old one and terminate the new one, depending
		// on command line devmode status - is done here for
		// smart people that double-click 20 times on the game
		// icon even before the previous detection on the main
		// executable has time to be executed - so now the game will quit
		HWND hwndPrev;
		static char szWndClass[] = "CryENGINE";
		
		// in devmode we don't care, we allow to run multiple instances
		// for mp debugging
		if (!m_bInDevMode)
		{
			hwndPrev = FindWindow (szWndClass, NULL);
			// not in devmode and we found another window - see if the
			// system is relaunching, in this case is fine 'cos the application
			// will be closed immediately after
			if (hwndPrev && (hwndPrev!=m_hWnd) && !m_bRelaunched)
			{
				SetForegroundWindow (hwndPrev);
				MessageBox( NULL,"You cannot start multiple instances of Crysis Wars!\nThe program will now quit.\n" ,"ERROR: STARTING MULTIPLE INSTANCES OF CRYSIS ",MB_OK|MB_ICONERROR );
				Quit();
			}
		}
	}
#endif

#ifdef WIN32
	if (m_env.pRenderer)
	{
		if(m_env.pHardwareMouse)
			m_env.pHardwareMouse->OnPreInitRenderer();
		
		SCustomRenderInitArgs args;
		args.appStartedFromMediaCenter = strstr(szCmdLine, "ReLaunchMediaCenter") != 0;

		m_hWnd = m_env.pRenderer->Init(0, 0, m_rWidth->GetIVal(), m_rHeight->GetIVal(), m_rColorBits->GetIVal(), m_rDepthBits->GetIVal(), m_rStencilBits->GetIVal(), m_rFullscreen->GetIVal() ? true : false, hinst, hwnd, false, &args);		
		return m_hWnd != 0;
	}
#else
	if (m_env.pRenderer)
	{
		WIN_HWND h = m_env.pRenderer->Init(0, 0, m_rWidth->GetIVal(), m_rHeight->GetIVal(), m_rColorBits->GetIVal(), m_rDepthBits->GetIVal(), m_rStencilBits->GetIVal(), m_rFullscreen->GetIVal() ? true : false, hinst, hwnd);
#if defined(_XBOX) || defined(XENON) || defined(PS3) || defined(LINUX)
		return true;
#else
		if (h)
			return true;
		return (false);
#endif 
	}
#endif
	return true;
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitSound(WIN_HWND hwnd)
{
	if (m_pUserCallback)
		m_pUserCallback->OnInitProgress( "Initializing Sound System..." );

#if !defined(LINUX)
#if defined(CRYMP_EMBEDDED_CRYSYSTEM) && !defined(PS3)
	// CryMP: the old launcher patched stock CrySystem.dll so this factory was used.
	// With CrySystem embedded there is no DLL to patch, so call it directly.
	const bool useOldSound = GetICmdLine() && GetICmdLine()->FindArg(eCLAT_Pre, "oldsound");
	if (!useOldSound)
	{
		CryLogAlways("$3[CryMP] Initializing Sound System");
		m_env.pSoundSystem = CreateSoundSystem(this, hwnd);
	}
	else
	{
		CryLogAlways("$3[CryMP] Using stock CrySoundSystem (-oldsound)");
		m_dll.hSound = LoadDLL(DLL_SOUND, false);
		if (!m_dll.hSound)
		{
			CryLogAlways("Failed to load CrySoundSystem.DLL!");
			return false;
		}

		PFNCREATESOUNDSYSTEM pfnCreateSoundSystem =
			(PFNCREATESOUNDSYSTEM)CryGetProcAddress(m_dll.hSound, DLL_INITFUNC_SOUND);
		if (!pfnCreateSoundSystem)
		{
			Error("Error loading function CreateSoundSystem");
			return false;
		}

		m_env.pSoundSystem = pfnCreateSoundSystem(this, hwnd);
	}
#elif !defined(_LIB) && !defined(LINUX) && !defined(PS3)
	m_dll.hSound = LoadDLL(DLL_SOUND, false);

	if(!m_dll.hSound)
	{
		CryLogAlways("Failed to load CrySoundSystem.DLL!");
		return false;
	}

	m_env.pSoundSystem = NULL;
	PFNCREATESOUNDSYSTEM pfnCreateSoundSystem = (PFNCREATESOUNDSYSTEM) CryGetProcAddress(m_dll.hSound,DLL_INITFUNC_SOUND);
	if (!pfnCreateSoundSystem)
	{
		Error( "Error loading function CreateSoundSystem");
		return false;
	}

	m_env.pSoundSystem = pfnCreateSoundSystem(this,hwnd);
#elif defined PS3 && !defined _LIB
	m_env.pSoundSystem = PS3Init::CrySoundSystem(this,hwnd);
#else //_LIB
  m_env.pSoundSystem = CreateSoundSystem(this,hwnd);
#endif //_LIB
	if (!m_env.pSoundSystem)
	{
		Error( "Error creating the sound system interface");
		return false;
	}
	m_env.pMusicSystem = m_env.pSoundSystem->CreateMusicSystem();
	if (!m_env.pMusicSystem)
	{
		Error( "Error creating the music system interface");
		return false; 
	}
	
#endif //_LIB
	return true;
}

/////////////////////////////////////////////////////////////////////////////////

char *PhysHelpersToStr(int iHelpers, char *strHelpers)
{
	char *ptr = strHelpers;
	if (iHelpers & 128) *ptr++ = 't';
	if (iHelpers & 256) *ptr++ = 's';
	if (iHelpers & 512) *ptr++ = 'r';
	if (iHelpers & 1024) *ptr++ = 'R';
	if (iHelpers & 2048) *ptr++ = 'l';
	if (iHelpers & 4096) *ptr++ = 'i';
	if (iHelpers & 8192) *ptr++ = 'e';
	if (iHelpers & 16384) *ptr++ = 'g';
	if (iHelpers & 32768) *ptr++ = 'w';
	if (iHelpers & 32) *ptr++ = 'a';
	if (iHelpers & 64) *ptr++ = 'y';
	*ptr++ = iHelpers ? '_' : '0';
	if (iHelpers & 1) *ptr++ = 'c';
	if (iHelpers & 2) *ptr++ = 'g';
	if (iHelpers & 4) *ptr++ = 'b';
	if (iHelpers & 8) *ptr++ = 'l';
	if (iHelpers & 16) *ptr++ = 'j';
	if (iHelpers>>16) 
		if (!(iHelpers & 1<<27))
			ptr += sprintf(ptr,"t(%d)",iHelpers>>16);
		else for(int i=0;i<16;i++) if (i!=11 && iHelpers & 1<<(16+i))
			ptr += sprintf(ptr,"f(%d)",i);
	*ptr++ = 0;
	return strHelpers;
}

int StrToPhysHelpers(const char *strHelpers)
{
	const char *ptr;
	int iHelpers=0,level;
	if (*strHelpers=='1')
		return 7970;
	if (*strHelpers=='2')
		return 7970 | 1<<31 | 1<<27;
	for(ptr=strHelpers; *ptr && *ptr!='_'; ptr++) switch(*ptr) 
	{
		case 't': iHelpers |= 128; break;
		case 's': iHelpers |= 256; break;
		case 'r': iHelpers |= 512; break;
		case 'R': iHelpers |= 1024; break;
		case 'l': iHelpers |= 2048; break;
		case 'i': iHelpers |= 4096; break;
		case 'e': iHelpers |= 8192; break;
		case 'g': iHelpers |= 16384; break;
		case 'w': iHelpers |= 32768; break;
		case 'a': iHelpers |= 32; break;
		case 'y': iHelpers |= 64; break;
	}
	if (*ptr=='_') ptr++;
	for(; *ptr; ptr++) switch(*ptr)
	{
		case 'c': iHelpers |= 1; break;
		case 'g': iHelpers |= 2; break;
		case 'b': iHelpers |= 4; break;
		case 'l': iHelpers |= 8; break;
		case 'j': iHelpers |= 16; break;
		case 'f': 
			if (*++ptr && *++ptr) for(level=0; *(ptr+1) && *ptr!=')'; ptr++) level = level*10+*ptr-'0'; 
			iHelpers |= 1<<(16+level) | 1<<27; break;
		case 't': 
			if (*++ptr && *++ptr) for(level=0; *(ptr+1) && *ptr!=')'; ptr++) level = level*10+*ptr-'0'; 
			iHelpers |= level<<16 | 2; 
	}
	return iHelpers;
}

void OnDrawHelpersStrChange(ICVar *pVar)
{
	gEnv->pPhysicalWorld->GetPhysVars()->iDrawHelpers = StrToPhysHelpers(pVar->GetString());
}

bool CSystem::InitPhysics()
{
	if (m_pUserCallback)
		m_pUserCallback->OnInitProgress( "Initializing Physics..." );

#if defined(CRYMP_EMBEDDED_CRYSYSTEM) && !defined(LINUX) && !defined(PS3)
	// CryMP: preserve the launcher's -oldphysics fallback, but use our source-built
	// physics implementation by default now that CrySystem.dll is gone.
	const bool useOldPhysics = GetICmdLine() && GetICmdLine()->FindArg(eCLAT_Pre, "oldphysics");
	if (!useOldPhysics)
	{
		CryLogAlways("$3[CryMP] Initializing Physics");
		m_env.pPhysicalWorld = CreatePhysicalWorld(this);
	}
	else
	{
		CryLogAlways("$3[CryMP] Using stock CryPhysics (-oldphysics)");
		m_dll.hPhysics = LoadDLL(DLL_PHYSICS);
		if (!m_dll.hPhysics)
			return false;

		IPhysicalWorld *(*pfnCreatePhysicalWorld)(ISystem *pSystem) =
			(IPhysicalWorld*(*)(ISystem*))CryGetProcAddress(m_dll.hPhysics, DLL_INITFUNC_PHYSIC);
		if (!pfnCreatePhysicalWorld)
		{
			Error("Error loading function CreatePhysicalWorld");
			return false;
		}

		m_env.pPhysicalWorld = pfnCreatePhysicalWorld(this);
	}
#elif !defined(_LIB) && !defined(LINUX) && !defined(PS3)
	m_dll.hPhysics = LoadDLL(DLL_PHYSICS);
	if(!m_dll.hPhysics)
		return false;

	IPhysicalWorld *(*pfnCreatePhysicalWorld)(ISystem *pSystem) = (IPhysicalWorld*(*)(ISystem*)) CryGetProcAddress(m_dll.hPhysics,DLL_INITFUNC_PHYSIC);
	if(!pfnCreatePhysicalWorld)
	{
		Error( "Error loading function CreatePhysicalWorld" );
		return false;
	}

	m_env.pPhysicalWorld = pfnCreatePhysicalWorld(this);
#elif defined PS3 && !defined _LIB
	m_env.pPhysicalWorld = PS3Init::CryPhysics(this);
#else //_LIB
	m_env.pPhysicalWorld = CreatePhysicalWorld(this);
#endif //_LIB

	if(!m_env.pPhysicalWorld)
	{
		Error( "Error creating the physics system interface" );
		return false;
	}
	m_env.pPhysicalWorld->Init();

	// Register physics console variables.
	IConsole *pConsole = GetIConsole();

  PhysicsVars *pVars = m_env.pPhysicalWorld->GetPhysVars();

  pConsole->Register("p_fly_mode", &pVars->bFlyMode, pVars->bFlyMode,VF_CHEAT,
		"Toggles fly mode.\n"
		"Usage: p_fly_mode [0/1]");
  pConsole->Register("p_collision_mode", &pVars->iCollisionMode, pVars->iCollisionMode,VF_CHEAT,
		"This variable is obsolete.\n");
  pConsole->Register("p_single_step_mode", &pVars->bSingleStepMode, pVars->bSingleStepMode,VF_CHEAT,
		"Toggles physics system 'single step' mode."
		"Usage: p_single_step_mode [0/1]\n"
		"Default is 0 (off). Set to 1 to switch physics system (except\n"
		"players) to single step mode. Each step must be explicitly\n"
		"requested with a 'p_do_step' instruction.");
  pConsole->Register("p_do_step", &pVars->bDoStep, pVars->bDoStep,VF_CHEAT,
		"Steps physics system forward when in single step mode.\n"
		"Usage: p_do_step 1\n"
		"Default is 0 (off). Each 'p_do_step 1' instruction allows\n"
		"the physics system to advance a single step.");
  pConsole->Register("p_fixed_timestep", &pVars->fixedTimestep, pVars->fixedTimestep,VF_CHEAT,
		"Toggles fixed time step mode."
		"Usage: p_fixed_timestep [0/1]\n"
		"Forces fixed time step when set to 1. When set to 0, the\n"
		"time step is variable, based on the frame rate.");
  pConsole->Register("p_draw_helpers_num", &pVars->iDrawHelpers, pVars->iDrawHelpers,VF_CHEAT,
		"Toggles display of various physical helpers. The value is a bitmask:\n"
		"bit 0  - show contact points\n"
		"bit 1  - show physical geometry\n"
		"bit 8  - show helpers for static objects\n"
		"bit 9  - show helpers for sleeping physicalized objects (rigid bodies, ragdolls)\n"
		"bit 10 - show helpers for active physicalized objects\n"
		"bit 11 - show helpers for players\n"
		"bit 12 - show helpers for independent entities (alive physical skeletons,particles,ropes)\n"
		"bits 16-31 - level of bounding volume trees to display (if 0, it just shows geometry)\n"
		"Examples: show static objects - 258, show active rigid bodies - 1026, show players - 2050");
  pConsole->Register("p_max_contact_gap", &pVars->maxContactGap, pVars->maxContactGap, 0,
		"Sets the gap, enforced whenever possible, between\n"
		"contacting physical objects."
		"Usage: p_max_contact_gap 0.01\n"
		"This variable is used for internal tweaking only.");
  pConsole->Register("p_max_contact_gap_player", &pVars->maxContactGapPlayer, pVars->maxContactGapPlayer, 0,
		"Sets the safe contact gap for player collisions with\n"
		"the physical environment."
		"Usage: p_max_contact_gap_player 0.01\n"
		"This variable is used for internal tweaking only.");
  pConsole->Register("p_gravity_z", &pVars->gravity.z, pVars->gravity.z, CVAR_FLOAT);
  pConsole->Register("p_max_substeps", &pVars->nMaxSubsteps, pVars->nMaxSubsteps, 0,
		"Limits the number of substeps allowed in variable time step mode.\n"
		"Usage: p_max_substeps 5\n"
		"Objects that are not allowed to perform time steps\n"
		"beyond some value make several substeps.");
	pConsole->Register("p_prohibit_unprojection", &pVars->bProhibitUnprojection, pVars->bProhibitUnprojection, 0,
		"This variable is obsolete.");
	pConsole->Register("p_enforce_contacts", &pVars->bEnforceContacts, pVars->bEnforceContacts, 0,
		"This variable is obsolete.");
	pConsole->Register("p_damping_group_size", &pVars->nGroupDamping, pVars->nGroupDamping, 0,
		"Sets contacting objects group size\n"
		"before group damping is used."
		"Usage: p_damping_group_size 3\n"
		"Used for internal tweaking only.");
	pConsole->Register("p_group_damping", &pVars->groupDamping, pVars->groupDamping, 0,
		"Toggles damping for object groups.\n"
		"Usage: p_group_damping [0/1]\n"
		"Default is 1 (on). Used for internal tweaking only.");
	pConsole->Register("p_max_substeps_large_group", &pVars->nMaxSubstepsLargeGroup, pVars->nMaxSubstepsLargeGroup, 0,
		"Limits the number of substeps large groups of objects can make");
	pConsole->Register("p_num_bodies_large_group", &pVars->nBodiesLargeGroup, pVars->nBodiesLargeGroup, 0,
		"Group size to be used with p_max_substeps_large_group, in bodies");
	pConsole->Register("p_break_on_validation", &pVars->bBreakOnValidation, pVars->bBreakOnValidation, 0,
		"Toggles break on validation error.\n"
		"Usage: p_break_on_validation [0/1]\n"
		"Default is 0 (off). Issues DebugBreak() call in case of\n"
		"a physics parameter validation error.");
	pConsole->Register("p_time_granularity", &pVars->timeGranularity, pVars->timeGranularity, 0,
		"Sets physical time step granularity.\n"
		"Usage: p_time_granularity [0..0.1]\n"
		"Used for internal tweaking only.");
	pConsole->Register("p_list_active_objects", &pVars->bLogActiveObjects, pVars->bLogActiveObjects);
	pConsole->Register("p_profile_entities", &pVars->bProfileEntities, pVars->bProfileEntities, 0,
		"Enables per-entity time step profiling");
	pConsole->Register("p_profile_functions", &pVars->bProfileFunx, pVars->bProfileFunx, 0,
		"Enables detailed profiling of physical environment-sampling functions");
	pConsole->Register("p_GEB_max_cells", &pVars->nGEBMaxCells, pVars->nGEBMaxCells, 0,
		"Specifies the cell number threshold after which GetEntitiesInBox issues a warning");
	pConsole->Register("p_max_velocity", &pVars->maxVel, pVars->maxVel, 0,
		"Clamps physicalized objects' velocities to this value");
	pConsole->Register("p_max_player_velocity", &pVars->maxVelPlayers, pVars->maxVelPlayers, 0,
		"Clamps players' velocities to this value");

	pConsole->Register("p_max_MC_iters", &pVars->nMaxMCiters, pVars->nMaxMCiters, 0,
		"Specifies the maximum number of microcontact solver iterations");
	pConsole->Register("p_accuracy_MC", &pVars->accuracyMC, pVars->accuracyMC, 0,
		"Desired accuracy of microcontact solver (velocity-related, m/s)");
	pConsole->Register("p_accuracy_LCPCG", &pVars->accuracyLCPCG, pVars->accuracyLCPCG, 0,
		"Desired accuracy of LCP CG solver (velocity-related, m/s)");
	pConsole->Register("p_max_contacts", &pVars->nMaxContacts, pVars->nMaxContacts, 0,
		"Maximum contact number, after which contact reduction mode is activated");
	pConsole->Register("p_max_plane_contacts", &pVars->nMaxPlaneContacts, pVars->nMaxPlaneContacts, 0,
		"Maximum number of contacts lying in one plane between two rigid bodies\n"
		"(the system tries to remove the least important contacts to get to this value)");
	pConsole->Register("p_max_plane_contacts_distress", &pVars->nMaxPlaneContactsDistress, pVars->nMaxPlaneContactsDistress, 0,
		"Same as p_max_plane_contacts, but is effective if total number of contacts is above p_max_contacts");
	pConsole->Register("p_max_LCPCG_subiters", &pVars->nMaxLCPCGsubiters, pVars->nMaxLCPCGsubiters, 0,
		"Limits the number of LCP CG solver inner iterations (should be of the order of the number of contacts)");
	pConsole->Register("p_max_LCPCG_subiters_final", &pVars->nMaxLCPCGsubitersFinal, pVars->nMaxLCPCGsubitersFinal, 0,
		"Limits the number of LCP CG solver inner iterations during the final iteration (should be of the order of the number of contacts)");
	pConsole->Register("p_max_LCPCG_microiters", &pVars->nMaxLCPCGmicroiters, pVars->nMaxLCPCGmicroiters, 0,
		"Limits the total number of per-contact iterations during one LCP CG iteration\n"
		"(number of microiters = number of subiters * number of contacts)");
	pConsole->Register("p_max_LCPCG_microiters_final", &pVars->nMaxLCPCGmicroitersFinal, pVars->nMaxLCPCGmicroitersFinal, 0,
		"Same as p_max_LCPCG_microiters, but for the final LCP CG iteration");
	pConsole->Register("p_max_LCPCG_iters", &pVars->nMaxLCPCGiters, pVars->nMaxLCPCGiters, 0,
		"Maximum number of LCP CG iterations");
	pConsole->Register("p_min_LCPCG_improvement", &pVars->minLCPCGimprovement, pVars->minLCPCGimprovement, 0,
		"Defines a required residual squared length improvement, in fractions of 1");
	pConsole->Register("p_max_LCPCG_fruitless_iters", &pVars->nMaxLCPCGFruitlessIters, pVars->nMaxLCPCGFruitlessIters, 0,
		"Maximum number of LCP CG iterations w/o improvement (defined by p_min_LCPCGimprovement)");
	pConsole->Register("p_accuracy_LCPCG_no_improvement", &pVars->accuracyLCPCGnoimprovement, pVars->accuracyLCPCGnoimprovement, 0,
		"Required LCP CG accuracy that allows to stop if there was no improvement after p_max_LCPCG_fruitless_iters");
	pConsole->Register("p_min_separation_speed", &pVars->minSeparationSpeed, pVars->minSeparationSpeed, 0,
		"Used a threshold in some places (namely, to determine when a particle\n"
		"goes to rest, and a sliding condition in microcontact solver)");
	pConsole->Register("p_use_distance_contacts", &pVars->bUseDistanceContacts, pVars->bUseDistanceContacts, 0,
		"Allows to use distance-based contacts (is forced off in multiplayer)");
	pConsole->Register("p_unproj_vel_scale", &pVars->unprojVelScale, pVars->unprojVelScale, 0,
		"Requested unprojection velocity is set equal to penetration depth multiplied by this number");
	pConsole->Register("p_max_unproj_vel", &pVars->maxUnprojVel, pVars->maxUnprojVel, 0,
		"Limits the maximum unprojection velocity request");
	pConsole->Register("p_penalty_scale", &pVars->penaltyScale, pVars->penaltyScale, 0, 
		"Scales the penalty impulse for objects that use the simple solver");
	pConsole->Register("p_max_contact_gap_simple", &pVars->maxContactGapSimple, pVars->maxContactGapSimple, 0, 
		"Specifies the maximum contact gap for objects that use the simple solver");
	pConsole->Register("p_skip_redundant_colldet", &pVars->bSkipRedundantColldet, pVars->bSkipRedundantColldet, 0, 
		"Specifies whether to skip furher collision checks between two convex objects using the simple solver\n"
		"when they have enough contacts between them");
	pConsole->Register("p_limit_simple_solver_energy", &pVars->bLimitSimpleSolverEnergy, pVars->bLimitSimpleSolverEnergy, 0, 
		"Specifies whether the energy added by the simple solver is limited (0 or 1)");
	pConsole->Register("p_max_world_step", &pVars->maxWorldStep, pVars->maxWorldStep, 0, 
		"Specifies the maximum step physical world can make (larger steps will be truncated)");
	pConsole->Register("p_use_unproj_vel", &pVars->bCGUnprojVel, pVars->bCGUnprojVel, 0, "internal solver tweak");
	pConsole->Register("p_tick_breakable", &pVars->tickBreakable, pVars->tickBreakable, 0, 
		"Sets the breakable objects structure update interval");
	pConsole->Register("p_log_lattice_tension", &pVars->bLogLatticeTension, pVars->bLogLatticeTension, 0, 
		"If set, breakable objects will log tensions at the weakest spots");
	pConsole->Register("p_debug_joints", &pVars->bLogLatticeTension, pVars->bLogLatticeTension, 0, 
		"If set, breakable objects will log tensions at the weakest spots");
	pConsole->Register("p_lattice_max_iters", &pVars->nMaxLatticeIters, pVars->nMaxLatticeIters, 0, 
		"Limits the number of iterations of lattice tension solver");
	pConsole->Register("p_max_entity_cells", &pVars->nMaxEntityCells, pVars->nMaxEntityCells, 0,
		"Limits the number of entity grid cells an entity can occupy");
	pConsole->Register("p_max_MC_mass_ratio", &pVars->maxMCMassRatio, pVars->maxMCMassRatio, 0, 
		"Maximum mass ratio between objects in an island that MC solver is considered safe to handle");
	pConsole->Register("p_max_MC_vel", &pVars->maxMCVel, pVars->maxMCVel, 0, 
		"Maximum object velocity in an island that MC solver is considered safe to handle");
	pConsole->Register("p_max_LCPCG_contacts", &pVars->maxLCPCGContacts, pVars->maxLCPCGContacts, 0, 
		"Maximum number of contacts that LCPCG solver is allowed to handle");
	pConsole->Register("p_approx_caps_len", &pVars->approxCapsLen, pVars->approxCapsLen, 0, 
		"Breakable trees are approximated with capsules of this length (0 disables approximation)");
	pConsole->Register("p_max_approx_caps", &pVars->nMaxApproxCaps, pVars->nMaxApproxCaps, 0, 
		"Maximum number of capsule approximation levels for breakable trees");
	pConsole->Register("p_players_can_break", &pVars->bPlayersCanBreak, pVars->bPlayersCanBreak, 0, 
		"Whether living entities are allowed to break static objects with breakable joints");
	pConsole->Register("p_max_debris_mass", &pVars->massLimitDebris, 10.0f, 0, 
		"Broken pieces with mass<=this limit use debris collision settings");
	pConsole->Register("p_max_object_splashes", &pVars->maxSplashesPerObj, pVars->maxSplashesPerObj, 0, 
		"Specifies how many splash events one entity is allowed to generate");
	pConsole->Register("p_splash_dist0", &pVars->splashDist0, pVars->splashDist0, 0, 
		"Range start for splash event distance culling");
	pConsole->Register("p_splash_force0", &pVars->minSplashForce0, pVars->minSplashForce0, 0, 
		"Minimum water hit force to generate splash events at p_splash_dist0");
	pConsole->Register("p_splash_vel0", &pVars->minSplashVel0, pVars->minSplashVel0, 0, 
		"Minimum water hit velocity to generate splash events at p_splash_dist0");
	pConsole->Register("p_splash_dist1", &pVars->splashDist1, pVars->splashDist1, 0, 
		"Range end for splash event distance culling");
	pConsole->Register("p_splash_force1", &pVars->minSplashForce1, pVars->minSplashForce1, 0, 
		"Minimum water hit force to generate splash events at p_splash_dist1");
	pConsole->Register("p_splash_vel1", &pVars->minSplashVel1, pVars->minSplashVel1, 0, 
		"Minimum water hit velocity to generate splash events at p_splash_dist1");
	pConsole->Register("p_debug_explosions", &pVars->bDebugExplosions, pVars->bDebugExplosions, 0, 
		"Turns on explosions debug mode");

	pConsole->Register("p_net_minsnapdist", &pVars->netMinSnapDist, pVars->netMinSnapDist, 0,
		"Minimum distance between server position and client position at which to start snapping");
	pConsole->Register("p_net_velsnapmul", &pVars->netVelSnapMul, pVars->netVelSnapMul, 0,
		"Multiplier to expand the p_net_minsnapdist based on the objects velocity");
	pConsole->Register("p_net_minsnapdot", &pVars->netMinSnapDot, pVars->netMinSnapDot, 0,
		"Minimum quat dot product between server orientation and client orientation at which to start snapping");
	pConsole->Register("p_net_angsnapmul", &pVars->netAngSnapMul, pVars->netAngSnapMul, 0,
		"Multiplier to expand the p_net_minsnapdot based on the objects angular velocity");
	pConsole->Register("p_net_smoothtime", &pVars->netSmoothTime, pVars->netSmoothTime, 0,
		"How much time should non-snapped positions take to synchronize completely?");

	pVars->flagsColliderDebris = geom_colltype_debris;
	pVars->flagsANDDebris = ~(geom_colltype_vehicle|geom_colltype6);

#ifndef EXCLUDE_SCALEFORM_SDK
	// CryMP: stock CryMovie/CryFont path owns Flash initialization for now.
#endif

	if (m_bEditor)
	{
		// Setup physical grid for Editor.
		int nCellSize = 16;
		m_env.pPhysicalWorld->SetupEntityGrid(2,Vec3(0,0,0), (2048)/nCellSize,(2048)/nCellSize, (float)nCellSize,(float)nCellSize);
		pConsole->CreateKeyBind("comma", "#System.SetCVar(\"p_single_step_mode\",1-System.GetCVar(\"p_single_step_mode\"));");
		pConsole->CreateKeyBind("period", "p_do_step 1");
	}
	//pConsole->CreateKeyBind("lbracket", "sys_physics_CPU 0");
	//pConsole->CreateKeyBind("rbracket", "sys_physics_CPU 1");

	return true;
}

#ifndef EXCLUDE_GPU_PARTICLE_PHYSICS
bool CSystem::InitGPUPhysics()
{

	if (m_pUserCallback)
		m_pUserCallback->OnInitProgress( "Initializing GPU Physics..." );

#if !defined(_LIB) && !defined(LINUX) && !defined(PS3)
	//m_dll.hPhysicsGPU = LoadDLL(DLL_GPU_PHYSICS, false);
	m_dll.hPhysicsGPU = LoadDLL( DLL_GPU_PHYSICS, false );

	if (m_dll.hPhysicsGPU)
	{
		pfnCreateGPUPhysics = (CreateGPUPhysicsProc) CryGetProcAddress(m_dll.hPhysicsGPU, "CreateGPUPhysics" );

		if( ( pfnCreateGPUPhysics != NULL ) )
		{
			m_pGPUPhysics = pfnCreateGPUPhysics( this );
		} 
		else 
		{
			m_pGPUPhysics = NULL;
			pfnCreateGPUPhysics	= NULL;

			Warning(VALIDATOR_MODULE_PHYSICS, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, DLL_GPU_PHYSICS, "Couldn't load all required functions from " DLL_GPU_PHYSICS );
		}
	} 
	else 
	{
		Warning(VALIDATOR_MODULE_PHYSICS, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, DLL_GPU_PHYSICS, "Couldn't load " DLL_GPU_PHYSICS);
		return false;
	}

#else //_LIB
	m_pGPUPhysics = CreateGPUPhysics(this);
#endif //_LIB

	return true;

}
#endif

/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitMovieSystem()
{
	if (m_pUserCallback)
		m_pUserCallback->OnInitProgress( "Initializing Movie System..." );

	if (!IsDedicated())
	{
#if !defined(_LIB) && !defined(LINUX) && !defined(PS3)
		m_dll.hMovie = LoadDLL(DLL_MOVIE);
		if(!m_dll.hMovie)
			return false;

		PFNCREATEMOVIESYSTEM pfnCreateMovieSystem = (PFNCREATEMOVIESYSTEM) CryGetProcAddress(m_dll.hMovie,DLL_INITFUNC_MOVIE);
		if (!pfnCreateMovieSystem)
		{
			Error( "Error loading function CreateMovieSystem" );
			return false;
		}

		m_env.pMovieSystem = pfnCreateMovieSystem(this);
#elif defined PS3 && !defined _LIB
		m_env.pMovieSystem = PS3Init::CryMovieSystem(this);
#else
		m_env.pMovieSystem = CreateMovieSystem( this );
#endif

		if (!m_env.pMovieSystem)
		{
			Error("Error creating the movie system interface");
			return false;
		}
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitAISystem()
{
#if !defined(_LIB) && !defined(LINUX) && !defined(PS3)
	m_dll.hAI = LoadDLL(DLL_AI,false);
	if (!m_dll.hAI)
		return true;

	IAISystem *(*pFnCreateAISystem)(ISystem*) = (IAISystem *(*)(ISystem*)) CryGetProcAddress(m_dll.hAI,DLL_INITFUNC_AI);
	if (!pFnCreateAISystem)
	{
		Error( "Cannot fins entry proc in AI system");
		return true;
	}
	m_env.pAISystem = pFnCreateAISystem(this);
#elif defined PS3 && !defined _LIB
	m_env.pAISystem = PS3Init::CryAISystem(this);
#else //_LIB
	m_env.pAISystem = CreateAISystem(this);
#endif //_LIB
	if (!m_env.pAISystem)
		Error( "Cannot instantiate AISystem class" );

//	m_env.pAISystem->Init( this, NULL, NULL );

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitScriptSystem()
{
	if (m_pUserCallback)
		m_pUserCallback->OnInitProgress( "Initializing Script System..." );
#if defined(CRYMP_EMBEDDED_CRYSYSTEM) && !defined(LINUX) && !defined(PS3)
	// CryMP: ReplaceScriptSystem used to patch the stock CrySystem factory call.
	// Invoke the existing CryMP ScriptSystem directly in the embedded build.
	CryLogAlways("$3[CryMP] Initializing Script System");
	ScriptSystem* pScriptSystem = new ScriptSystem();
	pScriptSystem->Init();
	m_env.pScriptSystem = pScriptSystem;
#elif !defined(_LIB) && !defined(LINUX) && !defined(PS3)
	m_dll.hScript = LoadDLL("CryScriptSystem.dll");

	if(m_dll.hScript==NULL)
		return (false);

	CREATESCRIPTSYSTEM_FNCPTR fncCreateScriptSystem;
	fncCreateScriptSystem = (CREATESCRIPTSYSTEM_FNCPTR) CryGetProcAddress(m_dll.hScript,DLL_INITFUNC_SCRIPT);
	if(fncCreateScriptSystem==NULL)
	{
		Error( "Error initializeing ScriptSystem" );
		return (false);
	}

	m_env.pScriptSystem = fncCreateScriptSystem(this,true);
#elif defined PS3 && !defined _LIB
	m_env.pScriptSystem = PS3Init::CryScriptSystem(this,true);
#else //_LIB
	m_env.pScriptSystem = CreateScriptSystem(this,true);
#endif
	if(m_env.pScriptSystem==NULL)
	{
		Error( "Error initializeing ScriptSystem" );
		return (false);
	}

	m_env.pScriptSystem->PostInit();

	// Load script surface types.	
	if (m_env.pScriptSystem)
		m_env.pScriptSystem->LoadScriptedSurfaceTypes( "Scripts/Materials",false );

	return (true);
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitFileSystem()
{
	if (m_pUserCallback)
		m_pUserCallback->OnInitProgress("Initializing File System...");

	const ICmdLineArg* pArg = m_pCmdLine->FindArg(eCLAT_Pre, "LvlRes");

	bool bLvlRes = false;

	if (pArg)
		bLvlRes = true;

	// CryMP: use our C++20 CryPak implementation directly.
	// This reproduces what CreateNewCryPak() used to do when
	// CrySystem.dll was patched by ReplaceCryPak().
	gEnv = GetGlobalEnvironment();

	CryPak* pCryPak = &CryPak::GetInstance();
	m_env.pCryPak = pCryPak;

	pCryPak->SetGameFolderWritable(m_bGameFolderWritable);

	// CryMP: load the embedded Internal.pak resource.
	// This used to happen inside CreateNewCryPak().
	const auto internalPak =
		WinAPI::GetDataResource(nullptr, RESOURCE_INTERNAL_PAK);

	if (!internalPak.empty())
	{
		pCryPak->LoadClientPak(
			internalPak.data(),
			internalPak.size()
		);
	}

	if (m_bEditor || bLvlRes)
		m_env.pCryPak->RecordFileOpen(ICryPak::RFOM_EngineStartup);

	const bool bRes = m_env.pCryPak->Init("");

	if (bRes)
	{
		const ICmdLineArg* pakalias =
			m_pCmdLine->FindArg(eCLAT_Pre, "pakalias");

		if (pakalias && strlen(pakalias->GetValue()) > 0)
			m_env.pCryPak->ParseAliases(pakalias->GetValue());
	}

	// Load engine folders.
	SCryEngineFoldersLoader foldersLoader(this);
	foldersLoader.Load(CRYENGINE_FOLDERS_CONFIG_FILE);

	ChangeUserPath(m_engineFolders[ENGINE_FOLDER_USER].c_str());

	if (const ICmdLineArg* pModArg =
		GetICmdLine()->FindArg(eCLAT_Pre, "MOD"))
	{
		if (IsMODValid(pModArg->GetValue()))
		{
			string modPath;
			modPath.append("Mods\\");
			modPath.append(pModArg->GetValue());
			modPath.append("\\");

			pCryPak->AddMod(modPath.c_str());
		}
	}

	return bRes;
}

//////////////////////////////////////////////////////////////////////////
bool CSystem::InitStreamEngine()
{
	if (m_pUserCallback)
		m_pUserCallback->OnInitProgress( "Initializing Stream Engine..." );

	// CryMP: reuse the current C++20 stream engine instead of Wars CStreamEngine.
	m_pStreamEngine = &StreamEngine::GetInstance();
	return true;
}

/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitFont()
{
	if (m_pUserCallback)
		m_pUserCallback->OnInitProgress( "Initializing Fonts..." );

	// In Editor mode Renderer is not initialized yet, so skip InitFont.
	if (m_bEditor && !m_env.pRenderer)
		return true;

#if !defined(_LIB) && !defined(LINUX) && !defined(PS3)
	m_dll.hFont = LoadDLL(DLL_FONT);
	if(!m_dll.hFont)
		return (false);

	PFNCREATECRYFONTINTERFACE pfnCreateCryFontInstance = (PFNCREATECRYFONTINTERFACE) CryGetProcAddress(m_dll.hFont,DLL_INITFUNC_FONT);
	if(!pfnCreateCryFontInstance)
	{
		Error( "Error loading CreateCryFontInstance" );
		return (false);
	}

	m_env.pCryFont = pfnCreateCryFontInstance(this);
#elif defined PS3 && !defined _LIB
	m_env.pCryFont = PS3Init::CryFont(this);
#else //_LIB
	m_env.pCryFont = CreateCryFontInterface(this);
#endif //_LIB
	if(!m_env.pCryFont)
	{
		Error( "Error loading CreateCryFontInstance" );
		return false;
	}

	if (IsDedicated())
		return true;

	// Load the default font
	IFFont *pConsoleFont = m_env.pCryFont->NewFont("console");
	m_pIFont = m_env.pCryFont->NewFont("default");
	if(!m_pIFont || !pConsoleFont)
	{
		CryWarning( VALIDATOR_MODULE_SYSTEM,VALIDATOR_ERROR,"Error creating the default fonts" );
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	string szFontPath = "Fonts/default.xml";

	if(!m_pIFont->Load(szFontPath.c_str()))
	{
		string szError = "Error loading the default font from ";
		szError += szFontPath;
		szError += ". You're probably running the executable from the wrong working folder.";
		CryWarning( VALIDATOR_MODULE_SYSTEM,VALIDATOR_ERROR,szError.c_str());

		//return false;
	}

	int n = szFontPath.find("default.xml");
	assert(n != string::npos);

	szFontPath.replace(n, strlen("default.xml"), "console.xml");

	if(!pConsoleFont->Load(szFontPath.c_str()))
	{
		string szError = "Error loading the console font from ";
		szError += szFontPath;
		szError += ". You're probably running the executable from the wrong working folder.";
		CryWarning( VALIDATOR_MODULE_SYSTEM,VALIDATOR_ERROR,szError.c_str() );

		//return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CSystem::Init3DEngine()
{
	if (m_pUserCallback)
		m_pUserCallback->OnInitProgress( "Initializing 3D Engine..." );

#if !defined(_LIB) && !defined(LINUX) && !defined(PS3)
  ::SetLastError(0);
  m_dll.h3DEngine = LoadDLL(DLL_3DENGINE);
	if (!m_dll.h3DEngine)
		return false;

	PFNCREATECRY3DENGINE pfnCreateCry3DEngine;
	pfnCreateCry3DEngine = (PFNCREATECRY3DENGINE) CryGetProcAddress( m_dll.h3DEngine, DLL_INITFUNC_3DENGINE);
	if (!pfnCreateCry3DEngine)
	{
		Error("CreateCry3DEngine is not exported api function in Cry3DEngine.dll");
		return false;
	} 

	m_env.p3DEngine = (*pfnCreateCry3DEngine)(this,g3deInterfaceVersion);
#elif defined PS3 && !defined _LIB
	m_env.p3DEngine = PS3Init::Cry3DEngine(this,g3deInterfaceVersion);
#else //_LIB
	m_env.p3DEngine = CreateCry3DEngine(this,g3deInterfaceVersion);
#endif //_LIB

  if (!m_env.p3DEngine )
	{
    Error( "Error Creating 3D Engine interface" );
		return false;
	}

	if (!m_env.p3DEngine->Init())
	{
		Error( "Error Initializing 3D Engine" );
		return false;
	}
	m_pProcess = m_env.p3DEngine;
	m_pProcess->SetFlags(PROC_3DENGINE);

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CSystem::InitAnimationSystem()
{
	if (m_pUserCallback)
		m_pUserCallback->OnInitProgress( "Initializing Animation System..." );

#if !defined(_LIB) && !defined(LINUX) && !defined(PS3)
	m_dll.hAnimation = LoadDLL("CryAnimation.dll");

	if (!m_dll.hAnimation)
		return false;

	PFNCREATECRYANIMATION pfnCreateCharManager;
	pfnCreateCharManager = (PFNCREATECRYANIMATION) CryGetProcAddress( m_dll.hAnimation, DLL_INITFUNC_ANIMATION);
	if (!pfnCreateCharManager)
		return false;

	m_env.pCharacterManager = (*pfnCreateCharManager)(this,gAnimInterfaceVersion);
#elif defined PS3 && !defined _LIB
	m_env.pCharacterManager = PS3Init::CryAnimation(this,gAnimInterfaceVersion);
#else //_LIB
	m_env.pCharacterManager = CreateCharManager(this,gAnimInterfaceVersion);
#endif //_LIB

	if (m_env.pCharacterManager)
		GetILog()->LogPlus(" ok");
	else
		GetILog()->LogPlus (" FAILED");

	return m_env.pCharacterManager != NULL;
}

//////////////////////////////////////////////////////////////////////////
void CSystem::InitVTuneProfiler()
{
#ifdef PROFILE_WITH_VTUNE
	HMODULE hModule = CryLoadLibrary( "VTuneApi.dll" );
	if (hModule)
	{
		VTPause = (VTuneFunction) CryGetProcAddress( hModule, "VTPause");
		VTResume = (VTuneFunction) CryGetProcAddress( hModule, "VTResume");
		if (!VTPause || !VTResume)
			CryError( "Failed to find VTPause" );
		else
			CryLogAlways( "VTune API Initialized" );
	}
	else
	{
		CryError( "Failed to load VTuneAPI.dll" );
	}
#endif //PROFILE_WITH_VTUNE
}

//////////////////////////////////////////////////////////////////////////
void CSystem::InitLocalization()
{
	SCryEngineLanguageConfigLoader defaultLanguageLoader(this);
	defaultLanguageLoader.Load( CRYENGINE_DEFAULT_LANGUAGE_CONFIG_FILE );
	string language = defaultLanguageLoader.m_language;
	if (language.empty())
	{
		language = "english";
	}

	// CryMP: use the existing Crysis 1 localization implementation.
	// The Wars localization manager is not compatible with our C1 localized data/paks.
	if (m_pLocalizationManager == NULL)
		m_pLocalizationManager = reinterpret_cast<CLocalizedStringsManager*>(&LocalizationManager::GetInstance());

	ICVar *pCVar = m_env.pConsole->GetCVar( "g_language" );
	if (pCVar)
	{
		if (strlen(pCVar->GetString()) == 0)
		{
			pCVar->Set(language.c_str());
		}
		else
		{
			language = pCVar->GetString();
		}
	}
	GetLocalizationManager()->SetLanguage(language.c_str());
}



static void ChangeLowSpecPakChange( ICVar *pVar )
{
	assert(pVar);
	assert(gEnv->pCryPak);

	static bool bOldState = false;
	static bool bPakMissing = false;

	const char *szLowSpecPak = "Game/Lowspec/*.pak";

	bool bNewState = pVar->GetIVal()!=0;

	bool bOk=false;

	if(!bOldState && bNewState)
	{
		// change to on
		bOk = gEnv->pCryPak->OpenPacks("",szLowSpecPak);

		if(!bOk)
			bPakMissing=true;

		CryLog("opening LowSpec.pak %s",bOk?"ok":"failed");
	}
	else if(bOldState && !bNewState)
	{
		// change to off
		bOk = gEnv->pCryPak->ClosePacks(szLowSpecPak);

		CryLog("closing LowSpec.pak %s",bOk?"ok":"failed");
	}
	else return;		// no change

	bOldState=bNewState;

	if(bPakMissing)
	{
		// fallback behavior if there is no LowSpec.pak
		ICVar *pTex = gEnv->pConsole->GetCVar("r_TexResolution");						assert(pTex);
		ICVar *pDDN = gEnv->pConsole->GetCVar("r_TexBumpResolution");				assert(pDDN);

		int iDir  = bNewState ? 1 : -1;

		CryLog("LowSpec.pak is missing - emulating wth cvars delta=%d",iDir);

		pTex->Set(pTex->GetIVal()+iDir);
		pDDN->Set(pDDN->GetIVal()+iDir);
		return;
	}

	if(gEnv->pRenderer)
	{
		CryLog("changing LowSpec.pak trigger reloading of all textures");
		((CSystem*)gEnv->pSystem)->ReloadTexturesNextFrame();
	}
}


static void ClampSysSpecVars()
{
	int iMaxSysSpec = ((CSystem*)gEnv->pSystem)->GetMaxConfigSpec();

	const char *szPrefix = "sys_spec_";

	std::vector<const char*> cmds;

	cmds.resize(gEnv->pConsole->GetSortedVars(0,0,szPrefix));
	gEnv->pConsole->GetSortedVars(&cmds[0],cmds.size(),szPrefix);

	std::vector<const char*>::const_iterator it, end=cmds.end();

	for(it=cmds.begin();it!=end;++it)
	{
		const char *szName = *it;

		if(stricmp(szName,"sys_spec_Full")==0)			// clamp it on a sys_spec_* level, not at top level (sys_spec)
			continue;

		ICVar *pCVar = gEnv->pConsole->GetCVar(szName);			assert(pCVar);

		if(!pCVar)
			continue;

		int iSpec = pCVar->GetIVal();

		// intended behavior: you still can change all controlled cvars,
		// we only clamp the convenient way changing them through sys_spec_ cvars and menu UI
		if(iSpec>iMaxSysSpec)
		{
//			CryLog("DEBUG ClampSysSpecVars %s was: %d",pCVar->GetName(),iSpec);			// TODO: remove this line
			pCVar->Set(iMaxSysSpec);
		}
	}
}


// System initialization
/////////////////////////////////////////////////////////////////////////////////
// INIT
/////////////////////////////////////////////////////////////////////////////////
bool CSystem::Init(const SSystemInitParams &startupParams)
{
	assert(IsHeapValid());
	
	//_controlfp(0, _EM_INVALID|_EM_ZERODIVIDE | _PC_64 );

#if defined(WIN32) || defined(WIN64)
	// check OS version - we only want to run on XP or higher - talk to Martin Mittring if you want to change this
	{
		OSVERSIONINFO osvi;

		osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

		GetVersionExA(&osvi);

		bool bIsWindowsXPorLater = osvi.dwMajorVersion>5 || ( osvi.dwMajorVersion==5 && osvi.dwMinorVersion>=1 );

		if(!bIsWindowsXPorLater)
		{
			CryError("Windows XP or later is required");
			return false;
		}
	}
#endif

	m_pMemoryManager = new CCryMemoryManager;

	// Get file version information.
	QueryVersionInfo();
	DetectGameFolderAccessRights();

	m_hInst = (WIN_HINSTANCE)startupParams.hInstance;
	m_hWnd = (WIN_HWND)startupParams.hWnd;

	m_bEditor = startupParams.isEditor;
	m_bPreviewMode = startupParams.isMinimal;
	m_bTestMode = startupParams.isTesting;
	m_pUserCallback = startupParams.pUserCallback;
	m_bDedicatedServer = startupParams.isDedicatedServer;
	m_pCmdLine = new CCmdLine(startupParams.cmdLine);
	// CryMP: protected-function table was removed from SSystemGlobalEnvironment.

	if (*startupParams.userPath)
		m_engineFolders[ENGINE_FOLDER_USER] = startupParams.userPath;

	m_env.bEditor = m_bEditor;
	m_env.bEditorGameMode = false;

	if (!startupParams.pValidator)
	{
		m_pDefaultValidator = new SDefaultValidator(this);
		m_pValidator = m_pDefaultValidator;
	}
	else
	{
		m_pValidator = startupParams.pValidator;
	}

	if (!m_bDedicatedServer)
	{
		const ICmdLineArg *dedicated = m_pCmdLine->FindArg(eCLAT_Pre,"dedicated");
		if (dedicated)
			m_bDedicatedServer = true;
	}

#if defined(USE_UNIXCONSOLE)
	if (m_pUserCallback == NULL && m_bDedicatedServer)
	{
		char headerString[128];
		m_pUserCallback = &g_UnixConsole;
		g_UnixConsole.SetRequireDedicatedServer(true);
		strcpy(
				headerString,
				"CryEngine2 - "
#if defined(LINUX)
				"Linux "
#endif
				"Dedicated Server"
				" - Version ");
		GetProductVersion().ToString(headerString + strlen(headerString));
		g_UnixConsole.SetHeader(headerString);
	}
#endif

#ifdef USE_MEM_POOL
	//Timur[9/30/2002]
	/*
	m_pMemoryManager = GetMemoryManager();
	if(g_hMemManager)
	{

		FNC_GetMemoryManager fncGetMemoryManager=(FNC_GetMemoryManager) CryGetProcAddress(g_hMemManager,"GetMemoryManager");
		if(fncGetMemoryManager)
		{
			m_pMemoryManager=fncGetMemoryManager();
		}
	}
	*/
#endif

	//////////////////////////////////////////////////////////////////////////
	// File system, must be very early
	//////////////////////////////////////////////////////////////////////////
	InitFileSystem();
	//////////////////////////////////////////////////////////////////////////

	const ICmdLineArg* root = m_pCmdLine->FindArg(eCLAT_Pre,"root");
	if (root)
	{
		string temp = PathUtil::ToUnixPath(PathUtil::AddSlash(root->GetValue()));
		if ( gEnv->pCryPak->MakeDir(temp.c_str()) )
			m_root = temp;
	}
	else
	{
		// always force the root dir to ~/.Crysis Wars/ if not specified/
#ifdef LINUX
		const char *envr = getenv("HOME");
		if(envr == NULL)
			envr = ".";	// oh well.
		string temp(envr);
		temp += "/.Crysis Wars/";
		if( gEnv->pCryPak->MakeDir(temp.c_str()) )
			m_root = temp;
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	// Logging is only availbe after file system initialization.
	//////////////////////////////////////////////////////////////////////////
	if (!startupParams.pLog)
	{
		m_env.pLog = new CLog(this);
		if (startupParams.pLogCallback)
			m_env.pLog->AddCallback(startupParams.pLogCallback);

		const ICmdLineArg *logfile = m_pCmdLine->FindArg(eCLAT_Pre,"logfile");
		if (logfile && strlen(logfile->GetValue()) > 0)
			if ( m_env.pLog->SetFileName(logfile->GetValue()) )
				goto L_done;
		if (startupParams.logFileName)
			if ( m_env.pLog->SetFileName(startupParams.logFileName) )
				goto L_done;
		m_env.pLog->SetFileName(DEFAULT_LOG_FILENAME);
L_done:;
	}
	else
	{
		m_env.pLog = startupParams.pLog;
	}
	
	LogVersion();

	// CryMP CryPak logs through the global CryMP logger; no SetLog step required.

	//////////////////////////////////////////////////////////////////////////
	// CREATE CONSOLE
	//////////////////////////////////////////////////////////////////////////
	m_env.pConsole = new CXConsole;
	if (m_pUserCallback)
		m_pUserCallback->OnInit(this);

	m_FrameProfileSystem.Init( this );

	m_env.pLog->RegisterConsoleVariables();

	// Register system console variables.
	CreateSystemVars();

  m_pCpu = new CCpuFeatures;
  m_pCpu->Detect();
	m_env.pi.numCoresAvailableToProcess = m_pCpu->GetCPUCount();

	LogSystemInfo();

	//////////////////////////////////////////////////////////////////////////

	AddCVarGroupDirectory("Game/Config/CVarGroups");

	//////////////////////////////////////////////////////////////////////////
	// Stream Engine
	//////////////////////////////////////////////////////////////////////////
	CryLogAlways("Stream Engine Initialization");
	InitStreamEngine();

#ifdef SP_DEMO
	if(m_bEditor)
		SetDevMode(true);											// In Dev mode.
	else
		SetDevMode(false);										// Not Dev mode.	
#else
	if(m_bEditor || m_pCmdLine->FindArg(eCLAT_Pre,"devmode"))
		SetDevMode(true);											// In Dev mode.
	 else
		SetDevMode(false);										// Not Dev mode.
#endif
	//m_sys_spec->Set(4); // default spec is veryhighspec, likely to be overwritten later

	//////////////////////////////////////////////////////////////////////////
	//Load config files
	//////////////////////////////////////////////////////////////////////////
#if !defined(PS3)	
	// load the game.cfg unless we're told to reset the profile
	if (m_pCmdLine->FindArg(eCLAT_Pre, "ResetProfile") == 0)
		LoadConfiguration("%USER%/game.cfg");
#endif
	LoadConfiguration("system.cfg");
#if !defined(PS3)	
	LoadConfiguration("systemcfgoverride.cfg");
#endif
//	if (!g_sysSpecChanged)
//		OnSysSpecChange( m_sys_spec );

	{
		if (m_pCmdLine->FindArg(eCLAT_Pre,"DX9"))
			m_env.pConsole->LoadConfigVar("r_Driver","DX9");
		else
//		if (m_pCmdLine->FindArg(eCLAT_Pre,"DX9+"))							// required to be deactivated for final release
//			m_env.pConsole->LoadConfigVar("r_Driver","DX9+");
//		else
		if (m_pCmdLine->FindArg(eCLAT_Pre,"DX10"))
			m_env.pConsole->LoadConfigVar("r_Driver","DX10");
	}

	CreateRendererVars();

	if(m_bDedicatedServer)
	{
		m_sSavedRDriver=m_rDriver->GetString();
		m_rDriver->Set("NULL");
	}


#if defined(WIN32) || defined(WIN64)
	if (stricmp(m_rDriver->GetString(), "Auto") == 0)
	{
		if (Win32SysInspect::IsDX10Supported())
			m_rDriver->Set("DX10");
		 else
			m_rDriver->Set("DX9");
	}
#endif

	// at the earliest point after getting game.cfg, system.cfg and command line (renderer type can be set there)
	// DX9 limits the system to high spec
	{
		/* uncomment this block to disable very high spec in dx9
		if(stricmp(m_rDriver->GetString(), "DX9") == 0)
		{
			m_nMaxConfigSpec = CONFIG_HIGH_SPEC;

			if (GetConfigSpec() > CONFIG_HIGH_SPEC)
				SetConfigSpec(CONFIG_HIGH_SPEC, true);
		}
		*/

		// that might require some adjustments to the sys_spec_ variables
		ClampSysSpecVars();
	}

#ifdef WIN32
	if (g_cvars.sys_WER) SetUnhandledExceptionFilter(CryEngineExceptionFilterWER);
#endif
/*
	if(m_sys_filecache->GetIVal())
	{
		if(!GetIPak()->OpenPack("","FileCache.dat"))
			CryLog("Base FileCache.dat not loaded");
	}
*/
	InitLocalization();

	//////////////////////////////////////////////////////////////////////////
	// Open basic pak files.
	//////////////////////////////////////////////////////////////////////////
	OpenBasicPaks();

	//////////////////////////////////////////////////////////////////////////
	// NETWORK
	//////////////////////////////////////////////////////////////////////////
	if (!startupParams.isMinimal)
	{
		CryLogAlways("Network initialization");
		InitNetwork();

		if (IsDedicated())
			m_pServerThrottle.reset( new CServerThrottle(this, m_pCpu->GetCPUCount()) );
	}
	//////////////////////////////////////////////////////////////////////////
	// PHYSICS
	//////////////////////////////////////////////////////////////////////////
	//if (!params.bPreview)
	{
		CryLogAlways("Physics initialization");
		if (!InitPhysics())
			return false;
	}

	//////////////////////////////////////////////////////////////////////////
	// MOVIE
	//////////////////////////////////////////////////////////////////////////
	//if (!params.bPreview)
	{
		if (IsDedicated())
			CryLogAlways("MovieSystem initialization skipped for dedicated server");
		else
		{
			CryLogAlways("MovieSystem initialization");
			if (!InitMovieSystem())
				return false;
		}
	}


  // open lowspec.pak after other paks as binding order matters
  // test results:
  //
  // loading rescue on P4 2.4 sys_spec=1
  //   45,34,27 sec without lowspec.pak -> 18.8,18.2,16.5 sec with lowspec.pak
  // complete loading time:
  //   98,95,83 sec with lowspec.pak
  ICVar *pLowSpecVar = m_env.pConsole->RegisterInt( "sys_LowSpecPak",0,0,
    "use low resolution textures from special pak file or emulate if no such pak exists\n"
    "0=don't use lowspec.pak (full texture quality)\n"
    "1=use lowspec.pak (faster loading of textures, reduced texture quality)\n"
    "Usage: sys_LowSpecPak 0/1",ChangeLowSpecPakChange);

	//////////////////////////////////////////////////////////////////////////
	// RENDERER
	//////////////////////////////////////////////////////////////////////////
	CryLogAlways("Renderer initialization");
	if (!InitRenderer(m_hInst, (startupParams.isEditor)? (WIN_HWND)1 : m_hWnd, startupParams.cmdLine))
		return false;

  ChangeLowSpecPakChange(pLowSpecVar);

	//////////////////////////////////////////////////////////////////////////
	// Hardware mouse
	//////////////////////////////////////////////////////////////////////////
#ifdef WIN32
	// - Dedicated server is in console mode by default (Hardware Mouse is always shown when console is)
	// - Mouse is always visible by default in Editor (we never start directly in Game Mode)
	// - Mouse has to be enabled manually by the Game (this is typically done in the main menu)
	m_env.pHardwareMouse = &HardwareMouse::GetInstance();
	HardwareMouse::GetInstance().Init();
#else
	m_env.pHardwareMouse = NULL;
#endif

	//////////////////////////////////////////////////////////////////////////
	// Physics Renderer (for debug helpers)
	//////////////////////////////////////////////////////////////////////////
	(m_pPhysRenderer = new CPhysRenderer)->Init(); // needs to be created after physics and renderer
	m_pPhysRenderer->SetCamera(&m_PhysRendererCamera);
	m_p_draw_helpers_str = GetIConsole()->RegisterString("p_draw_helpers","0",VF_CHEAT,
		"Same as p_draw_helpers_num, but encoded in letters\n"
		"Usage [Entity_Types]_[Helper_Types] - [t|s|r|R|l|i|g|a|y|e]_[g|c|b|l|t(#)]\n"
		"Entity Types:\n"
		"t - show terrain\n"
		"s - show static entities\n"
		"r - show sleeping rigid bodies\n"
		"R - show active rigid bodies\n"
		"l - show living entities\n"
		"i - show independent entities\n"
		"g - show triggers\n"
		"a - show areas\n"
		"y - show rays in RayWorldIntersection\n"
		"e - show explosion occlusion maps\n"
		"Helper Types\n"
		"g - show geometry\n"
		"c - show contact points\n"
		"b - show bounding boxes\n"
		"l - show tetrahedra lattices for breakable objects\n"
		"j - show structural joints (will force translucency on the main geometry)\n"
		"t(#) - show bounding volume trees up to the level #\n"
		"f(#) - only show geometries with this bit flag set (multiple f\'s stack)\n"
		"Example: p_draw_helpers larRis_g - show geometry for static, sleeping, active, independent entities and areas",
		OnDrawHelpersStrChange);

	GetIConsole()->Register("p_cull_distance", &m_pPhysRenderer->m_cullDist, m_pPhysRenderer->m_cullDist, 0,
		"Culling distance for physics helpers rendering");
	GetIConsole()->Register("p_wireframe_distance", &m_pPhysRenderer->m_wireframeDist, m_pPhysRenderer->m_wireframeDist, 0,
		"Maximum distance at which wireframe is drawn on physics helpers");
	GetIConsole()->Register("p_ray_fadein", &m_pPhysRenderer->m_timeRayFadein, m_pPhysRenderer->m_timeRayFadein, 0,
		"Fade-in time for ray physics helpers");
	GetIConsole()->Register("p_ray_peak_time", &m_pPhysRenderer->m_rayPeakTime, m_pPhysRenderer->m_rayPeakTime, 0,
		"Rays that take longer then this (in ms) will use different color");
	GetIConsole()->Register("p_jump_to_profile_ent", &(m_iJumpToPhysProfileEnt=0), 0, 0,
		"Move the local player next to the corresponding entity in the p_profile_entities list");
	GetIConsole()->CreateKeyBind("alt 1", "p_jump_to_profile_ent 1");
	GetIConsole()->CreateKeyBind("alt 2", "p_jump_to_profile_ent 2");
	GetIConsole()->CreateKeyBind("alt 3", "p_jump_to_profile_ent 3");
	GetIConsole()->CreateKeyBind("alt 4", "p_jump_to_profile_ent 4");
	GetIConsole()->CreateKeyBind("alt 5", "p_jump_to_profile_ent 5");
	

	//////////////////////////////////////////////////////////////////////////
	// CONSOLE
	//////////////////////////////////////////////////////////////////////////
	CryLogAlways("Console initialization");
	if (!InitConsole())
		return false;

	//////////////////////////////////////////////////////////////////////////
	// THREAD PROFILER
	//////////////////////////////////////////////////////////////////////////
	m_pThreadProfiler = new CThreadProfiler;

	//////////////////////////////////////////////////////////////////////////
	// TIME
	//////////////////////////////////////////////////////////////////////////
	CryLogAlways("Time initialization");
	if (!m_Time.Init(this))
		return (false);
	m_Time.ResetTimer();

	//////////////////////////////////////////////////////////////////////////
	// INPUT
	//////////////////////////////////////////////////////////////////////////
	if (!startupParams.isMinimal && !IsDedicated())
	{
		CryLogAlways("Input initialization");
		if (!InitInput(m_hWnd))
			return false;

		if(m_env.pHardwareMouse)
			m_env.pHardwareMouse->OnPostInitInput();
	}


	//////////////////////////////////////////////////////////////////////////
	// SOUND
	//////////////////////////////////////////////////////////////////////////
	if (!startupParams.isMinimal && !m_bDedicatedServer)
	{
		CryLogAlways("Sound initialization");
		bool bResult = InitSound(m_hWnd);
		//if (!bResult)
			//return false;
	}

#ifndef EXCLUDE_GPU_PARTICLE_PHYSICS
	/////////////////////////////////////////////////////////////////	
	// GPU PHYSICS
	// note GPU physics needs to be instanciated after the renderer!
	/////////////////////////////////////////////////////////////////	//if (!params.bPreview)
	if (m_gpu_particle_physics->GetIVal())
	{
		CryLogAlways("GPUPhysics initialization");
		bool bResult = InitGPUPhysics();
		if (!bResult)
		{
			CryLogAlways("GPUPhysics initialization failed!");
			m_gpu_particle_physics->Set(0);
		}

		//GPU physics is optional, so no need to fail if load doesn't happen.
		//if (!bResult)
		//return false;
	}
#endif

	//////////////////////////////////////////////////////////////////////////
	// FONT
	//////////////////////////////////////////////////////////////////////////
	CryLogAlways("Font initialization");
	if (!InitFont())
		return false;

	//////////////////////////////////////////////////////////////////////////
	// AI
	//////////////////////////////////////////////////////////////////////////
	if (!startupParams.isMinimal)
	{
		if (!gEnv->bEditor && m_sysAISystem && !m_sysAISystem->GetIVal())
			;
		else
		{
			CryLogAlways("AI initialization");
			if (!InitAISystem())
				return false;
		}
	}

	((CXConsole*)m_env.pConsole)->Init(this);

	//////////////////////////////////////////////////////////////////////////
	// Init Animation system
	//////////////////////////////////////////////////////////////////////////
	CryLogAlways("Initializing Animation System");
	if (!InitAnimationSystem())
		return false;
	//////////////////////////////////////////////////////////////////////////
	// Init 3d engine
	//////////////////////////////////////////////////////////////////////////
	CryLogAlways("Initializing 3D Engine");
	if (!Init3DEngine())
		return false;

#ifdef DOWNLOAD_MANAGER
	m_pDownloadManager = new CDownloadManager;
	m_pDownloadManager->Create(this);
#endif //DOWNLOAD_MANAGER

//#ifndef MEM_STD
//  CConsole::AddCommand("MemStats",::DumpAllocs);
//#endif

	//////////////////////////////////////////////////////////////////////////
	// SCRIPT SYSTEM
	//////////////////////////////////////////////////////////////////////////
	// We need script materials for now 

	// if (!startupParams.isMinimal)
	{
		CryLogAlways("Script System Initialization");
		if (!InitScriptSystem())
			return false;
	}
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// ENTITY SYSTEM
	//////////////////////////////////////////////////////////////////////////
	if (!startupParams.isMinimal)
	{
		CryLogAlways("Entity system initialization");
		if (!InitEntitySystem(m_hInst, m_hWnd))
			return false;
	}
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// AI SYSTEM INITIALIZATION
	//////////////////////////////////////////////////////////////////////////
	// AI System needs to be initialized after entity system
	if (!startupParams.isMinimal && m_env.pAISystem)
	{
		if (m_pUserCallback)
			m_pUserCallback->OnInitProgress( "Initializing AI System..." );
		CryLogAlways("Initializing AI System");
		m_env.pAISystem->Init();
	}

	/*
#ifdef SECUROM_32
	{
		CDataProbe probe;
		if (!probe.CheckPaul())	
		{
			Strange();
		}
	}
#endif
	*/

	//////////////////////////////////////////////////////////////////////////
	// BUDGETING SYSTEM
	m_pIBudgetingSystem = new CBudgetingSystem();

	//////////////////////////////////////////////////////////////////////////
	// Check loader.
	//////////////////////////////////////////////////////////////////////////
#if defined(_DATAPROBE) && defined(_CHECKLOADER) && !defined(LINUX)
	CDataProbe probe;
	if (!startupParams.pCheckFunc || !probe.CheckLoader( startupParams.pCheckFunc ))
	{
		Strange();
	}
#endif

	//////////////////////////////////////////////////////////////////////////
	// Initialize task threads.
	//////////////////////////////////////////////////////////////////////////
	m_pThreadTaskManager->InitThreads();

	SetAffinity();
	assert(IsHeapValid());


	if (strstr(startupParams.cmdLine,"-VTUNE") != 0 || g_cvars.sys_vtune != 0)
		InitVTuneProfiler();


	RegisterEngineStatistics();

#if defined(WIN32)
#	if !defined(WIN64)
	_controlfp(_PC_64,_MCW_PC); // not supported on WIN64
#	endif

#endif

	EnableFloatExceptions( g_cvars.sys_float_exceptions );

#if defined(WIN64) && defined(SECUROM_64)
	if (!m_bEditor && !IsDedicated())
	{
		int res = TestSecurom64();
		if (res != b64_ok)
		{
			_controlfp( 0,_MCW_EM ); // Enable floating point exceptions (Will eventually cause crash).
		}
	}
#endif

  return (true);
}


static void LoadConfigurationCmd( IConsoleCmdArgs *pParams )
{
	assert(pParams);

	if(pParams->GetArgCount()!=2)
	{
		gEnv->pLog->LogError("LoadConfiguration failed, one parameter needed");
		return;
	}

	GetISystem()->LoadConfiguration((string("Game/Config/") + pParams->GetArg(1)).c_str());
}


// --------------------------------------------------------------------------------------------------------------------------



static void _LvlRes_export_IResourceList( FILE *hFile, const ICryPak::ERecordFileOpenList eList )
{
	IResourceList *pResList = gEnv->pCryPak->GetRecorderdResourceList(eList);

	for (const char *filename = pResList->GetFirst(); filename; filename = pResList->GetNext())
	{
		enum {nMaxPath = 0x800};
		char szAbsPathBuf[nMaxPath];

		const char *szAbsPath = gEnv->pCryPak->AdjustFileName(filename,szAbsPathBuf,0);

		gEnv->pCryPak->FPrintf(hFile,"%s\n",szAbsPath);
	}
}

void LvlRes_export( IConsoleCmdArgs *pParams )
{
	// * this assumes the level was already loaded in the editor (resources have been recorded)
	// * it could be easily changed to run the launcher, start recording, load a level and quit (useful to autoexport many levels)

	const char *szLevelName = gEnv->pGame->GetIGameFramework()->GetLevelName();
	const char *szAbsLevelPath = gEnv->pGame->GetIGameFramework()->GetAbsLevelPath();

	if(!szAbsLevelPath || !szLevelName)
	{
		gEnv->pLog->LogError("Error: LvlRes_export no level loaded?");
		return;
	}

	string sPureLevelName = PathUtil::GetFile(szLevelName);		// level name without path

	// record all assets that might be loaded after level loading
	if(gEnv->pGame)
	if(gEnv->pGame->GetIGameFramework())
		gEnv->pGame->GetIGameFramework()->PrefetchLevelAssets(true);

	enum {nMaxPath = 0x800};
	char szAbsPathBuf[nMaxPath];

	sprintf(szAbsPathBuf,"%s/%s%s",szAbsLevelPath,sPureLevelName.c_str(),g_szLvlResExt);

	// Write resource list to file.
	FILE *hFile = gEnv->pCryPak->FOpen(szAbsPathBuf,"wt");

	if(!hFile)
	{
		gEnv->pLog->LogError("Error: LvlRes_export file open failed");
		return;
	}

	gEnv->pCryPak->FPrintf(hFile,"; this file can be safely deleted - it's only purpose is to produce a striped build (without unused assets)\n\n");

	char rootpath[_MAX_PATH];
	CryGetCurrentDirectory(sizeof(rootpath),rootpath);

	gEnv->pCryPak->FPrintf(hFile,"; EngineStartup\n");
	_LvlRes_export_IResourceList(hFile,ICryPak::RFOM_EngineStartup);
	gEnv->pCryPak->FPrintf(hFile,"; Level '%s'\n",szAbsLevelPath);
	_LvlRes_export_IResourceList(hFile,ICryPak::RFOM_Level);

	gEnv->pCryPak->FClose(hFile);
}


// create all directories needed to represent the path, \ and / are handled
// currently no error checking
// (should be moved to PathUtil)
// Arguments:
//   szPath - e.g. "c:\temp/foldername1/foldername2"
static void CreateDirectoryPath( const char *szPath )
{
	const char *p = szPath;

	string sFolder;

	for(;;)
	{
		if(*p=='/' || *p=='\\' || *p==0)
		{
			CryCreateDirectory(sFolder.c_str(),0);

			if(*p==0)
				break;

			sFolder+='\\';++p;
		}
		else
			sFolder += *p++;
	}
}

static string ConcatPath( const char *szPart1, const char *szPart2 )
{
	if(szPart1[0]==0)
		return szPart2;

	string ret;

	ret.reserve(strlen(szPart1)+1+strlen(szPart2));

	ret=szPart1;
	ret+="/";
	ret+=szPart2;

	return ret;
}

static string ConcatPath( const string &sPart1, const char *szPart2 )
{
	return ConcatPath(sPart1.c_str(), szPart2);
}

static string ConcatPath( const char *szPart1, const string &sPart2 )
{
	return ConcatPath(szPart1, sPart2.c_str());
}

static string ConcatPath( const string &sPart1, const string &sPart2 )
{
	return ConcatPath(sPart1.c_str(), sPart2.c_str());
}



class CLvlRes_base
{
public:

	// destructor
	virtual ~CLvlRes_base()
	{
	}

	void RegisterAllLevelPaks( const string &sPath )
	{
		_finddata_t fd;

		string sPathPattern = ConcatPath(sPath,"*.*");

		intptr_t handle = gEnv->pCryPak->FindFirst(sPathPattern.c_str(), &fd );

		if(handle<0)
		{
			gEnv->pLog->LogError("ERROR: CLvlRes_base failed '%s'",sPathPattern.c_str());
			return;
		}

		do 
		{
			if(fd.attrib&_A_SUBDIR)
			{
				if(strcmp(fd.name,".")!=0 && strcmp(fd.name,"..")!=0)
					RegisterAllLevelPaks(ConcatPath(sPath,fd.name));
			}
			else if(HasRightExtension(fd.name))			// open only the level paks if there is a LvlRes.txt, opening all would be too slow 
			{
				OnPakEntry(sPath,fd.name);
			}

		} while (gEnv->pCryPak->FindNext( handle, &fd ) >= 0);

		gEnv->pCryPak->FindClose(handle);
	}



	void Process( const string &sPath )
	{
		_finddata_t fd;

		string sPathPattern = ConcatPath(sPath,"*.*");

		intptr_t handle = gEnv->pCryPak->FindFirst(sPathPattern.c_str(), &fd );

		if(handle<0)
		{
			gEnv->pLog->LogError("ERROR: LvlRes_finalstep failed '%s'",sPathPattern.c_str());
			return;
		}

		do 
		{
			if(fd.attrib&_A_SUBDIR)
			{
				if(strcmp(fd.name,".")!=0 && strcmp(fd.name,"..")!=0)
					Process(ConcatPath(sPath,fd.name));
			}
			else if(HasRightExtension(fd.name))
			{
				string sFilePath = ConcatPath(sPath,fd.name);

				gEnv->pLog->Log("CLvlRes_base processing '%s' ...",sFilePath.c_str());

				FILE *hFile = gEnv->pCryPak->FOpen(sFilePath.c_str(),"rb");

				if(hFile)
				{
					std::vector<char> vBuffer;		

					size_t len = gEnv->pCryPak->FGetSize(hFile);
					vBuffer.resize(len+1);

					if(len)
					{
						if(gEnv->pCryPak->FReadRaw(&vBuffer[0],len,1,hFile)==1)
						{
							vBuffer[len]=0;															// end terminator

							char *p = &vBuffer[0];

							while(*p)
							{
								while(*p!=0 && *p<=' ')										// jump over whitespace
									++p;

								char *pLineStart = p;

								while(*p!=0 && *p!=10 && *p!=13)					// goto end of line
									++p;

								char *pLineEnd = p;

								while(*p!=0 && (*p==10 || *p==13))				// goto next line with data
									++p;

								if(*pLineStart!=';')											// if it's not a commented line
								{
									*pLineEnd=0;
									OnFileEntry(pLineStart);				// add line
								}
							}
						}
						else gEnv->pLog->LogError("Error: LvlRes_finalstep file open '%s' failed",sFilePath.c_str());
					}

					gEnv->pCryPak->FClose(hFile);
				}
				else gEnv->pLog->LogError("Error: LvlRes_finalstep file open '%s' failed",sFilePath.c_str());
			}
		} while (gEnv->pCryPak->FindNext( handle, &fd ) >= 0);

		gEnv->pCryPak->FindClose(handle);
	}

	bool IsFileKnown( const char *szFilePath )
	{
		string sFilePath = szFilePath;
		
		return m_UniqueFileList.find(sFilePath)!=m_UniqueFileList.end();
	}

protected: // -------------------------------------------------------------------------

	static bool HasRightExtension( const char *szFileName )
	{
		const char *szLvlResExt=szFileName;			

		size_t lenName = strlen(szLvlResExt);
		static size_t lenLvlExt = strlen(g_szLvlResExt);

		if(lenName>=lenLvlExt)
			szLvlResExt+=lenName-lenLvlExt;			// "test_LvlRes.txt" -> "_LvlRes.txt"

		return stricmp(szLvlResExt,g_szLvlResExt)==0;
	}

	// Arguments
	//   sFilePath - e.g. "game/object/vehices/car01.dds"
	void OnFileEntry( const char *szFilePath )
	{
		string sFilePath = szFilePath;
		if(m_UniqueFileList.find(sFilePath)==m_UniqueFileList.end())		// to to file processing only once per file
		{
			m_UniqueFileList.insert(sFilePath);

			ProcessFile(sFilePath);

			gEnv->pLog->UpdateLoadingScreen(0);
		}
	}

	virtual void ProcessFile( const string &sFilePath )=0;

	virtual void OnPakEntry( const string &sPath, const char *szPak ) {}

// -----------------------------------------------------------------

	std::set<string>				m_UniqueFileList;				// to removed duplicate files
};





class CLvlRes_finalstep :public CLvlRes_base
{
public:

	// constructor
	CLvlRes_finalstep( const char *szPath ) :m_sPath(szPath)
	{
		assert(szPath);
	}

	// destructor
	virtual ~CLvlRes_finalstep()
	{
		// free registered paks
		std::set<string>::iterator it, end=m_RegisteredPakFiles.end();

		for(it=m_RegisteredPakFiles.begin();it!=end;++it)
		{
			string sName = *it;

			gEnv->pCryPak->ClosePack(sName.c_str());
		}
	}

	// a pak file so all files within do not become file entries but the pak file becomes
	void RegisterPak( const string &sPath, const char *szFile )
	{
		string sPak = ConcatPath(sPath,szFile);

		gEnv->pCryPak->ClosePack(sPak.c_str());			// so we don't get error for paks that were already opened

		if(!gEnv->pCryPak->OpenPack(sPak.c_str()))
		{
			CryLog("RegisterPak '%s' failed - file not present?", sPak.c_str());
			return;
		}

		enum {nMaxPath = 0x800};
		char szAbsPathBuf[nMaxPath];

		const char *szAbsPath = gEnv->pCryPak->AdjustFileName(sPak.c_str(),szAbsPathBuf,0);

//		string sAbsPath = PathUtil::RemoveSlash(PathUtil::GetPath(szAbsPath));

		// debug
		CryLog("RegisterPak '%s'", szAbsPath);

		m_RegisteredPakFiles.insert(string(szAbsPath));

		OnFileEntry(sPak.c_str());		// include pak as file entry
	}

	// finds a specific file
	static bool FindFile( const char *szFilePath, const char *szFile, _finddata_t &fd )
	{
		intptr_t handle = gEnv->pCryPak->FindFirst(szFilePath, &fd );

		if(handle<0)
			return false;

		do
		{
			if(stricmp(fd.name,szFile)==0)
			{
				gEnv->pCryPak->FindClose(handle);
				return true;
			}
		} while(gEnv->pCryPak->FindNext(handle,&fd));

		gEnv->pCryPak->FindClose(handle);
		return false;
	}

	// slow but safe (to correct path and file name upper/lower case to the existing files)
	// some code might rely on the case (e.g. CVarGroup creation) so it's better to correct the case
	static void CorrectCaseInPlace( char *szFilePath )
	{
		// required for FindFirst, TODO: investigate as this seems wrong behavior
		{
			// jump over "Game"
			while(*szFilePath!='/' && *szFilePath!='\\' && *szFilePath!=0)
				++szFilePath;
			// jump over "/"
			if(*szFilePath!=0)
				++szFilePath;
		}

		char *szFile=szFilePath, *p=szFilePath;

		for(;;)
		{
			if(*p=='/' || *p=='\\' || *p==0)
			{
				char cOldChar = *p;		*p=0;			// create zero termination
				_finddata_t fd;

				bool bOk = FindFile(szFilePath,szFile,fd);

				if(bOk)
					assert(strlen(szFile)==strlen(fd.name));
				
				*p = cOldChar;									// get back the old separator

				if(!bOk)
					return;

				
				memcpy((void *)szFile,fd.name,strlen(fd.name));		// set

				if(*p==0)
					break;

				++p;szFile=p;
			}
			else ++p;
		}
	}

	virtual void ProcessFile( const string &_sFilePath )
	{
		string sFilePath=_sFilePath;

		CorrectCaseInPlace((char *)&sFilePath[0]);

		gEnv->pLog->LogWithType(ILog::eAlways,"LvlRes: %s",sFilePath.c_str());

		CCryFile file;
		std::vector<char> data;		

		if(!file.Open( sFilePath.c_str(),"rb" ))
		{
			OutputDebugString(">>>>> failed to open '");
			OutputDebugString(sFilePath.c_str());
			OutputDebugString("'\n");
			//			gEnv->pLog->LogError("ERROR: failed to open '%s'",sFilePath.c_str());			// pak not opened ?
			//			assert(0);
			return;
		}

		if(IsInRegisteredPak(file.GetHandle()))
			return;					// then don't process as we include the pak

		// Save this file in target folder.
		string trgFilename = PathUtil::Make( m_sPath,sFilePath );
		int fsize = file.GetLength();

		size_t len = file.GetLength();

		if (fsize > data.size())
			data.resize(fsize + 16);

		// Read data.
		file.ReadRaw( &data[0],fsize );

		// Save this data to target file.
		string trgFileDir = PathUtil::ToDosPath(PathUtil::RemoveSlash(PathUtil::GetPath(trgFilename)));	

		CreateDirectoryPath( trgFileDir.c_str() );		// ensure path exists

		// Create target file
		FILE *trgFile = fopen( trgFilename.c_str(),"wb" );

		if (trgFile)
		{
			fwrite(&data[0],fsize,1,trgFile);
			fclose(trgFile);
		}
		else
		{
			gEnv->pLog->LogError("ERROR: failed to write '%s' (write protected/disk full/rights)",trgFilename.c_str());
			assert(0);
		}
	}

	bool IsInRegisteredPak( FILE *hFile )
	{
		const char *szPak = gEnv->pCryPak->GetFileArchivePath(hFile);

		if(!szPak)
			return false;			// outside pak

		bool bInsideRegisteredPak = m_RegisteredPakFiles.find(szPak)!=m_RegisteredPakFiles.end();

		return bInsideRegisteredPak;
	}

	virtual void OnPakEntry( const string &sPath, const char *szPak )
	{
		RegisterPak(sPath,"level.pak");
		RegisterPak(sPath,"levellm.pak");
	}

	// -------------------------------------------------------------------------------

	string									m_sPath;								// directory path to store the assets e.g. "c:\temp\Out"
	std::set<string>				m_RegisteredPakFiles;		// abs path to pak files we registered e.g. "c:\MasterCD\game\GameData.pak", to avoid processing files inside these pak files - the ones we anyway want to include
};


class CLvlRes_findunused :public CLvlRes_base
{
public:

	virtual void ProcessFile( const string &sFilePath )
	{
	}
};






static void LvlRes_finalstep( IConsoleCmdArgs *pParams )
{
	assert(pParams);

	uint32 dwCnt = pParams->GetArgCount();

	if(dwCnt!=2)
	{
		gEnv->pLog->LogWithType(ILog::eError,"ERROR: sys_LvlRes_finalstep requires destination path as parameter");
		return;
	}

	const char *szPath = pParams->GetArg(1);			assert(szPath);

	gEnv->pLog->LogWithType(ILog::eInputResponse,"sys_LvlRes_finalstep %s ...",szPath);

	// open console
	gEnv->pConsole->ShowConsole(true);

	CLvlRes_finalstep sink(szPath);

	sink.RegisterPak("Game","GameData.pak");
	sink.RegisterPak("Game","Shaders.pak");

	sink.RegisterAllLevelPaks("Game/levels");
	sink.Process("Game/levels");
}




static void _LvlRes_findunused_recursive( CLvlRes_findunused &sink, const string &sPath,
	uint32 &dwUnused, uint32 &dwAll )
{
	_finddata_t fd;

	string sPathPattern = ConcatPath(sPath,"*.*");

	// ignore some directories
	if(stricmp(sPath.c_str(),"Shaders")==0
	|| stricmp(sPath.c_str(),"ScreenShots")==0
	|| stricmp(sPath.c_str(),"Scripts")==0
	|| stricmp(sPath.c_str(),"Config")==0
	|| stricmp(sPath.c_str(),"LowSpec")==0)
		return;

//	gEnv->pLog->Log("_LvlRes_findunused_recursive '%s'",sPath.c_str());

	intptr_t handle = gEnv->pCryPak->FindFirst(sPathPattern.c_str(), &fd );

	if(handle<0)
	{
		gEnv->pLog->LogError("ERROR: _LvlRes_findunused_recursive failed '%s'",sPathPattern.c_str());
		return;
	}

	do 
	{
		if(fd.attrib&_A_SUBDIR)
		{
			if(strcmp(fd.name,".")!=0 && strcmp(fd.name,"..")!=0)
				_LvlRes_findunused_recursive(sink,ConcatPath(sPath,fd.name),dwUnused,dwAll);
		}
		else
		{
			/*
			// ignore some extensions
			if(stricmp(PathUtil::GetExt(fd.name),"cry")==0)
				continue;

			// ignore some files
			if(stricmp(fd.name,"TerrainTexture.pak")==0)
				continue;
			*/

			string sFilePath = CryStringUtils::toLower(ConcatPath(sPath,fd.name));
			enum {nMaxPath = 0x800};
			char szAbsPathBuf[nMaxPath];

			gEnv->pCryPak->AdjustFileName(sFilePath.c_str(),szAbsPathBuf,0);

			if(!sink.IsFileKnown(szAbsPathBuf))
			{
				gEnv->pLog->LogWithType(ILog::eAlways,"%d, %s",(uint32)fd.size,szAbsPathBuf);
				++dwUnused;
			}
			++dwAll;
		}
	} while (gEnv->pCryPak->FindNext( handle, &fd ) >= 0);

	gEnv->pCryPak->FindClose(handle);
}


static void LvlRes_findunused( IConsoleCmdArgs *pParams )
{
	assert(pParams);

	gEnv->pLog->LogWithType(ILog::eInputResponse,"sys_LvlRes_findunused ...");

	// open console
	gEnv->pConsole->ShowConsole(true);

	CLvlRes_findunused sink;

	sink.RegisterAllLevelPaks("Game/levels");
	sink.Process("Game/levels");

	gEnv->pLog->LogWithType(ILog::eInputResponse,"");
	gEnv->pLog->LogWithType(ILog::eInputResponse,"Assets not used by the existing LvlRes data:");
	gEnv->pLog->LogWithType(ILog::eInputResponse,"");
	
	char rootpath[_MAX_PATH];
	CryGetCurrentDirectory(sizeof(rootpath),rootpath);

	gEnv->pLog->LogWithType(ILog::eInputResponse,"Folder: %s",rootpath);

	uint32 dwUnused=0, dwAll=0;

	_LvlRes_findunused_recursive(sink,"",dwUnused,dwAll);

	gEnv->pLog->LogWithType(ILog::eInputResponse," ");
	gEnv->pLog->LogWithType(ILog::eInputResponse,"Unused assets: %d/%d",dwUnused,dwAll);
	gEnv->pLog->LogWithType(ILog::eInputResponse," ");
}

// --------------------------------------------------------------------------------------------------------------------------


static void ScreenshotCmd( IConsoleCmdArgs *pParams )
{
	assert(pParams);

	uint32 dwCnt = pParams->GetArgCount();

	if(dwCnt<=1)
	{
		if(!gEnv->pSystem->IsEditorMode())
		{
			// open console one line only
			gEnv->pConsole->ShowConsole(true,16);
			gEnv->pConsole->SetInputLine("Screenshot ");
		}
		else
		{
			gEnv->pLog->LogWithType(ILog::eInputResponse,"Screenshot <annotation> missing - no screenshot was done");
		}
	}
	else
	{
		static int iScreenshotNumber=-1;

		const char *szPrefix="Screenshot";
		uint32 dwPrefixSize=strlen(szPrefix);

		char path[ICryPak::g_nMaxPath];
		path[sizeof(path) - 1] = 0;
		gEnv->pCryPak->AdjustFileName("%USER%/ScreenShots", path, ICryPak::FLAGS_NO_MASTER_FOLDER_MAPPING | ICryPak::FLAGS_FOR_WRITING);

		if(iScreenshotNumber==-1)		// first time - find max number to start
		{
			ICryPak *pCryPak = gEnv->pCryPak;
			_finddata_t fd;

			intptr_t handle = pCryPak->FindFirst((string(path)+"/*.*").c_str(),&fd );		// mastercd folder
			if (handle != -1)
			{
				int res = 0;
				do
				{
					int iCurScreenshotNumber;

					if(strnicmp(fd.name,szPrefix,dwPrefixSize)==0)
					{
						int iCnt = sscanf(fd.name+10,"%d",&iCurScreenshotNumber);

						if(iCnt)
							iScreenshotNumber=max(iCurScreenshotNumber,iScreenshotNumber);
					}

					res = pCryPak->FindNext( handle,&fd );
				} while (res >= 0);
				pCryPak->FindClose(handle);
			}
		}

		++iScreenshotNumber;

		char szNumber[16];
		sprintf(szNumber,"%.4d ",iScreenshotNumber);

		string sScreenshotName = string(szPrefix)+szNumber;

		for(uint32 dwI=1;dwI<dwCnt;++dwI)
		{
			if(dwI>1)
				sScreenshotName += "_";

			sScreenshotName += pParams->GetArg(dwI);
		}

		sScreenshotName.replace("\\","_");
		sScreenshotName.replace("/","_");
		sScreenshotName.replace(":","_");
		sScreenshotName.replace(".","_");

		gEnv->pConsole->ShowConsole(false);

		CSystem *pCSystem = (CSystem *)(gEnv->pSystem);
		pCSystem->GetDelayedScreeenshot() = string(path)+"/"+sScreenshotName;// to delay a screenshot call for a frame
	}
}




//////////////////////////////////////////////////////////////////////////
bool CSystem::IsEditorMode()
{
	if (!gEnv->pGame || !gEnv->pGame->GetIGameFramework())
		return IsEditor();
	else
		return IsEditor() && gEnv->pGame->GetIGameFramework()->IsEditing();
}




static void SysRestoreSpecCmd( IConsoleCmdArgs *pParams )
{
	assert(pParams);

	if(pParams->GetArgCount()==2)
	{
		const char *szArg = pParams->GetArg(1);

		ICVar *pCVar = gEnv->pConsole->GetCVar("sys_spec_Full");			

		if(!pCVar)
		{
			CryLog("sys_RestoreSpec: no action");		// e.g. running Editor in shder compile mode 
			return;
		}

		ICVar::EConsoleLogMode mode=ICVar::eCLM_Off;

		if(stricmp(szArg,"test")==0)
			mode= ICVar::eCLM_ConsoleAndFile;
		 else if(stricmp(szArg,"test*")==0)
			mode= ICVar::eCLM_FileOnly;
		 else if(stricmp(szArg,"info")==0)
			mode= ICVar::eCLM_FullInfo;

		if(mode!=ICVar::eCLM_Off)
		{
			bool bFileOrConsole = (mode==ICVar::eCLM_FileOnly || mode==ICVar::eCLM_FullInfo);

			if(bFileOrConsole)
				gEnv->pLog->LogToFile(" ");
			 else
				CryLog(" ");

			int iSysSpec = pCVar->GetRealIVal();

			if(iSysSpec==-1)
			{
				iSysSpec = ((CSystem*)gEnv->pSystem)->GetMaxConfigSpec();

				if(bFileOrConsole)
					gEnv->pLog->LogToFile("   sys_spec = Custom (assuming %d)",iSysSpec);
				 else
					CryLog("   $3sys_spec = $6Custom (assuming %d)",iSysSpec);
			}
			else
			{
				if(bFileOrConsole)
					gEnv->pLog->LogToFile("   sys_spec = %d",iSysSpec);
				 else
					CryLog("   $3sys_spec = $6%d",iSysSpec);
			}

			pCVar->DebugLog(iSysSpec,mode);

			if(bFileOrConsole)
				gEnv->pLog->LogToFile(" ");
			 else
				CryLog(" ");

			return;
		}
		else if(strcmp(szArg,"apply")==0)
		{
			const char *szPrefix = "sys_spec_";

			std::vector<const char*> cmds;

			cmds.resize(gEnv->pConsole->GetSortedVars(0,0,szPrefix));
			gEnv->pConsole->GetSortedVars(&cmds[0],cmds.size(),szPrefix);

			gEnv->pLog->LogWithType(ILog::eInputResponse,"");

			std::vector<const char*>::const_iterator it, end=cmds.end();

			for(it=cmds.begin();it!=end;++it)
			{
				const char *szName = *it;

				if(stricmp(szName,"sys_spec_Full")==0)
					continue;

				ICVar *pCVar = gEnv->pConsole->GetCVar(szName);			assert(pCVar);

				if(!pCVar)
					continue;

				bool bNeeded = pCVar->GetIVal()!=pCVar->GetRealIVal();

				gEnv->pLog->LogWithType(ILog::eInputResponse," $3%s = $6%d ... %s",
					szName,pCVar->GetIVal(),
					bNeeded?"$4restored":"valid");

				if(bNeeded)
					pCVar->Set(pCVar->GetIVal());
			}

			gEnv->pLog->LogWithType(ILog::eInputResponse,"");
			return;
		}
	}

	gEnv->pLog->LogWithType(ILog::eInputResponse,"ERROR: sys_RestoreSpec invalid arguments");
}

void ChangeLogAllocations( ICVar* pVal )
{
	g_bTraceAllocations = pVal->GetIVal() > 0;
}

//////////////////////////////////////////////////////////////////////////
void CSystem::CreateSystemVars()
{
	m_pCVarQuit = GetIConsole()->RegisterInt("ExitOnQuit",1,0);

	m_cvAIUpdate = GetIConsole()->RegisterInt("ai_NoUpdate",0,VF_CHEAT);

	m_iTraceAllocations = g_bTraceAllocations;
	GetIConsole()->Register( "sys_logallocations", &m_iTraceAllocations, m_iTraceAllocations, VF_DUMPTODISK, "Save allocation call stack", ChangeLogAllocations);

	m_cvMemStats = GetIConsole()->RegisterInt("MemStats", 0, 0,
		"0/x=refresh rate in milliseconds\n"
		"Use 1000 to switch on and 0 to switch off\n"
		"Usage: MemStats [0..]\n");
	m_cvMemStatsThreshold = GetIConsole()->RegisterInt ("MemStatsThreshold", 32000, 0);
	m_cvMemStatsMaxDepth = GetIConsole()->RegisterInt("MemStatsMaxDepth", 4, 0);

	m_sys_SaveCVars =  GetIConsole()->RegisterInt("sys_SaveCVars", 0, 0,
		"1 to activate saving of console variables, 0 to deactivate\n"
		"The variables are stored in 'system.cfg' on quit, only marked variables are saved (0)\n"
		"Usage: sys_SaveCVars [0/1]\n"
		"Default is 0");
	m_sys_StreamCallbackTimeBudget = GetIConsole()->RegisterInt("sys_StreamCallbackTimeBudget", 50000, 0,
		"Time budget, in microseconds, to be spent every frame in StreamEngine callbacks.\n"
		"Additive with cap: if more time is spent, the next frame gets less budget, and\n"
		"there's never more than this value per frame.");
	
	m_PakVar.nPriority  = 0;
	m_PakVar.nReadSlice = 0;
	m_PakVar.nLogMissingFiles = 0;
	m_cvPakPriority = attachVariable("sys_PakPriority", &m_PakVar.nPriority,"If set to 1, tells CryPak to try to open the file in pak first, then go to file system",VF_READONLY|VF_CHEAT);
	m_cvPakReadSlice = attachVariable("sys_PakReadSlice", &m_PakVar.nReadSlice,"If non-0, means number of kilobytes to use to read files in portions. Should only be used on Win9x kernels");
	m_cvPakLogMissingFiles = attachVariable("sys_PakLogMissingFiles",&m_PakVar.nLogMissingFiles, 
		"If non-0, missing file names go to mastercd/MissingFilesX.log.\n"
		"1) only resulting report\n"
		"2) run-time report is ON, one entry per file\n"
		"3) full run-time report");

	// fix to solve a flaw in the system
	// 0 (default) needed for game, 1 better for having artifact free movie playback (camera pos is valid during entity update)
	m_cvEarlyMovieUpdate = GetIConsole()->RegisterInt("sys_EarlyMovieUpdate",0,0,
		"0 needed for game, 1 better for having artifact free movie playback\n"
		"Usage: sys_EarlyMovieUpdate [0/1]\n"
		"Default is 0");

	m_sysNoUpdate = GetIConsole()->RegisterInt("sys_noupdate",0,VF_CHEAT,
		"Toggles updating of system with sys_script_debugger.\n"
		"Usage: sys_noupdate [0/1]\n"
		"Default is 0 (system updates during debug).");

	m_sysWarnings = GetIConsole()->RegisterInt("sys_warnings",0,0,
		"Toggles printing system warnings.\n"
		"Usage: sys_warnings [0/1]\n"
		"Default is 0 (off).");

	m_svDedicatedMaxRate = GetIConsole()->RegisterFloat("sv_DedicatedMaxRate",30.0f,0,
		"Sets the maximum update rate when running as a dedicated server.\n"
		"Usage: sv_DedicatedMaxRate [5..500]\n"
		"Default is 50.");
	GetIConsole()->RegisterFloat("sv_DedicatedCPUPercent",0.0f,0,
		"Sets the target CPU usage when running as a dedicated server, or disable this feature if it's zero.\n"
		"Usage: sv_DedicatedCPUPercent [0..100]\n"
		"Default is 0 (disabled).");
	GetIConsole()->RegisterFloat("sv_DedicatedCPUVariance",10.0f,0,
		"Sets how much the CPU can vary from sv_DedicateCPU (up or down) without adjusting the framerate.\n"
		"Usage: sv_DedicatedCPUVariance [5..50]\n"
		"Default is 10.");
	m_sysAISystem = GetIConsole()->RegisterInt("sys_AISystem", 1, VF_REQUIRE_APP_RESTART, "Load and use the AI system");

	m_cvSSInfo =  GetIConsole()->RegisterInt("sys_SSInfo",0,0,
		"Show SourceSafe information (Name,Comment,Date) for file errors."
		"Usage: sys_SSInfo [0/1]\n"
		"Default is 0 (off).");

	m_cvEntitySuppressionLevel = GetIConsole()->RegisterInt("e_EntitySuppressionLevel",0,0,
		"Defines the level at which entities are spawned.\n"
		"Entities marked with lower level will not be spawned - 0 means no level.\n"
		"Usage: e_EntitySuppressionLevel [0-infinity]\n"
		"Default is 0 (off).");

	m_sys_profile = GetIConsole()->RegisterInt("profile",0,0,"Enable profiling.\n"
		"Usage: profile #\n"
		"Where # sets the profiling to:\n"
		"	0: Profiling off\n"
		"	1: Self Time\n"
		"	2: Hierarchical Time\n"
		"	3: Extended Self Time\n"
		"	4: Extended Hierarchical Time\n"
		"	5: Peaks Time\n"
		"	6: Subsystem Info\n"
		"	7: Calls Numbers\n"
		"	8: Standard Deviation\n"
		"	-1: Profiling enabled, but not displayed\n"
		"Default is 0 (off).");
	m_sys_profile_filter = GetIConsole()->RegisterString("profile_filter","",0,
		"Profiles a specified subsystem.\n"
		"Usage: profile_filter subsystem\n"
		"Where 'subsystem' may be:\n"
		"Renderer\n"
		"3DEngine\n"
		"Animation\n"
		"AI\n"
		"Entity\n"
		"Physics\n"
		"Sound\n"
		"System\n"
		"Game\n"
		"Editor\n"
		"Script\n"
		"Network\n"
		);
	m_sys_profile_graph = GetIConsole()->RegisterInt("profile_graph",0,0,
		"Enable drawing of profiling graph.\n");
	m_sys_profile_graphScale = GetIConsole()->RegisterFloat("profile_graphScale",100.0f,0,
		"Sets the scale of profiling histograms.\n"
		"Usage: profileGraphScale 100\n");
	m_sys_profile_pagefaultsgraph = GetIConsole()->RegisterInt("profile_pagefaults",0,0,
		"Enable drawing of page faults graph.\n");
	m_sys_profile_allThreads = GetIConsole()->RegisterInt("profile_allthreads",0,0,
		"Enables profiling of non-main threads.\n" );
	m_sys_profile_network = GetIConsole()->RegisterInt("profile_network",0,0,
		"Enables network profiling.\n" );
	m_sys_profile_peak = GetIConsole()->RegisterFloat("profile_peak",10.0f,0,
		"Profiler Peaks Tolerance in Milliseconds.\n" );
	m_sys_profile_memory = GetIConsole()->RegisterInt("MemInfo",0,0,"Display memory information by modules" );

	m_sys_profile_sampler = GetIConsole()->RegisterFloat("profile_sampler", 0, 0, 
		"Set to 1 to start sampling profiling");
	m_sys_profile_sampler_max_samples = GetIConsole()->RegisterFloat("profile_sampler_max_samples", 2000, 0, 
		"Number of samples to collect for sampling profiler");
#if defined(PS3)
	m_sys_spu_profile = GetIConsole()->RegisterInt("spu_profile",0,0,
		"Switch profiling between PPU (0) / SPU (1).");
	m_sys_spu_enable = GetIConsole()->RegisterInt("spu_enable",0,0,	"Enable SPU Jobs");
	m_sys_spu_debug	= GetIConsole()->RegisterString("spu_debug","",0,
		"Enables debugging of a SPU Job.\n"
		"Usage: spu_debug name\n"
		"Where 'name' refers to the exact name of the SPU job");
	m_sys_spu_dump_stats = GetIConsole()->RegisterInt("spu_dump_stats",0,0,	"Dump SPU job stats for frame.");
#endif
	m_sys_spec=GetIConsole()->RegisterInt("sys_spec",CONFIG_CUSTOM,0,		// starts with CONFIG_CUSTOM so callback is called when setting inital value
		"Tells the system cfg spec. (0=custom, 1=low, 2=med, 3=high, 4=veryhigh)",OnSysSpecChange );

	m_sys_firstlaunch = GetIConsole()->RegisterInt( "sys_firstlaunch", 0, 0,
		"Indicates that the game was run for the first time." );

	m_sys_physics_CPU = GetIConsole()->RegisterInt("sys_physics_CPU", 1, 0, 
		"Specifies the physical CPU index physics will run on");
	m_sys_min_step = GetIConsole()->RegisterFloat("sys_min_step", 0.01f, 0, 
		"Specifies the minimum physics step in a separate thread");
	m_sys_max_step = GetIConsole()->RegisterFloat("sys_max_step", 0.05f, 0, 
		"Specifies the maximum physics step in a separate thread");

	m_sys_enable_budgetmonitoring = GetIConsole()->RegisterInt( "sys_enable_budgetmonitoring", 0, 0,
		"Enables budget monitoring. Use #System.SetBudget( sysMemLimitInMB, videoMemLimitInMB,\n"
		"frameTimeLimitInMS, soundChannelsPlaying ) or sys_budget_sysmem, sys_budget_videomem\n"
		"or sys_budget_fps to set budget limits.");

	// used in define MEMORY_DEBUG_POINT()
	m_sys_memory_debug=m_env.pConsole->RegisterInt("sys_memory_debug",0,VF_CHEAT,
		"Enables to activate low memory situation is specific places in the code (argument defines which place), 0=off");

	m_env.pConsole->Register( "sys_vtune",&g_cvars.sys_vtune,0,0 );
	m_env.pConsole->Register( "sys_streaming_sleep",&g_cvars.sys_streaming_sleep,0,0 );
	m_env.pConsole->Register( "sys_float_exceptions",&g_cvars.sys_float_exceptions,0,0,"Use or not use floating point exceptions.\n" );

	m_env.pConsole->Register( "sys_no_crash_dialog",&g_cvars.sys_no_crash_dialog,0,0 );
	m_env.pConsole->Register( "sys_WER",&g_cvars.sys_WER,0,0,"Enables Windows Error Reporting" );

	m_sys_preload = m_env.pConsole->RegisterInt("sys_preload",0,0,"Preload Game Resources" );
	m_sys_crashtest = m_env.pConsole->RegisterInt("sys_crashtest",0,0 );

	m_env.pConsole->RegisterInt( "capture_frames", 0, 0, "Enables capturing of frames." );
	m_env.pConsole->RegisterString( "capture_folder", "CaptureOutput", 0, "Specifies sub folder to write captured frames." );
	m_env.pConsole->RegisterString( "capture_file_format", "jpg", 0, "Specifies file format of captured files (jpg, bmp, tga, hdr)." );

	m_gpu_particle_physics = m_env.pConsole->RegisterInt("gpu_particle_physics", 0, VF_REQUIRE_APP_RESTART, "Enable GPU physics if available (0=off / 1=enabled).");

	m_env.pConsole->AddCommand("LoadConfig",&LoadConfigurationCmd,0,
		"Load .cfg file from disk (from the Game/Config directory)\n"
		"e.g. LoadConfig lowspec.cfg\n"
		"Usage: LoadConfig <filename>");

	m_env.pConsole->CreateKeyBind("alt f12", "Screenshot");

	// screenshot functionality in system as console 	
	m_env.pConsole->AddCommand("Screenshot",&ScreenshotCmd,0,
		"Create a screenshot with annotation\n"
		"e.g. Screenshot beach scene with shark\n"
		"Usage: Screenshot <annotation text>");

	m_env.pConsole->AddCommand("sys_LvlRes_finalstep",&LvlRes_finalstep,0,"to combine all recorded level resources and create finial stripped build");
	m_env.pConsole->AddCommand("sys_LvlRes_findunused",&LvlRes_findunused,0,"find unused level resources");
/*
	// experimental feature? - needs to be created very early
	m_sys_filecache = m_env.pConsole->RegisterInt("sys_FileCache",0,0,
		"To speed up loading from non HD media\n"
		"0=off / 1=enabled");
*/
	m_env.pConsole->Register( "sys_AI",&g_cvars.sys_ai,1,0,"Enables AI Update" );
	m_env.pConsole->Register( "sys_physics",&g_cvars.sys_physics,1,0,"Enables Physics Update" );
	m_env.pConsole->Register( "sys_entities",&g_cvars.sys_entitysystem,1,0,"Enables Entities Update" );
	m_env.pConsole->Register( "sys_trackview",&g_cvars.sys_trackview,1,0,"Enables TrackView Update" );

	m_env.pConsole->RegisterString( "g_language", "", 0);

	gEnv->pConsole->AddCommand("sys_RestoreSpec",&SysRestoreSpecCmd,0,
		"Restore or test the cvar settings of game specific spec settings,\n"
		"'test*' and 'info' log to the log file only\n"
		"Usage: sys_RestoreSpec [test|test*|apply|info]");

	m_env.pConsole->RegisterString("sys_root", m_root.c_str(),VF_READONLY);
}




/////////////////////////////////////////////////////////////////////
void CSystem::AddCVarGroupDirectory( const string &sPath )
{
	CryLog("creating CVarGroups from directory '%s' ...",sPath.c_str());

	_finddata_t fd;

	intptr_t handle = gEnv->pCryPak->FindFirst(ConcatPath(sPath,"*.cfg").c_str(), &fd );

	if(handle<0)
		return;

	do 
	{
		if(fd.attrib&_A_SUBDIR)
		{
			if(strcmp(fd.name,".")!=0 && strcmp(fd.name,"..")!=0)
				AddCVarGroupDirectory(ConcatPath(sPath,fd.name));
		}
		else
		{
			string sFilePath = ConcatPath(sPath,fd.name);

			string sCVarName = sFilePath;
			PathUtil::RemoveExtension(sCVarName);

			((CXConsole*)m_env.pConsole)->RegisterCVarGroup(PathUtil::GetFile(sCVarName).c_str(),sFilePath.c_str());
		}
	} while (gEnv->pCryPak->FindNext( handle, &fd ) >= 0);

	gEnv->pCryPak->FindClose(handle);
}

