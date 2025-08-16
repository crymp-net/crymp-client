--------------------------------------------------------------------------
--	Crytek Source File.
-- 	Copyright (C), Crytek Studios, 2001-2004.
--------------------------------------------------------------------------
--	$Id$
--	$DateTime$
--	Description: Implementation of an asian troop truck
--  
--------------------------------------------------------------------------
--  History:
--  - 15/11/2004   16:00 : Created by Mathieu Pinard
--
--------------------------------------------------------------------------



Asian_truck = 
{
	Properties = 
	{	
		material = "",
		bFrozen = 0,
		Modification = "",
		FrozenModel = "",
		soclasses_SmartObjectClass = "",
	},

	Client = {},
	Server = {},
	
}


--------------------------------------------------------------------------
Asian_truck.AIProperties = 
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
		sprintSpeed = 20.0,
		maneuverSpeed = 3.0,
		maneuverTrh = 0.0,
    minTurnRadius = 2.5,
    maxTurnRadius = 15,
    pathType = AIPATH_CAR,
		pathLookAhead = 8,
		pathRadius = 1,
		pathSpeedLookAheadPerSpeed = 3,
		cornerSlowDown = 0.75,
		pathFindPrediction = 1.0,
		resolveStickingInTrace = 0,
		pathRegenIntervalDuringTrace = 4.0,
		avoidanceRadius = 10.0,
  },    
 	hidesUser=0,    
}

