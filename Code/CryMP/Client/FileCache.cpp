#include <algorithm>

#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CrySystem/ICryPak.h"
#include "Library/StdFile.h"
#include "Library/Util.h"

#include "FileCache.h"
#include "Client.h"
#include "FileDownloader.h"

json FileCache::LoadIndex()
{
	json index;

	if (StdFile file((m_cacheDir / "index").string().c_str(), "r"); file.IsOpen())
	{
		index = json::parse(file.GetHandle(), nullptr, false);  // no exceptions
	}

	if (!index.contains("files") || !index["files"].is_object())
	{
		index["files"] = json::object();
	}

	if (!index.contains("lru") || !index["lru"].is_array())
	{
		index["lru"] = json::array();
	}

	return index;
}

void FileCache::SaveIndex(const json & index)
{
	std::string content = index.dump(-1, ' ', false, json::error_handler_t::replace);  // no exceptions

	StdFile file((m_cacheDir / "index").string().c_str(), "w");
	if (!file.IsOpen())
	{
		CryLogAlways("$4[CryMP] [FileCache] Failed to open index file for writing");
		return;
	}

	file.Write(content.c_str(), content.length());
}

void FileCache::DownloadFile(FileCacheRequest && request, const std::filesystem::path & filePath)
{
	FileDownloaderRequest download;
	download.url = request.fileURL;
	download.filePath = FileDownloaderRequest::MakeFileNameUnique(filePath); // temporary file during download

	CryLogAlways("$3[CryMP] [FileCache] Downloading from $6%s$3", download.url.c_str());

	download.onProgress = [request](FileDownloaderProgress & progress) -> bool
	{
		if (request.onProgress)
			return request.onProgress("Downloading " + request.fileType + " " + progress.ToString());
		else
			return true;
	};

	download.onComplete = [request = std::move(request), finalPath = filePath, this](FileDownloaderResult & result)
	{
		bool success = false;

		if (!result.error.empty())
		{
			CryLogAlways("$4[CryMP] [FileCache] Download error: %s", result.error.c_str());
		}
		else if (result.canceled)
		{
			CryLogAlways("$6[CryMP] [FileCache] Download canceled!");
		}
		else if (result.statusCode != HTTP::STATUS_OK)
		{
			const int code = result.statusCode;
			const char *codeName = HTTP::StatusCodeToString(code);

			CryLogAlways("$4[CryMP] [FileCache] Bad status code %d (%s)!", code, codeName);
		}
		else if (result.downloadedBytes == 0)
		{
			CryLogAlways("$4[CryMP] [FileCache] Empty file!");
		}
		else if (result.contentLength && result.contentLength != result.downloadedBytes)
		{
			CryLogAlways("$4[CryMP] [FileCache] Incomplete file!");
		}
		else
		{
			CryLogAlways("$3[CryMP] [FileCache] Download finished");

			success = true;
		}

		if (success)
		{
			// rename the temporary file
			// no exceptions
			std::error_code error;
			std::filesystem::rename(result.filePath, finalPath, error);
			if (error)
			{
				CryLogAlways("$4[CryMP] [FileCache] Renaming temporary file failed: Error %d (%s)",
					error.value(), error.message().c_str());

				success = false;
			}
		}

		if (!success)
		{
			RemoveFile(result.filePath);
		}

		CompleteRequest(request, success, finalPath);
	};

	gClient->GetFileDownloader()->Request(std::move(download));
}

void FileCache::RemoveFile(const std::filesystem::path & path)
{
	CryLog("$3[CryMP] [FileCache] Removing %s", path.string().c_str());

	// no exceptions
	std::error_code code;
	std::filesystem::remove(path, code);
}

void FileCache::CompleteRequest(const FileCacheRequest & request, bool success, const std::filesystem::path & filePath)
{
	if (request.onComplete)
	{
		FileCacheResult result;
		result.success = success;
		result.filePath = filePath;

		request.onComplete(result);
	}
}

FileCache::FileCache()
{
	m_cacheDir = std::filesystem::canonical(gEnv->pCryPak->GetAlias("%USER%")) / "Downloads" / "Cache";

	// make sure the cache directory exists
	std::filesystem::create_directories(m_cacheDir);
}

FileCache::~FileCache()
{
}

void FileCache::Request(FileCacheRequest && request)
{
	const std::filesystem::path filePath = GetPath(request.fileURL);

	if (std::filesystem::exists(filePath))
	{
		CompleteRequest(request, true, filePath);
	}
	else
	{
		DownloadFile(std::move(request), filePath);
	}
}

std::filesystem::path FileCache::GetPath(const std::string_view & path)
{
	const std::string hash = Util::SHA256(path);

	json index = LoadIndex();
	json & lru = index["lru"];

	auto it = std::find_if(lru.begin(), lru.end(), [&hash](const json & item)
	{
		return item.is_string() && item.get_ref<const std::string&>() == hash;
	});

	if (it == lru.end())
	{
		lru.emplace_back(hash);
	}

	index["files"][hash] = path;

	SaveIndex(index);

	return m_cacheDir / hash;
}

std::deque<FileCacheEntry> FileCache::LoadEntries()
{
	std::deque<FileCacheEntry> entries;

	json index = LoadIndex();

	unsigned int order = 0;
	unsigned int removedCount = 0;
	std::vector<std::string> newEntries;

	for (const json & item : index["lru"])
	{
		if (item.is_string())
		{
			FileCacheEntry entry;
			entry.hash = item.get<std::string>();
			entry.path = m_cacheDir / entry.hash;

			if (std::filesystem::exists(entry.path))
			{
				entry.size = std::filesystem::file_size(entry.path);
				entry.order = order++;

				newEntries.emplace_back(entry.hash);
				entries.emplace_back(std::move(entry));
			}
			else
			{
				removedCount++;
			}
		}
	}

	if (removedCount > 0)
	{
		index["lru"] = newEntries;

		SaveIndex(index);
	}

	return entries;
}

std::deque<FileCacheEntry> FileCache::Cleanup()
{
	const uint64_t CACHE_MAX_SIZE = 2147483648ULL;

	std::deque<FileCacheEntry> entries = LoadEntries();

	uint64_t cacheSize = 0;
	for (const FileCacheEntry & entry : entries)
	{
		cacheSize += entry.size;
	}

	std::deque<FileCacheEntry> removed;

	// > 1, because we cannot remove very last file downloaded
	while (cacheSize > CACHE_MAX_SIZE && entries.size() > 1)
	{
		FileCacheEntry & entryToRemove = entries.front();

		RemoveFile(entryToRemove.path);

		cacheSize -= entryToRemove.size;

		removed.push_back(entryToRemove);
		entries.pop_front();
	}

	// Clean-up LRU array inside index
	LoadEntries();

	return removed;
}
