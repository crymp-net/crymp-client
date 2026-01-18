/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a part for vehicles which uses animated characters

-------------------------------------------------------------------------
History:
- 24:08:2005: Created by Mathieu Pinard

*************************************************************************/
#include "CryCommon/CrySystem/ISystem.h"

#include "CryCommon/CryAnimation/ICryAnimation.h"
#include "CryCommon/CryAction/IVehicleSystem.h"
#include "CryCommon/CryRenderer/IRenderAuxGeom.h"

//#include "CryAction.h"
#include "Vehicle.h"
#include "VehicleComponent.h"
#include "VehiclePartSubPartWheel.h"
#include "VehiclePartTread.h"

int CVehiclePartTread_PostProcess(ICharacterInstance* pCharInstance, CVehiclePartTread* pTread);

//------------------------------------------------------------------------
CVehiclePartTread::CVehiclePartTread()
{
	m_typeId = eVPT_Tread;

	m_pCharInstance = nullptr;
	m_slot = -1;
	m_lastWheelIndex = 0;
	m_pMaterial = 0;

	m_damageRatio = 0.f;
	m_pShaderResources = nullptr;
	m_pMaterial = nullptr;
}

//------------------------------------------------------------------------
bool CVehiclePartTread::Init(IVehicle* pVehicle, const CVehicleParams& table, IVehiclePart* parent, CVehicle::SPartInitInfo& initInfo)
{
	if (!CVehiclePartBase::Init(pVehicle, table, parent, initInfo))
		return false;

	CVehicleParams subTable = table.findChild("Tread"); // Tread subtable
	if (!subTable)
		return false;

	string filename = subTable.getAttr("filename");
	if (filename.empty())
		return false;

	m_uvSpeedMultiplier = 1.0f;

	if (table.haveAttr("component"))
	{
		if (CVehicleComponent* pComponent
			= static_cast<CVehicleComponent*>(m_pVehicle->GetComponent(table.getAttr("component"))))
		{
			pComponent->AddPart(this);
		}
	}

	m_slot = GetEntity()->LoadCharacter(m_slot, filename.c_str());

	m_pCharInstance = GetEntity()->GetCharacter(m_slot);
	if (!m_pCharInstance)
		return false;

	//CryMP
	m_pCharInstance->GetISkeletonPose()->SetPostProcessCallback0(
		(int (*)(ICharacterInstance*, void*)) CVehiclePartTread_PostProcess, (void*)this);

	if (subTable.haveAttr("materialName"))
	{
		string materialName = subTable.getAttr("materialName");
		materialName.MakeLower();

		IMaterial* pMaterial = 0;
		const char* subMtlName = 0;
		int subMtlSlot = -1;

		// find tread material
		IEntityRenderProxy* pRenderProxy = (IEntityRenderProxy*)GetEntity()->GetProxy(ENTITY_PROXY_RENDER);
		if (pRenderProxy)
		{
			pMaterial = pRenderProxy->GetRenderMaterial(m_slot);
			if (pMaterial)
			{
				// if matname doesn't fit, look in submaterials
				if (string(pMaterial->GetName()).MakeLower().find(materialName.c_str()) == string::npos)
				{
					for (int i = 0; i < pMaterial->GetSubMtlCount(); ++i)
					{
						if (string(pMaterial->GetSubMtl(i)->GetName()).MakeLower().find(materialName.c_str()) != string::npos)
						{
							subMtlName = pMaterial->GetSubMtl(i)->GetName();
							subMtlSlot = i;
							break;
						}
					}
				}
			}
		}

		if (pMaterial)
		{
			// clone      
			IMaterial* pCloned = pMaterial->GetMaterialManager()->CloneMultiMaterial(pMaterial, subMtlName);
			if (pCloned)
			{
				pRenderProxy->SetSlotMaterial(m_slot, pCloned);
				pMaterial = pCloned;
				m_pMaterial = pMaterial;
			}

			if (subMtlSlot > -1)
				m_pShaderResources = pMaterial->GetShaderItem(subMtlSlot).m_pShaderResources;
			else
				m_pShaderResources = pMaterial->GetShaderItem().m_pShaderResources;
		}

		m_texSlots.clear();
		m_texSlots.reserve(4);

		if (m_pShaderResources)
		{
			for (int i = 0; i < EFTT_MAX; ++i)
			{
				if (SEfResTexture* pTexture = m_pShaderResources->GetTexture(i))
				{
					//CryMP: Skip nameless slots
					const char* file = pTexture->m_Name.c_str();
					if (file && file[0] != '\0')
					{
						m_texSlots.push_back(i);

						SEfTexModificator& modif = m_pShaderResources->GetTexture(i)->m_TexModificator;
						modif.m_bDontUpdate = false;

						modif.SetMember("m_eUMoveType", 1.0f);
						modif.SetMember("m_UOscRate", 0.0f);
						modif.SetMember("m_UOscAmplitude", 1.0f);
					}
				}
			}
		}
	}

	char wheelName[256];

	ISkeletonPose* pSkeletonPose = m_pCharInstance->GetISkeletonPose();
	for (int i = 0; i < pSkeletonPose->GetJointCount(); ++i)
	{
		const char* boneName = pSkeletonPose->GetJointNameByID(i);
		if (0 != boneName)
		{
			// extract wheel name
			size_t len = strcspn(boneName, "_");
			if (len < strlen(boneName) && len < 255)
			{
				strncpy(wheelName, boneName, len);
				wheelName[len] = '\0';

				CVehiclePartSubPartWheel* pWheel = (CVehiclePartSubPartWheel*)m_pVehicle->GetPart(wheelName);
				if (pWheel)
				{
					SWheelInfo wheelInfo;
					wheelInfo.slot = pWheel->m_slot;
					wheelInfo.jointId = i;
					wheelInfo.pWheel = pWheel;
					m_wheels.push_back(wheelInfo);

					m_lastWheelIndex = pWheel->m_wheelIndex;
				}
			}
		}
	}

	m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_AlwaysUpdate);

	m_state = eVGS_Default;
	return true;
}

//------------------------------------------------------------------------
void CVehiclePartTread::Release()
{
	//CryMP
	if (m_pCharInstance)
	{
		ISkeletonPose* pSkeletonPose = m_pCharInstance->GetISkeletonPose();
		if (pSkeletonPose)
		{
			pSkeletonPose->SetPostProcessCallback0(nullptr, nullptr);
		}
	}

	CVehiclePartBase::Release();
}

//------------------------------------------------------------------------
void CVehiclePartTread::Reset()
{
	m_forceSetU = false;
	m_wantedU = 0.0f;
	m_currentU = m_wantedU + 1.0f;
	UpdateU();

	CVehiclePartBase::Reset();
}

//------------------------------------------------------------------------
void CVehiclePartTread::OnEvent(const SVehiclePartEvent& event)
{
	CVehiclePartBase::OnEvent(event);

	if (event.type == eVPE_Damaged && event.fparam >= 1.0f)
	{
		SetDamageRatio(event.fparam);
	}
	else if (event.type == eVPE_Repair && event.fparam <= 0.25f)
	{
		SetDamageRatio(0.0f);
	}
}

//------------------------------------------------------------------------
bool CVehiclePartTread::ChangeState(EVehiclePartState state, int flags/* =0 */)
{
	EVehiclePartState old = m_state;
	bool bOK = CVehiclePartBase::ChangeState(state, flags);
	m_state = state;

	if (bOK && m_pVehicle->GetMovement())
	{
		if (m_state > old && m_state == IVehiclePart::eVGS_Damaged1)
		{
			SVehicleMovementEventParams params;
			m_pVehicle->GetMovement()->OnEvent(IVehicleMovement::eVME_TireBlown, params);
		}
		else if (m_state < old && m_state == eVGS_Default)
		{
			SVehicleMovementEventParams params;
			m_pVehicle->GetMovement()->OnEvent(IVehicleMovement::eVME_TireRestored, params);
		}
	}

	return bOK;
}

//------------------------------------------------------------------------
void CVehiclePartTread::SetDamageRatio(float value)
{
	if (value >= 1.0f)
	{
		UpdateU();
		Hide(true);
	}
	else if (value == 0.0f)
	{
		Hide(false);
	}

	m_damageRatio = value;
}

//------------------------------------------------------------------------
void CVehiclePartTread::InitGeometry()
{
}

//------------------------------------------------------------------------
void CVehiclePartTread::Physicalize()
{
	Update(1.0f);
}

//------------------------------------------------------------------------
const Matrix34& CVehiclePartTread::GetLocalTM(bool relativeToParentPart)
{
	return GetEntity()->GetSlotLocalTM(m_slot, relativeToParentPart);
}

//------------------------------------------------------------------------
const Matrix34& CVehiclePartTread::GetWorldTM()
{
	return GetEntity()->GetSlotWorldTM(m_slot);
}

//------------------------------------------------------------------------
const AABB& CVehiclePartTread::GetLocalBounds()
{
	if (m_pCharInstance)
	{
		m_bounds = m_pCharInstance->GetAABB();
		m_bounds.SetTransformedAABB(GetEntity()->GetSlotLocalTM(m_slot, false), m_bounds);
	}
	else
		GetEntity()->GetLocalBounds(m_bounds);

	return m_bounds;
}

//------------------------------------------------------------------------
void CVehiclePartTread::Update(const float frameTime)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	if (!m_pCharInstance)
		return;

	if (m_forceSetU)
	{
		m_forceSetU = false;
		m_currentU = m_wantedU + 1.0f;
		UpdateU();
	}

	ISkeletonPose* pSkeletonPose = m_pCharInstance->GetISkeletonPose();

	pSkeletonPose->SetForceSkeletonUpdate(0);

	if (m_pVehicle->GetStatus().speed < 0.001f)
	{
		return;
	}

	if (frameTime > 0.f &&
		(m_damageRatio >= 1.f || !m_pVehicle->GetGameObject()->IsProbablyVisible()))
	{
		return;
	}

	// we need a tread update in next frame
	pSkeletonPose->SetForceSkeletonUpdate(1);
	m_pVehicle->NeedsUpdate();

	// animate the UV texture according to the wheels speed
	if (m_uvSpeedMultiplier != 0.0f && frameTime > 0.0f)
	{
		IPhysicalEntity* pPhysics = GetEntity()->GetPhysics();
		pe_status_wheel wheelStatus;
		wheelStatus.iWheel = m_lastWheelIndex;

		if (pPhysics && pPhysics->GetStatus(&wheelStatus) != 0)
		{
			m_wantedU += m_uvSpeedMultiplier * (wheelStatus.w * wheelStatus.r * frameTime);
			m_wantedU -= std::floor(m_wantedU);
			UpdateU();
		}
	}
}

//------------------------------------------------------------------------
int CVehiclePartTread_PostProcess(ICharacterInstance* pCharInstance, CVehiclePartTread* pTread)
{
	pTread->SkeletonPostProcess(pCharInstance);
	return 0;
}

//------------------------------------------------------------------------
void CVehiclePartTread::SkeletonPostProcess(ICharacterInstance* pCharInstance)
{
    if (!pCharInstance)
        return;

    // Deform the tread to follow the wheels
    ISkeletonPose* pSkeletonPose = pCharInstance->GetISkeletonPose();
    if (!pSkeletonPose)
        return;

    const QuatT absRoot = pSkeletonPose->GetAbsJointByID(0);

    const Matrix34 worldTM = GetEntity()->GetWorldTM();
    const Matrix34 invWorldTM = worldTM.GetInverted();

    for (const auto& wheelInfo : m_wheels)
    {
        const auto slotTM = GetEntity()->GetSlotLocalTM(wheelInfo.slot, true);
        
        QuatT relQuat = pSkeletonPose->GetRelJointByID(wheelInfo.jointId);
        relQuat.t = absRoot.GetInverted() * slotTM.GetTranslation();
        pSkeletonPose->SetPostProcessQuat(wheelInfo.jointId, relQuat);

        if (VehicleCVars().v_debugdraw == 4)
        {
            const Vec3 local = invWorldTM * slotTM.GetTranslation();
            gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(
                worldTM * (local + Vec3(static_cast<float>(sgn(local.x)) * 0.5f, 0.f, 0.f)), 
                0.1f, 
                ColorB(0, 0, 255, 255));
        }
    }
}


//------------------------------------------------------------------------
void CVehiclePartTread::UpdateU()
{
	if (!m_pShaderResources || m_currentU == m_wantedU)
		return;

	m_currentU = m_wantedU;
	const float u = -m_wantedU;

	for (int idx : m_texSlots)
	{
		if (SEfResTexture* pTexture = m_pShaderResources->GetTexture(idx))
		{
			pTexture->m_TexModificator.SetMember("m_UOscRate", u);

			//CryLogAlways("%s (%d) pTexture -> %s (frame updated %d, m_bDontUpdate %d)", GetName().c_str(), idx, pTexture->m_Name.c_str(), pTexture->m_TexModificator.m_nFrameUpdated, pTexture->m_TexModificator.m_bDontUpdate);
		}
	}
}

//------------------------------------------------------------------------
void CVehiclePartTread::Serialize(TSerialize ser, unsigned aspects)
{
	const float damageRatio = m_damageRatio;

	CVehiclePartBase::Serialize(ser, aspects);

	if (ser.GetSerializationTarget() != eST_Network)
	{
		if (ser.IsReading())
		{
			if (damageRatio != m_damageRatio)
				SetDamageRatio(m_damageRatio);
		}
	}
}

void CVehiclePartTread::GetMemoryStatistics(ICrySizer* s)
{
	s->Add(*this);
	GetBaseMemoryStatistics(s);
	s->AddContainer(m_wheels);
}

DEFINE_VEHICLEOBJECT(CVehiclePartTread);
