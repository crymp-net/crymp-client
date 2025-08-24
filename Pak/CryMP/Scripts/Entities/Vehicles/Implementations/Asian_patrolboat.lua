--------------------------------------------------------------------------
--	Crytek Source File.
-- 	Copyright (C), Crytek Studios, 2001-2005.
--------------------------------------------------------------------------
--	$Id$
--	$DateTime$
--	Description: Asian patrolboat impl (created because AI properties are 
--               still here, shall dissappear soon)
--  
--------------------------------------------------------------------------
--  History:
--  - 09/14/2005   16:35 : Created by Michael Rauh
--
--------------------------------------------------------------------------



Asian_patrolboat = 
{		
}

--------------------------------------------------------------------------
function Asian_patrolboat:Event_SetBrake(params)
	Log("SetBrake is deprecated, use vehicle entity flownode for this");
end
--------------------------------------------------------------------------
function Asian_patrolboat:Event_ReleaseBrake(params)
  Log("ReleaseBrake is deprecated, use vehicle entity flownode for this");
end


Asian_patrolboat.AIProperties = 
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
		runSpeed = 15.0,
		sprintSpeed = 20.0,
		maneuverSpeed = 5.0,
    minTurnRadius = 0,
    maxTurnRadius = 20,
    passRadius = 9.0,
    pathType = AIPATH_BOAT,
		pathLookAhead = 20,
		pathRadius = 10,
		pathSpeedLookAheadPerSpeed = 1.5,
		cornerSlowDown = 0.75,
		pathFindPrediction = 1.0,
    velDecay = 3,
		maneuverTrh = 2,
		resolveStickingInTrace = 0,
		pathRegenIntervalDuringTrace = 15.0,
		avoidanceRadius = 20.0,
  },    
}

Asian_patrolboat.Events =
{
	Inputs =
	{
		ReleaseBreak = { Asian_patrolboat.Event_ReleaseBreak, "bool" },
		SetBreak = { Asian_patrolboat.Event_SetBreak, "bool" },
	},
	Outputs =
	{
		ReleaseBreak = "bool",
		SetBreak = "bool",
	},
}

