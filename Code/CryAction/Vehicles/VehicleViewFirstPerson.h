/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements the first person pit view for vehicles

-------------------------------------------------------------------------
History:
- 29:01:2006: Created by Mathieu Pinard

*************************************************************************/
#ifndef __VEHICLEVIEWFIRSTPERSON_H__
#define __VEHICLEVIEWFIRSTPERSON_H__

#include "VehicleViewBase.h"

class CVehicleViewFirstPerson
	: public CVehicleViewBase
{
	IMPLEMENT_VEHICLEOBJECT;
public:

	CVehicleViewFirstPerson();
	~CVehicleViewFirstPerson() {}

	// IVehicleView
	virtual bool Init(CVehicleSeat* pSeat, const CVehicleParams& table) override;
	virtual void Reset() override;

	virtual const char* GetName() override { return m_name; }
	virtual bool IsThirdPerson() override { return false; }

	virtual void OnStartUsing(EntityId playerId) override;
	virtual void OnStopUsing() override;

	virtual void Update(const float frameTime) override;
	virtual void UpdateView(SViewParams& viewParams, EntityId playerId = 0) override;

	void GetMemoryStatistics(ICrySizer* s) override;
	// ~IVehicleView

	// IVehicleEventListener
	virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params) override;
	// ~IVehicleEventListener

protected:

	struct OpacitySettings
	{
		std::string name;
		float defaultOpacity = 0.0f;
		float opacity = 0.0f;
	};

	Vec3 GetWorldPosGoal() const;
	Quat GetWorldRotGoal() const;
	Quat GetVehicleRotGoal() const;

	void HideEntity(IEntity* pEntity, bool hide);
	void SetSubMtlOpacity(IMaterial* pSubMtl, float opacity);
	bool GetOpacitySettings(OpacitySettings& settings);

	void HideEntitySlots(IEntity* pEnt, bool hide);

	static const char* m_name;

	IVehicleHelper* m_pHelper = nullptr;
	Vec3 m_offset = Vec3();
	float m_relToHorizon = 0.0f;
	bool m_hideVehicle = false;
	int m_frameSlot = 0;

	using TSlots = std::multimap<EntityId, int>;
	TSlots m_slotFlags;

	Vec3 m_viewPosition = Vec3();
	Quat m_viewRotation = Quat(IDENTITY);

	float m_speedPos = 0.0f;
	float m_speedRot = 0.0f;

	EntityId m_passengerId = 0;

	std::vector<OpacitySettings> m_opacitySettings;

};

#endif

