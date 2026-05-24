/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: PowerStruggle mode HUD (BuyMenu) code (refactored from old HUD code)

-------------------------------------------------------------------------
History:
- 21:02:20067  20:00 : Created by Jan Müller

*************************************************************************/

#ifndef HUD_POWER_STRUGGLE_H
#define HUD_POWER_STRUGGLE_H

# pragma once


#include "HUDObject.h"
#include "HUD.h"

class CGameFlashAnimation;

class CHUDPowerStruggle : public CHUDObject
{
	friend class CHUD;
public:

	CHUDPowerStruggle(CHUD *pHUD, CGameRules *pGameRules, CGameFlashAnimation *pBuyMenu, CGameFlashAnimation *pHexIcon);
	~CHUDPowerStruggle();

	void Update(float fDeltaTime);
	void UpdatePlayerPP();
	void UpdateSwingOMeter(float fDeltaTime);
	void UpdateHexIcons(float fDeltaTime);
	void Reset();

	enum ESOMHideReason
	{
		eSOMHideReason_None = 0,
		eSOMHideReason_PDA = BIT(0),
		eSOMHideReason_Binoculars = BIT(1),
	};

	//buyable item types
	enum BuyMenuPage
	{
		BUY_PAGE_NONE = 0,
		BUY_PAGE_WEAPONS = 1 << 0,
		BUY_PAGE_VEHICLES = 1 << 1,
		BUY_PAGE_EQUIPMENT = 1 << 2,
		BUY_PAGE_AMMO = 1 << 3,
		BUY_PAGE_PROTOTYPES = 1 << 4,
		BUY_PAGE_LOADOUT = 1 << 5
	};

	enum EHexIconState
	{
		E_HEX_ICON_NONE,
		E_HEX_ICON_EXPOSURE,
		E_HEX_ICON_CAPTURING,
		E_HEX_ICON_BUY,
		E_HEX_ICON_BUILDING,
		E_HEX_ICON_LAST
	};

	struct SItem
	{
		string strName;
		string strDesc;
		string strClass;
		string strCategory;
		int uniqueLoadoutGroup;
		int uniqueLoadoutCount;
		int iPrice;
		int isUnique;
		int iCount;
		int iMaxCount;
		int iInventoryID;
		float level;
		bool isWeapon;
		bool bAmmoType;
		bool bVehicleType;
		bool loadout;
		bool special;
	};

	struct SEquipmentPack
	{
		string strName;
		int iPrice;
		std::vector<SItem> itemArray;
	};

	virtual bool IsFactoryType(EntityId entity, BuyMenuPage type);
	virtual bool CanBuild(IEntity *pEntity, const char *vehicle);

	bool IsPlayerSpecial();

	void UpdateLastPurchase();
	void UpdateBuyZone(bool trespassing, EntityId zone);
	void UpdateServiceZone(bool trespassing, EntityId zone);
	void UpdateBuyList(const char *page = nullptr, bool clearEntities = true);
	void Scroll(int direction);

	//	Swing-O-Meter interface
	// sets current client team
	void SetSOMTeam(int teamId);
	// hide/unhide SOM
	void SetSOMHidden(ESOMHideReason reason, bool hide);

	void SetLastBuyMenuPage(BuyMenuPage page, bool updateTime = false);

	void DetermineCurrentBuyZone(bool sendToFlash = false);

	void GetTeamStatus(int teamId, float &power, float &turretstatus, int &controlledAliens, EntityId &prototypeFactoryId);

	void ShowCaptureProgress(bool show);
	void SetCaptureProgress(float progress);
	void SetCaptureContested(bool contested);

	void ShowConstructionProgress(bool show, bool queued, float time);

	ILINE EHexIconState GetCurrentIconState() {return m_currentHexIconState; };
	
	ILINE void SetLastPurchase(const char* itemName)
	{
		SItem itemdef;
		if(GetItemFromName(itemName, itemdef))
		{
			itemdef.iInventoryID = 0;
			m_thisPurchase.itemArray.push_back(itemdef);
			m_thisPurchase.iPrice += itemdef.iPrice;

			g_pHUD->m_listBoughtItems.push_back(itemdef.strClass);
		}
	}
private:

	int GetPlayerPP();
	int GetPlayerTeamScore();
	void Buy(const char* item, bool reload = true);

	void DeletePackage(int index);
	void BuyPackage(int index);
	void BuyPackage(SEquipmentPack equipmentPackage);

	void InitEquipmentPacks();
	void SaveEquipmentPacks();
	SEquipmentPack LoadEquipmentPack(int index);
	void SavePackage(const char *name, int index = -1);
	void RequestNewLoadoutName(string &name, const char *bluePrint);
	bool CheckDoubleLoadoutName(const char *name);
	void CreateItemFromTableEntry(IScriptTable *pItemScriptTable, SItem &item);
	bool GetItemFromName(const char *name, SItem &item);

	bool WeaponUseAmmo(CWeapon *pWeapon, IEntityClass* pAmmoType);
	bool CanUseAmmo(IEntityClass* pAmmoType);
	void GetItemList(BuyMenuPage itemType, std::vector<SItem> &itemList, bool buyMenu = true );
	void PopulateBuyList(bool clearEntitites = true);
	void UpdateEnergyBuyList(int energy_before, int energy_after);
	void UpdatePackageItemList(const char *page);
	BuyMenuPage ConvertToBuyList(const char *page);
	void ActivateBuyMenuTabs();
	void UpdatePackageList();
	void UpdateCurrentPackage();
	void OnSelectPackage(int index);
	void UpdateModifyPackage(int index);

	//****************************************** MEMBER VARIABLES ***********************************

	//current active buy zones
	std::vector<EntityId> m_currentBuyZones;
	//current active buy zones
	std::vector<EntityId> m_currentServiceZones;
	
	//standing in buy zone ? Or service zone ?
	bool m_bInBuyZone;
	bool m_bInServiceZone;
	//-2=inactive, -1=catch page, 1-0=catch item on page
	int m_iCatchBuyMenuPage;	
	//available equipment packs
	std::vector<SEquipmentPack> m_EquipmentPacks;
	//items bought between the last buy menu open and buy menu close
	SEquipmentPack m_lastPurchase;
	//items bought in a sequence after buy menu open
	SEquipmentPack m_thisPurchase;

	//current buy menu page
	BuyMenuPage m_eCurBuyMenuPage = BUY_PAGE_LOADOUT;
	BuyMenuPage m_restoreBuyMenuPage = BUY_PAGE_NONE;
	int m_restoreBuyMenuPageTime = 0;

	//buy menu flash movie - managed by HUD
	CGameFlashAnimation *g_pBuyMenu;
	CGameFlashAnimation *g_pHexIcon;
	//swing-o-meter - managed here
	CGameFlashAnimation m_animSwingOMeter;
	//the main hud
	CHUD *g_pHUD;
	//gamerules
	CGameRules *m_pGameRules;
	//currently available buy menu pages
	std::vector<bool> m_factoryTypes;
	std::vector<bool> m_serviceZoneTypes;
	
	//swing-o-meter setup
	bool	m_nkLeft;
	int m_lastTimer = -1;

	//new powerstruggle rules 
	std::vector<EntityId> m_powerpoints;
	std::vector<EntityId> m_hqs;
	
	bool m_gotpowerpoints;
	EntityId m_protofactory;

	bool m_capturing;
	float m_captureProgress;
	float m_captureProgressSmooth = 0.0f;
	bool m_updateCaptureProgress = false;

	bool m_constructing;
	bool m_constructionQueued;
	float m_constructionTime;
	float m_constructionTimer;
	int		m_lastConstructionTime;
	int		m_lastBuildingTime;
	EHexIconState m_currentHexIconState;
	int m_lastTeamCol, m_lastPowerPoints;
	int m_lastAlienArg1, m_lastAlienArg2;
	int m_lastPowerArg1, m_lastPowerArg2;
	wstring m_lastHQArg1, m_lastHQArg2;

	int m_teamId;

	int m_somHideFlags = 0;
};

#endif
