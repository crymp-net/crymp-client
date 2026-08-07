#include "Utilities.h"

#include "CryCommon/CryEntitySystem/IEntitySystem.h"
#include "CryAction/ItemSystem.h"
#include "CryGame/Game.h"
#include "CrySystem/CryPak.h"

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

void ResetGameObjectSystem() {

#ifdef BUILD_64BIT
	constexpr size_t kSchedulingParamsOffset = 0xD8;
	constexpr uintptr_t kMapOperator = 0x306B3120;
	typedef SEntitySchedulingProfiles* (__fastcall* PFNEMPLACE)(void* map, string* key);
#else
	constexpr size_t kSchedulingParamsOffset = 0x6C;
	constexpr uintptr_t kMapOperator = 0x3062EE00;
	typedef SEntitySchedulingProfiles* (__fastcall* PFNEMPLACE)(void* map, void *dummyEdx, string* key);
#endif

	PFNEMPLACE Emplace = (PFNEMPLACE)kMapOperator;

	// By default game initializes this only once, it contains information on how to schedule
	// entity classes on network. Now, the issue is if there are new classes in the server/client PAK
	// the network system won't recognize it and it will disconnect the player on protocol error.
	// Therefore we need to reconstruct the database like this, since this is internal function of CE SDK class
	// and it's not exposed in the public SDK.
	IGameObjectSystem* pGOS = static_cast<IGameObjectSystem*>(gEnv->pGame->GetIGameFramework()->GetIGameObjectSystem());
	if (XmlNodeRef schedParams = gEnv->pSystem->LoadXmlFile("Game/Scripts/Network/EntityScheduler.xml"))
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

			// CE2 SDK uses MSVC 2005, since we are building with at least MSVC 2022, calling std::map::emplace
			// would crash on ABI differences.
			// Instead we call existing MSVC 2005 API inside CE2 SDK to emplace the object
			//  original code: m_schedulingParams[name] = p;
			void* m_schedulingParams = reinterpret_cast<void*>(
				reinterpret_cast<uintptr_t>(pGOS) + kSchedulingParamsOffset
			);
#ifdef BUILD_64BIT
			auto value = Emplace(m_schedulingParams, &name);
			*value = p;
#else
			// We need to fill EDX with a dummy value to force &name to be pushed on stack
			// since this is a thiscall masked as a fastcall.
			auto value = Emplace(m_schedulingParams, NULL, &name);
			*value = p;
#endif
		}
	}

	// Reset entity system and item system, so they recognize possibly new classes imported
	// by server/client PAK

	IEntityClassRegistry* pClassRegistry = gEnv->pEntitySystem->GetClassRegistry();
	pClassRegistry->LoadClasses("LoadClasses", true);

	// Merely calling Reload doesn't create new classes in the Network system, that's why
	// Scan(...) must be called instead
	gEnv->pGame->GetIGameFramework()->GetIItemSystem()->Scan("Scripts/Entities/Items/XML");
}