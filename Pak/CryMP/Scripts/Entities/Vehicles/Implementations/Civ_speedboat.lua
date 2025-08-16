--------------------------------------------------------------------------
--	Crytek Source File.
-- 	Copyright (C), Crytek Studios, 2001-2006.
--------------------------------------------------------------------------
--	$Id$
--	$DateTime$
--	Description: Civ_speedboat implementation
--  
--------------------------------------------------------------------------
--  History:
--  - Created by Michael Rauh
--
--------------------------------------------------------------------------




Civ_speedboat = 
{	
}



Civ_speedboat.AIProperties = 
{
	-- AI attributes
  AIType = AIOBJECT_BOAT,
  PropertiesInstance = 
  {
    aibehavior_behaviour = "BoatIdle",
  },
  Properties = 
  {
    aicharacter_character = "Boat",
  },
  AIMovementAbility = 
  {
		walkSpeed = 10.0,
		runSpeed = 20.0,
		sprintSpeed = 30.0,
		maneuverSpeed = 5.0,
    minTurnRadius = 0,
    maxTurnRadius = 25,
    passRadius = 9.0,
    pathType = AIPATH_BOAT,
    pathLookAhead = 20,
    pathRadius = 5,
		pathSpeedLookAheadPerSpeed = 1.5,
		cornerSlowDown = 0.75,
		pathFindPrediction = 1.0,
    velDecay = 3,
		maneuverTrh = 2.0,
		resolveStickingInTrace = 0,
		pathRegenIntervalDuringTrace = 10.0,
		avoidanceRadius = 10.0,
  },    
	hidesUser=1,  
}


