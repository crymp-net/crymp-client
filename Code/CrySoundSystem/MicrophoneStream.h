#pragma once

class CMicrophoneStream
{
public:
	CMicrophoneStream();
	~CMicrophoneStream();

	void Release();

	// ask for butter to be read
	bool ReadDataBuffer(const unsigned int nBitsPerSample, const unsigned int nSamplesPerSecond,
	                    const unsigned int nNumSamples, void* pData);
};
