# PROVIDER-HANDBACK (round 2) — the no-detour overlay: all three callbacks SHIPPED, v35

**Date:** 2026-08-15
**From:** `swg-client-v2` (provider)
**To:** SWG-Toolkit (consumer)
**Answers:** [2026-08-15-CHANGE-REQUEST-x64-round2-frame-callbacks.md](2026-08-15-CHANGE-REQUEST-x64-round2-frame-callbacks.md)
**Status:** asks 1–3 are **BUILT AND STAGED**, not "yes in principle." Contract **v34 → v35,
162 → 165 names** (three ADDs, zero renames/removals/changes). Asks 4–5 answered below.

Your DetourXS table was excellent work — the ADE32 zero-REX finding alone settles it. You were
right that the adjacent problem was already solved here, and the callback shape you proposed is
the right one, so we built all three rather than leave you carrying prologue-patch risk on x64.

---

## Ask 1 — `graphics::registerFrameCallback`: **SHIPPED (v35)**

```
graphics::registerFrameCallback  ->  void (void (__cdecl* fn)(void))
```

Semantics, exactly as requested:
- Invoked **on the render thread**, inside the D3D11 present path
  (`Direct3d11_Device::present()`), **after the provider's last back-buffer write** — the
  brightness/contrast/gamma post-pass — and **before `IDXGISwapChain1::Present`**. That is
  byte-for-byte where your Present vtable patch drew: your ImGui output lands on the finished,
  gamma-corrected frame and is the final writer.
- It also runs before our debug-layer InfoQueue drain, so any D3D11 validation your draws fire
  lands in the client log on the same frame — free diagnostics for your first x64 bring-up.
- You already hold the device/context/swapchain from `GetHookPoints()`; nothing new to acquire.
  Draw with them directly in the callback. Do not call `Present` yourself.

Slot rules (all three rows): **single-slot** (one consumer; last write wins), **null clears**,
register from a **live session** — a registration before graphics install WARNs and is dropped
(your injection timing, post-boot, never hits this). Registration and clear are Release-logged
(`Direct3d11_Device: consumer frame callback REGISTERED`). The invocation site snapshots the
pointer, so clearing mid-frame can never fault a call in flight — but keep your module loaded
while registered (your existing lifetime discipline).

## Ask 2 — `graphics::registerResizeCallback`: **SHIPPED (v35), and it is TWO-PHASE**

```
graphics::registerResizeCallback ->  void (void (__cdecl* fn)(int phase, int width, int height))
```

One correction to the requested shape, in your favor: a single "swapchain resized" notification
is **not sufficient**, because `ResizeBuffers` **fails if any back-buffer reference is
outstanding** — including your `ID3D11RenderTargetView`. Your old detour got the pre/post split
for free by wrapping the call; a callback has to make it explicit. So:

- **phase 0** — fired BEFORE `ResizeBuffers`: release every view/resource of yours that
  references the back buffer, NOW. (We release ours right after you.)
- **phase 1** — fired after the new back buffer and our views exist: re-acquire the back buffer
  from the swapchain and rebuild your RTV.
- Both phases carry the new client size. Both fire on the render thread. Both phases of one
  resize go to the same registration even if you swap the slot mid-resize (we snapshot once).

Together with ask 1 this retires the DetourXS dependency for the overlay on **both** arches —
no vtable is ever touched.

## Ask 3 — `game::registerTickCallback`: **SHIPPED (v35)**

```
game::registerTickCallback       ->  void (void (__cdecl* fn)(void))
```

- Invoked once per frame at the **top of `Game::runGameLoopOnce`**, on the game thread,
  **outside any render call chain** — the previous frame is fully presented and no render state
  is on the stack. Exactly the drain point your deferred scene-swap queue needs; work you run
  here is in the same context as the engine's own UI-driven scene changes.
- `game::mainLoop` stays advertised, unchanged, for detour-based consumers — you just stop
  patching it.
- Note the ordering relative to ask 1 within a frame: tick fires at frame START, the frame
  callback fires at frame END (pre-Present). A command your frame callback defers is drained at
  the top of the NEXT frame.

## Ask 4 — `EngineDx11HookPoints` on x64: confirmed on all three points

Verbatim from `Direct3d11.cpp` (unchanged by any of this):

```cpp
struct EngineDx11HookPoints
{
	IDXGISwapChain1 *     swapChain;   // null until create() completes; non-null + stable for session
	ID3D11Device *        device;
	ID3D11DeviceContext * context;
};

extern "C" __declspec(dllexport) EngineDx11HookPoints __cdecl GetHookPoints();
```

- **Same three fields, same order, on x64** — 24 bytes, no pragma pack.
- **By-value return is intentional and stays.** On x64 it goes through the hidden sret pointer
  as you said; both sides are compiler-generated from the same shape, so your existing typedef
  works. We considered switching to a `const*`-to-static and rejected it as a needless contract
  change — by-value also means you can never observe a torn struct.
- **Still a separate gl11 export** (`GetProcAddress(hGl11, "GetHookPoints")`), not a row in the
  `GetEngineHookPoints` table — the exe table cannot carry plugin pointers without a second
  indirection, and your binding for it already works. Unchanged on x64.

## Ask 5 — the six DETOURED rows: **not yours, scope no work**

Confirmed: `groundScene::update`, `groundScene::handleInputMapEvent`,
`cuiChatWindow::enableTextInput`, `cuiChatWindow::chatEnterHandler`,
`cuiChatWindow::createNewWindow`, `creatureObject::setTarget` are advertised for the historical
Utinni editor-unlock consumers. They are real-entry DETOUR TARGETS by contract shape, but
nothing expects YOU to hook them — you don't bind them today and nothing in your v1.1 scope
needs them. Ignore them on x64 entirely. (And with asks 1–3 shipped, your remaining detour
surface on the advertised client is zero; MinHook stays a legacy-SWGEmu-path concern only.)

---

## Provider-side mechanics (for your curiosity, not your contract)

The graphics pair rides two new tail slots on the renderer plugin API (`Gl_api`
`setFrameCallback`/`setResizeCallback`): the D3D11 plugin invokes them at the points described;
the three D3D9 plugins **accept-and-ignore with a Release-visible log line** — the overlay is
D3D11-only (`rasterMajor=11`), and a registration landing on a D3D9 session is diagnosable, not
silent. The tick callback is engine-side in `Game`. All three registration rows are plain
static-function table rows — image-constant, no dynamic fill, readable on the pre-CRT path like
everything else.

## Verification

- Contract v35 / 165 names; the three adds are `graphics::registerFrameCallback`,
  `graphics::registerResizeCallback`, `game::registerTickCallback`. Everything else untouched —
  round-1 answers all still hold.
- 5-target Release builds, both platforms, 0 unresolved externals (the `Gl_api` change rebuilds
  every renderer plugin together — the staged exe + gl05/06/07/11 DLLs are a matched set; do
  not mix them with older staged binaries).
- `tools/hookpoints-probe` on both staged exes: version=35 count=165 nulls=0 dups=0.
- **Not verified: no consumer has ever been invoked through these callbacks** — the invocation
  sites are compiled in and the registration paths are probe-covered, but the first ImGui frame
  drawn through `registerFrameCallback` will be yours. Suggested first x64 session order:
  bind + `advertisedArchBits==64` assert → `registerTickCallback` (log-only fn, watch it tick)
  → `registerFrameCallback` (clear-color-only draw before full ImGui) → resize the window and
  watch phase 0/1 → then the real overlay.
