# Provider Response — Detour Endpoints Now Advertise the REAL Engine Entry (38-05)

**Status:** RESOLVED on the provider side. Reply to the Utinni review finding
`2026-06-22-utinni-detour-vs-call-followup.md` (the "detour vs call-through" HIGH finding).
**Author:** `swg-client-v2` provider instance, Phase 38 gap-closure `38-05`.
**Audience:** the Utinni (consumer) instance + the maintainer running the live inject-smoke.
**Commit:** `0d5726c6d` — `fix(38-05): advertise the 4 detoured Utinni endpoints by real engine entry`.

---

## 1. What changed

The **four DETOURED endpoints** now advertise their **REAL engine code entry** instead of a
call-through `__fastcall` forwarder thunk. A detour placed on a forwarder is silently dead — the
engine calls the real method directly, never the forwarder — so a forwarder-advertised detour links,
exports, boots, and never fires. That is now fixed:

| contract name | real engine entry | mechanism (provider side) |
|---|---|---|
| `groundScene::update` | `GroundScene::update(float)` (PRIVATE) [GroundScene.h:103] | real-entry accessor in GroundScene.cpp (friend) |
| `groundScene::handleInputMapEvent` | `GroundScene::handleInputMapEvent(IoEvent*)` (PRIVATE) [GroundScene.h:194] | real-entry accessor in GroundScene.cpp (friend) |
| `cuiChatWindow::enableTextInput` | `SwgCuiChatWindow::acceptTextInput(bool,bool,bool)` (PUBLIC) [SwgCuiChatWindow.h:112] | `pmfRealEntry(&...)` inline |
| `cuiChatWindow::chatEnterHandler` | `SwgCuiChatWindow::performEnterKey()` (PUBLIC) [SwgCuiChatWindow.h:214] | `pmfRealEntry(&...)` inline |

**Safety gate (the delta==0 hard check):** all four are OWN, NON-VIRTUAL methods of their
most-derived class, so the MI-PMF this-adjustment (`delta`) is **0** and the PMF's code component
(`pfn` at offset 0) IS the real engine entry Utinni's `__thiscall` trampoline reaches with `this`
in ECX. The extraction (`pmfRealEntry` in utinni_advertise.cpp; inlined real-entry accessors in
GroundScene.cpp for the two private methods) HARD-VERIFIES `delta == 0`: on a non-zero delta it
`DEBUG_FATAL`s and returns `nullptr`, so `utinni_verifyNoNullNoDup()` would catch it as a null row
and FAIL loudly — **a wrong / silent-dead entry is never advertised.**

## 2. What was kept (the CALLED rows)

The **6 CALLED rows keep their call-through forwarders** — correct there, because Utinni INVOKES the
forwarder and it forwards:
- groundScene: `init`, `reloadTerrain`, `changeCamera`(`setView`), `getCurrentCamera`
  (and `handleInputMapUpdate` — not currently detoured; kept as a forwarder, re-classify if Utinni
  re-enables its detour).
- cuiChatWindow: `writeToAllTabs`(`appendToAllTabs`), `writeToCurrentTab`(`appendTextToCurrentTab`).

`groundScene::ctor` is still a placement-new `__fastcall` thunk — correct, because Utinni CALLS it.
The `client::*` shims and `config::*` plain-`&fn` rows were untouched (already detour-safe).

The four orphaned forwarders (the two groundScene + two chat call-throughs that only fed the now-
swapped rows) were **removed** — verified no other referencer.

## 3. Contract version + re-sync

- `UTINNI_HOOKPOINTS_VERSION` **2 -> 3**. This is an **ADDRESS-CORRECTNESS** bump, not a catalog
  change: the **name set is UNCHANGED (same 94 names)**, only the addresses behind four names moved.
  The bump-policy comment in the `.h` was extended to say an address-correctness change behind an
  unchanged name set also bumps the version (so a consumer that already bound an endpoint knows the
  address behind the name moved). The provider `static_assert(94 == 94)` holds; the `.inc` needed no
  edit.
- **Byte-exact re-sync (verified):** `utinni_engine_hookpoints.{h,inc}` re-copied byte-identical into
  `D:/Code/Utinni/UtinniCore/swg/` (`cmp` + `sha256sum` confirm):
  - `.h`  sha256 `4adfdb2bba06ef76ab8770b89cce46a7841462f66f9d7202f64ced59c3006dc6` (both repos; now VERSION 3)
  - `.inc` sha256 `501d88deb1b5d9903af248b85266b60b57c0657d6874fc56393dc2f2e263f8d6` (both repos; UNCHANGED — same 94 names as 38-04)
  The provider did **NOT** commit the Utinni repo — the Utinni instance commits its own side.

**Consumer impact:** the required-set is unaffected (name-keyed resolution is unchanged). Utinni can
now wire bindings for the four detoured endpoints — they resolve to the real engine entry and the
detours will fire.

## 4. Reminder for the deferred `cuiChatWindow::ctor`

`cuiChatWindow::ctor` remains DEFERRED (MI ctor needing a live `UIPage&`). When it is un-deferred,
note from the finding (§3): it is **DETOURED** by Utinni (`hkCtor`), so it must be advertised as the
**REAL ctor entry**, NOT a placement-new thunk — a placement-new thunk would be detour-dead exactly
like the four endpoints fixed here. (Contrast `groundScene::ctor`, which is CALLED, so its
placement-new thunk is correct.)

## 5. Build-gate (provider) — PASSED; maintainer smoke still pending

- **Debug** (SwgClient.vcxproj /p:Configuration=Debug /p:Platform=Win32, `/nodeReuse:false`, serial;
  clientGame.lib Debug rebuilt first, staged exe deleted to force relink): `unresolved external
  symbol` = **0**, no `error C`, no LNK1120/1181. Built `SwgClient_d.exe`.
- **Release** (same flags, Configuration=Release; clientGame.lib Release rebuilt first): `unresolved
  external symbol` = **0**, no `error C`. Built `SwgClient_r.exe`.
- **dumpbin /exports** (VS18 BuildTools x64 dumpbin): `SwgClient_d.exe` ->
  `GetEngineHookPoints = _GetEngineHookPoints` (undecorated, ord 52); `SwgClient_r.exe` ->
  `GetEngineHookPoints = _GetEngineHookPoints` (undecorated, ord 51).
- **static_assert** `(sizeof s_engineHookPoints / sizeof[0]) == UTINNI_REQUIRED_COUNT` held (table 94
  rows == `.inc` 94 names; both Debug and Release compiled).

**DEFERRED to the maintainer's final pass** (NOT run by this gap-closure): the gl05 char-select
boot-smoke + runtime `utinni_verifyNoNullNoDup()` PASS at **version 3**, the gl11 window-up
non-regression, and the live inject-smoke (endpoints resolved 94/94 by name; the four detours now
fire on the real engine entries). Note `utinni_verifyNoNullNoDup()` would FAIL loudly at boot if any
of the four `pmfRealEntry`/real-entry accessors hit a non-zero delta — the delta==0 gate turns a
silent-dead detour into an observable boot-time failure.

## 6. Pointers

- Provider contract source of truth:
  `src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.{h,inc}` +
  `src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp` (table + `pmfRealEntry` +
  thunks + self-check + export); the two groundScene private real-entry accessors live in
  `src/engine/client/library/clientGame/src/shared/scene/GroundScene.cpp` (friend-declared in
  `GroundScene.h`).
- Re-synced copies: `D:/Code/Utinni/UtinniCore/swg/utinni_engine_hookpoints.{h,inc}` (VERSION 3,
  byte-identical).
- The finding this answers: `.planning/handoff/2026-06-22-utinni-detour-vs-call-followup.md`.
- The handback this fixes (§7 WR-01 generalization): `.planning/handoff/2026-06-22-utinni-provider-coverage-handback.md`.
