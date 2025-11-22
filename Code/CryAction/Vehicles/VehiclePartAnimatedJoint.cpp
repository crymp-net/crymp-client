/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a part for vehicles which is the an attachment 
of a parent Animated part

-------------------------------------------------------------------------
History:
- 25:08:2005: Created by Mathieu Pinard

*************************************************************************/
#include "CryCommon/CrySystem/ISystem.h"

#include "CryCommon/CryAnimation/ICryAnimation.h"
#include "CryCommon/CryAction/IVehicleSystem.h"
#include "CryCommon/CryAction/IGameTokens.h"

//#include "CryAction.h"
#include "Vehicle.h"
#include "VehiclePartBase.h"
#include "VehiclePartAnimated.h"
#include "VehiclePartAnimatedJoint.h"

int CVehiclePartAnimatedJoint_UpdateCharacter(ICharacterInstance* pCharInstance, int* jointId);

//------------------------------------------------------------------------
CVehiclePartAnimatedJoint::CVehiclePartAnimatedJoint()
{
  m_typeId = eVPT_AnimatedJoint;

  m_pCharInstance = NULL;
  m_pAnimatedPart = NULL;
  m_jointId = -1;
  m_localTMInvalid = false;
  m_isMoveable = false;
  m_dialsRotMax = 0.f;
  m_initialRotOfs = 0.f;
  m_detachBaseForce.zero();
  m_detachProbability = 0.f;
  
  m_pGeometry = 0;
  m_pDestroyedGeometry = 0;
  m_pMaterial = 0;
    
  m_baseTM.SetIdentity();
  m_initialTM.SetIdentity();
  m_localTM.SetIdentity();  
  m_worldTM.SetIdentity();
}

//------------------------------------------------------------------------
CVehiclePartAnimatedJoint::~CVehiclePartAnimatedJoint()
{ 
}

//------------------------------------------------------------------------
bool CVehiclePartAnimatedJoint::Init(IVehicle* pVehicle, const CVehicleParams& table, IVehiclePart* parent, CVehicle::SPartInitInfo& initInfo)
{
	if (!CVehiclePartBase::Init(pVehicle, table, parent, initInfo))
	return false;

	CVehicleParams animatedJointTable = table.findChild("AnimatedJoint");
	
	if (animatedJointTable)
	{
		animatedJointTable.getAttr("detachBaseForce", m_detachBaseForce);
		animatedJointTable.getAttr("detachProbability", m_detachProbability);

		CVehicleParams dialsTable = animatedJointTable.findChild("Dials");
		if (dialsTable)
		{
			if (dialsTable.getAttr("rotationMax", m_dialsRotMax))
				m_dialsRotMax = DEG2RAD(m_dialsRotMax);
			if (dialsTable.getAttr("ofs", m_initialRotOfs))
				m_initialRotOfs = DEG2RAD(m_initialRotOfs);
		}
	}
  
  InitGeometry(table);

  m_state = eVGS_Default;
  
  return true;
}

//------------------------------------------------------------------------
void CVehiclePartAnimatedJoint::PostInit()
{
	CVehiclePartBase::PostInit();

	if(m_pGeometry)
		SetMaterial(m_pMaterial);
}

//------------------------------------------------------------------------
void CVehiclePartAnimatedJoint::Reset()
{
	CVehiclePartBase::Reset();

	if(m_pGeometry)
		SetMaterial(m_pMaterial);
}

//------------------------------------------------------------------------
void CVehiclePartAnimatedJoint::ResetLocalTM(bool recursive)
{
  CVehiclePartBase::ResetLocalTM(recursive);

  if (m_jointId > -1)
  {
	    SetLocalBaseTM(m_initialTM);
  }
}

//------------------------------------------------------------------------
void CVehiclePartAnimatedJoint::InitGeometry(const CVehicleParams& table)
{
  m_pAnimatedPart = CAST_VEHICLEOBJECT(CVehiclePartAnimated, GetParent(true));		
	
  if (m_pAnimatedPart)
	{		
    if (m_pCharInstance = m_pAnimatedPart->GetEntity()->GetCharacter(m_pAnimatedPart->GetSlot()))
		{
			ISkeletonPose* pSkeletonPose = m_pCharInstance->GetISkeletonPose();

			m_slot = m_pAnimatedPart->GetSlot();
			m_jointId = pSkeletonPose->GetJointIDByName(m_name.c_str());

      if (m_useOption >= 0 && m_useOption <= MAX_OPTIONAL_PARTS)          
      { 
        char jointName[128];                        
        if (m_useOption > 0)
        {  
          // store actual used name
          _snprintf(jointName, 128, "%s_option_%i", m_name.c_str(), m_useOption);
          jointName[sizeof(jointName)-1] = '\0';
          
          int jointId = pSkeletonPose->GetJointIDByName(jointName);
          if (jointId != -1)
            m_jointId = jointId;
        }
                
        // remove unused statobjs
        for (int i=0; i<=MAX_OPTIONAL_PARTS; ++i)
        {
          if (i == m_useOption)
            continue;
          
          if (i == 0)
            _snprintf(jointName, 128, "%s", m_name.c_str());            
          else
            _snprintf(jointName, 128, "%s_option_%i", m_name.c_str(), i);
          
          jointName[sizeof(jointName)-1] = '\0';

          int jointId = pSkeletonPose->GetJointIDByName(jointName);
          if (jointId > -1)
          { 
            // remove all children this joint might have
            for (int n=0,count=pSkeletonPose->GetJointCount(); n<count; ++n)
            {
              if (n!=jointId && pSkeletonPose->GetParentIDByID(n)==jointId)
                GetEntity()->SetStatObj(NULL, GetCGASlot(n), true);
            }

            GetEntity()->SetStatObj(NULL, GetCGASlot(jointId), true);
          }
        }
      }

      if (m_jointId != -1)
      {
        CVehicleParams animatedJointTable = table.findChild("AnimatedJoint");
        if (animatedJointTable)
        {		
          string externalFile = animatedJointTable.getAttr("filename");
          if (!externalFile.empty())
          {
            if (m_pGeometry = gEnv->p3DEngine->LoadStatObj(externalFile.c_str()))
            { 
              SetStatObj(m_pGeometry);

							externalFile = animatedJointTable.getAttr("filenameDestroyed");
              if (!externalFile.empty())
                m_pDestroyedGeometry = gEnv->p3DEngine->LoadStatObj(externalFile.c_str());
            }
          }
        }
      }

			if (m_jointId > -1)
			{     
				Matrix34 tm = Matrix34(pSkeletonPose->GetRelJointByID(m_jointId));        
        
        SetLocalBaseTM( tm );
        m_initialTM = GetLocalTM(true);

				// NB SetMoveable may also be called by CSeatActionRotateTurret
				if (m_name.substr(0, 6) == "dials_")
        {
          SetMoveable();
          m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_PassengerUpdate);
        }
			}
		}
	}
}

//------------------------------------------------------------------------
void CVehiclePartAnimatedJoint::Physicalize()
{
  if (m_pCharInstance && m_jointId != -1)
  {
    ISkeletonPose* pSkeletonPose = m_pCharInstance->GetISkeletonPose();

    if (pSkeletonPose)
    {
      m_physId = pSkeletonPose->GetPhysIdOnJoint(m_jointId);
    }

    m_pVehicle->RequestPhysicalization(this, false);
  }  
}

//------------------------------------------------------------------------
void CVehiclePartAnimatedJoint::SetMoveable()
{ 
  if (!m_pCharInstance)
  {
    CryLogWarning("AnimatedJoint %s is missing CharInstance", m_name.c_str());
    return;
  }
      
	CVehiclePartAnimated* pPartAnimated = CAST_VEHICLEOBJECT(CVehiclePartAnimated, GetParent(true));
	
	if (pPartAnimated)
		pPartAnimated->RegisterRotatingAnimatedJoint(this);

  m_isMoveable = true;
}

//------------------------------------------------------------------------
bool CVehiclePartAnimatedJoint::ChangeState(EVehiclePartState state, int flags)
{
  if (!CVehiclePartBase::ChangeState(state, flags))
    return false;
  
  if (m_jointId != -1)
  {
    bool damaging  = (state > m_state && state > eVGS_Default && state < eVGS_Destroyed);
    bool repairing = (state < m_state && m_state > eVGS_Default && m_state < eVGS_Destroyed);
    if (damaging || repairing)
    {
      CVehiclePartAnimated* pParent = CAST_VEHICLEOBJECT(CVehiclePartAnimated, GetParent(true));
      if (pParent)
      {    
        pParent->ChangeChildState(this, state, flags);      
      }
    }    
    else if (m_pDestroyedGeometry)
    {
      if (state == eVGS_Destroyed)
        SetStatObj(m_pDestroyedGeometry);
      else if (state == eVGS_Default)
        SetStatObj(m_pGeometry);
    }
  }
    
  m_state = state;

  return true;
}


//------------------------------------------------------------------------
const Matrix34& CVehiclePartAnimatedJoint::GetLocalTM(bool relativeToParentPart)
{
    FUNCTION_PROFILER( gEnv->pSystem, PROFILE_GAME);

	if (m_pCharInstance && m_jointId > -1)
	{ 
    VALIDATE_MAT(m_localTM);

    if (!m_isMoveable)
    {     
      ISkeletonAnim* pSkeletonAnim = m_pCharInstance->GetISkeletonAnim();
			ISkeletonPose* pSkeletonPose = m_pCharInstance->GetISkeletonPose();
      if (pSkeletonAnim /*&& pSkeletonAnim->GetNumAnimsInFIFO(0)*/) //CryMP: Commented for working VTOL wing exhaust
      { 
        const QuatT& relJointQuat = pSkeletonPose->GetRelJointByID(m_jointId);
        assert(relJointQuat.IsValid());
        
        SetLocalBaseTM(Matrix34(relJointQuat));                 
      }
    }

    if (relativeToParentPart)
    {
      return m_localTM;
    }
    else      
    {
      const Matrix34& vehicleTM = LocalToVehicleTM(m_localTM);
      assert(vehicleTM.IsValid() && vehicleTM.IsOrthonormalRH());
      
      return VALIDATE_MAT(vehicleTM);
    }
	}

	m_localTM.SetIdentity();
	return m_localTM;	
}

//------------------------------------------------------------------------
void CVehiclePartAnimatedJoint::SetLocalTM(const Matrix34& localTM)
{
	if (!m_pCharInstance || m_jointId < 0)
		return;

  assert(localTM.IsValid() && localTM.IsOrthonormalRH());

  if (m_localTM.IsEquivalent(VALIDATE_MAT(localTM), 0.0001f))
		return;
    
  m_localTM = localTM;
  
  if (m_pAnimatedPart)
    m_pAnimatedPart->RotationChanged(this);
  
  InvalidateTM(true);
}


//------------------------------------------------------------------------
void CVehiclePartAnimatedJoint::SetLocalBaseTM(const Matrix34& baseTM)
{ 
  m_baseTM = VALIDATE_MAT(baseTM);  

  SetLocalTM(m_baseTM);
}


//------------------------------------------------------------------------
void CVehiclePartAnimatedJoint::SerMatrix(TSerialize ser, Matrix34& mat)
{
	Vec3 col0 = mat.GetColumn(0);
	Vec3 col1 = mat.GetColumn(1);
	Vec3 col2 = mat.GetColumn(2);
	Vec3 col3 = mat.GetColumn(3);
	ser.Value("matrixCol0", col0);
	ser.Value("matrixCol1", col1);
	ser.Value("matrixCol2", col2);
	ser.Value("matrixCol3", col3);
	if(ser.IsReading())
	{
		mat.SetColumn(0, col0);
		mat.SetColumn(1, col1);
		mat.SetColumn(2, col2);
		mat.SetColumn(3, col3);
	}
}


//------------------------------------------------------------------------
void CVehiclePartAnimatedJoint::Serialize(TSerialize ser, EEntityAspects aspects)
{
	CVehiclePartBase::Serialize(ser, aspects);

	if(ser.GetSerializationTarget() != eST_Network)
	{
		ser.BeginGroup("VehiclePartJointMatrices");
		ser.Value("MatrixInvalid", m_localTMInvalid);		
		Matrix34 baseTM(m_baseTM);
    SerMatrix(ser, baseTM);		
		ser.EndGroup();		

    if (ser.IsReading())
    {
      SetLocalBaseTM(baseTM);
    }
	}
}


//------------------------------------------------------------------------
const Matrix34& CVehiclePartAnimatedJoint::GetWorldTM()
{
    FUNCTION_PROFILER(gEnv->pSystem, PROFILE_GAME);

	if (m_pCharInstance && m_jointId > -1)
  { 
    const Matrix34& localTM = GetLocalTM(false);
    m_worldTM = m_pVehicle->GetEntity()->GetWorldTM() * localTM;
  }
	else
		m_worldTM = GetEntity()->GetSlotWorldTM(m_slot);

  assert(m_worldTM.IsValid() && m_worldTM.IsOrthonormalRH());
  	
  return VALIDATE_MAT(m_worldTM);
}

//------------------------------------------------------------------------
const AABB& CVehiclePartAnimatedJoint::GetLocalBounds()
{
	if (m_pCharInstance && m_jointId > -1)
	{
		ISkeletonPose* pSkeletonPose = m_pCharInstance->GetISkeletonPose();
		if (IStatObj* pStatObj = pSkeletonPose->GetStatObjOnJoint(m_jointId))
			m_localBounds = pStatObj->GetAABB();
		else
			m_localBounds.Reset();

    m_localBounds.SetTransformedAABB( GetLocalTM(false), m_localBounds );
    
		return m_localBounds;
	}
	else
	{
		GetEntity()->GetLocalBounds(m_localBounds);    
		return m_localBounds;
	}
}

//------------------------------------------------------------------------
void CVehiclePartAnimatedJoint::Update(float frameTime)
{
    FUNCTION_PROFILER( GetISystem(), PROFILE_GAME);

	CVehiclePartBase::Update(frameTime);

	if (!m_pCharInstance || m_jointId < 0)		
    return;

	if (m_dialsRotMax > 0.f)
	{

		if(m_pVehicle->IsPlayerDriving(true)||m_pVehicle->IsPlayerPassenger())
		{

			if (m_name == "dials_speedometer")
			{      
				float value = 0.0f;
				IGameTokenSystem* pGTS = gEnv->pGame->GetIGameFramework()->GetIGameTokenSystem();
				pGTS->GetTokenValueAs("vehicle.speedNorm", value);      
				if ( fabsf(m_initialRotOfs) > 0.0f )
				{
					float halfmeter = fabsf(m_initialRotOfs)*0.25f;
					if ( value < halfmeter )
						value = halfmeter;
				}
				SetLocalBaseTM(m_initialTM * Matrix33::CreateRotationZ( - value * m_dialsRotMax - m_initialRotOfs ));
			}
			else if (m_name == "dials_revometer")
			{
				float value = 0.0f;
				IGameTokenSystem* pGTS = gEnv->pGame->GetIGameFramework()->GetIGameTokenSystem();
				pGTS->GetTokenValueAs("vehicle.rpmNorm", value);
				SetLocalBaseTM(m_initialTM * Matrix33::CreateRotationZ( - value * m_dialsRotMax - m_initialRotOfs ));
			}

		}

	}

}

//------------------------------------------------------------------------
IStatObj* CVehiclePartAnimatedJoint::GetStatObj()
{
	if (m_pCharInstance && m_jointId > -1)
	{
		ISkeletonPose* pSkeletonPose = m_pCharInstance->GetISkeletonPose();
		assert(pSkeletonPose);
		return pSkeletonPose->GetStatObjOnJoint(m_jointId);
	}

	return NULL;
}

//------------------------------------------------------------------------
bool CVehiclePartAnimatedJoint::SetStatObj(IStatObj* pStatObj)
{
	if (!m_pCharInstance || m_jointId == -1)
		return false;

  if (pStatObj)
  { 
    IStatObj* pOld = GetEntity()->GetStatObj(GetCGASlot(m_jointId)); 
    if (!pOld)
    {
      if (pStatObj->IsPhysicsExist())
      {
        // if setting a StatObj (with physics) on an empty joint, need to rephysicalize parent
        m_pVehicle->RequestPhysicalization(GetParent(true), true);
        m_pVehicle->RequestPhysicalization(this, true);
      }        
    }    
  }
  	
  GetEntity()->SetStatObj(pStatObj, GetCGASlot(m_jointId), true);
  
  if (pStatObj && m_pGeometry)
    m_pMaterial = pStatObj->GetMaterial();

	return true;
}

//------------------------------------------------------------------------
IMaterial* CVehiclePartAnimatedJoint::GetMaterial()
{
	if (m_pCharInstance && m_jointId > -1)
	{
		if (IStatObj* pStatObj = m_pCharInstance->GetISkeletonPose()->GetStatObjOnJoint(m_jointId))
		{
			return pStatObj->GetMaterial();
		}
	}

	return NULL;
}

//------------------------------------------------------------------------
void CVehiclePartAnimatedJoint::SetMaterial(IMaterial* pMaterial)
{
	if (m_pCharInstance && m_jointId > -1)
	{
		if (IStatObj* pStatObj = m_pCharInstance->GetISkeletonPose()->GetStatObjOnJoint(m_jointId))
    {
      // if external geometry used, re-assign original material; the CGA might have overwritten it beforehand      
      if (m_pGeometry && m_pMaterial)
			{
        pStatObj->SetMaterial(m_pMaterial);
				m_pCharInstance->GetISkeletonPose()->SetMaterialOnJoint(m_jointId, m_pMaterial);
			}
      else
        pStatObj->SetMaterial(pMaterial);
    }
	}
}

//------------------------------------------------------------------------
void CVehiclePartAnimatedJoint::ApplyRotations()
{
  //if (!m_localTMInvalid) //CryMP: commented, otherwise turrets fallback to original rotation
    //return;

	if (!m_pCharInstance)
    return;
    
  VALIDATE_MAT(m_localTM);	(void)m_localTM;
  
  if (VehicleCVars().v_draw_tm)
  {         
    VehicleUtils::DrawTM(GetWorldTM());    
  }
    
  if (ISkeletonPose* pSkeletonPose = m_pCharInstance->GetISkeletonPose())
  {
    // write joint rotation, keep translation
    QuatT newRelQuat = pSkeletonPose->GetRelJointByID(m_jointId);
    newRelQuat.q = Quat(m_localTM);  
    pSkeletonPose->SetPostProcessQuat(m_jointId, newRelQuat);   

    InvalidateTM(false);
  }
}

//------------------------------------------------------------------------
void CVehiclePartAnimatedJoint::InvalidateTM(bool invalidate)
{ 
  if (invalidate && !m_localTMInvalid)
  {
    // invalidate all children
    for (TVehicleChildParts::iterator it = m_children.begin(); it != m_children.end(); ++it)
      (*it)->InvalidateTM(true);
  }
  
  m_localTMInvalid = invalidate;
}

//------------------------------------------------------------------------
void CVehiclePartAnimatedJoint::OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
{
  CVehiclePartBase::OnVehicleEvent(event, params);

  switch (event)
  {
  case eVE_PreVehicleDeletion:
    { 
      m_pAnimatedPart = 0;
      break;
    }
  }
}

//------------------------------------------------------------------------
void CVehiclePartAnimatedJoint::OnEvent(const SVehiclePartEvent& event)
{
  CVehiclePartBase::OnEvent(event);
}



DEFINE_VEHICLEOBJECT(CVehiclePartAnimatedJoint);
