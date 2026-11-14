#pragma once

#include <map>
#include <cstdint>

#include "CryCommon/CryAction/IActorSystem.h"

class ActorSystem;

class ActorIterator final : public IActorIterator
{
	int m_refCount = 0;
	ActorSystem* m_pActorSystem = nullptr;
	std::map<EntityId, uintptr_t>::iterator m_iterator;

public:
	explicit ActorIterator(ActorSystem* pActorSystem);

	void Init();

	////////////////////////////////////////////////////////////////////////////////
	// IActorIterator
	////////////////////////////////////////////////////////////////////////////////

	size_t Count() override;
	IActor* Next() override;
	void AddRef() override;
	void Release() override;

	////////////////////////////////////////////////////////////////////////////////
};
