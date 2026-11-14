#pragma once

#include "CryCommon/CryMath/Cry_Math.h"

class RigidBody
{
public:
	RigidBody();

	void Create(const Vec3& center, const Vec3& Ibody0, const quaternionf& q0, float volume, float mass,
	            const quaternionf& qframe, const Vec3& posframe);
	void Add(const Vec3& center, const Vec3 Ibodyop, const quaternionf& qop, float volume, float mass);
	void zero();

	void Step(float dt);
	void UpdateState();
	void GetContactMatrix(const Vec3& r, Matrix33& K);

	Vec3 pos;
	quaternionf q;
	Vec3 P, L;
	Vec3 w, v;

	float M, Minv;    // mass, 1.0/mass (0 for static objects)
	float V;          // volume
	Diag33 Ibody;     // diagonalized inertia tensor (aligned with body's axes of inertia)
	Diag33 Ibody_inv; // { 1/Ibody.ii }
	quaternionf qfb;  // frame->body rotation
	Vec3 offsfb;      // frame->body offset

	Matrix33 Iinv; // I^-1(t)

	Vec3 Fcollision;
	Vec3 Tcollision;
	int bProcessed; // used internally
	float Eunproj;
	float softness[2];
};

extern RigidBody g_StaticRigidBody;
