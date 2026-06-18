# Requirements: whitengold — v3.0 x64 Port

**Defined:** 2026-06-15
**Core Value:** Every change must leave the client bootable to character select under both renderers (`rasterMajor=5` D3D9 + `=11` D3D11). For v3.0 the bar extends: the new **x64** build must reach the same boot-to-character-select parity the 32-bit build already has — never regress either renderer in either bitness.

## v3.0 Requirements

Requirements for this milestone. Each maps to roadmap phases (continuing from Phase 30 → starting at Phase 31).

### x64 Build Platform

- [x] **X64-01**: A native 64-bit client builds — the `x64` platform is added to `src/build/win32/swg.sln` and every dependent `.vcxproj`; `SwgClient_d.exe` / `SwgClient_r.exe` are produced as x64 binaries (dumpbin-confirmed `machine (x64)`)
- [x] **X64-02**: The D3D9 renderer plugins (gl05 / gl06 / gl07) build as x64 and the x64 client boots to character select under `rasterMajor=5`, `=6`, and `=7`
- [x] **X64-03**: The D3D11 renderer plugin (gl11) builds as x64 and the x64 client boots to character select under `rasterMajor=11`
- [x] **X64-04**: All third-party dependencies resolve as x64 (dpvs, bink, pcre, libxml2, icu, jpeg, zlib, discord-rpc, …) — the x64 client launches with no missing-DLL popup or load failure at boot (Restoration's `D:\SWG Restoration\x64\` is the availability reference)

### 64-bit Correctness

- [x] **BITS-01**: The x87 FPU-control inline asm (`FloatingPointUnit.cpp` `__asm fnstcw/fldcw`) is replaced with `_controlfp` / `_control87` intrinsics, and the whole tree is swept for `__asm` and made x64-clean (x64 forbids inline asm)
- [x] **BITS-02**: Pointer/integer truncation defects are resolved — the touched code compiles x64-clean with truncation warnings (C4311 / C4312 / C4244) treated as errors; no `(int)pointer` / `DWORD`-holds-pointer survivors in the build path
- [x] **BITS-03**: Struct packing / hardcoded `sizeof` / serialization-width assumptions are audited and corrected for IFF/TRE and network-message layouts — no x64 data-layout regressions (assets load, saved data and network messages parse correctly)

### Audio Port (Miles 7.2e → 9.3b)

- [x] **AUDIO-01**: The client links the vendored x64 **Miles 9.3b** SDK — the `clientAudio` call sites (`Audio.cpp`, `SoundObject3d.cpp`, `FirstClientAudio.h`) are ported from the 7.2e API to 9.x (incl. the `AIL_room_type` signature edit); SDK vendored into `src/external/3rd/library/miles-9.3b/`
- [x] **AUDIO-02**: The x64 Miles redist + provider set (`mss64.dll` + `.asi`/`.flt` providers) is staged next to the x64 exe; in-game audio works — music, 2D UI, 3D positional, and reverb/room-type — without the half-dead-audio / warning-flood failure mode

### Shader Pipeline (HARD-05 carry-forward)

- [ ] **SHADER-01**: The legacy D3DX shader-compile path is ported to `d3dcompiler_47` (`D3DCompile`) and **D3DX is removed from the x64 build** (D3DX is x64-hostile); both renderers compile/load their shaders correctly under x64. *(Carries the v2.3-deferred HARD-05; the Fix-A SEH guard is retained until the asm path is also off D3DX.)*

### Verification & Cleanup (HARD-02 / HARD-03 carry-forward)

- [ ] **VERIFY-01**: The cantina door-snap is confirmed **resolved** in the x64 build — verified against the kept CORNERSNAP `_DEBUG` probes and the clean Restoration x64 D3D9 reference behavior. *(Carries the v2.3-deferred HARD-02 — root-caused as a 32-bit codegen float transient that x64's deterministic SSE float removes.)*
- [ ] **VERIFY-02**: The 32-bit address-space OOM crash class is eliminated — an extended world-load / play session no longer hits the `MemoryManager` OOM FATAL (`b0780503` class) that 32-bit exhaustion produces
- [ ] **VERIFY-03**: The CORNERSNAP `_DEBUG` instrumentation (`CollisionResolve.cpp` + `CellProperty.cpp`) is removed from shipped code paths **after** the door-snap is verified clean — completing the deferred half of HARD-03 (link-grep 0 unresolved on removal)

## Future Requirements

Deferred to future milestones. Tracked but not in this roadmap.

### Client

- **SYNC-01**: SWG-Source community-compat fixes synced from client-tools master
- **VAL-01**: Nyquist validation backfill for phases 18, 19–22 (milestone audit currently stands as the verification record)

### TRE Management (the compare tool's trajectory)

- **TREM-01**: Extract individual files or whole archives from a TRE set
- **TREM-02**: Repack/build TRE archives (override-management workflow)
- **TREM-03**: IFF-aware content diffing/preview for SWG asset types

## Out of Scope

Explicitly excluded. Documented to prevent scope creep.

| Feature | Reason |
|---------|--------|
| Removing or altering the D3D9 renderers | v3.0 is a platform port that **keeps both** renderers; the D3D9 x64 build is also the door-snap/OOM verification reference (Restoration runs x64 D3D9) |
| New rendering features / further D3D11 visual work | Visual parity already shipped in v2.2; v3.0 changes bitness, not appearance |
| Replacing Miles with open middleware (OpenAL Soft / miniaudio) | Clean-but-expensive fallback only if the 9.3b port proves painful; Restoration kept Miles, signaling replacement is the harder road — not the planned path |
| Gameplay-parity work (combat, space, professions, housing, GCW…) | Future-milestone arc; v3.0 is a platform port |
| Linux / non-Windows builds | Windows-only remains (server-side toolchain anyway) |
| TRE tool management features (TREM-01..03) | Separate tool trajectory; the v2.3 compare tool ships read-only |

## Traceability

Which phases cover which requirements. Filled during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| X64-01 | Phase 33 | Complete |
| X64-02 | Phase 33 | Complete |
| X64-03 | Phase 34 | Complete |
| X64-04 | Phase 33 | Complete |
| BITS-01 | Phase 31 | Complete |
| BITS-02 | Phase 31 | Complete |
| BITS-03 | Phase 31 | Complete |
| AUDIO-01 | Phase 35 | Complete |
| AUDIO-02 | Phase 35 | Complete |
| SHADER-01 | Phase 32 | Pending |
| VERIFY-01 | Phase 36 | Pending |
| VERIFY-02 | Phase 36 | Pending |
| VERIFY-03 | Phase 36 | Pending |

**Coverage:**
- v3.0 requirements: 13 total
- Mapped to phases: 13 (Phases 31-36; roadmap created 2026-06-15)
- Unmapped: 0

---
*Requirements defined: 2026-06-15 — v3.0 x64 Port milestone start*
