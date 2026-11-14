#include "IntersectData.h"

std::atomic<std::thread::id> g_physicsThreadId{};
IntersectData g_idata[2]{};

int GetCaller()
{
	return std::this_thread::get_id() == g_physicsThreadId.load(std::memory_order_relaxed) ? 0 : 1;
}

void ResetGlobalPrimsBuffers(int iCaller)
{
	g_idata[iCaller].BBoxBufPos = 0;
	g_idata[iCaller].IdxTriBufPos = 0;
	g_idata[iCaller].CylBufPos = 0;
	g_idata[iCaller].BoxBufPos = 0;
	g_idata[iCaller].SphBufPos = 0;
}
