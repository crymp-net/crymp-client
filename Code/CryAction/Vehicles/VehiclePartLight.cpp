/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a light part

-------------------------------------------------------------------------
History:
- 24:10:2005: Created by Mathieu Pinard

*************************************************************************/
#include "CryCommon/CrySystem/ISystem.h"

#include "CryCommon/CryAnimation/ICryAnimation.h"
#include "CryCommon/CryAction/IVehicleSystem.h"
#include "CryCommon/CryAction/IActorSystem.h"

//#include "CryAction.h"
#include "Vehicle.h"
#include "VehicleComponent.h"
#include "VehiclePartBase.h"
#include "VehiclePartLight.h"


#define DEFAULT_FLOAT -1.f
#define DEFAULT_INT -1
#define DEFAULT_VEC3 Vec3()

//------------------------------------------------------------------------
CVehiclePartLight::CVehiclePartLight()
{
	m_typeId = eVPT_Light;
	m_pMaterial = 0;
	m_pHelper = 0;

	m_diffuseMult[0] = m_diffuseMult[1] = 1.f;
	m_diffuseCol.Set(1, 1, 1);
	m_enabled = false;
}

//------------------------------------------------------------------------
CVehiclePartLight::~CVehiclePartLight()
{
	ToggleLight(false);
}

//------------------------------------------------------------------------
bool CVehiclePartLight::Init(IVehicle* pVehicle, const CVehicleParams& table, IVehiclePart* parent, CVehicle::SPartInitInfo& initInfo)
{
	if (!CVehiclePartBase::Init(pVehicle, table, parent, initInfo))
		return false;

	Vec3 specularCol(1.f, 1.f, 1.f);
	float specularMul = 1.0f;

	const char* texture = "";

	m_light.m_nLightStyle = 0;
	m_light.m_Origin.Set(0.0f, 0.0f, 0.0f);
	m_light.m_fRadius = 5.0f;
	m_light.m_fCoronaScale = 1.0f;
	m_light.m_fCoronaDistIntensityFactor = 1.0f;
	m_light.m_fCoronaDistSizeFactor = 1.0f;
	m_light.m_NumCM = -1;
	m_light.m_AreaLightType = DLAT_POINT;
	m_light.m_nAreaSampleNumber = 1;
	m_light.m_nEntityId = m_pVehicle->GetEntityId();

	m_light.m_Flags |= DLF_LIGHTSOURCE;
	//m_light.m_Flags |= DLF_DEFERRED_LIGHT;
  //m_light.m_Flags |= DLF_THIS_AREA_ONLY;
	m_light.m_Flags &= ~DLF_DISABLED;

	XmlNodeRef defaultDataRef;

	if (CVehicleParams lightTable = table.findChild("Light"))
	{
		const char* pType = lightTable.getAttr("type");
		m_lightType = pType;

		// open the default lights	
		defaultDataRef = gEnv->pSystem->LoadXmlFile("CryMP/Xml/Vehicles/Lights/DefaultVehicleLights.xml");
		XmlNodeRef lightDefault;
		if (defaultDataRef)
		{
			if (XmlNodeRef lightDefaultsRef = defaultDataRef->findChild("Lights"))
			{
				int i = 0;
				int c = lightDefaultsRef->getChildCount();

				for (; i < c; i++)
				{
					if (XmlNodeRef tableRef = lightDefaultsRef->getChild(i))
					{
						const char* pValue = tableRef->getAttr("type");
						if (!strcmp(pValue, pType))
						{
							lightDefault = tableRef;
							break;
						}
					}
				}
			}
		}

#define GET_LIGHT_VALUE_FLOAT(name, var) \
	if (lightDefault) lightDefault->getAttr(name, var); \
	lightTable.getAttr(name, var); \

#define GET_LIGHT_VALUE_DEF_FLOAT(name, var) { float val(DEFAULT_FLOAT); \
	GET_LIGHT_VALUE_FLOAT(name, val); if (val != DEFAULT_FLOAT) var = val; }

#define GET_LIGHT_VALUE_INT(name, var) \
	if (lightDefault) lightDefault->getAttr(name, var); \
	lightTable.getAttr(name, var); \

#define GET_LIGHT_VALUE_DEF_INT(name, var) { int val(DEFAULT_INT); \
	GET_LIGHT_VALUE_INT(name, val); if (val != DEFAULT_INT) var = val; }

#define GET_LIGHT_VALUE_VEC3(name, var) \
	if (lightDefault) lightDefault->getAttr(name, var); \
	lightTable.getAttr(name, var); \


#define GET_LIGHT_VALUE_DEF_VEC3(name, var) \
{ \
	Vec3 val; \
	GET_LIGHT_VALUE_VEC3(name, val); \
	if (val != DEFAULT_VEC3) \
		var = val; \
}

#define GET_LIGHT_VALUE_STRING(name, var) \
	if (lightDefault) {var = lightDefault->getAttr(name);} else \
		{ var = lightTable.getAttr(name); } \

#define GET_LIGHT_VALUE_BOOL(name, var) \
	if (lightDefault) lightDefault->getAttr(name, var); else \
	lightTable.getAttr(name, var); \

		// read table and overwrite non-default values
		const char* tex = "";
		GET_LIGHT_VALUE_STRING("texture", tex);
		if (tex[0]) texture = tex;

		const char* mat = "";
		GET_LIGHT_VALUE_STRING("material", mat);
		if (mat[0]) m_pMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(mat);

		GET_LIGHT_VALUE_DEF_FLOAT("radius", m_light.m_fRadius);
		GET_LIGHT_VALUE_DEF_VEC3("diffuse", m_diffuseCol);
		GET_LIGHT_VALUE_DEF_FLOAT("diffuseMult", m_diffuseMult[1]);
		m_diffuseMult[0] = m_diffuseMult[1];
		GET_LIGHT_VALUE_DEF_FLOAT("diffuseMult_fp", m_diffuseMult[0]);
		GET_LIGHT_VALUE_DEF_VEC3("specular", specularCol);
		GET_LIGHT_VALUE_DEF_FLOAT("specularMult", specularMul);
		GET_LIGHT_VALUE_DEF_FLOAT("coronaDistIntensity", m_light.m_fCoronaDistIntensityFactor);
		GET_LIGHT_VALUE_DEF_FLOAT("coronaDistSize", m_light.m_fCoronaDistSizeFactor);
		GET_LIGHT_VALUE_DEF_FLOAT("coronaScale", m_light.m_fCoronaScale);
		GET_LIGHT_VALUE_DEF_INT("style", m_light.m_nLightStyle);
		GET_LIGHT_VALUE_DEF_FLOAT("frustumAngle", m_light.m_fLightFrustumAngle);

		//CryMP: Fakelight support
		bool fakeLight = false;
		GET_LIGHT_VALUE_BOOL("fakeLight", fakeLight);

		if (fakeLight)
		{
			m_light.m_Flags |= DLF_FAKE;
		}
	}

	m_light.SetLightColor(ColorF(m_diffuseCol * m_diffuseMult[1], 1.f));
	m_light.m_SpecMult = specularMul / m_diffuseMult[1];

	if (texture[0])
	{
		const int flags = FT_FORCE_CUBEMAP; //CryMP
		m_light.m_pLightImage = gEnv->pRenderer->EF_LoadTexture(texture, flags, 0);
	}

	if (m_light.m_fLightFrustumAngle && m_light.m_pLightImage && m_light.m_pLightImage->IsTextureLoaded())
	{
		m_light.m_Flags |= DLF_PROJECT;
	}
	else
	{
		if (m_light.m_pLightImage)
		{
			m_light.m_pLightImage->Release();
			m_light.m_pLightImage = 0;
		}
		m_light.m_Flags |= DLF_POINT;
	}

	m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_Visible);
	return true;
}


//------------------------------------------------------------------------
void CVehiclePartLight::PostInit()
{
	m_pHelper = m_pVehicle->GetHelper(m_helperPosName.c_str());

	// get Components this Part belongs to. 
	// currently that's only needed for Lights. 
	for (int i = 0, nComps = m_pVehicle->GetComponentCount(); i < nComps; ++i)
	{
		IVehicleComponent* pComponent = m_pVehicle->GetComponent(i);

		for (int j = 0, nParts = pComponent->GetPartCount(); j < nParts; ++j)
		{
			if (pComponent->GetPart(j) == this)
			{
				m_components.push_back(pComponent);
				break;
			}
		}
	}

	if (VehicleCVars().v_lights_enable_always)
		ToggleLight(true);
}

//------------------------------------------------------------------------
void CVehiclePartLight::Reset()
{
	CVehiclePartBase::Reset();

	ToggleLight(false);
}


//------------------------------------------------------------------------
void CVehiclePartLight::OnEvent(const SVehiclePartEvent& event)
{
	if (event.type == eVPE_Damaged)
	{
		if (event.fparam >= 1.0f)
		{
			float dmg = 1.f / max(1.f, (float)m_components.size());
			m_damageRatio += dmg;

			if (m_damageRatio >= 1.f && IsEnabled())
				ToggleLight(false);
		}
	}
	else
	{
		CVehiclePartBase::OnEvent(event);
	}
}

//------------------------------------------------------------------------
void CVehiclePartLight::ToggleLight(bool enable)
{
	if (enable && !IsEnabled())
	{
		// 0: no lights at all (only intended for debugging etc.)
		// 1: lights only enabled for the local player
		// 2: all lights enabled
		if (VehicleCVars().v_lights == 0
			|| (VehicleCVars().v_lights == 1 && !m_pVehicle->IsPlayerPassenger()))
			return;

		if (m_pHelper && m_damageRatio < 1.0f)
		{
			m_slot = GetEntity()->LoadLight(m_slot, &m_light);

			if (m_slot != -1 && m_pMaterial)
				GetEntity()->SetSlotMaterial(m_slot, m_pMaterial);

			UpdateLight(0.f);
		}

		m_enabled = true;
	}
	else if (!enable)
	{
		if (m_slot != -1)
		{
			GetEntity()->FreeSlot(m_slot);
			m_slot = -1;
		}

		m_enabled = false;
	}

}

//-----------------------------------------------------------------------
void CVehiclePartLight::Update(const float frameTime)
{
	if (IsEnabled())
		UpdateLight(frameTime);
}

//-----------------------------------------------------------------------
void CVehiclePartLight::UpdateLight(const float frameTime)
{
	if (m_slot == -1)
		return;

	// move to vehicle event change view?
	if (m_diffuseMult[0] != m_diffuseMult[1])
	{
		SEntitySlotInfo info;
		if (m_pVehicle->GetEntity()->GetSlotInfo(m_slot, info) && info.pLight)
		{
			CDLight& light = static_cast<ILightSource*>(info.pLight)->GetLightProperties();

			IActor* pActor = gEnv->pGame->GetIGameFramework()->GetClientActor();
			bool localPlayer = (pActor != NULL) && (pActor->GetLinkedVehicle() == m_pVehicle);

			IVehicleSeat* pSeat = pActor ? m_pVehicle->GetSeatForPassenger(pActor->GetEntityId()) : NULL;
			IVehicleView* pView = pSeat ? pSeat->GetView(pSeat->GetCurrentView()) : NULL;
			bool isThirdPersonView = pView ? pView->IsThirdPerson() : true;
			if (localPlayer && !isThirdPersonView)
				light.SetLightColor(ColorF(m_diffuseCol * m_diffuseMult[0], 1.f));
			else
				light.SetLightColor(ColorF(m_diffuseCol * m_diffuseMult[1], 1.f));
		}
	}

	if (m_pHelper)
	{
		const static Matrix33 rot(Matrix33::CreateRotationXYZ(Ang3(0.f, 0.f, DEG2RAD(90.f))));

		const Matrix34& helperTM = m_pHelper->GetVehicleTM();
		Matrix34 localTM = Matrix33(helperTM) * rot;
		localTM.SetTranslation(helperTM.GetTranslation());

		GetEntity()->SetSlotLocalTM(m_slot, localTM);
	}

}

//------------------------------------------------------------------------
void CVehiclePartLight::Serialize(TSerialize ser, unsigned aspects)
{
	CVehiclePartBase::Serialize(ser, aspects);

	if (ser.GetSerializationTarget() != eST_Network)
	{
		bool isEnabled = IsEnabled();
		ser.Value("lightEnabled", isEnabled, 'bool');

		if (ser.IsReading())
		{
			ToggleLight(isEnabled);
		}
	}
}

DEFINE_VEHICLEOBJECT(CVehiclePartLight);
