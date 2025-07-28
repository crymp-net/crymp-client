--------------------------------------------------------------------------
--	Crytek Source File.
-- 	Copyright (C), Crytek Studios, 2001-2004.
--------------------------------------------------------------------------
--	$Id$
--	$DateTime$
--	Description: GameRules implementation for Death Match
--  
--------------------------------------------------------------------------
--  History:
--  - 22/ 9/2004   16:20 : Created by Mathieu Pinard
--  - 04/10/2004   10:43 : Modified by Craig Tiller
--  - 07/10/2004   16:02 : Modified by Marcio Martins
--
----------------------------------------------------------------------------------------------------
Script.LoadScript("scripts/gamerules/instantaction.lua", 1, 1);
--------------------------------------------------------------------------
TeamInstantAction = new(InstantAction);
TeamInstantAction.States = { "Reset", "PreGame", "InGame", "PostGame", };


-- timers
TeamInstantAction.TEAM_CHANGE_MIN_TIME			= 60; -- time before allowing teamchange
TeamInstantAction.MIN_TEAM_LIMIT_WARN_TIMER	= 15; -- team limit warning timer



TeamInstantAction.TIA_SPAWN_LOCATIONS			= true;
TeamInstantAction.TEAM_SPAWN_LOCATIONS			= true;
TeamInstantAction.NEUTRAL_SPAWN_LOCATIONS	= true;

TeamInstantAction.TEAMSCORE_TEAM0_KEY 			= 10;
TeamInstantAction.TEAMSCORE_TEAM1_KEY 			= 11;
TeamInstantAction.TEAMSCORE_TEAM2_KEY 			= 12;


TeamInstantAction.BALANCE_ACTION_TIME				= 30;
TeamInstantAction.BALANCE_ACTION_TIMERID		= 7000;

TeamInstantAction.SCORE_TEAMKILLS_KEY 		= InstantAction.SCORE_LAST_KEY+1;
TeamInstantAction.SCORE_SELFKILLS_KEY 		= InstantAction.SCORE_LAST_KEY+2;

TeamInstantAction.TEAMSCORE_VICTORY_POINTS_REMAINING	= 10;

----------------------------------------------------------------------------------------------------
TeamInstantAction.teamName={ "tan", "black" };
--[[
TeamInstantAction.teamModel=
{
	black	={
						{
							"objects/characters/human/us/nanosuit/nanosuit_us_multiplayer.cdf",
							"objects/weapons/arms_global/arms_nanosuit_us.chr",
							"objects/characters/human/asian/nk_soldier/nk_soldier_frozen_scatter.cgf",
							"objects/characters/human/us/nanosuit/nanosuit_us_fp3p.cdf",
						},
					},
					
	tan		={
						{
							"objects/characters/human/asian/nanosuit/nanosuit_asian_multiplayer.cdf",
							"objects/weapons/arms_global/arms_nanosuit_asian.chr",
							"objects/characters/human/asian/nk_soldier/nk_soldier_frozen_scatter.cgf",
							"objects/characters/human/asian/nanosuit/nanosuit_asian_fp3p.cdf",
						},
					},
}
]]

TeamInstantAction.teamRadio=
{
	black = 
	{
		[1]=
		{
			{"mp_american/us_F5_1_10-4","@mp_radio_Yes",3},
			{"mp_american/us_F5_2_negative","@mp_radio_No",3},
			{"mp_american/us_F5_3_wait","@mp_radio_Wait",3},
			{"mp_american/us_F5_4_follow_me","@mp_radio_FollowMe",3},
			{"mp_american/us_F5_6_thank_you","@mp_radio_Thanks",3},
			{"mp_american/us_F5_5_sorry","@mp_radio_Sorry", 3},
			{"mp_american/us_F5_7_watch_out","@mp_radio_WatchOut", 3},
			{"mp_american/us_F5_8_well_done","@mp_radio_WellDone", 3},
			{"mp_american/us_F5_9_hurry_up","@mp_radio_HurryUp", 3},
		},
		[2]=
		{
			--Deactivated, needs to be 7 entries
			{},
			{},
			{},
			{},
			{},
			{},
			{},
		},
		[3]=
		{
			{"mp_american/us_F7_1_armor_spotted","@mp_radio_ArmorSpotted"},
			{"mp_american/us_F7_2_aircraft_spotted","@mp_radio_AircraftSpotted"},
			{"mp_american/us_F7_3_boat_spotted","@mp_radio_BoatSpotted"},
			{"mp_american/us_F7_4_vehicle_spotted","@mp_radio_LTVSpotted"},
			{"mp_american/us_F7_5_infantry_spotted","@mp_radio_InfantrySpotted"},
			{"mp_american/us_F7_6_sniper","@mp_radio_SniperSpotted"},
		},
		[4]=
		{
			{"mp_american/us_F8_1_request_assistance","@mp_radio_Assistance"},
			{"mp_american/us_F8_2_get_into_vehicle","@mp_radio_GetIn"},
			{"mp_american/us_F8_3_get_out_vehicle","@mp_radio_GetOut"},
			{"mp_american/us_F8_4_mechanical_assistance_needed","@mp_radio_MechAssistance"},
			{"mp_american/us_F8_5_radar_scan","@mp_radio_Radar"},
			{"mp_american/us_F5_10_request_pickup","@mp_radio_RequestPickup", 3},
		},
	},
	tan = 
	{
		[1]=
		{
			{"mp_korean/nk_F5_1_10-4","@mp_radio_Yes",3},
			{"mp_korean/nk_F5_2_negative","@mp_radio_No",3},
			{"mp_korean/nk_F5_3_wait","@mp_radio_Wait",3},
			{"mp_korean/nk_F5_4_follow_me","@mp_radio_FollowMe",3},
			{"mp_korean/nk_F5_6_thank_you","@mp_radio_Thanks",3},
			{"mp_korean/nk_F5_5_sorry","@mp_radio_Sorry", 3},
			{"mp_korean/nk_F5_7_watch_out","@mp_radio_WatchOut", 3},
			{"mp_korean/nk_F5_8_well_done","@mp_radio_WellDone", 3},
			{"mp_korean/nk_F5_9_hurry_up","mp_radio_HurryUp", 3},
		},
		[2]=
		{
			--Deactivated, needs to be 7 entries
			{},
			{},
			{},
			{},
			{},
			{},
			{},
		},
		[3]=
		{
			{"mp_korean/nk_F7_1_armor_spotted","@mp_radio_ArmorSpotted"},
			{"mp_korean/nk_F7_2_aircraft_spotted","@mp_radio_AircraftSpotted"},
			{"mp_korean/nk_F7_3_boat_spotted","@mp_radio_BoatSpotted"},
			{"mp_korean/nk_F7_4_vehicle_spotted","@mp_radio_LTVSpotted"},
			{"mp_korean/nk_F7_5_infantry_spotted","@mp_radio_InfantrySpotted"},
			{"mp_korean/nk_F7_6_sniper","@mp_radio_SniperSpotted"},
		},
		[4]=
		{
			{"mp_korean/nk_F8_1_request_assistance","@mp_radio_Assistance"},
			{"mp_korean/nk_F8_2_get_into_vehicle","@mp_radio_GetIn"},
			{"mp_korean/nk_F8_3_get_out_vehicle","@mp_radio_GetOut"},
			{"mp_korean/nk_F8_4_mechanical_assistance_needed","@mp_radio_MechAssistance"},
			{"mp_korean/nk_F8_5_radar_scan","@mp_radio_Radar"},
			{"mp_korean/nk_F5_10_request_pickup","@mp_radio_Pickup", 3},
		},
	},
}

TeamInstantAction.SoundAlert=
{
	Radio=
	{
		tan=
		{
			start                           = "mp_korean/nk_commander_kyong_roundstart_01",
			timer2m							= "mp_korean/nk_commander_2_minute_warming_01", --CryMP: both warning and warming sounds exist, but only warming will play..
			timer1m							= "mp_korean/nk_commander_1_minute_warming_01",
			timer30s						= "mp_korean/nk_commander_30_second_warming_01",
			timer5s							= "mp_korean/nk_commander_final_countdown_01",
			win								= "mp_korean/nk_commander_win_mission",
			lose							= "mp_korean/nk_commander_fail_mission_01",
			almost_lost						= "mp_korean/nk_commander_round_almost_lost_01",
		},
		
		black=
		{
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
	},
}

----------------------------------------------------------------------------------------------------

Net.Expose {
	Class = TeamInstantAction,
	ClientMethods = {
		ClSetupPlayer					= { RELIABLE_UNORDERED, NO_ATTACH, DEPENTITYID, },
		ClSetSpawnGroup	 			= { RELIABLE_UNORDERED, NO_ATTACH, ENTITYID, },
		ClSetPlayerSpawnGroup	= { RELIABLE_UNORDERED, NO_ATTACH, ENTITYID, ENTITYID },
		ClSpawnGroupInvalid		= { RELIABLE_UNORDERED, NO_ATTACH, ENTITYID, },
		ClVictory							= { RELIABLE_ORDERED, NO_ATTACH, INT8, INT8 },
		
		ClStartWorking				= { RELIABLE_ORDERED, NO_ATTACH, ENTITYID; STRINGTABLE },
		ClStepWorking					= { RELIABLE_ORDERED, NO_ATTACH, INT8 },
		ClStopWorking					= { RELIABLE_ORDERED, NO_ATTACH, ENTITYID, BOOL },
		ClWorkComplete				= { RELIABLE_ORDERED, NO_ATTACH, ENTITYID, STRINGTABLE },
		
		ClClientConnect				= { RELIABLE_UNORDERED, NO_ATTACH, STRING, BOOL },
		ClClientDisconnect		= { RELIABLE_UNORDERED, NO_ATTACH, STRING, },
		ClClientEnteredGame		= { RELIABLE_UNORDERED, NO_ATTACH, STRING, },	
		--ClTimerAlert					= { RELIABLE_UNORDERED, NO_ATTACH, INT8 },
		ClPlayAlert				= { RELIABLE_UNORDERED, NO_ATTACH, STRINGTABLE, BOOL }, --CryMP
	},
	ServerMethods = {
		RequestRevive		 = { RELIABLE_UNORDERED, NO_ATTACH, ENTITYID, },
		RequestSpawnGroup			= { RELIABLE_UNORDERED, NO_ATTACH, ENTITYID, ENTITYID },
		RequestSpectatorTarget= { RELIABLE_UNORDERED, NO_ATTACH, ENTITYID, INT8 },
	},
	ServerProperties = {
	},
};



----------------------------------------------------------------------------------------------------
function TeamInstantAction:PlayRadioAlert(alertName, teamId)
	teamId = tonumber(teamId)
	local teamName=self.game:GetTeamName(teamId);
	if (teamName and teamName~="") then
		local alert=self.SoundAlert.Radio[teamName];
		if (alert) then
			alert=alert[alertName];
			if (alert) then
				self:QueueVoice(alert, bor(SOUND_LOAD_SYNCHRONOUSLY, SOUND_VOICE), SOUND_SEMANTIC_MP_CHAT);
			end
		end
	end
end

----------------------------------------------------------------------------------------------------

function TeamInstantAction:AutoTeamBalanceCheck()
	if ((not self.auto_team_balancing) and (not self.starting_auto_team_balance) and (self.game:GetAutoTeamBalance()~=0)) then
		local tmax, tmin;
		local pmax, pmin;
		for i,teamId in pairs(self.teamId) do
			local c=self.game:GetTeamPlayerCount(teamId);
			if ((not pmax) or (c>pmax)) then
				pmax=c;
				tmax=teamId;
			end
			
			if ((not pmin) or (c<pmin)) then
				pmin=c;
				tmin=teamId;
			end
		end
		
		local diff=pmax-pmin;
		if (diff>=math.max(2, self.game:GetAutoTeamBalanceThreshold())) then
			self.starting_auto_team_balance=true;
			self:SetTimer(self.BALANCE_ACTION_TIMERID, self.BALANCE_ACTION_TIME*1000);
			
			Log("Team imbalance detected. Auto Team Balance starting in %d seconds...", self.BALANCE_ACTION_TIME);
			
			self.game:SendTextMessage(TextMessageCenter, "@mp_AutoTeamBalanceStarting", TextMessageToAll, nil, self.BALANCE_ACTION_TIME);
		end
	end
end


----------------------------------------------------------------------------------------------------
function TeamInstantAction:StartAutoTeamBalance()
	self.auto_team_balancing = true;
	self.auto_team_balance_actions = 0;
	self.auto_team_balance_affected = {};
	self.starting_auto_team_balance=false;
	
	Log("Auto Team Balance Started...");
end


----------------------------------------------------------------------------------------------------
function TeamInstantAction:EndAutoTeamBalance()
	if (self.auto_team_balancing and self.auto_team_balance_actions>0) then
--		self.game:SendTextMessage(TextMessageCenter, "@mp_AutoTeamBalanceFinished", TextMessageToAll, nil);

		self.auto_team_balancing = nil;
		self.auto_team_balance_affected = nil;
		
		Log("Auto Team Balance finished...");
	end
end

----------------------------------------------------------------------------------------------------
function TeamInstantAction:UpdateAutoTeamBalance()
	if (not self.auto_team_balancing) then
		return;
	end
	
	while(true) do
		local tmax, tmin;
		local pmax, pmin;
		
		for i,teamId in pairs(self.teamId) do
			local c=self.game:GetTeamPlayerCount(teamId);
			if ((not pmax) or (c>pmax)) then
				pmax=c;
				tmax=teamId;
			end
			
			if ((not pmin) or (c<pmin)) then
				pmin=c;
				tmin=teamId;
			end
		end
		
		if ((not pmax) or (not pmin) or (pmax-pmin<=1)) then
			self:EndAutoTeamBalance();

			return;
		end
		
		local mrplayer,mrtime;
		local players=self.game:GetTeamPlayers(tmax);
		
		if (players) then
			for i,player in pairs(players) do
				local time=player.last_team_change or _time;
				if ((not mrtime) or (time>mrtime)) then
					if (self:AutoTeamBalanceCanSwitchPlayer(player, tmin)) then
						mrplayer=player;
						mrtime=time;
					end
				end
			end
		end

		if (not mrplayer) then
			return;
		end
		
		local curTeam=self.game:GetTeam(mrplayer.id);
		if (curTeam==tmin) then
			return;
		end

		Log("Auto Team Balance: Switching %s from %d to %d...", mrplayer:GetName(), curTeam, tmin);
			
		self.auto_team_balance_affected[mrplayer.id]=tmin;
		self.auto_team_balance_actions=self.auto_team_balance_actions+1;

		self:AutoTeamBalanceSwitchPlayer(mrplayer, tmin);	
	end
end

----------------------------------------------------------------------------------------------------
function TeamInstantAction:AutoTeamBalanceSwitchPlayer(player, dstTeamId)

	self.game:SendTextMessage(TextMessageCenter, "@mp_AutoTeamBalanceSwapYou", TextMessageToClient, player.id);
	self.game:SendTextMessage(TextMessageCenter, "@mp_AutoTeamBalanceSwap", TextMessageToOtherClients, player.id, player:GetName());

--	self.Server.RequestSpawnGroup(self, player.id, NULL_ENTITY, true);
	self.game:SetTeam(dstTeamId, player.id);
	player.last_team_change=_time;
	
end

----------------------------------------------------------------------------------------------------
function TeamInstantAction:AutoTeamBalanceCanSwitchPlayer(player, dstTeamId)
	return player:IsDead();
end


----------------------------------------------------------------------------------------------------
function TeamInstantAction:ResetScore(playerId)
	self.game:SetSynchedEntityValue(playerId, self.SCORE_TEAMKILLS_KEY, 0);
	self.game:SetSynchedEntityValue(playerId, self.SCORE_SELFKILLS_KEY, 0);
	InstantAction.ResetScore(self, playerId);
end

------------------------------------------------------------------------------------------------------
function TeamInstantAction:Award(player, deaths, kills, headshots, teamkills, selfkills)

	if (player) then
		local cTeamKills=teamkills + (self.game:GetSynchedEntityValue(player.id, self.SCORE_TEAMKILLS_KEY, 0) or 0);
		self.game:SetSynchedEntityValue(player.id, self.SCORE_TEAMKILLS_KEY, cTeamKills);
		local cSelfKills=selfkills + (self.game:GetSynchedEntityValue(player.id, self.SCORE_SELFKILLS_KEY, 0) or 0);
		self.game:SetSynchedEntityValue(player.id, self.SCORE_SELFKILLS_KEY, cSelfKills);		
	end
	InstantAction.Award(self, player, deaths, kills, headshots);
end

----------------------------------------------------------------------------------------------------
function TeamInstantAction:ProcessScores(hit, tk)
	if (self:GetState()=="PostGame") then return; end
	
	local target=hit.target;
	local shooter=hit.shooter;
	local headshot=self:IsHeadShot(hit);

	if(tk == nil) then
		tk = self:IsTeamKill(target, shooter);
	end

	local h=0;
	if (headshot) then
		h=1;
	end

	if (target.actor and target.actor:IsPlayer()) then
		local targetTeamId=self.game:GetTeam(target.id);
		if (target == shooter) then	--selfkill -3
			self:Award(target, 1, 0, 0, 0, 1);
			return;			-- no need to do anything else for suicide
		elseif(not tk)then  -- if teamkill - nothing for target
			self:Award(target, 1, 0, 0, 0, 0);
		end
	end
	
	if (shooter and shooter.actor and shooter.actor:IsPlayer()) then
		local teamId=self.game:GetTeam(shooter.id);
		if (target ~= shooter) then
			if (not tk) then
				self:Award(shooter, 0, 1, h, 0, 0);
				
				-- update team score
				self:SetTeamScore(teamId, self:GetTeamScore(teamId) + self:CalculateScore(0,1,0,0));
			else
				self:Award(shooter, 0, 0, 0, 1, 0);
			end
		end
	end
end


----------------------------------------------------------------------------------------------------
function TeamInstantAction:CheckPlayerScoreLimit(playerId, score)
end


----------------------------------------------------------------------------------------------------
function TeamInstantAction:ResetTeamScores()
	for i,teamId in pairs(self.teamId) do
		self.game:SetSynchedGlobalValue(self.TEAMSCORE_TEAM0_KEY+teamId, 0);
	end
end


----------------------------------------------------------------------------------------------------
function TeamInstantAction:SetTeamScore(teamId, score)
	self.game:SetSynchedGlobalValue(self.TEAMSCORE_TEAM0_KEY+teamId, score);

	if (self:GetState() and self:GetState()=="InGame") then
		self:CheckScoreLimit(teamId, score);
	end
end


----------------------------------------------------------------------------------------------------
function TeamInstantAction:GetTeamScore(teamId)
	return self.game:GetSynchedGlobalValue(self.TEAMSCORE_TEAM0_KEY+teamId) or 0;
end


----------------------------------------------------------------------------------------------------
function TeamInstantAction:GetPlayerScore( playerId )

	local myKillN = self.game:GetSynchedEntityValue(playerId, self.SCORE_KILLS_KEY, 0) or 0;
	local myDeathN = self.game:GetSynchedEntityValue(playerId, self.SCORE_DEATHS_KEY, 0) or 0;	
	local myTeamKillN = self.game:GetSynchedEntityValue(playerId, self.SCORE_TEAMKILLS_KEY, 0) or 0;	
	local mySelfKillN = self.game:GetSynchedEntityValue(playerId, self.SCORE_SELFKILLS_KEY, 0) or 0;		
--	local myTeamKillN = self.SCORE_TEAMKILLS_KEY;	
	local myScore = self:CalculateScore(myDeathN, myKillN, myTeamKillN, mySelfKillN);
	
	return myScore;
end


----------------------------------------------------------------------------------------------------
function TeamInstantAction:GetPlayerTeamKills( playerId )

	return self.game:GetSynchedEntityValue(playerId, self.SCORE_TEAMKILLS_KEY, 0) or 0
	
end


----------------------------------------------------------------------------------------------------
function TeamInstantAction:CheckScoreLimit(teamId, score)

	local state=self:GetState();
	if (state and state~="InGame") then
		return;
	end

	local scoreLimit = self.game:GetScoreLimit(); --System.GetCVar("g_scorelimit");
	local scoreLead = self.game:GetScoreLead(); --System.GetCVar("g_scorelead"); 
	
	if (scoreLimit > 0) then
		if (score >= scoreLimit) then
			if (scoreLead > 1 and self.teamId) then
				for i,id in pairs(self.teamId) do
					if(id ~= teamId) then
						if (self:GetTeamScore(id)+scoreLead > score) then
							return;
						end
					end
				end
			end
		
			self:OnGameEnd(teamId, 3);

		elseif (not self.ALMOST_LOST_ANNOUNCED) then

			if (scoreLimit - self:GetTeamScore(teamId) <= self.TEAMSCORE_VICTORY_POINTS_REMAINING) then
				local enemyTeamId = teamId == 1 and 2 or 1;
				local enemyTeamPlayers=self.game:GetTeamPlayers(enemyTeamId);
				if (enemyTeamPlayers) then
					for i, enemy in pairs(enemyTeamPlayers) do
						local channelId = enemy.actor:GetChannel();
						if (channelId > 0) then
							self.onClient:ClPlayAlert(channelId, "almost_lost", true);
						end
					end
				end

				self.ALMOST_LOST_ANNOUNCED = true;
			end

		end
	end
end


----------------------------------------------------------------------------------------------------
function TeamInstantAction:CheckTimeLimit()	
	if (self.game:IsTimeLimited() and self.game:GetRemainingGameTime()<=0) then
		local state=self:GetState();
		if (state and state~="InGame") then
			return;
		end

		local maxScore=0;
		local maxId=0;
		local draw=false;

		for i,id in pairs(self.teamId) do
			local score=self:GetTeamScore(id);
			if (score>=maxScore) then
				if ((maxId~=0) and (maxScore==score)) then
					draw=true;
				else
					draw=false;
					maxId=id;
					maxScore=score;
				end
			end
		end

		if (not draw or self.OVERTIME_ACTIVE) then
			self:OnGameEnd(maxId, 2);
		else
			local overtimeTime=3;
			self.game:AddOvertime(overtimeTime);
			self.game:SendTextMessage(TextMessageCenter, "@ui_msg_overtime_0", TextMessageToAll, nil, overtimeTime);
			self.OVERTIME_ACTIVE = true;
--			self:OnGameEnd(nil, 2);
		end
	end
end


----------------------------------------------------------------------------------------------------
function TeamInstantAction:PlayerCountOk()
	local preGame=self:GetState()=="PreGame";
	
	for i,teamName in ipairs(self.teamName) do
		local teamId = self.game:GetTeamId(teamName);
		if (self.game:GetTeamChannelCount(teamId, preGame) < self.game:GetMinTeamLimit()) then
			return false;
		end
	end
	
	return true;
end

----------------------------------------------------------------------------------------------------
function TeamInstantAction.Server:OnStartGame()
	InstantAction.Server.OnStartGame(self);
	self.teamkills = {};
end	

----------------------------------------------------------------------------------------------------
function TeamInstantAction:OnGameEnd(winningTeamId, type)
	if (winningTeamId and winningTeamId~=0) then
		local teamName=self.game:GetTeamName(winningTeamId);
	end
	
	self.allClients:ClVictory(winningTeamId or 0, type or 0);

	self.game:EndGame();

	self:GotoState("PostGame");	
end


----------------------------------------------------------------------------------------------------
function TeamInstantAction.Server:OnInit()
	self.teamId={};
	for i,v in ipairs(self.teamName) do
		table.insert(self.teamId, self.game:CreateTeam(v));
	end
	
	InstantAction.Server.OnInit(self);
end


----------------------------------------------------------------------------------------------------
function TeamInstantAction.Client:OnInit()
	InstantAction.Client.OnInit(self);

	-- if we are a server, it will be done by the server-side
	if (not self.isServer) then
		self.teamId={};
		for i,v in ipairs(self.teamName) do
			table.insert(self.teamId, self.game:CreateTeam(v));
		end
	end
	
	--Sound.SetMasterVolumeScale(1);
	
end

----------------------------------------------------------------------------------------------------
function TeamInstantAction.Server:OnClientConnect(channelId, reset, name)
	local player = InstantAction.Server.OnClientConnect(self, channelId, reset, name);

	return player;
end

----------------------------------------------------------------------------------------------------
function TeamInstantAction.Server:OnClientEnteredGame(channelId, player, reset)
	InstantAction.Server.OnClientEnteredGame(self, channelId, player, reset);
end

----------------------------------------------------------------------------------------------------
function TeamInstantAction:CanRevive(playerId)

	local result = InstantAction.CanRevive(self, playerId);
	
	result = self.game:GetTeam(playerId)~=0 and result;

	return result;
end

----------------------------------------------------------------------------------------------------

function TeamInstantAction.Server:OnChangeTeam(playerId, teamId)
	if (teamId ~= self.game:GetTeam(playerId)) then
		local player=System.GetEntity(playerId);
		if (player) then
		
			if (player.last_team_change and teamId~=0) then
				if (self:GetState()=="InGame") then
					if (_time-player.last_team_change<self.TEAM_CHANGE_MIN_TIME) then
						if ((not player.last_team_change_warning) or (_time-player.last_team_change_warning>=4)) then
							player.last_team_change_warning=_time;
							self.game:SendTextMessage(TextMessageError, "@mp_TeamChangeLimit", TextMessageToClient, playerId, self.TEAM_CHANGE_MIN_TIME-math.floor(_time-player.last_team_change+0.5));
						end
						return;
					end
				end
			end
			
			if (self:IsTeamLocked(teamId, playerId)) then
				if ((not player.last_team_locked_warning) or (_time-player.last_team_locked_warning>=4)) then
					player.last_team_locked_warning=_time;
					Log("team change request by %s denied: team %d has too many players", EntityName(playerId), teamId);
					self.game:SendTextMessage(TextMessageError, "@mp_TeamLockedTooMany", TextMessageToClient, playerId);
				end
				return;
			end
		
			if (player.actor:GetHealth()>0 and player.actor:GetSpectatorMode()==0) then
				self:KillPlayer(player);
			end

			self.game:SetTeam(teamId, playerId);
			player.last_team_change=_time;

			self.game:ChangeSpectatorMode(playerId, 0, NULL_ENTITY);
			self:RevivePlayer(player.actor:GetChannel(), player);
		end
	end
end

----------------------------------------------------------------------------------------------------
function TeamInstantAction:StartAnnouncement(playerId, teamId)
	if (playerId ~= g_localActorId) then
		return;
	end
	if (self:GetState() == "PreGame") then
		return;
	end
	if (self.START_ANNOUNCED) then
		return;
	end
	if (teamId == 0) then
		return;
	end
	Script.SetTimer(1000, function()
		local enemyTeamId = teamId == 1 and 2 or 1;
		if (self.game:GetTeamPlayerCount(enemyTeamId) > 0) then
			self:PlayRadioAlert("start", teamId);
		end
	end);
	self.START_ANNOUNCED = true;
end

----------------------------------------------------------------------------------------------------
function TeamInstantAction.Client:OnRevive(playerId, pos, rot, teamId)
	--[[  CryMP: now in c++
	local teamId = teamId;
	local player = System.GetEntity(playerId);
	
	if (player and teamId and teamId~=0) then
		local teamName=self.game:GetTeamName(teamId);
		local models=self.teamModel[teamName];
		if (models and table.getn(models)>0) then
			local model=models[1];
			player:SetModel(model[1], model[2], model[3], model[4]);
		end
	end]]

	self:StartAnnouncement(playerId, teamId);
end

----------------------------------------------------------------------------------------------------
function TeamInstantAction.Client:OnReviveInVehicle(playerId, vehicleId, seatId, teamId)
	--[[  CryMP: now in c++
	local teamId = teamId;
	local player = System.GetEntity(playerId);
	
	if (player and teamId and teamId~=0) then
		local teamName=self.game:GetTeamName(teamId);
		local models=self.teamModel[teamName];
		if (models and table.getn(models)>0) then
			local model=models[1];
			player:SetModel(model[1], model[2], model[3], model[4]);
		end
	end
	]]

	self:StartAnnouncement(playerId, teamId);
end

----------------------------------------------------------------------------------------------------
function TeamInstantAction.Server:RequestRevive(playerId)
	local player = System.GetEntity(playerId);

	if (player and player.actor) then
		-- allow respawn if spectating player and on a team
		if (((player.actor:GetSpectatorMode() == 3 and self.game:GetTeam(playerId)~=0) or (player:IsDead() and player.death_time and _time-player.death_time>2.5))) then
			self:RevivePlayer(player.actor:GetChannel(), player);
		end
	end
end

----------------------------------------------------------------------------------------------------
function TeamInstantAction:AutoAssignTeam(player, forceTeamId)
	if (forcedTeamId and forcedTeamId~=0) then
		self.game:SetTeam(forcedTeamId, player.id);
	else
		teamId=self.teamId[1];
		local teamIdCount=self.game:GetTeamChannelCount(teamId);
		for i,v in pairs(self.teamId) do
			if (teamId ~= v) then
				local c1=self.game:GetTeamChannelCount(v);
				if (c1<teamIdCount) then
					teamId=v;
					teamIdCount=c1;
				end
			end
		end
		
		if (teamId and teamId~=0) then
			self.game:SetTeam(teamId, player.id);
		end
	end
end


----------------------------------------------------------------------------------------------------
function TeamInstantAction.Server:OnHit(hit)
	local shooter = hit.shooter;
	local target = hit.target;
	if (shooter and target and shooter.actor and shooter.actor:IsPlayer()) then
		local team1=self.game:GetTeam(shooter.id);
		local team2=self.game:GetTeam(target.id);
		if(team1 == team2 and team1~=0 and shooter.id~=target.id and (hit.type~="repair")) then
			hit.damage = hit.damage*self.game:GetFriendlyFireRatio();
		end
	end
	return InstantAction.Server.OnHit(self, hit);
end


----------------------------------------------------------------------------------------------------
function TeamInstantAction:CalcExplosionDamage(entity, explosion, obstruction)
	local dmg=InstantAction.CalcExplosionDamage(self, entity, explosion, obstruction);
	
	local shooter = explosion.shooter;
	if (shooter and entity and shooter.actor and shooter.actor:IsPlayer()) then
		local team1=self.game:GetTeam(shooter.id);
		local team2=self.game:GetTeam(entity.id);
		if(team1 == team2 and team1~=0 and shooter.id~=entity.id) then
			dmg = dmg*self.game:GetFriendlyFireRatio();
		end
	end
	
	return dmg;	
end

----------------------------------------------------------------------------------------------------
function TeamInstantAction:GetCollisionDamageMult(entity, collider, hit)	
	local mult = InstantAction.GetCollisionDamageMult(self, entity, collider, hit);
	
	-- vehicles colliding with entities need to respect friendly fire setting.
	if(entity and entity.actor and collider and collider.vehicle) then
		local team1 = self.game:GetTeam(entity.id);
		local team2 = self.game:GetTeam(collider.id);
		if(team1 == team2 and team1~=0 and entity.id~=collider.id) then
			mult = mult * self.game:GetFriendlyFireRatio();
		end
	end
	
	return mult;
end

----------------------------------------------------------------------------------------------------
function TeamInstantAction.Server:OnFreeze(targetId, shooterId, weaponId, value)
	local target=System.GetEntity(targetId);
	if ((self.game:GetFriendlyFireRatio()>0) or (self.game:GetTeam(targetId)~=self.game:GetTeam(shooterId))) then
		if (target.OnFreeze and not target:OnFreeze(shooterId, weaponId, value)) then
			return false;
		end
		
		if (target.actor or target.vehicle) then
			target.frostShooterId=shooterId;
		end
		return true;
	end

	return false;
end


----------------------------------------------------------------------------------------------------
function TeamInstantAction.Server:OnPlayerKilled(hit)
	local target=hit.target;
	target.death_time=_time;
	target.death_pos=target:GetWorldPos(target.death_pos);
	
	local tk = self:IsTeamKill(hit.target, hit.shooter);
	self.game:KillPlayer(hit.targetId, not tk, true, hit.shooterId, hit.weaponId, hit.damage, hit.materialId, hit.typeId, hit.dir);
	self:ProcessScores(hit, tk);
end

----------------------------------------------------------------------------------------------------
function TeamInstantAction:IsTeamKill(target, shooter)

	local tk=false;

	if (shooter and shooter.actor and shooter.actor:IsPlayer() and
			target and target ~= shooter) then
			
			local team1=self.game:GetTeam(shooter.id);
			local team2=self.game:GetTeam(target.id);
			if ((team1~=0) and (team1==team2)) then
				tk=true;
				if (self.OnTeamKill) then
					self:OnTeamKill(target.id, shooter.id);
				end
			end
	end
	return tk;
end

----------------------------------------------------------------------------------------------------
function TeamInstantAction:OnTeamKill(targetId, shooterId)

	if((System.GetCVar("g_tk_punish")~=0) and (self:GetState()=="InGame")) then
		self.teamkills[shooterId] = 1 + (self.teamkills[shooterId] or 0);
	
		if (self.teamkills[shooterId] >= System.GetCVar("g_tk_punish_limit")) then
			CryAction.BanPlayer(shooterId, "You were banned for exceeding team kill limit!");
		end
	end
end

----------------------------------------------------------------------------------------------------
function TeamInstantAction:CalculateScore(deaths, kills, teamkills, selfKills)
--	local score = (deaths * -1) + (kills * 3) + (teamkills * -3);
	if g_gameRules.class == "PowerStruggle" then
		return kills
	else
		local score = (kills * 3);	

		return score;
	end
end


----------------------------------------------------------------------------------------------------
function TeamInstantAction.Server:OnTimer(timerId, msec)
	InstantAction.Server.OnTimer(self, timerId, msec);
	
	if (timerId==self.BALANCE_ACTION_TIMERID) then
		self:StartAutoTeamBalance();
	end
end

----------------------------------------------------------------------------------------------------
function TeamInstantAction:DefaultState(cs, state)
	local default=self[cs];
	self[cs][state]={
		OnClientConnect = default.OnClientConnect,
		OnClientDisconnect = default.OnClientDisconnect,
		OnClientEnteredGame = default.OnClientEnteredGame,
		OnDisconnect = default.OnDisconnect, -- client only
		OnActorAction = default.OnActorAction, -- client only
		OnStartLevel = default.OnStartLevel,
		OnStartGame = default.OnStartGame,

		OnKill = default.OnKill,
		OnHit = default.OnHit,
		OnFreeze = default.OnFreeze,
		OnExplosion = default.OnExplosion,
		OnChangeTeam = default.OnChangeTeam,
		OnChangeSpectatorMode = default.OnChangeSpectatorMode,
		OnSetTeam = default.OnSetTeam,
		OnItemPickedUp = default.OnItemPickedUp,
		OnItemDropped = default.OnItemDropped,

		OnTimer = default.OnTimer,
		OnUpdate = default.OnUpdate,	
	}
end

----------------------------------------------------------------------------------------------------
TeamInstantAction:DefaultState("Server", "Reset");
TeamInstantAction:DefaultState("Client", "Reset");

----------------------------------------------------------------------------------------------------
TeamInstantAction:DefaultState("Server", "PreGame");
TeamInstantAction:DefaultState("Client", "PreGame");

----------------------------------------------------------------------------------------------------
TeamInstantAction:DefaultState("Server", "InGame");
TeamInstantAction:DefaultState("Client", "InGame");

----------------------------------------------------------------------------------------------------
TeamInstantAction:DefaultState("Server", "PostGame");
TeamInstantAction:DefaultState("Client", "PostGame");

----------------------------------------------------------------------------------------------------
TeamInstantAction.Server.PostGame.OnChangeTeam = nil;
TeamInstantAction.Server.PostGame.OnSpectatorMode = nil;


----------------------------------------------------------------------------------------------------
function TeamInstantAction.Client.PreGame:OnBeginState()
	InstantAction.Client.PreGame.OnBeginState(self);
end

----------------------------------------------------------------------------------------------------
function TeamInstantAction.Server.PreGame:OnBeginState()
	self:ResetTime();	
	self:StartTicking();
	self:ResetPlayers();
	self:ResetTeamScores();
		
	self.starting=false;
	self.warningTimer=0;
end


----------------------------------------------------------------------------------------------------
function TeamInstantAction.Server.PreGame:OnUpdate(frameTime)
	TeamInstantAction.Server.InGame.OnUpdate(self, frameTime);
end


----------------------------------------------------------------------------------------------------
function TeamInstantAction.Server.PreGame:OnTick()
	if (self:PlayerCountOk()) then
		if (not self.starting) then
			self.starting=true;
			--self.game:ResetGameStartTimer(System.GetCVar("g_roundRestartTime"));
			self.game:ResetGameStartTimer(self.START_TIMER);
		end
	elseif (self.starting) then
		self.starting=false;
		self.warningTimer=0;
		
		self.game:ResetGameStartTimer(-1);
	end
	
	if (self.starting) then
		if (self.game:GetRemainingStartTimer()<=0) then
			self.starting=false;
			
			self:RestartGame(true);
		end
	else
		self.warningTimer = self.warningTimer-1;
		if (self.warningTimer<=0) then
			self.game:SendTextMessage(TextMessageCenter, "@mp_MinTeamWarning", TextMessageToAll, nil, self.game:GetMinTeamLimit());
			self.warningTimer=self.MIN_TEAM_LIMIT_WARN_TIMER;
		end
	end
end


----------------------------------------------------------------------------------------------------
function TeamInstantAction.Client.PreGame:OnTick()
	InstantAction.Client.PreGame.OnTick(self);
end

----------------------------------------------------------------------------------------------------
function TeamInstantAction.Server.InGame:OnTick()
	self:AutoTeamBalanceCheck();
	self:UpdateAutoTeamBalance();
	
	InstantAction.Server.InGame.OnTick(self);
end


----------------------------------------------------------------------------------------------------
function TeamInstantAction.Server.InGame:OnBeginState()
	self:ResetTime();
	self:StartTicking();
	self:ResetPlayers();
	self:ResetTeamScores();
end

----------------------------------------------------------------------------------------------------
function TeamInstantAction.Client.InGame:OnBeginState()
	InstantAction.Client.InGame.OnBeginState(self);

-- let's not reset it - keep counters for all games
--	local thisActor = System.GetEntity(g_localActorId);
end


----------------------------------------------------------------------------------------------------
function TeamInstantAction.Client.PreGame:OnUpdate(frameTime)

	SinglePlayer.Client.OnUpdate(self, frameTime);

--System.DrawText(300, 50, "tia-PreGame> "..tostring( math.floor(self.game:GetRemainingGameTime()/60))..":"..tostring( (math.floor(self.game:GetRemainingGameTime())%60)), 3);
		 
--System.DrawText(300, 80, "SCORE_LIMIT> "..self.game:GetScoreLimit(), self.game:GetFragLimit(), 2.5, 1, 1, 0, 1);

	--if(self.show_scores == true) then --CryMP: not needed anymore
	--	self:UpdateScores();
	--end

end


----------------------------------------------------------------------------------------------------
function TeamInstantAction.Client.InGame:OnUpdate(frameTime)
	InstantAction.Client.OnUpdate(self, frameTime);
end


----------------------------------------------------------------------------------------------------
function TeamInstantAction.Server.InGame:OnUpdate(frameTime)
	TeamInstantAction.Server.OnUpdate(self, frameTime);

	self:CheckTimeLimit();
	--self:UpdateClAlerts();	--CryMP handled on client side now
end


----------------------------------------------------------------------------------------------------
function TeamInstantAction.Server.PostGame:OnBeginState()
	self:StartTicking();
	self:SetTimer(self.NEXTLEVEL_TIMERID, self.NEXTLEVEL_TIME);
end


----------------------------------------------------------------------------------------------------
function TeamInstantAction.Client.PostGame:OnBeginState()
	
	InstantAction.Client.PostGame.OnBeginState(self);
end


----------------------------------------------------------------------------------------------------
function TeamInstantAction.Client.PostGame:OnEndState()
	InstantAction.Client.PostGame.OnEndState(self);
end

----------------------------------------------------------------------------------------------------
function TeamName(p)
	if (type(p)=="table") then
		p=p.id;
	elseif (type(p)=="string") then
		p=EntityNamed(p) and EntityNamed(p).id;
	end
	return g_gameRules.game:GetTeamName(g_gameRules.game:GetTeam(p));
end

function Team(p)
	if (type(p)=="table") then
		p=p.id;
	elseif (type(p)=="string") then
		p=EntityNamed(p) and EntityNamed(p).id;
	end
	return g_gameRules.game:GetTeam(p);
end

----------------------------------------------------------------------------------------------------
function TeamInstantAction:PlaySoundAlert(alertName, teamId)
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
function TeamInstantAction.Client:ClVictory(teamId, type)
	if (teamId and teamId~=0) then
		local ownTeamId=self.game:GetTeam(g_localActorId);
		if(ownTeamId == teamId) then
			self:PlayRadioAlert("win", ownTeamId);
			self.game:GameOver(1, teamId, g_localActorId);
		else
			self:PlayRadioAlert("lose", ownTeamId);
			self.game:GameOver(-1, teamId, g_localActorId);
		end
	else
		self.game:GameOver(0, 0, g_localActorId);
	end
end

----------------------------------------------------------------------------------------------------
function TeamInstantAction.Client:ClTimerAlert(time) --CryMP: must be there for PowerStruggle
	if (not g_localActorId) then return end
	--[[
	local teamId=self.game:GetTeam(g_localActorId);
	if (time==120) then
		self:PlayRadioAlert("timer2m", teamId);
	elseif(time==60) then
		self:PlayRadioAlert("timer1m", teamId);
	elseif(time==30) then
		self:PlayRadioAlert("timer30s", teamId);		
	else
		self:PlayRadioAlert("timer5s", teamId);
	end
	]]
end

----------------------------------------------------------------------------------------------------
function TeamInstantAction.Client:ClPlayAlert(sound, isRadio)
	if (not g_localActorId) then return end
	
	local teamId=self.game:GetTeam(g_localActorId);
	if (isRadio) then
		self:PlayRadioAlert(sound, teamId);
	else
		self:PlaySoundAlert(sound, teamId);
	end
end

----------------------------------------------------------------------------------------------------
function TeamInstantAction:OnEnterVehicleSeat(vehicle, seat, entityId)
	InstantAction.OnEnterVehicleSeat(self, vehicle, seat, entityId);
	if(self.isServer) then
		self.game:SetTeam(self.game:GetTeam(entityId), vehicle.id);
	end
end

----------------------------------------------------------------------------------------------------
function TeamInstantAction:OnLeaveVehicleSeat(vehicle, seat, passengerId, exiting)
	InstantAction.OnLeaveVehicleSeat(self, vehicle, seat, passengerId, exiting);

	if(self.isServer) then
		if (exiting) then
			local empty=true;
			for i,seat in pairs(vehicle.Seats) do
				local passengerId = seat:GetPassengerId();
				if (passengerId and passengerId~=NULL_ENTITY and passengerId~=entityId) then
					empty=false;
					break;
				end
			end

			if (empty) then
				self.game:SetTeam(0, vehicle.id);
			end
		end
	end
end

----------------------------------------------------------------------------------------------------
function TeamInstantAction:CanEnterVehicle(vehicle, userId)
	if (vehicle.vehicle:GetOwnerId()==userId) then
		return true;
	end
	
	local vteamId=self.game:GetTeam(vehicle.id);
	local pteamId=self.game:GetTeam(userId);

	if (pteamId==vteamId or vteamId==0) then
		return vehicle.vehicle:GetOwnerId()==nil;
	elseif (pteamId~=vteamId) then
		return false;
	end
end

----------------------------------------------------------------------------------------------------
function TeamInstantAction:IsTeamLocked(teamId, playerId)
	local lock=self.game:GetTeamLock();
	if (lock<=0) then
		return false;
	end
	
	local currentTeam=self.game:GetTeam(playerId) or 0;
	
	if (currentTeam~=0) then -- changing team
		local cCount=self.game:GetTeamChannelCount(currentTeam)-1;
		local nCount=self.game:GetTeamChannelCount(teamId)+1;
		
		if (nCount-cCount>lock) then
			return true;
		end
	else	-- joining the game.
		local minTeamId;
		local minCount;
	
		-- check if there is an outnumbered team, and if so, only allow joining the "weakest" team
		local imba=false;
		for i,v in pairs(self.teamId) do
			local vCount=self.game:GetTeamChannelCount(v);

			for k,j in pairs(self.teamId) do
				if (k~=i) then
					local jCount=self.game:GetTeamChannelCount(j);

					if (math.abs(jCount-vCount)>=lock) then
						imba=true;
						if (jCount>vCount) then
							if ((not minCount) or (minCount>vCount)) then
								minCount=vCount;
								minTeamId=v;
							end
						else
							if ((not minCount) or (minCount>jCount)) then
								minCount=jCount;
								minTeamId=j;
							end
						end
					end
				end
			end
		end
		
		if (imba and (teamId~=minTeamId)) then
			return true;
		end
	end
	
	return false;
end
