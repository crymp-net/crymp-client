#include <cstdint>

#include "ScriptBind_ItemSystem.h"

extern std::uintptr_t CRYACTION_BASE;

ScriptBind_ItemSystem::ScriptBind_ItemSystem(ISystem* pSystem, ItemSystem* pItemSystem, IGameFramework* pGameFramework)
{
#ifdef BUILD_64BIT
	std::uintptr_t ctor = CRYACTION_BASE + 0x328d0;
#else
	std::uintptr_t ctor = CRYACTION_BASE + 0x25d40;
#endif

	(this->*reinterpret_cast<void(ScriptBind_ItemSystem::*&)(ISystem*, ItemSystem*, IGameFramework*)>(ctor))(pSystem, pItemSystem, pGameFramework);
}
