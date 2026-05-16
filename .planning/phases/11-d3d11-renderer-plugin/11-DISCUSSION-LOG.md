# Phase 11: D3D11 Renderer Plugin - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or
> execution agents. Decisions are captured in CONTEXT.md — this log
> preserves the alternatives considered.

**Date:** 2026-05-15
**Phase:** 11-d3d11-renderer-plugin
**Areas discussed:** Plugin source-tree layout, Renderer selection mechanism, Shader compile pipeline, FFP spike methodology
**SPEC.md status:** Loaded — 7 requirements locked (ambiguity 0.11). Discussion confined to HOW; WHAT was already locked.

---

## Plugin source-tree layout (D-01)

| Option | Description | Selected |
|--------|-------------|----------|
| Copy D3D9 tree, edit in-place | Duplicate all 89 files as Direct3d11/ counterparts, then rewrite each function body. Complete starting skeleton; every Gl_api function has a home day one; easy A/B with D3D9 side-by-side. Risk: carries D3D9 idioms (D3DPOOL, lost-device) that must be excised; high noise in diffs; potential to port D3D9 bugs forward. | |
| Clean rewrite, D3D9 as reference | Start with empty Direct3d11/ tree, build Gl_api implementations file-by-file as needed. Read D3D9 source for design intent but author idiomatic D3D11 from scratch (ComPtr, explicit lifetime, modern shader model). No D3D9 idiom pollution; smaller initial commits; modern C++ patterns. Risk: blank-page start; potential to miss edge cases D3D9 plugin handles. | ✓ |
| Hybrid: skeleton + selective port | Skeleton Direct3d11/ with stub Gl_api table (119 functions returning safe defaults), MVP plugin first, then port subsystems one wave at a time. Each wave bounded; bootable-at-every-step invariant holds; reuse D3D9 logic case-by-case. Risk: more upfront design work; wave boundaries to define. | |

**User's choice:** Clean rewrite, D3D9 as reference
**Notes:** D3D11-02 explicitly bans D3DPOOL_MANAGED / OnLostDevice — a copy would pull those forward as noise. Modern C++ patterns (ComPtr, explicit lifetime) align with v2 tree's modernized direction.

---

## Renderer selection mechanism (D-02)

| Option | Description | Selected |
|--------|-------------|----------|
| rasterMajor=11 → gl11_d.dll | Reuse existing rasterMajor int. Extend Graphics.cpp:209 range to accept 11. Zero churn to selection plumbing; mnemonic (11 = D3D11); future D3D12 → rasterMajor=12. 8/9/10 gap is acceptable visual oddity. | ✓ |
| rasterMajor=8 → gl08_d.dll | Next available number after D3D9 trio (5/6/7). Contiguous numbering; smaller range-check delta. 8 has no semantic meaning; readers must remember 8=D3D11; can't slot in future D3D9 variant. | |
| New readable key: renderer=Direct3d11 | Add new ConfigClientGraphics string key that overrides rasterMajor when set; keep rasterMajor for D3D9 backward-compat. Human-readable in client.cfg; supports future renderers without numeric collision. Two parallel selection paths; one more config knob; engineers must remember which takes precedence. | |

**User's choice:** rasterMajor=11 → gl11_d.dll
**Notes:** Plumbing already trusted by engine; extension is a one-line range-check edit at Graphics.cpp:209. Existing DLL_NAME_FORMAT macro at :194 handles the naming automatically.

---

## Shader compile pipeline (D-03)

| Option | Description | Selected |
|--------|-------------|----------|
| Runtime D3DCompile, mirror D3D9 model | Replace D3DXCompileShader with D3DCompile from d3dcompiler_47.dll. Compile each shader at first use; in-memory cache by hash. Smallest delta from current behavior; no asset-pipeline touch; matches plugin-internal model. First-frame compile latency; ships d3dcompiler_47.dll runtime dep; recompiles on every client launch. | |
| Offline fxc.exe pre-compile pass | CI/build step runs fxc.exe over full shader catalog at build time; emits .cso bytecode into a packed asset. Plugin loads pre-compiled bytecode via CreateVertexShader/CreatePixelShader. Zero compile-at-runtime; faster startup; shader errors caught at build. Catalog enumeration step needed; asset pipeline touched; iteration loop slower. | |
| Hybrid: compile-on-first-use + disk cache | First run compiles via D3DCompile and writes .cso blobs to local cache; subsequent runs load from cache, recompile if source hash changes. Best-of-both — fast warm starts, no pipeline changes upfront; survives shader edits. Cache invalidation complexity; first-run latency cost; cache dir to manage. | ✓ |

**User's choice:** Hybrid: compile-on-first-use + disk cache
**Notes:** Cache directory location + d3dcompiler_47.dll packaging path left to research (Claude's discretion in CONTEXT.md). Source-hash invalidation pattern handles iteration loop.

---

## FFP spike methodology (D-04) — first Phase 11 plan per SPEC R4

| Option | Description | Selected |
|--------|-------------|----------|
| Runtime instrumentation in D3D9 plugin | Temporary counters around D3DTSS_COLOROP/D3DTSS_ALPHAOP SetTextureStageState calls in Direct3d9_StateCache. Log per-frame and per-scene; Kenny plays Tatooine + cantina ~5min each; dump CSV; revert instrumentation. Ground truth — measures actual material activations in actual scenes; falsifiable. Needs throwaway D3D9 source edits (mirrors Phase 10 D-15 pattern); ~1-day capture session. | |
| Static analysis of shader templates | Grep + parse asset-side shader templates and material definitions for FFP-tier references (TSS_COLOROP literals, MODULATE/ADD/SELECTARG1 verbs). Output FFP-using materials list cross-referenced against Tatooine/cantina assets. Zero source edits; fast iteration; full catalog of FFP materials. May miss runtime-conditional FFP paths. | |
| Both — static first, runtime to validate | Static analysis produces candidate FFP-material list; runtime instrumentation then confirms which of those materials are actually drawn in target scenes. Highest confidence; bounds the runtime capture to suspect materials only. Longest spike; need both code paths working. | ✓ |

**User's choice:** Both — static first, runtime to validate
**Notes:** Two-phase spike. Phase A (static) bounds the candidate set; Phase B (runtime) confirms what's actually drawn. If Phase A is empty, Phase B is skipped (FFP descope evidence is the static output). Phase B reverts in a THROWAWAY commit mirroring Phase 10 plan 10-07 / D-15 pattern. Verdict threshold (D-04a): any FFP activation in either target scene = build generator covering MODULATE/ADD/SELECTARG1; empty = descope generator entirely.

---

## Claude's Discretion

The following implementation details were deliberately deferred to the researcher / planner:

- Shader-cache directory location (D-03) — suggestion `<user-data>/shader-cache/` or `build-v2/shader-cache/`, researcher picks based on engine conventions
- d3dcompiler_47.dll packaging path (D-03) — system-supplied vs vendored, researcher checks minimum-target hardware
- D3D11 SDK header sourcing (SPEC §Constraints) — Windows SDK system headers vs vendor alongside DX9 vs new parallel directory
- Microsoft::WRL::ComPtr use (D-01) — idiomatic D3D11 ownership; project-wide vs alternative RAII wrapper
- Wave boundaries for the clean-rewrite plan series — suggested progression in CONTEXT.md `<decisions>` Claude's Discretion
- DPVS remeasurement tool choice (SPEC R7) — PIX / Nsight / GPUView, researcher picks based on Kenny's hardware + capture-protocol fit
- Renderer-tag plumbing for TAG_DX11 (D-02a) — declaration site + caller audit
- D3D9 baseline screenshot capture timing (SPEC R6) — suggestion: first plan after the FFP spike, before any D3D11 code lands

## Deferred Ideas

Future phases / out-of-scope items captured in CONTEXT.md `<deferred>`:

- Space scenes (JTL nebulae, ship combat) under D3D11 — SPEC §Boundaries
- Other planets under D3D11 — SPEC §Boundaries
- Pixel-perfect parity with D3D9 — SPEC §Boundaries
- Performance parity with D3D9 — SPEC §Boundaries
- Deletion of Direct3d9 plugin tree — SPEC §Boundaries
- Direct3d9_ffp / Direct3d9_vsps D3D11 mirrors — D3D11 has no FFP pipeline; SM5.0 unifies
- Compute shaders, tessellation, geometry shaders — unused by SWG
- Cantina corner-snap engine-improvement fix — separate todo, not a Phase 11 target

### Reviewed todos (not folded)

Two todos returned by `gsd-sdk query todo.match-phase 11` were reviewed but NOT folded into Phase 11 scope:

- `2026-05-15-cantina-corner-snap-engine-improvement.md` — pre-existing SOE engine quirk; cross-client confirmed; not a Phase 11 regression target
- `2026-05-15-swgsource-vs-whitengold-tre-asset-diff.md` — space-scene research; Phase 11 explicitly excludes space scenes per SPEC §"Adjacent research item"
