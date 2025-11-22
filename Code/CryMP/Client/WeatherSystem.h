#pragma once
#include <array>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include "CryCommon/CryMath/Cry_Math.h"

struct WeatherVariable {
	float value;
	float min;
	float max;
};

class CWeatherSystem {
	enum EWeatherVars {
		WEATHER_ENABLED,
		WEATHER_WIND,
		WEATHER_FX,
		WEATHER_LAYERS,
		WEATHER_VAR_COUNT
	};

	struct SEffectInfo {
		std::string effect;
		float scale = 1.0f;
		unsigned instances = 1;
		Vec3 dir = Vec3(0.0f, 0.0f, 1.0f);
		Vec3 pos = Vec3(512.0f, 512.0f, 64.0f);
	};

	bool                          m_enabled = false;
	string                        m_lastTodXmlPath;
	std::map<int, float[3]>       m_originalWeatherValues;
	std::optional<Vec3>           m_originalWind;
	std::map<int, IStatInstGroup> m_originalGroups;

	std::map<int, string>                                 m_activeValues;
	std::set<std::string>                                 m_activeEffects;
	std::optional<unsigned>                               m_activeMask;
	std::map<std::string, std::vector<IParticleEmitter*>> m_activeEmitters;

	float m_time;
	float m_lastUpdate;

	static bool tod_hooked;
	static constexpr int WEATHER_NAMESPACE = 2000;
	static constexpr int WEATHER_ENV_NAMESPACE = 2100;
	static constexpr int WEATHER_TOD_PATH_ID = 2300;

	static constexpr std::array<EERType, 1> static_entities = {
		eERType_Brush,
	};

public:
	CWeatherSystem();
	void Update(float frameTime);
	void Reset(bool deapply = true);

	bool IsFrozen() const;
	bool IsWet() const;
private:
	bool SetWeatherVariable(int variableId, float value, bool interpolate = true, bool update = false);
	bool SetWeatherVariable(int variableId, float x, float y, float z, bool update = false);
	std::optional<WeatherVariable> GetWeatherVariable(int variableId);
	std::optional<WeatherVariable> GetWeatherVariableRange(int variableId);
	bool RestoreWeatherVariable(int variableId, bool update = false);
	bool RestoreWeather(bool update = false);
	bool UpdateWeather();

	void SyncEffects(const char *effects);
	void SpawnEffect(const char *eff);
	void RemoveEffect(const char *eff);

	void SyncLayers(const char* layers);
	void ApplyLayer(unsigned layer);
	void RemoveLayer(unsigned layer);

	// Hook functions
	void TODUpdate(bool interpolate, bool force);

	// Utility functions
	std::string GetFrozenGrassName(std::string_view path);
	std::string GetNormalGrassName(std::string_view path);
	SEffectInfo GetEffectInfo(std::string_view effect);
};