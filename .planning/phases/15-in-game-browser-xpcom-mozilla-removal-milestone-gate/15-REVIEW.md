---
phase: 15-in-game-browser-xpcom-mozilla-removal-milestone-gate
reviewed: 2026-05-27T01:04:29Z
depth: standard
files_reviewed: 10
files_reviewed_list:
  - src/engine/client/library/clientGame/src/shared/core/Game.cpp
  - src/engine/client/library/clientUserInterface/src/shared/core/CuiIoWin.cpp
  - src/engine/client/library/clientUserInterface/src/shared/core/IMEManager.cpp
  - src/external/3rd/library/ui/src/shared/core/UITypeID.h
  - src/game/client/library/swgClientUserInterface/src/shared/core/SwgCuiManager.cpp
  - src/game/client/library/swgClientUserInterface/src/shared/core/SwgCuiTcgManager.cpp
  - src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiHud.cpp
  - src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiHudAction.cpp
  - src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiTcgControl.h
  - src/game/client/library/swgClientUserInterface/src/shared/parser/SwgCuiCommandParserUI.cpp
findings:
  critical: 0
  warning: 3
  info: 2
  total: 5
status: issues_found
---

# Phase 15: Code Review Report

**Reviewed:** 2026-05-27T01:04:29Z
**Depth:** standard
**Files Reviewed:** 10
**Status:** issues_found

## Summary

Reviewed the source-logic de-wiring for Phase 15 (XPCOM/Mozilla in-game-browser removal). The mechanical de-wiring is clean and consistent: all `libMozilla` includes removed, the `TUIWebBrowser` enum value deleted, the 5 `IsA(TUIWebBrowser)` focus-routing disjuncts dropped, the TCG navigate-callback registration severed, and the `SwgCuiWebBrowserManager` install/remove/update lifecycle calls removed. No dangling `TUIWebBrowser`/`SwgCuiWebBrowser`/`libMozilla` references remain in compiled source (the only residual "WebBrowser" hits are in the unrelated vendored `atlmfc` COM library). `libEverQuestTCG` is preserved intact, and the sibling list/table widget UIs are untouched.

Two correctness concerns survive the de-wiring, both centered on the fact that `TUIWebBrowser` was being used as a **shared input-routing identity** by code that was NOT the browser:

1. **The TCG card-game control (`SwgCuiTcgControl`) deliberately masqueraded as `TUIWebBrowser`** to receive keyboard/navigation focus routing. Deleting the enum value AND removing the control's `IsA(TUIWebBrowser)` claim together severs that control's text/nav input path. This is collateral scope over-deletion — the requirement explicitly states `libEverQuestTCG` and the TCG path must remain intact. Mitigated to WARNING because the HUD entry point that activates the TCG window is currently commented out, but it is a latent functional regression.
2. **The TCG navigate callbacks are now never registered**, leaving the `libEverQuestTCG` callback table's `navigateProc`/`navigateWithPostDataProc` members as NULL (zero-init namespace statics) when handed to the closed-source TCG DLL.

There is also a dead-store left behind in the customer-service action handler (`finalUrl` is computed but never consumed). The remaining items are informational (commented-out code blocks and a debug-command removal).

## Enum ordinal-shift hazard — checked, NOT a defect

`TUIWebBrowser` was removed from the middle of the `UITypeID` enum, shifting `TUIStyle` and everything after it down by one ordinal. This was verified to be safe: `UIBaseObject::IsA` (UIBaseObject.h:323) performs a direct compile-time `Type == TUIxxx` comparison, the enum value is never serialized to disk, and widget instantiation is by string `TypeName` rather than numeric ordinal. All comparisons recompile consistently against the new ordinals. (This is the opposite of the radial-menu enum case where ordinals are persisted datatable row indices.) No action required.

## Warnings

### WR-01: TCG control loses keyboard/navigation input routing (severed shared identity)

**File:** `src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiTcgControl.h:91-94` (combined with the 5 dropped `IsA(TUIWebBrowser)` disjuncts in CuiIoWin.cpp:513 & :647, IMEManager.cpp:353, SwgCuiHud.cpp:1025)
**Issue:** Before this change, `SwgCuiTcgControl::IsA` returned `Type == TUIWebBrowser || UIWidget::IsA(Type)` — the TCG card-game widget intentionally answered `true` to `IsA(TUIWebBrowser)`. The focus-routing predicates in `CuiIoWin::parseMessage` (`isTextMessage` block), `CuiIoWin::processEvent` (`textIsFocused`/`navigationIsFocused`), `IMEManager::handleProc`, and `SwgCuiHud::OnMessage` each included `focused->IsA(TUIWebBrowser)` so that, when a TCG control had focus, arrow/navigation/Enter/Tab/Backspace/Delete and character input were delivered to the focused widget. `SwgCuiTcgControl::ProcessMessage` (SwgCuiTcgControl.cpp:322-371) consumes exactly those keys and forwards them to the TCG game via `m_eqTcgWindow->onKeyDown/onKeyUp`. By deleting the `TUIWebBrowser` enum value, dropping the 5 disjuncts, AND removing the control's `TUIWebBrowser` self-identification all at once, the TCG control no longer matches any text/navigation focus predicate. Result: when the TCG window is focused, navigation keystrokes leak past it to the game input map (movement/hotkeys) / chat instead of reaching the TCG game. There is no `TUITcg`-style replacement enum value. Mouse input is unaffected (it does not depend on these predicates). Severity is WARNING rather than BLOCKER because the HUD action that launches the TCG window (`SwgCuiHudAction.cpp:1595-1606`) is fully commented out, so the control is not currently reachable in normal play — but the regression is real if/when the TCG path is re-enabled, which the phase scope says must remain intact.
**Fix:** Reintroduce a stable type identity for the TCG control instead of relying on the now-deleted browser enum. Add a dedicated value (e.g. `TUITcgControl`) to `UITypeID` *appended at the end of the TUIWidget group* (do not reinsert mid-enum) and have the focus-routing predicates and `SwgCuiTcgControl::IsA` reference it:
```cpp
// UITypeID.h, in the TUIWidget block (append, e.g. after TUIRunner)
TUITcgControl,
// SwgCuiTcgControl.h
inline bool SwgCuiTcgControl::IsA(const UITypeID Type) const
{
    return Type == TUITcgControl || UIWidget::IsA(Type);
}
// and re-add `|| focused->IsA(TUITcgControl)` to the 4 focus-routing call sites
```
Alternatively, if the TCG mini-game is being deliberately retired alongside the browser, document that explicitly and remove the dead TCG launch path too — but that is a larger scope decision, not part of "browser removal."

### WR-02: TCG navigate callbacks now passed to the TCG DLL as NULL

**File:** `src/game/client/library/swgClientUserInterface/src/shared/core/SwgCuiTcgManager.cpp:65-70` (was lines registering `setNavigateCallback`/`setNavigateWithPostDataCallback`)
**Issue:** `SwgCuiTcgManager::install` no longer calls `libEverQuestTCG::setNavigateCallback(navigateProc)` / `setNavigateWithPostDataCallback(navigateWithPostDataProc)`. These callbacks were the TCG client's way of asking the host to open a URL (they previously delegated to `SwgCuiWebBrowserManager`, which is correctly removed). The `libEverQuestTCG::PrivateData::callbackTable` is a namespace-scope static (`libEverQuestTCG.cpp:144,153`), so `navigateProc`/`navigateWithPostDataProc` are zero-initialized to NULL and that table is handed to the closed-source TCG DLL via `pInitializeProc(..., &PrivateData::callbackTable)` (libEverQuestTCG.cpp:532). Behaviorally, any in-TCG action that previously triggered a navigation (e.g. "buy more cards"/store links) is now a silent no-op at best. If the TCG DLL invokes the callback without a null-check, it would call through a NULL function pointer (crash); this cannot be confirmed from source since the DLL is closed. The other audio/window-state callbacks remain registered, so the table itself is still expected by the DLL.
**Fix:** This is intentional collateral of browser removal, but it should be made explicit and safe rather than left as a silent NULL. Either (a) register a small stub that logs/ignores the navigate request so the DLL always has a non-NULL pointer, or (b) confirm via the DLL contract/testing that NULL navigate callbacks are tolerated, and record that confirmation in the phase notes. Recommended (a):
```cpp
void __stdcall navigateStub(const char * /*url*/) {}
void __stdcall navigateWithPostDataStub(const char * /*url*/, const char * /*postData*/) {}
// in install():
libEverQuestTCG::setNavigateCallback(navigateStub);
libEverQuestTCG::setNavigateWithPostDataCallback(navigateWithPostDataStub);
```

### WR-03: Dead-store — `finalUrl` computed but never used in customer-service action

**File:** `src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiHudAction.cpp:1170-1190`
**Issue:** In the `CuiActions::service` handler, the entire HTTP-parameter build and `finalUrl = HttpGetEncoder::getUrl(...)` computation (lines 1081-1189) now produces a result that is never consumed — the only consumers (`SwgCuiWebBrowserManager::createWebBrowserPage(false)` / `setURL(finalUrl.c_str(), true)`) were removed, leaving `finalUrl` and the surrounding `httpParams` work as a dead store followed by a bare `}`. Functionally the Customer Service browser no longer opens at all. The truncation branch (line 1175-1188) can still early-`return false`, so the only remaining effect of this block is potentially returning false on an over-length URL that is never used. This is over-deletion residue: the call site was de-wired but the now-purposeless computation was left in place.
**Fix:** Decide the intended post-removal behavior for the CS action. If CS is meant to launch the system browser instead, replace the removed calls with a `ShellExecute`/`Os`-level open of `finalUrl` (the commented `//ShellExecute(...)` at line 1189 hints at the original alternative). If CS is being retired with the browser, remove the dead `httpParams`/`finalUrl` build entirely (lines ~1081-1189) so the handler does not compute and discard a URL. As-is the block is misleading and the feature is silently broken.

## Info

### IN-01: Removed `browser`/`url` parser commands were already dead (commented out)

**File:** `src/game/client/library/swgClientUserInterface/src/shared/parser/SwgCuiCommandParserUI.cpp:554-573` (removed block) and CommandNames `browser`/`url` (removed)
**Issue:** The `browser` and `url` command handlers removed from `performParsing` were inside a `/* ... */` comment block before this change, so no live command behavior was lost. The corresponding `CommandNames::browser`/`url` string constants and their (absent) command-table rows were correctly removed alongside. Pure cleanup — noted only to confirm the removal did not drop a live command path.
**Fix:** None required.

### IN-02: `debugBrowserOutput` (`mozillaBrowserOutput`) debug command removed cleanly

**File:** `src/game/client/library/swgClientUserInterface/src/shared/parser/SwgCuiCommandParserUI.cpp` (CommandNames + command-table row + handler, all under `#if DEBUG==0`)
**Issue:** The live `mozillaBrowserOutput` debug command and its `SwgCuiWebBrowserManager::debugOutput()` handler were removed symmetrically (declaration, command-table entry, and dispatch branch all gated by the same `#if DEBUG==0`). No mismatch between the command name list and the dispatch chain was introduced. The `#if DEBUG==0` gating remains balanced.
**Fix:** None required.

---

_Reviewed: 2026-05-27T01:04:29Z_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
