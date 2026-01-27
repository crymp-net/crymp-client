/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements an entity class which can serialize vehicle parts

-------------------------------------------------------------------------
History:
- 16:09:2005: Created by Mathieu Pinard

*************************************************************************/
#ifndef __VEHICLEPARTENTITY_H__
#define __VEHICLEPARTENTITY_H__

#include "CryCommon/CryAction/IGameObject.h"

class CVehiclePartEntity
	: public CGameObjectExtensionHelper<CVehiclePartEntity, IGameObjectExtension>
{
public:

	virtual bool Init(IGameObject * pGameObject);
	virtual void InitClient(int channelId) {};
	virtual void PostInit(IGameObject*) {}
	virtual void PostInitClient(int channelId) {};
	virtual void Release() { delete this; }

	virtual void Serialize(TSerialize ser, unsigned aspects);
	virtual void PostSerialize() {}
	virtual void SerializeSpawnInfo( TSerialize ser ) {}
	virtual ISerializableInfoPtr GetSpawnInfo() {return 0;}
	virtual void Update(SEntityUpdateContext& ctx, int);
	virtual void HandleEvent(const SGameObjectEvent& event);
	virtual void ProcessEvent(SEntityEvent& event) {};
	virtual void SetChannelId(uint16 id) {};
	virtual void SetAuthority(bool auth) {};
	virtual void PostUpdate(float frameTime) { assert(false); };
	virtual void GetMemoryStatistics(ICrySizer * s) { s->Add(*this); }

protected:

};

#endif
