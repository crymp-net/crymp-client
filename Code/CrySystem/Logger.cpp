#include <algorithm>
#include <cstring>

#include <tracy/Tracy.hpp>

#include "CryCommon/CrySystem/CryColorCode.h"
#include "CryCommon/CrySystem/IConsole.h"
#include "CryCommon/CrySystem/ISystem.h"

#include "Library/StringTools.h"
#include "Library/WinAPI.h"

#include "Logger.h"

Logger Logger::s_globalInstance;

static uint64_t Now() {
	std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
	return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

Logger::Logger()
{
	m_mainThreadID = std::this_thread::get_id();
}

Logger::~Logger()
{
}

void Logger::OnUpdate()
{
	std::lock_guard lock(m_mutex);

	for (const Message& message : m_messages)
	{
		WriteMessage(message);
	}

	m_messages.clear();
}

static std::string_view ExtractBackupNameAttachment(std::string_view header)
{
	constexpr std::string_view prefix = "BackupNameAttachment=";

	if (header.length() >= prefix.length() && header.substr(0, prefix.length()) == prefix)
	{
		header.remove_prefix(prefix.length());

		if (auto pos = header.find('"'); pos != std::string_view::npos)
		{
			header.remove_prefix(pos + 1);
		}

		if (auto pos = header.find_first_of("\"\r\n"); pos != std::string_view::npos)
		{
			header.remove_suffix(header.length() - pos);
		}

		return header;
	}
	else
	{
		return "";
	}
}

static void BackupLogFile(std::FILE* file, const std::filesystem::path& filePath)
{
	char buffer[256];
	const std::string_view header(buffer, std::fread(buffer, 1, sizeof buffer, file));

	if (header.empty() && std::feof(file))
	{
		// the existing log file is empty, so no backup is needed
		return;
	}

	const std::filesystem::path backupDirPath = filePath.parent_path() / "LogBackups";

	std::filesystem::create_directory(backupDirPath);

	std::filesystem::path backupFilePath = backupDirPath;
	backupFilePath /= filePath.stem();
	backupFilePath += ExtractBackupNameAttachment(header);
	backupFilePath += filePath.extension();

	std::filesystem::copy_file(filePath, backupFilePath, std::filesystem::copy_options::overwrite_existing);
}

void Logger::OpenFile(const std::filesystem::path& filePath)
{
	const std::string filePathString = filePath.string();

	if (FileHandle existingFile(std::fopen(filePathString.c_str(), "r")); existingFile)
	{
		BackupLogFile(existingFile.get(), filePath);
	}

	FileHandle file(std::fopen(filePathString.c_str(), "w"));
	if (!file)
	{
		throw StringTools::SysErrorErrnoFormat("Failed to open log file: %s", filePathString.c_str());
	}

	m_file = std::move(file);
	m_filePath = filePath;
	m_fileName = filePath.filename().string();

	LogAlways("BackupNameAttachment=\"%s\"", FormatPrefix(" Date(%Y %m %d) Time(%H %M %S)").c_str());
	LogAlways("");
}

void Logger::CloseFile()
{
	m_file.reset();
	m_filePath.clear();
	m_fileName.clear();
}

void Logger::SetPrefix(const std::string_view& prefix)
{
	m_prefix = prefix;

	if (m_cvars.prefix)
	{
		m_cvars.prefix->Set(m_prefix.c_str());
	}
}

static void AddCurrentThreadID(std::string& result)
{
	const auto id = std::this_thread::get_id();

	unsigned long raw_id = 0;
	static_assert(sizeof(id) == sizeof(raw_id));

	std::memcpy(&raw_id, &id, sizeof(raw_id));

	StringTools::FormatTo(result, "%04lx", raw_id);
}

static void AddTimeZoneOffset(std::string& result)
{
	long bias = WinAPI::GetTimeZoneBias();

	if (bias == 0)
	{
		result += 'Z';  // UTC
	}
	else
	{
		char sign = '-';

		if (bias < 0)
		{
			bias = -bias;
			sign = '+';
		}

		StringTools::FormatTo(result, "%c%02u%02u", sign, bias / 60, bias % 60);
	}
}

static void ExpandPrefixFormatSpecifier(std::string& result, char specifier, const WinAPI::DateTime& time)
{
	switch (specifier)
	{
		case '%':
		{
			result += '%';
			break;
		}
		case 't':
		{
			AddCurrentThreadID(result);
			break;
		}
		case 'd':
		{
			StringTools::FormatTo(result, "%02u", time.day);
			break;
		}
		case 'm':
		{
			StringTools::FormatTo(result, "%02u", time.month);
			break;
		}
		case 'Y':
		{
			StringTools::FormatTo(result, "%04u", time.year);
			break;
		}
		case 'F':
		{
			StringTools::FormatTo(result, "%04u-%02u-%02u", time.year, time.month, time.day);
			break;
		}
		case 'H':
		{
			StringTools::FormatTo(result, "%02u", time.hour);
			break;
		}
		case 'M':
		{
			StringTools::FormatTo(result, "%02u", time.minute);
			break;
		}
		case 'S':
		{
			StringTools::FormatTo(result, "%02u", time.second);
			break;
		}
		case 'T':
		{
			StringTools::FormatTo(result, "%02u:%02u:%02u", time.hour, time.minute, time.second);
			break;
		}
		case 'N':
		{
			StringTools::FormatTo(result, "%03u", time.millisecond);
			break;
		}
		case 'z':
		{
			AddTimeZoneOffset(result);
			break;
		}
	}
}

std::string Logger::FormatPrefix(const std::string_view& prefix)
{
	const WinAPI::DateTime currentTime = WinAPI::GetCurrentDateTimeLocal();

	std::string result;
	result.reserve(prefix.length());

	for (std::size_t i = 0; i < prefix.length(); i++)
	{
		if (prefix[i] == '%')
		{
			if ((i+1) < prefix.length())
			{
				ExpandPrefixFormatSpecifier(result, prefix[++i], currentTime);
			}
		}
		else
		{
			result += prefix[i];
		}
	}

	return result;
}

void Logger::LogAlways(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	LogV(ILog::eAlways, format, args);
	va_end(args);
}

void Logger::LogV(ILog::ELogType type, const char* format, va_list args)
{
	PushMessageV(type, Message::FLAG_FILE | Message::FLAG_CONSOLE, format, args);
}

void Logger::Log(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	LogV(ILog::eMessage, format, args);
	va_end(args);
}

void Logger::LogWarning(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	LogV(ILog::eWarning, format, args);
	va_end(args);
}

void Logger::LogError(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	LogV(ILog::eError, format, args);
	va_end(args);
}

void Logger::Release()
{
	// don't let the engine to delete us
}

bool Logger::SetFileName(const char* fileName)
{
	// don't let the engine to mess with the log file name
	return true;
}

const char* Logger::GetFileName()
{
	return m_fileName.c_str();
}

void Logger::LogPlus(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	PushMessageV(ILog::eMessage, Message::FLAG_FILE | Message::FLAG_CONSOLE, format, args);
	va_end(args);
}

void Logger::LogToFile(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	PushMessageV(ILog::eMessage, Message::FLAG_FILE, format, args);
	va_end(args);
}

void Logger::LogToFilePlus(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	PushMessageV(ILog::eMessage, Message::FLAG_FILE, format, args);
	va_end(args);
}

void Logger::LogToConsole(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	PushMessageV(ILog::eMessage, Message::FLAG_CONSOLE, format, args);
	va_end(args);
}

void Logger::LogToConsolePlus(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	PushMessageV(ILog::eMessage, Message::FLAG_CONSOLE, format, args);
	va_end(args);
}

void Logger::UpdateLoadingScreen(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	LogV(ILog::eMessage, format, args);
	va_end(args);

	if (std::this_thread::get_id() != m_mainThreadID || !gEnv || gEnv->bEditor)
	{
		return;
	}

	ISystem* pSystem = gEnv->pSystem;

	ISystem::ILoadingProgressListener* pLoadingProgressListener = pSystem->GetLoadingProgressListener();
	if (pLoadingProgressListener)
	{
		pLoadingProgressListener->OnLoadingProgress(0);
	}
}

void Logger::RegisterConsoleVariables()
{
	if (!gEnv)
	{
		return;
	}

	IConsole* pConsole = gEnv->pConsole;

	if (!pConsole)
	{
		return;
	}

	m_cvars.verbosity = pConsole->RegisterInt("log_Verbosity", m_verbosity, VF_DUMPTODISK,
		"Defines the verbosity level for console log messages (use log_FileVerbosity for file logging).\n"
		"Usage: log_Verbosity [-1/0/1/2/3/4]\n"
		" -1 = Suppress all logs (including eAlways).\n"
		"  0 = Suppress all logs (except eAlways).\n"
		"  1 = Additional errors.\n"
		"  2 = Additional warnings.\n"
		"  3 = Additional messages.\n"
		"  4 = Additional comments."
	);

	m_cvars.fileVerbosity = pConsole->RegisterInt("log_FileVerbosity", m_verbosity, VF_DUMPTODISK,
		"Defines the verbosity level for file log messages (if log_Verbosity is higher, this one is used).\n"
		"Usage: log_FileVerbosity [-1/0/1/2/3/4]\n"
		" -1 = Suppress all logs (including eAlways).\n"
		"  0 = Suppress all logs (except eAlways).\n"
		"  1 = Additional errors.\n"
		"  2 = Additional warnings.\n"
		"  3 = Additional messages.\n"
		"  4 = Additional comments."
	);

	m_cvars.prefix = pConsole->RegisterString("log_Prefix", m_prefix.c_str(), VF_NOT_NET_SYNCED,
		"Defines prefix of each message written to the log file.\n"
		"Usage: log_Prefix FORMAT\n"
		"The format string consists of normal characters and the following conversion specifiers:\n"
		"  %% = %\n"
		"  %d = Day of the month (01..31)\n"
		"  %m = Month (01..12)\n"
		"  %Y = Year (e.g. 2007)\n"
		"  %H = Hour (00..23)\n"
		"  %M = Minute (00..59)\n"
		"  %S = Second (00..60)\n"
		"  %N = Millisecond (000..999)\n"
		"  %z = Offset from UTC (time zone) in the ISO 8601 format (e.g. +0100)\n"
		"  %F = Equivalent to \"%Y-%m-%d\" (the ISO 8601 date format)\n"
		"  %T = Equivalent to \"%H:%M:%S\" (the ISO 8601 time format)\n"
		"  %t = Thread ID where the message was logged"
	);

	m_cvars.paralog = pConsole->RegisterInt("log_Paralog", m_paralog, VF_NOT_NET_SYNCED,
		"Enable and set time-frame for parallel logging of all messages for debugging purposed."
	);
}

void Logger::UnregisterConsoleVariables()
{
	// keep all cvars registered and reuse them
}

void Logger::SetVerbosity(int verbosity)
{
	m_verbosity = verbosity;

	if (m_cvars.verbosity)
	{
		m_cvars.verbosity->Set(verbosity);
	}

	if (m_cvars.fileVerbosity)
	{
		m_cvars.fileVerbosity->Set(verbosity);
	}
}

int Logger::GetVerbosityLevel()
{
	return (m_cvars.verbosity) ? m_cvars.verbosity->GetIVal() : m_verbosity;
}

void Logger::AddCallback(ILogCallback* callback)
{
	if (std::this_thread::get_id() != m_mainThreadID)
	{
		return;
	}

	if (callback && std::count(m_callbacks.begin(), m_callbacks.end(), callback) == 0)
	{
		m_callbacks.emplace_back(callback);
	}
}

void Logger::RemoveCallback(ILogCallback* callback)
{
	if (std::this_thread::get_id() != m_mainThreadID)
	{
		return;
	}

	m_callbacks.erase(std::remove(m_callbacks.begin(), m_callbacks.end(), callback), m_callbacks.end());
}

void Logger::PushMessage(ILog::ELogType type, unsigned int flags, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	PushMessageV(type, flags, format, args);
	va_end(args);
}

void Logger::PushMessageV(ILog::ELogType type, unsigned int flags, const char* format, va_list args)
{
	if (!format)
	{
		// drop messages with null format
		return;
	}

	const int currentVerbosity = GetVerbosityLevel();
	const int requiredVerbosity = GetRequiredVerbosity(type);

	bool logged = true;
	const int paralog = m_cvars.paralog ? m_cvars.paralog->GetIVal() : 0;

	if (currentVerbosity < requiredVerbosity)
	{
		if (paralog <= 0) {
			// drop messages above the current verbosity level
			return;
		} else {
			// continue, but do not log the message into neither console or file, but still into paralog
			logged = false;
		}
	}

	const int currentFileVerbosity = (m_cvars.fileVerbosity) ? m_cvars.fileVerbosity->GetIVal() : currentVerbosity;

	if (currentFileVerbosity < requiredVerbosity)
	{
		flags &= ~Message::FLAG_FILE;
	}

	Message message;
	message.type = type;
	message.flags = flags;
	message.time = Now();

	BuildMessagePrefix(message);
	BuildMessageContent(message, format, args);

	TracyMessage(message.content.data(), message.content.length());

	if (paralog) {
		std::lock_guard lock(m_mutex);
		m_paralogMessages.push_back(message);
		while (!m_paralogMessages.empty()) {
			auto& first = m_paralogMessages.front();
			if (message.time - first.time >= paralog * 1000) {
				m_paralogMessages.pop_front();
			} else {
				break;
			}
		}
	}

	if (logged) {
		if (std::this_thread::get_id() == m_mainThreadID)
		{
			WriteMessage(message);
		}
		else
		{
			std::lock_guard lock(m_mutex);

			m_messages.emplace_back(std::move(message));
		}
	}
}

int Logger::GetRequiredVerbosity(ILog::ELogType type)
{
	switch (type)
	{
		case ILog::eAlways:
		case ILog::eWarningAlways:
		case ILog::eErrorAlways:
		case ILog::eInput:
		case ILog::eInputResponse:
		{
			return 0;
		}
		case ILog::eError:
		{
			return 1;
		}
		case ILog::eWarning:
		{
			return 2;
		}
		case ILog::eMessage:
		{
			return 3;
		}
		case ILog::eComment:
		{
			return 4;
		}
	}

	return 0;
}

void Logger::BuildMessagePrefix(Message& message)
{
	if (!m_cvars.prefix)
	{
		// no log prefix until cvars are registered in the engine
		return;
	}

	const std::string_view prefix = m_cvars.prefix->GetString();

	if (prefix.empty() || prefix == "0")
	{
		// empty string or "0" means log prefix is disabled
		return;
	}

	message.prefix = FormatPrefix(prefix);

	if (!message.prefix.empty())
	{
		message.prefix += ' ';
	}
}

void Logger::BuildMessageContent(Message& message, const char* format, va_list args)
{
	switch (message.type)
	{
		case ILog::eWarning:
		case ILog::eWarningAlways:
		{
			message.content += CRY_COLOR_CODE_YELLOW_STRING "[Warning] ";
			break;
		}
		case ILog::eError:
		case ILog::eErrorAlways:
		{
			message.content += CRY_COLOR_CODE_RED_STRING "[Error] ";
			break;
		}
		case ILog::eComment:
		{
			message.content += CRY_COLOR_CODE_GRAY_STRING;
			break;
		}
		case ILog::eMessage:
		case ILog::eAlways:
		case ILog::eInput:
		case ILog::eInputResponse:
		{
			break;
		}
	}

	StringTools::FormatToV(message.content, format, args);
}

void Logger::WriteMessage(const Message& message)
{
	if (message.flags & Message::FLAG_FILE)
	{
		WriteMessageToFile(message);
	}

	if (message.flags & Message::FLAG_CONSOLE)
	{
		WriteMessageToConsole(message);
	}
}

void Logger::WriteMessageToFile(const Message& message)
{
	if (!m_file)
	{
		return;
	}

	std::string buffer;
	buffer.reserve(message.content.length() + 1);

	for (std::size_t i = 0; i < message.content.length(); i++)
	{
		if (message.content[i] == '$')
		{
			// drop color codes
			// and convert "$$" to "$"
			if ((i+1) < message.content.length() && message.content[++i] == '$')
			{
				buffer += '$';
			}
		}
		else
		{
			buffer += message.content[i];
		}
	}

	if (buffer.empty() || buffer.back() != '\n')
	{
		buffer += '\n';
	}

	std::fwrite(message.prefix.c_str(), 1, message.prefix.length(), m_file.get());
	std::fwrite(buffer.c_str(), 1, buffer.length(), m_file.get());
	std::fflush(m_file.get());

	for (ILogCallback* callback : m_callbacks)
	{
		callback->OnWriteToFile(message.content.c_str(), true);
	}
}

void Logger::WriteMessageToConsole(const Message& message)
{
	if (!gEnv)
	{
		return;
	}

	IConsole* pConsole = gEnv->pConsole;

	if (!pConsole)
	{
		return;
	}

	pConsole->PrintLine(message.content.c_str());

	for (ILogCallback* callback : m_callbacks)
	{
		callback->OnWriteToConsole(message.content.c_str(), true);
	}
}


void Logger::GetParalog(std::vector<Message>& outMessages) {
	std::lock_guard guard(m_mutex);
	for (auto& message : m_paralogMessages) {
		outMessages.push_back(message);
	}
}