#include "Heightfield.h"
#include "PhysicalEntity.h"
#include "PhysicalWorld.h"
#include "Qhull.h"
#include "RigidBody.h"
#include "SingleBoxTree.h"
#include "TriMesh.h"
#include "Utils.h"
#include "WaterMan.h"

struct SWaterTile : SWaterTileBase
{
	int nCells = 0;

	explicit SWaterTile(int _nCells) : nCells(_nCells)
	{
		bActive = 0;
		ph = new float[sqr(nCells)];
		pvel = new float[sqr(nCells)];
	}

	~SWaterTile()
	{
		delete[] ph;
		delete[] pvel;
	}

	SWaterTile* Activate()
	{
		if (!bActive)
		{
			bActive = 1;
			memset(ph, 0, sqr(nCells) * sizeof(ph[0]));
			memset(pvel, 0, sqr(nCells) * sizeof(pvel[0]));
		}

		return this;
	}

	SWaterTile* Deactivate()
	{
		bActive = 0;
		return this;
	}
};

CWaterMan::CWaterMan(CPhysicalWorld* pWorld)
{
	m_pWorld = pWorld;
	m_nTiles = -1;
	m_nCells = 32;
	m_pTiles = 0;
	m_pTilesTmp = 0;
	m_pCellMask = 0;
	m_dt = 0.02f;
	m_timeSurplus = 0;
	m_rtileSz = 1.0f / (m_tileSz = 10.0f);
	m_cellSz = m_tileSz / m_nCells;
	m_waveSpeed = 5.f;
	m_minhSpread = 0.05f;
	m_minVel = 0.1f;
	m_dampingCenter = 1.0f;
	m_dampingRim = 3.5f;
	m_origin.zero();
	m_waterOrigin.Set(1E10f, 1E10f, 1E10f);
	m_ix = m_iy = -10000;
	m_R.SetIdentity();
	m_pCellMask = 0;
	m_posViewer.zero();
	m_bActive = 0;
	m_pCellQueue = new int[m_szCellQueue = 64];
}

CWaterMan::~CWaterMan()
{
	if (m_pTiles)
	{
		for (int i = 0; i <= sqr((m_nTiles * 2) + 1); i++)
		{
			delete m_pTiles[i];
		}
		delete[] m_pTiles;
		delete[] m_pTilesTmp;
		delete[] m_pCellMask;
		delete[] m_pCellNorm;
	}
	if (m_pCellQueue)
	{
		delete[] m_pCellQueue;
	}
}

int CWaterMan::SetParams(pe_params* _params)
{
	if (_params->type == pe_params_waterman::type_id)
	{
		pe_params_waterman* params = (pe_params_waterman*)_params;
		int i, bRealloc = 0;
		if (!is_unused(params->nExtraTiles))
		{
			m_nTiles = params->nExtraTiles;
			bRealloc = 1;
		}
		if (!is_unused(params->nCells))
		{
			m_nCells = params->nCells;
			m_rcellSz = 1.0f / (m_cellSz = m_tileSz / m_nCells);
			bRealloc = 1;
		}
		if (!is_unused(params->tileSize))
		{
			m_rtileSz = 1.0f / (m_tileSz = params->tileSize);
			m_rcellSz = 1.0f / (m_cellSz = m_tileSz / m_nCells);
		}
		if (bRealloc)
		{
			if (m_pTiles)
			{
				for (i = 0; i < sqr((m_nTiles * 2) + 1); i++)
				{
					delete m_pTiles[i];
				}
				delete[] m_pTiles;
				delete[] m_pTilesTmp;
				delete[] m_pCellMask;
				delete[] m_pCellNorm;
				m_pTiles = 0;
				m_pTilesTmp = 0;
				m_pCellMask = 0;
			}
			m_pTiles = new SWaterTile*[sqr((m_nTiles * 2) + 1) + 1];
			m_pTilesTmp = new SWaterTile*[sqr((m_nTiles * 2) + 1) + 1];
			m_pCellMask = new char[i = sqr(((m_nTiles * 2 + 1) * m_nCells) + 2)];
			memset(m_pCellNorm = new vector2df[i], 0, i * sizeof(m_pCellNorm[0]));
			for (i = 0; i <= sqr((m_nTiles * 2) + 1); i++)
			{
				(m_pTiles[i] = new SWaterTile(m_nCells))->Activate()->Deactivate();
			}
		}
		if (!is_unused(params->timeStep))
		{
			m_dt = params->timeStep;
		}
		if (!is_unused(params->waveSpeed))
		{
			m_waveSpeed = params->waveSpeed;
		}
		if (!is_unused(params->dampingCenter))
		{
			m_dampingCenter = params->dampingCenter;
		}
		if (!is_unused(params->dampingRim))
		{
			m_dampingCenter = params->dampingRim;
		}
		if (!is_unused(params->minhSpread))
		{
			m_minhSpread = params->minhSpread;
		}
		if (!is_unused(params->minVel))
		{
			m_minVel = params->minVel;
		}
		if (!is_unused(params->posViewer))
		{
			SetViewerPos(params->posViewer, 1);
		}
		return 1;
	}
	return 0;
}

int CWaterMan::GetParams(pe_params* _params)
{
	if (_params->type == pe_params_waterman::type_id)
	{
		pe_params_waterman* params = (pe_params_waterman*)_params;
		params->posViewer = m_posViewer;
		params->nExtraTiles = m_nTiles;
		params->nCells = m_nCells;
		params->tileSize = m_tileSz;
		params->timeStep = m_dt;
		params->waveSpeed = m_waveSpeed;
		params->dampingCenter = m_dampingCenter;
		params->dampingRim = m_dampingRim;
		params->minhSpread = m_minhSpread;
		params->minVel = m_minVel;
		return 1;
	}
	return 0;
}

int CWaterMan::GetStatus(pe_status* _status)
{
	if (_status->type == pe_status_waterman::type_id)
	{
		pe_status_waterman* status = (pe_status_waterman*)_status;
		status->bActive = m_bActive;
		status->nTiles = (m_nTiles * 2) + 1;
		status->nCells = m_nCells;
		status->R = m_R;
		status->origin = m_origin - m_R * Vec3(m_nTiles + 0.5f, m_nTiles + 0.5f, 0) * m_tileSz;
		status->pTiles = (SWaterTileBase**)m_pTiles;
		return 1;
	}
	return 0;
}

void CWaterMan::OnWaterInteraction(CPhysicalEntity* pent)
{
	if (m_nTiles < 0)
	{
		return;
	}

	int i, j, ipart, icont, npt, icell, ncont, nMeshes, nPrims;
	int idx[] = {0, 1, 2, 0, 3, 1, 0, 2, 3, 1, 3, 2};
	trinfo top[4] = {{{1, 3, 2}}, {{2, 3, 0}}, {{0, 3, 1}}, {{1, 2, 0}}};
	Vec3 vtx[4], normals[4], wloc, org, sz, pt, vloc, dir;
	vector2di ibbox[2], ibbox1[2], ic, itile, strideTiles(1, (m_nTiles * 2) + 1);
	float w, h;
	char* pcell;
	CTriMesh mesh;
	CSingleBoxTree sbtree;
	geom_world_data gwd;
	intersection_params ip;
	geom_contact* pcontacts;
	ptitem2d pts[256], *pvtx, *plist;
	edgeitem pcontour[256], *pedge;
	primitives::grid wgrid;
	SWaterTile* ptile;
	jgrid_checker jgc;
	Matrix33 Rabs;
	RigidBody body, *pbody;
	primitives::box bbox;

	// project entity's bbox on the water grid, intersect with the viewer's vicinity rect
	org = ((pent->m_BBox[0] + pent->m_BBox[1]) * 0.5f - m_origin) * m_R;
	sz = (Rabs = m_R).Fabs() * ((pent->m_BBox[1] - pent->m_BBox[0]) * 0.5f);
	ibbox[0].x = max(-m_nTiles, physics_float2int((org.x - sz.x) * m_rtileSz));
	ibbox[0].y = max(-m_nTiles, physics_float2int((org.y - sz.y) * m_rtileSz));
	ibbox[1].x = min(m_nTiles, physics_float2int((org.x + sz.x) * m_rtileSz));
	ibbox[1].y = min(m_nTiles, physics_float2int((org.y + sz.y) * m_rtileSz));
	if (((ibbox[1].x - ibbox[0].x) | (ibbox[1].y - ibbox[0].y)) < 0)
	{
		return;
	}
	wgrid.size.set((ibbox[1].x - ibbox[0].x + 1) * m_nCells, (ibbox[1].y - ibbox[0].y + 1) * m_nCells);
	wgrid.step.set(m_cellSz, m_cellSz);
	wgrid.stepr.set(m_rcellSz, m_rcellSz);
	wgrid.stride.set(1, wgrid.size.x + 1);
	wgrid.origin.zero();
	jgc.pgrid = &wgrid;
	jgc.ppt = 0;
	jgc.pCellMask = m_pCellMask + wgrid.size.x + 2;
	jgc.pnorms = m_pCellNorm;
	jgc.bMarkCenters = 1;

	// build a tetrahedron around the intersection rect
	w = (ibbox[1].x - ibbox[0].x + 1) * m_tileSz;
	h = (ibbox[1].y - ibbox[0].y + 1) * m_tileSz;
	vtx[0].x = -(vtx[1].x = w * 1.01f);
	vtx[0].y = vtx[1].y = h * -0.51f;
	vtx[2].y = h * 1.51f;
	vtx[3].z = (w + h) * -0.5f;
	vtx[0].z = vtx[1].z = vtx[2].z = vtx[2].x = vtx[3].x = vtx[3].y = 0;

	mesh.m_nTris = mesh.m_nVertices = 4;
	mesh.m_pNormals = normals;
	mesh.m_pVertices = vtx;
	mesh.m_pIndices = idx;
	mesh.m_flags = mesh_shared_topology | mesh_shared_idx | mesh_shared_vtx | mesh_shared_normals;
	mesh.m_nMaxVertexValency = 3;
	for (i = 0; i < 4; i++)
	{
		normals[i] =
		    (vtx[idx[(i * 3) + 1]] - vtx[idx[i * 3]] ^ vtx[idx[(i * 3) + 2]] - vtx[idx[i * 3]]).normalized();
	}
	mesh.m_pTopology = top;
	sbtree.m_Box.Basis.SetIdentity();
	sbtree.m_Box.bOriented = 0;
	sbtree.m_Box.center.Set(0, h * 0.5f, vtx[3].z * 0.5f);
	sbtree.m_Box.size.Set(w * 1.01f, h * 1.01f, vtx[3].z * -0.5f);
	sbtree.m_pGeom = &mesh;
	sbtree.m_nPrims = mesh.m_nTris;
	mesh.m_pTree = &sbtree;
	memset(m_pCellMask + wgrid.size.x + 1, 28, (wgrid.size.x + 1) * wgrid.size.y * sizeof(m_pCellMask[0]));
	for (i = 0; i < wgrid.size.x + 2; i++)
	{
		m_pCellMask[i] = m_pCellMask[(wgrid.stride.y * (wgrid.size.y + 1)) + i] = 66;
	}
	for (i = 0; i < wgrid.size.y + 2; i++)
	{
		m_pCellMask[i * wgrid.stride.y] = 66;
	}

	for (ipart = nMeshes = nPrims = 0; ipart < pent->m_nParts; ipart++)
	{
		// intersect each entity part with the prism
		gwd.offset = (pent->m_pos + pent->m_qrot * pent->m_parts[ipart].pos - m_origin) * m_R -
		             Vec3(ibbox[0].x + ibbox[1].x, ibbox[0].y + ibbox[1].y, 0) * m_tileSz * 0.5f;
		gwd.R = m_R.T() * Matrix33(pent->m_qrot * pent->m_parts[ipart].q);
		gwd.scale = pent->m_parts[ipart].scale;
		j = pent->m_parts[ipart].pPhysGeomProxy->pGeom->GetType();
		if (j == GEOM_TRIMESH || j == GEOM_VOXELGRID || j == GEOM_HEIGHTFIELD)
		{
			ncont = mesh.Intersect(pent->m_parts[ipart].pPhysGeomProxy->pGeom, 0, &gwd, &ip, pcontacts);
			if (ncont)
			{
				WriteLockCond lockColl(*ip.plock, 0);
				lockColl.SetActive();
				for (icont = 0; icont < ncont; icont++)
				{
					for (i = npt = 0; i < pcontacts[icont].nborderpt; i++)
					{
						if (fabs_tpl(pcontacts[icont].ptborder[i].z) < 0.001f)
						{
							(pts[npt].pt = vector2df(pcontacts[icont].ptborder[i])) +=
							    vector2df(w, h) * 0.5f;
							pts[npt].next = pts + npt + 1;
							pts[npt].prev = pts + npt - 1;
							if (++npt == sizeof(pts) / sizeof(pts[0]))
							{
								break;
							}
						}
					}
					pts[0].prev = pts + npt - 1;
					pts[npt - 1].next = pts;
					plist = pts;
					if (!pcontacts[icont].bBorderConsecutive && qhull2d(pts, npt, pcontour))
					{
						plist = pvtx = (pedge = pcontour)->pvtx;
						npt = 0;
						do
						{
							pvtx->next = pedge->next->pvtx;
							pvtx->prev = pedge->prev->pvtx;
							pvtx = pvtx->next;
							npt++;
						}
						while ((pedge = pedge->next) != pcontour);
					}

					jgc.iedge[0] = jgc.iedge[1] = jgc.iprevcell = jgc.iedgeExit0 = -1;
					for (pvtx = plist, i = 0, j = 2; i < npt; i++, pvtx = pvtx->next)
					{
						jgc.org = pvtx->pt;
						jgc.dirn = norm(jgc.dir = pvtx->next->pt - pvtx->pt);
						Vec3 gorg(jgc.org.x, jgc.org.y, 0), gdir(jgc.dir.x, jgc.dir.y, 0);
						DrawRayOnGrid(&wgrid, gorg, gdir, jgc);
					}
					if (jgc.iedge[0] != jgc.iedgeExit0)
					{
						for (i = (jgc.iedgeExit0 + 1) & 3; i != jgc.iedge[0]; i = (i + 1) & 3)
						{
							j |= 4 << i;
						}
					}
					jgc.pCellMask[vector2di(jgc.iprevcell & 0xFFFF, jgc.iprevcell >> 16) *
					              wgrid.stride] &= j;
				}
				nMeshes++;
			}
		}
		else
		{
			// for non-mesh geoms, just project them on the grid and check each affected cell for point
			// containment
			pent->m_parts[ipart].pPhysGeomProxy->pGeom->GetBBox(&bbox);
			org = (pent->m_qrot * (pent->m_parts[ipart].q * bbox.center * pent->m_parts[ipart].scale +
			                       pent->m_parts[ipart].pos) +
			       pent->m_pos - m_origin) *
			      m_R;
			gwd.R = Matrix33(!(pent->m_qrot * pent->m_parts[ipart].q)) * m_R;
			gwd.scale = pent->m_parts[ipart].scale == 1.0f ? 1.0f : 1.0f / pent->m_parts[ipart].scale;
			gwd.offset.Set(((ibbox[0].x - 0.5f) * m_tileSz) + (m_cellSz * 0.5f),
			               ((ibbox[0].y - 0.5f) * m_tileSz) + (m_cellSz * 0.5f), 0);
			gwd.offset =
			    ((m_R * gwd.offset + m_origin - pent->m_pos) * pent->m_qrot - pent->m_parts[ipart].pos) *
			    pent->m_parts[ipart].q * gwd.scale;
			gwd.scale *= m_cellSz;
			sz = (bbox.size * (bbox.Basis *= gwd.R).Fabs()) * pent->m_parts[ipart].scale;
			ibbox1[0].x = max(0, physics_float2int(((org.x - sz.x) * m_rcellSz) -
			                                       ((ibbox[0].x - 0.5f) * m_nCells) - 0.5f));
			ibbox1[0].y = max(0, physics_float2int(((org.y - sz.y) * m_rcellSz) -
			                                       ((ibbox[0].y - 0.5f) * m_nCells) - 0.5f));
			ibbox1[1].x = min(
			    ((ibbox[1].x - ibbox[0].x + 1) * m_nCells) - 1,
			    physics_float2int(((org.x + sz.x) * m_rcellSz) - ((ibbox[0].x - 0.5f) * m_nCells) - 0.5f));
			ibbox1[1].y = min(
			    ((ibbox[1].y - ibbox[0].y + 1) * m_nCells) - 1,
			    physics_float2int(((org.y + sz.y) * m_rcellSz) - ((ibbox[0].y - 0.5f) * m_nCells) - 0.5f));
			pcell = jgc.pCellMask + ibbox1[0] * wgrid.stride;
			for (ic.y = ibbox1[0].y; ic.y <= ibbox1[1].y;
			     ic.y++, pcell += wgrid.stride.y - (ibbox1[1].x - ibbox1[0].x + 1))
			{
				for (ic.x = ibbox1[0].x, pt = gwd.R * Vec3(ic.x, ic.y, 0) * gwd.scale + gwd.offset;
				     ic.x <= ibbox1[1].x; ic.x++, pcell++, pt += gwd.R.GetColumn(0) * gwd.scale)
				{
					*pcell |= pent->m_parts[ipart].pPhysGeomProxy->pGeom->PointInsideStatus(pt) * 2;
				}
			}
			// find border cells by counting filled neighbours; set cell normal to
			// cell_center-primitive_center
			org -= Vec3(((ibbox[0].x - 0.5f) * m_tileSz) + (0.5f * m_cellSz),
			            ((ibbox[0].y - 0.5f) * m_tileSz) + (0.5f * m_cellSz), 0);
			for (ic.y = ibbox1[0].y, icell = ibbox1[0] * wgrid.stride; ic.y <= ibbox1[1].y;
			     ic.y++, icell += wgrid.stride.y - (ibbox1[1].x - ibbox1[0].x + 1))
			{
				for (ic.x = ibbox1[0].x, pt = gwd.R * Vec3(ic.x, ic.y, 0) * gwd.scale + gwd.offset;
				     ic.x <= ibbox1[1].x; ic.x++, icell++, pt += gwd.R.GetColumn(0) * gwd.scale)
				{
					if (((jgc.pCellMask[icell - 1] ^ jgc.pCellMask[icell - 1] >> 5) & 2) +
					        ((jgc.pCellMask[icell - wgrid.stride.y] ^
					          jgc.pCellMask[icell - wgrid.stride.y] >> 5) &
					         2) +
					        ((jgc.pCellMask[icell + 1] ^ jgc.pCellMask[icell + 1] >> 5) & 2) +
					        ((jgc.pCellMask[icell + wgrid.stride.y] ^
					          jgc.pCellMask[icell + wgrid.stride.y] >> 5) &
					         2) <
					    (jgc.pCellMask[icell] & 2) * 4)
					{
						jgc.pnorms[icell].set((ic.x * m_cellSz) - org.x,
						                      (ic.y * m_cellSz) - org.y);
					}
				}
			}
			nPrims++;
		}
	}

	if (nMeshes + nPrims)
	{
		if (nMeshes)
		{
			jgc.pqueue = m_pCellQueue;
			jgc.szQueue = m_szCellQueue;
			for (i = 0; i < wgrid.size.x * wgrid.size.y; i++)
			{
				if (jgc.pCellMask[i] & 2 && jgc.pCellMask[i] & 28)
				{
					for (j = 0; j < 4; j++)
					{
						if (jgc.pCellMask[i] & 4 << j &&
						    !(jgc.pCellMask[icell = i + vector2di((1 - (j & 2)) & -(j & 1),
						                                          ((j & 2) - 1) & ~-(j & 1)) *
						                                    wgrid.stride] &
						      2))
						{
							jgc.MarkCellInteriorQueue(icell);
						}
					}
				}
			}
			m_pCellQueue = jgc.pqueue;
			m_szCellQueue = jgc.szQueue;
		}
		pbody = pent->GetRigidBodyData(&body, ipart);
		for (itile.x = ibbox[0].x; itile.x <= ibbox[1].x; itile.x++)
		{
			for (itile.y = ibbox[0].y; itile.y <= ibbox[1].y; itile.y++)
			{
				(ptile = m_pTiles[(itile + vector2di(1, 1) * m_nTiles) * strideTiles])->Activate();
				icell = ((itile - ibbox[0]) * wgrid.stride) * m_nCells;
				wloc = pbody->w * m_R;
				org = Vec3(((itile.x - 0.5f) * m_tileSz) + (0.5f * m_cellSz),
				           ((itile.y - 0.5f) * m_tileSz) + (0.5f * m_cellSz), 0) -
				      (pbody->pos - m_origin) * m_R;
				vloc = pbody->v * m_R + (wloc ^ org);
				for (ic.y = i = 0; ic.y < m_nCells; ic.y++, icell += wgrid.stride.y - m_nCells)
				{
					for (ic.x = 0; ic.x < m_nCells; ic.x++, icell++, i++)
					{
						if ((jgc.pCellMask[icell] & 3) == 2)
						{
							// ptile->pvel[i] = min(ptile->pvel[i],
							// (v+(wloc.x*ic.y-wloc.y*ic.x)*m_cellSz)*m_dt);
							dir.Set(0, 0, 1);
							if (len2(jgc.pnorms[icell]) > 0)
							{
								vector2df dir2d = norm(jgc.pnorms[icell]);
								dir += Vec3(dir2d.x, dir2d.y, 0);
								jgc.pnorms[icell].zero();
							}
							ptile->pvel[i] = min(
							    0.1f,
							    ((vloc + (wloc ^ Vec3(ic.x, ic.y, 0) * m_cellSz)) * dir) *
								m_dt);
						}
					}
				}
			}
		}
	}
	mesh.m_pTree = 0;
}

void CWaterMan::OnWaterHit(const Vec3& pthit, const Vec3& vel)
{
	Vec3 pt;
	vector2di itile, icell;
	SWaterTile* ptile;
	pt = (pthit - m_origin) * m_R;
	itile.set(physics_float2int(pt.x * m_rtileSz), physics_float2int(pt.y * m_rtileSz));
	if (max(fabs_tpl(itile.x), fabs_tpl(itile.y)) > m_nTiles)
	{
		return;
	}
	ptile = m_pTiles[(itile + vector2di(1, 1) * m_nTiles) * vector2di(1, (m_nTiles * 2) + 1)];
	icell.set(physics_float2int((pt.x * m_rcellSz) - ((itile.x - 0.5f) * m_nCells) - 0.5f),
	          (int)(physics_float2int(pt.y * m_rcellSz) - ((itile.y - 0.5f) * m_nCells) - 0.5f));
	ptile->Activate();
	ptile->pvel[icell * vector2di(1, m_nCells)] += min(0.5f, max(-0.5f, m_R.GetRow(2) * vel));
}

void CWaterMan::SetViewerPos(const Vec3& newpos, int iCaller)
{
	if (m_nTiles < 0)
	{
		return;
	}

	int i, j, ix, iy, nbuoys;
	Vec3 axisx, axisy, g, newOrigin;
	vector2di ic, ibbox[2], strideTile(1, (m_nTiles * 2) + 1);
	pe_params_buoyancy pb[4];
	for (i = 0, nbuoys = m_pWorld->CheckAreas(newpos, g, pb, 4, Vec3(), 0, iCaller); i < nbuoys && pb[i].iMedium;
	     i++)
		;
	if (i >= nbuoys ||
	    fabs_tpl((newpos - pb[i].waterPlane.origin) * pb[i].waterPlane.n) > m_tileSz * (m_nTiles + 1))
	{
		m_bActive = 0;
		return;
	}

	m_bActive = 1;
	axisx = pb[i].waterPlane.origin - pb[i].waterPlane.n * (pb[i].waterPlane.n * pb[i].waterPlane.origin);
	if (axisx.len2() == 0)
	{
		axisx = Vec3(1, 0, 0) - pb[i].waterPlane.n * pb[i].waterPlane.n.x;
	}
	axisx.normalize();
	axisy = pb[i].waterPlane.n ^ axisx;
	m_R.SetFromVectors(axisx, axisy, pb[i].waterPlane.n);
	m_R.Transpose();
	ix = physics_float2int(((axisx * (newpos - pb[i].waterPlane.origin)) * m_rtileSz) - 0.5f);
	iy = physics_float2int(((axisy * (newpos - pb[i].waterPlane.origin)) * m_rtileSz) - 0.5f);
	newOrigin = pb[i].waterPlane.origin + axisx * ((ix + 0.5f) * m_tileSz) + axisy * ((iy + 0.5f) * m_tileSz);
	ibbox[0].set(max(0, ix - m_ix), max(0, iy - m_iy));
	ibbox[1].set(min(0, ix - m_ix) + (m_nTiles * 2), min(0, iy - m_iy) + (m_nTiles * 2));

	if ((m_waterOrigin - pb[i].waterPlane.origin).len2() > 0 || ibbox[1].x < ibbox[0].x || ibbox[1].y < ibbox[0].y)
	{
		for (j = 0; j < sqr((m_nTiles * 2) + 1); j++)
		{
			m_pTiles[j]->bActive = 0;
		}
	}
	else
	{
		memcpy(m_pTilesTmp, m_pTiles, sqr((m_nTiles * 2) + 1) * sizeof(m_pTiles[0]));
		for (ic.x = ibbox[0].x; ic.x <= ibbox[1].x; ic.x++)
		{
			for (ic.y = ibbox[0].y; ic.y <= ibbox[1].y; ic.y++)
			{
				m_pTiles[(ic + vector2di(m_ix - ix, m_iy - iy)) * strideTile] =
				    m_pTilesTmp[ic * strideTile];
			}
		}
		for (ic.y = j = 0; ic.y <= m_nTiles * 2; ic.y++)
		{
			for (ic.x = 0; ic.x <= m_nTiles * 2; ic.x++)
			{
				m_pTilesTmp[j] = m_pTilesTmp[ic * strideTile];
				j += (inrange(ic.x, ibbox[0].x - 1, ibbox[1].x + 1) &
				      inrange(ic.y, ibbox[0].y - 1, ibbox[1].y + 1)) ^
				     1;
			}
		}
		ibbox[0] += vector2di(m_ix - ix, m_iy - iy);
		ibbox[1] += vector2di(m_ix - ix, m_iy - iy);
		for (ic.y = j = 0; ic.y <= m_nTiles * 2; ic.y++)
		{
			for (ic.x = 0; ic.x <= m_nTiles * 2; ic.x++)
			{
				if (!(inrange(ic.x, ibbox[0].x - 1, ibbox[1].x + 1) &
				      inrange(ic.y, ibbox[0].y - 1, ibbox[1].y + 1)))
				{
					(m_pTiles[ic * strideTile] = m_pTilesTmp[j++])->bActive = 0;
				}
			}
		}
	}
	m_origin = newOrigin;
	m_ix = ix;
	m_iy = iy;
	m_waterOrigin = pb[i].waterPlane.origin;
	m_posViewer = newpos;
}

void CWaterMan::TimeStep(float time_interval)
{
	if (m_nTiles < 0 || !m_bActive)
	{
		return;
	}

	int ioffs[] = {-m_nCells, m_nCells, -1, 1};
	int i, j, ix, iy, ibrd, ilim;
	float **ph, *phbuf[5], havg, vmax, hmax, dt, damping, rnTiles = 1.0f / m_nTiles, minh = -m_cellSz * 2.0f,
							      maxh = m_cellSz * 2.0f;
	vector2di itile, itile1[4], ic, strideTile(1, (m_nTiles * 2) + 1), stride(1, m_nCells);
	SWaterTile* ptile;
	ph = phbuf + 1;

	for (dt = m_timeSurplus; dt < time_interval; dt += m_dt)
	{
		for (itile.x = 0; itile.x <= m_nTiles * 2; itile.x++)
		{
			for (itile.y = 0; itile.y <= m_nTiles * 2; itile.y++)
			{
				if ((ptile = m_pTiles[itile * strideTile])->bActive)
				{
					damping =
					    max(fabs_tpl(itile.x - m_nTiles), fabs_tpl(itile.y - m_nTiles)) * rnTiles;
					damping = (m_dampingRim * damping) + (m_dampingCenter * (1 - damping));
					damping = max(0.0f, 1 - (m_dt * damping));
					for (iy = 1, i = m_nCells + 1; iy < m_nCells - 1; iy++, i += 2)
					{
						for (ix = 1; ix < m_nCells - 1; ix++, i++)
						{
							for (j = 0, havg = 0; j < 4; j++)
							{
								havg += ptile->ph[i + ioffs[j]];
							}
							ptile->pvel[i] =
							    (ptile->pvel[i] * damping) +
							    ((havg - ptile->ph[i] * 4) * m_waveSpeed * m_dt);
						}
					}

					ph[-1] = ptile->ph;
					for (ibrd = 0; ibrd < 4; ibrd++)
					{
						ix = ibrd >> 1;
						(itile1[ibrd] = itile)[ix ^ 1] += (ibrd << 1 & 2) - 1;
						ilim = itile1[ibrd].x >> 31 | ((m_nTiles * 2) - itile1[ibrd].x) >> 31 |
						       itile1[ibrd].y >> 31 | ((m_nTiles * 2) - itile1[ibrd].y) >> 31;
						itile1[ibrd].x += ((m_nTiles * 2) + 1 - itile1[ibrd].x) & ilim;
						itile1[ibrd].y += ((m_nTiles * 2) - itile1[ibrd].y) & ilim;
						i = itile1[ibrd] * strideTile;
						ph[ibrd] =
						    m_pTiles[m_pTiles[i]->bActive ? i : sqr((m_nTiles * 2) + 1)]->ph +
						    ((m_nCells - (m_nCells - 1 & -ix)) * m_nCells *
						     (1 - (ibrd << 1 & 2)));
					}

					for (ibrd = 0; ibrd < 4; ibrd++)
					{
						ix = ibrd >> 1;
						ic[ix] = ix;
						ic[ix ^ 1] = (m_nCells - 1) & -(ibrd & 1);
						for (i = ic * stride, hmax = 0; ic[ix] < m_nCells - ix;
						     ic[ix]++, i += 1 + (m_nCells - 1 & -ix))
						{
							for (j = 0, havg = 0; j < 4; j++)
							{
								ilim = (m_nCells - 1) & -(j & 1);
								havg += ph[j | (ic[j >> 1 ^ 1] - ilim) >> 31 |
								           (ilim - ic[j >> 1 ^ 1]) >> 31][i + ioffs[j]];
							}
							ptile->pvel[i] =
							    (ptile->pvel[i] * damping) +
							    ((havg - ptile->ph[i] * 4) * m_waveSpeed * m_dt);
							hmax = max(hmax, fabs_tpl(ptile->ph[i]));
						}
						if (hmax > m_minhSpread &&
						    len2(itile1[ibrd]) < sqr(sqr((m_nTiles * 2) + 1)))
						{
							m_pTiles[itile1[ibrd] * strideTile]->Activate();
						}
					}

					for (i = 0, vmax = hmax = 0; i < sqr(m_nCells); i++)
					{
						ptile->ph[i] = min(maxh, max(minh, ptile->ph[i] + ptile->pvel[i]));
						vmax = max(vmax, fabs_tpl(ptile->pvel[i]));
						hmax = max(hmax, fabs_tpl(ptile->ph[i]));
					}

					if (max(vmax, hmax) < m_minVel * m_dt)
					{
						ptile->Deactivate();
					}
				}
			}
		}
	}
	m_timeSurplus = dt - time_interval;
}

void CWaterMan::DrawHelpers(IPhysRenderer* pRenderer)
{
	if (m_bActive)
	{
		int i;
		vector2di ic, stride(1, (m_nTiles * 2) + 1);
		CHeightfield hf;
		geom_world_data gwd;
		hf.m_hf.Basis.SetIdentity();
		hf.m_hf.size.set(m_nCells - 1, m_nCells - 1);
		hf.m_hf.step.set(m_cellSz, m_cellSz);
		hf.m_hf.stride.set(1, m_nCells);
		hf.m_hf.stepr.x = hf.m_hf.stepr.y = 1.0f / m_cellSz;
		hf.m_hf.heightscale = 1.0f;
		hf.m_hf.heightmask = 0;
		hf.m_Tree.m_phf = &hf.m_hf;
		hf.m_Tree.m_pMesh = &hf;
		hf.m_pTree = &hf.m_Tree;
		hf.m_hf.fpGetHeightCallback = 0;
		gwd.R = m_R;
		gwd.offset = m_origin - m_R * (Vec3(m_nTiles + 0.5f, m_nTiles + 0.5f, 0) * m_tileSz -
		                               Vec3(0.5f, 0.5f, 0) * m_cellSz);

		for (ic.x = 0; ic.x <= m_nTiles * 2; ic.x++)
		{
			for (ic.y = 0; ic.y <= m_nTiles * 2; ic.y++)
			{
				if (m_pTiles[i = ic * stride]->bActive)
				{
					hf.m_hf.origin.Set(ic.x * m_tileSz, ic.y * m_tileSz, 0);
					hf.m_hf.fpGetSurfTypeCallback =
					    (primitives::getSurfTypeCallback)m_pTiles[i]->ph;
					pRenderer->DrawGeometry(&hf, &gwd, 7);
				}
			}
		}
	}
}
