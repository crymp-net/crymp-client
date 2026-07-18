#pragma once
#include <string>

namespace CodeWall {
	enum ECodeWall {
		eCW_CLK = 1,
		eCW_MEM = 2,
		eCW_CIG = 4,
		eCW_ACG = 8,
		eCW_DACL = 16,

		eCW_Default = eCW_MEM
	};

	struct CodeWallStatus {
		bool changed = true;
		int status = (int)eCW_Default;
		double elapsed = 0.0;

		void* clkQpcCave = nullptr;
	};

	int  InitializeCodeWallExternal();
	const CodeWallStatus& UpdateCodeWall(bool enabled, bool ingame, float frameTime);
	const CodeWallStatus& GetCodeWallStatus();
	std::string GetErrorMessage();
}
