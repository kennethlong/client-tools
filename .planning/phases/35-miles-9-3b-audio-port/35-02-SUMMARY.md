---
phase: 35-miles-9-3b-audio-port
plan: 02
subsystem: audio
tags: [miles, mss, audio, x64, room_type, reverb, codec-probe, ail]

# Dependency graph
requires:
  - phase: 35-miles-9-3b-audio-port
    provides: "35-01 vendored the Miles 9.3b SDK + repointed clientAudio.vcxproj includes; its Win32 compile-only smoke surfaced the 27 C2660 room_type call sites this plan ports"
  - phase: 33-x64-build-platform-d3d9-renderers
    provides: "the three Phase-33 x64 Miles disables (macro-stub block, install() x64 early-return, SoundObject3d listener gate) that this plan removes"
provides:
  - "room_type API ported to the 9.3b bus_index signature: setter (dig, 0, room_type) MIDDLE arg at 26 sites + getter (dig, 0) trailing arg at 1 site"
  - "codec-presence probe ported off the 9.3b-absent .m3d filenames to the platform-correct 9.3b .dll/.asi/.flt set, tiered required-core vs optional-effect with by-name WARNING"
  - "all three Phase-33 x64 Miles disables removed -> x64 runs the same real AIL_startup path as Win32; only surviving _M_X64 is the intentional codec-probe split"
  - "install-time REPORT_LOG proving room_type bus_index=0 (master bus) for boot-log census of the reverb edit (A1)"
affects: [35-03, 35-04, miles-audio-port, x64-link, x64-audio-boot]

# Tech tracking
tech-stack:
  added: []
  patterns: [miles-9.3b-room_type-bus_index-port, tiered-codec-probe-required-vs-optional, semantic-comment-gate-over-count]

key-files:
  created:
    - .planning/phases/35-miles-9-3b-audio-port/35-02-SUMMARY.md
  modified:
    - src/engine/client/library/clientAudio/src/win32/Audio.cpp
    - src/engine/client/library/clientAudio/src/win32/SoundObject3d.cpp

key-decisions:
  - "bus_index=0 (master mixer bus, RESEARCH A1) for every ported room_type call -- middle arg on the setter, trailing on the getter"
  - "codec probe split into REQUIRED-CORE (dll + mp3/ogg decoders, miss => suppress audio) vs OPTIONAL-EFFECT (ds3d/eax filters, miss => warn-by-name only, no mute) per concern 11"
  - "the version[256] zero-init at getMilesVersion() KEPT (defensive, in case AIL_MSS_version no-fills) but its Phase-33 narration comment reworded so the semantic gate holds"
  - "the UINTa file-callback comment KEPT (real x64 ABI fact, BITS-02) but its (Phase 33 X64-01) tag reworded to (x64 ABI) so no 'Phase 33' token survives"
  - "D-04: legacy [ClientAudio] disableMiles cfg short-circuit PRESERVED (retail off-switch, not Phase-33 scaffolding)"

patterns-established:
  - "miles-9.3b-room_type-bus_index-port: 9.3b added S32 bus_index; setter takes it MIDDLE (dig, bus, rt), getter TRAILING (dig, bus); transposed (dig, ENVIRONMENT_X, 0) compiles clean but routes to a garbage bus -- the inverse grep is the only static defense"
  - "tiered-codec-probe-required-vs-optional: an optional effect-filter miss degrades 3D/reverb (D-06) but does NOT suppress audio; only a required-core miss flips s_milesCodecRedistAvailable; WARNING names the specific missing filename"
  - "semantic-comment-gate-over-count: brittle '_M_X64 == N' exact-count gate replaced by semantic content gates ('Phase 33' / 'Miles disabled' == 0 + read each surviving _M_X64)"

requirements-completed: [AUDIO-01, AUDIO-02]

# Metrics
duration: ~4min
completed: 2026-06-18
---

# Phase 35 Plan 02: Miles 7.2e->9.3b Source Port (room_type + codec probe + x64-disable removal) Summary

**Ported the only two TUs that call Miles to the 9.3b API on the shared (non-_M_X64) path: room_type gained its S32 bus_index arg (26 setters MIDDLE + 1 getter trailing, all bus 0), the codec probe stopped probing the 9.3b-absent .m3d filenames for a tiered required/optional 9.3b .dll/.asi/.flt set, and all three Phase-33 x64 Miles disables were deleted so x64 runs the same real AIL_startup path as Win32 -- while the legacy disableMiles cfg off-switch was preserved.**

## Performance

- **Duration:** ~4 min
- **Started:** 2026-06-18T15:04:15Z
- **Completed:** 2026-06-18T15:08:08Z
- **Tasks:** 3
- **Files modified:** 2

## Accomplishments

- **room_type bus_index port (Task 1):** all 26 `AIL_set_room_type` setter sites carry bus_index `0` as the MIDDLE arg `(s_digitalDevice2d, 0, ENVIRONMENT_X)`; the single `AIL_room_type` getter carries trailing `0` `(s_digitalDevice2d, 0)`. No 2-arg AND no transposed `(dig, ENVIRONMENT_X, 0)` form survives. Added a one-shot install-time `REPORT_LOG` ("room_type bus_index=0 (master bus)") so the reverb edit is observable in the boot log, not UAT-ear-only (concern 7).
- **Codec probe defused + tiered (Task 2):** replaced the 7.2e `.m3d` hardcode (which 9.3b never ships -> always false-reported "redist missing" -> silently gated audio off, Pitfall 1) with a platform-aware 9.3b set. Split into REQUIRED-CORE (`mss[64].dll` + mp3/ogg `.asi` decoders -- a miss suppresses audio) and OPTIONAL-EFFECT (`mss[64]ds3d.flt` 3D + `mss[64]eax.flt` reverb -- a miss warns by name only, music/UI/decode still play). The WARNING interpolates the specific missing filename so the boot log states WHICH file suppressed/degraded audio (concern 11). The probe `#if defined(_M_X64)` is comment-marked as the intentional survivor.
- **Phase-33 x64 disables removed (Task 3, D-03):** deleted the ~60-macro `AIL_*` no-op stub block + its Phase-33 banner comment; collapsed the `install()` `#if _M_X64` x64 force-disable / `#else` / real-body / `#endif` into just the shared real body; removed the `SoundObject3d::alter()` `#if !defined(_M_X64)` listener gate so the 3 `AIL_set_listener_3D_*` calls run on both platforms. x64 now reaches `AIL_startup()` on the same path as Win32.
- **Semantic gate satisfied (concern 1):** `Phase 33` == 0 and `Miles disabled` == 0 across both files; the ONLY surviving `_M_X64` in Audio.cpp is the codec-probe split (lines 1308 comment + 1309 `#if`), verified by reading each.
- **D-04 preserved:** the legacy `getKeyBool("ClientAudio", "disableMiles", false)` short-circuit is intact and still precedes the real body.

## Task Commits

Each task was committed atomically:

1. **Task 1: Port room_type bus_index API delta (getter + 26 setters) + install-time log** - `d59682a54` (feat)
2. **Task 2: Port codec probe .m3d -> 9.3b .flt/.asi/.dll + required/optional tiers** - `17c18d99e` (feat)
3. **Task 3: Remove the three Phase-33 x64 disables + banner comment (D-03); keep legacy cfg (D-04)** - `b933a61e2` (feat)

**Plan metadata:** (this commit) `docs(35-02): complete Miles 9.3b source port plan`

## Files Created/Modified

- `src/engine/client/library/clientAudio/src/win32/Audio.cpp` - ported room_type (26 setters + 1 getter), ported+tiered codec probe, deleted macro-stub block + install() x64 early-return + Phase-33 banner/residual comments, added install-time room_type bus log; legacy disableMiles cfg kept
- `src/engine/client/library/clientAudio/src/win32/SoundObject3d.cpp` - removed the `#if !defined(_M_X64)` listener gate; 3 `AIL_set_listener_3D_*` calls now run on both platforms

## Surviving _M_X64 audit (Task 3 read-each, concern 1)

In `Audio.cpp` exactly two `_M_X64` tokens remain, both the intentional codec-probe split:
- **Line 1308** — comment: `// codec-probe platform split -- intentional _M_X64 (NOT Phase-33 scaffolding); keep.`
- **Line 1309** — `#if defined(_M_X64)` selecting the x64 vs Win32 9.3b filename arrays (matching `#else`/`#endif` at 1312/1315 carry no `_M_X64` token).

`SoundObject3d.cpp` `_M_X64` count == 0. The gate is the semantic content (no `Phase 33` / `Miles disabled` text), not a fixed count.

## Verification (grep gates, all PASS)

| Gate | Result |
|------|--------|
| `AIL_set_room_type(s_digitalDevice2d, 0,` count | 26 (want 26) |
| `AIL_room_type(s_digitalDevice2d, 0)` count | 1 (want 1) |
| un-ported 2-arg `AIL_set_room_type(...ENVIRONMENT` | 0 (want 0) |
| transposed `(s_digitalDevice2d, ENVIRONMENT_X, 0)` | 0 (want 0) |
| `room_type bus_index` install log | 1 (want >=1) |
| `Msseax.m3d`/`msssoft.m3d` (any `.m3d`) | 0 (want 0) |
| `mss64ds3d.flt` / `mssds3d.flt` | 1 / 1 (want >=1 each) |
| `mss64eax.flt` / `msseax.flt` | 1 / 1 (want >=1 each) |
| intentional codec-probe marker | 1 (want >=1) |
| Audio.cpp `Phase 33` / `Miles disabled` | 0 / 0 (want 0) |
| Audio.cpp `disableMiles` cfg keep (D-04) | 1 (want >=1) |
| Audio.cpp `AIL_startup` (shared body) | 1 (want >=1) |
| SoundObject3d `_M_X64` / `Phase 33` | 0 / 0 (want 0) |
| SoundObject3d `AIL_set_listener_3D` calls | 3 (want 3) |

Preprocessor balance after edits: `#if` 27 == `#endif` 27 (single `#else` = codec-probe split). No accidental file deletions (`git diff --diff-filter=D HEAD~1 HEAD` empty for Task 3).

## Decisions Made

See `key-decisions` in frontmatter. Headline: bus_index=0 for all room_type calls; tiered required-vs-optional codec probe; two residual Phase-33 comments (UINTa callback ABI fact, version[] zero-init) reworded rather than deleting their still-valid technical content.

## Deviations from Plan

None - plan executed exactly as written. (All three tasks completed; the two residual "Phase 33" comments at the UINTa callback decl and `getMilesVersion()` `version[256]` zero-init were reworded to satisfy the semantic gate -- this is explicitly anticipated by Task 3's "delete any `Phase 33` comment text" instruction, applied as a content-preserving reword since the underlying code is correct on both platforms, not Phase-33 scaffolding.)

## Issues Encountered

- **Comment-grep false positives (handled inline):** my first-draft Task-2 comment quoted the literal `Msseax.m3d / msssoft.m3d` filenames and a `*.m3d` glob, which tripped the "no `.m3d`" acceptance grep. Reworded the comment to "legacy software-codec provider filenames" so zero `.m3d` text survives -- the landmine is genuinely defused, not just the probed array. Same class of issue for the two surviving Phase-33 narration comments (Task 3), reworded to drop the `Phase 33` / `Miles disabled` tokens while keeping the technical fact.

## Commit-sequencing compliance (concern 6 / HIGH)

**Option chosen: (a) — keep 35-02 local, push together with 35-03 once 35-03's x64 link gate is green.** This plan deletes the Phase-33 x64 macro stubs + install() early-return, which makes the `AIL_*` symbols LIVE again on x64 -- but the matching `mss64.lib` add to SwgClient.vcxproj is in 35-03. Between this commit and 35-03's, the x64 tree does NOT link (stubs gone, lib not yet added). Per the plan's `<commit_sequencing>`, **this plan's commits (`d59682a54`, `17c18d99e`, `b933a61e2`) MUST NOT be pushed to `origin/master` ahead of 35-03's clean x64 link.** They remain LOCAL on `master`; push only after 35-03 lands the lib add and the link-grep gate (`unresolved external symbol` == 0) is green. `master` is a live upstream-integration branch and the boot gate must hold across a mid-phase interruption.

## Build/link status (per plan <verification>)

This plan does NOT compile/link standalone -- the link gate runs in plan 35-03 (which adds `mss64.lib` + stages redist64). 35-01 already proved the 9.3b header is v145/C++20-clean on both platforms; Win32 will now compile the ported room_type call sites (the 27 C2660 errors 35-01 surfaced are exactly what Task 1 fixes). No build was run here by design.

## Next Phase Readiness

- **35-03 ready:** the source port is complete on the shared path. 35-03 adds `mss64.lib` to SwgClient.vcxproj (x64 Debug + Release), repoints lib dirs to `miles-9.3b\lib\win`, adds the x64 redist64 PostBuild staging, and runs the x64 link gate (grep `unresolved external symbol` == 0). The Win32 link/build should also be exercised to confirm the room_type call sites compile clean now that the real 9.3b prototypes are in scope.
- **Push discipline:** see Commit-sequencing above -- do NOT push 35-02 ahead of 35-03's green x64 link.
- **No blockers.**

## Self-Check: PASSED

- All 3 task commits verified in git log (`d59682a54`, `17c18d99e`, `b933a61e2`)
- Both modified source files verified present (Audio.cpp, SoundObject3d.cpp)
- SUMMARY.md verified present

---
*Phase: 35-miles-9-3b-audio-port*
*Completed: 2026-06-18*
