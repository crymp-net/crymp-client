/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 18:1:2007   15:17 : Created by Márcio Martins

*************************************************************************/
#include "CryCommon/CrySystem/ISystem.h"
#include "Scan.h"
#include "CryGame/Game.h"
#include "CryGame/Actors/Actor.h"
#include "CryGame/GameRules.h"
#include "CryCommon/CryEntitySystem/IEntityProxy.h"
#include "CryCommon/Cry3DEngine/IMaterial.h"
#include "CryGame/HUD/HUD.h"
#include "CryCommon/CrySystem/IFlashPlayer.h"

#include "CryCommon/CrySoundSystem/ISound.h"


//------------------------------------------------------------------------
CScan::CScan()
{
}

//------------------------------------------------------------------------
CScan::~CScan()
{
}

//------------------------------------------------------------------------
void CScan::Init(IWeapon *pWeapon, const struct IItemParamsNode *params)
{
	m_pWeapon = static_cast<CWeapon *>(pWeapon);

	if (params)
		ResetParams(params);
}

//------------------------------------------------------------------------
void CScan::Release()
{
	delete this;
}

//------------------------------------------------------------------------
void CScan::ResetParams(const struct IItemParamsNode *params)
{
	const IItemParamsNode *scan = params?params->GetChild("scan"):0;
	const IItemParamsNode *actions = params?params->GetChild("actions"):0;
	m_scanparams.Reset(scan);
	m_scanactions.Reset(actions);
}

//------------------------------------------------------------------------
void CScan::PatchParams(const struct IItemParamsNode *patch)
{
	const IItemParamsNode *scan = patch->GetChild("scan");
	const IItemParamsNode *actions = patch->GetChild("actions");
	m_scanparams.Reset(scan, false);
	m_scanactions.Reset(actions, false);
}

//------------------------------------------------------------------------
const char *CScan::GetType() const
{
	return "Scan";
}

//------------------------------------------------------------------------
void CScan::Activate(bool activate)
{
	m_scanning = false;

	KillTimers();

	StopScanSound();
}

//------------------------------------------------------------------------
void CScan::Update(float frameTime, unsigned int frameId)
{
}

//------------------------------------------------------------------------
struct CScan::DelayTimer
{
	DelayTimer(CScan* _scan) : pScan(_scan) {};
	CScan* pScan;

	void execute(CItem* _this)
	{
		CWeapon* pWeapon = pScan->m_pWeapon;
		int slot = pWeapon->GetStats().fp ? CItem::eIGS_FirstPerson : CItem::eIGS_ThirdPerson;
		int id = pWeapon->GetStats().fp ? 0 : 1;

		pScan->StopScanSound();

		pScan->m_scanSoundId = pWeapon->PlayAction(pScan->m_scanactions.scan, 0, true, CItem::eIPAF_Default | CItem::eIPAF_CleanBlending);

		ISound* pSound = pWeapon->GetSoundProxy()->GetSound(pScan->m_scanSoundId);
		if (pSound)
		{
			pSound->SetLoopMode(true);
		}
		pScan->StartTimers();
	}
};

//------------------------------------------------------------------------
struct CScan::TagEntitiesDelay
{
	TagEntitiesDelay(CScan* _scan) : pScan(_scan) {};
	CScan* pScan;

	void execute(CItem* _this)
	{
		CWeapon* pWeapon = pScan->m_pWeapon;
		//Here is when entities are displayed on Radar
		if (gEnv->bServer)
		{
			pScan->NetShoot(ZERO, 0);
		}
		else
		{
			pWeapon->RequestShoot(0, ZERO, ZERO, ZERO, ZERO, 1.0f, 0, 0, 0, false);
		}
	}
};

//------------------------------------------------------------------------
struct CScan::DurationTimer
{
	DurationTimer(CScan* _scan) : pScan(_scan) {};
	CScan* pScan;

	void execute(CItem* _this)
	{
		pScan->StopFire();
	}
};

//------------------------------------------------------------------------
void CScan::StartTimers()
{
	m_delayTimerId = m_pWeapon->GetScheduler()->TimerAction(static_cast<uint32>(m_scanparams.tagDelay * 1000.f),
		CSchedulerAction<TagEntitiesDelay>::Create(this), false);

	m_durationTimerId = m_pWeapon->GetScheduler()->TimerAction(static_cast<uint32>(m_scanparams.duration * 1000.f),
		CSchedulerAction<DurationTimer>::Create(this), false);
}

//------------------------------------------------------------------------
void CScan::KillTimers()
{
	m_pWeapon->GetScheduler()->KillTimer(m_delayTimerId);
	m_pWeapon->GetScheduler()->KillTimer(m_durationTimerId);
	m_pWeapon->GetScheduler()->KillTimer(m_scanTimerId);
}

//------------------------------------------------------------------------
void CScan::ShowFlashAnimation(bool enable)
{
	// add the flash animation part here
	IEntityRenderProxy* pRenderProxy = static_cast<IEntityRenderProxy*>(m_pWeapon->GetEntity()->GetProxy(ENTITY_PROXY_RENDER));
	if (!pRenderProxy)
		return;

	IMaterial* pMtl = pRenderProxy->GetRenderMaterial(0);
	pMtl = pMtl ? pMtl->GetSafeSubMtl(2) : nullptr;
	if (pMtl)
	{
		const SShaderItem& shaderItem = pMtl->GetShaderItem();
		if (shaderItem.m_pShaderResources)
		{
			SEfResTexture* pTex = shaderItem.m_pShaderResources->GetTexture(0);
			if (pTex->m_Sampler.m_pDynTexSource)
			{
				IFlashPlayer* pFlashPlayer(0);
				IDynTextureSource::EDynTextureSource type(IDynTextureSource::DTS_I_FLASHPLAYER);

				pTex->m_Sampler.m_pDynTexSource->GetDynTextureSource((void*&)pFlashPlayer, type);
				if (pFlashPlayer && type == IDynTextureSource::DTS_I_FLASHPLAYER)
				{
					if (enable)
					{
						pFlashPlayer->Invoke0("startScan");
					}
					else
					{
						pFlashPlayer->Invoke0("cancelScan");
					}
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CScan::StartFire()
{
	if (!m_pWeapon->IsBusy())
	{
		CActor* pOwner = m_pWeapon->GetOwnerActor();
		if (pOwner)
		{
			m_scanning=true;

			if (m_pWeapon->IsClient() && pOwner->IsClient())
			{
				ShowFlashAnimation(true);

				m_scanTimerId = m_pWeapon->GetScheduler()->TimerAction(static_cast<uint32>(m_scanparams.delay * 1000.f),
					CSchedulerAction<DelayTimer>::Create(this), false);
			}

			m_pWeapon->SetBusy(true);
		}

		m_pWeapon->RequestStartFire();
		m_pWeapon->PlayAction(m_scanactions.spin_up, 0, false, CItem::eIPAF_Default | CItem::eIPAF_CleanBlending);
	}
}

//------------------------------------------------------------------------
void CScan::StopFire()
{
	if (!m_scanning)
		return;

	ShowFlashAnimation(false);

	m_pWeapon->PlayAction(m_scanactions.spin_down, 0, false, CItem::eIPAF_Default|CItem::eIPAF_CleanBlending);

	m_scanning=false;
	m_pWeapon->SetBusy(false);
	m_pWeapon->RequestStopFire();

	KillTimers();

	StopScanSound();
}

//------------------------------------------------------------------------
void CScan::NetStartFire()
{
	if (!m_pWeapon->IsClient())
		return;

	CActor* pOwner = m_pWeapon->GetOwnerActor();
	if (pOwner)
	{
		m_scanning = true;

		if (pOwner->IsFpSpectatorTarget()) //CryMP: Fp spec support
		{
			ShowFlashAnimation(true);

			m_pWeapon->PlayAction(m_scanactions.spin_up, 0, false, CItem::eIPAF_Default | CItem::eIPAF_CleanBlending);
		}

		StopScanSound();

		m_scanSoundId = m_pWeapon->PlayAction(m_scanactions.scan);

		ISound* pSound = m_pWeapon->GetSoundProxy()->GetSound(m_scanSoundId);
		if (pSound)
		{
			pSound->SetLoopMode(true);
		}
	}
}

//------------------------------------------------------------------------
void CScan::NetStopFire() 
{	
	if (!m_pWeapon->IsClient())
		return;

	StopScanSound();

	m_scanning = false;

	CActor* pOwner = m_pWeapon->GetOwnerActor();
	if (pOwner)
	{
		if (pOwner && pOwner->IsFpSpectatorTarget()) //CryMP: Fp spec support
		{
			ShowFlashAnimation(false);

			m_pWeapon->PlayAction(m_scanactions.spin_down, 0, false, CItem::eIPAF_Default | CItem::eIPAF_CleanBlending);
		}
	}
}

//------------------------------------------------------------------------
void CScan::NetShoot(const Vec3 &hit, int ph)
{
	if (m_pWeapon->IsServer())
	{
		IEntity *pOwner=m_pWeapon->GetOwner();
		EntityId ownerId=pOwner->GetId();

		SEntityProximityQuery query;
		float radius=m_scanparams.range;
		Vec3 pos=pOwner->GetWorldPos();
		query.box = AABB(Vec3(pos.x-radius,pos.y-radius,pos.z-radius), Vec3(pos.x+radius,pos.y+radius,pos.z+radius));
		query.nEntityFlags = ENTITY_FLAG_ON_RADAR; // Filter by entity flag.
		gEnv->pEntitySystem->QueryProximity( query );

		for(int i=0; i<query.nCount; i++)
		{
			IEntity *pEntity = query.pEntities[i];
			if(pEntity && pEntity != pOwner && !pEntity->IsHidden())
			{
				CActor *pActor=m_pWeapon->GetActor(pEntity->GetId());
				if (pActor && (pActor->GetSpectatorMode()!=0 || pActor->GetHealth()<=0.0f))
					continue;

/*				if (pActor && pActor->GetActorClass()==CPlayer::GetActorClassType())
				{
					CPlayer *pPlayer=static_cast<CPlayer *>(pActor);
					CNanoSuit *pSuit=pPlayer->GetNanoSuit();
					if (pSuit && pSuit->GetMode()==NANOMODE_CLOAK && pSuit->GetCloak()->GetType()==CLOAKMODE_REFRACTION)
						continue;
				}
				else*/ if(IItem *pItem = g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(pEntity->GetId()))
					continue;

				g_pGame->GetGameRules()->AddTaggedEntity(ownerId, pEntity->GetId(), true);
			}
		}
	}
}

//------------------------------------------------------------------------
bool CScan::IsClientScanning() const
{
	CPlayer* pClientActor = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetClientActor());
	if (pClientActor)
	{
		CWeapon* pWeapon = pClientActor->GetCurrentWeapon(false);
		if (pWeapon && pWeapon->GetEntity()->GetClass() == CItem::sRadarKitClass)
		{
			CScan* pScan = static_cast<CScan*>(pWeapon->GetActiveFireMode());
			if (pScan && pScan->IsScanning())
			{
				return true;
			}
		}
	}
	return false;
}

//------------------------------------------------------------------------
void CScan::OnEnterFirstPerson()
{
	if (!m_scanning) //Only if scanning active...
		return;

	ShowFlashAnimation(true);

	StopScanSound();

	//CryMP: We might not be First person untill next frame, so need to force FP with flag
	m_scanSoundId = m_pWeapon->PlayAction(m_scanactions.scan, 0, true, 
		CItem::eIPAF_ForceFirstPerson | CItem::eIPAF_Default | CItem::eIPAF_CleanBlending);

	ISound* pSound = m_pWeapon->GetSoundProxy()->GetSound(m_scanSoundId);
	if (pSound)
	{
		pSound->SetLoopMode(true);
	}
}

//------------------------------------------------------------------------
void CScan::OnEnterThirdPerson()
{
	if (!m_scanning) //Only if scanning active...
		return;

	ShowFlashAnimation(false);

	StopScanSound();

	m_scanSoundId = m_pWeapon->PlayAction(m_scanactions.scan, 0, true, 
		CItem::eIPAF_ForceThirdPerson | CItem::eIPAF_Default | CItem::eIPAF_CleanBlending);

	ISound* pSound = m_pWeapon->GetSoundProxy()->GetSound(m_scanSoundId);
	if (pSound)
	{
		pSound->SetLoopMode(true);
	}
}

//------------------------------------------------------------------------
void CScan::StopScanSound()
{
	if (m_scanSoundId != INVALID_SOUNDID)
	{
		m_pWeapon->StopSound(m_scanSoundId);
		m_scanSoundId = INVALID_SOUNDID;
	}
}

//------------------------------------------------------------------------
void CScan::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(m_name);
	m_scanparams.GetMemoryStatistics(s);
	m_scanactions.GetMemoryStatistics(s);
}
