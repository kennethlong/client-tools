# Project Research Summary

**Project:** whitengold (swg-client-v2) -- Milestone v2.3 Hardening
**Domain:** (1) In-client C++ hardening fixes; (2) standalone web-based TRE archive compare tool
**Researched:** 2026-06-12
**Confidence:** HIGH

## Executive Summary

v2.3 Hardening has two distinct, non-interfering work streams. The first is a set of six in-client C++ fixes: a DPVS config-gate (default and auto-mode logic), removal of diagnostic instrumentation (DPVS D-15 probes and the CORNERSNAP debug harness), machine-portability de-hardcoding (paths and stage/client_d.cfg cleanup), an Options-window FATAL fix, a D3DXCompileShader-to-D3DCompile API port, and the cantina corner-snap re-entrancy guard. The second is a genuinely new user-facing deliverable: a modern web-based tool for comparing two SWG installations at both the archive-set level and the merged-virtual-file-tree level -- a capability that does not exist anywhere in the SWG tooling ecosystem. These two streams share zero code and can be phased independently.

The TRE compare tool recommendation is decisive: a mature, version-aware, golden-file-tested Python TRE parser already exists at D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py and handles all five format variants plus the COT2000 and SearchTOC master-index layouts. Reimplementing this in any other language is the single largest risk and offers no functional return. The correct architecture is a thin Python/FastAPI backend that reuses this parser, wrapped by a Vite+React+shadcn/ui SPA, served as a localhost server. The tool should live at tools/tre-compare/ inside this repo, co-located with the authoritative TreeFile.cpp engine source it mirrors. While v2.3 scope is read-only compare, the architecture must be designed so that write/extract/repack/override-management capabilities can be added in future milestones without structural rewrites.

The dominant risks are: (a) for the TRE tool -- getting the merged-virtual-tree resolution semantics wrong (the engine highest-priority-number-wins first-hit-wins-within-priority rule is non-obvious and easy to invert), and misidentifying the TOC crc field as a content checksum (it is a path-CRC used as binary-search key, not a content hash; content-change detection requires size comparison + optional real hashing); (b) for the client hardening -- the CORNERSNAP instrumentation removal must be sequenced after the corner-snap fix that uses it as its acceptance harness, and the D3DCompile port must not silently drop the assembly-shader (D3DXAssembleShader) path.

---

## Key Findings

### Recommended Stack

The TRE compare tool should be built as a **Python 3.12 + FastAPI 0.136 + Uvicorn 0.34** backend paired with a **React 19 + Vite 8 + TypeScript + Tailwind v4 + shadcn/ui** frontend, delivered as a single-port localhost server. The decisive rationale is the existing tre_reader.py parser: ~470 lines of pure-stdlib Python, version-aware across all five TREE tags plus COT2000/SearchTOC, with golden-file tests. Every other technology choice follows from the constraint that the backend must be Python. No desktop shell (Tauri, Electron) is warranted for a single-developer local tool -- a browser tab is sufficient.

The note in project memory referencing D:/Code/swg-tools as the parser location is stale. **The live parser is D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py.** Prefer vendoring tre_reader.py + tre_decrypt.py into tools/tre-compare/backend/tre/ (both pure-stdlib, no Blender imports) to keep the tool self-contained and portable.

**Core technologies:**
- **Python 3.12**: Backend runtime -- existing parser is pure-stdlib Python; reuse eliminates the largest risk
- **FastAPI 0.136 + Uvicorn 0.34**: API + server -- typed endpoints, automatic OpenAPI, trivially serves built SPA as static files
- **React 19 + Vite 8 + TypeScript**: Frontend -- broadest support for tree/diff UI components; Vite proxy eliminates CORS in dev
- **Tailwind v4 + shadcn/ui**: Styling + components -- Table, ScrollArea, Tabs, Command palette; owned as source, no runtime lock-in
- **TanStack Virtual v3 + TanStack Query v5**: Virtualized tree rendering (mandatory at 10k-100k entry counts) + async scan lifecycle
- **xxhash (Python)**: Content hashing on the deep-diff path -- ~10x faster than SHA-256; cryptographic strength unnecessary
- **uv**: Python env management -- faster and cleaner than pip/venv on Windows 11

### Expected Features

The TRE compare tool fills a gap that no existing SWG tool addresses: install-vs-install comparison at both the archive-set and merged-virtual-file-tree levels. Every existing tool (TRE Explorer, Swg.Explorer) is single-archive-centric, uses archived WinForms or 2000s-era UIs, and has no comparison concept at all.

**Must have (table stakes for v2.3):**
- TRE archive parser (header + zlib TOC + name block) -- irreducible foundation
- client.cfg [SharedFile] search-priority parse -- required for correct overlay order; cannot substitute a directory glob
- Set-level archive list + delta (added/removed/size/version/file-count)
- Merged virtual file tree per install with engine-faithful overlay resolution
- File-level diff (added/removed/changed) via (length, compressedLength) comparison -- NOT via the TOC crc field (path-CRC, not content hash; see resolved conflict below)
- Tree view with status badges + folder roll-up
- Status filter + name/path search + summary stats
- Per-file detail panel (winning archive, shadowed archives, size, CRC display)
- Virtualised tree rendering (mandatory -- non-virtualised rendering hangs on real data sizes)
- Modern clean web UI

**Should have (add after validation):**
- Hex view of a single selected file (lazy decompress on click)
- Single-file extract / download
- Permalink / shareable diff state (URL-encoded install pair + filter + path)
- Loose-file (searchAbsolute) overlay support

**Defer to future milestones (v2+):**
- In-tool TRE editing/repacking -- future milestone capability; architecture must accommodate it but v2.3 is read-only
- IFF-aware chunk-tree preview
- Structural IFF diff (chunk-tree diff of two file versions)
- N-way (3+ install) compare

**Architecture trajectory note:** v2.3 delivers a read-only compare tool. Future milestones will extend toward browse, extract, repack/build, and override management. Architecture decisions in v2.3 (parser abstraction in backend/tre/, vtree.py as an isolated pure module, clean API shape, in-repo location) are explicitly chosen to support this trajectory without a structural rewrite. Any v2.3 design choice that forecloses write capabilities is a known anti-pattern.

### Architecture Approach

The tool is a single Python process (uvicorn app:app) that serves both the JSON API and the built Vite SPA on 127.0.0.1:8000. All filesystem work happens server-side; the browser renders JSON only. The project structure lives at tools/tre-compare/ inside this repo, co-located with the TreeFile.cpp engine source it mirrors.

The single most important correctness decision is that vtree.py must mirror the engine TreeFile::install + TreeFile::find semantics exactly: nodes sorted by (priority DESC, parseOrder ASC), first-hit-wins, length-0 entries treated as tombstones. This module must have its own unit tests against the real stage/client.cfg before any UI code exists.

**Major components:**
1. **TRE/TOC parser** (backend/tre/) -- vendored tre_reader.py + tre_decrypt.py; reads all format variants; do not rewrite
2. **Installation scanner** (scanner.py) -- hand-parses client.cfg [SharedFile]; stdlib configparser cannot handle repeated keys; yields priority-ordered node list
3. **Virtual-tree builder** (vtree.py) -- first-hit-wins merge + tombstones + fixUpFileName canonicalization; the correctness keystone
4. **Diff engine** (diff.py) -- set-delta + file-level diff using (length, compressedLength) as change signal; never decompresses up front
5. **Index cache** (cache.py) -- sqlite persistence keyed by (abspath, mtime, size); makes re-runs instant after initial parse
6. **Web API** (api.py/app.py) -- FastAPI routes; paginated file lists; lazy file-detail on demand; serves static frontend
7. **Frontend SPA** -- install picker, set-delta table, filterable virtualised tree-diff, file-detail compare

### Critical Pitfalls

**Cross-document conflict resolved:** FEATURES.md states the TOC per-entry crc field enables free changed detection. PITFALLS.md (verified against engine source) establishes that crc is Crc::calculate(fileName) -- a CRC of the path string, not the file content. It is the binary-search key inside each archive, not a content checksum. Two archives can have the same path-CRC for the same filename while the file bytes differ entirely -- that is the purpose of override TREs. **PITFALLS.md wins.** Content-change detection must use (length, compressedLength) as the cheap first signal and a real content hash for confirmation.

1. **TOC crc is a path-CRC, not a content checksum** -- use (length, compressedLength) for changed-file detection; document this distinction in the diff UI
2. **Search-path precedence inversion** -- higher priority number = searched first (front of list); first-hit-wins; test against real stage/client.cfg that searchPath_00_10 beats searchTOC_00_0
3. **Directory-glob instead of client.cfg parse** -- stdlib configparser mangles repeated keys; hand-parse [SharedFile]
4. **TRE version hardcoding** -- branch on (token, version) exactly as the engine does; COT2000 fileNameOffset-is-length quirk is a known silent mis-parse trap
5. **Eager full-archive hashing** -- compare by TOC metadata first; decompress only on file-detail drill-in; 30-archive compare must complete in seconds
6. **CORNERSNAP removal sequencing** -- CORNERSNAP _DEBUG probes (a9b419daf) are the acceptance harness for the corner-snap fix; strip them only after the fix is verified
7. **D3DCompile assembly-path gap** -- D3DCompile compiles HLSL only; census the asm (.vsh) shader count before starting; keep D3DXAssemble + SEH guard until the asm path is explicitly handled

---

## Implications for Roadmap

The two work streams are independent and can be parallelised. Within client hardening: CORNERSNAP removal must follow the corner-snap fix; D3DCompile needs an asm-shader census first. Within the TRE tool: parser -> scanner -> vtree -> diff -> API -> UI is a hard dependency chain.

### Phase 1: DPVS Config-Gate + Machine Portability

**Rationale:** Least-risky hardening items; provide early dual-renderer boot confidence before touching more complex items.
**Delivers:** DPVS occlusion auto-mode keyed on POB-cell membership; de-hardcoded stage/override + stage/miles paths; stage/client_d.cfg cleanup; dual-renderer (rasterMajor=5 + 11) boot verified on a non-dev machine.
**Avoids:** Pitfall 11 -- config-gate default flip without dual-renderer test; portability change that assumes stage/miles/ present on a fresh machine.
**Research flags:** None -- DPVS verdict doc docs/recon/23-dpvs-d3d11-profiling.md is the spec.

### Phase 2: Cantina Corner-Snap Fix

**Rationale:** Root cause is fully known; CORNERSNAP instrumentation is the acceptance harness; must complete before Phase 3 removes that harness.
**Delivers:** Re-entrancy guard that suppresses A->B->A oscillation specifically; fast door-traversal regressions absent; instrumentation confirms zero ping-pong frames.
**Avoids:** Pitfall 8 -- blanket one-transition-per-frame guard that breaks legitimate door traversals.
**Research flags:** None -- mechanism is committed to memory; implement against existing instrumentation.

### Phase 3: Instrumentation Removal (D-15 DPVS + CORNERSNAP)

**Rationale:** Sequenced after Phase 2 (CORNERSNAP harness needed through acceptance) and after Phase 1 (D-15 profiling purpose superseded by DPVS verdict).
**Delivers:** Both probe sets removed; callers + config-flag registrations + build-graph entries stripped atomically; Debug+Release link clean with zero unresolved externals (grep link output -- not just MSBuild exit 0).
**Avoids:** Pitfall 10 -- stripping the symbol without stripping callers; /FORCE masking makes MSBuild exit 0 insufficient.
**Research flags:** None -- atomic removal pattern in project_decruft_removal_build_graph_gotchas.

### Phase 4: Options-Window FATAL Fix

**Rationale:** Self-contained; no sequencing dependencies; placed after build is clean.
**Delivers:** Options window no longer produces a FATAL under D3D11 or D3D9; both renderers verified.
**Research flags:** None -- targeted bug fix; root cause investigation is the first step.

### Phase 5: D3DCompile Port (D3DXCompileShader to D3DCompile)

**Rationale:** Most technically complex hardening item. Requires asm-shader census before starting. SEH guard (Fix A, db83b0f5c) remains as safety net until port is proven.
**Delivers:** D3DXCompileShader replaced with D3DCompile (HLSL path); include handler reimplemented against ID3DInclude; d3dcompiler_47.dll staged; asm-input shaders explicitly handled (via D3DAssemble or confirmed HLSL-available); D3D9 visual parity A/B baseline held; SEH guard retained until assembly path is also off D3DX.
**Avoids:** Pitfall 9 -- silently dropping the asm path (null VS -> skipped draws); copying D3DXSHADER_* flag values verbatim (re-derive by intent); removing SEH guard prematurely.
**Research flags:** Needs pre-phase asm-shader census (grep D3DXAssembleShader + .vsh reference count). D3DAssemble is sparsely documented; census result determines the approach.

### Phase 6: TRE Compare Tool -- Foundation (Parser + Scanner + Virtual Tree)

**Rationale:** Backend correctness must be locked before any web code exists. This phase is entirely headless and fully unit-testable.
**Delivers:** tools/tre-compare/ scaffolded; backend/tre/ vendored from swg-blender-plugin; scanner.py parses client.cfg [SharedFile] correctly (repeated keys, priority extraction); vtree.py implements first-hit-wins + tombstones + fixUpFileName; unit tests verify correct override shadowing and tombstone behavior against real stage/client.cfg.
**Addresses:** TRE parser, client.cfg parse, merged virtual tree (all P1 features).
**Avoids:** Pitfalls 1 (version hardcoding), 2 (precedence inversion), 3 (directory glob), 4 (case/slash mismatch).
**Research flags:** None -- format spec fully derived from engine source in ARCHITECTURE.md; vtree pseudocode provided; vendor existing parser.

### Phase 7: TRE Compare Tool -- Diff Engine + API

**Rationale:** With vtree proven, diff is a pure dict-vs-dict pass with no I/O, fully testable headless. Building diff before UI means the UI works against real data from day one.
**Delivers:** diff.py set-delta + file-level diff ((length, compressedLength) change signal, not CRC); cache.py sqlite index cache keyed by mtime+size; FastAPI routes for summary + paginated file rows + lazy file-detail; correct diff JSON for the SWGSource-vs-whitengold pair.
**Avoids:** Pitfalls 5 (eager hashing), 6 (CRC misuse), 7 (Windows file-locking).
**Research flags:** None -- tiered comparison design fully specified in PITFALLS.md.

### Phase 8: TRE Compare Tool -- Frontend SPA

**Rationale:** Last phase; builds web UI over a proven running API. TanStack Virtual must be wired from the first working tree view -- not retrofitted.
**Delivers:** React+Vite+shadcn/ui SPA with install picker, set-delta table, filterable virtualised file-tree diff, per-file detail panel; tool works end-to-end for the SWGSource-vs-whitengold space-asset diagnosis.
**Addresses:** All remaining P1 features: modern clean UI, virtualised tree, status filter, search, summary stats, per-file detail.
**Research flags:** None -- shadcn/ui + TanStack Virtual + TanStack Query are well-documented.

### Phase Ordering Rationale

- Client hardening (Phases 1-5) and TRE tool (Phases 6-8) are independent streams; parallelise or reorder by priority. If the space-artifact diagnosis is urgent, Phases 6-8 can run concurrently with or ahead of Phases 1-5.
- Within client hardening: Phase 2 (corner-snap) must precede Phase 3 (removal). Phase 5 (D3DCompile) requires the census but is otherwise independent. Phases 1 and 4 have no dependencies on each other.
- Within the TRE tool: Phase 6 -> Phase 7 -> Phase 8 is a hard dependency chain.
- The architecture-forward structure of Phases 6-7 (no UI until backend logic proven) is specifically motivated by Pitfalls 2, 3, and 4 -- all produce silent wrong answers not visible until real data was flowing.

### Research Flags

Phases needing deeper research during planning:
- **Phase 5 (D3DCompile port):** Pre-phase asm-shader census required to scope the D3DXAssembleShader replacement. D3DAssemble is sparsely documented; census output determines the approach.

Phases with standard patterns (skip research-phase):
- **Phases 1-4 (client hardening):** All have documented patterns in project memory or existing verdict docs.
- **Phases 6-8 (TRE tool):** Format spec fully derived from engine source; parser already exists; architecture fully specified in ARCHITECTURE.md.

---

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | TRE parser confirmed on disk and read in full; FastAPI/Vite/shadcn/ui version compatibility confirmed. Stale D:/Code/swg-tools pointer confirmed absent -- correct path is D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py. |
| Features | HIGH | Engine source for TRE format read directly (authoritative); competitor landscape surveyed; CRC conflict with PITFALLS.md resolved (PITFALLS.md wins). |
| Architecture | HIGH | Engine source (TreeFile.cpp, TreeFile_SearchNode.cpp, Crc.cpp) read in full; resolution semantics, canonicalization, tombstone behavior all derived from source. stage/client.cfg confirmed as canonical test fixture. |
| Pitfalls | HIGH | TRE pitfalls derived from engine source. Client-hardening pitfalls from project memory. D3DCompile pitfalls from MS docs + in-tree D3D9 source. One MEDIUM area: cross-distribution TRE variant claims (community data real but under-documented; in-tree source is authoritative regardless). |

**Overall confidence:** HIGH

### Gaps to Address

- **Asm-shader census (Phase 5):** Count of .vsh / D3DXAssembleShader call sites unknown; run as first step of Phase 5 planning.
- **CORNERSNAP guard exact implementation:** Root cause known (A->B->A reversal per frame); exact reversal-detection logic should be written against committed instrumentation output to confirm snap-gone AND traversal-correct before closing the phase.
- **Future TRE tool write-path architecture:** v2.3 is read-only; parser abstraction boundary, API surface, and vtree/diff module separation should be made with explicit awareness that write capabilities are the next milestone target. Phase docs should flag this so future work requires no structural changes.
- **searchAbsolute loose-file overlay support:** Confirmed gap for v2.3; both installs are TRE-driven for the immediate use case. Flagged as v1.x addition.

---

## Sources

### Primary (HIGH confidence)

- src/engine/shared/library/sharedFile/src/shared/TreeFile.cpp -- install, find/open, fixUpFileName, searchNodePriorityOrder
- src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.{h,cpp} -- binary layout, CRC binary search, length-0 tombstone, COT2000 quirk
- src/engine/shared/library/sharedFoundation/src/shared/Crc.cpp -- CRC-32/MPEG-2 variant (poly 0x04C11DB7, MSB-first)
- D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py -- existing version-aware parser (read in full, confirmed on disk)
- src/engine/client/application/Direct3d9/src/win32/Direct3d9_VertexShaderData.cpp -- D3DXCompileShader/D3DXAssembleShader + include handler
- stage/client.cfg / stage/client_d.cfg -- real [SharedFile] layout (maxSearchPriority=12, 4x searchTOC, 2x searchTree, searchPath @priority 10/11)
- Microsoft Learn D3DCompile: https://learn.microsoft.com/en-us/windows/win32/api/d3dcompiler/nf-d3dcompiler-d3dcompile
- FastAPI release notes: https://fastapi.tiangolo.com/release-notes/
- Vite releases: https://vite.dev/releases
- React 19.2 blog: https://react.dev/blog/2025/10/01/react-19-2
- shadcn/ui Tailwind v4 docs: https://ui.shadcn.com/docs/tailwind-v4

### Secondary (MEDIUM confidence)

- swg_tre Rust crate: https://lib.rs/crates/swg_tre -- parser cross-reference; in-tree source supersedes for exact layout
- Swg.Explorer (wverkley): https://github.com/wverkley/Swg.Explorer -- competitor feature baseline
- TRE Explorer (MTGUli/ilikenwf): https://github.com/MTGUli/TREExplorer -- competitor feature baseline
- Electron vs Tauri 2026 guide: https://www.pkgpulse.com/guides/electron-vs-tauri-2026 -- bundle-size comparison informing no-desktop-shell decision
- Project memory records: project_cantina_corner_snap_engine_quirk, project_decruft_removal_build_graph_gotchas, project_audio_fixed_missing_miles_redist, project_phase23_dpvs_verdict_option_alpha_revised

### Tertiary (LOW confidence)

- SWGANH Wiki TRE breakdown: http://wiki.swganh.org/index.php/TRE:TRE_Breakdown -- community format documentation; superseded by in-tree source for exact layout

---
*Research completed: 2026-06-12*
*Ready for roadmap: yes*