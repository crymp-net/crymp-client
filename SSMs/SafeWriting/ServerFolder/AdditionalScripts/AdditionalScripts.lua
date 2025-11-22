--Created 9.9.2012 as part of SSM SafeWriting project by 'Zi;'
function GetProtectionKind(player,isVehicle)
	if(isVehicle)then if(player:GetDriverId())then player=System.GetEntity(player:GetDriverId()) else player=nil; end end
	--Log("player name: %s",player:GetName());
	if(player)then
		local plname=player:GetName();
		local admintag=SafeWriting.Settings.AdminTag;
		local moderatortag=SafeWriting.Settings.ModeratorTag;
		local premiumtag=SafeWriting.Settings.PremiumTag;
		if(plname:sub(0,string.len(admintag))==admintag)then
			if(IsAdmin(player))then
				return 1;
			end
		--elseif(plname:sub(0,string.len(moderatortag))==moderatortag)then
		--	if(IsModerator(player))then
		--		return 2;
		--	end
		--elseif(plname:sub(0,string.len(premiumtag))==premiumtag)then
		--	if(IsPremium(player))then
		--		return 3;
		--	end
		else
			return 0;
		end
	else
		return 0;
	end
end
function ProcessDamageOfBullet(hit,isVehicle)
	MakePluginEvent("OnActorHit",hit,isVehicle);
	local se=SafeWriting.Settings;
	local reduceVeh=se.ReduceVehicleDamage;			-- if enabled, doing stunts with vehicles will make 8 times less damage on vehicles, but veh wont explode after fuel tank is shot
	local blockLawInfantries=se.InfantriesBlockLAWC4;	-- if enabled bazookas and C4 will make 0 dmg on infantries
	local dmg=hit.damage;
	local mult=1;
	
	local shooter=hit.shooter;
	local target=hit.target;
	local protecttype=GetProtectionKind(target,isVehicle);
	local ttlDistance=1;
	if(shooter and target)then
		ttlDistance=shooter:GetDistance(target.id);
	end
	local weapon,verifyWpn=hit.weapon;
	local wpnclass="Terrain";
	if(weapon)then
		wpnclass=weapon.class;
	end
	if wpnclass=="AACannon" and dmg==8190 then
		dmg=0
	end
	if hit.shooter.weapon and hit.type=="collision" then --prevent null kills!
		dmg=0;
	end
	if(g_gameRules.class=="PowerStruggle" and wpnclass~="Terrain")then
		local tk=System.GetCVar("g_friendlyfireratio");
		if(tostring(tk)=="0")then
			if(shooter and target)then
				if(g_gameRules.game:GetTeam(shooter.id)==g_gameRules.game:GetTeam(target.id))then
					if(weapon.vehicle and target~=shooter)then
						if(target.IsOnVehicle)then
							if(not target:IsOnVehicle())then
								return 0;
							end
						end
					else
						if(not weapon.vehicle and target~=shooter)then
							return 0;
						end
					end
				end
				if((weapon.vehicle or shooter==target) and isVehicle and reduceVeh)then
					dmg=dmg/8;
					odmg=dmg;
				end
			end
		end
	end
	
	if(not isVehicle)then mult=(1-g_gameRules:GetDamageAbsorption(hit.target, hit)); end
	
	dmg=dmg*mult;
	local odmg=dmg;
	
	local vwpnclass=wpnclass;
	if shooter and target then
		if shooter.inventory then
			local wpnId=shooter.inventory:GetCurrentItemId();
			if(wpnId)then
				verifyWpn=System.GetEntity(wpnId);
				vwpnclass=verifyWpn.class or wpnclass;
			end
		end
		if shooter.IsOnVehicle and target.IsOnVehicle and blockLawInfantries then
			if shooter~=target and not shooter:IsOnVehicle() and (vwpnclass=="LAW" or wpnclass=="c4explosive") and not target:IsOnVehicle() then
				Chat:SendToTarget(nil,shooter,"Do not use LAW/C4 on simple players!");
				return 0;
			end
		end
	end
	if(protecttype==1)then
		if(shooter)then
			Msg:SendToTarget(shooter,"You can't kill admin");
		end
		dmg=0;
	elseif(protecttype==2)then		
		dmg=dmg/4;		
	elseif(protecttype==3)then
		dmg=dmg/2;
	end
	if(AntiCheat:HitCheck(hit))then
		dmg=0;		
	end	
	--if(wpnclass=="FY71" and dmg>63)then dmg=0; odmg=0; end
	if(not isVehicle and target and target.actor)then
		if(target.actor:GetHealth()-dmg<=0)then
			if(shooter)then
				if(target)then
					if(not shooter.rKills)then
						shooter.rKills=0;
					end
					if(not target.rDeaths)then
						target.rDeaths=0;
					end
					if(shooter~=target)then
						shooter.rKills=shooter.rKills+1;
						if(target.Reward~=nil)then
							g_gameRules:AwardPPCount(shooter.id,target.Reward);
							Msg:SendToAll("    "..shooter:GetName().." killed "..target:GetName().." and gets reward "..target.Reward.." PP    ");
							target.Reward=nil;
						end
					end
					target.rDeaths=target.rDeaths+1;
				end
				if(SafeWriting.Settings.EnableStatistics and ((wpnclass or "")=="DSG1"))then --Sniper stats
					if(shooter.statsidx)then
						if(tonumber(ttlDistance or 0)>tonumber(SafeWriting.GlobalStorage.PlayerInfo.Players[shooter.statsidx].distance_kill or 0))then
							SafeWriting.GlobalStorage.PlayerInfo.Players[shooter.statsidx].distance_kill=tostring(ttlDistance);
						end
					end
				else
					if(not shooter.initStats)then
						shooter.initStats=true;
						shooter.dstkill=0;
					end
					if(ttlDistance>shooter.dstkill)then
						shooter.dstkill=ttlDistance;
					end
				end
				if(wpnclass=="DSG1" and ttlDistance>=300 and SafeWriting.Settings.DistanceKillRewardAllowed) then
					Msg:SendToTarget(shooter,SpecialFormat("        Distance kill: %s m!        ",tostring(ttlDistance)));
					if(g_gameRules.class=="PowerStruggle")then
					g_gameRules:AwardPPCount(shooter.id,math.ceil(ttlDistance));
					end
				end
				MakePluginEvent("OnKill",hit);
			end
		end
	end
	if(dmg~=odmg and shooter and target and protecttype~=1)then	
		Msg:SendToTarget(shooter,SpecialFormat("%s's health: %d",target:GetName(),(target.actor:GetHealth()-dmg)*(odmg/dmg)));
	end
	local funcs=SafeWriting.FuncContainer:GetFuncs("ProcessBulletDamage");
	if(funcs and protecttype==0)then
		local avgdmg=0;
		local count=0;
		for i,v in pairs(funcs) do
			avgdmg=avgdmg+v(hit,isVehicle);
			count=count+1;
		end
		return (avgdmg/count);
	end
	if not isVehicle then
		if shooter and (shooter.ShowTargetHealth or se.ShowTargetHealth) and target and target and target.actor and target.host and shooter.host and target~=shooter then
			local tgtHealth=math.floor(target.actor:GetHealth()-dmg);
			local tgtArmor=0;
			if target.actor:GetNanoSuitMode()==SafeWriting.NanosuitModes["armor"] then
				tgtArmor=math.floor(target.actor:GetNanoSuitEnergy()/2);
			end
			local healthBars=math.max(0,tgtHealth/5);
			local armorBars=math.max(0,tgtArmor/5);
			local armor=string.rep("|",armorBars);
			local health=string.rep("|",healthBars);
			Msg:SendToTarget(shooter,"Health: %20s [%4d hp] | Armor: %20s [%4d]","center",health,tgtHealth,armor,tgtArmor);
		end
	end
	return dmg;
end