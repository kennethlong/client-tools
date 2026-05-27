---
phase: 16
reviewers: [codex, cursor]
reviewed_at: 2026-05-27T13:56:09Z
plans_reviewed: [16-01-PLAN.md, 16-02-PLAN.md, 16-03-PLAN.md]
self_skipped: claude (running inside Claude Code — skipped for independence)
review_round: 2 (re-review of plans patched per round-1 feedback)
prior_review: see git history commit 43f0870a5 (round-1 codex+cursor review that drove the replan)
---

# Cross-AI Plan Review — Phase 16 (v2.1 Tech-Debt Cleanup) — ROUND 2 (Re-Review)

This is a **re-review** of the three plans after they were patched (commit `b994f306c`) to
incorporate the round-1 cross-AI feedback. Both reviewers again had read access to the live
tree at `D:/Code/swg-client-v2` and verified plan claims against real source.

**Bottom line — both reviewers independently APPROVE for execution.** Every round-1 HIGH and
MEDIUM finding is confirmed RESOLVED against the live tree. No new HIGH/blocking issue was
introduced. The remaining items are LOW polish plus one genuinely new finding both/one caught:
**ancillary doc drift** (`16-VALIDATION.md` + `16-PATTERNS.md` still carry the corrected-elsewhere
"no save/load hook" claim) and a now-dead `#include "shellapi.h"`.

---

## Codex Review

*(OpenAI Codex, `exec --sandbox read-only` — performed its own verification against the tree)*

**Summary**

The revised plans address the prior substantive findings. I verified against the live tree that `REGISTER_OPTION(speakerVolume)` and `REGISTER_OPTION(micVolume)` still exist at `CuiPreferences.cpp:841`, and 16-02 now explicitly removes them with the statics/accessors/header declarations. I also verified 16-03 now depends on both wave-1 plans at `16-03-PLAN.md:6`. No new blocker found; remaining items are polish.

**Prior Findings Status**

- HIGH `REGISTER_OPTION` miss: resolved. 16-02 includes the two registration lines in must-haves, task action, and grep gates.
- MEDIUM 16-03 dependency missing 16-01: resolved. `depends_on: [16-01, 16-02]`.
- LOW per-token KEEP-list grep: mostly resolved in intent, but still uses one alternation plus manual spot-check. See low concern below.
- LOW dead `ConfigClientGame.h` include: resolved in 16-02.
- LOW D-07 mistag: resolved; D-07 now correctly means no ordinal placeholders.
- LOW execution context path: resolved; paths point at `D:/Code/swg-client-v2`.
- LOW `swg.sln /t:SwgClient` gate: resolved in 16-03.

**Plan 16-01**

Strengths:
- Correctly scopes the SwgGodClient change to only `989crypt.lib;` at `SwgGodClient.vcxproj:99`.
- Preserves the adjacent soePlatform tokens and separate `crypto.lib`.
- Correctly keeps SwgGodClient grep-only and does not attempt a known-broken tool build.
- D-04 editor `lcdui` verify-only claim is live-tree correct: the four editor vcxproj files have 0 `lcdui` hits.

Concerns:
- LOW: The KEEP-list check says "per-token" but the command is still a single alternation with human spot-check. This is acceptable, but weaker than a true per-token loop.

Suggestions:
- Replace the KEEP check with an explicit token loop: `foreach ($t in 'Base.lib','ChatAPI.lib',...) { rg ([regex]::Escape($t)) <file> }`
- Keep `crypto.lib` as its own separate assertion, as planned.

Risk Assessment: LOW. It is a one-token build-config edit with good residue gates.

**Plan 16-02**

Strengths:
- Correctly includes the full voice-volume removal set: 2 statics, 2 `REGISTER_OPTION` lines, 4 accessor definitions, and 4 header declarations.
- The broad repo-wide grep for `speakerVolume|micVolume|SpeakerVolume|MicVolume` now catches the exact prior failure mode.
- The `finalUrl` scope guard is precise: delete only the URL-construction block and leave `httpParams` accumulation intact.
- Correctly removes `ConfigClientGame.h` after deleting its only live use.

Concerns:
- LOW: After deleting the commented `ShellExecute` line, `#include "shellapi.h"` at `SwgCuiHudAction.cpp:11` appears dead too. The live tree currently only references `ShellExecute`/`SW_SHOW` in that deleted block. Not a build risk, just residual include cleanup.

Suggestions:
- Add `shellapi.h` to the dead-include check for 16-02, or explicitly leave it out of scope. Removing it is likely safe if a post-edit `rg "ShellExecute|SW_SHOW|shellapi" <file>` shows only the include.

Risk Assessment: LOW. The revised plan directly closes the compile-breaker from the prior review.

**Plan 16-03**

Strengths:
- Correct dependency ordering: waits for both 16-01 and 16-02.
- Correctly validates through `src/build/win32/swg.sln /t:SwgClient`, not bare `SwgClient.vcxproj`.
- Correctly uses link-log grep for `unresolved external symbol` because `/FORCE` can mask failures.
- Correctly skips Optimized due to the pre-existing SAFESEH defect.
- Boot smoke uses `stage/client_d.cfg`, and the live tree already has `rasterMajor=11` at line 37.

Concerns:
- LOW: The build action says build Debug and Release but does not spell out the exact `/p:Configuration=Debug /p:Platform=Win32` and Release equivalents. Likely obvious to the executor, but exact commands reduce drift.

Suggestions:
- Add concrete command templates using the full MSBuild path, `src/build/win32/swg.sln`, `/t:SwgClient`, `/p:Configuration=Debug|Release`, `/p:Platform=Win32`, and `/nodeReuse:false`.
- Keep deleting `stage/SwgClient_d.exe` and `stage/SwgClient_r.exe`; those names match the live project outputs.

Risk Assessment: LOW. The gate matches the project's proven validation pattern and covers the edited libraries.

**Overall Risk**

LOW. The revised plans now cover the prior high and medium issues, match the live tree, and preserve the phase goal: small no-behavior-change cleanup, grep-zero residue checks, Debug+Release link-grep, and one D3D11 character-select boot. No new blocker found.

---

## Cursor Review

*(cursor-agent, `--mode ask --trust` — read the actual repo tree)*

# Phase 16 Re-Review — Cross-AI Plan Quality Assessment

## Executive Summary

The revised plans are **substantially improved** and ready for execution. All **HIGH** and **MEDIUM** findings from the prior review are correctly addressed in the plan text, and live-tree verification confirms the underlying claims (token locations, dead code bounds, zero callers, `REGISTER_OPTION` persistence wiring, lcdui already clean, `rasterMajor=11` in `client_d.cfg`). Remaining gaps are **documentation drift** in ancillary files and **build-recipe underspecification** in 16-03 — neither is a blocker if the executor follows Phase 12–15 precedent, but they are worth tightening.

## (a) Prior Findings — Resolution Status

| Prior Finding | Severity | Status | Evidence |
|---------------|----------|--------|----------|
| **16-02 missed `REGISTER_OPTION(speakerVolume)` / `REGISTER_OPTION(micVolume)` at CuiPreferences.cpp:841–842** | HIGH | **RESOLVED** | 16-02 `must_haves`, Task 2 action (deletion group 2), and grep gates all include the 2 lines. Live tree: macro at `:415`, registrations at `:841–842`, statics at `:397–398`. |
| **16-03 `depends_on` should include 16-01** | MEDIUM | **RESOLVED** | `16-03-PLAN.md` frontmatter: `depends_on: [16-01, 16-02]` with explicit rationale in notes. |
| **Per-token KEEP-list grep (not 3-token sample)** | LOW | **RESOLVED** | 16-01 acceptance criteria now require all 9 soePlatform tokens present with spot-check instruction. |
| **Dead `ConfigClientGame.h` include removal** | LOW | **RESOLVED** | 16-02 Task 1 action + verify (`rg ConfigClientGame == 0`). Live: include at `:24`, sole use `getCsTrackingBaseUrl()` inside block `:1172`. |
| **D-07 mistag on Task 1 (`finalUrl`)** | LOW | **RESOLVED** | Task 1 title is now `(D-06, narrow scope)` only; D-07 confined to Task 2. |
| **`execution_context` wrong repo path** | LOW | **RESOLVED** | All three plans use `@D:/Code/swg-client-v2/.claude/...`. |
| **`swg.sln /t:SwgClient` build discipline** | LOW | **RESOLVED** | 16-03 Task 1 explicitly requires solution build (not bare vcxproj), exe delete-first, link-log grep, Optimized EXEMPT. Live: `src/build/win32/swg.sln` exists; SwgClient `ProjectDependencies` includes both `clientUserInterface` and `swgClientUserInterface`. |

## Live-Tree Verification (Plan Claims vs Source)

| Claim | Verified |
|-------|----------|
| `989crypt.lib` only in SwgGodClient Debug deps (~line 99), between `TcpLibrary.lib;` and `tinyxmld.lib;` | Yes — live line 99 |
| Separate live `crypto.lib` on same line (adjacency trap) | Yes — appears earlier in dep string |
| Optimized/Release dep lists have no `989crypt` | Yes — lines 143, 185 |
| 4 editor vcxproj: 0 `lcdui` hits | Yes — repo-wide grep under `src/engine/client/application` |
| `finalUrl` dead block `:1170–1189`, consumer commented at `:1189` | Yes — exact bounds match |
| Voice API: statics, accessors, decls, `REGISTER_OPTION` lines | Yes — all at stated lines; **0 external callers** repo-wide |
| `stage/client_d.cfg:37` → `rasterMajor=11` | Yes |
| SwgClient links both edited libs | Yes — `SwgClient.vcxproj` AdditionalDependencies include both `.lib`s |

## Plan-by-Plan Assessment

### 16-01 — SwgGodClient + Editor Verify

**Summary:** Clean, low-risk build-config plan. Single-token edit is correctly scoped; D-02 sweep is confirm-zero for everything except `989crypt.lib`; D-04 folded in appropriately.

**Strengths**
- Explicit `crypto.lib` vs `989crypt.lib` adjacency warning
- Per-token KEEP-list acceptance criteria (improved from prior review)
- Correct grep-only gate for unbuildable SwgGodClient (D-03)
- Analog commit reference (`4bc512b45`) gives executor a diff template

**Concerns**
- **LOW:** KEEP-list verification still uses one combined OR-regex; relies on manual "spot-check all 9" rather than 9 individual assertions. Acceptable but not fully deterministic.
- **LOW:** Task 2 git-status check for "no editor edits" is good hygiene but slightly redundant given verify-only scope.

**Risk:** LOW

### 16-02 — Source Residue Removal

**Summary:** The critical compile-breaker from the prior review is fully fixed. The plan is precise about narrow vs wide `finalUrl` scope and treats the voice-volume API as a four-part atomic deletion (statics + `REGISTER_OPTION` + accessors + decls).

**Strengths**
- REGISTER_OPTION fix is thorough across objective, must_haves, action, and triple grep verification
- Narrow-scope guard for `httpParams` accumulation (`:1081–1169`) is explicit with warning signs
- Repo-wide grep `speakerVolume|micVolume|SpeakerVolume|MicVolume` correctly catches persistence keys
- D-07 correctly scoped to voice API only (Task 2)

**Concerns**
- **LOW:** After Task 1, `#include "shellapi.h"` (`:11`) becomes dead (only use was commented `ShellExecute`). Not in audit scope; harmless, but leaves minor include residue.
- **LOW (informational):** Retained `httpParams` block is still dead-store when `params.length() > 0` — intentional per locked D-06 narrow bounds, not a plan defect.

**Risk:** LOW

### 16-03 — Verification Gate

**Summary:** Correct wave-2 gate with proper dependency ordering, `/FORCE` trap awareness, and human boot checkpoint. Build-graph claims verified against `swg.sln`.

**Strengths**
- `depends_on: [16-01, 16-02]` with documented sign-off rationale
- Solution-level build to avoid stale `.lib`s
- Link-log grep (not exit code) + exe delete-first
- Boot cfg trap documented (`client_d.cfg`, not `client.cfg`)
- Optimized EXEMPT correctly inherited from DEF-14-01

**Concerns**
- **MEDIUM:** Build recipe omits explicit paths/params an inexperienced executor might need:
  - Solution path: `src/build/win32/swg.sln` (exists; plan says bare `swg.sln`)
  - Separate invocations with `/p:Configuration=Debug|Release` and `/p:Platform=Win32`
  - How to capture link output reliably (tee redirect, `/fl`, or MSBuild file logger)

  Phase 15 plans have the same underspecification and succeeded via project memory — but this is the main operational risk for a fresh executor.
- **LOW:** 16-03 references `16-VALIDATION.md` in `read_first`, but that file's T3b row still says "no save/load hook" and its grep omits `REGISTER_OPTION` keys (see below).

**Risk:** LOW–MEDIUM (operational, not design)

## New / Still-Missed Issues

| Issue | Severity | Notes |
|-------|----------|-------|
| **`16-VALIDATION.md` stale on T3b** — still claims "no save/load hook"; grep omits `REGISTER_OPTION` | LOW | Plans 16-02 are correct; ancillary doc could mislead executor reading validation map |
| **`16-PATTERNS.md` Target 3b** — still says "no save/load hook" | LOW | Same doc drift; contradicts corrected RESEARCH/plans |
| **16-03 build command underspecification** | MEDIUM | See above; suggest explicit recipe |
| **Dead `#include "shellapi.h"` after finalUrl removal** | LOW | Optional micro-cleanup; out of audit scope |
| **16-VALIDATION T3a row tags D-07** | LOW | Minor doc tagging error (D-07 is voice-only) |

No new HIGH or blocking issues introduced by the revisions.

## Suggestions

1. **16-03 Task 1 — add explicit build recipe** (copy Phase 15 pattern): full MSBuild path + `src/build/win32/swg.sln` + `/t:SwgClient` + `/p:Configuration=Debug|Release` + `/p:Platform=Win32` + `/nodeReuse:false`, capturing each link log, then `rg "unresolved external symbol" <logs>` expecting exit 1.
2. **Sync `16-VALIDATION.md` + `16-PATTERNS.md`** with the REGISTER_OPTION correction and updated T3b grep (match 16-02 acceptance criteria).
3. **Optional:** Note in 16-02 Task 1 that `#include "shellapi.h"` may be removed if `rg ShellExecute` shows 0 hits post-edit (cosmetic only).
4. **Optional:** Split 16-01 KEEP-list check into 9 individual `rg -c` assertions for full automation.

## Risk Assessment

**Overall: LOW**

Well-bounded, no-behavior-change cleanup with proven Phase 12–15 validation discipline. The prior HIGH compile-breaker (`REGISTER_OPTION` dangling references) is fully addressed in 16-02. Live-tree evidence matches every edit target. Wave ordering is correct. The main residual risk is **operational** — an executor unfamiliar with MSBuild conventions might run an incomplete link gate — not a design flaw.

**Phase goal achievability:** Yes. Executing these three plans should close v2.1 tech debt while preserving SwgClient Debug+Release link health and a D3D11 char-select boot.

## Verdict

**Approve for execution** with optional polish on 16-03 build recipe and ancillary doc sync. No blockers remain from the prior cross-AI review.

---

## Consensus Summary

### Verdict (both reviewers)

**APPROVE FOR EXECUTION — overall risk LOW.** Every round-1 HIGH and MEDIUM finding is confirmed
RESOLVED against the live tree. No new HIGH or blocking issue was introduced by the patch. The
phase goal (no-behavior-change v2.1 tech-debt cleanup that still links clean + boots to
char-select under D3D11) is achievable as planned.

### Round-1 Findings — Resolution Confirmed (both reviewers, live-tree verified)

- 🔴→✅ **HIGH `REGISTER_OPTION` compile-breaker:** 16-02 now deletes `REGISTER_OPTION(speakerVolume)` / `REGISTER_OPTION(micVolume)` (`:841-842`) alongside the statics/accessors/decls, across must_haves + action + triple grep gate; the false "no save/load hook" claim is corrected in 16-02 objective, RESEARCH §Target 3b, and CONTEXT D-06.
- 🟠→✅ **MEDIUM 16-03 dependency:** `depends_on: [16-01, 16-02]`.
- 🟡→✅ **All 5 round-1 LOWs:** per-token KEEP-list intent, `ConfigClientGame.h` include removal, D-07 retag, `execution_context` → `swg-client-v2`, `swg.sln /t:SwgClient` build.

### Agreed Strengths (both reviewers)

- The REGISTER_OPTION fix is thorough and the repo-wide grep now catches the exact prior failure mode.
- `finalUrl` narrow-scope guard (delete `:1170-1189` only, leave `httpParams` `:1081-1169`) is precise.
- 16-03 wave ordering, `/FORCE` link-log-grep discipline, Optimized-EXEMPT, and `client_d.cfg` boot trap all match the live tree and the proven Phase 12–15 pattern.

### Agreed / Highest-Priority Concerns This Round (none blocking)

- **16-03 build-recipe explicitness** (cursor MEDIUM, codex LOW): the Debug+Release build step doesn't spell out the exact `src/build/win32/swg.sln /t:SwgClient /p:Configuration=… /p:Platform=Win32 /nodeReuse:false` invocations + link-log capture. Works via project memory (as in Phase 15) but is the main *operational* risk for a fresh executor.
- **Dead `#include "shellapi.h"`** (both, LOW): after the `finalUrl`/`ShellExecute` block is removed, `SwgCuiHudAction.cpp:11`'s `shellapi.h` include becomes dead (orchestrator verified: only ref is the commented `:1189`). A sibling of the `ConfigClientGame.h` cleanup already in 16-02.

### New Findings (cursor) — ancillary doc drift

- **`16-VALIDATION.md` + `16-PATTERNS.md` still carry the old "no save/load hook" claim** (LOW). The round-1 replan corrected RESEARCH.md + CONTEXT.md but not these two ancillary docs; 16-03 `read_first` points at VALIDATION.md and 16-01/02 `read_first` point at PATTERNS.md, so the stale claim could mislead an executor. `16-VALIDATION.md` T3a row also mistags D-07 (voice-only).

### Divergent Views

- Only on *severity* of the 16-03 build-recipe underspecification: Cursor rates it MEDIUM (operational risk for a fresh executor), Codex rates it LOW (obvious to the executor). Neither treats it as a blocker.

### Recommended Next Action

All blockers are gone — the plans are execute-ready. The remaining items are optional polish.
To fold them in: `/gsd:plan-phase 16 --reviews` (re-run the patch loop), or apply the trivial
edits directly (shellapi.h dead-include + VALIDATION/PATTERNS doc-drift sync + explicit 16-03
build recipe). Otherwise proceed to `/gsd:execute-phase 16`.
