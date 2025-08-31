#pragma once

#include <cstddef>
#include <cstdint>
#include <forward_list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "CryCommon/CrySystem/ICryPak.h"
#include "Library/PathTree.h"
#include "Library/SlotVector.h"
#include "Library/StringTools.h"

#include "Pak/IFileInPak.h"
#include "Pak/IPak.h"

class CryPak final : public ICryPak
{
	enum class Priority
	{
		NORMAL,
		CLIENT_PAK,
		SERVER_PAK,  // highest priority
	};

	enum
	{
		FILE_MODE_READ     = (1 << 0),
		FILE_MODE_WRITE    = (1 << 1),
		FILE_MODE_APPEND   = (1 << 2),
		FILE_MODE_EXTENDED = (1 << 3),
		FILE_MODE_BINARY   = (1 << 4),
	};

	using FileModeFlags = std::uint16_t;

	struct FileSlot
	{
		std::string path;
		std::unique_ptr<IFileInPak> impl;
		std::uint32_t indexInPak = 0;
		SlotVectorHandle pakHandle;
		SlotVectorSerial serial;
		FileModeFlags mode = 0;

		bool empty() const
		{
			return impl == nullptr;
		}

		void clear()
		{
			impl = nullptr;
		}
	};

	struct FindSlot
	{
		struct Entry
		{
			std::string name;
			std::uint64_t size = 0;
			bool isDirectory = false;
			bool isInPak = false;
		};

		std::string wildcard;
		std::vector<Entry> entries;
		std::vector<Entry>::size_type pos = 0;
		SlotVectorSerial serial;

		bool empty() const
		{
			return entries.empty();
		}

		void clear()
		{
			entries.clear();
		}
	};

	struct PakSlot
	{
		std::string path;
		std::string root;
		std::unique_ptr<IPak> impl;
		SlotVectorHandle fileHandle;
		SlotVectorSerial serial;
		Priority priority = {};
		std::int32_t refCount = 0;

		bool empty() const
		{
			return impl == nullptr;
		}

		void clear()
		{
			impl = nullptr;
		}
	};

	template<class T>
	struct SlotDeleter
	{
		CryPak* pCryPak = nullptr;

		explicit SlotDeleter(CryPak* pCryPak) : pCryPak(pCryPak) {}

		void operator()(T* slot) const
		{
			this->pCryPak->CloseSlot(slot);
		}
	};

	template<class T>
	using SlotGuard = std::unique_ptr<T, SlotDeleter<T>>;

	struct FileTreeNode
	{
		struct FileInPak
		{
			std::uint32_t fileIndex = 0;
			std::uint64_t fileSize = 0;
			SlotVectorHandle pakHandle;
			Priority priority = {};
		};

		FileInPak current;
		std::forward_list<FileInPak> alternatives;
	};

	using Tree = PathTree<FileTreeNode, StringTools::ComparatorNoCase>;
	using Redirects = PathTree<std::string, StringTools::ComparatorNoCase>;

	std::recursive_mutex m_mutex;
	std::map<std::string, std::string, StringTools::ComparatorNoCase> m_aliases;
	SlotVector<FileSlot> m_files;
	SlotVector<FindSlot> m_finds;
	SlotVector<PakSlot> m_paks;
	Tree m_tree;
	Redirects m_redirects;

	bool m_gameFolderWritable = false;

	_smart_ptr<IResourceList> m_resourceList_EngineStartup;
	_smart_ptr<IResourceList> m_resourceList_NextLevel;
	_smart_ptr<IResourceList> m_resourceList_Level;

public:
	CryPak();
	~CryPak();

	// to be removed once we have our own CrySystem
	static CryPak& GetInstance()
	{
		static CryPak* instance = new CryPak;
		return *instance;
	}

	////////////////////////////////////////////////////////////////////////////////
	// ICryPak
	////////////////////////////////////////////////////////////////////////////////

	const char* AdjustFileName(const char* src, char* dst, unsigned int flags, bool* foundInPak = nullptr) override;

	bool Init(const char* basePath) override;
	void Release() override;

	bool OpenPack(const char* name, unsigned int flags = FLAGS_PATH_REAL) override;
	bool OpenPack(const char* root, const char* name, unsigned int flags = FLAGS_PATH_REAL) override;
	bool ClosePack(const char* name, unsigned int flags = FLAGS_PATH_REAL) override;
	bool OpenPacks(const char* wildcard, unsigned int flags = FLAGS_PATH_REAL) override;
	bool OpenPacks(const char* root, const char* wildcard, unsigned int flags = FLAGS_PATH_REAL) override;
	bool ClosePacks(const char* wildcard, unsigned int flags = FLAGS_PATH_REAL) override;

	void AddMod(const char* mod) override;
	void RemoveMod(const char* mod) override;

	void ParseAliases(const char* commandLine) override;
	void SetAlias(const char* name, const char* alias, bool add) override;
	const char* GetAlias(const char* name, bool returnSame = true) override;

	PakInfo* GetPakInfo() override;
	void FreePakInfo(PakInfo* pakInfo) override;

	FILE* FOpen(const char* name, const char* mode, unsigned int flags = 0) override;
	FILE* FOpen(const char* name, const char* mode, char* fileGamePath, int length) override;

	size_t FReadRaw(void* data, size_t elementSize, size_t elementCount, FILE* handle) override;
	size_t FReadRawAll(void* data, size_t fileSize, FILE* handle) override;

	void* FGetCachedFileData(FILE* handle, size_t& fileSize) override;

	size_t FWrite(const void* data, size_t elementSize, size_t elementCount, FILE* handle) override;

	int FPrintf(FILE* handle, const char* format, ...) override;
	char* FGets(char* buffer, int bufferSize, FILE* handle) override;
	int Getc(FILE* handle) override;
	size_t FGetSize(FILE* handle) override;
	int Ungetc(int ch, FILE* handle) override;
	bool IsInPak(FILE* handle) override;
	bool RemoveFile(const char* name) override;
	bool RemoveDir(const char* name, bool recurse) override;
	bool IsAbsPath(const char* path) override;
	size_t FSeek(FILE* handle, long seek, int mode) override;
	long FTell(FILE* handle) override;
	int FClose(FILE* handle) override;
	int FEof(FILE* handle) override;
	int FError(FILE* handle) override;
	int FGetErrno() override;
	int FFlush(FILE* handle) override;

	void* PoolMalloc(size_t size) override;
	void PoolFree(void* pool) override;

	intptr_t FindFirst(const char* wildcard, struct _finddata_t* fd, unsigned int flags = 0) override;
	int FindNext(intptr_t handle, struct _finddata_t* fd) override;
	int FindClose(intptr_t handle) override;

	FileTime GetModificationTime(FILE* handle) override;

	bool IsFileExist(const char* name) override;

	bool MakeDir(const char* path) override;

	ICryArchive* OpenArchive(const char* path, unsigned int flags = FLAGS_PATH_REAL) override;

	const char* GetFileArchivePath(FILE* handle) override;

	int RawCompress(const void* uncompressed, unsigned long* dstSize, void* compressed, unsigned long srcSize, int level = -1) override;
	int RawUncompress(void* uncompressed, unsigned long* dstSize, const void* compressed, unsigned long srcSize) override;

	void RecordFileOpen(const ERecordFileOpenList list) override;
	void RecordFile(FILE* handle, const char* name) override;
	IResourceList* GetRecorderdResourceList(const ERecordFileOpenList list) override;
	ERecordFileOpenList GetRecordFileOpenList() override;

	void Notify(ENotifyEvent event) override;

	std::uint32_t ComputeCRC(const char* path) override;
	bool ComputeMD5(const char* path, unsigned char* md5) override;

	void RegisterFileAccessSink(ICryPakFileAcesssSink* pSink) override;
	void UnregisterFileAccessSink(ICryPakFileAcesssSink* pSink) override;

	bool GetLvlResStatus() const override;

	const char* GetModDir() const override;

	////////////////////////////////////////////////////////////////////////////////

	void SetGameFolderWritable(bool writable) { m_gameFolderWritable = writable; }
	bool IsGameFolderWritable() const { return m_gameFolderWritable; }

	bool LoadClientPak(const void* data, std::size_t size);
	bool LoadServerPak(std::string_view path);
	bool UnloadServerPak(std::string_view path);

	void AddRedirect(std::string_view path, std::string_view newPath);
	void RemoveRedirect(std::string_view path);

	void LogInfo();

private:
	std::string AdjustFileNameImplWithoutRedirect(std::string_view path, unsigned int flags);
	std::string AdjustFileNameImpl(std::string_view path, unsigned int flags);

	SlotGuard<FileSlot> GetFreeFileSlot();
	SlotGuard<FindSlot> GetFreeFindSlot();
	SlotGuard<PakSlot> GetFreePakSlot();

	bool OpenFileImpl(SlotGuard<FileSlot>& file);
	bool OpenFileInPakImpl(SlotGuard<FileSlot>& file);
	bool OpenFileOutsideImpl(SlotGuard<FileSlot>& file);

	bool OpenFindImpl(SlotGuard<FindSlot>& find);

	bool OpenPakImpl(SlotGuard<PakSlot>& pak, Priority priority);

	PakSlot* FindLoadedPakByPath(std::string_view pakPath);

	void CloseSlot(FileSlot* file);
	void CloseSlot(FindSlot* find);
	void CloseSlot(PakSlot* pak);

	void IncrementPakRefCount(PakSlot* pak);
	void DecrementPakRefCount(PakSlot* pak);

	void ExpandAliases(std::string& path);

	std::vector<FindSlot::Entry> SearchWildcardPath(std::string_view wildcardPath);

	void FillFindData(struct _finddata_t* fd, const FindSlot::Entry& entry);

	FileModeFlags FileModeFromString(std::string_view mode);
	std::string FileModeToString(FileModeFlags flags);
	bool IsFileModeWriting(FileModeFlags flags);

	void AddPakToTree(PakSlot* pak);
	void RemovePakFromTree(PakSlot* pak);

	void AddFileAlternative(FileTreeNode& fileNode, const FileTreeNode::FileInPak& newFile);
};
