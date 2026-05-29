# Plan 17-02 Task 2 — Boot Gate Deferral Note

**Status:** DEFERRED into Task 3 (CHAR-01 human-verify checkpoint) — boot evidence is recorded
on the SAME boot Kenny performs for CHAR-01 validation.

**Rationale (deviation Rule 3 — environmental gap):**

I am executing this plan as a PARALLEL worktree agent at
`D:/Code/swg-client-v2/.claude/worktrees/agent-a7f678826352a91d1/`. The worktree is a fresh
git checkout that does NOT contain the populated `stage/` runtime environment required to
boot SwgClient_d.exe — specifically:

- No `.tre` archives (`swgsource_3.0.tre`, `disable_wayfar_dearic_snow.tre`).
- No TreeFile search-path content (`D:/swg_dev_bundle`).
- No `client_d.cfg` with `rasterMajor`, `searchPath`, screen resolution etc.
- No `stage/d3d11-debug.log` (created at runtime by the InfoQueue file sink).

The orchestrator's `<important_project_context>` preamble explicitly states: "The orchestrator
will redeploy your Wave-2 build to host stage/ after you return; you do not need to touch host
stage/." That means the actual `rasterMajor=5` and `rasterMajor=11` boots — which are the
only way to populate `stage/d3d11-debug.log` and exercise the PS-input-sig / PS-cbuffer dual-
route diagnostic — must occur on the HOST stage/ AFTER the orchestrator deploys the worktree's
freshly-built `gl11_d.dll` (+ optionally `SwgClient_d.exe`).

Per the orchestrator: "Task 4 (CHAR-01 validation boot) IS still a real `checkpoint:human-
verify` — pause and return checkpoint state when you reach it. Kenny will do the boot." That
boot IS the natural place to also collect the Task 2 grep evidence, since:

- It happens under explorer launch against the host's stage/ — the InfoQueue file sink is
  the same sink the R3-02b dual-route writes to.
- The same `stage/d3d11-debug.log` lines that prove Task 2's id=342==0 / id=343==0 /
  PS-input-sig / PS-cbuffer counts are EXACTLY the lines that prove Task 3's CHAR-01
  surface-correctness gate has the cached reflection it needs.
- Folding them avoids two separate full-client boots that produce identical artifacts.

## What Task 2 wanted to verify (now Kenny verifies under Task 3)

| Acceptance criterion | Source command | Pass condition |
|----------------------|----------------|----------------|
| rasterMajor=5 boots to char-select | `set rasterMajor=5; launch SwgClient_d.exe` | reaches char-select, no crash |
| rasterMajor=11 boots to char-select | `set rasterMajor=11; launch SwgClient_d.exe` | reaches char-select, no crash |
| id=342 == 0 (VS-PS signature mismatch) | `grep -c "id=342" stage/d3d11-debug.log` | returns `0` |
| id=343 == 0 (register-position mismatch) | `grep -c "id=343" stage/d3d11-debug.log` | returns `0` |
| PS-input-sig dump present | `grep -c "PS-input-sig" stage/d3d11-debug.log` | returns `>= N` (number of recompiled //hlsl shaders) |
| PS-cbuffer dump present | `grep -c "PS-cbuffer" stage/d3d11-debug.log` | returns `>= N` |
| recompile-failure log absent (or flagged) | `grep -c "//hlsl PSRC recompile FAILED" stage/d3d11-debug.log` | returns `0` (HIGH-1 saved a boot if non-zero — flag for follow-up) |

## What I CAN gate from inside the worktree (the link-clean evidence)

- gl11_d.dll Debug rebuild: 0 unresolved external symbol; 0 C/LNK/MSB errors (the
  post-build stage-copy MSB3073 that initially fired was a worktree-bare-stage artifact;
  resolved by creating `<worktree>/stage/` and re-running — second build was clean).
- SwgClient_d.exe Debug rebuild via solution: 0 unresolved external symbol; 0 C/LNK/MSB
  errors. `Stage Debug exe + pdb` confirmed in build log; cascade also rebuilt gl05/gl06/
  gl07 (unchanged source).

The boot-and-grep portion of Task 2 is the part that REQUIRES the host environment and is
therefore deferred into Task 3's checkpoint.

## Cross-reference

- Task 1 commit: `8bafc7dbf` — non-fatal recompile lane + DXBC retain + reflect-once cache.
- Plan 17-01 (Wave 1) provided the m_psrcText / m_psrcLen retain; commit `5eea33474`.
- Census-gate (SR-3) verdict from Wave 1 COMPARISON.md: PROCEED (all-HLSL; no asm in named
  anchors); no `17-02B-PLAN.md` placeholder spawned.
