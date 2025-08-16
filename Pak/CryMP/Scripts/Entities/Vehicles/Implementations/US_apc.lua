--------------------------------------------------------------------------
--	Crytek Source File.
-- 	Copyright (C), Crytek Studios, 2001-2004.
--------------------------------------------------------------------------
--	$Id$
--	$DateTime$
--	Description: Implementation of US APC
--  
--------------------------------------------------------------------------
--  History:
--  - Created by MichaelR
--
--------------------------------------------------------------------------

US_apc = 
{	
}

--------------------------------------------------------------------------
US_apc.AIProperties = 
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
    minTurnRadius = .2,
    maxTurnRadius = 10, 
    pathType = AIPATH_TANK,
		pathLookAhead = 8,
		pathRadius = 3,
		pathSpeedLookAheadPerSpeed = 1.0,
		cornerSlowDown = 0.75,
		pathFindPrediction = 1.0,
    maneuverTrh = 2.0,
    passRadius = 5.0,
		resolveStickingInTrace = 0,
		pathRegenIntervalDuringTrace = 4.0,
		avoidanceRadius = 10.0,
  },    
}

