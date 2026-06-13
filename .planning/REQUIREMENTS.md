# Requirements: whitengold — v2.3 Hardening

**Defined:** 2026-06-12
**Core Value:** Every change must leave the client bootable to character select (the TRE tool is a standalone web app, outside that invariant but inside this milestone).

## v2.3 Requirements

Requirements for this milestone. Each maps to roadmap phases.

### Client Hardening

- [ ] **HARD-01**: DPVS occlusion is config-gated per the Phase 23 verdict — auto mode enables the occlusion bit only outside POB cells (outdoor on / indoor off), with an explicit config override for forcing either mode
- [ ] **HARD-02**: Cantina corner-snap no longer occurs — re-entrancy guard stops the same-frame portal ping-pong without breaking legitimate fast door traversals (verified with the committed CORNERSNAP instrumentation before it is removed)
- [ ] **HARD-03**: D-15 DPVS instrumentation and CORNERSNAP `_DEBUG` probes are removed from shipped code paths (sequenced after HARD-02 is verified — the probes are its acceptance harness)
- [ ] **HARD-04**: Opening the Options window no longer FATALs — `checkShowToolbarCommandCooldownTimer` CodeData/.ui mismatch fixed (pre-existing, from feature commit `d1b3c0eaf`)
- [ ] **HARD-05**: D3D9 shader compilation uses `D3DCompile` instead of `D3DXCompileShader` (Fix B) — preceded by an asm-shader census (`D3DCompile` is HLSL-only); the Phase-19 SEH guard is retained for any path still on D3DX and superseded where ported

### Machine Portability

- [ ] **PORT-01**: A fresh clone + build produces a bootable client with no machine-specific absolute paths — `stage/override` and `stage/miles` handling documented and/or automated (miles redist is not in git; postbuild doesn't copy it)
- [ ] **PORT-02**: `stage/client_d.cfg` is cleaned of accumulated Phase-11+ test settings; client boots clean under both `rasterMajor=5` and `rasterMajor=11` afterward

### TRE Compare Tool

- [ ] **TRE-01**: User can point the tool at two SWG installations and scan their TRE sets — archives discovered plus each install's cfg search-path order parsed (engine-faithful: `searchTree`/`searchTOC`/`searchPath` priorities)
- [ ] **TRE-02**: User can see a set-level compare — which TRE archives are added/removed/changed between the two installations, with size/version deltas
- [ ] **TRE-03**: User can see a file-level compare of the merged virtual trees — first-match-wins precedence mirroring `TreeFile.cpp` semantics (incl. `fixUpFileName` normalization), per-file added/removed/changed status
- [ ] **TRE-04**: User can drill into any file — metadata, which archive wins, shadowed copies in other archives, and on-demand content comparison (TOC `crc` is a path CRC; content change detection uses length/compressedLength + real hashing on demand)
- [ ] **TRE-05**: The UI is a modern web app — virtualized tree view with added/removed/changed badges, hide-identical filter, search, and summary stats

## Future Requirements

Deferred to future milestones. Tracked but not in current roadmap.

### TRE Management (the tool's trajectory — compare is increment 1)

- **TREM-01**: User can extract individual files or whole archives from a TRE set
- **TREM-02**: User can repack/build TRE archives (override-management workflow)
- **TREM-03**: IFF-aware content diffing/preview for SWG asset types
- *Constraint on v2.3:* the compare tool's architecture (parser layer, API shape, repo location) must not preclude these — read-only is v2.3 scope, not a design ceiling.

### Client

- **SYNC-01**: SWG-Source community-compat fixes synced from client-tools master
- **VAL-01**: Nyquist validation backfill for phases 18, 19–22 (milestone audit currently stands as the verification record)

## Out of Scope

Explicitly excluded. Documented to prevent scope creep.

| Feature | Reason |
|---------|--------|
| Gameplay-parity work (combat, space, professions…) | Future-milestone arc; this milestone is consolidation |
| TRE editing/repack in this milestone | Compare ships first; management capabilities are the explicit next increment (TREM-01..03) |
| IFF-aware content diffing | v2+ of the tool; hash/metadata diff is sufficient for the space-asset use case |
| Hosting the TRE tool beyond localhost | Single-user local tool against local installs; no auth/multi-user surface |
| Acting further on DPVS beyond the config-gate | Scene-aware redesign of culling is its own future effort; HARD-01 only operationalizes the Phase 23 verdict |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| HARD-01 | Phase 24 | Pending |
| HARD-02 | Phase 25 | Pending |
| HARD-03 | Phase 26 | Pending |
| HARD-04 | Phase 26 | Pending |
| HARD-05 | Phase 27 | Pending |
| PORT-01 | Phase 24 | Pending |
| PORT-02 | Phase 24 | Pending |
| TRE-01 | Phase 28 | Pending |
| TRE-02 | Phase 29 | Pending |
| TRE-03 | Phase 29 | Pending |
| TRE-04 | Phase 29 | Pending |
| TRE-05 | Phase 30 | Pending |

**Coverage:**
- v2.3 requirements: 12 total
- Mapped to phases: 12 ✓ (100% — Phases 24–30)
- Unmapped: 0

---
*Requirements defined: 2026-06-12*
*Last updated: 2026-06-12 — roadmap created (Phases 24–30); all 12 requirements mapped*
