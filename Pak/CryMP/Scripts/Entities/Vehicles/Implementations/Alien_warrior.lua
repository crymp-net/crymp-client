--------------------------------------------------------------------------
--	Crytek Source File.
-- 	Copyright (C), Crytek Studios, 2001-2006.
--------------------------------------------------------------------------
--	$Id$
--	$DateTime$
--	Description: Alien Warrior
--  
--------------------------------------------------------------------------
--  History:
--  - 05/16/2006   11:47 : Created by Michael Rauh
--
--------------------------------------------------------------------------



Alien_warrior = 
{
	Properties = 
	{	
		bFrozen = 0,
		Modification = "",
		FrozenModel = "",
		soclasses_SmartObjectClass = "",
	},
		
	Client = {},
	Server = {}
}


--------------------------------------------------------------------------
function Alien_warrior:OnPostSpawn()
	
end


--------------------------------------------------------------------------
function Alien_warrior:Event_EnableZeroG()
	-- spawn gravity sphere
		
	if (self.gravitySphereId) then
		self:Event_DisableZeroG();
	end
	
	local spawnParams = {};
		
	spawnParams.class = "GravitySphere";
	spawnParams.name = self:GetName().."_GravitySphere";		
	spawnParams.position = self.vehicle:GetHelperPos("gravityPos", true);	
	--spawnParams.position = {x=50, y=50, z=10};
	spawnParams.properties = { Radius=9 };

	local sphere = System.SpawnEntity(spawnParams);
	if (sphere) then		
		self:AttachChild(sphere.id, 0);
		self.gravitySphereId = sphere.id;
	else
		Log("Couldn't spawn gravity sphere");
	end		
end

--------------------------------------------------------------------------
function Alien_warrior:Event_DisableZeroG()
	-- remove zero-g sphere
	if (self.gravitySphereId) then
		Entity.DetachThis(self.gravitySphereId, 0);
		System.RemoveEntity(self.gravitySphereId);		
		self.gravitySphereId = nil;
	end	
end

--------------------------------------------------------------------------
function Alien_warrior:Event_Collapsing(sender)	
	Log("Event_Collapsing");
	self:ActivateOutput( "Collapsing", true );
end


--------------------------------------------------------------------------
function Alien_warrior:Event_Collapsed(sender)
	Log("Event_Collapsed");
	self:ActivateOutput( "Collapsed", true );
end

--------------------------------------------------------------------------
function Alien_warrior:OnDestroy()	
	
	self:Event_DisableZeroG();	
	self:DestroyVehicleBase();	
end

--------------------------------------------------------------------------
Alien_warrior.AIProperties = 
{
	-- AI attributes
  AIType = AIOBJECT_CAR,
  PropertiesInstance = 
  {
    aibehavior_behaviour = "WarriorIdle",
    triggerRadius = 90,
  },
  Properties = 
  {
    aicharacter_character = "Warrior",
    attackrange = 400,

    Perception =
		{
			FOVPrimary = -1,			-- normal fov
			FOVSecondary = -1,		-- periferial vision fov
			sightrange = 400,
			persistence = 10,
		},		

  },
  AIMovementAbility = 
  {
  	
  	--[[
    minTurnRadius = 0,
    maxTurnRadius = 10,
    maxSpeed = 5,
    passRadius = 9.0,
    pathType = AIPATH_BOAT,
    pathLookAhead = 20,
    pathRadius = 5,
		pathSpeedLookAheadPerSpeed = 1.5,
    velDecay = 3,
    maneuvrVel = .2,
		maneuvrTrh = .2,
		resolveStickingInTrace = 0,
		pathRegenIntervalDuringTrace = 4.0,
		--]]

   	maxSpeed = 15.0,
		walkSpeed = 7.0,
		runSpeed = 11.0,
		sprintSpeed = 15.0,
		maneuverSpeed = 5.0,
    minTurnRadius = .2,
    maxTurnRadius = 10, 
    pathType = AIPATH_TANK,
		pathLookAhead = 8,
		pathRadius = 1,
		pathSpeedLookAheadPerSpeed = 1.0,
		cornerSlowDown = 0,
    maneuverTrh = 2.0,
    passRadius = 5.0,
		resolveStickingInTrace = 0,
		pathRegenIntervalDuringTrace = 30.0,
		avoidanceRadius = 20.0,
  },    
}

--------------------------------------------------------------------------
Alien_warrior.FlowEvents =
{
	Inputs =
	{
		EnableZeroG = { Alien_warrior.Event_EnableZeroG, "bool" },		
		DisableZeroG = { Alien_warrior.Event_DisableZeroG, "bool" },
	},
	Outputs =
	{
		Collapsing = "bool",
		Collapsed = "bool",
	},
}