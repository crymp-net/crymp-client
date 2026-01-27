/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements vehicle functionality

-------------------------------------------------------------------------
History:
- 05:10:2004: Created by Mathieu Pinard

*************************************************************************/
#include "CryCommon/CrySystem/ISystem.h"

#include "CryCommon/CryAction/IViewSystem.h"
#include "CryCommon/CryAction/IItemSystem.h"
#include "CryCommon/CryAction/IVehicleSystem.h"
#include "CryCommon/CryPhysics/IPhysics.h"
#include "CryCommon/CryAnimation/ICryAnimation.h"
#include "CryCommon/CryAction/IActorSystem.h"
#include "CryCommon/CrySoundSystem/ISound.h"
#include "CryCommon/CryNetwork/ISerialize.h"
#include "CryCommon/CryAISystem/IAgent.h"
#include "CryCommon/CryMath/Cry_Math.h"
#include "CryCommon/CrySystem/gEnv.h"

#include "CrySystem/CryPak.h"

#include "CryCommon/CryAction/IGameRulesSystem.h"

#include "VehicleSystem.h"
#include "ScriptBind_Vehicle.h"
#include "ScriptBind_VehicleSeat.h"
#include "VehicleCVars.h"

#include "Vehicle.h"
#include "VehicleAnimation.h"
#include "VehicleComponent.h"
#include "VehicleHelper.h"
#include "VehicleSeat.h"
#include "VehicleSeatGroup.h"
#include "VehicleSeatActionWeapons.h"

#include "VehiclePartAnimated.h"
#include "VehiclePartAnimatedJoint.h"
#include "VehiclePartSubPartWheel.h"
#include "VehiclePartTread.h"
#include "CryCommon/CryInput/IInput.h"
#include "CryCommon/CryRenderer/IRenderAuxGeom.h"
#include "CryCommon/CryCore/StlUtils.h"
#include "CryAction/GameFramework.h"
#include "ScriptBind_Vehicle.h"
#include "ScriptBind_VehicleSeat.h"

#define ENGINESLOT_MINSPEED 0.05f

// Max view distance ratio for vehicles.
#define VEHICLE_VIEW_DIST_RATIO (50)

ISurfaceType* CVehicle::sSurfaceMatGlassVehicle = 0;

//------------------------------------------------------------------------
CVehicle::CVehicle() :
	m_mass(0.0f),
	m_wheelCount(0),
	m_pMovement(NULL),
	m_ownerId(0),
	m_frozen(false),
	m_customEngineSlot(false),
	m_bRetainGravity(false),
	m_debugIndex(0),
	m_usesVehicleActionToEnter(false),
	m_isDestroyed(false),
	m_initialposition(0.0f),
	m_lastWeaponId(0),
	m_pSoundProxy(0),
	m_abandonedSoundId(INVALID_SOUNDID),
	m_bNeedsUpdate(true),
	m_lastFrameId(0),
	m_pInventory(NULL),
	m_collisionDisabledTime(0.0f),
	m_indestructible(false),
	m_pVehicleSystem(NULL),
	m_damageMax(0.f),
	m_pBrokenWindowMat(0)
{
	m_gravity.zero();
}

//------------------------------------------------------------------------
unsigned int CVehicle::GetPartChildrenCount(IVehiclePart* pParentPart)
{
	TVehiclePartVector::iterator partIte = m_parts.begin();
	TVehiclePartVector::iterator partEnd = m_parts.end();

	unsigned int counter = 0;

	for (; partIte != partEnd; ++partIte)
	{
		IVehiclePart* pPart = partIte->second;

		if (pPart && pPart->GetParent() == pParentPart)
			counter++;
	}

	return counter;
}

//------------------------------------------------------------------------
CVehicle::~CVehicle()
{
	for (TVehicleSeatVector::iterator it = m_seats.begin(), end = m_seats.end(); it != end; ++it)
	{
		if (it->second->GetPassenger())
			it->second->Exit(false, true);
	}

	SVehicleEventParams pa;
	BroadcastVehicleEvent(eVE_PreVehicleDeletion, pa);

	KillTimers();

	for (TVehicleSoundEventId id = 0; id < m_soundEvents.size(); ++id)
		StopSound(id);

	if (!gEnv->pSystem->IsSerializingFile())
		ReleaseDamages();

	GetGameObject()->ReleaseProfileManager(this);

	SVehicleEventParams eventParams;
	BroadcastVehicleEvent(eVE_VehicleDeleted, eventParams);

	if (SpawnAndDeleteEntities(true))
	{
		// delete AI anchors
		IEntityClass* pAnchorClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("AIAnchor");

		int nChildren = GetEntity()->GetChildCount();
		for (int i = nChildren - 1; i >= 0; --i)
		{
			IEntity* pEnt = GetEntity()->GetChild(i);

			if (pEnt && pEnt->GetClass() == pAnchorClass)
			{
				gEnv->pEntitySystem->RemoveEntity(pEnt->GetId(), true);
			}
		}
	}

	TVehicleActionVector::iterator actionIte = m_actions.begin();
	TVehicleActionVector::iterator actionEnd = m_actions.end();
	for (; actionIte != actionEnd; ++actionIte)
	{
		SVehicleActionInfo& actionInfo = *actionIte;
		actionInfo.pAction->Release();
	}

	for (TVehicleSeatVector::iterator ite = m_seats.begin(); ite != m_seats.end(); ++ite)
	{
		ite->second->Release();
	}

	for (TVehicleHelperMap::iterator it = m_helpers.begin(); it != m_helpers.end(); ++it)
	{
		delete it->second;
	}

	//std::for_each(m_components.begin(), m_components.end(), stl::container_object_deleter());
	std::for_each(m_components.begin(), m_components.end(), [](auto* ptr) { delete ptr; }); //CryMP: Fixme?

	TVehiclePartVector::iterator partIte;
	TVehiclePartVector::iterator partEnd;

	bool isEmpty;

	do
	{
		partIte = m_parts.begin();
		partEnd = m_parts.end();

		isEmpty = true;

		for (; partIte != partEnd; ++partIte)
		{
			IVehiclePart* pPart = partIte->second;
			if (pPart)
			{
				if (GetPartChildrenCount(pPart) == 0)
				{
					pPart->Release();
					partIte->second = NULL;
				}
				else
				{
					isEmpty = false;
				}
			}
		}
	} while (m_parts.size() > 0 && isEmpty != true);

	m_parts.clear();


	if (m_pMovement)
	{
		GetGameObject()->EnablePhysicsEvent(false, eEPE_OnPostStepImmediate);
		SAFE_RELEASE(m_pMovement);
	}

	if (m_pInventory)
	{
		if (gEnv->bServer)
			m_pInventory->Destroy();

		GetGameObject()->ReleaseExtension("Inventory");
	}

	gEnv->pGame->GetIGameFramework()->GetIVehicleSystem()->RemoveVehicle(GetEntityId());

	GetGameObject()->EnablePhysicsEvent(false, eEPE_OnCollisionLogged | eEPE_OnStateChangeLogged);
	//GetGameObject()->DisablePostUpdates(this); 
}

//------------------------------------------------------------------------
bool CVehicle::SpawnAndDeleteEntities(bool clientBasedOnly)
{
	if (!clientBasedOnly)
		return !(GetISystem()->IsSerializingFile() || IsDemoPlayback());
	return !(GetISystem()->IsSerializingFile());
}

//------------------------------------------------------------------------
void MergeTable(IScriptTable* dest, IScriptTable* source)
{
	IScriptTable::Iterator ite = source->BeginIteration();
	do
	{
		dest->SetValue(ite.sKey, ite.value);
	} while (source->MoveNext(ite));
	source->EndIteration(ite);
}


//------------------------------------------------------------------------
void CVehicle::LoadParts(const CVehicleParams& table, IVehiclePart* pParent, SPartInitInfo& initInfo)
{
	int c = table.getChildCount();
	int i = 0;

	for (; i < c; i++)
	{
		CVehicleParams partTable = table.getChild(i);
		if (IVehiclePart* pPart = AddPart(partTable, pParent, initInfo))
		{
			if (CVehicleParams childPartsTable = partTable.findChild("Parts"))
			{
				LoadParts(childPartsTable, pPart, initInfo);
			}
		}
	}
}

namespace
{
	//------------------------------------------------------------------------
	XmlNodeRef LoadXmlForClass(const char* className, const string& xmlDir)
	{
		// try to load file with same filename as class name
		const static string ext(".xml");

		//bool bPrevBinaryXmlLoading = GetISystem()->GetXmlUtils()->EnableBinaryXmlLoading(false);

		string xmlFile = xmlDir + className + ext;
		XmlNodeRef xmlData = gEnv->pSystem->LoadXmlFile(xmlFile.c_str());

		//GetISystem()->GetXmlUtils()->EnableBinaryXmlLoading(bPrevBinaryXmlLoading);

		if (0 == xmlData)
		{
			// if not found, search xml dir for files with name="<class>" attribute    
			_finddata_t fd;
			intptr_t handle;
			int res;

			if ((handle = gEnv->pCryPak->FindFirst((xmlDir + string("*") + ext).c_str(), &fd)) != -1)
			{
				do
				{
					if (XmlNodeRef root = GetISystem()->LoadXmlFile((xmlDir + string(fd.name)).c_str()))
					{
						const char* name = root->getAttr("name");
						if (0 == strcmp(name, className))
						{
							xmlData = root;
							break;
						}
					}
					res = gEnv->pCryPak->FindNext(handle, &fd);
				} while (res >= 0);

				gEnv->pCryPak->FindClose(handle);
			}
		}

		return xmlData;
	}
}


//------------------------------------------------------------------------
void CVehicle::SerializeSpawnInfo(TSerialize ser)
{
	assert(ser.IsReading());
	ser.Value("modification", m_modification);
	ser.Value("paint", m_paint);
}

//------------------------------------------------------------------------
ISerializableInfoPtr CVehicle::GetSpawnInfo()
{
	struct SInfo : public ISerializableInfo
	{
		string modification;
		string paint;
		void SerializeWith(TSerialize ser)
		{
			ser.Value("modification", modification);
			ser.Value("paint", paint);

		}
	};

	SInfo* p = new SInfo;
	p->modification = m_modification;
	p->paint = m_paint;
	return p;
}

//------------------------------------------------------------------------
bool CVehicle::Init(IGameObject* pGameObject)
{
	SetGameObject(pGameObject);

	if (!GetGameObject()->CaptureProfileManager(this))
	{
		CryLogWarning("<%s> CaptureProfileManager failed", GetEntity()->GetName());
		return false;
	}

	m_pSoundProxy = (IEntitySoundProxy*)GetEntity()->CreateProxy(ENTITY_PROXY_SOUND);
	assert(m_pSoundProxy && "no sound proxy on entity");

	m_pVehicleSystem = gEnv->pGame->GetIGameFramework()->GetIVehicleSystem();
	m_engineSlotBySpeed = true;

	m_parts.clear();
	m_seats.clear();

	gEnv->pGame->GetIGameFramework()->GetIVehicleSystem()->AddVehicle(GetEntityId(), this);
	GameFramework::GetInstance()->GetScriptBind_Vehicle()->AttachTo(this); //CryMP

	m_pInventory = static_cast<IInventory*>(GetGameObject()->AcquireExtension("Inventory"));

	InitRespawn();

	const char* className = GetEntity()->GetClass()->GetName();

	// load the xml data into the vehicle lua table
	const static string xmlDef = "CryMP/Xml/Vehicles/def_vehicle.xml";
	const static string xmlDir = "CryMP/Xml/Vehicles/Implementations/";

	XmlNodeRef vehicleXmlData = LoadXmlForClass(className, xmlDir);

	if (!vehicleXmlData)
	{
		CryLogWarning("<%s>: failed loading xml file (directory %s), aborting initialization", GetEntity()->GetName(), xmlDir.c_str());
		return false;
	}

	// if modification is used, merge its elements with main tree
	if (m_modification.empty())
	{
		IScriptTable* pScriptTable = GetEntity()->GetScriptTable();
		if (pScriptTable)
		{
			SmartScriptTable pProps;
			if (pScriptTable->GetValue("Properties", pProps))
			{
				const char* modification = 0;
				if (pProps->GetValue("Modification", modification))
					m_modification = modification;
			}
		}
	}

	if (m_paint.empty())
	{
		IScriptTable* pScriptTable = GetEntity()->GetScriptTable();
		if (pScriptTable)
		{
			SmartScriptTable pProps;
			if (pScriptTable->GetValue("Properties", pProps))
			{
				const char* paint = 0;
				if (pProps->GetValue("Paint", paint))
					m_paint = paint;
			}
		}
	}

	// this merges modification data and must be done before further processing
	//InitModification(xmlData, m_modification.c_str()); //CryMP: Fixme?

	CryLogAlways("$3[CryMP] Spawning vehicle %s with modification '%s'", GetEntity()->GetName(), m_modification.c_str());

	CVehicleModificationParams vehicleModificationParams(vehicleXmlData, m_modification.c_str());
	CVehicleParams vehicleParams(vehicleXmlData, vehicleModificationParams);

	// read physics properties

	if (CVehicleParams physicsTable = vehicleParams.findChild("Physics"))
	{
		if (CVehicleParams buoyancyTable = physicsTable.findChild("Buoyancy"))
		{
			buoyancyTable.getAttr("waterDamping", m_buoyancyParams.waterDamping);
			buoyancyTable.getAttr("waterDensity", m_buoyancyParams.waterDensity);
			buoyancyTable.getAttr("waterResistance", m_buoyancyParams.waterResistance);
		}

		physicsTable.getAttr("damping", m_simParams.damping);
		physicsTable.getAttr("dampingFreefall", m_simParams.dampingFreefall);
		physicsTable.getAttr("gravityFreefall", m_simParams.gravityFreefall);
		physicsTable.getAttr("retainGravity", m_bRetainGravity);

		if (CVehicleParams simulationTable = physicsTable.findChild("Simulation"))
		{
			simulationTable.getAttr("maxTimeStep", m_simParams.maxTimeStep);
			simulationTable.getAttr("minEnergy", m_simParams.minEnergy);
			simulationTable.getAttr("maxLoggedCollisions", m_simParams.maxLoggedCollisions);

			if (VehicleCVars().v_vehicle_quality == 1)
				m_simParams.maxTimeStep = max(m_simParams.maxTimeStep, 0.04f);
		}

		bool pushable = false;
		physicsTable.getAttr("pushable", pushable);
		if (!pushable)
			m_paramsFlags.flagsAND = ~pef_pushable_by_players;
	}

	// load parts  
	SPartInitInfo partInitInfo;
	CVehicleParams partsTable = vehicleParams.findChild("Parts");
	if (partsTable)
	{
		LoadParts(partsTable, nullptr, partInitInfo);
	}
	else
		CryLog("%s: No Parts table found!", GetEntity()->GetName());

	for (TVehiclePartVector::iterator ite = m_parts.begin(); ite != m_parts.end(); ite++)
	{
		IVehiclePart* pPart = ite->second;
		pPart->ChangeState(IVehiclePart::eVGS_Default);
	}

	InitHelpers(vehicleParams);

	// load the components (after the parts)
	if (CVehicleParams componentsTable = vehicleParams.findChild("Components"))
	{
		int componentCount = componentsTable.getChildCount();
		m_components.reserve(componentCount);

		int i = 0;
		for (; i < componentCount; i++)
		{
			if (CVehicleParams componentTable = componentsTable.getChild(i))
			{
				AddComponent(componentTable);
			}
		}

		// register parts at their components
		for (std::vector<SPartComponentPair>::const_iterator it = partInitInfo.partComponentMap.begin(), end = partInitInfo.partComponentMap.end(); it != end; ++it)
		{
			CVehicleComponent* pVehicleComponent = static_cast<CVehicleComponent*>(GetComponent(it->component.c_str()));
			if (pVehicleComponent)
				pVehicleComponent->AddPart(it->pPart);
		}
	}

	InitParticles(vehicleParams); // must be after part init because it needs to find the helpers!

	// Init Animations
	if (CVehicleParams animationsTable = vehicleParams.findChild("Animations"))
	{
		int i = 0;
		int c = animationsTable.getChildCount();

		for (; i < c; i++)
		{
			if (CVehicleParams animTable = animationsTable.getChild(i))
			{
				IVehicleAnimation* pVehicleAnimation = new CVehicleAnimation;

				if (animTable.haveAttr("name") && pVehicleAnimation->Init(this, animTable))
					m_animations.push_back(TVehicleStringAnimationPair(animTable.getAttr("name"), pVehicleAnimation));
				else
					delete pVehicleAnimation;
			}
		}
	}

	// Init the movement
	if (CVehicleParams movementsTable = vehicleParams.findChild("MovementParams"))
	{
		int i = 0;
		int c = movementsTable.getChildCount();

		for (; i < c; i++)
		{
			if (CVehicleParams moveTable = movementsTable.getChild(i))
			{
				if (!SetMovement(moveTable.getTag(), moveTable))
					return false;
			}
		}
	}

	// load the seats, if any
	if (CVehicleParams seatsTable = vehicleParams.findChild("Seats"))
	{
		int c = seatsTable.getChildCount();
		m_seats.reserve(c);

		int i = 0;
		for (; i < c; i++)
		{
			if (CVehicleParams seatRef = seatsTable.getChild(i))
			{
				CVehicleSeat* pVehicleSeat = new CVehicleSeat;

				if (seatRef.haveAttr("name") && pVehicleSeat)
				{
					if (pVehicleSeat->Init(this, m_seats.size() + 1, seatRef))
					{
						m_seats.push_back(TVehicleSeatPair(seatRef.getAttr("name"), pVehicleSeat));
					}
					else
					{
						if (pVehicleSeat)
							delete pVehicleSeat;
					}
				}
			}
		}

		m_transitionInfo.resize(c);

		i = 0;
		for (; i < c; i++)
		{
			if (CVehicleParams seatRef = seatsTable.getChild(i))
			{
				if (IVehicleSeat* pVehicleSeat = GetSeatById(i + 1))
				{
					STransitionInfo& info = m_transitionInfo[i];

					if (CVehicleParams transitionTable = seatRef.findChild("Transitions"))
					{
						if (CVehicleParams waitTable = transitionTable.findChild("WaitFor"))
						{
							unsigned int waitCount = waitTable.getChildCount();
							info.waitFor.reserve(waitCount);

							// if "Transitions" table is specified, read the seats from there. 
							for (int w = 1, nWait = waitCount; w <= nWait; ++w)
							{
								TVehicleSeatId waitForId = InvalidVehicleSeatId;
								CVehicleParams waitRef = waitTable.getChild(w);

								waitForId = GetSeatId(waitRef.getAttr("value"));
								if (waitForId != InvalidVehicleSeatId)
									info.waitFor.push_back(waitForId);
							}
						}
					}
					else
					{
						// else add all seats that use the same exit helper as the current one
						for (TVehicleSeatVector::const_iterator it = m_seats.begin(), end = m_seats.end(); it != end; ++it)
						{
							if (it->second == pVehicleSeat)
								continue;

							if (IVehicleHelper* pHelper = it->second->GetExitHelper())
							{
								if (pHelper == static_cast<CVehicleSeat*>(pVehicleSeat)->GetExitHelper())
									info.waitFor.push_back(it->second->GetSeatId());
							}
						}
					}
				}
			}
		}
	}

	if (CVehicleParams seatGroupsTable = vehicleParams.findChild("SeatGroups"))
	{
		int c = seatGroupsTable.getChildCount();
		int i = 0;

		m_seatGroups.reserve(c);

		for (; i < c; i++)
		{
			if (CVehicleParams seatGroupTable = vehicleParams.getChild(i))
			{
				CVehicleSeatGroup* pVehicleSeatGroup = new CVehicleSeatGroup;

				if (pVehicleSeatGroup->Init(this, seatGroupTable))
					m_seatGroups.push_back(pVehicleSeatGroup);
				else
					delete pVehicleSeatGroup;
			}
		}
	}

	if (CVehicleParams inventoryTable = vehicleParams.findChild("Inventory"))
	{
		if (CVehicleParams ammosTable = inventoryTable.findChild("AmmoTypes"))
		{
			int i = 0;
			int c = ammosTable.getChildCount();

			for (; i < c; i++)
			{
				if (CVehicleParams ammoTable = ammosTable.getChild(i))
				{
					string type;
					int capacity = 0;

					if (!(type = ammoTable.getAttr("type")).empty() && ammoTable.getAttr("capacity", capacity))
					{
						IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(type.c_str());

						if (pClass)
							SetAmmoCapacity(pClass, capacity);
					}
				}
			}
		}
	}

	InitPaint(vehicleParams);
	InitActions(vehicleParams);

	m_actionMap = vehicleParams.getAttr("actionMap");

	GetEntity()->AddFlags(ENTITY_FLAG_CASTSHADOW | ENTITY_FLAG_ON_RADAR | ENTITY_FLAG_CUSTOM_VIEWDIST_RATIO);
	//////////////////////////////////////////////////////////////////////////
	IEntityRenderProxy* pRenderProxy = (IEntityRenderProxy*)GetEntity()->GetProxy(ENTITY_PROXY_RENDER);
	if (pRenderProxy)
		pRenderProxy->GetRenderNode()->SetViewDistRatio(VEHICLE_VIEW_DIST_RATIO);
	//////////////////////////////////////////////////////////////////////////

	InitDamages(this, vehicleParams);

	if (VehicleCVars().v_debugdraw == eVDB_Parts)
	{
		for (TVehiclePartVector::iterator it = m_parts.begin(); it != m_parts.end(); ++it)
		{
			if (CVehiclePartAnimated* pPart = CAST_VEHICLEOBJECT(CVehiclePartAnimated, it->second))
				pPart->Dump();
		}
	}

	// calculate total damage
	m_damageMax = 0.0f;
	m_majorComponentDamageMax = 0.0f;
	for (TVehicleComponentVector::iterator ite = m_components.begin(); ite != m_components.end(); ++ite)
	{
		m_damageMax += (*ite)->GetMaxDamage();
		if ((*ite)->IsMajorComponent())
			m_majorComponentDamageMax += (*ite)->GetMaxDamage();
	}

	// each component should also record the percentage it contributes to the total vehicle health 
	if (m_majorComponentDamageMax > 0)
	{
		for (TVehicleComponentVector::iterator ite = m_components.begin(); ite != m_components.end(); ++ite)
		{
			if ((*ite)->IsMajorComponent())
			{
				float proportion = (*ite)->GetMaxDamage() / m_majorComponentDamageMax;
				(*ite)->SetProportionOfVehicleHealth(proportion);
			}
		}
	}

	// attach seat scriptbinds
	// this is done during OnSpawn when spawning initially, or here in case only the extension is created
	for (const auto &seat : m_seats)
	{
		//CryMP: vehicle.seat script binds 
		GameFramework::GetInstance()->GetScriptBind_VehicleSeat()->AttachTo(this, GetSeatId(seat.second));
	}

	if (0 == (GetEntity()->GetFlags() & (ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_SERVER_ONLY)))
	{
		if (!GetGameObject()->BindToNetwork())
		{
			CryLogWarning("<%s> BindToNetwork failed", GetEntity()->GetName());
			return false;
		}
	}

	return true;
}

//------------------------------------------------------------------------
void CVehicle::InitPaint(const CVehicleParams& xmlContent)
{
	// check and apply "paint"   
	if (IScriptTable* pScriptTable = GetEntity()->GetScriptTable())
	{
		if (CVehicleParams paintsTable = xmlContent.findChild("Paints"))
		{
			int i = 0;
			int c = paintsTable.getChildCount();

			for (; i < c; i++)
			{
				if (CVehicleParams paintRef = paintsTable.getChild(i))
				{
					string paintName = paintRef.getAttr("name");
					if (!paintName.empty() && paintName == m_paint)
					{
						string materialName = paintRef.getAttr("material");
						if (!materialName.empty())
						{
							IMaterial* pMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(materialName.c_str(), false);
							if (pMaterial)
								GetEntity()->SetMaterial(pMaterial);
							else
								CryLog("[CVehicle::Paint] <%s> Material %s for Paint %s not found", GetEntity()->GetName(), materialName.c_str(), m_paint.c_str());
						}
						m_destroyedMaterial = paintRef.getAttr("materialDestroyed");
						break;
					}
				}
			}
		}
	}
}

//------------------------------------------------------------------------
bool CVehicle::InitActions(const CVehicleParams& vehicleTable)
{
	CVehicleParams actionsTable = vehicleTable.findChild("Actions");
	if (!actionsTable)
		return false;

	int i = 0;
	int c = actionsTable.getChildCount();

	for (; i < c; i++)
	{
		if (CVehicleParams actionRef = actionsTable.getChild(i))
		{
			if (actionRef.haveAttr("class"))
			{
				string className = actionRef.getAttr("class");
				if (IVehicleAction* pAction = m_pVehicleSystem->CreateVehicleAction(className))
				{
					if (pAction->Init(this, actionRef))
					{
						m_actions.resize(m_actions.size() + 1);
						SVehicleActionInfo& actionInfo = m_actions.back();

						actionInfo.pAction = pAction;

						if (className == "Enter")
							m_usesVehicleActionToEnter = true;

						actionInfo.useWhenFlipped = (className == "Flip") ? 1 : 0;

						if (CVehicleParams activationsTable = actionRef.findChild("Activations"))
						{
							int n = 0;
							int numActivations = activationsTable.getChildCount();

							for (; n < numActivations; n++)
							{
								if (CVehicleParams activationRef = activationsTable.getChild(n))
								{
									actionInfo.activations.resize(actionInfo.activations.size() + 1);
									SActivationInfo& activationInfo = actionInfo.activations.back();

									string type = activationRef.getAttr("type");
									if (!type.empty())
									{
										if (type == "OnUsed")
											activationInfo.m_type = SActivationInfo::eAT_OnUsed;
										else if (type == "OnGroundCollision")
											activationInfo.m_type = SActivationInfo::eAT_OnGroundCollision;
									}

									string param1 = activationRef.getAttr("param1");
									if (!param1.empty())
									{
										if (param1 == "part")
											activationInfo.m_param1 = SActivationInfo::eAP1_Part;
										else if (param1 == "component")
											activationInfo.m_param1 = SActivationInfo::eAP1_Component;
									}

									activationInfo.m_param2 = activationRef.getAttr("param2");

									if (!activationRef.getAttr("distance", activationInfo.m_distance))
										activationInfo.m_distance = 1.5f;
								}
							}
						}
					}
				}
			}
		}
	}

	return true;
}

//------------------------------------------------------------------------
void CVehicle::PostInit(IGameObject* pGameObject)
{
	if (GetMovement())
		GetMovement()->PostInit();

	for (TVehiclePartVector::iterator ite = m_parts.begin(); ite != m_parts.end(); ite++)
	{
		IVehiclePart* part = ite->second;
		part->PostInit();
	}

	pGameObject->EnablePhysicsEvent(true, eEPE_OnCollisionLogged | eEPE_OnStateChangeLogged);
	//pGameObject->EnablePostUpdates(this);

  // not needed anymore - was required previously to make sure vehicle is not disabled when player drives
  //  for (int i=0; i<MAX_UPDATE_SLOTS_PER_EXTENSION; ++i)
  //    pGameObject->SetUpdateSlotEnableCondition(this, i, eUEC_WithoutAI);

	if (!gEnv->bServer)
	{
		pGameObject->SetUpdateSlotEnableCondition(this, eVUS_Always, eUEC_VisibleOrInRangeIgnoreAI);
		pGameObject->SetUpdateSlotEnableCondition(this, eVUS_EnginePowered, eUEC_Always); //CryMP: was eUEC_VisibleOrInRangeIgnoreAI;
		pGameObject->SetUpdateSlotEnableCondition(this, eVUS_PassengerIn, eUEC_VisibleOrInRangeIgnoreAI);
	}

	pGameObject->SetUpdateSlotEnableCondition(this, eVUS_Visible, eUEC_VisibleIgnoreAI);
	pGameObject->EnableUpdateSlot(this, eVUS_Visible);

	pGameObject->SetMovementController(GetMovementController());

	m_initialposition = GetEntity()->GetWorldPos();

	DumpParts();

	for (TVehicleSeatVector::iterator it = m_seats.begin(), end = m_seats.end(); it != end; ++it)
	{
		it->second->PostInit(this);
	}

	if (!gEnv->pGame->GetIGameFramework()->IsEditing() || IsDebugDrawing())
		NeedsUpdate();

	if (gEnv->bServer)
		SendWeaponSetup(eRMI_ToRemoteClients);

	if (VehicleCVars().v_debug_mem > 0)
	{
		ICrySizer* pSizer = gEnv->pSystem->CreateSizer();
		GetMemoryStatistics(pSizer);
		int vehicleSize = pSizer->GetTotalSize();
		CryLog("Vehicle initialized: <%s> takes %d bytes.", GetEntity()->GetName(), pSizer->GetTotalSize());

		pSizer->Release();
	}
}

//------------------------------------------------------------------------
void CVehicle::PostInitClient(int channelId)
{
	SendWeaponSetup(eRMI_ToClientChannel, channelId);
};

//------------------------------------------------------------------------
void CVehicle::DumpParts() const
{
	if (VehicleCVars().v_debugdraw == eVDB_PhysParts || VehicleCVars().v_debugdraw == eVDB_PhysPartsExt)
	{
		CryLog("============ %s =============", GetEntity()->GetName());

		IPhysicalEntity* pPhysics = GetEntity()->GetPhysics();
		pe_status_nparts nparts;
		int numParts = pPhysics->GetStatus(&nparts);
		CryLog("%i physical parts", numParts);
		CryLog("------------------");

		if (VehicleCVars().v_debugdraw == eVDB_PhysParts)
		{
			for (TVehiclePartVector::const_iterator ite = m_parts.begin(); ite != m_parts.end(); ++ite)
			{
				IVehiclePart* part = ite->second;
				int status = -1;
				pe_params_part params;

				if (part->GetPhysId() != -1)
				{
					params.partid = part->GetPhysId();
					status = pPhysics->GetParams(&params);
				}

				CryLog("Part <%-25s>: slot %2i, physId %8i, status %i", part->GetName().c_str(), part->GetSlot(), part->GetPhysId(), status);
			}
		}
		else
		{
			ICharacterInstance* pCharInstance = GetEntity()->GetCharacter(0);
			ISkeletonPose* pSkeletonPose = pCharInstance ? pCharInstance->GetISkeletonPose() : 0;

			for (int part = 0; part < numParts; ++part)
			{
				pe_status_pos status;
				Matrix34 tm;
				status.ipart = part;
				status.pMtx3x4 = &tm;
				if (pPhysics->GetStatus(&status))
				{
					Vec3& min = status.BBox[0];
					Vec3& max = status.BBox[1];
					CryLog("Part [%i]: partId %i, min: (%.2f, %.2f, %.2f), max: (%.2f, %.2f, %.2f)", part, status.partid, min.x, min.y, min.z, max.x, max.y, max.z);

					if (pSkeletonPose)
					{
						// quadratic, but well, we're debugging
						uint32 numJoints = pSkeletonPose->GetJointCount();
						for (uint32 i = 0; i < numJoints; ++i)
						{
							if (pSkeletonPose->GetPhysIdOnJoint(i) == status.partid)
								CryLog("partId %i matches Joint <%s>", status.partid, pSkeletonPose->GetJointNameByID(i));
						}
					}

					tm = GetEntity()->GetWorldTM().GetInverted() * tm;
					VehicleUtils::LogMatrix("", tm);
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CVehicle::SetAmmoCapacity(IEntityClass* pAmmoType, int capacity)
{
	if (m_pInventory)
		m_pInventory->SetAmmoCapacity(pAmmoType, capacity);
}

//------------------------------------------------------------------------
void CVehicle::SetAmmoCount(IEntityClass* pAmmoType, int amount)
{
	if (m_pInventory)
	{
		int oldAmount = m_pInventory->GetAmmoCount(pAmmoType);
		m_pInventory->SetAmmoCount(pAmmoType, amount);

		// if we didn't have any of this ammo before, and we do now, and we have a weapon inuse which uses this ammo type,
		// then trigger a reload
		if (oldAmount == 0 && amount != 0 && gEnv->bClient)
		{
			if (IItemSystem* pItemSystem = gEnv->pGame->GetIGameFramework()->GetIItemSystem())
			{
				for (int i = 0; i < GetWeaponCount(); ++i)
				{
					IItem* pItem = pItemSystem->GetItem(GetWeaponId(i));
					if (pItem && pItem->GetOwnerId() == gEnv->pGame->GetIGameFramework()->GetClientActorId())
					{
						if (IWeapon* pWeapon = pItem->GetIWeapon())
						{
							if (IFireMode* pfm = pWeapon->GetFireMode(pWeapon->GetCurrentFireMode()))
							{
								if (pfm->GetAmmoType() == pAmmoType)
								{
									pWeapon->Reload(false);
									float time = 0;
									float rate = pfm->GetFireRate();
									if (rate > 0.0f)
										time = 60.0f / rate;
									pfm->SetNextShotTime(time);
									break;
								}
							}
						}
					}
				}
			}
		}
	}

	if (gEnv->bServer)
		GetGameObject()->InvokeRMI(ClSetAmmo(), AmmoParams(pAmmoType->GetName(), amount), eRMI_ToRemoteClients);
}

//------------------------------------------------------------------------
int CVehicle::GetAmmoCount(IEntityClass* pAmmoType) const
{
	if (m_pInventory)
		return m_pInventory->GetAmmoCount(pAmmoType);

	return 0;
}

//------------------------------------------------------------------------
int CVehicle::GetAmmoCapacity(IEntityClass* pAmmoType) const //CryMP: added
{
	if (m_pInventory)
		return m_pInventory->GetAmmoCapacity(pAmmoType);

	return 0;
}

//------------------------------------------------------------------------
void CVehicle::SendWeaponSetup(int where, int channelId)
{
	SetupWeaponsParams params;
	for (TVehicleSeatVector::iterator it = m_seats.begin(); it != m_seats.end(); ++it)
	{
		SetupWeaponsParams::SeatWeaponParams seatparams;
		CVehicleSeat* pSeat = it->second;

		TVehicleSeatActionVector& seatActions = pSeat->GetSeatActions();
		for (TVehicleSeatActionVector::iterator ait = seatActions.begin(); ait != seatActions.end(); ++ait)
		{
			IVehicleSeatAction* pSeatAction = ait->pSeatAction;
			if (CVehicleSeatActionWeapons* weapAction = CAST_VEHICLEOBJECT(CVehicleSeatActionWeapons, pSeatAction))
			{
				SetupWeaponsParams::SeatWeaponParams::SeatActionWeaponParams actionParams;

				int n = weapAction->GetWeaponCount();
				for (int w = 0;w < n;w++)
					actionParams.weapons.push_back(weapAction->GetWeaponEntityId(w));

				seatparams.seatactions.push_back(actionParams);
			}
		}

		params.seats.push_back(seatparams);
	}

	GetGameObject()->InvokeRMI(ClSetupWeapons(), params, where, channelId);
}


//------------------------------------------------------------------------
TVehicleSoundEventId CVehicle::AddSoundEvent(SVehicleSoundInfo& info)
{
	m_soundEvents.push_back(info);

	return TVehicleSoundEventId(m_soundEvents.size() - 1);
}

//------------------------------------------------------------------------
void CVehicle::ProcessEvent(SEntityEvent& entityEvent)
{
	switch (entityEvent.event)
	{
	case ENTITY_EVENT_RESET:
		Reset(entityEvent.nParam[0] == 1 ? true : false);
		break;

	case ENTITY_EVENT_DONE:
		break;

	case ENTITY_EVENT_TIMER:
		OnTimer((int)entityEvent.nParam[0]);
		break;

	case ENTITY_EVENT_MATERIAL_LAYER:
		OnMaterialLayerChanged(entityEvent);
		break;
	}

	if (m_pMovement)
		m_pMovement->ProcessEvent(entityEvent);
}

//------------------------------------------------------------------------
void CVehicle::OnMaterialLayerChanged(const SEntityEvent& event)
{
	// only handle frozen layer for now.  
	bool frozen = event.nParam[0] & MTL_LAYER_FROZEN;
	bool prev = event.nParam[1] & MTL_LAYER_FROZEN;
	if (frozen != prev)
	{
		int n = GetEntity()->GetChildCount();
		for (int i = 0; i < n; ++i)
		{
			IEntity* pChild = GetEntity()->GetChild(i);
			IEntityRenderProxy* pRenderProxy = (IEntityRenderProxy*)pChild->GetProxy(ENTITY_PROXY_RENDER);
			if (pRenderProxy)
			{
				if (IActor* pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pChild->GetId()))
					if (pActor->IsPlayer()) // don't freeze players inside vehicles
						continue;

				uint8 mask = pRenderProxy->GetMaterialLayersMask();
				pRenderProxy->SetMaterialLayersMask(frozen ? mask | MTL_LAYER_FROZEN : mask & ~MTL_LAYER_FROZEN);
			}
		}
	}
}

//------------------------------------------------------------------------
int CVehicle::SetTimer(int timerId, int ms, IVehicleObject* pObject)
{
	if (timerId == -1)
	{
		if (!m_timers.empty())
		{
			TVehicleTimerMap::const_reverse_iterator last = m_timers.rbegin();
			timerId = last->first + 1;
		}
		else
			timerId = eVT_Last;
	}
	else
		KillTimer(timerId);

	if (m_timers.insert(std::make_pair(timerId, pObject)).second)
	{
		GetEntity()->SetTimer(timerId, ms);
		//CryLog("vehicle <%s> setting timer %i, %i ms", GetEntity()->GetName(), timerId, ms);

		return timerId;
	}

	return -1;
}

//------------------------------------------------------------------------
int CVehicle::KillTimer(int timerId)
{
	if (timerId != -1)
	{
		GetEntity()->KillTimer(timerId);
		m_timers.erase(timerId);
	}

	return -1;
}

//------------------------------------------------------------------------
void CVehicle::KillTimers()
{
	KillAbandonedTimer();

	GetEntity()->KillTimer(-1);
	m_timers.clear();
}

//------------------------------------------------------------------------
void CVehicle::OnTimer(int timerId)
{
	TVehicleTimerMap::iterator it = m_timers.find(timerId);
	if (it != m_timers.end())
	{
		IVehicleObject* pObject = it->second;
		m_timers.erase(it);

		if (pObject)
		{
			SVehicleEventParams params;
			params.iParam = timerId;
			pObject->OnVehicleEvent(eVE_Timer, params);
		}
	}

	switch (timerId)
	{
	case eVT_Abandoned:
		Destroy();
		break;
	case eVT_AbandonedSound:
		EnableAbandonedWarnSound(true);
		break;
	case eVT_Flipped:
		ProcessFlipped();
		break;
	}
}


void CVehicle::Reset(bool enterGame)
{
	KillTimers();

	m_ownerId = 0;

	// disable engine slot in case it was enabled by speed threshold
	if (m_customEngineSlot)
	{
		GetGameObject()->DisableUpdateSlot(this, eVUS_EnginePowered);
		m_customEngineSlot = false;
	}

	ResetDamages();

	for (TVehiclePartVector::iterator ite = m_parts.begin(); ite != m_parts.end(); ite++)
	{
		IVehiclePart* part = ite->second;
		part->Reset();
	}

	for (TVehicleSeatVector::iterator ite = m_seats.begin(); ite != m_seats.end(); ite++)
	{
		CVehicleSeat* pSeat = ite->second;
		pSeat->Reset();
	}

	for (TVehicleComponentVector::iterator ite = m_components.begin(); ite != m_components.end(); ++ite)
	{
		CVehicleComponent* pComponent = *ite;
		pComponent->Reset();
	}

	for (TVehicleAnimationsVector::iterator ite = m_animations.begin(); ite != m_animations.end(); ++ite)
	{
		IVehicleAnimation* pAnimation = ite->second;
		pAnimation->Reset();
	}

	for (TVehicleActionVector::iterator ite = m_actions.begin(); ite != m_actions.end(); ++ite)
	{
		SVehicleActionInfo& actionInfo = *ite;
		actionInfo.pAction->Reset();
	}

	for (TVehicleSoundEventId id = 0; id < m_soundEvents.size(); ++id)
		StopSound(id);

	m_isDestroyed = false;
	m_status.Reset();

	m_initialposition = GetEntity()->GetWorldPos();

	DoRequestedPhysicalization();

	if (enterGame)
	{
		// activate when going to game, to adjust vehicle to terrain etc.
		// will be deactivated some frames later    
		NeedsUpdate(eVUF_AwakePhysics);
	}
	else
	{
		m_bNeedsUpdate = false;

		for (int i = 0; i < eVUS_Visible; ++i)
		{
			while (GetGameObject()->GetUpdateSlotEnables(this, i) > 0)
				GetGameObject()->DisableUpdateSlot(this, i);
		}
	}

	m_collisionDisabledTime = 0.0f;
	m_indestructible = false;
}

//------------------------------------------------------------------------
void CVehicle::RequestPhysicalization(IVehiclePart* pPart, bool request)
{
	if (request)
		m_physicalizeParts.insert(pPart);
	else
		stl::find_and_erase(m_physicalizeParts, pPart);
}

//------------------------------------------------------------------------
void CVehicle::DoRequestedPhysicalization()
{
	if (!m_physicalizeParts.empty())
	{
		TVehiclePartSet parts(m_physicalizeParts);
		for (TVehiclePartSet::iterator it = parts.begin(), end = parts.end(); it != end; ++it)
		{
			(*it)->Physicalize();
		}

		m_physicalizeParts.clear();
	}
}


//------------------------------------------------------------------------
void CVehicle::Update(SEntityUpdateContext& ctx, int slot)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	const float frameTime = ctx.fFrameTime;
	gEnv->pRenderer->GetIRenderAuxGeom()->SetRenderFlags(e_Def3DPublicRenderflags);

	if (ctx.nFrameID != m_lastFrameId)
	{
		m_bNeedsUpdate = false;
		m_lastFrameId = ctx.nFrameID;
	}

	switch (slot)
	{
	case eVUS_Always:
	{
		m_debugIndex = 0;

		if (!GetEntity()->GetPhysics())
			return;

		UpdateHelpers();
		UpdateStatus(frameTime);
		UpdateDamages(frameTime);

		for (TVehicleObjectUpdateInfoList::iterator ite = m_objectsToUpdate.begin(), end = m_objectsToUpdate.end(); ite != end; ++ite)
		{
			SObjectUpdateInfo& updateInfo = *ite;
			if (updateInfo.updatePolicy == eVOU_AlwaysUpdate)
				updateInfo.pObject->Update(frameTime);
		}

		if (m_collisionDisabledTime > 0.0f)
			m_collisionDisabledTime -= frameTime;
	}
	break;

	case eVUS_EnginePowered:
	{
		NeedsUpdate(eVUF_AwakePhysics);

		if (m_pMovement)
		{
			m_pMovement->Update(frameTime);
		}

		break;
	}

	case eVUS_PassengerIn:
	{
		NeedsUpdate();

		for (TVehicleSeatVector::iterator ite = m_seats.begin(), end = m_seats.end(); ite != end; ++ite)
		{
			CVehicleSeat* seat = ite->second;
			seat->Update(frameTime);
		}

		for (TVehicleObjectUpdateInfoList::iterator ite = m_objectsToUpdate.begin(), end = m_objectsToUpdate.end(); ite != end; ++ite)
		{
			SObjectUpdateInfo& updateInfo = *ite;
			if (updateInfo.updatePolicy == eVOU_PassengerUpdate)
				updateInfo.pObject->Update(frameTime);
		}

		break;
	}

	case eVUS_Visible:
	{
		for (TVehicleObjectUpdateInfoList::iterator ite = m_objectsToUpdate.begin(), end = m_objectsToUpdate.end(); ite != end; ++ite)
		{
			SObjectUpdateInfo& updateInfo = *ite;
			if (updateInfo.updatePolicy == eVOU_Visible)
				updateInfo.pObject->Update(frameTime);
		}

		DebugDraw(frameTime);
	}
	}

	CheckDisableUpdate(slot);
}

//------------------------------------------------------------------------
void CVehicle::PostUpdate(float frameTime)
{
	//DebugDraw(frameTime);
}

//------------------------------------------------------------------------
void CVehicle::NeedsUpdate(int flags, bool bThreadSafe/*=false*/)
{
	m_bNeedsUpdate = true;

	if (0 == GetGameObject()->GetUpdateSlotEnables(this, eVUS_Always))
	{
		// enable, if necessary
		GetGameObject()->EnableUpdateSlot(this, eVUS_Always);
	}

	if (flags & eVUF_AwakePhysics)
	{
		if (IPhysicalEntity* pPE = GetEntity()->GetPhysics())
		{
			pe_action_awake awakeParams;
			awakeParams.bAwake = 1;
			pPE->Action(&awakeParams, (int)bThreadSafe);
		}
	}
}

//------------------------------------------------------------------------
void CVehicle::CheckDisableUpdate(int slot)
{
	if (m_bNeedsUpdate)
		return;

	if (0 == CVehicleCVars::Get().v_autoDisable || IsDebugDrawing())
		return;

	// check after last updated slot if we can be disabled  
	for (int i = eVUS_Visible - 1; i >= 0; --i)
	{
		if (GetGameObject()->GetUpdateSlotEnables(this, i))
		{
			if (slot >= i)
			{
				if (IPhysicalEntity* pPhysics = GetEntity()->GetPhysics())
				{
					pe_status_pos status;
					if (pPhysics->GetStatus(&status) && status.iSimClass > 1)
						break;
				}

				GetGameObject()->DisableUpdateSlot(this, eVUS_Always);
			}
			break;
		}
	}
}

//------------------------------------------------------------------------
void CVehicle::OnPhysStateChange(EventPhysStateChange* pEvent)
{
	if (pEvent->iSimClass[0] < 2 && pEvent->iSimClass[1] > 1)
	{
		// awakened
		NeedsUpdate();
	}
	else if (pEvent->iSimClass[0] > 1 && pEvent->iSimClass[1] < 2)
	{
		// falling asleep         

		// do one update pass on visibility-based objects
		for (TVehicleObjectUpdateInfoList::iterator ite = m_objectsToUpdate.begin(); ite != m_objectsToUpdate.end(); ++ite)
		{
			SObjectUpdateInfo& updateInfo = *ite;
			if (updateInfo.updatePolicy == eVOU_Visible)
				updateInfo.pObject->Update(0.f);
		}
	}
}


//------------------------------------------------------------------------
void CVehicle::DebugDraw(const float frameTime)
{
	if (IsDebugDrawing())
		NeedsUpdate();

	//if (GetEntity()->GetWorldPos().GetSquaredDistance(gEnv->pSystem->GetViewCamera().GetPosition()) > sqr(100.f))
	//	return;

	CVehicleCVars& cvars = CVehicleCVars::Get();

	if (cvars.v_draw_components)
	{
		for (TVehicleComponentVector::const_iterator ite = m_components.begin(); ite != m_components.end(); ++ite)
		{
			CVehicleComponent* pComponent = *ite;
			pComponent->DebugDraw();
		}
	}

	if (cvars.v_debugdraw == eVDB_Parts)
	{
		static float drawColor[4] = { 1,1,1,1 };

		for (TVehiclePartVector::iterator ite = m_parts.begin(); ite != m_parts.end(); ++ite)
		{
			gEnv->pRenderer->DrawLabelEx(ite->second->GetWorldTM().GetTranslation(), 1.0f, drawColor, true, true, "<%s>", ite->first.c_str());

			/*IRenderAuxGeom* pGeom = gEnv->pRenderer->GetIRenderAuxGeom();
			AABB bounds = AABB::CreateTransformedAABB(ite->second->GetWorldTM(), ite->second->GetLocalBounds());
			ColorB col(0,255,0,255);
			pGeom->DrawAABB(bounds, false, col, eBBD_Extremes_Color_Encoded);*/
		}
	}

	if (cvars.v_draw_helpers)
	{
		static float drawColor[4] = { 1,0,0,1 };

		IPersistantDebug* pDB = gEnv->pGame->GetIGameFramework()->GetIPersistantDebug();
		CryFixedStringT<32> dbgName;
		dbgName.Format("VehicleHelpers_%s", GetEntity()->GetName());
		pDB->Begin(dbgName.c_str(), true);
		for (TVehicleHelperMap::const_iterator ite = m_helpers.begin(); ite != m_helpers.end(); ++ite)
		{
			const Matrix34 tm = ite->second->GetWorldTM();
			gEnv->pRenderer->DrawLabelEx(tm.GetTranslation(), 1.0f, drawColor, true, true, "<%s>", ite->first.c_str());
			pDB->AddDirection(tm.GetTranslation(), 0.25f, tm.GetColumn(1), ColorF(1, 1, 0, 1), 0.05f);
		}
	}

	if (cvars.v_draw_seats)
	{
		float seatColor[] = { 0.5,1,1,1 };
		//    float enterColor[] = {0.5,1,1,0.5};
		for (TVehicleSeatVector::const_iterator it = m_seats.begin(), end = m_seats.end(); it != end; ++it)
		{
			Vec3 pos(0);
			if (IVehicleHelper* pHelper = it->second->GetSitHelper())
				pos = pHelper->GetVehicleTM().GetTranslation();

			gEnv->pRenderer->DrawLabelEx(GetEntity()->GetWorldTM() * pos, 1.1f, seatColor, true, true, "[%s]", it->second->GetName().c_str());

			if (IVehicleHelper* pHelper = it->second->GetEnterHelper())
				gEnv->pRenderer->DrawLabelEx(GetEntity()->GetWorldTM() * pHelper->GetVehicleTM().GetTranslation(), 1.0f, seatColor, true, true, "[%s enter]", it->second->GetName().c_str());
		}
	}

	if (cvars.v_debugdraw == eVDB_Sounds && IsPlayerPassenger())
	{
		static float color[] = { 1,1,1,1 };

		for (TVehicleSoundEventId i = 0; i < m_soundEvents.size(); ++i)
		{
			if (SVehicleSoundInfo* info = GetSoundInfo(i))
			{
				if (ISound* pSound = m_pSoundProxy->GetSound(info->soundId))
				{
					float speed = 0.f;
					if (pSound->GetParam("speed", &speed, false))
					{
						gEnv->pRenderer->Draw2dLabel(50.f, (float)(100 + 15 * GetDebugIndex()), 1.25f, color, false, "%s: speed %.2f", info->name.c_str(), speed);
					}
				}
			}
		}
	}
}

//------------------------------------------------------------------------
bool CVehicle::IsDebugDrawing()
{
	CVehicleCVars& cvars = CVehicleCVars::Get();

	return (cvars.v_draw_components || cvars.v_draw_helpers || cvars.v_debugdraw);
}

//------------------------------------------------------------------------
void CVehicle::UpdateHelpers()
{
	TVehicleHelperMap::iterator helperIteEnd = m_helpers.end();
	for (TVehicleHelperMap::iterator helperIte = m_helpers.begin(); helperIte != helperIteEnd; helperIte++)
	{
		const string& name = helperIte->first;
		CVehicleHelper* pHelper = helperIte->second;
		assert(pHelper);

		pHelper->m_isWorldUpdated = false;
		pHelper->m_isVehicleUpdated = false;
	}
}

//------------------------------------------------------------------------
void CVehicle::UpdateView(SViewParams& viewParams, EntityId playerId)
{
	const CVehicleCVars& vars = VehicleCVars();
	if (vars.v_debugViewDetach == 1) return;

	if (vars.v_debugViewDetach == 2)
	{
		IEntity* pEntity = GetEntity();
		Vec3 entityPos = pEntity->GetPos();
		Vec3 dir = (entityPos - viewParams.position);
		float radius = dir.GetLength();
		if (radius > 0.01f)
		{
			dir = dir * (1.f / radius);
			viewParams.rotation = Quat::CreateRotationVDir(dir);
		}
		return;
	}
	else if (vars.v_debugViewAbove)
	{
		IEntity* pEntity = GetEntity();
		viewParams.position = pEntity->GetPos();
		viewParams.position.z += vars.v_debugViewAboveH;
		viewParams.rotation = Quat(0.7071067811865475f, -0.7071067811865475f, 0.f, 0.f);
		return;
	}

	for (TVehicleSeatVector::iterator ite = m_seats.begin(); ite != m_seats.end(); ite++)
	{
		CVehicleSeat* seat = ite->second;
		if (seat->GetPassenger() == playerId && seat->GetCurrentTransition() != CVehicleSeat::eVT_RemoteUsage)
		{
			seat->UpdateView(viewParams);
			return;
		}
	}
}

//------------------------------------------------------------------------
void CVehicle::HandleEvent(const SGameObjectEvent& event)
{
	if (event.event == eGFE_OnPostStep)
	{
		OnPhysPostStep((EventPhys*)event.ptr);
	}
	else if (event.event == eGFE_OnCollision)
	{
		EventPhysCollision* pCollision = static_cast<EventPhysCollision*>(event.ptr);
		OnCollision(pCollision);
	}
	else if (event.event == eGFE_OnStateChange)
	{
		EventPhysStateChange* pStateChange = static_cast<EventPhysStateChange*>(event.ptr);
		OnPhysStateChange(pStateChange);
	}
	else if (event.event == eGFE_OnBecomeVisible)
	{
		if (m_pMovement)
		{
			SVehicleMovementEventParams params;
			m_pMovement->OnEvent(IVehicleMovement::eVME_BecomeVisible, params);
		}
	}
	else if (event.event == eGFE_OnBreakable2d)
	{
		const EventPhysCollision* pCEvent = (const EventPhysCollision*)event.ptr;
		ISurfaceType* pSurface = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceType(pCEvent->idmat[1]);
		IStatObj* pStatObj = (IStatObj*)event.param;

		if (pStatObj && pSurface && pSurface == sSurfaceMatGlassVehicle)
		{
			if (!m_pBrokenWindowMat)
			{
				if (IMaterial* pMat = pStatObj->GetMaterial())
				{
					for (int i = 0; i < pMat->GetSubMtlCount(); ++i)
					{
						IMaterial* pSubMat = pMat->GetSubMtl(i);

						if (0 == strcmp(pSubMat->GetName(), "broken_windowpanes"))
						{
							m_pBrokenWindowMat = pSubMat;
							break;
						}
					}
				}
			}

			if (m_pBrokenWindowMat)
			{
				int partid = pCEvent->partid[1];
				if (partid >= PARTID_CGA)
				{
					int slot = partid / PARTID_CGA - 1;
					int jointId = partid % PARTID_CGA;
					if (ICharacterInstance* pCharInstance = GetEntity()->GetCharacter(slot))
					{
						pCharInstance->GetISkeletonPose()->SetMaterialOnJoint(jointId, m_pBrokenWindowMat);

						if (VehicleCVars().v_debugdraw == eVDB_Parts)
							CryLog("%s: setting mat %s on slot %i, joint %i (%s)", GetEntity()->GetName(), m_pBrokenWindowMat->GetName(), slot, jointId, pCharInstance->GetISkeletonPose()->GetJointNameByID(jointId));
					}
				}
				else
				{
					GetEntity()->SetSlotMaterial(partid, m_pBrokenWindowMat);

					if (VehicleCVars().v_debugdraw == eVDB_Parts)
						CryLog("%s: setting window mat on slot %i", GetEntity()->GetName(), partid);
				}
			}
		}
	}
	else if (event.event == eGFE_PreFreeze)
	{
		IVehicleMovement* pMovement = GetMovement();
		if (pMovement)
		{
			SVehicleMovementEventParams params;
			params.fValue = event.param ? 1.0f : 0.0f;
			pMovement->OnEvent(IVehicleMovement::eVME_Freeze, params);
		}

		m_frozen = event.param != 0;

		// freeze all the weapons
		for (TVehicleSeatVector::iterator it = m_seats.begin(); it != m_seats.end(); ++it)
		{
			CVehicleSeat* pSeat = it->second;
			TVehicleSeatActionVector& seatActions = pSeat->GetSeatActions();
			for (TVehicleSeatActionVector::iterator ait = seatActions.begin(); ait != seatActions.end(); ++ait)
			{
				IVehicleSeatAction* pSeatAction = ait->pSeatAction;
				if (CVehicleSeatActionWeapons* weapAction = CAST_VEHICLEOBJECT(CVehicleSeatActionWeapons, pSeatAction))
				{
					int n = weapAction->GetWeaponCount();
					for (int w = 0;w < n;w++)
					{
						EntityId weaponId = weapAction->GetWeaponEntityId(w);
						if (IItem* pItem = gEnv->pGame->GetIGameFramework()->GetIItemSystem()->GetItem(weaponId))
							pItem->GetGameObject()->SendEvent(event);
					}
				}
			}
		}

		this->GetGameObject()->ChangedNetworkState(ASPECT_FROZEN);
	}
	else if (event.event == eGFE_PreShatter)
	{
		// evacuate all the passengers
		for (TVehicleSeatVector::iterator it = m_seats.begin(), end = m_seats.end(); it != end; ++it)
		{
			if (it->second->GetPassenger())
				it->second->Exit(false, true);
		}

		OnDestroyed();
		SetDestroyedStatus(true);
	}
}


//------------------------------------------------------------------------
void CVehicle::UpdateStatus(const float deltaTime)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	int frameId = gEnv->pRenderer->GetFrameID();
	if (!(frameId > m_status.frameId))
		return;

	m_status.frameId = frameId;
	IPhysicalEntity* pPhysics = GetEntity()->GetPhysics();

	m_status.altitude = 0.0f;

	m_status.passengerCount = 0;
	for (TVehicleSeatVector::iterator ite = m_seats.begin(), end = m_seats.end(); ite != end; ++ite)
	{
		CVehicleSeat* seat = ite->second;
		if (seat && seat->GetPassenger() && seat->GetCurrentTransition() != CVehicleSeat::eVT_RemoteUsage)
		{
			++m_status.passengerCount;
		}
	}

	if (IsDestroyed())
		m_status.health = 0.f;

	if (m_status.frozenAmount > 0.f && !m_frozen)
	{
		m_status.frozenAmount = max(0.f, m_status.frozenAmount - 0.1f * deltaTime); //todo: speed?
		NeedsUpdate();
	}

	if (m_bRetainGravity)
	{
		pe_simulation_params simParams;
		simParams.gravity = m_gravity;
		GetEntity()->GetPhysics()->SetParams(&simParams);
	}

	pe_status_dynamics dyn;
	if (pPhysics->GetStatus(&dyn))
	{
		float speed = dyn.v.len();

		if (m_pMovement && TriggerEngineSlotBySpeed())
		{
			// we enable/disable EnginePowered slot when speed crosses 
			// this threshold, so the engine/movement update can also be 
			// executed when engine itself is off

			float minSpeed = ENGINESLOT_MINSPEED;

			// todo: add a small timer here
			if (speed > minSpeed && !m_customEngineSlot)
			{
				GetGameObject()->EnableUpdateSlot(this, IVehicle::eVUS_EnginePowered);
				m_customEngineSlot = true;
			}
			else if (speed <= minSpeed && m_customEngineSlot)
			{
				GetGameObject()->DisableUpdateSlot(this, IVehicle::eVUS_EnginePowered);
				m_customEngineSlot = false;

				SVehicleMovementEventParams params;
				params.bValue = false;
				m_pMovement->OnEvent(IVehicleMovement::eVME_ToggleEngineUpdate, params);
			}
		}

		float preSubmerged = m_status.submergedRatio;

		m_status.speed = speed;
		m_status.vel = dyn.v;
		m_status.submergedRatio = dyn.submergedFraction;

		pe_status_collisions collisions;
		collisions.pHistory = 0;
		collisions.len = 0;
		collisions.age = 1.f;

		m_status.contacts = pPhysics->GetStatus(&collisions);

		const SVehicleDamageParams& damageParams = m_pVehicle->GetDamageParams();
		float max = damageParams.submergedRatioMax - 0.001f;
		if (m_status.submergedRatio >= max && preSubmerged < max)
			OnSubmerged(m_status.submergedRatio);
	}

	// check flipped over status      
	CheckFlippedStatus(deltaTime);
}

//------------------------------------------------------------------------
const SVehicleStatus& CVehicle::GetStatus()
{
	return m_status;
}

//------------------------------------------------------------------------
// AI related

//------------------------------------------------------------------------
IMovementController* CVehicle::GetMovementController()
{
	if (m_seats.empty())
		return NULL;

	return m_seats[0].second;
}

//------------------------------------------------------------------------
IMovementController* CVehicle::GetPassengerMovementController(EntityId passengerId)
{
	CVehicleSeat* pSeat = (CVehicleSeat*)GetSeatForPassenger(passengerId);

	if (pSeat)
		return pSeat->GetMovementController();

	return NULL;
}

//------------------------------------------------------------------------
IFireController* CVehicle::GetFireController()
{
	if (m_seats.empty())
		return NULL;

	return m_seats[0].second;
}

bool CVehicle::NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags)
{
	if (aspect == ASPECT_FROZEN)
		ser.Value("frozen", m_frozen, 'bool');

	if (m_pMovement)
		m_pMovement->Serialize(ser, aspect);

	// components
	float prevDmgRatio = (aspect == ASPECT_COMPONENT_DAMAGE) ? GetDamageRatio() : 0.f;

	for (TVehicleComponentVector::iterator itc = m_components.begin(); itc != m_components.end(); ++itc)
		(*itc)->Serialize(ser, aspect);

	if (aspect == ASPECT_COMPONENT_DAMAGE && ser.IsReading())
	{
		float deltaRatio = GetDamageRatio() - prevDmgRatio;
		if (abs(deltaRatio) > 0.001f)
		{
			EVehicleEvent event = (deltaRatio > 0.f) ? eVE_Damaged : eVE_Repair;
			SVehicleEventParams eventParams;
			eventParams.fParam = deltaRatio * m_damageMax;
			BroadcastVehicleEvent(event, eventParams);
		}
	}

	// before reading in the seat passengers, we generate a map of the current passenger for each sit
	// so that we can then tell who left the vehicle after reading in the new passenger positions

	m_ser_passengers.clear();
	if (ser.IsReading())
	{
		for (TVehicleSeatVector::iterator its = m_seats.begin(); its != m_seats.end(); ++its)
			if (its->second->GetPassenger())
				m_ser_passengers.insert(std::map<EntityId, int>::value_type(its->second->GetPassenger(), its->second->GetSeatId()));
	}

	for (TVehicleSeatVector::iterator its = m_seats.begin(); its != m_seats.end(); ++its)
		(its->second)->Serialize(ser, aspect);

	if ((aspect & ASPECT_SEAT_PASSENGER) && ser.IsReading())
	{
		for (TVehicleSeatVector::iterator its = m_seats.begin(); its != m_seats.end(); ++its)
		{
			CVehicleSeat* pSeat = its->second;
			if (pSeat && pSeat->GetPassenger())
				pSeat->SetAnimGraphInputs(pSeat->GetPassenger(), true, false);
		}
	}

	if (aspect == eEA_Physics)
	{
		if (profile == eVPhys_DemoRecording)
		{
			uint8 currentProfile = 255;
			if (ser.IsWriting())
				currentProfile = GetGameObject()->GetAspectProfile(eEA_Physics);
			ser.Value("PhysicalizationProfile", currentProfile);
			return NetSerialize(ser, aspect, currentProfile, flags);
		}

		pe_type type = PE_NONE;

		switch (profile)
		{
		case eVPhys_Physicalized:
		{
			type = PE_RIGID;
			if (m_pMovement && m_pMovement->GetPhysicalizationType() != PE_NONE)
				type = m_pMovement->GetPhysicalizationType();
		}
		break;
		case eVPhys_NotPhysicalized:
			type = PE_NONE;
			break;
		}

		if (type == PE_NONE)
			return true;

		IEntityPhysicalProxy* pEPP = (IEntityPhysicalProxy*)GetEntity()->GetProxy(ENTITY_PROXY_PHYSICS);
		if (!pEPP && ser.IsWriting())
		{
			gEnv->pPhysicalWorld->SerializeGarbageTypedSnapshot(ser, type, 0);
			return true;
		}
		else if (!pEPP)
			return false;

		pEPP->SerializeTyped(ser, type, flags);
	}

	return true;
}

//------------------------------------------------------------------------
void CVehicle::FullSerialize(TSerialize ser)
{
	ser.Value("indestructible", m_indestructible);
	bool isDestroyed = IsDestroyed();
	ser.Value("isDestroyed", isDestroyed, 'bool');

	if (ser.IsReading() && isDestroyed != IsDestroyed())
	{
		SetDestroyedStatus(isDestroyed);
	}

	ser.BeginGroup("Status");

	EEntityAspects eEA_All = EEntityAspects(EEntityAspects::eEA_GameClientDynamic | EEntityAspects::eEA_GameServerStatic 
		| EEntityAspects::eEA_GameServerDynamic | EEntityAspects::eEA_GameClientStatic | EEntityAspects::eEA_Physics); //CryMP: fixme

	m_status.Serialize(ser, eEA_All);
	ser.EndGroup();

	ser.BeginGroup("Components");
	for (TVehicleComponentVector::iterator ite = m_components.begin(); ite != m_components.end(); ++ite)
	{
		CVehicleComponent* pComponent = *ite;
		ser.BeginGroup(pComponent->GetName().c_str());
		pComponent->Serialize(ser, eEA_All);
		ser.EndGroup();
	}
	ser.EndGroup();

	for (TVehicleDamagesGroupVector::iterator ite = m_damagesGroups.begin(); ite != m_damagesGroups.end(); ++ite)
	{
		CVehicleDamagesGroup* pDamageGroup = *ite;
		ser.BeginGroup("DamageGroup");
		pDamageGroup->Serialize(ser, eEA_All);
		ser.EndGroup();
	}

	for (TVehiclePartVector::iterator it = m_parts.begin(); it != m_parts.end(); ++it)
	{
		ser.BeginGroup("Part");
		it->second->Serialize(ser, eEA_All);
		ser.EndGroup();
	}

	if (m_pMovement)
	{
		ser.BeginGroup("Movement");
		m_pMovement->Serialize(ser, eEA_All);
		ser.EndGroup();
	}

	ser.BeginGroup("Seats");
	for (TVehicleSeatVector::iterator it = m_seats.begin(); it != m_seats.end(); ++it)
	{
		ser.BeginGroup(it->first.c_str());
		(it->second)->Serialize(ser, eEA_All);
		ser.EndGroup();
	}
	ser.EndGroup();

	ser.BeginGroup("Actions");
	TVehicleActionVector::iterator actionIte = m_actions.begin();
	TVehicleActionVector::iterator actionEnd = m_actions.end();
	for (; actionIte != actionEnd; ++actionIte)
	{
		SVehicleActionInfo& actionInfo = *actionIte;
		ser.BeginGroup("Actions");
		actionInfo.pAction->Serialize(ser, eEA_All);
		ser.EndGroup();
	}
	ser.EndGroup();

	if (ser.IsReading())
		DoRequestedPhysicalization();
}

//------------------------------------------------------------------------
void CVehicle::PostSerialize()
{
	for (TVehiclePartVector::iterator it = m_parts.begin(), end = m_parts.end(); it != end; ++it)
		it->second->PostSerialize();

	for (TVehicleSeatVector::iterator it = m_seats.begin(), end = m_seats.end(); it != end; ++it)
		it->second->PostSerialize();

	if (m_pMovement)
		m_pMovement->PostSerialize();
}

//------------------------------------------------------------------------
void CVehicle::OnAction(const TVehicleActionId actionId, int activationMode, float value, EntityId callerId)
{
	if (CVehicleSeat* pCurrentSeat = (CVehicleSeat*)GetSeatForPassenger(callerId))
	{
		if (!m_frozen)
			pCurrentSeat->OnAction(actionId, activationMode, value);

		bool performAction = pCurrentSeat->GetCurrentTransition() == CVehicleSeat::eVT_None;
		if (!performAction)
		{
			IActorSystem* pActorSystem = gEnv->pGame->GetIGameFramework()->GetIActorSystem();
			IActor* pActor = pActorSystem->GetActor(callerId);
			if (pActor && pActor->IsClient())
				performAction = (pCurrentSeat->GetCurrentTransition() == CVehicleSeat::eVT_Entering && actionId != eVAI_Exit);
		}
		if (performAction)
		{
			if (actionId == eVAI_ChangeSeat && activationMode == eAAM_OnPress)
				GetGameObject()->InvokeRMI(SvRequestChangeSeat(), RequestChangeSeatParams(callerId, 0), eRMI_ToServer);
			else if (actionId == eVAI_ChangeSeat1 && activationMode == eAAM_OnPress)
				GetGameObject()->InvokeRMI(SvRequestChangeSeat(), RequestChangeSeatParams(callerId, 1), eRMI_ToServer);
			else if (actionId == eVAI_ChangeSeat2 && activationMode == eAAM_OnPress)
				GetGameObject()->InvokeRMI(SvRequestChangeSeat(), RequestChangeSeatParams(callerId, 2), eRMI_ToServer);
			else if (actionId == eVAI_ChangeSeat3 && activationMode == eAAM_OnPress)
				GetGameObject()->InvokeRMI(SvRequestChangeSeat(), RequestChangeSeatParams(callerId, 3), eRMI_ToServer);
			else if (actionId == eVAI_ChangeSeat4 && activationMode == eAAM_OnPress)
				GetGameObject()->InvokeRMI(SvRequestChangeSeat(), RequestChangeSeatParams(callerId, 4), eRMI_ToServer);
			else if (actionId == eVAI_ChangeSeat5 && activationMode == eAAM_OnPress)
				GetGameObject()->InvokeRMI(SvRequestChangeSeat(), RequestChangeSeatParams(callerId, 5), eRMI_ToServer);
			else if (actionId == eVAI_ChangeSeat && activationMode == eAAM_OnPress)
				GetGameObject()->InvokeRMI(SvRequestChangeSeat(), RequestChangeSeatParams(callerId, 0), eRMI_ToServer);
			else if (actionId == eVAI_Exit && activationMode == eAAM_OnPress)
			{
				IActorSystem* pActorSystem = gEnv->pGame->GetIGameFramework()->GetIActorSystem();
				IActor* pActor = pActorSystem->GetActor(callerId);
				assert(pActor);

				Matrix34 worldTM = pCurrentSeat->GetExitTM(pActor, false);
				Vec3 pos = worldTM.GetTranslation();
				EntityId blockerId = 0;
				bool canExit = pCurrentSeat->TestExitPosition(pActor, pos, &blockerId);
				if (!canExit)
					canExit = m_pVehicle->GetExitPositionForActor(pActor, pos);

				//CryMP
				if (canExit)
				{
					const bool success = pCurrentSeat->Exit(true, false);
					if (success)
					{
						GetGameObject()->InvokeRMI(SvRequestLeave(), RequestLeaveParams(callerId), eRMI_ToServer);
					}
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CVehicle::OnHit(EntityId targetId, EntityId shooterId, float damage, Vec3 position, float radius, const char* pHitClass, bool explosion)
{
	if (IsDestroyed() || damage == 0.0f)
		return;

	// repair event
	if (!strcmp(pHitClass, "repair"))
	{
		damage = -damage;
	}

	if (!strcmp(pHitClass, "disableCollisions"))
	{
		m_collisionDisabledTime = damage;
		return;
	}

	if (damage > 0.f)
	{
		if (m_indestructible || VehicleCVars().v_goliathMode == 1)
			return;

		IActor* pActor = gEnv->pGame->GetIGameFramework()->GetClientActor();

		if (IsPlayerDriving())
		{
			if (pActor != NULL)
			{
				if (pActor->IsGod() > 1) // return in Team God
					return;
			}
		}
		else if (pActor && shooterId == pActor->GetEntityId() && !gEnv->bMultiplayer && HasFriendlyPassenger(pActor->GetEntity())) //don't deal damage to friendly tanks
			return;

		SVehicleEventParams eventParams;
		eventParams.fParam = damage;
		BroadcastVehicleEvent(eVE_Hit, eventParams);
	}

	if (!strcmp("collision", pHitClass))
	{
		if (m_collisionDisabledTime > 0.0f)
			return;

		if (damage < 1.f)
			return;

		// collisions don't pass radius, therefore we determine one based on the damage 
		// weighted against collision resistance. this is pretty approximate, but should 
		// serve our purpose of damaging components near the hit pos.
		if (radius == 0.f)
			radius = max(0.25f, min(1.f, 0.5f * (damage + m_damageParams.collisionDamageThreshold) / max(1.f, m_damageParams.collisionDamageThreshold)));
	}

	Vec3 localPos = GetEntity()->GetWorldTM().GetInverted() * position;

	if (VehicleCVars().v_debugdraw == eVDB_Damage)
	{
		CryLog("============================================================");
		CryLog("<%s> OnHit ('%s'): localPos %.2f %.2f %.2f, radius %.2f", GetEntity()->GetName(), pHitClass, localPos.x, localPos.y, localPos.z, radius);
	}

	float prevDmgRatio = GetDamageRatio();

	// first gather all affected components, then call OnHit and pass them the list
	TVehicleComponentVector components;
	for (TVehicleComponentVector::iterator ite = m_components.begin(), end = m_components.end(); ite != end; ++ite)
	{
		CVehicleComponent* pComponent = *ite;

		if (pComponent->GetMaxDamage() == 0.f)
			continue;

		const AABB& localBounds = pComponent->GetBounds();
		if (localBounds.IsReset() || localBounds.IsEmpty())
			continue;

		bool hit = true;
		if (damage > 0.f)
		{
			if (radius > 0.f)
			{
				if (!localBounds.IsOverlapSphereBounds(localPos, radius))
					hit = false;
			}
			else
			{
				if (!localBounds.IsContainPoint(localPos))
					hit = false;
			}
		}

		if (hit)
			components.push_back(pComponent);
	}

	for (TVehicleComponentVector::iterator ite = components.begin(), end = components.end(); ite != end; ++ite)
	{
		CVehicleComponent* pComponent = *ite;
		pComponent->OnHit(targetId, shooterId, damage, position, radius, pHitClass, explosion, &components);

		if (pComponent->IsHull())
			m_status.health = 1.f - pComponent->GetDamageRatio();
	}

	// direct explosion hits (eg with LAW) kill gunners in singleplayer, but only if the weapon was fired by the player.
	IActor* pShooter = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(shooterId);
	if (!gEnv->bMultiplayer && explosion && (targetId == GetEntityId()) && damage > 260 && pShooter && pShooter->IsPlayer())
	{
		KillPassengersInExposedSeats(false);
	}

	float deltaRatio = (GetDamageRatio() - prevDmgRatio);
	if (deltaRatio != 0.f)
	{
		EVehicleEvent event = (deltaRatio > 0.f) ? eVE_Damaged : eVE_Repair;
		SVehicleEventParams eventParams;
		eventParams.fParam = deltaRatio * m_damageMax;
		BroadcastVehicleEvent(event, eventParams);
	}

	if (gEnv->bMultiplayer && gEnv->bServer && GetDamageRatio() > 0.05f && IsEmpty())
		StartAbandonedTimer();

	return;
}


//------------------------------------------------------------------------
bool CVehicle::IsCrewHostile(EntityId userId)
{
	if (GetStatus().passengerCount == 0)
		return false;

	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(userId);
	IAIObject* pUserAIObject = pEntity ? pEntity->GetAI() : 0;

	if (!pUserAIObject)
		return false;

	for (TVehicleSeatVector::const_iterator it = m_seats.begin(), end = m_seats.end(); it != end; ++it)
	{
		IActor* pPassengerActor = it->second->GetPassengerActor();

		if (pPassengerActor != NULL)
		{
			if (pPassengerActor->GetHealth() <= 0)
				continue;

			IAIObject* pPassengerAIObject = pPassengerActor->GetEntity()->GetAI();

			if (pPassengerAIObject)
				return pPassengerAIObject->IsHostile(pUserAIObject, false);
		}
	}

	return false;
}

//------------------------------------------------------------------------
bool CVehicle::IsActionUsable(const SVehicleActionInfo& actionInfo, const SMovementState* movementState)
{
	for (TActivationInfoVector::const_iterator ite = actionInfo.activations.begin(), end = actionInfo.activations.end();
		ite != end; ++ite)
	{
		const SActivationInfo& activationInfo = *ite;

		if (activationInfo.m_type == SActivationInfo::eAT_OnUsed)
		{
			AABB localbounds;
			AABB bounds;
			localbounds.Reset();

			if (activationInfo.m_param1 == SActivationInfo::eAP1_Component)
			{
				if (IVehicleComponent* pComponent = GetComponent(activationInfo.m_param2.c_str()))
					localbounds = pComponent->GetBounds();
			}
			else if (activationInfo.m_param1 == SActivationInfo::eAP1_Part)
			{
				if (IVehiclePart* pPart = GetPart(activationInfo.m_param2.c_str()))
					localbounds = pPart->GetLocalBounds();
			}
			bounds = AABB::CreateTransformedAABB(GetEntity()->GetWorldTM(), localbounds);

			Vec3 intersectOut;
			Lineseg lineSeg(movementState->eyePosition, movementState->eyePosition + movementState->eyeDirection * activationInfo.m_distance);

			int hit = Intersect::Lineseg_AABB(lineSeg, bounds, intersectOut);

			if (VehicleCVars().v_debugdraw > 1)
			{
				IRenderer* pRenderer = gEnv->pRenderer;
				IRenderAuxGeom* pRenderAux = pRenderer->GetIRenderAuxGeom();
				const Matrix34& worldTM = m_pVehicle->GetEntity()->GetWorldTM();
				pRenderAux->DrawAABB(localbounds, worldTM, false, hit ? Col_Green : Col_Red, eBBD_Faceted);
				pRenderAux->DrawLine(lineSeg.start, Col_Green, lineSeg.end, Col_Green);
			}

			if (hit) return true;
		}
	}

	return false;
}

//------------------------------------------------------------------------
bool CVehicle::IsFrozen()
{
	if (m_frozen)
		return true;

	IEntityRenderProxy* pRenderProxy = (IEntityRenderProxy*)GetEntity()->GetProxy(ENTITY_PROXY_RENDER);
	if (pRenderProxy && pRenderProxy->GetMaterialLayersMask() & MTL_LAYER_FROZEN)
		return true;

	return false;
}

//------------------------------------------------------------------------
int CVehicle::IsUsable(EntityId userId)
{
	if (IsDestroyed() || IsFrozen() || IsSubmerged() || VehicleCVars().v_disableEntry != 0)
		return 0;

	if (!gEnv->bMultiplayer)
	{
		if (IsCrewHostile(userId))
			return 0;
	}

	IActor* pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(userId);
	if (pActor == NULL) return false;

	IMovementController* pController = pActor->GetMovementController();
	assert(pController);
	SMovementState movementState;
	pController->GetMovementState(movementState);

	const int actionsUsable = IsFlipped() ? 0 : 1;

	if (m_usesVehicleActionToEnter)
	{
		int actionIndex = 1;

		for (TVehicleActionVector::iterator ite = m_actions.begin(); ite != m_actions.end(); ++ite)
		{
			SVehicleActionInfo& actionInfo = *ite;

			if ((actionsUsable ^ actionInfo.useWhenFlipped) && IsActionUsable(actionInfo, &movementState))
			{
				SVehicleEventParams eventParams;
				eventParams.entityId = userId;

				if (actionInfo.pAction->OnEvent(eVAE_IsUsable, eventParams) > 0)
					return (actionIndex * 100) + eventParams.iParam + 1000;
			}

			actionIndex++;
		}
	}
	else
	{
		int seatIndex = 0;

		for (TVehicleSeatVector::iterator ite = m_seats.begin(); ite != m_seats.end(); ++ite)
		{
			CVehicleSeat* pSeat = ite->second;
			seatIndex++;

			if (pSeat->IsFree())
				return 300 + seatIndex;
		}
	}

	return 0;
}

//------------------------------------------------------------------------
bool CVehicle::OnUsed(EntityId userId, int index)
{
	if (index <= 0)
		return false;

	if (gEnv->bServer)
	{
		if (IsFrozen())
			return false;

		IActor* pUser = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(userId);
		if (!pUser || !CanEnter(userId))
			return false;

		if (m_usesVehicleActionToEnter)
		{
			int actionIndex = 1;

			for (TVehicleActionVector::iterator ite = m_actions.begin(); ite != m_actions.end(); ++ite)
			{
				SVehicleActionInfo& actionInfo = *ite;

				int filteredIndex = index - (actionIndex * 100) - 1000;
				if (filteredIndex < 100)
				{
					SVehicleEventParams eventParams;
					eventParams.entityId = userId;
					eventParams.iParam = filteredIndex;

					if (!actionInfo.pAction->OnEvent(eVAE_OnUsed, eventParams))
					{
						return eventParams.iParam != 0;
					}
				}

				actionIndex++;
			}
		}
		else
		{
			if (index > 300)
			{
				TVehicleSeatId seatIndex = index - 300;

				if (IVehicleSeat* pSeat = GetSeatById(seatIndex))
				{
					if (pSeat->Enter(userId))
						return true;
				}
			}
		}
	}
	else
		GetGameObject()->InvokeRMI(SvRequestUse(), RequestUseParams(userId, index), eRMI_ToServer);

	return true;
}

//------------------------------------------------------------------------
IVehiclePart* CVehicle::AddPart(const CVehicleParams& partParams, IVehiclePart* parent, SPartInitInfo& initInfo)
{
	string partClass = partParams.getAttr("class");
	string partName = partParams.getAttr("name");

	if (partClass.empty() || partName.empty())
		return NULL;

	for (TVehiclePartVector::iterator ite = m_parts.begin(); ite != m_parts.end(); ++ite)
	{
		if (ite->first == partName)
			return NULL;
	}

	if (IVehiclePart* pPart = m_pVehicleSystem->CreateVehiclePart(partClass))
	{
		CVehiclePartBase *pPartBase = static_cast<CVehiclePartBase*>(pPart);

		if (!pPartBase->Init(this, partParams, parent, initInfo))
		{
			CryLogWarningAlways("Vehicle error: part <%s> failed to initialize on vehicle <%s>", partName.c_str(), GetEntity()->GetName());

			pPart->Release();
			return NULL;
		}

		//CryMP:
		InitHelpers(partParams, pPart);

		m_parts.push_back(TVehiclePartPair(partName, pPart));
		return pPart;
	}
	else
	{
		CryLogWarningAlways("Vehicle error: failed to create part <%s> on vehicle <%s>", partName.c_str(), GetEntity()->GetName());
	}

	return NULL;
}

//------------------------------------------------------------------------
bool CVehicle::AddSeat(const SmartScriptTable& seatTable)
{
	int seatId = 0;
	if (seatTable->GetValue("seatId", seatId))
	{
		GameFramework::GetInstance()->GetScriptBind_VehicleSeat()->AttachTo(this, seatId); //CryMP
	}

	return false;
}

//------------------------------------------------------------------------
IVehiclePart* CVehicle::GetPart(const char* name)
{
	if (0 == name)
		return NULL;

	for (TVehiclePartVector::iterator ite = m_parts.begin(); ite != m_parts.end(); ite++)
	{
		if (0 == strcmp(name, ite->first.c_str()))
		{
			IVehiclePart* part = ite->second;
			return part;
		}
	}

	return NULL;
}

//------------------------------------------------------------------------
IVehicleAnimation* CVehicle::GetAnimation(const char* name)
{
	if (0 == name)
		return NULL;

	for (TVehicleAnimationsVector::iterator ite = m_animations.begin(); ite != m_animations.end(); ++ite)
	{
		if (0 == strcmp(name, ite->first.c_str()))
			return ite->second;
	}

	return NULL;
}

//------------------------------------------------------------------------
bool CVehicle::AddHelper(const char* pName, Vec3 position, Vec3 direction, IVehiclePart* pPart)
{
	CVehicleHelper* pHelper = new CVehicleHelper;

	if (position.IsZero() && (direction.IsZero() || direction.IsEquivalent(FORWARD_DIRECTION)))
	{
		if (CVehiclePartAnimated* pAnimatedPart = GetVehiclePartAnimated())
		{
			if (ICharacterInstance* pCharInstance =
				pAnimatedPart->GetEntity()->GetCharacter(pAnimatedPart->GetSlot()))
			{
				ISkeletonPose* pSkeletonPose = pCharInstance->GetISkeletonPose();
				assert(pSkeletonPose);

				int16 id = pSkeletonPose->GetJointIDByName(pName);
				if (id > -1)
				{
					pHelper->m_localTM = Matrix34(pSkeletonPose->GetRelJointByID(id));
					pHelper->m_pParentPart = pPart;
					pHelper->m_isVehicleUpdated = false;
					pHelper->m_isWorldUpdated = false;

					m_helpers.insert(TVehicleHelperMap::value_type(pName, pHelper));
					return true;
				}
			}
		}
	}
	else
	{
		pHelper->m_vehicleTM = Matrix33::CreateRotationVDir(direction.GetNormalizedSafe(FORWARD_DIRECTION));
		pHelper->m_vehicleTM.SetTranslation(position);

		pHelper->m_localTM = pPart->GetLocalTM(false).GetInverted() * pHelper->m_vehicleTM;

		pHelper->m_pParentPart = pPart;

		pHelper->m_isVehicleUpdated = true;
		pHelper->m_isWorldUpdated = false;

		m_helpers.insert(TVehicleHelperMap::value_type(pName, pHelper));
		return true;
	}

	return false;
}

//------------------------------------------------------------------------
void CVehicle::InitHelpers(const CVehicleParams& table, IVehiclePart *pPart)
{
	// currently this only initializes AI Anchors. If you use this method
	// for something else, be sure to adjust the checks below
	if (!SpawnAndDeleteEntities(true))
		return;

	CVehicleParams helpersTable = table.findChild("Helpers");
	if (!helpersTable)
		return;

	int i = 0;
	int c = helpersTable.getChildCount();

	for (; i < c; i++)
	{
		if (CVehicleParams helperRef = helpersTable.getChild(i))
		{
			//<Helper name="exhaust_pos" position="0.56879997,-2.7351,0.62400001" direction="0,-1,0" part="body" />

			if (!helperRef.haveAttr("name")) 
				continue;

			//if (!helperRef.haveAttr("part")) CryMP: commented out
			//	continue;

			Vec3 position, direction;
			if (!helperRef.getAttr("position", position))
				position.zero();

			if (!helperRef.getAttr("direction", direction))
				direction.zero();

			if (!pPart)
			{
				pPart = GetPart(helperRef.getAttr("part"));
			}

			if (pPart)
			{
				const bool ok = AddHelper(helperRef.getAttr("name"), position, direction, pPart);
				if (!ok)
				{
					CryLogWarningAlways("Failed to add helper <%s> on vehicle <%s, part: %s>", 
						helperRef.getAttr("name"), GetEntity()->GetName(), pPart->GetName());
				}
			}
			else
				CryLogWarningAlways("Failed to add helper <%s> on vehicle <%s>, no part found!", 
					helperRef.getAttr("name"), GetEntity()->GetName());
		}
	}

	TVehicleHelperMap::iterator endIte = m_helpers.end();
	for (TVehicleHelperMap::iterator it = m_helpers.begin(); it != endIte; ++it)
	{
		const string& helperName = it->first;
		IVehicleHelper* pHelper = it->second;

		// if AI helper, spawn AIAnchor entity 
		if (helperName.substr(0, 3) == "ai_")
		{
			IEntityClass* pEntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("AIAnchor");

			if (!pEntityClass)
			{
				CryLogWarning("Entity class 'AIAnchor' not found (entity name: %s)", GetEntity()->GetName());
				continue;
			}

			string entName(GetEntity()->GetName());
			entName += "_";
			entName += helperName;

			IEntity* pEntity = 0;

			SEntitySpawnParams spawnParams;
			spawnParams.sName = entName.c_str();
			spawnParams.nFlags = ENTITY_FLAG_CLIENT_ONLY;
			spawnParams.pUserData = 0;
			spawnParams.pClass = pEntityClass;

			pEntity = gEnv->pEntitySystem->SpawnEntity(spawnParams, false);
			IScriptTable* pEntityTable = pEntity->GetScriptTable();

			if (pEntityTable)
			{
				SmartScriptTable entityProps;
				if (pEntityTable->GetValue("Properties", entityProps))
				{
					entityProps->SetValue("aianchor_AnchorType", "COMBAT_HIDESPOT");
				}
			}
			gEnv->pEntitySystem->InitEntity(pEntity, spawnParams);

			GetEntity()->AttachChild(pEntity);
			pEntity->SetLocalTM(pHelper->GetVehicleTM());
		}
	}
}

//------------------------------------------------------------------------
bool CVehicle::HasHelper(const char* pName)
{
	TVehicleHelperMap::iterator ite = m_helpers.find(CONST_TEMP_STRING(pName));
	return (ite != m_helpers.end());
}

//------------------------------------------------------------------------
bool CVehicle::AddComponent(const CVehicleParams& componentParams)
{
	CVehicleComponent* pComponent = new CVehicleComponent;

	if (pComponent && pComponent->Init(this, componentParams))
	{
		m_components.push_back(pComponent);
		return true;
	}

	if (pComponent)
		delete pComponent;

	return false;
}

//------------------------------------------------------------------------
IVehicleComponent* CVehicle::GetComponent(const char* name)
{
	if (0 == name)
		return NULL;

	for (TVehicleComponentVector::iterator ite = m_components.begin(); ite != m_components.end(); ++ite)
	{
		if (0 == strcmpi(name, (*ite)->GetName().c_str()))
		{
			return (IVehicleComponent*)*ite;
		}
	}

	return NULL;
}

//------------------------------------------------------------------------
// TODO: this could be updated and stored in OnHit instead of calculated each time
float CVehicle::GetDamageRatio(bool onlyMajorComponents/*=false*/) const
{
	float damageRatio = 0.0f;
	if (onlyMajorComponents)
	{
		if (m_majorComponentDamageMax == 0.0f)
			return 0.0f;

		int numComp = 0;
		for (TVehicleComponentVector::const_iterator ite = m_components.begin(); ite != m_components.end(); ++ite)
		{
			if ((*ite)->IsMajorComponent())
			{
				damageRatio += (*ite)->GetDamageRatio();
				++numComp;
			}
		}
		if (numComp)
			damageRatio /= numComp;
	}
	else
	{
		if (m_damageMax == 0.0f)
			return 0.0f;

		float invMaxDamage = 1.0f / m_damageMax;
		for (TVehicleComponentVector::const_iterator ite = m_components.begin(); ite != m_components.end(); ++ite)
		{
			damageRatio += (*ite)->GetDamageRatio() * (*ite)->GetMaxDamage() * invMaxDamage;
		}
	}

	return damageRatio;
}

//------------------------------------------------------------------------
IVehiclePart* CVehicle::GetWeaponParentPart(EntityId weaponId)
{
	for (TVehicleSeatId id = 1; id <= GetSeatCount(); ++id)
	{
		CVehicleSeat* pSeat = (CVehicleSeat*)GetSeatById(id);

		TVehicleSeatActionVector::const_iterator end = pSeat->GetSeatActions().end();
		for (TVehicleSeatActionVector::const_iterator it = pSeat->GetSeatActions().begin(); it != end; ++it)
		{
			if (CVehicleSeatActionWeapons* pAction = CAST_VEHICLEOBJECT(CVehicleSeatActionWeapons, it->pSeatAction))
			{
				if (SVehicleWeapon* pWeaponInfo = pAction->GetVehicleWeapon(weaponId))
				{
					return pWeaponInfo->pPart;
				}
			}
		}
	}

	return NULL;
}

//------------------------------------------------------------------------
IVehicleSeat* CVehicle::GetWeaponParentSeat(EntityId weaponId)
{
	for (TVehicleSeatId id = 1; id <= GetSeatCount(); ++id)
	{
		CVehicleSeat* pSeat = (CVehicleSeat*)GetSeatById(id);

		TVehicleSeatActionVector::const_iterator end = pSeat->GetSeatActions().end();
		for (TVehicleSeatActionVector::const_iterator it = pSeat->GetSeatActions().begin(); it != end; ++it)
		{
			if (CVehicleSeatActionWeapons* pAction = CAST_VEHICLEOBJECT(CVehicleSeatActionWeapons, it->pSeatAction))
			{
				if (SVehicleWeapon* pWeaponInfo = pAction->GetVehicleWeapon(weaponId))
				{
					return pSeat;
				}
			}
		}
	}

	return NULL;
}


//------------------------------------------------------------------------
bool CVehicle::SetMovement(const string& movementName, const CVehicleParams& table)
{
	m_pMovement = m_pVehicleSystem->CreateVehicleMovement(movementName);

	if (m_pMovement && m_pMovement->Init(this, table)) 
	{
		if (gEnv->bServer)
			GetGameObject()->SetAspectProfile(eEA_Physics, eVPhys_Physicalized);

		return true;
	}
	else
	{
		CryLogAlways("CVehicle::SetMovemement() failed to init movement <%s> on vehicle of class <%s>.", movementName.c_str(), GetEntity()->GetClass()->GetName());
		SAFE_RELEASE(m_pMovement);

		return false;
	}
}

//------------------------------------------------------------------------
void CVehicle::OnPhysPostStep(const EventPhys* pEvent)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);
	const EventPhysPostStep* eventPhys = (const EventPhysPostStep*)pEvent;

	float deltaTime = eventPhys->dt;
	if (IVehicleMovement* pMovement = GetMovement())
	{
		// (MATT) Avoid f.p. exceptions in quickload {2009/05/25}
		if (pMovement->IsMovementProcessingEnabled() && deltaTime > 0.0f)
			pMovement->ProcessMovement(deltaTime);
	}
}

//------------------------------------------------------------------------
void CVehicle::OnCollision(EventPhysCollision* pCollision)
{
	if (m_status.flipped > 0.0f || m_collisionDisabledTime > 0.0f)
		return;

	IEntity* pE1 = gEnv->pEntitySystem->GetEntityFromPhysics(pCollision->pEntity[0]);
	IEntity* pE2 = gEnv->pEntitySystem->GetEntityFromPhysics(pCollision->pEntity[1]);

	int idx = (pE1 == GetEntity()) ? 0 : 1;
	IEntity* pCollider = idx ? pE1 : pE2;

	SVehicleEventParams eventParams;
	eventParams.entityId = pCollider ? pCollider->GetId() : 0;
	eventParams.vParam = pCollision->pt;
	eventParams.fParam = pCollision->vloc[idx].GetLength();
	eventParams.fParam2 = pCollision->normImpulse;
	BroadcastVehicleEvent(eVE_Collision, eventParams);

	if (m_pMovement)
	{
		SVehicleMovementEventParams params;

		if (pCollider)
		{
			params.iValue = pCollision->partid[idx];
			m_pMovement->OnEvent(IVehicleMovement::eVME_Collision, params);
		}
		else
			m_pMovement->OnEvent(IVehicleMovement::eVME_GroundCollision, params);
	}

	if (IsPlayerDriving(true))
		gEnv->pInput->ForceFeedbackEvent(SFFOutputEvent(eDI_XI, eFF_Rumble_Basic, 0.3f, 0.3f, clamp_tpl(pCollision->normImpulse * 0.00005f, 0.0f, 1.0f)));

	if (!pCollider)
	{
		for (TVehicleActionVector::const_iterator ite = m_actions.begin(); ite != m_actions.end(); ++ite)
		{
			const SVehicleActionInfo& actionInfo = *ite;

			for (TActivationInfoVector::const_iterator iter = actionInfo.activations.begin(); iter != actionInfo.activations.end(); ++iter)
			{
				const SActivationInfo& activationInfo = *iter;

				if (activationInfo.m_type == SActivationInfo::eAT_OnGroundCollision)
				{
					IVehicleAction* pAction = actionInfo.pAction;

					SVehicleEventParams eventParams2;
					eventParams2.iParam = 0;
					eventParams2.vParam = pCollision->pt;
					EVehicleActionEvent event = eVAE_OnGroundCollision;
					pAction->OnEvent(event, eventParams2);
				}
			}
		}
	}

	if (m_wheelCount && m_wheelCount < 8 && pCollider && pCollision->partid[idx] >= 0 && pCollision->partid[idx] < PARTID_CGA)
	{
		pe_status_wheel sw;
		sw.partid = pCollision->partid[idx];
		if (GetEntity()->GetPhysics()->GetStatus(&sw))
		{
			Matrix34 tm;
			pe_status_pos sp;
			sp.pMtx3x4 = &tm;
			sp.partid = pCollision->partid[idx];
			if (GetEntity()->GetPhysics()->GetStatus(&sp))
			{
				Vec3 pos = tm.GetInverted() * pCollision->pt;
				pos.x = 0;

				for (int i = 0; i < m_wheelCount; ++i)
				{
					CVehiclePartSubPartWheel* pWheel = static_cast<CVehiclePartSubPartWheel*>(GetWheelPart(i));
					if (pWheel && pWheel->GetPhysId() == pCollision->partid[idx])
					{
						if (VehicleCVars().v_debugCollisionDamage > 1)
						{
							//CryLog("[coll] wheel %i: vloc (%.1f %.1f %.1f)", i, pCollision->vloc[idx].x, pCollision->vloc[idx].y, pCollision->vloc[idx].z);
						}

						if (pWheel->m_rimRadius > 0.f && pos.len2() > sqr(pWheel->m_rimRadius))
						{
							IMaterialManager* pMatMan = gEnv->p3DEngine->GetMaterialManager();
							int matId = pMatMan->GetSurfaceTypeIdByName("mat_rubber");

							if (matId > 0)
							{
								// this is ugly, but the 'only' way according to Anton
								// update: maybe possible to remove it since bullets can be checked 
								// against the rendermesh material now
								pCollision->idmat[idx] = matId;
							}
						}
						break;
					}
				}
			}
		}
	}
}

//------------------------------------------------------------------------
float CVehicle::GetSelfCollisionMult(const Vec3& velocity, const Vec3& normal, int partId, EntityId colliderId)
{
	float mult = 1.f;
	bool wheel = false;

	if (m_wheelCount&& partId >= 0 && partId < PARTID_CGA)
	{
		pe_status_wheel sw;
		sw.partid = partId;
		wheel = GetEntity()->GetPhysics()->GetStatus(&sw) && sw.suspLen0 > 0.f;
	}

	float zspeed = -velocity.z * normal.z;
	if (colliderId == 0 && zspeed > m_damageParams.groundCollisionMinSpeed)
	{
		if (VehicleCVars().v_debugCollisionDamage)
			CryLog("[coll] %s [part %i], zspeed: %.1f [min: %.1f, max: %.1f]", GetEntity()->GetName(), partId, zspeed, m_damageParams.groundCollisionMinSpeed, m_damageParams.groundCollisionMaxSpeed);

		float diffSpeed = m_damageParams.groundCollisionMaxSpeed - m_damageParams.groundCollisionMinSpeed;
		float diffMult = m_damageParams.groundCollisionMaxMult - m_damageParams.groundCollisionMinMult;

		float speedratio = (diffSpeed > 0.f) ? (zspeed - m_damageParams.groundCollisionMinSpeed) / diffSpeed : 1.f;
		float groundMult = m_damageParams.groundCollisionMinMult + speedratio * diffMult;

		bool hard = zspeed > m_damageParams.groundCollisionMaxSpeed && !wheel;
		if (hard)
			groundMult = max(groundMult, 5.f * m_damageParams.groundCollisionMaxMult);

		if (VehicleCVars().v_debugCollisionDamage)
			CryLog("[coll] %s ratio %.2f, groundmult %.2f", GetEntity()->GetName(), speedratio, groundMult);

		mult *= groundMult;
	}

	if (wheel)
	{
		const float wheelMaxAbsorbance = 0.5f;

		// absorb collisions along suspension axis (here: always along entity z)
		float dot = normal.Dot(GetEntity()->GetWorldTM().GetColumn2());
		float wheelMult = 1.f - wheelMaxAbsorbance * abs(dot);

		if (wheelMult < 0.98f)
		{
			if (VehicleCVars().v_debugCollisionDamage)
				CryLog("[coll] %s, wheelMult %.2f", GetEntity()->GetName(), wheelMult);
		}

		mult *= wheelMult;
	}

	// for flying vehicle, should be implemented later
	if (GetMovement() && GetMovement()->GetMovementType() == IVehicleMovement::eVMT_Air)
	{

		if (colliderId && colliderId != GetEntityId())
		{
			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(colliderId);
			IAIObject* pAI = 0;
			if (pEntity)
				pAI = pEntity->GetAI();

			if (pAI && pAI->GetAIType() == AIOBJECT_PUPPET)
			{
				mult = 0.01f * GetStatus().speed / 10.0f;
				return mult;
			}
		}
		else if (!colliderId)
		{
			IAIObject* pAI = GetEntity()->GetAI();
			if (pAI && GetEntity()->GetAI()->IsEnabled())
				mult = 0.0f;
		}

	}

	// for task 29233: TASK: SYSTEMIC: VEHICLE: Upon 2 Vehicles Colliding at a certain speed - both should explode
	if (colliderId && m_damageParams.vehicleCollisionDestructionSpeed > 0 && !IsDestroyed())
	{
		if (IVehicle* pVehicle = m_pVehicleSystem->GetVehicle(colliderId))
		{
			// this will destroy us
			float thresholdSq = m_damageParams.vehicleCollisionDestructionSpeed * m_damageParams.vehicleCollisionDestructionSpeed;
			if (velocity.GetLengthSquared() > thresholdSq)
			{
				// tweak: make sure at least one of the vehicles is above the threshold. Prevents the MP 2-vehicles-at-moderate-speed explosion...
				if ((GetStatus().speed * GetStatus().speed > thresholdSq)
					|| (pVehicle->GetStatus().speed * pVehicle->GetStatus().speed > thresholdSq))
				{
					mult *= 1000.0f;	// should do it :)
					if (VehicleCVars().v_debugCollisionDamage)
						CryLog("[coll] above destruction speed: speed %.2f, threshold %.2f, mult %.2f", velocity.GetLength(), m_damageParams.vehicleCollisionDestructionSpeed, mult);
				}
			}

			// also destroy other vehicle? If the other vehicle has a vehicleCollisionDestructionSpeed set,
			//	then it will also get this collision and kill itself. Otherwise, it'll just be damaged by
			//	our explosion (which is enough to take out a truck, destroy a tank's treads (but not the tank), etc...
		}
	}

	return mult;
}


//------------------------------------------------------------------------
bool CVehicle::InitParticles(const CVehicleParams& table)
{
	CVehicleParams particleTable = table.findChild("Particles");

	if (!table || !particleTable)
		return false;

	CEnvironmentParticles& envParams = m_particleParams.m_envParams;

	// previously this was hardcoded to use vfx_<vehicleclass> as the lookup for 
	//	material effects (requiring a new row in the materialeffects.xml file for
	//	each vehicle type). Now can be shared between vehicles.
	if (particleTable.haveAttr("mfxRow"))
		envParams.mfx_rowName = particleTable.getAttr("mfxRow");
	else
		envParams.mfx_rowName.Format("vfx_%s", GetEntity()->GetClass()->GetName());

	if (CVehicleParams exhaustTable = particleTable.findChild("Exhaust"))
	{
		CExhaustParams& exhaustParams = m_particleParams.m_exhaustParams;

		if (CVehicleParams pTable = exhaustTable.findChild("Helpers"))
		{
			int i = 0;
			int c = pTable.getChildCount();

			for (; i < c; i++)
			{
				if (CVehicleParams helperRef = pTable.getChild(i))
				{
					if (IVehicleHelper* pHelper = GetHelper(helperRef.getAttr("value")))
						exhaustParams.helpers.push_back(pHelper);
				}
			}
		}

		if (exhaustParams.helpers.size() != 0)
		{
			exhaustTable.getAttr("insideWater", exhaustParams.insideWater);
			exhaustTable.getAttr("outsideWater", exhaustParams.outsideWater);

			if (CVehicleParams pTable = exhaustTable.findChild("EngineStart"))
			{
				exhaustParams.startEffect = pTable.getAttr("effect");

				if (!exhaustParams.startEffect.empty())
					m_pParticleEffects.push_back(gEnv->p3DEngine->FindParticleEffect(exhaustParams.startEffect.c_str(), "CVehicle::InitParticles"));
			}

			if (CVehicleParams pTable = exhaustTable.findChild("EngineStop"))
			{
				exhaustParams.stopEffect = pTable.getAttr("effect");

				if (!exhaustParams.stopEffect.empty())
					m_pParticleEffects.push_back(gEnv->p3DEngine->FindParticleEffect(exhaustParams.stopEffect.c_str(), "CVehicle::InitParticles"));
			}

			if (CVehicleParams pTable = exhaustTable.findChild("EngineRunning"))
			{
				exhaustParams.runEffect = pTable.getAttr("effect");
				exhaustParams.boostEffect = pTable.getAttr("boostEffect");

				if (!exhaustParams.runEffect.empty())
					m_pParticleEffects.push_back(gEnv->p3DEngine->FindParticleEffect(exhaustParams.runEffect.c_str(), "CVehicle::InitParticles"));

				if (!exhaustParams.boostEffect.empty())
					m_pParticleEffects.push_back(gEnv->p3DEngine->FindParticleEffect(exhaustParams.boostEffect.c_str(), "CVehicle::InitParticles"));

				pTable.getAttr("baseSizeScale", exhaustParams.runBaseSizeScale);
				pTable.getAttr("minSpeed", exhaustParams.runMinSpeed);
				pTable.getAttr("minSpeedSizeScale", exhaustParams.runMinSpeedSizeScale);
				pTable.getAttr("minSpeedCountScale", exhaustParams.runMinSpeedCountScale);
				pTable.getAttr("minSpeedSpeedScale", exhaustParams.runMinSpeedSpeedScale);
				pTable.getAttr("maxSpeed", exhaustParams.runMaxSpeed);
				pTable.getAttr("maxSpeedSizeScale", exhaustParams.runMaxSpeedSizeScale);
				pTable.getAttr("maxSpeedCountScale", exhaustParams.runMaxSpeedCountScale);
				pTable.getAttr("maxSpeedSpeedScale", exhaustParams.runMaxSpeedSpeedScale);
				pTable.getAttr("minPower", exhaustParams.runMinPower);
				pTable.getAttr("minPowerSizeScale", exhaustParams.runMinPowerSizeScale);
				pTable.getAttr("minPowerCountScale", exhaustParams.runMinPowerCountScale);
				pTable.getAttr("minPowerSpeedScale", exhaustParams.runMinPowerSpeedScale);
				pTable.getAttr("maxPower", exhaustParams.runMaxPower);
				pTable.getAttr("maxPowerSizeScale", exhaustParams.runMaxPowerSizeScale);
				pTable.getAttr("maxPowerCountScale", exhaustParams.runMaxPowerCountScale);
				pTable.getAttr("maxPowerSpeedScale", exhaustParams.runMaxPowerSpeedScale);
				pTable.getAttr("disableWithNegativePower", exhaustParams.disableWithNegativePower);
			}

			exhaustParams.hasExhaust = true;
		}
	}

	/*
	<DamageEffects>
	<DamageEffect name="EngineDamaged" effect="" helper="engineSmokeOut" />
	</DamageEffects>
	*/
	if (CVehicleParams damageEffectsTable = particleTable.findChild("DamageEffects"))
	{
		int i = 0;
		int c = damageEffectsTable.getChildCount();

		for (; i < c; i++)
		{
			if (CVehicleParams damageEffectRef = damageEffectsTable.getChild(i))
			{
				SDamageEffect damageEffect;

				if (damageEffectRef.haveAttr("name") && damageEffectRef.haveAttr("effect"))
				{
					const char* pEffectName = damageEffectRef.getAttr("name");
					damageEffect.effectName = damageEffectRef.getAttr("effect");

					if (damageEffectRef.haveAttr("helper"))
						damageEffect.pHelper = GetHelper(damageEffectRef.getAttr("helper"));

					if (damageEffectRef.getAttr("gravityDirection", damageEffect.gravityDir))
					{
						damageEffect.isGravityDirUsed = true;
					}
					else
					{
						damageEffect.isGravityDirUsed = false;
						damageEffect.gravityDir.zero();
					}

					if (!damageEffectRef.getAttr("pulsePeriod", damageEffect.pulsePeriod))
						damageEffect.pulsePeriod = 0.0f;

					if (VehicleCVars().v_debugdraw == eVDB_Damage)
						CryLog("Adding damage effect: name %s, effect %s",
							damageEffectsTable.getAttr("name"), damageEffect.effectName.c_str());

					m_particleParams.damageEffects.insert(
						TDamageEffectMap::value_type(pEffectName, damageEffect)
					);

					// preload
					m_pParticleEffects.push_back(
						gEnv->p3DEngine->FindParticleEffect(damageEffect.effectName.c_str(), "CVehicle::InitParticles")
					);
				}
			}
		}
	}

	// <ShatterEffect effect="vehicle_fx.frozen_shatter.small" />
	if (CVehicleParams shatterTable = particleTable.findChild("ShatterEffect"))
	{
		// CVehicleParams::getAttr returns const char*, so assign directly (like you do elsewhere)
		m_particleParams.m_shatterEffect = shatterTable.getAttr("effect");

		if (!m_particleParams.m_shatterEffect.empty())
		{
			m_pParticleEffects.push_back(
				gEnv->p3DEngine->FindParticleEffect(
					m_particleParams.m_shatterEffect.c_str(),
					"CVehicle::InitParticles"
				)
			);
		}
	}


	if (CVehicleParams envTable = particleTable.findChild("EnvironmentLayers"))
	{
		int cnt = envTable.getChildCount();
		envParams.layers.reserve(cnt);

		for (int i = 1;i <= cnt;++i)
		{
			CVehicleParams layerTable = envTable.getChild(i - 1);
			if (!layerTable)
				continue;

			CEnvironmentLayer layerParams;

			layerParams.name = layerTable.getAttr("name");

			layerTable.getAttr("active", layerParams.active);
			layerTable.getAttr("minSpeed", layerParams.minSpeed);
			layerTable.getAttr("minSpeedSizeScale", layerParams.minSpeedSizeScale);
			layerTable.getAttr("minSpeedCountScale", layerParams.minSpeedCountScale);
			layerTable.getAttr("minSpeedSpeedScale", layerParams.minSpeedSpeedScale);
			layerTable.getAttr("maxSpeed", layerParams.maxSpeed);
			layerTable.getAttr("maxSpeedSizeScale", layerParams.maxSpeedSizeScale);
			layerTable.getAttr("maxSpeedCountScale", layerParams.maxSpeedCountScale);
			layerTable.getAttr("maxSpeedSpeedScale", layerParams.maxSpeedSpeedScale);
			layerTable.getAttr("minPowerSizeScale", layerParams.minPowerSizeScale);
			layerTable.getAttr("maxPowerSizeScale", layerParams.maxPowerSizeScale);
			layerTable.getAttr("minPowerCountScale", layerParams.minPowerCountScale);
			layerTable.getAttr("maxPowerCountScale", layerParams.maxPowerCountScale);
			layerTable.getAttr("minPowerSpeedScale", layerParams.minPowerSpeedScale);
			layerTable.getAttr("maxPowerSpeedScale", layerParams.maxPowerSpeedScale);

			SmartScriptTable wheels;

			if (CVehicleParams alignmentTable = layerTable.findChild("Alignment"))
			{
				alignmentTable.getAttr("alignGroundHeight", layerParams.alignGroundHeight);
				alignmentTable.getAttr("maxHeightSizeScale", layerParams.maxHeightSizeScale);
				alignmentTable.getAttr("maxHeightCountScale", layerParams.maxHeightCountScale);
				alignmentTable.getAttr("alignToWater", layerParams.alignToWater);
			}

			if (CVehicleParams emitterTable = layerTable.findChild("Emitters"))
			{
				int n = 0;
				int cEmitters = emitterTable.getChildCount();

				for (; n < cEmitters; n++)
				{
					if (CVehicleParams emitterRef = emitterTable.getChild(n))
					{
						if (IVehicleHelper* pHelper = GetHelper(emitterRef.getAttr("value")))
							layerParams.helpers.push_back(pHelper);
					}
				}
			}

			if (CVehicleParams wheelsTable = layerTable.findChild("Wheels"))
			{
				bool all = false;

				if (wheelsTable.getAttr("all", all) && all)
				{
					bool allActive = true;
					wheelsTable.getAttr("allActive", allActive);

					for (int n = 1; n <= GetWheelCount(); ++n)
					{
						CEnvironmentLayer::SWheelGroup group;
						group.m_wheels.push_back(n);
						group.active = allActive;
						layerParams.wheelGroups.push_back(group);
					}
				}
				else
				{
					if (CVehicleParams groupsTable = wheelsTable.findChild("WheelGroups"))
					{
						int n = 0;
						int cWheelGroups = groupsTable.getChildCount();

						for (; n < cWheelGroups; n++)
						{
							if (CVehicleParams wheelGroupRef = groupsTable.getChild(n))
							{
								CEnvironmentLayer::SWheelGroup group;

								if (CVehicleParams wheelArrayRef = wheelGroupRef.findChild("Wheels"))
								{
									int k = 0;
									int cWheels = wheelArrayRef.getChildCount();

									for (; k < cWheels; k++)
									{
										if (CVehicleParams wheelRef = wheelArrayRef.getChild(k))
										{
											int wheelIndex = 0;
											wheelRef.getAttr("value", wheelIndex);
											group.m_wheels.push_back(wheelIndex);
										}
									}
								}

								groupsTable.getAttr("active", group.active);
								layerParams.wheelGroups.push_back(group);
							}
						}
					}
				}
			}

			envParams.layers.push_back(layerParams);
		}
	}

	return true;
}

//------------------------------------------------------------------------
void CVehicle::RegisterHUDElement(CVehicleHUDElement* pHudElement)
{
	m_hudElements.push_back(pHudElement);
	pHudElement->AddRef();
}

//------------------------------------------------------------------------
void CVehicle::UnregisterHUDElement(CVehicleHUDElement* pHudElement)
{
	m_hudElements.remove(pHudElement);
	pHudElement->Release();
}

//------------------------------------------------------------------------
void CVehicle::UpdateHUD(float deltaTime)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	IConsole* pConsole = gEnv->pConsole;
	assert(pConsole);

	if (VehicleCVars().v_debugdraw > 0)
	{
		IRenderer* pRenderer = gEnv->pRenderer;
		float yPos = 500.0f;

		for (TVehicleHUDElementList::iterator ite = m_hudElements.begin(); ite != m_hudElements.end(); ++ite)
		{
			CVehicleHUDElement* pHudElement = *ite;
			pRenderer->Draw2dLabel(5.0f, yPos, 1.5f, pHudElement->m_color, false, pHudElement->m_message.c_str());
			yPos -= 12.0f;
		}
	}
}

//------------------------------------------------------------------------
TVehicleSeatId CVehicle::GetSeatId(const char* pSeatName)
{
	int seatId = InvalidVehicleSeatId + 1;

	for (TVehicleSeatVector::iterator ite = m_seats.begin(); ite != m_seats.end(); ++ite)
	{
		if (ite->first == pSeatName)
			return seatId;

		seatId++;
	}

	return InvalidVehicleSeatId;
}

//------------------------------------------------------------------------
TVehicleSeatId CVehicle::GetSeatId(CVehicleSeat* pSeat)
{
	int i = 1;

	for (TVehicleSeatVector::iterator ite = m_seats.begin(); ite != m_seats.end(); ++ite)
	{
		if (ite->second == pSeat)
		{
			return i;
		}

		i++;
	}

	return 0;
}

//------------------------------------------------------------------------
TVehicleSeatId CVehicle::GetDriverSeatId()
{
	for (TVehicleSeatVector::iterator ite = m_seats.begin(); ite != m_seats.end(); ++ite)
	{
		IVehicleSeat* pSeat = ite->second;
		if (pSeat->IsDriver())
			return pSeat->GetSeatId();
	}

	return InvalidVehicleSeatId;
}

//------------------------------------------------------------------------
IActor* CVehicle::GetDriver()
{
	if (CVehicleSeat* pSeat = (CVehicleSeat*)GetSeatById(GetDriverSeatId()))
		return pSeat->GetPassengerActor();

	return NULL;
}


//------------------------------------------------------------------------
IVehicleSeat* CVehicle::GetSeatForPassenger(EntityId passengerId)
{
	if (passengerId)
	{
		for (TVehicleSeatVector::iterator ite = m_seats.begin(); ite != m_seats.end(); ++ite)
		{
			CVehicleSeat* pSeat = ite->second;
			if (pSeat->GetPassenger() == passengerId && pSeat->GetCurrentTransition() != CVehicleSeat::eVT_RemoteUsage)
				return pSeat;
		}
	}

	return NULL;
}

//------------------------------------------------------------------------
IVehicleSeat* CVehicle::GetSeatById(const TVehicleSeatId seatId)
{
	if (seatId > 0 && seatId <= m_seats.size())
	{
		return m_seats[seatId - 1].second;
	}

	return NULL;
}

//------------------------------------------------------------------------
TVehicleSeatId CVehicle::GetLastSeatId()
{
	if (m_seats.size() > 0)
		return m_seats.size();
	else
		return InvalidVehicleSeatId;
}

//------------------------------------------------------------------------
TVehicleSeatId CVehicle::GetNextSeatId(const TVehicleSeatId currentSeatId)
{
	if (currentSeatId >= 0)
	{
		TVehicleSeatId seatId = currentSeatId + 1;
		if (seatId <= GetLastSeatId())
			return seatId;
		else
			return 1;
	}

	return InvalidVehicleSeatId;
}

//------------------------------------------------------------------------
void CVehicle::ChangeSeat(EntityId actorId, int seatChoice, TVehicleSeatId newSeatId)
{
	if (IsFlipped())
		return; // cannot change seats when vehicle is turned over

	IActorSystem* pActorSystem = gEnv->pGame->GetIGameFramework()->GetIActorSystem();
	assert(pActorSystem);

	IActor* pActor = pActorSystem->GetActor(actorId);
	if (!pActor)
		return;

	CVehicleSeat* pCurrentSeat = (CVehicleSeat*)GetSeatForPassenger(actorId);
	if (!pCurrentSeat)
		return;

	CVehicleSeat* pNewSeat = pCurrentSeat;
	CVehicleSeat* pSeat = pCurrentSeat;

	if (newSeatId != InvalidVehicleSeatId)
	{
		if (CVehicleSeat* pSeatById = (CVehicleSeat*)GetSeatById(newSeatId))
		{
			if (pSeatById->IsFree())
				pNewSeat = pSeatById;
		}
	}
	else
	{
		for (int i = 0; i < GetSeatCount(); i++)
		{
			TVehicleSeatId seatId = GetNextSeatId(pSeat->GetSeatId());
			pSeat = (CVehicleSeat*)GetSeatById(seatId);

			if (pSeat->IsFree()
				&& (seatChoice == 0 || pSeat->m_seatGroupIndex == seatChoice))
			{
				pNewSeat = pSeat;
				break;
			}
		}
	}

	if (pNewSeat && pNewSeat != pCurrentSeat)
	{
		if (pNewSeat->GetPassenger(true)) // remote or dead 
			pNewSeat->Exit(false);

		EntityId passengerId = pCurrentSeat->GetPassenger();

		pActor->GetEntity()->GetCharacter(0)->GetISkeletonAnim()->StopAnimationsAllLayers();

		pCurrentSeat->GivesActorSeatFeatures(false);
		pCurrentSeat->SetAnimGraphInputs(actorId, false, false);

		// allow lua side of the seat implementation to do its business
		HSCRIPTFUNCTION scriptFunction(0);
		IScriptSystem* pIScriptSystem = gEnv->pScriptSystem;
		if (IScriptTable* pScriptTable = GetEntity()->GetScriptTable())
		{
			if (pActor->IsPlayer())
			{
				if (pScriptTable->GetValue("OnActorStandUp", scriptFunction))
				{
					assert(scriptFunction);
					ScriptHandle passengerHandle(pCurrentSeat->m_passengerId);
					Script::Call(pIScriptSystem, scriptFunction, pScriptTable, passengerHandle, false);
					pIScriptSystem->ReleaseFunc(scriptFunction);
				}
			}
			else
			{
				if (pScriptTable->GetValue("OnActorChangeSeat", scriptFunction))
				{
					assert(scriptFunction);
					ScriptHandle passengerHandle(pCurrentSeat->m_passengerId);
					Script::Call(pIScriptSystem, scriptFunction, pScriptTable, passengerHandle, false);
					pIScriptSystem->ReleaseFunc(scriptFunction);
				}
			}
		}

		pCurrentSeat->m_passengerId = 0;

		if (IVehicleClient* pVehicleClient = gEnv->pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicleClient())
			pVehicleClient->OnExitVehicleSeat(pCurrentSeat);

		pNewSeat->m_passengerId = actorId;
		pNewSeat->SetAnimGraphInputs(actorId, true, false);
		pNewSeat->SitDown();

		//CHANGED_NETWORK_STATE(this, ASPECT_SEAT_PASSENGER);
		GetGameObject()->ChangedNetworkState(ASPECT_SEAT_PASSENGER);

		SVehicleEventParams eventParams;
		eventParams.entityId = passengerId;
		eventParams.iParam = pCurrentSeat->GetSeatId();
		BroadcastVehicleEvent(eVE_SeatFreed, eventParams);

		eventParams.entityId = actorId;
		eventParams.iParam = pNewSeat->GetSeatId();
		BroadcastVehicleEvent(eVE_PassengerChangeSeat, eventParams);
	}
}

//------------------------------------------------------------------------
void CVehicle::RegisterVehicleEventListener(IVehicleEventListener* pEvenListener, const char* name)
{
	m_eventListeners[pEvenListener] = TVehicleListenerName(name);
}

//------------------------------------------------------------------------
void CVehicle::UnregisterVehicleEventListener(IVehicleEventListener* pEventListener)
{
	std::erase_if(m_eventListeners, [pEventListener](const auto& pair) {
		return pair.first == pEventListener;
	});	
}

//------------------------------------------------------------------------
void CVehicle::BroadcastVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
{
	/*if (event == eVE_BeingFlipped) //CryMP: fixme
	{
		m_status.beingFlipped = params.bParam;
		return;
	}*/

	if (event == eVE_PassengerEnter) // need to setup listener _before_ the listener event is sent (which params contain seat id).
	{
		//m_pVehicleSystem->BroadcastVehicleUsageEvent(event, params.entityId/*playerId*/, this);
	}

	TVehicleEventListenerList::iterator ite = m_eventListeners.begin();
	TVehicleEventListenerList::const_iterator end = m_eventListeners.end();
	for (; ite != end; ++ite)
	{
		IVehicleEventListener* pEventListener = ite->first;
		pEventListener->OnVehicleEvent(event, params);
	}

	TVehicleSeatVector::iterator seat_it = m_seats.begin();
	TVehicleSeatVector::const_iterator seat_end = m_seats.end();
	for (; seat_it != seat_end; seat_it++)
	{
		CVehicleSeat* pSeat = seat_it->second;
		pSeat->OnVehicleEvent(event, params);
	}

	switch (event)
	{
	case eVE_Collision:
	{
		break;
	}
	case eVE_PassengerExit:
	case eVE_PassengerEnter:
	{
		if (params.iParam == 1) // driver seat
		{
			IVehiclePart::SVehiclePartEvent partEvent;
			partEvent.type = (eVE_PassengerEnter == event) ? IVehiclePart::eVPE_DriverEntered : IVehiclePart::eVPE_DriverLeft;

			for (TVehiclePartVector::iterator it = m_parts.begin(), end = m_parts.end(); it != end; ++it)
				it->second->OnEvent(partEvent);
		}
		break;
	}
	case eVE_Indestructible:
	{
		m_indestructible = params.bParam;
		break;
	}
	case eVE_PreVehicleDeletion:
	{
		SVehicleEventParams seatEventParams;
		for (seat_it = m_seats.begin(); seat_it != seat_end; ++seat_it)
		{
			CVehicleSeat* pSeat = seat_it->second;
			EntityId playerId = pSeat->GetPassenger();
			if (playerId)
			{
				seatEventParams.iParam = pSeat->GetSeatId();
				seatEventParams.entityId = playerId;
				BroadcastVehicleEvent(eVE_PassengerExit, seatEventParams);
			}
		}

		for (TVehiclePartVector::iterator it = m_parts.begin(), end = m_parts.end(); it != end; ++it)
			it->second->OnVehicleEvent(event, params);
		break;
	}
	default:
		break;
	}
}

//------------------------------------------------------------------------
bool CVehicle::IsDestroyed()
{
	return m_isDestroyed;
}

bool CVehicle::IsSubmerged()
{
	return (m_status.submergedRatio >= (GetDamageParams().submergedRatioMax - 0.001f));
}

//------------------------------------------------------------------------
EntityId CVehicle::GetCurrentWeaponId(EntityId passengerId, bool secondary/*=false*/)
{
	if (CVehicleSeat* pSeat = (CVehicleSeat*)GetSeatForPassenger(passengerId))
	{
		int transition = pSeat->GetCurrentTransition();
		if (!(transition == CVehicleSeat::eVT_None || transition == CVehicleSeat::eVT_RemoteUsage))
			return 0;

		TVehicleSeatActionVector& seatActions = pSeat->GetSeatActions();

		for (TVehicleSeatActionVector::iterator it = seatActions.begin(); it != seatActions.end(); ++it)
		{
			IVehicleSeatAction* pSeatAction = it->pSeatAction;
			if (CVehicleSeatActionWeapons* weapAction = CAST_VEHICLEOBJECT(CVehicleSeatActionWeapons, pSeatAction))
			{
				bool match = secondary ? weapAction->IsSecondary() : !weapAction->IsSecondary();
				if (match)
					return weapAction->GetWeaponEntityId(0);
			}
		}

		if (CVehicleSeat* pRemoteSeat = pSeat->GetSeatUsedRemotely(true))
		{
			TVehicleSeatActionVector& remoteSeatActions = pRemoteSeat->GetSeatActions();
			for (TVehicleSeatActionVector::iterator it = remoteSeatActions.begin(); it != remoteSeatActions.end(); ++it)
			{
				if (!it->isAvailableRemotely)
					continue;

				if (CVehicleSeatActionWeapons* weapAction = CAST_VEHICLEOBJECT(CVehicleSeatActionWeapons, it->pSeatAction))
				{
					bool match = secondary ? weapAction->IsSecondary() : !weapAction->IsSecondary();
					if (match)
						return weapAction->GetWeaponEntityId(0);
				}
			}
		}
	}

	return 0;
}


//------------------------------------------------------------------------
void CVehicle::SetObjectUpdate(IVehicleObject* pObject, EVehicleObjectUpdate updatePolicy)
{
	assert(pObject);

	for (TVehicleObjectUpdateInfoList::iterator ite = m_objectsToUpdate.begin(); ite != m_objectsToUpdate.end(); ++ite)
	{
		SObjectUpdateInfo& updateInfo = *ite;
		if (updateInfo.pObject == pObject)
		{
			updateInfo.updatePolicy = updatePolicy;
			return;
		}
	}

	if (updatePolicy != eVOU_NoUpdate)
	{
		// it wasn't found, it needs to be added
		SObjectUpdateInfo newUpdateInfo;
		newUpdateInfo.pObject = pObject;
		newUpdateInfo.updatePolicy = updatePolicy;
		m_objectsToUpdate.push_back(newUpdateInfo);
	}
}

//------------------------------------------------------------------------
bool CVehicle::IsPlayerDriving(bool clientOnly)
{
	CVehicleSeat* pSeat = (CVehicleSeat*)GetSeatById(GetDriverSeatId());

	if (!pSeat)
		return false;

	IActor* pActor = pSeat->GetPassengerActor();

	if (pActor)
	{
		if (clientOnly)
			return pActor->IsClient();
		else
			return pActor->IsPlayer();
	}

	return false;
}

//------------------------------------------------------------------------
bool CVehicle::HasFriendlyPassenger(IEntity* pPlayer)
{
	if (!pPlayer || !pPlayer->GetAI())
		return false;

	for (int i = 1; i <= GetSeatCount(); ++i)
	{
		CVehicleSeat* pSeat = (CVehicleSeat*)GetSeatById(i);
		if (!pSeat)
			continue;

		IActor* pActor = pSeat->GetPassengerActor();

		if (pActor)
		{
			if (pActor->GetEntityId() == pPlayer->GetId())
				continue; // the requester itself doesn't count as a passenger

			IAIObject* aiO = pActor->GetEntity()->GetAI();
			if (aiO && !aiO->IsHostile(pPlayer->GetAI(), false))
				return true;
		}
	}

	return false;
}

//------------------------------------------------------------------------
bool CVehicle::IsPlayerPassenger()
{
	IActor* pActor = gEnv->pGame->GetIGameFramework()->GetClientActor();
	if (pActor == NULL)
		return false;

	return (pActor->GetLinkedVehicle() == this);
}

//------------------------------------------------------------------------
IVehiclePart* CVehicle::GetPart(unsigned int index)
{
	if (index >= m_parts.size())
		return NULL;

	return m_parts[index].second;
}

//------------------------------------------------------------------------
void CVehicle::GetParts(IVehiclePart** parts, int nMax)
{
	for (int i = 0; i < m_parts.size() && i < nMax; ++i)
		parts[i] = m_parts[i].second;
}

//------------------------------------------------------------------------
IVehiclePart* CVehicle::GetWheelPart(int idx)
{
	assert(idx >= 0 && idx < GetWheelCount());

	int i = -1;
	for (TVehiclePartVector::iterator it = m_parts.begin(); it != m_parts.end(); ++it)
	{
		if (CAST_VEHICLEOBJECT(CVehiclePartSubPartWheel, it->second))
			if (++i == idx)
				return it->second;
	}

	return NULL;
}

//------------------------------------------------------------------------
IVehicleAction* CVehicle::GetAction(int index)
{
	if (index < 0 || index >= m_actions.size())
		return NULL;

	SVehicleActionInfo& actionInfo = m_actions[index];
	return actionInfo.pAction;
}

//------------------------------------------------------------------------
IVehicleHelper* CVehicle::GetHelper(const char* pName)
{
	if (!pName)
		return NULL;

	TVehicleHelperMap::iterator it = m_helpers.find(CONST_TEMP_STRING(pName));
	if (it != m_helpers.end())
		return it->second;

	for (TVehiclePartVector::iterator ite = m_parts.begin(); ite != m_parts.end(); ite++)
	{
		IVehiclePart* pPart = ite->second;
		if (CVehiclePartAnimated* pAnimatedPart = CAST_VEHICLEOBJECT(CVehiclePartAnimated, pPart))
		{
			if (ICharacterInstance* pCharInstance =
				pAnimatedPart->GetEntity()->GetCharacter(pAnimatedPart->GetSlot()))
			{
				ISkeletonPose* pSkeletonPose = pCharInstance->GetISkeletonPose();
				assert(pSkeletonPose);

				int16 id = pSkeletonPose->GetJointIDByName(pName);
				if (id > -1)
				{
					CVehicleHelper* pHelper = new CVehicleHelper;

					pHelper->m_localTM = Matrix34(pSkeletonPose->GetRelJointByID(id));
					pHelper->m_pParentPart = pPart;
					pHelper->m_isVehicleUpdated = false;
					pHelper->m_isWorldUpdated = false;

					m_helpers.insert(TVehicleHelperMap::value_type(pName, pHelper));
					return pHelper;
				}
			}
			break;
		}
	}

	TVehicleComponentVector::iterator compIte = m_components.begin();
	TVehicleComponentVector::iterator compEnd = m_components.end();

	for (; compIte != compEnd; ++compIte)
	{
		CVehicleComponent* pComponent = *compIte;

		if (pComponent->GetName() == pName)
		{
			CVehicleHelper* pHelper = new CVehicleHelper;

			if (pComponent->GetPartCount() > 0)
			{
				pHelper->m_pParentPart = pComponent->GetPart(0);
			}
			else
			{
				TVehiclePartVector::iterator partIte = m_parts.begin();
				TVehiclePartVector::iterator partEnd = m_parts.end();

				for (; partIte != partEnd; ++partIte)
				{
					IVehiclePart* pPart = partIte->second;
					if (pPart->GetParent() == NULL)
					{
						pHelper->m_pParentPart = pPart;
						break;
					}
				}
			}

			if (pHelper->m_pParentPart)
			{
				pHelper->m_vehicleTM.SetIdentity();
				pHelper->m_vehicleTM.SetTranslation(pComponent->GetBounds().GetCenter());

				pHelper->m_localTM = pHelper->m_pParentPart->GetLocalTM(false).GetInverted() * pHelper->m_vehicleTM;
				pHelper->m_isVehicleUpdated = true;
				pHelper->m_isWorldUpdated = false;

				m_helpers.insert(TVehicleHelperMap::value_type(pName, pHelper));
				return pHelper;
			}
			else
				delete pHelper;

			break;
		}
	}

	return nullptr;
}

//------------------------------------------------------------------------
int CVehicle::GetComponentCount() const
{
	return (int)m_components.size();
}

//------------------------------------------------------------------------
IVehicleComponent* CVehicle::GetComponent(int index)
{
	if (index >= 0 && index < m_components.size())
	{
		return m_components[index];
	}
	return NULL;
}

//------------------------------------------------------------------------
bool CVehicle::SetAspectProfile(EEntityAspects aspect, uint8 profile)
{
	if (aspect == eEA_Physics)
	{
		switch (profile)
		{
		case eVPhys_NotPhysicalized:
		{
			m_physicsParams.nSlot = -1;
			m_physicsParams.type = PE_NONE;
			GetEntity()->Physicalize(m_physicsParams);
		}
		return true;
		case eVPhys_Physicalized:
		{
			m_physicsParams.nSlot = -1;
			m_physicsParams.type = PE_RIGID;
			m_physicsParams.mass = m_mass;
			m_physicsParams.pCar = 0;

			if (!(m_pMovement && m_pMovement->GetPhysicalizationType() == PE_WHEELEDVEHICLE))
				GetEntity()->Physicalize(m_physicsParams);

			if (m_pMovement)
				m_pMovement->Physicalize();

			IPhysicalEntity* pPhysics = GetEntity()->GetPhysics();
			if (!pPhysics)
				return false;

			pe_params_buoyancy buoyancyParams(m_buoyancyParams);
			pPhysics->SetParams(&buoyancyParams);

			pe_simulation_params simParams(m_simParams);
			pPhysics->SetParams(&simParams);

			pe_params_flags paramsFlags(m_paramsFlags);
			pPhysics->SetParams(&paramsFlags);

			for (TVehiclePartVector::iterator ite = m_parts.begin(); ite != m_parts.end(); ite++)
			{
				IVehiclePart* pPart = ite->second;
				pPart->Physicalize();
			}

			GetGameObject()->EnablePhysicsEvent(true, eEPE_OnPostStepImmediate);

			//if (pPhysics->GetParams(&paramsFlags))
			{
				//paramsFlags.flags = paramsFlags.flags | pef_monitor_poststep | pef_log_collisions | pef_fixed_damping;
				pe_params_flags flags;
				flags.flagsOR = pef_monitor_poststep | pef_log_collisions | pef_fixed_damping;
				pPhysics->SetParams(&flags);
			}

			if (pPhysics->GetParams(&simParams))
				m_gravity = simParams.gravity;

			if (m_bRetainGravity)
			{
				pe_params_flags flags;
				flags.flagsOR = pef_ignore_areas;
				pPhysics->SetParams(&flags);
			}

			if (m_pMovement)
				m_pMovement->PostPhysicalize();

			if (!gEnv->pGame->GetIGameFramework()->IsEditing())
				NeedsUpdate(eVUF_AwakePhysics);
		}
		return true;
		}
	}

	return false;
}

//------------------------------------------------------------------------
const char* CVehicle::GetSoundName(const char* eventName)
{
	if (strchr(eventName, ':'))
		return eventName;

	static char buffer[256];
	_snprintf(buffer, 256, "sounds/vehicles:%s:%s", GetEntity()->GetClass()->GetName(), eventName);
	buffer[sizeof(buffer) - 1] = '\0';

	return buffer;
}

//------------------------------------------------------------------------
bool CVehicle::EventIdValid(TVehicleSoundEventId eventId)
{
	return m_pSoundProxy && eventId > -1 && eventId < m_soundEvents.size();
}

//------------------------------------------------------------------------
SVehicleSoundInfo* CVehicle::GetSoundInfo(TVehicleSoundEventId eventId)
{
	if (!EventIdValid(eventId))
		return NULL;

	return &m_soundEvents[eventId];
}

//------------------------------------------------------------------------
ISound* CVehicle::GetSound(TVehicleSoundEventId eventId, bool start/*=true*/)
{
	if (!EventIdValid(eventId))
		return NULL;

	SVehicleSoundInfo& info = m_soundEvents[eventId];
	ISound* pSound = m_pSoundProxy->GetSound(info.soundId);

	if (start && (!pSound || !pSound->IsPlaying()))
	{
		const char* name = GetSoundName(info.name.c_str());
		const Matrix34& tm = info.pHelper->GetVehicleTM();

		info.soundId = m_pSoundProxy->PlaySound(name, tm.GetTranslation(), tm.GetColumn1(), FLAG_SOUND_DEFAULT_3D, eSoundSemantic_Vehicle);
	}

	return m_pSoundProxy->GetSound(info.soundId);
}


//------------------------------------------------------------------------
void CVehicle::SetSoundParam(TVehicleSoundEventId eventId, const char* param, float value, bool start/*=true*/)
{
	if (ISound* pSound = GetSound(eventId, start))
	{
		pSound->SetParam(param, value, false);
	}
}

//------------------------------------------------------------------------
void CVehicle::StopSound(TVehicleSoundEventId eventId)
{
	SVehicleSoundInfo* pInfo = GetSoundInfo(eventId);
	if (!pInfo)
		return;

	if (ISound* pSound = m_pSoundProxy->GetSound(pInfo->soundId))
	{
		if (pSound->IsPlaying())
			pSound->Stop();
	}

	pInfo->soundId = INVALID_SOUNDID;
}

//------------------------------------------------------------------------
void CVehicle::StartAbandonedTimer(bool force, float timer)
{
	if (IsDestroyed() || gEnv->bEditor || !gEnv->bServer)
		return;

	SmartScriptTable props, respawnprops;
	if (!GetEntity()->GetScriptTable()->GetValue("Properties", props) || !props->GetValue("Respawn", respawnprops))
		return;

	if (!force)
	{
		int abandon = 0;
		if (!respawnprops->GetValue("bAbandon", abandon) || !abandon)
			return;

		float dmg = GetDamageRatio();
		if (dmg == 0.0f)
		{
			float dp = (m_initialposition - GetEntity()->GetWorldPos()).len2();
			if (dp < 3.0f * 3.0f)
				return;
		}

		if (timer < 0.0f)
			respawnprops->GetValue("nAbandonTimer", timer);
	}

	const float mintime = 5.0f;

	IEntity* pEntity = GetEntity();

	pEntity->KillTimer(eVT_Abandoned);
	pEntity->SetTimer(eVT_Abandoned, (int)(max(mintime + 0.5f, timer) * 1000.0f));

	pEntity->KillTimer(eVT_AbandonedSound);
	pEntity->SetTimer(eVT_AbandonedSound, (int)(max(0.0f, ((timer + 0.5f) - mintime)) * 1000.0f)); // warn sound
}

//------------------------------------------------------------------------
void CVehicle::KillAbandonedTimer()
{
	if (!gEnv->bServer)
		return;

	GetEntity()->KillTimer(eVT_Abandoned);
	GetEntity()->KillTimer(eVT_AbandonedSound);

	EnableAbandonedWarnSound(false);
}

//------------------------------------------------------------------------
void CVehicle::Destroy()
{
	if (!gEnv->bServer || IsDestroyed())
		return;

	// force vehicle destruction
	for (TVehicleComponentVector::iterator it = m_components.begin(); it != m_components.end(); ++it)
	{
		CVehicleComponent* pComponent = *it;
		pComponent->OnHit(GetEntityId(), 0, 50000.0f, ZERO, 0.0f, "normal");
	}
}

//------------------------------------------------------------------------
void CVehicle::EnableAbandonedWarnSound(bool enable)
{
	if (gEnv->bClient)
	{
		// kill old 
		if (m_abandonedSoundId != INVALID_SOUNDID)
		{
			if (ISound* pSound = m_pSoundProxy->GetSound(m_abandonedSoundId))
				m_pSoundProxy->StopSound(m_abandonedSoundId);

			m_abandonedSoundId = INVALID_SOUNDID;
		}

		if (enable)
		{
			m_abandonedSoundId = m_pSoundProxy->PlaySound("sounds/interface:multiplayer_interface:mp_vehicle_alarm", Vec3(0), FORWARD_DIRECTION, FLAG_SOUND_DEFAULT_3D, eSoundSemantic_Vehicle);
		}
	}

	if (gEnv->bServer)
		GetGameObject()->InvokeRMIWithDependentObject(ClAbandonWarning(), AbandonWarningParams(enable), eRMI_ToRemoteClients, GetEntityId());
}

//------------------------------------------------------------------------
bool CVehicle::IsFlipped(float maxSpeed)
{
	const SVehicleStatus& status = GetStatus();
	const Matrix34& worldTM = GetEntity()->GetWorldTM();
	const static Vec3 up(0, 0, 1);

	bool flipped = !IsDestroyed() && worldTM.GetColumn2().z < 0.1f && (status.contacts || status.submergedRatio > 0.1f);

	return flipped;
}

//------------------------------------------------------------------------
void CVehicle::CheckFlippedStatus(const float deltaTime)
{
	// check flipped over status      
	static const float activationTime = 20.f;
	static const float activationTimeAI = 5.f;

	float initialTime = activationTimeAI;
	float prev = m_status.flipped;
	bool flipped = IsFlipped();

	if (flipped)
	{
		m_status.flipped = max(0.f, m_status.flipped);
		m_status.flipped += deltaTime;

		bool ai = false;
		// go through all seats, not just driver...
		for (TVehicleSeatVector::iterator it = m_seats.begin(); it != m_seats.end(); ++it)
		{
			IVehicleSeat* pSeat = it->second;
			if (pSeat && pSeat->GetPassenger())
			{
				IActor* pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pSeat->GetPassenger());
				if (pActor && !pActor->IsPlayer())
				{
					ai = true;
					break;
				}
			}
		}

		if (!ai)
			initialTime = activationTime;

		if (m_status.flipped <= initialTime)
			NeedsUpdate();

		if (m_status.flipped > initialTime && prev <= initialTime)
			ProcessFlipped();
	}
	else
	{
		m_status.flipped = min(0.f, m_status.flipped);
		m_status.flipped -= deltaTime;

		if (prev >= initialTime)
		{
			KillTimer(eVT_Flipped);
		}
	}

	bool flipping = m_status.flipped > 0.5f && prev <= 0.5f;
	bool unflipping = m_status.flipped < -0.25f && prev >= -0.25f;

	if (flipping || unflipping)
	{
		if (VehicleCVars().v_debugdraw)
			CryLog("%s: %s (%.2f)", GetEntity()->GetName(), flipping ? "flipping" : "unflipping", m_status.flipped);

		IVehiclePart::SVehiclePartEvent partEvent;
		partEvent.type = IVehiclePart::eVPE_FlippedOver;
		partEvent.bparam = flipping;

		for (TVehiclePartVector::iterator it = m_parts.begin(), end = m_parts.end(); it != end; ++it)
		{
			IVehiclePart* pPart = it->second;
			pPart->OnEvent(partEvent);
		}
	}

	// if flipping, kill AI in exposed seats (eg LTV gunners)
	if (gEnv->bServer && m_status.flipped > 0.1f && prev < 0.1f)
	{
		KillPassengersInExposedSeats(true);
	}
}

//------------------------------------------------------------------------
void CVehicle::ProcessFlipped()
{
	if (!gEnv->bServer)
		return;

	if (!IsFlipped())
		return;

	if (m_indestructible)
		return;

	const Matrix34& worldTM = GetEntity()->GetWorldTM();
	bool playerClose = false;

	bool ai = false;
	// go through all seats, not just driver...
	for (TVehicleSeatVector::iterator it = m_seats.begin(); it != m_seats.end(); ++it)
	{
		IVehicleSeat* pSeat = it->second;
		if (pSeat && pSeat->GetPassenger())
		{
			IActor* pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pSeat->GetPassenger());
			if (pActor && !pActor->IsPlayer())
			{
				ai = true;
				break;
			}
		}
	}

	if (!ai)
	{
		// if AI guys inside, we blow up in any case 
		// if not, we only blow up if no players are around
		static const float r = 25.f;
		IActorSystem* pActorSystem = gEnv->pGame->GetIGameFramework()->GetIActorSystem();

		if (!gEnv->bMultiplayer)
		{
			IActor* pActor = gEnv->pGame->GetIGameFramework()->GetClientActor();
			if (pActor)
			{
				float distSq = pActor->GetEntity()->GetWorldPos().GetSquaredDistance(worldTM.GetTranslation());
				if (distSq < r * r)
					playerClose = true;
			}
		}
		else
		{
			SEntityProximityQuery query;
			query.box = AABB(worldTM.GetTranslation() - Vec3(r, r, r), worldTM.GetTranslation() + Vec3(r, r, r));

			int count = gEnv->pEntitySystem->QueryProximity(query);
			for (int i = 0; i < query.nCount; ++i)
			{
				IEntity* pEntity = query.pEntities[i];
				if (IActor* pActor = pActorSystem->GetActor(pEntity->GetId()))
				{
					if (pActor->IsPlayer())
					{
						playerClose = true;
						break;
					}
				}
			}
		}
	}

	if (playerClose)
	{
		// check again later        
		SetTimer(eVT_Flipped, 10000, 0);
	}
	else
	{
		if (CVehicleComponent* pComponent = (CVehicleComponent*)GetComponent("Engine"))
			pComponent->OnHit(GetEntityId(), GetEntityId(), 10000.f, Vec3(0), 0.f, "");

		if (CVehicleComponent* pComponent = (CVehicleComponent*)GetComponent("FlippedOver"))
			pComponent->OnHit(GetEntityId(), GetEntityId(), 10000.f, Vec3(0), 0.f, "");

		KillTimer(eVT_Flipped);
	}
}


//------------------------------------------------------------------------
bool CVehicle::IsEmpty()
{
	int count = 0;
	for (TVehicleSeatVector::iterator it = m_seats.begin(); it != m_seats.end(); ++it)
	{
		IVehicleSeat* pSeat = it->second;
		if (pSeat->GetPassenger())
			count++;
	}

	return count == 0;
}

//------------------------------------------------------------------------
bool CVehicle::InitRespawn()
{
	if (gEnv->bServer)
	{
		IScriptTable* pScriptTable = GetEntity()->GetScriptTable();
		if (pScriptTable)
		{
			SmartScriptTable props, respawnprops;
			if (pScriptTable->GetValue("Properties", props) && props->GetValue("Respawn", respawnprops))
			{
				int respawn = 0;
				if (respawnprops->GetValue("bRespawn", respawn) && respawn)
				{
					if (IGameRules* pGameRules = gEnv->pGame->GetIGameFramework()->GetIGameRulesSystem()->GetCurrentGameRules())
						pGameRules->CreateEntityRespawnData(GetEntityId());

					return true;
				}
			}
		}
	}

	return false;
}

//------------------------------------------------------------------------
void CVehicle::OnDestroyed()
{
	EnableAbandonedWarnSound(false);

	DumpParts();

	IGameRules* pGameRules = gEnv->pGame->GetIGameFramework()->GetIGameRulesSystem()->GetCurrentGameRules();
	if (pGameRules)
		pGameRules->OnVehicleDestroyed(GetEntityId());

	if (gEnv->bServer && !gEnv->bEditor)
	{
		bool mp = gEnv->bMultiplayer;

		SmartScriptTable props, respawnprops;
		if (GetEntity()->GetScriptTable()->GetValue("Properties", props) && props->GetValue("Respawn", respawnprops))
		{
			int respawn = 0;
			if (respawnprops->GetValue("bRespawn", respawn) && respawn)
			{
				int timer = 0;
				int unique = 0;

				respawnprops->GetValue("nTimer", timer);
				respawnprops->GetValue("bUnique", unique);

				float rtimer = max(0.0f, timer - 5.0f);
				if (!mp)
					rtimer = timer + 20.0f;

				if (!mp)
					unique = 0; // forced to false in singleplayer to avoid unintuitive behaviour

				if (pGameRules)
				{
					pGameRules->ScheduleEntityRemoval(GetEntityId(), rtimer, !mp);
					pGameRules->ScheduleEntityRespawn(GetEntityId(), unique != 0, (float)timer);
				}

				return;
			}
		}

		// if it's not set to respawn, remove it after the set time
		if (mp && pGameRules)
			pGameRules->ScheduleEntityRemoval(GetEntityId(), 15.0f, false);
	}

	TVehicleAnimationsVector::iterator animIte = m_animations.begin();
	TVehicleAnimationsVector::iterator animIteEnd = m_animations.end();

	for (; animIte != animIteEnd; ++animIte)
	{
		IVehicleAnimation* pAnim = animIte->second;
		pAnim->StopAnimation();
	}

	m_status.health = 0.0f;
}


//------------------------------------------------------------------------
void CVehicle::OnSubmerged(float ratio)
{
	if (gEnv->bMultiplayer && gEnv->bServer && IsEmpty())
		StartAbandonedTimer();

	if (IGameRules* pGameRules = gEnv->pGame->GetIGameFramework()->GetIGameRulesSystem()->GetCurrentGameRules())
		pGameRules->OnVehicleSubmerged(GetEntityId(), ratio);
}

//------------------------------------------------------------------------
int CVehicle::GetNextPhysicsSlot(bool high) const
{
	// use partid starting from 100 for parts without slot geometry
	// their partid must not mix up with other entity slots
	int idMax = (high) ? 100 : -1;

	// get next physid not belonging to CGA range
	for (TVehiclePartVector::const_iterator ite = m_parts.begin(); ite != m_parts.end(); ite++)
	{
		int physId = ite->second->GetPhysId();

		if (physId > idMax && physId < PARTID_CGA)
			idMax = physId;
	}

	return idMax + 1;
}

//------------------------------------------------------------------------
CVehiclePartAnimated* CVehicle::GetVehiclePartAnimated()
{
	TVehiclePartVector::const_iterator end = m_parts.end();
	for (TVehiclePartVector::const_iterator it = m_parts.begin(); it != end; ++it)
	{
		if (CVehiclePartAnimated* pPartAnimated = CAST_VEHICLEOBJECT(CVehiclePartAnimated, it->second))
		{
			// since anyway there shouldn't be more than one of such class
			return pPartAnimated;
		}
	}

	return NULL;
}


//------------------------------------------------------------------------
bool CVehicle::CanEnter(EntityId userId)
{
	bool result = true;

	if (IScriptTable* pScriptTable = GetEntity()->GetScriptTable())
	{
		HSCRIPTFUNCTION pfnCanEnter = 0;
		if (pScriptTable->GetValue("CanEnter", pfnCanEnter) && pfnCanEnter)
		{
			IScriptSystem* pIScriptSystem = gEnv->pScriptSystem;
			Script::CallReturn(pIScriptSystem, pfnCanEnter, pScriptTable, ScriptHandle(userId), result);
			pIScriptSystem->ReleaseFunc(pfnCanEnter);
		}
	}

	return result;
}

//------------------------------------------------------------------------
float CVehicle::GetAltitude()
{
	if (iszero(m_status.altitude))
	{
		Vec3 worldPos = GetEntity()->GetWorldPos();
		ray_hit rayHits[10];

		m_status.altitude = 0.0f;

		float terrainZ = gEnv->p3DEngine->GetTerrainElevation(worldPos.x, worldPos.y);
		m_status.altitude = worldPos.z - terrainZ;

		if (int hitCount = gEnv->pPhysicalWorld->RayWorldIntersection(worldPos, Vec3(0.0f, 0.0f, -m_status.altitude),
			ent_static | ent_sleeping_rigid | ent_rigid | ent_independent | ent_terrain, rwi_colltype_any | rwi_ignore_noncolliding,
			&rayHits[0], 10, GetEntity()->GetPhysics()))
		{
			for (int i = 0; i < hitCount; i++)
				if (rayHits[i].dist >= 0.0f)
					m_status.altitude = min(rayHits[i].dist, m_status.altitude);
		}
	}

	return m_status.altitude;
}

//------------------------------------------------------------------------
int CVehicle::GetWeaponCount() const
{
	int count = 0;

	TVehicleSeatVector::const_iterator itSeatsEnd = m_seats.end();
	for (TVehicleSeatVector::const_iterator it = m_seats.begin(); it != itSeatsEnd; ++it)
	{
		TVehicleSeatActionVector::const_iterator itActionsEnd = it->second->GetSeatActions().end();
		for (TVehicleSeatActionVector::const_iterator itActions = it->second->GetSeatActions().begin(); itActions != itActionsEnd; ++itActions)
		{
			if (CVehicleSeatActionWeapons* pAction = CAST_VEHICLEOBJECT(CVehicleSeatActionWeapons, itActions->pSeatAction))
			{
				count += pAction->GetWeaponCount();
			}
		}
	}

	return count;
}

//------------------------------------------------------------------------
EntityId CVehicle::GetWeaponId(int index) const
{
	int count = 0;

	TVehicleSeatVector::const_iterator itSeatsEnd = m_seats.end();
	for (TVehicleSeatVector::const_iterator it = m_seats.begin(); it != itSeatsEnd; ++it)
	{
		TVehicleSeatActionVector::const_iterator itActionsEnd = it->second->GetSeatActions().end();
		for (TVehicleSeatActionVector::const_iterator itActions = it->second->GetSeatActions().begin(); itActions != itActionsEnd; ++itActions)
		{
			if (CVehicleSeatActionWeapons* pAction = CAST_VEHICLEOBJECT(CVehicleSeatActionWeapons, itActions->pSeatAction))
			{
				for (int i = 0; i < pAction->GetWeaponCount(); ++i)
				{
					if (count++ == index)
						return pAction->GetWeaponEntityId(i);
				}
			}
		}
	}
	return 0;
}

//------------------------------------------------------------------------
float CVehicle::GetFrozenAmount() const
{
	return m_status.frozenAmount;
}

//------------------------------------------------------------------------
void CVehicle::SetFrozenAmount(float frost)
{
	m_status.frozenAmount = min(1.0f, frost);
	NeedsUpdate();
}

//------------------------------------------------------------------------
void CVehicle::GetMemoryStatistics(ICrySizer* s)
{
	s->Add(*this);
	s->AddContainer(m_pParticleEffects);
	s->AddContainer(m_transitionInfo);

	GetDamagesMemoryStatistics(s);

	if (m_pMovement)
	{
		SIZER_COMPONENT_NAME(s, "Movement");
		m_pMovement->GetMemoryStatistics(s);
	}

	{
		SIZER_COMPONENT_NAME(s, "Seats");
		s->AddContainer(m_seats);
		for (TVehicleSeatVector::iterator iter = m_seats.begin(); iter != m_seats.end(); ++iter)
		{
			s->Add(iter->first);
			iter->second->GetMemoryStatistics(s);
		}
	}

	{
		SIZER_COMPONENT_NAME(s, "SeatGroups");
		s->AddContainer(m_seatGroups);
		for (size_t i = 0; i < m_seatGroups.size(); i++)
			m_seatGroups[i]->GetMemoryStatistics(s);
	}

	{
		SIZER_COMPONENT_NAME(s, "Components");
		s->AddContainer(m_components);
		for (size_t i = 0; i < m_components.size(); i++)
			m_components[i]->GetMemoryStatistics(s);
	}

	{
		SIZER_COMPONENT_NAME(s, "Parts");
		s->AddContainer(m_parts);
		for (size_t i = 0; i < m_parts.size(); i++)
		{
			s->Add(m_parts[i].first);
			m_parts[i].second->GetMemoryStatistics(s);
		}
	}
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CVehicle, SvRequestUse)
{
	OnUsed(params.actorId, params.index);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CVehicle, SvRequestChangeSeat)
{
	ChangeSeat(params.actorId, params.seatChoice);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CVehicle, SvRequestLeave)
{
	ExitVehicleAtPosition(params.actorId, Vec3(ZERO)); //CryMP: fixme

	return true;
}

/* //CryMP: Not available in Crysis 1..
//------------------------------------------------------------------------
IMPLEMENT_RMI(CVehicle, ClRequestLeave)
{
	IVehicleSeat* pCurrentSeat = GetSeatForPassenger(params.actorId);
	if (pCurrentSeat)
		pCurrentSeat->Exit(true, false);

	return true;
}
*/
//------------------------------------------------------------------------
IMPLEMENT_RMI(CVehicle, ClSetupWeapons)
{
	int nseats = (int)params.seats.size();
	for (int s = 0;s < nseats;s++)
	{
		CVehicleSeat* pSeat = m_seats[s].second;
		TVehicleSeatActionVector& seatActions = pSeat->GetSeatActions();

		int iaction = 0;
		for (TVehicleSeatActionVector::iterator ait = seatActions.begin(); ait != seatActions.end(); ++ait)
		{
			IVehicleSeatAction* pSeatAction = ait->pSeatAction;
			if (CVehicleSeatActionWeapons* weapAction = CAST_VEHICLEOBJECT(CVehicleSeatActionWeapons, pSeatAction))
			{
				assert(iaction < params.seats[s].seatactions.size());
				int nweapons = (int)params.seats[s].seatactions[iaction].weapons.size();

				for (int w = 0;w < nweapons;w++)
					weapAction->SetWeaponEntityId(w, params.seats[s].seatactions[iaction].weapons[w]);

				++iaction;
			}
		}
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CVehicle, ClSetAmmo)
{
	IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(params.ammo.c_str());
	assert(pClass);
	SetAmmoCount(pClass, params.count);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CVehicle, ClAmmoCounts)
{
	m_pInventory->ResetAmmo();

	for (const AmmoCountsParams::Ammo& ammo : params.ammos)
	{
		IEntityClass* pAmmoClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(ammo.name.c_str());
		if (pAmmoClass)
		{
			this->SetAmmoCount(pAmmoClass, ammo.count);
		}
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CVehicle, ClAbandonWarning)
{
	EnableAbandonedWarnSound(params.enable);

	return true;
}

//------------------------------------------------------------------------
bool CVehicle::GetExitPositionForActor(IActor* pActor, Vec3& pos, bool extended/*=false*/)
{
	Vec3 outPos = ZERO;
	Matrix34 worldTM;

	if (!pActor)
		return false;

	// loop through seats in order, check each one's exit position
	for (int i = 0; i < m_pVehicle->GetSeatCount(); ++i)
	{
		CVehicleSeat* pSeat = (CVehicleSeat*)GetSeatById(i + 1);
		// skip testing the actor's current seat (this will have been tried already)
		if (pSeat && pSeat->GetPassenger() != pActor->GetEntityId())
		{
			worldTM = pSeat->GetExitTM(pActor, false);
			outPos = worldTM.GetTranslation();
			EntityId blockerId = 0;
			if (pSeat->TestExitPosition(pActor, outPos, &blockerId))
			{
				pos = outPos;
				return true;
			}

			if (extended)
			{
				Vec3 startPos = pSeat->GetSitHelper()->GetWorldTM().GetTranslation();
				Vec3 endPos = outPos;
				if (DoExtendedExitTest(startPos, endPos, blockerId, outPos))
				{
					pos = outPos;
					return true;
				}
			}
		}
	}

	// if we still can't exit, try the seat helper positions reflected to the opposite side of the vehicle
	// (helps for tanks, which have two exits on the same side but not one on the other)
	for (int i = 0; i < m_pVehicle->GetSeatCount(); ++i)
	{
		CVehicleSeat* pSeat = (CVehicleSeat*)GetSeatById(i + 1);
		if (pSeat)
		{
			worldTM = pSeat->GetExitTM(pActor, true);
			outPos = worldTM.GetTranslation();
			EntityId blockerId = 0;
			if (pSeat->TestExitPosition(pActor, outPos, &blockerId))
			{
				pos = outPos;
				return true;
			}

			if (extended)
			{
				Vec3 startPos = pSeat->GetSitHelper()->GetWorldTM().GetTranslation();
				Vec3 endPos = outPos;
				if (DoExtendedExitTest(startPos, endPos, blockerId, outPos))
				{
					pos = outPos;
					return true;
				}
			}
		}
	}

	// and if we *still* can't find a valid place to get out, try in front and behind the vehicle
	//	(depending on speed ;)
	Vec3 startPos = GetEntity()->GetWorldPos();

	CVehicleSeat* pSeat = (CVehicleSeat*)GetSeatById(1);

	if (pSeat)
	{
		AABB bbox;
		GetEntity()->GetLocalBounds(bbox);
		float length = bbox.GetRadius();
		Quat rot = GetEntity()->GetWorldRotation();

		Vec3 vel = GetStatus().vel;

		Vec3 fw = rot * Vec3(0, 1, 0);
		if (vel.Dot(fw) < 0.1f)
		{
			// get vehicle bounds and put the player about half a metre in front of the centre.
			outPos = startPos + (length * fw);
			EntityId blockerId = 0;
			if (pSeat->TestExitPosition(pActor, outPos, &blockerId))
			{
				pos = outPos;
				return true;
			}

			if (extended)
			{
				Vec3 startPosEx = pSeat->GetSitHelper()->GetWorldTM().GetTranslation();
				Vec3 endPos = outPos;
				if (DoExtendedExitTest(startPosEx, endPos, blockerId, outPos))
				{
					pos = outPos;
					return true;
				}
			}
		}

		if (vel.Dot(fw) > -0.1f)
		{
			// try behind the vehicle.
			outPos = startPos + (-length * fw);
			EntityId blockerId = 0;
			if (pSeat->TestExitPosition(pActor, outPos, &blockerId))
			{
				pos = outPos;
				return true;
			}

			if (extended)
			{
				Vec3 startPosEx = pSeat->GetSitHelper()->GetWorldTM().GetTranslation();
				Vec3 endPos = outPos;
				if (DoExtendedExitTest(startPosEx, endPos, blockerId, outPos))
				{
					pos = outPos;
					return true;
				}
			}
		}
	}

	// last chance - put the player directly on top?

	return false;
}

void CVehicle::ExitVehicleAtPosition(EntityId passengerId, const Vec3& pos)
{
	if (gEnv->bServer)
	{
		RequestLeaveParams params(passengerId);
		IVehicleSeat* pCurrentSeat = GetSeatForPassenger(params.actorId);
		if (pCurrentSeat)
		{
			pCurrentSeat->Exit(true, false);
			//GetGameObject()->InvokeRMI(ClRequestLeave(), params, eRMI_ToAllClients); //CryMP
		}
	}
	else
		GetGameObject()->InvokeRMI(SvRequestLeave(), RequestLeaveParams(passengerId), eRMI_ToServer);
}

void CVehicle::EvictAllPassengers()
{
	// Does this need an RMI?
	for (TVehicleSeatVector::iterator it = m_seats.begin(), end = m_seats.end(); it != end; ++it)
	{
		if (it->second->GetPassenger())
		{
			OnAction(eVAI_Exit, eAAM_OnPress, 0.f, it->second->GetPassenger());
			//it->second->Exit(false, true);
		}
	}
}

void CVehicle::KillPassengersInExposedSeats(bool includePlayers)
{
	for (TVehicleSeatVector::iterator seatIter = m_seats.begin(), end = m_seats.end(); seatIter != end; ++seatIter)
	{
		IVehicleSeat* pSeat = seatIter->second;
		if (pSeat && pSeat->IsPassengerExposed() && pSeat->GetPassenger())
		{
			IActor* pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pSeat->GetPassenger());
			if (pActor && (!pActor->IsPlayer() || includePlayers))
			{
				if (pActor->GetHealth() > 0)
				{
					if (IGameRules* pGameRules = gEnv->pGame->GetIGameFramework()->GetIGameRulesSystem()->GetCurrentGameRules())
					{
						HitInfo hit;

						hit.targetId = pActor->GetEntityId();
						hit.shooterId = GetEntityId();
						hit.weaponId = GetEntityId();
						hit.damage = 1000.f;
						hit.type = 0;
						hit.pos = pActor->GetEntity()->GetWorldPos();

						pGameRules->ServerHit(hit);
					}
				}
			}
		}
	}
}

int CVehicle::GetSkipEntities(IPhysicalEntity** pSkipEnts, int nMaxSkip)
{
	pSkipEnts[0] = m_pVehicle->GetEntity()->GetPhysics();
	int nskip = 0;

	for (int i = 0, count = m_pVehicle->GetEntity()->GetChildCount(); i < count && nskip < nMaxSkip - 1; ++i)
	{
		IEntity* pChild = m_pVehicle->GetEntity()->GetChild(i);
		IPhysicalEntity* pPhysics = pChild->GetPhysics();
		if (pPhysics)
		{
			// todo: shorter way to handle this?
			if (pPhysics->GetType() == PE_LIVING)
				if (ICharacterInstance* pCharacter = pChild->GetCharacter(0))
					if (IPhysicalEntity* pCharPhys = pCharacter->GetISkeletonPose()->GetCharacterPhysics())
						pPhysics = pCharPhys;

			pSkipEnts[++nskip] = pPhysics;
		}
	}

	return nskip + 1;
}

bool CVehicle::ExitSphereTest(IPhysicalEntity** pSkipEnts, int nskip, Vec3 startPos, Vec3 testPos, EntityId* pBlockingEntity)
{
	static float r = 0.5f;
	primitives::sphere sphere;
	sphere.center = startPos;
	sphere.r = r;

	Vec3 endPos = testPos;
	endPos.z += r + 0.3f;

	geom_contact* pContact = 0;
	float hitDist = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(sphere.type, &sphere, endPos - sphere.center, ent_static | ent_terrain | ent_rigid | ent_sleeping_rigid | ent_living,
		&pContact, 0, (geom_colltype_player << rwi_colltype_bit) | rwi_stop_at_pierceable, 0, 0, 0, pSkipEnts, nskip);

	/* //CryMP: fixme
	if (VehicleCVars().v_debugdraw > 0)
	{
		CPersistantDebug* pPD = gEnv->pGame->GetIGameFramework()->GetPersistantDebug();
		pPD->Begin("VehicleSeat", false);

		ColorF col(1.0f, 1.0f, 1.0f, 1.0f);
		if (pContact && hitDist > 0.001f)
			col = ColorF(1.0f, 0.0f, 0.0f, 1.0f);
		pPD->AddLine(startPos, testPos, col, 30.0f);
		if (pContact && hitDist > 0.0f)
		{
			//pPD->AddSphere(rayhit.pt, 0.2f, col, 10.0f);
			pPD->AddSphere(pContact->pt, 0.1f, col, 30.0f);
		}
	}*/

	if (hitDist > 0.001f)
	{
		assert(pContact);
		if (pBlockingEntity)
			*pBlockingEntity = pContact->iPrim[0];	//?
		//CryLog("Pos failed: %.2f, %.2f, %.2f", endPos.x, endPos.y, endPos.z);
		return false;
	}
	else if (pContact)
	{
		//CryLog("Pos succeeded: %.2f, %.2f, %.2f", endPos.x, endPos.y, endPos.z);
		return true;
	}

	return false;
}

bool CVehicle::DoExtendedExitTest(Vec3 seatPos, Vec3 firstTestPos, EntityId blockerId, Vec3& outPos)
{
	//CryLog("Starting extended vehicle exit check");
	static int numChecks = 4;

	Vec3 startPos = seatPos;
	Vec3 endPos = firstTestPos;

	// if the test hit an actor...
	// try a further check 1.5m further in the same direction.
	static const int maxSkip = 15;
	IPhysicalEntity* pSkipEnts[maxSkip];
	int numSkip = GetSkipEntities(pSkipEnts, maxSkip - numChecks);

	// each check will proceed further from the vehicle, adding the previous contact to the skip entities.
	//	Hitting another actor will update blockerId for the next check.
	for (int check = 0; check < numChecks; ++check)
	{
		IActor* pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(blockerId);
		if (!pActor)
			break;

		// skip previous contact
		pSkipEnts[numSkip++] = pActor->GetEntity()->GetPhysics();

		// additional swept sphere test from the previous position to the new one...
		Vec3 prevDir = endPos - startPos;
		prevDir.z = 0.0f;
		startPos.x = endPos.x;
		startPos.y = endPos.y;
		endPos = startPos + 1.5f * prevDir.GetNormalizedSafe();
		if (ExitSphereTest(pSkipEnts, numSkip, startPos, endPos, &blockerId))
		{
			outPos = endPos;
			return true;
		}
	}

	return false;
}

const char* CVehicle::GetModification() const
{
	return m_modification.c_str();
}

void CVehicle::DebugFlipOver()
{
	IEntity* pEntity = GetEntity();
	Matrix34 tm = GetEntity()->GetWorldTM();
	const Vec3 zAxis = tm.GetColumn2();
	if (zAxis.z > 0.f)
	{
		AABB bbox;
		pEntity->GetWorldBounds(bbox);
		const Vec3 pos = tm.GetColumn3();
		const Vec3 centre = (bbox.min + bbox.max) * 0.5f;
		const Vec3 offset = centre - pos;
		const Vec3 xAxis = tm.GetColumn0();
		tm.SetColumn(0, -xAxis);
		tm.SetColumn(2, -zAxis);
		tm.SetColumn(3, pos + 2.f * offset);
		pEntity->SetWorldTM(tm);
		NeedsUpdate(eVUF_AwakePhysics);
	}
}

/*
#include UNIQUE_VIRTUAL_WRAPPER(IVehicleAnimation)
#include UNIQUE_VIRTUAL_WRAPPER(IVehicleSystem)
#include UNIQUE_VIRTUAL_WRAPPER(SEnvironmentParticles)
#include UNIQUE_VIRTUAL_WRAPPER(IVehicleComponent)
*/