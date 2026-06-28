# TASK (Opus — A/B experiment matrix + confound isolation methodology)

Read `D:/Code/swg-client-v2/.planning/research/CONSULT-36-EVIDENCE.md` first (given facts).

The user's plan: (1) build Release; (2) check 32-bit + new 9.3b DLL; (3) revert 32-bit Release to OLD
Miles 7.2e for a baseline, probe everything, trace flow; (4) run the same probe on the 9.3b client and
diff. **Pressure-test and sequence this into a rigorous experiment design.**

There are 4 confounded variables: **Miles version** (7.2e vs 9.3b), **bitness** (32 vs 64), **build**
(Debug vs Release), and **OS audio path** (DirectSound emulated over WASAPI on Win11 — constant). The
naive plan risks comparing 7.2e-32-Release against 9.3b-64-Debug and mis-attributing the delta.

Produce:
1. **The experiment matrix** — the minimal set of build/run configs that isolates EACH variable (esp.
   version vs bitness vs Debug/Release), with the one-variable-at-a-time deltas each comparison yields.
   Note which configs already exist vs must be built. (Known-good anchor = 7.2e + 32-bit + Release.)
2. **Normalization:** what must be held equal across runs for the numbers to be comparable (same area/
   route, same cfg, same sample set, same listener path, frame-rate effects on per-frame probes,
   `AIL_digital_latency` caveats). How to make the probe output diffable.
3. **Decision tree:** given the cheap runs first (Release x64 same-or-not? 32-bit+9.3b same-or-not?),
   what each outcome implies and whether it short-circuits the full 7.2e revert.
4. **Failure modes of the plan** — e.g. if 7.2e-32-Release ALSO shows the symptom (then it's not a Miles
   regression at all but our engine port / x64 / Win11), or if Release fixes it (Debug artifact).

Output: a numbered run sheet (config -> what to measure -> what the delta tells us), ordered cheapest-
and-most-discriminating first. This is methodology only — no code, no fixes.
