# Phase 13: VideoCapture Library Unlink - Context

**Gathered:** 2026-05-25
**Status:** Ready for planning

<domain>
## Phase Boundary

Shed the dormant **VideoCapture** in-client recorder from the active MSBuild tree
(`src/build/win32/swg.sln`) — link, engine source, callers, and console-command
surface — plus its bundled **AudioCapture** sibling, leaving the client bootable to
character select under **both** renderers. Satisfies **DECRUFT-04**.

VideoCapture is a non-functional, D3D9-only, proprietary (PICTools/PICVideo) gameplay
recorder; its capture path guards on `IDirect3DDevice9` and has no D3D11 equivalent.
The decision to **remove** (not keep) was made explicitly during discussion — it
offers no working feature on the modern stack and only adds port surface + a latent
crash path. This phase clarifies HOW to remove it, not WHETHER.

**Not a one-line `.rsp` edit** (despite the roadmap summary's framing): the lib is
backed by live engine source — a wrapper in `clientGraphics`, callers + a callback
class in `clientGame`, an audio-capture sibling in `clientAudio`, and console-command
dispatch in `swgClientUserInterface`. All of it is in scope.

</domain>

<decisions>
## Implementation Decisions

### Removal Approach
- **D-01:** **Full delete the entire closed caller chain together — no no-op stubs.**
  Chain: `SwgCuiCommandParserScene` handlers → `Game::videoCapture*` + `VideoCaptureCallback`
  → `SwgVideoCapture` wrapper → the lib. Phase 12's stub-in-place (`ClientHeadTracking.cpp`)
  was only needed because *its* callers stayed; here every consumer is removed, so deleting
  the chain atomically leaves no orphaned symbols.

### Removal Scope
- **D-02:** **Remove the WHOLE capture family** — `VideoCapture` AND its audio-capture
  sibling `AudioCapture` (both live under `external/3rd/library/videocapture/`). Concretely:
  - `clientGraphics` — `SwgVideoCapture.cpp/.h` (+ public re-include header)
  - `clientGame` — `Game::videoCaptureConfig/Start/Stop`, `VideoCaptureCallback`, and the
    `SwgAudioCaptureManager` definition (all in `Game.cpp`/`Game.h`)
  - `clientAudio` — `Audio::getAudioCaptureConfig/startAudioCapture/stopAudioCapture`,
    the `namespace AudioCapture` surface, and `clientAudio/SwgAudioCapture.h`
  - `swgClientUserInterface` — the `videoCaptureConfig/Start/Stop` console-command consts +
    live dispatch handlers
- **D-02a:** **Live Miles audio playback is OUT of scope and untouched.** The audio-capture
  methods are a separate DirectShow-style `"AudioCapture Filter"` (`AIL_find_filter`) with
  **zero independent callers** (grep-confirmed) — they exist solely to feed the recorder.
  Gameplay music/SFX/voice go through the unrelated `AIL_*` playback path.

### External SDK & Reference Purge
- **D-03:** **Delete the vendored `src/external/3rd/library/videocapture/` tree entirely**
  (AudioCapture, CaptureCommon, Docs, ImageCapture, PICTools, Smart, SoeUtil, VideoCapture,
  ZlibUtil). Mirrors Phase 12's trackIR/stationapi dir-delete precedent (DECRUFT-01).
- **D-04:** **Purge every `VideoCapture`/`AudioCapture` reference repo-wide, INCLUDING the
  out-of-scope editor `.rsp` files** (`AnimationEditor`, `ClientEffectEditor`, + any others
  a fresh grep enumerates). Required to satisfy DECRUFT-04 criterion #1's "grep finds zero
  `VideoCapture` references across `.rsp`" literally. Mirrors Phase 12's dangling-editor-ref
  cleanup (commit `1d6b80242`). Editors are pre-broken/out-of-scope as *build targets* — but
  their stale refs must still go to pass the gate.

### Verification (carried forward — LOCKED by the v2.1 milestone invariant)
- **D-05:** **Dual-renderer boot gate** after the unlink — boots to character select under
  **both** `rasterMajor=5` (D3D9) and `=11` (D3D11). Set `rasterMajor` in `client_d.cfg`
  (the **debug** exe reads `client_d.cfg`, not `client.cfg`).
- **D-06:** **Build gate greps link output for 0 `unresolved external symbol`** — NOT just
  MSBuild exit 0. `/FORCE` downgrades unresolved externals to warnings and still emits a
  binary (Phase 12 finding). **Debug AND Release** must both link clean.

### Claude's Discretion
- Plan/commit granularity and the exact edit order within the atomic removal — planner's call,
  provided the removal lands as a coherent unit that never leaves the tree un-buildable mid-step.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Requirement & roadmap (this phase)
- `.planning/REQUIREMENTS.md` — **DECRUFT-04** (the requirement) + Out-of-Scope table
  (Bink codec removal is explicitly OUT; stale `stlport453/` `.rsp` strings are inert).
- `.planning/ROADMAP.md` §"Phase 13: VideoCapture Library Unlink" — Goal + 3 success criteria.

### Prior-phase precedent (apply the lessons)
- `.planning/STATE.md` §Blockers/Concerns — the **`/FORCE` false-pass** finding and the
  "dead modules link via inline `.vcxproj` deps, not just `.rsp`" finding from Phase 12.
- `.planning/PROJECT.md` §"Current Milestone: v2.1 — Decruft" — milestone framing + invariant.
- Memory `project_decruft_removal_build_graph_gotchas` — `.rsp` vestigial / edit inline
  vcxproj; `/FORCE` masks unresolved-externals (grep for 0); editors pre-broken on Qt.

### Reference diff template (external — sibling repo, not in this tree)
- Original **whitengold (swg-client) Phase 07** removal commits (CLEAN-01..05; the video bits
  were bundled in CLEAN-02), retargeted CMake → MSBuild. Reference only — the active tree is
  the Koogie MSBuild tree, so paths/build-graph differ. No in-repo path; treat as a shape guide.

</canonical_refs>

<code_context>
## Existing Code Insights

### Removal surface (files the planner will touch)
**Build / link (SwgClient):**
- `src/game/client/application/SwgClient/build/win32/libraries_d.rsp:3` → `VideoCapture_debug.lib`
- `.../SwgClient/build/win32/libraries_r.rsp` + `libraries_o.rsp` → `VideoCapture_release.lib`
- `.../SwgClient/build/win32/libraryPaths.rsp:10-16` → AudioCapture, VideoCapture, ImageCapture,
  Smart, ZlibUtil, SoeUtil, PICTools search paths

**Engine source — `clientGraphics` (wrapper):**
- `src/engine/client/library/clientGraphics/src/shared/SwgVideoCapture.cpp` (`#include "VideoCapture/VideoCapture.h"`)
- `.../clientGraphics/src/shared/SwgVideoCapture.h`
- `.../clientGraphics/include/public/clientGraphics/SwgVideoCapture.h` (public re-include)
- → drop these from the `clientGraphics` `.vcxproj` compile list (check inline, not just `.rsp`).

**Engine source — `clientGame` (callers + manager):**
- `.../clientGame/src/shared/core/Game.cpp:157` (`#include "clientGraphics/SwgVideoCapture.h"`)
- `Game.cpp:3015-3062` — `videoCaptureConfig/Start/Stop`, `VideoCaptureCallback`, `SwgAudioCaptureManager`
- `Game.h:258-260` — the three method declarations

**Engine source — `clientAudio` (audio-capture sibling):**
- `.../clientAudio/src/win32/Audio.cpp:44` (`#include "clientAudio/SwgAudioCapture.h"`)
- `Audio.cpp:5407-5468` — `getAudioCaptureConfig/startAudioCapture/stopAudioCapture` (Miles `"AudioCapture Filter"`)
- `Audio.h:22` (`namespace AudioCapture`), `Audio.h:321-326` (declarations)
- `clientAudio/.../SwgAudioCapture.h` (wrapper header)

**Game UI — `swgClientUserInterface` (console-command surface):**
- `.../swgClientUserInterface/src/shared/parser/SwgCuiCommandParserScene.cpp:266-268` (command-name consts)
- `:536-538` — help-table entries **already commented out** (SOE/Koogie were phasing it out)
- `:4896-4924` — **live** dispatch handlers calling `Game::videoCapture*` (these still fire)

**External vendored SDK (delete the tree):**
- `src/external/3rd/library/videocapture/` — AudioCapture, CaptureCommon, Docs, ImageCapture,
  PICTools, Smart, SoeUtil, VideoCapture, ZlibUtil

**Out-of-scope-as-targets but refs must be purged (D-04):**
- `src/engine/client/application/AnimationEditor/build/win32/*.rsp`
- `src/engine/client/application/ClientEffectEditor/build/win32/*.rsp`
- (researcher: re-grep `--include="*.rsp"` for `VideoCapture|videocapture` to enumerate the full set)

### Established Patterns (from Phase 12 — apply directly)
- **`/FORCE` false-pass:** SwgClient links under `/FORCE` → unresolved externals become warnings,
  binary still emits at exit 0. Build gate MUST grep link output for `unresolved external symbol` (== 0).
- **Inline `.vcxproj` deps, not just `.rsp`:** the `.rsp` files can be vestigial; the real compile/
  link wiring is often inline in the `.vcxproj`. Check both.
- **Closed-chain delete > stub:** stub-in-place was a Phase-12 workaround for callers that *stayed*;
  here the whole chain goes, so atomic delete is correct.

### Integration / blast-radius notes for research
- `clientGraphics` is the engine lib shared by **both** the D3D9 and D3D11 renderers — both must
  rebuild + link clean after `SwgVideoCapture` is removed.
- **Symbol-resolution risk to confirm:** where do `VideoCapture::` / `AudioCapture::` / `Smart::`
  symbols currently resolve (VideoCapture_debug.lib vs header-only/inline)? If from the lib, every
  consumer must be removed in the same change or the link breaks — reinforcing the atomic-delete (D-01).

</code_context>

<specifics>
## Specific Ideas

- The recorder is invoked only via debug console commands (`videoCaptureStart/Stop/Config`), whose
  help entries SOE/Koogie already commented out — confirming it's a vestigial debug feature, not a
  user-facing one. No UI surface to preserve.

</specifics>

<deferred>
## Deferred Ideas

- **Bink video-codec removal** — explicitly OUT of v2.1 scope (active cutscene codec, higher blast
  radius than the dormant capture modules; per `.planning/REQUIREMENTS.md` Out-of-Scope). Do not
  conflate with VideoCapture — different middleware.
- Stale `stlport453/` path strings in `.rsp` files — inert; opportunistic cleanup only if a removal
  edit already touches the same file.

*Otherwise: discussion stayed within phase scope.*

</deferred>

---

*Phase: 13-videocapture-library-unlink*
*Context gathered: 2026-05-25*
