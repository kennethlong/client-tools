# CONSULT-74 SYNTHESIS — combining two independent x64+D3D11 SWG client forks

**Status: DRAFT for adversarial review.** Four consultants ran non-overlapping angles against both
trees. Evidence pack (locked facts): [CONSULT-74-EVIDENCE-two-fork-comparison.md](CONSULT-74-EVIDENCE-two-fork-comparison.md).
Raw outputs: `CONSULT-74-codex.out` (renderer architecture), `CONSULT-74-cursor.out` (x64/build).
Audio/feature and bug-fix-delta consults returned in-session; their findings are carried below.

Repo A = `D:\Code\swg-client-v2` (Kenny). Repo B = `D:\Code\Galaxies-Reborn\client-tools` (Sais).
Common ancestor: `949451032` (upstream PR #18, 2026-05-13), verified an ancestor of both.

---

## 0. Corrections to the evidence pack (found during the round)

1. **VR does not exist.** `x64-dx11-vanilla-vr` is git-identical to `x64-dx11-vanilla` (same commit
   `4ce0386af`); `x64-dx11-wicked-vr` is a strict ancestor of the wicked branch with zero unique
   commits. No commit in 288 mentions VR/OpenXR/Oculus/SteamVR/OpenVR. The branch names promise a
   feature that is not implemented.
2. **`econ-sim` contains no economy code.** 94 of 96 commits are `x64-dx11-vanilla` verbatim. Unique
   content is `TopDownCamera` + a static-object-layer config flag. The author's own tip commit marks
   TopDownCamera **unverified** (access violation before logging initialises, reproducing on a
   pre-change build).
3. **B's editor tools are not broadly x64-build-ready.** Only the **God client** is validated.
   `AnimationEditor` et al. have x64 project metadata but still invoke the Win32 `qt\3.3.4\bin\moc`
   for x64 configurations — not wired to `deps/qt3-win64-src`.
4. **B's D3D11 renderer is not on his `master`** — it lives on `x64-dx11-vanilla` and descendants.

---

## 1. Q1 — Did B do things fundamentally differently?

Yes, in four ways that matter. Everything else is convergent (both were Claude-assisted on the same
codebase and repeatedly reached the same design independently).

| Axis | Repo A | Repo B |
| --- | --- | --- |
| **Net direction** | **Subtractive** — +54k/−449k. Legacy-SDK removal, dead-library decruft | **Additive** — +412k/−9k, dominated by vendored JUCE; +75k excluding it |
| **Legacy D3DX / DirectX SDK** | Removed from the shipping path; shipped binaries import no `d3dx9_*` on either platform | **Retained** — DirectX SDK (June 2010) is a documented build prerequisite; `d3dx9_43.dll` a runtime requirement |
| **Audio** | Miles retained; version matrix resolved by A/B conviction (retail 7.2a Win32 / 9.3v x64) | **Miles replaced** by a 1,556-line Miles-API shim over vendored **JUCE 8**; backend is a build flag; x64 Miles is a link-only stub |
| **D3D9 assembly shader corpus** | Runtime fallback generation + **17** hand-authored `//hlsl` overrides | **Offline translation**: `scripts/asm2hlsl/` (Python, fxc-verified) producing **239** converted programs; the renderer refuses assembly by name |
| **Shader constant convention** | Matrices **transposed** at upload | **Never** transposed; relies on `ENABLE_BACKWARDS_COMPATIBILITY` placing `register(cN)` at `$Globals` offset 16N |
| **x64 memory manager** | Pool **root-caused and kept** (free-block undersize + split threshold) | Pool **bypassed** on x64 (`::malloc`/`::realloc`/`::free`) |
| **History** | 1,020 commits, per-fix, with live-verification records | 288 commits across 14 clean feature branches |
| **Build** | Documented manual MSBuild, 5-target convention | `Directory.Build.props/.targets` + matrix scripts with **PE machine-type verification** |

Also B-only, with no A equivalent: **SDL3 multi-controller input**, **Entertainer MIDI performance
modes**, **cascaded shadow maps**, a **headless `gl00` renderer** (`rasterMajor=0`), **Qt3-win64**
for the God client, and prerequisite-check/build-matrix scripting.

A-only, with no B equivalent: the **engine-advertise editor contract** (v33/160) and its live world
editor, the **performance catalogue** (all kill-switched), the **diagnosability layer** (stall
watchdog + audio-safe stack sampler + Release-surviving probes), and the **data/override payload**.

## 2. Q2 — Where they diverge, which is better?

**The renderer: B, on architecture.** Codex traced six subsystems in both and rates B more complete
on four — device/context (B splits `Device`/`SwapChain`, D3D11.1 throughout, tearing/occlusion, and
fires device-lost/restored callbacks on resize, where A treats `DEVICE_REMOVED` as a `FATAL` with no
recovery by design decision D-13), state management (a separate descriptor-keyed state-object cache
plus a pointer-shadow bind cache), textures (a formal native/storage/DXGI format model, converter
subsystem, initialized-subresource tracking, SRV dimension validation), and constant-buffer ABI
clarity. A leads on two: runtime fallback for legacy assembly assets, and broader `NO_OVERWRITE`
ring coverage.

**⚠ The renderer verdict has an untested edge.** A's parity was established by RenderDoc
capture-and-diff against D3D9 across a long, recorded bug list **including JTL/space content**.
B has **no space/JTL content at all** — so several of A's hardest parity bugs (additive-UI
premultiply, nebula, texren) correspond to content B's renderer has never drawn. Compile-clean
(B verifies 239 conversions with fxc) and visually-correct are different claims. **No runtime A/B
was performed by anyone this round.**

**Engine correctness: A, decisively.** Of A's engine crash/correctness fixes checked at B's code
sites, **14+ are still defective in B** — including the erase-then-`++` iterator UB, all four
cross-thread container corruptions, the unlocked `SearchCache` map, fail-loud zlib, all six dPVS
portal fixes, the `CuiMediator` re-entrancy guard, and the HUD/reticle/workspace null guards.

**But A is not clean.** B found real defects in A that A's own testing was never going to surface:

- **Transform send-rate throttle** (B `097ff875b`; A `ClientController.cpp:310` stock, and A's
  `frameRateLimit` defaults to 0). At 180–240 fps the client sends transforms every 4–5 ms; the
  server's `maxDistSqr = sqr(maxSpeed × timeDiffMs/1000)` tolerance is exceeded by ordinary float
  jitter and `handleInvalidMove` warps the player back. **Symptom: your own movement and animation
  stall while NPCs keep moving, at full frame rate.** A stock defect that only appears on modern
  hardware. (Adopt the throttle at the `sendTransform` chokepoint — **not** B's fixed-rate
  simulation step, which he shipped and then reverted because `IoWinManager::processEvents` both
  dispatches input and enqueues `IOET_Update`.)
- **Packed-map wire counts, the three explicit specializations** (B `e0d46921e`). A pinned the
  *generic* template to `uint32_t` under BITS-03/D-07 but missed
  `UnicodeAutoDeltaPackedMap.cpp`, `NetworkIdAutoDeltaPackedMap.cpp`, `AutoDeltaNetworkIdPackedMap.h`.
  On x64 these `put` a 4-byte count then an 8-byte `size_t`, and `unpack` reads 16 bytes for 12
  written — every subsequent key/value misaligns. **Client-reachable via
  `ShipObject.h:548 m_componentNames`.** `PlayerQuestData.cpp:261-262,316` is still `size_t` in
  **both** trees — a joint todo.
- **Colour write mask is bit-reversed** (B `287ab8f15`). A's `Direct3d11_StateCache.cpp:2329-2343`
  carries a comment asserting the engine mask is bitwise-identical to `D3D11_COLOR_WRITE_ENABLE_*`.
  It is not: engine is ARGB MSB-first, D3D11 is R=1/G=2/B=4/A=8. It survives only because the
  default `0xF` is symmetric — **any pass with a partial mask writes the wrong channels.**
- **`TextureFormatInfo` table is two rows short of `TF_Count`** (B `13c9abbba`). `Texture.def`
  declares `TF_Count = 17` in both trees; A's table has 15 initialisers. `TF_ABGR_16F`/`TF_ABGR_32F`
  are zero-filled with a **null `name`**. Renderer-agnostic — reachable from gl05 too.
- **`Gl_api` layout verification** (B `16b0057bf`). `Gl_api` has three binary layouts driven by
  `_DEBUG`/`PRODUCTION` conditionals and nothing checks exe/DLL agreement. This is the
  machine-checkable form of A's documented "shared-header ABI cascade trap" — A currently detects it
  by comparing file mtimes. A also calls `setDynamicIndexBufferSize` through a permanently-null slot.
- **Short-buffer `ReadException` guards in `Archive.h`** — A has none; a truncated server packet
  reads past the buffer.

**Audio: split, and the split is a licensing decision more than a technical one.** B's JUCE shim is
comprehensive, not a stub — 78 `AIL_*` entry points against the 75 A's engine calls, with genuinely
implemented 3D positional audio (power-law distance falloff, dot-product panning, Doppler,
obstruction as a one-pole lowpass). Two real gaps, both performance-shape: `AIL_open_stream` decodes
the **whole file to RAM** before playback (Miles streams progressively — matters for multi-minute
music), and the 64-voice mixer budget is set as a preference but never enforced, so every playing
sample mixes with no voice-stealing. Against that, A's Miles path is the shipped-retail behaviour
and needs no new licence — but *does* require a licensed Miles runtime that cannot ship in-repo.
**JUCE 8 is AGPLv3-or-commercial, and AGPL's network-interaction clause is triggered by networked
use** — a materially different obligation profile for an MMO client. B documents this honestly and
leaves the choice to the distributor. XAudio2 (no licence cost) and DaisySP (MIT) were both explored
and abandoned; JUCE's win looks like an infrastructure bet, since it was the only one that also
brought the MIDI stack Entertainer Reborn depends on.

**x64 platform: mixed.** A leads on `intptr_t` `PathSearch` marks, `std::hash<uintptr_t>` shader
sort keys (B still truncates a pointer to `int`), on-disk-layout `static_assert`s, `/we4311 /we4312`
as errors, complete boot-path assembly removal (B's `Transform.cpp:281-286` still has
`__declspec(naked)` `__asm`), and the MemoryManager root-cause. B leads on the packed-map
specializations, centralized 8-byte archive overloads, `RtlCaptureContext` for all bitness, and PE
machine-type build verification.

## 3. Q3 — Bug-fix delta

**The asymmetry is structural, not a quality judgement.** A's fixes cluster in *concurrency, object
lifetime, and visibility* — long-tail engine defects surfaced by an external editor and a stall
watchdog exercising paths no player takes. B's cluster in *data tolerance, x64 ABI, D3D9→D3D11
translation rules, and his own new renderer* — surfaced by running against a different, less
well-formed asset set and by building a renderer from the assembly corpus up.

- **A→B: ~15 fixes**, headed by the WorldSnapshot delete drain (which B needs *before* shipping his
  detail-slider query-range increase, since that multiplies the leak), all six dPVS portal fixes, the
  iterator UB, the four cross-thread container corruptions, and `D3DCREATE_FPU_PRESERVE` for his
  Win32 configs.
- **B→A: ~17 fixes**, headed by the six listed in §2.
- **Convergent (no work):** both independently fixed `TravelManager` wrong-container `end()`,
  `WeatherManager` reversed `std::find`, two Watcher-vs-raw-pointer defects, an `_snprintf` arg
  count, `SafeCast.h`, the Miles `UINTa` callback ABI, and the dpvs toolchain.

**Not recommended for adoption from B** (workarounds for his data set, not stock fixes): the
`Direct3d9_LightManager` 0.3 ambient floor and synthesized tangent colours; runtime source-patching
of shaders hard-coded to his TRE set; the `Camera.cpp` hFov change (an ultrawide *feature* that
silently alters FOV for every player); the ~50-site `FATAL → DEBUG_WARNING` sweep and
`DataTableManager::getTable` returning an empty sentinel instead of NULL (converts fail-fast into
fail-silent across the data layer); `canUse() → true`; and the removal of `entertainerCaptchaPercent`
from the wire, which breaks the stock-protocol constraint.

## 4. 🚩 The four real conflicts

1. **`MemoryManager` on x64 — mutually exclusive.** A root-caused it (min-size block rounds to
   48 < 64, so `addToFreeList` writes 8 bytes past the block) and **kept the pool**. B **bypassed**
   it to `::malloc`, conceding in-comment that he never found the actual bug. B's bypass loses leak
   tracking, guard bands, owner attribution, the `maxMbytes` cap, and A's OOM address-space reserve
   that keeps crashes writing a non-zero minidump. **Take A's.**
2. **`FreeChaseCamera.cpp` — both rewrote it from different diagnoses.** A: door-snap seam-graze +
   camera pull-in + interior zoom cap (A's interior see-through root cause turned out to be dPVS +
   FPU_PRESERVE, not camera geometry). B: collision backoff + a terrain-height ground-clearance zoom
   clamp + a `DEFH` float-count heuristic. **B's `DEFH` heuristic is dangerous** — it exists because
   *his* `freechasecamera.iff` reports `floats[1] = 2.1e8`, a data-set property; merged into a tree
   with a well-formed asset it silently discards the authored shoulder offset. **A's work is the
   base; adopt only B's optional-`INZM` read; gate the rest off by default.**
3. **`PortalProperty::cellLoaded` parenting.** B flipped `attachToObject_p(..., false)` → `true`,
   arguing cells are never added to the world in the multiplayer `SceneCreate` ordering. A's client
   demonstrably renders and collides interiors correctly with `false`. **This sits directly upstream
   of all six of A's dPVS fixes and A's camera-cell derivation — do not merge without re-running A's
   portal see-through repro.**
4. **cbuffer matrix convention — architectural, not a bug.** A transposes; B must never transpose
   and must never pass `PACK_MATRIX_ROW_MAJOR`. Each is self-consistent with its own shader corpus.
   **Neither half can be cross-merged; whichever renderer is chosen brings its convention whole.**
   Note B's shadow cascades *do* transpose deliberately (they build their own matrices), so porting
   cascades onto A's renderer needs that examined, not copied.

## 5. Q4 — Which repo is the merge base?

**Recommendation: Repo A is the base; Repo B's renderer and features merge into it.** Reasoning:

- **The base's changes come for free; the other side's get replayed.** A is diffuse — 3,548 changed
  files, fixes spread across the engine, and −449k of deletions through `external/3rd`. Replaying
  deletions and cross-cutting concurrency fixes into B is materially more error-prone than carrying
  them. B is unusually **modular** — a self-contained renderer plugin directory, vendored deps in
  their own trees, features isolated on branches. That is what merges cleanly *into* something else.
- **The renderer choice does not force the base choice.** B's renderer is a self-contained plugin
  directory implementing the same `Gl_api`; adopting it is closer to a directory swap plus its
  shader corpus and constant convention than to a merge. (Caveat: he added
  `clientGraphics/ShaderConstantRegisters.h`, so the coupling is not zero — verify before assuming.)
- **A tracks upstream live.** A's `master` is an active SWG-Source integration branch and has
  already merged upstream PRs; its diff is maintained against their moving master. B is a clean
  snapshot from the shared May 13 base that has not tracked upstream since.
- **Measured merge cost is small either way.** Trial merge of `sais/x64-dx11-vanilla` into A's HEAD:
  **259 conflicted files** — of which 131 are `.vcxproj` (mechanical x64 platform blocks), 45 are the
  two renderers colliding (a *choice*, not a merge), and 12 are modify/delete on dead features A
  deleted and B only x64-fixed (Vivox, libMozilla, videocapture, lcdui, SwgClientSetup — upstream
  already lists these as deprecated). **83 files of genuine hand-merge work.** Taking B's shadows
  branch instead costs exactly **one** additional conflicted file.
- **Counter-argument, stated fairly:** the bug-fix consult found that merging A's engine-defect set
  *into B* is close to a clean apply (none of the 14 sites are files B has touched, bar the three
  conflicts). So B-as-base is not unworkable — it is just a larger replay on the other axis.

**Proposed order** (each stage independently bootable, per A's boot-gate discipline):

1. **Zero-risk adoptions, no renderer decision needed.** SDL3 input; the transform send-rate
   throttle; packed-map specializations; `TextureFormatInfo` rows; colour-write-mask reversal;
   `Gl_api` layout verification; `Archive.h` short-buffer guards; PE machine-type build verification.
   Send A's engine crash-fix set to B in parallel so both clients improve immediately.
2. **Decide the renderer on measurement, not architecture.** Build both, run the same scenes on the
   same machine, RenderDoc-diff, and specifically exercise the content B has never rendered (JTL
   space, nebula, additive UI). This is the one question consensus cannot settle.
3. **Merge the losing renderer's unique work forward** — A's `NO_OVERWRITE` ring and legacy-asset
   fallback, or B's cascades, headless `gl00`, and converted shader corpus.
4. **Resolve the four conflicts** in §4 with the stated recommendations.
5. **Defer the licensing-bound choices** — JUCE-vs-Miles and the Qt3 tools — until SWG-Source
   leadership rules, since both change what a distributor must comply with.

## 6. Open questions

- **Who is the canonical destination?** SWG-Source's own master is the real endpoint; it may be that
  neither fork is "the base" and both merge upstream in waves.
- **Has B's client been verified booting and playing**, and at what bar (both platforms, both
  renderers)?
- **Does A's JTL ship-component path actually fire?** The packed-map misalignment should be loud if
  exercised. Empirical test: fly a JTL ship on x64 and inspect component names.
- **Does A's cbuffer ring bound its copy by a dirty extent under `MAP_WRITE_DISCARD`?** B found that
  a partial upload under DISCARD destroys every constant above the dirty span.
- **Miles SDK licensing** for A's `external/3rd` additions remains open from the upstream-offering
  plan.
