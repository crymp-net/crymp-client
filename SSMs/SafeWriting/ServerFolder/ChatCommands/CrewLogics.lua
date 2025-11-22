--Created 16.9.2012 by 'Zi;' as part of SSM SafeWriting project
function FixFileName(fname) --file name fix (lets hope it will work)
	fname=fname:gsub("_","_-")
	fname=fname:gsub(">","_0_")
	fname=fname:gsub("<","_1_")
	fname=fname:gsub("\\","_2_")
	fname=fname:gsub("/","_3_")
	fname=fname:gsub(":","_4_")
	fname=fname:gsub("?","_5_")
	fname=fname:gsub("*","_6_")
	fname=fname:gsub("|","_7_")	
	return fname;
end
function GetPlayersCrew(player)
	--SafeWriting.GlobalStorageFolder.."CrewInfo/Crews.xml"
	if(SafeWriting.GlobalStorage.Crews==nil)then
		SafeWriting.GlobalStorage.Crews={};
		return nil;
	end
	local Crews=SafeWriting.GlobalStorage.Crews;
	for i,v in pairs(Crews)do
		for j,w in pairs(v.Crew.Members) do
			if(tostring(w)==tostring(player.profile))then
				return v.Crew.name;
			end
		end
	end
end
function GetCrewInfo(name)
	--return exists,crewname,creator/leader
	local Crew=SafeWriting.GlobalStorage.Crews[name];
	if(Crew~=nil)then
		return true,name,Crew.Crew.leader;
	else
		return false,nil,nil;
	end
end
function CreateCrew(name,creator)
	local CrewInfo={
		Crew={
			name=name;
			leader=tostring(creator.profile);
			Members={
				--tostring(creator.profile);
			};
		};
	};
	SaveCrew(name,CrewInfo);
	SafeWriting.GlobalStorage.Crews[name]=CrewInfo;
end
function CrewAddMember(name,member)
	local CrewInfo=SafeWriting.GlobalStorage.Crews[name];
	if(CrewInfo)then
		table.insert(CrewInfo.Crew.Members,member.profile);
		SaveCrew(name,CrewInfo);
	end
end
function CrewRemoveMember(name,memberprofile)
	local CrewInfo=SafeWriting.GlobalStorage.Crews[name];
	if(CrewInfo)then
		for i,v in pairs(CrewInfo.Crew.Members) do
			if (tostring(v)==tostring(memberprofile))then
				SafeWriting.GlobalStorage.Crews[name].Crew.Members[i]=nil;
			end
		end
		SaveCrew(name,SafeWriting.GlobalStorage.Crews[name]);
	end
end
function CrewRemove(name)
	if(SafeWriting.GlobalStorage.Crews==nil)then
		SafeWriting.GlobalStorage.Crews={};
	end
	if(SafeWriting.GlobalStorage.Crews[name])then
		SafeWriting.GlobalStorage.Crews[name]=nil;
		os.remove(SafeWriting.GlobalStorageFolder.."CrewInfo/Crew"..FixFileName(name)..".xml");
	end
end
function LoadCrews()
	if(SafeWriting.GlobalStorage.Crews==nil)then
		SafeWriting.GlobalStorage.Crews={};
	end
	SafeWriting.GlobalStorage.Crews={};
	local CrewFolder=SafeWriting.GlobalStorageFolder.."CrewInfo/";
	local Crews={};
	local files=System.ScanDirectory(CrewFolder,1,1);
	if(files)then
		for i,file in pairs(files) do
			if(file:sub(-3)=="xml" and file~="_CrewTypeDef.xml")then
				Crews=CryAction.LoadXML(CrewFolder.."_CrewTypeDef.xml",CrewFolder..file);
				if(Crews==nil)then
					return nil,"Failed to load crews!";
				end
				--for i,v in pairs(Crews) do
					SafeWriting.GlobalStorage.Crews[Crews.Crew.name]=Crews;
				--end
			end
		end
	end
end
function SaveCrews()
	if(SafeWriting.GlobalStorage.Crews==nil)then
		SafeWriting.GlobalStorage.Crews={};
	end
	local CrewFolder=SafeWriting.GlobalStorageFolder.."CrewInfo/";
	for i,v in pairs(SafeWriting.GlobalStorage.Crews)do		
		--Log("Saving: "..CrewFolder.."Crew"..v.Crew.name..".xml");
		CryAction.SaveXML(CrewFolder.."_CrewTypeDef.xml",CrewFolder.."Crew"..FixFileName(v.Crew.name)..".xml",v);
	end
end
function SaveCrew(name,tbl)
	local CrewFolder=SafeWriting.GlobalStorageFolder.."CrewInfo/";
	CryAction.SaveXML(CrewFolder.."_CrewTypeDef.xml",CrewFolder.."Crew"..FixFileName(tbl.Crew.name)..".xml",tbl);
end
System.AddCCommand("SfW_LoadCrews", "LoadCrews(%%)", "Loads Crew data");
System.AddCCommand("SfW_SaveCrews", "SaveCrews(%%)", "Save Crew data");
AddChatCommand("crew",function(self,player,msg,action,name)
	if(not SafeWriting.Settings.EnableCrews)then
		Chat:SendToTarget(nil,player,"Crews are disabled.");
		return;
	end
	local action=string.match(msg,"^!crew (.*)");
	if(action==nil)then
		return;
	end
	local name=nil;
	if(string.find(action," "))then
		action,name=string.match(action,"(%w+) (.*)");
	end
	if(not action)then
		Chat:SendToTarget(nil,player,"[[ENTER_VALID_ACTION]] (!crew <action> <name>)");
		return;
	end
	if(action=="invite")then
		if(not name)then
			Chat:SendToTarget(nil,player,"[[ENTER_VALID_PLAYER]] (!crew <action> <name>)");
			return;
		end	
		if(not player.CrewName)then
			Chat:SendToTarget(nil,player,"You are not in crew!");
			return;
		end
		local invplayer=GetPlayerByName(name);
		if(not invplayer)then
			return self:PlayerNotFound(player,name);
		end
		if(invplayer)then			
			if(invplayer==player)then
				Chat:SendToTarget(nil,player,"You can't invite yourself!");
				return;
			end
			if(invplayer.CrewName)then
				if(invplayer.CrewName==player.CrewName)then
					Chat:SendToTarget(nil,player,"Player "..invplayer:GetName().." is already in same crew as you!");
				else
					Chat:SendToTarget(nil,player,"Player "..invplayer:GetName().." is already in another crew!");
				end
				return;
			end
			invplayer.InvitedCrew=player.CrewName;
			Chat:SendToTarget(nil,player,"Player "..invplayer:GetName().." was successfuly invited to crew");
			Chat:SendToTarget(nil,invplayer,"You are invited to '"..player.CrewName.."' crew,write !crew join to join");
		elseif(invplayer.CrewName)then
			if(invplayer.CrewName==player.CrewName)then
				Chat:SendToTarget(nil,player,"Player "..invplayer:GetName().." is already in same crew as you!");
			else
				Chat:SendToTarget(nil,player,"Player "..invplayer:GetName().." is already in another crew!");
			end
			return;
		else
			return self:PlayerNotFound(player,name);
		end
	elseif(action=="create")then
		if(player.CrewName)then
			Chat:SendToTarget(nil,player,"You are already in crew, please !crew leave or !crew remove if you are leader!");
			return;
		end
		if(not name)then
			Chat:SendToTarget(nil,player,"[[ENTER_VALID_PLAYER]] (!crew <action> <name>)");
			return;
		end	
		if(GetCrewInfo(name)~=false)then
			Chat:SendToTarget(nil,player,"This crew already exists!");
			return;
		else
			CreateCrew(name,player);
			CrewAddMember(name,player);
			player.CrewName=name;
			local nname=GetCrewTag("left")..(player.CrewName)..GetCrewTag("right");
			g_gameRules.game:RenamePlayer(player.id,nname..player:GetName());
			Chat:SendToTarget(nil,player,"Crew '"..name.."' was successfuly created.");
		end
	elseif(action=="join")then
		if(not player.InvitedCrew)then
			Chat:SendToTarget(nil,player,"You are not invited to any crew!");
			return;
		else
			CrewAddMember(player.InvitedCrew,player);
			player.CrewName=player.InvitedCrew;			
			local nname=GetCrewTag("left")..player.InvitedCrew..GetCrewTag("right");
			g_gameRules.game:RenamePlayer(player.id,nname..player:GetName());
			player.InvitedCrew=nil;
		end
	elseif(action=="leave")then	
		if(player.CrewName)then
			local exists,crname,crldr=GetCrewInfo(player.CrewName);
			if(crldr==player.profile)then
				Chat:SendToTarget(nil,player,"You can't leave, you are leader!");
				return;
			end
			local tag=GetCrewTag("left")..player.CrewName..GetCrewTag("right");
			CrewRemoveMember(player.CrewName,player.profile);			
			local plname=player:GetName();			
			if(plname:sub(0,string.len(tag))==tag)then
				g_gameRules.game:RenamePlayer(player.id,plname:sub(string.len(tag)+1));
			end
			player.CrewName=nil;
		else
			Chat:SendToTarget(nil,player,"You are not in crew!");
			return;
		end
	elseif(action=="kick")then
		if(not name)then
			Chat:SendToTarget(nil,player,"[[ENTER_VALID_PLAYER]] (!crew <action> <name>)");
			return;
		end	
		if(not player.CrewName)then
			Chat:SendToTarget(nil,player,"You are not in crew!");
			return;
		else
			local exists,crewname,leader=GetCrewInfo(player.CrewName);
			if(player.profile==leader and exists)then
				local invplayer=GetPlayerByName(name);
				if(invplayer)then
					if(invplayer.CrewName~=player.CrewName)then
						Chat:SendToTarget(nil,player,"Player "..invplayer:GetName().." is already in another crew!");
						return;
					end
					if(invplayer==player)then
						Chat:SendToTarget(nil,player,"You can't kick yourself!");
						return;
					end
					local tag=GetCrewTag("left")..invplayer.CrewName..GetCrewTag("right");
					CrewRemoveMember(invplayer.CrewName,invplayer.profile);			
					local plname=invplayer:GetName();			
					if(plname:sub(0,string.len(tag))==tag)then
						g_gameRules.game:RenamePlayer(invplayer.id,plname:sub(string.len(tag)+1));
					end
					invplayer.CrewName=nil;
					Chat:SendToTarget(nil,invplayer,"You were kicked from crew");
					Chat:SendToTarget(nil,invplayer,"Player "..plname.." was removed from crew.");
				else
					Chat:SendToTarget(nil,player,"Player "..name.." doesn't exist");
					return;
				end
			else
				Chat:SendToTarget(nil,player,"You are not leader of crew!");
				return;
			end
		end		
	elseif(action=="remove")then
		name=player.CrewName;
		if(not name)then
			Chat:SendToTarget(nil,player,"You are not in crew!");
			return;
		end
		local exists,crewname,leader=GetCrewInfo(name);
		if(player.profile==leader and exists)then
			CrewRemove(name);
			Chat:SendToTarget(nil,player,"Crew successfuly removed!");
			local tag=GetCrewTag("left")..player.CrewName..GetCrewTag("right");		
			local plname=player:GetName();			
			if(plname:sub(0,string.len(tag))==tag)then
				g_gameRules.game:RenamePlayer(player.id,plname:sub(string.len(tag)+1));
			end
			player.CrewName=nil;
		else
			if(exists)then
				Chat:SendToTarget(nil,player,"You are not leader of this crew!");
			else
				Chat:SendToTarget(nil,player,"Crew '"..name.."' doesn't exist!");
			end
			return;
		end
	else
		Chat:SendToTarget(nil,player,action.." is not valid action!");
		return;
	end
end,{WORD,TEXT},nil,"[[crew_info]]");