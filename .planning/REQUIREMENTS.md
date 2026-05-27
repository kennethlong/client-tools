# Requirements: whitengold — v2.1 "Decruft"

**Defined:** 2026-05-25
**Core Value:** Every change must leave the client bootable to character select.

## v2.1 Requirements

Re-apply the orphaned CLEAN-01..04 dead-code removals against the active Koogie/MSBuild
tree (Phase 9 "Option D" base swap dropped them). Reference diff template: the original
whitengold (swg-client) **Phase 07** removal commits (CLEAN-01..05), retargeted from CMake
to MSBuild (`src/build/win32/swg.sln` + `.rsp` response files).

### Dead-Code Removal

- [x] **DECRUFT-01**: `trackIR/` + `stationapi/` orphaned directories deleted from the MSBuild tree — no project or `.rsp` file references them; none are linked or built; client builds + boots to character select (both renderers)
- [x] **DECRUFT-02**: SwgClientSetup removed — `SwgClientSetup.vcxproj` dropped from `swg.sln` + `src/game/client/application/SwgClientSetup/` directory deleted; client builds + boots
- [x] **DECRUFT-03**: lcdui (G15 LCD, `disableG15Lcd=true`) removed — `lcdui.vcxproj` dropped from `swg.sln` + lcdui include/lib references purged from `.rsp` files; client builds + boots
- [x] **DECRUFT-04**: VideoCapture removed — `VideoCapture_debug.lib` unlinked from SwgClient `libraries_d.rsp` (+ release `.rsp`) + any source/include references purged; client builds + boots
- [x] **DECRUFT-05**: Voice chat (Vivox) fully removed from source and build — `vivoxSharedWrapper_debug.lib` unlinked; `CuiVoiceChatManager.cpp/.h`, `SwgCuiVoiceFlyBar.cpp/.h`, `CuiVoiceChatEventHandler` deleted; voice-related preference keys stripped from `CuiPreferences`; client builds + boots
- [x] **DECRUFT-06**: In-game browser (XPCOM/Mozilla) fully removed from source and build — `libMozilla.vcxproj` dropped from `swg.sln`; `libMozilla/include/public` removed from `includePaths.rsp`; `CuiWebBrowser*`, `UIWebBrowserWidget`, XPCOM bridge code deleted; Mozilla DLLs removed from any POST_BUILD/stage copy list; client builds + boots
- [x] **DECRUFT-07**: Client compiles clean and boots to character select against the SWGSource VM under **both** `rasterMajor=5` (D3D9) and `rasterMajor=11` (D3D11) after the full removal set (cross-cutting milestone gate — mirrors v2.0 CLEAN-05; verified incrementally after each removal)

## Future Requirements

Deferred to **v2.2 Visual Parity** (reordered after v2.1 — cleanup-first shrinks surface area before visual work + SWG-Source upstream imports). Tracked but not in the v2.1 roadmap. Requirements derive from `docs/research/phase12-baseline/COMPARISON.md`.

### Visual Parity (v2.2)

- **VIS-XX**: Asset pixel-shader pipeline — bridge/re-author engine PS so D3D11 binds real asset shaders (D3D9 PEXE bytecode rejected by `CreatePixelShader`; currently falls back to magenta PS) — **the** blocker
- **VIS-XX**: Gamma LUT pass
- **VIS-XX**: Multi-stage texture sampling
- **VIS-XX**: Load-screen half-texel seam
- **VIS-XX**: Circular minimap / radar
- **VIS-XX**: Particles / ribbon rendering

## Out of Scope

| Feature | Reason |
|---------|--------|
| Bink video codec removal | Active cutscene video codec — higher blast radius than the dormant modules; not in v2.1 stated scope. Was bundled in original CLEAN-02; can be a future call. |
| Any D3D11 visual-parity work | Deferred to v2.2; v2.1 is cleanup-only by design |
| CLEAN-06 ~30 deferred tool projects | Tooling integration backlog (MFC↔STLPort, Qt batch, NGE mismatches) — separate from dead-code removal |
| Stale `stlport453/` path strings in `.rsp` files | Inert (dir already deleted; MSVC ignores) — out of scope unless a removal step touches the same file, then opportunistic cleanup is fine |

## Traceability

Which phases cover which requirements. Updated during roadmap creation (2026-05-25).

| Requirement | Phase | Status |
|-------------|-------|--------|
| DECRUFT-01 | Phase 12 | Complete |
| DECRUFT-02 | Phase 12 | Complete |
| DECRUFT-03 | Phase 12 | Complete |
| DECRUFT-04 | Phase 13 | Complete |
| DECRUFT-05 | Phase 14 | Complete |
| DECRUFT-06 | Phase 15 | Complete |
| DECRUFT-07 | Phase 15 | Complete (cross-cutting gate; verified incrementally in Phases 12–15, full-removal-set dual-renderer boot human-confirmed PASS at commit 16fd3ac4c) |

**Coverage:**
- v2.1 requirements: 7 total
- Mapped to phases: 7 (100%) ✓
- Unmapped: 0

**Phase mapping rationale (risk gradient, low → high):**
- Phase 12 — pure directory/project deletes (trackIR, stationapi, SwgClientSetup, lcdui): lowest risk; re-establishes the dual-renderer boot baseline.
- Phase 13 — VideoCapture lib unlink: low/medium risk; `.rsp` edit + residue purge.
- Phase 14 — Vivox source removal: higher risk; touches live UI + `CuiPreferences`.
- Phase 15 — XPCOM/Mozilla browser removal: highest risk (live UI + project + include path + stage copy list), and owns the DECRUFT-07 milestone-wide gate as the final phase. The `/Zc:wchar_t` interlock that historically blocked XPCOM removal is already resolved (Phase 9 went `char16_t`), so this is a clean unlink.

---
*Requirements defined: 2026-05-25 — milestone v2.1 "Decruft"*
*Last updated: 2026-05-25 — roadmap created; 7/7 requirements mapped to Phases 12–15 (100% coverage)*
