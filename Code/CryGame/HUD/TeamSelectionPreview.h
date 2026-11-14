#pragma once

struct IFlashPlayer;

//CryMP Reset on level changes; preserve the cache across HUD reloads
namespace TeamSelectionPreview
{
	void Preload();
	void Reset();
	void Draw(IFlashPlayer* pFlash, float frameTime);
	void Suspend();
}
