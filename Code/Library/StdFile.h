#pragma once

#include <cstdarg>
#include <cstdio>
#include <memory>
#include <string>

class StdFile
{
	struct FileCloser
	{
		void operator()(std::FILE* file) const
		{
			std::fclose(file);
		}
	};

	std::unique_ptr<std::FILE, FileCloser> m_file;

public:
	StdFile() = default;

	explicit StdFile(const char* path, const char* mode) : m_file(std::fopen(path, mode))
	{
	}

	bool IsOpen() const
	{
		return m_file != nullptr;
	}

	void Open(const char* path, const char* mode)
	{
		m_file.reset(std::fopen(path, mode));
	}

	void Close()
	{
		m_file.reset();
	}

	std::FILE* GetHandle()
	{
		return m_file.get();
	}

	std::size_t Read(char* buffer, std::size_t size)
	{
		return std::fread(buffer, 1, size, m_file.get());
	}

	std::size_t Write(const char* buffer, std::size_t size)
	{
		return std::fwrite(buffer, 1, size, m_file.get());
	}

	int Printf(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		int result = std::vfprintf(m_file.get(), format, args);
		va_end(args);

		return result;
	}

	std::string ReadAll();
};
