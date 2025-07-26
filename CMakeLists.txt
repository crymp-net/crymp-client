cmake_minimum_required(VERSION 3.15)

################################################################################

project(CryMP LANGUAGES C CXX RC)

################################################################################

if(NOT MSVC)
	message(FATAL_ERROR "MSVC is the only supported compiler!")
endif()

if(NOT CMAKE_MSVC_RUNTIME_LIBRARY)
	# use statically linked MSVC runtime library
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# use /ZI instead of /Zi in Debug and RelWithDebInfo to enable Edit and Continue
foreach(flags
	CMAKE_C_FLAGS_DEBUG
	CMAKE_C_FLAGS_RELWITHDEBINFO
	CMAKE_CXX_FLAGS_DEBUG
	CMAKE_CXX_FLAGS_RELWITHDEBINFO
)
	string(REPLACE /Zi "" ${flags} "${${flags}}")
	set(${flags} "${${flags}} /ZI")
endforeach()

# use /INCREMENTAL in Debug and RelWithDebInfo to enable Edit and Continue
foreach(flags
	CMAKE_EXE_LINKER_FLAGS_DEBUG
	CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO
)
	set(${flags} "${${flags}} /INCREMENTAL")
endforeach()

# C11
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

################################################################################

option(CRYMP_CONSOLE_APP "Enable console window with standard output." OFF)

option(CRYMP_USE_MIMALLOC "Use mimalloc as global allocator." OFF)

option(CRYMP_DEBUG_ALLOCATOR_ENABLED "Enable debug allocator to detect memory errors." OFF)
option(CRYMP_DEBUG_ALLOCATOR_VERBOSE "Log each allocation and deallocation to standard output." OFF)
option(CRYMP_DEBUG_ALLOCATOR_OVERFLOW_INSTEAD_OF_UNDERFLOW "Detect buffer overflows instead of underflows." OFF)
option(CRYMP_DEBUG_ALLOCATOR_CHECK_READS "Check for both invalid reads and writes." OFF)

option(CRYMP_TRACY_ENABLED "Enable Tracy profiler." OFF)

################################################################################

math(EXPR BUILD_BITS "${CMAKE_SIZEOF_VOID_P} * 8")

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Build bits: ${BUILD_BITS}-bit")

################################################################################

find_package(Git REQUIRED)

include(CMake/GenerateVersion.cmake)

add_custom_target(GenerateVersion
	${CMAKE_COMMAND}
		-D GIT_EXECUTABLE=${GIT_EXECUTABLE}
		-D PROJECT_SOURCE_DIR=${PROJECT_SOURCE_DIR}
		-D PROJECT_BINARY_DIR=${PROJECT_BINARY_DIR}
		-P ${PROJECT_SOURCE_DIR}/CMake/GenerateVersion.cmake
	VERBATIM
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
	BYPRODUCTS ${PROJECT_BINARY_DIR}/version.h
)

message(STATUS "Version number: ${CRYMP_VERSION}")
message(STATUS "Version string: ${CRYMP_VERSION_STRING}")

################################################################################

add_library(Base OBJECT
	Code/Cry3DEngine/TimeOfDay.cpp
	Code/Cry3DEngine/TimeOfDay.h
	Code/CryAction/ActionGame.cpp
	Code/CryAction/ActionGame.h
	Code/CryAction/ActionMapManager.cpp
	Code/CryAction/ActionMapManager.h
	Code/CryAction/ActorIterator.cpp
	Code/CryAction/ActorIterator.h
	Code/CryAction/ActorSystem.cpp
	Code/CryAction/ActorSystem.h
	Code/CryAction/AnimationGraphSystem.cpp
	Code/CryAction/AnimationGraphSystem.h
	Code/CryAction/CallbackTimer.cpp
	Code/CryAction/CallbackTimer.h
	Code/CryAction/DebrisMgr.cpp
	Code/CryAction/DebrisMgr.h
	Code/CryAction/DebugHistoryManager.cpp
	Code/CryAction/DebugHistoryManager.h
	Code/CryAction/DevMode.cpp
	Code/CryAction/DevMode.h
	Code/CryAction/DialogSystem.cpp
	Code/CryAction/DialogSystem.h
	Code/CryAction/EffectSystem.cpp
	Code/CryAction/EffectSystem.h
	Code/CryAction/EquipmentManager.cpp
	Code/CryAction/EquipmentManager.h
	Code/CryAction/FlowSystem.cpp
	Code/CryAction/FlowSystem.h
	Code/CryAction/GameChannel.cpp
	Code/CryAction/GameChannel.h
	Code/CryAction/GameClientChannel.cpp
	Code/CryAction/GameClientChannel.h
	Code/CryAction/GameClientNub.cpp
	Code/CryAction/GameClientNub.h
	Code/CryAction/GameContext.cpp
	Code/CryAction/GameContext.h
	Code/CryAction/GameFramework.cpp
	Code/CryAction/GameFramework.h
	Code/CryAction/GameFrameworkCVars.cpp
	Code/CryAction/GameFrameworkCVars.h
	Code/CryAction/GameObjectSystem.cpp
	Code/CryAction/GameObjectSystem.h
	Code/CryAction/GameplayAnalyst.cpp
	Code/CryAction/GameplayAnalyst.h
	Code/CryAction/GameplayRecorder.cpp
	Code/CryAction/GameplayRecorder.h
	Code/CryAction/GameObject.cpp
	Code/CryAction/GameObject.h
	Code/CryAction/GameQueryListener.cpp
	Code/CryAction/GameQueryListener.h
	Code/CryAction/GameRulesSystem.cpp
	Code/CryAction/GameRulesSystem.h
	Code/CryAction/GameSerialize.cpp
	Code/CryAction/GameSerialize.h
	Code/CryAction/GameServerChannel.cpp
	Code/CryAction/GameServerChannel.h
	Code/CryAction/GameServerNub.cpp
	Code/CryAction/GameServerNub.h
	Code/CryAction/GameStatsConfig.cpp
	Code/CryAction/GameStatsConfig.h
	Code/CryAction/GameTokenSystem.cpp
	Code/CryAction/GameTokenSystem.h
	Code/CryAction/ItemParamsNode.cpp
	Code/CryAction/ItemParamsNode.h
	Code/CryAction/ItemSystem_XMLData.cpp
	Code/CryAction/ItemSystem.cpp
	Code/CryAction/ItemSystem.h
	Code/CryAction/LevelSystem.cpp
	Code/CryAction/LevelSystem.h
	Code/CryAction/MaterialEffects.cpp
	Code/CryAction/MaterialEffects.h
	Code/CryAction/MusicLogic.cpp
	Code/CryAction/MusicLogic.h
	Code/CryAction/NetworkCVars.cpp
	Code/CryAction/NetworkCVars.h
	Code/CryAction/PersistantDebug.cpp
	Code/CryAction/PersistantDebug.h
	Code/CryAction/PlayerProfileManager.cpp
	Code/CryAction/PlayerProfileManager.h
	Code/CryAction/ScriptBind_ActionMapManager.cpp
	Code/CryAction/ScriptBind_ActionMapManager.h
	Code/CryAction/ScriptBind_Actor.cpp
	Code/CryAction/ScriptBind_Actor.h
	Code/CryAction/ScriptBind_AI.cpp
	Code/CryAction/ScriptBind_AI.h
	Code/CryAction/ScriptBind_CryAction.cpp
	Code/CryAction/ScriptBind_CryAction.h
	Code/CryAction/ScriptBind_DialogSystem.cpp
	Code/CryAction/ScriptBind_DialogSystem.h
	Code/CryAction/ScriptBind_Inventory.cpp
	Code/CryAction/ScriptBind_Inventory.h
	Code/CryAction/ScriptBind_ItemSystem.cpp
	Code/CryAction/ScriptBind_ItemSystem.h
	Code/CryAction/ScriptBind_MaterialEffects.cpp
	Code/CryAction/ScriptBind_MaterialEffects.h
	Code/CryAction/ScriptBind_Network.cpp
	Code/CryAction/ScriptBind_Network.h
	Code/CryAction/ScriptBind_Vehicle.cpp
	Code/CryAction/ScriptBind_Vehicle.h
	Code/CryAction/ScriptBind_VehicleSeat.cpp
	Code/CryAction/ScriptBind_VehicleSeat.h
	Code/CryAction/ScriptBind_VehicleSystem.cpp
	Code/CryAction/ScriptBind_VehicleSystem.h
	Code/CryAction/ScriptRMI.cpp
	Code/CryAction/ScriptRMI.h
	Code/CryAction/SubtitleManager.cpp
	Code/CryAction/SubtitleManager.h
	Code/CryAction/TimeDemoRecorder.cpp
	Code/CryAction/TimeDemoRecorder.h
	Code/CryAction/TimeOfDayScheduler.cpp
	Code/CryAction/TimeOfDayScheduler.h
	Code/CryAction/UIDraw.cpp
	Code/CryAction/UIDraw.h
	Code/CryAction/Vehicles/Actions/VehicleActionEnter.cpp
	Code/CryAction/Vehicles/Actions/VehicleActionEnter.h
	Code/CryAction/Vehicles/DamageBehaviors/VehicleDamageBehaviorAISignal.cpp
	Code/CryAction/Vehicles/DamageBehaviors/VehicleDamageBehaviorAISignal.h
	Code/CryAction/Vehicles/DamageBehaviors/VehicleDamageBehaviorDestroy.cpp
	Code/CryAction/Vehicles/DamageBehaviors/VehicleDamageBehaviorDestroy.h
	Code/CryAction/Vehicles/DamageBehaviors/VehicleDamageBehaviorDetachPart.cpp
	Code/CryAction/Vehicles/DamageBehaviors/VehicleDamageBehaviorDetachPart.h
	Code/CryAction/Vehicles/DamageBehaviors/VehicleDamageBehaviorEffect.cpp
	Code/CryAction/Vehicles/DamageBehaviors/VehicleDamageBehaviorEffect.h
	Code/CryAction/Vehicles/DamageBehaviors/VehicleDamageBehaviorGroup.cpp
	Code/CryAction/Vehicles/DamageBehaviors/VehicleDamageBehaviorGroup.h
	Code/CryAction/Vehicles/DamageBehaviors/VehicleDamageBehaviorHitPassenger.cpp
	Code/CryAction/Vehicles/DamageBehaviors/VehicleDamageBehaviorHitPassenger.h
	Code/CryAction/Vehicles/DamageBehaviors/VehicleDamageBehaviorImpulse.cpp
	Code/CryAction/Vehicles/DamageBehaviors/VehicleDamageBehaviorImpulse.h
	Code/CryAction/Vehicles/DamageBehaviors/VehicleDamageBehaviorIndicator.cpp
	Code/CryAction/Vehicles/DamageBehaviors/VehicleDamageBehaviorIndicator.h
	Code/CryAction/Vehicles/DamageBehaviors/VehicleDamageBehaviorMovementNotification.cpp
	Code/CryAction/Vehicles/DamageBehaviors/VehicleDamageBehaviorMovementNotification.h
	Code/CryAction/Vehicles/DamageBehaviors/VehicleDamageBehaviorSink.cpp
	Code/CryAction/Vehicles/DamageBehaviors/VehicleDamageBehaviorSink.h
	Code/CryAction/Vehicles/DamageBehaviors/VehicleDamageBehaviorSpawnDebris.cpp
	Code/CryAction/Vehicles/DamageBehaviors/VehicleDamageBehaviorSpawnDebris.h
	Code/CryAction/Vehicles/Implementations/Alien_warrior.cpp
	Code/CryAction/Vehicles/Implementations/Alien_warrior.h
	Code/CryAction/Vehicles/Implementations/Asian_aaa.cpp
	Code/CryAction/Vehicles/Implementations/Asian_aaa.h
	Code/CryAction/Vehicles/Implementations/Asian_apc.cpp
	Code/CryAction/Vehicles/Implementations/Asian_apc.h
	Code/CryAction/Vehicles/Implementations/Asian_helicopter.cpp
	Code/CryAction/Vehicles/Implementations/Asian_helicopter.h
	Code/CryAction/Vehicles/Implementations/Asian_ltv.cpp
	Code/CryAction/Vehicles/Implementations/Asian_ltv.h
	Code/CryAction/Vehicles/Implementations/Asian_patrolboat.cpp
	Code/CryAction/Vehicles/Implementations/Asian_patrolboat.h
	Code/CryAction/Vehicles/Implementations/Asian_tank.cpp
	Code/CryAction/Vehicles/Implementations/Asian_tank.h
	Code/CryAction/Vehicles/Implementations/Asian_truck.cpp
	Code/CryAction/Vehicles/Implementations/Asian_truck.h
	Code/CryAction/Vehicles/Implementations/Civ_car1.cpp
	Code/CryAction/Vehicles/Implementations/Civ_car1.h
	Code/CryAction/Vehicles/Implementations/Civ_speedboat.cpp
	Code/CryAction/Vehicles/Implementations/Civ_speedboat.h
	Code/CryAction/Vehicles/Implementations/DefaultVehicle.cpp
	Code/CryAction/Vehicles/Implementations/DefaultVehicle.h
	Code/CryAction/Vehicles/Implementations/US_apc.cpp
	Code/CryAction/Vehicles/Implementations/US_apc.h
	Code/CryAction/Vehicles/Implementations/US_hovercraft.cpp
	Code/CryAction/Vehicles/Implementations/US_hovercraft.h
	Code/CryAction/Vehicles/Implementations/US_ltv.cpp
	Code/CryAction/Vehicles/Implementations/US_ltv.h
	Code/CryAction/Vehicles/Implementations/US_smallboat.cpp
	Code/CryAction/Vehicles/Implementations/US_smallboat.h
	Code/CryAction/Vehicles/Implementations/US_tank.cpp
	Code/CryAction/Vehicles/Implementations/US_tank.h
	Code/CryAction/Vehicles/Implementations/US_transportVTOL.cpp
	Code/CryAction/Vehicles/Implementations/US_transportVTOL.h
	Code/CryAction/Vehicles/Implementations/US_trolley.cpp
	Code/CryAction/Vehicles/Implementations/US_trolley.h
	Code/CryAction/Vehicles/Implementations/US_vtol.cpp
	Code/CryAction/Vehicles/Implementations/US_vtol.h
	Code/CryAction/Vehicles/Parts/VehiclePartAnimated.cpp
	Code/CryAction/Vehicles/Parts/VehiclePartAnimated.h
	Code/CryAction/Vehicles/Parts/VehiclePartAnimatedJoint.cpp
	Code/CryAction/Vehicles/Parts/VehiclePartAnimatedJoint.h
	Code/CryAction/Vehicles/Parts/VehiclePartBase.cpp
	Code/CryAction/Vehicles/Parts/VehiclePartBase.h
	Code/CryAction/Vehicles/Parts/VehiclePartEntityAttachment.cpp
	Code/CryAction/Vehicles/Parts/VehiclePartEntityAttachment.h
	Code/CryAction/Vehicles/Parts/VehiclePartLight.cpp
	Code/CryAction/Vehicles/Parts/VehiclePartLight.h
	Code/CryAction/Vehicles/Parts/VehiclePartMassBox.cpp
	Code/CryAction/Vehicles/Parts/VehiclePartMassBox.h
	Code/CryAction/Vehicles/Parts/VehiclePartStatic.cpp
	Code/CryAction/Vehicles/Parts/VehiclePartStatic.h
	Code/CryAction/Vehicles/Parts/VehiclePartSubPart.cpp
	Code/CryAction/Vehicles/Parts/VehiclePartSubPart.h
	Code/CryAction/Vehicles/Parts/VehiclePartSubPartWheel.cpp
	Code/CryAction/Vehicles/Parts/VehiclePartSubPartWheel.h
	Code/CryAction/Vehicles/Parts/VehiclePartTread.cpp
	Code/CryAction/Vehicles/Parts/VehiclePartTread.h
	Code/CryAction/Vehicles/SeatActions/VehicleSeatActionAnimation.cpp
	Code/CryAction/Vehicles/SeatActions/VehicleSeatActionAnimation.h
	Code/CryAction/Vehicles/SeatActions/VehicleSeatActionLights.cpp
	Code/CryAction/Vehicles/SeatActions/VehicleSeatActionLights.h
	Code/CryAction/Vehicles/SeatActions/VehicleSeatActionMovement.cpp
	Code/CryAction/Vehicles/SeatActions/VehicleSeatActionMovement.h
	Code/CryAction/Vehicles/SeatActions/VehicleSeatActionPassengerIK.cpp
	Code/CryAction/Vehicles/SeatActions/VehicleSeatActionPassengerIK.h
	Code/CryAction/Vehicles/SeatActions/VehicleSeatActionRotateTurret.cpp
	Code/CryAction/Vehicles/SeatActions/VehicleSeatActionRotateTurret.h
	Code/CryAction/Vehicles/SeatActions/VehicleSeatActionSound.cpp
	Code/CryAction/Vehicles/SeatActions/VehicleSeatActionSound.h
	Code/CryAction/Vehicles/SeatActions/VehicleSeatActionSteeringWheel.cpp
	Code/CryAction/Vehicles/SeatActions/VehicleSeatActionSteeringWheel.h
	Code/CryAction/Vehicles/SeatActions/VehicleSeatActionWeapons.cpp
	Code/CryAction/Vehicles/SeatActions/VehicleSeatActionWeapons.h
	Code/CryAction/Vehicles/Views/VehicleViewActionThirdPerson.cpp
	Code/CryAction/Vehicles/Views/VehicleViewActionThirdPerson.h
	Code/CryAction/Vehicles/Views/VehicleViewFirstPerson.cpp
	Code/CryAction/Vehicles/Views/VehicleViewFirstPerson.h
	Code/CryAction/Vehicles/Views/VehicleViewThirdPerson.cpp
	Code/CryAction/Vehicles/Views/VehicleViewThirdPerson.h
	Code/CryAction/Vehicles/Vehicle.cpp
	Code/CryAction/Vehicles/Vehicle.h
	Code/CryAction/Vehicles/VehicleAnimation.cpp
	Code/CryAction/Vehicles/VehicleAnimation.h
	Code/CryAction/Vehicles/VehicleComponent.cpp
	Code/CryAction/Vehicles/VehicleComponent.h
	Code/CryAction/Vehicles/VehicleCVars.cpp
	Code/CryAction/Vehicles/VehicleCVars.h
	Code/CryAction/Vehicles/VehicleDamagesTemplateRegistry_XMLData.cpp
	Code/CryAction/Vehicles/VehicleDamagesTemplateRegistry.cpp
	Code/CryAction/Vehicles/VehicleDamagesTemplateRegistry.h
	Code/CryAction/Vehicles/VehicleIterator.cpp
	Code/CryAction/Vehicles/VehicleIterator.h
	Code/CryAction/Vehicles/VehiclePartDetached.cpp
	Code/CryAction/Vehicles/VehiclePartDetached.h
	Code/CryAction/Vehicles/VehicleSeat.cpp
	Code/CryAction/Vehicles/VehicleSeat.h
	Code/CryAction/Vehicles/VehicleSeatGroup.cpp
	Code/CryAction/Vehicles/VehicleSeatGroup.h
	Code/CryAction/Vehicles/VehicleSeatSerializer.cpp
	Code/CryAction/Vehicles/VehicleSeatSerializer.h
	Code/CryAction/Vehicles/VehicleSystem.cpp
	Code/CryAction/Vehicles/VehicleSystem.h
	Code/CryAction/ViewSystem.cpp
	Code/CryAction/ViewSystem.h
	Code/CryCommon/Cry3DEngine/CGF/CGFContent.h
	Code/CryCommon/Cry3DEngine/CGF/CryCompiledFile.h
	Code/CryCommon/Cry3DEngine/CGF/CryHeaders.h
	Code/CryCommon/Cry3DEngine/CGF/IChunkFile.h
	Code/CryCommon/Cry3DEngine/CryEngineDecalInfo.h
	Code/CryCommon/Cry3DEngine/CryParticleSpawnInfo.h
	Code/CryCommon/Cry3DEngine/CryVertexBinding.h
	Code/CryCommon/Cry3DEngine/I3DEngine.h
	Code/CryCommon/Cry3DEngine/I3DSampler.h
	Code/CryCommon/Cry3DEngine/IFoliage.h
	Code/CryCommon/Cry3DEngine/IIndexedMesh.h
	Code/CryCommon/Cry3DEngine/ILMSerializationManager.h
	Code/CryCommon/Cry3DEngine/IMaterial.h
	Code/CryCommon/Cry3DEngine/ISplines.h
	Code/CryCommon/Cry3DEngine/IStatObj.h
	Code/CryCommon/Cry3DEngine/ISurfaceType.h
	Code/CryCommon/Cry3DEngine/LMCompStructures.h
	Code/CryCommon/Cry3DEngine/ParticleParams.h
	Code/CryCommon/CryAISystem/AIFormationDescriptor.h
	Code/CryCommon/CryAISystem/AITrackPatternDescriptor.h
	Code/CryCommon/CryAISystem/AgentParams.h
	Code/CryCommon/CryAISystem/IAIAction.h
	Code/CryCommon/CryAISystem/IAIGroup.h
	Code/CryCommon/CryAISystem/IAIRecorder.h
	Code/CryCommon/CryAISystem/IAISystem.h
	Code/CryCommon/CryAISystem/IAgent.h
	Code/CryCommon/CryAISystem/IInterestSystem.h
	Code/CryCommon/CryAction/IActionMapManager.h
	Code/CryCommon/CryAction/IActorSystem.h
	Code/CryCommon/CryAction/IAnimatedCharacter.h
	Code/CryCommon/CryAction/IAnimationGraph.h
	Code/CryCommon/CryAction/IAnimationGraphSystem.h
	Code/CryCommon/CryAction/IAnimationStateNode.h
	Code/CryCommon/CryAction/IDebrisMgr.h
	Code/CryCommon/CryAction/IDebugHistory.h
	Code/CryCommon/CryAction/IDialogSystem.h
	Code/CryCommon/CryAction/IEffectSystem.h
	Code/CryCommon/CryAction/IFlowSystem.h
	Code/CryCommon/CryAction/IGameFramework.h
	Code/CryCommon/CryAction/IGameObject.h
	Code/CryCommon/CryAction/IGameObjectSystem.h
	Code/CryCommon/CryAction/IGameRulesSystem.h
	Code/CryCommon/CryAction/IGameTokens.h
	Code/CryCommon/CryAction/IGameplayRecorder.h
	Code/CryCommon/CryAction/IInteractor.h
	Code/CryCommon/CryAction/IItem.h
	Code/CryCommon/CryAction/IItemSystem.h
	Code/CryCommon/CryAction/ILevelSystem.h
	Code/CryCommon/CryAction/ILoadGame.h
	Code/CryCommon/CryAction/IMaterialEffects.h
	Code/CryCommon/CryAction/IMovementController.h
	Code/CryCommon/CryAction/IPlayerProfiles.h
	Code/CryCommon/CryAction/ISaveGame.h
	Code/CryCommon/CryAction/ISubtitleManager.h
	Code/CryCommon/CryAction/IUIDraw.h
	Code/CryCommon/CryAction/IVehicleSystem.h
	Code/CryCommon/CryAction/IViewSystem.h
	Code/CryCommon/CryAction/IWeapon.h
	Code/CryCommon/CryAction/IWorldQuery.h
	Code/CryCommon/CryAction/Network/NetActionSync.h
	Code/CryCommon/CryAnimation/CryAnimationScriptCommands.h
	Code/CryCommon/CryAnimation/CryCharAnimationParams.h
	Code/CryCommon/CryAnimation/CryCharMorphParams.h
	Code/CryCommon/CryAnimation/ICryAnimation.h
	Code/CryCommon/CryAnimation/IFacialAnimation.h
	Code/CryCommon/CryAnimation/IJoystick.h
	Code/CryCommon/CryCore/ConfigurableVariant.h
	Code/CryCommon/CryCore/CryArray.h
	Code/CryCommon/CryCore/CryFixedString.h
	Code/CryCommon/CryCore/CryMalloc.h
	Code/CryCommon/CryCore/CryName.h
	Code/CryCommon/CryCore/CryPodArray.h
	Code/CryCommon/CryCore/CrySizer.h
	Code/CryCommon/CryCore/CryString.h
	Code/CryCommon/CryCore/Endian.h
	Code/CryCommon/CryCore/MiniQueue.h
	Code/CryCommon/CryCore/MultiThread.h
	Code/CryCommon/CryCore/PoolAllocator.h
	Code/CryCommon/CryCore/PoolAllocatorSynchronization.h
	Code/CryCommon/CryCore/StlUtils.h
	Code/CryCommon/CryCore/StringUtils.h
	Code/CryCommon/CryCore/TArray.h
	Code/CryCommon/CryCore/Typelist.h
	Code/CryCommon/CryCore/TypelistUtils.h
	Code/CryCommon/CryCore/VectorMap.h
	Code/CryCommon/CryCore/VectorSet.h
	Code/CryCommon/CryCore/crc32.h
	Code/CryCommon/CryCore/functor.h
	Code/CryCommon/CryCore/platform.h
	Code/CryCommon/CryCore/smartptr.h
	Code/CryCommon/CryEntitySystem/EntityId.h
	Code/CryCommon/CryEntitySystem/IEntity.h
	Code/CryCommon/CryEntitySystem/IEntityClass.h
	Code/CryCommon/CryEntitySystem/IEntityProxy.h
	Code/CryCommon/CryEntitySystem/IEntityRenderState.h
	Code/CryCommon/CryEntitySystem/IEntitySerialize.h
	Code/CryCommon/CryEntitySystem/IEntitySystem.h
	Code/CryCommon/CryFont/IFont.h
	Code/CryCommon/CryGame/CoherentValue.h
	Code/CryCommon/CryGame/GameUtils.h
	Code/CryCommon/CryGame/IGame.h
	Code/CryCommon/CryGame/IGameRef.h
	Code/CryCommon/CryGame/IGameStartup.h
	Code/CryCommon/CryInput/IInput.h
	Code/CryCommon/CryMath/AABBSV.h
	Code/CryCommon/CryMath/Cry_Camera.h
	Code/CryCommon/CryMath/Cry_Color.h
	Code/CryCommon/CryMath/Cry_Geo.h
	Code/CryCommon/CryMath/Cry_GeoDistance.h
	Code/CryCommon/CryMath/Cry_GeoIntersect.h
	Code/CryCommon/CryMath/Cry_GeoOverlap.h
	Code/CryCommon/CryMath/Cry_Math.h
	Code/CryCommon/CryMath/Cry_Matrix.h
	Code/CryCommon/CryMath/Cry_Quat.h
	Code/CryCommon/CryMath/Cry_ValidNumber.h
	Code/CryCommon/CryMath/Cry_Vector2.h
	Code/CryCommon/CryMath/Cry_Vector3.h
	Code/CryCommon/CryMath/GeomQuery.h
	Code/CryCommon/CryMath/Range.h
	Code/CryCommon/CryMovie/AnimKey.h
	Code/CryCommon/CryMovie/IMovieSystem.h
	Code/CryCommon/CryNetwork/INetwork.h
	Code/CryCommon/CryNetwork/INetworkService.h
	Code/CryCommon/CryNetwork/IRemoteControl.h
	Code/CryCommon/CryNetwork/ISerialize.h
	Code/CryCommon/CryNetwork/ISimpleHttpServer.h
	Code/CryCommon/CryNetwork/NetHelpers.h
	Code/CryCommon/CryNetwork/SerializationTypes.h
	Code/CryCommon/CryNetwork/SerializeFwd.h
	Code/CryCommon/CryPhysics/IPhysics.h
	Code/CryCommon/CryPhysics/physinterface.h
	Code/CryCommon/CryPhysics/primitives.h
	Code/CryCommon/CryRenderer/IRenderAuxGeom.h
	Code/CryCommon/CryRenderer/IRenderMesh.h
	Code/CryCommon/CryRenderer/IRenderer.h
	Code/CryCommon/CryRenderer/IShader.h
	Code/CryCommon/CryRenderer/ISkinnable.h
	Code/CryCommon/CryRenderer/IVideoPlayer.h
	Code/CryCommon/CryRenderer/RendElement.h
	Code/CryCommon/CryRenderer/VertexBufferSource.h
	Code/CryCommon/CryRenderer/VertexFormats.h
	Code/CryCommon/CryScriptSystem/IScriptSystem.h
	Code/CryCommon/CrySoundSystem/IMusicSystem.h
	Code/CryCommon/CrySoundSystem/IReverbManager.h
	Code/CryCommon/CrySoundSystem/ISound.h
	Code/CryCommon/CrySoundSystem/ISoundMoodManager.h
	Code/CryCommon/CrySystem/CryColorCode.h
	Code/CryCommon/CrySystem/CryFile.h
	Code/CryCommon/CrySystem/CryFind.h
	Code/CryCommon/CrySystem/CryPath.h
	Code/CryCommon/CrySystem/CryVersion.h
	Code/CryCommon/CrySystem/FrameProfiler.h
	Code/CryCommon/CrySystem/IAVI_Reader.h
	Code/CryCommon/CrySystem/IBudgetingSystem.h
	Code/CryCommon/CrySystem/ICmdLine.h
	Code/CryCommon/CrySystem/IConsole.h
	Code/CryCommon/CrySystem/ICryPak.h
	Code/CryCommon/CrySystem/IDataProbe.h
	Code/CryCommon/CrySystem/IFlashPlayer.h
	Code/CryCommon/CrySystem/IGlobalTaskScheduler.h
	Code/CryCommon/CrySystem/IHardwareMouse.h
	Code/CryCommon/CrySystem/ILog.h
	Code/CryCommon/CrySystem/IProcess.h
	Code/CryCommon/CrySystem/IReadWriteXMLSink.h
	Code/CryCommon/CrySystem/IResourceCollector.h
	Code/CryCommon/CrySystem/IStreamEngine.h
	Code/CryCommon/CrySystem/ISystem.h
	Code/CryCommon/CrySystem/ITestSystem.h
	Code/CryCommon/CrySystem/ITextModeConsole.h
	Code/CryCommon/CrySystem/IThreadTask.h
	Code/CryCommon/CrySystem/ITimer.h
	Code/CryCommon/CrySystem/IValidator.h
	Code/CryCommon/CrySystem/IXml.h
	Code/CryCommon/CrySystem/TimeValue.h
	Code/CryCommon/CrySystem/gEnv.h
	Code/CryMP/Client/Advertising.cpp
	Code/CryMP/Client/Advertising.h
	Code/CryMP/Client/Client.cpp
	Code/CryMP/Client/Client.h
	Code/CryMP/Client/DrawTools.cpp
	Code/CryMP/Client/DrawTools.h
	Code/CryMP/Client/EngineCache.cpp
	Code/CryMP/Client/EngineCache.h
	Code/CryMP/Client/FFontHooks.cpp
	Code/CryMP/Client/FFontHooks.h
	Code/CryMP/Client/FileCache.cpp
	Code/CryMP/Client/FileCache.h
	Code/CryMP/Client/FileDownloader.cpp
	Code/CryMP/Client/FileDownloader.h
	Code/CryMP/Client/HealthManager.cpp
	Code/CryMP/Client/HealthManager.h
	Code/CryMP/Client/MapDownloader.cpp
	Code/CryMP/Client/MapDownloader.h
	Code/CryMP/Client/MapExtractor.cpp
	Code/CryMP/Client/MapExtractor.h
	Code/CryMP/Client/ParticleManager.cpp
	Code/CryMP/Client/ParticleManager.h
	Code/CryMP/Client/ScriptCallbacks.cpp
	Code/CryMP/Client/ScriptCallbacks.h
	Code/CryMP/Client/ScriptCommands.cpp
	Code/CryMP/Client/ScriptCommands.h
	Code/CryMP/Client/ServerBrowser.cpp
	Code/CryMP/Client/ServerBrowser.h
	Code/CryMP/Client/ServerConnector.cpp
	Code/CryMP/Client/ServerConnector.h
	Code/CryMP/Client/ServerInfo.cpp
	Code/CryMP/Client/ServerInfo.h
	Code/CryMP/Client/ServerPAK.cpp
	Code/CryMP/Client/ServerPAK.h
	Code/CryMP/Client/SpeedAggregator.cpp
	Code/CryMP/Client/SpeedAggregator.h
	Code/CryMP/Client/WeatherSystem.cpp
	Code/CryMP/Client/WeatherSystem.h
	Code/CryMP/Common/Executor.cpp
	Code/CryMP/Common/Executor.h
	Code/CryMP/Common/GSMasterHook.cpp
	Code/CryMP/Common/GSMasterHook.h
	Code/CryMP/Common/HTTP.cpp
	Code/CryMP/Common/HTTP.h
	Code/CryMP/Common/HTTPClient.cpp
	Code/CryMP/Common/HTTPClient.h
	Code/CryMP/Common/ScriptBind_CPPAPI.cpp
	Code/CryMP/Common/ScriptBind_CPPAPI.h
	Code/CryMP/Server/Server.cpp
	Code/CryMP/Server/Server.h
	Code/CryMP/Server/SSM.h
	Code/CryMP/Server/SafeWriting/API.h
	Code/CryMP/Server/SafeWriting/APIImpl.h
	Code/CryMP/Server/SafeWriting/APIImpl.cpp
	Code/CryMP/Server/SafeWriting/SafeWriting.h
	Code/CryMP/Server/SafeWriting/SafeWriting.cpp
	Code/CryScriptSystem/LuaLibs/bitlib.c
	Code/CryScriptSystem/ScriptBindings/ScriptBindings.cpp
	Code/CryScriptSystem/ScriptBindings/ScriptBindings.h
	Code/CryScriptSystem/ScriptBindings/ScriptBind_Movie.cpp
	Code/CryScriptSystem/ScriptBindings/ScriptBind_Movie.h
	Code/CryScriptSystem/ScriptBindings/ScriptBind_Particle.cpp
	Code/CryScriptSystem/ScriptBindings/ScriptBind_Particle.h
	Code/CryScriptSystem/ScriptBindings/ScriptBind_Physics.cpp
	Code/CryScriptSystem/ScriptBindings/ScriptBind_Physics.h
	Code/CryScriptSystem/ScriptBindings/ScriptBind_Script.cpp
	Code/CryScriptSystem/ScriptBindings/ScriptBind_Script.h
	Code/CryScriptSystem/ScriptBindings/ScriptBind_Sound.cpp
	Code/CryScriptSystem/ScriptBindings/ScriptBind_Sound.h
	Code/CryScriptSystem/ScriptBindings/ScriptBind_System.cpp
	Code/CryScriptSystem/ScriptBindings/ScriptBind_System.h
	Code/CryScriptSystem/FunctionHandler.cpp
	Code/CryScriptSystem/FunctionHandler.h
	Code/CryScriptSystem/ScriptSystem.cpp
	Code/CryScriptSystem/ScriptSystem.h
	Code/CryScriptSystem/ScriptTable.cpp
	Code/CryScriptSystem/ScriptTable.h
	Code/CryScriptSystem/ScriptTimerManager.cpp
	Code/CryScriptSystem/ScriptTimerManager.h
	Code/CryScriptSystem/ScriptUtil.cpp
	Code/CryScriptSystem/ScriptUtil.h
	Code/CrySystem/CPUInfo.cpp
	Code/CrySystem/CPUInfo.h
	Code/CrySystem/CrashTest.cpp
	Code/CrySystem/CrashTest.h
	Code/CrySystem/CryLog.cpp
	Code/CrySystem/CryLog.h
	Code/CrySystem/CryMemoryManager.cpp
	Code/CrySystem/CryMemoryManager.h
	Code/CrySystem/CryPak.cpp
	Code/CrySystem/CryPak.h
	Code/CrySystem/GameWindow.cpp
	Code/CrySystem/GameWindow.h
	Code/CrySystem/HardwareMouse.cpp
	Code/CrySystem/HardwareMouse.h
	Code/CrySystem/LocalizationManager.cpp
	Code/CrySystem/LocalizationManager.h
	Code/CrySystem/Logger.cpp
	Code/CrySystem/Logger.h
	Code/CrySystem/Pak/FileInZipPak.cpp
	Code/CrySystem/Pak/FileInZipPak.h
	Code/CrySystem/Pak/FileOutsidePak.cpp
	Code/CrySystem/Pak/FileOutsidePak.h
	Code/CrySystem/Pak/IFileInPak.h
	Code/CrySystem/Pak/IPak.h
	Code/CrySystem/Pak/ZipCache.cpp
	Code/CrySystem/Pak/ZipCache.h
	Code/CrySystem/Pak/ZipPak.cpp
	Code/CrySystem/Pak/ZipPak.h
	Code/CrySystem/RandomGenerator.cpp
	Code/CrySystem/RandomGenerator.h
	Code/CrySystem/ResourceList.cpp
	Code/CrySystem/ResourceList.h
	Code/CrySystem/StreamEngine.cpp
	Code/CrySystem/StreamEngine.h
	Code/Library/CPUID.cpp
	Code/Library/CPUID.h
	Code/Library/CrashLogger.cpp
	Code/Library/CrashLogger.h
	Code/Library/DigitCount.h
	Code/Library/PathTools.cpp
	Code/Library/PathTools.h
	Code/Library/PathTree.h
	Code/Library/SlotVector.h
	Code/Library/StdFile.cpp
	Code/Library/StdFile.h
	Code/Library/StlportMap.h
	Code/Library/StlportSet.h
	Code/Library/StlportVector.h
	Code/Library/StringTools.cpp
	Code/Library/StringTools.h
	Code/Library/Util.cpp
	Code/Library/Util.h
	Code/Library/WinAPI.cpp
	Code/Library/WinAPI.h
	ThirdParty/Lua/src/lapi.c
	ThirdParty/Lua/src/lapi.h
	ThirdParty/Lua/src/lauxlib.c
	ThirdParty/Lua/src/lauxlib.h
	ThirdParty/Lua/src/lbaselib.c
	ThirdParty/Lua/src/lcode.c
	ThirdParty/Lua/src/lcode.h
	ThirdParty/Lua/src/ldblib.c
	ThirdParty/Lua/src/ldebug.c
	ThirdParty/Lua/src/ldebug.h
	ThirdParty/Lua/src/ldo.c
	ThirdParty/Lua/src/ldo.h
	ThirdParty/Lua/src/ldump.c
	ThirdParty/Lua/src/lfunc.c
	ThirdParty/Lua/src/lfunc.h
	ThirdParty/Lua/src/lgc.c
	ThirdParty/Lua/src/lgc.h
	ThirdParty/Lua/src/linit.c
	ThirdParty/Lua/src/liolib.c
	ThirdParty/Lua/src/llex.c
	ThirdParty/Lua/src/llex.h
	ThirdParty/Lua/src/llimits.h
	ThirdParty/Lua/src/lmathlib.c
	ThirdParty/Lua/src/lmem.c
	ThirdParty/Lua/src/lmem.h
	ThirdParty/Lua/src/loadlib.c
	ThirdParty/Lua/src/lobject.c
	ThirdParty/Lua/src/lobject.h
	ThirdParty/Lua/src/lopcodes.c
	ThirdParty/Lua/src/lopcodes.h
	ThirdParty/Lua/src/loslib.c
	ThirdParty/Lua/src/lparser.c
	ThirdParty/Lua/src/lparser.h
	ThirdParty/Lua/src/lstate.c
	ThirdParty/Lua/src/lstate.h
	ThirdParty/Lua/src/lstring.c
	ThirdParty/Lua/src/lstring.h
	ThirdParty/Lua/src/lstrlib.c
	ThirdParty/Lua/src/ltable.c
	ThirdParty/Lua/src/ltable.h
	ThirdParty/Lua/src/ltablib.c
	ThirdParty/Lua/src/ltm.c
	ThirdParty/Lua/src/ltm.h
	ThirdParty/Lua/src/luaconf.h
	ThirdParty/Lua/src/lua.h
	ThirdParty/Lua/src/lualib.h
	ThirdParty/Lua/src/lundump.c
	ThirdParty/Lua/src/lundump.h
	ThirdParty/Lua/src/lvm.c
	ThirdParty/Lua/src/lvm.h
	ThirdParty/Lua/src/lzio.c
	ThirdParty/Lua/src/lzio.h
	ThirdParty/miniz/miniz.c
	ThirdParty/miniz/miniz.h
	ThirdParty/nlohmann/json.hpp
	ThirdParty/nlohmann/json_fwd.hpp
	ThirdParty/picosha2.h
	ThirdParty/tracy/public/TracyClient.cpp
	${PROJECT_BINARY_DIR}/config.h
	${PROJECT_BINARY_DIR}/version.h
)

if(BUILD_BITS EQUAL 64)
	target_compile_definitions(Base PUBLIC BUILD_64BIT)
endif()

if(CRYMP_USE_MIMALLOC)
	target_sources(Base PRIVATE
		ThirdParty/mimalloc/src/alloc.c
		ThirdParty/mimalloc/src/alloc-aligned.c
		ThirdParty/mimalloc/src/alloc-posix.c
		ThirdParty/mimalloc/src/arena.c
		ThirdParty/mimalloc/src/bitmap.c
		ThirdParty/mimalloc/src/heap.c
		ThirdParty/mimalloc/src/init.c
		ThirdParty/mimalloc/src/options.c
		ThirdParty/mimalloc/src/os.c
		ThirdParty/mimalloc/src/page.c
		ThirdParty/mimalloc/src/random.c
		ThirdParty/mimalloc/src/segment.c
		ThirdParty/mimalloc/src/segment-map.c
		ThirdParty/mimalloc/src/stats.c
		ThirdParty/mimalloc/src/prim/prim.c
	)

	target_include_directories(Base SYSTEM PUBLIC ThirdParty/mimalloc/include)
endif()

if(CRYMP_TRACY_ENABLED)
	target_compile_definitions(Base PUBLIC TRACY_ENABLE)
endif()

target_compile_definitions(Base PUBLIC _CRT_SECURE_NO_WARNINGS)

target_compile_options(Base PUBLIC /W4
	/wd4018  # Disable - 'token' : signed/unsigned mismatch
	/wd4100  # Disable - 'identifier' : unreferenced formal parameter
	/wd4127  # Disable - conditional expression is constant
	/wd4189  # Disable - 'identifier' : local variable is initialized but not referenced
	/wd4201  # Disable - nonstandard extension used : nameless struct/union
	/wd4244  # Disable - 'conversion' : conversion from 'type1' to 'type2', possible loss of data
	/wd4245  # Disable - 'conversion' : conversion from 'type1' to 'type2', signed/unsigned mismatch
	/wd4267  # Disable - 'var' : conversion from 'size_t' to 'type', possible loss of data
	/wd4305  # Disable - 'conversion': truncation from 'type1' to 'type2'
	/wd4456  # Disable - declaration of 'identifier' hides previous local declaration
	/wd4458  # Disable - declaration of 'identifier' hides class member
	/wd4702  # Disable - unreachable code
	/wd5054  # Disable - operator 'operator-name': deprecated between enumerations of different types
	/w15262  # Enable - implicit fall-through occurs here; are you missing a break statement?
	/w45263  # Enable - calling 'std::move' on a temporary object prevents copy elision
)

target_include_directories(Base PUBLIC Code ${PROJECT_BINARY_DIR})
target_include_directories(Base SYSTEM PUBLIC
	ThirdParty
	ThirdParty/Lua/src
	ThirdParty/miniz
	ThirdParty/tracy/public
)

target_link_libraries(Base PUBLIC dbghelp ws2_32 winhttp)

# disable ASLR and unlock memory above 2 GB
target_link_options(Base PUBLIC /DYNAMICBASE:NO /LARGEADDRESSAWARE)

################################################################################

add_library(CryGame OBJECT
	Code/CryGame/Actors/Actor.cpp
	Code/CryGame/Actors/Actor.h
	Code/CryGame/Actors/Alien/Alien.cpp
	Code/CryGame/Actors/Alien/Alien.h
	Code/CryGame/Actors/Alien/CompatibilityAlienMovementController.cpp
	Code/CryGame/Actors/Alien/CompatibilityAlienMovementController.h
	Code/CryGame/Actors/Alien/Hunter.cpp
	Code/CryGame/Actors/Alien/Hunter.h
	Code/CryGame/Actors/Alien/Observer.cpp
	Code/CryGame/Actors/Alien/Observer.h
	Code/CryGame/Actors/Alien/Scout.cpp
	Code/CryGame/Actors/Alien/Scout.h
	Code/CryGame/Actors/Alien/Trooper.cpp
	Code/CryGame/Actors/Alien/Trooper.h
	Code/CryGame/Actors/GrabHandler.cpp
	Code/CryGame/Actors/GrabHandler.h
	Code/CryGame/Actors/Player/IPlayerInput.h
	Code/CryGame/Actors/Player/NanoSuit.cpp
	Code/CryGame/Actors/Player/NanoSuit.h
	Code/CryGame/Actors/Player/NetPlayerInput.cpp
	Code/CryGame/Actors/Player/NetPlayerInput.h
	Code/CryGame/Actors/Player/Player.cpp
	Code/CryGame/Actors/Player/Player.h
	Code/CryGame/Actors/Player/PlayerInput.cpp
	Code/CryGame/Actors/Player/PlayerInput.h
	Code/CryGame/Actors/Player/PlayerMovement.cpp
	Code/CryGame/Actors/Player/PlayerMovement.h
	Code/CryGame/Actors/Player/PlayerMovementController.cpp
	Code/CryGame/Actors/Player/PlayerMovementController.h
	Code/CryGame/Actors/Player/PlayerRotation.cpp
	Code/CryGame/Actors/Player/PlayerRotation.h
	Code/CryGame/Actors/Player/PlayerView.cpp
	Code/CryGame/Actors/Player/PlayerView.h
	Code/CryGame/Actors/Player/WeaponAttachmentManager.cpp
	Code/CryGame/Actors/Player/WeaponAttachmentManager.h
	Code/CryGame/Actors/ScreenEffects.cpp
	Code/CryGame/Actors/ScreenEffects.h
	Code/CryGame/Actors/ScriptBind_Actor.cpp
	Code/CryGame/Actors/ScriptBind_Actor.h
	Code/CryGame/Actors/Shark/Shark.cpp
	Code/CryGame/Actors/Shark/Shark.h
	Code/CryGame/Actors/Shark/SharkMovementController.cpp
	Code/CryGame/Actors/Shark/SharkMovementController.h
	Code/CryGame/BulletTime.cpp
	Code/CryGame/BulletTime.h
	Code/CryGame/ClientSynchedStorage.cpp
	Code/CryGame/ClientSynchedStorage.h
	Code/CryGame/Environment/BattleDust.cpp
	Code/CryGame/Environment/BattleDust.h
	Code/CryGame/Environment/Shake.cpp
	Code/CryGame/Environment/Shake.h
	Code/CryGame/Environment/Tornado/FlowTornado.cpp
	Code/CryGame/Environment/Tornado/FlowTornado.h
	Code/CryGame/Environment/Tornado/Tornado.cpp
	Code/CryGame/Environment/Tornado/Tornado.h
	Code/CryGame/FlashAnimation.cpp
	Code/CryGame/FlashAnimation.h
	Code/CryGame/Game.cpp
	Code/CryGame/Game.h
	Code/CryGame/GameActions.cpp
	Code/CryGame/GameActions.h
	Code/CryGame/GameCVars.cpp
	Code/CryGame/GameCVars.h
	Code/CryGame/GameFactory.cpp
	Code/CryGame/GameFactory.h
	Code/CryGame/GameRules.cpp
	Code/CryGame/GameRules.h
	Code/CryGame/GameRulesClientServer.cpp
	Code/CryGame/HUD/FlashPlayerNULL.h
	Code/CryGame/HUD/GameFlashAnimation.cpp
	Code/CryGame/HUD/GameFlashAnimation.h
	Code/CryGame/HUD/GameFlashLogic.cpp
	Code/CryGame/HUD/GameFlashLogic.h
	Code/CryGame/HUD/HUD.cpp
	Code/CryGame/HUD/HUD.h
	Code/CryGame/HUD/HUDAirStrike.cpp
	Code/CryGame/HUD/HUDCommon.cpp
	Code/CryGame/HUD/HUDCommon.h
	Code/CryGame/HUD/HUDCrosshair.cpp
	Code/CryGame/HUD/HUDCrosshair.h
	Code/CryGame/HUD/HUDEnums.h
	Code/CryGame/HUD/HUDHealthEnergyWeapon.cpp
	Code/CryGame/HUD/HUDInterfaceEffects.cpp
	Code/CryGame/HUD/HUDMissionObjectiveSystem.cpp
	Code/CryGame/HUD/HUDMissionObjectiveSystem.h
	Code/CryGame/HUD/HUDObituary.cpp
	Code/CryGame/HUD/HUDObituary.h
	Code/CryGame/HUD/HUDObject.cpp
	Code/CryGame/HUD/HUDObject.h
	Code/CryGame/HUD/HUDPDA.cpp
	Code/CryGame/HUD/HUDPowerStruggle.cpp
	Code/CryGame/HUD/HUDPowerStruggle.h
	Code/CryGame/HUD/HUDRadar.cpp
	Code/CryGame/HUD/HUDRadar.h
	Code/CryGame/HUD/HUDScopes.cpp
	Code/CryGame/HUD/HUDScopes.h
	Code/CryGame/HUD/HUDScore.cpp
	Code/CryGame/HUD/HUDScore.h
	Code/CryGame/HUD/HUDSilhouettes.cpp
	Code/CryGame/HUD/HUDSilhouettes.h
	Code/CryGame/HUD/HUDSoundImpl.cpp
	Code/CryGame/HUD/HUDTagNames.cpp
	Code/CryGame/HUD/HUDTagNames.h
	Code/CryGame/HUD/HUDTextArea.cpp
	Code/CryGame/HUD/HUDTextArea.h
	Code/CryGame/HUD/HUDTextChat.cpp
	Code/CryGame/HUD/HUDTextChat.h
	Code/CryGame/HUD/HUDTeamInstantAction.cpp
	Code/CryGame/HUD/HUDTeamInstantAction.h
	Code/CryGame/HUD/HUDTextEvents.cpp
	Code/CryGame/HUD/HUDVehicleInterface.cpp
	Code/CryGame/HUD/HUDVehicleInterface.h
	Code/CryGame/HUD/HUDWeaponAccessories.cpp
	Code/CryGame/HUD/ScriptBind_HUD.cpp
	Code/CryGame/HUD/ScriptBind_HUD.h
	Code/CryGame/Items/FlowItemAnimation.cpp
	Code/CryGame/Items/FlowItemAnimation.h
	Code/CryGame/Items/Item.cpp
	Code/CryGame/Items/Item.h
	Code/CryGame/Items/ItemAccessory.cpp
	Code/CryGame/Items/ItemClientServer.cpp
	Code/CryGame/Items/ItemDualWield.cpp
	Code/CryGame/Items/ItemEffect.cpp
	Code/CryGame/Items/ItemEvents.cpp
	Code/CryGame/Items/ItemParamReader.h
	Code/CryGame/Items/ItemParams.cpp
	Code/CryGame/Items/ItemResource.cpp
	Code/CryGame/Items/ItemScheduler.cpp
	Code/CryGame/Items/ItemScheduler.h
	Code/CryGame/Items/ItemSharedParams.cpp
	Code/CryGame/Items/ItemSharedParams.h
	Code/CryGame/Items/ItemString.cpp
	Code/CryGame/Items/ItemString.h
	Code/CryGame/Items/ItemView.cpp
	Code/CryGame/Items/Lam.cpp
	Code/CryGame/Items/Lam.h
	Code/CryGame/Items/PlayerFeature.cpp
	Code/CryGame/Items/PlayerFeature.h
	Code/CryGame/Items/ScriptBind_Item.cpp
	Code/CryGame/Items/ScriptBind_Item.h
	Code/CryGame/Items/Weapons/AmmoParams.cpp
	Code/CryGame/Items/Weapons/AmmoParams.h
	Code/CryGame/Items/Weapons/AmmoPickup.cpp
	Code/CryGame/Items/Weapons/AmmoPickup.h
	Code/CryGame/Items/Weapons/Binocular.cpp
	Code/CryGame/Items/Weapons/Binocular.h
	Code/CryGame/Items/Weapons/C4.cpp
	Code/CryGame/Items/Weapons/C4.h
	Code/CryGame/Items/Weapons/C4Detonator.cpp
	Code/CryGame/Items/Weapons/C4Detonator.h
	Code/CryGame/Items/Weapons/DebugGun.cpp
	Code/CryGame/Items/Weapons/DebugGun.h
	Code/CryGame/Items/Weapons/FireModes/Automatic.cpp
	Code/CryGame/Items/Weapons/FireModes/Automatic.h
	Code/CryGame/Items/Weapons/FireModes/Beam.cpp
	Code/CryGame/Items/Weapons/FireModes/Beam.h
	Code/CryGame/Items/Weapons/FireModes/Burst.cpp
	Code/CryGame/Items/Weapons/FireModes/Burst.h
	Code/CryGame/Items/Weapons/FireModes/Charge.cpp
	Code/CryGame/Items/Weapons/FireModes/Charge.h
	Code/CryGame/Items/Weapons/FireModes/Detonate.cpp
	Code/CryGame/Items/Weapons/FireModes/Detonate.h
	Code/CryGame/Items/Weapons/FireModes/FreezingBeam.cpp
	Code/CryGame/Items/Weapons/FireModes/FreezingBeam.h
	Code/CryGame/Items/Weapons/FireModes/Melee.cpp
	Code/CryGame/Items/Weapons/FireModes/Melee.h
	Code/CryGame/Items/Weapons/FireModes/Plant.cpp
	Code/CryGame/Items/Weapons/FireModes/Plant.h
	Code/CryGame/Items/Weapons/FireModes/Rapid.cpp
	Code/CryGame/Items/Weapons/FireModes/Rapid.h
	Code/CryGame/Items/Weapons/FireModes/Scan.cpp
	Code/CryGame/Items/Weapons/FireModes/Scan.h
	Code/CryGame/Items/Weapons/FireModes/Shotgun.cpp
	Code/CryGame/Items/Weapons/FireModes/Shotgun.h
	Code/CryGame/Items/Weapons/FireModes/Single.cpp
	Code/CryGame/Items/Weapons/FireModes/Single.h
	Code/CryGame/Items/Weapons/FireModes/SingleTG.cpp
	Code/CryGame/Items/Weapons/FireModes/SingleTG.h
	Code/CryGame/Items/Weapons/FireModes/Throw.cpp
	Code/CryGame/Items/Weapons/FireModes/Throw.h
	Code/CryGame/Items/Weapons/FireModes/WorkOnTarget.cpp
	Code/CryGame/Items/Weapons/FireModes/WorkOnTarget.h
	Code/CryGame/Items/Weapons/Fists.cpp
	Code/CryGame/Items/Weapons/Fists.h
	Code/CryGame/Items/Weapons/GunTurret.cpp
	Code/CryGame/Items/Weapons/GunTurret.h
	Code/CryGame/Items/Weapons/OffHand.cpp
	Code/CryGame/Items/Weapons/OffHand.h
	Code/CryGame/Items/Weapons/Projectile.cpp
	Code/CryGame/Items/Weapons/Projectile.h
	Code/CryGame/Items/Weapons/Projectiles/AVMine.cpp
	Code/CryGame/Items/Weapons/Projectiles/AVMine.h
	Code/CryGame/Items/Weapons/Projectiles/Bullet.cpp
	Code/CryGame/Items/Weapons/Projectiles/Bullet.h
	Code/CryGame/Items/Weapons/Projectiles/C4Projectile.cpp
	Code/CryGame/Items/Weapons/Projectiles/C4Projectile.h
	Code/CryGame/Items/Weapons/Projectiles/Claymore.cpp
	Code/CryGame/Items/Weapons/Projectiles/Claymore.h
	Code/CryGame/Items/Weapons/Projectiles/EMPField.cpp
	Code/CryGame/Items/Weapons/Projectiles/EMPField.h
	Code/CryGame/Items/Weapons/Projectiles/HomingMissile.cpp
	Code/CryGame/Items/Weapons/Projectiles/HomingMissile.h
	Code/CryGame/Items/Weapons/Projectiles/Rock.cpp
	Code/CryGame/Items/Weapons/Projectiles/Rock.h
	Code/CryGame/Items/Weapons/Projectiles/Rocket.cpp
	Code/CryGame/Items/Weapons/Projectiles/Rocket.h
	Code/CryGame/Items/Weapons/Projectiles/TacBullet.cpp
	Code/CryGame/Items/Weapons/Projectiles/TacBullet.h
	Code/CryGame/Items/Weapons/Projectiles/TagBullet.cpp
	Code/CryGame/Items/Weapons/Projectiles/TagBullet.h
	Code/CryGame/Items/Weapons/ReferenceWeapon.cpp
	Code/CryGame/Items/Weapons/ReferenceWeapon.h
	Code/CryGame/Items/Weapons/RocketLauncher.cpp
	Code/CryGame/Items/Weapons/RocketLauncher.h
	Code/CryGame/Items/Weapons/ScriptBind_Weapon.cpp
	Code/CryGame/Items/Weapons/ScriptBind_Weapon.h
	Code/CryGame/Items/Weapons/ThrowableWeapon.cpp
	Code/CryGame/Items/Weapons/ThrowableWeapon.h
	Code/CryGame/Items/Weapons/TracerManager.cpp
	Code/CryGame/Items/Weapons/TracerManager.h
	Code/CryGame/Items/Weapons/VehicleWeapon.cpp
	Code/CryGame/Items/Weapons/VehicleWeapon.h
	Code/CryGame/Items/Weapons/Weapon.cpp
	Code/CryGame/Items/Weapons/Weapon.h
	Code/CryGame/Items/Weapons/WeaponClientServer.cpp
	Code/CryGame/Items/Weapons/WeaponEvent.cpp
	Code/CryGame/Items/Weapons/WeaponInput.cpp
	Code/CryGame/Items/Weapons/WeaponSystem_XMLData.cpp
	Code/CryGame/Items/Weapons/WeaponSystem.cpp
	Code/CryGame/Items/Weapons/WeaponSystem.h
	Code/CryGame/Items/Weapons/ZoomModes/IronSight.cpp
	Code/CryGame/Items/Weapons/ZoomModes/IronSight.h
	Code/CryGame/Items/Weapons/ZoomModes/Scope.cpp
	Code/CryGame/Items/Weapons/ZoomModes/Scope.h
	Code/CryGame/MPTutorial.cpp
	Code/CryGame/MPTutorial.h
	Code/CryGame/Menus/CreateGame.cpp
	Code/CryGame/Menus/CreateGame.h
	Code/CryGame/Menus/FlashMenuObject.cpp
	Code/CryGame/Menus/FlashMenuObject.h
	Code/CryGame/Menus/FlashMenuObjectOptions.cpp
	Code/CryGame/Menus/FlashMenuObjectProfiles.cpp
	Code/CryGame/Menus/FlashMenuObjectSingleplayer.cpp
	Code/CryGame/Menus/FlashMenuScreen.cpp
	Code/CryGame/Menus/FlashMenuScreen.h
	Code/CryGame/Menus/GameNetworkProfile.cpp
	Code/CryGame/Menus/GameNetworkProfile.h
	Code/CryGame/Menus/MPHub.cpp
	Code/CryGame/Menus/MPHub.h
	Code/CryGame/Menus/MPLobbyUI.cpp
	Code/CryGame/Menus/MPLobbyUI.h
	Code/CryGame/Menus/MultiplayerMenu.cpp
	Code/CryGame/Menus/MultiplayerMenu.h
	Code/CryGame/Menus/OptionsManager.cpp
	Code/CryGame/Menus/OptionsManager.h
	Code/CryGame/Menus/QuickGame.cpp
	Code/CryGame/Menus/QuickGame.h
	Code/CryGame/NetInputChainDebug.cpp
	Code/CryGame/NetInputChainDebug.h
	Code/CryGame/Nodes/FlowActorSensor.cpp
	Code/CryGame/Nodes/FlowFadeNode.cpp
	Code/CryGame/Nodes/FlowHitInfoNode.cpp
	Code/CryGame/Nodes/FlowNanoSuitNodes.cpp
	Code/CryGame/Nodes/FlowPlayerStagingNode.cpp
	Code/CryGame/Nodes/FlowPostFXNodes.cpp
	Code/CryGame/Nodes/FlowVehicleNodes.cpp
	Code/CryGame/Nodes/G2FlowBaseNode.h
	Code/CryGame/Nodes/HUDNodes.cpp
	Code/CryGame/Nodes/MPNodes.cpp
	Code/CryGame/Nodes/WeaponNodes.cpp
	Code/CryGame/Radio.cpp
	Code/CryGame/Radio.h
	Code/CryGame/SPAnalyst.cpp
	Code/CryGame/SPAnalyst.h
	Code/CryGame/ScriptBind_Game.cpp
	Code/CryGame/ScriptBind_Game.h
	Code/CryGame/ScriptBind_GameRules.cpp
	Code/CryGame/ScriptBind_GameRules.h
	Code/CryGame/ScriptControlledPhysics.cpp
	Code/CryGame/ScriptControlledPhysics.h
	Code/CryGame/ScriptUtils.cpp
	Code/CryGame/ScriptUtils.h
	Code/CryGame/ServerSynchedStorage.cpp
	Code/CryGame/ServerSynchedStorage.h
	Code/CryGame/ShotValidator.cpp
	Code/CryGame/ShotValidator.h
	Code/CryGame/SoundMoods.cpp
	Code/CryGame/SoundMoods.h
	Code/CryGame/SynchedStorage.cpp
	Code/CryGame/SynchedStorage.h
	Code/CryGame/Vehicles/Action/VehicleActionAutomaticDoor.cpp
	Code/CryGame/Vehicles/Action/VehicleActionAutomaticDoor.h
	Code/CryGame/Vehicles/Action/VehicleActionDeployRope.cpp
	Code/CryGame/Vehicles/Action/VehicleActionDeployRope.h
	Code/CryGame/Vehicles/Action/VehicleActionEntityAttachment.cpp
	Code/CryGame/Vehicles/Action/VehicleActionEntityAttachment.h
	Code/CryGame/Vehicles/Action/VehicleActionLandingGears.cpp
	Code/CryGame/Vehicles/Action/VehicleActionLandingGears.h
	Code/CryGame/Vehicles/Damage/VehicleDamageBehaviorBurn.cpp
	Code/CryGame/Vehicles/Damage/VehicleDamageBehaviorBurn.h
	Code/CryGame/Vehicles/Damage/VehicleDamageBehaviorCameraShake.cpp
	Code/CryGame/Vehicles/Damage/VehicleDamageBehaviorCameraShake.h
	Code/CryGame/Vehicles/Damage/VehicleDamageBehaviorCollisionEx.cpp
	Code/CryGame/Vehicles/Damage/VehicleDamageBehaviorCollisionEx.h
	Code/CryGame/Vehicles/Damage/VehicleDamageBehaviorExplosion.cpp
	Code/CryGame/Vehicles/Damage/VehicleDamageBehaviorExplosion.h
	Code/CryGame/Vehicles/Damage/VehicleDamageBehaviorTire.cpp
	Code/CryGame/Vehicles/Damage/VehicleDamageBehaviorTire.h
	Code/CryGame/Vehicles/Movement/VehicleMovementAmphibious.cpp
	Code/CryGame/Vehicles/Movement/VehicleMovementAmphibious.h
	Code/CryGame/Vehicles/Movement/VehicleMovementBase.cpp
	Code/CryGame/Vehicles/Movement/VehicleMovementBase.h
	Code/CryGame/Vehicles/Movement/VehicleMovementHelicopter.cpp
	Code/CryGame/Vehicles/Movement/VehicleMovementHelicopter.h
	Code/CryGame/Vehicles/Movement/VehicleMovementHovercraft.cpp
	Code/CryGame/Vehicles/Movement/VehicleMovementHovercraft.h
	Code/CryGame/Vehicles/Movement/VehicleMovementStdBoat.cpp
	Code/CryGame/Vehicles/Movement/VehicleMovementStdBoat.h
	Code/CryGame/Vehicles/Movement/VehicleMovementStdWheeled.cpp
	Code/CryGame/Vehicles/Movement/VehicleMovementStdWheeled.h
	Code/CryGame/Vehicles/Movement/VehicleMovementTank.cpp
	Code/CryGame/Vehicles/Movement/VehicleMovementTank.h
	Code/CryGame/Vehicles/Movement/VehicleMovementTweaks.cpp
	Code/CryGame/Vehicles/Movement/VehicleMovementTweaks.h
	Code/CryGame/Vehicles/Movement/VehicleMovementVTOL.cpp
	Code/CryGame/Vehicles/Movement/VehicleMovementVTOL.h
	Code/CryGame/Vehicles/Movement/VehicleMovementWarrior.cpp
	Code/CryGame/Vehicles/Movement/VehicleMovementWarrior.h
	Code/CryGame/Vehicles/VehicleClient.cpp
	Code/CryGame/Vehicles/VehicleClient.h
	Code/CryGame/Voting.cpp
	Code/CryGame/Voting.h
)

target_link_libraries(CryGame PUBLIC Base)

################################################################################

set(CRYMP_CLIENT_EXE "CryMP-Client${BUILD_BITS}")
set(CRYMP_SERVER_EXE "CryMP-Server${BUILD_BITS}")

message(STATUS "Target client executable: ${CRYMP_CLIENT_EXE}.exe")
message(STATUS "Target server executable: ${CRYMP_SERVER_EXE}.exe")

add_executable(${CRYMP_CLIENT_EXE} WIN32
	Code/Launcher/Launcher.cpp
	Code/Launcher/Launcher.h
	Code/Launcher/Main.cpp
	Code/Launcher/MemoryPatch.cpp
	Code/Launcher/MemoryPatch.h
	Code/Launcher/Resources.h
	Code/Launcher/StartupTime.cpp
	Code/Launcher/StartupTime.h
	Resources/ClientLauncher.rc
	Resources/Launcher.manifest
)

add_executable(${CRYMP_SERVER_EXE} WIN32
	Code/Launcher/Launcher.cpp
	Code/Launcher/Launcher.h
	Code/Launcher/Main.cpp
	Code/Launcher/MemoryPatch.cpp
	Code/Launcher/MemoryPatch.h
	Code/Launcher/Resources.h
	Code/Launcher/StartupTime.cpp
	Code/Launcher/StartupTime.h
	Resources/ServerLauncher.rc
	Resources/Launcher.manifest
)

target_link_libraries(${CRYMP_CLIENT_EXE} PRIVATE Base CryGame)
target_link_libraries(${CRYMP_SERVER_EXE} PRIVATE Base CryGame)

# use these only in Launcher code to avoid recompiling everything for each exe
target_compile_definitions(${CRYMP_CLIENT_EXE} PRIVATE CLIENT_LAUNCHER)
target_compile_definitions(${CRYMP_SERVER_EXE} PRIVATE SERVER_LAUNCHER)

if(CRYMP_CONSOLE_APP)
	set_target_properties(${CRYMP_CLIENT_EXE} PROPERTIES WIN32_EXECUTABLE FALSE)
	set_target_properties(${CRYMP_SERVER_EXE} PROPERTIES WIN32_EXECUTABLE FALSE)
endif()

################################################################################

set(CRYMP_INTERNAL_PAK_PATH ${PROJECT_BINARY_DIR}/Pak.zip)

# also list directories to detect removed files
file(GLOB_RECURSE CRYMP_INTERNAL_PAK_DEPENDENCIES LIST_DIRECTORIES true CONFIGURE_DEPENDS Pak/*)
list(INSERT CRYMP_INTERNAL_PAK_DEPENDENCIES 0 ${PROJECT_SOURCE_DIR}/Pak)

# build internal pak
add_custom_command(
	OUTPUT ${CRYMP_INTERNAL_PAK_PATH}
	DEPENDS ${CRYMP_INTERNAL_PAK_DEPENDENCIES}
	COMMAND ${CMAKE_COMMAND} -E tar cf ${CRYMP_INTERNAL_PAK_PATH} --format=zip .
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/Pak
	VERBATIM
)

################################################################################

set(CRYMP_CLIENT_RESOURCES
	${PROJECT_SOURCE_DIR}/Code/Launcher/Resources.h
	${PROJECT_SOURCE_DIR}/Resources/ClientIcon.ico
	${PROJECT_SOURCE_DIR}/Resources/CursorAmber.cur
	${PROJECT_SOURCE_DIR}/Resources/CursorBlue.cur
	${PROJECT_SOURCE_DIR}/Resources/CursorGreen.cur
	${PROJECT_SOURCE_DIR}/Resources/CursorRed.cur
	${PROJECT_SOURCE_DIR}/Resources/CursorWhite.cur
	${PROJECT_SOURCE_DIR}/masters.txt
	${PROJECT_BINARY_DIR}/config.h
	${PROJECT_BINARY_DIR}/version.h
	${CRYMP_INTERNAL_PAK_PATH}
)

set(CRYMP_SERVER_RESOURCES
	${PROJECT_SOURCE_DIR}/Code/Launcher/Resources.h
	${PROJECT_SOURCE_DIR}/Resources/ServerIcon.ico
	${PROJECT_SOURCE_DIR}/masters.txt
	${PROJECT_BINARY_DIR}/config.h
	${PROJECT_BINARY_DIR}/version.h
	${CRYMP_INTERNAL_PAK_PATH}
)

set_source_files_properties(Resources/ClientLauncher.rc PROPERTIES OBJECT_DEPENDS "${CRYMP_CLIENT_RESOURCES}")
set_source_files_properties(Resources/ServerLauncher.rc PROPERTIES OBJECT_DEPENDS "${CRYMP_SERVER_RESOURCES}")

################################################################################

configure_file(config.h.in ${PROJECT_BINARY_DIR}/config.h)
