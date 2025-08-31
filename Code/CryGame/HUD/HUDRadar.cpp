/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: HUD radar

-------------------------------------------------------------------------
History:
- 2006/2007: Jan Müller

*************************************************************************/
#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CryAction/IUIDraw.h"
#include "CryCommon/CryEntitySystem/EntityId.h"
#include "CryCommon/CryAction/IVehicleSystem.h"
#include "CryCommon/CryAISystem/IAIGroup.h"
#include "CryCommon/CryAction/IWorldQuery.h"

#include "HUD.h"
#include "HUDRadar.h"
#include "HUDPowerStruggle.h"
#include "HUDScopes.h"
#include "HUDSilhouettes.h"

#include "GameFlashAnimation.h"
#include "GameFlashLogic.h"

#include "CryGame/Game.h"
#include "CryGame/Actors/Actor.h"
#include "CryGame/Items/Weapons/Weapon.h"
#include "CryGame/GameRules.h"
#include "CryGame/GameCVars.h"
#include "CryGame/Items/Weapons/FireModes/Scan.h"
#include "CryGame/Items/Weapons/GunTurret.h"

#define RANDOM() ((((float)cry_rand()/(float)RAND_MAX)*2.0f)-1.0f)

const static float fRadarSizeOverTwo = 47.0f;
const static float fEntitySize = 4.0f;
const static float fEntityMaxDistance = fRadarSizeOverTwo - fEntitySize;
const static float fRadarDefaultRadius = 75.0f;

//-----------------------------------------------------------------------------------------------------

CHUDRadar::CHUDRadar(CHUD *pHUD)
{
	m_pHUD = pHUD;
	m_pGameRules = m_pHUD->m_pGameRules;
	m_pClientActor = m_pHUD->m_pClientActor;
	m_pActorSystem = gEnv->pGame->GetIGameFramework()->GetIActorSystem();
	m_pVehicleSystem = gEnv->pGame->GetIGameFramework()->GetIVehicleSystem();

	m_fX = 93.75f; // RadarCenterX in Flash * 800 (CryEngineRefSizeX) / 1024 (FlashRefSizeX)
	m_fY = 525.0f; // RadarCenterX in Flash * 600 (CryEngineRefSizeY) / 768  (FlashRefSizeY)

	m_radarDotSizes[0] = 1.3f;
	m_radarDotSizes[1] = 2.0f;
	m_radarDotSizes[2] = 2.5f;
	m_radarDotSizes[3] = 2.0f;	//this is currently used as a multiplier for the hunter

	m_fTime = 0.0f;

	m_taggedPointer = 0;
	ResetTaggedEntities();

	m_soundIdCounter = unsigned(1 << 26);

	m_fLastViewRot = m_fLastCompassRot = m_fLastStealthValue = m_fLastStealthValueStatic = m_fLastFov = -9999.99f;
	m_lookAtObjectID = 0;
	m_lookAtTimer = 0.0f;
	m_scannerObjectID = 0;
	m_scannerTimer = 0.0f;
	m_scannerGatherTimer = 0.50f;
	m_startBroadScanTime = .0f;
	m_lastScan = 0.0f;
	m_jammingValue = 0.0f;
	m_jammerID = 0;
	m_jammerDisconnectMap = false;
	m_mapId = 0;
	m_fPDAZoomFactor = 1.0;
	m_fPDATempZoomFactor = 1.0;
	m_vPDAMapTranslation = Vec2(0, 0);
	m_vPDATempMapTranslation = Vec2(0, 0);
	m_bDragMap = false;
	m_fLastRadarRatio = -1.0f;

	for (int i = 0; i < NUM_MAP_TEXTURES; ++i)
	{
		m_miniMapStartX[i] = m_miniMapStartY[i] = 0;
		m_miniMapEndX[i] = m_miniMapEndY[i] = 2048;
		m_mapRadarRadius[i] = -1;
	}

	m_startOnScreenX = m_startOnScreenY = m_endOnScreenX = m_endOnScreenY = 0;
	m_mapDivX = m_mapDivY = 1.0f;
	m_mapDimX = m_mapDimY = 1;

	m_renderMiniMap = false;

	m_bsRadius = 0.0f;
	m_bsUseParameter = m_bsKeepEntries = false;
	m_bsPosition = Vec3(0, 0, 0);

	m_flashPDA = NULL;
	m_flashRadar = NULL;
	m_iMultiplayerEnemyNear = 0;

	m_pLevelData = NULL;

	m_coordinateToString[0] = "A%d";
	m_coordinateToString[1] = "B%d";
	m_coordinateToString[2] = "C%d";
	m_coordinateToString[3] = "D%d";
	m_coordinateToString[4] = "E%d";
	m_coordinateToString[5] = "F%d";
	m_coordinateToString[6] = "G%d";
	m_coordinateToString[7] = "H%d";

	IEntityClassRegistry* pEntityClassRegistry = gEnv->pEntitySystem->GetClassRegistry();

	//save some classes for comparison
	m_pVTOL = pEntityClassRegistry->FindClass("US_vtol");
	m_pHeli = pEntityClassRegistry->FindClass("Asian_helicopter");
	m_pHunter = pEntityClassRegistry->FindClass("Hunter");
	m_pWarrior = pEntityClassRegistry->FindClass("Alien_warrior");
	m_pAlien = pEntityClassRegistry->FindClass("Alien");
	m_pTrooper = pEntityClassRegistry->FindClass("Trooper");
	m_pPlayerClass = pEntityClassRegistry->FindClass("Player");
	m_pGrunt = pEntityClassRegistry->FindClass("Grunt");
	m_pScout = pEntityClassRegistry->FindClass("Scout");
	m_pTankUS = pEntityClassRegistry->FindClass("US_tank");
	m_pTankA = pEntityClassRegistry->FindClass("Asian_tank");
	m_pLTVUS = pEntityClassRegistry->FindClass("US_ltv");
	m_pLTVA = pEntityClassRegistry->FindClass("Asian_ltv");
	m_pAAA = pEntityClassRegistry->FindClass("Asian_aaa");
	m_pTruck = pEntityClassRegistry->FindClass("Asian_truck");
	m_pAPCUS = pEntityClassRegistry->FindClass("US_apc");
	m_pAPCA = pEntityClassRegistry->FindClass("Asian_apc");
	m_pBoatCiv = pEntityClassRegistry->FindClass("Civ_speedboat");
	m_pHover = pEntityClassRegistry->FindClass("US_hovercraft");
	m_pBoatUS = pEntityClassRegistry->FindClass("US_smallboat");
	m_pBoatA = pEntityClassRegistry->FindClass("Asian_patrolboat");
	m_pCarCiv = pEntityClassRegistry->FindClass("Civ_car1");
	m_pParachute = pEntityClassRegistry->FindClass("Parachute");
	m_pAutoTurret = pEntityClassRegistry->FindClass("AutoTurret");
	m_pAutoTurretAA = pEntityClassRegistry->FindClass("AutoTurretAA");

	assert(m_pLTVA && m_pLTVUS && m_pTankA && m_pTankUS && m_pWarrior && m_pHunter && m_pAlien && m_pScout && m_pGrunt && m_pHeli && m_pVTOL && m_pAAA && m_pTruck && m_pAPCUS && m_pAPCA && m_pBoatCiv && m_pHover && m_pBoatUS && m_pBoatA && m_pCarCiv && m_pParachute);
}

//-----------------------------------------------------------------------------------------------------

CHUDRadar::~CHUDRadar()
{
}

//-----------------------------------------------------------------------------------------------------

void CHUDRadar::AddTaggedEntity(EntityId iEntityId)
{
	m_taggedEntities[m_taggedPointer] = iEntityId;
	m_taggedPointer++;
	if (m_taggedPointer > NUM_TAGGED_ENTITIES)
		m_taggedPointer = 0;
}

void CHUDRadar::AddEntityToRadar(EntityId id)
{
	if (!IsOnRadar(id, 0, m_entitiesOnRadar.size() - 1))
	{
		AddToRadar(id);
		m_pHUD->OnEntityAddedToRadar(id);
	}
}

void CHUDRadar::AddEntityTemporarily(EntityId id, float time)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id);
	if (pEntity)
	{
		ShowEntityTemporarily(ChooseRadarIcon(pEntity), ChooseMiniMapIcon(pEntity), id, time);
	}
}

void CHUDRadar::ShowEntityTemporarily(RadarIcon radarIcon, MiniMapIcon miniMapIcon, EntityId id, float timeLimit)
{
	for (TempEntity& entity : m_tempEntitiesOnRadar)
	{
		if (entity.m_id == id)
		{
			entity.m_radarIcon = radarIcon;
			entity.m_miniMapIcon = miniMapIcon;
			entity.m_spawnTime = gEnv->pTimer->GetFrameStartTime().GetSeconds();

			if (timeLimit > entity.m_timeLimit)
			{
				entity.m_timeLimit = timeLimit;
			}

			return;
		}
	}

	m_tempEntitiesOnRadar.emplace_back(id, radarIcon, miniMapIcon, timeLimit);
}

bool CHUDRadar::IsEntityOnTempRadar(const EntityId id)
{
	for (TempEntity& entity : m_tempEntitiesOnRadar)
	{
		if (entity.m_id == id)
		{
			return true;
		}
	}

	return false;
}

void CHUDRadar::ClearExpiredTempEntities(const float now)
{
	std::erase_if(m_tempEntitiesOnRadar, [now](const TempEntity& entity) -> bool {
		const float diffTime = now - entity.m_spawnTime;
		return diffTime > entity.m_timeLimit || diffTime < 0.0f;
	});
}

void CHUDRadar::AddStoryEntity(EntityId id, MiniMapIcon type /* = MiniMapIcon::WayPoint */, const char* text /* = nullptr */)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id);
	if (pEntity)
	{
		std::vector<TempEntity>::iterator it = m_storyEntitiesOnRadar.begin();
		std::vector<TempEntity>::iterator end = m_storyEntitiesOnRadar.end();
		for (; it != end; ++it)
			if ((*it).m_id == id)
				return;

		TempEntity temp(id, RadarIcon::None, type, 0.0f, text);
		m_storyEntitiesOnRadar.push_back(temp);
	}
}

void CHUDRadar::RemoveStoryEntity(EntityId id)
{
	std::vector<TempEntity>::iterator it = m_storyEntitiesOnRadar.begin();
	std::vector<TempEntity>::iterator end = m_storyEntitiesOnRadar.end();
	for (; it != end; ++it)
	{
		if ((*it).m_id == id)
		{
			m_storyEntitiesOnRadar.erase(it);
			return;
		}
	}
}

void CHUDRadar::ShowSoundOnRadar(Vec3 pos, float intensity)
{
	intensity = std::min(8.0f, intensity); //limit the size of the sound on radar ...

	m_soundsOnRadar.push_back(RadarSound(pos, intensity, m_soundIdCounter++));

	if (m_soundIdCounter > unsigned(1 << 29))
		m_soundIdCounter = unsigned(1 << 26);
}

//-----------------------------------------------------------------------------------------------------

void CHUDRadar::RemoveFromRadar(EntityId id)
{
	{
		std::deque<RadarEntity>::iterator it = m_entitiesOnRadar.begin();
		std::deque<RadarEntity>::iterator end = m_entitiesOnRadar.end();
		for (; it != end; ++it)
		{
			if ((*it).m_id == id)
			{
				m_entitiesOnRadar.erase(it);
				return;
			}
		}
	}
	{
		for (int i = 0; i < NUM_TAGGED_ENTITIES; ++i)
		{
			if (m_taggedEntities[i] == id)
			{
				m_taggedEntities[i] = 0;
				break;
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------------

template <typename T, EFlashVariableArrayType flashType, int NUM_VALS> struct ArrayFillHelper
{
	ArrayFillHelper(IFlashPlayer* pFlashPlayer, const char* varName) : m_pFlashPlayer(pFlashPlayer), m_varName(varName), m_nIndex(0), m_nValues(0)
	{
	}

	~ArrayFillHelper()
	{
		Flush();
	}

	ILINE void push_back(const T& v)
	{
		m_values[m_nValues++] = v;
		if (m_nValues == NUM_VALS)
			Flush();
	}

	void Flush()
	{
		if (m_nValues > 0)
		{
			m_pFlashPlayer->SetVariableArray(flashType, m_varName, m_nIndex, m_values, m_nValues);
			m_nIndex += m_nValues;
			m_nValues = 0;
		}
	}

	IFlashPlayer* m_pFlashPlayer;
	const char* m_varName;
	int m_nIndex;
	int m_nValues;
	T m_values[NUM_VALS];
};


template<typename T, EFlashVariableArrayType flashType, int NUM_VALS>
int FillUpDoubleArray(ArrayFillHelper<T, flashType, NUM_VALS>* fillHelper,
	double a, double b, double c, double d,
	double e, double f, double g = 100.0f, double h = 100.0f, double i = 0.0f, double j = 0.0f, double k = 0.0f)
{
	fillHelper->push_back(a);
	fillHelper->push_back(b);
	fillHelper->push_back(c);
	fillHelper->push_back(d);
	fillHelper->push_back(e);
	fillHelper->push_back(f);
	fillHelper->push_back(g);
	fillHelper->push_back(h);
	fillHelper->push_back(i);
	fillHelper->push_back(j);
	fillHelper->push_back(k);
	return 11;
}

//-----------------------------------------------------------------------------------------------------

void CHUDRadar::Update(float fDeltaTime)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	const float now = gEnv->pTimer->GetFrameStartTime().GetSeconds();
	ClearExpiredTempEntities(now);

	if (!m_flashRadar)
		return; //we require the flash radar now

	if (m_pClientActor->GetHealth() <= 0)
	{
		if (gEnv->bMultiplayer)
		{
			//render the mini map
			if (m_renderMiniMap)
				RenderMiniMap();
		}
		return;
	}

	if (!m_flashRadar->IsAvailable("setObjectArray"))
		return;

	//double array buffer for flash transfer optimization
	ArrayFillHelper<double, FVAT_Double, NUM_ARRAY_FILL_HELPER_SIZE> entityValues(m_flashRadar->GetFlashPlayer(), "m_allValues");
	int numOfValues = 0;

	float fRadius = fRadarDefaultRadius;
	if (m_mapRadarRadius[m_mapId] > 2)
		fRadius = (float)m_mapRadarRadius[m_mapId];

	//check for a broadscan:
	//CryMP: This is never called, m_startBroadScanTime always 0
	if (m_startBroadScanTime && now - m_startBroadScanTime > 1.5f)
		StartBroadScan();

	m_fTime += fDeltaTime * 3.0f;
	IRenderer* pRenderer = gEnv->pRenderer;
	float fWidth43 = pRenderer->GetHeight() * 1.333f;
	float lowerBoundX = m_fX - fRadarSizeOverTwo;	//used for flash radar position computation

	Matrix34 playerViewMtxInverted = m_pClientActor->GetViewMatrix().GetInverted();
	Vec3 playerViewMtxTranslation = m_pClientActor->GetViewMatrix().GetTranslation();

	//CompassStealth***************************************************************
	UpdateCompassStealth(m_pClientActor, fDeltaTime);
	//~CompassStealth**************************************************************

	//binocs***********************************************************************
	UpdateBinoculars(m_pClientActor, fDeltaTime);
	//~binocs**********************************************************************

	//jammer***********************************************************************
	UpdateRadarJammer(m_pClientActor);
	//~jammer**********************************************************************

	//*********************************ACTUAL SCANNING****************************
	Vec3 vOrigin = GetISystem()->GetViewCamera().GetPosition();
	//scan proximity for entities
	if (now - m_lastScan > 0.2f) //5hz scanning
	{
		float scanningRadius = fRadius * 1.3f;
		ScanProximity(vOrigin, scanningRadius);
		m_lastScan = now;
	}

	//*********************************MAIN ENTITY UPDATE*************************
	UpdateRadarEntities(m_pClientActor, fRadius, playerViewMtxInverted, numOfValues, &entityValues);
	//****************************************************************************

	// Draw MissionObjectives or their target entities
	// we draw after normal targets, so AI guys which are targets show up as Objective
	std::map<EntityId, RadarObjective>::const_iterator it = m_missionObjectives.begin();
	std::map<EntityId, RadarObjective>::const_iterator end = m_missionObjectives.end();
	for (; it != end; ++it)
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(it->first);
		if (pEntity && !pEntity->IsHidden())
		{
			Vec3 vTransformed = pEntity->GetWorldPos();
			vTransformed = playerViewMtxInverted * vTransformed;
			vTransformed.z = 0;

			float scaledX = (vTransformed.x / fRadius) * fEntityMaxDistance;
			float scaledY = (vTransformed.y / fRadius) * fEntityMaxDistance;

			vTransformed.Set(scaledX, scaledY, 0.0f);

			FlashRadarFaction faction = ENeutral;
			float fAngle = 0.0f;
			float distSq = scaledX * scaledX + scaledY * scaledY;

			float fX = m_fX + vTransformed.x;
			float fY = m_fY - vTransformed.y;
			float fAlpha = 0.75f;

			Vec2 intersection;
			if (!RadarBounds_Inside(Vec2(fX, fY), intersection))
			{
				fX = intersection.x;
				fY = intersection.y;
				faction = EFriend;
			}

			if (faction == EFriend) //dirty angle computation for arrow
			{
				Vec2 dir(m_fX - fX, m_fY - fY);
				dir.Normalize();

				fAngle = acos_tpl(Vec2(0.0f, 1.0f).Dot(dir));
				if (m_fX > fX)
					fAngle = 2 * gf_PI - fAngle;
			}

			if (it->second.secondaryObjective)
				faction = (FlashRadarFaction)(int(faction) + 2);	//yellow version

			//in flash
			float lowerBoundY = m_fY - fRadarSizeOverTwo;
			float dimX = (m_fX + fRadarSizeOverTwo) - lowerBoundX;
			float dimY = (m_fY + fRadarSizeOverTwo) - lowerBoundY;
			numOfValues += ::FillUpDoubleArray(&entityValues, 
				pEntity->GetId(), 
				static_cast<int>(RadarIcon::Five), //5
				(fX - lowerBoundX) / dimX, 
				(fY - lowerBoundY) / dimY, 
				180 + RAD2DEG(fAngle), 
				faction, 
				75.0f, 
				fAlpha * 100.0f
			);
		}
	}

	//temp units on radar
	for (const TempEntity& temp : m_tempEntitiesOnRadar)
	{

		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(temp.m_id);
		if (pEntity && !pEntity->IsHidden())
		{
			if (IActor* tempActor = m_pActorSystem->GetActor(pEntity->GetId()))
			{
				if (tempActor->GetHealth() <= 0)
					continue;
			}

			Vec3 vTransformed = pEntity->GetWorldPos();
			vTransformed = playerViewMtxInverted * vTransformed;
			vTransformed.z = 0;

			float scaledX = (vTransformed.x / fRadius) * fEntityMaxDistance;
			float scaledY = (vTransformed.y / fRadius) * fEntityMaxDistance;
			vTransformed.Set(scaledX, scaledY, 0.0f);

			float distSq = scaledX * scaledX + scaledY * scaledY;
			float fX = m_fX + vTransformed.x;
			float fY = m_fY - vTransformed.y;

			Vec2 intersection;
			if (!RadarBounds_Inside(Vec2(fX, fY), intersection))
			{
				fX = intersection.x;
				fY = intersection.y;
			}

			// Determine correct rotation (turret barrel or entity base)
			CGunTurret *pTurret = nullptr;
			bool turretAggressive = false;
			Ang3 turretAngles;

			if (IItem* pItem = g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(pEntity->GetId()))
			{
				IEntityClass* pClass = pEntity->GetClass();
				if (pClass == CItem::sAutoTurret || pClass == CItem::sAutoTurretAA)
				{
					const int barrelSlot = CItem::eIBA_Secondary; 
					if (pEntity->GetSlotFlags(barrelSlot) & ENTITY_SLOT_RENDER)
					{
						//CryMP: Update barrel rotation in radar
						const Matrix34& slotTM = pEntity->GetSlotWorldTM(barrelSlot);
						turretAngles = Ang3::GetAnglesXYZ(Matrix33(slotTM));
						pTurret = static_cast<CGunTurret*>(pItem);

						if (pTurret->IsFiring(false) || pTurret->IsFiring(true) || pTurret->IsWarningSoundPlayedRecently())
						{
							turretAggressive = true;
						}
					}
				}
			}

			Ang3 entityAngles = pTurret ? turretAngles : pEntity->GetWorldAngles();
			float fAngle = m_pClientActor->GetAngles().z - entityAngles.z;
			float fAlpha = 0.85f;

			float sizeScale = GetRadarSize(pEntity, m_pClientActor);

			// in flash
			float lowerBoundY = m_fY - fRadarSizeOverTwo;
			float dimX = (m_fX + fRadarSizeOverTwo) - lowerBoundX;
			float dimY = (m_fY + fRadarSizeOverTwo) - lowerBoundY;

			int playerTeam = m_pGameRules->GetTeam(m_pClientActor->GetEntityId());
			FlashRadarFaction friendOrFoe = FriendOrFoe(gEnv->bMultiplayer, playerTeam, pEntity, m_pGameRules);

			//CryMP: Different colors according to turret state
			//Blue: friendy, Red: enemy, Yellow: aggressive
			if (pTurret)
			{
				friendOrFoe = pTurret->IsHostileTowardsClient() ? EFriend : ENeutral; //EFriend is red color in flash

				if (turretAggressive)
				{
					friendOrFoe = EEnemy; //yellow color in flash
				}
			}

			// CryMP: Brighter red color for Enemy
			if (friendOrFoe == EEnemy)
			{
				fAlpha = 1.0f;
			}

			numOfValues += ::FillUpDoubleArray(
				&entityValues,
				temp.m_id,
				static_cast<int>(temp.m_radarIcon),
				(fX - lowerBoundX) / dimX,
				(fY - lowerBoundY) / dimY,
				180.0f + RAD2DEG(fAngle),
				friendOrFoe,
				sizeScale * 25.0f,
				fAlpha * 100.0f
			);
		}
	}

	//tac bullet marked entities
	for (int i = 0; i < NUM_TAGGED_ENTITIES; ++i)
	{
		EntityId id = m_taggedEntities[i];
		if (id)
		{
			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id);

			if (pEntity)
			{
				Vec3 vTransformed = pEntity->GetWorldPos();
				vTransformed = playerViewMtxInverted * vTransformed;
				vTransformed.z = 0;

				if (vTransformed.len() > fEntityMaxDistance)
				{
					vTransformed.SetLength(fEntityMaxDistance);
				}

				float fX = m_fX + vTransformed.x;
				float fY = m_fY - vTransformed.y;
				float sizeScale = GetRadarSize(pEntity, m_pClientActor);

				//in flash
				float lowerBoundY = m_fY - fRadarSizeOverTwo;
				float dimX = (m_fX + fRadarSizeOverTwo) - lowerBoundX;
				float dimY = (m_fY + fRadarSizeOverTwo) - lowerBoundY;
				numOfValues += ::FillUpDoubleArray(&entityValues, 
					pEntity->GetId(), 
					static_cast<int>(RadarIcon::Seven), //7
					(fX - lowerBoundX) / dimX, 
					(fY - lowerBoundY) / dimY, 
					0.0f, 
					0, 
					sizeScale * 100.0f, 
					100.0f
				);
			}
		}
	}

	//sounds on radar
	for (int i = 0; i < m_soundsOnRadar.size(); ++i)
	{
		RadarSound temp = m_soundsOnRadar[i];
		float diffTime = now - temp.m_spawnTime;
		if (diffTime > 2.0f || diffTime < 0.0f) //show only 2 seconds (and catch errors)
		{
			m_soundsOnRadar.erase(m_soundsOnRadar.begin() + i);
			--i;
			continue;
		}
		else
		{
			Vec3 pos = temp.m_pos;
			pos = playerViewMtxInverted * pos;

			float intensity = temp.m_intensity;
			if (intensity > 4.0f)
			{
				if (!temp.m_hasChild && diffTime > (0.05 + 0.1f * RANDOM()))
				{
					m_soundsOnRadar[i].m_hasChild = true;
					ShowSoundOnRadar(temp.m_pos, temp.m_intensity * (0.5f + 0.2f * RANDOM()));	//aftermatch
				}

				intensity = 4.0f;
			}

			float scaledX = (pos.x / fRadius) * fEntityMaxDistance;
			float scaledY = (pos.y / fRadius) * fEntityMaxDistance;
			float distSq = (scaledX * scaledX + scaledY * scaledY) * 0.70f;
			float size = intensity * (4.0f + diffTime * 4.0f);
			if (size > fEntityMaxDistance)
				continue;
			if (distSq > (fEntityMaxDistance * fEntityMaxDistance * 1.5f))
				continue;
			float fX = m_fX + scaledX;
			float fY = m_fY - scaledY;
			//in flash
			float lowerBoundY = m_fY - fRadarSizeOverTwo;
			float dimX = (m_fX + fRadarSizeOverTwo) - lowerBoundX;
			float dimY = (m_fY + fRadarSizeOverTwo) - lowerBoundY;
			numOfValues += ::FillUpDoubleArray(&entityValues, 
				temp.m_id, 
				static_cast<int>(RadarIcon::Six), //6 
				(fX - lowerBoundX) / dimX, 
				(fY - lowerBoundY) / dimY, 
				0.0f, 
				0, 
				size * 10.0f, 
				(1.0f - (0.5f * diffTime)) * 100.0f
			);
		}
	}
	//render the mini map
	if (m_renderMiniMap)
		RenderMiniMap();

	entityValues.Flush();
	float playerX = 0.5f;
	float playerY = 0.5f;

	GetPosOnMap(m_pClientActor->GetEntity(), playerX, playerY, true);
	
	SFlashVarValue args[3] = { playerX, playerY, 270.0f - RAD2DEG(m_pClientActor->GetAngles().z) };

	m_flashRadar->Invoke("setObjectArray", args, 3);
	float radarRatio = (((float)(m_miniMapEndX[m_mapId] - m_miniMapStartX[m_mapId])) / fRadius) * 50.0f;
	if (radarRatio != m_fLastRadarRatio)
	{
		m_flashRadar->Invoke("setMapScale", SFlashVarValue(radarRatio / 2048.0f));
		m_fLastRadarRatio = radarRatio;
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDRadar::UpdateRadarEntities(CActor* pClientActor, float& fRadius, Matrix34& playerViewMtxInverted, int& numOfValues, ArrayFillHelper<double, FVAT_Double, NUM_ARRAY_FILL_HELPER_SIZE>* entityValues)
{
	float fCos = cosf(m_fTime);
	float lowerBoundX = m_fX - fRadarSizeOverTwo;	//used for flash radar position computation

	//****************************************************************************
	//singleplayer squadmates are now 100% design controlled, see "SetTeamMate"

	//we get the player's team mates for team-based MP
	int clientTeam = m_pGameRules->GetTeam(pClientActor->GetEntityId());
	if (gEnv->bMultiplayer)
	{
		m_teamMates.clear();
		m_pGameRules->GetTeamPlayers(clientTeam, m_teamMates);
	}
	//****************************************************************************

	//extra vehicle checks
	bool inVehicle = false;
	bool inAAA = false;
	bool bShowScan = false;
	float aaaDamage = 0.0f;
	if (IVehicle* pVehicle = pClientActor->GetLinkedVehicle())
	{
		fRadius *= 2.0f;
		inVehicle = true;

		if (pVehicle->GetEntity()->GetClass() == m_pAAA)
		{
			inAAA = true;
			if (IVehicleComponent* pAAARadar = pVehicle->GetComponent("radar"))
			{
				aaaDamage = pAAARadar->GetDamageRatio();
				m_flashRadar->Invoke("setDamage", aaaDamage);
			}
		}
	}

	//*********************************CHECKED FOUND ENTITIES*********************
	for (const EntityId id : m_entitiesInProximity)
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id);
		if (pEntity)
		{
			//reasons (not) to go on ... *******************************************************************
			if (IsEntityTagged(id))
				continue;

			//is it a corpse ?
			CActor* tempActor = static_cast<CActor*>(m_pActorSystem->GetActor(pEntity->GetId()));
			if (tempActor && !(tempActor->GetHealth() > 0))
				continue;

			//is it the player ?
			const bool bIsClient = pClientActor->GetEntityId() == id;

			//lets find out whether this entity belongs on the radar
			bool isOnRadar = false;
			bool mate = false;
			bool unknownEnemyObject = false;
			bool unknownEnemyActor = false;
			bool airPlaneInAAA = false;

			if (inAAA)	//check whether it's an unknown, flying airplane when you are in an AAA (show all airplanes > 3m over ground)
			{
				MiniMapIcon eType = ChooseMiniMapIcon(pEntity);
				if (eType == MiniMapIcon::Heli)
				{
					Vec3 pos = pEntity->GetPos();
					if (pos.z - gEnv->p3DEngine->GetTerrainZ((int)pos.x, (int)pos.y) > 5.0f)
					{
						IVehicle* pAirplane = m_pVehicleSystem->GetVehicle(pEntity->GetId());
						if (pAirplane && pAirplane->IsPlayerDriving(false) && pAirplane->GetAltitude() > 2.5f) //GetAltitude also checks game brushes
							airPlaneInAAA = isOnRadar = true;
					}
				}
			}

			//is it a team mate in multiplayer or is it a squad mate in singleplayer ?
			if (!isOnRadar && stl::find(m_teamMates, id))
				isOnRadar = mate = true;

			//has the object been scanned already?
			if (!isOnRadar && IsOnRadar(id, 0, m_entitiesOnRadar.size() - 1))
				isOnRadar = true;

			if (!isOnRadar)	//check whether it's an aggressive (non-vehicle) AI (in possible proximity),
				//which is not yet on the radar
			{
				IAIObject* pTemp = pEntity->GetAI();
				if (pTemp && AIOBJECT_VEHICLE != pTemp->GetAIType() && pTemp->IsHostile(m_pClientActor->GetEntity()->GetAI(), false))
				{
					isOnRadar = true;
					unknownEnemyObject = true;
				}
				else if (tempActor && gEnv->bMultiplayer)	//not teammate, not AI -> enemy!?
				{
					isOnRadar = true;
					unknownEnemyActor = true;
				}
			}

			if (!isOnRadar)
				continue;

			//**********************************************************************************************

			Vec3 vTransformed = pEntity->GetWorldPos();
			vTransformed = playerViewMtxInverted * vTransformed;

			//distance check********************************************************************************

			if (fabsf(vTransformed.z) > 500.0f)
				continue;
			vTransformed.z = 0;

			float sizeScale = GetRadarSize(pEntity, pClientActor);
			float scaledX = (vTransformed.x / fRadius) * fEntityMaxDistance;
			float scaledY = (vTransformed.y / fRadius) * fEntityMaxDistance;

			float fX = m_fX + scaledX;
			float fY = m_fY - scaledY;

			float distSq = scaledX * scaledX + scaledY * scaledY;

			if (distSq < fEntityMaxDistance * fEntityMaxDistance * 3.0f)
			{
				//these entities are near enough to check them against the border
				Vec2 intersection;
				if (!RadarBounds_Inside(Vec2(fX, fY), intersection))
				{
					if (airPlaneInAAA && !(aaaDamage == 1.0f || aaaDamage > RANDOM()))	// AAA can see airplanes over far distances
					{
						fX = intersection.x;
						fY = intersection.y;
					}
					else
						continue;
				}
			}
			else		//far away
				continue;

			float fAngle = pClientActor->GetAngles().z - pEntity->GetWorldAngles().z;
			float fAlpha = 0.85f;

			//faction***************************************************************************************

			//AI Object
			IAIObject* pAIObject = pEntity->GetAI();
			int friendly = mate ? EFriend : ENeutral;
			bool checkDriver = false;
			if (pAIObject)
			{
				if (AIOBJECT_VEHICLE == pAIObject->GetAIType())	//switch to vehicle texture ?
				{
					if (IVehicle* pVehicle = m_pVehicleSystem->GetVehicle(id))
					{
						if (IVehicleSeat* pSeat = pVehicle->GetSeatById(1))
						{
							if (pSeat->IsDriver())
							{
								EntityId driverId = pSeat->GetPassenger();
								IEntity* temp = gEnv->pEntitySystem->GetEntity(driverId);
								if (temp && temp->GetAI())
								{
									pAIObject = temp->GetAI(); //check the driver instead of the vehicle
									checkDriver = true;
								}
							}
						}
					}
				}

				if ((unknownEnemyObject || pAIObject->IsHostile(pClientActor->GetEntity()->GetAI(), false)) && (checkDriver || AIOBJECT_VEHICLE != pAIObject->GetAIType()))
				{
					friendly = EEnemy;

					IUnknownProxy* pUnknownProxy = pAIObject->GetProxy();
					if (pUnknownProxy)
					{
						int iAlertnessState = pUnknownProxy->GetAlertnessState();

						if (unknownEnemyObject) //check whether the object is near enough and alerted
						{
							if (iAlertnessState < 1)
								continue;
							if (distSq > 225.0f)
								continue;
							else if (distSq > 25.0f)
								fAlpha -= 0.5f - ((225.0f - distSq) * 0.0025f); //fade out with distance

							if (tempActor && g_pGameCVars->g_difficultyLevel < 4) //new rule : once aggroing the player, enemies get added permanently
								AddToRadar(id);
						}

						if (1 == iAlertnessState)
						{
							fAlpha = 0.65f + fCos * 0.35f;
							friendly = EAggressor;
						}
						else if (2 == iAlertnessState)
						{
							fAlpha = 0.65f + fCos * 0.35f;
							friendly = ESelf;
						}
					}
					else
						continue;
				}
				else if (gEnv->bMultiplayer)
				{
					if (mate)
						friendly = EFriend;
					else
						friendly = EEnemy;
				}
				else
				{
					if (checkDriver || mate)	//probably the own player
						friendly = EFriend;
					else
						friendly = ENeutral;
				}
			}
			else if (gEnv->bMultiplayer)	//treats factions in multiplayer
			{
				if (tempActor)
				{
					bool scannedEnemy = false;
					if (!mate && !unknownEnemyActor) //probably MP scanned enemy
					{
						friendly = EEnemy;
						scannedEnemy = true;
					}

					if (unknownEnemyActor || scannedEnemy)	//unknown or known enemy in MP !?
					{
						CNanoSuit* pNanoSuit = CPlayer::GetNanoSuit(tempActor);

						if (pNanoSuit && !(pNanoSuit->GetCloak()->GetState() != 0))
						{
							float length = vTransformed.GetLength();
							if (length < 20.0f)
							{
								if (length < 0.1f)
									length = 0.1f;
								const int cLenThres = (int)(100.0f / length);
								if (m_iMultiplayerEnemyNear < cLenThres)
									m_iMultiplayerEnemyNear = cLenThres;
							}
						}
					}
					if (!scannedEnemy && !mate)
						continue;
				}
				else
				{
					IVehicle* pVehicle = m_pVehicleSystem->GetVehicle(pEntity->GetId());
					if (pVehicle && !pVehicle->GetDriver())
					{
						int team = m_pGameRules->GetTeam(pEntity->GetId());
						if (team == 0)
							friendly = ENeutral;
						else if (team == clientTeam)
							friendly = EFriend;
						else
							friendly = EEnemy;
					}
					else
						continue;
				}
			}
			else
				continue;
			//**********************************************************************************************

			RadarIcon FlashType = ChooseRadarIcon(pEntity);

			float sizeMult = 25.f;
			if (mate)
			{
				CWeapon* pWeapon = tempActor ? tempActor->GetCurrentWeapon(false) : nullptr;
				if (pWeapon)
				{
					CScan* pScan = CScan::FromCWeapon(pWeapon);
					if (pScan && pScan->IsScanning())
					{
						bShowScan = true;
						FlashType = RadarIcon::Five; //CryMP: special icon
						sizeScale *= 1.5f;
						const float speed = gEnv->pTimer->GetRealFrameTime();
						fAlpha *= pScan->Pulse(0.1f, 0.7f, speed); //highlite scanning teammates
					}
				}

				if (!bShowScan)
				{
					//requested by Sten : teammates should be more transparent
					fAlpha *= 0.50f;
				}
			}

			//draw entity
			float lowerBoundY = m_fY - fRadarSizeOverTwo;
			float dimX = (m_fX + fRadarSizeOverTwo) - lowerBoundX;
			float dimY = (m_fY + fRadarSizeOverTwo) - lowerBoundY;

			if (!bIsClient || bShowScan)
			{
				numOfValues += ::FillUpDoubleArray(entityValues, 
					pEntity->GetId(),
					static_cast<int>(FlashType), 
					(fX - lowerBoundX) / dimX, 
					(fY - lowerBoundY) / dimY,
					180.0f + RAD2DEG(fAngle), 
					friendly, 
					sizeScale * sizeMult, 
					fAlpha * 100.0f
				);
			}
		}
	}

	if (!bShowScan)
	{
		CWeapon* pWeapon = m_pHUD->GetCurrentWeapon();
		if (pWeapon)
		{
			CScan* pScan = CScan::FromCWeapon(pWeapon);
			if (pScan)
			{
				bShowScan = pScan->IsScanning();
			}
		}
	}

	m_pHUD->SetRadarScanningEffect(bShowScan);
}

//-----------------------------------------------------------------------------------------------------

void CHUDRadar::UpdateCompassStealth(CActor* pActor, float fDeltaTime)
{
	IEntity* pPlayerEntity = pActor->GetEntity();

	//**********************update flash radar values
	float fStealthValue = 0;
	float fStealthValueStatic = 0;

	if (!gEnv->bMultiplayer)
	{
		SAIDetectionLevels levels;
		gEnv->pAISystem->GetDetectionLevels(0, levels);
		//			float exposure = 0, threat = 0;
		//			gEnv->pAISystem->GetExposureAndThreat(NULL, exposure, threat);

		static int test = 0;
		if (test == 0)
		{
			fStealthValue = max(levels.puppetExposure, levels.puppetThreat) * 100.0f;
			fStealthValueStatic = max(levels.vehicleExposure, levels.vehicleThreat) * 100.0f;
		}
		else if (test == 1)
		{
			fStealthValue = max(levels.vehicleThreat, levels.puppetThreat) * 100.0f;
			fStealthValueStatic = levels.puppetExposure * 100.0f;
		}

	}
	else if (g_pGameCVars->g_enableMPStealthOMeter && m_iMultiplayerEnemyNear)
	{
		fStealthValueStatic = fStealthValue = m_iMultiplayerEnemyNear * 10.0f;
		m_iMultiplayerEnemyNear = 0;
	}

	if (m_fLastStealthValue != fStealthValue)
	{
		m_flashRadar->CheckedInvoke("Root.RadarCompassStealth.StealthBar.gotoAndStop", fStealthValue);
		m_fLastStealthValue = fStealthValue;
	}
	if (m_fLastStealthValueStatic != fStealthValueStatic && !gEnv->bMultiplayer)
	{
		//m_flashRadar->CheckedInvoke("Root.RadarCompassStealth.StealthExposure.gotoAndStop", fStealthValueStatic);

		m_pHUD->SetStealthExposure(fStealthValueStatic);
		m_fLastStealthValueStatic = fStealthValueStatic;
	}

	if (m_fLastViewRot != 0.0f)
	{
		m_flashRadar->Invoke("setView", 0.0f);
		m_fLastViewRot = 0.0f;
	}
	float fCompass = pActor->GetAngles().z;

	if (m_jammingValue >= g_pGameCVars->hud_radarJammingThreshold) //spin compass
	{
		float compassDir = cosf(gEnv->pTimer->GetAsyncCurTime());
		float originalDirection = (fCompass + gf_PI) - (m_fLastCompassRot + gf_PI);
		if (originalDirection > 0.1f)
			originalDirection = 2.0f;
		else if (originalDirection < -0.1f)
			originalDirection = -2.0f;
		fCompass = m_fLastCompassRot + (compassDir * m_jammingValue * 10.0f * fDeltaTime) /*+ fDeltaTime*((compassDir > 0)?3.0f:(-3.0f))*/ + originalDirection * fDeltaTime;
		if (fCompass > gf_PI)
			fCompass = -gf_PI + (fabsf(fCompass) - fabsf(int(fCompass / gf_PI) * gf_PI));
		else if (fCompass < -gf_PI)
			fCompass = gf_PI - (fabsf(fCompass) - fabsf(int(fCompass / gf_PI) * gf_PI));
	}

#define COMPASS_EPSILON (0.01f)
	if (m_fLastCompassRot <= fCompass - COMPASS_EPSILON ||
		m_fLastCompassRot >= fCompass + COMPASS_EPSILON)
	{
		char szCompass[HUD_MAX_STRING_SIZE];
		sprintf(szCompass, "%f", fCompass * 180.0f / gf_PI - 90.0f);
		m_flashRadar->Invoke("setCompassRotation", szCompass);
		m_fLastCompassRot = fCompass;
	}

	float fFov = g_pGameCVars->cl_fov * pActor->GetActorParams()->viewFoVScale;
	if (m_fLastFov != fFov)
	{
		m_flashRadar->Invoke("setFOV", fFov * 0.5f);
		m_fLastFov = fFov;
	}

	if (gEnv->bMultiplayer)	//shows the player coordinates in MP
	{
		float fX, fY;
		fX = fY = -1;
		GetPosOnMap(pPlayerEntity, fX, fY);
		char strCoords[32];
		float value = ceil(fX * 8.0f);
		int index = min(int(fY / 0.125f), 7);
		sprintf(strCoords, m_coordinateToString[index].c_str(), (int)value);
		if (strcmp(strCoords, m_lastPlayerCoords.c_str()))
		{
			m_flashRadar->Invoke("setSector", strCoords);
			m_lastPlayerCoords = strCoords;
		}
	}
	//************************* end of flash compass
}

//-----------------------------------------------------------------------------------------------------

void CHUDRadar::UpdateRadarJammer(CActor* pActor)
{
	//update radar jammer**********************************************************
	if (m_jammerID && m_jammerRadius > 0.0f)
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_jammerID);
		if (pEntity)
		{
			Vec3 jamPos = pEntity->GetWorldPos();
			float dist = jamPos.GetDistance(pActor->GetEntity()->GetWorldPos());
			if (dist < m_jammerRadius)
			{
				m_jammingValue = 1.0f - dist / m_jammerRadius;
				m_flashRadar->Invoke("setNoiseValue", m_jammingValue * g_pGameCVars->hud_radarJammingEffectScale);

				if (m_jammingValue >= 0.25f)
				{
					if (!m_jammerDisconnectMap)
					{
						m_jammerDisconnectMap = true;
						m_flashPDA->Invoke("setDisconnect", m_jammerDisconnectMap);
					}
				}
				else if (m_jammerDisconnectMap)
				{
					m_jammerDisconnectMap = false;
					m_flashPDA->Invoke("setDisconnect", m_jammerDisconnectMap);
				}

				if (m_jammingValue >= g_pGameCVars->hud_radarJammingThreshold)
				{
					//update to remove entities
					m_flashRadar->Invoke("setObjectArray");
					if (m_renderMiniMap)
					{
						m_flashPDA->Invoke("Root.PDAArea.Map_M.MapArea.setObjectArray");
					}
					return;
				}
			}
			else if (m_jammingValue)
			{
				m_jammingValue = 0.0f;
				m_flashRadar->Invoke("setNoiseValue", 0.0f);
				if (m_jammerDisconnectMap)
				{
					m_jammerDisconnectMap = false;
					m_flashPDA->Invoke("setDisconnect", m_jammerDisconnectMap);
				}
			}
		}
	}
	else if (m_jammerDisconnectMap)
	{
		m_jammerDisconnectMap = false;
		m_flashPDA->Invoke("setDisconnect", m_jammerDisconnectMap);
	}
}

//-----------------------------------------------------------------------------------------------------

float CHUDRadar::GetRadarSize(IEntity* entity, CActor* actor)
{
	float posEnt = entity->GetWorldPos().z;
	float posPlayer = actor->GetEntity()->GetWorldPos().z;
	float returnValue = 1.0f;

	if ((posEnt + 15) < posPlayer)
		returnValue = m_radarDotSizes[0];
	else if ((posEnt - 15) > posPlayer)
		returnValue = m_radarDotSizes[2];
	else
		returnValue = m_radarDotSizes[0] + ((posEnt - (posPlayer - 15)) / 30.0f) * (m_radarDotSizes[2] - m_radarDotSizes[0]);

	if (entity->GetClass() == m_pHunter || entity->GetClass() == m_pWarrior)
		returnValue *= m_radarDotSizes[3];

	return returnValue;
}

bool CHUDRadar::IsOnRadar(EntityId id, int first, int last)
{
	int size = last - first;
	if (size > 1)
	{
		int mid = first + (size / 2);
		EntityId midId = m_entitiesOnRadar[mid].m_id;
		if (midId == id)
			return true;
		else if (midId > id)
			return IsOnRadar(id, first, mid - 1);
		else
			return IsOnRadar(id, mid + 1, last);
	}
	else if (size == 1)
		return (m_entitiesOnRadar[first].m_id == id || m_entitiesOnRadar[last].m_id == id);
	else if (m_entitiesOnRadar.size() > 0)
		return (m_entitiesOnRadar[first].m_id == id);
	return false;
}

void CHUDRadar::AddToRadar(EntityId id)
{
	if (m_entitiesOnRadar.size() > 0)
	{
		if (m_entitiesOnRadar[0].m_id > id)
			m_entitiesOnRadar.push_front(RadarEntity(id));
		else if (m_entitiesOnRadar[m_entitiesOnRadar.size() - 1].m_id < id)
			m_entitiesOnRadar.push_back(RadarEntity(id));
		else
		{
			std::deque<RadarEntity>::iterator it, itTemp;
			for (it = m_entitiesOnRadar.begin(); it != m_entitiesOnRadar.end(); ++it)
			{
				itTemp = it;
				itTemp++;
				if (itTemp != m_entitiesOnRadar.end())
				{
					if (it->m_id < id && itTemp->m_id > id)
					{
						m_entitiesOnRadar.insert(itTemp, 1, RadarEntity(id));
						break;
					}
				}
				else { assert(false); }
			}
		}
	}
	else
		m_entitiesOnRadar.push_back(RadarEntity(id));
}

bool CHUDRadar::ScanObject(EntityId id)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id);
	if (!pEntity)
		return false;

	IPhysicalEntity* pPE = pEntity->GetPhysics();
	if (!pPE)
		return false;

	if (IActor* pActor = m_pActorSystem->GetActor(id))
	{
		if (CActor* pCActor = static_cast<CActor*>(pActor))
		{
			if (CNanoSuit* pNanoSuit = CPlayer::GetNanoSuit(pCActor))
			{
				if (pNanoSuit->GetMode() == NANOMODE_CLOAK && pNanoSuit->GetCloak()->GetType() == CLOAKMODE_REFRACTION)
					return false;
			}
		}
	}

	if (gEnv->bMultiplayer && !CheckObjectMultiplayer(id))
	{
		return false;
	}

	if (CheckObject(pEntity, id != m_lookAtObjectID, id != m_lookAtObjectID) &&
		!IsOnRadar(id, 0, m_entitiesOnRadar.size() - 1))
	{
		m_pHUD->AutoAimNoText(id);

		m_scannerObjectID = id;
		m_scannerTimer = g_pGameCVars->hud_binocsScanningDelay;//0.55f;

		return true;
	}

	return false;
}

void CHUDRadar::UpdateScanner(float frameTime)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	if (m_scannerTimer > 0.0f)
	{
		m_scannerTimer -= frameTime;
		if (m_scannerTimer <= 0.0f)
		{
			m_scannerTimer = 0.0f;

			m_pHUD->AutoAimUnlock(0);

			if (m_scannerObjectID)
			{
				bool isTurret = false;
				if (CItem* pItem = static_cast<CItem*>(gEnv->pGame->GetIGameFramework()->GetIItemSystem()->GetItem(m_scannerObjectID)))
				{
					isTurret = (pItem->GetEntity()->GetClass() == CItem::sAutoTurret || pItem->GetEntity()->GetClass() == CItem::sAutoTurretAA);
				}
				if (isTurret || m_pActorSystem->GetActor(m_scannerObjectID) ||
					m_pVehicleSystem->GetVehicle(m_scannerObjectID))
				{
					if (!gEnv->bMultiplayer || CheckObjectMultiplayer(m_scannerObjectID))
					{
						EntityId clientId = m_pClientActor->GetEntityId();
						m_pGameRules->ClientSimpleHit(SimpleHitInfo(clientId, m_scannerObjectID, 0, 0));
						m_pHUD->PlaySound(ESound_BinocularsLock);
					}
				}
			}

			m_scannerObjectID = 0;
		}

		if (m_scannerTimer < 0.8f) //make them blink
		{
			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_scannerObjectID);
			if (pEntity)
			{
				float r = 0.8f;
				float g = 0.8f;
				float b = 1.0f;
				//CryMP: Custom colors for tagging objects with binoculars
				//Neutral: White
				//Team: Blue
				//Enemy: Orange-Red
				if (gEnv->bMultiplayer)
				{
					if (m_pGameRules->IsHostile(m_scannerObjectID, m_pClientActor->GetEntityId()) 
						&& (!m_pGameRules->IsNeutral(m_scannerObjectID) || m_pGameRules->GetTeamCount() < 2))
					{
						//ColorF V(0.8f, 0.498039f, 0.196078f);
						r = 1.0f;
						g = 0.1f;
						b = 0.0f;
					}
					else if (m_pGameRules->IsSameTeam(m_scannerObjectID, m_pClientActor->GetEntityId()))
					{
						r = 0.137255f;
						g = 0.137255f;
						b = 0.556863f;
					}
				}
				m_pHUD->GetSilhouettes()->SetSilhouette(pEntity, r, g, b, 0.5f, -1.0f);
			}
		}
	}

	if (m_scannerTimer <= 0.0f && !m_scannerQueue.empty())
	{
		EntityId scanId = 0;
		do
		{
			scanId = m_scannerQueue.front();
			m_scannerQueue.pop_front();

		} while (!ScanObject(scanId) && !m_scannerQueue.empty());
	}

	if (m_scannerGatherTimer > 0.0f)
	{
		m_scannerGatherTimer -= frameTime;
		if (m_scannerGatherTimer <= 0.0f)
		{
			m_scannerGatherTimer = 0.0f;
			//GatherScannableObjects(); // design @GC 2007 : only scan lookat

			m_scannerGatherTimer = 0.50f;
		}
	}
}

void CHUDRadar::Reset()
{
	ResetScanner();

	//remove scanned / tac'd entities
	m_entitiesOnRadar.clear();
	ResetTaggedEntities();
	m_tempEntitiesOnRadar.clear();
	m_storyEntitiesOnRadar.clear();
}

void CHUDRadar::ResetScanner()
{
	m_scannerQueue.clear();

	if (m_scannerTimer > 0.0f)
	{
		m_pHUD->AutoAimUnlock(0);
		m_scannerTimer = 0.0f;
	}
}

bool CHUDRadar::IsObjectInQueue(EntityId id)
{
	if (std::find(m_scannerQueue.begin(), m_scannerQueue.end(), id) != m_scannerQueue.end())
		return true;
	return false;
}

bool CHUDRadar::IsNextObject(EntityId id)
{
	if (m_scannerQueue.empty())
		return false;

	return m_scannerQueue.front() == id;
}


bool CHUDRadar::CheckObject(IEntity* pEntity, bool checkVelocity, bool checkVisibility)
{
	IVehicle* pVehicle = m_pVehicleSystem->GetVehicle(pEntity->GetId());
	if (pVehicle && pVehicle->IsDestroyed())
		return false;

	CCamera& rCamera = GetISystem()->GetViewCamera();
	uint32 uiFlags = pEntity->GetFlags();
	IPhysicalEntity* pPE = pEntity->GetPhysics();
	float fSquaredDistance = pEntity->GetWorldPos().GetSquaredDistance(rCamera.GetPosition());
	bool bPointVisible = rCamera.IsPointVisible(pEntity->GetWorldPos());

	if ((uiFlags & ENTITY_FLAG_ON_RADAR) && pPE && (!checkVisibility || (checkVisibility && fSquaredDistance < 1000000.0f && bPointVisible)))
	{
		pe_status_dynamics dyn;
		pPE->GetStatus(&dyn);

		if (checkVelocity)
		{
			if (dyn.v.len2() < 0.5f * 0.5f)
				return false;
		}

		if (checkVisibility)
		{
			IPhysicalEntity* pSkipEnt = m_pClientActor->GetEntity()->GetPhysics();

			ray_hit hit;
			Vec3 dir = (dyn.centerOfMass - rCamera.GetPosition()) * 1.15f;

			if (gEnv->pPhysicalWorld->RayWorldIntersection(rCamera.GetPosition(), dir, ent_all, (13 & rwi_pierceability_mask), &hit, 1, &pSkipEnt, pSkipEnt ? 1 : 0))
			{
				if (!hit.bTerrain && hit.pCollider != pPE)
				{
					IEntity* pHitEntity = gEnv->pEntitySystem->GetEntityFromPhysics(hit.pCollider);
					if (pHitEntity != pEntity)
						return false;
				}
			}
		}

		return true;
	}

	return false;
}

Vec2i CHUDRadar::GetMapGridPosition(IEntity* pEntity)
{
	if (pEntity)
	{
		Vec3 pos = pEntity->GetWorldPos();
		return GetMapGridPosition(pos.x, pos.y);
	}

	return Vec2i(-1, -1);
}

Vec2i CHUDRadar::GetMapGridPosition(float x, float y)
{
	Vec2i retVal(-1, -1);

	float outX = 0.0f, outY = 0.0f;

	GetPosOnMap(x, y, outX, outY);

	retVal.y = (int)cry_ceilf(outX * 8.0f);
	retVal.x = (int)cry_ceilf(outY * 8.0f);

	return retVal;
}

void CHUDRadar::GatherScannableObjects()
{
	/*IEntityItPtr pIt=gEnv->pEntitySystem->GetEntityIterator();

	while (!pIt->IsEnd())
	{
		if (IEntity *pEntity=pIt->Next())
		{
			EntityId id=pEntity->GetId();
			if (CheckObject(pEntity, true, false) && !IsOnRadar(id, 0, m_entitiesOnRadar.size()-1) && !IsObjectInQueue(id))
				m_scannerQueue.push_back(id);
		}
	}*/

	int count = m_pActorSystem->GetActorCount();
	if (count > 0)
	{
		IActorIteratorPtr pIter = m_pActorSystem->CreateActorIterator();
		while (IActor* pActor = pIter->Next())
		{
			if (IEntity* pEntity = pActor->GetEntity())
			{
				EntityId id = pEntity->GetId();
				if (CheckObject(pEntity, true, false) && !IsOnRadar(id, 0, m_entitiesOnRadar.size() - 1) && !IsObjectInQueue(id))
					m_scannerQueue.push_back(id);
			}
		}
	}

	count = m_pVehicleSystem->GetVehicleCount();
	if (count > 0)
	{
		IVehicleIteratorPtr pIter = m_pVehicleSystem->CreateVehicleIterator();
		while (IVehicle* pVehicle = pIter->Next())
		{
			if (IEntity* pEntity = pVehicle->GetEntity())
			{
				EntityId id = pEntity->GetId();
				if (CheckObject(pEntity, true, false) && !IsOnRadar(id, 0, m_entitiesOnRadar.size() - 1) && !IsObjectInQueue(id))
					m_scannerQueue.push_back(id);
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------------
bool CHUDRadar::GetPosOnMap(float inX, float inY, float& outX, float& outY, bool flashCoordinates)
{
	float posX = std::max(0.0f, inX - std::min((float)m_miniMapStartX[m_mapId], (float)m_miniMapEndX[m_mapId]));
	float posY = std::max(0.0f, inY - std::min((float)m_miniMapStartY[m_mapId], (float)m_miniMapEndY[m_mapId]));
	float temp = posX;
	posX = posY;
	posY = temp;
	outX = std::max((float)m_startOnScreenX, (float)m_startOnScreenX + (std::min(posX * m_mapDivX, 1.0f) * (float)m_mapDimX));
	outY = std::max((float)m_startOnScreenY, (float)m_startOnScreenY + (std::min(posY * m_mapDivY, 1.0f) * (float)m_mapDimY));
	if (flashCoordinates)
	{
		outX = (outX - (float)m_startOnScreenX) / (float)m_mapDimX;
		outY = (outY - (float)m_startOnScreenY) / (float)m_mapDimY;
	}
	return true;
}

//-----------------------------------------------------------------------------------------------------
bool CHUDRadar::GetPosOnMap(IEntity* pEntity, float& outX, float& outY, bool flashCoordinates)
{
	if (!pEntity)
		return false;
	SEntitySlotInfo info;
	Vec3 pos = Vec3(0, 0, 0);
	int slotCount = pEntity->GetSlotCount();
	for (int i = 0; i < slotCount; ++i)
	{
		if (pEntity->GetSlotInfo(i, info))
		{
			if (info.pCharacter)
			{
				int16 id = info.pCharacter->GetISkeletonPose()->GetJointIDByName("objectiveicon");
				if (id >= 0)
				{
					//vPos = pCharacter->GetISkeleton()->GetHelperPos(helper);
					pos = info.pCharacter->GetISkeletonPose()->GetAbsJointByID(id).t;
					if (!pos.IsZero())
					{
						pos = pEntity->GetSlotWorldTM(i).TransformPoint(pos);
						break;
					}
				}
			}
		}
	}
	if (pos.IsZero())
		pos = pEntity->GetWorldPos();

	return GetPosOnMap(pos.x, pos.y, outX, outY, flashCoordinates);
}

//---------------------------------- -------------------------------------------------------------------
void CHUDRadar::ComputeMiniMapResolution()
{
	//size of the displayed 3d area
	const float mapSizeX = fabsf(m_miniMapEndX[m_mapId] - m_miniMapStartX[m_mapId]);
	m_mapDivX = 1.0f / mapSizeX;
	const float mapSizeY = fabsf(m_miniMapEndY[m_mapId] - m_miniMapStartY[m_mapId]);
	m_mapDivY = 1.0f / mapSizeY;
	float width = (float)gEnv->pRenderer->GetWidth();
	float height = (float)gEnv->pRenderer->GetHeight();
	//coordinates of the map in screen pixels in 4:3 at a reference resolution of 800*600
	m_startOnScreenX = 358; m_startOnScreenY = 50; m_endOnScreenX = 756; m_endOnScreenY = 446;
	//adopt x-coordinates to current resolution
	m_startOnScreenX = (int)(800 - ((800 - m_startOnScreenX) / 600.0f * 800 / width * height));
	m_endOnScreenX = (int)(800 - ((800 - m_endOnScreenX) / 600.0f * 800 / width * height));
	//compute screen dimensions
	m_mapDimX = m_endOnScreenX - m_startOnScreenX;
	m_mapDimY = m_endOnScreenY - m_startOnScreenY;
}

//------------------------------------------------------------------------
void CHUDRadar::OnLoadingComplete(ILevel* pLevel)
{
	if (!pLevel)
	{
		if (gEnv->pSystem->IsEditor())
		{
			char* levelName;
			char* levelPath;
			g_pGame->GetIGameFramework()->GetEditorLevel(&levelName, &levelPath);		//this is buggy, returns path for both
			LoadMiniMap(levelPath);
		}
	}
	else
	{
		LoadMiniMap(pLevel->GetLevelInfo()->GetPath());
		m_pLevelData = pLevel;
	}

	RenderMiniMap();
	SetMiniMapTexture(m_mapId, true);
}

//------------------------------------------------------------------------
void CHUDRadar::ReloadMiniMap()
{
	if (m_pLevelData)
		OnLoadingComplete(m_pLevelData);
	SetMiniMapTexture(m_mapId, true);
}

//-----------------------------------------------------------------------------------------------------

void CHUDRadar::LoadMiniMap(const char* mapPath)
{
	//get the factories (and other buildings)
	if (gEnv->bMultiplayer)
	{
		IEntityClass* factoryClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("Factory");
		IEntityClass* hqClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("HQ");
		IEntityClass* alienClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("AlienEnergyPoint");

		m_buildingsOnRadar.clear();

		IEntityItPtr pIt = gEnv->pEntitySystem->GetEntityIterator();

		float minDistance = 0;

		EntityId uiOnScreenObjectiveEntityId = 0;
		while (!pIt->IsEnd())
		{
			if (IEntity* pEntity = pIt->Next())
			{
				IEntityClass* cls = pEntity->GetClass();
				if (cls == factoryClass || cls == hqClass || cls == alienClass)
				{
					m_buildingsOnRadar.push_back(RadarEntity(pEntity->GetId()));
				}
				if (cls == factoryClass && m_pHUD->GetPowerStruggleHUD() && m_pHUD->GetPowerStruggleHUD()->IsFactoryType(pEntity->GetId(), CHUDPowerStruggle::BUY_PAGE_PROTOTYPES))
				{
					Vec3 dirvec = (m_pClientActor->GetEntity()->GetPos() - pEntity->GetPos());
					float distance = dirvec.GetLength2D();
					if (!minDistance || minDistance > distance)
					{
						minDistance = distance;
						uiOnScreenObjectiveEntityId = pEntity->GetId();
					}
				}
			}
		}

		EntityId uiOldOnScreenObjectiveEntityId = SAFE_HUD_FUNC_RET(GetOnScreenObjective());
		if (uiOldOnScreenObjectiveEntityId)
		{
			// If there was already an objective id, it's certainly because we unloaded the map to go into menus
			SAFE_HUD_FUNC(SetOnScreenObjective(uiOldOnScreenObjectiveEntityId));
		}
		else
		{
			// Nothing, pickup the default map objective
			SAFE_HUD_FUNC(SetOnScreenObjective(uiOnScreenObjectiveEntityId));
		}
	}

	//now load the actual map
	string fullPath(mapPath);
	int slashPos = fullPath.rfind('\\');
	if (slashPos == -1)
		slashPos = fullPath.rfind('/');
	string mapName = fullPath.substr(slashPos + 1, fullPath.length() - slashPos);
	m_currentLevel = mapName;

	fullPath.append("\\");
	fullPath.append(mapName);
	fullPath.append(".xml");
	XmlNodeRef mapInfo = GetISystem()->LoadXmlFile(fullPath.c_str());
	if (mapInfo == 0)
	{
		CryLogWarning("Did not find a level meta data file %s in %s.", fullPath.c_str(), mapName.c_str());
		return;
	}

	m_fLastRadarRatio = -1.0f;
	//retrieve the coordinates of the map
	if (mapInfo)
	{
		for (int i = 0; i < NUM_MAP_TEXTURES; ++i)
		{
			m_mapFile[i].clear();
			m_mapRadarRadius[i] = -1;
		}

		for (int n = 0; n < mapInfo->getChildCount(); ++n)
		{
			XmlNodeRef mapNode = mapInfo->getChild(n);
			const char* name = mapNode->getTag();
			if (!_stricmp(name, "MiniMap"))
			{
				int attribs = mapNode->getNumAttributes();
				const char* key;
				const char* value;
				int mapNr = 0;
				for (int i = 0; i < attribs; ++i)
				{
					mapNode->getAttributeByIndex(i, &key, &value);
					CryFixedStringT<64> keyString(key);
					size_t pos = keyString.find("Filename");
					if (pos != string::npos)
					{
						pos += 8; //add size of "FileName"
						if (pos == keyString.length())
							mapNr = 0;
						else
						{
							CryFixedStringT<64> number = keyString.substr(pos, keyString.length() - pos);
							int n = atoi(number.c_str());
							if (n >= 0 && n < NUM_MAP_TEXTURES)
								mapNr = n;
							else
							{
								CryLogWarning("Couldn't read map index correctly : %s.", keyString.c_str());
								return;
							}
						}

						m_mapFile[mapNr] = mapPath;
						m_mapFile[mapNr].append("\\");
						m_mapFile[mapNr].append(value);

						if (mapNr == 0)
						{
							m_flashPDA->Invoke("setMapBackground", m_mapFile[mapNr].c_str());
							m_flashRadar->Invoke("setMapBackground", m_mapFile[mapNr].c_str());
						}
					}
					else if (keyString.find("startX") != CryFixedStringT<64>::npos)
						m_miniMapStartX[mapNr] = atoi(value);
					else if (keyString.find("startY") != CryFixedStringT<64>::npos)
						m_miniMapStartY[mapNr] = atoi(value);
					else if (keyString.find("endX") != CryFixedStringT<64>::npos)
						m_miniMapEndX[mapNr] = atoi(value);
					else if (keyString.find("endY") != CryFixedStringT<64>::npos)
						m_miniMapEndY[mapNr] = atoi(value);
					else if (keyString.find("mapRadius") != CryFixedStringT<64>::npos)
						m_mapRadarRadius[mapNr] = atoi(value);
				}
				break;
			}
		}
	}

	//compute the dimensions of the miniMap
	ComputeMiniMapResolution();
}

//-----------------------------------------------------------------------------------------------------

void CHUDRadar::RenderMiniMap()
{
	CGameFlashAnimation* m_flashMap = m_flashPDA;
	if (!m_flashMap || !m_flashMap->IsAvailable("Root.PDAArea.Map_M.MapArea"))
		return;
	if (m_jammerDisconnectMap)
		return;
	//LoadMiniMap(m_currentLevel);

	//the local player
	CPlayer* pClientActor = m_pHUD->m_pClientActor;

	m_possibleOnScreenObjectives.resize(0);
	//double array buffer for flash transfer optimization
	std::vector<double> entityValues;
	int numOfValues = 0;
	//array of text strings
	std::map<EntityId, string> textOnMap;

	float fX = 0;
	float fY = 0;

	//draw vehicles only once
	std::map<EntityId, bool> drawnVehicles;
	std::map<EntityId, bool> drawnPlayers;

	EntityId iOnScreenObjective = m_pHUD->GetOnScreenObjective();
	if (iOnScreenObjective)
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(iOnScreenObjective);
		if (pEntity)
		{
			GetPosOnMap(pEntity, fX, fY);
			char strCoords[32];
			float value = ceil(fX * 8.0f);

			int index = min(int(fY / 0.125f), 7);
			sprintf(strCoords, m_coordinateToString[index].c_str(), (int)value);
			m_flashMap->CheckedSetVariable("Root.PDAArea.TextBottom.Colorset.ObjectiveText.text", strCoords);
		}
	}
	EntityId iCurrentSpawnPoint = m_pGameRules->GetPlayerSpawnGroup(pClientActor);
	if (iCurrentSpawnPoint)
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(iCurrentSpawnPoint);
		if (pEntity)
		{
			GetPosOnMap(pEntity, fX, fY);
			char strCoords[3];
			float value = ceil(fX * 8.0f);

			int index = min(int(fY / 0.125f), 7);
			sprintf(strCoords, m_coordinateToString[index].c_str(), (int)value);
			m_flashMap->CheckedSetVariable("Root.PDAArea.TextBottom.Colorset.DeploySpotText.text", strCoords);
		}
	}

	bool isMultiplayer = gEnv->bMultiplayer;

	int team = m_pGameRules->GetTeam(pClientActor->GetEntityId());

	//draw buildings first
	for (int i = 0; i < m_buildingsOnRadar.size(); ++i)
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_buildingsOnRadar[i].m_id);
		if (GetPosOnMap(pEntity, fX, fY))
		{

			int friendly = FriendOrFoe(isMultiplayer, team, pEntity, m_pGameRules);
			bool addBuilding = true;
			if (friendly == 2)
			{
				SmartScriptTable props;
				if (pEntity->GetScriptTable() && pEntity->GetScriptTable()->GetValue("Properties", props))
				{
					int capturable = 0;
					if (props->GetValue("bCapturable", addBuilding))
					{
						if (!addBuilding)
						{
							if (m_pGameRules->IsSameTeam(pClientActor->GetEntityId(), pEntity->GetId()))
							{
								addBuilding = true;
							}
						}
					}
				}
			}

			MiniMapIcon type = ChooseMiniMapIcon(pEntity);
			if ((type == MiniMapIcon::Headquarter || type == MiniMapIcon::Headquarter2) && m_pHUD->HasTACWeapon() && friendly == EEnemy)
			{
				type = MiniMapIcon::HitZone;
			}
			if (addBuilding == true)
			{
				//bool underAttack = m_pHUD->IsUnderAttack(pEntity) && friendly == EFriend;
				const CHUD::CaptureState state = m_pHUD->GetCaptureState(pEntity);
				int color = 0; //hidden
				if (state == CHUD::CaptureState::EnemyCapturing || state == CHUD::CaptureState::EnemyUncapturing)
				{
					color = 1; //red
				}
				else if (state == CHUD::CaptureState::TeamCapturing || state == CHUD::CaptureState::TeamUncapturing)
				{
					color = 2; //blue
				}
				
				m_possibleOnScreenObjectives.push_back(pEntity->GetId());
				numOfValues += FillUpDoubleArray(&entityValues, 
					pEntity->GetId(), 
					static_cast<int>(type), 
					fX, 
					fY, 
					270.0f - RAD2DEG(pEntity->GetWorldAngles().z), 
					friendly, 
					100,
					100, 
					iOnScreenObjective == m_buildingsOnRadar[i].m_id, 
					iCurrentSpawnPoint == m_buildingsOnRadar[i].m_id, 
					color
				);
			}
		}
	}

	if (isMultiplayer)
	{
		//now spawn points
		std::vector<EntityId> locations;
		m_pGameRules->GetSpawnGroups(locations);
		for (int i = 0; i < locations.size(); ++i)
		{
			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(locations[i]);
			if (!pEntity)
				continue;
			if (pEntity)
			{
				IVehicle* pVehicle = m_pVehicleSystem->GetVehicle(pEntity->GetId());
				bool isVehicle = (pVehicle) ? true : false;
				if (GetPosOnMap(pEntity, fX, fY))
				{
					int friendly = FriendOrFoe(isMultiplayer, team, pEntity, m_pGameRules);
					if (isVehicle /*&& !stl::find_in_map(drawnVehicles, pVehicle->GetEntityId(), false)*/)
					{
						if (friendly == EFriend)
						{
							numOfValues += FillUpDoubleArray(&entityValues,
								pEntity->GetId(),
								static_cast<int>(MiniMapIcon::SpawnTruck),
								fX,
								fY,
								270.0f - RAD2DEG(pEntity->GetWorldAngles().z),
								friendly,
								100,
								100,
								iOnScreenObjective == locations[i],
								iCurrentSpawnPoint == locations[i]
							);
							drawnVehicles[pVehicle->GetEntityId()] = true;
						}
					}
					else
					{
						//bool underAttack = m_pHUD->IsUnderAttack(pEntity) && friendly == EFriend;
						const CHUD::CaptureState state = m_pHUD->GetCaptureState(pEntity);
						int color = 0; //hidden
						if (state == CHUD::CaptureState::EnemyCapturing || state == CHUD::CaptureState::EnemyUncapturing)
						{
							color = 1; //red
						}
						else if (state == CHUD::CaptureState::TeamCapturing || state == CHUD::CaptureState::TeamUncapturing)
						{
							color = 2; //blue
						}

						if (friendly != 2)
						{
							numOfValues += FillUpDoubleArray(&entityValues,
								pEntity->GetId(),
								static_cast<int>(MiniMapIcon::SpawnPoint),
								fX,
								fY,
								270.0f - RAD2DEG(pEntity->GetWorldAngles().z),
								friendly,
								100,
								100, iOnScreenObjective == locations[i],
								iCurrentSpawnPoint == locations[i],
								color
							);
							m_possibleOnScreenObjectives.push_back(pEntity->GetId());
						}
						else
						{
							SmartScriptTable props;
							if (pEntity->GetScriptTable() && pEntity->GetScriptTable()->GetValue("Properties", props))
							{
								int capturable = 0;
								if (props->GetValue("bCapturable", capturable) && capturable)
								{
									numOfValues += FillUpDoubleArray(&entityValues,
										pEntity->GetId(),
										static_cast<int>(MiniMapIcon::SpawnPoint),
										fX,
										fY,
										270.0f - RAD2DEG(pEntity->GetWorldAngles().z),
										friendly,
										100,
										100,
										iOnScreenObjective == locations[i],
										iCurrentSpawnPoint == locations[i],
										color
									);
									m_possibleOnScreenObjectives.push_back(pEntity->GetId());
								}
							}
						}
					}
				}
			}
		}
	}

	//we need he players position later int the code
	Vec2 vPlayerPos(0.5f, 0.5f);

	if (isMultiplayer)
	{
		//special units
		const std::vector<CGameRules::SMinimapEntity> synchEntities = m_pGameRules->GetMinimapEntities();
		for (int m = 0; m < synchEntities.size(); ++m)
		{
			CGameRules::SMinimapEntity mEntity = synchEntities[m];
			MiniMapIcon type = GetSynchedEntityType(mEntity.type);
			IEntity* pEntity = NULL;
			if (type == MiniMapIcon::NuclearWeapon || type == MiniMapIcon::TechCharger)	//might be a gun
			{
				if (IItem* pWeapon = g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(mEntity.entityId))
				{
					if (pWeapon->GetOwnerId())
						pEntity = gEnv->pEntitySystem->GetEntity(pWeapon->GetOwnerId());
				}
				else
					pEntity = gEnv->pEntitySystem->GetEntity(mEntity.entityId);
			}

			if (GetPosOnMap(pEntity, fX, fY))
			{
				int friendly = FriendOrFoe(isMultiplayer, team, pEntity, m_pGameRules);
				if (friendly == EFriend || friendly == ENeutral)
				{
					if (type == MiniMapIcon::AmmoTruck && stl::find_in_map(drawnVehicles, mEntity.entityId, false))
					{
						numOfValues += FillUpDoubleArray(&entityValues, 
							pEntity->GetId(), 
							static_cast<int>(MiniMapIcon::Barracks), 
							fX, 
							fY, 
							270.0f - RAD2DEG(pEntity->GetWorldAngles().z), 
							friendly, 
							100, 
							100, 
							iOnScreenObjective == mEntity.entityId, 
							iCurrentSpawnPoint == mEntity.entityId
						);
					}
					else
						numOfValues += FillUpDoubleArray(&entityValues, 
							pEntity->GetId(), 
							static_cast<int>(type),
							fX, 
							fY, 
							270.0f - RAD2DEG(pEntity->GetWorldAngles().z), 
							friendly, 
							100, 
							100, 
							iOnScreenObjective == mEntity.entityId, 
							iCurrentSpawnPoint == mEntity.entityId
						);
				}
			}
		}
	}

	//helpers for actor rendering
	IActor* pTempActor = NULL;
	//draw tagged guys and vehicles...
	{
		for (int i = 0; i < NUM_TAGGED_ENTITIES; ++i)
		{
			EntityId id = m_taggedEntities[i];
			if (id)
			{
				pTempActor = m_pActorSystem->GetActor(id);
				if (pTempActor)
				{
					if (IVehicle* pVehicle = pTempActor->GetLinkedVehicle())
					{
						if (!stl::find_in_map(drawnVehicles, pVehicle->GetEntityId(), false))
						{
							GetPosOnMap(pVehicle->GetEntity(), fX, fY);
							numOfValues += FillUpDoubleArray(&entityValues, 
								pVehicle->GetEntityId(),
								static_cast<int>(MiniMapIcon::TaggedEntity),
								fX, 
								fY, 
								270.0f - RAD2DEG(pVehicle->GetEntity()->GetWorldAngles().z), 
								ENeutral, 
								100, 
								100, 
								iOnScreenObjective == pVehicle->GetEntityId(), 
								iCurrentSpawnPoint == pVehicle->GetEntityId()
							);
							drawnVehicles[pVehicle->GetEntityId()] = true;
						}
					}
					else
					{
						GetPosOnMap(pTempActor->GetEntity(), fX, fY);
						numOfValues += FillUpDoubleArray(&entityValues, 
							pClientActor->GetEntityId(),
							static_cast<int>(MiniMapIcon::TaggedEntity),
							fX, 
							fY, 
							270.0f - RAD2DEG(pTempActor->GetEntity()->GetWorldAngles().z), 
							ENeutral,
							100, 
							100, 
							iOnScreenObjective == pClientActor->GetEntityId(),
							iCurrentSpawnPoint == pClientActor->GetEntityId()
						);
					}
				}
				else if (IVehicle* pVehicle = m_pVehicleSystem->GetVehicle(id))
				{
					if (!stl::find_in_map(drawnVehicles, pVehicle->GetEntityId(), false))
					{
						GetPosOnMap(pVehicle->GetEntity(), fX, fY);
						numOfValues += FillUpDoubleArray(&entityValues, 
							pVehicle->GetEntityId(), 
							static_cast<int>(MiniMapIcon::TaggedEntity),
							fX, 
							fY, 
							270.0f - RAD2DEG(pVehicle->GetEntity()->GetWorldAngles().z), 
							ENeutral, 
							100, 
							100, 
							iOnScreenObjective == pVehicle->GetEntityId(), 
							iCurrentSpawnPoint == pVehicle->GetEntityId()
						);
						drawnVehicles[pVehicle->GetEntityId()] = true;
					}
				}
			}
		}
	}

	//draw story entities (icons with text)
	{
		for (int e = 0; e < m_storyEntitiesOnRadar.size(); ++e)
		{
			EntityId id = m_storyEntitiesOnRadar[e].m_id;
			if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id))
			{
				GetPosOnMap(pEntity, fX, fY);
				numOfValues += FillUpDoubleArray(&entityValues, 
					id, 
					static_cast<int>(m_storyEntitiesOnRadar[e].m_miniMapIcon),
					fX, 
					fY,
					270.0f - RAD2DEG(pEntity->GetWorldAngles().z),
					ENeutral, 
					100, 
					100, 
					iOnScreenObjective == id, 
					iCurrentSpawnPoint == id
				);
				if (!m_storyEntitiesOnRadar[e].m_text.empty())
					textOnMap[id] = m_storyEntitiesOnRadar[e].m_text;
			}
		}
	}

	//draw position of teammates ...
	{
		for (const EntityId id : m_teamMates)
		{
			pTempActor = m_pActorSystem->GetActor(id);
			if (pTempActor && pTempActor != pClientActor)
			{
				if (IVehicle* pVehicle = pTempActor->GetLinkedVehicle())
				{
					if (!stl::find_in_map(drawnVehicles, pVehicle->GetEntityId(), false))
					{
						GetPosOnMap(pVehicle->GetEntity(), fX, fY);
						numOfValues += FillUpDoubleArray(&entityValues, 
							pTempActor->GetEntityId(),
							static_cast<int>(ChooseMiniMapIcon(pVehicle->GetEntity())),
							fX, 
							fY, 
							270.0f - RAD2DEG(pVehicle->GetEntity()->GetWorldAngles().z), 
							EFriend, 
							100, 
							100, 
							iOnScreenObjective == pVehicle->GetEntityId(), 
							iCurrentSpawnPoint == pVehicle->GetEntityId()
						);
						drawnVehicles[pVehicle->GetEntityId()] = true;
					}
					//draw teammate name if selected
					if (gEnv->bMultiplayer)
					{
						if (IsTeamMateSelected(id))
						{
							textOnMap[id] = pTempActor->GetEntity()->GetName();
						}
					}
				}
				else
				{
					bool spectating = false;
					if (isMultiplayer)
						spectating = (static_cast<CActor*>(pTempActor)->GetSpectatorMode() != CActor::eASM_None);
					if (!spectating)
					{
						MiniMapIcon icon = MiniMapIcon::Player;
						if (g_pGameCVars->mp_deadPlayersOnMinimap && static_cast<CActor*>(pTempActor)->GetPhysicsProfile() == eAP_Ragdoll)
						{
							icon = MiniMapIcon::DeathSkull;
						}

						GetPosOnMap(pTempActor->GetEntity(), fX, fY);
						numOfValues += FillUpDoubleArray(&entityValues, 
							pTempActor->GetEntityId(), 
							static_cast<int>(icon),
							fX, 
							fY, 
							270.0f - RAD2DEG(pTempActor->GetEntity()->GetWorldAngles().z), 
							EFriend,
							100, 
							100, 
							iOnScreenObjective == pTempActor->GetEntityId(),
							iCurrentSpawnPoint == pTempActor->GetEntityId()
						);

						drawnPlayers[pTempActor->GetEntityId()] = true;

						//draw teammate name if selected
						if (gEnv->bMultiplayer)
						{
							if (IsTeamMateSelected(id))
							{
								textOnMap[id] = pTempActor->GetEntity()->GetName();
							}
						}
					}
				}
			}
		}
	}

	//draw radar entities on the map (scanned enemies and vehicles) 
	//scanned vehicles have to be drawn last to find the "neutral" ones correctly
	for (int i = 0; i < m_entitiesOnRadar.size(); ++i)
	{
		EntityId uiEntityId = m_entitiesOnRadar[i].m_id;
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(uiEntityId);
		if (!pEntity || pEntity->IsHidden())
		{
			RemoveFromRadar(uiEntityId);
			continue;
		}

		pTempActor = m_pActorSystem->GetActor(uiEntityId);
		if (pTempActor)
		{
			IVehicle* pVehicle = pTempActor->GetLinkedVehicle();
			if (pVehicle && !pVehicle->IsDestroyed())
			{
				if (!stl::find_in_map(drawnVehicles, pVehicle->GetEntityId(), false))
				{
					GetPosOnMap(pVehicle->GetEntity(), fX, fY);
					int friendly = FriendOrFoe(isMultiplayer, team, pVehicle->GetEntity(), m_pGameRules);
					numOfValues += FillUpDoubleArray(&entityValues, 
						pVehicle->GetEntityId(), 
						static_cast<int>(ChooseMiniMapIcon(pVehicle->GetEntity())),
						fX, 
						fY, 
						270.0f - RAD2DEG(pVehicle->GetEntity()->GetWorldAngles().z), 
						friendly,
						100,
						100, 
						iOnScreenObjective == pVehicle->GetEntityId(),
						iCurrentSpawnPoint == pVehicle->GetEntityId()
					);
					drawnVehicles[pVehicle->GetEntityId()] = true;
				}
			}
			else
			{
				int friendly = FriendOrFoe(isMultiplayer, team, pTempActor->GetEntity(), m_pGameRules);
				GetPosOnMap(pTempActor->GetEntity(), fX, fY);
				numOfValues += FillUpDoubleArray(&entityValues, 
					pTempActor->GetEntityId(), 
					static_cast<int>(MiniMapIcon::Player),
					fX, 
					fY, 
					270.0f - RAD2DEG(pTempActor->GetEntity()->GetWorldAngles().z),
					friendly, 
					100,
					100,
					iOnScreenObjective == pTempActor->GetEntityId(), 
					iCurrentSpawnPoint == pTempActor->GetEntityId()
				);
			}
		}
		else if (IVehicle* pVehicle = m_pVehicleSystem->GetVehicle(uiEntityId))
		{
			if (!stl::find_in_map(drawnVehicles, uiEntityId, false))
			{
				if (pVehicle->IsDestroyed())
				{
					RemoveFromRadar(uiEntityId);
					continue;
				}

				int friendly = FriendOrFoe(isMultiplayer, team, pEntity, m_pGameRules);
				GetPosOnMap(pEntity, fX, fY);
				numOfValues += FillUpDoubleArray(&entityValues, 
					uiEntityId, 
					static_cast<int>(ChooseMiniMapIcon(pEntity)),
					fX, 
					fY, 
					270.0f - RAD2DEG(pEntity->GetWorldAngles().z), 
					friendly,
					100, 
					100, 
					iOnScreenObjective == uiEntityId, 
					iCurrentSpawnPoint == uiEntityId
				);
				drawnVehicles[uiEntityId] = true;
			}
		}
	}

	//draw temporarily tagged units
	{
		for (int e = 0; e < m_tempEntitiesOnRadar.size(); ++e)
		{
			EntityId id = m_tempEntitiesOnRadar[e].m_id;
			pTempActor = m_pActorSystem->GetActor(id);
			if (pTempActor)
			{
				IVehicle* pVehicle = pTempActor->GetLinkedVehicle();
				if (pVehicle && !pVehicle->IsDestroyed())
				{
					if (!stl::find_in_map(drawnVehicles, pVehicle->GetEntityId(), false))
					{
						MiniMapIcon icon = m_tempEntitiesOnRadar[e].m_miniMapIcon;

						if (icon == MiniMapIcon::Player)
						{
							icon = ChooseMiniMapIcon(pVehicle->GetEntity());
						}

						GetPosOnMap(pVehicle->GetEntity(), fX, fY);
						numOfValues += FillUpDoubleArray(&entityValues,
							pVehicle->GetEntityId(),
							static_cast<int>(icon),
							fX,
							fY,
							270.0f - RAD2DEG(pVehicle->GetEntity()->GetWorldAngles().z),
							FriendOrFoe(isMultiplayer, team, pVehicle->GetEntity(), m_pGameRules),
							100,
							100,
							iOnScreenObjective == pVehicle->GetEntityId(),
							iCurrentSpawnPoint == pVehicle->GetEntityId()
						);
						drawnVehicles[pVehicle->GetEntityId()] = true;
					}
				}
				else
				{
					if (!stl::find_in_map(drawnPlayers, pTempActor->GetEntityId(), false))
					{
						MiniMapIcon icon = m_tempEntitiesOnRadar[e].m_miniMapIcon != MiniMapIcon::None ?
							m_tempEntitiesOnRadar[e].m_miniMapIcon : ChooseMiniMapIcon(pTempActor->GetEntity());

						if (icon == MiniMapIcon::DeathSkull && static_cast<CActor*>(pTempActor)->GetPhysicsProfile() != eAP_Ragdoll)
						{
							continue;
						}

						GetPosOnMap(pTempActor->GetEntity(), fX, fY);
						numOfValues += FillUpDoubleArray(&entityValues,
							pClientActor->GetEntityId(),
							static_cast<int>(icon),
							fX,
							fY,
							270.0f - RAD2DEG(pTempActor->GetEntity()->GetWorldAngles().z),
							FriendOrFoe(isMultiplayer, team, pTempActor->GetEntity(), m_pGameRules),
							100,
							100,
							iOnScreenObjective == pTempActor->GetEntityId(),
							iCurrentSpawnPoint == pTempActor->GetEntityId()
						);
					}
				}
			}
			else if (IVehicle* pVehicle = m_pVehicleSystem->GetVehicle(id))
			{
				if (!stl::find_in_map(drawnVehicles, pVehicle->GetEntityId(), false))
				{
					MiniMapIcon icon = m_tempEntitiesOnRadar[e].m_miniMapIcon != MiniMapIcon::None ?
						m_tempEntitiesOnRadar[e].m_miniMapIcon : ChooseMiniMapIcon(pVehicle->GetEntity());

					GetPosOnMap(pVehicle->GetEntity(), fX, fY);
					numOfValues += FillUpDoubleArray(&entityValues,
						pVehicle->GetEntityId(),
						static_cast<int>(icon),
						fX,
						fY,
						270.0f - RAD2DEG(pVehicle->GetEntity()->GetWorldAngles().z),
						FriendOrFoe(isMultiplayer, team, pVehicle->GetEntity(), m_pGameRules),
						100,
						100,
						iOnScreenObjective == pVehicle->GetEntityId(),
						iCurrentSpawnPoint == pVehicle->GetEntityId()
					);
					drawnVehicles[pVehicle->GetEntityId()] = true;
				}
			}
		}
	}

	//draw player VEHICLE position

	if (IVehicle* pVehicle = pClientActor->GetLinkedVehicle())
	{
		//if(!stl::find_in_map(drawnVehicles, pVehicle->GetEntityId(), false))
		//{
		GetPosOnMap(pVehicle->GetEntity(), fX, fY);
		vPlayerPos.x = fX;
		vPlayerPos.y = fY;
		numOfValues += FillUpDoubleArray(&entityValues,
			pVehicle->GetEntityId(), 
			static_cast<int>(ChooseMiniMapIcon(pVehicle->GetEntity())),
			fX, 
			fY,
			270.0f - RAD2DEG(pVehicle->GetEntity()->GetWorldAngles().z),
			ESelf,
			100, 
			100,
			iOnScreenObjective == pVehicle->GetEntityId(), 
			iCurrentSpawnPoint == pVehicle->GetEntityId()
		);
		drawnVehicles[pVehicle->GetEntityId()] = true;
		//}
	}

	//draw player position
	if (!pClientActor->GetLinkedVehicle() && pClientActor->GetSpectatorMode() == CActor::eASM_None)
	{
		string name(pClientActor->GetEntity()->GetName());
		int icon = (name.find("Quarantine", 0) != string::npos) ? static_cast<int>(MiniMapIcon::TechCharger) : static_cast<int>(MiniMapIcon::Player);
		int faction = ESelf;

		if (g_pGameCVars->mp_deadPlayersOnMinimap && pClientActor->GetPhysicsProfile() == eAP_Ragdoll)
		{
			icon = static_cast<int>(MiniMapIcon::DeathSkull);
		}

		GetPosOnMap(pClientActor->GetEntity(), fX, fY);
		vPlayerPos.x = fX;
		vPlayerPos.y = fY;
		numOfValues += FillUpDoubleArray(&entityValues,
			pClientActor->GetEntityId(),
			icon,
			fX, 
			fY, 
			270.0f - RAD2DEG(pClientActor->GetEntity()->GetWorldAngles().z),
			faction, 
			100, 
			100,
			iOnScreenObjective == pClientActor->GetEntityId(),
			iCurrentSpawnPoint == pClientActor->GetEntityId()
		);
	}

	//.. and mission objectives
	if (!gEnv->bMultiplayer)
	{
		std::map<EntityId, RadarObjective>::const_iterator it = m_missionObjectives.begin();
		std::map<EntityId, RadarObjective>::const_iterator end = m_missionObjectives.end();

		for (; it != end; ++it)
		{
			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(it->first);
			if (GetPosOnMap(pEntity, fX, fY))
			{
				numOfValues += FillUpDoubleArray(&entityValues, 
					pEntity->GetId(), 
					(it->second.secondaryObjective) ? static_cast<int>(MiniMapIcon::SecondaryObjective) : static_cast<int>(MiniMapIcon::WayPoint),
					fX, 
					fY,
					180.0f, 
					ENeutral,
					100, 
					100, 
					iOnScreenObjective == it->first, 
					iCurrentSpawnPoint == it->first
				);
				textOnMap[pEntity->GetId()] = it->second.text;
			}
		}
	}

	ComputePositioning(vPlayerPos, &entityValues);

	if (entityValues.size())
		m_flashMap->GetFlashPlayer()->SetVariableArray(FVAT_Double, 
			"Root.PDAArea.Map_M.MapArea.m_allValues", 0, &entityValues[0], numOfValues);

	m_flashMap->Invoke("Root.PDAArea.Map_M.MapArea.setObjectArray");
	//render text strings
	std::map<EntityId, string>::const_iterator itText = textOnMap.begin();
	for (; itText != textOnMap.end(); ++itText)
	{
		SFlashVarValue args[2] = { itText->first, itText->second.c_str() };
		m_flashMap->Invoke("Root.PDAArea.Map_M.MapArea.setText", args, 2);
	}
}

//-----------------------------------------------------------------------------------------------------

//calculate correct map positions, while the map is zoomed or translated
void CHUDRadar::ComputePositioning(Vec2 playerpos, std::vector<double>* doubleArray)
{

	bool bUpdate = false;
	//update zooming
	if (m_fPDATempZoomFactor != m_fPDAZoomFactor)
	{
		float fRatio = m_fPDATempZoomFactor - m_fPDAZoomFactor;
		if (cry_fabsf(fRatio) > 0.01)
		{
			m_fPDAZoomFactor += fRatio * 1.0;
		}
		else
		{
			m_fPDAZoomFactor = m_fPDATempZoomFactor;
		}
		bUpdate = true;
	}

	float fMapSize = 508.0f * m_fPDAZoomFactor;

	//update dragging
	float fLimit = (1.0f / fMapSize);

	if (m_vPDAMapTranslation.x != m_vPDATempMapTranslation.x)
	{
		float fDiffX = m_vPDATempMapTranslation.x - m_vPDAMapTranslation.x;
		if (cry_fabsf(fDiffX) > fLimit)
		{
			m_vPDAMapTranslation.x += fDiffX * 1.0f;
		}
		else
		{
			m_vPDAMapTranslation.x = m_vPDATempMapTranslation.x;
		}
		bUpdate = true;
	}
	if (m_vPDAMapTranslation.y != m_vPDATempMapTranslation.y)
	{
		float fDiffY = m_vPDATempMapTranslation.y - m_vPDAMapTranslation.y;
		if (cry_fabsf(fDiffY) > fLimit)
		{
			m_vPDAMapTranslation.y += fDiffY * 1.0;
		}
		else
		{
			m_vPDAMapTranslation.y = m_vPDATempMapTranslation.y;
		}
		bUpdate = true;
	}

	//calculate offset
	Vec2 vOffset = Vec2(0, 0);
	Vec2 vMapPos = Vec2((254.0f - playerpos.x * fMapSize) + m_vPDAMapTranslation.x, (254.0f + (1.0f - playerpos.y) * fMapSize) + m_vPDAMapTranslation.y);

	if (vMapPos.x > 0.0f)
	{
		vOffset.x = -vMapPos.x;
	}
	else if (vMapPos.x < (508.0f - fMapSize))
	{
		vOffset.x = (508.0f - fMapSize) - vMapPos.x;
	}

	if (vMapPos.y < 508)
	{
		vOffset.y = 508 - vMapPos.y;
	}
	else if (vMapPos.y > fMapSize)
	{
		vOffset.y = fMapSize - vMapPos.y;
	}
	//if(bUpdate || m_initMap)
	{
		m_initMap = false;

		m_flashPDA->SetVariable("Root.PDAArea.Map_M.MapArea.Map.Map_G._xscale", SFlashVarValue(100.0f * m_fPDAZoomFactor));
		m_flashPDA->SetVariable("Root.PDAArea.Map_M.MapArea.Map.Map_G._yscale", SFlashVarValue(100.0f * m_fPDAZoomFactor));
		m_flashPDA->SetVariable("Root.PDAArea.Map_M.MapArea.Map.Map_G._x", SFlashVarValue(vMapPos.x + vOffset.x));
		m_flashPDA->SetVariable("Root.PDAArea.Map_M.MapArea.Map.Map_G._y", SFlashVarValue(vMapPos.y + vOffset.y));

		float fStep = 63.5f * m_fPDAZoomFactor;
		float fOffsetX = (fStep * 0.5f) - 70.0f;
		float fOffsetY = (fStep * 0.5f) - 18.0f;

		m_flashPDA->SetVariable("Root.PDAArea.Map_M.SectorA._y", SFlashVarValue((vMapPos.y + vOffset.y) - fStep * 8.0f + fOffsetY));
		m_flashPDA->SetVariable("Root.PDAArea.Map_M.SectorB._y", SFlashVarValue((vMapPos.y + vOffset.y) - fStep * 7.0f + fOffsetY));
		m_flashPDA->SetVariable("Root.PDAArea.Map_M.SectorC._y", SFlashVarValue((vMapPos.y + vOffset.y) - fStep * 6.0f + fOffsetY));
		m_flashPDA->SetVariable("Root.PDAArea.Map_M.SectorD._y", SFlashVarValue((vMapPos.y + vOffset.y) - fStep * 5.0f + fOffsetY));
		m_flashPDA->SetVariable("Root.PDAArea.Map_M.SectorE._y", SFlashVarValue((vMapPos.y + vOffset.y) - fStep * 4.0f + fOffsetY));
		m_flashPDA->SetVariable("Root.PDAArea.Map_M.SectorF._y", SFlashVarValue((vMapPos.y + vOffset.y) - fStep * 3.0f + fOffsetY));
		m_flashPDA->SetVariable("Root.PDAArea.Map_M.SectorG._y", SFlashVarValue((vMapPos.y + vOffset.y) - fStep * 2.0f + fOffsetY));
		m_flashPDA->SetVariable("Root.PDAArea.Map_M.SectorH._y", SFlashVarValue((vMapPos.y + vOffset.y) - fStep * 1.0f + fOffsetY));

		m_flashPDA->SetVariable("Root.PDAArea.Map_M.Sector1._x", SFlashVarValue((vMapPos.x + vOffset.x) + fStep * 1.0f - fOffsetX));
		m_flashPDA->SetVariable("Root.PDAArea.Map_M.Sector2._x", SFlashVarValue((vMapPos.x + vOffset.x) + fStep * 2.0f - fOffsetX));
		m_flashPDA->SetVariable("Root.PDAArea.Map_M.Sector3._x", SFlashVarValue((vMapPos.x + vOffset.x) + fStep * 3.0f - fOffsetX));
		m_flashPDA->SetVariable("Root.PDAArea.Map_M.Sector4._x", SFlashVarValue((vMapPos.x + vOffset.x) + fStep * 4.0f - fOffsetX));
		m_flashPDA->SetVariable("Root.PDAArea.Map_M.Sector5._x", SFlashVarValue((vMapPos.x + vOffset.x) + fStep * 5.0f - fOffsetX));
		m_flashPDA->SetVariable("Root.PDAArea.Map_M.Sector6._x", SFlashVarValue((vMapPos.x + vOffset.x) + fStep * 6.0f - fOffsetX));
		m_flashPDA->SetVariable("Root.PDAArea.Map_M.Sector7._x", SFlashVarValue((vMapPos.x + vOffset.x) + fStep * 7.0f - fOffsetX));
		m_flashPDA->SetVariable("Root.PDAArea.Map_M.Sector8._x", SFlashVarValue((vMapPos.x + vOffset.x) + fStep * 8.0f - fOffsetX));

		float value = 0;
		value = (vMapPos.x + vOffset.x) + fStep * 4.0f - fOffsetX;
		value = (vMapPos.x + vOffset.x) + fStep * 4.0f - fOffsetX;
	}

	//calculate icon positions
	for (int i(0); i < doubleArray->size(); i += 11)
	{
		if (doubleArray->size() > i + 6)
		{
			(*doubleArray)[i + 2] = 254.0f - ((playerpos.x - (*doubleArray)[i + 2]) * fMapSize) + m_vPDAMapTranslation.x + vOffset.x;
			(*doubleArray)[i + 3] = 254.0f - ((playerpos.y - (*doubleArray)[i + 3]) * fMapSize) + m_vPDAMapTranslation.y + vOffset.y;
			(*doubleArray)[i + 6] = (*doubleArray)[i + 6] * (m_fPDAZoomFactor + 2.0f) * 0.25f;
		}
	}
	if (m_bDragMap)
		m_vPDATempMapTranslation += vOffset;
}

//-----------------------------------------------------------------------------------------------------

void CHUDRadar::InitMap()
{
	m_initMap = true;
	m_vPDATempMapTranslation = Vec2(0, 0);
}

bool CHUDRadar::ZoomPDA(bool bZoomDirection)
{
	float fNextStep = 0.25f - (0.25f / m_fPDATempZoomFactor);

	if (bZoomDirection)
		m_fPDATempZoomFactor *= (1.1f + fNextStep);
	else
		m_fPDATempZoomFactor /= (1.1f + fNextStep);

	m_fPDATempZoomFactor = clamp_tpl(m_fPDATempZoomFactor, 1.0f, 8.0f);
	return true;
}

bool CHUDRadar::ZoomChangePDA(float p_fValue)
{
	m_fPDATempZoomFactor -= (p_fValue * 0.03);
	m_fPDATempZoomFactor = clamp_tpl(m_fPDATempZoomFactor, 1.0f, 8.0f);
	return true;
}

bool CHUDRadar::DragMap(Vec2 p_vDir)
{
	m_vPDATempMapTranslation += p_vDir * m_fPDAZoomFactor;
	return true;
}

void CHUDRadar::SetDrag(bool enabled)
{
	m_bDragMap = enabled;
}

//-----------------------------------------------------------------------------------------------------

void CHUDRadar::Serialize(TSerialize ser)
{
	if (ser.GetSerializationTarget() != eST_Network)
	{
		ser.BeginGroup("HUDRadar");
		int amount = 0;
		amount = m_entitiesOnRadar.size();
		ser.Value("AmountOfRadarEntities", amount);
		if (ser.IsReading())
		{
			m_entitiesOnRadar.clear();
			m_entitiesOnRadar.resize(amount);
		}
		for (int h = 0; h < amount; ++h)
		{
			ser.BeginGroup("RadarEntity");
			ser.Value("id", m_entitiesOnRadar[h].m_id);
			ser.EndGroup();
		}

		amount = m_storyEntitiesOnRadar.size();
		ser.Value("AmountOfStoryEntities", amount);
		if (ser.IsReading())
		{
			m_storyEntitiesOnRadar.clear();
			m_storyEntitiesOnRadar.resize(amount);
		}
		for (int s = 0; s < amount; ++s)
		{
			ser.BeginGroup("StoryEntity");
			ser.Value("id", m_storyEntitiesOnRadar[s].m_id);

			int type = static_cast<int>(m_storyEntitiesOnRadar[s].m_miniMapIcon);
			ser.EnumValue("type", type,
				static_cast<int>(MiniMapIcon::None),
				static_cast<int>(MiniMapIcon::LastType));
			m_storyEntitiesOnRadar[s].m_miniMapIcon = static_cast<MiniMapIcon>(type);
			
			ser.Value("spawnTime", m_storyEntitiesOnRadar[s].m_spawnTime);
			ser.Value("timeLimit", m_storyEntitiesOnRadar[s].m_timeLimit);
			ser.Value("text", m_storyEntitiesOnRadar[s].m_text);
			ser.EndGroup();
		}

		amount = m_teamMates.size();
		ser.Value("teamMatesAmount", amount);
		if (amount)
		{
			if (ser.IsReading())
			{
				m_teamMates.clear();
				m_teamMates.resize(amount);
			}

			for (int i = 0; i < amount; ++i)
			{
				ser.BeginGroup("teamMateId");
				ser.Value("mateID", m_teamMates[i]);
				ser.EndGroup();
			}
		}

		ser.Value("PDAZoomFactor", m_fPDAZoomFactor);

		for (int h = 0; h < NUM_TAGGED_ENTITIES; ++h)
		{
			ser.BeginGroup("TaggedEntity");
			ser.Value("id", m_taggedEntities[h]);
			ser.EndGroup();
		}

		ser.Value("jammerID", m_jammerID);
		ser.Value("jammerRadius", m_jammerRadius);
		ser.Value("jammerValue", m_jammingValue);
		ser.Value("m_jammerDisconnectMap", m_jammerDisconnectMap);

		ser.Value("miniMapId", m_mapId);

		amount = m_missionObjectives.size();
		ser.Value("AmountOfMissionObjectives", amount);
		if (ser.IsReading())
		{
			if (gEnv->pSystem->IsSerializingFile() == 1) //only when quickloading
				SetMiniMapTexture(m_mapId);
			m_flashRadar->Invoke("setNoiseValue", m_jammingValue);
			m_flashPDA->Invoke("setDisconnect", m_jammerDisconnectMap);

			m_entitiesInProximity.clear();
			m_itemsInProximity.clear();

			m_missionObjectives.clear();
			EntityId id = 0;
			string text;
			for (int i = 0; i < amount; ++i)
			{
				ser.BeginGroup("MissionObjective");
				ser.Value("Active", id);
				RadarObjective objective;
				ser.Value("Text", objective.text);
				ser.Value("Secondary", objective.secondaryObjective);
				ser.EndGroup();
				m_missionObjectives[id] = objective;
			}

			//reset update timer
			m_lastScan = 0.0f;
			m_startBroadScanTime = 0.0f;
			m_fLastRadarRatio = -1.0f;
		}
		else //writing
		{
			std::map<EntityId, RadarObjective>::iterator it = m_missionObjectives.begin();
			for (int i = 0; i < amount; ++i, ++it)
			{
				EntityId id = it->first;
				string text = it->second.text;
				ser.BeginGroup("MissionObjective");
				ser.Value("Active", id);
				ser.Value("Text", text);
				ser.Value("Secondary", it->second.secondaryObjective);
				ser.EndGroup();
			}
		}

		ser.EndGroup(); //HUDRadar;
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDRadar::UpdateMissionObjective(EntityId id, bool active, const char* description, bool secondary)
{
	if (id == 0)
		return;

	std::map<EntityId, RadarObjective>::iterator iter = m_missionObjectives.find(id);
	if (iter != m_missionObjectives.end() && !active)
		m_missionObjectives.erase(iter);
	else if (active)
		m_missionObjectives.insert(std::map<EntityId, RadarObjective>::value_type(id, RadarObjective(description, secondary)));
}

//-----------------------------------------------------------------------------------------------------

const char* CHUDRadar::GetObjectiveDescription(EntityId id)
{
	std::map<EntityId, RadarObjective>::iterator iter = m_missionObjectives.find(id);
	if (iter != m_missionObjectives.end())
	{
		return iter->second.text.c_str();
	}
	return "";
}

//-----------------------------------------------------------------------------------------------------

MiniMapIcon CHUDRadar::ChooseMiniMapIcon(IEntity* pEntity)
{
	if (!pEntity)
	{
		return MiniMapIcon::None;
	}

	const IEntityClass* pCls = pEntity->GetClass();
	const char* cls = pCls->GetName();
	const char* name = pEntity->GetName();

	MiniMapIcon returnType = MiniMapIcon::LTV;

	if (pCls == m_pPlayerClass || pCls == m_pGrunt)
		returnType = MiniMapIcon::Player;
	else if (pCls == m_pAlien || pCls == m_pTrooper)
		returnType = MiniMapIcon::Player;
	else if (pCls == m_pLTVUS || pCls == m_pLTVA)
		returnType = MiniMapIcon::LTV;
	else if (pCls == m_pTankUS || pCls == m_pTankA)
		returnType = MiniMapIcon::Tank;
	else if (pCls == m_pAAA)
		returnType = MiniMapIcon::AAA;
	else if (pCls == m_pTruck)
		returnType = MiniMapIcon::Truck;
	else if (pCls == m_pTruck || pCls == m_pAPCUS || pCls == m_pAPCA)
		returnType = MiniMapIcon::APC;
	else if (pCls == m_pHeli)
		returnType = MiniMapIcon::Heli;
	else if (pCls == m_pVTOL)
		returnType = MiniMapIcon::VTOL;
	else if (pCls == m_pScout)
		returnType = MiniMapIcon::Heli;
	else if (pCls == m_pWarrior || pCls == m_pHunter)
		returnType = MiniMapIcon::INVALID1;
	else if (pCls == m_pBoatCiv)
		returnType = MiniMapIcon::SmallBoat;
	else if (pCls == m_pHover)
		returnType = MiniMapIcon::Hovercraft;
	else if (pCls == m_pBoatA)
		returnType = MiniMapIcon::PatrolBoat;
	else if (pCls == m_pBoatUS)
		returnType = MiniMapIcon::SpeedBoat;
	else if (!_stricmp(cls, "HQ"))
	{
		if (m_pGameRules->GetTeam(pEntity->GetId()) == 2) //us team
			returnType = MiniMapIcon::Headquarter2;
		else
			returnType = MiniMapIcon::Headquarter;
	}
	else if (!_stricmp(cls, "Factory") && m_pHUD->GetPowerStruggleHUD())	//this should be much bigger and choose out of the different factory versions (not yet existing)
	{
		if (m_pHUD->GetPowerStruggleHUD()->CanBuild(pEntity, "ustank"))
		{
			returnType = MiniMapIcon::FactoryTank;
		}
		else if (m_pHUD->GetPowerStruggleHUD()->CanBuild(pEntity, "nkhelicopter"))
		{
			returnType = MiniMapIcon::FactoryAir;
		}
		else if (m_pHUD->GetPowerStruggleHUD()->CanBuild(pEntity, "nkboat"))
		{
			returnType = MiniMapIcon::FactorySea;
		}
		else if (m_pHUD->GetPowerStruggleHUD()->CanBuild(pEntity, "us4wd"))
		{
			returnType = MiniMapIcon::FactoryVehicle;
		}
		else
		{
			returnType = MiniMapIcon::FactoryPrototype;
		}
	}
	else if (!_stricmp(cls, "AlienEnergyPoint"))
	{
		returnType = MiniMapIcon::AlienEnergySource;
	}

	return returnType;
}

//-----------------------------------------------------------------------------------------------------

RadarIcon CHUDRadar::ChooseRadarIcon(IEntity* pEntity)
{
	if (!pEntity)
		return RadarIcon::None;

	const IEntityClass* pCls = pEntity->GetClass();

	if (pCls == m_pAutoTurret || pCls == m_pAutoTurretAA)
		return RadarIcon::Eight; // 8

	if (pCls == m_pPlayerClass || pCls == m_pGrunt || pCls == m_pAlien || pCls == m_pTrooper)
		return RadarIcon::One; // 1

	if (pCls == m_pHeli || pCls == m_pScout)
		return RadarIcon::Two; // 2

	if (pCls == m_pWarrior || pCls == m_pHunter)
		return RadarIcon::One; // 1

	return RadarIcon::Three; // 3
}

//-----------------------------------------------------------------------------------------------------

FlashRadarFaction CHUDRadar::FriendOrFoe(bool multiplayer, int playerTeam, IEntity* entity, CGameRules* pGameRules)
{
	FlashRadarFaction val = ENeutral;

	if (multiplayer)
	{
		int friendly = pGameRules->GetTeam(entity->GetId());
		if (friendly != ENeutral)
		{
			if (friendly == playerTeam)
				friendly = EFriend;
			else
				friendly = EEnemy;
		}
		val = (FlashRadarFaction)friendly;
	}
	else if (entity->GetAI())
	{
		IAIObject* pAI = entity->GetAI();
		if (pAI && pAI->GetAIType() == AIOBJECT_VEHICLE)
		{
			if (IVehicle* pVehicle = m_pVehicleSystem->GetVehicle(entity->GetId()))
			{
				if (IVehicleSeat* pSeat = pVehicle->GetSeatById(1))
					if (pSeat->IsDriver())
					{
						EntityId driverId = pSeat->GetPassenger();
						IEntity* temp = gEnv->pEntitySystem->GetEntity(driverId);
						if (temp && temp->GetAI())
							pAI = temp->GetAI();
					}
			}
		}
		if (pAI && pAI->GetAIType() != AIOBJECT_VEHICLE) //also checking driver from former vehicle
		{
			IEntity* playerEntity = m_pClientActor->GetEntity();
			if (playerEntity && pAI->IsHostile(playerEntity->GetAI(), false))
				val = EEnemy;
			else
				val = EFriend;
		}
	}

	return val;
}

MiniMapIcon CHUDRadar::GetSynchedEntityType(int type)
{
	switch (type)
	{
	case 1:
		return MiniMapIcon::AmmoTruck;
		break;
	case 2:
		return MiniMapIcon::NuclearWeapon;	//this is a tac entity
		break;
	case 3:
		return MiniMapIcon::TechCharger;
		break;
	default:
		break;
	}
	return MiniMapIcon::None;
}

int CHUDRadar::FillUpDoubleArray(std::vector<double>* doubleArray, double a, double b, double c, double d, double e, double f, double g, double h, double i, double j, double k)
{
	doubleArray->push_back(a);
	doubleArray->push_back(b);
	doubleArray->push_back(c);
	doubleArray->push_back(d);
	doubleArray->push_back(e);
	doubleArray->push_back(f);
	doubleArray->push_back(g);
	doubleArray->push_back(h);
	doubleArray->push_back(i);
	doubleArray->push_back(j);
	doubleArray->push_back(k);

	return 11;
}

void CHUDRadar::StartBroadScan(bool useParameters, bool keepEntities, Vec3 pos, float radius)
{
	if (!m_startBroadScanTime) //wait (1.5) seconds before actually scanning - delete timer after scan 
	{
		m_startBroadScanTime = gEnv->pTimer->GetFrameStartTime().GetSeconds();
		//set scanning parameter
		m_bsUseParameter = useParameters;
		m_bsKeepEntries = keepEntities;
		m_bsPosition = pos;
		m_bsRadius = radius;

		return;
	}
	else if (m_bsUseParameter)	//this is a custom scan
	{
		ScanProximity(m_bsPosition, m_bsRadius);
		if (m_bsKeepEntries)
			m_pHUD->ShowDownloadSequence();
	}
	else	//this is a quick proximity scan
		m_bsKeepEntries = false;

	int playerTeam = m_pGameRules->GetTeam(m_pClientActor->GetEntityId());

	for (const EntityId id : m_entitiesInProximity)
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id);
		if (IsOnRadar(id, 0, m_entitiesOnRadar.size() - 1) || IsEntityTagged(id))
			continue;
		if (stl::find(m_teamMates, id))
			continue;

		m_pGameRules->AddTaggedEntity(m_pClientActor->GetEntityId(), id, m_bsKeepEntries ? false : true);
	}
	m_startBroadScanTime = 0.0f;
	m_bsKeepEntries = false;
}

void CHUDRadar::GetMemoryStatistics(ICrySizer* s)
{
	s->Add(*this);
	s->AddContainer(m_scannerQueue);
	s->AddContainer(m_teamMates);
	s->AddContainer(m_entitiesInProximity);
	s->AddContainer(m_itemsInProximity);
	s->AddContainer(m_tempEntitiesOnRadar);
	s->AddContainer(m_storyEntitiesOnRadar);
	s->AddContainer(m_entitiesOnRadar);
	s->AddContainer(m_soundsOnRadar);
	s->AddContainer(m_buildingsOnRadar);
	s->AddContainer(m_missionObjectives);
	for (std::map<EntityId, RadarObjective>::iterator iter = m_missionObjectives.begin(); iter != m_missionObjectives.end(); ++iter)
		s->Add(iter->second.text);
}

void CHUDRadar::SetTeamMate(EntityId id, bool active)
{
	const std::vector<EntityId>::iterator it = std::find(m_teamMates.begin(), m_teamMates.end(), id);

	if (active)
	{
		if (it != m_teamMates.end())
			return;

		if (IActor* pActor = m_pActorSystem->GetActor(id))
		{
			m_teamMates.push_back(id);
		}
		return;
	}

	if (it != m_teamMates.end())
	{
		std::erase(m_teamMates, id);
	}
}

void CHUDRadar::SelectTeamMate(EntityId id, bool active)
{
	if (active)
	{
		if (!IsTeamMateSelected(id))
		{
			if (m_pActorSystem->GetActor(id))
			{
				m_selectedTeamMates.push_back(id);
			}
		}
		return;
	}

	std::erase(m_selectedTeamMates, id);
}

bool CHUDRadar::IsTeamMateSelected(EntityId id) const noexcept
{
	return std::find(m_selectedTeamMates.begin(),
		m_selectedTeamMates.end(), id)
		!= m_selectedTeamMates.end();
}

void CHUDRadar::ScanProximity(Vec3& pos, float& radius)
{
	m_entitiesInProximity.clear();
	m_itemsInProximity.clear();

	SEntityProximityQuery query;
	query.box = AABB(Vec3(pos.x - radius, pos.y - (radius * 1.33f), pos.z - radius),
		Vec3(pos.x + radius, pos.y + (radius * 1.33f), pos.z + radius));
	query.nEntityFlags = ENTITY_FLAG_ON_RADAR; // Filter by entity flag.
	gEnv->pEntitySystem->QueryProximity(query);

	IEntity* pActorEntity = m_pClientActor->GetEntity();

	for (int iEntity = 0; iEntity < query.nCount; iEntity++)
	{
		IEntity* pEntity = query.pEntities[iEntity];
		if (pEntity && !pEntity->IsHidden())
		{
			EntityId id = pEntity->GetId();
			if (g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(id))
				m_itemsInProximity.push_back(id);
			else
				m_entitiesInProximity.push_back(id);	//create a list of all nearby entities
		}
	}
}

EntityId CHUDRadar::RayCastBinoculars(CPlayer* pPlayer, ray_hit* pRayHit)
{
	if (!pPlayer)
		return 0;

	Vec3 pos, dir, up, right;

	const Matrix34& rmatCamera = gEnv->pSystem->GetViewCamera().GetMatrix();
	pos = rmatCamera.GetTranslation();
	dir = rmatCamera.GetColumn1();
	up = rmatCamera.GetColumn2();
	right = (dir % up).GetNormalizedSafe();

	IPhysicalEntity* pPhysEnt = pPlayer->GetEntity()->GetPhysics();

	if (!pPhysEnt)
		return 0;

	static const int obj_types = ent_all;
	static const unsigned int flags = rwi_pierceability(1);

	right = right * g_pGameCVars->hud_binocsScanningWidth;
	up = up * g_pGameCVars->hud_binocsScanningWidth;

	Vec3 positions[4];
	positions[0] = pos + right + up;
	positions[1] = pos + right - up;
	positions[2] = pos - right - up;
	positions[3] = pos - right + up;
	for (int i = 0; i < 4; ++i)
	{
		memset(pRayHit, 0, sizeof(ray_hit));
		if (gEnv->pPhysicalWorld->RayWorldIntersection(positions[i], 300.0f * dir, obj_types, (13 & rwi_pierceability_mask), pRayHit, 1, &pPhysEnt, 1))
		{
			IEntity* pLookAt = gEnv->pEntitySystem->GetEntityFromPhysics(pRayHit->pCollider);

			if (pLookAt)
				return pLookAt->GetId();
		}
	}
	return 0;
}

bool CHUDRadar::IsEntityTagged(const EntityId& id) const
{
	for (int i = 0; i < NUM_TAGGED_ENTITIES; ++i)
		if (m_taggedEntities[i] == id)
			return true;
	return false;
}

void CHUDRadar::SetJammer(EntityId id, float radius)
{
	m_jammerID = id;
	m_jammerRadius = radius;

	if (!id && m_jammingValue)
	{
		m_jammingValue = 0.0f;
		m_flashRadar->Invoke("setNoiseValue", 0.0f);
	}
}

void CHUDRadar::SetMiniMapTexture(int mapId, bool forceUpdate)
{
	if (mapId < 0 || mapId >= NUM_MAP_TEXTURES)
	{
		CryLogWarning("Failed loading map texture id %i. (out of id space, current maximum are %i)", mapId, NUM_MAP_TEXTURES);
		return;
	}

	string* mapFile = &(m_mapFile[mapId]);

	if (mapFile && mapFile->size() > 0)
	{
		if (mapId != m_mapId || forceUpdate)
		{
			m_mapId = mapId;
			ComputeMiniMapResolution();

			m_flashPDA->Invoke("setMapBackground", mapFile->c_str());
			m_flashRadar->Invoke("setMapBackground", mapFile->c_str());
		}

	}
	else
		CryLogWarning("Failed loading map texture id %i.", mapId);
}

bool CHUDRadar::RadarBounds_Inside(const Vec2& pos, Vec2& intersectionPoint)
{
	Vec2 center(m_fX, m_fY);
	static const Vec2 radarPolygon[] =
	{
		Vec2(center + Vec2(-58, 17.0f)),
		Vec2(center + Vec2(-58, -42)),
		Vec2(center + Vec2(62, -42)),
		Vec2(center + Vec2(62, 32.15f)),
		Vec2(center + Vec2(46.25f, 45)),
		Vec2(center + Vec2(-46.75f, 45)),
		Vec2(center + Vec2(-46.75f, 25.8f))
	};

	int a, b;
	for (a = 0, b = 1; a < 7; ++a, ++b)
	{
		if (b == 7)
			b = 0;

		bool intersection = RadarBounds_IntersectsLineFromOutside(radarPolygon[a], radarPolygon[b], pos, center, intersectionPoint);
		if (intersection)
		{
			return false;
		}
	}
	return true;
}

bool CHUDRadar::RadarBounds_IntersectsLineFromOutside(const Vec2& A, const Vec2& B, const Vec2& P, const Vec2& M, Vec2& resultingIntersection)
{
	Vec2 line = B - A; //k
	Vec2 lineNormal = Vec2(-line.y, line.x);
	Vec2 faceLine = A - P;

	bool intersects = false;

	float dot = lineNormal.Dot(faceLine);
	if (dot > 0) //it's possibly crossing, try to compute the intersection
	{
		Vec2 crossingLine = (P - M); //t

		if (line.x == 0)
		{
			float i2 = crossingLine.y / crossingLine.x;
			float r2 = M.y - ((M.x * crossingLine.y) / crossingLine.x); //p
			resultingIntersection.x = A.x;
			resultingIntersection.y = i2 * resultingIntersection.x + r2;
		}
		else if (crossingLine.x == 0)
		{
			float i1 = line.y / line.x;
			float r1 = A.y - ((A.x * line.y) / line.x); //b
			resultingIntersection.x = M.x;
			resultingIntersection.y = i1 * resultingIntersection.x + r1;
		}
		else
		{
			float i1 = line.y / line.x;	//gradients m
			float i2 = crossingLine.y / crossingLine.x; //n
			float r1 = A.y - ((A.x * line.y) / line.x); //b
			float r2 = M.y - ((M.x * crossingLine.y) / crossingLine.x); //p
			resultingIntersection.x = (r1 - r2) / (i2 - i1);
			resultingIntersection.y = i1 * resultingIntersection.x + r1;
		}

		float onLine = -1;
		if (line.y != 0)
			onLine = (resultingIntersection.y - A.y) / line.y;
		else if (line.x != 0)
			onLine = (resultingIntersection.x - A.x) / line.x;

		if (onLine > 0 && onLine < 1.0f)
			intersects = true;
	}
	return intersects;
}

void CHUDRadar::UpdateBinoculars(CActor* pActor, float fDeltaTime)
{
	CHUDScopes* pScopes = m_pHUD->GetScopes();
	if (pScopes && pScopes->IsBinocularsShown())
	{
		EntityId lookAtObjectID = pActor->GetGameObject()->GetWorldQuery()->GetLookAtEntityId();
		ray_hit rayHit;
		if (!lookAtObjectID)
		{
			CPlayer* pPlayer = static_cast<CPlayer*> (pActor);
			lookAtObjectID = RayCastBinoculars(pPlayer, &rayHit);
		}

		// When in a cinematic, pActor view doesn't move (it's faked), so let's not use the lookAtEntityId
		if (lookAtObjectID && !m_pHUD->IsInCinematic())
		{
			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(lookAtObjectID);
			if (pEntity)
			{
				float fDistance = (pEntity->GetWorldPos() - pActor->GetEntity()->GetWorldPos()).GetLength();
				pScopes->SetBinocularsDistance(fDistance);
			}
		}
		else if (rayHit.pCollider)
		{
			pScopes->SetBinocularsDistance(rayHit.dist);
		}
		else
		{
			pScopes->SetBinocularsDistance(0);
		}

		if (lookAtObjectID && lookAtObjectID != m_lookAtObjectID)
		{
			// DevTrack 17224: set the delay to a very small value
			m_lookAtTimer = 0.05f;
			m_lookAtObjectID = lookAtObjectID;
		}
		else if (lookAtObjectID)
		{
			if (m_lookAtTimer > 0.0f)
			{
				m_lookAtTimer -= fDeltaTime;
				if (m_lookAtTimer <= 0.0f)
				{
					m_lookAtTimer = 0.0f;

					if (gEnv->bMultiplayer)
					{
						bool add = CheckObjectMultiplayer(lookAtObjectID);

						if (add)
							m_scannerQueue.push_front(lookAtObjectID);
					}
					else
					{
						if (!IsOnRadar(lookAtObjectID, 0, m_entitiesOnRadar.size() - 1) && !IsNextObject(lookAtObjectID))
							m_scannerQueue.push_front(lookAtObjectID);
					}
				}
			}
		}
		else
			m_lookAtObjectID = 0;

		UpdateScanner(fDeltaTime);
	}
	else
		ResetScanner();
}

bool CHUDRadar::CheckObjectMultiplayer(EntityId id)
{
	if (!gEnv->bMultiplayer)
		return true;

	// MP binoculars only add entities temporarily. Don't add if they are already there.
	for (const auto &m : m_tempEntitiesOnRadar)
	{
		if (id == m.m_id)
		{
			return false;
		}
	}

	// also, don't allow tagging of unoccupied vehicles
	if (IVehicle* pVehicle = m_pVehicleSystem->GetVehicle(id))
	{
		//CryMP 18.07.22 Allow tagging all vehicles 

		//if (!pVehicle->IsPlayerDriving(false)) //CryMP: False was missing, now you can tag vehicles with drivers
		//	return false;
	}

	// don't tag dead players
	if (CActor* pActor = static_cast<CActor*>(m_pActorSystem->GetActor(id)))
	{
		if (pActor->GetHealth() <= 0)
			return false;
	}

	return true;
}
