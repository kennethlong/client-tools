# Provider Handback — MISC/INPUT editor-unlock §4 batch (swg-client-v2 → Utinni)

**From:** swg-client-v2 (provider) · **To:** Utinni (consumer) · **Date:** 2026-06-24
**Re:** `24-PROVIDER-REQUEST-misc-input-editor-unlock.md` (copied here as
`2026-06-24-utinni-misc-input-editor-unlock.md`).
**Status:** §4 near-term batch DONE + build-gated. §5 NOT started (correctly — later wave). §6 answered below.
**Contract version:** `ENGINE_HOOKPOINTS_VERSION` **3 → 4**. **Not committed yet** (awaiting maintainer).

---

## 0. TL;DR

All four §4 items handled. Three are NAME ADDs (not two — see §1 accounting), one is an address re-point,
plus the new out-of-line accessor. The requested `cuiChatWindow::ctor` real-entry is **infeasible as
specified** (you cannot take a C++ ctor's address); delivered the equivalent via the sole construction
funnel instead. Build gate green: Release/Win32, 0 unresolved, `GetEngineHookPoints`(exe)+`GetHookPoints`(gl11)
present, contract self-check 97 rows == 97 names.

**Sync:** re-copy these two byte-identical into `D:/Code/Utinni/UtinniCore/swg/` (they are also the genericized
`ENGINE_`-token files — see §5 note, this closes the pending EngineHook rename on your side in the same copy):
```
engine_hookpoints.h    sha256 fbb83f90f95c54ca797418ee3ec709114abe8d242064c45df5bb82ffa1b03f59
engine_hookpoints.inc  sha256 85a7e63b2f5111c81313bfa7207eff632d79bac3ead87beac0b30001029e9c7a
```

---

## 1. Version accounting (correction to the request)

The request §4 said "two NAME ADDs → bump 3→4". It is actually **THREE name adds** — 4b adds a name too:

| item | kind | contract name | version impact |
|---|---|---|---|
| 4a | address re-point (name stable) | `game::mainLoop` | none (rides v4) |
| 4b | **NAME ADD** + new accessor | `game::g_mainLoopCounter` | +name |
| 4c | **NAME ADD** | `treeFile::searchTree` | +name |
| 4d | **NAME ADD** | `cuiChatWindow::createNewWindow` | +name |

Net: 94 → **97** names. Version 3 → 4 (a bump is a bump regardless of count). The `.inc` + table + the
`utinni_verifyNoNullNoDup()` name-set self-check are all consistent at 97.

---

## 2. §4a — `game::mainLoop`: RE-POINTED to the real per-frame tick (no consumer adapter needed)

**Ground truth (verified in source):**
- `Game::run()` [Game.h:96 / Game.cpp:1029] is the **OUTER once-per-process loop**:
  `install(A_client); ms_loops=0; while(!isOver()) runGameLoopOnce(false,NULL,0,0); ...cleanup`. Returns
  only at shutdown. (The literal `// THE loop.` comment is at Game.cpp:1038.)
- The **per-frame tick** is `static void Game::runGameLoopOnce(bool presentToWindow, HWND hwnd, int width,
  int height)` [Game.h:103 / Game.cpp:1059] — one frame's work per call (`Os::update`, scheduler alter,
  `DirectInput::update`, network, `Clock::limitFrameRate`, `++ms_loops`). **`__cdecl`, EXACT parameter-list
  match to your `hkMainLoop` `__cdecl(bool presentToWindow, HWND, int width, int height)`.**

**Action:** re-pointed `game::mainLoop` from `&Game::run` → `&Game::runGameLoopOnce`. **Name unchanged**, so
your existing `s_bindings[]` entry for `game::mainLoop` now resolves to the correct per-frame function with
**no rebind and no ABI adapter** — just drop the `Game::detour` wholesale-skip and add the `installable()`
gate. (This is better than the §4a "may need a consumer-side adapter" guess: the signature already matches.)
If you ALSO need the outer `Game::run` (once-per-process), ask and I'll add a separate `game::run` row.

## 3. §4b — `game::g_mainLoopCounter`: new out-of-line accessor

- Counter is `Game::ms_loops` (private `static int` [Game.h:276 / Game.cpp:403], `++` at Game.cpp:1248, once
  per `runGameLoopOnce`). `getLoopCount()` [Game.h:190] is **inline** → no ODR address (why you OMITTED it).
- **Added** `static int Game::getMainLoopCount()` (decl Game.h:191, def Game.cpp ~:1027) — out-of-line twin
  returning `ms_loops`, mirroring the existing `isOver()` accessor pattern. Advertised `game::g_mainLoopCounter
  → &Game::getMainLoopCount` (ACCESSOR / call-not-read). Read this instead of the hardcoded `0x1908830`.

## 4. §4c — `treeFile::searchTree`: real search-path registration (collision resolved) — ⚠️ ABI deltas

- `treeFile::open` stays `&TreeFile::open` (the static file-open; unchanged).
- **Added** `treeFile::searchTree → &TreeFile::addSearchTree`. `static void addSearchTree(const char* fileName,
  int priority)` [TreeFile.h:78] — registers a `.tre` at a search priority (the real twin of SWGEmu `searchTree`).
- **⚠️ Two ABI deltas you MUST adapt for** (TreeFile is an all-static class — no instances):
  1. **`static __cdecl`, NOT `__thiscall`** — there is **no `pThis`**. Your `hkSearchTree` typedef
     `__thiscall(pThis, priority, name)` must become `__cdecl(const char* fileName, int priority)`.
  2. **Arg order is REVERSED** — ours is `(fileName, priority)`; SWGEmu is `(priority, treeFilename)`. Swap.
- The loose-directory sibling `addSearchPath(const char* path, int priority)` [TreeFile.h:76] is the better
  target if the feature is loose-override *directory* reload rather than `.tre` registration — say the word
  and I'll add `treeFile::searchPath`. (Advertised only the one you named to avoid a speculative row.)

## 5. §4d — `cuiChatWindow::ctor` is INFEASIBLE as specified → advertised the construction funnel

**The ctor real-entry cannot be delivered:** you cannot take the address of a C++ constructor (`&Class::Class`
is ill-formed — no ctor PMF exists, so `pmfRealEntry()` has no input; a placement-new thunk is detour-dead,
exactly the failure mode §2 warns about). This is a hard language limitation, not a tree-specific gap.

**Delivered instead — the sole construction funnel:** there is exactly **one** `new SwgCuiChatWindow(...)`
site in the tree — inside the static factory `SwgCuiChatWindow::createNewWindow(UIPage&, Game::SceneType,
std::string const&)` [decl SwgCuiChatWindow.h:258, def .cpp:1531, the `new` at .cpp:1549]. Detouring
`createNewWindow`'s entry intercepts **every** chat-window construction — the same coverage your `hkCtor`
wanted. `createNewWindow` is a normal addressable function (a static — no MI-PMF inflation), so its real
entry is clean and correct-by-construction (no brittle mangled-name `/alias`).

- Added `cuiChatWindow::createNewWindow → utinni_chatWindowCreateNewWindowEntry()`. `createNewWindow` is
  **private**, so its address is taken by a friend accessor compiled in `SwgCuiChatWindow.cpp` (`#if
  !defined(_WIN64)`), declared in the exe-local `utinni_chatWindow_forward.h` — the same private-member
  real-entry pattern as the GroundScene accessors (`utinni_groundScene_forward.h`). Added one `friend` line
  to `SwgCuiChatWindow.h` (no struct ABI change).
- **CONSUMER:** retarget `hkCtor` to **detour `createNewWindow`** (typedef `SwgCuiChatWindow* __cdecl(UIPage&,
  Game::SceneType, std::string const&)`). It fires on construction the same way the ctor detour would. (The
  raw-ctor `cuiChatWindow::ctor` name is NOT advertised — there is nothing addressable to put behind it.)

---

## 6. §6 joint decisions — provider answers

**Bucket E (mid-function NOP/byte patches) — accept SWGEmu-only this wave, with one cross-reference:**
- `debugCamera::patch`, `cuiMisc::patch` (offline-scenes / location-button / RESO.Y→CUI.X), `client::
  midCrashLogWrite`: each is a Pre-CU instruction-stream edit at a non-entry offset with low marginal value
  vs. the editor-unlock core. **Recommendation: accept SWGEmu-only on the advertised client** (no cooperative
  toggle this wave). If a specific editor genuinely needs one, name it and we'll scope a real boolean setter.
- **Issue #11 (chat modal-context routing) — check the EXISTING contract first.** We already advertise
  `config::setModalChat → CuiPreferences::setModalChat` and `config::getModalChat → CuiPreferences::getModalChat`
  (real public statics, [CuiPreferences.h:94-95]). Before treating Issue #11 as a mid-fn patch, verify whether
  driving `CuiPreferences::setModalChat()` achieves the modal-chat-context behavior — that may dissolve the NOP
  into an already-advertised cooperative setter. If it doesn't, tell us the exact behavior needed.

**Bucket F (NONEXISTENT) — no provider action, confirmed:**
- `client::setupStartDataInstall` (`0x00A9F970`) and `client::writeCrashLog` (`0x00A9F640`) have no from-source
  twin (re-confirmed; the crash `.txt` is an inline `sprintf` in `SetupSharedFoundation.cpp`). **Proceed with
  your plan:** re-gate `Client::detour` off `setupStartDataInstall` so the advertised `clientMain` + `writeMiniDump`
  install independently, and accept crash-log relocation as SWGEmu-only. **Declining** the optional crash-log-dir
  setter (low value) for now — ask if that changes.

---

## 7. §5 later wave (bucket C) — NOT started, by design

Per the request ("do not advertise it all up front… an unverified batch is a batch of `0xC0000005`s"), none of
the §5 INPUT/HUD/menu rows were added. Pick them up **one editor at a time**: name the editor you're unlocking,
I'll source-confirm + advertise its real entry (virtual→SKIP / inline→OMIT / MI→`pmfRealEntry` / static→`&fn`),
bump the version, and you wire+gate+live-smoke it.

---

## 8. Build gate + sync protocol

- **Release/Win32**, canonical 5-target, `/nodeReuse:false`, forced relink. Exit 0; **0** `unresolved external
  symbol`; **0** errors; **0** `C2248/C2338/C3865` (the 4d friend access compiled clean). `dumpbin /exports`:
  `GetEngineHookPoints` (exe) + `GetHookPoints` (gl11) present. Restaged with matching PDBs.
- Contract self-check: 97 table rows == 97 `.inc` names, no dups, no null addrs.
- **`.h`/`.inc` re-sync:** copy both byte-identical into `D:/Code/Utinni/UtinniCore/swg/` and sha256-verify
  against §0. **Note:** these are the `ENGINE_`-token (genericized) files — copying them in also completes the
  pending EngineHook rename on your consumer side (your `endpoints*.{h,cpp}` still reference `UTINNI_HOOKPOINT`/
  `UtinniEngineHookPoints`; rename them to `ENGINE_HOOKPOINT`/`EngineHookPoints` to match). Binary contract
  unchanged; this is the same source-token map from the 2026-06-23 rename handback.
- Every advertised-client unlock still lands only after a maintainer **live inject-smoke** (§7.5 of the request).

## 9. Files changed (provider side)

- `engine_hookpoints.h` — version 3→4 + history note.
- `engine_hookpoints.inc` — +3 names (`game::g_mainLoopCounter`, `cuiChatWindow::createNewWindow`, `treeFile::searchTree`).
- `engine_advertise.cpp` — 4a re-point; +3 rows; include `utinni_chatWindow_forward.h`.
- `Game.h` / `Game.cpp` — new out-of-line `getMainLoopCount()` accessor.
- `SwgCuiChatWindow.h` — one `friend` decl. `SwgCuiChatWindow.cpp` — friend accessor (`#if !defined(_WIN64)`).
- `utinni_chatWindow_forward.h` — NEW exe-local forward header.
