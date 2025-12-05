--------------------------------------------------------------------------
--	Crytek Source File.
-- 	Copyright (C), Crytek Studios, 2001-2004.
--------------------------------------------------------------------------
--	$Id$
--	$DateTime$
--	Description: Common code for all/most of the vehicle implementations
--  
--------------------------------------------------------------------------
--  History:
--  - 30/11/2004   15:36 : Created by Mathieu Pinard
--
--------------------------------------------------------------------------

Script.ReloadScript("Scripts/Utils/Math.lua");
Script.ReloadScript("Scripts/Entities/Vehicles/VehicleSeat.lua");


VehicleBase =
{  
	State =
	{
		pos = {},		
		Carriage = {},		
		aiDriver = nil,
	},
	
	Seats = {},
	AI = {},

	Client = {},
	Server = {},
	
	Hit = {},	
}

--------------------------------------------------------------------------
function IsAnyPassenger(seats)
	for i,seat in pairs(seats) do	  	  
		if (seat:GetPassengerId()) then		  
			return true;
		end
	end
	
	return false;
end

--------------------------------------------------------------------------
function VehicleBase:HasDriver()
	for i,seat in pairs(self.Seats) do
		if (seat.isDriver) then
			if (seat.passengerId) then
				return true;
			end
		end
	end

	return false;
end

--------------------------------------------------------------------------
function VehicleBase:GetDriverId()
	for i,seat in pairs(self.Seats) do
		if (seat.isDriver) then
			return seat:GetPassengerId();
		end
	end

	return nil;
end

--------------------------------------------------------------------------
function VehicleBase:GetLastDriverId()
	local driverId = self:GetDriverId();
	if(not driverId) then
		return self.lastDriverId;
	end

	return nil;
end

--------------------------------------------------------------------------
function VehicleBase:ClearLastDriverId()
	-- don't clear if we've got a new driver
	if(not self:HasDriver()) then
		self.lastDriverId = nil;
	end
end

--------------------------------------------------------------------------
function GetNextAvailableSeat(seats)
	for i,seat in pairs(seats) do
		if (not seat:GetPassengerId()) then
			return i;
		end
	end
	
	return -1;
end

--------------------------------------------------------------------------
function VehicleBase:ApplyMaterial(strMat, bIncludeCharacters)	  
  --Log("VehicleBase: setting material "..strMat.." on "..self:GetName());
  
  Log("VehicleBase:ApplyMaterial is deprecated!");
  --self.vehicle:SetSlotMaterial(self.id, -1, strMat, bIncludeCharacters);
end

--------------------------------------------------------------------------
function VehicleBase:InitSeats()
  if (self.Seats) then
    for i,seat in pairs(self.Seats) do
  		mergef(seat, VehicleSeat, 1);
  		seat:Init(self, i);
  	end
  end
end

--------------------------------------------------------------------------
function VehicleBase:InitVehicleBase()
	self:OnPropertyChange();
end

--------------------------------------------------------------------------
function VehicleBase:OnPropertyChange()	
	if (self.OnPropertyChangeExtra) then		
		self:OnPropertyChangeExtra();
	end
end

--------------------------------------------------------------------------
function VehicleBase:MountEntity(a_className, transformTable, propertiesTable) -- integrate this with other child entities asap
  --Log("Attaching a "..a_className.." at "..self:GetName().."..");
  
  local spawnParams = {};
	spawnParams.class = a_className;
	spawnParams.name = self:GetName().."_"..a_className.."_mount"; -- todo: multiple names
	spawnParams.scale = 1;
	spawnParams.flags = 0;
			
  -- spawn at helper if present, otherwise 0,0,0
  -- add pos + rot if present
	if (transformTable.helper) then
	  if (self.vehicle:HasHelper(transformTable.helper)) then
  	  spawnParams.position = self.vehicle:GetHelperPos(transformTable.helper, true);	  
  		spawnParams.orientation = self.vehicle:GetHelperDir(transformTable.helper, true);
    else
      spawnParams.position = {0,0,0};
  		spawnParams.orientation = {0,1,0};
    end		
	else	
	  spawnParams.position = self:GetPos();
	end
	if (transformTable.position) then
	  FastSumVectors(spawnParams.position, spawnParams.position, transformTable.position);	  
	end
	if (transformTable.orientation) then
	  spawnParams.orientation = transformTable.orientation;	  
	end 
		
	if (propertiesTable) then
		spawnParams.properties = new(propertiesTable);
	else
		spawnParams.properties = nil;
	end
	
	local spawnedEntity = System.SpawnEntity(spawnParams);
	if (spawnedEntity ~= nil) then
	
	 	if (transformTable.scale) then
	  	spawnedEntity:SetScale(transformTable.scale);	  	
		end 
	  
	  spawnedEntity:EnablePhysics( 0 );
	  spawnedEntity:SetFlags(ENTITY_FLAG_RECVSHADOW, 0);
		spawnedEntity:SetFlags(ENTITY_FLAG_CASTSHADOW, 0);			 
	  
	  if (spawnedEntity.AIDriver) then
	    spawnedEntity:AIDriver( 0 );
	  end
	  		
		local physProps = 
		{	
			Physics = 
			{				
				mass = 100,
				bResting = 1, 
				bVisible = 1, 
				bRigidBodyActive = 0,
				damping = 0.1,
			},
			Simulation = 
			{
				max_time_step = 0.04,
				min_energy = 0.0016,
				max_logged_collisions = 1,
			},			
			Flags = 
			{				  
			  --flags_mask = 8,
				flags_collider_mask = 8,
			},
		};			
		--spawnedEntity:Physicalize(-1, PE_RIGID, physProps.Physics);
		--self:SetPhysicParams(PHYSICPARAM_SIMULATION, physProps.Simulation);
		--self:SetPhysicParams(PHYSICPARAM_PART_FLAGS, physProps.Flags);
		--spawnedEntity:EnablePhysics( 1 );
		
		self:AttachChild(spawnedEntity.id, 0);		
								
		self.State.Carriage[count(self.State.Carriage)+1] = 
		{
		  id = spawnedEntity.id, -- todo: correct index
		  object = propertiesTable.object_Model,
		  position = spawnParams.position,
		  orientation = spawnParams.orientation,		  
		  useText = transformTable.useText,
		};		
		
	else
		Log("Couldn't spawn the child entity attachment");
	end

	return spawnedEntity
end    

--------------------------------------------------------------------------
function VehicleBase:DestroyCarriage()
  if (self.State.Carriage) then
    for i,cargo in pairs(self.State.Carriage) do
    	if (cargo.id) then        		
  			Entity.DetachThis(cargo.id, 0);
  			System.RemoveEntity(cargo.id);	
  		end
  	end	
  end
end


--------------------------------------------------------------------------
function VehicleBase:DestroyVehicleBase()	
	--Log("DestroyVehicleBase()");

	--self:DestroyMountedWeapons();	
	self:DestroyCarriage();
	
	if (self.DestroyAI) then
	  self:DestroyAI();
	end		
end

--------------------------------------------------------------------------
function VehicleBase:GetExitPos(seatId)
	if (self.Seats[seatId] == nil) then
		Log("VehicleBase:GetExitPos(seatId) - Invalid seat id: "..tostring(seatId))
		return;
	end 

	local exitPos;
	local seat = self.Seats[seatId];
	
	if (seat.exitHelper) then
		exitPos = self.vehicle:MultiplyWithWorldTM(self:GetVehicleHelperPos(self.Seats[seatId].exitHelper));
	else
		exitPos = self.vehicle:MultiplyWithWorldTM(self:GetVehicleHelperPos(self.Seats[seatId].enterHelper));
	end
	
	--exitPos.z = exitPos.z + 1.0;
	return exitPos;
end

--------------------------------------------------------------------------
function VehicleBase:GetSeatPos(seatId)
	if (seatId == -1) then
		Log("Error: VehicleBase:GetSeatPos(seatId) - seatId -1 is invalid");
		return {x=0, y=0, z=0,};
	else
		
		local helper = self.Seats[seatId].enterHelper;
		local pos;
		if (self.vehicle:HasHelper(helper)) then		  
		  pos = self.vehicle:GetHelperWorldPos(helper);
		else		  
		  pos = self:GetHelperPos(helper, HELPER_WORLD);		  
		end
		
		--Log("GetSeatPos: "..helper..": "..Vec2Str(pos));
	  
		return pos;
	end
end

--------------------------------------------------------------------------
function VehicleBase:OnCargoUsed(playerId, idx)    
  local cargo = self.State.Carriage[idx];
  if (cargo) then
    local ent = System.GetEntity(cargo.id);
    if (ent) then
      if (ent.OnCargoUsed) then
        ent:OnCargoUsed(playerId);
      end           
      Entity.DetachThis(cargo.id, 0);
      System.RemoveEntity(cargo.id);	
    end
  end
end

--------------------------------------------------------------------------
function VehicleBase:OnUsed(user, idx)
	if (not CryAction.IsClient()) then
		return;
	end
	
	
	if (idx <= 100) then
		
	elseif (idx <= 300) then
--		local seatId = idx - 200;
--		self.server:RequestEnter(user.id, seatId);
  elseif (idx <= 400) then
    self:OnCargoUsed(user.id, idx - 300);  
	end
	
	self.vehicle:OnUsed(user.id, idx);
end

--------------------------------------------------------------------------
function VehicleBase:IsUsable(user)	  
	if (user:IsOnVehicle()) then
		return 0;
	end
		
	self.onUsablePos = user:GetWorldPos( self.onUsablePos );
	local pos = self.onUsablePos
	local radiusSq = 4;
	
	-- check cargo 
	if (self.State.Carriage) then 		  
	  for i,cargo in pairs(self.State.Carriage) do
      local ent = System.GetEntity(cargo.id);
      if (ent) then
        local distSq = DistanceSqVectors(pos, ent:GetWorldPos()); 
        if (distSq < 2.5) then
          return 300 + i;
        end
      end
    end
  end
  
  local ret = self.vehicle:IsUsable(user.id);
  if (ret ~= 0) then
  	return ret;
  end

	return 0
end


----------------------------------------------------------------------------------------------------
function VehicleBase:CanEnter(userId)
	if (g_gameRules and g_gameRules.CanEnterVehicle) then
		return g_gameRules:CanEnterVehicle(self, userId);
	end
end

--------------------------------------------------------------------------
function VehicleBase:GetSeat(userId)
	for i,seat in pairs(self.Seats) do
		if (seat:GetPassengerId() == userId) then
			return seat;
		end
	end
	
	return nil;
end

--------------------------------------------------------------------------
function VehicleBase:GetSeatByIndex(i)
	return self.Seats[i];		
end

--------------------------------------------------------------------------
function VehicleBase:GetSeatId(userId)	
	for i,seat in pairs(self.Seats) do
		if (seat:GetPassengerId() == userId) then
			return i;
		end
	end
	
	return nil;
end


--------------------------------------------------------------------------
function VehicleBase:ResetVehicleBase()
	self.State.pos = self:GetWorldPos(self.State.pos);
	
	-- disable the AI driver.
	if (self.AIDriver) then
		self:AIDriver( 0 );
	end
	
	self.State.aiDriver = nil;
	 
  self:OnPropertyChange();
  
  if (self.Seats) then
  	for i,seat in pairs(self.Seats) do
  		seat:OnReset();
  	end
  end
  
  if (self.ResetAI) then
    self:ResetAI();
  end
  
  if(self.ClearLastDriverId) then
		self:ClearLastDriverId();
	end
end
	

--------------------------------------------------------------------------
function VehicleBase:OnShutDown()	

end

--------------------------------------------------------------------------
function VehicleBase:OnDestroy()	
	self:DestroyVehicleBase();	
end

----------------------------------------------------------------------------------------------------
function VehicleBase:UseCustomFiring(weaponId)
	return false;
end

----------------------------------------------------------------------------------------------------
function VehicleBase:GetFiringPos(weaponId)
	return g_Vectors.v000;
end

--------------------------------------------------------------------------
function VehicleBase:GetFiringVelocity()
	return g_Vectors.v000;
end

--------------------------------------------------------------------------
function VehicleBase:OnAIShoot()	
	local weaponId = self.Seats[1]:GetWeaponId(1);
	
	if (weaponId) then
		local weapon = System.GetEntity(weaponId);

		weapon:StartFire(self.id);
		weapon:StopFire();
	end
end

--------------------------------------------------------------------------
function VehicleBase:IsGunner(userId)
	local seat = self:GetSeat(userId);
	
	if (seat) then
		if (seat.Weapons) then
			return true;
		end
	end
	
	return false;
end

--------------------------------------------------------------------------
function VehicleBase:IsDriver(userId)
	local seat = self:GetSeat(userId);
	
	if (seat) then
		if (seat.isDriver) then
			return true;
		end
	end

	return false;
end


--------------------------------------------------------------------------
function VehicleBase:GetVehicleHelperPos(helperName)
  if (not helperName) then
    helperName = "";
  end
  
  --Log("GetVehicleHelperPos <"..helperName..">");
  
	local pos;
	if (self.vehicle:HasHelper(helperName)) then
	  pos = self.vehicle:GetHelperPos(helperName, true);
	  --Log("pos from vehicle.."..Vec2Str(pos));
	else
	  pos = self:GetHelperPos(helperName, HELPER_LOCAL);
	  --Log("pos from entity.."..Vec2Str(pos));
	end

	return pos;
end

--------------------------------------------------------------------------
-- CryMP:
function VehicleBase:IsSeatFree(seat, player)
	if g_gameRules.CanEnterSeat then
		local free = seat:IsFree()
		if free then
			return g_gameRules:CanEnterSeat(self, seat, player, false)
		else
			return false
		end
	else
		return seat:IsFree()
	end
end

--------------------------------------------------------------------------
function VehicleBase:RequestSeatByPosition(userId)
	local actor = System.GetEntity(userId)
	local pos = actor:GetWorldPos();
	local radiusSq = 10;

	for i,seat in pairs(self.Seats) do
		-- Look if the seat is free

		if (seat.enterHelper and not seat.passengerId) and self:IsSeatFree(seat, actor) then
			-- Look if there's a seat entering pos near the player
			
			if (seat.useBoundsForEntering == nil or seat.useBoundsForEntering == true) then
				if (self.vehicle:IsInsideRadius(pos, 1)) then
					return i;
				end
			else			  
			  local enterPos;
				if (self.vehicle:HasHelper(seat.enterHelper)) then
				  enterPos = self.vehicle:GetHelperWorldPos(seat.enterHelper);
				else
				  enterPos = self:GetHelperPos(seat.enterHelper, HELPER_WORLD);
				end
				
				local distanceSq = DistanceSqVectors(pos, enterPos);

				if (distanceSq <= radiusSq) then
					-- This seat is within the radius, take it!
					return i;
				end
			end
		end
	end
	
	return nil;
end

--------------------------------------------------------------------------
function VehicleBase:RequestClosestSeat(userId)	
	local actor = System.GetEntity(userId)
	local pos = actor:GetWorldPos();
	
	local minDistanceSq = 100000;
	local selectedSeat;
	
	for i,seat in pairs(self.Seats) do
		-- Look if the seat is free

		if (seat.enterHelper and self:IsSeatFree(seat, actor)) then
			local enterPos; 
			if (self.vehicle:HasHelper(seat.enterHelper)) then
				enterPos = self.vehicle:GetHelperWorldPos(seat.enterHelper);
			else
				enterPos = self:GetHelperPos(seat.enterHelper, HELPER_WORLD);
			end
			
			--Log("Helper for seat "..tostring(i)..": "..seat.enterHelper..Vec2Str(enterPos));
			
			local distanceSq = DistanceSqVectors(pos, enterPos);
			
			if (distanceSq <= minDistanceSq) then
				minDistanceSq = distanceSq;
				selectedSeat = i;
			end
		end
	end
  
	if(selectedSeat) then
		if AI then AI.LogEvent(System.GetEntity(userId):GetName().." found seat "..selectedSeat) end;
	else
		if AI then AI.LogEvent(System.GetEntity(userId):GetName().." found no seat") end;
	end
	return selectedSeat;
end

--------------------------------------------------------------------------
function VehicleBase:RequestMostPrioritarySeat(userId)	
	local actor = System.GetEntity(userId)
	local pos = actor:GetWorldPos();	
	local selectedSeat;
	-- search driver seat first
	local seat = self.Seats[1];
	if self:IsSeatFree(seat, actor) then 
		return 1;
	end

	for i,seat in pairs(self.Seats) do
		-- search for gunner seats
		if (seat.enterHelper and seat.Weapons and self:IsSeatFree(seat, actor)) then
			if AI then AI.LogEvent(System.GetEntity(userId):GetName().." found seat "..i) end;
			return i;
		end
	end

	for i,seat in pairs(self.Seats) do
		-- search for remaining seats
		if (seat.enterHelper and self:IsSeatFree(seat, actor)) then
			if AI then AI.LogEvent(System.GetEntity(userId):GetName().." found seat "..i) end;
			return i;
		end
	end


	return ;
end

--------------------------------------------------------------------------
function VehicleBase:RequestSeat(userId)
	local actor = System.GetEntity(userId)
	local pos = actor:GetWorldPos();
	local radiusSq = 6;

	for i,seat in pairs(self.Seats) do
		if (self:IsSeatFree(seat, actor)) then
			return i;
		end
	end

	return nil;
end

--------------------------------------------------------------------------
function VehicleBase:EnterVehicle(passengerId, seatId, isAnimationEnabled)
	--Log("VehicleBase:EnterVehicle() - playerId = %s, seatId = %s", tostring(passengerId), tostring(seatId));
	return self.vehicle:EnterVehicle(passengerId, seatId, isAnimationEnabled);
end

--------------------------------------------------------------------------
function VehicleBase:LeaveVehicle(passengerId, fastLeave)
	--Log("VehicleBase:LeaveVehicle() - "..tostring(passengerId));
	if AI then AI.Signal(SIGNALFILTER_SENDER,0,"exited_vehicle",passengerId) end;
	return self.vehicle:ExitVehicle(passengerId);
end

--------------------------------------------------------------------------
function VehicleBase:MotionTrackable()
	return 1;
end

--------------------------------------------------------------------------
function VehicleBase:UpdateRadar(radarContact)
	if (not self.vehicle:IsDestroyed()) then
		radarContact.img = "textures/gui/hud/radar/vehicle_grey.dds";
		radarContact.radius = 4;
		radarContact.color[1] = 26;
		radarContact.color[2] = 255;
		radarContact.color[3] = 26;
		
		if (type(self:GetDriverId()) == "userdata") then		
			local driver = System.GetEntity(self:GetDriverId());
			
			if (driver and driver.actor) then				
				if (driver.actor:IsPlayer()) then
					return false;
				end			

				local alertness = driver.Behaviour.alertness;
					
				if (g_localActor) then
					local targetName = nil
					if AI then targetName = AI.GetAttentionTargetOf(self.id) end;
	
					if (targetName and targetName == g_localActor:GetName()) then
						alertness = 2;
	         	radarContact.blinking = 1.0;
						radarContact.blinkColor[1] = 255;
						radarContact.blinkColor[2] = 255;
						radarContact.blinkColor[3] = 255;
	        end
				end
					
				--idle
				if (not alertness or alertness == 0) then
					radarContact.color[1] = 26;
					radarContact.color[2] = 255;
					radarContact.color[3] = 26;
				--alerted
				elseif (alertness == 1) then
					radarContact.color[1] = 255;
					radarContact.color[2] = 128;
					radarContact.color[3] = 26;
				--combat
				else
					radarContact.color[1] = 255;
					radarContact.color[2] = 26;
					radarContact.color[3] = 26;
				end
			end
		end
			
		return true;
	else	
		return false;
	end
end

--------------------------------------------------------------------------
function VehicleBase:MotionTrackable()
	return (not self.vehicle:IsDestroyed());
end

--------------------------------------------------------------------------
function VehicleBase:ReserveSeat(userId,seatidx)
	self.Seats[seatidx].passengerId = userId;
end

--------------------------------------------------------------------------
function VehicleBase:IsDead()
  return self.vehicle:IsDestroyed();
end

--------------------------------------------------------------------------
function VehicleBase:GetWeaponVelocity(weaponId)
	return self:GetFiringVelocity();
end

--------------------------------------------------------------------------
function VehicleBase:OnShoot(weapon, remote)
	if (weapon.userId) then
		local seat = self:GetSeat(weapon.userId);
		if (seat) then
			if (seat.Animations and seat.Animations["weaponRecoil"]) then
				local user = System.GetEntity(weapon.userId);
				
				if (user:IsDead()) then
					return;
				end

				user:StartAnimation(0, seat.Animations["weaponRecoil"], 0, 0.000000001, 1.0, false);
			end
		end
	end
	
	return true;
end

--------------------------------------------------------------------------
function VehicleBase:SpawnVehicleBase()
  
  if (self.OnPreSpawn) then
    self:OnPreSpawn();
  end
    
  if (_G[self.class.."Properties"]) then
    mergef(self, _G[self.class.."Properties"], 1);  
  end
  
  if (self.OnPreInit) then
    self:OnPreInit();
  end  

	self:InitVehicleBase();
			
	self.ProcessMovement = nil;
	
	if (not EmptyString(self.Properties.FrozenModel)) then
	  self.frozenModelSlot = self:LoadObject(-1, self.Properties.FrozenModel);
	  self:DrawSlot(self.frozenModelSlot, 0);
	end
		
	if (self.OnPostSpawn) then
	    self:OnPostSpawn();
	end
	
	local aiSpeed = self.Properties.aiSpeedMult;
	local AIProps = self.AIMovementAbility;
	if (AIProps and aiSpeed and aiSpeed ~= 1.0) then	  
	  if (AIProps.walkSpeed) then AIProps.walkSpeed = AIProps.walkSpeed * aiSpeed; end;
	  if (AIProps.runSpeed) then AIProps.runSpeed = AIProps.runSpeed * aiSpeed; end;
	  if (AIProps.sprintSpeed) then AIProps.sprintSpeed = AIProps.sprintSpeed * aiSpeed; end;
	  if (AIProps.maneuverSpeed) then AIProps.maneuverSpeed = AIProps.maneuverSpeed * aiSpeed; end;
	end

  if (self.InitAI) then
    self:InitAI();
  end
	self:InitSeats();
	self:OnReset();
end

--------------------------------------------------------------------------
function VehicleBase:LoadXML()
  
  -- this loads the entity table from xml
        
  local dataTable = VehicleSystem.LoadXML(self.class);
  
  if (dataTable) then      
    -- only seats needed
    if (dataTable.Seats) then
      self.Seats = new(dataTable.Seats);    
    end
  else
    Log("[VehicleBase:LoadXML] dataTable nil!");
    return false;
  end    
  
  return true;  
end


----------------------------------------------------------------------------------------------------
function VehicleBase:GetFrozenSlot()
	return self.frozenModelSlot;
end

----------------------------------------------------------------------------------------------------
function VehicleBase:GetFrozenAmount()
	return self.vehicle:GetFrozenAmount();
end

--------------------------------------------------------------------------
function VehicleBase.Server:OnHit(hit)
    
  local explosion = hit.explosion or false;    
  local targetId = (explosion and hit.impact) and hit.impact_targetId or hit.targetId;
  local hitType = (explosion and hit.type == "") and "explosion" or hit.type;
  local direction = hit.dir;
  
	if(hit.type ~= "fire") then
		if(self.class ~= "US_vtol" and self.class ~= "Asian_helicopter") then
			g_gameRules.game:SendHitIndicator(hit.shooterId, hit.explosion or false);
		end
	end
	
	if(hit.type == "collision") then
		direction.x = -direction.x;
		direction.y = -direction.y;
		direction.z = -direction.z;
	end
	
	-- SP only
	if(g_localActorId and self:GetSeat(g_localActorId)) then
		HUD.DamageIndicator(hit.weaponId, hit.shooterId, direction, true);
	end
  	
	self.vehicle:OnHit(targetId, hit.shooterId, hit.damage, hit.pos, hit.radius, hitType, explosion);

	-- added 3 signals 20/12/05 tetsuji

	if (AI and hit.type ~= "collision") then
		if (hit.shooter) then
			g_SignalData.id = hit.shooterId;
		else
			g_SignalData.id = NULL_ENTITY;
		end	
		g_SignalData.fValue = hit.damage;		
		if (hit.shooter and self.Properties.species ~= hit.shooter.Properties.species) then
		  CopyVector(g_SignalData.point, hit.shooter:GetWorldPos());
			AI.Signal(SIGNALFILTER_SENDER,0,"OnEnemyDamage",self.id,g_SignalData);
		elseif (self.Behaviour and self.Behaviour.OnFriendlyDamage ~= nil) then
			AI.Signal(SIGNALFILTER_SENDER,0,"OnFriendlyDamage",self.id,g_SignalData);
		else
			AI.Signal(SIGNALFILTER_SENDER,0,"OnDamage",self.id,g_SignalData);
		end
	end
	
	return self.vehicle:IsDestroyed();
end

--------------------------------------------------------------------------
function VehicleBase.Client:OnHit(hit)

end

--------------------------------------------------------------------------
function VehicleBase:ProcessPassengerDamage(passengerId, actorHealth, damage, damageType, explosion)
	return self.vehicle:ProcessPassengerDamage(passengerId, actorHealth, damage, damageType, explosion);
end

--------------------------------------------------------------------------
function VehicleBase:OnActorRequestToSit(seatId, passengerId)
	--Log("VehicleBase:OnActorRequestToSit() - seatId=%s, passengerId=%s", tostring(seatId), tostring(passengerId));
end

--------------------------------------------------------------------------
function VehicleBase:OnActorRequestToSitCancelled(seatId, passengerId)
	--Log("VehicleBase:OnActorRequestToSitCancelled() - seatId=%s, passengerId=%s", tostring(seatId), tostring(passengerId));
end

--------------------------------------------------------------------------
function VehicleBase:OnActorSitDown(seatId, passengerId)
	--Log("VehicleBase:OnActorSitDown() seatId=%s, passengerId=%s", tostring(seatId), tostring(passengerId));

	local passenger = System.GetEntity(passengerId);
	if (not passenger) then
		Log("Error: entity for player id <%s> could not be found. %s", tostring(passengerId));
		return;
	end
	
	local seat = self.Seats[seatId];
	if (not seat) then
		Log("Error: entity for player id <%s> could not be found!", tostring(passengerId));
		return;
	end
	
	if (g_gameRules.OnEnterVehicleSeat) then
		g_gameRules:OnEnterVehicleSeat(self, seat, passengerId);
	end

	-- need to generate AI sound event (vehicle engine)
	if(seat.isDriver) then 
		--System.Log(">>> vehicleSoundTimer setting NOW >>>>>>");		
		self:SetTimer(AISOUND_TIMER, AISOUND_TIMEOUT);
	end
	
	seat.passengerId = passengerId;
	passenger.vehicleId = self.id;
	passenger.AI.theVehicle = self; -- fix for behaviors
	if (passenger.ai ) then
		if(seat.isDriver) then 
	  	self.State.aiDriver = 1;
	  	if (passenger.actor and passenger.actor:GetHealth() > 0) then
	  		self:AIDriver(1);
	  	else
	  		self:AIDriver(0);
	  	end
	  end
	else
		--AI.SetSkip(self.id);
		
		if( self.hidesUser == 1 )then
			-- Do not hide outside gunners, so that the AI may use them as targets.
			local isOutsideGunner = false;
			if (seat.Sounds) then
				isOutsideGunner = (seat.seat:GetWeaponCount() > 0) and (seat.Sounds.inout == 1);
			end
			if (AI and not isOutsideGunner) then
				AI.ChangeParameter(passengerId, AIPARAM_INVISIBLE, 1);
			end
		end	
		
		if(AI and seat.isDriver) then
			-- squadmates enter vehicle only if player is the driver
			CopyVector(g_SignalData.point, g_Vectors.v000);
			CopyVector(g_SignalData.point2, g_Vectors.v000);
			g_SignalData.iValue = AIUSEOP_VEHICLE;
			g_SignalData.iValue2 = 1; -- leader has already entered
			g_SignalData.fValue = 1; -- Leader is the driver
			g_SignalData.id = seat.vehicleId;
			AI.Signal(SIGNALFILTER_LEADER, 1, "ORD_USE", passengerId, g_SignalData);
			
		end
		self:EnableMountedWeapons(false);
	end

	-- set vehicle species to driver's 	
	if (seat.isDriver and passenger.Properties and passenger.Properties.species and self.ChangeSpecies) then 
		self:ChangeSpecies(passenger, 1);
		--System.Log("Changing species to "..passenger.Properties.species);
--		AI.ChangeParameter(self.id, AIPARAM_SPECIES, passenger.Properties.species);
	else
	end
	
	local wc = seat.seat:GetWeaponCount();
	--Log("VehicleBase:OnActorSitDown() weapons=%s", tostring(wc));

	if AI then
		if ( seat.seat:GetWeaponCount() > 0) then
			if (seat.isDriver) then
				AI.Signal(SIGNALFILTER_SENDER, 1, "entered_vehicle", passengerId);
			else
				AI.Signal(SIGNALFILTER_SENDER, 1, "entered_vehicle_gunner", passengerId);
			end
		else
			AI.Signal(SIGNALFILTER_SENDER, 1, "entered_vehicle", passengerId);
		end

		-- notify the "wait" goalop
		AI.Signal(SIGNALFILTER_SENDER, 9, "ENTERING_END", passengerId); -- 9 is to skip normal processing of signal
	end

	if g_gameRules.CanEnterSeat then
		local result = g_gameRules:CanEnterSeat(self, seat, passenger, true)
		if result == false then
			Script.SetTimer(20, function()
				self:LeaveVehicle(passengerId, true)
			end)
		elseif type(result) == "number" then
			Script.SetTimer(20, function()
				self:EnterVehicle(passengerId, result, false)
			end)
		end
	end
end
--------------------------------------------------------------------------
function VehicleBase:OnActorChangeSeat(passengerId, exiting)

	-- ai specific
	Log("ai changed a seat");
	local seat = self:GetSeat(passengerId);
	if (not seat) then
		Log("Error: VehicleBase:OnActorChangeSeat() could not find passenger id %s on the vehicle", tostring(passengerId));
		return;
	end

	seat.passengerId = nil;

	if (g_gameRules and g_gameRules.OnLeaveVehicleSeat) then
		g_gameRules:OnLeaveVehicleSeat(self, seat, passengerId, exiting);
	end

	local passenger = System.GetEntity(passengerId); --CryMP: was missing
	if (not passenger) then
		return;
	end
	
	passenger.vehicleId = nil;
	passenger.vehicleSeatId = nil;
	
	if (passenger.ai and passenger:IsDead()) then
		return;
	end

	if (seat.isDriver) then
	 	self.State.aiDriver = nil;
		if (passenger.ai and exiting) then
			self:AIDriver(0);
	  end
  end

	BroadcastEvent(self, "PassengerExit");

end

--------------------------------------------------------------------------
function VehicleBase:OnActorStandUp(passengerId, exiting)
	--Log("VehicleBase:OnActorStandUp() - passengerId=%s", tostring(passengerId));
	
	local seat = self:GetSeat(passengerId);
	if (not seat) then
		Log("Error: VehicleBase:OnActorStandUp() could not find passenger id %s on the vehicle", tostring(passengerId));
		return;
	end
	
	seat.passengerId = nil;
	
	if (g_gameRules and g_gameRules.OnLeaveVehicleSeat) then
		g_gameRules:OnLeaveVehicleSeat(self, seat, passengerId, exiting);
	end
	
	local passenger = System.GetEntity(passengerId);
	if (not passenger) then
		return;
	end
	
	passenger.vehicleId = nil;
	passenger.vehicleSeatId = nil;
	
	if (passenger.ai and passenger:IsDead()) then
		return;
	end

	if (seat.isDriver) then
	 	self.State.aiDriver = nil;
		if (passenger.ai and exiting) then
			self:AIDriver(0);
	  end
  end

	if (passenger.ai ~= 1 and exiting) then
		-- player is exiting vehicle, tell squadmates
		if (self.ChangeSpecies) then 
		  self:ChangeSpecies();	-- restore species
		end
		if AI then
			AI.Signal(SIGNALFILTER_LEADERENTITY, 0, "ORD_LEAVE_VEHICLE", passengerId);
			AI.Signal(SIGNALFILTER_GROUPONLY, 0, "ORDER_EXIT_VEHICLE", passengerId);
			passenger.AI.theVehicle = nil;
		end
		
		-- for MP record the last player driver, and the time they exited
		if(seat.isDriver and g_gameRules and g_gameRules:IsMultiplayer()) then
			self.lastDriverId = passengerId;
			self:SetTimer(PLAYEREXIT_TIMER, PLAYEREXIT_TIMEOUT);
		end
	end	
	

	if (self.AI.unloadCount) then
		self.AI.unloadCount = self.AI.unloadCount - 1;
	end;
	if (self.AI.unloadCount == 0) then
		AI.Signal(SIGNALFILTER_SENDER, 9, "UNLOAD_DONE", self.id); -- 9 is to skip normal processing of signal, only notify the waitsignal goal op.
	end

	if (passenger.AI.theVehicle~=nil) then
		if AI then AI.Signal( SIGNALFILTER_SENDER, 0, "EXIT_VEHICLE_STAND", passenger.id) end;
	end
			
	if(not passenger.ai) then
		if AI then AI.ChangeParameter(passengerId, AIPARAM_INVISIBLE, 0) end;
--		AI.SetSkip( );
		--Log( ">>>> HUMAN out " );
		self:EnableMountedWeapons(true);	
	end

	-- notify the "wait" goalop
	if AI then AI.Signal(SIGNALFILTER_SENDER, 9, "EXITING_END", passengerId) end; -- 9 is to skip normal processing of signal

	BroadcastEvent(self, "PassengerExit");

end

--------------------------------------------------------------------------
function VehicleBase:EnableMountedWeapons(enable)
 
	if not AI then return end

	-- disable/enable all the smartobject interactions for mounted weapons
	for i,seat in pairs(self.Seats) do
		local wc = seat.seat:GetWeaponCount();
		for j=1, wc do			
			local weaponid= seat.seat:GetWeaponId(j);
			if(weaponid) then 
				if(enable) then 
					AI.SetSmartObjectState(weaponid, "Idle");
				else
					AI.SetSmartObjectState(weaponid, "Busy");
				end
			end
		end
	end  
end


--------------------------------------------------------------------------
function VehicleBase:GetCollisionDamageThreshold()    
  return self.vehicle:GetCollisionDamageThreshold();
end

--------------------------------------------------------------------------
function VehicleBase:GetSelfCollisionMult(collider, hit)  
  
  local mult = 1;
  
  mult = self.vehicle:GetSelfCollisionMult(hit.velocity, hit.normal, hit.partId or -1, hit.target_id or NULL_ENTITY);
  
  return mult;
end

--------------------------------------------------------------------------
function VehicleBase:GetForeignCollisionMult(entity, hit)  
  
  local mult = 1;
  
  if (entity.vehicle) then 
   
    -- vehicle/vehicle collision
    
    local mass = self:GetMass()    
    local entityMass = entity:GetMass();
    local speedSq = vecLenSq(hit.target_velocity);
        
    if (mass > 1.5*entityMass and speedSq > 0.01) then
        
      -- reduce damage for collisions with large mass ratios, to avoid instant-killing
      local ratio = 1 + (0.35*__min(10, mass/entityMass))*(__min(1, speedSq/sqr(10)));
      mult = 1/ratio;
      
  	  local debug = g_gameRules and g_gameRules.game:DebugCollisionDamage() or 0;
  	  if ((mult < 0.99 and debug > 0) or debug > 2) then
  	    Log("vehicle/vehicle (%s <- %s), coll, mult: %.2f", entity:GetName(), self:GetName(), mult);
  	  end  	 	  
  	end
	  
	elseif (entity.actor) then
	  
	  -- vehicle/actor collision 
	  if (not entity.actor:IsPlayer()) then 
	  
	    -- increase actor vulnerability against vehicles
  		--local speedSq = vecLenSq(hit.target_velocity);
  		--mult = mult + 0.008*0.5*self:GetMass()*speedSq; 
  		
  		-- old AI hack: don't damage AIs by AI driven vehicles. We should remove/revise that
		  local driverId = self:GetDriverId();
			if (driverId) then
				local driver = System.GetEntity(driverId);
				
				if (driver and driver.actor and (not driver.actor:IsPlayer())) then
					mult = 0;
				end
			end  		
  	end		    
  end
  
  return mult;
end
