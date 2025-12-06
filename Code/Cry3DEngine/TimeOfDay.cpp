#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <numbers>

#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CrySystem/ITimer.h"
#include "CryCommon/CryNetwork/ISerialize.h"
#include "CryCommon/CryRenderer/IRenderer.h"

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
	if (m_paused || m_editMode || std::fabs(m_speed) <= 0.0001)
	{
		return;
	}

	float newTime = m_currentTime + (m_speed * m_timer->GetFrameTime());

	if (m_startTime <= 0.05 && m_endTime >= 23.5)
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
	else if (std::fabs(m_startTime - m_endTime) <= 0.05)
	{
		if (newTime > 24)
		{
			newTime -= 24;
		}
		else if (newTime < 0)
		{
			newTime += 24;
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

void TimeOfDay::Update(bool interpolate, bool forceUpdate)
{
	if (!gEnv->bClient) //CryMP: For Dedicated server this can be skipped
		return;

	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_3DENGINE);

	if (interpolate)
	{
		const float time = m_currentTime / 24;

		for (Variable& var : m_vars)
		{
			this->InterpolateVariable(var, time);
		}
	}

	I3DEngine* p3DEngine = gEnv->p3DEngine;

	if (gEnv->pRenderer->EF_Query(EFQ_HDRModeEnabled))
	{
		const float base = p3DEngine->GetHDRDynamicMultiplier();
		const float exponent = m_vars[HDR_DYNAMIC_POWER_FACTOR].value[0];

		m_HDRMultiplier = std::pow(base, exponent);
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
		const float dawnMiddle = 0.5 * (dawnStart + dawnEnd);

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
		const float duskMiddle = 0.5 * (duskStart + duskEnd);

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
			m_vars[SUN_COLOR].value[0],
			m_vars[SUN_COLOR].value[1],
			m_vars[SUN_COLOR].value[2]
		) * (
			m_vars[SUN_COLOR_MULTIPLIER].value[0] * sunColorMultiplier
		)
	);

	p3DEngine->SetSunSpecMultiplier(m_vars[SUN_SPECULAR_MULTIPLIER].value[0]);
	p3DEngine->SetSkyBrightness(m_vars[SKY_BRIGHTENING].value[0]);
	p3DEngine->SetSSAOAmount(m_vars[SSAO_AMOUNT_MULTIPLIER].value[0]);

	p3DEngine->SetSkyColor(
		Vec3(
			m_vars[SKY_COLOR].value[0],
			m_vars[SKY_COLOR].value[1],
			m_vars[SKY_COLOR].value[2]
		) * (
			m_vars[SKY_COLOR_MULTIPLIER].value[0] * m_HDRMultiplier
		)
	);

	p3DEngine->SetFogColor(
		Vec3(
			m_vars[FOG_COLOR].value[0],
			m_vars[FOG_COLOR].value[1],
			m_vars[FOG_COLOR].value[2]
		) * (
			m_vars[FOG_COLOR_MULTIPLIER].value[0] * m_HDRMultiplier
		)
	);

	p3DEngine->SetVolumetricFogSettings(
		m_vars[VOLUMETRIC_FOG_GLOBAL_DENSITY].value[0],
		m_vars[VOLUMETRIC_FOG_ATMOSPHERE_HEIGHT].value[0],
		m_vars[VOLUMETRIC_FOG_DENSITY_OFFSET].value[0]
	);

	p3DEngine->SetSkyLightParameters(
		Vec3(
			m_vars[SKY_LIGHT_SUN_INTENSITY].value[0],
			m_vars[SKY_LIGHT_SUN_INTENSITY].value[1],
			m_vars[SKY_LIGHT_SUN_INTENSITY].value[2]
		) * (
			m_vars[SKY_LIGHT_SUN_INTENSITY_MULTIPLIER].value[0] * sunIntensityMultiplier
		),
		m_vars[SKY_LIGHT_MIE_SCATTERING].value[0],
		m_vars[SKY_LIGHT_RAYLEIGH_SCATTERING].value[0],
		m_vars[SKY_LIGHT_SUN_ANISOTROPY_FACTOR].value[0],
		Vec3(
			m_vars[SKY_LIGHT_WAVELENGTH_R].value[0],
			m_vars[SKY_LIGHT_WAVELENGTH_G].value[0],
			m_vars[SKY_LIGHT_WAVELENGTH_B].value[0]
		),
		forceUpdate
	);

	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_HORIZON_COLOR,
		Vec3(
			m_vars[NIGHT_SKY_HORIZON_COLOR].value[0],
			m_vars[NIGHT_SKY_HORIZON_COLOR].value[1],
			m_vars[NIGHT_SKY_HORIZON_COLOR].value[2]
		) * (
			m_vars[NIGHT_SKY_HORIZON_COLOR_MULTIPLIER].value[0] * m_HDRMultiplier
		)
	);

	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_ZENITH_COLOR,
		Vec3(
			m_vars[NIGHT_SKY_ZENITH_COLOR].value[0],
			m_vars[NIGHT_SKY_ZENITH_COLOR].value[1],
			m_vars[NIGHT_SKY_ZENITH_COLOR].value[2]
		) * (
			m_vars[NIGHT_SKY_ZENITH_COLOR_MULTIPLIER].value[0] * m_HDRMultiplier
		)
	);

	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_ZENITH_SHIFT, m_vars[NIGHT_SKY_ZENITH_SHIFT].value[0]);
	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_STAR_INTENSITY, m_vars[NIGHT_SKY_STAR_INTENSITY].value[0]);

	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_MOON_COLOR,
		Vec3(
			m_vars[NIGHT_SKY_MOON_COLOR].value[0],
			m_vars[NIGHT_SKY_MOON_COLOR].value[1],
			m_vars[NIGHT_SKY_MOON_COLOR].value[2]
		) * (
			m_vars[NIGHT_SKY_MOON_COLOR_MULTIPLIER].value[0] * m_HDRMultiplier
		)
	);

	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_MOON_INNERCORONA_COLOR,
		Vec3(
			m_vars[NIGHT_SKY_MOON_INNER_CORONA_COLOR].value[0],
			m_vars[NIGHT_SKY_MOON_INNER_CORONA_COLOR].value[1],
			m_vars[NIGHT_SKY_MOON_INNER_CORONA_COLOR].value[2]
		) * (
			m_vars[NIGHT_SKY_MOON_INNER_CORONA_COLOR_MULTIPLIER].value[0] * m_HDRMultiplier
		)
	);

	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_MOON_INNERCORONA_SCALE,
		m_vars[NIGHT_SKY_MOON_INNER_CORONA_SCALE].value[0]
	);

	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_MOON_OUTERCORONA_COLOR,
		Vec3(
			m_vars[NIGHT_SKY_MOON_OUTER_CORONA_COLOR].value[0],
			m_vars[NIGHT_SKY_MOON_OUTER_CORONA_COLOR].value[1],
			m_vars[NIGHT_SKY_MOON_OUTER_CORONA_COLOR].value[2]
		) * (
			m_vars[NIGHT_SKY_MOON_OUTER_CORONA_COLOR_MULTIPLIER].value[0] * m_HDRMultiplier
		)
	);

	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_MOON_OUTERCORONA_SCALE,
		m_vars[NIGHT_SKY_MOON_OUTER_CORONA_SCALE].value[0]
	);

	float sunShaftsVis = m_vars[SUN_SHAFTS_VISIBILITY].value[0];
	sunShaftsVis = std::clamp<float>(sunShaftsVis, 0.0f, 0.3f);
	float sunRaysVis = m_vars[SUN_RAYS_VISIBILITY].value[0];

	p3DEngine->SetPostEffectParam("SunShafts_Active",
		(sunShaftsVis > 0.05 || sunRaysVis > 0.05) ? 1 : 0);
	p3DEngine->SetPostEffectParam("SunShafts_Amount", sunShaftsVis);
	p3DEngine->SetPostEffectParam("SunShafts_RaysAmount", sunRaysVis);
	p3DEngine->SetPostEffectParam("SunShafts_RaysAttenuation", m_vars[SUN_RAYS_ATTENUATION].value[0]);

	p3DEngine->SetCloudShadingMultiplier(
		m_vars[CLOUD_SHADING_SUN_LIGHT_MULTIPLIER].value[0],
		m_vars[CLOUD_SHADING_SKY_LIGHT_MULTIPLIER].value[0]
	);

	p3DEngine->SetGlobalParameter(E3DPARAM_OCEANFOG_COLOR_MULTIPLIER, m_vars[OCEAN_FOG_COLOR_MULTIPLIER].value[0]);
	p3DEngine->SetGlobalParameter(E3DPARAM_SKYBOX_MULTIPLIER, m_vars[SKYBOX_MULTIPLIER].value[0] * m_HDRMultiplier);
	p3DEngine->SetGlobalParameter(E3DPARAM_EYEADAPTIONCLAMP, m_vars[EYEADAPTION_CLAMP].value[0]);

	p3DEngine->SetGlobalParameter(E3DPARAM_COLORGRADING_COLOR_SATURATION, m_vars[COLOR_SATURATION].value[0]);
	p3DEngine->SetPostEffectParam("ColorGrading_Contrast", m_vars[COLOR_CONTRAST].value[0]);
	p3DEngine->SetPostEffectParam("ColorGrading_Brightness", m_vars[COLOR_BRIGHTNESS].value[0]);
	p3DEngine->SetPostEffectParam("ColorGrading_minInput", m_vars[LEVELS_MIN_INPUT].value[0]);
	p3DEngine->SetPostEffectParam("ColorGrading_gammaInput", m_vars[LEVELS_GAMMA].value[0]);
	p3DEngine->SetPostEffectParam("ColorGrading_maxInput", m_vars[LEVELS_MAX_INPUT].value[0]);
	p3DEngine->SetPostEffectParam("ColorGrading_minOutput", m_vars[LEVELS_MIN_OUTPUT].value[0]);
	p3DEngine->SetPostEffectParam("ColorGrading_maxOutput", m_vars[LEVELS_MAX_OUTPUT].value[0]);

	p3DEngine->SetPostEffectParamVec4("clr_ColorGrading_SelectiveColor",
		Vec4(
			m_vars[SELECTIVE_COLOR].value[0],
			m_vars[SELECTIVE_COLOR].value[1],
			m_vars[SELECTIVE_COLOR].value[2],
			1
		)
	);

	p3DEngine->SetPostEffectParam("ColorGrading_SelectiveColorCyans", m_vars[SELECTIVE_COLOR_CYANS].value[0]);
	p3DEngine->SetPostEffectParam("ColorGrading_SelectiveColorMagentas", m_vars[SELECTIVE_COLOR_MAGENTAS].value[0]);
	p3DEngine->SetPostEffectParam("ColorGrading_SelectiveColorYellows", m_vars[SELECTIVE_COLOR_YELLOWS].value[0]);
	p3DEngine->SetPostEffectParam("ColorGrading_SelectiveColorBlacks", m_vars[SELECTIVE_COLOR_BLACKS].value[0]);
	p3DEngine->SetGlobalParameter(E3DPARAM_COLORGRADING_FILTERS_GRAIN, m_vars[FILTERS_GRAIN].value[0]);
	p3DEngine->SetPostEffectParam("ColorGrading_SharpenAmount", m_vars[FILTERS_SHARPENING].value[0]);

	p3DEngine->SetGlobalParameter(E3DPARAM_COLORGRADING_FILTERS_PHOTOFILTER_COLOR,
		Vec3(
			m_vars[FILTERS_PHOTOFILTER_COLOR].value[0],
			m_vars[FILTERS_PHOTOFILTER_COLOR].value[1],
			m_vars[FILTERS_PHOTOFILTER_COLOR].value[2]
		)
	);

	p3DEngine->SetGlobalParameter(E3DPARAM_COLORGRADING_FILTERS_PHOTOFILTER_DENSITY,
		m_vars[FILTERS_PHOTOFILTER_DENSITY].value[0]
	);

	p3DEngine->SetPostEffectParam("Dof_Tod_FocusRange", m_vars[DOF_FOCUS_RANGE].value[0]);
	p3DEngine->SetPostEffectParam("Dof_Tod_BlurAmount", m_vars[DOF_BLUR_AMOUNT].value[0]);
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
				newTime = (m_currentTime * 0.95) + (remoteTime * 0.05);

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
	init(SKY_BRIGHTENING, "Sky brightening (terrain occlusion)", TYPE_FLOAT, 0.3, 0, 1);
	init(SSAO_AMOUNT_MULTIPLIER, "SSAO amount multiplier", TYPE_FLOAT, 1, 0, 2.5);
	init(SUN_COLOR, "Sun color", TYPE_COLOR, 215.0 / 255.0, 200.0 / 255.0, 170.0 / 255.0);
	init(SUN_COLOR_MULTIPLIER, "Sun color multiplier", TYPE_FLOAT, 2.4, 0, 16);
	init(SUN_SPECULAR_MULTIPLIER, "Sun specular multiplier", TYPE_FLOAT, 1, 0, 4);
	init(SKY_COLOR, "Sky color", TYPE_COLOR, 160.0 / 255.0, 200.0 / 255.0, 240.0 / 255.0);
	init(SKY_COLOR_MULTIPLIER, "Sky color multiplier", TYPE_FLOAT, 1.1, 0, 16);
	init(FOG_COLOR, "Fog color", TYPE_COLOR, 0, 0, 0);
	init(FOG_COLOR_MULTIPLIER, "Fog color multiplier", TYPE_FLOAT, 0, 0, 16);
	init(VOLUMETRIC_FOG_GLOBAL_DENSITY, "Volumetric fog: Global density", TYPE_FLOAT, 0.02, 0, 100);
	init(VOLUMETRIC_FOG_ATMOSPHERE_HEIGHT, "Volumetric fog: Atmosphere height", TYPE_FLOAT, 4'000, 100, 30'000);
	init(VOLUMETRIC_FOG_DENSITY_OFFSET, "Volumetric fog: Density offset (in reality 0)", TYPE_FLOAT, 0, 0, 100);
	init(SKY_LIGHT_SUN_INTENSITY, "Sky light: Sun intensity", TYPE_COLOR, 5.0 / 6.0, 5.0 / 6.0, 1);
	init(SKY_LIGHT_SUN_INTENSITY_MULTIPLIER, "Sky light: Sun intensity multiplier", TYPE_FLOAT, 30, 0, 1000);
	init(SKY_LIGHT_MIE_SCATTERING, "Sky light: Mie scattering", TYPE_FLOAT, 4.8, 0, 1'000'000);
	init(SKY_LIGHT_RAYLEIGH_SCATTERING, "Sky light: Rayleigh scattering", TYPE_FLOAT, 2, 0, 1'000'000);
	init(SKY_LIGHT_SUN_ANISOTROPY_FACTOR, "Sky light: Sun anisotropy factor", TYPE_FLOAT, -0.995, -0.999'9, 0.999'9);
	init(SKY_LIGHT_WAVELENGTH_R, "Sky light: Wavelength (R)", TYPE_FLOAT, 750, 380, 780);
	init(SKY_LIGHT_WAVELENGTH_G, "Sky light: Wavelength (G)", TYPE_FLOAT, 601, 380, 780);
	init(SKY_LIGHT_WAVELENGTH_B, "Sky light: Wavelength (B)", TYPE_FLOAT, 555, 380, 780);
	init(NIGHT_SKY_HORIZON_COLOR, "Night sky: Horizon color", TYPE_COLOR, 222.0 / 255.0, 148.0 / 255.0, 47.0 / 255.0);
	init(NIGHT_SKY_HORIZON_COLOR_MULTIPLIER, "Night sky: Horizon color multiplier", TYPE_FLOAT, 1, 0, 16);
	init(NIGHT_SKY_ZENITH_COLOR, "Night sky: Zenith color", TYPE_COLOR, 17.0 / 255.0, 38.0 / 255.0, 78.0 / 255.0);
	init(NIGHT_SKY_ZENITH_COLOR_MULTIPLIER, "Night sky: Zenith color multiplier", TYPE_FLOAT, 0.25, 0, 16);
	init(NIGHT_SKY_ZENITH_SHIFT, "Night sky: Zenith shift", TYPE_FLOAT, 0.25, 0, 1);
	init(NIGHT_SKY_STAR_INTENSITY, "Night sky: Star intensity", TYPE_FLOAT, 0, 0, 3);
	init(NIGHT_SKY_MOON_COLOR, "Night sky: Moon color", TYPE_COLOR, 1, 1, 1);
	init(NIGHT_SKY_MOON_COLOR_MULTIPLIER, "Night sky: Moon color multiplier", TYPE_FLOAT, 0, 0, 16);
	init(NIGHT_SKY_MOON_INNER_CORONA_COLOR, "Night sky: Moon inner corona color", TYPE_COLOR, 230.0 / 255.0, 1, 1);
	init(NIGHT_SKY_MOON_INNER_CORONA_COLOR_MULTIPLIER, "Night sky: Moon inner corona color multiplier", TYPE_FLOAT, 0, 0, 16);
	init(NIGHT_SKY_MOON_INNER_CORONA_SCALE, "Night sky: Moon inner corona scale", TYPE_FLOAT, 0.499, 0, 2);
	init(NIGHT_SKY_MOON_OUTER_CORONA_COLOR, "Night sky: Moon outer corona color", TYPE_COLOR, 128.0 / 255.0, 200.0 / 255.0, 1);
	init(NIGHT_SKY_MOON_OUTER_CORONA_COLOR_MULTIPLIER, "Night sky: Moon outer corona color multiplier", TYPE_FLOAT, 0, 0, 16);
	init(NIGHT_SKY_MOON_OUTER_CORONA_SCALE, "Night sky: Moon outer corona scale", TYPE_FLOAT, 0.006, 0, 2);
	init(CLOUD_SHADING_SUN_LIGHT_MULTIPLIER, "Cloud shading: Sun light multiplier", TYPE_FLOAT, 1.96, 0, 16);
	init(CLOUD_SHADING_SKY_LIGHT_MULTIPLIER, "Cloud shading: Sky light multiplier", TYPE_FLOAT, 0.8, 0, 16);
	init(SUN_SHAFTS_VISIBILITY, "Sun shafts visibility", TYPE_FLOAT, 0.25, 0, 1);
	init(SUN_RAYS_VISIBILITY, "Sun rays visibility", TYPE_FLOAT, 2.5, 0, 10);
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
	init(FILTERS_PHOTOFILTER_COLOR, "Filters: photofilter color", TYPE_COLOR, 0.952, 0.517, 0.09);
	init(FILTERS_PHOTOFILTER_DENSITY, "Filters: photofilter density", TYPE_FLOAT, 0, 0, 1);
	init(EYEADAPTION_CLAMP, "EyeAdaption: Clamp", TYPE_FLOAT, 4, 0, 10);
	init(DOF_FOCUS_RANGE, "Dof: focus range", TYPE_FLOAT, 1'000, 0, 10'000);
	init(DOF_BLUR_AMOUNT, "Dof: blur amount", TYPE_FLOAT, 0, 0, 1);
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

void TimeOfDay::DeserializeVariable(const XmlNodeRef& node)
{
	const int index = this->FindVariableIndex(node->getAttr("Name"));
	if (index < 0)
	{
		return;
	}

	Variable& var = m_vars[index];

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

int TimeOfDay::FindVariableIndex(const std::string_view& name) const
{
	const auto lower = [](unsigned char ch) { return std::tolower(ch); };

	const auto it = std::ranges::find_if(m_vars,
		[&](const Variable& var)
		{
			return std::ranges::equal(name, var.name, {}, lower, lower);
		}
	);

	return it == m_vars.end() ? -1 : static_cast<int>(it - m_vars.begin());
}

Vec3 TimeOfDay::CalculateSunDirection(const Vec3& sunRotation) const
{
	using std::numbers::pi;

	const float rotationZ = ((m_currentTime + 12.0) / 24.0) * pi * 2.0;
	const float rotationX = (0.5 * pi) - ((pi * sunRotation.y) / 180.0);
	const float rotationY = pi * ((-sunRotation.x) / 180.0);

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

void TimeOfDay::LoadCustomSettings(string xmlPath)
{
	if (xmlPath.empty())
	{
		RestoreLevelDefaults();
		return;
	}
	XmlNodeRef node = gEnv->pSystem->LoadXmlFile(xmlPath.c_str());
	if (!node)
	{
		CryLogWarningAlways("Failed to load custom ToD settings from '%s'", xmlPath.c_str());
		return;
	}
	for (int i = 0; i < node->getChildCount(); i++)
	{
		this->DeserializeVariable(node->getChild(i));
	}
	Update();
	CryLog("$3[CryMP] Loaded custom ToD settings from '%s'", xmlPath.c_str());
}

void TimeOfDay::RestoreLevelDefaults()
{
	if (!m_defaultLevelSettings)
		return;

	for (int i = 0; i < m_defaultLevelSettings->getChildCount(); i++)
	{
		this->DeserializeVariable(m_defaultLevelSettings->getChild(i));
	}
	Update();
	CryLog("$3[CryMP] Restored default ToD xml settings");
}
