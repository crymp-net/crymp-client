#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <numbers>

#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CrySystem/ITimer.h"
#include "CryCommon/CryNetwork/ISerialize.h"
#include "CryCommon/CryRenderer/IRenderer.h"
#include "CryCommon/CrySystem/IConsole.h"

#include "TimeOfDay.h"

int TimeOfDay::FloatSpline::GetNumDimensions()
{
	return 1;
}

ESplineType TimeOfDay::FloatSpline::GetSplineType()
{
	return ESPLINE_CATMULLROM;
}

void TimeOfDay::FloatSpline::Interpolate(float time, ValueType& value)
{
	value_type result;
	this->interpolate(time, result);
	this->ToValueType(result, value);
}

void TimeOfDay::FloatSpline::SerializeSpline(XmlNodeRef& node, bool loading)
{
	if (loading)
	{
		string keys = node->getAttr("Keys");

		this->resize(0);

		int currentPos = 0;
		string currentKey = keys.Tokenize(",", currentPos);
		while (!currentKey.empty())
		{
			float time = 0;
			float value = 0;
			std::sscanf(currentKey.c_str(), "%g:%g", &time, &value);

			ValueType tmp;
			tmp[0] = value;
			this->InsertKey(time, tmp);

			currentKey = keys.Tokenize(",", currentPos);
		}
	}
	else
	{
		string keys;
		string currentKey;
		for (int i = 0; i < this->num_keys(); i++)
		{
			currentKey.Format("%g:%g,", this->key(i).time, this->key(i).value);

			keys += currentKey;
		}

		node->setAttr("Keys", keys.c_str());
	}
}

int TimeOfDay::ColorSpline::GetNumDimensions()
{
	return 3;
}

ESplineType TimeOfDay::ColorSpline::GetSplineType()
{
	return ESPLINE_CATMULLROM;
}

void TimeOfDay::ColorSpline::Interpolate(float time, ValueType& value)
{
	value_type result;
	this->interpolate(time, result);
	this->ToValueType(result, value);
}

void TimeOfDay::ColorSpline::SerializeSpline(XmlNodeRef& node, bool loading)
{
	if (loading)
	{
		string keys = node->getAttr("Keys");

		this->resize(0);

		int currentPos = 0;
		string currentKey = keys.Tokenize(",", currentPos);
		while (!currentKey.empty())
		{
			float time = 0;
			float values[3] = {};
			std::sscanf(currentKey.c_str(), "%g:(%g:%g:%g),",
				&time,
				&values[0],
				&values[1],
				&values[2]
			);

			ValueType tmp;
			tmp[0] = values[0];
			tmp[1] = values[1];
			tmp[2] = values[2];
			this->InsertKey(time, tmp);

			currentKey = keys.Tokenize(",", currentPos);
		}
	}
	else
	{
		string keys;
		string currentKey;
		for (int i = 0; i < this->num_keys(); i++)
		{
			currentKey.Format("%g:(%g:%g:%g),",
				this->key(i).time,
				this->key(i).value.x,
				this->key(i).value.y,
				this->key(i).value.z
			);

			keys += currentKey;
		}

		node->setAttr("Keys", keys.c_str());
	}
}

TimeOfDay::TimeOfDay(void* pCry3DEngine)
{
	// offsets for the original Cry3DEngine DLL from Crysis build 6156
#ifdef BUILD_64BIT
	m_currentTimeCVarValue = *(static_cast<float**>(pCry3DEngine) + 0x4571C) + 0xED;
	m_speedCVarValue = *(static_cast<float**>(pCry3DEngine) + 0x4571C) + 0xEE;
	m_someOtherTimer = static_cast<ITimer**>(pCry3DEngine) + 0x44FAE;
#else
	m_currentTimeCVarValue = *(static_cast<float**>(pCry3DEngine) + 0x7126F) + 0xED;
	m_speedCVarValue = *(static_cast<float**>(pCry3DEngine) + 0x7126F) + 0xEE;
	m_someOtherTimer = static_cast<ITimer**>(pCry3DEngine) + 0x7040D;
#endif

	this->SetTimer(gEnv->pTimer);
	this->InitVariables();

	gEnv->pConsole->Register(
		"e_time_of_day_debug",
		&m_debug,
		0,
		VF_NOT_NET_SYNCED,
		"Enable TimeOfDay Debug mode. 0=Off, 1=Overview, 2=Atmosphere/Fog/Skylight, 3=Night Sky/Moon, 4=Color Grading/PostFX, 5=WeatherSystem Post Multipliers."
	);
}

TimeOfDay::~TimeOfDay()
{
}

int TimeOfDay::GetVariableCount()
{
	return static_cast<int>(m_vars.size());
}

bool TimeOfDay::GetVariableInfo(int index, SVariableInfo& info)
{
	if (index < 0 || index >= static_cast<int>(m_vars.size()))
	{
		return false;
	}

	Variable& var = m_vars[index];
	info.nParamId = index;
	info.name = var.name.data();
	info.type = var.type;
	info.fValue[0] = var.value[0];
	info.fValue[1] = var.value[1];
	info.fValue[2] = var.value[2];
	info.pInterpolator = std::visit([](auto& i) -> ISplineInterpolator* { return &i; }, var.interpolator);

	return true;
}

void TimeOfDay::SetVariableValue(int index, float value[3])
{
	if (index < 0 || index >= static_cast<int>(m_vars.size()))
	{
		return;
	}

	Variable& var = m_vars[index];
	var.value[0] = value[0];
	var.value[1] = value[1];
	var.value[2] = value[2];
}

void TimeOfDay::SetTime(float hour, bool forceUpdate)
{
	m_currentTime = hour;
	*m_currentTimeCVarValue = hour;

	this->Update(true, forceUpdate);
}

float TimeOfDay::GetTime()
{
	return m_currentTime;
}

void TimeOfDay::Tick()
{
	if (m_isTransitioning)
	{
		m_transitionTime += m_timer->GetFrameTime();

		if (m_transitionTime >= m_transitionDuration)
		{
			m_transitionTime = m_transitionDuration;
			m_isTransitioning = false;

			m_vars = m_transitionTargetVars;
			m_speed = m_transitionTargetSpeed;
			m_startTime = m_transitionTargetStartTime;
			m_endTime = m_transitionTargetEndTime;

			*m_speedCVarValue = m_speed;

			this->Update(true, true);
		}
	}

	if (m_paused || m_editMode || std::fabs(m_speed) <= 0.0001f)
	{
		if (m_isTransitioning)
		{
			this->Update(true, true);
		}
		return;
	}

	float newTime = m_currentTime + (m_speed * m_timer->GetFrameTime());

	if (m_startTime <= 0.05f && m_endTime >= 23.5f)
	{
		if (newTime > m_endTime)
		{
			newTime = m_startTime;
		}

		if (newTime < m_startTime)
		{
			newTime = m_endTime;
		}
	}
	else if (std::fabs(m_startTime - m_endTime) <= 0.05f)
	{
		if (newTime > 24.0f)
		{
			newTime -= 24.0f;
		}
		else if (newTime < 0.0f)
		{
			newTime += 24.0f;
		}
	}
	else
	{
		if (newTime > m_endTime)
		{
			newTime = m_endTime;
		}

		if (newTime < m_startTime)
		{
			newTime = m_startTime;
		}
	}

	this->SetTime(newTime);
}

void TimeOfDay::SetPaused(bool paused)
{
	m_paused = paused;
}

void TimeOfDay::SetAdvancedInfo(const SAdvancedInfo& advancedInfo)
{
	m_speed = advancedInfo.fAnimSpeed;
	*m_speedCVarValue = advancedInfo.fAnimSpeed;
	m_startTime = advancedInfo.fStartTime;
	m_endTime = advancedInfo.fEndTime;
}

void TimeOfDay::GetAdvancedInfo(SAdvancedInfo& advancedInfo)
{
	advancedInfo.fAnimSpeed = m_speed;
	advancedInfo.fStartTime = m_startTime;
	advancedInfo.fEndTime = m_endTime;
}

void TimeOfDay::EvaluateVariables(std::vector<Variable>& outVars, const std::vector<Variable>& sourceVars, float time) const
{
	outVars = sourceVars;

	for (Variable& var : outVars)
	{
		this->InterpolateVariable(var, time);
	}
}

void TimeOfDay::Update(bool interpolate, bool forceUpdate)
{
	if (!gEnv->bClient) //CryMP: For Dedicated server this can be skipped.
		return;

	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_3DENGINE);

	const std::vector<Variable>* pVars = &m_vars;
	std::vector<Variable> blendedVars;

	if (!m_isTransitioning)
	{
		if (interpolate)
		{
			const float time = m_currentTime / 24.0f;

			for (Variable& var : m_vars)
			{
				this->InterpolateVariable(var, time);
			}
		}
	}
	else
	{
		//CryMP: Only use temporary blended values while transitioning between presets.
		const float time = m_currentTime / 24.0f;

		std::vector<Variable> sourceVars = m_vars;
		std::vector<Variable> targetVars = m_transitionTargetVars;

		if (interpolate)
		{
			for (Variable& var : sourceVars)
			{
				this->InterpolateVariable(var, time);
			}

			for (Variable& var : targetVars)
			{
				this->InterpolateVariable(var, time);
			}
		}

		float blend = m_transitionTime / m_transitionDuration;
		blend = std::clamp(blend, 0.0f, 1.0f);
		blend = blend * blend * (3.0f - 2.0f * blend);

		blendedVars = sourceVars;

		for (size_t i = 0; i < blendedVars.size(); ++i)
		{
			blendedVars[i].value[0] = sourceVars[i].value[0] + (targetVars[i].value[0] - sourceVars[i].value[0]) * blend;
			blendedVars[i].value[1] = sourceVars[i].value[1] + (targetVars[i].value[1] - sourceVars[i].value[1]) * blend;
			blendedVars[i].value[2] = sourceVars[i].value[2] + (targetVars[i].value[2] - sourceVars[i].value[2]) * blend;
		}

		pVars = &blendedVars;
	}

	const std::vector<Variable>& vars = *pVars;

	I3DEngine* p3DEngine = gEnv->p3DEngine;

	if (gEnv->pRenderer->EF_Query(EFQ_HDRModeEnabled))
	{
		const float base = p3DEngine->GetHDRDynamicMultiplier();
		const float exponent = vars[HDR_DYNAMIC_POWER_FACTOR].value[0];

		m_HDRMultiplier = std::pow(base, exponent);

		m_HDRMultiplier = std::max(m_HDRMultiplier, 0.2f);
	}
	else
	{
		m_HDRMultiplier = 1;
	}

	const float dawnStart = p3DEngine->GetDawnStart();
	const float dawnEnd = p3DEngine->GetDawnEnd();
	const float duskStart = p3DEngine->GetDuskStart();
	const float duskEnd = p3DEngine->GetDuskEnd();

	Vec3 sunRotation(0, 0, 0);
	Vec3 sunDirection(0, 0, 0);
	float sunColorMultiplier = m_HDRMultiplier;
	float sunIntensityMultiplier = m_HDRMultiplier;
	float dayNightIndicator = 1;

	p3DEngine->GetGlobalParameter(E3DPARAM_SKY_SUNROTATION, sunRotation);

	if (m_currentTime < dawnStart || m_currentTime >= duskEnd)
	{
		dayNightIndicator = 0;
		sunIntensityMultiplier = 0;
		p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_DIRECTION, sunDirection);
	}
	else if (m_currentTime < dawnEnd)
	{
		const float dawnMiddle = 0.5f * (dawnStart + dawnEnd);

		if (m_currentTime < dawnMiddle)
		{
			sunColorMultiplier *= (dawnMiddle - m_currentTime) / (dawnMiddle - dawnStart);
			sunIntensityMultiplier = 0;
			p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_DIRECTION, sunDirection);
		}
		else
		{
			sunIntensityMultiplier = (m_currentTime - dawnMiddle) / (dawnEnd - dawnMiddle);
			sunColorMultiplier *= sunIntensityMultiplier;
			sunDirection = this->CalculateSunDirection(sunRotation);
		}

		dayNightIndicator = (m_currentTime - dawnStart) / (dawnEnd - dawnStart);
	}
	else if (m_currentTime < duskStart)
	{
		dayNightIndicator = 1;
		sunDirection = this->CalculateSunDirection(sunRotation);
	}
	else if (m_currentTime < duskEnd)
	{
		const float duskMiddle = 0.5f * (duskStart + duskEnd);

		if (m_currentTime < duskMiddle)
		{
			sunIntensityMultiplier = (duskMiddle - m_currentTime) / (duskMiddle - duskStart);
			sunColorMultiplier *= sunIntensityMultiplier;
			sunDirection = this->CalculateSunDirection(sunRotation);
		}
		else
		{
			sunColorMultiplier *= (m_currentTime - duskMiddle) / (duskEnd - duskMiddle);
			sunIntensityMultiplier = 0;
			p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_DIRECTION, sunDirection);
		}

		dayNightIndicator = (duskEnd - m_currentTime) / (duskEnd - duskStart);
	}

	p3DEngine->SetGlobalParameter(E3DPARAM_DAY_NIGHT_INDICATOR, dayNightIndicator);
	p3DEngine->SetSunDir(sunDirection);

	p3DEngine->SetSunColor(
		Vec3(
			vars[SUN_COLOR].value[0],
			vars[SUN_COLOR].value[1],
			vars[SUN_COLOR].value[2]
		) * (
			vars[SUN_COLOR_MULTIPLIER].value[0] * sunColorMultiplier
			)
	);

	p3DEngine->SetSunSpecMultiplier(vars[SUN_SPECULAR_MULTIPLIER].value[0]);
	p3DEngine->SetSkyBrightness(vars[SKY_BRIGHTENING].value[0]);
	p3DEngine->SetSSAOAmount(vars[SSAO_AMOUNT_MULTIPLIER].value[0]);

	p3DEngine->SetSkyColor(
		Vec3(
			vars[SKY_COLOR].value[0],
			vars[SKY_COLOR].value[1],
			vars[SKY_COLOR].value[2]
		) * (
			vars[SKY_COLOR_MULTIPLIER].value[0] * m_HDRMultiplier
			)
	);

	p3DEngine->SetFogColor(
		Vec3(
			vars[FOG_COLOR].value[0],
			vars[FOG_COLOR].value[1],
			vars[FOG_COLOR].value[2]
		) * (
			vars[FOG_COLOR_MULTIPLIER].value[0] * m_HDRMultiplier
			)
	);

	p3DEngine->SetVolumetricFogSettings(
		vars[VOLUMETRIC_FOG_GLOBAL_DENSITY].value[0],
		vars[VOLUMETRIC_FOG_ATMOSPHERE_HEIGHT].value[0],
		vars[VOLUMETRIC_FOG_DENSITY_OFFSET].value[0]
	);

	p3DEngine->SetSkyLightParameters(
		Vec3(
			vars[SKY_LIGHT_SUN_INTENSITY].value[0],
			vars[SKY_LIGHT_SUN_INTENSITY].value[1],
			vars[SKY_LIGHT_SUN_INTENSITY].value[2]
		) * (
			vars[SKY_LIGHT_SUN_INTENSITY_MULTIPLIER].value[0] * sunIntensityMultiplier
			),
		vars[SKY_LIGHT_MIE_SCATTERING].value[0],
		vars[SKY_LIGHT_RAYLEIGH_SCATTERING].value[0],
		vars[SKY_LIGHT_SUN_ANISOTROPY_FACTOR].value[0],
		Vec3(
			vars[SKY_LIGHT_WAVELENGTH_R].value[0],
			vars[SKY_LIGHT_WAVELENGTH_G].value[0],
			vars[SKY_LIGHT_WAVELENGTH_B].value[0]
		),
		forceUpdate
	);

	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_HORIZON_COLOR,
		Vec3(
			vars[NIGHT_SKY_HORIZON_COLOR].value[0],
			vars[NIGHT_SKY_HORIZON_COLOR].value[1],
			vars[NIGHT_SKY_HORIZON_COLOR].value[2]
		) * (
			vars[NIGHT_SKY_HORIZON_COLOR_MULTIPLIER].value[0] * m_HDRMultiplier
			)
	);

	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_ZENITH_COLOR,
		Vec3(
			vars[NIGHT_SKY_ZENITH_COLOR].value[0],
			vars[NIGHT_SKY_ZENITH_COLOR].value[1],
			vars[NIGHT_SKY_ZENITH_COLOR].value[2]
		) * (
			vars[NIGHT_SKY_ZENITH_COLOR_MULTIPLIER].value[0] * m_HDRMultiplier
			)
	);

	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_ZENITH_SHIFT, vars[NIGHT_SKY_ZENITH_SHIFT].value[0]);
	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_STAR_INTENSITY, vars[NIGHT_SKY_STAR_INTENSITY].value[0]);

	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_MOON_COLOR,
		Vec3(
			vars[NIGHT_SKY_MOON_COLOR].value[0],
			vars[NIGHT_SKY_MOON_COLOR].value[1],
			vars[NIGHT_SKY_MOON_COLOR].value[2]
		) * (
			vars[NIGHT_SKY_MOON_COLOR_MULTIPLIER].value[0] * m_HDRMultiplier
			)
	);

	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_MOON_INNERCORONA_COLOR,
		Vec3(
			vars[NIGHT_SKY_MOON_INNER_CORONA_COLOR].value[0],
			vars[NIGHT_SKY_MOON_INNER_CORONA_COLOR].value[1],
			vars[NIGHT_SKY_MOON_INNER_CORONA_COLOR].value[2]
		) * (
			vars[NIGHT_SKY_MOON_INNER_CORONA_COLOR_MULTIPLIER].value[0] * m_HDRMultiplier
			)
	);

	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_MOON_INNERCORONA_SCALE, vars[NIGHT_SKY_MOON_INNER_CORONA_SCALE].value[0]);

	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_MOON_OUTERCORONA_COLOR,
		Vec3(
			vars[NIGHT_SKY_MOON_OUTER_CORONA_COLOR].value[0],
			vars[NIGHT_SKY_MOON_OUTER_CORONA_COLOR].value[1],
			vars[NIGHT_SKY_MOON_OUTER_CORONA_COLOR].value[2]
		) * (
			vars[NIGHT_SKY_MOON_OUTER_CORONA_COLOR_MULTIPLIER].value[0] * m_HDRMultiplier
			)
	);

	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_MOON_OUTERCORONA_SCALE, vars[NIGHT_SKY_MOON_OUTER_CORONA_SCALE].value[0]);

	float sunShaftsVis = vars[SUN_SHAFTS_VISIBILITY].value[0];
	sunShaftsVis = std::clamp<float>(sunShaftsVis, 0.0f, 0.3f);
	const float sunRaysVis = vars[SUN_RAYS_VISIBILITY].value[0];

	p3DEngine->SetPostEffectParam("SunShafts_Active", (sunShaftsVis > 0.05f || sunRaysVis > 0.05f) ? 1 : 0);
	p3DEngine->SetPostEffectParam("SunShafts_Amount", sunShaftsVis);
	p3DEngine->SetPostEffectParam("SunShafts_RaysAmount", sunRaysVis);
	p3DEngine->SetPostEffectParam("SunShafts_RaysAttenuation", vars[SUN_RAYS_ATTENUATION].value[0]);

	p3DEngine->SetCloudShadingMultiplier(
		vars[CLOUD_SHADING_SUN_LIGHT_MULTIPLIER].value[0],
		vars[CLOUD_SHADING_SKY_LIGHT_MULTIPLIER].value[0]
	);

	p3DEngine->SetGlobalParameter(E3DPARAM_OCEANFOG_COLOR_MULTIPLIER, vars[OCEAN_FOG_COLOR_MULTIPLIER].value[0]);
	p3DEngine->SetGlobalParameter(E3DPARAM_SKYBOX_MULTIPLIER, vars[SKYBOX_MULTIPLIER].value[0] * m_HDRMultiplier);
	p3DEngine->SetGlobalParameter(E3DPARAM_EYEADAPTIONCLAMP, vars[EYEADAPTION_CLAMP].value[0]);

	p3DEngine->SetGlobalParameter(E3DPARAM_COLORGRADING_COLOR_SATURATION, vars[COLOR_SATURATION].value[0]);
	p3DEngine->SetPostEffectParam("ColorGrading_Contrast", vars[COLOR_CONTRAST].value[0]);
	p3DEngine->SetPostEffectParam("ColorGrading_Brightness", vars[COLOR_BRIGHTNESS].value[0]);
	p3DEngine->SetPostEffectParam("ColorGrading_minInput", vars[LEVELS_MIN_INPUT].value[0]);
	p3DEngine->SetPostEffectParam("ColorGrading_gammaInput", vars[LEVELS_GAMMA].value[0]);
	p3DEngine->SetPostEffectParam("ColorGrading_maxInput", vars[LEVELS_MAX_INPUT].value[0]);
	p3DEngine->SetPostEffectParam("ColorGrading_minOutput", vars[LEVELS_MIN_OUTPUT].value[0]);
	p3DEngine->SetPostEffectParam("ColorGrading_maxOutput", vars[LEVELS_MAX_OUTPUT].value[0]);

	p3DEngine->SetPostEffectParamVec4("clr_ColorGrading_SelectiveColor",
		Vec4(
			vars[SELECTIVE_COLOR].value[0],
			vars[SELECTIVE_COLOR].value[1],
			vars[SELECTIVE_COLOR].value[2],
			1
		)
	);

	p3DEngine->SetPostEffectParam("ColorGrading_SelectiveColorCyans", vars[SELECTIVE_COLOR_CYANS].value[0]);
	p3DEngine->SetPostEffectParam("ColorGrading_SelectiveColorMagentas", vars[SELECTIVE_COLOR_MAGENTAS].value[0]);
	p3DEngine->SetPostEffectParam("ColorGrading_SelectiveColorYellows", vars[SELECTIVE_COLOR_YELLOWS].value[0]);
	p3DEngine->SetPostEffectParam("ColorGrading_SelectiveColorBlacks", vars[SELECTIVE_COLOR_BLACKS].value[0]);
	p3DEngine->SetGlobalParameter(E3DPARAM_COLORGRADING_FILTERS_GRAIN, vars[FILTERS_GRAIN].value[0]);
	p3DEngine->SetPostEffectParam("ColorGrading_SharpenAmount", vars[FILTERS_SHARPENING].value[0]);

	p3DEngine->SetGlobalParameter(E3DPARAM_COLORGRADING_FILTERS_PHOTOFILTER_COLOR,
		Vec3(
			vars[FILTERS_PHOTOFILTER_COLOR].value[0],
			vars[FILTERS_PHOTOFILTER_COLOR].value[1],
			vars[FILTERS_PHOTOFILTER_COLOR].value[2]
		)
	);

	p3DEngine->SetGlobalParameter(E3DPARAM_COLORGRADING_FILTERS_PHOTOFILTER_DENSITY, vars[FILTERS_PHOTOFILTER_DENSITY].value[0]);

	p3DEngine->SetPostEffectParam("Dof_Tod_FocusRange", vars[DOF_FOCUS_RANGE].value[0]);
	p3DEngine->SetPostEffectParam("Dof_Tod_BlurAmount", vars[DOF_BLUR_AMOUNT].value[0]);
}

void TimeOfDay::BeginEditMode()
{
	m_editMode = true;
}

void TimeOfDay::EndEditMode()
{
	m_editMode = false;
}

void TimeOfDay::Serialize(XmlNodeRef& node, bool loading)
{
	if (loading)
	{
		m_defaultLevelSettings = node;

		node->getAttr("Time", m_currentTime);
		node->getAttr("TimeStart", m_startTime);
		node->getAttr("TimeEnd", m_endTime);
		node->getAttr("TimeAnimSpeed", m_speed);

		if (m_editMode)
		{
			m_currentTime = m_startTime;
		}

		*m_speedCVarValue = m_speed;

		for (int i = 0; i < node->getChildCount(); i++)
		{
			this->DeserializeVariable(node->getChild(i));
		}

		this->SetTime(m_currentTime);
	}
	else
	{
		node->setAttr("Time", m_currentTime);
		node->setAttr("TimeStart", m_startTime);
		node->setAttr("TimeEnd", m_endTime);
		node->setAttr("TimeAnimSpeed", m_speed);

		for (Variable& var : m_vars)
		{
			this->SerializeVariable(var, node->newChild("Variable"));
		}
	}
}

void TimeOfDay::Serialize(TSerialize ser)
{
	ser.Value("time", m_currentTime);
	ser.Value("mode", m_editMode);

	ser.BeginGroup("VariableValues");

	std::string varName;
	varName.reserve(64);

	for (Variable& var : m_vars)
	{
		varName = var.name;

		std::ranges::replace_if(varName,
			[](char ch) { return ch == ' ' || ch == '(' || ch == ')' || ch == ':'; },
			'_'
		);

		ser.BeginGroup(varName.c_str());
		ser.Value("Val0", var.value[0]);
		ser.Value("Val1", var.value[1]);
		ser.Value("Val2", var.value[2]);
		ser.EndGroup();
	}

	ser.EndGroup();

	ser.Value("AdvInfoSpeed", m_speed);
	ser.Value("AdvInfoStart", m_startTime);
	ser.Value("AdvInfoEnd", m_endTime);

	if (ser.IsReading())
	{
		this->SetTime(m_currentTime, true);
	}
}

void TimeOfDay::SetTimer(ITimer* timer)
{
	m_timer = timer;
	*m_someOtherTimer = timer;
}

void TimeOfDay::NetSerialize(TSerialize ser, float lag, std::uint32_t flags)
{
	if (flags & ITimeOfDay::NETSER_STATICPROPS)
	{
		return;
	}

	constexpr int POLICY = 0x746F64;

	if (ser.IsWriting())
	{
		ser.Value("time", m_currentTime, POLICY);
	}
	else
	{
		const bool isForceSet = (flags & NETSER_FORCESET);
		const bool isCompensateLag = (flags & NETSER_COMPENSATELAG);

		float newTime = 0;
		ser.Value("time", newTime, POLICY);

		if (isCompensateLag)
		{
			newTime += m_speed * lag;
		}

		if (!isForceSet)
		{
			float localTime = m_currentTime;
			float remoteTime = newTime;

			if (localTime < 2 && remoteTime > 22)
			{
				localTime += 24;
			}
			else if (remoteTime < 2 && localTime > 22)
			{
				remoteTime += 24;
			}

			if (std::fabs(remoteTime - localTime) < 1)
			{
				newTime = (m_currentTime * 0.95f) + (remoteTime * 0.05f);

				if (newTime > 24)
				{
					newTime -= 24;
				}
			}
		}

		this->SetTime(newTime, isForceSet);
	}
}

void TimeOfDay::InitVariables()
{
	m_vars.resize(VARIABLE_COUNT);

	const auto init = [this](int id, std::string_view name, EVariableType type, float val1, float val2, float val3)
		{
			Variable& var = m_vars[id];
			var.name = name;
			var.type = type;
			var.value[0] = val1;
			var.value[1] = val2;
			var.value[2] = val3;

			switch (type)
			{
			case TYPE_FLOAT:
			{
				var.interpolator.emplace<FloatSpline>();
				std::visit([&](auto& i) { return i.InsertKeyFloat(0, val1); }, var.interpolator);
				std::visit([&](auto& i) { return i.InsertKeyFloat(1, val1); }, var.interpolator);
				break;
			}
			case TYPE_COLOR:
			{
				var.interpolator.emplace<ColorSpline>();
				ColorSpline::ValueType values{ val1, val2, val3, 0 };
				std::visit([&](auto& i) { return i.InsertKey(0, values); }, var.interpolator);
				std::visit([&](auto& i) { return i.InsertKey(1, values); }, var.interpolator);
				break;
			}
			}
		};

	init(HDR_DYNAMIC_POWER_FACTOR, "HDR dynamic power factor", TYPE_FLOAT, 0, -4, 4);
	init(SKY_BRIGHTENING, "Sky brightening (terrain occlusion)", TYPE_FLOAT, 0.3f, 0, 1);
	init(SSAO_AMOUNT_MULTIPLIER, "SSAO amount multiplier", TYPE_FLOAT, 1, 0, 2.5f);
	init(SUN_COLOR, "Sun color", TYPE_COLOR, 215.0f / 255.0f, 200.0f / 255.0f, 170.0f / 255.0f);
	init(SUN_COLOR_MULTIPLIER, "Sun color multiplier", TYPE_FLOAT, 2.4f, 0, 16);
	init(SUN_SPECULAR_MULTIPLIER, "Sun specular multiplier", TYPE_FLOAT, 1, 0, 4);
	init(SKY_COLOR, "Sky color", TYPE_COLOR, 160.0f / 255.0f, 200.0f / 255.0f, 240.0f / 255.0f);
	init(SKY_COLOR_MULTIPLIER, "Sky color multiplier", TYPE_FLOAT, 1.1f, 0, 16);
	init(FOG_COLOR, "Fog color", TYPE_COLOR, 0, 0, 0);
	init(FOG_COLOR_MULTIPLIER, "Fog color multiplier", TYPE_FLOAT, 0, 0, 16);
	init(VOLUMETRIC_FOG_GLOBAL_DENSITY, "Volumetric fog: Global density", TYPE_FLOAT, 0.02f, 0, 100);
	init(VOLUMETRIC_FOG_ATMOSPHERE_HEIGHT, "Volumetric fog: Atmosphere height", TYPE_FLOAT, 4000, 100, 30000);
	init(VOLUMETRIC_FOG_DENSITY_OFFSET, "Volumetric fog: Density offset (in reality 0)", TYPE_FLOAT, 0, 0, 100);
	init(SKY_LIGHT_SUN_INTENSITY, "Sky light: Sun intensity", TYPE_COLOR, 5.0f / 6.0f, 5.0f / 6.0f, 1);
	init(SKY_LIGHT_SUN_INTENSITY_MULTIPLIER, "Sky light: Sun intensity multiplier", TYPE_FLOAT, 30, 0, 1000);
	init(SKY_LIGHT_MIE_SCATTERING, "Sky light: Mie scattering", TYPE_FLOAT, 4.8f, 0, 1000000);
	init(SKY_LIGHT_RAYLEIGH_SCATTERING, "Sky light: Rayleigh scattering", TYPE_FLOAT, 2, 0, 1000000);
	init(SKY_LIGHT_SUN_ANISOTROPY_FACTOR, "Sky light: Sun anisotropy factor", TYPE_FLOAT, -0.995f, -0.9999f, 0.9999f);
	init(SKY_LIGHT_WAVELENGTH_R, "Sky light: Wavelength (R)", TYPE_FLOAT, 750, 380, 780);
	init(SKY_LIGHT_WAVELENGTH_G, "Sky light: Wavelength (G)", TYPE_FLOAT, 601, 380, 780);
	init(SKY_LIGHT_WAVELENGTH_B, "Sky light: Wavelength (B)", TYPE_FLOAT, 555, 380, 780);
	init(NIGHT_SKY_HORIZON_COLOR, "Night sky: Horizon color", TYPE_COLOR, 222.0f / 255.0f, 148.0f / 255.0f, 47.0f / 255.0f);
	init(NIGHT_SKY_HORIZON_COLOR_MULTIPLIER, "Night sky: Horizon color multiplier", TYPE_FLOAT, 1, 0, 16);
	init(NIGHT_SKY_ZENITH_COLOR, "Night sky: Zenith color", TYPE_COLOR, 17.0f / 255.0f, 38.0f / 255.0f, 78.0f / 255.0f);
	init(NIGHT_SKY_ZENITH_COLOR_MULTIPLIER, "Night sky: Zenith color multiplier", TYPE_FLOAT, 0.25f, 0, 16);
	init(NIGHT_SKY_ZENITH_SHIFT, "Night sky: Zenith shift", TYPE_FLOAT, 0.25f, 0, 1);
	init(NIGHT_SKY_STAR_INTENSITY, "Night sky: Star intensity", TYPE_FLOAT, 0, 0, 3);
	init(NIGHT_SKY_MOON_COLOR, "Night sky: Moon color", TYPE_COLOR, 1, 1, 1);
	init(NIGHT_SKY_MOON_COLOR_MULTIPLIER, "Night sky: Moon color multiplier", TYPE_FLOAT, 0, 0, 16);
	init(NIGHT_SKY_MOON_INNER_CORONA_COLOR, "Night sky: Moon inner corona color", TYPE_COLOR, 230.0f / 255.0f, 1, 1);
	init(NIGHT_SKY_MOON_INNER_CORONA_COLOR_MULTIPLIER, "Night sky: Moon inner corona color multiplier", TYPE_FLOAT, 0, 0, 16);
	init(NIGHT_SKY_MOON_INNER_CORONA_SCALE, "Night sky: Moon inner corona scale", TYPE_FLOAT, 0.499f, 0, 2);
	init(NIGHT_SKY_MOON_OUTER_CORONA_COLOR, "Night sky: Moon outer corona color", TYPE_COLOR, 128.0f / 255.0f, 200.0f / 255.0f, 1);
	init(NIGHT_SKY_MOON_OUTER_CORONA_COLOR_MULTIPLIER, "Night sky: Moon outer corona color multiplier", TYPE_FLOAT, 0, 0, 16);
	init(NIGHT_SKY_MOON_OUTER_CORONA_SCALE, "Night sky: Moon outer corona scale", TYPE_FLOAT, 0.006f, 0, 2);
	init(CLOUD_SHADING_SUN_LIGHT_MULTIPLIER, "Cloud shading: Sun light multiplier", TYPE_FLOAT, 1.96f, 0, 16);
	init(CLOUD_SHADING_SKY_LIGHT_MULTIPLIER, "Cloud shading: Sky light multiplier", TYPE_FLOAT, 0.8f, 0, 16);
	init(SUN_SHAFTS_VISIBILITY, "Sun shafts visibility", TYPE_FLOAT, 0.25f, 0, 1);
	init(SUN_RAYS_VISIBILITY, "Sun rays visibility", TYPE_FLOAT, 2.5f, 0, 10);
	init(SUN_RAYS_ATTENUATION, "Sun rays attenuation", TYPE_FLOAT, 5, 0, 10);
	init(OCEAN_FOG_COLOR_MULTIPLIER, "Ocean fog color multiplier", TYPE_FLOAT, 1, 0, 1);
	init(SKYBOX_MULTIPLIER, "Skybox multiplier", TYPE_FLOAT, 1, 0, 1);
	init(COLOR_SATURATION, "Color: saturation", TYPE_FLOAT, 1, 0, 10);
	init(COLOR_CONTRAST, "Color: contrast", TYPE_FLOAT, 1, 0, 10);
	init(COLOR_BRIGHTNESS, "Color: brightness", TYPE_FLOAT, 1, 0, 10);
	init(LEVELS_MIN_INPUT, "Levels: min input", TYPE_FLOAT, 0, 0, 255);
	init(LEVELS_GAMMA, "Levels: gamma", TYPE_FLOAT, 1, 0, 10);
	init(LEVELS_MAX_INPUT, "Levels: max input", TYPE_FLOAT, 255, 0, 255);
	init(LEVELS_MIN_OUTPUT, "Levels: min output", TYPE_FLOAT, 0, 0, 0);
	init(LEVELS_MAX_OUTPUT, "Levels: max output", TYPE_FLOAT, 255, 0, 255);
	init(SELECTIVE_COLOR, "Selective Color: color", TYPE_COLOR, 0, 1, 1);
	init(SELECTIVE_COLOR_CYANS, "Selective Color: cyans", TYPE_FLOAT, 0, -100, 100);
	init(SELECTIVE_COLOR_MAGENTAS, "Selective Color: magentas", TYPE_FLOAT, 0, -100, 100);
	init(SELECTIVE_COLOR_YELLOWS, "Selective Color: yellows", TYPE_FLOAT, 0, -100, 100);
	init(SELECTIVE_COLOR_BLACKS, "Selective Color: blacks", TYPE_FLOAT, 0, -100, 100);
	init(FILTERS_GRAIN, "Filters: grain", TYPE_FLOAT, 0, 0, 1);
	init(FILTERS_SHARPENING, "Filters: sharpening", TYPE_FLOAT, 0, 0, 1);
	init(FILTERS_PHOTOFILTER_COLOR, "Filters: photofilter color", TYPE_COLOR, 0.952f, 0.517f, 0.09f);
	init(FILTERS_PHOTOFILTER_DENSITY, "Filters: photofilter density", TYPE_FLOAT, 0, 0, 1);
	init(EYEADAPTION_CLAMP, "EyeAdaption: Clamp", TYPE_FLOAT, 4, 0, 10);
	init(DOF_FOCUS_RANGE, "Dof: focus range", TYPE_FLOAT, 1000, 0, 10000);
	init(DOF_BLUR_AMOUNT, "Dof: blur amount", TYPE_FLOAT, 0, 0, 1);

	m_transitionTargetVars = m_vars;
}

void TimeOfDay::InterpolateVariable(Variable& var, float time) const
{
	struct InterpolateFunctor
	{
		Variable& var;
		float time;

		void operator()(FloatSpline& interpolator)
		{
			interpolator.InterpolateFloat(time, var.value[0]);
			var.value[0] = std::clamp<float>(var.value[0], var.value[1], var.value[2]);
		}

		void operator()(ColorSpline& interpolator)
		{
			interpolator.InterpolateFloat3(time, var.value.data());
			var.value[0] = std::clamp<float>(var.value[0], 0, 1);
			var.value[1] = std::clamp<float>(var.value[1], 0, 1);
			var.value[2] = std::clamp<float>(var.value[2], 0, 1);
		}
	};

	std::visit(InterpolateFunctor{ var, time }, var.interpolator);
}

void TimeOfDay::SerializeVariable(Variable& var, XmlNodeRef node) const
{
	node->setAttr("Name", var.name.data());

	struct SerializeFunctor
	{
		Variable& var;
		XmlNodeRef& node;

		void operator()(FloatSpline& interpolator)
		{
			node->setAttr("Value", var.value[0]);

			XmlNodeRef splineNode = node->newChild("Spline");
			interpolator.SerializeSpline(splineNode, false);
		}

		void operator()(ColorSpline& interpolator)
		{
			node->setAttr("Color", Vec3(var.value[0], var.value[1], var.value[2]));

			XmlNodeRef splineNode = node->newChild("Spline");
			interpolator.SerializeSpline(splineNode, false);
		}
	};

	std::visit(SerializeFunctor{ var, node }, var.interpolator);
}

void TimeOfDay::DeserializeVariable(const XmlNodeRef& node, std::vector<Variable>& vars)
{
	const int index = this->FindVariableIndex(node->getAttr("Name"), vars);
	if (index < 0)
	{
		return;
	}

	Variable& var = vars[index];

	struct DeserializeFunctor
	{
		Variable& var;
		const XmlNodeRef& node;

		void operator()(FloatSpline& interpolator)
		{
			node->getAttr("Value", var.value[0]);

			XmlNodeRef splineNode = node->findChild("Spline");
			if (splineNode)
			{
				interpolator.SerializeSpline(splineNode, true);
			}
		}

		void operator()(ColorSpline& interpolator)
		{
			Vec3 values(var.value[0], var.value[1], var.value[2]);
			node->getAttr("Color", values);
			var.value[0] = values[0];
			var.value[1] = values[1];
			var.value[2] = values[2];

			XmlNodeRef splineNode = node->findChild("Spline");
			if (splineNode)
			{
				interpolator.SerializeSpline(splineNode, true);

				for (int i = 0; i < interpolator.num_keys(); i++)
				{
					ColorSpline::ValueType value;
					if (interpolator.GetKeyValue(i, value))
					{
						value[0] = std::clamp<float>(value[0], -100, 100);
						value[1] = std::clamp<float>(value[1], -100, 100);
						value[2] = std::clamp<float>(value[2], -100, 100);
						interpolator.SetKeyValue(i, value);
					}
				}
			}
		}
	};

	std::visit(DeserializeFunctor{ var, node }, var.interpolator);
}

void TimeOfDay::DeserializeVariable(const XmlNodeRef& node)
{
	this->DeserializeVariable(node, m_vars);
}

int TimeOfDay::FindVariableIndex(const std::string_view& name, const std::vector<Variable>& vars) const
{
	const auto lower = [](unsigned char ch) { return std::tolower(ch); };

	const auto it = std::ranges::find_if(vars,
		[&](const Variable& var)
		{
			return std::ranges::equal(name, var.name, {}, lower, lower);
		}
	);

	return it == vars.end() ? -1 : static_cast<int>(it - vars.begin());
}

int TimeOfDay::FindVariableIndex(const std::string_view& name) const
{
	return this->FindVariableIndex(name, m_vars);
}

Vec3 TimeOfDay::CalculateSunDirection(const Vec3& sunRotation) const
{
	using std::numbers::pi;

	const float rotationZ = ((m_currentTime + 12.0f) / 24.0f) * pi * 2.0f;
	const float rotationX = (0.5f * pi) - ((pi * sunRotation.y) / 180.0f);
	const float rotationY = pi * ((-sunRotation.x) / 180.0f);

	Vec3 sunDirection = Vec3(0, 1, 0) * (
		Matrix33::CreateRotationZ(rotationZ) *
		Matrix33::CreateRotationX(rotationX) *
		Matrix33::CreateRotationY(rotationY)
		);

	const float oldZ = sunDirection.z;
	sunDirection.z = sunDirection.y;
	sunDirection.y = -oldZ;

	return sunDirection;
}

void TimeOfDay::LoadCustomSettings(string xmlPath, float blendDuration)
{
	if (xmlPath.empty())
	{
		this->RestoreLevelDefaults(blendDuration);
		return;
	}

	XmlNodeRef node = gEnv->pSystem->LoadXmlFile(xmlPath.c_str());
	if (!node)
	{
		CryLogWarningAlways("Failed to load custom ToD settings from '%s'", xmlPath.c_str());
		return;
	}

	if (blendDuration <= 0.0f)
	{
		for (int i = 0; i < node->getChildCount(); i++)
		{
			this->DeserializeVariable(node->getChild(i));
		}

		//CryMP: Keep current active clock settings when loading a custom visual ToD preset.
		this->Update(true, true);

		CryLog("$3[CryMP] Loaded custom ToD settings from '%s'", xmlPath.c_str());
		return;
	}

	m_activeCustomTodFile = xmlPath;

	m_transitionTargetVars = m_vars;

	//CryMP: Custom ToD weather presets should only affect visual variables, not the active world clock settings.
	m_transitionTargetSpeed = m_speed;
	m_transitionTargetStartTime = m_startTime;
	m_transitionTargetEndTime = m_endTime;

	for (int i = 0; i < node->getChildCount(); i++)
	{
		this->DeserializeVariable(node->getChild(i), m_transitionTargetVars);
	}

	m_isTransitioning = true;
	m_transitionTime = 0.0f;
	m_transitionDuration = std::max(blendDuration, 0.001f);

	CryLog("$3[CryMP] Blending custom ToD settings from '%s' over %.2f seconds", xmlPath.c_str(), blendDuration);
}

void TimeOfDay::RestoreLevelDefaults()
{
	this->RestoreLevelDefaults(0.0f);
}

void TimeOfDay::RestoreLevelDefaults(float blendDuration)
{
	if (!m_defaultLevelSettings)
	{
		return;
	}

	if (blendDuration <= 0.0f)
	{
		for (int i = 0; i < m_defaultLevelSettings->getChildCount(); i++)
		{
			this->DeserializeVariable(m_defaultLevelSettings->getChild(i));
		}

		m_defaultLevelSettings->getAttr("TimeAnimSpeed", m_speed);
		m_defaultLevelSettings->getAttr("TimeStart", m_startTime);
		m_defaultLevelSettings->getAttr("TimeEnd", m_endTime);

		*m_speedCVarValue = m_speed;
		this->Update(true, true);

		CryLog("$3[CryMP] Restored default ToD xml settings");
		return;
	}

	m_activeCustomTodFile.clear();

	m_transitionTargetVars = m_vars;

	m_defaultLevelSettings->getAttr("TimeAnimSpeed", m_transitionTargetSpeed);
	m_defaultLevelSettings->getAttr("TimeStart", m_transitionTargetStartTime);
	m_defaultLevelSettings->getAttr("TimeEnd", m_transitionTargetEndTime);

	for (int i = 0; i < m_defaultLevelSettings->getChildCount(); i++)
	{
		this->DeserializeVariable(m_defaultLevelSettings->getChild(i), m_transitionTargetVars);
	}

	m_isTransitioning = true;
	m_transitionTime = 0.0f;
	m_transitionDuration = std::max(blendDuration, 0.001f);

	CryLog("$3[CryMP] Restored default ToD xml settings over %.2f seconds", blendDuration);
}

void TimeOfDay::DebugDraw()
{
	if (m_debug <= 0)
	{
		return;
	}

	constexpr float UI_SCALE = 0.9f;
	const float screenH = (float)gEnv->pRenderer->GetHeight();
	const float FONT_SCALE = std::clamp(screenH / 1200.0f, 1.0f, 1.2f);

	constexpr float X_OFFSET = 550.0f;
	constexpr float BAR_X = 16.0f;
	constexpr float BAR_W = 180.0f;
	constexpr float VALUE_BOX_Y_OFFSET = 0.0f;
	constexpr float VALUE_BOX_GAP = 6.0f;
	constexpr float VALUE_BOX_PAD_X = 4.0f;
	constexpr float VALUE_BOX_H = 14.0f;
	constexpr float VALUE_BOX_WIDTH = 32.0f;
	constexpr float VALUE_BOX_WIDTH_TARGET = 32.0f;
	constexpr float VALUE_EPSILON = 0.001f;

	auto S = [&](float v) { return v * UI_SCALE; };

	auto DrawAlignedLabel = [=](float x, float y, float size, const ColorF& color, bool centered, const char* fmt, ...)
		{
			char buffer[256];

			va_list args;
			va_start(args, fmt);
			std::vsnprintf(buffer, sizeof(buffer), fmt, args);
			va_end(args);

			const float sx = gEnv->pRenderer->ScaleCoordX(x + X_OFFSET);
			const float sy = gEnv->pRenderer->ScaleCoordY(y);

			gEnv->pRenderer->Draw2dLabel(sx, sy, size * UI_SCALE * FONT_SCALE, (float*)&color, centered, "%s", buffer);
		};

	auto DrawValueBox = [=](float x, float y, float width, const char* text, const ColorF& textColor)
		{
			if (!text || !text[0])
			{
				return;
			}

			const float paddingY = S(1.0f);
			const float fontSize = 1.2f;

			gEnv->pRenderer->Draw2dImage(
				x + X_OFFSET,
				y,
				width,
				S(VALUE_BOX_H),
				0, 0, 0, 0, 0, 0,
				0.0f, 0.0f, 0.0f, 0.60f
			);

			const float sx = gEnv->pRenderer->ScaleCoordX(x + X_OFFSET + S(VALUE_BOX_PAD_X));
			const float sy = gEnv->pRenderer->ScaleCoordY(y + paddingY);

			gEnv->pRenderer->Draw2dLabel(sx, sy, fontSize * UI_SCALE * FONT_SCALE, (float*)&textColor, false, "%s", text);
		};

	auto DrawBar = [&](float x, float y, float width, float height, float border, float progress, const ColorF& color0, const ColorF& color1, const char* desc, const char* currText, const char* targetText, const ColorF& textColor, const ColorF& targetColor, float bgalpha, bool showTarget)
		{
			progress = std::clamp(progress, 0.0f, 1.0f);

			ColorF interp;
			interp.lerpFloat(color0, color1, progress);

			const float currw = width * progress;
			const float barX = x + X_OFFSET;
			const float sy = gEnv->pRenderer->ScaleCoordY(y);

			gEnv->pRenderer->Draw2dImage(barX - border, y - border, width + border + border, height + border + border, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, bgalpha);
			gEnv->pRenderer->Draw2dImage(barX, y, currw, height, 0, 0, 0, 0, 0, 0, interp.r, interp.g, interp.b, 0.75f);
			gEnv->pRenderer->Draw2dImage(barX + currw, y, width - currw, height, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.35f);

			if (desc && desc[0])
			{
				const float descX = gEnv->pRenderer->ScaleCoordX(barX);
				gEnv->pRenderer->Draw2dLabel(descX, sy, 1.2f * UI_SCALE * FONT_SCALE, (float*)&textColor, false, "%s", desc);
			}

			const float valueY = y - S(VALUE_BOX_Y_OFFSET);

			const float currBoxW = S(VALUE_BOX_WIDTH);
			const float targetBoxW = S(VALUE_BOX_WIDTH_TARGET);

			const float currBoxX = x + width + S(VALUE_BOX_GAP);
			const float targetBoxX = currBoxX + currBoxW + S(VALUE_BOX_GAP);

			if (currText && currText[0])
			{
				DrawValueBox(currBoxX, valueY, currBoxW, currText, Col_White);
			}

			if (showTarget && targetText && targetText[0])
			{
				DrawValueBox(targetBoxX, valueY, targetBoxW, targetText, targetColor);
			}
		};

	auto DrawCenteredMultiplierBar = [&](float x, float y, float width, float height, float border, float value, float neutralValue, float maxValue, const ColorF& leftColor, const ColorF& rightColor, const char* desc, const char* valueText, const ColorF& textColor, float bgalpha)
		{
			const float clampedValue = std::clamp(value, 0.0f, maxValue);
			const float barX = x + X_OFFSET;
			const float sy = gEnv->pRenderer->ScaleCoordY(y);

			const float neutralT = std::clamp(neutralValue / maxValue, 0.0f, 1.0f);
			const float valueT = std::clamp(clampedValue / maxValue, 0.0f, 1.0f);

			const float neutralX = barX + width * neutralT;
			const float valueX = barX + width * valueT;

			gEnv->pRenderer->Draw2dImage(barX - border, y - border, width + border + border, height + border + border, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, bgalpha);
			gEnv->pRenderer->Draw2dImage(barX, y, width, height, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.35f);

			if (valueX >= neutralX)
			{
				const float fillW = valueX - neutralX;
				if (fillW > 0.0f)
				{
					gEnv->pRenderer->Draw2dImage(neutralX, y, fillW, height, 0, 0, 0, 0, 0, 0, rightColor.r, rightColor.g, rightColor.b, 0.80f);
				}
			}
			else
			{
				const float fillW = neutralX - valueX;
				if (fillW > 0.0f)
				{
					gEnv->pRenderer->Draw2dImage(valueX, y, fillW, height, 0, 0, 0, 0, 0, 0, leftColor.r, leftColor.g, leftColor.b, 0.80f);
				}
			}

			gEnv->pRenderer->Draw2dImage(neutralX - S(1.0f), y - S(1.0f), S(2.0f), height + S(2.0f), 0, 0, 0, 0, 0, 0, 1.0f, 1.0f, 1.0f, 0.65f);

			if (desc && desc[0])
			{
				const float descX = gEnv->pRenderer->ScaleCoordX(barX);
				gEnv->pRenderer->Draw2dLabel(descX, sy, 1.2f * UI_SCALE * FONT_SCALE, (float*)&textColor, false, "%s", desc);
			}

			const float valueY = y - S(VALUE_BOX_Y_OFFSET);
			const float currBoxW = S(VALUE_BOX_WIDTH);
			const float currBoxX = x + width + S(VALUE_BOX_GAP);

			if (valueText && valueText[0])
			{
				DrawValueBox(currBoxX, valueY, currBoxW, valueText, Col_White);
			}
		};

	auto SafeComponentMul = [&](float finalValue, float baseValue)
		{
			constexpr float EPS = 0.0001f;
			if (std::fabs(baseValue) < EPS)
			{
				return 1.0f;
			}
			return finalValue / baseValue;
		};

	auto ComputePostMultiplier = [&](const Vec3& baseColor, const Vec3& finalColor)
		{
			return Vec3(
				SafeComponentMul(finalColor.x, baseColor.x),
				SafeComponentMul(finalColor.y, baseColor.y),
				SafeComponentMul(finalColor.z, baseColor.z)
			);
		};

	auto DrawPostMulBar = [&](float& yPos, const char* desc, float value, const ColorF& leftColor, const ColorF& rightColor)
		{
			char currLabel[64];
			std::snprintf(currLabel, sizeof(currLabel), "%.2f", value);

			DrawCenteredMultiplierBar(
				S(BAR_X), yPos,
				S(BAR_W), S(12.0f), S(2.0f),
				value, 1.0f, 2.0f,
				leftColor, rightColor,
				desc,
				currLabel,
				Col_White,
				0.40f
			);

			yPos += S(16.0f);
		};

	auto DrawModeHeader = [&](float& yPos, const char* title, const ColorF& modeColor)
		{
			DrawAlignedLabel(S(BAR_X), yPos, 1.4f, Col_Yellow, false, "TimeOfDay");
			DrawAlignedLabel(S(BAR_X) + S(110.0f), yPos, 1.25f, modeColor, false, "[Mode %d] %s", m_debug, title);

			if (!m_activeCustomTodFile.empty())
			{
				yPos += S(15.0f);
				DrawAlignedLabel(S(BAR_X), yPos, 1.15f, Col_SpringGreen, false, "XML: %s", m_activeCustomTodFile.c_str());
			}

			yPos += S(26.0f);
		};

	const float time = m_currentTime / 24.0f;

	std::vector<Variable> sourceVars;
	std::vector<Variable> targetVars;
	std::vector<Variable> blendedVars;

	this->EvaluateVariables(sourceVars, m_vars, time);
	this->EvaluateVariables(targetVars, m_transitionTargetVars, time);

	const bool hasTransition = (m_transitionDuration > 0.0f && m_transitionTime < m_transitionDuration);

	float blend = 0.0f;
	if (m_transitionDuration > 0.0f)
	{
		blend = std::clamp(m_transitionTime / m_transitionDuration, 0.0f, 1.0f);
		blend = blend * blend * (3.0f - 2.0f * blend);
	}

	blendedVars = sourceVars;

	for (size_t i = 0; i < blendedVars.size(); ++i)
	{
		blendedVars[i].value[0] = sourceVars[i].value[0] + (targetVars[i].value[0] - sourceVars[i].value[0]) * blend;
		blendedVars[i].value[1] = sourceVars[i].value[1] + (targetVars[i].value[1] - sourceVars[i].value[1]) * blend;
		blendedVars[i].value[2] = sourceVars[i].value[2] + (targetVars[i].value[2] - sourceVars[i].value[2]) * blend;
	}

	auto drawValueBar = [&](float& y, int id, const ColorF& c0, const ColorF& c1, float forcedMax = -1.0f)
		{
			const Variable& blendedVar = blendedVars[id];
			const Variable& targetVar = targetVars[id];

			const float currentValue = blendedVar.value[0];
			const float targetValue = targetVar.value[0];

			float displayMax = forcedMax;
			if (displayMax <= 0.0f)
			{
				displayMax = std::max(std::fabs(currentValue), 1.0f);
				if (hasTransition)
				{
					displayMax = std::max(displayMax, std::fabs(targetValue));
				}
			}

			const float progress = std::clamp(currentValue / displayMax, 0.0f, 1.0f);
			const bool showTarget = hasTransition && (std::fabs(targetValue - currentValue) > VALUE_EPSILON);

			char desc[128];
			std::snprintf(desc, sizeof(desc), "%s", blendedVar.name.data());

			char currLabel[64];
			std::snprintf(currLabel, sizeof(currLabel), "%.2f", currentValue);

			char targetLabel[64] = "";
			if (showTarget)
			{
				std::snprintf(targetLabel, sizeof(targetLabel), "%.2f", targetValue);
			}

			const ColorF targetColor = (targetValue < currentValue) ? Col_Orange : Col_Green;

			DrawBar(
				S(BAR_X), y,
				S(BAR_W), S(14.0f), S(2.0f),
				progress,
				c0, c1,
				desc,
				currLabel,
				targetLabel,
				Col_White,
				targetColor,
				0.45f,
				showTarget
			);

			y += S(18.0f);
		};

	auto drawColorBar = [&](float& y, int id, int component, const char* suffix, const ColorF& c0, const ColorF& c1)
		{
			const Variable& blendedVar = blendedVars[id];
			const Variable& targetVar = targetVars[id];

			const float currentValue = blendedVar.value[component];
			const float targetValue = targetVar.value[component];

			float displayMax = std::max(currentValue, 1.0f);
			if (hasTransition)
			{
				displayMax = std::max(displayMax, targetValue);
			}

			const float progress = std::clamp(currentValue / displayMax, 0.0f, 1.0f);
			const bool showTarget = hasTransition && (std::fabs(targetValue - currentValue) > VALUE_EPSILON);

			char desc[128];
			std::snprintf(desc, sizeof(desc), "%s %s", blendedVar.name.data(), suffix);

			char currLabel[64];
			std::snprintf(currLabel, sizeof(currLabel), "%.2f", currentValue);

			char targetLabel[64] = "";
			if (showTarget)
			{
				std::snprintf(targetLabel, sizeof(targetLabel), "%.2f", targetValue);
			}

			const ColorF targetColor = (targetValue < currentValue) ? Col_Orange : Col_Green;

			DrawBar(
				S(BAR_X), y,
				S(BAR_W), S(12.0f), S(2.0f),
				progress,
				c0, c1,
				desc,
				currLabel,
				targetLabel,
				Col_White,
				targetColor,
				0.40f,
				showTarget
			);

			y += S(16.0f);
		};

	float y = S(40.0f);

	if (m_debug == 1)
	{
		DrawModeHeader(y, "Overview", Col_Cyan);

		{
			float blendProgress = 0.0f;
			if (m_transitionDuration > 0.0f)
			{
				blendProgress = std::clamp(m_transitionTime / m_transitionDuration, 0.0f, 1.0f);
			}

			const bool showTarget = hasTransition && (std::fabs(m_transitionDuration - m_transitionTime) > VALUE_EPSILON);

			char desc[64];
			std::snprintf(desc, sizeof(desc), hasTransition ? "Blending active..." : "Blend target reached");

			char currLabel[64];
			std::snprintf(currLabel, sizeof(currLabel), "%.2f", m_transitionTime);

			char targetLabel[64] = "";
			if (showTarget)
			{
				std::snprintf(targetLabel, sizeof(targetLabel), "%.2f", m_transitionDuration);
			}

			DrawBar(S(BAR_X), y, S(BAR_W), S(14.0f), S(2.0f), blendProgress, Col_DarkGray, Col_LightGray, desc, currLabel, targetLabel, Col_White, Col_Green, 0.5f, showTarget);
			y += S(22.0f);
		}

		{
			const float timeProgress = std::clamp(m_currentTime / 24.0f, 0.0f, 1.0f);

			char currLabel[64];
			std::snprintf(currLabel, sizeof(currLabel), "%.2f", m_currentTime);

			DrawBar(S(BAR_X), y, S(BAR_W), S(14.0f), S(2.0f), timeProgress, Col_DarkGray, Col_Cyan, "Clock", currLabel, "", Col_White, Col_Green, 0.5f, false);
			y += S(22.0f);
		}

		{
			const float hdrDisplayMax = 10.0f;
			const float hdrProgress = std::clamp(m_HDRMultiplier / hdrDisplayMax, 0.0f, 1.0f);

			char currLabel[64];
			std::snprintf(currLabel, sizeof(currLabel), "%.3f", m_HDRMultiplier);

			DrawBar(S(BAR_X), y, S(BAR_W), S(14.0f), S(2.0f), hdrProgress, Col_DarkGray, Col_White, "HDR", currLabel, "", Col_White, Col_Green, 0.5f, false);
			y += S(24.0f);
		}

		drawValueBar(y, SUN_COLOR_MULTIPLIER, Col_DarkGray, Col_Yellow, 16.0f);
		drawValueBar(y, SKY_COLOR_MULTIPLIER, Col_DarkGray, Col_LightBlue, 16.0f);
		drawValueBar(y, FOG_COLOR_MULTIPLIER, Col_DarkGray, Col_LightGray, 16.0f);
		drawValueBar(y, SKY_BRIGHTENING, Col_DarkGray, Col_Cyan, 1.0f);
		drawValueBar(y, SSAO_AMOUNT_MULTIPLIER, Col_DarkGray, Col_Green, 2.5f);
		drawValueBar(y, SUN_SHAFTS_VISIBILITY, Col_DarkGray, Col_Orange, 1.0f);
		drawValueBar(y, SUN_RAYS_VISIBILITY, Col_DarkGray, Col_Red, 10.0f);
		drawValueBar(y, COLOR_SATURATION, Col_DarkGray, Col_Green, 10.0f);
		drawValueBar(y, COLOR_CONTRAST, Col_DarkGray, Col_Yellow, 10.0f);
		drawValueBar(y, COLOR_BRIGHTNESS, Col_DarkGray, Col_White, 10.0f);

		y += S(6.0f);

		drawColorBar(y, SUN_COLOR, 0, "R", Col_Black, Col_Red);
		drawColorBar(y, SUN_COLOR, 1, "G", Col_Black, Col_Green);
		drawColorBar(y, SUN_COLOR, 2, "B", Col_Black, Col_Blue);

		drawColorBar(y, SKY_COLOR, 0, "R", Col_Black, Col_Red);
		drawColorBar(y, SKY_COLOR, 1, "G", Col_Black, Col_Green);
		drawColorBar(y, SKY_COLOR, 2, "B", Col_Black, Col_Blue);

		drawColorBar(y, FOG_COLOR, 0, "R", Col_Black, Col_Red);
		drawColorBar(y, FOG_COLOR, 1, "G", Col_Black, Col_Green);
		drawColorBar(y, FOG_COLOR, 2, "B", Col_Black, Col_Blue);

		return;
	}

	if (m_debug == 2)
	{
		DrawModeHeader(y, "Atmosphere / Fog / Skylight", Col_LightBlue);

		drawValueBar(y, HDR_DYNAMIC_POWER_FACTOR, Col_DarkGray, Col_White, 4.0f);
		drawValueBar(y, SKY_BRIGHTENING, Col_DarkGray, Col_Cyan, 1.0f);
		drawValueBar(y, SSAO_AMOUNT_MULTIPLIER, Col_DarkGray, Col_Green, 2.5f);
		drawValueBar(y, VOLUMETRIC_FOG_GLOBAL_DENSITY, Col_DarkGray, Col_LightGray, 1.0f);
		drawValueBar(y, VOLUMETRIC_FOG_ATMOSPHERE_HEIGHT, Col_DarkGray, Col_Cyan, 30000.0f);
		drawValueBar(y, VOLUMETRIC_FOG_DENSITY_OFFSET, Col_DarkGray, Col_Orange, 1.0f);
		drawColorBar(y, SKY_LIGHT_SUN_INTENSITY, 0, "R", Col_Black, Col_Red);
		drawColorBar(y, SKY_LIGHT_SUN_INTENSITY, 1, "G", Col_Black, Col_Green);
		drawColorBar(y, SKY_LIGHT_SUN_INTENSITY, 2, "B", Col_Black, Col_Blue);
		drawValueBar(y, SKY_LIGHT_SUN_INTENSITY_MULTIPLIER, Col_DarkGray, Col_Yellow, 100.0f);
		drawValueBar(y, SKY_LIGHT_MIE_SCATTERING, Col_DarkGray, Col_Orange, 10.0f);
		drawValueBar(y, SKY_LIGHT_RAYLEIGH_SCATTERING, Col_DarkGray, Col_SkyBlue, 10.0f);
		drawValueBar(y, SKY_LIGHT_SUN_ANISOTROPY_FACTOR, Col_DarkGray, Col_Green, 1.0f);
		drawValueBar(y, SKY_LIGHT_WAVELENGTH_R, Col_DarkGray, Col_Red, 780.0f);
		drawValueBar(y, SKY_LIGHT_WAVELENGTH_G, Col_DarkGray, Col_Green, 780.0f);
		drawValueBar(y, SKY_LIGHT_WAVELENGTH_B, Col_DarkGray, Col_Blue, 780.0f);
		drawValueBar(y, CLOUD_SHADING_SUN_LIGHT_MULTIPLIER, Col_DarkGray, Col_Yellow, 16.0f);
		drawValueBar(y, CLOUD_SHADING_SKY_LIGHT_MULTIPLIER, Col_DarkGray, Col_Cyan, 16.0f);
		drawValueBar(y, OCEAN_FOG_COLOR_MULTIPLIER, Col_DarkGray, Col_LightBlue, 1.0f);
		drawValueBar(y, SKYBOX_MULTIPLIER, Col_DarkGray, Col_White, 1.0f);
		drawValueBar(y, SUN_SPECULAR_MULTIPLIER, Col_DarkGray, Col_Yellow, 4.0f);
		drawValueBar(y, SUN_RAYS_ATTENUATION, Col_DarkGray, Col_Orange, 10.0f);

		return;
	}

	if (m_debug == 3)
	{
		DrawModeHeader(y, "Night Sky / Moon", Col_SkyBlue);

		drawColorBar(y, NIGHT_SKY_HORIZON_COLOR, 0, "R", Col_Black, Col_Red);
		drawColorBar(y, NIGHT_SKY_HORIZON_COLOR, 1, "G", Col_Black, Col_Green);
		drawColorBar(y, NIGHT_SKY_HORIZON_COLOR, 2, "B", Col_Black, Col_Blue);
		drawValueBar(y, NIGHT_SKY_HORIZON_COLOR_MULTIPLIER, Col_DarkGray, Col_Orange, 16.0f);

		drawColorBar(y, NIGHT_SKY_ZENITH_COLOR, 0, "R", Col_Black, Col_Red);
		drawColorBar(y, NIGHT_SKY_ZENITH_COLOR, 1, "G", Col_Black, Col_Green);
		drawColorBar(y, NIGHT_SKY_ZENITH_COLOR, 2, "B", Col_Black, Col_Blue);
		drawValueBar(y, NIGHT_SKY_ZENITH_COLOR_MULTIPLIER, Col_DarkGray, Col_SkyBlue, 16.0f);

		drawValueBar(y, NIGHT_SKY_ZENITH_SHIFT, Col_DarkGray, Col_Cyan, 1.0f);
		drawValueBar(y, NIGHT_SKY_STAR_INTENSITY, Col_DarkGray, Col_White, 3.0f);

		drawColorBar(y, NIGHT_SKY_MOON_COLOR, 0, "R", Col_Black, Col_Red);
		drawColorBar(y, NIGHT_SKY_MOON_COLOR, 1, "G", Col_Black, Col_Green);
		drawColorBar(y, NIGHT_SKY_MOON_COLOR, 2, "B", Col_Black, Col_Blue);
		drawValueBar(y, NIGHT_SKY_MOON_COLOR_MULTIPLIER, Col_DarkGray, Col_White, 16.0f);

		drawValueBar(y, NIGHT_SKY_MOON_INNER_CORONA_SCALE, Col_DarkGray, Col_Cyan, 2.0f);
		drawValueBar(y, NIGHT_SKY_MOON_OUTER_CORONA_SCALE, Col_DarkGray, Col_SkyBlue, 2.0f);
		drawValueBar(y, NIGHT_SKY_MOON_INNER_CORONA_COLOR_MULTIPLIER, Col_DarkGray, Col_Orange, 16.0f);
		drawValueBar(y, NIGHT_SKY_MOON_OUTER_CORONA_COLOR_MULTIPLIER, Col_DarkGray, Col_LightBlue, 16.0f);

		return;
	}

	if (m_debug == 4)
	{
		DrawModeHeader(y, "Color Grading / PostFX", Col_SpringGreen);

		drawValueBar(y, COLOR_SATURATION, Col_DarkGray, Col_Green, 10.0f);
		drawValueBar(y, COLOR_CONTRAST, Col_DarkGray, Col_Yellow, 10.0f);
		drawValueBar(y, COLOR_BRIGHTNESS, Col_DarkGray, Col_White, 10.0f);
		drawValueBar(y, LEVELS_MIN_INPUT, Col_DarkGray, Col_Cyan, 255.0f);
		drawValueBar(y, LEVELS_GAMMA, Col_DarkGray, Col_Orange, 10.0f);
		drawValueBar(y, LEVELS_MAX_INPUT, Col_DarkGray, Col_Yellow, 255.0f);
		drawValueBar(y, LEVELS_MIN_OUTPUT, Col_DarkGray, Col_Cyan, 255.0f);
		drawValueBar(y, LEVELS_MAX_OUTPUT, Col_DarkGray, Col_Yellow, 255.0f);
		drawColorBar(y, SELECTIVE_COLOR, 0, "R", Col_Black, Col_Red);
		drawColorBar(y, SELECTIVE_COLOR, 1, "G", Col_Black, Col_Green);
		drawColorBar(y, SELECTIVE_COLOR, 2, "B", Col_Black, Col_Blue);
		drawValueBar(y, SELECTIVE_COLOR_CYANS, Col_DarkGray, Col_Cyan, 100.0f);
		drawValueBar(y, SELECTIVE_COLOR_MAGENTAS, Col_DarkGray, Col_Magenta, 100.0f);
		drawValueBar(y, SELECTIVE_COLOR_YELLOWS, Col_DarkGray, Col_Yellow, 100.0f);
		drawValueBar(y, SELECTIVE_COLOR_BLACKS, Col_DarkGray, Col_Gray, 100.0f);
		drawValueBar(y, FILTERS_GRAIN, Col_DarkGray, Col_White, 1.0f);
		drawValueBar(y, FILTERS_SHARPENING, Col_DarkGray, Col_Orange, 1.0f);
		drawColorBar(y, FILTERS_PHOTOFILTER_COLOR, 0, "R", Col_Black, Col_Red);
		drawColorBar(y, FILTERS_PHOTOFILTER_COLOR, 1, "G", Col_Black, Col_Green);
		drawColorBar(y, FILTERS_PHOTOFILTER_COLOR, 2, "B", Col_Black, Col_Blue);
		drawValueBar(y, FILTERS_PHOTOFILTER_DENSITY, Col_DarkGray, Col_Orange, 1.0f);
		drawValueBar(y, EYEADAPTION_CLAMP, Col_DarkGray, Col_Cyan, 10.0f);
		drawValueBar(y, DOF_FOCUS_RANGE, Col_DarkGray, Col_LightBlue, 10000.0f);
		drawValueBar(y, DOF_BLUR_AMOUNT, Col_DarkGray, Col_White, 1.0f);

		return;
	}

	if (m_debug == 5)
	{
		DrawModeHeader(y, "WeatherSystem Post Multipliers", Col_SpringGreen);

		I3DEngine* p3DEngine = gEnv->p3DEngine;

		const float dawnStart = p3DEngine->GetDawnStart();
		const float dawnEnd = p3DEngine->GetDawnEnd();
		const float duskStart = p3DEngine->GetDuskStart();
		const float duskEnd = p3DEngine->GetDuskEnd();

		float sunColorMultiplier = m_HDRMultiplier;

		if (m_currentTime < dawnStart || m_currentTime >= duskEnd)
		{
			sunColorMultiplier = 0.0f;
		}
		else if (m_currentTime < dawnEnd)
		{
			const float dawnMiddle = 0.5f * (dawnStart + dawnEnd);

			if (m_currentTime < dawnMiddle)
			{
				sunColorMultiplier *= (dawnMiddle - m_currentTime) / (dawnMiddle - dawnStart);
			}
			else
			{
				const float sunIntensityMultiplier = (m_currentTime - dawnMiddle) / (dawnEnd - dawnMiddle);
				sunColorMultiplier *= sunIntensityMultiplier;
			}
		}
		else if (m_currentTime < duskEnd && m_currentTime >= duskStart)
		{
			const float duskMiddle = 0.5f * (duskStart + duskEnd);

			if (m_currentTime < duskMiddle)
			{
				const float sunIntensityMultiplier = (duskMiddle - m_currentTime) / (duskMiddle - duskStart);
				sunColorMultiplier *= sunIntensityMultiplier;
			}
			else
			{
				sunColorMultiplier *= (m_currentTime - duskMiddle) / (duskEnd - duskMiddle);
			}
		}

		Vec3 baseSun(
			blendedVars[SUN_COLOR].value[0],
			blendedVars[SUN_COLOR].value[1],
			blendedVars[SUN_COLOR].value[2]
		);
		baseSun *= (blendedVars[SUN_COLOR_MULTIPLIER].value[0] * sunColorMultiplier);

		Vec3 baseSky(
			blendedVars[SKY_COLOR].value[0],
			blendedVars[SKY_COLOR].value[1],
			blendedVars[SKY_COLOR].value[2]
		);
		baseSky *= (blendedVars[SKY_COLOR_MULTIPLIER].value[0] * m_HDRMultiplier);

		Vec3 baseFog(
			blendedVars[FOG_COLOR].value[0],
			blendedVars[FOG_COLOR].value[1],
			blendedVars[FOG_COLOR].value[2]
		);
		baseFog *= (blendedVars[FOG_COLOR_MULTIPLIER].value[0] * m_HDRMultiplier);

		const Vec3 finalSun = p3DEngine->GetSunColor();
		const Vec3 finalSky = p3DEngine->GetSkyColor();
		const Vec3 finalFog = p3DEngine->GetFogColor();

		const Vec3 sunPostMul = ComputePostMultiplier(baseSun, finalSun);
		const Vec3 skyPostMul = ComputePostMultiplier(baseSky, finalSky);
		const Vec3 fogPostMul = ComputePostMultiplier(baseFog, finalFog);

		DrawPostMulBar(y, "Sun PostMul R", sunPostMul.x, Col_Orange, Col_Green);
		DrawPostMulBar(y, "Sun PostMul G", sunPostMul.y, Col_Orange, Col_Green);
		DrawPostMulBar(y, "Sun PostMul B", sunPostMul.z, Col_Orange, Col_Green);

		y += S(4.0f);

		DrawPostMulBar(y, "Sky PostMul R", skyPostMul.x, Col_Orange, Col_Green);
		DrawPostMulBar(y, "Sky PostMul G", skyPostMul.y, Col_Orange, Col_Green);
		DrawPostMulBar(y, "Sky PostMul B", skyPostMul.z, Col_Orange, Col_Green);

		y += S(4.0f);

		DrawPostMulBar(y, "Fog PostMul R", fogPostMul.x, Col_Orange, Col_Green);
		DrawPostMulBar(y, "Fog PostMul G", fogPostMul.y, Col_Orange, Col_Green);
		DrawPostMulBar(y, "Fog PostMul B", fogPostMul.z, Col_Orange, Col_Green);

		return;
	}

	DrawModeHeader(y, "Unknown", Col_Orange);
	DrawAlignedLabel(S(BAR_X), y, 1.2f, Col_White, false, "Valid modes: 1=Overview 2=Atmosphere 3=Night 4=PostFX 5=WeatherSystem");
}
