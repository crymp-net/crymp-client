#ifdef CRYMP_CONSOLE_APP
#include <cstdio>
#endif

#include "Library/StringTools.h"  // CryMP_Error
#include "Library/WinAPI.h"

#include "Launcher.h"

#include "config.h"

////////////////////////////////////////////////////////////////////////////////
// Request fast graphics card
////////////////////////////////////////////////////////////////////////////////

#ifdef CLIENT_LAUNCHER
extern "C" __declspec(dllexport) unsigned long NvOptimusEnablement = 1;
extern "C" __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
#endif

////////////////////////////////////////////////////////////////////////////////

#ifdef CRYMP_CONSOLE_APP
int main()
#else
int __stdcall WinMain(void*, void*, char*, int)
#endif
{
#ifdef CODEWALL_DEBUG
	Launcher().Run();
#else
	try
	{
		Launcher().Run();
	}
	catch (const CryMP_Error& error)
	{
#ifdef CRYMP_CONSOLE_APP
		std::fprintf(stderr, "%s\n", error.what());
#else
		WinAPI::ErrorBox(error.what());
#endif
		return 1;
	}
#endif

	return 0;
}