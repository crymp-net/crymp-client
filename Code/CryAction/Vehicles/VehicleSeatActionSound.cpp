/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a seat action for sounds (ie: honk on trucks)

-------------------------------------------------------------------------
History:
- 16:11:2005: Created by Mathieu Pinard

*************************************************************************/
#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CryAction/IVehicleSystem.h"
#include "Vehicle.h"
#include "VehicleSeat.h"
#include "VehicleSeatActionSound.h"

CVehicleSeatActionSound::CVehicleSeatActionSound()
: m_pHelper(0)
{
}

//------------------------------------------------------------------------
bool CVehicleSeatActionSound::Init(IVehicle* pVehicle, TVehicleSeatId seatId, const CVehicleParams& table)
{
	m_pVehicle = pVehicle;
	m_seatId = seatId;

	CVehicleParams soundTable = table.findChild("Sound");
	if (!soundTable)
		return false;

	m_soundName = soundTable.getAttr("sound");
	m_pHelper = NULL;

	if (soundTable.haveAttr("helper"))
		m_pHelper = m_pVehicle->GetHelper(soundTable.getAttr("helper"));

	if (!m_pHelper)
		return false;

	m_soundId = INVALID_SOUNDID;
	m_enabled = false;
	return true;
}

//------------------------------------------------------------------------
void CVehicleSeatActionSound::Serialize(TSerialize ser, unsigned aspects)
{
	if (aspects&CVehicle::ASPECT_SEAT_ACTION)
	{
		bool enabled=m_enabled;
		ser.Value("enabled", enabled, 'bool');

		if (ser.IsReading() && m_enabled!=enabled)
			PlaySound(enabled);

		m_enabled=enabled;
	}
}

//------------------------------------------------------------------------
void CVehicleSeatActionSound::StopUsing()
{
	if (m_enabled)
		PlaySound(false);
}

//------------------------------------------------------------------------
void CVehicleSeatActionSound::OnAction(const TVehicleActionId actionId, int activationMode, float value)
{
	if (actionId == eVAI_Horn && activationMode == eAAM_OnPress)
	{
		PlaySound(true);
	}
	else if (actionId == eVAI_Horn && activationMode == eAAM_OnRelease)
	{
		PlaySound(false);
	}
}

//------------------------------------------------------------------------
void CVehicleSeatActionSound::PlaySound(bool play)
{
	if (play && m_soundId != INVALID_SOUNDID)
		PlaySound(false);

	if (play != m_enabled)
	{
		CVehicleSeat* pSeat = static_cast<CVehicleSeat*>(m_pVehicle->GetSeatById(m_seatId));
		if (pSeat)
			pSeat->ChangedNetworkState(CVehicle::ASPECT_SEAT_ACTION);
	}

	if (play)
	{
		IEntitySoundProxy* pSoundProxy = (IEntitySoundProxy*)m_pVehicle->GetEntity()->CreateProxy(ENTITY_PROXY_SOUND);
		assert(pSoundProxy);

		if (ISound* pSound = pSoundProxy->GetSound(m_soundId))
		{
			if (pSound->IsPlaying())
				return;
		}

		Vec3 pos = m_pHelper->GetVehicleTM().GetTranslation();
		m_soundId = pSoundProxy->PlaySound(m_soundName.c_str(), pos, Vec3(0.0f, -1.0f, 0.0f), FLAG_SOUND_3D | FLAG_SOUND_LOOP, eSoundSemantic_Vehicle);
		m_enabled = true;

		if (!gEnv->bMultiplayer && gEnv->pAISystem)
		{
			IAIObject* pAI = m_pVehicle->GetEntity() ? m_pVehicle->GetEntity()->GetAI() : 0;
			gEnv->pAISystem->SoundEvent(pos, 200.0f, AISE_MOVEMENT_LOUD, pAI);
		}
	}
	else
	{
		if (m_soundId != INVALID_SOUNDID)
		{
			ISound* pSound = gEnv->pSoundSystem->GetSound(m_soundId);
			if (pSound)
				pSound->Stop();
		}

		m_soundId = INVALID_SOUNDID;
		m_enabled = false;
	}
}

void CVehicleSeatActionSound::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	s->Add(m_soundName);
}

DEFINE_VEHICLEOBJECT(CVehicleSeatActionSound);
