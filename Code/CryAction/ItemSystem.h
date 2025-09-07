#pragma once

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "CryCommon/CryAction/IGameFramework.h"
#include "CryCommon/CryAction/IItemSystem.h"
#include "CryCommon/CryAction/ILevelSystem.h"

struct ICVar;
struct IConsoleCmdArgs;
struct ICharacterInstance;
struct IStatObj;

class EquipmentManager;

class ItemSystem final : public ILevelSystemListener, public IItemSystem
{
	template<class T>
	struct Releaser
	{
		void operator()(T* p) const { p->Release(); }
	};

	using SmartParamsNode = std::unique_ptr<IItemParamsNode, Releaser<IItemParamsNode>>;
	using SmartCharacterInstance = std::unique_ptr<ICharacterInstance, Releaser<ICharacterInstance>>;
	using SmartStatObj = std::unique_ptr<IStatObj, Releaser<IStatObj>>;

	struct ItemParams
	{
		SmartParamsNode root;
		SmartParamsNode root_multiplayer;

		const char* category;
		int priority = 0;
		int uniqueId = 0;

		bool geometryCached = false;
		bool soundCached = false;
	};

	std::map<std::string, IGameFramework::IItemCreator*, std::less<void>> m_factories;
	std::map<std::string, ItemParams, std::less<void>> m_params;
	std::map<EntityId, IItem*> m_items;

	std::map<std::string, SmartCharacterInstance, std::less<void>> m_cachedCharacters;
	std::map<std::string, SmartStatObj, std::less<void>> m_cachedObjects;

	std::vector<EntityId> m_garbage;
	std::vector<IItemSystemListener*> m_listeners;
	std::vector<std::string> m_folders;

	std::unique_ptr<EquipmentManager> m_pEquipmentManager;

	XmlNodeRef m_playerLevelToLevelData;

	std::string m_spawnName;
	unsigned int m_spawnCount = 0;

	IGameFramework* m_pGameFramework = nullptr;

	bool m_reloading = false;
	bool m_recursing = false;
	bool m_multiplayer = false;

	ICVar* m_pPrecacheCVar = nullptr;
	ICVar* m_pNoWeaponLimitCVar = nullptr;
	ICVar* m_pLyingItemLimitCVar = nullptr;

public:
	explicit ItemSystem(IGameFramework* pGameFramework);
	virtual ~ItemSystem();

	void Update(float frameTime);

	void PrecacheLevel();

	void RegisterItemFactory(const char* name, IGameFramework::IItemCreator* pCreator);

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
	// ILevelSystemListener
	////////////////////////////////////////////////////////////////////////////////

	void OnLevelNotFound(const char* levelName) override;
	void OnLoadingStart(ILevelInfo* pLevel) override;
	void OnLoadingComplete(ILevel* pLevel) override;
	void OnLoadingError(ILevelInfo* pLevel, const char* error) override;
	void OnLoadingProgress(ILevelInfo* pLevel, int progressAmount) override;

	////////////////////////////////////////////////////////////////////////////////

	virtual void SerializePlayerLTLInfo(bool reading);

private:
	const IItemParamsNode* GetParamsRoot(const ItemParams& params) const;
	const IItemParamsNode* FindParamNode(const IItemParamsNode* root, const char* name) const;
	bool IsGiveableItem(const IItemParamsNode* root) const;
	bool IsDebugItem(const IItemParamsNode* root) const;

	void SetSpawnName(std::string_view name);

	void ScanXml(XmlNodeRef& root, const char* xmlFile);

	void RegisterCVars();
	void UnregisterCVars();

	void GiveOneItemOrAll(const char* actorName, const char* itemName, bool debugOnly);

	static void GiveItemCmd(IConsoleCmdArgs* args);
	static void GiveAllItemsCmd(IConsoleCmdArgs* args);
	static void GiveDebugItemsCmd(IConsoleCmdArgs* args);
};
