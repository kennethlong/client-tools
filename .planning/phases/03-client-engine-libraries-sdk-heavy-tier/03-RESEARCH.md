# Phase 3: Client Engine Libraries (SDK-Heavy Tier) — Research

**Researched:** 2026-05-04
**Domain:** CMake build-system authoring for 13 C++ client engine static libraries; Mozilla XPCOM stub implementation; SDK linkage (DX9, DPVS, Miles, Vivox, VideoCapture, lcdui)
**Confidence:** HIGH — all findings verified directly against the source tree, existing CMakeLists, and Phase 1/2 artefacts

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01:** Use the prebuilt `.lib` from `src/external/3rd/library/dpvs/lib/win32-x86/` — no source-build of DPVS in Phase 3.
- **D-02:** Debug builds use the same Release-config DPVS prebuilt (`dpvs.lib`); the separate `dpvsd.lib` is not required for Phase 3 static-lib builds. If LNK2005 appears at Phase 4 executable link, escalate to source-build then.
- **D-03:** If LNK2005 appears at Phase 4 executable link (not Phase 3), escalate to DPVS source-build as named fallback.
- **D-04:** New file `src/cmake/stubs/libMozilla_stub.cpp` implements all `libMozilla` namespace entry points called by any Phase 3 library as no-ops or `return true`/`return nullptr`.
- **D-05:** The `libMozilla` CMakeLists.txt builds ONLY the stub file, not `src/external/3rd/library/libMozilla/src/win32/libMozilla.cpp`.
- **D-06:** The planner must scan `clientUserInterface` (and `clientGame`) `#include "libMozilla/*"` call sites to enumerate stub entry points before writing the stub template.
- **D-07:** `clientUserInterface`'s CMakeLists confirms via `dumpbin /symbols` that no `xpcom`, `xul`, `nspr4`, `plc4`, or `profdirserviceprovider_s` symbols appear.
- **D-08:** `clientGame` CMakeLists is authored fully in Phase 3 (final, largest target).
- **D-09:** Phase 4 is strictly the executable link step — no library CMakeLists authoring in Phase 4.
- **D-10:** LNK2005/LNK2019 storms from STLPort are an executable-link concern (Phase 4), not a static-lib concern (Phase 3). Do NOT add STLPort runtime link steps to individual Phase 3 static lib targets.
- **D-11:** Tier ordering within Phase 3: `clientDirectInput` before `clientGraphics` — proves DX9 header resolution in isolation before full `clientGraphics` + `d3d9.h` + DPVS complexity.
- **D-12:** C4530 warnings pre-suppressed globally for all Phase 3 client targets via `target_compile_options`.
- **D-13:** If DX9 header conflicts appear, add `WIN32_LEAN_AND_MEAN` and `NOMINMAX` as `target_compile_definitions` on the offending target only — not globally.
- **D-14:** Each library wires `First<LibName>.h` via `target_precompile_headers` — same PCH pattern as Phase 2.
- **D-15:** `clientBugReporting` built as-is, no stub — crash reporter fails to connect at runtime only, no C++ source edits required.

### Claude's Discretion
*(None — all decisions were made by Claude during discussion.)*

### Deferred Ideas (OUT OF SCOPE)
*(None — discussion stayed within phase scope.)*
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| LAUNCH-02 | Mozilla XPCOM `libMozilla::init` stubbed — boot does not depend on linking real XPCOM libraries | XPCOM stub entry point scan (§XPCOM Stub section); stub placement at `src/cmake/stubs/libMozilla_stub.cpp`; dumpbin verification gate (D-07) |
| BUILD-03 (ext) | 13 `engine/client/library/*` CMakeLists authored from scratch | Source file inventory (§Source File Inventory); SDK dep map (§SDK Dependency Map); Phase 2→3 dep graph (§Dependency Graph) |
</phase_requirements>

---

## Summary

Phase 3 authors 13 CMakeLists from scratch — there is no swg-main precedent for any of these client libraries. The core challenge is threefold: (1) every SDK used in this codebase (DX9, DPVS, Miles, Vivox, VideoCapture, lcdui) has its first real compile in Phase 3, so include-path resolution and static-lib linkage must be proven against the vendored SDK layout; (2) `clientGame` is by far the largest target (343 `.cpp` files across 22 subdirectories) and requires a structured plan split; (3) the Mozilla XPCOM stub must exactly match the entry points called by `clientGame` and `clientUserInterface` without pulling in any of the real XPCOM headers or libraries.

The key finding from source scanning is that `libMozilla::init` and the rest of the `libMozilla` namespace are called from exactly two Phase 3 files: `clientGame/src/shared/core/Game.cpp` (uses `libMozilla::init`, `libMozilla::update`, `libMozilla::release`) and indirectly through `clientUserInterface` (which includes only engine-side headers that reference `libMozilla`). The stub is therefore small — 8–10 functions — and the complete API is captured in `src/external/3rd/library/libMozilla/src/win32/libMozilla.h`. DPVS is the second high-risk item: `dpvs*.hpp` headers are included directly (not through an abstraction) in `clientGraphics`, `clientObject`, `clientParticle`, `clientSkeletalAnimation`, `clientTerrain`, and `clientGame`, so the DPVS interface header path must be set on all six of those targets.

**Primary recommendation:** Split Phase 3 into 5 plans — Wave 0 (infrastructure + `engine/client` wiring), Tier 6 batch (6 simpler libs), Tier 7 HIGH RISK (`clientGraphics` + `clientAudio`), Tier 8 (`clientSkeletalAnimation` + `clientTerrain`), and the large `clientGame` + Tier 9 `clientUserInterface` finisher. The `clientGame` source enumeration alone justifies its own plan due to 22 subdirectories.

---

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Animation playback scripts | Client (clientAnimation) | — | Timed playback actions, no server-side analog |
| Audio (Miles Sound System 3) | Client (clientAudio) | — | Platform-specific; Miles is a win32-only runtime |
| Crash reporting | Client (clientBugReporting) | — | Windows-only; connects to defunct SOE endpoint at runtime |
| DirectInput / force feedback | Client (clientDirectInput) | — | Input HW abstraction, Windows-only |
| DX9 render abstraction | Client (clientGraphics) | CDN/Static (assets) | Graphics pipeline lives entirely client-side |
| Object appearance / mesh | Client (clientObject) | — | Appearance templates and mesh rendering are client-side |
| Particle effects | Client (clientParticle) | — | VFX is client-only |
| Skeletal animation | Client (clientSkeletalAnimation) | — | Bone deformation entirely client |
| Terrain rendering | Client (clientTerrain) | Shared (sharedTerrain) | sharedTerrain owns data; clientTerrain owns rendering |
| Texture rendering | Client (clientTextureRenderer) | — | Blueprint texture baking client-only |
| Mozilla XPCOM (in-game browser) | Client (clientUserInterface) | — | Browser embed is UI-tier; stubbed for v1 |
| Vivox voice chat UI | Client (clientUserInterface) | — | Voice session management lives in CUI layer |
| Game-layer integration | Client (clientGame) | Shared (serverGame sibling) | clientGame coordinates all client subsystems |

---

## Standard Stack

### Find Modules (verified present in `src/cmake/win32/`) [VERIFIED: directory scan]

| Module | Variables Exported | Verified Present |
|--------|-------------------|-----------------|
| `FindDirectX9.cmake` | `DIRECTX9_INCLUDE_DIR`, `DIRECTX9_LIBRARIES` (d3d9+d3dx9+dinput8+dsound+ddraw+dxguid+DxErr9) | YES |
| `FindDPVS.cmake` | `DPVS_INCLUDE_DIRS` (interface + implementation/include), `DPVS_LIBRARY` (debug/optimized) | YES |
| `FindMiles.cmake` | `MILES_INCLUDE_DIR`, `MILES_LIBRARY` | YES |
| `FindMozilla.cmake` | `MOZILLA_PUBLIC_INCLUDE_DIR`, `MOZILLA_PRIVATE_INCLUDE_DIR`, `MOZILLA_INCLUDE_DIRS` | YES |
| `FindVivox.cmake` | `VIVOX_INCLUDE_DIRS`, `VIVOX_LIBRARIES` | YES |
| `FindBink.cmake` | `BINK_INCLUDE_DIR`, `BINK_LIBRARY` | YES |
| `FindVideoCapture.cmake` | `VIDEOCAPTURE_INCLUDE_DIR`, `VIDEOCAPTURE_LIBRARY`, `AUDIOCAPTURE_LIBRARY` | YES |
| `FindLogitechLCD.cmake` | `LOGITECHLCD_INCLUDE_DIR`, `LOGITECHLCD_LIBRARY` | YES |

**Important:** All `find_package` calls are already in `src/CMakeLists.txt`. Phase 3 libraries consume the variables via `target_include_directories` and `target_link_libraries` — they do NOT add new `find_package` calls.

### SDK Prebuilt Libraries Verification [VERIFIED: directory scan]

| SDK | Prebuilt Path | Files Present |
|-----|---------------|---------------|
| DX9 | `external/3rd/library/directx9/lib/` | d3d9.lib, d3dx9.lib, dinput8.lib, dsound.lib, ddraw.lib, dxguid.lib, DxErr9.lib |
| DPVS | `external/3rd/library/dpvs/lib/win32-x86/` | dpvs.lib, dpvsd.lib |
| Miles | `external/3rd/library/miles/lib/win/` | Mss32.lib |
| Vivox | `external/3rd/library/vivox/lib/` | vivoxsdk.lib, vivoxplatform.lib, libsndfile-1.lib |
| Vivox Wrapper | `external/3rd/library/vivoxSharedWrapper/lib/` | vivoxSharedWrapper_Debug.lib, vivoxSharedWrapper_Release.lib |
| VideoCapture | `external/3rd/library/videocapture/VideoCapture/lib/win32/` | VideoCapture_debug.lib, VideoCapture_release.lib |
| AudioCapture | `external/3rd/library/videocapture/AudioCapture/lib/win32/` | AudioCapture.lib |
| CaptureCommon | `external/3rd/library/videocapture/CaptureCommon/lib/win32/` | CaptureCommon_debug.lib, CaptureCommon_release.lib |
| lcdui | `external/3rd/library/lcdui/lib/` | lgLcd.lib |

---

## Source File Inventory

### Tier 6: Basic Client Libraries [VERIFIED: filesystem scan]

| Library | .cpp Count | Source Dirs | PCH Header | PCH Source (win32) | Notes |
|---------|-----------|-------------|------------|-------------------|-------|
| `clientAnimation` | 16 | `shared/core/`, `shared/playback/`, `win32/` | `shared/core/FirstClientAnimation.h` | `win32/FirstClientAnimation.cpp` | No external SDK includes |
| `clientObject` | 43 | `shared/appearance/`, `shared/camera/`, `shared/core/`, `shared/environment/`, `shared/graphics/`, `shared/object/`, `win32/` | `shared/core/FirstClientObject.h` | `win32/FirstClientObject.cpp` | Includes `dpvsModel.hpp`, `dpvsObject.hpp` — needs DPVS interface path |
| `clientTextureRenderer` | 12 | `shared/`, `win32/` | `shared/FirstClientTextureRenderer.h` | `win32/FirstClientTextureRenderer.cpp` | No external SDK includes |
| `clientDirectInput` | 6 | `shared/`, `win32/` | `shared/FirstClientDirectInput.h` | `win32/FirstClientDirectInput.cpp` | Includes `<dinput.h>` — needs DIRECTX9_INCLUDE_DIR |
| `clientBugReporting` | 5 | `win32/` only | `shared/FirstClientBugReporting.h` | `win32/FirstClientBugReporting.cpp` | All 5 .cpp in `win32/` (win32-only lib); no external SDK headers |
| `clientParticle` | 30 | `shared/`, `win32/` | `shared/FirstClientParticle.h` | `win32/FirstClientParticle.cpp` | Includes `dpvsModel.hpp`, `dpvsObject.hpp` — needs DPVS interface path |

**clientBugReporting note:** All source files are in `src/win32/` (not `src/shared/`). The pattern for `PLATFORM_SOURCES` covers all 5 .cpp files; `SHARED_SOURCES` contains only `.h` files.

### Tier 7: HIGH RISK [VERIFIED: filesystem scan]

| Library | .cpp Count | Source Dirs | PCH Header | PCH Source | SDK Headers |
|---------|-----------|-------------|------------|-----------|-------------|
| `clientGraphics` | 68 | `Bink/`, `shared/`, `win32/` | `shared/FirstClientGraphics.h` | `win32/FirstClientGraphics.cpp` | `<d3d9.h>` (via BinkDLL.h), `dpvs*.hpp` (multiple files), Bink headers (LoadLibrary pattern — include path needed for header, no static link of binkw32.lib) |
| `clientAudio` | 20 | `win32/` only | `shared/FirstClientAudio.h` | `win32/FirstClientAudio.cpp` | `<mss.h>` included via PCH (`FirstClientAudio.h` does `#include <mss.h>` directly); VideoCapture (`AudioCapture/AudioCapture.h`) in `SwgAudioCapture.cpp` |

**clientGraphics critical note:** `FirstClientGraphics.h` is minimal — it only includes `sharedFoundation/FirstSharedFoundation.h`. The DX9 and DPVS headers are included inline by individual `.cpp` files. This means the include paths must be set at the target level (not via PCH).

**clientAudio critical note:** `FirstClientAudio.h` includes `<mss.h>` directly with `#pragma warning(push, 3)` / `#pragma warning(pop)` bracketing. This guarantees every TU in clientAudio sees Miles headers. The `MILES_INCLUDE_DIR` must be in the target's include path. All 20 `.cpp` files live in `src/win32/` (not `src/shared/`) — clientAudio is a win32-only lib like clientBugReporting.

### Tier 8 [VERIFIED: filesystem scan]

| Library | .cpp Count | Source Dirs | PCH Header | PCH Source | SDK Headers |
|---------|-----------|-------------|------------|-----------|-------------|
| `clientSkeletalAnimation` | 102 | `shared/animation/`, `shared/appearance/`, `shared/batch/`, `shared/controller/`, `shared/core/`, `shared/debug/`, `shared/modifier/`, `shared/playback/`, `win32/` | `shared/core/FirstClientSkeletalAnimation.h` | `win32/FirstClientSkeletalAnimation.cpp` | `dpvsModel.hpp`, `dpvsObject.hpp` in appearance files |
| `clientTerrain` | 37 | `shared/appearance/`, `shared/core/`, `shared/environment/`, `shared/flora/`, `shared/object/`, `shared/utility/`, `shared/water/`, `shared/weather/`, `win32/` | `shared/core/FirstClientTerrain.h` | `win32/FirstClientTerrain.cpp` | `dpvsObject.hpp` in appearance files |

### Tier 9 [VERIFIED: filesystem scan]

| Library | .cpp Count | Source Dirs | PCH Header | PCH Source | SDK Headers |
|---------|-----------|-------------|------------|-----------|-------------|
| `clientUserInterface` | 152 | `shared/core/` (118), `shared/page/` (24), `shared/widget/` (9), `win32/` (1) | `shared/core/FirstClientUserInterface.h` | `win32/FirstClientUserInterface.cpp` | `<dinput.h>` in CuiIoWin.cpp and IMEManager.cpp; `clientGraphics/SwgVideoCapture.h` in CuiIoWin.cpp; Vivox via `CuiVoiceChatGlue.h` (`#include "vivoxSharedWrapper/Vivox.h"`); UI lib via `FirstClientUserInterface.h` → `_precompile.h` → UIStlFwd.h etc. |

**clientUserInterface PCH note:** `FirstClientUserInterface.h` includes `_precompile.h` (from `external/3rd/library/ui/include/`). The `ui` library is a separate 126-file static lib that must be added as a Phase 3 build target — see §Additional Phase 3 Target section below.

### Integrator [VERIFIED: filesystem scan]

| Library | .cpp Count | Source Dirs | PCH Header | PCH Source | SDK Headers |
|---------|-----------|-------------|------------|-----------|-------------|
| `clientGame` | 343 | `shared/HTTPpost/`(6), `shared/animation/`(1), `shared/appearance/`(7), `shared/camera/`(7), `shared/clientEffect/`(5), `shared/collision/`(1), `shared/combat/`(6), `shared/command/`(6), `shared/container/`(1), `shared/controller/`(13), `shared/core/`(65), `shared/customization/`(3), `shared/deadReckoning/`(5), `shared/dynamics/`(3), `shared/event/`(1), `shared/graphics/`(1), `shared/group/`(1), `shared/modifier/`(1), `shared/mount/`(1), `shared/network/`(7), `shared/object/`(59), `shared/objectTemplate/`(41), `shared/playback/`(38), `shared/scene/`(15), `shared/space/`(33), `shared/synchronizedUi/`(3), `shared/test/`(11), `shared/trading/`(1), `win32/`(1) | `shared/core/FirstClientGame.h` | `win32/FirstClientGame.cpp` | `libMozilla/libMozilla.h` (Game.cpp); `dpvsObject.hpp`, `dpvsModel.hpp` (multiple appearance/space files); `<dinput.h>` none direct — pulled via clientDirectInput headers |

**clientGame PCH critical note:** `FirstClientGame.h` includes `_precompile.h` (same UI lib dependency as `clientUserInterface`) and `StringId.h` (from `external/ours/library/localization/include/StringId.h`). Both include paths must be set on `clientGame`.

---

## Additional Phase 3 Target: `ui` Static Library [VERIFIED: filesystem scan]

The `external/3rd/library/ui/` directory contains the **SOE in-house UI engine** — 126 `.cpp` files across `src/shared/` and `src/win32/`. Despite the `external/3rd/` path, it is SOE-authored code (not a third-party SDK) and must be built as a static library.

**Why Phase 3 (not Phase 4):** `clientUserInterface` and `clientGame` both include `_precompile.h` which pulls in `UIStlFwd.h`, `UiReport.h`, and `UnicodeUtils.h` from the UI library. These must compile against a built `ui` target.

**Key facts:**
- Headers at: `external/3rd/library/ui/include/` (public) and `external/3rd/library/ui/src/win32/` (implementation-level headers like `_precompile.h`)
- Source at: `external/3rd/library/ui/src/shared/` (most files) + `external/3rd/library/ui/src/win32/` (win32-specific)
- Include path to expose: `external/3rd/library/ui/include` (for `UIBaseObject.h` etc.) and `external/3rd/library/ui/src/win32` (for `_precompile.h`)
- Must be wired into `src/external/3rd/library/CMakeLists.txt` (currently only has `udplibrary`)

**Phase 3 Wave 0 task:** Add `ui` target before Tier 6 so dependent libs can configure.

---

## XPCOM Stub — Complete Entry Point List [VERIFIED: source scan of libMozilla.h]

Per D-04 through D-06, the stub at `src/cmake/stubs/libMozilla_stub.cpp` must implement the **public `libMozilla` namespace API** declared in `src/external/3rd/library/libMozilla/src/win32/libMozilla.h`.

### Direct callers in Phase 3 scope [VERIFIED: grep scan]

| File | Functions Called |
|------|-----------------|
| `clientGame/src/shared/core/Game.cpp:147` | `libMozilla::init(void*, const char*)`, `libMozilla::update()`, `libMozilla::release()` |

The `clientUserInterface` Vivox/voice files do NOT call `libMozilla` directly — only Game.cpp uses it in Phase 3 scope.

### Complete stub required (all public API declared in libMozilla.h) [VERIFIED: read of libMozilla.h]

```cpp
// src/cmake/stubs/libMozilla_stub.cpp
// STUB: implements libMozilla namespace for Phase 3 build.
// libMozilla.h is at external/3rd/library/libMozilla/include/public/libMozilla/
// which redirects to src/win32/libMozilla.h.
// The real implementation (src/win32/libMozilla.cpp) requires XPCOM headers
// that cannot compile against modern MSVC. This stub provides no-op versions.

#include "libMozilla/libMozilla.h"

namespace libMozilla
{
    bool    init( void * /*pNativeWindow*/, const char * /*szApplicationDirectory*/ ) { return true; }
    void    update() {}
    void    release() {}

    void    enableMemoryCache( bool /*bEnable*/ ) {}
    void    enableDiskCache( bool /*bEnable*/, unsigned /*uMaxSizeKB*/ ) {}
    void    setUserAgent( const char * /*pUserAgent*/ ) {}

    Window *createWindow( unsigned /*width*/, unsigned /*height*/ ) { return 0; }
    void    destroyWindow( Window * /*pWindow*/ ) {}

    // Window methods — cannot be implemented without Window definition
    // (Window has private ctor/dtor so only Manager can instantiate it).
    // The stub above provides null createWindow, so Window methods are never called.
}
```

**Key finding:** The `libMozilla::Window` and `libMozilla::IBlitter` / `libMozilla::ICallback` classes have vtables defined via their header, but since `createWindow` returns nullptr the Window method bodies are never reached. The stub does not need to provide Window member implementations — only the namespace-level free functions.

**libMozilla INTERFACE target:** The `libMozilla` CMakeLists.txt adds `add_library(libMozilla STATIC src/cmake/stubs/libMozilla_stub.cpp)` and sets `target_include_directories(libMozilla PUBLIC ${MOZILLA_PUBLIC_INCLUDE_DIR})`. The private XPCOM headers (`MOZILLA_PRIVATE_INCLUDE_DIR`) are NOT added — the stub only needs the public header to compile the `namespace libMozilla { ... }` declarations.

---

## SDK Dependency Map Per Library (Phase 3 `target_include_directories` and `target_link_libraries`)

[VERIFIED: source grep scan + FindModule inspection]

| Library | External SDK Include Paths Needed | External SDK Libs Needed | Notes |
|---------|------------------------------------|--------------------------|-------|
| `clientAnimation` | None (pure engine deps) | None | |
| `clientObject` | `DPVS_INCLUDE_DIRS` | `DPVS_LIBRARY` | interface + implementation/include both needed |
| `clientTextureRenderer` | None | None | |
| `clientDirectInput` | `DIRECTX9_INCLUDE_DIR` | `DIRECTX9_DINPUT8_LIBRARY`, `DIRECTX9_DXGUID_LIBRARY` | dinput8.lib + dxguid.lib for GUID definitions |
| `clientBugReporting` | None | None | Windows system libs only (via WinAPI) |
| `clientParticle` | `DPVS_INCLUDE_DIRS` | `DPVS_LIBRARY` | |
| `clientGraphics` | `DIRECTX9_INCLUDE_DIR`, `DPVS_INCLUDE_DIRS`, `BINK_INCLUDE_DIR` | `DPVS_LIBRARY` | Bink is LoadLibrary — no static link of binkw32.lib; d3d9.lib NOT needed at static-lib build time (only at executable link) |
| `clientAudio` | `MILES_INCLUDE_DIR`, VideoCapture root | `MILES_LIBRARY`, `VIDEOCAPTURE_LIBRARY` | Miles included via PCH; VideoCapture root = `${SWG_EXTERNALS_FIND}/videocapture` |
| `clientSkeletalAnimation` | `DPVS_INCLUDE_DIRS` | `DPVS_LIBRARY` | |
| `clientTerrain` | `DPVS_INCLUDE_DIRS` | `DPVS_LIBRARY` | |
| `clientUserInterface` | `MOZILLA_PUBLIC_INCLUDE_DIR`, `DIRECTX9_INCLUDE_DIR`, `VIVOX_INCLUDE_DIRS`, `LOGITECHLCD_INCLUDE_DIR`, UI lib include | `VIVOX_LIBRARIES`, `LOGITECHLCD_LIBRARY`, `libMozilla` (stub target) | Vivox included via `CuiVoiceChatGlue.h`; dinput.h in CuiIoWin + IMEManager |
| `clientGame` | `MOZILLA_PUBLIC_INCLUDE_DIR`, `DPVS_INCLUDE_DIRS`, UI lib include, `localization/include` | `libMozilla` (stub target), `DPVS_LIBRARY` | DPVS in appearance/space files; libMozilla in Game.cpp |
| `libMozilla` (stub) | `MOZILLA_PUBLIC_INCLUDE_DIR` only | None (stub has no link deps) | Must NOT include MOZILLA_PRIVATE_INCLUDE_DIR |
| `ui` | `ui/include`, STLPort | None | SOE UI engine; no external SDK deps |

**DPVS include path detail:** `DPVS_INCLUDE_DIRS` expands to two paths:
1. `${SWG_EXTERNALS_FIND}/dpvs/interface` — for `dpvs.hpp`, `dpvsCamera.hpp`, `dpvsCell.hpp`, etc.
2. `${SWG_EXTERNALS_FIND}/dpvs/implementation/include` — for `dpvsWrapper.hpp`

Both are needed because `RenderWorldCommander.h` includes `dpvsCommander.hpp` from the interface path, while other headers use implementation-level wrappers.

**VideoCapture include path detail:** `SwgAudioCapture.cpp` includes `"AudioCapture/AudioCapture.h"` and `SwgVideoCapture.cpp` includes `"VideoCapture/VideoCapture.h"` — both use the parent `videocapture/` directory as the include root (i.e., include path = `${SWG_EXTERNALS_FIND}/videocapture`).

---

## Phase 2 → Phase 3 Dependency Graph [VERIFIED: source include scan + Phase 2 research]

All Phase 2 `.lib` artefacts are available. Phase 3 libraries depend on these Phase 2 outputs via `target_link_libraries`:

| Phase 3 Library | Phase 2 Dependencies (target_link_libraries) |
|----------------|---------------------------------------------|
| `clientAnimation` | `sharedFoundation`, `sharedFile`, `sharedObject` |
| `clientObject` | `sharedFoundation`, `sharedFile`, `sharedObject`, `sharedMath`, `sharedImage` |
| `clientTextureRenderer` | `clientGraphics`, `sharedFoundation`, `sharedFile`, `sharedObject` |
| `clientDirectInput` | `sharedFoundation`, `sharedDebug` |
| `clientBugReporting` | `sharedFoundation`, `sharedDebug` |
| `clientParticle` | `clientGraphics`, `clientObject`, `sharedFoundation`, `sharedFile`, `sharedObject` |
| `clientGraphics` | `sharedFoundation`, `sharedFile`, `sharedDebug`, `sharedObject` |
| `clientAudio` | `sharedFoundation`, `sharedFile`, `sharedDebug`, `sharedObject`, `sharedMath`, `sharedUtility` |
| `clientSkeletalAnimation` | `clientGraphics`, `clientObject`, `clientAnimation`, `sharedFoundation`, `sharedFile`, `sharedObject`, `sharedMath` |
| `clientTerrain` | `clientGraphics`, `clientObject`, `sharedTerrain`, `sharedFoundation`, `sharedFile`, `sharedMath` |
| `clientUserInterface` | `clientGraphics`, `clientObject`, `clientAnimation`, `sharedFoundation`, `sharedFile`, `sharedGame`, `sharedObject`, `sharedUtility`, `sharedNetwork`, `sharedNetworkMessages`, `libMozilla`, `ui` |
| `clientGame` | `clientGraphics`, `clientAudio`, `clientObject`, `clientAnimation`, `clientSkeletalAnimation`, `clientTerrain`, `clientParticle`, `clientTextureRenderer`, `clientDirectInput`, `clientUserInterface`, `sharedFoundation`, `sharedFile`, `sharedGame`, `sharedObject`, `sharedTerrain`, `sharedNetwork`, `sharedNetworkMessages`, `sharedUtility`, `sharedMath`, `libMozilla`, `ui` |

**Note:** These dependency declarations are inferred from include-path scanning and the swg-main server-side patterns [ASSUMED for exact target_link_libraries sets — the planner should verify by checking for unresolved include errors during compilation and adjust the dep list]. The server-side swg-main CMakeLists have no client library equivalents to copy from, so exact client link orders should be validated iteratively.

---

## Architecture Patterns

### System Architecture Diagram

```
Phase 3 Build Graph (simplified)

Wave 0: Infrastructure
  src/cmake/stubs/libMozilla_stub.cpp → [libMozilla] (stub target)
  src/external/3rd/library/ui/ → [ui] (new static lib)
  src/engine/CMakeLists.txt ← add_subdirectory(client) [new]
  src/engine/client/CMakeLists.txt ← add_subdirectory(library) [new]
  src/engine/client/library/CMakeLists.txt ← add_subdirectory(all 13) [new]

Tier 6 → Tier 7 → Tier 8 → Tier 9 → clientGame

DX9 SDK ──────────────────────────────┐
                                       ├→ clientDirectInput (T6, proves DX9 headers)
                                       ├→ clientGraphics (T7, DX9 + DPVS)
DPVS SDK ──────────────────┬──────────┤→ clientObject (T6)
                            │          ├→ clientParticle (T6)
Miles SDK ─────────────────┼──────────┼→ clientAudio (T7)
                            │          ├→ clientSkeletalAnimation (T8)
Vivox SDK ─────────────────┤          └→ clientTerrain (T8)
libMozilla (stub) ─────────┤
UI lib ─────────────────────┤
                            └──────────→ clientUserInterface (T9)
                                        clientGame (integrator, all deps)
```

### Recommended Project Structure for Phase 3 New Files

```
src/
├── engine/
│   ├── CMakeLists.txt           (MODIFY: add add_subdirectory(client))
│   └── client/
│       ├── CMakeLists.txt       (NEW: add_subdirectory(library))
│       └── library/
│           ├── CMakeLists.txt   (NEW: add_subdirectory for all 13)
│           ├── clientAnimation/CMakeLists.txt  (NEW)
│           ├── clientAnimation/src/CMakeLists.txt  (NEW)
│           └── ... (same pattern for all 13 libs)
├── cmake/
│   └── stubs/
│       └── libMozilla_stub.cpp  (NEW: XPCOM stub implementation)
└── external/
    └── 3rd/
        └── library/
            ├── CMakeLists.txt   (MODIFY: add add_subdirectory(ui))
            └── ui/
                ├── CMakeLists.txt   (NEW)
                └── src/CMakeLists.txt   (NEW: 126 files)
```

### Pattern Inheritance from Phase 2

All Phase 2 patterns (A through I from `02-PATTERNS.md`) carry forward unchanged. Key reminders:

**PATTERN A (top-level CMakeLists):** Same template as Phase 2:
```cmake
cmake_minimum_required(VERSION 2.8)
project(clientAnimation)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/public)
add_subdirectory(src)
```

**PATTERN B (src CMakeLists with SHARED_SOURCES + PLATFORM_SOURCES):** Same structure. Client libs follow the same shared/win32 split. Note that `clientAudio`, `clientBugReporting` are **win32-only** — all `.cpp` in `src/win32/`, `SHARED_SOURCES` contains only headers.

**PATTERN D (PCH wiring):** `target_precompile_headers(<LibName> PRIVATE shared/First<LibName>.h)` — same as Phase 2. Exception: `clientBugReporting` has no `src/shared/` directory — use `shared/FirstClientBugReporting.h` (the header exists at `include/public/clientBugReporting/FirstClientBugReporting.h`; the PCH `.h` variant used by the compiler is the copy at `src/shared/FirstClientBugReporting.h`).

### NEW: Phase 3 Pattern — SDK include_directories on targets

```cmake
# Example: clientGraphics/src/CMakeLists.txt
target_include_directories(clientGraphics PRIVATE
    ${DIRECTX9_INCLUDE_DIR}
    ${DPVS_INCLUDE_DIRS}          # expands to interface + implementation/include
    ${BINK_INCLUDE_DIR}           # for BinkDLL.h
    ${SWG_ENGINE_SOURCE_DIR}/client/library/clientGraphics/src/shared
    # ... Phase 2 engine deps ...
)
target_link_libraries(clientGraphics
    ${DPVS_LIBRARY}               # dpvs.lib (Release config used for both Debug and Release per D-02)
    sharedFoundation
    sharedFile
    sharedDebug
    sharedObject
)
target_compile_options(clientGraphics PRIVATE /wd4530)   # D-12: C4530 pre-suppressed
```

### Anti-Patterns to Avoid

- **Linking binkw32.lib in clientGraphics:** Bink uses `LoadLibrary`/`GetProcAddress` at runtime. The BinkDLL.h includes `<d3d9.h>` and the Bink header in a namespace but only declares function pointers. No static link of `binkw32.lib` in Phase 3.
- **Adding Mozilla PRIVATE include dir to any target:** `MOZILLA_PRIVATE_INCLUDE_DIR` contains real XPCOM headers that cannot compile under modern MSVC. Only `MOZILLA_PUBLIC_INCLUDE_DIR` (which exposes `libMozilla/libMozilla.h`) should be in include paths.
- **Adding STLPort to individual lib target_link_libraries:** Per D-10, STLPort runtime linking is a Phase 4 executable-link concern.
- **Re-declaring global compile definitions:** `_USE_32BIT_TIME_T=1`, `_MBCS`, `PRODUCTION=0`, `DEBUG_LEVEL=2` are set globally in root CMakeLists. Do NOT re-add per-target.
- **Missing D-13 DX9/WinSDK conflict guard:** If `clientGraphics` produces C2146/C4005 errors from conflicting `winsock.h` / `winsock2.h` includes when DX9 headers interact with Windows SDK, add `WIN32_LEAN_AND_MEAN` and `NOMINMAX` to that target only.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| XPCOM browser embed | Any real XPCOM init | `libMozilla_stub.cpp` no-op stub | XPCOM symbols require MSVC 7.1-era headers; modern MSVC rejects the XPCOM macros (NS_IMPL_*, nsresult calling conventions) |
| DPVS from source | `dpvs/implementation/` source build | Prebuilt `dpvs.lib` from `dpvs/lib/win32-x86/` (D-01) | DPVS source uses C++98 patterns that fail under MSVC 14.4x with `/std:c++17`; prebuilt was already compiled with /MT |
| Miles sound wrapper | Any Miles audio abstraction | `Mss.h` directly (via PCH in clientAudio) | Miles API is stable; the wrapper pattern is already established in Audio.cpp |
| UI engine rebuild | SOE-custom UI reimplementation | Build `external/3rd/library/ui/` as a static lib | 126 source files, battle-tested; rebuilding would require source analysis of all UI widgets |

---

## STLPort First-Use Risk Assessment [VERIFIED: source and PCH inspection]

**First STLPort-heavy TU in Phase 3:** `clientAudio/src/shared/FirstClientAudio.h` includes `<mss.h>` (Miles header), which in turn pulls in STL types. However, the PCH bracket `#pragma warning(push, 3)` / `#pragma warning(pop)` around the Miles include prevents cascading warning storms.

The more significant STLPort-heavy files are in `clientUserInterface` (uses `<sstream>`, `<string>`) and `clientGame` (uses `<limits>`, `<list>`, `<map>`, `<sstream>`).

**Risk (D-10):** LNK2005 "already defined in libcmt.lib" errors from STLPort's conflicting `std::` implementations are an **executable-link** problem, not a static-lib problem. Static libs do not resolve symbols — they just accumulate `.obj` files. The LNK2005 risk surfaces only at Phase 4 when all `.lib` files are combined into `SwgClient.exe`. Phase 3 builds should succeed without STLPort runtime link configuration.

**Mitigation already in place:** Root CMakeLists sets `_STLP_DONT_FORCE_MSVC_LIB_NAME` and injects the STLPort include path before MSVC headers. This is sufficient for compilation; no per-target STLPort changes needed.

---

## DX9 Header Conflict Analysis [VERIFIED: existing Phase 1 research + vendored DX9 header inventory]

**Vendored DX9 headers present:** `d3d9.h`, `d3d9caps.h`, `d3d9types.h`, `d3d.h`, `d3dcaps.h`, `d3dtypes.h`, `d3dx9.h` and all `d3dx9*.h` variants, `ddraw.h`, `dinput.h`, `DxDiag.h`.

**Risk scenario:** Modern Windows 11 SDK ships its own `d3d9.h`, `dinput.h`, `ddraw.h`. If both the vendored path and the Windows SDK path are active, duplicate definitions cause C4005 (macro redefinition) and C2146 (syntax errors). This is P1.11 from Phase 1 research.

**Mitigation (D-11 + D-13):**
1. `FindDirectX9.cmake` resolves headers from the vendored path. The `DIRECTX9_INCLUDE_DIR` is added BEFORE the Windows SDK path on targets that use it.
2. If conflicts still appear: add `WIN32_LEAN_AND_MEAN` and `NOMINMAX` to the specific target's `target_compile_definitions` — these macros exclude the conflicting Windows headers from inclusion.
3. `clientDirectInput` is built FIRST (per D-11) — it exercises `<dinput.h>` from the vendored DX9 path with the narrowest surface area (no `d3d9.h`, no DPVS). This validates the include resolution before tackling `clientGraphics`.

**`/Zc:wchar_t-` interaction:** The vendored DX9 headers use `wchar_t` as `unsigned short` (matching the legacy project setting). This is already set globally in root CMakeLists. No per-target override needed.

---

## clientGame Size and Plan Split Recommendation

`clientGame` is 343 `.cpp` files across 22 subdirectories. This is comparable to `sharedNetworkMessages` (338 files across 9 subdirs) which required its own Phase 2 plan (02-05). Based on that precedent:

**Recommendation: clientGame warrants its own dedicated plan** (Plan 05 of Phase 3).

Suggested Phase 3 plan structure:

| Plan | Contents | Risk |
|------|----------|------|
| 03-01 | Wave 0: `engine/client` wiring, `libMozilla` stub, `ui` lib, `external/3rd/library/CMakeLists.txt` update | LOW |
| 03-02 | Tier 6: `clientAnimation`, `clientTextureRenderer`, `clientBugReporting`, `clientDirectInput`, `clientObject`, `clientParticle` | MEDIUM (clientObject + clientParticle need DPVS) |
| 03-03 | Tier 7 HIGH RISK: `clientGraphics` (DX9 + DPVS), `clientAudio` (Miles + VideoCapture) | HIGH |
| 03-04 | Tier 8 + Tier 9: `clientSkeletalAnimation`, `clientTerrain`, `clientUserInterface` | MEDIUM-HIGH (clientUserInterface has 152 files + Vivox + libMozilla dep) |
| 03-05 | Integrator: `clientGame` (343 files, 22 subdirs) + Phase 3 SC verification | HIGH (volume + DPVS + libMozilla deps) |

---

## Infrastructure New Files Required (Wave 0) [VERIFIED: directory scan]

The following files and directory CMakeLists do not exist and must be created in Wave 0 before any library CMakeLists:

| File | Action | Why Needed |
|------|--------|------------|
| `src/cmake/stubs/libMozilla_stub.cpp` | CREATE | XPCOM stub; `libMozilla` target builds this |
| `src/engine/CMakeLists.txt` | MODIFY | Add `add_subdirectory(client)` after `add_subdirectory(shared)` |
| `src/engine/client/CMakeLists.txt` | CREATE | `add_subdirectory(library)` |
| `src/engine/client/library/CMakeLists.txt` | CREATE | `add_subdirectory(clientAnimation)` through `add_subdirectory(clientGame)` in tier order |
| `src/external/3rd/library/CMakeLists.txt` | MODIFY | Add `add_subdirectory(ui)` after `add_subdirectory(udplibrary)` |
| `src/external/3rd/library/ui/CMakeLists.txt` | CREATE | `project(ui) + include_directories + add_subdirectory(src)` |
| `src/external/3rd/library/ui/src/CMakeLists.txt` | CREATE | 126-file SHARED_SOURCES + PLATFORM_SOURCES (shared/ + win32/); `add_library(ui STATIC ...)` |

**`libMozilla` target location:** The planner must decide whether `libMozilla` is declared in `src/engine/client/library/CMakeLists.txt` as an inline stub target (similar to Phase 2's INTERFACE stubs for `sharedCollision`) or as its own subdirectory. Given D-04 places the stub at `src/cmake/stubs/`, the simplest approach is an inline `add_library` call in the library-level CMakeLists using the absolute path to the stub file.

---

## Common Pitfalls

### Pitfall 1: DPVS include path incomplete
**What goes wrong:** Compilation fails with `dpvsCommander.hpp: No such file or directory` even after adding `DPVS_INCLUDE_DIRS`.
**Why it happens:** `DPVS_INCLUDE_DIRS` expands to both `interface/` and `implementation/include/`. If only one path is added, files that include from the other path fail.
**How to avoid:** Always use the full CMake variable `${DPVS_INCLUDE_DIRS}` (not the individual components) — it already contains both paths as set by `FindDPVS.cmake`.
**Warning signs:** C1083 errors on `dpvsWrapper.hpp` or `dpvsCommander.hpp` after other dpvs headers compile fine.

### Pitfall 2: mozilla private headers included accidentally
**What goes wrong:** C1083 cascade on `nsCOMPtr.h`, `nsEmbedCID.h`, etc. — real XPCOM headers that modern MSVC cannot compile.
**Why it happens:** Adding `MOZILLA_PRIVATE_INCLUDE_DIR` to a target's include path, or accidentally including the full `libMozilla.cpp` (which has `#include <mozilla-config.h>` and pulls in the XPCOM universe).
**How to avoid:** The stub target uses only `MOZILLA_PUBLIC_INCLUDE_DIR`. The real `src/external/3rd/library/libMozilla/src/win32/libMozilla.cpp` is NEVER compiled per D-05.
**Warning signs:** Errors on NS_* macros, `nsresult`, `nsCOMPtr` — these are XPCOM symbols from the private headers.

### Pitfall 3: Bink static-linked instead of LoadLibrary
**What goes wrong:** Linker error on `binkw32.lib` — this is an import lib for a DLL that uses a non-standard calling convention; linking it statically against `/MT` causes CRT conflicts.
**Why it happens:** Assuming the presence of `BINK_LIBRARY` in `FindBink.cmake` means it should be linked.
**How to avoid:** `clientGraphics` includes Bink headers for function pointer type declarations only. `BinkDLL.cpp` uses `LoadLibrary("binkw32.dll")` at runtime. Do NOT add `${BINK_LIBRARY}` to `target_link_libraries(clientGraphics ...)`.

### Pitfall 4: ui lib include path exposes win32/_precompile.h incorrectly
**What goes wrong:** Including `_precompile.h` from the wrong path — `external/3rd/library/ui/include/_precompile.h` is a redirect (one-liner: `#include "../src/win32/_precompile.h"`). If only the `include/` path is added, the redirect works but the `UIStlFwd.h`, `UiReport.h` etc. that `_precompile.h` includes then fail because they're looked up relative to `src/win32/`.
**How to avoid:** Add BOTH `${SWG_EXTERNALS_FIND}/ui/include` AND `${SWG_EXTERNALS_FIND}/ui/src/win32` to `clientUserInterface` and `clientGame` include paths.

### Pitfall 5: clientGame subdirectory enumeration incomplete
**What goes wrong:** LNK2019 (unresolved external) for game-side symbols that appear in the missing subdir files.
**Why it happens:** 343 files across 22 subdirs — easy to miss a subdirectory when manually enumerating `SOURCES` sets.
**How to avoid:** Use the count-per-subdir table in this document. Verify: `shared/core/`=65, `shared/object/`=59, `shared/objectTemplate/`=41, `shared/playback/`=38, `shared/space/`=33, `shared/scene/`=15.

### Pitfall 6: engine/CMakeLists.txt not updated to add_subdirectory(client)
**What goes wrong:** CMake configure succeeds but none of the Phase 3 targets are built — they were never included in the CMake graph.
**Why it happens:** `src/engine/CMakeLists.txt` currently only has `add_subdirectory(shared)`. Adding `add_subdirectory(client)` is a Wave 0 task.
**Warning signs:** `cmake --build` shows 0 new targets; `build/whitengold.sln` doesn't contain `clientGraphics` etc.

### Pitfall 7: FirstClientUserInterface.h _precompile.h dependency before ui is built
**What goes wrong:** CMake configure-time error "Target 'ui' not found" when processing `clientUserInterface/src/CMakeLists.txt`.
**Why it happens:** If `ui` is added to `external/3rd/library/CMakeLists.txt` AFTER `engine/client/library/CMakeLists.txt` references it, CMake may encounter the reference before the definition.
**How to avoid:** Wave 0 adds `ui` to the `external/3rd/library/CMakeLists.txt` first. The build graph processes `external/` before `engine/` because `add_subdirectory(external)` appears before `add_subdirectory(engine)` in the root `src/CMakeLists.txt`.

---

## Validation Architecture

### Test Framework

| Property | Value |
|----------|-------|
| Framework | CMake build verification (no unit-test framework in this codebase) |
| Config file | `src/CMakeLists.txt` (root) |
| Quick run command | `cmake --build D:/Code/swg-client/build --config Debug --target clientDirectInput -- /m:4` |
| Full suite command | `cmake --build D:/Code/swg-client/build --config Debug --parallel 8` (then `--config Release`) |

### Phase Requirements → Build Verification Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| SC-1 | 13 `.lib` files present in `build/lib/Debug/` | smoke (dir check) | `dir D:\Code\swg-client\build\lib\Debug\client*.lib` | Wave 0 gap |
| SC-2 | Release config produces same 13 `.lib` files | smoke | `cmake --build build --config Release; dir build\lib\Release\client*.lib` | Wave 0 gap |
| SC-3 | First DX9 `.cpp` compile succeeds in clientGraphics | build | `cmake --build build --config Debug --target clientGraphics -- /m:1` | Wave 0 gap |
| SC-4 | 3 consecutive parallel Debug builds succeed | determinism | `cmake --build build --config Debug --parallel 8` (x3) | Wave 0 gap |
| SC-5 | `dumpbin /symbols clientUserInterface.lib \| findstr xpcom` returns empty | symbol check | PowerShell: `dumpbin /symbols build\lib\Debug\clientUserInterface.lib | Select-String xpcom` | Wave 0 gap |
| LAUNCH-02 | `libMozilla::init` stub resolves without XPCOM libs | link symbol | implicit in SC-1 (no LNK2019 on libMozilla symbols) | Wave 0 gap |

### Sampling Rate
- **Per task commit:** `cmake --build build --config Debug --target <newlib> -- /m:4` (quick single-target build)
- **Per wave merge (after Tier 7 and after Tier 9):** Full parallel Debug build of all Phase 3 targets so far
- **Phase gate (SC-4):** 3x consecutive `cmake --build build --config Debug --parallel 8` with no errors

### Wave 0 Gaps
- [ ] Phase 3 success criteria verification script (PowerShell) — covers SC-1 through SC-5
- [ ] No test framework install needed — pure CMake build verification

---

## Security Domain

Security enforcement is enabled. Phase 3 is a CMake build-system authoring phase — no network code, no authentication, no user data handling. ASVS categories are largely not applicable.

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | No | Not applicable — no auth code authored |
| V3 Session Management | No | Not applicable |
| V4 Access Control | No | Not applicable |
| V5 Input Validation | No | No user input processing — build-system files only |
| V6 Cryptography | No | Crypto lib already built in Phase 1/2; Phase 3 only links it |

### Known Threat Patterns for Build System

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Vendored SDK with prebuilt .lib (DPVS) | Tampering | Prebuilt verified present before Phase 1; CRT policy `/MT` verified via `dumpbin /headers` at Phase 4 gate |
| libMozilla stub returning true for all calls | Spoofing | Acceptable for v1 — in-game browser is not a security boundary in Phase 3; the real threat is failing to link at all |

---

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | `target_link_libraries` sets for each Phase 3 library inferred from include-path scanning — not from verified swg-main CMakeLists (which don't exist for client libs) | Dependency Graph | LNK2019 at compile time of the static lib if a dep is missing; typically detected immediately and fixable by adding the dep |
| A2 | `ui` library (126 files) builds cleanly under MSVC 14.4x with C++17 and STLPort headers — SOE-authored but no verified modern build | Additional Phase 3 Target | Possible C4xxx warnings; if blocking, may need per-target C4530/C4996 suppressions or `-wd` additions |
| A3 | `clientBugReporting` all-win32 `.cpp` files compile without OS-level crash reporter headers beyond `<lmcons.h>` and standard WinAPI — no videocapture or soePlatform headers found in grep scan | SDK Dependency Map | Low risk — the 5 files use only `sharedFoundation` and Windows system headers |

**If this table is empty:** All other claims in this research were verified by direct source inspection.

---

## Open Questions (RESOLVED)

1. **`libMozilla` target placement: inline stub vs. subdirectory** (RESOLVED)
   - What we know: D-04 places stub at `src/cmake/stubs/libMozilla_stub.cpp`. The Phase 3 plan must declare the `libMozilla` CMake target somewhere.
   - **Resolution:** Inline target in `engine/client/library/CMakeLists.txt` using absolute path to the stub — same pattern as Phase 2 INTERFACE stubs (sharedCollision etc.) declared inline in the library-level CMakeLists. Implemented in Plan 03-01 Task 2.

2. **exact `ui` library source enumeration** (RESOLVED)
   - What we know: 126 `.cpp` files in `src/shared/` and `src/win32/`. Both directories exist.
   - **Resolution:** Include paths `${SWG_EXTERNALS_FIND}/ui/include`, `${SWG_EXTERNALS_FIND}/ui/src/win32`, and `${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFoundation/include/public`. Executor reads actual directories before writing source list. Implemented in Plan 03-01 Task 3.

3. **`clientGame` exact `target_link_libraries` list** (RESOLVED)
   - What we know: `clientGame` includes headers from all other Phase 3 libs plus all Phase 2 libs plus external SDKs.
   - **Resolution:** Starting dep set provided in Plan 03-05 Task 1 interfaces block. Executor iterates based on LNK2019 errors and documents additions in SUMMARY. The incomplete transitive closure is an accepted assumption (A1) — LNK2019 iteration is the standard mitigation pattern.

---

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| MSVC 14.4x (VS 2022 v143) | All Phase 3 targets | Already verified Phase 2 | v143 | None |
| DX9 vendored SDK | clientDirectInput, clientGraphics | Verified present (Phase 1) | Circa April 2005 | None |
| DPVS prebuilt .lib | clientGraphics, clientObject, clientParticle, clientSkeletalAnimation, clientTerrain, clientGame | Verified: dpvs.lib + dpvsd.lib at `dpvs/lib/win32-x86/` | N/A (prebuilt) | Source-build (Phase 4 fallback per D-03) |
| Miles Mss32.lib | clientAudio | Verified present (Phase 1) | N/A (prebuilt) | None |
| Vivox prebuilts | clientUserInterface | Verified: vivoxSharedWrapper_Debug.lib + Release.lib in `lib/` | N/A (prebuilt) | None |
| VideoCapture prebuilts | clientAudio, clientGraphics | Verified: all 15 .lib files present | N/A (prebuilt) | None |
| lgLcd.lib (Logitech LCD) | clientUserInterface | Verified present in `lcdui/lib/` | N/A (prebuilt) | Disable via config (`enableG15Lcd=false`); optional at runtime |
| libMozilla public headers | clientGame, clientUserInterface, libMozilla stub | Verified: `include/public/libMozilla/libMozilla.h` exists | N/A | Stub is the mitigation |

**No missing dependencies with no fallback.** All required SDKs are present in the vendored tree.

---

## Sources

### Primary (HIGH confidence)
- Direct filesystem scan of `D:/Code/swg-client/src/engine/client/library/` — source file counts, PCH header names, SDK include patterns
- Direct read of `src/cmake/win32/Find*.cmake` — all 13 Find modules read and variables catalogued
- Direct read of `src/external/3rd/library/libMozilla/src/win32/libMozilla.h` — complete public API for stub
- Direct read of `src/external/3rd/library/libMozilla/src/win32/libMozilla.cpp` — confirmed original requires XPCOM headers; stub approach validated
- Direct read of `FirstClient*.h` for all 12 libraries — SDK dependencies via PCH confirmed
- `docs/research/swgclient-build.md` — dependency graph, library list, external link libs
- Phase 2 `02-PATTERNS.md` and `02-RESEARCH.md` — pattern inheritance confirmed

### Secondary (MEDIUM confidence)
- Phase 2 `02-CONTEXT.md` decisions — D-01 through D-15 all confirmed in `03-CONTEXT.md`
- `src/engine/client/library/clientGame/src/shared/core/Game.cpp` — grep scan for libMozilla, DPVS usage confirmed

### Tertiary (LOW confidence)
- `target_link_libraries` dependency sets for Phase 3 libraries — inferred from include scanning; no swg-main client CMakeLists exist to cross-reference [A1]

---

## Metadata

**Confidence breakdown:**
- Source file inventory: HIGH — direct filesystem count + PCH header read for all 12 libraries
- SDK dependency map: HIGH — grep scan of actual source files confirms which SDK headers each library includes
- XPCOM stub entry points: HIGH — libMozilla.h read directly; all public API functions listed
- target_link_libraries graph: MEDIUM — inferred from includes, not from verified CMakeLists (no swg-main client equivalents exist)
- ui library characterisation: MEDIUM — directory counted; source content not exhaustively read

**Research date:** 2026-05-04
**Valid until:** 2026-06-04 (stable vendored tree; 30-day horizon)
