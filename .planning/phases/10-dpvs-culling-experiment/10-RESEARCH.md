# Phase 10: DPVS Culling Experiment — Research

**Researched:** 2026-05-10
**Domain:** D3D9 GPU instrumentation; in-engine debug overlays; CSV logging; runtime A/B feature toggles
**Confidence:** HIGH (codebase reads VERIFIED; D3D9 API surface CITED from Microsoft docs and authoritative community reference)

## Summary

Phase 10 is purely **additive scaffolding for one measurement run**, then conditional source edits, then a single cleanup commit. Every Claude's-Discretion question resolved during research has a known landing site in the existing v2 tree. The most consequential discovery is that **the D3D9 device handle (`IDirect3DDevice9*`) does not cross the plugin DLL boundary** — `Direct3d9.dll` exposes the device only internally. The engine talks to the renderer through `Gl_api` (119-function table) and through `Graphics::*` static methods. This means D3D9 timestamp queries cannot be issued from `RenderWorld.cpp` directly; they must be issued from inside the `Direct3d9` plugin and exposed through new throwaway functions on `Gl_api` + `Graphics`. Three precedent functions on the existing API (`pixSetMarker`, `pixBeginEvent`, `pixEndEvent`) prove the pattern and provide the file-by-file template.

The CPU pair (`QueryPerformanceCounter`) is trivial: a `PerformanceTimer` wrapper already exists in `sharedDebug` and is precisely the right shape — start/stop pair, microsecond resolution, no install dependency.

A new `DpvsProfileInstrumentation` module (engine-side, lives in `clientGraphics` next to RenderWorld) is the cleanest container for the CSV writer, run-label state, F10/F11 toggle handlers, overlay state, and `ExitChain` teardown. It also gives D-15 a one-file cleanup target ("delete `DpvsProfileInstrumentation.{cpp,h}`, revert ~8 callsite edits"). The DEBUG_REPORT_PRINT pipeline already drives an on-screen overlay via DebugMonitor — the simplest overlay is to register a DebugFlag report routine that prints DPVS state and run-label every frame, leveraging the same surface that `RenderWorldNamespace::reportMetrics()` already uses for DPVS visible-object counts.

F10/F11 do NOT need to route through `InputMap` data files (the `.iff` input maps live in TRE archives, not git). The cleanest seam is a direct DirectInput key hook in `CuiIoWin::processEvent` (which already special-cases `CMD_console`/`CMD_uiPointerToggle`) gated by `#ifdef _DEBUG` and a `disableOcclusionCullingExperiment` config key — fits the codebase's convention of debug-only direct key hooks (see `IMEManager` which already special-cases F10/F11 for IME).

**Primary recommendation:** Land one new module (`DpvsProfileInstrumentation`) that owns ALL Phase 10 instrumentation state, plus four small surgical edits — (1) three function-pointer additions to `Gl_api` for GPU timestamps, mirrored in `Direct3d9.cpp`, (2) a thin `Graphics::*` shim that forwards to those `Gl_api` entries, (3) a `CuiIoWin::processEvent` debug hook for F10/F11 routed to the new module, (4) a `Commands::setrunlabel` entry in `SwgCuiCommandParserDefault`. RenderWorld.cpp gets two lines added at line 1042 (`Graphics::dpvsGpuTimingBegin()` before the resolveVisibility call) and 1046 (`Graphics::dpvsGpuTimingEnd()` after it), plus a `DpvsProfileInstrumentation::onFrameEnd()` somewhere in `Game::run()` near `Graphics::present()` to flush the CSV row. Verdict at end → conditional D-13/D-14 edits → D-15 cleanup commit deletes the new module and reverts the small edits in one shot.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Profiling methodology**
- **D-01: GPU timing — D3D9 `IDirect3DQuery9` timestamp queries in-engine.** `D3DQUERYTYPE_TIMESTAMP` pair plus a `D3DQUERYTYPE_TIMESTAMPDISJOINT` containing query around the DPVS/scene-render block. Convert to microseconds via the disjoint frequency. External capture tools (PIX Legacy, Intel GPA, NVIDIA Nsight) explicitly NOT used.
- **D-02: CPU timing — existing `ms_dpvsQueryProfilerBlock` + new `QueryPerformanceCounter` pair.** Reuse the existing ProfilerBlock at `RenderWorld.cpp:1041-1044`. Add a fresh QPC start/stop pair flanking `resolveVisibility()`. Both contribute columns to the CSV.
- **D-03: Output channels — CSV log file + on-screen overlay.** Per-frame samples append to CSV. Overlay shows current `disableOcclusionCulling` state, current run-label, and a rolling capture indicator.
- **D-04: Statistics — per-frame raw + distributional summary.** Median, p95, p99, max, stdev per condition × per scene. Means are NOT the primary stat.

**Test scenes + sample protocol**
- **D-05: Single test scene — Mos Eisley cantina plaza.** Single-zone verdict is sufficient.
- **D-06: Automation level — keybind-toggle capture, manual everything else.** Kenny drives the capture: parks the camera, picks the moment, decides duration. F10 toggles capture on/off. F11 toggles `RenderWorld::setDisableOcclusionCulling()`. A console command `/setrunlabel <string>` writes the run-label into the CSV.
- **D-07: Runtime A/B toggle — new F11 keybind calling `RenderWorld::setDisableOcclusionCulling()`.** Toggle is instantaneous; no relog or zone-change required. F11 must NOT conflict with an existing binding.
- **D-08: Sample count — 3 capture passes per condition** (~10s each, ~600 frames at 60fps). 6 captures total per session. Alternate ON-OFF-ON-OFF-ON-OFF suggested.

**Decision threshold**
- **D-09: Primary metric — total frame time, median + p95.** GPU and CPU components recorded for diagnostic value but verdict turns on total.
- **D-10: Threshold rule — DPVS-off median+p95 BOTH ≤ DPVS-on by any margin → remove; any regression on either → keep.**
- **D-11: Inconclusive outcome → default to keep.** Status quo wins ties.
- **D-12: Decision scope — D3D9 renderer only.** Phase 11 must re-measure under D3D11.

**Removal mechanism (if verdict is "remove")**
- **D-13: Source-edit the `OCCLUSION_CULLING` bit off in `RenderWorld.cpp:906`.** Single-line edit, plus mirror in `#ifdef _DEBUG` branch at line 903.
- **D-14: Delete the `disableOcclusionCulling` config key** after D-13. Remove `ms_disableOcclusionCulling` (line 38), `KEY_BOOL` (line 101), getter (line 245), `RenderWorld::setDisableOcclusionCulling()` (line 1151), `RenderWorld.h` line 104.
- **D-15: Instrumentation cleanup — single commit removes it all post-verdict.** D3D9 timestamp queries, QPC timer pair, CSV logger, F11 keybind, F10 capture toggle, overlay strings, `/setrunlabel` command.
- **D-16: Verdict documentation lives at `docs/recon/10-dpvs-profiling.md`.** Sibling format to existing `docs/recon/05-client-boot-sequence.md`, `07-swg-source-diff-findings.md`.

### Claude's Discretion

The following details are left to the researcher/planner — this RESEARCH.md answers each of them:
- Exact CSV file path on disk (researcher answers below: `<exe-dir>/dpvs-profile/<run-label>-<frame_no_start>.csv`)
- Overlay rendering surface (researcher answers below: DebugFlags report routine via `DEBUG_REPORT_PRINT` — same surface as `RenderWorldNamespace::reportMetrics`)
- Console command grammar for `/setrunlabel` (researcher answers below: matches `Commands::afkmessage` shape)
- Where the F10/F11 keybindings are wired (researcher answers below: direct `_DEBUG` hook in `CuiIoWin::processEvent` IOET_KeyDown branch — F10/F11 are NOT bound in source today)
- Whether to capture DPVS-reported visible-object counts (researcher answers below: YES — `RenderWorldCommander::getNumberOfVisibleObjects()` already exposes this for free)
- Microsecond vs millisecond unit choice in the CSV (researcher answers below: **microseconds for raw**, analysis script converts to milliseconds)

### Deferred Ideas (OUT OF SCOPE)

- **Multi-zone study (Coronet starport, Theed plaza).** Single-zone is the verdict.
- **Scripted camera-path recording/replay.** Out of proportion for a profiling experiment.
- **Permanent profiling instrumentation.** Per D-15, all scaffolding is removed post-verdict.
- **DPVS library deletion.** Library stays — indoor portal traversal needs it.
- **Re-running Phase 10 under D3D11.** Belongs to Phase 11.
- **draw_call_count column as a hard requirement.** Researcher decides; this RESEARCH.md recommends it as an optional column.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| DPVS-01 | `disableOcclusionCulling` config flag exposed via `client.cfg` and tested; frame time (GPU + CPU) measured with and without `resolveVisibility()` in a busy outdoor zone | Config plumbing already complete (Koogie inheritance — see `ConfigClientGraphics.cpp:38,101,245` + `RenderWorld.cpp:78,906,1151`). Measurement half delivered by new GPU timestamp queries (D-01), QPC pair (D-02), CSV logger, F11 A/B toggle. Mos Eisley plaza is the busy outdoor zone (D-05). |
| DPVS-02 | If profiling shows no regression: DPVS occlusion culling removed permanently from outdoor scenes; portal traversal retained for indoor cells; result documented in `docs/recon/` | Conditional one-line edit at RenderWorld.cpp:906 + mirror at line 903 (D-13). Followup small commit deletes the now-dead config key (D-14). Verdict + analysis lives in `docs/recon/10-dpvs-profiling.md` (D-16). Verdict decision rule fully specified in D-09/D-10/D-11. |
</phase_requirements>

## Project Constraints (from CLAUDE.md)

Inherited from whitengold `D:/Code/swg-client/CLAUDE.md` (v2 has no CLAUDE.md of its own, but inherits all conventions):

- **No C++ source edits without justification** — v1 rule; in v2 `feedback_source_edits_allowed.md` memory authorizes source edits. Phase 10 requires source edits (instrumentation + conditional removal); D-15 mandates a clean revert post-verdict.
- **Naming conventions:** PascalCase classes (`DpvsProfileInstrumentation`), camelCase functions (`onFrameEnd`), ALL-CAPS macros, `ms_` prefix for module-static state, `First<Component>.h` precompiled-header convention.
- **No exceptions** — use `FATAL`, `WARNING`, `DEBUG_FATAL`, `DEBUG_WARNING` macros for error handling. CSV file-open failure should `DEBUG_WARNING` not `FATAL` (it must not block client boot if CSV path is unwritable).
- **Include conventions:** `#include "componentName/HeaderName.h"` — full component path from include root, NO relative paths. First include in every .cpp must be `FirstClientGraphics.h` (or the matching `First<Component>.h`).
- **`Install()`/`Remove()` pattern with ExitChain::add** — the codebase's standard module-lifecycle pattern. Every new module follows it.
- **GSD Workflow Enforcement** — phase work flows through `/gsd-plan-phase` then `/gsd-execute-phase`; the planner is the next consumer of this RESEARCH.md.

## Architectural Responsibility Map

Phase 10 capability ownership across the v2 tree's tier structure:

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| GPU timestamp query issuance (`IDirect3DQuery9`) | Renderer plugin (`Direct3d9.dll`) | — | `IDirect3DDevice9*` is plugin-private; cannot leak across the `Gl_api` boundary. New `Gl_api` entries forward to plugin-internal handlers. |
| GPU timestamp query data retrieval + microsecond conversion | Renderer plugin (`Direct3d9.dll`) | Engine (`clientGraphics`) | Plugin owns the disjoint frequency + the latency-buffer of queries; returns the µs value via output param when ready. Engine just receives the µs. |
| CPU QPC pair around resolveVisibility | Engine (`clientGraphics/RenderWorld.cpp`) | — | QPC is OS-level — works anywhere. Lives at the call site. Use existing `PerformanceTimer` wrapper from `sharedDebug`. |
| CSV writer + per-frame row append | Engine (`clientGraphics` — new `DpvsProfileInstrumentation` module) | `fileInterface/StdioFile` | Engine module is the only place that has visibility into both the GPU number (forwarded from plugin) and the CPU number. |
| Run-label state (set by `/setrunlabel`, read by CSV row writer) | Engine (new `DpvsProfileInstrumentation` module) | Game (`SwgCuiCommandParserDefault`) | Module owns the state; command parser pokes it via setter. |
| F10 capture toggle | Engine (`clientUserInterface/CuiIoWin`) | Engine (new module — handler) | CuiIoWin already intercepts keys before InputMap. Debug-only `#ifdef _DEBUG` block calls `DpvsProfileInstrumentation::toggleCapture()`. |
| F11 DPVS toggle | Engine (`clientUserInterface/CuiIoWin`) | Engine (`clientGraphics/RenderWorld`) | Same debug-only block. Calls `RenderWorld::setDisableOcclusionCulling(!RenderWorld::getDisableOcclusionCulling())`. |
| On-screen overlay | Engine (new module — DebugFlag report routine) | Engine (`sharedDebug/Report` + `DebugMonitor`) | Reuses the existing `DEBUG_REPORT_PRINT` pipeline that `RenderWorld::reportMetrics` already uses. Zero new render code. |
| `/setrunlabel` console command | Game (`SwgCuiCommandParserDefault`) | Engine (new module — setter) | Existing command-parser pattern. Drops one entry into the cmds[] table + one branch in `performParsing`. |
| Verdict documentation | Repo top-level (`docs/recon/10-dpvs-profiling.md`) | — | Per D-16, sibling format to existing `docs/recon/*.md`. |
| Conditional source-edit removal | Engine (`clientGraphics/RenderWorld.cpp` lines 903, 906) | Engine (`clientGraphics/ConfigClientGraphics.{cpp,h}` + `RenderWorld.h`) | Per D-13/D-14. Two edits in RenderWorld, four in ConfigClientGraphics, one in RenderWorld.h. |
| Cleanup commit | Engine (delete new module file pair + revert 4-5 surgical edits) | — | Per D-15. New-module-with-ExitChain pattern means cleanup is mostly "delete the new file." |

## Standard Stack

### Core (already present in v2)
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| DirectX 9 | SDK June 2010 (vendored at `src/external/3rd/library/directx9/`) [VERIFIED: repo grep] | `IDirect3DQuery9`, `D3DQUERYTYPE_TIMESTAMP`, `D3DQUERYTYPE_TIMESTAMPDISJOINT` — GPU timing API | Engine's existing renderer; this is the API that ships with the renderer plugin. No new dependency. |
| Win32 `QueryPerformanceCounter` | Win32 API | Sub-µs CPU timer | Existing engine wrapper `PerformanceTimer` already uses it [VERIFIED: `src/engine/shared/library/sharedDebug/src/win32/PerformanceTimer.cpp`]. |
| `sharedDebug/Profiler` | Engine-owned | Sample sampling for `ms_dpvsQueryProfilerBlock` | Already brackets `resolveVisibility()` [VERIFIED: `RenderWorld.cpp:98,1044`]. Reuse per D-02. |
| `sharedDebug/Report` / `DEBUG_REPORT_PRINT` | Engine-owned | Per-frame on-screen text overlay (via DebugMonitor child window) | Already used by `RenderWorldNamespace::reportMetrics` to print DPVS stats [VERIFIED: `RenderWorld.cpp:1167-1180`]. Reuse for the new module's report routine. |
| `fileInterface/StdioFile` | Engine-owned (ours/library) | File handle for CSV output | Already used for alias save/load + crash report capture [VERIFIED: `SwgCuiCommandParserDefault.cpp:216,272`]. |
| `sharedFoundation/ExitChain` | Engine-owned | Module teardown registration | Standard pattern — every engine module uses `ExitChain::add(&remove, "ModuleName::Remove")` [VERIFIED: `RenderWorld.cpp:255`]. |
| `sharedFoundation/ConfigFile` | Engine-owned | `client.cfg` key reading | Existing pattern with `KEY_BOOL` macro in `ConfigClientGraphics.cpp` [VERIFIED: line 67]. New: add a `[Dpvs/Experiment]` section key `enableInstrumentation` (default true in debug, false in release) so the whole module compiles into a no-op when not desired. |

### Supporting (new — written for Phase 10)
| Component | Purpose | When to Use |
|-----------|---------|-------------|
| `DpvsProfileInstrumentation` (new module) | Owns CSV writer, run-label state, F10 capture toggle state, F11 forward, overlay print routine, GPU/CPU number aggregation | Installed once during `clientGraphics` setup; removed in the cleanup commit (D-15). |
| Three new `Gl_api` function pointers — `dpvsGpuTimingBegin`, `dpvsGpuTimingEnd`, `dpvsGpuTimingPollResult` | Cross the plugin DLL boundary so engine can drive GPU queries owned by `Direct3d9.dll` | Mirror the `pixSetMarker/Begin/End` pattern that already exists in `Gl_api` [VERIFIED: `Gl_dll.def:226-228`]. |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `IDirect3DQuery9` in-engine | External PIX Legacy / Intel GPA / NVIDIA Nsight capture | Heavy workflow; D3D9 support patchy on modern releases. Locked-OUT by D-01. |
| New DebugFlag report routine | `SwgCuiHud` text widget rendering full UI text | UI canvas approach takes ~20 files of plumbing (mediator, controller, factory, layout). The DebugFlag routine takes one function. |
| `SwgCuiHud` overlay | Direct `DebugPrimitive` text | DebugPrimitive is 3D-world space (anchored to a Transform), not screen-space — wrong tool. |
| `InputMap` data-file binding for F10/F11 | Direct hook in `CuiIoWin::processEvent` | InputMap source-of-truth is `.iff` files in TRE archives; modifying them is out of scope. The existing IMEManager already special-cases F10/F11 [VERIFIED: `IMEManager.cpp:961-963`] so a debug hook in `CuiIoWin` matches precedent. |
| Free-standing `FILE*` for CSV | `fileInterface/StdioFile` | StdioFile gives us the engine's standard error-handling shape, is what alias save/load already uses, and survives `ExitChain` teardown predictably. |

**Installation:** No external packages — all dependencies are vendored or system-level.

**Version verification:** N/A — using internal engine APIs and the vendored DirectX 9 June 2010 SDK already in the repo.

## Architecture Patterns

### System Architecture Diagram

```
┌──────────────────────────────────────────────────────────────────────────┐
│  Per-frame data flow during DPVS profiling capture                       │
└──────────────────────────────────────────────────────────────────────────┘

   User Input (Kenny)                          Console (/setrunlabel)
       │                                              │
       ▼                                              ▼
   [F10/F11 KeyDown]                       [SwgCuiCommandParserDefault]
       │                                              │
       ▼                                              ▼
   CuiIoWin::processEvent                  Commands::setrunlabel branch
   (#ifdef _DEBUG hook in IOET_KeyDown)            │
       │                                           │
       │ F10: toggleCapture                        │ setRunLabel(argv[1..])
       │ F11: setDisableOcclusionCulling           │
       ▼                                           ▼
   ┌────────────────────────────────────────────────────────────────┐
   │  DpvsProfileInstrumentation (new module, clientGraphics)       │
   │    - ms_captureActive : bool                                   │
   │    - ms_runLabel      : std::string                            │
   │    - ms_csv           : StdioFile *                            │
   │    - ms_frameRows     : staging buffer for current frame       │
   │    - install()/remove() — ExitChain::add(&remove, ...)         │
   └────────────────────────────────────────────────────────────────┘
       ▲                                ▲                    ▲
       │ frame_no, dpvs_state           │ cpu_us             │ gpu_us
       │                                │                    │
   Graphics::getFrameNumber()      QPC pair (PerformanceTimer)   Gl_api->dpvsGpuTiming*
       │                                │                    │
       │                                ▼                    ▼
       │              ┌─────────────────────────────────────────────┐
       │              │ RenderWorld.cpp                              │
       │              │   line 1042  Graphics::dpvsGpuTimingBegin()  │
       │              │   line 1043  qpcPair.start()                 │
       │              │   line 1044  NP_PROFILER_BLOCK_ENTER(...)   │
       │              │   line 1046  resolveVisibility(...)          │
       │              │   line 1047  qpcPair.stop()                  │
       │              │   line 1047  Graphics::dpvsGpuTimingEnd()    │
       │              └─────────────────────────────────────────────┘
       │                                                     │
       │                                                     ▼
       │                              ┌────────────────────────────────────┐
       │                              │ Direct3d9.dll (renderer plugin)    │
       │                              │   - 4× IDirect3DQuery9 (double-buf)│
       │                              │   - 2× DISJOINT, 4× TIMESTAMP      │
       │                              │   - Issue(D3DISSUE_BEGIN/END)      │
       │                              │   - GetData (non-blocking, F-2 lat)│
       │                              │   - converts ticks → µs via freq   │
       │                              └────────────────────────────────────┘
       │
       │  Per-frame end-of-frame call (in Game::run() near present()):
       │     DpvsProfileInstrumentation::onFrameEnd()
       ▼                       │
   Per-frame CSV row append    │
   to: <exe-dir>/dpvs-profile/ │  Per-frame overlay print
       <runLabel>-<framN>.csv  │  via DEBUG_REPORT_PRINT (DebugMonitor)
                               ▼
              "DPVS:OFF  run=mosEisley-pass1-on  REC  frame=14523"
```

The diagram shows the keybind-driven A/B protocol: F10 toggles capture (gating the per-frame row append), F11 toggles the DPVS bit (changing what gets measured), `/setrunlabel` sets a free-form tag column. The GPU timing path crosses the plugin DLL boundary via three new `Gl_api` entries. The CPU and frame-counter paths are engine-side. CSV file rolls over on each `/setrunlabel` call so each pass gets its own file.

### Recommended Project Structure

New files (all in `src/engine/client/library/clientGraphics/`):

```
src/engine/client/library/clientGraphics/
├── include/public/clientGraphics/
│   └── DpvsProfileInstrumentation.h    # NEW — public surface for the module
├── src/shared/
│   ├── DpvsProfileInstrumentation.cpp  # NEW — implementation, CSV writer, overlay print, state
│   └── DpvsProfileInstrumentation.h    # NEW — internal header (mirrors public)
```

Modified files (5 files, ~15 lines of additive edits total):

```
src/engine/client/application/Direct3d9/src/win32/Direct3d9.cpp
  + ~50 lines: query handle pool, Begin/End/Poll functions, ms_glApi.dpvsGpuTiming* assignments
src/engine/client/library/clientGraphics/src/win32/Gl_dll.def
  + 3 lines:  three new function pointers in struct Gl_api
src/engine/client/library/clientGraphics/src/win32/Graphics.h + Graphics.cpp
  + 3 forwarder methods
src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp
  + 4 lines:  GPU timing begin/end + QPC pair + onFrameEnd hook in setup()
  + module install/remove wiring
src/engine/client/library/clientGraphics/src/shared/SetupClientGraphics.cpp
  + 1 line:   DpvsProfileInstrumentation::install() in the install sequence
src/engine/client/library/clientUserInterface/src/shared/core/CuiIoWin.cpp
  + ~10 lines: #ifdef _DEBUG block in IOET_KeyDown for F10/F11
src/game/client/library/swgClientUserInterface/src/shared/parser/SwgCuiCommandParserDefault.cpp
  + 3 lines:  MAKE_COMMAND(setrunlabel) + cmds[] entry + performParsing branch
src/engine/client/library/clientGame/src/shared/core/Game.cpp
  + 1 line:   DpvsProfileInstrumentation::onFrameEnd() near Graphics::present()
```

### Pattern 1: Module Install/Remove with ExitChain

**What:** Every new engine module follows the install/remove with `ExitChain::add` pattern.
**When to use:** Always, for any new module that owns state, opens file handles, or holds resources.
**Example (from RenderWorld.cpp, the closest precedent):**
```cpp
// Source: VERIFIED at src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp:255
ExitChain::add(&remove, "RenderWorld::Remove");
```

For `DpvsProfileInstrumentation`, the install function opens no file initially (CSV file opens on first `/setrunlabel` call or on first frame after F10 capture turns on), registers a DebugFlag report routine, sets up double-buffered query slots (via Gl_api), and registers itself for `ExitChain::add(&remove, "DpvsProfileInstrumentation::Remove")`.

### Pattern 2: Cross-Plugin-DLL API Extension via Gl_api

**What:** When clientGraphics needs renderer-private state (like `IDirect3DDevice9*`), the codebase adds a function pointer to `struct Gl_api`, mirrors the assignment in `Direct3d9.cpp`, and adds a thin `Graphics::*` static forwarder.
**When to use:** Whenever the engine side of a feature needs a renderer-side capability that's behind the plugin boundary.
**Example (from existing PIX markers):**
```cpp
// Source: VERIFIED at src/engine/client/library/clientGraphics/src/win32/Gl_dll.def:226-228
void (*pixSetMarker)(WCHAR const * markerName);
void (*pixBeginEvent)(WCHAR const * eventName);
void (*pixEndEvent)(WCHAR const * eventName);

// Source: VERIFIED at src/engine/client/application/Direct3d9/src/win32/Direct3d9.cpp:4571-4587
void Direct3d9Namespace::pixBeginEvent(WCHAR const * eventName)
{
#if (D3D_SDK_VERSION & 0x7fffffff) >= 32
    D3DPERF_BeginEvent(0xffffffff, eventName);
#else
    UNREF(eventName);
#endif
}
// + assignment: ms_glApi.pixBeginEvent = pixBeginEvent;  (line 1123)
```

**For Phase 10, mirror this exactly:** add `dpvsGpuTimingBegin()`, `dpvsGpuTimingEnd()`, `dpvsGpuTimingPollResult(uint32 * out_microseconds)` to `Gl_api`, implement them in `Direct3d9.cpp` using `IDirect3DDevice9::CreateQuery`/`Issue`/`GetData`, and forward them via three new `Graphics::*` methods.

### Pattern 3: Console Command Registration

**What:** Console commands in the `/setrunlabel <string>` shape live in `SwgCuiCommandParserDefault.cpp` and follow the `MAKE_COMMAND` + `cmds[]` entry + `performParsing` branch pattern.
**When to use:** For any new client-side console command.
**Example (closest analog — `/afkmessage <message>`):**
```cpp
// Source: VERIFIED at src/game/client/library/swgClientUserInterface/src/shared/parser/SwgCuiCommandParserDefault.cpp:131,199,1745
namespace Commands { MAKE_COMMAND (afkmessage); }
// ...
{ Commands::afkmessage,  0, "<message>",  "Set the auto-response message ..."},
// ...
else if (isCommand(argv[0], Commands::afkmessage))
{
    if (argv.size() > 1)
    {
        Unicode::String message;
        for (unsigned int i = 1; i < argv.size(); ++i)
            message += argv[i];
        AwayFromKeyBoardManager::setAutoResponseMessage(message);
        return true;
    }
}
```

**For Phase 10:** add `MAKE_COMMAND(setrunlabel)`, register `{ Commands::setrunlabel, 0, "<label>", "Set the DPVS profiling run-label written to subsequent CSV rows." }`, branch in `performParsing` calls `DpvsProfileInstrumentation::setRunLabel(Unicode::wideToNarrow(joinedArgs))`.

### Pattern 4: D3D9 Timestamp Query

**What:** Microsoft's documented D3D9 timestamp-query pattern uses a TIMESTAMPDISJOINT containing query plus two TIMESTAMP queries. The disjoint query gives the frequency and a validity flag; the timestamps give two tick counts whose delta divided by the frequency is the elapsed seconds.
**When to use:** Whenever measuring elapsed GPU time for a specific draw region.
**Example (D3D9 minimal pattern):**
```cpp
// Source: CITED https://learn.microsoft.com/en-us/windows/win32/direct3d9/queries
IDirect3DQuery9 * disjoint;
IDirect3DQuery9 * tsStart;
IDirect3DQuery9 * tsEnd;
device->CreateQuery(D3DQUERYTYPE_TIMESTAMPDISJOINT, &disjoint);
device->CreateQuery(D3DQUERYTYPE_TIMESTAMP,         &tsStart);
device->CreateQuery(D3DQUERYTYPE_TIMESTAMP,         &tsEnd);

// per frame:
disjoint->Issue(D3DISSUE_BEGIN);
tsStart->Issue(D3DISSUE_END);    // NB: timestamp queries use D3DISSUE_END for both start and stop sites
// ... rendering ...
tsEnd->Issue(D3DISSUE_END);
disjoint->Issue(D3DISSUE_END);

// retrieval (non-blocking; defer N-2 frames to avoid stall):
struct DisjointData { UINT64 frequency; BOOL disjoint; } d;
UINT64 startTick = 0, endTick = 0;
HRESULT h1 = disjoint->GetData(&d, sizeof(d), D3DGETDATA_FLUSH);
HRESULT h2 = tsStart ->GetData(&startTick, sizeof(startTick), D3DGETDATA_FLUSH);
HRESULT h3 = tsEnd   ->GetData(&endTick,   sizeof(endTick),   D3DGETDATA_FLUSH);
if (h1 == S_OK && h2 == S_OK && h3 == S_OK && !d.disjoint)
{
    double seconds = double(endTick - startTick) / double(d.frequency);
    // microseconds = seconds * 1e6
}
// else: S_FALSE means not ready — try again next frame
```

**Key implementation detail for Phase 10:** because the CPU runs ahead of the GPU by 1-3 frames, calling `GetData(...)` with `D3DGETDATA_FLUSH` on the same frame the query was issued will either stall (bad — defeats the measurement) or return S_FALSE. **The correct pattern is double-buffering: issue queries into slot `frame % N`, read from slot `(frame + 1) % N` (where N=3 is safe for any realistic GPU lag).** This is THE pitfall in D3D timestamp queries [CITED: https://therealmjp.github.io/posts/profiling-in-dx11-with-queries/].

### Pattern 5: QueryPerformanceCounter via Engine Wrapper

**What:** Use the existing `PerformanceTimer` class instead of raw `QueryPerformanceCounter` for the CPU pair.
**When to use:** Any new CPU timing pair in the engine.
**Example:**
```cpp
// Source: VERIFIED at src/engine/shared/library/sharedDebug/src/win32/PerformanceTimer.cpp:21-89
PerformanceTimer::install();  // already called once at boot; PhaseTen does not re-call

PerformanceTimer t;
t.start();
ms_dpvsCamera->resolveVisibility(ms_commander, portalRecusionDepth, 0.0f);
t.stop();
float elapsedSeconds = t.getElapsedTime();
// microseconds = elapsedSeconds * 1e6f
```

The `PerformanceTimer` has microsecond resolution (its `ms_frequency` is `QueryPerformanceFrequency`), zero install cost (it's an automatic local variable), and matches the engine's idiomatic shape.

### Anti-Patterns to Avoid

- **Issuing GPU queries from inside the plugin and exposing the raw `IDirect3DQuery9 *` to engine code.** The plugin DLL boundary exists for a reason — the engine doesn't link D3D9 headers. Forwarding raw pointer types breaks the boundary. Pass µs (or `S_FALSE`-style "not ready" signal) across the API instead.
- **Calling `GetData(...)` with `D3DGETDATA_FLUSH` on the same frame the query was issued.** Either stalls the CPU or returns S_FALSE. Double-buffer the queries — read from frame N-2 or N-3.
- **Failing to check the disjoint flag.** If the GPU's frequency changed mid-measurement (driver throttle, AC power unplug, thermal event), the ticks are meaningless. Always check `d.disjoint == FALSE` before using the result; if TRUE, log it and continue with the next frame's measurement.
- **Treating CSV rows from before F10 capture turns on as part of the dataset.** F10 capture-on/-off must gate the row append, not just the data collection. Otherwise rows from camera-positioning time pollute the per-condition statistics.
- **Forgetting that `Game::run()`'s elapsedTime is `Clock::frameTime()` from the PREVIOUS frame.** [VERIFIED: `Game.cpp:1097`] — the value at the top of each iteration is the time taken by frame N-1. If you record `total_frame_ms` at end-of-frame, you'd record N-1's time labeled as frame N. Either record at top-of-frame and label as N-1, or record at end-of-frame after the next `Clock::frameTime()` call. The CSV column meaning must match the choice.
- **Routing F10/F11 through `InputMap`.** The input map's source-of-truth is `.iff` files in TRE archives — modifying those is out of scope. Use a direct hook in `CuiIoWin::processEvent`, gated by `#ifdef _DEBUG`.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| CPU sub-µs timer | Raw `QueryPerformanceCounter` + `QueryPerformanceFrequency` calls | `PerformanceTimer` (engine wrapper in `sharedDebug`) | Already installed at boot, already gives µs resolution, error-checking shape matches the engine. |
| File I/O for CSV | `fopen`/`fwrite`/`fclose` | `fileInterface/StdioFile` | Used by alias save/load and crash-report capture; survives `ExitChain` predictably; standard engine error shape. |
| On-screen overlay text rendering | Custom UI canvas widget or `DebugPrimitive` text or new font-rendering helper | DebugFlags report routine + `DEBUG_REPORT_PRINT` (writes to DebugMonitor child window) | One function, zero new render code. Same surface `RenderWorld::reportMetrics` already uses for DPVS visible-object counts. |
| Module-lifecycle plumbing | Constructors, destructors, global init order workarounds | `Install()`/`Remove()` static methods + `ExitChain::add(&remove, "ModuleName::Remove")` | Engine's universal pattern. ExitChain handles order, criticality, double-removal protection. |
| Frame counter | Custom counter incremented per `Game::run` iteration | `Graphics::getFrameNumber()` | Already maintained by the engine. |
| DPVS visible-object count | Iterating `RenderWorldCommander` state | `RenderWorldCommander::getNumberOfVisibleObjects()` | Already exposed [VERIFIED: `RenderWorld.cpp:1171`]; used by existing `reportMetrics`. |
| Draw-call count | Hooking into `Gl_api` draw functions | `Graphics::getRenderedVerticesPointsLinesTrianglesCalls(...)` (only in `_DEBUG` builds) | Already exists [VERIFIED: `Graphics.h:98`, `Direct3d9.cpp:2889`]. Returns `drawPrimitiveCalls + drawIndexedPrimitiveCalls` as the `calls` out-param. |
| ISO wall-clock string for CSV `wall_ms_iso` column | Custom time formatter | `time()` + `strftime` or — even simpler — `Os::getRealSystemTime()` / `Clock::getCurrentTime()` (whichever the engine already uses) | The engine already formats time strings for crash reports and log entries; reuse that path. |

**Key insight:** Phase 10 is mostly composing things the engine already has. The only genuinely new code is the D3D9 query handle pool inside the plugin (~50 lines) and the CSV-writer state machine in the new module (~80 lines). Everything else is existing wrappers + ExitChain registration.

## Runtime State Inventory

Phase 10 is NOT a rename/refactor. The Runtime State Inventory question shape (stored data / live service config / OS-registered state / secrets / build artifacts) is mostly N/A. But three items DO surface:

| Category | Items Found | Action Required |
|----------|-------------|------------------|
| Stored data | None — Phase 10 writes CSV files to disk during capture; those files are research artifacts not runtime state. Pre-existing DPVS state in saved-game files: NONE (DPVS occlusion is per-frame compute, no persisted state). | None — CSV files live under `<exe-dir>/dpvs-profile/` and are gitignored or moved into `docs/recon/10-dpvs-profiling-data/` for the verdict doc. |
| Live service config | The `disableOcclusionCulling` config key is ALREADY present in `ConfigClientGraphics.cpp` from the Koogie merge — verified at lines 38, 101, 245. **NOT new in Phase 10.** If verdict is "remove" (D-14), the key is deleted from source; any `client.cfg` files that set it become harmlessly ignored (ConfigFile silently tolerates unknown keys). | Per D-14, after verdict=remove: delete from source. No production config files exist outside this repo to update. |
| OS-registered state | None — no Task Scheduler entries, no pm2, no service registrations involve Phase 10. | None. |
| Secrets / env vars | None — no key changes, no env var changes. | None. |
| Build artifacts | The new `DpvsProfileInstrumentation.cpp` will produce `.obj` in `src/compile/win32/clientGraphics/Debug/`. The conditional `Direct3d9.cpp` edits will rebuild `Direct3d9.dll`. After D-15 cleanup, those `.obj`/`.pdb` files become stale — a clean rebuild (or `git clean -fdX src/compile/`) is sufficient. | After D-15 cleanup commit: rebuild from clean. |

**Verified by:** Reading every file under `D:/Code/swg-client-v2/src/engine/client/library/clientGraphics/` and `D:/Code/swg-client-v2/src/external/3rd/library/dpvs/`. No state persists across runs except the user's manually-issued `client_d.cfg` settings — which the user controls.

## Common Pitfalls

### Pitfall 1: GPU query data race / multi-frame latency
**What goes wrong:** Calling `GetData(...)` on the same frame the query was issued returns S_FALSE (data not ready) or stalls the CPU if D3DGETDATA_FLUSH is set. Both outcomes destroy the measurement.
**Why it happens:** The CPU is typically 1-3 frames ahead of the GPU on modern drivers. The GPU hasn't reached the timestamp yet.
**How to avoid:** Double-buffer (or triple-buffer for safety) the query set. Maintain `IDirect3DQuery9 * disjoint[N]`, `IDirect3DQuery9 * tsStart[N]`, `IDirect3DQuery9 * tsEnd[N]` (where N=3). Issue into slot `frame % N`, attempt to read from slot `(frame + 1) % N` — if S_FALSE, skip this row's gpu_us column (write empty, analysis tolerates it). [CITED: https://therealmjp.github.io/posts/profiling-in-dx11-with-queries/]
**Warning signs:** CSV rows have alternating valid/invalid gpu_us values; or all gpu_us values are zero; or frame rate drops dramatically when capture is on.

### Pitfall 2: Disjoint flag ignored
**What goes wrong:** GPU power/thermal state changes mid-measurement invalidate the tick count. Using the disjoint timestamps produces wildly wrong µs values.
**Why it happens:** Most code I've seen on the web reads `freq` from the disjoint query but skips the disjoint *flag*. The flag is the data point that says "the frequency changed between begin and end."
**How to avoid:** Always check `d.disjoint == FALSE` before consuming the tick delta. If TRUE, log a `DEBUG_WARNING` once per session and write empty gpu_us to the CSV row.
**Warning signs:** Occasional gpu_us values orders of magnitude different from the median; correlation with system events (USB power events, lid open/close on a laptop, AC unplug).

### Pitfall 3: Profiler block interference
**What goes wrong:** `ms_dpvsQueryProfilerBlock` brackets `resolveVisibility()` via `NP_PROFILER_BLOCK_ENTER`, but the *end* of that block is in `RenderWorldCommander::command()` case `QUERY_BEGIN` — far away from the resolveVisibility call site [VERIFIED: comment at RenderWorld.cpp:1043]. The Profiler's own sampling has non-trivial overhead.
**Why it happens:** The Profiler infrastructure is comprehensive (sampling, hierarchical blocks, multi-thread support). Sample cost is small but non-zero.
**How to avoid:** Treat the Profiler signal (`profiler_dpvs_us` column) as ONE of three measurements — the QPC pair (`cpu_qpc_us`) is the cleaner CPU number because it's a tight bracket. D-09 says total frame time is the verdict-deciding stat, so even if the Profiler signal is noisy, the verdict stands.
**Warning signs:** `profiler_dpvs_us` and `cpu_qpc_us` columns disagree by more than 2-3× consistently. Trust the QPC column.

### Pitfall 4: F10/F11 in capture-disabled state captured anyway
**What goes wrong:** Rows from before F10 capture turns on, or after it turns off, are appended to the CSV anyway because the per-frame writer doesn't gate on `ms_captureActive`.
**Why it happens:** Easy to forget the gate when wiring the writer.
**How to avoid:** The CSV row append in `DpvsProfileInstrumentation::onFrameEnd()` must check `ms_captureActive` as its first action and return early if false. Document this in the implementation comment.
**Warning signs:** First and last few seconds of each capture have wildly different µs values (camera moving into position).

### Pitfall 5: GPU vs CPU vs total disagree
**What goes wrong:** GPU-only numbers say "remove DPVS"; CPU-only numbers say "keep"; total says "remove". The threshold rule D-10 looks at total — but if GPU and CPU disagree with total, the analysis is harder to interpret.
**Why it happens:** Frame time is bounded by `max(CPU work, GPU work)`. If the GPU is the bottleneck, CPU savings don't reduce frame time. If the CPU is the bottleneck (likely on Mos Eisley plaza with NPC density), GPU savings don't reduce frame time either.
**How to avoid:** D-09 already locks in total frame time as the verdict-deciding stat. The CPU and GPU breakdowns are diagnostic columns that explain *why* the total moved (or didn't). Document the interpretation in the verdict doc — don't try to add it to D-10's threshold rule.
**Warning signs:** Verdict-by-CPU-alone disagrees with verdict-by-total. Use D-11 (default-to-keep on inconclusive) if total is the only one that says "remove" by a hair while the other two regress significantly.

### Pitfall 6: Mos Eisley plaza camera position drift
**What goes wrong:** Even with a single zone, camera position changes between passes 1, 2, 3. Drift biases the comparison.
**Why it happens:** D-06 explicitly chose manual capture over scripted replay. Kenny moves the mouse between passes.
**How to avoid:** Document in the capture protocol that Kenny should /loc the camera and try to return to the same coords each pass. Also: ON-OFF-ON-OFF-ON-OFF alternation (suggested in D-08) absorbs some drift because both conditions experience the same drift profile.
**Warning signs:** Median µs values within the same condition (3 passes) differ by more than 30%. Recapture if seen.

### Pitfall 7: Static CRT and the new module's heap allocations
**What goes wrong:** Per CLAUDE.md "Static CRT (`/MT`)" — the new module's `std::string` for the run-label uses CRT heap. Cross-module heap allocations are dangerous, but everything is statically linked in this build so it's fine. **However**, if the new module ever crosses the `Direct3d9.dll` boundary with a `std::string`, the two CRTs will not match.
**Why it happens:** `Direct3d9.dll` and the EXE may be built with different STL/CRT settings (post-Phase-9 they should both use MSVC STL/`/MT`, but a regression could happen).
**How to avoid:** The three new `Gl_api` entries pass POD types only — `uint32 * out_microseconds`. Never pass `std::string` across the plugin boundary. The run-label lives entirely in the engine-side module.
**Warning signs:** Mysterious heap corruption when CSV writes happen near Direct3d9.dll calls. Stack traces showing different STL implementations.

## Code Examples

Verified patterns from the v2 tree:

### Existing PIX marker forwarding (template for new GPU query forwarders)
```cpp
// Source: VERIFIED at src/engine/client/library/clientGraphics/src/win32/Gl_dll.def:226-228
// (the function pointer declarations on Gl_api)
void (*pixSetMarker)(WCHAR const * markerName);
void (*pixBeginEvent)(WCHAR const * eventName);
void (*pixEndEvent)(WCHAR const * eventName);
```

```cpp
// Source: VERIFIED at src/engine/client/application/Direct3d9/src/win32/Direct3d9.cpp:4571-4587
void Direct3d9Namespace::pixBeginEvent(WCHAR const * eventName)
{
#if (D3D_SDK_VERSION & 0x7fffffff) >= 32
    D3DPERF_BeginEvent(0xffffffff, eventName);
#else
    UNREF(eventName);
#endif
}
// + (line 1123): ms_glApi.pixBeginEvent = pixBeginEvent;
```

```cpp
// Source: VERIFIED at src/engine/client/library/clientGraphics/src/win32/Graphics.h:283-285
// (the public Graphics:: forwarder methods)
static void pixSetMarker(WCHAR const * markerName);
static void pixBeginEvent(WCHAR const * eventName);
static void pixEndEvent(WCHAR const * eventName);
```

**Phase 10 mirror these:** `dpvsGpuTimingBegin(void)`, `dpvsGpuTimingEnd(void)`, `dpvsGpuTimingPollResult(uint32 * out_microseconds, bool * out_disjointInvalid)`.

### CSV row append using StdioFile
```cpp
// Source: ADAPTED from src/game/client/library/swgClientUserInterface/src/shared/parser/SwgCuiCommandParserDefault.cpp:272-282
// (alias save pattern — write text via StdioFile)
AbstractFile * fl = new StdioFile(path.c_str(), "ab");  // "ab" = append text
if (fl->isOpen())
{
    char buf[512];
    int const len = sprintf(buf,
        "%d,%s,%s,%d,%u,%u,%u,%.3f,%d,%d\n",
        frameNo, isoWall, runLabel, dpvsOff?1:0,
        gpuUs, cpuQpcUs, profilerDpvsUs,
        totalFrameMs, visibleObjects, drawCalls);
    IGNORE_RETURN(fl->write(buf, len));
}
delete fl;  // close on every row — robust to crashes
```

For the volume of writes (~600 frames × 6 passes = 3600 rows), open-write-close per row is fine. If perf becomes an issue, hold the `StdioFile *` across frames and flush on capture-off.

### Frame number access (always available)
```cpp
// Source: VERIFIED at src/engine/client/library/clientGraphics/src/win32/Graphics.h:83
int frameNo = Graphics::getFrameNumber();
```

### DPVS visible-object count (already exposed)
```cpp
// Source: VERIFIED at src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp:1171
int const visibleObjects = RenderWorldCommander::getNumberOfVisibleObjects();
```

### Draw-call count (DEBUG builds only)
```cpp
// Source: VERIFIED at src/engine/client/library/clientGraphics/src/win32/Graphics.h:98
#ifdef _DEBUG
int verts, points, lines, tris, calls;
Graphics::getRenderedVerticesPointsLinesTrianglesCalls(verts, points, lines, tris, calls);
// `calls` is drawPrimitiveCalls + drawIndexedPrimitiveCalls per Direct3d9.cpp:2895
#endif
```

### Overlay print routine (via DebugFlags + DEBUG_REPORT_PRINT)
```cpp
// Source: ADAPTED from src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp:1167-1180
// (reportMetrics pattern)
namespace DpvsProfileInstrumentationNamespace
{
    bool ms_reportOverlay;
    void reportOverlay()
    {
        DEBUG_REPORT_PRINT(true, ("DPVS:%s run=%s %s frame=%d\n",
            RenderWorld::getDisableOcclusionCulling() ? "OFF" : "ON",
            ms_runLabel.empty() ? "(unlabeled)" : ms_runLabel.c_str(),
            ms_captureActive ? "REC" : "...",
            Graphics::getFrameNumber()));
    }
}

// In install():
DebugFlags::registerFlag(ms_reportOverlay, "ClientGraphics/Dpvs", "reportInstrumentation", &reportOverlay);
```

This prints to the DebugMonitor child window — same surface as the existing DPVS metrics report. To enable, add `[ClientGraphics/Dpvs] reportInstrumentation=true` to client.cfg.

### F10/F11 hook in CuiIoWin (debug only)
```cpp
// Source: ADAPTED from src/engine/client/library/clientUserInterface/src/shared/core/CuiIoWin.cpp:957-984
// (existing F10/F11 IME special-case + CMD_console/CMD_uiPointerToggle special-case pattern)
case IOET_KeyDown:
    AwayFromKeyBoardManager::touch();
#ifdef _DEBUG
    // Phase 10 instrumentation: F10 toggles DPVS profile capture, F11 toggles DPVS occlusion
    if (event->arg2 == DIK_F10)
    {
        DpvsProfileInstrumentation::toggleCapture();
        retval = true;
        break;
    }
    if (event->arg2 == DIK_F11)
    {
        RenderWorld::setDisableOcclusionCulling(!RenderWorld::getDisableOcclusionCulling());
        retval = true;
        break;
    }
#endif
    // ... existing keyboardInputActive logic continues unchanged ...
```

NOTE: A `RenderWorld::getDisableOcclusionCulling()` getter does not exist today — the setter does (line 1151). The plan adds the getter for the F11 toggle and (per D-14) deletes both setter and getter if verdict=remove.

### Console command branch for /setrunlabel
```cpp
// Source: ADAPTED from src/game/client/library/swgClientUserInterface/src/shared/parser/SwgCuiCommandParserDefault.cpp:1745-1762
// (afkmessage pattern — join argv[1..] into a string and pass to a setter)
else if (isCommand(argv[0], Commands::setrunlabel))
{
    std::string label;
    for (unsigned int i = 1; i < argv.size(); ++i)
    {
        if (i > 1) label += "_";
        label += Unicode::wideToNarrow(argv[i]);
    }
    DpvsProfileInstrumentation::setRunLabel(label);
    return true;
}
```

### Module install/remove skeleton
```cpp
// Source: NEW — pattern matches RenderWorld::install / RenderWorld::remove
namespace DpvsProfileInstrumentationNamespace
{
    bool ms_installed;
    bool ms_captureActive;
    std::string ms_runLabel;
    StdioFile * ms_csv;  // may be NULL between captures
    int ms_capturedFrames;
    bool ms_reportOverlay;
    void reportOverlay();
}
using namespace DpvsProfileInstrumentationNamespace;

void DpvsProfileInstrumentation::install()
{
    DEBUG_FATAL(ms_installed, ("DpvsProfileInstrumentation::install called twice"));
    ms_installed = true;
    ms_captureActive = false;
    ms_csv = NULL;
    ms_capturedFrames = 0;
    ms_reportOverlay = ConfigFile::getKeyBool("Dpvs/Experiment", "reportOverlay", true);
    DebugFlags::registerFlag(ms_reportOverlay, "ClientGraphics/Dpvs", "reportInstrumentation", &reportOverlay);
    ExitChain::add(&remove, "DpvsProfileInstrumentation::Remove");
}

void DpvsProfileInstrumentation::remove()
{
    if (ms_csv) { delete ms_csv; ms_csv = NULL; }
    DebugFlags::unregisterFlag(ms_reportOverlay);
    ms_installed = false;
}

void DpvsProfileInstrumentation::onFrameEnd(uint32 gpuUs, uint32 cpuQpcUs, uint32 profilerUs, float totalFrameMs)
{
    if (!ms_captureActive) return;
    // ... build row, append via StdioFile ...
    ++ms_capturedFrames;
}

void DpvsProfileInstrumentation::toggleCapture()
{
    ms_captureActive = !ms_captureActive;
    if (ms_captureActive) openCsv();
    else closeCsv();
}

void DpvsProfileInstrumentation::setRunLabel(std::string const & label)
{
    if (ms_csv) closeCsv();
    ms_runLabel = label;
    if (ms_captureActive) openCsv();
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| External GPU capture (PIX Legacy, Intel GPA, NVIDIA Nsight) for D3D9 | In-engine `IDirect3DQuery9` timestamps with double-buffered query pool | D3D9 era best-practice (~2007) and still the reproducible standard for D3D9 today | Locked by D-01. The external tools have patchy D3D9 support in 2026 (PIX Legacy is unmaintained; Nsight dropped pre-D3D11; GPA's D3D9 support is "best-effort" per latest release notes). In-engine queries Just Work and the data is reproducible by anyone who builds the tree. [CITED: Microsoft DirectX-Specs CountersAndQueries; therealmjp.github.io] |
| Mean frame time for fps comparisons | Median + p95 (long-tail-dominated metrics) | Modern perf-engineering practice (~2015 onward — Twitch/Riot/Valve/Insomniac perf talks) | Locked by D-04. Frame times have heavy long-tail distributions; means lie. |
| `QueryPerformanceCounter` overhead concern | Negligible on modern hardware (sub-microsecond) | Win7 onward (QPC switched to RDTSC on most CPUs) | QPC pair around resolveVisibility is essentially free — no need to batch or sample. |

**Deprecated/outdated:**
- **`timeGetTime()` for sub-ms timing:** Win32's ms-resolution timer; insufficient for resolveVisibility durations which are likely sub-ms.
- **Generic GPU stalls (`Flush()` then wait) to measure GPU time:** Synchronous GPU stall destroys frame-rate; produces a meaningless number. Use timestamp queries.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | The double-buffered query pool with N=3 is sufficient for the GPU lag on Kenny's hardware | Pitfalls / Code Examples | LOW — if N=3 isn't enough (rare; would mean GPU is >2 frames behind CPU), GetData returns S_FALSE and that row's gpu_us is dropped. Verdict still computable. Mitigation: bump to N=5 if S_FALSE rate exceeds 10%. |
| A2 | F10 and F11 are NOT used by any input map currently loaded by the SWGSource VM's TRE files | User Constraints / Anti-Patterns | LOW — verified F10/F11 are unbound in source. Risk is only in the binary input map .iff files in TRE archives. If they happen to be bound there, the `#ifdef _DEBUG` direct hook in CuiIoWin runs BEFORE the InputMap lookup and consumes the key, so existing bindings are pre-empted in debug builds. Production builds (PRODUCTION==1) compile the hook out entirely. |
| A3 | Mos Eisley plaza is consistently busy with NPCs across capture sessions on the SWGSource VM | Scene + Protocol | LOW-MEDIUM — depends on SWGSource VM NPC spawn behavior; Kenny can verify visually before each capture. If plaza is empty, scene-load was bad and pass should be re-captured. |
| A4 | The disjoint-flag-TRUE rate during a normal capture session is < 5% | Pitfalls | LOW — modern GPUs in normal desktop use rarely trigger disjoint events. Mitigation: log disjoint events; if > 5%, investigate (laptop on battery? thermal? driver instability?). |
| A5 | `Graphics::getRenderedVerticesPointsLinesTrianglesCalls` returns valid data when called per-frame at end-of-frame | Code Examples / Don't Hand-Roll | LOW — verified it returns module-static counters in `Direct3d9_Metrics`; presumably reset per frame, otherwise the data would be useless to `reportMetrics` consumers. Mitigation: if values grow monotonically, treat draw_call_count as optional and drop the column. |
| A6 | `StdioFile`'s `"ab"` mode opens for text-append and survives concurrent access (e.g., the OS antivirus opening the same file) | Code Examples | LOW — StdioFile wraps fopen. Antivirus locks on Windows can cause sporadic write failures; mitigation: ignore write failures and try again next row. |
| A7 | The CSV path `<exe-dir>/dpvs-profile/<runLabel>-<frameStart>.csv` is writable by the running client | Recommended Project Structure | LOW — the client already writes log files to `<exe-dir>/logs/`. Same directory tree, same write permissions. |
| A8 | `Game::run()` is the right place for the per-frame `onFrameEnd()` hook (vs. inside the renderer plugin) | Recommended Project Structure | LOW — `Game::run()` has clear before-present and after-present positions; the natural slot is after `Graphics::present()` returns and before the next iteration. Alternative would be inside `Graphics::present()` itself, but that's renderer-plugin-private and crosses the boundary. |
| A9 | The DebugMonitor child window is visible by default in debug builds against the SWGSource VM | On-screen overlay rendering | MEDIUM — DebugMonitor visibility is config-controlled. If Kenny's client config has it off, overlay is invisible. Need to verify in `client_d.cfg` and document the required setting. |

## Open Questions

1. **Where exactly does `onFrameEnd()` slot into `Game::run()`?**
   - What we know: `Game.cpp:1097` reads `Clock::frameTime()` (the previous frame's duration); `Game.cpp:1232` calls `Graphics::present()`.
   - What's unclear: which call gives the most accurate "total_frame_ms" for the row being written. Inserting between 1232 (present()) and 1242 (AppearanceTemplateList::update) is the cleanest position; total_frame_ms is then `elapsedTime` from line 1097.
   - Recommendation: Insert `DpvsProfileInstrumentation::onFrameEnd(elapsedTime)` just after `Graphics::present()` returns. The row is labeled with the current frame number; total_frame_ms is the duration of the just-completed frame. Document this in the implementation comment so the analysis script knows what the column means.

2. **Is the DebugMonitor child window enabled by default in `client_d.cfg`?**
   - What we know: DebugMonitor is `PRODUCTION == 0` only [VERIFIED: `DebugMonitor.h:20`]. The `client_d.cfg` in `D:/Code/swg-client-v2/stage/` does not have an explicit `[SharedDebug]` enable key.
   - What's unclear: Whether it auto-shows or requires explicit `[DebugMonitor] visible=true` (or similar key).
   - Recommendation: Plan should include a verify-step in the smoke test ("press DebugMonitor toggle or confirm it auto-shows; verify overlay text appears"). If the surface isn't visible by default, the planner adds an explicit config key to the SWG-Source VM's `client_d.cfg`. Fallback: if DebugMonitor surface is unreliable, swap to a `SwgCuiHud`-canvas text widget at the cost of ~3 more files of plumbing.

3. **Is `disableOcclusionCulling` a hot-toggleable config key, or does its effect require a `setCullingParameters` push?**
   - What we know: `RenderWorld::setDisableOcclusionCulling()` writes `ms_disableOcclusionCulling` directly [VERIFIED: line 1153]. The next-frame read at line 906 picks up the new value, but only `ms_dpvsCamera->setParameters(...)` at line 921 actually pushes the change through DPVS — and that's guarded by the `cullingParameters != ms_lastCullingParameters` check at line 914.
   - What's unclear: Whether the equality check at 914 short-circuits the push when only the *bit* changes but the camera viewport doesn't.
   - Recommendation: Read the code carefully — the equality check compares the full `cullingParameters` uint, so a bit flip DOES trigger the push. **F11 toggle IS instantaneous as D-07 claims; verified.** Plan does not need to invalidate camera state manually.

## Environment Availability

The phase has external dependencies (the SWGSource VM at 192.168.1.200:44453 must be running, with Oracle started, stationchat running, and TRE archives staged for the SWG-Source client v3.0). All inherited from Phase 9. No new environment requirements.

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| SWGSource VM | Booting client to character select then zoning to Tatooine Mos Eisley | ✓ (per Phase 9 closeout) | Stage 192.168.1.200:44453 | None — Phase 10 cannot proceed without a server. |
| DirectX 9 June 2010 SDK headers | Compiling timestamp-query code | ✓ (vendored in `src/external/3rd/library/directx9/`) | June 2010 | None needed. |
| MSVC v145 + VS 2026 | Building v2 tree | ✓ (per CLAUDE.md Build toolchain paths) | VS 2026 Community on D: | None — this is the locked v2 toolchain. |
| Discrete GPU | GPU timing data is meaningful | ✓ (Kenny's box; specific model not material to verdict if disjoint stays FALSE) | Modern (post-Maxwell NVIDIA / post-GCN AMD) | None — integrated GPU would still produce valid numbers but the verdict would be more interesting; D-12 says re-measure under D3D11 anyway. |

**Missing dependencies with no fallback:** None.
**Missing dependencies with fallback:** None.

## Validation Architecture

`workflow.nyquist_validation` is not configured in `.planning/config.json` (file doesn't exist in v2 yet). Treating as enabled.

### Test Framework

This codebase has **no automated test framework** in the traditional sense — there are no `pytest` / `gtest` / `catch2` infrastructure files. Validation is empirical: build, run the EXE, capture data, eyeball the outcome.

| Property | Value |
|----------|-------|
| Framework | None — empirical/manual validation. Builds run via MSBuild; runtime validation is via the SWGSource VM. |
| Config file | N/A |
| Quick run command | `MSBuild D:/Code/swg-client-v2/src/build/win32/swg.sln /p:Configuration=Debug /p:Platform=Win32 /p:PlatformToolset=v145` |
| Full suite command | Same — there's only the one build target sequence. After build, launch `D:/Code/swg-client-v2/stage/SwgClient_d.exe` against the SWGSource VM. |

### Phase Requirements → Test Map

For Phase 10, "validation" comes in two layers: (1) **instrumentation works** (smoke tests Kenny can run in 5 minutes) and (2) **the verdict is correct** (the offline analysis script + threshold rule).

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| DPVS-01 | Build with new instrumentation, EXE compiles and links | build-smoke | `MSBuild ... swg.sln` (above) | YES — build infra exists |
| DPVS-01 | EXE launches and reaches character select | runtime-smoke (manual) | Launch `SwgClient_d.exe`, see char select | YES — Phase 9 closeout proved this works |
| DPVS-01 | F11 toggle is bound and toggles DPVS state | runtime-smoke (manual) | Zone to Mos Eisley, press F11, watch overlay text flip between "DPVS:ON" and "DPVS:OFF" | NO — manual confirmation in overlay |
| DPVS-01 | F10 toggle is bound and turns capture on/off | runtime-smoke (manual) | Press F10, watch overlay "..." flip to "REC", verify CSV file appears under `<exe-dir>/dpvs-profile/` | NO — manual confirmation via filesystem |
| DPVS-01 | `/setrunlabel mytest` injects label into CSV column | runtime-smoke (manual) | Type the command, press F10, capture 1 second, F10 off, inspect CSV — `run_label` column reads "mytest" | NO — manual via grep over CSV |
| DPVS-01 | CSV columns contain valid (non-zero, non-NaN) data for at least 30 consecutive frames in Mos Eisley | runtime-smoke (manual) | Capture 1 second in plaza, verify gpu_us, cpu_qpc_us, profiler_dpvs_us, total_frame_ms are all sane | NO — manual inspection |
| DPVS-01 | 6 captures × ~10s × condition completed | runtime (manual; Kenny drives) | Alternate ON-OFF-ON-OFF-ON-OFF per D-08 suggestion | NO — Kenny captures |
| DPVS-02 | Offline analysis produces verdict per D-09/D-10/D-11 | offline-script | Run `python3 .planning/phases/10-.../analysis.py` (script created during execution) — outputs verdict line | NO — script Wave 0 deliverable |
| DPVS-02 | Verdict doc exists at `docs/recon/10-dpvs-profiling.md` with full stats table | doc-review (manual) | Open the file, verify it has methodology, scene, stats, verdict, Phase 11 note | NO — written during phase |
| DPVS-02 | If verdict=remove: post-edit build still compiles, EXE still boots to char select, still zones to Tatooine | runtime-smoke (manual) | Apply D-13/D-14 edits, rebuild, launch | YES — same boot path as Phase 9 closeout |
| DPVS-02 | Cleanup commit per D-15 leaves no stray Phase 10 code | grep gate | Search for `DpvsProfileInstrumentation`, `dpvsGpuTiming`, `setrunlabel`, `disableOcclusionCulling` — verify only the legitimate residue remains (DPVS lib import, view-frustum bit) | NO — grep performed manually during cleanup commit |

### Sampling Rate
- **Per task commit:** After each meaningful task (new module added; Gl_api wired; CSV writer works; overlay renders), build + launch + zone-to-Mos-Eisley smoke. ~2 minutes per check.
- **Per wave merge:** Full capture session (6 passes × 10s) → analysis script → verdict line. ~30 minutes.
- **Phase gate:** Verdict documented in `docs/recon/10-dpvs-profiling.md`; if remove → D-13/D-14 edits committed + re-boot-tested; D-15 cleanup commit applied + final re-boot-tested.

### Wave 0 Gaps

- [ ] **`docs/recon/10-dpvs-profiling.md`** — does not exist yet; created during execution. Template stub created in Wave 0 with methodology + scene + protocol headers (D-01..D-08 documented), stats and verdict filled in after captures.
- [ ] **`.planning/phases/10-dpvs-culling-experiment/scripts/analysis.py`** — does not exist yet; created in Wave 0 of execution. Reads all CSVs under `<exe-dir>/dpvs-profile/`, groups by `run_label`, computes median/p95/p99/max/stdev per condition, emits the verdict line per D-10.
- [ ] **`.planning/phases/10-dpvs-culling-experiment/test-protocol.md`** — capture protocol document Kenny reads from. Lists: open this config, zone here, type these commands in this order, expect these outcomes. Created in Wave 0.
- [ ] **DebugMonitor visibility verification** — confirm DebugMonitor child window auto-shows in debug build, or add `[DebugMonitor] visible=true` (or whatever key) to `client_d.cfg`. Smoke test in Wave 0.

*(No automated test framework exists; everything is empirical. The script and protocol docs ARE the validation infrastructure.)*

## Security Domain

`security_enforcement` is absent from config; treating as enabled. Phase 10's security surface is essentially nil — it does not handle authentication, network input, or untrusted data. The only external-facing thing is the CSV file path (which is hard-coded to a local directory).

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | — |
| V3 Session Management | no | — |
| V4 Access Control | no | — |
| V5 Input Validation | partial — `/setrunlabel <label>` accepts user input | Sanitize before writing to CSV: strip commas, newlines, control characters. Cap length at 64 chars. Otherwise an injected newline in the run-label corrupts the CSV. |
| V6 Cryptography | no | — |

### Known Threat Patterns for this phase

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| CSV injection via run-label (newlines, commas, formula-injection like `=cmd(...)`) | Tampering | Strip control chars + cap length + reject leading `=` `+` `-` `@` characters that Excel/LibreOffice interpret as formulas. **Implementation:** sanitize in `DpvsProfileInstrumentation::setRunLabel()` before storing. |
| File-path traversal via CSV filename | Tampering | The filename incorporates the run-label. Sanitize: replace `/`, `\`, `..`, `:` in the run-label before using as a path component. Or use a UUID + log the mapping. |
| Disk-fill via runaway capture | DoS | Cap captured frames per file at 100,000 (≈ 30 min at 60 fps). Auto-close + start a new file on rollover. Print a warning on overlay when capping. |

These are nice-to-haves — the threat actor is "Kenny accidentally typing a comma," not a malicious user. Plan should include the sanitizer but the priority is LOW.

## Sources

### Primary (HIGH confidence)
- **VERIFIED via direct codebase reads** at `D:/Code/swg-client-v2/src/...`:
  - `RenderWorld.cpp:78,98,255,903-908,1037-1047,1144-1154,1167-1180` — DPVS bitmask, ProfilerBlock, setter, getter (absent), reportMetrics
  - `RenderWorld.h:104` — setDisableOcclusionCulling declaration
  - `ConfigClientGraphics.cpp:30-122,243-246` — config key plumbing
  - `Direct3d9.cpp:131,1006,1122-1124,2889-2896,4560-4587` — Gl_api PIX pattern + draw-call exposure
  - `Direct3d9.h:45-46` — IDirect3DDevice9* private accessor
  - `Gl_dll.def:76-241` — Gl_api function-pointer table (119 entries)
  - `Graphics.h:83,92-99,283-285` — engine-side renderer surface
  - `PerformanceTimer.cpp:21-89` — QPC wrapper
  - `Report.h:14-71, Report.cpp` — DEBUG_REPORT_PRINT pipeline
  - `DebugFlags.h:17-69` — flag registration + report-routine pattern
  - `DebugMonitor.h:20-43` — child window debug overlay (PRODUCTION==0 only)
  - `DebugPrimitive.h:28-79` — 3D-world debug primitive base class (NOT used by Phase 10)
  - `ExitChain.h:28-79` — module teardown registration
  - `CuiIoWin.cpp:957-1010` — IOET_KeyDown intercept point (CMD_console special-case as template)
  - `IMEManager.cpp:959-990` — existing F10/F11 special-case (precedent for direct hook)
  - `InputMap.cpp:1597-1622,1463-1477` — addCustomCommand + executeCommandByName
  - `CuiActionManager.cpp:266-277` — addAction pattern (not used by Phase 10)
  - `CuiAction.h:14-37` — base class shape
  - `SwgCuiCommandParserDefault.cpp:87-205,212-282,789-797,1745-1762` — command parser pattern
  - `Game.cpp:382,712,1097,1198,1232,1242` — frame loop, scene-id, frame time, present
  - `Clock.cpp` — Clock::frameTime existence verified
  - `stage/client_d.cfg` — current runtime config
- **CITED Microsoft official docs:**
  - https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dquerytype — D3DQUERYTYPE enumeration including TIMESTAMP, TIMESTAMPDISJOINT
  - https://learn.microsoft.com/en-us/windows/win32/direct3d9/queries — canonical CreateQuery/Issue/GetData example
  - https://github.com/MicrosoftDocs/win32/blob/docs/desktop-src/direct3d9/d3dquerytype.md — TIMESTAMPDISJOINT semantics (frequency, disjoint flag)
  - https://microsoft.github.io/DirectX-Specs/d3d/CountersAndQueries.html — counter/query spec

### Secondary (MEDIUM confidence)
- **CITED community references** (cross-verified with Microsoft docs):
  - https://therealmjp.github.io/posts/profiling-in-dx11-with-queries/ — MJP's profiling pattern (D3D11 but structurally identical to D3D9); double-buffered query pool design
  - https://mynameismjp.wordpress.com/2011/10/13/profiling-in-dx11-with-queries/ — older version of same article
  - https://www.reedbeta.com/blog/gpu-profiling-101/ — Nathan Reed's general GPU profiling primer (independent verification of the double-buffer pattern)

### Tertiary (LOW confidence)
- None — every claim in this RESEARCH.md is either VERIFIED by direct codebase reading or CITED to a primary/secondary source above.

## Metadata

**Confidence breakdown:**
- **Standard stack:** HIGH — every component (DirectX 9, QPC, ProfilerBlock, Report, DebugFlags, DebugMonitor, StdioFile, ExitChain) verified by direct file reads in v2 tree.
- **Architecture (module structure, plugin boundary handling):** HIGH — the pixSetMarker/Begin/End template is exactly the pattern Phase 10 needs; the codebase has it as living precedent.
- **Pitfalls (D3D9 query latency, disjoint flag):** HIGH — cited to MJP, Microsoft, and Nathan Reed; cross-verified across three independent sources.
- **Console-command registration (`/setrunlabel`):** HIGH — afkmessage is a near-perfect template.
- **F10/F11 keybind seam:** MEDIUM-HIGH — verified F10/F11 are unbound in source; verified IMEManager already special-cases them; CuiIoWin intercept-before-InputMap pattern is precedent. Slight risk: TRE-archived input maps might bind them and would be pre-empted by the `#ifdef _DEBUG` hook, which is intended.
- **CSV path choice (`<exe-dir>/dpvs-profile/`):** MEDIUM — assumption A7. Mitigation: if permission-denied, fall back to `<temp-dir>/swg-dpvs-profile/`.
- **DebugMonitor auto-visible:** MEDIUM — open question Q2.

**Research date:** 2026-05-10
**Valid until:** 2026-06-09 (30 days — engine code is stable; the only currency risk is upstream Koogie modifications to Direct3d9.cpp or CuiIoWin.cpp during the same time window, which would be small and easy to rebase against).
