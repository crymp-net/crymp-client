#pragma once

namespace CodeWall {
	enum ECodeWall {
		eCW_CIG = 1,
		eCW_ACG = 2,
		eCW_DACL = 4,
		eCW_SH = 8
	};

	struct CodeWallStatus {
		bool changed = true;
		int status = 0;
		int shDiscrepancies = 0;
		double shLastDiscrepancy = 0.0;
	};

	int  InitializeCodeWallInternal();
	int  InitializeCodeWallExternal();
	const CodeWallStatus& UpdateCodeWall(bool ingame, float frameTime);
	const CodeWallStatus& GetCodeWallStatus();
}