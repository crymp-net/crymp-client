#include "MicrophoneStream.h"

CMicrophoneStream::CMicrophoneStream()
{
}

CMicrophoneStream::~CMicrophoneStream()
{
}

void CMicrophoneStream::Release()
{
	// delete this;
}

// ask for butter to be read
bool CMicrophoneStream::ReadDataBuffer(const unsigned int nBitsPerSample, const unsigned int nSamplesPerSecond,
                                       const unsigned int nNumSamples, void* pData)
{
	return true;
}
