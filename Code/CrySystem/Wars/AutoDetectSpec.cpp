#include "StdAfx.h"

#if defined(WIN32) || defined(WIN64)

#include <intrin.h>
#include <d3d9.h>
#include <ddraw.h>
#include <DX10SDK/Include/dxgi.h>
#include <DX10SDK/Include/d3d10.h>
#include <map>
#include "CryCommon/CrySystem/ICryPak.h"
#include "CryCommon/CrySystem/IConsole.h"

#include "System.h"
#include "AutoDetectCPUTestSuit.h"
#include "AutoDetectSpec.h"


#if defined(WIN32) && defined(WIN64)
extern "C" void cpuid64(int*);
#endif


static bool IsAMD()
{
#if defined(WIN32) || defined(WIN64)
	int CPUInfo[4];
	char refID[] = "AuthenticAMD";
	__cpuid( CPUInfo, 0x00000000 );
	return ((int*) refID)[0] == CPUInfo[1] && ((int*) refID)[1] == CPUInfo[3] && ((int*) refID)[2] == CPUInfo[2];
#else
	return false;
#endif
}


static bool IsIntel()
{
#if defined(WIN32) || defined(WIN64)
	int CPUInfo[4];
	char refID[] = "GenuineIntel";
	__cpuid( CPUInfo, 0x00000000 );
	return ((int*) refID)[0] == CPUInfo[1] && ((int*) refID)[1] == CPUInfo[3] && ((int*) refID)[2] == CPUInfo[2];
#else
	return false;
#endif
}


static void TrimExcessiveWhiteSpaces(char* pStr)
{
	size_t len(strlen(pStr));	
	bool remove(true);
	for (size_t i(0); i<len; ++i)
	{
		if (pStr[i] == ' ' && remove)
		{
			size_t newlen(len-1);

			size_t j(i+1);
			for (; j<len && pStr[j] == ' '; ++j)
				--newlen;

			size_t ii(i);
			for (; j<len+1; ++j, ++ii)
				pStr[ii] = pStr[j];

			assert(newlen == strlen(pStr));
			len = newlen;
			remove = false;
		}
		else
			remove = pStr[i] == ' ';
	}

	if (len>0 && pStr[len-1] == ' ')
		pStr[len-1] = '\0';
}


static void GetCPUName(char* pName, size_t bufferSize)
{
	if (!pName || !bufferSize)
		return;

	char name[12*4+1];

	int CPUInfo[4];
	__cpuid(CPUInfo, 0x80000000);
	if (CPUInfo[0] >= 0x80000004)
	{
		__cpuid(CPUInfo, 0x80000002);
		((int*)name)[0] = CPUInfo[0];
		((int*)name)[1] = CPUInfo[1];
		((int*)name)[2] = CPUInfo[2];
		((int*)name)[3] = CPUInfo[3];

		__cpuid(CPUInfo, 0x80000003);
		((int*)name)[4] = CPUInfo[0];
		((int*)name)[5] = CPUInfo[1];
		((int*)name)[6] = CPUInfo[2];
		((int*)name)[7] = CPUInfo[3];

		__cpuid(CPUInfo, 0x80000004);
		((int*)name)[8] = CPUInfo[0];
		((int*)name)[9] = CPUInfo[1];
		((int*)name)[10] = CPUInfo[2];
		((int*)name)[11] = CPUInfo[3];
		
		name[48] = '\0';
	}
	else
		name[0] = '\0';

	int ret(_snprintf(pName, bufferSize, name));
	if (ret == bufferSize || ret < 0)
		pName[bufferSize - 1] = '\0';
}


void Win32SysInspect::GetOS(SPlatformInfo::EWinVersion& ver, bool& is64Bit, char* pName, size_t bufferSize)
{
	ver = SPlatformInfo::WinUndetected;
	is64Bit = false;

	if (pName && bufferSize)
		pName[0] = '\0';

	OSVERSIONINFOEX sysInfo;
	sysInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	if (GetVersionEx((OSVERSIONINFO*) &sysInfo))
	{
		if (sysInfo.dwPlatformId == VER_PLATFORM_WIN32_NT)
		{
			if (sysInfo.dwMajorVersion == 5)
			{
				if (sysInfo.dwMinorVersion == 0)
					ver = SPlatformInfo::Win2000;
				else if (sysInfo.dwMinorVersion == 1)
					ver = SPlatformInfo::WinXP;
				else if (sysInfo.dwMinorVersion == 2)
				{
					if (sysInfo.wProductType == VER_NT_WORKSTATION)
						ver = SPlatformInfo::WinXP; // 64 bit windows actually but this will be detected later anyway
					else if (sysInfo.wProductType == VER_NT_SERVER || sysInfo.wProductType == VER_NT_DOMAIN_CONTROLLER)
						ver = SPlatformInfo::WinSrv2003;
				}
			}
			else if (sysInfo.dwMajorVersion == 6)
			{
				if (sysInfo.dwMinorVersion == 0)
					ver = SPlatformInfo::WinVista;
			}
		}

		typedef BOOL (WINAPI *FP_GetSystemWow64Directory)(LPSTR, UINT);
		FP_GetSystemWow64Directory pgsw64d((FP_GetSystemWow64Directory) GetProcAddress(GetModuleHandle("kernel32"), "GetSystemWow64DirectoryA"));
		if (pgsw64d)
		{
			char str[MAX_PATH];
			if (!pgsw64d(str, sizeof(str)))
				is64Bit = GetLastError() != ERROR_CALL_NOT_IMPLEMENTED;
			else
				is64Bit = true;
		}

		if (pName && bufferSize)
		{
			const char* windowsVersionText(0);
			switch (ver)
			{
			case SPlatformInfo::Win2000:		windowsVersionText = "Windows 2000";				break;
			case SPlatformInfo::WinXP:			windowsVersionText = "Windows XP";					break;
			case SPlatformInfo::WinSrv2003:	windowsVersionText = "Windows Server 2003"; break;
			case SPlatformInfo::WinVista:		windowsVersionText = "Windows Vista";				break;
			default:												windowsVersionText = "Windows";							break;
			}

			char sptext[32]; sptext[0] = '\0';
			if (sysInfo.wServicePackMajor > 0)
				_snprintf(sptext, sizeof(sptext), "SP %d ", sysInfo.wServicePackMajor);

			int ret(_snprintf(pName, bufferSize, "%s %s %s(build %d.%d.%d)", windowsVersionText, is64Bit ? "64 bit" : "32 bit", 
				sptext, sysInfo.dwMajorVersion, sysInfo.dwMinorVersion, sysInfo.dwBuildNumber));
			if (ret == bufferSize || ret < 0)
				pName[bufferSize - 1] = '\0';
		}
	}
}


static void GetOSName(char* pName, size_t bufferSize)
{
	SPlatformInfo::EWinVersion winVer(SPlatformInfo::WinUndetected);
	bool is64bit(false);

	Win32SysInspect::GetOS(winVer, is64bit, pName, bufferSize);
}


bool Win32SysInspect::IsVistaKB940105Required()
{
#if defined(WIN32) && !defined(WIN64)
	OSVERSIONINFO osv;
	memset(&osv, 0, sizeof(osv));
	osv.dwOSVersionInfoSize = sizeof(osv);
	GetVersionEx(&osv);

	if (osv.dwMajorVersion != 6 || osv.dwMinorVersion != 0 || (osv.dwBuildNumber > 6000))
	{
		// This QFE only ever applies to Windows Vista RTM. Windows Vista SP1 already has this fix,
		// and earlier versions of Windows do not implement WDDM
		return false;
	}

	//MEMORYSTATUSEX mex;
	//memset(&mex, 0, sizeof(mex));
	//mex.dwLength = sizeof(mex);
	//GlobalMemoryStatusEx(&mex);

	//if (mex.ullTotalVirtual >= 4294836224)
	//{
	//	// If there is 4 GB of VA space total for this process, then we are a
	//	// 32-bit Large Address Aware application running on a Windows 64-bit OS.

	//	// We could be a 32-bit Large Address Aware application running on a
	//	// Windows 32-bit OS and get up to 3 GB, but that has stability implications.
	//	// Therefore, we recommend the QFE for all 32-bit versions of the OS.

	//	// No need for the fix unless the game is pushing 4 GB of VA
	//	return false;
	//}

	const char* sysFile = "dxgkrnl.sys";

	// Ensure we are checking the system copy of the file
	char sysPath[MAX_PATH];
	GetSystemDirectory(sysPath, sizeof(sysPath));

	strncat(sysPath, "\\drivers\\", MAX_PATH);
	strncat(sysPath, sysFile, MAX_PATH);

	char buf[2048];
	if (!GetFileVersionInfo(sysPath, 0, sizeof(buf), buf))
	{
		// This should never happen, but we'll assume it's a newer .sys file since we've
		// narrowed the test to a Windows Vista RTM OS.
		return false;
	}

	VS_FIXEDFILEINFO* ver;
	UINT size;
	if (!VerQueryValue(buf, "\\", (void**) &ver, &size) || size != sizeof(VS_FIXEDFILEINFO) || ver->dwSignature != 0xFEEF04BD)
	{
		// This should never happen, but we'll assume it's a newer .sys file since we've
		// narrowed the test to a Windows Vista RTM OS.
		return false;
	}

	// File major.minor.build.qfe version comparison
	//   WORD major = HIWORD( ver->dwFileVersionMS ); WORD minor = LOWORD( ver->dwFileVersionMS );
	//   WORD build = HIWORD( ver->dwFileVersionLS ); WORD qfe = LOWORD( ver->dwFileVersionLS );

	if (ver->dwFileVersionMS > MAKELONG(0, 6) || (ver->dwFileVersionMS == MAKELONG(0, 6) && ver->dwFileVersionLS >= MAKELONG(20648, 6000)))
	{
		// QFE fix version of dxgkrnl.sys is 6.0.6000.20648
		return false;
	}

	return true;
#else
	return false; // The QFE is not required for a 64-bit native application as it has 8 TB of VA
#endif
}


static void GetSystemMemory(uint64& totSysMem)
{
	typedef BOOL (WINAPI *FP_GlobalMemoryStatusEx)(LPMEMORYSTATUSEX);
	FP_GlobalMemoryStatusEx pgmsex((FP_GlobalMemoryStatusEx) GetProcAddress(GetModuleHandle("kernel32"), "GlobalMemoryStatusEx"));
	if (pgmsex)
	{
		MEMORYSTATUSEX memStats;
		memStats.dwLength = sizeof(memStats);
		if (pgmsex(&memStats))
			totSysMem = memStats.ullTotalPhys;
		else
			totSysMem = 0;
	}
	else
	{
		MEMORYSTATUS memStats;
		memStats.dwLength = sizeof(memStats);
		GlobalMemoryStatus(&memStats);
		totSysMem = memStats.dwTotalPhys;
	}
}


static bool IsVistaOrAbove()
{
	typedef BOOL (WINAPI* FP_VerifyVersionInfo) (LPOSVERSIONINFOEX, DWORD, DWORDLONG);
	FP_VerifyVersionInfo pvvi((FP_VerifyVersionInfo) GetProcAddress(GetModuleHandle("kernel32"), "VerifyVersionInfoA"));

	if (pvvi)
	{
		typedef ULONGLONG (WINAPI* FP_VerSetConditionMask) (ULONGLONG, DWORD, BYTE);
		FP_VerSetConditionMask pvscm((FP_VerSetConditionMask) GetProcAddress(GetModuleHandle("kernel32"), "VerSetConditionMask"));
		assert(pvscm);

		OSVERSIONINFOEX osvi;
		memset(&osvi, 0, sizeof(osvi));
		osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
		osvi.dwMajorVersion = 6;
		osvi.dwMinorVersion = 0;
		osvi.wServicePackMajor = 0;
		osvi.wServicePackMinor = 0;

		ULONGLONG mask(0);
		mask = pvscm(mask, VER_MAJORVERSION, VER_GREATER_EQUAL);
		mask = pvscm(mask, VER_MINORVERSION, VER_GREATER_EQUAL);
		mask = pvscm(mask, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);
		mask = pvscm(mask, VER_SERVICEPACKMINOR, VER_GREATER_EQUAL);

		if (pvvi(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR, mask))
			return true;
	}

	return false;
}


// Preferred solution to determine the number of available CPU cores, works reliably only on WinVista 32/64 and above
// See http://msdn2.microsoft.com/en-us/library/ms686694.aspx for reasons
static void GetNumCPUCoresGlpi(unsigned int& totAvailToSystem, unsigned int& totAvailToProcess)
{
	typedef BOOL (WINAPI *FP_GetLogicalProcessorInformation)(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, PDWORD);
	FP_GetLogicalProcessorInformation pglpi((FP_GetLogicalProcessorInformation) GetProcAddress(GetModuleHandle("kernel32"), "GetLogicalProcessorInformation"));
	if (pglpi && IsVistaOrAbove())
	{
		unsigned long bufferSize(0);
		pglpi(0, &bufferSize);

		void* pBuffer(malloc(bufferSize));

		SYSTEM_LOGICAL_PROCESSOR_INFORMATION* pLogProcInfo((SYSTEM_LOGICAL_PROCESSOR_INFORMATION*) pBuffer);
		if (pLogProcInfo && pglpi(pLogProcInfo, &bufferSize))
		{
			DWORD_PTR processAffinity, systemAffinity;
			GetProcessAffinityMask(GetCurrentProcess(), &processAffinity, &systemAffinity);

			unsigned long numEntries(bufferSize / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
			for (unsigned long i(0); i < numEntries; ++i)
			{
				switch (pLogProcInfo[i].Relationship)
				{
				case RelationProcessorCore:
					{
						++totAvailToSystem;
						if (pLogProcInfo[i].ProcessorMask & processAffinity)
							++totAvailToProcess;
					}
					break;

				default:
					break;
				}
			}
		}

		free(pBuffer);
	}
}


class CApicExtractor
{
public:
	CApicExtractor(unsigned int logProcsPerPkg = 1, unsigned int coresPerPkg = 1)
	{
		SetPackageTopology(logProcsPerPkg, coresPerPkg );
	}

	unsigned char SmtId(unsigned char apicId) const
	{
		return apicId & m_smtIdMask.mask;
	}

	unsigned char CoreId(unsigned char apicId) const
	{
		return (apicId & m_coreIdMask.mask) >> m_smtIdMask.width;
	}

	unsigned char PackageId(unsigned char apicId) const
	{
		return (apicId & m_pkgIdMask.mask) >> (m_smtIdMask.width + m_coreIdMask.width);
	}

	unsigned char PackageCoreId(unsigned char apicId) const
	{
		return (apicId & (m_pkgIdMask.mask | m_coreIdMask.mask)) >> m_smtIdMask.width;
	}

	unsigned int GetLogProcsPerPkg() const
	{
		return m_logProcsPerPkg;
	}

	unsigned int GetCoresPerPkg() const
	{
		return m_coresPerPkg;
	}

	void SetPackageTopology(unsigned int logProcsPerPkg, unsigned int coresPerPkg)
	{
		m_logProcsPerPkg   = (unsigned char) logProcsPerPkg;
		m_coresPerPkg      = (unsigned char) coresPerPkg;

		m_smtIdMask.width   = GetMaskWidth(m_logProcsPerPkg / m_coresPerPkg);
		m_coreIdMask.width  = GetMaskWidth(m_coresPerPkg);
		m_pkgIdMask.width   = 8 - (m_smtIdMask.width + m_coreIdMask.width);

		m_pkgIdMask.mask    = (unsigned char) (0xFF << (m_smtIdMask.width + m_coreIdMask.width));
		m_coreIdMask.mask   = (unsigned char) ((0xFF << m_smtIdMask.width) ^ m_pkgIdMask.mask);
		m_smtIdMask.mask    = (unsigned char) ~(0xFF << m_smtIdMask.width);
	}

private:
	unsigned char GetMaskWidth(unsigned char maxIds) const
	{
		--maxIds;
		unsigned char msbIdx(8);
		unsigned char msbMask(0x80);
		while (msbMask && !(msbMask & maxIds))
		{
			--msbIdx;
			msbMask >>= 1;
		}
		return msbIdx;
	}

	struct IdMask
	{
		unsigned char width;
		unsigned char mask;
	};

	unsigned char m_logProcsPerPkg;
	unsigned char m_coresPerPkg;
	IdMask m_smtIdMask;
	IdMask m_coreIdMask;
	IdMask m_pkgIdMask;
};


// Fallback solution for WinXP 32/64
static void GetNumCPUCoresApic(unsigned int& totAvailToSystem, unsigned int& totAvailToProcess)
{
	unsigned int numLogicalPerPhysical(1);
	unsigned int numCoresPerPhysical(1);

	int CPUInfo[4];
	__cpuid(CPUInfo, 0x00000001);
	if ((CPUInfo[3] & 0x10000000) != 0) // Hyperthreading / Multicore bit set
	{
		numLogicalPerPhysical = (CPUInfo[1] & 0x00FF0000) >> 16;

		if (IsIntel())
		{
			__cpuid(CPUInfo, 0x00000000);
			if (CPUInfo[0] >= 0x00000004)
			{
#if defined(WIN32) && !defined(WIN64)
				__asm
				{
					mov eax, 4
					xor ecx, ecx
					cpuid
					mov CPUInfo, eax
				}
#else
				CPUInfo[0] = 4;
				CPUInfo[2] = 0;
				cpuid64(CPUInfo);
#endif
				numCoresPerPhysical = ((CPUInfo[0] & 0xFC000000) >> 26) + 1;
			}
		}
		else if (IsAMD())
		{
			__cpuid(CPUInfo, 0x80000000);
			if (CPUInfo[0] >= 0x80000008)
			{
				__cpuid(CPUInfo, 0x80000008);
				if (CPUInfo[2] & 0x0000F000)
					numCoresPerPhysical = 1 << ((CPUInfo[2] & 0x0000F000) >> 12);
				else
					numCoresPerPhysical = (CPUInfo[2] & 0xFF) + 1;
			}
		}
	}

	HANDLE hCurProcess(GetCurrentProcess());
	HANDLE hCurThread(GetCurrentThread());

	const int c_maxLogicalProcessors(sizeof(DWORD_PTR) * 8);
	unsigned char apicIds[c_maxLogicalProcessors] = { 0 };
	unsigned char items(0);

	DWORD_PTR processAffinity, systemAffinity;
	GetProcessAffinityMask(hCurProcess, &processAffinity, &systemAffinity);

	if (systemAffinity == 1)
	{
		assert(numLogicalPerPhysical == 1);
		apicIds[items++] = 0;
	}
	else
	{
		if (processAffinity != systemAffinity)
			SetProcessAffinityMask(hCurProcess, systemAffinity);

		DWORD_PTR prevThreadAffinity(0);
		for (DWORD_PTR threadAffinity = 1; threadAffinity && threadAffinity <= systemAffinity; threadAffinity <<= 1)
		{
			if (systemAffinity & threadAffinity)
			{
				if (!prevThreadAffinity)
				{
					assert(!items);
					prevThreadAffinity = SetThreadAffinityMask(hCurThread, threadAffinity);
				}
				else
				{
					assert(items > 0);
					SetThreadAffinityMask(hCurThread, threadAffinity);
				}

				Sleep(0);

				int CPUInfo[4];
				__cpuid(CPUInfo, 0x00000001);
				apicIds[items++] = (unsigned char) ((CPUInfo[1] & 0xFF000000) >> 24);
			}
		}

		SetProcessAffinityMask(hCurProcess, processAffinity);
		SetThreadAffinityMask(hCurThread, prevThreadAffinity);
		Sleep(0);
	}

	CApicExtractor apicExtractor(numLogicalPerPhysical, numCoresPerPhysical);

	totAvailToSystem = 0;
	{
		unsigned char pkgCoreIds[c_maxLogicalProcessors] = { 0 };
		for (unsigned int i(0); i < items; ++i)
		{
			unsigned int j(0);
			for (; j < totAvailToSystem; ++j)
			{
				if (pkgCoreIds[j] == apicExtractor.PackageCoreId(apicIds[i]))
					break;
			}
			if (j == totAvailToSystem)
			{
				pkgCoreIds[j] = apicExtractor.PackageCoreId(apicIds[i]);
				++totAvailToSystem;
			}
		}
	}

	totAvailToProcess = 0;
	{
		unsigned char pkgCoreIds[c_maxLogicalProcessors] = { 0 };
		for (unsigned int i(0); i < items; ++i)
		{
			if (processAffinity & ((DWORD_PTR) 1 << i))
			{
				unsigned int j(0);
				for (; j < totAvailToProcess; ++j)
				{
					if (pkgCoreIds[j] == apicExtractor.PackageCoreId(apicIds[i]))
						break;
				}
				if (j == totAvailToProcess)
				{
					pkgCoreIds[j] = apicExtractor.PackageCoreId(apicIds[i]);
					++totAvailToProcess;
				}
			}
		}
	}
}


void Win32SysInspect::GetNumCPUCores(unsigned int& totAvailToSystem, unsigned int& totAvailToProcess)
{
	totAvailToSystem = 0;
	totAvailToProcess = 0;

	GetNumCPUCoresGlpi(totAvailToSystem, totAvailToProcess);

	if (!totAvailToSystem)
		GetNumCPUCoresApic(totAvailToSystem, totAvailToProcess);
}


bool Win32SysInspect::IsDX10Supported()
{
	if (!IsVistaOrAbove())
		return false;

	typedef HRESULT (WINAPI *FP_CreateDXGIFactory)(REFIID, void**);
	FP_CreateDXGIFactory pCDXGIF((FP_CreateDXGIFactory) GetProcAddress(LoadLibrary("dxgi.dll"), "CreateDXGIFactory"));

	bool dx10AdapterFound(false);
	IDXGIFactory* pFactory(0);
	if (pCDXGIF && SUCCEEDED(pCDXGIF(__uuidof(IDXGIFactory), (void**) &pFactory)))
	{
		typedef HRESULT (WINAPI *FP_D3D10CreateDevice)(IDXGIAdapter*, D3D10_DRIVER_TYPE, HMODULE, UINT, UINT, ID3D10Device**);
		FP_D3D10CreateDevice pD3D10CC((FP_D3D10CreateDevice) GetProcAddress(LoadLibrary("d3d10.dll"), "D3D10CreateDevice"));

		if (pD3D10CC)
		{
			unsigned int nAdapter(0);
			IDXGIAdapter* pAdapter(0);
			while (pFactory->EnumAdapters(nAdapter, &pAdapter) != DXGI_ERROR_NOT_FOUND)
			{
				if (pAdapter)
				{
					//DXGI_ADAPTER_DESC desc;
					//if (SUCCEEDED(pAdapter->GetDesc(&desc)))
					//{
					//	if (S_OK == pAdapter->CheckInterfaceSupport(__uuidof(ID3D10Device), 0))
					//		ret = true;
					//}

					ID3D10Device* pDevice(0);
					if (SUCCEEDED(pD3D10CC(pAdapter, D3D10_DRIVER_TYPE_HARDWARE, 0, 0, D3D10_SDK_VERSION, &pDevice)))
						dx10AdapterFound = true;

					SAFE_RELEASE(pDevice);
					SAFE_RELEASE(pAdapter);
				}
				if (dx10AdapterFound)
					break;
				++nAdapter;
			}
		}
	}
	SAFE_RELEASE(pFactory);
	return dx10AdapterFound;
}


void Win32SysInspect::GetGPUInfo(char* pName, size_t bufferSize, unsigned int& vendorID, unsigned int& deviceID, unsigned int& totLocalVidMem, bool& supportsSM20orAbove)
{
	if (pName && !bufferSize)
		return;

	{
		if (pName)
			pName[0] = '\0';
		vendorID = 0;
		deviceID = 0;
		supportsSM20orAbove = false;

		typedef IDirect3D9* (WINAPI *FP_Direct3DCreate9)(UINT);
		FP_Direct3DCreate9 pd3dc9((FP_Direct3DCreate9) GetProcAddress(LoadLibrary("d3d9.dll"), "Direct3DCreate9"));
		IDirect3D9* pD3D(pd3dc9 ? pd3dc9(D3D_SDK_VERSION) : 0);
		if (pD3D)
		{
			D3DADAPTER_IDENTIFIER9 adapterIdent;
			if (SUCCEEDED(pD3D->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &adapterIdent)))
			{
				if (pName)
				{
					int ret(_snprintf(pName, bufferSize, "%s", adapterIdent.Description));
					if (ret == bufferSize || ret < 0)
						pName[bufferSize - 1] = '\0';
				}
				vendorID = adapterIdent.VendorId;
				deviceID = adapterIdent.DeviceId;
			}

			D3DCAPS9 devCaps;
			if (SUCCEEDED(pD3D->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &devCaps)))
			{
				unsigned int vsMajor(D3DSHADER_VERSION_MAJOR(devCaps.VertexShaderVersion));
				//unsigned int vsMinor(D3DSHADER_VERSION_MINOR(devCaps.VertexShaderVersion));
				unsigned int psMajor(D3DSHADER_VERSION_MAJOR(devCaps.PixelShaderVersion));
				//unsigned int psMinor(D3DSHADER_VERSION_MINOR(devCaps.PixelShaderVersion));
				supportsSM20orAbove = vsMajor >= 2 && psMajor >= 2;
			}

			SAFE_RELEASE(pD3D);
		}
	}
	{
		totLocalVidMem = 0;

		typedef HRESULT (WINAPI *FP_DirectDrawCreate)(GUID FAR*, LPDIRECTDRAW FAR* , IUnknown* );		
		FP_DirectDrawCreate pddc((FP_DirectDrawCreate) GetProcAddress(LoadLibrary("ddraw.dll"), "DirectDrawCreate"));		
		IDirectDraw* pDD(0);
		if (pddc)
			pddc(0, &pDD, 0);
		if (pDD)
		{
			IDirectDraw7* pDD7(0);
			if (SUCCEEDED(pDD->QueryInterface(IID_IDirectDraw7, (void**) &pDD7)) && pDD7)
			{
				DDSCAPS2 sDDSCaps2;
				ZeroMemory(&sDDSCaps2, sizeof(sDDSCaps2));
				sDDSCaps2.dwCaps = DDSCAPS_LOCALVIDMEM;

				unsigned long localMem(0);
				pDD7->GetAvailableVidMem(&sDDSCaps2, &localMem, 0);
				SAFE_RELEASE(pDD7);
				totLocalVidMem = localMem;
			}

			SAFE_RELEASE(pDD);
		}
	}
}


class CGPURating
{
public:
	CGPURating();
	~CGPURating();

	int GetRating(unsigned int vendorId, unsigned int deviceId) const;	

private:
	struct SGPUID
	{
		SGPUID(unsigned int vendorId, unsigned int deviceId)
		: vendor(vendorId)
		, device(deviceId)
		{
		}

		bool operator < (const SGPUID& rhs) const
		{
			if (vendor == rhs.vendor)
				return device < rhs.device;
			else
				return vendor < rhs.vendor;
		}

		unsigned int vendor;
		unsigned int device;
	};

	typedef std::map<SGPUID, int> GPURatingMap;

private:
	GPURatingMap m_gpuRatingMap;

};


static size_t SafeReadLine(ICryPak* pPak, FILE* f, char* buffer, size_t bufferSize)
{
	assert(buffer && bufferSize);
	
	// read up to (bufferSize - 1) characters from stream
	size_t len(0); int c(0);
	while ((c = pPak->Getc(f)) != EOF && len < bufferSize - 1)
	{
		if (c == '\r' || c == '\n')
			break;

		buffer[len] = (char) c;
		++len;
	}

	// write trailing zero
	buffer[len] = '\0';

	// read to end of line if necessary
	if (c != EOF && c != '\r' && c != '\n')
	{
		while ((c = pPak->Getc(f)) != EOF)
		{
			if (c == '\r' || c == '\n')
				break;
		}
	}

	// handle CR/LF for file coming from different platforms
	if (c == '\r')
	{
		c = pPak->Getc(f);
		if (c != EOF && c != '\n')
			pPak->Ungetc(c, f);
	}

	return len;
}


#define BUILDPATH_GPURATING(x) "config/gpu/"x

CGPURating::CGPURating()
{
	ICryPak* pPak(gEnv->pCryPak);

	_finddata_t fd;
	intptr_t h(pPak->FindFirst(BUILDPATH_GPURATING("*.txt"), &fd));
	if (h != -1)
	{
		do
		{
			char filename[128];
			_snprintf(filename, sizeof(filename), BUILDPATH_GPURATING("%s"), fd.name);

			FILE* f(pPak->FOpen(filename, "rb"));
			if (f)
			{
				size_t lineNr(0);
				while (!pPak->FEof(f))
				{
					char line[1024]; line[0] = '\0';
					size_t len(SafeReadLine(pPak, f, line, sizeof(line)));
					++lineNr;

					if (len > 2 && line[0] != '/' && line[1] != '/')
					{
						unsigned int vendorId(0), deviceId(0); int rating(0);
						if (_snscanf(line, sizeof(line), "%x,%x,%d", &vendorId, &deviceId, &rating) == 3)
						{
							GPURatingMap::iterator it(m_gpuRatingMap.find(SGPUID(vendorId, deviceId)));
							if (it == m_gpuRatingMap.end())
								m_gpuRatingMap.insert(GPURatingMap::value_type(SGPUID(vendorId, deviceId), rating));
							else
								CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, 
									"%s line %d contains a multiple defined GPU rating!", filename, lineNr);
						}
						else
							CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, 
								"%s line %d contains incomplete GPU rating!", filename, lineNr);
					}
				}

				pPak->FClose(f);
			}
		} while (0 == pPak->FindNext(h, &fd));

		pPak->FindClose(h);
	}
}


CGPURating::~CGPURating()
{
}


int CGPURating::GetRating(unsigned int vendorId, unsigned int deviceId) const
{
	GPURatingMap::const_iterator it(m_gpuRatingMap.find(SGPUID(vendorId, deviceId)));
	if (it != m_gpuRatingMap.end())
		return (*it).second;
	else
		return 0;
}


int Win32SysInspect::GetGPURating(unsigned int vendorId, unsigned int deviceId)
{
	CGPURating gpuRatingDb;
	return gpuRatingDb.GetRating(vendorId, deviceId);
}


static int SafeMemoryThreshold(int memMB)
{
	return (memMB * 8) / 10;
}


static int GetFinalSpecValue(int cpuRating, unsigned int totSysMemMB, int gpuRating, unsigned int totVidMemMB, ESystemConfigSpec maxConfigSpec)
{
	int sysMemRating(2);
	if (totSysMemMB >= SafeMemoryThreshold(2048))
		sysMemRating = 3;

	cpuRating = sysMemRating < cpuRating ? sysMemRating : cpuRating;

	if (totVidMemMB < SafeMemoryThreshold(512))
		gpuRating = gpuRating > 2 ? 2 : gpuRating;

	int finalRating(4);
	if (cpuRating < 3 || gpuRating < 4)
		finalRating = cpuRating < gpuRating ? cpuRating: gpuRating;

	return min(finalRating, (int) maxConfigSpec);
}


void CSystem::AutoDetectSpec()
{
	CryLogAlways("Running machine spec auto detect (%d bit)...", sizeof(void*) << 3);

	char tempBuf[512];

	// get OS
	SPlatformInfo::EWinVersion winVer(SPlatformInfo::WinUndetected);
	bool is64bit(false);
	Win32SysInspect::GetOS(winVer, is64bit, tempBuf, sizeof(tempBuf));
	CryLogAlways("- %s", tempBuf);

	// get system memory
	uint64 totSysMem(0);
	GetSystemMemory(totSysMem);
	CryLogAlways("- System memory");
	CryLogAlways("--- %d MB", totSysMem >> 20);

	// get CPU name
	GetCPUName(tempBuf, sizeof(tempBuf));
	TrimExcessiveWhiteSpaces(tempBuf);
	CryLogAlways("- %s", tempBuf);

	// get number of CPU cores
	unsigned int numSysCores(1), numProcCores(1);
	Win32SysInspect::GetNumCPUCores(numSysCores, numProcCores);
	CryLogAlways("--- Number of available cores: %d (out of %d)", numProcCores, numSysCores);

	// run CPU benchmark
	double minTestRuntime(100000);
	const int numPerfTestRuns(3);

	CCPUTestSuite cpuTestSuite;
	int prevThreadPrio(SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL));
	for (int i(0); i < numPerfTestRuns; ++i)
	{
		float time(gEnv->pTimer->GetAsyncCurTime());
		cpuTestSuite.RunTest();
		time = gEnv->pTimer->GetAsyncCurTime() - time;
		float curTestRuntime(1000.0 * time);
		minTestRuntime = minTestRuntime > curTestRuntime ? curTestRuntime : minTestRuntime;
	}
	SetThreadPriority(GetCurrentThread(), prevThreadPrio);

	// get CPU rating
	double timeMult(1);
	if (numProcCores > 1)
		timeMult *= 0.7;
	if (numProcCores > 2)
		timeMult *= pow(0.9, (double) (numProcCores - 2));

	double timeForRating(minTestRuntime * timeMult);

	struct SCPUSpecInfo { double maxTime; int rating; };
	static const SCPUSpecInfo cpuSpecs[2][3] = 
	{ 
		{ {350.0, 3}, {800.0, 2}, {-1, 1} }, // XP
		{ {350.0, 3}, {720.0, 2}, {-1, 1} }  // Vista
	};

	int cpuSpecIdx(0);
	int cpuOSIdx(winVer == SPlatformInfo::WinVista ? 1 : 0);
	for (; cpuSpecIdx < sizeof(cpuSpecs[0]) / sizeof(cpuSpecs[0][0]) - 1; ++cpuSpecIdx)
	{
		if (timeForRating < cpuSpecs[cpuOSIdx][cpuSpecIdx].maxTime)
			break;
	}

	int cpuRating(cpuSpecs[cpuOSIdx][cpuSpecIdx].rating);

	CryLogAlways("--- Benchmark");
	CryLogAlways("----- Best time after %d runs: %.2f ms", numPerfTestRuns, minTestRuntime);
	CryLogAlways("----- Time multiplier: %.2f", timeMult);
	CryLogAlways("----- Final time: %.2f ms", timeForRating);
	CryLogAlways("--- Rating: CPU class %d", cpuRating);

	// get GPU info
	unsigned int gpuVendorId(0), gpuDeviceId(0), totVidMem(0);
	bool supportsSM20orAbove(false);
	Win32SysInspect::GetGPUInfo(tempBuf, sizeof(tempBuf), gpuVendorId, gpuDeviceId, totVidMem, supportsSM20orAbove);
	bool supportsDX10(Win32SysInspect::IsDX10Supported());

	CryLogAlways("- %s (vendor = 0x%.4x, device = 0x%.4x)", tempBuf, gpuVendorId, gpuDeviceId);
	CryLogAlways("--- Video memory: %d MB", totVidMem >> 20);
	CryLogAlways("--- Minimum SM 2.0 support: %s", (supportsSM20orAbove == true) ? "yes" : "no");
	CryLogAlways("--- DX10 supported: %s", (supportsDX10 == true) ? "yes" : "no");	

	// get GPU rating
	int gpuRating(Win32SysInspect::GetGPURating(gpuVendorId, gpuDeviceId));

	if (gpuRating > 0)
		CryLogAlways("--- Rating: GPU class %d", gpuRating);
	else
	{
		CryLogAlways("--- Rating: GPU not rated yet!");
		gpuRating = supportsSM20orAbove ? 3 : 1;
	}

	// get final rating
	int finalSpecValue(GetFinalSpecValue(cpuRating, totSysMem >> 20, gpuRating, totVidMem >> 20, GetMaxConfigSpec()));
	CryLogAlways("- Final rating: Machine class %d", finalSpecValue);
	
	m_sys_spec->Set(finalSpecValue);
}


#else

#include "System.h"

void CSystem::AutoDetectSpec()
{
}


#endif
