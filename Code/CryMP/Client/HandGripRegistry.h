#pragma once

#include <map>
#include <string>
#include <string_view>

#include "CryCommon/CryMath/Cry_Math.h"

struct IEntity;

struct HandGripInfo
{
	bool hasLeft = false;
	bool hasRight = false;
	Vec3 leftEL = Vec3(ZERO);
	Vec3 rightEL = Vec3(ZERO);
	Vec3 posOffset_FP = Vec3(ZERO); // additional entity offset if specified
	Vec3 posOffset_TP = Vec3(ZERO); // additional entity offset if specified
};

class HandGripRegistry
{
	std::map<std::string, HandGripInfo, std::less<>> m_grips;

public:
	HandGripRegistry();
	~HandGripRegistry();

	void SetGripLeft(std::string_view key, const Vec3& leftEL);
	void SetGripRight(std::string_view key, const Vec3& rightEL);
	void SetGripHoldObjectOffset(std::string_view key, const Vec3& fpOffset, const Vec3& tpOffset);
	HandGripInfo* GetGripByKey(std::string_view key);
	HandGripInfo* GetGripByEntity(IEntity* e);
	bool RemoveGripByKey(std::string_view key);
	bool RemoveGripForEntity(IEntity* e);
	void ClearGripRegistry();

	std::string SerializeToLuaScript() const;
};
