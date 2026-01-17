--------------------------------------------------------------------------
--	Crytek Source File.
-- 	Copyright (C), Crytek Studios, 2001-2006.
--------------------------------------------------------------------------
--	$Id$
--	$DateTime$
--	Description: Power Struggle relevant utility functions
--  
--------------------------------------------------------------------------
--  History:
--  - 26/10/2006   10:05 : Created by M�rcio Martins
--
----------------------------------------------------------------------------------------------------


----------------------------------------------------------------------------------------------------
function replace_pre(old, new)
	if (not old) then
		return new;
	else
		return function(...)
			local res=new(...);
			old(...)
			return res;
		end
	end
end

----------------------------------------------------------------------------------------------------
function replace_post(old, new)
	if (not old) then
		return new;
	else
		return function(...)
			local res=old(...);
			return new(...) or res;
		end
	end
end


----------------------------------------------------------------------------------------------------
function MakeCapturable(entity)
	entity.Properties.teamName 						= "";
	entity.Properties.bCapturable					= 1;
	entity.Properties.captureRequirement	= 2;
	entity.Properties.captureTime					= 15;
	entity.Properties.captureAreaId				= 0;
	entity.Properties.nCaptureIndex				= 1;
	entity.Properties.statusSubMtlId			= 5;
	
	local cm=entity.NetSetup.ClientMethods;
	cm.ClStartCapture			= { RELIABLE_ORDERED, POST_ATTACH, INT8 };
	cm.ClCancelCapture		= { RELIABLE_ORDERED, POST_ATTACH, };
	cm.ClStepCapture			= { RELIABLE_ORDERED, POST_ATTACH, INT8,  FLOAT };
	cm.ClCapture					= { RELIABLE_ORDERED, POST_ATTACH, INT8 };
	cm.ClStartUncapture		= { RELIABLE_ORDERED, POST_ATTACH, INT8 };
	cm.ClCancelUncapture	= { RELIABLE_ORDERED, POST_ATTACH, };
	cm.ClStepUncapture		= { RELIABLE_ORDERED, POST_ATTACH, INT8,  FLOAT };
	cm.ClUncapture				= { RELIABLE_ORDERED, POST_ATTACH, INT8, INT8 };
	cm.ClContested				= { RELIABLE_ORDERED, POST_ATTACH, BOOL };
	cm.ClInit							= { RELIABLE_ORDERED, POST_ATTACH, BOOL, INT8 };


	-- OnSpawn
	entity.OnSpawn=replace_pre(entity.OnSpawn, function(self)
		CryAction.CreateGameObjectForEntity(self.id);
		CryAction.BindGameObjectToNetwork(self.id);
		if ( CryAction.IsServer() ) then
			CryAction.ForceGameObjectUpdate(self.id, true);
		end
	end);

	-- OnPostInitClient
	entity.Server.OnPostInitClient=replace_post(entity.Server.OnPostInitClient, function(self, channelId)
		if (self.captured) then
			self.onClient:ClInit(channelId, self.captured, self:GetTeamId());
		end
	end);

	-- OnInit
	entity.Server.OnInit=replace_post(entity.Server.OnInit, function(self)
		self.capturable=tonumber(self.Properties.bCapturable)~=0;
		self.inside={};
	end);
	
	-- OnReset
	entity.OnReset=replace_post(entity.OnReset, function(self)
		self.isServer = CryAction.IsServer();
		self.isClient = CryAction.IsClient();

		if (self.isServer) then
			local teamId=g_gameRules.game:GetTeamId(self.Properties.teamName);
			if (teamId and teamId~=0) then
				self:SetTeamId(teamId);
				self.captured=true;
			else
				self:SetTeamId(0);
				self.captured=false;
			end
		end	
	end);
	
	-- OnUpdate
	entity.Server.OnUpdate=replace_post(entity.Server.OnUpdate, function(self, frameTime)
		if (self.capturable) then
			self:UpdateCapture(frameTime);
		end
	end);

	-- OnEnterArea
	entity.Server.OnEnterArea=replace_post(entity.Server.OnEnterArea, function(self, entity, areaId)
		if (areaId == self.Properties.captureAreaId) then
			local inserted=false;
			for i,id in ipairs(self.inside) do
				if (id==entity.id) then
					inserted=true;
					break;
				end
			end
			if (not inserted) then
				table.insert(self.inside, entity.id);
				
				if (g_gameRules.Server.OnEnterCaptureArea) then
					g_gameRules.Server.OnEnterCaptureArea(g_gameRules, self, entity);
				end
			end
		end		
	end);
	
	-- OnLeaveArea
	entity.Server.OnLeaveArea=replace_post(entity.Server.OnLeaveArea, function(self, entity, areaId)
		if (areaId == self.Properties.captureAreaId) then
			for i,id in ipairs(self.inside) do
				if (id==entity.id) then
					table.remove(self.inside, i);

					if (g_gameRules.Server.OnLeaveCaptureArea) then
						g_gameRules.Server.OnLeaveCaptureArea(g_gameRules, self, entity);
					end

					break;
				end
			end
		end		
	end);
	
	
	entity.IsPlayerInside=replace_post(entity.IsPlayerInside, function(self, playerId)
		for i,id in ipairs(self.inside) do
			if (id==playerId) then
				return true;
			end
		end
		return false;
	end);
	
	-- GetCaptureIndex
	entity.GetCaptureIndex=replace_post(entity.GetCaptureIndex, function(self)
		return self.Properties.nCaptureIndex;
	end);
	
	-- GetTeamId
	entity.GetTeamId=replace_post(entity.GetTeamId, function(self)
		return g_gameRules.game:GetTeam(self.id);
	end);
	
	-- SetTeamId
	entity.SetTeamId=replace_post(entity.SetTeamId, function(self, teamId)
		g_gameRules.game:SetTeam(teamId, self.id);
	end);
	
	-- UpdateCapture
	entity.UpdateCapture=replace_post(entity.UpdateCapture, function(self, frameTime)
		if (g_gameRules:GetState() ~= "InGame") then
			return;
		end
	
		local players=self.inside;
		local teamId=0;
		local count=0;
		
		if (players) then
			for i,playerId in ipairs(players) do
				local player=System.GetEntity(playerId);
				if (player and (not player:IsDead()) and (player.actor:GetSpectatorMode()==0)) then
					local playerTeamId=g_gameRules.game:GetTeam(playerId);
					if (teamId~=0 and teamId~=playerTeamId) then
						self:SetContested(true);
						return;
					end
					teamId=playerTeamId;
					count=count+1;
				end
			end
		end
	
		self:SetContested(false);
	
		if ((self:GetTeamId()~=0) and (teamId==self:GetTeamId())) then
			if (self.uncapturing and self.uncapturingTeamId~=teamId) then
				self:CancelUncapture();
			end

			return;
		end
	
		local ok=(count>0) and (count>=tonumber(self.Properties.captureRequirement));
	
		if (self.captured) then
			if (ok) then
				if (self.uncapturing) then
					if (teamId==self.uncapturingTeamId) then
						self:StepUncapture(frameTime, count);
					else
						self:CancelUncapture();
						self:StartUncapture(teamId);
					end
				else
					self:StartUncapture(teamId);
				end
			elseif (self.uncapturing) then
				self:CancelUncapture();
			end
		elseif ((not self.capturing) and (not self.captured)) then
			if (ok) then
				self:StartCapture(teamId);
			elseif (self.uncapturing) then
				self:CancelUncapture();
			end
		elseif (self.capturing or self.uncapturing) then
			if (ok) then
				if (teamId==self.capturingTeamId) then
					if (self.uncapturing) then
						self:CancelUncapture();
					else
						self:StepCapture(frameTime, count);
					end
				else
					if (not self.uncapturing) then
						self:StartUncapture(teamId);
					elseif (teamId~=self.uncapturingTeamId) then
						self:CancelUncapture();
						self:StartUncapture(teamId);
					elseif (self.uncapturing) then
						self:StepUncapture(frameTime, count);
					end
				end
			else
				if (self.uncapturing) then
					self:CancelUncapture();
				elseif (self.capturing) then
					self:CancelCapture();
				end
			end
		end
	end);
	
	-- StartUncapture
	entity.StartUncapture=replace_post(entity.StartUncapture, function(self, teamId)
		self.uncapturing=true;
		self.uncapturingTeamId=teamId;
		
		if (self.captured) then
			self.capturingTimer=tonumber(self.Properties.captureTime);
		end
		
		self.lastUncapturingTimer=self.capturingTimer;
		
		if (g_gameRules.Server.OnStartUncapture) then
			g_gameRules.Server.OnStartUncapture(g_gameRules, self, teamId);
		end
		
		self.allClients:ClStartUncapture(teamId);
		
		Log("%s - Starting uncapture for team '%s'...", self:GetName(), g_gameRules.game:GetTeamName(teamId) or "");
	end);
	
	-- CancelUncapture
	entity.CancelUncapture=replace_post(entity.CancelUncapture, function(self)
		if (self.uncapturing) then
			Log("%s - Canceled uncapture for team '%s'...", self:GetName(), g_gameRules.game:GetTeamName(self.uncapturingTeamId) or "");
			
			self.uncapturing=false;
			self.uncapturingTeamId=nil;
			self.lastUncapturingTimer=nil;			
			
			if (g_gameRules.Server.OnCancelUncapture) then
				g_gameRules.Server.OnCancelUncapture(g_gameRules, self);
			end	
			
			self.allClients:ClCancelUncapture();
		end	
	end);
	
	-- StepUncapture
	entity.StepUncapture=replace_post(entity.StepUncapture, function(self, frameTime, count)
		local count=1+math.min(3, count-tonumber(self.Properties.captureRequirement))*0.33;
		self.capturingTimer=self.capturingTimer-frameTime*count;

		if (self.capturingTimer<=0) then
			self.capturingTimer=0.0;
			self:Uncapture(self.uncapturingTeamId);
		else
			if (g_gameRules.Server.OnStepUncapture) then
				g_gameRules.Server.OnStepUncapture(g_gameRules, self, self.uncapturingTeamId, self.capturingTimer);
			end

			if (math.abs(self.lastUncapturingTimer-self.capturingTimer)>=0.75) then
				self.allClients:ClStepUncapture(self.uncapturingTeamId, self.capturingTimer);
				self.lastUncapturingTimer=self.capturingTimer;
			end
		end
	end);

	-- Uncapture
	entity.Uncapture=replace_post(entity.Uncapture, function(self, teamId)
		local oldTeamId=self:GetTeamId() or 0;
		self.captured=false;
		self.uncapturing=false;
		self.uncapturingTeamId=nil;
		self.capturing=false;

		self:SetTeamId(0);

		if (self.UncaptureLinks) then
			self:UncaptureLinks("capture", teamId);
			self:UncaptureLinks("setteam", teamId);
		end			
			
		if (g_gameRules.Server.OnUncapture) then
			g_gameRules.Server.OnUncapture(g_gameRules, self, teamId or 0, oldTeamId or 0);
		end
			
		self.allClients:ClUncapture(teamId or 0, oldTeamId or 0);

		Log("%s - Uncaptured...", self:GetName());
	end);
	
	-- StartCapture
	entity.StartCapture=replace_post(entity.StartCapture, function(self, teamId)
		self.capturing=true;
		self.capturingTeamId=teamId;
		self.capturingTimer=tonumber(self.Properties.captureTime);
		self.lastCapturingTimer=self.capturingTimer;
		
		if (g_gameRules.Server.OnStartCapture) then
			g_gameRules.Server.OnStartCapture(g_gameRules, self, teamId);
		end
		
		self.allClients:ClStartCapture(teamId);
		
		Log("%s - Starting capture for team '%s'...", self:GetName(), g_gameRules.game:GetTeamName(teamId) or "");		
	end);
	
	-- CancelCapture
	entity.CancelCapture=replace_post(entity.CancelCapture, function(self)
		if (self.capturing) then
			Log("%s - Canceled capture for team '%s'...", self:GetName(), g_gameRules.game:GetTeamName(self.capturingTeamId) or "");
			
			self.capturing=false;
			self.capturingTeamId=nil;
			self.lastCapturingTimer=nil;
			
			if (g_gameRules.Server.OnStopCapture) then
				g_gameRules.Server.OnStopCapture(g_gameRules, self);
			end
			
			self.allClients:ClCancelCapture();
		end
	end);
	
	-- StepCapture
	entity.StepCapture=replace_post(entity.StepCapture, function(self, frameTime, count)
		local count=1+math.min(3, count-tonumber(self.Properties.captureRequirement))*0.33;
		self.capturingTimer=self.capturingTimer-frameTime*count;
		
		if (self.capturingTimer<=0) then
			self.capturingTimer=0;
			self:Capture(self.capturingTeamId);
		else
			if (g_gameRules.Server.OnStepCapture) then
				g_gameRules.Server.OnStepCapture(g_gameRules, self, self.capturingTeamId, self.capturingTimer);
			end
			
			if (math.abs(self.lastCapturingTimer-self.capturingTimer)>=0.75) then
				self.lastCapturingTimer=self.capturingTimer
				self.allClients:ClStepCapture(self.capturingTeamId, self.capturingTimer);
			end
		end	
	end);
	
	-- Capture
	entity.Capture=replace_post(entity.Capture, function(self, teamId)
		self:SetTeamId(teamId);
		self.captured=true;
		self.capturing=false;
		self.capturingTeamId=nil;
		self.capturingTimer=nil;
		self.lastCapturingTimer=nil;
	
		Log("%s - CAPTURED by team '%s'...", self:GetName(), g_gameRules.game:GetTeamName(teamId) or "");
		
		local players=self.inside;
		if (players) then
			local value=0;
			if (g_gameRules.captureValue) then
				local idx=self:GetCaptureIndex();
				value=g_gameRules.captureValue[idx] or 0;
			end
		
			if (g_gameRules.AwardPPCount and (value>0)) then
				for i,playerId in ipairs(players) do
					if (g_gameRules.game:GetTeam(playerId)==teamId) then
						local player=System.GetEntity(playerId);
						if (player and player.actor and (not player:IsDead()) and (player.actor:GetSpectatorMode() == 0)) then
							g_gameRules:AwardPPCount(playerId, value);
							g_gameRules:AwardCPCount(playerId, g_gameRules.cpList.CAPTURE);
						end
					end
				end
			end
		end
		
		if (self.CaptureLinks) then
			self:CaptureLinks("capture", teamId);
			self:CaptureLinks("setteam", teamId);
		end

		if (g_gameRules.Server.OnCapture) then
			g_gameRules.Server.OnCapture(g_gameRules, self, teamId);
		end

		self.allClients:ClCapture(teamId);
	end);
	
	
	entity.SetContested=replace_post(entity.SetContested, function(self, contested)
		if (contested == (self.contested or false)) then
			return;
		end
		
		if (self.uncapturing) then
			Log("%s - UNCAPTURE CONTESTED!", self:GetName());
		else
			Log("%s - CAPTURE CONTESTED!", self:GetName());
		end
		
		self.contested=contested;
		
		if (g_gameRules.Server.OnContested) then
			g_gameRules.Server.OnContested(g_gameRules, self, contested);
		end
			
		self.allClients:ClContested(contested);
	end);
	
	-- CaptureLinks
	entity.CaptureLinks=replace_post(entity.CaptureLinks, function(self, linkName, teamId)
		local i=0;
		local link=self:GetLinkTarget(linkName, i);
		while (link) do
			if (g_gameRules.game:GetTeam(link.id)~=teamId) then
				if (link.Capture) then
					link:Capture(teamId);
				else
					g_gameRules.game:SetTeam(teamId, link.id);
				end
			end
			i=i+1;
			link=self:GetLinkTarget(linkName, i);
		end
	end);
	
	-- UncaptureLinks
	entity.UncaptureLinks=replace_post(entity.UncaptureLinks, function(self, linkName, teamId)
		local i=0;
		local link=self:GetLinkTarget(linkName, i);
		while (link) do
			if (g_gameRules.game:GetTeam(link.id)~=0) then
				if (link.Uncapture) then
					link:Uncapture(teamId);
				else
					g_gameRules.game:SetTeam(0, link.id);
				end
			end
			i=i+1;
			link=self:GetLinkTarget(linkName, i);
		end
	end);

	-- Status
	entity.Status=replace_post(entity.Status, function(self, proc)
		local i=0;
		local link=self:GetLinkTarget("status", i);
		if (link) then
			while (link) do
				if (not link.matCloned) then	-- flash movies are shared between objects that share the same material,
																			-- so we need to create a material clone here
					link:CloneMaterial(0);
					link.matCloned=true;
				end

				proc(self, link);
				i=i+1;
				link=self:GetLinkTarget("status", i);
			end		
		else
			proc(self, self);
		end
	end);
	
	-- Flag
	entity.Flag=replace_post(entity.Flag, function(self, flag)
		local i=0;
		local link=self:GetLinkTarget("flag", i);
		if (link) then
			while (link) do
				if (link.SetTeam) then
					link:SetTeam(flag);
				end
				i=i+1;
				link=self:GetLinkTarget("flag", i);
			end		
		end
	end);
	
	-- ClInit
	entity.Client.ClInit=replace_post(entity.Client.ClInit, function(self, captured, teamId)
		if (captured) then
			self:Status(function(factory, status)
				status:MaterialFlashInvoke(0, factory.Properties.statusSubMtlId, 0, "setStartCapture", teamId, 100);
			end);
			self:Flag(g_gameRules.game:GetTeamName(teamId));
		else
			self:Status(function(factory, status)
				status:MaterialFlashInvoke(0, factory.Properties.statusSubMtlId, 0, "setStartCapture", 0, 0);
			end);
			self:Flag("");
		end
	end);
	
	-- ClStartCapture
	entity.Client.ClStartCapture=replace_post(entity.Client.ClStartCapture, function(self, teamId)
		if (g_gameRules.Client.OnStartCapture) then
			g_gameRules.Client.OnStartCapture(g_gameRules, self, teamId);
		end
		
		self:Status(function(factory, status)
			status:MaterialFlashInvoke(0, factory.Properties.statusSubMtlId, 0, "setStartCapture", teamId, 0);
		end);
	end);
	
	-- ClCancelCapture
	entity.Client.ClCancelCapture=replace_post(entity.Client.ClCancelCapture, function(self)
		if (g_gameRules.Client.OnCancelCapture) then
			g_gameRules.Client.OnCancelCapture(g_gameRules, self);
		end
			
		self:Status(function(factory, status)
			status:MaterialFlashInvoke(0, factory.Properties.statusSubMtlId, 0, "setStartCapture", 0);
		end);
	end);

	-- ClStepCapture
	entity.Client.ClStepCapture=replace_post(entity.Client.ClStepCapture, function(self, teamId, remainingTime)
		if (g_gameRules.Client.OnStepCapture) then
			g_gameRules.Client.OnStepCapture(g_gameRules, self, teamId, remainingTime);
		end
		
		self:Status(function(factory, status)
			local p=math.floor((1-(remainingTime/factory.Properties.captureTime))*100+0.5);
			status:MaterialFlashInvoke(0, factory.Properties.statusSubMtlId, 0, "setStartCapture", teamId, p);
		end);		
	end);	
	
	-- ClCapture
	entity.Client.ClCapture=replace_post(entity.Client.ClCapture, function(self, teamId)
		if (g_gameRules.Client.OnCapture) then
			g_gameRules.Client.OnCapture(g_gameRules, self, teamId);
		end
		
		self:Status(function(factory, status)
			status:MaterialFlashInvoke(0, factory.Properties.statusSubMtlId, 0, "setStartCapture", teamId, 100);
			status:MaterialFlashInvoke(0, factory.Properties.statusSubMtlId, 0, "setCapture", teamId);
		end);
		self:Flag(g_gameRules.game:GetTeamName(teamId));
	end);
	
	-- ClStartUncapture
	entity.Client.ClStartUncapture=replace_post(entity.Client.ClStartUncapture, function(self, teamId)
		if (g_gameRules.Client.OnStartUncapture) then	
			g_gameRules.Client.OnStartUncapture(g_gameRules, self, teamId);
		end
		
		self:Status(function(factory, status)
			status:MaterialFlashInvoke(0, factory.Properties.statusSubMtlId, 0, "setStartUnCapture", factory:GetTeamId(), 100);
		end);
	end);
	
	-- ClCancelUncapture
	entity.Client.ClCancelUncapture=replace_post(entity.Client.ClCancelUncapture, function(self)
		if (g_gameRules.Client.OnCancelUncapture) then
			g_gameRules.Client.OnCancelUncapture(g_gameRules, self);
		end
		
		self:Status(function(factory, status)
			status:MaterialFlashInvoke(0, factory.Properties.statusSubMtlId, 0, "setStartUnCapture", 0);
			status:MaterialFlashInvoke(0, factory.Properties.statusSubMtlId, 0, "setStartCapture", self:GetTeamId(), 100);
			status:MaterialFlashInvoke(0, factory.Properties.statusSubMtlId, 0, "setCapture", self:GetTeamId());			
		end);
	end);
	
	-- ClStepUncapture
	entity.Client.ClStepUncapture=replace_post(entity.Client.ClStepUncapture, function(self, teamId, remainingTime)
		if (g_gameRules.Client.OnStepUncapture) then
			g_gameRules.Client.OnStepUncapture(g_gameRules, self, teamId, remainingTime);
		end
		
		self:Status(function(factory, status)
			local p=math.floor((remainingTime/factory.Properties.captureTime)*100+0.5);
			status:MaterialFlashInvoke(0, factory.Properties.statusSubMtlId, 0, "setStartUnCapture", teamId, p);
		end);
	end);
	
	-- ClUncapture
	entity.Client.ClUncapture=replace_post(entity.Client.ClUncapture, function(self, teamId, oldTeamId)
		if (g_gameRules.Client.OnUncapture) then
			g_gameRules.Client.OnUncapture(g_gameRules, self, teamId, oldTeamId);
		end
		
		self:Status(function(factory, status)
			status:MaterialFlashInvoke(0, factory.Properties.statusSubMtlId, 0, "setStartUnCapture", teamId, 0);
			status:MaterialFlashInvoke(0, factory.Properties.statusSubMtlId, 0, "setUnCapture", 1, teamId);
		end);
		self:Flag("");
	end);
	
	-- ClUncapture
	entity.Client.ClContested=replace_post(entity.Client.ClContested, function(self, contested)
		if (g_gameRules.Client.OnContested) then
			g_gameRules.Client.OnContested(g_gameRules, self, contested);
		end
		
		--self:Status(function(factory, status)
			--status:MaterialFlashInvoke(0, factory.Properties.statusSubMtlId, 0, "setStartUnCapture", teamId, 0);
			--status:MaterialFlashInvoke(0, factory.Properties.statusSubMtlId, 0, "setUnCapture", 1, teamId);
		--end);
		--self:Flag("");
	end);
end


----------------------------------------------------------------------------------------------------
function MakeBuyZone(entity, defaultBuyFlags)
	local hasFlag=function(option, flag)
		if (band(option, flag)~=0) then
			return 1;
		else
			return 0;
		end
	end

	if (entity.class) then -- has this entity spawned already?
		local buyFlags=0;
		local options=entity.Properties.buyOptions;
		if (tonumber(options.bVehicles)~=0) then	 	buyFlags=bor(buyFlags, DefuseAction.BUY_VEHICLE); end;
		if (tonumber(options.bWeapons)~=0) then 		buyFlags=bor(buyFlags, DefuseAction.BUY_WEAPON); end;
		if (tonumber(options.bEquipment)~=0) then		buyFlags=bor(buyFlags, DefuseAction.BUY_EQUIPMENT); end;
		if (tonumber(options.bPrototypes)~=0) then 	buyFlags=bor(buyFlags, DefuseAction.BUY_PROTOTYPE); end;
		if (tonumber(options.bAmmo)~=0) then		 		buyFlags=bor(buyFlags, DefuseAction.BUY_AMMO); end;
		entity.buyFlags=buyFlags;
	else
		entity.Properties.buyAreaId	= 0;
		entity.Properties.buyOptions={
			bVehicles 	= hasFlag(defaultBuyFlags, DefuseAction.BUY_VEHICLE),
			bWeapons 		= hasFlag(defaultBuyFlags, DefuseAction.BUY_WEAPON),
			bEquipment	= hasFlag(defaultBuyFlags, DefuseAction.BUY_EQUIPMENT),
			bPrototypes	= hasFlag(defaultBuyFlags, DefuseAction.BUY_PROTOTYPE),
			bAmmo				= hasFlag(defaultBuyFlags, DefuseAction.BUY_AMMO),
		};
	
		-- OnSpawn
		entity.OnSpawn=replace_post(entity.OnSpawn, function(self)
			local buyFlags=0;
			local options=self.Properties.buyOptions;
			if (tonumber(options.bVehicles)~=0) then	 	buyFlags=bor(buyFlags, DefuseAction.BUY_VEHICLE); end;
			if (tonumber(options.bWeapons)~=0) then 		buyFlags=bor(buyFlags, DefuseAction.BUY_WEAPON); end;
			if (tonumber(options.bEquipment)~=0) then		buyFlags=bor(buyFlags, DefuseAction.BUY_EQUIPMENT); end;
			if (tonumber(options.bPrototypes)~=0) then 	buyFlags=bor(buyFlags, DefuseAction.BUY_PROTOTYPE); end;
			if (tonumber(options.bAmmo)~=0) then		 		buyFlags=bor(buyFlags, DefuseAction.BUY_AMMO); end;
			self.buyFlags=buyFlags;
		end);
	end
	
	-- GetBuyFlags
	entity.GetBuyFlags=replace_post(entity.GetBuyFlags, function(self)
		return self.buyFlags;
	end);

	if (entity.class) then
		-- OnEnterArea
		entity.OnEnterArea=replace_post(entity.OnEnterArea, function(self, entity, areaId)
			if (areaId == self.Properties.buyAreaId) then
				if (g_gameRules.OnEnterBuyZone) then
					g_gameRules.OnEnterBuyZone(g_gameRules, self, entity);
				end
			end		
		end);
	
		-- OnLeaveArea
		entity.OnLeaveArea=replace_post(entity.OnLeaveArea, function(self, entity, areaId)
			if (areaId == self.Properties.buyAreaId) then
				if (g_gameRules.OnLeaveBuyZone) then
					g_gameRules.OnLeaveBuyZone(g_gameRules, self, entity);
				end		
			end
		end);	
	else
		-- OnEnterArea
		entity.Server.OnEnterArea=replace_post(entity.Server.OnEnterArea, function(self, entity, areaId)
			if (areaId == self.Properties.buyAreaId) then
				if (g_gameRules.OnEnterBuyZone) then
					g_gameRules.OnEnterBuyZone(g_gameRules, self, entity);
				end
			end
		end);
	
		-- OnLeaveArea
		entity.Server.OnLeaveArea=replace_post(entity.Server.OnLeaveArea, function(self, entity, areaId)
			if (areaId == self.Properties.buyAreaId) then
				if (g_gameRules.OnLeaveBuyZone) then
					g_gameRules.OnLeaveBuyZone(g_gameRules, self, entity);
				end		
			end
		end);	
	end
end