/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:	VehicleSystem CVars
-------------------------------------------------------------------------
History:
- 02:01:2007  12:47 : Created by MichaelR 

*************************************************************************/

#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CrySystem/IConsole.h"
#include "CryCommon/CryAction/IActorSystem.h"
#include "VehicleCVars.h"
#include "VehicleSystem.h"
#include "Vehicle.h"


CVehicleCVars* CVehicleCVars::s_pThis = 0;

void OnDebugViewVarChanged(ICVar* pDebugViewVar);
void OnDebugDrawVarChanged(ICVar* pVar);
void OnDriverControlledGunsChanged(ICVar* pVar);
void CmdExitPlayer(IConsoleCmdArgs* pArgs);

// Summary:
//	 Preferred way to register a CVar
#define REGISTER_CVAR(_var,_def_val,_flags,_comment)	(gEnv->pConsole == 0 ? 0 : gEnv->pConsole->Register((#_var), &(_var), (_def_val), (_flags), _comment))
// Summary:
//	 Preferred way to register a string CVar
#define REGISTER_STRING(_name,_def_val,_flags,_comment)	(gEnv->pConsole == 0 ? 0 : gEnv->pConsole->RegisterString(_name,(_def_val), (_flags), _comment))
// Summary:
//	 Preferred way to register a console command
#define REGISTER_COMMAND(_name,_func,_flags,_comment)	(gEnv->pConsole == 0 ? (void)0 : gEnv->pConsole->AddCommand(_name,_func,(_flags), _comment))


//------------------------------------------------------------------------
CVehicleCVars::CVehicleCVars()
{
	assert (s_pThis == 0);
	s_pThis = this;
  
	IConsole *pConsole = gEnv->pConsole;

  // debug draw
  REGISTER_STRING("v_debugVehicle", "", 0, "Vehicle entity name to use for debugging output");    
  REGISTER_CVAR(v_draw_components, 0, VF_DUMPTODISK, "Enables/disables display of components and their damage count")->SetOnChangeCallback(OnDebugDrawVarChanged);
  REGISTER_CVAR(v_draw_helpers, 0, 0, "Enables/disables display of vehicle helpers")->SetOnChangeCallback(OnDebugDrawVarChanged);
  REGISTER_CVAR(v_draw_seats, 0, 0, "Enables/disables display of seat positions")->SetOnChangeCallback(OnDebugDrawVarChanged);
  REGISTER_CVAR(v_draw_tm, 0, 0, "Enables/disables drawing of local part matrices");  
  REGISTER_CVAR(v_draw_passengers, 0, VF_CHEAT, "draw passenger TMs set by VehicleSeat");
  REGISTER_CVAR(v_debugdraw, 0, VF_DUMPTODISK, 
    "Displays vehicle status info on HUD\n"
    "Values:\n"
    "1:  common stuff\n"
    "2:  vehicle particles\n"
    "3:  parts\n"
    "4:  views\n"          
    "6:  parts + partIds\n"
    "7:  parts + transformations and bboxes\n"
    "8:  component damage\n"    
    "10: vehicle editor");
	REGISTER_CVAR(v_debug_mem, 0, 0, "display memory statistic for vehicles");
      
  // dev vars
  REGISTER_CVAR(v_lights, 2, 0, "Controls vehicle lights.\n"
		"0: disable all lights\n"
		"1: enable lights only for local player\n"
		"2: enable all lights");
  REGISTER_CVAR(v_lights_enable_always, 0, VF_CHEAT, "Vehicle lights are always on (debugging)");    
  REGISTER_CVAR(v_autoDisable, 1, VF_CHEAT, "Enables/disables vehicle autodisabling");
  REGISTER_CVAR(v_set_passenger_tm, 1, VF_CHEAT, "enable/disable passenger entity tm update"); 
  REGISTER_CVAR(v_disable_hull, 0, 0, "Disable hull proxies");  
  REGISTER_CVAR(v_treadUpdateTime, 0, 0, "delta time for tread UV update, 0 means always update");    
  REGISTER_CVAR(v_show_all, 0, VF_CHEAT, "");  
  REGISTER_CVAR(v_transitionAnimations, 1, VF_CHEAT, "Enables enter/exit transition animations for vehicles");
  REGISTER_CVAR(v_ragdollPassengers, 0, VF_CHEAT, "Forces vehicle passenger to detach and ragdoll when they die inside of a vehicle");
  REGISTER_CVAR(v_goliathMode, 0, VF_CHEAT, "Makes all vehicles invincible");  
  REGISTER_CVAR(v_debugView, 0, VF_CHEAT|VF_DUMPTODISK, "Activate a 360 degree rotating third person camera instead of the camera usually available on the vehicle class")
		->SetOnChangeCallback(OnDebugViewVarChanged);  
  REGISTER_CVAR(v_debugViewDetach, 0, VF_CHEAT|VF_DUMPTODISK, "Freeze vehicle camera position, (1) fixed rotation, (2) track the vehicle");
  REGISTER_CVAR(v_debugViewAbove, 0, VF_CHEAT|VF_DUMPTODISK, "Debug camera, looking down on vehicle");
  REGISTER_CVAR(v_debugViewAboveH, 10.f, VF_CHEAT|VF_DUMPTODISK, "Debug camera, looking down on vehicle, height ");
  REGISTER_CVAR(v_debugCollisionDamage, 0, VF_CHEAT, "Enable debug output for vehicle collisions");  
  REGISTER_COMMAND("v_tpvDist", "VehicleSystem.SetTpvDistance(%1)", VF_CHEAT, "Set default distance for vehicle thirdperson cam (0 means distance from vehicle class is used)");
  REGISTER_COMMAND("v_tpvHeight", "VehicleSystem.SetTpvHeight(%1)", VF_CHEAT, "Set default height offset for vehicle thirdperson cam\n(0 means height from vehicle class is used, if present)");

  // for tweaking  
  REGISTER_CVAR(v_slipSlopeFront, 0.f, VF_CHEAT, "coefficient for slip friction slope calculation (front wheels)");
  REGISTER_CVAR(v_slipSlopeRear, 0.f, VF_CHEAT, "coefficient for slip friction slope calculation (rear wheels)");  
  REGISTER_CVAR(v_slipFrictionModFront, 0.f, VF_CHEAT, "if non-zero, used as slip friction modifier (front wheels)");
  REGISTER_CVAR(v_slipFrictionModRear, 0.f, VF_CHEAT, "if non-zero, used as slip friction modifier (rear wheels)");
  REGISTER_CVAR(v_enterDirRadius, 7.0f, VF_CHEAT, "Maximum direction radius tolerated to enter with transition animation (AI only)");
  
  REGISTER_COMMAND("v_reload_system", "VehicleSystem.ReloadVehicleSystem()", 0, "Reloads VehicleSystem script");  
  REGISTER_COMMAND("v_exit_player", CmdExitPlayer, VF_CHEAT, "Makes the local player exit his current vehicle.");
  
	REGISTER_CVAR(v_disableEntry, 0, 0, "Don't allow players to enter vehicles");
	
  REGISTER_CVAR(v_vehicle_quality, 4, 0, "Geometry/Physics quality (1-lowspec, 4-highspec)");
  REGISTER_CVAR(v_driverControlledMountedGuns, 1, VF_CHEAT, "Specifies if the driver can control the vehicles mounted gun when driving without gunner.")
		->SetOnChangeCallback(OnDriverControlledGunsChanged);
  REGISTER_CVAR(v_independentMountedGuns, 1, 0, "Whether mounted gunners operate their turret independently from the parent vehicle");

  REGISTER_CVAR(v_debug_flip_over, 0, VF_CHEAT, "flip over the current vehicle that the player is driving");
}

//------------------------------------------------------------------------
CVehicleCVars::~CVehicleCVars()
{
	assert (s_pThis != 0);
	s_pThis = 0;

	IConsole *pConsole = gEnv->pConsole;

	pConsole->RemoveCommand("v_tpvDist");
	pConsole->RemoveCommand("v_tpvHeight");
	pConsole->RemoveCommand("v_reload_system");  
	pConsole->RemoveCommand("v_exit_player");

	pConsole->UnregisterVariable("v_debugVehicle", true);  	
	pConsole->UnregisterVariable("v_draw_components", true);
	pConsole->UnregisterVariable("v_draw_helpers", true);
  pConsole->UnregisterVariable("v_draw_seats", true);
	pConsole->UnregisterVariable("v_draw_tm", true);  
	pConsole->UnregisterVariable("v_draw_passengers", true);
	pConsole->UnregisterVariable("v_debugdraw", true);
	pConsole->UnregisterVariable("v_debug_mem", true);
   
	pConsole->UnregisterVariable("v_lights", true);
	pConsole->UnregisterVariable("v_lights_enable_always", true);    
	pConsole->UnregisterVariable("v_autoDisable", true);
	pConsole->UnregisterVariable("v_set_passenger_tm", true);
	pConsole->UnregisterVariable("v_disable_hull", true);  
	pConsole->UnregisterVariable("v_treadUpdateTime", true);    
	pConsole->UnregisterVariable("v_transitionAnimations", true);
	pConsole->UnregisterVariable("v_ragdollPassengers", true);
	pConsole->UnregisterVariable("v_goliathMode", true);  
	pConsole->UnregisterVariable("v_debugView", true);  
  
	pConsole->UnregisterVariable("v_slipSlopeFront", true);
	pConsole->UnregisterVariable("v_slipSlopeRear", true);  
	pConsole->UnregisterVariable("v_slipFrictionModFront", true);
	pConsole->UnregisterVariable("v_slipFrictionModRear", true);

  pConsole->UnregisterVariable("v_vehicle_quality", true);
  pConsole->UnregisterVariable("v_driverControlledMountedGuns", true);
}

//------------------------------------------------------------------------
void OnDebugViewVarChanged(ICVar* pDebugViewVar)
{ 
  if (IActor* pActor = gEnv->pGame->GetIGameFramework()->GetClientActor())
  {
		if (IVehicle* pVehicle = pActor->GetLinkedVehicle())
		{
			SVehicleEventParams eventParams;
			eventParams.bParam = pDebugViewVar->GetIVal() == 1;

			pVehicle->BroadcastVehicleEvent(eVE_ToggleDebugView, eventParams);
		}
  }
}

//------------------------------------------------------------------------
void OnDebugDrawVarChanged(ICVar* pVar)
{ 
  if (pVar->GetIVal())
  {
    IVehicleSystem* pVehicleSystem = gEnv->pGame->GetIGameFramework()->GetIVehicleSystem();
    
    IVehicleIteratorPtr pIter = pVehicleSystem->CreateVehicleIterator();    
    while (IVehicle* pVehicle = pIter->Next())
    {      
      pVehicle->NeedsUpdate();
    }     
  }  
}

//------------------------------------------------------------------------
void OnDriverControlledGunsChanged(ICVar* pVar)
{
  if (gEnv->bMultiplayer)
    return;

  if (IActor* pActor = gEnv->pGame->GetIGameFramework()->GetClientActor())
  {
		if (IVehicle* pVehicle = pActor->GetLinkedVehicle())    
		{
			SVehicleEventParams params;
			params.bParam = (pVar->GetIVal() != 0);
	      
			pVehicle->BroadcastVehicleEvent(eVE_ToggleDriverControlledGuns, params);
		}
  }
}

//------------------------------------------------------------------------
void CmdExitPlayer(IConsoleCmdArgs* pArgs)
{ 
	if (IActor* pActor = gEnv->pGame->GetIGameFramework()->GetClientActor())
	{
		if (IVehicle* pVehicle = pActor->GetLinkedVehicle())    
			pVehicle->GetGameObject()->InvokeRMI(CVehicle::SvRequestLeave(), CVehicle::RequestLeaveParams(pActor->GetEntityId()), eRMI_ToServer);    
	}
}

