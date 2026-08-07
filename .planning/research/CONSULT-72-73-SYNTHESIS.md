# CONSULT-72 / 73 — SYNTHESIS: the editor-scene world-loss round (2026-08-07)

**Outcome:** defect diagnosed and fixed (`f9ce87a21`), live-verified with exact accounting. Four
consultants; **three leading mechanisms were killed by measurement**, including two of mine. The
winning mechanism was sitting in a Cursor table hours before anyone assembled it.

Kept because the *process* is more reusable than the answer.

---

## 1. The defect, finally

An editor scene entered via `game::loadScene` **after a server login** came up with most of the world
missing. Two mechanisms, both correct in isolation, both wrong on same-scene re-entry:

1. **Stripped** — `suppressObject` (server copy supersedes the snapshot copy) and failed creates
   (`:1373-1375`) drop a node's sphere handle.
2. **Never indexed** — the `PP_sphereTree` gate (`:1034-1040`) skips buildout POB roots when not
   single-player.

`load()`'s same-scene early return (`:676-680`) skips the re-parse that would rebuild the index, and
only `engine_wsUnloadSnapshot` clears `ms_sceneName`. Fix = re-evaluate the gate under the current
mode on that branch. Class 2 emptied the city; class 1 removed authored buildings.

## 2. Four hypotheses died. How each died is the useful part.

| # | Hypothesis | Killed by | Lesson |
| --- | --- | --- | --- |
| 1 | **v33 `loadScene` teardown** (mine) — missing scene teardown → double `ClientWorld::install()` → `CEC_objectAlreadyExists` → no POB in tree | Consumer ran the direct test: made the teardown fire; reading was byte-identical | A story that explains every observation is not a tested mechanism |
| 2 | **Stale `realSphere` pruning** (Sonnet) — object in the tree, unreachable behind a cached ancestor bound | `tree=1/0/9` — you cannot prune your way to empty with 1 object | Sonnet named its own falsifier in advance; that is what made it cheap to kill |
| 3 | **Player-at-origin streaming** (Opus) — `loadScene` spawns at `(0,0,0)`, 5.9 km outside the 512 m reach | Fable, from the existing log: 103 s inside the cantina with no create, while a control building healed in 6 s | Made a **pre-registered numeric prediction that matched** (`1/0/9`) and was still wrong. Matching a prediction is not proof when a rival predicts the same number |
| 4 | **`clearSphereTree` stale handles** (Codex, Cursor, consumer, me) | Cannot fire on the shipping path — `cleanupScene` nulls without deleting, so `~GroundScene` never runs — yet the symptom persists | Independent convergence by four parties is not evidence. It was the *available* idea, not the right one |

**Every one of these was killed by a measurement, never by argument.** The standing rule held:
consensus ≠ correctness.

## 3. What each consultant was actually worth

- **Codex** — complete call graph and gate list. Independently found the `clearSphereTree` trap
  (withheld from the brief). Its enumeration was the map everything else was checked against.
- **Cursor** — **had the answer and I missed it.** His §2 and §5 tables named *both* inflictors
  (`suppressObject` via `GroundScene.cpp:2528-2544`; the `:1373-1375` strip) hours before Fable
  assembled them. His *opinion* section pointed elsewhere. See §4.
- **Sonnet (lateral)** — forced the round to stop treating `candidates=0` as proof of absence, by
  proving `findInRange` prunes on a **cached** ancestor bound while testing leaf contents live. Wrong
  mechanism, essential correction.
- **Opus (lifecycle)** — produced the confound that killed the reload A/B: the failed teleport *also*
  moved the player 5.9 km, so "reload repairs it" was never controlled. That fact had anchored
  everyone's reasoning, mine most of all.
- **Fable (adversarial)** — killed Opus using the existing log, then in CONSULT-73 caught that
  class 2 exists at all. A strips-only fix would have left the city broken.

**The disagreement was the mechanism of progress.** Every round where they converged, they converged
on something false.

## 4. My errors, and the single cause

Three wrong mechanisms in one day. The recurring cause is the one already in the notes — *reasoning
from a name or shape instead of reading what the thing does* — plus one new failure mode:

1. **v33 teardown asserted as "root-caused"** on an untested chain. Two true facts (the reload works;
   `wsUnloadSnapshot` is the only thing clearing `ms_sceneName`) made a false story feel proven.
2. **Read Cursor's opinion instead of his tables.** I explicitly asked him to quarantine his opinion,
   then privileged the quarantined section over the evidence I had asked for. Cost most of the round.
   → memory updated: `feedback_cursor_dissent_high_hit_rate`.
3. **Proposed a fix that would not have worked** — "clear `ms_sceneName` so `load()` rebuilds." Cursor:
   the rebuild is in the *parse*, not the name; clearing it alone rebuilds nothing.
4. **Accepted "never created" while knowing decorations rendered.** Cursor and Fable both flagged the
   contradiction; I had the fact in hand and did not reconcile it.

## 5. The pattern this round is really about

**A diagnostic that exists, is correct, and never reaches a human.** Third and fourth instances in
three days, and both were mine:

- `WorldSnapshot::suppressObject` — strips a node from the index **permanently**, in **total
  silence**, in every build. The effect was measurable for days while the cause left no trace at all.
- The `:1373-1375` strip — permanent, with every diagnostic under it a `DEBUG_WARNING`, i.e. nothing
  in Release.
- (2026-08-06: `ClientInteriorLayoutManager` — a Release crash whose only diagnostic was compiled out.)

All three now emit `REPORT_LOG`. **The fix's instrumentation was specified as part of the fix, not an
extra** — and it is what turned verification from "the building looks like it's there" into
`288 + 34 = 322` with zero post-re-arm failures.

## 6. Verification worth imitating

The confirming experiment was **pre-registered and binary**: *fresh process, never log in, editor
scene immediately → is Mos Eisley intact?* Predicted before running, unambiguous on sight, and it
needed no code change. After a day of hypotheses that fit the evidence afterwards, the thing that
settled it was one prediction made in advance.

The acceptance run then closed with exact arithmetic: every re-armed node accounted for by a logged
strip, no remainder.

## 7. Prior art (checked, on the maintainer's instinct)

`SwgGodClient` (`load("")`/`load(sceneId)`), Utinni (`wsUnloadSnapshot` + `load`), and SWG-Toolkit
(`Reload scene`) **all work around this with a forced full rebuild; none fixed it.** Utinni's own
comment calls the sequence *"the correct one-click reload"* — the workaround was so natural that
nobody saw the bug under it, us included (we built that shim).

Decisively, **all three expose the rebuild as an explicit user action**, never implicit on scene
load. That killed the forced-re-parse option: `unload()` calls `ms_reader.clear()`, destroying
unsaved editor edits. Prior art argued *against* the option it superficially endorsed.

## 8. Standing items

- `feedback_cursor_dissent_high_hit_rate` — read his enumeration first, mine it against the open
  question, treat his conclusion as one guess.
- Two real defects found in passing, **neither fixed**: `m_distanceSquaredTo` is never recomputed in
  the live diff branch (`:1227-1282`), so the snapshot **delete drain has been a no-op** and snapshot
  objects are never streamed out — memory grows with everywhere the player has been. And
  `detailLevelChanged` re-adds only nodes that already had handles, so it cannot repair a stripped
  one.
- The consumer's "teleport lands elsewhere after an editor scene load" is unexplained; may dissolve
  now that the world populates.
