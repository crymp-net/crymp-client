AddChatCommand("givepp",function(self,sender,msg,players,amount,reason)
	if(not players)then self:EnterValidPlayer(sender); return; end
	if(#players==0)then self:EnterValidPlayer(sender); return; end
	if(not amount)then self:EnterValidPP(sender); return; end
	for i,v in pairs(players)do
		GivePoints(v,amount);
	end
	if(reason)then
		Msg:SendToAll("Player %s gave %d PP to all, reason: %s","info",sender:GetName(),amount,reason);
	end
end,{PLAYERS,INT,TEXT},{AdminOnly=true;},"Gives X pp to players",true);
AddChatCommand("giveitem",function(self,sender,msg,players,item,reason)
	if(not players)then self:EnterValidPlayer(sender); return; end
	if(#players==0)then self:EnterValidPlayer(sender); return; end
	if(not item)then self:EnterValidClass(sender); return; end
	for i,v in pairs(players)do
		GiveItem(v,item);
	end
	if(reason)then
		Msg:SendToAll("Player %s gave %s, reason: %s","info",sender:GetName(),item,reason);
	end
end,{PLAYERS,WORD,TEXT},{AdminOnly=true;},"Gives item to players",true);
AddChatCommand("save",function(self,player)
	if g_gameRules.class=="PowerStruggle" then
		if HasEnoughPP(player,200) then
			GivePoints(player,-200)
		else return self:NeedPP(player,200); end
	end
	player._savedPos = player:GetPos();
	player._savedAngles = player:GetAngles();
	Chat:SendToTarget(player,"Position successfuly saved, use !load to teleport here later");
end,nil,nil,"Saves your current position");

AddChatCommand("load",function(self,player)
	if not player._savedPos then
		Chat:SendToTarget(player,"Please, use !save to save position first!");
		return;
	end
	if self:IsUsable(player,60,true) then
		if g_gameRules.class=="PowerStruggle" then
			if HasEnoughPP(player,100) then
				GivePoints(player,-100)
			else return self:NeedPP(player,100); end
		end
		Teleport(player,player._savedPos,player._savedAngles);
		Chat:SendToTarget(player,"Successfuly teleported to your saved position");
	end
end,nil,nil,"Saves your current position");

AddChatCommand("addtime",function(self,player,msg,t)
	t = t or 0;
	local orig=System.GetCVar("g_timelimit");
	local remain=g_gameRules.game:GetRemainingGameTime();
	System.SetCVar("g_timelimit",(remain/60)+t);
	g_gameRules.game:ResetGameTime();
	System.SetCVar("g_timelimit",orig);
end,{NUMBER},{AdminOnly=true;});