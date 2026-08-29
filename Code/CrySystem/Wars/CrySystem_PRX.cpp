#if defined PS3_PRX
#define PS3_INIT_MODULE 1
#include <InitPRX.h>

SYS_MODULE_INFO(CrySystem, 0, 1, 1);
SYS_MODULE_START(CrySystem_Start);
SYS_MODULE_STOP(CrySystem_Stop);

#if 0
PS3SystemEnvironment *gPS3Env = NULL;

extern "C" { void __init(); void __fini(); }
struct _CryMemoryManagerPoolHelper { static void Init(); };

extern "C" int CrySystem_Start(unsigned, void *argp)
{
  assert(argp != NULL);
  gPS3Env = reinterpret_cast<PS3SystemEnvironment *>(argp);
fprintf(stderr, "CrySystem_Start\n");
PS3_BREAK;
  _CryMemoryManagerPoolHelper::Init();
PS3_BREAK;
  __init();
  return SYS_PRX_RESIDENT;
}

extern "C" int CrySystem_Stop(unsigned, void *)
{
  __fini();
  return SYS_PRX_STOP_OK;
}
#endif

extern "C" int CrySystem_Start(unsigned, void *argp)
{ return StartPRX("CrySystem", argp); }

extern "C" int CrySystem_Stop(unsigned, void *)
{ return StopPRX("CrySystem"); }

SYS_LIB_DECLARE(libCrySystem, SYS_LIB_AUTO_EXPORT|SYS_LIB_WEAK_IMPORT);

SYS_LIB_EXPORT(CreateSystemInterface, libCrySystem);

SYS_LIB_EXPORT(CryMalloc, libCrySystem);
SYS_LIB_EXPORT(CryRealloc, libCrySystem);
SYS_LIB_EXPORT(CryFree, libCrySystem);

SYS_LIB_EXPORT(CryGetMemSize, libCrySystem);
SYS_LIB_EXPORT(CrySystemCrtMalloc, libCrySystem);
SYS_LIB_EXPORT(CrySystemCrtFree, libCrySystem);
SYS_LIB_EXPORT(CryGetUsedHeapSize, libCrySystem);
SYS_LIB_EXPORT(CryGetWastedHeapSize, libCrySystem);
SYS_LIB_EXPORT(CryCleanup, libCrySystem);
SYS_LIB_EXPORT(CryStats, libCrySystem);
SYS_LIB_EXPORT(CryFlushAll, libCrySystem);

#endif // PS3_PRX

// vim:ts=2:sw=2

