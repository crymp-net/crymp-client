#pragma once

namespace CodeWall {
	enum ECodeWall {
		eCW_CLK = 1,
		eCW_CIG = 2,
		eCW_ACG = 4,
		eCW_DACL = 8,

		eCW_Default = eCW_CLK
	};

	struct CodeWallStatus {
		bool changed = true;
		int status = (int)eCW_Default;

		int clkDiscrepancies = 0;
		double clkLastDiscrepancy = 0.0;
	};

	int  InitializeCodeWallInternal();
	int  InitializeCodeWallExternal();
	const CodeWallStatus& UpdateCodeWall(bool ingame, float frameTime);
	const CodeWallStatus& GetCodeWallStatus();
}