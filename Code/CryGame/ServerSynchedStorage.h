#pragma once

#include "ClientSynchedStorage.h"

class CServerSynchedStorage final : public CNetMessageSinkHelper<CServerSynchedStorage, CSynchedStorage>
{
	struct SChannel
	{
		INetChannel *pNetChannel = nullptr;
		SSendableHandle lastOrderedMessage;
		bool local = false;
		bool onhold = false;

		SChannel() = default;

		SChannel(INetChannel *pNetChannel, bool isLocal)
		: pNetChannel(pNetChannel),
		  local(isLocal),
		  onhold(false)
		{
		}
	};

	struct SChannelQueueEnt
	{
		int channel = 0;
		TSynchedKey key = 0;

		SChannelQueueEnt() = default;

		SChannelQueueEnt(int channel, TSynchedKey key)
		: channel(channel),
		  key(key)
		{
		}

		bool operator<(const SChannelQueueEnt & rhs) const
		{
			return rhs.channel < channel || (rhs.channel == channel && rhs.key < key);
		}
	};

	struct SChannelEntityQueueEnt
	{
		int channel = 0;
		EntityId entity = 0;
		TSynchedKey key = 0;

		SChannelEntityQueueEnt() = default;

		SChannelEntityQueueEnt(int channel, EntityId entity, TSynchedKey key)
		: channel(channel),
		  entity(entity),
		  key(key)
		{
		}

		bool operator<(const SChannelEntityQueueEnt& rhs) const
		{
			if (channel < rhs.channel)
				return true;
			else if (channel > rhs.channel)
				return false;
			else if (key < rhs.key)
				return true;
			else if (key > rhs.key)
				return false;
			else if (entity < rhs.entity)
				return true;
			else if (entity > rhs.entity)
				return false;
			else
				return false;
		}
	};

	using TChannelQueueMap = std::map<SChannelQueueEnt, SSendableHandle>;
	using TChannelEntityQueueMap = std::map<SChannelEntityQueueEnt, SSendableHandle>;
	using TChannelMap = std::map<int, SChannel>;

	TChannelQueueMap m_globalQueue;
	TChannelEntityQueueMap m_entityQueue;
	TChannelMap m_channels;

	SChannel *GetChannel(int channelId);
	SChannel *GetChannel(INetChannel *pNetChannel);
	int GetChannelId(INetChannel *pNetChannel);

	void AddToGlobalQueue(TSynchedKey key);
	void AddToEntityQueue(EntityId entityId, TSynchedKey key);

	void AddToGlobalQueueFor(int channelId, TSynchedKey key);
	void AddToEntityQueueFor(int channelId, EntityId entityId, TSynchedKey key);

public:
	explicit CServerSynchedStorage(IGameFramework* pGameFramework)
	{
		m_pGameFramework = pGameFramework;
	}

	void DefineProtocol(IProtocolBuilder *pBuilder) override;

	void Reset() override;
	void ResetChannel(int channelId);

	void FullSynch(int channelId, bool reset);

	void OnClientConnect(int channelId);
	void OnClientDisconnect(int channelId, bool onhold);
	void OnClientEnteredGame(int channelId);

protected:
	void OnGlobalChanged(TSynchedKey key) override;
	void OnEntityChanged(EntityId entityId, TSynchedKey key) override;
};
