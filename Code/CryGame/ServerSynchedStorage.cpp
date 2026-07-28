#include "CryCommon/CryAction/IGameFramework.h"
#include "CryCommon/CryEntitySystem/IEntitySystem.h"
#include "CryCommon/CrySystem/gEnv.h"

#include "ServerSynchedStorage.h"

CServerSynchedStorage::SChannel *CServerSynchedStorage::GetChannel(int channelId)
{
	auto it = m_channels.find(channelId);
	if (it == m_channels.end())
	{
		return nullptr;
	}

	return &it->second;
}

CServerSynchedStorage::SChannel *CServerSynchedStorage::GetChannel(INetChannel *pNetChannel)
{
	for (auto & item : m_channels)
	{
		if (item.second.pNetChannel == pNetChannel)
		{
			return &item.second;
		}
	}

	return nullptr;
}

int CServerSynchedStorage::GetChannelId(INetChannel *pNetChannel)
{
	for (const auto & item : m_channels)
	{
		if (item.second.pNetChannel == pNetChannel)
		{
			return item.first;
		}
	}

	return 0;
}

void CServerSynchedStorage::Reset()
{
	std::lock_guard lock(m_mutex);

	CSynchedStorage::Reset();

	for (const auto & item : m_channels)
	{
		ResetChannel(item.first);
	}
}

void CServerSynchedStorage::ResetChannel(int channelId)
{
	std::lock_guard lock(m_mutex);

	auto git = m_globalQueue.lower_bound(SChannelQueueEnt(channelId, 0));
	while (git != m_globalQueue.end() && git->first.channel == channelId)
	{
		git = m_globalQueue.erase(git);
	}

	auto eit = m_entityQueue.lower_bound(SChannelEntityQueueEnt(channelId, 0, 0));
	while (eit != m_entityQueue.end() && eit->first.channel == channelId)
	{
		eit = m_entityQueue.erase(eit);
	}

	SChannel *pChannel = GetChannel(channelId);
	if (pChannel && pChannel->pNetChannel)
	{
		auto *pMsg = new CClientSynchedStorage::CResetMsg(channelId, this);

		pChannel->pNetChannel->AddSendable(pMsg, 1, &pChannel->lastOrderedMessage, &pChannel->lastOrderedMessage);
	}
}

void CServerSynchedStorage::DefineProtocol(IProtocolBuilder *pBuilder)
{
	pBuilder->AddMessageSink(this, CClientSynchedStorage::GetProtocolDef(), CServerSynchedStorage::GetProtocolDef());
}

void CServerSynchedStorage::AddToGlobalQueue(TSynchedKey key)
{
	for (const auto & item : m_channels)
	{
		if (!item.second.local)
		{
			AddToGlobalQueueFor(item.first, key);
		}
	}
}

void CServerSynchedStorage::AddToEntityQueue(EntityId entityId, TSynchedKey key)
{
	for (const auto & item : m_channels)
	{
		if (!item.second.local)
		{
			AddToEntityQueueFor(item.first, entityId, key);
		}
	}
}

void CServerSynchedStorage::AddToGlobalQueueFor(int channelId, TSynchedKey key)
{
	SChannel *pChannel = GetChannel(channelId);
	if (!pChannel || !pChannel->pNetChannel || pChannel->local)
	{
		return;
	}

	SSendableHandle & msgHandle = m_globalQueue[SChannelQueueEnt(channelId, key)];

	TSynchedValue value;
	if (!GetGlobalValue(key, value))
	{
		return;
	}

	CClientSynchedStorage::CSetGlobalMsg *pMsg = nullptr;

	switch (static_cast<SynchedValueType>(value.index()))
	{
		case SynchedValueType::Bool:
			pMsg = new CClientSynchedStorage::CSetGlobalBoolMsg(channelId, this, key, value);
			break;
		case SynchedValueType::Float:
			pMsg = new CClientSynchedStorage::CSetGlobalFloatMsg(channelId, this, key, value);
			break;
		case SynchedValueType::Int:
			pMsg = new CClientSynchedStorage::CSetGlobalIntMsg(channelId, this, key, value);
			break;
		case SynchedValueType::EntityId:
			pMsg = new CClientSynchedStorage::CSetGlobalEntityIdMsg(channelId, this, key, value);
			break;
		case SynchedValueType::String:
			pMsg = new CClientSynchedStorage::CSetGlobalStringMsg(channelId, this, key, value);
			break;
	}

	if (pMsg)
	{
		pChannel->pNetChannel->SubstituteSendable(pMsg, 1, &pChannel->lastOrderedMessage, &msgHandle);
	}
}

void CServerSynchedStorage::AddToEntityQueueFor(int channelId, EntityId entityId, TSynchedKey key)
{
	SChannel *pChannel = GetChannel(channelId);
	if (!pChannel || !pChannel->pNetChannel || pChannel->local)
	{
		return;
	}

	SSendableHandle & msgHandle = m_entityQueue[SChannelEntityQueueEnt(channelId, entityId, key)];

	TSynchedValue value;
	if (!GetEntityValue(entityId, key, value))
	{
		return;
	}

	CClientSynchedStorage::CSetEntityMsg *pMsg = nullptr;

	switch (static_cast<SynchedValueType>(value.index()))
	{
		case SynchedValueType::Bool:
			pMsg = new CClientSynchedStorage::CSetEntityBoolMsg(channelId, this, entityId, key, value);
			break;
		case SynchedValueType::Float:
			pMsg = new CClientSynchedStorage::CSetEntityFloatMsg(channelId, this, entityId, key, value);
			break;
		case SynchedValueType::Int:
			pMsg = new CClientSynchedStorage::CSetEntityIntMsg(channelId, this, entityId, key, value);
			break;
		case SynchedValueType::EntityId:
			pMsg = new CClientSynchedStorage::CSetEntityEntityIdMsg(channelId, this, entityId, key, value);
			break;
		case SynchedValueType::String:
			pMsg = new CClientSynchedStorage::CSetEntityStringMsg(channelId, this, entityId, key, value);
			break;
	}

	if (pMsg)
	{
		pChannel->pNetChannel->SubstituteSendable(pMsg, 1, &pChannel->lastOrderedMessage, &msgHandle);
	}
}

//------------------------------------------------------------------------
void CServerSynchedStorage::FullSynch(int channelId, bool reset)
{
	std::lock_guard lock(m_mutex);

	if (reset)
	{
		ResetChannel(channelId);
	}

	for (const auto& [key, value] : m_globalStorage)
	{
		AddToGlobalQueueFor(channelId, key);
	}

	// Remove non-existent entities
	std::erase_if(m_entityStorage, [pEntitySystem = gEnv->pEntitySystem](const auto& item) {
		const auto& [entityId, storage] = item;
		return pEntitySystem->GetEntity(entityId) == nullptr;
	});

	for (const auto& [entityId, storage] : m_entityStorage)
	{
		for (const auto& [key, value] : storage)
		{
			AddToEntityQueueFor(channelId, entityId, key);
		}
	}
}

void CServerSynchedStorage::OnGlobalChanged(TSynchedKey key)
{
	AddToGlobalQueue(key);
}

void CServerSynchedStorage::OnEntityChanged(EntityId entityId, TSynchedKey key)
{
	AddToEntityQueue(entityId, key);
}

void CServerSynchedStorage::OnClientConnect(int channelId)
{
	std::lock_guard lock(m_mutex);

	INetChannel *pNetChannel = m_pGameFramework->GetNetChannel(channelId);

	SChannel *pChannel = GetChannel(channelId);
	if (pChannel && pChannel->onhold)
	{
		pChannel->pNetChannel = pNetChannel;
	}
	else
	{
		if (pChannel)
		{
			m_channels.erase(m_channels.find(channelId));
			// clear up anything in the queues.
			// the reset message won't be sent since we've deleted the channel from the map
			ResetChannel(channelId);
		}

		m_channels[channelId] = SChannel(pNetChannel, pNetChannel->IsLocal());
	}
}

void CServerSynchedStorage::OnClientDisconnect(int channelId, bool onhold)
{
	std::lock_guard lock(m_mutex);

	SChannel *pChannel = GetChannel(channelId);
	if (pChannel)
	{
		pChannel->pNetChannel = nullptr;
	}

	if (pChannel && onhold)
	{
		pChannel->onhold = true;
	}
	else
	{
		m_channels.erase(channelId);
	}

	//m_globalQueue.erase(channelId);
	//m_entityQueue.erase(channelId);

	ResetChannel(channelId);
}

void CServerSynchedStorage::OnClientEnteredGame(int channelId)
{
	std::lock_guard lock(m_mutex);

	SChannel *pChannel = GetChannel(channelId);
	if (pChannel && pChannel->pNetChannel && !pChannel->pNetChannel->IsLocal())
	{
		FullSynch(channelId, true);
	}
}
