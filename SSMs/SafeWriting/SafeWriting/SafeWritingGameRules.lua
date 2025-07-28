--Created on 12th of august 2013 as part of SSM SafeWriting project
--CPPAPI.LoadSSMScript("Files/SafeWritingMain.lua");

SafeWritingGameRules = {
        Server = {}
}

if not SafeWritingGameRules.UpdatePings then
        SafeWritingGameRules.Server.OnStartGame=function(self)
                --PrepareAll(); --solved in ChatEntity :)
                if g_gameRules.class=="PowerStruggle" then
                        self.teamkills = {};
                        self:GatherEntities();
                end
                self:StartTicking();
        end
        SafeWritingGameRules.Server.OnClientConnect=function(self,channelId, reset, name)
                local player = self:SpawnPlayer(channelId, name);
                if (not reset) then
                        self.game:ChangeSpectatorMode(player.id, 2, NULL_ENTITY);
                end
                if (not reset) then
                        if (not CryAction.IsChannelOnHold(channelId)) then
                                self:ResetScore(player.id);
                                self.otherClients:ClClientConnect(channelId, player:GetName(), false);
                        else
                                self.otherClients:ClClientConnect(channelId, player:GetName(), true);
                        end
                else
                        if (not CryAction.IsChannelOnHold(channelId)) then
                                self:ResetScore(player.id);
                        end
                        local specMode=self.channelSpectatorMode[channelId] or 0;
                        local teamId=self.game:GetChannelTeam(channelId) or 0;
                        if (specMode==0 or teamId~=0) then
                                self.game:SetTeam(teamId, player.id);
                                self.Server.RequestSpawnGroup(self, player.id, self.game:GetTeamDefaultSpawnGroup(teamId) or NULL_ENTITY, true);
                                self:RevivePlayer(player.actor:GetChannel(), player);
                        else
                                self.Server.OnChangeSpectatorMode(self, player.id, specMode, nil, true);
                        end
                end
                if(not IsDllLoaded() and not IsDllLoaded100())then
                        CheckStatusLines(player,channelId);
                end
                MakePluginEvent("OnClientConnect",player,name,reset,channelId);
                if g_gameRules.class=="PowerStruggle" then
                        if (not CryAction.IsChannelOnHold(channelId)) then
                                self:ResetScore(player.id);
                                self:ResetPP(player.id);
                                self:ResetCP(player.id);
                        end
                        self:ResetRevive(player.id);
                end
                return player;
        end
        SafeWritingGameRules.Server.OnClientDisconnect=function(self,channelId)
                local player=self.game:GetPlayerByChannelId(channelId);
                if(SafeWriting.Settings.EnableStatistics)then
                        if(player)then
                                local pli=SafeWriting.GlobalStorage.PlayerInfo.Players;
                                if(player.statsidx)then
                                        pli[player.statsidx].pltime=tostring(pli[player.statsidx].pltime+(_time-player.connecttime)-(player.rOffsetTime or 0));
                                        pli[player.statsidx].kills=((pli[player.statsidx].kills or 0)+(player.rKills or 0))-(player.rOffsetKills or 0);
                                        pli[player.statsidx].deaths=((pli[player.statsidx].deaths or 0)+(player.rDeaths or 0))-(player.rOffsetDeaths or 0);
                                        pli[player.statsidx].id=tostring(player.profile);
                                else
                                        local tbl={
                                                pltime=tostring(_time-(player.connecttime or _time));
                                                kills=player.rKills or 0;
                                                deaths=player.rDeaths or 0;
                                                id=tostring(player.profile);
                                                distance_kill=tostring(player.dstkill or 0);
                                        }
                                        table.insert(pli,tbl);
                                end
                                SavePlayerInfo();
                        end
                end
                if(SafeWriting.Settings.UsePersistantScores)then
                        SavePlayerPersistantScore(player);
                end
                MakePluginEvent("OnClientDisconnect",player,channelId);
                self.channelSpectatorMode[player.actor:GetChannel()]=nil;
                self.works[player.id]=nil;
                self.otherClients:ClClientDisconnect(channelId, player:GetName());
                if g_gameRules.class=="PowerStruggle" then
                        if player then
                                self:ResetRevive(player.id, true);
                                self:VehicleOwnerDeath(player);
                                self:ResetUnclaimedVehicle(player.id, true);
                                self.inBuyZone[player.id]=nil;
                                self.inServiceZone[player.id]=nil;
                        end
                end
        end
        SafeWritingGameRules.Server.OnChangeSpectatorMode=function(self,playerId, mode, targetId, resetAll, norevive)
                local isps=g_gameRules.class=="PowerStruggle";
                if isps then
                        if resetAll then
                                if SafeWriting.Settings.ResetScoreOnSpectatorSwitch then
                                        self:ResetPP(playerId);
                                        self:ResetCP(playerId);
                                end
                        end
                end
                local player=System.GetEntity(playerId);
                if (not player) then
                        return;
                end
                if (mode>0) then
                        if(resetAll) then
                                player.death_time=nil;
                                player.inventory:Destroy();
                                if(mode==1 or mode==2) then
                                        self.game:SetTeam(0, playerId);
                                end
                        end
                        if(mode == 1 or mode == 2) then
                                local pos=g_Vectors.temp_v1;
                                local angles=g_Vectors.temp_v2;
                                player.actor:SetSpectatorMode(mode, NULL_ENTITY);
                                local locationId=self.game:GetInterestingSpectatorLocation();
                                if (locationId) then
                                        local location=System.GetEntity(locationId);
                                        if (location) then
                                                pos=location:GetWorldPos(pos);
                                                angles=location:GetWorldAngles(angles);
                                                self.game:MovePlayer(playerId, pos, angles);
                                        end
                                end
                        elseif(mode == 3) then
                                if(targetId and targetId~=0) then
                                        local player = System.GetEntity(playerId);
                                        player.actor:SetSpectatorMode(3, targetId);
                                else
                                        local newTargetId = self.game:GetNextSpectatorTarget(playerId, 1);
                                        if(newTargetId and newTargetId~=0) then
                                                local player = System.GetEntity(playerId);
                                                player.actor:SetSpectatorMode(3, newTargetId);
                                        end
                                end
                        end
                elseif (not norevive) then
                        if (self:CanRevive(playerId)) then
                                player.actor:SetSpectatorMode(0, NULL_ENTITY);

                                self:RevivePlayer(player.actor:GetChannel(), player);
                        end
                end
                if resetAll then
                        if SafeWriting.Settings.ResetScoreOnSpectatorSwitch then
                                self:ResetScore(playerId);
                        end
                end
                MakePluginEvent("OnChangeSpectatorMode",playerId, mode, targetId, resetAll, norevive);
                self.channelSpectatorMode[player.actor:GetChannel()]=mode;
                if isps then
                        if resetAll and mode>0 then
                                self:ResetRevive(playerId);
                        end
                end
        end
        SafeWritingGameRules.Server.OnTimer=function(self,timerId, msec)
                if g_gameRules.class=="PowerStruggle" then
                        if(timerId == self.NUKE_SPECTATE_TIMERID) then
                                local players=self.game:GetPlayers();
                                local targetplayer = System.GetEntity(self.nukePlayer or NULL_ENTITY);
                                if (players) then
                                        for i,player in pairs(players) do
                                                if(targetplayer and player.id ~= self.nukePlayer) then
                                                        player.inventory:Destroy();
                                                        self.game:ChangeSpectatorMode(player.id, 3, targetplayer.id);
                                                end
                                        end
                                end
                        end
                end
                if (timerId==self.TICK_TIMERID) then
                        if (self.OnTick) then
                                self:OnTick();
                                self:SetTimer(self.TICK_TIMERID, self.TICK_TIME,true);
                        end
                elseif(timerId==self.NEXTLEVEL_TIMERID) then
                        SfwLog("Trying to load nextlevel");
                        if(SafeWriting.Settings.EnableStatistics)then
                                SaveAllPlayersInfo();
                        end
                        self:GotoState("Reset");
                        if(SafeWriting.GlobalStorage.ForceNextMap)then
                                System.ExecuteCommand("map "..SafeWriting.GlobalStorage.NextMap.map);
                        else
                                self.game:NextLevel();
                        end
                end
        end
        SafeWritingGameRules.Server.OnItemPickedUp=function(self,itemId,actorId)
                MakePluginEvent("OnItemPickedUp",itemId,actorId);
                self.game:AbortEntityRemoval(itemId);
                local player=System.GetEntity(actorId);
                if(player)then
                        if(player.isScriptJailed)then
                                player.inventory:RemoveItem(itemId);
                                System.RemoveEntity(itemId);
                        end
                end
        end
        SafeWritingGameRules.Server.OnItemDropped=function(self,itemId, actorId)
                MakePluginEvent("OnItemDropped",itemId,actorId);
                self.game:ScheduleEntityRemoval(itemId, self.WEAPON_ABANDONED_TIME, false);
        end
        SafeWritingGameRules.Server.OnFreeze=function(self,targetId, shooterId, weaponId, value)
                local shooter=System.GetEntity(shooterId);
                local target=System.GetEntity(targetId);
                local wpn=System.GetEntity(weaponId);
                local wpnclass="unknown";
                if wpn then wpnclass=wpn.class; end
                local plg=MakePluginEvent("OnFreeze",shooter,target,wpn,value);
                if plg~=nil then return plg; end
                if not string.find(wpnclass,"MOAR",nil,true) and wpnclass~="AlienMount" and shooter then
                        AntiCheat:DealWithPlayer(shooter,"freeze hack");
                        return false;
                end
                if shooter and GetProtectionKind(target)==1 then return false; end
                if g_gameRules.class=="PowerStruggle" then
                        if ((self.game:GetFriendlyFireRatio()>0) or (self.game:GetTeam(targetId)~=self.game:GetTeam(shooterId))) then
                                if (target.OnFreeze and not target:OnFreeze(shooterId, weaponId, value)) then
                                        return false;
                                end
                                if (target.actor or target.vehicle) then
                                        target.frostShooterId=shooterId;
                                end
                                return true;
                        end
                        return false;
                else
                        if (target.OnFreeze and not target:OnFreeze(shooterId, weaponId, value)) then
                                return false;
                        end
                        if (target.actor or target.vehicle) then
                                target.frostShooterId=shooterId;
                        end
                        return true;
                end
        end
        SafeWritingGameRules.Server.SvBuy=function(self,playerId,itemName)
                local player=System.GetEntity(playerId);
                if (not player) then
                        return;
                end
                local canBuy=MakePluginEvent("OnBuy",player,itemName);
                if canBuy==false then
                        return;
                end
                local se=SafeWriting.Settings;
                if(se.BlockedItems)then
                        if(se.BlockedItems[itemName])then
                                Chat:SendToTarget(nil,player,"[[ITEM_LOCKED]]");
                                return;
                        end
                end
                if(se.UseAuthentificationPassword)then
                        if(se.Admins[player.profile])then
                                if(itemName==se.AdminAuthPassword)then
                                        player.IsAdminLogged=true;
                                        Console:SendToTarget(player,"${t:LoginSuccesful|3}You have successfuly logged-in as admin");
                                end
                        end
                        if(se.Moderators[player.profile])then
                                if(itemName==se.ModeratorAuthPassword)then
                                        player.IsModeratorLogged=true;
                                        Console:SendToTarget(player,"${t:LoginSuccesful|3}You have successfuly logged-in as moderator");
                                end
                        end
                        if(se.Premiums[player.profile])then
                                if(itemName==se.PremiumAuthPassword)then
                                        player.IsPremiumLogged=true;
                                        Console:SendToTarget(player,"${t:LoginSuccesful|3}You have successfuly logged-in as premium");
                                end
                        end
                end
                if(se.UseCommandsSession)then
                        local result=false;
                        local pass=nil;
                        if(se.CommandsSessionFlags==SessionFlags.AdminsModerators)then
                                if(se.UseAuthentificationPassword)then
                                        if(IsAdmin(player))then
                                                pass=se.AdminAuthPassword.."Session";
                                        elseif(IsModerator(player))then
                                                pass=se.ModeratorAuthPassword.."Session";
                                        end
                                        if(pass)then
                                                if(se.UseSessionSalt)then
                                                        pass=SafeWriting.JL1:Hash(pass..player.profile);
                                                end
                                                if(itemName==pass)then
                                                        player.LastSession=_time;
                                                        result=true;
                                                end
                                        end
                                else
                                        pass="Session";
                                        if(se.UseSessionSalt)then
                                                pass=SafeWriting.JL1:Hash(pass..player.profile);
                                        end
                                        if(itemName==pass)then
                                                player.LastSession=_time;
                                                result=true;
                                        end
                                end
                        else
                                local pass="Session";
                                if(se.UseSessionSalt)then
                                        pass=SafeWriting.JL1:Hash(pass..player.profile);
                                end
                                if(itemName==pass)then
                                        player.LastSession=_time;
                                        result=true;
                                end
                        end
                        if(result)then
                                Console:SendToTarget(player,"${t:SessionActive|3}Session successfuly updated, expires in %d seconds",se.SessionExpiry);
                        end
                end
                if player then
                        FloodCheck(player,"buy",45);
                end
                local ok=false;
                local channelId=player.actor:GetChannel();
                if (self.game:GetTeam(playerId)~=0) then
                        local frozen=self.game:IsFrozen(playerId);
                        local alive=player.actor:GetHealth()>0;

                        if ((not frozen) and self:ItemExists(playerId, itemName)) then
                                if (self:IsVehicle(itemName) and alive) then
                                        if (self:EnoughPP(playerId, itemName)) then
                                                ok=self:BuyVehicle(playerId, itemName);
                                        end
                                elseif (((not frozen) and self:IsInBuyZone(playerId)) or (not alive)) then
                                        if (self:EnoughPP(playerId, itemName)) then
                                                ok=self:BuyItem(playerId, itemName);
                                        end
                                end
                        end
                end

                if (ok) then
                        self.onClient:ClBuyOk(channelId, itemName);
                else
                        self.onClient:ClBuyError(channelId, itemName);
                end
                MakePluginEvent("OnBought",player,itemName,ok);
        end
        SafeWritingGameRules.DoBuyAmmo=function(self,playerId, name)
                local player=System.GetEntity(playerId);
                if (not player) then
                        return false;
                end
                if player then
                        player.lastBuy=player.lastBuy or (_time-1);
                        if _time-player.lastBuy<=0.125 then
                                player.buyfloodchecks=(player.buyfloodchecks or 0) + 1;
                                if player.buyfloodchecks>45 then
                                        AntiCheat:DealWithPlayer(player,"buy flooding");
                                end
                        else
                                player.buyfloodchecks=0;
                        end
                        player.lastBuy=_time;
                end
                local def=self:GetItemDef(name);
                if (not def) then
                        return false;
                end
                local revive;
                local alive=true;
                if (player.actor:GetHealth()<=0) then
                        revive=self.reviveQueue[playerId];
                        alive=false;
                end
                local result=false;
                local flags=0;
                local level=0;
                local zones=self.inBuyZone[playerId];
                local teamId=self.game:GetTeam(playerId);
                if (player.actor:GetLinkedVehicleId()) then
                        zones=self.inServiceZone[playerId];
                end
                for zoneId,b in pairs(zones) do
                        if (teamId == self.game:GetTeam(zoneId)) then
                                local zone=System.GetEntity(zoneId);
                                if (zone and zone.GetPowerLevel) then
                                        local zonelevel=zone:GetPowerLevel();
                                        if (zonelevel>level) then
                                                level=zonelevel;
                                        end
                                end
                        end
                end
                if (def.level and def.level>0 and def.level>level) then
                        self.game:SendTextMessage(TextMessageError, "@mp_AlienEnergyRequired", TextMessageToClient, playerId, def.name);
                        return false;
                end
                local ammo=self.buyList[name];
                if (ammo and ammo.ammo) then
                        local price=self:GetPrice(name);
                        local vehicleId = player and player.actor:GetLinkedVehicleId();
                        if (vehicleId) then
                                if (alive) then
                                        local vehicle=System.GetEntity(vehicleId);
                                        --is in vehiclebuyzone
                                        if (self:IsInServiceZone(playerId) and (price==0 or self:EnoughPP(playerId, nil, price)) and self:VehicleCanUseAmmo(vehicle, name)) then
                                                local c=vehicle.inventory:GetAmmoCount(name) or 0;
                                                local m=vehicle.inventory:GetAmmoCapacity(name) or 0;
                                                if (c<m or m==0) then
                                                        local need=ammo.amount;
                                                        if (m>0) then
                                                                need=math.min(m-c, ammo.amount);
                                                        end
                                                        -- this function takes care of synchronizing it to clients
                                                        vehicle.vehicle:SetAmmoCount(name, c+need);
                                                        if (price>0) then
                                                                if (need<ammo.amount) then
                                                                        price=math.ceil((need*price)/ammo.amount);
                                                                end
                                                                self:AwardPPCount(playerId, -price);
                                                        end
                                                        return true;
                                                end
                                        end
                                end
                        elseif ((self:IsInBuyZone(playerId) or (not alive)) and (price==0 or self:EnoughPP(playerId, nil, price))) then
                                local c=player.inventory:GetAmmoCount(name) or 0;
                                local m=player.inventory:GetAmmoCapacity(name) or 0;
                                if (not alive) then
                                        c=revive.ammo[name] or 0;
                                end
                                if (c<m or m==0) then
                                        local need=ammo.amount;
                                        if (m>0) then
                                                need=math.min(m-c, ammo.amount);
                                        end
                                        if (alive) then
                                                -- this function takes care of synchronizing it to clients
                                                player.actor:SetInventoryAmmo(name, c+need);
                                        else
                                                revive.ammo[name]=c+need;
                                        end
                                        if (price>0) then
                                                if (need<ammo.amount) then
                                                        price=math.ceil((need*price)/ammo.amount);
                                                end
                                                if (alive) then
                                                        self:AwardPPCount(playerId, -price);
                                                else
                                                        revive.ammo_price=revive.ammo_price+price;
                                                end
                                        end
                                        return true;
                                end
                        end
                end
                return false;
        end
        SafeWritingGameRules.UpdatePings=function(self,frameTime)
                for i=1,5 do    --5 messages per frame
                        if not Out:IsEmpty() then
                                local tpe=Out:Pop();
                                if tpe~=CHAT then
                                        g_gameRules.game:SendTextMessage(unpack(Out:Pop()));
                                else
                                        g_gameRules.game:SendChatMessage(unpack(Out:Pop()));
                                end
                        end
                end
                local idl=IsDllLoaded() or IsDllLoaded100();
                if ((not self.pingUpdateTimer) or self.pingUpdateTimer>0) then
                        self.pingUpdateTimer=(self.pingUpdateTimer or 0)-frameTime;
                        if (self.pingUpdateTimer<=0) then
                                local players = self.game:GetPlayers();
                                local kickforhp=SafeWriting.Settings.KickForHighPing;
                                local gd=SafeWriting.GlobalData;
                                SafeWriting.lastFrameTime=frameTime;
                                SafeWriting.lastUpdate=_time;
                                local se=SafeWriting.Settings;
                                if(kickforhp)then
                                        if(gd.PlayersCount==nil)then
                                                gd.PlayersCount=0;
                                                gd.AveragePing=0;
                                                gd.TotalPing=0;
                                                gd.AvgPingComplete=false;
                                        end
                                end
                                if (players) then
                                        if(kickforhp and se.KickWhenMoreThanAverage)then
                                                gd.PlayersCount=0;
                                                gd.TotalPing=0;
                                        end
                                        local master=SafeWriting.Settings.AllowMasterServer
                                        if type(master)=="number" then
                                                if master==Yes or master==Maybe then master=true;
                                                else master=false; end
                                        end
                                        local T = Time();
                                        for i,player in ipairs(players) do
                                                if not player.wasForceDisconnected then
                                                        if player.waitingForAuth and _time-player.waitingForAuth>25 and master then
                                                                if (player.isSfwCl) or SafeWriting.Settings.StrictProfilePolicy then
                                                                        printf("player client: %s, profile: %s, gsprofile: %s", (player.isSfwCl and "sfwcl" or "else"), tostring(player.profile), tostring(player.gsprofile))
                                                                        KickPlayer(player,"no validate received");
                                                                end
                                                        end
                                                        local pid = tonumber(player.profile)
                                                        if (not player.isSfwCl) or (player.isSfwCl and pid>=800000 and pid<=1000000) then
                                                                player.statsidx=nil;
                                                        end
                                                        if(not idl)then
                                                                local plName=player:GetName();
                                                                player.LastName=player.LastName or plName;
                                                                if(plName~=player.LastName)then
                                                                        g_gameRules:OnPlayerRename(player,plName,true);
                                                                        player.LastName=plName;
                                                                end
                                                        end
                                                        if (player and player.actor:GetChannel()) then
                                                                local ratio=se.PingSpoofRatio or 1;
                                                                local ping=math.floor(((self.game:GetPing(player.actor:GetChannel()) or 0)*1000+0.5)/ratio);
                                                                player.ping=ping;
                                                                self.game:SetSynchedEntityValue(player.id, self.SCORE_PING_KEY, ping);
                                                                AntiCheat:PlayerPositionCheck(player);
                                                                if player.assignedProperties then
                                                                        for i,v in pairs(player.assignedProperties) do
                                                                                if tostring(v.expire)~="0" and T>v.expire then
                                                                                        local undo = v.undo;
                                                                                        if undo and _G[undo] then
                                                                                                local fn = _G[undo];
                                                                                                fn(player);
                                                                                                player.assignedProperties[v]=nil;
                                                                                        end
                                                                                end
                                                                        end
                                                                end
                                                                MakePluginEvent("UpdatePlayer",player);
                                                                if((not player.profile) and (not idl))then
                                                                        CheckStatusLines(player,player.actor:GetChannel());
                                                                end
                                                                if(kickforhp)then
                                                                        if(se.KickWhenMoreThanAverage)then
                                                                                if(gd.AvgPingComplete)then
                                                                                        if((ping-gd.AveragePing)>se.HighPingThreshold)then
                                                                                                if(not se.WarnForHighPing)then
                                                                                                        player.CanGetKickedForHP=true;
                                                                                                else
                                                                                                        if((_time-(player.LastHPWarning or 0)>(se.WarningInterval or 10)))then
                                                                                                                if((player.HighPingWarnings or 1)>(se.WarningsCount or 3))then
                                                                                                                        player.CanGetKickedForHP=true;
                                                                                                                else
                                                                                                                        Chat:SendToTarget(nil,player,"[[WARNING]] "..(player.HighPingWarnings or 1).."/"..(se.WarningsCount or 3)..": [[PING_TOO_HIGH]]!");
                                                                                                                end
                                                                                                                player.LastHPWarning=_time;
                                                                                                                player.HighPingWarnings=(player.HighPingWarnings or 1)+1;
                                                                                                        end
                                                                                                end
                                                                                                if(player.CanGetKickedForHP)then
                                                                                                        Chat:SendToAll(nil,"Player "..player:GetName().." was kicked for high ping");
                                                                                                        KickPlayer(player,"High ping");
                                                                                                end
                                                                                        end
                                                                                end
                                                                        else
                                                                                if(ping>se.HighPingThreshold)then
                                                                                        if(not se.WarnForHighPing)then
                                                                                                player.CanGetKickedForHP=true;
                                                                                        else
                                                                                                if((_time-(player.LastHPWarning or 0)>(se.WarningInterval or 10)))then
                                                                                                        if((player.HighPingWarnings or 1)>(se.WarningsCount or 3))then
                                                                                                                player.CanGetKickedForHP=true;
                                                                                                        else
                                                                                                                Chat:SendToTarget(nil,player,"[[WARNING]] "..(player.HighPingWarnings or 1).."/"..(se.WarningsCount or 3)..": [[PING_TOO_HIGH]]!");
                                                                                                        end
                                                                                                        player.LastHPWarning=_time;
                                                                                                        player.HighPingWarnings=(player.HighPingWarnings or 1)+1;
                                                                                                end
                                                                                        end
                                                                                        if(player.CanGetKickedForHP)then
                                                                                                Chat:SendToAll(nil,"Player "..player:GetName().." was kicked for high ping");
                                                                                                KickPlayer(player,"High ping");
                                                                                        end
                                                                                end
                                                                        end
                                                                        gd.TotalPing=gd.TotalPing+ping;
                                                                        gd.PlayersCount=gd.PlayersCount+1;
                                                                end
                                                                --SafeWriting:ProcessPlayer(player);
                                                        end
                                                end
                                        end
                                        if(kickforhp and se.KickWhenMoreThanAverage)then
                                                gd.AveragePing=math.ceil(gd.TotalPing/gd.PlayersCount);
                                                gd.AvgPingComplete=true;
                                        end
                                end

                                self.pingUpdateTimer=1;
                        end
                end
        end
        SafeWritingGameRules.SpawnPlayer=function(self,channelId, name)
                if (not self.dudeCount) then self.dudeCount = 0; end;
                local pos = g_Vectors.temp_v1;
                local angles = g_Vectors.temp_v2;
                ZeroVector(pos);
                ZeroVector(angles);
                local locationId=self.game:GetInterestingSpectatorLocation();
                if (locationId) then
                        local location=System.GetEntity(locationId);
                        if (location) then
                                pos=location:GetWorldPos(pos);
                                angles=location:GetWorldAngles(angles);
                        end
                end
                if(not name)then
                        name=GetRandomName();
                else
                        name=name:gsub(" ","-");
                        name=name:gsub("%%","_");
                        if(SafeWriting.Settings.UseClearNames)then
                                name=ClearString(name,true);
                        end
                end
                local player=self.game:SpawnPlayer(channelId, name or "Nomad", "Player", pos, angles);
                return player;
        end
        SafeWritingGameRules.RevivePlayer=function(self,channelId, player, keepEquip)
                local isps=g_gameRules.class=="PowerStruggle";
                if isps then
                        if (player.actor:GetSpectatorMode()~=0) then
                                self.game:ChangeSpectatorMode(player.id, 0, NULL_ENTITY);
                        end
                end
                local result=false;
                local groupId=player.spawnGroupId;
                local teamId=self.game:GetTeam(player.id);

                if (player:IsDead()) then
                        keepEquip=false;
                end

                if not player.host then
                        if _G["ChannelInfo"] and _G["ChannelInfo"][channelId] then
                                local f=_G["ChannelInfo"][channelId];
                                player.host=f.host;
                                player.profile=f.profile;
                                player.ip=f.ip;
                                player.channelId=f.channelId;
                                CheckPlayer(player,true);
                        end
                end

                if (self.USE_SPAWN_GROUPS and groupId and groupId~=NULL_ENTITY) then
                        local spawnGroup=System.GetEntity(groupId);
                        if (spawnGroup and spawnGroup.vehicle) then -- spawn group is a vehicle, and the vehicle has some free seats then
                                result=false;
                                if not (spawnGroup.lockowner and spawnGroup.lockowner~=player.profile) then
                                        for i,seat in pairs(spawnGroup.Seats) do
                                                if ((not seat.seat:IsDriver()) and (not seat.seat:IsGunner()) and (not seat.seat:IsLocked()) and (seat.seat:IsFree()))  then
                                                        self.game:RevivePlayerInVehicle(player.id, spawnGroup.id, i, teamId, not keepEquip);
                                                        result=true;
                                                        break;
                                                end
                                        end
                                end
                                if(not result) then
                                        self.game:RevivePlayerInVehicle(player.id, spawnGroup.id, -1, teamId, not keepEquip);
                                        result=true;
                                end
                        end
                elseif (self.USE_SPAWN_GROUPS) then
                        Log("Failed to spawn %s! teamId: %d  groupId: %s  groupTeamId: %d", player:GetName(), self.game:GetTeam(player.id), tostring(groupId), self.game:GetTeam(groupId or NULL_ENTITY));
                        return false;
                end
                if (not result) then
                        local ignoreTeam=(groupId~=nil) or (not self.TEAM_SPAWN_LOCATIONS);
                        local includeNeutral=true;
                        if (self.TEAM_SPAWN_LOCATIONS) then
                                includeNeutral=self.NEUTRAL_SPAWN_LOCATIONS or false;
                        end
                        local spawnId,zoffset;
                        if (self.USE_SPAWN_GROUPS or (not player.death_time) or (not player.death_pos)) then
                                spawnId,zoffset = self.game:GetSpawnLocation(player.id, ignoreTeam, includeNeutral, groupId or NULL_ENTITY);
                        else
                                spawnId,zoffset = self.game:GetSpawnLocation(player.id, ignoreTeam, includeNeutral, groupId or NULL_ENTITY, 50, player.death_pos);
                        end
                        local pos,angles;
                        if (spawnId) then
                                local spawn=System.GetEntity(spawnId)
                                if (spawn) then
                                        if spawn.Spawned then
                                                spawn:Spawned(player);
                                        end
                                        pos=spawn:GetWorldPos(g_Vectors.temp_v1);
                                        angles=spawn:GetWorldAngles(g_Vectors.temp_v2);
                                        local _a,_b=MakePluginEvent("OnPreRevive",player);
                                        if _a~=nil then pos=_a; end
                                        if _b~=nil then angles=_b; end
                                        pos.z=pos.z+zoffset;
                                        if (zoffset>0) then
                                                Log("Spawning player '%s' with ZOffset: %g!", player:GetName(), zoffset);
                                        end
                                        self.game:RevivePlayer(player.id, pos, angles, teamId, not keepEquip);
                                        result=true;
                                end
                        end
                end
                player:UpdateAreas();
                if (result) then
                        if(player.actor:GetSpectatorMode() ~= 0) then
                                player.actor:SetSpectatorMode(0, NULL_ENTITY);
                        end
                        if (not keepEquip) then
                                local additionalEquip;
                                if (groupId) then
                                        local group=System.GetEntity(groupId);
                                        if (group and group.GetAdditionalEquipmentPack) then
                                                additionalEquip=group:GetAdditionalEquipmentPack();
                                        end
                                end
                                self:EquipPlayer(player, additionalEquip);
                        end
                        player.death_time=nil;
                        player.frostShooterId=nil;
                        if (self.INVULNERABILITY_TIME and self.INVULNERABILITY_TIME>0) then
                                self.game:SetInvulnerability(player.id, true, self.INVULNERABILITY_TIME);
                        end
                end
                if (not result) then
                        Log("Failed to spawn %s! teamId: %d  groupId: %s  groupTeamId: %d", player:GetName(), self.game:GetTeam(player.id), tostring(groupId), self.game:GetTeam(groupId or NULL_ENTITY));
                end
                local se=SafeWriting.Settings;
                if(se.UsePersistantScores)then
                        if(not player.GotPersistantScore)then
                                SetupPlayerScore(player);
                                player.GotPersistantScore=true;
                        end
                end
                if(se.UseWelcomeMessage)then
                        if(not player.GotWelcomeMessage)then
                                player.GotWelcomeMessage=true;
                                Chat:SendToTarget(nil,player,string.format(se.WelcomeMessage,player:GetName()));
                                if(se.OtherWelcomeMessages)then
                                        for i,v in pairs(se.OtherWelcomeMessages) do
                                                Chat:SendToTarget(nil,player,v);
                                        end
                                end
                        end
                end

                local ent=System.GetEntityByName("JailTrolley");
                if(ent)then
                        if(player.isScriptJailed)then
                                player.inventory:Destroy();
                                local pos=ent:GetPos();
                                pos.z=pos.z-9;
                                TeleportPlayer(player,pos);
                        end
                end

                if BanChecker~=nil then
                        BanChecker(player)
                end

                MakePluginEvent("OnPlayerRevive",player,channelId,keepEquip);
                player.portalTime=_time;
                if self.ResetUnclaimedVehicle then
                        self:ResetUnclaimedVehicle(player.id, false);
                end
                player.lastVehicleId=nil;
                return result;
        end
        SafeWritingGameRules.Server.OnHit=function(self,hit)
                local target = hit.target;
                if g_gameRules.class=="PowerStruggle" then
                        local shooter = hit.shooter;
                        if (shooter and target and shooter.actor and shooter.actor:IsPlayer()) then
                                local team1=self.game:GetTeam(shooter.id);
                                local team2=self.game:GetTeam(target.id);
                                if(team1 == team2 and team1~=0 and shooter.id~=target.id and (hit.type~="repair")) then
                                        hit.damage = hit.damage*self.game:GetFriendlyFireRatio();
                                end
                        end
                end
                if (not target) then
                        return;
                end
                local shooter=hit.shooter;
                if target then if target.id==g_gameRules.id then return; end; end
                if shooter then if shooter.id==g_gameRules.id then return; end; end
                MakePluginEvent("OnHit",hit);
                if (target.actor and target.actor:IsPlayer()) then
                        if (self.game:IsInvulnerable(target.id)) then
                                hit.damage=0;
                        end
                end
                local headshot = self:IsHeadShot(hit);
                if(headshot) then
                        if((AI.GetGroupOf(target.id)==0 and target.AI and target.AI.theVehicle)
                                or (target.AI and target.AI.curSuitMode and target.AI.curSuitMode==BasicAI.SuitMode.SUIT_ARMOR)
                                or (target.Properties and target.Properties.bNanoSuit==1)
                                ) then
                                headshot = false;
                                hit.material_type = "torso";
                        end
                end
                if (self:IsMultiplayer() or ((not hit.target.actor) or (not hit.target.actor:IsPlayer()))) then
                        local material_type=hit.material_type;
                        if(headshot and hit.type == "melee") then
                                material_type="torso";
                        end
                        hit.damage = math.floor(0.5+self:CalcDamage(material_type, hit.damage, self:GetDamageTable(hit.shooter, hit.target), hit.assistance));
                end
                if (self.game:IsFrozen(target.id)) then
                        if ((not target.CanShatter) or (tonumber(target:CanShatter())~=0)) then
                                if (hit.damage>0 and hit.type~="frost") then
                                        self:ShatterEntity(hit.target.id, hit);
                                end
                                return;
                        end
                end
                local dead = (target.IsDead and target:IsDead());
                if (dead) then
                        if (target.Server) then
                                if (target.Server.OnDeadHit) then
                                        if (g_gameRules.game:PerformDeadHit()) then
                                                target.Server.OnDeadHit(target, hit);
                                        end
                                end
                        end
                end
                if ((not dead) and target.Server and target.Server.OnHit) then
                        if(headshot) then
                                if(target.actor and target.actor:LooseHelmet(hit.dir, hit.pos, false)) then
                                        if(not hit.weapon.weapon:IsZoomed()) then
                                                local health = target.actor:GetHealth();
                                                if(health > 2) then
                                                        target.actor:SetHealth(health - 1);
                                                end
                                                target:HealthChanged();
                                                return;
                                        end
                                end
                        end
                        local deadly=false;
                        if (hit.type == "event" and target.actor) then
                                target.actor:SetHealth(0);
                                target:HealthChanged();
                                self:ProcessDeath(hit);
                        elseif (target.Server.OnHit(target, hit)) then
                                if (target.actor and self.ProcessDeath) then
                                        self:ProcessDeath(hit);
                                end
                                deadly=true;
                        end
                        local debugHits = self.game:DebugHits();
                        if (debugHits>0) then
                                self:LogHit(hit, debugHits>1, deadly);
                        end
                end
        end
        SafeWritingGameRules.ProcessActorDamage=function(self,hit)
                local target=hit.target;
                local funcs=SafeWriting.FuncContainer:GetFuncs("ProcessActorDamage");
                if(funcs)then
                        for i,v in pairs(funcs) do
                                return PluginSafeCall(v,hit);
                        end
                end
                local health=target.actor:GetHealth();
                local healthBefore=health;
                local hack=false;
                if SafeWriting.Settings and SafeWriting.Settings.DisableTACOnInfantry then
                        if hit.type and hit.target then
                                if hit.type=="tac" and hit.target and not hit.target:IsOnVehicle() then
                                        if not (hit.shooter and hit.shooter==hit.target) then
                                                hit.damage=0;
                                        end
                                end
                        end
                end
                local dmg=ProcessDamageOfBullet(hit);
                health = math.floor(health - dmg);
                if dmg~=0 then
                        target.actor:SetHealth(health);
                end
                return (health <= 0);
        end
        SafeWritingGameRules.OnTick=function(self)
                if g_gameRules.class=="PowerStruggle" then
                        if (self:GetState()=="InGame") then
                                if(SafeWriting.GameVersion=="1.2.1")then
                                        self:AutoTeamBalanceCheck();
                                        self:UpdateAutoTeamBalance();
                                end
                        end
                        if (self:GetState()~="PostGame") then
                                self:UpdateReviveQueue();
                        end
                end
                --[[if self.GetState and self:GetState()=="PreGame" then
                        local players=self.game:GetPlayers()
                        if players and #players>0 then
                                System.ExecuteCommand("sv_restart")
                        end
                end--]]
                local onTick=self:GetServerStateTable().OnTick;
                SafeWriting:OnTimerTick();
                if (onTick) then
                        onTick(self);
                end
        end
        SafeWritingGameRules.EquipPlayer=function(self,player,additionalEquip)
                if(self.game:IsDemoMode() ~= 0) then -- don't equip actors in demo playback mode, only use existing items
                        Log("Don't Equip : DemoMode");
                        return;
                end;

                if player and player.inventory then
                        if SafeWriting.Settings then
                                if SafeWriting.Settings.DisableNades or SafeWriting.Settings.DisableAllNades then
                                        player.inventory:SetAmmoCapacity("explosivegrenade",0)
                                end
                                if SafeWriting.Settings.DisableAllNades then
                                        player.inventory:SetAmmoCapacity("flashbang",0)
                                        player.inventory:SetAmmoCapacity("smokegrenade",0)
                                        player.inventory:SetAmmoCapacity("empgrenade",0)
                                        player.inventory:SetAmmoCapacity("scargrenade",0)
                                end
                        end
                end

                local funcs=SafeWriting.FuncContainer:GetFuncs("EquipPlayer");
                if(funcs)then
                        for i,v in pairs(funcs) do
                                PluginSafeCall(v, player, additionalEquip);
                        end
                        return;
                end
                player.inventory:Destroy();
                ItemSystem.GiveItem("AlienCloak", player.id, false);
                ItemSystem.GiveItem("OffHand", player.id, false);
                ItemSystem.GiveItem("Fists", player.id, false);
                if (additionalEquip and additionalEquip~="") then
                        ItemSystem.GiveItemPack(player.id, additionalEquip, true);
                end
                if SafeWriting.Settings.UseCustomEquipment then
                        local equip=SafeWriting.Settings.BasicEquipment;
                        local wpn=nil;
                        local addons={};
                        local addonsidx=1;
                        if(equip)then
                                for i,v in ipairs(equip) do
                                        addons={};
                                        addonsidx=1;
                                        if type(v)=="table" then
                                                for q,w in ipairs(v) do
                                                        if(q==1)then
                                                                wpn=w;
                                                        else
                                                                addons[addonsidx]=w;
                                                                addonsidx=addonsidx+1;
                                                        end
                                                end
                                        else
                                                wpn=tostring(v);
                                        end
                                        if(i==1)then
                                                GiveItem(player,wpn,addons,true);
                                        else
                                                GiveItem(player,wpn,addons);
                                        end
                                end
                        end
                else
                        if g_gameRules.class=="PowerStruggle" then
                                local rank=self:GetPlayerRank(player.id);
                                if not rank or rank<1 then
                                        rank=1;
                                end
                                local equip=self.rankList[rank].equip;
                                if (equip) then
                                        for k,e in ipairs(equip) do
                                                ItemSystem.GiveItem(e, player.id, false);
                                        end
                                end
                        else
                                ItemSystem.GiveItem("SOCOM", player.id, true);
                        end
                end
                MakePluginEvent("OnEquipPlayer",player,additionalEquip);
        end
        SafeWritingGameRules.CanEnterVehicle=function(self,vehicle, userId)
                local funcs=SafeWriting.FuncContainer:GetFuncs("CanEnterVehicle");
                local player=System.GetEntity(userId);
                if(funcs)then
                        for i,v in pairs(funcs) do
                                local val=PluginSafeCall(v,vehicle,player, userId);
                                if val~=nil then return val; end
                        end
                end
                if (vehicle.vehicle:GetOwnerId()==userId) then
                        return true;
                end
                if(vehicle.lockowner~=nil and player)then
                        if(vehicle.lockowner==player.profile)then
                                return true;
                        else
                                local tgt=System.GetEntity(userId);
                                Chat:SendToTarget(nil,tgt,"[[THIS_VEHICLE_IS_LOCKED]]!");
                                return false;
                        end
                end

                local vteamId=self.game:GetTeam(vehicle.id);
                local pteamId=self.game:GetTeam(userId);

                if (pteamId==vteamId or vteamId==0) then
                        return vehicle.vehicle:GetOwnerId()==nil;
                elseif (pteamId~=vteamId) then
                        return false;
                end
        end
        SafeWritingGameRules.BuyVehicle=function(self,playerId, itemName)
                if(not SafeWriting.VehiclesAllowed)then
                        return false;
                end
                local factory=self:GetProductionFactory(playerId, itemName, true);
                if (factory) then
                        local limitOk, teamCheck=self:CheckBuyLimit(itemName, self.game:GetTeam(playerId));
                        if (not limitOk) then
                                if (teamCheck) then
                                        self.game:SendTextMessage(TextMessageError, "@mp_TeamItemLimit", TextMessageToClient, playerId, self:GetItemName(itemName));
                                else
                                        self.game:SendTextMessage(TextMessageError, "@mp_GlobalItemLimit", TextMessageToClient, playerId, self:GetItemName(itemName));
                                end
                                return false;
                        end
                        for i,factory in pairs(self.factories) do
                                factory:CancelJobForPlayer(playerId);
                        end
                        local price,energy=self:GetPrice(itemName);
                        if (factory:Buy(playerId, itemName)) then
                                self:AwardPPCount(playerId, -price);
                                self:AwardCPCount(playerId, self.cpList.BUYVEHICLE);

                                if (energy and energy>0) then
                                        local teamId=self.game:GetTeam(playerId);
                                        if (teamId and teamId~=0) then
                                                self:SetTeamPower(teamId, self:GetTeamPower(teamId)-energy);
                                        end
                                end
                                self:AbandonPlayerVehicle(playerId);
                                return true;
                        end
                end
                return false;
        end
        SafeWritingGameRules.OnEnterBuyZone=function(self,zone,player)
                if (zone.vehicle and (zone.vehicle:IsDestroyed() or zone.vehicle:IsSubmerged())) then
                        return;
                end
                if (not self.inBuyZone[player.id]) then
                        self.inBuyZone[player.id]={};
                end
                local was=self.inBuyZone[player.id][zone.id];
                if (not was) then
                        self.inBuyZone[player.id][zone.id]=true;
                        if (self.game:IsPlayerInGame(player.id)) then
                                self.onClient:ClEnterBuyZone(player.actor:GetChannel(), zone.id, true);
                        end
                end
                self.buyList[van][jeep]=true; -- ;<
                MakePluginEvent("OnEnterBuyZone",zone,player);
        end
        SafeWritingGameRules.OnEnterServiceZone=function(self,zone, player)
                if (not self.inServiceZone[player.id]) then
                        self.inServiceZone[player.id]={};
                end

                local was=self.inServiceZone[player.id][zone.id];
                if (not was) then
                        self.inServiceZone[player.id][zone.id]=true;
                        self.onClient:ClEnterServiceZone(player.actor:GetChannel(), zone.id, true);
                end
                MakePluginEvent("OnEnterServiceZone",zone,player);
        end
        SafeWritingGameRules.OnChatMessage=function(self,mType,mSourceId,mTargetId,mMsg)
                local source = NULL_ENTITY;
                local target = NULL_ENTITY;
                local sendfeedback=false;
                local feedbackmsg="";
                local show=true;
                local logCon=true;
                local se=SafeWriting.Settings
                if(IsDllLoaded() or IsDllLoaded100())then
                        if(mSourceId)then
                                source=System.GetEntity(mSourceId);
                        end
                        if(mTargetId)then
                                target=System.GetEntity(mTargetId);
                        end
                else
                        source=mSourceId;
                        target=mTargetId;
                end
                if(not target or target==nil)then
                        target=source;
                end
                source.ChatExceptions=source.ChatExceptions or 0;
                if source.ChatExceptions<=0 then
                        if se.DetectChatFlood then
                                FloodCheck(source,"chat",3);
                        end
                else
                        source.ChatExceptions=source.ChatExceptions-1;
                end
                local plg,plgs=MakePluginEvent("OnChatMessage",mType,source,target,mMsg);
                if plg~=nil then mMsg=plg; end
                if plgs~=nil then show=plgs; end
                local fMsg=mMsg;
                if(mMsg:sub(0,1)=='@')then
                        if(se.AllowShortPMs)then
                                ScanForChatCommand(source, "!pm "..mMsg:sub(2));
                                show=false;
                        end
                end
                if SafeWriting.NumVersion>=218 and source and (not source.IsMuted) then
                        local echo=false;
                        if g_gameRules.game.CanAllSeeChat then
                                echo=g_gameRules.game:CanAllSeeChat();
                                if not echo then
                                        echo=se.CanAllSeeChat;
                                end
                        else
                                echo=false;
                        end
                        local te=SafeWriting.TempEntity;
                        if te and source.id == te.id then if not IsDllLoaded100() then show=false; else logCon=false; end end
                        local origMsg=mMsg;
                        if echo and source.host then
                                local nname=source:GetName()
                                local isspect=false;
                                if source.actor then isspect=source.actor:GetSpectatorMode()~=0; end
                                local changed=false;
                                if isspect and (not source:IsDead()) then
                                        mMsg="[spectator] "..mMsg;
                                        changed=true;
                                elseif source:IsDead() then
                                        mMsg="[dead] "..mMsg;
                                        changed=true;
                                end
                                if changed and (not IsDllLoaded()) then
                                        mMsg=source:GetName()..mMsg;
                                end
                                if (not IsDllLoaded()) and changed then
                                        if self.class=="PowerStruggle" then
                                                local finalTeam=source.lastKnownTeam;
                                                if not finalTeam and not isspect then
                                                        finalTeam=g_gameRules.game:GetTeam(source.id);
                                                end
                                                if not finalTeam then
                                                        mType=ChatToAll;
                                                        finalTeam=2;
                                                end
                                                g_gameRules.game:SetTeam(finalTeam,te.id);
                                                if not isspect then
                                                        source.lastKnownTeam=g_gameRules.game:GetTeam(source.id);
                                                end
                                        end
                                        if mType==ChatToAll then
                                                g_gameRules.game:SendChatMessage(ChatToAll,te.id,te.id,mMsg);
                                        elseif mType==ChatToTeam then
                                                g_gameRules.game:SendChatMessage(ChatToTeam,te.id,te.id,mMsg);
                                        else
                                                if target then
                                                        g_gameRules.game:SendChatMessage(ChatToTarget,te.id,target.id,mMsg);
                                                end
                                        end
                                        te.ChatExceptions=(te.ChatExceptions or 0)+1;
                                        mMsg=origMsg;
                                        show=true;
                                end
                        end
                end
                local state=ScanForChatCommand(source,fMsg);
                if(state or state==nil)then
                        show=false;
                end
                if(source.IsMuted==true)then
                        show=false;
                        sendfeedback=true;
                        feedbackmsg="[[YOU_ARE_MUTED]]!";
                end
                local oMsg=mMsg;
                if (se.CensoreBadWords) then
                        for i,word in pairs(se.BadWords) do
                                if type(word)=="table" then
                                        mMsg=string.gsub(mMsg,word[1],word[2] or "");
                                else
                                        mMsg=string.gsub(mMsg,word,word:sub(1,1)..string.rep(se.CensoreCharacter or "*",string.len(word)-2)..word:sub(-1));
                                end
                        end
                        if(mMsg~=oMsg)then
                                sendfeedback=true;
                                feedbackmsg="[[PLEASE_DONT_SWEAR]]!";
                        end
                end
                if fMsg:len()==0 then
                        show = false;
                end
                if show then
                        if(se.EnableChatLog and logCon)then
                                local tgtname="<Unknown>";
                                if(mType==ChatToAll)then
                                        tgtname="ALL";
                                elseif(mType==ChatToTeam)then
                                        tgtname="TEAM";
                                else
                                        if(target)then
                                                tgtname=target:GetName();
                                        end
                                end
                                local srcname="<Unknown>";
                                if(source)then
                                        srcname=source:GetName();
                                end
                                local logMsg=mMsg;
                                if ClearString then
                                        logMsg=ClearString(logMsg);
                                end
                                if(tgtname=="ALL")then
                                        g_gameRules.game:SendTextMessage(TextMessageConsole,SpecialFormat("${t:ChatMsg1|5}[CHAT] ${t:ChatMsg2|8}%s ${t:ChatMsg3|6}to ${t:ChatMsg4|8}%s: ${t:ChatMsg5|6}%s", srcname, tgtname, logMsg),TextMessageToAll);
                                elseif(tgtname=="TEAM" or tgtname=="Team")then
                                        local players=g_gameRules.game:GetPlayers();
                                        for i,player in ipairs(players) do
                                                if(g_gameRules.game:GetTeam(player.id)==g_gameRules.game:GetTeam(source.id) or (se.LogChatToAdmins and IsAdmin(player)))then
                                                        g_gameRules.game:SendTextMessage(TextMessageConsole,SpecialFormat("${t:ChatMsg1|5}[CHAT] ${t:ChatMsg2|8}%s ${t:ChatMsg3|6}to ${t:ChatMsg4|8}%s: ${t:ChatMsg5|6}%s",srcname,tgtname,logMsg),TextMessageToClient,player.id);
                                                end
                                        end
                                else
                                        g_gameRules.game:SendTextMessage(TextMessageConsole,SpecialFormat("${t:ChatMsg1|5}[CHAT] ${t:ChatMsg2|8}%s ${t:ChatMsg3|6}to ${t:ChatMsg4|8}%s: ${t:ChatMsg5|6}%s",srcname,tgtname,logMsg),TextMessageToClient,target.id);
                                        if(se.LogChatToAdmins)then
                                                local players=g_gameRules.game:GetPlayers();
                                                if(players)then
                                                        for i,player in ipairs(players) do
                                                                if(IsAdmin(player) and target~=player)then
                                                                        g_gameRules.game:SendTextMessage(TextMessageConsole,SpecialFormat("${t:ChatMsg1|5}[CHAT] ${t:ChatMsg2|8}%s ${t:ChatMsg3|6}to ${t:ChatMsg4|8}%s: ${t:ChatMsg5|6}%s",srcname,tgtname,logMsg),TextMessageToClient,player.id);
                                                                end
                                                        end
                                                end
                                        end
                                end
                        end
                end
                if(sendfeedback)then
                        Chat:SendToTarget(nil,source,feedbackmsg);
                end
                if show and fMsg:len()>0 then
                        return mMsg;
                end
        end
        SafeWritingGameRules.OnPlayerRename=function(self,playerid,newname,isUserData)
                local player;
                if(not isUserData)then
                        player=System.GetEntity(playerid);
                else
                        player=playerid;
                end
                local plugName=(IsDllLoaded() or IsDllLoaded100()) and player:GetName() or player.LastName;
                local ignoreRename=player.exceptRename or 0;
                if ignoreRename==0 then
                        local canrename=true;
                        local name=newname;
                        FloodCheck(player,"rename",15,2);
                        if player.DisallowRenaming then canrename=false; end
                        local protected = {};
                        if SafeWriting.Settings.ProtectNames then
                                local lookup =  newname:gsub("^[\\$][0-9]", ""):gsub("([^a-zA-Z0-9])",""):lower();
                                local a,m,p,o=SafeWriting.Settings.Admins, SafeWriting.Settings.Moderators,SafeWriting.Settings.Premiums,SafeWriting.Settings.Others;
                                a = a or {};
                                m = m or {};
                                p = p or {};
                                o = o or {};
                                for i,v in pairs(a) do
                                        if type(v)=="string" then
                                                protected[v:gsub("^[\\$][0-9]", ""):gsub("([^a-zA-Z0-9])",""):lower()] = i;
                                        end
                                end
                                for i,v in pairs(m) do
                                        if type(v)=="string" then
                                                protected[v:gsub("^[\\$][0-9]", ""):gsub("([^a-zA-Z0-9])",""):lower()] = i;
                                        end
                                end
                                for i,v in pairs(p) do
                                        if type(v)=="string" then
                                                protected[v:gsub("^[\\$][0-9]", ""):gsub("([^a-zA-Z0-9])",""):lower()] = i;
                                        end
                                end
                                for i,v in pairs(o) do
                                        if type(v)=="string" then
                                                protected[v:gsub("^[\\$][0-9]", ""):gsub("([^a-zA-Z0-9])",""):lower()] = i;
                                        end
                                end
                                if protected[lookup] and tostring(player.profile) ~= tostring(protected[lookup]) then
                                        canrename = false;
                                        player.desiredName = newname;
                                end
                        end
                        if(canrename)then
                                name=name or "empty";
                                name=name:gsub(" ","-"); --block spaces in name because of chat commands
                                name=name:gsub("%%","_");
                                if(SafeWriting.Settings.UseClearNames)then
                                        name=ClearString(name,true);    --removes all !,�,y,�,2,�,1,^,�,c
                                end
                                if(SafeWriting.Settings.EnableCrews)then
                                        if(player.CrewName)then
                                                local tag=GetCrewTag("left")..player.CrewName..GetCrewTag("right");
                                                if(name:sub(0,string.len(tag))~=tag)then
                                                        name=tag..name;
                                                end
                                        end
                                end
                                local plg,plgc=MakePluginEvent("OnRename",player,name,plugName,newname);
                                if plg~=nil then
                                        name=plg;
                                end
                                if plgc~=nil then
                                        if plgc==false then return; end
                                end
                                return name;
                        end
                end
                player.exceptRename=math.max(0,ignoreRename-1);
                player.lastrename=_time;
        end
        SafeWritingGameRules.GatherClientData=function(self,channelId,hostname,profileId,ip)
                if(SafeWriting.Settings.UseDLLInfoLoader)then
                        local player;
                        player=g_gameRules.game:GetPlayerByChannelId(tonumber(channelId));
                        local hostport={};
                        hostport=split(hostname,":");
                        local ipdetect="C++";
                        if(player)then
                                local plName=player:GetName() or "<unknown>";
                                player.connecttime=_time;
                                player.channelId=channelId;
                                player.host=tostring(hostport[0]);
                                player.port=tonumber(hostport[1]);
                                player.profile=profileId;
                                player.ip=ip or "unknown";
                                local clientName="SfwCl";
                                if(not IsRealIP(ip))then
                                        player.ip=_GetIP(ip);
                                        ipdetect="Lua";
                                end
                                local profIdNum=tonumber(profileId);
                                if profIdNum~=0 then
                                        player.isOpenSpy=true;
                                        player.gsprofile = profileId;
                                        profIdNum=Random(800000,999999);
                                        player.profile=tostring(profIdNum);
                                        profileId=tostring(profIdNum);
                                        clientName="GSMaster";
                                else
                                        player.isSfwCl=true;
                                end
                                SfwLog("Player "..plName.."("..channelId..") connected ("..hostport[0]..":"..player.port..", IP: "..player.ip.." - "..ipdetect.."), profile: "..profileId..", client: "..clientName);
                                if not _G["ChannelInfo"] then _G["ChannelInfo"]={}; end
                                if not _G["ChannelInfo"][channelId] then
                                        _G["ChannelInfo"][channelId]={
                                                profile=player.profile;
                                                host=player.host;
                                                ip=player.ip;
                                                channelId=player.channelId;
                                                port=player.port;
                                                state=3;
                                        };
                                end
                                CheckPlayer(player,true);
                        else
                                SfwLog("Player "..channelId.." not found (host: "..hostport[0]..",port: "..hostport[1].."), profile: "..profileId);
                        end
                end
        end
        SafeWritingGameRules.ActorOnShoot=function(self,actorId,ammoId,pos,dir,vel,fireRate,weaponId,weaponclass)
                local gd=SafeWriting.GlobalData;
                local se=SafeWriting.Settings;
                local player=System.GetEntity(actorId);
                if(not player.host)then
                        return;
                end
                local vehicleclass=" - ";
                local wpn=nil;
                if(not IsDllLoaded())then
                        if(player)then
                                if(not player.class)then
                                        player.class="";
                                end
                                if(player.class=="AutoTurret" or player.class=="AutoTurretAA" or player.class=="AlienTurret")then
                                        return;
                                end
                                weaponclass="";
                                vehicleclass=" - ";
                                if(player.inventory)then
                                        local wpnId=player.inventory:GetCurrentItemId();
                                        if(wpnId)then
                                                wpn=System.GetEntity(wpnId);
                                                weaponclass=wpn.class or "";
                                                if(wpn)then
                                                        if(weaponclass=="AlienMount")then --Exception for MOAR, but also for MOAC ;(
                                                                return;
                                                        end
                                                end
                                        end
                                end
                                if(player:IsOnVehicle())then
                                        local vehicle = System.GetEntity(player.actor:GetLinkedVehicleId());
                                        if(vehicle)then
                                                local seat=vehicle:GetSeat(player.id);
                                                vehicleclass=vehicle.class;
                                                if(seat)then
                                                        local wc=seat.seat:GetWeaponCount();
                                                        for i=1,wc do
                                                                local wpnid=seat.seat:GetWeaponId(i);
                                                                if(wpnid)then
                                                                        wpn=System.GetEntity(wpnid);
                                                                        local wpnclass=wpn.class or "";
                                                                        weaponclass=wpnclass;
                                                                        if(wpnclass=="VehicleMOARMounted" or wpnclass=="VehicleMOAR")then --exception for MOAR
                                                                                return;
                                                                        end
                                                                        if(wpnclass=="Asian50Cal" or wpnclass=="USCoaxialGun" or wpnclass=="AvengerCannon" or wpnclass=="SideWinder")then --exception for machine guns
                                                                                return;
                                                                        end
                                                                end
                                                        end
                                                end
                                        end
                                end
                        end
                else
                        wpn=System.GetEntity(weaponId);
                        if(weaponclass=="AlienMount" or weaponclass=="VehicleMOARMounted" or weaponclass=="VehicleMOAR")then
                                return;
                        end
                end
                if(player.lastshoot==nil)then
                        player.recoilShootT=_time;
                        player.lastshoot=_time;
                        player.totalshoots=0;
                end
                if(player.lastfr)then
                        if(player.lastfr~=fireRate)then
                                player.totalshoots=0;
                        end
                end
                player.lastfr=fireRate;
                player.totalshoots=player.totalshoots+1;
                if(fireRate==0)then     --force fireRate (120) for all explosives
                        fireRate=120;
                end
                local chanId=0;
                if(player.actor)then
                        chanId=player.actor:GetChannel();
                end
                local ping=math.floor((self.game:GetPing(chanId) or 0)*1000+0.5);
                local expPing=ping+((_time-SafeWriting.lastUpdate)*1000);
                local expected=(500*fireRate+fireRate*expPing)/30000;
                --local timenow=_time;
                if(se.DetectRapidFire and _time-player.lastshoot>=1 and weaponclass~="Fists")then
                        if(player.totalshoots>((math.ceil(expected)+4)))then
                                if(not se.RapidFireLogDetails)then
                                        AntiCheat:DealWithPlayer(player,"rapid-fire hack");
                                else
                                        local isonveh="false";
                                        if(player:IsOnVehicle())then
                                                isonveh="true";
                                        end
                                        AntiCheat:DealWithPlayer(player,"rapid-fire hack",true,"Shoots per second detected: "..player.totalshoots,"Using weapon: "..weaponclass,"Fire rate: "..fireRate,"Is on vehicle: "..isonveh,"Vehicle class: "..vehicleclass,"Ping: "..ping,"Latency: "..(_time-SafeWriting.lastUpdate));
                                end
                        end
                        player.lastshoot=_time;
                        player.totalshoots=0;
                end
                local hit={
                        shooter=player;
                        weapon=wpn;
                        fireRate=fireRate;
                        dir=dir;
                        pos=pos;
                        velocity=vel;
                        weaponClass=wpnclass;
                        ammoId=ammoId;
                        weaponId=weaponId;
                        shooterId=actorId;
                };
                MakePluginEvent("OnShoot",hit)
        end
        SafeWritingGameRules.OnCheatDetected=function(self,playerId,desc,logd,...)
                local player=System.GetEntity(playerId);
                if(player)then
                        AntiCheat:DealWithPlayer(player,desc,logd,...);
                end
        end
        SafeWritingGameRules.InformChannelLua=function(self,chnl,host,profile,ip)
                if not _G["ChannelInfo"] then _G["ChannelInfo"]={}; end
                if(not IsRealIP(ip))then
                        ip=_GetIP(ip);
                end
                local hostport=split(host,":");
                _G["ChannelInfo"][chnl]={
                        host=hostport[0];
                        port=tonumber(hostport[1]);
                        profile=profile;
                        channelId=chnl;
                        ip=ip;
                };
        end
        SafeWritingGameRules.OnLeaveVehicleSeat=function(self,vehicle, seat, entityId, exiting)
                MakePluginEvent("OnLeaveVehicleSeat",vehicle,seat,entityId,exiting);
                if (self.isServer and self:GetState()=="InGame") then
                        if (exiting) then
                                local empty=true;
                                for i,seat in pairs(vehicle.Seats) do
                                        local passengerId = seat:GetPassengerId();
                                        if (passengerId and passengerId~=NULL_ENTITY and passengerId~=entityId) then
                                                empty=false;
                                                break;
                                        end
                                end
                                if (empty) then
                                        vehicle.lastOwnerId=entityId;
                                        local player=System.GetEntity(entityId);
                                        if (player) then
                                                player.lastVehicleId=vehicle.id;
                                        end
                                end
                                if(entityId==vehicle.vehicle:GetOwnerId()) then
                                        vehicle.vehicle:SetOwnerId(NULL_ENTITY);
                                end
                                --if vehicle.class=="US_vtol" or vehicle.class=="Asian_helicopter" then
                                        g_gameRules.game:SetInvulnerability(entityId,true, (vehicle.vehicle:GetMovementType()=="air" and 2 or 1));
                                --end
                        end
                end
        end
end

function IsSafeWritingGameRulesLoaded()
        return g_gameRules.UpdatePings == SafeWritingGameRules.UpdatePings
end

function SafeWriting_OnUpdate(dt)
        if not g_gameRules then
                return
        end
        if not SafeWriting then
                Script.LoadScript(SAFEWRITING_ROOTDIR:gsub("%\\", "/") ..  "/SafeWritingMain.lua", 1, 1)
        end

        --SafeWriting:OnTimerTick(dt)

        if not IsSafeWritingGameRulesLoaded() then
                printf("Overriding game rules")
                for i, v in pairs(SafeWritingGameRules.Server) do
                        g_gameRules.Server[i] = v
                end

                for i, v in pairs(SafeWritingGameRules) do
                        if i ~= "Server" then
                                g_gameRules[i] = v
                        end
                end

                --[[if not g_gameRules.____RedefinedTimer then
                        g_gameRules.SetTimer=MergeFunctions(g_gameRules.SetTimer,function(self,timerId,msec,so)
                                if not so then
                                        Script.SetTimer(msec,function()
                                                self.Server.OnTimer(self,timerId,msec);
                                        end);
                                end
                        end);
                        g_gameRules.____RedefinedTimer=true;
                end--]]

                LinkToRules("OnStartGame");
                LinkToRules("OnClientConnect");
                LinkToRules("OnClientDisconnect");
                LinkToRules("OnChangeSpectatorMode");
                LinkToRules("OnItemPickedUp");
                LinkToRules("OnItemDropped");
                LinkToRules("OnFreeze");
                LinkToRules("OnTimer");
                LinkToRules("OnHit");
                if g_gameRules.class=="PowerStruggle" then
                        LinkToRules("SvBuy");
                end
        else
                SafeWriting:OnTimerTick(dt)
        end
end