# Phase 24: DPVS Config-Gate + Machine Portability - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-06-12
**Phase:** 24-DPVS Config-Gate + Machine Portability
**Areas discussed:** DPVS knob design, client_d.cfg cleanup, Path portability mechanism, Miles redist handling

---

## Todo Cross-Reference

| Match | Decision |
|-------|----------|
| `2026-06-12-config-gate-dpvs-occlusion-per-phase-23-verdict.md` (score 0.6, `resolves_phase: 24`) | ✓ Folded — its proposed design becomes the starting point |
| TRE diff / D3DCompile / corner-snap / Options-FATAL todos | Reviewed, not folded — belong to Phases 25–28 |

---

## DPVS knob design

| Option | Description | Selected |
|--------|-------------|----------|
| Lock as proposed | auto\|on\|off, default auto, F11 wins | |
| Default off, knob available | Conservative; verdict acted-on by explicit config | |
| Default on outdoors after Release re-measure | Adds Release-safe CSV scope | |
| *Other (free text)* | "Add the knobs, default off, add a new task to measure under Release build; adjust to auto then if it makes sense" | ✓ |

**User's choice:** Knobs added with default `off`; Release measurement as follow-up task.
**Notes:** Debug-build measurement caveat from the verdict doc drove the conservative default.

| Option | Description | Selected |
|--------|-------------|----------|
| Lightweight A/B via the new knob | Config on/off + external frame-time capture | |
| Release-safe CSV variant | Port CSV writer to Release (todo's optional item 4) | |
| Defer measurement to Phase 26 | Measure when instrumentation fate is decided | ✓ |

**User's choice:** Defer to Phase 26 — pairs with instrumentation removal (measure with the harness, strip it, then decide the default flip).

---

## client_d.cfg cleanup

| Option | Description | Selected |
|--------|-------------|----------|
| Audit every key, verify risky ones | Per-key verdict; risky removals boot-tested | ✓ |
| Rewrite minimal cfg from scratch | Clean canonical cfg, riskier | |
| Remove only the obviously-dead | Conservative strip | |

**User's choice:** Full audit with per-key verdicts.
**Notes:** `runtimeDisableAsynchronousLoader` flagged as the canonical risky removal (re-enables loader thread, untested since Phase 19 fixes landed elsewhere).

| Option | Description | Selected |
|--------|-------------|----------|
| 11 (D3D11) | Modernized parity renderer as default | ✓ |
| 5 (D3D9) | Known-good reference as safe default | |
| You decide | Claude picks during planning | |

**User's choice:** rasterMajor=11 canonical.

| Option | Description | Selected |
|--------|-------------|----------|
| Allow small engine default flips | Fix defaults in code so permanent keys disappear | ✓ |
| Cfg-file-only | Zero code risk, keys stay | |
| You decide per key | Mixed | |

**User's choice:** Engine default flips allowed (multi-stream VBs, Bloom slot), each boot-verified.

---

## Path portability mechanism

| Option | Description | Selected |
|--------|-------------|----------|
| Tracked base + gitignored local cfg | Include-based layering | |
| Documented edit-in-place | SETUP.md + loud comments | |
| Setup script generates cfg | PowerShell template generation (CMake configure_file successor) | ✓ |

**User's choice:** Setup script generates the cfgs from a template.

| Option | Description | Selected |
|--------|-------------|----------|
| Template tracked, generated cfg untracked | Source of truth = template + script | ✓ |
| Both tracked, script refreshes | Paths keep leaking into commits | |
| You decide | | |

**User's choice:** Template tracked; client_d.cfg + client.cfg gitignored.

| Option | Description | Selected |
|--------|-------------|----------|
| Real fresh-clone test | Clone to new dir, script + build, boot to char select | ✓ |
| Audit + simulate | Grep audit without full reclone | |

**User's choice:** Real fresh-clone test as the PORT-01 gate.

---

## Miles redist handling

| Option | Description | Selected |
|--------|-------------|----------|
| Setup script copies from TRE root | No binaries in git | |
| Commit redist to git | Self-contained clone | ✓ (initial) |
| Document only | Manual copy + warn | |

**User's choice (initial):** Commit to git.
**Notes:** User asked to verify the miles binaries ship with the SWGSource client before committing "anything new". Verified: `stage/miles` is byte-identical to `D:/Code/SWGSource Client v3.0/miles/`, AND the repo already tracks 9 of the 12 files at `src/external/3rd/library/miles-7.2e/redist/`. This prompted a refinement question:

| Option | Description | Selected |
|--------|-------------|----------|
| Vendored redist + postbuild copy | Add 2 missing .m3d files to the tracked redist; copy redist → stage/miles | ✓ |
| Commit stage/miles directly | Two copies of 9 binaries in git | |
| You decide | | |

**User's choice (final):** Vendored redist + postbuild copy.

| Option | Description | Selected |
|--------|-------------|----------|
| Script + engine runtime check | Setup-time validation + one clear startup warning | ✓ |
| Script-only | Silent again if deleted later | |
| Engine-only | No setup-time validation | |

**User's choice:** Both layers of detection.

---

## Claude's Discretion

- Config key naming/section for the DPVS knob
- Auto-mode detection mechanics (per-frame `ms_cameraCell` check vs cell notification)
- Setup script UX, template format, gitignore arrangement
- Per-key cfg audit verdicts beyond those explicitly decided

## Deferred Ideas

- Phase 26 scope addition: Release-build DPVS A/B measurement (new knob + D-15 harness before strip), then decide the default flip from `off` to `auto`
