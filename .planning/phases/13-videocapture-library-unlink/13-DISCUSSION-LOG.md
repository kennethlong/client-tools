# Phase 13: VideoCapture Library Unlink - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-05-25
**Phase:** 13-videocapture-library-unlink
**Areas discussed:** Keep-or-remove (reconsideration), Removal depth, SDK family scope, External SDK dir

---

## Keep or Remove? (reconsideration before planning a removal)

User opened by asking whether VideoCapture should be kept at all, and for the advantages/disadvantages. Tradeoff analysis surfaced: VideoCapture is a D3D9-only (`IDirect3DDevice9`-guarded), proprietary PICTools/PICVideo recorder; its console-command help entries are already commented out; modern external tools (OBS/Win+G) obsolete it; removal touches `clientGraphics` (shared renderer lib).

| Option | Description | Selected |
|--------|-------------|----------|
| Remove it | Proceed with Phase 13 as scoped — unlink + purge source. The Decruft target. | ✓ |
| Keep it | Abandon/redefine Phase 13; leave VideoCapture linked + in source. | |

**User's choice:** Remove it
**Notes:** Non-functional on the modern stack, proprietary, fully replaced by external recorders, adds port surface + latent crash risk for zero working feature.

---

## Removal Depth

| Option | Description | Selected |
|--------|-------------|----------|
| Full delete | Delete the whole closed chain together (command handlers + Game::videoCapture* + VideoCaptureCallback + SwgVideoCapture wrapper). No orphaned symbols. | ✓ |
| Stub to no-ops | Keep Game::videoCapture* signatures + command handlers as empty no-ops; delete only wrapper + lib. | |

**User's choice:** Full delete
**Notes:** The caller chain is closed and entirely in scope, so atomic deletion avoids the `/FORCE` orphan-symbol trap without needing Phase 12's stub-in-place workaround.

---

## SDK Family Scope

| Option | Description | Selected |
|--------|-------------|----------|
| Whole capture family | Remove video AND its audio-capture sibling (Audio::*AudioCapture*, SwgAudioCapture.h, SwgAudioCaptureManager) + all videocapture/ libraryPaths. | ✓ |
| VideoCapture-only | Remove just video pieces; leave the Audio::startAudioCapture surface in place. | |

**User's choice:** Whole capture family
**Notes:** Grep confirmed the audio-capture methods have zero independent callers — they exist only to feed the recorder. Live Miles `AIL_*` playback audio is a separate path, untouched. Removing video-only would risk orphaned `AudioCapture::` symbols after the lib unlink.

---

## External SDK Dir + Reference Breadth

| Option | Description | Selected |
|--------|-------------|----------|
| Delete dir + purge all refs | rm the videocapture/ vendored tree AND purge every ref repo-wide, incl. out-of-scope editor .rsp files. | ✓ |
| Delete dir, SwgClient-scoped | rm the tree + purge SwgClient/engine refs only; leave editor .rsp refs dangling. | |
| Unlink-only, keep dir | Leave the vendored tree on disk; just drop SwgClient's lib link + engine source. | |

**User's choice:** Delete dir + purge all refs
**Notes:** Needed to satisfy DECRUFT-04 criterion #1's literal "zero VideoCapture references across .rsp." Mirrors Phase 12's dir-delete + dangling-editor-ref cleanup (commit `1d6b80242`).

---

## Claude's Discretion

- Plan/commit granularity and exact edit order within the atomic removal — left to the planner, provided the tree never goes un-buildable mid-step.

## Deferred Ideas

- Bink video-codec removal — explicitly OUT of v2.1 scope (active cutscene codec; different middleware from VideoCapture).
- Stale `stlport453/` `.rsp` path strings — inert; opportunistic cleanup only.
