---
status: complete
phase: 35-miles-9-3b-audio-port
source: [35-04-PLAN.md]
started: 2026-06-18
updated: 2026-06-18
---

## Current Test

[complete — D-06 four-dimension UAT run on x64; all 6 dimensions pass (login music fixed 5a894d327)]

## Tests

### 1. MUSIC (in-game)
expected: background/zone music plays in-game
result: pass

### 2. 2D UI SFX
expected: button rollover/click SFX audible (no half-dead-audio signature)
result: pass — audible immediately at all screens including login

### 3. 3D POSITIONAL
expected: cantina ambient localizes as the listener turns/moves
result: pass

### 4. REVERB / ROOM-TYPE
expected: audible dry→reverberant shift entering the cantina interior
result: pass

### 5. Win32 audio smoke (D-01 regression)
expected: Win32 client boots + audio initializes (music + UI)
result: pass

### 6. Login/character-select SCREEN music
expected: title music plays at the login/character-select screen
result: pass — FIXED 2026-06-18 (commit 5a894d327). Root cause was engine-side, not a Miles version
        difference (9.3v lead was a red herring): Miles' background IO thread re-opens the stream via
        our AIL_set_file_callbacks hook with an EMPTY filename → TreeFile::open("") → NULL → starved
        buffers. Fix (Audio.cpp, gated s_titleMusicStreamFix): serve the empty-name re-open from the
        remembered stream name + track/seek read offset across re-opens + close prior handles. Plays
        start-to-finish, transitions to zone music, handles bounded.

## Summary

total: 6
passed: 6
issues: 0
pending: 0
skipped: 0
blocked: 0

## Gaps

- (none) — all six dimensions pass. Login-screen music was the lone gap; FIXED 2026-06-18 (5a894d327).
