#include "Utilities.h"

#include "CryCommon/CryEntitySystem/IEntitySystem.h"
#include "CryAction/ItemSystem.h"
#include "CryGame/Game.h"
#include "CryGame/Items/Weapons/WeaponSystem.h"
#include "CrySystem/CryPak.h"

extern std::uintptr_t CRYACTION_BASE;

static bool StringToKey(const char* s, uint32& key)
{
	const size_t len = strlen(s);
	if (len > 4)
		return false;

	key = 0;
	for (size_t i = 0; i < len; i++)
	{
		key <<= 8;
		key |= uint8(s[i]);
	}

	return true;
}

struct SEntitySchedulingProfiles
{
	uint32 normal;
	uint32 owned;
};

struct SchedulingParamsMap
{
	SEntitySchedulingProfiles& operator[](const string& key)
	{
#ifdef BUILD_64BIT
		std::uintptr_t func = CRYACTION_BASE + 0x1B3120;
#else
		std::uintptr_t func = CRYACTION_BASE + 0x12EE00;
#endif

		return (this->*reinterpret_cast<decltype(&SchedulingParamsMap::operator[])&>(func))(key);
	}
};

void ResetGameObjectSystem() {
	// By default game initializes this only once, it contains information on how to schedule
	// entity classes on network. Now, the issue is if there are new classes in the server/client PAK
	// the network system won't recognize it and it will disconnect the player on protocol error.
	// Therefore we need to reconstruct the database like this, since this is internal function of CE SDK class
	// and it's not exposed in the public SDK.
	IGameObjectSystem* pGOS = gEnv->pGame->GetIGameFramework()->GetIGameObjectSystem();
	if (XmlNodeRef schedParams = gEnv->pSystem->LoadXmlFile("Scripts/Network/EntityScheduler.xml"))
	{
		uint32 defaultPolicy = 0;

		if (XmlNodeRef defpol = schedParams->findChild("Default"))
		{
			if (!StringToKey(defpol->getAttr("policy"), defaultPolicy))
			{
				return;
			}
		}

		for (int i = 0; i < schedParams->getChildCount(); i++)
		{
			XmlNodeRef node = schedParams->getChild(i);
			if (0 != strcmp(node->getTag(), "Class"))
				continue;

			string name = node->getAttr("name");

			SEntitySchedulingProfiles p;
			p.normal = defaultPolicy;
			if (node->haveAttr("policy"))
				StringToKey(node->getAttr("policy"), p.normal);
			p.owned = p.normal;
			if (node->haveAttr("own"))
				StringToKey(node->getAttr("own"), p.owned);

			SchedulingParamsMap& m_schedulingParams =
#ifdef BUILD_64BIT
				*reinterpret_cast<SchedulingParamsMap*>(reinterpret_cast<std::uintptr_t>(pGOS) + 0xD8);
#else
				*reinterpret_cast<SchedulingParamsMap*>(reinterpret_cast<std::uintptr_t>(pGOS) + 0x6C);
#endif
			m_schedulingParams[name] = p;
		}
	}

	// Reset entity system and item system, so they recognize possibly new classes imported
	// by server/client PAK

	IEntityClassRegistry* pClassRegistry = gEnv->pEntitySystem->GetClassRegistry();
	pClassRegistry->LoadClasses("Entities", true);

	// Now we must reconstruct the protocol definition, to do so we reset the safety state
	// which will force CryEngine to rebuild the extension ID tables for network sinks
	// before connecting to the server
#ifdef BUILD_64BIT
	bool* pDispatchSafety = reinterpret_cast<bool*>(
		reinterpret_cast<uintptr_t>(pGOS) + 0x50
	);
	*pDispatchSafety = false;
#else
	bool* pDispatchSafety = reinterpret_cast<bool*>(
		reinterpret_cast<uintptr_t>(pGOS) + 0x28
		);
	*pDispatchSafety = false;
#endif
	//CryMP: Load ammo definitions first, so new ammo classes are available
	//when newly imported weapons are loaded.
	if (g_pGame && g_pGame->GetWeaponSystem())
	{
		g_pGame->GetWeaponSystem()->Reload();
	}

	gEnv->pGame->GetIGameFramework()->GetIItemSystem()->Reload();
}