# CONSULT-63 — evidence pack: crackle storm at music track transitions + Miles version matrix

All items below are MEASURED or repo ground-truth. Treat as given; do not re-derive.

## Symptom (locked)

- Reproducible "crackle storm": heavy crackling when NEW one-shot sounds are triggered
  within roughly ±1s of an MP3 music-stream track transition (old stream EOF + release,
  next stream open + prime). Crisp repro: trigger the cantina-door sound just as the zone
  theme ends (~35s after zone-in).
- 3-run split experiment: (a)+(b) idle at the transition → NO storm; already-playing looped
  sounds (harvester hum) play cleanly through the transition. (c) door triggered at the
  transition → storm. Historical control: door-churn 8s AFTER the transition → clean.
- Engine instrumentation during the storm moment: every AIL open/close call measures 0ms;
  no main-thread stall >100ms (watchdog armed and near-silent); no starvation edges; no
  volume steps at the storm moment.
- Prompt stream release does NOT prevent the storm: the engine now polls
  AIL_stream_status()==SMP_DONE each frame and releases the finished stream within one
  frame; storm unchanged.
- Track-transition anatomy: the music .snd is a multi-sample playlist. At EOF the engine
  releases the old MP3 stream and opens+primes the NEXT MP3 stream ~16ms later
  (measured: OPENCALL 16ms after the EOF poll fired).

## Confirmed anomaly (locked)

- The stream end-of-sample callback registered via AIL_register_stream_callback NEVER
  fires for these MP3 music streams (Miles 9.3b, Win32). Verified with paired diag lines
  over many sessions: the SMP_DONE poll fires, the callback line never appears. Title
  music looping via AIL_set_stream_loop_count works correctly (Miles-side loop).

## Version matrix (measured 2026-07-04)

- Retail SWG client shipped **mss32.dll 7.2a** (`D:\Code\SWGSource Client v3.0\Mss32.dll`,
  FileVersion 7.2a). The engine's audio code (clientAudio) was written against that era.
- Our Win32 client: compiles against `src/external/3rd/library/miles-9.3b/include/mss.h`,
  links `miles-9.3b/lib/win/mss32.lib`, runs **mss32.dll 9.3b** (stage/miles).
- Our x64 client: compiles against the SAME 9.3b header, links 9.3b `mss64.lib`, but runs
  **mss64.dll 9.3v** at runtime (swapped in for a 9.3b-x64 3D-mixer bug; commit 984afc073).
  The 9.3v filter DLLs (dolby/dsp/srs .flt) report FileVersion **9.3g**.
- In-repo SDKs side by side under `src/external/3rd/library/`:
  - `miles-7.2e/` — include/mss.h (7.2e, 30-Oct-08), lib/win/Mss32.lib, redist/ with a
    full 7.2e runtime set (mss32.dll 7.2e + .flt/.asi).
  - `miles-9.3b/` — include + lib (mss32.lib/mss64.lib) + redist + redist64.
  - `miles-9.3v/` — redist64 only.
  - `miles/` — a stale 7.2a-era copy, NOT referenced by any build config (verified all 5
    clientAudio configs and SwgClient link configs point at miles-9.3b).
- Full Miles **9.3b SDK WITH SOURCE** at `D:\Code\milesss-v9.3b` (win/sdk, win/mp3,
  win/ogg, win/vox, doc/).

## Eliminated (locked — do not re-propose)

- Stream teardown cost (close calls 0ms).
- Live compressed-refill starvation (earlier STARVED edges were a close→reopen slot-reuse
  logging artifact; Miles reuses stream slots instantly).
- Unramped volume steps as the storm mechanism (VOLSTEP probes silent at storm moments).
- Main-thread stalls (watchdog armed at 100ms, near-silent through the repro).
- Engine-held Miles-mutex during start batches: cycling the lock per sound made audio
  WORSE (start latency) and was reverted; batch-wide hold is the shipped state.
- Mix-ahead cushion raises: a 192ms load premix did not touch this class (also reverted
  for latency reasons). In-game mix-ahead is 16ms, load 64ms, ring 256ms (stock).
- Engine-side release timing: prompt (1-frame) release after EOF did not change the storm.

## Relevant engine call pattern (Audio.cpp, for reference)

- Streams: AIL_open_stream → AIL_set_stream_loop_count (0 or N) →
  AIL_register_stream_callback → AIL_start_stream; volume via sample handle from
  AIL_stream_sample_handle; EOF now detected by polling AIL_stream_status==SMP_DONE
  (+position>0 guard); release via AIL_close_stream.
- One-shots: whole-file load + AIL_allocate_sample_handle / AIL_init_sample /
  AIL_set_named_sample_file → AIL_start_sample; MP3 one-shots decode via the ASI codec.
- File I/O is redirected through AIL_set_file_callbacks into the engine's TreeFile layer
  (callbacks now lock only map state; all I/O runs unlocked).
- Process runs timeBeginPeriod(1). Miles mixer = MSSTimer thread (boosted priority),
  service period AIL_MM_PERIOD; app-side batches sound starts under the Miles lock.
