---
phase: 16
reviewers: [codex, cursor]
reviewed_at: 2026-05-27T03:13:38Z
plans_reviewed: [16-01-PLAN.md, 16-02-PLAN.md, 16-03-PLAN.md]
self_skipped: claude (running inside Claude Code — skipped for independence)
---

# Cross-AI Plan Review — Phase 16 (v2.1 Tech-Debt Cleanup)

Both reviewers had read access to the live tree at `D:/Code/swg-client-v2` and verified plan
claims against real source. They **independently converged** on the same HIGH finding (a
compile-breaking gap in the voice-volume removal) and the same MEDIUM finding (16-03 dependency).
The HIGH finding was **re-verified by the orchestrator against the tree** (`CuiPreferences.cpp:841-842`
`REGISTER_OPTION(speakerVolume/micVolume)` + macro `:415` → `ms_ ## a`) and is confirmed real.

---

## Codex Review

*(OpenAI Codex v0.x, `exec --sandbox read-only` — performed its own `rg` verification against the tree)*

## Summary

The plans are mostly well-scoped and accurately reflect the live tree, but there is one concrete blocker in `16-02`: the voice-volume removal missed the registered preference keys. Live source still has `REGISTER_OPTION(speakerVolume)` and `REGISTER_OPTION(micVolume)` in `CuiPreferences.cpp`, and the macro expands to `ms_speakerVolume` / `ms_micVolume`. If the plan deletes only the statics/accessors/header declarations, `clientUserInterface` will fail to compile. Fix that before execution.

## Strengths

- `16-01` is accurate: `989crypt.lib` exists only in `SwgGodClient.vcxproj:99`, while `crypto.lib` is a separate live token backed by `src/compile/win32/crypto/*/crypto.lib`.
- `stationapi/` and `989crypt.lib` backing files are absent; removing the token is correct.
- The 4 editor `lcdui` check is clean: `rg -ni lcdui` across the named editor `.vcxproj` files returns 0.
- The `finalUrl` claim is accurate: all `finalUrl` uses are inside `SwgCuiHudAction.cpp:1170-1189`, and the only sink is the commented `ShellExecute`.
- The link-grep gate is well-designed for this repo because `/FORCE` is enabled in Debug and can mask unresolved externals.
- Optimized exemption is justified by the known SAFESEH issue; Debug+Release plus one D3D11 boot is proportionate for this no-behavior-change cleanup.

## Concerns

- **HIGH:** `16-02` falsely claims no save/load/register hook for the voice-volume statics. Live source has:
  - `CuiPreferences.cpp:841`: `REGISTER_OPTION(speakerVolume)`
  - `CuiPreferences.cpp:842`: `REGISTER_OPTION(micVolume)`
  - Macro at `CuiPreferences.cpp:415`: `REGISTER_OPTION(a)` expands to `ms_ ## a`

  Deleting `ms_speakerVolume` / `ms_micVolume` without deleting those two `REGISTER_OPTION` lines will create compile errors.

- **MEDIUM:** `16-03` depends only on `16-02`, but the phase roadmap says Wave 2 is blocked on Wave 1 completion. Since `16-01` is also Wave 1 and `16-03` success criteria mention all Plan 01 + 02 grep gates, `depends_on` should be `[16-01, 16-02]`.

- **LOW:** The `16-01` KEEP-list verification only checks `Base.lib|ChatAPI.lib|TcpLibrary.lib`, not the full preserved soePlatform cluster. The plan text says preserve the whole set, but the acceptance check is weaker than the stated risk.

- **LOW:** `16-02` leaves the upstream `httpParams` accumulation in place by design. That matches the narrow-scope decision, but the plan should avoid wording like "whole dead computation" unless it explicitly means only the URL construction block.

- **LOW:** Removing the `finalUrl` block will make `ConfigClientGame.h` unused in `SwgCuiHudAction.cpp` unless another use is added. Not a correctness issue, but it is minor residue.

## Suggestions

- Update `16-02` Task 2 to delete these two lines along with the statics:
  - `REGISTER_OPTION(speakerVolume);`
  - `REGISTER_OPTION(micVolume);`
- Update the `16-02` verification to require `rg "speakerVolume|micVolume|SpeakerVolume|MicVolume" src` returning 0 after the edit. This catches statics, accessors, declarations, and registration keys.
- Change `16-03` frontmatter to `depends_on: [16-01, 16-02]`.
- Strengthen the `16-01` KEEP-list check to verify every intended token individually: `Base.lib`, `ChatAPI.lib`, `ChatMono.lib`, `CommodityAPI.lib`, `dbgutil.lib`, `monapi.lib`, `Network.lib`, `rdp.lib`, `TcpLibrary.lib`, plus `crypto.lib`.
- In `16-02`, either remove the now-unused `ConfigClientGame.h` include or explicitly call it out as intentionally left alone for minimal diff.

## Risk Assessment

**Overall risk: MEDIUM until `16-02` is corrected; LOW after that.**

The actual code removals are small and the validation strategy is sound, but the missed `REGISTER_OPTION` lines are a real compile-breaking defect against the live tree. Once those two lines are folded into the voice-volume removal and `16-03` depends on both Wave 1 plans, the phase becomes low-risk and well-gated.

---

## Cursor Review

*(cursor-agent, `--mode ask --trust` — read the actual repo tree)*

## 1. Summary

The three plans are **well-aligned with CONTEXT D-01–D-08**, reuse the Phase 12–15 grep-zero / link-log-grep discipline, and **most tree claims check out** on the live repo at `D:/Code/swg-client-v2`. Verified: `989crypt.lib` only on `SwgGodClient.vcxproj:99` (Debug), separate live `crypto.lib` on the same line, editors at 0 `lcdui`, `finalUrl` dead through commented `ShellExecute`, narrow line bounds 1170–1189, `rasterMajor=11` in `stage/client_d.cfg:37`, SwgClient links both UI libs, `/FORCE` on SwgClient Debug link (`SwgClient.vcxproj:113`).

**One material defect:** RESEARCH and 16-02 Task 2 assert **no save/load hook** for voice volume, but the tree has **`REGISTER_OPTION(speakerVolume)` / `REGISTER_OPTION(micVolume)` at `CuiPreferences.cpp:841–842`**, which bind `ms_speakerVolume` / `ms_micVolume` into `LocalMachineOptionManager`. D-06's action list omits those lines; following it literally **will not compile**. 16-02's repo-wide `speakerVolume|micVolume` grep acceptance would catch the omission, but only after a failed build — fix the plan before execution.

Overall: **sound structure, one HIGH planning gap on Target 3b, otherwise LOW–MEDIUM nits.**

## 2. Strengths

- **Tree-verified Target 1:** Debug-only dead token; adjacency documented correctly (`...TcpLibrary.lib;989crypt.lib;tinyxmld.lib...` on line 99); `crypto.lib` is a distinct earlier token on the same line.
- **Target 2 correctly demoted:** All four editor `*.vcxproj` → **0** `lcdui` hits (verified).
- **Target 3a scope guard is right:** `finalUrl` has 5 hits; sole sink is commented at `:1189`; confirm-box (`:1074–1079`) and `onMessageBoxClosed` → `performAction(..., "yes")` (`:136–145`) stay live; narrow delete 1170–1189 matches the file.
- **crypto.lib / KEEP-list traps** called out repeatedly in 16-01, PATTERNS, and threat model — appropriate for line 99.
- **Verification matches project reality:** link-log grep (not msbuild exit 0), Optimized EXEMPT, `client_d.cfg` for Debug exe, delete-exe-before-relink.
- **Wave split is sensible:** 16-01/16-02 parallel (disjoint files); 16-03 gates in-closure edits only.
- **`swg.sln` ProjectDependencies** include `clientUserInterface` and `swgClientUserInterface` under SwgClient (`swg.sln:701–721`) — building `/t:SwgClient` through the solution should rebuild the edited libs.

## 3. Concerns

| Severity | Issue |
|----------|--------|
| **HIGH** | **Wrong claim: "no save/load hook" for voice volume.** `REGISTER_OPTION` at `CuiPreferences.cpp:841–842` expands to `registerOption(ms_speakerVolume, …)` / `registerOption(ms_micVolume, …)` (`:414–415`). Removing statics + accessors **without** removing these lines → **compile errors**. RESEARCH D-06 board says "no save/load hook"; 16-02 Task 2 action does not list `:841–842`. Acceptance `rg "speakerVolume\|micVolume" src → 0` would force removal but is not in the explicit edit steps. |
| **MEDIUM** | **16-03 `depends_on: [16-02]` only** — 16-01 can be skipped while 16-03 still passes. Success text says "Plans 01 + 02 grep-zero at sign-off" but nothing enforces 16-01 before phase close. |
| **MEDIUM** | **"No behavior change" is slightly loose for voice prefs:** Dropping `REGISTER_OPTION` stops persisting/loading `speakerVolume` / `micVolume` from local machine options (harmless with Vivox UI gone, but not literally zero runtime delta). |
| **LOW** | **Post–3a dead store remains:** After narrow delete, `CuiActions::service` with `params != ""` still runs `httpParams` build (`:1081–1169`) with no consumer — same as today (ShellExecute already dead). In scope per D-06; worth noting for "cosmetic" completeness. |
| **LOW** | **16-02 Task 1 title tags `D-07`** — D-07 applies only to voice API, not `finalUrl`. |
| **LOW** | **`execution_context` paths** point at `D:/Code/swg-client/` not `swg-client-v2` — executor may miss GSD templates if paths differ on disk. |
| **LOW** | **Research "9 KEEP libs PRESENT on disk"** — `soePlatform/libs/Win32-Debug/*.lib` not present in this workspace snapshot (may be gitignored / built locally). Irrelevant for 16-01 (grep-only, no SwgGodClient build) but weakens the "backed on disk" proof here. |

### Verified claims (no correction needed)

| Claim | Live tree |
|-------|-----------|
| `989crypt.lib` Debug-only, line 99 | ✅ `TcpLibrary.lib;989crypt.lib;tinyxmld.lib` |
| `989crypt` only under `src/` in that vcxproj | ✅ `rg` count 1 in `src` |
| `crypto.lib` separate from `989crypt` | ✅ same line 99, early in dep string |
| Editors `lcdui == 0` | ✅ |
| `finalUrl` dead, lines ~1170–1189 | ✅ matches read |
| Zero external callers of get/set Speaker/Mic | ✅ all hits in `CuiPreferences.cpp/.h` only |
| `rasterMajor=11` | ✅ `stage/client_d.cfg:37` |
| SwgClient links `clientUserInterface.lib` + `swgClientUserInterface.lib` | ✅ all three configs |
| `/FORCE` on Debug link | ✅ `SwgClient.vcxproj:113` |

### CORRECTED (research/plan vs tree)

| Claim | Status |
|-------|--------|
| "No save/load/register hook" for voice volume | **CORRECTED** — `REGISTER_OPTION(speakerVolume)` / `(micVolume)` at `:841–842` |

## 4. Suggestions

- **Amend 16-02 Task 2 (and D-06 in CONTEXT/RESEARCH)** to explicitly delete `REGISTER_OPTION(speakerVolume);` and `REGISTER_OPTION(micVolume);` at `CuiPreferences.cpp:841–842`, in the same atomic edit as statics/accessors/headers. Add verify: `rg "REGISTER_OPTION\(speakerVolume\)|REGISTER_OPTION\(micVolume\)" src` → 0.
- **Clarify D-06 "full API"** = statics + 4 accessors + 4 decls + **2 REGISTER_OPTION lines** (persistence registration, not just accessors).
- **Phase sign-off checklist:** require 16-01 complete before closing Phase 16 (or add `depends_on: [16-01, 16-02]` to 16-03 if you want tooling enforcement).
- **16-03 build command:** prefer `msbuild swg.sln /t:SwgClient` (not bare `SwgClient.vcxproj`) so solution `ProjectDependencies` rebuild `clientUserInterface` / `swgClientUserInterface`.
- **Optional follow-up (out of scope):** `stage/client_d.cfg:93` `voiceChatEnabled=false` — audit noted; keep deferred per D-05.
- **Fix `execution_context` paths** to the active repo root.

## 5. Risk Assessment

**Overall: LOW–MEDIUM**

| Area | Level | Justification |
|------|--------|----------------|
| Target 1 (989crypt) | **LOW** | Single-token delete; strong grep + crypto.lib guards; no build of SwgGodClient. |
| Target 3a (finalUrl) | **LOW** | Narrow bounds verified; live confirm/callback paths untouched. |
| Target 3b (voice API) | **MEDIUM** until plan fix | Missing `REGISTER_OPTION` removal is a **certain compile break** if executor follows D-06 text literally; acceptance grep would recover intent but wastes a cycle. |
| 16-03 gate | **LOW** | Link-log + one D3D11 boot matches no-behavior-change phase; solution deps mitigate stale `.lib` risk. |
| Over-removal (crypto / KEEP) | **LOW** | Well documented; post-edit greps in 16-01. |

**Bottom line:** Plans are **executable after a small 16-02 / RESEARCH patch for `REGISTER_OPTION`**. Without that, Target 3b is the main failure mode; everything else is disciplined cosmetic cleanup consistent with the live tree.

---

## Consensus Summary

### Agreed Strengths (both reviewers)

- **Target 1 (989crypt) is accurate.** Dead token is Debug-only on `SwgGodClient.vcxproj:99`; the adjacent `crypto.lib` is a *separate live token* (Codex located its backing at `src/compile/win32/crypto/*/crypto.lib`); the crypto.lib / KEEP-list adjacency trap is documented correctly.
- **Target 2 correctly demoted to verify-only** — all 4 editor vcxproj are 0 `lcdui` (both verified).
- **Target 3a `finalUrl` narrow scope is right** — sole sink commented at `:1189`; live confirm-box / callback paths stay untouched.
- **Verification gate matches project reality** — link-LOG grep (not msbuild exit, because Debug `/FORCE` masks unresolved externals), Optimized EXEMPT, `client_d.cfg` for the Debug exe, delete-exe-before-relink, single D3D11 boot proportionate for a no-behavior-change phase.
- **Wave split is sensible** — disjoint files in Wave 1; 16-03 gates the in-closure edits.

### Agreed Concerns (both reviewers — highest priority)

- **🔴 HIGH — `REGISTER_OPTION` compile-breaker (Target 3b).** `CuiPreferences.cpp:841-842` calls `REGISTER_OPTION(speakerVolume)` / `REGISTER_OPTION(micVolume)`; the macro at `:415` expands to `ms_ ## a` → `ms_speakerVolume` / `ms_micVolume`. 16-02 Task 2 deletes the statics but does **not** list `:841-842`, so following the plan literally **fails to compile** `clientUserInterface`. RESEARCH's "no save/load/register hook" claim (D-06) is **factually wrong**. **Orchestrator re-verified against the tree — confirmed real.** The plan's repo-wide acceptance grep would eventually force the removal, but only after a wasted failed-build cycle, and the explicit edit steps would mislead the executor.
  - **Fix:** fold `REGISTER_OPTION(speakerVolume);` + `REGISTER_OPTION(micVolume);` into the 16-02 Task 2 atomic edit; correct D-06 / RESEARCH "no hook" claim; the "full API" = statics + 4 accessors + 4 decls + **2 REGISTER_OPTION lines**.
- **🟠 MEDIUM — 16-03 `depends_on` is `[16-02]`, should be `[16-01, 16-02]`.** ROADMAP says Wave 2 is blocked on Wave 1 completion, and 16-03's own success criteria reference "all Plan 01 + 02 grep gates," but nothing enforces 16-01 before phase close.

### Divergent / single-reviewer findings

- **Cursor MEDIUM — "no behavior change" is slightly loose.** Dropping `REGISTER_OPTION` stops persisting/loading `speakerVolume`/`micVolume` from LocalMachineOptions. Harmless with the Vivox UI gone, but not literally zero runtime delta — worth a one-line acknowledgment.
- **Cursor LOW — `execution_context` paths point at `D:/Code/swg-client/` not `…-v2`.** (Independently noticed by the orchestrator during planning.) Executor may miss GSD templates if the v1 path doesn't exist on disk. Applies to all 3 plans.
- **Cursor LOW — 16-02 Task 1 title tags `D-07`**, but D-07 applies only to the voice API, not `finalUrl`.
- **Cursor LOW — RESEARCH "9 KEEP libs present on disk"** unverifiable in this snapshot (`soePlatform/libs/Win32-Debug` not present; may be gitignored). Irrelevant to execution (16-01 is grep-only, no SwgGodClient build) but weakens that proof line.
- **Cursor suggestion — build via `msbuild swg.sln /t:SwgClient`** so solution `ProjectDependencies` rebuild the edited libs (vs. bare `SwgClient.vcxproj`).
- **Codex LOW — 16-01 KEEP-list acceptance grep is weaker than the stated risk** — it only checks `Base.lib|ChatAPI.lib|TcpLibrary.lib`; suggest verifying every soePlatform token individually + `dbgutil.lib` + `crypto.lib`.
- **Codex LOW — removing the `finalUrl` block leaves `ConfigClientGame.h` include unused** in `SwgCuiHudAction.cpp` — minor residue; remove the include or note it as intentionally left for minimal diff.
- **Codex LOW — "whole dead computation" wording** could be read as including the httpParams accumulation; clarify it means only the URL-construction block (matches the narrow-scope decision).

### Verdict

Both reviewers: structure is sound and the validation strategy fits the project. **One HIGH (compile-break) and one MEDIUM (dependency) must be fixed before execution; the rest are LOW polish.** Risk is **MEDIUM until 16-02 is patched, LOW after.**
