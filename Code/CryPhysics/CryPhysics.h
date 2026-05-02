#pragma once

struct IPhysicalWorld;
struct ISystem;

IPhysicalWorld* CreatePhysicalWorld(ISystem* pSystem);
