# CONSULT-74 — dual-build scene-matrix A/B run protocol

**Purpose.** Convert the renderer question from four consultants reading structure into measured
ground truth, *before* either tree is modified. Decided 2026-08-14 (Kenny): **measure first, fix
after** — several pending fixes (COLOR-interpolant clamp, colour write mask, portal binding) are
exactly the ones whose effect we want visible against a clean reference.

**Freeze rule: no code changes to either tree until the matrix is captured.** Record both commits in
§1 and do not advance them mid-run.

---

## 0. Correction to the experiment as originally specified

**RenderDoc cannot capture D3D9.** The capture-diff is therefore **gl11(A) vs gl11(B)** only. gl05's
role is *visual reference + frame timing* — screenshots and frame-time series, no captures. This is
the same constraint recorded in `AGENTS.md`; it does not weaken the experiment (D3D9 is the parity
oracle by eye and by timing, not by capture).

## 1. Builds — record exactly what was measured

| | Repo A | Repo B |
| --- | --- | --- |
| Path | `D:\Code\swg-client-v2` | `D:\Code\Galaxies-Reborn\swg-source-x64-dx11` |
| Commit | `f5f14c3f6` (HEAD) — **fill in actual at build time** | `3ab047315` (packaged squash) |
| Build command | `$env:MSBUILD` 5-target x64 Release: `Direct3d11;Direct3d9;Direct3d9_ffp;Direct3d9_vsps;SwgClient`, `/p:Platform=x64 /nodeReuse:false` | `.\scripts\Build-Client.ps1 -Architecture x64 -Renderer DX11` |
| Staging | `stage-x64/` (postbuild) | per his script's `-StagePath` |
| Audio | Miles 9.3v | JUCE (his x64 default; his Miles x64 is a link-only stub) |

**A must be rebuilt clean**, not measured from the existing staging: `stage-x64/gl11_r.dll` dates
2026-08-01 while `SwgClient_r.exe` dates 2026-08-07 — currency is ambiguous, and a stale plugin
against a newer exe is the documented ABI-cascade trap. Delete `SwgClient_*.exe` first to force a
relink, and grep the build log for `unresolved external symbol` (must be 0 — SwgClient links under
`/FORCE`, so exit 0 is not proof).

## 2. Data — the part that decides whether this is a fair test

Both clients must read the **same TRE stack**: `D:\Code\SWGSource Client v3.0\`.

Each client additionally needs **its own** loose-override corpus, because the two renderers use
incompatible shader-constant conventions (A transposes matrices; B must not, and relies on
`ENABLE_BACKWARDS_COMPATIBILITY` register placement):

- **A:** `searchPath_00_10 = D:/Code/swg-client-v2/stage/override` (94 tracked files — 8 pixel +
  9 vertex `//hlsl` programs, 72 textures, `planet_tatooine.pln`, merged `interior.iff`). Note the
  x64 cfg deliberately points at the Win32 `stage/override` — single source, not drift.
- **B:** his 239 fxc-verified conversions at `scripts/asm2hlsl/converted/{pixel_program,vertex_program}/`.
  That layout mirrors A's override tree, so the same searchPath mechanism applies — **verify the
  path resolves before trusting a capture**, since a silently unresolved override would show up as
  a render difference that is really a staging error.

**Do not cross-stage the two override corpora.** Each is authored against its own renderer's
convention; mixing them produces meaningless differences.

## 3. Scene matrix

Five scenes, each captured on A-gl11 and B-gl11, with A-gl05 and B-gl05 as visual/timing reference.

| # | Scene | What it discriminates |
| --- | --- | --- |
| 1 | **Mos Eisley starport, interior** | COLOR-interpolant clamp (B's C35 find — D3D9 hardware clamped, D3D10+ doesn't); B's 0.3/0.85 ambient floors; portal/cell binding. **Doubles as the conflict-3 repro** — walk the portal transitions and watch for see-through |
| 2 | **Theed exterior** | terrain blend passes, `setAlphaBlendFactors`, detail maps |
| 3 | **Character select** | skinned meshes + baked vertex colours. ⚠ **Confounded on B** — see §4 |
| 4 | **JTL space + nebula + additive UI** | the content B's renderer has plausibly never drawn: additive-over-straight-alpha UI premultiply, nebula skybox family, emissive |
| 5 | **Timed zone-in, end to end** | A's perf catalogue claims vs B's 45/90 s force-finish loading. Frame-time series, not a capture |

Per scene, collect: a screenshot from each of the four configurations, a RenderDoc capture from each
gl11, and (scene 5) a frame-time series.

## 4. Known confounds — control or annotate these, or the results mislead

1. **B rewrites skinned-mesh vertex colours**: packed ARGB == 0 → `0xffffffff` (opaque white),
   *every* skinned mesh. This poisons scene 3 on B's build and any character/NPC comparison
   elsewhere. Either annotate it or disable it for the run.
2. **B's ambient floors** (0.3 in his gl05 path, 0.85 diffuse in HLSL) brighten interiors by design —
   scene 1 differences are partly this, not renderer capability.
3. **B under-reads `ParametersMessage`** against a stock server (registers 1 variable where the base
   sends 2, and drops the base's `unpackedSize < packedSize` bound). Watch for it at connect; it is
   probably benign per-message but it is the wire-drift canary.
4. **Audio backends differ** (Miles vs JUCE). Irrelevant to captures; relevant to scene 5 timing —
   B's JUCE `AIL_open_stream` decodes whole files to RAM before playback.
5. **B's `DEFH` camera heuristic and ground clamp** may change camera framing; match viewpoints by
   coordinates, not by eye.
6. **B's force-finish loading (45/90 s)** can enter the world with an incomplete snapshot — scene 5
   must confirm the world is actually complete before stopping the timer.

## 5. Execution rules

- **Kenny launches every client.** Boot smoke from the agent shell is invalid — DirectInput mouse
  `CreateDevice` FATALs there, upstream of `SetupClientGame`.
- **Never `Stop-Process` a `SwgClient` the session did not verifiably start** — concurrent live
  sessions exist; identity-check `StartTime` first.
- Renderer selection is `rasterMajor` in the *matching* cfg: `SwgClient_r.exe` → `client.cfg`,
  `SwgClient_d.exe` → `client_d.cfg`. `11` = gl11, `5` = gl05. Confirm exe + cfg + `rasterMajor` +
  DLL all line up **before** declaring a capture valid.
- **Never write a `.cfg` with PowerShell `Set-Content`/`Out-File`** — the UTF-8 BOM crashes the
  Release client at boot. Verify with `head -c 8 <cfg> | xxd`: clean starts `23 20`.
- RenderDoc: F12 to capture. CLI at
  `D:\Code\renderdoc-mcp\v0.3.0\renderdoc-mcp-windows-x64-v0.3.0\bin\renderdoc-cli.exe`.

## 6. What the run must answer

1. **Does B's renderer render A's hardest content correctly?** (scene 4 above all — if it has never
   drawn space/nebula/additive UI, that is where it breaks.)
2. **Does A's renderer carry C35-class latent parity debt?** (scene 1 — unclamped COLOR interpolants
   blowing interiors toward white.)
3. **Whose performance story survives contact?** (scene 5, plus frame times in 1–4.)

Anything else the matrix reveals is a bonus; these three are the decision inputs. If scene 4 shows
B failing on content A renders correctly, and scene 1 shows A failing on a rule B implements
correctly, the answer is "neither renderer wins outright" — which is a legitimate and likely result,
and would argue for choosing the base on other grounds and porting the loser's fixes forward.
