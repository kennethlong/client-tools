---
phase: 38-utinni-advertised-client-coverage-completion
status: context-locked
created: 2026-06-22
source_docs:
  - .planning/handoff/2026-06-22-utinni-advertised-client-coverage-status.md
  - .planning/handoff/2026-06-22-utinni-dx11-advertised-client-gap.md
  - .planning/handoff/2026-06-20-utinni-engine-entrypoint-advertisement-spec.md
  - .planning/phases/37-utinni-engine-entry-point-advertisement-getenginehookpoints/
---

# Phase 38 — Context & Locked Decisions

Provider-side follow-on to Phase 37. Finishes the Utinni integration by extending the
`GetEngineHookPoints` contract to the full **`&fn`-addressable** engine entry-point set.
Driven by the Utinni Phase-24 post-live-smoke reconciliation (the two 2026-06-22 handoffs).

## Locked decisions (from discuss)

- **D-01 — DX11 readiness is EVIDENCE-ONLY (no provider code change).** The gl11 provider is
  proven correct by static analysis (see "DX11 finding" below). The deliverable is a dated
  handoff-back with the dumpbin + create/destroy ordering proof. The `0xC0000005 target=0x34`
  is consumer-side; the fix (poll-until-device+context guard + 3-pointer diagnostic log) is
  Utinni's, out of scope here.
- **D-02 — Scope = priority buckets + remaining addressable full-set.** Largest provider pass:
  groundScene MI thunks, client/config external-linkage shims, cui::chatWindow MI thunks, fold
  in WR-05, AND the remaining full-catalog rows that are plain `&fn`-addressable (non-MI,
  non-virtual) where source-confirmable. Closest to full RVA retirement.
- **D-03 — Byte-exact contract.** Any name add/remove bumps `UTINNI_HOOKPOINTS_VERSION`; the
  `.inc` + `.h` re-sync byte-identical into `D:/Code/Utinni/UtinniCore/swg/`. The X-macro
  coverage self-check (count `static_assert` + runtime `utinni_verifyNoNullNoDup()` name-set
  equality) stays in lockstep with the table.
- **D-04 — A wrong `&fn` is worse than a missing row.** Every new row source-confirmed against
  the actual `swg-client-v2` source (header `file:line` cited in the row comment). A symbol that
  is virtual / inline / file-local-static / MI-inflated / non-existent is OMITTED-with-reason or
  routed to the correct mechanism (thunk/shim) — never guessed.

## The four provider mechanism buckets (from coverage-status §2)

| Bucket | Members | Mechanism |
|--------|---------|-----------|
| **MI / inflated PMF** | `groundScene::*` (NetworkScene : Scene, MessageDispatch::Receiver); `cui::chatWindow::*` | free-function `__fastcall(pThis /*ECX*/, int /*EDX*/, args) == __thiscall` thunk per method (the 37-03 ctor-thunk pattern); advertise the thunk address |
| **File-local exe statics** | `client::{wndProc,writeMiniDump,writeCrashLog,setupStartDataInstall}`, `config::{setModalChat,getModalChat}` | one-line external-linkage forwarder shim (distinctly named, like Phase-37's `utinni_installConfigFileOverride`) |
| **Remaining plain `&fn`** | non-MI/non-virtual full-catalog rows (object/template/terrain/camera/etc. extras the MVP skipped) | direct `&Symbol` / `pmfToVoid(&Class::member)` with overload `static_cast` where needed |
| **WR-05 carry-in** | `consoleHelper::sendInput` | ALREADY WRITTEN (parked uncommitted thunk in `utinni_advertise.cpp`): `__fastcall` thunk → `processInput(istr, getRecurseStackForCommandBeingParsed())`. Fold into this phase's build+commit. |

## OUT OF SCOPE (consumer-side / deferred — record in handoff-back, do NOT implement)

- Virtual-method resolution off the live vtable: `object::{addToWorld,removeFromWorld,setParentCell}`,
  `cui::io::processEvent` (Utinni resolves these off the live vtable, as it already does for D3D9 Present).
- Utinni's `directx11.cpp tryInstall()` hardening (poll until device+context non-null) + the
  one-shot 3-pointer diagnostic log. **This is the DX11 fix** and it is consumer-side.
- Dropping Utinni's per-subsystem `installable()` / group skips as coverage grows.
- Mid-function-patch features (Issue #11 chat routing, UI Y-axis cascade NOP, debug-camera
  input-suppress NOP, worldSnapshot `.trn`-name NOP, crashlog inline hook, `shader::popCell`) —
  each needs a per-feature cooperative-API decision JOINTLY with Utinni; do not start them here.
- Utinni-internal endpoints that never want a provider symbol (D3D9 device-vtable hooks,
  `D3DXCompileShader`, DirectInput8 vtable writes) — spec §6 "do NOT advertise".

## HARD CONSTRAINTS

- Client stays completely **Utinni-agnostic**: `GetEngineHookPoints` / `GetHookPoints` are pure
  read-only getters; nothing calls into Utinni.
- Do **NOT** regress the standalone D3D11 client or the SWGEmu path. Advertisement lights up only
  when the export is present (dual-path auto-select on the consumer side).
- The whole advertise body stays Win32-only (`#if !defined(_WIN64)` + vcxproj Platform=Win32).
- Verify EACH deliverable: `dumpbin /exports` (undecorated `GetEngineHookPoints`) + clean staged
  build with **0 `unresolved external symbol`** (SwgClient links under /FORCE — grep the log).
  The maintainer runs the live inject-smoke; this phase produces the staged build + dumpbin +
  coverage evidence and hands it back.
- Pre-existing, do NOT fix: Optimized (`_o`) flavor fails at LINK with `LNK1281 SAFESEH`
  (0 unresolved from our rows). Validation bar = Debug + Release per AGENTS.md.

## DX11 finding (LOCKED — do not re-investigate; this is the evidence to hand back)

- `dumpbin /exports`: `gl11_r.dll` → `GetHookPoints = _GetHookPoints`; `gl11_d.dll` →
  `GetHookPoints = @ILT+16485(_GetHookPoints)`. Export present + undecorated on both. (Stale-build
  hypothesis #3 ruled out.)
- `Direct3d11.cpp:881` `GetHookPoints()` returns the three getters live; getters return the ComPtr
  members directly (`Direct3d11_Device.cpp:702-726`: `getDevice/getSwapChain/getContext`).
- `Direct3d11_Device::create()`: device+context created FIRST (`D3D11CreateDevice → &ms_device,
  &ms_context`, line 435, `FATAL` on failure); swapChain created LAST
  (`CreateSwapChainForHwnd → &ms_swapChain`, line 574). `destroy()` resets swapChain FIRST
  (line 684), then context, then device.
- ⇒ Across the swapChain's ENTIRE non-null lifetime, device and context are non-null. There is NO
  provider window where `swapChain != null && (device == null || context == null)`. The consumer's
  null-swapChain-only guard is provably sufficient; the provider already satisfies graphics-spec
  §3.3 and more strongly. The advertised-exe launch does not touch the gl11 path, so create() is
  identical to the standalone client. **No provider change.**

## Reference anchors (swg-client-v2)

- Contract source of truth: `src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.{h,inc}`
  + `src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp` (the table + thunks +
  coverage self-check + export).
- MI-thunk pattern precedent: `utinni_commandParserCtor1/2` (37-03) in `utinni_advertise.cpp`.
- External-linkage shim precedent: `utinni_installConfigFileOverride()` (`ClientMain.h`/`.cpp`, 37-01).
- gl11 graphics twin (the export pattern): `Direct3d11.cpp:856-888`.
- Spec catalog (full ~230 RVA list + signature-source `file:line` per endpoint):
  `2026-06-20-utinni-engine-entrypoint-advertisement-spec.md` §6.
