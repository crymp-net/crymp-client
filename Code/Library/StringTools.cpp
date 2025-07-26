#include <cerrno>
#include <cstdio>

#include "StringTools.h"

////////////////////////////////////////////////////////////////////////////////
// windows.h
////////////////////////////////////////////////////////////////////////////////

#ifndef _INC_WINDOWS
typedef unsigned long DWORD;
typedef void* HMODULE;
#endif

extern "C" __declspec(dllimport) DWORD __stdcall GetLastError();
extern "C" __declspec(dllimport) DWORD __stdcall FormatMessageA(
	DWORD flags,
	const void* source,
	DWORD message,
	DWORD language,
	char* buffer,
	DWORD bufferSize,
	va_list* args
);

extern "C" __declspec(dllimport) HMODULE __stdcall GetModuleHandleA(const char* name);

////////////////////////////////////////////////////////////////////////////////

class WinHttpErrorCategory : public std::error_category
{
public:
	const char* name() const noexcept override
	{
		return "winhttp";
	}

	std::string message(int code) const override
	{
		HMODULE winhttp = ::GetModuleHandleA("winhttp.dll");
		if (!winhttp)
		{
			return {};
		}

		// FORMAT_MESSAGE_IGNORE_INSERTS
		// FORMAT_MESSAGE_FROM_HMODULE
		// FORMAT_MESSAGE_FROM_SYSTEM
		const DWORD flags = 0x200 | 0x800 | 0x1000;

		const DWORD message = static_cast<DWORD>(code);
		const DWORD language = 0;

		char buffer[256];
		DWORD length = ::FormatMessageA(flags, winhttp, message, language, buffer, sizeof(buffer), nullptr);

		return std::string(buffer, length);
	}
};

static const WinHttpErrorCategory g_winhttp_error_category;

static const std::error_category& GetSysErrorCategory(DWORD code)
{
	if (code >= 12000 && code <= 12999)
	{
		return g_winhttp_error_category;
	}
	else
	{
		return std::system_category();
	}
}

static std::error_code GetSysErrorCodeWithCategory(DWORD code)
{
	return std::error_code(static_cast<int>(code), GetSysErrorCategory(code));
}

CryMP_Error::CryMP_Error(const std::string& message, std::error_code code)
: m_what(message), m_message(message), m_code(code)
{
	if (code)
	{
		m_what += "\n\nError ";
		m_what += std::to_string(code.value());
		m_what += ": ";
		m_what += code.message();
	}
}

std::string StringTools::Format(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	std::string result = FormatV(format, args);
	va_end(args);

	return result;
}

std::string StringTools::FormatV(const char* format, va_list args)
{
	std::string result;
	FormatToV(result, format, args);

	return result;
}

std::size_t StringTools::FormatTo(std::string& result, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	std::size_t length = FormatToV(result, format, args);
	va_end(args);

	return length;
}

static std::size_t FormatToGenericStringV(auto& result, const char* format, va_list args)
{
	va_list argsCopy;
	va_copy(argsCopy, args);

	char buffer[512];
	std::size_t length = StringTools::FormatToV(buffer, sizeof(buffer), format, args);

	// make sure the resulting string is not truncated
	if (length < sizeof(buffer))
	{
		// do not overwrite the existing content
		result.append(buffer, length);
	}
	else
	{
		const std::size_t existingLength = result.length();

		result.resize(existingLength + length);

		// format string again with proper buffer size
		length = StringTools::FormatToV(result.data() + existingLength, length + 1, format, argsCopy);

		if (result.length() > (existingLength + length))
		{
			result.resize(existingLength + length);
		}
	}

	va_end(argsCopy);

	return length;
}

std::size_t StringTools::FormatToV(std::string& result, const char* format, va_list args)
{
	return FormatToGenericStringV(result, format, args);
}

std::size_t StringTools::FormatTo(std::pmr::string& result, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	std::size_t length = FormatToV(result, format, args);
	va_end(args);

	return length;
}

std::size_t StringTools::FormatToV(std::pmr::string& result, const char* format, va_list args)
{
	return FormatToGenericStringV(result, format, args);
}

std::size_t StringTools::FormatTo(char* buffer, std::size_t bufferSize, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	std::size_t length = FormatToV(buffer, bufferSize, format, args);
	va_end(args);

	return length;
}

std::size_t StringTools::FormatToV(char* buffer, std::size_t bufferSize, const char* format, va_list args)
{
	if (!buffer || !bufferSize)
	{
		buffer = NULL;
		bufferSize = 0;
	}
	else
	{
		buffer[0] = '\0';
	}

	std::size_t length = 0;

	if (format)
	{
		const int status = std::vsnprintf(buffer, bufferSize, format, args);

		if (status > 0)
		{
			length = static_cast<std::size_t>(status);

			if (length >= bufferSize && bufferSize > 0)
			{
				// make sure the result is always null-terminated
				buffer[bufferSize - 1] = '\0';
			}
		}
	}

	return length;
}

CryMP_Error StringTools::ErrorFormat(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	CryMP_Error error = ErrorFormatV(format, args);
	va_end(args);

	return error;
}

CryMP_Error StringTools::ErrorFormatV(const char* format, va_list args)
{
	return CryMP_Error(FormatV(format, args));
}

CryMP_Error StringTools::SysErrorFormat(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	CryMP_Error error = SysErrorFormatV(format, args);
	va_end(args);

	return error;
}

CryMP_Error StringTools::SysErrorFormatV(const char* format, va_list args)
{
	const DWORD code = ::GetLastError();

	return CryMP_Error(FormatV(format, args), GetSysErrorCodeWithCategory(code));
}

CryMP_Error StringTools::SysErrorErrnoFormat(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	CryMP_Error error = SysErrorErrnoFormatV(format, args);
	va_end(args);

	return error;
}

CryMP_Error StringTools::SysErrorErrnoFormatV(const char* format, va_list args)
{
	const int code = errno;

	return CryMP_Error(FormatV(format, args), std::error_code(code, std::generic_category()));
}
