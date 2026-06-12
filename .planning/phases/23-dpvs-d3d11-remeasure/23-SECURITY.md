---
phase: 23
slug: dpvs-d3d11-remeasure
status: secured
threats_open: 0
asvs_level: 1
created: 2026-06-12
---

# Phase 23 — Security

> Per-phase security contract: threat register, accepted risks, and audit trail.

---

## Trust Boundaries

| Boundary | Description | Data Crossing |
|----------|-------------|---------------|
| dev console → CSV file | `/setrunlabel` user string flows into a CSV filename + column later read by analysis.py | user-typed run label (dev-local) |
| _DEBUG build → shipped Release | restored instrumentation (occlusion re-gate, F10/F11, accessors, QPC bracket) must not alter shipped Option α behavior | none (compile-time fence) |

---

## Threat Register

| Threat ID | Category | Component | Disposition | Mitigation | Status |
|-----------|----------|-----------|-------------|------------|--------|
| T-23-01 | Tampering | /setrunlabel → run_label CSV column | mitigate | sanitizeRunLabel choke-point in setRunLabel (DpvsProfileInstrumentation.cpp:217-263) | closed |
| T-23-02 | Elevation of Privilege | _DEBUG instrumentation vs shipped Release | mitigate | #ifdef _DEBUG fencing at every site; #else Option α branch byte-identical (RenderWorld.cpp:912-916) | closed |

*Status: open · closed*

---

## Accepted Risks Log

No accepted risks.

---

## Security Audit Trail

| Audit Date | Threats Total | Closed | Open | Run By |
|------------|---------------|--------|------|--------|
| 2026-06-12 | 2 | 2 | 0 | gsd-security-auditor (claude-sonnet-4-6) |

---


---

## Threat Verification

| Threat ID | Category | Disposition | Status | Evidence |
|-----------|----------|-------------|--------|----------|
| T-23-01 | Tampering | mitigate | CLOSED | See detail below |
| T-23-02 | Elevation of Privilege | mitigate | CLOSED | See detail below |

---

## T-23-01: CLOSED — Run-label sanitizer present and enforced at all entry points

**Declared mitigation:** Run-label sanitizer in `DpvsProfileInstrumentation::setRunLabel` strips
path/format/control chars, maps `..`→`__`, prepends `_` to leading `= + - @` (formula guard),
truncates to 64 chars, empty→`unlabeled`. Sanitizer runs before the label reaches any CSV
filename or column. `analysis.py` consumes with stdlib `csv`.

**Verification findings:**

1. **`sanitizeRunLabel()` function body** — confirmed present in
   `DpvsProfileInstrumentation.cpp` lines 217–263. All declared transformations verified in
   source:
   - Path separators and special chars `/ \ : * ? " < > | ,` → `_` (switch case, lines 225-228)
   - CR, LF, TAB → `_` (lines 229-230)
   - Any byte `< 0x20` → `_` (lines 231-233)
   - `..` substring → `__` (while-loop, lines 240-247)
   - Leading `= + - @` → prepend `_` (lines 251-256)
   - Truncate to 64 chars (lines 258-260)
   - Empty → `"unlabeled"` (lines 262-263)

2. **`setRunLabel()` calls `sanitizeRunLabel()` before any storage** — confirmed at
   `DpvsProfileInstrumentation.cpp` lines 147-157: sanitized copy is computed first, then
   `ms_runLabel = sanitized`.

3. **`/setrunlabel` command routes through `setRunLabel()`** — confirmed in
   `SwgCuiCommandParserDefault.cpp` line 1792: `DpvsProfileInstrumentation::setRunLabel(label)`
   is the only action taken on the user-supplied string. No pre-sanitization in the command
   handler; the sanitizer runs entirely inside `setRunLabel`.

4. **No raw user string reaches the CSV filename or column** — `openCsv()` uses
   `ms_runLabel` (already sanitized) in the `snprintf` path. `writeRow()` uses
   `ms_runLabel.c_str()` directly as the CSV column value (already sanitized at storage time).

5. **GPU-forwarder calls absent** — `dpvsGpuTimingPollResult` appears 3 times in
   `DpvsProfileInstrumentation.cpp`, all in comments. Zero live calls. (Confirms Plan-B
   GPU-strip is complete — no unintended side channel through the timing API.)

6. **CSV header matches `analysis.py` EXPECTED_HEADER** — header string in `openCsv()`
   (lines 307-308) is byte-for-byte:
   `frame_no,wall_ms_iso,run_label,dpvs_occlusion_flag,gpu_us,cpu_qpc_us,profiler_dpvs_us,total_frame_ms,visible_object_count,draw_call_count`
   This matches the 10-column schema declared in the plan interfaces block.

**Result: CLOSED.** The sanitizer is present, runs at the single choke-point before storage,
and the stored value is what reaches both the filename and the CSV column. No bypass path found.

---

## T-23-02: CLOSED — _DEBUG instrumentation does not alter shipped Release

**Declared mitigation:** All of: the OCCLUSION_CULLING re-gate in `cullingParameters`, both new
accessors `getForceDisableOcclusionCulling` / `toggleForceDisableOcclusionCulling`, the QPC
bracket, and the CuiIoWin F10/F11 keybind block are inside `#ifdef _DEBUG`. The shipped `#else`
Option-alpha branch is byte-for-byte unchanged. `/setrunlabel` exists in Release but is harmless
without the `_DEBUG` capture path and is sanitizer-guarded.

**Verification findings:**

1. **OCCLUSION_CULLING re-gate is `#ifdef _DEBUG` only** — `RenderWorld.cpp` lines 903-916
   confirm the `cullingParameters` computation ORing in `OCCLUSION_CULLING` gated on
   `ms_forceDisableOcclusionCulling` is inside the `#ifdef _DEBUG` arm. The `#else` arm
   (lines 912-916) reads exactly:
   ```
   uint const cullingParameters = DPVS::Camera::VIEWFRUSTUM_CULLING;
   portalRecusionDepth = 8;
   ```
   This is the shipped Option-alpha branch, byte-for-byte unchanged.

2. **Both new accessor declarations are `#ifdef _DEBUG`** — `RenderWorld.h` lines 106-113
   confirm `getForceDisableOcclusionCulling()` and `toggleForceDisableOcclusionCulling()` are
   declared inside an `#ifdef _DEBUG` block. The Release build will not see these symbols.

3. **Both new accessor definitions are `#ifdef _DEBUG`** — `RenderWorld.cpp` lines 1186-1222
   place both definitions inside a single `#ifdef _DEBUG` / `#endif` block. Confirmed by
   reading the closing `#endif` at line 1222.

4. **QPC bracket is outside `#ifdef _DEBUG` for the timing record, but uses a `_DEBUG` guard
   for the visible-object count** — `RenderWorld.cpp` lines 1054-1074: the
   `PerformanceTimer` start/stop and `recordCpuQpcUs` call are unconditional (no `_DEBUG`
   guard). This is intentional and documented in Plan 01 deviation #2: `recordCpuQpcUs`
   is always-on (captures CPU timing in Release too, harmless — the value is cached but
   never written to CSV unless capture is active; the capture path itself is `_DEBUG`-only
   because `toggleCapture()` is only reachable via the F10 keybind which is in the
   `#ifdef _DEBUG` block). The `recordVisibleObjectCount` call uses an explicit
   `#ifdef _DEBUG` / `#else 0` guard (lines 1067-1073). No elevation risk: Release cannot
   open a CSV because `toggleCapture()` is unreachable without the F10/F11 keybind, and
   the capture-active flag starts false.

5. **F10/F11 keybind block is `#ifdef _DEBUG`** — `CuiIoWin.cpp` lines 960-982 confirm the
   entire F10/F11 intercept block is inside `#ifdef _DEBUG`. The deleted-symbol assertion also
   holds: `getDisableOcclusionCulling` has 0 hits in `CuiIoWin.cpp` (confirmed by grep —
   the rewired F11 calls `toggleForceDisableOcclusionCulling()` exclusively).

6. **`/setrunlabel` command is always-on (not `_DEBUG`-gated)** — confirmed in
   `SwgCuiCommandParserDefault.cpp` lines 1784-1794. This is the declared design: the command
   is harmless in Release because the capture path (`ms_captureActive`) can only be set to
   true via `toggleCapture()`, which is only reachable through the `_DEBUG`-gated F10 keybind.
   In Release, `setRunLabel` updates `ms_runLabel` but no CSV is ever opened. The sanitizer
   still runs on any Release invocations.

**Result: CLOSED.** The shipped `#else` Option-alpha branch is byte-for-byte unchanged. All
debug-only instrumentation surface is `#ifdef _DEBUG` gated. The one unconditional call
(`recordCpuQpcUs`) cannot produce observable Release behavior change because the capture
activation gate (`toggleCapture`) is `_DEBUG`-only.

---

## Unregistered Threat Flags

The SUMMARY files for Plans 01-03 contain no `## Threat Flags` section declaring new attack
surface beyond the two registered threats. No unregistered flags to log.

### Plan 01 deviations (informational, not new threats)
- `#ifdef` inside macro argument list (C2121 build error, fixed by hoisting to a variable) —
  a compilation correctness issue, not a security surface.
- QPC bracket called `_DEBUG`-only `RenderWorldCommander::getNumberOfVisibleObjects()` in
  Release (C3861 build error, fixed by `#ifdef _DEBUG` guard) — closes a potential
  compilation gap; not a threat escalation.

### Plan 02 deviations (informational, not new threats)
- Multi-library MSBuild stale-object rebuild issue — a build-process correctness concern;
  no security implication.

---

## Accepted Risks

None declared in the threat register. No `accept` dispositions exist for this phase.

---

## Transfer Documentation

None declared in the threat register. No `transfer` dispositions exist for this phase.

---

## Notes

- The `gpu_us` CSV column is structurally zero (DPVS is CPU-side software occlusion). This is
  intentional and documented — not a data integrity gap.
- The `profiler_dpvs_us` and `draw_call_count` columns are zero under D3D11 (profiler counter
  not fed; `getRenderedVerticesPointsLinesTrianglesCalls` returns 0 under gl11). Documented in
  23-03-SUMMARY.md. Neither column drives the `total_frame_ms`-based verdict rule.
- The audit scope covers Plans 01-03 (the full phase). Plan 03 adds no new code (docs + protocol
  only); T-23-02 explicitly covers Plan 03's non-modification of the Option-alpha branch, which
  was verified against `RenderWorld.cpp`.
