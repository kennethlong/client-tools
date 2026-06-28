# Cross-AI Plan Review Request

You are reviewing implementation plans for a software project phase. Provide structured, critical feedback on plan quality, completeness, and risks. Be adversarial — find blind spots.

## Project Context (excerpt)
# whitengold — SWG Client Modernisation Port

## Current State: v2.3 Hardening SHIPPED (2026-06-15)

**v2.3 Hardening shipped — 10/12 requirements satisfied, the rest root-caused as x64 carry-forwards.** Two independent streams closed. **(A) In-client C++ hardening (Phases 24–27):** DPVS occlusion is config-gated (`[ClientGraphics/Dpvs] occlusionMode = auto|on|off`, auto-gated on POB-cell membership, F11 override preserved) — HARD-01; the staged client is machine-portable (Miles redist vendored + postbuild-repointed with a codec-repair guard, a tracked `client.cfg.template` + `setup-client.ps1` generator de-hardcoding `stage/override`+`stage/miles` and cleaning the Phase-11+ test keys) — PORT-01/02; the D-15 DPVS instrumentation was stripped atomically and the Options-window FATAL fixed — HARD-03 (D-15)/HARD-04. The two remaining client items were **root-caused, not skipped**: the cantina corner-snap (HARD-02) reduced to a 32-bit codegen float transient in the cell→world Y transform (CONSULT-43), and the `D3DXCompileShader`→`D3DCompile` port (HARD-05) was attempted, reverted, and shown to re-fight the entire gl11 shader-modernization battle — both are 32-bit-bound and parked for the x64 milestone (HARD-05 is satisfied-by-Fix-A for v2.3; HARD-03's CORNERSNAP probes are intentionally kept as the x64 acceptance harness). **(B) TRE compare tool (Phases 28–30):** the repo's first web app — an isolated, extractable `tools/tre-compare/` uv package (vendored stdlib parser → `[SharedFile]` scanner → first-hit-wins merged-virtual-tree → set/file diff with `(length, compressedLength)` signal + on-demand xxhash → sqlite cache → 4 FastAPI routes) under a Vite 8 / React 19 / Tailwind 4 / shadcn virtualized tree-diff SPA, served from a single 127.0.0.1 process. SC#4 human-approved on a real 231,086-row cross-distribution diff (SWGSource × SWG Infinity). Audit `gaps_found` (the single gap = the pre-decided HARD-02 x64 deferral), 12/12 integration links WIRED, 2/2 E2E flows complete.

**Prior:** v2.2 Visual Parity (shipped 2026-06-12) — D3D11 (`rasterMajor=11`) reached full visual parity with the known-good D3D9 baseline across characters, interiors, open world, terrain, gamma, UI, and effects (13/13 requirements; asset-PS pipeline + FFP combiner cascade + gamma curve pass + round minimap). See `milestones/v2.2-*`.

## Shipped Milestone: v2.3 — Hardening (closed 2026-06-15)

**Goal (MET, with documented x64 deferrals):** Consolidate the v2.2 parity win — act on the DPVS verdict, strip the accumulated debug instrumentation, make the staged client machine-portable, fix the known runtime crashes/quirks, and ship a modern web-based TRE compare tool for cross-installation data diagnostics. 10/12 requirements fully satisfied; HARD-02 (corner-snap), HARD-05's clean D3DCompile port, and HARD-03's CORNERSNAP-probe removal deferred to the x64 milestone (all root-caused 32-bit-bound). Per-phase detail archived in `milestones/v2.3-ROADMAP.md`; audit `milestones/v2.3-MILESTONE-AUDIT.md`.

**Delivered:**
- **DPVS config-gate (HARD-01)** — occlusion outdoor-on / indoor-off as `occlusionMode = auto|on|off`, F11 override preserved
- **Machine portability (PORT-01/02)** — Miles redist vendored + postbuild-repointed (codec-repair guard); `client.cfg.template` + `setup-client.ps1` de-hardcode stage paths + clean `client_d.cfg`
- **Instrumentation removal + Options FATAL (HARD-03 D-15 / HARD-04)** — `DpvsProfileInstrumentation` grep-zero; Options window no longer FATALs; CORNERSNAP probes kept as the x64 harness
- **32-bit-bound quirks root-caused → x64 (HARD-02 / HARD-05)** — corner-snap = 32-bit codegen float transient; D3DCompile port re-fights the gl11 shader battle (satisfied-by-Fix-A for v2.3)
- **TRE compare tool (TRE-01..05)** — standalone localhost web app: two-install set-delta + merged-virtual-tree file diff, virtualized filterable tree UI, per-file drill-in with the metadata-vs-content honesty distinction

**Key context:**
- Core invariant held: every client-touching phase (24–27) left the client bootable to character select under both `rasterMajor=5` and `=11` (the TRE tool is a standalone web app, outside that invariant)
- The TRE tool's first real use case (SWGSource-vs-whitengold space-asset diff) is unblocked; the parser is vendored from `D:/Code/swg-blender-plugin/swg_pipeline/` (the stale `D:/Code/swg-tools` pointer is wrong)
- Excluded: SWG-Source community-compat sync, gameplay-parity work, Nyquist validation backfill for phases 18/19–22 (milestone audit stands as the verification record)

## Current Milestone: v3.0 — x64 Port

**Goal:** Port the modernized client to a native **x64** build — keeping **both** renderers (D3D9 gl05/06/07 + D3D11 gl11) bootable to character select — to eliminate the 32-bit-bound defects root-caused across v2.x (the cantina door-snap float-codegen transient, the chronic address-space OOM crash) and remove the x64-hostile legacy D3DX dependency. A proven reference exists: SWG Restoration ships a stable x64 D3D9 client (`D:\SWG Restoration\x64`) with no door-snap.

**Target features:**
- **x64 build platform** — add the `x64` platform to `src/build/win32/swg.sln` + every `.vcxproj`; client boots to character select in x64 under both `rasterMajor=5` (D3D9) and `=11` (D3D11)
- **x87 → SSE/intrinsics** — replace the x64-illegal `__asm fnstcw/fldcw` in `FloatingPointUnit.cpp` with `_controlfp`/`_control87`; tree-wide `__asm` sweep
- **64-bit correctness** — pointer/int truncation fixes (C4311/C4312/C4244 with warnings-as-errors), `#pragma pack` / hardcoded-`sizeof` / serialization-width audit (IFF/TRE/network message layout)
- **Third-party x64 libs** — rebuild/relink against x64 variants (Restoration's `x64/` is the reference: dpvs, bink, pcre, libxml2, icu, jpeg, discord-rpc); rebuild gl05/06/07 + gl11 plugins as x64
- **Miles 7.2e → 9.3b audio port** — vendor the 9.3b SDK (`D:/Code/milesss-v9.3b/`), port the `clientAudio` call sites (API verified ~source-compatible; the one signature edit is `AIL_room_type`), stage the x64 redist + `.asi`/`.flt` provider set
- **D3DX → `d3dcompiler_47`** — the deferred HARD-05; D3DX is x64-hostile, so the D3DCompile port becomes mandatory (Restoration did this for x64)
- **Verify + CORNERSNAP cleanup** — confirm the door-snap + OOM-crash class are resolved against the Restoration x64 reference; strip the CORNERSNAP `_DEBUG` probes once verified clean (completes the deferred half of HARD-03)

**Scope notes:** Complementary to (not competing with) the D3D11 work — this is a platform port that keeps both renderers; the "complete" client is eventually **64-bit + D3D11**. Mechanical port of the *same* renderers (bounded, unlike the D3D11 rewrite). Tracked in `todos/pending/2026-06-13-64bit-x64-port.md`. Phases continue from 30 → start at **31**.

## Prior State: v2.1 Decruft SHIPPED (2026-05-27)

## Phase 31: 64-bit Correctness Foundation — Roadmap Section
#### Phase 31: 64-bit Correctness Foundation
**Goal**: The whole source tree compiles x64-clean - x64-illegal inline asm replaced with intrinsics, pointer/integer truncation defects fixed, and data-layout (struct packing / hardcoded `sizeof` / serialization width) audited for IFF/TRE + network-message correctness - so the build path is ready for the x64 platform add.
**Depends on**: Nothing (first phase of v3.0; builds on the shipped 32-bit tree)
**Requirements**: BITS-01, BITS-02, BITS-03
**Success Criteria** (what must be TRUE):
  1. The x87 FPU-control inline asm (`FloatingPointUnit.cpp` `__asm fnstcw/fldcw`) is gone, replaced with `_controlfp`/`_control87`, and a tree-wide `__asm` sweep shows no x64-illegal inline asm survivors in the build path
  2. The touched code compiles with truncation warnings (C4311 / C4312 / C4244) treated as errors - no `(int)pointer` / `DWORD`-holds-pointer survivors
  3. IFF/TRE and network-message struct packing / hardcoded-`sizeof` / serialization-width assumptions are audited and corrected; the 32-bit build still loads assets, parses saved data, and exchanges network messages correctly (no data-layout regression)
  4. The 32-bit client still boots to character select under both `rasterMajor=5` and `=11` after the source changes (no regression while x64 is not yet building)
**Plans**: 6 plans in 3 waves
  - [ ] 31-01-PLAN.md — scratch Debug|x64 compile-only harness (D-01 worklist generator; gitignored; in-scope TU manifest)
  - [ ] 31-02-PLAN.md — BITS-01 FPU + SSE math (FloatingPointUnit _control87, SseMath ×13 + Transform naked-SSE → _mm_*)
  - [ ] 31-03-PLAN.md — BITS-01 misc __asm sweep (CollisionUtils/Fatal/Clock/ProfilerTimer/VeCritsec + DebugHelp _M_X64 fork)
  - [ ] 31-04-PLAN.md — BITS-02 pointer/int truncation (StaticShader/MemoryManager/Os + Direct3d11 re-truncation; width-correct types)
  - [ ] 31-05-PLAN.md — BITS-03 serialization/layout (Archive map uint32_t pin + TargaFormat/AssetCustomization static_asserts)
  - [ ] 31-06-PLAN.md — phase gate: full scratch-x64 sweep + Phase 33 residual worklist (D-02) + 32-bit link-grep + dual-renderer boot smoke (D-08/SC#4)

#### Phase 32: D3DX to d3dcompiler_47

## Requirements Addressed (BITS-01/02/03)
- [ ] **BITS-01**: The x87 FPU-control inline asm (`FloatingPointUnit.cpp` `__asm fnstcw/fldcw`) is replaced with `_controlfp` / `_control87` intrinsics, and the whole tree is swept for `__asm` and made x64-clean (x64 forbids inline asm)
- [ ] **BITS-02**: Pointer/integer truncation defects are resolved — the touched code compiles x64-clean with truncation warnings (C4311 / C4312 / C4244) treated as errors; no `(int)pointer` / `DWORD`-holds-pointer survivors in the build path
- [ ] **BITS-03**: Struct packing / hardcoded `sizeof` / serialization-width assumptions are audited and corrected for IFF/TRE and network-message layouts — no x64 data-layout regressions (assets load, saved data and network messages parse correctly)

### Audio Port (Miles 7.2e → 9.3b)

--
| BITS-01 | Phase 31 | Pending |
| BITS-02 | Phase 31 | Pending |
| BITS-03 | Phase 31 | Pending |
| AUDIO-01 | Phase 35 | Pending |
| AUDIO-02 | Phase 35 | Pending |
| SHADER-01 | Phase 32 | Pending |

## User Decisions (31-CONTEXT.md)
# Phase 31: 64-bit Correctness Foundation - Context

**Gathered:** 2026-06-15
**Status:** Ready for planning

<domain>
## Phase Boundary

Make the **whole SwgClient build path compile x64-clean while the tree is still building 32-bit** — three correctness fronts:
- **BITS-01** — replace x64-illegal x87/SSE inline asm (`__asm`) with intrinsics; tree-wide `__asm` sweep over the build path.
- **BITS-02** — fix pointer/integer truncation defects (C4311/C4312/C4244 class).
- **BITS-03** — audit + correct IFF/TRE and network-message data-layout / serialization-width assumptions.

**No `x64` platform is added in this phase** — that is Phase 33. Phase 31 is source surgery on the still-32-bit committed tree, validated against the existing dual-renderer boot-to-character-select smoke (`rasterMajor=5` and `=11`). The committed shipped 32-bit build must not regress.

**Explicitly out of scope:** adding the `x64` platform to `swg.sln`/`.vcxproj`s (Phase 33), the D3DX→`d3dcompiler_47` port (Phase 32 / SHADER-01), any third-party x64 relink work (Phase 33 / X64-04), and fixing the pre-broken editor apps.

</domain>

<decisions>
## Implementation Decisions

### Validation method (how we generate the worklist + define "x64-clean")
- **D-01:** Stand up a **throwaway, uncommitted scratch `Debug|x64` *compile-only* config** as the compiler-driven worklist generator. It COMPILES the in-scope engine libs (no link — so no x64 third-party libs are needed) and lets `cl.exe` emit the real C4311/C4312/C4244 + inline-asm errors. This converts the audit from grep+eyeballs into a compiler-generated worklist. **It must NOT touch committed `swg.sln`/`.vcxproj`** — the committed x64 platform-add stays Phase 33 scope.
- **D-02:** **Phase-31 acceptance bar** = all in-scope build-path engine libs COMPILE x64-clean in the scratch config. Anything that genuinely cannot be validated without the platform/link infra (e.g. third-party-lib-dependent code) is documented as a **residual worklist handed forward to Phase 33** — not forced clean here.

### Sweep & fix scope
- **D-03:** Scope = **SwgClient boot path + the 4 renderer plugins (gl05/gl06/gl07/gl11) + every engine lib they link** (sharedFoundation, sharedMath, sharedCollision, sharedMemoryManager, sharedDebug, clientGame, sharedNetwork, sharedFile/IFF, etc.). **Editor apps are explicitly OUT** (ShaderBuilder, Headless, DllExport, AnimationEditor/ParticleEditor/SwgGodClient, …) — they are already pre-broken on MSB8066, are not part of the boot invariant, and are not the x64 ship target. Matches the established "validation bar = SwgClient clean + dual-renderer boot, not a green full-solution build" convention.
  - Consequence: `DllExport.cpp` (91 of the 121 `__asm` sites, all `__asm int 3` debug-break traps) is an editor-side helper → **out of scope** under D-03.

### BITS-01 — inline asm port specifics
- **D-04:** **`FloatingPointUnit.cpp`** — port faithfully to `_controlfp`/`_control87`. Rounding-mode and exception-masking bits map cleanly to SSE/MXCSR. **Precision control (P_24/P_53/P_64) has NO SSE equivalent** — `_controlfp` ignores `_MCW_PC` on x64, so `setPrecision()` becomes a **documented no-op on x64**. Document this explicitly as expected x64 behavior and a **VERIFY-01 (door-snap) watch-item** — x64's deterministic SSE float is the working theory for why the door-snap disappears, and the x87 24-bit-precision force is exactly that domain (see CONSULT-43 / `project_cantina_corner_snap_engine_quirk`).
- **D-05:** **`SseMath.cpp`** (13 real `__asm { }` SSE blocks) — **faithful `_mm_*` intrinsic translation**, preserving register-level semantics. Intrinsics compile on BOTH x86 and x64, so there is **no `#ifdef` fork** — the committed 32-bit build gets the same intrinsic code. **Usage-audit each of the 13 routines first**: rewrite the live ones; delete/stub any found dead under modern auto-vectorization.

### BITS-02 — warnings-as-errors enforcement
- **D-06:** Enforce **`/we4311 /we4312 /we4244` in the scratch x64 harness ONLY**. That is where pointer truncation actually fires (x64 pointers are 64-bit; on 32-bit pointer==int so C4311/C4312 barely fire) and where "x64-clean" is defined. The **truncation FIXES land in shared source** (so the committed 32-bit build gets them too), but the **committed 32-bit `.vcxproj` flags and the global `/wd4244` suppression stay exactly as-is** — no warning flood, no regression risk to the shipped build. (Pointer-truncation fixes use proper width-correct types — `uintptr_t`/`intptr_t`/`DWORD_PTR`/`size_t` — not cast-to-silence.)

### BITS-03 — data-layout / serialization audit + regression proof
- **D-07:** **Targeted audit depth.** Audit only the types that actually change width on Windows **LLP64** x64: **pointer-derived fields, `size_t`, `ptrdiff_t`, `time_t`** (note the existing `_USE_32BIT_TIME_T=1` flag — a serialization-width lever) — wherever they appear in `#pragma pack` structs, hardcoded `sizeof()`-driven reads, `memcpy`/blob serialization, or `Archive` put/get of width-sensitive types. **`long` stays 32-bit on LLP64**, so it is NOT a mover. **Skip** the already-explicit fixed-width serialization (`int32`/`float`/explicit `get<T>`) that is the bulk of the IFF/network paths.
- **D-08:** **Regression proof = dual-renderer boot smoke + compile-time guards.** The existing boot-to-character-select smoke under both renderers already exercises TRE/IFF asset load + the login network handshake (proves the live layouts still parse). **PLUS add `static_assert(sizeof(X)==N)` / `offsetof` guards on each serialized/packed struct we touch**, baselined to the current 32-bit sizes — cheap, permanent, catches accidental layout drift forever, and the same asserts later document the intended x64 size.

### Claude's Discretion
- Per-phase task sequencing (asm → truncation → layout, or interleaved), exact structure of the residual-worklist artifact handed to Phase 33, and the mechanical choice of width-correct replacement type at each truncation site (`uintptr_t` vs `DWORD_PTR` vs `size_t`) are left to the planner/executor — the convention is "fix with the correct width-preserving type, never cast-to-silence."

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase requirements & milestone plan
- `.planning/ROADMAP.md` (Phase 31 block + v3.0 dependency-chain rationale) — BITS-01/02/03 success criteria; why 31 front-loads before the Phase 33 x64 link.
- `.planning/REQUIREMENTS.md` §"64-bit Correctness" — BITS-01/02/03 exact wording.
- `.planning/STATE.md` §"v3.0 x64 Port — the plan" + §Decisions — the locked milestone sequencing and core-invariant extension.

### BITS-01 primary sites
- `src/engine/shared/library/sharedFoundation/src/win32/FloatingPointUnit.cpp` — the named x87 `__asm fnstcw/fldcw` site (D-04); precision-control no-op analysis.
- `src/engine/shared/library/sharedMath/src/win32/SseMath.cpp` — 13 inline-asm SSE blocks → `_mm_*` (D-05).
- Build-path `__asm` survivors to sweep (D-03): `sharedCollision/.../CollisionUtils.cpp`, `sharedMath/.../Transform.cpp`, `sharedFoundation/.../Clock.cpp`, `sharedFoundation/.../Fatal.cpp`, `sharedMemoryManager/.../MemoryManager.cpp`, `sharedDebug/.../DebugHelp.cpp`, `sharedDebug/.../ProfilerTimer.cpp`, `clientGame/.../HTTPpost/VeCritsec.hpp`. (Editor-app sites incl. `DllExport.cpp` are OUT per D-03.)

### x64 reference (behavior/availability, NOT a source diff)
- `D:\SWG Restoration\x64\` — stable x64 D3D9 client; **binaries only** (per `reference_restoration_binaries_intel`). Use as a door-snap/OOM behavior reference, not a source-patch to lift.
- Door-snap root-cause context (VERIFY-01 link for D-04): `.planning/research/CONSULT-43-*.out`; memory `project_cantina_corner_snap_engine_quirk`.

### Build/convention constraints
- `AGENTS.md` §Build — `$env:MSBUILD` (VS18/v145), `/nodeReuse:false`, run from PowerShell, `/FORCE` link-grep (0 `unresolved external symbol`), shared-header ABI cascade trap, editor-apps-pre-broken validation bar.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- **Dual-renderer boot smoke** is the in-place regression gate (D-08) — no new harness needed for the functional proof; `rasterMajor` in `client_d.cfg` selects the renderer (Debug exe → `client_d.cfg`).
- **`_USE_32BIT_TIME_T=1`** compile flag already in the build — a known serialization-width lever to factor into the BITS-03 `time_t` audit (D-07).

### Established Patterns
- **`/FORCE` link masks unresolved externals** → every link gate greps the build log for `unresolved external symbol` (must be 0); MSBuild exit 0 is NOT proof.
- **Shared-header ABI cascade trap** — touching a public struct in a shared header (e.g. adding `static_assert` is safe, but changing layout is not) silently breaks stale plugin DLLs → boot crash. If a serialized struct in a shared header is altered, rebuild ALL plugin `.vcxproj`s (gl05/06/07/11), not just SwgClient.
- **Intrinsics compile both bitnesses** (D-05) — no `#ifdef _M_X64` fork is needed for the SSE-asm→intrinsic rewrite; this keeps the committed 32-bit build on the same code path.

### Integration Points
- Scratch x64 config (D-01) compiles the same `.cpp`/`.h` source the committed 32-bit projects use — fixes are shared-source edits, so the 32-bit build inherits them automatically and is re-validated by the boot smoke (D-08).
- The residual worklist (D-02) is the explicit hand-off artifact into Phase 33 (x64 platform add).

</code_context>

<specifics>
## Specific Ideas

- The scratch x64 harness is a **signal generator, not a deliverable** — keep it local/uncommitted; its value is the compiler worklist, not a checked-in artifact.
- The FPU precision no-op (D-04) is deliberately framed as a **VERIFY-01 hypothesis confirmation**, not a defect: if door-snap clears on x64 as theorized, the lost x87 24-bit precision force is part of *why*.

</specifics>

<deferred>
## Deferred Ideas

- **Committed `/we4311 /we4312` as a permanent tree-wide guardrail** — considered, deferred. Enforcement lives in the scratch x64 config this phase (D-06); a permanent committed guard could be revisited once the x64 platform exists (Phase 33+).
- **Full serialization sweep / golden round-trip fixtures** — considered for BITS-03 proof, deferred in favor of the targeted audit (D-07) + static_assert guards (D-08). Full inventory or round-trip test infra is a heavier follow-up if a layout regression ever surfaces.
- **Editor-app x64 cleanup** (ShaderBuilder/Headless/DllExport/etc.) — out of scope (D-03); a future concern only if/when the editors are revived (they are MSB8066-pre-broken independent of x64).
- Adding the committed `x64` platform, third-party x64 relink, and D3DX removal — Phases 32–33 (already roadmapped).

### Reviewed Todos (not folded)
- `todos/pending/2026-06-13-64bit-x64-port.md` — this is the whole-milestone driver, not a Phase-31-specific todo; it is satisfied across Phases 31–36, not folded into a single phase.

</deferred>

---

*Phase: 31-64-bit-correctness-foundation*
*Context gathered: 2026-06-15*

## Validation Strategy (31-VALIDATION.md)
---
phase: 31
slug: 64-bit-correctness-foundation
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-06-15
---

# Phase 31 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.
> This phase is C++ source surgery with **no unit-test framework in the tree** — validation is
> **compiler-driven + boot-smoke + compile-time `static_assert`**, not a pytest/jest suite.
> The "test framework" is the MSVC toolchain itself (per RESEARCH.md §Validation Architecture).

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | MSVC compiler (`cl.exe`, v145) as the assertion engine — no xUnit harness exists in this repo |
| **Config file** | scratch `src/build/win32/x64-scratch/x64-compile.props` (uncommitted, gitignored) for the x64 worklist; committed `.vcxproj`s for the 32-bit rebuild |
| **Quick run command** | `cl /c /we4311 /we4312 /we4244 <TU>` (scratch x64) per touched translation unit → log greps to 0 C4235/C4311/C4312/C4244 |
| **Full suite command** | `$env:MSBUILD swg.sln /t:Direct3d11;Direct3d9;Direct3d9_ffp;Direct3d9_vsps;SwgClient /p:Configuration=Debug /p:Platform=Win32 /nodeReuse:false` then dual-renderer boot smoke (`rasterMajor=5` and `=11`) |
| **Estimated runtime** | scratch per-TU compile ~seconds; full 5-target 32-bit build + dual boot ~minutes |

---

## Sampling Rate

- **After every task commit:** scratch x64 `cl /c` of the touched TU(s) → 0 C4235 (asm) / C4311 / C4312 / C4244; 32-bit build of the owning lib stays clean.
- **After every plan wave:** full canonical 5-target 32-bit build (`/nodeReuse:false`), link-grep `unresolved external symbol` = 0 (exit 0 ≠ clean under `/FORCE`).
- **Before `/gsd:verify-work`:** all in-scope libs compile x64-clean in the scratch harness (residual deferrals documented for Phase 33) + dual-renderer boot smoke green + all new `static_assert`s pass.
- **Max feedback latency:** per-TU scratch compile (seconds)

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| (populated during planning/execution) | — | — | BITS-01/02/03 | — | N/A (source-correctness) | compile / smoke / static_assert | see Phase Requirements → Test Map below | ⬜ pending | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

### Phase Requirements → Test Map (from RESEARCH.md)

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| BITS-01 | No x64-illegal `__asm` survivors in build path | compile (x64 scratch) | scratch x64 `cl /c` of each in-scope lib → grep log for C4235 (must be 0) | ✅ harness = Wave 0 |
| BITS-01 | FPU/SSE/asm replacements preserve 32-bit behavior | smoke | 32-bit build + dual-renderer boot to char-select | ✅ existing |
| BITS-02 | No pointer/int truncation survivors | compile (x64 scratch) | scratch x64 `cl /c /we4311 /we4312 /we4244` → grep log for C4311/C4312/C4244 (must be 0) | ✅ harness = Wave 0 |
| BITS-03 | Packed/serialized struct sizes unchanged | compile-time assert | 32-bit build compiles with new `static_assert(sizeof==N)` green | ✅ asserts added in-phase |
| BITS-03 | Assets load + saved data + network parse | smoke | boot to char-select (exercises TRE/IFF load + login handshake) under both renderers | ✅ existing |
| SC#4 | 32-bit boots both renderers after edits | smoke | `rasterMajor=5` then `=11` in `client_d.cfg`, Debug exe, boot to char-select | ✅ existing |

---

## Wave 0 Requirements

- [ ] `src/build/win32/x64-scratch/x64-compile.props` — the D-01 scratch x64 compile-only config (uncommitted, gitignored). Sets x64 target, `/we4311 /we4312 /we4244`, `PLATFORM_WIN32` + `_USE_32BIT_TIME_T=1` preprocessor parity, in-scope include dirs.
- [ ] `src/build/win32/x64-scratch/compile-all.ps1` — iterate the in-scope lib TU list, `cl /c` each, collect C4235/C4311/C4312/C4244 into the worklist.
- [ ] In-scope lib/TU manifest — the explicit list of `.cpp` to feed the harness (RESEARCH.md §Build-Graph Scope).
- [ ] No xUnit framework needed/added — the compiler + boot smoke + `static_assert` are the validation surface for C++ source-correctness work.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Dual-renderer boot to character select | BITS-01/02/03, SC#4 | No automated game-boot harness; rendering/asset-load correctness is visual | Set `rasterMajor=5` in `stage/client_d.cfg`, launch `SwgClient_d.exe`, confirm boot to character select; repeat with `rasterMajor=11`. |
| Door-snap watch-item (VERIFY-01, D-04) | BITS-01 | x87 precision-control no-op is an x64-build observation, not validatable in 32-bit | Note only — informational watch-item for Phase 33+ x64 build; no Phase 31 gate. |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify (scratch-x64 compile / 32-bit build / static_assert) or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references (scratch harness props + compile-all script + TU manifest)
- [ ] No watch-mode flags
- [ ] Feedback latency < per-TU scratch compile (seconds)
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending

## Plans to Review

### 31-01-PLAN.md

---
phase: 31-64-bit-correctness-foundation
plan: 01
type: execute
wave: 1
depends_on: []
files_modified:
  - .gitignore
  - src/build/win32/x64-scratch/x64-compile.props
  - src/build/win32/x64-scratch/compile-all.ps1
  - src/build/win32/x64-scratch/in-scope-tus.txt
autonomous: true
requirements: [BITS-01, BITS-02, BITS-03]
must_haves:
  truths:
    - "Running the scratch driver compiles an in-scope lib TU to x64 with cl /c and emits a worklist log (D-01 — the compiler-driven worklist generator)"
    - "The scratch harness enforces /we4311 /we4312 /we4244 in the scratch x64 config ONLY; committed 32-bit .vcxproj flags are untouched (D-06)"
    - "The scratch harness lives under src/build/win32/x64-scratch/ and is gitignored (git status shows none of it)"
    - "The committed swg.sln still has zero x64 entries (untouched)"
    - "The scratch compile carries _USE_32BIT_TIME_T=1 so time_t stays 32-bit on x64"
  artifacts:
    - path: "src/build/win32/x64-scratch/x64-compile.props"
      provides: "Shared x64 compile-only settings (/we4311 /we4312 /we4244, defines parity, includes)"
    - path: "src/build/win32/x64-scratch/compile-all.ps1"
      provides: "Driver that cl /c each in-scope TU targeting x64 and greps the worklist"
    - path: "src/build/win32/x64-scratch/in-scope-tus.txt"
      provides: "Explicit in-scope lib/TU manifest (D-03 build-graph scope)"
    - path: ".gitignore"
      provides: "Ignore line for src/build/win32/x64-scratch/"
      contains: "x64-scratch"
  key_links:
    - from: "compile-all.ps1"
      to: "Hostx64/x64 cl.exe"
      via: "per-TU /c invocation with /we4311 /we4312 /we4244"
      pattern: "cl.*\\/c.*\\/we4311"
    - from: "x64-compile.props"
      to: "committed lib .vcxproj defines"
      via: "preprocessor + include parity (_USE_32BIT_TIME_T=1, PLATFORM_WIN32, stdcpp20)"
      pattern: "_USE_32BIT_TIME_T"
---

<objective>
Stand up the throwaway, uncommitted scratch `Debug|x64` compile-only harness (D-01) — the
compiler-driven worklist generator that every later fix-wave samples against. It compiles the
in-scope engine-lib translation units to x64 with `cl /c` (no link → no x64 third-party libs
needed) and lets `cl.exe` emit the real C4235 (inline asm) + C4311/C4312/C4244 (truncation, as
errors) worklist. This converts the audit from grep+eyeballs into a deterministic compiler worklist.

Purpose: This is the Wave-1 enabler. BITS-02 truncation enumeration (plan 04) and the BITS-01
asm-survivor proof (plans 02–03) are validated against this harness; the phase gate (plan 06) runs
the full sweep through it. The harness MUST NOT touch the committed `swg.sln`/`.vcxproj` (the
committed x64 platform-add is Phase 33 scope per D-01).

Output: An uncommitted, gitignored `src/build/win32/x64-scratch/` directory containing the props,
the PowerShell driver, and the in-scope TU manifest — plus a `.gitignore` line so it never enters a commit.
</objective>

<execution_context>
@D:/Code/swg-client/.claude/get-shit-done/workflows/execute-plan.md
@D:/Code/swg-client/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@.planning/STATE.md
@.planning/ROADMAP.md
@.planning/phases/31-64-bit-correctness-foundation/31-CONTEXT.md
@.planning/phases/31-64-bit-correctness-foundation/31-RESEARCH.md
@.planning/phases/31-64-bit-correctness-foundation/31-PATTERNS.md
@.planning/phases/31-64-bit-correctness-foundation/31-VALIDATION.md
@AGENTS.md

<interfaces>
<!-- The harness must replicate the committed 32-bit ClCompile settings. Read out of any in-scope lib's .vcxproj. -->
From src/engine/shared/library/sharedMath/build/win32/sharedMath.vcxproj (Debug|Win32 ClCompile, ~lines 69-94):
  PreprocessorDefinitions: WIN32;_DEBUG;_MBCS;DEBUG_LEVEL=2;_CRT_SECURE_NO_DEPRECATE=1;_USE_32BIT_TIME_T=1;_LIB
  LanguageStandard: stdcpp20
  RuntimeTypeInfo: true ; WarningLevel: Level4 ; TreatWarningAsError: false  (scratch flips to /we4311 /we4312 /we4244 ONLY)
  AdditionalIncludeDirectories: long per-lib relative list (read per lib — they differ)
Note: lib .vcxproj files live in per-lib build/win32/ dirs (e.g. sharedMath/build/win32/sharedMath.vcxproj),
NOT src/build/win32/ (which holds only swg.sln + _all_*.vcxproj aggregators).
$env:MSBUILD points at the VS18/v145 MSBuild; the matching x64 cl.exe is under
<VS18>/VC/Tools/MSVC/<ver>/bin/Hostx64/x64/cl.exe.
</interfaces>
</context>

<tasks>

<task type="auto">
  <name>Task 1: Gitignore the scratch dir + create the in-scope TU manifest</name>
  <read_first>
    - .gitignore (repo root — confirm current ignore style)
    - .planning/phases/31-64-bit-correctness-foundation/31-RESEARCH.md §"Build-Graph Scope (D-03 — in-scope libs to feed the scratch harness)"
    - .planning/phases/31-64-bit-correctness-foundation/31-CONTEXT.md §decisions D-01/D-02/D-03
  </read_first>
  <action>
    Add a single line `src/build/win32/x64-scratch/` to `.gitignore` (repo root) so the entire scratch
    harness is uncommitted (D-01: "signal generator, not a deliverable"). Create
    `src/build/win32/x64-scratch/in-scope-tus.txt` listing the in-scope libs per D-03 (RESEARCH
    §Build-Graph Scope): sharedFoundation, sharedMath, sharedCollision, sharedMemoryManager,
    sharedDebug, sharedFile, sharedImage, sharedGame, clientGame, clientGraphics,
    clientSkeletalAnimation, external/ours/library/archive, Direct3d9 (gl05/06/07), Direct3d11 (gl11),
    plus the transitive engine libs they link (sharedNetwork, sharedNetworkMessages, sharedUtility,
    sharedTerrain, sharedObject, …). For the first cut, the manifest can list the lib roots + the
    specific known-defect TUs named in RESEARCH (FloatingPointUnit.cpp, SseMath.cpp, Transform.cpp,
    CollisionUtils.cpp, Fatal.cpp, Clock.cpp, ProfilerTimer.cpp, DebugHelp.cpp, VeCritsec.hpp's owning
    .cpp, StaticShader.cpp, MemoryManager.cpp, Os.cpp, Archive.h's consumers, TargaFormat.cpp,
    AssetCustomizationManager.cpp, Direct3d11_StaticVertexBufferData.cpp,
    Direct3d11_DynamicVertexBufferData.cpp). Explicitly EXCLUDE the D-03 out-of-scope set (DllExport.cpp,
    ShaderBuilder, Headless, AnimationEditor/ParticleEditor/SwgGodClient, editor/tool apps, external/3rd/*).
  </action>
  <verify>
    <automated>git check-ignore src/build/win32/x64-scratch/probe.txt && grep -c "x64-scratch" .gitignore</automated>
  </verify>
  <acceptance_criteria>
    - `git check-ignore src/build/win32/x64-scratch/probe.txt` exits 0 (the path is ignored)
    - `grep -c x64-scratch .gitignore` returns >= 1
    - `in-scope-tus.txt` lists the D-03 in-scope libs and EXCLUDES the named editor/tool/3rd-party set
    - `grep -c 'DllExport\|ShaderBuilder\|AnimationEditor' src/build/win32/x64-scratch/in-scope-tus.txt` returns 0
  </acceptance_criteria>
  <done>Scratch dir is gitignored; in-scope TU manifest exists and matches D-03 scope (in-scope listed, editor/3rd-party excluded).</done>
</task>

<task type="auto">
  <name>Task 2: Author the x64-compile.props parity settings</name>
  <read_first>
    - src/engine/shared/library/sharedMath/build/win32/sharedMath.vcxproj (Debug|Win32 ClCompile block, ~lines 69-94 — the settings to mirror)
    - .planning/phases/31-64-bit-correctness-foundation/31-PATTERNS.md §"NEW scratch harness .props — structural analog" (the exact define/standard list to carry)
    - .planning/phases/31-64-bit-correctness-foundation/31-RESEARCH.md §"Scratch x64 Compile-Only Harness — concrete HOW (D-01)"
  </read_first>
  <action>
    Create `src/build/win32/x64-scratch/x64-compile.props` carrying the committed 32-bit ClCompile
    parity the scratch x64 compile needs: PreprocessorDefinitions
    `PLATFORM_WIN32;WIN32;_DEBUG;_MBCS;DEBUG_LEVEL=2;_CRT_SECURE_NO_DEPRECATE=1;_USE_32BIT_TIME_T=1;_LIB`
    (the `_USE_32BIT_TIME_T=1` is CRITICAL — without it `time_t` widens to 8 bytes on x64 and breaks the
    BITS-03 size asserts; `PLATFORM_WIN32` drives the `#pragma pack` macros in TargaFormat.cpp), language
    standard `stdcpp20`, `ConfigurationType=StaticLibrary` (compile, no app link), and the scratch-only
    deltas: target x64, `/c` compile-only, `/Y-` (no PCH), and the warnings-as-errors set
    `/we4311 /we4312 /we4244` (D-06 — enforced in the scratch harness ONLY; committed 32-bit flags
    untouched). Note that NO `/wd4244` suppression exists in the committed tree (research correction 3) —
    C4244 fires today only as a non-fatal Level-4 warning; the scratch harness is where it becomes an error.
    The per-lib `AdditionalIncludeDirectories` differ per lib and are read out of each lib's own .vcxproj
    by the driver (Task 3), not hardcoded here.
  </action>
  <verify>
    <automated>grep -c "_USE_32BIT_TIME_T=1" src/build/win32/x64-scratch/x64-compile.props && grep -c "we4311" src/build/win32/x64-scratch/x64-compile.props</automated>
  </verify>
  <acceptance_criteria>
    - `grep -c _USE_32BIT_TIME_T=1 x64-compile.props` returns >= 1 (time_t parity preserved)
    - `grep -c 'we4311.*we4312.*we4244\|we4311' x64-compile.props` returns >= 1 (D-06 warnings-as-errors)
    - props carries `PLATFORM_WIN32` and `stdcpp20`
    - props sets `/Y-` (no PCH) and `/c` (compile-only, no link)
  </acceptance_criteria>
  <done>x64-compile.props mirrors the committed 32-bit defines/standard, carries _USE_32BIT_TIME_T=1 + PLATFORM_WIN32, and adds the scratch-only x64 + /we4311 /we4312 /we4244 + /Y- + /c deltas.</done>
</task>

<task type="auto">
  <name>Task 3: Author the compile-all.ps1 driver and prove it on one defect TU</name>
  <read_first>
    - .planning/phases/31-64-bit-correctness-foundation/31-RESEARCH.md §"Scratch x64 Compile-Only Harness — concrete HOW (D-01)" (the PowerShell sketch + the worklist grep)
    - src/build/win32/x64-scratch/in-scope-tus.txt (from Task 1)
    - src/build/win32/x64-scratch/x64-compile.props (from Task 2)
    - AGENTS.md §Build (run builds from PowerShell, $env:MSBUILD, /nodeReuse:false)
  </read_first>
  <action>
    Create `src/build/win32/x64-scratch/compile-all.ps1` that: resolves the `Hostx64\x64\cl.exe` from
    the VS18/v145 toolchain (derive from `$env:MSBUILD` or the VC Tools MSVC version dir), reads each
    in-scope lib's `AdditionalIncludeDirectories` out of its own `build/win32/*.vcxproj`, then for each TU
    in `in-scope-tus.txt` invokes `cl /c` with the props' common flags + the lib's includes, targeting
    x64, teeing output to `worklist.log`. Finish by `Select-String 'error C4235|error C4311|error
    C4312|error C4244' worklist.log` — that filtered output IS the worklist. Prove the harness works by
    compiling ONE known-defect TU end-to-end: `StaticShader.cpp` (a confirmed C4311 site,
    `reinterpret_cast<int>(&getStaticShaderTemplate())` at :289) — it MUST surface `error C4311` (proving
    truncation-as-error fires) AND/OR a `__asm`-bearing TU MUST surface `error C4235`. Use the worklist
    grep pattern `grep -v '^#'`-style filtering if the log header would otherwise self-match; count only
    actual `error C####` lines.
  </action>
  <verify>
    <automated>powershell -File src/build/win32/x64-scratch/compile-all.ps1 -SingleTu StaticShader.cpp 2>&1 | grep -E "error C4311|error C4235"</automated>
  </verify>
  <acceptance_criteria>
    - Running the driver on `StaticShader.cpp` (or a named __asm TU) emits at least one `error C4311` or `error C4235` line to `worklist.log` (proving the harness surfaces defects as errors)
    - The driver invokes a `Hostx64\x64` cl.exe (x64 target → `_M_X64` set → `__asm` becomes C4235)
    - The driver runs `/c` only (no `/link`) — no x64 third-party .lib is required
    - `git status --porcelain src/build/win32/x64-scratch/` shows the scratch files are NOT tracked (gitignored)
    - `grep -c x64 src/build/win32/swg.sln` returns 0 (committed solution untouched)
  </acceptance_criteria>
  <done>compile-all.ps1 cl /c-compiles an in-scope TU to x64, produces worklist.log filtered to C4235/C4311/C4312/C4244, demonstrably surfaces a known defect as an error, and the whole harness is gitignored with swg.sln unmodified.</done>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| build-tooling → committed tree | The scratch harness must NEVER mutate committed swg.sln/.vcxproj (D-01); accidental commit of x64 platform entries would pre-empt Phase 33 and corrupt the 32-bit build contract |
| compile-flag parity → BITS-03 size asserts | Missing `_USE_32BIT_TIME_T=1` in the harness silently widens `time_t` to 8 bytes on x64, making later BITS-03 size asserts baseline to the wrong value |

## STRIDE Threat Register

| Threat ID | Category | Component | Disposition | Mitigation Plan |
|-----------|----------|-----------|-------------|-----------------|
| T-31-01 | Tampering | committed swg.sln / .vcxproj | mitigate | Task 3 acceptance asserts `grep -c x64 swg.sln == 0` and scratch dir untracked; .gitignore line (Task 1) makes accidental staging impossible |
| T-31-02 | Tampering | time_t serialization width | mitigate | props carries `_USE_32BIT_TIME_T=1` (Task 2 acceptance greps for it); preserves 32-bit `time_t` so later size asserts are baselined correctly |
| T-31-03 | Info disclosure | none (local build tooling, no network/PII) | accept | Compile-only harness produces only local .obj + a log; no external surface |
</threat_model>

<verification>
- `git check-ignore` confirms scratch dir ignored; `.gitignore` carries the line.
- Driver run on a known-defect TU emits `error C4311`/`error C4235` (harness surfaces the worklist).
- `grep -c x64 src/build/win32/swg.sln` == 0 (committed solution untouched).
- Props carries `_USE_32BIT_TIME_T=1`, `PLATFORM_WIN32`, `stdcpp20`, `/we4311 /we4312 /we4244`, `/Y-`, `/c`.
</verification>

<success_criteria>
The scratch x64 compile-only harness exists, is gitignored, replicates the committed 32-bit define/standard parity (incl. `_USE_32BIT_TIME_T=1`), targets x64 with `/c` + `/we4311 /we4312 /we4244`, and demonstrably emits a C4235/C4311 worklist on a known-defect TU — without touching the committed `swg.sln`/`.vcxproj`.
</success_criteria>

<output>
After completion, create `.planning/phases/31-64-bit-correctness-foundation/31-01-SUMMARY.md`
</output>


### 31-02-PLAN.md

---
phase: 31-64-bit-correctness-foundation
plan: 02
type: execute
wave: 2
depends_on: [01]
files_modified:
  - src/engine/shared/library/sharedFoundation/src/win32/FloatingPointUnit.cpp
  - src/engine/shared/library/sharedMath/src/win32/SseMath.cpp
  - src/engine/shared/library/sharedMath/src/shared/Transform.cpp
autonomous: true
requirements: [BITS-01]
must_haves:
  truths:
    - "FloatingPointUnit's x87 __asm fnstcw/fldcw is replaced with _control87; the precision write is guarded out on x64 (not a silent no-op) (D-04)"
    - "SseMath's CPUID check + 4 SSE math routines are register-faithful _mm_* intrinsics, compiling on both x86 and x64 with no #ifdef fork (D-05)"
    - "Transform::sse_xf_matrix_3x4 is a normal _mm_* function (naked dropped), still selected by Transform::install when SSE is available"
    - "The 32-bit build still boots to character select under rasterMajor=5 and =11 with skinned characters rendering correctly"
  artifacts:
    - path: "src/engine/shared/library/sharedFoundation/src/win32/FloatingPointUnit.cpp"
      provides: "_control87-based FP control word read/write; x64-guarded precision write"
      contains: "_control87"
    - path: "src/engine/shared/library/sharedMath/src/win32/SseMath.cpp"
      provides: "_mm_* intrinsic SSE math (canDoSseMath via __cpuid + 4 transform routines)"
      contains: "_mm_"
    - path: "src/engine/shared/library/sharedMath/src/shared/Transform.cpp"
      provides: "sse_xf_matrix_3x4 as a normal _mm_* function"
      contains: "_mm_"
  key_links:
    - from: "Transform::install"
      to: "sse_xf_matrix_3x4"
      via: "SseMath::canDoSseMath() install selector (Transform.cpp:76-78)"
      pattern: "canDoSseMath"
    - from: "SoftwareBlendSkeletalShaderPrimitive"
      to: "SseMath::*_l2p"
      via: "s_useSSE runtime branch (skinning hot path)"
      pattern: "s_useSSE"
---

<objective>
BITS-01, the hard chunk: port the x87 FPU-control inline asm (D-04) and the SSE math inline asm
(D-05) to MSVC intrinsics. Three sites: `FloatingPointUnit.cpp` (`__asm fnstcw`/`fldcw` →
`_control87`), `SseMath.cpp` (CPUID check + 4 matrix-transform routines, 13 `__asm{}` blocks →
`_mm_*`), and `Transform.cpp`'s `__declspec(naked)` `sse_xf_matrix_3x4` (the LIVE primary
matrix-concat path — research correction 2 flags it as a PEER of SseMath, not a trivial sweep).

Purpose: x64 forbids inline asm (always error C4235). Intrinsics compile on BOTH bitnesses, so the
fixes land in shared source with no `#ifdef` fork (D-05) — the committed 32-bit build inherits the
same intrinsic code and is re-validated by the dual-renderer boot smoke. The SSE path is LIVE
(skeletal skinning + matrix concat); rewrite, never delete (Pitfall 4).

Output: Three intrinsic-ported files that compile x64-clean in the scratch harness (0 C4235) and keep
the 32-bit build booting both renderers with correct skinning.
</objective>

<execution_context>
@D:/Code/swg-client/.claude/get-shit-done/workflows/execute-plan.md
@D:/Code/swg-client/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@.planning/STATE.md
@.planning/ROADMAP.md
@.planning/phases/31-64-bit-correctness-foundation/31-CONTEXT.md
@.planning/phases/31-64-bit-correctness-foundation/31-RESEARCH.md
@.planning/phases/31-64-bit-correctness-foundation/31-PATTERNS.md
@AGENTS.md

<interfaces>
<!-- Verified sites + their intrinsic targets. Bodies are in RESEARCH §Patterns 1-2 — do NOT inline code here. -->
FloatingPointUnit.cpp:
  :119  WORD getControlWord(void)  { __asm fnstcw controlWord; }   -> _control87(0,0)
  :128  void setControlWord(WORD)  { __asm fldcw controlWord; }    -> _control87(cw, _MCW_RC | _MCW_EM)  (NO _MCW_PC on x64)
  :133  void setPrecision(Precision) — the precision FPU write MUST be guarded out on x64 (research correction 1)
SseMath.cpp:
  :34   bool canDoSseMath()  __asm{ mov eax,1; cpuid; mov featureBits,edx }  -> __cpuid/__cpuidex, EDX bit25=SSE, bit24=FXSR
  :97+  4 transform routines (rotateTranslateScale_l2p, rotateScale_l2p, skinPositionNormal_l2p, skinPositionNormalAdd_l2p), 13 __asm{} total -> _mm_load_ps/_mm_mul_ps/_mm_set1_ps/_mm_shuffle_ps/_mm_add_ps
Transform.cpp:
  :275  __declspec(naked) void sse_xf_matrix_3x4(float *out, const float *left, const float *right)  -> normal _mm_* fn, drop naked
  :76-78  Transform::install selects sse_xf_matrix_3x4 when SseMath::canDoSseMath() (DO NOT break this selector)
SoftwareBlendSkeletalShaderPrimitive.cpp:365,1569,1592  branch on s_useSSE = SseMath::canDoSseMath() (skinning hot path; the live consumer)
</interfaces>
</context>

<tasks>

<task type="auto">
  <name>Task 1: Port FloatingPointUnit FPU-control asm to _control87 (D-04 + research correction 1)</name>
  <read_first>
    - src/engine/shared/library/sharedFoundation/src/win32/FloatingPointUnit.cpp (the full file — getControlWord :115, setControlWord :125, setPrecision :133, plus ROUND_*/EXCEPTION_*/PRECISION_* mask definitions and the Precision enum)
    - .planning/phases/31-64-bit-correctness-foundation/31-RESEARCH.md §"Pattern 1: FPU control-word port" AND §"Pitfall 1: _controlfp(_, _MCW_PC) assertion on x64" (the CRITICAL correction to D-04)
    - .planning/phases/31-64-bit-correctness-foundation/31-PATTERNS.md §"BITS-01 intrinsic ports — house-style" (FloatingPointUnit row)
  </read_first>
  <action>
    Replace `__asm fnstcw controlWord;` (:119) in `getControlWord` with `return
    static_cast<WORD>(_control87(0, 0));` (mask 0 = read-only). Replace `__asm fldcw controlWord;` (:128)
    in `setControlWord` with `_control87(controlWord, _MCW_RC | _MCW_EM)` — apply ONLY the rounding-mode
    and exception-mask bits; do NOT pass `_MCW_PC`. Use `_control87` (not `_controlfp`) to preserve the
    DENORMAL-mask behavior the x87 code intended (research §Alternatives note). For `setPrecision` (:133):
    record the `precision` member for state-query parity but GUARD OUT the precision FPU write on x64 —
    `#if !defined(_M_X64)` keep the existing x87 precision-bits write; on x64 do not call
    `_control87(_, _MCW_PC)` (it raises an assertion + invokes the invalid-parameter handler — research
    correction 1; it is NOT a silent no-op). Never use `__control87_2` (hard compiler error on x64).
    Add a comment documenting the x64 precision-control loss as the VERIFY-01 (door-snap) watch-item per
    D-04. `#include <float.h>` for `_control87`/`_MCW_*`.
  </action>
  <verify>
    <automated>powershell -File src/build/win32/x64-scratch/compile-all.ps1 -SingleTu FloatingPointUnit.cpp 2>&1 | grep -cE "error C4235|error C4311|error C4312|error C4244"</automated>
  </verify>
  <acceptance_criteria>
    - scratch x64 `cl /c /we4311 /we4312 /we4244` of FloatingPointUnit.cpp emits 0 C4235 (asm gone) and 0 C4311/C4312/C4244 — the grep count above is 0
    - `grep -c _control87 FloatingPointUnit.cpp` >= 2 (read + write)
    - `grep -c '__asm' FloatingPointUnit.cpp` == 0
    - `grep -c _MCW_PC FloatingPointUnit.cpp` confirms _MCW_PC is NOT passed unguarded on x64 (it appears only inside a `#if !defined(_M_X64)` block, or not at all)
    - The setPrecision x64 path does not call `_control87(_, _MCW_PC)` nor `__control87_2`
  </acceptance_criteria>
  <done>FloatingPointUnit reads/writes the FP control word via _control87, applies only _MCW_RC|_MCW_EM, guards the precision write out of the x64 path (documented VERIFY-01 watch-item), and compiles x64-clean (0 C4235).</done>
</task>

<task type="auto">
  <name>Task 2: Port SseMath CPUID check + 4 transform routines to _mm_* / __cpuid intrinsics (D-05)</name>
  <read_first>
    - src/engine/shared/library/sharedMath/src/win32/SseMath.cpp (full file — canDoSseMath :34 and the 4 math routines from :97; note the register layout the asm uses so the intrinsic translation is register-faithful)
    - .planning/phases/31-64-bit-correctness-foundation/31-RESEARCH.md §"Pattern 2: SSE inline-asm → _mm_* intrinsics" (the load-matrix + transform-multiply core; the hsum4 helper note; the per-routine w-lane differences) AND §"Pitfall 4: Mistaking the SSE path for dead code"
    - .planning/phases/31-64-bit-correctness-foundation/31-PATTERNS.md §"BITS-01 intrinsic ports" (SseMath rows) and §"No Analog Found" (RESEARCH spec IS the analog — no in-tree _mm_* to copy)
  </read_first>
  <action>
    USAGE-AUDIT FIRST (D-05): confirm each of the 5 functions is live before rewriting. canDoSseMath drives
    Transform::install + s_useSSE — live; the 4 math routines are the skinning/matrix path — live (Pitfall
    4 confirms; do NOT delete any as dead). Replace canDoSseMath's `__asm{ mov eax,1; cpuid; mov
    featureBits,edx }` (:57-72) with `__cpuid` (or `__cpuidex`) reading EDX bit 25 (SSE) and bit 24 (FXSR);
    keep the surrounding try/catch + return logic. Rewrite the 4 transform routines
    (rotateTranslateScale_l2p, rotateScale_l2p, skinPositionNormal_l2p, skinPositionNormalAdd_l2p)
    register-faithfully to `_mm_load_ps`/`_mm_mul_ps`/`_mm_set1_ps`/`_mm_shuffle_ps`/`_mm_add_ps` per
    RESEARCH Pattern 2 — load the 3×4 matrix rows, broadcast scale, multiply, horizontal-sum the lanes
    (a small `hsum4` helper). Honor each routine's w-lane semantics: rotateScale sets the 4th source lane
    to 0.0f and sums 3 lanes; the skin variants apply to a position (w=1) and a normal, with the Add variant
    accumulating into the out param. NO `#ifdef _M_X64` fork (D-05 — intrinsics compile both bitnesses).
    `#include <xmmintrin.h>` and `<intrin.h>` (for __cpuid).
  </action>
  <verify>
    <automated>powershell -File src/build/win32/x64-scratch/compile-all.ps1 -SingleTu SseMath.cpp 2>&1 | grep -cE "error C4235|error C4311|error C4312|error C4244"</automated>
  </verify>
  <acceptance_criteria>
    - scratch x64 `cl /c` of SseMath.cpp emits 0 C4235 — the grep count above is 0
    - `grep -c '__asm' SseMath.cpp` == 0
    - `grep -c '_mm_' SseMath.cpp` >= 4 (the math routines use SSE intrinsics)
    - `grep -c '__cpuid' SseMath.cpp` >= 1 (CPUID feature check ported)
    - No routine deleted: all 4 `*_l2p` function definitions + `canDoSseMath` still present (`grep -c '_l2p' SseMath.cpp` unchanged in count of definitions)
    - No `#ifdef _M_X64`/`#if defined(_M_X64)` fork added in SseMath.cpp (D-05)
  </acceptance_criteria>
  <done>SseMath's CPUID check and 4 transform routines are register-faithful _mm_*/__cpuid intrinsics, no __asm remains, no routine deleted, no bitness fork, and the TU compiles x64-clean (0 C4235).</done>
</task>

<task type="auto">
  <name>Task 3: Port Transform::sse_xf_matrix_3x4 naked-SSE to a normal _mm_* function + 32-bit dual-renderer boot smoke</name>
  <read_first>
    - src/engine/shared/library/sharedMath/src/shared/Transform.cpp (sse_xf_matrix_3x4 :275 — the full naked asm body; AND Transform::install :76-78 — the canDoSseMath() install selector that wires it in)
    - .planning/phases/31-64-bit-correctness-foundation/31-RESEARCH.md §"Pattern 2" (the matrix-multiply core applies here too) AND §"__asm Survivor Inventory" row 16 (Transform naked-SSE is the LIVE primary matrix-concat path — peer of SseMath, research correction 2)
    - .planning/phases/31-64-bit-correctness-foundation/31-PATTERNS.md §"BITS-01 intrinsic ports" (Transform row) and §"Validation / regression (all fronts, D-08)"
    - AGENTS.md §Build (canonical 5-target build, /nodeReuse:false, /FORCE link-grep) and §"Run / renderer switch" (rasterMajor in client_d.cfg for Debug exe) and §"Config (.cfg) safety" (Edit/Write, never PS Set-Content — BOM crash)
  </read_first>
  <action>
    Rewrite `sse_xf_matrix_3x4(float *out, const float *left, const float *right)` (:275) as a NORMAL
    C++ function (drop `__declspec(naked)`, remove the manual `push`/`esp` stack management and the
    `__asm{}` body) doing a register-faithful 3×4 matrix concatenation in `_mm_*` intrinsics — load left's
    rows, multiply/accumulate against right's columns, store to `out`. It is the LIVE matrix-concat path
    selected by `Transform::install` (:76-78) when `SseMath::canDoSseMath()` — do NOT remove the selector
    wiring, the function must keep the same signature and observable result. No `#ifdef _M_X64` fork
    (intrinsics compile both bitnesses). `#include <xmmintrin.h>`. THEN run the 32-bit regression gate for
    plan 02's three files: full canonical 5-target Debug build from PowerShell with `$env:MSBUILD ...
    /p:Platform=Win32 /nodeReuse:false`, delete `SwgClient_d.exe` first to force a relink, grep the build
    log for `unresolved external symbol` (must be 0; `/FORCE` masks them). Then dual-renderer boot smoke:
    set `rasterMajor=5` in `stage/client_d.cfg` (Edit/Write — never PS Set-Content, BOM crashes), launch
    `SwgClient_d.exe`, confirm boot to character select with skinned characters correct; repeat with
    `rasterMajor=11`.
  </action>
  <verify>
    <automated>powershell -File src/build/win32/x64-scratch/compile-all.ps1 -SingleTu Transform.cpp 2>&1 | grep -cE "error C4235|error C4311|error C4312|error C4244"</automated>
  </verify>
  <acceptance_criteria>
    - scratch x64 `cl /c` of Transform.cpp emits 0 C4235 — the grep count above is 0
    - `grep -c '__declspec(naked)' Transform.cpp` == 0 (naked dropped); `grep -c '_mm_' Transform.cpp` >= 1
    - Transform::install still selects sse_xf_matrix_3x4 via canDoSseMath() (`grep -c canDoSseMath Transform.cpp` unchanged)
    - 32-bit canonical 5-target Debug build: build-log grep `unresolved external symbol` == 0 (delete exe first to force relink)
    - SwgClient_d.exe boots to character select under rasterMajor=5 AND rasterMajor=11 (D-08 dual-renderer smoke) with skinned characters rendering correctly (Pitfall 4 — SSE skinning not broken)
  </acceptance_criteria>
  <done>sse_xf_matrix_3x4 is a normal _mm_* function (naked dropped) still wired by Transform::install, compiles x64-clean, and the 32-bit build links clean (0 unresolved) and boots both renderers to character select with correct skinning.</done>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| FP precision control → float codegen determinism | The x64 precision-write guard-out (D-04) changes FP behavior on x64 (the VERIFY-01 door-snap theory); on 32-bit the precision path is preserved, so no behavior change ships now |
| SSE intrinsic translation → skinning/matrix correctness | A non-register-faithful _mm_* translation silently corrupts skinned-character vertices or matrix concatenation — a data-integrity (not memory-safety) hazard validated by the boot smoke |

## STRIDE Threat Register

| Threat ID | Category | Component | Disposition | Mitigation Plan |
|-----------|----------|-----------|-------------|-----------------|
| T-31-04 | Tampering | _control87 precision write on x64 | mitigate | Task 1 guards `_MCW_PC` out of the x64 path entirely; never `__control87_2`; acceptance greps confirm no unguarded _MCW_PC |
| T-31-05 | Denial of Service | skinning/matrix math correctness | mitigate | Register-faithful translation (D-05) + Task 3 dual-renderer boot smoke confirms skinned characters render (Pitfall 4 — the SSE path is live, not dead) |
| T-31-06 | Tampering | SseMath dead-code deletion | mitigate | D-05 usage-audit + acceptance assert that all 4 *_l2p routines + canDoSseMath remain (no deletion) |
</threat_model>

<verification>
- Per-task scratch x64 `cl /c` of each touched TU: 0 C4235/C4311/C4312/C4244.
- No `__asm`/`__declspec(naked)` survivors in the three files; no `#ifdef _M_X64` fork (intrinsics both bitnesses).
- SSE path preserved (no routine deleted; install selector intact).
- 32-bit canonical 5-target build links clean (0 unresolved external symbol) + dual-renderer boot to character select with correct skinning.
</verification>

<success_criteria>
BITS-01's FPU + SSE inline asm is ported to intrinsics: FloatingPointUnit uses `_control87` with the
x64 precision write guarded out, SseMath's CPUID check + 4 transform routines and Transform's
`sse_xf_matrix_3x4` are register-faithful `_mm_*`/`__cpuid` intrinsics (no `__asm`, no `naked`, no
bitness fork). All three TUs compile x64-clean (0 C4235); the 32-bit build links clean and boots both
renderers to character select with correct skeletal skinning.
</success_criteria>

<output>
After completion, create `.planning/phases/31-64-bit-correctness-foundation/31-02-SUMMARY.md`
</output>


### 31-03-PLAN.md

---
phase: 31-64-bit-correctness-foundation
plan: 03
type: execute
wave: 2
depends_on: [01]
files_modified:
  - src/engine/shared/library/sharedCollision/src/shared/core/CollisionUtils.cpp
  - src/engine/shared/library/sharedFoundation/src/shared/Fatal.cpp
  - src/engine/shared/library/sharedFoundation/src/shared/Clock.cpp
  - src/engine/shared/library/sharedDebug/src/win32/ProfilerTimer.cpp
  - src/engine/shared/library/sharedDebug/src/win32/DebugHelp.cpp
  - src/engine/client/library/clientGame/src/shared/HTTPpost/VeCritsec.hpp
autonomous: true
requirements: [BITS-01]
must_haves:
  truths:
    - "The scalar/instruction __asm sites (fsqrt, int 3, rdtsc, lock bts) are replaced with sqrtf/__debugbreak/__rdtsc/_interlockedbittestandset"
    - "DebugHelp's EIP/ESP/EBP register grab compiles x64-clean via a #if defined(_M_X64) RtlCaptureContext branch (the only bitness fork in the phase) with the 32-bit path unchanged"
    - "VeCritsec's #ifdef _WIN32 MSVC spinlock branch is ported to an intrinsic; the #else GCC __asm__ branch is untouched"
    - "No x64-illegal __asm/naked survivors remain in these six files"
  artifacts:
    - path: "src/engine/shared/library/sharedDebug/src/win32/ProfilerTimer.cpp"
      provides: "__rdtsc() cycle counter (naked dropped)"
      contains: "__rdtsc"
    - path: "src/engine/shared/library/sharedDebug/src/win32/DebugHelp.cpp"
      provides: "x64 RtlCaptureContext stack-walk branch + unchanged 32-bit path"
      contains: "RtlCaptureContext"
    - path: "src/engine/client/library/clientGame/src/shared/HTTPpost/VeCritsec.hpp"
      provides: "_interlockedbittestandset spinlock (MSVC branch)"
      contains: "_interlockedbittestandset"
  key_links:
    - from: "Fatal.cpp / Clock.cpp"
      to: "__debugbreak()"
      via: "breakpoint trap replacing __asm int 3"
      pattern: "__debugbreak"
    - from: "DebugHelp.cpp x64 branch"
      to: "STACKFRAME64 AMD64 fields"
      via: "RtlCaptureContext + IMAGE_FILE_MACHINE_AMD64 (Rip/Rsp/Rbp)"
      pattern: "IMAGE_FILE_MACHINE_AMD64"
---

<objective>
BITS-01 misc asm sweep: replace the remaining in-scope `__asm` survivors with intrinsics across six
files — `CollisionUtils.cpp` (x87 `fsqrt`), `Fatal.cpp` + `Clock.cpp` (`__asm int 3`),
`ProfilerTimer.cpp` (`__declspec(naked)` `rdtsc`), `VeCritsec.hpp` (`lock bts` spinlock), and
`DebugHelp.cpp` (EIP/ESP/EBP register grab — the ONE site that needs a `#if defined(_M_X64)` fork
because the register set is inherently bitness-specific).

Purpose: x64 forbids inline asm (C4235). These are the grep-tractable instruction-level sweep items
(RESEARCH §Patterns 3-4). All but DebugHelp are no-fork intrinsic swaps that compile both bitnesses;
DebugHelp is the justified exception (Rip/Rsp/Rbp vs Eip/Esp/Ebp). `MemoryManager.cpp:2074`'s
`__asm int 3` is ALREADY COMMENTED OUT — no work there (that file's BITS-02 ptr-math is plan 04).

Output: Six files compiling x64-clean (0 C4235) with intrinsic replacements, the 32-bit build
unchanged in behavior (re-validated by the shared dual-renderer smoke in plan 06's gate).
</objective>

<execution_context>
@D:/Code/swg-client/.claude/get-shit-done/workflows/execute-plan.md
@D:/Code/swg-client/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@.planning/STATE.md
@.planning/ROADMAP.md
@.planning/phases/31-64-bit-correctness-foundation/31-CONTEXT.md
@.planning/phases/31-64-bit-correctness-foundation/31-RESEARCH.md
@.planning/phases/31-64-bit-correctness-foundation/31-PATTERNS.md
@AGENTS.md

<interfaces>
<!-- Verified sites + intrinsic targets (RESEARCH §Patterns 3-4 + §__asm Survivor Inventory). Bodies in RESEARCH — do NOT inline. -->
CollisionUtils.cpp:427-429   __asm fld t; fsqrt; fstp t;            -> t = sqrtf(t);   (or _mm_sqrt_ss)   #include <cmath>
Fatal.cpp:174                __asm int 3                            -> __debugbreak();                     #include <intrin.h>
Clock.cpp:270                __asm int 3                            -> __debugbreak();                     #include <intrin.h>
ProfilerTimer.cpp:29-36      static __int64 __declspec(naked) __stdcall readTimeStampCounter(){ __asm{ rdtsc; ret; } }
                                                                    -> return __rdtsc();  (drop naked + __stdcall wrapper as needed)  #include <intrin.h>
VeCritsec.hpp:43-48          #ifdef _WIN32 branch: __asm{ mov esi,[p_i_lock]; lock bts dword ptr[esi],0; jnc Locked }
                                                                    -> _interlockedbittestandset((long*)&m_iLock, 0)   (port ONLY the _WIN32 MSVC branch; the #else GCC __asm__ stays)
DebugHelp.cpp:503-511        __asm{ call GetEIP; GetEIP: pop eax; mov context.Eip,eax; mov context.Esp,esp; mov context.Ebp,ebp }
                                then stackWalk64(IMAGE_FILE_MACHINE_I386, ...)  (Eip/Esp/Ebp)
                                                                    -> #if defined(_M_X64): RtlCaptureContext(&context); AddrPC=Rip/AddrStack=Rsp/AddrFrame=Rbp; IMAGE_FILE_MACHINE_AMD64
                                                                       #else: existing 32-bit asm path UNCHANGED
</interfaces>
</context>

<tasks>

<task type="auto">
  <name>Task 1: Sweep the trivial instruction sites (fsqrt, int 3, rdtsc)</name>
  <read_first>
    - src/engine/shared/library/sharedCollision/src/shared/core/CollisionUtils.cpp (around :427 — the fld/fsqrt/fstp block inside faster_normalize)
    - src/engine/shared/library/sharedFoundation/src/shared/Fatal.cpp (around :174 — __asm int 3 under its _WIN32 guard)
    - src/engine/shared/library/sharedFoundation/src/shared/Clock.cpp (around :270 — __asm int 3 on the frame-time anomaly path)
    - src/engine/shared/library/sharedDebug/src/win32/ProfilerTimer.cpp (:25-40 — the naked readTimeStampCounter and its install/callers)
    - .planning/phases/31-64-bit-correctness-foundation/31-RESEARCH.md §"Pattern 3: scalar/instruction intrinsics" and §__asm Survivor Inventory rows 17-22
  </read_first>
  <action>
    CollisionUtils.cpp:427-429 — replace `__asm fld t; fsqrt; fstp t;` with `t = sqrtf(t);` (`#include
    <cmath>` if not already). Fatal.cpp:174 and Clock.cpp:270 — replace each `__asm int 3` with
    `__debugbreak();` (`#include <intrin.h>`; keep the existing `_WIN32` guard). ProfilerTimer.cpp:29-36 —
    replace the `__declspec(naked) __stdcall readTimeStampCounter()` whose body is `__asm{ rdtsc; ret; }`
    with a normal function returning `__rdtsc()` (drop `naked`; keep the `__int64` return type and adjust
    the `__stdcall` wrapper only as needed so callers still link). No bitness fork on any of these
    (intrinsics compile both bitnesses).
  </action>
  <verify>
    <automated>for tu in CollisionUtils.cpp Fatal.cpp Clock.cpp ProfilerTimer.cpp; do powershell -File src/build/win32/x64-scratch/compile-all.ps1 -SingleTu $tu 2>&1 | grep -cE "error C4235"; done</automated>
  </verify>
  <acceptance_criteria>
    - scratch x64 `cl /c` of CollisionUtils.cpp, Fatal.cpp, Clock.cpp, ProfilerTimer.cpp each emits 0 C4235 (every grep count above is 0)
    - `grep -c '__asm' CollisionUtils.cpp Fatal.cpp Clock.cpp ProfilerTimer.cpp` totals 0
    - `grep -c sqrtf CollisionUtils.cpp` >= 1; `grep -c __debugbreak Fatal.cpp` >= 1 and in Clock.cpp >= 1; `grep -c __rdtsc ProfilerTimer.cpp` >= 1
    - `grep -c '__declspec(naked)' ProfilerTimer.cpp` == 0
  </acceptance_criteria>
  <done>The four trivial instruction sites use sqrtf/__debugbreak/__rdtsc (no naked, no __asm) and compile x64-clean (0 C4235).</done>
</task>

<task type="auto">
  <name>Task 2: Port the VeCritsec spinlock to _interlockedbittestandset (MSVC branch only)</name>
  <read_first>
    - src/engine/client/library/clientGame/src/shared/HTTPpost/VeCritsec.hpp (full file — the `#ifdef _WIN32` MSVC `__asm{ lock bts }` branch at :43-48 AND the `#else` GCC `__asm__` branch; the m_iLock/m_uLockCount/m_uThreadID members and the Locked: label flow)
    - .planning/phases/31-64-bit-correctness-foundation/31-RESEARCH.md §"Pattern 3" (VeCritsec row) and §__asm Survivor Inventory row 23 (note: port ONLY the _WIN32 branch; the #else GCC branch is untouched)
    - .planning/phases/31-64-bit-correctness-foundation/31-PATTERNS.md §"BITS-01 intrinsic ports" (VeCritsec row)
  </read_first>
  <action>
    In the `#ifdef _WIN32` MSVC branch only, replace the `__asm{ mov esi,[p_i_lock]; lock bts dword ptr
    [esi],0; jnc Locked }` test-and-set with `_interlockedbittestandset((long*)&m_iLock, 0)` (returns the
    previous bit value; acquire when it was 0). Restructure the `jnc Locked` control flow into the
    equivalent C++ branch: if the bit was already set, `return false`; else fall through to the existing
    `Locked:` body (set `m_uThreadID`, `++m_uLockCount`, `return true`). Preserve the `volatile unsigned
    int* p_i_lock` semantics via the intrinsic's pointer argument. Leave the `#else` (GCC `__asm__`)
    branch entirely UNTOUCHED. `#include <intrin.h>`.
  </action>
  <verify>
    <automated>powershell -File src/build/win32/x64-scratch/compile-all.ps1 -SingleTu VeCritsec 2>&1 | grep -cE "error C4235|error C4311|error C4312|error C4244"</automated>
  </verify>
  <acceptance_criteria>
    - scratch x64 `cl /c` of the TU including VeCritsec.hpp emits 0 C4235 — the grep count above is 0
    - `grep -c _interlockedbittestandset VeCritsec.hpp` >= 1
    - the `#ifdef _WIN32` MSVC branch contains no `__asm`; `grep -c 'lock bts' VeCritsec.hpp` == 0
    - the `#else` GCC `__asm__` branch is byte-unchanged (git diff shows no lines removed/added inside it)
  </acceptance_criteria>
  <done>The VeCritsec MSVC spinlock uses _interlockedbittestandset with equivalent acquire/fail control flow, the GCC branch is untouched, and the TU compiles x64-clean (0 C4235).</done>
</task>

<task type="auto">
  <name>Task 3: x64-correct DebugHelp stack-walk via a #if defined(_M_X64) RtlCaptureContext branch</name>
  <read_first>
    - src/engine/shared/library/sharedDebug/src/win32/DebugHelp.cpp (around :494-525 — the register-grab __asm block, the CONTEXT/STACKFRAME64 setup, and the stackWalk64(IMAGE_FILE_MACHINE_I386, ...) call with AddrPC/AddrStack/AddrFrame = Eip/Esp/Ebp)
    - .planning/phases/31-64-bit-correctness-foundation/31-RESEARCH.md §"Pattern 4: x64-correct stack-walk context capture" AND §Open Questions #3 (minimum viable Phase-31 scope = make it COMPILE x64-clean; full x64 backtrace runtime validation rides into Phase 33/36)
    - .planning/phases/31-64-bit-correctness-foundation/31-PATTERNS.md §"No Analog Found" (DebugHelp row — OS-API, the only bitness fork in the phase)
  </read_first>
  <action>
    Add a `#if defined(_M_X64)` branch around the register-capture + stack-walk setup. In the x64 branch:
    `RtlCaptureContext(&context);` then set `STACKFRAME64.AddrPC.Offset = context.Rip`,
    `AddrStack.Offset = context.Rsp`, `AddrFrame.Offset = context.Rbp`, and pass
    `IMAGE_FILE_MACHINE_AMD64` to `stackWalk64`. In the `#else` (32-bit) branch keep the EXISTING
    `__asm{ call GetEIP; ... mov context.Eip,eax; mov context.Esp,esp; mov context.Ebp,ebp }` register
    grab + `IMAGE_FILE_MACHINE_I386` path entirely UNCHANGED. This is the one justified bitness fork in
    the phase (the asm is inherently bitness-specific — Eip/Esp/Ebp vs Rip/Rsp/Rbp). Phase-31 bar is that
    the file COMPILES x64-clean (Open-Q3); runtime x64 backtrace validation is deferred to Phase 33/36.
    `#include <windows.h>` (winnt) is already present for CONTEXT/STACKFRAME64.
  </action>
  <verify>
    <automated>powershell -File src/build/win32/x64-scratch/compile-all.ps1 -SingleTu DebugHelp.cpp 2>&1 | grep -cE "error C4235|error C4311|error C4312|error C4244"</automated>
  </verify>
  <acceptance_criteria>
    - scratch x64 `cl /c` of DebugHelp.cpp emits 0 C4235/C4311/C4312/C4244 — the grep count above is 0
    - `grep -c RtlCaptureContext DebugHelp.cpp` >= 1; `grep -c IMAGE_FILE_MACHINE_AMD64 DebugHelp.cpp` >= 1
    - the 32-bit `#else` branch still contains the original `__asm` register grab + `IMAGE_FILE_MACHINE_I386` (unchanged); `grep -c IMAGE_FILE_MACHINE_I386 DebugHelp.cpp` >= 1
    - exactly one bitness fork added: `grep -c '_M_X64' DebugHelp.cpp` >= 1
  </acceptance_criteria>
  <done>DebugHelp captures the stack-walk context via RtlCaptureContext + AMD64 fields under #if defined(_M_X64), keeps the 32-bit asm path unchanged in the #else, and compiles x64-clean.</done>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| spinlock atomicity → HTTP worker correctness | A non-equivalent _interlockedbittestandset control-flow translation could break mutual exclusion in the HTTP-post worker (data race) |
| stack-walk register capture → crash-diagnostic integrity | Wrong register fields on x64 produce garbage backtraces, but this is diagnostic-only (no memory-safety or wire/disk impact); the 32-bit path is unchanged |

## STRIDE Threat Register

| Threat ID | Category | Component | Disposition | Mitigation Plan |
|-----------|----------|-----------|-------------|-----------------|
| T-31-07 | Tampering | VeCritsec lock acquire/fail control flow | mitigate | Faithful intrinsic translation preserving the jnc-equivalent branch; the #else GCC branch untouched (acceptance asserts no-change); spinlock is single-bit, well-defined intrinsic semantics |
| T-31-08 | Repudiation | DebugHelp x64 backtrace correctness | accept | Phase-31 bar is compile-clean only (Open-Q3); runtime x64 backtrace validation deferred to Phase 33/36; 32-bit diagnostic path unchanged, no shipped behavior change now |
| T-31-09 | Tampering | __debugbreak/__rdtsc/sqrtf equivalence | mitigate | Direct 1:1 intrinsic replacements for identical traps/instructions; no semantic change; per-TU compile-clean assert |
</threat_model>

<verification>
- Per-TU scratch x64 `cl /c`: 0 C4235/C4311/C4312/C4244 across all six files.
- No `__asm`/`naked` survivors in the MSVC paths; the GCC VeCritsec branch + the 32-bit DebugHelp branch unchanged.
- Exactly one bitness fork (DebugHelp `_M_X64`); all other replacements are no-fork.
- (32-bit boot regression is covered by plan 06's phase-gate dual-renderer smoke after the wave-2 fixes merge.)
</verification>

<success_criteria>
All in-scope misc `__asm` survivors are replaced with intrinsics: `sqrtf` (CollisionUtils),
`__debugbreak` (Fatal, Clock), `__rdtsc` (ProfilerTimer, naked dropped),
`_interlockedbittestandset` (VeCritsec MSVC branch), and a `#if defined(_M_X64)` `RtlCaptureContext`
branch (DebugHelp, 32-bit path unchanged). Every touched TU compiles x64-clean (0 C4235); the GCC
VeCritsec branch and the 32-bit DebugHelp path are byte-unchanged.
</success_criteria>

<output>
After completion, create `.planning/phases/31-64-bit-correctness-foundation/31-03-SUMMARY.md`
</output>


### 31-04-PLAN.md

---
phase: 31-64-bit-correctness-foundation
plan: 04
type: execute
wave: 2
depends_on: [01]
files_modified:
  - src/engine/client/library/clientGraphics/src/shared/StaticShader.cpp
  - src/engine/shared/library/sharedMemoryManager/src/shared/MemoryManager.cpp
  - src/engine/shared/library/sharedFoundation/src/win32/Os.cpp
  - src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticVertexBufferData.cpp
  - src/engine/client/application/Direct3d11/src/win32/Direct3d11_DynamicVertexBufferData.cpp
autonomous: true
requirements: [BITS-02]
must_haves:
  truths:
    - "Pointer-as-integer sites use width-correct types (uintptr_t/intptr_t/ptrdiff_t/INT_PTR) — never (int)ptr and never cast-to-silence"
    - "The Direct3d11 *VertexBufferData re-truncation defects (static_cast<int>(reinterpret_cast<uintptr_t>(...))) no longer drop high pointer bits"
    - "The touched TUs compile x64-clean with /we4311 /we4312 /we4244 (0 truncation errors)"
    - "The 32-bit build still links clean and renders under both renderers (sort keys remain functional)"
  artifacts:
    - path: "src/engine/client/library/clientGraphics/src/shared/StaticShader.cpp"
      provides: "Width-correct shader sort-key derivation"
    - path: "src/engine/shared/library/sharedMemoryManager/src/shared/MemoryManager.cpp"
      provides: "ptrdiff_t/uintptr_t pointer math + %p/%zx logging"
    - path: "src/engine/shared/library/sharedFoundation/src/win32/Os.cpp"
      provides: "INT_PTR/DWORD_PTR ShellExecute return handling"
  key_links:
    - from: "BITS-02 sites"
      to: "uintptr_t/intptr_t/ptrdiff_t"
      via: "width-correct pointer-as-integer (LeakFinder.h:81 template)"
      pattern: "uintptr_t|intptr_t|ptrdiff_t"
    - from: "scratch x64 harness"
      to: "this plan's truncation worklist"
      via: "/we4311 /we4312 /we4244 enumeration"
      pattern: "we4311"
---

<objective>
BITS-02: resolve the pointer/integer truncation defects so the touched code compiles x64-clean with
C4311/C4312/C4244 treated as errors — no `(int)pointer` / `DWORD`-holds-pointer survivors. The
canonical analog is `LeakFinder.h:81` (`reinterpret_cast<uintptr_t>(p)` — already correct). Fixes use
width-correct types per the D-06 toolbox; NEVER cast-to-silence (`#pragma warning(disable)`) and NEVER
re-truncate (`(int)(intptr_t)ptr`).

Purpose: x64 pointers are 64-bit; a pointer stored/hashed/diffed through `int` drops the high 32 bits
(a latent memory-safety class). The scratch harness (plan 01) is the AUTHORITATIVE enumerator — the
sites named in RESEARCH are representative, not exhaustive; the executor compiles the in-scope graphics
+ allocator + foundation TUs through the harness and fixes EVERY C4311/C4312/C4244 it surfaces.

Output: Truncation fixes in shared source (the 32-bit build inherits them) that compile x64-clean and
keep sort keys / pointer math / Win32-boundary returns functional in the 32-bit dual-renderer build.
</objective>

<execution_context>
@D:/Code/swg-client/.claude/get-shit-done/workflows/execute-plan.md
@D:/Code/swg-client/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@.planning/STATE.md
@.planning/ROADMAP.md
@.planning/phases/31-64-bit-correctness-foundation/31-CONTEXT.md
@.planning/phases/31-64-bit-correctness-foundation/31-RESEARCH.md
@.planning/phases/31-64-bit-correctness-foundation/31-PATTERNS.md
@AGENTS.md

<interfaces>
<!-- THE canonical analog — copy this width-correct shape (PATTERNS BITS-02 truncation). -->
From sharedDebug/src/shared/LeakFinder.h:79-83:
  struct ptr_hash { size_t operator()(void* p) const noexcept { return std::hash<uintptr_t>()(reinterpret_cast<uintptr_t>(p)); } };

<!-- Verified defect sites (RESEARCH BITS-02 Truncation-Defect Sample + PATTERNS). Compiler is authoritative for the rest. -->
StaticShader.cpp:289   int getShaderTemplateSortKey() const { return reinterpret_cast<int>(&getStaticShaderTemplate()); }   // C4311
MemoryManager.cpp:405  reinterpret_cast<int>(next) - reinterpret_cast<int>(this)   // ptr diff via int -> ptrdiff_t
MemoryManager.cpp:1329,1426,1753  reinterpret_cast<int>(memory) for %08x logging / block math   // -> uintptr_t + %p/%zx
Os.cpp:1579            reinterpret_cast<int>(ShellExecute(...))   // HINSTANCE->int -> INT_PTR/DWORD_PTR

<!-- ANTI-PATTERN already in the tree — FIX, do not copy (PATTERNS ANTI-PATTERN already in the tree). -->
Direct3d11_StaticVertexBufferData.cpp:167   return static_cast<int>(reinterpret_cast<uintptr_t>(m_d3dBuffer.Get()));   // re-truncation
Direct3d11_DynamicVertexBufferData.cpp:372  return static_cast<int>(reinterpret_cast<uintptr_t>(ms_d3dRingBuffer.Get()));  // re-truncation

<!-- D-06 toolbox: ptr stored/hashed -> uintptr_t ; signed ptr diff -> intptr_t/ptrdiff_t ; Win32 boundary -> DWORD_PTR/INT_PTR/LONG_PTR ; buffer size -> size_t -->
</interfaces>
</context>

<tasks>

<task type="auto">
  <name>Task 1: Fix the sort-key + re-truncation pointer-to-int defects (StaticShader + both Direct3d11 buffer data)</name>
  <read_first>
    - src/engine/client/library/clientGraphics/src/shared/StaticShader.cpp (around :287-291 — getShaderTemplateSortKey returns reinterpret_cast<int> of a pointer; AND every caller/consumer of getShaderTemplateSortKey so a return-type change doesn't break the sort comparator)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticVertexBufferData.cpp (around :165-170 — getSortKey re-truncation) and its base/interface declaring getSortKey's return type
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_DynamicVertexBufferData.cpp (around :370-374 — same shape)
    - .planning/phases/31-64-bit-correctness-foundation/31-PATTERNS.md section "BITS-02 truncation — THE canonical analog: LeakFinder.h:81" (incl. the StaticShader return-type caveat and the re-truncation anti-pattern)
  </read_first>
  <action>
    StaticShader.cpp:289 — the defect is `return reinterpret_cast<int>(&getStaticShaderTemplate());` in
    `int getShaderTemplateSortKey() const`. The pointer must not be truncated; per the D-06 toolbox a
    pointer-as-key uses `uintptr_t`. Resolve the int-return-type tension by EITHER widening the return type
    to a pointer-width integer (preferred if the comparator + all callers can take it) OR hashing the
    pointer to a stable key (mirroring LeakFinder's `std::hash<uintptr_t>`); the key must stay UNIQUE/stable
    on x64. Pick based on what the callers (read first) accept; document the choice in the SUMMARY. For
    Direct3d11_StaticVertexBufferData.cpp:167 and Direct3d11_DynamicVertexBufferData.cpp:372 — the
    `static_cast<int>(reinterpret_cast<uintptr_t>(...))` is the re-truncation anti-pattern (the uintptr_t is
    correct, the outer `static_cast<int>` defeats it). Fix the same way as StaticShader (widen getSortKey's
    return type to pointer width, or hash) and keep the sort-key contract consistent between StaticShader
    and the plugin buffer-data getSortKey. NEVER `(int)(intptr_t)ptr`, NEVER `#pragma warning(disable)`.
  </action>
  <verify>
    <automated>for tu in StaticShader.cpp Direct3d11_StaticVertexBufferData.cpp Direct3d11_DynamicVertexBufferData.cpp; do powershell -File src/build/win32/x64-scratch/compile-all.ps1 -SingleTu $tu 2>&1 | grep -cE "error C4311|error C4312|error C4244"; done</automated>
  </verify>
  <acceptance_criteria>
    - scratch x64 `cl /c /we4311 /we4312 /we4244` of StaticShader.cpp, Direct3d11_StaticVertexBufferData.cpp, Direct3d11_DynamicVertexBufferData.cpp each emits 0 C4311/C4312/C4244 (every grep count above is 0)
    - `grep -c 'reinterpret_cast<int>' StaticShader.cpp` == 0
    - `grep -c 'static_cast<int>(reinterpret_cast<uintptr_t>' Direct3d11_StaticVertexBufferData.cpp Direct3d11_DynamicVertexBufferData.cpp` totals 0 (re-truncation gone)
    - no `#pragma warning(disable:4311)`/`4312` added to any of the three files
    - the getSortKey return-type / hashing choice is documented in the SUMMARY
  </acceptance_criteria>
  <done>The shader + buffer-data sort keys derive from the full pointer width (widened return type or stable hash), the re-truncation anti-pattern is gone, and the three TUs compile x64-clean (0 C4311/C4312/C4244) with no warning suppression.</done>
</task>

<task type="auto">
  <name>Task 2: Fix MemoryManager pointer math + logging and the Os ShellExecute Win32 boundary</name>
  <read_first>
    - src/engine/shared/library/sharedMemoryManager/src/shared/MemoryManager.cpp (around :405 ptr-diff via int; :1329, :1426, :1753 reinterpret_cast<int>(memory) for %08x logging / block math — read each in context to pick diff vs hash vs log)
    - src/engine/shared/library/sharedFoundation/src/win32/Os.cpp (around :1579 — reinterpret_cast<int>(ShellExecute(...)) HINSTANCE return)
    - .planning/phases/31-64-bit-correctness-foundation/31-RESEARCH.md "BITS-02 Truncation-Defect Sample" + "The toolbox (D-06 discretion)"
    - .planning/phases/31-64-bit-correctness-foundation/31-PATTERNS.md "Pointer-as-integer (BITS-02 ...)" shared pattern
  </read_first>
  <action>
    MemoryManager.cpp:405 — the `reinterpret_cast<int>(next) - reinterpret_cast<int>(this)` pointer
    difference becomes a real pointer subtraction yielding `ptrdiff_t` (subtract the pointers directly, or
    cast each via `intptr_t` then subtract). MemoryManager.cpp:1329/1426/1753 — the
    `reinterpret_cast<int>(memory)` used for `%08x` logging / block math becomes `uintptr_t` with the
    format string updated to `%p` (or `%zx`) so the full address prints on x64; if the value feeds block
    arithmetic, keep it `uintptr_t`/`ptrdiff_t` per the operation. Os.cpp:1579 — the
    `reinterpret_cast<int>(ShellExecute(...))` HINSTANCE return uses `INT_PTR` (or `DWORD_PTR`) for the
    Win32 API boundary, comparing against the documented `> 32` success threshold with the widened type.
    Apply the D-06 toolbox: never `(int)(intptr_t)ptr`, never `#pragma warning(disable)`. The scratch
    harness is authoritative — fix every additional C4311/C4312/C4244 it surfaces in these two TUs, not
    only the named lines.
  </action>
  <verify>
    <automated>for tu in MemoryManager.cpp Os.cpp; do powershell -File src/build/win32/x64-scratch/compile-all.ps1 -SingleTu $tu 2>&1 | grep -cE "error C4311|error C4312|error C4244"; done</automated>
  </verify>
  <acceptance_criteria>
    - scratch x64 `cl /c /we4311 /we4312 /we4244` of MemoryManager.cpp and Os.cpp each emits 0 C4311/C4312/C4244 (every grep count above is 0)
    - `grep -c 'reinterpret_cast<int>' MemoryManager.cpp Os.cpp` totals 0
    - MemoryManager logging uses `%p`/`%zx` (not `%08x` of a pointer): `grep -c '%08x' MemoryManager.cpp` does not increase for pointer-bearing format strings
    - Os.cpp ShellExecute return uses `INT_PTR`/`DWORD_PTR`: `grep -cE 'INT_PTR|DWORD_PTR' Os.cpp` >= 1
    - no `#pragma warning(disable:4311)`/`4312`/`4244` added to either file
  </acceptance_criteria>
  <done>MemoryManager's pointer diffs use ptrdiff_t/intptr_t and its logging prints full addresses via uintptr_t+%p; Os.cpp's ShellExecute return uses a pointer-width Win32 type; both compile x64-clean with no suppression.</done>
</task>

<task type="auto">
  <name>Task 3: Sweep the in-scope graphics/allocator/foundation TUs through the harness for residual truncation</name>
  <read_first>
    - src/build/win32/x64-scratch/in-scope-tus.txt (the manifest from plan 01 — the TU list to sweep)
    - .planning/phases/31-64-bit-correctness-foundation/31-RESEARCH.md "Open Questions" #2 (the compile is the exhaustive backstop; the named sites are representative, NOT exhaustive — Assumption A2) and "Architectural Responsibility Map" (clientGraphics/renderer plugins/sharedMemoryManager hold the truncation sites)
    - .planning/phases/31-64-bit-correctness-foundation/31-PATTERNS.md "Pointer-as-integer" toolbox + the Direct3d9.cpp:137-203 `(DWORD)(uintptr_t)` diagnostic-log note
  </read_first>
  <action>
    Run the scratch harness over the in-scope clientGraphics / sharedMemoryManager / sharedFoundation /
    renderer-plugin TUs (per the plan-01 manifest) with `/we4311 /we4312 /we4244` to enumerate ANY residual
    truncation the named-site fixes (Tasks 1-2) did not cover — the compiler, not RESEARCH, is the
    authoritative worklist (A2 / Open-Q2). For each surfaced C4311/C4312/C4244 in a file THIS plan owns
    (the 5 files in files_modified), apply the D-06 toolbox width-correct fix. For surfaced defects in TUs
    owned by other plans or not yet in any plan, record them in the residual list to fold into plan 06's
    phase-gate sweep (do not edit files outside this plan's files_modified). Note the Direct3d9.cpp:137-203
    `(DWORD)(uintptr_t)` casts are diagnostic-log-only (`%08X`) — same truncation class; flag them for the
    gate sweep if the harness errors on them (Direct3d9.cpp is not in this plan's files_modified).
  </action>
  <verify>
    <automated>powershell -File src/build/win32/x64-scratch/compile-all.ps1 -Scope bits02 2>&1 | grep -E "error C4311|error C4312|error C4244" | grep -E "StaticShader|MemoryManager|Os\.cpp|Direct3d11_StaticVertexBufferData|Direct3d11_DynamicVertexBufferData" | wc -l</automated>
  </verify>
  <acceptance_criteria>
    - the harness sweep reports 0 C4311/C4312/C4244 in any of THIS plan's 5 files_modified (the grep+wc count above is 0)
    - any truncation defect surfaced in a TU NOT owned by this plan is written to a residual list for plan 06 (not silently fixed cross-plan, not ignored)
    - no `#pragma warning(disable)` introduced anywhere in this plan's files
  </acceptance_criteria>
  <done>The in-scope BITS-02 TUs owned by this plan compile x64-clean with truncation-as-errors; residual truncation in other plans' files is captured for the phase gate rather than silently edited.</done>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| pointer-as-integer → memory safety | A truncated pointer (high 32 bits dropped) is a latent memory-safety class on x64 — dereferencing a half-pointer reads/writes the wrong address |
| sort-key uniqueness → render correctness | Re-truncating a pointer sort key can collide distinct shaders/buffers on x64, mis-ordering or merging draws (data-integrity, render-correctness) |
| Win32 API return (ShellExecute HINSTANCE) → boundary correctness | Truncating the HINSTANCE return mis-evaluates the success threshold |

## STRIDE Threat Register

| Threat ID | Category | Component | Disposition | Mitigation Plan |
|-----------|----------|-----------|-------------|-----------------|
| T-31-10 | Elevation of Privilege | pointer truncation (latent memory-safety) | mitigate | Width-correct types (uintptr_t/intptr_t/ptrdiff_t) per D-06; scratch harness /we4311 /we4312 enforces 0 survivors; never cast-to-silence |
| T-31-11 | Tampering | shader/buffer sort-key uniqueness | mitigate | Widen return type or stable hash keeping keys unique on x64; consistent contract across StaticShader + plugin getSortKey; 32-bit render smoke (plan 06 gate) confirms draw ordering |
| T-31-12 | Spoofing | ShellExecute success-threshold check | mitigate | INT_PTR/DWORD_PTR return + widened threshold comparison; Win32-boundary toolbox type |
</threat_model>

<verification>
- Per-TU scratch x64 `cl /c /we4311 /we4312 /we4244`: 0 C4311/C4312/C4244 across the 5 owned files.
- No `reinterpret_cast<int>` of a pointer and no re-truncation `static_cast<int>(reinterpret_cast<uintptr_t>(...))` survivors.
- No `#pragma warning(disable)` introduced; fixes are width-correct types, not suppression.
- Residual truncation in non-owned TUs captured for the plan-06 gate sweep.
- (32-bit link + dual-renderer render smoke is covered by plan 06's phase gate after wave-2 merge.)
</verification>

<success_criteria>
The in-scope BITS-02 truncation defects this plan owns (StaticShader sort key, MemoryManager pointer
math + logging, Os ShellExecute return, both Direct3d11 buffer-data re-truncations) are fixed with
width-correct types — never cast-to-silence, never re-truncated — and compile x64-clean under
`/we4311 /we4312 /we4244` (0 errors). Sort-key contracts stay unique on x64; residual truncation in
other plans' files is handed to the phase gate.
</success_criteria>

<output>
After completion, create `.planning/phases/31-64-bit-correctness-foundation/31-04-SUMMARY.md`
</output>


### 31-05-PLAN.md

---
phase: 31-64-bit-correctness-foundation
plan: 05
type: execute
wave: 2
depends_on: [01]
files_modified:
  - src/external/ours/library/archive/src/shared/Archive.h
  - src/engine/shared/library/sharedImage/src/shared/TargaFormat.cpp
  - src/engine/shared/library/sharedGame/src/shared/core/AssetCustomizationManager.cpp
autonomous: true
requirements: [BITS-03]
must_haves:
  truths:
    - "The Archive std::map get/put count is pinned to uint32_t so the wire format is byte-identical to the shipped 32-bit server on both bitnesses (D-07 — targeted LLP64 width audit)"
    - "No 8-byte Archive overload is added (the serializer width is unchanged — only the map count type is pinned)"
    - "TargaHeader/TargaFooter and the AssetCustomizationManager packed structs carry static_assert(sizeof==N) guards baselined to the actual 32-bit sizeof (D-08 — compile-time layout guards + dual-renderer boot smoke regression proof)"
    - "The 32-bit build compiles green with the new static_asserts and still loads TGA assets + customization data"
  artifacts:
    - path: "src/external/ours/library/archive/src/shared/Archive.h"
      provides: "uint32_t-pinned std::map get/put count (wire-format-stable)"
      contains: "uint32_t"
    - path: "src/engine/shared/library/sharedImage/src/shared/TargaFormat.cpp"
      provides: "static_assert size guards on TargaHeader/TargaFooter"
      contains: "static_assert"
    - path: "src/engine/shared/library/sharedGame/src/shared/core/AssetCustomizationManager.cpp"
      provides: "static_assert size guards on the packed customization structs"
      contains: "static_assert"
  key_links:
    - from: "Archive::get/put(std::map)"
      to: "the existing 4-byte unsigned int overload"
      via: "uint32_t numKeys binding (Archive.h:47-50 overload)"
      pattern: "uint32_t numKeys"
    - from: "packed structs"
      to: "compile-time layout guards"
      via: "static_assert(sizeof(X)==N) baselined to live 32-bit sizeof (Direct3d11_ConstantBuffer.h convention)"
      pattern: "static_assert\\(sizeof"
---

<objective>
BITS-03: audit + correct the IFF/TRE and network-message data-layout / serialization-width assumptions
that move on Windows LLP64 x64 — and lock them with compile-time guards. Per the verified audit (D-07),
the IFF reader is fixed-width by design (SAFE), `int32`/`uint32` typedef to `long` = 32-bit on LLP64
(SAFE), and the ONE real hazard is the `Archive` `std::map` get/put using raw `size_t numKeys` (no
64-bit overload exists). Fix = pin to `uint32_t` (Pattern 5) so the wire format stays byte-identical to
the shipped 32-bit server. The two `#pragma pack` structs (TargaFormat, AssetCustomizationManager) are
fixed-width/layout-safe — guard them with `static_assert(sizeof==N)` (D-08; adding asserts does NOT
change layout, so no shared-header ABI cascade).

Purpose: A width-fix that silently changes the wire/disk format would corrupt parsed network messages
or saved/asset data. The fix pins the count width (keeps bytes identical) and the static_asserts catch
accidental layout drift forever. The dual-renderer boot smoke (plan 06 gate) exercises live TRE/IFF
load + the login handshake as the functional proof.

Output: A wire-stable Archive map serializer + permanent layout guards, compiling green in the 32-bit
build and x64-clean in the scratch harness.
</objective>

<execution_context>
@D:/Code/swg-client/.claude/get-shit-done/workflows/execute-plan.md
@D:/Code/swg-client/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@.planning/STATE.md
@.planning/ROADMAP.md
@.planning/phases/31-64-bit-correctness-foundation/31-CONTEXT.md
@.planning/phases/31-64-bit-correctness-foundation/31-RESEARCH.md
@.planning/phases/31-64-bit-correctness-foundation/31-PATTERNS.md
@AGENTS.md

<interfaces>
<!-- HAZARD sites (verified) + the in-file correct-house-style analog (PATTERNS BITS-03 serialization). Do NOT add an 8-byte overload. -->
Archive.h:191-204  get(std::map)  { size_t numKeys; get(source, numKeys); ... }   // HAZARD: raw size_t (8 bytes on x64)
Archive.h:380-389  put(std::map)  { size_t numKeys = source.size(); put(target, numKeys); ... }   // HAZARD twin
Archive.h:130-141  get(std::vector) { signed int length = 0; source.get(&length, 4); ... }   // correct fixed-4-byte idiom to match
Archive.h:368-371  put(std::vector) { signed int length = source.size(); target.put(&length, 4); }
Archive.h:47-50    inline void get(ReadIterator &, unsigned int & target) { source.get(&target, 4); }   // the 4-byte overload uint32_t binds to
Note: include/Archive/Archive.h is a 2-line forwarder to src/shared/Archive.h — edit the src/shared/ copy.

<!-- static_assert convention (PATTERNS BITS-03 layout guards) — the house style to copy. -->
From Direct3d11/src/win32/Direct3d11_ConstantBuffer.h:47-66, 240-249:
  static_assert(sizeof(Direct3d11_PerFrameCB) == 96, "Direct3d11_PerFrameCB size mismatch");
  static_assert(offsetof(Direct3d11_VertexSlot0CB, objectWorldMatrix) == 64, "c4 boundary");

<!-- packed structs (verified layout-safe — fixed-width fields, no pointer/size_t). Baseline N from the LIVE 32-bit sizeof, not from RESEARCH. -->
TargaFormat.cpp:61-88   #pragma pack(push,1) TargaHeader (uint8/uint16, ~18B) + TargaFooter (uint32x2 + char[N], ~26B)
AssetCustomizationManager.cpp:47-98   IntRange/etc. (uint16/int)
</interfaces>
</context>

<tasks>

<task type="auto">
  <name>Task 1: Pin the Archive std::map get/put count to uint32_t (wire-format-stable)</name>
  <read_first>
    - src/external/ours/library/archive/src/shared/Archive.h (the map get :191-204 and put :380-389 hazard sites; the vector get/put :130-141/:368-371 correct idiom; the unsigned int 4-byte overload :47-50)
    - .planning/phases/31-64-bit-correctness-foundation/31-RESEARCH.md "Pattern 5: LLP64 serialization width guard" AND "BITS-03 Serialization/Layout Audit" (the SAFE-by-design list — do NOT touch IFF/vector/int32 paths) AND the anti-pattern "Adding a 64-bit Archive overload that reads 8 bytes"
    - .planning/phases/31-64-bit-correctness-foundation/31-PATTERNS.md "BITS-03 serialization — Archive std::map size_t pin"
  </read_first>
  <action>
    In `src/shared/Archive.h` (NOT the include/ forwarder): in `get(std::map)` (:193) replace
    `size_t numKeys;` with `uint32_t numKeys;` (then `get(source, numKeys);` binds the existing 4-byte
    `unsigned int` overload on both bitnesses); adjust the loop index type so the `i < numKeys` comparison
    is clean. In `put(std::map)` (:382) replace `size_t numKeys = source.size();` with `const uint32_t
    numKeys = static_cast<uint32_t>(source.size());`. This keeps the wire format byte-identical to the
    shipped 32-bit server. Do NOT add an 8-byte Archive overload (would change the wire format and break
    32-bit-server interop — RESEARCH anti-pattern). Do NOT touch the SAFE-by-design paths (IFF fixed-width,
    vector/set/deque counts, `int32`/`uint32`=`long`=32-bit, the `long`/`unsigned long` 4-byte get/put).
    `#include <cstdint>` if `uint32_t` is not already visible. NOTE: Archive.h is a shared header consumed
    by gl05/06/07/11 — but this edit does NOT change any struct layout (only a local variable type), so no
    ABI cascade; the plan-06 gate still rebuilds the canonical 5 targets.
  </action>
  <verify>
    <automated>powershell -File src/build/win32/x64-scratch/compile-all.ps1 -SingleTu ArchiveMapTest 2>&1 | grep -cE "error C4311|error C4312|error C4244|error C2664"</automated>
  </verify>
  <acceptance_criteria>
    - the Archive map get/put no longer uses raw `size_t` for the count: `grep -c 'size_t numKeys' src/external/ours/library/archive/src/shared/Archive.h` == 0
    - `grep -c 'uint32_t numKeys' Archive.h` >= 2 (read + write)
    - NO 8-byte overload added: `grep -cE 'get\(.*size_t|put\(.*size_t' Archive.h` does not increase; no `, 8)` get/put count read introduced
    - a TU instantiating `Archive::get/put(std::map<...>)` compiles x64-clean in the scratch harness (0 overload-resolution / truncation errors — the grep count above is 0)
    - the SAFE-by-design vector/IFF/int32 paths are byte-unchanged (git diff touches only the two map functions)
  </acceptance_criteria>
  <done>The Archive std::map count is pinned to uint32_t binding the existing 4-byte overload, the wire format is unchanged (no 8-byte overload), the SAFE paths are untouched, and a map (de)serialization TU compiles x64-clean.</done>
</task>

<task type="auto">
  <name>Task 2: Add static_assert layout guards to the packed structs, baselined to the live 32-bit sizeof</name>
  <read_first>
    - src/engine/shared/library/sharedImage/src/shared/TargaFormat.cpp (the #pragma pack(push,1) block :61-88 — TargaHeader fields + TargaFooter fields + PACKING_END_STRUCT macro)
    - src/engine/shared/library/sharedGame/src/shared/core/AssetCustomizationManager.cpp (the packed structs :47-98 — IntRange/etc.)
    - .planning/phases/31-64-bit-correctness-foundation/31-RESEARCH.md "Pattern 6: compile-time layout guard" AND "Assumption A1"/"Open Questions #1" (baseline N from the COMPILER, not from RESEARCH — the 18/26 are TGA-spec values to CONFIRM, not trust)
    - .planning/phases/31-64-bit-correctness-foundation/31-PATTERNS.md "BITS-03 layout guards — static_assert(sizeof==N) + offsetof" (the Direct3d11_ConstantBuffer.h house convention)
  </read_first>
  <action>
    FIRST determine the actual 32-bit `sizeof` of each packed struct — compile a one-off `printf("%zu",
    sizeof(TargaHeader))` (or read the value the compiler reports) under the committed 32-bit build; do NOT
    trust the 18/26 literals from RESEARCH (A1). THEN add `static_assert(sizeof(TargaHeader) == N,
    "TargaFormat: TGA header must stay N bytes on disk");` and the TargaFooter equivalent immediately after
    each struct definition (inside the `#pragma pack` region or right after `#pragma pack(pop)`), matching
    the Direct3d11_ConstantBuffer.h house convention (sizeof guard + descriptive message; add an
    `offsetof` guard on a key field if the struct has a meaningful boundary). Do the same for the
    AssetCustomizationManager packed structs. Adding `static_assert`/`offsetof` does NOT change layout —
    these are shared-tree files but the guard is ABI-safe (no plugin rebuild needed for the assert itself).
    Do NOT change any field width (the structs are fixed-width and layout-safe — changing a field WOULD
    trip the ABI cascade and is not needed).
  </action>
  <verify>
    <automated>$env:MSBUILD src/build/win32/swg.sln /t:SwgClient /p:Configuration=Debug /p:Platform=Win32 /nodeReuse:false 2>&1 | grep -ciE "static_assert failed|error C2338"</automated>
  </verify>
  <acceptance_criteria>
    - the 32-bit Debug build compiles GREEN with the new static_asserts — `grep -ciE 'static_assert failed|error C2338'` of the build log is 0 (the asserts pass at the baselined N)
    - `grep -c 'static_assert(sizeof(TargaHeader)' TargaFormat.cpp` >= 1 and a TargaFooter guard present
    - `grep -c 'static_assert(sizeof' AssetCustomizationManager.cpp` >= 1
    - the baselined N values were read from the live 32-bit sizeof (documented in the SUMMARY), not copied from RESEARCH
    - no packed-struct FIELD width changed: `git diff` shows only added static_assert/offsetof lines, no field-type edits
  </acceptance_criteria>
  <done>Each packed struct carries a static_assert(sizeof==N) guard baselined to its actual 32-bit sizeof (plus offsetof where meaningful), the 32-bit build compiles green, and no field width was altered.</done>
</task>

<task type="auto">
  <name>Task 3: Backstop-sweep the serialization TUs through the harness for residual size_t/width survivors</name>
  <read_first>
    - src/build/win32/x64-scratch/in-scope-tus.txt (the manifest from plan 01)
    - .planning/phases/31-64-bit-correctness-foundation/31-RESEARCH.md "Open Questions" #1 and #2 (the scratch x64 compile is the exhaustive backstop — a raw-size_t Archive::get won't resolve to a 64-bit overload and surfaces as an error) and "Assumption A2"
    - .planning/phases/31-64-bit-correctness-foundation/31-RESEARCH.md "BITS-03 Serialization/Layout Audit" (the SAFE list to NOT touch)
  </read_first>
  <action>
    Run the scratch harness over the in-scope serialization TUs (sharedNetworkMessages, sharedFile,
    sharedGame, and any TU instantiating `Archive::get/put`) to surface ANY residual raw-`size_t` /
    pointer-width serializer beyond the map path (A2 / Open-Q2) — such a call won't bind a 64-bit Archive
    overload (none exists) and shows as an overload-resolution/C2664 or C4244 error. For any survivor in a
    file THIS plan owns, pin it to the correct fixed width per Pattern 5. For survivors in TUs NOT owned by
    this plan, record them in the residual list for the plan-06 gate (do not edit cross-plan). Confirm the
    SAFE-by-design paths (IFF fixed-width, vector/set counts, int32=long) remain unchanged — do NOT "fix"
    them (they are 32-bit on LLP64 by design).
  </action>
  <verify>
    <automated>powershell -File src/build/win32/x64-scratch/compile-all.ps1 -Scope bits03 2>&1 | grep -E "error C2664|error C4244" | grep -iE "archive|serial|size_t" | wc -l</automated>
  </verify>
  <acceptance_criteria>
    - the harness sweep reports 0 serialization-width errors in THIS plan's files_modified (the grep+wc count above is 0)
    - any raw-size_t/width serializer surfaced in a TU NOT owned by this plan is written to a residual list for plan 06 (not silently fixed cross-plan)
    - the SAFE-by-design paths are unchanged (no edits to IFF/vector/int32 serialization)
  </acceptance_criteria>
  <done>The serialization TUs this plan owns compile x64-clean with no residual raw-size_t serializer; any out-of-plan survivor is captured for the phase gate; the SAFE-by-design fixed-width paths are untouched.</done>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| Archive map count width → wire format | A width change to the serialized count alters the bytes on the wire/disk — corrupting parsed network messages and breaking interop with the shipped 32-bit server |
| packed-struct layout → disk/asset format | A field-width change (or accidental drift) changes the on-disk TGA / customization layout — corrupting asset reads |
| shared-header edit (Archive.h) → plugin ABI | Archive.h is consumed by gl05/06/07/11; a LAYOUT change would trip the ABI cascade (this plan changes only a local var type + adds asserts — both ABI-safe) |

## STRIDE Threat Register

| Threat ID | Category | Component | Disposition | Mitigation Plan |
|-----------|----------|-----------|-------------|-----------------|
| T-31-13 | Tampering | Archive std::map wire count width | mitigate | Pin to uint32_t binding the EXISTING 4-byte overload — bytes identical to 32-bit; NO 8-byte overload added (acceptance asserts none); dual-renderer boot smoke exercises the login handshake (plan-06 gate) |
| T-31-14 | Tampering | packed-struct on-disk layout drift | mitigate | static_assert(sizeof==N) baselined to live 32-bit sizeof catches drift at compile time forever; no field width changed (acceptance asserts diff is asserts-only) |
| T-31-15 | Tampering | Archive.h shared-header ABI cascade | mitigate | Edit changes only a local variable type + adds asserts (no struct-layout change → ABI-safe); plan-06 gate rebuilds the canonical 5 targets and link-greps 0 unresolved |
</threat_model>

<verification>
- Archive map count pinned to `uint32_t` (no raw `size_t`, no 8-byte overload); SAFE paths byte-unchanged.
- A map (de)serialization TU compiles x64-clean (no overload-resolution/truncation error).
- packed structs carry `static_assert(sizeof==N)` baselined to the live 32-bit sizeof; 32-bit build compiles green (asserts pass); no field width changed.
- Harness backstop sweep: 0 residual serialization-width survivors in this plan's files; out-of-plan survivors captured for the gate.
</verification>

<success_criteria>
The one real BITS-03 hazard (Archive `std::map` raw `size_t` count) is pinned to `uint32_t` keeping the
wire format byte-identical to the shipped 32-bit server (no 8-byte overload), the two `#pragma pack`
structs carry permanent `static_assert(sizeof==N)` guards baselined to their live 32-bit sizes, and the
SAFE-by-design fixed-width paths are untouched. The 32-bit build compiles green with the asserts and the
touched serialization TUs compile x64-clean.
</success_criteria>

<output>
After completion, create `.planning/phases/31-64-bit-correctness-foundation/31-05-SUMMARY.md`
</output>


### 31-06-PLAN.md

---
phase: 31-64-bit-correctness-foundation
plan: 06
type: execute
wave: 3
depends_on: [02, 03, 04, 05]
files_modified:
  - .planning/phases/31-64-bit-correctness-foundation/31-PHASE33-RESIDUAL-WORKLIST.md
autonomous: false
requirements: [BITS-01, BITS-02, BITS-03]
must_haves:
  truths:
    - "All in-scope build-path engine libs compile x64-clean in the scratch harness (0 C4235/C4311/C4312/C4244), or the residual is documented (D-02)"
    - "The committed 32-bit canonical 5-target build links clean (0 unresolved external symbol under /FORCE)"
    - "The 32-bit client boots to character select under both rasterMajor=5 and =11 with no asset-load / parse / render regression (SC#4)"
    - "A residual worklist artifact hands the third-party-dependent / un-compile-validatable items forward to Phase 33 (D-02)"
  artifacts:
    - path: ".planning/phases/31-64-bit-correctness-foundation/31-PHASE33-RESIDUAL-WORKLIST.md"
      provides: "The explicit D-02 hand-off of items not forced clean in Phase 31 (third-party-lib-dependent, runtime-only x64 behavior)"
      contains: "Phase 33"
  key_links:
    - from: "scratch x64 harness (full sweep)"
      to: "BITS-01/02/03 clean gate"
      via: "compile-all.ps1 over the full in-scope manifest, grep C4235/C4311/C4312/C4244 == 0"
      pattern: "C4235"
    - from: "committed 32-bit build"
      to: "dual-renderer boot smoke"
      via: "rasterMajor=5 then =11 in client_d.cfg, boot to character select"
      pattern: "rasterMajor"
---

<objective>
Phase 31 gate: prove the whole in-scope build path is x64-clean in the scratch harness, prove the
committed 32-bit build still links clean and boots both renderers to character select (the D-08
regression proof / SC#4), and hand the un-compile-validatable residue forward to Phase 33 (D-02).

Purpose: The acceptance bar (D-02) is "all in-scope build-path engine libs COMPILE x64-clean in the
scratch config" — not a green full-solution build. This plan runs the FULL scratch sweep over the
plan-01 manifest (folding in any residual truncation/serialization items the wave-2 plans flagged in
files they didn't own), confirms no regression to the shipped 32-bit build, and writes the explicit
residual worklist artifact. This is the only non-autonomous plan (it ends on a human-verify checkpoint
for the dual-renderer boot — there is no automated game-boot harness).

Output: A green full scratch-x64 sweep (or documented residual), a clean-linking 32-bit build booting
both renderers, and `31-PHASE33-RESIDUAL-WORKLIST.md`.
</objective>

<execution_context>
@D:/Code/swg-client/.claude/get-shit-done/workflows/execute-plan.md
@D:/Code/swg-client/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@.planning/STATE.md
@.planning/ROADMAP.md
@.planning/phases/31-64-bit-correctness-foundation/31-CONTEXT.md
@.planning/phases/31-64-bit-correctness-foundation/31-RESEARCH.md
@.planning/phases/31-64-bit-correctness-foundation/31-VALIDATION.md
@AGENTS.md

<interfaces>
<!-- The acceptance bar + commands (RESEARCH Validation Architecture / VALIDATION Sampling Rate). -->
Full scratch sweep:  compile-all.ps1 over in-scope-tus.txt  -> grep 'error C4235|error C4311|error C4312|error C4244' == 0 (or residual documented)
Full 32-bit build:   $env:MSBUILD src/build/win32/swg.sln /t:Direct3d11;Direct3d9;Direct3d9_ffp;Direct3d9_vsps;SwgClient /p:Configuration=Debug /p:Platform=Win32 /nodeReuse:false
                     (delete src/compile/win32/SwgClient/Debug/SwgClient_d.exe first to force relink)
Link gate:           grep build log 'unresolved external symbol' == 0  (/FORCE masks them; exit 0 != clean link)
Boot smoke (D-08):   set rasterMajor=5 then =11 in stage/client_d.cfg (Edit/Write — never PS Set-Content, BOM crash), launch SwgClient_d.exe, confirm character select
ABI note:            if any wave-2 plan changed a shared-header struct LAYOUT (it should not have — Archive.h was local-var-only + asserts), rebuild ALL plugin .vcxprojs (gl05/06/07/11)
</interfaces>
</context>

<tasks>

<task type="auto">
  <name>Task 1: Full scratch-x64 sweep + write the Phase 33 residual worklist (D-02)</name>
  <read_first>
    - src/build/win32/x64-scratch/in-scope-tus.txt and compile-all.ps1 (from plan 01)
    - the residual lists produced by plans 04 (Task 3) and 05 (Task 3) — out-of-plan truncation/serialization survivors
    - .planning/phases/31-64-bit-correctness-foundation/31-RESEARCH.md "Scratch x64 Compile-Only Harness" (the Residual worklist (D-02) paragraph) and "Build-Graph Scope" (the in-scope vs explicitly-OUT lists)
    - .planning/phases/31-64-bit-correctness-foundation/31-CONTEXT.md D-02 (acceptance bar + residual hand-off)
  </read_first>
  <action>
    Run `compile-all.ps1` over the FULL in-scope manifest and collect the worklist
    (`error C4235|C4311|C4312|C4244`). For every survivor in a file that an existing wave-2 plan owns,
    that is a wave-2 escape — fix it in place using the matching pattern (asm→intrinsic per RESEARCH
    Patterns 1-4, truncation→width-correct type per the D-06 toolbox, serializer→uint32_t pin per Pattern
    5). For survivors that genuinely CANNOT be validated compile-only here — third-party-lib-dependent TUs
    (need an x64 header/lib that only exists at Phase-33 link time) or runtime-only x64 behavior (e.g. the
    DebugHelp x64 backtrace runtime walk, the FPU-precision door-snap observation) — do NOT force them
    clean; instead write them into `.planning/phases/31-64-bit-correctness-foundation/31-PHASE33-RESIDUAL-WORKLIST.md`
    with: the file:line, the reason it's deferred (third-party / runtime-only), and what Phase 33 must do.
    This artifact is an explicit D-02 deliverable, not a silent omission. The phase bar is "in-scope libs
    compile x64-clean OR documented as residual," not "100% of the build path."
  </action>
  <verify>
    <automated>powershell -File src/build/win32/x64-scratch/compile-all.ps1 -Scope all 2>&1 | grep -cE "error C4235|error C4311|error C4312|error C4244"; test -f .planning/phases/31-64-bit-correctness-foundation/31-PHASE33-RESIDUAL-WORKLIST.md && echo RESIDUAL_OK</automated>
  </verify>
  <acceptance_criteria>
    - the full scratch sweep emits 0 C4235/C4311/C4312/C4244 for the in-scope libs that are NOT in the residual worklist (count above is 0 after excluding documented-residual TUs)
    - every remaining survivor is listed in `31-PHASE33-RESIDUAL-WORKLIST.md` with file:line + deferral reason + Phase-33 action (`echo RESIDUAL_OK` prints)
    - the residual worklist names Phase 33 as the consumer (`grep -c 'Phase 33' 31-PHASE33-RESIDUAL-WORKLIST.md` >= 1)
    - no editor/tool/3rd-party TU (D-03 OUT set) is counted against the gate
  </acceptance_criteria>
  <done>The full in-scope build path compiles x64-clean in the scratch harness except for explicitly-documented third-party/runtime residue, and that residue is handed to Phase 33 in 31-PHASE33-RESIDUAL-WORKLIST.md (D-02).</done>
</task>

<task type="auto">
  <name>Task 2: Committed 32-bit canonical 5-target build + link-grep gate</name>
  <read_first>
    - AGENTS.md "Build" (canonical 5-target list, $env:MSBUILD, /nodeReuse:false, delete exe to force relink, /FORCE link-grep `unresolved external symbol`=0, shared-header ABI cascade trap) and "Diagnostics" if a render anomaly appears
    - the wave-2 SUMMARYs (31-02/03/04/05) — confirm whether any shared-header struct LAYOUT changed (it should not have; Archive.h was local-var + asserts only)
  </read_first>
  <action>
    From PowerShell, delete `src/compile/win32/SwgClient/Debug/SwgClient_d.exe` to force a relink, then run
    the canonical 5-target Debug build: `$env:MSBUILD src/build/win32/swg.sln
    /t:Direct3d11;Direct3d9;Direct3d9_ffp;Direct3d9_vsps;SwgClient /p:Configuration=Debug
    /p:Platform=Win32 /nodeReuse:false`, teeing stdout+stderr to exactly
    `.planning/phases/31-64-bit-correctness-foundation/build-32bit.log` (the path the verify grep below
    reads — e.g. `& $env:MSBUILD ... 2>&1 | Tee-Object .planning/phases/31-64-bit-correctness-foundation/build-32bit.log`).
    Grep the log for `unresolved external
    symbol` — must be 0 (exit 0 is NOT proof; `/FORCE` masks unresolved externals). If any wave-2 plan
    altered a shared-header struct LAYOUT (it should not have — adding static_assert is layout-safe and the
    Archive.h edit was a local-var type), rebuild ALL plugin `.vcxproj`s (gl05/06/07/11) to avoid the stale
    plugin-DLL ABI crash; otherwise the canonical 5-target build already covers the plugins. Confirm
    `stat`/mtime: `gl11_d.dll` is newer than or equal to `SwgClient_d.exe` (no stale-plugin cascade).
  </action>
  <verify>
    <automated>grep -c "unresolved external symbol" .planning/phases/31-64-bit-correctness-foundation/build-32bit.log</automated>
  </verify>
  <acceptance_criteria>
    - the canonical 5-target Debug build completes and the build-log grep `unresolved external symbol` == 0 (the count above is 0)
    - `SwgClient_d.exe` and `gl0{5,6,7}_d.dll` + `gl11_d.dll` are freshly produced (mtime newer than the source edits) and auto-copied to `stage/`
    - no stale-plugin ABI mismatch (gl11_d.dll mtime >= SwgClient_d.exe mtime)
  </acceptance_criteria>
  <done>The committed 32-bit canonical 5-target build links clean (0 unresolved external symbol) with fresh, ABI-consistent plugin DLLs in stage/.</done>
</task>

<task type="checkpoint:human-verify" gate="blocking">
  <name>Task 3: Dual-renderer boot-to-character-select smoke (D-08 / SC#4)</name>
  <read_first>
    - AGENTS.md "Run / renderer switch" (rasterMajor in client_d.cfg for the Debug exe; 5=gl05 D3D9, 11=gl11 D3D11; 9 is FATAL) and "Config (.cfg) safety" (Edit/Write/bash only — PS Set-Content adds a BOM that crashes Release boot)
    - .planning/phases/31-64-bit-correctness-foundation/31-VALIDATION.md "Manual-Only Verifications"
  </read_first>
  <what-built>
    Phase 31's source surgery (FPU/SSE/asm→intrinsics, pointer-truncation fixes, Archive map uint32_t pin,
    packed-struct static_asserts) is complete and the committed 32-bit canonical 5-target build links clean.
    The dual-renderer boot smoke is the D-08 functional regression proof — it exercises live TRE/IFF asset
    load + the login network handshake (proving the BITS-03 layouts still parse) and skeletal skinning
    (proving the BITS-01 SSE rewrite is correct). There is no automated game-boot harness, so this is a
    human-verify checkpoint.
  </what-built>
  <how-to-verify>
    1. Edit `stage/client_d.cfg` with the Edit/Write tool (NOT PowerShell Set-Content — BOM crash): set `rasterMajor=5`.
    2. Launch `SwgClient_d.exe` (Debug exe reads client_d.cfg). Confirm it boots to the character-select screen, characters render (skinned mesh correct), textures/lighting look right.
    3. Edit `stage/client_d.cfg`: set `rasterMajor=11`. Launch `SwgClient_d.exe` again. Confirm boot to character select under D3D11 at the v2.2 parity bar (no new corruption).
    4. Expected: both renderers reach character select with no asset-load failure, no parse error, no skinning/matrix corruption, no new render regression vs the shipped v2.3 baseline.
    5. If either renderer fails to boot or shows new corruption: the SSE/FPU rewrite (plan 02) or a layout/width fix (plan 05) regressed — capture the failure and report it for gap closure.
  </how-to-verify>
  <acceptance_criteria>
    - SwgClient_d.exe boots to character select under rasterMajor=5 (D3D9/gl05) — confirmed by the user
    - SwgClient_d.exe boots to character select under rasterMajor=11 (D3D11/gl11) — confirmed by the user
    - No new asset-load / parse / skinning / render regression vs the shipped v2.3 baseline (user-confirmed)
  </acceptance_criteria>
  <resume-signal>Type "approved" if both renderers boot to character select with no regression, or describe the failure (which renderer, what corruption/crash).</resume-signal>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| source edits → shipped 32-bit build | Any BITS-01/02/03 edit that regresses asset load, network parse, skinning, or render breaks the shipped 32-bit client — the boot smoke is the integration gate |
| shared-header edit → plugin ABI | A layout change in a shared header (should not have occurred) would crash the stale plugin DLLs at boot — the link gate + plugin mtime check catch the cascade |
| /FORCE link → false-clean | /FORCE downgrades unresolved externals to warnings + emits a binary at exit 0 — the link-grep is the real gate |

## STRIDE Threat Register

| Threat ID | Category | Component | Disposition | Mitigation Plan |
|-----------|----------|-----------|-------------|-----------------|
| T-31-16 | Tampering | shipped 32-bit asset/network/render correctness | mitigate | Dual-renderer boot-to-character-select smoke (Task 3) exercises live TRE/IFF load + login handshake + skinning; any regression blocks the phase |
| T-31-17 | Tampering | stale plugin DLL ABI | mitigate | Task 2 link-grep + gl11_d.dll mtime >= SwgClient_d.exe check; rebuild all plugins if any shared-header layout changed (none should have) |
| T-31-18 | Repudiation | /FORCE false-clean link | mitigate | Grep build log for `unresolved external symbol` == 0 (exit 0 not trusted); delete exe to force a real relink |
| T-31-19 | Tampering | residual silently dropped | mitigate | D-02 residual worklist artifact makes every un-validated item an explicit Phase-33 hand-off, not a silent omission |
</threat_model>

<verification>
- Full scratch sweep: 0 C4235/C4311/C4312/C4244 for non-residual in-scope libs; residue documented in 31-PHASE33-RESIDUAL-WORKLIST.md.
- 32-bit canonical 5-target build: 0 `unresolved external symbol`; fresh ABI-consistent plugin DLLs.
- Dual-renderer boot smoke (human-verify): character select under rasterMajor=5 AND =11, no regression.
- D-02 residual worklist hands forward the third-party/runtime items to Phase 33.
</verification>

<success_criteria>
The whole in-scope build path compiles x64-clean in the scratch harness (or is documented as Phase-33
residual per D-02); the committed 32-bit canonical 5-target build links clean (0 unresolved external
symbol) with ABI-consistent plugin DLLs; and the 32-bit client boots to character select under both
`rasterMajor=5` and `=11` with no asset-load / parse / skinning / render regression (D-08 / SC#4). The
Phase 33 residual worklist artifact exists and names its deferred items.
</success_criteria>

<output>
After completion, create `.planning/phases/31-64-bit-correctness-foundation/31-06-SUMMARY.md`
</output>


## Review Instructions

Analyze the plans and provide:

1. **Summary** — One-paragraph assessment
2. **Strengths** — What's well-designed (bullets)
3. **Concerns** — Issues/gaps/risks, each tagged severity HIGH/MEDIUM/LOW
4. **Suggestions** — Specific improvements (bullets)
5. **Risk Assessment** — Overall LOW/MEDIUM/HIGH with justification

Focus especially on (this is a 32-bit → x64 C++ correctness port of a 2010-era SWG game client, MSVC/C++20):
- x87 FPU control-word semantics: does _control87/_controlfp correctly replicate the fnstcw/fldcw behavior (precision/rounding/exception-mask flags, per-thread vs per-process, MSVC _control87 vs _controlfp_s differences)?
- naked-SSE __asm → _mm_* intrinsic conversions: correctness of register/lane mapping, alignment assumptions, denormal handling
- Pointer/int truncation fixes: are the chosen width-correct types (uintptr_t/size_t/INT_PTR) right, and is the scope (touched code only) leaving latent x64 defects?
- Serialization/data-layout: does pinning Archive map sizes to uint32_t + static_asserts actually protect IFF/TRE + network wire-format across 32/64-bit, or are there hidden long/size_t/pointer-width fields in the formats?
- The "compile-only scratch x64 harness" approach — is a Debug|x64 compile-only TU manifest a sound way to drive the worklist without a full x64 link?
- Dependency ordering across the 6 plans / 3 waves, and whether the 32-bit boot-gate regression check is sufficient.
- Scope creep vs leaving the tree half-ported.

Output your review in markdown.
