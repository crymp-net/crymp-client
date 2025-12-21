// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#include "CryCommon/CrySystem/ISystem.h"
#include "CryGame/Actors/Player/IPlayerInput.h"
#include "HUD.h"
#include "CryGame/Items/Weapons/Weapon.h"
#include "CryGame/HUD/GameFlashLogic.h"

//-----------------------------------------------------------------------------------------------------

void CHUD::LoadWeaponAccessories(XmlNodeRef weaponXmlNode)
{
	if (weaponXmlNode)
	{
		TMapWeaponAccessoriesHelperOffsets mapWeaponAccessoriesHelpersOffsets;

		int iNumChildren = weaponXmlNode->getChildCount();
		for (int iChild = 0; iChild < iNumChildren; iChild++)
		{
			XmlNodeRef helperXmlNode = weaponXmlNode->getChild(iChild);
			SWeaponAccessoriesHelpersOffsets offsets;
			offsets.iX = atoi(helperXmlNode->getAttr("X"));
			offsets.iY = atoi(helperXmlNode->getAttr("Y"));
			mapWeaponAccessoriesHelpersOffsets.insert(std::make_pair(helperXmlNode->getTag(), offsets));
		}

		m_mapWeaponAccessoriesHelpersOffsets.insert(std::make_pair(weaponXmlNode->getTag(), mapWeaponAccessoriesHelpersOffsets));
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::LoadWeaponsAccessories()
{
	XmlNodeRef weaponAccessoriesXmlNode = GetISystem()->LoadXmlFile("Libs/UI/WeaponAccessories.xml");
	if (weaponAccessoriesXmlNode)
	{
		int iNumChildren = weaponAccessoriesXmlNode->getChildCount();
		for (int iChild = 0; iChild < iNumChildren; iChild++)
		{
			LoadWeaponAccessories(weaponAccessoriesXmlNode->getChild(iChild));
		}
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::AdjustWeaponAccessory(const char* szWeapon, const char* szHelper, Vec3* pvScreenSpace)
{
	std::map<string, TMapWeaponAccessoriesHelperOffsets>::iterator iter1 = m_mapWeaponAccessoriesHelpersOffsets.find(szWeapon);
	if (iter1 != m_mapWeaponAccessoriesHelpersOffsets.end())
	{
		TMapWeaponAccessoriesHelperOffsets::iterator iter2 = (*iter1).second.find(szHelper);
		if (iter2 != (*iter1).second.end())
		{
			pvScreenSpace->x += ((*iter2).second).iX;
			pvScreenSpace->y += ((*iter2).second).iY;
		}
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::ClearWeaponAccessoriesScreen()
{
	m_animWeaponAccessories.Invoke("clearAllSlotButtons");
	m_animWeaponAccessories.Invoke("hideWeaponAccessories");
}

//-----------------------------------------------------------------------------------------------------

void CHUD::WeaponAccessoriesInterface(bool visible, bool force)
{
	if (visible)
	{
		this->ShowMouseCursor(true);
	}
	else
	{
		ClearWeaponAccessoriesScreen();
	}

	m_acceptNextWeaponCommand = true;
	if (visible)
	{
		m_bIgnoreMiddleClick = false;
		if (WeaponHasAttachments())
		{
			m_animWeaponAccessories.Invoke("showWeaponAccessories"); //CryMP: makes the menu appear
			//m_animWeaponAccessories.SetVisible(true); //already set to visible when pressing C
		}
	}
	else
	{
		if (!force)
		{
			if (m_animWeaponAccessories.GetVisible())
			{
				m_bIgnoreMiddleClick = true;
				m_animWeaponAccessories.Invoke("hideWeaponAccessories");
				m_animWeaponAccessories.SetVisible(false);
				PlaySound(ESound_SuitMenuDisappear);
			}
		}
		else
		{
			if (&m_animWeaponAccessories == m_pModalHUD)
				SwitchToModalHUD(NULL, false);

			m_animWeaponAccessories.Invoke("hideWeaponAccessories");
			m_animWeaponAccessories.SetVisible(false);
		}
	}
}

//-----------------------------------------------------------------------------------------------------

bool CHUD::ShowWeaponAccessories(bool enable)
{
	CWeapon* pWeapon = GetCurrentWeapon();
	if (!pWeapon)
		return false;

	IPlayerInput* pPlayerInput = m_pClientActor->GetPlayerInput();

	m_bWeaponModifyOpen = enable;

	if (enable)
	{
		CItem* pOffhand = m_pClientActor->GetItemByClass(CItem::sOffHandClass);
		if (pOffhand && pOffhand->IsSelected())
		{
			return false;
		}

		const bool busy = pWeapon->IsBusy();
		const bool modify = pWeapon->IsModifying();
		const bool zooming = pWeapon->IsZooming();
		const bool switchFM = pWeapon->IsSwitchingFireMode();
		const bool reloading = pWeapon->IsReloading();
		const bool sprinting = m_pClientActor->IsSprinting();
		const bool isTargeting = pWeapon->IsTargetOn();

		if (!busy && !reloading && /*!modify &&*/ !zooming && !switchFM && !sprinting && !isTargeting)
		{
			if (!m_pClientActor->GetActorStats()->isFrozen.Value())
			{
				if (UpdateWeaponAccessoriesScreen())
				{
					m_animWeaponAccessories.SetVisible(true); //CryMP: adds the buttons for fast switch, but doesn't show the menu untill later

					//CryMP: Changed to false, showing mouse pointer only when menu available
					SwitchToModalHUD(&m_animWeaponAccessories, false); 

					if (g_pGameActions && g_pGameActions->FilterNoMouse())
					{
						//CryMP: Still need to call this to prevent changing weapons while opening weapon customization
						g_pGameActions->FilterNoMouse()->Enable(true); 
					}

					if (pPlayerInput)
					{
						pPlayerInput->DisableXI(true);
					}

					m_acceptNextWeaponCommand = false;
					pWeapon->OnAction(m_pClientActor->GetEntityId(), ActionId("modify"), 0, 1);
				}
			}
		}
	}
	else
	{
		if (!m_pClientActor->GetPlayerStats()->bSprinting)
		{
			SwitchToModalHUD(NULL, false);
			if (pPlayerInput)
			{
				pPlayerInput->DisableXI(false);
			}
			m_acceptNextWeaponCommand = false;
			if (!gEnv->pSystem->IsSerializingFile())
			{
				pWeapon->OnAction(m_pClientActor->GetEntityId(), ActionId("modify"), 0, 1);
			}
			else
			{
				WeaponAccessoriesInterface(false, true);
			}

			if (g_pGameActions && g_pGameActions->FilterNoMouse() && g_pGameActions->FilterNoMouse()->Enabled())
			{
				g_pGameActions->FilterNoMouse()->Enable(false); //CryMP: Disable if necessary
			}
		}
	}
	return true;
}

bool CHUD::UpdateWeaponAccessoriesScreen()
{
	bool hasAttachments = false;

	if (CWeapon* pCurrentWeapon = GetCurrentWeapon())
	{
		const auto& helpers = pCurrentWeapon->GetAttachmentHelpers();
		for (const auto& helper : helpers)
		{
			if (helper.slot != CItem::eIGS_FirstPerson)
				continue;

			if (!pCurrentWeapon->HasAttachmentAtHelper(helper.name.c_str()))
				continue;

			const std::vector<std::string> attachments = pCurrentWeapon->GetAttachmentsAtHelper(helper.name.c_str());
			if (attachments.empty())
				continue;

			const bool isSpecial = helper.name == "magazine" ||
				helper.name == "attachment_front" ||
				helper.name == "energy_source_helper" ||
				helper.name == "shell_grenade";

			int potentialButtons = 0;

			if (!isSpecial)
			{
				++potentialButtons; // NoAttachment
			}

			for (const auto& attachment : attachments)
			{
				if (attachment != "GrenadeShell")
				{
					++potentialButtons;
				}
			}
			
			if (potentialButtons < 2)
				continue;

			SFlashVarValue args1[3] = { helper.name.c_str(), "", "" };
			m_animWeaponAccessories.Invoke("addSlotButton", args1, 3);

			std::string control = "Root.slot_" + std::string(helper.name.c_str());
			std::string token = "hud.WS" + std::string(helper.name.c_str());
			m_animWeaponAccessories.AddVariable(control.c_str(), "_x", (token + "X").c_str(), 1.0f, 0.0f);
			m_animWeaponAccessories.AddVariable(control.c_str(), "_y", (token + "Y").c_str(), 1.0f, 0.0f);

			std::string curAttach;
			if (const char* szCurAttach = pCurrentWeapon->CurrentAttachment(helper.name))
				curAttach = szCurAttach;

			int selectedIndex = 0;
			int count = 0;

			if (!isSpecial)
			{
				bool isTop = (helper.name == "attachment_top");
				SFlashVarValue args2[3] = {
					helper.name.c_str(),
					isTop ? "@IronSight" : "@NoAttachment",
					"NoAttachment"
				};
				m_animWeaponAccessories.Invoke("addSlotButton", args2, 3);
				++count;
			}

			for (const auto& attachment : attachments)
			{
				if (attachment == "GrenadeShell")
					continue;

				std::string displayName = "@" + attachment;
				SFlashVarValue args3[3] = { helper.name.c_str(), displayName.c_str(), attachment.c_str() };
				m_animWeaponAccessories.Invoke("addSlotButton", args3, 3);

				if (curAttach == attachment)
					selectedIndex = count;

				hasAttachments = true;
				++count;
			}

			SFlashVarValue selectArgs[2] = { helper.name.c_str(), selectedIndex };
			m_animWeaponAccessories.Invoke("selectSlotButton", selectArgs, 2);
		}
	}

	return hasAttachments;
}

//-----------------------------------------------------------------------------------------------------
void CHUD::UpdateWeaponModify()
{
	if (!m_bWeaponModifyOpen)
		return;

	CWeapon* pCurrentWeapon = GetCurrentWeapon();
	if (!pCurrentWeapon || !pCurrentWeapon->IsModifying())
		return;

	IGameTokenSystem* pGameTokenSystem = gEnv->pGame->GetIGameFramework()->GetIGameTokenSystem();
	if (!pGameTokenSystem)
		return;

	const bool thirdPerson = m_pClientActor->IsThirdPerson();
	const auto& helpers = pCurrentWeapon->GetAttachmentHelpers();

	float scaleX = 0.0f, scaleY = 0.0f, halfUselessSize = 0.0f;
	GetProjectionScale(&m_animWeaponAccessories, &scaleX, &scaleY, &halfUselessSize);

	for (const auto& helper : helpers)
	{
		if (helper.slot != CItem::eIGS_FirstPerson)
			continue;

		Vec3 worldPos = Vec3(ZERO);

		if (thirdPerson)
		{
			if (SEntitySlotInfo info; pCurrentWeapon->GetEntity()->GetSlotInfo(CItem::eIGS_ThirdPerson, info))
			{
				if (info.pStatObj)
				{
					worldPos = info.pStatObj->GetHelperPos(helper.name.c_str());
					worldPos = pCurrentWeapon->GetEntity()->GetSlotLocalTM(CItem::eIGS_ThirdPerson, false).TransformPoint(worldPos);
				}

				worldPos = pCurrentWeapon->GetEntity()->GetWorldTM().TransformPoint(worldPos);
			}

			if (worldPos.IsZero())
				continue;
		}
		else
		{
			worldPos = pCurrentWeapon->GetSlotHelperPos(CItem::eIGS_FirstPerson, helper.bone.c_str(), true);
		}

		Vec3 screenSpace = Vec3(ZERO);
		m_pRenderer->ProjectToScreen(worldPos.x, worldPos.y, worldPos.z, &screenSpace.x, &screenSpace.y, &screenSpace.z);

		screenSpace.x = screenSpace.x * scaleX + halfUselessSize;
		screenSpace.y = screenSpace.y * scaleY;

		std::string tokenBase = "hud.WS" + std::string(helper.name.c_str());

		if (!thirdPerson)
		{
			AdjustWeaponAccessory(pCurrentWeapon->GetEntity()->GetClass()->GetName(), helper.name.c_str(), &screenSpace);
		}

		pGameTokenSystem->SetOrCreateToken((tokenBase + "X").c_str(), TFlowInputData(screenSpace.x, true));
		pGameTokenSystem->SetOrCreateToken((tokenBase + "Y").c_str(), TFlowInputData(screenSpace.y, true));
	}
}
