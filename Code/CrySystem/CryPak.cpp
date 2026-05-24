#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <utility>

#include "Library/PathTools.h"

#include "CryLog.h"
#include "CryPak.h"
#include "Pak/FileOutsidePak.h"
#include "Pak/ZipPak.h"
#include "ResourceList.h"

#include "config.h"

class CryPakWildcardMatcher
{
	enum class Wildcard
	{
		NONE, STEM, EXTENSION, FULL
	};

	Wildcard m_kind = Wildcard::NONE;
	std::string_view m_stem;
	std::string_view m_extension;

public:
	// | wildcard | kind      |
	// | :------: | :-------: |
	// | `xxx`    | NONE      |
	// | `*.xxx`  | STEM      |
	// | `xxx.*`  | EXTENSION |
	// | `*.*`    | FULL      |
	// | `*`      | FULL      |
	explicit CryPakWildcardMatcher(std::string_view wildcard)
	{
		std::tie(m_stem, m_extension) = PathTools::SplitNameIntoStemAndExtension(wildcard);

		if (!m_stem.empty() && m_stem.back() == '*')
		{
			m_stem.remove_suffix(1);
			m_kind = m_extension.empty() ? Wildcard::FULL : Wildcard::STEM;
		}

		if (!m_extension.empty() && m_extension.back() == '*')
		{
			m_extension.remove_suffix(1);
			m_kind = (m_kind == Wildcard::STEM) ? Wildcard::FULL : Wildcard::EXTENSION;
		}

		const auto hasWildcard = [](std::string_view x) {
			return x.find_first_of("*?") != std::string_view::npos;
		};

		if (hasWildcard(m_stem) || hasWildcard(m_extension))
		{
			CryLogErrorAlways("CryPakWildcardMatcher(\"%.*s\"): Unsupported wildcard!",
				static_cast<int>(wildcard.size()), wildcard.data());
		}
	}

	bool operator()(std::string_view name) const
	{
		auto [stem, extension] = PathTools::SplitNameIntoStemAndExtension(name);

		const auto check = [](std::string_view a, std::string_view b, bool prefix) -> bool
		{
			if (prefix)
			{
				return StringTools::StartsWithNoCase(a, b);
			}
			else
			{
				return StringTools::IsEqualNoCase(a, b);
			}
		};

		if (!check(stem, m_stem, m_kind == Wildcard::STEM || m_kind == Wildcard::FULL))
		{
			return false;
		}

		if (!check(extension, m_extension, m_kind == Wildcard::EXTENSION || m_kind == Wildcard::FULL))
		{
			return false;
		}

		return true;
	}
};

static FILE* ToDirtyHandle(const SlotVectorHandle& handle)
{
	return reinterpret_cast<FILE*>(static_cast<std::uintptr_t>(handle.GetValue()));
}

static SlotVectorHandle FromDirtyHandle(FILE* handle)
{
	return SlotVectorHandle(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(handle)));
}

#ifdef CRYMP_PAK_HOT_RELOADING_PATH
static std::string TryDevPakFolder(std::string_view path)
{
	std::string result;

	if (StringTools::StartsWithNoCase(path, "Game/"))
	{
		result = path;
		// replace "Game" prefix with the path
		result.replace(0, 4, CRYMP_PAK_HOT_RELOADING_PATH);

		std::error_code ec;
		if (!std::filesystem::exists(result, ec))
		{
			// not found
			result.clear();
		}
	}

	return result;
}
#endif

CryPak::CryPak()
{
	m_resourceList_EngineStartup = ResourceList::Create();
	m_resourceList_NextLevel = ResourceList::Create();
	m_resourceList_Level = ResourceList::Create();
}

CryPak::~CryPak()
{
}

////////////////////////////////////////////////////////////////////////////////
// ICryPak
////////////////////////////////////////////////////////////////////////////////

const char* CryPak::AdjustFileName(const char* src, char* dst, unsigned int flags, bool* foundInPak)
{
	std::lock_guard lock(m_mutex);

	if (foundInPak)
	{
		CryLogWarningAlways("%s(\"%s\"): Ignoring foundInPak!", __FUNCTION__, src);
		*foundInPak = false;
	}

	const std::string adjusted = this->AdjustFileNameImpl(StringTools::SafeView(src), flags);

	// we don't know the actual size of the dst buffer
	// it's supposed to be ICryPak::g_nMaxPath, which is 2048, but it's not always the case!
	// let's hope all buffers out there are at least MAX_PATH, which is 260
	if (adjusted.length() < 260)
	{
		std::memcpy(dst, adjusted.c_str(), adjusted.length() + 1);
	}
	else
	{
		CryLogErrorAlways("%s(\"%s\"): Too long path!", __FUNCTION__, src);
		dst[0] = '\0';
	}

	return dst;
}

bool CryPak::Init(const char* basePath)
{
	return true;
}

void CryPak::Release()
{
}

bool CryPak::OpenPack(const char* name, unsigned int flags)
{
	const std::string root(PathTools::DirPath(StringTools::SafeView(name)));

	return this->OpenPack(root.c_str(), name, flags);
}

bool CryPak::OpenPack(const char* root, const char* name, unsigned int flags)
{
	std::lock_guard lock(m_mutex);

	SlotGuard<PakSlot> pak = this->GetFreePakSlot();
	if (!pak)
	{
		CryLogErrorAlways("%s(\"%s\", \"%s\", 0x%x): No free pak slot!", __FUNCTION__, root, name, flags);
		return false;
	}

	pak->path = this->AdjustFileNameImpl(StringTools::SafeView(name), flags);
	pak->root = this->AdjustFileNameImpl(StringTools::SafeView(root), flags);

	if (this->FindLoadedPakByPath(pak->path))
	{
		CryLog("%s(\"%s\", \"%s\", 0x%x): Already loaded", __FUNCTION__, root, name, flags);
		return true;
	}

	if (!this->OpenPakImpl(pak, Priority::NORMAL))
	{
		CryLogErrorAlways("%s(\"%s\", \"%s\", 0x%x): Failed!", __FUNCTION__, root, name, flags);
		return false;
	}

	CryLog("%s(\"%s\", \"%s\", 0x%x): Success", __FUNCTION__, root, name, flags);

	// release ownership
	pak.release();
	return true;
}

bool CryPak::ClosePack(const char* name, unsigned int flags)
{
	std::lock_guard lock(m_mutex);

	const std::string adjustedName = this->AdjustFileNameImpl(StringTools::SafeView(name), flags);

	PakSlot* pak = this->FindLoadedPakByPath(adjustedName);
	if (!pak)
	{
		CryLogWarningAlways("%s(\"%s\", 0x%x): Not found!", __FUNCTION__, name, flags);
		return false;
	}

	this->CloseSlot(pak);

	CryLog("%s(\"%s\", 0x%x)", __FUNCTION__, name, flags);

	return true;
}

bool CryPak::OpenPacks(const char* wildcard, unsigned int flags)
{
	const std::string root(PathTools::DirPath(StringTools::SafeView(wildcard)));

	return this->OpenPacks(root.c_str(), wildcard, flags);
}

bool CryPak::OpenPacks(const char* root, const char* wildcard, unsigned int flags)
{
	std::lock_guard lock(m_mutex);

	CryLog("%s(\"%s\", \"%s\", 0x%x): Opening paks...", __FUNCTION__, root, wildcard, flags);

	const std::string adjustedWildcard = this->AdjustFileNameImpl(StringTools::SafeView(wildcard), flags);
	const std::string adjustedRoot = this->AdjustFileNameImpl(StringTools::SafeView(root), flags);
	const std::string_view dirPath = PathTools::DirPath(adjustedWildcard);

	bool success = false;

	for (const auto& entry : this->SearchWildcardPath(adjustedWildcard))
	{
		SlotGuard<PakSlot> pak = this->GetFreePakSlot();
		if (!pak)
		{
			CryLogErrorAlways("%s(\"%s\", \"%s\", 0x%x): No free pak slot!", __FUNCTION__, root, wildcard,
				flags);
			return false;
		}

		pak->path = PathTools::Join(dirPath, entry.name);
		pak->root = adjustedRoot;

		if (this->FindLoadedPakByPath(pak->path))
		{
			CryLog("%s(\"%s\", \"%s\", 0x%x): Already loaded \"%s\"", __FUNCTION__, root, wildcard, flags,
				pak->path.c_str());
			success = true;
			continue;
		}

		if (!this->OpenPakImpl(pak, Priority::NORMAL))
		{
			CryLogErrorAlways("%s(\"%s\", \"%s\", 0x%x): Failed to open \"%s\"", __FUNCTION__, root,
				wildcard, flags, pak->path.c_str());
			continue;
		}

		CryLog("%s(\"%s\", \"%s\", 0x%x): Opened \"%s\"", __FUNCTION__, root, wildcard, flags,
			pak->path.c_str());

		// release ownership
		pak.release();
		success = true;
	}

	return success;
}

bool CryPak::ClosePacks(const char* wildcard, unsigned int flags)
{
	std::lock_guard lock(m_mutex);

	CryLog("%s(\"%s\", 0x%x): Closing paks...", __FUNCTION__, wildcard, flags);

	const std::string adjustedWildcard = this->AdjustFileNameImpl(StringTools::SafeView(wildcard), flags);
	const std::string_view dirPath = PathTools::DirPath(adjustedWildcard);

	bool success = false;

	for (const auto& entry : this->SearchWildcardPath(adjustedWildcard))
	{
		const std::string path = PathTools::Join(dirPath, entry.name);

		PakSlot* pak = this->FindLoadedPakByPath(path);
		if (!pak)
		{
			continue;
		}

		this->CloseSlot(pak);

		CryLog("%s(\"%s\", 0x%x): Closed \"%s\"", __FUNCTION__, wildcard, flags, path.c_str());

		success = true;
	}

	return success;
}

void CryPak::AddMod(const char* mod)
{
	CryLogErrorAlways("%s(\"%s\"): Not implemented!", __FUNCTION__, mod);
}

void CryPak::RemoveMod(const char* mod)
{
	CryLogErrorAlways("%s(\"%s\"): Not implemented!", __FUNCTION__, mod);
}

void CryPak::ParseAliases(const char* commandLine)
{
	CryLogErrorAlways("%s: Not implemented!", __FUNCTION__);
}

void CryPak::SetAlias(const char* name, const char* alias, bool add)
{
	std::lock_guard lock(m_mutex);

	const std::string_view aliasName = StringTools::SafeView(name);
	if (aliasName.length() < 3 || aliasName.front() != '%' || aliasName.back() != '%')
	{
		CryLogErrorAlways("%s: Invalid alias name \"%s\"", __FUNCTION__, name);
		return;
	}

	if (add)
	{
#ifdef __cpp_lib_associative_heterogeneous_insertion
		const auto [it, added] = m_aliases.try_emplace(aliasName);
#else
		const auto [it, added] = m_aliases.try_emplace(std::string(aliasName));
#endif

		if (added)
		{
			CryLog("%s: Adding \"%s\" -> \"%s\"", __FUNCTION__, name, alias);
		}
		else
		{
			CryLog("%s: Changing \"%s\" -> \"%s\" to \"%s\"", __FUNCTION__,
				name, it->second.c_str(), alias);
		}

		it->second = StringTools::SafeView(alias);
	}
	else
	{
		const auto it = m_aliases.find(aliasName);
		if (it == m_aliases.end())
		{
			CryLog("%s: \"%s\" does not exist", __FUNCTION__, name);
			return;
		}

		CryLog("%s: Removing \"%s\" -> \"%s\"", __FUNCTION__, name, it->second.c_str());

		m_aliases.erase(it);
	}
}

const char* CryPak::GetAlias(const char* name, bool returnSame)
{
	std::lock_guard lock(m_mutex);

	const auto it = m_aliases.find(StringTools::SafeView(name));
	if (it == m_aliases.end())
	{
		return returnSame ? name : nullptr;
	}

	return it->second.c_str();
}

ICryPak::PakInfo* CryPak::GetPakInfo()
{
	std::lock_guard lock(m_mutex);

	const unsigned int pakCount = m_paks.GetActiveCount();

	PakInfo* info = static_cast<PakInfo*>(std::calloc(1, sizeof(PakInfo) + (sizeof(PakInfo::Pak) * pakCount)));
	if (info) {
		info->numOpenPaks = pakCount;

		const auto my_strdup = [](std::string_view str) -> char*
			{
				char* res = static_cast<char*>(std::malloc(str.length() + 1));
				if (res) {
					std::memcpy(res, str.data(), str.length());
					res[str.length()] = '\0';
				}
				return res;
			};

		PakSlot* pak = m_paks.GetFirstActive();

		for (unsigned int i = 0; i < pakCount; ++i)
		{
			info->arrPaks[i].szFilePath = my_strdup(pak->path);
			info->arrPaks[i].szBindRoot = my_strdup(pak->root);
			info->arrPaks[i].nUsedMem = pak->impl->GetCachedDataSize();

			pak = m_paks.GetNextActive(pak);
		}
	}

	return info;
}

void CryPak::FreePakInfo(PakInfo* pakInfo)
{
	// no need to lock m_mutex

	const unsigned int pakCount = pakInfo->numOpenPaks;

	for (unsigned int i = 0; i < pakCount; ++i)
	{
		std::free(const_cast<char*>(pakInfo->arrPaks[i].szFilePath));
		std::free(const_cast<char*>(pakInfo->arrPaks[i].szBindRoot));
	}

	std::free(pakInfo);
}

FILE* CryPak::FOpen(const char* name, const char* mode, unsigned int flags)
{
	std::lock_guard lock(m_mutex);

	SlotGuard<FileSlot> file = this->GetFreeFileSlot();
	if (!file)
	{
		CryLogErrorAlways("%s(\"%s\", \"%s\", 0x%x): No free file slot!", __FUNCTION__, name, mode, flags);
		return nullptr;
	}

	file->path = this->AdjustFileNameImpl(StringTools::SafeView(name), flags);
	file->mode = this->FileModeFromString(StringTools::SafeView(mode));

	if (!this->OpenFileImpl(file))
	{
		CryLogComment("%s(\"%s\", \"%s\", 0x%x): Failed!", __FUNCTION__, name, mode, flags);
		return nullptr;
	}

	FILE* handle = ToDirtyHandle(m_files.SlotToHandle(file.get()));

	if (file->pakHandle)
	{
		PakSlot* pak = m_paks.HandleToSlot(file->pakHandle);
		CryLogComment("%s(\"%s\", \"%s\", 0x%x): 0x%p Found in pak \"%s\"", __FUNCTION__, name, mode, flags,
			handle, pak ? pak->path.c_str() : "");
	}
	else
	{
		CryLogComment("%s(\"%s\", \"%s\", 0x%x): 0x%p Found outside \"%s\"", __FUNCTION__, name, mode, flags,
			handle, file->path.c_str());
	}

	// release ownership
	file.release();
	return handle;
}

FILE* CryPak::FOpen(const char* name, const char* mode, char* fileGamePath, int length)
{
	CryLogErrorAlways("%s(\"%s\", \"%s\", 0x%p, %d): Not implemented!", __FUNCTION__, name, mode,
		fileGamePath, length);
	return nullptr;
}

size_t CryPak::FReadRaw(void* data, size_t elementSize, size_t elementCount, FILE* handle)
{
	std::lock_guard lock(m_mutex);

	FileSlot* file = m_files.HandleToSlot(FromDirtyHandle(handle));
	if (!file)
	{
		CryLogErrorAlways("%s(0x%p): Invalid handle!", __FUNCTION__, handle);
		return 0;
	}

	return file->impl->FRead(data, elementSize, elementCount);
}

size_t CryPak::FReadRawAll(void* data, size_t fileSize, FILE* handle)
{
	std::lock_guard lock(m_mutex);

	FileSlot* file = m_files.HandleToSlot(FromDirtyHandle(handle));
	if (!file)
	{
		CryLogErrorAlways("%s(0x%p): Invalid handle!", __FUNCTION__, handle);
		return 0;
	}

	if (static_cast<size_t>(file->impl->GetSize()) != fileSize)
	{
		return 0;
	}

	if (file->impl->FSeek(0, SEEK_SET) < 0)
	{
		return 0;
	}

	return file->impl->FRead(data, 1, fileSize);
}

void* CryPak::FGetCachedFileData(FILE* handle, size_t& fileSize)
{
	std::lock_guard lock(m_mutex);

	FileSlot* file = m_files.HandleToSlot(FromDirtyHandle(handle));
	if (!file)
	{
		CryLogErrorAlways("%s(0x%p): Invalid handle!", __FUNCTION__, handle);
		return nullptr;
	}

	return file->impl->GetCachedFileData(fileSize);
}

size_t CryPak::FWrite(const void* data, size_t elementSize, size_t elementCount, FILE* handle)
{
	std::lock_guard lock(m_mutex);

	FileSlot* file = m_files.HandleToSlot(FromDirtyHandle(handle));
	if (!file)
	{
		CryLogErrorAlways("%s(0x%p): Invalid handle!", __FUNCTION__, handle);
		return 0;
	}

	return file->impl->FWrite(data, elementSize, elementCount);
}

int CryPak::FPrintf(FILE* handle, const char* format, ...)
{
	std::lock_guard lock(m_mutex);

	FileSlot* file = m_files.HandleToSlot(FromDirtyHandle(handle));
	if (!file)
	{
		CryLogErrorAlways("%s(0x%p): Invalid handle!", __FUNCTION__, handle);
		return -1;
	}

	va_list args;
	va_start(args, format);
	const int length = file->impl->VFPrintF(format, args);
	va_end(args);

	return length;
}

char* CryPak::FGets(char* buffer, int bufferSize, FILE* handle)
{
	std::lock_guard lock(m_mutex);

	FileSlot* file = m_files.HandleToSlot(FromDirtyHandle(handle));
	if (!file)
	{
		CryLogErrorAlways("%s(0x%p): Invalid handle!", __FUNCTION__, handle);
		return nullptr;
	}

	return file->impl->FGetS(buffer, bufferSize);
}

int CryPak::Getc(FILE* handle)
{
	std::lock_guard lock(m_mutex);

	FileSlot* file = m_files.HandleToSlot(FromDirtyHandle(handle));
	if (!file)
	{
		CryLogErrorAlways("%s(0x%p): Invalid handle!", __FUNCTION__, handle);
		return EOF;
	}

	return file->impl->FGetC();
}

size_t CryPak::FGetSize(FILE* handle)
{
	std::lock_guard lock(m_mutex);

	FileSlot* file = m_files.HandleToSlot(FromDirtyHandle(handle));
	if (!file)
	{
		CryLogErrorAlways("%s(0x%p): Invalid handle!", __FUNCTION__, handle);
		return 0;
	}

	return static_cast<size_t>(file->impl->GetSize());
}

int CryPak::Ungetc(int ch, FILE* handle)
{
	std::lock_guard lock(m_mutex);

	FileSlot* file = m_files.HandleToSlot(FromDirtyHandle(handle));
	if (!file)
	{
		CryLogErrorAlways("%s(0x%p): Invalid handle!", __FUNCTION__, handle);
		return EOF;
	}

	return file->impl->FUnGetC(ch);
}

bool CryPak::IsInPak(FILE* handle)
{
	std::lock_guard lock(m_mutex);

	FileSlot* file = m_files.HandleToSlot(FromDirtyHandle(handle));
	if (!file)
	{
		CryLogErrorAlways("%s(0x%p): Invalid handle!", __FUNCTION__, handle);
		return false;
	}

	return file->pakHandle.IsValid();
}

bool CryPak::RemoveFile(const char* name)
{
	std::lock_guard lock(m_mutex);

	const std::string adjustedName = this->AdjustFileNameImpl(StringTools::SafeView(name), 0);

	CryLog("%s(\"%s\"): Removing \"%s\"", __FUNCTION__, name, adjustedName.c_str());

	std::error_code ec;
	return std::filesystem::remove(adjustedName, ec);
}

bool CryPak::RemoveDir(const char* name, bool recurse)
{
	std::lock_guard lock(m_mutex);

	const std::string adjustedName = this->AdjustFileNameImpl(StringTools::SafeView(name), 0);

	CryLog("%s(\"%s\", recurse=%d): Removing \"%s\"", __FUNCTION__, name, static_cast<int>(recurse),
		adjustedName.c_str());

	if (recurse)
	{
		std::error_code ec;
		return std::filesystem::remove_all(adjustedName, ec) > 0 && !ec;
	}
	else
	{
		std::error_code ec;
		return std::filesystem::remove(adjustedName, ec);
	}
}

bool CryPak::IsAbsPath(const char* path)
{
	// no need to lock m_mutex

	return PathTools::IsAbsolutePath(StringTools::SafeView(path));
}

size_t CryPak::FSeek(FILE* handle, long seek, int mode)
{
	std::lock_guard lock(m_mutex);

	FileSlot* file = m_files.HandleToSlot(FromDirtyHandle(handle));
	if (!file)
	{
		CryLogErrorAlways("%s(0x%p): Invalid handle!", __FUNCTION__, handle);
		return static_cast<size_t>(-1);
	}

	return static_cast<size_t>(file->impl->FSeek(seek, mode));
}

long CryPak::FTell(FILE* handle)
{
	std::lock_guard lock(m_mutex);

	FileSlot* file = m_files.HandleToSlot(FromDirtyHandle(handle));
	if (!file)
	{
		CryLogErrorAlways("%s(0x%p): Invalid handle!", __FUNCTION__, handle);
		return -1;
	}

	const std::int64_t pos = file->impl->FTell();
	if (pos > 0x7fffffff)
	{
		CryLogErrorAlways("%s(0x%p): Cannot return position beyond 2GB!", __FUNCTION__, handle);
		return -1;
	}

	return static_cast<long>(pos);
}

int CryPak::FClose(FILE* handle)
{
	std::lock_guard lock(m_mutex);

	FileSlot* file = m_files.HandleToSlot(FromDirtyHandle(handle));
	if (!file)
	{
		CryLogErrorAlways("%s(0x%p): Invalid handle!", __FUNCTION__, handle);
		return EOF;
	}

	this->CloseSlot(file);

	CryLogComment("%s(0x%p)", __FUNCTION__, handle);

	return 0;
}

int CryPak::FEof(FILE* handle)
{
	std::lock_guard lock(m_mutex);

	FileSlot* file = m_files.HandleToSlot(FromDirtyHandle(handle));
	if (!file)
	{
		CryLogErrorAlways("%s(0x%p): Invalid handle!", __FUNCTION__, handle);
		return 0;
	}

	return file->impl->FEof();
}

int CryPak::FError(FILE* handle)
{
	std::lock_guard lock(m_mutex);

	FileSlot* file = m_files.HandleToSlot(FromDirtyHandle(handle));
	if (!file)
	{
		CryLogErrorAlways("%s(0x%p): Invalid handle!", __FUNCTION__, handle);
		return 0;
	}

	return file->impl->FError();
}

int CryPak::FGetErrno()
{
	return -1;
}

int CryPak::FFlush(FILE* handle)
{
	std::lock_guard lock(m_mutex);

	FileSlot* file = m_files.HandleToSlot(FromDirtyHandle(handle));
	if (!file)
	{
		CryLogErrorAlways("%s(0x%p): Invalid handle!", __FUNCTION__, handle);
		return 0;
	}

	return file->impl->FFlush();
}

void* CryPak::PoolMalloc(size_t size)
{
	return std::malloc(size);
}

void CryPak::PoolFree(void* pool)
{
	std::free(pool);
}

intptr_t CryPak::FindFirst(const char* wildcard, struct _finddata_t* fd, unsigned int flags)
{
	std::lock_guard lock(m_mutex);

	SlotGuard<FindSlot> find = this->GetFreeFindSlot();
	if (!find)
	{
		CryLogErrorAlways("%s(\"%s\", 0x%x): No free find slot!", __FUNCTION__, wildcard, flags);
		return -1;
	}

	find->wildcard = this->AdjustFileNameImpl(StringTools::SafeView(wildcard), flags);

	if (!this->OpenFindImpl(find))
	{
		return -1;
	}

	CryLogComment("%s(\"%s\", 0x%x): Found %zu entries", __FUNCTION__, wildcard, flags, find->entries.size());

	if (find->entries.empty())
	{
		return -1;
	}

	this->FillFindData(fd, find->entries[0]);
	find->pos++;

	// release ownership
	return static_cast<intptr_t>(m_finds.SlotToHandle(find.release()).GetValue());
}

int CryPak::FindNext(intptr_t handle, struct _finddata_t* fd)
{
	std::lock_guard lock(m_mutex);

	FindSlot* find = m_finds.HandleToSlot(SlotVectorHandle(static_cast<std::uint32_t>(handle)));
	if (!find)
	{
		CryLogErrorAlways("%s(0x%p): Invalid handle!", __FUNCTION__, reinterpret_cast<void*>(handle));
		return -1;
	}

	if (find->pos >= find->entries.size())
	{
		return -1;
	}

	this->FillFindData(fd, find->entries[find->pos]);
	find->pos++;

	return 0;
}

int CryPak::FindClose(intptr_t handle)
{
	std::lock_guard lock(m_mutex);

	FindSlot* find = m_finds.HandleToSlot(SlotVectorHandle(static_cast<std::uint32_t>(handle)));
	if (!find)
	{
		CryLogErrorAlways("%s(0x%p): Invalid handle!", __FUNCTION__, reinterpret_cast<void*>(handle));
		return -1;
	}

	this->CloseSlot(find);

	return 0;
}

ICryPak::FileTime CryPak::GetModificationTime(FILE* handle)
{
	std::lock_guard lock(m_mutex);

	FileSlot* file = m_files.HandleToSlot(FromDirtyHandle(handle));
	if (!file)
	{
		CryLogErrorAlways("%s(0x%p): Invalid handle!", __FUNCTION__, handle);
		return 0;
	}

	return file->impl->GetModificationTime();
}

bool CryPak::IsFileExist(const char* name)
{
	std::lock_guard lock(m_mutex);

	const std::string adjustedName = this->AdjustFileNameImpl(StringTools::SafeView(name), 0);

#ifdef CRYMP_PAK_HOT_RELOADING_PATH
	const std::string inDevPak = TryDevPakFolder(adjustedName);
	if (!inDevPak.empty())
	{
		CryLogComment("%s(\"%s\"): Found outside \"%s\"", __FUNCTION__, name, inDevPak.c_str());
		return true;
	}
#endif

	FileTreeNode* fileNode = m_tree.FindNode(adjustedName);
	if (fileNode)
	{
		CryLogComment("%s(\"%s\"): Found in a pak", __FUNCTION__, name);
		return true;
	}

	std::error_code ec;
	if (std::filesystem::exists(adjustedName, ec))
	{
		CryLogComment("%s(\"%s\"): Found outside \"%s\"", __FUNCTION__, name, adjustedName.c_str());
		return true;
	}

	CryLogComment("%s(\"%s\"): Not found!", __FUNCTION__, name);

	return false;
}

bool CryPak::MakeDir(const char* path)
{
	std::lock_guard lock(m_mutex);

	const std::string adjustedPath = this->AdjustFileNameImpl(StringTools::SafeView(path), 0);

	std::error_code ec;
	const bool created = std::filesystem::create_directories(adjustedPath, ec);

	if (ec)
	{
		CryLogErrorAlways("%s(\"%s\"): Error %d (%s)", __FUNCTION__, path, ec.value(), ec.message().c_str());
		return false;
	}
	else if (!created)
	{
		CryLog("%s(\"%s\"): Already exists", __FUNCTION__, path);
		return true;
	}
	else
	{
		CryLog("%s(\"%s\"): Success", __FUNCTION__, path);
		return true;
	}
}

ICryArchive* CryPak::OpenArchive(const char* path, unsigned int flags)
{
	CryLogErrorAlways("%s(\"%s\", 0x%x): Not implemented!", __FUNCTION__, path, flags);
	return nullptr;
}

const char* CryPak::GetFileArchivePath(FILE* handle)
{
	std::lock_guard lock(m_mutex);

	FileSlot* file = m_files.HandleToSlot(FromDirtyHandle(handle));
	if (!file)
	{
		CryLogErrorAlways("%s(0x%p): Invalid handle!", __FUNCTION__, handle);
		return nullptr;
	}

	PakSlot* pak = m_paks.HandleToSlot(file->pakHandle);
	if (!pak)
	{
		return nullptr;
	}

	return pak->path.c_str();
}

int CryPak::RawCompress(const void* uncompressed, unsigned long* dstSize, void* compressed, unsigned long srcSize, int level)
{
	CryLogErrorAlways("%s: Not implemented!", __FUNCTION__);
	return -1;
}

int CryPak::RawUncompress(void* uncompressed, unsigned long* dstSize, const void* compressed, unsigned long srcSize)
{
	CryLogErrorAlways("%s: Not implemented!", __FUNCTION__);
	return -1;
}

void CryPak::RecordFileOpen(const ERecordFileOpenList list)
{
	CryLogErrorAlways("%s(%d): Not implemented!", __FUNCTION__, static_cast<int>(list));
}

void CryPak::RecordFile(FILE* handle, const char* name)
{
	CryLogErrorAlways("%s(0x%p, \"%s\"): Not implemented!", __FUNCTION__, handle, name);
}

IResourceList* CryPak::GetRecorderdResourceList(const ERecordFileOpenList list)
{
	// no need to lock m_mutex

	switch (list)
	{
		case RFOM_Disabled:      break;
		case RFOM_EngineStartup: return m_resourceList_EngineStartup.get();
		case RFOM_NextLevel:     return m_resourceList_NextLevel.get();
		case RFOM_Level:         return m_resourceList_Level.get();
	}

	return nullptr;
}

ICryPak::ERecordFileOpenList CryPak::GetRecordFileOpenList()
{
	return RFOM_Disabled;
}

void CryPak::Notify(ENotifyEvent event)
{
}

std::uint32_t CryPak::ComputeCRC(const char* path)
{
	CryLogErrorAlways("%s(\"%s\"): Not implemented!", __FUNCTION__, path);
	return 0;
}

bool CryPak::ComputeMD5(const char* path, unsigned char* md5)
{
	CryLogErrorAlways("%s(\"%s\"): Not implemented!", __FUNCTION__, path);
	return false;
}

void CryPak::RegisterFileAccessSink(ICryPakFileAcesssSink* pSink)
{
	CryLogErrorAlways("%s(0x%p): Not implemented!", __FUNCTION__, pSink);
}

void CryPak::UnregisterFileAccessSink(ICryPakFileAcesssSink* pSink)
{
	CryLogErrorAlways("%s(0x%p): Not implemented!", __FUNCTION__, pSink);
}

bool CryPak::GetLvlResStatus() const
{
	return false;
}

const char* CryPak::GetModDir() const
{
	return "";
}

////////////////////////////////////////////////////////////////////////////////

bool CryPak::LoadClientPak(const void* data, std::size_t size)
{
#ifdef CRYMP_PAK_HOT_RELOADING_PATH
	return true;
#endif

	std::lock_guard lock(m_mutex);

	SlotGuard<PakSlot> pak = this->GetFreePakSlot();
	if (!pak)
	{
		CryLogErrorAlways("%s: No free pak slot!", __FUNCTION__);
		return false;
	}

	pak->impl = ZipPak::OpenMemory(data, size);
	if (!pak->impl)
	{
		CryLogErrorAlways("%s: Failed!", __FUNCTION__);
		return false;
	}

	pak->path.clear();
	pak->root = "Game";
	pak->fileHandle = {};
	pak->priority = Priority::CLIENT_PAK;
	pak->refCount = 1;

	this->AddPakToTree(pak.get());

	CryLog("%s: Success", __FUNCTION__);

	// release ownership
	pak.release();
	return true;
}

bool CryPak::LoadServerPak(std::string_view path)
{
	std::lock_guard lock(m_mutex);

	SlotGuard<PakSlot> pak = this->GetFreePakSlot();
	if (!pak)
	{
		CryLogErrorAlways("%s(\"%.*s\"): No free pak slot!", __FUNCTION__,
			static_cast<int>(path.size()), path.data());
		return false;
	}

	pak->path = this->AdjustFileNameImpl(path, 0);
	pak->root = "Game";

	if (this->FindLoadedPakByPath(pak->path))
	{
		CryLog("%s(\"%.*s\"): Already loaded", __FUNCTION__, static_cast<int>(path.size()), path.data());
		return true;
	}

	if (!this->OpenPakImpl(pak, Priority::SERVER_PAK))
	{
		CryLogErrorAlways("%s(\"%.*s\"): Failed!", __FUNCTION__, static_cast<int>(path.size()), path.data());
		return false;
	}

	CryLog("%s(\"%.*s\"): Success", __FUNCTION__, static_cast<int>(path.size()), path.data());

	// release ownership
	pak.release();
	return true;
}

bool CryPak::UnloadServerPak(std::string_view path)
{
	std::lock_guard lock(m_mutex);

	const std::string adjustedName = this->AdjustFileNameImpl(path, 0);

	PakSlot* pak = this->FindLoadedPakByPath(adjustedName);
	if (!pak)
	{
		CryLogWarningAlways("%s(\"%.*s\"): Not found!", __FUNCTION__,
			static_cast<int>(path.size()), path.data());
		return false;
	}

	this->CloseSlot(pak);

	CryLog("%s(\"%.*s\")", __FUNCTION__, static_cast<int>(path.size()), path.data());

	return true;
}

void CryPak::AddRedirect(std::string_view path, std::string_view newPath)
{
	std::lock_guard lock(m_mutex);

	auto [node, added] = m_redirects.AddNode(this->AdjustFileNameImplWithoutRedirect(path, 0));
	if (!node)
	{
		CryLogErrorAlways("%s(\"%.*s\", \"%.*s\"): Cannot replace more specific redirect!", __FUNCTION__,
			static_cast<int>(path.size()), path.data(), static_cast<int>(newPath.size()), newPath.data());
		return;
	}

	node->assign(this->AdjustFileNameImplWithoutRedirect(newPath, 0));

	CryLog("%s(\"%.*s\", \"%.*s\"): %s", __FUNCTION__, static_cast<int>(path.size()), path.data(),
		static_cast<int>(newPath.size()), newPath.data(), added ? "Added" : "Changed existing");
}

void CryPak::RemoveRedirect(std::string_view path)
{
	std::lock_guard lock(m_mutex);

	bool removed = m_redirects.Erase(this->AdjustFileNameImplWithoutRedirect(path, 0));

	CryLog("%s(\"%.*s\"): %s", __FUNCTION__, static_cast<int>(path.size()), path.data(),
		removed ? "Removed" : "Not found");
}

void CryPak::LogInfo()
{
	std::lock_guard lock(m_mutex);

	CryLogAlways("------------------------------------ CryPak ------------------------------------");

	{
		std::size_t totalCount = 0;
		CryLogAlways("Open files:");
		for (FileSlot* file = m_files.GetFirstActive(); file; file = m_files.GetNextActive(file))
		{
			CryLogAlways("- $8\"%s\"$o in_pak=$5%d$o mode=$8\"%s\"$o", file->path.c_str(),
				(file->pakHandle) ? 1 : 0, this->FileModeToString(file->mode).c_str());
			totalCount++;
		}
		CryLogAlways("Total open files: $5%zu$o", totalCount);
	}

	{
		std::size_t totalCount = 0;
		CryLogAlways("Open finds:");
		for (FindSlot* find = m_finds.GetFirstActive(); find; find = m_finds.GetNextActive(find))
		{
			CryLogAlways("- $8\"%s\"$o entries=$5%zu$o", find->wildcard.c_str(),
				find->entries.size());
			totalCount++;
		}
		CryLogAlways("Total open finds: $5%zu$o", totalCount);
	}

	{
		std::size_t totalCount = 0;
		std::size_t totalEntryCount = 0;
		std::size_t totalCacheSize = 0;
		CryLogAlways("Loaded paks:");
		for (PakSlot* pak = m_paks.GetFirstActive(); pak; pak = m_paks.GetNextActive(pak))
		{
			const std::size_t entryCount = pak->impl->GetEntryCount();
			const std::size_t cacheSize = pak->impl->GetCachedDataSize();
			CryLogAlways("- $8\"%s\"$o root=$8\"%s\"$o priority=$5%d$o entries=$5%zu$o cache_size=$5%zu$o",
				pak->path.c_str(), pak->root.c_str(), static_cast<int>(pak->priority), entryCount,
				cacheSize);
			totalCount++;
			totalEntryCount += entryCount;
			totalCacheSize += cacheSize;
		}
		CryLogAlways("Total paks: $5%zu$o", totalCount);
		CryLogAlways("Total pak entries: $5%zu$o", totalEntryCount);
		CryLogAlways("Total pak cache size: $5%zu$o", totalCacheSize);
	}

	CryLogAlways("--------------------------------------------------------------------------------");
}

std::string CryPak::AdjustFileNameImplWithoutRedirect(std::string_view path, unsigned int flags)
{
	std::string adjusted;
	adjusted.reserve(260);
	adjusted = path;

	const unsigned int supportedFlags = FLAGS_ADD_TRAILING_SLASH | FLAGS_NO_FULL_PATH
		| FLAGS_NO_MASTER_FOLDER_MAPPING | FLAGS_FOR_WRITING;
	const unsigned int garbageFlags = 0x1 | 0x2 | FLAGS_PATH_REAL;

	if (flags & ~(supportedFlags | garbageFlags))
	{
		CryLogWarningAlways("CryPak::AdjustFileName(\"%s\", 0x%x): Unknown flag(s)", adjusted.c_str(), flags);
	}

	// TODO: prevent access outside Crysis directories (main, root, user)

	if ((flags & FLAGS_FOR_WRITING) && !m_gameFolderWritable)
	{
		if (adjusted.empty() || (adjusted[0] != '%' && !PathTools::IsAbsolutePath(adjusted)))
		{
			adjusted.insert(0, "%USER%/");
		}
	}

	this->ExpandAliases(adjusted);

	PathTools::Normalize(adjusted);

	// check the original path because Normalize removes the "./" prefix
	const bool noFullPath = path.length() >= 2 && path[0] == '.' && (path[1] == '/' || path[1] == '\\');

	if (!noFullPath && !(flags & (FLAGS_NO_FULL_PATH | FLAGS_NO_MASTER_FOLDER_MAPPING))
	 && !PathTools::IsAbsolutePath(adjusted)
	 && !PathTools::StartsWith(adjusted, "Game/")
	 && !PathTools::StartsWith(adjusted, "Editor/")
	 && !PathTools::StartsWith(adjusted, "Bin32/")
	 && !PathTools::StartsWith(adjusted, "Bin64/")
	 && !PathTools::StartsWith(adjusted, "Mods/"))
	{
		adjusted.insert(0, "Game/");
	}

	if (flags & FLAGS_ADD_TRAILING_SLASH)
	{
		PathTools::AddTrailingSlash(adjusted);
	}

	return adjusted;
}

std::string CryPak::AdjustFileNameImpl(std::string_view path, unsigned int flags)
{
	std::string adjusted = this->AdjustFileNameImplWithoutRedirect(path, flags);

	auto [redirectNode, remainingPath] = m_redirects.FindNodeInPath(adjusted);
	if (redirectNode)
	{
		// remaining path is also a string view into adjusted
		std::string_view prefix(adjusted.data(), adjusted.length() - remainingPath.length());

		if (!prefix.empty() && prefix.back() == '/')
		{
			// keep the slash
			prefix.remove_suffix(1);
		}

		adjusted.replace(0, prefix.length(), *redirectNode);
	}

	return adjusted;
}

CryPak::SlotGuard<CryPak::FileSlot> CryPak::GetFreeFileSlot()
{
	return SlotGuard<FileSlot>(m_files.GetFreeSlot(), SlotDeleter<FileSlot>(this));
}

CryPak::SlotGuard<CryPak::FindSlot> CryPak::GetFreeFindSlot()
{
	return SlotGuard<FindSlot>(m_finds.GetFreeSlot(), SlotDeleter<FindSlot>(this));
}

CryPak::SlotGuard<CryPak::PakSlot> CryPak::GetFreePakSlot()
{
	return SlotGuard<PakSlot>(m_paks.GetFreeSlot(), SlotDeleter<PakSlot>(this));
}

bool CryPak::OpenFileImpl(SlotGuard<FileSlot>& file)
{
#ifdef CRYMP_PAK_HOT_RELOADING_PATH
	const std::string inDevPak = TryDevPakFolder(file->path);
	if (!inDevPak.empty())
	{
		file->path = inDevPak;
		return this->OpenFileOutsideImpl(file);
	}
#endif

	if (this->OpenFileInPakImpl(file))
	{
		return true;
	}

	if (this->OpenFileOutsideImpl(file))
	{
		return true;
	}

	return false;
}

bool CryPak::OpenFileInPakImpl(SlotGuard<FileSlot>& file)
{
	FileTreeNode* fileNode = m_tree.FindNode(file->path);
	if (!fileNode)
	{
		return false;
	}

	if (this->IsFileModeWriting(file->mode))
	{
		CryLogErrorAlways("%s(\"%s\"): Writing to paks is not supported!", __FUNCTION__, file->path.c_str());
		return false;
	}

	PakSlot* pak = m_paks.HandleToSlot(fileNode->current.pakHandle);
	if (!pak)
	{
		CryLogErrorAlways("%s(\"%s\"): Invalid pak handle!", __FUNCTION__, file->path.c_str());
		return false;
	}

	const bool isBinary = (file->mode & FILE_MODE_BINARY);

	file->indexInPak = fileNode->current.fileIndex;
	file->impl = pak->impl->OpenFile(file->indexInPak, isBinary);
	if (!file->impl)
	{
		CryLogErrorAlways("%s(\"%s\"): Error in pak!", __FUNCTION__, file->path.c_str());
		return false;
	}

	// set pak handle right after opening the file, so CloseSlot correctly informs the pak when closing the file
	file->pakHandle = m_paks.SlotToHandle(pak);
	this->IncrementPakRefCount(pak);

	return true;
}

bool CryPak::OpenFileOutsideImpl(SlotGuard<FileSlot>& file)
{
	std::filesystem::path fsPath(file->path);

	if (this->IsFileModeWriting(file->mode))
	{
		std::error_code ec;
		std::filesystem::create_directories(fsPath.parent_path(), ec);

		if (ec)
		{
			CryLogErrorAlways("%s(\"%s\"): Creating directories failed: Error %d (%s)", __FUNCTION__,
				file->path.c_str(), ec.value(), ec.message().c_str());
			return false;
		}
	}

	FileOutsidePak::Handle handle(std::fopen(file->path.c_str(), this->FileModeToString(file->mode).c_str()));
	if (!handle)
	{
		const int errnum = errno;
		if (errnum != ENOENT)
		{
			CryLogErrorAlways("%s(\"%s\"): File open failed: Error %d (%s)", __FUNCTION__,
				file->path.c_str(), errnum, std::strerror(errnum));
		}
		return false;
	}

	file->impl = std::make_unique<FileOutsidePak>(std::move(handle), std::move(fsPath));
	file->indexInPak = 0;
	file->pakHandle = {};

	return true;
}

bool CryPak::OpenFindImpl(SlotGuard<FindSlot>& find)
{
	find->entries = this->SearchWildcardPath(find->wildcard);
	find->pos = 0;

	return true;
}

bool CryPak::OpenPakImpl(SlotGuard<PakSlot>& pak, Priority priority)
{
	SlotGuard<FileSlot> file = this->GetFreeFileSlot();
	if (!file)
	{
		CryLogErrorAlways("%s(\"%s\"): No free file slot!", __FUNCTION__, pak->path.c_str());
		return false;
	}

	file->path = pak->path;
	file->mode = FILE_MODE_READ | FILE_MODE_BINARY;

	if (!this->OpenFileImpl(file))
	{
		CryLog("%s(\"%s\"): File open failed!", __FUNCTION__, pak->path.c_str());
		return false;
	}

	std::FILE* fileImplHandle = file->impl->GetHandle();
	if (!fileImplHandle)
	{
		// nested pak
		std::size_t pakSize = 0;
		const void* pakData = file->impl->GetCachedFileData(pakSize);

		pak->impl = ZipPak::OpenMemory(pakData, pakSize);
	}
	else
	{
		pak->impl = ZipPak::OpenFileHandle(fileImplHandle);
	}

	if (!pak->impl)
	{
		CryLogErrorAlways("%s(\"%s\"): Error in pak!", __FUNCTION__, pak->path.c_str());
		return false;
	}

	// transfer file ownership to the pak
	pak->fileHandle = m_files.SlotToHandle(file.release());

	pak->priority = priority;
	pak->refCount = 1;

	this->AddPakToTree(pak.get());

	return true;
}

CryPak::PakSlot* CryPak::FindLoadedPakByPath(std::string_view pakPath)
{
	if (pakPath.empty())
	{
		return nullptr;
	}

	return m_paks.Find([pakPath](const PakSlot& pak) { return StringTools::IsEqualNoCase(pak.path, pakPath); });
}

void CryPak::CloseSlot(FileSlot* file)
{
	PakSlot* pak = m_paks.HandleToSlot(file->pakHandle);

	file->clear();
	file->pakHandle = {};

	if (pak)
	{
		pak->impl->OnFileClosed(file->indexInPak);

		this->DecrementPakRefCount(pak);
	}
}

void CryPak::CloseSlot(FindSlot* find)
{
	find->clear();
}

void CryPak::CloseSlot(PakSlot* pak)
{
	if (pak->empty())
	{
		// not loaded
		return;
	}

	this->RemovePakFromTree(pak);
	this->DecrementPakRefCount(pak);
}

void CryPak::IncrementPakRefCount(PakSlot* pak)
{
	pak->refCount++;
}

void CryPak::DecrementPakRefCount(PakSlot* pak)
{
	pak->refCount--;

	if (pak->refCount > 0)
	{
		// keep open
		return;
	}

	FileSlot* file = m_files.HandleToSlot(pak->fileHandle);

	pak->clear();
	pak->fileHandle = {};

	if (file)
	{
		this->CloseSlot(file);
	}
}

void CryPak::ExpandAliases(std::string& path)
{
	std::string::size_type pos = 0;
	while ((pos = path.find('%', pos)) != std::string::npos)
	{
		auto endPos = path.find('%', pos + 1);
		if (endPos == std::string::npos)
		{
			break;
		}

		endPos++;

		const auto aliasIt = m_aliases.find(std::string_view(path.data() + pos, endPos - pos));
		if (aliasIt == m_aliases.end())
		{
			// ignore unknown aliases
			pos = endPos;
			continue;
		}

		path.replace(path.begin() + pos, path.begin() + endPos, aliasIt->second);

		pos += aliasIt->second.length();
	}
}

std::vector<CryPak::FindSlot::Entry> CryPak::SearchWildcardPath(std::string_view wildcardPath)
{
	const auto [dirPath, wildcardName] = PathTools::SplitPathIntoDirAndFile(wildcardPath);
	const CryPakWildcardMatcher matcher(wildcardName);

	std::map<std::string, FindSlot::Entry, StringTools::ComparatorNoCase> entryMap;

	const auto searchOutside = [&matcher, &entryMap](std::string_view path)
	{
		std::error_code ec;
		for (const auto& fsEntry : std::filesystem::directory_iterator(path, ec))
		{
			std::string name = fsEntry.path().filename().generic_string();

			if (!matcher(name))
			{
				continue;
			}

			std::uint64_t size = 0;
			bool isDirectory = true;

			if (!fsEntry.is_directory(ec))
			{
				isDirectory = false;
				size = fsEntry.file_size(ec);
			}

			entryMap.try_emplace(name, name, size, isDirectory, false);
		}
	};

#ifdef CRYMP_PAK_HOT_RELOADING_PATH
	const std::string inDevPak = TryDevPakFolder(dirPath);
	if (!inDevPak.empty())
	{
		searchOutside(inDevPak);
	}
#endif

	Tree::DirectoryNode* dirNode = m_tree.FindDirectoryNode(dirPath);
	if (dirNode)
	{
		for (auto& [name, node] : *dirNode)
		{
			if (!matcher(name))
			{
				continue;
			}

			std::uint64_t size = 0;
			bool isDirectory = true;

			if (FileTreeNode* fileNode = std::get_if<FileTreeNode>(&node); fileNode)
			{
				isDirectory = false;
				size = fileNode->current.fileSize;
			}

			entryMap.try_emplace(name, name, size, isDirectory, true);
		}
	}

	searchOutside(dirPath);

	std::vector<FindSlot::Entry> entries;
	entries.reserve(entryMap.size());
	for (auto& [name, entry] : entryMap)
	{
		entries.emplace_back(std::move(entry));
	}

	return entries;
}

void CryPak::FillFindData(struct _finddata_t* fd, const FindSlot::Entry& entry)
{
	CryLogComment("%s: \"%s\" directory=%d in_pak=%d", __FUNCTION__, entry.name.c_str(),
		static_cast<int>(entry.isDirectory), static_cast<int>(entry.isInPak));

	if (!fd)
	{
		return;
	}

	fd->attrib = _A_NORMAL;
	fd->size = entry.size;
	// nothing uses these anyway
	fd->time_create = 0;
	fd->time_access = 0;
	fd->time_write = 0;

	if (entry.isDirectory)
	{
		fd->attrib |= _A_SUBDIR;
	}

	if (entry.isInPak)
	{
		fd->attrib |= _A_IN_CRYPAK | _A_RDONLY;
	}

	const std::size_t length = std::min(entry.name.length(), std::size(fd->name) - 1);

	std::memcpy(fd->name, entry.name.c_str(), length);
	fd->name[length] = '\0';
}

CryPak::FileModeFlags CryPak::FileModeFromString(std::string_view mode)
{
	FileModeFlags flags = 0;

	for (char ch : mode)
	{
		switch (ch)
		{
			case 'r': flags |= FILE_MODE_READ; break;
			case 'w': flags |= FILE_MODE_WRITE; break;
			case 'a': flags |= FILE_MODE_APPEND; break;
			case '+': flags |= FILE_MODE_EXTENDED; break;
			case 'b': flags |= FILE_MODE_BINARY; break;
			case 't': flags &= ~FILE_MODE_BINARY; break;
			case 'x': break;
		}
	}

	return flags;
}

std::string CryPak::FileModeToString(FileModeFlags flags)
{
	std::string mode;

	if (flags & FILE_MODE_READ)
	{
		mode += 'r';
	}

	if (flags & FILE_MODE_WRITE)
	{
		mode += 'w';
	}

	if (flags & FILE_MODE_APPEND)
	{
		mode += 'a';
	}

	if (flags & FILE_MODE_EXTENDED)
	{
		mode += '+';
	}

	if (flags & FILE_MODE_BINARY)
	{
		mode += 'b';
	}

	return mode;
}

bool CryPak::IsFileModeWriting(FileModeFlags flags)
{
	return (flags & (FILE_MODE_WRITE | FILE_MODE_APPEND | FILE_MODE_EXTENDED));
}

void CryPak::AddPakToTree(PakSlot* pak)
{
	const SlotVectorHandle pakHandle = m_paks.SlotToHandle(pak);

	auto [baseNode, baseNodeAdded] = m_tree.AddDirectory(pak->root);
	if (!baseNode)
	{
		// root path points to a file
		return;
	}

	const std::uint32_t entryCount = pak->impl->GetEntryCount();

	std::string filePath;
	filePath.reserve(260);
	for (std::uint32_t i = 0; i < entryCount; i++)
	{
		if (pak->impl->IsDirectoryEntry(i))
		{
			continue;
		}

		std::uint64_t fileSize = 0;
		if (!pak->impl->GetEntryPathAndSize(i, filePath, fileSize))
		{
			continue;
		}

		PathTools::Normalize(filePath);

		auto [fileNode, fileNodeAdded] = m_tree.AddNode(filePath, baseNode);
		if (!fileNode)
		{
			continue;
		}

		const FileTreeNode::FileInPak file = {
			.fileIndex = i,
			.fileSize = fileSize,
			.pakHandle = pakHandle,
			.priority = pak->priority,
		};

		// we don't need to check if the node was added because newly added nodes have invalid pak handle
		this->AddFileAlternative(*fileNode, file);
	}
}

void CryPak::RemovePakFromTree(PakSlot* pak)
{
	const SlotVectorHandle pakHandle = m_paks.SlotToHandle(pak);

	m_tree.EraseIf([pakHandle](FileTreeNode& fileNode) -> bool {
		fileNode.alternatives.remove_if([pakHandle](const auto& x) { return x.pakHandle == pakHandle; });

		if (fileNode.current.pakHandle != pakHandle)
		{
			// keep the node
			return false;
		}

		if (!fileNode.alternatives.empty())
		{
			// use the previous alternative of the file in a different pak with the same or lower priority
			fileNode.current = fileNode.alternatives.front();
			fileNode.alternatives.pop_front();

			// keep the node
			return false;
		}

		// the node is now empty, so erase it
		return true;
	});
}

void CryPak::AddFileAlternative(FileTreeNode& fileNode, const FileTreeNode::FileInPak& newFile)
{
	if (!fileNode.current.pakHandle)
	{
		// newly added empty file node
		fileNode.current = newFile;
		return;
	}

	if (fileNode.current.priority <= newFile.priority)
	{
		fileNode.alternatives.push_front(fileNode.current);
		fileNode.current = newFile;
		return;
	}

	fileNode.alternatives.push_front(newFile);

	// sort from highest priority to lowest
	fileNode.alternatives.sort([](const auto& a, const auto& b) { return a.priority > b.priority; });
}
