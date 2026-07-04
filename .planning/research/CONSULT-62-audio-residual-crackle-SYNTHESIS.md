# CONSULT-62 SYNTHESIS — residual crackle: verdicts + fix wave

Round: 4 consultants, non-overlapping angles, shared LOCKED evidence
(CONSULT-62-audio-residual-crackle-EVIDENCE.md). Symptoms under investigation:
(a) ~1-2s minor crackle at zone-in on BOTH renderers; (b) rare isolated in-game
crackles on D3D9 only (D3D11 clean). All after the timeBeginPeriod(1) win.

## Convergence (the real signal: 3 independent directions, same mechanism)

**PRIMARY CONVICTION — app-thread Miles-mutex holds outrun the mix-ahead cushion.**
- Codex (engine trace): `Audio::alter`'s "start sounds" batch held `AIL_lock()`
  across EVERY queued `startSample()` (Audio.cpp:2594-2611) — stream opens, whole
  sample loads, decoder inits, all under one global-mutex hold, exactly when zone-in
  queues 10-30 sounds. Verified verbatim in-tree.
- Cursor (Miles internals): every `AIL_*` entry takes `s_MilesMutex` (mssdbg IN_AIL);
  `AIL_set_named_sample_file` holds it through full MP3/ASI codec init; MP3→PCM
  decode itself runs UNDER the same mutex on MSSTimer inside SS_fill; ThreadProc
  skips the ENTIRE mix pass after a failed 0ms+10ms mutex wait (genericmss.cpp:464).
- Opus (arithmetic): the mix-ahead C IS the whole safety margin (SS_serve fills only
  until queued-unplayed >= C). Underrun = DAC re-reads stale ring bytes = short
  click, self-heals in ms. A continuous hold H clicks iff H > C. In-game C=16ms sat
  exactly inside the 20-100ms D3D9 compile-hitch band → THAT is symptom (b). Two
  colliding 10ms-retry wakes exhaust 16ms.

**SECONDARY (zone-in only) — stream compressed-refill starvation (Opus path B).**
The mix-ahead protects mixed OUTPUT; a source-empty stream voice (compressed chunks
not refilled while loaders own the disk) clicks regardless (starved=1,
wavefile.cpp:11207). audio-diag shows repeated STARVED edges on live ambient loops
in the 2:18-build sessions (some are re-open artifacts; ambiguous without stream
ids). Held in reserve — re-test after the primary fix.

**ORTHOGONAL (real, backlogged) — rampless-gain "zipper" (Sonnet #1).**
`AIL_set_sample_volume_levels` is a bare assignment (wavefile.cpp:5168); Sound2d/3d
feed position/fade-driven targets per frame; a delayed Audio::alter snaps the gain.
DISCRIMINATOR RESULT: VOLSTEP mid-fade-in snaps (~0.5→1.0) fire on EVERY interior
ambient entry in BOTH renderer sessions identically — but Kenny hears in-game
crackle only on D3D9 → zipper is real but probably not the audible class. Cheap fix
(engine-side volume lerp) stays on the backlog.

## Killed this round
- SMT/core-pin + scheduling starvation: `rrThreadSetCPUCore(timer,1)` is
  PSP2/WiiU-only (Cursor caught my evidence-pack error — genericmss.cpp:745-748
  #if guard). Win32 MSSTimer floats at THREAD_PRIORITY_HIGHEST (the double
  OneHigherThanCurrent is cumulative +2). Opus: a HIGHEST thread preempts NORMAL
  loaders in tens of µs — 3 orders under budget. Priority/affinity tuning is moot.
- Wondering why timeBeginPeriod mattered: Miles' own rrThreadCreate also calls
  timeBeginPeriod(1) (win32_rrthreads.c:518) — yet our explicit call measurably
  fixed charselect/gl05. Unresolved wrinkle (shipped mss32.dll may differ from
  vendored source); empirical result stands.
- Sonnet #2 (GPU DPC preemption) weakened by the no-pin correction; #3 (MP3 open
  transient) can't explain (b). Neither ranked.

## Fix wave (landed this round, Audio.cpp)
1. Start batch: Miles lock cycled PER SOUND (each startSample atomic; mixer gets a
   serve window between sounds). Audio.cpp "start sounds" block.
2. In-game mix-ahead 16 → 32ms (s_bufferFragmentsMin) — margin doubles, cost = 2
   frames of sound-start latency.
3. Load mix-ahead 64 → 192ms (setLargePreMixBuffer) — masks holds ≤192ms in the
   zone-in window; stays under the 256ms ring (do NOT raise past ~192 without
   enlarging DIG_DS_FRAGMENT_CNT at device open).

## If residue survives (in order)
1. Zone-in: stream-refill protection — deepen stream pool modestly (streamBufferBytes,
   mind the 7/8-prime latency) or boost the MSSAsync thread; check STARVED edges
   with per-stream disambiguation first.
2. Zipper cleanup: engine-side volume lerp (max step/frame) in Audio::setSampleVolume.
3. Ring 256 → 512 at open + load mix-ahead up accordingly (Opus optional #5).
4. Still-open oddity: the theme VOLSTEP 0.750→0.000 hard cut at the load-end boundary
   recurs every zone-in (14:59:27.9, 15:04:04.2) — untraced (GameMusicManager
   day-night pick or unSilence path); Kenny hasn't flagged it audibly since round 3.
