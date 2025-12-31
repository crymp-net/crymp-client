--------------------------------------------------------------------------
--	Crytek Source File.
-- 	Copyright (C), Crytek Studios, 2001-2006.
--------------------------------------------------------------------------
--	$Id$
--	$DateTime$
--	Description: GameRules implementation for DefuseAction
--  
--------------------------------------------------------------------------
--  History:
--  - 30/ 6/2006   12:30 : Created by M�rcio Martins
--
----------------------------------------------------------------------------------------------------


-- buy flags
DefuseAction.BUY_WEAPON 		= 1;
DefuseAction.BUY_VEHICLE		= 2;
DefuseAction.BUY_EQUIPMENT	= 4;
DefuseAction.BUY_AMMO			= 8;
DefuseAction.BUY_PROTOTYPE	= 16;

DefuseAction.BUY_ALL				= 32-1;


DefuseAction.weaponList=
{
	{ id="flashbang",					name="@mp_eFlashbang",						price=10, 			amount=1, ammo=true, weapon=false, category="@mp_catExplosives", loadout=1},
	{ id="smokegrenade",			name="@mp_eSmokeGrenade",				price=10, 			amount=1, ammo=true, weapon=false, category="@mp_catExplosives", loadout=1 },
	{ id="explosivegrenade",	name="@mp_eFragGrenade",					price=25, 			amount=1, ammo=true, weapon=false, category="@mp_catExplosives", loadout=1 },
	{ id="empgrenade",			name="@mp_eEMPGrenade",					price=50,				amount=1, ammo=true, weapon=false, category="@mp_catExplosives", loadout=1 },
	
	{ id="pistol",						name="@mp_ePistol", 							price=50, 			class="SOCOM",						category="@mp_catWeapons", loadout=1, 		uniqueloadoutgroup=3, uniqueloadoutcount=2},
	{ id="claymore",					name="@mp_eClaymore",						price=25,				class="Claymore",					buyammo="claymoreexplosive",	selectOnBuyAmmo="true", category="@mp_catExplosives", loadout=1 },
	{ id="avmine",						name="@mp_eMine",								price=25,				class="AVMine",						buyammo="avexplosive",				selectOnBuyAmmo="true", category="@mp_catExplosives", loadout=1 },
	{ id="c4",							name="@mp_eExplosive", 						price=50, 			class="C4", 							buyammo="c4explosive",				selectOnBuyAmmo="true", category="@mp_catExplosives", loadout=1 },

	{ id="shotgun",					name="@mp_eShotgun", 						price=50, 			class="Shotgun", 					uniqueId=4,		category="@mp_catWeapons", loadout=1, 		uniqueloadoutgroup=1, uniqueloadoutcount=2},
	{ id="smg",							name="@mp_eSMG", 								price=75, 			class="SMG", 							uniqueId=5,		category="@mp_catWeapons", loadout=1, 		uniqueloadoutgroup=1, uniqueloadoutcount=2},
	{ id="fy71",							name="@mp_eFY71", 							price=125, 			class="FY71", 						uniqueId=6,		category="@mp_catWeapons", loadout=1, 	uniqueloadoutgroup=1, uniqueloadoutcount=2},
	{ id="macs",						name="@mp_eSCAR", 							price=150, 			class="SCAR", 						uniqueId=7,		category="@mp_catWeapons", loadout=1, 		uniqueloadoutgroup=1, uniqueloadoutcount=2},
	{ id="rpg",							name="@mp_eML", 								price=200, 			class="LAW", 							uniqueId=8,		category="@mp_catExplosives", loadout=1},
	{ id="dsg1",							name="@mp_eSniper"	,							price=200, 			class="DSG1", 						uniqueId=9,		category="@mp_catWeapons", loadout=1, uniqueloadoutgroup=1, uniqueloadoutcount=2},
	{ id="gauss",						name="@mp_eGauss", 							price=600, 			class="GaussRifle",				uniqueId=10,	category="@mp_catWeapons", loadout=1, 		uniqueloadoutgroup=1, uniqueloadoutcount=2},
};


DefuseAction.equipList=
{
	{ id="binocs",			name="@mp_eBinoculars",							price=50,				class="Binoculars", 			uniqueId=101,		category="@mp_catEquipment", loadout=1 },
	{ id="nsivion",			name="@mp_eNightvision", 						price=10, 			class="NightVision", 			uniqueId=102,		category="@mp_catEquipment", loadout=1 },
	{ id="pchute",			name="@mp_eParachute",							price=25,				class="Parachute",				uniqueId=103,		category="@mp_catEquipment", loadout=1 },
	{ id="lockkit",			name="@mp_eLockpick",							price=25, 			class="LockpickKit",			uniqueId=110,		category="@mp_catEquipment", loadout=1, 	uniqueloadoutgroup=2, uniqueloadoutcount=1},
	{ id="repairkit",		name="@mp_eRepair",								price=50, 			class="RepairKit", 				uniqueId=110,		category="@mp_catEquipment", loadout=1, 	uniqueloadoutgroup=2, uniqueloadoutcount=1},
	{ id="radarkit",		name="@mp_eRadar",								price=50, 			class="RadarKit", 				uniqueId=110,		category="@mp_catEquipment", loadout=1, 	uniqueloadoutgroup=2, uniqueloadoutcount=1},
}


DefuseAction.protoList=
{
	{ id="moac",					name="@mp_eAlienWeapon", 			price=300, 		class="AlienMount", 				level=50,		uniqueId=11,	category="@mp_catWeapons", loadout=1, 		uniqueloadoutgroup=1, uniqueloadoutcount=2},
	{ id="moar",					name="@mp_eAlienMOAR", 				price=100, 		class="MOARAttach", 			level=50,		uniqueId=12,	category="@mp_catWeapons", loadout=1 },
	
	{ id="minigun",				name="@mp_eMinigun",						price=250, 		class="Hurricane", 					level=50,		uniqueId=13,	category="@mp_catWeapons", loadout=1, 		uniqueloadoutgroup=1, uniqueloadoutcount=2},
	{ id="tacgun",					name="@mp_eTACLauncher", 			price=500, 		class="TACGun", 					level=100,	energy=5, uniqueId=14,	category="@mp_catWeapons", md=true, loadout=1, uniqueloadoutgroup=1, uniqueloadoutcount=2},

	{ id="usmoac4wd",			name="@mp_eMOACVehicle",			price=300, 		class="US_ltv", 						level=50, 	modification="MOAC", 				vehicle=true, buildtime=20,	category="@mp_catVehicles", loadout=0 },
	{ id="usmoar4wd",			name="@mp_eMOARVehicle",			price=350,		class="US_ltv", 						level=50,		modification="MOAR", 				vehicle=true, buildtime=20,	category="@mp_catVehicles", loadout=0 },

	{ id="ussingtank",			name="@mp_eSingTank",					price=800, 		class="US_tank",		 			level=100, 	energy=10, modification="Singularity",	vehicle=true, md=true, buildtime=60,	category="@mp_catVehicles", loadout=0 },
	{ id="ustactank",			name="@mp_eTACTank",					price=750,		class="US_tank", 					level=100, 	energy=10, modification="TACCannon",		vehicle=true, md=true, buildtime=60,	category="@mp_catVehicles", loadout=0 },
}

DefuseAction.vehicleList=
{	
	{ id="light4wd",				name="@mp_eLightVehicle", 				price=0,			class="US_ltv",						modification="Unarmed", 		buildtime=5,		category="@mp_catVehicles", loadout=0 },
	{ id="us4wd",					name="@mp_eHeavyVehicle", 			price=50,			class="US_ltv",						modification="MP", 		buildtime=5,					category="@mp_catVehicles", loadout=0 },
	{ id="usgauss4wd",		name="@mp_eGaussVehicle",			price=200,		class="US_ltv", 						modification="Gauss", buildtime=10,					category="@mp_catVehicles", loadout=0 },

	{ id="nktruck",				name="@mp_eTruck",						price=0,			class="Asian_truck", 				modification="Hardtop_MP", buildtime=5,			category="@mp_catVehicles", loadout=0 },

	{ id="ussupplytruck",		name="@mp_eSupplyTruck",				price=300,		class="Asian_truck",				modification="spawntruck",	teamlimit=3, abandon=0, spawngroup=true,	buyzoneradius=6, servicezoneradius=16,	buyzoneflags=bor(bor(DefuseAction.BUY_AMMO, DefuseAction.BUY_WEAPON), DefuseAction.BUY_EQUIPMENT),			buildtime=25,		category="@mp_catVehicles", loadout=0		},
		
	{ id="usboat",					name="@mp_eSmallBoat", 				price=0,			class="US_smallboat", 			modification="MP", buildtime=5,				category="@mp_catVehicles", loadout=0 },
	{ id="nkboat",					name="@mp_ePatrolBoat", 				price=100,		class="Asian_patrolboat", 		modification="MP", buildtime=5,				category="@mp_catVehicles", loadout=0 },
	{ id="nkgaussboat",		name="@mp_eGaussPatrolBoat", 		price=200,		class="Asian_patrolboat", 		modification="Gauss", buildtime=10,		category="@mp_catVehicles", loadout=0 },
	{ id="ushovercraft",		name="@mp_eHovercraft", 				price=100,		class="US_hovercraft",			modification="MP", buildtime=20,			category="@mp_catVehicles", loadout=0 },
	{ id="nkaaa",					name="@mp_eAAVehicle",					price=200,		class="Asian_aaa", 				modification="MP",	buildtime=20,			category="@mp_catVehicles", loadout=0 },
		
	{ id="usapc",					name="@mp_eICV",							price=350,		class="US_apc", 					buildtime=20,		category="@mp_catVehicles", loadout=0 },
	{ id="nkapc",					name="@mp_eAPC",							price=450,		class="Asian_apc", 				buildtime=20,	[jeep]=true,	category="@mp_catVehicles", loadout=0 },
	
	{ id="nktank",					name="@mp_eLightTank", 				price=400,		class="Asian_tank",				buildtime=30,		category="@mp_catVehicles", loadout=0 },
	{ id="ustank",					name="@mp_eBattleTank",				price=450,		class="US_tank", 					modification="GaussRifle", 	buildtime=40,		category="@mp_catVehicles", loadout=0 },
	{ id="usgausstank",		name="@mp_eGaussTank",				price=600,		class="US_tank", 					modification="FullGauss", 	buildtime=60,		category="@mp_catVehicles", loadout=0 },
	
	{ id="nkhelicopter",		name="@mp_eHelicopter", 				price=400,		class="Asian_helicopter",		modification="MP",	buildtime=30,		category="@mp_catVehicles", loadout=0 },
	{ id="usvtol",					name="@mp_eVTOL", 						price=600,		class="US_vtol", 					modification="MP",	buildtime=30,		category="@mp_catVehicles", loadout=0 },	
};


--us4wd,nk4wd,nktruck,ustank,ustactank,usgausstank,usapc,nktank,nktactank,nkgausstank,usspawntruck,usammotruck


DefuseAction.ammoList=
{
	{ id="",										name="@mp_eAutoBuy",				price=0,												category="@mp_catAmmo", loadout=1 },
	{ id="bullet",							name="@mp_eBullet", 					price=5,			amount=40,				category="@mp_catAmmo", loadout=1 },
	{ id="fybullet",						name="@mp_eFYBullet", 					price=5,			amount=30,				category="@mp_catAmmo", loadout=1 },
	{ id="shotgunshell",				name="@mp_eShotgunShell",		price=5,			amount=8,					category="@mp_catAmmo", loadout=1 },
	{ id="smgbullet",						name="@mp_eSMGBullet",				price=5,			amount=50,				category="@mp_catAmmo", loadout=1 },	
	{ id="lightbullet",					name="@mp_eLightBullet",				price=5,			amount=40,				category="@mp_catAmmo", loadout=1 },
	
	{ id="sniperbullet",				name="@mp_eSniperBullet",			price=10,			amount=10,				category="@mp_catAmmo", loadout=1 },
	{ id="scargrenade",					name="@mp_eRifleGrenade",			price=20,			amount=1,					category="@mp_catAmmo", loadout=1 },
	{ id="gaussbullet",					name="@mp_eGaussSlug",				price=50,			amount=5, 				category="@mp_catAmmo", loadout=1 },
	
	{ id="incendiarybullet",																	price=50,			amount=30,		invisible=true,		category="@mp_catAmmo", loadout=1 },	
	
	{ id="hurricanebullet",			name="@mp_eMinigunBullet",		price=50,			amount=500,				category="@mp_catAmmo", loadout=1 },
	
	{ id="claymoreexplosive",																price=25,			amount=1,			invisible=true,		category="@mp_catAmmo", loadout=1 },
	{ id="avexplosive",																			price=25,			amount=1,			invisible=true,		category="@mp_catAmmo", loadout=1 },
	{ id="c4explosive",																			price=50,		amount=1,			invisible=true,		category="@mp_catAmmo", loadout=1 },

	{ id="Tank_singularityprojectile",name="@mp_eSingularityShell",			price=200,		amount=1,					category="@mp_catAmmo", loadout=0, vehicleammo=1},	
		
	{ id="towmissile",					name="@mp_eAPCMissile",				price=50,			amount=2,					category="@mp_catAmmo", loadout=0, vehicleammo=1 },
	{ id="dumbaamissile",				name="@mp_eAAAMissile",			price=50,			amount=4,					category="@mp_catAmmo", loadout=0, vehicleammo=1 },
	{ id="tank125",						name="@mp_eTankShells",			price=100,		amount=10,				category="@mp_catAmmo", loadout=0, vehicleammo=1 },
	{ id="helicoptermissile",			name="@mp_eHelicopterMissile",	price=100,		amount=7,					category="@mp_catAmmo", loadout=0, vehicleammo=1 },

	{ id="tank30",							name="@mp_eAPCCannon",			price=100,		amount=100,				category="@mp_catAmmo", loadout=0, vehicleammo=1 },
	{ id="tankaa",							name="@mp_eAAACannon",			price=100,		amount=250,				category="@mp_catAmmo", loadout=0, vehicleammo=1 },
	{ id="a2ahomingmissile",		name="@mp_eVTOLMissile",			price=100,		amount=6,					category="@mp_catAmmo", loadout=0, vehicleammo=1 },
	{ id="gausstankbullet",			name="@mp_eGaussTankSlug",		price=100,		amount=10,				category="@mp_catAmmo", loadout=0, vehicleammo=1 },
	
	{ id="tacgunprojectile",name="@mp_eTACGrenade",			price=200,		amount=1,	ammo=true, 			level=100,		category="@mp_catAmmo", loadout=1},
	{ id="tacprojectile",		name="@mp_eTACTankShell",	price=200,		amount=1,	ammo=true, 			level=100,		category="@mp_catAmmo", vehicleammo=1 },

	{ id="iamag",								name="@mp_eIncendiaryBullet",	price=50, 			class="FY71IncendiaryAmmo",			ammo=false, equip=true, 	buyammo="incendiarybullet", category="@mp_catAddons", loadout=1 },	
	{ id="psilent",							name="@mp_ePSilencer",				price=10, 			class="SOCOMSilencer",			uniqueId=121, ammo=false, equip=true,		category="@mp_catAddons", loadout=1 },
	{ id="plam",								name="@mp_ePLAM",						price=25, 			class="LAM",				uniqueId=122, ammo=false, equip=true,		category="@mp_catAddons", loadout=1 },
	{ id="silent",							name="@mp_eRSilencer",				price=10, 			class="Silencer", 				uniqueId=123, ammo=false, equip=true,		category="@mp_catAddons", loadout=1 },	
	{ id="lam",								name="@mp_eRLAM",						price=25, 			class="LAMRifle",						uniqueId=124, ammo=false, equip=true,		category="@mp_catAddons", loadout=1 },
	{ id="reflex",							name="@mp_eReflex",					price=25,				class="Reflex", 					uniqueId=125, ammo=false, equip=true,		category="@mp_catAddons", loadout=1 },
	{ id="ascope",							name="@mp_eAScope",					price=50, 			class="AssaultScope", 			uniqueId=126, ammo=false, equip=true,		category="@mp_catAddons", loadout=1 },
	{ id="scope",							name="@mp_eSScope",					price=100, 			class="SniperScope", 			uniqueId=127, ammo=false, equip=true,		category="@mp_catAddons", loadout=1 },
	{ id="gl",									name="@mp_eGL",							price=50, 			class="GrenadeLauncher",		uniqueId=128, ammo=false, equip=true,		category="@mp_catAddons", loadout=1 },
};

DefuseAction.buyList={};

for i,v in ipairs(DefuseAction.weaponList) do DefuseAction.buyList[v.id]=v; if (type(v.weapon)=="nil") then v.weapon=true; end;	end;
for i,v in ipairs(DefuseAction.equipList) do DefuseAction.buyList[v.id]=v; if (type(v.equip)=="nil") then	v.equip=true; end; end;
for i,v in ipairs(DefuseAction.protoList) do DefuseAction.buyList[v.id]=v; if (type(v.proto)=="nil") then	v.proto=true; end; end;
for i,v in ipairs(DefuseAction.vehicleList) do DefuseAction.buyList[v.id]=v; if (type(v[jeep])~="nil") then v[jeep]=true; end; if (type(v.vehicle)=="nil") then v.vehicle=true; end; end;
for i,v in ipairs(DefuseAction.ammoList) do DefuseAction.buyList[v.id]=v; if (type(v.ammo)=="nil") then v.ammo=true; end; end;


DefuseAction.captureValue=
{
	[1]=100,
	[2]=250,
	[3]=250,
};

DefuseAction.defenseValue=
{
	[1]=5,
	[2]=10,
	[3]=10,
};

-- pp values
DefuseAction.ppList=
{
	SPAWN										= 100,
	START										= 100,
	KILL										= 100,
	KILL_RANKDIFF_MULT			= 10,
	TURRETKILL							= 100,
	HEADSHOT								= 50,
	MELEE										= 50,
	SUICIDE									= 0,
	TEAMKILL								= -200,
	
	REPAIR									= 20/1000, -- points/per damage repaired

	LOCKPICK								= 25,
	DISARM									= 20,
	TAG_ENEMY								= 5,
		
	VEHICLE_REFUND_MULT		  = 0.75,
	VEHICLE_KILL_MIN				= 10,
	VEHICLE_KILL_MULT				= 0.25,
};


DefuseAction.POWER_TICK_TIME	= 1;		-- time between each resource tick
DefuseAction.powerValue=
{
	[1]=0.1125,
	[2]=0.1485,
	[3]=0.1575,
};


----------------------------------------------------------------------------------------------------
function DefuseAction:ResetPP(playerId)
	self:SetPlayerPP(playerId, 0);
end


----------------------------------------------------------------------------------------------------
function DefuseAction:SetPlayerPP(playerId, pp)
	self.game:SetSynchedEntityValue(playerId, self.PP_AMOUNT_KEY, pp);
end


----------------------------------------------------------------------------------------------------
function DefuseAction:GetPlayerPP(playerId)
	return self.game:GetSynchedEntityValue(playerId, self.PP_AMOUNT_KEY) or 0;
end


----------------------------------------------------------------------------------------------------
function DefuseAction:CalcKillPP(hit)
	local target=hit.target;
	local shooter=hit.shooter;
	local headshot=self:IsHeadShot(hit);
	local melee=hit.type=="melee";

	if(target ~= shooter) then
		local team1=self.game:GetTeam(shooter.id);
		local team2=self.game:GetTeam(target.id);
		if(team1 ~= team2) then
			local ownRank = self:GetPlayerRank(shooter.id);
			local enemyRank = self:GetPlayerRank(target.id);
			local bonus=0;
			
			if (headshot) then
				bonus=bonus+self.ppList.HEADSHOT;
			end
			
			if (melee) then
				bonus=bonus+self.ppList.MELEE;
			end
			
			local rankDiff=enemyRank-ownRank;
			if (rankDiff~=0) then
				bonus=bonus+rankDiff*self.ppList.KILL_RANKDIFF_MULT;
			end
			
			-- check if inside a factory
			for i,factory in pairs(self.factories) do
				local factoryTeamId=self.game:GetTeam(factory.id);
				if (factory:IsPlayerInside(hit.targetId) and (factoryTeamId~=team2) and (factoryTeamId==team1)) then
					bonus=bonus+self.defenseValue[factory:GetCaptureIndex() or 0] or 0;
				end
			end

			return math.max(0, self.ppList.KILL+bonus);
		else
			return self.ppList.TEAMKILL;
		end
	else
		return self.ppList.SUICIDE;
	end
end


----------------------------------------------------------------------------------------------------
function DefuseAction:AwardKillPP(hit)
	local pp=self:CalcKillPP(hit);
	local playerId=hit.shooter.id;
	
	self:AwardPPCount(playerId, pp);	
end


----------------------------------------------------------------------------------------------------
function DefuseAction:AwardPPCount(playerId, c, why)
	if (c>0) then
		local g_pp_scale_income=System.GetCVar("g_pp_scale_income");
		if (g_pp_scale_income) then
			c=math.floor(c*math.max(0, g_pp_scale_income));
		end
	end

	local total=self:GetPlayerPP(playerId)+c;
	self:SetPlayerPP(playerId, math.max(0, total));

	local player=System.GetEntity(playerId);
	if (player) then
		self.onClient:ClPP(player.actor:GetChannel(), c);
	end

	CryAction.SendGameplayEvent(playerId, eGE_Currency, nil, total);
	CryAction.SendGameplayEvent(playerId, eGE_Currency, why, c);
end


----------------------------------------------------------------------------------------------------
function DefuseAction:GetPrice(itemName)
	if (not itemName) then
		return 0;
	end
	
	local entry=self.buyList[itemName];
	local price=0;
	local energy=0;
	if (entry) then
		price=entry.price;
		energy=entry.energy;
	end
	
	if (price>0) then
		g_pp_scale_price=System.GetCVar("g_pp_scale_price");
		if (g_pp_scale_price) then
			price=math.floor(price*math.max(0, g_pp_scale_price));
		end
	end
	
	if (energy and energy>0) then
		g_energy_scale_price=System.GetCVar("g_energy_scale_price");
		if (g_energy_scale_price) then
			energy=math.floor(energy*math.max(0, g_energy_scale_price));
		end
	end
	
	return price,energy;
end


----------------------------------------------------------------------------------------------------
function DefuseAction:GetItemName(itemName)
	if (not itemName) then
		return "";
	end
	
	local entry=self.buyList[itemName];
	if (entry) then
		return entry.name;
	end
	
	return itemName;
end


----------------------------------------------------------------------------------------------------
function DefuseAction:GetItemFlag(itemName)
	local item=self.buyList[itemName];
	local flag=0;
	if (item.ammo) then flag=bor(flag, self.BUY_AMMO); end;
	if (item.equip) then flag=bor(flag, self.BUY_EQUIPMENT); end;
	if (item.proto) then flag=bor(flag, self.BUY_PROTOTYPE); end;
	if (item.vehicle) then flag=bor(flag, self.BUY_VEHICLE); end;
	if (item.weapon) then flag=bor(flag, self.BUY_WEAPON); end;
	
	return flag;
end


----------------------------------------------------------------------------------------------------
function DefuseAction:GetItemDef(itemName)
	local entry=self.buyList[itemName];
	if (entry) then
		return entry;
	end
	return nil;
end


----------------------------------------------------------------------------------------------------
function DefuseAction:ItemExists(playerId, itemName)
	return self.buyList[itemName]~=nil;
end


----------------------------------------------------------------------------------------------------
function DefuseAction:IsVehicle(itemName)
	return self.buyList[itemName].vehicle;
end


----------------------------------------------------------------------------------------------------
function DefuseAction:EnoughPP(playerId, itemName, price)
	if (itemName and not price) then
		price=self:GetPrice(itemName);
	end
	
	if (not price) then
		price=0;
	end
	
	local player=System.GetEntity(playerId);
	if (player) then
		local alive=player.actor:GetHealth()>0;
		if (alive) then
			if (price and (price>self:GetPlayerPP(playerId))) then
				return false;
			end
			return true;
		else
			local revive=self.reviveQueue[playerId];
			local total=price;
			if (revive) then
				total=total+(revive.items_price or 0)+(revive.ammo_price or 0);
			end
			if (total>self:GetPlayerPP(playerId)) then
				return false;
			end
			return true;
		end
	end
	return false;
end


----------------------------------------------------------------------------------------------------
function DefuseAction:CheckBuyLimit(itemName, teamId, teamOnly)
	local def=self:GetItemDef(itemName);
	if (not def) then
		return false;
	end
	
	if (def.limit and (not teamOnly)) then
		local current=self:GetActiveItemCount(itemName);
		if (current>=def.limit) then
			-- send limit warning here
			return false;
		end
	end
	
	if (teamId and def.teamlimit) then
		local current=self:GetActiveItemCount(itemName, teamId);
		if (current>=def.teamlimit) then
			-- send team limit warning here
			return false,true;
		end
	end
	
	return true;
end


----------------------------------------------------------------------------------------------------
function DefuseAction:Buy(itemName)
	self.server:SvBuy(g_localActorId, itemName);
end


----------------------------------------------------------------------------------------------------
function DefuseAction.Server:SvBuy(playerId, itemName)
	local player=System.GetEntity(playerId);
	if (not player) then
		return;
	end
	
	local ok=false;
	local channelId=player.actor:GetChannel();
	if (self.game:GetTeam(playerId)~=0) then
		local frozen=self.game:IsFrozen(playerId);
		local alive=player.actor:GetHealth()>0;	

		if ((not frozen) and self:ItemExists(playerId, itemName)) then
			if (self:IsVehicle(itemName) and alive) then
				if (self:EnoughPP(playerId, itemName)) then
					ok=self:BuyVehicle(playerId, itemName);
				end
			elseif (((not frozen) and self:IsInBuyZone(playerId)) or (not alive)) then
				if (self:EnoughPP(playerId, itemName)) then
					ok=self:BuyItem(playerId, itemName);
				end
			end
		end
	end
	
	if (ok) then
		self.onClient:ClBuyOk(channelId, itemName);
	else
		self.onClient:ClBuyError(channelId, itemName);
	end
end


----------------------------------------------------------------------------------------------------
function DefuseAction.Client:ClBuyError(itemName)
	HUD.OnItemBought(false, itemName);
end

----------------------------------------------------------------------------------------------------
function DefuseAction.Client:ClBuyOk(itemName)
	HUD.OnItemBought(true, itemName);
end


----------------------------------------------------------------------------------------------------
function DefuseAction.Client:ClPP(amount)
	if (amount>0) then
		HUD.BattleLogEvent(eBLE_Currency, "@mp_BLAwardedPP", amount);
	elseif (amount<0) then
		HUD.BattleLogEvent(eBLE_Currency, "@mp_BLLostPP", -amount);
	end
end


----------------------------------------------------------------------------------------------------
function DefuseAction.Client:ClRank(rank, promoted)
	if (promoted) then
		if (rank>1) then
			self:PlaySoundAlert("promotion");
			self.game:TutorialEvent(eTE_Promotion);
			HUD.BattleLogEvent(eBLE_Information, "@mp_BLPromoted", self:GetRankName(rank));
		end
	else
		self:PlaySoundAlert("demotion");
		HUD.BattleLogEvent(eBLE_Information, "@mp_BLDemoted", self:GetRankName(rank));
	end
end


----------------------------------------------------------------------------------------------------
function DefuseAction:BuyAmmo(ammo)
	if (not ammo or ammo=="") then
		local vehicleId=g_localActor.actor:GetLinkedVehicleId();
		local weapon;
		if (vehicleId) then
			local vehicle=System.GetEntity(vehicleId);
			if (vehicle) then
				local seat=vehicle:GetSeat(g_localActorId);
				if (seat and seat.seat:GetWeaponCount()>0) then
					local weaponId = seat.seat:GetWeaponId(1);
					if (weaponId) then
						weapon=System.GetEntity(weaponId);
						
						-- heli + vtol have infinite ammo on primary weapon - buy ammo for 2nd weapon instead.
						if(weapon and weapon.weapon:GetClipSize() == -1 and seat.seat:GetWeaponCount() > 1) then
							local weaponId2 = seat.seat:GetWeaponId(2);
							if(weaponId2) then
								weapon = System.GetEntity(weaponId2);
							end
						end
					end				
				end
			end
		else
			weapon=g_localActor.inventory:GetCurrentItem();
		end
		
		if (weapon and weapon.weapon) then
			ammo=weapon.weapon:GetAmmoType();
		end
	end

	if (ammo and ammo~="") then
		self.server:SvBuyAmmo(g_localActorId, ammo);
	end
end	

----------------------------------------------------------------------------------------------------
function DefuseAction:GetAutoBuyPrice()
	local player=System.GetEntity(g_localActorId);
	if (not player) then
		return 0;
	end
	
	local vehicleId=g_localActor.actor:GetLinkedVehicleId();
	local weapon;
	if (vehicleId) then
		local vehicle=System.GetEntity(vehicleId);
		if (vehicle) then
			local seat=vehicle:GetSeat(g_localActorId);
			if (seat and seat.seat:GetWeaponCount()>0) then
				local weaponId = seat.seat:GetWeaponId(1);
				if (weaponId) then
					weapon=System.GetEntity(weaponId);
				end				
			end
		end
	else
		weapon=g_localActor.inventory:GetCurrentItem();
	end
	
	if (weapon and weapon.weapon) then
		name=weapon.weapon:GetAmmoType();
	end
	
	local ammo=self.buyList[name];
	if (ammo and ammo.ammo) then
		local price=self:GetPrice(name);	
		local vehicleId = player and player.actor:GetLinkedVehicleId();
		if (vehicleId) then
			local vehicle=System.GetEntity(vehicleId);
			local c=vehicle.inventory:GetAmmoCount(name) or 0;
			local m=vehicle.inventory:GetAmmoCapacity(name) or 0;
			if (c<m or m==0) then
				local need=ammo.amount;
				if (m>0) then
					need=math.min(m-c, ammo.amount);
				end

				if (price>0) then
					if (need<ammo.amount) then
						price=math.ceil((need*price)/ammo.amount);
					end
					return price;
				end
			end
		else
			local c=player.inventory:GetAmmoCount(name) or 0;
			local m=player.inventory:GetAmmoCapacity(name) or 0;
			if (c<m or m==0) then
				local need=ammo.amount;
				if (m>0) then
					need=math.min(m-c, ammo.amount);
				end

				if (price>0) then
					if (need<ammo.amount) then
						price=math.ceil((need*price)/ammo.amount);
					end
					return price;
				end
			end
		end
	end

	return 0;
end


-- WARNING: this function does not support vehicle weapons that use more than one ammo type
--	SNH: fixed for multiple weapons on the same seat
----------------------------------------------------------------------------------------------------
function DefuseAction:VehicleCanUseAmmo(vehicle, ammo)
	if (vehicle) then
		for i,seat in pairs(vehicle.Seats) do
			local weaponCount = seat.seat:GetWeaponCount();
			for j = 1,weaponCount do
				local weaponId = seat.seat:GetWeaponId(j);
				if (weaponId) then
					weapon=System.GetEntity(weaponId);
					if (weapon) then
						local weaponAmmo=weapon.weapon:GetAmmoType();
						if ((weaponAmmo==ammo) and (weapon.weapon:GetClipSize()~=-1)) then
							return true;
						end
					end
				end
			end
		end
	end
	
	return false;
end


----------------------------------------------------------------------------------------------------
function DefuseAction:GetActiveItemCount(itemName, teamId)
	local def=self:GetItemDef(itemName);
	if (not def) then
		return 0;
	end
	
	if (not def.class) then
		return -1;
	end
	
	local count=0;
	local entities=System.GetEntitiesByClass(def.class);
	if (entities) then
		if (teamId) then
			for i,entity in pairs(entities) do
				if (entity and entity.builtas and entity.builtas==itemName) then
					if (self.game:GetTeam(entity.id)==teamId) then
						count=count+1;
					end
				end
			end
		else
			for i,entity in pairs(entities) do
				if (entity and entity.builtas and entity.builtas==itemName) then
					count=count+1;
				end
			end
		end
	end
	
	return count;
end


----------------------------------------------------------------------------------------------------
function DefuseAction:DoBuyAmmo(playerId, name)
--Log("DefuseAction.Server:SvBuyAmmo(%s, %s)", EntityName(playerId), tostring(name));
	
	local player=System.GetEntity(playerId);
	if (not player) then
		return false;
	end
	
	local def=self:GetItemDef(name);
	if (not def) then
		return false;
	end
	
	local revive;
	local alive=true;
	if (player.actor:GetHealth()<=0) then
		revive=self.reviveQueue[playerId];
		alive=false;
	end

	local result=false;
	
	local flags=0;
	local level=0;
	local zones=self.inBuyZone[playerId];
	local teamId=self.game:GetTeam(playerId);

	local vehicleId = player and player.actor:GetLinkedVehicleId();
	if (vehicleId) then
		-- don't do this for spawn trucks, just use the buyzone
		local vehicle=System.GetEntity(vehicleId);
		if(not vehicle.buyFlags or vehicle.buyFlags == 0) then
			zones=self.inServiceZone[playerId];
		end
	end

	for zoneId,b in pairs(zones) do
		if (teamId == self.game:GetTeam(zoneId)) then
			local zone=System.GetEntity(zoneId);
			if (zone and zone.GetPowerLevel) then
				local zonelevel=zone:GetPowerLevel();
				if (zonelevel>level) then
					level=zonelevel;
				end
			end
		end
	end

	if (def.level and def.level>0 and def.level>level) then
		self.game:SendTextMessage(TextMessageError, "@mp_AlienEnergyRequired", TextMessageToClient, playerId, def.name);
		return false;
	end
	
	local ammo=self.buyList[name];
	if (ammo and ammo.ammo) then
		local price=self:GetPrice(name);

		local vehicle = vehicleId and System.GetEntity(vehicleId);
		-- ignore vehicles with buyzones here (we want to buy ammo for the player not the vehicle in this case)
		if (vehicleId and vehicle and not vehicle.buyFlags) then
			if (alive) then
				 --is in vehiclebuyzone 
				if (self:IsInServiceZone(playerId) and (price==0 or self:EnoughPP(playerId, nil, price)) and self:VehicleCanUseAmmo(vehicle, name)) then
					local c=vehicle.inventory:GetAmmoCount(name) or 0;
					local m=vehicle.inventory:GetAmmoCapacity(name) or 0;
	
					if (c<m or m==0) then
						local need=ammo.amount;
						if (m>0) then
							need=math.min(m-c, ammo.amount);
						end
	
						-- this function takes care of synchronizing it to clients
						vehicle.vehicle:SetAmmoCount(name, c+need);
					
						if (price>0) then
							if (need<ammo.amount) then
								price=math.ceil((need*price)/ammo.amount);
							end
							self:AwardPPCount(playerId, -price);
						end

						return true;
					end
				end
			end
		elseif ((self:IsInBuyZone(playerId) or (not alive)) and (price==0 or self:EnoughPP(playerId, nil, price))) then
			local c=player.inventory:GetAmmoCount(name) or 0;
			local m=player.inventory:GetAmmoCapacity(name) or 0;

			if (not alive) then
				c=revive.ammo[name] or 0;
			end

			if (c<m or m==0) then
				local need=ammo.amount;
				if (m>0) then
					need=math.min(m-c, ammo.amount);
				end

				if (alive) then
					-- this function takes care of synchronizing it to clients
					player.actor:SetInventoryAmmo(name, c+need);
				else
					revive.ammo[name]=c+need;
				end

				if (price>0) then
					if (need<ammo.amount) then
						price=math.ceil((need*price)/ammo.amount);
					end

					if (alive) then
						self:AwardPPCount(playerId, -price);
					else
						revive.ammo_price=revive.ammo_price+price;
					end
				end
			
				return true;
			end
		end
	end
	
	return false;
end


----------------------------------------------------------------------------------------------------
function DefuseAction.Server:SvBuyAmmo(playerId, name)
	--Log("DefuseAction.Server:SvBuyAmmo(%s, %s)", EntityName(playerId), tostring(name));
	
	local player=System.GetEntity(playerId);
	if (not player) then
		return;
	end
	
	local frozen=self.game:IsFrozen(playerId);
	local channelId = player.actor:GetChannel();		
	local ok=false;
	
	if (not frozen) then
		ok=self:DoBuyAmmo(playerId, name);
	end

	if (ok) then
		self.onClient:ClBuyOk(channelId, name);
	else
		self.onClient:ClBuyError(channelId, name);
	end
end


----------------------------------------------------------------------------------------------------
function DefuseAction:BuyItem(playerId, itemName)
	local price=self:GetPrice(itemName);
	local def=self:GetItemDef(itemName);
	
	if (not def) then
		return false;
	end
	
	if (def.buy) then
		local buydef=self:GetItemDef(def.buy);
		if (buydef and (not self:HasItem(playerId, buydef.class))) then
			local result=self:BuyItem(playerId, buydef.id);
			if (not result) then
				return false;
			end
		end
	end

	local player=System.GetEntity(playerId);

	if (def.buyammo and self:HasItem(playerId, def.class)) then
		local ret = self:DoBuyAmmo(playerId, def.buyammo);
		if(def.selectOnBuyAmmo and ret and player) then
			player.actor:SelectItemByNameRemote(def.class);
		end
		return ret;
	end

	if (not player) then
		return false;
	end
	
	local revive;
	local alive=true;
	if (player.actor:GetHealth()<=0) then
		revive=self.reviveQueue[playerId];
		alive=false;
	end
	
	local uniqueOld=nil;
	if (def.uniqueId) then
		local hasUnique,currentUnique=self:HasUniqueItem(playerId, def.uniqueId);
		if (hasUnique) then
			if (alive) then
				if(def.category == "@mp_catEquipment") then
					self.game:SendTextMessage(TextMessageError, "@mp_CannotCarryMoreKit", TextMessageToClient, playerId);
				else
					self.game:SendTextMessage(TextMessageError, "@mp_CannotCarryMore", TextMessageToClient, playerId);
				end
				return false;
			end
			uniqueOld=currentUnique;
		end
	end
	
	local flags=0;
	local level=0;
	local zones=self.inBuyZone[playerId];
	local teamId=self.game:GetTeam(playerId);

	for zoneId,b in pairs(zones) do
		if (teamId == self.game:GetTeam(zoneId)) then
			local zone=System.GetEntity(zoneId);
			if (zone and zone.GetPowerLevel) then
				local zonelevel=zone:GetPowerLevel();
				if (zonelevel>level) then
					level=zonelevel;
				end
			end
			if (zone and zone.GetBuyFlags) then
				flags=bor(flags, zone:GetBuyFlags());
			end
		end
	end

	-- dead players can't buy anything else
	if (not alive) then
		flags=bor(bor(self.BUY_WEAPON, self.BUY_AMMO), self.BUY_EQUIPMENT);
	end

	if (def.level and def.level>0 and def.level>level) then
		self.game:SendTextMessage(TextMessageError, "@mp_AlienEnergyRequired", TextMessageToClient, playerId, def.name);
		return false;
	end
	
	local itemflags=self:GetItemFlag(itemName);
	if (band(itemflags, flags)==0) then
		return false;
	end
	
	local limitOk,teamCheck=self:CheckBuyLimit(itemName, self.game:GetTeam(playerId));
	if (not limitOk) then
		if (teamCheck) then
			self.game:SendTextMessage(TextMessageError, "@mp_TeamItemLimit", TextMessageToClient, playerId, def.name);
		else
			self.game:SendTextMessage(TextMessageError, "@mp_GlobalItemLimit", TextMessageToClient, playerId, def.name);
		end
		
		return false;
	end
	
	-- check inventory
	local itemId;
	local ok;
	
	if (alive) then
		ok=player.actor:CheckInventoryRestrictions(def.class);
	else
		if (revive.items and table.getn(revive.items)>0) then
			local inventory={};
			for i,v in ipairs(revive.items) do
				local item=self:GetItemDef(v);
				if (item) then
					table.insert(inventory, item.class);
				end
			end
			ok=player.actor:CheckVirtualInventoryRestrictions(inventory, def.class);
		else
			ok=true;
		end
	end
	
	if (ok) then
		if ((not alive) and (uniqueOld)) then
			for i,old in pairs(revive.items) do
				if (old == uniqueOld) then
					revive.items_price=revive.items_price-self:GetPrice(old);
					table.remove(revive.items, i);
					break;
				end
			end
		end
	
		local price,energy=self:GetPrice(def.id);
		if (alive) then
			self:AwardPPCount(playerId, -price);
			if (energy and energy>0) then
				local teamId=self.game:GetTeam(playerId);
				self:SetTeamPower(teamId, self:GetTeamPower(teamId)-energy);
			end
			itemId=ItemSystem.GiveItem(def.class, playerId);
			
			local item=System.GetEntity(itemId);
			if (item) then
				item.builtas=def.id;
			end
		elseif ((not energy) or (energy==0)) then
			table.insert(revive.items, def.id);
			revive.items_price=revive.items_price+price;
		else
			return false;
		end
	else
		self.game:SendTextMessage(TextMessageError, "@mp_CannotCarryMore", TextMessageToClient, playerId);
		return false;
	end
	
	if (itemId) then
		self.Server.OnItemBought(self, itemId, itemName, playerId);
	end
	
	return true;
end


----------------------------------------------------------------------------------------------------
function DefuseAction:HasItem(playerId, itemName)
	local player=System.GetEntity(playerId);
	local inventory=player and player.inventory;
	if (inventory) then
		if (inventory:GetItemByClass(itemName)) then
			return true;
		end
	end
	
	return false;
end


----------------------------------------------------------------------------------------------------
function DefuseAction:HasUniqueItem(playerId, uniqueId)
	local player=System.GetEntity(playerId);
	if (not player) then
		return false;
	end
	
	local alive=(player.actor:GetHealth()>0);
	if (alive) then
		local inventory=player and player.inventory;
		if (inventory) then
			for item,def in pairs(self.buyList) do
				if (def.uniqueId and def.uniqueId==uniqueId) then
					local itemId=inventory:GetItemByClass(def.class);
					if (itemId) then
						local item=System.GetEntity(itemId);
						if (item and item.builtas==def.id) then
							return true, def.id;
						end
					end
				end
			end	
		end
	else
		local revive=self.reviveQueue[playerId];
		for i,v in pairs(revive.items) do
			local def=self:GetItemDef(v);
			if (def and def.uniqueId and def.uniqueId==uniqueId) then
				return true, def.id;
			end
		end
	end
	
	return false;
end


----------------------------------------------------------------------------------------------------
function DefuseAction:GetProductionFactory(playerId, itemName, insideOnly)
	if (not self.factories) then
		return;
	end
	
	local def=self:GetItemDef(itemName);
	if ((not def) or (not def.vehicle))then
		return;
	end
	
	if (def.special) then
		if (not CryAction.IsChannelSpecial(playerId)) then
			return;
		end
	end
	
	local playerTId=self.game:GetTeam(playerId);
	for i,factory in pairs(self.factories) do
		local factoryTId=self.game:GetTeam(factory.id);
		if ((factoryTId==playerTId) and (not def.level or def.level<=factory:GetPowerLevel()) and factory:CanBuild(itemName)) then
			if ((not insideOnly) or (self:IsInBuyZone(playerId, factory.id))) then
				return factory;
			end
		end
	end
end


----------------------------------------------------------------------------------------------------
function DefuseAction:GetProductionFactoryId(playerId, itemName, insideOnly)
	local factory=self:GetProductionFactory(playerId, itemName, insideOnly);
	if (factory) then
		return factory.id;
	end
end


----------------------------------------------------------------------------------------------------
function DefuseAction:BuyVehicle(playerId, itemName)
	local factory=self:GetProductionFactory(playerId, itemName, true);
	if (factory) then
		local limitOk, teamCheck=self:CheckBuyLimit(itemName, self.game:GetTeam(playerId));
		if (not limitOk) then
			if (teamCheck) then
				self.game:SendTextMessage(TextMessageError, "@mp_TeamItemLimit", TextMessageToClient, playerId, self:GetItemName(itemName));
			else
				self.game:SendTextMessage(TextMessageError, "@mp_GlobalItemLimit", TextMessageToClient, playerId, self:GetItemName(itemName));
			end
	
			return false;
		end

		for i,factory in pairs(self.factories) do
			factory:CancelJobForPlayer(playerId);
		end
	
		local price,energy=self:GetPrice(itemName);
		if (factory:Buy(playerId, itemName)) then
			self:AwardPPCount(playerId, -price);
			self:AwardCPCount(playerId, self.cpList.BUYVEHICLE);
			
			if (energy and energy>0) then
				local teamId=self.game:GetTeam(playerId);
				if (teamId and teamId~=0) then
					self:SetTeamPower(teamId, self:GetTeamPower(teamId)-energy);
				end
			end
		
			self:AbandonPlayerVehicle(playerId);
			
			return true;
		end
	end
	
	return false;
end


----------------------------------------------------------------------------------------------------
function DefuseAction:OnEnterBuyZone(zone, player)	
	if (zone.vehicle and (zone.vehicle:IsDestroyed() or zone.vehicle:IsSubmerged())) then
		return;
	end
	
	if (not self.inBuyZone[player.id]) then
		self.inBuyZone[player.id]={};
	end
	
	local was=self.inBuyZone[player.id][zone.id];
	if (not was) then
		self.inBuyZone[player.id][zone.id]=true;
		if (self.game:IsPlayerInGame(player.id)) then
			self.onClient:ClEnterBuyZone(player.actor:GetChannel(), zone.id, true);
		end
	end
	
	self.buyList[van][jeep]=true; -- ;<
end

----------------------------------------------------------------------------------------------------
function DefuseAction:OnLeaveBuyZone(zone, player)
	if (self.inBuyZone[player.id] and self.inBuyZone[player.id][zone.id]) then
		self.inBuyZone[player.id][zone.id]=nil;
		if (self.game:IsPlayerInGame(player.id)) then
			self.onClient:ClEnterBuyZone(player.actor:GetChannel(), zone.id, false);
		end
	end
end


----------------------------------------------------------------------------------------------------
function DefuseAction:IsInBuyZone(playerId, zoneId)
	local zones=self.inBuyZone[playerId];
	if (not zones) then
		return false;
	end
	
	local playerTeamId = self.game:GetTeam(playerId);
	
	if (zoneId) then
		local zoneTeamId = self.game:GetTeam(zoneId);
		return zoneTeamId and zones[zoneId] and zoneTeamId==playerTeamId;
	else
		for zoneId,inside in pairs(zones) do
			local zoneTeamId = self.game:GetTeam(zoneId);
			if (zoneTeamId and inside and zoneTeamId==playerTeamId) then
				return true;
			end
		end
		
		return false;	
	end
end


----------------------------------------------------------------------------------------------------
function DefuseAction:OnEnterServiceZone(zone, player)	
	if (not self.inServiceZone[player.id]) then
		self.inServiceZone[player.id]={};
	end
	
	local was=self.inServiceZone[player.id][zone.id];
	if (not was) then
		self.inServiceZone[player.id][zone.id]=true;
		self.onClient:ClEnterServiceZone(player.actor:GetChannel(), zone.id, true);
	end
end

----------------------------------------------------------------------------------------------------
function DefuseAction:OnLeaveServiceZone(zone, player)
	if (self.inServiceZone[player.id] and self.inServiceZone[player.id][zone.id]) then
		self.inServiceZone[player.id][zone.id]=nil;
		self.onClient:ClEnterServiceZone(player.actor:GetChannel(), zone.id, false);
	end
end


----------------------------------------------------------------------------------------------------
function DefuseAction:IsInServiceZone(playerId, zoneId)
	local zones=self.inServiceZone[playerId];
	if (not zones) then
		return false;
	end
	
	local playerTeamId = self.game:GetTeam(playerId);
	
	if (zoneId) then
		local zoneTeamId = self.game:GetTeam(zoneId);
		return zoneTeamId and zones[zoneId] and zoneTeamId==playerTeamId;
	else
		for zoneId,inside in pairs(zones) do
			local zoneTeamId = self.game:GetTeam(zoneId);
			if (zoneTeamId and inside and zoneTeamId==playerTeamId) then
				return true;
			end
		end
		
		return false;	
	end
end


----------------------------------------------------------------------------------------------------
function DefuseAction:SetTeamPower(teamId, power)
	local prevpower=self:GetTeamPower(teamId) or 0;
	
	if (power<0) then
		power=0;
	elseif (power>100) then
		power=100;
	end
	self.game:SetSynchedGlobalValue(self.TEAMPOWER_TEAM0_KEY+teamId, power);
	
	if (prevpower<100 and power>=100) then
		self.allClients:ClTeamPower(teamId, 100);
	elseif (prevpower<90 and power>=90) then
		self.allClients:ClTeamPower(teamId, 90);
	elseif (prevpower<50 and power>=50) then	
		self.allClients:ClTeamPower(teamId, 50);
	end
end


----------------------------------------------------------------------------------------------------
function DefuseAction:GetTeamPower(teamId)
	return self.game:GetSynchedGlobalValue(self.TEAMPOWER_TEAM0_KEY+teamId) or 0;
end


----------------------------------------------------------------------------------------------------
function DefuseAction:AddPowerPoint(entityId)
	if (not self.powerpoints) then
		self.powerpoints={};
	end

	for i,id in ipairs(self.powerpoints) do
		if (id == entityId) then
			return;
		end
	end
	
	table.insert(self.powerpoints, entityId);
end


----------------------------------------------------------------------------------------------------
function DefuseAction:RemovePowerPoint(entityId)
	if (self.powerpoints) then
		for i,id in ipairs(self.powerpoints) do
			if (id == entityId) then
				table.remove(self.powerpoints, i);
				return;
			end
		end
	end
end


----------------------------------------------------------------------------------------------------
function DefuseAction:ResetPower()
	for i,teamId in pairs(self.teamId) do
		self:SetTeamPower(teamId, 0);
	end
	
	self.powerTick=math.floor(self.POWER_TICK_TIME);
end


----------------------------------------------------------------------------------------------------
function DefuseAction:PowerTick()
	self.powerTick=self.powerTick-1;
	if (self.powerTick<=0) then
		self.powerTick=math.floor(self.POWER_TICK_TIME);
		local scale=1;
		local g_energy_scale_income=System.GetCVar("g_energy_scale_income");
		if (g_energy_scale_income) then
			scale=g_energy_scale_income;
		end

		if (self.powerpoints) then
			for i,teamId in ipairs(self.teamId) do
				local sum=0;
				for j,id in ipairs(self.powerpoints) do
					local powerTeamId=self.game:GetTeam(id);
					if (teamId==powerTeamId) then
						local power=System.GetEntity(id)
						if (power) then
							local idx=1;
							if (power.GetPowerIndex) then
								idx=power:GetPowerIndex();
							end	
							sum=sum+(self.powerValue[idx] or 0)*math.floor(self.POWER_TICK_TIME);
						end
					end
				end
				
				self:SetTeamPower(teamId, self:GetTeamPower(teamId)+sum*scale);
			end
		end

		--Log("RESOURCE TICK: %s=%.2f  %s=%.2f", "tan", self:GetTeamPower(1), "black", self:GetTeamPower(2));
	end
end


----------------------------------------------------------------------------------------------------
function DefuseAction:StopWork(playerId)
	local work=self.works[playerId];
	if (work and work.active and work.complete) then
		if (work.type=="repair") then
			self:AwardPPCount(playerId, math.floor(0.5+self.ppList.REPAIR*work.amount));
			self:AwardCPCount(playerId, math.floor(0.5+self.cpList.REPAIR*work.amount));
		elseif (work.type=="lockpick") then
			self:AwardPPCount(playerId, self.ppList.LOCKPICK);
			self:AwardCPCount(playerId, self.cpList.LOCKPICK);			
		end
	end
	
	TeamInstantAction.StopWork(self, playerId);
end


----------------------------------------------------------------------------------------------------
function DefuseAction:CanWork(entityId, playerId, work_type)
	local result=TeamInstantAction.CanWork(self, entityId, playerId, work_type);
	
	if (result and self.isServer and work_type=="lockpick") then
		local entity=System.GetEntity(entityId); -- are we lockpicking (i.e. changing the team of) a limited entity?
		if (entity and entity.builtas) then
			local def=self:GetItemDef(entity.builtas);
			if (def and def.teamlimit) then
				local limitOk,teamCheck=self:CheckBuyLimit(def.id, self.game:GetTeam(playerId), true);
				if (not limitOk) then
					if (teamCheck) then -- only care if we failed the team warning
						self.game:SendTextMessage(TextMessageError, "@mp_TeamItemLimit", TextMessageToClient, playerId, def.name);
						
						return false;			
					end				
				end
			end
		end
	end
	
	return result;
end
