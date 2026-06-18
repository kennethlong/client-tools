---
phase: 35
reviewers: [codex, cursor, opus, sonnet]
reviewed_at: 2026-06-18
plans_reviewed: [35-01-PLAN.md, 35-02-PLAN.md, 35-03-PLAN.md, 35-04-PLAN.md]
orchestrator_ground_truth_verified: true
---

# Cross-AI Plan Review — Phase 35: Miles 9.3b Audio Port

Four independent reviewers on non-overlapping angles: **Codex** (repo tracer), **Cursor** (detailed
code reader), **fresh Opus** (spec/correctness math), **fresh Sonnet** (lateral/blind-spots). The
`claude` CLI was skipped (this session is Claude Code — independence). The orchestrator independently
verified the load-bearing grep counts and the stale-stage divergence against live source before
adjudicating — see **Consensus Summary → Orchestrator ground truth**.

---

## Codex Review

**Summary** — Phase is well-shaped: separates procurement/header compat, source/API port, link/staging,
and human UAT in a sensible chain. Strongest part is that it accounts for this repo's known traps
(`/FORCE` false-green, gitignored libs/stage, codec staging, x64 disable cleanup, manual audio UAT).
Main risks: stale-state validation, exact grep-count brittleness, hidden consumers of the old Miles
tree, and whether staging proves the runtime uses the intended 9.3b *bytes* (not just a clean link).

**35-01** — Strengths: good first wave; vendoring + include/lib change before API edits isolates header
risk; force-add handles `*.lib` gitignore; mirroring 7.2e lowers churn; compile-only smoke is a useful
early check.
Concerns: **MEDIUM** compile-only is weak (stale 7.2e import resolution can survive until 35-03);
**MEDIUM** verify lib architecture with `dumpbin /headers`, not just existence; **LOW** add a documented
provider manifest so staging checks aren't inferred from directory contents.
Suggestions: acceptance check for zero remaining active `library/miles/` refs in clientAudio.vcxproj;
record SHA256/size/machine-type for both libs (match the x64 DLL checklist pattern); add a provider
manifest. Risk: **LOW-MEDIUM**.

**35-02** — Strengths: edits in the shared path (D-01); deletes all three Phase-33 disables while keeping
retail `disableMiles`; treats the codec probe as functional behavior, not cosmetic logging.
Concerns: **HIGH** exact setter `26` and `_M_X64 == 2` gates are brittle as hard pass/fail;
**HIGH** the probe rewrite risks a new all-or-nothing footgun — it must distinguish *fatal missing core
codec* from *degraded optional effect* (EAX/DS3D) unless wholesale-fail is truly intended;
**MEDIUM** bus_index=0 is UAT-deferred — add a log/runtime observation; **MEDIUM** removing the
SoundObject3d listener gate restores x64 calls — audit that listener creation cannot run before
`Audio::install()` creates the digital driver; **LOW** back "two TUs" with a live `AIL_` grep gate.
Suggestions: replace exact-count gates with semantic gates; classify required vs optional providers and
log exactly which missing filename suppressed audio; add a debug log around room_type during UAT #4.
Risk: **MEDIUM**.

**35-03** — Strengths: defers link validation until vendor+source land; re-adds `mss64.lib` to x64;
knows `/FORCE` makes exit code insufficient (uses unresolved-symbol grep); explicit stale-stage clear +
staged-provider existence checks; x64 PostBuild to `stage-x64/miles` aligns with the x64 stage split.
Concerns: **HIGH** `unresolved external symbol == 0` does not prove the *right* Miles lib was used (only
that symbols resolved — not that they resolved from 9.3b vs stale 7.2e/inherited path);
**HIGH** a 9.3b-only sentinel only repairs a stale 7.2e stage if the PostBuild *deletes/overwrites* the
old set — if old files remain beside new, the probe/loader may find stale providers first;
**MEDIUM** editor/tool vcxprojs still reference old Miles paths (latent breakage + confusing greps);
**MEDIUM** file-exists provider checks don't verify architecture (`dumpbin /headers` mss64.dll);
**MEDIUM** must test both "empty stage" and "dirty stage" PostBuild behavior or the repair path is
theoretical; **LOW** clearing stale stages can hide whether incremental PostBuild repairs a dirty local stage.
Suggestions: add `/VERBOSE:LIB` grep proving `mss32/mss64.lib` came from `miles-9.3b`; repo-wide grep
report of old `library/miles/` refs (blocking vs non-blocking editor refs); PostBuild should remove
known 7.2e-only provider names (esp. `.m3d`) before copying; `dumpbin /headers` on staged DLLs; run one
validation from a deliberately dirty stage. Risk: **MEDIUM-HIGH** — this is where stale paths/state can
produce a clean build but wrong runtime.

**35-04** — Strengths: human UAT correctly blocking; the four D-06 dimensions are the right matrix;
pre-flight "redist missing"/flood census is valuable.
Concerns: **HIGH** "no warning-flood" needs a concrete threshold + time window; **MEDIUM** specify
whether Win32 smoke uses the same 9.3b set and whether old `stage/miles` was purged first;
**MEDIUM** reverb/room-type UAT is subjective — needs a known location/action with an expected
transition; **LOW** confirm `disableMiles=false` in the actual cfg each exe reads, not a template.
Suggestions: define log thresholds (zero "redist missing", zero repeated provider-open failures, no
repeated audio warnings above N in M sec); record exact exe/cfg/stage tuples in the UAT; optional cheap
"negative control" — flip `disableMiles=true`, confirm silence, restore BOM-safely (proves the inspected
cfg is the one the exe reads). Risk: **MEDIUM**.

**Phase-wide** — **HIGH** wrong-lib resolution is the biggest hidden risk (require `/VERBOSE:LIB`);
**HIGH** stale provider files make runtime nondeterministic (repair must *delete* old names);
**MEDIUM** downgrade exact grep counts to review aids unless they check semantic tokens;
**MEDIUM** inventory non-client Miles refs (repoint now or document deferred);
**MEDIUM** bus_index=0 — add logging or a focused reverb scenario; **LOW** plan is artifact-heavy but
justified for binary/runtime-sensitive middleware. **Overall: MEDIUM-HIGH** — directionally strong;
risk is proof-quality around binary provenance and runtime staging, not the API delta.

---

## Cursor Review

**Summary** — Well-researched, mechanically scoped phase with correct wave decomposition, strong
alignment to D-01–D-06, and unusually good executor guidance via PATTERNS.md. Delivers AUDIO-01/02 and
preserves the boot gate **if wave discipline holds**. Residual risk is operational brittleness, not
wrong-API: grep gates that false-fail, Wave-1 parallel execution weakening the x64 compile smoke, an
**overstated Win32 sentinel auto-repair claim**, and a few unstaged ancillary paths.

**Strengths** — correct critical path (vendor → port → link/stage → UAT) matches the real failure modes;
Pitfall 1 (codec probe `.m3d`→`.flt`) explicitly defused; D-03/D-04 separation precise vs live code at
Audio.cpp:1298–1319; `/FORCE` gate correctly applied; stale-state awareness (35-03 mandates deleting the
stage dirs); 35-04 human checkpoint appropriate.

**Top concerns**
- **HIGH (35-02 T3)** — `grep -c "_M_X64" == 2` is brittle and likely wrong. Live Audio.cpp has `_M_X64`
  in Phase-33 *comments* (245, 1309, 1504) that survive stub removal. After Task 3 only the codec-probe
  `#if defined(_M_X64)` should remain — expected **1**, not 2. `!_M_X64` comments also match. Can
  false-fail a correct port or false-pass if the executor "fixes" the count by deleting the wrong thing.
- **HIGH (35-01 T2 / Wave 1)** — **Compile-only smoke on x64 is blind while Phase-33 macro stubs remain.**
  On x64, `#define AIL_set_room_type(dig,rt)` / `#define AIL_room_type(dig)` (lines 268/287) *shadow* the
  9.3b prototypes, so 35-01 can "pass" with unported 2-arg call sites. Win32 (no stubs) catches the
  mismatch; x64 will not until 35-02 removes the block. Parallel Wave 1 makes this worse if 01's SUMMARY
  is read as "x64 header-clean."
- **HIGH (35-03 T2 / PATTERNS §4)** — **"9.3b-only sentinel auto-repairs stale 7.2e Win32 stage" is
  overstated.** Both 7.2e and 9.3b ship `mssds3d.flt`/`msseax.flt` under the same names; a populated 7.2e
  `stage/miles/` will *skip* the new xcopy guard. x64 is fine (`mss64ds3d.flt` is genuinely 9.3b-only).
  Win32 repair depends on Task 3's explicit directory delete, not the sentinel.
- **MEDIUM (35-03 T1/T2)** — `Optimized|Win32` link block lists `mss32.lib` + old miles dirs (~212–214)
  but Task 2 updates PostBuild only for Debug + Release; Optimized has no miles staging at all.
- **MEDIUM** — Wave-1 parallelism lacks an explicit merge gate; recommend sequential 01→02 or "both
  SUMMARYs exist before 03 starts."
- **MEDIUM (35-03 T3)** — `unresolved external symbol == 0` is necessary not sufficient; residual risk is
  a stale `clientAudio` .obj after signature change without full rebuild — plan deletes exe but not
  `/t:Rebuild`/Clean clientAudio.
- **MEDIUM (35-04)** — warning-flood threshold subjective (no numeric gate).
- **MEDIUM (out of plan scope)** — `tools/setup/setup-client.ps1` still validates `.m3d` filenames and
  references `miles-7.2e/redist` (~120–129) → misleading fresh-setup warnings post-port.
- **LOW** — Win32 Debug `TreatWarningAsError=true` on clientAudio: 9.3b header W4 warnings could fail the
  35-01 smoke even when the port is fine; probe set (5 files) ⊂ D-05 staged set (binkawin/dolby/srs
  unchecked); `stage-x64/client_d.cfg` may not exist; editor vcxprojs still link from `miles\lib\win`.

**Suggestions** — replace `_M_X64` count with semantic checks (`"Phase 33: Miles disabled"`==0,
`#if defined(_M_X64)`==1, read each hit) and delete the Phase-33 banner comments (228–245) in Task 3;
reorder Wave 1 (`35-02 depends_on [35-01]`) or move the compile smoke after 35-02; correct the sentinel
docs (x64 sentinel works; Win32 mandatory first-build delete — elevate to MUST); extend Task 2 to cover
Optimized|Win32; harden the build (`/t:Clean` clientAudio + grep verbose for the lib actually consumed);
add a numeric flood gate (`"Unable to create a valid sample id" < N` in first 60s) + verify
`stage-x64/miles/mss64.dll` exists; follow-up ticket for `setup-client.ps1`; log room-type at install.
**Overall: MEDIUM — approve with revisions** to automation gates and Wave-1 ordering.

---

## Opus Review (independent correctness audit)

**Summary** — Unusually high-quality, line-verified, decision-traceable plans built on a research doc
whose central API-delta claim Opus independently confirmed byte-for-byte against both headers. room_type
math **correct** (getter trailing, setter middle); the 26/1 grep counts **match the live file exactly**;
the DAG is sound and the wave assignment valid; AUDIO-01/02 coverage complete. But found **one HIGH
correctness defect**: the plan's Pitfall-2 framing claims mis-positioning the setter `0` is a *compile
error* — it is **not**; it is a **silent wrong-bus call**, which changes how load-bearing the grep gate
is. Plus one HIGH link-soundness gap and two MEDIUM brittleness issues.

**Strengths** — API-delta math re-verified against 9.3b `mss.h:5049-5054` and 7.2e `mss.h:6047-6050`;
grep counts match ground truth (26 setters + 1 getter); risk-isolation sequencing excellent (compile-only
smoke before API edits); codec-probe landmine treated as first-class; `/FORCE` trap internalized.

**Concerns**
- **HIGH (35-02 / Pitfall 2)** — **the "compile error" claim is factually wrong.** `AIL_set_room_type(dig,
  ENVIRONMENT_X, 0)` has the correct arg count (3) and correct types (HDIGDRIVER, S32, S32) — it
  *compiles clean* and silently passes `ENVIRONMENT_X` as `bus_index`, `0` as `room_type` → every room
  sets room_type 0 (no reverb) on a garbage bus. So the grep gate `AIL_set_room_type(s_digitalDevice2d,
  0,` ==26 is the **only** line of defense against a transposed argument — *load-bearing, not cosmetic*.
  The gate as written *does* correctly distinguish middle from trailing; the defect is in the **rationale**.
  Fix the prose AND add an inverse guard: `grep -c "AIL_set_room_type(s_digitalDevice2d, ENVIRONMENT.*, 0)"`
  == 0 (catches the transposed form the compiler will not).
- **HIGH (35-03)** — link gate proves resolution, not provenance. With `/FORCE`, `unresolved==0` proves
  every external was satisfied by *something*, not that AIL_* resolved to mss64.lib (9.3b) vs the wrong
  lib. The project's own **Phase-34 "GetApi dumpbin /exports gate"** is the established positive-presence
  pattern and is not applied. Fix: `dumpbin /imports stage-x64\SwgClient_d.exe | grep -i mss64.dll` (or
  `dumpbin /directives mss64.lib`) to prove the real 9.3b x64 lib resolved the externals. Also: the
  `mss64.lib >= 2` gate is whole-file `grep -c`, not per-config — a Debug-only edit could pass falsely.
- **MEDIUM (35-02 T3)** — the `_M_X64 == 2` gate is off-by-reasoning. Live tokens: 6 (245 comment, 250,
  310, 1306, 1309, 1504); the three disables account for 250/310/1306/1309/1504 and the 245 comment is
  Phase-33 narration → after deletion all 6 are gone. A single `#if/#else/#endif` codec block names
  `_M_X64` **once** → arithmetically the surviving count is **1**, not 2, unless the intentional-marker
  comment contains the literal token. The hard `==2` will false-fail a correct edit depending on comment
  wording. Demote to soft; promote the semantic check (`"Phase 33"`==0 + read-each-match) to primary.
- **MEDIUM (PATTERNS §3)** — 8 editor/tool vcxprojs still reference `miles\lib\win` + `mss32.lib`
  (AnimationEditor, ClientEffectEditor, ShipComponentEditor, SoundEditor, TerrainEditor, Viewer, Turf,
  SwgGodClient). Correctly scoped out now, but a future `git rm` of the old trees breaks all 8 with no
  gate. Record it so the cleanup repoints atomically.
- **LOW** — A1 (bus_index=0) verified only by UAT #4 (acceptable; no static check possible); a benign
  `\b` word-boundary note on the `miles-9.3b\redist` grep (correct as written).

**Suggestions** — (1) correct Pitfall-2 prose + add the inverse transposed-form grep; (2) add the
Phase-34-style dumpbin positive-presence link gate; (3) demote `_M_X64==2` to semantic; (4) make
lib-dir/mss64.lib gates per-config (lib dir lives in 5 configs at 147/214/260/324/381 — verified);
(5) record the 8-file editor-vcxproj latent break.

**DAG & coverage (verified sound)** — wave assignment consistent with edges; 35-01 (vendoring tree +
clientAudio.vcxproj) and 35-02 (Audio.cpp + SoundObject3d.cpp) share **zero files**, no ordering hazard,
truly parallel; every `must_haves.truth` maps to an acceptance criterion; no requirement-coverage gap;
boot gate checked by 35-04 T1.

**Risk: LOW-MEDIUM** — down-rated from LOW only for the two HIGH items (Pitfall-2 misframing leaving the
26-setter edit guarded *only* by an under-sold grep with no inverse check; and the provenance gap vs the
Phase-34 precedent). Both cheap to fix → drops to LOW.

---

## Sonnet Review (lateral / blind-spots)

**Summary** — Well-researched, mechanically-grounded set for a vendor-swap + 28-site edit + staging
change. API delta locked to one family; codec landmine defused; Phase-24 staging reused; wave
decomposition logical. Most consequential gaps: (1) the `_M_X64 == 2` gate is fragile; (2) the
stale-stage "auto-repair" claim is only partially true — xcopy *adds* 9.3b files but does **not delete**
stale 7.2e files; (3) the "delete stage/miles first" instruction lives only in prose, not in any
acceptance criterion; (4) wave 1 commits non-linking source.

**Strengths** — airtight API-delta research; codec landmine addressed head-on; `/FORCE` trap documented
at every step; Phase-24 PostBuild is the right model; atomic/reviewable wave ordering; D-04 KEEP
reinforced everywhere; force-add gate present with `git ls-files`; human-checkpoint structure correct.

**Concerns**
- **HIGH (35-02 T3)** — `_M_X64 == 2` fragile (same mechanism as Cursor/Opus): line 245 comment survives
  if the executor deletes only 250–310 → count 2; if they delete the whole 227–249 banner too → count 1
  → gate fails; if they annotate `#endif // _M_X64` → count 3 → gate fails. Replace with content gates.
- **HIGH (35-03 T3)** — xcopy sentinel "auto-repairs" but `xcopy /E /I /Y` does **not delete** files
  already in the destination. A pre-existing `stage/miles/` with `Msseax.m3d`/`msssoft.m3d` STILL CONTAINS
  them after the 9.3b xcopy → the acceptance check `find ... -iname '*.m3d' == 0` **will fail**. The
  delete-before-build step is **prose in `<action>`, not in `<acceptance_criteria>`** → skippable.
- **HIGH (35-02)** — plan commits non-linking source to master; `autonomous: true` means an executor can
  commit 02 without 03, leaving the x64 link broken (removing stubs without adding mss64.lib). Violates
  the boot-gate intent for a mid-phase interruption. Add a commit-sequencing note: don't push 02 without
  03 (or fold the stub deletion into 03 after the mss64.lib add).
- **MEDIUM (35-02)** — total `AIL_set_room_type` occurrences = 27 (26 sites + the stub `#define` at 287);
  the gate targets `(s_digitalDevice2d,` so 26 is correct, but note the discrepancy so the executor
  doesn't second-guess.
- **MEDIUM (35-04 T1)** — no check that `AIL_open_digital_driver` actually succeeded (DLL load-path
  resolution differs between `stage/` and `stage-x64/` CWD; device init can fail). Add a boot-log grep for
  the "Audio: Finished initializing" success line (~1496) and assert "Audio: Miles is disabled" is absent.
- **MEDIUM (35-03 / 35-01)** — `mssvoice.asi` is in the 7.2e redist (vox tree) and currently staged, but
  35-01 T1's gather list omits `win/vox/redist/mssvoice.asi` → silently dropped from the 9.3b stage
  (regression for any voice path). Either add it or record the intentional exclusion.
- **MEDIUM (35-01 T2)** — Win32 Debug `TreatWarningAsError=true`: a W4 warning *inside* mss.h/rrcore.h is
  upgraded to error, indistinguishable from a real header-incompat. Add a sub-case: check for `warning
  C####` before concluding incompat; respond with a `#pragma warning(push/disable)` wrapper.
- **MEDIUM** — editor vcxprojs latent break (same as others).
- **LOW** — getter grep/task-order benign; no lib file-size check in 35-01 (LNK1181 catches it later);
  35-04 T1 automated boot may fail with no display (specify the Windows-MCP launch path or a manual
  fallback so the pre-flight is non-skippable).

**Suggestions** — (1) replace `_M_X64==2` with content gates; (2) add a **mandatory delete acceptance
criterion** for the stage dirs in 35-03 T3; (3) add a commit-sequencing note to 35-02; (4) decide
`mssvoice.asi` explicitly; (5) add the driver-open-success log grep to 35-04 T1; (6) add the
TreatWarningAsError handling note to 35-01 T2; (7) document the editor-vcxproj repoint as a deferred TODO.
**Risk: MEDIUM** — risks are execution-mechanics, not design; suggestions 1–3 drop it to LOW.

---

## Consensus Summary

### Orchestrator ground truth (verified against live source before adjudicating)

- **room_type counts are CORRECT.** 26 setter sites (`AIL_set_room_type(s_digitalDevice2d, …)` at
  Audio.cpp:3919–3944) + 1 getter (3957). The 35-02 gates (`==26` / `==1`) match reality. The two extra
  occurrences in a naive grep are the macro-stub `#define`s at 268/287 (deleted by 35-02 T3). ✓
- **`_M_X64` live count = 6** (Audio.cpp:245, 250, 310, 1306, 1309, 1504). The 35-02 T3 delete ranges
  (250–310, 1306–1504) leave the **comment at 245 outside the block** → the `==2` gate is genuinely
  ambiguous. **Consensus across all 4 reviewers: make it semantic.** ✓
- **DIVERGENCE RESOLVED (Cursor right, Opus/plan wrong):** `src/external/3rd/library/miles-7.2e/redist/`
  contains **both** `mssds3d.flt` AND `msseax.flt` (alongside `Msseax.m3d`/`msssoft.m3d`). Therefore a
  Win32 9.3b-only sentinel keyed on those `.flt` names will **NOT** detect a stale 7.2e stage — they
  already exist there. The RESEARCH claim "7.2e ships .m3d; 9.3b does not" is **true but incomplete**
  (7.2e ships the .flt files too). Only the explicit `Remove-Item stage/miles` repairs Win32; x64 is fine
  (`mss64*.flt` names are genuinely 9.3b-only). **This makes the missing delete-acceptance-gate a real
  defect, not a nicety.** ✓

### Agreed Strengths (2+ reviewers)
- Correct critical path / wave decomposition (vendor → port → link/stage → UAT) matching the real failure
  modes. (all 4)
- Codec-probe `.m3d`→`.flt` landmine identified and defused as a first-class required edit. (all 4)
- `/FORCE`-masks-LNK2019 trap internalized — grep for `unresolved external symbol`, not exit code; delete
  exe to force relink. (all 4)
- D-03/D-04 separation precise and verified against live code (delete the three Phase-33 disables, KEEP
  the retail `disableMiles` cfg). (all 4)
- room_type middle-vs-trailing distinction correct; grep counts match live source. (Opus + orchestrator)
- Force-add past the `*.lib` gitignore with `git ls-files` confirmation. (Codex, Sonnet)
- Human checkpoint correctly placed as the only honest AUDIO-02 runtime proof. (all 4)

### Agreed Concerns (highest priority — ranked by consensus weight)

1. **[HIGH — all 4] The `_M_X64 == 2` grep gate (35-02 T3) is brittle.** Replace the exact count with
   semantic gates: `grep "Phase 33: Miles disabled"` ==0 + a gate on the install()-block Phase-33 comment
   text ==0 + read each surviving `_M_X64` and confirm it belongs only to the codec probe. Delete the
   Phase-33 banner comment (Audio.cpp ~227–249) as part of Task 3 so it doesn't pollute the grep.

2. **[HIGH — Codex, Cursor, Opus] `unresolved external symbol == 0` proves resolution, not provenance.**
   Under `/FORCE` a clean grep does not prove AIL_* resolved from 9.3b mss64.lib vs a stale/wrong lib.
   Add the project's established **Phase-34 positive-presence pattern**: `dumpbin /imports
   stage-x64\SwgClient_d.exe | grep -i mss64.dll` (and/or `/VERBOSE:LIB` grep, `dumpbin /headers` to
   confirm machine type of the staged DLLs). Make the `mss64.lib`/lib-dir gates **per-config**, not
   whole-file `grep -c`.

3. **[HIGH — Codex, Cursor, Sonnet; confirmed by orchestrator] Stale-stage repair is overstated and the
   delete is not gated.** `xcopy` adds 9.3b files but does not remove stale 7.2e files, and (verified) the
   Win32 9.3b-only sentinel cannot detect a stale 7.2e stage because 7.2e ships the same `.flt` names.
   **Promote the `Remove-Item stage\miles\ + stage-x64\miles\` step from prose to a hard acceptance
   criterion in 35-03 T3** (it is the only thing that actually repairs Win32, and the `*.m3d == 0` gate
   will fail without it).

4. **[HIGH — Opus standout] Pitfall-2 "compile error" claim is factually wrong.** A transposed setter
   `AIL_set_room_type(dig, ENVIRONMENT_X, 0)` compiles clean (3 args, all S32) and is a silent wrong-bus
   call. The `==26` grep is therefore load-bearing. Correct the prose AND add the inverse guard
   `grep -c "AIL_set_room_type(s_digitalDevice2d, ENVIRONMENT.*, 0)"` == 0.

5. **[HIGH — Cursor standout] 35-01 x64 compile-smoke is blind while Phase-33 macro stubs remain.** The
   `#define AIL_(set_)room_type` stubs at 268/287 shadow the 9.3b prototypes on x64, so the smoke can pass
   with unported call sites. Scope 35-01's x64 smoke to "no in-header errors only" and run the real
   call-site compile after 35-02 removes the stubs (or sequence 35-02 after 35-01 and recompile).

6. **[HIGH — Sonnet; MEDIUM — Cursor] Wave-1 commits a non-linking tree.** `autonomous: true` on 35-02
   plus commit-per-plan can push a broken x64 link to master between waves. Add a commit-sequencing note
   (don't push 02 without 03) or fold the stub deletion into 03 after the mss64.lib add.

7. **[MEDIUM — all 4] bus_index=0 (A1) is UAT-only.** Add an install-time debug log around
   `AIL_set_room_type(dig, 0, …)` / `AIL_room_type(dig, 0)` and a known cell-transition UAT scenario so
   reverb is not the only signal.

8. **[MEDIUM — all 4] Editor vcxprojs (8 files) still point at the old miles tree.** Latent LNK1181 if the
   old trees are ever `git rm`'d. Record an explicit deferred TODO to repoint them atomically with the cleanup.

9. **[MEDIUM — Codex, Cursor] Warning-flood threshold is subjective.** Define a numeric gate (e.g.
   `"Unable to create a valid sample id" < N` in the first 60s to char-select) for 35-04 T1.

10. **[MEDIUM — Cursor, Codex] No clientAudio rebuild/clean after the signature change** — stale `.obj`
    risk; add `/t:Clean` (or delete clientAudio output) before the 35-03 link.

11. **[MEDIUM — Codex] Codec probe should classify required-core vs optional providers** (don't fail audio
    wholesale if EAX/DS3D is absent); log exactly which missing filename suppressed audio.

12. **[MEDIUM — Sonnet, Codex] No check that `AIL_open_digital_driver` actually succeeded on x64** — add a
    boot-log grep for the init-success line + assert "Miles is disabled" absent (distinguishes
    driver-opened from driver-failed-silently; Open Question 1).

### Divergent Views (worth investigating)
- **Win32 sentinel auto-repair** — Opus (and the plan/research) said the 9.3b-only sentinel auto-repairs a
  stale 7.2e Win32 stage; Cursor said it cannot. **Orchestrator verified Cursor is correct** — 7.2e ships
  `mssds3d.flt`/`msseax.flt`. Resolution: rely on the explicit delete for Win32, not the sentinel.
- **`_M_X64` surviving count** — Cursor/Opus argue the arithmetically-correct count is **1** (codec `#if`
  names the token once); the plan asserts **2** (assuming an intentional-marker comment containing the
  token). Both agree the fix is to stop gating on an exact number. Net: semantic gate, drop the count.
- **Single-reviewer items worth a look:** `setup-client.ps1` still validates `.m3d`/`miles-7.2e` (Cursor);
  `mssvoice.asi` silently dropped from the 9.3b gather list (Sonnet); record SHA256/size/machine-type for
  the vendored libs (Codex); `TreatWarningAsError` handling for the Win32 header smoke (Cursor, Sonnet);
  `stage-x64/client_d.cfg` bootstrap + a `disableMiles=true` negative control (Cursor, Sonnet, Codex).

### Overall risk (reviewer verdicts)
Codex **MEDIUM-HIGH** · Cursor **MEDIUM** · Opus **LOW-MEDIUM** · Sonnet **MEDIUM**. **Consensus: MEDIUM**
— the design is sound and AUDIO-01/02 coverage is complete; residual risk is execution-mechanics and
proof-quality (gates that can lie about provenance and stale state), not the API delta. Applying
consensus concerns 1–6 brings it to **LOW** before execution.

---

To incorporate this feedback into planning:
  /gsd-plan-phase 35 --reviews
