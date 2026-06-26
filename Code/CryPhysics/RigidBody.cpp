#include "Matrix.h"
#include "RigidBody.h"
#include "Utils.h"

namespace {

template<class ftype>
void OffsetInertiaTensor(Matrix33_tpl<ftype>& I, const Vec3_tpl<ftype>& center, ftype M)
{
	I(0, 0) += M * (center.y * center.y + center.z * center.z);
	I(1, 1) += M * (center.x * center.x + center.z * center.z);
	I(2, 2) += M * (center.x * center.x + center.y * center.y);
	I(1, 0) = I(0, 1) = I(0, 1) - M * center.x * center.y;
	I(2, 0) = I(0, 2) = I(0, 2) - M * center.x * center.z;
	I(2, 1) = I(1, 2) = I(1, 2) - M * center.y * center.z;
}

} // unnamed namespace

RigidBody g_StaticRigidBody;

RigidBody::RigidBody()
{
	P.zero();
	L.zero();
	v.zero();
	w.zero();
	q.SetIdentity();
	pos.zero();
	Ibody.zero();
	bProcessed = 0;
	M = Minv = V = 0;
	Iinv.SetZero();
	Ibody.zero();
	Ibody_inv.zero();
	qfb.SetIdentity();
	offsfb.zero();
	Fcollision.zero();
	Tcollision.zero();
	Eunproj = 0;
	softness[0] = 0.00015f;
	softness[1] = 0.001f;
}

void RigidBody::Create(const Vec3& center, const Vec3& Ibody0, const quaternionf& q0, float volume, float mass,
                       const quaternionf& qframe, const Vec3& posframe)
{
	float density = mass / volume;

	V = volume;
	M = mass;
	q = q0;
	pos = center;
	if (M > 0)
	{
		(Ibody_inv = Ibody = Ibody0 * density).invert();
		Minv = 1.0f / M;
	}
	else
	{
		Ibody_inv = Ibody.zero();
		Minv = 0;
	}
	qfb = !q * qframe;
	offsfb = (pos - posframe) * qframe;

	UpdateState();
}

void RigidBody::Add(const Vec3& center, const Vec3 Ibodyop, const quaternionf& qop, float volume, float mass)
{
	if (mass == 0.0f)
	{
		return;
	}
	if (fabs_tpl(M + mass) < M * 0.0001f)
	{
		zero();
		return;
	}
	float density = mass / volume;
	int i;
	Matrix33 Rop, Ibodyop_mtx, Iop, Ibody_mtx;
	Ibodyop_mtx.SetIdentity();
	for (i = 0; i < 3; i++)
	{
		Ibodyop_mtx(i, i) = Ibodyop[i] * density;
	}

	Rop = Matrix33(!q * qop);
	Iop = Rop * Ibodyop_mtx * Rop.T();

	Vec3 posnew = (pos * M + center * mass) / (M + mass);
	Ibody_mtx = Ibody;
	OffsetInertiaTensor(Ibody_mtx, (posnew - pos) * q, M);
	OffsetInertiaTensor(Iop, (posnew - center) * q, mass);
	M += mass;
	V += volume;
	Ibody_mtx += Iop;
	Minv = 1.0f / M;

	quaternionf qframe = q * qfb;
	offsfb += (posnew - pos) * qframe;

	float Ibody_buf[9], Rbody2newbody[9];
	SetMtxStrided<float, 3, 1>(Ibody_mtx, Ibody_buf);
	matrixf eigenBasis(3, 3, 0, Rbody2newbody);
	matrixf(3, 3, mtx_symmetric, Ibody_buf).jacobi_transformation(eigenBasis, &Ibody.x);
	(Ibody_inv = Ibody).invert();
	quaternionf qb2nb = (quaternionf)GetMtxStrided<float, 3, 1>(Rbody2newbody);
	q *= !qb2nb;
	qfb = qb2nb * qfb;
	pos = posnew;

	UpdateState();
}

void RigidBody::zero()
{
	M = Minv = 0;
	Ibody.zero();
	Ibody_inv.zero();
	P.zero();
	L.zero();
	v.zero();
	w.zero();
}

void RigidBody::UpdateState()
{
	Matrix33 R = Matrix33(q);
	Iinv = R * Ibody_inv * R.T();
	if (Minv > 0)
	{
		v = P * Minv;
		w = Iinv * L;
	}
}

void RigidBody::Step(float dt)
{
	UpdateState();

	pos += v * dt;
	if (w.len2() * sqr(dt) < sqr(0.003f))
	{
		q.w -= (w * q.v) * dt * 0.5f;
		q.v += ((w ^ q.v) + w * q.w) * (dt * 0.5f);
	}
	else
	{
		float wlen = w.len();
		q = Quat::CreateRotationAA(wlen * dt, w / wlen) * q;
	}

	q.Normalize();
	if (Minv > 0)
	{
		Matrix33 R = Matrix33(q);
		Iinv = R * Ibody_inv * R.T();
		w = Iinv * L;
	}
}

void RigidBody::GetContactMatrix(const Vec3& r, Matrix33& K)
{
	Matrix33 rmtx, rmtx1;
	((crossproduct_matrix(r, rmtx)) *= Iinv) *= crossproduct_matrix(r, rmtx1);
	K -= rmtx;
	K(0, 0) += Minv;
	K(1, 1) += Minv;
	K(2, 2) += Minv;
}
