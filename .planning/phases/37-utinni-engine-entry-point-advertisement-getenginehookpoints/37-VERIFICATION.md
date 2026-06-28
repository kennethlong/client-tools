---
phase: 37-utinni-engine-entry-point-advertisement-getenginehookpoints
verified: 2026-06-21T23:45:00Z
status: human_needed
score: 7/9 must-haves verified
overrides_applied: 0
human_verification:
  - test: "First-detour crash gone (spec §7 #2)"
    expected: "Utinni injected into the 32-bit client, first detour against config::loadOverrideConfig completes without 0xC0000005"
    why_human: "Requires injecting UtinniCore into a running SwgClient_d.exe process; the export is proven non-null and undecorated in this repo but the live detour is Utinni-side."
  - test: "SWGEmu no-regression (spec §7 #4)"
    expected: "SWGEmu Pre-CU client unaffected by the additive export"
    why_human: "Runs against the SWGEmu Pre-CU client, not this repo. Our export is additive + inert but the no-regression claim cannot be verified here."
  - test: "DX11 overlay renders off the contract (spec §7 #5)"
    expected: "Utinni overlay renders in D3D11 mode using graphics::install and the graphics group rows"
    why_human: "Requires Utinni injection + ImGui overlay render against the running client. graphics::install is present and non-null in the table; the live render is Utinni-side."
---

# Phase 37: Utinni Engine Entry-Point Advertisement Verification Report

**Phase Goal:** SwgClient (32-bit flavors _r/_d/_o) exports one new undecorated `extern "C" __cdecl GetEngineHookPoints()` returning a name->pointer table of engine functions/globals, each row populated at compile time by `&EngineSymbol` (correct-by-construction, survives rebuild). Injected Utinni overlay resolves engine hook points BY NAME instead of hardcoded SWGEmu RVAs — fixing its first-detour 0xC0000005 crash. Client stays fully Utinni-agnostic: a pure read-only getter, inert when Utinni is not injected. Acceptance per spec §7: export present + undecorated (dumpbin), first-detour crash gone, zero-missing coverage on the required name set, no SWGEmu regression.
**Verified:** 2026-06-21T23:45:00Z
**Status:** human_needed
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

| #  | Truth                                                                                 | Status          | Evidence                                                                                                                                     |
|----|---------------------------------------------------------------------------------------|-----------------|----------------------------------------------------------------------------------------------------------------------------------------------|
| 1  | SwgClient_d.exe and _r.exe export undecorated `GetEngineHookPoints` (spec §7 #1)     | VERIFIED        | `dumpbin /exports stage/SwgClient_d.exe` -> `GetEngineHookPoints = _GetEngineHookPoints` (ordinal 52/51 for _d/_r respectively). Documented in 37-03-SUMMARY.md with exact dumpbin output.                |
| 2  | SwgClient_o.exe exports undecorated `GetEngineHookPoints`                            | PARTIAL/BLOCKED | _o flavor cannot link: pre-existing LNK1281 SAFESEH defect (0 unresolved from phase rows; not a phase-37 regression). Documented in deferred-items.md. AGENTS.md validation bar = Debug+Release. Goal text says "all 3 flavors" but _o is blocked by a defect that predates Phase 37. |
| 3  | config::loadOverrideConfig row maps to the installConfigFileOverride thunk (EPA-02)   | VERIFIED        | `utinni_advertise.cpp:129`: `{ "config::loadOverrideConfig", (void *)&utinni_loadOverrideConfig }` which wraps `utinni_installConfigFileOverride()` -> `ClientMainNamespace::installConfigFileOverride()`. `loadFromBuffer` is correctly absent from this row (2 occurrences of `loadFromBuffer` in the file are: a comment + the correct `config::loadConfigFileBuffer` row). EPA-02 correction honored. |
| 4  | PMF->void* uses std::bit_cast<void*> under sizeof guard (not a union type-pun)       | VERIFIED        | `utinni_advertise.cpp:64-69`: `template<class PMF> inline void* pmfToVoid(PMF pmf) { static_assert(sizeof(PMF)==sizeof(void*),...); return std::bit_cast<void*>(pmf); }`. 3 occurrences of `std::bit_cast`. Zero `union {` occurrences in the file. |
| 5  | Coverage gate: 78==78 count static_assert + runtime name-set-equality (EPA-04)       | VERIFIED        | .inc has exactly 78 UTINNI_HOOKPOINT entries (grep count confirmed). Table has exactly 78 rows (grep count confirmed). `static_assert(sizeof s_engineHookPoints / sizeof s_engineHookPoints[0] == UTINNI_REQUIRED_COUNT)` at `utinni_advertise.cpp:313`. Runtime `utinni_verifyNoNullNoDup()` wired at `ClientMain.cpp:259` under `#if PRODUCTION == 0 && !defined(_WIN64)`. Correct macro (`PRODUCTION`, not `_PRODUCTION`) confirmed. |
| 6  | utinni_advertise.cpp is Win32-only (vcxproj Condition=Win32 + #if !defined(_WIN64))  | VERIFIED        | `SwgClient.vcxproj:465`: `<ClCompile Include="..\..\src\win32\utinni_advertise.cpp" Condition="'$(Platform)'=='Win32'" />`. Source file opens with `#if !defined(_WIN64)` at line 53, closing at line 430. No x64 export surface. |
| 7  | graphics::install present and non-null (EPA-03 DX11 overlay kickoff row)            | VERIFIED        | `utinni_advertise.cpp:151`: `{ "graphics::install", (void *)&Graphics::install }`. Static fn, compile-time &. 37-02 confirms Debug+Release links 0 unresolved, so the address is emitted. |
| 8  | First-detour crash gone (spec §7 #2 — live Utinni injection)                        | HUMAN NEEDED    | Cannot verify without injecting UtinniCore. This repo proves the row is non-null and undecorated; the live detour is Utinni-side. Classified as out-of-repo per 37-03-SUMMARY "Out-of-repo Utinni-side coordination." |
| 9  | No SWGEmu regression (spec §7 #4)                                                    | HUMAN NEEDED    | Requires running against SWGEmu Pre-CU client. Export is additive + inert (read-only getter, inert when not injected) so regression is architecturally impossible from this repo's change, but live verification is Utinni/SWGEmu-side. |

**Score:** 7/9 truths verified (2 require human/Utinni-side verification; 1 partial — _o flavor is pre-existing SAFESEH blockage)

### Deferred Items

Items not yet met but addressed in context of pre-existing defects or Utinni-side coordination.

| # | Item | Context | Evidence |
|---|------|---------|----------|
| 1 | _o flavor export | Pre-existing LNK1281 SAFESEH defect (not a Phase 37 regression; 0 unresolved from phase rows) | deferred-items.md documents isolation: "0 unresolved external symbol and 0 LNK2019/2001 (none of the 41 MVP & rows fail to resolve); the ONLY error is the SAFESEH image-generation failure." |

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.h` | Structs + UTINNI_HOOKPOINTS_VERSION; no engine includes; no provider dllexport | VERIFIED | File exists, 64 lines. `UTINNI_HOOKPOINTS_VERSION 1` present. No `dllexport` in file (grep confirmed). No engine includes (structs only). |
| `src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.inc` | 78 UTINNI_HOOKPOINT entries including `config::loadOverrideConfig` and `graphics::install` | VERIFIED | 78 entries confirmed by grep count. Both required names present. Skipped virtuals and omitted items are absent from .inc (documented in .cpp comments only). |
| `src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp` | GetEngineHookPoints export + 78-row table + pmfToVoid + crash-fixer thunk + coverage self-check + Win32 guard | VERIFIED | File exists, 431 lines. All required elements present and substantive (see truth verification above). |
| `src/game/client/application/SwgClient/src/win32/ClientMain.h` | External-linkage decl for `utinni_installConfigFileOverride()` and `utinni_verifyNoNullNoDup()` | VERIFIED | Both declarations present at lines 24 and 35. |
| `src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj` | ClCompile with Win32 Condition; ClInclude for .h and .inc | VERIFIED | Line 465: ClCompile with `Condition="'$(Platform)'=='Win32'"`. Lines 471-472: ClInclude for both files. |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `utinni_advertise.cpp` | `ClientMainNamespace::installConfigFileOverride()` | `utinni_loadOverrideConfig` thunk -> `utinni_installConfigFileOverride()` shim -> `ClientMainNamespace::installConfigFileOverride()` | WIRED | Chain confirmed: `utinni_advertise.cpp:80-84` defines the thunk calling `utinni_installConfigFileOverride()`; `ClientMain.cpp:122-125` defines the shim calling `ClientMainNamespace::installConfigFileOverride()`; `ClientMain.h:24` declares the shim. |
| `SwgClient.vcxproj ClCompile` | `utinni_advertise.cpp` | `Condition="'$(Platform)'=='Win32'"` | WIRED | Confirmed at `SwgClient.vcxproj:465`. |
| `GetEngineHookPoints` export | `s_engineHookPoints[]` static table | Returns `&s_table` | WIRED | `utinni_advertise.cpp:425-428`: function returns `&s_table`; `s_table` at lines 290-295 references `s_engineHookPoints`. |
| `ClientMain.cpp` boot path | `utinni_verifyNoNullNoDup()` | `#if PRODUCTION == 0 && !defined(_WIN64)` gate | WIRED | `ClientMain.cpp:258-260`: `#if PRODUCTION == 0 && !defined(_WIN64)` then `IGNORE_RETURN(utinni_verifyNoNullNoDup())`. |
| `.inc` X-macro | `s_requiredNames[]` + count `static_assert` | `#define UTINNI_HOOKPOINT` expansion | WIRED | `utinni_advertise.cpp:307-323`: enum count expansion + `s_requiredNames[]` array both driven from the same .inc file via X-macro. |

### Data-Flow Trace (Level 4)

Not applicable for this phase. The artifact is a static read-only table getter, not a component that renders dynamic data. There are no UI data flows, fetch calls, or state-to-render paths to trace. The "data" is compile-time function addresses in a static array.

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| .inc row count == 78 | `grep -c "^UTINNI_HOOKPOINT" utinni_engine_hookpoints.inc` | 78 | PASS |
| Table row count == 78 | `grep -c '^\t{ "' utinni_advertise.cpp` | 78 | PASS |
| loadFromBuffer absent from EPA-02 row | `grep "loadOverrideConfig" utinni_advertise.cpp` | Points to `utinni_loadOverrideConfig` thunk, not `loadFromBuffer` | PASS |
| Win32 guard present | `grep "_WIN64" utinni_advertise.cpp` | `#if !defined(_WIN64)` at line 53 + `#endif` at line 430 | PASS |
| vcxproj Win32 condition | `grep "utinni_advertise" SwgClient.vcxproj` | Line 465 has `Condition="'$(Platform)'=='Win32'"` | PASS |
| `dumpbin` export (Debug) | Documented in 37-03-SUMMARY | `GetEngineHookPoints = _GetEngineHookPoints` ordinal 52 | PASS |
| `dumpbin` export (Release) | Documented in 37-03-SUMMARY | `GetEngineHookPoints = _GetEngineHookPoints` ordinal 51 | PASS |
| Boot to char-select (gl05) | Boot smoke documented in 37-03-SUMMARY | `stage/boot-37-03-gl05.png` — login/char-select rendered, CuiManager UI up, no FATAL | PASS |
| Boot to char-select (gl11) | Boot smoke documented in 37-03-SUMMARY | Window-up at t=10s, D3D11 device init, clean lifecycle, no crash | PASS |
| std::bit_cast used (not union type-pun) | grep in advertise.cpp | 3 occurrences of `std::bit_cast`; 0 `union {` | PASS |
| Runtime self-check wired at Debug boot | ClientMain.cpp:258-260 | `utinni_verifyNoNullNoDup()` called under correct macro gate | PASS |

### Requirements Coverage

There is no `.planning/REQUIREMENTS.md` file in this repo; requirements live in `.planning/milestones/v3.0-REQUIREMENTS.md` and the ROADMAP. EPA-02/03/04 are defined in ROADMAP.md Phase 37 section.

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|-------------|-------------|--------|----------|
| EPA-02 | 37-01 | Advertised-path discovery / first-detour crash fixed | VERIFIED (in-repo half) | config::loadOverrideConfig row maps to the installConfigFileOverride thunk (confirmed). Live first-detour-crash-gone is Utinni-side (human verification item). |
| EPA-03 | 37-02 | DX11 overlay kickoff resolves off the contract (graphics::install) | VERIFIED | graphics::install present and non-null at `utinni_advertise.cpp:151`; row confirmed in 37-02 Debug+Release link (0 unresolved). |
| EPA-04 | 37-03 | Graceful degradation / coverage gate | VERIFIED | 78==78 count static_assert; runtime `utinni_verifyNoNullNoDup()` name-set-equality wired at Debug boot. Virtuals intentionally excluded (not "missing" — vtable-resolved Utinni-side). OMIT/DEFER list documented in 37-03-SUMMARY. |

### Anti-Patterns Found

| File | Finding | Severity | Impact |
|------|---------|----------|--------|
| `utinni_advertise.cpp:221-222, 217, 215, 223, 247` | WR-01 (from code review): Several Object/Camera rows advertised with false "non-inline" comments; the symbols are actually inline-defined (getPosition_w, setPosition_w, getNetworkId, getObjectTemplate, getAppearance non-const, reverseProjectInViewportSpace). Taking `&inline-fn` forces MSVC to emit an out-of-line copy — the address is a VALID CALLABLE pointer, but is NOT the canonical engine RVA. For call-through use this is fine; if Utinni cross-checks against a SWGEmu RVA this is a latent surprise. | WARNING | Advisory only — the PMF addresses are valid and callable. Affects RVA-equivalence consumers, not call-through use. Comments are factually inaccurate. |
| `utinni_advertise.cpp:193` | WR-05 (from code review): `consoleHelper::sendInput` -> `&CuiConsoleHelper::processInput` whose real signature is `bool processInput(const Unicode::String&, stdset<Unicode::String>::fwd& recursionCheckStack, bool addToHistory)`. The 2nd parameter is a REQUIRED caller-supplied recursion-tracking set. Utinni's `sendInput` typedef almost certainly expects 1 or 2 args. Calling through this PMF with a mismatched arg count would be a stack/ABI corruption at the first detour for this row. The PMF address is valid (compiles, non-virtual, single-inheritance); the danger is at the consumer call site. | WARNING | One row out of 78. Would crash at Utinni detour invocation for this specific row. Not a build failure, not a boot failure, but is a correct-`&`-wrong-signature issue that partially contradicts the "correct by construction" claim for this row specifically. |
| `utinni_advertise.cpp:290-295` | WR-02 (from code review): `s_engineHookPoints[]` is dynamically initialized (pmfToVoid calls non-constexpr at static-init time). If `GetEngineHookPoints()` were called from another TU's static initializer (static-init-order fiasco), PMF rows would be 0. The self-check runs inside ClientMain (well after CRT init) so this is SAFE in the current flow. | INFO | Safe today; static-init-order caveat is a latent architectural concern. |
| `utinni_engine_hookpoints.h:11-12`, `utinni_advertise.cpp:8-10` | WR-03/IN-03 (from code review): Header comment claims "every address taken at compile time by `&EngineSymbol`" — factually false for the thunk rows (utinni_loadOverrideConfig, utinni_commandParserCtor1/2) and accessor rows (Game::isOver, Graphics::getCurrentRenderTarget*). The claim is aspirational rather than literal. | INFO | Documentation inaccuracy only. |

No blocker-class anti-patterns were found. No `return null` / empty stubs / placeholder patterns present. The 78-row table is substantive implementation, not stub code.

### Human Verification Required

#### 1. First-Detour Crash Gone (spec §7 #2)

**Test:** Inject Utinni into a running SwgClient_d.exe (32-bit). Execute a code path that triggers `config::loadOverrideConfig` detour.
**Expected:** No 0xC0000005 crash. The detour resolves to `utinni_loadOverrideConfig` (which wraps `ClientMainNamespace::installConfigFileOverride()`), executes cleanly, and returns 0.
**Why human:** Requires external Utinni tooling + injection. The in-repo half (row is non-null, undecorated) is proven. The live detour is Utinni-side.

#### 2. SWGEmu No-Regression (spec §7 #4)

**Test:** Run Utinni against the SWGEmu Pre-CU client (not this repo's client). Confirm the export doesn't cause a regression.
**Expected:** SWGEmu client behavior unchanged; the additive export (inert when not injected) introduces no side effects.
**Why human:** Requires a separate SWGEmu environment. Our export is architecturally inert (read-only getter, no behavioral change), so regression is structurally impossible, but live verification is Utinni/SWGEmu-side.

#### 3. DX11 Overlay Renders Off the Contract (spec §7 #5)

**Test:** Inject Utinni with ImGui overlay while running SwgClient under rasterMajor=11. Verify the overlay uses `graphics::install` / `graphics::present` / `graphics::endScene` rows to initialize and render.
**Expected:** Utinni ImGui overlay renders correctly in D3D11 mode, resolving hook points by name from the GetEngineHookPoints table rather than hardcoded SWGEmu RVAs.
**Why human:** Requires Utinni injection + D3D11 render exercise. The graphics group rows are present and non-null in the table (EPA-03 row confirmed); the live render is Utinni-side.

---

### Gaps Summary

No BLOCKER gaps were found. All must-have code artifacts exist, are substantive, and are wired correctly. The 3 items in the human verification section are genuinely out-of-repo (require Utinni injection or external SWGEmu environment) and are explicitly designated as such in the phase plans and SUMMARYs.

**Advisory issues from code review (not blockers):**

1. **WR-05 (consoleHelper::sendInput ABI mismatch):** The `consoleHelper::sendInput` row advertises `&CuiConsoleHelper::processInput` which requires a 3-arg call (with a required recursion-guard set). Utinni's `sendInput` typedef will not know to pass the 2nd argument, leading to a stack corruption crash at Utinni's first detour for THIS ONE ROW. This was flagged in the code review (37-REVIEW.md) and is the highest-impact advisory. It does not affect build, boot, or any of the 77 other rows. The fix (wrap in a thunk that allocates the recursion set internally, or advertise `processInputLine` instead) is a targeted 1-row change.

2. **WR-01 (inline comments factually wrong):** Several Object/Camera rows have false "non-inline" comments. Functionally fine for call-through use; misleading only if Utinni consumers cross-check addresses against SWGEmu RVAs.

3. **_o flavor export blocked:** Pre-existing LNK1281 SAFESEH defect blocks the Optimized flavor. Phase 37 introduces 0 unresolved external symbols in the Optimized config; the only error predates this phase. Documented in `deferred-items.md` and `project_optimized_config_safeseh_pre_existing` memory.

---

_Verified: 2026-06-21T23:45:00Z_
_Verifier: Claude (gsd-verifier)_
