# Phase 37: Utinni Engine Entry-Point Advertisement (GetEngineHookPoints) - Pattern Map

**Mapped:** 2026-06-21
**Files analyzed:** 6 (3 new source/header/inc + 1 build-graph edit + 1 expose-edit + 1 coverage-self-check, which co-lives in the new .cpp)
**Analogs found:** 5 / 6 (the PMF→void* helper is net-new — no in-tree analog; cite RESEARCH Code Examples)

> NOTE for the planner: the required_reading path `src/engine/client/application/Direct3d11/Direct3d11.cpp` is **wrong** — the real file is
> `src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp`, and the shipped `GetHookPoints` block is at **lines 856-888** (not 840-890). All excerpts below are from the correct path/lines.

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|-------------------|------|-----------|----------------|---------------|
| `src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp` (NEW) | export TU (provider/getter) | request-response (read-only getter) | `src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp` `GetHookPoints` block (856-888) | exact (twin export pattern) |
| `src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.h` (NEW) | config/contract header (structs + version) | n/a (declarations) | same `GetHookPoints` block — the `UtinniDx11HookPoints` struct + `extern "C" __declspec(dllexport)` decl (872-879) | exact (struct+export-decl shape) |
| `src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.inc` (NEW) | config (X-macro canonical name list) | n/a (data list) | none in-tree (X-macro `.inc` is net-new); RESEARCH §"Shared Header Decision" + spec §4 | role-match only |
| `src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj` (MODIFY) | build config | n/a | existing `<ItemGroup>` source list (453-477) | exact (same file, add 2 lines) |
| `src/game/client/application/SwgClient/src/win32/ClientMain.cpp` + `ClientMain.h` (MODIFY) | expose crash-fixer symbol | n/a | `installConfigFileOverride()` already at ClientMain.cpp:96-110 (namespaced) | exact (expose existing fn) |
| coverage self-check (`static_assert` + runtime null/dup scan) — lives **inside** `utinni_advertise.cpp` | test/validation (compile-time + boot self-check) | batch (table scan) | `Direct3d11_ConstantBuffer.h` static_assert constellation (47-249) | role-match (compile-time table validation) |

## Pattern Assignments

### `utinni_advertise.cpp` + `utinni_engine_hookpoints.h` (export TU + contract header, request-response)

**Analog:** `src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp:856-888` — the SHIPPED, dumpbin-confirmed-undecorated Utinni graphics hook-point export. This is the exe-side twin to mirror.

**Export-decl + struct pattern** (Direct3d11.cpp:872-888) — copy this exact shape (struct definition, then `extern "C" __declspec(dllexport) <ret> __cdecl Name();` decl, then the definition):
```cpp
struct UtinniDx11HookPoints
{
	IDXGISwapChain1 *     swapChain;   // null until create() completes; non-null + stable for session
	ID3D11Device *        device;
	ID3D11DeviceContext * context;
};

extern "C" __declspec(dllexport) UtinniDx11HookPoints __cdecl GetHookPoints();

UtinniDx11HookPoints GetHookPoints()
{
	UtinniDx11HookPoints hp = {};
	hp.swapChain = Direct3d11_Device::getSwapChain();
	hp.device    = Direct3d11_Device::getDevice();
	hp.context   = Direct3d11_Device::getContext();
	return hp;
}
```
Key facts proven by this analog: **`__declspec(dllexport)` on `extern "C" __cdecl` alone** forces the undecorated public name — NO `.def`, NO `/EXPORT` pragma, NO `ModuleDefinitionFile` (RESEARCH Finding 1, dumpbin-verified `GetHookPoints = _GetHookPoints`). The comment-block convention (purpose / read-only / inert-if-not-injected / pointer-borrowing) at Direct3d11.cpp:857-870 should also be mirrored.

**Exe-side mirror to author** (from RESEARCH Code Examples / spec §0 — the struct shape goes in the `.h`, the table + getter in the `.cpp`):
```cpp
// utinni_engine_hookpoints.h (NEW)
struct UtinniEngineHookPoint  { const char* name; void* addr; };
struct UtinniEngineHookPoints { unsigned version; unsigned count; const UtinniEngineHookPoint* entries; };
#define UTINNI_HOOKPOINTS_VERSION 1
// utinni_advertise.cpp (NEW)
extern "C" __declspec(dllexport) const UtinniEngineHookPoints* __cdecl GetEngineHookPoints();
const UtinniEngineHookPoints* GetEngineHookPoints() { return &s_table; }
```

**Static-method row pattern (the easy majority — Pattern 2 in RESEARCH):** `&Class::staticMethod` is a plain function pointer; cast to `void*` directly. Applies to all `config`/`game`/`graphics`/`cui::manager` rows (all `static`):
```cpp
{ "game::install",     (void*)&Game::install },       // static void install(Application) [Game.h:94]
{ "graphics::install", (void*)&Graphics::install },    // static bool install()           [Graphics.h:70]
// overloads need a disambiguating cast:
{ "graphics::present", (void*)static_cast<bool(*)()>(&Graphics::present) },
```

**Crash-fixer thunk row (EPA-02, highest priority):** advertise a `int(__cdecl)()` thunk wrapping the exposed `installConfigFileOverride()`, NOT `&ConfigFile::loadFromBuffer` (the spec best-guess is the inner call — see RESEARCH MVP table note):
```cpp
extern void installConfigFileOverride();                                  // exposed from ClientMain.cpp
static int __cdecl utinni_loadOverrideConfig() { installConfigFileOverride(); return 0; }
{ "config::loadOverrideConfig", (void*)&utinni_loadOverrideConfig },
```

**Includes pattern:** match how the engine includes its own libs by public header name (the SwgClient include path already has every `clientGame`/`sharedFoundation`/`sharedCommandParser`/etc. `include\public` dir and `..\..\src\shared` — vcxproj:122,199). So `#include "clientGame/Game.h"`, `#include "sharedCommandParser/CommandParser.h"`, `#include "utinni_engine_hookpoints.h"`. Header locations confirmed present: `Game.h` at `src/engine/client/library/clientGame/src/shared/core/Game.h`, `CommandParser.h` at `src/engine/shared/library/sharedCommandParser/src/shared/CommandParser.h`.

---

### `utinni_engine_hookpoints.inc` (X-macro canonical name list)

**Analog:** NONE in-tree (no `.inc` X-macro exists in this repo). Net-new. Cite spec §4 and RESEARCH §"Shared Header Decision".

**Pattern to author** (spec §4):
```cpp
// utinni_engine_hookpoints.inc — canonical name list, shared verbatim with D:/Code/Utinni
//   UTINNI_HOOKPOINT(group, name)
UTINNI_HOOKPOINT(config,   loadOverrideConfig)
UTINNI_HOOKPOINT(game,     install)
UTINNI_HOOKPOINT(graphics, install)
// … one line per §6 MVP endpoint …
```
**Decision (RESEARCH):** use the X-macro for the **name list + required-set only** (uniform, shareable verbatim with Utinni). The table **body** in `utinni_advertise.cpp` is hand-written because rows are heterogeneous (static fn / union PMF / ctor thunk / accessor) and cannot share one macro expansion. The coverage self-check bridges name-list ↔ body so they cannot drift. Place under `..\..\src\shared` (already on the SwgClient include path, vcxproj:122).

---

### PMF → void* union helper + ctor thunks (instance-method tier)

**Analog:** NONE — grep for an existing member-function-pointer / `union`+`reinterpret_cast<void*>` helper in `src/engine` returned nothing. Net-new. Cite RESEARCH Pattern 3 / Code Examples.

**Pattern to author** (RESEARCH:420-423) — use `std::bit_cast<void*>` (C++20, already enabled via `stdcpp20`), NOT a `union` type-pun (reading a non-active union member is ill-formed; `bit_cast` is the standard, well-defined idiom):
```cpp
#include <bit>   // std::bit_cast (C++20)
template <class PMF> inline void* pmfToVoid(PMF pmf) {
    // The size guard catches multiple/virtual-INHERITANCE PMF inflation (>4 bytes) ONLY.
    // It does NOT catch a &Class::virtualMethod — that is still pointer-sized but yields a
    // vtable-dispatch stub, not the impl. Virtual rows must be SKIPPED per the landmine rules.
    static_assert(sizeof(PMF) == sizeof(void*), "inflated PMF (multiple/virtual inheritance) — needs a thunk");
    return std::bit_cast<void*>(pmf);
}
// row: { "commandParser::addSubCommand", pmfToVoid(&CommandParser::addSubCommand) }  // [CommandParser.h:149]
```
**Per-row symbol-kind checklist (verify against the header BEFORE writing each row):**
`{ static | pmf-nonvirtual | thunk | accessor | skip-virtual | omit }`. Pick one per row; the row form follows from the kind. Never bulk `&fn`.

**Landmine flags (do NOT bulk `&fn`)** — per RESEARCH §"Pattern 3 CAVEAT" + Pitfall 1:
- **virtual** members (`GroundScene::draw`, `Object::addToWorld/removeFromWorld`; NOTE `getObjectType`/`move_p`/`move_o` are NON-virtual — see corrected symbol list) → `&fn` yields a vtable thunk, NOT the impl; the `static_assert` does NOT catch this. Skip (Utinni resolves off vtable) or instance-bound thunk.
- **constructors** (`GroundScene::GroundScene`, `CommandParser::CommandParser`, all `UI*::ctor`) → address cannot be taken; free-function thunk mandatory (RESEARCH Pattern 4). **Mandate `__thiscall` on the ctor thunk typedef** (32-bit MSVC member ctors are `__thiscall`) — or match the UtinniCore typedef exactly; a convention mismatch links clean and crashes at the first detour.
- **private/protected** members (`GroundScene::init/update/handleInputMap*`; `CommandParser::createDelegateCommands` is PROTECTED at CommandParser.h:180) → need `friend`/derived-subclass/in-class thunk, else SKIP+document.
- **inline** methods (`Game::getLoopCount` at Game.h:398; `Object::move_o` inline at Object.h:1216) → address may not be ODR-emitted; force emission via a non-inline shim, or OMIT.

**Calling-convention contract (per-row, every non-`__cdecl` row):** the advertised `void*` carries NO convention — Utinni's typedef supplies it and it MUST match MSVC's emitted convention or the first live detour crashes. Non-static member PMFs are `__thiscall` (this in ECX); `client::wndProc` is `__stdcall`. State the convention in a row comment whenever it is not `__cdecl`.

---

### `SwgClient.vcxproj` (build-graph wiring)

**Analog:** the existing source `<ItemGroup>` at **SwgClient.vcxproj:453-465** — this is exactly how the 3 current TUs are added.

**Current source list** (453-465) — add the new `.cpp` here, mirroring the `ClientMain.cpp` entry (NO per-config metadata needed for a plain TU; the `<Optimization Condition=…>MaxSpeed` on ClientMain/WinMain is optional):
```xml
  <ItemGroup>
    <ClCompile Include="..\..\src\win32\ClientMain.cpp">
      <Optimization Condition="'$(Configuration)|$(Platform)'=='Optimized|Win32'">MaxSpeed</Optimization>
    </ClCompile>
    <ClCompile Include="..\..\src\win32\FirstSwgClient.cpp"> ... </ClCompile>
    <ClCompile Include="..\..\src\win32\WinMain.cpp"> ... </ClCompile>
  </ItemGroup>
  <ItemGroup>
    <ClInclude Include="..\..\src\shared\FirstSwgClient.h" />
    ...
  </ItemGroup>
```
**To add:** `<ClCompile Include="..\..\src\win32\utinni_advertise.cpp" Condition="'$(Platform)'=='Win32'" />` into the ClCompile ItemGroup (the `Condition` keeps it Win32-only — an UNconditioned item would also compile/export from the existing x64 configs, breaching the 32-bit-only scope; the source ALSO carries an `#if !defined(_WIN64)` guard belt-and-suspenders), and `<ClInclude Include="..\..\src\shared\utinni_engine_hookpoints.h" />` (+ `.inc`) into the ClInclude ItemGroup.

**Build-graph facts the planner needs (verified this session):**
- NO `<ModuleDefinitionFile>` anywhere in the project — confirms dllexport-only is the export mechanism (matches the gl11 twin).
- Links under `/FORCE` (`<ForceFileOutput>Enabled</ForceFileOutput>` at lines 161 + 361) → a typo'd `&Engine::symbol` becomes a LINK *warning*, not error. **Gate: grep build log for `unresolved external symbol` = 0** after every build (RESEARCH Pitfall 5, AGENTS.md).
- 3 Win32 configs export-relevant: `Debug` (→ `_d.exe`), `Optimized` (→ `_o.exe`), `Release` (→ `_r.exe`). The new `.cpp` must be in all 3 (single `<ClCompile>` with no Condition covers all). x64 configs exist but are **out of scope** (32-bit only, spec §0).
- All game libs already in the exe link closure (`clientGame.lib`, `sharedObject.lib`, `clientUserInterface.lib`, `sharedCommandParser.lib`, … — vcxproj:145) → every `&Class::method` resolves; no new `AdditionalDependencies`.
- Include path already has `..\..\src\shared` and `..\..\src\win32` (vcxproj:122) → new header/inc resolvable with no path edit.
- `LanguageStandard=stdcpp20` (vcxproj:142) → `static_assert` with message + `if constexpr` available.

---

### Expose `installConfigFileOverride` (ClientMain.cpp / ClientMain.h)

**Analog:** the function already exists — `src/game/client/application/SwgClient/src/win32/ClientMain.cpp:96-110`:
```cpp
namespace ClientMainNamespace
{
	void installConfigFileOverride ()
	{
		AbstractFile * const abstractFile = TreeFile::open ("misc/override.cfg", AbstractFile::PriorityData, true);
		if (abstractFile)
		{
			int const length = abstractFile->length ();
			byte * const data = abstractFile->readEntireFileAndClose ();
			IGNORE_RETURN (ConfigFile::loadFromBuffer (reinterpret_cast<char const *> (data), length));
			delete [] data; delete abstractFile;
		}
	}
}
```
**To do:** it is currently inside `namespace ClientMainNamespace` (file-local via the anon-ish namespace usage). To make it linkable from `utinni_advertise.cpp`, give it external linkage — e.g. add a free `void installConfigFileOverride();` declaration (the existing `ClientMain.h` at line 13 only declares `ClientMain(...)`; add the extern decl there or in the new header) and ensure the definition has external linkage. This is the single most important row to get right (EPA-02 crash-fixer). Confirm with Utinni in discuss-phase whether the zero-arg orchestrator or the buffer loader is the hook target (RESEARCH Open Question 1 / Assumption A2).

---

### Coverage self-check (compile-time `static_assert` + runtime null/dup scan, EPA-04)

**Analog:** `src/engine/client/application/Direct3d11/src/win32/Direct3d11_ConstantBuffer.h:47-249` — the engine's established compile-time table-validation idiom (`static_assert(sizeof(...)==N, "msg")`, `offsetof` boundary asserts). Mirror this for the count gate.

**Compile-time gate** (mirror the Direct3d11_ConstantBuffer.h pattern). **CANONICAL FORM (pinned 2026-06-21, post-review): NO `{nullptr,nullptr}` sentinel row; `count = sizeof/sizeof`; the static_assert has NO `- 1`.** All three docs (37-01 / PATTERNS / RESEARCH) use this exact form:
```cpp
static_assert(sizeof(Direct3d11_PerFrameCB) == 96, "Direct3d11_PerFrameCB size mismatch");   // ← the idiom
// → for Phase 37 (NO sentinel — do NOT add a {nullptr,nullptr} terminator row):
static_assert((sizeof s_engineHookPoints / sizeof s_engineHookPoints[0]) == UTINNI_REQUIRED_COUNT,
              "hookpoint table row count != required-set count (drift vs .inc)");
```
**Name-set-equality gate (the real zero-missing check — the count assert above is only a cheap drift smoke):** emit a parallel `s_requiredNames[]` from the SAME X-macro (`#define UTINNI_HOOKPOINT(g,n) #g "::" #n`, then `#include` the `.inc`), and have the runtime `utinni_verifyNoNullNoDup()` ALSO assert the table's name set equals `s_requiredNames` (every required name present exactly once, no extras). Count-parity alone cannot prove zero-missing (a missing name + a typo'd/duplicate name passes a count check).
**Runtime self-check** (RESEARCH Validation §"What THIS repo can self-verify" #3): a `utinni_verifyNoNullNoDup()` that returns false / logs on any null `addr`, duplicate `name`, OR a name-set mismatch against the X-macro-generated `s_requiredNames[]` (the zero-missing check). Callable once in a `#if !defined(_PRODUCTION)` boot path — wire it into Debug boot before EPA-04 is declared green. No external test framework exists (AGENTS.md: builds/boots are truth) — this build-time+boot self-check IS the gate. Build it in 37-01 so 37-02/37-03 inherit it.

## Shared Patterns

### Undecorated exe export
**Source:** `src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp:846,879` (`GetApi`, `GetHookPoints`)
**Apply to:** `GetEngineHookPoints` in `utinni_advertise.cpp`
```cpp
extern "C" __declspec(dllexport) <ret> __cdecl <Name>();   // dllexport ALONE → undecorated; no .def / no /EXPORT
```
Verify per flavor: `dumpbin /exports stage/SwgClient_{r,d,o}.exe | grep GetEngineHookPoints` → `GetEngineHookPoints = _GetEngineHookPoints`.

### Self-documenting read-only-getter comment block
**Source:** Direct3d11.cpp:857-870
**Apply to:** the new export — document purpose, read-only, inert-if-not-injected, borrowed-pointer semantics, and the spec handoff reference (mirror the `// See .planning/handoff/...` line).

### Compile-time table validation
**Source:** `src/engine/client/application/Direct3d11/src/win32/Direct3d11_ConstantBuffer.h:47-249`
**Apply to:** the coverage gate in `utinni_advertise.cpp` (count `static_assert` against the `.inc` required-set).

### Engine-lib include by public header name
**Source:** SwgClient.vcxproj:122 (`include\public` dirs already wired) + ClientMain.cpp includes (`#include "swgClientUserInterface/SwgCuiManager.h"`, `#include "sharedGame/PlatformFeatureBits.h"`)
**Apply to:** `utinni_advertise.cpp` includes — use the same `lib/Header.h` form.

## No Analog Found

Files/patterns with no close in-tree match (planner should use RESEARCH patterns + spec, not a real template):

| Artifact | Role | Data Flow | Reason |
|----------|------|-----------|--------|
| `utinni_engine_hookpoints.inc` (X-macro name list) | config | data list | No `.inc` X-macro exists in this repo; net-new shared artifact (spec §4) |
| `pmfToVoid` union helper | utility | transform | No member-function-pointer→void* helper exists in `src/engine`; net-new (RESEARCH Pattern 3) |
| ctor thunks (`*::ctor` rows) | utility | transform | No ctor-address-thunk pattern in-tree; net-new per RESEARCH Pattern 4 (37-03 tier) |

## Metadata

**Analog search scope:** `src/engine/client/application/Direct3d11/src/win32/` (export twin + static_assert idiom), `src/game/client/application/SwgClient/` (exe project, source list, ClientMain), `src/engine/**` (PMF-helper search — none found)
**Files scanned:** Direct3d11.cpp, SwgClient.vcxproj, ClientMain.cpp/.h, Direct3d11_ConstantBuffer.h, Game.h + CommandParser.h (existence-confirmed), 37-RESEARCH.md, the spec handoff
**Pattern extraction date:** 2026-06-21
