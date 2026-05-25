# Phase 4: SwgClient Executable Link — Context

**Gathered:** 2026-05-04
**Status:** Ready for planning

<domain>
## Phase Boundary

Author CMakeLists for the two remaining game-side libs (`swgClientUserInterface` — 266
cpp, `swgSharedNetworkMessages` — 10 cpp), then write the final
`add_executable(SwgClient WIN32 ...)` target with ~70 `target_link_libraries` entries,
config-correct binary naming (`SwgClient_d.exe` Debug / `SwgClient_r.exe` Release),
and post-build staging of 36 runtime DLLs + `mozilla/` chrome subtree + a placeholder
`client.cfg`. Phase ends when `cmake --build build --config Debug` produces
`bin/Debug/SwgClient_d.exe` from a clean clone (ARTIF-01 pass/fail gate), and
`cmake --build build --config Release` produces `bin/Release/SwgClient_r.exe`.

**Phase 4 scope (confirmed from vcproj analysis):**
- Author `game/client/library/swgClientUserInterface/CMakeLists.txt` (266 cpp files)
- Author `game/shared/library/swgSharedNetworkMessages/CMakeLists.txt` (10 cpp files;
  explicitly excluded from Phase 2, excluded from Phase 3)
- Wire both into their parent `game/*/library/CMakeLists.txt` subtree includes
- Author `game/client/application/SwgClient/CMakeLists.txt` +
  `game/client/application/SwgClient/src/CMakeLists.txt`
  — `add_executable(SwgClient WIN32 WinMain.cpp ClientMain.cpp FirstSwgClient.cpp
    SwgClient.rc)` with full `target_link_libraries` (~70 entries) and post-build staging

**NOT in Phase 4 scope (confirmed):**
- `swgClientAutomation` (3 cpp) — not referenced in SwgClient.vcproj; skip
- `swgClientQtWidgets` (2 cpp) — Qt widget tool lib, not referenced in SwgClient.vcproj; skip
- Any server-side libraries (server work is out of v1 scope)
- Full `client.cfg` with asset search paths (Phase 6 owns this)
- Boot sequence debugging / login handshake (Phase 6)

</domain>

<decisions>
## Implementation Decisions

### Link Line Exclusions

- **D-01:** Rely on `CMAKE_MSVC_RUNTIME_LIBRARY` (CMP0091 NEW) for CRT library
  exclusions. The global setting `MultiThreaded$<$<CONFIG:Debug>:Debug>` already
  emits the correct `/NODEFAULTLIB` flags (`/NODEFAULTLIB:MSVCRT` etc.). No explicit
  `/NODEFAULTLIB` linker options needed on SwgClient.

- **D-02:** Omit `nspr4.lib`, `plc4.lib`, `profdirserviceprovider_s.lib`, `xpcom.lib`,
  `xul.lib` entirely from `target_link_libraries`. Phase 3 D-07 confirmed via
  `dumpbin /symbols` that `clientUserInterface.lib` has zero XPCOM symbols; the stub
  generates no references so there is nothing to link.

### Post-build DLL Staging

- **D-03:** Stage runtime DLLs and `mozilla/` chrome subtree **alongside the exe** in
  CMake's `RUNTIME_OUTPUT_DIRECTORY` — i.e., `bin/Debug/` for the Debug exe and
  `bin/Release/` for the Release exe. Standard game-engine pattern; enables double-click
  launch without PATH manipulation.

- **D-04:** Use `add_custom_command(TARGET SwgClient POST_BUILD COMMAND cmake -E copy
  ... COMMAND cmake -E copy_directory ...)` for the staging. Fires automatically on
  every successful link. Do NOT use `cmake install()` rules — those require a manual
  `cmake --install` invocation and do not fire automatically.

- **D-05:** Drop a **minimal placeholder `client.cfg`** in Phase 4, staged alongside
  the exe via the same POST_BUILD command. Content: commented-out `[SharedFile]
  searchPath_00=` stubs so the exe can at least be launched without a fatal config
  error before Phase 6 wires the real TRE paths. Phase 6 owns the full runtime config
  (LAUNCH-01, CONFIG-01/02/03).

### STLPort Exe Linkage

- **D-06:** Link STLPort static runtime lib **eagerly from the start** — do not wait for
  LNK errors to appear. Add both variants to `target_link_libraries(SwgClient PRIVATE ...)`.
  Pre-empts LNK2019/LNK2005 storms rather than reacting to them.

- **D-07:** Express via CMake generator expression in direct `target_link_libraries` on
  SwgClient — no new INTERFACE target:
  ```cmake
  target_link_libraries(SwgClient PRIVATE
    $<$<CONFIG:Debug>:stlportstlx_vc71_debug.lib>
    $<$<CONFIG:Release>:stlportstlx_vc71.lib>
  )
  ```
  STLPort lib dir (`src/external/3rd/library/stlport453/lib/win32/`) must be added to
  `target_link_directories` or expressed via full path.

- **D-08:** If vc71 prebuilt causes unresolvable LNK storms that cannot be fixed without
  source edits, the named fallback is the **TheSuperHackers/stlport-4.5.3 rebuild**
  (locked Phase 1 decision in `PROJECT.md` Key Decisions table). This escalation stays
  within Phase 4 — do not create a Phase 4.x insertion unless source edits are required.

### Carried Forward from Phase 3 (D-09)

- **D-09 (locked):** Phase 4 is **strictly** the executable link step. No library
  CMakeLists authoring beyond `swgClientUserInterface` and `swgSharedNetworkMessages`.
  `clientGame` (Phase 3's integrator) is verified linkable here but its CMakeLists is
  already authored.

### Cross-cutting Constraints (from Phase 1–3)

- PCH: `FirstSwgClient.h` via `target_precompile_headers(SwgClient PRIVATE
  src/shared/FirstSwgClient.h)` (D-14 pattern).
- CRT: `CMAKE_MSVC_RUNTIME_LIBRARY` is global — SwgClient must NOT override it.
- No source edits: INV-01 hard constraint; if a link error requires a source change,
  escalate as a planning flag and update PROJECT.md.
- `/wd4530` (C4530 exception-handling unwind): apply on SwgClient target as well
  (consistent with all Phase 3 client targets, D-12).

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase Goals and Success Criteria
- `.planning/ROADMAP.md` §Phase 4 — authoritative success criteria (6 items), binary
  naming requirements (`SwgClient_d.exe` / `SwgClient_r.exe`), DLL staging list, and
  three-consecutive-build verification requirement
- `.planning/REQUIREMENTS.md` §BUILD-04, §ARTIF-01, §ARTIF-02, §ARTIF-03 — the four
  requirements owned by Phase 4; ARTIF-01 is the v1 pass/fail gate

### Project-level Decisions
- `.planning/PROJECT.md` §Key Decisions — locked decisions from Phase 1 (STLPort
  fallback strategy, DX9 vendored path, Mozilla XPCOM stub) that carry forward
- `.planning/PROJECT.md` §Constraints — `/MT[d]` CRT, no source edits, STLPort
  retained policies

### Phase 3 Outputs (verify these exist before planning Phase 4)
- `src/engine/client/library/clientGame/CMakeLists.txt` — integrator lib authored in
  Phase 3 (D-08/D-09); Phase 4 links against it but does not modify it
- `src/engine/client/library/clientUserInterface/CMakeLists.txt` — XPCOM stub gate
  verified (D-07); Phase 4 must NOT add XPCOM libs to the exe link
- `src/cmake/stubs/libMozilla_stub.cpp` — Phase 3 stub file; confirms XPCOM is
  stubbed at the object level

### Build Chain Research
- `docs/research/swgclient-build.md` — complete ~70-project dependency graph, per-config
  `libraries.rsp` contents, include path inventory, compiler settings (TreatWChar_tAsBuiltInType,
  CharacterSet, RuntimeTypeInfo, Detect64BitPortabilityProblems), and linker settings;
  **authoritative for `target_link_libraries` entry list and DLL staging inventory**
- `docs/recon/05-client-boot-sequence.md` — 17-phase boot sequence; Phase 4 produces
  the binary but Phase 6 boots it — useful context for DLL staging priorities

### SDK Locations for Phase 4 Link
- `src/external/3rd/library/stlport453/lib/win32/` — STLPort vc71 prebuilt `.lib`
  variants (stlportstlx_vc71.lib, stlportstlx_vc71_debug.lib)
- `src/external/3rd/library/dpvs/lib/win32-x86/` — DPVS prebuilt (dpvs.lib /
  dpvsd.lib); carry D-01/D-02 forward: no DPVS source build unless LNK escalation
- `src/external/3rd/library/vivox/lib/` — vivoxSharedWrapper_Debug.lib / _Release.lib
- `src/external/3rd/library/videocapture/` — VideoCapture, ImageCapture, Smart,
  SoeUtil, ZlibUtil, PICTools per-config libs (debug/release variants)
- `src/external/3rd/library/lcdui/lib/lgLcd.lib` — Logitech LCD (always linked)
- `src/external/3rd/library/pcre/4.1/win32/lib/libpcre.a` — PCRE (always linked)
- `src/external/3rd/library/libxml2-2.6.7.win32/lib/` — libxml2 debug/release variants

### swg-main Reference
- `C:\Code\swg-main` — server-only CMake reference; check how they express STLPort
  linkage at application exe level (closest structural analogy to Phase 4)

</canonical_refs>

<code_context>
## Existing Code Insights

### SwgClient Source Files (three .cpp + one .rc)
- `src/game/client/application/SwgClient/src/win32/WinMain.cpp` — Win32 entry shim
  (135 lines); calls `SetUserSelectedMemoryManagerTarget()` then `ClientMain()`
- `src/game/client/application/SwgClient/src/win32/ClientMain.cpp` — boot order
  (405 lines); installs all 22+ engine subsystems in order; calls
  `libMozilla::init(Os::getWindow(), sPath.c_str())` — stub returns true (D-04)
- `src/game/client/application/SwgClient/src/win32/FirstSwgClient.cpp` — PCH source
- `src/game/client/application/SwgClient/src/win32/SwgClient.rc` — version info, icons;
  CMake handles `.rc` compilation automatically when listed in `add_executable` sources
- `src/game/client/application/SwgClient/src/shared/FirstSwgClient.h` — PCH umbrella;
  includes `sharedFoundation/FirstSharedFoundation.h` and `_precompile.h`

### New Libs to Author (Phase 4)
- `src/game/client/library/swgClientUserInterface/` — 266 .cpp files;
  `src/win32/` subdirectory layout; follows same three-level nesting pattern as
  Phase 2/3 libs (`<lib>/CMakeLists.txt` → `<lib>/src/CMakeLists.txt`)
- `src/game/shared/library/swgSharedNetworkMessages/` — 10 .cpp files;
  `src/shared/` subdirectory (combat/, consent/, core/, permissionList/);
  also follows three-level pattern

### Windows Subsystem Flags
- `add_executable(SwgClient WIN32 ...)` sets `SubSystem=WINDOWS` automatically —
  no manual `/SUBSYSTEM:WINDOWS` needed
- `CharacterSet="2"` (MBCS) → `target_compile_definitions(SwgClient PRIVATE _MBCS)` —
  already inherited from global `add_compile_definitions` in root CMakeLists
- `RuntimeTypeInfo="TRUE"` (RTTI) → `/GR` is on by default in MSVC/CMake; verify
  no `/GR-` is set on any transitive target
- `TreatWChar_tAsBuiltInType="FALSE"` → `/Zc:wchar_t-` already set globally (Phase 1)

### Established Patterns (from Phases 1–3 to continue)
- Three-level nesting: `<lib>/CMakeLists.txt` → `<lib>/src/CMakeLists.txt`
- `target_precompile_headers(${LIB} PRIVATE First${LibName}.h)` on every target
- Global compile definitions (`_USE_32BIT_TIME_T=1`, `_MBCS`, `PRODUCTION=0`,
  `DEBUG_LEVEL=2`) inherited from root — do NOT re-add on individual targets
- `CMAKE_MSVC_RUNTIME_LIBRARY` set globally — individual targets must NOT override

### Integration Points
- Phase 4 exe links against the full output of Phases 1–3: all 22 shared engine libs,
  all 13 client engine libs, 6 ours/ libs, udplibrary, plus swgClientUserInterface and
  swgSharedNetworkMessages authored here
- `add_dependencies` or transitive `target_link_libraries` ensures build order
- Staged DLLs at `bin/Debug/` and `bin/Release/` are the Phase 6 launch target

</code_context>

<specifics>
## Specific Requirements

- Binary naming must match ROADMAP.md SC-3/SC-4:
  `set_target_properties(SwgClient PROPERTIES OUTPUT_NAME_DEBUG SwgClient_d OUTPUT_NAME_RELEASE SwgClient_r)`
- ARTIF-03 cross-check: both binaries' `dumpbin /imports` must show zero `MSVCR*.dll`
  / `VCRUNTIME*.dll` / `MSVCRTD.dll` references (static CRT) and zero `xpcom.dll` /
  `xul.dll` references (Mozilla stubbed)
- SC-6: three consecutive clean parallel builds must succeed deterministically

</specifics>

<deferred>
## Deferred Ideas

- `swgClientAutomation` CMakeLists — not needed for SwgClient.exe; deferred to a
  future tools-build phase if ever
- `swgClientQtWidgets` CMakeLists — Qt widget tool lib, not in SwgClient.vcproj
  dependencies; deferred
- Full `client.cfg` with real `searchPath_NN` entries pointing at retail TRE install
  — Phase 6 scope (LAUNCH-01)
- `/WX` warnings-as-errors re-enable — deferred to post-v1 per FLAGS-02

</deferred>

---

*Phase: 4-SwgClient Executable Link*
*Context gathered: 2026-05-04*
