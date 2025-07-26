#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <string_view>
#include <filesystem>
#include <functional>
#include <map>

namespace WinAPI
{
	inline constexpr std::string_view NEWLINE = "\r\n";

	/////////////
	// Utility //
	/////////////

	inline void *RVA(void *base, size_t offset)
	{
		return static_cast<uint8_t*>(base) + offset;
	}

	inline const void *RVA(const void *base, size_t offset)
	{
		return static_cast<const uint8_t*>(base) + offset;
	}

	//////////////////
	// Command line //
	//////////////////

	namespace CmdLine
	{
		const char* GetFull();
		const char* GetOnlyArgs();

		bool HasArg(const char* arg);

		const char* GetArgValue(const char* arg, const char* defaultValue = "");
	}

	////////////
	// Errors //
	////////////

	unsigned long GetCurrentErrorCode();

	///////////
	// Paths //
	///////////

	std::filesystem::path GetApplicationPath();

	void SetWorkingDirectory(const std::filesystem::path& path);

	uint64_t GetLastWriteTime(const std::filesystem::path& path);

	/////////////
	// Modules //
	/////////////

	namespace DLL
	{
		void AddSearchDirectory(const std::filesystem::path& path);

		void* Get(const char* name);
		void* Load(const char* name);
		void* GetSymbol(void* pDLL, const char* name);
		void Unload(void* pDLL);
	}

	/////////////////
	// Message box //
	/////////////////

	void ErrorBox(const char *message);

	///////////////
	// Resources //
	///////////////

	struct VersionResource
	{
		unsigned short major = 0;
		unsigned short minor = 0;
		unsigned short patch = 0;
		unsigned short tweak = 0;
	};

	bool GetVersionResource(void* pDLL, VersionResource& result);

	std::string_view GetDataResource(void *pDLL, int resourceID);

	///////////
	// Hacks //
	///////////

	void FillNOP(void *address, size_t length);
	void FillMem(void *address, const void *data, size_t length);

	void HookWithJump(void* address, void* pNewFunc);

	bool HookIATByAddress(void *pDLL, void *pFunc, void *pNewFunc);
	bool HookIATByName(void *pDLL, const char *dllName, const char *funcName, void *pNewFunc);

	//////////
	// Time //
	//////////

	struct DateTime
	{
		unsigned short year        = 0;
		unsigned short month       = 0;  // January = 1, February = 2, ..., December = 12
		unsigned short dayOfWeek   = 0;  // Sunday = 0, Monday = 1, ..., Saturday = 6
		unsigned short day         = 0;
		unsigned short hour        = 0;
		unsigned short minute      = 0;
		unsigned short second      = 0;
		unsigned short millisecond = 0;

		const char *GetDayName();
		const char *GetMonthName();
	};

	DateTime GetCurrentDateTimeUTC();
	DateTime GetCurrentDateTimeLocal();

	long GetTimeZoneBias();

	/////////////
	// Strings //
	/////////////

	std::size_t RawANSIToWide(const char* string, std::size_t stringSize, wchar_t* buffer, std::size_t bufferSize);

	wchar_t WideCharToLower(wchar_t ch, int languageID = 0);
	wchar_t WideCharToUpper(wchar_t ch, int languageID = 0);

	/////////////////
	// System info //
	/////////////////

	std::string GetMachineGUID();
	std::string GetLocale();
	std::string GetLocalIP();

	bool IsVistaOrLater();

	unsigned int GetLogicalProcessorCount();

	std::size_t GetSystemLanguageCode(char* buffer, std::size_t bufferSize);

	/////////////
	// Network //
	////////////
	std::string GetIP(const std::string& hostName);

	//////////
	// HTTP //
	//////////

	using HTTPRequestReader = std::function<size_t(void*,size_t)>;  // buffer, buffer size, returns data length
	using HTTPRequestCallback = std::function<void(uint64_t,const HTTPRequestReader&)>;  // content length, reader

	// blocking, returns HTTP status code, throws CryMP_Error
	int HTTPRequest(
		const std::string_view & method,
		const std::string_view & url,
		const std::string_view & data,
		const std::map<std::string, std::string> & headers,
		HTTPRequestCallback callback
	);

	///////////////
	// Clipboard //
	///////////////

	std::string GetClipboardText(std::size_t maxLength = std::size_t(-1));

	////////////
	// Cursor //
	////////////

	namespace Cursor
	{
		void GetPos(long& x, long& y);
		void SetPos(long x, long y);

		void Show(bool show);

		void Clip(void* window);
	}

	////////////
	// Window //
	////////////

	namespace Window
	{
		void ConvertPosToWindow(void* window, long& x, long& y);
		void ConvertPosToScreen(void* window, long& x, long& y);
	}
}
