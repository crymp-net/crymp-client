--Created 9.9.2012 as part of SSM SafeWriting project by 'Zi;'
function in_array_ent_inv_getname(a,b)
	for i,v in pairs(b) do
		if(System.GetEntity(v):GetName() == a:GetName())then
			return true;
		end
	end
	return false;
end
DetectWallHack=false
function AntiCheat:PlayerPositionCheck(player)
	local se=SafeWriting.Settings;	
	if(not se.AntiCheatEnabled)then
		return;
	end
	if not player.host then return; end
	local x,y,z,tz=0;
	local lasttp=0;
	self.gravitation=System.GetCVar("p_gravity_z");
	self.speed_mult=System.GetCVar("g_suitSpeedMultMultiplayer");
	self.speed_threshold=math.max(10,math.abs(self.gravitation))*(self.speed_mult+1)*3;
	x=player:GetWorldPos().x;
	y=player:GetWorldPos().y;
	z=player:GetWorldPos().z;
	tz=z-System.GetTerrainElevation(player:GetWorldPos());
	if(player.portalTime~=nil)then
		lasttp=player.portalTime;
	end	
	if(player.actor:GetSpectatorMode()==0 and (not player:IsDead())) then
		if se.DetectTeleportHack then
			AntiCheat:TeleportHackCheck(player,x,y,z,lasttp);
		end
		if se.DetectFlyHack then
			AntiCheat:FlyHackCheck(player,x,y,z,tz,lasttp);
		end
	end
end
function AntiCheat:TeleportHackCheck(player,x,y,z,lasttp)
	if(player)then
		if(not player.TpCount)then
			player.TpCount=0;
		end
		if(_time-lasttp>5 and not player:IsOnVehicle())then
			if(player.lastPos)then
				local dist=GetDistance(player:GetPos(),player.lastPos);
				self.maxTpDist=self.maxTpDist or 0;						--debug!
				if dist>self.maxTpDist then self.maxTpDist=dist; end	--debug!
				if(dist>(self.speed_threshold))then
					if(player.TpCount>=3)then
						AntiCheat:DealWithPlayer(player,"teleport hack",true,"Speed: "..dist,"Threshold: "..math.abs(self.speed_threshold));
					else
						local kills,deaths=GetPlayerScore(player);
						g_gameRules:KillPlayer(player);
						SetPlayerScore(player,kills,deaths);
						player.TpCount=player.TpCount+1;
					end
				end
			end				
		end
		player.lastPos=player:GetPos();
	end
end
function AntiCheat:FlyHackCheck(player,x,y,z,tz,lasttp)
	if(_time-lasttp>2)then
		if(not player)then
			return;
		end
		if(not player.lInAir)then
			player.lInAir=0;
			player.lastTz=tz;
		end
		local ents=System.GetEntitiesInSphere(player:GetWorldPos(),10);
		local count=table.getn(ents)-1;
		local inv=player.inventory:GetInventoryTable();
		for i,v in pairs(ents) do
			if(in_array_ent_inv_getname(v,inv))then
				count=count-1;				
			end
		end
		if(count<=0 and tz>4.0)then
			if(tz>player.lastTz)then
				player.lInAir=player.lInAir+2;
				if(player.lInAir>=16)then
					AntiCheat:DealWithPlayer(player,"fly hack");
				end
			else
				player.lInAir=math.max(0,player.lInAir-2);
			end
		else
			player.lInAir=0;
		end
		player.lastTz=tz;
	end
end
function AntiCheat:HitCheck(hit)
	local se=SafeWriting.Settings;
	if(not se.AntiCheatEnabled)then
		return false;
	end
	local distance=1;
	local shooter=hit.shooter;
	local target=hit.target;
	--local shooter=shooter or ({});
	if(shooter)then
		if(not shooter.hithackdetect)then
			shooter.hithackdetect=0;
		end
	end
	if(shooter and target)then
		distance=shooter:GetDistance(target.id);
	end
	local wpn=hit.weapon;
	local verifyWpn={class="<unknown>";};
	local wpnclass="<unknown>";
	if(wpn)then
		wpnclass=wpn.class;
	end
	verifyWpn.class=wpnclass;
	local dmg=hit.damage;
	local typ=hit.type or "none";
	local fistsexcept=false;
	local expectedDmg=4000;
	local veh=false;
	local multiplier=(g_gameRules.DamagePlayerToPlayer[hit.material_type] or 2.25);
	local forceCheck=false;
	local eAmmo=0;
	if(wpn)then
		if(wpn.weapon)then
			local wDmg=wpn.weapon:GetDamage();
			if((wDmg or -1)>0)then
				expectedDmg=multiplier*wDmg*1.5;
			end
			eAmmo=wpn.weapon:GetAmmoCount();
		elseif(wpn.class=="Player")then
			expectedDmg=200;
			forceCheck=true;
		end
		if(wpn.vehicle)then veh=true; end
	end
	if se.DetectDamageHack and dmg>expectedDmg and target and shooter and (not wpn) and shooter.host then
		if shooter~=target then
			AntiCheat:DealWithPlayer(shooter,"atom hack");
			return true;
		end
	end
	if(shooter and target and shooter~=target and shooter.host)then	
		if(not shooter.last_eAmmo)then shooter.last_eAmmo=0xFFFFFFFF; end
		if(not shooter.last_eShoot)then shooter.last_eShoot=_time; end
		if(dmg>expectedDmg)then
			if(shooter.inventory)then
				local wpnId=shooter.inventory:GetCurrentItemId();
				if(wpnId)then
					verifyWpn=System.GetEntity(wpnId);
				end
			end
			if(se.DetectDamageHack and (not wpnclass:find("explosive",nil,true)) and wpnclass~="AACannon" and (verifyWpn.class==wpnclass or forceCheck))then
				if(target.host and shooter.host)then
					local team=g_gameRules.game:GetTeam(target.id);
					if(team~=0 or (g_gameRules.class=="InstantAction" and target.host))then
						local isVeh=shooter:IsOnVehicle();
						local wpnVeh=false;
						if(hit.weapon)then
							if(hit.weapon.vehicle)then wpnVeh=true; end
						end
						if(not (isVeh or wpnVeh))then
							if(wpnclass=="Fists" or typ=="melee")then
								local cl_strength=tonumber(System.GetCVar("cl_strengthscale"));
								if(dmg<=(4000*cl_strength))then
									fistsexcept=true;
								end
							end
							if(not fistsexcept)then								
								shooter.hithackdetect=1;
								AntiCheat:DealWithPlayer(shooter,"damage hack",true,"Shot player '"..target:GetName().."' with "..wpnclass.." and made damage "..dmg,"Damage threshold: "..expectedDmg,"Material type: "..(hit.material_type or "<unknown>"),"Multiplier of damage: "..(g_gameRules.DamagePlayerToPlayer[hit.material_type] or 2.25));		
								return true;
							end
						end
					end
				end
			end
		end
		if(se.DetectGhostGlitch and wpnclass=="Fists" and distance>5 and (not target.actor:IsDead()))then
			AntiCheat:DealWithPlayer(shooter,"ghost glitch");
			return true;
		end
		local validAmmo=true;
		if(eAmmo<=0 or eAmmo==20)then validAmmo=false; end
		--if(eAmmo==shooter.last_eAmmo and eAmmo>0)then validAmmo=false; end
		if se.DetectInfiniteAmmo and wpn and wpn.Properties and (not shooter:IsOnVehicle()) and (not veh) then
			if( not validAmmo and _time-shooter.last_eShoot<=1 and shooter.last_eAmmo==eAmmo and wpnclass~="AlienMount" and wpn.Properties.bMounted==0 and wpnclass~="Shotgun" and hit.type=="bullet" and not wpnclass:find("explo") )then
				shooter.infAmmoSuspections=shooter.infAmmoSuspections or 0;
				if shooter.infAmmoSuspections>2 then
					AntiCheat:DealWithPlayer(shooter,"infinite ammo hack",true,"Using weapon "..wpnclass,"Ammo left: "..eAmmo,"last_eAmmo: "..shooter.last_eAmmo,"Time between hits: "..(_time-shooter.last_eShoot));
					return;
				else
					shooter.infAmmoSuspections=shooter.infAmmoSuspections+1;
				end			
			end	
		end
		if DetectWallHack and wpn and wpn.Properties and (shooter.IsOnVehicle and not shooter:IsOnVehicle()) and (not veh) and (target.IsOnVehicle and not target:IsOnVehicle()) then
			if not AntiCheat:CanHit(shooter.actor:GetHeadPos(), shooter.actor:GetHeadDir(), shooter.id, target) then
				Console:SendToAll("HACK!!! WALLHACK!!!")
			end
		end
		if se.LogHitsToConsole then
			if shooter.actor and target.actor then
				Msg:SendToTarget(target,string.format("$3Player $6%s $3hit you with $6%s$3, nanosuit mode $6%s$3 with $6%d$3 energy, damage $6%d$3, ammo left: $6%d$3, max dmg:$6%.2f",shooter:GetName(),wpnclass,SafeWriting.NanosuitModes[shooter.actor:GetNanoSuitMode() or 0],shooter.actor:GetNanoSuitEnergy()/2,dmg,eAmmo,expectedDmg),"console");
			end
		end
		shooter.last_eAmmo=eAmmo;
		shooter.last_eShoot=_time;
	end
	return false;
end
function GetDst(a,b)
	local dx,dy,dz=a.x-b.x,a.y-b.y,a.z-b.z;
	return math.sqrt(dx*dx+dy*dy+dz*dz)
end
function AntiCheat:CanHit(posvec, dirvec, shooterid, target)
	local hittbl={};
	local tgtpos = target:GetPos()
	dirvec.x = dirvec.x * 8192;
	dirvec.y = dirvec.y * 8192;
	dirvec.z = dirvec.z * 8192;
	local shid = shooterid
	local it = 0
	local found = false
	while (not found) and it<3 do
		local hits=Physics.RayWorldIntersection(posvec, dirvec, 10, ent_all, shooterid, nil, hittbl);
		for i,hit_ in pairs(hittbl) do
			local dst = GetDst(hit_.pos, tgtpos)
			--Console:SendToAll("RAY %d/%d (it: %d), DST: %f", i, hits, it, dst)
			if hit_.entity then
				shooterid = hit_.entity.id
			end
			if dst<2 then found = true; end
			posvec = hit_.pos;
			
			--g_gameRules:CreateExplosion(shid,sound,0,posvec,g_Vectors.up,1,1,1,1,"explosions.grenade_air.explosion",0.5, 0.5, 0.5, 0.5);
			posvec.x = posvec.x + 4
			posvec.y = posvec.y + 4
			posvec.z = posvec.z + 4
		end	
		it = it + 1
	end
	return true;
end
function AntiCheat:DealWithPlayer(player,hack,logd,...)
	if(not SafeWriting.Settings.AntiCheatEnabled)then
		return;
	end
	if(player.wasForceDisconnected)then
		return;
	end
	Chat:SendToAll(nil,SpecialFormat("Cheater detected: %s is using %s",player:GetName(),hack)); 
	if(hack=="damage hack" or hack=="freeze hack" or hack=="atom hack" or hack=="hit spoof")then
		AntiCheat:BanForHack(player,hack,true); --set to true,if permaban, otherwise normal ban
	elseif(hack=="teleport hack")then
		AntiCheat:KickForHack(player,hack);
	elseif(hack=="fly hack") then
		AntiCheat:KickForHack(player,hack);
	elseif(hack=="rename flooding") then
		player.DisallowRenaming=true;
	elseif(hack=="rapid-fire hack")then
		AntiCheat:KickForHack(player,hack);
	elseif(hack=="spoofed profile id")then
		AntiCheat:KickForHack(player,hack);
	elseif(sfwstrfind(hack,"spoof")>-1)then
		AntiCheat:KickForHack(player,hack, 1800);
	else
		AntiCheat:BanForHack(player,hack);	--default
	end
	if(logd)then
		if(...~=nil)then
			local f,err=io.open(SafeWriting.GlobalStorageFolder.."DetailsLog.txt","a+");
			if(f)then
				f:seek("end");
				f:write("Detected: "..player:GetName().."["..player.profile..","..player.host.."] using "..hack.."\n");
				for i,v in pairs({...})do
					f:write("  "..i..". - "..tostring(v).."\n");
				end
				f:close();
			end
		end
	end
end
function AntiCheat:BanForHack(player,hack,perma)
	local long=false;
	if(perma)then
		local fmt="%s";
		if long then fmt="You were permabanned from this server for using %s"; end
		local p, t = true, nil;
		if type(perma)=="number" then
			p = false;
			t = Time(perma);
		end
		PermaBanPlayer(player, SpecialFormat(fmt,hack), "anti cheat", p, t);
	else
		local fmt="%s";
		if long then fmt="You were banned from this server for using %s"; end
		BanPlayer(player,SpecialFormat(fmt,hack));
	end
end
function AntiCheat:KickForHack(player,hack)
	local long=false;
	local fmt="%s";
	if long then fmt="You were kicked from this server for using %s"; end
	KickPlayer(player,SpecialFormat(fmt,hack));
end
function AntiCheat:TellDebug()
	printf("Max teleport distance: %.3f",self.maxTpDist);
	printf("Speedhack threshold:   %.3f",self.speed_threshold);
end
System.AddCCommand("sfw_ac_info","AntiCheat:TellDebug()","Tells debug informations");