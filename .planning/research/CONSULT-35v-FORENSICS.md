# Restoration binary forensics (Codex's task — Codex was sandbox-blocked from D:\SWG Restoration; done by main via dumpbin)

All via dumpbin (VS2022 14.44), 2026-06-18.

## 1. Dynamic-load vs static (Task 2)
- Restoration x64 `SwgClient_r.exe` statically imports `mss64.dll` **by name** (L2 confirmed). It DOES
  import `LoadLibraryA/ExA/ExW` + `GetProcAddress` from KERNEL32 — but that's generic (gl0x renderer
  plugins, bink, etc.); mss64 is in the static import table, not dynamically loaded. **No name-based
  dynamic-load shim is needed; a static name-bound import is the mechanism (same as ours).**

## 2. CRT dependency (kills a RIB-crash candidate)
- BOTH 9.3v and 9.3b `mss64.dll` depend ONLY on **USER32 / KERNEL32 / WINMM**. No `msvcr71`, no CRT.
  → The "9.3v needs msvcr71 co-resident" theory for the L5 crash is FALSE. Miles core has no CRT dep.

## 3. Provider/core build-timestamp consistency (the L5 crash suspect)
Restoration's "9.3v set" is **VERSION-SPLIT**:

| File | Restoration timestamp | Ours (9.3b) |
|---|---|---|
| `mss64.dll` (CORE) | **Mon May 18 2015** | Thu Dec 13 2012 |
| `binkawin64.asi` | Wed Dec 4 2013 | Thu Dec 13 2012 |
| `mss64dolby.flt` | Wed Dec 4 2013 | Thu Dec 13 2012 |
| `mss64ds3d.flt` | Wed Dec 4 2013 | Thu Dec 13 2012 |
| `mss64dsp.flt` | Wed Dec 4 2013 | Thu Dec 13 2012 |
| `mss64eax.flt` | Wed Dec 4 2013 | Thu Dec 13 2012 |
| `mss64mp3.asi` | Wed Dec 4 2013 | Thu Dec 13 2012 |
| `mss64ogg.asi` | Wed Dec 4 2013 | Thu Dec 13 2012 |
| `mss64srs.flt` | Wed Dec 4 2013 | Thu Dec 13 2012 |

**Restoration ships a 2015 core with 2013 providers and runs fine.** Therefore:
- A core↔provider build-date skew is NOT inherently fatal (Restoration is the existence proof).
- The 2013 providers Restoration uses are what we'd stage for a 9.3v swap.
- → The L5 `RIB_register_interface` c0000005 on OUR swap was NOT an inherent core/provider mismatch.
  Remaining material difference between our crashing boot and Restoration's working boot of the SAME
  DLLs: **our exe is x64 Debug, Restoration's is x64 Release.** (Next-most-likely: a stray 9.3b file
  left in the redist dir during the swap, or our redist-probe code path. There is an `mss64.dll` copy
  living INSIDE `stage-x64\miles\` as well as next to the exe — verify the swap replaced/removed it.)

## Implication for the lead
Combined with Cursor's verdict (no 9.3b↔9.3v streaming-behavior delta in docs/exports/strings), the
9.3v hypothesis looks like a RED HERRING. The 9.3b starvation is an engine/Miles integration issue,
not a point-release servicing change. A 9.3v swap is cheap to TEST (name-bound, no relink) but is not
evidence-supported as the fix.
