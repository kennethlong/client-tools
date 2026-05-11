---
phase: 10
slug: dpvs-culling-experiment
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-05-10
---

# Phase 10 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.
>
> **Note:** Phase 10 is a profiling experiment, not a traditional code-delivery phase. "Tests" here are (a) build + boot smoke checks that the instrumentation didn't break the client, and (b) the empirical capture protocol that produces the verdict. The capture session itself is the validation of the phase goal. See RESEARCH.md §Validation Architecture for full detail.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | None — manual capture protocol + Python analysis script |
| **Config file** | none — Wave 0 creates `tools/dpvs-profile/analysis.py` |
| **Quick run command** | `msbuild src/build/win32/swg.sln /p:Configuration=Debug /p:Platform=Win32 /p:PlatformToolset=v145` (build smoke) |
| **Full suite command** | Manual: build → boot client → run 6 captures (3× DPVS-on, 3× DPVS-off in Mos Eisley plaza per D-08) → run analysis.py → verdict line emitted |
| **Estimated runtime** | Build ~3–5 min; capture session ~10 min wall; analysis ~10 sec |

---

## Sampling Rate

- **After every task commit:** Debug-build smoke (must compile)
- **After every plan wave:** Boot client to char-select; if instrumentation wave: zone-in to Mos Eisley plaza
- **Before `/gsd-verify-work`:** Full 6-capture protocol completes; `docs/recon/10-dpvs-profiling.md` is populated with verdict
- **Max feedback latency:** Build smoke ~5 min; full capture protocol ~15 min wall

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| TBD — populated by gsd-planner | | | DPVS-01 / DPVS-02 | — | N/A (debug-only instrumentation) | smoke + manual capture | `msbuild ...` + boot | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

*The planner populates this table per-task once plans are written.*

---

## Wave 0 Requirements

- [ ] `tools/dpvs-profile/analysis.py` — Python script reading CSVs, grouping by run-label, emitting median/p95/p99/max/stdev per condition × metric and the verdict line per D-10
- [ ] `tools/dpvs-profile/test-protocol.md` — Kenny-facing capture procedure: launch sequence, F10/F11 usage, /setrunlabel example, expected 6-capture session order
- [ ] `docs/recon/10-dpvs-profiling.md` — Stub verdict doc (skeleton sections: Methodology, Scenes & Protocol, Raw CSV manifest, Summary Statistics, Verdict, Phase 11 revisit note per D-12)
- [ ] Baseline build smoke — confirm current v2 tree at `koogie-msvc-cpp20-base` still builds and boots before instrumentation lands

*Wave 0 lands before Wave 1 (instrumentation). The planner enumerates the exact files.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| 6-capture session in Mos Eisley plaza | DPVS-01 | Per D-06: keybind-toggle capture, no scripted camera replay; capture timing/density is human judgment | Launch client → log in → travel to Mos Eisley cantina plaza → `/setrunlabel mosEisley-pass1-on` → F10 (start capture) → ~10 sec → F10 (stop) → F11 (toggle DPVS off) → `/setrunlabel mosEisley-pass1-off` → repeat for 3 passes per side, alternating |
| Verdict written to `docs/recon/10-dpvs-profiling.md` | DPVS-01 / Success #2 | Verdict is a written engineering judgment, not a programmatic check | Run `python tools/dpvs-profile/analysis.py` → copy summary table + verdict line into the doc → fill methodology/scene/Phase 11 revisit sections |
| F11 runtime toggle does NOT crash | Success #1 | Crash detection is runtime-empirical | Toggle F11 repeatedly during gameplay; client must stay up |
| Client boots and renders after D-13 + D-14 (if verdict=remove) | Success #3 | End-to-end render verification | Build → boot → zone into Mos Eisley → confirm world renders, no FATAL |

*All instrumentation-build-smoke checks are automated (msbuild succeeds); all capture/verdict steps are manual by design (D-06).*

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify (build smoke) OR explicit manual-procedure reference
- [ ] Sampling continuity: build smoke after every task; full capture before verdict
- [ ] Wave 0 covers analysis.py + protocol doc + verdict-doc skeleton
- [ ] No watch-mode flags (none applicable in C++ project)
- [ ] Feedback latency < 10 min for build smoke; < 15 min wall for capture session
- [ ] `nyquist_compliant: true` set in frontmatter once planner fills per-task map

**Approval:** pending
