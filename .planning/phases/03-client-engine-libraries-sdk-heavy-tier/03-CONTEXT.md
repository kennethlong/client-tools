# Phase 3: Client Engine Libraries (SDK-Heavy Tier) — Context

**Gathered:** 2026-05-03
**Status:** Ready for planning

<domain>
## Phase Boundary

Author `CMakeLists.txt` files for all 13 client engine libraries under
`engine/client/library/*` from scratch (no swg-main precedent), exercise every
vendored client SDK through at least one compiled `.cpp` translation unit, and
implement the Mozilla XPCOM stub so that `clientUserInterface` builds without
linking any XPCOM libraries. Phase ends when all 13 targets produce `.lib`
artefacts in both Debug and Release and three consecutive parallel Debug builds
succeed deterministically.

**Libraries in scope (13 total):**
- Tier 6 (basic): `clientAnimation`, `clientObject`, `clientTextureRenderer`, `clientDirectInput`, `clientBugReporting`, `clientParticle`
- Tier 7 (HIGH RISK): `clientGraphics` (DX9 + DPVS), `clientAudio` (Miles)
- Tier 8: `clientSkeletalAnimation`, `clientTerrain`
- Tier 9: `clientUserInterface` (XPCOM stub)
- Integrator: `clientGame` (largest Phase 3 target; depends on all above)

**Not in scope:** SwgClient executable link (`add_executable`) — that is Phase 4.

</domain>

<decisions>
## Implementation Decisions

### DPVS Linkage Strategy

- **D-01:** Use the prebuilt `.lib` from `src/external/3rd/library/dpvs/lib/win32-x86/`
  rather than building from vendored source. The no-source-edits constraint means VS 2022
  compilation failures in old DPVS C++ cannot be fixed in this milestone; the prebuilt
  `/MT`-compiled lib is the safe path.
- **D-02:** For Debug builds, use the same Release-config DPVS prebuilt (there is no
  separate `_d.lib` variant required for v1). Document the CRT assumption in the
  `FindDPVS.cmake` comments.
- **D-03:** If LNK2005 appears at Phase 4 executable link (not Phase 3 static lib
  build), escalate to a source-build of DPVS as the named fallback. Do not add a
  source-build target speculatively in Phase 3.

### Mozilla XPCOM Stub Placement

- **D-04:** Add a new file `src/cmake/stubs/libMozilla_stub.cpp` (new file — not
  editing existing sources) that implements all entry points called by
  `clientUserInterface` as no-ops or `return true`.
- **D-05:** The `libMozilla` CMakeLists.txt builds **only** the stub file, not the
  existing `src/external/3rd/library/libMozilla/src/win32/*.cpp` sources (those
  reference unavailable XPCOM symbols).
- **D-06:** The planner must scan `clientUserInterface`'s `#include "libMozilla/*"`
  call sites to enumerate the entry points the stub needs to provide before writing
  the stub template.
- **D-07:** `clientUserInterface`'s CMakeLists confirms via `dumpbin /symbols` that
  no `xpcom`, `xul`, `nspr4`, `plc4`, or `profdirserviceprovider_s` symbols appear.

### clientGame Phase Boundary

- **D-08:** `clientGame` CMakeLists is authored fully in Phase 3. It is the final
  and most complex Phase 3 target — it depends on all other Phase 3 lib outputs.
- **D-09:** Phase 4 is strictly the executable link step: `add_executable(SwgClient)`
  and `target_link_libraries` with the ~70 inputs. No library CMakeLists authoring
  in Phase 4.

### STLPort First-Use and DX9 Triage Plan

- **D-10:** LNK2005 / LNK2019 storms from STLPort are an executable-link concern
  (Phase 4), not a static-lib concern (Phase 3). Do NOT add STLPort runtime link
  steps (`stlport_static.lib`) to individual Phase 3 static lib targets.
- **D-11:** Tier ordering within Phase 3: author `clientDirectInput` before
  `clientGraphics`. `clientDirectInput` exercises DX9 with the narrower `<dinput.h>`
  surface (no `d3d9.h`, no DPVS). This proves DX9 header resolution in isolation
  before tackling the full `clientGraphics` + `d3d9.h` + DPVS complexity.
- **D-12:** C4530 (exception-handling unwind) warnings are pre-suppressed globally
  for all Phase 3 client targets via `target_compile_options`. This was already
  deferred from Phase 1's `crypto` lib — apply the same treatment consistently.
- **D-13:** If DX9 header conflicts with the Windows SDK appear in `clientGraphics`,
  add `WIN32_LEAN_AND_MEAN` and `NOMINMAX` as `target_compile_definitions` on the
  offending target. Do not add these globally (may affect other targets).

### Precompiled Headers

- **D-14:** Continue the Phase 2 PCH pattern: each library wires `First<LibName>.h`
  via `target_precompile_headers`. This mitigates P1.08 (include-order flakiness
  in parallel builds) and is required for deterministic three-consecutive-build
  verification.

### clientBugReporting

- **D-15:** Build `clientBugReporting` as-is — no stub needed. The crash reporter
  will fail to connect to the defunct SOE endpoint at runtime, which is acceptable
  for v1. No C++ source edits required.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase Goals and Success Criteria
- `.planning/ROADMAP.md` §Phase 3 — authoritative success criteria (5 items),
  tier breakdown, and SDK risk characterisation
- `.planning/PROJECT.md` §Key Decisions — locked Phase 1 decisions (STLPort staged
  layout, DX9 vendored path, Mozilla XPCOM stub strategy) that carry forward into
  Phase 3

### Locked Phase 1 Outputs (verify these exist before planning Phase 3)
- `src/cmake/win32/FindDirectX9.cmake` — vendored DX9 resolution; first real
  compile in Phase 3 clientGraphics depends on this being correct
- `src/cmake/win32/FindDPVS.cmake` — DPVS resolution; must point to prebuilt
  `.lib` per D-01 above
- `src/cmake/win32/FindMiles.cmake` — Miles resolution; `clientAudio` import-lib
  dependency
- `src/cmake/win32/FindMozilla.cmake` — headers-only resolution; stub target
  replaces real XPCOM linkage per D-04/D-05

### Build System Reference (swg-main, server-side only — patterns transferable)
- `C:\Code\swg-main` — reference for CMake structure patterns; no client library
  CMakeLists exist there (Phase 3 is from-scratch), but global flag / PCH / Find
  module patterns are reusable

### SDK Locations in Tree
- `src/external/3rd/library/directx9/` — vendored DX9 headers + import libs
- `src/external/3rd/library/dpvs/` — full source + prebuilt `.lib` at `lib/win32-x86/`
- `src/external/3rd/library/miles/` — Miles import lib (`Mss32.lib`) + headers
- `src/external/3rd/library/vivox/` — Vivox headers + lib (clientUserInterface only)
- `src/external/3rd/library/libMozilla/` — SOE XPCOM wrapper headers + src/win32/
  (src/win32 is NOT compiled per D-05; stub replaces it)
- `src/external/3rd/library/bink/` — Bink headers + import lib (loaded via
  `LoadLibrary` at runtime, not statically linked)

### Research Documents
- `docs/research/swgclient-build.md` — ~70-project dependency graph, tier
  breakdown, flag inventory; authoritative for what each client lib depends on
- `docs/research/runtime-middleware.md` — middleware characterisation for DX9,
  Miles, Vivox, Bink, DPVS, lcdui
- `docs/recon/05-client-boot-sequence.md` — boot phase map; useful to understand
  which client libs are load-critical vs. optional at startup

</canonical_refs>

<code_context>
## Existing Code Insights

### Library Sources (from-scratch; no prior CMakeLists)
- Each library lives at `src/engine/client/library/<libName>/src/` with a
  `First<LibName>.h` / `First<LibName>.cpp` precompiled-header pair — follow the
  same three-level nesting pattern as Phase 2 shared libs: `<lib>/CMakeLists.txt`
  → `<lib>/src/CMakeLists.txt`
- `clientGraphics` houses the DX9 render plug-in path; the whole renderer lives
  inside one DLL plug-in (`gl05_*.dll`) loaded at runtime. The static lib that
  Phase 3 builds is the engine-side abstraction, not the full renderer.

### Established Patterns (from Phase 2 to continue)
- `target_precompile_headers(${LIB_NAME} PRIVATE First${LibName}.h)` on every
  library target (P1.08 mitigation, Phase 2 pattern)
- `target_compile_definitions` for `_USE_32BIT_TIME_T=1`, `_MBCS`, `PRODUCTION=0`,
  `DEBUG_LEVEL=2` — inherited from root via `add_compile_definitions` in
  `src/CMakeLists.txt`; do NOT re-add on individual targets
- `CMAKE_MSVC_RUNTIME_LIBRARY` is set globally (`MultiThreaded$<$<CONFIG:Debug>:Debug>`);
  individual targets must NOT override it

### Integration Points
- Phase 3 outputs feed Phase 4's `target_link_libraries(SwgClient ...)` list
- `clientGame` (final Phase 3 target) is the primary integrator: it links
  `clientGraphics`, `clientAudio`, `clientSkeletalAnimation`, `clientTerrain`,
  `clientUserInterface`, and all Tier 6 libs
- The `clientUserInterface` stub for libMozilla integrates at
  `src/cmake/stubs/libMozilla_stub.cpp` (new file; planner creates it)

</code_context>

<specifics>
## Specific Requirements

- **All four decisions above were made by Claude** (user deferred to builder
  judgment). The planner should treat D-01 through D-15 as locked unless a
  hard technical blocker surfaces during research.
- Tier-ordering is fixed: T6 → clientDirectInput first within T7 → clientGraphics
  + DPVS second within T7 → clientAudio third within T7 → T8 → T9 →
  clientGame last.
- The `src/cmake/stubs/` directory is a new top-level CMake-only directory
  (parallel to `src/cmake/win32/`); it is NOT part of the engine source tree.

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 3-Client Engine Libraries (SDK-Heavy Tier)*
*Context gathered: 2026-05-03*
