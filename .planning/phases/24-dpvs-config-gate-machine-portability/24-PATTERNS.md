# Phase 24: DPVS Config-Gate + Machine Portability - Pattern Map

**Mapped:** 2026-06-12
**Files analyzed:** 9 (4 modify-engine, 1 modify-build, 1 modify-git, 3 new-tooling/data)
**Analogs found:** 8 / 9 (the PowerShell setup script has a partial analog only)

> This is an in-repo engineering + tooling phase, not feature construction. RESEARCH.md already pins every analog to exact file:line. This map confirms those analogs against the live tree and packages concrete excerpts the planner can copy directly. Almost everything is wiring/cleanup against existing patterns.

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|-------------------|------|-----------|----------------|---------------|
| `clientGraphics/src/shared/RenderWorld.cpp` (modify, ~908) | engine render-decision | request-response (per-frame) | self — the existing `_DEBUG` A/B branch at :908-916 IS the pattern | exact (in-file) |
| `clientGraphics/src/shared/ConfigClientGraphics.cpp` (modify) | config getter/cache | transform (read-once at install) | `getPsrcCensus` (:128 install, :348 getter) + `[ClientGraphics/Dpvs]` read in `DpvsProfileInstrumentation.cpp:85,90` | exact |
| `clientGraphics/include/public/clientGraphics/ConfigClientGraphics.h` (modify) | config decl | — | existing `static <T> get*()` getter rows (:36-55) | exact |
| `clientAudio/src/win32/Audio.cpp` (modify, ~1276) | engine audio-init | event-driven (one-shot startup probe) | the existing `AIL_open_digital_driver` failure `WARNING` block at :1292-1307 | role-match (same fn, different trigger) |
| `SwgClient/build/win32/SwgClient.vcxproj` (modify, :130 + :233) | build postbuild | file-I/O (stage copy) | self — the existing Miles `xcopy` block (Debug :130, Release :233); only the SOURCE path changes | exact (in-file) |
| `src/external/3rd/library/miles-7.2e/redist/` (data: +2 `.m3d`, reconcile bytes) | vendored binary redist | file-I/O (tracked asset) | the proven `stage/miles/` byte-set (Oct-2017) | data-reconcile (A1) |
| `tools/setup/setup-client.ps1` (NEW) | build tooling | transform (template→cfg) + file-I/O validate | `tools/d3d11-smoke/show-window.ps1` (param/CmdletBinding form); `stage/build-14-03-gate.ps1` (gate form) | role-match (no cfg-gen precedent) |
| `tools/setup/client*.cfg.template` (NEW) | tracked config template | transform source | current `stage/client_d.cfg` / `stage/client.cfg` structure | exact (templatize existing) |
| `.gitignore` (modify) | git config | — | existing `stage/*` + `!stage/override/` negation (:92-93) | exact |

## Pattern Assignments

### `ConfigClientGraphics.cpp` (config getter/cache, transform) — HARD-01

**Analog:** self, `getPsrcCensus`/`getDpvsImageScale` — the established "read once at install, expose via getter" convention for the `[ClientGraphics]` cache.

**Install-time read pattern** (`ConfigClientGraphics.cpp:70-72, 128` — the `KEY_*` macros + the closest precedent key):
```cpp
#define KEY_BOOL(a,b)    (ms_ ## a = ConfigFile::getKeyBool("ClientGraphics", #a, b))
// ...inside install():
// Phase 17 (Plan 17-01): default false so D3D9 behavior is unchanged when off.
KEY_BOOL(psrcCensus,                          false);
```
> NOTE: the existing `KEY_*` macros read the `[ClientGraphics]` section. The new `occlusionMode` key lives in `[ClientGraphics/Dpvs]` (Claude's Discretion in CONTEXT confirms this section as the natural home). It is a tri-state STRING, not a bool — so do NOT use `KEY_BOOL`; read it directly with `ConfigFile::getKeyString("ClientGraphics/Dpvs", "occlusionMode", "off")` and parse to the enum (mirror the `DpvsProfileInstrumentation.cpp:85` read, see Shared Pattern A). D-02: default `"off"`.

**Getter pattern to mirror** (`ConfigClientGraphics.cpp:278-281` and `:348-351`):
```cpp
float ConfigClientGraphics::getDpvsImageScale()
{
    return ms_dpvsImageScale;
}
// ----------------------------------------------------------------------
bool ConfigClientGraphics::getPsrcCensus()
{
    return ms_psrcCensus;
}
```
New: add a `DpvsOcclusionMode` enum + `ms_dpvsOcclusionMode` to the namespace (alongside `ms_psrcCensus` at `:63`), parse once in `install()`, return it from `getDpvsOcclusionMode()`.

**Header decl pattern to mirror** (`src/shared/ConfigClientGraphics.h:51`):
```cpp
static bool           getDisableMultiStreamVertexBuffers();
```
Add `static DpvsOcclusionMode getDpvsOcclusionMode();` in the same `static <T> get*()` block. (Note: there are TWO ConfigClientGraphics.h — `src/shared/` has the full surface; the `include/public/` one did not match the get* grep, so add the decl + enum to the `src/shared/ConfigClientGraphics.h` the .cpp includes.)

**D-07 default flips (allowed, in this same install() block):**
- `disableMultiStreamVertexBuffers`: flip `ConfigClientGraphics.cpp:104` from `KEY_BOOL(disableMultiStreamVertexBuffers, true)` → `false` so the cfg override key can disappear. One-line, reversible, boot-verify both renderers (D3D11 skinned DOT3 depends on multi-stream).

---

### `RenderWorld.cpp` (engine render-decision, request-response) — HARD-01

**Analog:** self — the gate site at `:908-916` ALREADY has both branches written. This is a surgical replacement of the `#else` (Option α) branch with the config switch, preserving the `_DEBUG` F11 semantics.

**Current gate site** (`RenderWorld.cpp:903-916` — verified live):
```cpp
#ifdef _DEBUG
    // Phase 23 -- DPVS D3D11 remeasure: re-introduce the OCCLUSION_CULLING bit
    // for the A/B toggle, gated on the surviving ms_forceDisableOcclusionCulling
    // DebugFlag (F11). Kept inside _DEBUG so the shipped Option α #else branch
    // is byte-for-byte unchanged. (Phase 10 D-13 dropped this bit globally.)
    uint const cullingParameters =
          (ms_forceDisableOcclusionCulling ? 0 : DPVS::Camera::OCCLUSION_CULLING)
        | (ms_disableViewFrustumCulling    ? 0 : DPVS::Camera::VIEWFRUSTUM_CULLING);
    portalRecusionDepth = ms_disablePortalTraversal ? 0 : 8;
#else
    // Phase 10 D-13: OCCLUSION_CULLING bit dropped (Option α).
    uint const cullingParameters = DPVS::Camera::VIEWFRUSTUM_CULLING;
    portalRecusionDepth = 8;
#endif
```

**Cell context available at the gate** (`RenderWorld.cpp:873` — `ms_cameraCell` is set THIS frame before the gate, valid for the `auto` read):
```cpp
ms_cameraCell = camera.getParentCell();
ms_dpvsCameraCell = ms_cameraCell->getDpvsCell();
```
> Caveat (verified at :869-871): under the `_DEBUG ms_lockViewFrustum` path `ms_cameraCell` is NOT reassigned that frame; it retains its last value — acceptable for a per-frame culling decision.

**Change-detection (do NOT touch — it already debounces `setParameters`)** (`RenderWorld.cpp:920-930`):
```cpp
if (  ms_cameraViewportWidth != ms_lastCameraViewportWidth
    || ms_cameraViewportHeight!= ms_lastCameraViewportHeight
    || cullingParameters != ms_lastCullingParameters )
{
    ms_lastCullingParameters    = cullingParameters;
    float const imageScale = ConfigClientGraphics::getDpvsImageScale();
    ms_dpvsCamera->setParameters(ms_cameraViewportWidth, ms_cameraViewportHeight, cullingParameters, imageScale, imageScale);
}
```

**Target shape (D-01 + D-04, from RESEARCH Pattern 1):** compute the occlusion bit from the config enum (off / on / auto→`ms_cameraCell->isWorldCell()`), then in `_DEBUG` let `ms_forceDisableOcclusionCulling` clear it unconditionally (F11 wins). The result feeds the SAME `cullingParameters` the change-detect at :920 already consumes — no new plumbing. The config switch becomes the shipped (`#else`/Release-visible) behavior; the `_DEBUG` F11 override layers on top.

---

### `Audio.cpp` (engine audio-init, event-driven one-shot) — PORT-01 / D-12

**Analog:** the existing one-shot `WARNING` block in the SAME function (`Audio::install`), `:1292-1307`.

**Existing one-shot warning pattern to mirror** (`Audio.cpp:1292-1307`):
```cpp
s_digitalDevice2d = AIL_open_digital_driver(getFrequency(), getBits(), getProviderSpec(getCurrent3dProvider()), 0);
if (!s_digitalDevice2d)
{
    WARNING(true, ("Audio:  Digital Audio - Attempting to use a generic stereo seting, user option failed. Miles Error (%s)", AIL_last_error()));
    s_digitalDevice2d = AIL_open_digital_driver(getFrequency(), getBits(), MSS_MC_STEREO, 0);
    s_soundProvider = "2 Speakers";
    if (!s_digitalDevice2d)
    {
        WARNING(true, ("Audio: Digital Audio - Shutting down the audio system. Miles Error (%s)", AIL_last_error()));
        remove();
        setEnabled(false);
        return false;
    }
}
```

**Redist-dir set context** (`Audio.cpp:1276-1280` — the hook site for D-12):
```cpp
std::string redistDirectory(AIL_set_redist_directory("miles"));
AIL_startup();
```

**D-12 design (RESEARCH Pitfall 4 + A2):** The existing `:1292` failure path does NOT fire on the half-dead case — the digital driver opens via system DirectSound; only codec DECODE fails later, per-sample, producing the 141k-line flood at `Audio.cpp:1525` ("Error loading and allocating the sample"). So D-12 needs an ADDITIONAL explicit probe right after `AIL_set_redist_directory("miles")` / `AIL_startup()` (:1276-1280): check the known codec files (`.asi` + `.m3d`) exist on disk in the redist dir, emit ONE `WARNING(true, (...))` if absent. Use the `WARNING(true, (...))` macro form exactly as the :1292 block does. Do NOT hook at the per-sample site (`:2823`/`:4436`/`:1525`) — that reproduces the flood.

---

### `SwgClient.vcxproj` (build postbuild, file-I/O) — PORT-01 / D-11

**Analog:** self — the Miles `xcopy` block, present in BOTH config blocks. Only the SOURCE path changes (additive; do NOT restructure Koogie's logic — MEMORY "don't modify Koogie's source-level changes").

**Current block, Debug** (`SwgClient.vcxproj:123-130`):
```xml
<PostBuildEvent>
  <Message>Stage Debug exe + pdb</Message>
  <Command>copy /Y "$(OutDir)$(ProjectName)_d.exe" "..\..\..\..\..\..\..\stage\"
copy /Y "$(OutDir)$(ProjectName)_d.pdb" "..\..\..\..\..\..\..\stage\"
rem 2026-06-12: ensure the Miles codec redist exists in stage (mssmp3.asi mp3 decoder + .m3d/.flt 3D providers).
rem Missing miles/ = silent music + in-world-audio loss AND a warning-flood perf drag (UI 2D wavs still play).
rem No-op when present; never fails the build when the source dir is absent on this machine.
if not exist "..\..\..\..\..\..\..\stage\miles\mssmp3.asi" if exist "D:\Code\SWGSource Client v3.0\miles\mssmp3.asi" xcopy /E /I /Y "D:\Code\SWGSource Client v3.0\miles" "..\..\..\..\..\..\..\stage\miles\"</Command>
</PostBuildEvent>
```
**Release block** is byte-identical except `_d`→`_r` and the `Stage Release exe + pdb` message (`:226-233`). BOTH blocks need the same source repoint.

**The change (RESEARCH Pattern 2):** swap the machine-specific `D:\Code\SWGSource Client v3.0\miles` source for the repo-relative vendored redist:
```bat
rem PORT-01: repo-relative vendored redist (no machine-specific path):
if not exist "..\..\..\..\..\..\..\stage\miles\mssmp3.asi" xcopy /E /I /Y "..\..\..\..\..\..\..\src\external\3rd\library\miles-7.2e\redist" "..\..\..\..\..\..\..\stage\miles\"
```
Keep the `if not exist` guard structure and the exe/pdb `copy` lines untouched.

---

### `redist/` reconciliation (vendored binary data) — PORT-01 / D-11 / A1

**Analog:** the proven `stage/miles/` Oct-2017 byte-set (runtime-verified by the 2026-06-12 audio fix).

**VERIFIED A1 divergence (live `ls` this session):**
| File | redist/ (May-2025) | stage/miles/ (Oct-2017, verified) | Action |
|------|--------------------|-----------------------------------|--------|
| `mssogg.asi` | 41,472 | 99,840 | redist is a DIFFERENT point-release → overwrite with stage bytes |
| `mssmp3.asi` | 95,744 | 94,208 | differ → use stage bytes |
| `msseax.flt` | 59,904 | 59,392 | differ → use stage bytes |
| `mssdsp.flt` | 57,856 | 56,832 | differ → use stage bytes |
| `mssds3d.flt` | 13,312 | 12,800 | differ → use stage bytes |
| `Msseax.m3d` | **ABSENT** | 143,872 | ADD to redist (`git add`) |
| `msssoft.m3d` | **ABSENT** | 79,360 | ADD to redist (`git add`) |
| `mssvoice.asi` | 153,600 | 153,600 | identical |
| `mss32.dll` | 444,416 | (not in stage/miles; in redist) | keep redist |
| `AudioCapture.flt` | absent | 65,536 | EXCLUDE (subsystem removed v2.1; 0 refs in src/) |
| `index.html` | absent | 0 bytes | EXCLUDE |

**Recommendation (RESEARCH A1 / Open Q1):** make redist = the verified `stage/miles` bytes for the shared `.asi/.flt`, keep redist's `mss32.dll`, ADD the 2 `.m3d` files, then boot-verify audio after the postbuild stages the new redist. Do NOT trust the existing redist vintage blind. `git add` the new/changed binaries (~240 KB, provenance = verified SWGSource set).

---

### `tools/setup/setup-client.ps1` (NEW, build tooling) — PORT-01 / D-08 / D-12a

**Closest analog:** `tools/d3d11-smoke/show-window.ps1` — the established repo PowerShell form: `[CmdletBinding()]` + `param(...)` block, `Get-Process`/`Write-Host` flow, versioned-type comment discipline. There is NO existing cfg-generation script (Wave 0 gap), so this is the structural analog only.

**Param/CmdletBinding header pattern to mirror** (`show-window.ps1:18-27`):
```powershell
[CmdletBinding()]
param(
    [string]$ProcessName = 'SwgClient_d',
    [int]$X = 100,
    ...
    [switch]$List
)
```
For setup: `param([string]$TreRoot, [int]$RasterMajor = 11, [string]$Resolution = '1920x1080', [switch]$NoPrompt)` (UX is Claude's Discretion per CONTEXT/D-08; RESEARCH Open Q2 recommends one parameterized template emitting both cfgs).

**Validation flow to mirror** (`show-window.ps1:52-56` — the Test/early-exit idiom):
```powershell
$proc = Get-Process -Name $ProcessName -ErrorAction SilentlyContinue
if (-not $proc) {
    Write-Host "..." -ForegroundColor Yellow
    exit 1
}
```
For setup (D-12a file-presence gate + V5 input validation): `Test-Path -PathType Container $TreRoot` before substitution; validate the expected `stage/miles` codec set (`.asi`/`.m3d`) exists; `Write-Host`/`exit N` on failure. Auto-substitute repo-relative `stage/override` from the script's repo root (no user input) per D-08.

**Secondary analog (gate/staging form):** `stage/build-14-03-gate.ps1` exists as another local PowerShell precedent for a build-adjacent gate script (form reference only; not read in depth).

---

### `tools/setup/client*.cfg.template` (NEW, tracked config template) — PORT-01 / D-09

**Analog:** the current `stage/client_d.cfg` + `stage/client.cfg` structure — the template IS those files with machine-specific paths replaced by placeholders, and the PORT-02-cleaned key set.

**Templatize these machine-specific paths (PORT-01 inventory, RESEARCH):**
| Path | client_d.cfg | client.cfg | Template handling |
|------|--------------|------------|-------------------|
| `TOCTreePath="D:/Code/SWGSource Client v3.0/"` | line 22 | line 18 | `$TreRoot` parameter (prompt); quote it (spaces) |
| `searchTOC_*` / `searchTree_*` | 23-28 | 19-24 | same `$TreRoot` prefix |
| `searchPath_*=...stage/override` | 31 | 27 | auto-substitute from repo root (D-08); preserve priority above TREs |
| `rasterMajor` | 41 | 37 | `$RasterMajor` parameter (D-06 canonical 11) |
| `searchPath_00_12=D:/swg_dev_bundle` | 29 | absent | OMIT (PORT-02 V→R; A3) |

Carry forward the still-true Phase-6 decision annotations (D-01..D-09) per CONTEXT specifics; strip the stale "Generated by CMake … re-run cmake" header (false since Phase 9) and replace with setup-script provenance; strip the Phase-11/17/18 inline essays (RESEARCH PORT-02 audit: "K value, R comment" rows).

---

### `.gitignore` (modify) — D-09

**Analog:** self — the existing `stage/*` + `!stage/override/` negation.

**Existing arrangement** (`.gitignore:92-93`, verified live):
```
stage/*
!stage/override/
```
**The actual work (RESEARCH Pitfall 2):** the cfgs are ALREADY untracked (matched by `stage/*`). Do NOT plan `git rm --cached stage/client_d.cfg` — it's a no-op. The real work: (a) place the template dir OUTSIDE `stage/*` (`tools/setup/` — tracked by default, no negation needed), (b) confirm generated cfgs stay ignored, (c) `git add` the 2 new `.m3d` files under `src/external/.../redist/` (that path is already tracked). No `stage/`-internal negation for the template — putting it under `stage/` would fight the `stage/*` intent.

## Shared Patterns

### Shared Pattern A — `[ClientGraphics/Dpvs]` config read (HARD-01)
**Source:** `DpvsProfileInstrumentation.cpp:85, 90` — verified, three lines from the new key's home.
**Apply to:** the `occlusionMode` parse in `ConfigClientGraphics::install`.
```cpp
char const * const csvDir = ConfigFile::getKeyString("Dpvs/Experiment", "csvDir", "dpvs-profile/");
ms_reportOverlay = ConfigFile::getKeyBool("ClientGraphics/Dpvs", "reportInstrumentation", true);
```
The string read (`getKeyString` with a string default) is the exact precedent for the tri-state `occlusionMode` — parse the returned `char const*` to the enum with `_stricmp` against `"auto"`/`"on"`, defaulting `"off"` (D-02).

### Shared Pattern B — POB-cell predicate (HARD-01 `auto` mode)
**Source:** `CellProperty.cpp:555` (`isWorldCell()`), consumed via `RenderWorld`'s already-tracked `ms_cameraCell` (set at `RenderWorld.cpp:873`).
**Apply to:** the `auto` branch of the RenderWorld occlusion-bit switch.
```cpp
// Outdoor  → ms_cameraCell->isWorldCell() == true  → occlusion ON in auto
// POB cell → ms_cameraCell->isWorldCell() == false → occlusion OFF in auto
bool CellProperty::isWorldCell() const { return this == ms_worldCellProperty; }
```
Do NOT hand-roll an "am I in a building" predicate — this one-liner is canonical and correct.

### Shared Pattern C — one-shot startup `WARNING` (D-12)
**Source:** `Audio.cpp:1292-1307` (the existing single-fire driver-failure warning).
**Apply to:** the new Miles codec-absence probe in `Audio::install` (after :1280).
```cpp
WARNING(true, ("Audio: <message> Miles Error (%s)", AIL_last_error()));
```
ONE `WARNING(true, ...)` at install, not per-sample. This is the anti-flood discipline the phase exists to enforce.

### Shared Pattern D — postbuild stage-copy guard (PORT-01)
**Source:** `SwgClient.vcxproj:130` / `:233` — `if not exist <stage marker> [if exist <source>] xcopy /E /I /Y <source> <stage>`.
**Apply to:** both Debug and Release postbuild blocks (repoint source only). Additive; preserve Koogie's exe/pdb copy + guard structure.

## No Analog Found

| File | Role | Data Flow | Reason |
|------|------|-----------|--------|
| `tools/setup/setup-client.ps1` | build tooling | transform (cfg generation) | No prior cfg-generation script in the tree (the dead CMake `configure_file` story is the conceptual predecessor, removed at Phase 9). Structural analog only: `tools/d3d11-smoke/show-window.ps1` (param/CmdletBinding/validation form) + `stage/build-14-03-gate.ps1` (gate form). The cfg-emit + path-substitution + miles-validation LOGIC is net-new; planner should follow RESEARCH.md's Pattern + the PowerShell idioms cited above. |

## Metadata

**Analog search scope:**
- `src/engine/client/library/clientGraphics/src/shared/` (RenderWorld, ConfigClientGraphics, DpvsProfileInstrumentation + header)
- `src/engine/client/library/clientAudio/src/win32/Audio.cpp`
- `src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj`
- `src/external/3rd/library/miles-7.2e/redist/` + `stage/miles/` (byte compare)
- `tools/**/*.ps1`, `stage/*.ps1` (PowerShell precedents)
- `.gitignore`

**Files scanned (read or grepped):** 9 source/build/config + 2 directory listings + 4 PowerShell glob hits.

**Verification notes:**
- Every RESEARCH.md file:line analog was confirmed against the live tree this session (gate site :903-916, ms_cameraCell :873, change-detect :920-930, ConfigClientGraphics getter/install :70-128/:278/:348, Audio one-shot :1276-1307, vcxproj postbuild :123-130/:226-233, gitignore :92-93).
- A1 (redist ≠ stage/miles bytes) CONFIRMED by live `ls`: `mssogg.asi` 41,472 (redist) vs 99,840 (stage); both `.m3d` absent from redist.

**Pattern extraction date:** 2026-06-12
