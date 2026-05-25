# Phase 13: VideoCapture Library Unlink - Research

**Researched:** 2026-05-25
**Domain:** C++ MSBuild dead-code removal (capture middleware unlink) in the Koogie `swg.sln` tree
**Confidence:** HIGH (all claims verified against the live tree at git `02f36d677`)
**Repo HEAD at research time:** `02f36d677` on `koogie-msvc-cpp20-base`

## Summary

VideoCapture (and its bundled AudioCapture / ImageCapture / Smart / SoeUtil / ZlibUtil / PICTools
siblings) is a dormant, debug-only, D3D9-era in-client gameplay recorder. The decision to remove it
(D-01..D-06) is locked; this research de-risks HOW the atomic removal lands clean under the
`/FORCE`-armed link and dual-renderer boot gate.

The single most important finding — which materially changes the plan vs. what CONTEXT.md assumed —
is **the `.rsp` files are entirely vestigial for the link.** `SwgClient.vcxproj` (and every engine
`.vcxproj`) references **no** `.rsp` file. The real link inputs live inline in each project's
per-config `<AdditionalDependencies>` and `<AdditionalLibraryDirectories>`, and the real compile
inputs live inline in `<AdditionalIncludeDirectories>` and `<ClCompile>` items. Editing
`libraries_d.rsp` alone changes nothing about the build. This is the Phase 12 "inline `.vcxproj`
deps, not just `.rsp`" finding, now confirmed concretely for this phase's exact files.

The second material finding is that **the caller chain CONTEXT.md described as "live" is actually
`#if 0`-dead, and the one genuinely-live caller (`CuiIoWin.cpp`) was not in CONTEXT.md's surface
map.** All of `Game::videoCapture*`, the parser dispatch handlers, and `Audio::*AudioCapture*` are
wrapped in `#if 0` and never compile. The ONLY live consumer is `CuiIoWin.cpp:209`
`VideoCapture::SingleUse::run()` (guarded by `#if PRODUCTION == 0`, which is TRUE in Debug). That
call resolves into `SwgVideoCapture.obj` (the wrapper, also `#if PRODUCTION == 0`), which is the
only translation unit that actually pulls the vendored capture-lib symbols into the link. This
dramatically narrows the live-symbol blast radius and confirms the atomic-delete approach.

**Primary recommendation:** Treat this as a closed, atomic removal of three things that must land
together — (1) the one live caller (`CuiIoWin.cpp`), (2) the wrapper (`SwgVideoCapture.*` + its
`clientGraphics.vcxproj` compile/include entries), and (3) the lib link inputs inline in
`SwgClient.vcxproj` (Debug + Optimized + Release `<Link>` blocks). Edit the inline `.vcxproj` link
lists — not the `.rsp` files — to satisfy the build gate; edit the `.rsp` files (and decide on the
editor `.vcxproj` inline refs) to satisfy the grep gate. Verify with a `link /VERBOSE` grep for
`unresolved external symbol == 0` under the existing `/FORCE` (`<ForceFileOutput>Enabled</ForceFileOutput>`).

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

- **D-01:** Full delete the entire closed caller chain together — no no-op stubs. Every consumer is
  removed, so deleting the chain atomically leaves no orphaned symbols.
- **D-02:** Remove the WHOLE capture family — `VideoCapture` AND its audio-capture sibling
  `AudioCapture` (both under `external/3rd/library/videocapture/`). Concretely:
  - `clientGraphics` — `SwgVideoCapture.cpp/.h` (+ public re-include header)
  - `clientGame` — `Game::videoCaptureConfig/Start/Stop`, `VideoCaptureCallback`,
    `SwgAudioCaptureManager` reference (in `Game.cpp`/`Game.h`)
  - `clientAudio` — `Audio::getAudioCaptureConfig/startAudioCapture/stopAudioCapture`, the
    `namespace AudioCapture` surface, and the (dead) `clientAudio/SwgAudioCapture.h` include
  - `swgClientUserInterface` — the `videoCaptureConfig/Start/Stop` console-command consts + dispatch
    handlers
- **D-02a:** Live Miles audio playback is OUT of scope and untouched. The audio-capture methods are a
  separate DirectShow-style `"AudioCapture Filter"` (`AIL_find_filter`) with zero independent callers.
  Gameplay music/SFX/voice go through the unrelated `AIL_*` playback path.
- **D-03:** Delete the vendored `src/external/3rd/library/videocapture/` tree entirely (AudioCapture,
  CaptureCommon, Docs, ImageCapture, PICTools, Smart, SoeUtil, VideoCapture, ZlibUtil).
- **D-04:** Purge every `VideoCapture`/`AudioCapture` reference repo-wide, INCLUDING the out-of-scope
  editor `.rsp` files, to satisfy DECRUFT-04 criterion #1 ("grep finds zero `VideoCapture` references
  across `.rsp`") literally.
- **D-05:** Dual-renderer boot gate after the unlink — boots to character select under both
  `rasterMajor=5` (D3D9) and `=11` (D3D11). Set `rasterMajor` in `client_d.cfg` (the debug exe reads
  `client_d.cfg`, not `client.cfg`).
- **D-06:** Build gate greps link output for 0 `unresolved external symbol` — NOT just MSBuild exit 0.
  `/FORCE` downgrades unresolved externals to warnings and still emits a binary. Debug AND Release
  must both link clean.

### Claude's Discretion

- Plan/commit granularity and the exact edit order within the atomic removal — planner's call,
  provided the removal lands as a coherent unit that never leaves the tree un-buildable mid-step.

### Deferred Ideas (OUT OF SCOPE)

- **Bink video-codec removal** — explicitly OUT of v2.1 scope (active cutscene codec; `binkw32.lib`
  stays linked). Do NOT conflate with VideoCapture — different middleware. (Note: `binkw32.lib` and
  the `bink/` library dir appear adjacent to capture entries in `SwgClient.vcxproj` — do not touch.)
- Stale `stlport453/` path strings in `.rsp` files — inert; opportunistic cleanup only if a removal
  edit already touches the same file.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| DECRUFT-04 | VideoCapture removed — `VideoCapture_debug.lib` unlinked from SwgClient (+ release `.rsp`) + source/include references purged; client builds + boots | Full removal surface enumerated below (live caller, wrapper, inline `.vcxproj` link inputs across 3 configs, vendored SDK tree, `.rsp` + `.vcxproj` reference inventory). Symbol-resolution direction confirmed: only `SwgVideoCapture.obj` (Debug) pulls the vendored libs; only `CuiIoWin.cpp` calls into it live. |
</phase_requirements>

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Recorder lib link inputs | SwgClient EXE (`.vcxproj` `<Link>`) | — | Externals resolve at the final EXE link; engine static libs carry unresolved refs until then. |
| Capture wrapper (`SwgVideoCapture`) | `clientGraphics` engine lib | — | The only TU that instantiates vendored capture symbols; compiled `#if PRODUCTION==0` (Debug only). |
| Live recorder tick | `clientUserInterface` engine lib (`CuiIoWin`) | — | The one live caller of the wrapper (`run()` each frame), `#if PRODUCTION==0`. |
| Dead recorder commands/handlers | `clientGame` + `swgClientUserInterface` | — | All `#if 0` — never compiled; pure source residue to delete. |
| Dead audio-capture filter | `clientAudio` engine lib | — | All `#if 0` — never compiled; unrelated to live Miles `AIL_*` playback. |
| Vendored SDK | `external/3rd/library/videocapture/` | — | Static `.lib` providers; deleted wholesale (D-03). |

## Symbol Resolution: The Atomic-Delete Linchpin (open question #1 — RESOLVED)

**Question CONTEXT.md asked:** Where do `VideoCapture::` / `AudioCapture::` / `Smart::` symbols resolve
— from the lib, or header-only/inline?

**Answer (HIGH confidence, verified):** They resolve **from the vendored `.lib` files**, pulled in by
exactly one translation unit. The dependency direction and live/dead split:

```
                   [ LIVE in Debug build (#if PRODUCTION == 0) ]
CuiIoWin.cpp:209  VideoCapture::SingleUse::run()        (clientUserInterface lib)
   │  #include "clientGraphics/SwgVideoCapture.h"  (CuiIoWin.cpp:19, UNGUARDED)
   ▼
SwgVideoCapture.cpp  (clientGraphics lib, whole file #if PRODUCTION == 0)
   │  instantiates: VideoCapture::VideoCaptureManager_SingleUse, AudioCapture::ICallback,
   │                Smart::SmartPtrT, SoeUtil::String, CaptureCommon::IBuffer, EncoderProps, ...
   ▼
[ vendored static libs, resolved at SwgClient EXE link ]
   VideoCapture_debug.lib, AudioCapture.lib, CaptureCommon_debug.lib, ImageCapture_debug.lib,
   Smart_debug.lib, SoeUtil_debug.lib, ZlibUtil_debug.lib, picn20md.lib (PICTools codec)

                   [ DEAD — #if 0, never compiled in any config ]
SwgCuiCommandParserScene.cpp:266-268 (consts), :536-538 (help, also /* */), :4896-4924 (dispatch)
   → Game::videoCaptureConfig/Start/Stop  (Game.cpp:3015-3064, all #if 0)
   → VideoCaptureCallback                 (Game.cpp:3025-3047, #if 0)
   → AudioCapture::SwgAudioCaptureManager::GetInstance()  (referenced only inside #if 0)
Audio.cpp:5407-5468  getAudioCaptureConfig/startAudioCapture/stopAudioCapture (all #if 0)
Audio.cpp:44         #include "clientAudio/SwgAudioCapture.h"  (#if 0; file no longer exists)
```

**Why this matters for the plan:**
- The live link footprint is ONE object file: `SwgVideoCapture.obj` (Debug). Remove the wrapper from
  the `clientGraphics` compile list AND remove the `run()` call in `CuiIoWin.cpp`, and nothing in the
  link references the vendored capture symbols anymore — so the inline lib tokens can be dropped from
  `SwgClient.vcxproj` cleanly.
- The atomic unit that MUST land together to avoid an unresolved-external break:
  1. `CuiIoWin.cpp` — remove `run()` call (line 209) + the `#include` (line 19).
  2. `clientGraphics.vcxproj` — remove the `SwgVideoCapture.cpp`/`.h` `<ClCompile>`/`<ClInclude>`
     items (lines 320, 436) + the `videocapture` `<AdditionalIncludeDirectories>` entry (3 configs).
  3. Delete `SwgVideoCapture.cpp/.h` (+ public re-include header).
  4. `SwgClient.vcxproj` — remove all capture-family lib tokens from the 3 `<Link>` blocks + the 8
     videocapture `<AdditionalLibraryDirectories>` entries (3 configs).
- The dead (`#if 0`) consumers can be deleted in the same change or separately — deleting them never
  affects the link (they produce no symbols and no references). They are pure source-residue cleanup
  for D-02. **Recommend deleting them in the same atomic change** so the source is coherent and a
  future re-enable of `PRODUCTION==0` blocks can't accidentally reanimate a half-removed feature.

## Drift From CONTEXT.md (verify-against-live-tree results)

CONTEXT.md was directionally correct but contains several inaccuracies the planner MUST NOT propagate.

| CONTEXT.md claim | Live-tree reality | Impact |
|------------------|-------------------|--------|
| "`SwgCuiCommandParserScene.cpp:4896-4924` — **live** dispatch handlers (these still fire)" | All `#if 0` — never compiled. Consts (266-268) and help (536-538) also `#if 0`. | The "live caller chain" framing is wrong. Real live caller is `CuiIoWin.cpp` (not mentioned in CONTEXT). |
| Parser path `swgClientUserInterface/src/shared/parser/...` under `application/` | Actual path is `src/game/client/library/swgClientUserInterface/...` (`library`, not `application`). | Fix path before editing. |
| "`clientAudio/.../SwgAudioCapture.h` (wrapper header)" exists | No such file anywhere. The `#include` at `Audio.cpp:44` is inside `#if 0` and points at a phantom. | Don't try to delete a non-existent file; just remove the dead include + dead methods. |
| Removal surface omits `CuiIoWin.cpp` | `CuiIoWin.cpp:19` (`#include`) + `:209` (`VideoCapture::SingleUse::run()`, `#if PRODUCTION==0`) is the ONLY live consumer. | Add to removal surface — missing it leaves an unresolved external after the wrapper is deleted. |
| Removal surface implies `.rsp` edits drive the build | `SwgClient.vcxproj` references NO `.rsp`. Link inputs are inline `<AdditionalDependencies>` per config; `.rsp` is vestigial. | The build gate is satisfied by `.vcxproj` edits; `.rsp` edits satisfy only the grep gate. |
| `libraries_d.rsp:3` lists `VideoCapture_debug.lib` (implying that's the lib list) | `libraries_d.rsp` lists 6 capture libs (VideoCapture/ImageCapture/Smart/SoeUtil/ZlibUtil + picn20md). The inline `.vcxproj` Debug list is even larger (adds `AudioCapture.lib`, `CaptureCommon_debug.lib`, and stray release variants). | The full lib set is bigger than CONTEXT named; enumerate from the `.vcxproj`, not the `.rsp`. |
| Editor `.rsp` set = AnimationEditor + ClientEffectEditor only | 9 projects carry capture `.rsp` refs: SwgGodClient, Viewer, TextureBuilder, LightningEditor, TerrainEditor, ClientEffectEditor, ParticleEditor, SwooshEditor, AnimationEditor, NpcEditor (+ SwgClient). 14 `.vcxproj` carry capture refs. | D-04's "+ any others a fresh grep enumerates" — full set below. |

## Complete Reference Inventory (open questions #2, #3 — RESOLVED)

### A. Source files referencing the capture family (9 files — all examined)

| File | Lines | Status | Action |
|------|-------|--------|--------|
| `src/engine/client/library/clientUserInterface/src/shared/core/CuiIoWin.cpp` | 19 (`#include`), 209 (`run()`, `#if PRODUCTION==0`) | **LIVE** | Remove both. The linchpin. |
| `src/engine/client/library/clientGraphics/src/shared/SwgVideoCapture.cpp` | whole file, `#if PRODUCTION==0` | LIVE (compiled in Debug) | Delete file. |
| `src/engine/client/library/clientGraphics/src/shared/SwgVideoCapture.h` | whole file | header | Delete file. |
| `src/engine/client/library/clientGraphics/include/public/clientGraphics/SwgVideoCapture.h` | 1 line (`#include "../../src/shared/SwgVideoCapture.h"`) | public re-include | Delete file. |
| `src/engine/client/library/clientGame/src/shared/core/Game.cpp` | 157 (`#include`), 3014-3064 (methods + callback, all `#if 0`) | DEAD | Remove include + `#if 0` blocks. |
| `src/engine/client/library/clientGame/src/shared/core/Game.h` | 257-261 (3 decls, `#if PRODUCTION==0`) | declarations | Remove the `#if PRODUCTION==0` block. |
| `src/engine/client/library/clientAudio/src/win32/Audio.cpp` | 43-45 (`#include`, `#if 0`), 5405-5470 (3 methods, all `#if 0`) | DEAD | Remove include + `#if 0` blocks. |
| `src/engine/client/library/clientAudio/src/win32/Audio.h` | 22-25 (`namespace AudioCapture { class ICallback; }`, UNGUARDED fwd-decl), 321-327 (3 decls, `#if PRODUCTION==0`) | declarations + fwd-decl | Remove both. |
| `src/game/client/library/swgClientUserInterface/src/shared/parser/SwgCuiCommandParserScene.cpp` | 265-269 (consts, `#if 0`), 532-539 (help, `#if 0` + `/* */`), 4895-4926 (dispatch, `#if 0`) | DEAD | Remove `#if 0` blocks. |

> Note `Game.h:255` includes `static void handleCollectionShowServerFirstOptionChanged(bool enabled);`
> immediately before the `#if PRODUCTION==0` block — be precise about the block boundaries (257-261).

### B. `.vcxproj` files referencing the capture family (14 files)

The active build target is **SwgClient**. The editors are separate solution projects (see Build
Graph below). Each `.vcxproj` carries refs in inline link/include blocks, one per config.

| `.vcxproj` | Ref type | Configs | In SwgClient's build path? |
|------------|----------|---------|---------------------------|
| `SwgClient` | `<AdditionalDependencies>` (capture libs) + `<AdditionalLibraryDirectories>` (8 videocapture paths) | Debug, Optimized, Release | YES — this is the target |
| `clientGraphics` | `<ClCompile>`/`<ClInclude>` SwgVideoCapture (320/436) + `videocapture` `<AdditionalIncludeDirectories>` | all 3 | YES (engine dep) |
| `clientGame` | `videocapture` `<AdditionalIncludeDirectories>` (73/111/148) | all 3 | YES (engine dep) |
| `clientAudio` | `videocapture` `<AdditionalIncludeDirectories>` (73/111/148) | all 3 | YES (engine dep) |
| `SwgGodClient` | libs + paths | all 3 | NO (separate app) |
| `Viewer`, `TextureBuilder`, `LightningEditor`, `TerrainEditor`, `ClientEffectEditor`, `ParticleEditor`, `SwooshEditor`, `AnimationEditor`, `NpcEditor` | `<AdditionalDependencies>` (capture libs) + library paths | all 3 each | NO (editor apps, pre-broken on Qt) |

> `swgClientUserInterface.vcxproj` and `clientUserInterface.vcxproj` do **not** appear in the
> `.vcxproj` grep — they reach the wrapper header via `clientGraphics/include/public` on their
> include path, and carry no capture lib/path refs of their own. Editing `CuiIoWin.cpp` source is
> sufficient there.

### C. `.rsp` files referencing the capture family (D-04 grep target)

`libraries_{d,r,o}.rsp` (lib lists) and `libraryPaths.rsp` (search paths) for: **SwgClient**,
SwgGodClient, Viewer, TextureBuilder, LightningEditor, TerrainEditor, ClientEffectEditor,
ParticleEditor, SwooshEditor, AnimationEditor, NpcEditor. Plus `includePaths.rsp` for the 3 engine
libs (clientGraphics, clientGame, clientAudio). **All vestigial** (no `.vcxproj` references them) —
but they must be purged to satisfy criterion #1's literal grep.

> `SwgClient/build/win32/libraryPaths.rsp:10` includes the `AudioCapture/lib/win32` path that the
> editor `libraryPaths.rsp` files omit — purge per-file, don't assume identical line numbers.

### D. Capture-family lib tokens to remove from `SwgClient.vcxproj` `<AdditionalDependencies>`

Remove ALL of these tokens wherever they appear (lists are messy with mixed debug/release names and
duplicates across the 3 configs):
`VideoCapture_debug.lib`, `VideoCapture_release.lib`, `AudioCapture.lib`, `CaptureCommon_debug.lib`,
`CaptureCommon_release.lib`, `ImageCapture_debug.lib`, `ImageCapture_release.lib`, `Smart_debug.lib`,
`Smart_release.lib`, `SoeUtil_debug.lib`, `SoeUtil_release.lib`, `ZlibUtil_debug.lib`,
`ZlibUtil_release.lib`, `picn20md.lib`, `picn20m.lib`.

And remove these 8 `<AdditionalLibraryDirectories>` entries (3 configs):
`videocapture/AudioCapture/lib/win32`, `.../CaptureCommon/lib/win32`, `.../ImageCapture/lib/win32`,
`.../PICTools/Lib/win32`, `.../Smart/lib/win32`, `.../SoeUtil/lib/win32`, `.../VideoCapture/lib/win32`,
`.../ZlibUtil/lib/win32`.

> Do NOT remove `binkw32.lib`, the `bink/lib/win32` path, `zlib.lib`, `dpvs.lib`, or `vivox*` tokens
> — Bink is OUT of scope, `zlib`/`dpvs` are unrelated live deps, Vivox is Phase 14.

## Build Graph & The `/FORCE` Gate (D-06 verified)

- **`/FORCE` is real and armed.** `SwgClient.vcxproj` Debug config has `<ForceFileOutput>Enabled</ForceFileOutput>`
  (line 113) and `<AdditionalOptions>/SAFESEH:NO /VERBOSE</AdditionalOptions>` (line 110). Under
  `/FORCE`, an `LNK2019 unresolved external symbol` becomes a warning and the EXE still emits at
  exit 0. **MSBuild exit 0 is NOT proof of a clean link.** `/VERBOSE` is on, so the link output is
  grep-able.
- **3 configs.** `SwgClient.vcxproj` has `Debug|Win32` (ItemDefinitionGroup @76), `Optimized|Win32`
  (@129), `Release|Win32` (@175). Each has its own `<Link>` block with its own capture-lib list.
  Success criterion #2 says "Debug and Release"; the `Optimized` config also carries capture refs —
  recommend purging all 3 for coherence even if only Debug+Release are gated.
- **Solution configs.** `swg.sln` defines `Debug|Win32`, `Optimized|Win32`, `Release|Win32`. The
  editor projects (AnimationEditor et al.) DO have `.Build.0` entries in the solution (they are
  marked to build) but are pre-broken on Qt/MFC per the Decruft memory — they are not the gate.
- **Engine libs are static.** `clientGraphics`/`clientGame`/`clientAudio` produce `.lib` files; they
  carry unresolved refs into the final EXE link, where the vendored libs satisfy them. This is why
  removing the wrapper compile from `clientGraphics` + the `run()` call from `clientUserInterface`
  must precede/accompany dropping the lib tokens from `SwgClient` — otherwise the EXE link has a
  reference with no provider (masked by `/FORCE`, caught only by the grep gate).

## Buildable Edit Order (open question #4 — RESOLVED)

No circular reference risk: the dependency is strictly one-directional
(EXE link ← `clientGraphics` wrapper ← vendored libs; `clientUserInterface` → wrapper header). A
safe atomic order that never leaves the tree with an unresolved-external (even momentarily under a
mid-step build):

1. **Remove the live caller first:** `CuiIoWin.cpp` — delete `run()` call (209) and `#include` (19).
   After this, nothing live references the wrapper symbols.
2. **Remove the wrapper from compilation:** drop `SwgVideoCapture.cpp`/`.h` from
   `clientGraphics.vcxproj` (`<ClCompile>` 320, `<ClInclude>` 436); remove the `videocapture`
   include path (3 configs). After this, no object file references the vendored capture symbols.
3. **Drop the lib link inputs:** remove the capture-family tokens from `SwgClient.vcxproj`'s 3
   `<Link>` blocks + the 8 library-search paths (3 configs). Now the link has neither references nor
   providers — clean.
4. **Delete source files:** `SwgVideoCapture.cpp/.h` + public re-include header.
5. **Delete the dead consumers** (`#if 0` blocks + dead includes/decls in Game.cpp/Game.h,
   Audio.cpp/Audio.h, SwgCuiCommandParserScene.cpp) — order-independent; produce/reference no symbols.
6. **Delete the vendored SDK tree** (`external/3rd/library/videocapture/`) — D-03.
7. **Purge the `.rsp` residue** + decide on editor `.vcxproj` inline refs (D-04) — grep-gate only.
8. **Boot gate** under `rasterMajor=5` and `=11` (D-05), with the link-output grep (D-06).

Because the `.rsp` files and editor projects are not in SwgClient's build path, steps 6-7 cannot
break the SwgClient link; they exist to satisfy D-03/D-04. The single coherent atomic commit
(Claude's discretion per CONTEXT) is fine — there is no intermediate state where SwgClient fails to
link if steps 1-3 are in the same change.

## AudioCapture Independence (open question #5 — CONFIRMED)

D-02a holds. The `Audio::getAudioCaptureConfig/startAudioCapture/stopAudioCapture` methods
(Audio.cpp:5407-5468) are all `#if 0` and have **zero live callers** — their only references were the
`#if 0` `Game::videoCapture*` chain via `AudioCapture::SwgAudioCaptureManager`. They use the Miles
DirectShow-style filter API (`AIL_find_filter("AudioCapture Filter", ...)`, `AIL_open_filter`,
`AIL_filter_property`), which is distinct from the live music/SFX/voice playback path. Removing them
does not touch any live `AIL_*` playback call. The `namespace AudioCapture { class ICallback; }`
forward-declaration at `Audio.h:22` (unguarded) exists only to type the (dead) `startAudioCapture`
parameter — safe to remove with the methods. Grep-confirmed: no `startAudioCapture`/`stopAudioCapture`/
`getAudioCaptureConfig` callers exist outside the `#if 0` blocks.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Confirming a clean link under `/FORCE` | A custom symbol-table parser | Grep the existing `/VERBOSE` link `.log` for `unresolved external symbol` (count must be 0) | Phase 12 already established this; the linker already emits the exact string. |
| Finding the "real" link inputs | Trust `.rsp` files | Read the per-config `<AdditionalDependencies>`/`<AdditionalLibraryDirectories>` in `SwgClient.vcxproj` | `.rsp` is vestigial in this tree; the `.vcxproj` is authoritative. |
| Detecting all references | A single broad grep | Grep separately by file type (`.cpp/.h`, `.vcxproj`, `.rsp`) — they need different actions | Source = delete code; `.vcxproj` = edit inline link/compile; `.rsp` = purge for grep gate. |

**Key insight:** In this tree the `.rsp` files describe the *intended* link, the `.vcxproj` files
describe the *actual* link, and they have drifted. Always edit and verify against the `.vcxproj`.

## Common Pitfalls

### Pitfall 1: Editing only the `.rsp` and trusting MSBuild exit 0
**What goes wrong:** You remove `VideoCapture_debug.lib` from `libraries_d.rsp`, the build still
links the lib (from the inline `.vcxproj`), exit 0, and you declare success — but nothing changed.
Or you remove the lib from the `.vcxproj` but leave the `run()` call, and `/FORCE` masks the
resulting `LNK2019` as a warning.
**Why it happens:** `.rsp` is vestigial; `/FORCE` hides unresolved externals.
**How to avoid:** Edit the inline `.vcxproj` link lists (all 3 configs), and verify with the
link-output grep for `unresolved external symbol == 0` — not exit code.
**Warning signs:** `link /VERBOSE` output shows `... : warning LNK4088` or `unresolved external` lines.

### Pitfall 2: Missing `CuiIoWin.cpp`, the one live caller
**What goes wrong:** You delete the wrapper and lib tokens but leave `CuiIoWin.cpp:209`; the link now
has an unresolved `VideoCapture::SingleUse::run()` reference (masked by `/FORCE`), and at runtime the
client may fault if that path is ever hit.
**Why it happens:** CONTEXT.md's surface map omitted `CuiIoWin.cpp` and mislabeled the dead parser
chain as live.
**How to avoid:** Treat `CuiIoWin.cpp` as the linchpin caller; remove it in step 1.
**Warning signs:** Post-edit grep for `VideoCapture::` still finds a hit in `clientUserInterface`.

### Pitfall 3: Confusing `#if 0` (always dead) with `#if PRODUCTION == 0` (live in Debug)
**What goes wrong:** Assuming everything guarded is dead, you skip the genuinely-compiled wrapper +
`CuiIoWin` call; or assuming everything is live, you over-scope.
**Why it happens:** Two different guards coexist in the same files. `PRODUCTION==0` is TRUE in Debug
(`DEBUG_LEVEL=2` → `Production.h` sets `PRODUCTION 0`); `#if 0` is never compiled.
**How to avoid:** `SwgVideoCapture.cpp` + `CuiIoWin.cpp:209` (both `#if PRODUCTION==0`) are LIVE in
Debug; everything in `#if 0` is dead. Verified `Production.h` mapping.
**Warning signs:** Underestimating the link footprint (thinking nothing compiles) — but `clientGraphics`
emits a `SwgVideoCapture.obj` in Debug (stale `.obj` exists at `src/compile/win32/clientGraphics/Debug/`).

### Pitfall 4: Touching Bink, zlib, dpvs, or Vivox by association
**What goes wrong:** The `SwgClient.vcxproj` link lists interleave `binkw32.lib`, `zlib.lib`,
`dpvs.lib`, and `vivox*` tokens near the capture tokens; an over-broad delete breaks live features or
poaches Phase 14's scope.
**How to avoid:** Remove ONLY the explicit capture-family tokens listed in section D. Bink is OUT
(active cutscene codec); Vivox is Phase 14; zlib/dpvs are live.

### Pitfall 5: Stale `.obj` and build artifacts masking the removal
**What goes wrong:** `src/compile/win32/clientGraphics/{Debug,Release}/SwgVideoCapture.obj` already
exist; an incremental build may relink the stale `.obj` even after the source is gone.
**How to avoid:** Clean/rebuild (or delete the stale `.obj`) before the verification link.
**Warning signs:** Capture symbols still resolve after the wrapper source is deleted.

## State of the Art

| Old Approach (CONTEXT.md framing) | Current Reality (verified) | Impact |
|-----------------------------------|----------------------------|--------|
| Edit `libraries_*.rsp` to unlink | Edit inline `.vcxproj` `<Link>` (3 configs); `.rsp` is vestigial | Build gate driven by `.vcxproj`, not `.rsp` |
| Live caller chain via parser → Game | Parser + Game methods are `#if 0` (dead); live caller is `CuiIoWin.cpp` | Narrower, different live footprint |
| `SwgAudioCapture.h` exists in clientAudio | File does not exist; include is dead | Nothing to delete there beyond the dead include |

**Deprecated/outdated:**
- The console commands `videoCaptureStart/Stop/Config` were already commented out / `#if 0`'d by
  SOE/Koogie — vestigial debug feature, no user-facing surface to preserve.

## Runtime State Inventory

> This is a code/build-only removal, but the rename/refactor inventory categories are answered for
> completeness (the removal touches build artifacts).

| Category | Items Found | Action Required |
|----------|-------------|------------------|
| Stored data | None — no datastore keys/collections reference VideoCapture. Verified: no DB/config persistence of capture state. | None |
| Live service config | None — recorder is in-process only; no external service. | None |
| OS-registered state | None — no scheduled tasks / services. | None |
| Secrets/env vars | None. | None |
| Build artifacts | Stale `src/compile/win32/clientGraphics/{Debug,Release}/SwgVideoCapture.obj` exist; will be orphaned after source delete. Also `external/3rd/library/videocapture/*/lib/win32/*.lib` (the vendored libs themselves) deleted by D-03. | Clean/rebuild `clientGraphics` (or delete the stale `.obj`) before the verification link, per Pitfall 5. |

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| MSBuild / VS 2026 (v145) | `swg.sln` build | ✓ (per STATE — Option D toolchain) | VS 18.1.11312 (sln header) | — |
| `link.exe` `/VERBOSE` output | D-06 link-output grep | ✓ (already enabled in `SwgClient.vcxproj` line 110) | — | — |
| SWGSource VM @ 192.168.1.200 | D-05 dual-renderer boot gate | (per prior phases) | SWGSource v3.0 | — |
| `gl05_d.dll` / `gl11_d.dll` | rasterMajor=5 / =11 boot | ✓ (Phase 11 shipped gl11_d.dll) | — | — |

**Missing dependencies with no fallback:** None identified — same toolchain/VM that built Phase 12.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | None (C++ game client; no unit-test harness). Validation is build-gate + manual boot-gate. |
| Config file | none — see Wave 0 |
| Quick run command | `link /VERBOSE` output grep for `unresolved external symbol` (count == 0) |
| Full suite command | `swg.sln` Debug + Release build of SwgClient, then dual-renderer boot to character select |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| DECRUFT-04 (crit #1) | Zero `VideoCapture`/`AudioCapture` refs in `.rsp`, source, include paths | grep gate | `rg -i "videocapture\|VideoCapture\|AudioCapture" --glob "*.rsp"` and `--glob "*.{cpp,h,inl}"` → 0 hits (excluding `.planning/`) | ✅ (rg available) |
| DECRUFT-04 (crit #2) | `swg.sln` builds clean Debug AND Release, no unresolved-symbol / missing-lib | build + link-grep gate | Build SwgClient Debug + Release; grep each link log for `unresolved external symbol` → 0 (NOT exit code, per `/FORCE`) | ❌ Wave 0 (link-log capture step must be authored) |
| DECRUFT-04 (crit #3) | Boots to character select under rasterMajor=5 AND =11 | manual boot gate | Launch `SwgClient_d.exe` (reads `client_d.cfg`) with `rasterMajor=5` then `=11` against the VM | ❌ manual (human-run, per prior phases) |

### Sampling Rate
- **Per task commit:** `.vcxproj` link-list grep (capture tokens removed) + source grep.
- **Per wave merge:** Build SwgClient Debug; grep link log for `unresolved external symbol == 0`.
- **Phase gate:** Debug + Release both link-clean (link-log grep), full repo grep == 0 for the
  `.rsp`/source/include scope, dual-renderer boot to character select.

### Wave 0 Gaps
- [ ] Author a link-log capture step: build with output redirected so the link `/VERBOSE` text can be
      grepped for `unresolved external symbol` (the `/FORCE` false-pass guard from D-06). Phase 12
      did this ad-hoc; formalize the command for both Debug and Release.
- [ ] Decide the grep scope for criterion #1 vs. D-04 intent (see Open Question 1) so the gate is
      deterministic.
- [ ] No test files / framework install needed — this is a build+boot validated phase.

## Security Domain

> `security_enforcement: true`, ASVS level 1. This is a dead-code REMOVAL — it reduces attack surface
> rather than adding it. No new inputs, endpoints, crypto, auth, or session handling are introduced.

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | No auth code touched. |
| V3 Session Management | no | No session code touched. |
| V4 Access Control | no | No access-control code touched. |
| V5 Input Validation | no | No new inputs; removing a debug-console command path. |
| V6 Cryptography | no | No crypto touched (`crypto.lib`/`989crypt` unrelated, untouched). |
| V14 Configuration (build) | yes | Removing proprietary PICTools/PICVideo (`picn20*.lib`) middleware reduces 3rd-party binary attack surface; `/FORCE` masking is a build-integrity concern addressed by D-06's link-grep. |

### Known Threat Patterns for this change

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Silent unresolved-external left in binary under `/FORCE` (latent crash / undefined behavior if reanimated) | Denial of Service / Tampering | D-06 link-output grep == 0; atomic removal of caller + provider together. |
| Over-broad delete removing a live security-relevant lib (e.g., `crypto.lib`, `989crypt`) | Tampering | Section D allow-list of capture-only tokens; explicit do-not-touch list (Bink/zlib/dpvs/Vivox). |

**Net security posture:** Improved — one fewer proprietary, unmaintained, D3D9-only middleware family
(VideoCapture + PICTools) in the shipped client, plus removal of a latent debug recorder code path.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | "Debug AND Release" in criterion #2 maps to the `Debug\|Win32` and `Release\|Win32` solution configs; the `Optimized\|Win32` config is not separately gated but should be cleaned for coherence. | Build Graph / Validation | LOW — if the gate intends Optimized too, it's already covered by the recommendation to purge all 3. |
| A2 | The editors (AnimationEditor et al.) are pre-broken on Qt and not part of the SwgClient build gate, so purging their `.rsp`/`.vcxproj` refs is grep-gate-only and cannot break the gated build. | Build Graph / Inventory B | LOW — consistent with Decruft memory + STATE; if an editor unexpectedly builds, removing a lib it links could surface a different error, but they don't compile today. |
| A3 | Deleting the vendored `videocapture/` tree (D-03) breaks nothing else, because only SwgClient's now-removed inline paths and the vestigial `.rsp`/editor refs point at it. | Edit Order step 6 | LOW — grep confirms no other consumers; verified the 14 `.vcxproj` + `.rsp` set. |

## Open Questions (RESOLVED during planning, 2026-05-25)

> Both questions were operationally answered by the Phase 13 plan set (commit `66ca59479`).
> Resolutions are annotated inline below.

1. **Criterion #1 grep scope vs. D-04 "repo-wide" intent — editor `.vcxproj` inline refs.**
   - **RESOLVED:** Plan 13-02 Task 3 purges the editor `.vcxproj` inline refs (satisfying D-04's
     repo-wide intent), and Plan 13-03 Task 1's full-repo grep gate covers `.vcxproj` alongside the
     `.rsp`/source/include scope. The `.rsp` + source + include grep remains the hard criterion-#1 gate.
   - What we know: Criterion #1 literally scopes the zero-reference grep to `.rsp`, source, and
     include paths. D-04 says "purge every reference repo-wide." The editor `.vcxproj` files carry
     inline `<AdditionalDependencies>` capture tokens that are NOT in criterion #1's literal scope but
     ARE the real (vestigial) wiring.
   - What's unclear: Whether the gate's "grep finds zero `VideoCapture` references" includes
     `.vcxproj`. If it does, the 9 editor `.vcxproj` + `SwgGodClient` + `Viewer`/`TextureBuilder` must
     also be purged (more edit surface, but they don't build, so it's safe).
   - Recommendation: Purge the editor/`SwgGodClient`/`Viewer`/`TextureBuilder` `.vcxproj` inline refs
     too (they're cheap, safe, and satisfy D-04's intent literally), but treat the `.rsp` + source +
     include grep as the hard criterion-#1 gate. Confirm scope with the planner/discuss before locking
     the verification command.

2. **`Optimized|Win32` config gating.** Criterion #2 names "Debug and Release." `Optimized` also
   carries capture refs.
   - **RESOLVED:** Plan 13-01 Task 3 purges capture tokens/paths from all 3 `<Link>` configs
     (Debug @103, Optimized @158, Release @204), so `Optimized` is cleaned for coherence; the
     link-grep gate (Plan 13-01 Task 3 / Plan 13-03 Task 2) runs on Debug + Release per criterion #2.

## Sources

### Primary (HIGH confidence — direct live-tree inspection at git `02f36d677`)
- `SwgClient.vcxproj` — 3 `<Link>` blocks (Debug @103, Optimized @158, Release @204),
  `<ForceFileOutput>Enabled</ForceFileOutput>` @113, `/VERBOSE` @110; no `.rsp` references.
- `clientGraphics.vcxproj` — SwgVideoCapture `<ClCompile>` @320 / `<ClInclude>` @436;
  `videocapture` `<AdditionalIncludeDirectories>` (3 configs); `DEBUG_LEVEL=2` preprocessor.
- `clientGame.vcxproj`, `clientAudio.vcxproj` — `videocapture` include path (lines 73/111/148).
- `CuiIoWin.cpp` (19, 209), `SwgVideoCapture.cpp/.h`, `Game.cpp` (157, 3014-3064), `Game.h` (257-261),
  `Audio.cpp` (43-45, 5405-5470), `Audio.h` (22-25, 321-327), `SwgCuiCommandParserScene.cpp`
  (265-269, 532-539, 4895-4926) — all read and guard-state verified.
- `Production.h` (src/shared) — `DEBUG_LEVEL==0 → PRODUCTION 1; else PRODUCTION 0`.
- `swg.sln` — SolutionConfigurationPlatforms (Debug/Optimized/Release|Win32), editor project
  `.Build.0` entries.
- `external/3rd/library/videocapture/` — directory listing + `.lib` file existence (VideoCapture_*,
  AudioCapture.lib, picn20*.lib).
- Repo-wide greps: `*.rsp` (capture refs), `*.vcxproj` (14 files), `*.{cpp,h,inl}` (9 files),
  `VideoCapture::|AudioCapture::|Smart::|...` symbol usages (closed set).

### Secondary (MEDIUM confidence)
- `.planning/STATE.md` §Blockers/Concerns — Phase 12 `/FORCE` false-pass + inline-`.vcxproj` findings
  (corroborated by direct `.vcxproj` inspection above).
- Memory `project_decruft_removal_build_graph_gotchas` — `.rsp` vestigial / edit inline vcxproj;
  `/FORCE` masks unresolved-externals; editors pre-broken on Qt (all corroborated).

### Tertiary (LOW confidence)
- None — every load-bearing claim was verified against the live tree.

## Metadata

**Confidence breakdown:**
- Removal surface / symbol resolution: HIGH — every file and guard state directly inspected.
- Build-graph (inline `.vcxproj` vs `.rsp`): HIGH — confirmed `SwgClient.vcxproj` has zero `.rsp`
  refs and all 3 configs' inline link lists read.
- `/FORCE` gate: HIGH — `<ForceFileOutput>Enabled</ForceFileOutput>` + `/VERBOSE` confirmed in file.
- Editor build status (pre-broken on Qt): MEDIUM — from Decruft memory + STATE, not re-validated by a
  build in this session.
- Boot-gate environment: MEDIUM — inferred from prior phases; not exercised in this session.

**Research date:** 2026-05-25
**Valid until:** 2026-06-24 (stable — local C++ tree, no fast-moving external deps; re-verify line
numbers if the working tree changes before planning)
