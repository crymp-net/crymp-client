#pragma once

struct ISoundSystem;
struct ISystem;

ISoundSystem* CreateSoundSystem(ISystem* pSystem, void* hWnd);
