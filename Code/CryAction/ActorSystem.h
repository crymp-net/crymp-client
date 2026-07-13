#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

#include "CryCommon/CryAction/IActorSystem.h"

#include "ActorIterator.h"

struct ISystem;
struct IEntitySystem;

class ActorSystem final : public IActorSystem
{
	EntityId m_originalDemoSpectator = 0;  // m_reserved_0x4_0x8
	EntityId m_currentDemoSpectator = 0;  // m_reserved_0x8_0xc
	EntityId m_demoPlaybackMappedOriginalServerPlayer = 0;  // m_reserved_0xc_0x10
	ISystem* m_pSystem = nullptr;  // m_reserved_0x10_0x18
	IEntitySystem* m_pEntitySystem = nullptr;  // m_reserved_0x14_0x20
	std::map<EntityId, uintptr_t> m_actors;  // m_reserved_0x18_0x28
	std::map<std::string, IGameFramework::IActorCreator*, std::less<void>> m_classes;  // m_reserved_0x30_0x58
	std::vector<std::unique_ptr<ActorIterator>> m_iteratorPool;  // m_reserved_0x48_0x88

	friend class ActorIterator;

public:
	explicit ActorSystem(ISystem* pSystem, IEntitySystem* pEntitySystem);
	virtual ~ActorSystem();

	void RegisterActorClass(const char* name, IGameFramework::IActorCreator* pCreator, bool isAI);

	////////////////////////////////////////////////////////////////////////////////
	// IActorSystem
	////////////////////////////////////////////////////////////////////////////////

	void Reset() override;
	IActor* GetActor(EntityId entityId) override;
	IActor* GetActorByChannelId(std::uint16_t channelId) override;
	IActor* CreateActor(
		std::uint16_t channelId,
		const char* name,
		const char* actorClass,
		const Vec3& pos,
		const Quat& rot,
		const Vec3& scale,
		EntityId id) override;

	int GetActorCount() const override;
	IActorIteratorPtr CreateActorIterator() override;

	void SetDemoPlaybackMappedOriginalServerPlayer(EntityId id) override;
	EntityId GetDemoPlaybackMappedOriginalServerPlayer() const override;
	void SwitchDemoSpectator(EntityId id) override;
	IActor* GetCurrentDemoSpectator() override;
	IActor* GetOriginalDemoSpectator() override;

	void AddActor(EntityId entityId, IActor* pActor) override;
	void RemoveActor(EntityId entityId) override;

	IActor* DecodeActor(uintptr_t actorRef) {
		return (IActor*)(actorRef ^ (uintptr_t)this);
	}

	uintptr_t EncodeActor(IActor* pActor) {
		return ((uintptr_t)pActor) ^ ((uintptr_t)this);
	}

	////////////////////////////////////////////////////////////////////////////////

private:
	static bool CreateActorHook(IEntity* pEntity, IGameObject* pGameObject, void* pUserData);
};
