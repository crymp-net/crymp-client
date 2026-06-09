GunGame={Enabled=false;};
GunGame.Ranks={
	{"SOCOM"},
	{"Shotgun"},
	{"SMG"},
	{"SCAR"},
	{"FY71"},
	{"DSG1"},
	{"AlienMount"},
	{"GaussRifle"},
	{"SCAR","FragGrenade","FragGrenade","FragGrenade"},
	{"FY71","FragGrenade","FragGrenade","EMPGrenade"},
	{"ScoutSingularity"},
	{"FragGrenade","FragGrenade","FragGrenade","EMPGrenade","EMPGrenade","SmokeGrenade","SmokeGrenade","Flashbang","Flashbang"},
};
GunGame.KillsForNextLvl=3;
function GunGame_SetEquipment(player,lvlup)
	if(not lvlup)then
		player.inventory:Destroy();
	else
		local invTable=player.inventory:GetInventoryTable();
		for i,iv in pairs(invTable) do
			local v=System.GetEntity(iv);
			if(v)then
				if(player.CanTake[v.class])then
					if(v.class~="Fists" and v.class~="OffHand" and v.class~="AlienCloak")then
						player.inventory:RemoveItem(v.id);
						System.RemoveEntity(v.id);
					end
				end
			end
		end
	end
	player.CanTake=nil;
	player.CanTake={};
	if(lvlup)then
		local pos=player:GetWorldPos();
		g_gameRules:CreateExplosion(player.id,sound,0,pos,g_Vectors.up,1,1,1,1,"explosions.cluster_bomb.impact",0.5, 0.5, 0.5, 0.5);
		g_gameRules.game:SendTextMessage(TextMessageCenter, "    !    You are level "..(player.rank).." now    !    ", TextMessageToClient, player.id);
	end
	local equip=GunGame.Ranks[player.rank or 1];
	if(not lvlup)then
		player.CanTake["Fists"]=true;
		player.CanTake["OffHand"]=true;
		player.CanTake["AlienCloak"]=true;
		ItemSystem.GiveItem("Fists", player.id, false);		
		ItemSystem.GiveItem("OffHand", player.id, false);		
		ItemSystem.GiveItem("AlienCloak", player.id, false);
	end	
	for i,e in ipairs(equip) do
		player.CanTake[e]=true;	
		ItemSystem.GiveItem(e, player.id, false);
		if(e=="DSG1") then
			player.CanTake["SniperScope"]=true;
			ItemSystem.GiveItem("SniperScope", player.id, false);			
		else
			player.CanTake["Reflex"]=true;	
			ItemSystem.GiveItem("Reflex", player.id, false);			
		end		
	end
end
function GunGame_CanTake(player,wpnclass)
	player.CanTake=player.CanTake or {};
	if(player.CanTake[wpnclass])then
		return true;
	else
		return false;
	end
end
function GunGame_EndWithWinner(player)
	g_gameRules:OnGameEnd(player.id);
end
function GunGame_InitMap()
	local ents=System.GetEntitiesByClass("CustomAmmoPickup");
	if(ents)then
		for i,v in ipairs(ents) do
			if(string.match(v:GetName(),"gren") or string.match(v:GetName(),"bang"))then
				System.RemoveEntity(v.id);
				Log("[GunGame] Deleting "..v:GetName());
			end
		end
	end	
	DisableChatCommand("tprand");
	DisableChatCommand("getsnipe");
	DisableChatCommand("veh");
end
function GunGame_EquipPlayer(actor, additionalEquip)
	GunGame_SetEquipment(actor,false);
end
function GunGame_OnItemPickedUp(itemId, actorId)
	local player=System.GetEntity(actorId);
	local wpn=System.GetEntity(itemId);
	if(not GunGame_CanTake(player,wpn.class))then
		if(wpn.class~="Fists" and wpn.class~="AlienCloak" and wpn.class~="OffHand")then
			player.inventory:RemoveItem(itemId);
			System.RemoveEntity(itemId);
		end
	end
end
function GunGame_OnActorHit(hit)
	hit.target.kills=hit.target.kills or 0;
	hit.target.deaths=hit.target.deaths or 0;
	hit.target.fistkills=hit.target.fistkills or 0;
	hit.target.rank=hit.target.rank or 1;
	hit.shooter.kills=hit.shooter.kills or 0;
	hit.shooter.deaths=hit.shooter.deaths or 0;
	hit.shooter.fistkills=hit.shooter.fistkills or 0;
	hit.shooter.rank=hit.shooter.rank or 1;
	local target=hit.target;
	local health=target.actor:GetHealth();
	local healthBefore=health;
	local hack=false;
	health = math.floor(health - ProcessDamageOfBullet(hit));
	local wpnclass="Unknown";
	if(hit.weapon)then
		wpnclass=hit.weapon.class or "Unknown";
	end
	if(health<=0)then
		if((wpnclass)=="Fists" and (hit.shooter.actor:GetNanoSuitMode()~=1))then
			hit.target.deaths=hit.target.deaths+1;
			if(hit.shooter~=hit.target)then
				hit.shooter.kills=hit.shooter.kills+1;
				hit.target.rank=math.max(1,hit.target.rank-1);
				hit.target.msg="    ):    Level down, because you were killed with fists    :(     ";
				local expectedRank=hit.shooter.rank+1;
				hit.shooter.rank=math.min(#(GunGame.Ranks),hit.shooter.rank+1);
				hit.shooter.fistkills=hit.shooter.fistkills+1;
				if(hit.shooter.rank~=expectedRank)then
					GunGame_EndWithWinner(hit.shooter);
				else
					GunGame_SetEquipment(hit.shooter,true);
				end
			end
		else
			hit.target.deaths=hit.target.deaths+1;
			if(hit.shooter~=hit.target)then
				hit.shooter.kills=hit.shooter.kills+1;
				if(hit.shooter.kills>0 and hit.shooter.kills%(GunGame.KillsForNextLvl)==0)then
					local expectedRank=hit.shooter.rank+1;
					hit.shooter.rank=math.min(#(GunGame.Ranks),hit.shooter.rank+1);
					if(hit.shooter.rank~=expectedRank)then
						GunGame_EndWithWinner(hit.shooter);
					else
						GunGame_SetEquipment(hit.shooter,true);
					end
				else
					g_gameRules.game:SendTextMessage(TextMessageCenter, "    Progress to next level: "..(hit.shooter.kills%(GunGame.KillsForNextLvl)).."/"..(GunGame.KillsForNextLvl).."    ", TextMessageToClient, hit.shooter.id);
				end
			end
		end
	end
	hit.target.actor:SetHealth(health);
	return (health<=0);
end
function GunGame_OnPlayerRevive(player)
	if(player)then
		if(not player.gotWelcomeMessage)then
			g_gameRules.game:SendTextMessage(TextMessageCenter, " Welcome to GunGame!", TextMessageToClient, player.id);
			Script.SetTimer(2500,function()
				g_gameRules.game:SendTextMessage(TextMessageCenter, " Rules: 3 kills => next level => new weapon", TextMessageToClient, player.id);
			end);
			Script.SetTimer(7500,function()
				g_gameRules.game:SendTextMessage(TextMessageCenter, " Kill with fists without being in strength mode => level up for you, level down for victim", TextMessageToClient, player.id);
			end);
			player.gotWelcomeMessage=true;
		else
			if(player.msg)then
				g_gameRules.game:SendTextMessage(TextMessageCenter, player.msg, TextMessageToClient, player.id);
				player.msg=nil;
			end
		end
	end
end
if(GunGame.Enabled)then
	SafeWriting.FuncContainer:AddFunc(GunGame_InitMap,"PrepareAll");
	SafeWriting.FuncContainer:AddFunc(GunGame_EquipPlayer,"EquipPlayer");
	SafeWriting.FuncContainer:AddFunc(GunGame_OnPlayerRevive,"OnPlayerRevive");
	SafeWriting.FuncContainer:AddFunc(GunGame_OnActorHit,"ProcessActorDamage");
	SafeWriting.FuncContainer:AddFunc(GunGame_OnItemPickedUp,"OnItemPickedUp");
end