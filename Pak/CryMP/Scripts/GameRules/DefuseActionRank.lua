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
--  - 30/ 6/2006   12:30 : Created by M�rcio Martins
--
----------------------------------------------------------------------------------------------------
DefuseAction.rankList=
{
	{ name="@ui_short_rank_1", 	desc="@ui_rank_1",	cp=0, 							min_pp=100,		equip={ "SOCOM" 	},},
	{ name="@ui_short_rank_2",		desc="@ui_rank_2",	cp=15, 		limit=16,	min_pp=200,		equip={ "SOCOM" 	},},
	{ name="@ui_short_rank_3", 	desc="@ui_rank_3",	cp=40, 		limit=8,	min_pp=300,		equip={ "SOCOM" 	},},
	{ name="@ui_short_rank_4", 	desc="@ui_rank_4",  cp=120,		limit=6,	min_pp=400,		equip={ "SOCOM" 	},}, 
	{ name="@ui_short_rank_5", 	desc="@ui_rank_5",	cp=220, 		limit=4,	min_pp=500,		equip={ "SOCOM" 	},}, 
	{ name="@ui_short_rank_6", 	desc="@ui_rank_6",	cp=320, 		limit=3,	min_pp=600,		equip={ "SOCOM" 	},}, 
	{ name="@ui_short_rank_7", 	desc="@ui_rank_7",	cp=475,	 	limit=2,	min_pp=750,		equip={ "SOCOM" 	},}, 
	{ name="@ui_short_rank_8", 	desc="@ui_rank_8",	cp=650, 		limit=1, 	min_pp=1000,		equip={ "SOCOM" 	},}, 
};


DefuseAction.cpList=
{
	KILL									= 5,
	KILL_RANKDIFF_MULT		= 1,
	TURRETKILL						= 10,
	REPAIR								= 0,
	LOCKPICK							= 0,
	CAPTURE								= 10,
	BUYVEHICLE						= 0,
	TAG_ENEMY							= 0,	--TODO once design is confirmed

	VEHICLE_KILL_MIN			= 5,
	VEHICLE_KILL_MULT			= 0.02,

	--ATTACKING FACILITY (Killing inside hostile facility)
	--DEFENDING FACILITY (Killing inside owned facility)
	--DESTROYING ENEMY TURRETS
	--REPAIRING TURRET
	--DAMAGING ENEMY HQ
	--DESTROYING ENEMY HQ
	--KILLING TAC WEAPON BEARER
};


----------------------------------------------------------------------------------------------------
function DefuseAction:EquipPlayer(player, additionalEquip)
	if(self.game:IsDemoMode() ~= 0) then -- don't equip actors in demo playback mode, only use existing items
		Log("Don't Equip : DemoMode");
		return;
	end;

	player.inventory:Destroy();

	ItemSystem.GiveItem("AlienCloak", player.id, false);
	ItemSystem.GiveItem("OffHand", player.id, false);
	ItemSystem.GiveItem("Fists", player.id, false);
	
	if (additionalEquip and additionalEquip~="") then
		ItemSystem.GiveItemPack(player.id, additionalEquip, true);
	end

	local rank=self:GetPlayerRank(player.id);
	if ((not rank) or (rank<1)) then
		rank=1;
	end

	local equip=self.rankList[rank].equip;
	if (equip) then
		for k,e in ipairs(equip) do
			ItemSystem.GiveItem(e, player.id, false);
		end
	end
end


----------------------------------------------------------------------------------------------------
function DefuseAction:GetPlayerRank(playerId)
	return self.game:GetSynchedEntityValue(playerId, self.RANK_KEY) or 1;
end


----------------------------------------------------------------------------------------------------
function DefuseAction:SetPlayerRank(playerId, rank)
	CryAction.SendGameplayEvent(playerId, eGE_Rank, nil, rank);
	return self.game:SetSynchedEntityValue(playerId, self.RANK_KEY, rank);
end


----------------------------------------------------------------------------------------------------
function DefuseAction:GetPlayerRankName(playerId)
	return self:GetRankName(self:GetPlayerRank(playerId));
end


----------------------------------------------------------------------------------------------------
function DefuseAction:GetRankName(rank, longName, includeShortName)
	local rankdef=self.rankList[rank];
	if (rankdef) then
		if (not longName) then
			return rankdef.name;
		--elseif (includeShortName) then
		--	return string.format("%s (%s)", rankdef.desc, rankdef.name);
		else
			return rankdef.desc;
		end
	end
	return;
end


----------------------------------------------------------------------------------------------------
function DefuseAction:GetRankCP(rank)
	local rankdef=self.rankList[rank];
	if (rankdef) then
		return rankdef.cp;
	end
	return;
end


function DefuseAction:GetRankPP(rank)
	local rankdef=self.rankList[rank];
	if (rankdef) then
		return rankdef.min_pp;
	end
	return;
end



----------------------------------------------------------------------------------------------------
function DefuseAction:UpdateTeamRanks(teamId)
	function compare_by_cp(player1, player2)
		local id1=player1.id;
		local id2=player2.id;
		
		local cp1=self.game:GetSynchedEntityValue(id1, self.CP_AMOUNT_KEY) or 0;
		local cp2=self.game:GetSynchedEntityValue(id2, self.CP_AMOUNT_KEY) or 0;

		if (cp1==cp2) then
			local s1=self.game:GetSynchedEntityValue(id1, self.SCORE_KILLS_KEY) or 0;
			local s2=self.game:GetSynchedEntityValue(id2, self.SCORE_KILLS_KEY, 0) or 0;
			
			if (s1==s2) then
				local d1=self.game:GetSynchedEntityValue(id1, self.SCORE_DEATHS_KEY) or 0;
				local d2=self.game:GetSynchedEntityValue(id2, self.SCORE_DEATHS_KEY) or 0;				

				return d1<d2;
			else
				return s1>s2
			end
		else
			return cp1>cp2;
		end
	end

	local players=self.game:GetTeamPlayers(teamId);
	if (players) then
		table.sort(players, compare_by_cp);
		
		local maxrank=table.getn(self.rankList);
		local currranki=maxrank;
		local rank=self.rankList[currranki];
		local rankc=0;
		
		for i,player in ipairs(players) do
			local playerId=player.id;
			local currcp=self.game:GetSynchedEntityValue(playerId, self.CP_AMOUNT_KEY) or 0;
			while((currcp<rank.cp or (rank.limit and rankc>=rank.limit)) and currranki>1) do
				currranki=currranki-1;
				rank=self.rankList[currranki];
				rankc=0;
			end
			
			rankc=rankc+1;
			
			local oldrank=self.game:GetSynchedEntityValue(playerId, self.RANK_KEY) or 1;
			if (oldrank~=currranki) then
				self.game:SetSynchedEntityValue(playerId, self.RANK_KEY, currranki);

				if (oldrank<currranki) then
				 	-- promoted
				 	self.onClient:ClRank(player.actor:GetChannel(), currranki, true);
				else
					-- demoted
					self.onClient:ClRank(player.actor:GetChannel(), currranki, false);
				end
				
				CryAction.SendGameplayEvent(playerId, eGE_Rank, nil, currranki);
			end
		end
	end
end


----------------------------------------------------------------------------------------------------
function DefuseAction:ResetCP(playerId)
	self:SetPlayerCP(playerId, 0);
	self:SetPlayerRank(playerId, 1);
end


----------------------------------------------------------------------------------------------------
function DefuseAction:SetPlayerCP(playerId, cp)
	self.game:SetSynchedEntityValue(playerId, self.CP_AMOUNT_KEY, cp);
end


----------------------------------------------------------------------------------------------------
function DefuseAction:GetPlayerCP(playerId)
	return self.game:GetSynchedEntityValue(playerId, self.CP_AMOUNT_KEY) or 0;
end


----------------------------------------------------------------------------------------------------
function DefuseAction:CalcKillCP(hit)
	local target=hit.target;
	local shooter=hit.shooter;
	local headshot=self:IsHeadShot(hit);

	if(target ~= shooter) then
		local team1=self.game:GetTeam(shooter.id);
		local team2=self.game:GetTeam(target.id);
		if(team1 ~= team2) then
			local ownRank = self:GetPlayerRank(shooter.id);
			local enemyRank = self:GetPlayerRank(target.id);

			return self.cpList.KILL+math.max(0, (enemyRank-ownRank)*self.cpList.KILL_RANKDIFF_MULT);
		else
			return -10;
		end
	end
	
	return 0;
end


----------------------------------------------------------------------------------------------------
function DefuseAction:AwardKillCP(hit)
	local cp=self:CalcKillCP(hit);
	
	self:AwardCPCount(hit.shooter.id, cp);
end


----------------------------------------------------------------------------------------------------
function DefuseAction:AwardCPCount(playerId, c)
	self:SetPlayerCP(playerId, self:GetPlayerCP(playerId)+c);
end
