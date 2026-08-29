# Embedded CrySystem Port --- CryMP Client

## Status

This document summarizes the work done to replace the stock
`CrySystem.dll` with a source-built CrySystem embedded directly into
`CryMP-Client.exe`.

The current milestone is a working embedded CrySystem which:

-   builds as part of the CryMP client executable;
-   starts Crysis successfully;
-   uses the real Scaleform/GFx integration and displays the menu;
-   keeps the stock CryAction, CryNetwork, Cry3DEngine and renderer DLLs
    for now;
-   uses CryMP's existing replacement implementations for CryPak,
    StreamEngine, ScriptSystem, Physics, SoundSystem, HardwareMouse and
    LocalizationManager;
-   restores CryMP's early-engine initialization and file logging;
-   initializes the DataProbe required by the stock CryAction DLL;
-   supports both Win32 and Win64 Scaleform libraries;
-   can connect to LAN and Internet servers after restoring the embedded
    CryMP client pak initialization.

The long-term plan is to translate the remaining binary patches against
`CrySystem.dll` into direct source changes, then continue embedding
other engine modules such as Cry3DEngine and the renderer.

------------------------------------------------------------------------

## 1. Original architecture

CryMP historically launched the original Crytek engine DLLs and
modified/replaced parts of CrySystem at runtime.

The launcher loaded the stock `CrySystem.dll`, applied hard-coded binary
patches, installed hooks, and replaced several subsystems before
CrySystem finished initialization.

The relevant old launcher path was conceptually:

``` cpp
if (g_pCrySystem)
{
    MemoryPatch::CrySystem::AllowDX9VeryHighSpec(g_pCrySystem);
    MemoryPatch::CrySystem::AllowMultipleInstances(g_pCrySystem);
    MemoryPatch::CrySystem::FixCPUInfoOverflow(g_pCrySystem);
    MemoryPatch::CrySystem::FixFlashAllocatorUnderflow(g_pCrySystem);
    MemoryPatch::CrySystem::HookCPUDetect(g_pCrySystem, &CPUInfo::Detect);

    if (!WinAPI::CmdLine::HasArg("-rawdump"))
        MemoryPatch::CrySystem::HookError(g_pCrySystem, &CrashLogger::OnEngineError);

    MemoryPatch::CrySystem::RemoveSecuROM(g_pCrySystem);
    MemoryPatch::CrySystem::UnhandledExceptions(g_pCrySystem);
    MemoryPatch::CrySystem::HookCryWarning(g_pCrySystem, &OnCryWarning);

    InstallEarlyEngineInitHook(g_pCrySystem);
    HookSystemWarning(g_pCrySystem);

    ReplaceCryPak(g_pCrySystem);
    ReplaceStreamEngine(g_pCrySystem);
    ReplaceScriptSystem(g_pCrySystem);
    ReplaceHardwareMouse(g_pCrySystem);
    ReplaceLocalizationManager(g_pCrySystem);

    if (!WinAPI::CmdLine::HasArg("-oldphysics"))
        ReplacePhysics(g_pCrySystem);

#ifdef CLIENT_LAUNCHER
    if (!WinAPI::CmdLine::HasArg("-oldsound"))
        ReplaceSoundSystem(g_pCrySystem);
#endif
}
```

Once CrySystem is compiled directly into `CryMP-Client.exe`, there is no
stock CrySystem DLL to patch. These behaviors therefore have to be
implemented directly in source.

------------------------------------------------------------------------

## 2. Main design decisions

### CrySystem is compiled directly into the client EXE

The Wars CrySystem source is included directly with `target_sources()`.

No new CrySystem static or object library is introduced.

Conceptually:

``` cmake
file(GLOB_RECURSE CRYMP_WARS_CRYSYSTEM_SOURCES CONFIGURE_DEPENDS
    Code/CrySystem/Wars/*.cpp
    Code/CrySystem/Wars/*.c
)

target_sources(${CRYMP_CLIENT_EXE} PRIVATE
    ${CRYMP_WARS_CRYSYSTEM_SOURCES}
)
```

This matches the intended final architecture: the engine implementation
gradually becomes part of the monolithic CryMP executable.

### Do not globally define `_LIB`

The stock Crytek modules still loaded as DLLs depend on the original
Crysis ABI.

For the current milestone, `_LIB` is deliberately not globally enabled.

The source-built CSystem selectively calls CryMP implementations where
they already exist, while the remaining modules continue through the DLL
paths.

### Crysis 1 ABI remains authoritative

The Wars source is used as a source reference and implementation base,
but the current CryMP/Crysis 1 interfaces remain authoritative while
stock Crysis DLLs are still present.

In particular, `SSystemGlobalEnvironment` must not be changed to the
Wars layout.

Wars-only fields such as:

``` cpp
pFileChangeMonitor
pGameFramework
```

must not be inserted into the Crysis 1 environment structure.

Doing so would shift subsequent members and break every stock DLL using
`gEnv`.

The file change monitor is instead kept privately by `CSystem`.

The current CryMP `ISystem` vtable is likewise treated as ABI-sensitive.

------------------------------------------------------------------------

## 3. Embedded CrySystem entry point

A new embedded entry path creates `CSystem` directly from the
executable.

The launcher has a `CRYMP_EMBEDDED_CRYSYSTEM` path which skips loading
and version-checking `CrySystem.dll` and instead calls the in-EXE system
creation entry point.

`EmbeddedEntry.cpp` creates `CSystem`, initializes the global system
state, and calls the normal CrySystem initialization sequence.

This allows the rest of Crysis to continue receiving an `ISystem*` just
as it did with the original DLL.

------------------------------------------------------------------------

## 4. Sources deliberately excluded from the Wars CrySystem build

Not every Wars CrySystem source is compiled.

The current exclusions include the following categories.

### DLL/platform entry files

``` text
DllMain.cpp
CrySystem_PRX.cpp
UnixConsole.cpp
StdAfx.cpp
```

These are either DLL-specific, unsupported-platform-specific, or
unnecessary in the monolithic executable.

### CryMP implementations already considered authoritative

``` text
CryPak.cpp
StreamEngine.cpp
RefStreamEngine.cpp
RefReadStream*.cpp
HardwareMouse.cpp
CryMemoryManager.cpp
```

CryMP already contains replacement/reconstructed versions of these
systems.

The goal is not to regress to the Wars implementations merely because
the Wars CSystem source is being used.

### Other exclusions

``` text
AutoDetectSpec.cpp
Statistics.cpp
TestSystem.cpp
ThreadSampler.cpp
```

`ThreadSampler.cpp` is textually included by `ThreadProfiler.cpp`, so
compiling it separately would duplicate it.

`AutoDetectSpec()` is currently supplied by a compatibility
implementation.

------------------------------------------------------------------------

## 5. C++20 modernization and compatibility work

The Wars CrySystem code predates modern C++ and modern MSVC. A
substantial compatibility pass was required.

### Standard library cleanup

Examples include:

``` text
std::auto_ptr      -> std::unique_ptr
std::binary_function inheritance removed
register keyword removed
```

Legacy flat CryCommon includes were also converted to the current CryMP
include layout where required.

### CryThread compatibility

A compatibility `CryThread.h` was added using the current Windows/std
threading primitives.

It provides the legacy APIs expected by the Wars source, including
functionality such as:

``` text
CrySleep
critical-section helpers
thread naming
CryThread::IsRunning()
```

### CryLibrary compatibility

A compatibility `CryLibrary.h` supplies the legacy helpers expected by
the source:

``` text
CryLoadLibrary
CryGetProcAddress
CryFreeLibrary
CryCreateDirectory
CryGetCurrentDirectory
CryGetFileAttributes
CrySetFileAttributes
```

### Logging compatibility

The old CrySystem code expected legacy logging APIs.

Compatibility was added for items such as:

``` text
IMiniLog
ILog::LogWithType
CryWarning
CryComment
CryError
```

without changing the ABI-sensitive `ISystem` vtable.

### General CryCommon compatibility

Additional compatibility work included:

``` text
CCryFile
CNameTable
namespace PathUtil = CryPath
ModuleAlloc
fxopen
CPU flag compatibility
DATA_FOLDER
FtoI
ModuleInitISystem
```

### XML/serialization compatibility

Compatibility additions included:

``` text
local allocator support
SimpleSerialize.h
PNoise3.h
EntityId
CHECK_SCRIPT_STACK
AUTO_STRUCT_INFO
```

### x64 timer/profiler fix

The old ThreadSampler timing code was adapted to use:

``` cpp
__rdtsc()
```

on x64.

### Windows macro collision

The current StreamEngine enum contained values such as:

``` text
PENDING
ERROR
```

which collide with Windows macros.

Those enum names were adjusted in the CryMP implementation.

### zlib include regression

During one pass, `crc32.c` was accidentally changed to include the
CryCommon `crc32.h`.

This was corrected back to the local zlib header:

``` cpp
#include "crc32.h"
```

------------------------------------------------------------------------

## 6. StreamEngine integration

The Wars StreamEngine is not used.

CryMP's existing StreamEngine remains authoritative.

Because Wars `CSystem` originally owned a concrete Wars StreamEngine
object, its member and initialization code were adjusted to work with
the current CryMP implementation through `IStreamEngine*`.

Concrete Wars-only calls such as the quota API were removed where they
were not present in the current implementation.

The embedded CSystem must not delete the CryMP singleton as if it owned
a newly allocated Wars object.

------------------------------------------------------------------------

## 7. CryPak integration

The Wars CryPak implementation is excluded.

The embedded CSystem uses CryMP's existing singleton:

``` cpp
CryPak* pCryPak = &CryPak::GetInstance();
m_env.pCryPak = pCryPak;
```

A critical difference was discovered later: merely assigning the
singleton was not enough.

The old CryMP launcher replacement factory performed additional
initialization:

``` cpp
static ICryPak* CreateNewCryPak(
    ISystem* pSystem,
    CryPakConfig* config,
    bool lvlRes,
    bool gameFolderWritable)
{
    gEnv = pSystem->GetGlobalEnvironment();

    CryLogAlways("$3[CryMP] Initializing CryPak");

    const auto internalPak =
        WinAPI::GetDataResource(nullptr, RESOURCE_INTERNAL_PAK);

    CryPak* pCryPak = &CryPak::GetInstance();

    pCryPak->SetGameFolderWritable(gameFolderWritable);
    pCryPak->LoadClientPak(internalPak.data(), internalPak.size());

    return pCryPak;
}
```

Because `ReplaceCryPak()` no longer runs against a stock CrySystem DLL,
this factory was no longer being called.

The embedded `InitFileSystem()` therefore has to reproduce this behavior
directly.

The required includes are:

``` cpp
#include "Library/WinAPI.h"
#include "Resources.h"
```

and the embedded initialization includes:

``` cpp
gEnv = GetGlobalEnvironment();

CryPak* pCryPak = &CryPak::GetInstance();
m_env.pCryPak = pCryPak;

pCryPak->SetGameFolderWritable(m_bGameFolderWritable);

const auto internalPak =
    WinAPI::GetDataResource(nullptr, RESOURCE_INTERNAL_PAK);

if (!internalPak.empty())
{
    pCryPak->LoadClientPak(
        internalPak.data(),
        internalPak.size()
    );
}
```

The normal CryPak initialization then continues.

Restoring `RESOURCE_INTERNAL_PAK` loading was a major runtime fix: after
it was restored, Internet-server connections worked again.

------------------------------------------------------------------------

## 8. ScriptSystem integration

Initially the embedded Wars CSystem still followed its normal non-`_LIB`
path and loaded:

``` text
CryScriptSystem.dll
```

This differed from normal CryMP behavior because the old launcher used
`ReplaceScriptSystem()` to redirect CrySystem to CryMP's source-built
ScriptSystem.

Since the CrySystem DLL no longer exists to patch, the embedded CSystem
was changed to initialize CryMP's ScriptSystem directly.

The embedded path now uses the existing CryMP implementation rather than
stock `CryScriptSystem.dll`.

The old DLL path remains conceptually relevant only as a
fallback/reference path.

------------------------------------------------------------------------

## 9. Physics integration

The same problem existed for physics.

Without the old `ReplacePhysics()` patch, the embedded Wars CSystem
would load stock:

``` text
CryPhysics.dll
```

instead of using CryMP's reconstructed/fixed physics implementation.

The embedded CSystem now directly initializes CryMP physics.

The existing compatibility switch is preserved:

``` text
-oldphysics
```

When requested, it can still use the original stock physics DLL.

This is important because CryMP contains substantial physics fixes that
would otherwise silently be bypassed by the embedded CrySystem.

------------------------------------------------------------------------

## 10. SoundSystem integration

Likewise, the old launcher normally used:

``` cpp
ReplaceSoundSystem(g_pCrySystem);
```

for the client unless:

``` text
-oldsound
```

was specified.

The embedded CSystem now initializes CryMP's current SoundSystem
directly.

The `-oldsound` fallback is preserved so the stock CrySoundSystem DLL
can still be selected when needed.

------------------------------------------------------------------------

## 11. HardwareMouse integration

The Wars HardwareMouse implementation is excluded.

CryMP's existing HardwareMouse remains authoritative and is initialized
directly by the embedded system rather than through the old
`ReplaceHardwareMouse()` binary patch.

------------------------------------------------------------------------

## 12. LocalizationManager integration

The Wars CrySystem normally constructs its own:

``` text
CLocalizedStringsManager
```

CryMP already has a custom `LocalizationManager`, originally intended to
replace the stock implementation until CryMP had its own CrySystem.

The embedded system was therefore changed so CryMP's existing
LocalizationManager is authoritative.

At the documented point in development, the manager replacement itself
is in place, but localization text is not yet working correctly.

This remains an active integration item.

The old CryMP language-selection behavior also needs to remain
authoritative rather than blindly adopting Wars-specific language
initialization behavior.

------------------------------------------------------------------------

## 13. Early engine initialization and logging

An important startup difference appeared after removing the stock
CrySystem DLL.

Historically:

``` cpp
InstallEarlyEngineInitHook(g_pCrySystem);
```

patched the original CrySystem binary so that CryMP's:

``` cpp
Launcher::OnEarlyEngineInit()
```

would run at the appropriate time.

With embedded CrySystem, that binary hook no longer runs.

As a result, early CryMP setup silently disappeared, including opening
the log file.

This explained why neither the Crysis directory nor the My Games
directory initially contained `CryMP-Client.log`.

The embedded startup path was changed to call
`Launcher::OnEarlyEngineInit()` directly.

This restored file logging.

A confirmed log after the fix reported:

``` text
Executable: ...\CryMP-Client64.exe
Main directory: C:\Program Files (x86)\Crytek\Crysis
Root directory: C:\Program Files (x86)\Crytek\Crysis
User directory: ...\My Games\Crysis
```

and normal CryMP startup output was written to the log again.

The other old CrySystem binary patches have intentionally not yet been
translated. They will be applied directly to the source-built CrySystem
later.

------------------------------------------------------------------------

## 14. Remaining old CrySystem binary patches

The old launcher still contains behavior that used to patch the stock
CrySystem DLL:

``` text
AllowDX9VeryHighSpec
AllowMultipleInstances
FixCPUInfoOverflow
FixFlashAllocatorUnderflow
HookCPUDetect
HookError
RemoveSecuROM
UnhandledExceptions
HookCryWarning
HookSystemWarning
```

These should not be redirected to the EXE using their old offsets.

The offsets belong to a specific CrySystem DLL binary and are
meaningless once CrySystem is source-built.

Where still necessary, the actual behavior of each patch should be
implemented directly in the corresponding CrySystem source.

This work is intentionally deferred until the core embedded system is
stable.

------------------------------------------------------------------------

## 15. DataProbe / stock CryAction runtime crash

One of the most important runtime bugs appeared after the embedded
CrySystem first compiled and launched.

The game could display the menu background but crashed several seconds
later while synchronizing the network/game state.

The call stack went through stock CryAction and CryNetwork into:

``` cpp
CSystem::Update()
```

Commenting out both:

``` cpp
m_env.pNetwork->SyncWithGame(eNGS_FrameStart);
m_env.pNetwork->SyncWithGame(eNGS_FrameEnd);
```

made the game stable.

Delaying FrameStart did not fix it, showing that this was not merely
initialization timing.

Disassembly of the stock CryAction crash showed an indirect call through
the CSystem vtable.

The call corresponded to vtable offset:

``` text
0x2F8
```

or slot:

``` text
95
```

in the current Crysis `ISystem`.

That slot is:

``` cpp
ISystem::GetIDataProbe()
```

The embedded Wars CSystem had:

``` cpp
virtual IDataProbe* GetIDataProbe()
{
    return m_pDataProbe;
}
```

but the constructor only created the DataProbe under:

``` cpp
#if defined(_DATAPROBE)
    m_pDataProbe = new CDataProbe;
#endif
```

Without `_DATAPROBE`, stock CryAction received `nullptr` and
dereferenced it.

The confirmed permanent fix was to compile the embedded client with:

``` cmake
target_compile_definitions(${CRYMP_CLIENT_EXE} PRIVATE
    CRYMP_EMBEDDED_CRYSYSTEM
    _DATAPROBE
    EXCLUDE_GPU_PARTICLE_PHYSICS
    NOMINMAX
)
```

After `_DATAPROBE` was enabled, both network sync calls were restored
and the crash disappeared.

This is a required compatibility definition while the stock Crysis
CryAction DLL is in use.

------------------------------------------------------------------------

## 16. Network architecture and server connection findings

CryNetwork remains the stock Crysis DLL.

The embedded CSystem loads it normally and creates the network through
the Crytek exported factory.

CryAction also remains the stock DLL, with CryMP's existing
reconstructed/wrapper portions still present.

LAN connections worked before Internet connections did, which
demonstrated that the fundamental CryNetwork/CryAction/IGameContext path
was functioning.

The Internet-server failure was initially suspected to involve GameSpy,
PunkBuster, synchronized CVars or another online-only path.

However, restoring the old CryMP CryPak initialization --- especially:

``` cpp
LoadClientPak(...)
```

for `RESOURCE_INTERNAL_PAK` --- restored Internet-server connectivity.

This showed that the internal CryMP client pak is part of required
client initialization and cannot be omitted when replacing the old
`ReplaceCryPak()` path.

GameSpy and PunkBuster should therefore not be enabled or changed merely
to work around this issue.

------------------------------------------------------------------------

## 17. PunkBuster

PunkBuster-specific CrySystem integration from Wars was deliberately
disabled for the embedded build.

This avoided pulling obsolete PB-specific code into the initial port.

There is currently no evidence that PunkBuster was responsible for the
observed Internet connection problem.

------------------------------------------------------------------------

## 18. AutoDetectSpec and system compatibility

The Wars `AutoDetectSpec.cpp` path was excluded because it pulled in
legacy DirectX 10 SDK assumptions not appropriate for the current build.

Compatibility implementations were added instead.

`EmbeddedCompat.cpp` provides system compatibility helpers including:

``` text
Win32SysInspect::GetNumCPUCores
Win32SysInspect::IsDX10Supported
Win32SysInspect::GetGPUInfo
Win32SysInspect::GetGPURating
Win32SysInspect::GetOS
Win32SysInspect::IsVistaKB940105Required
CSystem::AutoDetectSpec
```

`IsDX10Supported()` currently returns false so the embedded system
follows the DX9 path.

It also provides:

``` text
gDLLHandle
CryMemoryGetAllocatedSize()
CryStats()
```

compatibility required by the Wars source.

------------------------------------------------------------------------

## 19. Scaleform / GFx integration

The first successful embedded CrySystem build used temporary Flash
stubs:

``` cpp
IFlashPlayer* CSystem::CreateFlashPlayerInstance() const
{
    return nullptr;
}

void CSystem::SetFlashLoadMovieHandler(IFlashLoadMovieHandler*) const
{
}

void CSystem::RenderFlashInfo()
{
}

void CSystem::GetFlashMemoryUsage(ICrySizer*)
{
}
```

This allowed CrySystem to link and launch, but the real menu could not
appear.

The Wars CrySystem contains the real Scaleform integration under:

``` text
Code/CrySystem/Wars/Scaleform
```

including:

``` text
ConfigScaleform
FlashPlayerInstance
GAllocatorCryMem
GFileCryPak
GImageInfoXRender
GRendererXRender
GTextureXRender
SharedResources
SharedStates
```

The original Wars project expected the external Scaleform SDK at:

``` text
SDKs/Scaleform/Include
SDKs/Scaleform/Lib
```

The Scaleform SDK headers and libraries were added to the project and
the `/Scaleform/` source exclusion was removed.

The temporary Flash stubs were then removed so the real Wars
implementations owned those CSystem methods.

The result was a working real Scaleform menu.

------------------------------------------------------------------------

## 20. Win32 and Win64 Scaleform support

Both architectures are supported.

The SDK contains:

``` text
GFx_win32_debug.lib
GFx_win32_release.lib
GFx_win64_debug.lib
GFx_win64_release.lib

libjpeg_win32_debug.lib
libjpeg_win32_release.lib
libjpeg_win64_debug.lib
libjpeg_win64_release.lib
```

The correct libraries are selected according to build architecture and
configuration.

Debug builds use the debug GFx/libjpeg pair.

Release and RelWithDebInfo use the release pair.

The third-party Scaleform `.lib` files are allowed even though CrySystem
itself is compiled directly into the EXE; the restriction against
creating another static/object library applies to our engine source
organization, not to required third-party SDK binaries.

------------------------------------------------------------------------

## 21. Scaleform C++20 compatibility

Several old SDK assumptions failed under current MSVC/C++20.

### `CryGetAsyncKeyState`

Wars Scaleform expected:

``` text
CryGetAsyncKeyState
```

A compatibility wrapper was added around the Win32 `GetAsyncKeyState()`
API.

### `CryGetMemSize`

`GAllocatorCryMem.cpp` expected:

``` text
CryGetMemSize
```

The current CryMP memory-manager functionality was exposed through the
expected compatibility header/API.

### `GAtomic.h`

Old Scaleform template code relied on dependent-base lookup accepted by
older MSVC.

Current C++ requires explicit qualification.

Calls such as:

``` text
CompareAndSet_NoSync
Exchange_NoSync
ExchangeAdd_NoSync
```

were qualified through the relevant base/type so the SDK headers compile
under C++20.

------------------------------------------------------------------------

## 22. Scaleform old-CRT compatibility

The original GFx and bundled libjpeg static libraries were built with an
old MSVC runtime.

Modern MSVC initially produced unresolved imports such as:

``` text
__imp__vsnprintf_s
__imp_vsnprintf_s
__imp_rewind
__imp__ftime64
__imp___iob_func
```

### Incorrect UCRT experiment

Explicitly linking:

``` text
ucrt
```

was tested and found to be wrong.

The project already had its normal CRT setup, and adding the import UCRT
alongside the static UCRT produced a large number of duplicate
definitions such as:

``` text
fopen
strftime
exit
errno
fflush
read
write
...
```

Therefore explicit `ucrt` linking was removed.

### `legacy_stdio_definitions`

`legacy_stdio_definitions` is retained to satisfy compatible legacy
stdio symbols where appropriate.

### Narrow compatibility shims

The remaining old imports were handled with a deliberately narrow
compatibility layer rather than mixing two CRT implementations.

Compatibility was added for the legacy symbols actually required by
GFx/libjpeg:

``` text
__iob_func
rewind
_ftime64
```

The `__iob_func` case was specifically required by the old bundled
libjpeg `jerror.obj`.

The compatibility layer accounts for the old CRT FILE-array arithmetic
used by that object and supports both Win64 and Win32 layouts.

After the final CRT shims were added, the project compiled successfully
and the game started with a functioning Scaleform menu.

------------------------------------------------------------------------

## 23. Scaleform runtime milestone

After the SDK integration and CRT compatibility work:

-   the embedded CrySystem linked successfully;
-   Crysis started;
-   the real menu rendered;
-   Scaleform interaction worked.

This confirmed that the Wars Scaleform renderer bridge can operate
against the still-stock Crysis renderer through the current CryMP
`IRenderer` ABI.

This is an important intermediate validation before embedding the
renderer itself.

------------------------------------------------------------------------

## 24. Current module ownership

At the documented milestone, the effective architecture is:

  -----------------------------------------------------------------------
  Subsystem                           Current implementation
  ----------------------------------- -----------------------------------
  CrySystem                           Embedded Wars-derived/CryMP-adapted
                                      source in `CryMP-Client.exe`

  CryPak                              CryMP implementation

  StreamEngine                        CryMP implementation

  ScriptSystem                        CryMP implementation

  Physics                             CryMP implementation by default

  SoundSystem                         CryMP implementation by default

  HardwareMouse                       CryMP implementation

  LocalizationManager                 CryMP implementation

  Scaleform/GFx bridge                Wars CrySystem source + original
                                      Scaleform SDK

  CryAction                           Stock Crysis DLL + existing CryMP
                                      wrappers/reconstruction

  CryNetwork                          Stock Crysis DLL

  Cry3DEngine                         Stock Crysis DLL

  Renderer                            Stock Crysis renderer DLL

  CryAnimation                        Stock DLL

  CryEntitySystem                     Stock DLL

  CryFont                             Stock DLL

  CryInput                            Stock DLL

  CryMovie                            Stock DLL
  -----------------------------------------------------------------------

This is intentional.

The next large engine-module milestone is Cry3DEngine, followed later by
the renderer.

------------------------------------------------------------------------

## 25. Logging milestone

After restoring direct `OnEarlyEngineInit()` invocation, the embedded
build again creates `CryMP-Client.log`.

This is important for further ABI/runtime debugging because failures in
stock CryAction/CryNetwork can now be correlated with CrySystem
initialization and subsystem state.

------------------------------------------------------------------------

## 26. Known issues at this milestone

### Localization

CryMP's LocalizationManager is selected, but translated strings are not
yet functioning correctly.

This still needs investigation of the exact old CryMP
language-selection/loading path and the resources being supplied through
CryPak.

### Internal client pak ZIP compatibility

After `RESOURCE_INTERNAL_PAK` loading was restored, the client log
showed that the pak is mounted, but some entries cannot be decoded
correctly by the current CryMP `ZipPak`.

Examples observed include:

``` text
[Script] Parse error (CryMP/Scripts/JSON.lua):
[string "CryMP/Scripts/JSON.lua"]:1: unexpected symbol

ZipPak::DecompressEntry(...): unsupported method

CryPak::OpenFileInPakImpl(
    "Game/CryMP/Scripts/HandGripData.lua"
): Error in pak!
```

This causes follow-on Lua errors because `JSON.lua` fails to load and
the global `json` object remains nil.

The current ZipPak implementation therefore still needs compatibility
with all compression methods used by the embedded CryMP client pak.

This issue is separate from the successful Internet connection fix:
mounting the internal pak restored required client initialization, even
though some individual entries still fail to decompress.

------------------------------------------------------------------------

## 27. Things intentionally not done yet

The following are deliberately deferred:

-   embedding CryAction;
-   embedding CryNetwork;
-   embedding Cry3DEngine;
-   embedding the renderer;
-   globally defining `_LIB`;
-   changing the Crysis 1 `SSystemGlobalEnvironment` layout;
-   changing the ABI-sensitive current `ISystem` vtable;
-   re-enabling obsolete Wars PunkBuster code without a demonstrated
    need;
-   blindly applying old CrySystem binary-patch offsets to the EXE;
-   replacing CryMP implementations with Wars versions where CryMP
    already has a maintained implementation.

------------------------------------------------------------------------

## 28. Important compile definitions

The embedded client currently requires:

``` cmake
target_compile_definitions(${CRYMP_CLIENT_EXE} PRIVATE
    CRYMP_EMBEDDED_CRYSYSTEM
    _DATAPROBE
    EXCLUDE_GPU_PARTICLE_PHYSICS
    NOMINMAX
)
```

`_DATAPROBE` is not optional while the stock CryAction DLL is used.

It fixes a confirmed stock CryAction dependency on:

``` cpp
ISystem::GetIDataProbe()
```

during network synchronization.

------------------------------------------------------------------------

## 29. Important linker dependencies

In addition to the project's normal dependencies, the embedded CrySystem
work added/uses Windows libraries such as:

``` text
wininet
version
psapi
```

Scaleform additionally links the correct
architecture/configuration-specific GFx and bundled libjpeg static
libraries.

`legacy_stdio_definitions` is used for old-MSVC CRT compatibility.

Do not explicitly add `ucrt` to work around Scaleform symbols; that was
tested and caused static/import UCRT duplicate definitions.

------------------------------------------------------------------------

## 30. Development pass history

The port was developed iteratively.

### Early passes

The first passes focused on getting the Wars CrySystem source accepted
by the current CryMP/CryCommon headers and C++20 compiler.

This included legacy include fixes, C++ standard-library modernization,
threading/library compatibility and interface adaptation.

### Middle passes

Later passes handled:

-   StreamEngine ownership;
-   CryPak integration;
-   ABI-sensitive environment differences;
-   localization API compatibility;
-   XML/serialization compatibility;
-   profiler/x64 fixes;
-   zlib regression;
-   linker externals;
-   system-inspection compatibility.

### Pass 15

Pass 15 reached the first major compile/link/runtime milestone.

The embedded CrySystem launched the game and displayed the menu
background.

Temporary Flash stubs were still in use.

A runtime crash during network synchronization was then traced to the
missing DataProbe.

### DataProbe fix

Adding:

``` text
_DATAPROBE
```

fixed the stock CryAction network-sync crash.

This was confirmed by restoring both FrameStart and FrameEnd
`SyncWithGame()` calls and running without the previous crash.

### Passes 16--18

These enabled the real Wars Scaleform source and integrated the
Scaleform SDK.

Win32/Win64 libraries were added.

C++20 issues in the integration and SDK headers were fixed.

### Passes 19--21

These addressed the old Scaleform/libjpeg CRT ABI.

The explicit-UCRT approach was rejected after duplicate-definition
errors.

The final solution used `legacy_stdio_definitions` plus narrow
compatibility shims.

After pass 21, the client compiled and the real menu worked.

### Pass 22

CryMP LocalizationManager was made authoritative and the missing direct
early-engine initialization call was restored.

This brought `CryMP-Client.log` back.

### Pass 23

The embedded CSystem was changed to directly initialize the CryMP
implementations of:

``` text
ScriptSystem
Physics
SoundSystem
```

rather than silently falling back to the stock Crytek DLLs because the
old `Replace*()` CrySystem patches were no longer running.

CryPak, StreamEngine, HardwareMouse and LocalizationManager were already
being handled as CryMP implementations.

### Post-pass-23 CryPak correction

The old `CreateNewCryPak()` behavior was audited.

It was discovered that the embedded path had not reproduced:

``` cpp
LoadClientPak(internalPak.data(), internalPak.size());
```

for `RESOURCE_INTERNAL_PAK`.

That initialization was restored directly in
`CSystem::InitFileSystem()`.

After this correction, Internet-server connection worked.

------------------------------------------------------------------------

## 31. Lessons from the port so far

### Binary replacement hooks often contained initialization behavior

A `Replace*()` function was not necessarily equivalent to merely
selecting a different class.

Some replacement factories also performed required setup.

The CryPak factory is the clearest example: using the correct CryPak
singleton without loading `RESOURCE_INTERNAL_PAK` was still incomplete.

Every old replacement/hook therefore needs to be audited for side
effects before it is considered fully migrated.

### Stock DLL compatibility exposes hidden ISystem dependencies

The DataProbe crash demonstrated that stock CryAction depends on CSystem
functionality that may not appear important during initial startup.

Maintaining the exact Crysis interface ABI and expected subsystem
initialization is essential until those DLLs are also source-built.

### Do not fix source-owned behavior with binary patches

Now that CrySystem is source-built, old CrySystem machine-code patches
should be translated into source behavior.

The old offsets should never be retargeted to `CryMP-Client.exe`.

### LAN success is a useful diagnostic boundary

LAN connection working proved that basic
CryNetwork/CryAction/game-context communication was operational.

The Internet connection failure was eventually tied to missing CryMP
client-pak initialization rather than a fundamental network ABI problem.

### Scaleform can remain static while the renderer remains external

The original Scaleform SDK and Wars renderer bridge successfully operate
from the embedded CrySystem while the stock renderer DLL remains loaded.

This validates the staged migration strategy.

------------------------------------------------------------------------

## 32. Next work

The immediate cleanup/integration tasks after this milestone are:

1.  restore full CryMP localization behavior;
2.  fix the current ZipPak handling of compression methods used by
    `RESOURCE_INTERNAL_PAK`;
3.  audit every old CrySystem launcher patch and migrate still-needed
    behavior into source;
4.  continue runtime testing of the CryMP ScriptSystem, Physics and
    SoundSystem under embedded CSystem;
5.  once CrySystem is stable, begin the Cry3DEngine embedding stage;
6.  embed the renderer after Cry3DEngine.

The guiding rule remains: preserve the current Crysis/CryMP ABI while
stock DLLs remain, and migrate functionality into source deliberately
rather than globally switching to Wars assumptions.
