#pragma once

#include <array>
#include <variant>
#include <vector>
#include <string_view>

#include "CryCommon/Cry3DEngine/I3DEngine.h"
#include "CryCommon/Cry3DEngine/ISplines.h"

struct ITimer;

class TimeOfDay final : public ITimeOfDay
{
public:
	enum
	{
		HDR_DYNAMIC_POWER_FACTOR,
		SKY_BRIGHTENING,
		SSAO_AMOUNT_MULTIPLIER,
		SUN_COLOR,
		SUN_COLOR_MULTIPLIER,
		SUN_SPECULAR_MULTIPLIER,
		SKY_COLOR,
		SKY_COLOR_MULTIPLIER,
		FOG_COLOR,
		FOG_COLOR_MULTIPLIER,
		VOLUMETRIC_FOG_GLOBAL_DENSITY,
		VOLUMETRIC_FOG_ATMOSPHERE_HEIGHT,
		VOLUMETRIC_FOG_DENSITY_OFFSET,
		SKY_LIGHT_SUN_INTENSITY,
		SKY_LIGHT_SUN_INTENSITY_MULTIPLIER,
		SKY_LIGHT_MIE_SCATTERING,
		SKY_LIGHT_RAYLEIGH_SCATTERING,
		SKY_LIGHT_SUN_ANISOTROPY_FACTOR,
		SKY_LIGHT_WAVELENGTH_R,
		SKY_LIGHT_WAVELENGTH_G,
		SKY_LIGHT_WAVELENGTH_B,
		NIGHT_SKY_HORIZON_COLOR,
		NIGHT_SKY_HORIZON_COLOR_MULTIPLIER,
		NIGHT_SKY_ZENITH_COLOR,
		NIGHT_SKY_ZENITH_COLOR_MULTIPLIER,
		NIGHT_SKY_ZENITH_SHIFT,
		NIGHT_SKY_STAR_INTENSITY,
		NIGHT_SKY_MOON_COLOR,
		NIGHT_SKY_MOON_COLOR_MULTIPLIER,
		NIGHT_SKY_MOON_INNER_CORONA_COLOR,
		NIGHT_SKY_MOON_INNER_CORONA_COLOR_MULTIPLIER,
		NIGHT_SKY_MOON_INNER_CORONA_SCALE,
		NIGHT_SKY_MOON_OUTER_CORONA_COLOR,
		NIGHT_SKY_MOON_OUTER_CORONA_COLOR_MULTIPLIER,
		NIGHT_SKY_MOON_OUTER_CORONA_SCALE,
		CLOUD_SHADING_SUN_LIGHT_MULTIPLIER,
		CLOUD_SHADING_SKY_LIGHT_MULTIPLIER,
		SUN_SHAFTS_VISIBILITY,
		SUN_RAYS_VISIBILITY,
		SUN_RAYS_ATTENUATION,
		OCEAN_FOG_COLOR_MULTIPLIER,
		SKYBOX_MULTIPLIER,
		COLOR_SATURATION,
		COLOR_CONTRAST,
		COLOR_BRIGHTNESS,
		LEVELS_MIN_INPUT,
		LEVELS_GAMMA,
		LEVELS_MAX_INPUT,
		LEVELS_MIN_OUTPUT,
		LEVELS_MAX_OUTPUT,
		SELECTIVE_COLOR,
		SELECTIVE_COLOR_CYANS,
		SELECTIVE_COLOR_MAGENTAS,
		SELECTIVE_COLOR_YELLOWS,
		SELECTIVE_COLOR_BLACKS,
		FILTERS_GRAIN,
		FILTERS_SHARPENING,
		FILTERS_PHOTOFILTER_COLOR,
		FILTERS_PHOTOFILTER_DENSITY,
		EYEADAPTION_CLAMP,
		DOF_FOCUS_RANGE,
		DOF_BLUR_AMOUNT,

		VARIABLE_COUNT,
	};

private:
	struct FloatSpline final : public spline::CBaseSplineInterpolator<float, spline::CatmullRomSpline<float>>
	{
		int GetNumDimensions() override;
		ESplineType GetSplineType() override;

		void Interpolate(float time, ValueType& value) override;

		void SerializeSpline(XmlNodeRef& node, bool loading) override;
	};

	struct ColorSpline final : public spline::CBaseSplineInterpolator<Vec3, spline::CatmullRomSpline<Vec3>>
	{
		int GetNumDimensions() override;
		ESplineType GetSplineType() override;

		void Interpolate(float time, ValueType& value) override;

		void SerializeSpline(XmlNodeRef& node, bool loading) override;
	};

	struct Variable
	{
		std::string_view name;
		ITimeOfDay::EVariableType type = {};
		std::array<float, 3> value = {};
		std::variant<FloatSpline, ColorSpline> interpolator;
	};

	std::vector<Variable> m_vars;

	bool m_paused = false;
	bool m_editMode = false;

	float m_HDRMultiplier = 1;

	float m_currentTime = 0;
	float m_startTime = 0;
	float m_endTime = 0;
	float m_speed = 0;

	float* m_currentTimeCVarValue = nullptr;  // e_time_of_day
	float* m_speedCVarValue = nullptr;        // e_time_of_day_speed

	ITimer* m_timer = nullptr;
	ITimer** m_someOtherTimer = nullptr;

	XmlNodeRef m_defaultLevelSettings = nullptr;

public:
	explicit TimeOfDay(void* pCry3DEngine);
	~TimeOfDay();

	float GetHDRMultiplier() const { return m_HDRMultiplier; }

	////////////////////////////////////////////////////////////////////////////////
	// ITimeOfDay
	////////////////////////////////////////////////////////////////////////////////

	int GetVariableCount() override;
	bool GetVariableInfo(int index, SVariableInfo& info) override;
	void SetVariableValue(int index, float value[3]) override;

	void SetTime(float hour, bool forceUpdate = false) override;
	float GetTime() override;

	void Tick() override;

	void SetPaused(bool paused) override;

	void SetAdvancedInfo(const SAdvancedInfo& advancedInfo) override;
	void GetAdvancedInfo(SAdvancedInfo& advancedInfo) override;

	void Update(bool interpolate = true, bool forceUpdate = false) override;

	void BeginEditMode() override;
	void EndEditMode() override;

	void Serialize(XmlNodeRef& node, bool loading) override;
	void Serialize(TSerialize ser) override;

	void SetTimer(ITimer* timer) override;

	void NetSerialize(TSerialize ser, float lag, std::uint32_t flags) override;

	////////////////////////////////////////////////////////////////////////////////

	bool IsPaused() const 
	{ 
		return m_paused; 
	}
	void LoadCustomSettings(string xmlPath);
	void RestoreLevelDefaults();

private:
	void InitVariables();
	void InterpolateVariable(Variable& var, float time) const;
	void SerializeVariable(Variable& var, XmlNodeRef node) const;
	void DeserializeVariable(const XmlNodeRef& node);
	int FindVariableIndex(const std::string_view& name) const;

	Vec3 CalculateSunDirection(const Vec3& sunRotation) const;
};
