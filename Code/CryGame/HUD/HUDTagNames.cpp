/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:
	MP TagNames

-------------------------------------------------------------------------
History:
- 21:05:2007: Created by Julien Darre

*************************************************************************/

#include <algorithm>

#include "CryCommon/CrySystem/ISystem.h"
#include "CryGame/Actors/Actor.h"
#include "CryGame/GameCVars.h"
#include "GameFlashLogic.h"
#include "CryGame/GameRules.h"
#include "HUD.h"
#include "HUDRadar.h"
#include "HUDTagNames.h"
#include "CryCommon/CryAction/IUIDraw.h"

#include "Library/StringTools.h"
#include "CryMP/Client/HealthManager.h"

//-----------------------------------------------------------------------------------------------------

#define COLOR_DEAD		ColorF(0.4f,0.4f,0.4f)
#define COLOR_ENEMY		ColorF(0.9f,0.1f,0.1f)
#define COLOR_FRIEND	ColorF(0.0353f,0.6235f,0.9137f)
#define COLOR_TAGGED	ColorF(0.6f, 0.8f, 0.196078f)

#define COLOR_ARMOR     ColorF(0.175f, 0.425f, 0.5f, 1.0f)
#define COLOR_HEALTH    ColorF(0.35f, 0.475f, 0.2f, 1.0f)
#define COLOR_VEHICLE_HEALTH    ColorF(1.0f, 1.0f, 0.0f, 1.0f)

#define MAX_HEALTH_BARS 10

static wchar_t healthBars[MAX_HEALTH_BARS + 1][MAX_HEALTH_BARS + 1];

//-----------------------------------------------------------------------------------------------------

CHUDTagNames::CHUDTagNames(CHUD *pHUD)
{
	m_pHUD = pHUD;

	m_pUIDraw = gEnv->pGame->GetIGameFramework()->GetIUIDraw();

	m_pMPNamesFont = nullptr;

	if (gEnv->bMultiplayer)
	{
		m_pMPNamesFont = gEnv->pCryFont->NewFont("MPNames");
		m_pMPNamesFont->Load("fonts/hud.xml");
	}

	constexpr int RANK_COUNT = 8;

	// the default empty rank name at index 0
	m_rankNames.reserve(RANK_COUNT + 1);
	m_rankNames.resize(1);

	ILocalizationManager *pLocalizationManager = gEnv->pSystem->GetLocalizationManager();

	wstring rankName;

	for (int i = 1; i <= RANK_COUNT; i++)
	{
		if (pLocalizationManager->LocalizeLabel(StringTools::Format("@ui_short_rank_%d", i).c_str(), rankName))
		{
			m_rankNames.emplace_back(rankName.c_str(), rankName.length());
		}
	}

	for (int i = 0; i <= MAX_HEALTH_BARS; i++) {
		float progress = i / (float)MAX_HEALTH_BARS;
		wchar_t* row = healthBars[i];
		for (int j = 0; j < MAX_HEALTH_BARS; j++) {
			float now = j / (float)MAX_HEALTH_BARS;
			row[j] = now < progress ? (wchar_t)0x2588 : (wchar_t)0x2581;
		}
		row[MAX_HEALTH_BARS] = 0;
	}
}

//-----------------------------------------------------------------------------------------------------

CHUDTagNames::~CHUDTagNames()
{
	SAFE_RELEASE(m_pMPNamesFont);
}

//-----------------------------------------------------------------------------------------------------

const std::wstring& CHUDTagNames::GetPlayerRank(EntityId entityId)
{
	constexpr TSynchedKey RANK_KEY = 202;

	int rank = 0;
	m_pHUD->m_pGameRules->GetSynchedEntityValue(entityId, RANK_KEY, rank);

	if (rank < 0 || rank >= m_rankNames.size())
		rank = 0;

	return m_rankNames[rank];
}

//-----------------------------------------------------------------------------------------------------
// FIXME: an improvement would be to project on faces of the OBB, it should fix most of the Z fights
//-----------------------------------------------------------------------------------------------------

bool CHUDTagNames::ProjectOnSphere(Vec3& rvWorldPos, const AABB& rBBox)
{
	/*	SAuxGeomRenderFlags oldRF = gEnv->pRenderer->GetIRenderAuxGeom()->GetRenderFlags();
		SAuxGeomRenderFlags newRF;
		newRF.SetAlphaBlendMode(e_AlphaBlended);
		newRF.SetDepthWriteFlag(e_DepthWriteOff);
		gEnv->pRenderer->GetIRenderAuxGeom()->SetRenderFlags(newRF);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere((rBBox.min+rBBox.max)*0.5f,((rBBox.max-rBBox.min)*0.5f).len(),ColorB(255,0,255,64),true);
		gEnv->pRenderer->GetIRenderAuxGeom()->SetRenderFlags(oldRF);*/

	Vec3 vCamPos = gEnv->pSystem->GetViewCamera().GetPosition();

	Sphere sphere((rBBox.min + rBBox.max) * 0.5f, ((rBBox.max - rBBox.min) * 0.5f).len());

	// Project from helper position to campos
	Lineseg lineSeg(rvWorldPos, vCamPos);
	Vec3 v0, v1;
	if (Intersect::Lineseg_Sphere(lineSeg, sphere, v0, v1) > 1)
	{
		// A result is found, let's use it

		Vec3 vScreenSpace;
		gEnv->pRenderer->ProjectToScreen(v1.x, v1.y, v1.z, &vScreenSpace.x, &vScreenSpace.y, &vScreenSpace.z);

		if (vScreenSpace.z >= 0.0f)
		{
			rvWorldPos = v1;
		}
		else
		{
			return true;
		}
	}

	if ((vCamPos - sphere.center).len() <= sphere.radius)
	{
		// We are inside the sphere, we can't project on it, so let's draw on top
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------------------------------

bool CHUDTagNames::IsFriendlyToClient(EntityId uiEntityId)
{
	IActor* client = m_pHUD->m_pClientActor;
	CGameRules* pGameRules = m_pHUD->m_pGameRules;
	if (!client || !pGameRules)
		return false;

	// local player is always friendly to himself :)
	if (client->GetEntityId() == uiEntityId)
		return true;

	int playerTeam = pGameRules->GetTeam(client->GetEntityId());

	// if this actor is spectating, use the team of the player they are spectating instead...
	if (static_cast<CActor*>(client)->GetSpectatorMode() == CActor::eASM_Follow)
	{
		playerTeam = pGameRules->GetTeam(static_cast<CActor*>(client)->GetSpectatorTarget());
	}

	// Less than 2 teams means we are in a FFA based game.
	if (pGameRules->GetTeam(uiEntityId) == playerTeam && pGameRules->GetTeamCount() > 1)
		return true;
	return false;
}

//-----------------------------------------------------------------------------------------------------

void CHUDTagNames::AddEnemyTagName(EntityId uiEntityId)
{
	for (EnemyTag& tag : m_enemyTags)
	{
		if (tag.entityId == uiEntityId)
		{
			// Reset time
			tag.spawnTime = gEnv->pTimer->GetAsyncTime().GetSeconds();
			return;
		}
	}

	EnemyTag tag;
	tag.entityId = uiEntityId;
	tag.spawnTime = gEnv->pTimer->GetAsyncTime().GetSeconds();
	m_enemyTags.push_back(tag);
}

//-----------------------------------------------------------------------------------------------------

void CHUDTagNames::DrawTagName(IActor* pActor, bool bLocalVehicle)
{
	if (!pActor)
		return;

	if (!bLocalVehicle && pActor->GetLinkedVehicle())
		return;

	IEntity* pEntity = pActor->GetEntity();
	EntityId entityId = pActor->GetEntityId();
	Vec3 vWorldPos = pEntity->GetWorldPos();

	ICharacterInstance* pCharacterInstance = pEntity->GetCharacter(0);
	if (pCharacterInstance)
	{
		ISkeletonPose* pSkeletonPose = pCharacterInstance->GetISkeletonPose();
		if (pSkeletonPose)
		{
			int16 sHeadID = pSkeletonPose->GetJointIDByName("Bip01 Head");
			if (sHeadID != -1)
			{
				Matrix34 matWorld = pEntity->GetWorldTM() * Matrix34(pSkeletonPose->GetAbsJointByID(sHeadID));

				vWorldPos = matWorld.GetTranslation();
			}
		}
	}

	// Who has a bigger head? :)
	vWorldPos.z += 0.4f;

	AABB box;
	pEntity->GetWorldBounds(box);

	bool bDrawOnTop = bLocalVehicle;

	if (ProjectOnSphere(vWorldPos, box))
	{
		bDrawOnTop = true;
	}

	ColorF tagColor = COLOR_ENEMY;

	CPlayer* pClientActor = m_pHUD->m_pClientActor;
	CGameRules* pGameRules = m_pHUD->m_pGameRules;
	const int iClientTeam = pGameRules->GetTeam(pClientActor->GetEntityId());

	bool friendly = false;

	if (0 == iClientTeam)
	{
		if (IsFriendlyToClient(entityId))
		{
			tagColor = COLOR_FRIEND;
			friendly = true;
		}
	}
	else if (pGameRules->GetTeam(entityId) == iClientTeam)
	{
		tagColor = COLOR_FRIEND;
		friendly = true;
	}

	if (pActor->GetHealth() <= 0)
	{
		tagColor = COLOR_DEAD;
	}

	for (EntityId teamMateId : *m_pHUD->GetRadar()->GetSelectedTeamMates())
	{
		if (entityId == teamMateId)
		{
			// Teammate is selected in radar, force the visibility of that name
			bDrawOnTop = true;

			tagColor = COLOR_TAGGED;

			break;
		}
	}

	//CryMP: Always draw killer tag
	const EntityId killerId = pClientActor->GetDeathCamTarget();
	if (killerId == entityId && pClientActor->GetPhysicsProfile() == eAP_Ragdoll)
	{
		bDrawOnTop = true;
	}

	NameTag& nameTag = m_nameTags.emplace_back();
	FillNameTag(nameTag, entityId, pActor, pEntity, bDrawOnTop, vWorldPos, tagColor, friendly);
}

//-----------------------------------------------------------------------------------------------------

void CHUDTagNames::DrawTagName(IVehicle* pVehicle)
{
	if (!pVehicle)
		return;

	CPlayer* pClientActor = m_pHUD->m_pClientActor;
	CGameRules* pGameRules = g_pGame->GetGameRules();
	int iClientTeam = pGameRules->GetTeam(pClientActor->GetEntityId());
	bool bThirdPerson = pClientActor->IsThirdPerson();

	bool bDrawSeatTagNames = false;
	if (pClientActor->GetSpectatorMode() == CActor::eASM_Follow)
	{
		IActor* pFollowedActor = pClientActor->GetSpectatorTargetActor();
		if (pFollowedActor)
			bDrawSeatTagNames = (pVehicle == pFollowedActor->GetLinkedVehicle());
	}
	else
		bDrawSeatTagNames = (pVehicle == pClientActor->GetLinkedVehicle());

	if (bDrawSeatTagNames)
	{
		// When this is local player vehicle, we always display all passengers name above their head so that he can identify them

		for (int iSeatId = 1; iSeatId <= pVehicle->GetLastSeatId(); iSeatId++)
		{
			IVehicleSeat* pVehicleSeat = pVehicle->GetSeatById(iSeatId);
			if (!pVehicleSeat)
				continue;

			IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pVehicleSeat->GetPassenger());
			if (!pActor || (pActor == pClientActor && !bThirdPerson))
				continue;

			DrawTagName(pActor, true);
		}

		return;
	}

	ColorF rgbTagName = COLOR_ENEMY;

	// Driver seat is always 1
	IVehicleSeat* pVehicleSeat = pVehicle->GetSeatById(1);
	if (!pVehicleSeat)
		return;

	IVehicleHelper* pVehicleHelper = pVehicleSeat->GetSitHelper();
	if (!pVehicleHelper)
		return;

	Vec3 vWorldPos = pVehicleHelper->GetWorldTM().GetTranslation();

	// Add some offset to be above the driver/pilot
	vWorldPos.z += 1.2f;

	AABB box;
	pVehicle->GetEntity()->GetWorldBounds(box);

	bool bDrawOnTop = false;

	if (ProjectOnSphere(vWorldPos, box))
	{
		bDrawOnTop = true;
	}

	for (int iSeatId = 1; iSeatId <= pVehicle->GetLastSeatId(); iSeatId++)
	{
		IVehicleSeat* pVehicleSeat = pVehicle->GetSeatById(iSeatId);
		if (!pVehicleSeat)
			continue;

		EntityId uiEntityId = pVehicleSeat->GetPassenger();

		IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(uiEntityId);
		if (!pActor)
			continue;

		IEntity* pEntity = pActor->GetEntity();
		if (!pEntity)
			continue;

		bool friendly = false;
		if (0 == iClientTeam)
		{
			if (uiEntityId && IsFriendlyToClient(uiEntityId))
			{
				rgbTagName = COLOR_FRIEND;
				friendly = true;
			}
		}
		else if (uiEntityId && pGameRules->GetTeam(uiEntityId) == iClientTeam)
		{
			rgbTagName = COLOR_FRIEND;
			friendly = true;
		}

		if (pActor->GetHealth() <= 0)
		{
			rgbTagName = COLOR_DEAD;
		}

		//CryMP: Always draw killer tag
		const EntityId killerId = pClientActor->GetDeathCamTarget();
		if (killerId == pActor->GetEntityId() && pClientActor->GetPhysicsProfile() == eAP_Ragdoll)
		{
			bDrawOnTop = true;
		}

		NameTag& nameTag = m_nameTags.emplace_back();
		FillNameTag(nameTag, uiEntityId, pActor, pEntity, bDrawOnTop, vWorldPos, rgbTagName, friendly);
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDTagNames::Update()
{
	IActorSystem* pActorSystem = g_pGame->GetIGameFramework()->GetIActorSystem();
	CPlayer* pClientActor = m_pHUD->m_pClientActor;
	CGameRules* pGameRules = g_pGame->GetGameRules();

	if (!pGameRules || !gEnv->bMultiplayer)
	{
		return;
	}

	m_nameTags.clear();

	const int clientTeamId = pGameRules->GetTeam(pClientActor->GetEntityId());

	// previous approach didn't work in IA as there are no teams.
	IActorIteratorPtr it = pActorSystem->CreateActorIterator();
	while (IActor* pActor = it->Next())
	{
		//CryMP: Display tag name in third person mode
		if (pActor->IsClient() && (!gEnv->bMultiplayer || !pActor->IsThirdPerson()))
			continue;

		const EntityId killerId = pClientActor->GetDeathCamTarget();
		if (killerId == pActor->GetEntityId() && pClientActor->GetPhysicsProfile() == eAP_Ragdoll)
		{
			IVehicle* pKillerVehicle = pActor->GetLinkedVehicle();
			if (pKillerVehicle)
			{
				DrawTagName(pKillerVehicle);
			}
			else
			{
				DrawTagName(pActor);
			}
			continue;
		}
		// Skip enemies, they need to be added only when shot
		// (except in spectator mode when we display everyone)
		const int teamId = pGameRules->GetTeam(pActor->GetEntityId());
		if ((pActor->IsClient() || (teamId == clientTeamId && teamId != 0)) || (pClientActor->GetSpectatorMode() != CActor::eASM_None))
		{
			// never display other spectators
			if (static_cast<CActor*>(pActor)->GetSpectatorMode() != CActor::eASM_None)
				continue;

			// never display the name of the player we're spectating (it's shown separately with their current health)
			if (pClientActor->GetSpectatorMode() == CActor::eASM_Follow && pClientActor->GetSpectatorTarget() == pActor->GetEntityId())
				continue;

			DrawTagName(pActor);
		}
	}

	IVehicleSystem* pVehicleSystem = gEnv->pGame->GetIGameFramework()->GetIVehicleSystem();
	if (!pVehicleSystem)
	{
		DrawTagNames();
		return;
	}

	IVehicleIteratorPtr pVehicleIter = pVehicleSystem->CreateVehicleIterator();
	while (IVehicle* pVehicle = pVehicleIter->Next())
	{
		SVehicleStatus rVehicleStatus = pVehicle->GetStatus();
		if (0 == rVehicleStatus.passengerCount)
			continue;

		// Skip enemy vehicles, they need to be added only when shot (except in spectator mode...)
		bool bEnemyVehicle = true;
		for (int iSeatId = 1; iSeatId <= pVehicle->GetLastSeatId(); iSeatId++)
		{
			IVehicleSeat* pVehicleSeat = pVehicle->GetSeatById(iSeatId);
			if (!pVehicleSeat)
				continue;

			EntityId uiEntityId = pVehicleSeat->GetPassenger();

			if (0 == clientTeamId)
			{
				if (uiEntityId && IsFriendlyToClient(uiEntityId))
				{
					bEnemyVehicle = false;
				}
			}
			else if (uiEntityId && pGameRules->GetTeam(uiEntityId) == clientTeamId)
			{
				bEnemyVehicle = false;
			}
		}
		if (bEnemyVehicle && (pClientActor->GetSpectatorMode() == CActor::eASM_None))	// again, draw enemies in spectator mode
			continue;

		DrawTagName(pVehicle);
	}

	// don't need to do any of this if we're in spectator mode - all player names will have been drawn above.
	if (pClientActor->GetSpectatorMode() == CActor::eASM_None)
	{
		const float currentTime = gEnv->pTimer->GetAsyncTime().GetSeconds();
		const float tagDuration = g_pGameCVars->hud_mpNamesDuration;

		const auto expired = [&currentTime, &tagDuration](EnemyTag& tag) -> bool
		{
			const float tagEndTime = tag.spawnTime + tagDuration;

			return currentTime >= tagEndTime;
		};

		m_enemyTags.erase(std::remove_if(m_enemyTags.begin(), m_enemyTags.end(), expired), m_enemyTags.end());

		for (const EnemyTag& tag : m_enemyTags)
		{
			IActor* pActor = pActorSystem->GetActor(tag.entityId);
			if (pActor)
			{
				DrawTagName(pActor);
			}

			IVehicle* pVehicle = pVehicleSystem->GetVehicle(tag.entityId);
			if (pVehicle)
			{
				DrawTagName(pVehicle);
			}
		}
	}

	DrawTagNames();
}

//-----------------------------------------------------------------------------------------------------

void CHUDTagNames::DrawTagNames()
{
	for (const NameTag& nameTag : m_nameTags)
	{
		// It's important that the projection is done outside the UIDraw->PreRender/PostRender because of the Set2DMode(true) which is done internally

		Vec3 vScreenSpace;
		gEnv->pRenderer->ProjectToScreen(nameTag.pos.x, nameTag.pos.y, nameTag.pos.z, &vScreenSpace.x, &vScreenSpace.y, &vScreenSpace.z);

		if (vScreenSpace.z < 0.0f || vScreenSpace.z > 1.0f)
		{
			// Ignore out of screen entities
			continue;
		}

		vScreenSpace.x *= gEnv->pRenderer->GetWidth() * 0.01f;
		vScreenSpace.y *= gEnv->pRenderer->GetHeight() * 0.01f;

		// Seems that Z is on range [1 .. -1]
		vScreenSpace.z = 1.0f - (vScreenSpace.z * 2.0f);

		float fDistance = (nameTag.pos - gEnv->pSystem->GetViewCamera().GetPosition()).len();

		// Adjust distance when zoomed. Default fov is 60, so we use (1/(60*pi/180)=3/pi)
		fDistance *= 3.0f * gEnv->pRenderer->GetCamera().GetFov() / gf_PI;

		float fSize = 0.0f;
		float fAlpha = 0.0f;

		float fMinDistance = (float)g_pGameCVars->hud_mpNamesNearDistance;
		float fMaxDistance = (float)g_pGameCVars->hud_mpNamesFarDistance;

		// if local player is in a vehicle, increase the max distance
		if (m_pHUD->m_pClientActor->GetLinkedVehicle())
		{
			fMaxDistance *= 3.0f;
		}

		if (fDistance < fMinDistance)
		{
			fAlpha = 1.0f;
		}
		else if (fDistance < fMaxDistance)
		{
			fAlpha = 1.0f - (fDistance - fMinDistance) / (fMaxDistance - fMinDistance);
		}

		if (0.0f == fAlpha)
		{
			continue;
		}

		fAlpha = MIN(fAlpha, 0.8f);

		const float fBaseSize = 11.0f;

		if (fDistance < fMinDistance)
		{
			fSize = fBaseSize;
		}
		else if (fDistance < fMaxDistance)
		{
			fSize = fBaseSize * (1.0f - (fDistance - fMinDistance) / (fMaxDistance - fMinDistance));
		}

		m_pUIDraw->PreRender();

		float fScaleY = gEnv->pRenderer->GetHeight() / 600.0f;

		fSize *= fScaleY;

		m_pMPNamesFont->UseRealPixels(true);
		m_pMPNamesFont->SetSize(vector2f(fSize, fSize));
		m_pMPNamesFont->SetSameSize(false);

		const wchar_t* text = nameTag.text.c_str();

		vector2f vDim = m_pMPNamesFont->GetTextSizeW(text);

		float fTextX = vScreenSpace.x - vDim.x * 0.5f;
		float fTextY = vScreenSpace.y - vDim.y * 0.5f;

		if (nameTag.drawOnTop)
		{
			vScreenSpace.z = 1.0f;
		}

		if (nameTag.vehicle) {
			if (nameTag.healthBars) {
				DrawProgressBars(m_pMPNamesFont, vScreenSpace.x, vScreenSpace.y, 3.0f, vScreenSpace.z, nameTag.health, COLOR_VEHICLE_HEALTH);
			}
		} else {
			m_pMPNamesFont->SetEffect("simple");
			m_pMPNamesFont->SetColor(ColorF(0, 0, 0, fAlpha));
			m_pMPNamesFont->DrawStringW(fTextX + 1.0f, fTextY + 1.0f, vScreenSpace.z, text);

			m_pMPNamesFont->SetEffect("default");
			m_pMPNamesFont->SetColor(ColorF(nameTag.color.r, nameTag.color.g, nameTag.color.b, fAlpha));
			m_pMPNamesFont->DrawStringW(fTextX, fTextY, vScreenSpace.z, text);

			if (nameTag.healthBars) {
				DrawProgressBars(m_pMPNamesFont, vScreenSpace.x, vScreenSpace.y, 4.0f, vScreenSpace.z, nameTag.armor, COLOR_ARMOR);
				DrawProgressBars(m_pMPNamesFont, vScreenSpace.x, vScreenSpace.y, 3.0f, vScreenSpace.z, nameTag.health, COLOR_HEALTH);
			}
		}

		m_pUIDraw->PostRender();
	}
}

void CHUDTagNames::DrawProgressBars(IFFont *pFont, float centerX, float centerY, float marginY, float z, float progress, ColorF color) {
	int rowIndex = (int)round(progress * MAX_HEALTH_BARS);
	if (rowIndex > MAX_HEALTH_BARS) rowIndex = MAX_HEALTH_BARS;
	else if (rowIndex < 0) rowIndex = 0;

	vector2f size = pFont->GetSize();
	pFont->SetSize(vector2f(size.x, size.y * 0.25));

	wchar_t* row = healthBars[rowIndex];
	vector2f vDim = pFont->GetTextSizeW(row);
	float fTextX = centerX - vDim.x * (0.5f);
	float fTextY = centerY - vDim.y * (marginY * 1.2f + 0.5f);
	
	pFont->SetEffect("default");
	pFont->SetColor(color);
	pFont->DrawStringW(fTextX, fTextY, z, row);

	pFont->SetSize(size);
}

void CHUDTagNames::FillNameTag(
	NameTag& nameTag,
	EntityId entityId,
	IActor *iActor,
	IEntity *pEntity,
	bool bDrawOnTop,
	Vec3 vWorldPos,
	ColorF tagColor,
	bool friendly
) {
	CActor* pActor = static_cast<CActor*>(iActor);
	if(iActor != NULL && g_pGameCVars->mp_healthBars ) {
		nameTag.healthBars = /*!friendly &&*/ NeedsHealthBar(pActor);
		nameTag.vehicle = false;
		nameTag.armor = pActor->GetArmor() / (float)pActor->GetMaxArmor();
		if (nameTag.healthBars) {
			int health = g_pGame->GetHealthManager()->GetHealth(pActor);
			nameTag.health = health / (float)pActor->GetMaxHealth();
		} else {
			nameTag.health = 1.0f;
		}
	} else {
		nameTag.healthBars = false;
		nameTag.vehicle = false;
		nameTag.armor = 0.0f;
		nameTag.health = 1.0f;
	}

	if (const std::wstring& rank = GetPlayerRank(entityId); !rank.empty())
	{
		nameTag.text += rank;
		nameTag.text += L' ';
	}

	StringTools::AppendTo(nameTag.text, pEntity->GetName());

	nameTag.pos = vWorldPos;
	nameTag.drawOnTop = bDrawOnTop;
	nameTag.color = tagColor;
}


bool CHUDTagNames::NeedsHealthBar(IActor* iActor) {
	bool outcome = true;
	CActor* pActor = static_cast<CActor*>(iActor);

	if (!g_pGame->GetHealthManager()->IsActive()) {
		return false;
	}

	bool healthDamaged = g_pGame->GetHealthManager()->GetHealth(pActor) < pActor->GetMaxHealth();
	if (pActor->GetActorClass() == CPlayer::GetActorClassType()) {
		CPlayer* pPlayer = static_cast<CPlayer*>(pActor);
		CNanoSuit* pNanoSuit = pPlayer->GetNanoSuit();
		if (pNanoSuit && pNanoSuit->GetMode() == ENanoMode::NANOMODE_DEFENSE) {
			outcome = healthDamaged || pActor->GetArmor() < pActor->GetMaxArmor();
		} else {
			outcome = healthDamaged;
		}
	} else {
		outcome = healthDamaged;
	}
	return outcome;
}

bool CHUDTagNames::NeedsHealthBar(IVehicle* pVehicle) {
	return pVehicle->GetStatus().health < 1.0f;
}

//-----------------------------------------------------------------------------------------------------
