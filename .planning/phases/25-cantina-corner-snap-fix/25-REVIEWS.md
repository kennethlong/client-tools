---
phase: 25
reviewers: [codex, cursor]
reviewed_at: 2026-06-13T03:28:04Z
plans_reviewed: [25-01-PLAN.md]
self_skipped: claude (running inside Claude Code — skipped for independence)
---

# Cross-AI Plan Review — Phase 25 (Cantina Corner-Snap Fix)

Two independent external reviewers (Codex, Cursor) assessed `25-01-PLAN.md`.
Both converged on **MEDIUM overall risk**: the guard design is correct in shape,
but the **acceptance metric** and the **static-map keying/pruning** need
tightening before execution.

---

## Codex Review

## Summary

The plan is directionally strong and narrowly scoped: it targets the diagnosed same-frame A→B→A re-entrant portal reversal without imposing a broad per-frame transition cap, preserves the CORNERSNAP harness, and requires dual-renderer behavioral verification. The core mechanism is plausible, but I would not execute it exactly as written without tightening two design points: the static map pruning strategy does not actually bound growth, and keying only by `NetworkId` plus cell indices has avoidable false-positive/collision risks. The plan also needs clearer acceptance around "suppressed attempted reversals" versus "logged portal transitions," because the proposed placement logs the suppressed candidate before blocking it.

## Strengths

- The fix is scoped to the right function: `CellPropertyNamespace::Notification::positionChanged`, immediately around the `targetCell` / `setParentCell` decision point.
- The plan correctly avoids a blanket "one transition per frame" cap. That is important because the existing portal walk can legitimately traverse chained cells, and some fast movement can produce non-reversing transitions in one frame.
- The guard condition is specific: same object, same frame, exact `(from,to)->(to,from)` reversal. That is the right shape for the diagnosed ping-pong.
- Suppressing the second transition and leaving the object in the first crossed cell is likely the correct resting state for the diagnosed failure, because the first `setParentCell(targetCell)` is the transition caused by the player's actual movement segment; the re-entry is the artifact.
- Preserving the `_DEBUG` CORNERSNAP blocks until Phase 26 is the right sequencing. The instrumentation is the acceptance harness for this phase.
- Dual-renderer verification is correctly required. Even though this is engine traversal logic, the project invariant is boot and behavior under both `rasterMajor=5` and `rasterMajor=11`.
- The plan explicitly leaves collision pushback and camera hysteresis out of scope, which prevents the phase from expanding into unrelated movement/camera work.

## Concerns

- **HIGH: Lazy pruning as described does not bound the static map.**
  "Drop any entry whose recorded frame != current frame when that object is next seen" only bounds records for objects that move again. Any unique object that moves once and never re-enters the notification path leaves an entry forever. Over a long session this can grow to every NetworkId ever observed. Since the guard only needs current-frame history, clearing the whole map on frame change is safer and simpler.

- **MEDIUM: `NetworkId` alone may be an unsafe object key for client-only or invalid-ID objects.**
  The plan explicitly mentions camera `NetworkId 0`, and other local/transient/client-only objects may share invalid/default IDs. A shared key can cause cross-object suppression in the same frame. For player-only behavior this may not show up in the acceptance test, but it is risky for a production guard.

- **MEDIUM: Cell index comparison is weaker than pointer comparison.**
  `getCellIndex()` is convenient for logs, but runtime identity should prefer `CellProperty const *` pointers, or a composite including the cell owner identity. If indices are reused across different POBs or world/building contexts, a same-frame sequence on the same object could be falsely classified as a reverse.

- **MEDIUM: Acceptance metric may miss remaining attempted reversals.**
  The plan places the guard after the existing CORNERSNAP-PORTAL log, so a suppressed B→A candidate will still be logged. The expected post-fix signature may therefore be "two logged attempted transitions, zero recursive >=3 ping-pong frames," not "no reversal log." That is acceptable if intentional, but the plan should state this clearly.

- **MEDIUM: Return value on suppressed reversal deserves explicit justification.**
  Returning `false` matches the existing crossed-portal path, but in the suppressed path no new `setParentCell()` / `cellChanged()` follows. That may be right for a re-entrant notification, but the plan should explicitly verify that stopping the remaining notification list does not drop required side effects.

- **LOW: Same-frame legitimate A→B→A is intentionally blocked.**
  The plan preserves A→B→C, but a very fast actual backtrack through the same portal in the same frame would be suppressed. That is probably acceptable given frame granularity and the bug shape, but it should be acknowledged as the tradeoff.

- **LOW: Frontmatter dependency is inconsistent.**
  The roadmap says Phase 25 depends on Phase 24, but the plan frontmatter has `depends_on: []`. If that field is only intra-phase plan dependency, fine; otherwise it should name Phase 24 or the clean dual-renderer baseline.

- **LOW: Build verification should include the project's `/FORCE` link-grep gate.**
  MSBuild success alone is not enough in this repo. The plan should require zero `unresolved external symbol` in link output.

## Suggestions

- Replace lazy per-object pruning with frame-wide reset:

  ```cpp
  static int s_lastFrame = -1;
  static TransitionMap s_transitions;

  int const currentFrame = Os::getNumberOfUpdates();
  if (s_lastFrame != currentFrame)
  {
      s_transitions.clear();
      s_lastFrame = currentFrame;
  }
  ```

- Store `CellProperty const *fromCell` and `CellProperty const *toCell` in the guard record. Keep cell indices only for diagnostics.

- Use a safer key than raw `NetworkId` alone. Options:
  - key by `Object const *`, since the guard is frame-local;
  - or key by `(NetworkId value, Object const *)`;
  - or use `NetworkId` only when valid, falling back to pointer for invalid/default IDs.

- Add a small non-spammy `_DEBUG` log for suppressed reversals, separate from CORNERSNAP-PORTAL, or update the verification script to classify attempted reversals explicitly. This makes "guard fired" auditable without relying only on absence of 3+ recursion.

- Tighten Task 2's parser definition:
  - group by frame and object key;
  - classify direct adjacent reverse pairs;
  - report total portal logs, suppressed reversals if logged, and frames with `>=3` transitions;
  - require `>=3` frames to be zero under both renderers.

- Expand manual regression slightly beyond cantina entry/exit:
  - normal enter/exit;
  - fast run through a chained doorway/foyer;
  - deliberate wall-graze at the known corner;
  - attempt to reverse direction at a doorway to check for obvious stuck-cell behavior.

- Add source-level verification that the guard is outside `_DEBUG`, that the existing CORNERSNAP log still compiles, and that the only `setParentCell` call remains the existing one.

## Risk Assessment

**Overall risk: MEDIUM.**

The proposed behavioral fix is well-targeted and likely to solve the diagnosed recursive A→B→A snap. The main risk is not the concept; it is implementation detail: a static map with weak keying and incomplete pruning can introduce long-session state growth or false suppression across invalid-ID objects/cell-index reuse. Tightening the key to object/frame plus cell pointers and clearing on frame change would reduce the implementation risk substantially. Verification is mostly sufficient, but it should distinguish "suppressed attempted reversal still logged" from "actual recursive ping-pong still happening."

---

## Cursor Review

## 1. Summary

This is a **well-scoped, mechanism-aligned plan** for a root-caused engine bug. The reversal guard (same frame, same object, exact `(from,to)` ↔ `(to,from)`) is the right shape: it targets the diagnosed A→B→A ping-pong without a blanket per-frame cap, and keeping the first transition's cell is the correct resting state. The plan maps cleanly to the actual code in `CellProperty.cpp` and the 2026-06-12 capture. The main weaknesses are **verification metric design** (logging before suppression can still produce ≥3 `CORNERSNAP-PORTAL` lines per frame even when the fix works), a **slightly imprecise re-entry path description**, and **no Release-build behavioral check** despite shipping the guard outside `_DEBUG`.

## 2. Strengths

- **Root cause → fix alignment.** The plan correctly identifies that `dueToParent` does not catch re-entrant `positionChanged` calls, and places the guard at the single `setParentCell` call site in `Notification::positionChanged`.
- **Criterion #2 is explicitly designed for.** Reversal detection preserves forward chains (A→B then B→C); a per-frame cap is explicitly rejected with good reasoning from the baseline capture.
- **Minimal blast radius.** Single file, single function, production guard + preserved `_DEBUG` harness for Phase 26 removal — good dependency ordering with Phase 24/26.
- **Correct resting state.** Suppressing the *second*, reversed transition and leaving the object in cell B (the first crossing) matches intended movement; the visible snap in frame 13019 comes from the 3.6 m reversal segment, not from staying in B.
- **Baseline is concrete.** Frame 13019 shows the exact signature (forward 2→3, reverse 3→2, partial re-cross).
- **Deferred camera churn is sensible.** NetworkId 0 flapping at doorways is a separate hysteresis problem and correctly out of HARD-02 scope.
- **Threat model is proportionate** for an engine-internal, single-threaded client fix.

## 3. Concerns

| Severity | Concern |
|----------|---------|
| **HIGH** | **Acceptance metric may false-fail.** The guard runs *after* the `CORNERSNAP-PORTAL` log. Suppressed reversals still emit log lines. Frame 14895 shows **5 player portal logs in one frame**, interleaved with `CORNERSNAP-RESOLVE` collision replay — multiple `setPosition_p` calls can each hit the portal walk and log before suppression. Post-fix you could have zero visible snap and zero *applied* ping-pong, but still ≥3 log lines/frame from suppressed attempts. |
| **MEDIUM** | **Single-reversal guard vs. multi-hop ping-pong.** Baseline shows 3–5 transitions/frame. The guard only compares against the *last* recorded pair. That should break the *apply* chain after one forward + one suppressed reverse, but it does not guarantee ≤2 log lines if collision replay keeps re-firing `positionChanged` at a portal plane in the same frame. |
| **MEDIUM** | **No Release verification.** Guard ships in non-`_DEBUG` code, but Task 2 only exercises `SwgClient_d.exe`. A brief Release cantina pass (no log metric, visual + entry/exit only) would close the "ships in Release" gap. |
| **MEDIUM** | **Re-entry path is slightly misdocumented.** Plan says `setParentCell → cellChanged → attachToObject_w` re-fires `positionChanged`. In code, `attachToObject_w` and `cellChanged(false)` do **not** call `positionChanged`; re-entry almost certainly comes from subsequent `setPosition_p` / collision replay (`CollisionResolve.cpp:337`) after reparenting. Mechanism conclusion is still right; implementation note could mislead the executor. |
| **LOW** | **`std::map` + lazy prune is fine but underspecified.** Pruning only on next sighting of a NetworkId leaves dormant entries until reuse. Harmless at client scale, but a frame-boundary sweep (clear entries where `recordedFrame != currentFrame` once per frame) would be simpler and tighter. |
| **LOW** | **Legitimate same-frame A→B→A.** A player straddling a portal and genuinely oscillating in one frame would have the return crossing suppressed. Unlikely and probably desirable, but worth noting in SUMMARY. |
| **LOW** | **Cell indices are POB-local, not global.** Acceptable for reversal matching within one portal walk (same building), but the plan should not assume indices are globally unique across the world. |
| **LOW** | **Thread safety.** Static map with no locking is acceptable on the single-threaded game loop; would be unsafe if portal notifications ever ran off-thread. |

## 4. Suggestions

1. **Fix the acceptance metric before execution.** Either: add a **non-CORNERSNAP** `_DEBUG` counter/log for *applied* vs *suppressed* transitions; or change criterion #3 to count **applied** transitions only; or define pass as **zero frames with ≥2 *consecutive alternating* portal pairs** `(A→B)(B→A)` for the player.
2. **Clarify expected post-fix log shape.** Document that 1–2 `CORNERSNAP-PORTAL` lines per bad frame may still appear (forward + suppressed reverse attempt); success is **no ≥3 applied crossings** and no visible snap, not necessarily zero log lines.
3. **Add a lightweight Release smoke step** to Task 2: build `Release|Win32`, cantina wall-graze under gl05/gl11, confirm no snap and normal entry/exit (no log dependency).
4. **Consider recording on suppress without applying** so repeated collision waypoints in the same frame don't re-log/re-evaluate the same reversal — only if post-fix captures show log-count false failures.
5. **Provide a counting script** in the plan (PowerShell/Python one-liner) so Task 2's "≥3 transitions per frame" is reproducible, not ad hoc.
6. **Explicit fast-door test case** in Task 2: run through cantina **entrance → foyer chain → interior** at speed and confirm landing cell is correct.
7. **SUMMARY should note** that collision replay (`CORNERSNAP-RESOLVE`) co-occurs with portal ping-pong (frame 14895) even though `CELLJUMP` was zero — the guard fixes portal apply, not collision rewind itself.

## 5. Risk Assessment

**Overall: MEDIUM.** Guard logic correctness is Low–Medium risk; regression risk (fast traversals) is Low; **verification false-negatives are Medium–High** because the ≥3 log-line metric is fragile (logs precede suppression, collision replay multiplies portal-walk invocations per frame); scope/boot-gate risk is Low; stuck-in-cell risk is Low if the same-frame-only rule is implemented faithfully.

**Bottom line:** Proceed with this plan's guard design; tighten Task 2's acceptance metric (or add an applied-transition counter) so Phase 25 doesn't false-fail when the snap is fixed but logs still show suppressed portal-walk attempts.

---

## Consensus Summary

Both reviewers independently rate the plan **MEDIUM risk** and endorse the core
guard *design* — same-object, same-frame, exact `(from,to)↔(to,from)` reversal
detection, resting in cell B, no blanket per-frame cap. The objections are all
about **implementation detail and verification**, not the concept. The plan is
sound to execute *after* addressing the two HIGH-consensus items below.

### Agreed Strengths (raised by both)
- **Right scope / right function.** Guard belongs at the single `setParentCell`
  decision point in `Notification::positionChanged`; minimal blast radius (one
  file, one function).
- **Reversal detection over a per-frame cap** correctly satisfies criterion #2
  (preserves legitimate forward A→B→C fast traversals).
- **Resting state B is correct** — the first crossing reflects real movement; the
  reverse re-entry is the artifact.
- **CORNERSNAP harness preserved → Phase 26 removal** is the right sequencing.
- **Dual-renderer (gl05/gl11) verification** correctly honors the project invariant.
- **Camera (NetworkId 0) churn correctly deferred** — separate hysteresis concern.

### Agreed Concerns (raised by both — highest priority)

1. **[HIGH consensus] Acceptance metric is fragile / can false-fail.**
   The guard runs *after* the `CORNERSNAP-PORTAL` `_DEBUG` log, so a *suppressed*
   reversal still emits a log line. Collision replay can fire the portal walk
   several times per frame (Cursor cites frame 14895 = 5 player portal logs).
   Result: post-fix you can have zero visible snap and zero *applied* ping-pong
   yet still ≥3 *logged* lines/frame — failing criterion #3 spuriously. **Fix
   before execution:** count *applied* (not logged) transitions, or add a
   separate non-CORNERSNAP applied/suppressed counter, or redefine the pass
   condition as "no ≥2 consecutive alternating `(A→B)(B→A)` applied pairs."

2. **[HIGH/MEDIUM consensus] Static-map growth + keying.**
   - *Pruning:* lazy "prune when next seen" does **not** bound the map — an object
     that moves once and never returns leaks forever (Codex: HIGH; Cursor: LOW).
     Both recommend **clear-the-whole-map on frame change** instead (Codex gives
     the exact `s_lastFrame`/`clear()` snippet).
   - *Key:* raw `NetworkId` is unsafe for invalid/default-ID objects (camera = 0,
     client-only/transient objects) → cross-object suppression. Prefer
     `Object const *` (frame-local) or a composite key.
   - *Cell identity:* `getCellIndex()` is POB-local and reusable across buildings;
     prefer `CellProperty const *` pointers for the from/to comparison, keep
     indices for diagnostics only.

3. **[MEDIUM consensus] No Release-build behavioral check.**
   The guard ships outside `_DEBUG` but Task 2 only exercises the Debug exe. Add a
   brief `Release|Win32` cantina smoke pass (visual + entry/exit, no log metric).
   Codex frames the adjacent point as: add the repo's `/FORCE` link-grep gate
   (zero unresolved externals), not just MSBuild exit 0.

### Divergent / Single-Reviewer Findings (worth investigating)
- **Re-entry path may be misdocumented (Cursor, MEDIUM).** The plan's
  `<interfaces>` claims `setParentCell → cellChanged → attachToObject_w` re-fires
  `positionChanged`. Cursor asserts `attachToObject_w`/`cellChanged(false)` do
  **not** call `positionChanged`; the real re-entry is a later `setPosition_p` /
  collision replay (`CollisionResolve.cpp:337`) after reparenting. The *conclusion*
  (same-frame reversal) holds, but the executor's mental model could be wrong —
  **verify the actual re-entry call chain against source before/at execution.**
- **Return-value side-effects (Codex, MEDIUM).** On the suppressed path no new
  `setParentCell`/`cellChanged` follows yet it returns `false` (consumes the
  notification). Codex wants explicit confirmation that halting the remaining
  notification list drops no required side effects.
- **Frontmatter `depends_on: []` vs roadmap "depends on Phase 24" (Codex, LOW).**
  Reconcile or document that the field is intra-phase only.
- **Intentional suppression of a genuine same-frame A→B→A backtrack (both, LOW).**
  Acknowledge the tradeoff in the SUMMARY; considered acceptable at frame
  granularity.

### Recommended Pre-Execution Edits (net of both reviews)
1. Redefine criterion #3 around **applied** transitions (or add an applied/
   suppressed counter outside the frozen CORNERSNAP blocks) + supply a concrete
   counting script for Task 2.
2. Switch the guard to **clear-map-on-frame-change** and key on
   `Object const *` (or composite) with **`CellProperty const *` from/to** rather
   than raw NetworkId + cell indices.
3. Add a **Release smoke pass** + the **`/FORCE` zero-unresolved-externals** gate
   to Task 2's build step.
4. **Re-verify the re-entry call chain** in source and correct the `<interfaces>`
   note (Cursor's CollisionResolve hypothesis); justify the suppressed-path
   `return false` side-effect contract.
5. Note the deliberate same-frame-backtrack tradeoff and the depends_on
   reconciliation in the plan/SUMMARY.

**No reviewer recommends abandoning or re-architecting the plan.** Both say
*proceed with the guard, tighten verification + keying first.*

---

To incorporate this feedback into planning:
  /gsd-plan-phase 25 --reviews
