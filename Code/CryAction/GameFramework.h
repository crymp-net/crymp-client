#pragma once

#include "CryCommon/CryAction/IGameFramework.h"
#include "Library/StlportVector.h"

#include "ActionGame.h"

struct ICVar;

class ActorSystem;
class AnimationGraphSystem;
class CallbackTimer;
class DevMode;
class DialogSystem;
class GameFrameworkCVars;
class GameObjectSystem;
class GameplayAnalyst;
class GameplayRecorder;
class GameQueryListener;
class GameSerialize;
class GameServerNub;
class GameStatsConfig;
class ItemSystem;
class MaterialEffects;
class NetworkCVars;
class PersistantDebug;
class ScriptBind_ActionMapManager;
class ScriptBind_Actor;
class ScriptBind_AI;
class ScriptBind_CryAction;
class ScriptBind_DialogSystem;
class ScriptBind_Inventory;
class ScriptBind_ItemSystem;
class ScriptBind_MaterialEffects;
class ScriptBind_Network;
class ScriptBind_Vehicle;
class ScriptBind_VehicleSeat;
class ScriptBind_VehicleSystem;
class ScriptRMI;
class TimeDemoRecorder;
class TimeOfDayScheduler;
class VehicleSystem;
class ViewSystem;

class GameFramework : public IGameFramework
{
	// TODO: use std::unique_ptr instead of raw pointers

	struct Listener
	{
		IGameFrameworkListener* pListener = nullptr;
		CryStringT<char> name;
		EFRAMEWORKLISTENERPRIORITY priority = FRAMEWORKLISTENERPRIORITY_DEFAULT;
	};

	struct SomeStrings
	{
		CryStringT<char> delayedSaveGameName;
		CryStringT<char> checkPointName;
		CryStringT<char> nextLevelToLoad;
	};

	// m_reserved_<32-bit-offset>_<64-bit-offset>
	bool m_isPaused = false;  // m_reserved_0x4_0x8
	bool m_isForcedPaused = false; // m_reserved_0x5_0x9
	ISystem* m_pSystem = nullptr;
	INetwork* m_pNetwork = nullptr;
	I3DEngine* m_p3DEngine = nullptr;
	IScriptSystem* m_pScriptSystem = nullptr;
	IEntitySystem* m_pEntitySystem = nullptr;
	ITimer* m_pTimer = nullptr;
	ILog* m_pLog = nullptr;
	void* m_reserved_0x24_0x48 = nullptr;
	_smart_ptr<ActionGame> m_pActionGame;  // m_reserved_0x28_0x50
	char m_editorLevelName[512] = {};  // m_reserved_0x2c_0x58
	char m_editorLevelFolder[512] = {};  // m_reserved_0x22c_0x258
	char m_guid[128] = "{00000000-0000-0000-0000-000000000000}";
	ILevelSystem* m_pLevelSystem = nullptr;  // m_reserved_0x4ac_0x4d8
	ActorSystem* m_pActorSystem = nullptr;  // m_reserved_0x4b0_0x4e0
	ItemSystem* m_pItemSystem = nullptr;  // m_reserved_0x4b4_0x4e8
	VehicleSystem* m_pVehicleSystem = nullptr;  // m_reserved_0x4b8_0x4f0
	IActionMapManager* m_pActionMapManager = nullptr;  // m_reserved_0x4bc_0x4f8
	ViewSystem* m_pViewSystem = nullptr;  // m_reserved_0x4c0_0x500
	GameplayRecorder* m_pGameplayRecorder = nullptr;  // m_reserved_0x4c4_0x508
	IGameRulesSystem* m_pGameRulesSystem = nullptr;  // m_reserved_0x4c8_0x510
	IFlowSystem* m_pFlowSystem = nullptr;  // m_reserved_0x4cc_0x518
	IUIDraw* m_pUIDraw = nullptr;
	GameObjectSystem* m_pGameObjectSystem = nullptr;  // m_reserved_0x4d4_0x528
	ScriptRMI* m_pScriptRMI = nullptr;
	AnimationGraphSystem* m_pAnimationGraphSystem = nullptr;  // m_reserved_0x4dc_0x538
	MaterialEffects* m_pMaterialEffects = nullptr;  // m_reserved_0x4e0_0x540
	IPlayerProfileManager* m_pPlayerProfileManager = nullptr;  // m_reserved_0x4e4_0x548
	DialogSystem* m_pDialogSystem = nullptr;  // m_reserved_0x4e8_0x550
	IDebrisMgr* m_pDebrisMgr = nullptr;  // m_reserved_0x4ec_0x558
	ISubtitleManager* m_pSubtitleManager = nullptr;  // m_reserved_0x4f0_0x560
	IGameTokenSystem* m_pGameTokenSystem = nullptr;
	IEffectSystem* m_pEffectSystem = nullptr;
	GameSerialize* m_pGameSerialize = nullptr;  // m_reserved_0x4fc_0x578
	CallbackTimer* m_pCallbackTimer = nullptr;  // m_reserved_0x500_0x580
	GameplayAnalyst* m_pGameplayAnalyst = nullptr;  // m_reserved_0x504_0x588
	ILanQueryListener* m_pLanQueryListener = nullptr;  // m_reserved_0x508_0x590
	GameStatsConfig* m_pGameStatsConfig = nullptr;  // m_reserved_0x50c_0x598
	DevMode* m_pDevMode = nullptr;
	TimeDemoRecorder* m_pTimeDemoRecorder = nullptr;
	GameQueryListener* m_pGameQueryListener = nullptr;  // m_reserved_0x518_0x5b0
	void* m_reserved_0x51c_0x5b8 = nullptr;
	ScriptBind_CryAction* m_pScriptBind_CryAction = nullptr;  // m_reserved_0x520_0x5c0
	ScriptBind_ItemSystem* m_pScriptBind_ItemSystem = nullptr;  // m_reserved_0x524_0x5c8
	ScriptBind_Actor* m_pScriptBind_Actor = nullptr;  // m_reserved_0x528_0x5d0
	ScriptBind_AI* m_pScriptBind_AI = nullptr;  // m_reserved_0x52c_0x5d8
	ScriptBind_Network* m_pScriptBind_Network = nullptr;  // m_reserved_0x530_0x5e0
	ScriptBind_ActionMapManager* m_pScriptBind_ActionMapManager = nullptr;  // m_reserved_0x534_0x5e8
	ScriptBind_VehicleSystem* m_pScriptBind_VehicleSystem = nullptr;  // m_reserved_0x538_0x5f0
	ScriptBind_Vehicle* m_pScriptBind_Vehicle = nullptr;  // m_reserved_0x53c_0x5f8
	ScriptBind_VehicleSeat* m_pScriptBind_VehicleSeat = nullptr;  // m_reserved_0x540_0x600
	ScriptBind_Inventory* m_pScriptBind_Inventory = nullptr;  // m_reserved_0x544_0x608
	ScriptBind_DialogSystem* m_pScriptBind_DialogSystem = nullptr;  // m_reserved_0x548_0x610
	ScriptBind_MaterialEffects* m_pScriptBind_MaterialEffects = nullptr;  // m_reserved_0x54c_0x618
	TimeOfDayScheduler* m_pTimeOfDayScheduler = nullptr;  // m_reserved_0x550_0x620
	PersistantDebug* m_pPersistantDebug = nullptr;  // m_reserved_0x554_0x628
	NetworkCVars* m_pNetworkCVars = nullptr;  // m_reserved_0x558_0x630
	GameFrameworkCVars* m_pGameFrameworkCVars = nullptr;  // m_reserved_0x55c_0x638
	void* m_reserved_0x560_0x640 = nullptr;
	IAnimationGraphState* m_pMusicGraphState = nullptr;  // m_reserved_0x564_0x648
	IMusicLogic* m_pMusicLogic = nullptr;  // m_reserved_0x568_0x650
	ICVar* m_pEnableLoadingScreenCVar = nullptr;
	void* m_reserved_0x570_0x660 = nullptr;
	ICVar* m_pLanBrowserCVar = nullptr;  // m_reserved_0x574_0x668
	bool m_isLanBrowserRunning = false;  // m_reserved_0x578_0x670
	bool m_isEditing = false;  // m_reserved_0x579_0x671
	bool m_isLevelEndScheduled = false;  // m_reserved_0x57a_0x672
	int m_delayedSaveGameMethod = 0;  // m_reserved_0x57c_0x674
	ESaveGameReason m_delayedSaveGameReason = {};  // m_reserved_0x580_0x678
	SomeStrings* m_pSomeStrings = nullptr;  // m_reserved_0x584_0x680
	StlportVector_CryAction<Listener>* m_pListeners = nullptr;  // m_reserved_0x588_0x688
	StlportVector_CryAction<Listener>* m_pListenersCopy = nullptr;  // m_reserved_0x58c_0x690
	int m_voiceRecording = 0;  // m_reserved_0x590_0x698
	bool m_isSaveAllowed = true;  // m_reserved_0x594_0x69c
	bool m_isLoadAllowed = true;  // m_reserved_0x595_0x69d
	CryStringT<char>* m_pNextFrameCommand = nullptr;  // m_reserved_0x598_0x6a0
	void* m_reserved_0x59c_0x6a8 = nullptr;
	float m_lastSaveLoad = 0;  // m_reserved_0x5a0_0x6b0
	void* m_reserved_0x5a4_0x6b8[64] = {};  // some free space at the end, total size is unknown

	GameFramework();

public:
	virtual ~GameFramework();

	static GameFramework* GetInstance();

	////////////////////////////////////////////////////////////////////////////////
	// IGameFramework
	////////////////////////////////////////////////////////////////////////////////

	void RegisterFactory(const char* name, IAnimationStateNodeFactory* (*pCreator)(), bool isAI) override;
	void RegisterFactory(const char* name, ISaveGame* (*pCreator)(), bool isAI) override;
	void RegisterFactory(const char* name, ILoadGame* (*pCreator)(), bool isAI) override;
	void RegisterFactory(const char* name, IActorCreator* pCreator, bool isAI) override;
	void RegisterFactory(const char* name, IItemCreator* pCreator, bool isAI) override;
	void RegisterFactory(const char* name, IVehicleCreator* pCreator, bool isAI) override;
	void RegisterFactory(const char* name, IGameObjectExtensionCreator* pCreator, bool isAI) override;

	bool Init(SSystemInitParams& startupParams) override;
	bool CompleteInit() override;
	bool PreUpdate(bool haveFocus, unsigned int updateFlags) override;
	void PostUpdate(bool haveFocus, unsigned int updateFlags) override;
	void Shutdown() override;
	void Reset(bool clients) override;
	void PauseGame(bool pause, bool force) override;
	bool IsGamePaused() override;
	bool IsGameStarted() override;

	ISystem* GetISystem() override;
	ILanQueryListener* GetILanQueryListener() override;
	IUIDraw* GetIUIDraw() override;
	IGameObjectSystem* GetIGameObjectSystem() override;
	ILevelSystem* GetILevelSystem() override;
	IActorSystem* GetIActorSystem() override;
	IItemSystem* GetIItemSystem() override;
	IActionMapManager* GetIActionMapManager() override;
	IViewSystem* GetIViewSystem() override;
	IGameplayRecorder* GetIGameplayRecorder() override;
	IVehicleSystem* GetIVehicleSystem() override;
	IGameRulesSystem* GetIGameRulesSystem() override;
	IFlowSystem* GetIFlowSystem() override;
	IGameTokenSystem* GetIGameTokenSystem() override;
	IEffectSystem* GetIEffectSystem() override;
	IMaterialEffects* GetIMaterialEffects() override;
	IDialogSystem* GetIDialogSystem() override;
	IPlayerProfileManager* GetIPlayerProfileManager() override;
	IDebrisMgr* GetDebrisMgr() override;
	ISubtitleManager* GetISubtitleManager() override;

	bool StartGameContext(const SGameStartParams* pGameStartParams) override;
	bool ChangeGameContext(const SGameContextParams* pGameContextParams) override;
	void EndGameContext() override;
	bool StartedGameContext() const override;

	bool BlockingSpawnPlayer() override;

	void ResetBrokenGameObjects() override;

	void SetEditorLevel(const char* levelName, const char* levelFolder) override;
	void GetEditorLevel(char** levelName, char** levelFolder) override;

	void BeginLanQuery() override;
	void EndCurrentQuery() override;

	IActor* GetClientActor() const override;
	EntityId GetClientActorId() const override;
	INetChannel* GetClientChannel() const override;
	CTimeValue GetServerTime() override;
	uint16 GetGameChannelId(INetChannel* pNetChannel) override;
	bool IsChannelOnHold(uint16 channelId) override;
	INetChannel* GetNetChannel(uint16 channelId) override;
	IGameObject* GetGameObject(EntityId id) override;

	bool GetNetworkSafeClassId(uint16& id, const char* className) override;
	bool GetNetworkSafeClassName(char* className, size_t maxn, uint16 id) override;

	IGameObjectExtension* QueryGameObjectExtension(EntityId id, const char* name) override;

	bool SaveGame(const char* path, bool quick, bool forceImmediate, ESaveGameReason reason, bool ignoreDelay, const char* checkPoint) override;
	bool LoadGame(const char* path, bool quick, bool ignoreDelay) override;

	void OnEditorSetGameMode(int mode) override;

	bool IsEditing() override;
	bool IsInLevelLoad() override;
	bool IsLoadingSaveGame() override;
	bool IsInTimeDemo() override;

	void AllowSave(bool allow) override;
	void AllowLoad(bool allow) override;
	bool CanSave() override;
	bool CanLoad() override;

	bool CanCheat() override;

	const char* GetLevelName() override;

	const char* GetAbsLevelPath() override;

	IPersistantDebug* GetIPersistantDebug() override;
	IGameStatsConfig* GetIGameStatsConfig() override;

	IAnimationGraphState* GetMusicGraphState() override;
	IMusicLogic* GetMusicLogic() override;

	void RegisterListener(IGameFrameworkListener* pListener, const char* name, EFRAMEWORKLISTENERPRIORITY priority) override;
	void UnregisterListener(IGameFrameworkListener* pListener) override;

	INetNub* GetServerNetNub() override;

	void SetGameGUID(const char* gameGUID) override;
	const char* GetGameGUID() override;
	INetContext* GetNetContext() override;

	void GetMemoryStatistics(ICrySizer*) override;

	void EnableVoiceRecording(const bool enable) override;

	void MutePlayerById(EntityId mutePlayer) override;

	IDebugHistoryManager* CreateDebugHistoryManager() override;

	void DumpMemInfo(const char* format, ...) override;

	bool IsVoiceRecordingEnabled() override;

	bool IsImmersiveMPEnabled() override;

	void ExecuteCommandNextFrame(const char* command) override;

	void ShowPageInBrowser(const char* url) override;

	bool StartProcess(const char* cmdLine) override;

	bool SaveServerConfig(const char* path) override;

	void PrefetchLevelAssets(const bool enforceAll) override;

	bool GetModInfo(SModInfo* modInfo, const char* modPath) override;

	////////////////////////////////////////////////////////////////////////////////

	virtual void ScheduleEndLevel(const char* nextLevel);
	virtual GameServerNub* GetGameServerNub();

	void OnActionEvent(const SActionEvent& event);

	ScriptBind_Vehicle* GetScriptBind_Vehicle() { return m_pScriptBind_Vehicle; }
	ScriptBind_VehicleSeat* GetScriptBind_VehicleSeat() { return m_pScriptBind_VehicleSeat; }

private:
	void InitCVars();
	void InitCommands();
	void InitScriptBinds();

	void CheckEndLevelSchedule();

	template<class Lambda>
	void CallListeners(Lambda lambda)
	{
		*m_pListenersCopy = *m_pListeners;

		for (Listener& listener : *m_pListenersCopy)
		{
			lambda(listener.pListener);
		}
	}
};
