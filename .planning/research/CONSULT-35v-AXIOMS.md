# LOCKED ground-truth — Miles 9.3v login-music lead (2026-06-18)

These are MEASURED facts (dumpbin on this machine, paths below). Treat as axioms. **Do not re-derive
them and do NOT reason from the falsified framing in the BAN section.**

## Context (one paragraph)
Our x64 SWG client (`D:\Code\swg-client-v2`) plays in-game audio fine but the **login / character-select
screen title music is silent** (`music/mus_title_lp.mp3`, the one STREAMED sound via `AIL_open_stream`).
It "kicks in" only once heavy zone-load I/O starts. We vendor Miles **9.3b** (`mss64.dll`, 504,320 B).
Restoration (`D:\SWG Restoration`) ships a working x64 SWG client built from the same codebase that
runs Miles **9.3v** (`mss64.dll`, 483,328 B). Open question: does 9.3v fix idle streamed music, and
what's the cleanest way to adopt it.

## LOCKED axioms (measured)
- **L1.** BOTH DLLs export by **NAME**, not ordinal-only. 9.3v = 395 named exports (timestamp 2015-05-18),
  9.3b = 393 named exports (timestamp 2012-12-13). Ordinal 1 = `AIL_3D_distance_factor` in both.
  Evidence: `CONSULT-35v-exports-9.3v.txt`, `CONSULT-35v-exports-9.3b.txt`.
- **L2.** BOTH our x64 `SwgClient_d.exe` and Restoration's x64 `SwgClient_r.exe` **statically import
  `mss64.dll` BY NAME** (dumpbin /imports shows `AIL_*` function names in the Import Name Table, not
  bare ordinals).
- **L3.** Our exe imports **61** `mss64` symbols by name. **ALL 61 exist in 9.3v's exports** (zero
  missing). Export-set delta 9.3v vs 9.3b: 9.3v ADDS `AIL_event_system_break_on_mem_error`,
  `AIL_event_system_dump_handles`, `AIL_set_event_sample_functions`; 9.3v REMOVES `AIL_background`.
  Our exe imports **none** of those four. Evidence: `CONSULT-35v-ourexe-imports.txt`.
  → **A runtime DLL swap to 9.3v resolves every one of our imports by name. No relink, no import lib needed.**
- **L4.** Our init path: `Audio.cpp:1284` calls `AIL_set_redist_directory("miles")` (relative →
  resolves against cwd = `stage-x64\`, so providers load from `stage-x64\miles\`), then `Audio.cpp:1288`
  `AIL_startup()`. Our redist probe checks `mss64mp3.asi`/`mss64ogg.asi` (core) + `.flt` filters (optional).
- **L5.** A PRIOR attempt swapped in Restoration's 9.3v core (483,328) + Restoration's `x64\miles`
  providers and booted our (unchanged) `SwgClient_d.exe` → **c0000005 in `RIB_register_interface`
  during `AIL_startup`**. Restoration's OWN exe runs that same 9.3v DLL fine. NOTE: our exe is x64
  **Debug**; Restoration ships x64 **Release** only. The provider file sizes differ between the two
  versions for 4 of 8 files (mss64dsp/ds3d/eax/ogg); mp3/srs/dolby identical sizes; both sets include
  `binkawin64.asi`.

## BANNED framing (FALSIFIED — do not use)
> "Miles mss64.dll exports by ORDINAL only (0 named exports); our exe is link-bound to a 9.3b
> name→ordinal map, so against the 9.3v DLL ordinals resolve to the wrong functions → crash; therefore
> we must source/build a 9.3v import lib (mss64.lib) and relink."

This is **false** per L1/L2/L3. Exports and imports are both by NAME. Do not propose relinking or
sourcing an import lib as the fix, and do not attribute the L5 crash to an ordinal/ABI remap.

## Key paths
- Ours (9.3b): `D:\Code\swg-client-v2\src\external\3rd\library\miles-9.3b\redist64\mss64.dll`; staged in `stage-x64\`.
- 9.3b SDK (lib+headers): `D:\Code\milesss-v9.3b`.
- Restoration (9.3v): `D:\SWG Restoration\x64\mss64.dll`, providers in `D:\SWG Restoration\x64\miles\`.
- Decompiled Miles 9.3b help: `D:\Code\swg-client-v2\.planning\research\miles-chm\`.
- Engine audio: `src\engine\client\library\clientAudio\src\win32\Audio.cpp`.
- 35-04 root-cause doc: `.planning\phases\35-miles-9-3b-audio-port\35-04-LOGIN-MUSIC-FOLLOWUP.md`
  (proven: with auto-service OFF + main-thread `AIL_service_stream` loop, 542 service calls drove our
  sync read callback ZERO times; stream got only the single 8 KB read `AIL_open_stream` does at open → starves).
