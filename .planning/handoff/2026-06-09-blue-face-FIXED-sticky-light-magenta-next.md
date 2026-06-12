# Handoff 2026-06-09 — Blue NPC face FIXED (sticky dot3 light); particles fixed too; magenta wall-lights NEXT

Branch: `koogie-msvc-cpp20-base` (THE renderer checkout, D:\Code\swg-client-v2). Build: `$MSBUILD` (settings.json)
on `src/engine/client/application/Direct3d11/build/win32/Direct3d11.vcxproj -p:Configuration=Release -p:Platform=Win32 -nologo -m -nodeReuse:false -v:minimal`.
Postbuild stages `gl11_r.dll`; if client is running the copy no-ops, so always
`cp -f src/compile/win32/Direct3d11/Release/gl11_r.dll stage/gl11_r.dll` + verify md5. Boot `SwgClient_r.exe`
(reads `client.cfg`, rasterMajor=11). FULLY RESTART to load a new DLL. RenderDoc MCP + cdb (x86) both worked
this session and were decisive — `renderdoc-cli`/MCP open .rdc, list_draws, pixel_history, debug_pixel/vertex,
get_bindings, get_shader; cdb at `C:\Program Files (x86)\Windows Kits\10\Debuggers\x86\cdb.exe`.

## SHIPPED + VERIFIED THIS SESSION (3 D3D11 fixes, all in gl11_r.dll md5 ce467a45 16... 2026-06-09; UNCOMMITTED)
All built clean, runtime-verified in-game by Kenny. Changed files (4):
`Direct3d11_LightManager.cpp` (+113/-49ish), `Direct3d11_StateCache.cpp` (+33), `Direct3d11_StateCache.h` (+9),
`Direct3d11_StaticShaderData.cpp` (+22). D3D9 tree is CLEAN (probes reverted; empty diff).

1. **Sticky dot3-light snapshot — THE BIG ONE (CONSULT-42). Blue NPC face -> PINK, + particles now work, no
   black-walls regression.** Root cause: `Direct3d11_LightManager`'s dot3 snapshot `s_pixelDot3State` is a
   process-GLOBAL sticky buffer. The CONSULT-38 (`:181` dot3 sun) + CONSULT-39 (`:217` parallel fill) `if(psSlot)`
   gates persisted it across ANY sunless `setLights`. The sorter emits sunless calls, so a PRIOR cantina cell's
   **blue interior fill (parallel[0]=c20-23, the VS main-diffuse)** survived onto the NPC face (whose own list is
   sunless) -> blue face. D3D9 calls `selectLights()` PER DRAW (`Direct3d9.cpp:4012`) which NULLs
   `ms_currentLights.parallel[0..1]` every call (`Direct3d9_LightManager.cpp:380-391`) -> can't stick -> pink.
   FIX: changed the gate `if (psSlot)` -> `if (!lightList.empty())` on all 3 blocks (dot3 sun :181, parallel :217,
   ambient :243): a POPULATED call rebuilds/clears (D3D9 parity); only a TRULY EMPTY popCell `{}` persists
   (black-walls/flicker preserved). Opus proved numerically S1-warm is impossible for a B>R v1; the blue was
   S3 blue-fill via parallel[0]. See `.planning/research/CONSULT-42-SYNTHESIS.md`.
2. **Alpha-fade register leak (CONSULT-40).** dot3 PS output-alpha read `c1.a/c2.a`; D3D11 leaked the sun light's
   diffuse.a(=1.0)/specular.a(=~0.6 spec scale) there instead of the alpha-fade enabled/opacity, forcing every
   dot3 surface into the fade branch -> translucent/washed faces. FIX: `setAlphaFadeOpacity` (`Direct3d11_StateCache.cpp:2025`)
   now records enabled/opacity (+ getters in the .h); `StaticShaderData.cpp:962-985` packs c1.a=enabled?1:0,
   c2.a=opacity (mirrors `Direct3d9_LightManager.cpp:582,584`); removed the old c0-clobbering updatePS(0). RenderDoc-verified.
3. **Sonnet parallel-diffuse wipe (CONSULT-39)** — was an interim fix (the `if(psSlot)` parallel gate); the CONSULT-42
   change SUPERSEDES/evolves it (now `!lightList.empty()`). Don't treat #3 as separate any more.

## METHODOLOGY THAT WON (worth repeating)
- The cross-renderer **runtime light-color DUMP** (same fopen_s probe added to BOTH `Direct3d11_StaticShaderData::apply`
  AND `Direct3d9_LightManager::selectLights`, value-deduped) was THE decisive tool. It proved the interior light
  COLORS are byte-for-byte IDENTICAL D3D9-vs-D3D11 -> killed the Red<->Blue swap hypothesis and reframed it as
  "which STATE lands on her face" (selection/stickiness), not the colors.
- Probe TIMING matters: first dumps captured BOOT/char-select (flat white, no time-of-day drift) and missed the
  cantina. Value-dedup + skip-empty/flat-white made it survive to the world. Gate on a real frame, not the first N.
- The 4-AI crew caught THREE wrong consensus turns before the answer (alpha tangent-floor, hemispheric sky-blue,
  R<->B swap) — RenderDoc/dump hardware truth broke each. consensus != correctness; the dump is the arbiter.
- Opus's numeric back-solve (impossible-by-construction proof for S1) + Cursor/Codex order-trace converged on the
  parallel[0] line. Sonnet's refutations pruned full-ambient-bleed.

## NEXT TARGETS
1. **PRIMARY (Kenny's next): MAGENTA wall-lights in the cantina.** Magenta == the fallback pixel shader -> one
   cantina light-FIXTURE `.sht` is failing PS-gen/bind and dropping to the magenta fallback. Self-contained: find
   which shader template the wall-light fixtures use (RenderDoc: pixel_history a magenta pixel -> the draw -> get_shader),
   see why its PS doesn't compile/bind (likely an asset-PS-gen gap, the v2.2 slot0/PSRC lane). Kenny called it "an easy target."
2. Distance FOG still missing in D3D11 (D3D9 fixed-function `D3DFOG_EXP2` vertex fog; no D3D11 shader fog). Separate
   feature: VS fog factor from view depth + PS lerp-to-fog-color. `setFog` currently writes PS b0 c0 but nothing reads it.
3. (Resolved/була: the "faces too light" and cape-spike items from prior handoffs are DONE.)

## STATE TO RESTORE
- gl11_r.dll md5 **ce467a45...**, gl05_r.dll **384034d2...** staged. 4 D3D11 files uncommitted (above). D3D9 clean.
- NOT committed yet — Kenny hasn't asked. The repo is dirty with Koogie WIP; if committing, gate on an EXACT staged
  set (see memory feedback_gate_closeout_commits_on_dirty_repo) — do NOT `git add -A`.
- Synthesis docs: `.planning/research/CONSULT-40/41/42-SYNTHESIS.md` + the per-angle `.out` files.
