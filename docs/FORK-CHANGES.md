# What this fork changes in the SWG Source client

This repository is a fork of [`swg-source/client-tools`](https://github.com/swg-source/client-tools).
`README.md` in the repo root is deliberately left as **upstream's** file, unmodified, so that
periodic syncs from SWG-Source merge cleanly — this document is the fork's change catalogue
instead.

`master` here is a **live upstream-integration branch**: it syncs *from* SWG-Source continuously
(and has merged at least one PR back the other way), so the diff below is maintained against
their moving master rather than a stale fork point.

**Scope:** client-only. No server-side changes are required or implied; everything here was
developed and verified against a live stock server, speaking stock protocol.

---

## At a glance — the change set, measured

Measured `479d35df3` (v2 base, 2026-05-09) → `47797bfd8` (2026-08-12), paths under `src/`:

| Metric | Value |
| --- | --- |
| Files changed | 2,722 |
| Lines | **+53,903 / −448,771** |
| Engine/code commits | 381 (of 1,020 total; the remainder are planning/docs) |
| New files that are the D3D11 renderer | 57 |

The −449k concentrates in `external/3rd/library` — legacy-SDK removal, dead-library decruft, and
a Miles re-vendor. **The tree got substantially smaller and more auditable, not bigger.** The
single largest addition is a whole new renderer.

---

## 1. Build and run — what is different

| | Upstream | Here |
| --- | --- | --- |
| Toolchain | Visual Studio 2013 (`v120`) | **VS 2026 BuildTools (`v145`), `stdcpp20`** for every client/renderer project (the leftover `v120` is on server-side aggregate projects that aren't built in this tree) |
| Build system | MSBuild (`src/build/win32/swg.sln`) | Same solution, same layout — **no CMake, no build-system divergence** |
| Platform | Win32 only | **Win32 *and* x64**, both built and staged side-by-side |
| Renderers | `gl05` / `gl06` / `gl07` (D3D9) | Same three, plus **`gl11` (Direct3D 11)** |
| Renderer selection | `rasterMajor` in the active `.cfg` | Unchanged mechanism — `5`/`6`/`7` = D3D9 plugins, **`11` = D3D11** (`9` is not a value; DX9 is `5`) |
| Runtime deps | legacy DirectX SDK redist | **Shipped binaries import no `d3dx9_*.dll` on either platform** — no DirectX-web-setup step for players |

Canonical 5-target build: `Direct3d11;Direct3d9;Direct3d9_ffp;Direct3d9_vsps;SwgClient`. Postbuild
stages `gl0X_*.dll` + `SwgClient_*.exe` into **`stage/` (Win32)** or **`stage-x64/` (x64)**.
`SwgClient_d.exe` reads `client_d.cfg`; `SwgClient_r.exe` reads `client.cfg`.

Build/run/diagnostic operating detail lives in [`AGENTS.md`](../AGENTS.md).

---

## 2. Toolchain and dependency modernization

- **C++20 on a current MSVC toolset** across the client, the engine libraries, and all four
  renderer plugins. This completes and extends the MSVC/C++20 base the fork shares lineage with.
- **The legacy D3DX dependency is gone from the shipping path.** The HLSL compile path moved to
  `D3DCompile` (`d3dcompiler_47`) on *both* renderers; the last D3D9 D3DX call sites
  (`D3DXAssembleShader`, mesh optimize, surface save) were replaced with own-implementations or
  DirectXMath. The x64 link blocks drop `d3dx9.lib` outright; the Win32 project file still lists
  the import lib but nothing references it, so the shipped `gl05_r.dll` and `SwgClient_r.exe`
  carry **no `d3dx9_*` import on either platform** (verified against the staged binaries).
- **On-disk shader bytecode caches** for both renderers, so the compile cost is paid once rather
  than per session (and per cell — see §7 for the recompile leak that motivated it).
- **Dead-library decruft** through `external/3rd` — the bulk of the −449k. Note that `/FORCE` is
  in the SwgClient link line and downgrades unresolved externals to warnings, so decruft work is
  gated on grepping the build log for `unresolved external symbol` (must be 0), not on exit code.
- Matrix math moved off D3DX onto header-only **DirectXMath** (binary-layout-identical, x64-native).

## 3. 64-bit platform

A full x64 build of the client, the engine libraries, and all four renderer plugins — booting,
zoning, and playing.

**Claim discipline:** this is **not** the first 64-bit SWG client — Legends and Restoration both
field one. What is specific here: **x64 for the open SWG-Source lineage, D3DX-free**
(Restoration's x64 retains D3DX), on the same solution upstream already ships.

Why it matters downstream: the 32-bit 2 GB ceiling is the veteran-server crash class (long
sessions, dense cities, mod-heavy installs), and it is the platform constraint that blocks modern
middleware linking.

Included in the port work: pointer-width correctness sweeps, `_WIN64` guards, per-platform
staging, `dpvs` static-linked on x64 (it is a hand-staged DLL on Win32 — there is no postbuild for
it), Miles 9.3v on x64 vs the retail 7.2a runtime on Win32, and `/LARGEADDRESSAWARE` for the
32-bit Debug config (which previously locked up on world load). The x64 build is also
demonstrably not affected by the known 32-bit memory-leak class.

## 4. Direct3D 11 renderer — `gl11`

A native D3D11 backend (57 new files) at visual parity with the D3D9 path, selected with the stock
`rasterMajor=11`. The 2004 D3D9 path runs on modern Windows through the driver's compatibility
layer; a native D3D11 backend is the future-proofing story, and — unlike D3D9 — it is
**RenderDoc-capturable**, which turns render bugs into diagnosable ones.

Parity was reached item by item, each root-caused against a D3D9 reference capture:

- Fixed-function combiner cascade emulation; multi-texture stage→SRV **slot remap** driven by
  shader reflection.
- `dot3` / alpha-fade constant-register packing; sticky-light-snapshot gating; texren bake tint.
- Scene depth buffer (the world initially had none); gamma applied pre-Present.
- Additive-over-straight-alpha UI premultiply (the space-HUD cyan fills).
- Minimap round-trip, embed/resize (editor-host) path, alt-tab device loss.
- Skinning correctness: static index/vertex buffer CPU shadows after the collide path was found
  corrupting a live IB, and a cached raw D3D pointer converted to a `ComPtr` after a
  use-after-free.
- Baked render targets forced to BGRA8; cbuffer matrices transposed at upload.

**D3D9 remains fully supported and actively maintained** — this is an added backend, not a
replacement. Several fixes in §7 were found on the D3D9 side and are D3D9-only.

## 5. The D3D9 path — `gl05` / `gl06` / `gl07`

Not left to rot:

- `D3DXCompileShader` floating-point crash on modern toolchains — SEH-guarded, then the whole
  path migrated to `D3DCompile`.
- Missing `D3DCREATE_FPU_PRESERVE` — the cause of 32-bit see-through-wall collision failures.
- Shader **bytecode cache** + RAM preload, which also cured an 815 ms skeletal vertex-buffer
  lock stall (closed as cured on a second instrumented null run with skeletal content streaming).
- A standing, default-off dynamic buffer lock probe (`logDynamicBufferLockMs`) left in the DLL,
  re-armable without a rebuild.

## 6. Performance

Every item below was root-caused from instrumented evidence (stall watchdog + stack sampler, §9),
fixed, and live-verified on both platforms — and **every one carries a config kill switch**, so an
operator can revert a single behavior with one cfg line and no rebuild.

| Symptom | Fix |
| --- | --- |
| 3.1 s world-snapshot parse inside the `GroundScene` constructor | Phased, budgeted `loadStep()` |
| ~300 ms zone-in object-create bursts | Per-frame wall-time budget on the create drain (`createTimeBudgetMs`, default 6 ms) |
| Unbounded snapshot memory growth | **The stream-out delete drain restored** — it had been a no-op for the code's entire life (its distance guard read a value nothing recomputed), so nothing was ever streamed out. Live-verified at 1,800 objects drained with zero re-entry failures |
| 1.2 s char-select first draw; repeated 100–160 ms zone-in stalls | Shader-cache RAM preload, off-thread at install — the draw path was doing per-hit `fopen`/`fread` of `.cso` files (both renderers) |
| 500–800 ms localized-string-table stalls | Whole-file buffering instead of per-string streamed reads |
| 1 s blocking title-music crossfade at zone-in | Non-blocking fade; the next track defers on a new `isMusicPlaying()` query |
| ~390 ms of spec-map creates spilling past the loading screen | Loading-screen **gate** now waits for terrain warm-up completion |
| Repeated failed lookups through the search path | Search-path negative cache (note: loose-file drops mid-session then need a restart) |
| D3D11 constant-buffer driver renames | `NO_OVERWRITE` ring buffer — 865× reduction in renames |
| 32-bit zone-in memory growth | Shader recompile-per-cell leak fixed by the bytecode cache |

## 7. Crash and correctness fixes

Long-tail engine defects, most of them 20 years old, each with a recorded failure signature and
verification history:

- **Erase-then-increment map iterator UB** — the combat-kill crash.
- **Heap corruption from unguarded cross-thread containers** at terrain zone-in: shader-cache node
  list, crash-report information, `Shader::m_users`, then a follow-up audit that added a TreeFile
  snapshot, a Texture critical section, and `destroyShader` wiring.
- **Unlocked `std::map` in `SearchCache`** driving async "unknown shader template tag" crashes —
  leaf mutex plus fail-loud zlib.
- **`safe_cast` Release-soundness class**: a wrong-class template produces vtable garbage in
  Release, where the `dynamic_cast` check is compiled out. Narrowed with virtual discriminators at
  the create sites, plus a null-`dynamic_cast` guard.
- **dPVS portal see-through** — six root-caused fixes: dirty-node/frustum ordering, camera cell
  derived from its own position, meshless doors never closing portals, portal backface epsilon,
  derivation hysteresis, and (signature B) portals skipping the exact triangle-vs-AABB refinement
  in `splitInstance` that could drop a flat portal's instance from a child node.
- **Two use-after-frees** on the D3D11 side (cached IB pointer, collide path).
- **An NVIDIA driver thread race** at zone-in, mitigated with `PREVENT_INTERNAL_THREADING` (itself
  config-gated once it was measured to cost micro-stutter).
- **Camera/collision**: cantina door-snap (two stacked bugs — seam-graze rubberband and camera
  pull-in), interior zoom cap, D3D11 static-VB read-back returning zeros and thereby producing no
  collision at all.
- **JTL space**: an empty/magenta world traced to `gameFeatures` omitting the JTL SKU, plus an
  unwired loose-override layer behind a `buttonEnterSpace` FATAL.

## 8. Audio

The Miles version matrix was resolved by A/B conviction rather than guesswork, and the results are
documented so nobody re-walks the minefield:

- **Win32 runs the vendored retail 7.2a runtime; x64 runs 9.3v.** The 9.3b warning-storm was
  convicted as a vendor DLL defect (same x64 exe: 9.3b storms, 9.3v clean — engine exonerated),
  and the 7.2e SDK redist is a dud that fails MP3 ASI discovery. Always stage a full, hash-verified
  Miles set; mixed sets crash at boot.
- The Miles **stream-EOS callback never fires** in these runtimes — the engine polls `SMP_DONE`
  instead. This is structural, not a bug to re-fix.
- Never issue handle 0; `timeBeginPeriod(1)` is load-bearing; background-music duck handoff is a
  dedicated ramped fade rather than a volume step.
- A 20-year `getSampleTime` handle leak (the release was success-gated) fixed.
- Zone-in audio dips were traced to the stall watchdog's own dump writes, not the mixer — dumps
  are now budgeted (§9).

**Licensing note:** Miles redistributables are license-bound and are **not** in git. The version
matrix documents which runtimes are safe; each operator supplies their own.

## 9. Diagnosability

The instruments this codebase never had, all default-quiet and cfg-armed, all surviving into
Release builds:

- **Stall watchdog** (`stallWatchdogMs`) with a **dump budget** (`stallWatchdogMaxDumps`) — because
  `MiniDumpWriteDump` suspends *every* thread including the audio mixer for 0.2–1.3 s a dump.
- **Audio-safe stack sampler** (`stallWatchdogStackSamples`, default 100): suspends only the
  already-stalled main thread, snapshots context plus raw stack bytes in microseconds, resumes,
  and symbolizes the snapshot afterwards. This is what convicted the shader-cache disk reads and
  the string-table streaming.
- Release-visible probe families: `[ws.drain]`, `[editor.ws]`, `[cellAtPos]`, portal-cull,
  texture-create, dynamic-buffer-lock. Each is a single cfg key.
- A crash-forensics playbook for when the in-tree `.mdmp` is missing (WER LocalDumps, ASLR-relative
  addresses, nested-CONTEXT recovery).

## 10. In-client editor surface

A stable, versioned **engine-advertise contract** — currently **v33, 160 names** — exported by the
client and consumed by an external editor over a process boundary. It exposes world-snapshot
(`.ws`) editing, interior-layout (`.ilf`) editing, ray-pick and placement primitives, scene
control, and camera/free-cam access.

The contract is **name-keyed**: consumers resolve by stable name, so the version number is
advisory (it tells a consumer a re-sync is due). Rows are `{ name, address }`; the table has no
sentinel row, and `count` is `sizeof`/`sizeof`. Out-params use a size-first protocol so structs can
gain trailing fields without breaking a version-skewed pairing.

The engine defects found *because* an editor exercised paths no player does are the part that
benefits every client, whether or not the contract itself is ever adopted:

- Lossy `.ws` reload; interior-NPC deletion cascade on editor reload; proximity-index re-arm after
  `loadScene`; `loadScene` teardown ordering; a wrong-class guard on `.ilf` rows that now warns
  once and skips the row instead of loading garbage.
- The snapshot delete drain in §6 was found on this path, and it has a consumer-visible
  consequence worth repeating: **a borrowed `Object*` can now be invalidated by mere travel**, not
  only by scene events.

The module is `_WIN64`-aware and export-gated, and is optional — the client builds and runs
without a consumer attached.

## 11. Data and override layer

The client-side code changes above are self-contained, but `gl11` needs a small authored data
payload, layered through the stock `searchPath` mechanism rather than baked into a `.tre`:

- **94 tracked files** under `stage/override/`: 11 pixel + 9 vertex programs (HLSL re-authors of
  the original assembly assets, with `PSRC`-swapped `.psh` for texren/emissive/nebula/ui_radar),
  72 textures (the nebula skybox family and detail maps), `planet_tatooine.pln`, and a merged
  `datatables/interior/interior.iff` carrying the interior-fog fix.
- **`tools/tre-compare/`** — a standalone, zero-engine-imports byte-diff tool that can regenerate
  the "what changed vs stock and why" manifest for that payload, and lets anyone verify the data
  claims independently.
- Config deltas (searchPath layering, `gameFeatures=33297`, budgets and probes) are themselves
  documented — every key in the staged `.cfg` carries its rationale in a comment.

Two cautions recorded here because they cost real time:

- **`stage/` and `stage-x64/` cfgs drift silently.** A one-platform performance regression should
  start by diffing the two cfg files.
- **Never write a `.cfg` with PowerShell `Set-Content`/`Out-File`** — PS 5.1 prepends a UTF-8 BOM,
  which crashes the Release client at boot (Debug masks it). A clean file starts `23 20`; a bad one
  starts `ef bb bf`.

## 12. Config keys added

All default to stock-plus-fixes behavior; all are revertible without a rebuild.

| Section | Key | Default | Effect |
| --- | --- | --- | --- |
| `ClientGame/WorldSnapshot` | `createTimeBudgetMs` | `6` | Per-frame ms budget for the create/delete drain; `0` = unlimited (old behavior) |
| `ClientGame/WorldSnapshot` | `streamOutSnapshotObjects` | `true` | Kill switch for the restored delete drain; `false` restores never-stream-out |
| `ClientGame` | `stallWatchdogMs` | `0` (off) | Frame-stall threshold that arms the watchdog |
| `ClientGame` | `stallWatchdogMaxDumps` | `6` | Minidump budget per session (`0` = log only — recommended, dumps freeze audio) |
| `ClientGame` | `stallWatchdogStackSamples` | `100` | Main-thread stack samples per stall; `0` = off |
| `SharedFile` | `searchPathNegativeCache` | `true` | Caches failed search-path lookups |
| `Direct3d11` | `shaderCachePreload` | `true` | RAM-preloads the disk shader cache off-thread; `false` = per-hit disk reads |
| `Direct3d11` | `constantBufferRing` | `true` | `NO_OVERWRITE` cbuffer ring; `false` = legacy per-slot `WRITE_DISCARD` |
| `Direct3d11` | `preventDriverInternalThreading` | — | NV driver-race mitigation (costs micro-stutter; hence gated) |
| `Direct3d9` | `shaderCachePreload` | `true` | Same preload for the D3D9 bytecode cache |
| `Direct3d9` | `logDynamicBufferLockMs` | off | Logs dynamic VB/IB locks over N ms |
| `ClientGraphics` | `logTextureCreates` | `false` | One line per cold texture create, with wall time |
| `ClientGraphics` | `portalCullProbe` | `false` | Portal/door cull transition trace |

Two related tunables are **source constants**, not cfg keys: `cs_seamGrazeEpsilon` (0.05 m,
`FloorMesh.cpp`) and `cs_cameraPullInSpeed` (8.0 m/s, `FreeChaseCamera.cpp`) — both from the
door-snap fix.

## 13. Compatibility — what did *not* change

- **No server-side changes.** Client-only, stock protocol, developed against a live stock server.
- **Same build system and solution layout** — MSBuild, `src/build/win32/swg.sln`.
- **Same renderer-selection mechanism** — `rasterMajor`; D3D9 stays the default and stays supported.
- **Shared files still shared.** Anything prefixed `shared` is common with the
  [`src`](https://github.com/swg-source/src) server tree; enum and struct changes must be made in
  both or the result is crashes and silent corruption. That upstream rule is unchanged here.
- Upstream's own removals (in-game browser, TCG, CS help/bug form, Perforce references) are
  preserved as upstream left them.

## 14. Known liabilities

Stated plainly rather than discovered later:

- **Editor tools** (AnimationEditor, ParticleEditor, SwgGodClient, …) are pre-broken on the Qt
  `.ui` custom-build step (`MSB8066`) — in this tree *and* upstream. The validation bar is
  `/t:SwgClient` clean plus a dual-renderer boot, not a green full-solution build.
- The advertise surface is **Win32-export-oriented** today.
- **Miles SDK** header/lib additions under `external/3rd` need a licensing review before any
  public redistribution.
- The shader caches assume the **staged-directory layout**.
- `/FORCE` on the SwgClient link masks unresolved externals — build verification requires grepping
  the log, not trusting exit code 0.
- Changing a public struct in a shared header silently breaks ABI against stale plugin DLLs and
  produces a deterministic boot crash. Touch one, rebuild **all** plugin projects.
- A `LNK1281`/SAFESEH failure in the `Optimized` config is pre-existing and not worth
  re-investigating.

## 15. Where the detail lives

The engineering record behind every claim above is unusually complete:

- **`.planning/handoff/`** — one markdown file per workstream, written when context would
  otherwise be lost. `README.md` there is the index; the newest session-close file is the entry
  point. Root-cause narratives, failure signatures, and live-verification records are here.
- **`.planning/research/`** — consult rounds (cross-AI investigations, numbered `CONSULT-NN`) and
  synthesis documents for the harder defects.
- **`docs/research/`** — standing reference material: asset formats and modding guide, architecture
  layers, code conventions, the 64-bit port assessment, MayaExporter parity.
- **`AGENTS.md`** — the operating manual: build, run, renderer switch, cfg safety, diagnostics,
  key paths.
- **`git log`** — commits carry their verification state; the notable landings referenced above
  include `b47718cbc` (snapshot drain), `2869b3838` (create budget), `54fcd54d4` (crossfade),
  `e5404aaa7` (texture pre-warm gate), `2672dff0f` (gl05 bytecode preload), `9c03f53c5`
  (SearchCache), `885b190a0` (threading audit), `111e74cca` (Miles retail runtime),
  `3549c7104` (door-snap), `8cd8c2d82` (interior fog).
