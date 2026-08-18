# 2026-08-16 SESSION CLOSE 6 — the port queue emptied, his-cfg compatibility test, music-transition wave

**Branch state: `strict-data-defaults` at 48 commits over master, ALL PUSHED (tip `e455e4365`).
PR #1 body current through 47 commits (needs +1 section for the music wave — see owed list).
Everything field-verified same-day EXCEPT the music wave and the his-cfg resume issue below.**

## ✅ RESUMED 2026-08-16 afternoon — resume items done + a major find

**Both resume items below are DONE** (user.cfg autoConnect keys commented, originals in
`bak-20260816/user.cfg`; exe deployed after Kenny quit). Additional user.cfg env fixes the
original scan missed: `[ClientAudio] enabled=true` ADDED (his cfg family never enables audio —
the "no sound at all" run; warning.log said "Audio is disabled", JUCE itself initialized fine),
`disableWorldSnapshot=1` commented (his ECON-SIM blank-slate key — would suppress the whole
static world), `topDownCamera=1` commented (overhead camera, no input wiring).

**THE FIND: the dPVS 6-fix port `eb1b26024` was a ZOMBIE — never in the binary.** Kenny hit the
foyer skybox hole at the exact capture spot (3458,5,-4846) with the ported exe. Two mechanisms:
(1) the Copy-Item mtime trap (item 7's gotcha) silently skipped recompiling DoorObject.cpp;
(2) his repo NEVER compiles dpvs — the exe links prebuilt `deps/x64/lib/dpvs.lib` from the
original squash, and his dpvs.vcxproj is a VC60-upgrade artifact (Release|x64 = Utility, builds
nothing). Only the hand-merged FreeChaseCamera fixes ever compiled. "5 clean logins" at ~1-in-7
odds was a 46% coin flip, not a soak pass. **FIX = `bbda39e18` (commit 49, PUSHED):** our v145
StaticLibrary vcxproj + x64-platform.props ported into his tree, his two x64 compile breaks
fixed (ImpObject MatrixCache::Entry 68→64 bytes pad under _M_X64; RecursionSolver UPTR hash
cast), dpvs.lib rebuilt from HIS sources (`g_swgDpvsPortalRejects` marker verified in the lib),
vendored. Exe `B3BF329B…` + gl11 `83A821…` deployed to BOTH stage-B-x64 and _client_dx11.
**dPVS soak watch RESTARTED from zero.**

**Later same afternoon:** Kenny's `_client_dx11` run confirmed audio working AND **the music
wave is VERIFIED** ("audio doesn't have the volume shift on world render" = the duck-release
ramp, item 8a).

**D3D9 WHOLESALE WAVE LANDED `079eb8a22` (c50) + PSRC pixel-path fix `9c1e781f9` (c51), both
PUSHED 2026-08-16 PM — ✅ KENNY-VERIFIED: FULL FOUR-RENDERER SWEEP (gl05/06/07/11) boots and
renders clean on BOTH datasets, zero portal glitches across the sweep sessions (dPVS soak
building). PR #1 body refreshed through 51 (D3D9 section added, D3D9 known-gap retired) —
owed list CLEAR again.**
⚠ The wave's first cut BOOT-CRASHED on both installs (validator AV at
Direct3DShaderValidatorCreate9 loading 2d_vertexcolor.sht): the blind-PEXE pixel ctor met the
asm2hlsl corpus's 4-byte STUB PEXE chunks (hex-confirmed; the corpus carries real programs as
//hlsl PSRC only). LESSON: his recompile-from-PSRC pixel path was LOAD-BEARING corpus
infrastructure, NOT compensation — "wholesale replace" must inventory which of the target
tree's local changes are data-contract requirements. Fix = his ctor design restored D3DX-free
(D3DCompile + ID3DInclude port + our ShaderCache warm-start; his constants-inc override +
texren_copy_c1a1 substitution kept verbatim — both still required whenever compiling from
source). Watch item: `D3DCompile failed` WARNINGs in gl05 logs name corpus shaders the new
compiler rejects (fallback to stub PEXE can still fault). His Direct3d9 (incl. Direct3d9_LightManager compensation stack) replaced with our
stock-faithful gl05/06/07 sources; x64 now D3DX-FREE (D3DCompile + HlslRewrite + bytecode
ShaderCache — the 4 new files); FPU_PRESERVE; two clientGraphics x64 sort-key truncation fixes
came along (ShaderEffect.h + StaticShader.cpp, hash-to-int, no ABI cascade — caught by our
/we4311 props); v35 Gl_api tail rows STRIPPED from the port (his Gl_dll.def has no toolkit
surface; sizeof(Gl_api) gate preserved); our 3 vcxprojs ported (v145 + x64-platform.props);
jpeg62-x64.lib + jpeg62.dll vendored into his deps/x64 (his libjpeg headers are md5-identical
6b), gl05 postbuild stages jpeg62.dll. Build: 0 unresolved externals; exe `4189AF21…` + gl05
`7E8C8787…`/gl06 `822D7181…`/gl07 `E2E35601…` deployed to BOTH stage-B-x64 and _client_dx11.
NOTE: his solution references dpvs.vcxproj, so dpvs now REBUILDS in-solution since bbda39e18 —
but the exe still links deps/x64/lib/dpvs.lib: future dpvs source edits need the deps copy
refreshed (stale-deps trap, tracker note). VERIFY LIST: rasterMajor=5 boots to ground on x64;
log shows d3dcompiler paths (no D3DX); rasterMajor=6/7 boot; cantina walls solid; screenshots
still write (libjpeg). PR body owes the D3D9 section after verify.

**TOOLING SWEEP + DEBUG|x64 FIRST BUILD `e11bd2596` (c52, PUSHED) 2026-08-16 evening — ⚠ Kenny
Debug boot smoke pending.** Sweep verdict: the only real tooling asymmetries were (1) the
unresolved-external build gate (his exe links /FORCE, script had NO check — gate now in
Build-Client.ps1, tees to a log, fails loudly; it's what surfaced everything below) and
(2) Debug|x64 had NEVER built. Debug repair chain (all in c52): Tag.h strlen cast (kills the
tree-wide C4267 flood line) + MeshConstructionHelper VALIDATE size_t args + crypto filters.h
cast; clientAudio/clientDirectInput/crypto Debug-only /WX flipped off (their own Release
doesn't /WX); libMozilla Debug|x64 → Utility (SAIS'S OWN Release|x64 pattern — "XULRunner
can't compile on modern x64 SDKs, link uses the hand-written stub deps lib"; the _DEBUG/NDEBUG
archaeology was a dead end, /MTd implies _DEBUG); swg.sln dpvs Debug|x64 mapped to extinct
IntelCPP config → Debug|x64; dpvsMemory.cpp Debug-only guard tags UPTR-cast; SwgClient
Debug|x64 link REWIRED (LinkIncremental off — incremental silently disables /FORCE (LNK4075);
x86 soup CUT from deps — it was masking the real unresolved set; dpvs+lcdui explicit
Debug-built paths — deps Release/MT copies outranked them = the "130 mismatches" LNK1319 =
130 LNK2038 debug-CRT conflicts; dinput8+dxguid restored); deps/x64 gains
vivoxSharedWrapper_d (/MTd rebuild from in-tree source, needs VIVOX_VERSION=3) +
videocapture-stubs_d (+committed compat-source stub .cpp — video capture is structurally DEAD
on x64, impls are x86-only; Release never pulls SwgVideoCapture.obj, Debug does).
RESULT: first-ever SwgClient_d.exe (105MB, `092101AF…`) + gl05/06/07_d, ZERO unresolved.
Debug set + client_d.cfg staged in stage-B-x64. PR body owes the c52 section.

**FIRST DEBUG BOOT PAID OFF IMMEDIATELY — NEW ILM LANDMINE CLASS: ALL 102 `.skt` skeletons in
`ILM_visuals.tre` are CORRUPT** (Kenny's Debug boot FATALed at char select:
`all_b.skt/SKTM: exit form but not at the end of it`). Mechanism, byte-proven: the Legends
tool that stripped skeleton LODs (SLOD wrapper → bare LOD-0 SKTM; a preference-strip on top)
wrote FORM size fields LARGER than the actual payload (all_b: declares 2954, file has 2938
content bytes — 16 phantom bytes). The Debug-only `IFF_DEBUG_FATAL` in `Iff::exitForm` caught
it; **Release has silently seek-past-EOF'd these on every char-select load all along** (both
trees — loaders identical and correct; the data lies). ILM copy shadows the intact sku0 copy
(sku0_client.toc → patch_17_00.tre, a proper 4-LOD SLOD). FIX: validated all 102 ILM skts
programmatically (recursive IFF size-fit check) → 102/102 corrupt → all 102 overridden with
intact stock copies extracted from the sku0 chain into `stage-B-override/appearance/skeleton/`
(loose path outranks tres; also restores the stripped LODs = ILM-is-content rule). ⚠ Kenny
Debug re-run pending. ⚠ `_client_dx11` (his cfg) also mounts ILM_visuals — his-cfg DEBUG runs
would hit the same FATALs (Release fine); his `clientoverride` doesn't have the fixes.
ILM-audit scope: the corrupt-size-field CLASS may extend beyond .skt in ILM_visuals — audit
should IFF-validate every ILM file, not just content-diff.

**Debug NPC-vanish-until-10m: CONVICTED + FIXED same evening — it was MY skeleton overrides.**
Kenny's decisive clue: the held DRINK rendered and animated (skeleton ticking, attachments
drawing) while the body didn't; `noRenderScreenFraction=0` probe = no change (cull disproven,
probe removed from client_d.cfg). Real cause: ILM stripped the WHOLE visual LOD layer
(2,658 .lmg + 2,789 .lod in ILM_visuals, not just skeletons) — my stock 4-LOD SLOD skeleton
overrides mismatched the ILM single-LOD meshes, so beyond ~10m the appearance selected mesh
LODs that don't exist. FIX: all 102 overrides regenerated as **size-REPAIRED bare SKTMs**
(honest headers, LOD-0-only shape consistent with ILM meshes; bottom-up IFF clamp-repair +
revalidate, 0 failures). Full LOD restoration = the ILM saga's job, done LAYER-CONSISTENTLY
(skeletons + lmg/lod + sat together). ✅ **KENNY-VERIFIED "Debug is clean"** — char select
loads, NPCs draw at distance. The c52 Debug wave is field-verified END-TO-END; PR #1 body
refreshed through 52 (build-infrastructure section added — owed list CLEAR).

**TRE-SET BUILDER VISION captured (Kenny)** — the ILM saga's end-state: a config-driven
tool composing playable TRE sets from LAYERS (base = his ORIGINAL retail install on C:,
then SWG-Source bugfix layer, JTL, ILM-lighting, ILM-sound — each opt-in via recipe),
corrupt headers repaired at compose time, layer-consistency ENFORCED, recipe-not-bytes.
Full design in memory `project_ilm_tre_cleanup_saga.md`. New build needed: TRE/TOC writer
(v1 = loose-dir output). Sequencing: after merge close + PR #2.

**Tombstone — "weird shadows" on `_client_dx11` (2026-08-16, DO NOT re-investigate):** angular
dark polygon patches on characters = skeletal shadow-volume self-shadowing. Cause of the
A/B difference: HIS install's `local_machine_options.iff` has renderShadows ON + high
shadowDetailLevel + mesh/skeletal shadows; stage-B's has shadows OFF (never draws them).
Stale shaders RULED OUT (shadowvolume.vsh/.psh live-compiled that session + byte-identical
sources both layerings). **Kenny D3D9-verified: gl05 shows the SAME patches — and then made
it CONCLUSIVE: the STOCK retail binary (`SWGSource Client v3.0` install, artifact-run options
file carried over for exact parity, restored after) shows the same patches too.** Verdict
chain: gl11 = gl05 = stock retail ⇒ original-game skeletal shadow-volume jank at every layer.
Remedy is the in-game shadow options, not code. CASE CLOSED — do not reopen for any renderer. **PR #1 body refreshed to 49 commits** (music-wave section + dpvs-buildability
section added, the voided "three clean logins" verify claim corrected, dpvs-tidyings known-gap
updated; PR stays CLOSED). Owed list: CLEAR. Remaining opens: dPVS foyer soak (Kenny watching),
D3D9 wholesale wave, passive soaks.

## ~~⚠ RESUME HERE~~ (done, see above): the `_client_dx11` new-character-screen issue

Kenny's his-cfg run goes DIRECTLY to the new-character (creation) screen. **Cause identified,
one-line fix pending:** his `user.cfg` carries `loginClientID=1001` + `autoConnectToLoginServer=true`
+ `autoConnectToCentralServer=true` — it auto-logs into OUR server (192.168.1.200) as HIS station
id, which has no characters here, so the client skips login/char-select straight to creation.
His own comment block in that file documents the adjacent trap (`launcherAvatarName` alone
auto-connects via `SwgCuiAvatarSelection::autoConnectOk()`). **Fix: back up `user.cfg` to
`bak-20260816/`, comment `loginClientID` + both `autoConnectTo*` lines** so it presents the normal
login screen and Kenny logs in with his own credentials. (Our stage-B flow has no autoConnect keys
— Kenny logs in manually.)

**Also pending on `_client_dx11`:** its `SwgClient_r.exe` is the PREVIOUS build (`D2D225…` — has
everything except the music wave). The new exe (`A9D0E8…`) is built but the copy failed because the
client was still RUNNING (file locked). Once no client is running from that dir:
`Copy-Item src\build\win32\x64\Release\SwgClient_r.exe,...SwgClient_r.pdb D:\Code\Galaxies-Reborn\_client_dx11\ -Force`
(from the swg-source-x64-dx11 repo root). stage-B-x64 already has the new exe.

## The his-cfg compatibility test (Kenny's ask: "point his config back to the one he gave us")

- His real cfg family lives in **`D:\Code\Galaxies-Reborn\_client_dx11\`** (client.cfg →
  login/live/preload/options/user.cfg; relative TRE paths ⇒ must run FROM that dir).
  `stage-B-x64/client-stock.cfg` is NOT his file (it's our stock-acceptance layering).
- **RESULT SO FAR: loaded clean through his exact data layering** — 72 direct TREE0005 roots, raw
  ILM mounts, 4 TOCs, `clientoverride` loose path, rasterMajor=11, strictData default ON. No FATAL.
- Environment fixes applied (originals in `_client_dx11\bak-20260816\`): `options.cfg`
  3840×1080 (his super-ultrawide) → 1600×900 windowed; `login.cfg` 127.0.0.1 (his server) →
  192.168.1.200 (ours). His binaries (8/10 build) also backed up there; our branch binaries staged
  (exe+gl05/06/07/11).
- Expected on next run: first-run shader compiles (his 8/5 118-blob bake won't match our compiler
  keys), epic music (his raw ILM_music mount), his data quirks are HIS — the test is layering
  compatibility.

## Today's landed work (all on `strict-data-defaults`, chronological)

1. **Audio dropout drip CLOSED (4 rounds)** `604b66bf1`→`8bcbc9087`→`314efe78f`→`2d48b38ae`:
   lock-free hot setters + repeat-last-block mask; I/O evicted from the lock (open/fill/serve);
   callback waits (pause→yield, 3ms). Field: 0.162%→0.024%→~0 dropouts, waits recover ≤2.3ms,
   ear clean. KEY PHYSICS: miss rate tracks TOTAL lock-held time; only ms-class holds matter.
2. **Screenshots**: `4a17bc275` F12 backup key (Win11 Snipping Tool eats PrintScreen before
   DirectInput) + `4f74ae47c` WIC SetPixelFormat NEGOTIATES / WritePixels converts nothing
   (period-4 stripe proof). Both field-verified.
3. **dPVS 6-fix port** `eb1b26024` — capture-convicted first (Capture230 pixel history:
   clear→skybox→NOTHING = visibility-layer cull). 5 wholesale dpvs/DoorObject copies +
   FreeChaseCamera hand-merge (re-tag only; camera mandate respected). 5 clean foyer logins →
   SOAK WATCH. RenderWorld probe suite NOT ported (his own APIs there).
4. **cellLoaded flip REVERTED to stock false** `86606befe` — Kenny invoked the fix-it-all mandate.
   His rationale refuted in his own tree (Container::addToWorld contents-walk intact); his other
   two rework members KEPT. Verified: cantina interior normal, zero cell warnings.
5. **Shader cache incremental re-bake** `1dbd380f4`+`214c98165` — baker seeds from existing
   manifest (union; validation-gated), multi-word names fixed. Field: 198→204 programs, sessions
   now 0 misses. `dxbcbake` does NOT exist anywhere (comment-only aspiration).
6. **Phased WorldSnapshot load port** `c004ba687` — the "hard hand-port" was clean (his load() =
   our pre-CONSULT-60 baseline). Full mechanism + 11 valves + CONSULT-61 50ms preload slice.
   `[ClientGame] worldSnapshotParseBudgetMs` (40; ≤0 = kill switch). VERIFIED "buttery smooth".
7. **searchPath negCache + file manifest port** `7e9c653d8` — hunk-port preserving his TRE-0006
   TOC support. Field numbers same day: `[treefile.probe] stage-b-override: realProbes=63
   manifestSkips=34530` (−99.8%). Kill switches `[SharedFile] searchPathNegativeCache/FileManifest`.
   **⚠ GOTCHA: Copy-Item preserves source mtime → MSBuild skips recompiling copied files → LNK2019.
   Touch copied files before building.**
8. **Music-transition wave** `e455e4365` (**⚠ UNVERIFIED — the one open verify**): (a) duck-release
   snap fix — bg music gets its OWN continuously-ramped fade (Kenny's "abrupt attenuation at
   loading→scene handoff", exactly A's c994f74e8 find); (b) blocking 1s scene-change fade pump
   removed (fade rides main loop; `[ClientUserInterface] blockingSceneChangeMusicFade=true`
   restores; sequencing preserved via new CuiManager::isMusicPlaying + GameMusicManager deferral);
   (c) mid-load fake sunrise suppressed (first 15s of scene). Verify = zone-in: duck releases as a
   ramp, no track overlap, theme survives load.

Earlier same day (already in memory/tracker): PR body refreshed twice (through 47), F12+capture
workflow proven (Capture230 + screenshots), stage-B session logs triple-clean.

## Open board (priority order)

1. `_client_dx11` resume: user.cfg autoConnect fix + exe copy (top of file) → his-cfg full test
   (login OUR server with Kenny's creds, char select, zone in) → then read `_client_dx11` logs
   (report log, [treefile.probe], strict-data warnings against HIS dataset).
2. Music wave verify (any zone-in, ear + no track overlap).
3. **PR #1 body: add the music-wave section, bump to 48** (pattern: gh pr edit 1 --body-file on
   scratchpad pr1-body.md — but scratchpad dies with session; REBUILD body edits from
   `gh pr view 1 --json body` next time).
4. **D3D9 wholesale replacement wave** (Q1 answer recorded: NOT done; his Direct3d9 +
   Direct3d9_LightManager untouched) — the biggest remaining item, per the "D3D9 is ours" agreement.
5. Soak watches (all passive): dPVS foyer (5 clean), cellLoaded, audio dropouts ~0, shader cache
   0 misses, treefile.probe lines at exits.
6. Backlog: JUCE hardening list + voice cap; lock-free audio command queue (long-term); dpvs x64
   tidyings note for Sais; PR #1 reopen decision (Kenny's call).

## Standing gotchas re-learned today
- PS 5.1 mangles inline commit messages containing double quotes → `git commit -F <file>`.
- Copy-Item preserves source mtime (both the MSBuild-skip trap AND why staged-DLL mtimes look old).
- Kenny's shortcut launches stage-B — a `_client_dx11` test needs the exe launched FROM that dir
  (relative TRE paths).
- Never overwrite a running client's exe (lock = the reminder; kill discipline in memory).

Tracker: `.planning/SAIS-PR-QUEUE.md` (updated through today). Memory topic files current.

## LATE-NIGHT ADDENDUM 3 (post-coffee): stock-acceptance run -> c53 embedded shader corpus

Kenny's framing: "that is what we will be judged against by the SWGSource engineers." B = his
build + PURE stock mount (client-stock.cfg active in stage-B-x64; A = our client, same cfg
byte-identical in stage-x64; live cfgs backed up as client-live.cfg.bak-stockrun in BOTH dirs —
RESTORE AFTER the stock arc closes).

- Symptom chain: B black login (A fine) -> 9 asm pixel programs refused (ui.psh = the black) ->
  9-name builtin table -> still black (corpus include missing) -> include fallback -> login OK
  but cantina walls/floor missing -> log showed 31 asm pixel + 15 asm VERTEX programs (tfcl/tfcsl
  family = walls/floors) -> full corpus embed -> STILL failing: stock TREs carry ASM files at the
  SAME vertex_program/modules/*.inc paths (X3000 'm4x4'), TreeFile-first include order poisoned
  the substituted HLSL -> final shape: generation-time INLINE of all corpus includes.
- **Ground truth (scan, stock_shader_coverage.py in scratchpad + stock_shader_langs.json): stock
  mount = 806 shader programs, 582 hlsl, 224 asm; his corpus covers 221/224.** Missing 3:
  membrane.psh, water_pass2_20/25.psh — ps.1.x variants; ps20 HLSL siblings win validation
  (VERIFIED: Theed fountain/water renders). Stock .vsh are PLAIN TEXT (not IFF) — classifier trap.
- **c53 = 5232bb632** (pushed): Direct3d11_EmbeddedShaderCorpus.h/.cpp (GENERATED, 221 programs,
  includes inlined, ~1.6MB text; generator = scratchpad gen_embedded_corpus.py, source =
  _client_dx11 drop), pixel hook after refuseIfNotHlsl, vertex hook in parseHeader (re-parses
  embedded header — tag block drives TEXCOORD mapping), vcxproj adds. Fallback-by-contract:
  mounted corpus always wins. gl11_r.dll ~2.6MB now.
- **VERIFIED stock DX11 x64 Release: login, char select, cantina interior (walls/floor), NPCs,
  Theed water.** Kenny: "Cantina looks tacky as ever ;-)" = the ILM-lighting look is baked into
  the v3.0 BASE distribution (not retail-pure) — retail-install-as-base-layer argument for the
  TRE builder.
- gl05 stock pass PREDICTED GREEN, not yet run: our port assembles //asm via the undocumented
  D3DAssemble export in d3dcompiler_47 (Phase 33) + stock pixel PEXE is real D3D9 bytecode.
- PR #1 body owes the c53 section (53 commits now). Then reopen = Kenny's call.

## LATE-NIGHT ADDENDUM 4: the "tacky bright" mystery SOLVED — c54 `8a9021322` (54 commits)

Kenny field-caught it: DX11 bright/washed vs DX9 moody on the SAME stock data, BOTH clients —
so renderer-class, not data. **RenderDoc Capture240 (B, cantina ceiling pixel 700,150 → draw
4345) convicted it in one trace**: baked vColor0 arrives authored-correct (0.28 grey), the
corpus c_ambient.inc translation ADDS scene ambient c16=(1,1,1) → saturates → full-bright
albedo. Stock asm c_ambient = bare `mov r7, vColor0` (the bake IS the cell lighting).

- The add = HIS corpus's leftover compensation for the zeroed-vertex-stream era —
  **CONSULT-74 had already corrected this in stage-B-override** (`r7 = vColor0;` +
  .sais-orig preserved); the embedded corpus was generated from `_client_dx11` (uncorrected
  drop). c54 regenerates with the stock shape inlined (27 consumer translations).
- Kenny verified: cantina now matches DX9 (moody), Theed water good, **space + nebulas good**
  on stock.
- ⚠ FLAG-TO-SAIS (queued in SAIS-PR-QUEUE): (1) his distributed corpus still ships the boosted
  c_ambient.inc → his live-corpus interiors wash identically (likely the ORIGIN of the
  "tacky = ILM lighting" misattribution); (2) his [Direct3d11] ambientBoost include patch
  searches ASM text `mov r7, vColor0` which can never match the corpus's HLSL module — the
  opt-in gate is dead against the current corpus.
- **A-SIDE FOLLOW-UP (our repo, queued)**: OUR gl11 shows the same wash on stock — our
  "black walls full ambient" fix is the same compensation class. Adopt stock cell semantics
  (bake-is-lighting) in our vertex-lit path; RenderDoc re-verify vs DX9 reference.
- "Tacky bright is baked into v3.0 base" from earlier tonight is RETRACTED — it was this
  renderer bug all along (v3.0 base data is innocent on this count).

Remaining on stock arc: gl05 stock pass (predicted green), then restore BOTH live cfgs from
client-live.cfg.bak-stockrun. PR #1 body owes c52+c53+c54.

## STOCK-ACCEPTANCE ARC CLOSED (late night)

- **gl05 stock pass CLEAN (Kenny-verified): cantina moody there too** — both his renderers now
  verified against the shelf dataset. DX11 via embedded corpus (c53+c54), DX9 via D3DAssemble
  + real stock PEXE. The joint-upstream acceptance bar ("build the repo, point at stock,
  everything renders, zero external files") is MET.
- Live cfgs RESTORED from client-live.cfg.bak-stockrun in BOTH stage dirs (BOM-verified);
  `client-stock.cfg` kept in both for future one-swap stock runs.
- Override disposition decided: stage-B-override c_ambient/tfcl* now REDUNDANT with the
  embedded corpus (kept — they're the editable source the table regenerates from). STILL
  LOAD-BEARING on live: 102 repaired skeletons, fog-restore, nebula textures, ui_radar/
  texren/emissive pixel overrides (data fixes + fixes vanilla corpus lacks).
- STRATEGIC (Kenny): unified client = joint Kenny+Sais SWGSource upstream PR for x64+DX11
  when PR #1 is accepted; B becomes THE working codebase then — transition checklist banked
  in memory (migrate path-keyed Claude memory, port gitignored personal layer, archive-point
  handoff; v2 kept frozen as archive).
- Remaining before reopen: PR #1 body refresh (owes c52+c53+c54). Reopen = Kenny's call.

## THE BIG NIGHT WRAP: both PRs live + crew round complete

1. **Deliverable PR = GitHub #2, OPEN** (54 commits; GitHub refused reopening old #1 after the
   historical force-push — #1 carries a superseded-by comment). Body leads with the
   stock-dataset claim + compensation-removal notes. COVERT MODE OVER.
2. **Advertise PR = GitHub #3, OPEN, stacked on #2** (branch `toolkit-advertise`, 2 commits):
   c-tail `da91bbaa1` Gl_api v35 tail slots (frame/resize overlay callbacks; his gl11
   SwapChain implements natively — present()/resize() invoke sites; D3D9 ignore rows
   restored) + c-surface `67b109785` the whole advertise surface (engine_* + 24 shim-bearing
   files ported hunk-by-hunk + support chains: ExitChain shutdown-phase, Game tick callback,
   WorldSnapshot ws-shim family + WSRW saveFiltered/intern, TreeFile::enumerateFiles chain,
   interior-refresh chain, Os WM_SIZE embed-resize; SwgClient.vcxproj + 2 include dirs;
   hookpoints-probe tool). Full x64 Release All build CLEAN; **probe PASS v35/165/0 nulls**.
   Matched 6-binary set DEPLOYED to stage-B-x64 (exe + gl00/05/06/07/11 — Gl_api sizeof
   changed; never mix with pre-#3 DLLs). **Kenny boot smoke PENDING** (checklist in PR body).
   Port discipline notes: WorldSnapshot merged insert-only (his POB-CRC-proceed variant
   KEPT); skipped as out-of-scope for later fix-queue commits: Os.cpp x64 keyCode/menu/
   ShellExecute pointer-width fixes (keyCode = REAL x64 Release input bug), ClientMain
   timeBeginPeriod(1) + bootTrace, DebugHelp minidump-reserve, CreatureObject naked-NPC
   wearables retry, ClientWorld logCellAtPosition probe.
3. **CONSULT-76 TRE-builder round COMPLETE** (.planning/research/CONSULT-76-*): Codex writer
   spec (36KB), Cursor byte maps (24KB; found swg_iff/writer.py already exists!), Sonnet
   recipe ecosystem (TOML + provenance vocabulary + lockfile reproducibility), Opus
   composition semantics (Cell ADT, content-based cohort consistency, regression gating,
   round-trip Stage 8), **Fable SYNTHESIS** (CONSULT-76-SYNTHESIS-tre-builder.md): v1
   cut-line, resolved disagreements (exact pins v1, options = variant sub-trees, trust-split
   verification), REFUTED Codex's validate() risk (dead code — grepped engine) + Cursor's
   clamp formula (vendor the field-proven repairer), CONFIRMED .tre zero-length tombstone
   vs SearchTOC hardcoded deleted=false in the C++. **DECISIVE EXPERIMENT: hand-write one
   minimal TREE/0005 archive (stored+compressed+tombstone ±MD5 variants) + conflicting
   loose file, mount, boot stock client ONCE** — retires writer-format risk before any code.
