# CONSULT-62 task (Codex) — engine-side thread & Miles-mutex inventory for the zone-in window

FIRST read `.planning/research/CONSULT-62-audio-residual-crackle-EVIDENCE.md` in this repo — its facts are LOCKED ground truth.

Your angle (repo tracing, engine side only — do NOT analyze the Miles SDK internals, another consultant owns that):

1. THREAD INVENTORY: enumerate every thread the engine/client creates (runNamedThread, CreateThread, _beginthreadex, std::thread) under `src/engine` and `src/game`. For each: creation site (file:line), purpose, and any SetThreadPriority / SetThreadAffinityMask / SetThreadIdealProcessor / SetPriorityClass applied to it or to the process. Present as a table.

2. MAIN-THREAD MILES CALLS DURING ZONE-IN: trace the zone-in code path (GroundScene ctor/load loop, sound starts via Audio::playSound / Audio::playMusic / attachSound, Audio::alter) and list every call site that enters Miles (AIL_*) from the main thread, especially ones plausibly slow (stream opens, named-sample file loads, decoder inits, sample allocation/starts). Note any engine use of AIL_lock_mutex/AIL_unlock_mutex.

3. Identify the top 2 engine-side suspects for (a) the 1-2s zone-in crackle and (b) the rare D3D9-only in-game crackle, each with the exact call chain (file:line) and a one-line cheapest-probe suggestion.

Output: plain text/markdown, tables preferred, file:line citations for every claim.
