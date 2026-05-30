# Handoff: Phase 17 — char-select D3D11 beachhead

**Updated:** 2026-05-30 (17-05 T4–5 DONE → CHAR-01/02/03 = PASS; Phase 17 char-select verification COMPLETE)
**Branch:** koogie-msvc-cpp20-base
**Worktree (D3D11 work):** `D:\Code\swg-client-v2-d3d11` — Cursor uses the main checkout `D:\Code\swg-client-v2` on `swg-blender-m16`. Build gl11 in the worktree with `& $env:MSBUILD <worktree>\src\build\win32\swg.sln /t:Direct3d11 /p:Configuration=Debug /p:Platform=Win32 /m /nodeReuse:false`. The worktree `stage` is a **junction → main stage**, so the postbuild copy auto-deploys `gl11_d.dll` to `D:\Code\swg-client-v2\stage` (no manual copy; see memory `project_d3d11_worktree_stage_junction`).
**Status:** 17-01..17-04 done · 17-05 T1–3 PARKED · 17-06a/06b done · 17-07/GAP-3 DONE (9/9 binds) · GAP-4 (PS b0 lighting) + GAP-5 (VS vertex lighting) DONE `e1db7bf65` · **GAP-6 (bump sleeves/hands) FIXED+COMMITTED `a0d5ac80f` (pushed to origin)** → char-select asset-PS renders LIT+textured for ALL parts (face/skin, tunic, pants, shoes, AND bump sleeves/hands). · 17-05 T4–5 (A/B + verdict) still pending.

> ## ▶ PHASE 17 char-select VERIFICATION COMPLETE (2026-05-30) — CHAR-01/02/03 = PASS
>
> **17-05 T4–5 DONE** (commit `bb4b13a00`). Matched-pair A/B captured on Kenny's host: D3D9
> `screenShot0029` (rasterMajor=5 Release stack) vs D3D11 `screenShot0030` (rasterMajor=11) →
> near-identical lit+textured char-select; only residual is a minor brightness/tone delta tracked
> under **GAMMA-01 (Phase 19)**, out of CHAR scope. POST-gap metric set: `asset-PS bound=`**9**,
> `COMPATIBLE`**9** / `INCOMPATIBLE`**0**, `id=342`=0, `PSRC recompile FAILED`=0 (asset-PS lane
> delivering — the primary char-select visual driver). Verifier-produced `17-VERIFICATION.md`
> supersedes the synthesized stand-in (`a20e828d8`); 4 evidence PNGs committed (D3D9 + preGap +
> postGap + canonical alias); evidence/README.md §6 provenance corrected to the GAP-6 build.
>
> **GAP-1 closed.** No remaining char-select work. Possible next steps: push `bb4b13a00` to origin;
> run `/gsd:verify-work 17` for the formal pass; close/audit the phase; or advance to the next
> ROADMAP phase. NOTE: the 17-05-PLAN Task-5 acceptance criteria literally greps
> `authored_by: claude-opus-4.7` — the doc honestly records `claude-opus-4.8` (the real author), so
> that one stale literal-string gate will mismatch; the field's provenance intent is satisfied.
>
> _(Superseded resume blocks below — kept for the root-cause derivation history.)_
>
> ### Earlier resume: 17-05 Tasks 4–5 (now DONE)
> capture D3D9 baseline (rasterMajor=5 via the pre-17-01 Release stack — see "D3D9 capture
> workaround" below) + author `17-VERIFICATION.md`. Expect CHAR-01/02/03 PASS on pipeline/lighting and ALL
> body parts now (bump parts included). Append the POST-gap build provenance (HEAD `a0d5ac80f` + gl11_d.dll
> mtime) to evidence/README.md §6 before the A/B.
>
> ### How GAP-6 was actually fixed (the multi-stream theory was WRONG)
> The purple/green bump arms were NOT a tangent/NaN problem. Full detail: memory
> `project_d3d11_multitexture_srv_slot_swap`. Two parts:
> 1. **Tangent**: the per-key VS texcoord-set variant (Fix A — `Direct3d11_VertexShaderData`/`VertexDeclarationMap`
>    per-`textureCoordinateSetKey` recompile) gave the bump VS its real DOT3 tangent at TEXCOORD1. This did
>    NOT require activating multi-stream — single-stream per-key remap sufficed, so the
>    `SoftwareBlendSkeletalShaderPrimitive` multi-stream-activation rabbit hole was unnecessary.
> 2. **The real culprit — TEXTURE-SLOT SWAP**: fxc assigns SM4 texture registers by first-USE order when
>    splitting legacy `sampler2D`. specmap_bump samples `normalMap` before `diffuseMap` → normalMap=t0,
>    diffuseMap=t1, opposite the engine's D3D9 stage-index SRV bind → the blue normal map was sampled as
>    albedo. Fixed by a reflection-driven stage→SRV-slot remap (`Direct3d11_PixelShaderProgramData::
>    getSrvSlotForStage`, used in `Direct3d11_StaticShaderData::apply`). Also added the VS material c11–c15
>    feed (`Direct3d11_StateCache::setVertexMaterialColors`, mirrors D3D9 `VSCR_material`). The VS-NaN that
>    the old GAP-6 handoff (`gap6-bump-arms-vs-lighting-nan.md`) chased was already fixed by GAP-5 + the
>    material feed — RenderDoc "Debug this Vertex" confirmed finite COLOR0/COLOR1.
> The GAP-6 multi-stream infra (caps flip, VertexBufferDescriptorMap changes) committed in `e1db7bf65`
> remains DORMANT/unused — harmless; revisit only if a true multi-stream asset ever needs it.
>
> _(Superseded GAP-4 design block below — kept for the root-cause derivation; GAP-4 is now DONE+committed.)_
>
> **17-07 / GAP-3 is DONE** (commits `f9e5ac569`→`d8cc1ca99`, SUMMARY `8639c5458`). The asset-PS bind
> rate went 0/9 → **9/9** (`path=rewritten`) via VS-signature RECONSTRUCTION (axis-b reorder was wrong —
> the mismatch was a register-BASE offset: VS reserves o0=SV_Position, asset PS starts v0; reorder can't
> add the +1 base, so we now rebuild the PS input sig to mirror the VS exactly + wrap the asset main).
> Enabling the lane revealed **GAP-4: char-select body renders BLACK** — the asset PS reads zero lighting
> constants.
>
> **GAP-4 fix is fully designed + dual-AI converged + census-gate UNBLOCKED** (codex+cursor:
> `.planning/research/CONSULT-17-07-gap4-cbuffer-reconcile*`; full design in memory
> `project_17_07_ps_register_base_offset`). Root cause: asset PS reads `SwgVertexConstants`@b0 but the
> light constants (c0-c4) are pushed to b0 by NOBODY (D3D11 LightManager only fills `LightingCB`@b3);
> `StaticShaderData::apply` zero-inits b0 and clobbers it; engine userConstants go to b2. The c-index map
> is IN-REPO at `Direct3d9_PixelShaderConstantRegisters.h` (`PSCR_dot3LightDirection=c0 ... userConstant=c8`),
> matching the reflected b0 offsets — **no TRE .inc extraction needed.**
>
> **GAP-4 implementation (the converged plan):** a persistent D3D11 b0 shadow[400] fed by 3 producers:
> (A) port `Direct3d11_LightManager` to push `pixelDot3Data` (5 float4s) → shadow@c0 (from
> `Direct3d9_LightManager.cpp:601`/~592-625; specPower packs into `dot3LightDirection.w`); (B)
> `StaticShaderData::apply` RMW from the shared shadow (STOP the `staging(400,0)` zero-init) + patch
> textureFactor/materialSpecularColor by reflected offset; (C) `setPixelShaderUserConstants_impl` →
> shadow@`(PSCR_userConstant+i)*16` (offset 128+, NOT c0); upload merged shadow via `updatePS(0)`; keep
> b2/fallback untouched. Instrument b0 c0.xyz/c1.rgb/c7/c8 at flush (pass = c0 nonzero + lit character).
>
> **Also still PENDING:** D-06 `rasterMajor=5` re-boot (gl11-only change → D3D9 byte-for-byte; needed for
> 17-05 Task 4's D3D9 re-capture). 17-05 Tasks 4–5 (POST-gap A/B + 17-VERIFICATION.md) best run AFTER
> GAP-4 so they capture a LIT character, not the black interim → CHAR-01/02/03 verdict.
>
> `gl11_d.dll` currently = HEAD `d8cc1ca99` (5/29 17:23, GAP-3). `SwgClient_d.exe` (5/28) unchanged/compatible.

---

_Historical (pre-execution 17-07 plan notes — SUPERSEDED by the GAP-4 resume block above; 17-07 is done):_

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