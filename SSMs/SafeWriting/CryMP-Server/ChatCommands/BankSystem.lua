AllowBankSystem=false;
if AllowBankSystem then
	AddChatCommand("bank",function(self,player,msg)
		local profile = tonumber(player.profile);
		if profile >= 800000 and profile <= 1000000 then
			return Chat:SendToTarget(nil,player,"Sorry, you are not logged in to use bank");
		end
		local params=split(msg," ");
		local cmd=nil;
		local amount=nil;
		local playerPP=GetPlayerPP(player);
		params=CTableToLuaTable(params);
		if(#params==2 and params[2]=="status")then
			cmd="status";
		elseif(#params==3)then
			if(params[2]=="export")then
				cmd="export";
			elseif(params[2]=="import")then
				cmd="import";
			end
			amount=tonumber(params[3]);
		end
		--Log("#params: %d, cmd: %s, amount: %d",#params,cmd or "unknown",amount or -1);
		if(cmd)then
			local isu=true;
			local pli=SafeWriting.GlobalStorage.PlayerInfo.Players[(player.statsidx or -1)];
			if(not pli)then
				pli=player;
			end
			pli.in_bank=tonumber(pli.in_bank or 0);
			if(cmd=="export")then
				if(not amount)then
					return self:EnterValidPP(player);
				end
				if(amount<=0)then
					return self:MustBeGreaterThan0(player);
				end
				if(amount>(pli.in_bank or 0))then
					Chat:SendToTarget(nil,player,"Sorry, but you don't have enough pp in bank.");
					return;
				end
				pli.in_bank=tostring((pli.in_bank or 0)-amount);
				GivePoints(player,amount);
			elseif(cmd=="import")then
				if(not amount)then
					return self:EnterValidPP(player);
				end		
				if(amount<=0)then
					return self:MustBeGreaterThan0(player);
				end			
				if(amount>playerPP)then
					return self:NotEnoughPP(player);
				end
				if(pli.in_bank+amount>200000)then
					Chat:SendToTarget(nil,player,"Sorry, but bank limit is 200 000.");
					return;
				end
				if(IsCommandUsableForPlayer("bank import",player,180,true)==true)then
					pli.in_bank=tostring((pli.in_bank or 0)+amount);
					GivePoints(player,-amount);
				else
					isu=false;
				end
			end
			pli.in_bank=tostring(pli.in_bank);
			if(isu)then
				Chat:SendToTarget(nil,player,"You have "..(pli.in_bank or 0).." PP in bank");
			end
		end
	end,nil,nil,"Bank can store your pp and you can later get them back, usage: !bank <import/export/status> <amount>");
end