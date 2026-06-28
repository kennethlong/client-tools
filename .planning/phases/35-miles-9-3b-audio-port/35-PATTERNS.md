# Phase 35: Miles 9.3b Audio Port - Pattern Map

**Mapped:** 2026-06-18
**Files analyzed:** 6 (1 new vendoring tree, 2 vcxproj, 2 source, + grep/gate surfaces)
**Analogs found:** 6 / 6 (all in-repo; this is a modify-existing + mirror-existing-tree phase, no greenfield patterns)

> This phase is mechanical: mirror the `miles-7.2e` vendoring tree, repoint per-platform vcxproj
> lib/include/redist references, mirror the existing PostBuild redist-staging command, and apply
> in-place edits to `Audio.cpp` / `SoundObject3d.cpp`. Every "analog" is an existing file in this
> repo. Excerpts below are the EXACT current text to mirror/edit — line numbers verified this session.

## File Classification

| New/Modified File | Role | Data Flow (change kind) | Closest Analog | Match Quality |
|-------------------|------|-------------------------|----------------|---------------|
| `src/external/3rd/library/miles-9.3b/` (NEW tree) | config / vendored-dep | file-I/O (copy-in) | `src/external/3rd/library/miles-7.2e/` | exact |
| `src/engine/.../clientAudio/build/win32/clientAudio.vcxproj` | build (static lib) | transform (include repoint) | itself (current `miles\include` refs, 5 configs) | exact (self-edit) |
| `src/game/.../SwgClient/build/win32/SwgClient.vcxproj` (link blocks) | build (final link) | transform (lib repoint + add x64 lib) | itself (current Win32 link block as the x64 template) | exact (self-edit) |
| `src/game/.../SwgClient/build/win32/SwgClient.vcxproj` (PostBuild) | build (staging) | file-I/O (xcopy) | the existing Win32 PostBuild redist block (165-181) | exact (self-edit) |
| `src/engine/.../clientAudio/src/win32/Audio.cpp` | service (engine subsystem) | event-driven (API call sites) | in-place edit (room_type/probe/stub) | self-edit |
| `src/engine/.../clientAudio/src/win32/SoundObject3d.cpp` | service (engine subsystem) | event-driven | in-place edit (gate removal) | self-edit |

---

## Pattern Assignments

### 1. NEW vendoring tree `src/external/3rd/library/miles-9.3b/` (config, file-I/O)

**Analog:** `src/external/3rd/library/miles-7.2e/` (verified layout this session)

**Directory layout to mirror** (the 7.2e tree as it exists today):
```
src/external/3rd/library/miles-7.2e/
├── include/mss.h                 (239,823 B — single header)
├── lib/win/Mss32.lib             (88,562 B — Win32 import lib, FORCE-ADDED past *.lib gitignore)
├── redist/                       (mss32.dll, mssmp3.asi, mssogg.asi, mssdsp.flt, msseax.flt,
│                                   mssds3d.flt, mssdolby.flt, msssrs.flt, mssvoice.asi,
│                                   Msseax.m3d, msssoft.m3d)   <-- note the TWO .m3d (do NOT carry to 9.3b)
└── msswin32-72.EXE               (installer blob — do NOT replicate)
```

**Target layout for `miles-9.3b/`** (Claude's-discretion D-decision: mirror the 7.2e convention, NOT
the SDK's native `win/sdk/...` tree). Source SDK is at `D:/Code/milesss-v9.3b/`:
```
src/external/3rd/library/miles-9.3b/
├── include/mss.h                 <- D:/Code/milesss-v9.3b/win/sdk/include/mss.h
├── include/rrcore.h              <- D:/Code/milesss-v9.3b/win/sdk/include/rrcore.h  (mss.h #includes "rrCore.h"; Pitfall 4)
├── lib/win/mss32.lib             <- D:/Code/milesss-v9.3b/win/sdk/lib/mss32.lib  (100,882 B)
├── lib/win/mss64.lib             <- D:/Code/milesss-v9.3b/win/sdk/lib/mss64.lib  (94,246 B)
├── redist/    (Win32 set)        <- gathered from win/sdk/redist + win/mp3/redist + win/ogg/redist
└── redist64/  (x64 set)          <- gathered from win/sdk/redist64 + win/mp3/redist64 + win/ogg/redist64
```

**CRITICAL — gitignore (`.gitignore:14` is a bare `*.lib`):** the two vendored `.lib` files are
silently skipped by plain `git add`. After copying, run:
```
git add -f src/external/3rd/library/miles-9.3b/lib/win/mss32.lib src/external/3rd/library/miles-9.3b/lib/win/mss64.lib
```
Confirm with `git ls-files`. (This is exactly why 7.2e's `Mss32.lib` is tracked — it was force-added.)
The `.dll/.asi/.flt` redist bytes are NOT `*.lib`, so they track normally — but verify the in-repo
`redist/` + `redist64/` source bytes ARE committed (they feed the PostBuild copy; a fresh clone needs them).

**9.3b redist sources verified present** (`D:/Code/milesss-v9.3b/win/sdk/redist64/`):
`binkawin64.asi  mss64.dll  mss64dolby.flt  mss64ds3d.flt  mss64dsp.flt  mss64eax.flt  mss64srs.flt`
(+ mp3/ogg `.asi` live under `win/mp3/redist64` and `win/ogg/redist64` — gather from all three, per D-05).
**Confirmed absent:** no `*.m3d` anywhere in 9.3b — do NOT copy/probe/stage the `.m3d` providers.

---

### 2. clientAudio.vcxproj — include-path repoint (build, static lib)

**Analog:** the file itself. The include path currently points at the **unversioned** `miles\include`
(NOT `miles-7.2e`). It appears in **5 `<AdditionalIncludeDirectories>` lines, one per config**:
Debug|Win32 (109), Optimized|Win32 (147), Release|Win32 (184), Debug|x64 (217), Release|x64 (236).

**Current text (the substring to find/replace in all 5 lines):**
```
..\..\..\..\..\..\external\3rd\library\miles\include
```
**Repoint to:**
```
..\..\..\..\..\..\external\3rd\library\miles-9.3b\include
```
Each line is a long `;`-delimited list — replace only the `miles\include` token, leave the rest intact.
The `#include <mss.h>` in `src/shared/FirstClientAudio.h:18` is resolved via this include path and needs
NO change. (`rrcore.h` is pulled in by `mss.h` and must sit in the same vendored `include/` dir.)

**Toolset facts (already correct, do not touch):** all 5 configs are `PlatformToolset=v145`,
`LanguageStandard=stdcpp20`, `ConfigurationType=StaticLibrary`. Win32 Debug has `TreatWarningAsError=true`
(122-123) but the x64 configs do NOT set it (default false) — a 2012-era header warning won't fail the
x64 build (RESEARCH A4), but it COULD fail Win32 Debug. If the 9.3b header trips a W4 warning on Win32
Debug, that surfaces immediately at first compile.

---

### 3. SwgClient.vcxproj — per-platform lib repoint + ADD mss64 (build, final link)

**Analog:** the current Win32 Debug link block (145-152) is the working template; the x64 Debug block
(322-333) is the one missing the Miles lib. Both Win32 (Debug 145 / Release ~212 / Optimized ~258) and
x64 (Debug 322 / Release ~381) link blocks need edits.

**3a. Win32 `<AdditionalDependencies>` (line 145)** — currently lists Miles TWICE (harmless dup),
`Mss32.lib` (capital, from unversioned `miles\lib\win`) AND `mss32.lib` (lowercase, from `miles-7.2e`):
```
...;Mss32.lib;unicows.lib;...;libxml2-win32-release.lib;mss32.lib;mswsock.lib;legacy_stdio_definitions.lib;%(AdditionalDependencies)
```
→ The 9.3b SDK's import lib is named `mss32.lib` (lowercase). Keep `mss32.lib`; the dup `Mss32.lib`
can be removed or left (it resolves to the same name on case-insensitive Windows once the lib DIR points
at 9.3b). The **lib name does not change** — only the lib DIRECTORY it resolves from (3c below).

**3b. x64 `<AdditionalDependencies>` (line 322)** — currently has **NO Miles lib** (Phase 33 dropped it;
AIL_* symbol count went to zero). Tail of the current x64 list:
```
...;ws2_32.lib;winmm.lib;mswsock.lib;legacy_stdio_definitions.lib;libxml2-x64.lib;pcre-x64.lib;jpeg62-x64.lib;%(AdditionalDependencies)
```
→ **ADD `mss64.lib`** to this list (both x64 Debug 322 and x64 Release ~381). This is the edit that
re-arms the x64 link against the now-live AIL_* externals.

**3c. `<AdditionalLibraryDirectories>`** — Win32 (147) and x64 (324) BOTH currently list two Miles dirs:
```
...;..\..\..\..\..\..\..\src\external\3rd\library\miles\lib\win;..\..\..\..\..\..\..\src\external\3rd\library\miles-7.2e\lib\win;...
```
→ Repoint to the single 9.3b dir (`...\src\external\3rd\library\miles-9.3b\lib\win`). Replace BOTH old
tokens in BOTH Win32 and x64 lib-dir lines (and the Release configs). The same `miles-9.3b\lib\win`
dir holds `mss32.lib` and `mss64.lib`; the linker picks by platform via the lib NAME in 3a/3b.

**x64 link masking (DO NOT mistake exit 0 for a clean link):** the x64 Debug block has
`/SAFESEH:NO /VERBOSE` (329) + `<ForceFileOutput>Enabled</ForceFileOutput>` (332) +
`<TargetMachine>MachineX64</TargetMachine>` (333). `/FORCE` downgrades `unresolved external symbol`
(LNK2019/2001) to a warning and STILL emits an exe. **Gate = grep the build log for
`unresolved external symbol` (must be 0).** `LNK1181 cannot open input file 'mss64.lib'` is a hard
error and means the force-add / lib-dir repoint failed.

**Editor vcxprojs** (AnimationEditor/SoundEditor/ParticleEditor/etc.) also reference `miles\lib\win` +
`mss32.lib` — they are pre-broken on Qt (not in the validation bar). Repointing them is Claude's
discretion / deferred-cleanup, not required for the gate.

---

### 4. SwgClient.vcxproj — PostBuild redist staging (build, file-I/O)

**Analog (Win32, the proven Phase-24 PORT-01 model):** the existing Win32 Debug PostBuildEvent
(165-181). It copies the **in-repo tracked** redist to `stage/miles/`, sentinel-guarded. Exact current
command body (lines 167-180):
```bat
copy /Y "$(OutDir)$(ProjectName)_d.exe" "..\..\..\..\..\..\..\stage\"
copy /Y "$(OutDir)$(ProjectName)_d.pdb" "..\..\..\..\..\..\..\stage\"
rem ... (Phase-24 rationale comments) ...
if not exist "..\..\..\..\..\..\..\stage\miles\mssmp3.asi"  xcopy /E /I /Y "..\..\..\..\..\..\..\src\external\3rd\library\miles-7.2e\redist" "..\..\..\..\..\..\..\stage\miles\"
if not exist "..\..\..\..\..\..\..\stage\miles\Msseax.m3d"  xcopy /E /I /Y "..\..\..\..\..\..\..\src\external\3rd\library\miles-7.2e\redist" "..\..\..\..\..\..\..\stage\miles\"
if not exist "..\..\..\..\..\..\..\stage\miles\msssoft.m3d" xcopy /E /I /Y "..\..\..\..\..\..\..\src\external\3rd\library\miles-7.2e\redist" "..\..\..\..\..\..\..\stage\miles\"
```
(The Win32 **Release** PostBuild at 276-292 is the same body with `_r` — mirror both.)

**Win32 edit:** repoint source `miles-7.2e\redist` → `miles-9.3b\redist`. **Change the sentinel
filenames** — the `.m3d` guards (`Msseax.m3d`, `msssoft.m3d`) are 7.2e-only and will NEVER exist on a
9.3b stage, so they'd force a copy every build. Use 9.3b-only sentinels, e.g.:
```bat
if not exist "...\stage\miles\mssds3d.flt" xcopy /E /I /Y "...\src\external\3rd\library\miles-9.3b\redist" "...\stage\miles\"
if not exist "...\stage\miles\msseax.flt"  xcopy /E /I /Y "...\src\external\3rd\library\miles-9.3b\redist" "...\stage\miles\"
```
Picking a 9.3b-only sentinel also auto-REPAIRS a stale 7.2e stage (Pitfall 5): a `stage/miles` that
still has `mssmp3.asi` from 7.2e would be skipped by the old guard but is refreshed by a 9.3b-only one.

**Analog (x64, the gap to fill):** the x64 Debug PostBuild (343-348) currently stages **only exe+pdb**,
NO miles:
```bat
if not exist "..\..\..\..\..\..\..\stage-x64\" mkdir "..\..\..\..\..\..\..\stage-x64\"
copy /Y "$(OutDir)$(ProjectName)_d.exe" "..\..\..\..\..\..\..\stage-x64\"
copy /Y "$(OutDir)$(ProjectName)_d.pdb" "..\..\..\..\..\..\..\stage-x64\"
```
(x64 Release at 398-403 is the same with `_r`.)

**x64 edit:** ADD a miles-staging block mirroring the Win32 model, sourcing the **x64** redist set into
`stage-x64\miles\`:
```bat
if not exist "..\..\..\..\..\..\..\stage-x64\miles\mss64ds3d.flt" xcopy /E /I /Y "..\..\..\..\..\..\..\src\external\3rd\library\miles-9.3b\redist64" "..\..\..\..\..\..\..\stage-x64\miles\"
if not exist "..\..\..\..\..\..\..\stage-x64\miles\mss64eax.flt"  xcopy /E /I /Y "..\..\..\..\..\..\..\src\external\3rd\library\miles-9.3b\redist64" "..\..\..\..\..\..\..\stage-x64\miles\"
```
(Path-depth `..\..\..\..\..\..\..\` is 7 levels — identical to the existing copy lines in the same
block, so reuse that exact prefix.)

---

### 5. Audio.cpp — in-place edits (service, API call sites)

All line numbers verified against the LIVE file this session. Show the surrounding shape; these are
edits, not a separate analog.

**5a. DELETE the Phase-33 macro-stub block (250-310).** Current shape (head + tail):
```cpp
#if defined(_M_X64)                                  // line 250
#  ifdef AIL_MSS_version
#    undef AIL_MSS_version
#  endif
#  define AIL_MSS_version(str,len)   ((void)0)
   ... ~60 AIL_* no-op #defines ... incl.
#  define AIL_room_type(dig)         (0)             // line 268  <- becomes the real 2-arg fn
#  define AIL_set_room_type(dig,rt)  ((void)0)       // line 287  <- becomes the real 3-arg fn
#  define AIL_set_named_sample_file3D(...) ((void)0) // line 290  <- never called in live code; just drops
#endif // _M_X64 (Phase 33: Miles disabled)          // line 310
```
Delete the whole `#if ... #endif`. The real symbols come from `mss64.lib`. The `UINTa` file-callback
decls immediately below (318-321) STAY — they already match the 9.3b typedef byte-for-byte (no edit).

**5b. CONVERT the install() x64 early-return into the shared body (1306-1319 + matching #endif 1504).**
Current shape:
```cpp
bool Audio::install()
{
    DEBUG_FATAL(s_installed, ("Already installed"));

    s_disableMiles = ConfigFile::getKeyBool("ClientAudio", "disableMiles", false);   // 1298 -- KEEP (D-04)
    if (s_disableMiles) {                                                             // 1300 -- KEEP (D-04)
        REPORT_LOG(true, ("Audio: Miles is disabled. ... set \"disableMiles=false\" ..."));
        return false;
    }

#if defined(_M_X64)                          // 1306  <- DELETE this guard ...
    s_disableMiles = true;                   // 1315  <- ... and this whole x64 force-disable body
    setEnabled(false);                       // 1316
    REPORT_LOG(true, ("Audio: Miles disabled on x64 build (Phase 33)..."));
    return false;                            // 1318
#else                                        // 1319  <- DELETE the #else ...
    ... real install() body (registerFlag, AIL_startup, codec probe, callbacks) ...
#endif // !_M_X64 ...                         // 1504  <- ... and the closing #endif
}
```
Result: the `#if _M_X64 / x64-disable / #else / <real body> / #endif` collapses to just `<real body>`,
shared by both platforms (D-01/D-03). **KEEP lines 1298-1304** (the legacy `disableMiles` cfg
short-circuit — D-04; it is ABOVE and SEPARATE from the x64 block). After the edit, verify `install()`
reads cfg ONCE at 1298 and falls straight into the real body (the x64 block's `s_disableMiles = true`
reassignment is gone).

**5c. PORT the codec-presence probe (1399) `.m3d` → `.flt`, platform-aware.** Current line:
```cpp
static char const * const s_requiredMilesCodecs[] = { "mssmp3.asi", "mssogg.asi", "Msseax.m3d", "msssoft.m3d" };
```
The surrounding loop (1400-1418: `fopen` each name under `redistDirectory`, set
`s_milesCodecRedistAvailable=false` + one WARNING on any miss) is correct and STAYS. Replace ONLY the
filename array with a platform split (9.3b ships no `.m3d`; see RESEARCH Pitfall 1):
```cpp
#if defined(_M_X64)
    static char const * const s_requiredMilesCodecs[] =
        { "mss64.dll", "mss64mp3.asi", "mss64ogg.asi", "mss64ds3d.flt", "mss64eax.flt" };
#else
    static char const * const s_requiredMilesCodecs[] =
        { "mss32.dll", "mssmp3.asi", "mssogg.asi", "mssds3d.flt", "msseax.flt" };
#endif
```
NOTE: this `#if _M_X64` is the ONE intentional `_M_X64` that SURVIVES — the AUDIO-01 grep gate that
checks "Phase-33 `_M_X64` blocks removed" must EXCLUDE this codec-probe `#if`.

**5d. room_type bus_index edit (3919-3944 setter ×26; 3957 getter ×1).** Current setter shape:
```cpp
case RT_alley: { AIL_set_room_type(s_digitalDevice2d, ENVIRONMENT_ALLEY); } break;   // 3919
... 26 ENVIRONMENT_* cases total through line 3944 ...
```
9.3b setter signature is `AIL_set_room_type(HDIGDRIVER, S32 bus_index, S32 room_type)` — insert `0` as
the **MIDDLE** arg (NOT appended — Pitfall 2):
```cpp
case RT_alley: { AIL_set_room_type(s_digitalDevice2d, 0, ENVIRONMENT_ALLEY); } break;
```
Current getter (3957): `switch (AIL_room_type(s_digitalDevice2d))` → 9.3b getter is
`AIL_room_type(HDIGDRIVER, S32 bus_index)`, bus_index TRAILING:
```cpp
switch (AIL_room_type(s_digitalDevice2d, 0))
```
`bus_index = 0` = master mixer bus (RESEARCH A1, UAT #4 confirms). Grep gate:
`AIL_set_room_type(s_digitalDevice2d, 0,` count == 26; `AIL_room_type(s_digitalDevice2d, 0)` present.

---

### 6. SoundObject3d.cpp — remove the x64 listener gate (1 site, 32-36)

**Edit (delete the gate, keep the 3 calls):** current shape:
```cpp
void SoundObject3d::alter()
{
    m_positionPrevious = m_positionCurrent;

#if !defined(_M_X64)                                                    // 32  <- DELETE
    AIL_set_listener_3D_position(m_object, ...x,y,z);                   // 33
    AIL_set_listener_3D_velocity_vector(m_object, 0,0,0);               // 34
    AIL_set_listener_3D_orientation(m_object, ...fwd, ...up);           // 35
#endif // Phase 33 (X64-02): Miles AIL_* subsystem disabled on x64...   // 36  <- DELETE
}
```
Remove the `#if !defined(_M_X64)` (32) and the `#endif` (36); the three `AIL_set_listener_3D_*` calls
now run on both platforms (shared path, D-03). The calls themselves are unchanged (RESEARCH: identical
signatures in 7.2e and 9.3b).

---

## Shared Patterns

### Per-platform lib selection (Win32 vs x64) — the established mechanism
**Source:** `SwgClient.vcxproj` ItemDefinitionGroup `Condition="...|Win32"` vs `...|x64`, with
per-platform `<AdditionalDependencies>` + `<AdditionalLibraryDirectories>`.
**Apply to:** lib repoint (Section 3). The SAME 9.3b header compiles for both platforms (D-02); only the
lib NAME differs by platform (`mss32.lib` Win32 / `mss64.lib` x64), resolved from one shared
`miles-9.3b\lib\win` dir.

### In-repo-tracked-redist → sentinel-guarded PostBuild xcopy (Phase-24 PORT-01, commit c0ff0119a)
**Source:** `SwgClient.vcxproj` PostBuildEvent 165-181 (Win32 Debug) / 276-292 (Win32 Release).
**Apply to:** staging (Section 4) — Win32 repoint + x64 new block. The model: bytes live in-repo under
`miles-9.3b/redist{,64}/`, a fresh clone stages working audio with zero external dependency, and the
`if not exist <9.3b-only sentinel>` guard makes a stale 7.2e stage get REPAIRED rather than skipped.

### `*.lib` force-add (gitignore escape)
**Source:** `.gitignore:14` (`*.lib`); 7.2e `Mss32.lib` is tracked only because it was force-added.
**Apply to:** both 9.3b libs — `git add -f .../miles-9.3b/lib/win/mss32.lib mss64.lib`; verify with
`git ls-files`. Skipping this → `LNK1181 cannot open input file 'mss64.lib'` on a fresh checkout.

### `/FORCE`-masks-unresolved-externals (AGENTS.md) — the link gate
**Source:** `SwgClient.vcxproj` x64 Debug Link 329-332 (`/SAFESEH:NO /VERBOSE` + `ForceFileOutput`).
**Apply to:** every link verification. Exit 0 ≠ clean link. Gate = build-log grep
`unresolved external symbol` == 0 (and `LNK1181` == 0). Delete the target exe before building to force
the link gate to actually run.

### Build invariants (AGENTS.md)
`$env:MSBUILD` (VS18/v145), `/nodeReuse:false`, run from **PowerShell** (MSYS mangles `/p:Foo=Bar`).
`clientAudio` is a static lib consumed by `SwgClient` — the port touches `.cpp` + vcxproj + vendoring
only (NOT a public clientAudio header), so NO shared-header ABI cascade (RESEARCH Pitfall 6).

---

## No Analog Found

None. Every change maps to an existing in-repo file or the `miles-7.2e` tree. There is no greenfield
pattern in this phase — it is a vendor-mirror + repoint + in-place-edit phase.

---

## Stale-state cleanup (carry into the plan — not a file edit)

A prior run's `stage/miles/` and `stage-x64/miles/` may hold **7.2e** DLLs/`.m3d`. The `if not exist`
sentinel will NOT overwrite a populated dir under the OLD sentinel name. Either delete
`stage/miles/` + `stage-x64/miles/` before the first 9.3b build, OR (preferred) use a 9.3b-only sentinel
(`mssds3d.flt` / `mss64ds3d.flt`) so the stale 7.2e stage is treated as absent and refreshed
(RESEARCH Pitfall 5 / "canonical question").

## Metadata

**Analog search scope:** `src/external/3rd/library/miles-7.2e/` + `miles/`,
`src/engine/client/library/clientAudio/{build,src,include}`,
`src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj`, `.gitignore`,
`D:/Code/milesss-v9.3b/win/sdk/{include,lib,redist64}`.
**Files scanned:** 6 in-repo + 3 SDK-tree listings.
**Pattern extraction date:** 2026-06-18
