# Phase 14 — Deferred / Out-of-Scope Items

## DEF-14-01: SwgClient Optimized-config SAFESEH link failure (LNK1281 / LNK2026)

**Discovered during:** Plan 14-01 Task 3 build gate (2026-05-26)
**Status:** Pre-existing, out-of-scope, NOT caused by voice removal.

When building `SwgClient` in the **Optimized|Win32** configuration, the link step
fails with:
- 28× `LNK2026: module unsafe for SAFESEH image` (zlib.lib, dinput8.lib,
  libpcre.a, and other prebuilt 3rd-party libs)
- 2× `LNK1281: Unable to generate SAFESEH image`

No exe is produced for the Optimized config.

**Root cause (config gap, not voice-related):** The `SwgClient.vcxproj` linker
block disables SAFESEH for **Debug** (`/SAFESEH:NO` in `<AdditionalOptions>`,
line 110) and **Release** (`<ImageHasSafeExceptionHandlers>false</...>`, line 211),
but the **Optimized** `<Link>` block has NEITHER. The prebuilt 3rd-party libs are
not SAFESEH-compatible, so the Optimized link cannot generate a SAFESEH image.

**Why this is NOT a 14-01 regression:**
- The voice-removal link gate (D-09) measures `unresolved external symbol` count.
  Optimized = **0 unresolved externals** (Debug = 0, Release = 0). The voice symbol
  graph is fully resolved in all three configs — removing the voice source/tokens
  introduced zero unresolved symbols.
- The Optimized error log contains **0** `vivox`/`SwgVivox`/`VoiceChat`/`CuiVoiceChat`
  references. The failure is purely SAFESEH image generation against prebuilt libs.
- The Optimized config had **never been built** in this repo before 14-01 (no
  `src/compile/win32/SwgClient/Optimized/` output dir existed). The validated client
  configs are Debug (`SwgClient_d.exe`) and Release (`SwgClient_r.exe`), both of which
  link clean with the voice subsystem removed.

**Suggested fix (future, separate from Decruft):** Add `/SAFESEH:NO` to the
Optimized `<Link>`/`<AdditionalOptions>` (mirror Debug line 110) or set
`<ImageHasSafeExceptionHandlers>false</...>` (mirror Release line 211) in the
Optimized ItemDefinitionGroup. This is a build-config fix, not a voice-removal task.

## DEF-14-02: GATE-2 `getVoice` token collides with preserved soePlatform/ChatAPI2 community-chat methods

**Discovered during:** Plan 14-03 Task 1 GATE-2 repo-wide voice-symbol grep (2026-05-26)
**Status:** Benign false-positive in a deliberately-PRESERVED tree; NOT a voice-subsystem holdout.

The canonical GATE-2 symbol set includes the broad substrings `getVoice` and `setVoice`
(intended to catch `CuiPreferences::getVoiceChatEnabled`-style accessors). These substrings
also match four SOE **community-chat** (non-Vivox) methods in the KEEP-listed
`src/external/3rd/library/soePlatform/ChatAPI2/` tree (D-10 explicitly preserves ChatAPI2):

- `ChatRoom.h:386` / `ChatRoom.cpp:683,685` — `ChatRoom::getVoiceCount()`
- `ChatRoomCore.h:55,78,79` / `ChatRoomCore.cpp:858` — `getVoiceCount()`, `getVoiceCore()`, `getVoice()`

These are SOE ChatAPI room-membership accessors, **not** the Vivox voice subsystem.
Verification confirming benign:
- `rg -i "vivox|Vivox"` over `ChatAPI2/` == 0 (the tree carries ZERO vivox literals).
- The 4 files are untouched by Phase 14 (`git status` clean for ChatAPI2/).
- GATE-2 over `src` **excluding** `ChatAPI2/` == 0 (the Vivox voice subsystem itself is fully grep-clean).
- The voice-subsystem-specific tokens (`CuiVoiceChat`, `SwgCuiVoice`, `WS_VoiceFlyBar`,
  `VOICE_INVITE`, `VOICE_KICK`, `CommandParserVoice`, etc.) are ALL zero across all of `src`.

**Disposition:** Out-of-scope per SCOPE BOUNDARY (pre-existing, in a preserved tree, not caused
by this plan). DECRUFT-05 criterion #2 targets the Vivox voice subsystem; the SOE community
ChatAPI is not in scope. No action — documented so the verifier reads the literal
GATE-2-over-all-of-src as PASS-with-4-known-ChatAPI2-collisions.
