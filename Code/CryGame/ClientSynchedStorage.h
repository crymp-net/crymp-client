#pragma once

#include "CryCommon/CrySystem/ISystem.h"  // required by NetHelpers.h
#include "CryCommon/CryNetwork/NetHelpers.h"

#include "SynchedStorage.h"

#define DECLARE_GLOBAL_MESSAGE(classname) \
class classname: public CSetGlobalMsg \
	{ \
	public: \
	classname(int _channelId, CServerSynchedStorage *pStorage, TSynchedKey _key, TSynchedValue &_value); \
	EMessageSendResult WritePayload(TSerialize ser, uint32 currentSeq, uint32 basisSeq); \
	}; \

#define DECLARE_ENTITY_MESSAGE(classname) \
	class classname: public CSetEntityMsg \
	{ \
		public: \
		classname(int _channelId, CServerSynchedStorage *pStorage, EntityId id, TSynchedKey _key, TSynchedValue &_value); \
		EMessageSendResult WritePayload(TSerialize ser, uint32 currentSeq, uint32 basisSeq); \
	}; \

class CServerSynchedStorage;

class CClientSynchedStorage final : public CNetMessageSinkHelper<CClientSynchedStorage, CSynchedStorage>
{
public:
	explicit CClientSynchedStorage(IGameFramework* pGameFramework)
	{
		m_pGameFramework = pGameFramework;
	}

	void DefineProtocol(IProtocolBuilder *pBuilder) override;

	class CResetMsg: public INetMessage
	{
	public:
		CResetMsg(int _channelId, CServerSynchedStorage *pStorage);

		int channelId;
		CServerSynchedStorage *m_pStorage;

		EMessageSendResult WritePayload(TSerialize ser, uint32 currentSeq, uint32 basisSeq);
		void UpdateState(uint32 fromSeq, ENetSendableStateUpdate update);
		size_t GetSize();
	};

	class CSetGlobalMsg: public INetMessage
	{
	public:
		CSetGlobalMsg(const SNetMessageDef *pDef, int _channelId, CServerSynchedStorage *pStorage, TSynchedKey _key, TSynchedValue &_value);

		int channelId;
		CServerSynchedStorage *m_pStorage;

		TSynchedKey key;
		TSynchedValue value;

		EMessageSendResult WritePayload(TSerialize ser, uint32 currentSeq, uint32 basisSeq) override;
		void UpdateState(uint32 fromSeq, ENetSendableStateUpdate update) override;
		size_t GetSize() override;
	};

	DECLARE_GLOBAL_MESSAGE(CSetGlobalBoolMsg);
	DECLARE_GLOBAL_MESSAGE(CSetGlobalFloatMsg);
	DECLARE_GLOBAL_MESSAGE(CSetGlobalIntMsg);
	DECLARE_GLOBAL_MESSAGE(CSetGlobalEntityIdMsg);
	DECLARE_GLOBAL_MESSAGE(CSetGlobalStringMsg);

	class CSetEntityMsg: public INetMessage
	{
	public:
		CSetEntityMsg(const SNetMessageDef *pDef, int _channelId, CServerSynchedStorage *pStorage, EntityId id, TSynchedKey _key, TSynchedValue &_value);

		int channelId;
		CServerSynchedStorage *m_pStorage;

		EntityId entityId;
		TSynchedKey key;
		TSynchedValue value;

		EMessageSendResult WritePayload(TSerialize ser, uint32 currentSeq, uint32 basisSeq) override;
		void UpdateState(uint32 fromSeq, ENetSendableStateUpdate update) override;
		size_t GetSize() override;
	};

	DECLARE_ENTITY_MESSAGE(CSetEntityBoolMsg);
	DECLARE_ENTITY_MESSAGE(CSetEntityFloatMsg);
	DECLARE_ENTITY_MESSAGE(CSetEntityIntMsg);
	DECLARE_ENTITY_MESSAGE(CSetEntityEntityIdMsg);
	DECLARE_ENTITY_MESSAGE(CSetEntityStringMsg);

	NET_DECLARE_IMMEDIATE_MESSAGE(ResetMsg);

	NET_DECLARE_IMMEDIATE_MESSAGE(SetGlobalBoolMsg);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetGlobalFloatMsg);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetGlobalIntMsg);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetGlobalEntityIdMsg);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetGlobalStringMsg);

	// only for compatibility with vanilla servers
	NET_DECLARE_IMMEDIATE_MESSAGE(SetChannelBoolMsg);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetChannelFloatMsg);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetChannelIntMsg);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetChannelEntityIdMsg);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetChannelStringMsg);

	NET_DECLARE_IMMEDIATE_MESSAGE(SetEntityBoolMsg);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetEntityFloatMsg);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetEntityIntMsg);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetEntityEntityIdMsg);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetEntityStringMsg);
};

#undef DECLARE_GLOBAL_MESSAGE
#undef DECLARE_ENTITY_MESSAGE
