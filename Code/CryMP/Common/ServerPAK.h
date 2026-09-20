#pragma once

#include <memory>
#include <string>
#include <vector>

struct ICharacterInstance;

class ServerPAK
{
	std::string m_path;

	template<class T>
	struct Releaser
	{
		void operator()(T* p) const { p->Release(); }
	};

	using SmartCharacterInstance = std::unique_ptr<ICharacterInstance, Releaser<ICharacterInstance>>;

	std::vector<SmartCharacterInstance> m_cgaCache;

public:
	ServerPAK();
	~ServerPAK();

	bool Load(const std::string& path);
	bool Unload();
	void OnFirstLoadingProgress();
	void OnDisconnect(int reason, const char* message);
	void ResetSubSystems();

private:
	void ReloadCgaCache();
};
