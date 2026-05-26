---
phase: 15
reviewers: [codex, cursor]
reviewed_at: 2026-05-26T22:35:57Z
plans_reviewed: [15-01-PLAN.md, 15-02-PLAN.md, 15-03-PLAN.md, 15-04-PLAN.md]
self_skipped: claude (running inside Claude Code — skipped for independence)
---

# Cross-AI Plan Review — Phase 15 (In-Game Browser XPCOM/Mozilla Removal & Milestone Gate)

Reviewed by **Codex** (OpenAI) and **Cursor** (read-only repo access). `claude` skipped — the
orchestrator runs inside Claude Code, so it is excluded for reviewer independence.

Both reviewers rate the plan set **MEDIUM risk, execute-ready after targeted patches** — the
removal *strategy* is sound; the issues are *plan completeness*, fixable without re-research.

---

## Codex Review

## Summary

The plan set is strong and execution-ready for a removal phase. It correctly treats Phase 15 as build-graph and caller de-wiring work, not feature work, and it carries forward the key lessons from Phases 12-14: inline `.vcxproj` wiring matters, `/FORCE` makes MSBuild exit status insufficient, and positional enum edits require proof. The biggest residual risks are not in the high-level shape, but in execution details: broad grep/KEEP-list handling, the human boot/focus validation being too narrow, and the plans’ “commit once” instructions potentially conflicting with the repo rule that commits happen only when the user asks.

## Strengths

- The wave ordering is sound: 15-01 removes the closed symbol chain atomically, 15-02 purges residue, 15-03 drops the solution project and vendored tree last, and 15-04 runs the milestone gate.

- The `UITypeID` decision is well argued. The plan does not just assume enum deletion is safe; it requires evidence that `.ui` loading is string-keyed and that `UITypeID` is in-memory RTTI only.

- The TCG callback decision is also evidence-based. If `CallbackTable` defaults to null and setters are pure assignments, unregistering by deletion is equivalent to never registering.

- The build gate is appropriate for this codebase: Debug + Release, forced relink, grep link logs for `unresolved external symbol`, and do not trust MSBuild exit status.

- The plans preserve explicit scope guards: sibling `*Browser` UIs, `libEverQuestTCG`, Bink, Miles, `989crypt.lib`, and non-Mozilla editor deps are called out repeatedly.

- 15-03’s pre-delete merge gate is a good safety measure. It should catch incomplete 15-01/15-02 execution before deleting the vendored tree.

## Concerns

- **MEDIUM: Commit instructions may violate repo policy.**  
  The project constraint says commits only when the user asks. Plans 15-01 through 15-03 repeatedly require “ONE commit,” and 15-04 implies milestone closure. Unless the executor has an explicit user instruction to commit, this is a process violation. The implementation can still follow the wave boundaries without committing.

- **MEDIUM: 15-04 KEEP-list could hide real residue if implemented too broadly.**  
  The plan says exclude `soePlatform/ChatAPI2/**` and `soePlatform/libs/**` for some sweeps. That is acceptable for known Vivox false positives, but dangerous if applied globally to all milestone tokens. For XPCOM/Mozilla tokens, there should be no broad KEEP-list exclusions except generated `.filters`; those must be literally zero.

- **MEDIUM: Human boot validation may not exercise enough UITypeID surface.**  
  The `UITypeID` delete verdict is sound based on source evidence, but char-select plus “HUD/radial/IME focus” may not instantiate enough style-heavy UI paths to catch all enum-shift fallout. Since the shifted entries are mostly `*Style` IDs, the validation should explicitly open multiple UI pages that use styles, not only focus paths.

- **LOW/MEDIUM: The `libEverQuestTCG` null-callback proof depends on deployment assumptions.**  
  If `SWGTCG.dll` is absent, crash risk is low. If a user later restores the DLL, behavior depends on whether the DLL tolerates null navigate callbacks. The plan is fine for the stated SWGSource VM, but the summary should record that the safety proof is environment-specific unless the DLL contract is known.

- **LOW: 15-01 acceptance uses scoped greps that can miss global residual source refs until 15-03/15-04.**  
  This is not fatal because later gates are repo-wide, but after 15-01 it would be useful to run a broader source grep excluding the vendored tree to catch a missed caller earlier.

- **LOW: `.filters` exclusion is practical but should be documented as cosmetic debt.**  
  Excluding `*.filters` from grep-zero is reasonable if they are generated/non-build inputs, but stale WebBrowser entries there can still confuse IDE users. If they exist and are easy to purge, removing them would improve consistency.

## Suggestions

- Remove or conditionalize all “commit” requirements: “commit only if explicitly requested by the user; otherwise leave the working tree staged/unstaged with summaries.”

- In 15-04, split sweep commands by token family:
  - XPCOM/Mozilla family: literal zero over `src --glob '!*.filters'`.
  - Vivox: exclude only the known ChatAPI2/libs false positives.
  - P12/P13 tokens: manually classify remaining hits against the exact KEEP-list.

- Strengthen the UITypeID boot backstop. In addition to HUD/radial/IME, open inventory, options, chat, datapad, mission/journal, character sheet, and any style-heavy UI panels available before or just after character select.

- Record the TCG callback caveat in the summary: safe for current deployment because `SWGTCG.dll` is absent and callback storage defaults null; if TCG DLL support is restored later, navigation callback handling should be reviewed.

- Add an early post-15-01 grep:
  `rg -i "SwgCuiWebBrowser|WebBrowserManager|TUIWebBrowser|libMozilla|xpcom|xul" src --glob '!external/3rd/library/libMozilla/**' --glob '!*.filters'`
  This catches missed callers before 15-03.

- If `.filters` files contain deleted WebBrowser entries, purge them unless prior convention says they are intentionally ignored.

## Risk Assessment

**Overall risk: MEDIUM.** The plan quality is high and the removal graph is well understood, but this is still high-blast-radius live UI surgery plus build-graph deletion. The `UITypeID` outright delete appears justified by the evidence; it would be falsified by any persisted numeric `UITypeID` use, archive/read/write path, IFF/TRE row-index coupling, or a loader/asset format that stores enum ordinals instead of type names. The TCG unregister choice is reasonable for the current VM, but depends on the absent DLL/null-callback assumption. The biggest execution risks are grep exclusions that are too broad, accidental over-deletion of sibling Browser UIs or non-Mozilla libs, and insufficient manual UI coverage after the enum shift.

---

## Cursor Review

# Phase 15 Plan Review — XPCOM/Mozilla Removal & Milestone Gate

## 1. Summary

The four-plan set (15-01 → 15-04) is **strong, research-grounded, and largely ready to execute**. It correctly mirrors Phase 13/14 precedents: atomic symbol-chain deletion, inline-`.vcxproj`-first linking, `/FORCE`-aware link-log gating, parallel residue purge, vendored-tree delete last, and a human dual-renderer milestone close. The removal map matches the live tree (verified: callers, sln GUID locations, per-config link tokens, and the five `IsA(TUIWebBrowser)` sites are all accounted for). Two **concrete plan gaps** would likely fail Wave 2 or Wave 3 gates (`SwgClient/libraries.rsp` omitted from 15-02; 15-04 A1 would break link if it drops `SwgCuiG15Lcd` ClCompile while live callers remain). Fix those before execution and this set is **execute-ready with MEDIUM residual risk** (enum ordinal + milestone sweep semantics), not HIGH.

---

## 2. Strengths

- **Correct atomic-delete topology (15-01):** The symbol graph `callers → SwgCuiWebBrowser* → libMozilla(libs)` and `5× IsA → TUIWebBrowser` is closed in one build-gated unit. Sub-steps A/B/C as intentionally non-buildable intermediates, single commit after Task 3, is the right shape for this closed chain — not an unrecoverable half-state in git, just a deliberate “edit everything, then gate once” workspace pattern.

- **D-02 research is thorough and structurally distinct from CR-01:** The trace through string-keyed `UILoader::CreateObject(name)`, absence of a WebBrowser `TypeName`/loader, and in-memory-only `UITypeID` usage is convincing. Repo grep confirms no `GetTypeId` serialization path in the client `ui` lib; the only runtime `UITypeID` cast from persisted data is in out-of-scope `UiBuilder` editor tooling.

- **D-04a is defensible for the stated deployment target:** `callbackTable` is zero-init, navigate setters are pure assignment, and `navigateProc` is only consumed when `SWGTCG.dll` loads and `pInitializeProc(..., &callbackTable)` runs (line 532 in `libEverQuestTCG.cpp`). On SWGSource without that DLL, the path never executes.

- **Build-graph lessons are internalized:** Inline `SwgClient.vcxproj` edits (not vestigial `.rsp`), all **11** `swg.sln` locations (project + 6 config-platform + 9 dependency back-refs including SwgClient `:726`), Debug+Release link-grep (Optimized exempt per DEF-14-01), delete-exe-to-force-relink, PowerShell/msbuild path notes.

- **15-03 Wave-1 merge gate is excellent:** Re-grep before irreversible tree delete catches incomplete 15-01/15-02 early — the highest-value safety net in the wave sequence.

- **Scope guards are explicit and testable:** Sibling `*Browser` list/table UIs preserved; TCG infra preserved; Bink/Miles untouched; 15-01 includes SCOPE-PRESERVE acceptance assertions (`libEverQuestTCG` count, sibling Browser count).

- **15-02 correctly parallelizes with zero file overlap** against 15-01 (`.rsp` + editor `.vcxproj` vs source + client `.vcxproj`), matching Phase 14 14-02.

- **15-04 boot checkpoint is appropriately blocking** and includes the D-02 ordinal-shift backstop (HUD/radial/IME focus exercise) that automated gates cannot catch.

---

## 3. Concerns

| Severity | Issue |
|----------|-------|
| **HIGH** | **15-04 A1 lcdui cleanup would break the link.** Plan removes `SwgCuiG15Lcd.cpp` ClCompile/ClInclude from `swgClientUserInterface.vcxproj` but **keeps live callers**: `SwgCuiHud.cpp` (`initializeLcd`/`remove`/`updateLcd`) and `ClientMain.cpp` (`initializeLcd`). P12 intentionally left compiled no-op stubs (`USE_LCD` undefined → empty bodies) so callers keep linking. Dropping ClCompile without de-wiring callers → `unresolved external symbol` for `SwgCuiG15Lcd::*`, failing D-13 in Task 1 of 15-04. |
| **HIGH** | **15-02 omits `SwgClient/build/win32/libraries.rsp`.** Live tree still has `nspr4.lib`, `plc4.lib`, `profdirserviceprovider_s.lib`, `xpcom.lib`, `xul.lib` (lines 10–14). 15-02 purges `includePaths.rsp` + `libraryPaths.rsp` but not `libraries.rsp`. 15-03 repo-wide GATE-XPCOM grep (`src --glob '!*.filters'`) **will fail** on this file even after a perfect 15-01/15-02 otherwise. |
| **MEDIUM** | **15-04 milestone sweep for `SwgClientSetup` will false-fail.** `LaunchMeFirst/LaunchMeFirstDlg.cpp` still references `SwgClientSetup_{d,o,r}.exe` (P12 dropped the project, not this orphaned launcher string). Not in the documented KEEP-list. D-12.1 as written requires manual adjudication not specified in acceptance criteria. |
| **MEDIUM** | **`G15` as a separate sweep token is underspecified.** After A1, `G15` still appears in `SwgCuiG15Lcd*`, `getDisableG15Lcd()`, and filenames — unrelated to lcdui config residue. No KEEP-list entry for benign `G15` LCD stub symbols. Either drop `G15` as an independent token or document KEEP rules. |
| **MEDIUM** | **D-04a “safe to unregister entirely” is deployment-scoped, not absolute.** If `SWGTCG.dll` is ever present and TCG launches, `&callbackTable` is passed to `pInitializeProc` with null navigate members. Comment at `:485` (“make sure it's valid”) hints SOE expected populated callbacks. Low risk today; non-zero if TCG is revived. |
| **MEDIUM** | **15-01 Task 1 grep gate is narrower than final GATE-XPCOM.** Task 1 rg excludes `\bxpcom\b`, `nspr4`, etc. on edited caller files — fine for that sub-step, but executor could miss a stray `mozilla` literal outside the listed token set until Task 3/15-03. Low risk given the explicit CommandParser comment-block deletion. |
| **LOW** | **Parallel 15-01 + 15-02 commits before 15-03 merge gate:** If Wave 1 lands as two commits and someone runs 15-03 after only one, the merge gate catches it — but there is a brief window where the tree is grep-dirty between commits. Acceptable if 15-03 always follows both. |
| **LOW** | **D-02 verdict could be falsified** by any undiscovered client-loaded format storing numeric `UITypeID` (custom palette/cache, modded `.ui` tooling, non-`ui`-lib deserializers). Research found none; boot gate remains mandatory backstop. |
| **LOW** | **Criterion #1 `[Mm]ozilla` vs token set uses `nsIWebBrowser` but not bare `mozilla` in GATE-XPCOM** — mostly covered by deleting CommandParser `mozillaBrowserOutput` + comment block; worth one explicit `mozilla` grep in 15-03 merge gate. |

---

## 4. Suggestions

1. **Fix 15-04 A1 before execution:** Either (a) **only** remove lcdui *include/lib path segments* from `.vcxproj` (keep `SwgCuiG15Lcd` ClCompile — matches P12 stub design), or (b) expand A1 to de-wire `SwgCuiG15Lcd` callers in `SwgCuiHud.cpp` + `ClientMain.cpp` in the same atomic unit. Option (a) is minimal and satisfies “lcdui grep-zero in project config” without link risk.

2. **Add to 15-02 Task 1:** purge `src/game/client/application/SwgClient/build/win32/libraries.rsp` lines 10–14 (all five XPCOM family libs). Add to `files_modified` and acceptance grep.

3. **Extend 15-04 D-12.1 KEEP-list** for P12 stragglers:
   - `LaunchMeFirstDlg.cpp` `SwgClientSetup_*.exe` strings (orphaned launcher, not shipped via `swg.sln`), **or** add a one-line de-wire in A1 scope.
   - Benign `G15`/`SwgCuiG15Lcd` stub symbols if literal `G15`-zero is not required.

4. **Document D-04a residual risk in 15-01 threat model:** “If `SWGTCG.dll` is installed and TCG is launched, prefer D-04a fallback (logging no-op navigate procs).” Optional one-line comment at unregister site for future TCG revival.

5. **15-03 merge gate:** add explicit check for `SwgClient/build/win32/libraries.rsp` == 0 (catches the 15-02 gap class permanently).

6. **15-01 Task 3:** after link-grep, add one repo-scoped rg over `swgClientUserInterface` + `clientUserInterface` + `clientGame` + `ui/.../UITypeID.h` for the full GATE-XPCOM set — catches any missed browser literal before Wave 2.

7. **15-04 Task 1 acceptance:** automate P12 `SwgClientSetup` check with documented KEEP for `LaunchMeFirst` **or** require explicit SUMMARY line per surviving hit (currently only trackIR/lcdui manual confirm is described).

---

## 5. Risk Assessment — **MEDIUM**

**Justification:** Core removal surgery (15-01) is well-designed and matches the live symbol graph; D-02 outright delete and D-04 unregister are **sound for the SWGSource VM target** given current evidence. Risk is MEDIUM rather than LOW because:

1. **Two plan defects** (15-02 `libraries.rsp`, 15-04 A1 ClCompile drop) are likely gate failures or link breaks if executed literally.
2. **Milestone sweep semantics (15-04)** have undocumented false-positive paths (`SwgClientSetup`, `G15`) that could block v2.1 close or force ad-hoc scope expansion mid-execution.
3. **D-02 / D-04 verdicts are correct-but-not provable by grep/link alone** — boot + focus exercise remains essential (correctly planned in 15-04 Task 2).

Neither defect appears to indicate a wrong removal strategy; they are **plan completeness** issues fixable without re-research.

---

## Hazard-Specific Verdicts

### `UITypeID` / D-02 — **Verdict sound; boot gate mandatory**

Deleting `TUIWebBrowser` outright is **justified** for client runtime: `.ui` dispatch is string-keyed; no WebBrowser loader exists; `TUIWebBrowser` appears only in `UITypeID.h:64` within the `ui` lib; no client serialization of numeric `UITypeID` found.

**Would falsify D-02 if you find:**
- Any read/write of `UITypeID` integers into IFF/TRE/save/cache loaded by `SwgClient`
- A `.ui` or datatable row storing widget type as enum ordinal (not `TypeName` string)
- Third-party/plugin code outside the traced `ui` lib path using ordinals as stable IDs

Unlike CR-01, there is **no retail row-index coupling** identified. Still: link/grep **cannot** catch ordinal mis-RTTI; 15-04 focus-path exercise is not optional decoration.

### `libEverQuestTCG` navigate unregister — **Sound today; conditional crash if TCG DLL returns**

Unregister is safe when `SWGTCG.dll` is absent (current VM). `callbackTable.navigateProc` is passed to external DLL init — null navigate is **unverified against DLL internals**. Acceptable for Decruft scope; note in plan if TCG revival is ever in scope.

### Atomic 15-01 commit shape — **Correct**

Half-states are intentionally unbuildable within the workspace, not committed incrementally. No wave leaves the tree in a “committed but broken” state except briefly between parallel 15-01/15-02 commits — 15-03 merge gate mitigates. **15-04 A1 as written is the one plan that would commit a link break.**

### Debug+Release link gate / Optimized exempt — **Correct**

`/FORCE` + `/VERBOSE` false-pass is real project history; grep `unresolved external symbol` is the right gate. Optimized SAFESEH (DEF-14-01) exemption is appropriate for a removal phase gated on boot via Debug exe.

### Milestone KEEP-list (15-04) — **Mostly correct; incomplete for P12 stragglers**

ChatAPI2 `getVoice*`, prebuilt VChat libs, removal-provenance comments, `checkTrackIr` label — all verified sensible. **Gap:** `SwgClientSetup` in `LaunchMeFirst` and surviving `SwgCuiG15Lcd`/`G15` symbols are not covered. XPCOM family literal-zero requirement is correct (no KEEP FPs).

### Scope preservation — **Correct**

Sibling `SwgCuiCommandBrowser` / `SwgCuiMissionBrowser` / etc., TCG card path, Bink/Miles — all explicitly preserved with assertions. No dropped DECRUFT-06/07 requirements identified beyond the gaps above.

---

**Bottom line:** Approve the wave structure and 15-01/15-02/15-03 design after patching **15-02 (`libraries.rsp`)** and **15-04 A1 (do not drop `SwgCuiG15Lcd` ClCompile without caller de-wire)** plus milestone-sweep KEEP-list updates. Then execute.

---

## Consensus Summary

Two independent models converge on **MEDIUM overall risk**: the removal topology, D-02 (outright
`TUIWebBrowser` delete) and D-04 (TCG callback unregister) verdicts, the Debug+Release link-grep
gate, the atomic-15-01 commit shape, and the explicit scope guards are all judged **correct**. The
residual risk is in *execution-detail / plan-completeness*, not strategy.

### Agreed Strengths (both reviewers)
- **Wave ordering is sound** — atomic symbol-chain delete (15-01) → residue purge (15-02) → sln-drop + vendored-tree delete last (15-03) → milestone gate (15-04).
- **D-02 research is thorough and structurally distinct from CR-01** — string-keyed `.ui` loader, no WebBrowser `TypeName`/loader, in-memory-only `UITypeID`; no retail row-index coupling. (Both stress: link/grep CANNOT catch an ordinal-shift — the boot/focus exercise is a mandatory, not optional, backstop.)
- **D-04 callback unregister is evidence-based** — `callbackTable` zero-inits, setters are pure assignment, `navigateProc` only runs if the absent `SWGTCG.dll` loads.
- **Build-graph lessons internalized** — inline `.vcxproj` (not vestigial `.rsp`), all 11 `swg.sln` locations, `/FORCE`-aware link-log grep, Optimized-exempt, delete-exe-to-relink.
- **15-03 pre-delete merge gate** is the highest-value safety net — re-greps before the irreversible vendored-tree delete.
- **Scope guards explicit & testable** — sibling `*Browser` UIs, TCG infra, Bink/Miles all preserved with assertions.

### Agreed Concerns (raised by both — highest priority)
- **Milestone sweep / KEEP-list semantics (15-04).** The XPCOM/Mozilla family must be **literal-zero with NO KEEP exclusions except `.filters`** (Codex MEDIUM, Cursor confirms). Broad KEEP-listing risks masking real residue.
- **D-04a is deployment-scoped, not absolute** (both MEDIUM/LOW). Safe because `SWGTCG.dll` is absent on the SWGSource VM; record the caveat (and prefer the logging-no-op fallback) **if** TCG is ever revived.
- **The D-02 boot-gate backstop must be broad enough.** The shifted enum entries are mostly `*Style` IDs; Codex argues HUD/radial/IME focus alone may under-exercise the surface — open style-heavy pages (inventory, options, chat, datapad, character sheet) under both renderers.

### Cursor-Unique — TWO HIGH defects, both VERIFIED against the live tree by the orchestrator
Cursor's read-only repo access surfaced two concrete defects that the plan-checker and Codex missed.
**Both confirmed real (would fail Wave 2/3 gates if executed literally):**

1. **`15-02` omits `SwgClient/build/win32/libraries.rsp`.** VERIFIED: that file's lines 10–14 contain `nspr4.lib`, `plc4.lib`, `profdirserviceprovider_s.lib`, `xpcom.lib`, `xul.lib`. 15-02 purges SwgClient's `includePaths.rsp` + `libraryPaths.rsp` but **not** `libraries.rsp`. → 15-03's repo-wide `xpcom`/`xul` grep-zero gate **fails**. (Also corrects CONTEXT/RESEARCH **D-08**, which asserted the XPCOM libs are inline-only and "not in any `libraries*.rsp`" — they ARE in SwgClient's own `libraries.rsp`.) **Fix:** add `SwgClient/build/win32/libraries.rsp` (5 XPCOM lib lines) to 15-02's purge list + the 15-03 merge-gate check.

2. **`15-04` A1 drops `SwgCuiG15Lcd.cpp` ClCompile while live callers remain → link break.** VERIFIED: `SwgCuiG15Lcd.cpp` (420 lines) still *defines* `initializeLcd()` / `updateLcd()` / `remove()` as `#ifdef USE_LCD`-guarded no-op stubs; the callers are live and unguarded — `ClientMain.cpp:361`, `SwgCuiHud.cpp:678/686/1140`. Removing the `.cpp` from ClCompile (15-04 line 67) deletes those symbol definitions → `unresolved external symbol SwgCuiG15Lcd::initializeLcd` → fails D-13 at the milestone gate. **Additional finding:** removing the ClCompile entry does **nothing** for a literal `lcdui` grep (the entry string is `SwgCuiG15Lcd.cpp`, not `lcdui`) — so it is both unnecessary AND breaking. **Fix (Cursor option a):** A1 should remove ONLY the three lcdui *include-path segments* from `swgClientUserInterface.vcxproj:73,111,148` (+ SwgGodClient lcdui lib dirs); KEEP the `SwgCuiG15Lcd` ClCompile/ClInclude entries (the P12 stub must keep compiling so callers link).

3. **(Related, MEDIUM) `SwgClientSetup` + `G15` milestone-sweep false-positives.** `LaunchMeFirst/LaunchMeFirstDlg.cpp` still references `SwgClientSetup_{d,o,r}.exe` (orphaned P12 launcher string, not in the KEEP-list). And `G15` as an independent sweep token can never reach zero while the (in-scope-to-KEEP) `SwgCuiG15Lcd` stub subsystem exists. **Fix:** drop `G15` as an independent sweep token (it is not a v2.1 removed-subsystem requirement — `lcdui` is), and either KEEP-list or de-wire the `SwgClientSetup` launcher string.

### Divergent Views
- **Codex** flagged the plans' "ONE commit" instructions as a possible repo-policy violation (commit-only-when-asked). This is a **GSD-model misread** — `/gsd:execute-phase` commits atomically per the GSD contract (`commit_docs: true`), which is the user-sanctioned workflow. Not a real concern in this pipeline; noted for completeness.
- **Cursor** caught the two HIGH plan defects (repo access); **Codex** did not (prompt-only). Codex's unique value-add was the broaden-the-boot-gate-UI-coverage suggestion.

### Orchestrator recommendation
The two HIGH defects are real and would fail the Wave 2 (grep-zero) and Wave 3 (link) gates. Both are
**plan-completeness fixes, not strategy changes** — incorporate via `/gsd:plan-phase 15 --reviews`
(targeted: add `SwgClient/libraries.rsp` to 15-02; rescope 15-04 A1 to lcdui-path-only + drop the
`G15` sweep token + handle the `SwgClientSetup` launcher string; add the D-04a caveat + broaden the
boot-gate UI exercise). Strategy, waves, D-02/D-04 verdicts, and the gate design stand.

---

## Resolution (applied directly 2026-05-26 — user chose direct-fix over `--reviews` replan)

All findings folded into the plans by hand (verified against the live tree first); decision-coverage gate re-passed 14/14:

- **HIGH #1 (libraries.rsp)** → `15-02`: added `SwgClient/build/win32/libraries.rsp` to files_modified, the `<interfaces>` .rsp block, the D-06 truth, and Task 1 (files/read_first/action/acceptance/verify/done — now purges the 5 XPCOM lib lines + the grep covers `nspr4|plc4|profdirserviceprovider`).
- **HIGH #2 (A1 ClCompile link break)** → `15-04`: A1 RESCOPED to the lcdui **include-path strings only**; the `SwgCuiG15Lcd` ClCompile :450 / ClInclude :1004 entries are now explicitly **KEPT** (the stub must keep compiling for live callers `ClientMain.cpp:361` + `SwgCuiHud.cpp:678/686/1140`). Updated A1 truth, interfaces A1 block, Task 1 action.
- **MEDIUM (G15 / SwgClientSetup sweep)** → `15-04`: dropped `G15` as an independent grep-zero token; added KEEP-list entries for the benign `SwgCuiG15Lcd` stub and the `SwgClientSetup_*.exe` LaunchMeFirst launcher orphan (flagged pre-existing P12 cleanup, not a Phase-15 regression). Updated D-12.1 truth, sweep tokens, KEEP-list, and the P12/P13 acceptance assertion.
- **MEDIUM (D-04a deployment-scoped)** → `15-01`: added the environment-specific caveat to threat T-15-02 (safe because `SWGTCG.dll` absent; prefer logging-no-op fallback if TCG ever revived).
- **MEDIUM (boot-gate UI coverage)** → `15-04`: broadened the D-02 ordinal-shift backstop — the boot gate now also opens style-heavy pages (inventory, options, character sheet, chat, datapad/journal), since the shifted ordinals are mostly `*Style` IDs.

Strategy, waves, D-02/D-04 verdicts, and the gate design were unchanged. Plans are execute-ready.
