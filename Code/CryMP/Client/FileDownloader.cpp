#include <atomic>
#include <chrono>

#include "CryMP/Common/Executor.h"
#include "CrySystem/RandomGenerator.h"
#include "Library/StdFile.h"
#include "Library/StringTools.h"
#include "Library/Util.h"
#include "Library/WinAPI.h"

#include "FileDownloader.h"
#include "SpeedAggregator.h"
#include "Client.h"

struct FileDownloaderTask : public IExecutorTask
{
	FileDownloaderRequest request;
	FileDownloaderResult result;
	SpeedAggregator speedAggregator;
	std::atomic<bool> isActive;
	std::chrono::time_point<std::chrono::steady_clock> lastProgressUpdate;

	void UpdateProgress(size_t chunkLength)
	{
		const auto now = std::chrono::steady_clock::now();

		speedAggregator.push(chunkLength);

		if ((now - lastProgressUpdate) >= std::chrono::seconds(1))
		{
			FileDownloaderProgress progress;
			progress.speed = speedAggregator.getSpeed();
			progress.contentLength = result.contentLength;
			progress.downloadedBytes = result.downloadedBytes;

			gClient->GetExecutor()->RunOnMainThread([this, progress]() mutable
			{
				if (request.onProgress)
				{
					isActive = request.onProgress(progress);
				}
			});

			lastProgressUpdate = now;
		}
	}

	// worker thread
	void Execute() override
	{
		isActive = true;

		try
		{
			result.statusCode = WinAPI::HTTPRequest(
				"GET",
				request.url,
				{},  // data
				{},  // headers
				[this](uint64_t contentLength, const WinAPI::HTTPRequestReader& reader)
				{
					StdFile file(request.filePath.string().c_str(), "wb");
					if (!file.IsOpen())
					{
						throw StringTools::SysErrorErrnoFormat("Output file open failed");
					}

					// content length is zero if not provided by the server
					result.contentLength = contentLength;

					while (isActive)
					{
						char chunk[8192];
						const size_t chunkLength = reader(chunk, sizeof chunk);
						if (chunkLength == 0)
						{
							break;
						}

						file.Write(chunk, chunkLength);
						result.downloadedBytes += chunkLength;

						UpdateProgress(chunkLength);
					}
				}
			);
		}
		catch (const std::exception& ex)
		{
			result.error = ex.what();
		}
	}

	// main thread
	void Callback() override
	{
		if (request.onComplete)
		{
			result.canceled = !isActive;
			result.filePath = std::move(request.filePath);

			request.onComplete(result);
		}
	}
};

std::string FileDownloaderProgress::ToString() const
{
	const std::string speedString = Util::MakeHumanReadableBytes(speed);

	if (contentLength)
	{
		const double percentage = (100.0 * downloadedBytes) / contentLength;
		const double secondsRemaining = (contentLength - downloadedBytes) / speed;
		const std::string timeRemaining = Util::SecondsToString(secondsRemaining);

		return StringTools::Format("%.0f%% (%s/s, %s)", percentage, speedString.c_str(), timeRemaining.c_str());
	}
	else
	{
		return StringTools::Format("?? (%s/s, ??)", speedString.c_str());
	}
}

std::string FileDownloaderRequest::CreateUniqueFileName(std::string_view baseName, std::string_view extension)
{
	std::string name(baseName);

	// lowercase
	StringTools::ToLowerInPlace(name);

	// sanitize
	for (char& ch : name)
	{
		if (!(ch >= '0' && ch <= '9') && !(ch >= 'a' && ch <= 'z'))
		{
			ch = '_';
		}
	}

	// add a random suffix for uniqueness
	StringTools::FormatTo(name, "_%09u", RandomGenerator::GenerateUInt32(0, 999'999'999));

	// extension
	name += extension;

	return name;
}

std::filesystem::path FileDownloaderRequest::MakeFileNameUnique(const std::filesystem::path& path)
{
	std::filesystem::path newPath(path);
	newPath.replace_filename(CreateUniqueFileName(path.filename().string(), path.extension().string()));
	return newPath;
}

FileDownloader::FileDownloader()
{
}

FileDownloader::~FileDownloader()
{
}

void FileDownloader::Request(FileDownloaderRequest && request)
{
	std::unique_ptr<FileDownloaderTask> task = std::make_unique<FileDownloaderTask>();
	task->request = std::move(request);

	gClient->GetExecutor()->AddTask(std::move(task));
}
