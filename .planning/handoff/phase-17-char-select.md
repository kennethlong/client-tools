# Handoff: Phase 17 — char-select D3D11 beachhead

**Updated:** 2026-05-29 (Waves 5–6 + 17-06b executed; 17-07 is all that remains before 17-05 T4–5)
**Branch:** koogie-msvc-cpp20-base
**Worktree:** D:\Code\swg-client-v2
**Status:** Plans 17-01..17-04 **done** · 17-05 **T1–3 DONE, PARKED** at `17-05-task3` (PRE-gap `6668b7cac`) · 17-06a **DONE** (`cafbe6111`+`73d15101f`: userConstants = flat float4[17]) · 17-06b **DONE — Case C DEFERRED** (`97d7bbf93`+`c701b229e`: latent `writeVarFloat4AtOffset`, no evidence-backed offset; dual-AI confirmed) · **17-07 NEXT (the only remaining code plan)** · 17-05 T4–5 **deferred to after 17-07**

> **RESUME HERE → execute 17-07, then 17-05 Tasks 4–5.** 17-07 is the big one: HLSL parameter-list
> parser (`rewritePsMainParameterListForVSOutputs`) + per-VS rewrite cache (VS-output-sig-hash keyed,
> pointer-reuse invalidated) + per-VS reflected-inputs cache (HIGH-6) + StateCache bind wiring
> (native > rewritten > fallback) + `asset-PS bound=` log. Touches PSData.h + VertexShaderData.h →
> STAGE-1 MUST rebuild ALL plugin .vcxprojs + relink SwgClient_d.exe (ABI cascade trap). Bump
> D3D11_REWRITE_VERSION 21→22 at BOTH sites (PixelShaderProgramData.cpp:153 + :303). Do the built-in
> COLOR0/TEXCOORD spike FIRST (de-risk the parser) before the full impl. After 17-07's combined build,
> APPEND the "POST-gap build provenance" line (HEAD SHA + gl11_d.dll mtime) to evidence/README.md §6,
> then run 17-05 Task 4 (POST-gap capture) + Task 5 (author real 17-VERIFICATION.md).

> **Execution note (this session):** running inline/sequential (Kenny-in-the-loop), not parallel
> worktrees — the executor cannot boot the client (no `.tre`), every capture is a host checkpoint.
> Wave-only fallback ordering is in effect (orchestrator does not resolve named partial deps).

## PRE-gap baseline (recorded for Task 5's PRE/POST table)

Host boot 2026-05-29 12:55, `stage/d3d11-debug.log` (32,594 lines), HEAD `9f5db9c3f`:
`id=342`=0 · `id=343`=27 · `PSRC recompile FAILED`=0 · `wroteDiffuse=1`=0 · `wroteEmissive=1`=0 ·
`wroteSpecular=1`=6 · `asset-PS bound=`=0 · `fallback-PS bound=`=0 · `Plan 17-07 COMPATIBLE/INCOMPATIBLE`=0/0 ·
native `Plan 17-04 Task 1 INCOMPATIBLE`=27 (real PRE fallback driver). PRE state: GAP-2 open (0
diffuse/emissive writes), GAP-3 open (27 incompatible pairs, 0 asset-PS binds). PNGs at 1920×1080:
`evidence/char_default_d3d9_0003.png` + `evidence/char_default_d3d11_0003_preGap.png`.

## D3D9 capture workaround (needed again for Task 4's D-06 re-boot)

Current Debug binaries CANNOT boot D3D9 char-select (post-17-01 `SwgClient_d.exe` ABI-cascades with
`gl05_d.dll` — parked Phase-17.X gl05 regression). **Capture D3D9 via the pre-17-01 Release stack:**
`stage/SwgClient_r.exe` + `gl05_r.dll` (both 5/27). Release exe reads `stage/client.cfg`, which is
edited (reversible) to `rasterMajor=5` + 1920×1080 — **leave it** for Task 4's D3D9 re-boot; revert at
phase close. Use the in-client **F12** key (back-buffer dump to `stage/screenshots/`), not the OS snip
tool (fights fullscreen). jpg→png transcode for canonical naming.

---

## Pick up here

1. Read `.planning/STATE.md` and `.planning/phases/17-psrc-census-char-select-beachhead/17-VERIFICATION.md`.
2. Execution order after 17-05 Task 3 (Round-5 review item 2 — 17-07 MUST build LAST; `depends_on` enforces 17-06b -> 17-07):
   - **17-06a** (Wave 6, human checkpoint) — extend cbuffer discovery dump in `Direct3d11_StaticShaderData.cpp` (re-reflect via D3DReflect is MANDATORY — cached layout has no type info)
   - **17-06b** (Wave 7, autonomous, `depends_on: 17-06a`) — `writeVarFloat4AtOffset` + diffuse/emissive writes from 06a evidence (evidence-backed offset ONLY; folklore-slot writes forbidden — take Case-C DEFERRED instead)
   - **17-07** (Wave 7, human checkpoint, `depends_on: 17-04 + 17-05-task3 + 17-06b`) — PS parameter-list rewrite + StateCache bind path + per-VS cache + `Plan 17-07 COMPATIBLE vs=` rewritten-lane log; touches PSData.{cpp,h} + StateCache.cpp + VertexShaderData.{h,cpp}; bumps `D3D11_REWRITE_VERSION` 21->22 (live is already 21); STAGE-1 all-plugin rebuild runs LAST so the post-gap gl11_d.dll contains BOTH 17-06b and 17-07 (GAP-3, **primary visual driver**)
   - **17-05 Tasks 4–5** — POST-gap boot capture (hard-gated on the recorded 17-07 commit SHA + gl11_d.dll mtime in evidence/README.md) + CHAR verdict authoring
3. Rebuild **all plugin vcxprojs** after any `Direct3d11_*.h` touch (stale gl11_d.dll trap).
4. **Orchestrator contract for the `17-05-task3` named partial dependency (Round-5 review item 8):** 17-06a/06b/07 declare `depends_on` including `17-05-task3`, which is satisfied by Plan 17-05's Task-3 PRE-gap commit (the `## TASK 3 HANDOFF — PARK POINT` block, literal `17-05-task3`), NOT by 17-05 reaching its final task. If the orchestrator cannot resolve a named partial dependency, fall back to WAVE-ONLY ordering: Wave 5 (17-05 PRE-gap commit) -> Wave 6 (17-06a) -> Wave 7 (17-06b then 17-07) -> resume 17-05 Tasks 4–5. Either way 17-07 builds last in Wave 7.

---

## Gap inventory (char-select)

| Gap | Plan | Delivers |
| --- | --- | --- |
| GAP-1 | 17-05 | Screenshot A/B evidence (D3D9 vs D3D11) |
| GAP-2 | 17-06a/06b | Cbuffer diffuse/emissive **instrumentation** (`wroteDiffuse=1`) — **not** primary char-select visuals |
| GAP-3 | 17-07 | Asset PS bind rate + `asset-PS bound=` log — **primary char-select visual driver** |

Char-select PSes consume `textureFactor.rgb` + `COLOR0`, not cbuffer material colors (see `evidence/plan-17-04x-psrc-source-dump.txt`).

---

## Planning changes (uncommitted)

| Change | Detail |
| --- | --- |
| Deleted | `17-06-PLAN.md` (monolithic) |
| Added | `17-06a-PLAN.md` (discovery dump), `17-06b-PLAN.md` (offset writes) |
| Rewritten | `17-07-PLAN.md` — parameter-list parser primary; touches StateCache + PSData.h + VertexShaderData.{h,cpp}; real compile+reflect Task-0 spike; `Plan 17-07 COMPATIBLE vs=` rewritten-lane log; `D3D11_REWRITE_VERSION` 21->22 |
| Updated | `17-05-PLAN.md` (token + build-provenance + orchestrator-contract), `17-06a` (re-reflect mandatory), `17-06b` (evidence-backed-offset-only), `17-RESEARCH.md`/`17-PATTERNS.md`/`17-VERIFICATION.md` (REWRITE_VERSION live=21, next=22) |

Execute via GSD: `/gsd-execute-phase 17` or individual plan files.

---

## Boot gate

Both `rasterMajor=5` and `rasterMajor=11` must boot to char-select. D3D9 path byte-for-byte unchanged.