# Phase 38: utinni-advertised-client-coverage-completion - Research

**Researched:** 2026-06-22
**Domain:** MSVC 32-bit symbol advertisement (PMF/thunk/shim mechanics) for the Utinni `GetEngineHookPoints` contract
**Confidence:** HIGH (every in-scope row source-confirmed against `src/` headers `file:line` this session; the few NONEXISTENT/PRIVATE calls are negative-verified by grep)

## Summary

Phase 38 extends the existing 78-row `GetEngineHookPoints` table (37-03) toward the full `&fn`-addressable engine entry-point set, across four provider mechanism buckets: groundScene MI-thunks, client/config external-linkage shims, cui::chatWindow MI-thunks, and the remaining plain-`&fn` full-set rows. The crux is SOURCE-CONFIRMING each engine symbol in THIS tree — a wrong `&fn` detours the wrong function and is worse than a missing row (D-04).

The dominant finding from source-confirmation: **a large fraction of the spec §6 catalog is NOT cleanly advertisable in this tree** and routes to thunk/shim/SKIP/OMIT/NONEXISTENT — exactly as 37-03 already discovered for its scope. `GroundScene` is a multiple-inheritance class (`NetworkScene : public Scene, public MessageDispatch::Receiver`) so every PMF is inflated AND several target methods (`init`, `update`, `handleInputMapUpdate`, `handleInputMapEvent`) are **private** — a free-function thunk cannot call them without friendship. `SwgCuiChatWindow` is triple-inheritance MI and its ctor needs a live `UIPage&`. The `client::*` statics are split: `Os::WindowProc` is **private** (`__stdcall CALLBACK`), `DebugHelp::writeMiniDump` is public-but-in-a-private-include-dir, and `client::{writeCrashLog, setupStartDataInstall}` are **NONEXISTENT** in this tree. `config::{setModalChat,getModalChat}` DO exist — but as `CuiPreferences::{getModalChat,setModalChat}` public statics (the 37-02 "unresolved symbol" note was WRONG; they are on a different class, plain-`&fn`-addressable).

**Primary recommendation:** Plan in four waves matching the request priority (groundScene thunks → client/config shims → chatWindow thunks → remaining plain-`&fn`). For each MI-thunk, the thunk MUST be a `__fastcall(pThis /*ECX*/, int /*EDX*/, args)` free function (the 37-03 `utinni_commandParserCtor1` precedent; MSVC v145 forbids `__thiscall` on free functions, C3865). For private target methods, the thunk cannot be authored in `utinni_advertise.cpp` — it needs an external-linkage forwarder compiled INSIDE the defining class's TU (the friend/member context), exactly like `utinni_installConfigFileOverride` lives in `ClientMain.cpp`. Bump `UTINNI_HOOKPOINTS_VERSION` to 2 on any name add; re-sync `.h`+`.inc` byte-identical into `D:/Code/Utinni/UtinniCore/swg/` (a hand-off step — do NOT write there from this phase's build).

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Engine symbol advertisement table | Exe (`SwgClient_*.exe`) | — | Game symbols (`Game`,`Object`,`GroundScene`,...) link into the exe; the export must live there (`GetApi`/`GetHookPoints` graphics twin lives in `gl11` only because it sees graphics symbols) |
| MI/private-method thunks (groundScene/chatWindow) | Exe (`utinni_advertise.cpp`) for public methods; defining-class TU for private methods | — | A free thunk can only call public members; private targets need a forwarder in the class's own TU (friend/member context) |
| External-linkage shims (client/config statics) | Defining-class TU (`Os.cpp`/`ClientMain.cpp`) | Exe table | File-local/private statics are not addressable from `utinni_advertise.cpp`; the shim must be compiled where the symbol is visible |
| Contract version + coverage gate | Exe (`utinni_advertise.cpp` self-check) + shared `.h`/`.inc` | Utinni consumer (out of repo) | Compile-time `static_assert` + runtime `utinni_verifyNoNullNoDup()` keep table/`.inc` in lockstep |
| DX11 evidence handback (EPA-08) | Documentation only (handoff-back) | — | LOCKED: no code change; the gl11 provider is already correct (CONTEXT.md "DX11 finding") |

## Standard Stack

This is a native MSBuild C++ phase in an existing codebase — no new libraries. The "stack" is the established advertisement mechanism + MSVC v145 ABI rules.

### Core (existing, reuse verbatim)
| Component | Location | Purpose | Why Standard |
|-----------|----------|---------|--------------|
| `utinni_advertise.cpp` | `src/game/client/application/SwgClient/src/win32/` | The table + thunks + coverage self-check + export | The single provider TU; Win32-only (`#if !defined(_WIN64)`) |
| `utinni_engine_hookpoints.{h,inc}` | `src/game/client/application/SwgClient/src/shared/` | Shared struct + X-macro name list (single source of truth) | Re-synced byte-identical to Utinni so names can't drift |
| `pmfToVoid<PMF>()` helper | `utinni_advertise.cpp:64-69` | `std::bit_cast` PMF→void* with `sizeof(PMF)==sizeof(void*)` guard | Standard C++20 idiom; the sizeof guard catches MI/virtual inflation at compile time |
| `__fastcall(pThis,int /*edx*/,args)` thunk pattern | `utinni_commandParserCtor1/2` [utinni_advertise.cpp:109-119] | ABI-correct `__thiscall` emulation for MI/ctor/private-arg cases | The 37-03 precedent; dummy EDX makes `__fastcall`≡`__thiscall` byte-identical |
| External-linkage shim pattern | `utinni_installConfigFileOverride()` [ClientMain.h:24 / ClientMain.cpp:122-125] | Distinctly-named forwarder for file-local/private statics | The 37-01 precedent; compiled in the defining TU |
| WR-05 thunk (parked) | `utinni_consoleHelperSendInput` [git diff, uncommitted] | `consoleHelper::sendInput` → `processInput(istr, getRecurseStackForCommandBeingParsed())` | ALREADY WRITTEN; fold into this phase's build+commit |

### Build toolchain (from AGENTS.md — non-negotiable)
| Tool | Constraint |
|------|-----------|
| MSBuild | `$env:MSBUILD` (VS18/VS2026, `PlatformToolset=v145`); run from **PowerShell** not Git Bash |
| Build flags | `/nodeReuse:false` always; **build serially** (no `/m` — parallel postbuilds race on `d3dcompiler_47.dll`) |
| Link gate | SwgClient links under `/FORCE` (downgrades LNK2019/2001 to warnings) → **grep the build log for `unresolved external symbol` (must be 0)** |
| Relink | `Remove-Item src\compile\win32\SwgClient\<cfg>\SwgClient_*.exe` before building to force the link gate |
| Validation bar | Debug + Release clean; Optimized (`_o`) is pre-existing LNK1281 SAFESEH (0 unresolved from our rows) — do NOT fix |

**Installation:** None. No `npm`/package changes. (Version-verification step N/A — no external packages.)

## Architecture Patterns

### System Architecture Diagram

```
Utinni (injected, out of repo)                    SwgClient_r.exe (this provider)
  GetProcAddress(hExe,"GetEngineHookPoints") ───▶ extern "C" __cdecl GetEngineHookPoints()
        │                                              │ returns &s_table (process-lifetime static)
        ▼                                              ▼
  lookupByName(table, "groundScene::init") ◀──── s_engineHookPoints[] rows:
        │                                          ┌─ plain &Static            (config/game/graphics/...)
        │ resolves addr by NAME                    ├─ pmfToVoid(&Class::member) (non-virtual single-inherit PMF)
        ▼                                          ├─ __fastcall thunk          (MI class / private-arg / ctor)
  installs detour / calls original                 ├─ external-linkage shim     (file-local/private static)
                                                    └─ (SKIP virtual / OMIT inline / NONEXISTENT → not a row)
                                                          │
  coverage: every required .inc name      ◀──── utinni_verifyNoNullNoDup():
  must resolve non-null                          (a) no null addr (b) no dup name
                                                  (c) name-set == X-macro .inc set
                                                          │
                                                   static_assert(rowcount == .inc count)  [compile-time drift smoke]
```

The contract carries ONLY `{name, void* addr}` rows. The address kind (static/PMF/thunk/shim) is invisible to Utinni — its typedef supplies the calling convention; the thunk's job is to make the emitted MSVC convention match that typedef.

### Pattern 1: MI-class non-virtual public method → `__fastcall` thunk
**What:** A free-function thunk that calls the public member, advertised as the thunk's address.
**When to use:** Target class is multiple-inheritance (PMF inflated > `sizeof(void*)` → fails `pmfToVoid` guard) AND the method is **public** AND non-virtual.
**Example:**
```cpp
// Source: pattern from utinni_advertise.cpp:109 (utinni_commandParserCtor1)
// GroundScene::reloadTerrain() is PUBLIC [GroundScene.h:215], non-virtual, but
// GroundScene is MI (NetworkScene : Scene, MessageDispatch::Receiver) → PMF inflated.
static void __fastcall utinni_groundSceneReloadTerrain(GroundScene * pThis, int /*edx*/)
{
    pThis->reloadTerrain();
}
// Utinni-side typedef to match: void(__thiscall*)(GroundScene*)
```

### Pattern 2: MI-class PRIVATE method → forwarder in the defining TU (NOT a free thunk)
**What:** A friend/member-context forwarder compiled inside the class's own `.cpp`, exposed with external linkage, then wrapped (if MI) by a `__fastcall` thunk or advertised directly.
**When to use:** Target method is **private/protected** (a free thunk in `utinni_advertise.cpp` cannot name it — C2248 access violation).
**Why it matters:** `GroundScene::{init, update, handleInputMapUpdate, handleInputMapEvent}` are all **private** [GroundScene.h:77,168,170,173]. You CANNOT write `pThis->init(...)` in `utinni_advertise.cpp`. The forwarder must live in `GroundScene.cpp` (which has member access), exactly as `utinni_installConfigFileOverride()` lives in `ClientMain.cpp` to reach the file-local `installConfigFileOverride()`.
```cpp
// In GroundScene.cpp (has private access):
void utinni_groundSceneInit(GroundScene * gs, const char * terrain, CreatureObject * player, float t)
{
    gs->init(terrain, player, t);   // legal: same TU as the class definition
}
// Declared in a small header GroundScene exposes; advertised in the table.
```

### Pattern 3: file-local / private static → external-linkage shim
**What:** A distinctly-named (`utinni_*`) external-linkage forwarder in the defining TU.
**When to use:** Static has internal linkage (file-static) or is a private class member → not addressable from `utinni_advertise.cpp`.
**Example:**
```cpp
// Source: ClientMain.cpp:122 (utinni_installConfigFileOverride precedent)
// Os::WindowProc is PRIVATE [Os.h:127,138] and __stdcall CALLBACK.
// Shim in Os.cpp (member context), exposed external:
LRESULT CALLBACK utinni_osWindowProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    return Os::WindowProc(h, m, w, l);   // legal in Os.cpp; __stdcall preserved
}
// Utinni-side typedef: LRESULT(__stdcall*)(HWND,UINT,WPARAM,LPARAM)
```

### Anti-Patterns to Avoid
- **`&Class::virtualMethod`:** yields a vtable-dispatch thunk, NOT the impl → SKIP (Utinni resolves off the live vtable). e.g. `GroundScene::draw` [GroundScene.h:204] is `virtual`.
- **`&Class::Class` (taking a ctor address):** illegal in C++ → use a placement-new `__fastcall` thunk (37-03 pattern). MI ctors additionally need most-derived `this`-adjustment + any required ctor args.
- **`pmfToVoid(&MIClass::member)`:** trips the `sizeof(PMF)==sizeof(void*)` `static_assert` (C2338) → use a `__fastcall` thunk.
- **Calling a private/protected member from `utinni_advertise.cpp`:** C2248 → forwarder in the defining TU.
- **`__thiscall` on a free function:** C3865 → `__fastcall(pThis,int /*edx*/,...)`.
- **Guessing a symbol the spec named but that doesn't exist in this tree:** D-04 — OMIT-with-reason or route to NONEXISTENT; never guess.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| PMF→void* | union type-pun / reinterpret_cast | existing `pmfToVoid<PMF>()` (`std::bit_cast`) | type-pun is ill-formed; the helper's sizeof guard catches MI inflation |
| `__thiscall` emulation | a hand-rolled naked asm stub | `__fastcall(pThis,int /*edx*/,args)` | dummy-EDX makes it byte-identical to `__thiscall`, no asm |
| Coverage check | a hand-typed name list | the X-macro `.inc` expanded twice (table + `s_requiredNames[]`) | the two expansions cannot drift; that IS the gate |
| Reaching a private engine method | friend-declaring utinni_advertise.cpp into every class | a forwarder in the defining TU (37-01 shim precedent) | no engine-header edits to add `friend`; keeps client Utinni-agnostic |
| Undecorated export | `.def` file / `/EXPORT` pragma | `extern "C" __declspec(dllexport) __cdecl` alone | proven by the shipped `GetEngineHookPoints` (dumpbin-confirmed undecorated, 37-03) |

**Key insight:** Every "addressing" problem in this domain already has a precedent in `utinni_advertise.cpp` (37-01/02/03). The phase is mechanical application of three patterns (plain-`&fn`, `__fastcall` thunk, external shim) plus disciplined SKIP/OMIT/NONEXISTENT classification. The hard part is SOURCE-CONFIRMATION, not invention.

## Runtime State Inventory

Not a rename/refactor/migration phase. The contract is additive `{name,&fn}` rows in an exe export; no stored data, no live-service config, no OS-registered state, no secrets, no stale build artifacts beyond the normal relink. **One cross-repo sync obligation** (not runtime state, but a hand-off step): the `.h`+`.inc` must be re-copied byte-identical into `D:/Code/Utinni/UtinniCore/swg/` at wave close (the phase build does NOT write there).

---

## BUCKET 1 — groundScene::* (MI-thunk bucket)

**Class topology (CONFIRMED):** `GroundScene : public NetworkScene` [GroundScene.h:46]; `NetworkScene : public Scene, public MessageDispatch::Receiver` [NetworkScene.h:28-30]; `Scene : public IoWin` [Scene.h:21]. → **MULTIPLE INHERITANCE** (the 2nd base `MessageDispatch::Receiver` inflates every PMF). **Header:** `#include "clientGame/GroundScene.h"` (public stub redirects to `src/shared/scene/GroundScene.h`; reachable via existing `clientGame\include\public` dir — no vcxproj change).

| contract-name | engine symbol (this tree) | file:line | mechanism | exact signature | landmine notes |
|---|---|---|---|---|---|
| `groundScene::ctor` | `GroundScene::GroundScene(const char*, const char*, CreatureObject*)` | GroundScene.h:199 | **MI-thiscall ctor thunk** (placement-new) | `GroundScene(const char* terrainFilename, const char* playerFilename, CreatureObject* customizedPlayer)` | **public** ctor. MI class → placement-new thunk needs most-derived `this`; ctor #2 [:200] also exists (NetworkId overload). Spec §6 maps `ctor` 0x00519830. Thunk = `__fastcall(pThis,edx,...)`, `::new(pThis) GroundScene(...)`. |
| `groundScene::init` | `GroundScene::init(const char*, CreatureObject*, float)` | GroundScene.h:173 | **PRIVATE → forwarder in GroundScene.cpp, then thunk** | `void init(const char* terrainFilename, CreatureObject* player, float timeInSeconds)` | **PRIVATE** — cannot call from utinni_advertise.cpp (C2248). Needs a `utinni_groundSceneInit` forwarder compiled in GroundScene.cpp. |
| `groundScene::reloadTerrain` | `GroundScene::reloadTerrain()` | GroundScene.h:215 | **MI-thiscall thunk** | `void reloadTerrain()` | **public**, non-virtual. Free `__fastcall(pThis,edx)` thunk OK. |
| `groundScene::changeCamera` | `GroundScene::setView(int, float=0.f)` | GroundScene.h:207 | **MI-thiscall thunk** | `void setView(int newView, float value = 0.f)` | **public**. Spec name "changeCamera" → ours `setView` (MISMATCH; bake correction in the row comment, contract name stays `changeCamera`). |
| `groundScene::getCurrentCamera` | `GroundScene::getCurrentCamera()` | GroundScene.h:212 | **MI-thiscall thunk** | `GameCamera* getCurrentCamera()` | **public**, OVERLOADED (const sibling [:213]) → if thunked, no cast needed (call resolves on `this` constness); if ever PMF'd, static_cast the overload. |
| `groundScene::draw` | `GroundScene::draw() const` | GroundScene.h:204 | **SKIP: virtual** | `virtual void draw() const` | **VIRTUAL** override of Scene/IoWin → vtable thunk, NOT the impl. Already SKIP-listed in 37-03. Utinni resolves off the live vtable (spec §6). Do NOT advertise. |
| `groundScene::update` | `GroundScene::update(float)` | GroundScene.h:77 | **PRIVATE → forwarder in GroundScene.cpp, then thunk** | `void update(float elapsedTime)` | **PRIVATE**. Forwarder required (same as init). |
| `groundScene::handleInputMapUpdate` | `GroundScene::handleInputMapUpdate()` | GroundScene.h:170 | **PRIVATE → forwarder in GroundScene.cpp, then thunk** | `void handleInputMapUpdate()` | **PRIVATE**. Forwarder required. |
| `groundScene::handleInputMapEvent` | `GroundScene::handleInputMapEvent(IoEvent*)` | GroundScene.h:168 | **PRIVATE → forwarder in GroundScene.cpp, then thunk** | `void handleInputMapEvent(IoEvent* event)` | **PRIVATE**. Forwarder required. (Spec §6 0x0051AA40.) |
| `groundScene::g_instance` | `Game::getScene()` / `Game::ms_scene` | Game.h(src/shared/core):105-106,271,306 | **ACCESSOR — INLINE getter (OMIT) or new non-inline accessor** | `static Scene* getScene()` is **inline** [:306]; `ms_scene` is **private static** [:271] | **No dedicated GroundScene singleton in this tree.** The runtime singleton is reached via `Game::getScene()` cast to GroundScene (confirmed at 20+ call sites, e.g. GroundScene.cpp:489 `dynamic_cast<GroundScene*>(Game::getScene())`). `getScene` is INLINE → no ODR address (Pitfall: 37 omitted `getLoopCount` for the same reason). Options: (a) OMIT this row (graceful degradation), or (b) add a non-inline `Game::getScenePtrForUtinni()`-style accessor in Game.cpp. Recommend OMIT unless Utinni's groundScene editor strictly needs the singleton pointer — confirm with Utinni in the handoff. |

**Bucket 1 disposition:** 5 thunks (ctor + reloadTerrain + changeCamera/setView + getCurrentCamera + the 4 private ones via forwarder+thunk = init/update/handleInputMapUpdate/handleInputMapEvent), 1 SKIP (draw/virtual), 1 OMIT-or-accessor-decision (g_instance/inline). Note: the 4 private methods need BOTH a GroundScene.cpp forwarder AND (because the class is MI) a `__fastcall` thunk — or the forwarder itself can be the `__fastcall(pThis,edx,...)` free function declared `friend` is unnecessary since it's compiled in-TU. Simplest: author each private forwarder in GroundScene.cpp with the `__fastcall(GroundScene*,int,args)` signature directly (in-TU member access + correct ABI in one function), expose via a header, advertise the address.

---

## BUCKET 2 — client::* + config::* (external-linkage-shim bucket)

| contract-name | engine symbol (this tree) | file:line | mechanism | exact signature | landmine notes |
|---|---|---|---|---|---|
| `client::wndProc` | `Os::WindowProc` | Os.h(src/win32):138 (under `private:` at :127) | **external-linkage shim in Os.cpp** | `static LRESULT CALLBACK WindowProc(HWND,UINT,WPARAM,LPARAM)` | **PRIVATE** static member AND **`__stdcall`** (`CALLBACK`). Shim `utinni_osWindowProc` must be compiled in Os.cpp (member access) and keep `CALLBACK`/`__stdcall`. Header `Os.h` is in `src/win32` (NOT a public include dir) — but the shim lives in Os.cpp so utinni_advertise.cpp only needs the shim's forward declaration (put it in a tiny exe-local header or extern-declare it). Spec §6 0x00AA0970, `__stdcall`. |
| `client::writeMiniDump` | `DebugHelp::writeMiniDump` | DebugHelp.h(src/win32):36 | **plain-&fn IF include wired, else shim** | `static bool writeMiniDump(char const* fileName=0, PEXCEPTION_POINTERS exceptionPointers=0)` | **public** static, but declared in `src/win32/DebugHelp.h` (NOT `sharedDebug/include/public`). Two options: (a) add a `utinni_writeMiniDump` shim in DebugHelp.cpp (no include trap), or (b) wire the private include dir + take `&DebugHelp::writeMiniDump` directly. Recommend the shim (avoids exposing a win32-private header to the exe; consistent with the client::wndProc shim). Default args don't affect the address. |
| `client::writeCrashLog` | **NONEXISTENT** | — (grep: 0 hits for `writeCrashLog` in src/) | **NONEXISTENT → OMIT + handoff-back** | — | No `writeCrashLog` symbol anywhere. The crash-log `.txt` is written inline by `SetupSharedFoundation`'s exception handler [SetupSharedFoundation.cpp:92 `sprintf(fileName,"%s-%s-%I64d.txt",...)`], not a named function. Spec's `client::writeCrashLog` 0x00A9F640 is a Pre-CU SWGEmu function with no from-source twin. OMIT; record for Utinni (mid-function crashlog inline hook is already §8 #1 deferred SWGEmu-only). |
| `client::setupStartDataInstall` | **NONEXISTENT** | — (grep: 0 hits in src/) | **NONEXISTENT → OMIT + handoff-back** | — | No `setupStartDataInstall`/`StartDataInstall` symbol in this tree. SWGEmu Pre-CU concept with no twin. OMIT; record for Utinni. |
| `config::setModalChat` | `CuiPreferences::setModalChat(bool)` | CuiPreferences.h:95 | **plain-&fn (public static)** | `static void setModalChat(bool b)` | **CORRECTION to 37-02:** these are NOT on `config`/`ConfigFile`/missing — they are public statics on `CuiPreferences` (clientUserInterface), confirmed at CuiPreferences.h:94-95 + CuiPreferences.cpp:1267/1655. Plain `(void*)&CuiPreferences::setModalChat`. `CuiPreferences.h` reachable via `clientUserInterface\include\public` (already on the include path). Contract name stays `config::setModalChat`; row comment notes the MISMATCH (ours `CuiPreferences::`). |
| `config::getModalChat` | `CuiPreferences::getModalChat()` | CuiPreferences.h:94 | **plain-&fn (public static)** | `static bool getModalChat()` | Same as above; non-inline (defined in CuiPreferences.cpp:1655). Plain `(void*)&CuiPreferences::getModalChat`. |

**Bucket 2 disposition:** 2 shims (wndProc private+__stdcall, writeMiniDump win32-private-header), 2 plain-&fn (setModalChat/getModalChat — the 37-02 "unresolved" note was WRONG), 2 NONEXISTENT (writeCrashLog, setupStartDataInstall).

---

## BUCKET 3 — cui::chatWindow::* (MI-thunk bucket, conditional)

**Class topology (CONFIRMED):** `SwgCuiChatWindow : public SwgCuiLockableMediator, public UINotification, public MessageDispatch::Receiver` [SwgCuiChatWindow.h:58-61] → **TRIPLE inheritance, MI**. **Header:** `#include "swgClientUserInterface/SwgCuiChatWindow.h"` (public stub → `src/shared/page/SwgCuiChatWindow.h`; reachable via existing `swgClientUserInterface\include\public`). Spec method names do NOT match this tree — reconciled below.

| contract-name | engine symbol (this tree) | file:line | mechanism | exact signature | landmine notes |
|---|---|---|---|---|---|
| `cui::chatWindow::ctor` | `SwgCuiChatWindow(UIPage&, Game::SceneType, std::string const&)` | SwgCuiChatWindow.h:106 | **MI-thiscall ctor thunk (with required UIPage& arg)** | `SwgCuiChatWindow(UIPage & page, Game::SceneType sceneType, std::string const & windowName = std::string())` | MI class → placement-new thunk + most-derived `this`-adjustment AND a live `UIPage&` arg the injector must supply. **37-03 DEFERRED this exact ctor** for this reason. CONDITIONAL: advertise only if Utinni can supply a `UIPage&`; otherwise DEFER. Recommend DEFER unless Utinni confirms it constructs through a page. |
| `cui::chatWindow::enableTextInput` | `SwgCuiChatWindow::acceptTextInput(bool, bool, bool)` | SwgCuiChatWindow.h:112 | **MI-thiscall thunk** | `void acceptTextInput(bool b, bool setKeyboardInput = true, bool unfocusMediator = false)` | **public**, non-virtual. Spec name "enableTextInput" → ours `acceptTextInput` (MISMATCH). MI → `__fastcall` thunk (PMF inflated). |
| `cui::chatWindow::writeToAllTabs` | `SwgCuiChatWindow::appendToAllTabs(const Unicode::String&)` | SwgCuiChatWindow.h:172 | **MI-thiscall thunk** | `void appendToAllTabs(const Unicode::String & str)` | **public**. Spec "writeToAllTabs" → ours `appendToAllTabs` (MISMATCH). MI → thunk. |
| `cui::chatWindow::writeToCurrentTab` | `SwgCuiChatWindow::appendTextToCurrentTab(const Unicode::String&)` | SwgCuiChatWindow.h:174 | **MI-thiscall thunk** | `void appendTextToCurrentTab(const Unicode::String & str)` | **public**. Spec "writeToCurrentTab" → ours `appendTextToCurrentTab` (MISMATCH). MI → thunk. |
| `cui::chatWindow::chatEnterHandler` | `SwgCuiChatWindow::performEnterKey()` | SwgCuiChatWindow.h:214 | **MI-thiscall thunk (clean entry) — but verify Issue #11 intent** | `void performEnterKey()` | **public**, non-virtual, **clean function entry** (NOT a mid-function patch in this tree). HOWEVER spec §6 + CONTEXT.md flag `chatEnterHandler` as the Issue #11 chat-context-routing feature, which Utinni patches mid-function on SWGEmu. CONTEXT.md OUT-OF-SCOPE: "Mid-function-patch features (Issue #11 chat routing)". **Decision:** `performEnterKey` is the clean function-entry equivalent; advertising its address is in-scope (it's a real `&fn`), but whether Utinni's Issue #11 feature works off a function-entry hook vs needs the mid-body patch is a JOINT cooperative-API decision (CONTEXT.md). Recommend: advertise `performEnterKey` as the clean entry, flag in handoff that Issue #11's mid-function NOP behavior is separate and SWGEmu-only until Utinni confirms the entry hook suffices. |

**Bucket 3 disposition:** 3 clean MI-thunks (enableTextInput/writeToAllTabs/writeToCurrentTab), 1 conditional MI-thunk-or-advertise (chatEnterHandler/performEnterKey — clean entry, Issue #11 semantics deferred to joint decision), 1 DEFER (ctor — MI + UIPage& arg, 37-03 already deferred). All 3 clean ones need `__fastcall` thunks (PMF inflated by triple-inheritance).

---

## BUCKET 4 — Remaining addressable full-set (plain-&fn bucket)

The 37-03 catalog already advertised the bulk of the plain-`&fn` non-MI/non-virtual full-set: extent::intersect, object non-virtual getters/setters (12), objectTemplate (4), worldSnapshot statics (6), camera non-virtual setters (5), misc statics (memory/audio/treeFile/report = 6), commandParser ctors (2), graphics::g_frameNumber. **Diffing spec §6 "Full set" against the current `.inc`**, the remaining spec rows are almost entirely already-classified as SKIP/OMIT in 37-03 (virtual/inline/MI/nonexistent). New plain-`&fn` candidates this phase are LIMITED — most of the spec's "remaining ~30 subsystems" do NOT exist as plain symbols in this tree (37-03's "plan-vs-source taxonomy reconciliation" applies). Source-confirmation per candidate is REQUIRED in the plan; below is the classification from this session's reads:

| spec endpoint(s) | classification | reason (source-confirmed where cited) |
|---|---|---|
| `object::addToWorld/removeFromWorld/setParentCell` | **SKIP: virtual** | already SKIP-listed [Object.h:120-121,165], 37-03 |
| `object::move_o` | **OMIT: inline** | inline [Object.h:1216], 37-03 |
| `commandParser::createDelegateCommands` | **SKIP/OMIT: protected** | `protected` [CommandParser.h:180] — not addressable as plain &fn; would need an in-TU forwarder. Recommend OMIT (Utinni's TJT path uses addSubCommand/ctors already advertised). |
| `objectTemplate` base getters (getByFilename/getByIff/getByCrc/reload/...) | **OMIT/verify** | 37-03 took filename getters from non-virtual SharedObjectTemplate; the base `ObjectTemplate` factory statics need per-symbol confirmation if Utinni requires them — most are virtual or live on different classes in this tree. Source-confirm before adding. |
| `appearance::*`, `portal::*`, `skeletalAppearance::*` | **mostly SKIP: virtual / verify** | Appearance::render/collide are virtual (37-03 SKIP). The rest need per-symbol header confirmation; many are virtual. |
| `terrain::setTimeOfDay/getTimeOfDay/setWeatherIndex`, `proceduralTerrain::*` | **OMIT: virtual/absent** | 37-03 confirmed virtual/absent in this tree |
| `clientWorld::collide/internalCollide`, `renderWorld::render/addObjectNotifications` | **SKIP: virtual / verify** | RenderWorld::render SKIP-listed (37-03); collide overloads need confirmation |
| `camera` getters (getViewport*) | **OMIT: inline** | inline [Camera.h:210-258], 37-03 |
| `hud/loginScreen/gameMenu/radialMenu/menu` ctors + methods | **DEFER: MI ctors / verify** | UI mediators are MI (like chatWindow) → ctor thunks deferred (37-03); non-ctor methods need per-symbol confirmation |
| `UI*::ctor` (~20 low-level UI control ctors) | **DEFER wholesale** | 37-03 DEFERRED — each needs its own placement-new thunk; Utinni resolves via RVA today; not phase-blocking |
| read-only globals (player health/hud view-distance/terrain singleton/weather/static-shader) | **OMIT: private, no non-inline accessor** | 37-03 confirmed private-behind-no-non-inline-accessor (§8 #3: never take a private-member address) |
| `memory::allocateString/deallocateString`, `math::vectorNormalize`, `network idManager*`, `crcString::calculateCrc` | **OMIT/verify** | 37-03: MemoryManager has `free()` not `deallocate`; the rest unverified — never guess, source-confirm before adding |
| `bloomShader::preSceneRender/postSceneRender`, `shader::popCell` | **OMIT/DEFER** | popCell is mid-function (§8 #1 deferred); bloom shader symbols need confirmation |

**Include-path trap (37-01/37-03 re-bite watch):** every NEW library whose header you `#include` must have its `include\public` (or in-TU shim) wired in **all 5** SwgClient config blocks. Already wired: clientGame, clientGraphics, clientUserInterface, sharedCommandParser, **sharedCollision** (added 37-03), sharedObject, sharedGame, sharedMemoryManager(? — verify), clientAudio, sharedFile, sharedDebug, swgClientUserInterface. **NOT obviously on the path:** any header under a library's `src/win32` or `src/shared` private dir (e.g. `Os.h`, `DebugHelp.h` are in `src/win32`) — these are why the client::* bucket uses in-TU shims rather than direct includes. If a Bucket-4 add needs a new library, add its `include\public` to all 5 blocks (T-37-01 lesson) — harmless on x64 (TU is `#if !defined(_WIN64)` guarded).

**Bucket 4 disposition:** The plain-`&fn` add-list this phase is SMALL and must be per-symbol source-confirmed in the plan — the spec's "~30 subsystems" are overwhelmingly already SKIP/OMIT/DEFER/NONEXISTENT in this from-source tree (the spec catalog is the SWGEmu Pre-CU RVA list, not this tree's symbol set). Treat any Bucket-4 candidate not already in `.inc` as "confirm-or-OMIT," never "add on spec faith."

---

## BUCKET 5 — Contract version + coverage gate mechanics

| mechanic | current state | this phase's obligation |
|---|---|---|
| `UTINNI_HOOKPOINTS_VERSION` | pinned at **1** [utinni_engine_hookpoints.h:40] | **D-03: bump to 2** on any name add/remove. (NOTE: the header comment says "PINNED at 1 for all waves... do NOT bump per wave" — this was a 37-era decision; CONTEXT.md D-03 OVERRIDES it: "Any name add/remove bumps `UTINNI_HOOKPOINTS_VERSION`". Reconcile by bumping to 2 and updating the header comment to reflect the new policy.) |
| X-macro count `static_assert` | `(sizeof table/sizeof[0]) == UTINNI_REQUIRED_COUNT` [utinni_advertise.cpp:334-342] | stays in lockstep automatically: every `.inc` name add = +1 required count; the table must gain exactly one row or the `static_assert` fails (compile-time drift smoke). NO sentinel row (count = sizeof/sizeof, no -1). |
| runtime `utinni_verifyNoNullNoDup()` | 3-part: no-null + no-dup + name-set-equality vs X-macro `s_requiredNames[]` [utinni_advertise.cpp:371-443] | runs on the Debug boot path only (`PRODUCTION == 0 && !defined(_WIN64)`) [ClientMain.cpp:258-260]. Every new advertised name must appear in BOTH the `.inc` AND the table body exactly once, else this asserts. |
| `.inc` + `.h` byte-identical re-sync into `D:/Code/Utinni/UtinniCore/swg/` | done at 37-03 close (78 names, version 1) | **HAND-OFF STEP** — re-copy verbatim at this wave's close. The phase BUILD must NOT write to `D:/Code/Utinni` (that's a separate manual sync; CONTEXT.md "do not write there"). |

**Lockstep rule:** the `.inc` is the single source of truth for the name SET; the table body in `utinni_advertise.cpp` is the heterogeneous address list. A name lives in `.inc` ⟺ it has exactly one table row ⟺ it has a confirmed `&fn`/thunk/shim. SKIP-virtual/OMIT-inline/NONEXISTENT names are kept OUT of the `.inc` (they are not "missing required rows" — they're vtable-resolved or absent), so they never fail the gate.

---

## BUCKET 6 — Wave / sequencing recommendation

Recommended plan breakdown (4 plans, matching the request priority — groundScene first, smallest-blast-radius shims next, then conditional chatWindow, then the verified plain-`&fn` remainder + version/gate):

| Plan | Scope | Mechanism | Risk | Validation |
|---|---|---|---|---|
| **38-01 groundScene MI-thunks** | ctor + reloadTerrain + changeCamera(setView) + getCurrentCamera (free `__fastcall` thunks) + init/update/handleInputMapUpdate/handleInputMapEvent (in-TU GroundScene.cpp forwarders, private access) + g_instance decision (OMIT vs new accessor) | `__fastcall` thunks + Pattern-2 in-TU forwarders | MEDIUM — private methods need a GroundScene.cpp edit (new exe-visible forwarders); GroundScene.h is a shared engine header but the forwarders go in the .cpp (no ABI cascade) | Debug+Release 0 unresolved; dumpbin; self-check no-assert; boot gl05+gl11 |
| **38-02 client/config shims** | wndProc shim (Os.cpp, private+__stdcall) + writeMiniDump shim (DebugHelp.cpp) + setModalChat/getModalChat plain-&fn (CuiPreferences) + WR-05 commit (parked thunk already written) + OMIT writeCrashLog/setupStartDataInstall (NONEXISTENT) | Pattern-3 external shims + plain-&fn | LOW — small, mechanical; the WR-05 thunk just needs build+commit | same bar; confirm WR-05 `consoleHelper::sendInput` resolves non-null |
| **38-03 chatWindow MI-thunks** | enableTextInput(acceptTextInput) + writeToAllTabs(appendToAllTabs) + writeToCurrentTab(appendTextToCurrentTab) clean thunks + chatEnterHandler(performEnterKey) clean-entry + ctor DEFER decision | `__fastcall` thunks (triple-MI) | MEDIUM — name reconciliation + Issue #11 joint decision (advertise entry, defer mid-patch semantics) | same bar |
| **38-04 remaining plain-&fn + version bump + gate + EPA-08 handback** | per-symbol-confirmed Bucket-4 adds (small; confirm-or-OMIT) + `UTINNI_HOOKPOINTS_VERSION`→2 + `.inc`/`.h` re-sync to Utinni (hand-off) + EPA-08 DX11 evidence handoff-back doc | plain-&fn + doc | LOW — mostly classification + the dated handback doc (no DX11 code per D-01) | full self-check; dumpbin; EPA-08 doc dated + dumpbin/ordering proof transcribed from CONTEXT.md |

**Sequencing notes (AGENTS.md):** native MSBuild, `$env:MSBUILD`, run from PowerShell, `/nodeReuse:false`, **build serially**, grep log for 0 `unresolved external symbol`. Force relink by deleting the staged exe. Debug+Release is the bar (Optimized `_o` SAFESEH LNK1281 is pre-existing — 0 unresolved from our rows). Boot gate: client must reach char-select under rasterMajor=5 (gl05) AND not crash under =11 (gl11). EPA-08 is documentation-only (no code; the DX11 finding is LOCKED in CONTEXT.md).

---

## Common Pitfalls

### Pitfall 1: Calling a private/protected member from a free thunk
**What goes wrong:** `pThis->init(...)` in `utinni_advertise.cpp` → C2248 (init is private).
**Why:** `GroundScene::{init,update,handleInputMap*}` and `CommandParser::createDelegateCommands` are private/protected.
**How to avoid:** Author the forwarder in the defining class's `.cpp` (member access), expose external linkage, advertise that. The `utinni_installConfigFileOverride` precedent (ClientMain.cpp) is exactly this.
**Warning sign:** C2248 at the table-init line cascading into C2737/C2070.

### Pitfall 2: `__thiscall` on a free function
**What goes wrong:** C3865 — "__thiscall can only be used on native member functions."
**How to avoid:** `__fastcall(pThis /*ECX*/, int /*edx*/, args...)` — the dummy EDX makes it byte-identical to `__thiscall`. 37-03 hit this exact wall (deviation #3).
**Warning sign:** C3865 on any ctor/MI thunk.

### Pitfall 3: MI-class PMF trips the sizeof guard
**What goes wrong:** `pmfToVoid(&GroundScene::reloadTerrain)` → `static_assert sizeof(PMF)==sizeof(void*)` fails (C2338) because `NetworkScene`'s 2nd base inflates the PMF.
**How to avoid:** Use a `__fastcall` thunk for ALL GroundScene/SwgCuiChatWindow methods — never `pmfToVoid` on an MI class.
**Warning sign:** C2338 "inflated PMF (multiple/virtual inheritance) -- needs a thunk".

### Pitfall 4: Advertising a virtual as `&fn`
**What goes wrong:** `&GroundScene::draw` yields a vtable-dispatch stub, not the impl → Utinni detours the wrong code.
**How to avoid:** SKIP virtuals (keep out of `.inc`); Utinni resolves off the live vtable (spec §6). `draw` [GroundScene.h:204], `processEvent`, Appearance::render/collide, etc.
**Warning sign:** the method has `virtual` in its declaration.

### Pitfall 5: Guessing a spec symbol that doesn't exist in this tree
**What goes wrong:** Advertising `client::writeCrashLog` / `client::setupStartDataInstall` / `config::*` (as ConfigFile) → either a wrong-symbol detour or a link error.
**How to avoid:** D-04. grep the tree; if 0 hits → NONEXISTENT → OMIT + handoff-back. The spec §6 RVA list is SWGEmu Pre-CU, not this tree.
**Warning sign:** the only grep hits are in `.planning/` docs.

### Pitfall 6: Shared-header ABI cascade (AGENTS.md)
**What goes wrong:** If a forwarder declaration is added to a header consumed by the gl0X plugins, stale plugin DLLs crash at boot.
**How to avoid:** Keep `utinni_engine_hookpoints.h` exe-local (it already is — header comment line 23-24). Put GroundScene/Os/DebugHelp forwarder declarations in narrowly-scoped headers NOT pulled by the renderer plugins. Forwarder DEFINITIONS go in the engine `.cpp` (no ABI surface change to the struct).
**Warning sign:** touching `ShaderImplementation.h`-class shared headers; dll older than exe after a shared-header edit.

## Code Examples

### MI-class public method thunk (groundScene::reloadTerrain)
```cpp
// Source: utinni_advertise.cpp:109 pattern; GroundScene.h:215 (public, non-virtual, MI class)
static void __fastcall utinni_groundSceneReloadTerrain(GroundScene * pThis, int /*edx*/)
{
    pThis->reloadTerrain();
}
// table row: { "groundScene::reloadTerrain", (void*)&utinni_groundSceneReloadTerrain },
// Utinni typedef: void(__thiscall*)(GroundScene*)
```

### Private method forwarder in the defining TU (groundScene::init)
```cpp
// In GroundScene.cpp (member access to the private init); __fastcall = __thiscall ABI:
void __fastcall utinni_groundSceneInit(GroundScene * pThis, int /*edx*/,
    const char * terrainFilename, CreatureObject * player, float timeInSeconds)
{
    pThis->init(terrainFilename, player, timeInSeconds);   // legal: same TU as class def
}
// Declared extern in an exe-reachable forwarder header; advertised in the table.
// Utinni typedef: void(__thiscall*)(GroundScene*, const char*, CreatureObject*, float)
```

### Private __stdcall static shim (client::wndProc)
```cpp
// In Os.cpp (member access to private Os::WindowProc); preserve __stdcall/CALLBACK:
LRESULT CALLBACK utinni_osWindowProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    return Os::WindowProc(h, m, w, l);
}
// table row: { "client::wndProc", (void*)&utinni_osWindowProc },
// Utinni typedef: LRESULT(__stdcall*)(HWND,UINT,WPARAM,LPARAM)
```

### Plain-&fn public static (config::setModalChat — 37-02 correction)
```cpp
// Source: CuiPreferences.h:94-95 (public static; CuiPreferences.cpp:1267,1655)
{ "config::setModalChat", (void*)&CuiPreferences::setModalChat }, // MISMATCH: ours CuiPreferences::, NOT config/ConfigFile
{ "config::getModalChat", (void*)&CuiPreferences::getModalChat },
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|---|---|---|---|
| Hardcoded SWGEmu RVAs in UtinniCore | `&fn` advertisement table (this contract) | 37-01..03 | addresses correct-by-construction; survives rebuilds |
| `UTINNI_HOOKPOINTS_VERSION` pinned-at-1 (37-era) | bump-on-name-change (D-03) | Phase 38 | per CONTEXT.md D-03 — reconcile the header comment |
| 37-02 note: `config::setModalChat/getModalChat` "unresolved symbol" | they EXIST as `CuiPreferences::setModalChat/getModalChat` | this research | promotes 2 NONEXISTENT→plain-&fn |

**Deprecated/outdated in this tree (do not chase):** SWGEmu Pre-CU RVA-list symbols with no from-source twin — `client::writeCrashLog`, `client::setupStartDataInstall`, the mid-function-patch features (§8 #1), the offline-WorldSnapshot instance methods, virtual/inline catalog rows.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | `groundScene::g_instance` is best OMITted (Game::getScene is inline; no dedicated singleton) — UNLESS Utinni strictly needs the pointer, then add a non-inline Game accessor | Bucket 1 | If Utinni's groundScene editor needs the singleton pointer, OMIT degrades that editor; mitigated by flagging in handoff. Low — Utinni can also walk getScene's call sites or read ms_scene via a provided accessor. |
| A2 | `chatEnterHandler` → `performEnterKey` clean-entry advertisement is acceptable; Issue #11 mid-function semantics are a separate joint decision | Bucket 3 | If Utinni's Issue #11 strictly needs the mid-body NOP, the entry hook won't reproduce it; mitigated — CONTEXT.md already scopes mid-function patches OUT and routes them to joint decision. |
| A3 | `writeMiniDump`/`wndProc` are best done as in-TU shims rather than wiring the `src/win32` private include dirs | Bucket 2 | If a plan prefers wiring the include dir + direct `&`, that also works for the public `writeMiniDump` (not for the private `WindowProc`); shim is the safer default. Low. |
| A4 | The Bucket-4 plain-&fn add-list is SMALL (most spec "remaining ~30 subsystems" are SKIP/OMIT/DEFER/NONEXISTENT in this from-source tree) | Bucket 4 | If the plan adds spec rows on faith without per-symbol confirmation, risks wrong-&fn (D-04 violation). Mitigated by the "confirm-or-OMIT" rule. |
| A5 | Bumping `UTINNI_HOOKPOINTS_VERSION` to 2 (overriding the header's "pinned at 1" comment) is the correct read of D-03 | Bucket 5 | If Utinni's consumer hard-checks version==1, a soft mismatch warning fires (spec §3 says it resolves by name anyway). Low — the contract is name-keyed, version is advisory. |

## Open Questions

1. **groundScene::g_instance — OMIT or new accessor?**
   - What we know: no dedicated GroundScene singleton; reached via inline `Game::getScene()` cast to GroundScene.
   - What's unclear: whether Utinni's groundScene editor needs the raw singleton pointer.
   - Recommendation: OMIT for now; flag in handoff-back. If Utinni needs it, add a non-inline `Game` accessor in a follow-up (cheap).

2. **chatWindow::ctor — advertise or defer?**
   - What we know: MI ctor + required `UIPage&` arg; 37-03 already deferred it.
   - What's unclear: whether Utinni constructs chat windows through a page on the advertised client.
   - Recommendation: DEFER unless Utinni confirms; the 3 clean methods + chatEnterHandler cover the common path.

3. **EPA-08 handback doc — exact filename/location?**
   - What we know: CONTEXT.md D-01: dated handoff-back with dumpbin + create/destroy ordering proof (content already in CONTEXT.md "DX11 finding").
   - Recommendation: produce `.planning/handoff/2026-06-22-utinni-dx11-advertised-client-evidence.md` (or matching the consumer-status doc naming), transcribing the LOCKED DX11 finding; NO code.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| MSBuild (`$env:MSBUILD`, v145) | building all 5 targets | ✓ (per AGENTS.md, settings.json) | VS18/VS2026 | none — v145 hard requirement (VS2022 fails MSB8020) |
| `dumpbin` | export verification | ✓ (VS toolchain) | — | none needed |
| `D:/Code/Utinni/UtinniCore/swg/` | `.h`/`.inc` re-sync (hand-off) | ✓ (path exists per 37-03 sync) | — | manual copy; not a build dependency |

**Missing dependencies with no fallback:** none — this is a self-contained MSBuild phase.

## Validation Architecture

nyquist_validation is enabled (config.json `workflow.nyquist_validation: true`). This phase has NO automated unit-test framework (it's a native engine exe with no test harness); validation is build-gate + dumpbin + the in-binary coverage self-check + maintainer live-inject smoke.

### Test "Framework"
| Property | Value |
|----------|-------|
| Framework | none (native exe; no unit-test harness) — validation is the build gate + the compiled-in `utinni_verifyNoNullNoDup()` self-check |
| Config file | none |
| Quick run command | `dumpbin /exports stage/SwgClient_r.exe \| grep GetEngineHookPoints` |
| Full suite command | canonical 5-target build (Debug+Release) + boot smoke gl05/gl11 |

### Phase Requirements → Validation Map
| Req ID | Behavior | Validation Type | Command / Method | Exists? |
|--------|----------|-----------------|------------------|---------|
| EPA-05 | groundScene + chatWindow MI thunks resolve non-null | build + self-check | Debug build 0 unresolved; `utinni_verifyNoNullNoDup()` no-assert on Debug boot | ✅ self-check exists [utinni_advertise.cpp:371] |
| EPA-06 | client/config shims + WR-05 resolve non-null | build + self-check | same; confirm `consoleHelper::sendInput` + `client::wndProc` rows non-null | ✅ |
| EPA-07 | remaining adds + version bump + gate in lockstep | compile-time + runtime | `static_assert(rowcount==.inc count)` green [utinni_advertise.cpp:341]; name-set-equality at runtime | ✅ |
| EPA-08 | DX11 evidence handback | documentation | dated handoff doc transcribing the LOCKED dumpbin + create/destroy ordering (CONTEXT.md) | N/A (doc) |

### Sampling Rate
- **Per task commit:** Debug build of `/t:SwgClient` (or the 5-target if a shared `.cpp` changed) — grep log for 0 `unresolved external symbol`.
- **Per wave merge:** full 5-target Debug + Release; `dumpbin /exports` on `_d` and `_r` (undecorated `GetEngineHookPoints`); boot gl05 to char-select + gl11 window-up.
- **Phase gate:** all of the above green + the `.h`/`.inc` re-synced to Utinni + EPA-08 handback doc written. **Maintainer runs the live inject-smoke** (out of repo — Utinni injection → no `0xC0000005`, endpoints resolved by name).

### Wave 0 Gaps
- [ ] GroundScene.cpp forwarder header (exe-reachable, NOT plugin-shared) for the private-method forwarders — new file, narrow scope.
- [ ] Os.cpp / DebugHelp.cpp shim forward-declarations reachable from utinni_advertise.cpp (extern decl or tiny header).
- [ ] No test-framework install needed (none exists; the self-check IS the in-binary gate).

## Security Domain

security_enforcement enabled (config.json `security_enforcement: true`, ASVS level 1). This phase adds a **read-only, additive** export with NO untrusted-input path — the table is process-lifetime static, populated by `&fn` at compile time. The advertisement is inert unless Utinni is injected (the client never calls into Utinni; HARD CONSTRAINT, CONTEXT.md).

### Applicable ASVS Categories
| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | no auth surface |
| V3 Session Management | no | no sessions |
| V4 Access Control | no | export is read-only metadata; not a privilege boundary |
| V5 Input Validation | no | no input — table is compile-time static, `GetEngineHookPoints()` takes no args |
| V6 Cryptography | no | none |

### Known Threat Patterns for this stack
| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Wrong `&fn` detours wrong function (T-37-07) | Tampering | source-confirm every row (D-04); SKIP virtuals; thunk MI/ctors; OMIT unconfirmable; `/FORCE` log-grep 0 unresolved + static_assert + runtime no-null/no-dup |
| Mid-function byte-patch crossing the contract (T-37-09) | Tampering | contract holds only `{name,&fn-or-thunk}` rows; no offset arithmetic; §8 #1 mid-patches stay deferred SWGEmu-only |
| Exposing an attack surface via the export | Elevation/Info-disclosure | export is read-only metadata, inert without injection; client stays Utinni-agnostic (no inbound call path) |
| x64 export surface leak | Tampering | whole TU `#if !defined(_WIN64)` + vcxproj Platform=Win32 — no x64 export |

## Sources

### Primary (HIGH confidence — this-tree source, file:line confirmed this session)
- `src/.../scene/GroundScene.h` (MI topology, method access levels :77,168,170,173,199,204,207,212,215) + `network/NetworkScene.h:28-30` (the 2nd base) + `scene/Scene.h:21` (IoWin base)
- `src/.../page/SwgCuiChatWindow.h:58-61,106,112,172,174,214` (triple-MI; method name reconciliation)
- `src/.../win32/Os.h:127,138` (WindowProc private + __stdcall) + `win32/DebugHelp.h:36` (writeMiniDump public, private include dir)
- `src/.../core/CuiPreferences.h:94-95` + `CuiPreferences.cpp:1267,1655` (setModalChat/getModalChat exist — 37-02 correction)
- `src/.../core/Game.h(src/shared/core):105-106,271,306` (getScene inline, ms_scene private — no GroundScene singleton)
- `src/.../CommandParser.h:128-130,149,180` (ctors, addSubCommand public, createDelegateCommands protected)
- `utinni_advertise.cpp` (full: pmfToVoid, ctor thunks, WR-05 parked thunk via git diff, table, self-check, export) + `utinni_engine_hookpoints.{h,inc}` + `ClientMain.{h,cpp}` (shim precedent)
- `SwgClient.vcxproj:122` (include dirs — sharedCollision present, win32-private dirs absent)
- grep negative-confirmation: `writeCrashLog`, `setupStartDataInstall` = 0 source hits (NONEXISTENT)

### Secondary (HIGH — phase docs / locked decisions)
- `38-CONTEXT.md` (D-01..D-04, mechanism buckets, HARD CONSTRAINTS, DX11 finding LOCKED)
- `37-03-SUMMARY.md` (existing 78-row catalog, SKIP/OMIT/DEFER lists, the __fastcall/C3865/include-path lessons)
- `2026-06-20-...spec.md` §6 catalog + §8 open items; `2026-06-22-...coverage-status.md` §2 buckets
- `AGENTS.md` / `CLAUDE.md` (build/run/cfg conventions, shared-header ABI trap, /FORCE link-grep)

### Tertiary (LOW — none)
- No web/external sources needed; this is a closed-tree symbol-confirmation phase.

## Metadata

**Confidence breakdown:**
- Bucket 1 (groundScene): HIGH — MI topology + every method access level read from the header; the private-method forwarder requirement is the key non-obvious finding, source-confirmed.
- Bucket 2 (client/config): HIGH — wndProc private+__stdcall, writeMiniDump location, setModalChat correction, writeCrashLog/setupStartDataInstall NONEXISTENT all directly confirmed.
- Bucket 3 (chatWindow): HIGH — triple-MI + all 4 method name reconciliations read from the header; ctor defer rationale matches 37-03.
- Bucket 4 (plain-&fn remainder): MEDIUM — classified from 37-03's reconciliation + this session's spot-checks; the plan MUST per-symbol confirm any new add (flagged as "confirm-or-OMIT").
- Bucket 5 (version/gate): HIGH — mechanics read directly from the existing self-check code.
- Bucket 6 (sequencing): HIGH — derived from request priority + AGENTS.md build constraints.

**Research date:** 2026-06-22
**Valid until:** stable until the tree's GroundScene/CuiPreferences/Os/SwgCuiChatWindow headers change (~30 days; re-confirm file:line if those headers are edited).
