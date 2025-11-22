--Created 8.9.2012 by 'Zi;' as part of SSM SafeWriting project
--You can create any .lua file in this folder, you do not have to write commands to this file :)
AddChatCommand("killme",function(self,player,msg) g_gameRules:KillPlayer(player); end,{},{},"Makes you suicide");
AddChatCommand("lang",function(self,player,msg,l)
	if(l)then
		if(SafeWriting.Translator.__Languages[l])then
			player.lang=l;
			Chat:SendToTarget(nil,player,"[[LANGUAGE_WAS_CHANGED_TO]]",SafeWriting.Translator.__Languages[l][R.LANGUAGE_OBJECT]);
			if(SafeWriting.Translator.__Languages[l][R.TRANSLATED_BY])then
				Chat:SendToTarget(nil,player,SafeWriting.Translator.__Languages[l][R.TRANSLATED_BY]);
			end
		end
	end
end,{WORD},nil,"[[lang_info]]");
AddChatCommand("flare",function(self,player,msg)
	g_gameRules:CreateExplosion(player.id,weaponId,0,player:GetWorldPos(),g_Vectors.up,1,1,1,1,"explosions.flare.night_time",1.6, 1, 1, 1);
end,nil,nil,"[[flare_info]]");
AddChatCommand("firework",function(self,player,msg)
	g_gameRules:CreateExplosion(player.id,weaponId,0,player:GetWorldPos(),g_Vectors.up,1,1,1,1,"misc.extremly_important_fx.celebrate",1.6, 1, 1, 1);
end,nil,nil,"[[firework_info]]");
AddChatCommand("tprand",function(self,player,msg)
	if(self:IsUsable(player,120,true))then
		local randx,randy,randz;
		randx=math.random(0,1250);
		randy=math.random(0,1250);
		randz=100;
		player.portalTime=_time;
		local pos={
			x=randx+1250;
			y=randy+1250;
			z=randz+1250;
		};
		pos.z=System.GetTerrainElevation(pos);
		TeleportPlayer(player,pos);
	end
end,nil,nil,"[[tprand_info]]");
AddChatCommand("pm",function(self,player,msg,plplayer,text)
	if(not plplayer)then
		self:EnterValidPlayer(player);
		return;
	end	
	if(not text)then
		Chat:SendToTarget(nil,player,"[[ENTER_TEXT]]!");
		return;
	end
	Chat:SendToTarget(nil,player,"[[MESSAGE_SENT_TO_PLAYER]] ",plplayer:GetName());
	Chat:SendToTarget(player,plplayer,"[pm] "..text);
end,{PLAYER,TEXT},nil,"[[pm_info]]");
AddChatCommand("veh",function(self,player,msg,pt)
	local enabled=true;
	local psonly=false;
	if(enabled==false)then
		Chat:SendToTarget(nil,player,"[[COMMAND_BLOCKED]]!");
	else
		if(psonly and g_gameRules.class~="PowerStruggle")then
			Chat:SendToTarget(nil,player,"[[ONLY_ON_PS]]!");
			return;
		end
		if(self:IsUsable(player,30,true))then
			local t="land";
			local veh="US_ltv";
			local price=15;
			local spDist=10;			
			if(pt)then
				pt=string.lower(pt);
				if(pt=="water" or pt=="boat")then
					t="water";
					veh="Civ_speedboat";
					price=15;
					spDist=10;		
				elseif(pt=="hover")then
					t="hover";
					veh="US_hovercraft";
					price=25;
					spDist=10;			
				end
			else
				local pos=Spawn:CalculatePosition(player,10);
				pos.z=System.GetTerrainElevation(pos);
				local level,normal,flow=CryAction.GetWaterInfo(pos);
				if (level and level-1.9>=pos.z and pos.z-level<2.75) then
					veh="Civ_speedboat";
				end
			end
			if(g_gameRules.class=="PowerStruggle")then
				local playerPP=GetPlayerPP(player);
				if(playerPP<price)then
					Chat:SendToTarget(nil,player,"[[YOU_NEED_POINTS]]",price)
					return;
				end
				GivePoints(player,-price);
			end
			Spawn:Vehicle(player,veh,spDist,function(v)
				v.Properties.Respawn.bAbandon=1;
				v.Properties.Respawn.nAbandonTimer=30;
				v.lockowner=player.profile;
				v.vehicle:SetOwnerId(player.id);
				local driverSeat=nil;
				local seats=v.Seats;
				if(seats)then
					for i,seat in pairs(seats) do
						if(seat.isDriver)then
							driverSeat=i;
							break;
						end
					end
				end
				if driverSeat then
					Script.SetTimer(100,function()
						v.vehicle:EnterVehicle(player.id,driverSeat,false);
					end);
				end
			end,"MP",{
				properties={
					Respawn={
						bAbandon=1;
						nAbandonTimer=30;
					};
				}
			});
		end
	end
end,{WORD},nil,"[[veh_info]]");
AddChatCommand("getsnipe",function(self,player,msg)
	local psonly=true;
	local price=350;
	if(g_gameRules.class=="PowerStruggle")then
		local playerPP=GetPlayerPP(player);
		if(playerPP<price)then
			self:NeedPP(player,price);
			return;
		end
	end
	local wpnid=ItemSystem.GiveItem("DSG1",player.id,true);
	if(wpnid)then
		if(player.inventory:GetItemByClass("DSG1"))then
			AttachAttachments(wpnid,{"SniperScope"},player);
			GiveAmmo(player,"sniperbullet");
			GiveAmmo(player,"sniperbullet");
			if(g_gameRules.class=="PowerStruggle")then
				GivePoints(player,-price);
			end
		else
			Chat:SendToTarget(nil,player,"[[NOT_ENOUGH_SPACE]]");
			return;
		end
	end
end,nil,nil,"[[getsnipe_info]]");
AddChatCommand("name",function(self,player,msg,newname)
	if newname then
		RenamePlayer(player,newname,false);
	end
end,{TEXT},nil,"[[name_info]]");
AddChatCommand("commands",function(self,player,msg)
	local cmds={};
	for cmdname,other in pairs(SSMCMDS) do
		local cmdinfo="[[NO_DESCRIPTION]]";
		if(other.info)then
			cmdinfo=other.info;
		end
		if(HavePrivileges(player,cmdname))then
			cmds[#cmds+1]=cmdname;			
		end		
	end
	table.sort(cmds);
	local ce=SafeWriting.Settings.CommandsExtension or {'!'};
	if type(ce) ~= "table" then ce={tostring(ce)}; end
	ce=ce[1];
	local a,m,p,o={},{},{},{};
	for i,v in ipairs(cmds) do
		local cmd=SSMCMDS[v];
		if cmd.AdminOnly then a[#a+1]=v;
		elseif cmd.ModOnly or cmd.ModeratorOnly then m[#m+1]=v;
		elseif cmd.AdminModOnly then m[#m+1]=v;
		elseif cmd.PremiumOnly then p[#p+1]=v;
		else o[#o+1]=v; end
	end
	if #a>0 then
		Console:SendToTarget(player,"$7Admin commands:");
		for i,v in ipairs(a) do
			local cmd,cmdname=SSMCMDS[v],v;
			local cmdinfo="[[NO_DESCRIPTION]]";
			if cmd.info then cmdinfo=cmd.info; end
			Console:SendToTarget(player," $5%15s $1- $3"..cmdinfo,ce..cmdname);
		end
	end
	if #m>0 then
		Console:SendToTarget(player,"$8Moderator commands:");
		for i,v in ipairs(m) do
			local cmd,cmdname=SSMCMDS[v],v;
			local cmdinfo="[[NO_DESCRIPTION]]";
			if cmd.info then cmdinfo=cmd.info; end
			Console:SendToTarget(player," $6%15s $1- $3"..cmdinfo,ce..cmdname);
		end
	end
	if #p>0 then
		Console:SendToTarget(player,"$9Premium commands:");
		for i,v in ipairs(p) do
			local cmd,cmdname=SSMCMDS[v],v;
			local cmdinfo="[[NO_DESCRIPTION]]";
			if cmd.info then cmdinfo=cmd.info; end
			Console:SendToTarget(player," $7%15s $1- $3"..cmdinfo,ce..cmdname);
		end
	end
	if #o>0 then
		Console:SendToTarget(player,"$5User commands:");
		for i,v in ipairs(o) do
			local cmd,cmdname=SSMCMDS[v],v;
			local cmdinfo="[[NO_DESCRIPTION]]";
			if cmd.info then cmdinfo=cmd.info; end
			Console:SendToTarget(player," $3%15s $1- $3"..cmdinfo,ce..cmdname);
		end
	end
	Msg:SendToTarget(player,__qt(player.lang,R.OPEN_CONSOLE));
end,nil,nil,"[[commands_info]]");
AddChatCommand("myprofile",function(self,player,msg)
	local ip=player.ip or "[[UNKNOWN]]";
	if(string.len(ip)>16)then
		ip="[[UNKNOWN]]";
	end
	Chat:SendToTarget(nil,player,"Profile: "..player.profile);
	Chat:SendToTarget(nil,player,"Host: "..player.host);
	Chat:SendToTarget(nil,player,"IP: "..ip);
	Chat:SendToTarget(nil,player,"ChannelID: "..player.channelId);
	Chat:SendToTarget(nil,player,"Port: "..player.port);
end,nil,nil,"[[myprofile_info]]");
AddChatCommand("reset",function(self,player,msg)
	SetPlayerScore(player,0,0);
	Chat:SendToTarget(nil,player,"[[SCORE_WAS_RESET]]");
end,nil,nil,"[[reset_info]]");
AddChatCommand("version",function(self,player,msg)
	Chat:SendToTarget(nil,player,"SSM SafeWriting "..(SafeWriting.Version or " < 1.8.5"));
end,nil,nil,"[[version_info]]");
AddChatCommand("lock",function(self,player,msg)
	if(player:IsOnVehicle())then
		local vehicle = System.GetEntity(player.actor:GetLinkedVehicleId());
		if(vehicle)then
			local isDriver=vehicle:IsDriver(player.id);
			if(isDriver)then
				vehicle.vehicle:SetOwnerId(player.id);
				vehicle.lockowner=player.profile;
				Chat:SendToTarget(nil,player,"[[VEHICLE]] [[LOCKED]]");
			else
				Chat:SendToTarget(nil,player,"[[YOU_MUST_BE_DRIVER]]!");
			end
		end
	else
		self:MustBeInVehicle(player);
	end
end,nil,nil,"[[lock_info]]");
AddChatCommand("unlock",function(self,player,msg)
	if(player:IsOnVehicle())then
		local vehicle = System.GetEntity(player.actor:GetLinkedVehicleId());
		if(vehicle)then
			local isDriver=vehicle:IsDriver(player.id);
			if(isDriver)then
				vehicle.vehicle:SetOwnerId(NULL_ENTITY);
				vehicle.lockowner=nil;
				Chat:SendToTarget(nil,player,"[[VEHICLE]] [[UNLOCKED]].");
			else
				Chat:SendToTarget(nil,player,"[[YOU_MUST_BE_DRIVER]]!");
			end
		end
	else
		self:MustBeInVehicle(player);
	end
end,nil,nil,"[[unlock_info]]");
AddChatCommand("time",function(self,player,msg)
	local tnow = os.date("*t");
	Chat:SendToTarget(nil,player,"[[TIME_NOW]]: "..tnow.hour..":"..tnow.min.." (GMT+"..GetTimeZone()..")");
end,nil,nil,"[[time_info]]");
AddChatCommand("reward",function(self,player,msg,kplayer,pp)
	if(g_gameRules.class~="PowerStruggle")then
		Chat:SendToTarget(nil,player,"[[ONLY_ON_PS]]");
		return;
	end
	local playerPP=GetPlayerPP(player);
	if(not kplayer)then
		self:EnterValidPlayer(player);
		return;
	end
	if(pp)then
		pp=tonumber(pp);
	end
	if(not pp)then
		self:EnterValidPP(player);
		return;
	elseif(pp<=0)then
		self:MustBeGreaterThan0(player);
		return;
	elseif(pp>playerPP)then
		self:NotEnoughPP(player);
		return;
	end
	if(kplayer.Reward)then
		kplayer.Reward=kplayer.Reward+pp;
	else
		kplayer.Reward=pp;
	end
	GivePoints(player,-pp);
	Msg:SendToAll("    Player "..player:GetName().." has offered reward of "..pp.." for "..kplayer:GetName().."'s death (total reward: "..kplayer.Reward..")    ");
end,{PLAYER,INT},nil,"[[reward_info]]");
AddChatCommand("transfer",function(self,player,msg,kplayer,pp)
	if(g_gameRules.class~="PowerStruggle")then
		Chat:SendToTarget(nil,player,"[[ONLY_ON_PS]]");
		return;
	end
	local playerPP=GetPlayerPP(player);
	if(not kplayer)then
		self:EnterValidPlayer(player);
		return;
	end
	if(pp)then
		pp=tonumber(pp);
	end
	if(not pp)then
		self:EnterValidPP(player);
		return;
	elseif(pp<=0)then
		self:MustBeGreaterThan0(player);
		return;
	elseif(pp>playerPP)then
		self:NotEnoughPP(player);
		return;
	end
	GivePoints(player,-pp);
	GivePoints(kplayer,pp);
end,{PLAYER,INT},nil,"[[transfer_info]]");
AddChatCommand("ask",function(self,player,msg,question)
	local gd=SafeWriting.GlobalData;
	if(gd.__VoteInProgress)then
		Chat:SendToTarget(nil,player,"[[VOTING_IN_PROGRESS]]");
		return;
	end
	if(not question)then
		self:EnterText(player)
		return;
	end
	Chat:SendToAll(nil,"Question: "..question);
	Chat:SendToAll(nil,"Use !yes to vote for yes and !no to vote for no");
	MakeSimpleVote(45,function(n,w)
		Chat:SendToAll(nil,"Result of question: "..question)
		Chat:SendToAll(nil,"Yes: "..n);
	end,function(n,w)
		Chat:SendToAll(nil,"No: "..n);
	end);
end,{TEXT},nil,"[[ask_info]]");
AddChatCommand("yes",function(self,player,msg)
	local gd=SafeWriting.GlobalData;
	if(not gd.__VoteInProgress)then
		Chat:SendToTarget(nil,player,"[[NO_VOTING_IN_PROGRESS]]");
		return;
	end
	if(gd.__YesVotes[player.profile] or gd.__NoVotes[player.profile])then
		Chat:SendToTarget(nil,player,"[[ALREADY_VOTED]]");
		return;
	end
	gd.__YesVotes[player.profile]=true;
	Chat:SendToTarget(nil,player,"[[SUCCESSFULY_VOTED_FOR]] ,[[YES]]'");
end,nil,nil,"[[yes_info]]");
AddChatCommand("no",function(self,player,msg)
	local gd=SafeWriting.GlobalData;
	if(not gd.__VoteInProgress)then
		Chat:SendToTarget(nil,player,"[[NO_VOTING_IN_PROGRESS]]");
		return;
	end
	if(gd.__YesVotes[player.profile] or gd.__NoVotes[player.profile])then
		Chat:SendToTarget(nil,player,"[[ALREADY_VOTED]]");
		return;
	end
	gd.__NoVotes[player.profile]=true;
	Chat:SendToTarget(nil,player,"[[SUCCESSFULY_VOTED_FOR]] ,[[NO]]'");
end,nil,nil,"[[no_info]]");
AddChatCommand("spawninfo",function(self,player,msg)
	Chat:SendToTarget(nil,player,string.format(SafeWriting.Settings.WelcomeMessage,player:GetName()));
	if(SafeWriting.Settings.OtherWelcomeMessages)then
		for i,v in pairs(SafeWriting.Settings.OtherWelcomeMessages) do
			Chat:SendToTarget(nil,player,v);
		end
	end
end,nil,nil,"[[spawninfo_info]]");
-----------------------------------
--Rank needed commands:
-----------------------------------
AddChatCommand("snow",function(self,player,msg)
	g_gameRules:CreateExplosion(player.id,weaponId,25,player:GetWorldPos(),g_Vectors.up,1,1,1,1,"snow.snow.snow",1.6, 1, 1, 1);
end,nil,{AdminOnly=true;},"[[snow_info]]");
AddChatCommand("timescale",function(self,player,msg,sc)
	sc=sc or 1;
	System.ExecuteCommand("time_scale "..sc);
end,{NUMBER},{AdminOnly=true;},"[[timescale_info]]");
AddChatCommand("admin",function(self,player,msg)
	local plname=player:GetName();
	local admintag=SafeWriting.Settings.AdminTag;
	if(plname:sub(0,string.len(admintag))==admintag)then
		RenamePlayer(player,plname:sub(string.len(admintag)+1),true);
	else
		RenamePlayer(player,admintag..plname,true);
	end
end,nil,{AdminOnly=true;},"[[admin_info]]");
AddChatCommand("moderator",function(self,player,msg)
	local plname=player:GetName();
	local tag=SafeWriting.Settings.ModeratorTag;
	if(plname:sub(0,string.len(tag))==tag)then
		RenamePlayer(player,plname:sub(string.len(tag)+1),true);
	else
		RenamePlayer(player,tag..plname,true);
	end
end,nil,{ModOnly=true;},"[[moderator_info]]");
AddChatCommand("premium",function(self,player,msg)
	local plname=player:GetName();
	local tag=SafeWriting.Settings.PremiumTag;
	if(plname:sub(0,string.len(tag))==tag)then
		RenamePlayer(player,plname:sub(string.len(tag)+1),true);
	else
		RenamePlayer(player,tag..plname,true);
	end
end,nil,{PremiumOnly=true;},"[[premium_info]]");
AddChatCommand("mute",function(self,player,msg,kplayer,reason)
	if(not kplayer)then
		self:EnterValidPlayer(player);
		return;
	end
	kplayer.IsMuted=true;
	if(reason)then
		Chat:SendToTarget(nil,kplayer,"You have been muted, reason: "..reason);
	end
end,{PLAYER,TEXT},{AdminModOnly=true;},"[[mute_info]]");
AddChatCommand("unmute",function(self,player,msg,kplayer)
	if(not kplayer)then
		self:EnterValidPlayer(player);
		return;
	end
	kplayer.IsMuted=false;
	RemovePlayerProperty(player, "MutePlayer");
end,{PLAYER},{AdminModOnly=true;},"[[unmute_info]]");
AddChatCommand("muteex", function(self,player,msg,kplayer,t)
	if(not kplayer)then
		self:EnterValidPlayer(player);
		return;
	end
	SetPlayerProperty(kplayer, player, "MutePlayer", "UnmutePlayer", t or 0);
end,{PLAYER,TIME},{AdminModOnly=true;},  "Mutes player for certain time, usage !muteex <player> <1h30min>");
AddChatCommand("kick",function(self,player,msg,kplayer,reason)
	if(not kplayer)then
		self:EnterValidPlayer(player);
		return;
	end
	local originalTimeout=System.GetCVar("ban_timeout");
	KickPlayer(kplayer,reason);
end,{PLAYER,TEXT},{AdminModOnly=true;},"[[kick_info]]");
AddChatCommand("ban",function(self,player,msg,kplayer,reason)
	if(not kplayer)then
		self:EnterValidPlayer(player);
		return;
	end
	BanPlayer(kplayer,reason or("You were banned from this server by "..player:GetName()));
end,{PLAYER,TEXT},{AdminModOnly=true;},"[[ban_info]]");
AddChatCommand("permaban",function(self,player,msg,kplayer,reason)
	if(not kplayer)then
		self:EnterValidPlayer(player);
		return;
	end
	PermaBanPlayer(kplayer,reason or  "permabanned by "..player:GetName(),player:GetName(),false);
end,{PLAYER,TEXT},{AdminOnly=true;},"[[permaban_info]]");
AddChatCommand("tempban",function(self,player,msg,kplayer,t,reason)
	if(not kplayer)then
		self:EnterValidPlayer(player);
		return;
	end
	t = t or 3600*48;
	local name=kplayer:GetName();
	PermaBanPlayer(kplayer,reason or "tempbanned by "..player:GetName(),player:GetName(),false,Time(t));
	Chat:SendToTarget(nil,player,"Successfuly banned %s for %d hours for %s",name,t/3600,reason or "no reason");
end,{PLAYER,TIME,TEXT},{AdminModOnly=true;},"Temporarely bans a player",true);
AddChatCommand("cheateradd",function(self,player,msg,kplayer,reason)
	if(not kplayer)then
		self:EnterValidPlayer(player);
		return;
	end
	PermaBanPlayer(kplayer,reason or "permabanned by "..player:GetName(),player:GetName(),true);
end,{PLAYER,TEXT},{AdminModOnly=true;},"[[cheateradd_info]]");
AddChatCommand("reloadscripts",function(self,player,msg)
	SetReloadFlag();
	System.ExecuteCommand("exec sfw.cfg");
	UnsetReloadFlag();
end,nil,{AdminOnly=true;},"[[reloadscripts_info]]");
AddChatCommand("say",function(self,player,_msg,msg)
	local text=nil;
	local kind=nil;
	if(string.find(msg,"-k:"))then
		text,kind=string.match(msg,"(.*)-k:(.*)");
	else
		text=string.match(msg,"(.*)");
	end
	if(not text)then
		Chat:SendToTarget(nil,player,"[[ENTER_TEXT]]!");
		return ;
	end
	Msg:SendToAll(text,kind);
end,{TEXT},{AdminModOnly=true;},"[[msg_info]]");
AddChatCommand("players",function(self,player,msg)
	local players=g_gameRules.game:GetPlayers();
	Console:SendToTarget(player,SpecialFormat("${t:PlayersListHdr1|9} %4s ${t:PlayersListHdr2|8}%26s ${t:PlayersListHdr1|9}%12s ${t:PlayersListHdr2|8}%16s ${t:PlayersListHdr1|9}%s","ID","Name","ProfileID","IP","Host"));
	for i,iplayer in ipairs(players) do
		if(iplayer)then
			local ip=iplayer.ip;
			if(string.len(ip)>16)then --if ip same as hostname
				ip="[[UNKNOWN]]";
			end
			Console:SendToTarget(player,SpecialFormat("${t:PlayersList1|3} %4s ${t:PlayersList2|6}%26s ${t:PlayersList1|3}%12s ${t:PlayersList2|6}%16s ${t:PlayersList1|3}%s",iplayer.channelId,iplayer:GetName(),iplayer.profile,ip,iplayer.host));
		end
	end
	Msg:SendToTarget(player,__qt(player.lang,R.OPEN_CONSOLE));
end,nil,{AdminModOnly=true;},"[[players_info]]");
AddChatCommand("spawn",function(self,player,msg,distance,class)
	distance=tonumber(distance);
	class=tostring(class);
	if not distance then
		self:EnterValidDistance(player);
		return;
	end
	if not class then
		self:EnterValidClass(player);
		return;
	end
	Spawn:Entity(player,class,distance);
end,{NUMBER,TEXT},{AdminOnly=true;},"[[spawn_info]]");
AddChatCommand("mapvote",function(self,player,msg)
	SafeWriting.GlobalStorage.MapVotes={};
	SafeWriting.GlobalStorage.StartedMapVoting=true;
	SafeWriting.GlobalStorage.MapVotingInProgress=true;
	SafeWriting.GlobalStorage.MapVotingStartTime=_time;
	local ce=SafeWriting.Settings.CommandsExtension or {'!'};
	if type(ce) ~= "table" then ce={tostring(ce)}; end
	ce=ce[1];
	Chat:SendToAll(nil,"Map voting has started, please vote for your favorite maps");
	for i,v in pairs(SafeWriting.Settings.GameModes)do
		if(i==g_gameRules.class)then
			for q,w in pairs(v) do
				for n,u in pairs(w) do
					SafeWriting.GlobalStorage.MapVotes[n]={};
					Chat:SendToAll(nil,SpecialFormat(" "..ce.."%s for map %s",u.map,u.name));
					SSMCMDS[u.map]={info="Votes for map "..u.name;DotFunc=true};
					SSMCMDS[u.map].func = function(iplayer, imsg)
						for k,j in pairs(SafeWriting.GlobalStorage.MapVotes) do
							for o,p in pairs(SafeWriting.GlobalStorage.MapVotes[k])do
								if(o==iplayer and p==true)then
									Chat:SendToTarget(nil,iplayer,"[[ALREADY_VOTED]]!");
									return;
								end
							end
						end
						Chat:SendToTarget(nil,iplayer,"[[SUCCESSFULY_VOTED_FOR]] "..u.name);
						SafeWriting.GlobalStorage.MapVotes[n][iplayer]=true;
					end
				end   
			end
		end
	end
end,nil,{AdminModOnly=true;},"[[mapvote_info]]");
AddChatCommand("bring",function(self,player,msg,kplayer)
	if(not kplayer)then
		self:EnterValidPlayer(player);
		return;
	end
	local mypos=player:GetWorldPos();
	TeleportPlayer(kplayer,mypos);
end,{PLAYER},{AdminModOnly=true;},"[[bring_info]]");
AddChatCommand("behind",function(self,player,msg,kplayer)
	if(not kplayer)then
		self:EnterValidPlayer(player);
		return;
	end
	local hispos=kplayer:GetWorldPos();
	TeleportPlayer(player,hispos);
end,{PLAYER},{AdminModOnly=true;},"[[behind_info]]");
AddChatCommand("settime",function(self,player,msg,newtime)
	if(newtime==nil)then
		self:EnterValidValue(player);
		return;
	end
	ForceSet("e_time_of_day", tostring(newtime))
	Chat:SendToTarget(nil,player,"[[TIME_WAS_CHANGED_TO]] "..newtime);
end,{NUMBER},{AdminOnly=true;},"[[settime_info]]");
AddChatCommand("setgravity",function(self,player,msg,val)
	if(val==nil)then
		self:EnterValidValue(player);
		return;
	end
	ForceSet("p_gravity_z",tostring(val));
	Chat:SendToTarget(nil,player,"[[GRAVITATION_WAS_CHANGED_TO]] "..val);
end,{NUMBER},{AdminOnly=true;},"[[setgravity_info]]");
AddChatCommand("setcvar",function(self,player,msg,cvar,value)
	if(cvar==nil)then
		self:EnterValidCommand(player);
		return;
	end
	ForceSet(tostring(cvar) or "",tostring(value) or "");
	Chat:SendToTarget(nil,player,"[[SUCCESSFULY_DONE]]");
end,{WORD,TEXT},{AdminOnly=true;},"[[setcvar_info]]");
AddChatCommand("initrealtime",function(self,player,msg)
	local tspeed=1/3600;		
	local t=os.date("*t");
	local tminutes=t.min;
	local thours=t.hour;
	if(tminutes>0)then
			settime=thours+1/(60/(tminutes));
	else 
		settime=thours;
	end
	ForceSet("e_time_of_day",tostring(settime));
	ForceSet("e_time_of_day_speed",tostring(tspeed));
end,nil,{AdminOnly=true;},"[[initrealtime_info]]");
AddChatCommand("teleport",function(self,player,msg,_player,dir,y,z)
	if(not _player)then
		self:EnterValidPlayer(player);
		return 0;
	end
	local _pos=player:GetWorldPos();
	if(dir)then
		if(dir=="up" and y)then
			_pos.z=_pos.z+y;
		elseif(dir=="fwd" and y)then
			_pos=Spawn:CalculatePosition(player,y);
		else
			_pos.x=tonumber(dir);
			_pos.y=y or 512;
			_pos.z=z or System.GetTerrainElevation(_pos);
		end
	end
	TeleportPlayer(_player,_pos);
end,{PLAYER,WORD,NUMBER,NUMBER},{AdminOnly=true;},"[[teleport_info]]");
AddChatCommand("give",function(self,player,msg,tpe,_player,pp,cp)
	if(not tpe)then
		Chat:SendToTarget(nil,player,"[[ENTER_VALID_ACTION]] - item/points");
		return 0;
	end
	if(tpe~="points" and tpe~="item")then
		Chat:SendToTarget(nil,player,"[[ENTER_VALID_ACTION]] - item/points");
		return 0;
	end
	if(not _player)then
		self:EnterValidPlayer(player);
		return 0;
	end
	if(not pp)then
		self:EnterValidValue(player);
		return 0;
	end
	if(tpe=="item")then
		GiveItem(_player,pp);	--pp is here name of item!
	elseif(tpe=="points")then
		if(g_gameRules.class~="PowerStruggle")then
			Chat:SendToTarget(nil,player,"You can't award points in "..g_gameRules.class.."!");
			return 0;
		end
		pp=tonumber(pp or 0) or 0;
		cp=cp or 0;
		GivePoints(_player,pp,tonumber(cp));
	end
	return 1;
end,{WORD,PLAYER,WORD,INT},{AdminOnly=true;},"[[give_info]]");
AddChatCommand("exec",function(self,player,msg,cmd)
	if(not cmd)then
		self:EnterValidCommand(player);
	else
		System.ExecuteCommand(cmd);
	end
end,{TEXT},{AdminOnly=true;},"[[exec_info]]");
AddChatCommand("removeclass",function(self,player,msg,class)
	if not class then return; end
	local ents=System.GetEntitiesByClass(class);
	if(ents)then
		for i,v in pairs(ents) do
			System.RemoveEntity(v.id);
		end
	end
end,{TEXT},{AdminOnly=true;},"[[removeclass_info]]");
AddChatCommand("removename",function(self,player,msg,class)
	if not class then return; end
	local ent=System.GetEntityByName(class);
	if(ent)then
		System.RemoveEntity(ent.id);
	end
end,{TEXT},{AdminOnly=true;},"[[removename_info]]");
AddChatCommand("makejail",function(self,player,msg)
	CreateJail(player:GetPos(),10,24);
end,nil,{AdminOnly=true;},"[[makejail_info]]");
AddChatCommand("jail",function(self,player,msg,kplayer,reason)
	if(not kplayer)then
		self:EnterValidPlayer(player);
		return;
	end
	if(kplayer.isScriptJailed)then
		Chat:SendToTarget(nil,player,"[[ALREADY_JAILED]]!");
		return;
	end
	if(reason)then
		Chat:SendToTarget(nil,kplayer,"[[YOU_WERE_JAILED]], [[REASON]]: "..reason);
	end
	local ent=System.GetEntityByName("JailTrolley");
	if(not ent)then
		Chat:SendToTarget(nil,player,"Jail was not found, please use !makejail to create one");
		return;
	end
	kplayer.beforeJailedPos=kplayer:GetPos();
	kplayer.isScriptJailed=true;
	kplayer.inventory:Destroy();
	local pos=ent:GetPos();
	pos.z=pos.z-9;
	TeleportPlayer(kplayer,pos);
end,{PLAYER,TEXT},{AdminModOnly=true;},"[[jail_info]]");
AddChatCommand("unjail",function(self,player,msg,kplayer,reason)
	if(not kplayer)then
		self:EnterValidPlayer(player);
		return;
	end
	if(kplayer.isScriptJailed)then
		kplayer.isScriptJailed=false;
		g_gameRules:EquipPlayer(kplayer,nil);
		TeleportPlayer(kplayer,kplayer.beforeJailedPos);
		if(reason)then
			Chat:SendToTarget(nil,kplayer,"[[YOU_WERE_RELEASED]], [[REASON]]: "..reason);
		end
	else
		Chat:SendToTarget(nil,player,"[[NOT_JAILED]]!");
	end
end,{PLAYER,TEXT},{AdminModOnly=true;},"[[unjail_info]]");
AddChatCommand("team",function(self,player,msg,kplayer,team)
	if(not kplayer)then
		self:EnterValidPlayer(player);
		return;
	end
	if(team)then
		if(team=="us")then
			team=2;
		elseif(team=="nk")then
			team=1;
		else
			team=tonumber(team);
		end
	end
	ChangeTeam(kplayer,team);
end,{PLAYER,WORD},{AdminModOnly=true;},"[[team_info]]");
AddChatCommand("rename",function(self,player,msg,kplayer,newName)
	if(not kplayer)then
		self:EnterValidPlayer(player);
		return;
	end
	g_gameRules.game:RenamePlayer(kplayer.id,newName);
end,{PLAYER,TEXT},{AdminModOnly=true;},"[[rename_info]]");
AddChatCommand("kickid",function(self,player,msg,id,reason)
	local kplayer=g_gameRules.game:GetPlayerByChannelId(id or -1)
	if(not kplayer)then
		self:EnterValidPlayer(player);
		return;
	end
	local originalTimeout=System.GetCVar("ban_timeout");
	KickPlayer(kplayer,reason);
end,{INT,TEXT},{AdminModOnly=true;});
AddChatCommand("banid",function(self,player,msg,id,reason)
	local kplayer=g_gameRules.game:GetPlayerByChannelId(id or -1)
	if(not kplayer)then
		self:EnterValidPlayer(player);
		return;
	end
	BanPlayer(kplayer,reason or("banned by "..player:GetName()));
end,{INT,TEXT},{AdminModOnly=true;});
AddChatCommand("permabanid",function(self,player,msg,id,reason)
	local kplayer=g_gameRules.game:GetPlayerByChannelId(id or -1)
	if(not kplayer)then
		self:EnterValidPlayer(player);
		return;
	end
	PermaBanPlayer(kplayer,reason or "permabanned by "..player:GetName(),player:GetName(),false);
end,{INT,TEXT},{AdminOnly=true;});
AddChatCommand("cheateraddid",function(self,player,msg,id,reason)
	local kplayer=g_gameRules.game:GetPlayerByChannelId(id or -1)
	if(not kplayer)then
		self:EnterValidPlayer(player);
		return;
	end
	PermaBanPlayer(kplayer,reason or "permabanned by "..player:GetName(),player:GetName(),true);
end,{INT,TEXT},{AdminModOnly=true;});
AddChatCommand("tempbanid",function(self,player,msg,id,t,reason)
	local kplayer=g_gameRules.game:GetPlayerByChannelId(id or -1)
	if(not kplayer)then
		self:EnterValidPlayer(player);
		return;
	end
	t = t or 3600*48;
	local name=kplayer:GetName();
	PermaBanPlayer(kplayer,reason or ("tempbanned by"..player:GetName()),player:GetName(),false,Time(t));
	Chat:SendToTarget(nil,player,"Successfuly banned %s for %d hours for %s",name,t,reason or "no reason");
end,{INT,TIME,TEXT},{AdminModOnly=true;});