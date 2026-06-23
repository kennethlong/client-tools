# Provider Response — DX11 advertised-client follow-ups (§0 restage · §1 embed-resize crop · §2 UI-teardown)

**Date:** 2026-06-23
**Status:** HANDBACK — provider (`swg-client-v2`) response to the DX11 advertised-client follow-up
request. §0 + §1 + §2 addressed; §3 deferred as requested.
**Audience:** Utinni (consumer) + the swg-client-v2 maintainer (live re-smoke owner).
**Source of truth:** this file lives in `swg-client-v2`. **The provider did NOT write to
`D:/Code/Utinni`.** No contract change — `UTINNI_HOOKPOINTS_VERSION` stays **3**, 94 endpoints. These
are pure renderer/engine fixes; nothing to re-sync.

Responds to the request items: §0 rebuild+restage, §1 embed-resize crop, §2 verify UI-teardown
cluster, §3 cuiChatWindow::ctor (later).

---

## §0 — Rebuild + restage: **DONE**

The staged `SwgClient_r.exe` predated the 38-06..38-09 fixes. Rebuilt + restaged (Win32 / Release) to
`D:\Code\swg-client-v2\stage\`. The exe now carries all of: 38-06 (`CuiMediator::deactivate`
`m_objectCallbackVector` guard), 38-07 (Release-safe texture validation), 38-08
(`SwgCuiHudWindowManagerGround` `m_callback` guard), 38-09 (`CuiMediator::garbageCollect`
re-entrancy ROOT fix). `gl11_r.dll` restaged too (carries the 38-07 `Direct3d11_TextureData` desc
reject). Both staged binaries are now post-§1 (16:35) — see §1.

---

## §1 — Embed-resize crop (half-done client resize): **FIXED (provider/engine), needs a RenderDoc smoke**

### Root cause (confirmed; matches your DIAG + cross-checked by 2 external code readers)

The DX11 resize path resized **only the swap-chain back-buffer**. The engine's render-target
dimension globals, the **offscreen scene render target(s)**, and the **camera projection/aspect** never
updated, so the scene drew into a stale 1600×900 viewport/RT and only its top-left landed in the
(correctly back-buffer-sized) window. Standalone never resizes, which masked it.

Precise chain (file:line):
- `Os::WindowProc` `WM_SIZE` → `ms_displayModeChangedHookFunction` → `GraphicsNamespace::displayModeChanged()`
  (`Graphics.cpp:375`) → `Direct3d11_Device::displayModeChanged()` resized the back-buffer **only**.
- The engine's dims (`Graphics::getCurrentRenderTargetWidth/Height`, `getFrameBufferMaxWidth/Height`)
  were **never updated** — `Graphics::resize()` (`Graphics.cpp:520`, the existing path CutScene uses)
  was never called on a window resize.
- `PostProcessingEffectsManager` (`deviceRestored`, `PostProcessingEffectsManager.cpp:137`) creates the
  primary/secondary/tertiary scene RTs from `getFrameBufferMax*` once, and the D3D11 plugin **never
  fired** the device-restored callbacks on resize (the D3D11 `resize` Gl_api slot was `STUB(resize)` —
  a *fatal* stub).
- The per-frame camera DOES self-correct (`GroundScene.cpp:2289-2302` re-reads
  `getCurrentRenderTarget*` and calls `Camera::setViewport` → `Camera::setup` → rebuilds projection
  every frame) — it was simply being fed stale dims.

### The fix (mirrors the existing, tested D3D9 resize path; gl11-only)

1. **`GraphicsNamespace::displayModeChanged()`** (`Graphics.cpp`): for `ms_rasterMajor == 11`, query the
   window client rect (`Os::getWindow()` + `GetClientRect`) and, when it changed, call the existing
   `Graphics::resize(w,h)` — which updates all the dim globals AND calls `ms_api->resize`. **D3D9 is
   untouched** (still its legacy deferred `checkDisplayMode` path) so the working D3D9 RT-space-stretch
   embed does not regress.
2. **`Direct3d11` `resize` slot** (`Direct3d11.cpp`): replaced `STUB(resize)` with a real `resize_impl`
   that mirrors `Direct3d9Namespace::resize` — fire device-lost callbacks → resize the back-buffer →
   fire device-restored callbacks. The D3D11 device/context are **unchanged** across a swap-chain
   resize (NOT a true device loss); the 3 registered callbacks (`PostProcessingEffectsManager`,
   `Bloom`, `BinkVideo`) simply release + re-fetch their screen-sized RTs at the new `getFrameBufferMax*`.
3. **`Direct3d11_Device`**: factored the back-buffer-resize body into `resizeBackBuffer(w,h)` (shared by
   `displayModeChanged` and `resize_impl`).

Net effect after a resize: dims update → offscreen scene RTs recreated at new size → next frame's
`GroundScene` viewport + camera projection track the new size → scene fills the back-buffer. Crop gone.

**Bonus:** this also un-breaks **cutscenes on D3D11** — `CutScene` calls `Graphics::resize(640,480)`,
which previously hit the fatal `resize` stub.

### Safety analysis (why firing device-lost/restored on a non-lost D3D11 device is OK)

Only THREE callbacks are registered through `Graphics::addDeviceLost/RestoredCallback`:
`PostProcessingEffectsManager`, `Bloom`, `BinkVideo`. Each uses the same idiom — `deviceLost` releases
with null-guards, `deviceRestored` re-fetches RTs from `getFrameBufferMax*`. All three are safe to fire
on a swap-chain-only resize and all correctly pick up the new size. No other **screen-sized** RT is
created outside this path (texture-renderer baked RTs are **asset-sized**, not screen-sized; the gl11
per-frame begin-scene viewport reads `ms_width/ms_height`, which `resizeBackBuffer` updates).

### What the maintainer should confirm (live + RenderDoc — provider can't smoke headless)

- Embedded resize: login fills the panel (no bottom black bar); in-game renders cleanly (no
  tearing/top-left crop); resizing/maximizing the TJT panel rescales correctly.
- Standalone gl11 unchanged; SWGEmu/D3D9 unchanged.
- **RenderDoc:** capture a post-resize in-game frame; the bound viewport (`RSSetViewports`) + scene RT
  dims should now EQUAL the swap-chain back-buffer (pre-fix they were smaller / top-left-anchored).
- One change per smoke; revert this commit if a boot/render regresses.

### Files

- `src/engine/client/library/clientGraphics/src/win32/Graphics.cpp` (`displayModeChanged`)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp` (`resize_impl` + binding)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_Device.cpp` / `.h`
  (`resizeBackBuffer` extraction)

---

## §2 — UI-teardown cluster (`UIComboBox::~UIComboBox`): **verify on re-smoke; NO speculative guard (rationale below)**

The world-load crash was `UIComboBox::~UIComboBox` (`UIComboBox.cpp:141`,
`mButton->RemoveCallback(this)`), framed as "same family" as the `CuiMediator::deactivate` null-deref.
**It is the same FAMILY (UI torn down mid-iteration during embed focus/activate churn) but NOT the same
fix shape:**

- `UIComboBox::mButton` / `mTextbox` are `new`'d in the ctor init list (`UIComboBox.cpp:87-88`) and
  immediately `->Attach(this)`. They are **always non-null after a successful construction.** So a null
  `mButton` at the destructor is a **use-after-free / double-destruction**, not a legitimate nullable
  member (unlike `CuiMediator::m_objectCallbackVector`, which is genuinely nullable and guarded
  elsewhere at `:1170`).
- Adding `if (mButton)` there would **mask the UAF**, not fix it.
- The correct coverage is the **38-09 `CuiMediator::garbageCollect` re-entrancy ROOT fix** — the same
  re-entrant teardown that double-destroys a mediator/page mid-iteration is what double-destroys a
  combobox it owns. That fix is now in the rebuilt exe (it was NOT in the dump's exe — the dump's
  `SwgClient_r.exe` predated it, as you noted).

**Maintainer action:** on the re-smoke, do a world-load **and** a scene change. Expectation: 38-09
covers it (no `UIComboBox:141` fault). If it STILL faults there even on this exe, that proves a
residual re-entrancy/UAF path — root-cause it (don't paper over); the one-line guard + one-shot WARNING
(mirroring 38-06) is ready as a fallback, but masking a live UAF is the wrong default.

---

## §3 — `cuiChatWindow::ctor` by REAL ENTRY: **deferred (as requested)**

Noted for the chat-editor unlock: advertise the **real ctor entry** (like the 4 detoured endpoints
fixed in 38-05), NOT a placement-new thunk — Utinni detours the ctor to publish the instance, it
doesn't construct one. No action now.

---

## Build gate

- **Release / Win32** (`Direct3d11;SwgClient`, `/nodeReuse:false`, serial; staged + compiled binaries
  deleted to force relink): **0 `unresolved external symbol`** (the real link gate — `/FORCE` masks
  them; grepped to 0), 0 errors, Build succeeded. `SwgClient_r.exe` + `gl11_r.dll` restaged to
  `stage\` (16:35).
- **Debug / Win32**: same flags — **0 `unresolved external symbol`**, 0 errors, Build succeeded;
  `SwgClient_d.exe` + `gl11_d.dll` staged.
- **dumpbin /exports** (post-build, no contract regression): `SwgClient_r.exe` →
  `GetEngineHookPoints` (ord 51); `gl11_r.dll` → `GetHookPoints` (ord 1).
- No shared-header / Gl_api struct change → **no plugin ABI cascade**; gl05/06/07 unaffected
  (`Graphics.cpp` is exe-side; `Direct3d11_Device.h` is gl11-internal).
- Live inject-smoke + RenderDoc = maintainer-only → DEFERRED (see §1 "what to confirm").

## Cross-AI review

Root cause + fix shape independently cross-checked by Codex (call-graph / device-callback fan-out
safety + missed-RT enumeration) and Cursor (dim-flow / camera projection / DEBUG_FATAL / resize
timing). Tasks + outputs: `.planning/research/CONSULT-49-{codex,cursor}-dx11resize.md` / `.out`.
Key confirmations: camera self-corrects per-frame; the `preSceneRender` DEBUG_FATAL is tautological
(won't trip on a resize race); resize fires between frames via `Os::update` (not mid-scene); only the
3 contained callbacks fan out.
