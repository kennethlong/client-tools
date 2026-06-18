---
status: partial
phase: 35-miles-9-3b-audio-port
source: [35-04-PLAN.md]
started: 2026-06-18
updated: 2026-06-18
---

## Current Test

[complete — D-06 four-dimension UAT run on x64; one deferred gap]

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
result: issue — silent (pre-game). Root-caused (MSS 9.3b async stream I/O vs sync TRE callbacks).
        Deferred to follow-up; does not affect in-game audio.

## Summary

total: 6
passed: 5
issues: 1
pending: 0
skipped: 0
blocked: 0

## Gaps

- login-screen-music-silent — streamed title music starves under MSS 9.3b (async stream I/O path does
  not use the engine's synchronous TRE file callbacks). See 35-04-LOGIN-MUSIC-FOLLOWUP.md. Narrow,
  pre-game; in-game audio (AUDIO-02) is fully working.
