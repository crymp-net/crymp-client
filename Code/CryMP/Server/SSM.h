#pragma once
#include <string>
#include <optional>

#include "CryCommon/CryEntitySystem/EntityId.h"

struct IActor;
struct IGameRules;
struct INetChannel;
enum EChatMessageType;

struct ISSM {
public:
    ~ISSM() = default;
    virtual void OnGameRulesLoad(IGameRules * pGR) = 0;
    virtual void OnGameRulesUnload(IGameRules* pGR) = 0;
    virtual void Update(float dt) = 0;
    virtual void OnClientConnect(IGameRules *pGR, int channelId, bool isReset) = 0;
    virtual std::optional<std::string> OnChatMessage(IGameRules *pGR, EChatMessageType type, EntityId sourceId, EntityId targetId, const std::string& msg) = 0;
    virtual std::optional<std::string> OnPlayerRename(IGameRules *pGR, IActor* pActor, const std::string& name) = 0;
    virtual bool IsRMILegitimate(INetChannel* pNC, EntityId sourceId) = 0;
    virtual bool IsHitRMILegitimate(INetChannel* pNC, EntityId sourceId, EntityId weaponId) = 0;
    virtual bool CanReceiveChatMessage(EChatMessageType type, EntityId sourceId, EntityId targetId) = 0;
};