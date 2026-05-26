---
phase: 13-videocapture-library-unlink
plan: 03
subsystem: build
tags: [msbuild, videocapture, audiocapture, picn20, vendored-tree-delete, link-gate, dual-renderer-boot, rasterMajor, decruft, DECRUFT-04]

requires:
  - phase: 13-videocapture-library-unlink
    provides: "13-01 removed inline SwgClient/clientGraphics refs + locked the link-log gate; 13-02 purged .rsp/editor-vcxproj/dead-source refs"
provides:
  - vendored src/external/3rd/library/videocapture/ tree deleted (D-03) — the capture .lib/picn20*.lib providers gone
  - DECRUFT-04 fully closed (all 3 criteria): repo-wide zero-reference grep incl .vcxproj, Debug+Release link gate, dual-renderer boot
  - v2.1 Decruft milestone invariant (dual-renderer boot to character select) verified for Phase 13
affects: [decruft, 14, 15]

tech-stack:
  added: []
  patterns:
    - "Dir-delete sequenced LAST: delete a vendored tree only after every inline include/lib consumer is gone (else orphaned-path LNK1181)"
    - "Phase build gate = grep of /VERBOSE link log (NOT MSBuild exit), Debug blocking-primary + Release confirmation, re-captured AFTER the tree delete"

key-files:
  created:
    - .planning/phases/13-videocapture-library-unlink/13-03-SUMMARY.md
  modified: []
  deleted:
    - "src/external/3rd/library/videocapture/ (entire tree — 49 files across 9 subdirs: AudioCapture, CaptureCommon, Docs, ImageCapture, PICTools, Smart, SoeUtil, VideoCapture, ZlibUtil)"

key-decisions:
  - "D-03 honored: tree deleted LAST (commit 33d820c79), after the pre-delete grep confirmed zero references outside the tree across .rsp/source/include AND .vcxproj."
  - "D-06 honored: Debug (blocking-primary) AND Release (confirmation) link logs re-captured AFTER the delete; gate is the 3-condition grep, not MSBuild exit."
  - "D-05 honored: dual-renderer boot to character select confirmed under rasterMajor=5 (D3D9) and =11 (D3D11) against the SWGSource VM."

patterns-established:
  - "Post-Wave-1-merge cross-cutting gate lives in Wave 2 (Plan 03), not in the parallel Wave-1 plans — they carry only LOCAL acceptance."
---

# Plan 13-03 Summary — Vendored-Tree Delete + DECRUFT-04 Phase Gates

## What was built

The final removal step and the full DECRUFT-04 verification. With every referencing consumer
already gone (Plan 01 inline SwgClient/clientGraphics refs; Plan 02 `.rsp`/editor-`.vcxproj`/dead
source), the vendored capture SDK tree was deleted and all three DECRUFT-04 criteria verified.

## Task 1 — Delete vendored tree + post-Wave-1-merge grep gate (criterion #1, D-04) — PASS

- **Pre-delete preconditions** (proves both Wave-1 plans merged correctly):
  - 4 build-path `.vcxproj` (SwgClient, clientGraphics, clientGame, clientAudio) capture-token count: **0**
  - repo-wide refs OUTSIDE the vendored tree (`.rsp`/`.cpp`/`.h`/`.inl`/`.vcxproj`, excl `.planning/`): **none**
- **Delete:** `git rm -r src/external/3rd/library/videocapture/` → **49 files** removed across the 9 subdirs
  (AudioCapture, CaptureCommon, Docs, ImageCapture, PICTools, Smart, SoeUtil, VideoCapture, ZlibUtil),
  including the vendored `VideoCapture_*.lib` / `picn20*.lib` providers. Commit `33d820c79`.
- **FINAL gate (tree gone, `.vcxproj` INCLUDED):** repo-wide grep
  `videocapture|VideoCapture|videoCapture|AudioCapture` across `*.rsp,*.cpp,*.h,*.inl,*.vcxproj`
  (excl `.planning/`): **0 hits**.
- **Negative controls (preserved in their files):** `binkw32.lib` ×3 and `vivox` ×6 in SwgClient.vcxproj.

## Task 2 — Debug + Release link gate, tree gone (criterion #2, D-06) — PASS

Re-captured the LOCKED link logs (from 13-01-SUMMARY.md) for BOTH configs AFTER the delete, using
the full MSBuild path + `/nodeReuse:false`, with the SwgClient exe deleted first to force the link
and the stale `SwgVideoCapture.obj` cleared (Pitfall 5). Gate = 3 Select-String conditions on each log:

| Config | Build | (a) `Searching ...lib` | (b) `unresolved external symbol` | (c) `LNK1181`/`cannot open` | Verdict |
|--------|-------|-----------------------:|---------------------------------:|----------------------------:|---------|
| **Debug** (blocking-primary) | succeeded, 0 err / 272 warn, 22m29s | 2138 | 0 | 0 | ✅ |
| **Release** (confirmation)   | succeeded, 0 err / 253 warn, 23m25s | 2104 | 0 | 0 | ✅ |

- Fresh exes linked + staged: `stage/SwgClient_d.exe` (Debug, /FORCE+/VERBOSE) and `stage/SwgClient_r.exe`.
- Only benign LNK warnings (LNK4075/4099/4217). One benign Release FileTracker note —
  `...VIDEOCAPTURE\AUDIOCAPTURE\LIB\WIN32\AUDIOCAPTURE.LIB does not exist; source compilation required`
  — confirms the deleted lib is gone; it is a tracker note, not a linker request (LNK1181 == 0).

## Task 3 — Dual-renderer boot gate (criterion #3, D-05) — PASS (human-verified)

Human-run boot smoke against the SWGSource VM (192.168.1.200), debug exe `SwgClient_d.exe`
reading `stage/client_d.cfg`:

| Renderer | rasterMajor | Plugin | Config edited | Result | FATAL |
|----------|------------:|--------|---------------|--------|-------|
| D3D9  | 5  | gl05_d.dll | stage/client_d.cfg | Reached character select → zoned into **Tatooine**; "beautiful, no obvious regression" | none |
| D3D11 | 11 | gl11_d.dll | stage/client_d.cfg | Reached **character select** → zoned into **Tatooine** | none |

No FATAL attributable to the capture removal under either renderer. Both renderers exceeded the
pass bar (reached the game world, not just character select).

## DECRUFT-04 — fully closed

1. **Zero references** — vendored tree deleted; repo-wide grep incl `.vcxproj` == 0. ✅
2. **Clean link** — Debug (blocking-primary) + Release (confirmation): /VERBOSE captured, unresolved == 0, LNK1181 == 0. ✅
3. **Dual-renderer boot** — character select reached under rasterMajor=5 and =11. ✅

## Deviations / Notes

- T1+T2 were executed inline by the orchestrator (not a subagent): the first 13-01 executor died on a
  transient API 500 mid-build, and the long build steps proved 500-prone and infra-finicky (see below),
  so the orchestrator closed out the build-gate work directly. All edits/commits remained per-file and
  scope-disciplined; the pre-existing Direct3d9/docs/.cursor/tools working-tree changes were left untouched.
- **Build-environment gotchas captured for Phases 14-15** (memory-worthy): (1) bare `msbuild` is NOT on
  PATH for spawned sessions — use the full path
  `D:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe`;
  (2) add `/nodeReuse:false` — orphaned reused MSBuild nodes (and a hung `cl`) hold output-file locks and
  silently truncate/fail later builds ("Device or resource busy"); (3) do NOT run the locked build command
  from Git Bash — MSYS path conversion mangles `/t:`/`/p:` switches (MSB1008); use PowerShell.

## Self-Check: PASSED

- [x] Vendored tree deleted (49 files); repo-wide grep incl .vcxproj == 0
- [x] 4 build-path .vcxproj clean; binkw32/vivox preserved (negative control)
- [x] Debug link gate 2138/0/0; Release link gate 2104/0/0 (fresh, post-delete)
- [x] Dual-renderer boot PASS (D3D9 + D3D11), evidence recorded
- [x] DECRUFT-04 all 3 criteria closed
