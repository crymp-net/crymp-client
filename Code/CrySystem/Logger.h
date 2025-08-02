#pragma once

#include <cstdio>
#include <filesystem>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "CryCommon/CrySystem/ILog.h"

struct ICVar;

class Logger final : public ILog
{
public:
	struct Message
	{
		enum
		{
			FLAG_FILE    = (1 << 0),
			FLAG_CONSOLE = (1 << 1),
		};

		ILog::ELogType type = ILog::eMessage;
		unsigned int flags = 0;
		std::string prefix;
		std::string content;
		uint64_t time = 0;
	};

private:
	struct FileHandleDeleter
	{
		void operator()(std::FILE* file) const
		{
			std::fclose(file);
		}
	};

	using FileHandle = std::unique_ptr<std::FILE, FileHandleDeleter>;

	int m_verbosity = 0;
	int m_paralog = 0;
	FileHandle m_file;
	std::filesystem::path m_filePath;
	std::string m_fileName;
	std::string m_prefix;

	struct CVars
	{
		ICVar* verbosity = nullptr;
		ICVar* fileVerbosity = nullptr;
		ICVar* prefix = nullptr;
		ICVar* paralog = nullptr;
	};

	CVars m_cvars;
	std::mutex m_mutex;
	std::thread::id m_mainThreadID;
	std::vector<Message> m_messages;
	std::vector<ILogCallback*> m_callbacks;

	std::list<Message> m_paralogMessages;

	static Logger s_globalInstance;

public:
	Logger();
	~Logger();

	// to be removed once we have our own CrySystem
	static Logger& GetInstance()
	{
		return s_globalInstance;
	}

	void OnUpdate();

	void OpenFile(const std::filesystem::path& filePath);
	void CloseFile();

	std::FILE* GetFileHandle()
	{
		return m_file.get();
	}

	const std::filesystem::path& GetFilePath() const
	{
		return m_filePath;
	}

	void SetPrefix(const std::string_view& prefix);

	static std::string FormatPrefix(const std::string_view& prefix);

	void LogAlways(const char* format, ...);

	////////////////////////////////////////////////////////////////////////////////
	// ILog
	////////////////////////////////////////////////////////////////////////////////

	void LogV(ILog::ELogType type, const char* format, va_list args) override;

	void Log(const char* format, ...) override;
	void LogWarning(const char* format, ...) override;
	void LogError(const char* format, ...) override;

	void Release() override;

	bool SetFileName(const char* fileName) override;
	const char* GetFileName() override;

	void LogPlus(const char* format, ...) override;
	void LogToFile(const char* format, ...) override;
	void LogToFilePlus(const char* format, ...) override;
	void LogToConsole(const char* format, ...) override;
	void LogToConsolePlus(const char* format, ...) override;

	void UpdateLoadingScreen(const char* format, ...) override;

	void RegisterConsoleVariables() override;
	void UnregisterConsoleVariables() override;

	void SetVerbosity(int verbosity) override;
	int GetVerbosityLevel() override;

	void AddCallback(ILogCallback* callback) override;
	void RemoveCallback(ILogCallback* callback) override;

	void GetParalog(std::vector<Message>& outMessages);

	////////////////////////////////////////////////////////////////////////////////

private:
	void PushMessage(ILog::ELogType type, unsigned int flags, const char* format, ...);
	void PushMessageV(ILog::ELogType type, unsigned int flags, const char* format, va_list args);

	int GetRequiredVerbosity(ILog::ELogType type);

	void BuildMessagePrefix(Message& message);
	void BuildMessageContent(Message& message, const char* format, va_list args);

	void WriteMessage(const Message& message);

	void WriteMessageToFile(const Message& message);
	void WriteMessageToConsole(const Message& message);
};
