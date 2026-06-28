# Neutral evidence — in-game audio degradation under Miles 9.3b (measurement-strategy consult, 2026-06-18)

Hand to crew as **given facts**. Goal: design measurement paths to BASELINE the known-good behavior
(Miles 7.2e, which "worked") and DIFF it against the current 9.3b port to root-cause an in-game audio
regression. We are past hypothesizing — we want comparable NUMBERS across variants.

## The regression (observed, x64 Debug, Miles 9.3b)
- In-game SFX "feel stressed": **2D sounds + streams WORK** (cantina band [stream], NPC conversations).
  **3D / transient sounds fail**: cantina door fires **late or not at all**, overhead **ship flybys
  inaudible**, footsteps **drift out of sync**. Inconsistent (late OR dropped), not a uniform delay.
- This is a Phase-35 **7.2e -> 9.3b Miles port** regression and is **pre-existing** (reproduces with all
  our login-music toggles OFF). Login title-music (separate) is already FIXED + committed (5a894d327).

## Measured facts (x64 Debug, cantina, once/sec health probe — `stage-x64/audio-health.log`)
- **No leak / no exhaustion:** alloc3d oscillates 0..11 and frees; activeSamples peaks **12** of the
  **32** sample-handle cap (never saturates); fileMap bounded 2-4; soundMap plateaus ~50; cacheBytes flat.
- **`AIL_digital_latency` = a FIXED 196 ms** every frame (spikes to 724 ms during zone-load), and is
  **UNAFFECTED** by setting `DIG_DS_FRAGMENT_CNT`(96)/`DIG_DS_MIX_FRAGMENT_CNT`(24)/`DIG_DS_FRAGMENT_SIZE`
  BEFORE `AIL_open_digital_driver` (tested: no change). So that buffer lever does not control it here.
- In-game, **everything routes 3D**: sample2dMap ~= 0, sample3dMap up to 11. (2D map only non-zero at
  login/UI.) So the failing sounds (door/footsteps/ships) all go through the 3D `Sample3d` path.
- **3D provider opened OK** — NO stereo fallback warning; boot log: `DIG_MIXER_CHANNELS=64`,
  `speakers = 2 Speakers`, reverb `room_type bus_index=0`.

## Engine audio path (key refs, `src/engine/client/library/clientAudio/src/win32/Audio.cpp`)
- `Audio::alter(deltaTime, listener)` per frame (Game.cpp:1192): reads `AIL_get_timer_highest_delay`,
  `AIL_digital_latency`, `AIL_digital_CPU_percent`; queues sounds into consolidation **buckets**;
  starts samples; calls `serve()` -> `AIL_serve()` (only every 0.05s, main thread, if playing>0).
- **3D distance-cull gate** `Audio::alter` ~2058: `getDistanceSquaredFromListener() > sqr(getDistanceAtVolumeCutOff())`
  -> the sound is **stopped** (`stopSound`). Prime suspect for "door makes no sound" / ships inaudible.
- Driver: `AIL_open_digital_driver(getFrequency(), getBits(), getProviderSpec(getCurrent3dProvider()), 0)` (~1464).
- `setLargePreMixBuffer()`/`setNormalPreMixBuffer()` toggle `DIG_DS_MIX_FRAGMENT_CNT` 64/16 **AFTER**
  open (GroundScene around loads; install). Max sample handles `s_requestedMaxNumberOfSamples`=32.
- 3D sounds: `AIL_allocate_sample_handle` + `AIL_set_sample_3D_position/velocity/distances`; 2D cached via
  `AIL_set_named_sample_file` (in-memory); large 2D streams via `AIL_open_stream`.

## A/B plan to validate (the consult should pressure-test + sharpen this)
1. Build **Release** (NOT Debug — 32-bit Debug has crippling memory pressure / broken diag stack; Release
   is the only honest 32-bit baseline). See if x64 **Release** shows the same/similar in-game audio issue.
2. Check whether the **32-bit** client with the **new 9.3b DLL** shows the same signs.
3. If so, **revert the 32-bit Release client to the OLD Miles 7.2e** driver + code (pre-Phase-35) and
   probe there for **baseline numbers**: latency, sound culling, voice count, trigger->audible delay,
   3D distance/attenuation — and trace the flow.
4. Run the **same probe** against the 9.3b client (64- or 32-bit) and **compare the numbers** to isolate
   exactly what regressed.

## Git baseline for the revert (Codex to confirm exact commit)
- 9.3b source port: `d59682a54` + `17c18d99e` + `b933a61e2` (35-02, room_type/codec to 9.3b API),
  link/redist `2b8431d5a`/`dd5a7dd96` (35-03). **Pre-35-02 Audio.cpp = 7.2e API.**
- x64 disabled Miles entirely at `47665f7e1` (33-05); Phase 35 re-enabled. The clean **7.2e + 32-bit
  Release** baseline is a pre-Phase-33 commit (before x64 work) built Win32 Release with `mss32.dll`/`mss32.lib`.
- Restoration ships a 7.x-era 32-bit `Mss32.dll` (449,536 B) and an x64 9.3v `mss64.dll` — reference only.

## Tooling in place (reuse)
- `s_audioHealthProbe` (Audio.cpp, default OFF) -> once/sec `audio-health.log` (activeSamples, alloc2d/3d,
  map sizes, fileMap, latency). `s_loginMusicProbe` (default OFF) -> per-callback `login-music-probe.log`.
- 5s background process memory sampler recipe (PrivateMemorySize64) — see `[[project_x64_not_impacted_by_32bit_mem_leak]]`.
