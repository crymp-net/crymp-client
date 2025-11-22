#pragma once

#include <stdint.h>
#include <string>
#include <string_view>
#include <filesystem>
#include <functional>

#include "CryMP/Common/HTTP.h"

struct FileDownloaderProgress
{
	double speed = 0;              // bytes per second
	uint64_t contentLength = 0;    // HTTP content length, zero if not provided by the server
	uint64_t downloadedBytes = 0;  // current size of the file

	std::string ToString() const;
};

struct FileDownloaderResult
{
	std::string error;               // download error
	bool canceled = false;           // progress callback returned false
	int statusCode = 0;              // HTTP status code
	uint64_t contentLength = 0;      // HTTP content length, zero if not provided by the server
	uint64_t downloadedBytes = 0;    // total size of the file
	std::filesystem::path filePath;  // the output file path from the request
};

struct FileDownloaderRequest
{
	std::string url;
	std::filesystem::path filePath;
	std::function<bool(FileDownloaderProgress&)> onProgress;  // return false to cancel download
	std::function<void(FileDownloaderResult&)> onComplete;

	static std::string CreateUniqueFileName(std::string_view baseName, std::string_view extension);
	static std::filesystem::path MakeFileNameUnique(const std::filesystem::path& path);
};

class FileDownloader
{
public:
	FileDownloader();
	~FileDownloader();

	void Request(FileDownloaderRequest && request);
};
