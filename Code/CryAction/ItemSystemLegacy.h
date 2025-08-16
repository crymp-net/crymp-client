#pragma once

#include "CryCommon/CryAction/IItemSystem.h"
#include "CryCommon/CryAction/ILevelSystem.h"

struct IStatObj;
struct IGameFramework;
struct ISystem;

class ItemSystemLegacy : public ILevelSystemListener, public IItemSystem
{
#ifdef BUILD_64BIT
	unsigned char m_data[0x1b0 - 0x10] = {};
#else
	unsigned char m_data[0xe0 - 0x8] = {};
#endif

public:
	explicit ItemSystemLegacy(IGameFramework* pGameFramework, ISystem* pSystem);
	virtual ~ItemSystemLegacy();

	void Update();

	////////////////////////////////////////////////////////////////////////////////
	// ILevelSystemListener
	////////////////////////////////////////////////////////////////////////////////

	void OnLevelNotFound(const char* levelName) override;
	void OnLoadingStart(ILevelInfo* pLevel) override;
	void OnLoadingComplete(ILevel* pLevel) override;
	void OnLoadingError(ILevelInfo* pLevel, const char* error) override;
	void OnLoadingProgress(ILevelInfo* pLevel, int progressAmount) override;

	////////////////////////////////////////////////////////////////////////////////
	// IItemSystem
	////////////////////////////////////////////////////////////////////////////////

	void Reset() override;
	void Reload() override;
	void Scan(const char* folderName) override;
	IItemParamsNode* CreateParams() override;
	const IItemParamsNode* GetItemParams(const char* itemName) const override;
	int GetItemParamsCount() const override;
	const char* GetItemParamName(int index) const override;
	std::uint8_t GetItemPriority(const char* item) const override;
	const char* GetItemCategory(const char* item) const override;
	std::uint8_t GetItemUniqueId(const char* item) const override;

	bool IsItemClass(const char* name) const override;

	void RegisterForCollection(EntityId itemId) override;
	void UnregisterForCollection(EntityId itemId) override;

	void AddItem(EntityId itemId, IItem* pItem) override;
	void RemoveItem(EntityId itemId) override;
	IItem* GetItem(EntityId itemId) const override;

	void SetConfiguration(const char* name) override;
	const char* GetConfiguration() const override;

	ICharacterInstance* GetCachedCharacter(const char* fileName) override;
	IStatObj* GetCachedObject(const char* fileName) override;
	void CacheObject(const char* fileName) override;
	void CacheGeometry(const IItemParamsNode* geometry) override;
	void CacheItemGeometry(const char* className) override;
	void ClearGeometryCache() override;

	void CacheItemSound(const char* className) override;
	void ClearSoundCache() override;

	void Serialize(TSerialize ser) override;

	EntityId GiveItem(IActor* pActor, const char* item, bool sound, bool select, bool keepHistory) override;
	void SetActorItem(IActor* pActor, EntityId itemId, bool keepHistory = true) override;
	void SetActorItem(IActor* pActor, const char* name, bool keepHistory = true) override;
	void DropActorItem(IActor* pActor, EntityId itemId) override;
	void SetActorAccessory(IActor* pActor, EntityId itemId, bool keepHistory = true) override;
	void DropActorAccessory(IActor* pActor, EntityId itemId) override;

	void RegisterListener(IItemSystemListener* pListener) override;
	void UnregisterListener(IItemSystemListener* pListener) override;

	IEquipmentManager* GetIEquipmentManager() override;

	////////////////////////////////////////////////////////////////////////////////
};
