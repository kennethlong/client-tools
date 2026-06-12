# Phase 23: DPVS D3D11 Remeasure - Pattern Map

**Mapped:** 2026-06-12
**Files analyzed:** 9 (6 restore-from-tag, 3 surviving/new)
**Analogs found:** 9 / 9 (all have an exact pre-cleanup self-analog or a live in-repo analog)

> **Phase shape:** ~80% restore-from-git-tag, ~15% one source adaptation, ~5% run-protocol-and-write-doc.
> For every RESTORED file the canonical analog is **its own pre-cleanup version** at git tag
> `phase-10-instrumentation-pre-cleanup` (`9f2ec3715`). Extract via
> `git show phase-10-instrumentation-pre-cleanup:<path>`. The cleanup commit `151167d2c` is the
> exact restore manifest (a clean, revert-shaped diff per file).
>
> **THE load-bearing adaptation (do not miss it):** the pre-cleanup instrumentation .cpp calls
> `RenderWorld::getDisableOcclusionCulling()` (in `writeRow` and `reportOverlay`) and the deleted
> F11 hook called `RenderWorld::setDisableOcclusionCulling()`. **Both getter and setter were DELETED
> by Option α (D-13/D-14) and do NOT exist anywhere in the tree** (verified: 0 grep hits). A verbatim
> restore will NOT compile. The restore must re-point these at the surviving
> `ms_forceDisableOcclusionCulling` DebugFlag (RenderWorld.cpp:81 / :227) — see Shared Pattern A.

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|-------------------|------|-----------|----------------|---------------|
| `clientGraphics/include/public/clientGraphics/DpvsProfileInstrumentation.h` | instrumentation (header) | file-I/O (CSV) | self @ tag | exact (restore) |
| `clientGraphics/src/shared/DpvsProfileInstrumentation.cpp` | instrumentation (writer+overlay) | file-I/O (CSV) | self @ tag | exact (restore, strip GPU) |
| `clientGraphics/src/shared/RenderWorld.cpp` | engine / culling driver | transform (cull) + measure | self @ tag (diff) + current Option α state | exact (adapt) |
| `clientGraphics/build/win32/clientGraphics.vcxproj` | build config | — | self @ tag (diff) | exact (restore 2 lines) |
| `clientGraphics/src/win32/SetupClientGraphics.cpp` | install hook | — | self @ tag (diff) | exact (restore) |
| `clientGame/src/shared/core/Game.cpp` | frame-loop hook | event-driven (onFrameEnd) | self @ tag (diff) | exact (restore) |
| `clientUserInterface/src/shared/core/CuiIoWin.cpp` | UI keybind intercept | event-driven (keydown) | self @ tag (diff) | exact (restore + rewire F11) |
| `swgClientUserInterface/.../parser/SwgCuiCommandParserDefault.cpp` | console command | request-response | self @ tag (diff) | exact (restore verbatim) |
| `docs/recon/23-dpvs-d3d11-profiling.md` | verdict doc | — | `docs/recon/10-dpvs-profiling.md` | role-match (NEW) |

**Surviving / unchanged (NO restore — reuse as-is):**
- `tools/dpvs-profile/analysis.py` — aggregator + verdict emitter (`EXPECTED_HEADER` at line 35, `_compute_verdict` at 235). VERIFIED present.
- `tools/dpvs-profile/test-protocol.md` — capture session checklist. VERIFIED present.

**DO NOT restore (Direct3d9 GPU path — wrong renderer + measured structural zeros):**
`Direct3d9.cpp` dpvsGpuTiming pool, `Graphics.{h,cpp}` forwarders, `Gl_dll.def` entries. The pre-cleanup
.cpp's `Graphics::dpvsGpuTimingPollResult(...)` call in `onFrameEnd` and the
`Graphics::dpvsGpuTimingBegin/End` bracket in RenderWorld must be **stubbed out** on restore (push `gpuUs=0`,
`disjointInvalid=false`). See Pattern 2.

---

## Pattern Assignments

### `DpvsProfileInstrumentation.h` (instrumentation header, restore)

**Analog:** self @ `phase-10-instrumentation-pre-cleanup:.../DpvsProfileInstrumentation.h`

All-static class, no instances (mirrors `Graphics.h`). Public surface to restore verbatim:
```cpp
class DpvsProfileInstrumentation
{
public:
	static void install();
	static void remove();
	static void onFrameEnd(float totalFrameMs);   // from Game::runGameLoopOnce after present()
	static void toggleCapture();                  // from CuiIoWin F10 (_DEBUG)
	static void setRunLabel(std::string const & label);  // from /setrunlabel
	static void recordCpuQpcUs(uint32 us);
	static void recordProfilerUs(uint32 us);
	static void recordVisibleObjectCount(int count);
	static bool getCaptureActive();
	static char const * getRunLabel();
	static int getCapturedFrameCount();
private:
	DpvsProfileInstrumentation();   // all-statics; no instances
};
```
**Adaptation:** none required for the header (it has no occlusion-getter reference). Restore as-is.

---

### `DpvsProfileInstrumentation.cpp` (CSV writer + overlay, restore + strip GPU)

**Analog:** self @ tag `.../src/shared/DpvsProfileInstrumentation.cpp` (~471 lines).

**Install pattern** (config read + DebugFlag register + ExitChain teardown — all surviving infra):
```cpp
void DpvsProfileInstrumentation::install()
{
	DEBUG_FATAL(ms_installed, ("DpvsProfileInstrumentation::install called twice"));
	ms_installed = true; ms_captureActive = false; /* ...zero the rest... */
	char const * const csvDir = ConfigFile::getKeyString("Dpvs/Experiment", "csvDir", "dpvs-profile/");
	ms_csvDir = (csvDir != NULL) ? csvDir : "dpvs-profile/";
	ms_reportOverlay = ConfigFile::getKeyBool("ClientGraphics/Dpvs", "reportInstrumentation", true);
	DebugFlags::registerFlag(ms_reportOverlay, "ClientGraphics/Dpvs", "reportInstrumentation", reportOverlay);
	ExitChain::add(&DpvsProfileInstrumentation::remove, "DpvsProfileInstrumentation::Remove");
}
```
> Note: `[ClientGraphics/Dpvs] reportInstrumentation=true` already lingers in `stage/client_d.cfg:124-131`
> from Phase 10 — the restored writer consumes it directly.

**CSV header — MUST match `analysis.py` EXPECTED_HEADER exactly** (10 columns):
```cpp
char const * const header =
	"frame_no,wall_ms_iso,run_label,dpvs_occlusion_flag,gpu_us,cpu_qpc_us,"
	"profiler_dpvs_us,total_frame_ms,visible_object_count,draw_call_count\n";
```

**Run-label sanitizer** (RESTORE VERBATIM — this is the V5/CSV-injection control, ASVS L1):
strips `/ \ : * ? " < > | ,` + CR/LF/TAB + bytes <0x20 → `_`; `".."`→`"__"`; leading `= + - @` → prepend `_`;
truncate 64; empty→`"unlabeled"`. (Full body in the tag file; do not re-derive.)

**>>> GPU-strip adaptation (Pattern 2):** the pre-cleanup `onFrameEnd` calls
`Graphics::dpvsGpuTimingPollResult(&gpuUs, &disjointInvalid)` — that forwarder is NOT being restored.
Replace with constants:
```cpp
void DpvsProfileInstrumentation::onFrameEnd(float totalFrameMs)
{
	if (!ms_installed) return;
	uint32 gpuUs = 0; bool disjointInvalid = false;   // was Graphics::dpvsGpuTimingPollResult(...) — DROPPED (gpu_us≡0)
	if (!ms_captureActive) return;
	writeRow(gpuUs, disjointInvalid, totalFrameMs);
}
```

**>>> Occlusion-flag adaptation (Shared Pattern A):** `writeRow` and `reportOverlay` both call the
DELETED `RenderWorld::getDisableOcclusionCulling()`:
```cpp
// pre-cleanup (WILL NOT COMPILE — getter deleted by Option α):
int const dpvsOff = RenderWorld::getDisableOcclusionCulling() ? 1 : 0;     // in writeRow()
RenderWorld::getDisableOcclusionCulling() ? "OFF" : "ON",                  // in reportOverlay()
```
Re-point at the surviving flag via a new tiny accessor on RenderWorld (see Shared Pattern A) or read it
through a restored getter. The `dpvs_occlusion_flag` column is authoritative over `run_label` (Pitfall 4),
so it MUST reflect the real flag state.

---

### `RenderWorld.cpp` (culling driver + QPC pair — the critical adaptation)

**Analog:** current Option α state (lines 78-94 flags, 223-234 DebugFlag reg, 901-924 culling param,
1038-1051 resolveVisibility) + the pre-cleanup diff in `151167d2c`.

**Current Option α state (occlusion bit STRIPPED) — RenderWorld.cpp:901-910:**
```cpp
#ifdef _DEBUG
	// Phase 10 D-13: OCCLUSION_CULLING bit dropped (Option α).
	uint const cullingParameters = (ms_disableViewFrustumCulling ? 0 : DPVS::Camera::VIEWFRUSTUM_CULLING);
	portalRecusionDepth = ms_disablePortalTraversal ? 0 : 8;
#else
	uint const cullingParameters = DPVS::Camera::VIEWFRUSTUM_CULLING;   // shipped Option α — DO NOT TOUCH
	portalRecusionDepth = 8;
#endif
```

**Phase 23 re-introduction (gate occlusion on the surviving flag, stay inside `_DEBUG`):**
```cpp
#ifdef _DEBUG
	uint const cullingParameters =
		  (ms_forceDisableOcclusionCulling ? 0 : DPVS::Camera::OCCLUSION_CULLING)
		| (ms_disableViewFrustumCulling    ? 0 : DPVS::Camera::VIEWFRUSTUM_CULLING);
	portalRecusionDepth = ms_disablePortalTraversal ? 0 : 8;
#else
	uint const cullingParameters = DPVS::Camera::VIEWFRUSTUM_CULLING;   // shipped Option α — UNCHANGED
	portalRecusionDepth = 8;
#endif
```
> `ms_forceDisableOcclusionCulling` survives at RenderWorld.cpp:81, registered as DebugFlag
> `[ClientGraphics/Dpvs] disableOcclusionCulling` at :227 — currently **dead** (nothing reads it).
> This re-wire is the single most important adaptation; without it there is no A/B toggle.

**QPC bracket around resolveVisibility — restore from the pre-cleanup diff, DROP the GPU half.**
Pre-cleanup wrapped `RenderWorld.cpp:~1042-1050` (current resolveVisibility site) with:
```cpp
// pre-cleanup (RESTORE the CPU half; DROP the two Graphics::dpvsGpuTiming* calls):
//   Graphics::dpvsGpuTimingBegin();                    // <-- DROP (GPU path not restored)
	PerformanceTimer dpvsCpuTimer;
	dpvsCpuTimer.start();
	{
		NP_PROFILER_AUTO_BLOCK_DEFINE("resolveVisibility");
		NP_PROFILER_BLOCK_ENTER(ms_dpvsQueryProfilerBlock);
		ms_dpvsCamera->resolveVisibility(ms_commander, portalRecusionDepth, 0.0f);
	}
	dpvsCpuTimer.stop();
//   Graphics::dpvsGpuTimingEnd();                       // <-- DROP
	DpvsProfileInstrumentation::recordCpuQpcUs(static_cast<uint32>(dpvsCpuTimer.getElapsedTime() * 1e6f));
	DpvsProfileInstrumentation::recordVisibleObjectCount(RenderWorldCommander::getNumberOfVisibleObjects());
	DpvsProfileInstrumentation::recordProfilerUs(0u);
```
Restore the two dropped includes too (the cleanup removed them):
```cpp
#include "clientGraphics/DpvsProfileInstrumentation.h"
#include "sharedDebug/PerformanceTimer.h"
```
`ms_dpvsQueryProfilerBlock` (RenderWorld.cpp:97) already brackets the call today — no new profiler infra.

---

### `clientGraphics.vcxproj` (build config, restore 2 lines)

**Analog:** self @ tag (diff in `151167d2c`). Re-add alphabetically next to `DebugPrimitive`:
```xml
<ClCompile Include="..\..\src\shared\DpvsProfileInstrumentation.cpp">
  <Optimization Condition="'$(Configuration)|$(Platform)'=='Optimized|Win32'">MaxSpeed</Optimization>
</ClCompile>
...
<ClInclude Include="..\..\include\public\clientGraphics\DpvsProfileInstrumentation.h" />
```

---

### `SetupClientGraphics.cpp` (install hook, restore)

**Analog:** self @ tag. Re-add include + the install call immediately after `RenderWorld::install();`:
```cpp
#include "clientGraphics/DpvsProfileInstrumentation.h"
...
	RenderWorld::install();
	DpvsProfileInstrumentation::install();   // CSV writer + overlay DebugFlag + ExitChain teardown
```
> ExitChain::add() inside install() registers teardown automatically — no matching ::remove() call here.

---

### `Game.cpp` (frame-loop hook, restore)

**Analog:** self @ tag (`Game::runGameLoopOnce` diff). Re-add include + the onFrameEnd call right after
the `Graphics::present()` block:
```cpp
		// elapsedTime (top of loop) is the PREVIOUS frame's duration in seconds → ms for the CSV row.
		DpvsProfileInstrumentation::onFrameEnd(elapsedTime * 1000.0f);
```
Renderer-agnostic — fires identically under gl11. (Original site was Game.cpp:~1235.)

---

### `CuiIoWin.cpp` (keybind intercept, restore + rewire F11)

**Analog:** self @ tag (`CuiIoWin::processEvent` IOET_KeyDown `#ifdef _DEBUG` block).

**F10 restore (verbatim):**
```cpp
#ifdef _DEBUG
	if (event->arg2 == DIK_F10)
	{
		DpvsProfileInstrumentation::toggleCapture();
		retval = true;
		break;
	}
#endif
```
**F11 REWIRE (the pre-cleanup F11 was commented-out + called the deleted setter):** re-enable it pointed
at the surviving flag (Shared Pattern A):
```cpp
	if (event->arg2 == DIK_F11)
	{
		RenderWorld::toggleForceDisableOcclusionCulling();   // NEW accessor flipping ms_forceDisableOcclusionCulling
		retval = true;
		break;
	}
```
Restore the `_DEBUG` includes the cleanup removed: `clientGraphics/DpvsProfileInstrumentation.h` +
`clientGraphics/RenderWorld.h`. Hook runs BEFORE InputMap so it preempts any TRE input-map binding.

---

### `SwgCuiCommandParserDefault.cpp` (`/setrunlabel`, restore verbatim)

**Analog:** self @ tag. Three coordinated edits (the deleted setter is NOT involved here — safe to restore as-is):
1. `MAKE_COMMAND (setrunlabel);` in the command-declare block (~line 130).
2. `cmds[]` table entry: `{ Commands::setrunlabel, 0, "<label>", "...set the DPVS run-label..." }`.
3. `performParsing` else-if branch — joins `argv[1..]` with `_`, calls `DpvsProfileInstrumentation::setRunLabel(label)`:
```cpp
else if (isCommand(argv[0], Commands::setrunlabel))
{
	std::string label;
	for (unsigned int i = 1; i < argv.size(); ++i)
	{
		if (i > 1) label += "_";
		label += Unicode::wideToNarrow(argv[i]);
	}
	DpvsProfileInstrumentation::setRunLabel(label);   // sanitizer runs inside setRunLabel
	return true;
}
```
Re-add include `clientGraphics/DpvsProfileInstrumentation.h`.

---

### `docs/recon/23-dpvs-d3d11-profiling.md` (verdict doc, NEW)

**Analog:** `docs/recon/10-dpvs-profiling.md` (sibling format). Mirror its structure: methodology, per-scene
summary tables (median + p95 per condition), `§"Note on gpu_us = 0"`, Option α confirm/revise statement,
and a cross-link back to the Phase 10 doc. The final `verdict = remove|keep` line comes from
`analysis.py` stdout.

---

## Shared Patterns

### Pattern A: Occlusion A/B toggle re-gated on the surviving DebugFlag (THE cross-cutting adaptation)
**Source:** `RenderWorld.cpp:81` (flag decl), `:227` (DebugFlag registration) — both SURVIVE.
**Apply to:** RenderWorld.cpp (culling bitmask), CuiIoWin.cpp (F11), DpvsProfileInstrumentation.cpp
(`writeRow` + `reportOverlay`).
**Why:** Option α (D-13/D-14) deleted `getDisableOcclusionCulling()` / `setDisableOcclusionCulling()` /
the `ms_disableOcclusionCulling` static / the `disableOcclusionCulling` KEY_BOOL config — **all gone**
(0 grep hits, verified). Every pre-cleanup reference to that getter/setter is now a broken symbol.
**Cleanest seam:** add two thin accessors on RenderWorld that operate on the surviving
`ms_forceDisableOcclusionCulling`, e.g.:
```cpp
// RenderWorld.h / .cpp (NEW, _DEBUG-guarded — replaces the deleted get/setDisableOcclusionCulling pair):
#ifdef _DEBUG
	static bool getForceDisableOcclusionCulling();          // reads ms_forceDisableOcclusionCulling
	static void toggleForceDisableOcclusionCulling();       // flips it (F11 target)
#endif
```
Then: writeRow/overlay read `getForceDisableOcclusionCulling()`; F11 calls
`toggleForceDisableOcclusionCulling()`; culling bitmask gates on the raw flag. Keep ALL of it inside
`_DEBUG` so the shipped `#else` Option α branch is byte-for-byte unchanged.

### Pattern B: Drop the Direct3d9 GPU-timing path (structural zeros)
**Source:** Phase 10 verdict doc §"Note on gpu_us = 0" — DPVS `resolveVisibility()` is CPU-side software
occlusion (Umbra walks in-memory structures, issues no draw calls); 12,016/12,016 frames read gpu_us=0.
**Apply to:** DpvsProfileInstrumentation.cpp (`onFrameEnd` poll call), RenderWorld.cpp
(`dpvsGpuTimingBegin/End` bracket).
**How:** replace each `Graphics::dpvsGpuTiming*` call with a `gpuUs=0; disjointInvalid=false;` constant.
Do NOT restore the `Direct3d9.cpp` pool / `Graphics.{h,cpp}` forwarders / `Gl_dll.def` entries —
they are the wrong renderer and a known shared-header ABI cascade surface (project memory). The
`gpu_us` CSV column stays in the header (analysis.py expects 10 columns) but is always 0.

### Pattern C: `_DEBUG`-gated debug surface (ASVS V5 + Elevation mitigation)
**Source:** existing `RenderWorld.cpp` DebugFlag block (`#ifdef _DEBUG`, :223-234) + the pre-cleanup
keybind/command blocks.
**Apply to:** F10/F11 intercept (CuiIoWin), the occlusion re-gate (RenderWorld), the new RenderWorld
accessors. Keep `/setrunlabel` + its sanitizer always-on (it's a console command, sanitizer is the control).
**Why:** keeps the debug toggle out of shipped Release Option α behavior; the run-label sanitizer (V5)
neutralizes path-traversal / CSV-formula / filename injection on the one user-string input.

### Pattern D: CSV schema lock to analysis.py
**Source:** `tools/dpvs-profile/analysis.py` `EXPECTED_HEADER` (line 35); verdict logic
`_compute_verdict` (line 235, emits `verdict = remove|keep`, default-keep per D-11).
**Apply to:** DpvsProfileInstrumentation.cpp `openCsv` header + `writeRow` column order.
**Why:** analysis.py hard-rejects on header mismatch (line 91). The 10-column order is fixed; restore it byte-identical.

---

## No Analog Found

None. Every file has either an exact pre-cleanup self-analog (restore) or a live in-repo analog
(RenderWorld Option α state, `docs/recon/10-dpvs-profiling.md`). The only genuinely *new* artifact is
the verdict doc, which has a direct sibling template.

---

## Metadata

**Analog search scope:**
- git tag `phase-10-instrumentation-pre-cleanup` (`9f2ec3715`) — all 6 restore-target files
- cleanup commit `151167d2c` — the exact per-file restore manifest (diffs read)
- live `src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp` (current Option α state)
- `tools/dpvs-profile/{analysis.py,test-protocol.md}` (surviving, unchanged)

**Files scanned:** 9 target files + 2 surviving tools + cleanup-commit diff
**Verified facts:** `get/setDisableOcclusionCulling` = 0 grep hits (deleted, confirmed);
`ms_forceDisableOcclusionCulling` survives at :81/:227 but is dead;
`reportInstrumentation=true` lingers in `stage/client_d.cfg`; analysis.py 10-column header intact.
**Pattern extraction date:** 2026-06-12
