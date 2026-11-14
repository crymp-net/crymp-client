#include "IAudioDevice.h"
#include "Microphone.h"
#include "SoundBuffer.h"
#include "FMOD.h"

CMicrophone::CMicrophone(IAudioDevice* pAudioDevice) : m_pAudioDevice(pAudioDevice)
{
}

CMicrophone::~CMicrophone()
{
}

void CMicrophone::Release()
{
	std::lock_guard lock(m_mutex);
	Stop();
}

void CMicrophone::Update()
{
	std::lock_guard lock(m_mutex);

	if (!m_pBufMicro)
	{
		m_buffer.clear();
		return;
	}

	m_pBufMicro->Update();
}

bool CMicrophone::Stop()
{
	std::lock_guard lock(m_mutex);

	if (m_pBufMicro)
	{
		m_pBufMicro->UnloadData(sbUNLOAD_ALL_DATA);
		m_pBufMicro = nullptr;
	}

	m_buffer.clear();

	return true;
}

bool CMicrophone::Record(const unsigned int nRecordDevice, const unsigned int nBitsPerSample,
                         const unsigned int nSamplesPerSecond, const unsigned int nBufferSizeInSamples)
{
	std::lock_guard lock(m_mutex);

	SSoundBufferProps MicroProp = SSoundBufferProps("MicrophoneStream", 0, btMICRO, 0);
	m_pBufMicro = m_pAudioDevice->CreateSoundBuffer(MicroProp);

	m_pBufMicro->GetInfo()->nBitsPerSample = nBitsPerSample;
	m_pBufMicro->GetInfo()->nBaseFreq = nSamplesPerSecond;
	m_pBufMicro->GetInfo()->nChannels = 1;
	m_pBufMicro->GetInfo()->nLengthInSamples = nBufferSizeInSamples;

	m_pBufMicro->LoadAsMicro(this);

	m_buffer.clear();
	m_buffer.reserve(nBufferSizeInSamples);

	return true;
}

int16* CMicrophone::GetData()
{
	return m_buffer.empty() ? nullptr : reinterpret_cast<int16*>(m_buffer.data());
}

int CMicrophone::GetDataSize()
{
	return static_cast<int>(m_buffer.size());
}

bool CMicrophone::ReadDataBuffer(const void* ptr1, const unsigned int len1, const void* ptr2, const unsigned int len2)
{
	std::lock_guard lock(m_mutex);

	m_buffer.insert(m_buffer.end(), static_cast<const uint8_t*>(ptr1), static_cast<const uint8_t*>(ptr1) + len1);
	m_buffer.insert(m_buffer.end(), static_cast<const uint8_t*>(ptr2), static_cast<const uint8_t*>(ptr2) + len2);

	return true;
}

void CMicrophone::OnError(const char* error)
{
	// FIXME
}
