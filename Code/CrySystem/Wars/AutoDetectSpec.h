#ifndef _AUTODETECTSPEC_
#define _AUTODETECTSPEC_

#pragma once


#if defined(WIN32) || defined(WIN64)

// exposed AutoDetectSpec() helper functions for reuse in CrySystem
namespace Win32SysInspect
{
	void GetNumCPUCores(unsigned int& totAvailToSystem, unsigned int& totAvailToProcess);
	bool IsDX10Supported();
	void GetGPUInfo(char* pName, size_t bufferSize, unsigned int& vendorID, unsigned int& deviceID, unsigned int& totLocalVidMem, bool& supportsSM20orAbove);
	int GetGPURating(unsigned int vendorId, unsigned int deviceId);
	void GetOS(SPlatformInfo::EWinVersion& ver, bool& is64Bit, char* pName, size_t bufferSize);
	bool IsVistaKB940105Required();
}

#endif // #if defined(WIN32) || defined(WIN64)


#endif // #ifndef _AUTODETECTSPEC_