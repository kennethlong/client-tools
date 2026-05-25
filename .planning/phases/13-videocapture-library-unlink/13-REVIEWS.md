---
phase: 13
reviewers: [cursor, codex]
reviewed_at: 2026-05-25T17:18:08Z
plans_reviewed: [13-01-PLAN.md, 13-02-PLAN.md, 13-03-PLAN.md]
---

# Cross-AI Plan Review — Phase 13: VideoCapture Library Unlink

Two independent external reviewers (Cursor agent · ask mode, and OpenAI Codex · exec read-only)
reviewed the three Phase 13 plans against PROJECT/ROADMAP/REQUIREMENTS/CONTEXT/RESEARCH.

---

## Cursor Review

# Phase 13 Plan Review: VideoCapture Library Unlink

## 1. Summary

These three plans are **strong and execution-ready**. They correctly absorb the Phase 12 `/FORCE` and inline-`.vcxproj` lessons, fix CONTEXT.md drift (live caller is `CuiIoWin.cpp`, not the parser chain), and split work into a sensible wave: **Plan 01** owns the atomic link break, **Plan 02** owns grep-gate residue, **Plan 03** sequences the vendored-tree delete last and runs the full phase gates including dual-renderer boot. Verified against the live tree: `CuiIoWin.cpp:19`/`208–210`, `SwgClient.vcxproj` inline capture libs + `/FORCE` + `/VERBOSE`, and the closed `#if 0` dead chain in `Game.cpp`/`Audio.cpp`/parser all match the plans. The main residual risks are **link-log capture reliability** and **Wave 1 parallel-merge discipline**, not incorrect removal scope.

---

## 2. Strengths

- **Correct linchpin identification.** Plan 01 centers on the one live Debug path (`CuiIoWin.cpp` → `SwgVideoCapture.obj` → vendored `.lib`s), which matches the actual code (`#if PRODUCTION == 0`, not `#if 0`).
- **Inline `.vcxproj` treated as authoritative.** Plans explicitly say `.rsp` edits are grep-gate-only; real unlink is in `SwgClient.vcxproj` `<AdditionalDependencies>` / `<AdditionalLibraryDirectories>` across all three configs.
- **`/FORCE` false-pass explicitly guarded.** D-06 is wired into acceptance criteria, threat model, and Task 1’s link-log grep gate — not MSBuild exit code.
- **Atomic-delete sequencing is sound.** Caller → wrapper compile drop → lib token drop → source delete → tree delete matches symbol resolution direction and avoids masked LNK2019.
- **Scope boundaries are clear.** Bink, Vivox, zlib, dpvs, crypto have explicit do-not-touch lists and negative-control checks (`binkw32.lib` preserved, `vivox` unchanged).
- **Wave structure is well-partitioned.** Zero file overlap between 13-01 and 13-02; 13-03 correctly depends on both before `git rm -r videocapture/`.
- **Phase 12 precedent applied.** Stale `.obj` pitfall, editor `.rsp`/`.vcxproj` purge (1d6b80242 analog), and closed-chain delete vs stub-in-place are all referenced appropriately.
- **All three DECRUFT-04 success criteria are mapped.** Crit #2 in Plan 01/03 Task 2; crit #1 in Plan 02/03 Task 1; crit #3 in Plan 03 Task 3 (human checkpoint).
- **Threat model is proportionate.** T-13-01 correctly treats per-frame `draw()` unresolved externals as a latent runtime fault, not a benign link warning.

---

## 3. Concerns

| Severity | Concern |
|----------|---------|
| **HIGH** | **Link-log capture is underspecified.** Task 1 (13-01) delegates the exact MSBuild invocation to the executor. MSBuild `/fl` detailed logs do not always surface `link.exe` `/VERBOSE` text in a predictable, grep-friendly way. If the captured log lacks linker output, the D-06 gate becomes a false green while `/FORCE` still emits a broken EXE. |
| **HIGH** | **Plan 01 autonomy vs. atomic commit.** Plan 01 is `autonomous: true` with three tasks, but Tasks 2+3 must land as **one commit-unit**. A parallel executor could theoretically commit Task 2 (caller/wrapper removed) without Task 3 (lib tokens still present → masked unresolved, or vice versa). The plan states the requirement but does not enforce it structurally. |
| **MEDIUM** | **Criterion #1 grep scope is split across artifacts.** ROADMAP crit #1 is literal: `.rsp`, source, include paths. D-04 is repo-wide including `.vcxproj`. Plans resolve this across 02/03, but **13-03 Task 1’s phase grep excludes `.vcxproj`**. Phase sign-off could pass crit #1 while a missed editor `.vcxproj` token remains (build-inert but violates D-04 intent). |
| **MEDIUM** | **`13-VALIDATION.md` is still draft** (`wave_0_complete: false`, TBD task IDs). Plans reference it as canonical validation, but it is not yet synchronized with the finalized plan tasks — executors may diverge on grep commands. |
| **MEDIUM** | **Release link gate is a weaker detector for Debug-only mistakes.** Live capture symbols compile only under `#if PRODUCTION == 0` (Debug). Release/Optimized may link clean even if Debug caller removal is incomplete, as long as stale Debug `.obj` isn’t relinked. Pitfall 5 mitigates this, but the plan should treat **Debug link-grep as the primary signal** and Release as confirmation. |
| **MEDIUM** | **Wave 1 parallel merge to main.** 13-01 and 13-02 can land independently without breaking SwgClient (02’s edits are `#if 0` or vestigial). That’s fine incrementally, but **partial phase completion could be misread** if someone checks crit #1 after only Plan 01. |
| **LOW** | **Hardcoded line numbers** (e.g., `CuiIoWin.cpp:208–210`, `SwgClient.vcxproj @102`) will drift; plans say “reconfirm” but automated executors may not. |
| **LOW** | **`execution_context` paths** point at `D:/Code/swg-client`, not `swg-client-v2` — minor executor confusion risk. |
| **LOW** | **Plan 02 Audio.cpp negative control** (`grep -c "AIL_"` vs baseline) is fragile; a small unrelated edit could false-fail. The zero-reference grep is the real gate. |
| **LOW** | **Inventory count wording.** “10 editor/aux `.vcxproj`” plus SwgGodClient/Viewer/TextureBuilder is correct, but the label “editor” understates SwgGodClient — functionally fine, slightly confusing for auditors. |

---

## 4. Suggestions

- **Lock Plan 01 Tasks 2+3 into a single commit** in plan metadata (e.g., `commit_unit: [task-2, task-3]`) or merge them into one task so autonomous execution cannot split the atomic unit.
- **Formalize link-log capture with a proven pattern**, e.g.:
  - Build with `/bl` or stdout redirect to `build.log`, then `grep "unresolved external symbol" build.log`
  - Or grep the MSBuild-generated `SwgClient.log` under `src/compile/win32/SwgClient/{Debug,Release}/` if that’s where `/VERBOSE` lands in this tree
  - Task 1 acceptance should require finding **both** `Searching` (VERBOSE) **and** absence of `unresolved external symbol` in the **same** log file used for the gate.
- **Add a composite phase sign-off grep in 13-03** beyond crit #1’s literal scope:

  ```bash
  rg -i 'videocapture|VideoCapture|videoCapture|AudioCapture' \
    --glob '*.rsp' --glob '*.{cpp,h,inl}' --glob '*.vcxproj' src
  ```

  Keep crit #1 and D-04 aligned in the SUMMARY so future audits don’t argue about scope.

- **Treat Debug link-grep as blocking; Release as required but secondary** in 13-01 Task 3 narrative — Debug is where the live symbol path exists.
- **Sync `13-VALIDATION.md`** with finalized task IDs, Wave 0 completion, and the exact grep commands from 13-01-SUMMARY before execution starts.
- **13-03 Task 1 pre-delete check:** extend precondition grep to `.vcxproj` (not just `.rsp`/source), so tree delete cannot proceed if Plan 02 missed a file.
- **Optional Wave 1 sequencing:** if merge discipline is a concern, run 13-01 first (link gate), then 13-02 (grep gate) — parallel is safe for build health but sequential reduces audit confusion.

---

## 5. Risk Assessment

**Overall: MEDIUM (closer to LOW for removal correctness, MEDIUM for verification discipline)**

**Justification:**

- **Removal correctness: LOW risk.** Research matches the live tree. The atomic unit (caller + wrapper + inline lib tokens) is the right cut. Dead `#if 0` code and vestigial `.rsp`/editor refs are correctly classified as non-linking. Bink/Vivox boundaries are explicit. Sequencing vendored-tree delete in Wave 2 after reference purge follows the Phase 12 dir-delete precedent.
- **Verification / process: MEDIUM risk.** `/FORCE` means the cost of a bad link-log gate is a shipped EXE with unresolved externals in per-frame `draw()` code — exactly the failure mode Phase 12 surfaced. The plans *know* this and gate on grep, but the capture mechanism is still the weakest link. Parallel autonomous execution of Plan 01 also adds merge-order risk if Tasks 2+3 aren’t truly atomic.

**Success criteria coverage:**

| Criterion | Covered? | Where |
|-----------|----------|-------|
| #1 Zero refs (`.rsp`/source/include) | Yes | Plan 02 Tasks 1–3; Plan 03 Task 1 post-delete grep |
| #2 Clean Debug + Release link | Yes | Plan 01 Task 3; Plan 03 Task 2 (re-run after tree delete) |
| #3 Dual-renderer boot | Yes | Plan 03 Task 3 (`rasterMajor=5` and `=11` in `client_d.cfg`) |

**Bottom line:** Proceed with execution after tightening link-log capture and atomic-commit enforcement. The plans are among the better-documented decruft steps in this milestone — they just need the verification harness to be as rigorous as the removal analysis already is.

---

## Codex Review

## Summary

The three-plan set is strong and unusually well-grounded in the research findings. It correctly 
pivots away from the stale `.rsp` assumption, treats inline `.vcxproj` link wiring as authoritative, 
handles `/FORCE` by grepping linker output, and sequences the vendored-tree delete last. The main 
risk is operational rather than conceptual: Wave 1 is labeled “parallel / zero file overlap,” but 
some verification gates in Plan 02 assume Plan 01 has already landed, so independent execution could 
produce false failures. A second risk is that the grep gates may be too broad or too narrow in 
places depending on generated/build artifacts and planning docs, so the exact grep scope should be 
locked before execution.

## Plan 13-01 Review

### Strengths

- Correctly identifies the true atomic link unit: `CuiIoWin.cpp` live caller, `SwgVideoCapture.*` 
wrapper, `clientGraphics.vcxproj`, and `SwgClient.vcxproj`.
- Handles the key `.rsp` vs inline `.vcxproj` distinction correctly.
- Covers all three `SwgClient.vcxproj` configs: Debug, Optimized, Release.
- Explicitly preserves Bink, Vivox, zlib, dpvs, crypto, and related out-of-scope dependencies.
- Correctly treats `/FORCE` as unsafe and requires `unresolved external symbol == 0` in captured 
link logs.
- Includes stale-object mitigation for `SwgVideoCapture.obj`.

### Concerns

- **MEDIUM:** Task 1 asks to run the Debug build before the unlink is complete, but the “link-log 
capture” validation may be noisy if the pre-existing tree already has unrelated unresolved 
externals. The plan should clarify whether the initial run is only proving log capture, not proving 
cleanliness.
- **MEDIUM:** The plan says “no compiled TU references capture symbols” after Task 2, but Task 3 has 
not yet removed lib tokens. That is fine, but verification should include source grep plus project 
compile-list grep, not rely only on broad text grep.
- **LOW:** `grep -ci "...Smart_debug|Smart_release..." SwgClient.vcxproj == 0` could miss a generic 
`Smart` path/token if naming differs. The allow-list is good, but final verification should include 
`picn20`, `CaptureCommon`, `ImageCapture`, `SoeUtil`, `ZlibUtil`, `AudioCapture`, `VideoCapture`, 
and `videocapture` broadly.
- **LOW:** “No dangling/double semicolons” is good but vague. XML link fields often tolerate 
duplicated semicolons; this is polish, not a correctness gate.

### Suggestions

- Mark Task 1’s first build as “log-capture smoke only”; reserve clean-link judgment for after Tasks 
2 and 3.
- Add an explicit post-edit check that `clientGraphics.vcxproj` has no `SwgVideoCapture` in 
`<ClCompile>` or `<ClInclude>`.
- Use PowerShell-compatible examples as well as grep examples, since the environment is PowerShell.
- Capture the exact before/after `vivox` and `binkw32` counts before editing, then compare after.

### Risk Assessment

**LOW-MEDIUM.** The plan is technically sound and targets the real link graph. Risk comes mainly 
from execution details: stale objects, log capture reliability, and broad text-editing in dense 
MSBuild XML.

## Plan 13-02 Review

### Strengths

- Correctly isolates inert cleanup from the live link removal.
- Handles the corrected path for `swgClientUserInterface` under `src/game/client/library`.
- Explicitly protects live Miles `AIL_*` playback and does not conflate AudioCapture with gameplay 
audio.
- Correctly includes editor/aux `.vcxproj` and `.rsp` residue for the repo-wide grep gate.
- Good precision around `Game.h` block boundaries and the phantom `SwgAudioCapture.h`.

### Concerns

- **MEDIUM:** The plan is declared parallel with Plan 01, but Task 3 verification expects zero 
capture references in all `.vcxproj` files, including files owned by Plan 01. If Plan 02 runs before 
Plan 01 finishes, that verification will fail.
- **MEDIUM:** Plan 02’s success criteria say “except the vendored tree,” but some grep commands 
search all `src` and may still hit vendored headers/source until Plan 03 deletes them unless the 
glob scope excludes `src/external/3rd/library/videocapture`.
- **MEDIUM:** The “10 non-build-path editor/aux `.vcxproj`” count is easy to drift. Research says 14 
total `.vcxproj` with refs: SwgClient, clientGraphics, clientGame, clientAudio, plus 10 aux. This 
plan should require live enumeration first and reconcile the count.
- **LOW:** Negative control “unchanged file set” for Bink/Vivox can be brittle if unrelated files 
have changed or if Plan 01 changes SwgClient counts. Better to check that protected tokens remain in 
expected edited files where they previously existed.
- **LOW:** Task 2 touches `clientGraphics/includePaths.rsp` while Plan 01 touches 
`clientGraphics.vcxproj`; no file overlap, but both are same project area. That is okay, but the 
coordination should be explicit.

### Suggestions

- Split Plan 02 verification into local gates and merged-wave gates:
  - Local: no capture refs in files owned by Plan 02.
  - After Plan 01 + 02: no capture refs in all `.vcxproj` / `.rsp` outside vendored tree.
- Add explicit `--glob '!src/external/3rd/library/videocapture/**'` or equivalent to pre-Plan-03 
grep checks.
- Require the executor to save the live `rg -l` enumerated file list before edits and compare it to 
the expected inventory.
- For Audio.cpp, verify by targeted grep for removed function names rather than broad `AIL_` count, 
since baseline counts are not embedded in the plan.

### Risk Assessment

**MEDIUM.** The cleanup scope is broad and includes many vestigial build files. It is mostly low 
behavioral risk, but the parallel verification assumptions and wide `.vcxproj`/`.rsp` editing create 
room for false failures or accidental token removal.

## Plan 13-03 Review

### Strengths

- Correctly sequences the vendored `videocapture/` tree deletion after all consumers and references 
are removed.
- Includes a pre-delete grep to prevent orphaned references.
- Re-runs Debug and Release link gates after deleting the provider tree, which is the right final 
proof.
- Correctly includes the human dual-renderer boot gate with `client_d.cfg`, `rasterMajor=5`, and 
`rasterMajor=11`.
- Reuses Plan 13-01 and 13-02 summaries, which creates traceability.

### Concerns

- **HIGH:** The final grep in Task 1 only includes `*.rsp`, `*.cpp`, `*.h`, and `*.inl`, but 
DECRUFT-04 / D-04 also intends repo-wide purge including `.vcxproj` inline refs. Plan 03 should 
include `.vcxproj` in the final full-repo grep, otherwise it can miss the exact class of issue the 
research warned about.
- **MEDIUM:** The grep excludes `.planning/` conceptually, but the written command searches `src`, 
so that is fine for planning docs. However, it may miss non-`src` build files if any exist outside 
`src`. If the requirement is repo-wide, use repo-root with explicit excludes.
- **MEDIUM:** `autonomous: false` is appropriate for boot verification, but Tasks 1 and 2 are 
automated. The plan should distinguish “automated work can proceed; human gate blocks completion.”
- **LOW:** The negative-control grep for `binkw32|vivox|dpvs` over all `src` only proves those 
strings exist somewhere, not that they were preserved in the intended link files.
- **LOW:** The known unrelated crash/flakiness note is useful, but the plan should require recording 
exact pass/fail evidence per renderer so ambiguous retries do not blur the gate.

### Suggestions

- Add `.vcxproj` to the final grep gate:
  ```text
  rg -n -i "videocapture|videoCapture|VideoCapture|AudioCapture|picn20|CaptureCommon|ImageCapture|Soe
Util|ZlibUtil" src -g "*.rsp" -g "*.vcxproj" -g "*.cpp" -g "*.h" -g "*.inl"
  ```
- Add an explicit check that `SwgClient.vcxproj`, `clientGraphics.vcxproj`, `clientGame.vcxproj`, 
and `clientAudio.vcxproj` are clean after both Wave 1 plans.
- For boot evidence, record renderer, config file path changed, executable launched, result, and any 
FATAL line.
- Add a post-delete build check for missing library errors as well as unresolved externals, e.g. 
grep for `LNK1181` / `cannot open input file`.

### Risk Assessment

**MEDIUM.** The sequencing is right, but the final grep gate currently under-checks `.vcxproj`, 
which is a material risk given the phase’s central research finding. Once `.vcxproj` is added to the 
final gate, the risk drops to low-medium.

## Overall Assessment

The plan set is complete in architecture and aligns well with all three DECRUFT-04 success criteria: 
zero refs, clean Debug/Release link, and dual-renderer boot. It correctly handles the two big 
historical traps: vestigial `.rsp` files and `/FORCE` false-passes.

The main improvements I would make before execution are:

- Treat Wave 1 as “parallel edits, merged verification,” not independently green plans.
- Add `.vcxproj` to Plan 13-03’s final grep gate.
- Make pre-Plan-03 grep explicitly exclude the vendored tree, then make post-delete grep include 
`.vcxproj`.
- Add `LNK1181` / missing-lib grep alongside `unresolved external symbol`.
- Record exact protected-token before/after checks for Bink/Vivox rather than broad existence greps.

**Overall risk: MEDIUM, reducible to LOW-MEDIUM with the grep/verification fixes above.** The 
code-change blast radius is controlled and the dependency ordering is fundamentally correct; the 
remaining risk is mostly from verification mismatch and broad mechanical edits across many MSBuild 

---

## Consensus Summary

Both reviewers independently rate the plan set **strong on removal correctness, MEDIUM on
verification discipline** (Cursor: "MEDIUM, closer to LOW for removal correctness"; Codex:
"MEDIUM, reducible to LOW-MEDIUM with the grep/verification fixes"). Neither found a flaw in
*what* gets removed — the disagreements are entirely about *how the gates prove it*.

### Agreed Strengths (raised by both)
- Correct atomic link unit: `CuiIoWin.cpp` live caller + `SwgVideoCapture.*` wrapper + inline
  `SwgClient.vcxproj` lib tokens across all 3 configs.
- `.rsp` correctly treated as vestigial; inline `.vcxproj` `<Link>` wiring is authoritative.
- `/FORCE` false-pass explicitly guarded — gate greps link output for `unresolved external
  symbol == 0`, not MSBuild exit 0.
- Scope fences are explicit and enforced with negative controls: Bink, Vivox, zlib, dpvs,
  crypto preserved; live Miles `AIL_*` audio untouched.
- Vendored-tree delete correctly sequenced last (Wave 2, after both Wave-1 plans).
- All three DECRUFT-04 success criteria mapped to plan tasks.

### Agreed Concerns (raised by both — highest priority)
1. **[HIGH] Wave 1 "parallel / zero file overlap" vs merged verification.** Plan 02's Task 3
   verifies *zero capture refs in ALL `.vcxproj`* — including files Plan 01 owns. If 01 and 02
   run independently, that gate can false-fail (Plan 02 finishing first) or a crit-#1 check run
   after only Plan 01 can be misread as phase-complete. **Fix:** treat Wave 1 as "parallel edits,
   *merged* verification" — split Plan 02's gates into local (own files) vs merged-wave (all
   `.vcxproj`/`.rsp` outside the vendored tree).
2. **[HIGH/MEDIUM] 13-03 Task 1 final grep excludes `.vcxproj`.** The phase sign-off grep covers
   only `.rsp`/source/include, but D-04 intends repo-wide purge *including* `.vcxproj` inline refs
   — exactly the class of issue RESEARCH flagged. A missed editor `.vcxproj` token (build-inert
   but D-04-violating) would pass. **Fix:** add `-g "*.vcxproj"` to the final full-repo grep.
3. **[HIGH/MEDIUM] Link-log capture is the weakest link.** Task 13-01-1 delegates the exact
   MSBuild invocation to the executor; if the captured log doesn't reliably surface `link.exe`
   `/VERBOSE` output, the D-06 gate goes false-green while `/FORCE` still emits a broken EXE.
   **Fix:** lock the exact build+capture command in the plan; require the gate to find BOTH the
   `Searching`/`/VERBOSE` marker AND zero `unresolved external symbol` in the *same* log file;
   add `LNK1181` / "cannot open input file" (missing-lib) to the grep alongside unresolved-external.

### Additional Concerns (one reviewer, worth addressing)
- **[Cursor HIGH] Plan 01 atomic commit not structurally enforced** — `autonomous: true` + 3 tasks;
  Tasks 2+3 must land as one commit-unit. Stated in prose but not enforced. **Fix:** merge Tasks
  2+3 into one task, or annotate a `commit_unit`.
- **[Both MEDIUM] Debug is the primary signal, Release is confirmation** — live capture symbols
  compile only under `#if PRODUCTION==0` (Debug); Release/Optimized can link clean even with an
  incomplete Debug removal (esp. with stale `.obj`). Treat Debug link-grep as blocking-primary.
- **[Codex MEDIUM] Pre-Plan-03 greps should exclude the vendored tree** (`!src/external/3rd/library/videocapture/**`)
  until Plan 03 deletes it, or Plan 02's "zero refs except vendored tree" gate self-trips on
  vendored headers.
- **[Codex MEDIUM] Editor `.vcxproj` count drift** — "10 aux" vs RESEARCH's 14-total; require live
  `rg -l` enumeration before edits and reconcile against the expected inventory.
- **[Cursor MEDIUM] 13-VALIDATION.md is still draft** (`wave_0_complete: false`, TBD task IDs) —
  sync it with the finalized task IDs + exact grep commands before execution.
- **[Both LOW] Fragile negative controls** — `grep -c "AIL_"` vs baseline and bare Bink/Vivox
  existence greps can false-fail/false-pass; prefer targeted checks (removed function names;
  protected tokens remain in the specific files where they previously existed).
- **[Codex LOW] PowerShell environment** — the plans give `rg`/`grep` examples; add
  PowerShell-compatible equivalents since the executor runs on Windows PowerShell.
- **[Cursor LOW] `execution_context` paths point at `D:/Code/swg-client`, not `swg-client-v2`** —
  a concrete planner error in 13-01-PLAN.md (lines 61-62); will mislead the executor. **Fix now.**
- **[Cursor LOW] Hardcoded line numbers** (`CuiIoWin.cpp:208-210`, `SwgClient.vcxproj @102`) will
  drift; plans say "reconfirm" but autonomous executors may not.

### Divergent Views
Essentially none. Both converge on the same risk profile and the same remediation list. The only
nuance: Codex emphasizes the missing-lib (`LNK1181`) grep and the vendored-tree glob exclusion;
Cursor emphasizes the atomic-commit enforcement and the draft VALIDATION.md. They are complementary,
not contradictory.

### Bottom Line
Proceed to execution **after** tightening the verification harness: (1) merged-wave verification for
Wave 1, (2) `.vcxproj` in the final grep, (3) a locked link-log capture command + `LNK1181` check,
(4) the `swg-client` → `swg-client-v2` execution_context path fix. These are plan-doc refinements,
not redesigns — feed them back via `/gsd-plan-phase 13 --reviews`.
