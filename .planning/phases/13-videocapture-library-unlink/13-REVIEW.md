---
phase: 13-videocapture-library-unlink
reviewed: 2026-05-26T01:44:14Z
depth: standard
files_reviewed: 6
files_reviewed_list:
  - src/engine/client/library/clientUserInterface/src/shared/core/CuiIoWin.cpp
  - src/engine/client/library/clientGame/src/shared/core/Game.cpp
  - src/engine/client/library/clientGame/src/shared/core/Game.h
  - src/engine/client/library/clientAudio/src/win32/Audio.cpp
  - src/engine/client/library/clientAudio/src/win32/Audio.h
  - src/game/client/library/swgClientUserInterface/src/shared/parser/SwgCuiCommandParserScene.cpp
findings:
  critical: 0
  warning: 1
  info: 0
  total: 1
  resolved: 1
status: resolved
---

> **Resolution (post-review):** WR-01 fixed — the two orphaned file-statics deleted from
> Audio.cpp; incremental Debug rebuild re-confirmed the link gate (2138 Searching / 0 unresolved
> / 0 LNK1181). No other findings. Committed alongside this report.

# Phase 13: Code Review Report

**Reviewed:** 2026-05-26T01:44:14Z
**Depth:** standard
**Files Reviewed:** 6
**Status:** issues_found

## Summary

Phase 13 unlinks the VideoCapture/AudioCapture middleware (DECRUFT-04) by removing
all references across 6 source files: one live `#if PRODUCTION == 0` caller block in
`CuiIoWin.cpp::draw()`, dead `#if 0` recorder source residue in `Game.cpp`/`Audio.cpp`,
the AudioCapture/VideoCapture namespace surface in `Game.h`/`Audio.h`, and the dead
command-parser plumbing in `SwgCuiCommandParserScene.cpp`.

The removals are almost entirely clean. I verified each of the specific risk categories
called out for this phase:

- **No dangling references to removed symbols.** A repo-wide grep for
  `videoCapture|VideoCapture|AudioCapture|getAudioCaptureConfig|startAudioCapture|stopAudioCapture|SwgVideoCapture|SwgAudioCapture`
  across `src/` returns zero matches. No orphaned callers, no missed includes.
- **No broken control flow / orphaned braces.** Preprocessor `#if`/`#endif` counts are
  balanced in every file (Audio.cpp 29/29, Game.cpp 17/17, Game.h 1/1, Audio.h 2/2,
  Scene.cpp 11/11). In `SwgCuiCommandParserScene.cpp` the `#endif // PRODUCTION` at
  line 531 correctly closes the surviving `#if PRODUCTION == 0` block at line 427 — only
  the commented-out `/* ... */` cmds-table rows inside that guard were removed, not the
  guard itself.
- **The live `if/else if/else` chain in `performParsing` reconnects correctly.** The
  removed `#if 0 ... else if (...) {...} #endif` blocks were never compiled, so the live
  chain (`else if (ms_listGcwGroupsData) {...}` at 4871-4883 → `else {...}` at 4887-4890)
  is intact.
- **Live Miles `AIL_*` playback is untouched.** The deletions in `Audio.cpp` were confined
  to the three `#if 0` AudioCapture functions; no live `AIL_*` call sites were touched.
- **The `CuiIoWin.cpp::draw()` removal is a deliberate behavior change**, not a defect — the
  `#if PRODUCTION == 0 VideoCapture::SingleUse::run(); #endif` call was live in dev builds and
  its removal is exactly the intended middleware unlink. The surrounding draw flow
  (`IoWin::draw()` → `Bloom::postSceneRender()`) reconnects cleanly.

One leftover defect remains: removing the AudioCapture functions in `Audio.cpp` orphaned
two file-static variables that were exclusively consumed by those functions. They are now
dead state. See WR-01.

## Warnings

### WR-01: Orphaned file-static variables left after AudioCapture function removal  — ✅ RESOLVED

**Resolution:** Both file-statics deleted from `Audio.cpp` (the Miles include kept — `HPROVIDER` still used at Audio.cpp:57). Incremental Debug rebuild: 0 errors, link gate 2138/0/0.

**File:** `src/engine/client/library/clientAudio/src/win32/Audio.cpp:170-171`

**Issue:** The removed `#if 0` AudioCapture functions (`getAudioCaptureConfig`,
`startAudioCapture`, `stopAudioCapture`) were the *only* readers/writers of these two
file-static variables in `AudioNamespace`:

```cpp
HPROVIDER s_audioFilterProvider = 0;
HDRIVERSTATE s_audioFilter = 0;
```

A repo-wide grep confirms these two symbols now appear *only* at their own declarations
(`Audio.cpp:170` and `:171`) — there are no remaining uses anywhere in `src/`. Unlike the
removed function bodies (which lived in `#if 0`/`#if PRODUCTION == 0` blocks and never
compiled), these statics sit in plain unguarded namespace scope, so they *are* compiled in
every configuration and are now dead, write-only-at-init state. This is the kind of
now-unused leftover the removal should have cleaned up alongside the functions. Most MSVC
warning levels will not flag namespace-scope file-static globals as unused, so this will
slip past the clean-link gate silently.

Note: `HDRIVERSTATE` is now referenced only by the dead `s_audioFilter`, but `HPROVIDER` is
still used legitimately at `Audio.cpp:57` (`typedef std::map<HPROVIDER, ProviderData> ProviderMap`),
so the Miles header include must stay — do not remove it.

**Fix:** Delete both orphaned declarations to complete the decruft:

```cpp
// Audio.cpp, remove lines 170-171:
-	HPROVIDER s_audioFilterProvider = 0;
-	HDRIVERSTATE s_audioFilter = 0;
```

---

_Reviewed: 2026-05-26T01:44:14Z_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
