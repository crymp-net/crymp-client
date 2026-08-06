#include <string.h>

#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CrySoundSystem/ISound.h"
#include "CryCommon/CrySoundSystem/ISoundMoodManager.h"
#include "CryCommon/CrySoundSystem/IReverbManager.h"
#include "CryCommon/CrySoundSystem/IMusicSystem.h"

#include "ScriptBind_Sound.h"
#include "../ScriptTable.h"

ISound *ScriptBind_Sound::GetSound(IFunctionHandler *pH, int index)
{
	switch (pH->GetParamType(index))
	{
		case svtPointer:
		{
			ScriptHandle soundID;
			if (!pH->GetParam(index, soundID))
				return nullptr;

			return gEnv->pSoundSystem->GetSound(static_cast<tSoundID>(soundID.n));
		}
		case svtObject:
		{
			SmartScriptTable table;
			if (!pH->GetParam(index, table))
				return nullptr;

			void *data = table->GetUserDataValue();
			if (!data)
				return nullptr;

			return *static_cast<ISound**>(data);
		}
		default:
		{
			return nullptr;
		}
	}
}

SmartScriptTable ScriptBind_Sound::CreateSoundTable(IFunctionHandler *pH, ISound *pSound)
{
	SmartScriptTable userData = m_pSS->CreateUserData(&pSound, sizeof (ISound*));

	IScriptTable::SUserFunctionDesc fd;
	fd.pUserDataFunc = SoundTableDestructor;
	fd.sFunctionName = "__gc";
	fd.nDataSize = sizeof (ISound*);
	fd.pDataBuffer = &pSound;
	fd.sGlobalName = "<gc-sound>";
	fd.sFunctionParams = "()";

	SmartScriptTable metaTable = SmartScriptTable(m_pSS);
	metaTable->AddFunction(fd);

	static_cast<ScriptTable*>(userData.GetPtr())->SetMetatable(metaTable);

	return userData;
}

int ScriptBind_Sound::SoundTableDestructor(IFunctionHandler *pH, void *buffer, int size)
{
	(*static_cast<ISound**>(buffer))->Release();

	return pH->EndFunction();
}

ScriptBind_Sound::ScriptBind_Sound(IScriptSystem *pSS)
{
	CScriptableBase::Init(pSS, gEnv->pSystem);
	SetGlobalName("Sound");

	pSS->SetGlobalValue("SOUND_LOOP", FLAG_SOUND_LOOP);
	pSS->SetGlobalValue("SOUND_2D", FLAG_SOUND_2D);
	pSS->SetGlobalValue("SOUND_3D", FLAG_SOUND_3D);
	pSS->SetGlobalValue("SOUND_STEREO", FLAG_SOUND_STEREO);
	pSS->SetGlobalValue("SOUND_16BITS", FLAG_SOUND_16BITS);
	pSS->SetGlobalValue("SOUND_STREAM", FLAG_SOUND_STREAM);
	pSS->SetGlobalValue("SOUND_RELATIVE", FLAG_SOUND_RELATIVE);
	pSS->SetGlobalValue("SOUND_RADIUS", FLAG_SOUND_RADIUS);
	pSS->SetGlobalValue("SOUND_DOPPLER", FLAG_SOUND_DOPPLER);
	pSS->SetGlobalValue("SOUND_NO_SW_ATTENUATION", FLAG_SOUND_NO_SW_ATTENUATION);
	pSS->SetGlobalValue("SOUND_MUSIC", FLAG_SOUND_MUSIC);
	pSS->SetGlobalValue("SOUND_OUTDOOR", FLAG_SOUND_OUTDOOR);
	pSS->SetGlobalValue("SOUND_INDOOR", FLAG_SOUND_INDOOR);
	pSS->SetGlobalValue("SOUND_UNSCALABLE", FLAG_SOUND_UNSCALABLE);
	pSS->SetGlobalValue("SOUND_CULLING", FLAG_SOUND_CULLING);
	pSS->SetGlobalValue("SOUND_LOAD_SYNCHRONOUSLY", FLAG_SOUND_LOAD_SYNCHRONOUSLY);
	pSS->SetGlobalValue("SOUND_MANAGED", FLAG_SOUND_MANAGED);
	pSS->SetGlobalValue("SOUND_FADE_OUT_UNDERWATER", FLAG_SOUND_FADE_OUT_UNDERWATER);
	pSS->SetGlobalValue("SOUND_OBSTRUCTION", FLAG_SOUND_OBSTRUCTION);
	pSS->SetGlobalValue("SOUND_SELFMOVING", FLAG_SOUND_SELFMOVING);
	pSS->SetGlobalValue("SOUND_START_PAUSED", FLAG_SOUND_START_PAUSED);
	pSS->SetGlobalValue("SOUND_VOICE", FLAG_SOUND_VOICE);
	pSS->SetGlobalValue("SOUND_EVENT", FLAG_SOUND_EVENT);
	pSS->SetGlobalValue("SOUND_EDITOR", FLAG_SOUND_EDITOR);
	pSS->SetGlobalValue("SOUND_SPREAD", FLAG_SOUND_SPREAD);
	pSS->SetGlobalValue("SOUND_DAYLIGHT", FLAG_SOUND_DAYLIGHT);
	pSS->SetGlobalValue("SOUND_SQUELCH", FLAG_SOUND_SQUELCH);
	pSS->SetGlobalValue("SOUND_DOPPLER_PARAM", FLAG_SOUND_DOPPLER_PARAM);
	pSS->SetGlobalValue("SOUND_DEFAULT_3D", FLAG_SOUND_DEFAULT_3D);
	pSS->SetGlobalValue("SOUND_ACTIVELIST", FLAG_SOUND_ACTIVELIST);

	pSS->SetGlobalValue("SOUND_PRECACHE_LOAD_SOUND", FLAG_SOUND_PRECACHE_LOAD_SOUND);
	pSS->SetGlobalValue("SOUND_PRECACHE_LOAD_GROUP", FLAG_SOUND_PRECACHE_LOAD_GROUP);
	pSS->SetGlobalValue("SOUND_PRECACHE_STAY_IN_MEMORY", FLAG_SOUND_PRECACHE_STAY_IN_MEMORY);
	pSS->SetGlobalValue("SOUND_PRECACHE_UNLOAD_AFTER_PLAY", FLAG_SOUND_PRECACHE_UNLOAD_AFTER_PLAY);
	pSS->SetGlobalValue("SOUND_PRECACHE_UNLOAD_NOW", FLAG_SOUND_PRECACHE_UNLOAD_NOW);
	pSS->SetGlobalValue("SOUND_PRECACHE_EVENT_DEFAULT", FLAG_SOUND_PRECACHE_EVENT_DEFAULT);
	pSS->SetGlobalValue("SOUND_PRECACHE_DIALOG_DEFAULT", FLAG_SOUND_PRECACHE_DIALOG_DEFAULT);
	pSS->SetGlobalValue("SOUND_PRECACHE_READABILITY_DEFAULT", FLAG_SOUND_PRECACHE_READABILITY_DEFAULT);

	pSS->SetGlobalValue("SOUNDSCALE_MASTER", SOUNDSCALE_MASTER);
	pSS->SetGlobalValue("SOUNDSCALE_SCALEABLE", SOUNDSCALE_SCALEABLE);
	pSS->SetGlobalValue("SOUNDSCALE_DEAFNESS", SOUNDSCALE_DEAFNESS);
	pSS->SetGlobalValue("SOUNDSCALE_UNDERWATER", SOUNDSCALE_UNDERWATER);
	pSS->SetGlobalValue("SOUNDSCALE_MISSIONHINT", SOUNDSCALE_MISSIONHINT);

	pSS->SetGlobalValue("SOUND_VOLUMESCALEMISSIONHINT", 0.45f);

	pSS->SetGlobalValue("SOUND_SEMANTIC_NONE", eSoundSemantic_None);
	pSS->SetGlobalValue("SOUND_SEMANTIC_ONLYVOICE", eSoundSemantic_OnlyVoice);
	pSS->SetGlobalValue("SOUND_SEMANTIC_NOVOICE", eSoundSemantic_NoVoice);
	pSS->SetGlobalValue("SOUND_SEMANTIC_UNUSED1", eSoundSemantic_Unused1);
	pSS->SetGlobalValue("SOUND_SEMANTIC_UNUSED2", eSoundSemantic_Unused2);
	pSS->SetGlobalValue("SOUND_SEMANTIC_UNUSED3", eSoundSemantic_Unused3);
	pSS->SetGlobalValue("SOUND_SEMANTIC_UNUSED4", eSoundSemantic_Unused4);
	pSS->SetGlobalValue("SOUND_SEMANTIC_AMBIENCE", eSoundSemantic_Ambience);
	pSS->SetGlobalValue("SOUND_SEMANTIC_AMBIENCE_ONESHOT", eSoundSemantic_Ambience_OneShot);
	pSS->SetGlobalValue("SOUND_SEMANTIC_PHYSICS_COLLISION", eSoundSemantic_Physics_Collision);
	pSS->SetGlobalValue("SOUND_SEMANTIC_DIALOG", eSoundSemantic_Dialog);
	pSS->SetGlobalValue("SOUND_SEMANTIC_MP_CHAT", eSoundSemantic_MP_Chat);
	pSS->SetGlobalValue("SOUND_SEMANTIC_PHYSICS_FOOTSTEP", eSoundSemantic_Physics_Footstep);
	pSS->SetGlobalValue("SOUND_SEMANTIC_PHYSICS_GENERAL", eSoundSemantic_Physics_General);
	pSS->SetGlobalValue("SOUND_SEMANTIC_HUD", eSoundSemantic_HUD);
	pSS->SetGlobalValue("SOUND_SEMANTIC_UNUSED5", eSoundSemantic_Unused5);
	pSS->SetGlobalValue("SOUND_SEMANTIC_FLOWGRAPH", eSoundSemantic_FlowGraph);
	pSS->SetGlobalValue("SOUND_SEMANTIC_PLAYER_FOLEY_VOICE", eSoundSemantic_Player_Foley_Voice);
	pSS->SetGlobalValue("SOUND_SEMANTIC_LIVING_ENTITY", eSoundSemantic_Living_Entity);
	pSS->SetGlobalValue("SOUND_SEMANTIC_MECHANIC_ENTITY", eSoundSemantic_Mechanic_Entity);
	pSS->SetGlobalValue("SOUND_SEMANTIC_NANOSUIT", eSoundSemantic_NanoSuit);
	pSS->SetGlobalValue("SOUND_SEMANTIC_SOUNDSPOT", eSoundSemantic_SoundSpot);
	pSS->SetGlobalValue("SOUND_SEMANTIC_PARTICLE", eSoundSemantic_Particle);
	pSS->SetGlobalValue("SOUND_SEMANTIC_AI_PAIN_DEATH", eSoundSemantic_AI_Pain_Death);
	pSS->SetGlobalValue("SOUND_SEMANTIC_AI_READABILITY", eSoundSemantic_AI_Readability);
	pSS->SetGlobalValue("SOUND_SEMANTIC_AI_READABILITY_RESPONSE", eSoundSemantic_AI_Readability_Response);
	pSS->SetGlobalValue("SOUND_SEMANTIC_TRACKVIEW", eSoundSemantic_TrackView);
	pSS->SetGlobalValue("SOUND_SEMANTIC_PROJECTILE", eSoundSemantic_Projectile);
	pSS->SetGlobalValue("SOUND_SEMANTIC_VEHICLE", eSoundSemantic_Vehicle);
	pSS->SetGlobalValue("SOUND_SEMANTIC_WEAPON", eSoundSemantic_Weapon);
	pSS->SetGlobalValue("SOUND_SEMANTIC_EXPLOSION", eSoundSemantic_Explosion);
	pSS->SetGlobalValue("SOUND_SEMANTIC_PLAYER_FOLEY", eSoundSemantic_Player_Foley);
	pSS->SetGlobalValue("SOUND_SEMANTIC_ANIMATION", eSoundSemantic_Animation);
	pSS->SetGlobalValue("SOUND_SEMANTIC_FOOTSTEP", eSoundSemantic_Physics_Footstep); //for Hunter_x.lua scriptbug

#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &ScriptBind_Sound::

	// sound
	SCRIPT_REG_FUNC(AddToScaleGroup);
	SCRIPT_REG_FUNC(GetSoundLength);
	SCRIPT_REG_FUNC(GetSoundVolume);
	SCRIPT_REG_FUNC(IsPlaying);
	SCRIPT_REG_TEMPLFUNC(Load3DSound, "sound");
	SCRIPT_REG_TEMPLFUNC(LoadSound, "sound");
	SCRIPT_REG_TEMPLFUNC(LoadStreamSound, "sound");
	SCRIPT_REG_TEMPLFUNC(Play, "sound, pos, flags, semantic");
	SCRIPT_REG_TEMPLFUNC(PlayEx, "sound, pos, flags, volume, minRadius, maxRadius, semantic");
	SCRIPT_REG_FUNC(PlaySound);
	SCRIPT_REG_TEMPLFUNC(Precache, "sound, flags");
	SCRIPT_REG_TEMPLFUNC(RegisterSoundMood, "mood");
	SCRIPT_REG_TEMPLFUNC(RegisterWeightedEaxEnvironment, "preset");
	SCRIPT_REG_FUNC(SetFadeTime);
	SCRIPT_REG_TEMPLFUNC(SetGroupScale, "group, scale");
	SCRIPT_REG_TEMPLFUNC(SetMasterVolumeScale, "scale");
	SCRIPT_REG_FUNC(SetMinMaxDistance);
	SCRIPT_REG_FUNC(SetParameterValue);
	SCRIPT_REG_FUNC(GetParameterValue);
	SCRIPT_REG_FUNC(SetSoundLoop);
	SCRIPT_REG_FUNC(SetSoundPaused);
	SCRIPT_REG_FUNC(SetSoundPosition);
	SCRIPT_REG_FUNC(SetSoundVolume);
	SCRIPT_REG_FUNC(Silence);
	SCRIPT_REG_FUNC(StopSound);
	SCRIPT_REG_TEMPLFUNC(UnregisterSoundMood, "mood");
	SCRIPT_REG_TEMPLFUNC(UnregisterWeightedEaxEnvironment, "preset");
	SCRIPT_REG_TEMPLFUNC(UpdateSoundMood, "mood, fade, fadeTimeMs");
	SCRIPT_REG_TEMPLFUNC(UpdateWeightedEaxEnvironment, "preset, weight");

	// music
	SCRIPT_REG_FUNC(EndMusicTheme);
	SCRIPT_REG_FUNC(PlayPattern);
	SCRIPT_REG_FUNC(PlayStinger);
	SCRIPT_REG_FUNC(GetMusicStatus);
	SCRIPT_REG_FUNC(GetCurrentMood);
	SCRIPT_REG_FUNC(GetCurrentTheme);
	SCRIPT_REG_FUNC(GetMusicThemes);
	SCRIPT_REG_FUNC(GetMusicMoods);
	SCRIPT_REG_TEMPLFUNC(SetDefaultMusicMood, "mood");
	SCRIPT_REG_TEMPLFUNC(SetMenuMusic, "playMenuMusic, theme, mood");
	SCRIPT_REG_TEMPLFUNC(SetMusicMood, "mood");
	SCRIPT_REG_TEMPLFUNC(SetMusicTheme, "theme");

	SCRIPT_REG_FUNC(ReloadAudioDevice);
}

int ScriptBind_Sound::AddToScaleGroup(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);

	_smart_ptr<ISound> pSound = GetSound(pH, 1);

	int group = 0;

	pH->GetParam(2, group);

	if (pSound)
	{
		pSound->AddToScaleGroup(group);
	}

	return pH->EndFunction();
}

int ScriptBind_Sound::GetSoundLength(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	_smart_ptr<ISound> pSound = GetSound(pH, 1);

	float length = 0;

	if (pSound)
	{
		length = static_cast<float>(pSound->GetLengthMs()) / 1000;
	}

	return pH->EndFunction(length);
}

int ScriptBind_Sound::GetSoundVolume(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	_smart_ptr<ISound> pSound = GetSound(pH, 1);

	float volume = 0;

	if (pSound)
	{
		volume = pSound->GetVolume();
	}

	return pH->EndFunction(volume);
}

int ScriptBind_Sound::IsPlaying(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	_smart_ptr<ISound> pSound = GetSound(pH, 1);

	bool isPlaying = false;

	if (pSound)
	{
		isPlaying = pSound->IsPlaying();
	}

	return pH->EndFunction(isPlaying);
}

int ScriptBind_Sound::Load3DSound(IFunctionHandler *pH, const char *sound)
{
	ISoundSystem *pSoundSystem = gEnv->pSoundSystem;
	if (!pSoundSystem)
		return pH->EndFunction();

	const int paramCount = pH->GetParamCount();

	uint32_t flags = 0;
	float volume = 1;
	float minDistance = 1;
	float clipDistance = 500;
	int priority = 0;

	if (paramCount >= 2)
		pH->GetParam(2, flags);

	if (paramCount >= 3)
		pH->GetParam(3, volume);

	if (paramCount >= 4)
		pH->GetParam(4, minDistance);

	if (paramCount >= 5)
		pH->GetParam(5, clipDistance);

	if (paramCount >= 6)
		pH->GetParam(6, priority);

	if (clipDistance > 1000)
		clipDistance = 1000;

	_smart_ptr<ISound> pSound = pSoundSystem->CreateSound(sound, FLAG_SOUND_3D | flags);

	if (pSound)
	{
		pSound->SetVolume(volume);
		pSound->SetMinMaxDistance(minDistance, clipDistance / 2);

		if (paramCount >= 7)
		{
			int groups = 0;
			pH->GetParam(7, groups);

			pSound->SetScaleGroup(groups);
		}

		pSound->AddRef();
		pSound->SetSoundPriority(priority);

		return pH->EndFunction(CreateSoundTable(pH, pSound));
	}
	else
	{
		CryLogError("Sound.Load3DSound: Failed to load sound '%s'", sound);

		return pH->EndFunction();
	}
}

int ScriptBind_Sound::LoadSound(IFunctionHandler *pH, const char *sound)
{
	ISoundSystem *pSoundSystem = gEnv->pSoundSystem;
	if (!pSoundSystem)
		return pH->EndFunction();

	const int paramCount = pH->GetParamCount();

	uint32_t flags = 0;
	float volume = 1;

	if (paramCount >= 2)
		pH->GetParam(2, flags);

	if (paramCount >= 3)
		pH->GetParam(3, volume);

	if (volume < 0 || volume > 1)
		volume = 1;

	_smart_ptr<ISound> pSound = pSoundSystem->CreateSound(sound, flags);

	if (pSound)
	{
		pSound->SetVolume(volume);
		pSound->AddRef();

		return pH->EndFunction(CreateSoundTable(pH, pSound));
	}
	else
	{
		CryLogError("Sound.LoadSound: Failed to load sound '%s'", sound);

		return pH->EndFunction();
	}
}

int ScriptBind_Sound::LoadStreamSound(IFunctionHandler *pH, const char *sound)
{
	ISoundSystem *pSoundSystem = gEnv->pSoundSystem;
	if (!pSoundSystem)
		return pH->EndFunction();

	const int paramCount = pH->GetParamCount();

	uint32_t flags = 0;

	if (paramCount >= 2)
		pH->GetParam(2, flags);

	_smart_ptr<ISound> pSound = pSoundSystem->CreateSound(sound, FLAG_SOUND_STREAM | flags);

	if (pSound)
	{
		pSound->AddRef();

		return pH->EndFunction(CreateSoundTable(pH, pSound));
	}
	else
	{
		CryLogError("Sound.LoadStreamSound: Failed to load sound '%s'", sound);

		return pH->EndFunction();
	}
}

int ScriptBind_Sound::Play(IFunctionHandler *pH, const char *sound, Vec3 pos)
{
	ISoundSystem *pSoundSystem = gEnv->pSoundSystem;
	if (!pSoundSystem)
		return pH->EndFunction();

	uint32_t flags = 0;
	if (pH->GetParamCount() > 2)
		pH->GetParam(3, flags);

	_smart_ptr<ISound> pSound = pSoundSystem->CreateSound(sound, flags);

	if (pSound)
	{
		pSound->SetPosition(pos);

		uint32_t semantic = 0;
		if (pH->GetParamCount() > 3)
			pH->GetParam(4, semantic);

		pSound->SetSemantic(static_cast<ESoundSemantic>(semantic));
		pSound->Play();

		return pH->EndFunction(ScriptHandle(pSound->GetId()));
	}
	else
	{
		return pH->EndFunction();
	}
}

int ScriptBind_Sound::PlayEx(IFunctionHandler *pH, const char *sound, Vec3 pos, uint32_t flags, float volume, float minRadius, float maxRadius, uint32_t semantic)
{
	ISoundSystem *pSoundSystem = gEnv->pSoundSystem;
	if (!pSoundSystem)
		return pH->EndFunction();

	_smart_ptr<ISound> pSound = pSoundSystem->CreateSound(sound, flags);

	if (pSound)
	{
		pSound->SetPosition(pos);
		pSound->SetSemantic(static_cast<ESoundSemantic>(semantic));
		pSound->SetVolume(volume);

		if (minRadius > 0 && maxRadius > 0)
			pSound->SetMinMaxDistance(minRadius, maxRadius);

		pSound->Play();

		return pH->EndFunction(ScriptHandle(pSound->GetId()));
	}
	else
	{
		return pH->EndFunction();
	}
}

int ScriptBind_Sound::PlaySound(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS_MIN(1);

	_smart_ptr<ISound> pSound = GetSound(pH, 1);

	float volumeScale = 1;

	if (pH->GetParamCount() >= 2)
		pH->GetParam(2, volumeScale);

	if (pSound)
	{
		pSound->Play(volumeScale);
	}

	return pH->EndFunction();
}

int ScriptBind_Sound::Precache(IFunctionHandler *pH, const char *sound, uint32_t flags)
{
	ISoundSystem *pSoundSystem = gEnv->pSoundSystem;
	if (!pSoundSystem)
		return pH->EndFunction();

	const bool result = (pSoundSystem->Precache(sound, 0, flags) != ePrecacheResult_Error);

	return pH->EndFunction(result);
}

int ScriptBind_Sound::RegisterSoundMood(IFunctionHandler *pH, const char *mood)
{
	ISoundSystem *pSoundSystem = gEnv->pSoundSystem;
	if (!pSoundSystem)
		return pH->EndFunction();

	ISoundMoodManager *pSoundMoodManager = pSoundSystem->GetIMoodManager();
	if (!pSoundMoodManager)
		return pH->EndFunction();

	pSoundMoodManager->RegisterSoundMood(mood);

	return pH->EndFunction();
}

int ScriptBind_Sound::RegisterWeightedEaxEnvironment(IFunctionHandler *pH, const char *preset)
{
	ISoundSystem *pSoundSystem = gEnv->pSoundSystem;
	if (!pSoundSystem)
		return pH->EndFunction();

	IReverbManager *pReverbManager = pSoundSystem->GetIReverbManager();
	if (!pReverbManager)
		return pH->EndFunction();

	const int paramCount = pH->GetParamCount();

	bool fullEffectWhenInside = false;
	uint32_t flags = 0;

	if (paramCount >= 2)
		pH->GetParam(2, fullEffectWhenInside);

	if (paramCount >= 3)
		pH->GetParam(3, flags);

	pReverbManager->RegisterWeightedReverbEnvironment(preset, fullEffectWhenInside, flags);

	return pH->EndFunction();
}

int ScriptBind_Sound::SetFadeTime(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(3);

	_smart_ptr<ISound> pSound = GetSound(pH, 1);

	float fadeGoal = 0;
	int fadeTimeMs = 0;

	pH->GetParam(2, fadeGoal);
	pH->GetParam(3, fadeTimeMs);

	if (fadeGoal < 0)
		fadeGoal = 0;

	if (fadeGoal > 1)
		fadeGoal = 1;

	if (fadeTimeMs < 0)
		fadeTimeMs = 1;

	if (pSound)
	{
		pSound->SetFade(fadeGoal, fadeTimeMs);
	}

	return pH->EndFunction();
}

int ScriptBind_Sound::SetGroupScale(IFunctionHandler *pH, int group, float scale)
{
	ISoundSystem *pSoundSystem = gEnv->pSoundSystem;
	if (!pSoundSystem)
		return pH->EndFunction();

	const bool result = pSoundSystem->SetGroupScale(group, scale);

	return pH->EndFunction(result);
}

int ScriptBind_Sound::SetMasterVolumeScale(IFunctionHandler *pH, float scale)
{
	ISoundSystem *pSoundSystem = gEnv->pSoundSystem;
	if (!pSoundSystem)
		return pH->EndFunction();

	pSoundSystem->SetMasterVolumeScale(scale);

	return pH->EndFunction();
}

int ScriptBind_Sound::SetMinMaxDistance(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(3);

	_smart_ptr<ISound> pSound = GetSound(pH, 1);

	float minDist = 0;
	float maxDist = 0;

	pH->GetParam(2, minDist);
	pH->GetParam(3, maxDist);

	if (minDist < 1)
		minDist = 1;

	if (pSound)
	{
		pSound->SetMinMaxDistance(minDist, maxDist);
	}

	return pH->EndFunction();
}

int ScriptBind_Sound::SetParameterValue(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(3);

	_smart_ptr<ISound> pSound = GetSound(pH, 1);

	if (!pSound)
		return pH->EndFunction();

	const char *paramName = nullptr;
	int paramId = 0;
	float paramValue = 0.0f;

	pH->GetParam(3, paramValue);

	if (pH->GetParamType(2) == svtString && pH->GetParam(2, paramName))
	{
		if (strcmp(paramName, "pitch") == 0)
		{
			const int pitch = clamp_tpl(
				(int)(paramValue * 1000.0f),
				100,
				4000);

			pSound->SetPitch(pitch);
		}
		else
		{
			pSound->SetParam(paramName, paramValue);
		}
	}
	else if (pH->GetParamType(2) == svtNumber && pH->GetParam(2, paramId))
	{
		return pH->EndFunction(pSound->SetParam(paramId, paramValue));
	}

	return pH->EndFunction();
}

int ScriptBind_Sound::GetParameterValue(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);

	_smart_ptr<ISound> pSound = GetSound(pH, 1);

	if (!pSound)
		return pH->EndFunction();

	if (pH->GetParamType(2) == svtNumber)
	{
		int paramId = 0;
		if (pH->GetParam(2, paramId))
		{
			float value = 0;
			pSound->GetParam(paramId, &value);

			return pH->EndFunction(value);
		}
	}
	else if (pH->GetParamType(2) == svtString)
	{
		const char* paramStr = nullptr;
		if (pH->GetParam(2, paramStr))
		{
			float value = 0;
			pSound->GetParam(paramStr, &value);

			return pH->EndFunction(value);
		}
	}

	return pH->EndFunction();
}

int ScriptBind_Sound::SetSoundLoop(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);

	_smart_ptr<ISound> pSound = GetSound(pH, 1);

	bool enable = false;

	pH->GetParam(2, enable);

	if (pSound)
	{
		pSound->SetLoopMode(enable);
	}

	return pH->EndFunction();
}

int ScriptBind_Sound::SetSoundPaused(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);

	_smart_ptr<ISound> pSound = GetSound(pH, 1);

	bool paused = false;

	pH->GetParam(2, paused);

	if (pSound)
	{
		pSound->SetPaused(paused);
	}

	return pH->EndFunction();
}

int ScriptBind_Sound::SetSoundPosition(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);

	_smart_ptr<ISound> pSound = GetSound(pH, 1);

	Vec3 pos;

	pH->GetParam(2, pos);

	if (pSound)
	{
		if (!pSound->IsRelative())
			pSound->SetPosition(pos);
	}

	return pH->EndFunction();
}

int ScriptBind_Sound::SetSoundVolume(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);

	_smart_ptr<ISound> pSound = GetSound(pH, 1);

	float volume = 0;

	pH->GetParam(2, volume);

	if (pSound)
	{
		pSound->SetVolume(volume);
	}

	return pH->EndFunction();
}

int ScriptBind_Sound::Silence(IFunctionHandler *pH)
{
	ISoundSystem *pSoundSystem = gEnv->pSoundSystem;
	if (!pSoundSystem)
		return pH->EndFunction();

	const bool result = pSoundSystem->Silence(true, false);

	return pH->EndFunction(result);
}

int ScriptBind_Sound::StopSound(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	_smart_ptr<ISound> pSound = GetSound(pH, 1);

	if (pSound)
	{
		pSound->Stop();
	}

	return pH->EndFunction();
}

int ScriptBind_Sound::UnregisterSoundMood(IFunctionHandler *pH, const char *mood)
{
	ISoundSystem *pSoundSystem = gEnv->pSoundSystem;
	if (!pSoundSystem)
		return pH->EndFunction();

	ISoundMoodManager *pSoundMoodManager = pSoundSystem->GetIMoodManager();
	if (!pSoundMoodManager)
		return pH->EndFunction();

	pSoundMoodManager->UnregisterSoundMood(mood);

	return pH->EndFunction();
}

int ScriptBind_Sound::UnregisterWeightedEaxEnvironment(IFunctionHandler *pH, const char *preset)
{
	ISoundSystem *pSoundSystem = gEnv->pSoundSystem;
	if (!pSoundSystem)
		return pH->EndFunction();

	IReverbManager *pReverbManager = pSoundSystem->GetIReverbManager();
	if (!pReverbManager)
		return pH->EndFunction();

	pReverbManager->UnregisterWeightedReverbEnvironment(preset);

	return pH->EndFunction();
}

int ScriptBind_Sound::UpdateSoundMood(IFunctionHandler *pH, const char *mood, float fade, uint32_t fadeTimeMs)
{
	ISoundSystem *pSoundSystem = gEnv->pSoundSystem;
	if (!pSoundSystem)
		return pH->EndFunction();

	ISoundMoodManager *pSoundMoodManager = pSoundSystem->GetIMoodManager();
	if (!pSoundMoodManager)
		return pH->EndFunction();

	pSoundMoodManager->UpdateSoundMood(mood, fade, fadeTimeMs);

	return pH->EndFunction();
}

int ScriptBind_Sound::UpdateWeightedEaxEnvironment(IFunctionHandler *pH, const char *preset, float weight)
{
	ISoundSystem *pSoundSystem = gEnv->pSoundSystem;
	if (!pSoundSystem)
		return pH->EndFunction();

	IReverbManager *pReverbManager = pSoundSystem->GetIReverbManager();
	if (!pReverbManager)
		return pH->EndFunction();

	pReverbManager->UpdateWeightedReverbEnvironment(preset, weight);

	return pH->EndFunction();
}

int ScriptBind_Sound::GetMusicStatus(IFunctionHandler* pH)
{
	IMusicSystem* pMusicSystem = gEnv->pMusicSystem;
	if (!pMusicSystem)
		return pH->EndFunction();

	SCRIPT_CHECK_PARAMETERS(0);
	SMusicSystemStatus* pStatus = pMusicSystem->GetStatus();
	CryLogAlways("--- MusicSystem Status Info ---");
	CryLogAlways("  Streaming: %s", pStatus->bPlaying ? "Yes" : "No");
	CryLogAlways("  Theme: %s", pStatus->sTheme.c_str());
	CryLogAlways("  Mood: %s", pStatus->sMood.c_str());
	CryLogAlways("  Playing patterns:");
	for (TPatternStatusVecIt It = pStatus->m_vecPlayingPatterns.begin(); It != pStatus->m_vecPlayingPatterns.end(); ++It)
	{
		SPlayingPatternsStatus& PatternStatus = (*It);
		CryLogAlways("    %s (Layer: %s; Volume: %f)", PatternStatus.sName.c_str(), (PatternStatus.nLayer == MUSICLAYER_MAIN) ? "Main" : ((PatternStatus.nLayer == MUSICLAYER_RHYTHMIC) ? "Rhythmic" : "Incidental"), (float)PatternStatus.fVolume);
	}
	return pH->EndFunction();
}

int ScriptBind_Sound::GetCurrentTheme(IFunctionHandler* pH)
{
	IMusicSystem* pMusicSystem = gEnv->pMusicSystem;
	if (!pMusicSystem)
		return pH->EndFunction();

	SMusicSystemStatus* pStatus = pMusicSystem->GetStatus();
	if (!pStatus)
		return pH->EndFunction();

	return pH->EndFunction(pStatus->sTheme.c_str());
}

int ScriptBind_Sound::GetCurrentMood(IFunctionHandler* pH)
{
	IMusicSystem* pMusicSystem = gEnv->pMusicSystem;
	if (!pMusicSystem)
		return pH->EndFunction();

	SMusicSystemStatus* pStatus = pMusicSystem->GetStatus();
	if (!pStatus)
		return pH->EndFunction();

	return pH->EndFunction(pStatus->sMood.c_str());
}

int ScriptBind_Sound::GetMusicThemes(IFunctionHandler* pH)
{
	IMusicSystem* pMusicSystem = gEnv->pMusicSystem;
	if (!pMusicSystem)
		return pH->EndFunction();

	SCRIPT_CHECK_PARAMETERS(0);
	IStringItVec* pVec = pMusicSystem->GetThemes();
	if (!pVec)
		return pH->EndFunction();
	SmartScriptTable pTable(m_pSS);
	int i = 1;
	while (!pVec->IsEnd())
	{
		pTable->SetAt(i++, pVec->Next());
	}
	pVec->Release();
	return pH->EndFunction(pTable);
}

int ScriptBind_Sound::GetMusicMoods(IFunctionHandler* pH)
{
	IMusicSystem* pMusicSystem = gEnv->pMusicSystem;
	if (!pMusicSystem)
		return pH->EndFunction();

	SCRIPT_CHECK_PARAMETERS(1);
	const char* pszTheme;
	pH->GetParam(1, pszTheme);
	IStringItVec* pVec = pMusicSystem->GetMoods(pszTheme);
	if (!pVec)
		return pH->EndFunction();
	SmartScriptTable pTable(m_pSS);
	int i = 1;
	while (!pVec->IsEnd())
	{
		pTable->SetAt(i++, pVec->Next());
	}
	pVec->Release();
	return pH->EndFunction(pTable);
}

int ScriptBind_Sound::EndMusicTheme(IFunctionHandler *pH)
{
	IMusicSystem *pMusicSystem = gEnv->pMusicSystem;
	if (!pMusicSystem)
		return pH->EndFunction();

	const int paramCount = pH->GetParamCount();

	int themeFadeType = 0;
	int forceEndLimitInSec = 5;
	bool endEverything = false;

	if (paramCount >= 1)
		pH->GetParam(1, themeFadeType);

	if (paramCount >= 2)
		pH->GetParam(2, forceEndLimitInSec);

	if (paramCount >= 3)
		pH->GetParam(3, endEverything);

	const bool result = pMusicSystem->EndTheme(static_cast<EThemeFadeType>(themeFadeType), forceEndLimitInSec, endEverything);

	if (!result)
		CryLogError("Unable to end music theme!");

	return pH->EndFunction(result);
}

int ScriptBind_Sound::PlayPattern(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(3);

	IMusicSystem *pMusicSystem = gEnv->pMusicSystem;
	if (!pMusicSystem)
		return pH->EndFunction();

	const char *pattern = nullptr;
	bool stopPrevious = false;
	bool playSynched = false;

	pH->GetParam(1, pattern);
	pH->GetParam(2, stopPrevious);
	pH->GetParam(3, playSynched);

	pMusicSystem->PlayPattern(pattern, stopPrevious, playSynched);

	return pH->EndFunction();
}

int ScriptBind_Sound::PlayStinger(IFunctionHandler *pH)
{
	IMusicSystem *pMusicSystem = gEnv->pMusicSystem;
	if (!pMusicSystem)
		return pH->EndFunction();

	pMusicSystem->PlayStinger();

	return pH->EndFunction();
}

int ScriptBind_Sound::SetDefaultMusicMood(IFunctionHandler *pH, const char *mood)
{
	IMusicSystem *pMusicSystem = gEnv->pMusicSystem;
	if (!pMusicSystem)
		return pH->EndFunction();

	const bool result = pMusicSystem->SetDefaultMood(mood);

	if (!result)
		CryLogError("Unable to set default music mood '%s'", mood);

	return pH->EndFunction(result);
}

int ScriptBind_Sound::SetMenuMusic(IFunctionHandler *pH, bool playMenuMusic, const char *theme, const char *mood)
{
	IMusicSystem *pMusicSystem = gEnv->pMusicSystem;
	if (!pMusicSystem)
		return pH->EndFunction();

	if (playMenuMusic)
	{
		pMusicSystem->SerializeInternal(true);

		if (!m_isMenuMusicLoaded)
		{
			pMusicSystem->LoadFromXML("music/common_music.xml", true, false);
			m_isMenuMusicLoaded = true;
		}

		pMusicSystem->SetTheme(theme, false);
		pMusicSystem->SetMood(mood);
	}
	else
	{
		pMusicSystem->SerializeInternal(false);
	}

	return pH->EndFunction();
}

int ScriptBind_Sound::SetMusicMood(IFunctionHandler *pH, const char *mood)
{
	IMusicSystem *pMusicSystem = gEnv->pMusicSystem;
	if (!pMusicSystem)
		return pH->EndFunction();

	const int paramCount = pH->GetParamCount();

	bool playFromStart = true;
	bool forceChange = false;

	if (paramCount >= 2)
		pH->GetParam(2, playFromStart);

	if (paramCount >= 3)
		pH->GetParam(3, forceChange);

	const bool result = pMusicSystem->SetMood(mood, playFromStart, forceChange);

	if (!result)
		CryLogError("Unable to set music mood '%s'", mood);

	return pH->EndFunction(result);
}

int ScriptBind_Sound::SetMusicTheme(IFunctionHandler *pH, const char *theme)
{
	IMusicSystem *pMusicSystem = gEnv->pMusicSystem;
	if (!pMusicSystem)
		return pH->EndFunction();

	const int paramCount = pH->GetParamCount();

	bool forceChange = false;
	bool keepMood = false;
	int delaySec = -1;

	if (paramCount >= 2)
		pH->GetParam(2, forceChange);

	if (paramCount >= 3)
		pH->GetParam(3, keepMood);

	if (paramCount >= 4)
		pH->GetParam(4, delaySec);

	const bool result = pMusicSystem->SetTheme(theme, forceChange, keepMood, delaySec);

	if (!result)
		CryLogError("Unable to set music theme '%s'", theme);

	return pH->EndFunction(result);
}

int ScriptBind_Sound::ReloadAudioDevice(IFunctionHandler* pH)
{
	gEnv->pSoundSystem->DeactivateAudioDevice();
	const bool result = gEnv->pSoundSystem->ActivateAudioDevice();

	return pH->EndFunction(result);
}
