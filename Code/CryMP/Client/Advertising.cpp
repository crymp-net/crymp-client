#include "Advertising.h"

#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CrySystem/IConsole.h"
#include "CryCommon/CryRenderer/IRenderer.h"
#include "CryCommon/CryRenderer/IRenderAuxGeom.h"

#include "CryCommon/CryAction/IActorSystem.h"
#include "CryCommon/CryAction/IVehicleSystem.h"
#include "CryCommon/CryAction/IMovementController.h"
#include "CryCommon/CrySystem/ICryPak.h"

#include "CrySystem/RandomGenerator.h"

#include "CryGame/Actors/Actor.h"
#include "CryGame/Game.h"
#include "CryGame/GameCVars.h"
#include "CryGame/Actors/Player/Player.h"
#include "CryGame/GameRules.h"

#include "Client.h"
#include "FileCache.h"
#include "CryMP/Common/ScriptBind_CPPAPI.h"

#include "Library/StringTools.h"
#include "nlohmann/json.hpp"

#include <set>
#include <string>

#define AD_CYCLE_MAIN 0
#define AD_CYCLE_VIEW_CHECK 1
#define AD_CYCLE_ERROR 2

#define ADVERTISING_SERVER_TOKEN 3000

#ifndef ADS_ENABLED
#define ADS_ENABLED true
#endif

#ifndef ADS_SERVER
#define ADS_SERVER "https://crymp.org/adserve/api/v1/public/ads"
#endif

struct SAdCue {
	Vec3 pos;
	Vec3 entityDir;
	bool visible;
};

const std::vector<std::string> knownAdLayersOrdered = {
	//"auto_defense",
	"usmilbaseb", "usmilbase",
	"rynsville", "newfield",
	"bordercontrol", "bordercheck",
	"forward", "right" , "left",
	"sign", "research",
};

static std::set<std::string> knownAdLayers;
static std::vector<SAdCue> cues;

static Vec3 lastCameraPos;
static Vec3 lastCameraDir;

static SAdVec3 toAdVec3(Vec3 vec) {
	return SAdVec3{
		.x = vec.x,
		.y = vec.y,
		.z = vec.z
	};
}

CAdManager::CAdManager() {
	knownAdLayers.clear();
	for (auto& k : knownAdLayersOrdered) {
		knownAdLayers.insert(k);
	}
}

void CAdManager::Update(float delta) {
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_GAME);
	if (ADS_ENABLED && gEnv->bClient && (gEnv->bMultiplayer || gEnv->pSystem->IsDevMode())) {
		m_time += delta;

		if (!g_pGame->GetGameRules()) {
			return;
		}

		CycleAds(delta);
	}
}

void CAdManager::CycleAds(float delta) {
	const CCamera& camera = gEnv->pRenderer->GetCamera();
	const Vec3 cameraPos = camera.GetPosition();
	const Vec3 cameraDir = camera.GetViewdir();

	if ((cameraPos - lastCameraPos).len2() < 0.001f && (cameraDir - lastCameraDir).len2() < 0.001) {
		m_idleTime += delta;
	} else {
		m_idleTime = 0.0f;
	}

	lastCameraPos = cameraPos;
	lastCameraDir = cameraDir;


	switch (m_state) {
	case EAdState::eAS_None:
		m_state = EAdState::eAS_FetchAds;
		break;
	case EAdState::eAS_FetchAds:
		m_state = EAdState::eAS_FetchingAds;
		FetchAds();
		break;
	case EAdState::eAS_FetchedAds:
		m_state = EAdState::eAS_FetchAdsPak;
		break;
	case EAdState::eAS_FetchAdsPak:
		m_state = EAdState::eAS_FetchingAdsPak;
		FetchAdsPak();
		break;
	case EAdState::eAS_FetchedAdsPak:
		m_state = EAdState::eAS_Display;
		break;
	case EAdState::eAS_Display:
		m_cycles[AD_CYCLE_MAIN] = m_time;
		m_state = EAdState::eAS_Displaying;
		DisplayAds();
		break;
	case EAdState::eAS_Displaying:
		if (m_time - m_cycles[AD_CYCLE_VIEW_CHECK] > 1.0f) {
			m_cycles[AD_CYCLE_VIEW_CHECK] = m_time;
			CollectViewership(delta);
		}
		if (m_time - m_cycles[AD_CYCLE_MAIN] > 30.0f) {
			m_state = EAdState::eAS_None;
		}
		break;
	case EAdState::eAS_Error:
	case EAdState::eAS_DisplayError:
		if (m_time - m_cycles[AD_CYCLE_ERROR] > 15.0f) {
			m_state = EAdState::eAS_None;
		}
		break;
	}
}

void CAdManager::DisplayAds() {
	auto count = gEnv->p3DEngine->GetObjectsByType(EERType::eERType_Brush, NULL);
	std::vector<IRenderNode*> nodes;
	nodes.resize(count);
	gEnv->p3DEngine->GetObjectsByType(EERType::eERType_Brush, nodes.data());

	if (m_upcomingAds.size() > 0) {
		m_ads = std::move(m_upcomingAds);
	}

	if (m_upcomingPakPath != m_pakPath) {
		if (!m_pakPath.empty()) {
			if (gEnv->pCryPak->ClosePack(m_pakPath.c_str())) {
				m_pakPath.clear();
			} else {
				CryLogAlways("$4Failed to unload previous ad pak: %s", m_pakPath.c_str());
				m_cycles[AD_CYCLE_ERROR] = m_time;
				m_state = EAdState::eAS_DisplayError;
				return;
			}
		}
		if (!m_upcomingPakPath.empty()) {
			if (gEnv->pCryPak->OpenPack("game\\", m_upcomingPakPath.c_str())) {
				m_pakPath = std::move(m_upcomingPakPath);
			} else {
				CryLogAlways("$4Failed to load ad pak: %s", m_upcomingPakPath.c_str());
				m_cycles[AD_CYCLE_ERROR] = m_time;
				m_state = EAdState::eAS_DisplayError;
				return;
			}
		}
	}

	if (m_ads.size() == 0) {
		return;
	}

	for (auto& group : m_ads) {
		for (auto& version : group.versions) {
			version.nodes.clear();
		}
	}

	std::vector<size_t> shuffle(m_ads.size());

	for (size_t i = 0; i < m_ads.size(); i++) {
		shuffle[i] = i;
	}

	for (size_t i = 0; i < m_ads.size(); i++) {
		std::swap(
			shuffle[RandomGenerator::GenerateUInt32(0, m_ads.size() - 1)], 
			shuffle[RandomGenerator::GenerateUInt32(0, m_ads.size() - 1)]
		);
	}

	std::map<std::string, SAdVersion*> nodeToAdMappings;

	for (int r = 0; r < 2; r++) {
		if (r == 0 && m_collectedMappings) continue;
		else if(r == 0) m_collectedMappings = true;
		for (auto node : nodes) {
			if (!node->GetName() || !node->GetMaterial()) continue;
			std::string name = StringTools::ToLower(std::string{ node->GetName() });
			if (name.starts_with("objects/library/props/signs/")) {
				// find mapping or generate it if not known yet
				auto mappingIt = FindMaterialMapping(node, name);

				// skip if we are in mapping collection phase
				if (mappingIt == m_mappings.end() || (r == 0)) {
					continue;
				}

				IMaterial* pOM = node->GetMaterial();
				SAdMaterialMapping& mapping = mappingIt->second;

				if (mapping.id < pOM->GetSubMtlCount()) {
					int attempts = 0;
					IMaterial* pSM = pOM->GetSubMtl(mapping.id);
					IMaterial* pAdMaterial = NULL;
					IMaterial* pVersionMaterial = NULL;
					SAdGroup* ad = NULL;
					SAdVersion* adVersion = NULL;
					if (pSM) {
						std::string nodeGroupId = std::to_string((uintptr_t)pOM) + "/" + std::to_string(mappingIt->second.id);
						auto discoveredIt = nodeToAdMappings.find(nodeGroupId);

						if (discoveredIt != nodeToAdMappings.end()) {
							discoveredIt->second->nodes.insert(SAdNodeInformation{
								.node = node,
								.materialId = mapping.id,
								.parent = false
							});
							continue;
						} else {
							do {
								float closest = 1000.0f;
								ad = &m_ads[shuffle[(m_epoch++) % m_ads.size()]];
								for (auto& version : ad->versions) {
									float similarity = abs(version.aspectRatio - mapping.aspectRatio);

									if (similarity < closest) {
										pVersionMaterial = FindMaterial(*ad, version);
										if (pVersionMaterial) {
											adVersion = &version;
											pAdMaterial = pVersionMaterial;
											closest = similarity;
										}
									}

									attempts++;
								}
							} while (!pAdMaterial && attempts < 20);

							if (pAdMaterial) {
								adVersion->nodes.insert(SAdNodeInformation{
									.node = node,
									.materialId = mapping.id,
									.parent = true
								});
								nodeToAdMappings[nodeGroupId] = adVersion;
								pOM->SetSubMtl(mapping.id, pAdMaterial);
							}
						}
					}
				}
			}
		}
	}
}

std::map<std::string, SAdMaterialMapping>::iterator CAdManager::FindMaterialMapping(IRenderNode* node, const std::string& name) {
	auto mappingIt = m_mappings.find(name);
	if (mappingIt == m_mappings.end()) {
		std::optional<float> aspectRatio;
		if (
			(name.starts_with("objects/library/props/signs/sign_") && !name.starts_with("objects/library/props/signs/sign_mp"))
			|| name.starts_with("objects/library/props/signs/de_zone")
			|| name.starts_with("objects/library/props/signs/gas_station")
			) {
			aspectRatio = 2.0f;
		}
		else if (name.starts_with("objects/library/props/signs/auto_defense")) {
			aspectRatio = 4.0f;
		}
		else if (name.starts_with("objects/library/props/signs/corporate") || name.starts_with("objects/library/props/signs/advertising_sign_big_")) {
			aspectRatio = 1.0f;
		}

		if (aspectRatio) {
			IMaterial* pOM = node->GetMaterial();
			int nSub = pOM->GetSubMtlCount();
			int modified = 0;

			if (pOM->GetSubMtlCount() > 0) {
				int subId = -1;
				int lastSubId = 0;
				int foundMaterials = 0;

				for (int i = 0; i < nSub; i++) {
					int attempts = 0;
					IMaterial* pSM = pOM->GetSubMtl(i);
					if (!pSM->GetName()) continue;
					std::string materialName = StringTools::ToLower(std::string{ pSM->GetName() });
					if (materialName.starts_with("ad_")) continue;
					if (knownAdLayers.contains(materialName)) {
						foundMaterials++;
						lastSubId = i;
						if (
							(name.find(materialName) != std::string::npos)
							//|| (materialName == "left" && name.find("west") != std::string::npos)
							//|| (materialName == "right" && name.find("east") != std::string::npos)
							//|| (materialName == "forward" && name.find("north") != std::string::npos)
						){
							subId = i;
						}
					} else {
						//CryLogAlways("Unknown layer %s of %s", materialName.c_str(), name.c_str());
					}
				}

				if (subId == -1 && foundMaterials > 0) {
					subId = lastSubId;
				}

				if (subId >= 0) {
					mappingIt = m_mappings.emplace(std::make_pair(name, SAdMaterialMapping{
						.aspectRatio = *aspectRatio,
						.id = subId
					})).first;
				}
			}
		}
	}
	return mappingIt;
}

IMaterial* CAdManager::FindMaterial(const SAdGroup& group, const SAdVersion& version) {
	std::string name{ "ad_" + group.id + "_" + version.id };
	IMaterialManager* pMatMan(gEnv->p3DEngine->GetMaterialManager());
	IMaterial* pNM(pMatMan->FindMaterial(name.c_str()));
	if (!pNM) {
		IMaterial* pMatSrc(pMatMan->LoadMaterial("Materials/Presets/MaterialTypes/clean/mat_wood", false, true));
		if (pMatSrc)
		{
			IMaterial* pMatDst(pMatMan->CreateMaterial(name.c_str(), pMatSrc->GetFlags() | MTL_FLAG_NON_REMOVABLE));
			if (pMatDst)
			{
				SShaderItem& si(pMatSrc->GetShaderItem());

				SInputShaderResources isr(si.m_pShaderResources);
				isr.m_Textures[EFTT_DIFFUSE].m_Name = version.path.c_str();
				isr.m_GlowAmount = 0.4f;

				SShaderItem siDst(gEnv->pRenderer->EF_LoadShaderItem(si.m_pShader->GetName(), true, EF_FORCE_RELOAD, &isr, si.m_pShader->GetGenerationMask()));
				pMatDst->AssignShaderItem(siDst);
				pNM = pMatDst;
			}
		}
	}
	return pNM;
}

void CAdManager::FetchAds() {
	auto collected = std::move(m_collectedRecords);
	std::string collectedSerialized;
	try {
		std::string serverToken;

		CSynchedStorage* pSSS = g_pGame->GetSynchedStorage();
		if (pSSS) {
			string csServerToken;
			if (pSSS->GetGlobalValue(ADVERTISING_SERVER_TOKEN, csServerToken)) {
				serverToken.assign(csServerToken.c_str());
			}
		}

		auto arr = nlohmann::json::array();
		for (auto& seen : collected) {
			arr.emplace_back(nlohmann::json{
				{"epoch", seen.epoch},
				{"groupId", seen.groupId},
				{"versionId", seen.versionId},
				{"viewId", seen.viewId},
				{"serverToken", seen.serverToken},
				{"map", seen.map},
				{"cameraPos", nlohmann::json{ {"x", seen.cameraPos.x}, {"y", seen.cameraPos.y}, {"z", seen.cameraPos.z}}},
				{"cameraDir", nlohmann::json{ {"x", seen.cameraDir.x}, {"y", seen.cameraDir.y}, {"z", seen.cameraDir.z}}},
				{"pos", nlohmann::json{ {"x", seen.pos.x}, {"y", seen.pos.y}, {"z", seen.pos.z}}},
			});
		}
		auto req = nlohmann::json{
			{"records", std::move(arr)},
			{"epoch", time(NULL)},
			{"serverToken", serverToken},
		};
		if (auto profile = gClient->GetScriptBind_CPPAPI()->GetProfile("real")) {
			req["profileId"] = profile->id;
			req["profileToken"] = profile->token;
		}
		collectedSerialized = req.dump();
	} catch (nlohmann::json::exception& ex) {
		// fallback
		CryLogAlways("$4Failed to serialize ads: %s", ex.what());
		collectedSerialized = "{\"records\":[],\"epoch\":" + std::to_string(time(NULL)) + "}";
	}

	gClient->HttpRequest(HTTPClientRequest {
		.method = "POST",
		.url = ADS_SERVER,
		.data = collectedSerialized,
		.headers = {{"Content-type", "application/json"}},
		.callback = [this](const HTTPClientResult& result) -> void {
			if (result.code == 200) {
				try {
					m_upcomingAds.clear();
					auto ads{ nlohmann::json::parse(result.response) };
					auto& groups = ads["groups"];
					for (size_t i = 0; i < groups.size(); i++) {
						std::vector<SAdVersion> versions;
						auto& jsonVersions = groups[i]["versions"];
						for (size_t j = 0; j < jsonVersions.size(); j++) {
							versions.emplace_back(SAdVersion{
								.id = jsonVersions[j]["id"].get<std::string>(),
								.path = jsonVersions[j]["path"].get<std::string>(),
								.aspectRatio = jsonVersions[j]["aspectRatio"].get<float>(),
							});
						}
						m_upcomingAds.emplace_back(SAdGroup{
							.id = groups[i]["id"].get<std::string>(),
							.viewId = groups[i]["viewId"].get<std::string>(),
							.versions = std::move(versions)
						});
					}
					m_upcomingPakUrl = ads["resources"].get<std::string>();
					m_state = EAdState::eAS_FetchedAds;
				} catch (nlohmann::json::exception& ex) {
				  CryLogAlways("$4Failed to parse ads response: %s", ex.what());
				  m_cycles[AD_CYCLE_ERROR] = m_time;
				  m_state = EAdState::eAS_Error;
				}
			} else {
				 CryLogWarning("$4Failed to retrieve ads: %d", result.code);
				 m_cycles[AD_CYCLE_ERROR] = m_time;
				 m_state = EAdState::eAS_Error;
			}
		}
	});
}

void CAdManager::FetchAdsPak() {
	gClient->GetFileCache()->Request(FileCacheRequest{
		.fileURL = m_upcomingPakUrl,
		.fileType = "pak",
		.onProgress = [](const std::string& status) -> bool {
			return true;
		},
		.onComplete = [this](FileCacheResult& result) -> void {
			if (result.success) {
				m_upcomingPakPath = result.filePath.string();
				m_state = EAdState::eAS_FetchedAdsPak;
			} else {
				CryLogAlways("$4Failed to fetch ads pak: %s", result.filePath.c_str());
				m_cycles[AD_CYCLE_ERROR] = m_time;
				m_state = EAdState::eAS_Error;
			}
		}
	});
}

void CAdManager::CollectViewership(float delta) {
#ifdef ADS_DEBUGGING
	for (auto& cue : cues) {
		IRenderer* pRenderer = gEnv->pRenderer;
		IRenderAuxGeom* pAuxGeom = pRenderer->GetIRenderAuxGeom();
		pAuxGeom->SetRenderFlags(e_Def3DPublicRenderflags);
		pAuxGeom->DrawCone(
			cue.pos,
			cue.entityDir,
			0.2f, 1.0f,
			cue.visible ? Vec3(0.0f, 255.0f, 0.0f) : Vec3(255.0f, 0.0f, 0.0f)
		);
	}
	cues.clear();
#endif

	// do not collect ads statistics if the player is idle (most likely afk)
	// as that would be a waste of ad spend
	if (m_idleTime > 5.0f) {
		return;
	}

	int nSkipEnts = 0;
	CPlayer* pPlayer = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetClientActor());
	IPhysicalEntity* pSkipEnts[10];
	Vec3 origin = gEnv->pRenderer->GetCamera().GetPosition();
	Vec3 cameraDir = gEnv->pRenderer->GetCamera().GetViewdir();
	ray_hit rayhit;


	size_t localEpoch = static_cast<size_t>(time(NULL));
	std::string serverToken;
	std::string mapName;

	if (IGameFramework* pFW = g_pGame->GetIGameFramework()) {
		if (ILevelSystem* pLS = pFW->GetILevelSystem()) {
			if (ILevel* pLevel = pLS->GetCurrentLevel()) {
				if (ILevelInfo* pInfo = pLevel->GetLevelInfo()) {
					mapName = pInfo->GetName();
				}
			}
		}
	}

	CSynchedStorage* pSSS = g_pGame->GetSynchedStorage();
	if (pSSS) {
		string csServerToken;
		if (pSSS->GetGlobalValue(ADVERTISING_SERVER_TOKEN, csServerToken)) {
			serverToken.assign(csServerToken.c_str());
		}
	}

	if (pPlayer) {
		IEntity* pActorEntity = pPlayer->GetEntity();

		if (pActorEntity) {
			pSkipEnts[nSkipEnts] = pActorEntity->GetPhysics();
			if (pSkipEnts[nSkipEnts]) nSkipEnts++;

			// if player is on vehicle, exclude the vehicle from raycast as well
			if (pPlayer->GetLinkedVehicle()) {
				IEntity* pVehEntity = pPlayer->GetLinkedVehicle()->GetEntity();
				if (pVehEntity) {
					pSkipEnts[nSkipEnts] = pVehEntity->GetPhysics();
					if (pSkipEnts[nSkipEnts]) nSkipEnts++;
				}
			}
		}
	}

	for (auto& group : m_ads) {
		for (auto& version : group.versions) {
			for (auto& info : version.nodes) {
				Vec3 pos = info.node->GetPos();
				Vec3 screenPos;
				Vec3 entityDir;
				float dot = 0.0f;

				// reject any further computation is distance is over 65m
				if ((pos - origin).len() > 65.0f) {
					continue;
				}

				// find out real position and direction
				if (IPhysicalEntity* pPE = info.node->GetPhysics()) {
					pe_status_pos p;
					if (pPE->GetStatus(&p)) {
						pos = p.pos;
						pos.z += 0.66f * p.BBox[1].z;

						auto q = p.q *Quat::CreateRotationXYZ(Ang3(0.0f, 90.0f, 0.0f));
						entityDir = q * Vec3(0.0f, 0.0f, 1.0f);
						entityDir.z = 0.0f;
						entityDir.normalize();
					} else {
						continue;
					}
				} else {
					continue;
				}

				// compute entity location on player's screen plane
				gEnv->pRenderer->ProjectToScreen(pos.x, pos.y, pos.z, &screenPos.x, &screenPos.y, &screenPos.z);

				// ignore if the entity is not visible on player's screen
				if (screenPos.z > 1.0f || screenPos.y < 0.0f || screenPos.x < 0.0f || screenPos.x > 100.0f || screenPos.y > 100.0f) {
					continue;
				}

				// compute direction vector towards entity and a cosine of angle between entity rotation and this direction vector
				Vec3 dir = pos - origin;
				dot = entityDir * dir.normalized();

				// to prevent the following condition from failing on two-sided bigboards, make sure the value is negative
				if (dot > 0.1 && strstr(info.node->GetName(), "sign_big_b.cgf")) {
					dot = -dot;
				}

				// check whether entity is headed towards player or not, if not (player sees backside of panel), skip
				// counting ad time on this entity
				if (dot > -0.1f) {
					continue;
				}

#ifdef ADS_DEBUGGING
				SAdCue cue{
					.pos = pos,
					.entityDir = entityDir,
					.visible = false
				};
#endif

				int nHits = gEnv->pPhysicalWorld->RayWorldIntersection(origin, dir * 2.0f, ent_all, rwi_stop_at_pierceable | rwi_colltype_any, &rayhit, 1, pSkipEnts, nSkipEnts);
				if (nHits >= 1 && rayhit.pCollider->GetForeignData(PHYS_FOREIGN_ID_STATIC) == info.node) {
					m_collectedRecords.push_back(SAdCollectedRecord{
						.epoch = localEpoch,
						.groupId = group.id,
						.versionId = version.id,
						.viewId = group.viewId,
						.serverToken = serverToken,
						.map = mapName,
						.cameraPos = toAdVec3(origin),
						.cameraDir = toAdVec3(cameraDir),
						.pos = toAdVec3(pos)
					});
				}

#ifdef ADS_DEBUGGING
				cues.emplace_back(std::move(cue));
#endif
			}
		}
	}
}

void CAdManager::Reset(bool quit) {
	m_ads.clear();
	m_mappings.clear();
	m_collectedMappings = false;
	for (auto& cycle : m_cycles) {
		cycle = -1000.0f;
	}
	m_state = EAdState::eAS_None;
}