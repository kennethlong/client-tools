# DPVS Occlusion Culling — Profiling Verdict (Phase 10)

**Date measured:** TBD (filled in after capture session)
**Tree:** `D:/Code/swg-client-v2/` branch `koogie-msvc-cpp20-base`
**Renderer:** D3D9 (per D-12; Phase 11 D3D11 re-measure separate)
**Scene:** Mos Eisley cantina plaza (per D-05)
**Hardware:** TBD (filled in — Kenny's machine GPU model + driver version)

## Methodology

*Filled in from decisions D-01..D-04 of `.planning/phases/10-dpvs-culling-experiment/10-CONTEXT.md`.*

- **GPU timing:** D3D9 `IDirect3DQuery9` timestamp queries in-engine (D-01).
  Double-buffered query pool (N=3) per RESEARCH.md Pitfall 1.
- **CPU timing:** `QueryPerformanceCounter` pair around `resolveVisibility()`
  plus the existing `ms_dpvsQueryProfilerBlock` (D-02).
- **Output:** Per-frame CSV row + on-screen overlay (D-03).
- **Statistics:** Median + p95 + p99 + max + stdev per condition × metric
  (D-04). Means NOT primary.

## Scenes & Protocol

*Filled in from decisions D-05..D-08.*

- Single scene: Mos Eisley cantina plaza (D-05).
- Keybind-toggle capture (D-06): F10 toggles capture; F11 toggles DPVS;
  `/setrunlabel` injects run-label column.
- 3 passes per condition, ~10 s each, alternating ON-OFF-ON-OFF-ON-OFF (D-08).
- Full protocol: `tools/dpvs-profile/test-protocol.md`.

## Raw CSV Manifest

*Populated after capture. List relative paths:*

| Pass | Condition | File | Frames |
|------|-----------|------|--------|
| 1 | DPVS:ON  | `dpvs-profile/mosEisley-pass1-on-<frameStart>.csv`  | TBD |
| 1 | DPVS:OFF | `dpvs-profile/mosEisley-pass1-off-<frameStart>.csv` | TBD |
| 2 | DPVS:ON  | `...` | TBD |
| 2 | DPVS:OFF | `...` | TBD |
| 3 | DPVS:ON  | `...` | TBD |
| 3 | DPVS:OFF | `...` | TBD |

## Summary Statistics

*Paste markdown table output from `python tools/dpvs-profile/analysis.py`.*

| condition | metric            | median | p95 | p99 | max | stdev | n_frames |
|-----------|-------------------|--------|-----|-----|-----|-------|----------|
| on  | total_frame_ms    | TBD | TBD | TBD | TBD | TBD | TBD |
| off | total_frame_ms    | TBD | TBD | TBD | TBD | TBD | TBD |
| on  | gpu_us            | TBD | TBD | TBD | TBD | TBD | TBD |
| off | gpu_us            | TBD | TBD | TBD | TBD | TBD | TBD |
| on  | cpu_qpc_us        | TBD | TBD | TBD | TBD | TBD | TBD |
| off | cpu_qpc_us        | TBD | TBD | TBD | TBD | TBD | TBD |
| on  | profiler_dpvs_us  | TBD | TBD | TBD | TBD | TBD | TBD |
| off | profiler_dpvs_us  | TBD | TBD | TBD | TBD | TBD | TBD |

## Verdict

**Verdict:** TBD (`remove` or `keep`)

## Rationale

*Per D-09 (primary metric = total_frame_ms median + p95), D-10 (threshold
rule), D-11 (default to keep on inconclusive):*

*Fill in the verdict reasoning:*
- if off.total_median ≤ on.total_median AND off.total_p95 ≤ on.total_p95 →
  **remove** (DPVS occlusion culling is net cost in this scene under this
  renderer).
- else → **keep** (status quo wins per D-11).

## Phase 11 Revisit Note (per D-12)

This verdict is valid for the D3D9 renderer **only**. Phase 11's D3D11
plugin changes draw-call cost; DPVS occlusion savings depend on draw-call
cost; the verdict can flip under D3D11.

**Action for Phase 11:** Phase 11's success-criteria list must gain an
item to re-measure DPVS occlusion under D3D11 via the same protocol
(`tools/dpvs-profile/test-protocol.md`), and confirm or reverse this
decision before declaring D3D11 done.

## References

- `.planning/phases/10-dpvs-culling-experiment/10-CONTEXT.md` (user decisions D-01..D-16)
- `.planning/phases/10-dpvs-culling-experiment/10-RESEARCH.md` (research record)
- `.planning/phases/10-dpvs-culling-experiment/10-SUMMARY.md` (phase closeout — thin)
- `tools/dpvs-profile/test-protocol.md` (capture procedure)
- `tools/dpvs-profile/analysis.py` (verdict-line emitter)
