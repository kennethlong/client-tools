# TASK (Cursor — design the comparable measurement probe)

Read `D:/Code/swg-client-v2/.planning/research/CONSULT-36-EVIDENCE.md` first (given facts), then the
engine audio code under `src/engine/client/library/clientAudio/src/win32/` (Audio.cpp, Sound2d.cpp,
Sound3d.cpp, Sample3d.cpp, SampleStream).

**Single deliverable:** a concrete, minimal **probe specification** that captures COMPARABLE numbers
across all variants (7.2e vs 9.3b x 32-bit vs 64-bit x Debug vs Release) so we can diff a known-good
baseline against the regression. The numbers must mean the same thing in 7.2e and 9.3b.

Specify EXACTLY (with the Miles API + the engine hook point/file:line to instrument):
1. **Trigger -> audible delay**: timestamp when a sound is REQUESTED (`Audio::playSound` /
   `createSampleId`) vs when `AIL_start_sample`/`AIL_start_stream` actually fires for it, vs when
   `AIL_sample_status` first reports playing. (Distinguishes engine-scheduling lag from mixer lag.)
3. **3D cull / attenuation per sound**: at the cull gate (~Audio.cpp:2058) log name +
   `getDistanceSquaredFromListener()` + `sqr(getDistanceAtVolumeCutOff())` + computed volume + the
   listener position/orientation + the sound's world position — so we see if door/ship are culled,
   mispositioned, or attenuated to zero.
4. **Voice/handle pressure**: `AIL_active_sample_count`, alloc2d/3d, the 32-handle cap, any voice-steal.
5. **Mixer health**: `AIL_digital_latency` (note it read a fixed 196 — is there a better metric?),
   `AIL_digital_CPU_percent`, `AIL_get_timer_highest_delay`. Which of these are reliable/comparable in
   BOTH 7.2e and 9.3b? Flag any API that doesn't exist or means something different in 7.2e.
6. **Output format**: a single per-event CSV/log line keyed so 7.2e and 9.3b logs are directly diffable,
   plus a rate-limited per-frame summary. Must be low-overhead (no per-callback fflush — that stuttered).

Keep it to what's necessary to localize the regression to ONE of: engine scheduling, 3D cull,
voice-steal, or mixer latency. Cite the exact instrument points. No fixes.
