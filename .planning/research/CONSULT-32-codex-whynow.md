FIRST: read `.planning/research/CONSULT-32-EVIDENCE-spawnleak.md` in this repo — those are LOCKED measured facts. Do not re-derive them. Fact #7 (GROUND TRUTH): the Tatooine spawn-load did NOT OOM in recent prior work (~2026-06-12/13); it is a RECENT REGRESSION.

YOUR ANGLE (git archaeology — the WHAT-CHANGED / why-now): Find the recent commit that introduced a per-object or per-frame memory leak that manifests on the 32-bit (x86) build during the Tatooine spawn load.

Method:
- Use `git log --since=2026-06-11 --name-only` and `git show <commit>` to review changes in the object/appearance/skeletal/scene/alter path (clientSkeletalAnimation, sharedObject, clientGame object+scene, sharedFoundation). Phase-31 (BITS-01/02/03) and Phase-32 are the window.
- For EACH candidate change, determine whether it actually affects the **32-bit (x86)** build or is x64-only/inert (e.g. `#if defined(_M_X64)` guards, `static_cast<int>` no-ops, logging-only). The MemoryManager Phase-31 change (`0fd4a74a8`) is already ruled out as x86-inert — apply the same scrutiny to the others.
- Pay special attention to changes that are FUNCTIONAL on x86: the SSE-skinning `__asm`→intrinsics port (`d9ee92088`), the "2 FUNCTIONAL x64 fixes" (`8ae95abc6`), callback/width changes (`846a2ded6`, `feaddc08e`), `AlterScheduler.cpp`. Look for: a freed-resource that is no longer freed due to a width/pointer change, a loop bound that grew, a container reserve mis-sized, a connect/registration added without the matching disconnect, an allocation added per call.

Deliver: the single most likely regressing commit + the exact hunk (file:line) + WHY it leaks per-object on x86 + your confidence. If you conclude NO recent commit plausibly leaks on x86 (i.e. the leak likely predates the window and ground-truth #7 has another explanation), say so explicitly and explain. Repo root: D:/Code/swg-client-v2.
