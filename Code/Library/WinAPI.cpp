#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <winhttp.h>

#include "WinAPI.h"
#include "StringTools.h"

//////////////////
// Command line //
//////////////////

static int FindArgIndex(const char* arg)
{
	for (int i = 1; i < __argc; i++)
	{
		if (_stricmp(__argv[i], arg) == 0)
		{
			return i;
		}
	}

	return -1;
}

const char* WinAPI::CmdLine::GetFull()
{
	return GetCommandLineA();
}

const char* WinAPI::CmdLine::GetOnlyArgs()
{
	const char* args = GetFull();
	char separator = ' ';

	if (*args == '"')
	{
		separator = '"';
		args++;
	}
	else if (*args == '\'')
	{
		separator = '\'';
		args++;
	}

	for (; *args; args++)
	{
		if (*args == separator)
		{
			args++;
			break;
		}
	}

	while (*args == ' ')
	{
		args++;
	}

	return args;
}

bool WinAPI::CmdLine::HasArg(const char* arg)
{
	return FindArgIndex(arg) > 0;
}

const char* WinAPI::CmdLine::GetArgValue(const char* arg, const char* defaultValue)
{
	const int index = FindArgIndex(arg);

	if (index < 0 || (index + 1) >= __argc)
	{
		return defaultValue;
	}
	else
	{
		const char* value = __argv[index + 1];

		if (*value == '-' || *value == '+')
		{
			return defaultValue;
		}
		else
		{
			return value;
		}
	}
}

////////////
// Errors //
////////////

unsigned long WinAPI::GetCurrentErrorCode()
{
	return GetLastError();
}

///////////
// Paths //
///////////

std::filesystem::path WinAPI::GetApplicationPath()
{
	constexpr DWORD BUFFER_SIZE = 1024;

	wchar_t buffer[BUFFER_SIZE];
	DWORD length = GetModuleFileNameW(nullptr, buffer, BUFFER_SIZE);
	if (length == 0)
	{
		throw StringTools::SysErrorFormat("GetModuleFileNameW");
	}
	else if (length >= BUFFER_SIZE)
	{
		throw StringTools::ErrorFormat("Application path is too long!");
	}

	return std::filesystem::path(buffer);
}

void WinAPI::SetWorkingDirectory(const std::filesystem::path& path)
{
	if (!SetCurrentDirectoryW(path.c_str()))
	{
		throw StringTools::SysErrorFormat("SetCurrentDirectoryW");
	}
}

uint64_t WinAPI::GetLastWriteTime(const std::filesystem::path& path)
{
	WIN32_FILE_ATTRIBUTE_DATA attributes;
	if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attributes))
	{
		return 0;
	}

	// https://learn.microsoft.com/en-us/windows/win32/sysinfo/converting-a-time-t-value-to-a-file-time
	ULARGE_INTEGER time_value;
	time_value.LowPart = attributes.ftLastWriteTime.dwLowDateTime;
	time_value.HighPart = attributes.ftLastWriteTime.dwHighDateTime;
	time_value.QuadPart -= 116444736000000000ULL;
	return time_value.QuadPart / 10000000ULL;
}

/////////////
// Modules //
/////////////

void WinAPI::DLL::AddSearchDirectory(const std::filesystem::path& path)
{
	if (!SetDllDirectoryW(path.c_str()))
	{
		throw StringTools::SysErrorFormat("SetDllDirectoryW");
	}
}

void *WinAPI::DLL::Get(const char* name)
{
	return GetModuleHandleA(name);
}

void *WinAPI::DLL::Load(const char* name)
{
	return LoadLibraryA(name);
}

void *WinAPI::DLL::GetSymbol(void* pDLL, const char* name)
{
	return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(pDLL), name));
}

void WinAPI::DLL::Unload(void* pDLL)
{
	FreeLibrary(static_cast<HMODULE>(pDLL));
}

/////////////////
// Message box //
/////////////////

void WinAPI::ErrorBox(const char *message)
{
	MessageBoxA(nullptr, message, "Error", MB_OK | MB_ICONERROR);
}

///////////////
// Resources //
///////////////

namespace
{
	std::string_view GetResource(void *pDLL, const char *name, const char *type)
	{
		HRSRC resourceInfo = FindResourceA(static_cast<HMODULE>(pDLL), name, type);
		if (!resourceInfo)
		{
			return {};
		}

		HGLOBAL resourceData = LoadResource(static_cast<HMODULE>(pDLL), resourceInfo);
		if (!resourceData)
		{
			return {};
		}

		const void *data = LockResource(resourceData);
		size_t length = SizeofResource(static_cast<HMODULE>(pDLL), resourceInfo);

		return std::string_view(static_cast<const char*>(data), length);
	}
}

bool WinAPI::GetVersionResource(void* pDLL, VersionResource& result)
{
	const void* versionRes = GetResource(pDLL, MAKEINTRESOURCE(VS_VERSION_INFO), RT_VERSION).data();
	if (!versionRes)
	{
		return false;
	}

	const void* versionResKey = static_cast<const unsigned char*>(versionRes) + 0x6;
	if (memcmp(versionResKey, L"VS_VERSION_INFO", 0x20) != 0)
	{
		SetLastError(ERROR_INVALID_DATA);
		return false;
	}

	const void* versionResValue = static_cast<const unsigned char*>(versionResKey) + 0x20 + 0x2;
	const VS_FIXEDFILEINFO* fileInfo = static_cast<const VS_FIXEDFILEINFO*>(versionResValue);
	if (fileInfo->dwSignature != 0xFEEF04BD)
	{
		SetLastError(ERROR_INVALID_DATA);
		return false;
	}

	result.major = HIWORD(fileInfo->dwProductVersionMS);
	result.minor = LOWORD(fileInfo->dwProductVersionMS);
	result.patch = HIWORD(fileInfo->dwProductVersionLS);
	result.tweak = LOWORD(fileInfo->dwProductVersionLS);

	return true;
}

std::string_view WinAPI::GetDataResource(void *pDLL, int resourceID)
{
	return GetResource(pDLL, MAKEINTRESOURCE(resourceID), RT_RCDATA);
}

///////////
// Hacks //
///////////

namespace
{
	IMAGE_DATA_DIRECTORY *GetDirectoryData(void *pDLL, unsigned int directory)
	{
		using WinAPI::RVA;

		if (!pDLL)
			return nullptr;

		IMAGE_DOS_HEADER *pDOSHeader = static_cast<IMAGE_DOS_HEADER*>(pDLL);
		if (pDOSHeader->e_magic != IMAGE_DOS_SIGNATURE)
			return nullptr;

		IMAGE_NT_HEADERS *pPEHeader = static_cast<IMAGE_NT_HEADERS*>(RVA(pDLL, pDOSHeader->e_lfanew));
		if (pPEHeader->Signature != IMAGE_NT_SIGNATURE)
			return nullptr;

	#ifdef BUILD_64BIT
		const int ntOptionalHeaderMagic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
	#else
		const int ntOptionalHeaderMagic = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
	#endif

		if (pPEHeader->OptionalHeader.Magic != ntOptionalHeaderMagic)
			return nullptr;

		if (pPEHeader->OptionalHeader.NumberOfRvaAndSizes <= directory)
			return nullptr;

		IMAGE_DATA_DIRECTORY *pDirectoryData = &pPEHeader->OptionalHeader.DataDirectory[directory];
		if (pDirectoryData->VirtualAddress == 0 || pDirectoryData->Size == 0)
			return nullptr;

		return pDirectoryData;
	}
}

/**
 * @brief Fills read-only memory region with x86 NOP instruction.
 * @param address Address of the memory region.
 * @param length Size of the memory region in bytes.
 */
void WinAPI::FillNOP(void *address, size_t length)
{
	DWORD oldProtection;

	if (!VirtualProtect(address, length, PAGE_EXECUTE_READWRITE, &oldProtection))
	{
		throw StringTools::SysErrorFormat("VirtualProtect");
	}

	// 0x90 is opcode of NOP instruction on both x86 and x86-64
	memset(address, '\x90', length);

	if (!VirtualProtect(address, length, oldProtection, &oldProtection))
	{
		throw StringTools::SysErrorFormat("VirtualProtect");
	}
}

/**
 * @brief Fills read-only memory region with custom data.
 * The memory region and the data must not overlap.
 * @param address Address of the memory region.
 * @param data The data.
 * @param length Size of the data in bytes.
 */
void WinAPI::FillMem(void *address, const void *data, size_t length)
{
	DWORD oldProtection;

	if (!VirtualProtect(address, length, PAGE_EXECUTE_READWRITE, &oldProtection))
	{
		throw StringTools::SysErrorFormat("VirtualProtect");
	}

	memcpy(address, data, length);

	if (!VirtualProtect(address, length, oldProtection, &oldProtection))
	{
		throw StringTools::SysErrorFormat("VirtualProtect");
	}
}

void WinAPI::HookWithJump(void* address, void* pNewFunc)
{
	if (!address)
	{
		return;
	}

#ifdef BUILD_64BIT
	unsigned char code[] = {
		0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rax, 0x0
		0xFF, 0xE0                                                   // jmp rax
	};

	memcpy(&code[2], &pNewFunc, 8);
#else
	unsigned char code[] = {
		0xB8, 0x00, 0x00, 0x00, 0x00,  // mov eax, 0x0
		0xFF, 0xE0                     // jmp eax
	};

	memcpy(&code[1], &pNewFunc, 4);
#endif

	FillMem(address, &code, sizeof(code));
}

bool WinAPI::HookIATByAddress(void *pDLL, void *pFunc, void *pNewFunc)
{
	const IMAGE_DATA_DIRECTORY *pIATData = GetDirectoryData(pDLL, IMAGE_DIRECTORY_ENTRY_IAT);
	if (!pIATData)
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return false;
	}

	// import address table
	void **pIAT = static_cast<void**>(RVA(pDLL, pIATData->VirtualAddress));

	// number of functions in the table
	const size_t entryCount = pIATData->Size / sizeof (void*);

	bool found = false;

	for (size_t i = 0; i < entryCount; i++)
	{
		if (pIAT[i] == pFunc)
		{
			found = true;

			// hook the function
			FillMem(&pIAT[i], &pNewFunc, sizeof (void*));
		}
	}

	if (!found)
	{
		SetLastError(ERROR_PROC_NOT_FOUND);
		return false;
	}

	return true;
}

bool WinAPI::HookIATByName(void *pDLL, const char *dllName, const char *funcName, void *pNewFunc)
{
	const IMAGE_DATA_DIRECTORY* importData = GetDirectoryData(pDLL, IMAGE_DIRECTORY_ENTRY_IMPORT);
	if (!importData)
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return false;
	}

	bool found = false;

	const IMAGE_IMPORT_DESCRIPTOR* importDescriptor =
		static_cast<const IMAGE_IMPORT_DESCRIPTOR*>(RVA(pDLL, importData->VirtualAddress));

	for (; importDescriptor->Name && importDescriptor->FirstThunk; ++importDescriptor)
	{
		const char* currentDllName = static_cast<const char*>(RVA(pDLL, importDescriptor->Name));
		if (_stricmp(currentDllName, dllName) != 0)
		{
			continue;
		}

		const IMAGE_THUNK_DATA* thunks =
			static_cast<const IMAGE_THUNK_DATA*>(RVA(pDLL, importDescriptor->OriginalFirstThunk));

		const IMAGE_THUNK_DATA* pIAT =
			static_cast<const IMAGE_THUNK_DATA*>(RVA(pDLL, importDescriptor->FirstThunk));

		for (size_t i = 0; thunks[i].u1.Ordinal; ++i)
		{
			if (IMAGE_SNAP_BY_ORDINAL(thunks[i].u1.Ordinal))
			{
				continue;
			}

			const IMAGE_IMPORT_BY_NAME* thunkData =
				static_cast<const IMAGE_IMPORT_BY_NAME*>(RVA(pDLL, thunks[i].u1.AddressOfData));

			const char* thunkName = reinterpret_cast<const char*>(thunkData->Name);
			if (strcmp(thunkName, funcName) != 0)
			{
				continue;
			}

			found = true;

			// hook the function
			FillMem(const_cast<IMAGE_THUNK_DATA*>(&pIAT[i]), &pNewFunc, sizeof(void*));
		}
	}

	if (!found)
	{
		SetLastError(ERROR_PROC_NOT_FOUND);
		return false;
	}

	return true;
}

//////////
// Time //
//////////

namespace
{
	WinAPI::DateTime FromNativeDateTime(const SYSTEMTIME & dateTime)
	{
		WinAPI::DateTime result;
		result.year        = dateTime.wYear;
		result.month       = dateTime.wMonth;
		result.dayOfWeek   = dateTime.wDayOfWeek;
		result.day         = dateTime.wDay;
		result.hour        = dateTime.wHour;
		result.minute      = dateTime.wMinute;
		result.second      = dateTime.wSecond;
		result.millisecond = dateTime.wMilliseconds;

		return result;
	}
}

const char *WinAPI::DateTime::GetDayName()
{
	switch (dayOfWeek)
	{
		case 0: return "Sunday";
		case 1: return "Monday";
		case 2: return "Tuesday";
		case 3: return "Wednesday";
		case 4: return "Thursday";
		case 5: return "Friday";
		case 6: return "Saturday";
	}

	return "";
}

const char *WinAPI::DateTime::GetMonthName()
{
	switch (month)
	{
		case 1:  return "January";
		case 2:  return "February";
		case 3:  return "March";
		case 4:  return "April";
		case 5:  return "May";
		case 6:  return "June";
		case 7:  return "July";
		case 8:  return "August";
		case 9:  return "September";
		case 10: return "October";
		case 11: return "November";
		case 12: return "December";
	}

	return "";
}

WinAPI::DateTime WinAPI::GetCurrentDateTimeUTC()
{
	SYSTEMTIME dateTime;
	GetSystemTime(&dateTime);

	return FromNativeDateTime(dateTime);
}

WinAPI::DateTime WinAPI::GetCurrentDateTimeLocal()
{
	SYSTEMTIME dateTime;
	GetLocalTime(&dateTime);

	return FromNativeDateTime(dateTime);
}

long WinAPI::GetTimeZoneBias()
{
	TIME_ZONE_INFORMATION tz;
	switch (GetTimeZoneInformation(&tz))
	{
		case TIME_ZONE_ID_UNKNOWN:
		{
			return tz.Bias;
		}
		case TIME_ZONE_ID_STANDARD:
		{
			return tz.Bias + tz.StandardBias;
		}
		case TIME_ZONE_ID_DAYLIGHT:
		{
			return tz.Bias + tz.DaylightBias;
		}
		case TIME_ZONE_ID_INVALID:
		{
			throw StringTools::SysErrorFormat("GetTimeZoneInformation");
		}
	}

	return 0;
}

/////////////
// Strings //
/////////////

std::size_t WinAPI::RawANSIToWide(const char* string, std::size_t stringSize, wchar_t* buffer, std::size_t bufferSize)
{
	return MultiByteToWideChar(
		CP_ACP, 0,
		string, static_cast<int>(stringSize),
		buffer, static_cast<int>(bufferSize)
	);
}

wchar_t WinAPI::WideCharToLower(wchar_t ch, int languageID)
{
	if (languageID == 0)
	{
		languageID = LOCALE_SYSTEM_DEFAULT;
	}

	wchar_t result;
	if (LCMapStringW(languageID, LCMAP_LOWERCASE, &ch, 1, &result, 1))
	{
		return result;
	}
	else
	{
		return ch;
	}
}

wchar_t WinAPI::WideCharToUpper(wchar_t ch, int languageID)
{
	if (languageID == 0)
	{
		languageID = LOCALE_SYSTEM_DEFAULT;
	}

	wchar_t result;
	if (LCMapStringW(languageID, LCMAP_UPPERCASE, &ch, 1, &result, 1))
	{
		return result;
	}
	else
	{
		return ch;
	}
}

/////////////////
// System info //
/////////////////

std::string WinAPI::GetMachineGUID()
{
	std::string guid;

	HKEY key;
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Cryptography", 0,
	                  KEY_QUERY_VALUE | KEY_WOW64_64KEY, &key) == ERROR_SUCCESS)
	{
		char buffer[256];
		DWORD length = static_cast<DWORD>(sizeof buffer);
		if (RegQueryValueExA(key, "MachineGuid", nullptr, nullptr,
		                     reinterpret_cast<LPBYTE>(buffer), &length) == ERROR_SUCCESS && length > 0)
		{
			guid.assign(buffer, length - 1);  // skip terminating null character
		}
	}

	return guid;
}

std::string WinAPI::GetLocale()
{
	wchar_t buffer[LOCALE_NAME_MAX_LENGTH] = {};
	GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SNAME, buffer, LOCALE_NAME_MAX_LENGTH);

	std::string result;
	StringTools::AppendTo(result, std::wstring_view(buffer));

	return result;
}

bool WinAPI::IsVistaOrLater()
{
	OSVERSIONINFOW info = {};
	info.dwOSVersionInfoSize = sizeof info;
	__pragma(warning(suppress:4996))
	GetVersionExW(&info);

	return info.dwMajorVersion >= 6;
}

unsigned int WinAPI::GetLogicalProcessorCount()
{
	SYSTEM_INFO info;
	GetSystemInfo(&info);

	return info.dwNumberOfProcessors;
}

std::size_t WinAPI::GetSystemLanguageCode(char* buffer, std::size_t bufferSize)
{
	return static_cast<std::size_t>(::GetLocaleInfoA(
		LOCALE_SYSTEM_DEFAULT,
		LOCALE_SISO639LANGNAME,
		buffer,
		static_cast<int>(bufferSize)
	));
}

//////////
// HTTP //
//////////

namespace
{
	class HTTPHandleGuard
	{
		HINTERNET m_handle;

	public:
		HTTPHandleGuard(HINTERNET handle)
		: m_handle(handle)
		{
		}

		~HTTPHandleGuard()
		{
			if (m_handle)
			{
				WinHttpCloseHandle(m_handle);
			}
		}

		operator HINTERNET()
		{
			return m_handle;
		}

		explicit operator bool() const
		{
			return m_handle != nullptr;
		}
	};
}

int WinAPI::HTTPRequest(
	const std::string_view & method,
	const std::string_view & url,
	const std::string_view & data,
	const std::map<std::string, std::string> & headers,
	HTTPRequestCallback callback
){
	std::wstring urlW;
	StringTools::AppendTo(urlW, url);

	URL_COMPONENTS urlComponents = {};
	urlComponents.dwStructSize = sizeof urlComponents;
	urlComponents.dwSchemeLength = -1;
	urlComponents.dwHostNameLength = -1;
	urlComponents.dwUrlPathLength = -1;
	urlComponents.dwExtraInfoLength = -1;

	if (!WinHttpCrackUrl(urlW.c_str(), static_cast<DWORD>(urlW.length()), 0, &urlComponents))
	{
		throw StringTools::SysErrorFormat("WinHttpCrackUrl");
	}

	HTTPHandleGuard hSession = WinHttpOpen(L"CryMP-Client",
	                                       WINHTTP_ACCESS_TYPE_NO_PROXY,
	                                       WINHTTP_NO_PROXY_NAME,
	                                       WINHTTP_NO_PROXY_BYPASS, 0);
	if (!hSession)
	{
		throw StringTools::SysErrorFormat("WinHttpOpen");
	}

	const std::wstring serverNameW(urlComponents.lpszHostName, urlComponents.dwHostNameLength);

	HTTPHandleGuard hConnect = WinHttpConnect(hSession, serverNameW.c_str(), urlComponents.nPort, 0);
	if (!hConnect)
	{
		throw StringTools::SysErrorFormat("WinHttpConnect");
	}

	DWORD requestFlags = WINHTTP_FLAG_REFRESH;

	if (urlComponents.nScheme == INTERNET_SCHEME_HTTPS)
	{
		requestFlags |= WINHTTP_FLAG_SECURE;
	}

	std::wstring methodW;
	StringTools::AppendTo(methodW, method);

	// URL components are not null-terminated, but whole URL is, so URL path contains both path and parameters
	HTTPHandleGuard hRequest = WinHttpOpenRequest(hConnect,
	                                              methodW.c_str(),
	                                              urlComponents.lpszUrlPath, nullptr,
	                                              WINHTTP_NO_REFERER,
	                                              WINHTTP_DEFAULT_ACCEPT_TYPES, requestFlags);
	if (!hRequest)
	{
		throw StringTools::SysErrorFormat("WinHttpOpenRequest");
	}

	std::wstring headersW;

	for (const auto & [key, value] : headers)
	{
		StringTools::AppendTo(headersW, key);
		headersW += L": ";
		StringTools::AppendTo(headersW, value);
		headersW += L"\r\n";
	}

	if (!WinHttpSendRequest(hRequest, headersW.c_str(), static_cast<DWORD>(headersW.length()),
	                        const_cast<char*>(data.data()),
				static_cast<DWORD>(data.length()),
				static_cast<DWORD>(data.length()), 0))
	{
		throw StringTools::SysErrorFormat("WinHttpSendRequest");
	}

	if (!WinHttpReceiveResponse(hRequest, nullptr))
	{
		throw StringTools::SysErrorFormat("WinHttpReceiveResponse");
	}

	DWORD statusCode = 0;
	DWORD statusCodeSize = sizeof statusCode;
	if (!WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
	                         WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX))
	{
		throw StringTools::SysErrorFormat("WinHttpQueryHeaders(WINHTTP_QUERY_STATUS_CODE)");
	}

	if (callback)
	{
		uint64_t contentLength = 0;

		// WINHTTP_QUERY_CONTENT_LENGTH is limited to 32-bit (4 GB) content length
		// so we get content length value as a string instead
		wchar_t contentLengthString[32];
		DWORD contentLengthStringSize = sizeof contentLengthString;  // in bytes
		if (!WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CUSTOM, L"Content-Length",
			                 contentLengthString, &contentLengthStringSize, WINHTTP_NO_HEADER_INDEX))
		{
			if (GetLastError() != ERROR_WINHTTP_HEADER_NOT_FOUND)
			{
				throw StringTools::SysErrorFormat("WinHttpQueryHeaders(\"Content-Length\")");
			}
		}
		else
		{
			contentLength = _wcstoui64(contentLengthString, nullptr, 10);
		}

		const HTTPRequestReader dataReader = [&hRequest](void *buffer, size_t bufferSize) -> size_t
		{
			DWORD dataLength = 0;
			if (!WinHttpReadData(hRequest, buffer, static_cast<DWORD>(bufferSize), &dataLength))
			{
				throw StringTools::SysErrorFormat("WinHttpReadData");
			}

			return dataLength;
		};

		callback(contentLength, dataReader);
	}

	return statusCode;
}

///////////////
// Clipboard //
///////////////

std::string WinAPI::GetClipboardText(std::size_t maxLength)
{
	if (!OpenClipboard(nullptr))
	{
		throw StringTools::SysErrorFormat("OpenClipboard");
	}

	struct Closer { ~Closer() { CloseClipboard(); } } closer;

	if (!IsClipboardFormatAvailable(CF_TEXT))
	{
		return {};
	}

	HANDLE dataHandle = GetClipboardData(CF_TEXT);
	if (!dataHandle)
	{
		throw StringTools::SysErrorFormat("GetClipboardData");
	}

	const char* data = static_cast<char*>(GlobalLock(dataHandle));
	if (!data)
	{
		throw StringTools::SysErrorFormat("GlobalLock");
	}

	struct Unlocker { HANDLE handle; ~Unlocker() { GlobalUnlock(this->handle); } } unlocker{ dataHandle };

	const std::size_t dataLength = std::strlen(data);

	return { data, (dataLength <= maxLength) ? dataLength : maxLength };
}

////////////
// Cursor //
////////////

void WinAPI::Cursor::GetPos(long& x, long& y)
{
	POINT point = {};
	GetCursorPos(&point);

	x = point.x;
	y = point.y;
}

void WinAPI::Cursor::SetPos(long x, long y)
{
	SetCursorPos(x, y);
}

void WinAPI::Cursor::Show(bool show)
{
	ShowCursor(show ? TRUE : FALSE);
}

void WinAPI::Cursor::Clip(void* window)
{
	if (window)
	{
		RECT rect = {};
		GetClientRect(static_cast<HWND>(window), &rect);
		ClientToScreen(static_cast<HWND>(window), reinterpret_cast<POINT*>(&rect.left));
		ClientToScreen(static_cast<HWND>(window), reinterpret_cast<POINT*>(&rect.right));
		ClipCursor(&rect);
	}
	else
	{
		ClipCursor(nullptr);
	}
}

////////////
// Window //
////////////

void WinAPI::Window::ConvertPosToWindow(void* window, long& x, long& y)
{
	POINT point;
	point.x = x;
	point.y = y;

	ScreenToClient(static_cast<HWND>(window), &point);

	x = point.x;
	y = point.y;
}

void WinAPI::Window::ConvertPosToScreen(void* window, long& x, long& y)
{
	POINT point;
	point.x = x;
	point.y = y;

	ClientToScreen(static_cast<HWND>(window), &point);

	x = point.x;
	y = point.y;
}

std::string WinAPI::GetLocalIP() {
	char hostn[255];
	if (gethostname(hostn, sizeof(hostn)) != SOCKET_ERROR) {
		struct hostent *host = gethostbyname(hostn);
		if (host != NULL) {
			for (int i = 0; host->h_addr_list[i] != 0; ++i) {
				struct in_addr addr;
				memcpy(&addr, host->h_addr_list[i], sizeof(struct in_addr));
				return inet_ntoa(addr);
			}
		}
	}
	return "";
}

std::string WinAPI::GetIP(const std::string& hostName) {
	std::string host { hostName };
	if(size_t pos = host.find(':'); pos != std::string::npos) {
		host = host.substr(0, pos);
	}
	
	in_addr iaddr;
	hostent *result = gethostbyname(host.c_str());
	if(result == NULL) {
		return host;
	} else {
		iaddr.s_addr=*(unsigned long*)result->h_addr;
		return inet_ntoa(iaddr);
	}
}
