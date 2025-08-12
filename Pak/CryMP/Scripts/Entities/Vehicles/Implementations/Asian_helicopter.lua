--------------------------------------------------------------------------
--	Crytek Source File.
-- 	Copyright (C), Crytek Studios, 2001-2004.
--------------------------------------------------------------------------
--	$Id$
--	$DateTime$
--	Description: Implementation of an asian helicopter
--  
--------------------------------------------------------------------------
--  History:
--  - 03/05/2005   : Created by Mathieu Pinard
--
--------------------------------------------------------------------------


--------------------------------------------------------------------------

--------------------------------------------------------------------------
Asian_helicopter =
{
	
	Properties = 
	{	
		bDisableEngine = 0,
		material = "",
		bFrozen = 0,
		Modification = "",
		FrozenModel = "",
		soclasses_SmartObjectClass = "",
		Perception =
		{
			--how visible am I
			camoScale = 1,
			--movement related parameters
			--VELmultyplier = (velBase + velScale*CurrentVel^2);
			--current priority gets scaled by VELmultyplier
			velBase = 1,
			velScale = .5,
			--fov/angle related
			FOVPrimary = -1,			-- normal fov
			FOVSecondary = -1,		-- periferial vision fov
			--ranges			
			sightrange = 200,
			--how heights of the target affects visibility
			stanceScale = 1,
			persistence = 60,
		},

	},
	
	isWheelRetracted = true,

	Client = {},
	Server = {},
}


--------------------------------------------------------------------------
function Asian_helicopter:Event_Debug( )
	Log(">>>> Asian_helicopter AI character "..self.Properties.aicharacter_character);
end



Asian_helicopter.AIProperties = 
{
	-- AI attributes
  AIType = AIOBJECT_HELICOPTER,
  AICombatClass = SafeTableGet(AICombatClasses, "Heli"),  
  PropertiesInstance = 
  {
    aibehavior_behaviour = "HeliIdle",
  },
  Properties = 
  {
    aicharacter_character = "Heli",
    attackrange = 320,
  },
  AIMovementAbility = 
  {
		walkSpeed = 15.0,
		runSpeed = 25.0,
		sprintSpeed = 40.0,
		maneuverSpeed = 5.0,
    b3DMove = 1,
    attackZoneHeight = 6,
    pathType = AIPATH_HELI,
    minTurnRadius = 1,
    maxTurnRadius = 35,
		optimalFlightHeight = 45.0,
		minFlightHeight = 25.0,
		maxFlightHeight = 300.0,
		pathLookAhead = 40,
		pathSpeedLookAheadPerSpeed = 8.0,
		cornerSlowDown = 1,
		pathFindPrediction = 2.0,
		pathRadius = 10,
    velDecay = 60,
		passRadius = 9.0,		-- the radius used in path finder.     
		resolveStickingInTrace = 0,
		pathRegenIntervalDuringTrace = 0,
		avoidanceRadius = 20.0,
  },    
}

Asian_helicopter.FlowEvents =
{
	Inputs =
	{
		Debug = { Asian_helicopter.Event_Debug, "bool" },
	},
	Outputs =
	{
		Debug = "bool",
	},
}
