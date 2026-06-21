# 32-bit client cantina degradation — RESOLVED 2026-06-20

**Status:** RESOLVED 2026-06-20. The diagnosis below was **WRONG** — this was TWO separate things, and
neither is the leak-induced hitch theory:

1. **The "camera fling / see Mos Eisley from inside the cantina" (screenshots 0386-0389) = a D3D9-on-32-bit
   floating-point bug, NOT the leak.** Root cause: `Direct3d9::CreateDevice` was missing
   `D3DCREATE_FPU_PRESERVE`, so D3D9 clamps the x87 FPU to single precision → degrades the SHARED
   portal-visibility math → a borderline cell-cull flips at one cantina spot → exterior shows through the
   wall (deterministic, angle-dependent, clears on look up/down). FIXED: one line in `Direct3d9.cpp`
   (`vertexProcessingMode |= D3DCREATE_FPU_PRESERVE;`), built into gl05/gl06/gl07 Win32 Debug+Release,
   runtime-verified on gl05 32-bit Release. Localized by the **renderer×bitness matrix at the exact spot**
   (gl05-32bit BROKEN; gl05-x64 + gl11-32bit CLEAN). See memory
   `project_d3d9_32bit_fpu_preserve_cantina_seethrough`.
2. **The actual 2 GB memory leak is already fixed** by the Phase-32 VS bytecode cache (`ff02a367e`).
   Re-measured 2026-06-20 (`tools/setup/mem-sampler.ps1`) over ~7 min cantina cycling on gl05 32-bit
   Release: floor ~703 MB → plateaued **dead-flat at ~900 MB**, handles/GDI flat. The old +144 MB/2min
   monotonic-to-2GB signature is gone.

Carry-forward: rebuild x64 gl05/06/07 for source/binary consistency (FPU_PRESERVE is a no-op on x64).
Commit pending Kenny's ask. Original (incorrect) diagnosis preserved below for the record.

---

_Original handoff (diagnosis was wrong — kept for the record):_

Surfaced 2026-06-20 while testing the door-snap fix across all 4
configs. **NOT a door-snap regression** (that fix is committed `3549c7104` and verified).

## Symptom
In the **32-bit Release** client (`stage/SwgClient_r.exe`), in the **Mos Eisley cantina interior**:
- **Transient camera fling** — the chase camera briefly punches outside the building (you see Mos
  Eisley terrain/sky from above), then recovers a few seconds later. (Screenshots `stage/screenshots/
  screenShot0386..0389.jpg`, 16:22.)
- **Crackly audio** (buffer underruns).
- General lag/hitching.
- **NOT seen in either 64-bit client.** (32-bit Debug not explicitly tested — check it next session.)

## Diagnosis (high confidence — pre-existing, known)
This matches the **Phase-32 D3DCompile per-cell recompile heap-fragmentation leak** — the exact issue
that drove the v3.0 x64 migration. See `phase-32-d3dcompile-port.md` and memory
`project_phase32_d3dcompile_recompile_leak`, `project_x64_not_impacted_by_32bit_mem_leak`.
- **32-bit-specific** → the 2 GB address-space ceiling. 32-bit cantina cycling = +144 MB/2 min
  monotonic → 2 GB wall. x64 has no 2 GB ceiling → not impacted.
- **Cantina-specific** → gl05's D3DCompile path **recompiles shaders per cell-load** and fragments the
  heap. Cantina is worst.
- **Camera fling / audio crackle / lag** = the per-frame **hitches** under memory pressure: one giant
  `elapsedTime` integrates the camera/movement past geometry for a frame (camera clips the roof);
  Miles underruns → crackle. All transient/self-correcting until eventual OOM.

**Why it's NOT the door-snap fix:** the avatar-collision gate + camera zoom/shoulder ease are bounded
ops that can't fling the camera to the sky or crackle audio; the probe removals are `_DEBUG`-only
(Release byte-identical); the camera code is identical across all 4 configs (a logic bug would hit
64-bit too).

## Confirm first (cheap)
1. Task Manager on `SwgClient_r.exe` while cycling in/out of the cantina — does **memory climb**
   steadily toward ~2 GB, with hitches/crackle worsening as it climbs? (= the leak.)
2. Does **32-bit Debug** (`stage/SwgClient_d.exe`) show it too? (It should — leak is config-agnostic.)
3. (Optional, for certainty it's not the door-snap change) `git stash`-free now that it's committed —
   instead build a clean tree at `3549c7104^` 32-bit Release and compare; the leak will still be there.

## Fix candidates (next session)
1. **Port gl11's compile-once shader bytecode cache to gl05** — the parked Phase-32 item; this is
   *why D3D11 never leaked* (it caches compiled bytecode instead of recompiling per cell). Highest
   value; fixes the root for 32-bit AND reduces x64 churn. See memory
   `project_phase32_d3dcompile_recompile_leak` (FIX = port `Direct3d11_ShaderCache` to gl05).
2. **Broader 32-bit memory-leak walkthrough** — parked post-x64 (`project_memory_leak_walkthrough_post_x64`):
   AllTargets/StatusGround, CreatureObject look-at, TextureRendererManager bake queue, the 32-02 gl05
   shader-path leak under audit. The 32-bit diag stack is broken (DO_TRACK owners null, 2 GB masks
   OOMs, in-proc minidump=0 bytes — dump-reserve fix already landed).
3. **Accept 32-bit as deprecated** — v3.0 chose x64 as the milestone platform specifically to escape
   the 2 GB ceiling. If "get 32 working" just needs the cantina survivable, candidate 1 likely
   suffices; full 32-bit parity is a bigger lift.

## Method reminders
- 32-bit OOM masks real crashes; 2 GB ceiling is the killer. Method = 5 s bg sampler on
  `PrivateMemorySize64`, compare floor at the same in-world state (per `project_x64_not_impacted...`).
- gl05 = D3D9 (rasterMajor=5). Debug→client_d.cfg, Release→client.cfg. Never write .cfg via PS
  Set-Content (BOM crash).
