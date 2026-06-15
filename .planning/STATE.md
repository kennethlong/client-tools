---
gsd_state_version: 1.0
milestone: v2.3
milestone_name: Hardening
status: executing
last_updated: "2026-06-15T15:10:56.693Z"
last_activity: 2026-06-15
progress:
  total_phases: 7
  completed_phases: 6
  total_plans: 19
  completed_plans: 17
  percent: 89
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-06-12 after v2.2 close)

**Core value:** Every change must leave the client bootable to character select. Visual parity achieved in v2.2 — D3D11 now matches the D3D9 baseline; never regress either renderer. The v2.3 TRE compare tool is a standalone web app, outside that invariant but inside this milestone.
**Current focus:** Phase 30 — tre-compare-tool-frontend-spa

## Deferred Items (acknowledged at v2.0 close)

NOTE: "Remove dead code (CLEAN-01..04 vs MSBuild)" is now CLOSED — it was the v2.1 Decruft milestone (Phases 12–16, shipped 2026-05-27).

Acknowledged and deferred at v2.0 milestone close (2026-05-25):

| Category | Item | Status |
|----------|------|--------|
| debug | cantina-corner-snap | closed (pre-existing engine quirk; not a regression) |
| debug | safecast-null-cast | closed (resolved 2026-05-15) |
| todo | 2026-05-15-cantina-corner-snap-engine-improvement | low — workaround exists |
| todo | 2026-05-15-swgsource-vs-whitengold-tre-asset-diff | low — informational, post-Phase-11 |
| backlog | Dead-code re-removal (CLEAN-01..04) vs MSBuild tree | **CLOSED — shipped as v2.1 Decruft (Phases 12–16)** |
| backlog | DPVS D3D11 remeasure (SPEC R7); CLEAN-06 ~30 tools | DPVS-01 now scheduled as v2.2 Phase 23; CLEAN-06 tools carried to backlog |

## Deferred Items (acknowledged at v2.1 close)

5 open artifacts acknowledged and deferred at v2.1 Decruft milestone close (2026-05-27). All non-blocking — none are v2.1 regressions:

| Category | Item | Status |
|----------|------|--------|
| debug | cantina-corner-snap | closed (pre-existing SOE engine quirk; not a regression) |
| debug | safecast-null-cast | closed (resolved 2026-05-15, ContrailData D-18 guard) |
| todo | 2026-05-15-cantina-corner-snap-engine-improvement | low — annoying, workaround available |
| todo | 2026-05-15-swgsource-vs-whitengold-tre-asset-diff | low — informational, post-Phase-11 |
| todo | 2026-05-27-options-toolbar-cooldown-ui-data-mismatch-crash | medium — pre-existing Options-window FATAL (`checkShowToolbarCommandCooldownTimer` `.ui`-data mismatch from feature commit `d1b3c0eaf`); OUTSIDE the v2.1 diff, NOT a Decruft regression |

Plus v2.2-coupled deferrals: `stage/client_d.cfg` accumulated test-settings cleanup (after v2.2 visual parity); AR-15-01 future-TCG-revival re-evaluation (future v2.x). See `milestones/v2.1-MILESTONE-AUDIT.md`.

## Deferred Items (acknowledged at v2.2 close — now folded into v2.3 scope)

7 open artifacts acknowledged at v2.2 Visual Parity milestone close (2026-06-12). **Several are now scheduled as v2.3 requirements** (cross-referenced below):

| Category | Item | Status |
|----------|------|--------|
| debug | cantina-corner-snap | closed (pre-existing SOE engine quirk) — fix now scheduled as **HARD-02 / Phase 25** |
| debug | safecast-null-cast | closed (resolved 2026-05-15) |
| todo | 2026-05-15-cantina-corner-snap-engine-improvement | → **v2.3 HARD-02 (Phase 25)** |
| todo | 2026-05-15-swgsource-vs-whitengold-tre-asset-diff | → first real use case of the **v2.3 TRE compare tool (Phases 28–30)** |
| todo | 2026-05-27-options-toolbar-cooldown-ui-data-mismatch-crash | → **v2.3 HARD-04 (Phase 26)** |
| todo | 2026-05-31-port-d3d9-shader-compile-to-d3dcompile | → **v2.3 HARD-05 (Phase 27)** |
| todo | 2026-06-12-config-gate-dpvs-occlusion-per-phase-23-verdict | → **v2.3 HARD-01 (Phase 24)** |

Plus the v2.2 audit `tech_debt` list (see `milestones/v2.2-MILESTONE-AUDIT.md`): D-15 instrumentation removal (→ **HARD-03 / Phase 26**), machine-specific stage/override + stage/miles paths (→ **PORT-01 / Phase 24**), `stage/client_d.cfg` cleanup (→ **PORT-02 / Phase 24**), blend-factor scene-sweep risk, stale comment at Direct3d11_StaticShaderData.cpp:1655, missing Nyquist VALIDATIONs (18, 19–22 — deferred as VAL-01, milestone audit stands as the verification record).

## Current Position

Phase: 30 (tre-compare-tool-frontend-spa) — EXECUTING
Plan: 2 of 3
Plans: 30-01 SPA scaffold + static mount — DONE: 544359067 (scaffold) + e8b5154b0 (vitest/shadcn/tanstack) + 626b9a073 (test RED) + dd4e1da2b (feat GREEN). TRE-05 ticked. | 29-01 diff engine + deps — DONE: 4741094f0 + af7cd45e5 + a9eaced54 (TRE-02/03/04 ticked). 29-02 sqlite cache — DONE: c661c6aa3 (test RED) + 963a0ad0d (feat GREEN). 29-03 FastAPI surface — DONE: 7eb055cbf (config) + baf2e18b9 (test RED) + b0d5442b4 (feat GREEN) + 46eac9150 (integration test). (Phase 28 foundation: 28-01 ef582ae73…; 28-02 f222dc876…; 28-03 d0a784c16…; 28-04 behavioral suite landed.)
Outcome (29-01): pure `tre_compare.diff` engine (NO fastapi/sqlite3 — Phase-30/TREM headless import). `diff_archive_set` keyed by `(basename, kind)` (tree↔toc collision = two rows), fault-wrapped `stat_archive` (corrupt archive → `fault` row, never aborts). `diff_virtual_trees` lean `(length,compressed_length)` tri-state rows — never crc — + optional `qualifier` (tombstoned/rejected/error _left/_right; tombstoned-both never vanishes) + summary (per-side node_errors/rejected/tombstoned + status_counts). `drill_in` domain status ok/not_found/rejected + winner/shadowed/verdict; `hash_virtual_file` xxh3_64 of DECOMPRESSED bytes with SYMMETRIC TREE/TOC payload resolve (match `fix_up_file_name(e.path)==vpath`, read by RAW `e.path`) and `_HASH_FAULTS=(*_NODE_FAULTS,KeyError)` never-raise (Opus). Structured `DriveHashResult`. Deps: fastapi 0.137 / uvicorn 0.49 BARE (uv.lock grep-proven, no uvloop/httptools) / xxhash 3.7 + httpx dev. RED→GREEN; 14 diff tests + 52/52 synthetic suite green. Two deviations: posixpath.normpath rejected-key recovery (Rule 3), build_tre `payloads=` for deterministic false-identical bytes (Rule 2).
Outcome (29-02): stdlib-sqlite3 `tre_compare.cache` (NO fastapi). `Cache.archive_entries(node, node_errors, *, no_cache)` parse-skip cache keyed `(abspath, mtime_ns, size)` — INTEGER `st_mtime_ns`, never float (P4 closes FAT32/network 1-2s + float-rounding false-HIT). HIT proven by a spy on `tre_compare.cache.iter_node_entries` (call-count==1 across two calls) — parse-skip, NOT row-equality (P5). `build_virtual_tree_cached(scan, cache)` re-expresses the Phase-28 merge VERBATIM sourcing tree/toc rows from sqlite + `path` nodes from the live walk, threading `node.kind` from the LIVE node (a row carries no kind — P2); parity-equivalent to `build_virtual_tree` across an EXPANDED 7-case matrix incl. shadowed-tuple ORDER + rejected LIST order. `tre_file` stored per toc row (Pitfall 7 — drill-in without re-parsing a 193k-entry `.toc`). `get_file_hash`/`put_file_hash` memo the Plan-01 hexdigest. Concurrency: `check_same_thread=False` + WAL + `busy_timeout=5000` + a write `Lock` guarding the MISS detect+INSERT with an under-lock re-check + `INSERT OR IGNORE` (no concurrent-MISS 500, Sonnet); `__file__`-relative default db under `tools/tre-compare/.cache/` (gitignored). RED→GREEN; 8 cache tests + 60/60 synthetic suite green. One deviation: parity_matrix fixture had `a/t.dds` real in the top tree (case-1 never exercised) → removed it so the tombstone wins first (Rule 1).
Outcome (29-03): `tre_compare.api` — a `create_app(cache, installs_path)` FACTORY (NO module-level Cache leak, D-01/D-03) exposing four STATELESS routes composing the Plan-01 pure diff over the Plan-02 cached merge: `/compare/set` (corrupt archive → fault row, never 500), `/compare/files` (FULL `{rows,summary}`, no pagination D-08), `/file/detail` (`drill_in` not_found→404 / rejected→400 + content-indeterminate verdict), `/installs` (tomllib picker). API-layer `get_or_compute_hash` memoizes the drill-in hash keyed on the RESOLVED PAYLOAD `.tre` via a `replace(node, abspath=payload_tre)` synthetic SearchNode (P3 — sqlite stays OUT of diff.py; mutated payload .tre under an unchanged .toc → FRESH hash). `__main__` binds `127.0.0.1` ONLY (T-29-03-03). `config.py` tomllib `load_installs` (installs.toml gitignored, .example tracked, D-02). RED→GREEN; 12 api tests + env-gated real-pair integration test asserting a MEASURED second-compare cache HIT via an iter_node_entries spy (SC#4/P5); 72 passed, 2 deselected. Two Rule-3 deviations: cache get/put_file_hash take a SearchNode (payload-keyed via swapped abspath, no cache API change); removed a literal `0.0.0.0` from __main__ prose to satisfy the acceptance grep. create_app + diff importable for Phase 30 with zero backend refactor.
Outcome (30-01): FIRST frontend in the repo — `tools/tre-compare/web/` Vite 8 + React 19 + TS 6 + Tailwind 4 + shadcn (zinc base, dark default), sibling of `src/`, ZERO coupling to the engine MSBuild graph (D-01 extractability preserved). `build.outDir=build` dodges the gitignored Python `dist/` collision; `web/build/` is built-on-demand + gitignored (D-02/A3). vite.config: react+tailwindcss plugins, `@` alias, dev proxy (`/compare` `/file` `/installs` -> 127.0.0.1:8765 — relative-fetch dev+prod single code path), Vitest test block (jsdom+globals). 8 shadcn primitives (button badge table sheet input scroll-area separator tooltip) + @tanstack/react-virtual + @tanstack/react-query pre-installed for Wave 2. The ONLY backend touch of the whole phase: `web_static.SpaStaticFiles` (404->index.html fallback) + `WEB_DIR` (3-level .parent walk mirroring config.py) mounted at `/` LAST in `create_app` (greedy `/` never shadows the 4 routes — T-30-01-03), guarded on `web_dir.is_dir()`; new `create_app(web_dir=...)` param makes it testable. TDD RED→GREEN; 6 web_static tests + 80 passed / 2 deselected (no regression to the 72 prior). Verified end-to-end vs the REAL built bundle (GET / -> 200 text/html "TRE Compare"). __main__ 127.0.0.1 bind untouched. Three Rule-3 deviations: shadcn CLI 'Nova' preset drift → rewrote components.json+index.css to the locked zinc/dark contract; TS 6 deprecated baseUrl removed (@/* still resolves); root .gitignore global package.json/lib/ ignores negated for the web project. App.tsx + placeholder.test.ts are intentional Wave-2 stubs (real layout/tests = plan 30-03).
Next: Phase 30 Plan 02 (data layer: lib/api.ts typed client + lib/tree.ts buildFolderTree/flattenVisible), then 30-03 (UI: pickers, set-delta strip, virtualized tree, detail Sheet). Wave 2 builds on this green Vitest seam + the 4 frozen routes (consumed unchanged).
Prior-phase backlog (28 foundation):
Outcome (28-01): isolated `tools/tre-compare/` uv library — `uv init --lib` src layout, ZERO runtime deps (D-01), pytest 9.1.0 dev dep, committed `uv.lock` re-resolved under the 3.11 floor (`.python-version`=3.11, `requires-python>=3.11`), `[build-system].requires=uv_build>=0.11.7,<0.12` (no forward-pin, `uv build` exit 0 — review #11), registered `integration` marker (D-07 infra), package-local `.gitignore`, empty `parser/` subpackage (Plan 02 placeholder), pytest test root green (`uv run pytest -m "not integration"` → 1 passed, no marker warnings). TRE-01 ticked.
Outcome (28-02): vendored `tre_reader.py` + `tre_decrypt.py` from swg-blender-plugin (commit `f803f587…`) into `src/tre_compare/parser/` per D-03 — provenance headers + the single import rewrite (`swg_pipeline.tre_decrypt` → `.tre_decrypt`); ZERO swg_pipeline/engine imports (D-01 extractable); public API re-exported from `parser/__init__.py`; stdlib-only; all five TREE variants (0004/0005/6000/0006/5000) + COT2000 + SearchTOC recognized; every entry dataclass exposes snake_case `length` + `compressed_length` (Phase-29 changed-detection contract); smoke test `tests/test_parser.py` green (7 passed). SC#1 delivered.
Outcome (28-03): `scanner.py` hand-parses `[SharedFile]` repeated/indexed keys (NOT configparser; both `_NN_` and bare-priority grammars; cfg path a parameter, D-08) into an engine-faithful `(-priority, KIND_RANK[kind], cfg_seq)`-ordered `SearchNode` list (path<tree<toc within priority — review #1). `virtual_tree.py` ports `fix_up_file_name` VERBATIM (leading-`..` only) + a SEPARATE `safe_virtual_key` hardening wrapper (rejects interior-`..`/drive/UNC/empty — T-28-03-01), and merges first-hit-wins on canonical path in a SINGLE descending pass (guard BEFORE the tree-length-0 branch; no `claimed.pop` — review #1) with PER-NODE-TYPE tombstone (tree length-0 = global remove; toc length-0/offset-0 = skip-only, never shadows), `shadowed`=REAL-copies-only (review #4), eager deterministic `searchPath` `os.walk` (reparse-dir prune before descent; Open-Q1 RESOLVED), `.tre` AND `.toc` header bounds preflight + count×stride cap (T-28-03-04), and observable `node_errors`/`rejected` diagnostics (review #8). All behaviors smoke-verified; behavioral suite gated by Plan 04. Commits d0a784c16 + 3a1d3df83.
Next: Phase 28 Plan 04 — synthetic byte-built TRE/TOC/COT2000 fixtures + the behavioral test suite (test_scanner.py + test_virtual_tree.py incl. the inverse lower-priority-tombstone + same-priority cross-kind + oversized-header `.toc` + searchPath override/missing/empty-dir tests) + the env-gated D-07 integration test against real `stage/client.cfg`.

### Prior — Phase 26 (instrumentation removal / Options FATAL) — DONE

Plans: 2 (26-01 D-15 removal / HARD-03 partial — DONE commit 6c95fa990; 26-02 Options FATAL / HARD-04 — DONE, audit confirmed already-fixed in 5fce7bb83, no code change). Docs commit afd2a65bf. D-15 DpvsProfileInstrumentation fully removed (grep-zero), CORNERSNAP probes preserved (door-snap harness, deferred to x64/HARD-05). Release "not starting" was a PRE-EXISTING `stage/client.cfg` UTF-8 BOM — see [[reference_client_cfg_bom_release_crash]].

### Prior — Phase 25 (cantina-corner-snap-fix) — INTERIOR snap RESOLVED-by-config; residual DOOR snap PARKED for HARD-05

Plan: 1 of 1 (25-01 guard approach ABANDONED + reverted)
Status: Ready to execute
  • The frame-scoped reversal guard (7820aea50) was REVERTED (a6df32348) — runtime FALSIFIED the cell-ping-pong premise (it desynced cell membership → interior→skybox). A follow-up CollisionResolve resetPos fix was also built + REVERTED (collision-independent; didn't fix it).
  • INTERIOR corner snap (the original HARD-02 bug) is RESOLVED on every shippable config: Release D3D11 AND Release gl07 both render cantina interiors clean. Debug gl05 amplifies it (slow timestep).
  • Residual MAIN-DOOR snap (front+back, ~85%) is a SEPARATE, collision-independent, 32-bit-build/codegen-fragile one-frame float transient at the cell→world handoff (interior floor world-Y~5.1 → terrain~1.06). Our door-exit source is BYTE-IDENTICAL to pristine SWG-Source (D:\Code\client-tools); the Koogie cherry-picks never touched it. SWGEmu + Restoration run the SAME D3D9/gl07 renderer with NO snap — Restoration runs it in **x64**. Leading cause (Kenny): "D3D9 32-bit vs D3D9 64-bit" timing/codegen (x87/SSE-mix fragility vs deterministic 64-bit SSE). _fpreset MXCSR test moved it only ~10% → HARD-05 (D3DCompile) unlikely to fully fix the door.
  • Debug session PARKED. Evidence: stage/cornersnap-capture/EVIDENCE-*.log; .planning/research/CONSULT-43-*.out.
Next: baseline the DOOR snap from the HARD-05 (Phase 27 D3DCompile) build; real resolution likely ship-D3D11 (clean for us now) or a future 64-bit conversion (matches Restoration; also fixes the chronic 32-bit OOM crash). cfgs restored to rasterMajor=11; working tree clean.
Last activity: 2026-06-15

## v2.3 Milestone-Close Actions (DO AT CLOSE)

- [ ] **Promote koogie → master (fast-forward) and retire/rename the branch.** `master` is the stale Jan-2025 SWG-Source baseline; `koogie-msvc-cpp20-base` is the real trunk (732+ ahead, 0 behind → pure FF, zero conflicts). Decided 2026-06-13: keep committing trunk-style on koogie through v2.3, then FF master to it at close (same pattern as the v2.2 tag). **Push to `origin` ONLY, never the `koogie` (KoogieKepler) upstream.**

## Accumulated Context

### Roadmap Evolution

- [2026-06-12] **v2.3 Hardening ROADMAP CREATED** — 7 phases (24–30), 12/12 requirements mapped (HARD-01..05, PORT-01/02, TRE-01..05). Standard granularity. Phases continue from 23 → start at 24. Two independent streams: client hardening (24–27) and the new TRE compare tool (28–30). Consolidated HARD-03 (instrumentation removal) + HARD-04 (Options FATAL) into one cleanup phase (26); PORT-01/02 bundled with HARD-01 (DPVS config-gate) into the low-risk first phase (24).
- [2026-05-27] **v2.2 Visual Parity ROADMAP CREATED** — 7 phases (17–23), 13/13 requirements mapped (CHAR/WORLD/GAMMA/UI/FX/GEO/DPVS). Standard granularity. Phases continue from 16 → start at 17.
- Phase 16 added: v2.1 tech-debt cleanup (SwgGodClient 989crypt.lib + P12-P15 residue) — from milestone audit

### Decisions

**v2.3 Hardening (roadmap):**

- [2026-06-12] **Two independent streams interleave.** Client hardening (Phases 24–27) and the TRE compare tool (Phases 28–30) share zero code. The TRE tool's first real use case is the parked SWGSource-vs-whitengold space-asset diff, so it can run ahead of or concurrent with the hardening stream if that diagnosis becomes urgent.
- [2026-06-12] **Sequencing constraint (HARD-03 after HARD-02):** the CORNERSNAP `_DEBUG` instrumentation (`a9b419daf`) is the *acceptance harness* for the corner-snap fix — Phase 26 strips it only after Phase 25 verifies the fix. Likewise D-15 DPVS instrumentation removal waits until Phase 24 ships the verdict that supersedes its profiling purpose.
- [2026-06-12] **HARD-05 (D3DCompile port) census-first.** `D3DCompile` is HLSL-only; the first task is an asm-shader census (grep `D3DXAssembleShader` + `.vsh` references) to scope the assembly-path handling so the port does not silently null the VS and skip draws. The Fix-A SEH guard (`db83b0f5c`) stays as a safety net until the asm path is also off D3DX.
- [2026-06-12] **TRE tool = thin Python/FastAPI backend reusing the existing parser, Vite+React+shadcn SPA, localhost-only, at `tools/tre-compare/`.** Vendor `tre_reader.py` + `tre_decrypt.py` from `D:/Code/swg-blender-plugin/swg_pipeline/` (the stale memory pointer to `D:/Code/swg-tools` is wrong). Hard dependency chain: parser → scanner → vtree → diff → API → UI (Phases 28→29→30).
- [2026-06-12] **TRE correctness keystones:** (a) `vtree.py` mirrors `TreeFile::install`/`find` exactly — sort by (priority DESC, parseOrder ASC), first-hit-wins, length-0 = tombstone, `fixUpFileName` canonicalization — unit-tested against real `stage/client.cfg` before any UI; (b) the TOC `crc` field is a **path-CRC, not a content hash** — changed-file detection uses `(length, compressedLength)` first + on-demand real hash (xxhash), never eager full-archive hashing; (c) hand-parse `[SharedFile]` (stdlib configparser mangles repeated keys); (d) higher priority number is searched first.
- [2026-06-12] **Architecture-forward, read-only v2.3:** parser abstraction in `backend/tre/`, isolated `vtree.py`, clean API shape, in-repo location are chosen so future write/extract/repack (TREM-01..03) need no structural rewrite. Any v2.3 choice that forecloses write capabilities is a known anti-pattern.
- [2026-06-12] **Core invariant for every client-touching phase (24–27):** client stays bootable to character select under BOTH `rasterMajor=5` and `rasterMajor=11`. Debug exe reads `client_d.cfg` (not `client.cfg`) — set rasterMajor there for each smoke.

**v2.2 Visual Parity (roadmap):**

- [2026-05-27] **Char-select beachhead FIRST (Phase 17).** Prove the asset-PS pipeline on the deterministic, isolated character-select screen (textures + eyes + head) before any open-world work.
- [2026-05-27] **Asset-PS approach:** primary lane = recompile discarded `TAG_PSRC` `//hlsl`; secondary = asm→HLSL→`ps_4_0`; tertiary/narrow = FFP `TextureOperation` generator. Per-pass constants reflection-driven (D3DReflect), NOT copied D3D9 register indices.
- [2026-05-27] **DPVS D3D11 remeasure (Phase 23) STRICTLY LAST** — meaningless until geometry renders cleanly.
- [2026-05-27] **Validation = D3D9-vs-D3D11 screenshot diff** against `docs/research/phase12-baseline/COMPARISON.md` matched pairs. Success = "matches `rasterMajor=5`", NOT "no magenta".

**v2.1 Decruft (roadmap):**

- [2026-05-25] v2.1 framed as **cleanup-only**; phase ordering risk-gradient, low-first (pure deletes → lib unlink → live-source surgery).
- [2026-06-12] Phase 23-03: D3D11 DPVS verdicts FLIP vs Phase 10 D3D9 — outdoor remove→keep, indoor keep→remove. Option α REVISED; shipped #else branch untouched. `docs/recon/23-dpvs-d3d11-profiling.md`. (This verdict is the spec for v2.3 HARD-01.)
- [Phase ?]: [2026-06-14] Phase 28-01: tre-compare uv scaffold pinned to the 3.11 floor; uv.lock re-resolved under 3.11 for clone/CI portability; requires-python >=3.11 keeps 3.12-3.14 working
- [Phase ?]: [2026-06-14] Phase 28-01: kept [build-system].requires at uv_build>=0.11.7,<0.12 (no forward-pin) and proved it resolves via uv build exit 0 (review finding #11)
- [Phase ?]: [2026-06-14] Phase 28-02: vendored tre_reader.py + tre_decrypt.py from swg-blender-plugin (commit f803f587) per D-03 — provenance headers + the single import rewrite (swg_pipeline.tre_decrypt -> .tre_decrypt); zero swg_pipeline/engine imports (D-01 extractability)
- [Phase ?]: [2026-06-14] Phase 28-02: all three entry dataclasses already expose snake_case length + compressed_length verbatim — no wrapper; Phase-29 changed-detection contract asserted via dataclasses.fields()
- [Phase ?]: Phase 28-03: scanner.py engine-faithful sort (-priority, KIND_RANK, cfg_seq) + both key grammars; virtual_tree.py first-hit-wins single-pass, per-node-type tombstone (tree=global, toc=skip), fix_up_file_name verbatim + separate safe_virtual_key hardening; eager searchPath walk (Open-Q1); .tre/.toc bounds preflight
- [Phase ?]: [2026-06-15] Phase 29-01: tre_compare.diff pure engine (no fastapi/sqlite3) — (basename,kind) set keying, (len,clen) file tri-state never crc, tombstoned/rejected/error qualifier, symmetric TREE/TOC drill-in, never-raise xxhash; bare uvicorn proven
- [Phase ?]: [2026-06-15] Phase 29-02: tre_compare.cache sqlite parse-skip cache keyed (abspath, mtime_ns INTEGER, size) — HIT proven by an iter_node_entries spy (call-count==1) not row-equality (P5); build_virtual_tree_cached parity with the builder across an expanded 7-case matrix incl. shadowed-tuple ORDER, kind threaded from the live node (P2); INSERT OR IGNORE + under-lock MISS re-check (concurrent-MISS-safe, Sonnet); busy_timeout + __file__-relative default db path; tre_file stored per toc row (Pitfall 7)
- [Phase ?]: [2026-06-15] Phase 29-03: tre_compare.api create_app(cache,installs_path) FACTORY — four stateless routes, NO module-level Cache leak (D-01/D-03); /compare/files full {rows,summary} no pagination (D-08); /file/detail not_found->404 rejected->400 + content-indeterminate verdict; get_or_compute_hash API-layer memo keyed on the RESOLVED payload .tre via replace(node,abspath=payload_tre) (P3, sqlite stays out of diff.py); /installs tomllib picker (installs.toml gitignored, .example tracked); __main__ binds 127.0.0.1 only (T-29-03-03); env-gated real-pair integration test asserts a measured second-compare cache HIT via an iter_node_entries spy (SC#4/P5). RED baf2e18b9 -> GREEN b0d5442b4; 72 passed, 2 deselected
- [Phase ?]: [2026-06-15] Phase 30-01: FIRST repo frontend at tools/tre-compare/web (Vite 8/React 19/TS 6/Tailwind 4/shadcn zinc-dark), sibling of src/, zero coupling to the engine MSBuild graph (D-01); build.outDir=build dodges the gitignored Python dist/ collision, web/build/ built-on-demand+gitignored (D-02/A3); vite dev proxy + relative-fetch single code path; Vitest+RTL+jsdom seam wired; 8 shadcn primitives + @tanstack/react-virtual+react-query pre-installed for Wave 2. ONLY backend touch: web_static.SpaStaticFiles 404->index.html + WEB_DIR (3-level .parent walk) mounted at / LAST in create_app (greedy / never shadows the 4 routes, T-30-01-03), guarded on web_dir.is_dir(); create_app(web_dir=) injectable. TDD RED 626b9a073 -> GREEN dd4e1da2b; 6 web_static tests + 80 passed/2 deselected (no regression); verified vs the REAL built bundle. 127.0.0.1 bind untouched. 3 Rule-3 deviations (shadcn Nova preset->zinc contract, TS6 baseUrl removed, root .gitignore package.json/lib/ negated for web/)

### Pending Todos

7 pending (5 now folded into v2.3 phases):

- [Config-gate DPVS occlusion per Phase 23 verdict](todos/pending/2026-06-12-config-gate-dpvs-occlusion-per-phase-23-verdict.md) — **→ v2.3 HARD-01 (Phase 24)**
- [Cantina corner-snap engine improvement](todos/pending/2026-05-15-cantina-corner-snap-engine-improvement.md) — **→ v2.3 HARD-02 (Phase 25)**; mechanism FOUND 2026-06-12 (same-frame portal ping-pong, re-entrant positionChanged); CORNERSNAP instrumentation committed
- [Options toolbar cooldown UI data mismatch crash](todos/pending/2026-05-27-options-toolbar-cooldown-ui-data-mismatch-crash.md) — **→ v2.3 HARD-04 (Phase 26)**
- [Port D3D9 shader compile to D3DCompile](todos/pending/2026-05-31-port-d3d9-shader-compile-to-d3dcompile.md) — **→ v2.3 HARD-05 (Phase 27)**
- [SWGSource vs whitengold TRE asset diff](todos/pending/2026-05-15-swgsource-vs-whitengold-tre-asset-diff.md) — **→ first use case of the v2.3 TRE compare tool (Phases 28–30)**
- [Sync community compat fixes from SWG-Source/client-tools master](todos/pending/2026-05-08-sync-swg-source-community-compat.md) — future milestone (SYNC-01, not v2.3)

### Blockers/Concerns

- **[v2.3 — TRE precedence inversion]** The merged-virtual-tree resolution is the single biggest correctness risk: higher priority number = searched first, first-hit-wins, length-0 = tombstone. Easy to invert. `vtree.py` MUST be unit-tested against real `stage/client.cfg` (verify `searchPath_00_10` beats `searchTOC_00_0`) before any UI code exists.
- **[v2.3 — TOC crc misuse]** The per-entry `crc` is `Crc::calculate(fileName)` — a CRC of the path string, not file content. Two archives can share the same path-CRC for the same filename while the bytes differ (that's the whole point of override TREs). Use `(length, compressedLength)` as the cheap changed signal + real hash on drill-in.
- **[v2.3 — D3DCompile asm gap]** Census the asm (`.vsh` / `D3DXAssembleShader`) count BEFORE starting HARD-05. `D3DCompile` compiles HLSL only; silently dropping the asm path nulls the VS → skipped draws. Keep `D3DXAssemble` + SEH guard until the asm path is explicitly handled.
- **[v2.3 — CORNERSNAP removal sequencing]** Strip the CORNERSNAP `_DEBUG` probes (Phase 26 / HARD-03) ONLY after the corner-snap fix (Phase 25 / HARD-02) is verified against them — they are its acceptance harness.
- **[v2.3 — machine portability]** `stage/miles/` redist is NOT in git and postbuild doesn't copy it — a fresh clone has half-dead audio + warning-flood lag if missing. PORT-01 must detect/handle this, not assume the dir is present.
- **[boot invariant — /FORCE false-pass]** SwgClient links under `/FORCE` which downgrades unresolved externals to WARNINGS and still emits a binary with exit 0. Grep link output for `unresolved external symbol` (must be 0) — applies to the atomic instrumentation removal (Phase 26).

## Deferred Items

Items carried from v1 close:

| Category | Item | Status | Deferred At |
|----------|------|--------|-------------|
| Build warnings | Nested CMake minimum-version deprecation warnings | Deferred (historical CMake tree) | Phase 1 |
| Build warnings | `crypto` C4530 exception-unwind warnings | Deferred | Phase 1 |
| Runtime | 156k DEBUG_WARNINGs at exit (mostly per-frame nebula lightning) | Deferred | v1 post-launch |
| Runtime | 8,152 memory leaks at exit (singletons/caches, expected) | Deferred | v1 post-launch |

## Session Continuity

Last session: 2026-06-15T15:10:56.673Z
Resume (2026-06-12): **v2.3 Hardening ROADMAP CREATED** (Phases 24–30; 12/12 requirements mapped 100%). v2.2 Visual Parity shipped + tagged `v2.2`. Repo: swg-client-v2 (MSBuild/Koogie) is the single source of truth.

**v2.3 Hardening — the plan (7 phases, two independent streams):**

Client-hardening stream (boot-invariant on both renderers):

1. **Phase 24** (HARD-01, PORT-01, PORT-02) — DPVS occlusion config-gate (auto = POB-cell-keyed, outdoor on / indoor off, per Phase 23 verdict) + machine portability (de-hardcode `stage/override` + `stage/miles`, clean `client_d.cfg`). Least-risky first; establishes dual-renderer boot confidence.
2. **Phase 25** (HARD-02) — cantina corner-snap re-entrancy guard (suppress A→B→A same-frame portal ping-pong; preserve fast door traversals). Verified via the committed CORNERSNAP `_DEBUG` instrumentation. MUST precede Phase 26.
3. **Phase 26** (HARD-03, HARD-04) — strip D-15 DPVS + CORNERSNAP instrumentation atomically (grep link for 0 unresolved) + fix the Options-window FATAL (`checkShowToolbarCommandCooldownTimer`, commit `d1b3c0eaf`).
4. **Phase 27** (HARD-05) — port `D3DXCompileShader`→`D3DCompile` (Fix B). Asm-shader census FIRST. Most complex item; SEH guard retained until asm path off D3DX.

TRE compare tool stream (standalone web app, hard chain, can interleave):

5. **Phase 28** (TRE-01) — headless foundation: scaffold `tools/tre-compare/`, vendor `tre_reader.py`/`tre_decrypt.py`, `scanner.py` ([SharedFile] hand-parse), `vtree.py` (first-hit-wins + tombstones + fixUpFileName) unit-tested vs real `stage/client.cfg`.
6. **Phase 29** (TRE-02/03/04) — `diff.py` set + file-level diff ((length,compressedLength) signal, on-demand xxhash), `cache.py` sqlite index, FastAPI routes (summary + paginated rows + lazy file-detail).
7. **Phase 30** (TRE-05) — React+Vite+shadcn SPA: install picker, set-delta table, virtualized (TanStack Virtual) filterable tree-diff, per-file detail. UI hint.

**Next action:** `/gsd-plan-phase 24` — plan the DPVS config-gate + machine portability. (Or `/gsd-plan-phase 28` to start the TRE tool stream in parallel.)

Known unrelated long-tail (deferred): 0x087a armor_marauder async crash (cross-client, retry works); blend-factor scene-sweep risk; stale comment at Direct3d11_StaticShaderData.cpp:1655; Nyquist VALIDATION backfill (VAL-01, phases 18/19–22 — milestone audit stands as the record).
