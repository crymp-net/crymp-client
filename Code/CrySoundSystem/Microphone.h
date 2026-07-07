#pragma once

#include <mutex>
#include <vector>

#include "CryCommon/CrySoundSystem/ISound.h"

#include "ISoundBuffer.h"

class CSoundBuffer;

class CMicrophone final : public IMicrophone, public IMicrophoneStream
{
	std::recursive_mutex m_mutex;

	IAudioDevice* m_pAudioDevice = nullptr;
	CSoundBuffer* m_pBufMicro = nullptr;

	std::vector<uint8_t> m_buffer;

public:
	explicit CMicrophone(IAudioDevice* pAudioDevice);
	~CMicrophone();

	void Release();

	bool Record(const unsigned int nRecordDevice, const unsigned int nBitsPerSample,
	            const unsigned int nSamplesPerSecond, const unsigned int nBufferSizeInSamples);

	void Update();

	bool Stop();

	int GetDataSize();

	int16* GetData();

	// from IMicrophoneStream
	bool ReadDataBuffer(const void* ptr1, const unsigned int len1, const void* ptr2, const unsigned int len2);

	void OnError(const char* error);
};
