#include "CryCommon/CryAction/IViewSystem.h"
#include "CryCommon/CrySoundSystem/ISound.h"
#include "Library/PathTools.h"

#include "ActorSystem.h"
#include "GameFramework.h"
#include "GameServerNub.h"
#include "GameServerChannel.h"

ActorSystem::ActorSystem(ISystem* pSystem, IEntitySystem* pEntitySystem)
: m_pSystem(pSystem), m_pEntitySystem(pEntitySystem)
{
}

ActorSystem::~ActorSystem()
{
}

void ActorSystem::RegisterActorClass(const char* name, IGameFramework::IActorCreator* pCreator, bool isAI)
{
	const char* actorName = PathTools::FileName(name).data();

	std::string scriptPath(isAI ? "Scripts/Entities/AI/" : "Scripts/Entities/Actor/");
	scriptPath += name;
	scriptPath += ".lua";

	IEntityClassRegistry::SEntityClassDesc entityClass;
	entityClass.flags = 0;
	entityClass.sName = actorName;
	entityClass.sScriptFile = scriptPath.c_str();

	if (!isAI)
	{
		entityClass.flags |= ECLF_INVISIBLE;
	}

	GameFramework::GetInstance()->GetIGameObjectSystem()->RegisterExtension(actorName, pCreator, &entityClass);

	m_classes[name] = pCreator;
}

////////////////////////////////////////////////////////////////////////////////
// IActorSystem
////////////////////////////////////////////////////////////////////////////////

void ActorSystem::Reset()
{
	if (m_pSystem->IsSerializingFile() != 1)
	{
		return;
	}

	std::erase_if(m_actors, [this](const auto& actor) {
		return m_pEntitySystem->GetEntity(actor.first) == nullptr;
	});
}

IActor* ActorSystem::GetActor(EntityId entityId)
{
	const auto it = m_actors.find(entityId);
	if (it == m_actors.end())
	{
		return nullptr;
	}

	return DecodeActor(it->second);
}

IActor* ActorSystem::GetActorByChannelId(std::uint16_t channelId)
{
	for (const auto& [id, pActorRef] : m_actors)
	{
		IActor* pActor = DecodeActor(pActorRef);
		if (pActor->GetChannelId() == channelId)
		{
			return pActor;
		}
	}

	return nullptr;
}

IActor* ActorSystem::CreateActor(
	std::uint16_t channelId,
	const char* name,
	const char* actorClass,
	const Vec3& pos,
	const Quat& rot,
	const Vec3& scale,
	EntityId id)
{
	IEntityClass* pEntityClass = m_pEntitySystem->GetClassRegistry()->FindClass(actorClass);
	if (!pEntityClass)
	{
		return nullptr;
	}

	GameServerNub* pGameServerNub = GameFramework::GetInstance()->GetGameServerNub();
	if (pGameServerNub)
	{
		GameServerChannel* pGameServerChannel = pGameServerNub->GetChannel(channelId);
		if (pGameServerChannel && pGameServerChannel->GetNetChannel()->IsLocal())
		{
			id = LOCAL_PLAYER_ENTITY_ID;

			INetContext* pNetContext = GameFramework::GetInstance()->GetNetContext();
			if (pNetContext && pNetContext->IsDemoPlayback())
			{
				m_originalDemoSpectator = id;
				m_currentDemoSpectator = id;
			}
		}
	}

	IGameObjectSystem::SEntitySpawnParamsForGameObjectWithPreactivatedExtension userData;
	userData.hookFunction = &ActorSystem::CreateActorHook;
	userData.pUserData = &channelId;

	SEntitySpawnParams params;
	params.id = id;
	params.pClass = pEntityClass;
	params.sName = name;
	params.nFlags = ENTITY_FLAG_TRIGGER_AREAS;
	params.vPosition = pos;
	params.qRotation = rot;
	params.vScale = scale;
	params.pUserData = &userData;

	if (channelId)
	{
		params.nFlags |= ENTITY_FLAG_NEVER_NETWORK_STATIC;
	}

	IEntity* pEntity = m_pEntitySystem->SpawnEntity(params, true);
	if (!pEntity)
	{
		return nullptr;
	}

	return this->GetActor(pEntity->GetId());
}

int ActorSystem::GetActorCount() const
{
	return static_cast<int>(m_actors.size());
}

IActorIteratorPtr ActorSystem::CreateActorIterator()
{
	if (m_iteratorPool.empty())
	{
		m_iteratorPool.emplace_back(std::make_unique<ActorIterator>(this));
	}

	std::unique_ptr<ActorIterator> iterator(std::move(m_iteratorPool.back()));
	m_iteratorPool.pop_back();

	iterator->Init();

	return IActorIteratorPtr(iterator.release());
}

void ActorSystem::SetDemoPlaybackMappedOriginalServerPlayer(EntityId id)
{
	m_demoPlaybackMappedOriginalServerPlayer = id;
}

EntityId ActorSystem::GetDemoPlaybackMappedOriginalServerPlayer() const
{
	return m_demoPlaybackMappedOriginalServerPlayer;
}

void ActorSystem::SwitchDemoSpectator(EntityId id)
{
	if (m_actors.size() <= 1)
	{
		return;
	}

	IActor* nextActor = nullptr;

	if (id)
	{
		auto it = m_actors.find(id);
		if (it == m_actors.end())
		{
			return;
		}

		nextActor = DecodeActor(it->second);
	}
	else
	{
		auto it = m_actors.find(m_currentDemoSpectator);
		if (it == m_actors.end())
		{
			return;
		}

		++it;
		if (it == m_actors.end())
		{
			it = m_actors.begin();
		}

		nextActor = DecodeActor(it->second);
	}

	IActor* lastActor = nullptr;

	{
		auto it = m_actors.find(m_currentDemoSpectator);
		if (it == m_actors.end())
		{
			return;
		}

		lastActor = DecodeActor(it->second);
	}

	if (nextActor == lastActor)
	{
		return;
	}

	m_currentDemoSpectator = nextActor->GetEntityId();

	IView* pView = GameFramework::GetInstance()->GetIViewSystem()->GetActiveView();
	if (pView)
	{
		pView->LinkTo(nextActor->GetEntity());
	}

	nextActor->SwitchDemoModeSpectator(true);
	lastActor->SwitchDemoModeSpectator(false);

	ISoundSystem* pSoundSystem = gEnv->pSoundSystem;
	if (pSoundSystem)
	{
		pSoundSystem->SetListenerEntity(LISTENERID_STANDARD, m_currentDemoSpectator);
	}
}

IActor* ActorSystem::GetCurrentDemoSpectator()
{
	return this->GetActor(m_currentDemoSpectator);
}

IActor* ActorSystem::GetOriginalDemoSpectator()
{
	return this->GetActor(m_originalDemoSpectator);
}

void ActorSystem::AddActor(EntityId entityId, IActor* pActor)
{
	m_actors[entityId] = EncodeActor(pActor);
}

void ActorSystem::RemoveActor(EntityId entityId)
{
	m_actors.erase(entityId);
}

////////////////////////////////////////////////////////////////////////////////

bool ActorSystem::CreateActorHook(IEntity* pEntity, IGameObject* pGameObject, void* pUserData)
{
	pGameObject->SetChannelId(*static_cast<std::uint16_t*>(pUserData));
	return true;
}
