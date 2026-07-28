#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <variant>

#include "CryCommon/CryNetwork/INetwork.h"

struct IGameFramework;

using TSynchedKey = std::uint16_t;
using TSynchedValue = std::variant<bool, float, int, EntityId, std::string>;

enum class SynchedValueType
{
	None = -1,
	Bool = 0,
	Float = 1,
	Int = 2,
	EntityId = 3,
	String = 4,
};

static_assert(static_cast<SynchedValueType>(TSynchedValue(bool{}).index()) == SynchedValueType::Bool);
static_assert(static_cast<SynchedValueType>(TSynchedValue(float{}).index()) == SynchedValueType::Float);
static_assert(static_cast<SynchedValueType>(TSynchedValue(int{}).index()) == SynchedValueType::Int);
static_assert(static_cast<SynchedValueType>(TSynchedValue(EntityId{}).index()) == SynchedValueType::EntityId);
static_assert(static_cast<SynchedValueType>(TSynchedValue(std::string{}).index()) == SynchedValueType::String);

class CSynchedStorage : public INetMessageSink
{
protected:
	using TStorage = std::map<TSynchedKey, TSynchedValue>;
	using TEntityStorageMap = std::map<EntityId, TStorage>;

	TStorage m_globalStorage;
	TEntityStorageMap m_entityStorage;

	IGameFramework* m_pGameFramework = nullptr;

	std::recursive_mutex m_mutex;

	CSynchedStorage() = default;

public:
	virtual ~CSynchedStorage() = default;

	template<class T>
	void SetGlobalValue(TSynchedKey key, const T& value)
	{
		std::lock_guard lock(m_mutex);

		const auto [it, added] = m_globalStorage.try_emplace(key, value);
		if (added)
		{
			OnGlobalChanged(key);
		}
		else
		{
			const T* pStoredValue = std::get_if<T>(&it->second);
			if (!pStoredValue || *pStoredValue != value)
			{
				it->second = value;

				OnGlobalChanged(key);
			}
		}
	}

	template<class T>
	void SetEntityValue(EntityId id, TSynchedKey key, const T& value)
	{
		std::lock_guard lock(m_mutex);

		TStorage& storage = m_entityStorage[id];

		const auto [it, added] = storage.try_emplace(key, value);
		if (added)
		{
			OnEntityChanged(id, key);
		}
		else
		{
			const T* pStoredValue = std::get_if<T>(&it->second);
			if (!pStoredValue || *pStoredValue != value)
			{
				it->second = value;

				OnEntityChanged(id, key);
			}
		}
	}

	template<class T>
	bool GetGlobalValue(TSynchedKey key, T& value)
	{
		std::lock_guard lock(m_mutex);

		auto it = m_globalStorage.find(key);
		if (it == m_globalStorage.end())
		{
			return false;
		}

		const T* pStoredValue = std::get_if<T>(&it->second);
		if (!pStoredValue)
		{
			return false;
		}

		value = *pStoredValue;

		return true;
	}

	bool GetGlobalValue(TSynchedKey key, TSynchedValue& value)
	{
		std::lock_guard lock(m_mutex);

		auto it = m_globalStorage.find(key);
		if (it == m_globalStorage.end())
		{
			return false;
		}

		value = it->second;

		return true;
	}

	template<class T>
	bool GetEntityValue(EntityId entityId, TSynchedKey key, T& value)
	{
		std::lock_guard lock(m_mutex);

		auto eit = m_entityStorage.find(entityId);
		if (eit == m_entityStorage.end())
		{
			return false;
		}

		auto it = eit->second.find(key);
		if (it == eit->second.end())
		{
			return false;
		}

		const T* pStoredValue = std::get_if<T>(&it->second);
		if (!pStoredValue)
		{
			return false;
		}

		value = *pStoredValue;

		return true;
	}

	bool GetEntityValue(EntityId entityId, TSynchedKey key, TSynchedValue& value)
	{
		std::lock_guard lock(m_mutex);

		auto eit = m_entityStorage.find(entityId);
		if (eit == m_entityStorage.end())
		{
			return false;
		}

		auto it = eit->second.find(key);
		if (it == eit->second.end())
		{
			return false;
		}

		value = it->second;

		return true;
	}

	SynchedValueType GetGlobalValueType(TSynchedKey key)
	{
		std::lock_guard lock(m_mutex);

		auto it = m_globalStorage.find(key);
		if (it == m_globalStorage.end())
		{
			return SynchedValueType::None;
		}

		return static_cast<SynchedValueType>(it->second.index());
	}

	SynchedValueType GetEntityValueType(EntityId id, TSynchedKey key)
	{
		std::lock_guard lock(m_mutex);

		auto eit = m_entityStorage.find(id);
		if (eit == m_entityStorage.end())
		{
			return SynchedValueType::None;
		}

		auto it = eit->second.find(key);
		if (it == eit->second.end())
		{
			return SynchedValueType::None;
		}

		return static_cast<SynchedValueType>(it->second.index());
	}

	void ClearGlobalValue(TSynchedKey key)
	{
		std::lock_guard lock(m_mutex);

		if (m_globalStorage.erase(key))
		{
			OnGlobalChanged(key);
		}
	}

	void ClearEntityValue(EntityId id, TSynchedKey key)
	{
		std::lock_guard lock(m_mutex);

		auto entityIt = m_entityStorage.find(id);
		if (entityIt != m_entityStorage.end())
		{
			TStorage& storage = entityIt->second;

			if (storage.erase(key))
			{
				OnEntityChanged(id, key);
			}

			if (storage.empty())
			{
				m_entityStorage.erase(entityIt);
			}
		}
	}

	virtual void Reset();

	virtual void Dump();

	void SerializeValue(TSerialize ser, TSynchedKey& key, TSynchedValue& value, SynchedValueType type);
	void SerializeEntityValue(TSerialize ser, EntityId id, TSynchedKey& key, TSynchedValue& value, SynchedValueType type);

protected:
	virtual void OnGlobalChanged(TSynchedKey key)
	{
	}

	virtual void OnEntityChanged(EntityId id, TSynchedKey key)
	{
	}
};
