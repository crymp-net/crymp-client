/*************************************************************************
	Crytek Source File.
	Copyright (C), Crytek Studios, 2001-2004.
	-------------------------------------------------------------------------
	$Id$
	$DateTime$

	-------------------------------------------------------------------------
	History:
	- 15:2:2006   12:50 : Created by Márcio Martins

*************************************************************************/
#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CryAction/IGameplayRecorder.h"
#include "Weapon.h"
#include "CryGame/Actors/Actor.h"
#include "CryGame/Game.h"
#include "CryGame/GameRules.h"

/*
#define CHECK_OWNER_REQUEST()	\
	{ \
		uint16 channelId=m_pGameFramework->GetGameChannelId(pNetChannel);	\
		IActor *pOwnerActor=GetOwnerActor(); \
		if (!pOwnerActor || pOwnerActor->GetChannelId()!=channelId) \
		{ \
			CryLogAlways("[gamenet] Disconnecting %s. Bogus weapon action '%s' request! %s %d!=%d (%s!=%s)", \
			pNetChannel->GetName(), __FUNCTION__, pOwnerActor?pOwnerActor->GetEntity()->GetName():"null", \
			pOwnerActor?pOwnerActor->GetChannelId():0, channelId,\
			pOwnerActor?pOwnerActor->GetEntity()->GetName():"null", \
			m_pGameFramework->GetIActorSystem()->GetActorByChannelId(channelId)?m_pGameFramework->GetIActorSystem()->GetActorByChannelId(channelId)->GetEntity()->GetName():"null"); \

			return false; \
		} \
	} \
*/

#define CHECK_OWNER_REQUEST()	\
	{ \
		uint16 channelId=m_pGameFramework->GetGameChannelId(pNetChannel);	\
		IActor *pOwnerActor=GetOwnerActor(); \
		if (pOwnerActor && pOwnerActor->GetChannelId()!=channelId && !IsDemoPlayback()) \
			return true; \
	}

//------------------------------------------------------------------------
int CWeapon::NetGetCurrentAmmoCount() const
{
	if (!m_fm)
		return 0;

	return GetAmmoCount(m_fm->GetAmmoType());
}

//------------------------------------------------------------------------
void CWeapon::NetSetCurrentAmmoCount(int count)
{
	if (!m_fm)
		return;

	SetAmmoCount(m_fm->GetAmmoType(), count);
}

//------------------------------------------------------------------------
void CWeapon::NetShoot(const Vec3 &hit, int predictionHandle)
{
	if (m_fm)
		m_fm->NetShoot(hit, predictionHandle);
}

//------------------------------------------------------------------------
void CWeapon::NetShootEx(const Vec3 &pos, const Vec3 &dir, const Vec3 &vel, const Vec3 &hit, float extra, int predictionHandle)
{
	if (m_fm)
		m_fm->NetShootEx(pos, dir, vel, hit, extra, predictionHandle);
}

//------------------------------------------------------------------------
void CWeapon::NetStartFire()
{
	if (m_fm)
		m_fm->NetStartFire();
}

//------------------------------------------------------------------------
void CWeapon::NetStopFire()
{
	if (m_fm)
		m_fm->NetStopFire();
}

//------------------------------------------------------------------------
void CWeapon::NetStartMeleeAttack(bool weaponMelee)
{
	if (weaponMelee && m_melee)
		m_melee->NetStartFire();
	else if (m_fm)
		m_fm->NetStartFire();
}

//------------------------------------------------------------------------
void CWeapon::NetMeleeAttack(bool weaponMelee, const Vec3 &pos, const Vec3 &dir)
{
	if (weaponMelee && m_melee)
	{
		m_melee->NetShootEx(pos, dir, ZERO, ZERO, 1.0f, 0);
		if (IsServer())
		{
			void* extra = reinterpret_cast<void*>(static_cast<uintptr_t>(GetEntityId()));
			m_pGameplayRecorder->Event(GetOwner(), GameplayEvent(eGE_WeaponMelee, 0, 0, extra));
		}
	}
	else if (m_fm)
	{
		m_fm->NetShootEx(pos, dir, ZERO, ZERO, 1.0f, 0);
		if (IsServer())
		{
			void* extra = reinterpret_cast<void*>(static_cast<uintptr_t>(GetEntityId()));
			m_pGameplayRecorder->Event(GetOwner(), GameplayEvent(eGE_WeaponMelee, 0, 0, extra));
		}
	}
}

//------------------------------------------------------------------------
int CWeapon::GetZoomStepFromFoV(float fov) 
{
	if (fov > 0.99f)
		return 0;

	int zoomStep = fov < 0.12f ? 2 : 1;
	if (GetEntity()->GetClass() == CItem::sBinocularsClass)
	{
		//Binocs have 3 steps..
		zoomStep = fov < 0.13f ? 3 : 2;
		zoomStep = fov > 0.49f ? 1 : zoomStep;
	}
	return zoomStep;
}

//------------------------------------------------------------------------
void CWeapon::NetZoom(float fov)
{
	if (CActor *pOwner=GetOwnerActor())
	{
		if (pOwner->IsClient())
			return;

		SActorParams* pActorParams = pOwner->GetActorParams();
		if (!pActorParams)
			return;

		const float previousFoV = pActorParams->zoomFoV;
		pActorParams->zoomFoV = fov;

		const bool pFpTarget = pOwner->IsFpSpectatorTarget();
		if (!pFpTarget)
			return;

		if (m_zm)
		{
			if (fov > 0.99f)
			{
				ExitZoom();
			}
			else
			{
				const int currentZoomStep = GetZoomStepFromFoV(previousFoV);
				const int newZoomStep = GetZoomStepFromFoV(fov);
				const int diff = abs(currentZoomStep - newZoomStep);

				//CryLogAlways("CurrentZoomStep: %i, NewZoomStep: %i, Diff %i", currentZoomStep, newZoomStep, diff);

				if (!diff)
					return;

				const bool zoomed = IsZoomed();
				if (!zoomed)
				{
					StartZoom(GetEntityId(), 1);
					if (diff < 2)
						return;
				}

				//Weapons with more than 1 step
				if (diff)
				{
					//are we zooming in or out
					const bool zoomIn = previousFoV > fov;
					for (int i = 0, n = diff; i < n; ++i)
					{
						if (zoomIn)
							m_zm->ZoomIn();
						else
							m_zm->ZoomOut();
					}
				}
				
			}
		}
	}
}

//------------------------------------------------------------------------
void CWeapon::RequestShoot(IEntityClass* pAmmoType, const Vec3 &pos, const Vec3 &dir, const Vec3 &vel, const Vec3 &hit, float extra, int predictionHandle, uint16 seq, uint8 seqr, bool forceExtended)
{
	CActor* pOwner = GetOwnerActor();

	if ((!pOwner || pOwner->IsClient()) && IsClient())
	{
		if (pOwner)
			pOwner->GetGameObject()->Pulse('bang');

		GetGameObject()->Pulse('bang');

		if (pOwner)
		{
			if (IsServerSpawn(pAmmoType) || forceExtended)
				GetGameObject()->InvokeRMI(CWeapon::SvRequestShootEx(), SvRequestShootExParams(pos, dir, vel, hit, extra, predictionHandle, seq, seqr), eRMI_ToServer);
			else
				GetGameObject()->InvokeRMI(CWeapon::SvRequestShoot(), SvRequestShootParams(pos, dir, hit, predictionHandle, seq, seqr), eRMI_ToServer);
		}
	}
	else if (!IsClient() && IsServer())
	{
		if (IsServerSpawn(pAmmoType) || forceExtended)
		{
			GetGameObject()->InvokeRMI(CWeapon::ClShoot(), ClShootParams(pos+dir*5.0f, predictionHandle), eRMI_ToAllClients);
			NetShootEx(pos, dir, vel, hit, extra, predictionHandle);
		}
		else
		{
			GetGameObject()->InvokeRMI(CWeapon::ClShoot(), ClShootParams(hit, predictionHandle), eRMI_ToAllClients);
			NetShoot(hit, predictionHandle);
		}
	}
}

//------------------------------------------------------------------------
void CWeapon::RequestMeleeAttack(bool weaponMelee, const Vec3 &pos, const Vec3 &dir, uint16 seq)
{
	CActor* pOwner = GetOwnerActor();
	if ((!pOwner || pOwner->IsClient()) && IsClient())
	{
		if (pOwner)
		{
			GetGameObject()->InvokeRMI(CWeapon::SvRequestMeleeAttack(), RequestMeleeAttackParams(weaponMelee, pos, dir, seq), eRMI_ToServer);
		}
	}
	else if (!IsClient() && IsServer())
	{
		GetGameObject()->InvokeRMI(CWeapon::ClMeleeAttack(), ClMeleeAttackParams(weaponMelee, pos, dir), eRMI_ToAllClients);
		NetMeleeAttack(weaponMelee, pos, dir);
	}
}

//------------------------------------------------------------------------
void CWeapon::RequestStartFire()
{
	CActor* pOwner = GetOwnerActor();
	if ((!pOwner || pOwner->IsClient()) && IsClient())
	{
		if (pOwner)
		{
			GetGameObject()->InvokeRMI(CWeapon::SvRequestStartFire(), EmptyParams(), eRMI_ToServer);
		}
	}
	else if (!IsClient() && IsServer())
	{
		GetGameObject()->InvokeRMI(CWeapon::ClStartFire(), EmptyParams(), eRMI_ToAllClients);
	}
}

//------------------------------------------------------------------------
void CWeapon::RequestStartMeleeAttack(bool weaponMelee)
{
	CActor* pOwner = GetOwnerActor();
	if ((!pOwner || pOwner->IsClient()) && IsClient())
	{
		if (pOwner)
		{
			GetGameObject()->InvokeRMI(CWeapon::SvRequestStartMeleeAttack(), RequestStartMeleeAttackParams(weaponMelee), eRMI_ToServer);
		}
	}
	else if (!IsClient() && IsServer())
	{
		GetGameObject()->InvokeRMI(CWeapon::ClStartMeleeAttack(), RequestStartMeleeAttackParams(weaponMelee), eRMI_ToAllClients);
		NetStartMeleeAttack(weaponMelee);
	}
}

//------------------------------------------------------------------------
void CWeapon::RequestZoom(float fov)
{
	CActor* pOwner = GetOwnerActor();
	if ((!pOwner || pOwner->IsClient()) && IsClient())
	{
		if (pOwner)
		{
			GetGameObject()->InvokeRMI(CWeapon::SvRequestZoom(), ZoomParams(fov), eRMI_ToServer);
		}
	}
	else if (!IsClient() && IsServer())
	{
		GetGameObject()->InvokeRMI(CWeapon::ClZoom(), ZoomParams(fov), eRMI_ToAllClients);
		NetZoom(fov);
	}
}


//------------------------------------------------------------------------
void CWeapon::RequestStopFire()
{
	CActor* pOwner = GetOwnerActor();
	if ((!pOwner || pOwner->IsClient()) && IsClient())
	{
		if (pOwner)
		{
			GetGameObject()->InvokeRMI(CWeapon::SvRequestStopFire(), EmptyParams(), eRMI_ToServer);
		}
	}

	else if (!IsClient() && IsServer())
		GetGameObject()->InvokeRMI(CWeapon::ClStopFire(), EmptyParams(), eRMI_ToAllClients);
}

//------------------------------------------------------------------------
void CWeapon::RequestReload()
{
	CActor* pOwner = GetOwnerActor();
	if ((!pOwner || pOwner->IsClient()) && IsClient())
	{
		if (pOwner)
		{
			GetGameObject()->InvokeRMI(SvRequestReload(), EmptyParams(), eRMI_ToServer);
		}
	}
	else if (!IsClient() && IsServer())
		GetGameObject()->InvokeRMI(CWeapon::ClReload(), EmptyParams(), eRMI_ToAllClients);
}

//-----------------------------------------------------------------------
void CWeapon::RequestCancelReload()
{
	CActor* pOwner = GetOwnerActor();
	if ((!pOwner || pOwner->IsClient()) && IsClient())
	{
		if (pOwner)
		{
			GetGameObject()->InvokeRMI(SvRequestCancelReload(), EmptyParams(), eRMI_ToServer);
		}
	}
	else if (!IsClient() && IsServer())
	{
		GetGameObject()->InvokeRMI(CWeapon::ClCancelReload(), EmptyParams(), eRMI_ToAllClients);
	}
}

//------------------------------------------------------------------------
void CWeapon::RequestFireMode(int fmId)
{
	CActor* pOwner = GetOwnerActor();
	if (!pOwner || pOwner->IsClient())
	{
		if (gEnv->bServer)
			SetCurrentFireMode(fmId);	// serialization will fix the rest.
		else
		{
			if (pOwner)
			{
				GetGameObject()->InvokeRMI(SvRequestFireMode(), SvRequestFireModeParams(fmId), eRMI_ToServer);
			}
		}
	}
}

//------------------------------------------------------------------------
void CWeapon::RequestLock(EntityId id, int partId)
{
	CActor* pOwner = GetOwnerActor();
	if (!pOwner || pOwner->IsClient())
	{
		if (gEnv->bServer)
		{
			if (m_fm)
				m_fm->Lock(id, partId);

			GetGameObject()->InvokeRMI(CWeapon::ClLock(), LockParams(id, partId), eRMI_ToRemoteClients);
		}
		else
		{
			if (pOwner)
			{
				GetGameObject()->InvokeRMI(SvRequestLock(), LockParams(id, partId), eRMI_ToServer);
			}
		}
	}
}

//------------------------------------------------------------------------
void CWeapon::RequestUnlock()
{
	CActor* pOwner = GetOwnerActor();
	if (!pOwner || pOwner->IsClient())
	{
		if (pOwner)
		{
			GetGameObject()->InvokeRMI(SvRequestUnlock(), EmptyParams(), eRMI_ToServer);
		}
	}
}

//------------------------------------------------------------------------
void CWeapon::RequestWeaponRaised(bool raise)
{
	if(gEnv->bMultiplayer)
	{
		CActor* pOwner = GetOwnerActor();
		if(pOwner && pOwner->IsClient())
		{
			if (gEnv->bServer)
				GetGameObject()->InvokeRMI(ClWeaponRaised(), WeaponRaiseParams(raise), eRMI_ToRemoteClients|eRMI_NoLocalCalls);
			else
				GetGameObject()->InvokeRMI(SvRequestWeaponRaised(), WeaponRaiseParams(raise), eRMI_ToServer);
		}
	}
}

//------------------------------------------------------------------------
void CWeapon::SendEndReload()
{
	int channelId=0;
	if (CActor* pActor = GetOwnerActor())
		channelId=pActor->GetChannelId();

	GetGameObject()->InvokeRMI(ClEndReload(), EmptyParams(), eRMI_ToClientChannel|eRMI_NoLocalCalls, channelId);
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, SvRequestStartFire)
{
	CHECK_OWNER_REQUEST();

	GetGameObject()->InvokeRMI(CWeapon::ClStartFire(), params, eRMI_ToOtherClients|eRMI_NoLocalCalls, 
		m_pGameFramework->GetGameChannelId(pNetChannel));

	CActor *pActor=GetActorByNetChannel(pNetChannel);
	IActor *pLocalActor=m_pGameFramework->GetClientActor();
	bool isLocal = pLocalActor && pActor && (pLocalActor->GetChannelId() == pActor->GetChannelId());

	if (!isLocal)
		NetStartFire();

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, SvRequestStopFire)
{
	CHECK_OWNER_REQUEST();

	GetGameObject()->InvokeRMI(CWeapon::ClStopFire(), params, eRMI_ToOtherClients|eRMI_NoLocalCalls, 
		m_pGameFramework->GetGameChannelId(pNetChannel));

	CActor *pActor=GetActorByNetChannel(pNetChannel);
	IActor *pLocalActor=m_pGameFramework->GetClientActor();
	bool isLocal = pLocalActor && pActor && (pLocalActor->GetChannelId() == pActor->GetChannelId());

	if (!isLocal)
		NetStopFire();

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, ClStartFire)
{
	NetStartFire();

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, ClStopFire)
{
	NetStopFire();

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, SvRequestShoot)
{
	CHECK_OWNER_REQUEST();

	bool ok=true;
	CActor *pActor=GetActorByNetChannel(pNetChannel);
	if (!pActor || pActor->GetHealth()<=0)
		ok=false;

	ok &= !OutOfAmmo(false);

	if (ok)
	{
		if (pActor)
			pActor->GetGameObject()->Pulse('bang');
		GetGameObject()->Pulse('bang');

		//CryMP : Server side accuracy fix for First person spectator 
		GetGameObject()->InvokeRMI(CWeapon::ClShoot(), ClShootParams(params.hit, params.predictionHandle), 
			eRMI_ToOtherClients|eRMI_NoLocalCalls, m_pGameFramework->GetGameChannelId(pNetChannel));

		IActor *pLocalActor=m_pGameFramework->GetClientActor();
		bool isLocal = pLocalActor && (pLocalActor->GetChannelId() == pActor->GetChannelId());

		if (!isLocal)
			NetShoot(params.hit, params.predictionHandle);

		if (pActor && !isLocal && params.seq)
		{
			if (CGameRules *pGameRules=g_pGame->GetGameRules())
				pGameRules->ValidateShot(pActor->GetEntityId(), GetEntityId(), params.seq, params.seqr);
		}
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, SvRequestShootEx)
{
	CHECK_OWNER_REQUEST();

	bool ok=true;
	CActor *pActor=GetActorByNetChannel(pNetChannel);
	if (!pActor || pActor->GetHealth()<=0)
		ok=false;

	ok &= !OutOfAmmo(false);

	if (ok)
	{
		if (pActor)
			pActor->GetGameObject()->Pulse('bang');
		GetGameObject()->Pulse('bang');

		GetGameObject()->InvokeRMI(CWeapon::ClShoot(), ClShootParams(params.pos+params.dir*5.0f, params.predictionHandle),
			eRMI_ToOtherClients|eRMI_NoLocalCalls, m_pGameFramework->GetGameChannelId(pNetChannel));

		IActor *pLocalActor=m_pGameFramework->GetClientActor();
		bool isLocal = pLocalActor && (pLocalActor->GetChannelId() == pActor->GetChannelId());

		if (!isLocal)
			NetShootEx(params.pos, params.dir, params.vel, params.hit, params.extra, params.predictionHandle);

		if (pActor && !isLocal && params.seq)
		{
			if (CGameRules *pGameRules=g_pGame->GetGameRules())
				pGameRules->ValidateShot(pActor->GetEntityId(), GetEntityId(), params.seq, params.seqr);
		}
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, ClShoot)
{
	NetShoot(params.hit, params.predictionHandle);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, ClShootX)
{
	if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(params.eid))
	{
		Vec3 hit = pEntity->GetWorldTM() * params.hit;
		NetShoot(hit, params.predictionHandle);
	}
	else
	{
		CryLogWarning("ClShootX: invalid entity id %.8x", params.eid);
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, SvRequestStartMeleeAttack)
{
	CHECK_OWNER_REQUEST();

	GetGameObject()->InvokeRMI(CWeapon::ClStartMeleeAttack(), params, eRMI_ToOtherClients, 
		m_pGameFramework->GetGameChannelId(pNetChannel));

	CActor *pActor=GetActorByNetChannel(pNetChannel);
	IActor *pLocalActor=m_pGameFramework->GetClientActor();
	bool isLocal = pLocalActor && pActor && (pLocalActor->GetChannelId() == pActor->GetChannelId());

	if (!isLocal)
		NetStartMeleeAttack(params.wmelee);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, ClStartMeleeAttack)
{
	NetStartMeleeAttack(params.wmelee);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, SvRequestMeleeAttack)
{
	CHECK_OWNER_REQUEST();

	bool ok=true;
	CActor *pActor=GetActorByNetChannel(pNetChannel);
	if (pActor && pActor->GetHealth()<=0)
		ok=false;

	if (ok)
	{
		GetGameObject()->InvokeRMI(CWeapon::ClMeleeAttack(), ClMeleeAttackParams(params.wmelee, params.pos, params.dir),
			eRMI_ToOtherClients|eRMI_NoLocalCalls, m_pGameFramework->GetGameChannelId(pNetChannel));

		IActor *pLocalActor=m_pGameFramework->GetClientActor();
		bool isLocal = pLocalActor && pActor && (pLocalActor->GetChannelId() == pActor->GetChannelId());

		if (!isLocal)
			NetMeleeAttack(params.wmelee, params.pos, params.dir);

		if (pActor && !isLocal && params.seq)
		{
			if (CGameRules *pGameRules=g_pGame->GetGameRules())
				pGameRules->ValidateShot(pActor->GetEntityId(), GetEntityId(), params.seq, 0);
		}

		void* extra = reinterpret_cast<void*>(static_cast<uintptr_t>(GetEntityId()));
		m_pGameplayRecorder->Event(GetOwner(), GameplayEvent(eGE_WeaponMelee, 0, 0, extra));
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, ClMeleeAttack)
{
	NetMeleeAttack(params.wmelee, params.pos, params.dir);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, SvRequestZoom)
{
	CHECK_OWNER_REQUEST();

	bool ok=true;
	CActor *pActor=GetActorByNetChannel(pNetChannel);
	if (!pActor || pActor->GetHealth()<=0)
		ok=false;

	if (ok)
	{
		GetGameObject()->InvokeRMI(CWeapon::ClZoom(), params,
			eRMI_ToOtherClients|eRMI_NoLocalCalls, m_pGameFramework->GetGameChannelId(pNetChannel));

		IActor *pLocalActor=m_pGameFramework->GetClientActor();
		bool isLocal = pLocalActor && (pLocalActor->GetChannelId() == pActor->GetChannelId());

		if (!isLocal)
			NetZoom(params.fov);

		int event=eGE_ZoomedOut;
		if (params.fov<0.99f)
			event=eGE_ZoomedIn;

		void* extra = reinterpret_cast<void*>(static_cast<uintptr_t>(GetEntityId()));
		m_pGameplayRecorder->Event(GetOwner(), GameplayEvent(event, 0, 0, extra));
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, ClZoom)
{
	NetZoom(params.fov);

	return true;
}


//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, SvRequestFireMode)
{
	CHECK_OWNER_REQUEST();

	SetCurrentFireMode(params.id);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, ClSetFireMode)
{
	SetCurrentFireMode(params.id);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, SvRequestReload)
{
	CHECK_OWNER_REQUEST();
	
	bool ok=true;
	CActor *pActor=GetActorByNetChannel(pNetChannel);
	if (!pActor || pActor->GetHealth()<=0)
		ok=false;

	if (ok)
	{
		GetGameObject()->InvokeRMI(CWeapon::ClReload(), params, eRMI_ToOtherClients|eRMI_NoLocalCalls, m_pGameFramework->GetGameChannelId(pNetChannel));

		IActor *pLocalActor=m_pGameFramework->GetClientActor();
		bool isLocal = pLocalActor && (pLocalActor->GetChannelId() == pActor->GetChannelId());

		if (!isLocal && m_fm)
			m_fm->Reload(0);

		void* extra = reinterpret_cast<void*>(static_cast<uintptr_t>(GetEntityId()));
		m_pGameplayRecorder->Event(GetOwner(), GameplayEvent(eGE_WeaponReload, 0, 0, extra));
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, ClReload)
{
	if (m_fm)
	{
		if(m_zm)
			m_fm->Reload(m_zm->GetCurrentStep());
		else
			m_fm->Reload(false);
	}
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, ClEndReload)
{
	if(m_fm)
		m_fm->NetEndReload();

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, SvRequestCancelReload)
{
	CHECK_OWNER_REQUEST();

	if(m_fm)
	{
		m_fm->CancelReload();
		GetGameObject()->InvokeRMI(CWeapon::ClCancelReload(), params, eRMI_ToRemoteClients);
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, ClCancelReload)
{
	if(m_fm)
		m_fm->CancelReload();

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, ClLock)
{
	if (m_fm)
		m_fm->Lock(params.entityId, params.partId);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, ClUnlock)
{
	if (m_fm)
		m_fm->Unlock();

	return true;
}


//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, SvRequestLock)
{
	CHECK_OWNER_REQUEST();

	if (m_fm)
		m_fm->Lock(params.entityId, params.partId);

	GetGameObject()->InvokeRMI(CWeapon::ClLock(), params, eRMI_ToRemoteClients);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, SvRequestUnlock)
{
	CHECK_OWNER_REQUEST();

	if (m_fm)
		m_fm->Unlock();

	GetGameObject()->InvokeRMI(CWeapon::ClUnlock(), params, eRMI_ToRemoteClients);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, SvRequestWeaponRaised)
{
	CHECK_OWNER_REQUEST();

	GetGameObject()->InvokeRMI(CWeapon::ClWeaponRaised(), params, eRMI_ToAllClients);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, ClWeaponRaised)
{
	CActor* pActor = GetOwnerActor();
	if(pActor && !pActor->IsClient())
		RaiseWeapon(params.raise);

	return true;
}

