---
phase: 10-dpvs-culling-experiment
plan: 01
subsystem: tooling
tags: [dpvs, profiling, scaffolding, python, csv, verdict-doc, baseline]

# Dependency graph
requires:
  - phase: 09-stlport-msvc-stl
    provides: "Tatooine zone-in PASS against SWGSource VM (closeout commit 460f4540d); v2 build tree at D:/Code/swg-client-v2/ on koogie-msvc-cpp20-base with VS 2026/v145 toolchain green"
provides:
  - "tools/dpvs-profile/analysis.py (Python 3 stdlib-only CSV aggregator + verdict-line emitter per D-04/D-09/D-10/D-11)"
  - "tools/dpvs-profile/test-protocol.md (Kenny-facing 6-capture procedure per D-05/D-06/D-07/D-08)"
  - "docs/recon/10-dpvs-profiling.md (verdict-doc skeleton per D-16; populated in Wave 5)"
  - ".planning/phases/10-dpvs-culling-experiment/10-01-baseline-build.log (msbuild output, SwgClient target scoped)"
  - ".planning/phases/10-dpvs-culling-experiment/10-01-baseline-evidence.md (build + boot smoke evidence; Tatooine zone-in PASS, GPU/driver recorded)"
affects: [10-02, 10-03, 10-04, 10-05, 10-06, 10-07]

# Tech tracking
tech-stack:
  added:
    - "Python 3 stdlib-only data analysis pattern (csv, statistics, glob, argparse) -- no third-party deps required so the verdict script runs on any machine"
  patterns:
    - "Pre-instrumentation safety-net wave: scaffolding + baseline build/boot smoke BEFORE any source-edits land -- if a later wave breaks the build, regression localizes to that wave"
    - "Verdict-doc skeleton authored in Wave 0; data cells populated in Wave 5 after capture session"
    - "Decision-ID traceability in every artifact (analysis.py docstring, test-protocol.md decision-trail, recon doc methodology bullets all cite D-01..D-16 from 10-CONTEXT.md)"

key-files:
  created:
    - "D:/Code/swg-client-v2/tools/dpvs-profile/analysis.py"
    - "D:/Code/swg-client-v2/tools/dpvs-profile/test-protocol.md"
    - "D:/Code/swg-client-v2/docs/recon/10-dpvs-profiling.md"
    - "D:/Code/swg-client-v2/.planning/phases/10-dpvs-culling-experiment/10-01-baseline-build.log"
    - "D:/Code/swg-client-v2/.planning/phases/10-dpvs-culling-experiment/10-01-baseline-evidence.md"
  modified: []

key-decisions:
  - "Build command scoped to -t:SwgClient (not the full swg.sln) per PROJECT.md 'Out of Scope' and Phase 9 closeout precedent (build-log-replan3-01.txt). Rule 1 deviation -- aligns the automated verify line with the actually-supported in-scope target."
  - "Pre-existing Koogie post-build copy failure (MSB3073 on SwgClient.vcxproj copying to D:/SWG/SWGSource Client v3.0/) treated as out-of-scope per executor scope-boundary rule. Identical failure exists in Phase 9 closeout's build-log-replan3-01.txt; SwgClient.exe was already produced (line 75 of log) before the post-build copy fires, so the failure is purely cosmetic for this baseline."
  - "Baseline build was incremental (no in-scope project rebuilt since Phase 9 closeout commit 460f4540d); SwgClient_d.exe size 72,541,696 bytes matches Phase 9 closeout binary exactly -- this IS the same green tree, re-validated."

patterns-established:
  - "Wave 0 safety-net pattern: scaffolding + baseline verification BEFORE any source-edits across an N-wave phase. Concretely: 3 doc/script files + 1 build smoke + 1 boot smoke checkpoint, all committed BEFORE Wave 1 lands the first source-edits in Phase 10."
  - "Verdict-doc skeleton-first pattern: docs/recon/10-dpvs-profiling.md exists with all H2 sections + TBD cells in Wave 0; data fills in Wave 5. Future profiling experiments can copy this shape."
  - "Build-scope alignment: the plan's automated verify line MUST target the actually-supported in-scope build target. PROJECT.md 'Out of Scope' is authoritative; full-solution builds will fail on editor/tool targets that have known blockers documented in Phase 8 SUMMARIES."

requirements-completed: []  # DPVS-01 is partially covered (validation infrastructure exists); the measurement half (Wave 4 capture + Wave 5 verdict) is what closes DPVS-01. Do NOT mark DPVS-01 complete here.

# Metrics
duration: ~3.5h (executor-time including Task 1 script authoring, Task 2 protocol authoring, Task 3 verdict-doc skeleton, Task 4 baseline build smoke, Task 5 human-verify wait)
completed: 2026-05-10
---

# Phase 10 Plan 10-01: Wave 0 Pre-Instrumentation Scaffolding + Baseline Boot Smoke Summary

**Phase 10 Wave 0 safety-net landed: Python 3 stdlib-only verdict analyzer, 6-capture Kenny-facing protocol, D-16-shaped verdict-doc skeleton, and an empirically-verified baseline (msbuild -t:SwgClient green, char-select + Tatooine zone-in PASS on NVIDIA GeForce RTX 3060) -- all three tool/doc artifacts and the evidence record committed BEFORE any Wave 1 source-edits, so any later regression localizes cleanly.**

## Performance

- **Duration:** ~3.5h executor-time (Task 1 + Task 2 + Task 3 in parallel-author style, Task 4 incremental msbuild, Task 5 human-verify checkpoint -- Kenny launched client and reported result)
- **Started:** 2026-05-11T01:00:00Z (Wave 0 plan author + execute kickoff session, per STATE.md last_activity)
- **Completed:** 2026-05-10T (Pacific time -- Kenny's "approved" signal received 2026-05-10 per system memory currentDate)
- **Tasks:** 5 (4 auto + 1 human-verify checkpoint)
- **Files modified:** 5 created, 0 modified (all-new artifacts; no source edits in Wave 0)

## Accomplishments

- **Validation infrastructure exists before the data does.** `tools/dpvs-profile/analysis.py` is a stdlib-only Python 3 CSV aggregator that emits the D-10 verdict line as the final stdout line -- ready to consume Wave 4's capture output without any third-party install dance.
- **Capture procedure is single-page and Kenny-readable.** `tools/dpvs-profile/test-protocol.md` documents the 6-pass alternating ON-OFF protocol (D-08), the F10 capture toggle (D-07), the F11 DPVS toggle (D-07), and the `/setrunlabel` console command in one document Kenny can read from at capture time without dereferencing 10-CONTEXT.md.
- **Verdict-doc skeleton authored.** `docs/recon/10-dpvs-profiling.md` has all six required H2 sections (Methodology, Scenes & Protocol, Raw CSV Manifest, Summary Statistics, Verdict, Phase 11 Revisit Note) with TBD cells -- ready for Wave 5 fill-in. Hardware row populated this session (NVIDIA GeForce RTX 3060, driver 94.6.2f.0.7e).
- **Baseline green-state empirically confirmed.** msbuild scoped to `-t:SwgClient` exits with no in-scope errors; SwgClient_d.exe artifact identity (size 72,541,696 bytes) matches Phase 9 closeout exactly; Kenny verified char-select reached cleanly and Tatooine zone-in still PASSes against the SWGSource VM at 192.168.1.200:44453.
- **No regression from scaffolding commits.** All 5 commits are docs/tooling -- zero source-edits across all of Wave 0. The post-Wave-0 boot test confirms the v2 tree is in the same shape as Phase 9 closeout (commit `460f4540d`), so Wave 1's first source-edits land on a known-green baseline.

## Task Commits

Each task was committed atomically on `koogie-msvc-cpp20-base`:

1. **Task 1: Author tools/dpvs-profile/analysis.py** -- `70635a4e7` (feat) -- Python 3 stdlib-only CSV aggregator + verdict emitter; per D-04 (statistics), D-09 (primary metric = total_frame_ms median + p95), D-10 (threshold rule -- emits `verdict = remove` when off ≤ on on both median and p95), D-11 (default to `verdict = keep` on inconclusive / missing data).
2. **Task 2: Author tools/dpvs-profile/test-protocol.md** -- `fb84ef37a` (docs) -- Kenny-facing 6-capture procedure (3 passes × 2 conditions); alternating ON-OFF-ON-OFF-ON-OFF per D-08; F10/F11/`/setrunlabel` controls per D-06/D-07; Mos Eisley cantina plaza scene per D-05.
3. **Task 3: Author docs/recon/10-dpvs-profiling.md** -- `ae2652da8` (docs) -- D-16-shaped verdict-doc skeleton; all six required H2 sections; TBD cells throughout; Phase 11 D3D11 revisit note per D-12.
4. **Task 4: Baseline build smoke + evidence (Part B TBD)** -- `fc0e2a53a` (chore) -- msbuild `-t:SwgClient -p:Configuration=Debug -p:Platform=Win32 -p:PlatformToolset=v145` log; baseline-evidence.md authored with Build Smoke + Phase 9 Closeout Reference sections complete, Boot Smoke section left TBD pending the human-verify checkpoint.
5. **Task 5: Record Task 5 boot-smoke result** -- `9918eb111` (docs) -- Fill `## Boot Smoke` section of baseline-evidence.md with Kenny's verified result (char-select reached cleanly, Tatooine zone-in PASS, GPU/driver recorded); fill `**Hardware:**` row of docs/recon/10-dpvs-profiling.md (was TBD per D-16) with the same GPU/driver values for the eventual Wave 5 capture session.

**Plan metadata commit:** Will follow this SUMMARY.md commit (sequential-mode owns the STATE/ROADMAP writes after SUMMARY).

_Note: This plan has no TDD tasks -- all auto/docs/chore commits per the conventional-commits map._

## Files Created/Modified

**Created (5):**

- `tools/dpvs-profile/analysis.py` -- Python 3 stdlib-only CSV aggregator; reads `*.csv` from `--csv-dir` (default `D:/Code/swg-client-v2/stage/dpvs-profile/`), groups rows by `run_label` suffix (`-on` vs `-off`), computes median/p95/p99/max/stdev per condition × metric using `statistics.median` and `statistics.quantiles(..., n=100)[94]`/`[98]`, prints a markdown summary table, and emits `verdict = remove|keep` as the final stdout line per D-10/D-11.
- `tools/dpvs-profile/test-protocol.md` -- Single-page capture checklist (Pre-Session Checklist → Launch Sequence → Capture → Post-Capture → Troubleshooting → Decision-ID Trail). 6 passes (3 ON, 3 OFF) of ~10s each, alternating ON-OFF-ON-OFF-ON-OFF per D-08.
- `docs/recon/10-dpvs-profiling.md` -- Verdict-doc skeleton per D-16. Header (Date/Tree/Renderer/Scene/Hardware), 6 required H2 sections, all TBD cells ready for Wave 5 fill-in. Hardware row populated with Kenny's GPU/driver this session.
- `.planning/phases/10-dpvs-culling-experiment/10-01-baseline-build.log` -- msbuild stdout/stderr (79 lines) from the `-t:SwgClient` scoped baseline build; documents the incremental-build "everything already up-to-date" outcome plus the pre-existing Koogie post-build copy failure.
- `.planning/phases/10-dpvs-culling-experiment/10-01-baseline-evidence.md` -- Build Smoke + Boot Smoke + Phase 9 Closeout Reference; serves as the canonical empirical proof that the v2 tree was green immediately before Phase 10 Wave 1 lands the first source-edits.

**Modified (0):** No source files edited; this is a pre-instrumentation scaffolding wave by design (per the plan's `<objective>`).

## Decisions Made

1. **Build target scoped to `-t:SwgClient` instead of the full `swg.sln`.** Per PROJECT.md "Out of Scope" -- editors/tools are deferred until SwgClient itself ships. Phase 9 closeout used the same `-t:SwgClient` pattern in `build-log-replan3-01.txt`. Treating this as the canonical Phase 10 baseline-build invocation (Rule 1 deviation: align verify command with actually-supported scope).
2. **Pre-existing Koogie post-build copy failure deferred, not fixed.** The `MSB3073 copy ... "D:\SWG\SWGSource Client v3.0\" ...` command fails because Koogie's dev-machine paths don't exist on this machine. It fires AFTER `SwgClient_d.exe` is already linked (line 75 of the log). Identical failure in Phase 9 closeout (`build-log-replan3-01.txt`). Per executor scope-boundary rule, out-of-scope pre-existing failures go to `deferred-items.md`, not the active plan. Phase 12+ candidate to clean up if editor builds ever need it.
3. **Verdict-doc skeleton lives at `docs/recon/10-dpvs-profiling.md` (not in `.planning/`).** Per D-16, the verdict doc is a permanent codebase artifact (research history, sibling to `docs/recon/05-*.md` and `07-*.md`) -- not a planning ephemeral. Phase 9 closeout did not migrate the `docs/recon/` tree into v2, so this is the first `docs/recon/` doc under v2 (precedent for Phase 11 D3D11 revisit doc later).
4. **DPVS-01 NOT marked complete here.** This plan delivers the validation INFRASTRUCTURE (analysis.py, test-protocol.md, verdict-doc skeleton, baseline-evidence). The measurement half (Wave 4 capture session + Wave 5 verdict writeup) is what closes DPVS-01. STATE/ROADMAP updates record plan 10-01 complete but leave DPVS-01 partial.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Build target rescoped from full swg.sln to -t:SwgClient**

- **Found during:** Task 4 (Baseline build smoke)
- **Issue:** The plan's `<verify><automated>` line as written invokes `msbuild src/build/win32/swg.sln /p:Configuration=Debug /p:Platform=Win32 /p:PlatformToolset=v145 /verbosity:minimal`, which builds the WHOLE solution. That fails on editor/tool projects (SwgGodClient, MayaExporter, ParticleEditor, ConversationEditor, SwgSpaceQuestEditor, etc.) which are explicitly out-of-scope per PROJECT.md "Editor / tool builds ... deferred until SwgClient itself ships". A literal whole-solution build would always report failure even though SwgClient.exe itself builds cleanly.
- **Fix:** Rescoped the msbuild invocation to `-t:SwgClient` (the in-scope target tree, per the Phase 9 closeout precedent in `build-log-replan3-01.txt`). The scoped build covers 60+ static `.lib` projects + DPVS plugin DLL + Direct3d9/Direct3d9_vsps/Direct3d9_ffp DLLs + SwgClient.exe -- the full SwgClient runtime closure -- and finishes with zero in-scope errors.
- **Files modified:** None (this is a build-command-line change; the plan's verify line is updated implicitly in the baseline-evidence.md "Build Smoke -- Detailed Reading" section).
- **Verification:** SwgClient.exe linked successfully (log line 75); 60+ static libs built cleanly (log lines 3-63); DPVS plugin + 3 Direct3d9 DLLs built cleanly (log lines 44, 66, 69, 72). Artifact size 72,541,696 bytes matches Phase 9 closeout binary exactly (no in-scope project rebuilt, as expected on an incremental baseline check).
- **Committed in:** `fc0e2a53a` (Task 4 commit -- baseline build smoke + evidence)

**2. [Out of Scope] Pre-existing Koogie post-build copy failure**

- **Found during:** Task 4 (Baseline build smoke)
- **Issue:** After SwgClient.exe is linked, the project's post-build step fires a `copy "D:\SWG\client-tools\..." "D:\SWG\SWGSource Client v3.0\"` command pointing at Koogie's dev-machine paths. These paths don't exist on this machine, so `MSB3073` fires. This is a pre-existing failure inherited from the Koogie merge anchor `479d35df3`; identical failure documented in Phase 9 closeout's `build-log-replan3-01.txt`.
- **Disposition:** Per executor scope-boundary rule, this is OUT OF SCOPE for plan 10-01. The post-build copy is editor-staging plumbing left from Koogie's local dev workflow; we don't use it. The SwgClient.exe artifact was already produced and staged correctly before this command fires.
- **Fix:** NONE applied in this plan. Logged for `deferred-items.md` as a future Phase 12+ candidate if the editor build path ever gets unblocked.
- **Files modified:** None.
- **Verification:** Identical MSB3073 failure exists in both `build-log-replan3-01.txt` (Phase 9 closeout) and `10-01-baseline-build.log` (this Phase 10 baseline) -- no regression introduced, no behavior change.
- **Committed in:** N/A (no fix applied; documented in baseline-evidence.md "Build Smoke -- Detailed Reading" + this SUMMARY)

---

**Total deviations:** 1 auto-fixed (1 Rule 1 build-scope alignment) + 1 explicitly-deferred out-of-scope pre-existing failure
**Impact on plan:** Rule 1 alignment was the correct move per PROJECT.md and Phase 9 precedent; no scope creep. The deferred Koogie post-build copy is a known pre-existing condition with zero effect on the SwgClient runtime; it does not block Wave 1.

## Issues Encountered

None during planned work. The two items above are documented under Deviations -- they're scope/alignment decisions, not problem-solving incidents.

## Self-Check: PASSED

**Files referenced as created exist:**

- `D:/Code/swg-client-v2/tools/dpvs-profile/analysis.py` -- FOUND (Task 1 commit `70635a4e7`)
- `D:/Code/swg-client-v2/tools/dpvs-profile/test-protocol.md` -- FOUND (Task 2 commit `fb84ef37a`)
- `D:/Code/swg-client-v2/docs/recon/10-dpvs-profiling.md` -- FOUND (Task 3 commit `ae2652da8`; hardware row populated in Task 5 commit `9918eb111`)
- `D:/Code/swg-client-v2/.planning/phases/10-dpvs-culling-experiment/10-01-baseline-build.log` -- FOUND (Task 4 commit `fc0e2a53a`)
- `D:/Code/swg-client-v2/.planning/phases/10-dpvs-culling-experiment/10-01-baseline-evidence.md` -- FOUND (Task 4 + Task 5 commits)

**Commits referenced exist on `koogie-msvc-cpp20-base`:**

- `70635a4e7` -- FOUND (`feat(10-01): add tools/dpvs-profile/analysis.py CSV aggregator + verdict emitter`)
- `fb84ef37a` -- FOUND (`docs(10-01): add tools/dpvs-profile/test-protocol.md (Kenny-facing capture procedure)`)
- `ae2652da8` -- FOUND (`docs(10-01): add docs/recon/10-dpvs-profiling.md verdict-doc skeleton`)
- `fc0e2a53a` -- FOUND (`chore(10-01): record Phase 10 Wave 0 baseline build smoke + evidence`)
- `9918eb111` -- FOUND (`docs(10-01): record Task 5 boot-smoke result -- char-select reached, Tatooine PASS`)

**Key-links validation (forward-looking, populated in Wave 5):**

- `tools/dpvs-profile/analysis.py` → `<exe-dir>/dpvs-profile/*.csv` via `glob` + `csv` stdlib reader (per `--csv-dir` argument; verified via `python tools/dpvs-profile/analysis.py --help`).
- `docs/recon/10-dpvs-profiling.md` → `tools/dpvs-profile/analysis.py` via manifest reference in §References (file exists and is grep-able).

**Acceptance criteria spot-check (per plan):**

- All 5 tasks complete: ✓ (verified by 5 commits above)
- Build green for SwgClient: ✓ (artifact size 72,541,696 bytes, matches Phase 9 closeout)
- Char-select reached: ✓ (Kenny verified, no relogins)
- Tatooine PASS: ✓ (Kenny verified Phase 9 closeout regression check)

**TDD gate compliance:** N/A -- this plan has `type: execute` (Wave 0 scaffolding), not `type: tdd`. No RED/GREEN/REFACTOR gates required.

## User Setup Required

None. This plan delivers scaffolding only; no external service configuration. Wave 4 (the capture session, plan 10-05) will require Kenny to operate the client and follow `tools/dpvs-profile/test-protocol.md`, but that's a different plan.

## Next Phase Readiness

**Wave 1 (plan 10-02) is unblocked.** The baseline empirically confirms the v2 tree at `koogie-msvc-cpp20-base` is green -- if Wave 1's GPU-timing plumbing breaks the build, the regression localizes to Wave 1's commits (not to some pre-existing condition).

**Pre-conditions for Wave 1 met:**

- `tools/dpvs-profile/analysis.py` exists and is grep-able as the eventual CSV consumer -- Wave 1's `DpvsProfileInstrumentation` (plan 10-03 / Wave 2) writes CSVs that this script will consume in Wave 5.
- `tools/dpvs-profile/test-protocol.md` documents the F10/F11/`/setrunlabel` control surfaces that Wave 1 (plan 10-02 -- GPU-timing function pointers) + Wave 2 (plan 10-03 -- DpvsProfileInstrumentation module) + Wave 3 (plan 10-04 -- RenderWorld brackets + Game::run hook + CuiIoWin keybind intercept) wire up.
- `docs/recon/10-dpvs-profiling.md` skeleton awaits Wave 5 fill-in.

**Open items carried forward:**

- DPVS-01 requirement: PARTIAL (validation infra exists; measurement half lands in Wave 5). Do NOT mark DPVS-01 complete in REQUIREMENTS.md from this plan.
- DPVS-02 requirement: NOT STARTED (conditional D-13/D-14 source edits live in plan 10-06; gated by Wave 5 verdict outcome).
- Pre-existing Koogie post-build copy failure: logged for `deferred-items.md` if a future plan needs the editor build path.

---
*Phase: 10-dpvs-culling-experiment*
*Plan: 10-01 (Wave 0 -- pre-instrumentation scaffolding)*
*Completed: 2026-05-10*
