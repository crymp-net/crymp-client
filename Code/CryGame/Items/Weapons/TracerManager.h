/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Tracer Manager

-------------------------------------------------------------------------
History:
- 17:1:2006   11:12 : Created by Márcio Martins

*************************************************************************/
#ifndef __TRACERMANAGER_H__
#define __TRACERMANAGER_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "CryCommon/CryEntitySystem/EntityId.h"
#include <memory>

class CTracer
{
    friend class CTracerManager;

public:
    explicit CTracer(const Vec3& pos);
    virtual ~CTracer();

    void Reset(const Vec3& pos);
    void CreateEntity();
    void SetGeometry(const char* name, float scale);
    void SetEffect(const char* name, float scale);
    void SetLifeTime(float lifeTime) { m_lifeTime = lifeTime; }
    bool Update(float frameTime, const Vec3& camPos);
    void UpdateVisual(const Vec3& pos, const Vec3& dir, float scale, float length);
    float GetAge() const { return m_age; }
    void GetMemoryStatistics(ICrySizer* s) const;

private:
    float     m_speed = 0.0f;
    Vec3      m_pos = Vec3(ZERO);
    Vec3      m_dest = Vec3(ZERO);
    Vec3      m_startingpos = Vec3(ZERO);
    float     m_age = 0.0f;
    float     m_lifeTime = 1.5f;
    bool      m_useGeometry = false;
    int       m_geometrySlot = 0;
    EntityId  m_entityId = 0;
};

class CTracerManager
{
public:
    CTracerManager() = default;
    virtual ~CTracerManager();

    struct STracerParams
    {
        const char* geometry = nullptr;
        const char* effect = nullptr;
        Vec3        position = Vec3(ZERO);
        Vec3        destination = Vec3(ZERO);
        float       speed = 0.0f;
        float       lifetime = 1.5f;
    };

    void EmitTracer(const STracerParams& params);
    size_t CreateTracer(const STracerParams& params);
    void Update(float frameTime);
    void Reset();
    void GetMemoryStatistics(ICrySizer*);

    size_t GetPoolSize() const
    {
        return m_pool.size();
    }

    size_t GetActiveCount() const
    {
        return m_actives.size();
    }

    unsigned int GetNumReused() const 
    { 
        return m_numReused; 
    }

    unsigned int GetNumAllocated() const
    { 
        return m_numAllocated;
    }

private:
    std::vector<std::unique_ptr<CTracer>> m_pool;
    std::vector<int>                      m_updating;
    std::vector<int>                      m_actives;
    unsigned int m_numReused = 0;
    unsigned int m_numAllocated = 0;
};

#endif //__TRACERMANAGER_H__
