# Utinni Bucket A HANDBACK — per-editor real-entry detour rows (ledger §2.A)

**Status:** DONE, build-gated, committed + pushed to `origin/master`. **Awaiting maintainer inject-smoke.**
**Date:** 2026-06-28
**Commit:** `ef073dca2` (`feat(enginehook): Utinni Bucket A v9 -- per-editor real-entry detour rows`)
**Contract:** `ENGINE_HOOKPOINTS_VERSION` **8 → 9, 111 names** (+6 NAME ADDs).

The 14 requested §2.A rows were source-mapped via parallel subsystem reads. **6 are real-entry
addressable and advertised; 8 are OMIT/SKIP** (each accounted for; nothing guessed — a wrong `&` is
worse than a missing row). Full disposition below.

---

## v9 re-sync sha256 (LF working-copy bytes — copy the working tree, not a CRLF checkout)

Re-copy these two byte-identical into `D:/Code/Utinni/UtinniCore/swg/` and sha256-verify:

| file | sha256 (LF) |
|---|---|
| `engine_hookpoints.h`   | `a7205de2f8ce0ebdaf18239605d29b5a1fb5052e9002793c4b462853a2022290` |
| `engine_hookpoints.inc` | `b2f178e6ce0c2e48966f82a9df8e7fcd748bda05f0eb7248e1e61a9b67e8f523` |

(`engine_advertise.cpp` is provider-side only — not re-synced.)

---

## ADVERTISED (6) — name → real engine symbol → mechanism → editor unblocked

| Contract name | Real engine symbol (file:line) | Mechanism | Unblocks |
|---|---|---|---|
| `cuiRadialMenuManager::update` | `CuiRadialMenuManager::update()` static [CuiRadialMenuManager.h:46] | constant `(void*)&fn` | **Radial-menu** editing |
| `cuiMenu::infoTypesFindDefaultCursor` | `Cui::MenuInfoTypes::findDefaultCursor(ClientObject&)` **namespace free fn** [CuiMenuInfoTypes.h:199 / .cpp:404] | constant `(void*)&fn` | Menu cursor behavior |
| `systemMessageManager::receiveMessage` | `CuiSystemMessageManager::receiveSystemMessage(const ChatSystemMessage&)` static [CuiSystemMessageManager.h:36] *(MISMATCH name)* | constant `(void*)&fn` | **System-message** editor/diag |
| `creatureObject::setTarget` | `CreatureObject::setLookAtTarget(const NetworkId&)` [CreatureObject.h:311] *(MISMATCH name)* | real entry via `utinni_creatureSetTargetRealEntry()` (MI, delta==0) | **Target-change** callback |
| `messageQueue::appendMessage` | `MessageQueue::appendMessage(int,float,uint32)` [MessageQueue.h:51] | dyn[] `pmfToVoid` + static_cast | **INPUT-path** diag |
| `messageQueue::appendMessageData` | `MessageQueue::appendMessage(int,float,Data*,uint32)` [MessageQueue.h:52] *(MISMATCH name)* | dyn[] `pmfToVoid` + static_cast | INPUT-path diag |

### MISMATCH-name notes (the contract name is the consumer's lookup key; the `&` is the real entry)
- **`systemMessageManager::receiveMessage` → `receiveSystemMessage`.** `CuiSystemMessageManager` is a
  flat class that does NOT derive from `MessageDispatch::Receiver`; the only `receiveMessage` is a
  file-local anon-namespace `Listener` that just forwards to the static `receiveSystemMessage` — that
  static is the real handle entry. (A static injector `sendFakeSystemMessage(Unicode::String,bool)`
  also exists if the editor later wants to *inject* messages — offered, not advertised this wave.)
- **`creatureObject::setTarget` → `setLookAtTarget`.** ⚠️ **Maintainer: verify the consumer typedef.**
  There is NO `CreatureObject::setTarget` in this tree. `setLookAtTarget(const NetworkId&)` is the
  "current target" setter (`m_lookAtTarget` = "this creature's current target"). If the consumer's
  `setTarget` detour expects a different semantic, the alternates are `setIntendedTarget(const
  NetworkId&)` [:317] and `setLookAtAndIntendedTarget(...)` [:318] — re-point the accessor in
  `CreatureObject.cpp` and rebuild (no contract change). Mapped because the semantic match is strong
  and it's the documented MISMATCH-name pattern; reversible.
- **`messageQueue::appendMessageData` → `appendMessage(…,Data*,…)`.** No `appendMessageData` exists;
  the data-carrying overload of `appendMessage` is the equivalent. Both overloads are advertised (the
  3-arg as `appendMessage`, the 4-arg/Data as `appendMessageData`).

### `creatureObject::setTarget` address-provider mechanism (note for future heavy-header rows)
`engine_advertise.cpp` **cannot** `#include "clientGame/CreatureObject.h"` — it transitively pulls
`sharedSkillSystem/SkillObjectArchive.h`, whose include dir is on the **clientGame** project path but
NOT the **SwgClient (exe)** project path (`error C1083`). So the PMF real-entry is computed by an
out-of-line accessor `utinni_creatureSetTargetRealEntry()` compiled in `CreatureObject.cpp` (the
class's own TU, where the header builds), declared in the new exe-local
`utinni_creatureObject_forward.h`. This mirrors the `utinni_groundScene*RealEntry()` accessors in
`GroundScene.cpp`. `setLookAtTarget` is **public**, so no friend grant and **CreatureObject.h is
unchanged** (no shared-header ABI cascade). The accessor hard-gates the MI-PMF `delta==0` and returns
nullptr otherwise → `utinni_verifyNoNullNoDup()` fails loudly rather than advertising a wrong entry.

---

## OMIT / SKIP ledger (8) — name → why → consumer alternative

| Contract name | Disposition | Why | Consumer alternative |
|---|---|---|---|
| `cuiChatWindow::ctor` | **OMIT (ctor)** | Cannot take `&Class::Class`; a placement-new thunk is detour-dead | The SOLE construction funnel `cuiChatWindow::createNewWindow` is **already advertised** (v4; sole `new SwgCuiChatWindow` @ `SwgCuiChatWindow.cpp:1549`, `createInto` routes through it) — detour that to cover construction |
| `cuiLoginScreen::ctor` | **OMIT (ctor)** | Un-addressable ctor AND **no funnel** — 0 `new SwgCuiLoginScreen`; built only via the generic `CuiMediatorFactory::Constructor<T>` template (`new T(page)` @ `CuiMediatorFactory_Constructor.h:49`) | Consumer resolves via RVA (already DEFER'd). A login-specific placement-new `__fastcall` thunk + injector-supplied `UIPage&` is the only source hook — not justified now |
| `cuiManager::findObjectUnderCursor` | **OMIT (absent)** | No such member on the all-static `CuiManager` [CuiManager.h:26]; retail collapses this + `cuiHud::getTarget` onto one world-pick RVA with no 1:1 named twin here | `SwgCuiHud::getLastSelectedObject() const` [SwgCuiHud.h:95] (MI → would need a `__fastcall` thunk) — offered, not bound |
| `cuiHud::getTarget` | **OMIT (absent)** | No such member on `SwgCuiHud` (shares the retail world-pick RVA with `findObjectUnderCursor`) | `SwgCuiHud::getLastSelectedObject() const` [SwgCuiHud.h:95] |
| `cuiHud::actionPerformAction` | **SKIP virtual** | → `SwgCuiHudAction::performAction(...) const` is `virtual` [SwgCuiHudAction.h:24] | Consumer vtable-resolves off the live `SwgCuiHudAction` |
| `cuiHud::update` | **SKIP virtual** | `SwgCuiHud::update(float)` is `virtual` [SwgCuiHud.h:63], overrides `CuiMediator::update` [CuiMediator.h:186] | Consumer vtable-resolves off the live `SwgCuiHud` |
| `cuiLoginScreen::activate` | **SKIP virtual** | The login-specific work is `SwgCuiLoginScreen::performActivate()` `virtual` [SwgCuiLoginScreen.h:42]. The non-virtual `CuiMediator::activate` [CuiMediator.h:100] is generic (all mediators), not login-specific | Consumer vtable-resolves `performActivate` off the live `SwgCuiLoginScreen` |
| `debugCamera::alter` | **SKIP virtual** | `alter` is virtual at every level — `Object::alter` [Object.h:135] → `GameCamera::alter` [GameCamera.h:40] → `FreeCamera::alter` [FreeCamera.h:61]; no non-virtual real-entry helper (per-frame body is in the virtual `FreeCamera::alter` @ `FreeCamera.cpp:254`) | Consumer vtable-resolves `alter` off the live **`FreeCamera`** instance (free-cam target class; `DebugPortalCamera` is the portal-debug alternate) |

> The two highest-value rows (chat + free-cam, per the priority hint) land as **OMIT** (chat ctor —
> funnel already covers it) and **SKIP** (free-cam — virtual, consumer vtable-resolves). Both are the
> correct, honest dispositions, not blockers: chat construction is already hookable via
> `createNewWindow`, and free-cam is the standard virtual-vtable resolution Utinni already does for
> D3D9 `Present`.

---

## Build gate (passed)

- **Release/Win32**, `$env:MSBUILD`, `/nodeReuse:false`, exe deleted to force relink.
- **0** `unresolved external symbol`, **0** LNK1120/LNK1181, **0** `error C…`.
- X-macro count gate (`UTINNI_REQUIRED_COUNT` static_assert): **111 == 111** (table rows == `.inc` names).
- Undecorated export confirmed: `GetEngineHookPoints = _GetEngineHookPoints`.
- Staged `SwgClient_r.exe` (28,456,960 bytes, 2026-06-28 ~17:56). Advertise TU is **32-bit only**.

---

## Maintainer inject-smoke (priority order: radial/sysmsg/input first; chat/free-cam are OMIT/SKIP)

1. Inject into the staged `SwgClient_r.exe`; confirm `utinni_init` resolves the full table (**111/111**)
   and `GetEngineHookPoints` reports version **9**.
2. **Radial menu:** open the radial-menu editor path; `cuiRadialMenuManager::update` resolves → radial
   editing works.
3. **System messages:** `systemMessageManager::receiveMessage` detour fires when the client receives a
   system message (it maps to `receiveSystemMessage`).
4. **Target-change:** select a target in-world; the `creatureObject::setTarget` detour fires. **If it
   does NOT fire / fires on the wrong action**, the consumer typedef likely expects a different setter
   — tell me and I'll re-point the accessor to `setIntendedTarget` / `setLookAtAndIntendedTarget`.
5. **Input diag:** `messageQueue::appendMessage` / `appendMessageData` detours fire on input events.
6. **Chat / free-cam:** unchanged this wave — chat construction via the already-advertised
   `createNewWindow`; free-cam via consumer vtable-resolution of `FreeCamera::alter`.

> Live inject-smoke is **maintainer-side**. Provider claim is scoped to: contract populated (111/111),
> built + staged + exported, each advertised row maps to a real entry (or is in the OMIT/SKIP ledger
> with a reason + alternative). **Do not** touch `D:/Code/Utinni`.

---

## Files touched (commit `ef073dca2`)

- `src/game/client/application/SwgClient/src/win32/engine_advertise.cpp` — Bucket A includes, 6 rows (3 constant + 3 placeholder), 3 dyn[] entries, the OMIT/SKIP ledger.
- `src/game/client/application/SwgClient/src/win32/utinni_creatureObject_forward.h` — **NEW** exe-local accessor decl.
- `src/engine/client/library/clientGame/src/shared/object/CreatureObject.cpp` — `utinni_creatureSetTargetRealEntry()` accessor (32-bit-only).
- `src/game/client/application/SwgClient/src/shared/engine_hookpoints.h` — version 8→9 + note.
- `src/game/client/application/SwgClient/src/shared/engine_hookpoints.inc` — +6 `ENGINE_HOOKPOINT` rows.

## Carry-forward (next waves, ledger §2.C–F)
- **§2.C** virtual vtable rows (object::addToWorld/removeFromWorld/setParentCell, cui::io::processEvent) — consumer-side; provider thunk optional. The 4 SKIPs above also fall here.
- **§2.D** mid-function cooperative toggles (modal-chat setter already advertised as `config::setModalChat`; offline-scene flag; debug-cam input passthrough).
- **§2.E** `Utinni_RegisterOnSceneReady` callback. **§2.F** crash-log-dir setter (low value).
