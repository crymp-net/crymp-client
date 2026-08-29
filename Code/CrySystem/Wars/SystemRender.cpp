
//////////////////////////////////////////////////////////////////////
//
//	Crytek CryENGINE Source code
// 
//	File: SystemRender.cpp
//  Description: CryENGINE system core 
// 
//	History:
//	-Jan 6,2004: split from system.cpp
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "System.h"

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#endif

#if defined(PS3)
#include <IJobManSPU.h>
#endif

#include "CryCommon/CryRenderer/IRenderer.h"
#include "CryCommon/CryRenderer/IRenderAuxGeom.h"
#include "CryCommon/CrySystem/IProcess.h"
#include "Log.h"
#include "XConsole.h"
#include "CryCommon/Cry3DEngine/I3DEngine.h"
#include "CryCommon/CryAISystem/IAISystem.h"
#include "CryCommon/CrySoundSystem/ISound.h"
#include "CryLibrary.h"
#include "CryCommon/CrySystem/IBudgetingSystem.h"
#include "PhysRenderer.h"
#include "CryCommon/CryScriptSystem/IScriptSystem.h"
#include "CryCommon/CryEntitySystem/IEntitySystem.h"

#include "CrySizerStats.h"
#include "CrySizerImpl.h"
#include "CryCommon/CrySystem/ITestSystem.h"							// ITestSystem
#include "ThreadProfiler.h"
#include "CryCommon/CrySystem/ITextModeConsole.h"

#ifndef EXCLUDE_GPU_PARTICLE_PHYSICS
#include "IPhysicsGPU.h"   
#endif

#ifdef WIN32
#include <ddraw.h>
extern HRESULT GetDXVersion( DWORD* pdwDirectXVersion, TCHAR* strDirectXVersion, int cchDirectXVersion );
#endif

/////////////////////////////////////////////////////////////////////////////////
int CSystem::AutoDetectRenderer(char *Vendor, char *Device)
{
	int nRenderer = R_DX9_RENDERER;
#ifdef WIN32
  HRESULT hr;
  LPDIRECTDRAW7 pDD;
  HINSTANCE hDDInstance;

  // Dynamically load the DLL.
  typedef HRESULT (WINAPI *DD_CREATE_FUNC)(GUID* lpGUID, void* lplpDD, REFIID iid,IUnknown* pUnkOuter);
  typedef HRESULT (WINAPI *DD_ENUM_FUNC  )(LPDDENUMCALLBACKEXA lpCallback,void* lpContext,DWORD dwFlags);
  DD_CREATE_FUNC ddCreateFunc;
  DD_ENUM_FUNC ddEnumFunc;
  hDDInstance = CryLoadLibrary( "ddraw.dll" );
  if( hDDInstance == NULL )
    return nRenderer;
  ddCreateFunc = (DD_CREATE_FUNC)GetProcAddress( hDDInstance, "DirectDrawCreateEx"     );
  ddEnumFunc   = (DD_ENUM_FUNC  )GetProcAddress( hDDInstance, "DirectDrawEnumerateExA" );
  if( !ddCreateFunc || !ddEnumFunc )
    return nRenderer;
  if(FAILED(hr=ddCreateFunc(NULL,&pDD,IID_IDirectDraw7,NULL)))
  {
    FreeLibrary(hDDInstance);
    return nRenderer;
  }

  DDDEVICEIDENTIFIER2 DeviceIdentifier;
  memset(&DeviceIdentifier,0,sizeof(DeviceIdentifier));
  if(FAILED(hr=pDD->GetDeviceIdentifier(&DeviceIdentifier,0)))
  {
    pDD->Release();
    FreeLibrary(hDDInstance);
    return nRenderer;
  }
  bool bUnsupported = false;
  switch(DeviceIdentifier.dwVendorId)
  {
    // 3DFX
    case 0x1142:
    case 0x10d9:
    case 0x121a:
      strcpy(Vendor, "3DFX");
      bUnsupported = true;
      switch (DeviceIdentifier.dwDeviceId)
      {
        case 0x0001: strcpy(Device, "Voodoo 1"); break;
        case 0x0002: strcpy(Device, "Voodoo 2"); break;
        case 0x0003:
        case 0x0004: strcpy(Device, "Banshee");  break;
        case 0x0005: strcpy(Device, "Voodoo 3"); break;
        case 0x0007:
        case 0x0009: strcpy(Device, "Voodoo 4"); break;
        case 0x643d: strcpy(Device, "Rush (Alliance)"); break;
        case 0x8626: strcpy(Device, "Rush (Macronix)"); break;
        default:     strcpy(Device, "Unknown");
      }
  	  break;

    case 0x104a:
      strcpy(Vendor, "ST Microelectronics");
      bUnsupported = true;
      switch (DeviceIdentifier.dwDeviceId)
      {
        case 0x0010: strcpy(Device, "Kyro I/II"); break;
        default:     strcpy(Device, "Unknown");
      }
  	  break;

    case 0x1002:
      strcpy(Vendor, "ATI");
      nRenderer = R_DX9_RENDERER;
      switch (DeviceIdentifier.dwDeviceId)
      {
        case 0x4158: strcpy(Device, "Mach 32"); bUnsupported = true; break;
        case 0x4337: strcpy(Device, "IGP 340M"); bUnsupported = true; break;
        case 0x4354: strcpy(Device, "Mach 64");  bUnsupported = true; break;
        case 0x4358: strcpy(Device, "Mach 64"); bUnsupported = true; break;
        case 0x4554: strcpy(Device, "Mach 64"); bUnsupported = true; break;
        case 0x4654: strcpy(Device, "Mach 64"); bUnsupported = true; break;
        case 0x4742: strcpy(Device, "Rage Pro"); bUnsupported = true; break;
        case 0x4744: strcpy(Device, "Rage Pro"); bUnsupported = true; break;
        case 0x4747: strcpy(Device, "Rage Pro"); bUnsupported = true; break;
        case 0x4749: strcpy(Device, "Rage Pro"); bUnsupported = true; break;
        case 0x474c: strcpy(Device, "Rage XC"); bUnsupported = true; break;
        case 0x474d: strcpy(Device, "Rage XL"); bUnsupported = true; break;
        case 0x474e: strcpy(Device, "Rage XC"); bUnsupported = true; break;
        case 0x474f: strcpy(Device, "Rage XL"); bUnsupported = true; break;
        case 0x4750: strcpy(Device, "Rage Pro"); bUnsupported = true; break;
        case 0x4751: strcpy(Device, "Rage Pro"); bUnsupported = true; break;
        case 0x4752: strcpy(Device, "Rage XL"); bUnsupported = true; break;
        case 0x4753: strcpy(Device, "Rage XC"); bUnsupported = true; break;
        case 0x4754: strcpy(Device, "Rage II"); bUnsupported = true; break;
        case 0x4755: strcpy(Device, "Rage II+"); bUnsupported = true; break;
        case 0x4756: strcpy(Device, "Rage IIC"); bUnsupported = true; break;
        case 0x4757: strcpy(Device, "Rage IIC"); bUnsupported = true; break;
        case 0x4758: strcpy(Device, "Mach 64"); bUnsupported = true; break;
        case 0x4759: strcpy(Device, "Rage IIC"); bUnsupported = true; break;
        case 0x475a: strcpy(Device, "Rage IIC"); bUnsupported = true; break;
        case 0x4c42: strcpy(Device, "Rage LT Pro"); bUnsupported = true; break;
        case 0x4c44: strcpy(Device, "Rage LT Pro"); bUnsupported = true; break;
        case 0x4c47: strcpy(Device, "Rage LT"); bUnsupported = true; break;
        case 0x4c49: strcpy(Device, "Rage LT Pro"); bUnsupported = true; break;
        case 0x4c50: strcpy(Device, "Rage LT Pro"); bUnsupported = true; break;
        case 0x4c51: strcpy(Device, "Rage LT Pro"); bUnsupported = true; break;
        case 0x4c45: strcpy(Device, "Rage Mobility"); bUnsupported = true; break;
        case 0x4c46: strcpy(Device, "Rage Mobility"); bUnsupported = true; break;
        case 0x4c4d: strcpy(Device, "Rage Mobility"); bUnsupported = true; break;
        case 0x4c4e: strcpy(Device, "Rage Mobility"); bUnsupported = true; break;
        case 0x4c52: strcpy(Device, "Rage Mobility"); bUnsupported = true; break;
        case 0x4c53: strcpy(Device, "Rage Mobility"); bUnsupported = true; break;
        case 0x4c54: strcpy(Device, "Rage Mobility"); bUnsupported = true; break;
        case 0x4d46: strcpy(Device, "Rage Mobility 128"); bUnsupported = true; break;
        case 0x4d4c: strcpy(Device, "Rage Mobility 128"); bUnsupported = true; break;
        case 0x5041: strcpy(Device, "Rage 128 Pro PCI"); bUnsupported = true; break;
        case 0x5042: strcpy(Device, "Rage 128 Pro AGP2X"); bUnsupported = true; break;
        case 0x5043: strcpy(Device, "Rage 128 Pro AGP4X"); bUnsupported = true; break;
        case 0x5044: strcpy(Device, "Rage 128 Pro PCI TMDS"); bUnsupported = true; break;
        case 0x5045: strcpy(Device, "Rage 128 Pro AGP2X TMDS"); bUnsupported = true; break;
        case 0x5046: strcpy(Device, "Rage 128 Pro AGP4X TMDS"); bUnsupported = true; break;
        case 0x5047: strcpy(Device, "Rage 128 Pro PCI"); bUnsupported = true; break;
        case 0x5048: strcpy(Device, "Rage 128 Pro AGP2X"); bUnsupported = true; break;
        case 0x5049: strcpy(Device, "Rage 128 Pro AGP4X"); bUnsupported = true; break;
        case 0x504a: strcpy(Device, "Rage 128 Pro PCI TMDS"); bUnsupported = true; break;
        case 0x504b: strcpy(Device, "Rage 128 Pro AGP2X TMDS"); bUnsupported = true; break;
        case 0x504c: strcpy(Device, "Rage 128 Pro AGP4X TMDS"); bUnsupported = true; break;
        case 0x504d: strcpy(Device, "Rage 128 Pro PCI"); bUnsupported = true; break;
        case 0x504e: strcpy(Device, "Rage 128 Pro AGP2X"); bUnsupported = true; break;
        case 0x504f: strcpy(Device, "Rage 128 Pro AGP4X"); bUnsupported = true; break;
        case 0x5050: strcpy(Device, "Rage 128 Pro PCI TMDS"); bUnsupported = true; break;
        case 0x5051: strcpy(Device, "Rage 128 Pro AGP2X TMDS"); bUnsupported = true; break;
        case 0x5052: strcpy(Device, "Rage 128 Pro AGP4X TMDS"); bUnsupported = true; break;
        case 0x5053: strcpy(Device, "Rage 128 Pro PCI"); bUnsupported = true; break;
        case 0x5054: strcpy(Device, "Rage 128 Pro AGP2X"); bUnsupported = true; break;
        case 0x5055: strcpy(Device, "Rage 128 Pro AGP4X"); bUnsupported = true; break;
        case 0x5056: strcpy(Device, "Rage 128 Pro PCI TMDS"); bUnsupported = true; break;
        case 0x5057: strcpy(Device, "Rage 128 Pro AGP2X TMDS"); bUnsupported = true; break;
        case 0x5058: strcpy(Device, "Rage 128 Pro AGP4X TMDS"); bUnsupported = true; break;
        case 0x5245: strcpy(Device, "Rage 128 GL PCI"); bUnsupported = true; break;
        case 0x5246: strcpy(Device, "Rage 128 GL AGP"); bUnsupported = true; break;
        case 0x5247: strcpy(Device, "Rage 128"); bUnsupported = true; break;
        case 0x524b: strcpy(Device, "Rage 128 VR PCI"); bUnsupported = true; break;
        case 0x524c: strcpy(Device, "Rage 128 VR AGP"); bUnsupported = true; break;
        case 0x5345: strcpy(Device, "Rage 128 4X PCI"); bUnsupported = true; break;
        case 0x5346: strcpy(Device, "Rage 128 4X AGP2X"); bUnsupported = true; break;
        case 0x5347: strcpy(Device, "Rage 128 4X AGP4X"); bUnsupported = true; break;
        case 0x5348: strcpy(Device, "Rage 128 4X"); bUnsupported = true; break;
        case 0x534b: strcpy(Device, "Rage 128 4X PCI"); bUnsupported = true; break;
        case 0x534c: strcpy(Device, "Rage 128 4X AGP2X"); bUnsupported = true; break;
        case 0x534d: strcpy(Device, "Rage 128 4X AGP4X"); bUnsupported = true; break;
        case 0x534e: strcpy(Device, "Rage 128 4X"); bUnsupported = true; break;
        case 0x5354: strcpy(Device, "Mach 64"); bUnsupported = true; break;
        case 0x5446: strcpy(Device, "Rage 128 Pro Ultra GL AGP"); bUnsupported = true; break;
        case 0x544c: strcpy(Device, "Rage 128 Pro Ultra VR AGP"); bUnsupported = true; break;
        case 0x5452: strcpy(Device, "Rage 128 Pro Ultra4XL VR-R AGP"); bUnsupported = true; break;
        case 0x5453: strcpy(Device, "Rage 128 Pro"); bUnsupported = true; break;
        case 0x5454: strcpy(Device, "Rage 128 Pro"); bUnsupported = true; break;
        case 0x5455: strcpy(Device, "Rage 128 Pro"); bUnsupported = true; break;
        case 0x5654: strcpy(Device, "Mach 64"); bUnsupported = true; break;
        case 0x5655: strcpy(Device, "Mach 64"); bUnsupported = true; break;
        case 0x5656: strcpy(Device, "Mach 64"); bUnsupported = true; break;
        case 0x4c57: strcpy(Device, "Radeon Mobility 7500"); bUnsupported = true; break;
        case 0x4c58: strcpy(Device, "Radeon Mobility 7500"); bUnsupported = true; break;
        case 0x4c59: strcpy(Device, "Radeon Mobility VE"); bUnsupported = true; break;
        case 0x4c5a: strcpy(Device, "Radeon Mobility VE"); bUnsupported = true; break;

        case 0x4c64: strcpy(Device, "Radeon Mobility 9000");  break;
        case 0x4c66: strcpy(Device, "Radeon Mobility 9000");  break;
        case 0x4966: strcpy(Device, "Radeon 9000");  break;
        case 0x496e: strcpy(Device, "Radeon 9000 - Secondary");  break;
        case 0x514d: strcpy(Device, "Radeon 9100");  break;
        case 0x5834: strcpy(Device, "Radeon 9100 IGP");  break;
        case 0x4242: strcpy(Device, "Radeon 8500 DV");  break;
        case 0x4152: strcpy(Device, "Radeon 9600");  break;
        case 0x4172: strcpy(Device, "Radeon 9600 - Secondary");  break;
        case 0x4164: strcpy(Device, "Radeon 9500 - Secondary");  break;
        case 0x4144: strcpy(Device, "Radeon 9500");  break;
        case 0x4e45: strcpy(Device, "Radeon 9500 Pro / 9700");  break;
        case 0x4150: strcpy(Device, "Radeon 9600 Pro");  break;
        case 0x4151: strcpy(Device, "Radeon 9600");  break;
        case 0x4170: strcpy(Device, "Radeon 9600 Pro - Secondary");  break;
        case 0x4171: strcpy(Device, "Radeon 9600 - Secondary");  break;
        case 0x4e46: strcpy(Device, "Radeon 9600 TX");  break;
        case 0x4e66: strcpy(Device, "Radeon 9600 TX - Secondary");  break;
        case 0x4e44: strcpy(Device, "Radeon 9700 Pro");  break;
        case 0x4e64: strcpy(Device, "Radeon 9700 Pro - Secondary");  break;
        case 0x4e65: strcpy(Device, "Radeon 9500 Pro / 9700 - Secondary");  break;
        case 0x4e49: strcpy(Device, "Radeon 9800");  break;
        case 0x4e69: strcpy(Device, "Radeon 9800 - Secondary");  break;
        case 0x4148: strcpy(Device, "Radeon 9800");  break;
        case 0x4168: strcpy(Device, "Radeon 9800 - Secondary");  break;
        case 0x4e48: strcpy(Device, "Radeon 9800 Pro");  break;
        case 0x4e68: strcpy(Device, "Radeon 9800 Pro - Secondary");  break;
        case 0x4e4a: strcpy(Device, "Radeon 9800 XT");  break;
        case 0x4e6a: strcpy(Device, "Radeon 9800 XT - Secondary");  break;
        case 0x5960: strcpy(Device, "Radeon 9200 Pro");  break;
        case 0x5940: strcpy(Device, "Radeon 9200 Pro - Secondary");  break;
        case 0x5961: strcpy(Device, "Radeon 9200");  break;
        case 0x5941: strcpy(Device, "Radeon 9200 - Secondary");  break;
        case 0x5964: strcpy(Device, "Radeon 9200SE");  break;
        case 0x5144: strcpy(Device, "Radeon 7200");  break;
        case 0x5145: strcpy(Device, "Radeon");  break;
        case 0x5146: strcpy(Device, "Radeon");  break;
        case 0x5147: strcpy(Device, "Radeon");  break;
        case 0x5148: strcpy(Device, "Radeon FireGL");  break;
        case 0x5157: strcpy(Device, "Radeon 7500");  break;
        case 0x5159: strcpy(Device, "Radeon 7000 VE");  break;
        case 0x515a: strcpy(Device, "Radeon VE");  break;
        case 0x516c: strcpy(Device, "Radeon");  break;
        case 0x514c: strcpy(Device, "Radeon 8500");  break;
        case 0x514e: strcpy(Device, "Radeon 8500");  break;
        case 0x514f: strcpy(Device, "Radeon 8500");  break;
        case 0x4136: strcpy(Device, "IGP 320");  break;
        case 0x4137: strcpy(Device, "IGP 340");  break;

        case 0x4A49: strcpy(Device, "Radeon X800 Pro");  break;
        case 0x4A4A: strcpy(Device, "Radeon X800 SE");  break;
        case 0x4A4B: strcpy(Device, "Radeon X800");  break;
        case 0x4A4C: strcpy(Device, "Radeon X800 Series");  break;
        case 0x4A50: strcpy(Device, "Radeon X800 XT");  break;
        case 0x4A69: strcpy(Device, "Radeon X800 Pro Secondary");  break;
        case 0x4A6A: strcpy(Device, "Radeon X800 SE Secondary");  break;
        case 0x4A6b: strcpy(Device, "Radeon X800 Secondary");  break;
        case 0x4A6C: strcpy(Device, "Radeon X800 Series Secondary");  break;
        case 0x4A70: strcpy(Device, "Radeon X800 XT Secondary");  break;

        case 0x5B60: strcpy(Device, "Radeon X300 Series");  break;
        case 0x5B70: strcpy(Device, "Radeon X300 Series Secondary");  break;
        case 0x3E50: strcpy(Device, "Radeon X600 Series");  break;
        case 0x3E70: strcpy(Device, "Radeon X600 Series Secondary");  break;
        case 0x5549: strcpy(Device, "Radeon X800 Pro");  break;
        case 0x5569: strcpy(Device, "Radeon X800 Pro Secondary");  break;
        case 0x554B: strcpy(Device, "Radeon X800 SE");  break;
        case 0x556B: strcpy(Device, "Radeon X800 SE Secondary");  break;

        default: strcpy(Device, "Unknown");
      }
  	  break;

    case 0x104c:
    case 0x10ba:
    case 0x3d3d:
    case 0x1048:
      strcpy(Vendor, "3D Labs");
      switch (DeviceIdentifier.dwDeviceId)
      {
        case 0x0001: strcpy(Device, "GLiNT 300SX"); bUnsupported = true; break;
        case 0x0002: strcpy(Device, "GLiNT 500TX"); bUnsupported = true; break;
        case 0x0003: strcpy(Device, "GLiNT"); bUnsupported = true; break;
        case 0x0004: strcpy(Device, "Permedia"); bUnsupported = true; break;
        case 0x0005: strcpy(Device, "Permedia"); bUnsupported = true; break;
        case 0x0006: strcpy(Device, "GLiNT MX"); bUnsupported = true; break;
        case 0x0007: strcpy(Device, "Permedia 2"); bUnsupported = true; break;
        case 0x0008: strcpy(Device, "GLiNT G1"); bUnsupported = true; break;
        case 0x0009: strcpy(Device, "Permedia 2"); bUnsupported = true; break;
        case 0x000b: strcpy(Device, "Oxygen Series R3"); bUnsupported = true; break;
        case 0x000d: strcpy(Device, "Oxygen Series R4"); bUnsupported = true; break;
        case 0x000e: strcpy(Device, "Oxygen Series Gamma2"); bUnsupported = true; break;
        case 0x0100: strcpy(Device, "Permedia 2"); bUnsupported = true; break;
        case 0x0301: strcpy(Device, "Permedia 2"); bUnsupported = true; break;
        case 0x1004: strcpy(Device, "Permedia"); bUnsupported = true; break;
        case 0x3d04: strcpy(Device, "Permedia 1"); bUnsupported = true; break;
        case 0x3d07: strcpy(Device, "Permedia 2"); bUnsupported = true; break;
        case 0x8901: strcpy(Device, "GLiNT"); bUnsupported = true; break;

        case 0x000a: strcpy(Device, "Permedia 3"); break;
        case 0x000c: strcpy(Device, "Permedia 4"); break;
        default: strcpy(Device, "Unknown");
      }
  	  break;

    case 0x1039:
      strcpy(Vendor, "SiS");
      switch (DeviceIdentifier.dwDeviceId)
      {
        case 0x0204: strcpy(Device, "6215"); bUnsupported = true; break;
        case 0x0205: strcpy(Device, "6205"); bUnsupported = true; break;
        case 0x0305: strcpy(Device, "305"); bUnsupported = true; break;
        case 0x6306: strcpy(Device, "530"); bUnsupported = true; break;
        case 0x6326: strcpy(Device, "6326"); bUnsupported = true; break;
        case 0x6325: strcpy(Device, "650"); bUnsupported = true; break;

        case 0x0325: strcpy(Device, "315");       break;
        case 0x0330: strcpy(Device, "Xabre 330"); break;
        default:     strcpy(Device, "Unknown");
      }
  	  break;

    case 0x5333:
      strcpy(Vendor, "S3");
      switch (DeviceIdentifier.dwDeviceId)
      {
        case 0x9102: strcpy(Device, "Savage 2000"); bUnsupported = true; break;
        case 0x8a20: strcpy(Device, "Savage 3D"); bUnsupported = true; break;
        case 0x8a21: strcpy(Device, "Savage 3D S3"); bUnsupported = true; break;
        case 0x8a22: strcpy(Device, "Savage 3D S4"); bUnsupported = true; break;
        case 0x8a23: strcpy(Device, "Savage 3D S4"); bUnsupported = true; break;
        case 0x8a25: strcpy(Device, "Savage4 ProSavage"); bUnsupported = true; break;
        case 0x8a26: strcpy(Device, "ProSavage"); bUnsupported = true; break;
        case 0x8c10: strcpy(Device, "Savage MX"); bUnsupported = true; break;
        case 0x8c12: strcpy(Device, "Savage IX"); bUnsupported = true; break;
        case 0x8c22: strcpy(Device, "SuperSavage 128 MX"); bUnsupported = true; break;
        case 0x8c2a: strcpy(Device, "SuperSavage 128 IX"); bUnsupported = true; break;
        case 0x8c2b: strcpy(Device, "SuperSavage 128 IX DDR"); bUnsupported = true; break;
        case 0x8c2c: strcpy(Device, "SuperSavage IX"); bUnsupported = true; break;
        case 0x8c2d: strcpy(Device, "SuperSavage IX DDR"); bUnsupported = true; break;
        case 0x8c2e: strcpy(Device, "SuperSavage IXC SDR"); bUnsupported = true; break;
        case 0x8c2f: strcpy(Device, "SuperSavage IXC DDR"); bUnsupported = true; break;
        case 0x8d04: strcpy(Device, "ProSavage"); bUnsupported = true; break;
        case 0x5631: strcpy(Device, "Virge"); bUnsupported = true; break;
        case 0x8811: strcpy(Device, "Trio 64"); bUnsupported = true; break;
        case 0x8812: strcpy(Device, "Trio 64"); bUnsupported = true; break;
        case 0x8814: strcpy(Device, "Trio 64 Plus"); bUnsupported = true; break;
        case 0x8815: strcpy(Device, "Aurora 128"); bUnsupported = true; break;
        case 0x883d: strcpy(Device, "Virge"); bUnsupported = true; break;
        case 0x8880: strcpy(Device, "Virge"); bUnsupported = true; break;
        case 0x88c0: strcpy(Device, "Virge"); bUnsupported = true; break;
        case 0x88c1: strcpy(Device, "Virge"); bUnsupported = true; break;
        case 0x88d0: strcpy(Device, "Virge"); bUnsupported = true; break;
        case 0x88d1: strcpy(Device, "Virge"); bUnsupported = true; break;
        case 0x88f0: strcpy(Device, "Virge"); bUnsupported = true; break;
        case 0x8901: strcpy(Device, "Trio 64 DX"); bUnsupported = true; break;
        case 0x8904: strcpy(Device, "Trio 3D"); bUnsupported = true; break;
        case 0x8a01: strcpy(Device, "Virge DXGX"); bUnsupported = true; break;
        case 0x8a10: strcpy(Device, "Virge GX2"); bUnsupported = true; break;
        case 0x8a13: strcpy(Device, "Trio3D"); bUnsupported = true; break;
        case 0x8c00: strcpy(Device, "Virge MX"); bUnsupported = true; break;
        case 0x8c01: strcpy(Device, "Virge MX"); bUnsupported = true; break;
        case 0x8c02: strcpy(Device, "Virge MXC"); bUnsupported = true; break;
        case 0x8c03: strcpy(Device, "Virge MX"); bUnsupported = true; break;
        case 0x8d02: strcpy(Device, "Graphics Twister"); bUnsupported = true; break;
        default: strcpy(Device, "Unknown");
      }
  	  break;

    case 0x102b:
      strcpy(Vendor, "Matrox");
      switch (DeviceIdentifier.dwDeviceId)
      {
        case 0x0518: strcpy(Device, "Millennium"); bUnsupported = true; break;
        case 0x0519: strcpy(Device, "Millennium"); bUnsupported = true; break;
        case 0x051a: strcpy(Device, "Mystique"); bUnsupported = true; break;
        case 0x051b: strcpy(Device, "Millennium II"); bUnsupported = true; break;
        case 0x051f: strcpy(Device, "Millennium II"); bUnsupported = true; break;
        case 0x0d10: strcpy(Device, "Mystique"); bUnsupported = true; break;
        case 0x1000: strcpy(Device, "G100 PCI"); bUnsupported = true; break;
        case 0x1001: strcpy(Device, "G100 AGP"); bUnsupported = true; break;
        case 0x0520: strcpy(Device, "G200 PCI"); bUnsupported = true; break;
        case 0x0521: strcpy(Device, "G200 AGP"); bUnsupported = true; break;
        case 0x1525: strcpy(Device, "Fusion G450"); bUnsupported = true; break;
        case 0x0525: strcpy(Device, "G400/450"); bUnsupported = true; break;
        case 0x2007: strcpy(Device, "Mistral"); bUnsupported = true; break;

        case 0x2527: strcpy(Device, "G550"); break;
        case 0x1527: strcpy(Device, "Fusion G800"); break;
        case 0x0527: strcpy(Device, "Parhelia 128"); break;
        default: strcpy(Device, "Unknown");
      }
  	  break;

    case 0x14Af:
      strcpy(Vendor, "Guillemot");
      switch (DeviceIdentifier.dwDeviceId)
      {
        case 0x5810: strcpy(Device, "TNT2"); bUnsupported = true; break;
        case 0x5820: strcpy(Device, "TNT2 Ultra"); bUnsupported = true; break;
        case 0x5620: strcpy(Device, "TNT2 M64"); bUnsupported = true; break;
        case 0x5020: strcpy(Device, "GeForce 256"); bUnsupported = true; break;
        case 0x5008: strcpy(Device, "TNT Vanta"); bUnsupported = true; break;
        case 0x4D20: strcpy(Device, "TNT2 M64"); bUnsupported = true; break;
        default: strcpy(Device, "Unknown");
      }
  	  break;

    case 0x10b4:
    case 0x12d2:
    case 0x10de:
      strcpy(Vendor, "nVidia");
      switch (DeviceIdentifier.dwDeviceId)
      {
        case 0x1b1d: strcpy(Device, "Riva 128"); bUnsupported = true; break;
        case 0x0008: strcpy(Device, "NV 1"); bUnsupported = true; break;
        case 0x0009: strcpy(Device, "NV 1"); bUnsupported = true; break;
        case 0x0010: strcpy(Device, "NV 2"); bUnsupported = true; break;
        case 0x0018: strcpy(Device, "Riva 128"); bUnsupported = true; break;
        case 0x0019: strcpy(Device, "Riva 128 ZX"); bUnsupported = true; break;
        case 0x0020: strcpy(Device, "TNT"); bUnsupported = true; break;
        case 0x002e: strcpy(Device, "TNT Vanta"); bUnsupported = true; break;
        case 0x002f: strcpy(Device, "TNT Vanta"); bUnsupported = true; break;
        case 0x00a0: strcpy(Device, "TNT2 Aladdin"); bUnsupported = true; break;
        case 0x0028: strcpy(Device, "Riva TNT2/TNT2 Pro"); bUnsupported = true; break;
        case 0x0029: strcpy(Device, "TNT2 Ultra"); bUnsupported = true; break;
        case 0x002a: strcpy(Device, "TNT2"); bUnsupported = true; break;
        case 0x002b: strcpy(Device, "TNT2"); bUnsupported = true; break;
        case 0x002c: strcpy(Device, "TNT Vanta/Vanta LT"); bUnsupported = true; break;
        case 0x002d: strcpy(Device, "TNT2 M64/M64 Pro"); bUnsupported = true; break;

		case 0x0040: strcpy(Device, "GeForce 6800 Ultra"); break;
		case 0x0041: strcpy(Device, "GeForce 6800"); break;
        case 0x0042:
        case 0x0043:
        case 0x0044:
        case 0x0045:
        case 0x0046:
        case 0x0047:
        case 0x0048:
        case 0x004a:
        case 0x004b:
        case 0x004c:
        case 0x004d:
        case 0x004f:
                     strcpy(Device, "NV40");  break;

		case 0x0049:
		case 0x004e:
			strcpy(Device, "NV40GL");  break;

		case 0x00f9: strcpy(Device, "GeForce 6800 Ultra"); break;

        case 0x00fa: strcpy(Device, "GeForce PCX 5750"); break;
        case 0x00fb: strcpy(Device, "GeForce PCX 5900"); break;
        case 0x00fc: strcpy(Device, "GeForce PCX 5300"); break;
        case 0x00fd: strcpy(Device, "Quadro PCI-E Series"); break;
		case 0x00fe: strcpy(Device, "Quadro FX 1300"); break;
        case 0x00ff: strcpy(Device, "GeForce PCX 4300"); break;

        case 0x0100: strcpy(Device, "GeForce 256");  break;
        case 0x0101: strcpy(Device, "GeForce 256 DDR");  break;
        case 0x0102: strcpy(Device, "GeForce 256 Ultra");  break;
        case 0x0103: strcpy(Device, "GeForce 256 Quadro");  break;
        case 0x0110: strcpy(Device, "GeForce2 MX/MX 400");  break;
        case 0x0111: strcpy(Device, "GeForce2 MX 100/200");  break;
        case 0x0112: strcpy(Device, "GeForce2 Go");  break;
        case 0x0113: strcpy(Device, "Quadro2 MXR/EX");  break;
        case 0x0150: strcpy(Device, "GeForce2 GTS/GeForce2 Pro");  break;
        case 0x0151: strcpy(Device, "GeForce2 Ti");  break;
        case 0x0152: strcpy(Device, "GeForce2 Ultra");  break;
        case 0x0153: strcpy(Device, "Quadro2 Pro");  break;
        case 0x0170: strcpy(Device, "GeForce4 MX 460");  break;
        case 0x0171: strcpy(Device, "GeForce4 MX 440");  break;
        case 0x0172: strcpy(Device, "GeForce4 MX 420");  break;
        case 0x0173: strcpy(Device, "GeForce4 MX 440-SE");  break;
        case 0x0174: strcpy(Device, "GeForce4 Go 440");  break;
        case 0x0175: strcpy(Device, "GeForce4 Go 420");  break;
        case 0x0176: strcpy(Device, "GeForce4 Go 420");  break;
        case 0x0177: strcpy(Device, "GeForce4 460 Go");  break;
        case 0x0178: strcpy(Device, "Quadro4 550 XGL");  break;
        case 0x0179: strcpy(Device, "GeForce4 Go 440");  break;
        case 0x017a: strcpy(Device, "Quadro NVS");  break;
        case 0x017b: strcpy(Device, "Quadro 550 XGL");  break;
        case 0x017c: strcpy(Device, "Quadro4 500 GoGL");  break;
        case 0x017d: strcpy(Device, "GeForce4 410 Go 16M");  break;
        case 0x0181: strcpy(Device, "GeForce4 MX 440 with AGP8X");  break;
        case 0x0182: strcpy(Device, "GeForce4 MX 440SE with AGP8X");  break;
        case 0x0183: strcpy(Device, "GeForce4 MX 420 with AGP8X");  break;
        case 0x0188: strcpy(Device, "Quadro4 580 XGL");  break;
        case 0x018a: strcpy(Device, "Quadro NVS with AGP8X");  break;
        case 0x018b: strcpy(Device, "Quadro4 380 XGL");  break;
        case 0x01a0: strcpy(Device, "GeForce2 Integrated GPU (nForce)");  break;
        case 0x01f0: strcpy(Device, "GeForce4 MX Integrated GPU (nForce2)");  break;

        case 0x0200: strcpy(Device, "GeForce3");  break;
        case 0x0201: strcpy(Device, "GeForce3 Ti200");  break;
        case 0x0202: strcpy(Device, "GeForce3 Ti500");  break;
        case 0x0203: strcpy(Device, "Quadro DCC");  break;
        case 0x0250: strcpy(Device, "GeForce4 Ti 4600");  break;
        case 0x0251: strcpy(Device, "GeForce4 Ti 4400");  break;
        case 0x0253: strcpy(Device, "GeForce4 Ti 4200");  break;
        case 0x0258: strcpy(Device, "Quadro4 900 XGL");  break;
        case 0x0259: strcpy(Device, "Quadro4 750 XGL");  break;
        case 0x025b: strcpy(Device, "Quadro4 700 XGL");  break;
        case 0x0280: strcpy(Device, "GeForce4 Ti 4800");  break;
        case 0x0281: strcpy(Device, "GeForce4 Ti4200 with AGP8X");  break;
        case 0x0282: strcpy(Device, "GeForce4 Ti 4800 SE");  break;
        case 0x0288: strcpy(Device, "Quadro4 980 XGL");  break;
        case 0x0289: strcpy(Device, "Quadro4 780 XGL");  break;
        case 0x02a0: strcpy(Device, "GeForce3 XBOX");  break;

        case 0x0301: strcpy(Device, "GeForce FX 5800 Ultra");  break;
        case 0x0302: strcpy(Device, "GeForce FX 5800");  break;
        case 0x0308: strcpy(Device, "Quadro FX 2000");  break;
        case 0x0309: strcpy(Device, "Quadro FX 1000");  break;
		case 0x030a: strcpy(Device, "ICE FX 2000");  break;
        case 0x0311: strcpy(Device, "GeForce FX 5600 Ultra");  break;
        case 0x0312: strcpy(Device, "GeForce FX 5600");  break;
        case 0x0313: strcpy(Device, "NV31");  break;
        case 0x0314: strcpy(Device, "GeForce FX 5600XT");  break;
        case 0x031a: strcpy(Device, "GeForce FX Go5600");  break;
        case 0x0321: strcpy(Device, "GeForce FX 5200 Ultra");  break;
        case 0x0322: strcpy(Device, "GeForce FX 5200");  break;
        case 0x0323: strcpy(Device, "GeForce FX 5200SE");  break;
        case 0x0324: strcpy(Device, "GeForce FX Go5200");  break;
        case 0x0325: strcpy(Device, "GeForce FX Go5250");  break;
        case 0x0328: strcpy(Device, "GeForce FX Go5200 32M/64M");  break;
        case 0x032a: strcpy(Device, "Quadro NVS 280 PCI");  break;
        case 0x032b: strcpy(Device, "Quadro FX 500");  break;
        case 0x032c: strcpy(Device, "GeForce FX Go53xx Series");  break;
        case 0x032d: strcpy(Device, "GeForce FX Go5100");  break;
        case 0x032f: strcpy(Device, "NV34GL");  break;
        case 0x0330: strcpy(Device, "GeForce FX 5900 Ultra");  break;
        case 0x0331: strcpy(Device, "GeForce FX 5900");  break;
        case 0x0332: strcpy(Device, "GeForce FX 5900XT");  break;
        case 0x0333: strcpy(Device, "GeForce FX 5950 Ultra");  break;
        case 0x0338: strcpy(Device, "Quadro FX 3000");  break;
        case 0x0341: strcpy(Device, "GeForce FX 5700 Ultra");  break;
        case 0x0342: strcpy(Device, "GeForce FX 5700");  break;
        case 0x0343: strcpy(Device, "GeForce FX 5700LE");  break;
        case 0x0344: strcpy(Device, "GeForce FX 5700VE");  break;
        case 0x0345: strcpy(Device, "NV36");  break;
        case 0x0347: strcpy(Device, "GeForce FX Go5700");  break;
        case 0x0348: strcpy(Device, "GeForce FX Go5700");  break;
        case 0x0349: strcpy(Device, "NV36M Pro");  break;
        case 0x034b: strcpy(Device, "NV36MAP");  break;
        case 0x034c: strcpy(Device, "Quadro FX Go1000");  break;
        case 0x034e: strcpy(Device, "Quadro FX 1100");  break;
        case 0x034f: strcpy(Device, "NV36GL");  break;
        default:     strcpy(Device, "Unknown");
      }
  	  break;

    case 0x8086:
      strcpy(Vendor, "Intel");
      switch (DeviceIdentifier.dwDeviceId)
      {
        case 0x7121: strcpy(Device, "810"); bUnsupported = true; break;
        case 0x7123: strcpy(Device, "810"); bUnsupported = true; break;
        case 0x7125: strcpy(Device, "810e"); bUnsupported = true; break;
        case 0x7127: strcpy(Device, "810"); bUnsupported = true; break;
        case 0x1132: strcpy(Device, "815"); bUnsupported = true; break;
        case 0x7800: strcpy(Device, "740"); bUnsupported = true; break;
        case 0x1240: strcpy(Device, "752"); bUnsupported = true; break;

        case 0x3577: strcpy(Device, "830"); break;
        case 0x2562: strcpy(Device, "845"); break;
        case 0x2572: strcpy(Device, "865G"); break;
        default:     strcpy(Device, "Unknown");
      }
  	  break;

    case 0x1033:
      strcpy(Vendor, "VideoLogic");
      switch (DeviceIdentifier.dwDeviceId)
      {
        case 0x0046: strcpy(Device, "PowerVR"); bUnsupported = true; break;
        case 0x0067: strcpy(Device, "PowerVR2"); bUnsupported = true; break;
        default:     strcpy(Device, "Unknown");
      }
  	  break;

    case 0x1023:
      strcpy(Vendor, "Trident");
      switch (DeviceIdentifier.dwDeviceId)
      {
        case 0x8420: strcpy(Device, "CyberBlade i7"); bUnsupported = true; break;
        case 0x8820: strcpy(Device, "CyberBlade XP"); bUnsupported = true; break;
        case 0x9320: strcpy(Device, "Cyber9320"); bUnsupported = true; break;
        case 0x9388: strcpy(Device, "Cyber9388"); bUnsupported = true; break;
        case 0x9397: strcpy(Device, "Cyber9397"); bUnsupported = true; break;
        case 0x939A: strcpy(Device, "Cyber9397 DVD"); bUnsupported = true; break;
        case 0x9440: strcpy(Device, "Cyber"); bUnsupported = true; break;
        case 0x9520: strcpy(Device, "Cyber9520"); bUnsupported = true; break;
        case 0x9525: strcpy(Device, "Cyber9520 DVD"); bUnsupported = true; break;
        case 0x9540: strcpy(Device, "CyberBlade E4"); bUnsupported = true; break;
        case 0x9660: strcpy(Device, "Cyber9385"); bUnsupported = true; break;
        case 0x9750: strcpy(Device, "975"); bUnsupported = true; break;
        case 0x9754: strcpy(Device, "9753"); bUnsupported = true; break;
        case 0x9850: strcpy(Device, "3D Image"); bUnsupported = true; break;
        case 0x9880: strcpy(Device, "Blade 3D"); bUnsupported = true; break;
        case 0x9910: strcpy(Device, "CyberBlade XP"); bUnsupported = true; break;
        case 0x9930: strcpy(Device, "CyberBlade XPm"); bUnsupported = true; break;
        default:     strcpy(Device, "Unknown");
      }
  	  break;

    case 0x105d:
      strcpy(Vendor, "Number Nine");
      switch (DeviceIdentifier.dwDeviceId)
      {
        case 0x493d: strcpy(Device, "Revolution 3D"); bUnsupported = true; break;
        case 0x5348: strcpy(Device, "Revolution IV"); bUnsupported = true; break;
        default:     strcpy(Device, "Unknown");
      }
  	  break;

    case 0x126f:
      strcpy(Vendor, "Silicon Motion");
      switch (DeviceIdentifier.dwDeviceId)
      {
        case 0x720 : strcpy(Device, "Lynx 3DM"); bUnsupported = true; break;
        case 0x820 : strcpy(Device, "Lynx 3D"); bUnsupported = true; break;
        default:     strcpy(Device, "Unknown");
      }
  	  break;

    case 0x1013:
      strcpy(Vendor, "Cirrus");
      switch (DeviceIdentifier.dwDeviceId)
      {
        case 0x0038: strcpy(Device, "GD7548"); bUnsupported = true; break;
        case 0x00a0: strcpy(Device, "GD5430"); bUnsupported = true; break;
        case 0x00a8: strcpy(Device, "GD5434"); bUnsupported = true; break;
        case 0x00ac: strcpy(Device, "GD5436"); bUnsupported = true; break;
        case 0x00b8: strcpy(Device, "GD5446"); bUnsupported = true; break;
        case 0x00bc: strcpy(Device, "GD5480"); bUnsupported = true; break;
        case 0x00d0: strcpy(Device, "CL5462"); bUnsupported = true; break;
        case 0x00d4: strcpy(Device, "GD5464"); bUnsupported = true; break;
        case 0x00d6: strcpy(Device, "GD5465"); bUnsupported = true; break;
        case 0x0301: strcpy(Device, "GD5446"); bUnsupported = true; break;
        case 0x1100: strcpy(Device, "CL6729"); bUnsupported = true; break;
        case 0x1202: strcpy(Device, "GD7543"); bUnsupported = true; break;
        case 0x6001: strcpy(Device, "CL4610"); bUnsupported = true; break;
        case 0x6003: strcpy(Device, "CL4614"); bUnsupported = true; break;
        default:     strcpy(Device, "Unknown");
      }
  	  break;

    case 0x1163:
      strcpy(Vendor, "Rendition");
      switch (DeviceIdentifier.dwDeviceId)
      {
        case 0x0001: strcpy(Device, "Verite 1000"); bUnsupported = true; break;
        case 0x2000: strcpy(Device, "Verite 2100"); bUnsupported = true; break;
        default:     strcpy(Device, "Unknown");
      }
  	  break;

    case 0x110b:
      strcpy(Vendor, "Chromatic");
      switch (DeviceIdentifier.dwDeviceId)
      {
        case 0x0004: strcpy(Device, "MPact"); bUnsupported = true; break;
        default:     strcpy(Device, "Unknown");
      }
  	  break;

    case 0x100e:
      strcpy(Vendor, "Weitek");
      switch (DeviceIdentifier.dwDeviceId)
      {
        case 0x9001: strcpy(Device, "P9000"); bUnsupported = true; break;
        case 0x9100: strcpy(Device, "P9100"); bUnsupported = true; break;
        default:     strcpy(Device, "Unknown");
      }
  	  break;

    case 0x0e11:
      strcpy(Vendor, "Compaq");
      switch (DeviceIdentifier.dwDeviceId)
      {
        case 0x3032: strcpy(Device, "QVision"); bUnsupported = true; break;
        default:     strcpy(Device, "Unknown");
      }
  	  break;

    case 0x1011:
      strcpy(Vendor, "Digital");
      switch (DeviceIdentifier.dwDeviceId)
      {
        case 0x0004: strcpy(Device, "TGA"); bUnsupported = true; break;
        case 0x000d: strcpy(Device, "TGA2"); bUnsupported = true; break;
        default:     strcpy(Device, "Unknown");
      }
  	  break;

    case 0x10c8:
      strcpy(Vendor, "NeoMagic");
      switch (DeviceIdentifier.dwDeviceId)
      {
        case 0x0001: strcpy(Device, "128"); bUnsupported = true; break;
        case 0x0002: strcpy(Device, "128"); bUnsupported = true; break;
        case 0x0003: strcpy(Device, "128ZV"); bUnsupported = true; break;
        case 0x0004: strcpy(Device, "128XD"); bUnsupported = true; break;
        case 0x0005: strcpy(Device, "256AV"); bUnsupported = true; break;
        case 0x0006: strcpy(Device, "256ZX"); bUnsupported = true; break;
        case 0x0016: strcpy(Device, "256XL"); bUnsupported = true; break;
        case 0x0025: strcpy(Device, "256AV"); bUnsupported = true; break;
        case 0x0083: strcpy(Device, "128ZV"); bUnsupported = true; break;
        default:     strcpy(Device, "Unknown");
      }
  	  break;

    case 0x100c:
      strcpy(Vendor, "Tseng Labs");
      switch (DeviceIdentifier.dwDeviceId)
      {
        case 0x3202: strcpy(Device, "ET4000"); bUnsupported = true; break;
        case 0x3205: strcpy(Device, "ET4000"); bUnsupported = true; break;
        case 0x3206: strcpy(Device, "ET4000"); bUnsupported = true; break;
        case 0x3207: strcpy(Device, "ET4000"); bUnsupported = true; break;
        case 0x3208: strcpy(Device, "ET6000"); bUnsupported = true; break;
        case 0x4702: strcpy(Device, "ET6300"); bUnsupported = true; break;
        default:     strcpy(Device, "Unknown");
      }
  	  break;

    case 0x18ca:
      strcpy(Vendor, "XGI");
      switch (DeviceIdentifier.dwDeviceId)
      {
        case 0x40: strcpy(Device, "Volary V8 DUO Ultra"); break;
        default:     strcpy(Device, "Unknown");
      }
  	  break;

    default:
      strcpy(Vendor, "Unknown");
      break;
  }

  pDD->Release();
  FreeLibrary(hDDInstance);

  if (bUnsupported)
    return -1;

  //Check DX version
  DWORD dwDirectXVersion = 0;
  TCHAR strDirectXVersion[10];
  TCHAR strResult[128];

  hr = GetDXVersion( &dwDirectXVersion, strDirectXVersion, 10 );
  if( SUCCEEDED(hr) )
  {
    if( dwDirectXVersion > 0 )
    {
      sprintf(strResult, TEXT("DirectX %s installed"), strDirectXVersion);
      if (dwDirectXVersion < 0x90000)
        nRenderer = R_GL_RENDERER;
    }
    else
    {
      strncpy(strResult, TEXT("DirectX not installed"), 128 );
      nRenderer = R_GL_RENDERER;
    }
    strResult[127] = 0;
  }
  else
  {
    sprintf( strResult, TEXT("Unknown version of DirectX installed"), hr );
    nRenderer = R_GL_RENDERER;
    strResult[127] = 0;
  }
  GetILog()->LogToFile("System: INFO: %s\n", strResult);

#endif //WIN32
  
	return nRenderer;
}

/////////////////////////////////////////////////////////////////////////////////
void CSystem::CreateRendererVars()
{
	// load renderer settings from engine.ini
	m_rWidth = GetIConsole()->RegisterInt("r_Width", 1024, VF_DUMPTODISK,
		"Sets the display width, in pixels. Default is 1024.\n"
		"Usage: r_Width [800/1024/..]");
	m_rHeight = GetIConsole()->RegisterInt("r_Height", 768, VF_DUMPTODISK,
		"Sets the display height, in pixels. Default is 768.\n"
		"Usage: r_Height [600/768/..]");
	m_rColorBits = GetIConsole()->RegisterInt("r_ColorBits", 32, VF_DUMPTODISK,
		"Sets the color resolution, in bits per pixel. Default is 32.\n"
		"Usage: r_ColorBits [32/24/16/8]");
	m_rDepthBits = GetIConsole()->RegisterInt("r_DepthBits", 32, VF_DUMPTODISK);
	m_rStencilBits = GetIConsole()->RegisterInt("r_StencilBits", 8, VF_DUMPTODISK);
#if defined(WIN32) || defined(WIN64)
	const char* p_r_DriverDef = "Auto";
#elif defined(PS3)
	const char* p_r_DriverDef = "DX10";
#else
	const char* p_r_DriverDef = "DX9";							// required to be deactivated for final release
#endif
	m_rDriver= GetIConsole()->RegisterString("r_Driver", p_r_DriverDef, VF_DUMPTODISK,
		"Sets the renderer driver ( OpenGL/DX9/DX10/AUTO/NULL ). Default is DX10 on Vista and DX9 otherwise.\n"
		"Specify in system.cfg like this: r_Driver = \"DX10\"");

	int iFullScreenDefault = 1;
	int iDisplayInfoDefault = 0;

	if (IsDevMode())
	{
		iFullScreenDefault=0;
		iDisplayInfoDefault=1;
	}

#ifdef SP_DEMO
	iDisplayInfoDefault=2;	
#endif

	m_rFullscreen = GetIConsole()->RegisterInt("r_Fullscreen",iFullScreenDefault, VF_DUMPTODISK,
		"Toggles fullscreen mode. Default is 1 in normal game and 0 in DevMode.\n"
		"Usage: r_Fullscreen [0=window/1=fullscreen]\n");

	m_rDisplayInfo = GetIConsole()->RegisterInt("r_DisplayInfo",iDisplayInfoDefault, VF_RESTRICTEDMODE|VF_DUMPTODISK,
		"Toggles debugging information display.\n"
		"Usage: r_DisplayInfo [0=off/1=show/2=enhanced]\n");
}

//////////////////////////////////////////////////////////////////////////
void CSystem::RenderBegin()
{
	FUNCTION_PROFILER_FAST( GetISystem(),PROFILE_SYSTEM,g_bProfilerEnabled );

	if (m_bIgnoreUpdates)
		return;

	//////////////////////////////////////////////////////////////////////
	//start the rendering pipeline
	if (m_env.pRenderer) 
		m_env.pRenderer->BeginFrame();
}

char *PhysHelpersToStr(int iHelpers, char *strHelpers);
int StrToPhysHelpers(const char *strHelpers);

//////////////////////////////////////////////////////////////////////////
void CSystem::RenderEnd()
{
	FUNCTION_PROFILER_FAST( GetISystem(),PROFILE_SYSTEM,g_bProfilerEnabled );

	if (m_bIgnoreUpdates)
		return;

	if (!m_env.pRenderer)
		return;

	//////////////////////////////////////////////////////////////////////
	// draw physics helpers
	if (m_env.pPhysicalWorld)
	{
		ITextModeConsole * pTMC = GetITextModeConsole();

		char str[32];
		if (StrToPhysHelpers(m_p_draw_helpers_str->GetString()) != m_env.pPhysicalWorld->GetPhysVars()->iDrawHelpers)
			m_p_draw_helpers_str->Set(PhysHelpersToStr(m_env.pPhysicalWorld->GetPhysVars()->iDrawHelpers,str));
		m_PhysRendererCamera = GetViewCamera();
		m_PhysRendererCamera.SetFrustum(GetViewCamera().GetViewSurfaceX(), GetViewCamera().GetViewSurfaceZ(),
			GetViewCamera().GetFov(), GetViewCamera().GetNearPlane(), m_pPhysRenderer->m_cullDist);
		m_env.pPhysicalWorld->DrawPhysicsHelperInformation(m_pPhysRenderer);
		m_pPhysRenderer->DrawBuffers(m_Time.GetFrameTime());

		phys_profile_info *pInfos;
		PhysicsVars *pVars;
		int i=-2;
		char msgbuf[512];
		if ((pVars=m_env.pPhysicalWorld->GetPhysVars())->bProfileEntities)
		{
			pe_status_pos sp;
			int j,mask,nEnts = m_env.pPhysicalWorld->GetEntityProfileInfo(pInfos);
			float fColor[4] = { 0.3f,0.6f,1.0f,1.0f }, dt;
			if (!pVars->bSingleStepMode) {
				for(i=0;i<nEnts;i++) {
					pInfos[i].nTicksAvg = ((int64)pInfos[i].nTicksAvg*15+pInfos[i].nTicks)>>4;
					pInfos[i].nCallsAvg = pInfos[i].nCallsAvg*(15.0f/16)+pInfos[i].nCalls*(1.0f/16);
				}
				phys_profile_info ppi;
				for(i=0;i<nEnts-1;i++) for(j=nEnts-1;j>i;j--) if (pInfos[j-1].nTicksAvg<pInfos[j].nTicksAvg) {
					ppi=pInfos[j-1]; pInfos[j-1]=pInfos[j]; pInfos[j]=ppi;
				}
			}
			for(i=0;i<nEnts;i++) {
				mask = (pInfos[i].nTicksPeak-pInfos[i].nTicks) >> 31;
				mask |= (70-pInfos[i].peakAge)>>31;
				mask &= (pVars->bSingleStepMode-1)>>31;
				pInfos[i].nTicksPeak += pInfos[i].nTicks-pInfos[i].nTicksPeak & mask;
				pInfos[i].nCallsPeak += pInfos[i].nCalls-pInfos[i].nCallsPeak & mask;
				sprintf(msgbuf, "%.2fms/%.1f (peak %.2fms/%d) %s (id %d)",
					dt=CFrameProfilerTimer::TicksToSeconds(pInfos[i].nTicksAvg)*1000.0f, pInfos[i].nCallsAvg, 
					CFrameProfilerTimer::TicksToSeconds(pInfos[i].nTicksPeak)*1000.0f, pInfos[i].nCallsPeak, 
					pInfos[i].pName, pInfos[i].id);
				GetIRenderer()->Draw2dLabel( 10.0f,60.0f+i*12.0f, 1.3f, fColor,false, "%s", msgbuf );
				if (pTMC) pTMC->PutText( 0, i, msgbuf );
				IPhysicalEntity *pent = m_env.pPhysicalWorld->GetPhysicalEntityById(pInfos[i].id);
				if (dt>0.1f && pent && pent->GetStatus(&sp))
					GetIRenderer()->DrawLabelEx(sp.pos+Vec3(0,0,sp.BBox[1].z), 1.4f,fColor,true,true, "%s %.2fms", pInfos[i].pName,dt);
				pInfos[i].peakAge = pInfos[i].peakAge+1 & ~mask;
				pInfos[i].nCalls=pInfos[i].nTicks = 0;
			}

			if (inrange(m_iJumpToPhysProfileEnt,0,nEnts+1))
			{
				ScriptHandle hdl;	hdl.n=-1;
				m_env.pScriptSystem->GetGlobalValue("g_localActorId",hdl);
				IEntity *pPlayerEnt = m_env.pEntitySystem->GetEntity(hdl.n);
				IPhysicalEntity *pent = m_env.pPhysicalWorld->GetPhysicalEntityById(pInfos[m_iJumpToPhysProfileEnt-1].id);
				if (pPlayerEnt && pent) {
					pe_params_bbox pbb; pent->GetParams(&pbb);
					pPlayerEnt->SetPos((pbb.BBox[0]+pbb.BBox[1])*0.5f+Vec3(0,-3.0f,1.5f));
					pPlayerEnt->SetRotation(Quat(IDENTITY));
				}
				m_iJumpToPhysProfileEnt = 0;
			}
		}
		if (pVars->bProfileFunx)
		{
			int j,mask,nFunx = m_env.pPhysicalWorld->GetFuncProfileInfo(pInfos);
			float fColor[4] = { 0.75f,0.08f,0.85f,1.0f };
			for(j=0,++i;j<nFunx;j++,i++) {
				mask = (pInfos[j].nTicksPeak-pInfos[j].nTicks) >> 31;
				mask |= (70-pInfos[j].peakAge)>>31;
				pInfos[j].nTicksPeak += pInfos[j].nTicks-pInfos[j].nTicksPeak & mask;
				pInfos[j].nCallsPeak += pInfos[j].nCalls-pInfos[j].nCallsPeak & mask;
				GetIRenderer()->Draw2dLabel( 10.0f,60.0f+i*12.0f, 1.3f, fColor,false,
				 "%s %.2fms/%d (peak %.2fms/%d)", pInfos[j].pName, CFrameProfilerTimer::TicksToSeconds(pInfos[j].nTicks)*1000.0f,pInfos[j].nCalls, 
				 CFrameProfilerTimer::TicksToSeconds(pInfos[j].nTicksPeak)*1000.0f,pInfos[j].nCallsPeak);
				pInfos[j].peakAge = pInfos[j].peakAge+1 & ~mask;
				pInfos[j].nCalls=pInfos[j].nTicks = 0;
			}
		}
	}

	if (m_env.pRenderer->GetIRenderAuxGeom())
		m_env.pRenderer->GetIRenderAuxGeom()->Flush();

#ifndef EXCLUDE_GPU_PARTICLE_PHYSICS
	IGPUPhysicsManager *pGPUPhysicsManager;

	pGPUPhysicsManager = GetIGPUPhysicsManager();

	if( pGPUPhysicsManager != NULL )
	{
		pGPUPhysicsManager->RenderMessages();
	}
#endif

	// Flush render data and swap buffers.
	m_env.pRenderer->RenderDebug();

#if defined(PS3)
	NPPU::CJobManSPU *const __restrict pJobManSPU = (NPPU::CJobManSPU*)GetIJobManSPU();
	#if defined(SUPP_SPU_FRAME_STATS)
		pJobManSPU->GetAndResetSPUFrameStats(m_FrameProfileSystem.m_SPUFrameStats, m_FrameProfileSystem.m_SPUJobFrameStats);
	#endif//SUPP_SPU_FRAME_STATS
	gPS3Env->spuEnabled = m_sys_spu_enable->GetIVal();//update spu control once per frame
	//do only dump one frame
	const int cDumpProfStats = m_sys_spu_dump_stats->GetIVal();
	if(cDumpProfStats)
	{
		if(gPS3Env->spuDumpProfStats)
		{
			//reset
			m_sys_spu_dump_stats->Set(0);
			gPS3Env->spuDumpProfStats = 0;
		}
		else
			gPS3Env->spuDumpProfStats = true;
	}
	else
		gPS3Env->spuDumpProfStats = false;
	//enable spu debugging if set
	const char* const cpDebugName = m_sys_spu_debug->GetString();
	const uint32 cStrLen = strlen(cpDebugName);
	if(gPS3Env->spuEnabled && cStrLen > 0)
	{
		//enable and reset var
		pJobManSPU->EnableSPUJobDebugging(pJobManSPU->GetJobHandle(cpDebugName, cStrLen));
		m_sys_spu_debug->Set("");
	}
	else
	if(cStrLen == 1 && *cpDebugName == ' ' )//we must be able to deactivate it by ' '
		m_sys_spu_debug->Set("");

	//for PS3, the memory allocated on SPUs needs to be transmitted to memory outside those special buckets
	pJobManSPU->UpdateSPUMemMan();
	//test SPUs if they are still running and have not been halted due to some failure
	pJobManSPU->TestSPUs();
#endif //PS3

	RenderStats();
	RenderStatistics();
	MonitorBudget();
	RenderFlashInfo();
	//////////////////////////////////////////////////////////////////////
	//draw labels
	m_env.pRenderer->FlushTextMessages();

	if (m_env.pConsole)
	{
		m_env.pConsole->Draw();
		m_env.pRenderer->FlushTextMessages();
	}

	m_env.pRenderer->EndFrame();
}

//////////////////////////////////////////////////////////////////////////
void CSystem::UpdateLoadingScreen()
{
	if (!m_bEditor)
	{
		if (m_pProgressListener)
		{
			m_pProgressListener->OnLoadingProgress(0);
		}

		if (reinterpret_cast<intptr_t>(GetIRenderer()->EF_Query(EFQ_RecurseLevel)) < 0)
		{
			if (m_env.pConsole->IsOpened())
			{
				RenderBegin();
				GetIConsole()->Draw();
				RenderEnd();
			}
		}
	}
	// This happens during loading, give windows opportunity to process window messages.
#ifdef WIN32
	if (m_hWnd && ::IsWindow((HWND)m_hWnd))
	{
		MSG msg;
    // Don't make any steps while 3D device is lost
    //while (true)
    {
      while (PeekMessage(&msg, (HWND)m_hWnd, 0, 0, PM_REMOVE))
      {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
      //int *nLost = (int *)GetIRenderer()->EF_Query(EFQ_DeviceLost, 0);
      //if (!*nLost)
      //  break;
    }
	}
#endif
}

//! Renders the statistics; this is called from RenderEnd, but if the 
//! Host application (Editor) doesn't employ the Render cycle in ISystem,
//! it may call this method to render the essential statistics
//////////////////////////////////////////////////////////////////////////
void CSystem::RenderStatistics ()
{
	// Render profile info.
#if defined(PS3) && defined(SUPP_SPU_FRAME_STATS)
	m_FrameProfileSystem.SetSPUMode( m_sys_spu_profile->GetIVal() != 0 );
#endif
	m_FrameProfileSystem.Render();
	if (m_pThreadProfiler)
		m_pThreadProfiler->Render();
	RenderMemStats();

	if (m_sys_profile_sampler->GetIVal() > 0)
	{
		m_sys_profile_sampler->Set(0);
		m_FrameProfileSystem.StartSampling( m_sys_profile_sampler_max_samples->GetIVal() );
	}
	int profValue = m_sys_profile->GetIVal();
	if (profValue != m_profile_old)
	{
		m_profile_old = profValue;
		if (profValue)
		{
			// Turn on frame profiler.
			if (!m_FrameProfileSystem.IsEnabled())
			{
				if (profValue > 0)
					m_FrameProfileSystem.Enable(true,true);
				else
					m_FrameProfileSystem.Enable(true,false); // Not display.
			}
			if (profValue < 0)
				profValue = -profValue;
			m_FrameProfileSystem.SetDisplayQuantity( (CFrameProfileSystem::EDisplayQuantity)(profValue-1) );
		}
		else if (m_FrameProfileSystem.IsEnabled())
		{
			// Turn off frame profiler.
			m_FrameProfileSystem.Enable(false,false);
		}
	}
	if (m_FrameProfileSystem.IsEnabled())
	{
		static string sSysProfileFilter;
		if (stricmp(m_sys_profile_filter->GetString(),sSysProfileFilter.c_str()) != 0)
		{
			sSysProfileFilter = m_sys_profile_filter->GetString();
			m_FrameProfileSystem.SetSubsystemFilter( sSysProfileFilter.c_str() );
		}
		m_FrameProfileSystem.SetHistogramScale( m_sys_profile_graphScale->GetFVal() );
		m_FrameProfileSystem.SetDrawGraph( m_sys_profile_graph->GetIVal() != 0 );
#if defined(PS3) && defined(SUPP_SPU_FRAME_STATS)
		m_FrameProfileSystem.SetSPUMode( m_sys_spu_profile->GetIVal() != 0 );
#endif
		m_FrameProfileSystem.SetThreadSupport( m_sys_profile_allThreads->GetIVal() );
		m_FrameProfileSystem.SetNetworkProfiler( m_sys_profile_network->GetIVal() != 0 );
		m_FrameProfileSystem.SetPeakTolerance( m_sys_profile_peak->GetFVal() );
		m_FrameProfileSystem.SetPageFaultsGraph( m_sys_profile_pagefaultsgraph->GetIVal() != 0 );
	}
	static int memProfileValueOld = 0;
	int memProfileValue = m_sys_profile_memory->GetIVal();
	if (memProfileValue != memProfileValueOld)
	{
		memProfileValueOld = memProfileValue;
		m_FrameProfileSystem.EnableMemoryProfile( memProfileValue!=0 );
	}
}

//////////////////////////////////////////////////////////////////////
void CSystem::Render()
{
	if (m_bIgnoreUpdates)
		return;

	//check what is the current process 
	if (!m_pProcess)
		return; //should never happen

	//check if the game is in pause or
	//in menu mode
	//bool bPause=false;
	//if (m_pProcess->GetFlags() & PROC_MENU)
	//	bPause=true;

	FUNCTION_PROFILER_FAST( GetISystem(),PROFILE_SYSTEM,g_bProfilerEnabled );

	//////////////////////////////////////////////////////////////////////
	//draw	
  if (m_pProcess && (m_pProcess->GetFlags() & PROC_3DENGINE))
  {	
		if (!IsEquivalent(m_ViewCamera.GetPosition(),Vec3(0,0,0),VEC_EPSILON) || gEnv->pSystem->IsDedicated())		
		{
#ifdef XENON
      /*Matrix34 m;
      //m.SetIdentity();
      m = Matrix34::CreateRotationZ(gEnv->pTimer->GetCurrTime()*0.1f);
      m.SetTranslation(Vec3(235, 176, 23));

      m_ViewCamera.SetMatrix(m);*/
#endif


      if (m_env.p3DEngine)
			{
				GetIRenderer()->SetViewport(0,0,GetIRenderer()->GetWidth(),GetIRenderer()->GetHeight());
				m_env.p3DEngine->RenderWorld(SHDF_ALLOW_WATER|SHDF_ALLOWPOSTPROCESS | SHDF_ALLOWHDR | SHDF_SORT | SHDF_ZPASS | SHDF_ALLOW_AO, m_ViewCamera,"CSystem:Render I3DEngine");
			}


//			m_pProcess->Draw();		
						
			if (m_env.pAISystem)		
				m_env.pAISystem->DebugDraw(GetIRenderer());		
		}
  }
  else
  {
    if (m_pProcess)	
		{
			GetIRenderer()->SetViewport(0,0,GetIRenderer()->GetWidth(),GetIRenderer()->GetHeight());
      m_pProcess->RenderWorld(SHDF_ALLOW_WATER |SHDF_ALLOWPOSTPROCESS | SHDF_ALLOWHDR | SHDF_SORT | SHDF_ZPASS | SHDF_ALLOW_AO, m_ViewCamera,"CSystem:Render IProcess");		
		}
  }
}

//////////////////////////////////////////////////////////////////////////
void CSystem::RenderMemStats()
{
	// check for the presence of the system
	if (!m_env.pConsole || !m_env.pRenderer || !m_cvMemStats->GetIVal())
	{
		SAFE_DELETE(m_pMemStats);
		return;
	}

	TickMemStats();

	assert (m_pMemStats);
	// render the statistics
	{
		CrySizerStatsRenderer StatsRenderer (this, m_pMemStats, m_cvMemStatsMaxDepth->GetIVal(), m_cvMemStatsThreshold->GetIVal());
		StatsRenderer.render((m_env.pRenderer->GetFrameID(false)+2)%m_cvMemStats->GetIVal() <= 1);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystem::RenderStats()
{
	if (!m_env.pConsole || !m_env.p3DEngine)
		return;

	// Draw engine stats
	if (int iDisplayInfo = m_rDisplayInfo->GetIVal())
  {
    // Draw 3dengine stats and get last text cursor position
    float nTextPosX=101-20, nTextPosY=-2, nTextStepY=3;
    m_env.p3DEngine->DisplayInfo(nTextPosX, nTextPosY, nTextStepY,iDisplayInfo!=1);

    // Draw non 3dengine stats
	  if (m_rDisplayInfo->GetIVal()==2)
	  {
		  int nSoundCurrMem,nSoundMaxMem;
		  m_env.pRenderer->TextToScreen( nTextPosX-15, nTextPosY+=nTextStepY, "SysMem %.1f mb",  
        float(DumpMMStats(NULL))/1024.f);

			if (m_env.pSoundSystem)
			{
				m_env.pSoundSystem->GetSoundMemoryUsageInfo(&nSoundCurrMem,&nSoundMaxMem);	
				m_env.pRenderer->TextToScreen( nTextPosX-18, nTextPosY+=nTextStepY, "Sound %2.1f/%2.1f mb",
					(float)(nSoundCurrMem)/(1024.0f*1024.0f), (float)(nSoundMaxMem)/(1024.0f*1024.0f));
			}
	  }
  }
}

//////////////////////////////////////////////////////////////////////////
void CSystem::MonitorBudget()
{
	if(m_sys_enable_budgetmonitoring->GetIVal())
		m_pIBudgetingSystem->MonitorBudget();
}
