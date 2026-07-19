# 2026-07-19 — gl05 bytecode-cache sibling LANDED + VB-lock probe armed-on-demand — pushed `2672dff0f`

**READ FIRST after restart** (with the 2026-07-18 session-close checkpoint for the Goal B /
Utinni state — that arc is UNCHANGED this session, still awaiting consumer v19 bind+smoke).
One Saturday-evening session (2026-07-18 local; report-log timestamps read 07-19 because the
report log stamps UTC). `origin/master` = `2672dff0f`. Working tree clean except the
long-parked CONSULT-56/57/66 `.out` files (intentional, unchanged).

**Bottom line:** the "gl05 bytecode-cache sibling" backlog item (carried since 07-09, enriched
07-13 with two convicted stall classes) is now HALF closed + HALF instrumented:
the shader-compile/disk-read half is FIXED (CONSULT-68 RAM-preload port, verified live);
the 815ms skeletal first-draw `Lock()` half now has a purpose-built standing probe ready to
convict its mechanism on the next Win32 stall session. Kenny explicitly DEPRIORITIZED the
~560ms GroundScene-ctor mega frame ("hidden by loading screens, may not be worth the
squeeze") — do not resume it without a fresh ask.

## 1. What landed (`2672dff0f`, 6 files, all inside the Direct3d9 plugin)

### 1a. Direct3d9_ShaderCache RAM preload (CONSULT-68 sibling port)

The gl05 disk bytecode cache (CONSULT-45, `shader-cache-d3d9/<hash>.cso`) had the exact
pre-fix gl11 shape: `tryLoad` did a per-hit `fopen`/`fread` in the draw/first-create path.
Ported the gl11 CONSULT-68 fix faithfully:

- `install()` (called from `Direct3d9_VertexShaderData::install`, after `ConfigDirect3d9::install`
  — ordering verified Direct3d9.cpp:967 vs :1527) starts a background `std::thread` that slurps
  every `<16-hex>.cso` in the cache dir into a mutex-guarded `unordered_map<uint64_t, vector>`.
- `tryLoad` checks RAM first. While the preload is still running, a map miss falls through to
  the old per-hit disk read; **after `ms_preloadDone`, a map miss is authoritative** — the draw
  path never touches disk. RAM hit copies the blob out (few KB) and still populates the
  in-memory L1 at the call site as before.
- `store()` keeps the RAM map coherent (fresh compile re-queryable this session even if the
  disk write fails). `remove()` joins the thread and reports
  `hits=N (M from RAM) ... preloaded=K`.
- **Kill switch: `[Direct3d9] shaderCachePreload` (default true).** NO cfg edits made on either
  platform — defaults carry it, so zero cfg-parity risk (the 07-08 lesson).
- Content-hash key unchanged (source + defines + target + rewriteVersion + compiler tag), so
  stale-bytecode safety is identical to the disk path; the RAM map is keyed the same way.

Files: `Direct3d9_ShaderCache.{h,cpp}` (the port), `ConfigDirect3d9.{h,cpp}` (the key).

### 1b. Standing probe: `[Direct3d9] logDynamicBufferLockMs` (default 0 = OFF)

For the OTHER convicted class — the **815ms `Direct3d9_DynamicVertexBufferData::lock` stall
during skeletal first-draw** (2026-07-13 stack-sampler evidence, mechanism unknown). Both
`Direct3d9_Dynamic{Vertex,Index}BufferData::lock` now time the D3D `Lock()` call
(PerformanceTimer, TEXCREATE idiom) and, when the key is set to N, log any Lock blocking
>= N ms:

```
VBLOCK %8.3fms discard=%d offset=%d length=%d locks=<frame>/<sinceCreate> discards=<frame>/<sinceCreate>
IBLOCK ... (same shape)
```

That line set discriminates the two candidate mechanisms:
- **DISCARD rename pressure** → `discard=1` + high `discardsSinceBeginFrame` (the ring
  wrapped repeatedly in one frame).
- **Driver sync on NOOVERWRITE / other** → `discard=0` blocks, or isolated discards.

**Leading suspect (scoping find, NOT yet convicted):** the shared dynamic VB ring is sized by
a 16–64MB-VRAM-era heuristic in `Direct3d9_DynamicVertexBufferData::install` and **caps at
2MB** (256KB on FFP); ALL software-skinned skeletal meshes pump through it, so a first-draw
burst of avatars can plausibly wrap it many times in one frame. If the probe confirms
discard-storms, the candidate fix is ring sizing / sizing-heuristic modernization — but
measure first; do NOT touch `ms_size` on suspicion.

Files: `Direct3d9_Dynamic{Vertex,Index}BufferData.cpp`.

## 2. Gates run (all green)

- **Release Win32** `/t:Direct3d9;Direct3d9_ffp;Direct3d9_vsps;SwgClient`, serial (`/m:1`),
  `/nodeReuse:false`: exit 0, **0 errors, 0 `unresolved external symbol`**; gl05/06/07_r.dll
  restaged. SwgClient did NOT relink — correct: changes are plugin-internal
  (`ConfigDirect3d9.h` is consumed only by the three D3D9 plugins — no shared-header ABI
  cascade, and all three plugins rebuilt together anyway).
- **Release x64** same targets: exit 0, 0/0, `stage-x64/` DLLs restaged.
- **Win32 gl05 boot smoke** (stage cfg already `rasterMajor=5`): 50s run, no crash, no new
  dumps (every `.mdmp`/`.txt` in stage/ predates the smoke — newest is 21:14 from Friday's
  Goal B gate sessions), and the money line landed:
  `Direct3d9_ShaderCache: preload complete (71 cached shaders in RAM)`.
- Debug configs NOT rebuilt (the standing carried item "Debug-config 5-target refresh" still
  open; `SwgClient_d` + gl05_d will pick these files up on the next Debug build).

## 3. Standing tools / keys after this session

- `[Direct3d9] shaderCachePreload` — default ON, kill switch only. Regression signature would
  be a wrong/stale gl05 shader (should be impossible — identical content-hash key); flip to
  false first if one ever appears.
- `[Direct3d9] logDynamicBufferLockMs` — default 0=off; **arm at 5** in the matching cfg
  (`client.cfg` for `_r`, `client_d.cfg` for `_d` — remember exe↔cfg pairing) for the next
  skeletal-stall hunt, ideally together with the stall stack sampler. Remove the key after
  the run (cfg-parity hygiene); the code stays in-tree permanently.
- All prior standing tools unchanged: `[ClientGraphics] logTextureCreates`, stall stack
  sampler, `[editor.ws]` diags, allocator discriminator, `wsSelfTestSaveOnLoad`.

## 4. WHERE TO RESUME — perf-arc backlog (post this session)

1. **815ms gl05 skeletal VB-lock stall** — probe is in; needs a Kenny Win32 repro session
   with `logDynamicBufferLockMs=5` (+ sampler) → read VBLOCK lines → mechanism → fix.
2. ~~GroundScene-ctor mega frame~~ — **DEPRIORITIZED by Kenny this session** (hidden behind
   loading screens); leave parked unless he re-raises it.
3. preloadSomeAssets single-item overshoot (AsynchronousLoader routing) — still consciously
   deferred.
4. Driver-threading soak call (Kenny) → flip ConfigDirect3d11 default.
5. Probe strip pass after soaks (PortalCullProbe chatty; TEXCREATE + sampler + editor.ws +
   the two new [Direct3d9] keys are standing tools, NOT strip candidates).
6. Carried: ilm-extract audit, real-door trigger brittleness, Debug-config 5-target refresh,
   parked CONSULT `.out` files in `.planning/research/`.

**Goal B / Utinni:** untouched this session — the 2026-07-18 session-close checkpoint governs.
Open item remains consumer-side only (v19 re-sync + bind + smoke; §6 addendum + positionchanged
ANSWER are required reading their side; `cuiRadialMenuManager::clear` pre-approved for the
next bump).

## 5. Session gotchas (tooling — worth remembering)

- **PowerShell splits unquoted `;` in MSBuild `/t:`** — `/t:A;B;C` bare runs only target A and
  then tries to execute `B` as a command. Quote the whole switch: `"/t:A;B;C"`. (First build
  launch this session was wrong for exactly this; killed + relaunched.)
- **Sandboxed Stop-Process cannot kill MSBuild/cl orphans** (Access denied) — after killing a
  background build task, the child compilers survive and hold output locks; the kill needs an
  unsandboxed shell call. Verify `0 remaining` before rebuilding.
- **Report-log timestamps are UTC** — a `202607190044…` line at 7:44 PM local on 07-18 is the
  SAME moment, not a mystery future entry. Don't cross-match report-log times against file
  mtimes without the offset.
