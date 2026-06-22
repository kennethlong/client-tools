# Phase 38: utinni-advertised-client-coverage-completion - Pattern Map

**Mapped:** 2026-06-22
**Files analyzed:** 7 work units across 4 mechanism buckets + 1 version/gate edit
**Analogs found:** 7 / 7 (every unit has an in-tree precedent; the phase is mechanical replication, not invention)

This is a native C++/MSBuild engine phase that EXTENDS existing files (no greenfield architecture).
Every "new file" is a tiny exe-local forwarder header; all real work is edits to existing TUs.
The single source of truth for the existing pattern is
`src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp` (37-01/02/03).

> **Working-tree note (verified this session):** the WR-05 thunk
> `utinni_consoleHelperSendInput` is ALREADY WRITTEN and the `consoleHelper::sendInput`
> row already points at it — both **uncommitted** in `utinni_advertise.cpp`
> (`git diff` confirmed: thunk at lines 121-147, row at line 221). Phase 38 only
> needs to **build + commit** it, not re-author it. `sendInput` is already in the
> `.inc` (line 109) and in the committed name-set, so WR-05 is version-neutral.

---

## File Classification

| Work unit | Role | Data Flow | Closest Analog | Match Quality | New file? |
|-----------|------|-----------|----------------|---------------|-----------|
| groundScene PUBLIC methods (reloadTerrain, setView, getCurrentCamera) → MI `__fastcall` thunk | thunk (provider) | request-response (detour target) | `utinni_commandParserCtor1/2` [utinni_advertise.cpp:109-119] | exact (MI-thiscall thunk) | edit `utinni_advertise.cpp` |
| groundScene `ctor` → MI placement-new `__fastcall` thunk | thunk (provider, ctor) | request-response | `utinni_commandParserCtor1` [utinni_advertise.cpp:109-113] | exact (ctor placement-new thunk) | edit `utinni_advertise.cpp` |
| groundScene PRIVATE methods (init, update, handleInputMapUpdate, handleInputMapEvent) → in-TU forwarder | shim/forwarder | request-response | `utinni_installConfigFileOverride` [ClientMain.cpp:122-125 / ClientMain.h:24] | role-match (in-TU forwarder for inaccessible target) | edit `GroundScene.cpp` + NEW narrow forwarder header |
| client::wndProc → `__stdcall` external-linkage shim in Os.cpp | shim | event-driven (window proc) | `utinni_installConfigFileOverride` [ClientMain.cpp:122-125] | role-match (external shim, file-local/private static) | edit `Os.cpp` + NEW narrow forwarder header |
| client::writeMiniDump → external-linkage shim in DebugHelp.cpp | shim | file-I/O (dump write) | `utinni_installConfigFileOverride` [ClientMain.cpp:122-125] | role-match (external shim, win32-private header) | edit `DebugHelp.cpp` (win32 TU) + NEW narrow forwarder header |
| config::setModalChat / getModalChat → plain `&fn` | table row | request-response (static getter/setter) | `game::install` row [utinni_advertise.cpp:165] | exact (plain static `&fn`) | edit `utinni_advertise.cpp` |
| chatWindow PUBLIC methods (acceptTextInput, appendToAllTabs, appendTextToCurrentTab, performEnterKey) → MI `__fastcall` thunk | thunk (provider) | request-response | `utinni_commandParserCtor1/2` [utinni_advertise.cpp:109-119] | exact (MI-thiscall thunk) | edit `utinni_advertise.cpp` |
| table row + `.inc` entry + count `static_assert` + `utinni_verifyNoNullNoDup()` + version bump | config/contract | — | the existing `.inc` + self-check + `UTINNI_HOOKPOINTS_VERSION` [utinni_advertise.cpp:154-323,334-443 / .h:40 / .inc] | exact (the contract is its own template) | edit `.inc`, `.h`, `utinni_advertise.cpp` |

**Mechanism decision rule (the one that drives every row):**
- single-inheritance class, non-virtual, public method → `pmfToVoid(&Class::member)` (plain PMF row)
- plain static / free function → `(void *)&Symbol` (with overload `static_cast` where needed)
- **MI class** (PMF inflated > `sizeof(void*)`) OR ctor → `__fastcall(pThis /*ECX*/, int /*EDX*/, args)` free thunk
- **private/protected** target (free thunk can't name it, C2248) → in-TU forwarder compiled in the defining `.cpp`
- **file-local / private static** → external-linkage shim in the defining `.cpp`
- virtual / inline / nonexistent → OMIT (kept out of `.inc`; Utinni resolves virtuals off the live vtable)

---

## Shared Patterns

### Pattern A — MI-class `__fastcall` == `__thiscall` thunk (the most-used mechanism this phase)
**Source:** `utinni_advertise.cpp:109-119` (`utinni_commandParserCtor1/2`).
**Apply to:** all groundScene PUBLIC + chatWindow PUBLIC methods (Buckets 1 & 3).
**Why:** MSVC v145 **forbids `__thiscall` on a free function (C3865)**. `__thiscall(pThis, a, b)`
emulates byte-identically as `__fastcall(pThis /*ECX*/, dummy /*EDX*/, a, b)` — `__fastcall`
puts arg1 in ECX and arg2 in EDX, so a dummy EDX makes the two ABIs identical with no asm.
Use a thunk (NOT `pmfToVoid`) for any GroundScene / SwgCuiChatWindow member: those classes are
multiple-inheritance, so the PMF is inflated and trips the `sizeof(PMF)==sizeof(void*)`
`static_assert` (C2338) at `utinni_advertise.cpp:67`.

```cpp
// Analog: utinni_advertise.cpp:109-113 (single-inheritance ctor; same ABI trick for MI methods)
static CommandParser * __fastcall utinni_commandParserCtor1(CommandParser * pThis, int /*edx*/,
	const char * command, size_t argCount, const char * args, const char * helpInfo, CommandParser * delegate)
{
	return ::new (static_cast<void *>(pThis)) CommandParser(command, argCount, args, helpInfo, delegate);
}
// table row: { "commandParser::ctor1", (void *)&utinni_commandParserCtor1 },
```
**Landmine:** for an MI *ctor* (groundScene::ctor, chatWindow::ctor) the placement-new `this`
must be the **most-derived** pointer (the injector passes it); MI also means the thunk body's
`::new (pThis) GroundScene(...)` constructs the full object. chatWindow::ctor additionally needs
a live `UIPage&` arg → **DEFER** (37-03 already deferred it; see Bucket 3).

### Pattern B — external-linkage forwarder/shim in the defining TU (for private/file-local targets)
**Source:** `ClientMain.cpp:122-125` + declared in `ClientMain.h:24`.
**Apply to:** groundScene PRIVATE methods (Bucket 1), client::wndProc + client::writeMiniDump (Bucket 2).
**Why:** a free thunk in `utinni_advertise.cpp` **cannot name a private/protected member (C2248)**
nor a file-local static. The forwarder is compiled **inside the class's own `.cpp`** (member
access), given a distinctly-named (`utinni_*`) external-linkage symbol, declared in a narrow
exe-reachable header, and advertised in the table.

```cpp
// Analog: ClientMain.cpp:122-125 — distinctly-named external forwarder for a file-local target
void utinni_installConfigFileOverride()
{
	ClientMainNamespace::installConfigFileOverride();   // legal: same TU; private/file-local visible here
}
// Declared in ClientMain.h:24:  void utinni_installConfigFileOverride();
// Wrapped in utinni_advertise.cpp:80-84 by a thunk if the contract typedef differs:
//   static int __cdecl utinni_loadOverrideConfig() { utinni_installConfigFileOverride(); return 0; }
```

### Pattern C — plain static `&fn` row (Bucket 2 config, Bucket 4 remainder)
**Source:** `utinni_advertise.cpp:165` (`game::install`) — and the overload-cast variant at :168.
**Apply to:** `config::setModalChat`/`getModalChat` (→ `CuiPreferences::`), any confirmed Bucket-4 add.
```cpp
{ "game::install", (void *)&Game::install },   // plain static [Game.h:94]
// overloaded → static_cast to the exact signature, e.g. utinni_advertise.cpp:168:
{ "game::setupScene", (void *)static_cast<void(*)(bool,const char*,const char*,CreatureObject*)>(&Game::setScene) },
```

### Pattern D — contract lockstep (table row ⟺ `.inc` name ⟺ confirmed `&fn`)
**Source:** the X-macro `.inc` + the 3-part self-check at `utinni_advertise.cpp:334-443` + version at `.h:40`.
**Apply to:** every name add this phase.
- Compile-time count smoke: `static_assert((sizeof table/sizeof[0]) == UTINNI_REQUIRED_COUNT)` [utinni_advertise.cpp:341] — `.inc` expands to `+1` per name [utinni_advertise.cpp:337-339]. Add a name to `.inc` ⇒ must add exactly one table row or this fails.
- Runtime `utinni_verifyNoNullNoDup()` [utinni_advertise.cpp:371-443]: (a) no null addr, (b) no dup name, (c) name-set equality vs the X-macro `s_requiredNames[]`. Runs Debug-boot-only [ClientMain.cpp:258-260].
- **NO null-pair sentinel row; count is sizeof/sizeof with NO -1** (pinned 2026-06-21, utinni_advertise.cpp:150-152, 315, 321).
- **Version (D-03):** bump `UTINNI_HOOKPOINTS_VERSION` 1 → 2 on any name add/remove, AND rewrite the header's "PINNED at 1 / do NOT bump per wave" comment [.h:34-40] to the new bump-on-change policy (37-era comment is overridden by CONTEXT.md D-03).

---

## Pattern Assignments

### BUCKET 1 — `src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp` + `.../clientGame/src/shared/scene/GroundScene.cpp` (groundScene::*)

**Class topology (CONFIRMED this session):** `GroundScene : public NetworkScene` [GroundScene.h(src/shared/scene):46];
`NetworkScene : public Scene, public MessageDispatch::Receiver` → **MULTIPLE INHERITANCE** → every PMF inflated → thunk required (never `pmfToVoid`).
**Public header reach:** `#include "clientGame/GroundScene.h"` — the public stub
[`clientGame/include/public/clientGame/GroundScene.h`] one-line-redirects to the real header;
`clientGame\include\public` is already on all 5 SwgClient include blocks (vcxproj:122) — **no vcxproj change**.

**Sub-bucket 1a — PUBLIC methods → free `__fastcall` thunk in `utinni_advertise.cpp` (Pattern A):**

| contract name | engine symbol | file:line | exact signature |
|---|---|---|---|
| `groundScene::reloadTerrain` | `GroundScene::reloadTerrain()` | GroundScene.h:215 | `void reloadTerrain()` — public, non-virtual |
| `groundScene::changeCamera` | `GroundScene::setView(int,float=0.f)` | GroundScene.h:207 | `void setView(int, float=0.f)` — NAME MISMATCH (contract `changeCamera`, note in row comment) |
| `groundScene::getCurrentCamera` | `GroundScene::getCurrentCamera()` | GroundScene.h:212 | `GameCamera* getCurrentCamera()` — OVERLOADED (const sibling :213); thunk calls the non-const on a non-const `this`, no cast |
| `groundScene::ctor` | `GroundScene::GroundScene(const char*,const char*,CreatureObject*)` | GroundScene.h:199 | placement-new thunk (a 2nd NetworkId overload exists at :200); most-derived `this` |

Template excerpt for these (copy from `utinni_commandParserCtor1`, drop the ctor body for plain methods):
```cpp
static void __fastcall utinni_groundSceneReloadTerrain(GroundScene * pThis, int /*edx*/)
{
	pThis->reloadTerrain();   // public, member access fine in this TU
}
// row: { "groundScene::reloadTerrain", (void *)&utinni_groundSceneReloadTerrain },
// Utinni typedef: void(__thiscall*)(GroundScene*)
```

**Sub-bucket 1b — PRIVATE methods → forwarder in `GroundScene.cpp` (Pattern B), THEN advertised:**

| contract name | engine symbol | file:line (access) | exact signature |
|---|---|---|---|
| `groundScene::init` | `GroundScene::init(const char*,CreatureObject*,float)` | GroundScene.h:173 (**private**) | `void init(const char*, CreatureObject*, float)` |
| `groundScene::update` | `GroundScene::update(float)` | GroundScene.h:77 (**private**) | `void update(float)` |
| `groundScene::handleInputMapUpdate` | `GroundScene::handleInputMapUpdate()` | GroundScene.h:170 (**private**) | `void handleInputMapUpdate()` |
| `groundScene::handleInputMapEvent` | `GroundScene::handleInputMapEvent(IoEvent*)` | GroundScene.h:168 (**private**) | `void handleInputMapEvent(IoEvent*)` |

These are **private** [confirmed GroundScene.h:70 `private:` block opens before all four] — a free thunk
in `utinni_advertise.cpp` would hit **C2248**. Author each forwarder **in GroundScene.cpp** (member
access) with the `__fastcall(GroundScene*, int /*edx*/, args)` signature directly — that single
function is BOTH in-TU (member access) AND ABI-correct for the MI class. Expose via a NEW narrow header.

```cpp
// In GroundScene.cpp (has private access to ::init); __fastcall == __thiscall ABI:
void __fastcall utinni_groundSceneInit(GroundScene * pThis, int /*edx*/,
	const char * terrainFilename, CreatureObject * player, float timeInSeconds)
{
	pThis->init(terrainFilename, player, timeInSeconds);   // legal: same TU as the class def
}
// Utinni typedef: void(__thiscall*)(GroundScene*, const char*, CreatureObject*, float)
```
**NEW FILE:** a narrow forwarder header (e.g. `utinni_groundScene_forward.h`) declaring the four
`__fastcall` forwarders, reachable from `utinni_advertise.cpp`. **Keep it exe-local / NOT pulled by
gl0X plugins** (AGENTS.md shared-header ABI cascade trap; the forwarder DECLARATIONS are new, the
DEFINITIONS live in GroundScene.cpp so the `GroundScene` struct ABI is untouched).

**SKIP / OMIT in Bucket 1:**
- `groundScene::draw` — **virtual** [GroundScene.h:204] → SKIP (Utinni resolves off the live vtable). Already SKIP-listed [utinni_advertise.cpp:299].
- `groundScene::g_instance` — no dedicated singleton; reached via **inline** `Game::getScene()` (no ODR address) → **OMIT** (A1); flag in handoff. Alt: add a non-inline `Game` accessor only if Utinni needs the raw pointer.

---

### BUCKET 2 — `Os.cpp` + `DebugHelp.cpp` (win32) + `utinni_advertise.cpp` (client::* + config::*)

**Sub-bucket 2a — `client::wndProc` → external `__stdcall` shim in `Os.cpp` (Pattern B):**
- Target: `Os::WindowProc` — **private** [`Os.h(src/win32):127 private:` … `:138`] AND **`__stdcall`/CALLBACK**.
- `Os.h` lives in `sharedFoundation/src/win32` — **NOT a public include dir** (vcxproj:122 has `sharedFoundation\include\public` only) → the shim MUST be compiled in `Os.cpp`; `utinni_advertise.cpp` only sees the shim's extern declaration.
```cpp
// In Os.cpp (member access to private Os::WindowProc); PRESERVE __stdcall/CALLBACK:
LRESULT CALLBACK utinni_osWindowProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
	return Os::WindowProc(h, m, w, l);
}
// row: { "client::wndProc", (void *)&utinni_osWindowProc },
// Utinni typedef: LRESULT(__stdcall*)(HWND,UINT,WPARAM,LPARAM)
```
**Landmine:** `__stdcall` (CALLBACK) — NOT the `__thiscall`/`__fastcall` trick. Keep CALLBACK exactly.

**Sub-bucket 2b — `client::writeMiniDump` → external shim in `DebugHelp.cpp` (Pattern B):**
- Target: `DebugHelp::writeMiniDump(char const*=0, PEXCEPTION_POINTERS=0)` — **public static** [`DebugHelp.h(src/win32):36`], but the win32 `DebugHelp.h` is in `sharedDebug/src/win32` (private dir). (Note: `sharedDebug/include/public/sharedDebug/DebugHelp.h` exists but does NOT declare `writeMiniDump`.) → use an in-TU shim (avoids exposing the win32-private header to the exe; consistent with wndProc).
```cpp
// In DebugHelp.cpp (win32 TU; sees the win32 DebugHelp.h):
bool utinni_writeMiniDump(char const * fileName, PEXCEPTION_POINTERS ep)
{
	return DebugHelp::writeMiniDump(fileName, ep);
}
// row: { "client::writeMiniDump", (void *)&utinni_writeMiniDump },
```
**NEW FILE(S):** narrow exe-reachable forwarder header(s) for `utinni_osWindowProc` /
`utinni_writeMiniDump` (or `extern` decls in a tiny shared shim header). Keep exe-local.

**Sub-bucket 2c — `config::setModalChat` / `getModalChat` → plain `&fn` (Pattern C) — 37-02 CORRECTION:**
- These are **NOT** on `config`/`ConfigFile` (the 37-02 "unresolved symbol" note was WRONG). They are public statics on **`CuiPreferences`** [`CuiPreferences.h:94-95`, confirmed], reachable via `clientUserInterface\include\public` (already on path).
```cpp
// CuiPreferences.h:94-95 (public static; CuiPreferences.cpp:1655/1267)
{ "config::setModalChat", (void *)&CuiPreferences::setModalChat }, // MISMATCH: ours CuiPreferences::, NOT config/ConfigFile
{ "config::getModalChat", (void *)&CuiPreferences::getModalChat },
```
Add `#include "clientUserInterface/CuiPreferences.h"` to `utinni_advertise.cpp` (already-wired lib).

**NONEXISTENT in Bucket 2 (grep = 0 source hits) → OMIT + handoff-back (D-04, Pitfall 5):**
- `client::writeCrashLog` — crash `.txt` is written INLINE by `SetupSharedFoundation` [SetupSharedFoundation.cpp:92 `sprintf`], no named function. OMIT.
- `client::setupStartDataInstall` — SWGEmu Pre-CU concept, no twin. OMIT.

---

### BUCKET 3 — `utinni_advertise.cpp` (cui::chatWindow::*)

**Class topology (CONFIRMED):** `SwgCuiChatWindow : public SwgCuiLockableMediator, public UINotification, public MessageDispatch::Receiver` [SwgCuiChatWindow.h:58-61] → **TRIPLE INHERITANCE** → PMF inflated → thunk required.
**Header:** `#include "swgClientUserInterface/SwgCuiChatWindow.h"` (public stub → `src/shared/page/SwgCuiChatWindow.h`); `swgClientUserInterface\include\public` is on the path (vcxproj:122). Spec method names DON'T match this tree — reconciled below.

**Clean PUBLIC methods → `__fastcall` thunk (Pattern A), 3 rows:**

| contract name | engine symbol | file:line | exact signature |
|---|---|---|---|
| `cui::chatWindow::enableTextInput` | `SwgCuiChatWindow::acceptTextInput(bool,bool,bool)` | SwgCuiChatWindow.h:112 | `void acceptTextInput(bool, bool=true, bool=false)` — NAME MISMATCH |
| `cui::chatWindow::writeToAllTabs` | `SwgCuiChatWindow::appendToAllTabs(const Unicode::String&)` | SwgCuiChatWindow.h:172 | `void appendToAllTabs(const Unicode::String&)` — NAME MISMATCH |
| `cui::chatWindow::writeToCurrentTab` | `SwgCuiChatWindow::appendTextToCurrentTab(const Unicode::String&)` | SwgCuiChatWindow.h:174 | `void appendTextToCurrentTab(const Unicode::String&)` — NAME MISMATCH |

**Conditional row:**
- `cui::chatWindow::chatEnterHandler` → `SwgCuiChatWindow::performEnterKey()` [SwgCuiChatWindow.h:214], public, clean function entry. Advertise the clean entry address (it's a real `&fn` via thunk); flag in handoff that Issue #11's **mid-function** chat-routing NOP is a SEPARATE, SWGEmu-only joint decision (CONTEXT.md OUT-OF-SCOPE; A2).

**DEFER:**
- `cui::chatWindow::ctor` — `SwgCuiChatWindow(UIPage&, Game::SceneType, std::string const&)` [SwgCuiChatWindow.h:106] — MI ctor + **required live `UIPage&`** the injector must supply. 37-03 already deferred this exact ctor. DEFER unless Utinni confirms it constructs through a page.

Thunk template (same as Pattern A; triple-MI → never `pmfToVoid`):
```cpp
static void __fastcall utinni_chatWindowAcceptTextInput(SwgCuiChatWindow * pThis, int /*edx*/,
	bool b, bool setKeyboardInput, bool unfocusMediator)
{
	pThis->acceptTextInput(b, setKeyboardInput, unfocusMediator);
}
// row: { "cui::chatWindow::enableTextInput", (void *)&utinni_chatWindowAcceptTextInput },
```

---

### BUCKET 4 — `utinni_advertise.cpp` + `.inc` (remaining plain `&fn`)

**The add-list is SMALL.** 37-03 already advertised the bulk of the plain-`&fn` non-MI/non-virtual
full-set; the spec §6 "~30 remaining subsystems" are overwhelmingly already SKIP (virtual) / OMIT
(inline) / DEFER (MI ctor) / NONEXISTENT in this from-source tree (the spec catalog is the SWGEmu
Pre-CU RVA list, not this tree's symbol set). **Rule: confirm-or-OMIT per symbol — never add on spec
faith (D-04).** Representative classification (source-confirmed in 37-03 / this session):

| spec endpoint(s) | classification | reason |
|---|---|---|
| `object::addToWorld/removeFromWorld/setParentCell` | SKIP: virtual [Object.h:120-121,165] | vtable-resolved |
| `object::move_o` | OMIT: inline [Object.h:1216] | no ODR address |
| `commandParser::createDelegateCommands` | OMIT: protected [CommandParser.h:180] | would need in-TU forwarder; not needed |
| `camera` getViewport* | OMIT: inline [Camera.h:210-258] | no ODR address |
| appearance/portal/terrain/renderWorld/clientWorld | mostly SKIP: virtual / verify | per-symbol header confirm before any add |
| UI mediator ctors (hud/loginScreen/gameMenu/radialMenu/~20 UI controls) | DEFER: MI ctors | each needs its own placement-new thunk; Utinni resolves via RVA today |
| read-only globals (player health/hud view-distance/weather/static-shader) | OMIT: private, no non-inline accessor | never take a private-member address (§8 #3) |

Any genuinely new plain-`&fn` add follows **Pattern C** (`(void*)&Symbol` with overload `static_cast`).
**Include-path trap:** if a new add `#include`s a library not already on the path, add that library's
`include\public` to **all 5** SwgClient config blocks (vcxproj — T-37-01 lesson). Already wired:
clientGame, clientGraphics, clientUserInterface, sharedCommandParser, sharedCollision, sharedObject,
sharedGame, sharedMemoryManager, clientAudio, sharedFile, sharedDebug(public), sharedFoundation(public),
swgClientUserInterface. `src/win32` private dirs are NEVER on the path — that is exactly why Buckets 2a/2b use in-TU shims.

---

### BUCKET 5 — contract version + coverage gate (Pattern D)

**Files:** `utinni_engine_hookpoints.inc`, `utinni_engine_hookpoints.h`, `utinni_advertise.cpp`.

| mechanic | current state | this phase |
|---|---|---|
| `UTINNI_HOOKPOINTS_VERSION` | **1** [.h:40], comment says "PINNED at 1, do NOT bump per wave" [.h:34-40] | **bump → 2** (D-03 overrides the 37-era comment); rewrite the comment to bump-on-name-change |
| count `static_assert` | `(sizeof table/sizeof[0]) == UTINNI_REQUIRED_COUNT` [utinni_advertise.cpp:341], NO -1 | stays in lockstep: each `.inc` add = +1 required count = +1 table row, else compile fails |
| runtime `utinni_verifyNoNullNoDup()` | 3-part no-null/no-dup/name-set-equality [utinni_advertise.cpp:371-443] | every new name in BOTH `.inc` AND table exactly once; runs Debug-boot-only [ClientMain.cpp:258-260] |
| `.h`+`.inc` byte-identical re-sync into `D:/Code/Utinni/UtinniCore/swg/` | done at 37-03 (78 names, v1) | **HAND-OFF step at wave close** — re-copy verbatim; the phase BUILD must NOT write to `D:/Code/Utinni` (CONTEXT.md) |

Each new advertised name → add `UTINNI_HOOKPOINT(group, name)` to `.inc` AND a matching row to the table body in `utinni_advertise.cpp`. SKIP-virtual / OMIT-inline / NONEXISTENT names stay OUT of `.inc` (they are not "missing required rows").

---

### EPA-08 — DX11 evidence handback (documentation only, no code)

**No analog/code.** D-01 LOCKED: no provider change. Produce a dated handoff-back doc (e.g.
`.planning/handoff/2026-06-22-utinni-dx11-advertised-client-evidence.md`) transcribing the LOCKED
DX11 finding from CONTEXT.md (dumpbin `GetHookPoints` undecorated on gl11_r/_d; create/destroy
ordering proof: device+context first [Direct3d11_Device.cpp:435], swapChain last [:574], destroy
resets swapChain first [:684] ⇒ no window where swapChain≠null && (device==null||context==null)).

---

## New Files vs Edits Summary

**NEW files (all tiny, exe-local forwarder headers — must NOT be pulled by gl0X plugins):**
- groundScene private-method forwarder header (declares the 4 `__fastcall` forwarders defined in GroundScene.cpp)
- client shim forwarder header(s) for `utinni_osWindowProc` (Os.cpp) + `utinni_writeMiniDump` (DebugHelp.cpp) — or `extern` decls in one tiny shim header

**EDITS to existing files:**
- `src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp` — new thunks (groundScene public + chatWindow public), new rows (all buckets), `#include "clientUserInterface/CuiPreferences.h"` + groundScene/chatWindow public headers + the new forwarder headers; commit the already-written WR-05 thunk
- `src/engine/client/library/clientGame/src/shared/scene/GroundScene.cpp` — 4 private-method `__fastcall` forwarders (in-TU member access)
- `src/engine/shared/library/sharedFoundation/src/win32/Os.cpp` — `utinni_osWindowProc` `__stdcall` shim
- `src/engine/shared/library/sharedDebug/src/win32/DebugHelp.cpp` — `utinni_writeMiniDump` shim
- `src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.inc` — new `UTINNI_HOOKPOINT` rows
- `src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.h` — `UTINNI_HOOKPOINTS_VERSION` 1→2 + comment rewrite

**NO vcxproj change expected** (all needed `include\public` dirs already on all 5 blocks; the new
GroundScene.cpp / Os.cpp / DebugHelp.cpp edits are to files already in their library `.vcxproj`s).
GroundScene.cpp is in clientGame.lib, Os.cpp in sharedFoundation.lib, DebugHelp.cpp in sharedDebug.lib
— their forwarders link into SwgClient via the existing lib dependency chain (no new exe sources to add).

---

## No Analog Found

None. Every work unit maps to an existing in-tree precedent (the phase is mechanical replication of
the three established mechanisms — `__fastcall` thunk, in-TU forwarder/shim, plain `&fn` — plus the
contract lockstep). The only "new" artifacts are tiny forwarder headers, themselves modeled on
`ClientMain.h:24`.

---

## Critical Landmines (cross-cutting, apply to all plans)

1. **C3865** — `__thiscall` on a free function is illegal. Use `__fastcall(pThis, int /*edx*/, args)`. [utinni_advertise.cpp:93-100]
2. **C2338** — `pmfToVoid(&MIClass::member)` trips the sizeof guard. GroundScene + SwgCuiChatWindow are MI → ALWAYS thunk, NEVER `pmfToVoid`. [utinni_advertise.cpp:67]
3. **C2248** — calling a private/protected member from `utinni_advertise.cpp`. GroundScene init/update/handleInputMap* are private → forwarder in GroundScene.cpp. [GroundScene.h:70 private block]
4. **`__stdcall` preservation** — `Os::WindowProc` is `CALLBACK`/`__stdcall`; the shim must keep CALLBACK (not the fastcall trick). [Os.h:138]
5. **win32-private include dirs** — `Os.h`, win32 `DebugHelp.h` are in `src/win32`, NOT on the exe include path → shims live in the defining `.cpp`, exe sees only the extern decl. [vcxproj:122]
6. **Shared-header ABI cascade** (AGENTS.md) — keep new forwarder DECLARATION headers exe-local; do NOT add them to any header the gl0X plugins compile. Forwarder DEFINITIONS in `.cpp` don't change struct ABI.
7. **D-04 confirm-or-OMIT** — never advertise a spec symbol that greps to 0 source hits (`writeCrashLog`, `setupStartDataInstall`) or is virtual/inline. OMIT-with-reason.
8. **Version lockstep (D-03)** — name add ⇒ bump `UTINNI_HOOKPOINTS_VERSION` to 2 + re-sync `.h`/`.inc` to `D:/Code/Utinni` (hand-off, not build).
9. **Build (AGENTS.md)** — `$env:MSBUILD` (v145), PowerShell, `/nodeReuse:false`, build serially (no `/m`). Delete staged exe to force relink. **Grep the log for 0 `unresolved external symbol`** (SwgClient links under /FORCE). Debug+Release is the bar; Optimized `_o` LNK1281 SAFESEH is pre-existing (0 unresolved from our rows). Boot gl05 to char-select + gl11 window-up.
