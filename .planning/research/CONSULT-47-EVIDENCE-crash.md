# CONSULT-47 — EVIDENCE (treat as GIVEN; measured ground-truth from a real minidump)

A from-source 32-bit SWG client (`stage/SwgClient_r.exe`, MSVC) crashed. Below is measured fact from the
minidump via cdb against the matching PDB. Do NOT re-derive these; reason FROM them. Repo:
`D:/Code/swg-client-v2`. Engine source under `src/engine/`, app under `src/game/client/application/SwgClient/`.

## Process / environment (given)
- 32-bit Release client, on planet Tatooine, process uptime **38 s**, `BytesAllocated ~138 MB` (NOT OOM).
- **D3D11 renderer active** (`gl11_r.dll` + `d3d11.dll` loaded; rasterMajor=11).
- An external 32-bit overlay DLL **`UtinniCore.dll` IS injected** into the process (a modding overlay that
  hooks Present and shares the D3D11 device/swapchain).

## Exception (given)
- `c0000005` ACCESS_VIOLATION, **read of address 0x00000000** (null deref).
- Faulting instruction: `mov eax, dword ptr [eax]` with **eax=0**, in **`nvwgf2um.dll`** (the NVIDIA
  Direct3D user-mode driver, v32.0.15.9186 / "591.86", 2026-01).
- The **faulting thread is a driver-internal worker thread** — its entire stack is
  `nvwgf2um!… → kernel32!BaseThreadInitThunk → ntdll` (a thread the NVIDIA driver spawned).

## Main thread (thread 0) stack at crash — bottom→top (given; unwind info unavailable, frames approximate)
```
SwgClient_r!WinMain → ClientMain (ClientMain.cpp:387)
  → Game::run (Game.cpp:1041) → Game::runGameLoopOnce
    → AsynchronousLoader::processCallbacks (AsynchronousLoader.cpp:631)
      → MeshAppearanceTemplate::asynchronousLoadCallback (MeshAppearanceTemplate.cpp:242)
        → MeshAppearanceTemplate::load → MeshAppearanceTemplate::load_0005
          → ShaderPrimitiveSetTemplate::ShaderPrimitiveSetTemplate → ::load → ::load_sps → ::load_sps_0001
            → ShaderTemplateList::fetch (×6) → ShaderTemplateList::create
              → StaticShaderTemplate::create → StaticShaderTemplate::StaticShaderTemplate
                → [d3d11.dll resource-create frames] → nvwgf2um → win32u!NtGdiDdDDICreateAllocation
```
So the main/render thread was, in the normal game loop, draining the async-load callback queue; a finished
**MeshAppearanceTemplate** is building its shader (`ShaderPrimitiveSet` → `StaticShaderTemplate`), whose
construction creates D3D11 GPU resources (→ kernel `NtGdiDdDDICreateAllocation`). The NVIDIA driver
null-deref'd on a worker thread while servicing that allocation. The main thread did not fault; it was
mid-`CreateAllocation` when the driver-thread crash tore the process down.

## Crash-reporter breadcrumbs (given; each line set independently by its own subsystem's last async op)
- `MeshAppearanceTemplate: appearance/mesh/thm_tato_imprv_wall_sml_s01_l0.msh`  ← set at entry of the
  crashing `asynchronousLoadCallback`, cleared at its exit → THIS mesh's load was in flight at crash.
- `SkeletalMeshGeneratorTemplateAsync: appearance/mesh/armor_marauder_s01_belt_m_l3.mgn`  ← a *separate*
  subsystem slot (concurrent/last-seen skeletal load).
- `ShaderTemplate_Iff: shader/stco_tato_dtlbase_a1_adb13.sht`
- `AppearanceTemplate: appearance/mesh/wp_pistol_flare_l0.msh`
- `~Object: name=[(null)] template=[(null)]`

## Prior change present in the crashed binary (given)
- Commit `d1f92ab1f` (2026-06-19) hardened **`clientSkeletalAnimation` / `SkeletalMeshGenerator(Template).cpp`**
  async `.mgn` load (3 guards + `[MGN-PROBE]` logging). It is an ancestor of the crashed build. It does NOT
  modify `MeshAppearanceTemplate`, `ShaderPrimitiveSetTemplate`, `StaticShaderTemplate`, or any D3D11 path.
- Reproduction history: this crash is intermittent on Tatooine zone-in; never deterministically reproduced
  before this dump. The user reports it "always crashes on the marauder belt."

## What we want (answer YOUR assigned angle only; see your task file)
Independently determine: (a) the most likely PROXIMATE cause of a null reaching the D3D11 driver on this
path, (b) where the provider-side guard/fix should go, (c) whether the `d1f92ab1f` `.mgn` hardening could
plausibly be related to this fault at all. Cite `file:line` from the actual source. A productive SPLIT
between consultants is the goal — do not converge by assumption.
