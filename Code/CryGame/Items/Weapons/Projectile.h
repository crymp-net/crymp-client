/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Projectile

-------------------------------------------------------------------------
History:
- 12:10:2005   11:15 : Created by Márcio Martins

*************************************************************************/
#ifndef __PROJECTILE_H__
#define __PROJECTILE_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include "CryCommon/CryAction/IGameObject.h"
#include "CryCommon/CryAction/IGameRulesSystem.h"
#include "CryCommon/CryAction/IItemSystem.h"
#include "../ItemParamReader.h"
#include "TracerManager.h"
#include "Weapon.h"
#include "AmmoParams.h"


#define MIN_DAMAGE								5

class CProjectile :
	public CGameObjectExtensionHelper<CProjectile, IGameObjectExtension>,
	public IHitListener,	public IGameObjectProfileManager
{
public:
	enum ProjectileTimer
	{
		ePTIMER_LIFETIME	= 0x110,
		ePTIMER_SHOWTIME	= 0x111,
		ePTIMER_STICKY		= 0x112,
		ePTIMER_ACTIVATION= 0x113, // used by EMPField
	};

	CProjectile();
	virtual ~CProjectile();

	// IGameObjectExtension
	virtual bool Init(IGameObject *pGameObject);
	virtual void InitClient(int channelId) {};
	virtual void PostInit(IGameObject *pGameObject);
	virtual void PostInitClient(int channelId) {};
	virtual void Release();
	virtual void FullSerialize( TSerialize ser );
	bool RemoveIfExpired();
	virtual bool NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags );
	virtual void PostSerialize();
	virtual void SerializeSpawnInfo( TSerialize ser );
	virtual ISerializableInfoPtr GetSpawnInfo();
	virtual void Update( SEntityUpdateContext &ctx, int updateSlot);
	virtual void PostUpdate(float frameTime ) {};
	virtual void PostRemoteSpawn();
	virtual void HandleEvent( const SGameObjectEvent &);
	virtual void ProcessEvent(SEntityEvent &);
	virtual void SetChannelId(uint16 id) {};
	virtual void SetAuthority(bool auth);
	virtual void GetMemoryStatistics(ICrySizer *);
	virtual int  GetMemorySize() { return sizeof(*this); };
	//~IGameObjectExtension

	// IGameObjectProfileManager
	virtual bool SetAspectProfile( EEntityAspects aspect, uint8 profile );
	virtual uint8 GetDefaultProfile( EEntityAspects aspect );
	// ~IGameObjectProfileManager

	void ReInitFromPool();

	virtual void LoadGeometry();
	virtual void Physicalize();
	virtual void SetVelocity(const Vec3 &pos, const Vec3 &dir, const Vec3 &velocity, float speedScale=1.0f);
	virtual void SetParams(EntityId ownerId, EntityId hostId, EntityId weaponId, int fmId, int damage, int hitTypeId);
  virtual void SetDestination(const Vec3& pos){}
  virtual void SetDestination(EntityId targetId){}
	virtual void Launch(const Vec3 &pos, const Vec3 &dir, const Vec3 &velocity, float speedScale=1.0f);
	virtual void Destroy();
	virtual bool IsRemote() const;
	virtual void SetRemote(bool remote);
	virtual void SetTracked(bool tracked);
	virtual void SetSequence(uint16 seqn);
	virtual uint16 GetSequence() const;

	
	virtual void Explode(bool destroy, bool impact=false, const Vec3 &pos=ZERO, const Vec3 &normal=FORWARD_DIRECTION, const Vec3 &vel=ZERO, EntityId targetId=0 );
	virtual void TrailSound(bool enable, const Vec3 &dir=Vec3(0.0f,1.0f,0.0f));
	virtual void WhizSound(bool enable, const Vec3 &pos, const Vec3 &dir);
	void UpdateWhiz();
	virtual void TrailEffect(bool enable, bool underWater = false);
	virtual void FlashbangEffect(const SFlashbangParams* flashbang);
	virtual void ScaledEffect(const SScaledEffectParams* scaledEffect);
	virtual void EndScaledEffect(const SScaledEffectParams* scaledEffect);

	virtual void Ricochet(EventPhysCollision *pCollision);
	virtual void RicochetSound(const Vec3 &pos, const Vec3 &dir);

	virtual int AttachEffect(bool attach, int id, const char *name=0, const Vec3 &offset=Vec3(0.0f,0.0f,0.0f), const Vec3 &dir=Vec3(0.0f,1.0f,0.0f), float scale=1.0f, bool bParticlePrime = true);

  EntityId GetOwnerId()const;

	float GetSpeed() const;
	inline float GetLifeTime() const { return m_pAmmoParams? m_pAmmoParams->lifetime : 0.0f; }
	bool IsPredicted() const { return m_pAmmoParams? m_pAmmoParams->predictSpawn != 0 : false; }

	//IHitListener
	virtual void OnHit(const HitInfo&);
	virtual void OnExplosion(const ExplosionInfo&);
	virtual void OnServerExplosion(const ExplosionInfo&);

	//Helper function to initialize particle params in exceptional cases
	void SetDefaultParticleParams(pe_params_particle *pParams);

	const SAmmoParams *GetParams() const { return m_pAmmoParams; };

	virtual void InitWithAI( );

	bool IsExpired() const
	{
		const float lifeTime = GetLifeTime();
		if (lifeTime > 0.0f)
		{
			return (gEnv->pTimer->GetCurrTime() - m_spawnTime) > (lifeTime + 1.0f);
		}
		return false;
	}

	bool IsUpdated() const
	{
		return gEnv->pTimer->GetCurrTime() - m_lastUpdate < 0.1f;
	}

	float GetTotalLifeTime()
	{
		return m_totalLifetime;
	}

protected:
	CWeapon *GetWeapon();

	IEntitySoundProxy *GetSoundProxy();
	template<typename T> T GetParam(const char *name, T &def)
	{
		T v(def);
		if (m_pAmmoParams)
		{
			const IItemParamsNode *params = m_pAmmoParams->pItemParams->GetChild("params");
			if (params)
			{
				CItemParamReader reader(params);
				reader.Read(name, v);
			}
		}
		return v;
	}
	
	const SAmmoParams* m_pAmmoParams = nullptr;

	IPhysicalEntity* m_pPhysicalEntity = nullptr;

	int m_whizSoundId = INVALID_SOUNDID;
	int m_trailSoundId = INVALID_SOUNDID;
	int m_trailEffectId = -1;
	int m_trailUnderWaterId = -1;
	Vec3 m_last = ZERO;

	EntityId m_ownerId = 0;
	EntityId m_hostId = 0;
	EntityId m_weaponId = 0;
	int m_fmId = 0;
	int m_damage = 0;
	int m_hitTypeId = 0;

	bool m_destroying = false;
	bool m_tracked = false;

	bool m_firstDropApplied = false;
	Vec3 m_initial_pos = ZERO;
	Vec3 m_initial_dir = ZERO;
	Vec3 m_initial_vel = ZERO;

	bool m_remote = false;
	uint16 m_seq = 0;

	float m_totalLifetime = 0.0f;
	float m_scaledEffectval = 0.0f;
	bool m_scaledEffectSignaled = false;

	int m_hitPoints = -1;
	bool m_noBulletHits = false;
	bool m_hitListener = false;

	IPhysicalEntity* m_obstructObject = nullptr;

	float m_spawnTime = 0.0f;
	float m_lastUpdate = 0.0f;
};

#endif
