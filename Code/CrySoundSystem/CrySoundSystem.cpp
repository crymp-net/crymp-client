#include "AudioDeviceFmodEx400.h"
#include "CrySoundSystem.h"
#include "DummySound.h"
#include "SoundSystem.h"

ISoundSystem* CreateSoundSystem(ISystem* pSystem, void* hWnd)
{
	CSoundSystem* pSoundSystem = nullptr;
	bool deviceInitialized = false;
	bool soundSystemInitialized = false;

	CAudioDeviceFmodEx400* pAudioDevice = new CAudioDeviceFmodEx400(hWnd);
	if (pAudioDevice)
	{
		pSoundSystem = new CSoundSystem(hWnd, pAudioDevice);
		if (pSoundSystem)
		{
			if (pSoundSystem->IsOK())
			{
				CryLogAlways("Sound - initializing AudioDevice now!");
				deviceInitialized = pAudioDevice->InitDevice(pSoundSystem);
			}

			if (deviceInitialized)
			{
				CryLogAlways("Sound - initializing SoundSystem now!");
				soundSystemInitialized = pSoundSystem->Init();
			}
			else
			{
				CryLogAlways("Error: Sound - Cannot initialize SoundSystem!");
			}
		}
		else
		{
			CryLogAlways("Error: Sound - Cannot create SoundSystem!");
		}
	}

	if (!pSoundSystem || !soundSystemInitialized || !pAudioDevice || !deviceInitialized)
	{
		CryLogAlways("Error: Sound - Something went wrong. Creating DummySoundSystem now!");

		if (pSoundSystem)
		{
			pSoundSystem->Release();
			pSoundSystem = nullptr;
		}

		if (pAudioDevice)
		{
			pAudioDevice->ShutDownDevice();
			delete pAudioDevice;
			pAudioDevice = nullptr;
		}

		return new CDummySoundSystem(hWnd);
	}

	return pSoundSystem;
}
