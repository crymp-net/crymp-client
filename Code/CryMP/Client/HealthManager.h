#pragma once
#include <map>
#include <set>
#include <list>

struct IActor;

class CHealthManager {
	std::map<IActor*, int> m_lastHealth;
public:
	CHealthManager();
	~CHealthManager();
	bool IsActive();
	int GetHealth(IActor* pActor);
	void Reset(bool quit = true);
};