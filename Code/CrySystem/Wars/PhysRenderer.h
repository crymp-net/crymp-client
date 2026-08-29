//////////////////////////////////////////////////////////////////////
//
//	Crytek CryENGINE Source code
// 
//	File: PhysRenderer.h	
//  Description: declaration of a simple dedicated renderer for the physics subsystem
// 
//	History:
//
//////////////////////////////////////////////////////////////////////

#ifndef PHYSRENDERER_H
#define PHYSRENDERER_H

#if _MSC_VER > 1000
# pragma once
#endif

struct SRayRec {
	Vec3 origin;
	Vec3 dir;
	float time;
	int idxColor;
};

struct SGeomRec {
	int itype;
	char buf[sizeof(primitives::box)];
	Vec3 offset;
	Matrix33 R;
	float scale;
	Vec3 sweepDir;
	float time;
};


class CPhysRenderer : public IPhysRenderer {
public:
	CPhysRenderer();
	~CPhysRenderer();
	void Init();
	void DrawGeometry(IGeometry *pGeom, geom_world_data *pgwd, const ColorB &clr, const Vec3 &sweepDir=Vec3(0,0,0));
	void DrawGeometry(int itype, const void *pGeomData, geom_world_data *pgwd, const ColorB &clr, const Vec3 &sweepDir=Vec3(0,0,0));
	void DrawBuffers(float dt);
	void SetCamera(CCamera *pCamera) { m_pCamera = pCamera; }

	virtual void DrawGeometry(IGeometry *pGeom, geom_world_data *pgwd, int idxColor=0, int bSlowFadein=0, const Vec3 &sweepDir=Vec3(0,0,0));
	virtual void DrawLine(const Vec3& pt0, const Vec3& pt1, int idxColor=0, int bSlowFadein=0);
	virtual void DrawText(const Vec3 &pt, const char *txt, int idxColor, float saturation=0) {
		if (!m_pCamera->IsPointVisible(pt))
			return;
		float clr[4] = { min(saturation*2,1.0f), 0,0,1 };
		clr[1] = min((1-saturation)*2,1.0f)*0.5f;
		clr[idxColor+1] = min((1-saturation)*2,1.0f);
		gEnv->pRenderer->DrawLabelEx(pt, 1.5f,clr,true,true, txt);
	}
	virtual const char *GetForeignName(void *pForeignData,int iForeignData,int iForeignFlags);

	float m_cullDist,m_wireframeDist;
	float m_timeRayFadein;
	float m_rayPeakTime;

protected:
	CCamera *m_pCamera;
	IRenderAuxGeom *m_pAuxRenderer;
	IRenderer *m_pRenderer;
	SRayRec *m_rayBuf;
	int m_szRayBuf,m_iFirstRay,m_iLastRay,m_nRays;
	SGeomRec *m_geomBuf;
	int m_szGeomBuf,m_iFirstGeom,m_iLastGeom,m_nGeoms;
	IGeometry *m_pRayGeom;
	primitives::ray *m_pRay;
	static ColorB g_colorTab[8];
	volatile int m_lockDrawGeometry;
};

#endif
