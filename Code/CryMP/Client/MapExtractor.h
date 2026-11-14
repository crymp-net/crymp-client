#pragma once

#include <string>
#include <filesystem>

#include <miniz/miniz.h>

class MapExtractor
{
	mz_zip_archive m_zip;
	std::filesystem::path m_dirPath;
	std::filesystem::path m_mapPath;

	const char* GetErrorString();
	std::filesystem::path GetFilePath(unsigned int index);
	void ExtractFile(unsigned int index);

public:
	MapExtractor(const std::filesystem::path & zipPath, const std::filesystem::path & mapName);
	~MapExtractor();

	void Extract();
};
