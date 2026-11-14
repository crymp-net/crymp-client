#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>

#include <array>
#include <algorithm>
#include <string>
#include <string_view>

#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CrySystem/IConsole.h"
#include "Launcher/Launcher.h"
#include "Library/Util.h"
#include "Library/WinAPI.h"

#include "GSMasterHook.h"

extern void* g_pCryNetwork;

namespace
{
	ICVar *g_pHostnameCVar;

	constexpr std::array<std::string_view, 3> PREFIXES = {
		"crysis.available.",
		"crysis.master.",
		"crysis.ms5."
	};

	std::string_view FindPrefix(const std::string_view & name)
	{
		auto it = std::find_if(PREFIXES.begin(), PREFIXES.end(), [&name](const std::string_view & prefix)
		{
			return Util::StartsWith(name, prefix);
		});

		return (it != PREFIXES.end()) ? *it : std::string_view();
	}

	void* __stdcall gethostbyname_hook(const char *name)
	{
		if (g_pHostnameCVar)
		{
			const std::string_view prefix = FindPrefix(name);

			if (!prefix.empty())
			{
				std::string redirected;
				redirected += prefix;
				redirected += g_pHostnameCVar->GetString();

				CryLog("[CryMP] [GSMasterHook] Redirecting '%s' to '%s'", name, redirected.c_str());

				return gethostbyname(redirected.c_str());
			}

			CryLog("[CryMP] [GSMasterHook] Ignoring '%s'", name);
		}

		return gethostbyname(name);
	}
}

GSMasterHook::GSMasterHook()
{
	g_pHostnameCVar = gEnv->pConsole->RegisterString("cl_master", "m.crymp.org", VF_NOT_NET_SYNCED,
	                                                 "GameSpy master server hostname.");

	WinAPI::HookIATByAddress(g_pCryNetwork, gethostbyname, gethostbyname_hook);
}

GSMasterHook::~GSMasterHook()
{
}

ICVar *GSMasterHook::GetHostnameCVar()
{
	return g_pHostnameCVar;
}
