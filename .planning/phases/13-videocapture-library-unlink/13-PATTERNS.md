# Phase 13: VideoCapture Library Unlink - Pattern Map

**Mapped:** 2026-05-25
**Files analyzed:** 9 source + 14 `.vcxproj` + 14 `.rsp` + 1 vendored tree (37 removal targets)
**Analogs found:** 37 / 37 (every removal target maps to a verified Phase 12 DECRUFT precedent)

> This is a **removal** phase. "Closest analog" = "how Phase 12 did this exact KIND of edit."
> The four canonical Phase-12 analog commits (all on this branch, verified by `git show`):
>
> | Commit | What it did | Analog shape for Phase 13 |
> |--------|-------------|---------------------------|
> | `adda94729` | Delete `stationapi/` vendored dir + purge its `<AdditionalLibraryDirectories>` from `SwgClient.vcxproj` (Debug/Optimized/Release) + drop orphaned `989crypt.lib` from `<AdditionalDependencies>` | **Vendored-tree delete + inline `SwgClient.vcxproj` lib-token + lib-path purge** (D-03, section D) |
> | `7a7da726d` | Delete `trackIR/` header-only dir + purge `trackIr` include path from `clientGame` `includePaths.rsp` AND its inline `<AdditionalIncludeDirectories>` (3 configs); kept the `.cpp` in build, stubbed it because **callers stayed** | **`<AdditionalIncludeDirectories>` purge for the 3 engine libs** (clientGraphics/clientGame/clientAudio). NOTE the inverse decision: here callers do NOT stay (D-01), so we DELETE not stub. |
> | `c10d19f10` | Remove `SwgClientSetup` Project block + config lines from `swg.sln`, then delete the dir | **Whole-unit atomic delete** shape (D-02/D-03) — though Phase 13 has no `.sln` block to remove (capture is a lib, not a project) |
> | `1d6b80242` | Remove dangling `<ProjectReference>` blocks from 4 editor `.vcxproj` that the earlier `.sln`/`.rsp`-scoped greps missed | **Editor `.vcxproj` inline-ref purge** (D-04) — the "grep gate catches what the build gate doesn't" precedent |
>
> **CRITICAL guard-state distinction (Pitfall 3, RESEARCH):** `#if PRODUCTION == 0` is **LIVE in Debug** (compiles); `#if 0` is **never compiled**. Only `SwgVideoCapture.cpp` + `CuiIoWin.cpp:209` are live; all `Game.cpp`/`Audio.cpp`/`SwgCuiCommandParserScene.cpp` capture code is `#if 0` source-residue.

---

## File Classification

| Target | Role | Data Flow | Closest Phase-12 Analog | Match Quality |
|--------|------|-----------|-------------------------|---------------|
| `SwgClient.vcxproj` | build-wiring (EXE link) | link-input | `adda94729` (SwgClient lib-token + lib-path purge) | **exact** |
| `clientGraphics.vcxproj` | build-wiring (compile/include) | compile-input | `7a7da726d` (include-path purge) + new `<ClCompile>`/`<ClInclude>` drop | exact (path) / role-match (item drop) |
| `clientGame.vcxproj` | build-wiring (include) | compile-input | `7a7da726d` (include-path purge, 3 configs) | **exact** |
| `clientAudio.vcxproj` | build-wiring (include) | compile-input | `7a7da726d` (include-path purge, 3 configs) | **exact** |
| `CuiIoWin.cpp` | live caller (engine source) | request-response (per-frame tick) | `7a7da726d` ClientHeadTracking caller-edit — but **DELETE not stub** (D-01) | role-match (inverted decision) |
| `SwgVideoCapture.cpp/.h` + public re-include | wrapper (engine source) | transform (frame→encoder) | `c10d19f10` whole-unit delete + `adda94729` source delete | exact |
| `Game.cpp` / `Game.h` | dead caller + manager (`#if 0`) | n/a (never compiled) | `7a7da726d` dead-source removal | role-match (pure residue) |
| `Audio.cpp` / `Audio.h` | dead audio-capture sibling (`#if 0`) | n/a (never compiled) | `7a7da726d` dead-source removal | role-match (pure residue) |
| `SwgCuiCommandParserScene.cpp` | dead console-command surface (`#if 0`) | n/a (never compiled) | `7a7da726d` dead-source removal | role-match (pure residue) |
| `external/3rd/library/videocapture/` (9 subdirs) | vendored SDK tree | n/a | `adda94729` stationapi / `7a7da726d` trackIR dir-delete | **exact** |
| 14 `.rsp` files (11 projects + 3 engine `includePaths.rsp`) | vestigial build-wiring | n/a (not referenced by any `.vcxproj`) | `1d6b80242` grep-gate-only purge | exact |
| 10 editor/aux `.vcxproj` (SwgGodClient, Viewer, TextureBuilder, 7 editors) | dead build-wiring (pre-broken on Qt) | n/a | `1d6b80242` dangling editor-ref purge | **exact** |

---

## Pattern Assignments

### `SwgClient.vcxproj` (build-wiring, link-input) — THE BUILD GATE

**Analog:** `adda94729` (Phase 12 stationapi). That commit removed an orphaned `989crypt.lib` token from `<AdditionalDependencies>` AND its `<AdditionalLibraryDirectories>` segments across all 3 configs of this exact file. Do the same shape for the capture family.

**Reality check (RESEARCH, HIGH confidence):** This `.vcxproj` references **no `.rsp` file**. The link is driven by these inline blocks. Editing `libraries_*.rsp` changes nothing about the build. There are **3 `<Link>` blocks**: Debug (line ~103), Optimized (line ~158), Release (line ~204).

**Lib-token removal — `<AdditionalDependencies>` (line 103, Debug config; mirror in Optimized 158 + Release 204):**

The Debug `<AdditionalDependencies>` clusters the capture tokens together. Remove exactly these (and only these) tokens wherever they appear across the 3 configs — leave `binkw32.lib`, `zlib.lib`, `dpvs.lib`, `vivox*` UNTOUCHED:

```
...tinyxmld_STL.lib;AudioCapture.lib;CaptureCommon_debug.lib;ImageCapture_debug.lib;picn20md.lib;Smart_debug.lib;SoeUtil_debug.lib;VideoCapture_debug.lib;ZlibUtil_debug.lib;libsndfile-1.lib;vivoxplatform.lib;vivoxsdk.lib;vivoxSharedWrapper_Debug.lib;vivoxSharedWrapper_debug.lib;VideoCapture_release.lib;ImageCapture_release.lib;Smart_release.lib;SoeUtil_release.lib;ZlibUtil_release.lib;picn20m.lib;ws2_32.lib;...
```

Tokens to excise (the full allow-list from RESEARCH section D — note the debug cluster AND the stray release cluster both appear in the Debug `<Link>` line):
`VideoCapture_debug.lib`, `VideoCapture_release.lib`, `AudioCapture.lib`, `CaptureCommon_debug.lib`, `CaptureCommon_release.lib`, `ImageCapture_debug.lib`, `ImageCapture_release.lib`, `Smart_debug.lib`, `Smart_release.lib`, `SoeUtil_debug.lib`, `SoeUtil_release.lib`, `ZlibUtil_debug.lib`, `ZlibUtil_release.lib`, `picn20md.lib`, `picn20m.lib`.
(Leave `vivox*` and `libsndfile-1.lib` — Vivox is Phase 14; `binkw32.lib` is Bink, OUT of scope.)

**Lib-path removal — `<AdditionalLibraryDirectories>` (line 105, Debug; mirror in Optimized + Release):**

The 8 `videocapture\...\lib\win32` segments appear consecutively. Remove exactly this run (verbatim from the file, `\` path separators):

```
...src\external\3rd\library\videocapture\AudioCapture\lib\win32;...\videocapture\CaptureCommon\lib\win32;...\videocapture\ImageCapture\lib\win32;...\videocapture\PICTools\Lib\win32;...\videocapture\Smart\lib\win32;...\videocapture\SoeUtil\lib\win32;...\videocapture\VideoCapture\lib\win32;...\videocapture\ZlibUtil\lib\win32;...\vivox\lib;...
```

(8 segments: AudioCapture, CaptureCommon, ImageCapture, PICTools, Smart, SoeUtil, VideoCapture, ZlibUtil. The next segment after the run is `vivox\lib` — STOP there.)

**Phase-12 DEVIATION to anticipate (from `adda94729`):** Phase 12's plan assumed stationapi "was never linked," but dropping the search path orphaned a still-listed `989crypt.lib` → `LNK1181`. The fix was to remove the dangling token too. Here the analog is the inverse risk: if you drop the lib *tokens* but leave the wrapper compile / `CuiIoWin` `run()` call, `/FORCE` masks the resulting `LNK2019` (Pitfall 1). Remove **caller + provider together** (atomic, edit order steps 1-3).

---

### `clientGraphics.vcxproj` (build-wiring, compile-input) — wrapper unwire

**Analog:** `7a7da726d` (Phase 12 trackIR include-path purge) for the `<AdditionalIncludeDirectories>` edit; plus a `<ClCompile>`/`<ClInclude>` item-drop (no exact Phase-12 twin — Phase 12 stubbed the `.cpp` rather than dropping it because its callers stayed; here the chain is deleted per D-01, so drop the items).

**`<ClCompile>` item (line 320) — DELETE:**
```xml
    <ClCompile Include="..\..\src\shared\SwgVideoCapture.cpp" />
```

**`<ClInclude>` item (line 436) — DELETE:**
```xml
    <ClInclude Include="..\..\src\shared\SwgVideoCapture.h" />
```

**`<AdditionalIncludeDirectories>` — remove the `videocapture` segment (line 73 Debug; mirror lines 111 Optimized + 148 Release):**
```
...external\3rd\library\directx9\include;..\..\..\..\..\..\external\3rd\library\videocapture;..\..\..\..\..\..\external\ours\library\archive\include;...
```
(Remove the single `...\external\3rd\library\videocapture;` segment. The segment before is `directx9\include`, after is `external\ours\library\archive\include` — both stay.)

---

### `clientGame.vcxproj` (build-wiring, compile-input) — include-path purge

**Analog:** `7a7da726d` — identical shape to the trackIR `includePaths` purge across 3 configs.

**`<AdditionalIncludeDirectories>` — remove the `videocapture` segment (line 73 Debug; mirror 111 + 148):**
```
...external\3rd\library\directx9\include;..\..\..\..\..\..\external\3rd\library\videocapture;..\..\..\..\..\..\external\ours\library\archive\include;...
```

---

### `clientAudio.vcxproj` (build-wiring, compile-input) — include-path purge

**Analog:** `7a7da726d`. Same single-segment removal, lines 73/111/148.
```
...external\3rd\library\bink\include;..\..\..\..\..\..\external\3rd\library\videocapture;..\..\..\..\..\..\external\ours\library\archive\include;...
```
> (clientAudio's neighbor segments differ slightly from clientGraphics — the segment before is `bink\include`. Confirm per-config; don't blind-copy line content across the 3 projects.)

---

### `CuiIoWin.cpp` (live caller, engine source) — THE LINCHPIN

**Analog:** `7a7da726d` (Phase 12 caller edit on ClientHeadTracking's consumers) — but **INVERTED decision**: Phase 12 kept callers and stubbed the impl because the callers stayed; here D-01 deletes the whole chain, so just remove the call. This is the one file RESEARCH found that CONTEXT.md's surface map **omitted** (Pitfall 2).

**Include (line 19) — DELETE (unguarded):**
```cpp
#include "clientGraphics/SwgVideoCapture.h"
```

**Call site (line 208-210) — DELETE the guarded call (the `#if PRODUCTION == 0` block exists ONLY for this call; remove the whole 3-line block):**
```cpp
#if PRODUCTION == 0
	VideoCapture::SingleUse::run();
#endif // PRODUCTION
```
> Context (lines 206-212): sits between `IoWin::draw();` and `Bloom::postSceneRender();`. Removing the block leaves the surrounding draw flow intact. This is per-frame code (`draw()`), which is why a `/FORCE`-masked unresolved external here is a latent runtime crash, not benign.

---

### `SwgVideoCapture.cpp` / `.h` + public re-include (wrapper, engine source) — DELETE FILES

**Analog:** `adda94729` / `c10d19f10` whole-unit source delete.

Delete all three (confirmed present):
- `src/engine/client/library/clientGraphics/src/shared/SwgVideoCapture.cpp` (whole file `#if PRODUCTION == 0`; the ONLY TU that instantiates vendored capture symbols)
- `src/engine/client/library/clientGraphics/src/shared/SwgVideoCapture.h`
- `src/engine/client/library/clientGraphics/include/public/clientGraphics/SwgVideoCapture.h` (1-line public re-include `#include "../../src/shared/SwgVideoCapture.h"`)

> Pitfall 5: stale `src/compile/win32/clientGraphics/{Debug,Release}/SwgVideoCapture.obj` exist and an incremental build may relink them. Clean/rebuild `clientGraphics` before the verification link.

---

### `Game.cpp` / `Game.h` (dead caller + manager, `#if 0`) — source-residue delete

**Analog:** `7a7da726d` dead-source removal. **Pure residue — never compiled, cannot affect the link.** Deleting is for D-02 source coherence only.

- `Game.cpp:157` — `#include "clientGraphics/SwgVideoCapture.h"` (remove)
- `Game.cpp:3014-3064` — `videoCaptureConfig/Start/Stop`, `VideoCaptureCallback`, `AudioCapture::SwgAudioCaptureManager::GetInstance()` ref — all inside `#if 0` (remove the block)
- `Game.h:257-261` — the 3 method decls inside `#if PRODUCTION == 0` (remove the block)

> RESEARCH precision note: `Game.h:255` (`handleCollectionShowServerFirstOptionChanged`) sits immediately ABOVE the block — do not over-delete. Block boundary is 257-261.

---

### `Audio.cpp` / `Audio.h` (dead audio-capture sibling, `#if 0`) — source-residue delete

**Analog:** `7a7da726d` dead-source removal. Pure residue (D-02a: zero live callers; live Miles `AIL_*` playback is untouched).

- `Audio.cpp:43-45` — `#include "clientAudio/SwgAudioCapture.h"` inside `#if 0` (the header **does not exist** — phantom include; just remove the dead include, do NOT try to delete a file)
- `Audio.cpp:5405-5470` — `getAudioCaptureConfig/startAudioCapture/stopAudioCapture` (Miles `"AudioCapture Filter"`), all `#if 0` (remove)
- `Audio.h:22-25` — `namespace AudioCapture { class ICallback; }` UNGUARDED fwd-decl (remove — it only types the dead `startAudioCapture` param)
- `Audio.h:321-327` — 3 decls inside `#if PRODUCTION == 0` (remove)

---

### `SwgCuiCommandParserScene.cpp` (dead console-command surface, `#if 0`) — source-residue delete

**Analog:** `7a7da726d` dead-source removal. CONTEXT.md mislabeled these as "live" — RESEARCH confirms all `#if 0`.

**Path correction (RESEARCH):** `src/game/client/library/swgClientUserInterface/...` (`library`, NOT `application` as CONTEXT.md wrote).
- `:265-269` — command-name consts (`#if 0`)
- `:532-539` — help-table entries (`#if 0` + already `/* */`)
- `:4895-4926` — dispatch handlers (`#if 0`)

---

### `external/3rd/library/videocapture/` (vendored SDK tree) — DELETE DIR (D-03)

**Analog:** `adda94729` (stationapi dir-delete) + `7a7da726d` (trackIR dir-delete). Exact shape: `git rm -r` the whole tree.

Subdirs confirmed present (delete all 9): `AudioCapture`, `CaptureCommon`, `Docs`, `ImageCapture`, `PICTools`, `Smart`, `SoeUtil`, `VideoCapture`, `ZlibUtil`.

> Safe per RESEARCH A3: after the `SwgClient.vcxproj` lib-path purge + editor/`.rsp` purges, nothing references this tree. The vendored `.lib`/`picn20*.lib` providers vanish with it.

---

## Shared Patterns

### Pattern A — Inline `.vcxproj` is authoritative; `.rsp` is vestigial
**Source precedent:** `7a7da726d` (had to edit BOTH `clientGame/includePaths.rsp` AND inline `<AdditionalIncludeDirectories>`; only the inline one affected the build) + Phase 12 memory `project_decruft_removal_build_graph_gotchas`.
**Apply to:** All build-wiring targets.
**Rule:** Edit inline `<AdditionalDependencies>`/`<AdditionalLibraryDirectories>`/`<AdditionalIncludeDirectories>`/`<ClCompile>`/`<ClInclude>` to satisfy the BUILD gate (D-06). Edit `.rsp` to satisfy the GREP gate (D-04) only. `SwgClient.vcxproj` references no `.rsp`.

### Pattern B — `/FORCE` false-pass guard: grep link output, not exit code
**Source precedent:** `7a7da726d` commit body ("link only returned exit 0 because the project links under /FORCE, which downgrades unresolved externals to warnings"). Confirmed: `SwgClient.vcxproj` has `<ForceFileOutput>Enabled</ForceFileOutput>` (line 113) + `/VERBOSE` (line 110, `<AdditionalOptions>`).
**Apply to:** D-06 verification of `SwgClient.vcxproj` (Debug + Release, recommend Optimized too).
**Rule:** Build with `/VERBOSE` link output captured; grep for `unresolved external symbol` → count must be **0**. MSBuild exit 0 is NOT proof.

### Pattern C — Atomic caller+provider removal (closed-chain delete, not stub)
**Source precedent (inverted):** `7a7da726d` STUBBED ClientHeadTracking because its 3 callers STAYED → excluding the `.cpp` orphaned them (LNK2019). Here D-01 deletes the whole chain, so the stub workaround is unnecessary.
**Apply to:** The atomic unit — `CuiIoWin.cpp` (live caller) + `SwgVideoCapture.*` (wrapper/provider) + `SwgClient.vcxproj` lib tokens must land in the SAME change.
**Rule:** Edit order (RESEARCH steps 1-3): (1) remove `CuiIoWin.cpp` call+include, (2) drop wrapper from `clientGraphics.vcxproj` + include path, (3) drop lib tokens+paths from `SwgClient.vcxproj`. Then no link state ever has a reference without a provider.

### Pattern D — Grep gate catches what the build gate can't (editor `.vcxproj` + `.rsp`)
**Source precedent:** `1d6b80242` — the verifier caught 4 editor `.vcxproj` carrying dangling refs that the `.sln`/`.rsp`-scoped greps missed; they were inert to the build but failed the "zero refs" gate.
**Apply to:** The 10 non-SwgClient `.vcxproj` (SwgGodClient, Viewer, TextureBuilder, AnimationEditor, ClientEffectEditor, LightningEditor, ParticleEditor, SwooshEditor, TerrainEditor, NpcEditor) + all 14 `.rsp`.
**Rule:** These are pre-broken on Qt and not in the SwgClient build path (A2) — purging them cannot break the gated build, but D-04 requires it. Purge per-file (RESEARCH note: `SwgClient/libraryPaths.rsp:10` has an `AudioCapture/lib/win32` path the editor `libraryPaths.rsp` files omit — line numbers differ; don't assume identical).

### Do-NOT-touch allow-list guard (Pitfall 4)
**Source precedent:** `adda94729` deviation discipline (touched only the orphaned token).
**Apply to:** All `.vcxproj`/`.rsp` edits.
**Rule:** Remove ONLY the 15 capture-family lib tokens (section D) + 8 `videocapture\...\lib\win32` paths + `videocapture` include segments. NEVER touch `binkw32.lib`/`bink\lib\win32` (Bink, OUT), `vivox*` (Phase 14), `zlib.lib`/`dpvs.lib` (live), `crypto.lib`/`989crypt` (auth, live).

---

## Reference Inventory (planner edit checklist)

### Build-path `.vcxproj` (4 — the build gate)
| File | Edit |
|------|------|
| `src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj` | 15 lib tokens + 8 lib paths × 3 configs (`<Link>` @103/158/204, `<AdditionalLibraryDirectories>` @105/...) |
| `src/engine/client/library/clientGraphics/build/win32/clientGraphics.vcxproj` | `<ClCompile>` @320, `<ClInclude>` @436, `videocapture` include seg @73/111/148 |
| `src/engine/client/library/clientGame/build/win32/clientGame.vcxproj` | `videocapture` include seg @73/111/148 |
| `src/engine/client/library/clientAudio/build/win32/clientAudio.vcxproj` | `videocapture` include seg @73/111/148 |

### Non-build-path `.vcxproj` (10 — grep gate only, D-04)
`SwgGodClient`, `Viewer`, `TextureBuilder`, `AnimationEditor`, `ClientEffectEditor`, `LightningEditor`, `ParticleEditor`, `SwooshEditor`, `TerrainEditor`, `NpcEditor` (all under `src/{game,engine}/client/application/.../build/win32/`).

### `.rsp` (14 — grep gate only, D-04; all vestigial)
- `libraries_{d,r,o}.rsp` + `libraryPaths.rsp` for: SwgClient, SwgGodClient, Viewer, AnimationEditor, ClientEffectEditor, LightningEditor, ParticleEditor, SwooshEditor, TerrainEditor, NpcEditor (TextureBuilder has only `libraries_r.rsp` + `libraryPaths.rsp`)
- `includePaths.rsp` for: clientGraphics, clientGame, clientAudio

### Source files (9 — code edits)
See per-file Pattern Assignments above. Live: `CuiIoWin.cpp`, `SwgVideoCapture.cpp/.h` (+public). Dead `#if 0`: `Game.cpp/.h`, `Audio.cpp/.h`, `SwgCuiCommandParserScene.cpp`.

### Vendored tree (1 — dir delete, D-03)
`src/external/3rd/library/videocapture/` (9 subdirs).

---

## No Analog Found

None. Every removal target maps to a verified Phase-12 DECRUFT precedent commit on this branch. The only NEW shape (no exact Phase-12 twin) is dropping a `<ClCompile>`/`<ClInclude>` item outright rather than stubbing the `.cpp` — but this is the deliberate D-01 inversion of the `7a7da726d` stub pattern (callers are deleted, not kept), so the planner should treat the stub-vs-delete choice as already decided, not re-derive it.

---

## Metadata

**Analog search scope:** Phase 12 commit range `f4d06d9ea..8e2179d68` (DECRUFT-01/-02/-03); live `.vcxproj`/`.rsp`/source tree at git `02f36d677`.
**Files scanned:** 4 build-path `.vcxproj` (line-level), 14 `.vcxproj` + 14 `.rsp` (grep inventory), `CuiIoWin.cpp`, vendored tree listing; 4 Phase-12 commits read via `git show`.
**Pattern extraction date:** 2026-05-25
**Cross-checked against:** RESEARCH.md (authoritative on build graph) — all line numbers and the `.rsp`/`.vcxproj` set counts independently re-verified (14 `.vcxproj`, 14 `.rsp`, line 103/105 token+path clusters, `CuiIoWin.cpp:19/209`).
