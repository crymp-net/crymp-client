--------------------------------------------------------------------------
--	Crytek Source File.
-- 	Copyright (C), Crytek Studios, 2001-2006.
--------------------------------------------------------------------------
--	$Id$
--	$DateTime$
--	Description: Implementation of Civ_car1
--  
--------------------------------------------------------------------------
--  History:11
--  - : Created by Michael Rauh
--
--------------------------------------------------------------------------


--------------------------------------------------------------------------


--------------------------------------------------------------------------
Civ_car1 =
{	
}



--------------------------------------------------------------------------


Civ_car1.AIProperties = 
{
	-- AI attributes
  AIType = AIOBJECT_CAR,
  PropertiesInstance = 
  {
    aibehavior_behaviour = "CarIdle",
  },
  Properties = 
  {
    aicharacter_character = "Car",
  },
  AIMovementAbility = 
  {
		walkSpeed = 7.0,
		runSpeed = 15.0,
		sprintSpeed = 25.0,
		maneuverSpeed = 5.0,
    minTurnRadius = 4,
    maxTurnRadius = 15,    
    pathType = AIPATH_CAR,
		pathLookAhead = 8,
		pathRadius = 1,
		pathSpeedLookAheadPerSpeed = 1.5,
		cornerSlowDown = 0.75,
		pathFindPrediction = 1.0,
    velDecay = 130,
		resolveStickingInTrace = 0,
		pathRegenIntervalDuringTrace = 4.0,
		avoidanceRadius = 10.0,
  },     
	hidesUser=1,
}

