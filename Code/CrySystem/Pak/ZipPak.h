#pragma once

#include <cstdio>
#include <memory>
#include <string>

#include <miniz/miniz.h>

#include "IPak.h"
#include "ZipCache.h"

class ZipPak final : public IPak
{
	mz_zip_archive m_zip = {};
	ZipCache m_cache;

public:
	ZipPak() = default;
	ZipPak(const ZipPak&) = delete;
	ZipPak& operator=(const ZipPak&) = delete;
	~ZipPak() noexcept;

	static std::unique_ptr<ZipPak> OpenFileHandle(std::FILE* file);
	static std::unique_ptr<ZipPak> OpenFileName(const std::string& adjustedName);
	static std::unique_ptr<ZipPak> OpenMemory(const void* mem, std::size_t size);

	////////////////////////////////////////////////////////////////////////////////
	// IPak
	////////////////////////////////////////////////////////////////////////////////

	std::uint32_t GetEntryCount() override;
	bool IsDirectoryEntry(std::uint32_t index) override;
	bool GetEntryPathAndSize(std::uint32_t index, std::string& path, std::uint64_t& size) override;

	std::unique_ptr<IFileInPak> OpenFile(std::uint32_t index, bool isBinary) override;
	void OnFileClosed(std::uint32_t index) override;

	std::size_t GetCachedDataSize() const override;

	////////////////////////////////////////////////////////////////////////////////

private:
	const char* GetCurrentErrorString();
	bool GetEntryStat(std::uint32_t index, mz_zip_archive_file_stat& stat);
	bool DecompressEntry(std::uint32_t index, ZipCache::DecompressedEntry& entry);
};
