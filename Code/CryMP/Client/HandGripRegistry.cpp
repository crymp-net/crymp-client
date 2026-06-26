#include "CryCommon/Cry3DEngine/IStatObj.h"
#include "CryCommon/CryEntitySystem/IEntitySystem.h"
#include "Library/StringTools.h"

#include "HandGripRegistry.h"

HandGripRegistry::HandGripRegistry()
{
}

HandGripRegistry::~HandGripRegistry()
{
}

void HandGripRegistry::SetGripLeft(std::string_view key, const Vec3& leftEL)
{
#ifdef __cpp_lib_associative_heterogeneous_insertion
	HandGripInfo& e = m_grips[key];
#else
	HandGripInfo& e = m_grips[std::string(key)];
#endif

	e.leftEL = leftEL;
	e.hasLeft = true;
}

void HandGripRegistry::SetGripRight(std::string_view key, const Vec3& rightEL)
{
#ifdef __cpp_lib_associative_heterogeneous_insertion
	HandGripInfo& e = m_grips[key];
#else
	HandGripInfo& e = m_grips[std::string(key)];
#endif

	e.rightEL = rightEL;
	e.hasRight = true;
}

void HandGripRegistry::SetGripHoldObjectOffset(std::string_view key, const Vec3& fpOffset, const Vec3& tpOffset)
{
#ifdef __cpp_lib_associative_heterogeneous_insertion
	HandGripInfo& e = m_grips[key];
#else
	HandGripInfo& e = m_grips[std::string(key)];
#endif

	e.posOffset_FP = fpOffset;
	e.posOffset_TP = tpOffset;
}

HandGripInfo* HandGripRegistry::GetGripByKey(std::string_view key)
{
	const auto it = m_grips.find(key);
	return (it != m_grips.end()) ? &it->second : nullptr;
}

HandGripInfo* HandGripRegistry::GetGripByEntity(IEntity* e)
{
	if (!e)
	{
		return nullptr;
	}

	const char* keyCStr = nullptr;

	IStatObj* so = e->GetStatObj(0);
	if (so)
	{
		const char* p = so->GetFilePath();
		if (p && *p)
		{
			keyCStr = p;
		}
	}

	if (!keyCStr)
	{
		const IEntityClass* c = e->GetClass();
		if (c)
		{
			const char* n = c->GetName();
			if (n && *n)
			{
				keyCStr = n;
			}
		}
	}

	if (!keyCStr)
	{
		return nullptr;
	}

	return GetGripByKey(std::string_view(keyCStr));
}

bool HandGripRegistry::RemoveGripByKey(std::string_view key)
{
#ifdef __cpp_lib_associative_heterogeneous_erasure
	return m_grips.erase(key) > 0;
#else
	return m_grips.erase(std::string(key)) > 0;
#endif
}

bool HandGripRegistry::RemoveGripForEntity(IEntity* e)
{
	if (!e)
	{
		return false;
	}

	// Build key: prefer CGF path (slot 0), else entity class name.
	const char* keyCStr = nullptr;

	IStatObj* so = e->GetStatObj(0);
	if (so)
	{
		const char* p = so->GetFilePath();
		if (p && *p)
		{
			keyCStr = p;
		}
	}

	if (!keyCStr)
	{
		const IEntityClass* c = e->GetClass();
		if (c)
		{
			const char* n = c->GetName();
			if (n && *n)
			{
				keyCStr = n;
			}
		}
	}

	if (!keyCStr)
	{
		return false;
	}

	return RemoveGripByKey(std::string_view(keyCStr));
}

void HandGripRegistry::ClearGripRegistry()
{
	m_grips.clear();
}

std::string HandGripRegistry::SerializeToLuaScript() const
{
	std::string script;
	script.reserve(4095);

	const auto addLine = [&script](const char* format, auto... args)
	{
		StringTools::FormatTo(script, format, args...);
		script += '\n';
	};

	addLine("CPPAPI.CreateHandGripData({");

	for (const auto& [key, grip] : m_grips)
	{
		addLine("  {");
		addLine("    key  = \"%s\",", key.c_str());
		addLine("    hasL = %s,", grip.hasLeft ? "true" : "false");
		addLine("    hasR = %s,", grip.hasRight ? "true" : "false");

		if (grip.hasLeft)
		{
			addLine("    L = {");
			addLine("      x = %.6f,", grip.leftEL.x);
			addLine("      y = %.6f,", grip.leftEL.y);
			addLine("      z = %.6f", grip.leftEL.z);
			addLine("    },");
		}

		if (grip.hasRight)
		{
			addLine("    R = {");
			addLine("      x = %.6f,", grip.rightEL.x);
			addLine("      y = %.6f,", grip.rightEL.y);
			addLine("      z = %.6f", grip.rightEL.z);
			addLine("    },");
		}

		if (!grip.posOffset_FP.IsZero())
		{
			addLine("    posOffset_FP = {");
			addLine("      x = %.6f,", grip.posOffset_FP.x);
			addLine("      y = %.6f,", grip.posOffset_FP.y);
			addLine("      z = %.6f", grip.posOffset_FP.z);
			addLine("    },");
		}

		if (!grip.posOffset_TP.IsZero())
		{
			addLine("    posOffset_TP = {");
			addLine("      x = %.6f,", grip.posOffset_TP.x);
			addLine("      y = %.6f,", grip.posOffset_TP.y);
			addLine("      z = %.6f", grip.posOffset_TP.z);
			addLine("    },");
		}

		addLine("  },");
	}

	addLine("})");

	return script;
}
