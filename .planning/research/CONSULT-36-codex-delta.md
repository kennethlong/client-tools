# TASK (Codex — repo tracer / 7.2e->9.3b audio code-delta + revert baseline)

Read `D:/Code/swg-client-v2/.planning/research/CONSULT-36-EVIDENCE.md` first (given facts).

**Single question:** What EXACTLY changed in the audio path between the working Miles **7.2e** integration
and the current **9.3b** port that could affect 3D voice scheduling / culling / latency — and what is the
precise git baseline to revert to for a known-good 7.2e measurement?

Do, citing commit hashes + file:line:
1. Full diff of `src/engine/client/library/clientAudio/src/win32/Audio.cpp` (and Sound2d/Sound3d/Sample3d,
   SampleStream, ConfigClientAudio, FirstClientAudio, and the .vcxproj link/lib changes) across the
   Phase-35 port commits (`d59682a54`, `17c18d99e`, `b933a61e2`, `2b8431d5a`, `dd5a7dd96`, plus the
   Phase-33 disable `47665f7e1`). Focus on ANY change to: `AIL_open_digital_driver` args/flags,
   `AIL_set_preference` calls, sample-handle allocation/limits, 3D position/distance/velocity calls,
   the distance-cull gate, the per-frame `serve()`/`AIL_serve` cadence, room_type/reverb, provider
   selection (`getProviderSpec`/`getCurrent3dProvider`), frequency/bits.
2. Identify the **exact commit** that is the last clean **7.2e + 32-bit** state (pre-Phase-33 x64 work)
   to checkout for the baseline build, and list what files must come from there (Audio.cpp, the .vcxproj
   Miles lib/redist pointing at `mss32.lib`/`mss32.dll`, any 7.2e headers).
3. Flag anything in the 7.2e->9.3b delta that is a behavior change (not just an API rename) — e.g. a
   dropped preference, a changed default, a flag that was passed in 7.2e but not 9.3b.

Output: a ranked list of "suspicious deltas" (most-likely-to-affect-3D-SFX first) with hash+file:line,
plus the concrete revert recipe (commit + file list). No fixes — just the delta map and baseline path.
