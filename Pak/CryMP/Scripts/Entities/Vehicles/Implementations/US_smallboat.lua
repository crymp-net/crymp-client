--------------------------------------------------------------------------
--	Crytek Source File.
-- 	Copyright (C), Crytek Studios, 2001-2004.
--------------------------------------------------------------------------
--	$Id$
--	$DateTime$
--	Description: Navy Seal Insertion RIB Implementation 
--  
--------------------------------------------------------------------------
--  History:
--  - 05/30/2005   16:35 : Created by Michael Rauh
--
--------------------------------------------------------------------------




US_smallboat = 
{	
}

--------------------------------------------------------------------------
function US_smallboat:Event_SetBrake(params)
	Log("SetBrake is deprecated, use vehicle entity flownode for this");
end
--------------------------------------------------------------------------
function US_smallboat:Event_ReleaseBrake(params)
  Log("ReleaseBrake is deprecated, use vehicle entity flownode for this");
end

US_smallboat.AIProperties = 
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
		walkSpeed = 7.0,
		runSpeed = 15.0,
		sprintSpeed = 20.0,
		maneuverSpeed = 5.0,
    minTurnRadius = 5,
    maxTurnRadius = 15,
    passRadius = 8.0,
    pathType = AIPATH_BOAT,
    pathLookAhead = 20,
    pathRadius = 3,
		pathSpeedLookAheadPerSpeed = 3.0,
		cornerSlowDown = 0.75,
		pathFindPrediction = 1.0,
    velDecay = 3,
		maneuverTrh = 2.0,
		resolveStickingInTrace = 0,
		pathRegenIntervalDuringTrace = 10.0,
		avoidanceRadius = 10.0,
  },    
}

US_smallboat.FlowEvents =
{
	Inputs =
	{
		ReleaseBreak = { US_smallboat.Event_ReleaseBreak, "bool" },
		SetBreak = { US_smallboat.Event_SetBreak, "bool" },
	},
	Outputs =
	{
		ReleaseBreak = "bool",
		SetBreak = "bool",
	},
}

