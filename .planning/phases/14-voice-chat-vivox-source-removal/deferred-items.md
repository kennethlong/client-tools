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
