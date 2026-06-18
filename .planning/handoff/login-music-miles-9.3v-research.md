# Handoff: login-screen music — Miles 9.3v (Restoration) research lead

**Date:** 2026-06-18 · **Status:** Phase 35 COMPLETE/committed; this is the open follow-up.
**Read first:** `.planning/phases/35-miles-9-3b-audio-port/35-04-LOGIN-MUSIC-FOLLOWUP.md` (full root cause + every fix already tried/reverted).

## Where Phase 35 landed
- x64 Miles 9.3b audio port COMPLETE and committed (HEAD `83cfd9889`). In-game audio fully works
  (music, 2D UI, 3D positional, reverb); boot crash fixed; tree is clean (all login-music experiments reverted).
- ONE open regression: **login/character-select SCREEN music is silent** (pre-game, narrow). In-game
  music is unaffected (it uses the in-memory buffered path; the title theme is the one STREAMED sound).

## Root cause (proven, do not re-litigate)
Under our vendored Miles **9.3b** (`src/external/3rd/library/miles-9.3b/redist64/mss64.dll`, 504,320 B),
a streamed `AIL_open_stream` for the TRE-backed title music (`music/mus_title_lp.mp3`) opens + starts +
is full-volume but **never produces audio** at the idle login screen (it "kicks in" only during heavy
zone-load I/O). Every servicing/descriptor/in-memory fix was tried and reverted (see followup doc).

## THE NEW LEAD (this session's discovery) — Restoration runs a DIFFERENT Miles point release
Restoration (`D:\SWG Restoration`) ships a **working x64 SWG client with streamed music**, built from
the same SWG codebase. Verified by dumpbin/strings/FileVersionInfo (2026-06-18):

- Restoration's x64 client references the **same Miles streaming API we call** — `AIL_open_stream`,
  `AIL_set_file_callbacks`, `AIL_start_stream`, `AIL_serve`, `AIL_set_named_sample_file`,
  `AIL_register_stream_callback`, `AIL_set_stream_loop_*`, `AIL_stream_status`, `AIL_close_stream`.
  It does **NOT** call `AIL_service_stream`, `AIL_auto_service_stream`, `AIL_set_async_callbacks`, or
  `MilesAsyncStartup` — i.e. identical surface to ours (same Audio.cpp lineage).
- **The DLL build differs:** Restoration `mss64.dll` = **9.3v**, 483,328 B (`D:\SWG Restoration\x64\mss64.dll`).
  Ours = **9.3b**, 504,320 B. Restoration's matched x64 providers live in `D:\SWG Restoration\x64\miles\`
  (`mss64mp3.asi`, `mss64ogg.asi`, `mss64ds3d.flt`, `mss64eax.flt`, `mss64dolby.flt`, `mss64dsp.flt`, `mss64srs.flt`).
- Same layout as ours (core next to exe + providers in `miles/`).

**Conclusion:** the silence is almost certainly a **9.3b vs 9.3v point-release streaming behavior
difference**, not our code. (This also explains the original boot crash: the stale 483,328 `mss64.dll`
that was next to our exe was Restoration's **9.3v** core mixed with our **9.3b** providers → RIB ABI mismatch.)

## SESSION UPDATE: direct 9.3v runtime-swap TESTED → crashed (link-ABI blocker)
Staged Restoration's MATCHED 9.3v set (core 483,328 + its x64/miles providers) into `stage-x64` and
booted our (unchanged, 9.3b-linked) `SwgClient_d.exe`. Result: **c0000005 at `RIB_register_interface`
during `AIL_startup`** — the same startup crash class. Restoration's OWN exe runs this exact 9.3v DLL
fine, so it's an **our-exe ↔ 9.3v-DLL ABI mismatch**, not a bad DLL.

Cause (confirmed): **Miles `mss64.dll` exports by ORDINAL only** (dumpbin: 0 named exports on both
9.3v and 9.3b). Our exe is link-bound to **9.3b `mss64.lib`** (name→9.3b-ordinal map), so against the
9.3v DLL the ordinals resolve to the wrong functions → crash. A pure runtime DLL swap therefore CANNOT
validate 9.3v. (Restoration likely either built against a 9.3v import lib OR loads Miles dynamically by
name — their packed exe shows `AIL_*` name strings and no readable static import table.)

**So the 9.3v hypothesis is PLAUSIBLE BUT UNVALIDATED.** To actually test it we must relink against a
9.3v import lib. The 9.3b set is restored in `stage-x64/` (backup in `stage-x64/_miles93b_bak/`).

### Sharpened next steps
1. **Source a Miles 9.3v SDK** (the `mss64.lib` + headers Restoration built against), OR generate an
   import lib that maps our required `AIL_*` names to the 9.3v DLL's ordinals (needs the 9.3v
   ordinal map — not in the DLL since exports are ordinal-only; may need the 9.3v SDK or RAD docs).
2. Relink `SwgClient` (+ `clientAudio`) against the 9.3v lib, stage the matched 9.3v set, boot → test login music.
3. Alternatively investigate whether Restoration **dynamically loads** Miles by name (GetProcAddress)
   — if so, a name-based dynamic-load shim would be version-agnostic (bigger change, but decouples us
   from the ordinal lib entirely).

## (superseded) FASTEST next step — direct runtime swap (now known to crash, see SESSION UPDATE above)
Stage Restoration's COMPLETE, MATCHED 9.3v set into our x64 stage and boot:
1. Copy `D:\SWG Restoration\x64\mss64.dll` → `stage-x64\mss64.dll` (next to exe).
2. Copy `D:\SWG Restoration\x64\miles\mss64*.asi` + `mss64*.flt` → `stage-x64\miles\` (overwrite our 9.3b providers — MUST be a matched 9.3v set, do NOT mix with 9.3b).
3. Boot `stage-x64\SwgClient_d.exe` → check login-screen music.
- If music plays → fix = re-vendor the 9.3v set into `src/external/3rd/library/miles-9.3b/` (or a new
  `miles-9.3v/`) and repoint the PostBuild staging. (Keep `mss64.lib`/headers compatible — confirm the
  9.3v core is link-compatible with our `mss64.lib`, or get Restoration's import lib.)
- If still silent → it's not the DLL build; reopen the followup-doc directions (a)/(b)/(c).

## Crew research questions (fan out non-overlapping; hand neutral evidence, not the hypothesis)
1. **Miles 9.3v vs 9.3b stream-servicing delta:** what changed between these MSS 9.3 point releases in
   how `AIL_open_stream`/`AIL_serve`/the background stream thread feed a stream at an idle (low-I/O)
   app state? (RAD changelogs, the decompiled `miles-chm` help at `.planning/research/miles-chm/`,
   binary diff of the two `mss64.dll` builds.)
2. **Link/ABI compatibility:** is Restoration's 9.3v `mss64.dll` callable through our existing
   `mss64.lib` (same export ordinals/names), or do we need a 9.3v import lib? Does 9.3v keep the same
   `AIL_set_file_callbacks` / stream RIB ABI?
3. **Provenance/licensing:** is shipping Restoration's 9.3v Miles redist acceptable for this fork, vs
   sourcing 9.3v from elsewhere? (We already vendor 9.3b from `D:\Code\milesss-v9.3b`.)
4. **Why our 9.3b starves at idle:** is it the documented "auto-streaming secondary thread" not
   running pre-mainloop in 9.3b specifically, fixed in 9.3v?

## Tooling already built (reuse)
- `tools/setup/audio-stream-probe.ps1` + `.cdb` — cdb stream probe (OPEN/START/SERVICE/READ). Run it,
  play to login, quit; reads `stage-x64\stream-probe.log`. (Note: it's a SEPARATE command from
  launching the client — the log only updates when the probe is run.)
- `.planning/research/CONSULT-35-*` — prior crew consults + evidence.
- `.planning/research/miles-chm/` — decompiled Miles 9.3b help (streaming docs).
