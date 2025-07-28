/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 17:1:2006   11:18 : Created by Márcio Martins

*************************************************************************/
#include "CryCommon/CrySystem/ISystem.h"
#include "TracerManager.h"
#include "CryGame/Game.h"
#include "CryGame/GameCVars.h"
#include "CryGame/Actors/Actor.h"
#include "CryCommon/Cry3DEngine/I3DEngine.h"


#define TRACER_GEOM_SLOT  0
#define TRACER_FX_SLOT    1
//------------------------------------------------------------------------
CTracer::CTracer(const Vec3& pos)
    : m_startingpos(pos)
{
    CreateEntity();
}

CTracer::~CTracer()
{
    if (m_entityId)
    {
        gEnv->pEntitySystem->RemoveEntity(m_entityId);
        m_entityId = 0;
    }
}

void CTracer::Reset(const Vec3& pos)
{
    m_pos = Vec3(ZERO);
    m_dest = Vec3(ZERO);
    m_startingpos = pos;
    m_age = 0.0f;
    m_lifeTime = 1.5f;

    if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_entityId))
    {
        pEntity->FreeSlot(TRACER_GEOM_SLOT);
        pEntity->FreeSlot(TRACER_FX_SLOT);
    }
}

void CTracer::CreateEntity()
{
    if (!m_entityId)
    {
        SEntitySpawnParams spawnParams;
        spawnParams.pClass = gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();
        spawnParams.sName = "_tracer";
        spawnParams.nFlags = ENTITY_FLAG_NO_PROXIMITY | ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_NO_SAVE;

        if (IEntity* pEntity = gEnv->pEntitySystem->SpawnEntity(spawnParams))
        {
            m_entityId = pEntity->GetId();
        }
    }
}

void CTracer::SetGeometry(const char* name, float scale)
{
    if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_entityId))
    {
        m_geometrySlot = pEntity->LoadGeometry(TRACER_GEOM_SLOT, name);

        if (scale != 1.0f)
        {
            Matrix34 tm = Matrix34::CreateIdentity();
            tm.Scale(Vec3(scale));
            pEntity->SetSlotLocalTM(m_geometrySlot, tm);
        }
    }
}

void CTracer::SetEffect(const char* name, float scale)
{
    if (IParticleEffect* pEffect = gEnv->p3DEngine->FindParticleEffect(name))
    {
        if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_entityId))
        {
            const int slot = pEntity->LoadParticleEmitter(TRACER_FX_SLOT, pEffect, 0, true);
            if (scale != 1.0f)
            {
                Matrix34 tm = Matrix34::CreateIdentity();
                tm.Scale(Vec3(scale));
                pEntity->SetSlotLocalTM(slot, tm);
            }
        }
    }
}

bool CTracer::Update(float frameTime, const Vec3& camera)
{
    if (m_age == 0.0f)
    {
        m_age += 0.002f;
        frameTime = 0.002f;
    }
    else
    {
        m_age += frameTime;
    }

    if (m_age >= m_lifeTime)
        return false;

    Vec3 dp = m_dest - m_pos;
    float dist = dp.len();
    if (dist <= 0.5f)
        return false;

    Vec3 dir = dp / dist;
    float scaleMult = 1.0f;

    float cameraDistanceSq = (m_pos - camera).len2();
    float speed = m_speed;

    if (cameraDistanceSq <= g_pGameCVars->tracer_player_radiusSqr)
        speed *= (0.35f + (cameraDistanceSq / (g_pGameCVars->tracer_player_radiusSqr * 2)));

    m_pos += dir * std::min(speed * frameTime, dist);

    cameraDistanceSq = (m_pos - camera).len2();
    float minDistanceSq = sqr(g_pGameCVars->tracer_min_distance);
    float maxDistanceSq = sqr(g_pGameCVars->tracer_max_distance);

    if (cameraDistanceSq <= minDistanceSq)
        scaleMult = g_pGameCVars->tracer_min_scale;
    else if (cameraDistanceSq >= maxDistanceSq)
        scaleMult = g_pGameCVars->tracer_max_scale;
    else
    {
        float t = (sqrtf(cameraDistanceSq) - g_pGameCVars->tracer_min_distance) /
            (g_pGameCVars->tracer_max_distance - g_pGameCVars->tracer_min_distance);
        scaleMult = g_pGameCVars->tracer_min_scale +
            t * (g_pGameCVars->tracer_max_scale - g_pGameCVars->tracer_min_scale);
    }

    if ((m_pos - m_dest).len2() < 0.25f)
        return false;

    UpdateVisual(m_pos, dir, m_useGeometry ? scaleMult : 1.0f, 1.0f);
    return true;
}

void CTracer::UpdateVisual(const Vec3& pos, const Vec3& dir, float scale, float /*length*/)
{
    Matrix34 tm(Matrix33::CreateRotationVDir(dir));
    tm.SetTranslation(pos);

    if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_entityId))
    {
        pEntity->SetWorldTM(tm);
        if (m_useGeometry)
        {
            Matrix34 geomTM = Matrix34::CreateIdentity();
            geomTM.SetScale(Vec3(1.0f, scale, 1.0f));
            pEntity->SetSlotLocalTM(m_geometrySlot, geomTM);
        }
    }
}

void CTracer::GetMemoryStatistics(ICrySizer* s) const
{
    s->Add(*this);
}

CTracerManager::~CTracerManager() = default;

void CTracerManager::EmitTracer(const STracerParams& params)
{
    if (!g_pGameCVars->g_enableTracers || !gEnv->bClient)
        return;

    const size_t idx = CreateTracer(params);
    CTracer* tracer = m_pool[idx].get();

    if (params.geometry && params.geometry[0])
    {
        tracer->SetGeometry(params.geometry, 1.0f);
        tracer->m_useGeometry = true;
    }
    if (params.effect && params.effect[0])
    {
        tracer->SetEffect(params.effect, 1.0f);
    }

    tracer->SetLifeTime(params.lifetime);
    tracer->m_speed = params.speed;
    tracer->m_pos = params.position;
    tracer->m_dest = params.destination;

    if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(tracer->m_entityId))
    {
        pEntity->Hide(false);
    }

    m_actives.push_back(static_cast<int>(idx));
}

size_t CTracerManager::CreateTracer(const STracerParams& params)
{
    IEntitySystem* pEntitySystem = gEnv->pEntitySystem;

    // try to reuse an existing one
    for (size_t i = 0; i < m_pool.size(); i++)
    {
        IEntity* pEntity = pEntitySystem->GetEntity(m_pool[i]->m_entityId);
        if (pEntity && pEntity->IsHidden())
        {
            m_pool[i]->Reset(params.position);
            ++m_numReused;
            return i;
        }
    }

    // create a new one
    m_pool.emplace_back(std::make_unique<CTracer>(params.position));
    ++m_numAllocated;
    return m_pool.size() - 1;
}

void CTracerManager::Update(float frameTime)
{
    IActor* pActor = g_pGame->GetIGameFramework()->GetClientActor();
    if (!pActor || !pActor->GetMovementController())
        return;

    SMovementState state;
    pActor->GetMovementController()->GetMovementState(state);

    m_actives.swap(m_updating);
    for (int id : m_updating)
    {
        CTracer* tracer = m_pool[id].get();
        if (tracer->Update(frameTime, state.eyePosition))
        {
            m_actives.push_back(id);
        }
        else if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(tracer->m_entityId))
        {
            pEntity->Hide(true);
            pEntity->SetWorldTM(Matrix34::CreateIdentity());
        }
    }
    m_updating.clear();
}

void CTracerManager::Reset()
{
    m_pool.clear();
    m_updating.clear();
    m_actives.clear();
}

void CTracerManager::GetMemoryStatistics(ICrySizer* s)
{
    SIZER_SUBCOMPONENT_NAME(s, "TracerManager");
    s->Add(*this);
    s->AddContainer(m_updating);
    s->AddContainer(m_actives);
    s->AddContainer(m_pool);

    for (const auto& tracer : m_pool)
    {
        tracer->GetMemoryStatistics(s);
    }
}
