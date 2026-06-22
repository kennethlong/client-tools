# Utinni Provider-Coverage Handback — Phase 38 (EPA-07 / EPA-08)

**Status:** HANDBACK — Phase 38 close-out (2026-06-22). Companion / reply to the two Utinni
Phase-24 handoffs: `2026-06-22-utinni-advertised-client-coverage-status.md` (the coverage backlog)
and `2026-06-22-utinni-dx11-advertised-client-gap.md` (the DX11 live-smoke gap).
**Author:** `swg-client-v2` provider instance, Phase 38 (`38-01` … `38-04`).
**Audience:** the Utinni (consumer) instance + the maintainer running the live inject-smoke.
**Self-contained:** act on this doc without reading the Phase-38 plans/SUMMARYs.

---

## 0. TL;DR

The provider-side `GetEngineHookPoints` contract grew from **78 → 94** advertised endpoints
(`UTINNI_HOOKPOINTS_VERSION` bumped **1 → 2**). The `.h` + `.inc` were re-synced **byte-identical**
into `D:/Code/Utinni/UtinniCore/swg/` (sha256-verified; the Utinni repo is **not** committed by this
phase — it commits its own side). The 16 new names cover the three priority buckets the coverage-status
handoff named (`groundScene`, `client`/`config` shims, `cui::chatWindow`) plus the WR-05 carry-in.

**EPA-08 (DX11) is documentation-only — NO provider code change.** Static analysis proves the gl11
provider already satisfies the "all three pointers live" invariant across the swapChain's entire
non-null lifetime, so the `0xC0000005 WRITE target=0x34` is **consumer-side**. The fix (poll until
device+context are also non-null + a 3-pointer diagnostic log) is Utinni's `directx11.cpp tryInstall()`
work — §3 below.

The remaining provider step is the **maintainer's consolidated boot-smoke + live inject-smoke** — see §5.

---

## 1. What was advertised this phase (the new-row ledger)

16 new source-confirmed rows. Every row's engine symbol was confirmed against the actual
`swg-client-v2` source (header `file:line` cited in each row's `utinni_advertise.cpp` comment); a
symbol that is virtual / inline / file-local-static / MI-inflated / NONEXISTENT was OMITTED-with-reason
or routed to the correct mechanism — never guessed (D-04: a wrong `&fn` is worse than a missing row).

### 1a. groundScene::* — 8 rows (38-01)

NetworkScene : Scene, MessageDispatch::Receiver → an **MI / inflated-PMF** class, so `pmfToVoid` is
banned (it would trip the C2338 sizeof guard). Public methods use a free `__fastcall(GroundScene* /*ECX*/,
int /*EDX*/, args)` thunk (== `__thiscall`); **private** methods use an in-TU `__fastcall` forwarder
**defined in GroundScene.cpp + friend-declared in GroundScene.h** (same-TU alone is NOT enough — C++
grants private access only to members/friends; friend decls are ABI-neutral, no plugin cascade).

| contract name | engine symbol | file:line | mechanism |
|---|---|---|---|
| `groundScene::ctor` | `GroundScene(const char*, const char*, CreatureObject*)` | GroundScene.h:199 | placement-new `__fastcall` ctor thunk |
| `groundScene::init` | `GroundScene::init` (private) | GroundScene.h | in-TU `__fastcall` forwarder + friend decl |
| `groundScene::reloadTerrain` | `GroundScene::reloadTerrain` (public) | GroundScene.h | free `__fastcall` MI thunk |
| `groundScene::changeCamera` | `GroundScene::setView` (public) | GroundScene.h | free `__fastcall` MI thunk |
| `groundScene::getCurrentCamera` | `GroundScene::getCurrentCamera` (public) | GroundScene.h | free `__fastcall` MI thunk |
| `groundScene::update` | `GroundScene::update` (private) | GroundScene.h | in-TU `__fastcall` forwarder + friend decl |
| `groundScene::handleInputMapUpdate` | `GroundScene::handleInputMapUpdate` (private) | GroundScene.h | in-TU `__fastcall` forwarder + friend decl |
| `groundScene::handleInputMapEvent` | `GroundScene::handleInputMapEvent` (private) | GroundScene.h | in-TU `__fastcall` forwarder + friend decl |

- **SKIPPED:** `groundScene::draw` (virtual — vtable-resolved consumer-side).
- **OMITTED:** `groundScene::g_instance` (no dedicated singleton; reached via inline `Game::getScene()`
  with no ODR address, and `Game::ms_scene` is private). See §3 — if the terrain editor needs the raw
  singleton pointer, the cheap follow-up is a non-inline `Game` accessor.

### 1b. client / config — 4 rows (38-02), + WR-05 carry-in

**File-local exe statics / private members** → external-linkage `utinni_*` shim defined in the symbol's
own win32 TU; `config::*` are plain `&fn` on confirmed `CuiPreferences` statics.

| contract name | engine symbol | file:line | mechanism |
|---|---|---|---|
| `client::wndProc` | `Os::WindowProc` (private) | Os.h:138 | `__stdcall`/CALLBACK shim `utinni_osWindowProc` in Os.cpp + friend decl in Os.h |
| `client::writeMiniDump` | `DebugHelp::writeMiniDump` (public, win32-private header) | DebugHelp.h:36 | external-linkage shim `utinni_writeMiniDump` in DebugHelp.cpp |
| `config::setModalChat` | `CuiPreferences::setModalChat(bool)` (public static) | CuiPreferences.h:95 | plain `&fn` (37-02 correction: CuiPreferences static, NOT ConfigFile) |
| `config::getModalChat` | `CuiPreferences::getModalChat()` (public static) | CuiPreferences.h:94 | plain `&fn` |

- **WR-05 carry-in** (`consoleHelper::sendInput`): `__fastcall` thunk →
  `processInput(istr, getRecurseStackForCommandBeingParsed())`. Authored + committed in 38-01
  (`b99515648`); verified present pointing at `&utinni_consoleHelperSendInput` (not `pmfToVoid`).
- **OMITTED — NONEXISTENT (0 source twin; SWGEmu Pre-CU):** `client::writeCrashLog` (crash `.txt`
  written inline by `SetupSharedFoundation.cpp:92 sprintf`, no named fn) and
  `client::setupStartDataInstall` (no twin). See §3 — Utinni keeps these on RVA or drops them.

### 1c. cui::chatWindow::* — 4 rows (38-03)

SwgCuiChatWindow is **triple-MI** (`SwgCuiLockableMediator` + `UINotification` +
`MessageDispatch::Receiver`, SwgCuiChatWindow.h:58-61) → free `__fastcall(SwgCuiChatWindow* /*ECX*/,
int /*EDX*/, args)` thunks; `pmfToVoid` banned. All four targets are **PUBLIC** → no friend decl, no
engine-library edit (the whole change is exe-local to `utinni_advertise.cpp` + `.inc` + `.h`).

| contract name | engine symbol | file:line | mechanism |
|---|---|---|---|
| `cuiChatWindow::enableTextInput` | `acceptTextInput(bool,bool,bool)` | SwgCuiChatWindow.h:112 | triple-MI `__fastcall` thunk |
| `cuiChatWindow::writeToAllTabs` | `appendToAllTabs(const Unicode::String&)` | SwgCuiChatWindow.h:172 | triple-MI `__fastcall` thunk |
| `cuiChatWindow::writeToCurrentTab` | `appendTextToCurrentTab(const Unicode::String&)` | SwgCuiChatWindow.h:174 | triple-MI `__fastcall` thunk |
| `cuiChatWindow::chatEnterHandler` | `performEnterKey()` (CLEAN entry) | SwgCuiChatWindow.h:214 | triple-MI `__fastcall` thunk |

- **DEFERRED:** `cuiChatWindow::ctor` — `SwgCuiChatWindow(UIPage&, Game::SceneType, std::string const&)`
  [SwgCuiChatWindow.h:106]: an MI ctor needing a live `UIPage&` the injector must supply. 37-03 already
  deferred this exact ctor. See §3 (joint decision).
- **Issue #11 separation:** `chatEnterHandler` advertises ONLY the clean-entry `performEnterKey()`. The
  mid-function chat-context-routing NOP Utinni patches mid-body on SWGEmu is a separate joint decision
  (§3) — no offset arithmetic enters the contract.

### 1d. Bucket-4 (remaining full-set) — ZERO net new rows (38-03 confirm-or-OMIT ledger)

37-03 already took the plain-`&fn` bulk. Every remaining spec §6 candidate source-confirmed this wave
is virtual / inline / protected / MI-ctor / private-no-accessor / NONEXISTENT → OMITTED-with-reason,
never added on faith:

| spec candidate(s) | disposition | reason |
|---|---|---|
| `Object::move_o` | OMIT inline | `inline void Object::move_o` [Object.h:1216] (no ODR address) |
| `Object::addToWorld/removeFromWorld/setParentCell` | SKIP virtual | [Object.h:120-121,168] (vtable-resolved) |
| `CommandParser::createDelegateCommands` | OMIT protected | under `protected:` [CommandParser.h:178-180] |
| `Camera::getViewportX0/Y0/Width/Height` | OMIT inline | `inline int Camera::getViewport*` [Camera.h] (no ODR address) |
| `Camera::getViewport(int&,…)/(float&,…)` | OMIT (no distinct slot) | out-of-line addressable [Camera.h:170-171] but no concrete contract slot — a speculative row would be a spec-faith add (flag if Utinni names a slot) |
| `Appearance::render/collide`, `RenderWorld::render`, `clientWorld::collide`, `proceduralTerrain::*` | SKIP virtual | already in VIRTUAL SKIPS / 37-03 |
| `hud/loginScreen/gameMenu/radialMenu` + ~20 UI control ctors | DEFER MI ctor | each needs its own placement-new thunk + injector args (same as cuiChatWindow::ctor) |
| read-only globals (player health/stats, hud view-distance, terrain singleton/weather/filename, static-shader) | OMIT private | private members with no non-inline ODR accessor this wave |
| `memory::deallocateString`, `math::vectorNormalize`, network `idManager*`, `crcString::calculateCrc`, `client::writeCrashLog`, `client::setupStartDataInstall` | OMIT NONEXISTENT | 0 source twin (MemoryManager has `free()` not `deallocate`) |

---

## 2. New contract version: 2

- `#define UTINNI_HOOKPOINTS_VERSION 2` (was 1). The 37-era "PINNED at 1 / do NOT bump per wave" comment
  was rewritten to the **D-03 bump-on-name-change policy**: any NAME ADD/REMOVE to the `.inc` bumps the
  version. The version is advisory (the contract is name-keyed — a consumer resolves by name regardless)
  but the bump is the explicit "the catalog grew" signal.
- **Single bump** covers ALL Phase-38 growth: 38-01 groundScene ×8 + 38-02 client/config ×4 +
  38-03 cuiChatWindow ×4 = 16 new names; 78 + 16 = **94** required-set total.
- **Byte-exact re-sync (verified):** `utinni_engine_hookpoints.h` and `.inc` were copied byte-identical
  into `D:/Code/Utinni/UtinniCore/swg/`, overwriting the 37-03 v1 copies. `cmp` + `sha256sum` confirm
  byte-identity on both files:
  - `.h`  sha256 `8c7ff88a64fa7434004bfffdd3c7a571be9118b573833c92fd002e7a60b881ed` (both repos)
  - `.inc` sha256 `501d88deb1b5d9903af248b85266b60b57c0657d6874fc56393dc2f2e263f8d6` (both repos)
  The provider phase did **NOT** commit the Utinni repo — the Utinni instance commits its own side.
- The X-macro `.inc` is the **single name source**: provider + consumer expand the same file, so the
  count `static_assert` and runtime `utinni_verifyNoNullNoDup()` name-set-equality self-check stay in
  lockstep with the table (94-row table == 94-name `.inc`).

---

## 3. EPA-08 — DX11 provider-correctness evidence (LOCKED; NO provider change)

This is the reply to `2026-06-22-utinni-dx11-advertised-client-gap.md`. **D-01 is LOCKED: the gl11
provider is proven correct by static analysis; the deliverable is this evidence, not a code change.**
The `0xC0000005 WRITE target=0x34` is **consumer-side** (§4).

**Evidence (transcribed verbatim from the LOCKED finding):**

- **`dumpbin /exports`:** `gl11_r.dll` → `GetHookPoints = _GetHookPoints`; `gl11_d.dll` →
  `GetHookPoints = @ILT+16485(_GetHookPoints)`. Export present + **undecorated** on both. (The
  stale-build hypothesis #3 from the gap doc is ruled out — the export is there.)
- **`Direct3d11.cpp:881`** `GetHookPoints()` returns the three getters live; the getters
  (`Direct3d11_Device.cpp:702-726`: `getDevice` / `getSwapChain` / `getContext`) return the ComPtr
  members directly.
- **Creation order — device+context FIRST, swapChain LAST:** `Direct3d11_Device::create()` calls
  `D3D11CreateDevice → &ms_device, &ms_context` at **line 435** (FATAL on failure), and only later
  `CreateSwapChainForHwnd → &ms_swapChain` at **line 574**.
- **Destruction order — swapChain FIRST:** `destroy()` resets `ms_swapChain` at **line 684**, then
  context, then device.
- **⇒ Invariant:** across the swapChain's ENTIRE non-null lifetime, `device` and `context` are
  non-null. There is **NO provider window** where `swapChain != null && (device == null || context ==
  null)`. The consumer's null-swapChain-only guard is provably sufficient; the provider satisfies
  graphics-spec §3.3 and more strongly. The advertised-exe launch does **not** touch the gl11 path —
  `create()` is identical to the standalone client. **No provider change.**

This eliminates the gap doc's hypothesis #1 (device/context null behind a non-null swapChain) as a
*provider* defect: the provider cannot present a non-null swapChain with a null device/context. The
`+0x34` write therefore originates consumer-side in `dx11Singleton()->init()` / `ImGui_ImplDX11_Init`
or in poll context, not in the provider's `GetHookPoints()` return.

---

## 4. Consumer-side worklist — what Utinni must now wire (out of scope here)

These were ALL flagged OUT-OF-SCOPE in 38-CONTEXT.md (consumer-side / deferred). Listed so the
maintainer can act from this doc alone.

### 4a. The DX11 fix (closes Checkpoint 2 / Phase-18 D-08 / Phase-19 D-22) — consumer-side

- Harden `UtinniCore/swg/graphics/directx11.cpp` `tryInstall()` (~:148-149) to **poll until `device`
  AND `context` are non-null**, not just `swapChain` (cheap; removes the crash even though the provider
  is already proven to populate all three — defense-in-depth + clarity).
- Add a **one-shot 3-pointer diagnostic log** of `hp.swapChain / hp.device / hp.context` at the
  acquisition point. (Per §3, on a correct gl11 these will be all-non-null together — the log confirms
  it live and turns a silent `0xC0000005` into an observable.)
- Re-run the live inject-smoke under `rasterMajor=11`: expect
  `directX11::tryInstall: D3D11 overlay installed (advertised swapchain latched)`, no `0xC0000005`.

### 4b. Virtual-method resolution off the live vtable — consumer-side (no provider change)

These are SKIPPED provider-side by design (vtable slots, not stable function addresses); resolve them
off the live vtable as Utinni already does for D3D9 Present:

- `object::addToWorld`, `object::removeFromWorld`, `object::setParentCell`
- `cui::io::processEvent`

### 4c. Drop the per-subsystem `installable()` / group skips as coverage grows

Now that `groundScene` + the client/config + chatWindow buckets are advertised at version 2, Utinni can
drop the corresponding `swg::endpoints::installable()` / group skips so the advertised client installs
the same hook set SWGEmu does (the coverage-status §5 acceptance) — except the mid-function-patch
bucket, which stays per-feature.

### 4d. DEFERRED items needing a joint (provider+consumer) decision

- **`cuiChatWindow::ctor`** — MI ctor + live `UIPage&` arg. If Utinni confirms it constructs chat
  windows through a page, the provider adds a placement-new `__fastcall` thunk in a follow-up (same
  pattern as the advertised `groundScene::ctor` thunk — note `groundScene::ctor` IS already advertised).
- **Issue #11 (`chatEnterHandler` mid-function chat routing)** — the clean-entry `performEnterKey()` is
  advertised; the SWGEmu-only mid-body NOP patch is a cooperative-API decision (a real "modal chat
  context" setter vs. accepting it SWGEmu-only).
- **`groundScene::g_instance` OMIT** — if the groundScene editor strictly needs the raw singleton
  pointer (currently inline `Game::getScene()` / private `Game::ms_scene`), the cheap provider follow-up
  is a non-inline `Game` accessor.

### 4e. OMITTED NONEXISTENT symbols — Utinni keeps on RVA or drops

- `client::writeCrashLog` — no named fn (crash `.txt` is inline `sprintf` at
  `SetupSharedFoundation.cpp:92`).
- `client::setupStartDataInstall` — no twin in this tree (SWGEmu Pre-CU).

Neither will ever be a provider `&fn` row in this tree; Utinni keeps them on the hardcoded RVA path or
drops the feature.

### 4f. Explicitly NOT advertised (never wants a provider symbol — spec §6)

D3D9 device-vtable hooks (Utinni harvests at runtime), `D3DXCompileShader` (in the D3DX DLL), the
DirectInput8 vtable writes (patched on the COM instance). Listed for completeness — no action.

---

## 5. Remaining step — the maintainer's consolidated boot-smoke + live inject-smoke

The provider build-gate is green (see the 38-04 SUMMARY: Debug+Release link 0 unresolved; undecorated
`GetEngineHookPoints` on `_d` ord and `_r` ord; `static_assert (94 == 94)` holds). The following is the
**maintainer's** final pass — NOT run by this phase:

1. Boot `SwgClient_d.exe` under `rasterMajor=5` to character select → confirm
   `utinni_verifyNoNullNoDup()` logs **PASS** for the full 94-name set at **version 2** (no null /
   no dup / name-set equal).
2. Boot once under `rasterMajor=11` → window up, no crash (D3D11 + SWGEmu-path non-regression).
3. Live inject-smoke OUT OF REPO (Utinni injection) → no `0xC0000005`, endpoints resolved by name,
   `endpoints: resolved 94/94 by name`. (Requires the §4a consumer DX11 fix for the DX11 overlay leg.)

---

## 6. Pointers

- Provider contract source of truth:
  `src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.{h,inc}` +
  `src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp` (table + thunks + self-check +
  export).
- Re-synced copies: `D:/Code/Utinni/UtinniCore/swg/utinni_engine_hookpoints.{h,inc}` (v2, byte-identical).
- Phase-38 ledgers: `38-01-SUMMARY.md` (groundScene), `38-02-SUMMARY.md` (client/config + WR-05),
  `38-03-SUMMARY.md` (chatWindow + version bump + Bucket-4), `38-04-SUMMARY.md` (this re-sync + build).
- The two Utinni handoffs this doc answers: `2026-06-22-utinni-advertised-client-coverage-status.md`,
  `2026-06-22-utinni-dx11-advertised-client-gap.md`.
