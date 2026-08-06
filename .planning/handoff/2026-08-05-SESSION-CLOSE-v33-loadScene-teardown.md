# 2026-08-05 — SESSION CLOSE: v33 `loadScene` teardown, the wsAddObject round, three queued items

**READ FIRST.** Covers the 2026-08-04 evening session.

## ⚠ STATE: NOTHING IS COMMITTED

`HEAD` = `345bba54b`, `master` == `origin/master`. **The entire v33 change set is uncommitted in the
working tree.** Nothing is lost (files persist), but the first decision next session is whether to
commit it.

```
 M src/engine/client/library/clientGame/src/shared/core/ClientWorld.cpp        (+52  probe)
 M src/engine/client/library/clientGame/src/shared/core/WorldSnapshot.cpp      (+20  comment ONLY)
 M src/game/client/application/SwgClient/src/shared/engine_hookpoints.h        (+34  v33 + notes)
 M src/game/client/application/SwgClient/src/shared/engine_hookpoints.inc      (+20  stale-claim fix)
 M src/game/client/application/SwgClient/src/win32/engine_advertise.cpp        (+31  THE FIX)
 M .planning/handoff/README.md
```

Both platforms built clean from this exact tree and are staged (§5). Stage a set of explicit paths
if committing — `stage/override/*` is deliberately untracked.

---

## 1. What landed — v32 → v33, no name change (still 160)

**`game::loadScene` was missing the outgoing-scene teardown.** `Game::_setScene(Scene*)` never closes
or deletes the scene it replaces; that is the caller's job, and all three engine by-name installers do
it (`GameNetwork.cpp:483-492`, commented *"First destroy the old scene if need be"*;
`SwgCuiCommandParserScene.cpp:271-283`; `SwgCuiLocations.cpp:171-177`). Our shim copied the
`setScene` line and not the prologue.

Not merely a leak: `~GroundScene` is what calls `ClientWorld::remove()` (`GroundScene.cpp:1132`), so
the incoming ctor's `ClientWorld::install()` (`:691`) ran a **second time over a live world**. Both
install guards are `DEBUG_FATAL` (`ClientWorld.cpp:854`, `World.cpp:162`) → **compiled out in
Release**, where `World::install()` replaces every `WOL_*` object list with a fresh empty one
(`World.cpp:165-168`).

**Why it produced the world cell:** `WorldSnapshot::load`'s opening `ms_loadedList.clear()` is a
deliberate **same-scene re-stream** — rebuild from the already-parsed `ms_reader`, skip a second `.ws`
parse — valid *only* if the caller destroyed the outgoing objects. With them alive, every re-queued
create fails `CEC_objectAlreadyExists`, no POB lands in `ms_tangibleSphereTree`, and
`findClosestCellObjectFromWorldPosition` falls back to the world cell (`dest == world`, as the
consumer logged). Their reload workaround works because `wsUnloadSnapshot` is the **only** place
`ms_sceneName` is cleared (`:2947`); `SwgGodClient` independently dodges the same hazard with
`load("")`/`load(sceneId)` (`BuildoutAreaListView.cpp:101-102`).

Deliberately **not** copied from `performSceneChange`: its `getAttachedTo() != 0` refusal — v29 proved
that predicate is true for any player merely standing indoors, so mirroring it would make `loadScene`
a silent no-op exactly where the editor is used. `GameNetwork::startScene` has no such guard either.

**Consumer-visible consequence (why it bumps):** any `Object*`/`CellProperty*` cached across a
`loadScene` is now genuinely **deleted** rather than stale-but-readable.

Also landed:
- **`[ClientGame/ClientWorld] logCellAtPosition`** (default 0) — discriminator naming which of the two
  world-cell paths fired: `candidates=0` (population) / `portals=0` / `rejectedForId>0` (the
  `ClientObject.cpp:296` `getSinglePlayer() || !getScene()` fake-id gate). **It earned its keep on its
  first outing** — the consumer used it to turn "portal system or scene load?" into one line.
- **Stale header claims corrected** — four *"shared verbatim with D:/Code/Utinni, re-copied at each
  catalog wave"* claims in `.h`/`.inc`, wrong twice over (Utinni sunset 2026-07-19; the toolkit does
  not vendor those files). Both now state that changing a **catalog string** breaks a row **silently**
  on the consumer side. Verified mechanically that the 08-04 `engine_` rename left catalog strings
  identical (160 rows, `Compare-Object` clean).

## 2. Finding 2 (theirs) — the fix is correct and UNREACHABLE from their call path

They call `game::cleanupScene` one frame **before** `loadScene`. `Game::cleanupScene` (`Game.cpp:989`)
does `quit(); _setScene(0)` — nulls without deleting — so `outgoing` is null and the new teardown is
skipped. They reach the same double-install by a different route. **Their analysis is correct.**

Their fix (drop the `cleanupScene` frame, let v33's teardown do it) is right and stays theirs to test
after sign-off; the sequence was introduced after a re-entrancy crash, so they are testing rather
than assuming.

**`Game::cleanupScene` leaking is a real bug, deliberately NOT fixed as a drive-by.** Four callers,
**three are `ExitChain::add` registrations** (`Game.cpp:853/870/901`) plus logout
(`SwgCuiGameMenu.cpp:128`). Making it delete would newly run `~GroundScene` → `ClientWorld::remove()`
**during ExitChain teardown** — precisely the ordering question CONSULT-71 had to settle by
measurement. Separate change, own probe. Consumer is not asking for it.

## 3. The wsAddObject blocker — CLOSED, was theirs, v33 exonerated

They reported `wsAddObject` returning 0 on 100% of ~19 placements and called it HIGH/blocking. I
reframed it from **their own session log, which was sitting in `stage/SwgClient_report.log`**: 45
`[editor.ws]` lines — 39 allocator, 2 saves, 2 renames, 2 unloads, and **zero `wsAddObject` lines**.
Since `WS_EDITOR_LOG` is `#define WS_EDITOR_LOG(x) REPORT_LOG(true, x)` (`:2328`) — unconditional,
same sink as the line that did print — absence was evidence:

- allocator did **not** refuse (a refusal always prints `wsAllocateIdRange REFUSED: seed=… collisions=…`, `:2297`); the ceiling line they suspected is **informational**, emitted before the mint loop, permanently true for tatooine (`609457649`);
- **no** fail-closed branch fired (all ten `return 0` paths log their branch);
- the success path did not complete either (`wsAddObject OK:` absent).

⇒ control entered, minted, and never reached either logged exit — so not "returns 0" at all. I named
the unlogged window (`:2439`→`:2471`/`:2487`), excluded `NOT_NULL` (a FATAL) and a null `createObject`
(it logs), and **inferred a swallowing SEH wrapper**. All confirmed: `hkSwapChainPresent` wraps
`renderFrame()` in `__try/__except`, and the fault is `0xC0000005` **DEP/execute** at `0x736E6172` —
ASCII `"rans"`, i.e. an indirect call through a pointer read out of **string data** — 3 ms after the
allocator line.

**Root cause was theirs:** their new decoration picker matched `/furniture/` as a *substring*,
admitting `object/draft_schematic/furniture/*` — crafting schematics handed to us as world props.
Fixed by anchoring on the top-level object class. (They got it wrong once more en route by anchoring
on `object/tangible/furniture/`, which silently dropped the cantina's band gear — instrument/speaker/
microphone. Top-level class is the boundary.)

**The v32 control binary killed the regression theory in one run** — identical fault address on v33,
on v32, and on the morning's `.ws`, exonerating all three. It is still staged (§5) and worth keeping
for the next A/B.

## 4. OPEN — three items they left, in priority order

### 4a. ⭐ `ClientInteriorLayoutManager.cpp:143` — Release load-time crash from `.ilf` data (THEIRS to raise, OURS to judge; I verified it)

```cpp
ClientObject * const interiorObject = safe_cast<ClientObject *>(ObjectTemplateList::createObject(objectTemplateName));
if (interiorObject) { … addClientOnlyInteriorLayoutObject … setParentCell … endBaselines … addToWorld … }
else DEBUG_WARNING(true, ("… invalid interior object template name %s. Object will be skipped."));
```

**Verified by me, and the discriminator that matters: this is inside `update()` (72-172) — the LIVE
path — not the dead `applyInteriorLayout()` (176-238).** Guards only NULL; the "invalid template"
diagnostic is a **`DEBUG_WARNING`, compiled out of Release**, where `safe_cast` is also unchecked. So
a wrong-class template that *creates non-null* yields a bad pointer and the next virtual call
crashes.

**Worse than the placement path:** a bad row **persisted into an `.ilf`** would crash on **every
subsequent load of that building**, not just at placement. Nothing poisoned reached their `.ilf`
(placements died before persisting; they verified the file after). But a hand-edited or third-party
`.ilf` naming one wrong template is a Release load-time crash whose only diagnostic exists in a build
nobody ships. This is exactly the fail-loud class this project cares about.

### 4b. `wsAddObject` executes text on a wrong-class-but-existing template

Reaches the mint, then dies on a pointer read out of string data (`0xC0000005` DEP at `0x736E6172`).
Ten instrumented `return 0` branches cover argument and lookup failures; this path is not covered. A
validation branch would convert a client-killing AV into an existing `REFUSED (…)` line. **Not
blocking them** (their filter can no longer produce it) — raised because the next consumer to get a
template path wrong lands here without our instrumentation.

### 4c. `wsForgetNode` does not un-intern the template name

A placement whose template is **novel to that snapshot** grows `.ws` by exactly
`strlen(templatePath)+1` (`object/static/creature/shared_endor_roba.iff`, 44 chars →
`1,400,272 → 1,400,317`, +45). No node written — `wsForgetNode` worked; the name table grew.

They **corrected their own over-generalised invariant**: the v32 acceptance test placed a *carbine*,
already interned in that cantina, hence byte-identical. Rule now: unchanged **only when the template
is already interned**. No ask; bounded and harmless as far as either side can tell. Flagged because
`wsForgetNode`'s contract does not mention the name table — **decide knowingly whether the intern
should be reverted on forget, rather than have someone discover it diffing snapshots.**

## 5. Gates + what is staged

Full canonical 5-target Release build (`Direct3d11;Direct3d9;Direct3d9_ffp;Direct3d9_vsps;SwgClient`)
from the current tree, **both platforms: exit 0, 0 unresolved, 0 hard errors.** The only log hits are
pre-existing `LNK4217` warnings (`_xmlFree`, libxml2 in `sharedXml`).

| Binary | Built | Note |
| --- | --- | --- |
| `stage/SwgClient_r.exe` | 19:46:42 | **v33**, `GetEngineHookPoints` @ `0x00701420` |
| `stage/SwgClient_r_v32.exe` (+ `.pdb`) | 19:41:00 | **v32 control**, @ `0x007013C0` — keep for A/B |
| `stage-x64/SwgClient_r.exe` | 19:47:25 | x64 (no advertise surface there by design) |

Renderer DLLs stayed at 8/1 — up to date, no source touched, so no ABI cascade.
The earlier **hybrid** compile-dir exe (v33 advertise relinked against v32 `clientGame.lib`, because
`SwgClient.vcxproj` alone does not rebuild the library) is **resolved** by this full build.

Consumer-verified this session: v32's three rows still good on v33 — edit/rotate/persist byte-verified
twice, `refreshInteriorLayout` acked code 1 in 130 ms on the occupied cantina, NPCs intact.

## 6. My errors this session

1. **The WorldSnapshot reorder — caught pre-ship, and it would have been worse than the bug.** My
   first fix moved `load()`'s already-loaded test *above* the "clear the current snapshot" prologue,
   reading that prologue as destructive. Checking who clears `ms_sceneName` (nobody but
   `wsUnloadSnapshot`) showed it would have early-returned *after* `~GroundScene` deleted the objects,
   leaving a re-entered scene **completely empty**. Reverted; that spot now carries a
   do-not-fix-this comment. Same recurring cause as the 08-03 errors: I concluded "destructive" from
   a call's name without asking what `update()` does with an empty loaded list.
2. **I launched the client myself and reported noise as findings.** Cost Kenny a round trip on a
   popup that was mine. Now a memory: `feedback_boot_smoke_agent_shell_launch_invalid` — from the
   agent shell `IDirectInput8::CreateDevice` for the system mouse FATALs
   (`DirectInput.cpp:335`, sandbox-independent, upstream of `SetupClientGame` so never our
   regression), and stale binaries block on a modal *entry point not found* loader dialog that mimics
   a 0s-CPU hang. **Ask Kenny to launch; a toolkit launch-and-inject is the strongest gate.**

## 7. The pattern worth keeping

Both defects tonight were the same class, from opposite sides: **a diagnostic that exists, is
correct, and never reaches a human.** Ours put ten branch names in a report log neither side was
reading; theirs wrote a refusal reason to a variable nothing rendered. §4a is that class too — a
`DEBUG_WARNING` in a Release-only failure. The consumer named it the sharpest thing either side wrote
this round; treat it as a standing lens, not an anecdote.
