#include "SafeWriting.h"

#include "CryAction/ActorSystem.h"
#include "CryAction/GameRulesSystem.h"
#include "CryCommon/CryNetwork/INetwork.h"

#include "CryMP/Server/SafeWriting/APIImpl.h"
#include "CryGame/Game.h"

#include <string>
#include <filesystem>

CSafeWriting::CSafeWriting(IGameFramework *pFW, ISystem* pSystem) {
    CryLog("$6[SafeWriting] Created CSafeWriting");
    m_pAPI = std::make_unique<CSafeWritingAPI>();
    m_pFR = std::make_unique<FunctionRegisterer>(pSystem, pFW, m_pAPI.get());
}

CSafeWriting::~CSafeWriting() {

}

void CSafeWriting::OnGameRulesLoad(IGameRules* pGR) {
    if (!m_initialized) {
        std::filesystem::path root{ gEnv->pSystem->GetRootFolder() };
        root = root.make_preferred();

        std::filesystem::path possiblePaths[] = {
            root / "SafeWriting" / "SafeWritingGameRules.lua",
            root / ".." / "SafeWriting" / "SafeWritingGameRules.lua",
            root / ".." / "Mods" / "SafeWriting" / "Game" / "Scripts" / "ModFiles" / "SafeWritingGameRules.lua",
            root / ".." / "Mods" / "SafeWriting" / "Files" / "SafeWritingGameRules.lua"
        };

        for (auto& path : possiblePaths) {
            if (std::filesystem::exists(path)) {
                CryLogAlways("$6[SafeWriting] Loading SafeWriting from %s", path.parent_path().string().c_str());
                gEnv->pScriptSystem->SetGlobalValue("SAFEWRITING_ROOTDIR", path.parent_path().string().c_str());
                if (gEnv->pScriptSystem->ReloadScript(path.make_preferred().string().c_str())) {
                    m_initialized = true;
                }
                break;
            }
        }
        if (!m_initialized) {
            CryLogAlways("$6[SafeWriting] Failed to locate SafeWriting files, tried looking in following paths: ");
            for (auto& path : possiblePaths) {
                CryLogAlways("$6 %s", path.string().c_str());
            }
        }
    }
}

void CSafeWriting::OnGameRulesUnload(IGameRules* pGR) {

}

void CSafeWriting::Update(float dt) {
    if (m_initialized) {
        IScriptSystem* pSS = gEnv->pScriptSystem;
        if (pSS->BeginCall("SafeWriting_OnUpdate")) {
            pSS->PushFuncParam(dt);
            pSS->EndCall();
        }
    }
}

void CSafeWriting::OnClientConnect(IGameRules *pGR, int channelId, bool isReset) {
    if(INetChannel *pNetChannel=gEnv->pGame->GetIGameFramework()->GetNetChannel(channelId)) {
#ifdef BUILD_64BIT
            int n_ip = *(int*)(((const unsigned char*)pNetChannel) + 0xd0);
#else
            int n_ip = *(int*)(((const unsigned char*)pNetChannel) + 0x78);
#endif
			std::string profileId = std::to_string(pNetChannel->GetProfileId());

			char ip[256];
			std::string hostName { pNetChannel->GetName() };
			if(size_t pos = hostName.find(':'); pos != std::string::npos) {
				hostName = hostName.substr(0, pos);
			}

			snprintf(ip, 255, "%d.%d.%d.%d", (n_ip >> 24) & 255, (n_ip >> 16) & 255, (n_ip >> 8) & 255, (n_ip & 255));

			SmartScriptTable pScript=pGR->GetEntity()->GetScriptTable();
			if(IScriptSystem *pSS = gEnv->pScriptSystem; pSS->BeginCall(pScript, "GatherClientData")) {
				const char *newMessage = NULL;
				pSS->PushFuncParam(pScript);
				pSS->PushFuncParam(channelId);
				pSS->PushFuncParam(pNetChannel->GetName());
				pSS->PushFuncParam(profileId.c_str());
				pSS->PushFuncParam(ip);
				pSS->EndCall();
			}
		}
}

std::optional<std::string> CSafeWriting::OnChatMessage(IGameRules *pGR, EChatMessageType type, EntityId sourceId, EntityId targetId, const std::string& msg) {
    SmartScriptTable pScript=pGR->GetEntity()->GetScriptTable();
    if(IScriptSystem *pSS = gEnv->pScriptSystem; pSS->BeginCall(pScript, "OnChatMessage")) {
        const char *newMessage = NULL;
        pSS->PushFuncParam(pScript);
        pSS->PushFuncParam(type);
        pSS->PushFuncParam(sourceId);
        pSS->PushFuncParam(targetId);
        pSS->PushFuncParam(msg.c_str());
        bool ok = pSS->EndCall(newMessage);
        if(!ok || !newMessage) {
            return {};
        } else {
            return newMessage;
        }
    } else {
        return msg;
    }
}

std::optional<std::string> CSafeWriting::OnPlayerRename(IGameRules *pGR, IActor* pActor, const std::string& name) {
    SmartScriptTable pScript=pGR->GetEntity()->GetScriptTable();
    if(IScriptSystem *pSS = gEnv->pScriptSystem; pSS->BeginCall(pScript, "OnPlayerRename")) {
        const char *newName = NULL;
        pSS->PushFuncParam(pScript);
        pSS->PushFuncParam(pActor->GetEntityId());
        pSS->PushFuncParam(name.c_str());
        bool ok = pSS->EndCall(newName);
        if(!ok || !newName) {
            return {};
        } else {
            return newName;
        }
    } else {
        return name;
    }
}

bool CSafeWriting::IsRMILegitimate(INetChannel* pNC, EntityId sourceId) {
    IGameFramework* pFW = gEnv->pGame->GetIGameFramework();
    IActorSystem* pAS = pFW->GetIActorSystem();
    IActor* client = pAS->GetActorByChannelId(pFW->GetGameChannelId(pNC));
    IActor* claimsToBe = pAS->GetActor(sourceId);
    return client != NULL && client == claimsToBe;
}

bool CSafeWriting::IsHitRMILegitimate(INetChannel* pNC, EntityId sourceId, EntityId weaponId) {
    if (!IsRMILegitimate(pNC, sourceId)) {
        IEntity* pEntity = gEnv->pSystem->GetIEntitySystem()->GetEntity(weaponId);
        if (!pEntity || !strcmp(pEntity->GetClass()->GetName(), "AACannon")) {
            return false;
        } else {
            return true;
        }
    } else {
        return true;
    }
}

bool CSafeWriting::CanReceiveChatMessage(EChatMessageType type, EntityId sourceId, EntityId targetId) {
    return true;
}