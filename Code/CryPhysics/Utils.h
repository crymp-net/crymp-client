#pragma once

#include <array>

#include "CryCommon/CryMath/Cry_Math.h"
#include "CryCommon/CryPhysics/IPhysics.h"

#include "Matrix.h"
#include "Quotient.h"

int physics_float2int(float x);

unsigned int physics_rand();
unsigned int physics_rand(int range);
float physics_frand(float range = 1.0f);
float physics_frand(float start, float end);

inline double min(double op1, double op2)
{
	return (op1 + op2 - fabs(op1 - op2)) * 0.5;
}

inline double max(double op1, double op2)
{
	return (op1 + op2 + fabs(op1 - op2)) * 0.5;
}

inline float max(float op1, float op2)
{
	return (op1 + op2 + fabsf(op1 - op2)) * 0.5f;
}

inline float min(float op1, float op2)
{
	return (op1 + op2 - fabsf(op1 - op2)) * 0.5f;
}

inline int max(int op1, int op2)
{
	return op1 - (op1 - op2 & (op1 - op2) >> 31);
}

inline int min(int op1, int op2)
{
	return op2 + (op1 - op2 & (op1 - op2) >> 31);
}

inline double minmax(double op1, double op2, int bMax)
{
	return (op1 + op2 + fabs(op1 - op2) * (bMax * 2 - 1)) * 0.5;
}

inline float minmax(float op1, float op2, int bMax)
{
	return (op1 + op2 + fabsf(op1 - op2) * (bMax * 2 - 1)) * 0.5f;
}

inline int minmax(int op1, int op2, int bMax)
{
	return (op1 & -bMax | op2 & ~-bMax) + ((op1 - op2 & (op1 - op2) >> 31) ^ -bMax) + bMax;
}

template<class F>
inline F max_safe(F op1, F op2)
{
	return op1 > op2 ? op1 : op2;
}

template<class F>
inline F min_safe(F op1, F op2)
{
	return op1 < op2 ? op1 : op2;
}

template<class F>
inline Vec3_tpl<F> min(const Vec3_tpl<F>& v0, const Vec3_tpl<F>& v1)
{
	return Vec3_tpl<F>(min(v0.x, v1.x), min(v0.y, v1.y), min(v0.z, v1.z));
}

template<class F>
inline Vec3_tpl<F> max(const Vec3_tpl<F>& v0, const Vec3_tpl<F>& v1)
{
	return Vec3_tpl<F>(max(v0.x, v1.x), max(v0.y, v1.y), max(v0.z, v1.z));
}

template<class F>
inline Vec3_tpl<F> min_safe(const Vec3_tpl<F>& v0, const Vec3_tpl<F>& v1)
{
	return Vec3_tpl<F>(min_safe(v0.x, v1.x), min_safe(v0.y, v1.y), min_safe(v0.z, v1.z));
}

template<class F>
inline Vec3_tpl<F> max_safe(const Vec3_tpl<F>& v0, const Vec3_tpl<F>& v1)
{
	return Vec3_tpl<F>(max_safe(v0.x, v1.x), max_safe(v0.y, v1.y), max_safe(v0.z, v1.z));
}

template<class F>
inline Vec2_tpl<F> min(const Vec2_tpl<F>& v0, const Vec2_tpl<F>& v1)
{
	return Vec2_tpl<F>(min(v0.x, v1.x), min(v0.y, v1.y));
}

template<class F>
inline Vec2_tpl<F> max(const Vec2_tpl<F>& v0, const Vec2_tpl<F>& v1)
{
	return Vec2_tpl<F>(max(v0.x, v1.x), max(v0.y, v1.y));
}

template<class F>
inline Vec2_tpl<F> min_safe(const Vec2_tpl<F>& v0, const Vec2_tpl<F>& v1)
{
	return Vec2_tpl<F>(min_safe(v0.x, v1.x), min_safe(v0.y, v1.y));
}

template<class F>
inline Vec2_tpl<F> max_safe(const Vec2_tpl<F>& v0, const Vec2_tpl<F>& v1)
{
	return Vec2_tpl<F>(max_safe(v0.x, v1.x), max_safe(v0.y, v1.y));
}

template<class F>
inline F len(const Vec2_tpl<F>& v)
{
	return sqrt_tpl(v.x * v.x + v.y * v.y);
}

template<class F>
inline F len2(const Vec2_tpl<F>& v)
{
	return v.x * v.x + v.y * v.y;
}

template<class F>
inline Vec2_tpl<F> norm(const Vec2_tpl<F>& v)
{
	F rlen = v.x * v.x + v.y * v.y;
	if (rlen > 0)
	{
		rlen = (F)1 / sqrt_tpl(rlen);
		return Vec2_tpl<F>(v.x * rlen, v.y * rlen);
	}
	return Vec2_tpl<F>(1, 0);
}

template<class F>
inline F condmax(F, int masknot)
{
	return 1 << (sizeof(int) * 8 - 2) & ~masknot;
}

inline float condmax(float, int masknot)
{
	return 1E30f * (masknot + 1);
}

template<class F>
inline int unite_lists(F* pSrc0, int nSrc0, F* pSrc1, int nSrc1, F* pDst, int szdst)
{
	int i0, i1, n;
	INT_PTR inrange0 = -nSrc0 >> 31, inrange1 = -nSrc1 >> 31;
	F a0, a1, ares, dummy = 0;
	INT_PTR pDummy((INT_PTR)&dummy);
	pSrc0 = (F*)(((INT_PTR)pSrc0 & inrange0) | (pDummy & ~inrange0));
	pSrc1 = (F*)(((INT_PTR)pSrc1 & inrange1) | (pDummy & ~inrange1));
	for (n = i0 = i1 = 0; (inrange0 | inrange1) & (n - szdst) >> 31;
	     inrange0 = ((i0 += isneg(a0 - ares - 1)) - nSrc0) >> 31,
	    inrange1 = ((i1 += isneg(a1 - ares - 1)) - nSrc1) >> 31)
	{
		a0 = pSrc0[i0 & inrange0] + condmax(pSrc0[0], inrange0);
		a1 = pSrc1[i1 & inrange1] + condmax(pSrc1[0], inrange1);
		pDst[n++] = ares = min(a0, a1);
	}
	return n;
}

template<class F>
inline int intersect_lists(F* pSrc0, int nSrc0, F* pSrc1, int nSrc1, F* pDst)
{
	int i0, i1, n;
	F ares;
	for (i0 = i1 = n = 0; isneg(i0 - nSrc0) & isneg(i1 - nSrc1);
	     i0 += isneg(pSrc0[i0] - ares - 1), i1 += isneg(pSrc1[i1] - ares - 1))
	{
		pDst[n] = ares = min(pSrc0[i0], pSrc1[i1]);
		n += iszero(pSrc0[i0] - pSrc1[i1]);
	}
	return n;
}

void compute_projection_integrals(Vec3r* ab, real pi[10]);
void compute_face_integrals(Vec3r* p, Vec3r n, real fi[12]);

real ComputeMassProperties(strided_pointer<const Vec3> points, const index_t* faces, int nfaces, Vec3r& center,
                           Matrix33r& I);

enum booltype
{
	bool_intersect = 1
};

int boolean2d(booltype type, vector2df* ptbuf1, int npt1, vector2df* ptbuf2, int npt2, int bClosed, vector2df*& ptbuf,
              int*& pidbuf);

real RotatePointToPlane(const Vec3r& pt, const Vec3r& axis, const Vec3r& center, const Vec3r& n, const Vec3r& origin);

template<class ftype, int si, int sj>
inline Matrix33_tpl<ftype> GetMtxStrided(const ftype* pdata)
{
	Matrix33_tpl<ftype> res;
	res(0, 0) = pdata[(0 * si) + (0 * sj)];
	res(0, 1) = pdata[(0 * si) + (1 * sj)];
	res(0, 2) = pdata[(0 * si) + (2 * sj)];
	res(1, 0) = pdata[(1 * si) + (0 * sj)];
	res(1, 1) = pdata[(1 * si) + (1 * sj)];
	res(1, 2) = pdata[(1 * si) + (2 * sj)];
	res(2, 0) = pdata[(2 * si) + (0 * sj)];
	res(2, 1) = pdata[(2 * si) + (1 * sj)];
	res(2, 2) = pdata[(2 * si) + (2 * sj)];
	return res;
}

template<class ftype, int si, int sj>
inline void SetMtxStrided(const Matrix33_tpl<ftype>& mtx, ftype* pdata)
{
	pdata[(0 * si) + (0 * sj)] = mtx(0, 0);
	pdata[(0 * si) + (1 * sj)] = mtx(0, 1);
	pdata[(0 * si) + (2 * sj)] = mtx(0, 2);
	pdata[(1 * si) + (0 * sj)] = mtx(1, 0);
	pdata[(1 * si) + (1 * sj)] = mtx(1, 1);
	pdata[(1 * si) + (2 * sj)] = mtx(1, 2);
	pdata[(2 * si) + (0 * sj)] = mtx(2, 0);
	pdata[(2 * si) + (1 * sj)] = mtx(2, 1);
	pdata[(2 * si) + (2 * sj)] = mtx(2, 2);
}

template<class ftype>
inline Matrix33_tpl<ftype> GetMtxFromBasis(const Vec3_tpl<ftype>* pBasis)
{
	Matrix33_tpl<ftype> res;
	res(0, 0) = pBasis[0].x;
	res(0, 1) = pBasis[0].y;
	res(0, 2) = pBasis[0].z;
	res(1, 0) = pBasis[1].x;
	res(1, 1) = pBasis[1].y;
	res(1, 2) = pBasis[1].z;
	res(2, 0) = pBasis[2].x;
	res(2, 1) = pBasis[2].y;
	res(2, 2) = pBasis[2].z;
	return res;
}

template<class ftype>
inline Matrix33_tpl<ftype> GetMtxFromBasisT(const Vec3_tpl<ftype>* pBasis)
{
	Matrix33_tpl<ftype> res;
	res(0, 0) = pBasis[0].x;
	res(1, 0) = pBasis[0].y;
	res(2, 0) = pBasis[0].z;
	res(0, 1) = pBasis[1].x;
	res(1, 1) = pBasis[1].y;
	res(2, 1) = pBasis[1].z;
	res(0, 2) = pBasis[2].x;
	res(1, 2) = pBasis[2].y;
	res(2, 2) = pBasis[2].z;
	return res;
}

template<class ftype, class ftype1>
inline void SetBasisFromMtx(Vec3_tpl<ftype1>* pBasis, const Matrix33_tpl<ftype>& mtx)
{
	pBasis[0].x = mtx(0, 0);
	pBasis[0].y = mtx(0, 1);
	pBasis[0].z = mtx(0, 2);
	pBasis[1].x = mtx(1, 0);
	pBasis[1].y = mtx(1, 1);
	pBasis[1].z = mtx(1, 2);
	pBasis[2].x = mtx(2, 0);
	pBasis[2].y = mtx(2, 1);
	pBasis[2].z = mtx(2, 2);
}

template<class ftype, class ftype1>
inline void SetBasisTFromMtx(Vec3_tpl<ftype1>* pBasis, const Matrix33_tpl<ftype>& mtx)
{
	pBasis[0].x = mtx(0, 0);
	pBasis[0].y = mtx(1, 0);
	pBasis[0].z = mtx(2, 0);
	pBasis[1].x = mtx(0, 1);
	pBasis[1].y = mtx(1, 1);
	pBasis[1].z = mtx(2, 1);
	pBasis[2].x = mtx(0, 2);
	pBasis[2].y = mtx(1, 2);
	pBasis[2].z = mtx(2, 2);
}

template<class ftype>
inline void TransposeBasis(Vec3_tpl<ftype>* axes)
{
	ftype t;
	t = axes[0].y;
	axes[0].y = axes[1].x;
	axes[1].x = t;
	t = axes[0].z;
	axes[0].z = axes[2].x;
	axes[2].x = t;
	t = axes[1].z;
	axes[1].z = axes[2].y;
	axes[2].y = t;
}

template<class ftype>
inline void IdentityBasis(Vec3_tpl<ftype>* axes)
{
	axes[0].Set((ftype)1, (ftype)0, (ftype)0);
	axes[1].Set((ftype)0, (ftype)1, (ftype)0);
	axes[2].Set((ftype)0, (ftype)0, (ftype)1);
}

inline void LeftOffsetSpatialMatrix(matrixf& mtx, const Vec3& offset)
{
	Matrix33 A = GetMtxStrided<float, 6, 1>(mtx.data);
	Matrix33 B = GetMtxStrided<float, 6, 1>(mtx.data + 3);
	Matrix33 C = GetMtxStrided<float, 6, 1>(mtx.data + 18);
	Matrix33 D = GetMtxStrided<float, 6, 1>(mtx.data + 21);
	Matrix33 rmtx;

	crossproduct_matrix(offset, rmtx);

	SetMtxStrided<float, 6, 1>(C + rmtx * A, mtx.data + 18);
	SetMtxStrided<float, 6, 1>(D + rmtx * B, mtx.data + 21);
}

inline void RightOffsetSpatialMatrix(matrixf& mtx, const Vec3& offset)
{
	Matrix33 A = GetMtxStrided<float, 6, 1>(mtx.data), B = GetMtxStrided<float, 6, 1>(mtx.data + 3),
		 C = GetMtxStrided<float, 6, 1>(mtx.data + 18), D = GetMtxStrided<float, 6, 1>(mtx.data + 21), rmtx;
	crossproduct_matrix(offset, rmtx);
	SetMtxStrided<float, 6, 1>(A + B * rmtx, mtx.data);
	SetMtxStrided<float, 6, 1>(C + D * rmtx, mtx.data + 18);
}

template<class itype>
inline void ComputeMeshEigenBasis(strided_pointer<const Vec3> pVertices, strided_pointer<itype> pIndices, int nTris,
                                  Vec3r* eigen_axes, Vec3r& center)
{
	int i, j, k;
	Vec3r m, mean(ZERO), v[3];
	real s, t, sum = 0, mtxbuf[9];
	matrix C(3, 3, mtx_symmetric, mtxbuf);
	C.zero();

	// find mean point
	for (i = 0; i < nTris * 3; i += 3)
	{
		v[0] = pVertices[pIndices[i + 0]];
		v[1] = pVertices[pIndices[i + 1]];
		v[2] = pVertices[pIndices[i + 2]];
		s = (v[1] - v[0] ^ v[2] - v[0]).len() * (real)0.5;
		mean += (v[0] + v[1] + v[2]) * real(1.0 / 3) * s;
		sum += s;
	}
	if (sum == 0)
	{
		center = nTris > 0 ? pVertices[pIndices[0]] : Vec3(ZERO);
		IdentityBasis(eigen_axes);
		return;
	}
	mean /= sum;

	// calculate covariance matrix
	for (i = 0; i < nTris * 3; i += 3)
	{
		v[0] = pVertices[pIndices[i + 0]] - mean;
		v[1] = pVertices[pIndices[i + 1]] - mean;
		v[2] = pVertices[pIndices[i + 2]] - mean;
		s = (v[1] - v[0] ^ v[2] - v[0]).len() * (real)0.5;
		m = v[0] + v[1] + v[2];
		for (j = 0; j < 3; j++)
		{
			for (k = j; k < 3; k++)
			{
				t = s * real(1.0 / 12) *
				    (m[j] * m[k] + v[0][j] * v[0][k] + v[1][j] * v[1][k] + v[2][j] * v[2][k]);
				C[j][k] += t;
				if (k != j)
				{
					C[k][j] += t;
				}
			}
		}
	}

	// find eigenvectors of covariance matrix (normalized)
	real eval[3];
	matrix eigenBasis(3, 3, 0, static_cast<double*>(eigen_axes[0]));
	C.jacobi_transformation(eigenBasis, eval, 0);
	center = mean;
}

inline void SpatialTranspose(matrixf& src, matrixf& dst)
{
	int i, j;
	dst.nRows = src.nCols;
	dst.nCols = src.nRows;
	for (i = 0; i < src.nRows; i++)
	{
		for (j = 0; j < 3; j++)
		{
			dst[j][i] = src[i][j + 3];
			dst[j + 3][i] = src[i][j];
		}
	}
}

template<class ftype>
inline Vec3_tpl<ftype> cross_with_ort(const Vec3_tpl<ftype>& vec, int iz)
{
	Vec3_tpl<ftype> res(ZERO);
	int ix = inc_mod3[iz], iy = dec_mod3[iz];
	res[iz] = 0;
	res[ix] = vec[iy];
	res[iy] = -vec[ix];
	return res;
}

template<class dtype>
inline dtype* ReallocateList(dtype*& plist, int szold, int sznew, bool bZero = false)
{
	dtype* newlist = new dtype[sznew];
	if (bZero)
	{
		memset(newlist + szold, 0, sizeof(dtype) * max(0, sznew - szold));
	}
	memcpy(newlist, plist, min(szold, sznew) * sizeof(dtype));
	if (plist)
	{
		delete[] plist;
	}
	plist = newlist;
	return plist;
}

inline void get_xqs_from_matrices(Matrix34* pMtx3x4, Matrix33* pMtx3x3, Vec3& pos, quaternionf& q, float& scale)
{
	if (pMtx3x4)
	{
		scale = sqrt_tpl(sqr((*pMtx3x4)(0, 0)) + sqr((*pMtx3x4)(0, 1)) + sqr((*pMtx3x4)(0, 2)));
		q = quaternionf(Matrix33(*pMtx3x4) / scale);
		pos = pMtx3x4->GetTranslation();
	}
	else if (pMtx3x3)
	{
		scale = pMtx3x3->GetRow(0).len();
		q = quaternionf(*pMtx3x3 / scale);
	}
}

int GetProjCubePlane(const Vec3& pt);

void RasterizePolygonIntoCubemap(const Vec3* pt, int npt, int iPass, int* pGrid[6], int nRes, float rmin, float rmax,
                                 float zscale);

int get_cubemap_cell_buddy(int idCell, int iBuddy, int nRes);

void GrowAndCompareCubemaps(int* pGridOcc[6], int* pGrid[6], int nRes, int nGrow, int& nCells, int& nOccludedCells);

int crop_polygon_with_plane(const Vec3* ptsrc, int nsrc, Vec3* ptdst, const Vec3& n, float d);

void CalcMediumResistance(const Vec3* ptsrc, int npt, const Vec3& n, const primitives::plane& waterPlane,
                          const Vec3& vworld, const Vec3& wworld, const Vec3& com, Vec3& P, Vec3& L);

int CoverPolygonWithCircles(strided_pointer<vector2df> pt, int npt, bool bConsecutive, const vector2df& center,
                            vector2df*& centers, float*& radii, float minCircleRadius);

int ChoosePrimitiveForMesh(strided_pointer<const Vec3> pVertices, strided_pointer<const unsigned short> pIndices,
                           int nTris, const Vec3r* eigen_axes, const Vec3r& center, int flags, float tolerance,
                           primitives::primitive*& pprim);

void ExtrudeBox(const primitives::box* pbox, const Vec3& dir, float step, primitives::box* pextbox);

inline int g_nTriangulationErrors;

int TriangulatePoly(vector2df* pVtx, int nVtx, int* pTris, int szTriBuf);

template<class CellChecker>
inline int DrawRayOnGrid(primitives::grid* pgrid, Vec3& origin, Vec3& dir, CellChecker& cell_checker)
{
	int i, ishort, ilong, bStep, bPrevStep, ilastcell;
	float dshort, dlong, frac;
	quotientf tx[2], ty[2], t[2];
	vector2di istep, icell, idirsgn;

	// crop ray with grid bounds
	idirsgn.set(sgnnz(dir.x), sgnnz(dir.y));
	i = idirsgn.x;
	tx[(1 - i) >> 1].set(-origin.x * i, dir.x * i);
	tx[(1 + i) >> 1].set((pgrid->size.x * pgrid->step.x - origin.x) * i, dir.x * i);
	i = idirsgn.y;
	ty[(1 - i) >> 1].set(-origin.y * i, dir.y * i);
	ty[(1 + i) >> 1].set((pgrid->size.y * pgrid->step.y - origin.y) * i, dir.y * i);
	t[0] = max(t[0].set(0, 1), max(tx[0], ty[0]));
	t[1] = min(t[1].set(1, 1), min(tx[1], ty[1]));
	if (t[0] >= t[1])
	{
		return 0;
	}
	if (t[0] > 0)
	{
		origin += dir * t[0].val();
	}
	if (t[0] > 0 || t[1] < 1)
	{
		dir *= (t[1] - t[0]).val();
	}

	ilong = isneg((fabs_tpl(dir.x) * pgrid->stepr.x) - (fabs_tpl(dir.y) * pgrid->stepr.y));
	ishort = ilong ^ 1;
	dshort = fabs_tpl(dir[ishort]) * pgrid->stepr[ishort];
	dlong = fabs_tpl(dir[ilong]) * pgrid->stepr[ilong];
	istep[ilong] = idirsgn[ilong];
	istep[ishort] = idirsgn[ishort];

	icell.set(physics_float2int((origin.x * pgrid->stepr.x) - 0.5f),
	          physics_float2int((origin.y * pgrid->stepr.y) - 0.5f));
	icell.x = min(pgrid->size.x - 1, max(0, icell.x));
	icell.y = min(pgrid->size.y - 1, max(0, icell.y));
	ilastcell = min(pgrid->size.y - 1, max(0, physics_float2int(((origin.y + dir.y) * pgrid->stepr.y) - 0.5f)))
	                << 16 |
	            min(pgrid->size.x - 1, max(0, physics_float2int(((origin.x + dir.x) * pgrid->stepr.x) - 0.5f)));
	if (cell_checker.check_cell(icell, ilastcell) || (icell.y << 16 | icell.x) == ilastcell)
	{
		return 1;
	}
	if (fabs_tpl(dir[ilong]) * pgrid->stepr[ilong] < 0.0001f)
	{
		return 0;
	}

	t[0].set(((icell[ilong] + ((istep[ilong] + 1) >> 1)) * pgrid->step[ilong]) - origin[ilong], dir[ilong]);
	frac = ((origin[ishort] + dir[ishort] * t[0].val()) * pgrid->stepr[ishort]) - icell[ishort];
	frac = ((1 - idirsgn[ishort]) >> 1) + (frac * idirsgn[ishort]);
	if (frac > 1.0f)
	{
		icell[ishort] += istep[ishort];
		if (cell_checker.check_cell(icell, ilastcell) || (icell.y << 16 | icell.x) == ilastcell)
		{
			return 1;
		}
		frac -= 1;
	}
	frac *= dlong;
	icell[ilong] += istep[ilong];

	bPrevStep = 0;
	do
	{
		if (cell_checker.check_cell(icell, ilastcell) || (icell.y << 16 | icell.x) == ilastcell)
		{
			return 1;
		}
		frac += dshort * (bPrevStep ^ 1);
		bStep = isneg(dlong - frac);
		icell[ishort] += bStep * istep[ishort];
		frac -= dlong * bStep;
		icell[ilong] += istep[ilong] & ~-bStep;
		bPrevStep = bStep;
	}
	while (true);

	return 0;
}

struct jgrid_checker
{
	primitives::grid* pgrid;
	char* pCellMask;
	vector2df org, dir, dirn, *ppt, *pnorms;
	int iedge[2], iedgeExit0;
	int iprevcell;
	int bMarkCenters;

	int check_cell(const vector2di& icell, int& ilastcell);

	int* pqueue;
	int ihead, itail, nQueuedCells;
	int szQueue;

	void MarkCellInterior(int i);
	void MarkCellInteriorQueue(int icell);
};

template<class T>
inline T* align16(T* ptr)
{
	return (T*)(((INT_PTR)ptr - 1 & ~15) + 16);
}

inline bool is_valid(float op)
{
	return op * op >= 0 && op * op < 1E30f;
}

inline bool is_valid(int op)
{
	return true;
}

inline bool is_valid(unsigned int op)
{
	return true;
}

inline bool is_valid(const Quat& op)
{
	return is_valid(op | op);
}

template<class dtype>
inline bool is_valid(const dtype& op)
{
	return is_valid(op.x * op.x + op.y * op.y + op.z * op.z);
}

constexpr int SINCOSTABSZ = 1024;
constexpr int SINCOSTABSZ_LOG2 = 10;

extern const std::array<float, SINCOSTABSZ> g_costab;
extern const std::array<float, SINCOSTABSZ> g_sintab;

extern const std::array<int, 256> g_bitcount;

inline int check_mask(unsigned int* pMask, int i)
{
	return pMask[i >> 5] >> (i & 31) & 1;
}

inline void set_mask(unsigned int* pMask, int i)
{
	pMask[i >> 5] |= 1u << (i & 31);
}

inline void clear_mask(unsigned int* pMask, int i)
{
	pMask[i >> 5] &= ~(1u << (i & 31));
}

inline void SpinLock(volatile int* pLock, int checkVal, int setVal)
{
	CrySpinLock(pLock, checkVal, setVal);
}

inline void AtomicAdd(volatile int* pVal, int iAdd)
{
	CryInterlockedAdd(pVal, iAdd);
}

inline void AtomicAdd(volatile unsigned int* pVal, int iAdd)
{
	CryInterlockedAdd((volatile int*)pVal, iAdd);
}

constexpr int mesh_shared_topology = 0x1000000;
constexpr int pef_use_geom_callbacks = 0x20000000;

enum geom_flags_aux
{
	geom_car_wheel = 0x40000000,
	geom_invalid = 0x20000000,
	geom_removed = 0x10000000,
	geom_will_be_destroyed = 0x8000000,
	geom_constraint_on_break = geom_proxy,
};

const char* numbered_tag(const char* s, unsigned int num);

inline intptr_t iszero_mask(void* ptr)
{
	return ((intptr_t)ptr >> ((sizeof(intptr_t) * 8) - 1)) ^ (((intptr_t)ptr - 1) >> ((sizeof(intptr_t) * 8) - 1));
}

template<class ftype>
inline int AABB_overlap(Vec3_tpl<ftype>* BBox0, Vec3_tpl<ftype>* BBox1)
{
	return isneg(fabs_tpl(BBox0[0].x + BBox0[1].x - BBox1[0].x - BBox1[1].x) - (BBox0[1].x - BBox0[0].x) -
	             (BBox1[1].x - BBox1[0].x)) &
	       isneg(fabs_tpl(BBox0[0].y + BBox0[1].y - BBox1[0].y - BBox1[1].y) - (BBox0[1].y - BBox0[0].y) -
	             (BBox1[1].y - BBox1[0].y)) &
	       isneg(fabs_tpl(BBox0[0].z + BBox0[1].z - BBox1[0].z - BBox1[1].z) - (BBox0[1].z - BBox0[0].z) -
	             (BBox1[1].z - BBox1[0].z));
}

template<class ftype>
inline int PtInAABB(Vec3_tpl<ftype>* BBox0, const Vec3_tpl<ftype>& pt)
{
	return isneg(fabs_tpl(BBox0[0].x + BBox0[1].x - pt.x * 2) - (BBox0[1].x - BBox0[0].x)) &
	       isneg(fabs_tpl(BBox0[0].y + BBox0[1].y - pt.y * 2) - (BBox0[1].y - BBox0[0].y)) &
	       isneg(fabs_tpl(BBox0[0].z + BBox0[1].z - pt.z * 2) - (BBox0[1].z - BBox0[0].z));
}
