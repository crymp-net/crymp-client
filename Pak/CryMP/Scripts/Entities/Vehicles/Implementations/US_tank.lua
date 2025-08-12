--------------------------------------------------------------------------
--	Crytek Source File.
-- 	Copyright (C), Crytek Studios, 2001-2004.
--------------------------------------------------------------------------
--	$Id$
--	$DateTime$
--	Description: Implementation of US tank
--  
--------------------------------------------------------------------------
--  History:
--  - 03/12/2004   16:35 : Created by Mathieu Pinard
--
--------------------------------------------------------------------------

US_tank = 
{	
}


--------------------------------------------------------------------------
function US_tank:GetSpecificFireProperties()
	if (self.AIFireProperties) then
		self.AIFireProperties[1] = {};
		self.AIFireProperties[1].min_distance = 20;
		self.AIFireProperties[1].max_distance = 400;
	end
end

--------------------------------------------------------------------------
US_tank.AIProperties = 
{
	-- AI attributes
  AIType = AIOBJECT_CAR,
  AICombatClass = SafeTableGet(AICombatClasses, "Tank"),  
  PropertiesInstance = 
  {
    aibehavior_behaviour = "TankIdle",
    triggerRadius = 90,
  },
  Properties = 
  {
    aicharacter_character = "Tank",
    
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
		walkSpeed = 7.0,
		runSpeed = 11.0,
		sprintSpeed = 15.0,
		maneuverSpeed = 5.0,
    minTurnRadius = 1.0,
    maxTurnRadius = 10, 
    pathType = AIPATH_TANK,
		pathLookAhead = 10,
		pathRadius = 3,
		pathSpeedLookAheadPerSpeed = 1.0,
		cornerSlowDown = 0.75,
    maneuverTrh = 2.0,
    passRadius = 3.0,
		resolveStickingInTrace = 0,
		pathRegenIntervalDuringTrace = 4.0,
		avoidanceRadius = 10.0,
  },    
}

