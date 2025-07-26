#include <array>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <sstream>
#include <vector>

#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/Cry3DEngine/I3DEngine.h"
#include "CryCommon/CryRenderer/IRenderer.h"
#include "CryGame/Game.h"
#include "Library/WinAPI.h"

#include "WeatherSystem.h"

bool CWeatherSystem::tod_hooked = false;

typedef void (ITimeOfDay::* ITODUpdate)(bool, bool);
static ITODUpdate pTODUpdate = nullptr;

CWeatherSystem::CWeatherSystem() {
	m_originalWeatherValues.clear();
	m_time = 0;
	m_lastUpdate = -1000.0f;

	if (!tod_hooked) {
		ITimeOfDay* pTOD = gEnv->p3DEngine->GetTimeOfDay();
		void** pTODVtable = *reinterpret_cast<void***>(pTOD);

		auto update = &CWeatherSystem::TODUpdate;
		pTODUpdate = *reinterpret_cast<ITODUpdate*>(pTODVtable + 9);

		WinAPI::FillMem(&pTODVtable[9], &reinterpret_cast<void*&>(update), sizeof(void*));
		tod_hooked = true;
	}
}

void CWeatherSystem::Reset(bool deapply) {
	RestoreWeather(true);

	if (deapply) {
		if (m_originalWind) {
			gEnv->p3DEngine->SetWind(*m_originalWind);
		}

		if (m_activeMask) {
			RemoveLayer(*m_activeMask);
		}

		for (auto& k : m_activeEffects) {
			if (k[0] == '-') SpawnEffect(k.c_str() + 1);
			else RemoveEffect(k.c_str());
		}
	}
	
	m_enabled = false;
	m_originalGroups.clear();
	m_originalWeatherValues.clear();
	m_originalWind.reset();

	m_activeEffects.clear();
	m_activeEmitters.clear();
	m_activeValues.clear();
	m_activeMask.reset();
}

void CWeatherSystem::Update(float frameTime) {
	CSynchedStorage *pSSS = g_pGame->GetSynchedStorage();
	if (!pSSS || !g_pGame->GetGameRules() || !gEnv->bClient) {
		return;
	}

	m_time += frameTime;

	bool b;
	if (!pSSS->GetGlobalValue(WEATHER_NAMESPACE + WEATHER_ENABLED, b) || !b) {
		if (m_enabled) {
			Reset();
		}
		return;
	} else {
		m_enabled = true;
	}

	if (m_time - m_lastUpdate >= 1.0f) {

		int changed = 0;
		ITimeOfDay* pTOD = gEnv->p3DEngine->GetTimeOfDay();
		if (pTOD) {
			int count = pTOD->GetVariableCount();
			for (int i = 0; i < count; i++) {
				string value;
				auto existing = m_activeValues.find(WEATHER_ENV_NAMESPACE + i);
				if (pSSS->GetGlobalValue(WEATHER_ENV_NAMESPACE + i, value) && value.length() > 0) {
					if (existing == m_activeValues.end() || existing->second != value) {
						float x, y, z;
						if (value[0] >= 'a' && value[0] <= 'z') {
							if (value[0] == 'i' && sscanf(value.c_str() + 1, "%f", &x) == 1) {
								SetWeatherVariable(i, x, true);
							} else if (value[0] == 'r' && sscanf(value.c_str() + 1, "%f", &x) == 1) {
								SetWeatherVariable(i, x, false);
							} else if (value[0] == 'v' && sscanf(value.c_str() + 1, "%f,%f,%f", &x, &y, &z) == 3) {
								SetWeatherVariable(i, x, y, z, false);
							}
						} else if(sscanf(value.c_str(), "%f", &x) == 1){
							SetWeatherVariable(i, x, true);
						}
						m_activeValues[WEATHER_ENV_NAMESPACE + i] = value;
						changed++;
					}
				} else {
					RestoreWeatherVariable(i);
					if (existing != m_activeValues.end()) {
						m_activeValues.erase(existing);
						changed++;
					}
				}
			}
			if (changed > 0) {
				pTOD->Update();
			}
			for (int i = WEATHER_ENABLED + 1; i < WEATHER_VAR_COUNT; i++) {
				float f3[3];
				string value;
				auto existing = m_activeValues.find(WEATHER_NAMESPACE + i);
				pSSS->GetGlobalValue(WEATHER_NAMESPACE + i, value);
				if (existing == m_activeValues.end() || existing->second != value) {
					switch (i) {
					case WEATHER_WIND:
						if (value.length() > 0 && value[0] == 'v' && sscanf(value.c_str() + 1, "%f,%f,%f", f3 + 0, f3 + 1, f3 + 2) == 3) {
							if (!m_originalWind) m_originalWind = gEnv->p3DEngine->GetWind(AABB(), false);
							gEnv->p3DEngine->SetWind(Vec3(f3[0], f3[1], f3[2]));
						} else if (value.length() > 0 && sscanf(value.c_str(), "%f", f3 + 0) == 1) {
							if (!m_originalWind) m_originalWind = gEnv->p3DEngine->GetWind(AABB(), false);
							gEnv->p3DEngine->SetWind((*m_originalWind) * f3[0]);
						} else {
							if (m_originalWind) {
								gEnv->p3DEngine->SetWind(*m_originalWind);
								m_originalWind.reset();
							}
						}
						break;
					case WEATHER_FX:
						SyncEffects(value.c_str());
						break;
					case WEATHER_LAYERS:
						SyncLayers(value.c_str());
						break;
					}
					if (existing != m_activeValues.end() && value.length() == 0) {
						m_activeValues.erase(existing);
					}
				}
			}
		}
		m_lastUpdate = m_time;
	}
}

bool CWeatherSystem::SetWeatherVariable(int variableId, float value, bool interpolate, bool update) {
	ITimeOfDay* pTOD = gEnv->p3DEngine->GetTimeOfDay();
	if (pTOD) {
		ITimeOfDay::SVariableInfo info;
		if (pTOD->GetVariableInfo(variableId, info)) {
			auto existing = m_originalWeatherValues.find(variableId);
			float fMin = info.fValue[1];
			float fMax = info.fValue[2];
			if (existing == m_originalWeatherValues.end()) {
				memcpy(m_originalWeatherValues[variableId], info.fValue, sizeof(info.fValue));
			}
			else {
				fMin = existing->second[1];
				fMax = existing->second[2];
			}
			if (interpolate) {
				value = fMax * value + fMin * (1.0 - value);
			}
			info.fValue[1] = value;
			info.fValue[2] = value;
			pTOD->SetVariableValue(variableId, info.fValue);
			if (update) {
				pTOD->Update();
			}
			return true;
		}
	}
	return false;
}

bool CWeatherSystem::SetWeatherVariable(int variableId, float x, float y, float z, bool update) {
	ITimeOfDay* pTOD = gEnv->p3DEngine->GetTimeOfDay();
	if (pTOD) {
		ITimeOfDay::SVariableInfo info;
		if (pTOD->GetVariableInfo(variableId, info)) {
			auto existing = m_originalWeatherValues.find(variableId);
			if (existing == m_originalWeatherValues.end()) {
				memcpy(m_originalWeatherValues[variableId], info.fValue, sizeof(info.fValue));
			}
			info.fValue[0] = x;
			info.fValue[1] = y;
			info.fValue[2] = z;
			pTOD->SetVariableValue(variableId, info.fValue);
			if (update) {
				pTOD->Update();
			}
			return true;
		}
	}
	return false;
}

std::optional<WeatherVariable> CWeatherSystem::GetWeatherVariable(int variableId) {
	ITimeOfDay* pTOD = gEnv->p3DEngine->GetTimeOfDay();
	if (pTOD) {
		ITimeOfDay::SVariableInfo info;
		if (pTOD->GetVariableInfo(variableId, info)) {
			return WeatherVariable{
				.value = info.fValue[0],
				.min = info.fValue[1],
				.max = info.fValue[2]
			};
		}
	}
	return {};
}

std::optional<WeatherVariable> CWeatherSystem::GetWeatherVariableRange(int variableId) {
	ITimeOfDay* pTOD = gEnv->p3DEngine->GetTimeOfDay();
	if (pTOD) {
		ITimeOfDay::SVariableInfo info;
		if (pTOD->GetVariableInfo(variableId, info)) {
			auto existing = m_originalWeatherValues.find(variableId);
			if (existing != m_originalWeatherValues.end()) {
				return WeatherVariable{
					.value = -1.0,
					.min = existing->second[1],
					.max = existing->second[2]
				};
			}
			return WeatherVariable{
				.value = -1.0,
				.min = info.fValue[1],
				.max = info.fValue[2]
			};
		}
	}
	return {};
}

bool CWeatherSystem::RestoreWeatherVariable(int variableId, bool update) {
	ITimeOfDay* pTOD = gEnv->p3DEngine->GetTimeOfDay();
	if (pTOD) {
		ITimeOfDay::SVariableInfo info;
		if (pTOD->GetVariableInfo(variableId, info)) {
			auto existing = m_originalWeatherValues.find(variableId);
			if (existing != m_originalWeatherValues.end()) {
				memcpy(info.fValue, m_originalWeatherValues[variableId], sizeof(info.fValue));
				m_originalWeatherValues.erase(existing);
			}
			pTOD->SetVariableValue(variableId, info.fValue);
			if (update) {
				pTOD->Update();
			}
			return true;
		}
	}
	return false;
}

bool CWeatherSystem::RestoreWeather(bool update) {
	ITimeOfDay* pTOD = gEnv->p3DEngine->GetTimeOfDay();
	if (pTOD) {
		ITimeOfDay::SVariableInfo info;
		for (auto& [variableId, value] : m_originalWeatherValues) {
			if (pTOD->GetVariableInfo(variableId, info)) {
				memcpy(info.fValue, value, sizeof(info.fValue));
				pTOD->SetVariableValue(variableId, info.fValue);
			}
		}
		m_originalWeatherValues.clear();
		if (update) {
			pTOD->Update();
		}
	}
	return false;
}

bool CWeatherSystem::UpdateWeather() {
	ITimeOfDay* pTOD = gEnv->p3DEngine->GetTimeOfDay();
	if (pTOD) {
		pTOD->Update();
		return true;
	}
	else {
		return false;
	}
}


void CWeatherSystem::SyncEffects(const char* effects) {
	std::stringstream ss;
	std::string item;
	std::set<std::string> effects_now;

	std::set<std::string> to_remove;
	std::set<std::string> to_add;

	ss << effects;

	while ((ss >> item) && item.length() > 0) {
		effects_now.insert(item);
	}

	for (auto& k : effects_now) {
		if (!m_activeEffects.contains(k)) to_add.insert(k);
	}

	for (auto& k : m_activeEffects) {
		if (!effects_now.contains(k)) to_remove.insert(k);
	}

	for (auto& k : to_add) {
		if (k[0] == '-') {
			RemoveEffect(k.c_str() + 1);
		}
		else {
			SpawnEffect(k.c_str());
		}
	}

	for (auto& k : to_remove) {
		if (k[0] == '-') {
			SpawnEffect(k.c_str() + 1);
		}
		else {
			RemoveEffect(k.c_str());
		}
	}

	m_activeEffects = effects_now;
}

void CWeatherSystem::SpawnEffect(const char *eff) {
	std::string effect_name{ eff };
	SEffectInfo info = GetEffectInfo(eff);

	IParticleEffect* pEffect = gEnv->p3DEngine->FindParticleEffect(info.effect.c_str(), "", "Particle.SpawnEffect");
	if (pEffect) {
		auto emitters = m_activeEmitters.find(effect_name);
		if (emitters != m_activeEmitters.end()) {
			for (auto emitter : emitters->second) {
				emitter->Activate(true);
			}
		}
		else {
			pEffect->SetEnabled(true);
			Vec3& pos = info.pos;
			pos.z = gEnv->p3DEngine->GetTerrainElevation(pos.x, pos.y);
			for (unsigned i = 0; i < info.instances; i++) {
				if (i > 0) {
					pos.x += (((float)rand()) / RAND_MAX) * 4.0f - 2.0f;
					pos.y += (((float)rand()) / RAND_MAX) * 4.0f - 2.0f;
				}
				IParticleEmitter* pEmitter = pEffect->Spawn(true, IParticleEffect::ParticleLoc(pos, info.dir, info.scale));
				if (pEmitter) {
					m_activeEmitters[effect_name].push_back(pEmitter);
				}
			}
		}
	}
}

void CWeatherSystem::RemoveEffect(const char *eff) {
	std::string effect_name{ eff };
	SEffectInfo info = GetEffectInfo(eff);

	IParticleEffect* pEffect = gEnv->p3DEngine->FindParticleEffect(info.effect.c_str(), "", "Particle.SpawnEffect");
	if (pEffect) {
		auto emitters = m_activeEmitters.find(effect_name);
		if (emitters != m_activeEmitters.end()) {
			for (auto emitter : emitters->second) {
				emitter->Activate(false);
			}
			m_activeEmitters.erase(emitters);
		}
		else {
			pEffect->SetEnabled(false);
		}
	}
}

void CWeatherSystem::SyncLayers(const char* layers) {
	std::stringstream ss;
	std::string layer;
	ss << layers;

	std::optional<unsigned> mask;

	while ((ss >> layer) && layer.length() > 0) {
		if (layer == "frozen") {
			if (!mask) mask.emplace(static_cast<unsigned>(MTL_LAYER_FROZEN));
			else mask.emplace(*mask | static_cast<unsigned>(MTL_LAYER_FROZEN));
		}
		else if (layer == "wet") {
			if (!mask) mask.emplace(static_cast<unsigned>(MTL_LAYER_WET));
			else mask.emplace(*mask | static_cast<unsigned>(MTL_LAYER_WET));
		}
	}

	if (mask && (!m_activeMask || *m_activeMask != *mask)) {
		if (m_activeMask) RemoveLayer(*m_activeMask);
		ApplyLayer(*mask);
		m_activeMask = std::move(mask);
	} else if(!mask && m_activeMask) {
		RemoveLayer(*m_activeMask);
		m_activeMask.reset();
	}
}

void CWeatherSystem::ApplyLayer(unsigned layer) {
	for (auto type : static_entities) {
		uint32_t count = gEnv->p3DEngine->GetObjectsByType(type, NULL);
		if (count == 0) continue;
		std::vector<IRenderNode*> nodes;
		nodes.resize(count);
		count = gEnv->p3DEngine->GetObjectsByType(type, nodes.data());
		for (auto node : nodes) {
			node->SetMaterialLayers(node->GetMaterialLayers() | layer);
		}
	}

	for (int i = 0; i < 65536; i++) {
		IStatInstGroup group;
		if (gEnv->p3DEngine->GetStatInstGroup(i, group)) {
			if (!group.szFileName || strlen(group.szFileName) == 0 || !group.pStatObj) continue;

			std::string_view sv_name{ group.pStatObj->GetFilePath() };
			auto original = m_originalGroups.find(i);
			if (original == m_originalGroups.end()) {
				m_originalGroups[i] = group;
			}
			group.nMaterialLayers = group.nMaterialLayers | layer;
			if (layer & MTL_LAYER_FROZEN) {
				// nicegrass and bigpatch need special treatment, so check for those
				// and if so, replace the underlying cgf model with a new model
				auto new_name = GetFrozenGrassName(sv_name);
				if (new_name != sv_name) {
					group.nMaterialLayers = group.nMaterialLayers & (~MTL_LAYER_FROZEN);
					strncpy(group.szFileName, new_name.c_str(), sizeof(group.szFileName) - 1);
					IStatObj* pClone = group.pStatObj->Clone();
					pClone->SetFilePath(group.szFileName);
					pClone->Refresh(FRO_GEOMETRY);
					group.pStatObj = pClone;
					group.pMaterial = NULL;
				} else {
					group.fBending = 0.0f;
				}
			}
			gEnv->p3DEngine->SetStatInstGroup(i, group);
		}
	}
}

void CWeatherSystem::RemoveLayer(unsigned layer) {
	for (auto type : static_entities) {
		uint32_t count = gEnv->p3DEngine->GetObjectsByType(type, NULL);
		if (count == 0) continue;
		std::vector<IRenderNode*> nodes;
		nodes.resize(count);
		count = gEnv->p3DEngine->GetObjectsByType(type, nodes.data());
		for (auto node : nodes) {
			node->SetMaterialLayers(node->GetMaterialLayers() & (~layer));
		}
	}

	for (int i = 0; i < 65536; i++) {
		IStatInstGroup group;
		if (gEnv->p3DEngine->GetStatInstGroup(i, group)) {
			if (!group.szFileName || strlen(group.szFileName) == 0 || !group.pStatObj) continue;

			std::string_view sv_name{ group.pStatObj->GetFilePath() };
			auto pStatBefore = group.pStatObj;
			auto original = m_originalGroups.find(i);
			if (original != m_originalGroups.end()) {
				group = original->second;
				m_originalGroups.erase(original);
			} else {
				// this shouldn't ever happen, but just in case it occurs
				// handle it by replacing original grasses back
				group.nMaterialLayers = group.nMaterialLayers & (~layer);
				if (layer & MTL_LAYER_FROZEN) {
					auto new_name = GetNormalGrassName(sv_name);
					if (new_name != sv_name) {
						strncpy(group.szFileName, new_name.c_str(), sizeof(group.szFileName) - 1);
						IStatObj* pClone = group.pStatObj->Clone();
						pClone->SetFilePath(group.szFileName);
						pClone->Refresh(FRO_GEOMETRY);
						group.pStatObj = pClone;
					}
				}
			}

			if (pStatBefore != group.pStatObj) {
				// if stat object changed (nicegrass handling), force bUseTerrainColor
				group.bUseTerrainColor = true;
			}

			gEnv->p3DEngine->SetStatInstGroup(i, group);

		}
	}
}

bool CWeatherSystem::IsWet() const {
	return m_activeMask && ((*m_activeMask & MTL_LAYER_WET) == MTL_LAYER_WET);
}

bool CWeatherSystem::IsFrozen() const {
	return m_activeMask && ((*m_activeMask & MTL_LAYER_FROZEN) == MTL_LAYER_FROZEN);
}


//------------------------------------------------------------------------
// Hook functions
//------------------------------------------------------------------------
void CWeatherSystem::TODUpdate(bool interpolate, bool force) {
	// ! use self in the entiriety of this function
	// as this here refers to the ITimeOfDay instance !
	CWeatherSystem* self = g_pGame->GetWeatherSystem();

	// call the original update
	ITimeOfDay* pTOD = reinterpret_cast<ITimeOfDay*>(this);
	(pTOD->*pTODUpdate)(interpolate, force);

	// weather system by default is only able to force values for
	// float values, not for color splines, therefore do some
	// extra handling here for at least the sky & fog color
	//
	// assume that values specified by the server/user are multipliers
	// as otherwise setting these would be really cumbersome given that
	// i.e. sky color is orange in morning, but blue during the day
	if (interpolate) {
		ITimeOfDay::SVariableInfo info;
		float x, y, z;

		// 2103 = Sun color
		auto it = self->m_activeValues.find(WEATHER_ENV_NAMESPACE + 3);
		if (it != self->m_activeValues.end() && it->second.length() > 0 && it->second[0] == 'v' && sscanf(it->second.c_str() + 1, "%f,%f,%f", &x, &y, &z) == 3) {
			gEnv->p3DEngine->SetSunColor(
				gEnv->p3DEngine->GetSunColor().CompMul(Vec3(x, y, z))
			);
		}

		// 2106 = Sky color
		it = self->m_activeValues.find(WEATHER_ENV_NAMESPACE + 6);
		if (it != self->m_activeValues.end() && it->second.length() > 0 && it->second[0] == 'v' && sscanf(it->second.c_str() + 1, "%f,%f,%f", &x, &y, &z) == 3) {
			gEnv->p3DEngine->SetSkyColor(
				gEnv->p3DEngine->GetSkyColor().CompMul(Vec3(x, y, z))
			);
		}

		// 2108 = Fog color
		it = self->m_activeValues.find(WEATHER_ENV_NAMESPACE + 8);
		if (it != self->m_activeValues.end() && it->second.length() > 0 && it->second[0] == 'v' && sscanf(it->second.c_str() + 1, "%f,%f,%f", &x, &y, &z) == 3) {
			gEnv->p3DEngine->SetFogColor(
				gEnv->p3DEngine->GetFogColor().CompMul(Vec3(x, y, z))
			);
		}
	}
}

//------------------------------------------------------------------------
// Utility functions
//------------------------------------------------------------------------

std::string CWeatherSystem::GetFrozenGrassName(std::string_view path) {
	auto pos = path.rfind("/nicegrass");
	if (pos != std::string::npos) {
		std::string name;
		pos += 1;
		name += path.substr(0, pos);
		name += "frozen_";
		name += path.substr(pos);
		return name;
	}
	pos = path.rfind("/bigpatch");
	if (pos != std::string::npos) {
		pos = path.rfind("._frozen.cgf");
		if (pos != std::string::npos) {
			return std::string{ path };
		}
		pos = path.rfind(".cgf");
		if (pos != std::string::npos) {
			std::string name;
			name += path.substr(0, pos);
			name += "_frozen.cgf";
			return name;
		}
	}
	return std::string{ path };
}

std::string CWeatherSystem::GetNormalGrassName(std::string_view path) {
	auto pos = path.rfind("/frozen_nicegrass");
	if (pos != std::string::npos) {
		std::string name;
		pos += 1;
		name += path.substr(0, pos);
		name += path.substr(pos + 7);
		return name;
	}
	pos = path.rfind("/bigpatch");
	if (pos != std::string::npos) {
		pos = path.rfind("_frozen.cgf");
		if (pos != std::string::npos) {
			std::string name;
			name += path.substr(0, pos);
			name += ".cgf";
			return name;
		}
	}
	return std::string{ path };
}

CWeatherSystem::SEffectInfo CWeatherSystem::GetEffectInfo(std::string_view effect) {
	/*
		Parse effect info from the following format "effect_name;key1=value1;key2=value2;key3=value3..."
		Valid keys are:
			x=123.45 (scale)
			i=123 (instances)
			p=123.45,123.45,123.45 (position)
			d=123.45,123.45,123.45 (direction)
	*/
	auto pos = effect.find(";");
	float x, y, z;
	unsigned u = 1;
	if (pos == std::string::npos) {
		return SEffectInfo{
			.effect = std::string{ effect }
		};
	}
	else {
		auto effect_name = effect.substr(0, pos);
		auto tail = effect.substr(pos + 1);
		SEffectInfo info{
			.effect = std::string{ effect_name }
		};
		do {
			pos = tail.find(";");
			auto kv{ tail };
			if (pos != std::string::npos) {
				kv = kv.substr(0, pos);
				tail = tail.substr(pos + 1);
			}
			auto eq = kv.find("=");
			if (eq == std::string::npos) break;
			auto key = kv.substr(0, eq);
			std::string value{ kv.substr(eq + 1) };

			if (key == "x" && sscanf(value.c_str(), "%f", &x) == 1) {
				info.scale = x;
			}
			else if (key == "p" && sscanf(value.c_str(), "%f,%f,%f", &x, &y, &z) == 3) {
				info.pos = Vec3(x, y, z);
			}
			else if (key == "d" && sscanf(value.c_str(), "%f,%f,%f", &x, &y, &z) == 3) {
				info.dir = Vec3(x, y, z);
			}
			else if (key == "i" && sscanf(value.c_str(), "%u", &u) == 1) {
				info.instances = u;
			}
		} while (pos != std::string::npos);
		return info;
	}
}