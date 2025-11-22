#include "CrySystem/CryLog.h"
#include "Library/WinAPI.h"

#include "FileOutsidePak.h"

#if defined(_MSC_VER) || defined(__MINGW64__)
#define FSEEK64 _fseeki64
#define FTELL64 _ftelli64
#else
#define FSEEK64 std::fseek
#define FTELL64 std::ftell
#endif

////////////////////////////////////////////////////////////////////////////////
// IFileInPak
////////////////////////////////////////////////////////////////////////////////

std::size_t FileOutsidePak::FRead(void* buffer, std::size_t elementSize, std::size_t elementCount)
{
	return std::fread(buffer, elementSize, elementCount, m_handle.get());
}

std::size_t FileOutsidePak::FWrite(const void* buffer, std::size_t elementSize, std::size_t elementCount)
{
	return std::fwrite(buffer, elementSize, elementCount, m_handle.get());
}

int FileOutsidePak::VFPrintF(const char* format, va_list args)
{
	return std::vfprintf(m_handle.get(), format, args);
}

char* FileOutsidePak::FGetS(char* buffer, int bufferSize)
{
	return std::fgets(buffer, bufferSize, m_handle.get());
}

int FileOutsidePak::FGetC()
{
	return std::fgetc(m_handle.get());
}

int FileOutsidePak::FUnGetC(int ch)
{
	return std::ungetc(ch, m_handle.get());
}

int FileOutsidePak::FSeek(std::int64_t offset, int mode)
{
	return FSEEK64(m_handle.get(), offset, mode);
}

std::int64_t FileOutsidePak::FTell() const
{
	return FTELL64(m_handle.get());
}

std::uint64_t FileOutsidePak::GetSize() const
{
	const auto pos = FTELL64(m_handle.get());
	FSEEK64(m_handle.get(), 0, SEEK_END);
	const auto size = FTELL64(m_handle.get());
	FSEEK64(m_handle.get(), pos, SEEK_SET);

	return (size < 0) ? 0 : size;
}

int FileOutsidePak::FFlush()
{
	return std::fflush(m_handle.get());
}

int FileOutsidePak::FEof() const
{
	return std::feof(m_handle.get());
}

int FileOutsidePak::FError() const
{
	return std::ferror(m_handle.get());
}

void* FileOutsidePak::GetCachedFileData(std::size_t& fileSize)
{
	if (!m_data)
	{
		const std::uint64_t rawSize = this->GetSize();

		if (rawSize > 0x7fffffff)
		{
			CryLogErrorAlways("%s: Too big file \"%s\"", __FUNCTION__, m_path.string().c_str());
			return nullptr;
		}

		m_dataSize = static_cast<std::size_t>(rawSize);
		m_data = std::make_unique<std::byte[]>(m_dataSize);

		const auto pos = FTELL64(m_handle.get());
		FSEEK64(m_handle.get(), 0, SEEK_SET);
		std::fread(m_data.get(), 1, m_dataSize, m_handle.get());
		FSEEK64(m_handle.get(), pos, SEEK_SET);
	}

	fileSize = m_dataSize;

	return m_data.get();
}

std::uint64_t FileOutsidePak::GetModificationTime()
{
	return WinAPI::GetLastWriteTime(m_path);
}

std::FILE* FileOutsidePak::GetHandle()
{
	return m_handle.get();
}

////////////////////////////////////////////////////////////////////////////////
