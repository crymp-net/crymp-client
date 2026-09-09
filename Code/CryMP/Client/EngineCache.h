#pragma once

#include <memory>
#include <vector>

struct ICharacterInstance;
struct ILevelInfo;
struct IMaterial;
struct IStatObj;

class EngineCache
{
	enum ECacheLevel
	{
		DISABLED,
		MINIMUM,
		RECOMMENDED,
		HIGH,
		LUDICROUS
	};

	template<class T>
	struct Releaser
	{
		void operator()(T* p) const { p->Release(); }
	};

	using SmartCharacterInstance = std::unique_ptr<ICharacterInstance, Releaser<ICharacterInstance>>;
	using SmartMaterial = std::unique_ptr<IMaterial, Releaser<IMaterial>>;
	using SmartStatObj = std::unique_ptr<IStatObj, Releaser<IStatObj>>;

	std::vector<SmartCharacterInstance> m_cachedCharacterInstances;
	std::vector<SmartMaterial> m_cachedMaterials;
	std::vector<SmartStatObj> m_cachedStatObjs;

	bool m_isCached = false;
	int cl_engineCacheLevel = 0;

	int ScanFolder(const char* folderName);
	bool Cache(const char* file);

public:
	EngineCache();
	~EngineCache();

	void OnLoadingStart(ILevelInfo* pLevel);
	void OnLoadingProgress(ILevelInfo* pLevel, int progressAmount);
	void OnDisconnect();
};
