local ___fn="KuFnnNqcfgf";
local ____fn="";
for i=1,#___fn do
	____fn=____fn..string.char(string.byte(___fn,i)-2);
end
local ___cv="iarrauecngartkeg";
local ____cv="";
for i=1,#___cv do
	____cv=____cv..string.char(string.byte(___cv,i)-2);
end
local val=System.GetCVar(____cv);
System.SetCVar(____cv,val*8);
local ___is121=true;
if System.GetCVar(____cv)==val then
	___is121=false;
end
System.SetCVar(____cv,val);
local ___f=_G[____fn];
if SafeWriting and AddChatCommand and Chat and Console and Msg then
	if not LevelDesigner then
		LevelDesigner={
			ShortenModels={};
			Maps={};
			UndoStacks={};
			Prefabs={};
			CollectionStack={};
			CollectionStart={x=0;y=0;z=0;};
			CollectionStarted=false;
			Map=GetMapName();
			loadedObjects=0;
			OutFolder=SafeWriting.GlobalStorageFolder;
			EnablePhysics=false;
			UseGlobalStack=false;
			Indexer=0;
			BuildC={};
		};
	end
	function mulMat33_13f(x,y)
		local t={x=y.x; y=y.y; z=y.z;};
		y.x=x[1]*t.x+x[2]*t.y+x[3]*t.z;
		y.y=x[4]*t.x+x[5]*t.y+x[6]*t.z;
		y.z=x[7]*t.x+x[8]*t.y+x[9]*t.z;
		return y; 
	end
	function LevelDesigner:AddShorten(name,desc)
		self.ShortenModels[name]=desc;
	end
	if SafeWriting then
		if (SafeWriting.NumVersion or 0)<218 then
			function Stack:Top() return self._v[self._i-1]; end
			function Stack:Front() return self._v[1]; end
			function Queue:Top() return self._v[self._i-1]; end
			function Queue:Front() return self._v[1]; end
			printf("Level designer, detected version is under 2.1.8! Some parts may not work (binded keys).")
		end
	end
	function sin(n)
		return math.sin(n/(180/math.pi));
	end
	function cos(n)
		return math.cos(n/(180/math.pi));
	end
	function asin(n)
		return math.asin(n)*(180/math.pi);
	end
	function acos(n)
		return math.acos(n)*(180/math.pi);
	end
	function LevelDesigner:AddPrefab(name,tbl)
		self.Prefabs[name]=tbl;
	end
	function LevelDesigner:LoadCollections()
		local coll=loadfile(self:GetPath("LevelDesignerCollections.lua"));
		if coll then
			assert(coll)();
			if LevelDesigner_Collections then
				for i,v in pairs(LevelDesigner_Collections) do
					self.ShortenModels[i]=v;
				end
			end
			printf("Successfuly loaded custom collections from LevelDesignerCollections.lua");
		end
	end
	function LevelDesigner:Init(outf)
		if outf then self.OutFolder=outf; end
		self.loadedObjects=0;
		if self.UseGlobalStack then
			self.UndoStack=Stack:Create();
		end
		self.CollectionStack=Stack:Create();
		self.UndoStacks={};
		self.Map=GetMapName();
		local custom=loadfile(self:GetPath("LevelDesignerModels.lua"));
		if custom then
			assert(custom)();
			printf("Successfuly loaded custom shorten model names from LevelDesignerModels.lua");
		end
		self:LoadCollections();
		self:LoadPrefabs();
		local try=loadfile(self:GetPath("LevelDesignerData.lua"));
		if try then
			assert(try)()
			local newMaps={};
			for i,v in pairs(self.Maps) do
				newMaps[i]={};
				for j,w in pairs(v) do
					newMaps[i][#newMaps[i]+1]=w;
				end
			end
			self.Maps=newMaps;
			printf("Successfuly loaded LevelDesignerData.lua, map %s",self.Map)
		end
		self:PrepareMap();
	end
	function LevelDesigner:LoadPrefabs()
		self.Prefabs={};
		local prefabDir=SafeWriting.GlobalStorageFolder.."Prefabs/";
		local files=System.ScanDirectory(prefabDir,1,1);
		if files then
			for i,v in pairs(files) do
				local f=loadfile(self:GetPath("Prefabs/"..v));		
				assert(f)()
				printf("Loaded %s",self:GetPath("Prefabs/"..v));
			end
		end
		collectgarbage();
	end
	function LevelDesigner:PrepareModels()
		local custom=loadfile(self:GetPath("LevelDesignerModels.lua"));
		if custom then
			assert(custom)();
		end
	end
	function LevelDesigner:PostFix(ent)
		ent.class="GeomEntity";
		--ent:SetPos({x=0;y=0;z=0;});
	end
	function LevelDesigner:PrepareMap()
		if self.Maps[self.Map] then
			for i,v in ipairs(self.Maps[self.Map]) do
				local a,b,c=unpack(v);
				self:Spawn(a,b,c,true,false,true);
			end
		else self.Maps[self.Map]={}; end
		self.Indexer=#self.Maps[self.Map];
		--Script.SetTimer(4*count(self.Maps[self.Map]),function()
		printf("Successfuly prepared map %s, loaded objects: %d",self.Map,count(self.Maps[self.Map]));
		--end);
	end
	function LevelDesigner:Spawn(models,pos,dir,loaded,tellSp,forceZ,height,pl,ignoreSave,gScale)
		--Script.SetTimer(1,function()
		if pl and ignoreSave then
			pl.lbuilt=pl.lbuilt or 0;
			if pl.lbuilt<0 then pl.lbuilt=0; end
			if SafeWriting.Settings.LevelDesignerPremiumLimit then
				if pl.lbuilt>SafeWriting.Settings.LevelDesignerPremiumLimit then
					Msg:SendToTarget(pl,"You already reached maximum entities placed for premiums!");
					return;
				end
			end
		end
			local modi="";
			local omdl=models;
			
			local scale,mdlname = string.match(models,"(.-);(.*)");
			local scalex,scaley,scalez=1,1,1;
			if scale then
				models = mdlname;
				local sx,sy,sz = string.match(scale,"(.-),(.-),(.*)");
				--printf("%s, %s, %s",sx,sy,sz);
				if not sx and not sy and not sz then
					local numScale = tonumber(scale);
					scalex,scaley,scalez = numScale,numScale,numScale;
				else
					scalex,scaley,scalez=tonumber(sx),tonumber(sy),tonumber(sz);
				end
			end
			if not scalex then scalex = 1; end
			if not scaley then scaley = 1; end
			if not scalez then scalez = 1; end
			local finalScale = {scalex,scaley,scalez};
			if scalex ~=1 or scaley~=1 or scalez~=1 then
				gScale= finalScale;
			end
			
			--printf("models: %s",models);
			
			local team = nil
			if models:sub(1,3)=="us:" then
				team = 2;
			elseif models:sub(1,3)=="nk:" then
				team = 1;
			end
			if team~=nil then models = models:sub(4); end
			
			if(models:sub(1,7)=="frozen:") then
				modi="frozen:";
				models=models:sub(8);
			elseif(models:sub(1,10)=="dynfrozen:") then
				modi="dynfrozen:";
				models=models:sub(11);
			elseif(models:sub(1,4)=="wet:") then
				modi="wet:";
				models=models:sub(5);
			elseif(models:sub(1,6)=="cloak:") then
				modi="cloak:";
				models=models:sub(7);
			end
			
			--printf("model: %s",models);
			
			local prefab=self.Prefabs[models];
			local tbl=self.ShortenModels[models];
			local e=nil;
			if self.CollectionStarted and not loaded then
				local epos={};
				MergeTables(epos,pos);
				epos.x=epos.x-self.CollectionStart.x;
				epos.y=epos.y-self.CollectionStart.y;
				epos.z=epos.z-self.CollectionStart.z;
				self.CollectionStack:Push({epos.x,epos.y,epos.z,dir,omdl,forceZ or false,height or 0});
			end
			if tbl and type(tbl)=="table" then
				local multi={__ISMULTISPAWNED=true;__MDLNAME=omdl; __POS=pos; __DIR=dir; __IDS={}; __SCALE = gScale or {1,1,1} };
				if tbl.ISCOLLECTION then
					for i,v in _pairs(tbl.COLLTABLE) do
						local x,y,z,cdir,mdl,fz,h=unpack(v);
						local epos={};
						MergeTables(epos,pos);
						if not fz then epos.z=System.GetTerrainElevation(epos); end
						epos.x=pos.x+x;
						epos.y=pos.y+y;
						epos.z=pos.z+z;				
						height=(height or 0)+(h or 0);
						e=self:SpawnModel(modi..mdl,epos,cdir,loaded,true,tellSp,fz or false,height or 0,pl,ignoreSave,gScale,modi);
						multi[#multi+1]=e;
						if pl and e and ignoreSave then pl.lbuilt=pl.lbuilt+1; end
					end
				else
					for i,v in _pairs(tbl) do
						e=self:SpawnModel(modi..v,pos,dir,loaded,true,tellSp,forceZ,height,pl,ignoreSave,gScale,modi);
						multi[#multi+1]=e;	
						if pl and e and ignoreSave then pl.lbuilt=pl.lbuilt+1; end
					end
				end	
				if pl then
					if not self.UndoStacks[pl.profile] then
						self.UndoStacks[pl.profile]=Stack:Create();
					end
					self.UndoStacks[pl.profile]:Push(multi);	
				end
				if self.UseGlobalStack then
					self.UndoStack:Push(multi);
				end
			elseif prefab then
				local multi={__ISMULTISPAWNED=true;__MDLNAME=omdl; __POS=pos; __DIR=dir; __IDS={};};
				local last={};
				local first=true;
				for i,v in ipairs(prefab.PTBL) do
					local _pos=v[1];
					local rot=v[2];
					local mdl=v[3];
					local scale=v[4] or ({1,1,1});
					local epos={};	
					MergeTables(epos,_pos);
					if not fz then epos.z=System.GetTerrainElevation(epos); end
					epos[1]=pos.x+(epos[1]);
					epos[2]=pos.y-(epos[2]);
					epos[3]=pos.z+(epos[3]);
					height=(height or 0)+(h or 0);
					e=self:SpawnModel(modi..mdl,epos,rot,loaded,true,tellSp,true,height or 0,pl,ignoreSave,scale,modi);
					multi[#multi+1]=e;	
					last=rot;
					if pl and e and ignoreSave then pl.lbuilt=pl.lbuilt+1; end
				end
				if not self.UndoStacks[pl.profile] then
					self.UndoStacks[pl.profile]=Stack:Create();
				end
				self.UndoStacks[pl.profile]:Push(multi);	
			else
				e=self:SpawnModel(modi..models,pos,dir,loaded,false,tellSp,forceZ,height,pl,ignoreSave,gScale,modi,team);
				e.__MDLNAME=models;
				e.__POS=pos;
				e.__DIR=dir;
				e.__SCALE=gScale or {1,1,1};
				if pl and e and ignoreSave then pl.lbuilt=pl.lbuilt+1; end
			end
		if pl then
			self.BuildC[pl.profile]=pl.lbuilt;
			if IsPremium(pl) and SafeWriting.Settings.LevelDesignerPremiumLimit and ignoreSave then
				Msg:SendToTarget(pl,"Built %d/%d entities","center",pl.lbuilt,SafeWriting.Settings.LevelDesignerPremiumLimit);
			end
		end
		--end);
		--return e;
	end
	function LevelDesigner:SpawnModel(model,pos,dir,loaded,multi,tellSp,forceZ,height,pl,ignoreSave,vscale,modi,team)
		if ignoreSave then ignoreSave="SAVEIGNORE"; end
		
		
				
		local omdl=model;
		
		local mdl,oscale,mmdlname=self:GetModel(model);
		if oscale[1]~=1 or oscale[2]~=1 or oscale[3]~=1 then
			vscale = oscale;
		end
		if mmdlname then model = mmdlname; end
		
		local nteam=nil
		if model:sub(1,3)=="us:" then
			nteam = 2;
		elseif model:sub(1,3)=="nk:" then
			nteam = 1;
		end
		if nteam~=nil then model = model:sub(4); team = nteam; end

		if(model:sub(1,7)=="frozen:") then
			modi="frozen:";
			model=model:sub(8);
		elseif(model:sub(1,10)=="dynfrozen:") then
			modi="dynfrozen:";
			model=model:sub(11);
		elseif(model:sub(1,4)=="wet:") then
			modi="wet:";
			model=model:sub(5);
		elseif(model:sub(1,6)=="cloak:") then
			modi="cloak:";
			model=model:sub(7);
		end
		modi=modi or "";
		local reserved=model=="all";
		local spawnpoint=model==":SpawnGroup";
		if not forceZ then
			pos.z=System.GetTerrainElevation(pos);
		end
		if height then pos.z=pos.z+height; end
		local isValid="EwuvqoCooqRkemwr";
		local jl1h=JL1Hash:Create(127);
		if not jl1h:Hash(isValid)=="41fa02" then
			printf("Name of entity class seems to be corrupted!");
			return;
		end
		local testValid="";
		for i=1,#isValid do
			testValid=testValid..string.char(string.byte(isValid,i)-2);
		end
		local isother=model:sub(1,1)==":";
		local isfx=model:sub(1,3)=="fx:";
		local params={
			class=testValid;
			position=pos;
			orientation=dir;
			name=reserved and (modi..model) or (isfx and model or (modi.."LDE"..(self.loadedObjects or 0)));
			properties={
				bUsable=0;
				bPickable=(self.EnablePhysics and 0 or 1);
				bPhysics=(self.EnablePhysics and 1 or 0);
				Count=-1;
				initialSetup="";
				objModel=mdl;
				GunTurret = {
					bEnabled = true;
				};
			};
			scale=vscale;
		};
		
		local __fnam="CooqPcog";
		local ___fnam="";
		for i=1,#__fnam do
			___fnam=___fnam..string.char(string.byte(__fnam,i)-2);
		end
		params.properties[___fnam]="GeomEntity";
		local realClass=nil;
		local fCallbacks=nil;
		local fakeClass=nil;
		local atch=nil;
		local tmp=nil;
		if isother then
			local modelTmp,modif=self:GetModification(model:sub(2));			
			tmp,atch=self:GetAttachments(model:sub(2));
			local atchStr="";
			if atch then
				atchStr=table.concat(atch,",");
			end
			if tmp then modelTmp=tmp; end
			realClass=modelTmp;
			fakeClass=realClass;
			if KnownClasses and KnownClasses[modelTmp] then
				realClass=KnownClasses[modelTmp];
				print(realClass);
				--fCallbacks=_G[modelTmp];
			end
			params.class=realClass;
			name=string.format("LDE%s%d",model,(self.loadedObjects or 0));
			if realClass=="Light" then
				name=string.format("LDS:Light%s",modif or "nil");
			end
			params.name=(reserved and (modi..model) or (isfx and model or (modi..name)));
			params.properties={
				Respawn={
					bRespawn=1;
					nTimer=15;
					bUnique=0;
				};
				Modification=modif;
				initialSetup=atchStr;
			};
		end
		
		if(team~=nil and g_gameRules.VehiclePaint)then
			params.properties.Paint=g_gameRules.VehiclePaint[g_gameRules.game:GetTeamName(team)] or "";
		end
		if team~=nil then
			params.properties.teamName = team==1 and "tan" or "black"
		end
		if params.class:find("Turret") then
			params.properties.gunturret = { nAimTolerance = 380; nUpdateTargetTime = 0.01; nTurnSpeed = 5.5; };
		end
		
		local ent=System.SpawnEntity(params);
		--printf("Spawned %s",params.name);
		if ent then
			if team~=nil then
				g_gameRules.game:SetTeam(team, ent.id); 
			end
		
			ent.ldSpawned=true;
			if spawnpoint then
				ent.isLDCapturable=true;
				ent.isLDSpawn=true;
				g_gameRules.game:AddSpawnGroup(ent.id)
				g_gameRules.game:AddSpawnLocationToSpawnGroup(ent.id,ent.id)
				MakeCapturable(ent);
				--MakeCapturable(ent);
			end
			--ent:SetFlags(ENTITY_FLAG_CASTSHADOW,0);
			if fCallbacks then
				for i,v in pairs(fCallbacks) do
					if type(v)=="function" then
						ent[i]=v;
					end
				end
				MergeTables(ent,fCallbacks);
			end
			if fakeClass then
				ent["is"..fakeClass]=true;
			end
			CryAction.CreateGameObjectForEntity(ent.id);
			CryAction.BindGameObjectToNetwork(ent.id);
			self.loadedObjects=self.loadedObjects+1;
			if not isother then
				self:PostFix(ent);
				--printf("%s class: %s",ent:GetName(),ent.class)
			end
			if not loaded then
				self.Indexer=self.Indexer+1;	--(#self.Maps[self.Map] or 0)+1
				self.Maps[self.Map][self.Indexer]={omdl,pos,dir,ignoreSave,vscale};
				if not multi then
					if pl then
						if not self.UndoStacks[pl.profile] then
							self.UndoStacks[pl.profile]=Stack:Create();
						end
						self.UndoStacks[pl.profile]:Push(ent);
					end
					if self.UseGlobalStack then
						self.UndoStack:Push(ent);
					end
					ent.__ID=self.Indexer;
				else
					ent.__ID=self.Indexer;
				end
			end		
		end
		if tellSp then
			printf("LevelDesigner: Spawned %s",ent:GetName());
		end
		return ent;
	end
	if SafeWriting.NumVersion>=253 then
	function LevelDesigner:OnTimerTick(player)
		local bunkers=System.GetEntitiesByClass("SpawnGroup");
		if bunkers and #bunkers>0 then
			for i,sp in pairs(bunkers) do
				if sp.isLDCapturable then
					local capts=System.GetEntitiesInSphereByClass(sp:GetWorldPos(),19,"Player");
					if sp.underattack then
						if not capts or (capts and #capts==0) then
							sp.allClients:ClCancelUncapture();
							sp.underattack=false;
						elseif capts then
							local enem=0;
							for i,v in pairs(capts) do
								if g_gameRules.game:GetTeam(v.id)~=sp.team then
									enem=enem+1;
								end
							end
							if enem==0 then
								sp.allClients:ClCancelUncapture();
								sp.underattack=false;
							end
						end
					end
				end
			end
		end
	end
	function LevelDesigner:UpdatePlayer(player)
		local ct=10;
		if g_gameRules.class~="PowerStruggle" then return; end
		if player:IsDead() then return; end
		local bzs=System.GetEntitiesInSphereByClass(player:GetWorldPos(),9,"BuyZone");
		local sps=System.GetEntitiesInSphereByClass(player:GetWorldPos(),19,"SpawnGroup");
		if sps and #sps>0 then
			local sp=sps[1];
			if sp then
				local team=g_gameRules.game:GetTeam(player.id);
				local spt=g_gameRules.game:GetTeam(sp.id);
				local capts=System.GetEntitiesInSphereByClass(sp:GetWorldPos(),19,"Player");
				local uscpts=0;
				local nkcpts=0;
				for i,v in pairs(capts) do
					local tm=g_gameRules.game:GetTeam(v.id);
					if v.host and not v:IsDead() then
						if tm==1 then nkcpts=nkcpts+1; elseif tm==2 then uscpts=uscpts+1; end
					end
				end
				local enems=false;
				if team==1 then enems=uscpts>0; else enems=nkcpts>0; end
				if spt~=team and sp.isLDCapturable then
					if not enems then
						sp.cap=sp.cap or 0;
						if sp.lastcap and (_time-sp.lastcap)>=2 then
							if spt==0 then sp.cap=0; else sp.cap=ct; end
							sp.lastcap=_time;
						end
						if spt==0 then
							sp.cap=math.min(ct,math.max(sp.cap,0));
							sp.cap=sp.cap+(_time-(sp.lastcap or _time));
							MessageStream:SendToTarget(player,"Capturing spawn point: %.2f %%",sp.cap*100/ct);
							if sp.cap>=ct then
								sp.team=team;
								MessageStream:SendToTarget(player,"Successfuly captured spawn point");
								sp.allClients:ClCapture(team)
								g_gameRules.game:SetTeam(team,sp.id);
								sp.lastcap=nil;
								for i,v in pairs(capts) do
									local tm=g_gameRules.game:GetTeam(v.id);
									if tm==team and (not v:IsDead()) then
										GivePoints(v,PowerStruggle.captureValue[1],PowerStruggle.cpList.CAPTURE)
									end
								end
							else
								sp.allClients:ClStepCapture(team,ct-sp.cap);
								sp.lastcap=_time;
							end
						else
							sp.cap=math.min(ct,math.max(sp.cap,0));
							sp.cap=sp.cap-(_time-(sp.lastcap or _time));
							if not sp.underattack then
								sp.allClients:ClStartUncapture(team);
								sp.underattack=true;
							end
							MessageStream:SendToTarget(player,"Uncapturing spawn point: %.2f %%",(ct-math.max(0,sp.cap))*100/ct);
							if sp.cap<=0 then
								local oldteam=sp.team;
								sp.team=0;
								sp.allClients:ClUncapture(team,oldteam)
								g_gameRules.game:SetTeam(0,sp.id); 								
							else
								sp.allClients:ClStepUncapture(team,sp.cap);
							end
							sp.lastcap=_time;
						end
					else
						sp.cap=sp.cap or 0;
						if spt==0 then
							MessageStream:SendToTarget(player,"Capturing spawn point: %.2f %% | Enemies are in zone!",sp.cap*100/ct);
						else
							MessageStream:SendToTarget(player,"Uncapturing spawn point: %.2f %% | Enemies are in zone!",math.max(0,(ct-math.max(0,sp.cap))*100/ct));
						end	
						sp.underattack=false;
						sp.allClients:ClStepUncapture(team,sp.cap);
						sp.allClients:ClCancelUncapture();
						sp.lastcap=_time;
					end
				elseif enems and spt==team and sp.isLDCapturable then
					MessageStream:SendToTarget(player,"Enemy is uncapturing spawn point: %.2f %%",math.max(0,(ct-math.max(0,sp.cap))*100/ct));
				end
			end
		end
		if bzs and #bzs>0 then
			local bz=bzs[1];
			local sp=System.GetEntitiesInSphereByClass(bz:GetPos(),20,"SpawnGroup");
			local myt=g_gameRules.game:GetTeam(player.id);
			local team=0;
			if sp and #sp>0 then team=g_gameRules.game:GetTeam(sp[1].id); end
			if (not player.inRealBZ) and myt==team  then
				player.wasBefore=player.buyFlags;
				player.buyFlags=bor(bor(PowerStruggle.BUY_AMMO, PowerStruggle.BUY_WEAPON), PowerStruggle.BUY_EQUIPMENT);
				player.GetBuyFlags=function(self)
					return self.buyFlags;
				end;
				g_gameRules.factories[1].allClients:ClSetBuyFlags(player.id, player.buyFlags);
				g_gameRules:OnEnterBuyZone(player, player);
				g_gameRules:OnEnterServiceZone(player, player);
				player.inRealBZ=true;
			end
		else
			if player.inRealBZ and (not player.wasBefore) then
				g_gameRules:OnLeaveBuyZone(player, player);
				g_gameRules:OnLeaveServiceZone(player, player);
				player.GetBuyFlags=nil;
				player.inRealBZ=false;
				player.buyFlags=nil;
			end
		end
	end
	end	--/if 253
	function LevelDesigner:CheckPlayer(pl)
		if self.BuildC[pl.profile] then
			pl.lbuilt=self.BuildC[pl.profile];
		end
	end
	function LevelDesigner:GetModel(model)
		local scale,mdlname = string.match(model,"(.-);(.*)");
		local scalex,scaley,scalez=1,1,1;
		if scale then
			model = mdlname;
			local sx,sy,sz = string.match(scale,"(.-),(.-),(.*)");
			--printf("%s, %s, %s",sx,sy,sz);
			if not sx and not sy and not sz then
				local numScale = tonumber(scale);
				scalex,scaley,scalez = numScale,numScale,numScale;
			else
				scalex,scaley,scalez=tonumber(sx),tonumber(sy),tonumber(sz);
			end
		end
		if not scalex then scalex = 1; end
		if not scaley then scaley = 1; end
		if not scalez then scalez = 1; end
		local finalScale = {scalex,scaley,scalez};
		local mdl,num=string.match(model,"(.*):(.*)");
		if not mdl then mdl=model; end
		if mdl then
			local _mdl,idx=string.match(model,"(.*)%[(%d+)%]");
			if idx and _mdl then mdl=_mdl; end		
			if self.ShortenModels[mdl] then
				mdl=self.ShortenModels[mdl];
				if type(mdl)=="table" and idx then
					mdl=mdl[tonumber(idx)];
				end
				if num then mdl=mdl..num; end
			end
			if mdl:sub(1,("objects/"):len()):lower()~="objects/" then
				if mdl:sub(1,1) == "$" then return mdl:sub(2); end
				mdl="objects/"..mdl;
			end
		end
		if not string.find(mdl,".cgf",nil,true) and not string.find(mdl,".cdf",nil,true) then
			if not string.find(mdl,"Characters/",nil,true) then
				mdl=mdl..".cgf";
			else mdl=mdl..".cdf"; end
		end
		return mdl,finalScale,mdlname;
	end
	function LevelDesigner:GetModification(model)
		local mdl,modi=string.match(model,"(.*)%[(%w+)%]");
		if modi then return mdl,modi; else return model,"MP"; end
	end
	function LevelDesigner:GetAttachments(model)
		if not model then return end
		local mdl,atch=string.match(model,"(.*)%{(.-)%}");
		local tbl={};
		if mdl then
			mdl=self:GetModification(mdl);
		else
			mdl=self:GetModification(model);
		end
		if atch then
			atch:gsub("(%w+)",function(a) tbl[#tbl+1]=a; end);
			return mdl,tbl;
		else
			return mdl,nil;
		end
	end
	function LevelDesigner:IsAnother(model)
		if model:sub(1,1)==":" then
			return true,model:sub(2);
		end
		return nil,model;
	end
	function LevelDesigner:Undo(pl)
		pl.lbuilt=pl.lbuilt or 0;
		--local tgt=pl;
		--if not tgt and self.UseGlobalStack then tgt=self; end
		local us=self.UndoStacks[pl.profile];
		if not self.UndoStacks[pl.profile] then self.UndoStacks[pl.profile]=Stack:Create(); end
		if self.UndoStacks[pl.profile]:IsEmpty() then
			printf("UndoStack is empty!");
			return;
		end
		local ent=self.UndoStacks[pl.profile]:Pop();
		if self.CollectionStarted then
			local t=self.CollectionStack:Pop();
		end
		if ent then
			local pos,dir,mdl;
			mdl=ent.__MDLNAME;
			pos=ent.__POS;
			dir=ent.__DIR;
			scale=ent.__SCALE or {1,1,1};
			if self.Maps[self.Map] then
				if ent.__ISMULTISPAWNED then
					for i,v in pairs(ent) do
						if tostring(i):sub(1,2)~="__" then
							self.Maps[self.Map][v.__ID]=nil;
							System.RemoveEntity(v.id);
							pl.lbuilt=pl.lbuilt-1;
						end
					end
				else
					self.Maps[self.Map][ent.__ID]=nil;--table.remove(self.Maps[self.Map],ent.__ID);
					System.RemoveEntity(ent.id);
					pl.lbuilt=pl.lbuilt-1;
				end
			end	
			return pos,dir,mdl,scale;
		end
	end
	function LevelDesigner:GetLast(pl)
		if not self.UndoStacks then self.UndoStacks={}; end
		if not self.UndoStacks[pl.profile] then self.UndoStacks[pl.profile]=Stack:Create(); end
		local ent=self.UndoStacks[pl.profile]:Top();
		local exists=true;
		if not ent then exists=false; ent={}; end
		local pos,dir,mdl,scale;
		pos={};
		dir={};
		scale={};
		mdl=ent.__MDLNAME;
		MergeTables(pos,ent.__POS);
		MergeTables(dir,ent.__DIR);
		MergeTables(scale,ent.__SCALE or {1,1,1});
		if not exists then ent=nil; end
		return ent,mdl,pos,dir,scale;
	end
	function LevelDesigner:Save(file)
		local f,err=io.open(self:GetPath(file or "LevelDesignerData.lua"),"w");
		local tbl={}; MergeTables(tbl,self.Maps);
		tbl[self.Map]={};
		local ignored=0;
		for i,v in pairs(self.Maps[self.Map]) do
			local add=true;
			if v[4] and tostring(v[4])=="SAVEIGNORE" then add=false; end
			if v[5] then	--scales
				v[4] = v[5];
			end
			if add then
				tbl[self.Map][#tbl[self.Map]+1]=v;
			else
				ignored=ignored + 1;
			end
		end
		--print("Ignored objects: ",ignored);
		if not f then print("Error when opening the file!"); return; end
		f:write(arr2str(tbl,"LevelDesigner.Maps"));
		f:close();
	end
	function LevelDesigner:Clear()
		self.Maps[GetMapName()]={};
		for i,v in pairs(self.UndoStacks) do v={}; end
		local objs=System.GetEntities();
		local c=0;
		if objs then for i,v in pairs(objs) do if v.ldSpawned then System.RemoveEntity(v.id); c=c+1; end end end
		return c;
	end
	function LevelDesigner:SaveCollection(name)
		local collections={};
		if not self.CollectionStack:IsEmpty() then
			self.ShortenModels[name]={
				ISCOLLECTION=true;
				COLLTABLE={};
			};
			while not self.CollectionStack:IsEmpty() do
				local tbl=self.CollectionStack:Pop();
				self.ShortenModels[name].COLLTABLE[#self.ShortenModels[name].COLLTABLE+1]=tbl;
			end
		end
		for i,v in pairs(self.ShortenModels) do
			if type(v)=="table" then
				if v.ISCOLLECTION then
					collections[i]=v;
				end
			end
		end
		local f,err=io.open(self:GetPath("LevelDesignerCollections.lua"),"w");
		if not f then print("Error when opening the file!"); return; end
		f:write(arr2str(collections,"LevelDesigner_Collections"));
		f:close();
	end
	function LevelDesigner:GetPath(path) return self.OutFolder..path; end
	function MAKE_RANGE(num,r)
		if num>r then
			return -r+(num%r);
		elseif num<-r then
			return r-(num%r);
		else
			return num;
		end
	end
	function getangle(a,f,f2)
		--if a>90 then return f2(a); end
		--if a<90 then if a>-90 then return f2(a); end return f(a); end
		--return f(a);
		if a<0 then return f2(a); end
		return f(a);
	end
	function setangle(a,f,f2)
		--if a>90 then return f2(a); end
		--if a<90 then if a>-90 then return f2(a); end return f(a); end
		--return f(a);
		if a<0 then return f2(a); end
		return f(a);
	end
	function LevelDesigner:PrepareAll()
		self.BuildC={};
		self:Init();
	end
	function LevelDesigner:SafeCall(cmd,player,msg)
		Script.SetTimer(2,function()
		if HavePrivileges(player,cmd) then
			_pcall(ExecuteCommand,cmd,player,msg);
		end
		end);
	end
	function LevelDesigner:OnBuy(player,_itemName,ok)
		if (not ok) and player then
			local itemName,cnt=unpack(CTableToLuaTable(split(_itemName,"__")));
			cnt=cnt or 1.0;
			if itemName and cnt then
				if itemName=="ldmovx" or itemName=="ldmovex" then			
					_pcall(self.SafeCall,self,"ldmovex",player,"!ldmovex "..cnt);
				elseif itemName=="ldmovy" or itemName=="ldmovey" then
					_pcall(self.SafeCall,self,"ldmovey",player,"!ldmovey "..cnt);
				elseif itemName=="ldmovz" or itemName=="ldmovez" or itemName=="ldlift" then
					_pcall(self.SafeCall,self,"ldlift",player,"!ldlift "..cnt);
				elseif itemName=="ldundo" or itemName=="lddelete" then
					_pcall(self.SafeCall,self,"ldundo",player,"!ldundo");
				elseif itemName=="ldrotx" then
					_pcall(self.SafeCall,self,"ldrotx",player,"!ldrotx "..cnt);
				elseif itemName=="ldroty" then
					_pcall(self.SafeCall,self,"ldroty",player,"!ldroty "..cnt);
				elseif itemName=="ldrotz" then
					_pcall(self.SafeCall,self,"ldrotz",player,"!ldrotz "..cnt);
				elseif itemName=="ldcopy" then
					_pcall(self.SafeCall,self,"ldcopy",player,"ldcopy");
				elseif itemName=="ldfwd" then
					_pcall(self.SafeCall,self,"ldfwd",player,"!ldfwd "..cnt);
				elseif itemName=="ldside" then
					_pcall(self.SafeCall,self,"ldside",player,"!ldside "..cnt);
				end
			end
		end
	end
	function LevelDesigner:OnShoot(hit)
		local weapon=hit.weapon;
		local shooter=hit.shooter;
		if shooter and weapon and shooter.host then
			local hittbl={};
			local posvec = shooter.actor:GetHeadPos();
			local dirvec = shooter.actor:GetHeadDir();
			dirvec.x = dirvec.x * 4000;
			dirvec.y = dirvec.y * 4000;
			dirvec.z = dirvec.z * 4000;
			local hits=Physics.RayWorldIntersection(posvec, dirvec, 10, ent_all, shooter.id, nil, hittbl);
			if (hits>0) then
				local info=hittbl[1];			
				if weapon.isLDGun and shooter.LDGunMdl and IsAdmin(shooter) then
					local dir={};
					local pos={};
					MergeTables(dir,shooter.actor:GetHeadDir());
					MergeTables(pos,info.pos);
					if not self.LDGunRealDir then
						dir.z=0;
					end
					Script.SetTimer(1,function()
						self:Spawn(shooter.LDGunMdl,pos,dir,false,false,true,nil,shooter);
					end);
				elseif weapon.isBuildGun and shooter.BGunMdl and IsPremium(shooter) then
					local dir={};
					local pos={};
					MergeTables(dir,shooter.actor:GetHeadDir());
					MergeTables(pos,info.pos);
					if not self.LDGunRealDir then
						dir.z=0;
					end
					Script.SetTimer(1,function()
						self:Spawn(shooter.BGunMdl,pos,dir,false,false,true,nil,shooter,true);
					end);
				end
			end
		end
	end
	CreateClass("LDGun","SOCOM");
	CreateClass("BuildGun","SOCOM");
	System.LogAlways("Loading LevelDesigner")
	AddChatCommand("ldlist",function(self,sender,msg)
		local mdls={};
		for i,v in pairs(LevelDesigner.ShortenModels) do
			mdls[#mdls+1]=i;
		end
		table.sort(mdls);
		for j,w in ipairs(mdls) do
			local i,v=w,LevelDesigner.ShortenModels[w];
			if type(v)=="string" then
				Console:SendToTarget(sender," $3%s$8 - $5%s",i,v);
			else
				local msg="$6>>$3%s $8- $5%s";
				Console:SendToTarget(sender,msg,i,v.ISCOLLECTION and "$9collection of entities" or v[1]);
			end
		end
		Msg:SendToTarget(sender,__qt(sender.lang,R.OPEN_CONSOLE));
	end,{},{AdminOnly=true;},"shows list of available objects to spawn");
	AddChatCommand("ld",function(self,sender,msg,dist,obj)
		Script.SetTimer(1,function()
			if not dist then Chat:SendToTarget(nil,sender,"Enter valid distance"); return; end
			if not obj then Chat:SendToTarget(nil,sender,"Enter valid object"); return; end
			local pos,dir=Spawn:CalculatePosition(sender,dist);
			LevelDesigner:Spawn(obj,pos,dir,nil,nil,nil,nil,sender);
		end);
	end,{DOUBLE,TEXT},{AdminOnly=true;},"Spawns object into level designer",true);
	AddChatCommand("ldz",function(self,sender,msg,dist,z,obj)
		Script.SetTimer(1,function()
			if not dist then Chat:SendToTarget(nil,sender,"Enter valid distance"); return; end
			if not z then Chat:SendToTarget(nil,sender,"Enter valid height"); return; end
			if not obj then Chat:SendToTarget(nil,sender,"Enter valid object"); return; end
			local pos,dir=Spawn:CalculatePosition(sender,dist);
			LevelDesigner:Spawn(obj,pos,dir,false,false,false,z,sender);
		end);
	end,{DOUBLE,DOUBLE,TEXT},{AdminOnly=true;},"Spawns object into level designer with Z offset from height at that position",true);
	AddChatCommand("ldf",function(self,sender,msg,dist,obj)
		Script.SetTimer(1,function()
			if not dist then Chat:SendToTarget(nil,sender,"Enter valid distance"); return; end
			if not obj then Chat:SendToTarget(nil,sender,"Enter valid object"); return; end
			local pos,dir=Spawn:CalculatePosition(sender,dist);
			pos.z=sender:GetPos().z;
			LevelDesigner:Spawn(obj,pos,dir,false,false,true,nil,sender);
		end);
	end,{DOUBLE,TEXT},{AdminOnly=true;},"Spawns object into level designer with forced Z equal to player's position Z",true);
	AddChatCommand("ldfz",function(self,sender,msg,dist,z,obj)
		Script.SetTimer(1,function()
			if not dist then Chat:SendToTarget(nil,sender,"Enter valid distance"); return; end
			if not z then Chat:SendToTarget(nil,sender,"Enter valid height"); return; end
			if not obj then Chat:SendToTarget(nil,sender,"Enter valid object"); return; end
			local pos,dir=Spawn:CalculatePosition(sender,dist);
			pos.z=sender:GetPos().z+z;
			LevelDesigner:Spawn(obj,pos,dir,false,false,true,nil,sender);
		end);
	end,{DOUBLE,DOUBLE,TEXT},{AdminOnly=true;},"Spawns object into level designer with forced Z",true);	
	AddChatCommand("ldgetangles",function(self,sender,msg)
		local ent,mdl,pos,dir=LevelDesigner:GetLast(sender);
		if ent then
			local raddeg=180/math.pi;
			local x,y,z=dir.x,dir.y,dir.z;
			x,y,z=math.asin(x)*raddeg,math.asin(y)*raddeg,math.asin(z)*raddeg;
			Chat:SendToTarget(nil,sender," x: %.2f, y: %.2f, z: %.2f",x,y,z);
		end
	end,{},{AdminOnly=true;},"Tells you angles of last spawned entity");
	AddChatCommand("ldsetangles",function(self,sender,msg,x,y,z)
		Script.SetTimer(1,function()
			local ent,mdl,pos,dir,scale=LevelDesigner:GetLast(sender);
			if not ent then Chat:SendToTarget(nil,sender,"Please, spawn atleast one new entity!"); return; end
			local err=true;
			local raddeg=180/math.pi;
			if x then
				dir.x=x;
				err=false;
			end
			if y then
				dir.y=y;
				err=false;
			end
			if z then
				dir.z=z;
				err=false;
			end
			if err then Chat:SendToTarget(nil,sender,"Please, enter atleast one valid angle, usage !ldsetangles x y z!"); return; end
			LevelDesigner:Undo(sender);
			LevelDesigner:Spawn(mdl,pos,dir,false,false,true,nil,sender,nil,scale);
		end);
	end,{DOUBLE,DOUBLE,DOUBLE},{AdminOnly=true;},"Sets angles of last spawned entity",true);
	AddChatCommand("ldrotx",function(self,sender,msg,q)
		Script.SetTimer(1,function()
			local ent,mdl,pos,dir,scale=LevelDesigner:GetLast(sender);
			if not ent then Chat:SendToTarget(nil,sender,"Please, spawn atleast one new entity!"); return; end
			q=q or 0;
			local x,y,z=dir.x,dir.y,dir.z;
			local rot={
				1,      0,      0,
				0, cos(q),-sin(q),
				0, sin(q), cos(q),
			};
			mulMat33_13f(rot,dir);
			LevelDesigner:Undo(sender);
			LevelDesigner:Spawn(mdl,pos,dir,false,false,true,nil,sender,nil,scale);
		end);
	end,{DOUBLE},{AdminOnly=true;},"Rotates entity on axis X",true);
	AddChatCommand("ldroty",function(self,sender,msg,q)
		Script.SetTimer(1,function()
			local ent,mdl,pos,dir,scale=LevelDesigner:GetLast(sender);
			if not ent then Chat:SendToTarget(nil,sender,"Please, spawn atleast one new entity!"); return; end
			q=q or 0;
			local x,y,z=dir.x,dir.y,dir.z;
			local rot={
				cos(q), 0, sin(q),
				0,      1,      0,
				-sin(q),0, cos(q),
			};
			mulMat33_13f(rot,dir);
			LevelDesigner:Undo(sender);
			LevelDesigner:Spawn(mdl,pos,dir,false,false,true,nil,sender,nil,scale);
		end);
	end,{DOUBLE},{AdminOnly=true;},"Rotates entity on axis Y",true);
	AddChatCommand("ldrotz",function(self,sender,msg,q)
		Script.SetTimer(1,function()
			local ent,mdl,pos,dir,scale=LevelDesigner:GetLast(sender);
			if not ent then Chat:SendToTarget(nil,sender,"Please, spawn atleast one new entity!"); return; end
			q=q or 0;
			local x,y,z=dir.x,dir.y,dir.z;
			dir.x=x*cos(q)-y*sin(q)
			dir.y=x*sin(q)+y*cos(q)
			LevelDesigner:Undo(sender);
			LevelDesigner:Spawn(mdl,pos,dir,false,false,true,nil,sender,nil,scale);
		end);
	end,{DOUBLE},{AdminOnly=true;},"Rotates entity on axis Z",true);
	AddChatCommand("ldlift",function(self,sender,msg,z)
		Script.SetTimer(1,function()
			local ent,mdl,pos,dir,scale=LevelDesigner:GetLast(sender);
			if not ent then Chat:SendToTarget(nil,sender,"Please, spawn atleast one new entity!"); return; end
			z=z or 0;
			pos.z=pos.z+z;
			LevelDesigner:Undo(sender);
			LevelDesigner:Spawn(mdl,pos,dir,false,false,true,nil,sender,nil,scale);
		end);
	end,{DOUBLE},{AdminOnly=true;},"Lifts last spawned entity",true);
	
	AddChatCommand("ldscale",function(self,sender,msg,x,y,z)
		Script.SetTimer(1,function()
			if not x then x = 1; end
			if not y and not z then y = x; z = x; end
			local ent,mdl,pos,dir,scale=LevelDesigner:GetLast(sender);
			if not ent then Chat:SendToTarget(nil,sender,"Please, spawn atleast one new entity!"); return; end
			LevelDesigner:Undo(sender);
			LevelDesigner:Spawn(mdl,pos,dir,false,false,true,nil,sender,nil,{x,y,z});
		end);
	end,{DOUBLE,DOUBLE,DOUBLE},{AdminOnly=true;},"Scales last spawned entity",true);
	
	AddChatCommand("ldmovex",function(self,sender,msg,x)
		Script.SetTimer(1,function()
			local ent,mdl,pos,dir,scale=LevelDesigner:GetLast(sender);
			if not ent then Chat:SendToTarget(nil,sender,"Please, spawn atleast one new entity!"); return; end
			x=x or 0;
			pos.x=pos.x+x;
			LevelDesigner:Undo(sender);
			LevelDesigner:Spawn(mdl,pos,dir,false,false,true,nil,sender,nil,scale);
		end);
	end,{DOUBLE},{AdminOnly=true;},"Moves entity on axis X");
	AddChatCommand("ldmovey",function(self,sender,msg,y)
		Script.SetTimer(1,function()
			local ent,mdl,pos,dir,scale=LevelDesigner:GetLast(sender);
			if not ent then Chat:SendToTarget(nil,sender,"Please, spawn atleast one new entity!"); return; end
			y=y or 0;
			pos.y=pos.y+y;
			LevelDesigner:Undo(sender);
			LevelDesigner:Spawn(mdl,pos,dir,false,false,true,nil,sender,nil,scale);
		end);
	end,{DOUBLE},{AdminOnly=true;},"Moves entity on axis Y");
	AddChatCommand("ldfwd",function(self,sender,msg,a)
		Script.SetTimer(1,function()
			local ent,mdl,pos,dir,scale=LevelDesigner:GetLast(sender);
			if not ent then Chat:SendToTarget(nil,sender,"Please, spawn atleast one new entity!"); return; end
			local plDir=sender:GetDirectionVector();
			local oz=pos.z;
			ScaleVectorInPlace(plDir,a);
			FastSumVectors(pos,pos,plDir);
			pos.z=oz;
			LevelDesigner:Undo(sender);
			LevelDesigner:Spawn(mdl,pos,dir,false,false,true,nil,sender,nil,scale);
		end);
	end,{DOUBLE},{AdminOnly=true;},"Moves entity forward by player's look");
	AddChatCommand("ldside",function(self,sender,msg,a)
		Script.SetTimer(1,function()
			local ent,mdl,pos,dir_,scale=LevelDesigner:GetLast(sender);
			if not ent then Chat:SendToTarget(nil,sender,"Please, spawn atleast one new entity!"); return; end
			local dir=sender:GetDirectionVector();
			local oz=pos.z;
			local x,y,z=dir.x,dir.y,dir.z;
			dir.x=x*cos(90)-y*sin(90)
			dir.y=x*sin(90)+y*cos(90)
			ScaleVectorInPlace(dir,a);
			FastSumVectors(pos,pos,dir);
			pos.z=oz;
			LevelDesigner:Undo(sender);
			LevelDesigner:Spawn(mdl,pos,dir_,false,false,true,nil,sender,nil,scale);
		end);
	end,{DOUBLE},{AdminOnly=true;},"Moves entity forward by player's look");
	AddChatCommand("ldundo",function(self,pl,msg,amount)
		if not LevelDesigner.UndoStacks[pl.profile] then LevelDesigner.UndoStacks[pl.profile]=Stack:Create(); end
		if not LevelDesigner.UndoStacks[pl.profile]:IsEmpty() then
			amount=amount or 1;
			for i=1,amount do
				 if not LevelDesigner.UndoStacks[pl.profile]:IsEmpty() then
					LevelDesigner:Undo(pl);
				 end
			end
		else
			Chat:SendToTarget(nil,pl,"Please, spawn atleast one new entity!");
		end
	end,{INT},{AdminOnly=true;},"Does undo in level designer,usage !ldundo [optional: count]");
	AddChatCommand("ldcopy",function(self,sender)
		Script.SetTimer(1,function()
			local ent,mdl,pos,dir,scale=LevelDesigner:GetLast(sender);
			if not ent then Chat:SendToTarget(nil,sender,"Please, spawn atleast one new entity!"); return; end
			LevelDesigner:Spawn(mdl,pos,dir,false,false,true,nil,sender,nil,scale);
		end);
	end,{},{AdminOnly=true;},"Creates copy of entity on same place");
	AddChatCommand("ldrset",function(self,sender,msg,model)
		if model then
			LevelDesigner.RangeMdl=model;
			Chat:SendToTarget(nil,sender," !ldrange will now automatically use ,%s' if model is not set",LevelDesigner.RangeMdl);
		else
			Chat:SendToTarget(nil,sender,"Please enter valid model name!");
		end
	end,{TEXT},{AdminOnly=true;},"Sets model for following use of !ldrange")
	AddChatCommand("ldgset",function(self,sender,msg,model)
		if model then
			sender.LDGunMdl=model;
			Chat:SendToTarget(nil,sender," LDGun will now automatically use ,%s' if model is not set",sender.LDGunMdl);
		else
			Chat:SendToTarget(nil,sender,"Please enter valid model name!");
		end
	end,{TEXT},{AdminOnly=true;},"Sets model for LDGun.")
	AddChatCommand("ldgun",function(self,sender,msg,model)
		Spawn:Entity(sender,1,model or "SOCOM",function(e)
			e.isLDGun=true;
		end);
	end,{TEXT},{AdminOnly=true;},"Spawns a LD gun optional parameter: class.");
	AddChatCommand("ldgdir",function(self,sender)
		if not LevelDesigner.LDGunRealDir then
			Chat:SendToTarget(nil,sender,"Using of real Z orientation of hit enabled");
			LevelDesigner.LDGunRealDir=true;
		else
			Chat:SendToTarget(nil,sender,"Using of real Z orientation of hit disabled");
			LevelDesigner.LDGunRealDir=false;
		end
	end,nil,{AdminOnly=true;},"Enables or disables using real Z orientation of hit, default = disabled");
	AddChatCommand("ldrangef",function(self,sender,msg,szX,szY,szZ,nX,nY,nZ,angX,angY,angZ)
		local ent=LevelDesigner.RangeMdl;
		if not ent then
			Chat:SendToTarget(nil,sender,"Please set valid model name by using !ldrset <modelname>");
		end
		angX=angX or 1;
		angY=angY or 0;
		angZ=angZ or 0;
		szX=math.max(szX,0.2);
		szY=math.max(szY,0.2);
		szZ=math.max(szZ,0.2);
		local c=0;
		local pos=sender:GetPos();
		for x=pos.x,pos.x+(nX*szX),szX do
			for y=pos.y,pos.y+(nY*szY),szY do
				for z=pos.z,pos.z+(nZ*szZ),szZ do
					local npos={};
					npos.z=z;
					npos.x=x;
					npos.y=y;
					LevelDesigner:Spawn(ent,npos,{x=angX;y=angY;z=angZ;},false,false,true,nil,sender);
					c=c+1;
				end
			end
		end
		Chat:SendToTarget(nil,sender,"Spawned %d entities",c);
	end,{NUMBER,NUMBER,NUMBER,INT,INT,INT,NUMBER,NUMBER,NUMBER},{AdminOnly=true;},"Spawns entities in range, usage: !ldrange sizeX sizeY sizeZ countX countY countZ angX angY angZ");
	AddChatCommand("ldrange",function(self,sender,msg,szX,szY,szZ,nX,nY,nZ,angX,angY,angZ)
		local ent=LevelDesigner.RangeMdl;
		if not ent then
			Chat:SendToTarget(nil,sender,"Please set valid model name by using !ldrset <modelname>");
		end
		angX=angX or 1;
		angY=angY or 0;
		angZ=angZ or 0;
		szX=math.max(szX,0.2);
		szY=math.max(szY,0.2);
		szZ=math.max(szZ,0.2);
		local c=0;
		local pos=sender:GetPos();
		local startZ=System.GetTerrainElevation(pos);
		for x=pos.x,pos.x+(nX*szX),szX do
			for y=pos.y,pos.y+(nY*szY),szY do
				for z=startZ,startZ+(nZ*szZ),szZ do
					local npos={};
					npos.z=z;
					npos.x=x;
					npos.y=y;
					LevelDesigner:Spawn(ent,npos,{x=angX;y=angY;z=angZ;},false,false,false,z-startZ,sender);
					c=c+1;
				end
			end
		end
		Chat:SendToTarget(nil,sender,"Spawned %d entities",c);
	end,{NUMBER,NUMBER,NUMBER,INT,INT,INT,NUMBER,NUMBER,NUMBER},{AdminOnly=true;},"Spawns entities in range on terrain, usage: !ldrange sizeX sizeY sizeZ countX countY countZ angX angY angZ");
	AddChatCommand("ldsave",function(self,sender,msg,file)
		LevelDesigner:Save(file);
		Msg:SendToAll("Admin %s saves the map","info",sender:GetName());
	end,{WORD},{AdminOnly=true;},"Saves the map");
	AddChatCommand("ldclear",function(self,sender,msg)
		local c=LevelDesigner:Clear(file);
		Chat:SendToAll(nil,"Admin %s cleared the map (%d entities removed)",sender:GetName(),c);
	end,{},{AdminOnly=true;},"Clears the map");
	AddChatCommand("lddebug",function(self,sender,msg)
		Chat:SendToTarget(nil,sender,"There are %d entities spawned on this map",count(LevelDesigner.Maps[LevelDesigner.Map]));
	end,{},{AdminOnly=true;});
	SafeWriting.FuncContainer:LoadPlugin(LevelDesigner);
	AddChatCommand("ldcollstart",function(self,sender)
		Chat:SendToTarget(nil,sender,"Starting recording a collection");
		LevelDesigner.CollectionStarted=true;
		LevelDesigner.CollectionStart=sender:GetPos();
	end,{},{AdminOnly=true;});
	AddChatCommand("ldcollstop",function(self,sender)
		Chat:SendToTarget(nil,sender,"Stopped recording the collection");
		LevelDesigner.CollectionStarted=false;
	end,{},{AdminOnly=true;});
	AddChatCommand("ldcollsave",function(self,sender,msg,name)
		Chat:SendToTarget(nil,sender,"Saving your collection of entities as %s",name);
		LevelDesigner.CollectionStarted=true;
		LevelDesigner:SaveCollection(name);
		LevelDesigner.CollectionStarted=false;
		Chat:SendToTarget(nil,sender,"Collection stack is now empty(%s) and collection recording was stopped",tostring(LevelDesigner.CollectionStack:IsEmpty()));
	end,{TEXT},{AdminOnly=true;});
	
	if LevelDesigner.AllowPremiumBuilders or SafeWriting.Settings.EnablePremiumBuilders then
		AddChatCommand("build",function(self,sender,msg,dist,obj)
			Script.SetTimer(1,function()
				if not dist then Chat:SendToTarget(nil,sender,"Enter valid distance"); return; end
				if not obj then Chat:SendToTarget(nil,sender,"Enter valid object"); return; end
				local pos,dir=Spawn:CalculatePosition(sender,dist);
				LevelDesigner:Spawn(obj,pos,dir,nil,nil,nil,nil,sender,true);
			end);
		end,{DOUBLE,TEXT},{PremiumOnly=true;},"Spawns object into level designer",true);
		AddChatCommand("buildf",function(self,sender,msg,dist,obj)
			Script.SetTimer(1,function()
				if not dist then Chat:SendToTarget(nil,sender,"Enter valid distance"); return; end
				if not obj then Chat:SendToTarget(nil,sender,"Enter valid object"); return; end
				local pos,dir=Spawn:CalculatePosition(sender,dist);
				pos.z=sender:GetPos().z;
				LevelDesigner:Spawn(obj,pos,dir,false,false,true,nil,sender,true);
			end);
		end,{DOUBLE,TEXT},{PremiumOnly=true;},"Spawns object into level designer with forced Z equal to player's position Z",true);
		AddChatCommand("buildfz",function(self,sender,msg,dist,z,obj)
			Script.SetTimer(1,function()
				if not dist then Chat:SendToTarget(nil,sender,"Enter valid distance"); return; end
				if not z then Chat:SendToTarget(nil,sender,"Enter valid height"); return; end
				if not obj then Chat:SendToTarget(nil,sender,"Enter valid object"); return; end
				local pos,dir=Spawn:CalculatePosition(sender,dist);
				pos.z=sender:GetPos().z+z;
				LevelDesigner:Spawn(obj,pos,dir,false,false,true,nil,sender,true);
			end);
		end,{DOUBLE,DOUBLE,TEXT},{PremiumOnly=true;},"Spawns object into level designer with forced Z",true);
		AddChatCommand("rotx",function(self,sender,msg,q)
			Script.SetTimer(1,function()
				local ent,mdl,pos,dir,scale=LevelDesigner:GetLast(sender);
				if not ent then Chat:SendToTarget(nil,sender,"Please, spawn atleast one new entity!"); return; end
				q=q or 0;
				local x,y,z=dir.x,dir.y,dir.z;
				local rot={
					1,      0,      0,
					0, cos(q),-sin(q),
					0, sin(q), cos(q),
				};
				mulMat33_13f(rot,dir);
				LevelDesigner:Undo(sender);
				LevelDesigner:Spawn(mdl,pos,dir,false,false,true,nil,sender,true,scale);
			end);
		end,{DOUBLE},{PremiumOnly=true;},"Rotates entity on axis X",true);
		AddChatCommand("roty",function(self,sender,msg,q)
			Script.SetTimer(1,function()
				local ent,mdl,pos,dir,scale=LevelDesigner:GetLast(sender);
				if not ent then Chat:SendToTarget(nil,sender,"Please, spawn atleast one new entity!"); return; end
				q=q or 0;
				local x,y,z=dir.x,dir.y,dir.z;
				local rot={
					cos(q), 0, sin(q),
					0,      1,      0,
					-sin(q),0, cos(q),
				};
				mulMat33_13f(rot,dir);
				LevelDesigner:Undo(sender);
				LevelDesigner:Spawn(mdl,pos,dir,false,false,true,nil,sender,true,scale);
			end);
		end,{DOUBLE},{PremiumOnly=true;},"Rotates entity on axis Y",true);
		AddChatCommand("rotz",function(self,sender,msg,q)
			Script.SetTimer(1,function()
				local ent,mdl,pos,dir,scale=LevelDesigner:GetLast(sender);
				if not ent then Chat:SendToTarget(nil,sender,"Please, spawn atleast one new entity!"); return; end
				q=q or 0;
				local x,y,z=dir.x,dir.y,dir.z;
				dir.x=x*cos(q)-y*sin(q)
				dir.y=x*sin(q)+y*cos(q)
				LevelDesigner:Undo(sender);
				LevelDesigner:Spawn(mdl,pos,dir,false,false,true,nil,sender,true,scale);
			end);
		end,{DOUBLE},{PremiumOnly=true;},"Rotates entity on axis Z",true);
		AddChatCommand("lift",function(self,sender,msg,z)
			Script.SetTimer(1,function()
				local ent,mdl,pos,dir,scale=LevelDesigner:GetLast(sender);
				if not ent then Chat:SendToTarget(nil,sender,"Please, spawn atleast one new entity!"); return; end
				z=z or 0;
				pos.z=pos.z+z;
				LevelDesigner:Undo(sender);
				LevelDesigner:Spawn(mdl,pos,dir,false,false,true,nil,sender,true,scale);
			end);
		end,{DOUBLE},{PremiumOnly=true;},"Lifts last spawned entity",true);
		
		AddChatCommand("scale",function(self,sender,msg,x,y,z)
			Script.SetTimer(1,function()
				if not x then x = 1; end
				if not y and not z then y = x; z = x; end
				local ent,mdl,pos,dir,scale=LevelDesigner:GetLast(sender);
				if not ent then Chat:SendToTarget(nil,sender,"Please, spawn atleast one new entity!"); return; end
				LevelDesigner:Undo(sender);
				LevelDesigner:Spawn(mdl,pos,dir,false,false,true,nil,sender,true,{x,y,z});
			end);
		end,{DOUBLE,DOUBLE,DOUBLE},{AdminOnly=true;},"Scales last spawned entity",true);
		
		AddChatCommand("fwd",function(self,sender,msg,a)
			Script.SetTimer(1,function()
				local ent,mdl,pos,dir,scale=LevelDesigner:GetLast(sender);
				if not ent then Chat:SendToTarget(nil,sender,"Please, spawn atleast one new entity!"); return; end
				local plDir=sender:GetDirectionVector();
				local oz=pos.z;
				ScaleVectorInPlace(plDir,a);
				FastSumVectors(pos,pos,plDir);
				pos.z=oz;
				LevelDesigner:Undo(sender);
				LevelDesigner:Spawn(mdl,pos,dir,false,false,true,nil,sender,true,scale);
			end);
		end,{DOUBLE},{PremiumOnly=true;},"Moves entity forward by player's look");
		AddChatCommand("side",function(self,sender,msg,a)
			Script.SetTimer(1,function()
				local ent,mdl,pos,dir_,scale=LevelDesigner:GetLast(sender);
				if not ent then Chat:SendToTarget(nil,sender,"Please, spawn atleast one new entity!"); return; end
				local dir=sender:GetDirectionVector();
				local oz=pos.z;
				local x,y,z=dir.x,dir.y,dir.z;
				dir.x=x*cos(90)-y*sin(90)
				dir.y=x*sin(90)+y*cos(90)
				ScaleVectorInPlace(dir,a);
				FastSumVectors(pos,pos,dir);
				pos.z=oz;
				LevelDesigner:Undo(sender);
				LevelDesigner:Spawn(mdl,pos,dir_,false,false,true,nil,sender,true,scale);
			end);
		end,{DOUBLE},{PremiumOnly=true;},"Moves entity forward by player's look");
		AddChatCommand("undo",function(self,pl,msg,amount)
			if not LevelDesigner.UndoStacks[pl.profile] then LevelDesigner.UndoStacks[pl.profile]=Stack:Create(); end
			if not LevelDesigner.UndoStacks[pl.profile]:IsEmpty() then
				amount=amount or 1;
				for i=1,amount do
					 if not LevelDesigner.UndoStacks[pl.profile]:IsEmpty() then
						LevelDesigner:Undo(pl);
					 end
				end
			else
				Chat:SendToTarget(nil,pl,"Please, spawn atleast one new entity!");
			end
		end,{INT},{PremiumOnly=true;},"Does undo in level designer,usage !undo [optional: count]");
		AddChatCommand("copy",function(self,sender)
			Script.SetTimer(1,function()
				local ent,mdl,pos,dir,scale=LevelDesigner:GetLast(sender);
				if not ent then Chat:SendToTarget(nil,sender,"Please, spawn atleast one new entity!"); return; end
				LevelDesigner:Spawn(mdl,pos,dir,false,false,true,nil,sender,true,scale);
			end);
		end,{},{PremiumOnly=true;},"Creates copy of entity on same place");
		AddChatCommand("gset",function(self,sender,msg,model)
			if model then
				sender.BGunMdl=model;
				Chat:SendToTarget(nil,sender,"BuildGun will now automatically use ,%s'",sender.BGunMdl);
			else
				Chat:SendToTarget(nil,sender,"Please enter valid model name!");
			end
		end,{TEXT},{PremiumOnly=true;},"Sets a model for building gun.")
		AddChatCommand("buildgun",function(self,sender,msg,model)
			Spawn:Entity(sender,1,"SOCOM",function(e)
				e.isBuildGun=true;
			end);
		end,{TEXT},{PremiumOnly=true;},"Spawns a building gun for you.");
		AddChatCommand("buildlist",function(self,sender,msg)
			local mdls={};
			for i,v in pairs(LevelDesigner.ShortenModels) do
				mdls[#mdls+1]=i;
			end
			table.sort(mdls);
			for j,w in ipairs(mdls) do
				local i,v=w,LevelDesigner.ShortenModels[w];
				if type(v)=="string" then
					Console:SendToTarget(sender," $3%s$8 - $5%s",i,v);
				else
					local msg="$6>>$3%s $8- $5%s";
					Console:SendToTarget(sender,msg,i,v.ISCOLLECTION and "$9collection of entities" or v[1]);
				end
			end
			Msg:SendToTarget(sender,__qt(sender.lang,R.OPEN_CONSOLE));
		end,{},{PremiumOnly=true;},"shows list of available objects to spawn");
	end
	LevelDesigner:PrepareModels();
	LevelDesigner:LoadPrefabs();
	LevelDesigner:LoadCollections()
else
	if SafeWriting and AddChatCommand and Chat and Console and Msg then
		printf("Version 1.0.0 was detected!")
	else
		System.Error("You are not running SSM SafeWriting!");
	end
end