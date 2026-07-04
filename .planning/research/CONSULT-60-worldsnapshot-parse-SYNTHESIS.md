# CONSULT-60 SYNTHESIS — WorldSnapshot phased load (4-consultant round, 2026-07-04)

Evidence: [CONSULT-60-worldsnapshot-parse-EVIDENCE.md](CONSULT-60-worldsnapshot-parse-EVIDENCE.md)
(the 3146ms residual zone-in stall after the CONSULT-59 wave — both watchdog samples in
`WorldSnapshotReaderWriter::load` under the GroundScene ctor, pre-loading-screen).
Angles: Codex = consumer call-graph + thread-safety deps; Cursor = parser internals /
chunkability; Opus = design ranking + lifetime; Sonnet = incomplete-data behavior +
audio + next-stall sweep.

## Consensus (genuine convergence-from-divergence)

All four converged on **S1: chunked main-thread parse** pumped from GroundScene's
loading update:
- Cursor: Iff is instance-local (whole file copied to RAM at open, file handle closed
  immediately, no global cursor) → safe to suspend/resume between TOP-LEVEL NODS
  subtrees only (Node::load recurses children internally). The post-parse networkId-map
  walk folds into a per-root-subtree insert with identical semantics.
- Codex: **S2/S3 worker designs are blocked for the buildout half** —
  `SharedBuildoutAreaManager::load` mutates unlocked globals that GroundScene::init
  reads immediately after postload (the GameMusicManager install), and
  `DataTableManager` static cache state is unsynchronized. `ObjectTemplateList::lookUp`
  is read-only-safe only absent concurrent reload. The pure .ws parse alone is
  thread-clean (Crc + allocator only) but doesn't justify a thread given S1 exists.
- Opus: ranked S1 > S3 > S2 — the parse is worker-safe but its CONSUMERS are not;
  S1 turns every race into mere semantic incompleteness, gated single-threaded. New
  invariants: donePreloading/getLoadingPercent must gate on parse-pending (counters are
  0/0 mid-parse and read as DONE → loading screen would drop over a half-built world),
  and teardown must cancel the in-flight parse (unload() is only reached via the next
  load() or ExitChain remove()).
- Sonnet: the containment landmine is wider than loadIfClientCached — player
  `endBaselines` has NO snapshot fallback (silently skips setParentCell; Release has no
  diagnostic) and `handleContainerChangeWithWorld` sets shouldBeInWorld=false
  permanently on a missing container. Today's protection = the map is 100% complete
  before any baseline processes. Snapshot-less scenes (space, disableSnapshot,
  missing .ws) must degrade to instantly-done.

## The design decision where I overrode a consultant

Opus recommended queue-and-flush for `loadIfClientCached` + a conservative
`isClientCached()==true` mid-parse. REJECTED in favor of a **force-finish exactness
valve** (`finishLoadNow()`): queue-and-flush can still lose the player's one-shot
endBaselines cell attach (the flush runs after the message already processed), and a
conservative "true" lies to non-loading callers. The valve: any consumer that needs
complete data mid-parse and misses against the partial map simply RUNS THE REMAINING
PARSE TO COMPLETION synchronously — exact answers always, zero behavioral drift, worst
case degrades to the old synchronous cost for precisely the rare login that needs it
(player inside a client-cached POB whose containment message beats the ~2-4s pumped
parse). The existing `<= INT_MAX` guard in loadIfClientCached keeps live-server ids
from triggering spurious force-finishes.

## Landed implementation (2026-07-04 PM)

- `WorldSnapshotReaderWriter`: new `beginIncrementalLoad(Iff&)` /
  `stepIncrementalLoad(Iff&, budgetMs)` / private `insertSubtreeIntoNetworkIdMap` —
  per-root-subtree map insert (DEBUG_FATAL dup parity), OTNL read at the end of the
  final step. The synchronous `load(filename)` API is untouched (server/tools).
- `WorldSnapshot`: `load()` = cheap prologue (unload → open .ws Iff on heap →
  beginIncrementalLoad → SYNC SharedBuildoutAreaManager::load → phase init). New
  `loadStep()` pumped from GroundScene's loading block under
  `[ClientGame] worldSnapshotParseBudgetMs` (default 40; <=0 = old synchronous
  behavior via finishLoadNow in load()). Phases: wsNodes (Iff freed the moment the
  parse ends — tens of MB, 32-bit matters) → buildout (chunked per AREA; per-area
  locals stay function-local; ms_buildoutObjects persists) → sphereTree (chunked per
  4096 nodes) → done (preload counters + first preloadSomeAssets, parity with the old
  tail). `unload()` cancels in-flight parse state.
- Gates: update()/preloadSomeAssets() early-return while pending;
  donePreloading→false / getLoadingPercent→0 while pending. Valves (finishLoadNow):
  isClientCached / loadIfClientCached (on miss only), addObject / moveObject /
  removeObject (a mid-parse missed delete would resurrect as a duplicate),
  detailLevelChanged, findClosestCellIdFromWorldPosition. Event-object functions
  need no guard (their map fills only via post-parse update() creates).
- Audio (Sonnet's complementary fix): `Audio::setLargePreMixBuffer` 64→1024 fragments
  (~1s mix-ahead; brackets the whole load, restored at _onFinishedLoading) — masks the
  sub-second budgeted pump frames (string tables / buildout areas) for music.

### Audio correction 2 (Kenny: "still skips crackles on world load... crackles and pops in game")

The 1MB stream buffer did NOT cure load skips, and load-phase crackling appeared. SDK
source read of the LIVE backend (RadSoundSystem, genericdig.cpp — NOT the legacy
mssdig.cpp DirectSound path the first analysis used) convicted our own
setLargePreMixBuffer(1024): `SS_serve` fills until free space drops below
`DesiredRemainder = ringTotal − mixAhead` where ringTotal = FRAGMENT_SIZE(1ms) ×
FRAGMENT_CNT(256 default) = 256ms, READ AT DEVICE OPEN (genericdig.cpp:339). A 1024ms
mix-ahead target makes the remainder NEGATIVE → the fill loop (limiter 6/tick) runs
unconditionally every timer tick → overwrites unplayed audio → continuous crackle
whenever the large premix is active (= exactly the loading screens). FIX:
`DIG_DS_FRAGMENT_CNT=2048` before `AIL_open_digital_driver` (2s hardware ring, ~355KB)
+ large mix-ahead 1500ms (< ring) + normal 16ms untouched. Streams: buffer size now
config-gated `[ClientAudio] streamBufferBytes` (default 0 = stock) and a config-gated
per-stream starvation probe (`[ClientAudio] audioDiagLog` → stage/audio-diag.log,
STARVED edge lines + 500ms summaries reading the public SAMPLE::starved/head/tail
fields) is armed for the next session — it discriminates stream-IO starvation vs
mixer-level causes for whatever remains. Miles architecture facts established: streams
are serviced by a Miles-internal 16Hz timer (`AIL_register_timer(background)`,
mssstrm.cpp:841) and SS_serve by its own registered timer (genericdig.cpp:746) — all
off-main; main-thread AIL_serve is supplemental. In-game pops = the pre-existing
2026-07-03 todo class, not this regression.

The premix bump was the WRONG LAYER. Two buffers exist: the mixed-output DS ring
(sized at driver open; ~3s after the 1KB-granularity frag rounding — never the binding
constraint) and the STREAM buffer: `AIL_open_stream(..., 0)` = Miles default of exactly
ONE SECOND of compressed data (`bufsize = datarate/MSS_STREAM_CHUNKS`,
mssstrm.cpp:739-740). Stream refills run on Miles' background IO thread THROUGH our
TreeFile file callbacks — which during load-in screens contend with the loading pumps'
near-continuous file opens. The 1s drains → decoder runs dry → mixer mixes silence =
the skip. Gameplay is immune because TreeFile goes quiet. FIX: stream buffer 0 → 1 MB
(Audio.cpp:631, tens of seconds of compressed music; whole-file for smaller streams).
This also retroactively explains why the AM sessions' music glitched during the
mega-stalls: main-thread TreeFile work starved the same refill path.

## Verification plan + regression signatures

Build SwgClient Release/Win32 (0 unresolved) → boot → auto-login zone-in with watchdog
armed. EXPECT: no ~3s stall dump at receiveCmdStartScene (the old loop-16982 class);
loading screen holds until parse+preload done, then dismisses; world intact (buildout
buildings present — e.g. Mos Eisley structures — since those come from buildout
tables). REGRESSIONS: loading screen never dismisses = donePreloading gate stuck
(check parse phase advances / pump runs); missing buildings/objects in world = buildout
chunking dropped rows; player logged into a POB appearing OUTSIDE at world position =
the endBaselines containment landmine (valve failed — check loadIfClientCached hit);
immediate ~3s stall relocated under the loading screen = a force-finish fired (log
would show one big stall post-ctor — acceptable if rare, investigate if every login).
Next dominant load cost after this (per Sonnet's sweep): the TreeFile loose-searchPath
stat-storm (already ticketed in CONSULT-59 synthesis).
