---
gsd_state_version: 1.0
milestone: v3.0
milestone_name: x64 Port
status: executing
last_updated: "2026-06-16T00:52:00.000Z"
last_activity: 2026-06-16
progress:
  total_phases: 6
  completed_phases: 0
  total_plans: 6
  completed_plans: 5
  percent: 83
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-06-15 after v2.3 close)

**Core value:** Every change must leave the client bootable to character select. Visual parity achieved in v2.2 — D3D11 now matches the D3D9 baseline; never regress either renderer.
**Current focus:** Phase 31 — 64-bit-correctness-foundation

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

## Deferred Items (acknowledged at v2.3 close)

8 open artifacts acknowledged at v2.3 Hardening milestone close (2026-06-15). None are v2.3 regressions; the headline three are root-caused 32-bit-bound carry-forwards to the **x64 Port** milestone:

| Category | Item | Status |
|----------|------|--------|
| debug | cantina-corner-snap | parked — root cause found (32-bit codegen float transient, CONSULT-43); **→ x64 (HARD-02)** |
| debug | safecast-null-cast | closed (resolved 2026-05-15) |
| todo | 2026-05-15-cantina-corner-snap-engine-improvement | low — **→ x64 (HARD-02)**; cell-ping-pong premise falsified, guard reverted |
| todo | 2026-05-31-port-d3d9-shader-compile-to-d3dcompile | low — satisfied-by-Fix-A for v2.3; **clean port → x64 (HARD-05)** |
| todo | 2026-06-13-64bit-x64-port | **medium-high value, large effort — the next milestone**; absorbs HARD-02/HARD-05/HARD-03-CORNERSNAP |
| todo | 2026-05-15-swgsource-vs-whitengold-tre-asset-diff | low — informational; now unblocked by the shipped TRE compare tool |
| todo | 2026-06-13-test-jtl-space-rendering-post-v2.2 | low — informational; gauges JTL revivability (future) |
| uat | Phase 24 `24-HUMAN-UAT.md` | `passed` / 0 pending scenarios (no action — surfaces only because the file exists) |

Plus the v2.3 audit `tech_debt` (see `milestones/v2.3-MILESTONE-AUDIT.md`): HARD-03 CORNERSNAP `_DEBUG` probes intentionally KEPT (x64 harness); backend `/compare/files` bare-500 on a missing searchTree archive + relative-searchTree paths not anchored to the cfg dir (WARNING — workaround = absolute-path verify-*.cfg); `api.py` couples to private `diff.py` symbols; code-review WR-01..04 / IN-01..03 (buildFolderTree bare-segment key collision, tree-row keyboard nav, SPA 404→index masking, silent re-Compare no-op); `29-*-SUMMARY.md` frontmatter omits `requirements-completed`.

## Current Position

Phase: 31 (64-bit-correctness-foundation) — EXECUTING
Plan: 6 of 6
Status: Ready to execute (31-05 BITS-03 complete; only the phase-gate plan 06 remains)
Last activity: 2026-06-16

### v3.0 x64 Port — the plan (6 phases, strict numeric order, dependency-chained)

Core invariant (extended): every client-touching phase must reach boot-to-character-select parity in the **x64** build under BOTH `rasterMajor=5` (D3D9) and `=11` (D3D11), and must never regress either renderer in the 32-bit build. Debug exe reads `client_d.cfg`. Reference: SWG Restoration's stable x64 D3D9 client at `D:\SWG Restoration\x64`; Miles 9.3b SDK cloned at `D:/Code/milesss-v9.3b/`.

1. **Phase 31** (BITS-01/02/03) — 64-bit Correctness Foundation: x87 `__asm fnstcw/fldcw` → `_controlfp`/`_control87` + tree-wide `__asm` sweep; pointer/int truncation (C4311/C4312/C4244 as errors); struct-packing/hardcoded-sizeof/serialization-width audit (IFF/TRE + network). Source work, no x64 build yet — front-loaded enabling correctness.
2. **Phase 32** (SHADER-01) — D3DX → `d3dcompiler_47`: the v2.3-deferred HARD-05, now a true prerequisite (D3DX is x64-hostile). Asm-shader census FIRST; Fix-A SEH guard retained until the asm path is also off D3DX.
3. **Phase 33** (X64-01/04/02) — x64 Build Platform + D3D9 Renderers: add `x64` to swg.sln + every .vcxproj, resolve all third-party x64 libs (dpvs/bink/pcre/libxml2/icu/jpeg/zlib/discord-rpc — Restoration's `x64/` is the availability map), gl05/06/07 boot under rasterMajor=5/6/7. FIRST linking x64 client.
4. **Phase 34** (X64-03) — x64 D3D11 Renderer: gl11 as x64, boots under rasterMajor=11 at the v2.2 parity bar.
5. **Phase 35** (AUDIO-01/02) — Miles 9.3b Audio Port: the one third-party needing real code work. Port `clientAudio` call sites 7.2e→9.x (only signature edit `AIL_room_type(dig, 0)` at Audio.cpp:3852), vendor SDK to `src/external/3rd/library/miles-9.3b/`, stage `mss64.dll` + `.asi`/`.flt` provider set.
6. **Phase 36** (VERIFY-01/02/03) — Verification & CORNERSNAP Cleanup: confirm door-snap clean (vs the kept CORNERSNAP probes + Restoration x64 reference) + OOM-crash class eliminated (extended session, no `MemoryManager` `b0780503` FATAL), THEN strip the CORNERSNAP `_DEBUG` probes (link-grep 0 unresolved) — completing the deferred half of HARD-03.

**Next action:** `/gsd-plan-phase 31` — plan the 64-bit correctness foundation.

### Prior — Phase 26 (instrumentation removal / Options FATAL) — DONE

Plans: 2 (26-01 D-15 removal / HARD-03 partial — DONE commit 6c95fa990; 26-02 Options FATAL / HARD-04 — DONE, audit confirmed already-fixed in 5fce7bb83, no code change). Docs commit afd2a65bf. D-15 DpvsProfileInstrumentation fully removed (grep-zero), CORNERSNAP probes preserved (door-snap harness, deferred to x64/HARD-05). Release "not starting" was a PRE-EXISTING `stage/client.cfg` UTF-8 BOM — see [[reference_client_cfg_bom_release_crash]].

### Prior — Phase 25 (cantina-corner-snap-fix) — INTERIOR snap RESOLVED-by-config; residual DOOR snap PARKED for HARD-05

Plan: 1 of 1 (25-01 guard approach ABANDONED + reverted)
Status: Milestone complete
  • The frame-scoped reversal guard (7820aea50) was REVERTED (a6df32348) — runtime FALSIFIED the cell-ping-pong premise (it desynced cell membership → interior→skybox). A follow-up CollisionResolve resetPos fix was also built + REVERTED (collision-independent; didn't fix it).
  • INTERIOR corner snap (the original HARD-02 bug) is RESOLVED on every shippable config: Release D3D11 AND Release gl07 both render cantina interiors clean. Debug gl05 amplifies it (slow timestep).
  • Residual MAIN-DOOR snap (front+back, ~85%) is a SEPARATE, collision-independent, 32-bit-build/codegen-fragile one-frame float transient at the cell→world handoff (interior floor world-Y~5.1 → terrain~1.06). Our door-exit source is BYTE-IDENTICAL to pristine SWG-Source (D:\Code\client-tools); the Koogie cherry-picks never touched it. SWGEmu + Restoration run the SAME D3D9/gl07 renderer with NO snap — Restoration runs it in **x64**. Leading cause (Kenny): "D3D9 32-bit vs D3D9 64-bit" timing/codegen (x87/SSE-mix fragility vs deterministic 64-bit SSE). _fpreset MXCSR test moved it only ~10% → HARD-05 (D3DCompile) unlikely to fully fix the door.
  • Debug session PARKED. Evidence: stage/cornersnap-capture/EVIDENCE-*.log; .planning/research/CONSULT-43-*.out.
Next: baseline the DOOR snap from the HARD-05 (Phase 27 D3DCompile) build; real resolution likely ship-D3D11 (clean for us now) or a future 64-bit conversion (matches Restoration; also fixes the chronic 32-bit OOM crash). cfgs restored to rasterMajor=11; working tree clean.
Last activity: 2026-06-15

## v2.3 Milestone-Close Actions

- [x] **Promote koogie → master (fast-forward) and retire the branch.** DONE 2026-06-14 — `koogie-msvc-cpp20-base` was FF-merged into `master` and retired; the repo is now trunk-based directly on `master` (the v2.3 work and this milestone close all landed on `master`). **Push to `origin` ONLY, never the `koogie` (KoogieKepler) upstream.**
- [ ] **Tag `v2.3` + push (origin only).** Tag created at close; push pending user confirmation.

## Accumulated Context

### Roadmap Evolution

- [2026-06-15] **v3.0 x64 Port ROADMAP CREATED** — 6 phases (31–36), 13/13 requirements mapped 100% (X64-01..04, BITS-01..03, AUDIO-01/02, SHADER-01, VERIFY-01..03). Standard granularity. Phases continue from 30 → start at 31. Dependency-chained, strict numeric order: correctness foundation (31) + D3DX removal (32) front-load as prerequisites for the first x64 link (33); D3D9 (33) before D3D11 (34); audio (35) after a linking x64 client; verify + CORNERSNAP cleanup (36) last (VERIFY-03 strips the door-snap acceptance harness only after VERIFY-01 confirms it clean).
- [2026-06-12] **v2.3 Hardening ROADMAP CREATED** — 7 phases (24–30), 12/12 requirements mapped (HARD-01..05, PORT-01/02, TRE-01..05). Standard granularity. Phases continue from 23 → start at 24. Two independent streams: client hardening (24–27) and the new TRE compare tool (28–30). Consolidated HARD-03 (instrumentation removal) + HARD-04 (Options FATAL) into one cleanup phase (26); PORT-01/02 bundled with HARD-01 (DPVS config-gate) into the low-risk first phase (24).
- [2026-05-27] **v2.2 Visual Parity ROADMAP CREATED** — 7 phases (17–23), 13/13 requirements mapped (CHAR/WORLD/GAMMA/UI/FX/GEO/DPVS). Standard granularity. Phases continue from 16 → start at 17.
- Phase 16 added: v2.1 tech-debt cleanup (SwgGodClient 989crypt.lib + P12-P15 residue) — from milestone audit

### Decisions

**v3.0 x64 Port (roadmap):**

- [2026-06-15] **SHADER-01 (D3DX → d3dcompiler_47) sequenced BEFORE the first x64 link (Phase 32 before 33).** D3DX is x64-hostile, so the D3DCompile port (the v2.3-deferred HARD-05) becomes mandatory and must land before the x64 platform add can link cleanly — not after. Restoration did exactly this for their x64 build.
- [2026-06-15] **BITS-01/02/03 front-load as Phase 31 (the enabling correctness work).** x64 forbids inline asm (`__asm fnstcw/fldcw`) and surfaces pointer/int truncation + data-layout defects — fix the source x64-clean on the still-32-bit tree first so the platform add is a build/link exercise, not a debugging swamp.
- [2026-06-15] **Both renderers are separate, separately-verifiable x64 boot gates.** X64-02 (D3D9 gl05/06/07, Phase 33) and X64-03 (D3D11 gl11, Phase 34) are distinct requirements with distinct boot smokes; D3D9 lands first (it's the door-snap/OOM verification reference — Restoration runs x64 D3D9), D3D11 second.
- [2026-06-15] **AUDIO-01/02 is a self-contained chunk (Phase 35), gated on a linking x64 client (33).** Miles is the one third-party that needs real code work, not a relink: the engine is on Miles 7.2e and the only x64 Miles is 9.x (9.3b SDK cloned at `D:/Code/milesss-v9.3b/`). API is ~source-compatible (single signature edit `AIL_room_type(dig, 0)`); the real risk is reverb/room-type behavior + the .asi/.flt provider set staging (the half-dead-audio/warning-flood failure mode).
- [2026-06-15] **VERIFY-03 (strip CORNERSNAP probes) comes AFTER VERIFY-01 (door-snap clean), both in Phase 36.** The CORNERSNAP `_DEBUG` probes are the door-snap's acceptance harness (intentionally KEPT through v2.3); they can only be removed once the x64 build verifies the door-snap clean against them — completing the deferred half of HARD-03. VERIFY-01/02 can only be checked once the x64 build runs in-world (Phases 34 + 35 done).
- [2026-06-15] **Core invariant extended, not replaced.** Every client-touching phase must reach boot-to-character-select parity in the x64 build under both `rasterMajor=5` and `=11`, AND never regress either renderer in the 32-bit build (the shipped v2.2/v2.3 baseline). `/FORCE` link-grep (0 unresolved) applies to every x64 link gate.

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
- [Phase ?]: [2026-06-15] Phase 30-02: PURE Wave-2 data layer (lib/types.ts + tree.ts + status.ts + api.ts). Frozen-contract transcription — SetStatus/FileStatus TWO distinct unions (Pitfall 3), no path-checksum field (D-04 Option 1). tree.ts RESEARCH Pattern 1 (build-once/flatten-per-interaction) + empty-folder suppression via rollup (Pitfall 6; identical bucket counts identical-by-metadata + tombstoned). status.ts HONESTY DISTINCTION HARD RULE tested — solid green only for the post-xxhash content-confirmed-identical, metadata match neutral + contentVerified:false; two separate badge maps. api.ts relative-path client + ApiError {detail} envelope, no hardcoded host. TDD RED->GREEN; 23/23 Vitest, build exit 0. 9c32f5fd0/d52a6df78/3bb08b864/00e44fc04
- [Phase ?]: [2026-06-15] Phase 30-03: linked master-detail SPA over the 30-02 data layer (App shell + InstallPicker/SetDeltaStrip/SummaryStats/StatusBadge + virtualized FileTree + auto-verdict DetailPanel). StatusBadge = the single semantic-variant->shadcn-Badge mapper so the honesty distinction (≈ metadata neutral vs = content green) cannot drift across the 5 consumers; committed-pair pattern (picker selection != committed compare pair); keyed-query stale-discard on [left_cfg,right_cfg,path] (Pitfall 4); set-delta cross-filter is a best-effort path-stem scope (frozen /compare/files rows carry no per-file archive field). SC#4 human-verify APPROVED 2026-06-15 — plan's literal SWGSource × whitengold pair NOT realizable (whitengold absent); SWGLegends was a degenerate identical stub; resolved by registering 4 real open-zlib distributions (SWGSource/SWG Infinity/SWGEmu/Stardust) via gitignored absolute-path verify-*.cfg wrappers and accepting against SWGSource × SWG Infinity (231,086 file rows: changed 126,542 / added 80,653 / identical-by-metadata 19,954 / removed 3,232 / tombstoned 705) — virtualized tree no hang, filter/search/hide-identical/cross-filter + detail honesty distinction confirmed. TRE-05 satisfied; Phase 30 + all v2.3 phases complete. 3754b2a73 + 362e2e576
- [Phase ?]: [2026-06-15] Phase 30-01: FIRST repo frontend at tools/tre-compare/web (Vite 8/React 19/TS 6/Tailwind 4/shadcn zinc-dark), sibling of src/, zero coupling to the engine MSBuild graph (D-01); build.outDir=build dodges the gitignored Python dist/ collision, web/build/ built-on-demand+gitignored (D-02/A3); vite dev proxy + relative-fetch single code path; Vitest+RTL+jsdom seam wired; 8 shadcn primitives + @tanstack/react-virtual+react-query pre-installed for Wave 2. ONLY backend touch: web_static.SpaStaticFiles 404->index.html + WEB_DIR (3-level .parent walk) mounted at / LAST in create_app (greedy / never shadows the 4 routes, T-30-01-03), guarded on web_dir.is_dir(); create_app(web_dir=) injectable. TDD RED 626b9a073 -> GREEN dd4e1da2b; 6 web_static tests + 80 passed/2 deselected (no regression); verified vs the REAL built bundle. 127.0.0.1 bind untouched. 3 Rule-3 deviations (shadcn Nova preset->zinc contract, TS6 baseUrl removed, root .gitignore package.json/lib/ negated for web/)
- [Phase ?]: [2026-06-15] Phase 31-01: scratch x64 harness manifest exhaustive (2216 TUs/57 libs, ClCompile-derived); _USE_32BIT_TIME_T DROPPED on x64 (UCRT hard-errors under _WIN64 -> time_t is 8 bytes, a BITS-03 residual); Misc.h:236 memmove C2668 is the dominant x64 blocker gating the whole in-scope tree
- [Phase ?]: [2026-06-15] Phase 31-02: BITS-01 hard chunk DONE — FloatingPointUnit x87 fnstcw/fldcw -> _controlfp_s with x87<->_MCW_* boundary translation (strategy A: module stays x87-layout, translate only at get/set; update() compare preserved); P_24 RETAINED on 32-bit (named decision, VERIFY-01 door-snap), _MCW_PC omitted only on x64 (#if _M_X64; avoids invalid-param handler), never __control87_2; _DEBUG round-trip self-check. SseMath canDoSseMath cpuid->__cpuid + 4 *_l2p routines + prefetch -> _mm_*/_mm_prefetch register-faithful (verified lane semantics: rotateScale 3-lane/w=0 vs rotateTranslateScale 4-lane/w=1; skin position-4-lane vs normal-3-lane); _mm_loadu_ps unaligned (x64 movaps-fault avoided), global sseVariable retired; _DEBUG numeric oracle. Transform sse_xf_matrix_3x4 naked->normal _mm_* fn, shufps 0x15 = translate to LANE 3 only via _mm_set_ps (NOT _mm_set1_ps broadcast); _DEBUG equivalence oracle vs scalar. NO #ifdef _M_X64 fork for SSE (D-05). All 3 TUs 0 C4235 in scratch x64; sharedMath 32-bit ClCompile clean. Rule-1 fix: oracle *_l2p calls needed SseMath:: qualification (C3861 on 32-bit). Misc.h C2668 DEFERRED to 31-04 (deferred-items.md DEF-31-01). Commits e9edaeca8/673efdd28/6a1fd14b7/717a66689
- [Phase ?]: [2026-06-15] Phase 31-03: BITS-01 misc __asm sweep DONE — CollisionUtils x87 fld/fsqrt/fstp->sqrtf; Fatal/Clock __asm int 3->__debugbreak; ProfilerTimer naked rdtsc->static_cast<__int64>(__rdtsc()) (__int64 return KEPT so no caller break + no C4244); VeCritsec MSVC lock-bts spinlock->_interlockedbittestandset (C-style (long*) cast deliberately strips m_iLock volatile, intrinsic is a full barrier; GCC __asm__ branch byte-untouched); DebugHelp the phase's ONE justified #if defined(_M_X64) RtlCaptureContext fork with FULL 64-bit Rip/Rsp/Rbp .Offset (NO DWORD trunc, review #6), 32-bit asm path unchanged. All 6 TUs 0 C4235/C4311/C4312/C4244 in scratch x64. Rule-1 fix: reinterpret_cast couldn't drop volatile (C2440)->C-style cast. Two PHASE-33 RUNTIME residuals handed to plan 06: (1) x64 unwind WALK compile-clean-only, (2) uint32* callStack output still narrows Rip. DEF-31-01 Misc.h:236 memmove C2668 remains the only residual error (owned by plan 31-04). Commits a4f711419/379920283/5a8924b8c.
- [Phase ?]: [2026-06-16] Phase 31-04: BITS-02 truncation DONE — DEF-31-01 Misc.h memmove C2668 RESOLVED (::memmove + size_t; 359214d2b unblocks ~every in-scope TU). 7 pointer sort keys -> stable hash-to-int (int return kept, ShaderPrimitiveSorter byte-unchanged, NO ABI cascade; D-06 review #3). MemoryManager ptr-diff->ptrdiff_t + %p logging; Os ShellExecute->INT_PTR, menu HMENU->UINT_PTR, RaiseException->const ULONG_PTR*. All 10 owned TUs 0 C4311/C4312/C4244 x64-clean; no #pragma disable. RESIDUAL-31-04 classified for plan 06. Commits 359214d2b/02edecf2b/0fd4a74a8/9cf9c49ee
- [Phase ?]: [2026-06-16] Phase 31-05: BITS-03 serialization+layout DONE — Archive get/put(std::map) raw size_t numKeys -> uint32_t (binds the EXISTING 4-byte unsigned-int overload on both bitnesses; wire byte-identical to the shipped 32-bit server; NO 8-byte overload). Corrected narrative (review #5): raw size_t on x64 FAILS overload resolution = compile error, NOT a silent widen. put() overflow-guarded with assert (NOT DEBUG_FATAL — sharedFoundation sits ABOVE the external/ours archive layer; assert is the lib house guard). EXERCISED by a gitignored scratch archive-map-instantiation.cpp (zero std::map Archive instantiations exist under src/engine; boot smoke wouldn't prove it) — round-trips std::map<int,int>, compiles x64-clean. static_assert(sizeof==N)+offsetof on TargaHeader=18/TargaFooter=26 + 5 AssetCustomization packed structs (IntRange=8/VariableUsage=6/UsageIndexEntry=5/LinkIndexEntry=5/CrcLookupEntry=6), N baselined to the LIVE 32-bit cl (standalone probe exit 0, then full SwgClient Debug build 0 C2338, exe produced; int32/uint32=long stays 4 on LLP64). bits03 sweep (61 TUs) 0 C2664/C2665 in owned files; RESIDUAL-31-05 ByteStream.cpp:347 C4311 ptr-truncation -> plan 06; vector signed-int C4244 documented as D-07-excluded. SAFE paths byte-unchanged. Commits f9efe220c/650848aff/f9fefbc21

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
- [Phase 31] ~~Misc.h:236 memmove C2668~~ — RESOLVED 2026-06-16 by plan 31-04 (commit 359214d2b): `::memmove(dst, src, static_cast<size_t>(length))` binds the CRT overload, engine wrapper int-length signature unchanged. The dominant cross-plan x64 blocker is cleared — full in-scope TUs (e.g. StaticShader.obj) now compile exit 0 in the scratch harness.
- [Phase 31 — RESIDUAL-31-04 -> plan 06] 7 NON-owned BITS-02 truncation sites recorded + classified by 31-04 Task 3 (deferred-items.md): must-fix-in-Phase-31 (RenderWorld.cpp:1127, Direct3d9.cpp:137/183/185/203 (DWORD)(uintptr_t) log casts that evade /we4311, EditableAnimationState.cpp, CuiMediator.cpp, LeakFinder.cpp, WinMain.cpp ShellExecute) vs deferrable (WaterTestAppearance test code). Plan 06 (phase gate) owns the fixes.
- [Phase 31 — RESIDUAL-31-05 -> plan 06] 1 NON-owned BITS-02 ptr-truncation surfaced by 31-05's bits03 backstop sweep (deferred-items.md): ByteStream.cpp:347 reinterpret_cast<unsigned int>(Data*) C4311 in the freed-memory-poison sentinel assert (archive lib, in-scope boot path, NOT in 31-05 files_modified). Class (a) MUST-FIX; fix = compare full-width uintptr_t vs a uintptr_t sentinel. The vector signed-int C4244 (Archive.h:348/360/370) is the D-07 EXCLUSION (NOT a defect) — do not "fix" it.

## Deferred Items

Items carried from v1 close:

| Category | Item | Status | Deferred At |
|----------|------|--------|-------------|
| Build warnings | Nested CMake minimum-version deprecation warnings | Deferred (historical CMake tree) | Phase 1 |
| Build warnings | `crypto` C4530 exception-unwind warnings | Deferred | Phase 1 |
| Runtime | 156k DEBUG_WARNINGs at exit (mostly per-frame nebula lightning) | Deferred | v1 post-launch |
| Runtime | 8,152 memory leaks at exit (singletons/caches, expected) | Deferred | v1 post-launch |

## Session Continuity

Last session: 2026-06-16T00:52:00.000Z (completed 31-05-PLAN.md — BITS-03 serialization-width + layout guards; Archive std::map count pinned to uint32_t + exercising TU + static_assert layout guards; 5/6 Phase-31 plans done, only the plan-06 gate remains)
Prior session: 2026-06-16T00:17:19.126Z
Earlier session: 2026-06-15T16:30:00.000Z (completed 30-03-PLAN.md — master-detail SPA; SC#4 human-verify APPROVED; Phase 30 + all v2.3 phases done, 19/19 plans. v2.3 milestone closed + tagged.)
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
