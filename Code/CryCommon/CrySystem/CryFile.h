#pragma once

#include <cstddef>
#include <memory>

#include "gEnv.h"
#include "ICryPak.h"

//////////////////////////////////////////////////////////////////////////
// Defines for CryEngine filetypes extensions.
//////////////////////////////////////////////////////////////////////////
#define CRY_GEOMETRY_FILE_EXT                    "cgf"
#define CRY_CHARACTER_FILE_EXT                   "chr"
#define CRY_CHARACTER_ANIMATION_FILE_EXT         "caf"
#define CRY_CHARACTER_DEFINITION_FILE_EXT        "cdf"
#define CRY_CHARACTER_LIST_FILE_EXT              "cid"
#define CRY_ANIM_GEOMETRY_FILE_EXT               "cga"
#define CRY_ANIM_GEOMETRY_ANIMATION_FILE_EXT     "anm"
#define CRY_COMPILED_FILE_EXT                    "(c)"
//////////////////////////////////////////////////////////////////////////

/*
inline const char* CryGetExt( const char *filepath )
{
	const char *str = filepath;
	int len = strlen(filepath);
	for (const char* p = str + len-1; p >= str; --p)
	{
		switch(*p)
		{
		case ':':
		case '/':
		case '\\':
			// we've reached a path separator - it means there's no extension in this name
			return "";
		case '.':
			// there's an extension in this file name
			return p+1;
		}
	}
	return "";
}

//////////////////////////////////////////////////////////////////////////
// Check if specified file name is a character file.
//////////////////////////////////////////////////////////////////////////
inline bool IsCharacterFile( const char *filename )
{
	const char *ext = CryGetExt(filename);
	if (_stricmp(ext,CRY_CHARACTER_FILE_EXT) == 0 ||
			_stricmp(ext,CRY_CHARACTER_DEFINITION_FILE_EXT) == 0 ||
			_stricmp(ext,CRY_ANIM_GEOMETRY_FILE_EXT) == 0)
	{
		return true;
	}
	else
		return false;
}

//////////////////////////////////////////////////////////////////////////
// Check if specified file name is a static geometry file.
//////////////////////////////////////////////////////////////////////////
inline bool IsStatObjFile( const char *filename )
{
	const char *ext = CryGetExt(filename);
	if (_stricmp(ext,CRY_GEOMETRY_FILE_EXT) == 0)
	{
		return true;
	}
	else
		return false;
}
*/

class CryFile
{
	struct Releaser
	{
		void operator()(FILE* file) const
		{
			gEnv->pCryPak->FClose(file);
		}
	};

	std::unique_ptr<FILE, Releaser> m_file;

public:
	CryFile() = default;

	explicit CryFile(const char* filename, const char* mode) : m_file(gEnv->pCryPak->FOpen(filename, mode))
	{
	}

	bool IsOpen() const
	{
		return m_file != nullptr;
	}

	explicit operator bool() const
	{
		return this->IsOpen();
	}

	bool Open(const char* filename, const char* mode)
	{
		m_file.reset(gEnv->pCryPak->FOpen(filename, mode));

		return this->IsOpen();
	}

	void Close()
	{
		m_file.reset();
	}

	std::size_t Write(const void* data, std::size_t size)
	{
		return gEnv->pCryPak->FWrite(data, 1, size, m_file.get());
	}

	std::size_t ReadRaw(void* buffer, std::size_t size)
	{
		return gEnv->pCryPak->FReadRaw(buffer, 1, size, m_file.get());
	}

	template<class... Args>
	int FPrintf(const char* format, Args... args)
	{
		return gEnv->pCryPak->FPrintf(m_file.get(), format, args...);
	}

	void Puts(const char* line)
	{
		this->FPrintf("%s\n", line);
	}

	std::size_t GetSize()
	{
		return gEnv->pCryPak->FGetSize(m_file.get());
	}

	int Seek(long seek, int mode)
	{
		return static_cast<int>(gEnv->pCryPak->FSeek(m_file.get(), seek, mode));
	}

	void SeekToBegin()
	{
		this->Seek(0, SEEK_SET);
	}

	void SeekToEnd()
	{
		this->Seek(0, SEEK_END);
	}

	long GetPosition()
	{
		return gEnv->pCryPak->FTell(m_file.get());
	}

	bool IsInPak()
	{
		return gEnv->pCryPak->IsInPak(m_file.get());
	}

	const char* GetPakPath()
	{
		return gEnv->pCryPak->GetFileArchivePath(m_file.get());
	}
};


//////////////////////////////////////////////////////////////////////////
// CryEngine 2 / Crysis Wars compatibility wrapper.
// Kept alongside CryMP's modern CryFile so imported engine code can compile
// without replacing existing CryMP call sites.
//////////////////////////////////////////////////////////////////////////
class CCryFile
{
public:
    CCryFile() : m_file(nullptr), m_pIPak(gEnv ? gEnv->pCryPak : nullptr) {}

    CCryFile(const char* filename, const char* mode)
        : m_file(nullptr), m_pIPak(gEnv ? gEnv->pCryPak : nullptr)
    {
        Open(filename, mode);
    }

    virtual ~CCryFile() { Close(); }

    virtual bool Open(const char* filename, const char* mode, int nOpenFlagsEx = 0)
    {
        Close();
        if (!m_pIPak && gEnv)
            m_pIPak = gEnv->pCryPak;
        if (!m_pIPak)
            return false;
        m_filename = filename ? filename : "";
        m_file = m_pIPak->FOpen(filename, mode, nOpenFlagsEx);
        return m_file != nullptr;
    }

    virtual void Close()
    {
        if (m_file && m_pIPak)
            m_pIPak->FClose(m_file);
        m_file = nullptr;
        m_filename.clear();
    }

    virtual std::size_t Write(const void* data, std::size_t size)
    {
        return m_pIPak->FWrite(data, 1, size, m_file);
    }

    virtual std::size_t ReadRaw(void* data, std::size_t size)
    {
        return m_pIPak->FReadRaw(data, 1, size, m_file);
    }

    template<class T>
    std::size_t ReadTypeRaw(T* dest, std::size_t count = 1)
    {
        return ReadRaw(dest, sizeof(T) * count);
    }

    template<class T>
    std::size_t ReadType(T* dest, std::size_t count = 1)
    {
        const std::size_t read = ReadRaw(dest, sizeof(T) * count);
        SwapEndian(dest, count);
        return read;
    }

    virtual std::size_t GetLength()
    {
        if (!m_file)
            return 0;
        const long current = m_pIPak->FTell(m_file);
        m_pIPak->FSeek(m_file, 0, SEEK_END);
        const long size = m_pIPak->FTell(m_file);
        m_pIPak->FSeek(m_file, current, SEEK_SET);
        return size > 0 ? static_cast<std::size_t>(size) : 0;
    }

    virtual std::size_t Seek(std::size_t seek, int mode)
    {
        return static_cast<std::size_t>(m_pIPak->FSeek(m_file, static_cast<long>(seek), mode));
    }

    void SeekToBegin() { Seek(0, SEEK_SET); }
    std::size_t SeekToEnd() { return Seek(0, SEEK_END); }
    std::size_t GetPosition() { return static_cast<std::size_t>(m_pIPak->FTell(m_file)); }
    virtual bool IsEof() { return m_pIPak->FEof(m_file) != 0; }
    virtual void Flush() { m_pIPak->FFlush(m_file); }
    FILE* GetHandle() const { return m_file; }
    const char* GetFilename() const { return m_filename.c_str(); }
    const char* GetAdjustedFilename() const
    {
        static thread_local char adjusted[_MAX_PATH];
        return (m_pIPak && !m_filename.empty())
            ? m_pIPak->AdjustFileName(m_filename.c_str(), adjusted, 0)
            : m_filename.c_str();
    }
    bool IsInPak() const { return m_file && m_pIPak && m_pIPak->IsInPak(m_file); }
    const char* GetPakPath() const { return (m_file && m_pIPak) ? m_pIPak->GetFileArchivePath(m_file) : ""; }

private:
    string m_filename;
    FILE* m_file;
    ICryPak* m_pIPak;
};
