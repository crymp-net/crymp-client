/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: DebugGun Implementation

-------------------------------------------------------------------------
History:
- 09:01:2006   14:00 : Created by Michael Rauh

*************************************************************************/
#ifndef __DebugGun_H__
#define __DebugGun_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include "CryCommon/CryAction/IItemSystem.h"
#include "Weapon.h"

struct IRenderNode;

class CDebugGun :
  public CWeapon
{
public:
  CDebugGun();
  void OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value);
  void Update(SEntityUpdateContext& ctx, int update);
  void Shoot( bool bPrimary);
	virtual void GetMemoryStatistics(ICrySizer * s) { s->Add(*this); CWeapon::GetMemoryStatistics(s); s->AddContainer(m_fireModes); }

  virtual void Select(bool select);
  virtual void PostUpdate(float frameTime) override;

private:
  ICVar* m_pAIDebugDraw;
  int m_aiDebugDrawPrev;
  
  typedef std::pair<string, float> TFmPair;
  std::vector<TFmPair> m_fireModes;    
  size_t m_fireMode;

  void LogMaterial(IMaterial* pMat, IRenderNode* pNode, int& x, int& y, float font, float* color, const char* label);
  void EnableHighlighting(IMaterial* pMat, IRenderNode *pNode);
  void DisableHighLighting();
  void DrawBackgroundBox2D(float x, float y, float width, float height, float alpha);
  void DrawLog(int& x, int& y, float font, float* color, const char* fmt, ...);
  IRenderNode* GetRenderNodeFromCollider(IPhysicalEntity* pCollider);
  IMaterial* m_pLastHighlightedMat = nullptr;
  int m_lastHighlightedType = -1;
  int m_selectedSurfaceIdx = 0;

  bool m_armLeft = false;
  bool m_armRight = false;
  bool m_removeGripForEntity = false;

  // helpers
  void ArmLeftCapture();
  void ArmRightCapture();
  void CaptureIfArmed(IEntity* pEntity, const ray_hit& rayhit);
  void DebugDrawHandGripsForEntity(IEntity* pEntity, float lifeSec);

  void DumpGripListOnly() const;

  void PlaceTempSphere(const Vec3& worldPos, bool isRight, float lifeSec) const;

};

#endif // __DebugGun_H__
