#include <cstdint>

#include "CryCommon/CrySystem/gEnv.h"
#include "CryCommon/CrySystem/IConsole.h"

#include "ItemSystemLegacy.h"

extern std::uintptr_t CRYACTION_BASE;

ItemSystemLegacy::ItemSystemLegacy(IGameFramework* pGameFramework, ISystem* pSystem)
{
#ifdef BUILD_64BIT
	std::uintptr_t ctor = CRYACTION_BASE + 0x2e450;
#else
	std::uintptr_t ctor = CRYACTION_BASE + 0x22e00;
#endif

	(this->*reinterpret_cast<void(ILevelSystemListener::*&)(IGameFramework*, ISystem*)>(ctor))(pGameFramework, pSystem);

	/*
		IConsole* pConsole = gEnv->pConsole;

		pConsole->AddCommand("i_giveitem", &ItemSystemLegacy::GiveItemCmd, VF_CHEAT, "Gives specified item to the player!");
		pConsole->AddCommand("i_giveallitems", &ItemSystemLegacy::GiveAllItemsCmd, VF_CHEAT, "Gives all available items to the player!");
		pConsole->AddCommand("i_givedebugitems", &ItemSystemLegacy::GiveDebugItemsCmd, VF_CHEAT, "Gives special debug items to the player!");
		pConsole->RegisterInt("i_noweaponlimit", 0, VF_CHEAT, "Player can carry all weapons he wants!");
		pConsole->RegisterInt("i_precache", 1, VF_DUMPTODISK, "Enables precaching of items during level loading.");
		pConsole->RegisterInt("i_lying_item_limit", 64, 0, "Max number of items lying around in a level. Only works in multiplayer.");
	*/
}

ItemSystemLegacy::~ItemSystemLegacy()
{
	/*
		IConsole* pConsole = gEnv->pConsole;

		pConsole->RemoveCommand("i_giveitem");
		pConsole->RemoveCommand("i_giveallitems");
		pConsole->RemoveCommand("i_givedebugitems");
		pConsole->UnregisterVariable("i_noweaponlimit", true);
		pConsole->UnregisterVariable("i_precache", true);
		pConsole->UnregisterVariable("i_lying_item_limit", true);
	*/
}

void ItemSystemLegacy::Update()
{
#ifdef BUILD_64BIT
	std::uintptr_t func = CRYACTION_BASE + 0x28d90;
#else
	std::uintptr_t func = CRYACTION_BASE + 0x20880;
#endif

	(this->*reinterpret_cast<void(ILevelSystemListener::*&)()>(func))();
}

////////////////////////////////////////////////////////////////////////////////
// ILevelSystemListener
////////////////////////////////////////////////////////////////////////////////

void ItemSystemLegacy::OnLevelNotFound(const char* levelName)
{
}

void ItemSystemLegacy::OnLoadingStart(ILevelInfo* pLevel)
{
}

void ItemSystemLegacy::OnLoadingComplete(ILevel* pLevel)
{
}

void ItemSystemLegacy::OnLoadingError(ILevelInfo* pLevel, const char* error)
{
}

void ItemSystemLegacy::OnLoadingProgress(ILevelInfo* pLevel, int progressAmount)
{
}

////////////////////////////////////////////////////////////////////////////////
// IItemSystem
////////////////////////////////////////////////////////////////////////////////

void ItemSystemLegacy::Reset()
{
}

void ItemSystemLegacy::Reload()
{
}

void ItemSystemLegacy::Scan(const char* folderName)
{
}

IItemParamsNode* ItemSystemLegacy::CreateParams()
{
	return nullptr;
}

const IItemParamsNode* ItemSystemLegacy::GetItemParams(const char* itemName) const
{
	return nullptr;
}

int ItemSystemLegacy::GetItemParamsCount() const
{
	return 0;
}

const char* ItemSystemLegacy::GetItemParamName(int index) const
{
	return nullptr;
}

std::uint8_t ItemSystemLegacy::GetItemPriority(const char* item) const
{
	return 0;
}

const char* ItemSystemLegacy::GetItemCategory(const char* item) const
{
	return nullptr;
}

std::uint8_t ItemSystemLegacy::GetItemUniqueId(const char* item) const
{
	return 0;
}

bool ItemSystemLegacy::IsItemClass(const char* name) const
{
	return false;
}

void ItemSystemLegacy::RegisterForCollection(EntityId itemId)
{
}

void ItemSystemLegacy::UnregisterForCollection(EntityId itemId)
{
}

void ItemSystemLegacy::AddItem(EntityId itemId, IItem* pItem)
{
}

void ItemSystemLegacy::RemoveItem(EntityId itemId)
{
}

IItem* ItemSystemLegacy::GetItem(EntityId itemId) const
{
	return nullptr;
}

void ItemSystemLegacy::SetConfiguration(const char* name)
{
}

const char* ItemSystemLegacy::GetConfiguration() const
{
	return nullptr;
}

ICharacterInstance* ItemSystemLegacy::GetCachedCharacter(const char* fileName)
{
	return nullptr;
}

IStatObj* ItemSystemLegacy::GetCachedObject(const char* fileName)
{
	return nullptr;
}

void ItemSystemLegacy::CacheObject(const char* fileName)
{
}

void ItemSystemLegacy::CacheGeometry(const IItemParamsNode* geometry)
{
}

void ItemSystemLegacy::CacheItemGeometry(const char* className)
{
}

void ItemSystemLegacy::ClearGeometryCache()
{
}

void ItemSystemLegacy::CacheItemSound(const char* className)
{
}

void ItemSystemLegacy::ClearSoundCache()
{
}

void ItemSystemLegacy::Serialize(TSerialize ser)
{
}

EntityId ItemSystemLegacy::GiveItem(IActor* pActor, const char* item, bool sound, bool select, bool keepHistory)
{
	return 0;
}

void ItemSystemLegacy::SetActorItem(IActor* pActor, EntityId itemId, bool keepHistory)
{
}

void ItemSystemLegacy::SetActorItem(IActor* pActor, const char* name, bool keepHistory)
{
}

void ItemSystemLegacy::DropActorItem(IActor* pActor, EntityId itemId)
{
}

void ItemSystemLegacy::SetActorAccessory(IActor* pActor, EntityId itemId, bool keepHistory)
{
}

void ItemSystemLegacy::DropActorAccessory(IActor* pActor, EntityId itemId)
{
}

void ItemSystemLegacy::RegisterListener(IItemSystemListener* pListener)
{
}

void ItemSystemLegacy::UnregisterListener(IItemSystemListener* pListener)
{
}

IEquipmentManager* ItemSystemLegacy::GetIEquipmentManager()
{
	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
