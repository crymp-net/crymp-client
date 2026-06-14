function AttemptValidate(player,prof,uid,name,attempt)
	local url=urlfmt("/api/validate.php?prof=%s&uid=%s",prof,uid);
	local se=SafeWriting.Settings;
	attempt=attempt or 0;
	AsyncConnectHTTP(se.MasterHost or "crymp.org", url, "GET", 443, true, 15, function(c)
		local content,hdr,error=ParseHTTP(c);
		if not error then
			local err=string.find(c,"%Validation:Failed%",nil,true);
			if err then
				printf("Validation error: %s",tostring(err));
				player.profile=math.random(800000,1000000);
				player.waitingForAuth=nil;
				CheckPlayer(player);
			else
				printf("Validation success for %s/%s",name or "<unknown>",player:GetName() or "<unknown>");
				player.profile=prof;
				local se=SafeWriting.Settings;
				if(se.UseAuthentificationPassword and g_gameRules.class=="PowerStruggle")then
					player.IsAdminLogged=false;
					player.IsModeratorLogged=false;
					player.IsPremiumLogged=false;
				else
					if(se.Admins[player.profile])then
						player.IsAdminLogged=true;
					end
					if(se.Moderators[player.profile])then
						player.IsModeratorLogged=true;
					end
					if(se.Premiums[player.profile])then
						player.IsPremiumLogged=true;
					end
				end
				--RenamePlayer(player,name);
				player.waitingForAuth=nil;
				player.isSfwCl=true;
				_G["ValidIds"]=_G["ValidIds"] or {};
				_G["ValidIds"][uid]=prof;
				CheckPlayer(player);
			end
		else
			if attempt<4 then
				printf("Validation warning: %s , attempt: %d",tostring(error),attempt+1);
				AttemptValidate(player,prof,uid,name,attempt+1);
			else
				printf("Validation error: %s",tostring(error));
				player.profile=math.random(800000,1000000);
				player.waitingForAuth=nil;
				CheckPlayer(player);
			end
		end
	end);
end
AddChatCommand("validate",function(self,player,msg,prof,uid,name)
	player.WasChecked = false;
	Script.SetTimer(1,function()
		local se=SafeWriting.Settings;
		if SafeWriting.Settings.AllowMasterServer and prof and uid and name then
			local numpr=tonumber(prof);
			if numpr and uid=="200000" and numpr>=800000 and numpr<=1000000 then
				player.profile=prof;
				player.waitingForAuth=nil;
				player.isSfwCl = true;
				--RenamePlayer(player,name);
				CheckPlayer(player);
				return;
			end
			
			if _G["ValidIds"] and _G["ValidIds"][uid]==prof then
				player.waitingForAuth=nil;
				player.profile=prof;
				player.isSfwCl=true;
				--RenamePlayer(player,name);
				if(se.UseAuthentificationPassword and g_gameRules.class=="PowerStruggle")then
					player.IsAdminLogged=false;
					player.IsModeratorLogged=false;
					player.IsPremiumLogged=false;
				else
					if(se.Admins[player.profile])then
						player.IsAdminLogged=true;
					end
					if(se.Moderators[player.profile])then
						player.IsModeratorLogged=true;
					end
					if(se.Premiums[player.profile])then
						player.IsPremiumLogged=true;
					end
				end
				CheckPlayer(player);
				return;
			end
			AttemptValidate(player,prof,uid,name,0);
		end
	end);
end,{WORD,WORD,WORD});