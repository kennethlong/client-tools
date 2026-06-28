# Utinni provider collaboration — consolidated state (context-clear checkpoint)

**Date:** 2026-06-25 · **Purpose:** single "where are we now" snapshot across the EngineHook provider work
this session. Per-request detail is in the individual handbacks below; this is the index + open-items list.

> **UPDATE 2026-06-26 — provider work REOPENED.** The "nothing pending on our side" below was true as of
> 2026-06-25 but is now stale. The consolidated ledger
> [`2026-06-26-utinni-provider-outstanding-editor-unlock.md`](2026-06-26-utinni-provider-outstanding-editor-unlock.md)
> (Utinni Phase 24) reopens per-editor provider work in buckets A–F.
> **Bucket B (Effects editor live preview) is DONE** — `ENGINE_HOOKPOINTS_VERSION` **6 → 7, 104 names**,
> commit `db3ca5895`, build-gated + staged, maintainer-smoke pending. See
> [`2026-06-26-effects-render-retrigger-HANDBACK.md`](2026-06-26-effects-render-retrigger-HANDBACK.md).
> **Remaining (priority order, one wave per smoke):** A (per-editor real-entry rows: chat ctor, free-cam,
> world-pick/HUD, sys-msg/input) · C (virtual vtable — consumer-side) · D (mid-function joint toggles) ·
> E (WS-5 scene-ready callback) · F (crash-log setter).

## Where the contract stands

- **`ENGINE_HOOKPOINTS_VERSION` = 6, 99 names.** Provider files:
  `src/game/client/application/SwgClient/src/shared/engine_hookpoints.{h,inc}` (the shared contract) +
  `.../src/win32/engine_advertise.cpp` (table + thunks + `ensureDynamicRowsFilled` + export).
- **Staged `SwgClient_r.exe` (Release/Win32) carries the full v6 catalog** + the static-init race fix + the
  setupScene re-map + the loadScene add + the in-world CUI reflow fix. Built 2026-06-25 ~14:59.
- The advertise TU is **32-bit only** (`#if !defined(_WIN64)`); x64 has no contract by design. Consumer runs
  against `SwgClient_r.exe`.

## This session's chain (newest first) — all landed, gated, pushed

| commit | what | contract |
|---|---|---|
| `63119bd03` | **game::loadScene** — full SceneCreator-lifecycle editor scene-load (`setScene(true,terrain,player,nullptr)`); fixes half-integrated-scene throw | v5→**6** (NAME ADD → re-sync) |
| `883d7a0fc` | **static-init race fix** (40→96: 29 call-rows → constant placeholders + lazy `ensureDynamicRowsFilled` on reader's thread) **+ setupScene re-map** (`_setScene(Scene*)` thunk, fixed `0xC0000005`) | no change (both) |
| `56b2cc4a3` | **v5** — `treeFile::enumerateFiles` (editor file pickers) + in-world CUI reflow (back-buffer size) | v4→**5** |
| `80370ac4f` | **v4** — MISC/INPUT §4 batch: `game::mainLoop` re-point, `game::g_mainLoopCounter`, `treeFile::searchTree`, `cuiChatWindow::createNewWindow` | v3→**4** |
| `734299751` | **EngineHook rename** — dropped `Utinni` branding from the shared contract (UTINNI_→ENGINE_) | v3 (rename only) |

Per-request handbacks (read for detail): `2026-06-25-utinni-scene-load-lifecycle-HANDBACK.md`,
`2026-06-25-utinni-table-static-init-race-HANDBACK.md`, `2026-06-25-utinni-setupscene-remap-HANDBACK.md`,
`2026-06-24-utinni-treefile-enum-and-inworld-reflow-HANDBACK.md`,
`2026-06-24-utinni-misc-input-editor-unlock-HANDBACK.md`, `2026-06-23-enginehook-rename-provider-handback.md`.

## OPEN — all on the Utinni / maintainer side (NOT our action; we never touch `D:/Code/Utinni`)

1. **Utinni re-sync to v6.** Copy `engine_hookpoints.{h,inc}` byte-identical into `UtinniCore/swg/`
   (sha256 in the v6 handback: .h `b869747687dc1bef…`, .inc `12143e1f1459aef6…`) AND finish the
   `UTINNI_`→`ENGINE_` token rename in their consumer (`endpoints*.{h,cpp}` still carried `UTINNI_` per the
   rename handback). Then bind `game::loadScene` + switch "Load scene" to pass `(terrain, avatar)` filenames
   (stop building a GroundScene).
2. **Maintainer live inject-smokes** (the ground truth that headless gates can't replace):
   - Re-inject → `utinni_init` resolver should jump **40/96 → 96/96** (static-init race fix).
   - In-world "Load scene" → loads + renderable, **no Fatal/throw** (loadScene) and **no `0xC0000005`** in
     `GroundScene::GroundScene` (setupScene re-map).
   - Editor file pickers populate (`treeFile::enumerateFiles`); in-world minimap/toolbar/chat not clipped
     (CUI reflow).

## Notes for whoever picks this up

- **Same-name re-maps don't bump version / don't re-sync; NAME ADDs do.** Keep `.h`/`.inc` byte-identical with
  Utinni; provide sha256s in the handback.
- **The 29 static-init placeholders + `ensureDynamicRowsFilled` are load-bearing:** any NEW function-call row
  (pmfToVoid/pmfRealEntry/*Entry()) must be added as a `{name,0}` placeholder AND to the `dyn[]` fill list, or
  it'll be null on the consumer's pre-resume read. Constant `(void*)&Symbol` rows are fine as-is.
- **`game::setupScene`** is the low-level `_setScene(Scene*)` set-pre-built primitive; **`game::loadScene`** is
  the full filename-based load. Don't conflate them.
- Build/gate recipe unchanged: Release/Win32, `/nodeReuse:false`, delete `SwgClient_r.exe` to force relink,
  grep log for `unresolved external symbol` (must be 0), dumpbin for undecorated `GetEngineHookPoints`.
