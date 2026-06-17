---
phase: 33
reviewers: [codex, cursor, sonnet, opus]
reviewed_at: 2026-06-17
plans_reviewed: [33-01-PLAN.md, 33-02-PLAN.md, 33-03-PLAN.md, 33-04-PLAN.md, 33-05-PLAN.md, 33-06-PLAN.md]
self_skipped: claude (running inside Claude Code — skipped for independence)
---

# Cross-AI Plan Review — Phase 33 (x64 Build Platform + D3D9 Renderers)

Four independent reviewers with read-only repo access, fanned out on different angles:
**Codex** (repo tracer / call-graph), **Cursor** (detailed file:line code reader), **fresh
Sonnet** (lateral / blind-spot), **fresh Opus** (spec/logic rigor + Phase-32 precondition
verification). Each verified the plans' factual claims against the live tree. Raw outputs:
`.planning/research/gsd-review-33-{codex,cursor}.out` (CLI) + the Sonnet/Opus agent transcripts.

**Orchestrator cross-check (independent grep):** `Direct3d9_VertexShaderData.cpp:34` includes
`<d3dcompiler.h>` and `:183` calls `D3DCompile` — the Phase 32-02 HLSL port **is in the live
committed source**; `:760` still calls `D3DXAssembleShader` (Plan 02's target) and `:32-33`
still include `<d3dx9.h>`/`<d3dx9shader.h>` (Plan 03's target). All four reviewers independently
confirmed this. **The ROADMAP showing 32-02 unchecked is artifact drift, NOT a real blocker.**

---

## Codex Review

### Summary
The six-plan sequence is directionally sound. Would NOT rubber-stamp execution as written. The
biggest blocker is that `x64-platform.props` is copied from a Debug static-library scratch harness
but then imported into Release, DLL, and EXE projects — that compiles the wrong preprocessor
surface into x64 Release/plugins/client unless corrected. Also verification holes around
surface-copy assumptions, Miles stubbing, DebugHelp stack-walk validation, tinyxml library
naming, and grep-based checks.

### Strengths
- Correct high-level ordering: D3DX removal before x64 link, then platform-add, then link, then boot validation.
- Phase-32 HLSL precondition is artifact drift, not a real blocker: ROADMAP shows 32-02 unchecked (`ROADMAP.md:151`) but handoff says complete (`handoff/phase-32-d3dcompile-port.md:15-16`) and live code calls `D3DCompile` (`Direct3d9_VertexShaderData.cpp:183`).
- Research correctly identifies Bink as dynamic-load only (`BinkVideo.cpp:55,100`) and Miles as the real x64 link blocker (`SwgClient.vcxproj:103`).
- `/FORCE` false-clean handled explicitly (`33-05-PLAN.md:188,196`; `33-06-PLAN.md:139,145`).
- `stage-x64/` isolation is the right protection against mixed-bitness stage failures (`33-06-PLAN.md:85-97`).

### Concerns
- **HIGH — `x64-platform.props` is wrong as a universal props file.** Plan 01 copies a platform-only block with `_DEBUG;DEBUG_LEVEL=2;_LIB` (`33-01-PLAN.md:76-78`) and Plans 04/05 import it into Release configs, DynamicLibrary projects, and the Application project (`33-04-PLAN.md:79-85`; `33-05-PLAN.md:148,178`). Existing projects have target/config-specific defines: D3D9 DLL uses `_USRDLL;DIRECT3D9_EXPORTS` and Release uses `NDEBUG;DEBUG_LEVEL=0` (`Direct3d9.vcxproj:91,152`); SwgClient Release uses `NDEBUG;DEBUG_LEVEL=0` (`SwgClient.vcxproj:195`). Can produce a bogus x64 Release/client/plugin.
- **HIGH — Plan 03's surface-copy census is shaky.** Claims 8 `D3DXLoadSurfaceFromSurface` sites (`33-03-PLAN.md:76-82`), but live grep shows **4** (`Direct3d9_RenderTarget.cpp:317`; `Direct3d9_TextureData.cpp:549,603,681`). The same-format/no-conversion assumption is asserted, not proven (`33-03-PLAN.md:134-145`); D3D11 code notes D3DX/StretchRect format permissiveness (`Direct3d11_RenderTarget.cpp:91`).
- **HIGH — Miles stubbing under-specified for a 163-call-site surface.** Plan 05 allows "each AIL_* call site" or broad function wrapping (`33-05-PLAN.md:122`) verified mostly with grep/build. Live: init (`Audio.cpp:1280,1284`), playback (`:2909,3018,3109`), 3D listener (`SoundObject3d.cpp:32-34`). Needs a defined stub boundary + object/symbol-level proof that x64 emits zero `AIL_*` references.
- **MEDIUM — tinyxml output/name contract mismatch.** Plan 01 builds `tinyxml.lib` (`33-01-PLAN.md:181-186`) but SwgClient links `tinyxmld.lib;tinyxmld_STL.lib` (`SwgClient.vcxproj:103`); Plan 05's link-delta never maps the swap. Avoidable x64 LNK1181 / wrong-STL-flavor.
- **MEDIUM — Plan 01 ICU/discord N/A grep is invalid.** `grep -rl "icu\|discord" src/` — `icu` matches ordinary words like "partic**u**lar" tree-wide, so the gate fails noisily or is misread. Use path/vendor/exact-token checks.
- **MEDIUM — Plan 02 overstates D3DAssemble certainty.** Calls the port "mechanical" on a 6-shader PASS (`33-02-PLAN.md:33`); the Phase-32 review framed it as PASS-on-sample, not full corpus. Rollback path helps, but acceptance shouldn't imply all 96 asm shaders are proven.
- **MEDIUM — Plan 06 makes stack-walk validation optional** (`33-06-PLAN.md:117,121-123`). Doesn't validate A1-DBGHELP-WALK; live code narrows stack entries to `uint32` (`DebugHelp.cpp:495,574,586,610`). Deserves a real controlled x64 stack test.
- **LOW — Plan 03 save-to-file should use the existing D3D9 `WriteTGA` path** (`Direct3d9.cpp:46,2871`) not a generic sharedImage writer — the lower-risk local pattern.

### Risk Assessment: **MEDIUM-HIGH** — architecture/ordering good enough to reach the goal, but the Debug/static-lib props imported everywhere + the under-specified surface-copy/Miles-stub work can directly derail or miscompile the x64 build. Fix those and it drops toward MEDIUM.

---

## Cursor Review

### Summary
The six-plan wave structure is directionally correct and matches the locked decisions. Ground
truth confirmed: no `x64-platform.props`, no `Debug|x64` in `swg.sln`, `D3DXAssembleShader` still
live at `:760`, HLSL `D3DCompile` path landed (32-02 in code, ROADMAP unchecked). Several factual
census errors, dependency edges that are too loose, and Release|x64 / tinyxml / matrix-path blind
spots could burn executor time or pass green gates that miss gl06/gl07 regressions. Executable
with fixes, not rubber-stamp.

### Strengths
- D3DX-before-x64-link ordering is sound (`Direct3d9.vcxproj:115` still lists `d3dx9.lib` today).
- Research corrections verified: Bink dynamic (`BinkVideo.cpp:55`), dpvs `!_M_X64` guard (`dpvsPrivateDefs.hpp:129-131`), icu/discord absent, `/FORCE` trap documented.
- Plan 02/03 ownership split for `Direct3d9_VertexShaderData.cpp` is coherent (`:155-156` comment confirms asm path untouched).
- Miles stub decision locked & necessary (`SwgClient.vcxproj:103`; 163+3 `AIL_*` refs).
- Non-regression architecture well designed: distinct `\x64\` OutDirs, separate `stage-x64/`, Win32 `#else` branches preserved.
- Human-verify gates well placed (02-T3, 03-T4, 06-T2, 06-T3).

### Concerns
- **HIGH — 33-01-T1/04: `x64-platform.props` hardcodes `_DEBUG`/`DEBUG_LEVEL=2` with a `Platform=='x64'`-only condition — no Debug vs Release split.** Release|x64 projects importing it compile with `_DEBUG`. Plans add Release|x64 everywhere but validate mostly on Debug.
- **HIGH — 33-04 depends_on too loose.** Plan 04 depends only on 33-01, not 33-03. Wave 3 can start while D3DX is still in plugin source; 04-T3 adds sln x64 plugin mappings before 05 adds the plugin vcxproj x64 blocks → premature `/p:Platform=x64 /t:Direct3d9` fails confusingly.
- **HIGH — 33-03-T1 / Pitfall 1: `:4031` `D3DXMatrixMultiplyTranspose` is inside `#ifdef FFP` within `#ifdef VSPS` (`Direct3d9.cpp:4014-4036`).** gl05=FFP+VSPS (transpose path); gl07=VSPS-only (else branch, **different multiply order**, no transpose); gl06=FFP-only (**entire VSPS block excluded**). Plan 03 A/B is **gl05/Tatooine-only** → can miss gl06/gl07 matrix regressions required by X64-02 (rasterMajor 6/7).
- **HIGH — 33-05-T1: the 163-site Miles stub is high blast-radius.** Miss one `AIL_*` → x64 link unresolved despite the `/FORCE` grep. Acceptance ("grep `_M_X64` >= 1") is too weak vs "zero `AIL_*` in x64 TU."
- **MEDIUM — surface-copy count wrong: 4 live sites, not 8** (`Direct3d9_RenderTarget.cpp:317`; `Direct3d9_TextureData.cpp:549,603,681`). Plans over-scope Task 2 and under-specify per-site format checks (A3).
- **MEDIUM — 33-01-T3 tinyxml OutDir depth likely wrong.** Plan uses 5 `..\` levels from `tinyxml/build/win32/`; peer `udplibrary.vcxproj` uses 4, `dpvs.vcxproj` uses 6 — expect output in the wrong place.
- **MEDIUM — tinyxml link name:** SwgClient links `tinyxmld.lib;tinyxmld_STL.lib`; Plan 01 outputs `tinyxml.lib`; Plan 05 never maps the rename → likely LNK2019.
- **MEDIUM — 33-02-T3:** 32-01 probed 6/96 asm shaders; Tatooine smoke is necessary but not sufficient — acceptable if documented as sampling risk.
- **MEDIUM — 33-03-T3 A4:** `optimizeIndexBuffer` no-op reasonable, but NvTriStrip is already in SwgClient link (`NvTriStrip.lib` in `:103`) — plan skips the in-repo tool without noting it.
- **MEDIUM — 33-06-T1 dpvs.dll staging:** dpvs is a DynamicLibrary with no postbuild-to-stage; checklist includes `dpvs.dll` but Plan 05 postbuild only covers exe/plugins → manual step easy to miss.
- **LOW** — 33-CONTEXT "committed x64-platform.props" implies done state (file doesn't exist yet); "~57" vs "~53 engine + 4 + 3 + 1" count (`sharedTemplateDefinition` exclusion correct — absent from SwgClient closure); A2-TIME-T serialized audit has no owning task; 33-VALIDATION still draft (`nyquist_compliant:false`).

### Risk Assessment: **MEDIUM-HIGH** — architecture/sequencing among the best-aligned in the milestone; risk concentrates in execution details (Release props, three-flavor matrix `#ifdef` geometry, 163-site Miles completeness, tinyxml path/naming, under-specified renderer A/B for rasterMajor 6/7). None are design-fatal; several fixable in plan text before execute.

---

## Sonnet Review (lateral / blind-spot)

### Summary
Credible architecture for the first fully-linking x64 client; wave sequencing sound; research/
patterns unusually thorough. Three issues could each independently prevent a booting x64 client
and are not fully addressed: (1) `optimizeIndexBuffer` is part of the GL API contract, not a local
no-op; (2) the D3DCompile/D3DAssemble export resolution on the x64 DLL is unverified; (3) the
import-lib generation has no export-name-match validation.

### Strengths
- Wave ordering logically correct for the x64-link precondition; `depends_on` edges faithfully declared.
- Phase-32 precondition correctly assessed — `D3DCompile` already at `:183`, `D3DXAssembleShader` sole remaining compile symbol at `:760`; Plan 02 correctly scopes to the asm path only.
- Bink finding correct and saves real work (`BinkVideo.cpp:55,100`).
- Miles stub decision sound & explicit (163 `AIL_*` sites confirmed); does not lift `mss64.dll` prematurely.
- dpvs CPU-detect gotcha correctly located (`dpvsPrivateDefs.hpp:129-130`); C `#else` fallbacks verified present.
- `/FORCE` link-grep mandate prominently placed; `stage-x64/` isolation enforced; D3DAssemble dialect gate already PASSED (32-01-SUMMARY.md); `_USE_32BIT_TIME_T` handled once in shared props.

### Concerns
- **HIGH — `optimizeIndexBuffer` is a GL API contract slot, not a local no-op** (`Gl_dll.def:220`, registered at `Direct3d9.cpp:1251`, exposed via `Graphics.cpp:3379`, called from `clientSkeletalAnimation/SoftwareBlendSkeletalShaderPrimitive.cpp:1421`). Plan 03-T3's "no-op pass-through" is ambiguous about whether the slot is set to a real empty-body function or left unset. If unset, callers hit a **null function pointer → D3D9 boot crash on skeletal-mesh optimize** — endangering the 32-bit non-regression gate (SC#4) before x64 work begins. The D3D11 `STUB(optimizeIndexBuffer)` at `Direct3d11.cpp:1185` is the reference pattern. Also: the "NvTriStrip is already linked" alternative (33-RESEARCH A4) is a **phantom** — `GenerateStrips`/`PrimitiveGroup` do not appear in the Direct3d9 plugin source.
- **HIGH — `D3DAssemble` x64 export not confirmed.** The 32-01 probe ran against the **x86** staged `d3dcompiler_47.dll` ("D3DAssemble ordinal 1, undecorated, x86 staged DLL"). No acceptance criterion confirms `GetProcAddress("D3DAssemble")` resolves from the **x64** `C:\Windows\System32\D3DCompiler_47.dll`. If the export differs, `s_d3dAssemble` is null on x64 → FATAL on every asm shader → boot crash. Add `dumpbin /exports <x64 dll> | grep D3DAssemble` to Plan 02 or 06.
- **HIGH — pcre import-lib export-name mismatch.** Live link uses `libpcre.a` (MinGW archive, typically undecorated `pcre_exec`/`pcre_compile`); Plan generates `pcre-x64.lib` from Restoration's `pcre.dll` (possibly MSVC-decorated exports). A name mismatch yields a machine-x64 lib whose symbols don't resolve → LNK2019 only visible at Plan 05's full link. Plan 01-T2 verifies machine but not export-name match. Spot-check `dumpbin /exports pcre.dll | grep pcre_` against `grep -r pcre_ src/engine/shared/library/sharedRegex/`.
- **MEDIUM — typedef-swap coupling on the HLSL path.** `D3DXMACRO` is the element type of `ms_defines` (`:66`) used by BOTH paths; the HLSL `reinterpret_cast<ID3DInclude*>`/`ID3DBlob` (`:178-183`) and the `IncludeHandler` vtable (`:59-63`) rely on ABI-identity. An executor in Plan 03 touching the `IncludeHandler` class could subtly break the HLSL path. Require reading the `:153` reinterpret-cast comment first.
- **MEDIUM — Plan 04 sln plugin `.Build.0` before Plan 05 adds plugin `ProjectConfiguration`.** After Plan 04, `swg.sln` has `.Debug|x64.Build.0` for the plugin GUIDs but the plugin `.vcxproj`s have no `Debug|x64` ProjectConfiguration yet → `MSB8020` if an x64 solution build runs between Plan 04 and 05. Add the plugin `ProjectConfiguration` entries (no Link blocks) in Plan 04.
- **MEDIUM — `stage-x64/client_d.cfg` `rasterMajor` not set in Plan 06-T1.** The cfg-edit instruction lives in Task 2's how-to-verify, not Task 1's action; the copied cfg could carry a stale `rasterMajor` (incl. the FATAL `9`). Move "set `rasterMajor=5`" into Task 1.
- **MEDIUM — `D3DXSaveSurfaceToFile` `.bmp`→`.tga` extension.** `:2890` passes `D3DXIFF_BMP`; writing TGA "instead" can produce a `.bmp`-named TGA if the filename string hard-codes `.bmp`. Inspect the filename construction at `:2880–2895`.
- **MEDIUM — `Direct3d9_TextureData.cpp:681` `copyFrom()` uses non-NULL src+dst rects** (potentially different sizes = a scaled blit). 7/8 sites use NULL (whole-surface); this 8th is the asymmetric case where a point-copy StretchRect/memcpy would truncate/garble. Call it out explicitly for a required pre-read.
- **MEDIUM — `x64-platform.props` `_DEBUG` hardcoded** (converges with Codex/Cursor) — Release|x64 silently enables debug-only paths; no acceptance criterion catches it.
- **LOW** — dpvs may carry `DPVS_ASSERT(sizeof(void*)==4)` boot-time asserts not visible as C4311/C4312; the truncation pass should report a count of fixes, not just "build succeeds." Matrix "one op at a time" is advisory not enforced (note `:3357`/`:3359` have reversed arg order vs `:3291`/`:4034`). The x64 `d3dcompiler_47.dll` copy from `C:\Windows\System32\` may hit the **WoW64 redirector** if MSBuild is 32-bit — use `Sysnative`.

### Risk Assessment: **MEDIUM** — well-researched, ordering sound, pitfalls enumerated. Not LOW because: (1) `optimizeIndexBuffer` no-op can crash the 32-bit build's skeletal path (SC#4 gate); (2) the props `_DEBUG` miscompiles Release|x64 undetected; (3) pcre/libxml2/jpeg import-libs rest on name-compatible exports (pcre structurally suspect — MinGW `.a` → MSVC DLL). Each has a clear low-effort fix; with the three HIGH items addressed, risk → LOW.

---

## Opus Review (spec/logic rigor)

### Summary
A well-sequenced, correctly-grounded path to a first fully-linking boot-to-char-select x64 client.
The highest-stakes premise — that the HLSL path already migrated so Plans 02/03 land only the
residual D3DX — is **TRUE in live committed source** (`Direct3d9_VertexShaderData.cpp:183` calls
`D3DCompile`, committed in `405bba1c1`/`ff02a367e`; `:760` still `D3DXAssembleShader`). Dependency
ordering, wave edges, `/FORCE` grep, `stage-x64/` isolation, dumpbin gates are real signals. The
dominant residual risk is Plan 05's Miles `#if _M_X64` stub — materially under-specified for a
166-call-site handle/HRESULT/type-returning surface.

### Strengths
- Phase-32 precondition is real, not drift — `git log` confirms two committed `feat/fix(32-02)` commits; plans trust source over artifact (per AGENTS.md "builds are truth").
- Precondition-before-platform-add ordering correct; 32-bit non-regression held via distinct OutDirs + separate stage.
- `/FORCE` trap treated as primary acceptance signal everywhere (02-T1, 05-T2, 05-T3, 06).
- DirectXMath transpose risk handled with discipline (one op at a time, `:4031` preserved, `Direct3d11_StateCache.cpp:385-395` precedent; `NO PACK_MATRIX_ROW_MAJOR` already baked into the HLSL guard `:180-182`).
- OutDir/stage isolation rigorous (`git status` additive-only as acceptance).
- dpvs Pitfall 4 located exactly (`dpvsPrivateDefs.hpp:129-131` confirmed, no `_M_X64` check); MAC/HPUX/PS2 branches as in-file precedent.
- icu/discord N/A backed by `grep -rl` zero-ref check, not hand-waved.

### Concerns
- **HIGH — Plan 05-T1 Miles stub under-specified for the actual API surface.** 163 `AIL_` refs in `Audio.cpp` (+3 in `SoundObject3d.cpp`) span handle returns (`AIL_open_stream`/`AIL_allocate_sample_handle` → `HSAMPLE`/`HSTREAM`), int returns (`AIL_active_sample_count`), type-bearing (`S32 AIL_file_type`), string returns (`AIL_last_error`), and MSS types/constants (`DIG_MIXER_CHANNELS`, `MSS_MC_STEREO`, `HDIGDRIVER s_digitalDevice2d`). A blanket "return success" leaves callers holding null/garbage handles flowing into `AIL_set_sample_3D_position`, map inserts, status loops. To get zero `AIL_*` AND not DEBUG_FATAL/crash, guard the handle-bearing data members and the loops too — closer to "compile out the clientAudio Miles subsystem" than "stub a few functions." Scope it as gating the whole `s_digitalDevice2d`-rooted subsystem behind `#if !_M_X64`, with public entry points as no-op shells, as its own sub-task.
- **MEDIUM — file-ownership coupling on `Direct3d9_VertexShaderData.cpp`.** Both Plan 02 (asm call + Fix-A) and Plan 03-T3 (residual typedef swap + drop `d3dx9*.h`) modify it; the live `:160-163` comment says Fix-A removal is a "single isolated Wave-2 change." `depends_on:["33-02"]` serializes correctly, but state the file-ownership boundary explicitly so a Plan 03 executor doesn't "helpfully" also touch the asm path.
- **MEDIUM — Fix-A *retirement* is marked "preferred" (02-T2) but guards a real observed crash** (0xC0000090 FP fault). Its verify is only a link grep; the render smoke (02-T3) tests the asm path, not that retiring the SEH guard on the HLSL path didn't reintroduce the FP-crash class on a `.vsh` 32-02 never exercised. **Default to retain-with-comment**; if retiring, require the full //hlsl+//asm render smoke after the retirement.
- **MEDIUM — A3 surface-copy rests on an unverified same-format/same-dimension assumption** across the sites; `StretchRect` has device/pool/format constraints and `D3DXLoadSurfaceFromSurface` silently did conversion+filtering. The A/B is char-select/Tatooine only — a texture-download/screenshot path elsewhere could regress unnoticed. Record actual src/dst `D3DFORMAT`+dims per site in the SUMMARY.
- **LOW** — A4 mesh no-op needs the load-bearing check actually performed (not eyeballed); 05-T3 repoints "EVERY lib dir" to `\x64\` but editor/Qt/Maya/Perforce libs stay x86 under `/FORCE` → a non-zero unresolved count must be triaged against the editor-lib list, not assumed a boot-path miss; A2-TIME-T serialized re-audit is a floating "Wave B" item with no owning task — state it deferred-or-done; A1-DBGHELP-RIP "OR document accepted truncation" is a soft out for an x64 crash-handler — prefer the widen.

### Risk Assessment: **MEDIUM** — foundational premise verified correct against live source; ordering, wave edges, OutDir/stage isolation, `/FORCE` grep, dpvs guard lines (129-131), swg.sln config block (1583-1606, 127 projects), SwgClient link inventory all factually accurate (why it's not HIGH). Not LOW because of three convergent execution risks: the large under-specified Miles stub, the Fix-A retirement marked "preferred" despite guarding a real crash, and correctness-sensitive surface/matrix conversions validated only at one scene. Tightening the Miles scope + flipping Fix-A to "retain" → LOW.

---

## Consensus Summary

### Agreed Strengths (2+ reviewers)
- **Wave ordering is correct** — D3DX removal on the still-32-bit tree (02→03) before the platform-add (04) and first x64 link (05), with boot validation last (06). All four.
- **The Phase-32 (32-02) HLSL→D3DCompile precondition is LANDED in live source** — `Direct3d9_VertexShaderData.cpp:183`; the ROADMAP checkbox is artifact drift, not a blocker. All four + orchestrator grep. (32-03 asm + 32-05 Fix-A correctly absorbed into 33-02.)
- **`/FORCE` link-grep as the primary x64-link acceptance signal** is prominent and load-bearing everywhere. All four.
- **`stage-x64/` isolation + distinct `\x64\` OutDirs** correctly protect the 32-bit non-regression gate (SC#4). All four.
- **Research corrections verified** — Bink is dynamic-load (no import lib), dpvs needs the exact `!_M_X64` guard at `dpvsPrivateDefs.hpp:129-131`, icu/discord absent from the tree. All four.
- **Human-verify checkpoints well placed** (02-T3, 03-T4, 06-T2, 06-T3). Cursor, Sonnet.

### Agreed Concerns (highest priority — raised by 3-4 reviewers)

1. **[HIGH — 4/4] `x64-platform.props` is mis-designed as a universal import.** It hardcodes `_DEBUG;DEBUG_LEVEL=2` under a `Platform=='x64'`-only condition, so **Release|x64 compiles with `_DEBUG`** (silently enabling debug-only FATAL/memory-tracker paths). Codex adds it also lacks ConfigurationType-specific defines the existing projects carry (`_USRDLL;DIRECT3D9_EXPORTS` for the DLLs; `NDEBUG;DEBUG_LEVEL=0` for Release — `Direct3d9.vcxproj:91,152`, `SwgClient.vcxproj:195`). **No acceptance criterion catches this.** → Split into `Debug|x64` / `Release|x64` ItemDefinitionGroups (or `'$(Configuration)|$(Platform)'` conditions); keep config/type-specific defines per-project. (33-01-T1)

2. **[HIGH — 4/4] The Miles `#if _M_X64` stub is materially under-specified** for the 163-site (+3 in SoundObject3d) handle/HRESULT/type-returning surface (`HSAMPLE`/`HSTREAM`/`S32 AIL_file_type`/`HDIGDRIVER s_digitalDevice2d`, MSS constants). "Return success" leaves null/garbage handles flowing into downstream calls. → Re-scope as a **subsystem-disable** (gate the `s_digitalDevice2d`-rooted state + loops, public clientAudio entry points become no-op shells on x64), as its own sub-task, with **symbol-level proof** (`dumpbin /symbols clientAudio…x64 | grep AIL_` == 0), not just `grep _M_X64 >= 1`. (33-05-T1)

3. **[HIGH — 4/4] Surface-copy census is wrong (4 sites, not 8) AND the same-format/same-dimension assumption is unproven.** Live: `Direct3d9_RenderTarget.cpp:317`; `Direct3d9_TextureData.cpp:549,603,681`. The `:681` `copyFrom()` site uses non-NULL src+dst rects (potential scaled blit) — the asymmetric case a naive point-copy would garble. → Correct the count; add a pre-task recording each site's `D3DFORMAT`+pool+dims+rects before choosing StretchRect vs lock-blit; call out `:681` explicitly. (33-03-T2, 33-RESEARCH)

4. **[MEDIUM — 3/4] tinyxml output-name contract mismatch.** Plan 01 builds `tinyxml.lib`; SwgClient links `tinyxmld.lib;tinyxmld_STL.lib` (`SwgClient.vcxproj:103`); Plan 05 never maps the rename → x64 LNK1181/2019 or wrong STL flavor. Cursor adds the OutDir `..\` depth is likely wrong (5 levels vs peer 4/6). → Match output names (+ `TINYXML_USE_STL` consistently) and add tinyxml to `swg.sln`. (33-01-T3, 33-05-T3)

5. **[MEDIUM — 4/4] "D3DAssemble port is mechanical" overstates a 6/96-shader sample.** The 32-01 gate PASSED byte-identical on 6 probed shaders; the full 96-//asm corpus is not proven. → Keep the asm render smoke (02-T3) as THE gate and label the sampling limitation explicitly in acceptance. (Sonnet/Codex add: the probe ran against the **x86** d3dcompiler_47.dll — add a `dumpbin /exports` check that the **x64** DLL also exports `D3DAssemble`.) (33-02)

### Divergent / single-reviewer findings (high value — investigate)
- **[Sonnet, HIGH] `optimizeIndexBuffer` is a GL-API function-pointer slot** (`Gl_dll.def:220`, registered `Direct3d9.cpp:1251`, called from `SoftwareBlendSkeletalShaderPrimitive.cpp:1421`). A "no-op" that leaves the slot unset → **null-pointer boot crash in the 32-bit skeletal path** (SC#4 risk). Mirror D3D11's `STUB(optimizeIndexBuffer)` (`Direct3d11.cpp:1185`). The "NvTriStrip already linked" alternative is a **phantom** (not in the Direct3d9 plugin source). *Standout find — the others missed this.*
- **[Cursor, HIGH] The `:4031` transpose lives inside `#ifdef FFP` within `#ifdef VSPS`** (`Direct3d9.cpp:4014-4036`) — gl05 (FFP+VSPS) takes the transpose path, gl07 (VSPS-only) takes an **else branch with different multiply order**, gl06 (FFP-only) excludes the VSPS block entirely. **gl05-only A/B misses gl06/gl07 matrix regressions** that X64-02 (rasterMajor 6/7) requires. → Expand 03-T4/06-T2 to A/B all three renderers. *Standout find.*
- **[Codex] The ICU/discord N/A grep `grep -rl "icu\|discord"` is invalid** — `icu` matches "partic**u**lar" etc.; the gate fails noisily. Use exact-token/path/vendor checks. (33-01-T3)
- **[Opus] Fix-A retirement is marked "preferred" but guards a real observed crash** (0xC0000090) — default to retain-with-comment; if retiring, run the full render smoke after. (33-02-T2)
- **[Sonnet/Opus] File-ownership coupling on `Direct3d9_VertexShaderData.cpp`** across Plans 02/03 — state the boundary explicitly (02 owns asm+Fix-A; 03 owns typedef swap + `d3dx9*.h` drop); the HLSL `reinterpret_cast`/`IncludeHandler` vtable is ABI-sensitive.
- **[Sonnet] pcre import-lib export-name mismatch** (MinGW `libpcre.a` undecorated vs Restoration MSVC `pcre.dll`) → LNK2019 only at Plan 05. Spot-check export names in Plan 01-T2.
- **[Codex] Save-to-file should reuse the existing D3D9 `WriteTGA` path** (`Direct3d9.cpp:46,2871`), not a generic sharedImage writer. (33-03-T3)
- **[Sonnet] Plan 04 sln plugin `.Build.0` added before Plan 05 adds the plugin `ProjectConfiguration`** → `MSB8020` if an x64 build runs between the plans. Add the plugin ProjectConfiguration (no Link block) in Plan 04. (Cross-ref Cursor's 33-04 depends_on concern.)
- **[Codex/Opus/Sonnet] Stack-walk validation (A1-DBGHELP-WALK) is optional in Plan 06** — make a controlled x64 stack test required; prefer the A1-DBGHELP-RIP widen (`DebugHelp.cpp:574`) over "document truncation."
- **[Sonnet] `stage-x64/client_d.cfg` `rasterMajor` not set in 06-T1** (could carry stale/FATAL `9`); the `.bmp`→`.tga` extension may mismatch the filename string; the x64 `d3dcompiler_47.dll` `System32` copy may hit the **WoW64 redirector** (use `Sysnative`).
- **[Cursor/Opus] A2-TIME-T serialized re-audit is a floating residual with no owning task** — make it an explicit deferred-or-done line item.

### Divergence on the Plan-04 dependency edge
Cursor argues **33-04 should depend on 33-03** (don't advertise buildable x64 plugin configs before D3DX is cleared). Codex argues the **33-01-only edge is acceptable** because Plan 04 is static-lib/platform work that doesn't link the plugins. **Resolution:** both are reconcilable — Plan 04's *engine static-lib* x64 configs genuinely only need 33-01, but its *sln plugin mappings* (04-T3) shouldn't be buildable until the plugin `ProjectConfiguration` + Link blocks exist (05) and D3DX is gone (03). Cleanest: keep 04's lib work on 33-01, but defer/guard the plugin+SwgClient `.Build.0` sln rows so an x64 solution build between waves can't try to build a not-yet-configured plugin.

### Overall risk (consensus): **MEDIUM** (Cursor & Codex lean MEDIUM-HIGH)
The architecture, wave ordering, and the load-bearing gates (`/FORCE` grep, stage isolation, dpvs guard) are correct and verified against the live tree — this is **not** a design-flawed plan. Risk concentrates in **execution details that no current acceptance criterion catches**: the Release|x64 props miscompile, the under-specified Miles subsystem-disable, the surface-copy format assumptions, the `optimizeIndexBuffer` null-slot crash, and the gl05-only matrix A/B. All are fixable in plan text before execution. The first x64 link (05-T3) will be a multi-iteration unresolved-symbol chase regardless — expected and documented.

### Recommended pre-execution fixes (priority order)
1. Fix `optimizeIndexBuffer` to register a callable empty-body function (mirror D3D11 `STUB`) — **SC#4 crash risk** (Sonnet). [33-03-T3]
2. Split `x64-platform.props` Debug|x64 / Release|x64 + restore ConfigurationType-specific defines — **silent Release miscompile** (all 4). [33-01-T1]
3. Re-scope the Miles stub as a subsystem-disable with symbol-level (`dumpbin /symbols … AIL_` == 0) proof (all 4). [33-05-T1]
4. Correct the surface-copy census (4 not 8) + per-site format/dim recording, esp. `:681` (all 4). [33-03-T2]
5. Fix tinyxml output-name → `tinyxmld_STL.lib` (+ Plan 05 link swap, OutDir depth, sln registration) (Codex/Cursor/Sonnet). [33-01-T3, 33-05]
6. Expand matrix A/B to gl05 **and** gl06 **and** gl07 (FFP/VSPS `#ifdef` geometry) (Cursor). [03-T4, 06-T2]
7. Add x64-export confirmation for `D3DAssemble` + export-name spot-check for pcre/libxml2/jpeg (Sonnet). [33-02, 33-01-T2]
8. Default Fix-A to retain-with-comment (Opus). [33-02-T2]
9. Add plugin `ProjectConfiguration` in Plan 04 before the sln `.Build.0`; tighten the 04 dependency/guard (Sonnet/Cursor). [33-04]
10. Make A1-DBGHELP-WALK a required test + widen RIP; set `rasterMajor=5` in 06-T1; fix the ICU grep; reuse `WriteTGA`; note A2-TIME-T deferred. [33-06, 33-01, 33-03]

To incorporate into planning: `/gsd:plan-phase 33 --reviews`
