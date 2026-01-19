/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Rapid Fire Mode Implementation
*************************************************************************/
#ifndef __RAPID_H__
#define __RAPID_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "Single.h"

class CRapid : public CSingle
{
protected:
	struct SRapidParams
	{
		SRapidParams() { Reset(); };

		void Reset(const IItemParamsNode* params = 0, bool defaultInit = true)
		{
			CItemParamReader reader(params);

			ResetValue(min_speed, 1.5f);
			ResetValue(max_speed, 3.0f);
			ResetValue(acceleration, 3.0f);
			ResetValue(deceleration, -3.0f);

			ResetValue(barrel_attachment, "");
			ResetValue(engine_attachment, "");

			ResetValue(camshake_rotate, Vec3(0));
			ResetValue(camshake_shift, Vec3(0));
			ResetValue(camshake_perShot, 0.0f);

			ResetValue(tp_barrel_enable, false);
			ResetValue(tp_barrel_slot, (int)CItem::eIGS_Aux1);
			ResetValue(tp_barrel_model, "");
			ResetValue(tp_barrel_hide_fp, false);

			ResetValue(tp_barrel_offset, Vec3(ZERO));
			ResetValue(tp_barrel_scale, 1.0f);
			ResetValue(tp_barrel_rotation, Vec3(ZERO));
			ResetValue(barrel_spin_mult, 1.0f);
		}

		void GetMemoryStatistics(ICrySizer* s)
		{
			s->Add(barrel_attachment);
			s->Add(engine_attachment);
			s->Add(tp_barrel_model);
		}

		float min_speed;
		float max_speed;
		float acceleration;
		float deceleration;

		ItemString barrel_attachment;
		ItemString engine_attachment;

		Vec3  camshake_rotate;
		Vec3  camshake_shift;
		float camshake_perShot;

		bool       tp_barrel_enable;
		int        tp_barrel_slot;
		ItemString tp_barrel_model;
		bool       tp_barrel_hide_fp;

		Vec3       tp_barrel_offset;
		float      tp_barrel_scale;
		Vec3       tp_barrel_rotation;   // degrees XYZ
		float      barrel_spin_mult;
	};

	struct SRapidActions
	{
		SRapidActions() { Reset(); };

		void Reset(const IItemParamsNode* params = 0, bool defaultInit = true)
		{
			CItemParamReader reader(params);
			ResetValue(rapid_fire, "rapid_fire");
			ResetValue(blast, "blast");
		}

		void GetMemoryStatistics(ICrySizer* s)
		{
			s->Add(rapid_fire);
			s->Add(blast);
		}

		ItemString rapid_fire;
		ItemString blast;
	};

public:
	CRapid();
	virtual ~CRapid();

	virtual void Update(float frameTime, unsigned int frameId);

	virtual void ResetParams(const struct IItemParamsNode* params);
	virtual void PatchParams(const struct IItemParamsNode* patch);

	virtual void GetMemoryStatistics(ICrySizer* s);

	virtual void Activate(bool activate);

	virtual void StartReload(int zoomed);

	virtual void StartFire();
	virtual void StopFire();
	virtual bool IsFiring() const { return m_firing || m_accelerating; };

	virtual void NetStartFire();
	virtual void NetStopFire();

	virtual float GetSpinUpTime() const;
	virtual float GetSpinDownTime() const;

	virtual bool AllowZoom() const;

	virtual const char* GetType() const;
	virtual int PlayActionSAFlags(int flags) { return (flags | CItem::eIPAF_Animation) & ~CItem::eIPAF_Sound; };

	void OnEnterFirstPerson() override;
	void OnEnterThirdPerson() override;

protected:
	virtual void Accelerate(float acc);
	virtual void Firing(bool firing);
	void UpdateRotation(CItem::eGeometrySlot slot, float frameTime);
	virtual void UpdateSound(float frameTime);
	virtual void FinishDeceleration();

	void SetSlotRender(IEntity* pEnt, int slot, bool render);
	bool ShouldShowTpBarrel() const;
	void UpdateTpBarrelVisibility();

	SRapidActions m_rapidactions;
	SRapidParams  m_rapidparams;

	float m_speed = 0.0f;
	float m_acceleration = 0.0f;
	float m_rotation_angle = 0.0f;

	bool  m_netshooting = false;

	bool  m_accelerating = false;
	bool  m_decelerating = false;

	unsigned int m_soundId = INVALID_SOUNDID;
	unsigned int m_spinUpSoundId = INVALID_SOUNDID;

	bool  m_startedToFire = false;

	bool  m_hasBarrelAttachment = false;
	bool  m_hasEngineAttachment = false;

	Matrix34 m_barrelBaseTM = Matrix34::CreateIdentity();
	bool     m_barrelBaseTMValid = false;
};

#endif
