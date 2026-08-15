# CONSULT-74 — Evidence pack: two independent x64+D3D11 SWG client forks

**Purpose.** Two developers independently modernized the SWG-Source client. The SWG-Source team is
now coordinating a combination of the two efforts. This is a **coordinated merge planning exercise,
not a competition** — the goal is the best possible combined client.

**Treat everything in §1–§3 as LOCKED, measured facts.** Do not re-derive them. Answer only the
question assigned to you in your own task file. Do not assume either repo is the intended winner;
several axes are expected to split.

---

## 1. Shared lineage (measured)

Both forks descend from the **same upstream commit**: `949451032` — "Merge pull request #18 from
SWG-Source/feature/configurable-entertainer-captcha", 2026-05-13. There is no fork-point ambiguity.

| | **Repo A** | **Repo B** |
| --- | --- | --- |
| Path | `D:\Code\swg-client-v2` | `D:\Code\Galaxies-Reborn\swg-source-x64-dx11` |
| Author | Kenny | "Sais" (github@swgsais.com) |
| History | **1,020 commits** (381 touch `src/`), granular, per-fix, with verification records | **1 squashed commit** `3ab047315` "Add x64 and Direct3D 11 client support", 2026-08-13 |
| `src/` diff vs base | **+53,903 / −448,771** over 2,722 files | **+411,906 / −8,950** over 1,685 files |
| `src/` diff excluding vendored JUCE | (n/a — A vendors no new libs) | **+74,906 / −8,950** over 811 files |
| Net direction | **Subtractive** — the −449k is legacy-SDK removal + dead-library decruft | **Additive** — the +412k is dominated by vendored JUCE 8.0.14 |
| Outside `src/` | `docs/`, `.planning/`, `tools/tre-compare/` | `deps/qt3-win64-src` (**6,529 files**), `deps/x64`, `deps/win32`, `mss64-stub/`, `scripts/`, `Directory.Build.props`/`.targets`, `.clang-format` |

Both were substantially Claude-assisted. Expect convergent design in places and genuine divergence
in others.

## 2. Repo A — what it contains (measured/verified in-tree)

- **Toolchain:** VS 2026 BuildTools `v145`, `stdcpp20`, pure MSBuild, same solution layout.
- **D3DX/legacy SDK:** removed from the shipping path. Shader compile moved to `D3DCompile`
  (`d3dcompiler_47`) on **both** renderers; the staged `gl05_r.dll` / `SwgClient_r.exe` binaries
  import **no `d3dx9_*.dll` on either platform** (verified against the built binaries). The Win32
  `Direct3d9.vcxproj` still lists an unreferenced `d3dx9.lib`; x64 link blocks drop it outright.
- **x64:** full client + all four renderer plugins; `/LARGEADDRESSAWARE` on the 32-bit Debug config;
  `dpvs` static on x64 (a hand-staged DLL on Win32).
- **Renderer:** `gl11` D3D11 plugin, **57 new files**, selected by stock `rasterMajor=11`.
- **Audio:** Miles retained. **Win32 = vendored retail 7.2a; x64 = 9.3v** (matrix resolved by A/B
  conviction; 9.3b convicted as a vendor DLL defect, 7.2e identified as a dud). Redists are
  license-bound and NOT in git.
- **Performance catalogue** (all config-gated with kill switches): phased/budgeted snapshot load,
  per-frame create-drain time budget, restored stream-out delete drain, shader-cache RAM preload
  (both renderers), string-table whole-file buffering, non-blocking title-music fade, loading-screen
  gate on terrain warm-up, search-path negative cache, D3D11 `NO_OVERWRITE` cbuffer ring.
- **Crash/correctness fixes:** erase-then-increment iterator UB; cross-thread container heap
  corruption (multiple sites); unlocked `SearchCache` map; `safe_cast` Release-soundness class;
  six dPVS portal see-through fixes; two use-after-frees; NV driver thread race; D3D9 32-bit
  `D3DCREATE_FPU_PRESERVE`; camera/collision (door-snap, interior zoom cap).
- **Diagnosability:** stall watchdog + budgeted dumps; audio-safe main-thread stack sampler;
  Release-surviving cfg-armed probe families.
- **Editor surface:** versioned engine-advertise contract (**v33, 160 names**) driving an external
  in-client world editor (`.ws` / `.ilf` editing, ray-pick, scene control).
- **Full catalogue:** `D:\Code\swg-client-v2\docs\FORK-CHANGES.md`. Detail: `.planning/handoff/`.

## 3. Repo B — what it contains (measured/verified in-tree)

- **Audio — the biggest philosophical divergence.** JUCE 8.0.14 vendored into
  `src/external/3rd/library/`. `clientAudio/src/win32/JuceMiles.cpp` (**1,556 lines**) is a
  **Miles-API-shaped shim implemented over JUCE**, so engine call sites are unchanged. Audio
  backend is a **build-time option** (`-AudioBackend Juce|Miles`, JUCE default). `mss64-stub/`
  contains stub `Mss32`/`mss64` DLLs+libs and a libMozilla stub — his README states the x64 Miles
  stubs "satisfy the build but are not a full audio implementation." JUCE brings MIDI device
  support (the user observed playing along with in-game music via a MIDI keyboard).
  **Licensing: JUCE 8 is AGPLv3-or-commercial** — his README makes distributors choose.
- **Input:** **SDL3 multi-controller** support (`clientDirectInput/.../SdlJoystickInput.cpp`),
  enabled on both architectures, with a keymap-compatibility guide at `docs/inputreborn.md`.
  Repo A has no equivalent.
- **Tooling/editors:** `deps/qt3-win64-src` (6,529 files) — Qt3 ported/vendored for win64 — and
  **~576 modified files** including nearly every editor-tool `.vcxproj` (AnimationEditor,
  ParticleEditor, TerrainEditor, MayaExporter, ShaderBuilder, …). Repo A's editor tools are
  pre-broken on the Qt `.ui` custom-build step (`MSB8066`), as they are upstream.
- **Headless renderer:** `rasterMajor=0` selects a `gl00` headless stub graphics driver
  (+ `VertexBufferDescriptorCache`). Repo A has no headless path.
- **Renderer:** D3D11 plugin at the **same path** as A's (`src/engine/client/application/Direct3d11`),
  **88 new files**. Links `d3dcompiler.lib`. Keeps `d3dx9.lib` on gl05; his README lists the
  **DirectX SDK (June 2010) as a build prerequisite**.
- **Build infrastructure:** `Directory.Build.props`/`.targets` (central MSBuild config),
  `scripts/Build-Client.ps1` (full matrix: Win32/x64 × DX9/DX11 × Juce/Miles, with **PE
  machine-type verification** of every produced binary) and `Test-ClientBuildPrerequisites.ps1`.
  Repo A builds by documented manual MSBuild invocation.
- **Code-style policy:** `.clang-format` as formatting authority; a stated `constexpr`-over-magic-
  literal policy; vendored deps keep upstream formatting.
- **Engine-shared changes of note:** new/split archive files across `sharedNetworkMessages`
  (`*Archive.cpp/.h`, `AuctionQueryTypes.h`, `ClientCentralMessagesTypes.h`,
  `AIDebuggingMessagesArchiveTypes.h`) plus a new `sharedUtility/PackedArchive.{h,cpp}`; new
  `clientGraphics/ShaderConstantRegisters.h` (public + shared copies) and reworked D3D9
  `*ConstantRegisters.h`; substantial `SwgCuiSkills` UI work (`SwgCuiSkillBoxData.h` alone is
  ~2,859 lines); Headless and MayaExporter source changes.
- He **edited the upstream `README.md`** in place (Repo A deliberately left it untouched to keep
  upstream syncs conflict-free).

## 3b. Repo B — FULL HISTORY IS AVAILABLE (use this, not the squash)

`D:\Code\Galaxies-Reborn\client-tools` is B's complete repo: **288 commits across 14 branches**.
The packaged `swg-source-x64-dx11` squash is a *curated subset* of it. **Read this repo** — the
commit messages carry the intent and provenance the squash destroyed.

| Branch (`origin/`) | Commits vs base | Tip date | What it is |
| --- | --- | --- | --- |
| `x64-dx9-vanilla` | 14 | 2026-07-27 | x64 on the existing D3D9 path; fixed-rate simulation step |
| `x64-dx9-vanilla-Xaudio2` | 4 | 2026-07-15 | XAudio2 audio backend |
| `x64-dx9-vanilla-daisysp` | 4 | 2026-07-15 | libDaisy/DaisySP audio backend |
| `x64-dx9-vanilla-juce` | 6 | 2026-07-16 | JUCE audio backend (the one that shipped in the package) |
| `x64-dx9-vanilla-inputreborn` | 4 | 2026-07-15 | SDL3 multi-controller input |
| `x64-dx9-vanilla-entertainerreborn` | 20 | 2026-07-16 | **Entertainer Reborn MIDI performance modes** |
| `x64-dx11-vanilla` | 94 | 2026-07-28 | the D3D11 renderer |
| `x64-dx11-vanilla-vr` | 94 | 2026-07-28 | VR variant of the above |
| `x64-dx11-graphics-reborn` | 132 | 2026-07-28 | graphics enhancement line (shadow volumes) |
| `x64-dx11-graphics-reborn-wicked` | 158 | 2026-08-01 | **cascaded shadow maps** + tunables |
| `x64-dx11-wicked-vr` | 155 | 2026-07-30 | cascaded shadows + VR |
| `econ-sim` | 96 | 2026-08-01 | economy simulation + `TopDownCamera` (point-and-click rig) |
| `reborn-master` | 1 | 2026-07-11 | self-contained x64 DX9 client builds |
| `master` | 1 | 2026-07-27 | fixed-rate simulation step |

**Three distinct layers fall out of this and should not be conflated:**

- **L1 — platform/core, where the two forks OVERLAP and a choice must be made:** x64 port, D3D11
  renderer, audio backend, toolchain/build, D3DX-or-not.
- **L2 — additive features B has and A does not, mostly orthogonal (adoption decisions, not
  conflicts):** SDL3 multi-controller, entertainer MIDI, cascaded shadows, VR, headless renderer,
  Qt3-win64 tools, econ-sim, TopDownCamera, build-matrix scripting.
- **L3 — additive work A has and B does not:** engine-advertise editor contract, performance
  catalogue, engine crash-fix set, D3DX removal, diagnostics layer, data/override payload.

## 4. Known open items (do not treat as settled)
- Neither tree's runtime behavior has been A/B tested against the other. No claim about which
  renders or performs better is currently evidence-backed.
- Repo A's data/override layer (94 tracked files) and Repo B's `deps/` vendoring have not been
  compared for conflict.

## 5. The four questions the combination must answer

1. Did B do things **fundamentally differently** from A (architecture, not detail)?
2. Where they diverge, **which implementation is better**, and on what evidence?
3. **Bug-fix delta both directions** — what did each fix that the other did not?
4. If combined, **which repo is the merge base**, and what is the merge order?
