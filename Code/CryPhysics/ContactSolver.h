#pragma once

#include "CryCommon/CryPhysics/IPhysics.h"

class CPhysicalEntity;
class RigidBody;

constexpr int MAX_CONTACTS = 4096;

enum contactflags
{
	contact_count_mask = 0x3F,
	contact_new = 0x40,
	contact_2b_verified = 0x80,
	contact_2b_verified_log2 = 7,
	contact_angular = 0x100,
	contact_constraint_3dof = 0x200,
	contact_constraint_2dof = 0x400,
	contact_constraint_1dof = 0x800,
	contact_solve_for = 0x1000,
	contact_constraint = contact_constraint_3dof | contact_constraint_2dof | contact_constraint_1dof,
	contact_angular_log2 = 8,
	contact_bidx = 0x2000,
	contact_bidx_log2 = 13,
	contact_maintain_count = 0x4000,
	contact_wheel = 0x8000,
	contact_use_C = 0x10000,
	contact_inexact = 0x20000,
	contact_last = 0x40000,
	contact_remove = 0x80000,
	contact_archived = 0x100000,
	contact_rope = 0x200000,
	contact_preserve_Pspare = 0x400000
};

struct entity_contact
{
	entity_contact *next, *prev;
	entity_contact *nextAux, *prevAux;
	int bChunkStart;

	Vec3 pt[2];
	Vec3 n;
	Vec3 dir;
	Vec3 ptloc[2];
	CPhysicalEntity* pent[2];
	int ipart[2];
	RigidBody* pbody[2];
	Vec3 nloc;
	float friction;
	int id[2];
	int flags;
	Vec3 vrel;
	Vec3 vreq;
	// float vsep;
	float Pspare;
	float penetration;
	Matrix33 K, Kinv;
	Matrix33 C;
	int iNormal;
	int iPrim[2];
	int iFeature[2];
	int bProcessed;
	int iCount, *pBounceCount;

	Vec3 r0, r;
	Vec3 dP, P;
	float dPn;
};

#define CONTACT_END(pFirstContact) ((entity_contact*)&(pFirstContact))

struct contact_helper
{
	Vec3 r0, r1;
	Matrix33 K;
	Vec3 n;
	Vec3 vreq;
	float Pspare;
	float friction;
	int flags;
	int iBody[2];
	int iCount : 16;
	int iCountDst : 16;
	float Pn;
};

struct body_helper
{
	Vec3 v, w;
	float Minv, M;
	Matrix33 Iinv;
	Vec3 L;
};

struct contact_sandwich
{
	int iMiddle;
	int iBread[2];
	contact_sandwich* next;
	int bProcessed;
};

struct buddy_info
{
	int iBody;
	Vec3 vreq;
	int flags;
	buddy_info* next;
};

struct follower_thunk
{
	int iBody;
	follower_thunk* next;
};

struct body_info
{
	buddy_info* pbuddy;
	contact_sandwich* psandwich;
	follower_thunk* pfollower;
	float Minv;
	int iLevel;
	int idUpdate;
	int idx;
	Vec3 v_unproj, w_unproj;
	Vec3 Fcollision, Tcollision;
};

extern bool g_bUsePreCG;
extern int g_nContacts;
extern int g_nBodies;
extern entity_contact* g_pContacts[MAX_CONTACTS];

void InitContactSolver(float time_interval);
void InvokeContactSolver(float time_interval, SolverSettings* pss, float Ebefore);
char* AllocSolverTmpBuf(int size);
void RegisterContact(entity_contact* pcontact);
