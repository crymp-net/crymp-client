#pragma once

#include <stdint.h>
#include <string>
#include <string_view>
#include <functional>
#include <filesystem>
#include <deque>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct FileCacheResult
{
	bool success = false;
	std::filesystem::path filePath;
};

struct FileCacheRequest
{
	std::string fileURL;
	std::string fileType;
	std::function<bool(const std::string&)> onProgress;  // progress message, return false to cancel download
	std::function<void(FileCacheResult&)> onComplete;
};

struct FileCacheEntry
{
	std::filesystem::path path;
	std::string hash;
	uint64_t size = 0;
	unsigned int order = 0;
};

class FileCache
{
	std::filesystem::path m_cacheDir;

	json LoadIndex();
	void SaveIndex(const json & index);
	void DownloadFile(FileCacheRequest && request, const std::filesystem::path & filePath);
	void RemoveFile(const std::filesystem::path & path);
	void CompleteRequest(const FileCacheRequest & request, bool success, const std::filesystem::path & filePath);

public:
	FileCache();
	~FileCache();

	void Request(FileCacheRequest && request);

	std::filesystem::path GetPath(const std::string_view & path);

	std::deque<FileCacheEntry> LoadEntries();
	std::deque<FileCacheEntry> Cleanup(unsigned maxAge);
};
