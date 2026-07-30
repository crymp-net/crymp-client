#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CryEntitySystem/IEntitySystem.h"

#include "SynchedStorage.h"

struct DumpVisitor
{
	TSynchedKey key{};

	explicit DumpVisitor(TSynchedKey key) : key(key) {}

	void operator()(bool v)               { CryLogAlways("  %.08u -     bool: %s", key, v ? "true" : "false"); }
	void operator()(float v)              { CryLogAlways("  %.08u -    float: %f", key, v); }
	void operator()(int v)                { CryLogAlways("  %.08u -      int: %d", key, v); }
	void operator()(EntityId v)           { CryLogAlways("  %.08u - entityId: %u", key, v); }
	void operator()(const std::string& v) { CryLogAlways("  %.08u -   string: %s", key, v.c_str()); }
};

void CSynchedStorage::Reset()
{
	std::lock_guard lock(m_mutex);

	m_globalStorage.clear();
	m_entityStorage.clear();
}

void CSynchedStorage::Dump()
{
	std::lock_guard lock(m_mutex);

	CryLogAlways("-------------------------------- SynchedStorage --------------------------------");
	CryLogAlways("Globals:");

	for (const auto& [key, value] : m_globalStorage)
	{
		std::visit(DumpVisitor{key}, value);
	}

	for (const auto& [entityId, storage] : m_entityStorage)
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId);
		const char* name = pEntity ? pEntity->GetName() : "null";

		CryLogAlways("Entity %u (%s):", entityId, name);

		for (const auto& [key, value] : storage)
		{
			std::visit(DumpVisitor{key}, value);
		}
	}

	CryLogAlways("--------------------------------------------------------------------------------");
}

void CSynchedStorage::SerializeValue(TSerialize ser, TSynchedKey& key, TSynchedValue& value, SynchedValueType type)
{
	ser.Value("key", key, /* 'ssk' */ 0x0073736B);

	const auto impl = [&]<class T>(int policy) {
		T v{};

		if (ser.IsWriting())
		{
			T* p = std::get_if<T>(&value);
			if (p)
			{
				v = *p;
			}
		}

		ser.Value("value", v, policy);

		if (ser.IsReading())
		{
			SetGlobalValue(key, v);
		}
	};

	switch (type)
	{
		case SynchedValueType::Bool:
		{
			impl.operator()<bool>(/* 'bool' */ 0x626F6F6C);
			break;
		}
		case SynchedValueType::Float:
		{
			impl.operator()<float>(/* 'ssfl' */ 0x7373666C);
			break;
		}
		case SynchedValueType::Int:
		{
			impl.operator()<int>(/* 'ssi' */ 0x00737369);
			break;
		}
		case SynchedValueType::EntityId:
		{
			impl.operator()<EntityId>(/* 'eid' */ 0x00656964);
			break;
		}
		case SynchedValueType::String:
		{
			impl.operator()<std::string>(0);
			break;
		}
	}
}

void CSynchedStorage::SerializeEntityValue(TSerialize ser, EntityId id, TSynchedKey& key, TSynchedValue& value, SynchedValueType type)
{
	ser.Value("key", key, /* 'ssk' */ 0x0073736B);

	const auto impl = [&]<class T>(int policy) {
		T v{};

		if (ser.IsWriting())
		{
			T* p = std::get_if<T>(&value);
			if (p)
			{
				v = *p;
			}
		}

		ser.Value("value", v, policy);

		if (ser.IsReading())
		{
			SetEntityValue(id, key, v);
		}
	};

	switch (type)
	{
		case SynchedValueType::Bool:
		{
			impl.operator()<bool>(/* 'bool' */ 0x626F6F6C);
			break;
		}
		case SynchedValueType::Float:
		{
			impl.operator()<float>(/* 'ssfl' */ 0x7373666C);
			break;
		}
		case SynchedValueType::Int:
		{
			impl.operator()<int>(/* 'ssi' */ 0x00737369);
			break;
		}
		case SynchedValueType::EntityId:
		{
			impl.operator()<EntityId>(/* 'eid' */ 0x00656964);
			break;
		}
		case SynchedValueType::String:
		{
			impl.operator()<std::string>(0);
			break;
		}
	}
}
