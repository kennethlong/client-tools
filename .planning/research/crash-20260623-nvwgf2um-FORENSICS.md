# Crash Forensics — 2026-06-23 Tatooine async-load D3D11 driver null-deref (nvwgf2um)

**Status:** FORENSIC CAPTURE — high-value recurring target ("we thought we fixed this").
**Dump:** `stage/SwgClient_r.exe-unknown.0-20260623120326.mdmp` (219,841 bytes) + companion `.txt`.
**Captured by:** cdb x86 (Win Kits 10) against `stage/SwgClient_r.pdb`. Raw log:
`.planning/research/crash-20260623-nvwgf2um-raw.txt`.

---

## 0. One-line

`c0000005` **null-pointer read inside the NVIDIA D3D usermode driver** (`nvwgf2um.dll`, v32.0.15.9186 /
"591.86", ts 2026-01-20) on a **driver worker thread**, triggered while the **main thread was allocating a
D3D11 shader/texture resource during an asynchronous Tatooine asset-load callback**. NOT in our engine
code, NOT in UtinniCore, NOT the Phase-38 CuiMediator/contract work — but our async D3D11 resource-create
path is what fed the driver the work it died on.

## 1. Exception

- Code: `c0000005` (ACCESS_VIOLATION), read.
- Faulting instr: `nvwgf2um!NVAPI_DirectMethods+0x9f81c: 8b00  mov eax,dword ptr [eax]` with **eax=0** →
  read of `0x00000000` (null deref). (Symbol is nearest-export only; driver ships export symbols.)
- Process uptime: **0:00:38**. `MainLoop: 2368`, breadcrumb `UpTime: 23`.
- Faulting thread id `c040` (thread 041 in the dump) — a **driver-internal worker thread**.

## 2. Faulting thread (driver worker) — pure nvwgf2um

```
eax=00000000 ... eip=56c98b3c
nvwgf2um!NVAPI_DirectMethods+0x9f81c   ← mov eax,[eax], eax=0  (CRASH)
nvwgf2um!NVENCODEAPI_Thunk+0xd1832
nvwgf2um!SetDependencyInfo+0x55a3c
nvwgf2um!SetDependencyInfo+0x5590f
nvwgf2um!NVDEV_Thunk+0x1587b5
nvwgf2um!NVDEV_Thunk+0x492f17          ← driver thread-proc
kernel32!BaseThreadInitThunk
ntdll!Rtl...                           ← thread start
```
(NVENCODEAPI/SetDependencyInfo are nearest-export approximations in a stripped driver — read as
"driver-internal", not literally NVENC.) 31 `nvwgf2um` worker-thread frames across the dump = the driver's
own thread pool.

## 3. Main thread (thread 0) — THE TRIGGER (our async D3D11 resource create)

```
win32u!NtGdiDdDDICreateAllocation+0xc            ← kernel: allocate GPU surface (eax=c000003a on return)
d3d11!D3D11CoreCreateLayeredDevice+0xa8f6        ← (nearest export; really D3D11 resource create)
d3d11!...  /  nvwgf2um!...  (D3D11 runtime <-> driver resource creation)
d3d11!D3D11CoreCreateDevice+0x14ef2
...
SwgClient_r!StaticShaderTemplate::StaticShaderTemplate(   ← building the shader's D3D11 resources
SwgClient_r!StaticShaderTemplate::create(
SwgClient_r!ShaderTemplateList::create(
SwgClient_r!ShaderTemplateList::fetch(  ×6
SwgClient_r!ShaderPrimitiveSetTemplate::load_sps_0001(
SwgClient_r!ShaderPrimitiveSetTemplate::load_sps(
SwgClient_r!ShaderPrimitiveSetTemplate::load(
SwgClient_r!ShaderPrimitiveSetTemplate::ShaderPrimitiveSetTemplate(
SwgClient_r!MeshAppearanceTemplate::load_0005(
SwgClient_r!MeshAppearanceTemplate::load(
SwgClient_r!MeshAppearanceTemplate::asynchronousLoadCallback  [MeshAppearanceTemplate.cpp:233]
SwgClient_r!AsynchronousLoader::processCallbacks             [AsynchronousLoader.cpp:631]
SwgClient_r!Game::runGameLoopOnce(
SwgClient_r!Game::run                                        [Game.cpp:1041]
SwgClient_r!ClientMain                                       [ClientMain.cpp:387]
SwgClient_r!WinMain
```
So at crash time the **main/render thread** is, inside the normal game loop, draining the async loader's
callback queue; a finished **MeshAppearanceTemplate** is building its `ShaderPrimitiveSetTemplate` →
`StaticShaderTemplate`, whose construction creates **D3D11 GPU resources** (textures/shaders →
`NtGdiDdDDICreateAllocation`). The driver services that allocation on its worker pool and one worker
null-derefs. The main thread did not fault; it was mid-`CreateAllocation` syscall when the driver-thread
crash tore the process down.

## 4. Environment (loaded modules of interest)

| Module | Base | Note |
|---|---|---|
| `SwgClient_r.exe` | — | the advertised client (38-06 build, has the CuiMediator guard) |
| `gl11_r.dll` | (stage\) | **D3D11 renderer active — rasterMajor=11** |
| `d3d11.dll` | 607d0000 | D3D11 runtime |
| `nvwgf2um.dll` | 56b60000 | **NVIDIA D3D usermode driver v32.0.15.9186 ("591.86"), ts 2026-01-20** |
| `UtinniCore.dll` | 62320000 | **Utinni IS injected** (+ `UtinniCore_Symbols`) — this was the live inject-smoke |

## 5. Engine-state breadcrumbs (from the .txt — main-thread last-known)

- Player on **Tatooine** (3475.50, 4.01, -4862.30), `terrain/tatooine.trn`.
- `SkeletalMeshGeneratorTemplateAsync: armor_marauder_s01_belt_m_l3.mgn` ← matches the KNOWN intermittent
  Tatooine async-`.mgn`-load crash (memory `project_intermittent_tatooine_crash_087a`).
- `AppearanceTemplate: appearance/mesh/wp_pistol_flare_l0.msh`
- `ShaderTemplate_Iff: shader/stco_tato_dtlbase_a1_adb13.sht`
- `MeshAppearanceTemplate: appearance/mesh/thm_tato_imprv_wall_sml_s01_l0.msh`
- `~Object: name=[(null)] template=[(null)]` (an object torn down with null name/template — teardown churn).
- `BytesAllocated: ~138 MB` (not an OOM; 32-bit headroom fine).

## 6. Assessment

- **This is a D3D11 asset-streaming crash that surfaces inside the NVIDIA driver.** It is on the
  async-load → shader/texture **resource-creation** path, NOT the per-frame draw path, NOT the overlay's
  ImGui path, NOT CuiMediator, NOT the GetEngineHookPoints contract. **It is not a regression from this
  session's Phase 37/38 work** (which never touched the gl11/D3D11 renderer or the shader-resource path).
- It correlates with the long-known **intermittent Tatooine async-`.mgn` crash** (retry-usually-succeeds),
  here landing as a **driver-side null deref under D3D11** rather than the 32-bit `0x087a` face it has worn
  before. The "we thought we fixed this" likely refers to that intermittent async-load crash and/or a prior
  D3D11 resource-lifetime fix (cf. `project_d3d11_collide_use_after_free`,
  `project_d3d11_cape_spike_skinning_regression` — both were us feeding the GPU bad/stale resource data).

## 7. Open question — driver bug vs. our resource params

A null deref *inside* the driver during `CreateAllocation` is usually one of:
1. **We hand the D3D11 runtime a bad/partial resource descriptor** (null/garbage shader bytecode, a
   texture desc with a zero/invalid field, a stale/freed sub-resource pointer) during the async
   `StaticShaderTemplate`/`ShaderPrimitiveSet` build, and the driver deref's it. Most actionable on our side.
2. **A driver bug** in nvwgf2um 591.86 on this specific create sequence (possibly aggravated by the Utinni
   overlay sharing the device / by the embed reparent churn that was in flight this session).
3. A **resource-lifetime race** between the async-load resource create and the overlay/engine using the
   device (both on the render thread here, so less likely a raw data race — but eviction/timing, as in the
   prior collide use-after-free, has bitten this engine before).

## 8. Next diagnostic steps (ranked)

1. **D3D9 control:** repro on `rasterMajor=5` (gl05) at the same Tatooine spot. Clean under D3D9 ⇒
   D3D11-path-specific (driver or our D3D11 resource create).
2. **No-Utinni control:** standalone `gl11_r` (no injection) at the same spot. Clean standalone but crashes
   injected ⇒ the overlay's device-sharing/embed churn is implicated, not pure asset streaming.
3. **Driver control:** the driver is 591.86 (2026-01). Test a different NVIDIA driver (newer/older).
   A driver-version-sensitive repro = driver bug, file with NVIDIA / pin a known-good driver.
4. **RenderDoc** capture approaching the Tatooine load (gl11 only) — inspect the `StaticShaderTemplate`
   resource-create calls for a malformed desc / null bytecode at the crashing asset
   (`stco_tato_dtlbase_a1_adb13.sht` / the `thm_tato_imprv_wall` mesh / `armor_marauder` belt).
5. **Instrument** `MeshAppearanceTemplate::asynchronousLoadCallback` (:233) + the `StaticShaderTemplate`
   D3D11 create to validate every resource desc/bytecode is non-null before the D3D11 call (turns a driver
   null-deref into an observable engine-side assert at the real culprit).

## 8b. THE KEY FINDING — the prior "fix" hardened the WRONG path

The crash the team thought was fixed is `d1f92ab1f` (2026-06-19,
"fix(clientSkeletalAnimation): harden async .mgn load against the intermittent world-load crash"),
documented in `.planning/handoff/gl11-x64-tatooine-zonein-crash.md`. It IS in the crashed binary (ancestor
of HEAD; 65 commits back). BUT:

- That fix was **defensive-by-reading, NEVER reproduced** ("repro exhausted, fixed by reading"; ~120
  headless iters never hit it). It guarded **three hazards entirely inside `clientSkeletalAnimation` /
  `SkeletalMeshGenerator(Template)`** (I4 load-wedge, RANK-1 null pending-list, I3 release-deref guards),
  each with `[MGN-PROBE]` "would-have-fired" logging.
- It attributed the crash to the **`armor_marauder` belt `.mgn`** purely from the crash **breadcrumb** —
  and the handoff explicitly hedged: *"these are last-seen breadcrumbs per category, not necessarily the
  faulting object."*
- **This dump's real stack proves the fault is NOT on the `.mgn` skeletal path at all.** It is on the
  parallel **`MeshAppearanceTemplate` (`.msh`) async path → `StaticShaderTemplate::create` → D3D11
  resource allocation → driver**. None of the hardened `SkeletalMeshGenerator` code is on the faulting
  stack. The engine's own `ms_crashReportInfo` for the `MeshAppearanceTemplate` slot at crash time =
  **`thm_tato_imprv_wall_sml_s01_l0.msh`** (set at `asynchronousLoadCallback` entry, cleared at exit —
  so that mesh's load was in flight). The marauder belt `.mgn` is a concurrent/last-seen breadcrumb in a
  different slot, exactly as the handoff feared.

**Conclusion:** the prior fix could not have prevented this crash — it hardened a path the fault doesn't
take. The real culprit is async **`MeshAppearanceTemplate` → `ShaderPrimitiveSet` → `StaticShaderTemplate`
D3D11 shader/texture resource creation** for `stco_tato_dtlbase_a1_adb13.sht`, where a null/invalid
resource (shader bytecode / texture / desc) is most likely handed to the D3D11 runtime and the NVIDIA
driver deref's it. (Cf. the v2.2 asset-PS pipeline work that touched `StaticShaderTemplate` /
PSRC recompile — a Tatooine detail-base shader producing null bytecode under a cold-load timing would fit.)

The crashing callback (`MeshAppearanceTemplate::asynchronousLoadCallback`, MeshAppearanceTemplate.cpp:242):
sets `ms_crashReportInfo`, `iff.open(getName()); load(iff);` (← the crashing `ShaderPrimitiveSet`/
`StaticShaderTemplate` build happens in `load`), then `(*i)->create()` on the uninitialized appearances.

## 8c. Recommended fix direction (mirror d1f92ab1f's method onto the RIGHT path)

1. **Apply the same defensive-harden + `[probe]`-log treatment to the `StaticShaderTemplate::create` /
   `ShaderPrimitiveSetTemplate::load` D3D11-resource-creation path** — validate every shader bytecode /
   texture / D3D11 resource desc is non-null BEFORE the D3D11 create call; on a null, skip + log the asset
   name instead of handing the driver a null. This converts the opaque driver null-deref into an
   engine-side, named-asset assert at the true culprit (`stco_tato_dtlbase_a1_adb13.sht` first suspect).
2. Confirm with the controls in §8 (D3D9 / no-Utinni / driver-version) whether the bad resource is ours
   (param) vs a driver bug on a valid-but-edge sequence.

## 9. Artifacts

- Dump: `stage/SwgClient_r.exe-unknown.0-20260623120326.mdmp`
- Crash .txt (breadcrumbs): `stage/SwgClient_r.exe-unknown.0-20260623120326.txt`
- Raw cdb log: `.planning/research/crash-20260623-nvwgf2um-raw.txt`
- Symbols: `stage/SwgClient_r.pdb`
