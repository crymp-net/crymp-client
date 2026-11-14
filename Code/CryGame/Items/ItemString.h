#pragma once

#include "CryCommon/CryCore/CrySizer.h"
#include "CrySystem/CryLog.h"

#include <string>
#include <string_view>
#include <unordered_set>

class ItemString
{
	class NameTable
	{
		struct Hash
		{
			using is_transparent = void;

			template<typename T>
			std::size_t operator()(const T& name) const
			{
				return std::hash<T>{}(name);
			}
		};

		struct KeyEqual
		{
			using is_transparent = void;

			bool operator()(const auto& a, const auto& b) const
			{
				return a == b;
			}
		};

		std::unordered_set<std::string, Hash, KeyEqual> m_table;

	public:
		NameTable() = default;

		std::string_view GetEntry(const std::string_view& name);

		void DumpToLog();
	};

	static NameTable& GetNameTable()
	{
		static NameTable table;
		return table;
	}

	std::string_view m_name;

public:
	ItemString()
	{
		clear();
	}

	explicit ItemString(const char* name)
	{
		m_name = GetNameTable().GetEntry(name ? name : "");
	}

	ItemString& operator=(const char* name)
	{
		m_name = GetNameTable().GetEntry(name ? name : "");

		return *this;
	}

	bool operator==(const ItemString& other) const
	{
		return m_name.data() == other.m_name.data();
	}

	bool operator!=(const ItemString& other) const
	{
		return !(*this == other);
	}

	bool operator==(const char* name) const
	{
		return name && std::string_view(name) == m_name;
	}

	bool operator!=(const char* name) const
	{
		return !(*this == name);
	}

	bool operator<(const ItemString& other) const
	{
		// NOTE: this is broken on purpose for maximum performance
		return m_name.data() < other.m_name.data();
	}

	bool empty() const
	{
		return m_name.empty();
	}

	void clear()
	{
		m_name = GetNameTable().GetEntry("");
	}

	const char* c_str() const
	{
		return m_name.data();
	}

	std::string_view sv() const {
		return m_name;
	}

	std::size_t length() const
	{
		return m_name.length();
	}

	static void DumpNameTableToLog()
	{
		GetNameTable().DumpToLog();
	}
};

template<>
inline bool ICrySizer::Add<ItemString>(const ItemString& object)
{
	return AddObject(object.c_str(), object.length());
}
