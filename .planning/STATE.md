---
gsd_state_version: 1.0
milestone: v2.3
milestone_name: Hardening
status: ready_to_plan
last_updated: "2026-06-13T01:49:23.962Z"
last_activity: 2026-06-13 -- Phase 24 execution started
progress:
  total_phases: 7
  completed_phases: 1
  total_plans: 3
  completed_plans: 0
  percent: 14
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-06-12 after v2.2 close)

**Core value:** Every change must leave the client bootable to character select. Visual parity achieved in v2.2 — D3D11 now matches the D3D9 baseline; never regress either renderer. The v2.3 TRE compare tool is a standalone web app, outside that invariant but inside this milestone.
**Current focus:** Phase 24 — dpvs-config-gate-machine-portability

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

Phase: 25
Plan: Not started
Status: Ready to plan
Last activity: 2026-06-13

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

Last session: 2026-06-13T00:55:22.820Z
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
