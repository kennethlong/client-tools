# Utinni FREE-CAM accessors — provider HANDBACK (2026-06-29)

**Status:** code complete, build-gated _(see Build gate below)_. **NOT committed** (project rule:
commit only when asked). **`ENGINE_HOOKPOINTS_VERSION` 12 → 13, 113 → 119 names** (+6, one wave).

Unlocks the Utinni FREE-CAM editor. Every new row is a **CALLED accessor** (the consumer invokes it;
none detoured) that **encapsulates a fragile NGE struct byte-offset** so the consumer stops hardcoding
offsets that drift vs our layout — the same principle as `particlePreview::retrigger` and the
`config::setModalChat` external-linkage shims.

> ⚠️ The request named a primary artifact `2026-06-29-utinni-freecam-accessors.md` (rev. 2). **That file
> did not exist** in `.planning/` at execution time. I worked from the prompt summary + the live engine
> source (the source is ground truth here) and the live consumer source in `D:/Code/Utinni` (read-only,
> not modified). Everything below is source-verified.

---

## THE KEY FACT (resolves rows 1–3 + confirmation A)

The consumer's **"free camera" is OUR `DebugPortalCamera`**, not `FreeCamera`:

| consumer (`UtinniCore/swg/camera/camera.h`) | our engine (`GroundScene.h` `CameraIds`) |
|---|---|
| `cm_FreeChase = 2` | `CI_freeChase = 2` |
| `cm_Free = 5`      | **`CI_debugPortal = 5`** |

`toggleFreeCamera()` → `changeCamera(cm_Free=5)` → `setView(CI_debugPortal)` →
`getCurrentCamera()` returns `m_cameras[5] == m_debugPortalCamera`. Confirmed in
`GroundScene.cpp:715,720,772-776,803,1163-1283`.

---

## NEW ROWS (6) — all in `engine_advertise.cpp`, names in `engine_hookpoints.inc`

| # | contract name | real entry / mechanism | kind |
|---|---|---|---|
| 1 | `groundScene::isFreeCameraActive` | `__fastcall` thunk → `getCurrentView() == CI_debugPortal` | `&thunk` |
| 2 | `groundScene::getDebugPortalCameraMessageQueue` | friend forwarder → `m_debugPortalCameraInputMap->getMessageQueue()` | `&thunk` |
| 3 | `gameCamera::getMessageQueue` | `__fastcall` thunk → `getController()->getMessageQueue()` | `&thunk` |
| 4a | `messageQueue::getCount` | **MISMATCH** → `MessageQueue::getNumberOfMessages` (`pmfToVoid`) | `dyn[]` |
| 4b | `messageQueue::getMessage` | 4-arg overload `getMessage(int,int*,float*,uint32*) const` (`pmfToVoid` + static_cast) | `dyn[]` |
| 5 | `object::isActive` | external-linkage `__fastcall` shim (method is NON-virtual but **INLINE** → no PMF address) | `&shim` |

Calling convention for all `__fastcall` thunks: `__fastcall(pThis /*ECX*/, int /*EDX*/, …)` ==
`__thiscall` (MSVC v145 forbids `__thiscall` on a free fn, C3865; dummy EDX makes them byte-identical).

### Per-row detail
1. **`isFreeCameraActive`** — uses PUBLIC `getCurrentView()` + PUBLIC `CI_debugPortal` enumerator; no
   friend needed. Returns `bool`. Consumer typedef `bool(__thiscall*)(GroundScene*)`. (`CI_debugPortal`
   == 5 == `cm_Free`, so this returns exactly what the consumer's `currentView == cm_Free` did.)
2. **`getDebugPortalCameraMessageQueue`** — `m_debugPortalCameraInputMap` is PRIVATE
   (`GroundScene.h:111`) → friend forwarder DEFINED in `GroundScene.cpp` (member access + `InputMap`
   already included), DECLARED in exe-local `utinni_groundScene_forward.h`, friend-declared in the
   existing `#if !defined(_WIN64)` block of `GroundScene.h`. **ABI-neutral** (no member, no vtable
   change → no plugin cascade). Returns the MQ the consumer read at the hardcoded `InputMap+0xC`.
   Null-safe. Consumer typedef `MessageQueue*(__thiscall*)(GroundScene*)`.
3. **`gameCamera::getMessageQueue`** — reaches the camera's movement MQ via the base
   `Object::getController()` (`Object.h:190`) + `Controller::getMessageQueue()` (`Controller.h:67`) —
   layout-independent, no subclass `m_queue` offset. Cameras are single-inheritance so the `GameCamera*`
   from `getCurrentCamera()` needs no adjustment. Consumer typedef `MessageQueue*(__thiscall*)(GameCamera*)`.
4. **`getCount` / `getMessage`** — `MessageQueue` is a flat single-inheritance class → `pmfToVoid`
   (4-byte PMF == real entry). `getCount` is a **name MISMATCH** (no `getCount`; the real method is
   `getNumberOfMessages`, `MessageQueue.h:42`). `getMessage` advertises the **4-arg overload**
   `getMessage(int index, int* message, float* value, uint32* flags=0) const` (`MessageQueue.h:43`) —
   **exactly your requested `(i, outType, outValue, outFlags)` signature** (`uint32` == `uint32_t` on
   Win32; you pass `nullptr` for flags). `static_cast` disambiguates from the `Data**` overload (`:44`).
   ⚠️ Your stale `io_win.h` currently declares a **3-arg** `getMessage(int,int*,float*)` — your paired
   consumer wave must widen that wrapper to the 4-arg real entry (or it reads the wrong stack shape).
5. **`object::isActive`** — `Object::isActive()` is **NON-virtual** (answers your confirmation) **but
   INLINE** (`Object.h:158` decl / `:1328` def) so `&Object::isActive` has no ODR-emitted address (the
   `g_mainLoopCounter` inline-accessor pitfall). External-linkage shim supplies it. Consumer typedef
   `bool(__thiscall*)(const Object*)`. **No vtable slot needed — it is not virtual.**

---

## CONFIRMATIONS (you asked, answered from source)

**A. `alter` detour target.** ✅ The live free-cam camera is `DebugPortalCamera` (per the KEY FACT).
`DebugPortalCamera` overrides `virtual float alter(float)` (`DebugPortalCamera.h:36`), so on the live
instance the alter vslot resolves to **`DebugPortalCamera::alter`, NOT `GameCamera::alter`**.
**`alter` is at Object virtual slot 4** — verified from the Object virtual declaration order
(`Object.h`: dtor=0, `isInitialized`=1, `addToWorld`=2, `removeFromWorld`=3, **`alter`=4**) — matching
your "Object virtual slot 4" exactly. And `getCurrentCamera()` after `changeCamera(cm_Free)` returns
`m_debugPortalCamera` (`GroundScene.cpp:1272-1275`, `m_cameras[CI_debugPortal]=m_debugPortalCamera`).
_(The SWGEmu RVAs 0x006DA1B0 / 0x00788740 are yours to map at runtime; the class identity + slot index
are source-certain. A live-vtable dump is your last-mile check, but it will hold.)_

**B. MQ aliasing.** ✅ **YES — they alias when free-cam (debugPortal) is active.** `init` wires:
`debugPortalCameraInputMap->setMessageQueue(debugPortalCameraController->getMessageQueue())`
(`GroundScene.cpp:776`) and `m_debugPortalCamera->setMessageQueue(m_debugPortalCameraInputMap->getMessageQueue())`
(`:803`), and the camera's controller IS `debugPortalCameraController` (`:772-773`). So the
**InputMap MQ == the CameraController MQ == the camera's drained `m_queue`** — one pointer. Row 2 and
row 3 therefore return the same `MessageQueue*` while free-cam is active. **You may drop one row on your
side** — I shipped both as requested; dropping is your consumer-side call.

**C. IoEvent ABI.** ✅ **Identical.** Our `IoEvent` (`sharedIoWin/IoWin.def:73`):
`IoEvent* next` (+0x00), `IoEventType type` (+0x04), `int arg1` (+0x08), `int arg2` (+0x0C),
`real arg3` (+0x10). Your `io_win.h` models it as `swgptr unk; int type; int arg1; int arg2; float arg3;`
— byte-for-byte match (your `unk` = our private `next`; `real` == `float`; `IoEventType` is int-sized).
Read the fields directly; do NOT read `type` at offset 0 (that's `next`). No IoEvent accessors needed.

---

## Files changed (provider side only — `D:/Code/Utinni` NOT touched)
- `…/SwgClient/src/shared/engine_hookpoints.h` — version 12 → 13 + history note.
- `…/SwgClient/src/shared/engine_hookpoints.inc` — 6 `ENGINE_HOOKPOINT` rows (119 total).
- `…/SwgClient/src/win32/engine_advertise.cpp` — 2 includes, 3 thunks, 4 constant + 2 placeholder rows,
  2 `dyn[]` fills.
- `…/SwgClient/src/win32/utinni_groundScene_forward.h` — `MessageQueue` fwd-decl + row-2 extern.
- `…/clientGame/src/shared/scene/GroundScene.h` — 1 ABI-neutral friend decl (`#if !defined(_WIN64)`).
- `…/clientGame/src/shared/scene/GroundScene.cpp` — row-2 friend forwarder definition.

## RE-SYNC (maintainer)
Copy the v13 working-tree (LF) `engine_hookpoints.{h,inc}` byte-identical into
`D:/Code/Utinni/UtinniCore/swg/`, then rebuild `UtinniCore.dll`. **v13 sha256:**
- `engine_hookpoints.h`   `bbd02175e820f12294e56d339afbc639ad6072c3bc6a3d814d2f7785d8aaf5ec`
- `engine_hookpoints.inc` `2fdd77c523658cb13f17edbe5822282a42a124ffacea8359e1744545668fd96d`

Consumer paired wave (your court, no provider action): re-source the live GroundScene without `get()`,
rework `processIoEvent`/`FreeCamImpl`, widen `io_win.h` `getMessage` to 4-arg. Mouse-wheel speed stays
SWGEmu-only for v1 (no ask).

## Build gate — PASS
Release/Win32 `/t:SwgClient`, force-relink (deleted `SwgClient_r.exe` first), `/nodeReuse:false /m:1`:
- **0 unresolved external symbols** (grep of the build log; `/FORCE` masks LNK2019/2001 so this is the real gate, not exit 0).
- Build succeeded, **0 errors**. X-macro count `static_assert` (119 == `.inc`) passed (a mismatch is a compile error).
- **`GetEngineHookPoints`** undecorated export present (dumpbin: ordinal 82, RVA `00700860`).
- Restaged: `stage/SwgClient_r.exe` == built exe (28,457,984 bytes, 2026-06-29 22:14, postbuild auto-copy).
- 32-bit-only TU (whole advertise body `#if !defined(_WIN64)`). No plugin rebuild — the lone shared-header
  touch (`GroundScene.h`) is an ABI-neutral friend decl (no member / no vtable change).

**Not committed** (project rule: commit only when asked). When you give the word I'll stage the exact 6
files + this handback + the README entry and commit (mirroring the prior wave commits).
