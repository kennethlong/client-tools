# CONSULT-61 — in-game audio: dead music stream, pops, late sample starts — SHARED EVIDENCE (treat as GIVEN)

SWG client, Miles 9.3b (vendored SDK source at D:\Code\milesss-v9.3b — READ IT, all
mechanisms below are verifiable there), Win32 Release, RadSoundSystem backend.

## Instrumented session (2026-07-04 11:17-11:18, stage/audio-diag.log — full log below facts)

A per-frame probe edge-logs each stream sample's `SAMPLE::starved` flag (set by the
mixer when it wants data and no stream buffer can supply it; CLEARED when a buffer is
loaded into the sample — wavefile.cpp:8240; NOTE it is also the INITIAL state of a
sample, wavefile.cpp:8176, so a STARVED edge at stream open is an init artifact) plus
500ms summaries. Session config: `[ClientAudio] streamBufferBytes=1048576` (streams
open with a 1MB buffer pool = 8 chunks x 128KB; Miles stock default is 1 second of
compressed data split into 8 chunks).

**Measured facts:**
1. `music/mus_theme_tatooine.mp3` opened at zone-in (11:18:02, while the budgeted
   loading-screen pumps were hammering TreeFile) and its `starved` flag NEVER cleared
   for the remaining 40s of the log — no buffer was EVER loaded into its sample. The
   stream is effectively silent/dead while `AIL_stream_status`=4 and it stays in the
   stream list. It never recovered.
2. Streams opened moments later recovered instantly and stayed healthy
   (`amb_desert_lp.wav`, `amb_cantina_medium/large_lp.wav`,
   `music/mus_figrin_dan_song_2.mp3` in the cantina — starved=0 throughout).
3. User-audible symptoms in the same session: **popping in the (audible) music** —
   i.e., pops in the HEALTHY figrin-dan stream — and **late sound starts**: the
   cantina door-open one-shot played late and footstep sounds started late
   (door/footsteps are small cached SAMPLES, not streams).
4. Frame rate was smooth (stall watchdog armed, zero >100ms frames in-world; movement
   fluid) — these are NOT main-thread frame stalls.
5. Two instances of `amb_desert_lp.wav` streamed concurrently at 11:18:10 (log line)
   — concurrent same-name streams happen.

## Engine-side architecture facts (established CONSULT-60, verified in SDK source)

- Streams are serviced by a Miles-internal timer (`AIL_register_timer(background)`,
  16Hz, mssstrm.cpp:841): `load_buffer_into_Miles` moves async-completed chunk reads
  into the sample; `start_IOs_if_we_can` issues chunk reads on Miles' ASYNC IO thread.
- ALL Miles file IO goes through OUR file callbacks
  (`AIL_set_file_callbacks(fileOpenCallBack, fileCloseCallBack, fileSeekCallBack,
  fileReadCallBack)`, clientAudio/Audio.cpp:1399) → TreeFile. The callbacks carry a
  **title-music fix** (memory: login-music, commit 5a894d327): Miles' IO thread
  re-opens a stream with an EMPTY filename; the fix remembers the last real stream
  name in a SINGLE GLOBAL (`s_lastAudioOpenName`), tracks a sequential read offset
  (`s_streamResumeOffset`), and substitutes handles (`s_substitutedHandleMap`) — see
  Audio.cpp ~:4040-4090. This state is one-slot GLOBAL; at zone-in THREE+ streams open
  near-simultaneously through it.
- The output side was fixed earlier today (RadSS ring 2048ms at open, load-phase
  mix-ahead 1500ms, in-game 16ms — genericdig.cpp SS_serve remainder math) — output
  overrun crackle is handled; do not re-derive.
- Sample (non-stream) sounds: cached 2D/3D samples, `s_requestedMaxNumberOfSamples`
  (user option, historically 32) caps concurrent sample handles.

## Open questions (each consultant gets a distinct angle — see your task file)

The full audio-diag.log is at D:\Code\swg-client-v2\stage\audio-diag.log (108 lines).
