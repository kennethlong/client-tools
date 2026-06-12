# Handoff — 2026-06-02 (end of a long session)

Branch: `koogie-msvc-cpp20-base`, main checkout `D:\Code\swg-client-v2`. Two threads this session: (1) the D3D11 release boot-crash — **RESOLVED**; (2) the cape/garment "black spikes" — **root-cause hypothesis + decisive next experiment ready**.

---

## 1. D3D11 release boot-crash — RESOLVED ✅
**Root cause: EXE/gl11 ABI mismatch.** The staged `SwgClient_r.exe` linked a **stale `clientGraphics.lib`** where `ShaderImplementationPassPixelShaderProgram::m_graphicsData` is at **+0x18**; `gl11_r.dll` was built against the current header where it's at **+0x20** (+8 = the Plan-17-01 `m_psrcText`/`m_psrcLen` members). EXE stored the valid PS-data at +0x18; gl11 read +0x20 = leftover garbage → `c0000005` in `StaticShaderData::apply` at the first PPEM composite draw. The documented [[shared-header-ABI-cascade trap]].

**Proof:** disasm of the old EXE's Program ctor = `mov [esi+18h],eax`; in the crash dump `program+0x18 = 0x0a96dee0` (a live, round-tripping PS-data) while `program+0x20 = garbage`. Took 8 runtime experiments + 2 CODEX/Cursor consult rounds; the `ba`-hardware-watch zero-fires were *correct* (store goes to +0x18, nothing writes +0x20) — WoW64/32-bit `ba` is also flaky here, so we use **software bps only**.

**Fix applied + verified:** rebuilt `clientGraphics.vcxproj` Release, relinked `SwgClient.vcxproj` Release (deleted output exe to force the link; killed leftover cdb/client holding the stage copy-lock; manually staged). New EXE disasm = `mov [esi+20h],eax` ✓. **Kenny confirmed release client BOOTS.** Old broken exe: `binary-backups/broken-abi-mismatch-20260602/`.

**LESSON (now in memory):** when the shared struct header changes, rebuild the EXE's dependency libs (clientGraphics etc.) **before** relinking the exe — the exe vcxproj does NOT auto-rebuild them. That one missed `clientGraphics` rebuild caused the whole crash.

**Cleanup follow-up (not blocking):** the live-deref change in `Direct3d11_StaticShaderData.{h,cpp}` (`m_passVSSlot`/`m_passPSSlot`) is **uncommitted and now known INERT** (the bug was never a stale cache). Harmless, but its comments assert the falsified device-restore/UAF theory. Revert it or rewrite the comments. (Offered; Kenny hadn't decided.)

---

## 2. Cape/garment "black spikes" — investigation (next up)
**Symptom:** sharp dark spikes erupt from the neck/shoulder/back of **dresses, robes, garments** (a CLASS, per Kenny — not one asset; char bodies are fine). A few skinned-mesh verts flung to garbage positions.

**Established facts:**
- **D3D11-only** — D3D9 (`gl05`, rasterMajor=5) renders the SAME character's robe clean. (Rebuilt `gl05_r.dll` to match the +0x20 EXE so it's a valid reference.) A/B evidence: `.planning/research/cape-spike-evidence/` (d3d11-cape-spikes vs d3d9-clean + README — also a ready GAMMA-01 before/after).
- **Capture-invisible** — RenderDoc replays clean (Kenny + a 162-draw capture `…/Temp/RenderDoc/SwgClient_r_*_frame4908.rdc`). So RenderDoc **cannot localize** it: the corrupting data isn't in the captured state (zero-filled on replay). NOTE: the renderdoc-mcp **can't inject the 32-bit client** (no 32-bit renderoccmd) — capture via qrenderdoc GUI, analyze via MCP `open_capture`.

**Leading hypothesis:** undefined-memory-read class. KEY asymmetry: cbuffers (`P19_CBUF_ZERO_TAIL`) and static VBs memset their `WRITE_DISCARD` tail, but the **dynamic VB/IB ring does NOT** (`Direct3d11_DynamicVertexBufferData.cpp` lock ~250-287). Prior `P19_DISCARD_ONLY` only cleared the ring for a *sync* hazard, never tail-garbage. → **garment-class skinned mesh likely reads dynamic-ring bytes past what it wrote** → undefined live (spikes) / zeroed on replay (clean).

**DECISIVE NEXT EXPERIMENT:** add a `P19_VB_ZERO_TAIL` memset to the dynamic-VB ring on `WRITE_DISCARD` (mirror the cbuffer fix), rebuild `gl11_r`, view a dress/robe:
- spikes collapse-to-origin / vanish → **confirmed**; real fix = stop the overrun (correct garment vert/index count).
- no change → garments use a static/system VB, not the ring → pivot.
- **FIRST verify** garment skinned verts actually flow through the dynamic ring vs a SystemVertexBuffer/static VB (trace SkeletalAppearance2 / ShaderPrimitive → which VB). SWG appears to CPU-skin (no bone-matrix VS constants in Graphics.cpp).

**Memory:** [[project_d3d11_cape_spike_skinning_regression]] (full detail + hypotheses).

---

## Environment state
- `stage/client.cfg` `rasterMajor=11` (D3D11, restored after the D3D9 reference check).
- Working: release `SwgClient_r.exe` (boots), `gl11_r.dll` (6/2 17:13), `gl05_r.dll` (rebuilt today, +0x20). DEBUG client also works.
- MCP servers up: windows-mcp (drives the desktop; DirectInput needs foreground), renderdoc-mcp (analyzes .rdc; can't inject 32-bit). Local SWG server runs in VirtualBox (SWGSource v3.0.2). Login: user `swg` / pass `swg` → Enter through char-select.
- Build: `& $env:MSBUILD <vcxproj> /p:Configuration=Release /p:Platform=Win32 /nodeReuse:false /m`; delete the output exe to force a relink; back up `stage/SwgClient_r.exe` before client rebuilds.

## Start-here next session
1. (optional) decide on the inert `StaticShaderData.{h,cpp}` live-deref change (revert or keep+fix comments).
2. Cape: trace garment skinned-vert VB path (dynamic ring vs static), then run the `P19_VB_ZERO_TAIL` experiment.
