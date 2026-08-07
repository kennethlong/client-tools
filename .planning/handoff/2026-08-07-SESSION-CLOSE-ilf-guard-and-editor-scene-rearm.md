# 2026-08-07 — SESSION CLOSE: .ilf wrong-class guard + editor-scene re-arm; five loose ends queued

**READ FIRST.** Covers 2026-08-06 morning through 2026-08-07 evening. Written before a context clear;
§6 is the work queued for next session and is the reason this file exists.

## 0. STATE — everything is committed and pushed

`origin/master` = **`364687a52`**, `master` == `origin/master`, working tree clean apart from
`stage/override/*` (deliberately untracked). **Contract unchanged all session: v33, 160 names**,
`GetEngineHookPoints` ord-82 @ `0x00701420`. No consumer re-sync owed.

```
364687a52  docs: editor-scene re-arm CONSUMER-VERIFIED live
877a22357  docs: correct the unpersisted-edits claim in the re-arm handback
751e7f485  docs: index the 2026-08-07 editor-scene re-arm handback
9fee1b52f  docs: CONSULT-72/73 handback + round synthesis
f9ce87a21  fix(WorldSnapshot): re-arm the proximity index on same-scene re-entry   <-- main fix
c90fdbf40  probe(ClientWorld): [cellAtPos] reports all three sphere-tree counts
5ec8e36fc  docs: 4a handback
528aa999b  fix(clientGame): .ilf interior layout refuses wrong-class templates      <-- main fix
18a18ea74  docs: v33 handback, wsAddObject round, session close
0aca54213  fix(advertise): v33 -- game::loadScene destroys the outgoing scene
```

Both platforms built clean from `364687a52`'s tree (5-target Release, 0 unresolved, 0 hard errors)
and are staged. Renderer DLLs untouched all session — no shared header changed, no ABI cascade.

---

## 1. Fix 1 — `.ilf` interior layout refuses wrong-class templates (`528aa999b`)

Queued item 4a from 2026-08-04, raised by the toolkit. **Both halves of the create were unsound in
Release**, not just the diagnostic they reported:

- `safe_cast` is a bare `static_cast` in Release (`SafeCast.h:16-18`), and
  `ObjectTemplateList::createObject` returns whatever the template's own `createObject()` built —
  any class not overriding it gets the base `new Object(...)` (`ObjectTemplate.cpp:155-158`), a plain
  `Object`. A wrong-CLASS row therefore produced a non-null, wrongly-typed pointer that passed
  `if (o)`, and `endBaselines()`/`addToWorld()` dispatched through a vtable slot read from whatever
  the `Object` layout holds at that offset.
- The only diagnostic was `DEBUG_WARNING`, NOP in Release (`Fatal.h:50-52`).

FIX: both create sites route through a file-local `createInteriorLayoutObject` — narrow with the
**virtual** `asClientObject()` (0 in the base, `Object.cpp:2628-2631`), `delete` the wrongly-classed
object, report either failure with **`WARNING`** naming the class it got, return 0 so the caller
skips the row. Idiom already existed: `SwgCuiQuestJournal` narrows a `createObject` result the same
way (`:1118-1123`) and disposes with plain `delete` (`:388`, `:1105`).

Checked rather than assumed: `delete` is safe (`~Object` handles never-added, `:846-868`; both
callers run in the **alter** phase via `IOET_Update`, outside the `setDisallowObjectDelete` window
that wraps only `IoWinManager::draw`, `Game.cpp:1690-1704`); the warning cannot storm (`update()`'s
cursor advances past a bad row → one attempt per cell-arming); no warning-flood fatal exists.

**LIVE-VERIFIED** (gl11): cantina furniture + band gear intact, and — the stronger unplanned control
— a speaker lifted into the air in a *previous* session was still floating, i.e. the **edited,
persisted `.ilf`** loading through the new helper with its transform applied.

⚠ **The refusal branch itself is STILL UNEXERCISED** — see §6.3.

## 2. Fix 2 — WorldSnapshot proximity-index re-arm (`f9ce87a21`)

**The big one.** An editor scene entered via `game::loadScene` *after a server login* came up with
most of the world missing (consumer measured `tree=1/0/9` vs `228/458/0` live). **Two mechanisms,
both correct in isolation:**

1. **STRIPPED (class 1)** — `suppressObject` drops a node's sphere handle when the server streams a
   POB the snapshot already spawned; failed creates (`:1373-1375`) strip too.
2. **NEVER INDEXED (class 2)** — the `PP_sphereTree` gate (`:1034-1040`) skips buildout POB roots
   entirely when **not** single-player.

`load()`'s same-scene early return (`:676-680`) skips the re-parse that would rebuild the index, and
only `engine_wsUnloadSnapshot` clears `ms_sceneName`. **Class 2 emptied the city; class 1 removed
individual authored buildings** — so a strips-only fix would have left Mos Eisley broken.

FIX: re-evaluate the gate under the **current mode** on that branch. Guards, each load-bearing:
`!isDeleted()` (`removeNode` tombstones IN PLACE, zeroing handle **and network id** while leaving the
node enumerable → arming those injects id-0 phantoms), root indices only, skip while
`ms_parsePending` (`PP_sphereTree` does not test `handle==0` → double-insert), `ms_eventObjectMap`
cleared to match `unload()`.

**REJECTED: forcing a full re-parse** (what SwgGodClient / Utinni / the toolkit's reload all do) —
~3.1 s per editor load, re-triggers the CONSULT-71 kept-root residual every time, and the
id-allocator free-test is a reader map-miss so a cleared reader makes minted ids look free.

**LIVE + CONSUMER-VERIFIED same day.** Ours: `322 stripped + 27 buildout`, strips accounted for
exactly (`288 suppressObject + 34 failed creates = 322`), **zero** post-re-arm failures. Theirs:
`232 + 27`, tangible **1→106**, not-targetable **0→236**, cell resolves with no reload.
**Cross-run invariant worth remembering: buildout is 27 on BOTH runs while stripped varies (322/232)
— exactly what the two-class model predicts.**

Retired on their side: the "load the editor scene LAST" ordering rule (it shaped their whole 05.1
checkpoint), the teleport-lands-elsewhere oddity (`warpPlayer` was always correct; the answer was
empty), and the unexplained `flora=9` (near-empty-state artifact).

## 3. Three probes added — all Release-surviving

Every failure mode in this session was invisible in the only build anyone runs. Do not "clean these
up".

| Probe | Where | Fires |
| --- | --- | --- |
| `[ws.load] same-scene re-arm: N stripped + M buildout` | `WorldSnapshot::load` early-return | once per same-scene re-entry |
| `[editor.ws] suppressObject: id=…` | `WorldSnapshot::suppressObject` | server supersedes a snapshot POB (~288/login) |
| `[editor.ws] createObject FAILED: id=… reason=…` | `WorldSnapshot::update` strip site | any failed create (permanent strip) |
| `[cellAtPos] … tree=T/N/F` | `ClientWorld::findClosestCellObjectFromWorldPosition` | `logCellAtPosition=1` |

**Reading rule:** `suppressObject` or `createObject FAILED` appearing **after** a re-arm line is the
"fix landed but bug persists" signature. Neither has ever appeared there.

## 4. Config state (gitignored, so it only lives here)

- **`singlePlayerStartLocation = 3480 / 3 / -4870`** in **both** `stage/client.cfg` and
  `stage-x64/client.cfg` — **KEEP, deliberate.** Engine default is `(0,0,0)`
  (`ConfigClientGame.cpp:981-983`), which put every editor scene 5.9 km from anything worth editing.
  Mirrored across platforms on purpose (the silent stage/ vs stage-x64/ divergence trap).
- `logCellAtPosition=1` still armed (`[ClientGame/ClientWorld]`). Toolkit's; they may want it off now.
- Unchanged: `logUnloadOccupancy=1`, `maxInteriorCreatesPerFrame=10`, `stallWatchdogMaxDumps=0`,
  `rasterMajor=11`.
- Both cfgs verified BOM-clean after editing (`head -c 8` → `23 20`).

## 5. Dead theories — DO NOT RESURRECT

Four mechanisms were killed by measurement this session, three of them mine. Full record in
[CONSULT-72/73 SYNTHESIS](../research/CONSULT-72-73-SYNTHESIS.md).

| Theory | Status |
| --- | --- |
| **v33 `loadScene` teardown** caused the world-cell fallback | **FALSIFIED** by the consumer's direct test. The teardown fix itself is correct and stays — and their §4a shows it retired the `InputScheme.cpp:480` FATAL their two-frame sequence was built around |
| **Stale `realSphere` pruning** (Sonnet) | Dead — cannot prune to empty with 1 object in the tree |
| **Player-at-origin streaming** (Opus) | Dead — 103 s inside the cantina with no create, while a control healed in 6 s. Made a pre-registered numeric prediction, matched it, still wrong |
| **`clearSphereTree` stale handles** (Codex, Cursor, consumer, me) | Dead, and **inverted** — real polarity is handles at **zero**, never re-armed |

---

## 6. ⭐ LOOSE ENDS — the queue for next session

### 6.1 `wsAddObject` executes text on a wrong-class-but-existing template
Same defect class as §1, on the **placement** path. Reaches the id mint, then dies on an indirect
call through a pointer read out of string data (`0xC0000005` DEP at `0x736E6172` = ASCII `"rans"`).
Ten instrumented `return 0` branches cover argument/lookup failures; **this path is not covered**.
A validation branch turns a client-killing AV into an existing `REFUSED (…)` line. Not blocking the
consumer (their top-level-class filter can no longer produce it) — raised because the next consumer
to get a template path wrong lands here with no instrumentation. **Likely fix: the same
`asClientObject()` discriminator §1 used, applied in `engine_wsAddObject` before the object is used.**

### 6.2 `wsForgetNode` does not un-intern the template name
A placement whose template is **novel to that snapshot** grows `.ws` by exactly
`strlen(templatePath)+1` (measured: `shared_endor_roba.iff`, 44 chars → `1,400,272 → 1,400,317`).
No node written — `wsForgetNode` worked; the name table grew. Consumer's corrected byte rule (`.ws`
unchanged **only** when the template is already interned) has now held for five consecutive
placements. **Decide knowingly whether the intern should be reverted on forget**, rather than have
someone discover it diffing snapshots. Bounded and harmless as far as either side can tell.

### 6.3 The `.ilf` wrong-class refusal branch is unexercised
§1's `asClientObject()` → 0 → `delete` → `WARNING` path has never executed — nothing has handed it a
wrong-class template. **Negative test, designed and deferred:** plant one `draft_schematic` row in a
derived `.ilf` for a **different building** — deliberately **not** `edit_1082874.ilf`, which is the
toolkit's live working file. (Do **not** use the byte baselines from the 08-06 handback; they were
stale and are corrected in place.) Expect a `WRONG CLASS` line naming the class, and the building
loading fine minus that one object. Needs one launch from Kenny.

### 6.4 ⚠ The snapshot delete drain has been a NO-OP (found by Opus, not fixed)
`m_distanceSquaredTo` is **never recomputed in the live diff branch** — `computeDistanceSquaredTo`
is called only in the dead `#else` (`WorldSnapshot.cpp:1292`, `:1305`); the live `#if 1` merge diff
(`:1227-1282`) never calls it. It is initialised to `0.f` (`WorldSnapshotReaderWriter.cpp:110`) and
nothing else writes it. So the delete guard at `:1450` is `0.f < (something ≥ 128.f)` → **always
true → the drain always `continue`s and deletes nothing.** Consequences: **snapshot objects are never
streamed out, so memory grows monotonically with everywhere the player has been**, and the
"nearest-first" create sort at `:1330` uses a constant key and is meaningless. **This is a real
find for the perf arc** — see the D3D11-peak-perf memory. Not urgent, not user-visible yet.

### 6.5 `detailLevelChanged` cannot repair a stripped node (latent)
`WorldSnapshot.cpp:1657-1673` re-adds only nodes that **already had** handles, so it cannot repair a
gate-excluded or stripped node — it looked like a model for the §2 fix and is not. Fable also flagged
its shape: it re-adds by **reader index** while iterating `saveList.size()`, a latent blind-walk bug.
Worth a look while §6.4 is open (same function neighbourhood).

---

## 7. Working notes worth keeping

**Consultant crew.** CONSULT-72 (4 seats: Codex call-graph / Cursor asymmetry-enumeration / Sonnet
lateral / Opus lifecycle) then CONSULT-73 (Cursor + Fable on the fix choice, both landed on the same
option). Every round where they converged, they converged on something false; the disagreement was
the mechanism of progress. **Memory updated: `feedback_cursor_dissent_high_hit_rate` — read Cursor's
ENUMERATION first and mine it against the open question; his tables named both inflictors hours
before anyone assembled them while his opinion section pointed elsewhere.** Prior-art check (Kenny's
instinct) was valuable: SwgGodClient, Utinni and the toolkit all work around this with a forced
rebuild and **none fixed it**; all three make it an explicit user action, which argued *against* the
option it superficially endorsed.

**The recurring error, five times in two days:** reasoning from what a mechanism's name/shape implies
rather than what it does. Latest instance was in the write-up of that very lesson — I claimed a
re-parse would destroy accumulated unsaved editor work without checking that the toolkit persists
immediately (corrected in `877a22357`; real exposure is in-flight-only, which the consumer confirmed
and is closing their side).

**Tooling gotchas hit:**
- `dumpbin` invoked through Git Bash with a quoted Program Files path **fails silently** and looks
  like a missing export. Use the PowerShell tool for it.
- Bash tool CWD persists between calls — a stale `cd stage` made a `||` fallback fire and I misread
  "no report log present" when the log existed. **Use absolute paths.**
- `git commit -F -` needs a **bash heredoc**, not a PowerShell here-string (`@'…'@` is a parse error
  in the Bash tool).
- **`#if !defined(_WIN64)` in `WorldSnapshot.cpp` spans lines 1996-3136.** A helper defined in there
  and called from `update()` (built on both platforms) is LNK2019 on x64 — hit this with
  `wsCreateErrorCodeName`, now moved to the top-of-file namespace block. Also: `WS_EDITOR_LOG` is now
  defined near the top (after `using namespace WorldSnapshotNamespace`), not at `:2453`.
- Report-log stamps are **UTC** and differ from local file mtimes; scope greps by date prefix and
  cross-check against `ls -l` when identifying which session a line belongs to.

**Verification discipline that paid.** The thing that finally settled a day of competing mechanisms
was a **pre-registered binary experiment** — *fresh process, never log in, editor scene immediately →
is Mos Eisley intact?* — predicted before running, unambiguous on sight, no code change. After that,
acceptance closed with exact arithmetic (`288 + 34 = 322`, zero remainder). **Never boot-smoke from
the agent shell** (`feedback_boot_smoke_agent_shell_launch_invalid`); ask Kenny.
