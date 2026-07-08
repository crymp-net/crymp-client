#include <windows.h>
#include <winhttp.h>
#include <winnls.h>

#include <processthreadsapi.h>
#include <aclapi.h>
#include <sddl.h>
#include <ctime>
#include <cmath>

#include "CodeWall.h"

static CodeWall::CodeWallStatus status;

int CodeWall::InitializeCodeWallInternal() {
	HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
	if (!hKernel32) return status.status;

	typedef BOOL(WINAPI* PFN_SetProcessMitigationPolicy)(
		PROCESS_MITIGATION_POLICY MitigationPolicy,
		PVOID                    lpBuffer,
		SIZE_T                   dwLength
		);

	PFN_SetProcessMitigationPolicy pSetProcessMitigationPolicy =
		(PFN_SetProcessMitigationPolicy)GetProcAddress(hKernel32, "SetProcessMitigationPolicy");

	// 1. Enable Code Integrity Guard (CIG)
	if (pSetProcessMitigationPolicy && (status.status & eCW_CIG) == 0) {
		// ProcessSignaturePolicy is enum value 8
		PROCESS_MITIGATION_BINARY_SIGNATURE_POLICY sigPolicy = { 0 };
		sigPolicy.MicrosoftSignedOnly = 1;

		if (pSetProcessMitigationPolicy((PROCESS_MITIGATION_POLICY)8, &sigPolicy, sizeof(sigPolicy))) {
			status.changed = true;
			status.status |= eCW_CIG;
		}
	}

	// 2. Enable Arbitrary Code Guard (ACG)
	if (pSetProcessMitigationPolicy && (status.status & eCW_ACG) == 0) {
		// ProcessDynamicCodePolicy is enum value 2
		PROCESS_MITIGATION_DYNAMIC_CODE_POLICY dynamicCodePolicy = { 0 };
		dynamicCodePolicy.ProhibitDynamicCode = 1;

		if (pSetProcessMitigationPolicy((PROCESS_MITIGATION_POLICY)2, &dynamicCodePolicy, sizeof(dynamicCodePolicy))) {
			status.changed = true;
			status.status |= eCW_ACG;
		}
	}

	return status.status;
}

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

		if (pSetSecurityInfo && (status.status & eCW_DACL) == 0) {
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
						status.changed = true;
						status.status |= eCW_DACL;
					}
					LocalFree(pNewDacl);
				}
				FreeSid(pEveryoneSid);
			}
		}
		FreeLibrary(hAdvapi32);
	}
	return status.status;
}

const CodeWall::CodeWallStatus& CodeWall::GetCodeWallStatus() {
	return status;
}

const CodeWall::CodeWallStatus& CodeWall::UpdateCodeWall(bool ingame, float frameTime) {
	static double elapsed = 0.0;
	static double lastCheckClock = 0.0;
	static time_t lastCheckTime = time(NULL);

	elapsed += (double)frameTime;

	int before = status.status;
	if (ingame) {
		if (elapsed - lastCheckClock >= 10.0f) {
			time_t now = time(NULL);
			time_t elapsedTime = now - lastCheckTime;
			status.clkLastDiscrepancy = elapsedTime - (elapsed - lastCheckClock);

			if (std::abs(status.clkLastDiscrepancy) > 0.5) {
				status.clkDiscrepancies++;
			}
			else {
				status.clkDiscrepancies = 0;
			}

			lastCheckClock = elapsed;
			lastCheckTime = now;
		}
	} else {
		time_t now = time(NULL);
		status.clkDiscrepancies = 0;
		status.clkLastDiscrepancy = 0.0;
		lastCheckClock = elapsed;
		lastCheckTime = now;
	}

	if (status.clkDiscrepancies >= 3) {
		status.status &= ~(int)eCW_CLK;
	} else {
		status.status |= (int)eCW_CLK;
	}

	status.changed = status.status != before;
	return status;
}

std::string CodeWall::GetErrorMessage() {
	static std::string_view message{ "Sex*mkgo*neoyd-~*knboxo*~e*yox|ox*yoixc~s*zefcis" };
	std::string copy(message);
	for (char& c : copy) {
		c ^= 0x0A;
	}
	return copy;
}