---
phase: 35-miles-9-3b-audio-port
plan: 01
subsystem: audio
tags: [miles, mss, audio, x64, vendoring, vcxproj, third-party]

# Dependency graph
requires:
  - phase: 33-x64-build-platform-d3d9-renderers
    provides: "linking x64 client (clientAudio Miles disabled under #if _M_X64; AIL_* stubbed)"
provides:
  - "src/external/3rd/library/miles-9.3b/ vendored SDK tree (header + rrcore.h + mss32.lib + mss64.lib + redist/ + redist64/)"
  - "clientAudio.vcxproj include path repointed miles/ -> miles-9.3b (all 5 configs)"
  - "build-time proof: 9.3b header is v145/C++20-clean on Win32 + x64 (A4 de-risked)"
  - "build-time provenance: SHA256 + byte size + dumpbin machine-type for both libs"
affects: [35-02, 35-03, miles-audio-port, x64-link]

# Tech tracking
tech-stack:
  added: [miles-9.3b-sdk]
  patterns: [vendor-mirror-7.2e-layout, force-add-lib-past-gitignore, scoped-header-compat-smoke]

key-files:
  created:
    - src/external/3rd/library/miles-9.3b/include/mss.h
    - src/external/3rd/library/miles-9.3b/include/rrcore.h
    - src/external/3rd/library/miles-9.3b/lib/win/mss32.lib
    - src/external/3rd/library/miles-9.3b/lib/win/mss64.lib
    - src/external/3rd/library/miles-9.3b/redist/
    - src/external/3rd/library/miles-9.3b/redist64/
  modified:
    - src/engine/client/library/clientAudio/build/win32/clientAudio.vcxproj

key-decisions:
  - "Vendor mirrors the miles-7.2e convention (include/ + lib/win/ + redist/ + redist64/), NOT the SDK native win/sdk/ tree"
  - "mssvoice.asi INCLUDED in Win32 redist/ for 7.2e parity (present in SDK win/vox/redist)"
  - "x64 redist64/ intentionally has no voice ASI (vox tree is Win32-only) — not a gap"
  - "x64 compile-smoke scoped header-only: Phase-33 macro stubs still shadow 9.3b prototypes, so x64 call-site port is NOT validated here (gated in 35-03)"

patterns-established:
  - "Vendor-mirror-7.2e-layout: third-party Miles SDK vendored as include/+lib/win/+redist{,64}/ mirroring miles-7.2e"
  - "Force-add lib past gitignore: git add -f for *.lib so vendored import libs survive a fresh checkout"
  - "Scoped header-compat smoke: separate in-header errors (true incompat) from call-site arg-count errors (plan-02 trigger) and TreatWarningAsError-upgraded W4 noise"

requirements-completed: [AUDIO-01]

# Metrics
duration: ~12min
completed: 2026-06-18
---

# Phase 35 Plan 01: Miles 9.3b SDK Vendoring + clientAudio Include Repoint Summary

**Vendored the Miles 9.3b SDK (header + both import libs + full 7.2e-parity provider sets for Win32 & x64) into src/external/3rd/library/miles-9.3b/ and repointed clientAudio.vcxproj off the unversioned miles/ tree onto it across all 5 configs — proving the 2012-era header compiles clean under v145/C++20 on both platforms before any API edit.**

## Performance

- **Duration:** ~12 min
- **Started:** 2026-06-18T~14:53Z
- **Completed:** 2026-06-18
- **Tasks:** 2
- **Files modified:** 1 (clientAudio.vcxproj) + 23 vendored files created

## Accomplishments

- Vendored the complete Miles 9.3b SDK tree mirroring the miles-7.2e convention: `include/mss.h` (341,232 B, MILESVERSION ×4) + `include/rrcore.h` (Pitfall-4 dependency), `lib/win/mss32.lib` + `mss64.lib` (force-added past the `*.lib` gitignore), `redist/` (Win32 D-05 set), `redist64/` (x64 D-05 set). No `.m3d` carried over (9.3b ships none).
- Recorded build-time provenance for both libs (SHA256 + byte size + dumpbin machine-type) — the architecture-correct proof, before 35-03's link gate proves they resolve.
- Repointed all 5 `<AdditionalIncludeDirectories>` in clientAudio.vcxproj from `library\miles\include` to `library\miles-9.3b\include` (grep: 5 new / 0 old).
- Compile-only smoke on both platforms proved the 9.3b header is v145/C++20-clean: zero errors AND zero warnings originating inside `mss.h`/`rrcore.h` on both Win32 and x64. A4 (RESEARCH header-compat risk) de-risked.

## Lib Provenance (Task 1, build-time proof)

| Lib | Size | Expected | dumpbin machine | SHA256 |
|-----|------|----------|-----------------|--------|
| `mss32.lib` | 100,882 B | 100,882 B ✓ | `14C machine (x86)` ✓ | `fe5b4b1a0ccb43296385b8fc31ce0ebd169dff2bd9e43863cf8d7ba1467c130d` |
| `mss64.lib` | 94,246 B | 94,246 B ✓ | `8664 machine (x64)` ✓ | `5ec9dea06f0ea09c878199b3280650fabd72c60d11126b63e06fa355b93566c2` |

Both architecture-correct (concern-2 build-time provenance satisfied).

## Provider Sets Vendored (D-05)

- **Win32 `redist/`** (10 files): `mss32.dll`, `mssmp3.asi`, `mssogg.asi`, `mssvoice.asi`, `mssdsp.flt`, `msseax.flt`, `mssds3d.flt`, `mssdolby.flt`, `msssrs.flt`, `binkawin.asi` — gathered from FOUR SDK trees (`win/sdk/redist`, `win/mp3/redist`, `win/ogg/redist`, `win/vox/redist`).
- **x64 `redist64/`** (9 files): `mss64.dll`, `mss64mp3.asi`, `mss64ogg.asi`, `mss64dsp.flt`, `mss64eax.flt`, `mss64ds3d.flt`, `mss64dolby.flt`, `mss64srs.flt`, `binkawin64.asi` — gathered from THREE SDK trees (`win/sdk/redist64`, `win/mp3/redist64`, `win/ogg/redist64`).

**mssvoice.asi decision:** INCLUDED for 7.2e parity (present at `D:/Code/milesss-v9.3b/win/vox/redist/mssvoice.asi`) — NOT an intentional exclusion. There is intentionally no x64 voice ASI (the `vox` tree is Win32-only).

## Task Commits

1. **Task 1: Vendor the Miles 9.3b SDK tree (mirror miles-7.2e), force-add both libs, record provenance** — `36a9b975a` (feat) — 23 files, +10,513 insertions
2. **Task 2: Repoint clientAudio.vcxproj include path miles/ -> miles-9.3b (5 configs) + scoped compile-only smoke both platforms** — `4b1fc0960` (build) — 1 file, 5 insertions / 5 deletions

## Files Created/Modified

- `src/external/3rd/library/miles-9.3b/include/{mss.h,rrcore.h}` - 9.3b API header + its `#include "rrCore.h"` dependency
- `src/external/3rd/library/miles-9.3b/lib/win/{mss32.lib,mss64.lib}` - Win32 + x64 import libs (force-added)
- `src/external/3rd/library/miles-9.3b/{redist,redist64}/` - Win32 + x64 provider sets (D-05)
- `src/engine/client/library/clientAudio/build/win32/clientAudio.vcxproj` - include path repointed to miles-9.3b across 5 configs

## Decisions Made

- **Vendor mirrors the 7.2e convention** (`include/`+`lib/win/`+`redist{,64}/`), not the SDK's native `win/sdk/...` layout — Claude's-discretion D-decision per PATTERNS §1.
- **mssvoice.asi included** in Win32 `redist/` for parity (the 7.2e tree staged it); confirmed present in the SDK clone.
- **x64 smoke scoped header-only** — see Deviations / Issues below.

## Deviations from Plan

None - plan executed exactly as written. (Both tasks completed; the Win32 C2660 call-site errors are the explicitly-EXPECTED plan-02 trigger documented in the plan action, not a deviation.)

## Issues Encountered

- **Win32 compile-smoke surfaced the expected plan-02 trigger (NOT a gate failure):** Win32 has no Phase-33 macro stubs, so the real 9.3b `AIL_set_room_type`/`AIL_room_type` prototypes are in scope and the 27 unported 2-arg call sites at `Audio.cpp:3919-3944` (setter ×26) + `:3957` (getter ×1) fail with `error C2660: function does not take N arguments`. **Critically: zero of these errors originate inside `mss.h`/`rrcore.h`** — all are in `Audio.cpp`. This is exactly the room_type signature change that plan 35-02 ports (insert middle `0` bus_index arg). Captured here, not chased as a header incompat.
- **x64 compile-smoke is HEADER-CLEAN ONLY — call-site port NOT yet validated on x64.** The Phase-33 macro stubs (`#define AIL_room_type`/`AIL_set_room_type` at `Audio.cpp:268/287`, deleted by 35-02) still SHADOW the 9.3b prototypes on x64, so x64 compiled with 0 errors even with unported 2-arg call sites. **DO NOT read 01's x64 pass as proof the call sites are ported on x64** — the real x64 call-site compile is gated in 35-03 (depends_on [35-01, 35-02]).
- **TreatWarningAsError trap N/A:** Win32 Debug clientAudio has `TreatWarningAsError=true`, but no warning originates inside `mss.h`/`rrcore.h` on either platform, so no W4-upgraded-to-error noise needed the `#pragma warning(disable)` wrapper. The 164 x64 warnings are pre-existing `Audio.cpp` warnings (e.g. C4700 uninitialized-local at `:3420`/`:3845`), unrelated to the header.
- **Build logs are UTF-16LE** (BOM `fffe`) — analysis used `tr -d '\000'` before grep. Both logs (`build-35-01-clientAudio-win32.log`, `build-35-01-clientAudio-x64.log`) are gitignored, not committed.

## Threat Surface

No new surface. Per the plan threat model (T-35-01, disposition `accept`): vendored proprietary middleware bytes copied from the local SDK clone, same trust posture as the existing tracked miles-7.2e binaries. Provenance now positively recorded (SHA256 + dumpbin machine-type).

## Deferred TODO (recorded, NOT executed this phase)

8 editor/tool vcxprojs (AnimationEditor, ClientEffectEditor, ShipComponentEditor, SoundEditor, TerrainEditor, Viewer, Turf, SwgGodClient) still reference the old `miles\lib\win` + `mss32.lib`. They are pre-broken on Qt and out of the validation bar. A future `git rm` of the dormant `miles-7.2e` + unversioned `miles/` trees would break all 8 with LNK1181 and no gate — repoint them atomically in the same commit that deletes the old trees (optional cleanup follow-up, not a Phase-35 gate).

## Next Phase Readiness

- **35-02 ready:** the 9.3b header + both libs are vendored and committed, the include path is repointed, and the header is proven v145/C++20-clean. 35-02 can now delete the Phase-33 macro stubs and port the `Audio.cpp` room_type call sites (Win32 C2660 errors are the exact trigger list) + `SoundObject3d.cpp` listener gate.
- **No blockers.** The x64 call-site compile + the full x64 link (with `mss64.lib` added to SwgClient.vcxproj + redist64 staging) are 35-02/35-03 scope.

## Self-Check: PASSED

- All 6 key files verified present (2 headers, 2 libs, vcxproj, SUMMARY)
- Both task commits verified in git log (`36a9b975a`, `4b1fc0960`)
- Both libs git-tracked (`git ls-files` lists 2 in `lib/win/`)

---
*Phase: 35-miles-9-3b-audio-port*
*Completed: 2026-06-18*
