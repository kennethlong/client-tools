# 2026-08-04 — SESSION CLOSE: contract v25 → v32, two engine defect fixes, one rename

**READ FIRST.** Covers 2026-08-01 → 2026-08-04 in one file.
**`origin/master` = `05903f9f6` — everything pushed, working tree clean** except
`stage/override/{interiorlayout,object,snapshot}/` (toolkit test output, deliberately untracked).

Both platforms staged and current: `stage/SwgClient_r.exe` (Win32, 10:50) ·
`stage-x64/SwgClient_r.exe` (10:53).

---

## 1. Headline

**Contract v25 → v32 (147 → 160 names)** plus two engine defect fixes and a naming refactor.
SWG-Toolkit is the sole consumer and drove almost all of it; the loop was tight enough that
several rows were requested, built, field-tested and corrected inside a single day.

| Commit | What |
| --- | --- |
| `be71f4dce` | gl05 VB-lock non-repro + shutdown-signal design |
| `18a919e36` | **v26** `game::getShutdownPhase` |
| `04c3f8e11` | **fix** — phased parse pumped every frame (lossy reload) |
| `b9363b5b0` | **v27** `object::setParentCell` + `cellProperty::getWorldCellProperty` |
| `96e20b5aa` | shutdown-phase transition logging (v26 measured) |
| `2ea5673ec` | **v28** five rows |
| `0b2e9259c` | **fix** — `unload()` no longer deletes server-owned roots (interior NPCs) |
| `3147cc7f6` | **v29** `object::isChildObject` + **v30** `warpClient` |
| `ddd7b9e0c` | **v31** `warpPlayer` rewritten |
| `01d5454bf` | **v32** `wsForgetNode` + `getCellName` + `refreshInteriorLayout` |
| `05903f9f6` | **refactor** — `utinni_` → `engine_`, `UtinniWsNodeInfo` → `EngineWsNodeInfo` |

Plus doc commits `7e752d863`, `327556c37`, `1d65a96e9`, `4802cf72a`.

---

## 2. The two engine defect fixes (both consumer-reported, both convicted from source)

### 2a. Lossy reload — `WorldSnapshot::loadStep()` was never pumped in-world (`04c3f8e11`)

`loadStep()` had exactly **one** call site outside `finishLoadNow()`, and it sat inside
`GroundScene::updateLoading()` — which opens `if (!m_loading) return;`. Once in-world that
function is dead, so an in-place `load()` started a phased parse **nothing would ever pump**,
while `WorldSnapshot::update()` early-outs on `ms_parsePending`. Result: reload returned no
buildings, no collision, no snapshot creatures.

The consumer's puzzling detail — *"everything returned at once after moving around"* — was not
movement: **11 advertised rows force `finishLoadNow()`**, so any panel refresh or pick drained
the parse synchronously. Their buildout-vs-authored ruling-out was right for the wrong reason.

**FIX:** hoist the pump into `GroundScene::update` (its single call site, per-frame). Moved not
duplicated, so loading-screen rate is unchanged; `loadStep()` self-early-outs when idle.
**FIELD-VERIFIED by the consumer** — 1-2s progressive rebuild, standing still.

### 2b. Interior NPCs destroyed by editor reload — `unload()` unguarded delete (`0b2e9259c`)

`WorldSnapshot::unload()` deleted every snapshot root unconditionally — the one snapshot path
violating the invariant `update()`'s drain already enforces (`isClientCachedOnly`).

**Cascade, fully traced:** `delete building` → `~Object` destroys properties →
`PortalProperty : public Container` → `~Container` deletes each **cell** → `~Object(cell)` →
`CellProperty : public Container` → `~Container` deletes each **occupant**. Server NPCs are in
cell `m_contents` via `ClientObject::depersistContainedBy` → `Container::insertNewItem`
(`Container.cpp:451`). The **player** is not (guard at `ClientObject.cpp:1019`; the player uses
`setParentCell` at `:784`), which is exactly why the 07-18 occupancy guard needed to be
bidirectional — this **explains** that earlier finding rather than contradicting it.

**FIX:** guard the delete with `isClientCachedOnly`. `Container::~Container` untouched — the
ownership cascade other code depends on (`World::remove` skips contained objects because *"their
container will delete them"*) still works.

**VERIFIED LIVE:** `node=1082874 live=1 cells=16 contents=54 serverOwned=54`, `KEPT 255`,
maintainer *"npcs are there and targetable"*, world reloads normally. Of the 255 kept, only ONE
had server-owned cell occupants; the other 254 are roots that are themselves not client-cached
(server-replaced via `8fe51deb0`).

---

## 3. CONSULT-71 — the crew round that killed two of my own proposals

Worth keeping as a template. Four consultants, four different angles, and **every one changed
something**:

- **Codex** (call graph): no discriminator inside `unload()`; the cascade is depended on by
  `World::remove` ⇒ the fix belongs at the delete, not in `Container`.
- **Cursor** (containment trace): confirmed the `m_contents` link in source; **corrected my
  cascade trace** — cells attach with `asChildObject=false`, so the path is the `PortalProperty`
  container, *not* the child-object loop; found the exact player/NPC divergence branch.
- **Opus** (adversarial): **killed my eviction fix** — `setParentCell` only touches the
  attachment graph, never `Container::m_contents`, so `~Container` would have deleted them
  anyway. But was **wrong** about shutdown.
- **Fable** (synthesis): killed *both* the refuse and skip variants, found the ~5-line guard, and
  **correctly refuted Opus** on ExitChain registration order.

**The disagreement was the most valuable output.** Opus said ExitChain teardown sees a live
world; Fable said the opposite from registration order (`ClientMain.cpp:417` vs `:409`). Rather
than pick the more persuasive argument I built a probe — and the probe settled it: a clean quit
emitted **zero** `reason=exitchain` lines while v26's `[shutdown] phase 0→1→2` proved ExitChain
really ran. Fable was right, and the guard is a **measured** no-op off the editor path.

---

## 4. Contract rows added this session

| Ver | Row | Note |
| --- | --- | --- |
| v26 | `game::getShutdownPhase` | monotonic 0/1/2; `ExitChain::isRunning()` is per-thread and unusable; `isOver()` goes unsafe during teardown |
| v27 | `object::setParentCell` | virtual → shim; null cell **FATALs** (`NOT_NULL`), so the guard is load-bearing |
| v27 | `cellProperty::getWorldCellProperty` | required — null cannot mean "outside" |
| v28 | `cellProperty::setPortalTransitionsEnabled` | ⚠ **global state, not scoped** — consumer wrapped it in RAII |
| v28 | `collisionWorld::objectWarped` | |
| v28 | `clientWorld::findCellAtWorldPosition` | the placement-routing primitive; never null |
| v28 | `object::getAttachedTo` | ⚠ **NOT a mount guard** — see §5 |
| v28 | `worldSnapshot::wsIsParsePending` | pure, non-forcing; replaced their `wsGetNodeCount`-as-barrier hack |
| v29 | `object::isChildObject` | the real mount discriminator |
| v30→31 | `playerCreatureController::warpClient` | client-initiated sequenced teleport |
| v32 | `worldSnapshot::wsForgetNode` | drop a node, keep the live Object |
| v32 | `cellProperty::getCellName` | copy-out |
| v32 | `clientInteriorLayoutManager::refreshInteriorLayout` | per-building interior refresh |

---

## 5. My errors this session — all consumer-caught, one recurring cause

Recorded because the **pattern** matters more than the individual mistakes: *reasoning from a
name and signature instead of reading what the thing does.*

1. **`getAttachedTo` as the mount guard (v28 §1.4).** Falsified live — it refused every teleport
   from indoors, because cell parentage and mount attachment share `m_attachedToObject`. I had
   read that exact `attachToObject_w` call earlier the same day. → v29 `isChildObject`.
2. **`warpClient` as the teleport primitive (v30).** `CM_netUpdateTransform`'s handler *opens by
   un-parenting* (`ClientController.cpp:433`), so it structurally cannot place a player in a
   cell. Their **original** sequence was nearly right — the only missing piece was telling the
   server. → v31 via `ClientController::sendTransform`.
3. **`applyInteriorLayout` offered as "already exists".** It is **dead code** in this
   configuration — its gate is the inverse of `update()`'s and `disableLazyInteriorLayoutCreation`
   defaults false. Caught in scoping, before they built against it.
4. **Interior-refresh scoping missed the template cache.** The layout is cached on
   `ClientBuildingObjectTemplate`, not the object, so latch+cursor reset alone would rebuild the
   **pre-edit** `.ilf`. Caught before shipping.

**Commitment made to the consumer, keep honouring it:** for anything touching the network path,
state explicitly whether it was **live-verified** or only **build-verified**.

The consumer reciprocated with two retractions of their own (writing the teleport revert off as
"server authority"; proposing "drop the conversion", which would not have worked). Their standing
rule, now adopted here: **any observation about server-streamed content taken after a
`game::loadScene` is INVALID** — that scene has no `GameNetwork` session.

---

## 6. OPEN — what the next session picks up

### 6a. Awaiting consumer (all v32 rows are BUILD-VERIFIED ONLY)

Named acceptance tests are in `2026-08-04-toolkit-v32-HANDBACK.md`:

- **`wsForgetNode`** — place, Persist, object **stays**; `.ws` has no runtime child node.
- **`getCellName`** — real cell at the placement point; the doorway is the interesting case.
- **`refreshInteriorLayout`** — ⚠ **most wanted result.** Largest new surface, and its failure
  mode is a **silent no-op** (any of its three steps missing ⇒ "nothing happened", no error).

### 6b. Ours, queued

- **`findCellAtWorldPosition` returns the world cell after `game::loadScene`** until a manual
  reload. Consumer has a clean repro; accepted as ours. This is the real signal under the
  `[PortalCullProbe] 1095 → 0` observation they withdrew as editor-scene contaminated. **They owe
  a re-run from a server-connected session** — worth having before digging in.
- **Shutdown event + ack** — designed and costed in
  `2026-08-01-gl05-vblock-NONREPRO-and-shutdown-signal-design.md`, deliberately unbuilt.
  `getShutdownPhase` is a **poll**; if their agent needs waking rather than sampling, this is the
  next step.
- **Stale header comment** — `engine_hookpoints.h` still claims the `.h`/`.inc` are *"re-copied
  verbatim into `D:/Code/Utinni/UtinniCore/swg/` at each wave"*. Wrong twice: Utinni is sunset,
  and the toolkit **does not vendor those files at all** (they bind by name; their only artifact
  is `ENGINE_HOOKPOINTS_VERSION`). Same misleading-comment class that cost a cycle on
  `getAttachedTo`.

### 6c. Perf arc — the close-out plan recorded on 08-01 is INVALID

The gl05 815ms VB-lock did **not** reproduce (probe threshold 5ms, zero lines, setup audited five
ways). Leading explanation: cured by the 07-18 `shaderCachePreload` (`2672dff0f`), which the
07-13 attribution predates.

⚠ **The "leave it armed and play normally" close-out cannot execute:** SWG-Toolkit requires
`rasterMajor=11`, and `logDynamicBufferLockMs` is a **gl05/D3D9-only** probe that can never fire
there. Silence would be guaranteed and meaningless. The key has been **stripped** from
`stage/client.cfg` with a comment on how to re-add. Closing this needs either a deliberate
non-toolkit gl05 session or an honest close on the single clean negative (which was a genuine
gl05 run — `rasterMajor=5` verified at the time).

New and unchased: **873ms NV-driver `WaitForSingleObject` inside
`Direct3d9Namespace::drawIndexedTriangleList`** — the largest non-shutdown stall in that capture.

### 6d. Housekeeping

- `[ClientGame/WorldSnapshot] logUnloadOccupancy=1` **still armed** in `stage/client.cfg`. Cheap,
  but wants stripping for cfg-parity hygiene.
- `stage/override/{interiorlayout,object,snapshot}/` untracked by design (toolkit test output).
- `[Direct3d11] preventDriverInternalThreading=false` + `censusLog=true` — driver-threading soak
  armed; a crash there is expected-data, not a probe artifact.

---

## 7. Standing tools added/changed

- `[ClientGame/WorldSnapshot] logUnloadOccupancy` (default 0) — per-node
  `reason=<load|wsUnload|exitchain> node= live= cells= contents= serverOwned=`. The `reason` tag
  exists because **no state inside `unload()` distinguishes the call paths** (`GameNetwork` is
  connected in both; `ms_sceneName` is set *before* the call by `load()` and cleared *after* it by
  `wsUnloadSnapshot`).
- `[shutdown] phase N -> M` — permanent, fires at most twice per process. The only evidence either
  side can point at for "did the signal fire, and on which path?".
- `keptServerOwnedRoots=N` on `wsUnloadSnapshot`'s `[editor.ws]` line.
- `[editor.ilf] refreshInteriorLayout OK id= deleted= cellsArmed=`.

---

## 8. Tooling gotchas that cost real cycles

- **PowerShell here-strings inside a one-liner get mangled** — a Cursor consult was silently
  truncated to the pack header (it *told* us rather than guessing, which is the only reason it
  was caught). Write long prompts to a **file** and point the CLI at it.
- **Python `\n` inside a shell heredoc became a literal newline**, splitting a C string across two
  lines. **Use the Edit tool for anything containing escapes**; reserve scripted replacement for
  plain-text swaps.
- **`| Out-Null` on MSBuild hides the actual error** — a failure surfaced only as `EXIT=1` with no
  log file, which looked like a toolchain problem and was actually `MSB1009`.
- **Working directory is shared between the Bash and PowerShell tools and persists.** A `cd` into
  `stage/` for log rotation broke a later relative-path MSBuild invocation. **Use absolute paths**
  for anything that matters.
- **Copy the existing form rather than reconstruct it.** `WS_EDITOR_LOG` is defined partway down
  its TU (not file-wide) and `NetworkIdManager.h` lives in `sharedObject/` — both were visible in
  the very file being edited, both cost a build cycle.
- **`grep -rn` over `src/` can exceed 2 minutes.** Scope to known paths or use the Grep tool.
