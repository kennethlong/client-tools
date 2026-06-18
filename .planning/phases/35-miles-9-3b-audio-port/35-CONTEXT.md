# Phase 35: Miles 9.3b Audio Port - Context

**Gathered:** 2026-06-18
**Status:** Ready for planning

<domain>
## Phase Boundary

Vendor the Miles 9.3b SDK, port the `clientAudio` call sites off the 7.2e API, undo the
Phase-33 Miles-disable, and stage the x64 redist/provider set so in-game audio (music, 2D UI,
3D positional, reverb/room-type) works on the x64 client — satisfying AUDIO-01 / AUDIO-02.

**Scope-shaping decision (D-01):** This is no longer an x64-only swap. The user elected to move
**both** Win32 and x64 to Miles 9.3b in one phase, retiring 7.2e project-wide. The API port
therefore lands in the **shared** code path (not under `#if _M_X64`), and the 32-bit build is
re-validated against 9.3b — not left on 7.2e.

In scope: vendor 9.3b SDK; port `Audio.cpp` / `SoundObject3d.cpp` / `FirstClientAudio.h` to the
9.x API for both platforms; relink (`mss32.lib` Win32, `mss64.lib` x64) from the 9.3b SDK; remove
the Phase-33 x64 stubs + the install() disable; stage the full 9.3b provider set for both Win32
and x64 stages; UAT both renderers/platforms for working audio.

Out of scope (own phases): the CORNERSNAP/OOM verification (Phase 36); any new audio features
(banks, new codecs) beyond restoring 7.2e-equivalent behavior on 9.3b.

</domain>

<decisions>
## Implementation Decisions

### 32-bit Miles policy (SC#4 fork)
- **D-01:** **Upgrade BOTH platforms to 9.3b** — do not keep Win32 on 7.2e. Retire `miles-7.2e`
  (and the unversioned `miles/`) as the active dependency; repoint both platforms' lib + include +
  redist to the vendored `miles-9.3b`. Consequence: the 7.2e→9.3b API edits move into the shared
  (non-`_M_X64`) code path, and the 32-bit client must be re-verified for audio (SC#4 is satisfied
  via "the swap is applied consistently and audio still works in Win32", not via "unregressed 7.2e").
- **D-02:** SDK provides both `mss32.lib` (Win32) and `mss64.lib` (x64) at
  `D:\Code\milesss-v9.3b\win\sdk\lib\` — vendor both; per-platform lib selection in
  `clientAudio.vcxproj` selects by platform, but **the same 9.3b header set** compiles for both.

### x64 enable strategy
- **D-03:** **Delete the Phase-33 x64 disable — Miles always-on per platform.** Remove the Phase-33
  `#if defined(_M_X64)` AIL_* macro-stub block (`Audio.cpp:250-310`), the `SoundObject3d.cpp:32`
  `#if !defined(_M_X64)` gate, and the `install()` x64 early-return (`Audio.cpp:1306-1318`,
  `s_disableMiles=true` x64 force-disable). No **x64-specific** disable survives — x64 runs the same
  real AIL_* path as Win32.
- **D-04 (user clarification — KEEP the legacy short-circuit):** **Leave the legacy `disableMiles`
  `[ClientAudio]` cfg short-circuit (`Audio.cpp:1300`) intact.** It is older retail config, NOT
  Phase-33 scaffolding — "delete all disable paths" scoped to the **Phase-33 x64** disable only.
  Keeping it preserves the user-facing off-switch (default `disableMiles=false` → Miles on) and gives
  a committed, supported way to silence audio if x64 bring-up destabilizes the boot. Net: the only
  remaining disable path is the legacy cfg flag, which applies equally to both platforms.

### Provider / redist set
- **D-05:** **Full 7.2e-parity provider set**, staged for each platform's stage dir:
  - x64 (next to x64 exe): `mss64.dll` + `mss64mp3.asi` + `mss64ogg.asi` + `mss64dsp.flt` +
    `mss64eax.flt` + `mss64ds3d.flt` (+ `mss64dolby.flt` / `mss64srs.flt`) + `binkawin64.asi`
  - Win32 (next to 32-bit exe): the 9.3b **32-bit** equivalents (`mss32.dll` + `mssmp3.asi` +
    `mssogg.asi` + `mssdsp.flt` + `msseax.flt` + `mssds3d.flt` + filters + `binkawin.asi`)
  - Rationale: reverb/room-type is an explicit success criterion → the EAX / DS3D filters must be
    present, not just the mp3/ogg decoders. Mirror the working 7.2e `redist/` contents (which include
    `msseax.flt` / `mssds3d.flt`). Providers live across `win/sdk/redist64`, `win/mp3/redist64`,
    `win/ogg/redist64` in the SDK — research must gather them from all three.

### Audio acceptance test (AUDIO-02 bar)
- **D-06:** Manual UAT scene checklist, all four named dimensions, on the x64 client (and a Win32
  smoke for D-01 regression):
  1. **Music** — char-select / login music plays.
  2. **2D UI** — UI rollover + click SFX audible (the "UI-works-but-silent" signature = Miles
     half-dead; must NOT recur).
  3. **3D positional** — cantina interior 3D ambient localizes.
  4. **Reverb / room-type** — interior reverb / room-type change is audible on cell entry.
  - Pass bar: all four present **with no warning-flood lag** (the missing-`stage/miles` failure
    signature). Silent-or-warning-flood = fail.

### Claude's Discretion
- Vendoring directory **internal layout** under `src/external/3rd/library/miles-9.3b/` — mirror the
  existing `miles-7.2e/` convention (`include/`, `lib/win/`, `redist/`) vs the SDK's native
  `win/sdk/...` tree. Planner/executor's call; keep diff minimal and consistent with the repo.
- Whether to physically `git rm` the dormant `miles-7.2e` + unversioned `miles/` trees in this phase
  or leave them unreferenced for a later cleanup — minimize risk; repointing references is the
  decision, deletion is optional.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase scope & requirements
- `.planning/ROADMAP.md` §"Phase 35: Miles 9.3b Audio Port" — goal + 4 success criteria (the
  AIL_room_type `, 0` signature edit is called out explicitly).
- `.planning/REQUIREMENTS.md` — AUDIO-01 (link + port call sites) / AUDIO-02 (redist + working audio).

### The SDK (external, absolute paths — git is at `D:\Code\milesss-v9.3b\.git`)
- `D:\Code\milesss-v9.3b\win\sdk\include\mss.h` — the 9.3b API header; **diff against**
  `src/external/3rd/library/miles-7.2e/include/mss.h` to enumerate EVERY 7.2e→9.x signature change
  (AIL_room_type is the known one; research must find the rest — this is the core research task).
- `D:\Code\milesss-v9.3b\win\sdk\lib\` — `mss32.lib` (Win32) + `mss64.lib` (x64) to vendor.
- `D:\Code\milesss-v9.3b\win\sdk\redist64\` + `win\mp3\redist64\` + `win\ogg\redist64\` — x64 providers.
- `D:\Code\milesss-v9.3b\win\sdk\redist\` + `win\mp3\redist\` + `win\ogg\redist\` — Win32 providers.
- `D:\Code\milesss-v9.3b\README.md` + `doc\` — SDK migration notes.

### Code to port (current 7.2e call sites + Phase-33 stubs to remove)
- `src/engine/client/library/clientAudio/src/win32/Audio.cpp` — port site; Phase-33 macro-stubs
  (`:250-310`), TreeFile file-callback UINTa ABI note (`:312+`), install() x64 disable (`:1306-1318`),
  legacy `disableMiles` short-circuit (`:1300`).
- `src/engine/client/library/clientAudio/src/win32/SoundObject3d.cpp` — `:32` `#if !defined(_M_X64)` gate.
- `src/engine/client/library/clientAudio/include/public/clientAudio/FirstClientAudio.h` — Miles include/config.
- `src/engine/client/library/clientAudio/build/win32/` — the clientAudio `.vcxproj` (per-platform lib).

### Existing 7.2e vendoring (the layout/redist to mirror & repoint)
- `src/external/3rd/library/miles-7.2e/` — `include/mss.h`, `lib/win/Mss32.lib`, `redist/` (note it
  ships `msseax.flt` + `mssds3d.flt` → confirms the reverb/3D filter set for D-05).
- `src/external/3rd/library/miles/` — unversioned legacy copy (also repoint/retire).

### Project memory
- `C:\Users\kenne\.claude\projects\D--Code-swg-client-v2\memory\project_miles_9_3b_local_sdk.md`
- `C:\Users\kenne\.claude\projects\D--Code-swg-client-v2\memory\project_audio_fixed_missing_miles_redist.md`
  — the `stage/miles/` missing-redist failure signature this phase's UAT must guard against.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- **Phase-33 stub block as a complete AIL_* call-site inventory:** `Audio.cpp:250-310` macro-defines
  ~60 AIL_* symbols that are exactly the call sites in this TU — a ready-made checklist of every API
  the port must satisfy against the 9.3b header.
- **`miles-7.2e/` vendoring layout** is the template for `miles-9.3b/` (include/ + lib/win/ + redist/).

### Established Patterns
- **Per-platform lib in vcxproj** (Win32 vs x64) is the existing mechanism for selecting the right
  Mss lib; reused here to pick `mss32.lib` vs `mss64.lib` from the 9.3b SDK.
- **TreeFile file-callback ABI** already uses pointer-width `UINTa` (Phase-33 note at `Audio.cpp:312+`)
  — the 9.x callback signatures must stay pointer-width-correct on x64.
- **`if (s_installed)` guards** gate every downstream play/alter/query consumer — once `install()`
  succeeds on x64, those paths light up automatically.

### Integration Points
- `Audio::install()` is the single enable gate — removing the x64 early-return (D-03) is what
  flips audio on.
- `stage/` postbuild auto-deploy lands the exe + plugin DLLs; the Miles redist is **separate**
  (`stage/miles/`, gitignored) — D-05 staging must be wired so it isn't lost on a fresh checkout
  (cross-ref the `project_audio_fixed_missing_miles_redist` failure).

</code_context>

<specifics>
## Specific Ideas

- The "UI-rollovers-work-but-silent" symptom is the canonical signature of half-dead Miles — D-06
  makes it an explicit fail condition, not an afterthought.
- The user deliberately chose the more ambitious, more consistent path on both forks (both-platforms
  + always-on) over the lower-risk x64-only/keep-toggle defaults — bias the plan toward a clean,
  single-version end state rather than a minimal x64 patch.

</specifics>

<deferred>
## Deferred Ideas

- Physical `git rm` of dormant `miles-7.2e` + unversioned `miles/` trees — optional cleanup, can be
  a follow-up; not required to satisfy AUDIO-01/02.
- New 9.3b-only audio capabilities (sound banks `.mssbnk`, additional codecs, Miles Sound Studio
  tooling) — out of scope; this phase restores 7.2e-equivalent behavior on the 9.3b runtime.

None blocking — discussion stayed within phase scope.

</deferred>

---

*Phase: 35-miles-9-3b-audio-port*
*Context gathered: 2026-06-18*
