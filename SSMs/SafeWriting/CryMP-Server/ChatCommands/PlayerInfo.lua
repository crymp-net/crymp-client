--Created 27.12.2012 by 'Zi;' as part of SSM SafeWriting project
function PlayerStatsSort(a,b)
	if(_G["CustomSort"])then
		return tonumber(a[(_G["CustomSortType"])])>tonumber(b[(_G["CustomSortType"])]);
	else
		return a.kills>b.kills;
	end
end
function GenerateTop(referer,sorttype)
	local trf=true;
	if(not referer)then
		trf=false;
		referer={Players={}};
	end
	if(not SafeWriting.GlobalStorage.PlayerInfo)then
		Log("No PlayerInfo, trying to load it again!");
		LoadPlayerInfo();
		if(not SafeWriting.GlobalStorage.PlayerInfo)then
			Log("Operation to load player info again failed!");
			return;
		end
	end
	MergeTables(referer,SafeWriting.GlobalStorage.PlayerInfo);	
	for i,v in pairs(referer.Players) do
		local player=GetPlayerByProfile(v.id);
		if(player)then
			local kills,deaths=GetRealPlayerScore(player);
			local tkills=(v.kills or 0)+kills-(player.rOffsetKills or 0);
			local tdeaths=(v.deaths or 0)+deaths-(player.rOffsetDeaths or 0);
			local tpltime=((v.pltime or 0)+(_time-player.connecttime))-(player.rOffsetTime or 0);
			v.kills=tkills;
			v.deaths=tdeaths;
			v.pltime=tpltime;
		end
	end
	table.sort(referer.Players,sorttype);
	if(trf~=true)then
		return referer;
	end
end
function GetPlayersInfo(player)
	local profile = tonumber(player.profile)
	if profile >= 800000 and profile <= 1000000 then return nil,nil,nil,nil; end
	if(SafeWriting.GlobalStorage.PlayerInfo==nil)then
		SafeWriting.GlobalStorage.PlayerInfo={};
		return nil;
	end
	local PlInfo=SafeWriting.GlobalStorage.PlayerInfo.Players;
	for i,v in pairs(PlInfo)do
		if(tostring(v.id)==tostring(player.profile))then
			return i,v.pltime,v.kills,v.deaths; --idx,time,kills,deaths
		end
	end
	return nil,nil,nil,nil;
end
function LoadPlayerInfo()
	_LoadPlayerInfo();
	OptiScores();
	SaveAllPlayersInfo();
	_LoadPlayerInfo();
	local players=g_gameRules.game:GetPlayers();
	if(players)then
		for i,v in pairs(players) do
			player.statsidx=GetPlayersInfo(player);
		end
	end
end
function _LoadPlayerInfo()
	if(SafeWriting.GlobalStorage.PlayerInfo==nil)then
		SafeWriting.GlobalStorage.PlayerInfo={};
	end
	SafeWriting.GlobalStorage.PlayerInfo={};
	local PlayerInfoFolder=SafeWriting.GlobalStorageFolder;
	local PlayerInfo={};
	local file="PlayerInfo.xml";
	if(file:sub(-3)=="xml" and file~="_PlayerInfoTypeDef.xml")then
		PlayerInfo=CryAction.LoadXML(PlayerInfoFolder.."_PlayerInfoTypeDef.xml",PlayerInfoFolder..file);
		if(PlayerInfo==nil)then
			return nil,"Failed to load PlayerInfo!";
		end
		SafeWriting.GlobalStorage.PlayerInfo=PlayerInfo;
	end
end
function OptiScores(resetfields)
	local onlineplayers={};
	local players=g_gameRules.game:GetPlayers();
	if(players)then
		for i,v in pairs(players)do
			if(v.profile)then
				local pnum=tonumber(v.profile);
				if v.isSfwCl and ((pnum>0 and pnum<800000) or pnum>1000000) then
					onlineplayers[v.profile]=v;
				end
			end
		end
	end
	if(not resetfields)then
		resetfields={};
	end
	for i,v in pairs(resetfields)do
		resetfields[v]=true;
	end
	local PlayerInfoFolder=SafeWriting.GlobalStorageFolder;
	local fname=PlayerInfoFolder.."PlayerInfo.xml";
	local f,err=io.open(fname,"r");
	local tmps={};
	local non_add={
		["distance_kill"]=true;
		["id"]=true;
		["lo"]=true;
		["usual_name"]=true;
	};
	local converts={
		["id"]=tostring;
		["pltime"]=tostring;
		["kills"]=tonumber;
		["deaths"]=tonumber;
		["distance_kill"]=tostring;
		["in_bank"]=tostring;
		["lo"]=tonumber;
		["usual_name"]=tostring;
	};
	local PlInfo=SafeWriting.GlobalStorage.PlayerInfo.Players;
	local before=#PlInfo;
	for i,v in pairs(PlInfo) do
		if(tmps[v.id])then
			for j,w in pairs(v)do
				local f=converts[j];
				if(not non_add[j])then
					PlInfo[tmps[v.id]][j]=f(tonumber(PlInfo[tmps[v.id]][j])+tonumber(v[j]));
				end
				if(resetfields[j])then
					PlInfo[tmps[v.id]][j]=f(0);
				end
			end
			table.remove(PlInfo,i);
			if(onlineplayers[v.id])then
				onlineplayers[v.id].statsidx=tmps[v.id];
				printf("Assigning new statsidx (%s) to %s",tmps[v.id],onlineplayers[v.id]:GetName());
			end
		else
			if(v.id=="nil")then
				table.remove(PlInfo,i);
			else
				for j,w in pairs(v)do
					local f=converts[j];
					if(resetfields[j])then
						PlInfo[i][j]=f(0);
					end
				end
				tmps[v.id]=i;
			end
		end
	end
end
function SavePlayerInfo()
	if(SafeWriting.GlobalStorage.PlayerInfo==nil)then
		SafeWriting.GlobalStorage.PlayerInfo={};
	end
	local PlayerInfoFolder=SafeWriting.GlobalStorageFolder;
	local pli=SafeWriting.GlobalStorage.PlayerInfo.Players;
	local known={
		["id"]="0";
		["kills"]=0;
		["deaths"]=0;
		["pltime"]="0";
		["in_bank"]="0";
		["lo"]=os.time();
		["usual_name"]="@has_none";
	};
	if(pli)then
		for i,v in pairs(pli)do
			if(v)then
				for q,w in pairs(known) do
					pli[i][q]=pli[i][q] or w;
				end
			end
		end
	end
	OptiScores();
	CryAction.SaveXML(PlayerInfoFolder.."_PlayerInfoTypeDef.xml",PlayerInfoFolder.."PlayerInfo.xml",SafeWriting.GlobalStorage.PlayerInfo);
end
function SaveAllPlayersInfo()
	if(SafeWriting.Settings.EnableStatistics)then
		local pli=SafeWriting.GlobalStorage.PlayerInfo.Players;
		local aplayers=g_gameRules.game:GetPlayers();
		if(aplayers)then
			for i,player in pairs(aplayers) do
				local profile = tonumber(player.profile);
				if(player and player.isSfwCl and ((profile>0 and profile<800000) or profile>1000000) )then
					if(not player.rOffsetKills)then
						player.rOffsetKills=0;
						player.rOffsetDeaths=0;
						player.rOffsetTime=0;
					end
					if(player.statsidx and player.statsidx<=#pli)then
						if(not player.connecttime)then player.connecttime=_time; end
						local plid=pli[player.statsidx];
						plid.pltime=tostring(math.abs(plid.pltime+(_time-(player.connecttime))-player.rOffsetTime));
						plid.kills=((plid.kills or 0)+(player.rKills or 0))-player.rOffsetKills;
						plid.deaths=((plid.deaths or 0)+(player.rDeaths or 0))-player.rOffsetDeaths;
						plid.id=tostring(player.profile);
						plid.in_bank=tostring(pli[player.statsidx].in_bank or 0).."";
						plid.lo=tonumber(os.time());
						plid.usual_name=player.usual_name or "@has_none";
					else
						local in_bank=math.floor(tonumber(player.in_bank or 0));					
						local tbl={
							pltime=tostring((0)+(_time-(player.connecttime or _time))-player.rOffsetTime);
							kills=(((player.rKills or 0)-player.rOffsetKills) or 0);
							deaths=(((player.rDeaths or 0)-player.rOffsetDeaths) or 0);
							id=tostring(player.profile);
							distance_kill=tostring(player.dstkill or 0);
							in_bank=tostring(in_bank).."";
							lo=tonumber(os.time());
							usual_name=player.usualName or "@has_none";
						};
						table.insert(pli,tbl);
						player.statsidx=GetPlayersInfo(player);
					end
					player.rOffsetKills=player.rKills or 0;
					player.rOffsetDeaths=player.rDeaths or 0;
					player.rOffsetTime=(_time-(player.connecttime or 0));
				end
			end
		end
		SavePlayerInfo();
	end
end
System.AddCCommand("SfW_LoadPlayerInfo", "LoadPlayerInfo(%%)", "Loads PlayerInfo data");
System.AddCCommand("SfW_SavePlayerInfo", "SavePlayerInfo(%%)", "Save PlayerInfo data");
SSMCMDS.mystats={info="[[mystats_info]]";};
function SSMCMDS.mystats:func(player,msg)
	if(not SafeWriting.Settings.EnableStatistics)then
		Chat:SendToTarget(nil,player,"Sorry, but statistics are disabled now");
		return;
	end
	local pli=SafeWriting.GlobalStorage.PlayerInfo.Players;
	if((not player.statsidx) or (not pli[player.statsidx]))then
		Chat:SendToTarget(nil,player,"[[YOU_DONT_HAVE_STATS_YET]]");
		return;
	end
	local total=pli[player.statsidx].pltime+(_time-player.connecttime)-(player.rOffsetTime or 0);
	local thours=math.floor(total/3600);
	local tminutes=math.floor((total/60)%60);
	local kills,deaths=GetRealPlayerScore(player);
	local tkills=kills+(pli[player.statsidx].kills or 0)-(player.rOffsetKills or 0);
	local tdeaths=deaths+(pli[player.statsidx].deaths or 0)-(player.rOffsetDeaths or 0);
	local tdstkill=pli[player.statsidx].distance_kill or 0;
	Chat:SendToTarget(nil,player,"[[PLAYED_TIME]]: "..thours.." [[HOURS]] [[AND]] "..tminutes.." [[MINUTES]]");
	Chat:SendToTarget(nil,player,"[[KILLS]]: "..tkills);
	Chat:SendToTarget(nil,player,"[[DEATHS]]: "..tdeaths);
	Chat:SendToTarget(nil,player,"[[BEST_DSG1_KILL]]: "..tdstkill.."m");
	Chat:SendToTarget(nil,player,"ProfileID: "..player.profile);
end
AddChatCommand("top10",function(self,player,msg,srt)
	if(not SafeWriting.Settings.EnableStatistics)then
		Chat:SendToTarget(nil,player,"[[COMMAND_BLOCKED]]");
		return;
	end
	local secureIDs=true;
	local srt=string.match(msg,"^!top10 (.*)");
	local tbl=nil;
	if(srt)then
		srt=tostring(srt);
		if(srt=="distance_kill" or srt=="kills" or srt=="deaths" or srt=="pltime" or srt=="lo")then
			_G["CustomSort"]=true;
			_G["CustomSortType"]=srt;
		end
	end
	tbl=GenerateTop(nil,PlayerStatsSort);
	_G["CustomSort"]=nil;
	_G["CustomSortType"]=nil;
	local ctr=1;
	local count=10;
	local lang=player.lang;
	Console:SendToTarget(player,string.format("${t:Top10Hdr1|9}    %12s ${t:Top10Hdr2|8}%10s ${t:Top10Hdr1|9}%10s ${t:Top10Hdr2|8}%24s ${t:Top10Hdr1|9}%11s ${t:Top10Hdr2|8}%19s ${t:Top10Hdr1|9}%s","ProfileID",__qt(lang,R.KILLS),__qt(lang,R.DEATHS),__qt(lang,R.BEST_DSG1_KILL),__qt(lang,R.PLAYED_TIME),__qt(lang,R.LAST_ONLINE),__qt(lang,R.NAME_NOW)));
	for i,v in pairs(tbl.Players) do
		local pl=GetPlayerByProfile(v.id);
		local total=v.pltime;
		local kills=0;
		local deaths=0;
		local thours=math.floor(total/3600);
		local tminutes=math.floor((total/60)%60);
		if(pl)then
			kills,deaths=GetRealPlayerScore(pl);
		end
		local tkills=(v.kills or 0);
		local tdeaths=(v.deaths or 0);
		local dstkill=(v.distance_kill or 0);
		local pltime=tostring(thours).."h "..tostring(tminutes).."min";
		if(secureIDs)then
			local len=v.id:len();
			v.id=v.id:sub(1,math.ceil(len/2));
			v.id=v.id..string.rep("*",len/2);
		end
		if(pl)then
			Console:SendToTarget(player,SpecialFormat("${t:Top10On1|3} %2s %12s ${t:Top10On2|6}%10s ${t:Top10On1|3}%10s ${t:Top10On2|6}%24s ${t:Top10On1|3}%11s ${t:Top10On2|6}%19s ${t:Top10On1|3}%s",tostring(ctr),v.id,tkills,tdeaths,tostring(dstkill).."m",pltime,"-",pl:GetName()));
		else
			local lo=v.lo or 0;
			local d=os.date("%d.%m.%Y %H:%M:%S",lo);
			Console:SendToTarget(player,SpecialFormat("${t:Top10Off1|4} %2s %12s ${t:Top10Off2|8}%10s ${t:Top10Off1|4}%10s ${t:Top10Off2|8}%24s ${t:Top10Off1|4}%11s ${t:Top10Off2|8}%19s ${t:Top10Off1|4}-",tostring(ctr),v.id,tkills,tdeaths,tostring(dstkill).."m",pltime,d));
		end
		if(ctr==count)then
			break;
		end
		ctr=ctr+1;
	end
	self:OpenConsole(player);
end,{WORD},nil,"[[top10_info]]");