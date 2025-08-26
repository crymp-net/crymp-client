--------------------------------------------------------------------------
--	Crytek Source File.
-- 	Copyright (C), Crytek Studios, 2001-2006.
--------------------------------------------------------------------------
--	$Id$
--	$DateTime$
--	Description: GameRules implementation for Power Struggle
--  
--------------------------------------------------------------------------
--  History:
--  - 30/ 6/2006   12:30 : Created by Márcio Martins
--
----------------------------------------------------------------------------------------------------



PowerStruggle.SoundAlert=
{
	Radio=
	{
		tan=
		{
			war_underattack			= "mp_korean/nk_commander_building_under_attack_01",
			naval_underattack		= "mp_korean/nk_commander_building_under_attack_02",
			air_underattack			= "mp_korean/nk_commander_building_under_attack_03",
			prototype_underattack="mp_korean/nk_commander_building_under_attack_05",
			bunker_underattack	= "mp_korean/nk_commander_building_under_attack_06",
			alien_underattack		= "mp_korean/nk_commander_building_under_attack_09",
			
			war_lost						= "mp_korean/nk_commander_facility_lost_03",
			naval_lost					= "mp_korean/nk_commander_facility_lost_05",
			air_lost						= "mp_korean/nk_commander_facility_lost_07",
			prototype_lost			= "mp_korean/nk_commander_facility_lost_02",
			bunker_lost					= "mp_korean/nk_commander_facility_lost_06",
			alien_lost					= "mp_korean/nk_commander_facility_lost_crashsite_08",
			
			coord								= "mp_korean/nk_commander_%s_%s",
			
			war_captured				= "mp_korean/nk_commander_facility_captured_war_03",
			naval_captured			= "mp_korean/nk_commander_facility_captured_naval_06",
			air_captured				= "mp_korean/nk_commander_facility_captured_air_05",
			prototype_captured	= "mp_korean/nk_commander_facility_captured_ptype_02",
			bunker_captured			= "mp_korean/nk_commander_facility_captured_bunker_07",
			alien_captured			= "mp_korean/nk_commander_facility_captured_crashsite_08",
			
			war_enemycaptured		= "mp_korean/nk_commander_enemy_captured_war_03",
			naval_enemycaptured	= "mp_korean/nk_commander_enemy_captured_naval_06",
			air_enemycaptured		= "mp_korean/nk_commander_enemy_captured_air_05",
			prototype_enemycaptured	= "mp_korean/nk_commander_enemy_captured_ptype_02",
			bunker_enemycaptured= "mp_korean/nk_commander_enemy_captured_bunker_07",
			alien_enemycaptured	= "mp_korean/nk_commander_enemy_captured_crashsite_08",

			tac									= "mp_korean/nk_commander_nuke_detected_01",
			sing								= "mp_korean/nk_commander_singularity_detected_01",
			
			perimeter						= "mp_korean/nk_commander_base_breach_01",
			turrethit						= "mp_korean/nk_commander_building_under_attack_07",
			turretdestroyed			= "mp_korean/nk_commander_turret_destroyed_01",
			hqhit								= "mp_korean/nk_commander_building_under_attack_08",	

			hqdamaged							= "mp_korean/nk_commander_hq_damaged_01",	
			hqdestroyed							= "mp_korean/nk_commander_hq_destroyed_01",
			
			win									= "mp_korean/nk_commander_win_mission",
			lose								= "mp_korean/nk_commander_fail_mission_01",

			start                           = "mp_korean/nk_commander_kyong_roundstart_01",
			timer2m							= "mp_korean/nk_commander_2_minute_warming_01", --CryMP: both warning and warming sounds exist, but only warming will play..
			timer1m							= "mp_korean/nk_commander_1_minute_warming_01",
			timer30s						= "mp_korean/nk_commander_30_second_warming_01",
			timer5s							= "mp_korean/nk_commander_final_countdown_01",
			almost_lost						= "mp_korean/nk_commander_round_almost_lost_01",
		},
		
		black=
		{
			war_underattack			= "mp_american/us_commander_building_under_attack_01",
			naval_underattack		= "mp_american/us_commander_building_under_attack_02",
			air_underattack			= "mp_american/us_commander_building_under_attack_03",
			prototype_underattack="mp_american/us_commander_building_under_attack_05",
			bunker_underattack	= "mp_american/us_commander_building_under_attack_06",
			alien_underattack		= "mp_american/us_commander_building_under_attack_09", --
			
			war_lost						= "mp_american/us_commander_facility_lost_03",
			naval_lost					= "mp_american/us_commander_facility_lost_05",
			air_lost						= "mp_american/us_commander_facility_lost_07",
			prototype_lost			= "mp_american/us_commander_facility_lost_02",
			bunker_lost					= "mp_american/us_commander_facility_lost_06",
			alien_lost					= "mp_american/us_commander_lost_crashsite_08",
			
			coord								= "mp_american/us_commander_%s_%s",
		
			war_captured				= "mp_american/us_commander_facility_captured_war_03",
			naval_captured			= "mp_american/us_commander_facility_captured_naval_06",
			air_captured				= "mp_american/us_commander_facility_captured_air_05",
			prototype_captured	= "mp_american/us_commander_facility_captured_ptype_02",
			bunker_captured			= "mp_american/us_commander_facility_captured_bunker_07",
			alien_captured			= "mp_american/us_commander_facility_captured_crashsite_08",
			
			war_enemycaptured		= "mp_american/us_commander_enemy_captured_war_03",
			naval_enemycaptured	= "mp_american/us_commander_enemy_captured_naval_06",
			air_enemycaptured		= "mp_american/us_commander_enemy_captured_air_05",
			prototype_enemycaptured	= "mp_american/us_commander_enemy_captured_ptype_02",
			bunker_enemycaptured= "mp_american/us_commander_enemy_captured_bunker_07",
			alien_enemycaptured	= "mp_american/us_commander_enemy_captured_crashsite_08",			
					
			tac									= "mp_american/us_commander_nuke_detected_01",
			sing								= "mp_american/us_commander_singularity_detected_01",
			
			perimeter						= "mp_american/us_commander_base_breach_01",
			turrethit						= "mp_american/us_commander_building_under_attack_07",
			turretdestroyed			= "mp_american/us_commander_turret_destroyed_01",
			hqhit								= "mp_american/us_commander_building_under_attack_08",

			hqdamaged							= "mp_american/us_commander_hq_damaged_01",	
			hqdestroyed							= "mp_american/us_commander_hq_destroyed_01",
				
			win									= "mp_american/us_commander_round_won_01",
			lose								= "mp_american/us_commander_round_lost_01",

			start           = "mp_american/us_commander_prophet_roundstart_01",
			timer2m 		= "mp_american/us_commander_mission_2_minute_warning_01",
			timer1m 		= "mp_american/us_commander_mission_1_minute_warning_02",
			timer30s 		= "mp_american/us_commander_mission_30_second_03",
			timer5s			= "mp_american/us_commander_endgame_countdown_01",
			win				= "mp_american/us_commander_round_won_01",
			lose			= "mp_american/us_commander_round_lost_01",
			almost_lost		= "mp_american/us_commander_round_almost_lost_01",
		},
	},
	
	Sound=
	{
		tan=
		{
--			win								= "sounds/interface:multiplayer_interface:music_win",
--			lose							= "sounds/interface:multiplayer_interface:music_lose",
		},
		
		black=
		{
--			win								= "sounds/interface:multiplayer_interface:music_win",
--			lose							= "sounds/interface:multiplayer_interface:music_lose",
		},
		
		promotion						= "sounds/interface:suit:promotion",
		demotion 						= "sounds/interface:suit:demotion",
		tacalarm						= "sounds/interface:multiplayer_interface:mp_tac_alarm_suit",
		
		constructstart			= "mp_neutral/mp_factory_production_started_01",	
		constructprogress		= "mp_neutral/mp_factory_production_in_progress_01",	
		constructcomplete		= "mp_neutral/mp_factory_production_completed_01",	
		constructcancel			= "mp_neutral/mp_factory_production_cancelled_01",	
		
		power50							= "mp_neutral/mp_prototype_energy_level_07",
		power50available		= "mp_neutral/mp_prototype_energy_level_alientech_01",
		power90							= "mp_neutral/mp_prototype_energy_level_11",
		power100						= "mp_neutral/mp_prototype_energy_level_12",
		power100available		= "mp_neutral/mp_prototype_energy_level_wmd_01",
	},
	
	PA=
	{
		tan=
		{
			hqhit 						=	"sounds/environment:soundspots:alarm_harbor",
		},
		
		black=
		{
			hqhit 						=	"sounds/environment:soundspots:alarm_harbor",
		},
	},
}


----------------------------------------------------------------------------------------------------
----------------------------------------------------------------------------------------------------
function PowerStruggle:GetTextCoord(entity)
	local x,y=1,1;
	if (entity) then
		local pos=entity:GetWorldPos(g_Vectors.temp_v1);
		x,y=pos.x,pos.y;
	end
	
	local alpha={
		"A",	--1
		"B",	--2
		"C",	--3
		"D",	--4
		"E",	--5
		"F",	--6
		"G",	--7
	};
	
	local numeric={
		"1",	--1
		"2",	--2
		"3",	--3
		"4",	--4
		"5",	--5
		"6",	--6
		"7",	--7
	};
	
	local ix, iy=HUD.GetMapGridCoord(x, y);
	
	return (alpha[ix or 1] or "A") .. (numeric[iy or 1] or "1");
end


----------------------------------------------------------------------------------------------------
function PowerStruggle:BLAlert(type, msg, entity, param1)
	local coord=self:GetTextCoord(entity);
	HUD.BattleLogEvent(type, msg, coord, param1);
end


----------------------------------------------------------------------------------------------------
function PowerStruggle:PlayRadioAlertCoord(alertName, teamId, entity)
	local x,y=1,1;
	if (entity) then
		local pos=entity:GetWorldPos(g_Vectors.temp_v1);
		x,y=pos.x,pos.y;
	end
	
	self:PlayRadioAlert(alertName, teamId);
	self:PlayRadioCoord(teamId, x, y);
end


----------------------------------------------------------------------------------------------------
function PowerStruggle:PlayRadioCoord(teamId, x, y)
	local teamName=self.game:GetTeamName(teamId);
	if (teamName and teamName~="") then
		local alert=self.SoundAlert.Radio[teamName];
		if (alert) then
			alert=alert.coord;
			if (alert) then
				local alpha, numeric=self:GetAlertGridCoord(x, y);
				alert=string.format(alert, alpha, numeric);
				self:QueueVoice(alert, bor(SOUND_LOAD_SYNCHRONOUSLY, SOUND_VOICE), SOUND_SEMANTIC_MP_CHAT, 1.5);
			end
		end
	end
end


----------------------------------------------------------------------------------------------------
function PowerStruggle:PlaySoundAlert(alertName, teamId)
	local id;
	if (teamId) then
		local teamName=self.game:GetTeamName(teamId);
		if (teamName and teamName~="") then
			local alert=self.SoundAlert.Sound[teamName];
			if (alert) then
				alert=alert[alertName];
				if (alert) then
					self:QueueVoice(alert, bor(SOUND_LOAD_SYNCHRONOUSLY, SOUND_VOICE), SOUND_SEMANTIC_MP_CHAT);
				end
			end
		end
	else
		local alert=self.SoundAlert.Sound[alertName];
		if (alert) then
			self:QueueVoice(alert, bor(SOUND_LOAD_SYNCHRONOUSLY, SOUND_VOICE), SOUND_SEMANTIC_MP_CHAT);
		end
	end
end

----------------------------------------------------------------------------------------------------
function PowerStruggle:PlayPA(alertName, teamId, building)
	local alert;
	if (teamId) then
		local teamName=self.game:GetTeamName(teamId);
		if (teamName and teamName~="") then
			alert=self.SoundAlert.PA[teamName];
			if (alert) then
				alert=alert[alertName];
			end
		end
	else
		alert=self.SoundAlert.PA[alertName];
	end

	if (alert) then
	  local sndFlags = SOUND_DEFAULT_3D;
	  sndFlags = band(sndFlags, bnot(SOUND_OBSTRUCTION));
	  sndFlags = bor(sndFlags, SOUND_LOAD_SYNCHRONOUSLY);

	  local pos=building:GetWorldPos(g_Vectors.temp_v1);
	  pos.z=pos.z+5;

	  return Sound.Play(alert, pos, sndFlags, SOUND_SEMANTIC_MP_CHAT);
	end
end


----------------------------------------------------------------------------------------------------
function PowerStruggle:GetAlertBuildingName(building)
	if (building) then
		if (building.class=="Factory") then
			return building.Properties.szName, "Factory", "@mp_BLFactory_"..building.Properties.szName;
		elseif (building.class=="AlienEnergyPoint") then
			return "alien", "Alien";
		elseif (building.class=="SpawnGroup") then
			return "bunker", "Bunker";
		end
	end
	
	return "";
end


----------------------------------------------------------------------------------------------------
function PowerStruggle:GetAlertGridCoord(x, y)
	local alpha={
		"alpha",		--1
		"bravo",		--2
		"charlie",	--3
		"delta",		--4
		"echo",			--5
		"foxtrot",	--6
		"golf",			--7
	};
	
	local numeric={
		"one",			--1
		"two",			--2
		"three",		--3
		"four",			--4
		"five",			--5
		"six",			--6
		"seven",		--7
	};
	
	local ix, iy=HUD.GetMapGridCoord(x, y);
	
	return alpha[ix or 1] or "alpha", numeric[iy or 1] or "one";
end


----------------------------------------------------------------------------------------------------
function PowerStruggle:OnGameEnd(teamId, type, shooterId) -- server only
	if (teamId and teamId~=0) then
		local teamName=self.game:GetTeamName(teamId);
		self.game:SendTextMessage(TextMessageCenter, "@mp_GameOverWinner", TextMessageToAll, nil, "@mp_team_"..teamName);
	else
		self.game:SendTextMessage(TextMessageCenter, "@mp_GameOverNoWinner", TextMessageToAll);
	end
	
	self.allClients:ClVictory(teamId or 0, type, shooterId or NULL_ENTITY);
			
	self.nukePlayer = shooterId or NULL_ENTITY;
		
	self.game:EndGame();

	self:GotoState("PostGame");	
end


----------------------------------------------------------------------------------------------------
function PowerStruggle:OnHQDestroyed(hq, shooterId, teamId) -- server only
	if (self.objectives) then
		for i,v in pairs(self.objectives) do
			if (v.OnHQDestroyed) then
				v:OnHQDestroyed(hq, shooterId, teamId);
			end
		end
	end

	self:OnGameEnd(teamId, 1, shooterId);
end


----------------------------------------------------------------------------------------------------
function PowerStruggle.Client:ClTeamPower(teamId, power)
	if (not g_localActor) then
		return;
	end
	
	local ownteam=self.game:GetTeam(g_localActorId)==teamId;
	local type=eBLE_Warning;
	if (ownteam) then		
		type=eBLE_Information;
	end

	if (power==100) then
		if (ownteam) then
			self:PlaySoundAlert("power100");
			self:PlaySoundAlert("power100available");
			self.game:TutorialEvent(eTE_Reactor100);
			HUD.BattleLogEvent(type, "@mp_BLPower100");
			if(HUD) then
				if(HUD.GetObjectiveStatus("PS.Obj3_BuildTAC") ~= MO_COMPLETED) then
					HUD.SetMainObjective("PS.Obj3_BuildTAC");
				end
			end
		else
			HUD.BattleLogEvent(type, "@mp_BLPower100Enemy");
		end
	elseif (power==90) then
		if (ownteam) then
			self:PlaySoundAlert("power90");
			HUD.BattleLogEvent(type, "@mp_BLPower90");
		else
			HUD.BattleLogEvent(type, "@mp_BLPower90Enemy");
		end
	elseif (power==50) then
		if (ownteam) then
			self:PlaySoundAlert("power50");
			self:PlaySoundAlert("power50available");
			self.game:TutorialEvent(eTE_Reactor50);
			HUD.BattleLogEvent(type, "@mp_BLPower50");
		else
			HUD.BattleLogEvent(type, "@mp_BLPower50Enemy");
		end
	end
end


----------------------------------------------------------------------------------------------------
function PowerStruggle.Client:ClVictory(teamId, type, shooterId)
	if (not g_localActorId) then return end;
	
	local ownTeamId=self.game:GetTeam(g_localActorId);
	
	if (teamId and teamId~=0) then
		if(ownTeamId == teamId) then
			--self:PlaySoundAlert("win", ownTeamId);
			self:PlayRadioAlert("win", ownTeamId);
			self.game:TutorialEvent(eTE_GameOverWin);
			self.game:GameOver(1);
		else
			--self:PlaySoundAlert("lose", ownTeamId);
			self:PlayRadioAlert("lose", ownTeamId);
			self.game:TutorialEvent(eTE_GameOverLose);
			self.game:GameOver(-1);
		end
	else
		self.game:GameOver(0);
	end
	
	if (teamId~=0) then
		local win=ownTeamId==(teamId or 0);
		local icon=eBLE_Warning;
		if (win) then
			icon=eBLE_Information;
		end

		local teamName="@mp_team_"..self.game:GetTeamName(teamId);
	
		if (type==1) then
			local shooter=System.GetEntity(shooterId);
			if (win) then
				HUD.BattleLogEvent(icon, "@mp_BLEnemyHQDestroyed", shooter:GetName());
			else
				HUD.BattleLogEvent(icon, "@mp_BLHQDestroyedBy", shooter:GetName());
			end
		elseif (type==2) then
			HUD.BattleLogEvent(icon, "@mp_BLTimeOut", teamName);
		elseif (type==3) then
			HUD.BattleLogEvent(icon, "@mp_BLScoreLimit", teamName);
		end
	else
		if (type==2) then
			HUD.BattleLogEvent(eBLE_Information, "@mp_BLTimeOutNoWinner");
		end
	end
end


----------------------------------------------------------------------------------------------------
function PowerStruggle.Client:ClPerimeterBreached(baseId)
	self:PlayRadioAlert("perimeter", self.game:GetTeam(baseId));

	HUD.BattleLogEvent(eBLE_Warning, "@mp_BLPerimeterWarning");
	
	self.game:TutorialEvent(eTE_EnemyNearBase);
end


----------------------------------------------------------------------------------------------------
function PowerStruggle.Client:ClTurretHit(turretId)
	if (not g_localActorId) then return end;
	
	local teamId=self.game:GetTeam(g_localActorId);
	self:PlayRadioAlert("turrethit", teamId);

	self.game:TutorialEvent(eTE_TurretUnderAttack);
	
	HUD.BattleLogEvent(eBLE_Warning, "@mp_BLUnderAttack");
end

----------------------------------------------------------------------------------------------------
function PowerStruggle.Client:ClTurretDestroyed(turretId)
	if (not g_localActorId) then return end;
	HUD.BattleLogEvent(eBLE_Warning, "@mp_BLTurretDestroyed");
	
	local teamId=self.game:GetTeam(g_localActorId);
	self:PlayRadioAlert("turretdestroyed", teamId);
end	

----------------------------------------------------------------------------------------------------
function PowerStruggle.Client:ClHQHit(hqId)
	if (not g_localActorId) then return end
	
	local teamId=self.game:GetTeam(g_localActorId);
	local hq=System.GetEntity(hqId);
	self:PlayPA("hqhit", teamId, hq);
	local soundId=self:PlayRadioAlert("hqhit", teamId);	
	HUD.BattleLogEvent(eBLE_Warning, "@mp_BLUnderAttack");
end


----------------------------------------------------------------------------------------------------
-- Server Alerts
----------------------------------------------------------------------------------------------------
function PowerStruggle:ResetAlerts()
	self.lastTurretHit={};
	self.lastHQHit={};
	self.lastPerimeterBreached={};
	self.lastTimerAlert=nil;

	for i,teamId in ipairs(self.teamId) do
		self.lastTurretHit[teamId]=0;
		self.lastHQHit[teamId]=0;
		self.lastPerimeterBreached[teamId]=0;
	end
end


----------------------------------------------------------------------------------------------------
function PowerStruggle:GetCondition(teamId)	
	local nturrets=0;
	local ndestroyed=0;

	for i,turretId in ipairs(self.turrets) do
		local turretTeamId=self.game:GetTeam(turretId);
		if (teamId==turretTeamId) then
			local turret=System.GetEntity(turretId);
			if (not turret or turret.item:IsDestroyed()) then
				ndestroyed=ndestroyed+1;
			end
			nturrets=nturrets+1;
		end
	end
	
	local turretcondition=0;
	
	if (nturrets>0) then
		turretcondition=1-(ndestroyed/nturrets);
	end

	local hqcondition=0;
	
	for i,hqId in ipairs(self.hqs) do
		local hqTeamId=self.game:GetTeam(hqId);
		if (teamId==hqTeamId) then
			local hq=System.GetEntity(hqId);
			if (hq) then
				hqcondition=math.max(0, hq.synched.health)/hq.Properties.nHitPoints;
				break;
			end
		end
	end

	return hqcondition, turretcondition;
end


----------------------------------------------------------------------------------------------------
function PowerStruggle:GetCombatStatus(teamId)
	local hqcombat=false;
	local turretcombat=false;

	if (_time-self.lastHQHit[teamId]<=5) then
		hqcombat=true;
	end

	if (_time-self.lastTurretHit[teamId]<=5) then
		turretcombat=true;
	else
		for i,turretId in ipairs(self.turrets) do
			local turretTeamId=self.game:GetTeam(turretId);
			if (teamId==turretTeamId) then
				local turret=System.GetEntity(turretId);
				if (not turret or turret.weapon:IsFiring()) then
					turretcombat=true;
					break;
				end
			end
		end
	end
	
	-- if perimeter is breached, show both as under attack
	if (_time-self.lastPerimeterBreached[teamId]<=5) then
		hqcombat=true;
		turretcombat=true;
	end


	return hqcombat, turretcombat;
end


----------------------------------------------------------------------------------------------------
function PowerStruggle.Server:OnVehicleBuildStart(building, vehicleName, ownerId, teamId, gateId, time)
end


----------------------------------------------------------------------------------------------------
function PowerStruggle.Server:OnVehicleBuilt(building, vehicleName, vehicleId, ownerId, teamId, gateId)
	local def=self:GetItemDef(vehicleName);
	if (def and def.md) then
		self:SendMDAlert(teamId, vehicleName, ownerId);
		
		Script.SetTimer(1000, function() 
			self.allClients:ClEndGameNear(vehicleId);
		end);
		
		self.game:AddMinimapEntity(vehicleId, 2, 0);
	end

	if (self.objectives) then
		for i,v in pairs(self.objectives) do
			if (v.OnVehicleBuilt) then
				v:OnVehicleBuilt(vehicleId, vehicleName, teamId);
			end
		end
	end
	
	self:SetUnclaimedVehicle(vehicleId, ownerId, teamId, vehicleName, building.id, gateId);
end


----------------------------------------------------------------------------------------------------
function PowerStruggle.Server:OnStartCapture(building, teamId)
end


----------------------------------------------------------------------------------------------------
function PowerStruggle.Server:OnCancelCapture(building)
end


----------------------------------------------------------------------------------------------------
function PowerStruggle.Server:OnStepCapture(building, teamId, remainingTime)
end


----------------------------------------------------------------------------------------------------
function PowerStruggle.Server:OnEnterCaptureArea(building, entity)
	if (entity.actor and entity.actor:IsPlayer()) then
		if (self:GetState()=="InGame") then
			self.onClient:ClEnterCaptureArea(entity.actor:GetChannel(), building.id, true);
		end
	end
end


----------------------------------------------------------------------------------------------------
function PowerStruggle.Server:OnLeaveCaptureArea(building, entity)
	if (entity.actor and entity.actor:IsPlayer()) then
		if (self:GetState()=="InGame") then
			self.onClient:ClEnterCaptureArea(entity.actor:GetChannel(), building.id, false);
		end
	end
end


----------------------------------------------------------------------------------------------------
function PowerStruggle.Server:OnCapture(building, teamId)
	if (self.objectives) then
		for i,v in pairs(self.objectives) do
			if (v.OnCapture) then
				v:OnCapture(building, teamId);
			end
		end
	end	
end


----------------------------------------------------------------------------------------------------
function PowerStruggle.Server:OnStartUncapture(building, teamId)
end


----------------------------------------------------------------------------------------------------
function PowerStruggle.Server:OnCancelUncapture(building)
end


----------------------------------------------------------------------------------------------------
function PowerStruggle.Server:OnStepUncapture(building, teamId, remainingTime)
end


----------------------------------------------------------------------------------------------------
function PowerStruggle.Server:OnUncapture(building, teamId, oldTeamId)
	if (self.objectives) then
		for i,v in pairs(self.objectives) do
			if (v.OnUncapture) then
				v:OnUncapture(building, teamId, oldTeamId);
			end
		end
	end	
end


----------------------------------------------------------------------------------------------------
function PowerStruggle.Server:OnContested(building, contested)
end


----------------------------------------------------------------------------------------------------
function PowerStruggle.Server:OnItemBought(itemId, itemName, playerId)
	-- TODO: make it a flag, maybe?
	local def=self:GetItemDef(itemName);
	if (def and def.md) then
		self:SendMDAlert(self.game:GetTeam(playerId), itemName, playerId);
		self.game:AddMinimapEntity(itemId, 2, 0);
		
		Script.SetTimer(1000, function() 
			self.allClients:ClEndGameNear(itemId);
		end);
	end
	
	if (self.objectives) then
		for i,v in pairs(self.objectives) do
			if (v.OnItemBought) then
				v:OnItemBought(itemId, itemName, playerId);
			end
		end	
	end
end


----------------------------------------------------------------------------------------------------
function PowerStruggle.Server:OnPerimeterBreached(base, player)
	if (base and self:GetState()=="InGame") then
		local teamId=self.game:GetTeam(base.id);
		if (teamId~=0) then
			if (_time-self.lastPerimeterBreached[teamId]>=5) then
				self.lastPerimeterBreached[teamId]=_time;
				local players=self.game:GetTeamPlayers(teamId, true);
				if (players) then
					for i,p in pairs(players) do
						local channel=p.actor:GetChannel();
						if (channel>0) then
							self.onClient:ClPerimeterBreached(channel, base.id);
						end
					end
				end
			end
		end
	end
end


----------------------------------------------------------------------------------------------------
function PowerStruggle.Server:OnTurretHit(turret, hit)
	if (turret and self:GetState()=="InGame") then
		local teamId=self.game:GetTeam(turret.id) or 0;

		if (teamId~=0) then
			if (_time-self.lastTurretHit[teamId]>=5) then
				self.lastTurretHit[teamId]=_time;
				local players=self.game:GetTeamPlayers(teamId, true);
				if (players) then
					for i,p in pairs(players) do
						local channel=p.actor:GetChannel();
						if (channel>0) then
							self.onClient:ClTurretHit(channel, turret.id);
							if(turret.item:IsDestroyed()) then
								self.onClient:ClTurretDestroyed(channel, turret.id);
							end
						end
					end
				end
			end			
			
			if ((teamId==0 or (teamId~=self.game:GetTeam(hit.shooterId))) and turret.item:IsDestroyed()) then
				self:AwardPPCount(hit.shooterId, self.ppList.TURRETKILL);
				self:AwardCPCount(hit.shooterId, self.cpList.TURRETKILL);
			end			
		end
	end
end


----------------------------------------------------------------------------------------------------
function PowerStruggle.Server:OnHQHit(hq, hit)
	if (hq and hit.damage>0 and self:GetState()=="InGame") then
		local teamId=self.game:GetTeam(hq.id) or 0;

		if (teamId~=0) then
			if (_time-self.lastHQHit[teamId]>=5) then
				self.lastHQHit[teamId]=_time;
				local players=self.game:GetTeamPlayers(teamId, true);
				if (players) then
					for i,p in pairs(players) do
						local channel=p.actor:GetChannel();
						if (channel>0) then
							self.onClient:ClHQHit(channel, hq.id);
						end
					end
				end
			end			
		end
	end
end


----------------------------------------------------------------------------------------------------
-- Client Alerts
----------------------------------------------------------------------------------------------------

----------------------------------------------------------------------------------------------------
function PowerStruggle:SendMDAlert(teamId, name, playerId)
	for i,id in ipairs(self.teamId) do	
		if (id ~= teamId) then
			local players=self.game:GetTeamPlayers(id, true);
			if (players) then
				for k,player in pairs(players) do
					self.onClient:ClMDAlert(player.actor:GetChannel(), name);
				end
			end
		end
	end
	
	--also alert the player who just bought the MD weapon
	local thisPlayer = System.GetEntity(playerId);
	if(thisPlayer) then
		self.onClient:ClMDAlert_ToPlayer(thisPlayer.actor:GetChannel());
	end
end

----------------------------------------------------------------------------------------------------
function PowerStruggle.Client:ClMDAlert(name)
	if (not g_localActorId) then return end
	
	self:PlaySoundAlert("tacalarm");
	if ((name == "ustactank") or (name == "nktactank") or (name == "tacgun")) then
		self:PlayRadioAlert("tac", self.game:GetTeam(g_localActorId));
		
		if(name == "tacgun") then
			self.game:TutorialEvent(eTE_TACLauncherCompleted);
		else
			self.game:TutorialEvent(eTE_TACTankCompleted);
		end
	else
		if ((name == "ussingtank")) then
			self:PlayRadioAlert("sing", self.game:GetTeam(g_localActorId));
			self.game:TutorialEvent(eTE_SingularityCompleted);
		end
 	end

 	HUD.BattleLogEvent(eBLE_Warning, "@mp_BLMDAlert");
end

----------------------------------------------------------------------------------------------------
function PowerStruggle.Client:ClMDAlert_ToPlayer()
	if(HUD) then
		HUD.SetMainObjective("PS.Obj4_DestroyHQ");
	end
	
	local teamId = self.game:GetTeam(g_localActorId);
	for i,hqId in ipairs(self.hqs) do
		local hqTeamId=self.game:GetTeam(hqId);
		if (teamId ~= hqTeamId) then
			local hq=System.GetEntity(hqId);
			if (hq) then
				if(hq.Properties.hqType == "bunker") then
					self.game:TutorialEvent(eTE_ApproachEnemyHq);
				elseif(hq.Properties.hqType == "submarine") then
					self.game:TutorialEvent(eTE_ApproachEnemySub);
				elseif(hq.Properties.hqType == "carrier") then
					self.game:TutorialEvent(eTE_ApproachEnemyCarrier);
				end
				break;
			end
		end
	end
end

----------------------------------------------------------------------------------------------------
function PowerStruggle.Client:OnVehicleBuildStart(building, vehicleName, ownerId, teamId, gateId, time)
	if (not g_localActorId) then return end
	
	if (self.game:GetTeam(g_localActorId) == teamId) then
		HUD.BattleLogEvent(eBLE_Information, "@mp_BLVehicleConstruction", self:GetItemName(vehicleName));
	else
		if((vehicleName == "ustactank") or (vehicleName == "nktactank")) then
			self.game:TutorialEvent(eTE_TACTankStarted);
		end
	
		if((vehicleName == "ussingtank") or (vehicleName == "nksingtank")) then
			self.game:TutorialEvent(eTE_SingularityStarted);
		end
	end
	
	if (ownerId == g_localActorId) then
		self:PlaySoundAlert("constructstart");
	end
	
	if (ownerId==g_localActorId) then
		HUD.ShowConstructionProgress(true, false, time);
	end
end


----------------------------------------------------------------------------------------------------
function PowerStruggle.Client:OnVehicleBuilt(building, vehicleName, vehicleId, ownerId, teamId, gateId)	
	if (not g_localActorId) then return end
	
 	if (g_localActorId == ownerId) then
 		self:PlaySoundAlert("constructcomplete");
 		MusicLogic.SetEvent(MUSICLOGICEVENT_VEHICLE_CREATED);
 	end

	if (self.game:GetTeam(g_localActorId) == teamId) then
		HUD.BattleLogEvent(eBLE_Information, "@mp_BLVehicleConstructionComplete", self:GetItemName(vehicleName), gateId);
		
		HUD.OnPlayerVehicleBuilt(ownerId, vehicleId);
		if(g_localActorId == ownerId) then
			HUD.RadarShowVehicleReady(vehicleId);
		end
		
		--local def=self:GetItemDef(vehicleName);
		--if (def and def.md) then
			-- this team has built a MD vehicle
			--HUD.SetObjectiveStatus("PS.Obj3_BuildTAC", MO_COMPLETED, 1); -- 'build a TAC'
			--HUD.SetObjectiveStatus("PS.Obj4_DestroyHQ", MO_ACTIVATED, 0); -- 'destroy enemy base'
		--	HUD.SetMainObjective("PS.Obj4_DestroyHQ");
		--end
	end

	if (ownerId==g_localActorId) then
		HUD.ShowConstructionProgress(false, false, 0);
	end
end


----------------------------------------------------------------------------------------------------
function PowerStruggle.Client:OnVehicleQueued(building, vehicleName)
	HUD.BattleLogEvent(eBLE_Information, "@mp_BLVehicleConstructionQueued", self:GetItemName(vehicleName));
	HUD.ShowConstructionProgress(true, true, 0);
end


----------------------------------------------------------------------------------------------------
function PowerStruggle.Client:OnVehicleCancel(building, vehicleName)
	HUD.BattleLogEvent(eBLE_Information, "@mp_BLVehicleConstructionCancel", self:GetItemName(vehicleName));
	self:PlaySoundAlert("constructcancel");
	
	HUD.ShowConstructionProgress(false, false, 0);
end


----------------------------------------------------------------------------------------------------
function PowerStruggle.Client:ClEnterCaptureArea(buildingId, entered) -- currently doesn't support overlapping capture areas
	if (entered) then
		self.capturingId=buildingId;
	else
		self.capturingId=nil;
	end
	
	self:UpdateCaptureProgress(buildingId);
end


----------------------------------------------------------------------------------------------------
function PowerStruggle.Client:OnStartCapture(building, teamId)
	self.capturing[building.id]=teamId;
	self.captureProgress[building.id]=nil;
	self:UpdateCaptureProgress(building.id);
end


----------------------------------------------------------------------------------------------------
function PowerStruggle.Client:OnCancelCapture(building)
	self.capturing[building.id]=nil;
	self.captureProgress[building.id]=nil;
	self:UpdateCaptureProgress(building.id);
end


----------------------------------------------------------------------------------------------------
function PowerStruggle.Client:OnStepCapture(building, teamId, remainingTime)
	self.captureProgress[building.id]=1-(remainingTime/tonumber(building.Properties.captureTime));
	self:UpdateCaptureProgress(building.id);
end


----------------------------------------------------------------------------------------------------
function PowerStruggle.Client:OnCapture(building, teamId)
	self.contested[building.id]=nil;
	self.capturing[building.id]=nil;
	self.captureProgress[building.id]=1;
	self:UpdateCaptureProgress(building.id);
	
	if (not g_localActorId) then return end
	
	local ownTeamId=self.game:GetTeam(g_localActorId);
	if (teamId==ownTeamId) then
		local name, locName, locFactoryName=self:GetAlertBuildingName(building);
		self:PlayRadioAlertCoord(name.."_captured", ownTeamId, building);
		self:BLAlert(eBLE_Information, "@mp_BL"..locName.."Captured", building, locFactoryName);
		
		if(building and building.Properties.buyOptions and building.Properties.buyOptions.bPrototypes == 1) then 
			--HUD.SetObjectiveStatus("PS.Obj1_CapturePT", MO_COMPLETED, 1); -- 'capture prototype factory'
			--HUD.SetObjectiveStatus("PS.Obj2_SecureAliens", MO_ACTIVATED, 0);		-- 'secure aliens'
			HUD.SetMainObjective("PS.Obj2_SecureAliens");
		end
		
		MusicLogic.SetEvent(MUSICLOGICEVENT_FACTORY_CAPTURED);

	else
		local name, locName, locFactoryName=self:GetAlertBuildingName(building);
		self:PlayRadioAlertCoord(name.."_enemycaptured", ownTeamId, building);
		self:BLAlert(eBLE_Warning, "@mp_BLEnemyCaptured"..locName, building, locFactoryName);
		
		MusicLogic.SetEvent(MUSICLOGICEVENT_FACTORY_LOST);
	end
end


----------------------------------------------------------------------------------------------------
function PowerStruggle.Client:OnStartUncapture(building, teamId)
	self.uncapturing[building.id]=teamId;
	self.uncaptureProgress[building.id]=nil;
	self:UpdateCaptureProgress(building.id);
	
	if (not g_localActorId) then return end
	
	if (not self.lastUncaptureAlert) then
		self.lastUncaptureAlert=0;
	end
	
	if (_time-self.lastUncaptureAlert>=5) then
		local ownTeamId=self.game:GetTeam(g_localActorId);
		if (teamId~=ownTeamId) then
			local name, locName, locFactoryName=self:GetAlertBuildingName(building);
			self:PlayRadioAlertCoord(name.."_underattack", ownTeamId, building);
			self:BLAlert(eBLE_Warning, "@mp_BL"..locName.."UnderAttack", building, locFactoryName);
		end
		
		self.lastUncaptureAlert=_time;
	end
end


----------------------------------------------------------------------------------------------------
function PowerStruggle.Client:OnCancelUncapture(building)
	self.uncapturing[building.id]=nil;
	self.uncaptureProgress[building.id]=nil;
	self:UpdateCaptureProgress(building.id);
end


----------------------------------------------------------------------------------------------------
function PowerStruggle.Client:OnStepUncapture(building, teamId, remainingTime)
	self.uncaptureProgress[building.id]=remainingTime/tonumber(building.Properties.captureTime);
	self:UpdateCaptureProgress(building.id);
end


----------------------------------------------------------------------------------------------------
function PowerStruggle.Client:OnUncapture(building, teamId, oldTeamId)
	self.contested[building.id]=nil;
	self.uncapturing[building.id]=nil;
	self.uncaptureProgress[building.id]=0;
	self:UpdateCaptureProgress(building.id);

	if (not g_localActorId) then return end
	
	local ownTeamId=self.game:GetTeam(g_localActorId);
	if (oldTeamId==ownTeamId) then
		local name, locName, locFactoryName=self:GetAlertBuildingName(building);
		self:PlayRadioAlertCoord(name.."_lost", ownTeamId, building);
		self:BLAlert(eBLE_Warning, "@mp_BL"..locName.."Lost", building, locFactoryName);

		if(building and building.Properties.buyOptions and building.Properties.buyOptions.bPrototypes == 1) then 
			--HUD.SetObjectiveStatus("PS.Obj1_CapturePT", MO_ACTIVATED, 0); -- 'capture prototype factory'
			--HUD.SetObjectiveStatus("PS.Obj2_SecureAliens", MO_DEACTIVATED, 1);		-- 'secure aliens'
			--HUD.SetObjectiveStatus("PS.Obj3_BuildTAC", MO_DEACTIVATED, 1);
			--HUD.SetObjectiveStatus("PS.Obj4_DestroyHQ", MO_DEACTIVATED, 1);
			HUD.SetMainObjective("PS.Obj1_CapturePT");
		end
	end
end


function PowerStruggle.Client:OnContested(building, contested)
	if (contested) then
		self.contested[building.id]=true;
	else
		self.contested[building.id]=nil;
	end
	self:UpdateCaptureProgress(building.id);
end


-----------------------------------------------------------------------------------------------------
function PowerStruggle.Client:ClEndGameNear(entityId)
	self.game:EndGameNear(entityId);
end


----------------------------------------------------------------------------------------------------
function PowerStruggle:IsUncapturing(buildingId)
	if (self.uncapturing[buildingId]) then
		return true;
	end
	
	return false;
end


----------------------------------------------------------------------------------------------------
function PowerStruggle:GetTeamCapturing(buildingId)
	return self.capturing[buildingId];
end


----------------------------------------------------------------------------------------------------
function PowerStruggle:GetTeamUncapturing(buildingId)
	return self.uncapturing[buildingId];
end


----------------------------------------------------------------------------------------------------
function PowerStruggle:ResetCaptureProgress()
	self.contested={};
	self.uncapturing={};
	self.capturing={};
	self.captureProgress={};
	self.uncaptureProgress={};
	self.capturingId=nil;
end


----------------------------------------------------------------------------------------------------
function PowerStruggle:UpdateCaptureProgress(buildingId)
	--Log("PowerStruggle:UpdateCaptureProgress(%s)", tostring(buildingId));
	if (not g_localActorId) then return end;

	if (self.capturingId) then
		local teamId=self.game:GetTeam(g_localActorId);
		if (self.capturingId==buildingId and teamId~=0) then
			local capturing=self.capturing[buildingId] or 0;
			local uncapturing=self.uncapturing[buildingId] or 0;
			local contested=self.contested[buildingId] or false;

			if (capturing==teamId or uncapturing==teamId or contested) then
				HUD.SetCaptureContested(false);
				HUD.ShowCaptureProgress(true);

				if (capturing==teamId) then
					HUD.SetCaptureProgress(self.captureProgress[buildingId] or 0);
				elseif (uncapturing==teamId) then
					HUD.SetCaptureProgress(self.uncaptureProgress[buildingId] or 1);
				end
				
				if (contested) then
					HUD.SetCaptureContested(true);
				end
			else
				HUD.SetCaptureContested(false);
				HUD.ShowCaptureProgress(false);
			end
		end
	else
		HUD.SetCaptureContested(false);
		HUD.ShowCaptureProgress(false);
	end
end
