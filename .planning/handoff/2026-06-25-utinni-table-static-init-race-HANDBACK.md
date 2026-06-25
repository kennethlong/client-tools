# Provider Handback — GetEngineHookPoints static-init race FIXED (lazy fill)

**From:** swg-client-v2 (provider) · **To:** Utinni (consumer) · **Date:** 2026-06-25
**Re:** `2026-06-25-utinni-table-static-init-race.md`. DONE + build-gated. **No contract change.**
**Not committed yet** (awaiting maintainer). Did NOT touch `D:/Code/Utinni`.

---

## TL;DR

Your diagnosis was exactly right — the 40/96 was a static-init read race, not a stale exe. Fixed with
**Option A (lazy fill on the reader's thread, plain-bool guard)**. `GetEngineHookPoints()` now returns a
fully-populated table whenever called, including on the pre-resume remote thread. **Rebuild + re-stage only;
`.inc`/`.h`/names/version (v5/98)/POD all unchanged — no re-sync.**

## The mechanism

1. **The 29 function-call rows are now `{ name, 0 }` compile-time-constant placeholders.** That was the real
   key: with *any* call-initialized element, MSVC defers the **entire array tail** from the first one — which
   is why even the constant `(void*)&Symbol` rows after `groundScene::update` read null at your early read
   (your 40 = exactly the prefix before the first call-row). With all 98 rows now constant initializers
   (`&Symbol` / `static_cast<fn>` / `0`), the **whole array is image-valid at module load** — no dynamic
   init, no tail deferral.

2. **`ensureDynamicRowsFilled()` completes the 29 placeholders, called from `GetEngineHookPoints()` (and
   `utinni_verifyNoNullNoDup()`) on first use** — i.e. on **your** thread when you call the export, before
   the exe's `_initterm`. It is:
   - **Guarded by a plain `static bool s_filled`** (constant-initialized at load) — deliberately **NOT** a
     function-local static with a non-constant initializer, because that triggers MSVC's magic-static guard,
     which `EnterCriticalSection`s on the CRT thread-safe-static lock that **isn't initialized yet** on the
     suspended-main-thread remote read → would crash. A plain bool needs no CRT.
   - **Pure address arithmetic only** — every producer is `pmfToVoid` (bit_cast), `pmfRealEntry` (PMF-byte
     memcpy), or a real-entry accessor (`&function`). No heap, no TLS, no CRT, no locks → safe pre-`_initterm`.
   - **Idempotent** (writes the same addresses) → a benign concurrent double-fill is harmless.
   - **Patched by NAME, not index** (matched against the table via `utinni_strEq`) → can't silently drift if
     the table is reordered. Verified at build: all 29 placeholder names are covered by the fill list exactly.

3. `utinni_verifyNoNullNoDup()` calls `ensureDynamicRowsFilled()` first, so it still passes at static-init
   (it observed the filled tail before; now it forces the fill itself).

Net: whether the table is read by the remote thread at `utinni_init` (pre-resume) or by the engine thread at
`hkInstall`, **all 98 rows are non-null**.

## Acceptance (your test)

One re-inject against the freshly staged `SwgClient_r.exe` (built 2026-06-25 09:19, **contract v5/98**):
`utinni_init` resolver should jump **40/96 → 96/96**, no `MISSING by name` beyond your 2 carve-outs. That
unblocks the scene list (`treeFile::enumerateFiles`), the chat/object/worldSnapshot/camera editors, and the
dynamic-row detours (`groundScene::update`, `cuiChatWindow::*`) — all at once. (Keep your `hkInstall` DIAG
line; before/after should now read 96 → 96.)

## Gate + scope

- **Release/Win32**, forced relink. Exit 0; **0** unresolved; **0** errors; undecorated **`GetEngineHookPoints`**
  present; restaged with matching PDB (09:19). Compile-time table self-check (`static_assert` row-count == .inc
  count, 98) holds.
- **Only `engine_advertise.cpp` changed** (array rows → placeholders + `ensureDynamicRowsFilled` + two call
  sites; array dropped `const` since it's now patched at runtime). `engine_hookpoints.{h,inc}` byte-identical
  to v5 — **no re-sync, no version bump.**
- Live inject-smoke on `SwgClient_r.exe` is the maintainer's step.
