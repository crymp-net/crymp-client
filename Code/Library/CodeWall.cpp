#include <windows.h>
#include <aclapi.h>
#include <ctime>

#include "CodeWall.h"

static CodeWall::CodeWallStatus gStatus;


int CodeWall::InitializeCodeWallExternal() {
	// 3. Apply Restricted DACL to the process handle
	// We deny VM Write, VM Operation, and Remote Thread creation to the current user/everyone else.
	HMODULE hAdvapi32 = LoadLibraryA("advapi32.dll");
	if (hAdvapi32) {
		typedef DWORD(WINAPI* PFN_SetSecurityInfo)(
			HANDLE               handle,
			SE_OBJECT_TYPE       ObjectType,
			SECURITY_INFORMATION SecurityInfo,
			PSID                 psidOwner,
			PSID                 psidGroup,
			PACL                 pDacl,
			PACL                 pSacl
			);

		PFN_SetSecurityInfo pSetSecurityInfo = (PFN_SetSecurityInfo)GetProcAddress(hAdvapi32, "SetSecurityInfo");

		if (pSetSecurityInfo && (gStatus.status & eCW_DACL) == 0) {
			// "D:" specifies a DACL. 
			// "D;OICI;WD;;;WD)" format can be customized, but a highly effective approach 
			// is building an empty or highly restrictive explicit ACL.
			// For simplicity and effectiveness, we create an explicit DACL that denies critical rights to Everyone (WD).

			PSID pEveryoneSid = NULL;
			SID_IDENTIFIER_AUTHORITY SIDAuthWorld = SECURITY_WORLD_SID_AUTHORITY;

			if (AllocateAndInitializeSid(&SIDAuthWorld, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &pEveryoneSid)) {
				EXPLICIT_ACCESSA ea;
				ZeroMemory(&ea, sizeof(EXPLICIT_ACCESSA));

				// Deny memory manipulation and thread injection rights
				ea.grfAccessPermissions = PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_CREATE_THREAD | PROCESS_DUP_HANDLE;
				ea.grfAccessMode = DENY_ACCESS;
				ea.grfInheritance = NO_INHERITANCE;
				ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
				ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
				ea.Trustee.ptstrName = (LPSTR)pEveryoneSid;

				PACL pNewDacl = NULL;
				if (SetEntriesInAclA(1, &ea, NULL, &pNewDacl) == ERROR_SUCCESS) {
					// Apply the DACL to the current process
					DWORD secResult = pSetSecurityInfo(
						GetCurrentProcess(),
						SE_KERNEL_OBJECT,
						DACL_SECURITY_INFORMATION,
						NULL, NULL, pNewDacl, NULL
					);

					if (secResult == ERROR_SUCCESS) {
						gStatus.changed = true;
						gStatus.status |= eCW_DACL;
					}
					LocalFree(pNewDacl);
				}
				FreeSid(pEveryoneSid);
			}
		}
		FreeLibrary(hAdvapi32);
	}
	return gStatus.status;
}

const CodeWall::CodeWallStatus& CodeWall::GetCodeWallStatus() {
	return gStatus;
}

const CodeWall::CodeWallStatus& CodeWall::UpdateCodeWall(bool enabled, bool ingame, float frameTime) {
	static bool enabledBefore = false;
	gStatus.elapsed += (double)frameTime;

	if (enabled && !enabledBefore) {
		// Reset the state when we go from disabled state to enabled
		gStatus.clkLastDiscrepancy = 0.0;
		gStatus.clkDiscrepancies = 0;
		gStatus.clkLastClock = gStatus.elapsed;
		gStatus.clkLastTime = time(NULL);
	}

	enabledBefore = enabled;

	int before = gStatus.status;
	if (enabled) {
		// Only check when CodeWall is enabled
		if (ingame) {
			// CLK is checked only when player is in-game
			if (gStatus.elapsed - gStatus.clkLastClock >= 10.0) {
				time_t now = time(NULL);
				time_t elapsedTime = now - gStatus.clkLastTime;
				gStatus.clkLastDiscrepancy = elapsedTime - (gStatus.elapsed - gStatus.clkLastClock);

				if (std::abs(gStatus.clkLastDiscrepancy) > 0.5) {
					gStatus.clkDiscrepancies++;
				} else {
					gStatus.clkDiscrepancies = 0;
				}

				gStatus.clkLastClock = gStatus.elapsed;
				gStatus.clkLastTime = now;
			}

			if (gStatus.clkDiscrepancies >= 3) {
				gStatus.status &= ~(int)eCW_CLK;
			} else {
				gStatus.status |= (int)eCW_CLK;
			}
		}
	}

	gStatus.changed = gStatus.status != before;
	return gStatus;
}

std::string CodeWall::GetErrorMessage() {
	static std::string_view message{ "Sex*mkgo*neoyd-~*knboxo*~e*yox|ox*yoixc~s*zefcis" };
	std::string copy(message);
	for (char& c : copy) {
		c ^= 0x0A;
	}
	return copy;
}
