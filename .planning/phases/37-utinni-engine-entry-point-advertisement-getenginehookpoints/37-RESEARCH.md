# Phase 37: Utinni Engine Entry-Point Advertisement (GetEngineHookPoints) - Research

**Researched:** 2026-06-21
**Domain:** MSVC v145 32-bit linker exports, pointer-to-member-function extraction, engine-symbol resolution
**Confidence:** HIGH (export mechanics + MVP symbol resolution verified against source); MEDIUM (full-set 37-03 method); the PMF/virtual/ctor landmines are HIGH-confidence blockers verified against actual headers.

## Summary

This phase adds one undecorated `extern "C" __cdecl GetEngineHookPoints()` export to the 32-bit `SwgClient` exe that returns a name→pointer table of engine functions/globals, each row populated at compile time by `&EngineSymbol`. It is the exe-side twin of the already-shipped `gl11_r.dll!GetHookPoints` (Direct3d11.cpp:879). The spike (37-01) is de-risked by two HIGH-confidence findings below; the catalog work (37-02/37-03) is gated by a third.

**Finding 1 (export mechanics — SOLVED):** The shipped `gl11_r.dll!GetHookPoints` is forced undecorated by **nothing more than `__declspec(dllexport)` on an `extern "C" __cdecl` function** — no `.def`, no `/EXPORT` pragma. `dumpbin /exports stage/gl11_r.dll` proves it: `GetApi = _GetApi`, `GetHookPoints = _GetHookPoints` (the public name is undecorated; `_Get…` is the internal symbol). The same mechanism works in an EXE. So the SwgClient.vcxproj needs **no .def, no ModuleDefinitionFile, no /EXPORT** — just the same source pattern. (A `#pragma comment(linker, "/EXPORT:GetEngineHookPoints")` is the belt-and-suspenders fallback only if dumpbin shows decoration, which it will not for `extern "C" __cdecl`.)

**Finding 2 (the SWG engine is static-method-heavy — the PMF problem is SMALL):** The crash-fixer `config::loadOverrideConfig` and the entire MVP `config`/`game`/`graphics`/`cui` groups are **`static` class methods** (`ConfigFile::loadFromBuffer`, `Game::install`, `Graphics::install`, `CuiManager::render`, …). `&ClassName::staticMethod` is an **ordinary function pointer** that casts to `void*` cleanly — NO pointer-to-member-function machinery needed for ~60% of the catalog. The PMF helper is only needed for true non-static members (the `scene::groundScene::*`, `object::*`, `worldSnapshot::*`, `commandParser::*` instance methods).

**Finding 3 (the real landmines, all in 37-03's instance-method tier):** (a) **Constructors cannot have their address taken in C++** — `&GroundScene::GroundScene`, `&CommandParser::CommandParser`, every `*::ctor` row is a hard compile error and needs a thunk wrapper. (b) **`&Class::virtualMethod` under MSVC yields a vtable-dispatch thunk, NOT the real code address** — `Object::addToWorld/removeFromWorld/getType`, `GroundScene::draw/processEvent` are `virtual`; a `&fn` pointer for these will NOT be the function Utinni wants to detour. (c) **Private members** (`GroundScene::init/update/handleInputMap*`) can't be addressed from outside the class — needs `friend` or an in-class thunk. (d) **Overloaded methods** (`Graphics::present`, `Game::getCamera`, `Game::setScene`) need an explicit cast to disambiguate `&fn`.

**Primary recommendation:** Land 37-01 as a pure mirror of the gl11 pattern (`__declspec(dllexport)` only, no def/pragma), proving 5 rows: a static fn (`Game::install`), a global (`&Game::ms_loops` or a real exported global), the crash-fixer (`config::loadOverrideConfig` — see signature-mismatch caveat below), one non-virtual non-static PMF (`CommandParser::addSubCommand` via the union helper), and a thunk for one ctor. Build the catalog static-first (free win), and treat ctors/virtuals/privates as a per-row thunk-wrapper decision in 37-03, not a bulk `&fn`.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Export the name→pointer table | SwgClient.exe (game-logic tier) | — | Game symbols (Game, Object, CuiManager, GroundScene) are statically linked INTO the exe, not a plugin DLL — confirmed by SwgClient.vcxproj AdditionalDependencies (clientGame.lib, sharedObject.lib, clientUserInterface.lib all in the exe link closure) |
| Graphics hook points | gl11_r.dll (renderer plugin) | — | Already shipped as `GetHookPoints` (Direct3d11.cpp:879); the exe's `graphics::*` rows advertise the engine-facade `Graphics::*` statics, distinct from the DLL's DXGI pointers |
| Detour installation / overlay render | Utinni (external, out of scope) | — | Client stays Utinni-agnostic; every row is just `{name, address}` |
| Symbol→address binding | MSVC v145 linker (compile time) | — | `&fn` is linker-resolved; correct-by-construction across rebuilds |

## User Constraints

No CONTEXT.md exists for this phase (research spawned ahead of /gsd-discuss-phase). Constraints below are extracted from the canonical spec, ROADMAP, and AGENTS.md and must be honored by the planner with the same authority as locked decisions.

### Locked (from spec §0/§5 + ROADMAP Goal)
- **32-bit only.** x64 is Utinni's deferred half (user-locked). Build for Win32 platform → `stage/`, NOT `stage-x64/`.
- Export name is exactly `GetEngineHookPoints`, `extern "C" __cdecl`, returning `const UtinniEngineHookPoints*`.
- Export from **every** 32-bit flavor: `_r` (Release), `_d` (Debug), `_o` (Optimized).
- The client stays **Utinni-agnostic**: pure read-only getter, no Utinni dependency, inert when not injected.
- Mirror the shipped graphics pattern at Direct3d11.cpp:856-888.
- **A wrong `&` is worse than a missing row** (spec §0): a missing row degrades gracefully; a wrong row detours the wrong function. Mark every unconfirmed mapping.

### Claude's Discretion (from spec §4/§8)
- X-macro shared header vs hand-maintained header (spec §4: "if an X-macro is awkward in your build, a hand-maintained header is acceptable — the coverage test is what guarantees zero drift").
- Flat versioned struct vs name→pointer table shape (spec §8 #4: table recommended for graceful degradation, but flat struct "acceptable — coordinate with Utinni before diverging").
- How globals are advertised when behind a file-static accessor (spec §8 #3: advertise the accessor instead).

### Out of Scope (spec §6 "do NOT advertise" + §8 #1)
- D3D9 device vtable hooks (Utinni harvests off live vtable).
- `D3DXCompileShader` (resolved in the D3DX DLL, not the exe).
- `IDirectInput8A` vtable writes (Utinni patches the COM instance).
- Mid-function JMP/NOP/byte patches (CrashLog inline hook, ChatWindow mid-hook, `shader::popCell`, chat-context NOPs) — cannot be `&fn`; deferred per §8 #1.
- Virtual/vtable endpoints like `cui::io::processEvent` — Utinni resolves off the live vtable (spec §6 marks it "from vtable").

## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| EPA-02 | Advertised-path discovery / first-detour crash fixed | The `config::loadOverrideConfig` row resolves the `0xC0000005 target=0x00401000` crash. Self-verifiable on our side: export present (dumpbin) + non-null pointer in table for that name. Live first-detour-gone verification requires Utinni injection (out-of-repo). |
| EPA-03 | DX11 overlay kickoff off the contract | `graphics::install` etc. rows + the shipped gl11 `GetHookPoints`. Self-verifiable: rows present + non-null. Live overlay render requires Utinni. |
| EPA-04 | Graceful degradation / coverage gate | Coverage test asserting zero-missing + no-duplicate on the required name set (spec §7 #3). Fully self-verifiable in this repo (build-time or runtime self-check against the shared `.inc`). |

## Standard Stack

This is pure in-tree C++ against the existing MSBuild graph — no new libraries.

### Core
| Tool | Version | Purpose | Why Standard |
|------|---------|---------|--------------|
| MSBuild (VS18 BuildTools) | v145 toolset | Build SwgClient + plugins | `$env:MSBUILD` per AGENTS.md; v145 required (v143/VS2022 fails MSB8020) [VERIFIED: SwgClient.vcxproj PlatformToolset=v145] |
| `dumpbin /exports` | any (16.x+ fine) | Verify export presence + undecorated name | Ships with VC Tools; the §7 #1 acceptance probe [VERIFIED: dumpbin run this session] |
| C++20 (`stdcpp20`) | — | Source language level | SwgClient.vcxproj LanguageStandard=stdcpp20; enables `if constexpr`, `static_assert` with message [VERIFIED: SwgClient.vcxproj:142] |

### Supporting
| Mechanism | Purpose | When to Use |
|-----------|---------|-------------|
| `__declspec(dllexport)` on `extern "C" __cdecl` | Force undecorated export from the exe | The one and only export mechanism needed — see Finding 1 |
| `union { PMF pmf; void* p; }` extractor | Convert non-static, non-virtual member fn pointers to `void*` | Only for true instance methods (groundScene/object/commandParser/worldSnapshot) |
| `static_assert(sizeof(PMF)==sizeof(void*))` | Guard against multiple/virtual-inheritance PMF inflation | At the top of the PMF helper; fails the build if a class has inflated PMFs |
| Free-function thunk `static T classCtorThunk(...)` | Wrap a constructor (whose address can't be taken) | Every `*::ctor` row |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `__declspec(dllexport)` only | `.def` file with EXPORTS section | Works, but adds a build-graph artifact + a ModuleDefinitionFile link setting; the gl11 twin proves dllexport alone is enough — match it for consistency |
| `__declspec(dllexport)` only | `#pragma comment(linker, "/EXPORT:GetEngineHookPoints")` | Useful ONLY if a future toolchain decorates `extern "C" __cdecl` (it does not on v145); keep as documented fallback, do not ship by default |
| `reinterpret_cast<void*&>(pmf)` | `union` helper | `reinterpret_cast` between PMF and `void*` is ill-formed (different sizes possible) and trips `-Werror`-class diagnostics; the union (or `memcpy`) is the portable idiom under the `sizeof` guard |
| X-macro `.inc` | hand-maintained header + coverage test | X-macro guarantees the name list exists once; but the SWG `&fn` rows are heterogeneous (static vs PMF vs thunk vs global) so the table body can't be a uniform macro expansion. Recommend: X-macro for the NAME LIST + required-set (shared with Utinni), hand-written table BODY on our side, coverage test bridges them. See §"Shared header decision". |

**Installation:** None — no packages. New source file + 3 lines in the vcxproj ItemGroup.

## Architecture Patterns

### System Architecture Diagram

```
                          COMPILE TIME (MSVC v145, Win32)
   utinni_engine_hookpoints.inc  ──(X-macro: name list + required set)──┐
   (shared w/ D:/Code/Utinni)                                           │
                                                                        ▼
   utinni_advertise.cpp ──&Game::install (static → fn ptr)──────► s_engineHookPoints[]
   (NEW, linked into exe)  &CommandParser::addSubCommand ──union──►  { "name", void* }
                           ctorThunk(GroundScene) ────────────────►  (NUL-terminated)
                           &g_someGlobal ──────────────────────────►
                                                                        │
                           coverage_check() ◄──(asserts 0-missing,──────┘
                           (build-time or                  0-dup vs .inc required set)
                            runtime self-check)
                                                                        │
                                                              __declspec(dllexport)
                                                              extern "C" __cdecl
                                                                        ▼
                          RUNTIME                          SwgClient_{r,d,o}.exe
                                                           exports GetEngineHookPoints
                                                                        │
   Utinni UtinniCore.dll (INJECTED, out of repo) ──GetProcAddress(hExe,──┘
       │                                            "GetEngineHookPoints")
       ├── if present  → resolve each swg::* endpoint BY NAME from table → detour/call/read
       └── if absent   → fall back to hardcoded SWGEmu RVA path (no regression)
```

File-to-implementation mapping is in the Component Responsibilities table below; the diagram shows data flow only.

### Recommended Project Structure
```
src/game/client/application/SwgClient/
├── src/win32/
│   ├── WinMain.cpp                  # existing entry (WinMain → ClientMain)
│   ├── ClientMain.cpp               # existing; installConfigFileOverride() lives here (:98)
│   ├── utinni_advertise.cpp         # NEW — GetEngineHookPoints + s_engineHookPoints[] + thunks
│   └── ...
├── src/shared/
│   └── utinni_engine_hookpoints.h   # NEW (or under a shared loc) — structs + version + .inc include
│   └── utinni_engine_hookpoints.inc # NEW — X-macro canonical name list (shared with Utinni)
└── build/win32/SwgClient.vcxproj    # add the 2 new files to <ItemGroup> (ClCompile + ClInclude)
```

**Component Responsibilities:**
| File | Responsibility |
|------|----------------|
| `utinni_advertise.cpp` | Defines `s_engineHookPoints[]`, the PMF union helper, ctor thunks, `GetEngineHookPoints()`, the coverage self-check |
| `utinni_engine_hookpoints.h` | `UtinniEngineHookPoint`/`UtinniEngineHookPoints` structs + `UTINNI_HOOKPOINTS_VERSION`; pulls in `.inc` |
| `utinni_engine_hookpoints.inc` | The canonical X-macro name list + required-set marker; the single artifact shared verbatim with D:/Code/Utinni |
| `SwgClient.vcxproj` | Adds the new `.cpp` to all 3 Win32 configs (Debug/Optimized/Release); header is include-only |

### Pattern 1: Mirror the shipped gl11 export (37-01 core)
**What:** Define the struct + the exported getter exactly like Direct3d11.cpp's `GetHookPoints`.
**When to use:** The whole export surface.
```cpp
// Source: src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp:872-888 (shipped pattern)
struct UtinniDx11HookPoints { IDXGISwapChain1* swapChain; ID3D11Device* device; ID3D11DeviceContext* context; };
extern "C" __declspec(dllexport) UtinniDx11HookPoints __cdecl GetHookPoints();   // NO .def, NO /EXPORT
UtinniDx11HookPoints GetHookPoints() { /* ... */ }
```
The exe-side mirror:
```cpp
// NEW utinni_advertise.cpp
struct UtinniEngineHookPoint  { const char* name; void* addr; };
struct UtinniEngineHookPoints { unsigned version; unsigned count; const UtinniEngineHookPoint* entries; };
extern "C" __declspec(dllexport) const UtinniEngineHookPoints* __cdecl GetEngineHookPoints();
const UtinniEngineHookPoints* GetEngineHookPoints() { return &s_table; }
```

### Pattern 2: Static method → plain function pointer (the easy majority)
**What:** `&Class::staticMethod` is a normal function pointer; cast to `void*` directly.
**When to use:** All of `config`, `game`, `graphics`, `cui::manager` (every method is `static`).
```cpp
{ "game::install",   (void*)&Game::install },        // Game::install is `static void install(Application)` [VERIFIED Game.h:94]
{ "graphics::install",(void*)&Graphics::install },    // `static bool install()` [VERIFIED Graphics.h:70]
{ "cui::manager::render",(void*)&CuiManager::render },// `static void render()` [VERIFIED CuiManager.h:88]
```
Note: overloads need a cast — `(void*)static_cast<bool(*)(HWND,int,int)>(&Graphics::present)`.

### Pattern 3: Non-static, non-virtual member → union extractor
**What:** PMF→void* via a union under the size guard.
**When to use:** `CommandParser::addSubCommand`, `CommandParser::createDelegateCommands`, `WorldSnapshot::*`, non-virtual `GroundScene` members.
```cpp
template <class PMF>
inline void* pmfToVoid(PMF pmf) {
    static_assert(sizeof(PMF) == sizeof(void*), "inflated PMF (multiple/virtual inheritance) — needs a thunk");
    union { PMF m; void* p; } u; u.p = nullptr; u.m = pmf; return u.p;
}
// row: { "commandParser::addSubCommand", pmfToVoid(&CommandParser::addSubCommand) }
```
**CAVEAT (virtual):** `&Class::virtualMethod` yields a vtable-thunk address under MSVC, not the implementation. The `static_assert` does NOT catch this (single-inheritance virtual PMF is still pointer-sized). For virtual rows, advertise via a thunk that calls the method on a known instance, OR skip and let Utinni resolve off the vtable (spec already does this for `cui::io::processEvent`).

### Pattern 4: Constructor → free-function thunk (mandatory for every `*::ctor` row)
**What:** You cannot take `&Class::Class`. Wrap it.
**When to use:** `scene::groundScene::ctor`, `commandParser::ctor1/ctor2`, `cui::chatWindow::ctor`, every object/UI ctor row.
```cpp
// matches Utinni's __thiscall ctor typedef shape (pThis, args...) by exporting the real ctor's effect.
// Simplest correct form: a thunk that placement-news / forwards. Decide per-row in 37-03; the spec's
// "*::ctor" rows are the §8 open-item tier, not MVP-blocking except scene::groundScene::ctor.
static void* groundSceneCtorThunk = /* address of an explicit wrapper, NOT &GroundScene::GroundScene */;
```

### Anti-Patterns to Avoid
- **`reinterpret_cast<void*>(&Class::member)`** — ill-formed for PMFs; use the union helper.
- **Advertising `&Class::Class` directly** — does not compile; thunk required.
- **Advertising `&Class::virtualMethod` and assuming it's the impl address** — it's a thunk; wrong detour target. Worse than a missing row (spec §0).
- **Adding a `.def` / ModuleDefinitionFile** — unnecessary; the gl11 twin proves dllexport-only works; extra surface to maintain across `_r/_d/_o`.
- **Relying on `&Game::getScene` for inline methods** — `Game::getScene` is `inline` (Game.h:306); its address may not be emitted unless ODR-used. Prefer non-inline rows for the catalog; if an inline row is required, force emission. (Most hooked methods are non-inline; flagged where relevant.)

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Undecorated exe export | Custom .def + name-mangling logic | `__declspec(dllexport)` on `extern "C" __cdecl` | Proven by gl11 twin; dumpbin-confirmed undecorated |
| PMF → void* | Hand-rolled byte copy / cast chain | `union` helper + `static_assert(sizeof==)` | Portable, catches inflated PMFs at compile time |
| Drift between repos | Two hand-typed name lists | One shared `.inc` X-macro | Spec §4; the only safe single-source-of-truth |
| Coverage check | Manual eyeball of the table | Build-time `static_assert` over the .inc count, or a runtime self-check | Spec §7 #3 gate; deterministic |

**Key insight:** The hard part is not the export (trivially solved); it is *which engine symbol each contract name maps to* and *whether `&fn` even produces a usable address for that symbol kind*. The taxonomy (static / non-virtual-member / virtual-member / ctor / global / inline) decides the row form. Get the taxonomy right per row and the rest is mechanical.

## Runtime State Inventory

Not applicable — this is a greenfield additive code change (new export, new file). No rename/refactor/migration; no stored data, service config, OS-registered state, secrets, or stale build artifacts are affected. The only build-graph touch is adding 2 source files to one .vcxproj. **None — verified by scope (additive export, no string/symbol renames).**

## MVP Symbol Resolution Table (37-02)

Resolution method: each contract name → our fully-qualified C++ symbol, with file:line, kind, and a confidence/landmine flag. **CONFIRM** = verified against our header this session. **CHECK** = plausible but not yet opened. **MISMATCH** = the spec's best-guess is wrong against our source.

### config (sharedFoundation — all `static`, free win)
| Contract name | Our symbol | file:line | Kind | Flag |
|---|---|---|---|---|
| `config::loadOverrideConfig` | **NOT `ConfigFile::loadFromBuffer`** — Utinni's typedef is `int(__cdecl*)()` zero-arg (config.cpp:32). Our zero-arg override loader is `ClientMainNamespace::installConfigFileOverride()` (returns void) | ClientMain.cpp:98 | file-static free fn (anon-ish ns), void ret | **MISMATCH / AMBIGUOUS** — see note below. The crash-fixer row needs a thunk: a `int(__cdecl)()` wrapper that calls `installConfigFileOverride()` and returns a result. CONFIRM the contract: does Utinni hook the *zero-arg orchestrator* or the *buffer loader*? Spec best-guess (`loadFromBuffer`) contradicts Utinni's own typedef. |
| `config::loadConfigFileBuffer` | `ConfigFile::loadFromBuffer(char const*, int)` | ConfigFile.h:136 | `static bool` | CONFIRM (sig: Utinni `bool(__cdecl*)(byte*,int)` vs ours `(char const*,int)` — same ABI) |
| `config::loadConfigFileString` | `ConfigFile::loadFile(char const*)` (spec said `loadFromString`; ours is `loadFile`) | ConfigFile.h:135 | `static bool` | CHECK — Utinni typedef `bool(__cdecl*)(const char* filename)` matches `loadFile`, NOT a "loadFromString". Likely MISMATCH on the name; `loadFile` is the match. |
| `config::setModalChat` / `getModalChat` | modal-chat setter/getter | — | likely `static` | CHECK — grep `ModalChat` / chat-input-active in clientUserInterface |

> **`loadOverrideConfig` note (the crash-fixer — highest priority):** Utinni's typedef `int(__cdecl*)()` + its hook (`hkLoadOverrideConfig` calls `loadOverrideConfig()` then loads an extra cfg) shows it hooks a **zero-arg orchestrator that loads the override cfg**, exactly what `installConfigFileOverride()` does (ClientMain.cpp:98-109: opens `misc/override.cfg`, calls `ConfigFile::loadFromBuffer`). The spec's best-guess `ConfigFile::loadFromBuffer` is the *inner* call, not the hooked function. **Recommend:** advertise a thunk `int __cdecl utinni_loadOverrideConfig() { installConfigFileOverride(); return 0; }` and the planner must surface this to discuss-phase. `installConfigFileOverride` is currently file-local — it must be made linkable (move to a named function or expose it). This is the single most important row to get right (EPA-02).

### game (clientGame — all `static`, free win)
| Contract name | Our symbol | file:line | Kind | Flag |
|---|---|---|---|---|
| `game::install` | `Game::install(Application)` | Game.h:94 | `static void` | CONFIRM (takes an enum arg; Utinni typedef should match) |
| `game::quit` | `Game::quit()` | Game.h:98 | `static void` | CONFIRM |
| `game::mainLoop` | `Game::run()` (spec guessed `runGame`/main loop) | Game.h:96 | `static void` | CHECK — `run()` is the loop entry; `runGameLoopOnce` (Game.h:103) is the per-frame. Confirm which Utinni hooks. |
| `game::setupScene` | `Game::setScene(...)` (2 overloads) | Game.h:108,115 | `static void`, OVERLOADED | CHECK — needs cast to disambiguate |
| `game::cleanupScene` | `Game::cleanupScene()` | Game.h:101 | `static void` | CONFIRM |
| `game::getPlayer` | `Game::getPlayer()` | Game.h:150 | `static Object*` | CONFIRM |
| `game::getPlayerCreatureObject` | `Game::getPlayerCreature()` | Game.h:157 | `static CreatureObject*` | CHECK (name differs: `getPlayerCreature`) |
| `game::getCamera` | `Game::getCamera()` | Game.h:173 | `static Camera*`, OVERLOADED w/ const | CHECK — cast needed |
| `game::getConstCamera` | `Game::getConstCamera()` | Game.h:174 | `static Camera const*` | CONFIRM |
| `game::isViewFirstPerson` | `Game::isViewFirstPerson()` | Game.h:185 | `static bool` | CONFIRM |
| `game::isHudSceneTypeSpace` | `Game::isHudSceneTypeSpace()` | Game.h:215 | `static bool` | CONFIRM |
| `game::g_mainLoopCounter` (global) | `Game::ms_loops` (private static) | Game.h:276 | private static int | LANDMINE — private; advertise via `&Game::getLoopCount` accessor address OR friend. Prefer accessor (spec §8 #3). |
| `game::g_runningFlags` (globals) | `Game::ms_done` + ? | Game.h:277 | private static | CHECK — two globals in spec; map to ms_done + another |

### graphics (clientGraphics facade — all `static`)
| Contract name | Our symbol | file:line | Kind | Flag |
|---|---|---|---|---|
| `graphics::install` | `Graphics::install()` | Graphics.h:70 | `static bool` | CONFIRM |
| `graphics::update` | `Graphics::update(float)` | Graphics.h:153 | `static void` | CONFIRM |
| `graphics::beginScene` | `Graphics::beginScene()` | Graphics.h:155 | `static void` | CONFIRM |
| `graphics::endScene` | `Graphics::endScene()` | Graphics.h:156 | `static void` | CONFIRM |
| `graphics::present` | `Graphics::present()` | Graphics.h:161 | `static bool`, OVERLOADED | CHECK — cast `static_cast<bool(*)()>(&Graphics::present)` |
| `graphics::presentWindow` | `Graphics::present(HWND,int,int)` | Graphics.h:162 | `static bool` overload | CHECK — cast `static_cast<bool(*)(HWND,int,int)>` |
| `graphics::resize` | `Graphics::resize(int,int)` | Graphics.h:130 | `static void` | CONFIRM |
| `graphics::flushResources` | `Graphics::flushResources(bool)` | Graphics.h:81 | `static void` | CONFIRM |
| `graphics::screenshot` | `Graphics::screenShot(const char*)` | Graphics.h:170 | `static bool` | CHECK (name: `screenShot`; spec signature differs) |
| `graphics::useHardwareCursor` | `Graphics::setHardwareMouseCursorEnabled(bool)` | Graphics.h:177 | `static void` | CHECK (name differs) |
| `graphics::showMouseCursor` | `Graphics::showMouseCursor(bool)` | Graphics.h:180 | `static bool` | CONFIRM |
| `graphics::setSystemMouseCursorPosition` | `Graphics::setSystemMouseCursorPosition(int,int)` | Graphics.h:181 | `static void` | CONFIRM |
| `graphics::setStaticShader` | `Graphics::setStaticShader(const StaticShader&,int)` | Graphics.h:175 | `static void` | CONFIRM |
| `graphics::g_renderTargetWidth/Height` (globals) | `Graphics::getCurrentRenderTargetWidth/Height()` accessors | Graphics.h:103,104 | `static int` accessors | LANDMINE — spec wants the global's address; ours are behind static accessors. Advertise the accessor (spec §8 #3) and tell Utinni it's call-not-read. |

### scene::groundScene (clientGame — THE landmine cluster)
| Contract name | Our symbol | file:line | Kind | Flag |
|---|---|---|---|---|
| `scene::groundScene::ctor` | `GroundScene::GroundScene(const char*,const char*,CreatureObject*)` | GroundScene.h:199 | **CTOR** | LANDMINE — can't take address; thunk required. (Matches Utinni's offline-scene `__thiscall` ctor typedef, ground_scene.cpp:46.) |
| `scene::groundScene::init` | `GroundScene::init(const char*,CreatureObject*,float)` | GroundScene.h:173 | **PRIVATE** member | LANDMINE — private; friend or in-class thunk |
| `scene::groundScene::reloadTerrain` | `GroundScene::reloadTerrain()` | GroundScene.h:215 | public non-virtual member | PMF via union helper — CONFIRM |
| `scene::groundScene::changeCamera` | `GroundScene::setView(int,float)` (spec `changeCamera`) | GroundScene.h:207 | public member | CHECK (name differs; `setView` is the closest) |
| `scene::groundScene::getCurrentCamera` | `GroundScene::getCurrentCamera()` | GroundScene.h:212 | public member, OVERLOADED (const) | CHECK — cast |
| `scene::groundScene::draw` | `GroundScene::draw() const` | GroundScene.h:204 | **VIRTUAL** member | LANDMINE — `&GroundScene::draw` is a vtable thunk; thunk-wrap or skip |
| `scene::groundScene::update` | `GroundScene::update(float)` | GroundScene.h:77 | **PRIVATE** member | LANDMINE — private |
| `scene::groundScene::handleInputMapUpdate` | `GroundScene::handleInputMapUpdate()` | GroundScene.h:170 | **PRIVATE** member | LANDMINE — private |
| `scene::groundScene::handleInputMapEvent` | `GroundScene::handleInputMapEvent(IoEvent*)` | GroundScene.h:168 | **PRIVATE** member | LANDMINE — private |
| `scene::groundScene::g_instance` (global) | GroundScene singleton ptr | — | global | CHECK — grep for the GroundScene singleton/current-scene global (Game::ms_scene?) |

> **GroundScene access strategy:** init/update/handleInputMap* are private; draw is virtual. Cleanest: add a small **friend struct** declared in GroundScene.h (one line) that exposes the addresses, OR a set of free-function thunks in a TU that is `friend`. The planner should treat the entire `groundScene` cluster as the 37-03 open-item tier — only `reloadTerrain`/`getCurrentCamera` are clean PMFs. This is the scene-change repro path so it matters, but it is NOT the boot/first-detour fix.

### cui::manager (clientUserInterface — all `static`)
| Contract name | Our symbol | file:line | Kind | Flag |
|---|---|---|---|---|
| `cui::manager::render` | `CuiManager::render()` | CuiManager.h:88 | `static void` | CONFIRM |
| `cui::manager::setSize` | `CuiManager::setSize(int,int)` | CuiManager.h:129 | `static void` | CONFIRM |
| `cui::manager::togglePointer` | `CuiManager::setPointerToggledOn(bool)` (spec `togglePointer`) | CuiManager.h:107 | `static void` | CHECK (name differs) |
| `cui::manager::restartMusic` | `CuiManager::restartMusic(bool)` | CuiManager.h:97 | `static void` | CONFIRM |
| `cui::manager::findObjectUnderCursor` | — | — | — | CHECK — not on CuiManager; likely a different class (CuiIoWin/SwgCuiManager). UNRESOLVED. |
| `cui::manager::g_instance` (global) | CuiManager is all-static (no instance) | — | — | MISMATCH — CuiManager has no instance singleton; the "g_instance" is likely `ms_theIoWin` (CuiManager.h:155, private static). Advertise via `&CuiManager::getIoWin` accessor (CuiManager.h:100). |

### command_parser (sharedCommandParser — member methods, PMF tier)
| Contract name | Our symbol | file:line | Kind | Flag |
|---|---|---|---|---|
| `commandParser::ctor1` | `CommandParser::CommandParser(const CmdInfo&, CommandParser*)` | CommandParser.h:128 | **CTOR** | LANDMINE — thunk |
| `commandParser::ctor2` | `CommandParser::CommandParser(const char*,size_t,...)` | CommandParser.h:130 | **CTOR** | LANDMINE — thunk |
| `commandParser::createDelegateCommands` | `CommandParser::createDelegateCommands(const CmdInfo[])` | CommandParser.h:180 | non-virtual member | PMF via union — but it is PROTECTED/private (line 180 below the public block) — CHECK access |
| `commandParser::addSubCommand` | `CommandParser::addSubCommand(CommandParser*)` | CommandParser.h:149 | public non-virtual member | PMF via union — CONFIRM (good spike candidate) |

### cui::io / cui::chatWindow / cui::consoleHelper (37-02 secondary — CHECK tier)
Not yet opened this session. Headers exist: CuiIoWin.h (`./client/library/clientUserInterface/src/shared/core/CuiIoWin.h`), SwgCuiChatWindow.h (`../game/client/library/swgClientUserInterface/src/shared/page/SwgCuiChatWindow.h`). Spec marks `cui::io::processEvent` as "from vtable" → virtual → skip (Utinni resolves off vtable). `cui::chatWindow::ctor` → ctor thunk. `consoleHelper::sendInput` → member PMF. Resolve in 37-02 execution.

## Full-Set Resolution Method (37-03 scoping)

The planner should scope 37-03 by **taxonomy bucket**, not by count, because the row form (and difficulty) is determined by symbol kind:

**Where each subsystem lives (grep anchors in `src/engine/`):**
| Subsystem | Library dir | Grep pattern |
|---|---|---|
| object/clientObject/creature/player | `shared/library/sharedObject/`, `client/library/clientObject/`, `client/library/clientGame/` | `class Object`, `class ClientObject`, `class CreatureObject` |
| objectTemplate / crcString | `shared/library/sharedObject/`, `shared/library/sharedFoundation/` (CrcString) | `class ObjectTemplate`, `class CrcString`, `getByFilename` |
| appearance/portal/skeleton/extent | `client/library/clientGraphics/`, `client/library/clientObject/`, `shared/library/sharedCollision/` (Extent) | `class Appearance`, `class PortalProperty`, `class Extent` |
| terrain/worldSnapshot/proceduralTerrain | `client/library/clientTerrain/`, `shared/library/sharedTerrain/`, `client/library/clientGame/` (WorldSnapshot) | `class WorldSnapshot`, `class ProceduralTerrainAppearance` |
| camera/debugCamera | `client/library/clientGame/` | `class GameCamera`, `class FreeCamera`, `class DebugPortalCamera` |
| clientWorld/renderWorld | `client/library/clientGame/`, `client/library/clientGraphics/` (RenderWorld) | `collide`, `class RenderWorld` |
| hud/login/radial/menu/io_win/systemMessage | `game/client/library/swgClientUserInterface/`, `client/library/sharedIoWin/` | `class SwgCuiHud`, `class IoWin` |
| misc: directInput/audio/treeFile/math/memory/report/network | `client/library/clientDirectInput/`, `client/library/clientAudio/`, `shared/library/sharedFile/` (TreeFile), `shared/library/sharedMath/`, `shared/library/sharedMemoryManager/`, `shared/library/sharedDebug/` (Report), `shared/library/sharedNetwork/` | `class TreeFile`, `class MemoryManager`, `class Report` |
| UI control ctors (UI*::ctor) | `external/3rd/library/ui/` | `class UIButton`, `class UIPage`, … — ALL ctors → thunks |

**Bucket-by-difficulty (drives 37-03 task structure):**
1. **Static methods** (easiest, do first): much of misc (MemoryManager::allocate, Audio::setMasterVolume), Graphics extras. Plain `&fn`.
2. **Non-virtual instance methods** (union helper): WorldSnapshot::*, ObjectTemplate getters, most object getters/setters.
3. **Virtual instance methods** (thunk or skip): Object::addToWorld/removeFromWorld/getType/move (all `virtual` — VERIFIED Object.h:120-121), Appearance::render/collide, GroundScene::draw. `&fn` gives a vtable thunk — DO NOT advertise raw; either skip (Utinni resolves off vtable) or wrap.
4. **Constructors** (mandatory thunk): every `*::ctor` and all ~20 `UI*::ctor` rows.
5. **Globals** (§8 #3): advertise `&g` if it's a real exported static; if file-static behind an accessor, advertise the accessor.
6. **Mid-function patches** (§8 #1): OUT — cannot be `&fn`; deferred.

**Spec best-guesses that are WRONG against our source (flag for execution):**
- `config::loadOverrideConfig` → spec says `ConfigFile::loadFromBuffer`; reality is the zero-arg orchestrator `installConfigFileOverride()` (needs thunk). **HIGHEST PRIORITY mismatch.**
- `config::loadConfigFileString` → spec says `loadFromString`; ours is `ConfigFile::loadFile`.
- `game::mainLoop` → spec says `runGame`; ours is `Game::run`.
- `game::getPlayerCreatureObject` → ours is `Game::getPlayerCreature`.
- `graphics::screenshot/useHardwareCursor` → ours are `screenShot`/`setHardwareMouseCursorEnabled`.
- `cui::manager::g_instance` → CuiManager is all-static, no instance; map to `getIoWin()` accessor or `ms_theIoWin`.
- `scene::groundScene::changeCamera` → ours is `setView`.

## §8 Open-Items Decisions

| § | Item | Decision from research |
|---|------|------------------------|
| §8 #1 | Mid-function patches | OUT for 37-02; 37-03 triage. Cannot be `&fn` (depend on Pre-CU instruction offsets). The MVP boot/render/scene path needs none. |
| §8 #2 | `extent::intersect` build split | `&Extent::intersect` collapses the retail/SWGEmu RVA split to one symbol — confirmed advantage of advertisement. Extent lives in `shared/library/sharedCollision/`. CHECK virtual-ness before advertising. |
| §8 #3 | Globals as `&g` | Most "globals" in our build are PRIVATE static members behind accessors (Game::ms_loops, Graphics RT W/H via getCurrentRenderTargetWidth, CuiManager::ms_theIoWin). Advertise the **accessor address**, not the data address (Utinni calls vs reads). Only advertise `&g` where the global is genuinely accessible (non-private, ODR-emitted). |
| §8 #4 | Contract shape | Keep the name→pointer table (spec recommendation). It survives partial population (graceful degradation = EPA-04). |

## Validation Architecture

**Test framework:** This repo has **no unit-test framework** — validation is build-gates + boot smokes (per AGENTS.md: "Builds/boots/renders are truth"). The coverage gate (EPA-04) is therefore best implemented as a **build-time `static_assert`** or a **runtime self-check called once at startup**, not an external test harness.

### What THIS repo can self-verify (no Utinni)
| Acceptance criterion (spec §7) | Probe (this repo) | Gate |
|---|---|---|
| §7 #1 Export present + undecorated | `dumpbin /exports stage/SwgClient_r.exe \| grep GetEngineHookPoints` shows `GetEngineHookPoints = _GetEngineHookPoints` (undecorated public name) | Per build of each flavor `_r/_d/_o` |
| §7 #1 (all flavors) | Same dumpbin on `_d.exe` and `_o.exe` | Phase gate |
| §7 #3 Coverage = zero-missing + no-duplicate | Build-time: `static_assert(s_count == UTINNI_REQUIRED_COUNT)` over the shared `.inc`; AND a runtime self-check `verifyNoNullNoDup(s_engineHookPoints)` (returns false / logs on any null addr or duplicate name) callable in a `#if !PRODUCTION` boot path | Build (static_assert) + boot (self-check) |
| Build links clean | Grep build log for `unresolved external symbol` = 0 (AGENTS.md: `/FORCE` masks LNK2019/2001) | Every build |
| `config::loadOverrideConfig` non-null | The crash-fixer row resolves to a non-null thunk address in the table | Coverage self-check covers it |
| No SWGEmu regression | N/A in this repo — the SWGEmu hardcoded path is Utinni-side; our export is purely additive and inert when not injected. Verify by: client still boots to char-select (boot gate, AGENTS.md) under rasterMajor=5 and =11 | Boot smoke |

### What requires the Utinni side (out of this repo's CI)
| Criterion | Why not self-verifiable |
|---|---|
| §7 #2 First-detour crash gone (live) | Requires injecting UtinniCore into the running client and observing no `0xC0000005 target=0x00401000`. We can only prove the table *advertises* `config::loadOverrideConfig` non-null; the live detour is Utinni's. |
| §7 #5 DX11 overlay renders off the contract | Requires Utinni injection + ImGui overlay render. We prove the rows present + non-null. |
| §7 #4 SWGEmu D3D9 live-smoke unchanged | Runs against the SWGEmu Pre-CU client, not ours. |

### Sampling rate (this repo)
- **Per task commit (37-01):** build SwgClient Debug Win32 + `dumpbin /exports` grep for the export; run the boot gate to char-select.
- **Per wave (37-02/37-03):** full canonical 5-target Win32 build (`Direct3d11;Direct3d9;Direct3d9_ffp;Direct3d9_vsps;SwgClient`), link-log `unresolved external symbol`=0, coverage self-check green, dumpbin on all 3 flavors.
- **Phase gate:** all 3 flavors export undecorated; coverage zero-missing on the required `.inc` set; dual-renderer boot (rasterMajor=5 and 11) to char-select.

### Wave 0 gaps
- [ ] `utinni_engine_hookpoints.h` + `.inc` — must exist (shared with D:/Code/Utinni) before any row references a name.
- [ ] PMF union helper + ctor-thunk pattern — establish in 37-01 spike before catalog.
- [ ] Decision on `installConfigFileOverride` linkability (currently file-local in ClientMain.cpp) — must be exposed or thunked for the crash-fixer row.
- [ ] Coverage self-check harness (`verifyNoNullNoDup`) — build it in 37-01 so 37-02/37-03 inherit the gate.
- No external test-framework install needed.

## Shared Header Decision

D:/Code/Utinni exists locally (`D:/Code/Utinni/UtinniCore/swg/...`) — the name list CAN be coordinated against it directly. The Utinni consumer side uses per-subsystem typedefs + RVA literals (e.g. config.cpp:30-42); it does NOT yet consume a `.inc`. So Phase 37 must **author** `utinni_engine_hookpoints.inc` and hand a copy to Utinni (same dual-placement as the spec doc).

**Recommendation:** X-macro for the **name list + required-set** only (uniform, shareable verbatim); hand-written table **body** on our side (rows are heterogeneous: static fn / union PMF / ctor thunk / accessor — they cannot share one macro expansion). The coverage self-check bridges name-list ↔ table-body so they cannot drift. Put the header at `src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.h` (already on the SwgClient include path: `..\..\src\shared` per vcxproj:122).

## Common Pitfalls

### Pitfall 1: Treating `&Class::virtualMethod` as the implementation address
**What goes wrong:** Utinni detours a vtable thunk, not the real function — wrong-row crash (worse than missing, spec §0).
**Why:** MSVC emits a vcall thunk for `&` of a virtual member.
**How to avoid:** Bucket virtuals separately; thunk-wrap or skip (Utinni resolves off vtable).
**Warning sign:** Row symbol is declared `virtual` in the header.

### Pitfall 2: `&Game::getScene` (inline) has no emitted address
**What goes wrong:** Linker error or a row whose address isn't unique/stable.
**Why:** Inline methods (Game.h has ~15) may not be ODR-emitted unless used.
**How to avoid:** Prefer non-inline rows; force emission if an inline row is required.
**Warning sign:** Method body is in the header `inline`.

### Pitfall 3: Shared-header ABI cascade (AGENTS.md)
**What goes wrong:** Touching a shared header consumed by plugins → stale-DLL boot crash.
**Why:** ABI mismatch.
**How to avoid:** `utinni_engine_hookpoints.h` is NEW and consumed ONLY by the exe (`utinni_advertise.cpp`) — no plugin includes it. Do NOT add the struct to any shared header the plugins compile. Risk: **none** if kept exe-local. Confirmed safe.

### Pitfall 4: BOM on any new .cfg / source written via PowerShell
**What goes wrong:** Release client boot crash (AGENTS.md). N/A for .cpp/.h (compiler tolerates BOM) but never write .cfg via PS Set-Content. Use Edit/Write tools.

### Pitfall 5: `/FORCE` masks the missing-symbol error
**What goes wrong:** A typo'd `&Engine::symbol` that doesn't resolve becomes a LINK warning, not error; exe still links.
**Why:** SwgClient links under `/FORCE` (ForceFileOutput=Enabled, vcxproj:161).
**How to avoid:** Grep the build log for `unresolved external symbol` = 0 after every build (AGENTS.md gate).

## Code Examples

### Verified export (37-01 spike skeleton)
```cpp
// Source: mirrors Direct3d11.cpp:872-888 [VERIFIED shipped + dumpbin-undecorated]
#include "utinni_engine_hookpoints.h"
#include "clientGame/Game.h"
#include "sharedCommandParser/CommandParser.h"

template <class PMF> inline void* pmfToVoid(PMF pmf) {
    static_assert(sizeof(PMF) == sizeof(void*), "inflated PMF — needs a thunk");
    union { PMF m; void* p; } u; u.p = nullptr; u.m = pmf; return u.p;
}

// crash-fixer thunk (installConfigFileOverride must be made linkable)
extern void installConfigFileOverride();           // expose from ClientMain.cpp
static int __cdecl utinni_loadOverrideConfig() { installConfigFileOverride(); return 0; }

static const UtinniEngineHookPoint s_engineHookPoints[] = {
    { "config::loadOverrideConfig", (void*)&utinni_loadOverrideConfig },   // thunk, NOT loadFromBuffer
    { "game::install",              (void*)&Game::install },               // static fn ptr [Game.h:94]
    { "commandParser::addSubCommand", pmfToVoid(&CommandParser::addSubCommand) }, // union PMF [CommandParser.h:149]
    { nullptr, nullptr }
};
static const UtinniEngineHookPoints s_table = {
    UTINNI_HOOKPOINTS_VERSION,
    (unsigned)(sizeof s_engineHookPoints / sizeof s_engineHookPoints[0]) - 1,
    s_engineHookPoints
};
extern "C" __declspec(dllexport) const UtinniEngineHookPoints* __cdecl GetEngineHookPoints();
const UtinniEngineHookPoints* GetEngineHookPoints() { return &s_table; }
```

### dumpbin verification (acceptance §7 #1)
```bash
dumpbin /exports stage/SwgClient_r.exe | grep GetEngineHookPoints
# expect:  N  M  XXXXXXXX  GetEngineHookPoints = _GetEngineHookPoints
```

## State of the Art

| Old Approach | Current Approach | When | Impact |
|---|---|---|---|
| Hardcoded SWGEmu RVAs in UtinniCore | Cooperative `&fn` advertisement | This phase | Addresses correct-by-construction; survives rebuilds |
| `.def` EXPORTS for undecorated names | `__declspec(dllexport)` on `extern "C" __cdecl` | Already shipped (gl11) | No def-file maintenance |

**Deprecated/outdated:** The spec's "best-guess engine symbol" column predates source verification — several guesses are wrong (see mismatch list). Trust the source over the spec column (spec itself says "CONFIRM every one").

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | `__declspec(dllexport)` alone gives an undecorated EXE export (not just DLL) for `extern "C" __cdecl` | Finding 1 | LOW — same PE export mechanism for exe/dll; `/EXPORT` pragma is the documented fallback. Verify with dumpbin in 37-01 (the spike's first probe). |
| A2 | `installConfigFileOverride()` is the function Utinni's zero-arg `loadOverrideConfig` hooks | MVP table / §8 | **RESOLVED (Open Q1)** → LOW. Utinni's typedef `int(__cdecl*)()` is zero-arg, matching `installConfigFileOverride()` exactly; `loadFromBuffer(char*,int)` is two-arg (the inner call, not the hook). Advertised via the `utinni_loadOverrideConfig` thunk in 37-01. Live first-detour confirmation is a Utinni-side acceptance check (out of repo); only this one row would change if ever wrong (additive). |
| A3 | `game::g_runningFlags`, `scene::groundScene::g_instance`, `cui::manager::g_instance` map to private statics / accessors (not exported globals) | MVP table | MEDIUM — needs a grep pass per global; advertise accessor where private. |
| A4 | The MVP `cui::io`/`chatWindow`/`consoleHelper` rows resolve cleanly (not yet opened) | MVP secondary | MEDIUM — headers exist; resolve in 37-02 execution. |
| A5 | No plugin includes the new header, so no ABI cascade | Pitfall 3 | LOW — header is exe-local by design; planner must keep it so. |

## Open Questions (RESOLVED)

> Resolved 2026-06-21 during plan-phase (no separate discuss-phase; user chose "lean on the spec"). The
> resolutions below are the locked decisions the 37-01/02/03 plans build on. Q1's live-confirmation
> remains a Utinni-side acceptance check (out of this repo), documented in Validation Architecture.

1. **Does Utinni hook the zero-arg override orchestrator or the buffer loader for `config::loadOverrideConfig`?**
   — **RESOLVED: the zero-arg orchestrator (`installConfigFileOverride`), advertised via a `int __cdecl` thunk.**
   - Evidence (sufficient to proceed): Utinni's typedef is `int(__cdecl*)()` — zero-arg, `__cdecl` — at
     `D:/Code/Utinni/UtinniCore/swg/misc/config.cpp:39`. `ConfigFile::loadFromBuffer(char*,int)` (the spec's
     best-guess) is two-arg and is the INNER call, so it cannot be the hooked symbol. Our zero-arg
     `installConfigFileOverride()` (ClientMain.cpp:98) matches the typedef exactly and performs the same
     "load an override cfg" behavior the Utinni hook wraps.
   - Decision: 37-01 advertises `config::loadOverrideConfig` → `&utinni_loadOverrideConfig` (a `static int __cdecl`
     thunk wrapping the exposed `installConfigFileOverride()`). The spike (37-01) empirically validates link +
     undecorated export + non-null row; the LIVE "first-detour crash gone" check is Utinni-side (EPA-02 manual
     verification — see Validation Architecture). Risk if wrong is now LOW given the signature match; if Utinni
     ever expects the exact inner loader, only this one row changes (additive, version-tolerant).

2. **How should virtual-member rows (Object::addToWorld, GroundScene::draw, Appearance::render) be advertised?**
   — **RESOLVED: SKIP + document.** `&Class::virtualFn` under MSVC yields a vtable thunk, not the impl address,
     so advertising it would detour the wrong target (worse than missing, spec §0). Utinni resolves virtuals off
     the live vtable already (as it does for `cui::io::processEvent`). 37-03 skips every confirmed-virtual row with
     a `// SKIP: virtual` comment and does NOT put it in the `.inc` required-set (skipped ≠ missing). Per-row
     virtual-ness is confirmed before skip-vs-advertise.

3. **Coverage gate: build-time `static_assert` vs runtime self-check?**
   — **RESOLVED: BOTH.** Compile-time `static_assert` (table row count == `.inc` required-set count — hard compile
     fail on drift, mirroring the Direct3d11_ConstantBuffer.h idiom) PLUS a runtime `utinni_verifyNoNullNoDup()`
     null/duplicate scan (graceful — logs/returns false, never crashes). Seeded in 37-01, extended per-tier in
     37-02/37-03. Lightest gate that cannot silently drift (EPA-04).

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|---|---|---|---|---|
| MSBuild VS18 (v145) | All builds | ✓ | `$env:MSBUILD` (BuildTools\18) | — |
| dumpbin | §7 #1 export verify | ✓ | VS2022 14.44 found (any version fine) | link.exe /dump |
| D:/Code/Utinni | Shared .inc coordination | ✓ | local checkout present | copy .inc into Utinni manually |
| Utinni injection runtime | Live §7 #2/#5 | ✗ (out of this repo) | — | self-verify export+coverage only; live checks are Utinni-side |

**Missing with no fallback:** none blocking this repo's deliverable.
**Missing with fallback:** live first-detour / overlay-render verification → deferred to the Utinni side (documented in Validation Architecture).

## Security Domain

`security_enforcement: true`, but this phase's attack surface is minimal: a read-only getter returning process-internal static addresses, inert unless an already-injected DLL calls it.

### Applicable ASVS Categories
| ASVS Category | Applies | Standard Control |
|---|---|---|
| V5 Input Validation | no | The export takes no input (zero-arg getter) |
| V6 Cryptography | no | No crypto |
| V14 Config/Build | yes | Export must be present on all 3 flavors; no .def to drift; coverage gate prevents shipping a partial contract |

### Threat patterns for an injected-overlay contract
| Pattern | STRIDE | Mitigation |
|---|---|---|
| Exposing internal fn addresses to any process via export | Information Disclosure | Accepted by design — this is the cooperative contract; the addresses are already discoverable by an injected DLL via the PE/PDB. No new exposure beyond what injection already grants. |
| Wrong-row detour (advertising a vtable thunk or wrong symbol as a real fn) | Tampering | The "wrong `&` worse than missing" rule (spec §0); coverage gate + per-row taxonomy review; mark all unconfirmed rows. |
| Stale plugin ABI from a shared-header touch | Tampering/DoS | Keep the new header exe-local (Pitfall 3). |

## Sources

### Primary (HIGH confidence)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp:856-888` — shipped GetHookPoints export pattern.
- `dumpbin /exports stage/gl11_r.dll` (this session) — proves `GetHookPoints = _GetHookPoints` undecorated via dllexport-only.
- `src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj` — exe project: Application type, v145, /FORCE, /LARGEADDRESSAWARE, no .def/ModuleDefinitionFile, 3 source files, all game libs in link closure, `..\..\src\shared` on include path.
- `src/game/client/application/SwgClient/src/win32/ClientMain.cpp:98-109` — `installConfigFileOverride()` (the real zero-arg override loader = crash-fixer target).
- `src/engine/shared/library/sharedFoundation/src/shared/ConfigFile.h:135-136` — `loadFile`/`loadFromBuffer` (static).
- `src/engine/client/library/clientGame/src/shared/core/Game.h:94-215` — Game::* all static; ms_loops/ms_done private statics; several inline.
- `src/engine/client/library/clientGraphics/src/win32/Graphics.h:70-181` — Graphics::* all static, overloads, RT W/H accessors.
- `src/engine/client/library/clientGame/src/shared/scene/GroundScene.h:77,168-204,215` — init/update/handleInputMap* private, draw virtual, ctors at 199-201.
- `src/engine/client/library/clientUserInterface/src/shared/core/CuiManager.h:88-129,155` — CuiManager all static, ms_theIoWin private static.
- `src/engine/shared/library/sharedCommandParser/src/shared/CommandParser.h:128-180` — ctors + addSubCommand (public)/createDelegateCommands.
- `src/engine/shared/library/sharedObject/src/shared/object/Object.h:120-121` — addToWorld/removeFromWorld virtual.
- `D:/Code/Utinni/UtinniCore/swg/misc/config.cpp:30-82` — `loadOverrideConfig` typedef `int(__cdecl*)()` zero-arg (signature-mismatch evidence).
- `D:/Code/Utinni/UtinniCore/swg/scene/ground_scene.cpp:44-72` — GroundScene `__thiscall` typedefs incl. ctor.

### Secondary (MEDIUM confidence)
- `.planning/handoff/2026-06-20-utinni-engine-entrypoint-advertisement-spec.md` — the canonical spec (RVA/best-guess columns informational only, per its own authority note).
- `.planning/ROADMAP.md:147-162` — Phase 37 plans 37-01/02/03 + Goal + requirements.

## Metadata

**Confidence breakdown:**
- Export mechanics (37-01): HIGH — dumpbin-verified the gl11 twin uses dllexport-only; exe mirrors it.
- PMF/static taxonomy: HIGH — verified against actual headers; static-method-heavy engine is the key de-risk.
- Landmines (ctor/virtual/private): HIGH — verified `virtual`/private/ctor declarations in source.
- MVP symbol mapping: MEDIUM-HIGH — config/game/graphics/cui CONFIRM tier verified; several spec best-guesses corrected; `loadOverrideConfig` semantics flagged as the key MEDIUM risk.
- Full-set (37-03): MEDIUM — method + grep anchors given; per-row resolution deferred to execution.

**Research date:** 2026-06-21
**Valid until:** Stable (in-tree code + MSVC behavior; ~90 days). Re-confirm only if SwgClient.vcxproj or the engine class headers change.
