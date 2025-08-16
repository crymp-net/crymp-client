--------------------------------------------------------------------------
--	Crytek Source File.
-- 	Copyright (C), Crytek Studios, 2001-2004.
--------------------------------------------------------------------------
--	$Id$
--	$DateTime$
--	Description: Asian AAA vehicle implementation
--  
--------------------------------------------------------------------------
--  History:
--  - 03/12/2004   16:35 : Created by Michael Rauh
--
--------------------------------------------------------------------------




Asian_aaa = 
{
	--State = VehicleState,
	Sounds = {},
	
	
	Properties = 
	{	
	  material = "",
	  bDisableEngine = 0,
	  bFrozen = 0,
	  Modification = "",
	  FrozenModel = "",
	  soclasses_SmartObjectClass = "",
	},
	
	turretAngles = {},
	difPos = {},
	
	Client = {},
	Server = {}
}




----------------------------------------------------------------------------------------------------
function Asian_aaa:UseCustomFiring(weaponId)
  Log("UseCustomFiring");  
	return true;
end




--------------------------------------------------------------------------
Asian_aaa.AIProperties = 
{
	-- AI attributes
  AIType = AIOBJECT_CAR,
  AICombatClass = SafeTableGet(AICombatClasses, "AAA"),
  PropertiesInstance = 
  {
    aibehavior_behaviour = "AAAIdle",    
  },
  Properties = 
  {
    aicharacter_character = "AAA",
    Perception =
		{
			FOVPrimary = -1,			-- normal fov
			FOVSecondary = -1,		-- periferial vision fov
			sightrange = 400,
			
			persistence = 1,
		},		
  },
  AIMovementAbility = 
  {
		walkSpeed = 7.0,
		runSpeed = 15.0,
		sprintSpeed = 20.0,
		maneuverSpeed = 5.0,
    minTurnRadius = 1,
    maxTurnRadius = 30, 
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


