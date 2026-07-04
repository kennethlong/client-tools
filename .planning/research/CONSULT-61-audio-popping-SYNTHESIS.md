# CONSULT-61 SYNTHESIS — in-game audio: dead stream, pops, late starts (4-crew round, 2026-07-04)

Evidence: [CONSULT-61-audio-popping-EVIDENCE.md](CONSULT-61-audio-popping-EVIDENCE.md)
(instrumented session: stage/audio-diag.log — mus_theme_tatooine permanently starved
from zone-in; figrin-dan healthy but popping; door/footsteps late). Angles: Codex = our
file-callback layer; Cursor = Miles SDK stream internals; Opus = late-sample-start
class; Sonnet = log forensics + lateral pop mechanisms + A/B matrix.

## THE root cause (Codex — the round's decisive find)

**`s_nextFileHandle` started at 0, and Miles' async IO treats FileHandle==0 as "not
opened yet — open by name" (milesasync.cpp:440-443) with an EMPTY name for stream chunk
requests (mssstrm.cpp zeroes the request).** The "Miles re-opens the stream with an
empty filename" behavior that the 2026-06-18 login-music fix (5a894d327) was built
around was never a Miles quirk — it was our handle 0 being read as Miles' unopened
sentinel. The first stream after install (title music) degenerated into per-chunk
empty-name re-opens; the shim + resume-offset + substituted-handle machinery is
scaffolding over that one-character bug, and its ONE-SLOT GLOBAL state is actively
destructive with concurrent in-world streams: real-name opens reset the shared offset
and CLOSE substituted handles other streams may be mid-read on (Audio.cpp:4195-4202) →
cross-stream wrong-file/wrong-offset reads (decoder pops) and wedged streams.

Supporting convictions:
- **Unsynchronized callback state** (s_fileMap, s_nextFileHandle, shim state) accessed
  from the main thread + Miles' async IO thread — the CONSULT-56
  unguarded-cross-thread-container class, in the audio layer.
- **Tatooine-theme death signature** (st=4, starved=1, error=0, 40s): Cursor's SDK table
  narrows it to a silent async wedge (asyncs_loaded stuck at 1 / enqueue lost /
  per-handle serialization jam — mssstrm.cpp:1187-1189, milesasync.cpp:547-558) — all
  candidate triggers live in the racy callback layer. No retry exists; only
  close+reopen recovers a wedged stream. AIL_stream_status is USELESS for detection
  (returns SMP_PLAYING while dead); SAMPLE::starved is the reliable signal.
- **1MB streamBufferBytes was a net negative** (Cursor): Miles couples prime to 7/8 of
  the pool with NO independent cap (mssstrm.cpp:791-793) → music start latency 18-56s
  contended; 128KB chunk turnover predicts pops every 4-8s. Reverted to stock 0.

## Landed (this build)

1. `s_nextFileHandle` starts at **1** — the empty-name path never fires again (0 also
   remains the open-failure return).
2. `s_titleMusicStreamFix` **defaults OFF** (config `[ClientAudio] titleMusicStreamFix`
   re-arms if ever needed — it should not be: title music now streams via its real handle,
   which also removes the historical per-chunk handle leak the shim managed).
3. **Critical section over all four file callbacks** (open/close/seek/read — map,
   counter, shim state). Miles has a single async IO thread, so no read parallelism lost.
4. cfg `streamBufferBytes=0` (stock 1s pool, <1s prime).
5. Probe extended: **VOLSTEP lines** (one-frame volume steps >0.05 on stream samples) —
   discriminates Sonnet's pop candidate (below) in the same session.

## Remaining suspects (filed, awaiting the next session's log)

- **Volume-snap clicks (Sonnet #1/#2, code-confirmed candidate):** Sound2d::alter sets
  sample volume EVERY frame (Sound2d.cpp:391-392) and 9.3b's
  AIL_set_sample_volume_levels is a bare scalar assignment with no ramp
  (wavefile.cpp:5161-5169; the VolRamp RIB filter exists but is never wired). The .snd
  volume-wander path SNAPS when volumeInterpolationRate==0 (Sound2d.cpp:553-556).
  Pre-existing code — likely "newly dominant" as louder artifact classes get fixed.
  VOLSTEP lines coinciding with heard pops = convicted; fix = ramp in engine (lerp
  toward target volume with a max-step per frame) or wire VolRamp.
- **Late sample starts (Opus, full anatomy):** cold synchronous first-touch load inside
  playSound on the MAIN thread (SoundTemplateList fetch → .snd IFF parse → cacheSample
  readEntireFileAndClose w/ zlib + a REDUNDANT Miles duration-probe decode,
  Audio.cpp:1640-1650/:4733-4751), and event sounds hold NO template ref → samples
  freed between one-shots → footsteps re-decompress per step when walking. Handle cap
  = red herring (exhaustion drops, never delays; session peaked 23/32). Fix wave
  (separate): (a) keep-alive LRU pin of recently played event templates, (b) pre-warm
  interior event sounds at load (CONSULT-59 name-table pattern), (c) compute duration
  from the WAV header instead of the throwaway AIL parse. Async-load = last resort.
- Sonnet's forensics: double amb_desert = designed crossfade (not a leak); chunks=X/8
  probe field is bimodal/useless (sampling artifact); dead streams stay in the stream
  map indefinitely — an optional starved>Nsec → close/reopen safety net remains on the
  table if a dead stream ever recurs post-fix.

## Round 2 (same day): the zone-in "big skip on music change" convicted

Kenny's session on the fixed build: theme stream now plays to completion (dead-stream
class CURED — its lingering starved=1 in the log tail is end-of-track residue), but
"still skips on zone in, big skip on music change". Watchdog dumps for the 11:38:20-26
zone-in cluster (1469/1380/885ms + smaller):
- **stall-loop1164 (1.47s): `SwgCuiAvatarCreationHelper::stopMusic` = a DELIBERATE
  blocking `Sleep(5)+Audio::alter` loop for the full 1s fade**, called from the
  scene-change listener inside the GroundScene ctor (SwgCuiManager.cpp:343-344) —
  original SOE code; the sibling `restartMusic` blocks a full second UNCONDITIONALLY.
  Both fixed: fire the fade and return (CuiManager::stopMusic never blocked).
- **stall-loop1168 (1.38s): `WorldSnapshot::preloadSomeAssets`' own `cms_callbackTime
  = 1.f` — one-SECOND synchronous preload slices per loading frame** (this is the
  budget INSIDE the pump; CONSULT-60's 40ms loadStep budget correctly hands off to it
  at PP_done). Fixed: 0.05f, plus the same for CachedFileManager's 1000ms slice and
  SpacePreloadedAssetManager's 1.f. Total load work unchanged; frames stay ~50ms so
  the mixer queue survives.
"World 3D sounds late for 15-20s then settles" = Opus's cold-first-play class (filed
wave: keep-alive pin + interior pre-warm + duration-probe removal) — the settle-out is
everything going warm.

## Verification (Kenny's next session, diag armed)

EXPECT: no permanently-starved stream in audio-diag.log; load-in music clean; music
start prompt (<1s after trigger); if pops persist, VOLSTEP lines at pop moments convict
the volume-snap class (next fix), and their ABSENCE at pop moments sends us to Sonnet's
provider/sample-rate tests (matrix in the Sonnet .out / task file).
