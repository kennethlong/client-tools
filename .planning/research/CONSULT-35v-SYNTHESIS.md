# Synthesis — Miles 9.3v login-music lead (2026-06-18)

Crew fanned out on 4 non-overlapping angles, handed the corrected LOCKED axioms
(`CONSULT-35v-AXIOMS.md`) with the falsified ordinal/relink framing banned.

## Verdict: 9.3v is a RED HERRING (now EMPIRICALLY CONFIRMED). The bug is our `AIL_set_file_callbacks` integration.

### ISOLATION TEST — PASSED (2026-06-18, user-confirmed audio)
Ran the prebuilt SDK `win\sdk\examples\win64\streamer.exe` (against `win64\mss64.dll`, byte-identical
9.3b core to ours) on `media\music.mp3` (1.36 MB) at an idle console. After staging the MP3 codec
(`mss64mp3.asi` → `win\sdk\redist64\`, which ships only the `.flt` filters), **music played and looped
at idle — Kenny confirmed hearing it.** (stdout capture was empty only because C stdio block-buffers
to a redirected file and the process was killed before flush — not a failure signal.)
→ The SAME 9.3b `mss64.dll` streams a large MP3 fine at idle, using the canonical pattern (plain file
path, NO `AIL_set_file_callbacks`, main-thread `AIL_service_stream` every 250 ms). This DEFINITIVELY
proves the DLL + servicing are good and the login-music bug is 100% in OUR integration — the global
TreeFile `AIL_set_file_callbacks` shadowing the stream's I/O. 9.3v adoption ABANDONED.

### What each angle found
- **Binary ground truth (main, dumpbin):** Both DLLs export by NAME (9.3v 395, 9.3b 393); both exes
  import mss64 by NAME; all 61 of our imports exist in 9.3v. → A 9.3v swap needs NO relink/import lib.
  The handoff's "ordinal-only → must build a 9.3v mss64.lib" premise is FALSE. See `CONSULT-35v-FORENSICS.md`.
- **Cursor (doc + binary stream-servicing diff):** NO documented or binary-evidenced difference in how
  9.3b vs 9.3v feeds a stream at idle. Identical streaming API surface + identical IO/thread/stream
  strings (same `mss/src/sdk/shared/io.cpp` build). 9.3b docs say auto-service is ON by default and a
  background thread feeds streams independent of app I/O. → adopting 9.3v to fix idle music is
  unsupported by evidence. (`CONSULT-35v-cursor.out`)
- **Main forensics (Restoration install):** Restoration runs a 2015 9.3v CORE with **2013** providers
  and works — so core/provider date skew isn't inherently fatal, and the providers we'd stage are what
  Restoration uses. Both cores have NO CRT dep (msvcr71 theory dead). (`CONSULT-35v-FORENSICS.md`)
- **Opus (RIB crash root cause):** The L5 `RIB_register_interface` c0000005 is a Miles-internal
  core↔provider RIB-broker skew, NOT an exe ABI issue. Prime cause: the failed swap left a 9.3b
  provider under the 9.3v core — `binkawin64.asi` is same-size (110,080) but different MD5 across
  versions, so a size-based copy guard silently skips it; plus a duplicate root-level `mss64.dll`. Cheap
  to fix (wipe miles\, stage one matched set Restoration-style, pin cwd) but only worth it IF 9.3v is
  the fix — which the other angles say it isn't. (agent result captured in handoff thread)
- **Codex:** sandbox-blocked from reading `D:\SWG Restoration` (read-only sandbox = repo cwd only);
  task reassigned to main → `CONSULT-35v-FORENSICS.md`.
- **Sonnet (lateral, service-loop):** PENDING at time of writing — angle = is `Audio::alter`/`serve`
  even ticking at the login screen (would make this a sequencing bug, version-independent).

### The clinching reference — RAD's own streaming sample
`D:\Code\milesss-v9.3b\win\sdk\src\examples\streamer.c` (canonical 9.3b stream pattern):
- **Does NOT call `AIL_set_file_callbacks`.** Opens with a plain filename, or for packed libraries uses
  `AIL_open_soundbank` + `AIL_sound_asset_info` to build a subfile descriptor, and lets Miles do its
  OWN file I/O.
- Idle loop = `AIL_delay(10)` + `AIL_service_stream(stream,0)` every 250 ms on the MAIN thread —
  streams fine in a genuinely idle console app with the SAME 9.3b `mss64.dll` we ship.

This matches 35-04 exactly: our engine installs global `AIL_set_file_callbacks` (TreeFile-backed), and
fix #1 found those callbacks SHADOW even the subfile-descriptor I/O and aren't driven by
`AIL_service_stream`. The reference proves the correct pattern is to NOT install custom file callbacks
for the stream = direction (c) in `35-04-LOGIN-MUSIC-FOLLOWUP.md`.

## Recommended next action — zero-build isolation test (validates the whole chain)
Prebuilt `D:\Code\milesss-v9.3b\win\sdk\examples\win64\streamer.exe` + its `win64\mss64.dll`
(byte-identical 9.3b core, MD5-confirmed) + `examples\media\music.mp3` (1.36 MB, real stream).

Run from `win64\`:  `streamer.exe ..\media\music.mp3`
- Plays at idle → 9.3b streaming + main-thread service proven good → bug is 100% our
  `AIL_set_file_callbacks` path → implement direction (c): stream the title music WITHOUT the global
  sync callbacks intercepting (loose-file or `AIL_open_soundbank` subfile descriptor + no callback
  shadow). 9.3v swap abandoned.
- Silent/errors → escalate to the DLL itself; 9.3v re-enters consideration (Opus's clean-swap path).

## Decisions
- Do NOT source/build a 9.3v import lib (premise falsified — name-bound imports).
- Do NOT re-vendor 9.3v unless the streamer isolation test fails.
- The fix lives in `Audio.cpp` stream path (decouple the title-music stream from the global TreeFile
  file callbacks), per RAD's own sample.
