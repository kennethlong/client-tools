# Phase 35 — Deferred Items (out-of-scope discoveries)

## gl07 (Direct3d9_vsps) x64 PostBuild fails on `%SystemRoot%\Sysnative\d3dcompiler_47.dll`

**Found during:** 35-03 Task 3 (first full `/t:SwgClient /p:Platform=x64` build).
**Pre-existing:** Phase 33-05, commit `9531ba73b` (Direct3d9_vsps.vcxproj x64 PostBuild lines ~335/373). NOT introduced by Phase 35.

**Symptom:** the x64 `/t:SwgClient` build aborts with `MSB3073 ... :VCEnd exited with code 1` in the
gl07 PostBuildEvent — the `copy /Y "%SystemRoot%\Sysnative\d3dcompiler_47.dll" "...\stage-x64\"` step.
`Sysnative` is the 32-bit-process virtual alias to 64-bit `System32`; the VS18 MSBuild runs as a
**64-bit** process where `%SystemRoot%\Sysnative` does not resolve → the copy fails. (`stage-x64\d3dcompiler_47.dll`
already exists from a prior run, so this is purely the copy command failing, not a missing dependency.)

**Why out of scope for 35-03:** it lives in `Direct3d9_vsps.vcxproj`, not in SwgClient.vcxproj or any
Miles file; it is a Phase-33 x64-staging defect. 35-03 only touches the Miles lib/redist wiring.

**35-03 workaround used (does NOT fix the underlying defect):** the gl05/06/07 x64 import libs were
already current from Phase 33 (Jun 17), and clientAudio x64 recompiled cleanly this run, so the SwgClient
x64 link was driven directly with `/p:BuildProjectReferences=false` (plugin DLLs/libs already built).
The link + provenance gates all passed. The gl07 PostBuild copy still needs a real fix for a from-clean
`/t:SwgClient /p:Platform=x64` build.

**Suggested fix (for a future plan, likely Phase 36 verification or a Phase-33 follow-up):** replace
`%SystemRoot%\Sysnative\d3dcompiler_47.dll` with `%SystemRoot%\System32\d3dcompiler_47.dll` in the x64
PostBuild of Direct3d9_vsps.vcxproj (and audit gl05/gl06 x64 PostBuilds for the same pattern), OR guard
the copy with `if not exist "...stage-x64\d3dcompiler_47.dll"`. Verify a from-clean x64 solution build
then completes the gl0x PostBuilds without MSB3073.
