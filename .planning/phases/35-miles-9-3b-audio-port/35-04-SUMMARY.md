---
plan: 35-04
phase: 35-miles-9-3b-audio-port
status: complete
requirements: [AUDIO-02]
updated: 2026-06-18
---

# 35-04 Summary — boot smoke + D-06 four-dimension UAT

## Outcome

x64 client boots and **in-game audio works** under Miles 9.3b — the AUDIO-02 goal is met.
One narrow regression (login/character-select **screen** music) is root-caused and deferred to a
focused follow-up (see Gaps).

## What happened

- **Boot smoke:** the first boot attempt crashed at `AIL_startup` (c0000005). Root-caused via cdb to
  a stale next-to-exe `mss64.dll` (483,328 B) mismatched against the staged 9.3b provider set
  (504,320 B) — a provider/core RIB ABI mismatch. Fixed by staging the matching 9.3b core next to the
  exe in every SwgClient PostBuild config (commit `a1b94abea`). After the fix, both x64 and Win32
  reach `Audio: Finished initializing` with the room_type 9.3b bus_index=0 port logged and 0 AV.
- **D-06 UAT (human, on x64):**
  - **MUSIC (in-game):** PASS — zone/in-game music plays.
  - **2D UI SFX:** PASS — button rollovers/clicks audible immediately at all screens.
  - **3D POSITIONAL:** PASS — cantina ambient localizes with listener movement.
  - **REVERB / ROOM-TYPE:** PASS — exterior→cantina-interior dry→wet shift audible.
  - No warning-flood / half-dead-audio failure mode.
- **Win32 (D-01 regression):** boots and initializes audio clean (same 9.3b core staging fix applied).

## Key files
- created: `.planning/phases/35-miles-9-3b-audio-port/35-HUMAN-UAT.md` (UAT record)
- created: `.planning/phases/35-miles-9-3b-audio-port/35-04-LOGIN-MUSIC-FOLLOWUP.md` (root cause + fix options)
- (boot-crash staging fix committed under `a1b94abea`: `SwgClient.vcxproj`)

## Gaps (deferred follow-up — does NOT block AUDIO-02 in-game goal)
- **Login/character-select screen music is silent** (pre-game). Regression from the 7.2e→9.3b swap.
  ROOT CAUSE (cdb-proven): MSS 9.3b `AIL_service_stream` reads stream chunks through Miles's own
  **async I/O subsystem** (`MilesAsyncFileRead`), NOT the synchronous `AIL_set_file_callbacks` the
  engine registers — so the TRE-archive-backed title stream (`music/mus_title_lp.mp3`) gets only the
  one 8 KB buffer read during `AIL_open_stream` and then starves. In-game music is unaffected because
  it uses the in-memory buffered-sample path (`s_bufferedMusicSample` / `AIL_set_named_sample_file`),
  like UI sounds. Full analysis + 3 candidate fixes in `35-04-LOGIN-MUSIC-FOLLOWUP.md`.

## Diagnostics produced (kept for the follow-up)
- `tools/setup/audio-stream-probe.ps1` + `.cdb` — cdb stream-servicing probe.
- `.planning/research/CONSULT-35-*` — cross-AI consult (Codex/Cursor/Opus/Sonnet) + evidence.
- `.planning/research/miles-chm/` — decompiled Miles 9.3b help (the streaming docs that confirmed the async model).

## Self-Check: PASSED
- [x] Boot smoke green (after `a1b94abea`); audio init completes both platforms.
- [x] D-06: all four IN-GAME audio dimensions pass (AUDIO-02 goal met).
- [x] Login-screen music regression root-caused and tracked as a follow-up (not a phase blocker).
