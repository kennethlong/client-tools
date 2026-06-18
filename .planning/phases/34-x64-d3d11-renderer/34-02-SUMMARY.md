---
phase: 34-x64-d3d11-renderer
plan: 02
status: complete
completed: 2026-06-18
requirements: [X64-03]
human_signoff: approved
deferred_followup: formal RenderDoc 32-bit-vs-x64 semantic A/B (deferred to post-memory-leak final audit)
---

# Plan 34-02 Summary — x64 client boots rasterMajor=11 + visual v2.2 parity (human-approved)

**Outcome:** The x64 client boots to **dressed character-select under gl11 (rasterMajor=11)** and renders
the world at the **v2.2 visual-parity bar** across the canonical triad — **human-approved** on visual
inspection. Both non-regression boots are green (32-bit gl11 rasterMajor=11; gl05 x64 rasterMajor=5 in
the shared x64 client). The second renderer (D3D11) has reached x64 parity. **X64-03 satisfied.**

## What was verified

### Task 1 — autonomous prep + x64 boot
- `stage-x64/client_d.cfg` → `rasterMajor=11`, BOM-free (set via Edit tool; the cfg starts `23 20`, no `ef bb bf`).
- **Pre-boot DLL-bitness sweep (review #5):** all **8664** — `SwgClient_d.exe`, `gl11_d.dll`, `DllExport.dll`, `d3dcompiler_47.dll`. The x64 client, plugin, host bridge, and shader runtime are all consistently x64.
- **Shader-cache cwd pre-flight (review #4):** `ConfigDirect3d11.cpp:57` hardcodes `ms_shaderCacheDir = "stage/shader-cache/"` (cwd-relative). Launched from `stage-x64/`, this resolves to a **cold** cache → first x64 boot cold-compiles shaders (slow startup, **not** a hang/TDR). No warm cache existed on either side to seed.
- **Boot result:** the x64 client (gl11) reached **dressed character-select** — the real runtime proof of the `GetApi` export + the DllExport x64 delay-load bridge that the build gate cannot establish. (Human-observed.)

### Task 2 — parity + non-regression
- **Visual parity (human):** the v2.2 bar holds across the **triad** — dressed character-select (face/dot3/armor tints), Tatooine (terrain blend, world, sky), cantina interior (FFP wall-lights, textured walls/floor, NPC faces). "Everything looks good."
- **Non-regression boots — all green (human-confirmed):**
  - 32-bit gl11 rasterMajor=11 → character-select reached; `stage/gl11_d.dll` confirmed still **14C (x86)**, untouched by the x64 build.
  - gl05 x64 rasterMajor=5 → character-select reached (gl05 still loads in the shared x64 client). Cfg restored to `rasterMajor=11` (BOM-free) afterward.

### Task 3 — human checkpoint
- **Signed off: "approved"** — v2.2 parity bar holds on x64 across the triad and all four boots are green.

## DEFERRED FOLLOW-UP (not done this session, by user decision)
- **Formal RenderDoc gl11-32bit-Debug-vs-gl11-Debug|x64 semantic A/B (D-02 method) was NOT completed.**
  RenderDoc could not capture the **32-bit** side due to **memory-pressure issues** on the 32-bit build
  (the chronic 2 GB-ceiling problem that motivated the whole v3.0 x64 port). Per the user's decision, the
  formal capture-diff is **deferred to a final audit after the memory-leak investigation** (see
  [[project_memory_leak_walkthrough_post_x64]], [[project_phase32_d3dcompile_recompile_leak]]). SC#2's
  parity bar is met this session by **human visual sign-off**; the rigorous RenderDoc semantic A/B
  (semantic draw matching + ≤1 ULP tolerance, reviews #1/#2) remains as the deferred objective probe.
  This is an honest scope note — the A/B method was the *plan's* rigor mechanism, accepted-by-visual
  this phase with the objective diff pending the leak fix.
- **Lead for the deferred audit (user, 2026-06-18): run the RenderDoc A/B under the RELEASE stack** — the
  Release build's far-lower memory overhead sidesteps the 32-bit Debug memory pressure that blocked the
  capture (the same Release-stack trick used for prior parity captures, [[project_d3d9_charselect_capture_via_release_stack]]).
  Two forms:
  - **Cleanest — 32-bit Release `gl11_r` vs x64 Release `gl11_r`** (identical Release codegen, pure
    arch-only diff, both dodge the Debug OOM). **Caveat:** x64 Release (`gl11_r.dll`) is **not linked yet**
    (D-04 deferred `Release|x64` to the all-plugin consolidation phase); the `Release|x64` vcxproj block is
    already authored, so it's a one-build add when that phase lands. Prefer this form once x64 Release exists.
  - **Available now — 32-bit Release vs x64 Debug** (32-bit `gl11_r.dll` in `stage/`, x64 `gl11_d.dll` in
    `stage-x64/`): dodges the memory pressure but mixes Release-vs-Debug codegen, so it is NOT a pure
    arch-only diff (optimization deltas would confound a strict ≤1 ULP read). Usable as a coarse sanity
    pass, not the rigorous probe.

## Success criteria
- **SC#1** (gl11 builds x64): ✅ Plan 01 — gl11_d.dll machine 8664, clean link, exports GetApi, staged (Debug|x64 scope; Release|x64 deferred per D-04).
- **SC#2** (x64 boots rasterMajor=11 + v2.2 parity): ✅ boot-to-dressed-char-select confirmed; v2.2 bar **human-approved visually** across the triad. Formal RenderDoc A/B deferred (above).
- **SC#3** (32-bit gl11 non-regression): ✅ 32-bit gl11 boots rasterMajor=11 to char-select; stage/gl11_d.dll still 14C.
- **Cross** (gl05 x64 non-regression): ✅ gl05 x64 boots rasterMajor=5 to char-select in the shared client.

## Key files
- `stage-x64/client_d.cfg` — `rasterMajor=11` (BOM-free; local dev-state toggle, left in place, uncommitted per the stage-cfg convention).
- No source files modified in Plan 02 (boot + verify only).

## Deviations from plan
1. **RenderDoc A/B deferred** (see DEFERRED FOLLOW-UP) — 32-bit-side capture blocked by 32-bit memory
   pressure; SC#2 accepted by visual human sign-off, objective diff pending the memory-leak fix.
2. **cfg left uncommitted** — `stage-x64/client_d.cfg` is tracked but the `rasterMajor` value is a
   reversible local dev toggle (Plan 02 itself toggles it 11→5→11 for the gl05 cross-check); left at 11,
   uncommitted, consistent with the stage-cfg local-state convention.

## Self-Check: PASSED (with documented deferral)
The x64 client boots rasterMajor=11 to dressed char-select; v2.2 parity human-approved across the triad;
four green boots; 32-bit + gl05-x64 non-regression confirmed. X64-03 satisfied. The formal RenderDoc
semantic A/B is the one deferred objective probe (post-memory-leak audit) — recorded, not silently dropped.
