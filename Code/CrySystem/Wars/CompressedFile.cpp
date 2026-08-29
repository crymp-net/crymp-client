#include "StdAfx.h"
#include "CryCommon/CrySystem/ILog.h"
//#include <Stream.h>
#include "System.h"
#include "zlib/zlib.h"
#include "CryCommon/CrySystem/ILog.h"
#include "CryCommon/CrySystem/ICryPak.h"
#include <fcntl.h>

#define USE_COMPRESSION

#define DIV8(n)((n) >> 3)
#define MODULO8(n)((n)&0x00000007)
#define BITS2BYTES(n)(MODULO8((n))?(DIV8((n)) + 1):DIV8((n)))
#define BYTES2BITS(n)((n) << 3)


bool CSystem::CompressDataBlock(const void * input, size_t inputSize, void * output, size_t& outputSize, int level )
{
	uLongf destLen = outputSize;
	Bytef * dest = static_cast<Bytef *>(output);
	uLong sourceLen = inputSize;
	const Bytef * source = static_cast<const Bytef *>(input);
	bool ok = Z_OK == compress2( dest, &destLen, source, sourceLen, level );
	outputSize = destLen;
	return ok;
}

bool CSystem::DecompressDataBlock(const void * input, size_t inputSize, void * output, size_t& outputSize )
{
	uLongf destLen = outputSize;
	Bytef * dest = static_cast<Bytef *>(output);
	uLong sourceLen = inputSize;
	const Bytef * source = static_cast<const Bytef *>(input);
	bool ok = Z_OK == uncompress( dest, &destLen, source, sourceLen );
	outputSize = destLen;
	return ok;
}

bool CSystem::WriteCompressedFile(const char *filename, void *data, unsigned int bitlen)
{
#ifdef USE_COMPRESSION
  gzFile f = gzopen(filename, "wb9");
	if(f == Z_NULL)
		return false;
	gzwrite(f, &bitlen, sizeof(int));
	gzwrite(f, data, BITS2BYTES(bitlen));
	gzclose(f);
#else
	FILE *pFile = fxopen(filename, "wb+");
	if (!pFile)
		return false;
	fwrite(&bitlen, sizeof(int), 1, pFile);
	fwrite(data, BITS2BYTES(bitlen), 1, pFile);
	fclose(pFile);
#endif
    
	return true;
};

unsigned int CSystem::GetCompressedFileSize(const char *filename)
{
	int nLen = 0;
	{
		CCryFile file;
		if (!file.Open(filename,"rb"))
		{
			return 0;
		}
		nLen = file.GetLength();
		if (nLen == 0)
			return 0;
	}

	unsigned int bitlen = 0;
	gzFile f = gzopen(filename, "rb9");
	if (f)
	{
		gzread(f, &bitlen, sizeof(int));
		gzclose(f);
	}

	return bitlen;
}

unsigned int CSystem::ReadCompressedFile(const char *filename, void *data, unsigned int maxbitlen)
{
	unsigned int bitlen;
	
#ifdef USE_COMPRESSION
  gzFile f = gzopen(filename, "rb9");
	if(f == Z_NULL)
		return 0;
	gzread(f, &bitlen, sizeof(int));
	assert(bitlen<=maxbitlen);  // FIXME: nicer if caller doesn't need to know buffer size in advance
	gzread(f, data, BITS2BYTES(bitlen));
	gzclose(f);
	//fclose(pFile);
#else	
	CCryFile file;
	if (file.Open(filename.c_str(),"rb"))
	{
		file.ReadRaw(data,BITS2BYTES(bitlen));
	}
#endif

	return bitlen;
};
