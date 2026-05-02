#include <bit>    // std::bit_cast
#include <cstdio> // std::snprintf

#include "CryCommon/CryCore/MTPseudoRandom.h"

#include "Qhull.h"
#include "Utils.h"

const std::array<float, SINCOSTABSZ> g_costab = []() {
	std::array<float, SINCOSTABSZ> costab{};

	for (int i = 0; i < SINCOSTABSZ; i++)
	{
		costab[i] = cosf(i * (gf_PI * 0.5f / SINCOSTABSZ));
	}

	return costab;
}();

const std::array<float, SINCOSTABSZ> g_sintab = []() {
	std::array<float, SINCOSTABSZ> sintab{};

	for (int i = 0; i < SINCOSTABSZ; i++)
	{
		sintab[i] = sinf(i * (gf_PI * 0.5f / SINCOSTABSZ));
	}

	return sintab;
}();

const std::array<int, 256> g_bitcount = []() {
	std::array<int, 256> bitcount{};

	for (int i = 0; i < 256; i++)
	{
		for (int j = bitcount[i] = 0; j < 8; j++)
		{
			bitcount[i] += (i >> j) & 1;
		}
	}

	return bitcount;
}();

int physics_float2int(float x)
{
	constexpr float fmag = 1.5f * (1 << 23);
	constexpr int imag = (23 + 127) << 23 | 1 << 22;

	return std::bit_cast<int>(x + fmag) - imag;
}

unsigned int physics_rand()
{
	return g_random_generator.Generate() & RAND_MAX;
}

unsigned int physics_rand(int range)
{
	return g_random_generator.Generate() % range;
}

float physics_frand(float range)
{
	return g_random_generator.GenerateFloat() * range;
}

float physics_frand(float start, float end)
{
	return physics_frand(end - start) + start;
}

void compute_projection_integrals(Vec3r* ab, real pi[10])
{
	real a0[4], b0[4], a1[3], b1[3], C[4][3], da, db;
	int i, edge;
	for (i = 0; i < 10; i++)
	{
		pi[i] = 0;
	}

	for (edge = 0; edge < 3; edge++, ab++)
	{
		for (i = 1, a0[0] = ab[0].x; i < 4; i++)
		{
			a0[i] = a0[i - 1] * ab[0].x;
		}
		for (i = 1, b0[0] = ab[0].y; i < 4; i++)
		{
			b0[i] = b0[i - 1] * ab[0].y;
		}
		for (i = 1, a1[0] = ab[1].x; i < 3; i++)
		{
			a1[i] = a1[i - 1] * ab[1].x;
		}
		for (i = 1, b1[0] = ab[1].y; i < 3; i++)
		{
			b1[i] = b1[i - 1] * ab[1].y;
		}
		for (i = 1, C[0][0] = a1[1] + (a1[0] * a0[0]) + a0[1]; i < 3; i++)
		{
			C[0][i] = (a1[0] * C[0][i - 1]) + a0[i + 1];
		}
		for (i = 1, C[1][0] = b1[1] + (b1[0] * b0[0]) + b0[1]; i < 3; i++)
		{
			C[1][i] = (b1[0] * C[1][i - 1]) + b0[i + 1];
		}
		da = a1[0] - a0[0];
		db = b1[0] - b0[0];
		C[2][0] = (3 * a1[1]) + (2 * a1[0] * a0[0]) + a0[1];
		C[2][1] = (a0[0] * C[2][0]) + (4 * a1[2]);
		C[2][2] = (4 * b1[2]) + (3 * b1[1] * b0[0]) + (2 * b1[0] * b0[1]) + b0[2];
		C[3][0] = (3 * a0[1]) + (2 * a0[0] * a1[0]) + a1[1];
		C[3][1] = (a1[0] * C[3][0]) + (4 * a0[2]);
		C[3][2] = (4 * b0[2]) + (3 * b0[1] * b1[0]) + (2 * b0[0] * b1[1]) + b1[2];
		pi[0] += db * (a0[0] + a1[0]);
		for (i = 0; i < 3; i++)
		{
			pi[i + 1] += db * C[0][i];
			pi[i + 4] += da * C[1][i];
		}
		pi[7] += db * (b1[0] * C[2][0] + b0[0] * C[3][0]);
		pi[8] += db * (b1[0] * C[2][1] + b0[0] * C[3][1]);
		pi[9] += da * (a1[0] * C[2][2] + a0[0] * C[3][2]);
	}

	pi[0] *= 0.5;
	pi[1] *= 1.0 / 6;
	pi[2] *= 1.0 / 12;
	pi[3] *= 1.0 / 20;
	pi[4] *= -1.0 / 6;
	pi[5] *= -1.0 / 12;
	pi[6] *= -1.0 / 20;
	pi[7] *= 1.0 / 24;
	pi[8] *= 1.0 / 60;
	pi[9] *= -1.0 / 60;
}

void compute_face_integrals(Vec3r* p, Vec3r n, real fi[12])
{
	real pi[10], k[4], t, w;
	int i;

	compute_projection_integrals(p, pi);

	w = -(n * p[0]);
	for (k[0] = 1.0 / n.z, i = 1; i < 4; i++)
	{
		k[i] = k[i - 1] * k[0];
	}

	for (i = 0; i < 3; i++)
	{
		fi[(i * 3) + 0] = k[0] * pi[i + 1]; // a, a2, a3
	}
	for (i = 0; i < 3; i++)
	{
		fi[(i * 3) + 1] = k[0] * pi[i + 4]; // b, b2, b3
	}

	// a2b, b2g, g2a
	fi[9] = k[0] * pi[8];
	fi[10] = -k[1] * (n.x * pi[9] + n.y * pi[6] + w * pi[5]);
	fi[11] = k[2] * (n.x * n.x * pi[3] + n.y * n.y * pi[9] + w * w * pi[1] +
	                 2 * (n.x * n.y * pi[8] + n.x * w * pi[2] + n.y * w * pi[7]));

	for (i = 0, t = n.x; i < 3; i++, t *= n.x)
	{
		pi[i + 1] *= t;
	}
	for (i = 0, t = n.y; i < 3; i++, t *= n.y)
	{
		pi[i + 4] *= t;
	}
	for (i = 0; i < 3; i++)
	{
		pi[i + 7] *= n.x * n.y;
	}
	pi[8] *= n.x;
	pi[9] *= n.y;

	// g, g2, g3
	fi[2] = -k[1] * (pi[1] + pi[4] + w * pi[0]);
	fi[5] = k[2] * (pi[2] + 2 * pi[7] + pi[5] + w * (2 * (pi[1] + pi[4]) + w * pi[0]));
	fi[8] = -k[3] * (pi[3] + 3 * (pi[8] + pi[9]) + pi[6] +
	                 w * (3 * (pi[2] + pi[5] + 2 * pi[7] + w * (pi[1] + pi[4])) + w * w * pi[0]));
}

real ComputeMassProperties(strided_pointer<const Vec3> points, const index_t* faces, int nfaces, Vec3r& center,
                           Matrix33r& I)
{
	real M = 0, fi[12], nmax, diag[3] = {0, 0, 0};
	Vec3r n, p[4];
	int i, g;
	center.zero();
	I.SetZero();

	for (nfaces--; nfaces >= 0; nfaces--, faces += 3)
	{
		n = points[faces[1]] - points[faces[0]] ^ points[faces[2]] - points[faces[0]];
		if (n.len2() == 0)
		{
			continue;
		}
		n.normalize();

		for (i = 0, nmax = -1; i < 3; i++)
		{
			if (n[i] * n[i] > nmax)
			{
				nmax = n[i] * n[i];
				g = i;
			}
		}
		for (i = 0; i < 3; i++)
		{
			p[i] = points[faces[i]].GetPermutated(g);
		}
		p[3] = p[0];
		n = n.GetPermutated(g);

		compute_face_integrals(p, n, fi);
		g = g ^ g >> 1 ^ 1;
		for (i = 0; i < 4; i++)
		{
			((Vec3r*)fi)[i] = ((Vec3r*)fi)[i].GetPermutated(g);
		}
		n = n.GetPermutated(g);

		M += n.x * fi[0];
		for (i = 0; i < 3; i++)
		{
			center[i] += n[i] * fi[i + 3];
			diag[i] += n[i] * fi[i + 6];
		}
		I(0, 1) += n.x * fi[9];
		I(1, 2) += n.y * fi[10];
		I(0, 2) += n.z * fi[11];
	}

	if (M > 0)
	{
		center /= M * 2;
	}
	I(0, 0) = ((diag[1] + diag[2]) * (1.0 / 3)) - (M * (center.y * center.y + center.z * center.z));
	I(1, 1) = ((diag[0] + diag[2]) * (1.0 / 3)) - (M * (center.x * center.x + center.z * center.z));
	I(2, 2) = ((diag[0] + diag[1]) * (1.0 / 3)) - (M * (center.x * center.x + center.y * center.y));
	I(1, 0) = I(0, 1) = (-I(0, 1) * 0.5) + (M * center.x * center.y);
	I(2, 0) = I(0, 2) = (-I(0, 2) * 0.5) + (M * center.x * center.z);
	I(2, 1) = I(1, 2) = (-I(1, 2) * 0.5) + (M * center.y * center.z);
	return M;
}

namespace {

struct inters2d
{
	vector2df pt;
	int iedge[2];
};

vector2df g_BoolPtBuf[4096];
int g_BoolIdBuf[4096];
int g_BoolGrid[4096];
unsigned int g_BoolHash[8192];
inters2d g_BoolInters[256];

int line_seg_inters(const vector2df* seg0, const vector2df* seg1, vector2df* ptres)
{
	float denom, t0, t1, sg;
	vector2df dp0, dp1, ds;
	dp0 = seg0[1] - seg0[0];
	dp1 = seg1[1] - seg1[0];
	ds = seg1[0] - seg0[0];
	denom = dp0 ^ dp1;

	if (sqr(denom) > 1E-6f * len2(dp0) * len2(dp1))
	{
		t0 = ds ^ dp1;
		t1 = ds ^ dp0;
		sg = sgnnz(denom);
		denom *= sg;
		t0 *= sg;
		t1 *= sg;
		if (isneg(fabs_tpl((t0 * 2) - denom) - denom) & isneg(fabs_tpl((t1 * 2) - denom) - denom))
		{
			ptres[0] = seg0[0] + dp0 * (t0 / denom);
			return 1;
		}
		else
		{
			return 0;
		}
	}

	if (sqr(ds ^ dp0) < 1E-6f * len2(ds) * len2(dp0))
	{
		float t[2][2];
		const vector2df* ppt[2][2];
		int idir;
		t[0][0] = 0;
		t[0][1] = len2(dp0);
		ppt[0][0] = seg0;
		ppt[0][1] = seg0 + 1;
		t0 = (seg1[0] - seg0[0]) * dp0;
		t1 = (seg1[1] - seg0[0]) * dp0;
		idir = isneg(t1 - t0);
		t[1][idir] = t0;
		t[1][idir ^ 1] = t0;
		ppt[1][idir] = seg1;
		ppt[1][idir ^ 1] = seg1 + 1;
		if (max(t[0][0], t[1][0]) > min(t[0][1], t[1][1]))
		{
			return 0;
		}
		ptres[0] = *ppt[0][isneg(t[0][0] - t[1][0])];
		ptres[1] = *ppt[1][isneg(t[1][1] - t[0][1])];
		return 2;
	}

	return 0;
}

vector2di& get_cell(const vector2df& pt, const vector2df& rstep, vector2di& ipt)
{
	ipt.set(physics_float2int((pt.x * rstep.x) - 0.5f), physics_float2int((pt.y * rstep.y) - 0.5f));
	return ipt;
}

void get_rect(const vector2di& ipt0, const vector2di& ipt1, vector2di* irect, const vector2di& isz)
{
	irect[0].x = max(0, min(ipt0.x, ipt1.x));
	irect[0].y = min(isz.y, max(0, min(ipt0.y, ipt1.y)));
	irect[1].x = min(isz.x - 1, max(ipt0.x, ipt1.x));
	irect[1].y = min(isz.y, max(ipt0.y, ipt1.y));
}

int check_if_inside(int iobj, vector2di& ipt, const vector2di& isz, const vector2df* ptsrc, const vector2df& pt)
{
	int bInside, i, bStop = 0;
	vector2df pt0, pt1, dp;
	quotientf ycur;
	if ((unsigned int)ipt.x >= (unsigned int)isz.x || ipt.y < 0)
	{
		return 0;
	}

	for (bInside = 0; ipt.y <= isz.y && !bStop; ipt.y++)
	{
		for (i = g_BoolGrid[(ipt.y * isz.x) + ipt.x]; i < g_BoolGrid[(ipt.y * isz.x) + ipt.x + 1]; i++)
		{
			if (g_BoolHash[i] >> 31 ^ iobj)
			{
				pt0 = ptsrc[g_BoolHash[i] & 0x7FFFFFFF];
				pt1 = ptsrc[(g_BoolHash[i] & 0x7FFFFFFF) + 1];
				dp = pt1 - pt0;
				ycur.set((dp ^ pt0) + (pt.x * dp.y), dp.x).fixsign();
				if (isneg(fabs_tpl((pt0.x + pt1.x) - (pt.x * 2)) - fabs_tpl(pt0.x - pt1.x)) &
				    isneg(pt.y - ycur))
				{
					bInside -= sgn(dp.x);
					bStop = 1;
				}
			}
		}
	}

	return isneg(-bInside);
}

} // unnamed namespace

int boolean2d(booltype type, vector2df* ptbuf1, int npt1, vector2df* ptbuf2, int npt2, int bClosed, vector2df*& ptres,
              int*& pidres)
{
	vector2df* ptsrc[2] = {ptbuf1, ptbuf2};
	int npt[2] = {npt1, npt2};
	vector2df dp, dp1, ptmin[2], ptmax[2], sz, ptbox[2], ptint[2], pttest, rstep;
	int iobj, i, j, idx, nsz, n, npttmp, nptres, ninters = 0, istart, ix, iy, bInside, bPrevInside, idx_next,
						     idx_prev, inext, inext1, iobj1;
	vector2di ipt0, ipt1, irect[2], isz;
	float t, ratioxy, ratioyx;

	pidres = g_BoolIdBuf;
	if (npt1 < 3)
	{
		ptres = ptbuf2;
		return npt2 & -(bClosed);
	}
	if (npt2 < 2 + bClosed)
	{
		ptres = ptbuf1;
		return npt1 & -(bClosed);
	}

	for (iobj = 0; iobj < 2; iobj++)
	{
		ptmin[iobj] = ptmax[iobj] = ptsrc[iobj][0];
		for (i = 1; i < npt[iobj]; i++)
		{
			ptmin[iobj].x = min(ptmin[iobj].x, ptsrc[iobj][i].x);
			ptmin[iobj].y = min(ptmin[iobj].y, ptsrc[iobj][i].y);
			ptmax[iobj].x = max(ptmax[iobj].x, ptsrc[iobj][i].x);
			ptmax[iobj].y = max(ptmax[iobj].y, ptsrc[iobj][i].y);
		}
	}
	ptbox[0].x = max(ptmin[0].x, ptmin[1].x);
	ptbox[0].y = max(ptmin[0].y, ptmin[1].y);
	ptbox[1].x = min(ptmax[0].x, ptmax[1].x);
	ptbox[1].y = min(ptmax[0].y, ptmax[1].y);
	sz = ptbox[1] - ptbox[0];
	sz.x += fabs_tpl(sz.y) * 1E-5f;
	sz.y += fabs_tpl(sz.x) * 1E-5f;
	if (sz.x <= fabs_tpl(sz.y) * 5E-6f || sz.y <= fabs_tpl(sz.x) * 5E-6f)
	{
		return 0;
	}
	ptbox[0] -= sz * 0.01f;
	ptbox[1] += sz * 0.01f;
	sz = ptbox[1] - ptbox[0];
	sz.x += fabs_tpl(sz.y) * 1E-5f;
	sz.y += fabs_tpl(sz.x) * 1E-5f;

	i = (int)((sizeof(g_BoolGrid) / sizeof(g_BoolGrid[0])) - 1);
	npttmp = min(npt[0] + npt[1] << 1, i);
	ratioyx = max(min(sz.y / sz.x, (float)npttmp), 1.0f);
	ratioxy = max(min(sz.x / sz.y, (float)npttmp), 1.0f);
	isz.y = max(1, physics_float2int((cry_sqrtf((npttmp * 4 * ratioyx) + 1) * 0.5f) - 1.0f));
	isz.x = max(1, physics_float2int((isz.y * ratioxy) - 0.5f));
	if (isz.x * (isz.y + 1) > i)
	{
		isz.y = min(isz.y, i);
		isz.x = i / (isz.y + 1);
	}
	rstep.set(isz.x / sz.x, isz.y / sz.y);
	nsz = isz.x * (isz.y + 1);
	if (nsz >= (sizeof(g_BoolGrid) / sizeof(g_BoolGrid[0])) - 1)
	{
		return 0;
	}
	npt[1] -= bClosed ^ 1;

	for (i = 0; i <= nsz; i++)
	{
		g_BoolGrid[i] = 0;
	}

	for (iobj = 0; iobj < 2; iobj++)
	{ // calculate number of elements in each cell
		for (get_cell(ptsrc[iobj][i = 0] - ptbox[0], rstep, ipt0); i < npt[iobj]; i++)
		{
			get_cell(ptsrc[iobj][i + 1] - ptbox[0], rstep, ipt1);
			get_rect(ipt0, ipt1, irect, isz);
			for (ix = irect[0].x; ix <= irect[1].x; ix++)
			{
				for (iy = irect[0].y; iy <= irect[1].y; iy++)
				{
					g_BoolGrid[(iy * isz.x) + ix]++;
				}
			}
			ipt0 = ipt1;
		}
	}

	for (i = 1; i <= nsz; i++)
	{
		g_BoolGrid[i] += g_BoolGrid[i - 1];
	}
	if (g_BoolGrid[nsz - 1] > (int)(sizeof(g_BoolHash) / sizeof(g_BoolHash[0])))
	{
		return 0;
	}

	for (iobj = 0; iobj < 2; iobj++)
	{ // put each line segment into the corresponding hash cell(s)
		for (get_cell(ptsrc[iobj][i = 0] - ptbox[0], rstep, ipt0); i < npt[iobj]; i++)
		{
			get_cell(ptsrc[iobj][i + 1] - ptbox[0], rstep, ipt1);
			get_rect(ipt0, ipt1, irect, isz);
			for (ix = irect[0].x; ix <= irect[1].x; ix++)
			{
				for (iy = irect[0].y; iy <= irect[1].y; iy++)
				{
					g_BoolHash[--g_BoolGrid[(iy * isz.x) + ix]] = iobj << 31 | i;
				}
			}
			ipt0 = ipt1;
		}
	}

	// select iobj which is likely to have shorter edges
	if (bClosed)
	{
		iobj = isneg(((ptmax[1].x - ptmin[1].x + ptmax[1].y - ptmin[1].y) * npt[0]) -
		             ((ptmax[0].x - ptmin[0].x + ptmax[0].y - ptmin[0].y) * npt[1]));
	}
	else
	{
		iobj = 1;
	}

	// build list of intersections by traversing iobj
	if (!bClosed)
	{
		g_BoolInters[0].pt = ptsrc[1][0];
		g_BoolInters[0].iedge[0] = -1;
		g_BoolInters[0].iedge[1] = 0;
		ninters = 1;
	}
	for (get_cell(ptsrc[iobj][0] - ptbox[0], rstep, ipt0), i = 0; i < npt[iobj]; i++)
	{
		get_cell(ptsrc[iobj][i + 1] - ptbox[0], rstep, ipt1);
		get_rect(ipt0, ipt1, irect, isz);
		istart = ninters;

		for (ix = irect[0].x; ix <= irect[1].x; ix++)
		{
			for (iy = irect[0].y; iy <= irect[1].y; iy++)
			{
				for (j = g_BoolGrid[(iy * isz.x) + ix]; j < g_BoolGrid[(iy * isz.x) + ix + 1]; j++)
				{
					if (g_BoolHash[j] >> 31 ^ iobj)
					{
						for (n = line_seg_inters(ptsrc[iobj] + i,
						                         ptsrc[iobj ^ 1] + (g_BoolHash[j] & 0x7FFFFFFF),
						                         ptint) -
						         1;
						     n >= 0; n--)
						{
							dp = ptsrc[iobj][i + 1] - ptsrc[iobj][i];
							t = (ptint[n] - ptsrc[iobj][i]) * dp;
							for (idx = istart;
							     idx < ninters &&
							     fabs_tpl((g_BoolInters[idx].pt - ptsrc[iobj][i]) * dp -
							              t) > t * 1E-7f;
							     idx++)
								;
							if (idx < ninters)
							{
								continue; // ignore possible intersections with the same
								          // line found in different cell
							}
							if (ninters ==
							    (sizeof(g_BoolInters) / sizeof(g_BoolInters[0])) - 1)
							{
								return 0;
							}
							for (idx = ninters - 1;
							     idx >= istart &&
							     (g_BoolInters[idx].pt - ptsrc[iobj][i]) * dp > t;
							     idx--)
							{
								g_BoolInters[idx + 1] = g_BoolInters[idx];
							}
							g_BoolInters[idx + 1].pt = ptint[n];
							g_BoolInters[idx + 1].iedge[iobj] = i;
							g_BoolInters[idx + 1].iedge[iobj ^ 1] =
							    g_BoolHash[j] & 0x7FFFFFFF;
							ninters++;
						}
					}
				}
			}
		}
		ipt0 = ipt1;
	}
	if (!bClosed)
	{
		if (ninters == (sizeof(g_BoolInters) / sizeof(g_BoolInters[0])) - 1)
		{
			return 0;
		}
		g_BoolInters[ninters].pt = ptsrc[1][npt[1]];
		g_BoolInters[ninters].iedge[0] = -1;
		g_BoolInters[ninters].iedge[1] = npt[1];
		g_BoolInters[ninters + 1] = g_BoolInters[ninters];
		ninters++;
	}
	else
	{
		g_BoolInters[ninters] = g_BoolInters[0];
	}

	nptres = 0;
	ptres = g_BoolPtBuf;
	// if there were no intersections, return the object that is inside the other
	if (ninters - ((bClosed ^ 1) * 2) == 0)
	{
		get_cell(ptsrc[iobj][0] - ptbox[0], rstep, ipt0);
		bInside = check_if_inside(iobj, ipt0, isz, ptsrc[iobj ^ 1], ptsrc[iobj][0]);
		npt[1] += bClosed ^ 1;
		if (bClosed) // assume that objects cannot have empty intersection area, thus if iobj is not inside
		             // iobj^1, ibj1^1 should be inside iobj
		{
			iobj ^= bInside ^ 1;
		}
		ptres = ptsrc[iobj];
		for (nptres = 0; nptres < npt[iobj]; nptres++)
		{
			g_BoolIdBuf[nptres] = nptres + 1 << iobj * 16;
		}
		return nptres & -(bClosed | bInside);
	}
	npt[1] += bClosed ^ 1; // compensate for 1 subtracted earlier

	// build boolean intersection by at each intersection point selecting the stripe that is more 'inward' than the
	// other one
	for (idx = bPrevInside = 0; idx < ninters; idx++)
	{
		idx_next = idx + 1; //& ~(ninters-2-idx>>31);
		idx_prev = idx - 1;
		idx_prev = (idx_prev & ~(idx_prev >> 31)) | ((ninters - 1) & idx_prev >> 31);
		i = g_BoolInters[idx].iedge[iobj];
		inext = (i + 1) & i + 1 - npt[iobj] >> 31;
		dp = ptsrc[iobj][inext] - ptsrc[iobj][i];
		inext1 = min(inext + 1, npt[iobj] - 1) & (i + 1 - npt[iobj] >> 31 | ~-bClosed);

		j = g_BoolInters[idx].iedge[iobj ^ 1];
		if (j >= 0)
		{
			dp1 = ptsrc[iobj ^ 1][(j + 1) & j + 1 - npt[iobj ^ 1] >> 31] - ptsrc[iobj ^ 1][j];
			bInside = isneg(dp ^ dp1);
		}
		else
		{
			pttest = g_BoolInters[idx].pt;
			get_cell(pttest - ptbox[0], rstep, ipt0);
			bInside = check_if_inside(iobj, ipt0, isz, ptsrc[iobj ^ 1], pttest);
		}

		iobj1 = iobj ^ bInside ^ 1;
		if (bInside | bPrevInside | bClosed)
		{
			g_BoolPtBuf[nptres] = g_BoolInters[idx].pt;
			g_BoolIdBuf[nptres++] = g_BoolInters[idx].iedge[1] + 1 << 16 | (g_BoolInters[idx].iedge[0] + 1);
			if (nptres >= (int)(sizeof(g_BoolPtBuf) / sizeof(g_BoolPtBuf[0])))
			{
				return nptres;
			}
		}
		if (bInside | bClosed)
		{
			i = g_BoolInters[idx].iedge[iobj1];
			inext = (i + 1) & ~(npt[iobj1] - 2 - i >> 31);
			dp = ptsrc[iobj1][inext] - ptsrc[iobj1][i];
			bool bForceFirstStep = i == g_BoolInters[idx_next].iedge[iobj1] &&
			                       (g_BoolInters[idx].pt - ptsrc[iobj1][i]) * dp >
			                           (g_BoolInters[idx_next].pt - ptsrc[iobj1][i]) * dp;
			for (; bForceFirstStep || i != g_BoolInters[idx_next].iedge[iobj1] &&
			                              (iobj1 == iobj || i != g_BoolInters[idx_prev].iedge[iobj1]);
			     i = (i + 1) & ~(npt[iobj1] - 2 - i >> 31))
			{
				g_BoolPtBuf[nptres] = ptsrc[iobj1][i + 1];
				g_BoolIdBuf[nptres++] = i + 2 << iobj1 * 16;
				if (nptres >= (int)(sizeof(g_BoolPtBuf) / sizeof(g_BoolPtBuf[0])))
				{
					return nptres;
				}
				bForceFirstStep = false;
			}
		}
		bPrevInside = bInside;
	}

	return nptres;
}

real RotatePointToPlane(const Vec3r& pt, const Vec3r& axis, const Vec3r& center, const Vec3r& n, const Vec3r& origin)
{
	Vec3r ptc, ptz, ptx, pty;
	real kcos, ksin, k, a, b, c, d;
	ptc = pt - center;
	ptz = axis * (axis * ptc);
	ptx = ptc - ptz;
	pty = axis ^ ptx;
	kcos = ptx * n;
	ksin = pty * n;
	k = (ptz + center - origin) * n;
	a = sqr(ksin) + sqr(kcos);
	b = ksin * k;
	c = sqr(k) - sqr(kcos);
	d = (b * b) - (a * c);
	if (d >= 0)
	{
		return asin_tpl((sqrt_tpl(d) * sgnnz(b) - b) / a);
	}
	else
	{
		return 0;
	}
}

int GetProjCubePlane(const Vec3& pt)
{
	int iPlane = isneg(fabsf(pt.x) - fabsf(pt.y));
	iPlane |= isneg(fabsf(pt[iPlane]) - fabsf(pt.z)) << 1;
	iPlane &= 2 | (iPlane >> 1 ^ 1);
	return iPlane << 1 | isnonneg(pt[iPlane]);
}

void RasterizePolygonIntoCubemap(const Vec3* pt, int npt, int iPass, int* pGrid[6], int nRes, float rmin, float rmax,
                                 float zscale)
{
	int iPlane, iPlaneEnd, ipt, ipt1, i, j, iBrd[2], iBrdNext[2], idx, lxdir, iSign, ixlim[2], iylim, imask, idcell,
	    iz, irmin, iyend, iOrder, nCells, ixleft, iEnter, nPlanes, maskUsed, loopIter;
	Vec3i ic;
	vector2di icell, ibound;
	Vec3 nplane, n, rn;
	vector2df ptint[2], dp[2];
	float kres, krres, koffs, dz, dp0[2], denom;
	quotientf z;

	struct cube_plane
	{
		int iEnter, iExit;
		float minz;
		int npt;
		vector2df pt[32];
	};
	cube_plane planes[6];

	nplane = pt[1] - pt[0] ^ pt[2] - pt[0];
	if (isnonneg(nplane * pt[0]) ^ iPass)
	{
		return;
	}
	kres = 0.5f * nRes;
	krres = 2.0f / nRes;
	koffs = 1.0f - (krres * 0.5f);
	irmin = physics_float2int(rmin * zscale);
	for (i = 0; i < 6; i++)
	{
		planes[i].npt = 0;
		planes[i].iExit = -1;
		planes[i].minz = rmax;
	}
	z.x = pt[0] * nplane;

	for (i = 0; i < 3; i++)
	{
		for (ipt = 0; ipt < npt; ipt++)
		{
			planes[(i * 2) + 0].minz = min(planes[(i * 2) + 0].minz, max(0.0f, -pt[ipt][i]));
			planes[(i * 2) + 1].minz = min(planes[(i * 2) + 1].minz, max(0.0f, pt[ipt][i]));
		}
	}

	for (ipt = 0; ipt < npt; ipt++)
	{
		ipt1 = (ipt + 1) & ipt - npt + 1 >> 31;
		iPlane = GetProjCubePlane(pt[ipt]);
		iPlaneEnd = GetProjCubePlane(pt[ipt1]);
		n = pt[ipt] ^ pt[ipt1];
		rn.zero();
		ic.z = iPlane >> 1;
		denom = 1.0f /
		        (fabsf(pt[ipt][ic.z]) + isneg(fabsf(pt[ipt][ic.z]) - 1E-5f) * 1E4f); // store the starting point
		planes[iPlane].pt[planes[iPlane].npt++ & 31].set(pt[ipt][inc_mod3[ic.z]] * denom,
		                                                 pt[ipt][dec_mod3[ic.z]] * denom);
		maskUsed = 0;

		for (nPlanes = 0; iPlane != iPlaneEnd && nPlanes < 6; nPlanes++)
		{
			maskUsed |= 1 << iPlane;
			ic.z = iPlane >> 1;
			iSign = ((iPlane & 1) << 1) - 1;
			ic.x = inc_mod3[ic.z];
			ic.y = dec_mod3[ic.z];
			ibound.x = sgnnz(n[ic.y]) * iSign;
			ibound.y = -sgnnz(n[ic.x]) * iSign;
			ptint[0].x = ibound.x; // intersect line with face boundary conditions and take the intersection
			                       // that is inside face
			ptint[0].y = (-n[ic.z] * iSign) -
			             (n[ic.x] * ptint[0].x); // only check boundaries that lie farther along ccw
			                                     // movement of line around origin-edge plane normal
			ptint[1].y = ibound.y;
			ptint[1].x = (-n[ic.z] * iSign) - (n[ic.y] * ptint[1].y);
			idx = inrange(ptint[1].x, -n[ic.x], n[ic.x]);
			loopIter = 0;
		nextidx:
			if (rn[ic[idx ^ 1]] == 0)
			{
				rn[ic[idx ^ 1]] = 1.0f / (n[ic[idx ^ 1]] + isneg(fabsf(n[ic[idx ^ 1]]) - 1E-5f) * 1E4f);
			}
			j = ic[idx] << 1 | ibound[idx] + 1 >> 1;
			if (j != iPlaneEnd && maskUsed & 1 << j && ++loopIter < 8)
			{
				idx ^= 1;
				goto nextidx;
			}
			ptint[idx][idx ^ 1] *= rn[ic[idx ^ 1]];
			// store ptint[idx] in iPlane's point list
			planes[iPlane].pt[planes[iPlane].npt++ & 31] = ptint[idx];
			planes[iPlane].iExit = idx + 1 - ibound[idx];
			iPlane = j;
			iEnter = (idx ^ 1) + 1 - iSign;
			if (planes[iPlane].iExit >= 0)
			{ // add corner points between the last exit point and this enter point
				iOrder = (((iPlane & 1) << 1) - 1) * ((iPass << 1) - 1);
				j = iOrder >> 31;
				for (i = planes[iPlane].iExit; i != iEnter; i = (i + iOrder) & 3)
				{
					planes[iPlane].pt[planes[iPlane].npt++ & 31].set(1 - ((i + j ^ i + j << 1) & 2),
					                                                 1 - (i + j & 2));
				}
				planes[iPlane].iExit = -1;
			}
			else
			{
				planes[iPlane].iEnter = iEnter;
			}
			// store ptint[idx] in the new iPlane's point list (transform it to the new iPlane beforehand)
			ptint[idx][idx] = ptint[idx][idx ^ 1];
			ptint[idx][idx ^ 1] = iSign;
			planes[iPlane].pt[planes[iPlane].npt++ & 31] = ptint[idx];
			if (planes[iPlane].npt > 32)
			{
				planes[iPlane].npt = 0; // should not happen
			}
		}
	}

	for (iPlane = nCells = i = 0, ic.z = 6; iPlane < 6; iPlane++)
	{
		j = iszero(planes[iPlane].npt) ^ 1;
		nCells += j;
		j <<= iPlane >> 1; // j = plane axis bit
		ic.z -= ((iPlane >> 1) + 1) &
		        -(j & (i & j ^ j)) >>
		            31; // subtract plane axis index+1 from the sum if this axis hasn't been encountered yet
		i |= j;         // accumulate used axes mask
	}
	if (nCells == 4 && ic.z >= 1 && ic.z <= 3)
	{
		// we have 4 sides that form a 'ring', meaning one (and only one) axis is unaffected edges
		ic.z--;
		iPlane = (isneg(nplane[ic.z]) ^ iPass) | ic.z << 1;
		iOrder = (((iPlane & 1) << 1) - 1) * ((iPass << 1) - 1);
		j = iOrder >> 31;
		for (i = 0; planes[iPlane].npt < 4; i = (i + iOrder) & 3)
		{
			planes[iPlane].pt[planes[iPlane].npt++ & 31].set(1 - ((i + j ^ i + j << 1) & 2),
			                                                 1 - (i + j & 2));
		}
	}

	// rasterize resulting polygons in each plane
	for (iPlane = nCells = 0; iPlane < 6; iPlane++)
	{
		iOrder = (((iPlane & 1) << 1) - 1) * ((iPass << 1) - 1);
		j = iOrder >> 31;
		if (planes[iPlane].iExit >= 0) // close pending exits
		{
			for (i = planes[iPlane].iExit; i != planes[iPlane].iEnter; i = (i + iOrder) & 3)
			{
				planes[iPlane].pt[planes[iPlane].npt++ & 31].set(1 - ((i + j ^ i + j << 1) & 2),
				                                                 1 - (i + j & 2));
			}
		}

		if (planes[iPlane].npt && planes[iPlane].npt < 32)
		{
			ic.z = iPlane >> 1;
			ic.x = inc_mod3[ic.z];
			ic.y = dec_mod3[ic.z];
			iSign = ((iPlane & 1) * 2) - 1;
			dz = nplane[ic.x] * krres;

			for (i = 1, iBrd[0] = iBrd[1] = 0; i < planes[iPlane].npt; i++)
			{ // find the uppermost and the lowest points
				imask = -isneg(planes[iPlane].pt[iBrd[0]].y - planes[iPlane].pt[i].y);
				iBrd[0] = (iBrd[0] & ~imask) | (i & imask);
				imask = -isneg(planes[iPlane].pt[i].y - planes[iPlane].pt[iBrd[1]].y);
				iBrd[1] = (iBrd[1] & ~imask) | (i & imask);
			}
			iyend = min(nRes - 1,
			            max(0, physics_float2int(((planes[iPlane].pt[iBrd[1]].y + koffs) * kres) + 0.5f)));
			ixleft =
			    min(nRes - 1, max(0, physics_float2int((planes[iPlane].pt[iBrd[0]].x + koffs) * kres)));
			icell.y =
			    min(nRes - 1, max(0, physics_float2int((planes[iPlane].pt[iBrd[0]].y + koffs) * kres)));
			iBrd[1] = iBrd[0];

			do
			{
				iBrdNext[0] = iBrd[0] + iOrder;
				iBrdNext[0] += planes[iPlane].npt & iBrdNext[0] >> 31; // wrap -1 to npt-1 and npt to 0
				iBrdNext[0] &= iBrdNext[0] - planes[iPlane].npt >> 31;
				iBrdNext[1] = iBrd[1] - iOrder;
				iBrdNext[1] += planes[iPlane].npt & iBrdNext[1] >> 31; // wrap -1 to npt-1 and npt to 0
				iBrdNext[1] &= iBrdNext[1] - planes[iPlane].npt >> 31;
				idx = isneg(planes[iPlane].pt[iBrdNext[0]].y - planes[iPlane].pt[iBrdNext[1]].y);
				dp[0] = planes[iPlane].pt[iBrdNext[0]] - planes[iPlane].pt[iBrd[0]];
				dp0[0] = (dp[0] ^ planes[iPlane].pt[iBrd[0]] + vector2df(koffs, koffs)) * kres;
				dp[1] = planes[iPlane].pt[iBrdNext[1]] - planes[iPlane].pt[iBrd[1]];
				dp0[1] = (dp[1] ^ planes[iPlane].pt[iBrd[1]] + vector2df(koffs, koffs)) * kres;
				lxdir = sgnnz(dp[0].x);
				ixlim[0] =
				    min(nRes - 1, max(0, physics_float2int((min(planes[iPlane].pt[iBrd[0]].x,
				                                                planes[iPlane].pt[iBrdNext[0]].x) +
				                                            koffs) *
				                                           kres)));
				ixlim[1] =
				    min(nRes - 1, max(0, physics_float2int((max(planes[iPlane].pt[iBrd[1]].x,
				                                                planes[iPlane].pt[iBrdNext[1]].x) +
				                                            koffs) *
				                                           kres)));
				icell.y = max(icell.y, iyend);
				iylim = min(
				    nRes - 1,
				    max(iyend, physics_float2int((planes[iPlane].pt[iBrdNext[idx]].y + koffs) * kres)));
				do
				{
					// search left or right (dep. on sgn(dp.x)) from the left border to find the
					// leftmost filled cell left: iterate while point is inside and x!=xmin-1,
					// increment x after loop; right: while point is outside and x!=xmax
					for (icell.x = ixleft;
					     isneg(((dp[0] ^ icell) * lxdir) - (dp0[0] * lxdir)) &
					     (iszero(ixlim[lxdir + 1 >> 1] + (lxdir >> 31) - icell.x) ^ 1);
					     icell.x += lxdir)
						;
					icell.x -= lxdir >> 31;
					ixleft = icell.x;
					// search right from the leftmost cell to the end of the right border, filling
					// data
					z.y = (nplane[ic.x] * (icell.x * krres - koffs)) +
					      (nplane[ic.y] * (icell.y * krres - koffs)) + (nplane[ic.z] * iSign);
					idcell = icell.x + (icell.y * nRes);
					// 1st (front face) pass output:
					//  iz<rmin - set the highest bit
					//  else - update z value in the lower 30 bits
					// 2nd (back face) pass output:
					//  iz<rmin - clear the highest bit
					//  else - update z value in the lower 30 bits
					// after both passes:
					//  if the highest bit is set, change cell z to irmin
					if (iPass == 0)
					{
						for (; isneg((dp[1] ^ icell) - dp0[1]) & isneg(icell.x - ixlim[1] - 1);
						     icell.x++, z.y += dz, idcell++)
						{
							iz = physics_float2int(
							    max(planes[iPlane].minz, min(fabsf(z.val()), rmax)) *
							    zscale);
							nCells++;
							imask = iz - irmin >> 31;
							iz = (iz | imask) & ((1u << 31) - 1);
							pGrid[iPlane][idcell] =
							    min(pGrid[iPlane][idcell] & ((1u << 31) - 1), iz) |
							    ((pGrid[iPlane][idcell] | imask) & (1 << 31));
						}
					}
					else
					{
						for (; isneg((dp[1] ^ icell) - dp0[1]) & isneg(icell.x - ixlim[1] - 1);
						     icell.x++, z.y += dz, idcell++)
						{
							iz = physics_float2int(
							    max(planes[iPlane].minz, min(fabsf(z.val()), rmax)) *
							    zscale);
							nCells++;
							// pGrid[iPlane][idcell] &= irmin-iz>>31 | (1u<<31)-1;
							imask = iz - irmin >> 31;
							iz = (iz | imask) & ((1u << 31) - 1);
							pGrid[iPlane][idcell] =
							    min(pGrid[iPlane][idcell] & ((1u << 31) - 1), iz) |
							    ((pGrid[iPlane][idcell] & ~imask) & (1 << 31));
						}
					}
				}
				while (--icell.y >= iylim);
				iBrd[idx] = iBrdNext[idx];
				ixleft =
				    (ixleft & -idx) | (ixlim[0] & ~-idx); // update ixleft if the left branch advances
			}
			while (iylim > iyend);
		}
	}

	if (nCells == 0)
	{ // do not allow objects to take no rasterized space
		iPlane = GetProjCubePlane(pt[0]);
		ic.z = iPlane >> 1;
		ic.x = inc_mod3[ic.z];
		ic.y = dec_mod3[ic.z];
		denom = 1.0f / fabsf(pt[0][ic.z]);
		icell.x = min(nRes - 1, max(0, physics_float2int((pt[0][ic.x] * denom + koffs) * kres)));
		icell.y = min(nRes - 1, max(0, physics_float2int((pt[0][ic.y] * denom + koffs) * kres)));
		idcell = icell.x + (icell.y * nRes);
		iz = physics_float2int(min(fabsf(pt[0][ic.z]), rmax) * zscale);
		if (iPass == 0)
		{
			imask = iz - irmin >> 31;
			iz = (iz | imask) & ((1u << 31) - 1);
			pGrid[iPlane][idcell] = min(pGrid[iPlane][idcell] & ((1u << 31) - 1), iz) |
			                        ((pGrid[iPlane][idcell] | imask) & (1 << 31));
		}
		else
		{
			pGrid[iPlane][idcell] &= irmin - iz >> 31 | ((1u << 31) - 1);
		}
	}
}

int get_cubemap_cell_buddy(int idCell, int iBuddy, int nRes)
{
	int idx, istep, bWrap, idBuddy, idWrappedBuddy;
	Vec3i icell, iaxis;
	idx = iBuddy & 1;         // step axis: 0 - local x, 1 - local y
	istep = (iBuddy & 2) - 1; // step direction

	icell[idx] = (idCell >> 8 * idx & 0xFF) + istep; // unpacked cell (x,y,z)
	icell[idx ^ 1] = idCell >> 8 * (idx ^ 1) & 0xFF;
	icell.z = (nRes - 1) & -(idCell >> 16 & 1);

	iaxis.z = idCell >> 17;
	iaxis.x = inc_mod3[iaxis.z];
	iaxis.y = dec_mod3[iaxis.z];

	idBuddy = icell.x | icell.y << 8 | (idCell & 0x70000);
	idWrappedBuddy = icell[idx ^ 1] << 8 * idx | icell.z << 8 * (idx ^ 1) | iaxis[idx] << 17 | istep + 1 << 15;

	bWrap = icell[idx] >> 31 | nRes - 1 - icell[idx] >> 31;
	return (idWrappedBuddy & bWrap) | (idBuddy & ~bWrap);
}

void GrowAndCompareCubemaps(int* pGridOcc[6], int* pGrid[6], int nRes, int nGrow, int& nCells, int& nOccludedCells)
{
	struct cell_info
	{
		int idcell;
		int z;
	};
	int i, iPlane, icell, icell1, idcell, idcell1, ix, iy, bUsed, bVisible, ipass, ihead = 0, itail = 0, itailend,
										       imaxz = (1u << 31) - 1;
	cell_info queue[4096];
	nCells = nOccludedCells = 0;

	for (iPlane = 0; iPlane < 6; iPlane++)
	{
		for (iy = 0; iy < nRes; iy++)
		{
			for (ix = 0; ix < nRes; ix++)
			{
				icell = (iy * nRes) + ix;
				bUsed = isneg(pGrid[iPlane][icell] - imaxz);
				bVisible = isneg(pGrid[iPlane][icell] - pGridOcc[iPlane][icell]);
				nCells += bUsed;
				nOccludedCells += bUsed & (bVisible ^ 1);

				if (bUsed & -nGrow >> 31)
				{
					for (i = 0, idcell = ix | iy << 8 | iPlane << 16; i < 4; i++)
					{ // enqueue neighbouring unused cells
						idcell1 = get_cubemap_cell_buddy(idcell, i, nRes);
						icell1 = ((idcell1 >> 8 & 0xFF) * nRes) + (idcell1 & 0xFF);
						if (pGrid[idcell1 >> 16][icell1] >= imaxz)
						{
							queue[ihead].idcell = idcell1;
							queue[ihead].z = pGrid[iPlane][icell];
							ihead = (ihead + 1) & ((sizeof(queue) / sizeof(queue[0])) - 1);
						}
					}
				}
			}
		}
	}

	for (ipass = 0; ipass < nGrow; ipass++)
	{
		for (itailend = ihead; itail != itailend;
		     itail = (itail + 1) & ((sizeof(queue) / sizeof(queue[0])) - 1))
		{
			idcell = queue[itail].idcell;
			icell = ((idcell >> 8 & 0xFF) * nRes) + (idcell & 0xFF);
			iPlane = idcell >> 16;
			if (pGrid[iPlane][icell] < imaxz)
			{
				continue;
			}
			pGrid[iPlane][icell] = queue[itail].z;

			bVisible = isneg(pGrid[iPlane][icell] - pGridOcc[iPlane][icell]);
			nCells++;
			nOccludedCells += (bVisible ^ 1);

			for (i = 0; i < 4; i++)
			{ // enqueue neighbouring unused cells
				idcell1 = get_cubemap_cell_buddy(idcell, i, nRes);
				icell1 = ((idcell1 >> 8 & 0xFF) * nRes) + (idcell1 & 0xFF);
				if (pGrid[idcell1 >> 16][icell1] >= imaxz)
				{
					queue[ihead].idcell = idcell1;
					queue[ihead].z = pGrid[iPlane][icell];
					ihead = (ihead + 1) & ((sizeof(queue) / sizeof(queue[0])) - 1);
				}
			}
		}
	}
}

int crop_polygon_with_plane(const Vec3* ptsrc, int nsrc, Vec3* ptdst, const Vec3& n, float d)
{
	int i0, i1, ndst, iCount;
	for (i0 = 0; i0 < nsrc && ptsrc[i0] * n >= d; i0++)
		;
	if (i0 == nsrc)
	{
		return 0;
	}
	for (iCount = ndst = 0; iCount < nsrc; iCount++, i0 = i1)
	{
		i1 = (i0 + 1) & i0 - nsrc + 1 >> 31;
		ptdst[ndst] = ptsrc[i0];
		ndst += isneg(ptsrc[i0] * n - d);
		if ((ptsrc[i0] * n - d) * (ptsrc[i1] * n - d) < 0)
		{
			ptdst[ndst++] =
			    ptsrc[i0] + (ptsrc[i1] - ptsrc[i0]) * ((d - ptsrc[i0] * n) / ((ptsrc[i1] - ptsrc[i0]) * n));
		}
	}
	return ndst;
}

void CalcMediumResistance(const Vec3* ptsrc, int npt, const Vec3& n, const primitives::plane& waterPlane,
                          const Vec3& vworld, const Vec3& wworld, const Vec3& com, Vec3& P, Vec3& L)
{
	int i;
	Vec3 pt0[16], pt[16], v, w, rotax, dP(ZERO), dL(ZERO);
	float x0, y0, dx, dy, Fxy, Fxx, Fxxy, Fxyy, Fxxx, square = 0, sina;
	npt = crop_polygon_with_plane(ptsrc, npt, pt0, waterPlane.n, waterPlane.origin * waterPlane.n);
	for (i = 0; i < npt; i++)
	{
		pt0[i] -= com;
	}
	npt = crop_polygon_with_plane(pt0, npt, pt, wworld ^ n, vworld * n);

	rotax = n ^ Vec3(0, 0, 1);
	sina = rotax.len();
	if (sina > 0.001f)
	{
		rotax /= sina;
	}
	else
	{
		rotax.Set(1, 0, 0);
	}
	v = vworld.GetRotated(rotax, n.z, sina);
	w = wworld.GetRotated(rotax, n.z, sina);
	for (i = 0; i < npt; i++)
	{
		pt[i] = pt[i].GetRotated(rotax, n.z, sina);
	}
	pt[npt] = pt[0];

	for (i = 0; i < npt; i++)
	{
		square += (pt[i].x * pt[i + 1].y) - (pt[i + 1].x * pt[i].y);
		x0 = pt[i].x;
		y0 = pt[i].y;
		dx = pt[i + 1].x - pt[i].x;
		dy = pt[i + 1].y - pt[i].y;
		Fxy = (x0 * y0) + ((dx * y0 + dy * x0) * 0.5f) + (dx * dy * (1.0f / 3));
		Fxx = (x0 * x0) + (dx * x0) + (dx * dx * (1.0f / 3));
		dP.z += dy * (w.x * Fxy - w.y * 0.5f * Fxx);
		dL.x += v.z * dy * Fxy;
		dL.y -= v.z * dy * 0.5f * Fxx;
		Fxxy = (dx * dx * dy * 0.25f) + ((dx * dx * y0 + dx * dy * x0 * 2) * (1.0f / 3)) +
		       ((x0 * y0 * dx * 2 + x0 * x0 * dy) * 0.5f) + (x0 * x0 * y0);
		Fxyy = (dy * dy * dx * 0.25f) + ((dy * dy * x0 + dy * dx * y0 * 2) * (1.0f / 3)) +
		       ((y0 * x0 * dy * 2 + y0 * y0 * dx) * 0.5f) + (y0 * y0 * x0);
		Fxxx = (dx * dx * dx * 0.25f) + (dx * dx * x0) + (dx * x0 * x0 * 1.5f) + (x0 * x0 * x0);
		dL.x += dy * (w.x * Fxyy - w.y * 0.5f * Fxxy);
		dL.y -= dy * (w.x * 0.5 * Fxxy - w.y * (1.0f / 3) * Fxxx);
	}
	dP.z += v.z * square * 0.5f;
	P -= dP.GetRotated(rotax, n.z, -sina);
	L -= dL.GetRotated(rotax, n.z, -sina);
}

int CoverPolygonWithCircles(strided_pointer<vector2df> pt, int npt, bool bConsecutive, const vector2df& center,
                            vector2df*& centers, float*& radii, float minCircleRadius)
{
	intptr_t imask;
	int i, nCircles = 0, nSkipped;
	vector2df pts[3], bisector;
	float r, area, l2;
	ptitem2d *pdata, *plist, *pvtx, *pvtx_max, *pvtx_left, *pvtx_right;

	static vector2df g_centers[32];
	static float g_radii[32];
	centers = g_centers;
	radii = g_radii;
	if (npt < 2)
	{
		return 0;
	}
	pdata = plist = new ptitem2d[npt];
	for (i = 0, r = 0; i < npt; i++)
	{
		pdata[i].pt = pt[i] - center;
		pdata[i].next = pdata + (i + 1 & i + 1 - npt >> 31);
		pdata[i].prev = pdata + i - 1 + (npt & i - 1 >> 31);
		r = max(r, len2(pdata[i].pt));
	}
	if (r < sqr(minCircleRadius))
	{
		g_centers[0] = center;
		g_radii[0] = sqrt_tpl(r);
		return 1;
	}
	if (!bConsecutive)
	{
		edgeitem *pcontour = new edgeitem[npt], *pedge;
		if (qhull2d(pdata, npt, pcontour))
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
		delete[] pcontour;
	}
	for (i = 0, area = 0, pvtx = plist; i < npt; i++, pvtx = pvtx->next)
	{
		area += pvtx->pt ^ pvtx->next->pt;
	}
	if (fabs_tpl((area * 0.5f) - (r * g_PI)) < area * 0.4f)
	{
		// one circle fits the figure quite ok
		g_centers[0] = center;
		g_radii[0] = sqrt_tpl(r);
		return 1;
	}

	do
	{
		pvtx = pvtx_max = plist;
		do
		{ // find the farthest from the center vertex
			imask = -isneg(len2(pvtx_max->pt) - len2(pvtx->pt));
			pvtx_max = (ptitem2d*)(((intptr_t)pvtx_max & ~imask) | ((intptr_t)pvtx & imask));
		}
		while ((pvtx = pvtx->next) != plist);
		l2 = len2(pvtx_max->pt);

		// find the farthest from the center vertex in +30 degrees vicinity of the global maximum
		for (pvtx = (pvtx_left = pvtx_max)->next;
		     pvtx != pvtx_max && sqr(pvtx->pt ^ pvtx_max->pt) < 0.25f * len2(pvtx->pt) * l2 &&
		     pvtx->pt * pvtx_max->pt > 0;
		     pvtx = pvtx->next)
		{
			imask = -((intptr_t)isneg(len2(pvtx_left->pt) - len2(pvtx->pt)) |
			          iszero((intptr_t)pvtx_left - (intptr_t)pvtx_max));
			pvtx_left = (ptitem2d*)(((intptr_t)pvtx_left & ~imask) | ((intptr_t)pvtx & imask));
		}
		// find the farthest from the center vertex in -30 degrees vicinity of the global maximum
		for (pvtx = (pvtx_right = pvtx_max)->prev;
		     pvtx != pvtx_max && sqr(pvtx->pt ^ pvtx_max->pt) < 0.25f * len2(pvtx->pt) * l2 &&
		     pvtx->pt * pvtx_max->pt > 0;
		     pvtx = pvtx->prev)
		{
			imask = -((intptr_t)isneg(len2(pvtx_right->pt) - len2(pvtx->pt)) |
			          iszero((intptr_t)pvtx_right - (intptr_t)pvtx_max));
			pvtx_right = (ptitem2d*)(((intptr_t)pvtx_right & ~imask) | ((intptr_t)pvtx & imask));
		}

		// find a circle w/ center on left-right bisector that covers all 3 max vertices
		bisector = norm(pvtx_left->pt + pvtx_right->pt);
		pts[0] = pvtx_left->pt;
		pts[1] = pvtx_right->pt;
		pts[2] = pvtx_max->pt;
		for (i = 0, r = 0; i < 3; i++)
		{
			float x = bisector * pts[i];
			r = max(r, (sqr(x) + sqr(bisector ^ pts[i])) / (2 * x));
		}
		g_centers[nCircles] = center + bisector * r;
		g_radii[nCircles++] = r;

		// remove all vertices that lie inside (or close enough to) this new circle
		for (i = nSkipped = 0, pvtx = plist; i < npt; i++, pvtx = pvtx->next)
		{
			if (len2(pvtx->pt + center - g_centers[nCircles - 1]) < r * 1.1f)
			{
				pvtx->next->prev = pvtx->prev;
				pvtx->prev->next = pvtx->next;
				nSkipped++;
				if (pvtx == plist)
				{
					if (pvtx->prev != pvtx)
					{
						plist = pvtx->prev;
					}
					else
					{
						goto allcircles;
					}
				}
			}
		}
		npt -= nSkipped;
	}
	while (nSkipped && nCircles < sizeof(g_centers) / sizeof(g_centers[0]));

allcircles:
	delete[] pdata;
	return nCircles;
}

int ChoosePrimitiveForMesh(strided_pointer<const Vec3> pVertices, strided_pointer<const unsigned short> pIndices,
                           int nTris, const Vec3r* eigen_axes, const Vec3r& center, int flags, float tolerance,
                           primitives::primitive*& pprim)
{
	static primitives::cylinder acyl;
	static primitives::box abox;
	static primitives::sphere asphere;
	int i, j, ibest;
	real error_max[3], error_avg[4], locerror, locarea;

	if (flags & mesh_approx_cylinder)
	{
		float r[3], h[3], area[2], zloc, rinv, hinv;
		Matrix33 Basis = GetMtxFromBasis(eigen_axes);
		Vec3 axis, ptloc, n, ptmin, ptmax, c;
		int iz, bBest, itype;
		error_avg[3] = (real)1E10;
		ibest = 3;

		ptmin = ptmax = Basis * pVertices[pIndices[0]];
		for (i = 1; i < nTris * 3; i++)
		{
			ptloc = Basis * pVertices[pIndices[i]];
			ptmin = min(ptmin, ptloc);
			ptmax = max(ptmax, ptloc);
		}
		c = ((ptmin + ptmax) * 0.5f) * Basis;

		for (iz = 0; iz < 3; iz++)
		{
			axis = eigen_axes[iz];
			for (i = 0, r[iz] = h[iz] = 0; i < nTris * 3; i++)
			{
				ptloc = pVertices[pIndices[i]] - c;
				zloc = ptloc * axis;
				r[iz] = max(r[iz], (ptloc - axis * zloc).len2());
				h[iz] = max(h[iz], zloc);
			}
			r[iz] = sqrt_tpl(r[iz]);
			if (fabs_tpl(r[iz]) < (real)1E-5 || fabs_tpl(h[iz]) < (real)1E-5)
			{
				continue;
			}
			rinv = (real)1 / r[iz];
			hinv = (real)1 / h[iz];
			error_max[iz] = error_avg[iz] = 0;
			area[0] = area[1] = 0;

			for (i = 0; i < nTris; i++)
			{
				n = pVertices[pIndices[(i * 3) + 1]] - pVertices[pIndices[i * 3]] ^
				    pVertices[pIndices[(i * 3) + 2]] - pVertices[pIndices[i * 3]];
				if (n.len2() == 0)
				{
					continue;
				}
				locarea = n.len();
				n /= locarea;
				locarea *= (real)0.5;
				zloc = fabs_tpl(n * axis);
				itype = isneg((real)0.5 - zloc); // 0-cylinder side, 1-cap
				locerror = 0; // locerror will contain maximum distance from from triangle points to the
				              // cyl surface, normalized by cyl size
				if (itype)
				{
					for (j = 0; j < 3; j++)
					{
						locerror = max(
						    locerror,
						    fabs_tpl((fabs_tpl((pVertices[pIndices[(i * 3) + j]] - c) * axis) *
						              hinv) -
						             (real)1));
					}
				}
				else
				{
					for (j = 0; j < 3; j++)
					{
						ptloc = pVertices[pIndices[(i * 3) + j]] - c;
						locerror = max(
						    locerror,
						    fabs_tpl(((ptloc - axis * (ptloc * axis)).len() * rinv) - (real)1));
					}
				}
				error_max[iz] = max(error_max[iz], locerror);
				error_avg[iz] += locerror * locarea;
				area[itype] += locarea;
			}
			error_avg[iz] /= (area[0] + area[1]);
			// additionally check if object area is close to that of the cylinder
			locerror = fabs_tpl((area[0] - r[iz] * h[iz] * g_PI * 4) * (rinv * hinv * ((real)0.5 / g_PI)));
			locerror = max(locerror, fabs_tpl((area[1] - r[iz] * r[iz] * g_PI * 2) *
			                                  (rinv * rinv * ((real)0.5 / g_PI))));
			error_max[iz] = max(error_max[iz], locerror);
			error_avg[iz] = (error_avg[iz] * (real)0.7) + (locerror * (real)0.3);
			bBest = isneg(error_avg[iz] - error_avg[ibest]);
			ibest = (ibest & ~-bBest) | (iz & -bBest);
		}

		if (ibest < 3 && error_max[ibest] < tolerance * 1.5f && error_avg[ibest] < tolerance)
		{
			acyl.axis = eigen_axes[ibest];
			acyl.center = c;
			acyl.r = r[ibest];
			acyl.hh = h[ibest];
			pprim = &acyl;
			return primitives::cylinder::type;
		}
	}

	if (flags & mesh_approx_capsule)
	{
		float r[3], h[3], area[2], zloc, rinv, hinv;
		Matrix33 Basis = GetMtxFromBasis(eigen_axes);
		Vec3 axis, ptloc, n, ptmin, ptmax, c;
		int iz, bBest, itype;
		error_avg[3] = (real)1E10;
		ibest = 3;

		ptmin = ptmax = Basis * pVertices[pIndices[0]];
		for (i = 1; i < nTris * 3; i++)
		{
			ptloc = Basis * pVertices[pIndices[i]];
			ptmin = min(ptmin, ptloc);
			ptmax = max(ptmax, ptloc);
		}
		c = ((ptmin + ptmax) * 0.5f) * Basis;

		for (iz = 0; iz < 3; iz++)
		{
			axis = eigen_axes[iz];
			for (i = 0, r[iz] = h[iz] = 0; i < nTris * 3; i++)
			{
				ptloc = pVertices[pIndices[i]] - c;
				zloc = ptloc * axis;
				r[iz] = max(r[iz], (ptloc - axis * zloc).len2());
				h[iz] = max(h[iz], zloc);
			}
			r[iz] = sqrt_tpl(r[iz]);
			h[iz] -= r[iz];
			if (fabs_tpl(r[iz]) < (real)1E-5 || fabs_tpl(h[iz]) < (real)1E-5)
			{
				continue;
			}
			rinv = (real)1 / r[iz];
			hinv = (real)1 / h[iz];
			error_max[iz] = error_avg[iz] = 0;
			area[0] = area[1] = 0;

			for (i = 0; i < nTris; i++)
			{
				n = pVertices[pIndices[(i * 3) + 1]] - pVertices[pIndices[i * 3]] ^
				    pVertices[pIndices[(i * 3) + 2]] - pVertices[pIndices[i * 3]];
				if (n.len2() == 0)
				{
					continue;
				}
				locarea = n.len();
				n /= locarea;
				locarea *= (real)0.5;
				zloc = ((pVertices[pIndices[i * 3]] + pVertices[pIndices[(i * 3) + 1]] +
				         pVertices[pIndices[(i * 3) + 2]]) *
				            (1.0f / 3) -
				        c) *
				       axis;
				itype = isneg(h[iz] - fabs_tpl(zloc)); // 0-capsule side, 1-cap
				locerror = 0; // locerror will contain maximum distance from from triangle points to the
				              // capsule surface, normalized by capsule size
				if (itype)
				{
					for (j = 0; j < 3; j++)
					{
						locerror = max(locerror, ((pVertices[pIndices[(i * 3) + j]] - c -
						                           axis * (h[iz] * sgnnz(zloc)))
						                              .len() *
						                          rinv) -
						                             (real)1);
					}
				}
				else
				{
					for (j = 0; j < 3; j++)
					{
						ptloc = pVertices[pIndices[(i * 3) + j]] - c;
						locerror = max(
						    locerror,
						    fabs_tpl(((ptloc - axis * (ptloc * axis)).len() * rinv) - (real)1));
					}
				}
				error_max[iz] = max(error_max[iz], locerror);
				error_avg[iz] += locerror * locarea;
				area[itype] += locarea;
			}
			error_avg[iz] /= (area[0] + area[1]);
			// additionally check if object area is close to that of the cylinder
			locerror = fabs_tpl((area[0] - r[iz] * h[iz] * g_PI * 4) * (rinv * hinv * ((real)0.5 / g_PI)));
			locerror = max(locerror, fabs_tpl((area[1] - r[iz] * r[iz] * g_PI * 4) *
			                                  (rinv * rinv * ((real)0.25 / g_PI))));
			error_max[iz] = max(error_max[iz], locerror);
			error_avg[iz] = (error_avg[iz] * (real)0.7) + (locerror * (real)0.3);
			bBest = isneg(error_avg[iz] - error_avg[ibest]);
			ibest = (ibest & ~-bBest) | (iz & -bBest);
		}

		if (ibest < 3 && error_max[ibest] < tolerance * 1.5f && error_avg[ibest] < tolerance)
		{
			acyl.axis = eigen_axes[ibest];
			acyl.center = c;
			acyl.r = r[ibest];
			acyl.hh = h[ibest];
			pprim = &acyl;
			return primitives::capsule::type;
		}
	}

	if (flags & mesh_approx_box)
	{
		int itry, iside;
		Matrix33 Basis = GetMtxFromBasis(eigen_axes);
		Vec3 size[2], rsize, pt, ptmin, ptmax, c[2];
		real area;
		error_max[0] = error_avg[0] = error_max[1] = error_avg[1] = 1E10;

		for (itry = 0; itry < 2; itry++)
		{
			ptmin = ptmax = Basis * pVertices[pIndices[0]];
			for (i = 1; i < nTris * 3; i++)
			{
				pt = Basis * pVertices[pIndices[i]];
				ptmin.x = min(ptmin.x, pt.x);
				ptmax.x = max(ptmax.x, pt.x);
				ptmin.y = min(ptmin.y, pt.y);
				ptmax.y = max(ptmax.y, pt.y);
				ptmin.z = min(ptmin.z, pt.z);
				ptmax.z = max(ptmax.z, pt.z);
			}
			c[itry] = ((ptmin + ptmax) * 0.5f) * Basis;
			size[itry] = (ptmax - ptmin) * 0.5f;
			if (size[itry].x * size[itry].y * size[itry].z == 0)
			{
				continue;
			}
			error_max[itry] = error_avg[itry] = 0;
			rsize.x = 1.0f / size[itry].x;
			rsize.y = 1.0f / size[itry].y;
			rsize.z = 1.0f / size[itry].z;
			for (i = 0, area = 0; i < nTris; i++)
			{
				pt = Basis * ((pVertices[pIndices[i * 3]] + pVertices[pIndices[(i * 3) + 1]] +
				               pVertices[pIndices[(i * 3) + 2]]) *
				                  (1.0f / 3) -
				              c[itry]);
				pt.x = fabsf(pt.x * rsize.x);
				pt.y = fabsf(pt.y * rsize.y);
				pt.z = fabsf(pt.z * rsize.z);
				locarea = (pVertices[pIndices[(i * 3) + 1]] - pVertices[pIndices[i * 3]] ^
				           pVertices[pIndices[(i * 3) + 2]] - pVertices[pIndices[i * 3]])
				              .len();
				iside = idxmax3(&pt.x);
				locerror = 0;
				for (j = 0; j < 3; j++)
				{
					locerror = max(
					    locerror, fabs_tpl(((fabs_tpl((pVertices[pIndices[(i * 3) + j]] - c[itry]) *
					                                  Basis.GetRow(iside))) *
					                        rsize[iside]) -
					                       (real)1));
				}
				error_max[itry] = max(error_max[itry], locerror);
				error_avg[itry] += locerror * locarea;
				area += locarea;
			}
			error_avg[itry] /= area;
			// additionally check if object area is close to that of the box
			locerror = fabs_tpl(
			    ((size[itry].x * size[itry].y + size[itry].x * size[itry].z + size[itry].y * size[itry].z) *
			     16 / area) -
			    1);
			error_max[itry] = max(error_max[itry], locerror);
			error_avg[itry] = (error_avg[itry] * (real)0.7) + (locerror * (real)0.3);
			Basis.SetIdentity(); // try axis aligned box after eigen-vectors aligned
		}

		ibest = isneg(error_avg[1] - (error_avg[0] * 0.95f)); // favor axis-aligned box slightly
		if (error_max[ibest] < tolerance * 1.5f && error_avg[ibest] < tolerance)
		{
			abox.size = size[ibest];
			abox.center = c[ibest];
			if (ibest)
			{
				abox.bOriented = 0;
				abox.Basis.SetIdentity();
			}
			else
			{
				abox.bOriented = 1;
				abox.Basis = GetMtxFromBasis(eigen_axes);
			}
			pprim = &abox;
			return primitives::box::type;
		}
	}

	if (flags & mesh_approx_sphere)
	{
		Vec3r p0, p1, p2, n;
		real r, rinv, area;
		for (i = 0, r = 0; i < nTris * 3; i++)
		{
			r += (pVertices[pIndices[i]] - center).len();
		}
		r /= nTris * 3;
		rinv = (real)1.0 / r;
		error_max[0] = error_avg[0] = area = 0;
		for (i = 0; i < nTris; i++)
		{
			p0 = pVertices[pIndices[i * 3]];
			p1 = pVertices[pIndices[(i * 3) + 1]];
			p2 = pVertices[pIndices[(i * 3) + 2]];
			locerror = fabs_tpl((p0 - center).len() - r) * rinv;
			locerror = max(locerror, fabs_tpl((p1 - center).len() - r) * rinv);
			locerror = max(locerror, fabs_tpl((p2 - center).len() - r) * rinv);
			n = p1 - p0 ^ p2 - p0;
			locarea = n.len();
			if (locarea > 1E-5)
			{
				locerror = max(locerror, fabs_tpl((((p0 - center) * n) / locarea) - r) * rinv);
			}
			error_max[0] = max(error_max[0], locerror);
			error_avg[0] += locerror * locarea;
			area += locarea;
		}
		error_avg[0] /= area;
		if (error_max[0] < tolerance * 1.5f && error_avg[0] < tolerance)
		{
			asphere.r = r;
			asphere.center = center;
			pprim = &asphere;
			return primitives::sphere::type;
		}
	}

	return primitives::triangle::type;
}

void ExtrudeBox(const primitives::box* pbox, const Vec3& dir, float step, primitives::box* pextbox)
{
	float proj, maxproj;
	int i;
	maxproj = (pbox->Basis.GetRow(0) - dir * (dir * pbox->Basis.GetRow(0))).len2() * pbox->size[0];
	proj = (pbox->Basis.GetRow(1) - dir * (dir * pbox->Basis.GetRow(1))).len2() * pbox->size[1];
	i = isneg(maxproj - proj);
	maxproj = max(proj, maxproj);
	proj = (pbox->Basis.GetRow(2) - dir * (dir * pbox->Basis.GetRow(2))).len2() * pbox->size[2];
	i |= isneg(maxproj - proj) << 1;
	i &= 2 | (i >> 1 ^ 1);

	pextbox->Basis.SetRow(2, dir);
	pextbox->Basis.SetRow(0, (pbox->Basis.GetRow(i) - dir * (dir * pbox->Basis.GetRow(i))).normalized());
	pextbox->Basis.SetRow(1, pextbox->Basis.GetRow(2) ^ pextbox->Basis.GetRow(0));
	pextbox->bOriented = 1;
	Matrix33 mtx = pextbox->Basis * pbox->Basis.T();
	(pextbox->size = mtx.Fabs() * pbox->size).z += fabs_tpl(step) * 0.5f;
	pextbox->center = pbox->center + dir * (step * 0.5f);
}

namespace {

struct vtxthunk
{
	vtxthunk* next[2];
	vtxthunk* jump;
	vector2df* pt;
	int bProcessed;
};

int TriangulatePolyBruteforce(vector2df* pVtx, int nVtx, int* pTris, int szTriBuf)
{
	int i, nThunks, nNonEars, nTris = 0;
	vtxthunk *ptr, *ptr0, bufThunks[32], *pThunks = nVtx <= 31 ? bufThunks : new vtxthunk[nVtx + 1];

	ptr = ptr0 = pThunks;
	for (i = nThunks = 0; i < nVtx; i++)
	{
		if (!is_unused(pVtx[i].x))
		{
			pThunks[nThunks].next[0] = pThunks + nThunks - 1;
			pThunks[nThunks].next[1] = pThunks + nThunks + 1;
			pThunks[nThunks].pt = pVtx + i;
			ptr = pThunks + nThunks++;
		}
	}
	if (nThunks < 3)
	{
		return 0;
	}
	ptr->next[1] = ptr0;
	ptr0->next[0] = ptr;
	for (i = 0; i < nThunks; i++)
	{
		pThunks[i].bProcessed =
		    (*pThunks[i].next[1]->pt - *pThunks[i].pt ^ *pThunks[i].next[0]->pt - *pThunks[i].pt) > 0;
	}

	for (nNonEars = 0; nNonEars < nThunks && nTris < szTriBuf; ptr0 = ptr0->next[1])
	{
		if (nThunks == 3)
		{
			pTris[nTris * 3] = ptr0->pt - pVtx;
			pTris[(nTris * 3) + 1] = ptr0->next[1]->pt - pVtx;
			pTris[(nTris * 3) + 2] = ptr0->next[0]->pt - pVtx;
			nTris++;
			break;
		}
		for (i = 0; (*ptr0->next[1]->pt - *ptr0->pt ^ *ptr0->next[0]->pt - *ptr0->pt) < 0 && i < nThunks;
		     ptr0 = ptr0->next[1], i++)
			;
		if (i == nThunks)
		{
			break;
		}
		for (ptr = ptr0->next[1]->next[1]; ptr != ptr0->next[0] && ptr->bProcessed; ptr = ptr->next[1])
			; // find the 1st non-convex vertex after ptr0
		for (; ptr != ptr0->next[0] &&
		       min(min(*ptr0->pt - *ptr0->next[0]->pt ^ *ptr->pt - *ptr0->next[0]->pt,
		               *ptr0->next[1]->pt - *ptr0->pt ^ *ptr->pt - *ptr0->pt),
		           *ptr0->next[0]->pt - *ptr0->next[1]->pt ^ *ptr->pt - *ptr0->next[1]->pt) < 0;
		     ptr = ptr->next[1])
			;
		if (ptr == ptr0->next[0])
		{ // vertex is an ear, output the corresponding triangle
			pTris[nTris * 3] = ptr0->pt - pVtx;
			pTris[(nTris * 3) + 1] = ptr0->next[1]->pt - pVtx;
			pTris[(nTris * 3) + 2] = ptr0->next[0]->pt - pVtx;
			nTris++;
			ptr0->next[1]->next[0] = ptr0->next[0];
			ptr0->next[0]->next[1] = ptr0->next[1];
			nThunks--;
			nNonEars = 0;
		}
		else
		{
			nNonEars++;
		}
	}

	if (pThunks != bufThunks)
	{
		delete[] pThunks;
	}
	return nTris;
}

} // unnamed namespace

int TriangulatePoly(vector2df* pVtx, int nVtx, int* pTris, int szTriBuf)
{
	if (nVtx < 3)
	{
		return 0;
	}
	vtxthunk *pThunks, *pPrevThunk, *pContStart, **pSags, **pBottoms, *pPinnacle, *pBounds[2], *pPrevBounds[2],
	    *ptr, *ptr_next;
	vtxthunk bufThunks[32], *bufSags[16], *bufBottoms[16];
	int i, nThunks, nBottoms = 0, nSags = 0, iBottom = 0, nConts = 0, j, isag, nThunks0, nTris = 0, nPrevSags,
			nTrisCnt, iter;
	float ymax, ymin, e, area0 = 0, area1 = 0, cntarea, minCntArea;

	isag = is_unused(pVtx[0].x);
	ymin = ymax = pVtx[isag].y;
	for (i = isag; i < nVtx; i++)
	{
		if (!is_unused(pVtx[i].x))
		{
			ymin = min(ymin, pVtx[i].y);
			ymax = max(ymax, pVtx[i].y);
		}
	}
	e = (ymax - ymin) * 0.0005f;
	for (i = 1 + isag; i < nVtx; i++)
	{
		if (!is_unused(pVtx[i].x))
		{
			j = i < nVtx - 1 && !is_unused(pVtx[i + 1].x) ? i + 1 : isag;
			if ((ymin = min(pVtx[j].y, pVtx[i - 1].y)) > pVtx[i].y - e)
			{
				if ((pVtx[j] - pVtx[i] ^ pVtx[i - 1] - pVtx[i]) > 0)
				{
					nBottoms++; // we have a bottom
				}
				else if (ymin > pVtx[i].y + 1E-8f)
				{
					nSags++; // we have a sag
				}
			}
		}
		else
		{
			nConts++;
			isag = ++i;
		}
	}
	nSags += nConts;
	pThunks = nVtx + (nSags * 2) <= sizeof(bufThunks) / sizeof(bufThunks[0]) ? bufThunks
	                                                                         : new vtxthunk[nVtx + (nSags * 2)];

	for (i = nThunks = 0, pContStart = pPrevThunk = pThunks; i < nVtx; i++)
	{
		if (!is_unused(pVtx[i].x))
		{
			pThunks[nThunks].next[1] = pThunks + nThunks;
			pThunks[nThunks].next[1] = pPrevThunk->next[1];
			pPrevThunk->next[1] = pThunks + nThunks;
			pThunks[nThunks].next[0] = pPrevThunk;
			pThunks[nThunks].jump = 0;
			pPrevThunk = pThunks + nThunks;
			pThunks[nThunks].bProcessed = 0;
			pThunks[nThunks++].pt = &pVtx[i];
		}
		else
		{
			pPrevThunk->next[1] = pContStart;
			pContStart->next[0] = pThunks + nThunks - 1;
			pContStart = pPrevThunk = pThunks + nThunks;
		}
	}

	for (i = j = 0, cntarea = 0, minCntArea = 1; i < nThunks; i++)
	{
		cntarea += *pThunks[i].pt ^ *pThunks[i].next[1]->pt;
		j++;
		if (pThunks[i].next[1] != pThunks + i + 1)
		{
			if (j >= 3)
			{
				area0 += cntarea;
				minCntArea = min(cntarea, minCntArea);
			}
			cntarea = 0;
			j = 0;
		}
	}
	if (minCntArea > 0 && nConts > 1)
	{
		// if all contours are positive, triangulate them as separate (it's more safe)
		for (i = 0; i < nThunks; i++)
		{
			if (pThunks[i].next[0] != pThunks + i - 1)
			{
				nTrisCnt = TriangulatePoly(pThunks[i].pt, (pThunks[i].next[0]->pt - pThunks[i].pt) + 2,
				                           pTris + (nTris * 3), szTriBuf - (nTris * 3));
				for (j = 0, isag = pThunks[i].pt - pVtx; j < nTrisCnt * 3; j++)
				{
					pTris[(nTris * 3) + j] += isag;
				}
				i = pThunks[i].next[0] - pThunks;
				nTris += nTrisCnt;
			}
		}
		if (pThunks != bufThunks)
		{
			delete[] pThunks;
		}
		return nTris;
	}

	pSags = nSags <= sizeof(bufSags) / sizeof(bufSags[0]) ? bufSags : new vtxthunk*[nSags];
	pBottoms = nSags + nBottoms <= sizeof(bufBottoms) / sizeof(bufBottoms[0]) ? bufBottoms
	                                                                          : new vtxthunk*[nSags + nBottoms];

	for (i = nSags = nBottoms = 0; i < nThunks; i++)
	{
		if ((ymin = min(pThunks[i].next[1]->pt->y, pThunks[i].next[0]->pt->y)) > pThunks[i].pt->y - e)
		{
			if ((*pThunks[i].next[1]->pt - *pThunks[i].pt ^ *pThunks[i].next[0]->pt - *pThunks[i].pt) >= 0)
			{
				pBottoms[nBottoms++] = pThunks + i; // we have a bottom
			}
			else if (ymin > pThunks[i].pt->y + e)
			{
				pSags[nSags++] = pThunks + i; // we have a sag
			}
		}
	}
	iBottom = -1;
	pBounds[0] = pBounds[1] = pPrevBounds[0] = pPrevBounds[1] = 0;
	nThunks0 = nThunks;
	nPrevSags = nSags;
	iter = nThunks * 4;

	do
	{
	nextiter:
		if (!pBounds[0])
		{ // if bounds are empty, get the next available bottom
			for (++iBottom; iBottom < nBottoms && !pBottoms[iBottom]->next[0]; iBottom++)
				;
			if (iBottom >= nBottoms)
			{
				break;
			}
			pBounds[0] = pBounds[1] = pPinnacle = pBottoms[iBottom];
		}
		pBounds[0]->bProcessed = pBounds[1]->bProcessed = 1;
		if (pBounds[0] == pPrevBounds[0] && pBounds[1] == pPrevBounds[1] && nSags == nPrevSags ||
		    !pBounds[0]->next[0] || !pBounds[1]->next[0])
		{
			pBounds[0] = pBounds[1] = 0;
			continue;
		}
		pPrevBounds[0] = pBounds[0];
		pPrevBounds[1] = pBounds[1];
		nPrevSags = nSags;

		// check if left or right is a top
		for (i = 0; i < 2; i++)
		{
			if (pBounds[i]->next[0]->pt->y < pBounds[i]->pt->y &&
			    pBounds[i]->next[1]->pt->y <= pBounds[i]->pt->y &&
			    (*pBounds[i]->next[0]->pt - *pBounds[i]->pt ^ *pBounds[i]->next[1]->pt - *pBounds[i]->pt) >
			        0)
			{
				if (pBounds[i]->jump)
				{
					do
					{
						ptr = pBounds[i]->jump;
						pBounds[i]->jump = 0;
						pBounds[i] = ptr;
					}
					while (pBounds[i]->jump);
				}
				else
				{
					pBounds[i]->jump = pBounds[i ^ 1];
					pBounds[0] = pBounds[1] = 0;
					goto nextiter;
				}
				if (!pBounds[0]->next[0] || !pBounds[1]->next[0])
				{
					pBounds[0] = pBounds[1] = 0;
					goto nextiter;
				}
			}
		}
		i = isneg(pBounds[1]->next[1]->pt->y - pBounds[0]->next[0]->pt->y);
		ymax = pBounds[i ^ 1]->next[i ^ 1]->pt->y;
		ymin = min(pBounds[0]->pt->y, pBounds[1]->pt->y);

		for (j = 0, isag = -1; j < nSags; j++)
		{
			if (inrange(pSags[j]->pt->y, ymin,
			            ymax) && // find a sag in next left-left-right-next right quad
			    pSags[j] != pBounds[0]->next[0] &&
			    pSags[j] != pBounds[1]->next[1] &&
			    (*pBounds[0]->pt - *pBounds[0]->next[0]->pt ^ *pSags[j]->pt - *pBounds[0]->next[0]->pt) >=
			        0 &&
			    (*pBounds[1]->pt - *pBounds[0]->pt ^ *pSags[j]->pt - *pBounds[0]->pt) >= 0 &&
			    (*pBounds[1]->next[1]->pt - *pBounds[1]->pt ^ *pSags[j]->pt - *pBounds[1]->pt) >= 0 &&
			    (*pBounds[0]->next[0]->pt - *pBounds[1]->next[1]->pt ^
			     *pSags[j]->pt - *pBounds[1]->next[1]->pt) >= 0)
			{
				ymax = pSags[j]->pt->y;
				isag = j;
			}
		}

		if (isag >= 0)
		{ // build a bridge between the sag and the highest active point
			if (pSags[isag]->next[0])
			{
				pPinnacle->next[1]->next[0] = pThunks + nThunks;
				pSags[isag]->next[0]->next[1] = pThunks + nThunks + 1;
				pThunks[nThunks].next[0] = pThunks + nThunks + 1;
				pThunks[nThunks].next[1] = pPinnacle->next[1];
				pThunks[nThunks + 1].next[1] = pThunks + nThunks;
				pThunks[nThunks + 1].next[0] = pSags[isag]->next[0];
				pPinnacle->next[1] = pSags[isag];
				pSags[isag]->next[0] = pPinnacle;
				pThunks[nThunks].pt = pPinnacle->pt;
				pThunks[nThunks + 1].pt = pSags[isag]->pt;
				pThunks[nThunks].jump = pThunks[nThunks + 1].jump = 0;
				pThunks[nThunks].bProcessed = pThunks[nThunks + 1].bProcessed = 0;
				if (pBounds[1] == pPinnacle)
				{
					pBounds[1] = pThunks + nThunks;
				}
				for (ptr = pThunks + nThunks, j = 0; ptr != pBounds[1]->next[1] && j < nThunks;
				     ptr = ptr->next[1], j++)
				{
					if (min(ptr->next[0]->pt->y, ptr->next[1]->pt->y) > ptr->pt->y)
					{ // ptr is a bottom
						pBottoms[nBottoms++] = ptr;
						break;
					}
				}
				pBounds[1] = pPinnacle;
				pPinnacle = pSags[isag];
				nThunks += 2;
			}
			for (j = isag; j < nSags - 1; j++)
			{
				pSags[j] = pSags[j + 1];
			}
			--nSags;
			continue;
		}

		// create triangles featuring the new vertex
		for (ptr = pBounds[i]; ptr != pBounds[i ^ 1] && nTris < szTriBuf; ptr = ptr_next)
		{
			if ((*ptr->next[i ^ 1]->pt - *ptr->pt ^ *ptr->next[i]->pt - *ptr->pt) * (1 - i * 2) > 0 ||
			    pBounds[0]->next[0] == pBounds[1]->next[1])
			{
				// output the triangle
				pTris[nTris * 3] = pBounds[i]->next[i]->pt - pVtx;
				pTris[(nTris * 3) + 1 + i] = ptr->pt - pVtx;
				pTris[(nTris * 3) + 2 - i] = ptr->next[i ^ 1]->pt - pVtx;
				area1 += pVtx[pTris[(nTris * 3) + 1]] - pVtx[pTris[nTris * 3]] ^
				         pVtx[pTris[(nTris * 3) + 2]] - pVtx[pTris[nTris * 3]];
				nTris++;
				ptr->next[i ^ 1]->next[i] = ptr->next[i];
				ptr->next[i]->next[i ^ 1] = ptr->next[i ^ 1];
				pBounds[i] = ptr_next = ptr->next[i ^ 1];
				if (pPinnacle == ptr)
				{
					pPinnacle = ptr->next[i];
				}
				ptr->next[0] = ptr->next[1] = 0;
				ptr->bProcessed = 1;
			}
			else
			{
				break;
			}
		}

		if ((pBounds[i] = pBounds[i]->next[i]) == pBounds[i ^ 1]->next[i ^ 1])
		{
			pBounds[0] = pBounds[1] = 0;
		}
		else if (pBounds[i]->pt->y > pPinnacle->pt->y)
		{
			pPinnacle = pBounds[i];
		}
	}
	while (nTris < szTriBuf && --iter);

	if (pThunks != bufThunks)
	{
		delete[] pThunks;
	}
	if (pBottoms != bufBottoms)
	{
		delete[] pBottoms;
	}
	if (pSags != bufSags)
	{
		delete[] pSags;
	}

	if (nTris < nThunks0 - (nConts * 2) || fabs_tpl(area0 - area1) > area0 * 0.003f || nTris >= szTriBuf)
	{
		if (nConts == 1)
		{
			return TriangulatePolyBruteforce(pVtx, nVtx, pTris, szTriBuf);
		}
		else
		{
			g_nTriangulationErrors++;
		}
	}

	return nTris;
}

int jgrid_checker::check_cell(const vector2di& icell, int& ilastcell)
{
	int i, idx, icurcell, mask = 2, bhit;
	vector2df c, step = pgrid->step, pt, pt0(pgrid->origin.x, pgrid->origin.y);

	idx = icell * pgrid->stride;
	pCellMask[idx] |= 2;
	c.set(step.x * (icell.x + 0.5f), step.y * (icell.y + 0.5f));
	icurcell = icell.y << 16 | icell.x;
	pCellMask[idx] |= bMarkCenters & isneg((c - org) * dir);
	if (pnorms)
	{
		pnorms[idx] += dirn.rot90cw();
	}

	if ((icurcell | iprevcell >> 31) != iprevcell)
	{
		if (iedge[0] != (iedge[1] | iedge[0] >> 31))
		{
			for (i = (iedge[1] + 1) & 3; i != iedge[0]; i = (i + 1) & 3)
			{
				mask |= 4 << i;
			}
		}
		pCellMask[vector2di(iprevcell & 0xFFFF, iprevcell >> 16) * pgrid->stride] &= mask;
		for (i = 0; i < 2; i++)
		{
			bhit = -inrange(((dirn ^ org - c) * (1 - i * 2)) - (fabs_tpl(dirn[i ^ 1]) * step[i]),
			                -dirn[i] * step[i ^ 1] * 1.001f, dirn[i] * step[i ^ 1] * 1.001f);
			(iedge[0] &= ~bhit) |= (i ^ 1 | (i ^ isnonneg(dirn[i])) << 1) & bhit;
		}
		if (ppt && icurcell != ilastcell)
		{
			for (pt = org + dirn * (dirn * (c - org)), i = 0;
			     (fabs_tpl(pt.x - c.x) > step.x * 0.5f || fabs_tpl(pt.y - c.y) > step.y * 0.5f) && i < 4;
			     i++)
			{
				pt = org + dirn * (dirn * (c + vector2df(step.x * (0.5f - (i & 1)), 0) +
				                           vector2df(0, step.y * (0.5f - (i >> 1))) - org));
			}
			ppt[idx] = pt + pt0;
		}
	}
	else if (ppt)
	{
		ppt[idx] = org + pt0;
	}
	if (icurcell != ilastcell)
	{
		for (i = 0; i < 2; i++)
		{
			bhit = -inrange(((dirn ^ org - c) * (1 - i * 2)) + (fabs_tpl(dirn[i ^ 1]) * step[i]),
			                -dirn[i] * step[i ^ 1] * 1.001f, dirn[i] * step[i ^ 1] * 1.001f);
			(iedge[1] &= ~bhit) |= (i ^ 1 | (i ^ isneg(dirn[i])) << 1) & bhit;
		}
		iedgeExit0 += (iedge[1] + 1) & iedgeExit0 >> 31;
	}
	else if (ppt)
	{
		ppt[idx] = org + dir + pt0;
	}

	iprevcell = icurcell;
	return 0;
}

void jgrid_checker::MarkCellInterior(int i)
{
	int j, icell;
	pCellMask[i] = 2;
	for (j = 0; j < 4; j++)
	{
		if (!(pCellMask[icell = i + vector2di((1 - (j & 2)) & -(j & 1), ((j & 2) - 1) & ~-(j & 1)) *
		                                pgrid->stride] &
		      2))
		{
			MarkCellInterior(icell);
		}
	}
}

void jgrid_checker::MarkCellInteriorQueue(int icell)
{
	int j, icellNext;
	pCellMask[pqueue[ihead = itail = 0] = icell] = 2;
	for (nQueuedCells = 1; nQueuedCells > 0;)
	{
		icell = pqueue[itail];
		itail = (itail + 1) & itail + 1 - szQueue >> 31;
		nQueuedCells--;
		for (j = 0; j < 4; j++)
		{
			if (!(pCellMask[icellNext =
			                    icell + vector2di((1 - (j & 2)) & -(j & 1), ((j & 2) - 1) & ~-(j & 1)) *
			                                pgrid->stride] &
			      2))
			{
				pCellMask[icellNext] = 2;
				if (nQueuedCells == szQueue)
				{
					ReallocateList(pqueue, szQueue, szQueue + 64);
					memmove(pqueue + itail + 64, pqueue + itail,
					        (szQueue - ihead - 1) * sizeof(pqueue[0]));
					if (itail > 0)
					{
						itail += 64;
					}
					szQueue += 64;
				}
				ihead = (ihead + 1) & ihead + 1 - szQueue >> 31;
				nQueuedCells++;
				pqueue[ihead] = icellNext;
			}
		}
	}
}

const char* numbered_tag(const char* s, unsigned int num)
{
	static char str[64];
	std::snprintf(str, sizeof(str), "%s_%u", s, num);
	return str;
}
