/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:

-------------------------------------------------------------------------
History:
- 22:03:2006: Created by Mathieu Pinard

*************************************************************************/
#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CryAnimation/ICryAnimation.h"
#include "CryCommon/CryAction/IVehicleSystem.h"
#include "VehiclePartAnimated.h"
#include "VehicleAnimation.h"

//------------------------------------------------------------------------
CVehicleAnimation::CVehicleAnimation()
	: m_pPartAnimated(NULL)
{
}

//------------------------------------------------------------------------
bool CVehicleAnimation::Init(IVehicle* pVehicle, const CVehicleParams& table)
{
	m_currentAnimIsWaiting = false;

	if (table.haveAttr("part"))
	{
		if (IVehiclePart* pPart = pVehicle->GetPart(table.getAttr("part")))
			m_pPartAnimated = CAST_VEHICLEOBJECT(CVehiclePartAnimated, pPart);
	}

	if (!m_pPartAnimated)
		return false;

	if (CVehicleParams statesTable = table.findChild("States"))
	{
		int c = statesTable.getChildCount();
		int i = 0;

		m_animationStates.reserve(c);

		for (; i < c; i++)
		{
			if (CVehicleParams stateTable = statesTable.getChild(i))
				ParseState(stateTable, pVehicle);
		}
	}

	if (m_animationStates.size() > 0)
		m_currentStateId = 0;
	else
		m_currentStateId = InvalidVehicleAnimStateId;

	m_layerId = m_pPartAnimated->AssignAnimationLayer();

	return true;
}

//------------------------------------------------------------------------
bool CVehicleAnimation::ParseState(const CVehicleParams& table, IVehicle* pVehicle)
{
	m_animationStates.resize(m_animationStates.size() + 1);
	SAnimationState& animState = m_animationStates.back();

	animState.name = table.getAttr("name");
	animState.animation = table.getAttr("animation");
	animState.sound = table.getAttr("sound");
	animState.pSoundHelper = nullptr;

	if (table.haveAttr("sound"))
		animState.pSoundHelper = pVehicle->GetHelper(table.getAttr("sound"));
	else
		animState.pSoundHelper = nullptr;

	animState.soundId = INVALID_SOUNDID;

	table.getAttr("speedDefault", animState.speedDefault);
	table.getAttr("speedMin", animState.speedMin);
	table.getAttr("speedMax", animState.speedMax);
	table.getAttr("isLooped", animState.isLooped);
	animState.isLoopedEx = false;
	table.getAttr("isLoopedEx", animState.isLoopedEx);

	table.getAttr("timeRestartAnim", animState.timeRestartAnim);

	if (CVehicleParams materialsTable = table.findChild("Materials"))
	{
		int i = 0;
		int c = materialsTable.getChildCount();

		animState.materials.reserve(c);

		for (; i < c; i++)
		{
			if (CVehicleParams materialTable = materialsTable.getChild(i))
			{
				animState.materials.resize(animState.materials.size() + 1);
				SAnimationStateMaterial& stateMaterial = animState.materials.back();

				stateMaterial.material = materialTable.getAttr("name");
				stateMaterial.setting = materialTable.getAttr("setting");
				materialTable.getAttr("invertValue", stateMaterial.invertValue);
			}
		}
	}

	return true;
}

//------------------------------------------------------------------------
void CVehicleAnimation::Reset()
{
	if (m_currentStateId != InvalidVehicleAnimStateId)
	{
		const SAnimationState& animState = m_animationStates[m_currentStateId];

		if (IEntity* pEntity = m_pPartAnimated->GetEntity())
		{
			for (TAnimationStateMaterialVector::const_iterator ite = animState.materials.begin();
				ite != animState.materials.end(); ++ite)
			{
				const SAnimationStateMaterial& stateMaterial = *ite;

				if (IMaterial* pMaterial = FindMaterial(stateMaterial, m_pPartAnimated->GetMaterial()))
				{
					float value = max(0.00f, min(1.0f, animState.speedDefault));

					if (stateMaterial.invertValue)
						value = 1.0f - value;

					pMaterial->SetGetMaterialParamFloat(stateMaterial.setting.c_str(), value, false);
				}
			}
		}

		if (m_currentStateId != 0)
			ChangeState(0);
	}

	if (m_layerId > -1)
		StopAnimation();
}

//------------------------------------------------------------------------
bool CVehicleAnimation::StartAnimation()
{
	if (m_currentStateId == -1)
		m_currentStateId = 0;

	SAnimationState& animState = m_animationStates[m_currentStateId];

	if (animState.materials.size() > 0)
	{
		m_pPartAnimated->CloneMaterial();
	}

	if (IEntity* pEntity = m_pPartAnimated->GetEntity())
	{
		if (ICharacterInstance* pCharInstance = pEntity->GetCharacter(m_pPartAnimated->GetSlot()))
		{
			ISkeletonAnim* pSkeletonAnim = pCharInstance->GetISkeletonAnim();
			assert(pSkeletonAnim);

			CryCharAnimationParams animParams;
			animParams.m_nFlags = CA_FORCE_SKELETON_UPDATE;
			animParams.m_nLayerID = m_layerId;

			if (animState.isLooped)
			{
				animParams.m_nFlags |= CA_LOOP_ANIMATION;
			}

			if (animState.isLoopedEx)
			{
				animParams.m_nFlags |= CA_REPEAT_LAST_KEY;
				uint32 numAnimsLayer0 = pSkeletonAnim->GetNumAnimsInFIFO(animParams.m_nLayerID);
				if (numAnimsLayer0)
				{
					CAnimation& animation = pSkeletonAnim->GetAnimFromFIFO(animParams.m_nLayerID, numAnimsLayer0 - 1);
					if (animation.m_fAnimTime == 1.0f)
						animation.m_fAnimTime = 0.0f;
				}

			}

			// cope with empty animation names (for disabling some in certain vehicle modifications)
			if (animState.animation.empty())
				return false;

			if (!pSkeletonAnim->StartAnimation(animState.animation.c_str(), nullptr, nullptr, nullptr, animParams))
				return false;

			if (!animState.sound.empty())
			{
				IEntitySoundProxy* pEntitySoundsProxy = (IEntitySoundProxy*)pEntity->CreateProxy(ENTITY_PROXY_SOUND);
				assert(pEntitySoundsProxy);

				int soundFlags = FLAG_SOUND_DEFAULT_3D;
				if (animState.isLooped)
					soundFlags |= FLAG_SOUND_LOOP;

				Vec3 pos;
				if (animState.pSoundHelper)
					pos = animState.pSoundHelper->GetVehicleTM().GetTranslation();
				else
					pos.zero();

				animState.soundId = pEntitySoundsProxy->PlaySound(animState.sound.c_str(), pos, FORWARD_DIRECTION, soundFlags, eSoundSemantic_Vehicle);
			}

			pSkeletonAnim->SetLayerUpdateMultiplier(m_layerId, animState.speedDefault);
			return true;
		}
	}

	return true;
}

//------------------------------------------------------------------------
void CVehicleAnimation::StopAnimation()
{
	if (m_layerId < 0)
		return;

	if (IsUsingManualUpdates())
		SetTime(0.0f);

	IEntity* pEntity = m_pPartAnimated->GetEntity();
	SAnimationState& animState = m_animationStates[m_currentStateId];

	if (ICharacterInstance* pCharInstance = pEntity->GetCharacter(m_pPartAnimated->GetSlot()))
	{
		ISkeletonAnim* pSkeletonAnim = pCharInstance->GetISkeletonAnim();
		if (pSkeletonAnim && animState.timeRestartAnim <= 0.0f)
		{
			pSkeletonAnim->StopAnimationInLayer(m_layerId, 0.0f);
		}
	}

	if (animState.soundId != INVALID_SOUNDID)
	{
		IEntitySoundProxy* pEntitySoundsProxy = static_cast<IEntitySoundProxy*>(pEntity->CreateProxy(ENTITY_PROXY_SOUND));
		if (pEntitySoundsProxy)
		{
			pEntitySoundsProxy->StopSound(animState.soundId);
		}
		animState.soundId = INVALID_SOUNDID;
	}

	m_currentAnimIsWaiting = false;
}

//------------------------------------------------------------------------
bool CVehicleAnimation::PlaySound(TVehicleAnimStateId stateId)
{
	if (gEnv->pSystem->IsDedicated())
		return false;

	if (stateId <= InvalidVehicleAnimStateId || stateId > m_animationStates.size())
		return false;

	SAnimationState& animState = m_animationStates[stateId];

	if (animState.sound.empty())
		return false;

	IEntity* pEntity = m_pPartAnimated->GetEntity();
	IEntitySoundProxy* pEntitySoundsProxy = static_cast<IEntitySoundProxy*>(pEntity->CreateProxy(ENTITY_PROXY_SOUND));
	if (!pEntity || !pEntitySoundsProxy)
		return  false;

	for (SAnimationState& state : m_animationStates)
	{
		if (state.soundId != INVALID_SOUNDID)
		{
			ISound* pSound = pEntitySoundsProxy->GetSound(state.soundId);
			if (pSound && pSound->IsPlaying())
			{
				return false;
			}
			else
			{
				state.soundId = INVALID_SOUNDID;
			}
		}
	}

	int soundFlags = FLAG_SOUND_DEFAULT_3D;
	if (animState.isLooped)
	{
		soundFlags |= FLAG_SOUND_LOOP;
	}

	Vec3 pos;
	if (animState.pSoundHelper)
	{
		pos = animState.pSoundHelper->GetVehicleTM().GetTranslation();
	}
	else
	{
		pos.zero();
	}

	animState.soundId = pEntitySoundsProxy->PlaySound(animState.sound.c_str(), pos, FORWARD_DIRECTION, soundFlags, eSoundSemantic_Vehicle);

	//CryLogAlways("CVehicleAnimation playing sound %s", animState.sound.c_str());

	return true;
}

//------------------------------------------------------------------------
TVehicleAnimStateId CVehicleAnimation::GetState()
{
	return m_currentStateId;
}

//------------------------------------------------------------------------
bool CVehicleAnimation::ChangeState(TVehicleAnimStateId stateId)
{
	if (stateId <= InvalidVehicleAnimStateId || stateId > m_animationStates.size())
		return false;

	SAnimationState& animState = m_animationStates[stateId];

	m_currentStateId = stateId;

	if (IEntity* pEntity = m_pPartAnimated->GetEntity())
	{
		if (ICharacterInstance* pCharInstance = pEntity->GetCharacter(m_pPartAnimated->GetSlot()))
		{
			ISkeletonAnim* pSkeletonAnim = pCharInstance->GetISkeletonAnim();
			if (pSkeletonAnim && pSkeletonAnim->GetNumAnimsInFIFO(m_layerId) > 0)
			{
				CAnimation& anim = pSkeletonAnim->GetAnimFromFIFO(m_layerId, 0);
				if (anim.m_fAnimTime > 0.0f)
				{
					//float speed = pSkeletonAnim->GetLayerUpdateMultiplier(m_layerId) * -1.0f; //CryMP: commented
					//pSkeletonAnim->SetLayerUpdateMultiplier(m_layerId, speed);
				}
				else
					StopAnimation();
			}
			else
				StartAnimation();
		}
	}

	return true;
}

//------------------------------------------------------------------------
string CVehicleAnimation::GetStateName(TVehicleAnimStateId stateId)
{
	if (stateId <= InvalidVehicleAnimStateId || stateId > m_animationStates.size())
		return "";

	SAnimationState& animState = m_animationStates[stateId];
	return animState.name;
}

//------------------------------------------------------------------------
TVehicleAnimStateId CVehicleAnimation::GetStateId(const string& name)
{
	TVehicleAnimStateId stateId = 0;

	for (TAnimationStateVector::iterator ite = m_animationStates.begin(); ite != m_animationStates.end(); ++ite)
	{
		SAnimationState& animState = *ite;
		if (animState.name == name)
			return stateId;

		stateId++;
	}

	return InvalidVehicleAnimStateId;
}

//------------------------------------------------------------------------
void CVehicleAnimation::SetSpeed(float speed)
{
	if (m_currentStateId == InvalidVehicleAnimStateId)
		return;

	const SAnimationState& animState = m_animationStates[m_currentStateId];

	IEntity* pEntity = m_pPartAnimated->GetEntity();
	assert(pEntity);

	ICharacterInstance* pCharInstance = pEntity->GetCharacter(m_pPartAnimated->GetSlot());
	if (!pCharInstance)
		return;

	ISkeletonAnim* pSkeletonAnim = pCharInstance->GetISkeletonAnim();
	assert(pSkeletonAnim);

	pSkeletonAnim->SetLayerUpdateMultiplier(m_layerId, max(min(speed, animState.speedMax), animState.speedMin));

	//CryMP
	if (animState.timeRestartAnim > 0.0f && pSkeletonAnim->GetLayerTime(m_layerId) > animState.timeRestartAnim)
	{
		pSkeletonAnim->SetLayerTime(m_layerId, 0.0f);
	}

	for (TAnimationStateMaterialVector::const_iterator ite = animState.materials.begin();
		ite != animState.materials.end(); ++ite)
	{
		const SAnimationStateMaterial& stateMaterial = *ite;

		IMaterial* pPartMaterial = m_pPartAnimated->GetMaterial();
		if (pPartMaterial)
		{
			if (IMaterial* pMaterial = FindMaterial(stateMaterial, pPartMaterial))
			{
				float value;
				if (stateMaterial.invertValue)
					value = 1.0f - speed;
				else
					value = speed;

				value = max(0.001f, min(0.999f, value));

				pMaterial->SetGetMaterialParamFloat(stateMaterial.setting.c_str(), value, false);
			}
		}
	}
}

//------------------------------------------------------------------------
IMaterial* CVehicleAnimation::FindMaterial(const SAnimationStateMaterial& animStateMaterial, IMaterial* pMaterial)
{
	assert(pMaterial);
	if (!pMaterial)
		return NULL;

	if (animStateMaterial.material == pMaterial->GetName())
		return pMaterial;

	for (int i = 0; i < pMaterial->GetSubMtlCount(); i++)
	{
		if (IMaterial* pSubMaterial = pMaterial->GetSubMtl(i))
		{
			if (IMaterial* pFoundSubMaterial = FindMaterial(animStateMaterial, pSubMaterial))
				return pFoundSubMaterial;
		}
	}

	return NULL;
}

//------------------------------------------------------------------------
void CVehicleAnimation::ToggleManualUpdate(bool isEnabled)
{
	if (m_layerId < 0 || !m_pPartAnimated)
		return;

	if (ICharacterInstance* pCharInstance =
		m_pPartAnimated->GetEntity()->GetCharacter(m_pPartAnimated->GetSlot()))
	{
		ISkeletonAnim* pSkeletonAnim = pCharInstance->GetISkeletonAnim();
		assert(pSkeletonAnim);

		if (pSkeletonAnim->GetNumAnimsInFIFO(m_layerId) != 0)
		{
			CAnimation& animation = pSkeletonAnim->GetAnimFromFIFO(m_layerId, 0);

			if (isEnabled)
				animation.m_AnimParams.m_nFlags |= CA_MANUAL_UPDATE;
			else
				animation.m_AnimParams.m_nFlags &= ~CA_MANUAL_UPDATE;
		}
	}
}

//------------------------------------------------------------------------
float CVehicleAnimation::GetAnimTime(bool raw/*=false*/)
{
	if (m_layerId < 0 || !m_pPartAnimated)
		return 0.0f;

	if (ICharacterInstance* pCharInstance =
		m_pPartAnimated->GetEntity()->GetCharacter(m_pPartAnimated->GetSlot()))
	{
		ISkeletonAnim* pSkeletonAnim = pCharInstance->GetISkeletonAnim();
		assert(pSkeletonAnim);

		if (pSkeletonAnim->GetNumAnimsInFIFO(m_layerId) != 0)
		{
			CAnimation& anim = pSkeletonAnim->GetAnimFromFIFO(m_layerId, 0);

			if (!raw && anim.m_fAnimTime == 0.0f)
				return 1.0f;

			return max(0.0f, anim.m_fAnimTime);
		}
		else
		{
			return 1.0f;
		}
	}

	return 0.0f;
}

//------------------------------------------------------------------------
void CVehicleAnimation::SetTime(float time, bool force)
{
	if (m_layerId < 0 || !m_pPartAnimated)
		return;

	if (ICharacterInstance* pCharInstance =
		m_pPartAnimated->GetEntity()->GetCharacter(m_pPartAnimated->GetSlot()))
	{
		ISkeletonAnim* pSkeletonAnim = pCharInstance->GetISkeletonAnim();
		assert(pSkeletonAnim);

		if (pSkeletonAnim->GetNumAnimsInFIFO(m_layerId) > 0)
		{
			CAnimation& animation = pSkeletonAnim->GetAnimFromFIFO(m_layerId, 0);

			if (force)
				animation.m_AnimParams.m_nFlags |= CA_MANUAL_UPDATE;

			if (animation.m_AnimParams.m_nFlags & CA_MANUAL_UPDATE)
				animation.m_fAnimTime = time;

			assert(animation.m_fAnimTime >= 0.0f && animation.m_fAnimTime <= 1.0f);

			//else
				//CryLogAlways("Error: can't use SetTime on a VehicleAnimation that wasn't set for manual updates.");
		}
	}
}

//------------------------------------------------------------------------
bool CVehicleAnimation::IsUsingManualUpdates()
{
	if (ICharacterInstance* pCharInstance =
		m_pPartAnimated->GetEntity()->GetCharacter(m_pPartAnimated->GetSlot()))
	{
		ISkeletonAnim* pSkeletonAnim = pCharInstance->GetISkeletonAnim();
		assert(pSkeletonAnim);

		if (pSkeletonAnim->GetNumAnimsInFIFO(m_layerId) != 0)
		{
			CAnimation& animation = pSkeletonAnim->GetAnimFromFIFO(m_layerId, 0);
			if (animation.m_AnimParams.m_nFlags & CA_MANUAL_UPDATE)
				return true;
		}
	}

	return false;
}
