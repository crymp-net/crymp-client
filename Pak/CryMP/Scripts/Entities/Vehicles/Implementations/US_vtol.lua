--------------------------------------------------------------------------
--	Crytek Source File.
-- 	Copyright (C), Crytek Studios, 2001-2006.
--------------------------------------------------------------------------
--	$Id$
--	$DateTime$
--	Description: Implementation of the us vtol
--  
--------------------------------------------------------------------------
--  History:
--  - 31/05/2006   : Created by Mathieu Pinard
--
--------------------------------------------------------------------------


--------------------------------------------------------------------------

--------------------------------------------------------------------------
US_vtol =
{

	foreignCollisionDamageMult = 1.0,
	vehicleCollisionDamageMult = 1.0,

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

	Client = {},
	Server = {},
}

	-- vtol never be frozen
function US_vtol:OnPreFreeze(freeze, vapor)
	if (System.IsMultiplayer()) then
		return true;
	elseif (freeze) then
		return false; -- don't allow freezing
	end
end
	

US_vtol.AIProperties = 
{
	-- AI attributes
  AIType = AIOBJECT_HELICOPTER,
  PropertiesInstance = 
  {
    aibehavior_behaviour = "VtolIdle",
  },
  Properties = 
  {
    aicharacter_character = "Vtol",
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
		minFlightHeight = 15.0,
		maxFlightHeight = 300.0,
		pathLookAhead = 40,
		pathSpeedLookAheadPerSpeed = 8.0,
		cornerSlowDown = 1,
		pathFindPrediction = 5.0,
		pathRadius = 10,
    velDecay = 60,
		passRadius = 9.0,		-- the radius used in path finder.     
		resolveStickingInTrace = 0,
		pathRegenIntervalDuringTrace = 0,
		avoidanceRadius = 20.0,
  },    
}

US_vtol.FlowEvents =
{
	Inputs =
	{
		Debug = { US_vtol.Event_Debug, "bool" },
	},
	Outputs =
	{
		Debug = "bool",
	},
}

