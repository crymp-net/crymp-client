AddChatCommand("vote",function(self,player,msg,tpe,tgt)
	local allowMapVote=false;
	if not tpe then
		Chat:SendToTarget(nil,player,"Enter valid action - !vote kick");
	end
	if tpe~="kick" then
		if allowMapVote then
			if tpe~="map" then
				Chat:SendToTarget(nil,player,"Enter valid action - !vote kick or !vote map");
				return;
			end
		else
			Chat:SendToTarget(nil,player,"Enter valid action - !vote kick");
			return;
		end
	end
	
	local gd=SafeWriting.GlobalData;
	if(gd.__VoteInProgress)then
		Chat:SendToTarget(nil,player,"[[Prebieha hlasovanie]]");
		return;
	end
	if(not tgt)then
		if tpe=="kick" then
			Chat:SendToTarget(nil,player,"Enter valid player!");
		else
			Chat:SendToTarget(nil,player,"Enter valid map name!");
		end
		return;
	end
	local validmaps={};
	if g_gameRules.class=="PowerStruggle" then
		validmaps={
			"mesa","shore","beach","refinery","plantation"
		};
	else
		validmaps={
			"steelmill","quarry","armada","outpost"
		};
	end
	if tpe=="kick" then
		local pl=GetPlayerByName(tgt);
		if not pl then
			Chat:SendToTarget(nil,player,"Enter valid player!");
			return;
		end
		Chat:SendToAll(nil,"Player %s has started a kick vote for player %s, use !yes or !no to vote.",player:GetName(),pl:GetName());
		for i=1,12 do
			Script.SetTimer(i*5000,function()
				Msg:SendToAll("%s kick vote in progress - vote !yes, !no","center",pl:GetName());
			end);
		end
		KICKVOTETGT=pl;
		MakeSimpleVote(60,nil,nil,function(y,n)
			local total=g_gameRules.game:GetPlayers();
			if not total then total=0; else total=#total; end
			if (y+n)>=total*0.2 then
				if y>n then
					Chat:SendToAll(nil,"Kick vote was successful, %s was kicked",KICKVOTETGT:GetName());
					if KICKVOTETGT then
						PermaBanPlayer(KICKVOTETGT, "kick vote" , "kick vote", false, Time(1800));
					end
				else
					Chat:SendToAll(nil,"Kick vote failed, voted %.2f %% of players, yes: %d, no: %d",((y+n)*100)/total,y,n);
				end
			else
				Chat:SendToAll(nil,"Kick vote failed, only %.2f %% of players voted (minimum: 20%%).",((y+n)*100)/total);
			end
		end);
	else
		local valid=false;
		tgt=tgt:lower();
		for i,v in pairs(validmaps) do if v==tgt then valid=true; break; end end
		if not valid then
			Chat:SendToTarget(nil,player,"Enter valid map name!");
			return;
		end
		Chat:SendToAll(nil,"Player %s has started a map vote for map %s.",player:GetName(),tgt);
		MAPVOTETGT=tgt;
		local gs=SafeWriting.GlobalStorage;
		MakeSimpleVote(60,nil,nil,function(y,n)
			local total=g_gameRules.game:GetPlayers();
			if not total then total=0; else total=#total; end
			--if total==1 then total=2; end
			if (y+n)>=total*0.6 then
				if y>n then
					Chat:SendToAll(nil,"Map vote was successful, next map: %s",MAPVOTETGT);
					gs.ForceNextMap=true;
					gs.NextMap=nextmap;
				else
					Chat:SendToAll(nil,"Map vote failed, not enough votes (atleast 60 percent must be)");
				end
			end
		end);
	end
end,{WORD,TEXT},nil,"Starts kick voting, usage !vote kick playername");