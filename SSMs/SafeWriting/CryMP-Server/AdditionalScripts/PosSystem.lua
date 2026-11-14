if not Pos then
	Pos={
		Maps={};
		Map=GetMapName();
		OutFolder=SafeWriting.GlobalStorageFolder;
	};
end
function Pos:Init(outf)
	if outf then self.OutFolder=outf; end
	self.Map=GetMapName();
	local try=loadfile(self:GetPath("PosData.lua"));
	if try then
		assert(try)()
		printf("Successfuly loaded PosData.lua, map %s",self.Map)
	end
	self:PrepareMap();
end
function Pos:PrepareMap()
	if not self.Maps[self.Map] then
		self.Maps[self.Map]={};
	end
	printf("Successfuly prepared map %s, loaded positions: %d",self.Map,#self.Maps[self.Map]);
end
function Pos:Create(player,name)
	local idx,pos,auth,pName,angle=self:GetPos(name);
	if idx then
		return false,"Position with this name already exists on this map.";
	else
		self.Maps[self.Map][#self.Maps[self.Map]+1]={player:GetPos(),player.profile,name,player:GetAngles(),true};
		self:Save();
		return true;
	end
end
function Pos:Remove(player,name,force)
	local idx,pos,auth,pName=self:GetPos(name);
	if idx then
		if auth==player.profile or force then
			table.remove(self.Maps[self.Map],idx);
			self:Save();
			return true;
		else
			return false,"You are not owner of this position!";
		end
	else
		return false,"Position with this name does not exist on this map.";
	end
end
function Pos:GetPos(name)
	for i,v in pairs(self.Maps[self.Map]) do
		local pos,auth,pName,angle,pvp=unpack(v);
		if name==pName then
			return i,pos,auth,pName,angle,pvp;
		end
	end
	return nil,nil,nil,nil,nil;
end
function Pos:SetPVP(player,name,state)
	local i,pos,auth,pName=self:GetPos(name);
	if i then
		if auth==player.profile then
			if state then
				self.Maps[self.Map][i][5]=true;
			else
				self.Maps[self.Map][i][5]=false;
			end
			self:Save();
			return true;
		else
			return false,"You are not owner of this position!";
		end
	else
		return false,"Position with this name does not exist on this map.";
	end
end
function Pos:Save()
	local f,err=io.open(self:GetPath("PosData.lua"),"w");
	if not f then print("Error when opening the file!"); return; end
	f:write(arr2str(Pos.Maps,"Pos.Maps"));
	f:close();
end
function Pos:GetPath(path) return self.OutFolder..path; end
function Pos:OnActorHit(hit)
	if hit.shooter and hit.target and hit.shooter~=hit.target then
		if hit.target.currentPos and hit.shooter.currentPos then
			if not hit.target.posPVP then
				hit.damage=0;
				Msg:SendToTarget(hit.shooter,"You cannot make damage on protected pos");
			end
		end
	end
end
function Pos:OnKill(hit)
	if hit.target then
		if hit.target.currentPos then
			hit.target.deadFlag=true;
		end
	end
end
function Pos:UpdatePlayer(player)
	if player.host and player.currentPos and player.deadFlag then
		if not player:IsDead() then
			Chat:SendToTarget(nil,player,"Use !inside to get back to map");
			TeleportPlayer(player,player.currentPos);
			player.deadFlag=nil;
		end
	end 
end
function Pos:PrepareAll()
	Pos:Init();
end
SafeWriting.FuncContainer:LoadPlugin(Pos);
AddChatCommand("pos",function(self,player,msg,text)
	if not text then Chat:SendToTarget(nil,player,"Enter valid position name"); return; end
	local idx,pos,auth,name,angle,pvp=Pos:GetPos(text);
	if not idx then Chat:SendToTarget(nil,player,"This position does not exist on this map."); return; end
	if not player.insidePos then
		player.insidePos=player:GetWorldPos();
	end
	player.currentPos={};
	MergeTables(player.currentPos,pos);
	player.posPVP=pvp;
	TeleportPlayer(player,pos);
	g_gameRules.game:MovePlayer(player.id, pos, angle);
	--if angle then Script.SetTimer(1000,function() player:SetAngles(angle); end); end
	Msg:SendToAll("Player ,%s' has teleported to ,%s', use ,!pos %s' to teleport there too","info",player:GetName(),text,text);
	Chat:SendToTarget(nil,player,"Welcome to ,%s', use !inside to get back!",name);
end,{TEXT},{},"Teleports you to some position");
AddChatCommand("poslist",function(self,player,msg,page)
	page=page or 1;
	Console:SendToTarget(player,"${t:PosListSide1|6}--[[  ${t:PosListHdr1|9}%3s$o|${t:PosListHdr2|8}%20s$o|${t:PosListHdr1|9}%8s$o|${t:PosListHdr2|8}%8s$o|${t:PosListHdr1|9}%8s|${t:PosListHdr2|8}%4s${t:PosListSide1|6}  ]]--","idx","Name","X","Y","Z","PVP");
	for i=(((page-1)*10)+1),page*10 do
		if not Pos.Maps[Pos.Map][i] then break; end
		local pos,auth,name,angl,pvp=unpack(Pos.Maps[Pos.Map][i]);
		Console:SendToTarget(player,"  ${t:PosListSide2|0}[[  ${t:PosList1|9}%3d$o|${t:PosList2|8}%20s$o|${t:PosList1|9}%8s$o|${t:PosList2|8}%8s$o|${t:PosList1|9}%8s|${t:PosList2|8}%4s ${t:PosListSide2|0} ]]  ",i,name,tostring(math.floor(pos.x*100)/100),tostring(math.floor(pos.y*100)/100),tostring(math.floor(pos.z*100)/100),pvp and "on" or "off");
	end
	Console:SendToTarget(player,"${t:PosListSide1|6}--[[                       ${t:PosListInfo1|8}Page ${t:PosListInfo2|5}%d ${t:PosListInfo3|8}of${t:PosListInfo4|5} %d${t:PosListSide1|6}                     ]]--",page,math.ceil(#Pos.Maps[Pos.Map]/10));
	Msg:SendToTarget(player,__qt(player.lang,R.OPEN_CONSOLE));
end,{INT},{},"Shows you all positions on this map, usage !poslist <page>");
AddChatCommand("cpos",function(self,player,msg,text)
	if not text then Chat:SendToTarget(nil,player,"Enter valid position name"); return; end
	text=text:sub(1,20);
	local res,err=Pos:Create(player,text);
	if not res then
		Chat:SendToTarget(nil,player,err);
	else
		Msg:SendToAll("Player ,%s' has created new position on this map, you can use ,!pos %s' to teleport there","info",player:GetName(),text);
		Chat:SendToTarget(nil,player,"If you want to remove this position on this map, use ,!rpos %s', PVP is on, to disable it, use !pvpoff",text);
	end
end,{TEXT},{PremiumOnly=true},"Creates new position, usage !cpos <name>");
AddChatCommand("rpos",function(self,player,msg,text)
	if not text then Chat:SendToTarget(nil,player,"Enter valid position name"); return; end
	local res,err=Pos:Remove(player,text);
	if not res then
		Chat:SendToTarget(nil,player,err);
	else
		Msg:SendToAll("Player ,%s' has removed his position ,%s'","info",player:GetName(),text);
	end
end,{TEXT},{PremiumOnly=true},"Removes some of your positions, usage !rpos <name>");
AddChatCommand("arpos",function(self,player,msg,text)
	if not text then Chat:SendToTarget(nil,player,"Enter valid position name"); return; end
	local res,err=Pos:Remove(player,text,true);
	if not res then
		Chat:SendToTarget(nil,player,err);
	else
		Msg:SendToAll("Admin ,%s' has removed position ,%s'","info",player:GetName(),text);
	end
end,{TEXT},{AdminOnly=true},"Removes position, usage !arpos <name>");
AddChatCommand("pvpon",function(self,player,msg,text)
	if not text then Chat:SendToTarget(nil,player,"Enter valid position name"); return; end
	local res,err=Pos:SetPVP(player,text,true);
	if not res then
		Chat:SendToTarget(nil,player,err);
	else
		Chat:SendToTarget(nil,player,"PVP on ,%s' was successfuly enabled",text);
	end
end,{TEXT},{PremiumOnly=true},"Enables PVP on your pos",true);
AddChatCommand("pvpoff",function(self,player,msg,text)
	if not text then Chat:SendToTarget(nil,player,"Enter valid position name"); return; end
	local res,err=Pos:SetPVP(player,text,false);
	if not res then
		Chat:SendToTarget(nil,player,err);
	else
		Chat:SendToTarget(nil,player,"PVP on ,%s' was successfuly disabled",text);
	end
end,{TEXT},{PremiumOnly=true},"Disables PVP on your pos",true);
AddChatCommand("inside",function(self,player)
	if player.currentPos then
		player.currentPos=nil;
		TeleportPlayer(player,player.insidePos);
		player.insidePos=nil;
		player.posPVP=nil;
		Msg:SendToAll("%s has teleported back to map using !inside","info",player:GetName());
	end
end,nil,nil,"Teleports you back to map");