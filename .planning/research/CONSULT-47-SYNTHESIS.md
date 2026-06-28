# CONSULT-47 — Synthesis: Tatooine async-load D3D11 driver null-deref

4 consultants, non-overlapping angles, neutral evidence (CONSULT-47-EVIDENCE-crash.md). Forensics:
`crash-20260623-nvwgf2um-FORENSICS.md`. Raw: `CONSULT-47-{codex,cursor}.out` + the two agent transcripts.

## Unanimous (4/4) — high confidence
1. **Crash path = `MeshAppearanceTemplate` (.msh) async callback → `StaticShaderTemplate` → D3D11 resource
   creation.** NOT the `.mgn`/`SkeletalMeshGenerator` path. Corroborated independently by the
   `ms_crashReportInfo` enter/exit breadcrumb (`MeshAppearanceTemplate.cpp:225/243`), not just unwind.
2. **`d1f92ab1f` (".mgn hardening") is on a DISJOINT path and could not have prevented this crash.** The
   marauder belt `.mgn` is a *temporal co-occurrence* in a different async-loader slot, not a caller→callee
   cause of the wall-mesh/shader load that faulted (Opus + Sonnet both explicit).
3. **My original proposed fix is WRONG LAYER.** `StaticShaderTemplate::create` (StaticShaderTemplate.cpp:99-102)
   is `return new StaticShaderTemplate(...)`; the ctor parses IFF + fetches textures — **no D3D11 call there.**
   The real D3D11 allocation is downstream in the **gl11 `Direct3d11` layer**.

## Productive SPLIT (2–2) on ROOT CAUSE
- **Engine bad-descriptor camp — Codex + Cursor:** the D3D11 call is `ID3D11Device::CreateTexture2D` at
  `Direct3d11_TextureData.cpp:359-373` (TEXTURE, not VB/IB or shader bytecode). Root suspect: a
  **malformed/short DDS** → garbage `D3D11_TEXTURE2D_DESC` (bad Width/Height/Mips/Format). `Texture::load`'s
  DDS header checks are **`DEBUG_FATAL`-only (Texture.cpp:482-492) → no-op in Release**, so a bad read
  continues and feeds the driver a bogus desc. Fix: validate the desc before `CreateTexture2D` + make the
  DDS checks Release-safe (fall back to default texture).
- **Overlay/driver-contention camp — Sonnet + Opus:** the fault is on an **NVIDIA driver worker thread**;
  app-side nulls usually fault *synchronously in our call*, not on a driver thread. `D3D11CreateDevice` was
  called WITHOUT `D3D11_CREATE_DEVICE_SINGLETHREADED` (`Direct3d11_Device.cpp:417-435`), and **UtinniCore is
  injected, shares the device, hooks Present, with active resize churn**. Sonnet H1: the overlay races
  resource-create on the shared device during the cold-cache create burst → driver allocation-tracker
  corruption → null. Cheapest test: **repro with UtinniCore disabled.**

## DE-ANCHORING BREAK (orchestrator — measured history none of the four used)
**This crash occurred WITHOUT Utinni.** The 2026-06-19 occurrence (`gl11-x64-tatooine-zonein-crash.md`,
standalone gl11 x64, no overlay — Utinni integration didn't start until Phase 37 / 2026-06-20) and the older
pre-existing `0x087a` Tatooine crash both predate any injection, with the **same asset signature** (armor
belt + `stco_tato_dtlbase` + `thm_tato` wall, Tatooine zone-in, cold-load, intermittent).
⇒ **Utinni is at most an AGGRAVATOR (extra concurrent device pressure), NOT the root.** This downgrades
Sonnet/Opus H1-as-root and shifts weight to the **overlay-independent** causes: engine bad-desc (Codex/Cursor)
or a driver bug under the cold-cache concurrent-create burst (Sonnet H3). Caveat: the pre-Utinni occurrences
were never stack-captured (the prior fix was "by reading"), so "same crash" is inferred from the identical
signature, not proven.

## Caveats held against false consensus
- "Bad DDS desc" is the leading INFERENCE but **unmeasured** — no one has shown a malformed DDS. The only
  measured truth is the driver null during `CreateAllocation`.
- The `stco_tato_dtlbase_a1_adb13.sht` attribution is **weak** — that breadcrumb (`ShaderTemplateList.cpp:542`)
  is set-at-entry-but-NEVER-cleared, so it's "last shader that began parsing," possibly stale / one-of-six.

## Adjudicating plan (ranked — the instrumentation both discriminates AND can fix)
1. **Instrument + Release-guard the texture-create path** (the one code action that resolves the split):
   at `Direct3d11_TextureData.cpp:~313-373`, log the `D3D11_TEXTURE2D_DESC` (Width/Height/Mips/Format/Usage/
   Bind) + source asset name and **reject/fallback** on an invalid desc *before* `CreateTexture2D`; make
   `Texture::load`'s DDS checks (Texture.cpp:482-492) Release-safe (fall back to the default texture).
   - If the desc is malformed → it NAMES the bad asset and the fallback FIXES the crash (engine camp).
   - If the desc logs CLEAN and it still crashes → proves it's NOT our desc → driver/contention (Sonnet/Opus).
2. **No-Utinni control** (maintainer, 10 min, no rebuild): rename `UtinniCore.dll`, 5+ cold Tatooine zone-ins.
   Crash persists ⇒ overlay not the root (consistent with the de-anchoring break) ⇒ engine/driver.
3. **D3D9 control:** same spot under gl05 (rasterMajor=5). Clean ⇒ D3D11-path-specific.
4. **Driver-version control:** swap NVIDIA 591.86 for a different build. Version-sensitive ⇒ driver bug → pin/file.

**Verdict on the original fix:** correctly-motivated (it IS an async-load D3D11 resource-create crash, and the
prior `.mgn` fix was indeed on the wrong path) but the proposed guard was at a function with no D3D11 call and
over-attributed to a shader. Corrected target: the **gl11 `Direct3d11_TextureData` CreateTexture2D path +
`Texture::load` DDS validation**, run alongside the no-Utinni / driver / D3D9 controls.
