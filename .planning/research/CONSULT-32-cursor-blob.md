FIRST: read `.planning/research/CONSULT-32-EVIDENCE-round2.md` — LOCKED runtime-measured axioms. The leak in Phase-32's D3DCompile path is PROVEN (+144 MB/2min, A/B isolated). Do NOT conclude "balanced / no leak" — it leaks; FIND the missed release.

YOUR ANGLE (line-level reader — Question C: the missed/conditional non-release, under a leak-EXISTS prior): Re-audit the D3DCompile call path in `Direct3d9_VertexShaderData.cpp` (the new compile site ~433-650, the `compileVertexShaderFpGuarded`/`__try/__except` Fix-A wrapper ~110-145, the include handler ~185-245) assuming a per-compile leak IS present — find what is allocated and NOT released on some path.

PRIORITY SUSPECT — the error/warnings blob: every SWG vertex shader emits a FLOOD of `X3206` truncation warnings, so `D3DCompile` returns a **non-NULL `ID3DBlob` error/warnings blob on EVERY SUCCESSFUL compile** (the old `D3DXCompileShader` rarely produced one on success — this is the behavioral delta). Trace EXHAUSTIVELY whether that error blob is `Release()`d on EVERY path:
- success-with-warnings (the common case),
- the retained Fix-A `__try`/`__except`/`_fpreset` fault path,
- any early return / FATAL / branch.
A 1-300 KB warnings blob leaked per compile would exactly match the raw-private-bytes, handle-flat signature.

ALSO check: the code blob (`compiledShader`) release on every path; the include-handler buffers under D3DCompile's Open/Close (does D3DCompile call Close for every Open the same way D3DXCompileShader did?); any `Direct3d9_HlslRewrite` scratch buffer; the `ms_defines` reset.

Deliver: the exact unreleased allocation (file:line + which path misses the release), or — if the error blob IS released everywhere — explicitly rule it out and name the next candidate. Cite file:line. Repo root: D:/Code/swg-client-v2.
