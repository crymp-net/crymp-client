#include <cstring>

#include "CrySystem/CryLog.h"

#include "FileInZipPak.h"

////////////////////////////////////////////////////////////////////////////////
// IFileInPak
////////////////////////////////////////////////////////////////////////////////

std::size_t FileInZipPak::FRead(void* buffer, std::size_t elementSize, std::size_t elementCount)
{
	const std::size_t requestedBytes = elementSize * elementCount;
	if (requestedBytes == 0)
	{
		return 0;
	}

	if (this->isBinary)
	{
		std::size_t totalBytes = this->size - this->pos;
		if (totalBytes > requestedBytes)
		{
			totalBytes = requestedBytes;
		}

		std::memcpy(buffer, &this->data[this->pos], totalBytes);

		this->pos += totalBytes;

		return totalBytes / elementSize;
	}
	else
	{
		const std::byte* in = this->data + this->pos;
		const std::byte* const inEnd = this->data + this->size;

		char* out = static_cast<char*>(buffer);
		char* const outEnd = out + requestedBytes;

		while (in != inEnd && out != outEnd)
		{
			char ch = std::to_integer<char>(*in++);

			// convert CRLF to LF in text mode
			if (ch == '\r' && in != inEnd && std::to_integer<char>(*in) == '\n')
			{
				ch = '\n';
				++in;
			}

			*out++ = ch;
		}

		this->pos = in - this->data;

		return (out - static_cast<char*>(buffer)) / elementSize;
	}
}

std::size_t FileInZipPak::FWrite(const void* buffer, std::size_t elementSize, std::size_t elementCount)
{
	CryLogErrorAlways("%s: Not implemented!", __FUNCTION__);
	return 0;
}

int FileInZipPak::VFPrintF(const char* format, va_list args)
{
	CryLogErrorAlways("%s: Not implemented!", __FUNCTION__);
	return -1;
}

char* FileInZipPak::FGetS(char* buffer, int bufferSize)
{
	if (buffer == nullptr || bufferSize <= 1)
	{
		return nullptr;
	}

	const bool isBinary = this->isBinary;

	const std::byte* in = this->data + this->pos;
	const std::byte* const inEnd = this->data + this->size;

	char* out = buffer;
	char* const outEnd = buffer + (bufferSize - 1);  // null terminator

	while (in != inEnd && out != outEnd)
	{
		char ch = std::to_integer<char>(*in++);

		// convert CRLF to LF in text mode
		if (!isBinary && ch == '\r' && in != inEnd && std::to_integer<char>(*in) == '\n')
		{
			ch = '\n';
			++in;
		}

		*out++ = ch;

		if (ch == '\n')
		{
			break;
		}
	}

	this->pos = in - this->data;

	*out = '\0';

	return buffer;
}

int FileInZipPak::FGetC()
{
	const std::byte* in = this->data + this->pos;
	const std::byte* const inEnd = this->data + this->size;

	if (in == inEnd)
	{
		return EOF;
	}

	int ch = std::to_integer<int>(*in++);

	// convert CRLF to LF in text mode
	if (!this->isBinary && ch == '\r' && in != inEnd && std::to_integer<char>(*in) == '\n')
	{
		ch = '\n';
		++in;
	}

	this->pos = in - this->data;

	return ch;
}

int FileInZipPak::FUnGetC(int ch)
{
	CryLogErrorAlways("%s: Not implemented!", __FUNCTION__);
	return EOF;
}

int FileInZipPak::FSeek(std::int64_t offset, int mode)
{
	std::int64_t newPos = 0;

	switch (mode)
	{
		case SEEK_SET:
		{
			newPos = offset;
			break;
		}
		case SEEK_CUR:
		{
			newPos = this->pos;
			newPos += offset;
			break;
		}
		case SEEK_END:
		{
			newPos = this->size;
			newPos += offset;
			break;
		}
		default:
		{
			return -1;
		}
	}

	if (newPos < 0)
	{
		newPos = 0;
	}
	else if (static_cast<std::uint64_t>(newPos) > this->size)
	{
		newPos = this->size;
	}

	this->pos = static_cast<std::size_t>(newPos);

	return 0;
}

std::int64_t FileInZipPak::FTell() const
{
	return this->pos;
}

std::uint64_t FileInZipPak::GetSize() const
{
	return this->size;
}

int FileInZipPak::FFlush()
{
	return 0;
}

int FileInZipPak::FEof() const
{
	return this->pos >= this->size;
}

int FileInZipPak::FError() const
{
	return 0;
}

void* FileInZipPak::GetCachedFileData(std::size_t& fileSize)
{
	fileSize = this->size;

	return this->data;
}

std::uint64_t FileInZipPak::GetModificationTime()
{
	return this->modificationTime;
}

std::FILE* FileInZipPak::GetHandle()
{
	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
