---
phase: 17-psrc-census-char-select-beachhead
plan: 01
subsystem: renderer
tags: [d3d11, d3d9, clientGraphics, ShaderImplementation, ConfigClientGraphics, psrc, census, char-select, hlsl, asm, iff, abi-cascade]

# Dependency graph
requires:
  - phase: 11-d3d11-plugin
    provides: "compilePixelShaderFromHlsl helper (unused for assets until Plan 02 wires it); D3DReflect VS precedent; SRV slots 0..7 bound; PerMaterialCB struct + updatePS upload"
  - phase: 12-visual-baseline
    provides: "matched-pair COMPARISON dir + naming convention (docs/research/phase12-baseline/COMPARISON.md) — mirrored by docs/research/phase17-char-select/COMPARISON.md (D-05)"
provides:
  - "PSRC source retention on ShaderImplementationPassPixelShaderProgram (m_psrcText/m_psrcLen) — Plan 02 reads this to recompile //hlsl PSRC into a real D3D11 PS"
  - "Shared ConfigClientGraphics psrcCensus flag (KEPT, not throwaway — Phase 20 re-runs the census on an open-world boot per D-03)"
  - "Flag-gated PSRC language census emission to BOTH DEBUG_REPORT_LOG_PRINT AND stage/psrc-census.tsv (HIGH-5 file sink, captures under explorer-launched boots)"
  - "char-select COMPARISON dir + naming convention scaffold (docs/research/phase17-char-select/COMPARISON.md) — D-05"
  - "Plan-01-only HEAD sha recorded for census-boot auditability (R3-01d): 5eea334742f9e070ed7364cead452cbfac9df866"
  - "Census ARTIFACT — char-select PS-program HLSL:asm ratio captured: 22 HLSL / 10 asm unique programs (68.75% HLSL); SR-3 STOP NOT TRIGGERED — character-body shaders (h_*/a_*/e_*) are 100% HLSL ps_2_0; asm rows are basic primitives only"
affects: [17-02-psrc-recompile-char-select, 17-03-reflection-driven-constants, 20-world-extension, char-select-validation]

# Tech tracking
tech-stack:
  added: []  # zero new deps; all in-tree
  patterns:
    - "PSRC retain ownership shape mirrors VS m_text (new char[len+1] via Iff::read_string(void), null-terminated; delete[] in dtor + reload with LOW-3 re-null AFTER delete)"
    - "Shared ConfigClientGraphics KEY_BOOL flag gating any diagnostic that must NOT touch the D3D9 path when off"
    - "File-sink mirror of Direct3d11_VertexShaderData.cpp:546-575 (fopen_s + fwrite + fclose) — one-shot diagnostic dump per record"
    - "R3-01c first-non-whitespace HLSL classification (skip BOM + whitespace, then literal lowercase-first-7 == \"//hlsl \" — matches ShaderBuilder PixelShaderProgramView.cpp:301-308)"
    - "R3-01a TSV header bytes via sizeof(literal) - 1 — never hand-counted (eliminates NUL-write risk)"

key-files:
  created:
    - "docs/research/phase17-char-select/COMPARISON.md (D-05 char-select COMPARISON scaffold; aggregation point for stage/psrc-census.tsv per HIGH-5; records the Plan-01-only HEAD sha per R3-01d; FULLY POPULATED post-census-boot with 32 unique-program table + HLSL:asm ratio + SR-3 verdict)"
    - "docs/research/phase17-char-select/char_default_d3d11_0001.png (1920x1080 char-select default-pose D3D11 baseline screenshot; demonstrates the pre-Plan-17-02 washed-out fallback-PS state)"
    - "docs/research/phase17-char-select/psrc-census.tsv (snapshot of the gating artifact at census-boot time; 127 raw rows / 32 unique programs)"
  modified:
    - "src/engine/client/library/clientGraphics/src/shared/ShaderImplementation.h (added m_psrcText/m_psrcLen public members on ShaderImplementationPassPixelShaderProgram)"
    - "src/engine/client/library/clientGraphics/src/shared/ShaderImplementation.cpp (ctor init list + dtor free + reload free/re-null + load_0000 retain-with-cap + flag-gated census log+file emission with R3-01c classification + R3-01a header bytes)"
    - "src/engine/client/library/clientGraphics/src/shared/ConfigClientGraphics.h (added getPsrcCensus() declaration)"
    - "src/engine/client/library/clientGraphics/src/shared/ConfigClientGraphics.cpp (added ms_psrcCensus member, KEY_BOOL(psrcCensus, false) registration on [ClientGraphics], getter definition)"

key-decisions:
  - "Retain happens UNCONDITIONALLY (so Plan 02's recompile lane always has the text); only census EMISSION is flag-gated. Default false so D3D9 path is behaviorally unchanged when off."
  - "Census log sink (DEBUG_REPORT_LOG_PRINT) AND file sink (stage/psrc-census.tsv) — file sink is the gating artifact because explorer-launched boots cannot see the debug print sink (HIGH-5)."
  - "Sanity cap on PSRC chunk length = 1 MiB (LOW-3 / T-17-01 / ASVS V5); over-cap warns + skips retain (no allocation)."
  - "m_psrcLen stores the IFF chunk-bytes-consumed length (INCLUDES the trailing NUL — Iff.cpp:1606); downstream consumers MUST use strlen(m_psrcText) for the source-text length (R3-01b)."
  - "Classification skips UTF-8 BOM + leading whitespace BEFORE the lowercase-first-7 == \"//hlsl \" compare (R3-01c); mirrors ShaderBuilder's first-line semantics."
  - "TSV header byte count derived from sizeof(literal) - 1, never hand-counted (R3-01a) — eliminates NUL-write risk."
  - "D3D9 ground-truth pair DEFERRED at census-boot time (Kenny's call): gl05_d.dll is rebuilt and staged; the HLSL:asm ratio + SR-3 verdict from the D3D11 census + screenshot is sufficient evidence for Plan 17-02 to proceed. D3D9 pair captured later costs one boot cycle."
  - "Public-member storage strategy KEPT (vs sidecar map) per Kenny's call after the ABI-cascade snag: Plan 17-02 + 17-03 both modify Direct3d11 plugin source so the rebuild cascade happens naturally as part of the phase. See `[[project_shared_header_abi_cascade_trap]]` for the architectural alternative considered + why deferred."

patterns-established:
  - "Pattern: PSRC retain ownership shape on PS-program struct (mirrors VS m_text/m_textLength; new char[len+1] via Iff::read_string; delete[] in dtor + reload; re-null AFTER delete on reload to guard against dangling pointer)"
  - "Pattern: shared-clientGraphics diagnostic gating via ConfigClientGraphics::KEY_BOOL on the [ClientGraphics] section — NOT the plugin's [Direct3d11]/ConfigDirect3d11 (clientGraphics cannot link the plugin)"
  - "Pattern: dual-sink diagnostic emission (log + file) for any artifact that must survive an explorer-launched boot"
  - "Pattern: cwd-relative diagnostic paths from shared/clientGraphics use `stage/...` literal — resolves on host to `D:/Code/swg-client-v2/stage/stage/...` (exe cwd is `stage/`). Mirrors Direct3d11_VertexShaderData.cpp's stage/vs-disasm-all.txt convention."

requirements-completed: []  # plan ENABLES CHAR-01/02/03 (Plan 02 satisfies CHAR-01; Plan 03 satisfies CHAR-02/03) but SATISFIES none directly — see plan's enables: [CHAR-01, CHAR-02, CHAR-03]

# Metrics
duration: ~3h (incl. ABI-cascade diagnosis + plugin rebuilds + host stage prep)
completed: 2026-05-28
---

# Phase 17 Plan 01: PSRC Census + Char-Select Beachhead — COMPLETE

**Retained the discarded TAG_PSRC source text on ShaderImplementationPassPixelShaderProgram and added a flag-gated PSRC language census at the load site emitting to BOTH DEBUG_REPORT_LOG_PRINT AND stage/psrc-census.tsv — Plan 02's recompile lane now has the source text, and the gating HLSL:asm ratio artifact is captured. Census boot executed; char-select reached at 1920x1080 with the expected pre-Plan-17-02 washed-out fallback-PS state; HLSL:asm ratio is 22 HLSL / 10 asm unique programs (68.75% HLSL); SR-3 STOP gate NOT triggered (no asm in character-body shader families h_*/a_*/e_* — all 100% HLSL ps_2_0). Plan 17-02 cleared to proceed.**

## Performance

- **Total duration:** ~3 hours wall-clock (planned: ~30 min; actual: 31 min for Tasks 1+2, then ~2.5 hours for the unplanned ABI-cascade diagnosis + plugin rebuilds + host stage prep before Task 3's boot could succeed)
- **Started:** 2026-05-28T16:45Z
- **Tasks 1+2 + scaffold committed:** 2026-05-28T17:17Z
- **Census boot succeeded:** 2026-05-28T20:09Z (after gl11/gl05 rebuilds)
- **COMPARISON populated + Task 3 finalized:** 2026-05-28T20:20Z
- **Tasks committed:** 3 of 3
- **Files modified:** 4 source + 1 settings.json + 2 memory files + 1 cfg
- **Files created:** 3 docs (COMPARISON.md, screenshot, TSV snapshot)

## Accomplishments

- **m_psrcText / m_psrcLen retain members** added to `ShaderImplementationPassPixelShaderProgram` (public, mirroring the VS `m_text` / `m_textLength` ownership shape) — Plan 02 reads these directly.
- **psrcCensus shared config flag** registered on the `[ClientGraphics]` section via `KEY_BOOL` with a `getPsrcCensus()` getter; default false so the D3D9 path is behaviorally untouched when off. KEPT (not throwaway) per D-03 — Phase 20 re-runs the census on an open-world boot.
- **`load_0000` PSRC retain** via `Iff::read_string(void)` (DIV-1 pin; allocates `new char[len+1]`, returns null-terminated buffer the caller owns). Guarded by:
  - **Empty-PSRC** path (LOW-4) — leaves `m_psrcText = nullptr`, census skip.
  - **1 MiB sanity cap** (LOW-3 / T-17-01 / ASVS V5) — over-cap warns + skips retain (no allocation).
- **Dtor + reload ownership** — `delete[] m_psrcText` in both; reload re-nulls `m_psrcText`/`m_psrcLen` AFTER `delete[]` so a failed mid-reload cannot leave a dangling pointer.
- **Flag-gated census emission** at `load_0000` end:
  - **R3-01c classification:** skip UTF-8 BOM (`EF BB BF`) + leading whitespace BEFORE the lowercase-first-7 `== "//hlsl "` compare (matches ShaderBuilder `PixelShaderProgramView.cpp:301-308` first-line semantics).
  - **Log sink:** `DEBUG_REPORT_LOG_PRINT` (visible under console launch).
  - **File sink (HIGH-5):** per-record `fopen_s` + `fwrite` + `fclose` to `stage/psrc-census.tsv`. R3-01a: header byte count derived from `sizeof(literal) - 1`.
- **TAG_PEXE block byte-for-byte unchanged** (D-06 D3D9 parity invariant).
- **Char-select COMPARISON scaffold** at `docs/research/phase17-char-select/COMPARISON.md` — D-05 dir + naming convention mirroring `phase12-baseline`.
- **Plan-01-only HEAD sha recorded** (R3-01d): `5eea33474` — referenced in COMPARISON.md.
- **Census boot executed under D3D11 (rasterMajor=11) at 1920x1080** — char-select reached cleanly, one wide screenshot captured (`char_default_d3d11_0001.png`), TSV populated with 127 raw rows / 32 unique PS programs.
- **HLSL:asm aggregation + SR-3 verdict written to COMPARISON.md** — 22 HLSL / 10 asm unique; SR-3 STOP NOT TRIGGERED (h_*/a_*/e_* families all HLSL ps_2_0; asm rows are low-level primitives not the character-body path). **Plan 17-02 cleared to proceed without a 17-02B asm-port sub-plan.**
- **Deferred-feed scope verdict for Plan 03** — constants column = 0 across all 32 unique rows (classifier-limitation noted; PSRC declares constants via cbuffer-style syntax, not `register(c…)` hints). No textureScroll/fog/stencil signal at census; Plan 03 scope remains material color + textureFactor only.

## Task Commits

1. **Task 1: add PSRC-retain member + psrcCensus shared config flag** — `3667b3b06` (feat)
2. **Task 2: retain PSRC text + flag-gated census at load_0000 (log + file sinks)** — `5eea33474` (feat) — **R3-01d Plan-01-only HEAD pin**
3. **Task 3 prep: partial SUMMARY + COMPARISON scaffold** — `4da90e5b5` (docs)
4. **Task 3 finalization: census-boot artifacts + COMPARISON aggregation + SUMMARY update** — _next commit (this one)_

## Files Created/Modified

### Created
- `docs/research/phase17-char-select/COMPARISON.md` — D-05 matched-pair scaffold + HLSL:asm ratio aggregation + per-shader table (32 unique programs) + SR-3 STOP verdict + deferred-feed flags + findings buckets.
- `docs/research/phase17-char-select/char_default_d3d11_0001.png` — 1920x1080 D3D11 char-select default-pose baseline screenshot.
- `docs/research/phase17-char-select/psrc-census.tsv` — snapshot of `stage/stage/psrc-census.tsv` from the census boot (127 rows).

### Modified
- `src/engine/client/library/clientGraphics/src/shared/ShaderImplementation.h` — added public `m_psrcText`/`m_psrcLen` members.
- `src/engine/client/library/clientGraphics/src/shared/ShaderImplementation.cpp` — ctor init list + dtor free + reload free/re-null + `load_0000` retain-with-cap + flag-gated census log+file emission.
- `src/engine/client/library/clientGraphics/src/shared/ConfigClientGraphics.h` — added `static bool getPsrcCensus()` declaration.
- `src/engine/client/library/clientGraphics/src/shared/ConfigClientGraphics.cpp` — added `ms_psrcCensus` member, `KEY_BOOL(psrcCensus, false)` registration, getter definition.

### Modified (host stage / out-of-source-tree, NOT committed in this plan)
- `D:/Code/swg-client-v2/stage/client_d.cfg` — `screenWidth=1920` + `screenHeight=1080` for capture quality; `psrcCensus=true` for the census boot, reset to `=false` post-boot per D-03. (Note: also has accumulated phase 11 test settings — see `[[project_stage_client_d_cfg_cleanup_todo]]`.)
- `D:/Code/swg-client-v2/.claude/settings.json` — added `env.MSBUILD` pointing to VS18 BuildTools v145 MSBuild path (orchestrator-side QoL fix surfaced during the ABI-cascade rebuild). Effective on session restart.

## Decisions Made

1. **Public-member storage strategy KEPT** (vs sidecar map): Kenny's call after the ABI-cascade snag — Plan 17-02 + 17-03 both modify Direct3d11 plugin source so the rebuild cascade happens naturally as part of the phase. Sidecar alternative documented in `[[project_shared_header_abi_cascade_trap]]` for future shared-header touches that don't already touch a plugin.
2. **D3D9 ground-truth pair DEFERRED** at census-boot time: `gl05_d.dll` is rebuilt and staged on host; the HLSL:asm verdict from the D3D11 census is sufficient gating evidence for Plan 17-02 to proceed. D3D9 pair captured later (during or after 17-02) costs one boot cycle.
3. **D3D11 baseline screenshot count** = 1 wide overview (`char_default_d3d11_0001.png`) instead of per-anchor zoomed shots. Default-pose char-select shows the eyes/head/body anchors all in one frame; per-anchor zoomed comparisons are deferred until Plan 17-02 / 17-03 validation phases need them.

## Deviations from Plan

1. **UNPLANNED ABI-cascade snag — gl11_d.dll + gl05_d.dll rebuilt out of scope.** Plan 17-01's `files_modified` list did NOT include the renderer plugin .vcxprojs. The new public members on `ShaderImplementationPassPixelShaderProgram` shifted `m_graphicsData`'s struct offset by 16 bytes in `ShaderImplementation.h`, which the host's 4-day-stale `gl11_d.dll` (built 2026-05-24) was NOT compiled against. First census-boot attempt crashed deterministically (3 attempts in a row) with exception `0x087a` + ntdll address `771f4984` on `shader/ui_planet_sel_ordmantel_as8.sht` — distinguishable from the pre-existing intermittent crash class in `[[project_intermittent_tatooine_crash_087a]]` by being deterministic + on a planet-select asset, not Tatooine. Resolution: rebuilt `gl11_d.dll` (Direct3d11.vcxproj) AND `gl05_d.dll` (Direct3d9.vcxproj) inside the worktree against the new header; copied to host stage/. New trap memory recorded: `[[project_shared_header_abi_cascade_trap]]`.

2. **MSBuild path discovery friction surfaced.** The bash-tool `find` for MSBuild didn't reach VS18 BuildTools on first try; needed a PowerShell `Get-ChildItem` over multiple candidate roots. Resolved permanently by adding `env.MSBUILD` to `.claude/settings.json` so future tool-shells can reference `$env:MSBUILD` / `"$MSBUILD"` without rediscovery. Documented in `[[project_decruft_removal_build_graph_gotchas]]`.

3. **Anchor-name vs file-name semantics mismatch (handled, not a code change).** SR-3 STOP gate was framed in terms of `sul_eye.sht` / `sul_head.sht` anchors, but the census TSV records `.psh` pixel-program names (shader templates reference programs by distinct paths). Reframed at aggregation time as: "are any character-body shader families (h_*/a_*/e_*) asm?". Answer: no — all HLSL ps_2_0. SR-3 NOT triggered. The exact `.sht` → `.psh` mapping for each named anchor will surface during Plan 17-02 Task 3 CHAR-01 validation.

## Issues Encountered

**1. Initial PowerShell build script logger race** (Task 1) — separated console redirect from `/fl` file logger path; build was clean.

**2. ABI cascade — see Deviation #1.** Cost ~2 hours wall-clock (diagnosis + dual plugin rebuilds + redeploy + retry boot). Now documented for future phases.

**3. cwd-double-stage path resolution** — `psrc-census.tsv` initially "missing" from `stage/`; turned out to be at `stage/stage/psrc-census.tsv` (exe cwd is `stage/`; shared/clientGraphics relative paths resolve from there). Matches the established `stage/vs-disasm-all.txt` convention. NOT a bug; pattern recorded in `patterns-established`.

## Pre-existing warnings out of scope (not introduced by this plan)

Four `warning C4840: non-portable use of class 'PersistentCrcString' as an argument to a variadic function` entries in `ShaderImplementation.cpp` at lines that resolved (post-Task-2 insertions) to `:2145`, `:2277`, `:2785`, `:2837` — existing `DEBUG_FATAL` invocations passing `m_fileName` (the `PersistentCrcString` object) instead of `m_fileName.getString()` to a variadic `%s` formatter. They predate Plan 17-01.

## Self-Check: PASSED

- [x] `src/engine/client/library/clientGraphics/src/shared/ShaderImplementation.h` exists and contains `m_psrcText` (verified line 689).
- [x] `src/engine/client/library/clientGraphics/src/shared/ShaderImplementation.cpp` exists and contains `iff.read_string()`, `m_psrcText(nullptr)`, `delete [] m_psrcText` ×2, `getPsrcCensus()` guard, `"//hlsl "`, `0xEF` BOM byte, `hlslStart`, `psrc-census.tsv`, `sizeof(header) - 1`, and 0 hand-counted `fwrite(..., 61, fp)` patterns.
- [x] `src/engine/client/library/clientGraphics/src/shared/ConfigClientGraphics.cpp` contains `ms_psrcCensus`, `KEY_BOOL(psrcCensus`, `getPsrcCensus`.
- [x] `docs/research/phase17-char-select/COMPARISON.md` exists, fully populated with the 32-unique-program census table, HLSL:asm ratio (22/10), SR-3 STOP verdict (NOT triggered), Plan-01-only HEAD sha (`5eea33474`), and findings buckets.
- [x] `docs/research/phase17-char-select/char_default_d3d11_0001.png` exists (362 KB, 1920x1080).
- [x] `docs/research/phase17-char-select/psrc-census.tsv` exists (6477 bytes, 128 lines incl. header).
- [x] Commits `3667b3b06`, `5eea33474`, `4da90e5b5` exist in `git log --oneline --all`.
- [x] Debug build: 0 `unresolved external symbol`, 0 `error C/LNK/MSB`.
- [x] `gl11_d.dll` + `gl05_d.dll` rebuilt against new header — 0 unresolved externals.
- [x] Census boot succeeded under rasterMajor=11 at 1920x1080 windowed; `stage/stage/psrc-census.tsv` populated with 127 rows.
- [x] HLSL:asm verdict aggregated, SR-3 STOP NOT TRIGGERED, Plan 17-02 cleared to proceed.

---
*Phase: 17-psrc-census-char-select-beachhead*
*Plan: 01 — COMPLETE 2026-05-28*
