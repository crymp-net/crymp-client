#pragma once

#include <cstdio>

namespace CryMemoryManager
{
	void Init();

	void ProvideHeapInfo(std::FILE* file, void* address);

	void LogInfo();
}
