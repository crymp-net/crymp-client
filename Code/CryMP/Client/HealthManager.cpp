#include "HealthManager.h"
#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CrySystem/IConsole.h"

#include "CryCommon/CryAction/IActorSystem.h"
#include "CryCommon/CryAction/IVehicleSystem.h"
#include "CryCommon/CryAction/IMovementController.h"
#include "CryCommon/CrySystem/ICryPak.h"

#include "CrySystem/RandomGenerator.h"

#include "CryGame/Actors/Actor.h"
#include "CryGame/Game.h"
#include "CryGame/GameCVars.h"
#include "CryGame/Actors/Player/Player.h"
#include "CryGame/GameRules.h"

#define HEALTH_KEY			901

CHealthManager::CHealthManager() {

}

CHealthManager::~CHealthManager() {

}

bool CHealthManager::IsActive() {
	CGameRules* pGR = g_pGame->GetGameRules();
	CActor* pLocalActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());
	return pGR && pLocalActor;
}

int CHealthManager::GetHealth(IActor* iActor) {
	int health = 0;
	float fHealth = 0.0f;
	CSynchedStorage* pSSS = g_pGame->GetSynchedStorage();
	CActor* pActor = static_cast<CActor*>(iActor);
	IActor* pLocalActor = g_pGame->GetIGameFramework()->GetClientActor();

	if (pLocalActor == pActor) {
		health = pActor->GetHealth();
	} else if (pActor->GetHealth() <= 0) {
		health = 0;
	} else if (pSSS && pSSS->GetEntityValue(iActor->GetEntityId(), HEALTH_KEY, health)) {
		health = health;
	} else if (pSSS && pSSS->GetEntityValue(iActor->GetEntityId(), HEALTH_KEY, fHealth)) {
		health = (int)fHealth;
	} else {
		auto it = m_lastHealth.find(iActor);
		if (it != m_lastHealth.end()) {
			health = it->second;
		} else {
			health = 100;
		}
	}
	m_lastHealth[iActor] = health;
	return health;
}

void CHealthManager::Reset(bool quit) {
	m_lastHealth.clear();
}
