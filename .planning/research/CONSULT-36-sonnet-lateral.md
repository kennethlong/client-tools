# TASK (Sonnet — lateral / challenge the measurement assumptions)

Read `D:/Code/swg-client-v2/.planning/research/CONSULT-36-EVIDENCE.md` first (given facts).

We are about to spend real effort on an A/B baseline (revert 32-bit to 7.2e, probe, compare). Before we
do, challenge the framing and find the CHEAPEST discriminating experiments.

Address:
1. **Is `AIL_digital_latency`=196 (fixed, unaffected by buffer prefs) even meaningful?** What does MSS
   actually return there, and what would be a TRUE end-to-end output-latency measurement? Could 196 be a
   nominal/constant and the real perceived lag come from elsewhere?
2. **"Late OR no sound" for 3D specifically** — enumerate causes that fit BOTH late and dropped (not a
   uniform buffer delay): voice-stealing / sample priority eviction (32-handle cap, but peak was only 12 —
   does priority still cull?), the per-frame consolidation-bucket delay, 3D distance-cull at 2058,
   sample-rate/resampling mismatch (9.3b resample filter default?), 3D provider processing latency,
   doppler/velocity, listener-position update lag/order in `Audio::alter`.
3. **Debug vs Release confound:** the user will build Release anyway (32-bit Debug has memory pressure).
   Could the in-game lag be partly a **Debug-build audio-thread starvation** that Release fixes? What's
   the cheapest way to rule Debug in/out FIRST (one Release x64 run) before the full 7.2e revert?
4. **Smallest experiment set:** propose the 2-3 cheapest runs (ranked by likelihood x cost) that would
   localize the regression WITHOUT the full 7.2e revert — e.g. Release x64 A/B, toggling the resample
   filter, raising the handle cap, bypassing the cull gate temporarily.

Rank hypotheses by (likelihood x cheapness-to-test). For the top 2, give the exact one-run experiment +
what number/observation confirms or kills it. Goal: don't burn the full revert if a 1-run test settles it.
