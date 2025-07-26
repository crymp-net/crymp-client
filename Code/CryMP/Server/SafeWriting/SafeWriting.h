#pragma once
#include "CryMP/Server/SSM.h"
#include <memory>

struct IGameFramework;
struct ISystem;
struct SafeWritingAPI;
class FunctionRegisterer;

class CSafeWriting : public ISSM {
    bool m_initialized = false;
    std::shared_ptr<SafeWritingAPI> m_pAPI;
    std::unique_ptr<FunctionRegisterer> m_pFR;
public:
    CSafeWriting(IGameFramework *pFW, ISystem *pSystem);
    ~CSafeWriting();
    void OnGameRulesLoad(IGameRules* pGR) override;
    void OnGameRulesUnload(IGameRules* pGR) override;
    void Update(float dt) override;
    void OnClientConnect(IGameRules *pGR, int channelId, bool isReset) override;
    std::optional<std::string> OnChatMessage(IGameRules* pGR, EChatMessageType type, EntityId sourceId, EntityId targetId, const std::string& msg) override;
    std::optional<std::string> OnPlayerRename(IGameRules *pGR, IActor* pActor, const std::string& name) override;
    bool IsRMILegitimate(INetChannel* pNC, EntityId sourceId) override;
    bool IsHitRMILegitimate(INetChannel* pNC, EntityId sourceId, EntityId weaponId) override;
    bool CanReceiveChatMessage(EChatMessageType type, EntityId sourceId, EntityId targetId) override;
};