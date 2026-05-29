codex.cmd : OpenAI Codex v0.133.0
At line:3 char:11
+ $prompt | & "C:\Users\kenne\AppData\Roaming\npm\codex.cmd" exec --ski ...
+           ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    + CategoryInfo          : NotSpecified: (OpenAI Codex v0.133.0:String) [], RemoteException
    + FullyQualifiedErrorId : NativeCommandError
 
--------
workdir: D:\Code\swg-client-v2
model: gpt-5.5
provider: openai
approval: never
sandbox: read-only
reasoning effort: medium
reasoning summaries: none
session id: 019e715d-2b3e-7563-bd5e-225bab1103cc
--------
user
# CONSULT ??? D3D11 modernization port: systemic VS???PS interpolator-schema mismatch

**Project context.** Star Wars Galaxies (SWG) is a 2003-era MMO client. We are running a 
long-running modernization port (`swg-client-v2`, MSBuild, MSVC v145 toolset) whose v2.2 milestone 
is "Visual Parity" ??? D3D11 (`rasterMajor=11`) must visually match the known-good D3D9 
(`rasterMajor=5`) baseline at char-select and beyond. The D3D11 plugin (`gl11_d.dll`) is a Phase 11 
work product; the original engine uses a plugin architecture where the renderer plugin (`gl05_d.dll` 
D3D9, `gl11_d.dll` D3D11) is `LoadLibrary`'d at runtime by `SwgClient_d.exe`. Asset shaders are 
stored in IFF files with two chunks per pixel program: `TAG_PSRC` (HLSL source) and `TAG_PEXE` 
(precompiled D3D9 asm). Until Phase 17, the D3D11 plugin discarded `TAG_PSRC` and could not consume 
the asm `TAG_PEXE` blobs (D3D11 has no asm path), so it fell back to a built-in magenta pixel shader 
for every asset PS. Result: char-select rendered with "washed-out white / magenta" character.

**Phase 17 plan structure.** Three plans:
- **Plan 17-01** ??? retain `TAG_PSRC` source text on the shared 
`ShaderImplementationPassPixelShaderProgram` struct + flag-gated PSRC census at the load site. 
**DONE.** Confirmed census: 22 unique HLSL + 10 unique asm pixel programs at char-select (68.75% 
HLSL). All character-rendering shader families (`h_*` body, `a_*` armor, `e_*` effects, 
`ui_membrane_color2`) are 100% HLSL ps_2_0. The 10 asm programs are basic primitives only.
- **Plan 17-02** ??? wire the primary recompile lane in the D3D11 plugin's 
`Direct3d11_PixelShaderProgramData` ctor: classify retained PSRC via first-non-whitespace `//hlsl` 
check, feed `//hlsl` source into a new `tryCompilePixelShaderFromHlslNoFatal` sibling (HIGH-1: must 
survive PSRC compile failures gracefully), bind the resulting real `m_d3dPS`, retain the compiled 
DXBC blob, run `D3DReflect` ONCE to cache cbuffer layout + PS input signature, dump both via 
`ID3D11InfoQueue::AddApplicationMessage` + `DEBUG_REPORT_LOG_PRINT` for verification. **CODE-DONE.** 
Builds clean (0 unresolved external symbols). Boot confirms: 116 PS input signatures dumped, 26 
cbuffer layouts reflected (all PS programs share one cbuffer `SwgVertexConstants` bindPoint=0 
totalSize=400 vars=9), 0 compile failures, 0 fallback uses.
- **Plan 17-03** ??? wire `StaticShader`/`ConstantBuffer` to feed Plan 17-02's reflected cbuffer 
layout (material color, texture-factor constants). PENDING.

**The finding.** With Plan 17-02 live, char-select renders **flat-black character silhouette** 
(clean geometry, perfect z, background visible behind, no see-through, all surfaces shading to 
zero). `stage/d3d11-debug.log` shows the D3D11 debug runtime firing `id=343` (linkage error: 
"Signatures between stages are incompatible. Semantic 'X' is defined for mismatched hardware 
registers between the output stage and input stage") **565,056 times per session**, split:

- `COLOR` semantic: 188,352 mismatches
- `TEXCOORD` semantic: 376,704 mismatches
- (`id=342` "missing semantic" fires 0 times ??? semantic NAMES match, only register POSITIONS 
mismatch)

**Root-cause diagnosis (please validate or refute).** The recompiled HLSL PS programs declare a rich 
input struct. Sample (character body shader `h_color2_specmap_bump_aniso_ps20.psh`, captured via 
D3DReflect):

```
PS-input-sig: COLOR0    register=0 mask=0x7 componentType=3
PS-input-sig: COLOR1    register=1 mask=0x7 componentType=3
PS-input-sig: TEXCOORD0 register=2 mask=0x3 componentType=3
PS-input-sig: TEXCOORD1 register=2 mask=0xC componentType=3  (note: shares reg 2 with TEXCOORD0 via 
complementary mask ??? HLSL packing)
PS-input-sig: TEXCOORD2 register=3 mask=0x7 componentType=3
PS-input-sig: TEXCOORD3 register=4 mask=0x7 componentType=3
```

Simpler PS sample (`h_simple_pp_ps20.psh`):
```
COLOR0    register=0 mask=0x7
TEXCOORD0 register=1 mask=0x3
TEXCOORD1 register=2 mask=0x7
```

But the VS programs being bound at draw time output essentially nothing. The ONLY unique 
`VS-outputs(...)` pattern observed across the entire 153 MB / 589,451-line log:

```
VS-outputs(1): [TEXCOORD0 r=o1 mask=0x3 rwmask=0xC]
```

That's it. One interpolator, `TEXCOORD0` at register `o1`, 2 components (mask 0x3), the remaining 2 
components (`rwmask=0xC`) unwritten. No `COLOR0`, no `COLOR1`, no `TEXCOORD1+`. So:

- PS expects `COLOR0` at register 0 ??? VS provides nothing at register 0 ??? PS reads `(0,0,0,0)` 
for COLOR0
- PS expects `TEXCOORD0` at register 1 ??? VS provides `TEXCOORD0` at o1 (register 1, mask 0x3) ??? 
semantic + register slot DO line up here, but `id=343` still fires for `TEXCOORD` ??? perhaps 
because additional `TEXCOORDN` (1+) are missing, or because the mask differs
- PS expects `TEXCOORD1+` at registers 2-4 ??? VS provides nothing ??? garbage

In the PS body, the texture sample modulates against `COLOR0` (vertex color ?? material ?? diffuse 
texture). `COLOR0` reads zero ??? result zero ??? flat black.

**Historical context.** The minimal VS output makes sense for the pre-Plan-17-02 world where the 
bound PS was the magenta fallback ??? that PS ignored its inputs entirely and just output magenta. 
So Phase 11 work optimized the VS to write only what it strictly needed for position transform + the 
one UV the magenta fallback PS still consumed (apparently for alpha/clip). With Plan 17-02's "real" 
HLSL PS programs now bound, the VS schema is inadequate.

**The architectural question.** What is the canonical D3D11 modernization pattern to align VS output 
schema with HLSL-recompiled PS input schema, given:

- We cannot modify the original .sht/.psh asset files in-tree (they ship in TRE archives).
- We CAN modify how the D3D11 plugin recompiles them at load time (we already do this for PS via 
Plan 17-02 ??? the sibling `Direct3d11_VertexShaderData.cpp` could do the analogous job for VS).
- Both shaders are HLSL ps_2_0 / vs_2_0 era (no SM5 shader linkage objects available).
- We want a single canonical pattern for ~50-100+ shader families across the asset library, not 
per-shader manual fix-ups.

**Candidate approaches to compare (rank, recommend, or propose better):**

1. **VS recompile lane mirroring Plan 17-02's PS lane.** Add a sibling 
`tryCompileVertexShaderFromHlslNoFatal` to `Direct3d11_VertexShaderData.cpp`. Recompile VS programs 
from their `TAG_VSRC` (or equivalent HLSL source ??? verify the IFF tag name; likely exists since PS 
had it). The HLSL compiler will then assign register positions consistently with the PS recompile 
(FXC pass on related struct types tends to be deterministic by declaration order in the same compile 
invocation). Risk: VS and PS are compiled in SEPARATE `D3DCompile()` calls in different .cpp files 
??? separate compile contexts may still produce divergent register allocations even from 
"compatible" source. Pro: structurally symmetric to PS work; aligns with the v2.2 "recompile, don't 
decompile" reframe.

2. **D3DCompile2 with `ID3D11Linker` / `ID3D11FunctionLinkingGraph` to compile VS + PS as a linked 
pair.** Compile each shader to a library blob first, then link VS + PS pair so the linker guarantees 
register-slot compatibility. Pro: D3D11 sanctioned way to do this. Con: significant API complexity 
for an SM2-era codebase; not sure if linker supports SM2 targets (linker is SM5+ in most 
documentation I've read).

3. **Post-compile reflection-based register remapping.** Compile VS and PS independently (as we do 
today). Use `D3DReflect` on BOTH; build a remapping table; insert a small SHADER FIXUP pass (binary 
patching of the VS DXBC output declarations, or VS post-pass that emits at the slots the PS 
expects). Con: binary-patching DXBC is fragile and unsupported; SUPER easy to break. Pro: doesn't 
require recompiling VS.

4. **Explicit register binding hints in PSRC source on recompile.** Before passing PSRC text to 
`D3DCompile`, scan for declarations like `float4 col : COLOR0;` and replace with `float4 col : 
COLOR0 : packoffset(c0.x);` or similar register hints (or insert SV-style semantics that pin slots). 
Pro: minimal code change. Con: doesn't match how HLSL semantics work in practice ??? semantics 
name+index pair selects the slot, not packoffset (which is for cbuffer members, not interpolators); 
explicit interpolator register hints in SM2 HLSL are uncommon / non-standard. Unlikely to work.

5. **Unify VS output struct AND PS input struct in a shared HLSL header.** Modify both the PS 
recompile (Plan 17-02) AND a new VS recompile to `#include` a shared `Interpolators.hlsli` that 
defines a canonical struct. Both compiles see the identical struct type ??? identical register 
allocation. Pro: maintainable, clearly correct. Con: requires interfering with both compile calls + 
supplying the include in-memory.

**Specific asks:**

1. **Validate or refute** the root-cause diagnosis (interpolator schema mismatch as described).
2. **Rank approaches 1-5** for an SM2 ps_2_0 / vs_2_0 asset base, expressing the tradeoff axes 
(correctness, structural fit with the existing recompile-lane architecture, fragility, complexity, 
maintainability for 50-100+ shader families).
3. **Recommend a primary approach + a fallback** so we can structure Plan 17-04 (or a Phase 18 plan) 
accordingly.
4. **Surface any subtle gotchas** with how D3D11 validates VS???PS linkage that we might miss (e.g., 
interpolation mode interactions, system-value handling, the `centroid`/`linear`/`nointerpolation` 
qualifiers, mask-bit handling when VS writes only `mask=0x3` to a register where PS reads 
`mask=0xF`, FXC's behavior on shared register packing per HLSL component-mask rules).
5. **Flag if the diagnosis suggests Plan 17-03 (cbuffer wiring) needs design changes** ??? currently 
the cbuffer is `SwgVertexConstants bindPoint=0 totalSize=400 vars=9` (single cbuffer at b0, all 26 
PS programs share it). Plan 17-03 will populate it from the StaticShader's material parameter map. 
Anything we should know about how `id=343` resolution might affect the cbuffer story?

**Out of scope for this consult.** The separate `gl05_d.dll` rebuild texture-binding regression (#5) 
is a build-config drift issue ??? a focused build-log diff is the right tool, not a consult.

**Operational notes for the consultee.** This is an existing Anthropic Claude Code session running 
`/gsd-execute-phase 17`. We've completed Wave 1 (Plan 17-01) and Wave 2 (Plan 17-02). Wave 3 (Plan 
17-03 cbuffer wiring) will proceed in parallel with this consult; your input shapes the post-Wave-3 
work, not Wave 3 itself. Project memory at `~/.claude/projects/D--Code-swg-client-v2/memory/` 
includes `project_v2.2_slot0_psrc_reframe.md` (the v2.2 strategic reframe) and 
`project_shared_header_abi_cascade_trap.md` (the related plugin-rebuild gotcha that surfaced in Plan 
17-01) ??? neither is required reading for this consult but they provide additional context if 
useful.


codex
I’ll ground this in the repo handoff first, then answer as an architecture consult rather than 
changing code. I’ll also check the D3D11 linkage details against Microsoft docs where they matter.
exec
"C:\\WINDOWS\\System32\\WindowsPowerShell\\v1.0\\powershell.exe" -Command 'Get-Content -Raw 
.planning/handoff/README.md' in D:\Code\swg-client-v2
 succeeded in 1239ms:
# Session handoffs

## Active handoffs

| File | Topic | Updated |
| --- | --- | --- |
| [swg-blender-import.md](swg-blender-import.md) | SWG Blender IFF pipeline (Phases 0â€“15 done) | 
2026-05-26 |

exec
"C:\\WINDOWS\\System32\\WindowsPowerShell\\v1.0\\powershell.exe" -Command 'Get-Content -Raw 
.planning/STATE.md' in D:\Code\swg-client-v2
exec
"C:\\WINDOWS\\System32\\WindowsPowerShell\\v1.0\\powershell.exe" -Command 'Get-Content -Raw 
.planning/PROJECT.md' in D:\Code\swg-client-v2
 succeeded in 767ms:
# whitengold â€” SWG Client Modernisation Port

## Current State: v2.1 Decruft SHIPPED (2026-05-27)

v2.1 re-applied the orphaned v2.0 dead-code removals (CLEAN-01..04) against the active 
Koogie/MSBuild tree: five dormant subsystems â€” trackIR/stationapi/SwgClientSetup/lcdui (Phase 12), 
VideoCapture (13), Vivox voice chat (14), and the XPCOM/Mozilla in-game browser (15) â€” were fully 
unlinked and deleted, plus a final tech-debt cleanup pass (Phase 16). The client stays bootable to 
character-select under **both** D3D9 (`rasterMajor=5`) and D3D11 (`=11`) after every removal. Audit: 
`tech_debt` (7/7 DECRUFT satisfied, 0 blockers, 28/28 integration). See `MILESTONES.md` + 
`milestones/v2.1-*`.

**Prior:** v2.0 (shipped 2026-05-25) delivered the modern MSVC/C++20/MSBuild client with selectable 
D3D9 + D3D11 renderers. **Key pivot (Phase 9, "Option D"):** the original CMake/whitengold build was 
*replaced* by adopting Koogie's already-MSVC/C++20-migrated **MSBuild** tree wholesale (merge 
`479d35df3`). The active build is `src/build/win32/swg.sln`; the CMake build that v1 + early-v2 docs 
describe is superseded (kept for history).

## Shipped Milestone: v2.1 â€” Decruft (closed 2026-05-27)

**Goal (MET):** Re-applied the orphaned CLEAN-01..04 dead-code removals against the active 
Koogie/MSBuild tree, shrinking the client's surface area before any SWG-Source upstream import work. 
All 7 DECRUFT requirements satisfied across Phases 12â€“16; per-phase detail archived in 
`milestones/v2.1-ROADMAP.md`.

**Target removals (each its own boot-gated step):**
- Vivox voice chat â€” `vivoxSharedWrapper_debug.lib` (SwgClient `libraries_d.rsp`) + 
`CuiVoiceChatManager` source
- VideoCapture â€” `VideoCapture_debug.lib`
- XPCOM/Mozilla in-game browser â€” `libMozilla.vcxproj` (swg.sln) + `libMozilla/include/public` 
(`includePaths.rsp`)
- lcdui (G15 LCD) â€” `lcdui.vcxproj`
- SwgClientSetup â€” `SwgClientSetup.vcxproj`
- trackIR/ + stationapi/ orphaned directories

**Key context:** These removals were done once on the original whitengold **CMake** tree (Phase 7, 
CLEAN-01..05) but the Phase 9 "Option D" base swap to Koogie's MSBuild tree orphaned them â€” the 
dead code is dormant-but-present in the active build. Reference diff template: the whitengold 
(swg-client) Phase 07 removal commits. The Mozillaâ†”`/Zc:wchar_t` interlock is already resolved 
(Phase 9 went `char16_t`), so XPCOM removal is a clean unlink. **Invariant:** every step leaves the 
client bootable to character select under both `rasterMajor=5` (D3D9) and `=11` (D3D11).

**Progress:** âœ“ Phase 12 complete (2026-05-25) â€” DECRUFT-01/-02/-03 done: trackIR + stationapi 
orphan dirs deleted, SwgClientSetup dropped from swg.sln, and lcdui (G15 LCD) fully removed (live UI 
de-wired to no-op stubs + build graph + dirs). Dual-renderer boot baseline re-established (both 
renderers to character select). A recurring finding drove 4 deviations: dead modules were still 
*linked* via inline `.vcxproj` deps (not the vestigial `.rsp` files), and the `/FORCE` linker masks 
unresolved-symbol false-passes â€” so removal-step build gates must check for 0 unresolved 
externals, not just MSBuild exit 0.
âœ“ Phase 13 complete (2026-05-26) â€” DECRUFT-04 done: VideoCapture/AudioCapture middleware fully 
unlinked (live `CuiIoWin` caller + `SwgVideoCapture` wrapper + 15 lib tokens/8 paths removed 
atomically; all dead `#if 0` source + `.rsp` + 10 editor `.vcxproj` refs purged; vendored 
`src/external/3rd/library/videocapture/` tree deleted). Repo-wide grep incl `.vcxproj` == 0; 
Debug+Release link gate clean (2138/0/0, 2104/0/0); dual-renderer boot (D3D9+D3D11) human-confirmed 
to Tatooine. Bink/Vivox/Miles preserved. Next: Phase 14 (Vivox voice-chat source removal â€” 
higher-risk live-UI surgery).
âœ“ Phase 14 complete (2026-05-26) â€” DECRUFT-05 done: Vivox voice-chat subsystem fully removed 
from source and build. ~24 voice source files + 3 voicechat network messages deleted, 10 live 
callers + 5 in-repo registrations de-wired, 6 CuiPreferences voice keys stripped, 
vivox/VChat/libsndfile unlinked from SwgClient (3 configs) + all vestigial `.rsp`/editor `.vcxproj` 
refs purged, and the 3 vendored voice trees (vivox/, vivoxSharedWrapper/, soePlatform/VChatAPI/) 
deleted. Repo-wide `rg -i vivox src` == 0; Debug+Release link gate clean (0 unresolved); 
dual-renderer boot (D3D9+D3D11) human-confirmed to character-select. Code review caught + fixed a 
real regression (CR-01: voice-enum deletion shifted radial-menu ordinals off their retail-datatable 
rows â€” fixed with ordinal-preserving placeholders). Deferred (non-Decruft): DEF-14-01 Optimized 
SAFESEH config defect. Next: Phase 15 (XPCOM/Mozilla removal + milestone gate â€” highest-risk 
surgery).
âœ“ Phase 15 complete (2026-05-27) â€” DECRUFT-06/-07 done: XPCOM/Mozilla in-game browser fully 
removed + v2.1 milestone gate PASS. The 3 `SwgCuiWebBrowser*` units (the only `libMozilla::` 
consumers) deleted, all live callers + 5 `IsA(TUIWebBrowser)` focus sites de-wired, `TUIWebBrowser` 
deleted outright from `UITypeID.h` (ordinal never serialized â€” no placeholder, unlike CR-01), TCG 
browser tie severed (libEverQuestTCG kept), Mozilla-family link tokens stripped from 
`SwgClient.vcxproj` + `.rsp` + 7 editors + SwgGodClient, `libMozilla.vcxproj` dropped from `swg.sln` 
(11 GUID locations), and the vendored `libMozilla/` tree (1,866 files: XPCOM/Gecko SDK) deleted. 
Milestone-wide P12â€“P15 residue sweep == 0 / KEEP-listed (incl. lcdui A1 cleanup); Debug+Release 
link gate clean (0 unresolved; Optimized EXEMPT per DEF-14-01); dual-renderer boot (rasterMajor=5 
D3D9 + =11 D3D11) human-confirmed to character-select with correct HUD/radial/IME focus. Verified 
9/9 must-haves; code review 0 blockers (3 advisory warnings, all the deliberate T-15-02/D-04a 
TCG-tie severance â€” TCG revival out of scope). **v2.1 Decruft COMPLETE (Phases 12â€“15; 
DECRUFT-01..07 all done).** Next: v2.2 Visual Parity.
âœ“ Phase 16 complete (2026-05-27) â€” v2.1 tech-debt cleanup (post-milestone-audit closure, no new 
product reqs): unlinked the dead SwgGodClient `989crypt.lib` latent-LNK1181 token (soePlatform 
KEEP-list + the separate live `crypto.lib` preserved), confirmed the 4 editor vcxproj lcdui-clean 
(verify-only), and removed the cosmetic P12â€“P15 source residue â€” the dead `finalUrl` browser-CS 
URL block (+ now-dead `shellapi.h`/`ConfigClientGame.h` includes) in `SwgCuiHudAction.cpp` and the 
orphaned voice-volume API (2 statics + 2 REGISTER_OPTION persistence lines + 4 accessors + 4 decls) 
in `CuiPreferences.cpp/.h`. All grep-zero gates green; SwgClient Debug+Release relink clean (0 
unresolved externals, Optimized EXEMPT per DEF-14-01); single rasterMajor=11 (D3D11) boot to 
character-select human-confirmed (no-behavior-change phase â†’ single boot replaces the 
dual-renderer matrix). Verified 8/8 must-haves (D-01..D-08); code review 0 blocker/0 warning/1 INFO 
(the `httpParams` builder is now write-only â€” 16-02's intentional narrow scope, follow-up-pass 
candidate). Discovered + ruled out a pre-existing Options-window crash 
(`checkShowToolbarCommandCooldownTimer` `.ui`-data mismatch from feature commit `d1b3c0eaf` â€” NOT 
a Decruft regression; captured as a pending todo). **v2.1 Decruft milestone-audit tech-debt fully 
closed.**

## Current Milestone: v2.2 â€” Visual Parity (ACTIVE, started 2026-05-27)

**Goal:** Close the D3D11 visual gaps so it matches the known-good D3D9 baseline â€” **starting with 
a deterministic character-select beachhead** (get textures + eyes correct there first), then 
extending the working PS pipeline outward to the open world. The #1 blocker is **the asset 
pixel-shader pipeline**: the engine ships pre-compiled D3D9 PEXE PS bytecode that 
`CreatePixelShader` rejects, so D3D11 falls back to a magenta PS and renders untextured surfaces. 
Requirements derive from `docs/research/phase12-baseline/COMPARISON.md` (5 gap buckets). Phases 
continue from 16 â†’ **start at Phase 17**.

**Target features (all confirmed in scope):**
- **Asset PS pipeline** â€” re-author/bridge engine pixel shaders so D3D11 binds real asset shaders. 
Proven **first on character-select (textures + eyes)**, then world surfaces.
- **Gamma LUT + lighting** â€” fix blown-out flat-white interiors.
- **Minimap shape** â€” square â†’ round (PS-gen / 2D-texturing family).
- **Multi-stage sampling** â€” multi-texture material stages.
- **Load-screen centerline seam** â€” half-texel fullscreen-blit fix (isolated, deterministic 
sampling-path canary).
- **Geometry distortion** â€” exterior static-mesh shards (OPEN investigation; the skeletal-path 
twin was already fixed at `905fb5d64`).
- **Particles / ribbon** â€” post-PS-pipeline.
- **DPVS D3D11 remeasure** â€” SPEC R7, deferred from v2.0 ("after the asset PS pipeline lands" = 
now).

**Key sequencing constraint:** the **character-select screen is the first vertical slice** â€” quick 
to reach, deterministic, isolated. Nail textures + eyes there before any open-world work. 
*(Milestone reordered after v2.1: cleanup-first shrank the surface area before visual work + 
upstream imports.)*

---

## What This Is

whitengold is the leaked Star Wars Galaxies source tree (NGE-era ~2010 build, leaked
Feb 2015). This project takes the **client** â€” `SwgClient` and its ~70-project
dependency graph â€” and modernises it off Visual Studio 2005 onto a current
**MSVC / C++20 / MSBuild + Visual Studio 2022** toolchain (via Koogie's
`MSVC-CPP20-Upgrade` base, adopted in Phase 9) so it can be compiled, debugged, and
launched against a community-run **SWG-Source** server VM. The long-arc
goal is full feature parity with the live NGE client (ground/space, professions,
housing, GCW, jukebox). v1 (compile + launch) and v2.0 (modernisation: dead-code,
STL swap, DPVS verdict, D3D11 renderer) are complete; v2.1 (Decruft) re-applies the
orphaned dead-code removals to the active MSBuild tree; v2.2 targets D3D11 visual parity.

> **Build-system note:** v1 + early-v2 sections below describe the original **CMake**
> port (the "whitengold" tree). Phase 9 "Option D" replaced it with Koogie's
> **MSBuild** solution (`src/build/win32/swg.sln`) â€” the active build. CMake-specific
> requirements/decisions are historical; the engine-level architecture they describe
> still holds.

## Core Value

**Every change must leave the client bootable to character select.** Dead code removal, STL swap, 
renderer work â€” all of it is only valid if the client still runs at the end. Build health is the 
invariant; modernisation is layered on top.

## Requirements

### Validated

<!-- Recon and prior work that's already locked. -->

- âœ“ **Codebase mapped** â€” `.planning/codebase/` (7 docs, 2265 lines, commit `4349655bb`)
- âœ“ **Source structure recon'd** â€” engine/game/dsrc/external layout documented in
  `STRUCTURE.md` and `CLAUDE.md`
- âœ“ **Boot sequence (17 phases) traced** â€” `docs/recon/05-client-boot-sequence.md`
- âœ“ **Build chain analysed** â€” `docs/research/swgclient-build.md` (project graph,
  flag inventory, third-party dependency map)
- âœ“ **Middleware inventory** â€” animation, foliage, audio, video, physics middleware
  identified and characterised (`docs/research/runtime-middleware.md`,
  `animation-system.md`, `foliage-system.md`)
- âœ“ **SWG-Source diff** â€” confirmed server-only fork, no client patches to adopt;
  build patterns are transferable (`docs/recon/07-swg-source-diff-findings.md`)

**v2.1 Decruft (shipped 2026-05-27) â€” dead-code removals re-applied to the MSBuild tree:**

- âœ“ **DECRUFT-01** â€” trackIR + stationapi orphan dirs deleted; ClientHeadTracking de-wired to 
no-op stubs â€” v2.1 (Phase 12)
- âœ“ **DECRUFT-02** â€” SwgClientSetup dropped from `swg.sln` + dir deleted â€” v2.1 (Phase 12)
- âœ“ **DECRUFT-03** â€” lcdui (G15 LCD) fully removed (live UI stubbed + 7 sln deps + `.rsp` purge) 
â€” v2.1 (Phase 12)
- âœ“ **DECRUFT-04** â€” VideoCapture unlinked + vendored tree deleted; Debug+Release link clean â€” 
v2.1 (Phase 13)
- âœ“ **DECRUFT-05** â€” Vivox voice chat fully removed (source + build + 3 vendored trees); CR-01 
radial-ordinal regression fixed â€” v2.1 (Phase 14)
- âœ“ **DECRUFT-06** â€” XPCOM/Mozilla in-game browser fully removed (`libMozilla.vcxproj` + 
1,866-file tree) â€” v2.1 (Phase 15)
- âœ“ **DECRUFT-07** â€” full-removal-set dual-renderer boot to character-select human-confirmed 
(D3D9 + D3D11) â€” v2.1 (Phase 15, re-confirmed Phase 16)

### Active

<!-- v1 milestone scope: compile + launch -->

- âœ“ **CMake build system** authored parallel to existing `.vcproj`s, mirroring
  swg-main's CMakeLists structure (top-level `CMakeLists.txt` + `cmake/win32/`
  toolchain modules) â€” Validated in Phase 1 (foundations) + Phase 2 (23 shared libs)
- âœ“ **Third-party SDK integration** â€” `Find*.cmake` modules for DX9, Bink, Miles,
  Vivox, libMozilla XPCOM, DPVS, lcdui, STLPort 4.5.3, libxml2, pcre, zlib, Boost
  â€” Validated in Phase 1; STLPort compat shims for VS 2022 / Win SDK 26100 resolved
- âœ“ **Compile flags ported** â€” `/Zc:wchar_t-`, `_USE_32BIT_TIME_T=1`, `_MBCS`,
  `DEBUG_LEVEL`, `PRODUCTION`, `/wd4244 /wd4996 /wd4018 /wd4351`, `/MP`, `/Ob1`,
  `/MT[d]` verified across 24 static `.lib` targets â€” Validated in Phase 2
- âœ“ **All 13 client engine libraries build** â€” `engine/client/library/` complete:
  clientAnimation, clientAudio, clientBugReporting, clientDirectInput, clientGame,
  clientGraphics (P1.11 resolved â€” d3d9.h from vendored DX9 path), clientObject,
  clientParticle, clientSkeletalAnimation, clientTerrain, clientTextureRenderer,
  clientUserInterface (XPCOM stub gate passed â€” zero xpcom/xul symbols); plus ui.lib
  â€” Validated in Phase 3; libMozilla XPCOM stub locks D-04/D-05/D-07
- âœ“ **Game client libraries + SwgClient executable link** â€” `libEverQuestTCG`,
  `swgSharedNetworkMessages`, `swgClientUserInterface` (266 cpp), and `SwgClient`
  WIN32 executable all link cleanly; `SwgClient_d.exe` (34 MB) + `SwgClient_r.exe`
  (16.5 MB) produced; 30 runtime DLLs + `mozilla/` staged via POST_BUILD;
  dumpbin confirms static CRT + zero XPCOM imports â€” Validated in Phase 4
- âœ“ **Debug build first** â€” `cmake --build build --config Debug` produces
  `SwgClient_d.exe`; binary launches (cursor spins, no missing DLL popup);
  Phase 1 pass/fail gate cleared â€” Validated in Phase 4 (2026-05-05)
- âœ“ **Release build** â€” same toolchain produces `SwgClient_r.exe` (16.6 MB); clean-clone build 
verified â€” Validated in Phase 5 (2026-05-05)
- [ ] **Asset path wired** â€” client reads `.tre` archives from a retail SWG install
  via `searchPath*` configs in `client.cfg`
- [ ] **Login screen reached** â€” client boots far enough to render the login UI
  and connect to the local SWG-Source LoginServer
- [ ] **swg-main login flow handshake** â€” authenticate against the SWG-Source VM's
  LoginServer (whatever station-key stub it ships with) and reach character select

### Out of Scope

<!-- Deferred to future milestones or rejected outright. -->

- **Modifying C++ source files** â€” Phase 1/2 are build-config and configuration
  work only; source edits come later (avoid blast radius, keep diff focused on
  CMake authoring)
- **Replacing STLPort 4.5.3 with MSVC STL** â€” swg-main openly warns this is
  non-trivial; deferred to a future modernisation milestone
- **Replacing DirectX 9 / migrating to D3D11+** â€” multi-year effort, deferred
- **TRE asset reconstruction** â€” user has a retail SWG install; community-rebuilt
  client-assets is not needed for v1
- **Server-side modifications** â€” swg-main is treated as a **black-box network
  endpoint**; if server changes are required they happen upstream in the
  swg-main repo, not here
- **Linux server build** â€” `src/build/linux/Makefile` and the Oracle/JVM/IBM-Java
  toolchain are server-side and out of scope
- **Editor / tool builds** â€” ParticleEditor, ConversationEditor, SpaceQuestEditor,
  GodClient, etc. are deferred until SwgClient itself ships
- **Beyond zone-in** â€” combat, AI, quests, professions, housing, GCW, space,
  jukebox are all future-milestone scope
- **Patcher / live-update infrastructure** â€” `publish/*.mft` describe a retired
  patcher tree; we ship via direct CMake build, no patcher
- **SOE Station authentication** â€” the real auth service is gone; the swg-main
  VM stubs it and we accept whatever it provides
- **Customer service / metrics / chat / commodities servers** â€” these are
  separate processes in the SWG cluster; only LoginServer + Connection +
  PlanetServer + GameServer are needed to reach character select

## Context

**The repo is a frozen archive, not a runnable system.** Per `CLAUDE.md`'s
"What's NOT here" inventory, several gaps prevent end-to-end execution from
this tree alone: no `.tre` asset archives, no Oracle DB content, no working
Linux build environment (gcc 2.95/3.2, IBM Java 1.3, Oracle 9.2), no SOE
Station auth, proprietary SDKs as headers/stubs, internal SOE infra
references. The user's plan accepts these gaps and works around them â€”
retail SWG install supplies assets, swg-main VM supplies the server tier,
proprietary SDKs are reconstructed from headers + vendored libs where licensed
copies are available.

**Reference repo:** `C:\Code\swg-main` (sibling checkout, populated submodules).
Server-only fork descended from the same SOE source. Authoritative for build
patterns, **not** for client port work (no client patches exist there).

**Target environment:** Windows 10/11 with Visual Studio 2022 (MSVC 14.4x)
and CMake 3.27+. C++17 standard. STLPort 4.5.3 retained for v1.

**Server runtime target:** SWG-Source StellaBellum-style VM, stood up locally
by the user. Out of scope for this repo's commits.

**Asset source:** The user owns a legitimate retail SWG install. Client will
be configured to read its data tree at runtime via `searchPath*` entries.

## Constraints

- **Tech stack (current, post-v2):** Windows-only, VS 2022, **MSBuild** (`src/build/win32/swg.sln`),
  **C++20**, **MSVC STL** â€” Linux work out of scope (server-side toolchain anyway).
  *(v1 used CMake + C++17 + STLPort; superseded by Phase 9 Option-D.)*
- **Source edits expected (v2+):** unlike v1's zero-source-edit rule, v2 modernisation
  edits engine source freely (STL swap, D3D11 plugin). Keep diffs focused per phase.
- **Don't modify Koogie's diagnostic patches** without strong reason â€” fix the caller/data instead.
- **Static CRT:** `/MT` (Release) / `/MTd` (Debug) per swg-main pattern; matches
  the third-party static libs we link against
- **Server contract is read-only:** swg-main VM is the runtime target; if a
  client behaviour requires a server change, it's deferred or pushed upstream
- **Asset licensing:** Only the user's own retail SWG install is used for `.tre`
  archives; no redistribution
- **Timeline:** No deadline â€” exploratory hobby cadence; granularity matters more
  than speed

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| **Framing B** (CMake + VS 2022, no source edits) over Framing A (period-correct VS 2005) | Aligns 
with swg-main's build system, easier onboarding, "minimum change" still holds because we're *adding* 
CMakeLists not editing source | Phase 1 foundation graph configures and builds with added CMake 
files only |
| **Debug build first**, Release later | Debug build is the more useful artefact for Phase 2 launch 
debugging â€” step-through is required to diagnose boot failures | â€” Pending |
| **swg-main as build-pattern reference** (server-only, but their CMake structure / flags / C++17 
transfer) | They've already solved the modernisation problem for the server tree; client porting can 
follow the same shape | â€” Pending |
| **Retail SWG install for assets**, not community client-assets | User owns a legitimate copy; 
simpler licensing story; closer to original NGE behaviour | â€” Pending |
| **swg-main VM as black-box server**, no server edits in this repo | Keeps blast radius narrow; if 
server changes needed, push upstream to swg-main | â€” Pending |
| **STLPort 4.5.3 retained** for v1 | swg-main warns the swap is non-trivial; defeats "no source 
edits" principle | Phase 1 uses a build-local staged STLPort layout so VS 2022 can supply modern 
UCRT/MSVC native headers without editing vendored STLPort |
| **DirectX 9 vendored SDK first** | The repo ships the 2005-era DirectX 9 headers/import libs used 
by the client; system SDK drift is avoidable | Phase 1 `FindDirectX9.cmake` resolves vendored 
headers/libs; first real compile remains Phase 3 `clientGraphics` |
| **Mozilla XPCOM stub strategy** | Modern MSVC cannot safely compile the old XPCOM surface; the 
in-game browser is not required for v1 launch | Phase 1 `FindMozilla.cmake` resolves headers only; 
do not link `xul.lib`/`xpcom.lib` in client targets |
| **/MT static CRT** | Matches third-party static libs (Bink, Miles, DPVS) we link against | Phase 1 
root CMake sets `CMAKE_MSVC_RUNTIME_LIBRARY` to `/MT` and `/MTd` |
| **Phase 9 "Option D": adopt Koogie's MSVC/C++20 MSBuild tree wholesale** (`479d35df3`) instead of 
hand-migrating STLPort in the CMake tree | Koogie already solved the MSVC migration; collapsed ~30 
risky refactor commits into ~3 | âœ“ Good â€” STL swap done, Tatooine zone-in PASS. Side effect: 
CMake build + Phase 7 dead-code removals orphaned (active tree is MSBuild) |
| **D3D11 as a second selectable renderer** (`gl11_d.dll`), keep D3D9 (`gl05_d.dll`) buildable 
(invariant D-05) | De-risks the port; lets D3D9 stay the known-good reference for visual parity | 
âœ“ Good â€” both selectable via `rasterMajor`; single `Gl_api` loader multiplexes |
| **v2.1 cleanup-first, risk-gradient ordering** (pure deletes â†’ lib unlinks â†’ live-source 
surgery) before v2.2 visual work | Re-establish the dual-renderer boot baseline on the low-risk 
deletes before the riskier Vivox/XPCOM source surgery; shrink surface area before upstream imports | 
âœ“ Good â€” every removal step boot-verified; v2.1 closed clean (7/7 DECRUFT) |
| **`/FORCE` link-grep gate** â€” grep link output for `unresolved external symbol` (==0), not just 
MSBuild exit 0 | SwgClient links under `/FORCE`, which downgrades unresolved externals to warnings 
and still emits a binary with exit 0 â€” exit 0 is NOT proof of a clean link | âœ“ Good â€” caught 2 
real Phase 12 defects (live `989crypt.lib` dep, orphaned ClientHeadTracking callers) |
| **Ordinal-preserving placeholders** for deletions from positional enums/tables that mirror 
retail-TRE row indices | Mid-sequence enum deletes silently shift surviving ordinals off their 
datatable rows â€” a regression the link + boot gate cannot catch | âœ“ Good â€” CR-01 (radial-menu 
ordinal shift from voice-enum delete) caught in code review + fixed with `RESERVED_*` placeholders 
(Phase 14) |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? â†’ Move to Out of Scope with reason
2. Requirements validated? â†’ Move to Validated with phase reference
3. New requirements emerged? â†’ Add to Active
4. Decisions to log? â†’ Add to Key Decisions
5. "What This Is" still accurate? â†’ Update if drifted

**After each milestone** (via `/gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check â€” still the right priority?
3. Audit Out of Scope â€” reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-05-27 â€” **v2.2 Visual Parity STARTED** (`/gsd-new-milestone`). Scope 
confirmed: full COMPARISON.md gap set (asset PS pipeline, gamma/lighting, minimap, multi-stage 
sampling, load-screen seam, geometry distortion, particles/ribbon, DPVS D3D11 remeasure). 
Sequencing: character-select beachhead first (textures + eyes), then open world. Phases continue 
from 16 â†’ start at 17. Prior: v2.1 Decruft SHIPPED + tagged `v2.1` (Phases 12â€“16; 7/7 DECRUFT); 
roadmap/requirements archived to `milestones/v2.1-*`. Requirements being defined now via the 
milestone workflow (research â†’ REQUIREMENTS.md â†’ ROADMAP.md).*


 succeeded in 779ms:
---
gsd_state_version: 1.0
milestone: v2.2
milestone_name: Visual Parity
status: executing
last_updated: "2026-05-28T16:41:26.428Z"
last_activity: 2026-05-28 -- Phase 17 execution started
progress:
  total_phases: 7
  completed_phases: 0
  total_plans: 3
  completed_plans: 0
  percent: 0
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-05-27 after v2.1 close)

**Core value:** Every change must leave the client bootable to character select â€” and for v2.2, 
D3D11 (`rasterMajor=11`) must visually match the known-good D3D9 (`rasterMajor=5`) baseline. Never 
regress the D3D9 reference path.
**Current focus:** Phase 17 â€” psrc-census-char-select-beachhead

## Deferred Items (acknowledged at v2.0 close)

NOTE: "Remove dead code (CLEAN-01..04 vs MSBuild)" is now CLOSED â€” it was the v2.1 Decruft 
milestone (Phases 12â€“16, shipped 2026-05-27).

Acknowledged and deferred at v2.0 milestone close (2026-05-25):

| Category | Item | Status |
|----------|------|--------|
| debug | cantina-corner-snap | closed (pre-existing engine quirk; not a regression) |
| debug | safecast-null-cast | closed (resolved 2026-05-15) |
| todo | 2026-05-15-cantina-corner-snap-engine-improvement | low â€” workaround exists |
| todo | 2026-05-15-swgsource-vs-whitengold-tre-asset-diff | low â€” informational, post-Phase-11 |
| backlog | Dead-code re-removal (CLEAN-01..04) vs MSBuild tree | **CLOSED â€” shipped as v2.1 
Decruft (Phases 12â€“16)** |
| backlog | DPVS D3D11 remeasure (SPEC R7); CLEAN-06 ~30 tools | DPVS-01 now scheduled as v2.2 Phase 
23; CLEAN-06 tools carried to backlog |

## Deferred Items (acknowledged at v2.1 close)

5 open artifacts acknowledged and deferred at v2.1 Decruft milestone close (2026-05-27). All 
non-blocking â€” none are v2.1 regressions:

| Category | Item | Status |
|----------|------|--------|
| debug | cantina-corner-snap | closed (pre-existing SOE engine quirk; not a regression) |
| debug | safecast-null-cast | closed (resolved 2026-05-15, ContrailData D-18 guard) |
| todo | 2026-05-15-cantina-corner-snap-engine-improvement | low â€” annoying, workaround available |
| todo | 2026-05-15-swgsource-vs-whitengold-tre-asset-diff | low â€” informational, post-Phase-11 |
| todo | 2026-05-27-options-toolbar-cooldown-ui-data-mismatch-crash | medium â€” pre-existing 
Options-window FATAL (`checkShowToolbarCommandCooldownTimer` `.ui`-data mismatch from feature commit 
`d1b3c0eaf`); OUTSIDE the v2.1 diff, NOT a Decruft regression |

Plus v2.2-coupled deferrals (milestone-audit `tech_debt`): `stage/client_d.cfg` accumulated 
test-settings cleanup (after v2.2 visual parity); AR-15-01 future-TCG-revival re-evaluation (future 
v2.x). See `milestones/v2.1-MILESTONE-AUDIT.md`.

## Current Position

Phase: 17 (psrc-census-char-select-beachhead) â€” EXECUTING
Plan: 1 of 3
Status: Executing Phase 17
Last activity: 2026-05-28 -- Phase 17 execution started

## Accumulated Context

### Roadmap Evolution

- [2026-05-27] **v2.2 Visual Parity ROADMAP CREATED** â€” 7 phases (17â€“23), 13/13 requirements 
mapped (CHAR/WORLD/GAMMA/UI/FX/GEO/DPVS). Standard granularity. Phases continue from 16 â†’ start at 
17.
- Phase 16 added: v2.1 tech-debt cleanup (SwgGodClient 989crypt.lib + P12-P15 residue) â€” from 
milestone audit

### Decisions

**v2.2 Visual Parity (roadmap):**

- [2026-05-27] **Char-select beachhead FIRST (Phase 17).** Prove the asset-PS pipeline on the 
deterministic, isolated character-select screen (textures + eyes + head) before any open-world work. 
Use `sul_eye.sht` + `sul_*_head.sht` PLUS one single-stage control (body/clothing) to separate 
"pipeline works" from "multi-sampler works."
- [2026-05-27] **Phase 17's gating first task = PSRC language census on the REAL asset tree** 
(retail-TRE extraction; the repo checkout has no extracted `.psh`/`.sht`). Classify `//hlsl` vs asm 
â€” the HLSL:asm ratio decides the lane mix and gates the rest of the phase.
- [2026-05-27] **Asset-PS approach (per CODEX+Cursor consult, SUPERSEDES SUMMARY.md on the asm 
lane):** primary lane = recompile discarded `TAG_PSRC` `//hlsl` via existing 
`compilePixelShaderFromHlsl` (mirror the VS compile stack); secondary lane = port asm `PSRC` â†’ 
HLSL â†’ `ps_4_0` (re-assembling asm just reproduces the rejected D3D9 bytecode â€” named landmine); 
FFP `TextureOperation` generator is tertiary/narrow ONLY for genuine FFP-only passes. Per-pass 
`Pass::apply` constants upload **reflection-driven (D3DReflect)**, NOT via copied D3D9 register 
indices.
- [2026-05-27] **Independent/parallel gaps:** load-screen half-texel seam (Phase 18, UI-01) is 
small/early/standalone (`getOneToOneUVMapping` stub), independent of the PS pipeline. Gamma LUT 
(Phase 19) scheduled AFTER the PS pipeline so it's tuned on correctly-shaded content; NO sRGB-view 
gamma (double-correction trap â€” keep SRVs `_UNORM`).
- [2026-05-27] **Satellite gaps are SEPARATE from the PS pipeline, not assumed to fall out of it:** 
interior flat-white (Phase 19, WORLD-03) also needs the simplified `Direct3d11_LightManager` + 
gamma, not just PS; "eyes through back of head" was already fixed by Iter-44A depth wiring (verify 
by screenshot in Phase 17, don't re-fold into PS scope); stencil is its own state-parity item; 
exterior/skeletal shard distortion (Phase 22, GEO-01) gated on a fully-settled re-capture (0013/0014 
were mid-load LOD smear) with a single-stream-fix vs flip-to-multistream decision.
- [2026-05-27] **DPVS D3D11 remeasure (Phase 23, DPVS-01) is STRICTLY LAST** â€” meaningless until 
geometry renders cleanly.
- [2026-05-27] **Validation = D3D9-vs-D3D11 screenshot diff** against the 
`docs/research/phase12-baseline/COMPARISON.md` matched pairs. Success = "matches `rasterMajor=5`", 
NOT "no magenta". Every parity claim needs a matched pair (Iter-44B minimap over-claim lesson). Do 
NOT mark UI-02 minimap done without a diff.
- [2026-05-27] **Cross-cutting landmines for executors:** boot-gate BOTH `rasterMajor=5` and `=11` 
on any `ShaderImplementation.cpp` edit; keep D3D9 `load_0000` PSRC behavior byte-for-byte identical 
except storing text; mirror the intentional D3D9 `Compare[]` swap 
(`C_GreaterOrEqual`â†”`C_NotEqual`), don't "fix" it; every new cbuffer matrix needs 
`XMMatrixTranspose`; persistent baked RT stays `B8G8R8A8_UNORM`; do NOT re-enable per-pass blend 
factors early (Iter-44C amplification regression); feed generator/rewrite/profile changes into the 
`.cso` cache key/version.

**v2.1 Decruft (roadmap):**

- [2026-05-25] v2.1 framed as **cleanup-only**: re-apply the orphaned CLEAN-01..04 removals to the 
active MSBuild tree. Visual Parity reordered to v2.2 (cleanup-first shrinks surface area before 
upstream imports). Reference diff template: the original whitengold (swg-client) **Phase 07** 
removal commits, retargeted CMake â†’ MSBuild.
- [2026-05-25] Phase ordering is **risk-gradient, low-first**: pure deletes (Phase 12) â†’ lib 
unlink (Phase 13) â†’ live-source surgery (Phases 14 Vivox, 15 XPCOM). Re-establishes the boot 
baseline before the riskier source removals.
- [2026-05-25] DECRUFT-07 (dual-renderer boot gate) is a cross-cutting milestone gate owned by the 
final phase (15) but **verified incrementally** after every removal in Phases 12â€“15 â€” mirrors 
v2.0 CLEAN-05.

### Pending Todos

1 pending:

- [Sync community compat fixes from SWG-Source/client-tools 
master](todos/pending/2026-05-08-sync-swg-source-community-compat.md) â€” future milestone (not v2.2)

### Blockers/Concerns

- **[v2.2 â€” census gate]** Phase 17's first deliverable (PSRC census) MUST run against the real 
retail-TRE asset extraction â€” this repo checkout has no extracted `.psh`/`.sht`. Neither consult 
reviewer could verify asset contents from the tree. The HLSL:asm ratio is the single biggest open 
unknown and gates the lane mix.
- **[v2.2 â€” screenshot-diff discipline]** Every parity win requires a matched D3D9/D3D11 pair from 
the same scene/pose. The Iter-44B minimap over-claim propagated a false win through a full plan 
cycle. Do NOT mark UI-02 done without a diff.
- **[v2.2 â€” boot invariant]** Every shared-`clientGraphics` edit must boot-test BOTH 
`rasterMajor=5` AND `rasterMajor=11` before being claimed done. Debug exe reads `client_d.cfg` (not 
`client.cfg`) â€” set rasterMajor there for each smoke (memory 
feedback_debug_exe_reads_client_d_cfg).
- **[Phase 12 â€” /FORCE false-pass, still applies]** SwgClient links under `/FORCE`, which 
downgrades unresolved externals to WARNINGS and still emits a binary with exit 0. `MSBuild exit 0` 
is NOT proof of a clean link â€” grep the link output for `unresolved external symbol` (must be 0).

## Deferred Items

Items carried from v1 close:

| Category | Item | Status | Deferred At |
|----------|------|--------|-------------|
| Build warnings | Nested CMake minimum-version deprecation warnings | Deferred (historical CMake 
tree) | Phase 1 |
| Build warnings | `crypto` C4530 exception-unwind warnings | Deferred | Phase 1 |
| Runtime | 156k DEBUG_WARNINGs at exit (mostly per-frame nebula lightning) | Deferred | v1 
post-launch |
| Runtime | 8,152 memory leaks at exit (singletons/caches, expected) | Deferred | v1 post-launch |

## Session Continuity

Last session: 2026-05-27T20:59:43.979Z
Resume (2026-05-27): **v2.2 Visual Parity ROADMAP CREATED** (Phases 17â€“23; 13/13 requirements 
mapped 100%). v2.1 Decruft shipped + tagged `v2.1`. Repo: swg-client-v2 (MSBuild/Koogie) is the 
single source of truth.

**v2.2 Visual Parity â€” the plan (7 phases):**

1. **Phase 17** (CHAR-01/02/03) â€” PSRC census on the real asset tree (gating first task), then 
prove the recompile + reflection-driven-constant pipeline on char-select: textures, eyes, 
multi-stage head. Primary lane = `//hlsl` PSRC recompile; secondary = asmâ†’HLSL port; 
tertiary/narrow = FFP generator. UI hint.
2. **Phase 18** (UI-01) â€” load-screen half-texel centerline-seam fix (`getOneToOneUVMapping` 
stub). Independent of Phase 17; safe early/parallel canary. UI hint.
3. **Phase 19** (GAMMA-01, WORLD-03) â€” gamma LUT post-pass (D3D9 `pow()` ramp, NO sRGB views) + 
interior lighting via per-pass light constants + simplified `Direct3d11_LightManager` parity. After 
Phase 17.
4. **Phase 20** (WORLD-01, WORLD-02, UI-02) â€” extend the PS pipeline to open-world surfaces + 
multi-stage `TextureOperation` cascade + round minimap (screenshot-diff verified). After Phase 17.
5. **Phase 21** (FX-01, FX-02) â€” particles (blend/additive/alpha) + ribbon/swoosh (instrument the 
draw path first; stretch is NOT a transform bug). After Phases 17/20.
6. **Phase 22** (GEO-01) â€” exterior static-mesh shard distortion, gated on a fully-settled 
re-capture (0013/0014 were mid-load LOD smear) + single-stream-fix vs multistream-flip decision. 
After Phase 20.
7. **Phase 23** (DPVS-01) â€” DPVS D3D11 remeasure + keep/remove verdict (SPEC R7 deferral). 
STRICTLY LAST.

Validation throughout = D3D9-vs-D3D11 screenshot diff against 
`docs/research/phase12-baseline/COMPARISON.md` matched pairs. Success = matches `rasterMajor=5`, not 
"no magenta".

**Next action:** `/gsd-plan-phase 17` â€” plan the PSRC census + char-select beachhead. Census tool 
is the gating first deliverable; needs the real retail-TRE asset extraction.

Known unrelated long-tail (deferred): 0x087a armor_marauder async crash (cross-client, retry works); 
pre-existing Options-window cooldown-UI crash (commit `d1b3c0eaf`, NOT a regression). Koogie's 
uncommitted Direct3d9.cpp Utinni vtable probe in the working tree is a separate sidequest â€” leave 
untouched.


web search: 
web search: Microsoft Direct3D 11 shader signature linkage compatible input output semantics 
registers masks
web search: 
web search: Microsoft ID3D11Linker Link shader model 5 library profile documentation
codex
Yes, the diagnosis is almost certainly right: this is a VS-to-PS signature/linkage bug, not a 
cbuffer bug.

D3D10/11 stage linkage is register-signature based. Microsoft’s docs describe adjacent stages as 
sharing a register array, with the downstream shader reading from the same locations the upstream 
shader writes; compatibility depends on exact signature matches for the consumed parameters, 
including order/type/semantics/signature layout. See Microsoft’s HLSL signature docs and 
`D3D11_SIGNATURE_PARAMETER_DESC` remarks: 
https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-signatures and https://
learn.microsoft.com/en-us/windows/win32/api/d3d11shader/ns-d3d11shader-d3d11_signature_parameter_desc
.

Given your observed state:

```text
PS expects: COLOR0, COLOR1, TEXCOORD0..3 across r0..r4
VS emits:   TEXCOORD0 only, at o1, xy only
```

that cannot be a valid D3D11 pair. The flat-black silhouette is a very plausible symptom because the 
real PS now executes and multiplies against missing/invalid interpolants such as `COLOR0`. Treat the 
read values as undefined rather than guaranteed zero, but “everything goes black” is exactly the 
kind of failure I would expect.

**Ranking**

1. **Approach 1: VS recompile lane mirroring PS lane**
   Best primary path. It fits the architecture you just established: retain source, recompile, 
reflect, cache signature, degrade gracefully. Separate `D3DCompile()` calls are not a blocker if the 
original VS and PS HLSL structs are actually compatible. The risk is asset reality: old D3D9 HLSL 
may have VS/PS structs that were compatible under D3D9 semantic matching but compile into 
incompatible D3D11 signature order/registers.

2. **Approach 5: shared generated/interposed interpolator contract**
   Best fallback or second layer. I would not start by forcing every shader through a synthetic 
shared header, because it risks semantic surgery across 50-100 shader families. But if independent 
VS/PS recompiles still disagree, generate a per-pass canonical interpolator ABI from reflection and 
patch/wrap source at compile time. Do this narrowly, per shader family/pass, not as one global 
mega-struct.

3. **Approach 2: `ID3D11Linker` / FLG**
   Poor fit here. Shader linking is a Shader Model 5-era feature path and adds a lot of machinery. 
Microsoft positions shader linking around libraries/graphs and runtime linking, supported starting 
with Windows 8.1: https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/using-shader-linking. 
It is not the natural answer for SM2-era asset HLSL or `vs_4_0_level_9_x`/`ps_4_0_level_9_x` style 
ports.

4. **Approach 4: explicit register binding hints in PSRC**
   Mostly a dead end. `packoffset` is for constant-buffer layout, not interpolators. SM2-era HLSL 
semantic text does not give you a reliable D3D11 interpolator register pinning mechanism. You may 
influence order by struct declaration order, but that is source-shaping, not explicit binding.

5. **Approach 3: DXBC binary patch/remap**
   Avoid except as a last-resort research tool. It is fragile, unsupported, hard to validate across 
driver/compiler versions, and likely to create future artifacts that look like renderer bugs.

**Recommendation**

Plan 17-04 should be: add the VS source-retention/recompile lane, reflect VS output signatures, then 
validate each bound VS/PS pair at bind or draw time before issuing the draw.

Concrete shape:

```text
1. Retain VSRC on the shared VS program structure.
2. Recompile //hlsl VS source in Direct3d11_VertexShaderData.
3. Reflect VS output signature once.
4. At shader pair bind/apply time, compare VS outputs against PS inputs.
5. Log pair key, shader names, pass/material name, and exact mismatch.
6. If valid: draw.
7. If invalid: use a controlled fallback path, not silent broken rendering.
```

Fallback should be source-level ABI normalization, not DXBC patching. Start by preserving/using the 
original asset VS source. If signatures still mismatch, build a narrow “interpolator bridge” system 
that emits or rewrites the VS output struct to satisfy the PS input signature for that pass. The PS 
input signature should generally be treated as the consumer contract.

**Gotchas**

`COLOR0`/`COLOR1` are not magic D3D9 color registers in D3D11. They are just user semantics unless 
translated to system values. They still must match between stages.

Mask matters. A VS writing `xy` while the PS reads `xyzw` is not compatible for the wider read. If 
the PS only consumes `xy`, `mask=0x3` can be fine, but reflection comparison must use the PS read 
mask, not just semantic name.

Order/register assignment matters, not just semantic spelling. Your `TEXCOORD0`/`TEXCOORD1` sharing 
register 2 with complementary masks is legal packing, but only if the VS produces the same packed 
layout.

Interpolation modifiers can matter. `linear`, `centroid`, `nointerpolation`, sample qualifiers, and 
component types should be included in your validator. A mismatch may not always show as the same 
debug-layer ID.

Do not ignore `SV_Position`/position handling. It may be omitted from your “interpolator” dump, but 
the actual VS output still needs the correct position output for rasterization. Keep user varyings 
and system values distinct in the logs.

The lack of `id=342` does not refute missing data. The debug layer may be reporting register-layout 
incompatibility first or aggregating by semantic family. `id=343` at that volume is already decisive.

**Plan 17-03 Impact**

Plan 17-03 does not need a conceptual redesign, but it should not be judged visually until linkage 
is fixed. The reflected `SwgVertexConstants` cbuffer at `b0`, size 400, shared across PS programs, 
still sounds like the right lane for material/texture-factor constants. Once interpolators become 
valid, cbuffer mistakes will probably change the failure mode from “flat black silhouette” to wrong 
tint/spec/bump/material response. Keep the cbuffer reflection-driven and add the VS cbuffer 
reflection too, because the VS recompile lane may reveal its own `b0`/matrix expectations.
tokens used
32,506
Yes, the diagnosis is almost certainly right: this is a VS-to-PS signature/linkage bug, not a cbuffer bug.

D3D10/11 stage linkage is register-signature based. Microsoft’s docs describe adjacent stages as sharing a register array, with the downstream shader reading from the same locations the upstream shader writes; compatibility depends on exact signature matches for the consumed parameters, including order/type/semantics/signature layout. See Microsoft’s HLSL signature docs and `D3D11_SIGNATURE_PARAMETER_DESC` remarks: https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-signatures and https://learn.microsoft.com/en-us/windows/win32/api/d3d11shader/ns-d3d11shader-d3d11_signature_parameter_desc.

Given your observed state:

```text
PS expects: COLOR0, COLOR1, TEXCOORD0..3 across r0..r4
VS emits:   TEXCOORD0 only, at o1, xy only
```

that cannot be a valid D3D11 pair. The flat-black silhouette is a very plausible symptom because the real PS now executes and multiplies against missing/invalid interpolants such as `COLOR0`. Treat the read values as undefined rather than guaranteed zero, but “everything goes black” is exactly the kind of failure I would expect.

**Ranking**

1. **Approach 1: VS recompile lane mirroring PS lane**
   Best primary path. It fits the architecture you just established: retain source, recompile, reflect, cache signature, degrade gracefully. Separate `D3DCompile()` calls are not a blocker if the original VS and PS HLSL structs are actually compatible. The risk is asset reality: old D3D9 HLSL may have VS/PS structs that were compatible under D3D9 semantic matching but compile into incompatible D3D11 signature order/registers.

2. **Approach 5: shared generated/interposed interpolator contract**
   Best fallback or second layer. I would not start by forcing every shader through a synthetic shared header, because it risks semantic surgery across 50-100 shader families. But if independent VS/PS recompiles still disagree, generate a per-pass canonical interpolator ABI from reflection and patch/wrap source at compile time. Do this narrowly, per shader family/pass, not as one global mega-struct.

3. **Approach 2: `ID3D11Linker` / FLG**
   Poor fit here. Shader linking is a Shader Model 5-era feature path and adds a lot of machinery. Microsoft positions shader linking around libraries/graphs and runtime linking, supported starting with Windows 8.1: https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/using-shader-linking. It is not the natural answer for SM2-era asset HLSL or `vs_4_0_level_9_x`/`ps_4_0_level_9_x` style ports.

4. **Approach 4: explicit register binding hints in PSRC**
   Mostly a dead end. `packoffset` is for constant-buffer layout, not interpolators. SM2-era HLSL semantic text does not give you a reliable D3D11 interpolator register pinning mechanism. You may influence order by struct declaration order, but that is source-shaping, not explicit binding.

5. **Approach 3: DXBC binary patch/remap**
   Avoid except as a last-resort research tool. It is fragile, unsupported, hard to validate across driver/compiler versions, and likely to create future artifacts that look like renderer bugs.

**Recommendation**

Plan 17-04 should be: add the VS source-retention/recompile lane, reflect VS output signatures, then validate each bound VS/PS pair at bind or draw time before issuing the draw.

Concrete shape:

```text
1. Retain VSRC on the shared VS program structure.
2. Recompile //hlsl VS source in Direct3d11_VertexShaderData.
3. Reflect VS output signature once.
4. At shader pair bind/apply time, compare VS outputs against PS inputs.
5. Log pair key, shader names, pass/material name, and exact mismatch.
6. If valid: draw.
7. If invalid: use a controlled fallback path, not silent broken rendering.
```

Fallback should be source-level ABI normalization, not DXBC patching. Start by preserving/using the original asset VS source. If signatures still mismatch, build a narrow “interpolator bridge” system that emits or rewrites the VS output struct to satisfy the PS input signature for that pass. The PS input signature should generally be treated as the consumer contract.

**Gotchas**

`COLOR0`/`COLOR1` are not magic D3D9 color registers in D3D11. They are just user semantics unless translated to system values. They still must match between stages.

Mask matters. A VS writing `xy` while the PS reads `xyzw` is not compatible for the wider read. If the PS only consumes `xy`, `mask=0x3` can be fine, but reflection comparison must use the PS read mask, not just semantic name.

Order/register assignment matters, not just semantic spelling. Your `TEXCOORD0`/`TEXCOORD1` sharing register 2 with complementary masks is legal packing, but only if the VS produces the same packed layout.

Interpolation modifiers can matter. `linear`, `centroid`, `nointerpolation`, sample qualifiers, and component types should be included in your validator. A mismatch may not always show as the same debug-layer ID.

Do not ignore `SV_Position`/position handling. It may be omitted from your “interpolator” dump, but the actual VS output still needs the correct position output for rasterization. Keep user varyings and system values distinct in the logs.

The lack of `id=342` does not refute missing data. The debug layer may be reporting register-layout incompatibility first or aggregating by semantic family. `id=343` at that volume is already decisive.

**Plan 17-03 Impact**

Plan 17-03 does not need a conceptual redesign, but it should not be judged visually until linkage is fixed. The reflected `SwgVertexConstants` cbuffer at `b0`, size 400, shared across PS programs, still sounds like the right lane for material/texture-factor constants. Once interpolators become valid, cbuffer mistakes will probably change the failure mode from “flat black silhouette” to wrong tint/spec/bump/material response. Keep the cbuffer reflection-driven and add the VS cbuffer reflection too, because the VS recompile lane may reveal its own `b0`/matrix expectations.
