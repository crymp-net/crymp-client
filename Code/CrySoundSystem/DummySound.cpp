#include "CryCommon/CryRenderer/IRenderer.h"
#include "CryCommon/CrySystem/gEnv.h"

#include "DummySound.h"

void CDummySoundSystem::Update(ESoundUpdateMode UpdateMode)
{
	if (g_nSoundInfo)
	{
		if (gEnv && gEnv->pRenderer)
		{
			float fColorRed[4] = {1.0f, 0.0f, 0.0f, 0.7f};
			gEnv->pRenderer->Draw2dLabel(1, 1, 2, fColorRed, false,
			                             "SoundSystem in DUMMYMODE (check system.cfg)");
		}
	}
}
