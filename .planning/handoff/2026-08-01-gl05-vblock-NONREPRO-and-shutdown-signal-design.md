# 2026-08-01 — gl05 815ms VB-lock did NOT reproduce (likely cured by `2672dff0f`) + shutdown-signal design

**Repo state:** `origin/master` = `fa346b421` at session start. This session: one instrumented
cold-boot repro run (no code changes), plus the docs commit that carries this file.

**Bottom line:** the 815ms `Direct3d9_DynamicVertexBufferData::lock` skeletal first-draw stall —
armed and hunted since 2026-07-19 — **did not fire at all**. Not "smaller": the probe threshold is
5ms and **zero VBLOCK/IBLOCK lines were emitted for the entire session**. Leading explanation is
that the 07-18 `shaderCachePreload` landing (`2672dff0f`) cured it as a side effect; the 815ms
attribution is from **07-13**, five days earlier, and nobody re-measured the Lock afterward.
Status: **NOT CLOSED — one clean negative.** Probe left armed for passive confirmation.

## 1. The run, and why the negative is trustworthy

Kenny's cold boot, 2026-08-01 19:22:45 → 19:23:26 local (~41s), Win32 Release.
Deliberately **no cache clearing** — see §5 for why that was the correct call.

Setup verified before interpreting, so a null result is a real null and not a misconfiguration:

| Check | Value |
| --- | --- |
| `rasterMajor` | `5` (gl05) — `stage/client.cfg:51` |
| Probe key | `[Direct3d9] logDynamicBufferLockMs=5` — `stage/client.cfg:72`, correct section |
| Staged DLL currency | `gl05_r.dll` 07-18 19:42 vs probe source 07-18 19:41 → **probe is in the running DLL** |
| Renderer confirmed live | `Direct3d9_ShaderCache: preload complete (71 cached shaders in RAM)` in report log |
| exe↔cfg pairing | `SwgClient_r.exe` → `client.cfg` ✓ |
| Watchdog + sampler | `stallWatchdogMs=100`, `stallWatchdogMaxDumps=0` (log-only), `stallWatchdogStackSamples` absent → default 100, **on** |

Logs rotated to `stage/*.pre-vblock-run.log` before the run so the capture is unambiguous.

**Skeletal content was genuinely present** — the shutdown stacks show
`CreatureObject::~CreatureObject` → `SkeletalAppearance2::~SkeletalAppearance2` teardown, so
avatars were loaded and drawing. The negative is not "no skinned meshes ran."

## 2. What the load window actually contains (Kenny's hypothesis, tested)

Hypothesis on the day: *"the delay is during world load, so I don't see it."* **Testable, and the
answer is no** — the load window was fully instrumented and captured. Stalls fired at loops
1682–1706 (19:22:57→19:23:00) inside `GroundScene::init`, with sampler stacks, and none is a
buffer lock. All of it is cold file I/O on the audio/effect preload path:

- `GameMusicManager::install` → `PreloadedAssetManager::addSoundTemplate` → `SoundTemplate::addSample`
  → `TreeFile::getFileSize` → `NtCreateFile`
- `Audio::cacheSample` → `AbstractFile::readEntireFileAndClose` → blocking `Gate::wait` on FileStreamer
- `CommandTable::loadCommandTables` → `DataTable::findColumnNumber`
- `ClientEffectTemplate::load_0001`, `ClientWeaponObjectTemplate::postLoad`,
  `ClientBuildingObjectTemplate::PreloadManager`

That is the **~560ms GroundScene-ctor composite**, still present, still behind the loading screen,
still deprioritized by Kenny (07-19). Unchanged in character.

**After world entry: 25 consecutive seconds with zero stalls ≥100ms** (19:23:00→19:23:25, nothing
logged). Kenny's "no stuttering or slow down noticed" is corroborated by instrumentation, not just
perception.

## 3. Two readings that the raw numbers invite and that are WRONG

1. **The ~1048ms "stall" at loop 3654 is SHUTDOWN, not gameplay.** Every one of its ~30 sampled
   stacks bottoms out in `ExitChain::run` → `SetupSharedFoundation::remove` → `ClientMain.cpp:449`
   → `exit@exit.cpp:301`. Ordered teardown: `CuiManager::remove` @280ms → UIManager gc →
   `WorldSnapshot::unload` @658ms → `Direct3d9Namespace::remove` @765ms → `LocalizationManager`,
   `PaletteArgbList`, `TreeFile::SearchPathA` dtors → CRT atexit. **Kenny's call: keep it.** A
   clean ordered unwind is worth ~1s nobody watches, specifically because SWG-Toolkit attaches to
   the live process (§6).
2. **`Ordinal23` / `Direct3DCreate9On12Ex` frames are nearest-exported-symbol artifacts** inside
   `d3d9.dll`, not evidence of d3d9on12. Do not read them literally — the sampler resolves to the
   nearest export when it has no PDB for a module.

## 4. NEW: the largest non-shutdown stall is now a driver wait inside the DRAW call

Loop 684 @ 19:22:50, **873ms total** (112ms at first sample), pre-world:

```
ZwWaitForSingleObject < WaitForSingleObject < nvd3dum.dll+0x14d032d < ... < d3d9.dll
  < Direct3d9Namespace::drawIndexedTriangleList@Direct3d9.cpp:4469
```

A **draw call** blocking on the NV user-mode driver. Same *family* as the VB-lock hypothesis
(driver sync at first draw) but caught at the draw rather than at the Lock. If the mechanism
survived `2672dff0f` anywhere, this is where it moved. Sits in the same
"hidden behind loading/char-select" bucket Kenny deprioritized — logged, not chased.

Minor: 29 warnings total (no flood), but one was sampled mid-`StackWalk64` from
`ClientWeaponObjectTemplate.cpp:337` — every `Warning()` during preload does a full symbolized
stack walk. Harmless at 29; would be a real cost at 2,900. Worth remembering as a shape.

## 5. Why no caches were cleared (and why clearing them would have been wrong)

Asked at the top of the session. The answer, recorded because the reflex is strong:

- **Do NOT clear `stage/shader-cache-d3d9/`** (71 `.cso`). That is the disk bytecode cache the
  `shaderCachePreload` slurps into RAM at install. Wiping it forces 71 `D3DCompile` runs and
  injects a *different known* stall class into the exact frames under measurement. It would not
  be "more faithful to the original" — the 815ms predates preload, so a cleared cache is a **third**
  environment matching neither.
- **Do NOT clear the NV DXCache** — same failure mode, driver-side recompiles competing with the
  very Lock being timed.
- **OS page cache is the only coldness with a hypothesis behind it**, and "hasn't run all day"
  supplies it free. Forcing a standby-list purge adds a variable without a reason.
- Coldness is secondary regardless: a D3D9 `Lock()` block is driven by GPU/driver state within a
  frame, not by disk. The real driver is how many software-skinned meshes pump the shared 2MB
  dynamic-VB ring at first draw — a function of location and population, not cache warmth.

## 6. Shutdown signal — Kenny wants one; design verified, shape decision open

**Motivation:** SWG-Toolkit attaches to the live process (Present hook + the advertised hookpoint
table). Today the contract has `game::quit` and `game::cleanupScene` — both **imperative** (the
consumer *causes* teardown) — and **no notification**: nothing tells the consumer "unwind has
begun, stop issuing calls."

**Why it is safe today anyway:** their calls marshal onto the Present-hook drain, and by the time
`ExitChain::run` executes the main loop is already over, so Present has stopped firing before the
first destructor runs. **The guarantee is emergent from ordering, not promised by contract.** It
breaks the moment any toolkit thread (their MCP server / embedded agent) calls an advertised row
directly instead of queueing — a `cuiManager::*` or `worldSnapshot::ws*` call arriving inside that
~1s window hits freed memory and presents as an unreproducible exit-time crash.

### 6a. The obvious candidates, and why they fail

- **`ExitChain::isRunning()`** — *looks* perfect (public, out-of-line at `ExitChain.cpp:54`, so
  directly advertisable, no shim). **Unusable:** it returns `PerThreadData::getExitChainRunning()`
  — **per-thread** state, set via `PerThreadData::setExitChainRunning(true)` at `ExitChain.cpp:192`.
  A toolkit thread asking gets *its own* flag (`false`), never the main thread's. The one thread
  for which it is true is the thread blocked inside teardown, which cannot answer.
- **`Game::isOver()`** (`Game.cpp:1023`, out-of-line, process-wide, advertisable as-is) — usable as
  an **early** signal only. It returns `ms_done || !IoWinManager::haveWindow() || Os::isGameOver()`,
  so it dereferences `IoWinManager`/`Os` state and stops being safe to call once teardown of those
  subsystems begins — i.e. it goes unsafe exactly when it matters most.

### 6b. Recommended: a monotonic process-wide phase int

Own a plain `static int` in **sharedFoundation** (`ExitChain.cpp` is the natural home — it owns the
unwind), deliberately distinct from the existing per-thread flag:

| Phase | Meaning | Set where |
| --- | --- | --- |
| `0` | running | initial |
| `1` | shutdown requested | `Game::quit()` (`Game.cpp:1000`, sets `ms_done`) + the second `ms_done` site (`Game.cpp:1584`), calling down into sharedFoundation |
| `2` | engine unwinding | top of `ExitChain::run` — the universal funnel every shutdown stack in this session passed through |

Consumer contract: **`>=1` stop queueing new work; `>=2` issue no advertised calls at all.**
Monotonic, never decreases, never resets. Reading it is a plain `int` load — no locks, no dependent
subsystem state, **safe from any thread at any time including CRT teardown**, which is precisely
what `Game::isOver()` cannot promise.

Layering: clientGame → sharedFoundation is the correct direction (down). Advertise as one row,
`game::getShutdownPhase`, a thin out-of-line forwarder in `Game` returning
`ExitChain::getShutdownPhase()` — keeps the contract's existing prefix vocabulary rather than
inventing a `foundation::` namespace for a single row.

Cost: **contract v25→v26, 147→148 names**, append-only. Plus the standing gate set (0 unresolved,
ord-82 export, 148==148, 45s boot smoke, restage) and a toolkit re-sync (sha256s + rva table).

### 6c. Zero-cost interim, no contract bump

`game::g_mainLoopCounter` is **already advertised** (`engine_hookpoints.inc:85`, backed by the
out-of-line `Game::getMainLoopCount()` twin at `Game.cpp:1033` returning `ms_loops`). If it stops
advancing, the loop is over. The toolkit can gate any direct call on "counter moved recently" and
get a liveness check today, out of a row that already exists. Worth telling them regardless of
whether 6b lands.

### 6d. OPEN DECISION (Kenny)

Which shape:
- **(A)** Full 6b phase global — robust, always-safe, 1 row, v26. *Recommended.*
- **(B)** Advertise `game::isOver` only — 1 row, v26, ~zero new code, but unsafe-to-call during the
  window it is meant to protect. Cheap and partial.
- **(C)** 6c only — no bump, consumer-side liveness gate on the existing counter.

Not implemented pending that call, because it obligates the toolkit maintainer to re-sync and the
three shapes give them materially different guarantees.

## 7. How to close the VB-lock item (do not close on this run alone)

The probe costs nothing when nothing exceeds 5ms — it wrote **zero** lines this session. Leave
`[Direct3d9] logDynamicBufferLockMs=5` armed in `stage/client.cfg` and play normally for a couple
of sessions, **ideally including a populated area or an NPC-dense cantina** (more skinned meshes =
the actual driver of the ring-wrap hypothesis; this run was a sparse solo entry). If it stays
silent:

1. Close the 815ms class as **cured-by-preload**, crediting `2672dff0f`.
2. Strip `logDynamicBufferLockMs` from `stage/client.cfg` (cfg-parity hygiene; the code stays
   in-tree permanently, default 0).
3. Do **not** touch `ms_size` / the 2MB ring sizing heuristic — the leading suspect was never
   convicted, and there is now no measured stall to justify the change.

If it ever fires, the discriminator is unchanged: `discard=1` + high `discards=<frame>` → DISCARD
rename pressure on the ring; `discard=0` or isolated discards → driver sync.

## 8. Carried backlog (unchanged)

- Driver-threading soak call → flip `ConfigDirect3d11` default.
- Probe strip pass (PortalCullProbe chatty; TEXCREATE + sampler + the two `[Direct3d9]` keys are
  standing tools, NOT strip candidates).
- `preloadSomeAssets` single-item overshoot (AsynchronousLoader routing) — consciously deferred.
- GroundScene-ctor mega frame — deprioritized 07-19, do not resume without a fresh ask.
- Toolkit: AWAITING their adoption of the 08-01 research docs + the roadmap-reorder call.
- Carried: ilm-extract audit, real-door trigger brittleness, Debug-config 5-target refresh.

## 9. Session gotchas

- **A null probe result demands a setup audit before it means anything.** Five checks (cfg key +
  section, `rasterMajor`, staged-DLL-vs-source mtime, renderer-confirmed log line, exe↔cfg pairing)
  turn "nothing logged" from ambiguous into evidence. Do this *before* interpreting, not after.
- **Rotate `SwgClient_report.log` + `stall-watchdog.log` before any instrumented run.** They were
  8.9MB / 2.4MB of prior-session traffic; a fresh file makes the capture greppable and dated.
- **Read the whole stack before calling a number a stall.** The biggest number in this run (1048ms)
  was `exit()`. A stall figure without its stack is not a finding.
- **`isRunning()`-style APIs may be per-thread.** Check the storage (`PerThreadData`) before
  advertising anything that looks like process state — the name does not tell you.
