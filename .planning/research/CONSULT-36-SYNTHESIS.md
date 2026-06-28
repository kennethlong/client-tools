# Synthesis — in-game 3D-audio regression measurement (CONSULT-36, 2026-06-18)

Four crew on non-overlapping angles (evidence: CONSULT-36-EVIDENCE.md; outputs: -codex/-cursor.out,
Sonnet/Opus inline). They CONVERGE.

## The keystone (Codex): engine 3D code is UNCHANGED across the 7.2e->9.3b port
Driver-open args, frequency/bits (22050/16), the distance-cull gate (Audio.cpp:2165), 3D
position/distance calls (3200/3201/3223), serve cadence (5314), premix toggles — all identical. ONLY
the Miles runtime/provider DLL changed. => Any 3D-behavior regression is in the **9.3b runtime**, not
our code. Revert baseline for a measured 7.2e anchor = commit **895efb48e** (last clean 7.2e Win32),
bring Audio.cpp + clientAudio.vcxproj + SwgClient.vcxproj + miles-7.2e libs/redist; build Win32 Release.

## Prime suspects (ranked)
1. **Sonnet H1 — `DIG_3D_MUTE_AT_MAX` defaults YES in 9.3b** (mss.h:800). Hard-MUTES a 3D sample at its
   max distance (engine sets distanceMax = 4x distanceAtMaxVolume). One-frame lag between the engine
   cull and Miles' internal mute => "door late or not at all, ship flybys inaudible." 7.2e may default
   NO. TEST: `AIL_set_preference(DIG_3D_MUTE_AT_MAX, 0)` before AIL_open_digital_driver.
2. **User hypothesis / Sonnet H3 — Debug system-lag.** Sound triggering is main-thread/per-frame
   (Audio::alter @ Game.cpp:1192); 9.3b resample filter (DIG_ENABLE_RESAMPLE_FILTER default YES) + the
   50ms serve gate (5311) in a slow Debug build => short transients start/end between service ticks.
   TEST: x64 Release run (much higher fps). [latency spiked 196->724ms during load = main-thread stall.]
3. Sonnet H4 — bucket consolidation silences rapid same-template re-triggers. TEST: cfg
   `[ClientAudio] consolidateQueuedSounds=false` (Debug-only DebugFlag, no rebuild).
4. Sonnet H5 — 3D path uses `AIL_set_sample_file` (no extension, 3160); 9.3b stricter format detect
   could silently drop. TEST: enable s_debugSoundStartStop, watch AIL_last_error after 3D set.
5. Codex #2 — 3D provider filter (mssds3d.flt/eax) stale/mismatched degrades positioning (not mute).

## Confirmed dead ends
- `AIL_digital_latency` = nominal buffer constant (196), meaningless with 3D provider (Cursor + Sonnet);
  NOT a diff lever. Real metric = per-event mixer_ms (T_playing - T_start) + AIL_get_timer_highest_delay.
- DIG_DS_FRAGMENT_CNT/MIX_FRAGMENT_CNT set before open did NOT change latency (already tested) -> reverted.

## Methodology (Opus): isolate one variable at a time
Anchor = 7.2e+Win32+Release (and it MUST be MEASURED, not trusted — FM-1). Decision tree:
- Step 1: N64D->N64R (Debug->Release, x64). Symptom gone => Debug artifact, short-circuit.
- Step 2: N64R->N32R (x64->Win32). Gone => bitness.
- Step 3: N32R->A (9.3b->7.2e). The version test. Only if 1&2 don't localize.
Normalize: same area/route/cfg/assets; cap fps; dual-domain (per-frame AND per-wall-sec); >=3 runs
(symptom is intermittent); assert the loaded mss*.dll version per run (FM-6).

## Current action — instrumented x64 Release (Run 1)
Built: fps + frameMs_max in audio-health.log (tests system-lag), health probe ON, `s_disable3dMuteAtMax`
toggle staged OFF (so Run 1 = pure 9.3b-Release behavior). Run cantina/Tatooine, listen + read fps.
- fps high + audio clean -> Debug system-lag confirmed (user hypothesis). Done.
- fps high + audio still bad -> flip s_disable3dMuteAtMax=true (Run 2) to test H1.
- fps low/spiky in Release too -> cantina is frame-bound; fix is perf, audio just rides on it.
Probe spec for a full trigger->audible/cull/voice instrument (if needed): see CONSULT-36-cursor.out.
