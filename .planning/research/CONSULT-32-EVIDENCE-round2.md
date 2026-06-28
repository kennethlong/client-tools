# ROUND 2 — DE-ANCHORING. A runtime A/B probe has FALSIFIED round 1. Read carefully.

Round 1 (and a follow-up audit) concluded "the Phase-32 shader code is balanced / the leak is the 32-bit
budget or object-loading, not 32-02." **A controlled runtime A/B test has now PROVEN that wrong.** Treat the
LOCKED facts below as measured ground-truth. Do NOT re-derive them, do NOT contradict them, do NOT resurrect
the banned framings. Your job is the MECHANISM and a FIX — not whether the leak exists.

## LOCKED measured ground-truth (do not re-derive)

**A/B isolation — OS-level process Private Bytes, two RELEASE builds, identical config, ONLY the shader code differs:**
- **Release + OLD `D3DXCompileShader`** (`gl05_r.dll`, pre-Phase-32): cantina in/out cycling for ~6.5 min →
  `910 → 918 → 919 → 921 MB` = **+11 MB, FLAT. No leak.**
- **Release + NEW `D3DCompile` + `Direct3d9_HlslRewrite`** (`gl05_r.dll` rebuilt with Phase-32 commits): cantina
  in/out cycling for ~2 min → `720 → 864 MB` = **+144 MB, ~72 MB/min. LEAKING.**
- => **Phase 32's `D3DCompile` vertex-shader-compile path IS the leak.** Same lean Release build both times;
  the ONLY variable is the Phase-32 shader code (commits `f6ffe4570`, `405bba1c1`, `4877f5a94`).

**The leak is in raw PROCESS PRIVATE BYTES. Handles are FLAT (823→815) and threads do NOT grow.** => It is NOT
an engine-object / handle / GDI / engine-pool-object leak. It is raw heap memory — consistent with
`d3dcompiler_47`-internal heap retention per `D3DCompile` call, OR a large raw per-compile allocation that is
not freed. NOT object-graph accumulation.

**It accumulates PER CANTINA CYCLE** (interior↔exterior cell transitions), linearly, ~72 MB/min — it scales
with cell-transition shader activity, not one-time load.

## BANNED framings (falsified — do not propose any of these)
- ❌ "It's not 32-02 / it's the 32-bit 2GB budget / Debug-build bloat / object-loading." FALSIFIED: Release+32-02
  leaks, Release+old-path is flat, SAME build config.
- ❌ "The Phase-32 engine-visible code is allocation-balanced, therefore no leak." The leak is MEASURED. If the
  engine code looks balanced, the retention is `d3dcompiler`-internal OR a conditional/missed non-release you
  have not yet found. "Looks balanced" ≠ "no leak." Find it.
- ❌ "Compiles are bounded by the cache, so a per-compile leak can't accumulate." FALSIFIED: +144 MB of LINEAR
  accumulation proves EITHER (a) compiles are NOT bounded (shaders recompile every cell transition), OR (b) each
  compile retains process-private memory. Determine which — do not assume bounded.

## THE OPEN QUESTIONS (mechanism + can D3DCompile be SAVED?)
A. Do shaders RECOMPILE on every cantina cell transition, or are compiles bounded? Trace the shader-object
   lifetime across `GroundScene` cell load/unload and the `getVertexShader`/`createVertexShader`/`m_container`/
   `m_lastRequestedKey` cache. If recompiling, what tears down + rebuilds the cache per transition?
B. Does `D3DCompile` (d3dcompiler_47) retain process-private memory per call that the static `D3DXCompileShader`
   did not (internal module/reflection/preprocessor/#include cache)? How is it released — a flag, `D3DCompile2`,
   `FreeLibrary`, draining a cache? Is this a known d3dcompiler behavior?
C. Is there a conditional/missed non-release in the new engine path? NOTE: every SWG shader emits a flood of
   `X3206` warnings, so `D3DCompile` returns a non-NULL **error/warnings blob on EVERY successful compile** (not
   just failures). Verify that error blob is released on EVERY path (success-with-warnings, the retained Fix-A
   `__try/__except` path, early returns). Same for the code blob and any include buffer.
D. THE FIX: can `D3DCompile` be kept LEAK-FREE (cache the compiled bytecode so it never recompiles; release the
   d3dcompiler retention; fix a missed blob release)? Or is it an unavoidable d3dcompiler-on-32-bit retention
   that justifies reverting this path to `D3DXCompileShader` and deferring D3DX-removal to x64 (precedent: the
   Phase-27 revert `c0f890875`)? The user WANTS to save the deliverable if it's savable.

Repo root: `D:/Code/swg-client-v2`. Key files: `Direct3d9_VertexShaderData.cpp` (compile path :433-650, include
handler :185-245, getVertexShader cache :654-686), `Direct3d9_HlslRewrite.cpp`. Cite file:line + commit.
