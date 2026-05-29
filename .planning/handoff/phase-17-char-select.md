# Handoff: Phase 17 — char-select D3D11 beachhead

**Updated:** 2026-05-29 (planning complete, execution not started)
**Branch:** koogie-msvc-cpp20-base
**Worktree:** D:\Code\swg-client-v2
**Status:** Plans 17-01..17-04 **done** · 17-05 **parked at Task 3** · 17-06 split → **17-06a + 17-06b** · 17-07 **rewritten (Round-4 review)** · **No engine code changes in current WIP diff**

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