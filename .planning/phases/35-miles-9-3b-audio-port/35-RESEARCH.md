# Phase 35: Miles 9.3b Audio Port - Research

**Researched:** 2026-06-18
**Domain:** External audio-middleware SDK migration (Miles Sound System 7.2e → 9.3b) + x64 link enablement + redist staging
**Confidence:** HIGH (the central deliverable — the mss.h API delta — is verified by direct header diff of both vendored/local SDKs; staging and vcxproj wiring verified against live files)

## Summary

This phase swaps the engine's audio middleware from the x86-only Miles 7.2e SDK to the locally-cloned Miles 9.3b SDK (`D:/Code/milesss-v9.3b/`), on BOTH platforms (D-01), undoes the Phase-33 x64 Miles-disable scaffolding, and stages the 9.3b provider set so in-game audio works on x64 (and is re-verified on Win32). The work is overwhelmingly **mechanical and source-compatible** — the 7.2e→9.3b API surface the client actually calls is nearly identical, with the `FAR` macro removed (cosmetic) and **exactly one breaking signature family**: the room-type API gained a `bus_index` argument.

The header diff (the locked core research task) is **complete and verified**. Of the ~75 `AIL_*` symbols the client references, only TWO functions changed in a way that requires a code edit: `AIL_room_type(dig)` → `AIL_room_type(dig, S32 bus_index)` and `AIL_set_room_type(dig, room_type)` → `AIL_set_room_type(dig, S32 bus_index, S32 room_type)`. The roadmap's "`AIL_room_type(dig, 0)`" note covers the getter; the **setter** (26 call sites) needs `0` inserted as a **middle** argument, not appended. Everything else — file callbacks (already `UINTa`-correct from Phase 33), `AIL_open_digital_driver` (now a macro but 4-arg source-compatible), `AIL_set_named_sample_file`, `AIL_speaker_configuration`, all `MSS_MC_*`/`ENVIRONMENT_*`/`AILFILETYPE_*` constants — is unchanged or cosmetic-only.

The single biggest **runtime landmine** is NOT in the API: the 9.3b SDK **does not ship the `.m3d` 3D-provider modules** (`Msseax.m3d`, `msssoft.m3d`) that 7.2e shipped — Miles 9.x replaced them with `.flt` filters. The current codec-presence probe at `Audio.cpp:1399` hardcodes those two `.m3d` filenames, so on a 9.3b stage it will **always** report "redist missing," falsely suppress audio, and is itself a required code edit. The second runtime risk is staging mechanics: `stage/*` is gitignored and `*.lib` is gitignored, so vendored 9.3b libs/redist need force-add or a tracked-source + PostBuild-copy mirror of the proven Phase-24 model.

**Primary recommendation:** Vendor `miles-9.3b/` mirroring the `miles-7.2e/` layout; repoint both platforms' include + lib + redist; make the room_type bus_index edit (28 sites total); update the codec probe to 9.3b filenames; delete the three Phase-33 x64 disable blocks (keep the legacy `disableMiles` cfg flag); mirror the Phase-24 in-repo-redist→stage PostBuild for stage-x64 (x64 set) and stage (refreshed Win32 set); force-add the `.lib` files past gitignore.

## User Constraints (from CONTEXT.md)

<user_constraints>

### Locked Decisions
- **D-01:** Upgrade BOTH platforms to 9.3b — do not keep Win32 on 7.2e. Retire `miles-7.2e` (and the unversioned `miles/`) as the active dependency; repoint both platforms' lib + include + redist to the vendored `miles-9.3b`. The 7.2e→9.3b API edits move into the **shared** (non-`_M_X64`) code path; the 32-bit client must be re-verified for audio.
- **D-02:** SDK provides both `mss32.lib` (Win32) and `mss64.lib` (x64) at `D:\Code\milesss-v9.3b\win\sdk\lib\` — vendor both; per-platform lib selection in `clientAudio.vcxproj` selects by platform, but the **same 9.3b header set** compiles for both.
- **D-03:** Delete the Phase-33 x64 disable — Miles always-on per platform. Remove the Phase-33 `#if defined(_M_X64)` AIL_* macro-stub block (`Audio.cpp:250-310`), the `SoundObject3d.cpp:32` `#if !defined(_M_X64)` gate, and the `install()` x64 early-return (`Audio.cpp:1306-1318`, `s_disableMiles=true` x64 force-disable). No x64-specific disable survives.
- **D-04 (KEEP the legacy short-circuit):** Leave the legacy `disableMiles` `[ClientAudio]` cfg short-circuit (`Audio.cpp:1300`) intact. It is older retail config, NOT Phase-33 scaffolding — "delete all disable paths" scoped to the Phase-33 x64 disable only. Default `disableMiles=false` → Miles on. The only remaining disable path is the legacy cfg flag, applying equally to both platforms.
- **D-05:** Full 7.2e-parity provider set, staged for each platform's stage dir:
  - x64 (next to x64 exe): `mss64.dll` + `mss64mp3.asi` + `mss64ogg.asi` + `mss64dsp.flt` + `mss64eax.flt` + `mss64ds3d.flt` (+ `mss64dolby.flt` / `mss64srs.flt`) + `binkawin64.asi`
  - Win32 (next to 32-bit exe): the 9.3b 32-bit equivalents (`mss32.dll` + `mssmp3.asi` + `mssogg.asi` + `mssdsp.flt` + `msseax.flt` + `mssds3d.flt` + filters + `binkawin.asi`)
  - Reverb/room-type is an explicit success criterion → the EAX / DS3D filters must be present, not just mp3/ogg decoders. Providers live across `win/sdk/redist64`, `win/mp3/redist64`, `win/ogg/redist64` — gather from all three.
- **D-06:** Manual UAT scene checklist, all four named dimensions, on the x64 client (and a Win32 smoke for D-01 regression): 1) Music (char-select/login), 2) 2D UI (rollover + click SFX), 3) 3D positional (cantina interior ambient localizes), 4) Reverb/room-type (interior reverb on cell entry). Pass bar: all four present with **no warning-flood lag**. Silent-or-warning-flood = fail.

### Claude's Discretion
- Vendoring directory **internal layout** under `src/external/3rd/library/miles-9.3b/` — mirror the existing `miles-7.2e/` convention (`include/`, `lib/win/`, `redist/`) vs the SDK's native `win/sdk/...` tree. Planner/executor's call; keep diff minimal and consistent with the repo.
- Whether to physically `git rm` the dormant `miles-7.2e` + unversioned `miles/` trees in this phase or leave them unreferenced for a later cleanup — minimize risk; repointing references is the decision, deletion is optional.

### Deferred Ideas (OUT OF SCOPE)
- Physical `git rm` of dormant `miles-7.2e` + unversioned `miles/` trees — optional cleanup, can be a follow-up.
- New 9.3b-only audio capabilities (sound banks `.mssbnk`, additional codecs, Miles Sound Studio tooling) — out of scope; this phase restores 7.2e-equivalent behavior on the 9.3b runtime.
- The CORNERSNAP/OOM verification (Phase 36); any new audio features (banks, new codecs).

</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| AUDIO-01 | Client links vendored Miles 9.3b SDK; `clientAudio` call sites (`Audio.cpp`, `SoundObject3d.cpp`, `FirstClientAudio.h`) ported 7.2e→9.x (incl. `AIL_room_type` signature edit); SDK vendored into `src/external/3rd/library/miles-9.3b/` | API delta table (only room_type changes) + vendoring-layout + vcxproj-wiring sections below give the exact edits and lib/include repoint. D-01 widens the port to the shared (non-`_M_X64`) path on both platforms. |
| AUDIO-02 | x64 Miles redist + provider set staged next to x64 exe; in-game audio works (music, 2D UI, 3D positional, reverb/room-type) without half-dead-audio / warning-flood | Provider-set inventory (all three redist trees, both platforms) + the `.m3d`-probe landmine + the Phase-24 PostBuild staging model + the codec-probe edit. Validation Architecture maps the four UAT dimensions. |

</phase_requirements>

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| AIL_* API source port (room_type edit, stub removal) | Engine `clientAudio` (Audio.cpp / SoundObject3d.cpp) | — | The only TU calling Miles; all play/alter/query gated on `s_installed` |
| SDK vendoring (include/lib) | Repo `src/external/3rd/library/miles-9.3b/` | — | 3rd-party dependency; mirrors miles-7.2e tree |
| Per-platform lib selection (mss32/mss64) | Build (`clientAudio.vcxproj` + `SwgClient.vcxproj` link blocks) | — | Win32 vs x64 ItemDefinitionGroup AdditionalDependencies/LibraryDirectories |
| Redist / provider staging | Build (SwgClient `PostBuildEvent`) → `stage/miles` & `stage-x64/miles` | Git (tracked in-repo redist source) | Phase-24 model: tracked in-repo bytes → sentinel-guarded xcopy to stage |
| Runtime codec-presence probe | Engine `Audio::install()` (Audio.cpp:1390-1419) | — | Must be updated to 9.3b filenames or it false-fails |
| DLL discovery at runtime | OS loader + `AIL_set_redist_directory("miles")` | — | Resolves `mss*.dll`/`.asi`/`.flt` from `stage/miles` relative to CWD |

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Miles Sound System | **9.3b** (2012-12-12, RAD/SIGMA preservation build) | Digital audio driver, 2D/3D sample playback, streaming, reverb/room-type, mp3/ogg decode | The only Miles release with a native x64 lib; the engine is built on the Miles API; replacement (OpenAL/miniaudio) is explicitly out-of-scope. `[VERIFIED: D:/Code/milesss-v9.3b README + lib]` |

**SDK location (external — must be vendored INTO the repo):** `D:/Code/milesss-v9.3b/` (git at `D:/Code/milesss-v9.3b/.git`). Not currently tracked by swg-client-v2. `[VERIFIED: git ls-files empty for milesss-v9.3b]`

### Supporting (provider modules — staged, not linked)
| File (x64 / Win32) | Purpose | When to Use |
|--------------------|---------|-------------|
| `mss64.dll` / `mss32.dll` | Miles runtime DLL | always (loaded at AIL_startup) |
| `mss64mp3.asi` / `mssmp3.asi` | MP3 decoder | music + mp3-compressed world sounds |
| `mss64ogg.asi` / `mssogg.asi` | Ogg Vorbis decoder | ogg-compressed sounds |
| `mss64ds3d.flt` / `mssds3d.flt` | DirectSound3D positional filter | 3D positional (UAT #3) |
| `mss64eax.flt` / `msseax.flt` | EAX reverb/environment filter | reverb / room-type (UAT #4) |
| `mss64dsp.flt` / `mssdsp.flt` | DSP filter | DSP effects |
| `mss64dolby.flt` / `mssdolby.flt`, `mss64srs.flt` / `msssrs.flt` | Dolby / SRS surround filters | surround speaker configs |
| `binkawin64.asi` / `binkawin.asi` | Bink-video→Miles audio bridge ASI | Bink movie audio (not exercised by clientAudio directly; D-05 parity) |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Vendoring full 9.3b SDK tree | Lifting Restoration's `mss64.dll` | SUPERSEDED — local 9.3b SDK has the import lib + header; Restoration lift is the abandoned escape hatch `[CITED: memory project_miles_9_3b_local_sdk]` |
| Miles 9.3b | OpenAL Soft / miniaudio | OUT OF SCOPE (REQUIREMENTS.md) — clean-but-expensive fallback only if the port proves painful; Restoration kept Miles |

**Installation (vendoring — no npm):** copy from the external SDK into the repo. Recommended layout mirrors `miles-7.2e/`:
```
src/external/3rd/library/miles-9.3b/
├── include/mss.h            # from D:/Code/milesss-v9.3b/win/sdk/include/mss.h
├── include/rrcore.h         # mss.h does #include "rrCore.h" -- REQUIRED (see Pitfall 4)
├── lib/win/mss32.lib        # from .../win/sdk/lib/mss32.lib   (git add -f -- *.lib is gitignored)
├── lib/win/mss64.lib        # from .../win/sdk/lib/mss64.lib   (git add -f)
└── redist/                  # gathered from THREE trees (see Provider Set below); git add -f for any .lib not relevant; .dll/.asi/.flt are not gitignored by *.lib but ARE under stage/* once copied
```

**Version verification:** Header banner confirms 9.3b: `#define MILESVERSION "9.3b"`, `MILESMAJORVERSION 9`. `[VERIFIED: D:/Code/milesss-v9.3b/win/sdk/include/mss.h:16-18]`. Lib sizes: `mss32.lib` 100,882 B, `mss64.lib` 94,246 B. `[VERIFIED: ls -la]`

## The mss.h API Delta (THE central deliverable)

**Method:** Direct diff of `src/external/3rd/library/miles-7.2e/include/mss.h` (6693 lines) vs `D:/Code/milesss-v9.3b/win/sdk/include/mss.h` (~v9.3b), restricted to the ~75 `AIL_*` symbols enumerated from the live call sites (the Phase-33 stub block at `Audio.cpp:250-310` + the symbols used outside it). `[VERIFIED: header diff this session]`

### Per-symbol signature table (symbols the client actually calls)

| Symbol | 7.2e signature | 9.3b signature | Required edit |
|--------|----------------|----------------|---------------|
| **AIL_room_type** | `S32 AIL_room_type(HDIGDRIVER dig)` | `S32 AIL_room_type(HDIGDRIVER dig, S32 bus_index)` | **EDIT** — call site `Audio.cpp:3957`: `AIL_room_type(s_digitalDevice2d, 0)` |
| **AIL_set_room_type** | `void AIL_set_room_type(HDIGDRIVER dig, S32 room_type)` | `void AIL_set_room_type(HDIGDRIVER dig, S32 bus_index, S32 room_type)` | **EDIT** — insert `0` as MIDDLE arg at 26 call sites (`Audio.cpp:3919-3944`). NOTE: bus_index is the new Miles-9.x multi-bus mixer index; `0` = master bus. |
| AIL_startup | `S32 AIL_startup(void)` + func-macro wrapper `MSS_auto_cleanup()` | `S32 AIL_startup(void)` (no macro wrapper) | unchanged (call compatible) |
| AIL_shutdown | `void AIL_shutdown(void)` | identical | unchanged |
| AIL_set_redist_directory | `char FAR* (char const FAR*)` | `char* (char const*)` | unchanged (FAR cosmetic) |
| AIL_set_file_callbacks | 4 callback typedefs | identical typedefs (FAR removed) | unchanged — see callback ABI note below |
| AIL_set_preference / AIL_get_preference | `SINTa(U32,…)` (get had `#define AIL_preference[]` macro) | identical fn; 9.3b drops the get macro | unchanged (call compatible) |
| **AIL_open_digital_driver** | `HDIGDRIVER AIL_open_digital_driver(U32,S32,S32,U32)` (real function) | **MACRO** → `AIL_open_generic_digital_driver(freq,bits,channel,flags, RADSS_DSInstallDriver(0,0))` (IS_GENERICDIG + IS_WIN32 branch) | unchanged at the **call site** (4-arg macro is source-compatible); but the symbol that links is now `AIL_open_generic_digital_driver` + `RADSS_DSInstallDriver` (both in mss*.lib). No code change. |
| AIL_close_stream / AIL_open_stream | `(HSTREAM)` / `(HDIGDRIVER,char const FAR*,S32)` | identical (FAR removed) | unchanged |
| AIL_allocate_sample_handle / AIL_release_sample_handle | `(HDIGDRIVER)` / `(HSAMPLE)` | identical | unchanged |
| AIL_set_named_sample_file | `(HSAMPLE,C8 const FAR*,void const FAR*,U32,S32)` | identical (FAR removed) | unchanged |
| AIL_set_sample_file | identical | identical | unchanged |
| AIL_WAV_info / AIL_file_type / AIL_file_type_named | `(void const FAR*,…)` | `(void const*,…)` | unchanged (FAR cosmetic) |
| AIL_speaker_configuration | `MSSVECTOR3D FAR*(HDIGDRIVER,S32 FAR*,S32 FAR*,F32 FAR*,MSS_MC_SPEC FAR*)` | identical (FAR removed) | unchanged |
| AIL_set_3D_rolloff_factor / AIL_set_listener_3D_* | identical | identical | unchanged |
| AIL_set_sample_3D_position / _velocity_vector / _distances | identical | identical (exist in both) | unchanged |
| AIL_set_sample_occlusion / _obstruction / _reverb_levels / _volume_levels | identical | identical | unchanged |
| AIL_set_sample_loop_block / _loop_count / _ms_position / _playback_rate / _position | identical | identical | unchanged |
| AIL_set_stream_loop_block / _loop_count / _ms_position; AIL_stream_volume_levels | identical | identical | unchanged |
| AIL_register_EOS_callback / _stream_callback; AIL_serve; AIL_lock/unlock | identical | identical | unchanged |
| AIL_sample_status / _stream_status / _ms_position / _playback_rate / _position; AIL_active_sample_count; AIL_digital_CPU_percent; AIL_digital_latency; AIL_get_timer_highest_delay | identical | identical | unchanged |
| AIL_last_error / AIL_file_error | identical | identical | unchanged |
| AIL_MSS_version | func-like macro (platform branches) | func-like macro (platform branches, equivalent) | unchanged |
| Constants: AIL_NO_ERROR, AIL_FILE_SEEK_BEGIN/CURRENT/END, AIL_FILE_NOT_FOUND/CANT_READ/WRITE/IO_ERROR/OUT_OF_MEMORY/DISK_FULL, ENVIRONMENT_* (all 26), MSS_MC_* (all), AILFILETYPE_*, DIG_MIXER_CHANNELS | present | present, same values | unchanged |

**Migration landmines (symbol-level):** NONE in the function set. No symbol the client uses was removed or renamed between 7.2e and 9.3b. `AIL_set_named_sample_file3D` appears in the Phase-33 stub block as a variadic stub but is **never actually called** in live code (real 3D playback uses `AIL_set_sample_3D_position/velocity/distances`, which exist in both headers) — it can be dropped with the stub block, no replacement needed. `[VERIFIED: grep — only the macro stub references it]`

### File-callback ABI on x64 (UINTa) — NO work needed

The 7.2e and 9.3b file-callback typedefs are **byte-identical** apart from `FAR`:
```c
// BOTH headers:
typedef U32  (AILCALLBACK*AIL_file_open_callback)  (MSS_FILE const* Filename, UINTa* FileHandle);
typedef void (AILCALLBACK*AIL_file_close_callback) (UINTa FileHandle);
typedef S32  (AILCALLBACK*AIL_file_seek_callback)  (UINTa FileHandle, S32 Offset, U32 Type);
typedef U32  (AILCALLBACK*AIL_file_read_callback)  (UINTa FileHandle, void* Buffer, U32 Bytes);
```
The Phase-33 callback decls at `Audio.cpp:318-321` already use `UINTa` (pointer-width) and match the 9.3b typedef exactly. **No callback ABI change is required for the 9.3b port.** `[VERIFIED: header diff + Audio.cpp:312-321]`

### Symbols NOT in the Phase-33 stub block (real .lib symbols once install() body compiles on x64)

The macro-stub block (`:250-310`) stubs the symbols that appear in the *unreachable* body, but these real symbols are used in `install()`/`SoundObject3d` and are NOT stubbed — once the x64 early-return is removed and the body compiles, they will require the real `mss64.lib`: `AIL_set_redist_directory`, `AIL_startup`, `AIL_open_digital_driver` (→ `AIL_open_generic_digital_driver` + `RADSS_DSInstallDriver`), `AIL_set_file_callbacks`, `AIL_get_preference`, `AIL_speaker_configuration`, `AIL_set_3D_rolloff_factor`, `AIL_set_listener_3D_position/velocity_vector/orientation`, `AIL_stream_volume_levels`, `AIL_shutdown`, `AIL_set_preference`. This is the expected/desired outcome — linking `mss64.lib` resolves them. The `/FORCE`-link-grep gate (`unresolved external symbol` == 0) is what proves the x64 link is complete. `[VERIFIED: grep stub block vs full symbol list]`

## Phase-33 disable scaffolding to REMOVE (D-03) vs KEEP (D-04)

All line numbers verified against the **live** `Audio.cpp` / `SoundObject3d.cpp` this session.

| Action | Location | Exact range | Notes |
|--------|----------|-------------|-------|
| **DELETE** | `Audio.cpp` — macro-stub block | `#if defined(_M_X64)` at **line 250** through `#endif // _M_X64 (Phase 33: Miles disabled)` at **line 310** | The ~60 AIL_* no-op `#define`s. Delete the whole block; the real symbols come from mss64.lib. |
| **DELETE** | `Audio.cpp` — install() x64 early-return | `#if defined(_M_X64)` at **line 1306** through `#else` at **line 1319** (the `s_disableMiles=true; setEnabled(false); return false;` x64 force-disable), AND the matching `#endif // !_M_X64 …` at **line 1504** | Convert `#if _M_X64 / x64-disable / #else / <real body> / #endif` into just `<real body>` (shared path, per D-01). |
| **DELETE** | `SoundObject3d.cpp` — listener gate | `#if !defined(_M_X64)` at **line 32** through `#endif …` at **line 36** | Remove the gate so the 3 `AIL_set_listener_3D_*` calls run on both platforms (shared path). |
| **KEEP** | `Audio.cpp` — legacy disableMiles cfg short-circuit | **lines 1298-1304** (`s_disableMiles = ConfigFile::getKeyBool("ClientAudio","disableMiles",false); if (s_disableMiles) { … return false; }`) | D-04: this is retail config, NOT Phase-33 scaffolding. It is SEPARATE from and ABOVE the x64 block. Default false → Miles on. Preserves the user off-switch on both platforms. |

**Caution:** the x64 block at 1306 currently *reassigns* `s_disableMiles = true` — deleting the x64 block removes that reassignment but the legacy read at 1298 stays. Verify the resulting `install()` reads cfg once (line 1298) and proceeds to the real body. `[VERIFIED: Audio.cpp:1293-1319, 1504]`

## Vendoring layout + vcxproj wiring (how 7.2e is referenced today → mirror for 9.3b)

### Current 7.2e/unversioned references (every one must be repointed)

| Reference type | Current value | Where | Repoint to |
|----------------|---------------|-------|-----------|
| **Include path** | `external\3rd\library\miles\include` (the **unversioned** `miles/`, file `Mss.h`) | `clientAudio.vcxproj` lines 109/147/184/217/236 (all 4 configs) | `external\3rd\library\miles-9.3b\include` |
| Include (header) | `#include <mss.h>` | `clientAudio/src/shared/FirstClientAudio.h:18` | unchanged (resolved via include path) |
| **Lib dir** | `external\3rd\library\miles\lib\win` AND `external\3rd\library\miles-7.2e\lib\win` | `SwgClient.vcxproj` Win32 + x64 AdditionalLibraryDirectories (lines 147, 214, 260, 324, 381); editor vcxprojs | `external\3rd\library\miles-9.3b\lib\win` |
| **Lib name (Win32)** | `Mss32.lib` / `mss32.lib` | `SwgClient.vcxproj` Win32 Debug (line 145) + Release (212/258); editor vcxprojs | `mss32.lib` (from 9.3b SDK — name unchanged) |
| **Lib name (x64)** | **none** (Phase-33 dropped it) | `SwgClient.vcxproj` x64 Debug (line 322) + x64 Release — no mss in AdditionalDependencies | **ADD `mss64.lib`** to x64 AdditionalDependencies |

**Key facts:**
- The active **include** path is the *unversioned* `miles/include` (capital `Mss.h`), NOT `miles-7.2e`. Both vendored trees exist; the include points at `miles/`, the Win32 lib dir lists both. Per D-01, repoint to `miles-9.3b` and retire both old trees as the active dependency. `[VERIFIED: clientAudio.vcxproj:109; SwgClient.vcxproj:147]`
- `clientAudio` is a **static lib**; `SwgClient.vcxproj` is the final link that pulls in `Mss32.lib` (Win32 Debug line 145 lists `Mss32.lib` AND `mss32.lib` — duplicated, harmless). The x64 link block has **no** Miles lib (Phase-33 made AIL_* symbol-count zero so none was needed). `[VERIFIED: SwgClient.vcxproj:145, 322]`
- Editor vcxprojs (AnimationEditor, SoundEditor, ParticleEditor, etc.) also link `mss32.lib` from `miles\lib\win` — they are **pre-broken on Qt** (per AGENTS.md, not in the validation bar) but their lib dir should be repointed for consistency if the old trees are removed (Claude's discretion / deferred).

### x64 link reality

- Phase 33 produced the first full x64 link by **dropping** Mss32.lib and reducing AIL_* symbols to zero. Phase 35 reverses this: link `mss64.lib`, which resolves all the now-live AIL_* externals. The x64 lib dir (`miles-9.3b\lib\win`) + `mss64.lib` in x64 AdditionalDependencies is the wiring.
- Build per AGENTS.md: `$env:MSBUILD`, VS18/v145, `/nodeReuse:false`, run from PowerShell, delete the target exe to force relink. Gate: grep build log for `unresolved external symbol` (must be **0**) — `/FORCE` (ForceFileOutput + /SAFESEH:NO + /VERBOSE active on the x64 link, lines 152/155/322) masks LNK2019/2001 to warnings. `LNK1181` is a hard error. `[VERIFIED: SwgClient.vcxproj link options + AGENTS.md]`
- The same 9.3b `mss.h` header compiles for both platforms (D-02). Platform detection in `mss.h` comes from `rrCore.h` (`__RADNT__`/`__RAD64__`/`__RADWIN__`, set from `_WIN32`/`_WIN64`) — `IS_WIN32`/`IS_WIN64`/`IS_GENERICDIG` resolve the digital-driver macro automatically. `[VERIFIED: mss.h:101-119, 2323, 3071]`

## Provider / redist set (D-05) — concrete files per platform

The SDK splits providers across **three** redist trees per platform. Confirmed presence (`[VERIFIED: find this session]`):

### x64 set (→ `stage-x64/miles/`)
| File | Source in SDK | Present |
|------|---------------|---------|
| `mss64.dll` | `win/sdk/redist64/mss64.dll` | ✓ |
| `mss64mp3.asi` | `win/mp3/redist64/mss64mp3.asi` | ✓ |
| `mss64ogg.asi` | `win/ogg/redist64/mss64ogg.asi` | ✓ |
| `mss64dsp.flt` | `win/sdk/redist64/mss64dsp.flt` | ✓ |
| `mss64eax.flt` | `win/sdk/redist64/mss64eax.flt` | ✓ (reverb — UAT #4) |
| `mss64ds3d.flt` | `win/sdk/redist64/mss64ds3d.flt` | ✓ (3D — UAT #3) |
| `mss64dolby.flt` | `win/sdk/redist64/mss64dolby.flt` | ✓ |
| `mss64srs.flt` | `win/sdk/redist64/mss64srs.flt` | ✓ |
| `binkawin64.asi` | `win/sdk/redist64/binkawin64.asi` | ✓ |

### Win32 set (→ `stage/miles/`, refreshing the current 7.2e set)
| File | Source in SDK | Present |
|------|---------------|---------|
| `mss32.dll` | `win/sdk/redist/mss32.dll` | ✓ |
| `mssmp3.asi` | `win/mp3/redist/mssmp3.asi` | ✓ |
| `mssogg.asi` | `win/ogg/redist/mssogg.asi` | ✓ |
| `mssdsp.flt` | `win/sdk/redist/mssdsp.flt` | ✓ |
| `msseax.flt` | `win/sdk/redist/msseax.flt` | ✓ (reverb) |
| `mssds3d.flt` | `win/sdk/redist/mssds3d.flt` | ✓ (3D) |
| `mssdolby.flt` | `win/sdk/redist/mssdolby.flt` | ✓ |
| `msssrs.flt` | `win/sdk/redist/msssrs.flt` | ✓ |
| `binkawin.asi` | `win/sdk/redist/binkawin.asi` | ✓ |
| (`mssvoice.asi`) | `win/vox/redist/mssvoice.asi` | ✓ (voice ASI — in 7.2e parity set; not required by clientAudio) |

### Providers the SDK does NOT ship (the critical gap)
- **`Msseax.m3d` and `msssoft.m3d` (the `.m3d` 3D-provider modules) do NOT exist anywhere in the 9.3b SDK.** Miles 9.x replaced the `.m3d` 3D-provider architecture with the `.flt` filters (`mssds3d.flt`/`msseax.flt`). The current 7.2e working set ships `.m3d`; 9.3b does not. This is the source of the codec-probe landmine (Pitfall 1). `[VERIFIED: find -iname "*.m3d" returns 0 in 9.3b]`
- No 32-bit `binkawin` in `win/mp3` or `win/ogg` redist — `binkawin.asi` is only in `win/sdk/redist`. (Present, just one location.)

### Staging mechanism (mirror the proven Phase-24 model)
The Win32 redist is staged by the SwgClient `PostBuildEvent` (`SwgClient.vcxproj:165-180`), which since Phase 24 (PORT-01, commit `c0ff0119a`) copies from the **in-repo tracked** `src\external\3rd\library\miles-7.2e\redist` to `stage\miles\`, sentinel-guarded (`if not exist stage\miles\mssmp3.asi`). `[VERIFIED: SwgClient.vcxproj:169-180]`
- **Win32:** repoint the existing PostBuild source from `miles-7.2e\redist` to `miles-9.3b\redist` (and update the sentinel filenames — see Pitfall 1).
- **x64:** the x64 PostBuild (`SwgClient.vcxproj` x64 Debug ~line 343, Release ~line 393) currently stages **only exe+pdb** to `stage-x64\` — NO miles staging. Add an equivalent xcopy of the x64 provider set from `miles-9.3b\redist64` (or wherever the vendored x64 redist lands) to `stage-x64\miles\`. `[VERIFIED: SwgClient.vcxproj x64 PostBuildEvent]`

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Audio mixing / 3D positional / reverb | A custom audio path | Miles 9.3b AIL_* API (the existing engine path) | Replacement is out-of-scope; the engine is built on Miles |
| Redist staging on fresh checkout | A manual "copy miles by hand" step | The Phase-24 in-repo-tracked-redist + sentinel-guarded PostBuild xcopy | Already proven; fresh clone stages working audio with zero external dependency `[CITED: SwgClient.vcxproj:169-180]` |
| Detecting "is the codec set present" | A new bespoke probe | Update the existing `Audio::install()` probe (Audio.cpp:1399) to 9.3b filenames | The loud-once-then-quiet flood-suppression logic (s_milesCodecRedistAvailable) is already wired; only the filename list is wrong for 9.3b |
| x64 callback pointer-width | New callback signatures | The existing `UINTa` callbacks (Phase 33) | Already match the 9.3b typedef byte-for-byte |

**Key insight:** This is a re-link + 2-line-API-shape + filename-list change, NOT an audio re-architecture. The risk is entirely in (a) the room_type bus_index edit, (b) the `.m3d`→`.flt` probe filename change, and (c) staging/gitignore mechanics — not in the AIL_* surface.

## Runtime State Inventory

> rename/refactor-adjacent (string + provider-filename migration). Categories answered explicitly.

| Category | Items Found | Action Required |
|----------|-------------|------------------|
| Stored data | None — no datastore keys reference Miles version. The `datatables/music/music.iff` data table (loaded at install) is codec-agnostic (sound paths, not provider names). | none — verified by reading install() data-table load (Audio.cpp:1471-1490) |
| Live service config | `[ClientAudio]` cfg keys (`disableMiles`, `soundProvider`, volumes, `enabled`) live in `stage/client.cfg` + `client_d.cfg` + `stage-x64/client*.cfg`. `disableMiles` MUST default false (D-04). `soundProvider` maps to MSS_MC_* channel names (unchanged in 9.3b). | Verify `disableMiles=false` (or absent) in all four cfgs before UAT. Provider names unchanged — no cfg value migration needed. `[VERIFIED: getProviderSpec uses MSS_MC_* names, identical in both headers]` |
| OS-registered state | None — Miles registers no OS-level services/tasks. DLLs are discovered at runtime via `AIL_set_redist_directory("miles")` relative to CWD. | none |
| Secrets/env vars | None — no Miles secrets/env vars. Build uses `$env:MSBUILD` (unrelated). | none |
| Build artifacts | Vendored `.lib` files are caught by gitignore `*.lib` (line 14) — `miles-7.2e/lib/win/Mss32.lib` is tracked only because force-added. **New `miles-9.3b/lib/win/mss*.lib` will be silently un-tracked unless `git add -f`.** `stage/*` (gitignore line 92) and `stage-x64/` (not explicitly ignored but staged binaries are proprietary) hold copied redist — sourced from in-repo tracked redist via PostBuild, so a fresh checkout regenerates them. | `git add -f` the two 9.3b libs (and the vendored redist if any `.lib` slips in). Confirm the in-repo `miles-9.3b/redist` source bytes ARE tracked (they're not `.lib`, so not auto-ignored). `[VERIFIED: .gitignore:14,92; git ls-files miles-7.2e]` |

**The canonical question — after every repo file is updated, what still has the old runtime state?** `stage/miles/` and `stage-x64/miles/` directories from a prior run will hold **7.2e** DLLs/`.m3d` files. The sentinel-guarded PostBuild (`if not exist`) will **NOT** overwrite an existing populated `stage/miles` — so a stale 7.2e stage survives the rebuild. **Action:** delete `stage/miles/` and `stage-x64/miles/` before the first 9.3b build (or change the sentinel to a 9.3b-only filename like `mss64.dll` so a 7.2e stage is treated as absent and refreshed). This is the same class as the Phase-24 "stale stage repair" review note.

## Common Pitfalls

### Pitfall 1: The codec-presence probe false-fails on 9.3b (audio silently suppressed)
**What goes wrong:** `Audio::install()` (Audio.cpp:1399) checks `{ "mssmp3.asi", "mssogg.asi", "Msseax.m3d", "msssoft.m3d" }`. 9.3b ships NO `.m3d` files, so the probe always finds them missing → sets `s_milesCodecRedistAvailable = false` → emits the "redist missing" WARNING and gates audio off — even with a perfectly staged 9.3b set.
**Why it happens:** Miles 9.x replaced `.m3d` 3D-provider modules with `.flt` filters. The probe is a 7.2e-era filename hardcode.
**How to avoid:** Update the probe list to 9.3b filenames. Suggested per platform: Win32 `{ "mss32.dll", "mssmp3.asi", "mssogg.asi", "mssds3d.flt", "msseax.flt" }`; x64 `{ "mss64.dll", "mss64mp3.asi", "mss64ogg.asi", "mss64ds3d.flt", "mss64eax.flt" }`. (Use a platform `#if _M_X64` to pick the right list, since both platforms now run this code path.)
**Warning signs:** Boot log prints "Miles codec/provider redist missing" with a fully-populated `stage/miles`. `[VERIFIED: Audio.cpp:1390-1419]`

### Pitfall 2: AIL_set_room_type bus_index inserted in the WRONG position
**What goes wrong:** Treating the room_type change as "append `, 0`" breaks `AIL_set_room_type` — the new `bus_index` is the **middle** arg (`dig, bus_index, room_type`), not trailing. Appending `0` after `room_type` is a compile error (3 args expected, the call would pass `dig, ENVIRONMENT_X, 0` — wrong: room_type lands in the bus_index slot).
**Why it happens:** The roadmap shorthand "`AIL_room_type(dig, 0)`" describes only the getter (where `0` IS trailing). The setter is different.
**How to avoid:** Getter (`AIL_room_type`, 1 site at 3957): `AIL_room_type(dig, 0)`. Setter (`AIL_set_room_type`, 26 sites at 3919-3944): `AIL_set_room_type(dig, 0, ENVIRONMENT_X)`. `[VERIFIED: 9.3b mss.h:5049-5054]`

### Pitfall 3: Vendored mss*.lib silently not committed (gitignore *.lib)
**What goes wrong:** `.gitignore:14` is a bare `*.lib`. Copying `mss32.lib`/`mss64.lib` into the repo and `git add` skips them; a fresh checkout has no Miles lib → link fails (`LNK1181 cannot open input file 'mss64.lib'`, a hard error).
**How to avoid:** `git add -f src/external/3rd/library/miles-9.3b/lib/win/mss32.lib mss64.lib`. Confirm with `git ls-files`. (This is exactly why 7.2e's `Mss32.lib` shows in `git ls-files` — it was force-added.) `[VERIFIED: .gitignore:14; git ls-files miles-7.2e]`

### Pitfall 4: rrCore.h missing from the vendored include dir
**What goes wrong:** `mss.h` does `#include "rrCore.h"` (line ~101). If only `mss.h` is vendored without `rrcore.h`, compile fails (`cannot open include file 'rrCore.h'`).
**Why it happens:** The 7.2e vendored `include/` may have inlined or shipped rrCore differently; the 9.3b SDK ships `rrcore.h` (lowercase) alongside `mss.h`.
**How to avoid:** Vendor `rrcore.h` from `D:/Code/milesss-v9.3b/win/sdk/include/rrcore.h` into the same include dir. Note the case mismatch (`#include "rrCore.h"` vs file `rrcore.h`) is harmless on Windows NTFS (case-insensitive) but flag it. `[VERIFIED: mss.h:101 include; ls include/ shows rrcore.h]`

### Pitfall 5: Stale 7.2e stage survives the rebuild (sentinel guard)
**What goes wrong:** The PostBuild `if not exist stage\miles\mssmp3.asi` skips copying if a 7.2e `stage/miles` already exists → the client runs against stale 7.2e DLLs against a 9.3b-linked exe (ABI/version mismatch, crashes or silent audio).
**How to avoid:** Delete `stage/miles/` + `stage-x64/miles/` before the first 9.3b build, OR change the sentinel to a 9.3b-only file (`mss64.dll` / a 9.3b-versioned name). `[VERIFIED: SwgClient.vcxproj:178]`

### Pitfall 6: Shared-header ABI cascade
**What goes wrong:** `clientAudio` is consumed by `SwgClient` and editor projects. The port touches `Audio.cpp` (a .cpp, not a public header) so no ABI cascade is expected — but if any public clientAudio header changes, all consumers must rebuild (per AGENTS.md shared-header trap).
**How to avoid:** Keep the port confined to `.cpp` + vcxproj + vendoring. `FirstClientAudio.h` only includes `<mss.h>` — changing the include path is a build-setting change, not a struct-ABI change. `[CITED: AGENTS.md shared-header ABI trap]`

## Code Examples

### The room_type edits (verified call sites)
```cpp
// Audio.cpp ~3919-3944  (setter — 26 sites, bus_index 0 is the MIDDLE arg):
case RT_alley: { AIL_set_room_type(s_digitalDevice2d, 0, ENVIRONMENT_ALLEY); } break;
// ... (repeat for all 26 ENVIRONMENT_* cases)

// Audio.cpp ~3957  (getter — trailing bus_index):
switch (AIL_room_type(s_digitalDevice2d, 0))
```
`// Source: D:/Code/milesss-v9.3b/win/sdk/include/mss.h:5049-5054 (verified)`

### The codec probe edit (platform-aware)
```cpp
// Audio.cpp ~1399 — replace the .m3d-bearing 7.2e list:
#if defined(_M_X64)
    static char const * const s_requiredMilesCodecs[] =
        { "mss64.dll", "mss64mp3.asi", "mss64ogg.asi", "mss64ds3d.flt", "mss64eax.flt" };
#else
    static char const * const s_requiredMilesCodecs[] =
        { "mss32.dll", "mssmp3.asi", "mssogg.asi", "mssds3d.flt", "msseax.flt" };
#endif
// (rest of the loop + s_milesCodecRedistAvailable logic unchanged)
```
`// Source: Audio.cpp:1399 (current) + 9.3b redist inventory (verified)`

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `.m3d` 3D-provider modules (Msseax.m3d, msssoft.m3d) | `.flt` filters (mssds3d.flt, msseax.flt) | Miles 9.x | The codec probe + redist set must drop `.m3d`, add the `.flt` 3D/reverb filters |
| `AIL_open_digital_driver` as a real function | A macro → `AIL_open_generic_digital_driver(..., RADSS_DSInstallDriver(0,0))` | Miles 9.x (IS_GENERICDIG) | Source-compatible at the 4-arg call site; different symbols link (in the lib) |
| `AIL_room_type(dig)` / `AIL_set_room_type(dig, rt)` | `+ S32 bus_index` (multi-bus mixer) | Miles 9.x | The only client-facing breaking change; bus 0 = master |
| `FAR` pointer qualifiers | removed (flat 32/64-bit) | Miles 9.x | Cosmetic; no code impact |
| Per-call x64 stub/disable (Phase 33) | Real mss64.lib link (Phase 35) | this phase | x64 audio goes live |

**Deprecated/outdated:**
- `Msseax.m3d` / `msssoft.m3d` — not shipped in 9.3b; do not stage or probe for them.
- The Phase-33 `#if _M_X64` AIL_* macro-stub block — delete entirely (D-03).

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | `bus_index = 0` is the correct value for the room_type calls (master/default mixer bus). | room_type edit | LOW — Miles docs treat bus 0 as the default mixer bus; if reverb routes to a non-default bus, room-type would apply to the wrong bus (UAT #4 would catch it — reverb audible vs not). Doc confirmation deferred to UAT. |
| A2 | The codec probe filename set I propose (mss*.dll + mp3/ogg/ds3d/eax) is sufficient to represent "redist present." | Pitfall 1 / probe edit | LOW — probe is advisory (flood-suppression), not a hard gate; even a wrong list only mis-warns. The real audio gate is `AIL_open_digital_driver` succeeding. |
| A3 | `binkawin(64).asi` is NOT required for the four UAT dimensions (it's the Bink-movie audio bridge). | provider set | LOW — D-05 stages it for parity regardless; if a startup Bink movie needs it and it's absent, only movie audio is affected, not music/UI/3D/reverb. |
| A4 | The 9.3b `mss.h` compiles clean under the project's v145/C++20/stdcpp20 toolchain (it's a 2012 header). | vcxproj wiring | MEDIUM — old RAD headers sometimes trip modern warnings-as-errors. clientAudio is WarningLevel4 but `TreatWarningAsError=false` (verified SwgClient x64 block:313), so warnings won't fail the build. A hard compile error would surface immediately at first build. |
| A5 | `RADSS_DSInstallDriver` / `AIL_open_generic_digital_driver` are exported by `mss64.lib` (the macro target). | API delta / x64 link | LOW-MEDIUM — they're declared in mss.h under IS_GENERICDIG+IS_WIN32; the link-grep gate (unresolved==0) is the definitive check. If absent, link fails loudly (not silently). |

## Open Questions (RESOLVED)

1. **Does 9.3b on x64 need a different `AIL_set_redist_directory` value or DLL search behavior than Win32?**
   - What we know: `AIL_set_redist_directory("miles")` resolves relative to CWD; the codec probe builds paths the same way. Both platforms call the same code now.
   - What's unclear: whether the x64 `mss64.dll` + ASIs load cleanly from a relative `miles\` subdir when the exe runs from `stage-x64\`.
   - Recommendation: stage to `stage-x64\miles\` (mirroring `stage\miles\`) and verify at first x64 boot; the probe will report success/failure in the log.
   - **RESOLVED:** verified at execution time by the 35-04 T1 boot-log census (asserts the driver-open / "Finished initializing" success line and absence of "Miles is disabled").

2. **Will the 2012 `mss.h`/`rrcore.h` compile without modification under v145 + `/std:c++20`?**
   - What we know: TreatWarningAsError is false; the header is C-compatible.
   - What's unclear: any hard C++20 incompatibility (unlikely — it's plain C declarations).
   - Recommendation: first task should be a compile-only smoke of `clientAudio` against the 9.3b header on both platforms before the room_type edits, to isolate header-compat from API-edit issues.
   - **RESOLVED:** verified at execution time by the 35-01 T2 compile-only header smoke (with the `warning C####`-vs-real-incompat distinction added per review).

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| Miles 9.3b SDK (mss.h, rrcore.h) | AUDIO-01 compile | ✓ | 9.3b | — |
| mss32.lib (Win32 import lib) | AUDIO-01 Win32 link | ✓ `win/sdk/lib/mss32.lib` | 9.3b | — |
| mss64.lib (x64 import lib) | AUDIO-01 x64 link | ✓ `win/sdk/lib/mss64.lib` | 9.3b | — |
| x64 provider set (mss64.dll + asi/flt) | AUDIO-02 x64 runtime | ✓ across redist64 trees | 9.3b | — |
| Win32 provider set | AUDIO-02 / D-01 Win32 | ✓ across redist trees | 9.3b | — |
| `.m3d` 3D providers | (legacy probe) | ✗ NOT in 9.3b | — | Use `.flt` filters + update probe (Pitfall 1) |
| `$env:MSBUILD` (VS18/v145) | build/relink | ✓ (per AGENTS.md / settings.json) | VS18 | — |
| d3dcompiler_47.dll in stage-x64 | x64 boot (unrelated, Phase 32/33) | ✓ (present in stage-x64) | — | — |

**Missing dependencies with no fallback:** none.
**Missing dependencies with fallback:** `.m3d` 3D providers — replaced by `.flt` filters in 9.3b; resolved by updating the codec probe and staging the `.flt` set.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | None automated for audio — this is a native MSBuild C++ engine subsystem; audio output is inherently runtime/manual (no test harness in repo for clientAudio) |
| Config file | n/a (Nyquist: no unit-test infra for this subsystem) |
| Quick run command | Build gate: `$env:MSBUILD ... /t:SwgClient /p:Platform=x64 /p:Configuration=Debug /nodeReuse:false` then grep log for `unresolved external symbol` (==0) |
| Full suite command | Boot the staged client + run the D-06 manual UAT checklist |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| AUDIO-01 | x64 client links mss64.lib (0 unresolved AIL_* externals) | build-time | build log grep `unresolved external symbol` == 0 + `LNK1181` == 0 | ✅ build gate |
| AUDIO-01 | Win32 client links mss32.lib (D-01 regression) | build-time | same grep on Win32 build log | ✅ build gate |
| AUDIO-01 | Phase-33 x64 stubs removed | build-time | `grep -c "_M_X64" Audio.cpp SoundObject3d.cpp` for the deleted blocks → 0 (excluding the codec-probe `#if`) | ✅ grep gate |
| AUDIO-01 | room_type bus_index edit applied | build-time | grep `AIL_set_room_type(s_digitalDevice2d, 0,` count == 26; `AIL_room_type(s_digitalDevice2d, 0)` present | ✅ grep gate |
| AUDIO-02 | Provider set staged | build-time | `stage-x64/miles/mss64.dll` + asi/flt exist; `stage/miles/mss32.dll` + set exist | ✅ file-exists gate |
| AUDIO-02 | Codec probe passes (no false "redist missing") | runtime-log | boot log: NO "Miles codec/provider redist missing" warning | ⚠️ manual (log check) |
| AUDIO-02 #1 | Music plays (char-select/login) | manual UAT | — | ❌ manual-only |
| AUDIO-02 #2 | 2D UI rollover + click SFX audible | manual UAT | — | ❌ manual-only |
| AUDIO-02 #3 | 3D positional ambient localizes (cantina) | manual UAT | — | ❌ manual-only |
| AUDIO-02 #4 | Reverb/room-type audible on cell entry | manual UAT | — | ❌ manual-only |
| AUDIO-02 | No warning-flood lag | manual UAT + log | log line count not exploding (the 5,327× "Unable to create a valid sample id" signature) | ⚠️ manual (log census) |

### Sampling Rate
- **Per task commit:** build the touched target (clientAudio compile, then SwgClient link) + grep `unresolved external symbol`.
- **Per wave merge:** full 5-target build both platforms + boot smoke to character select.
- **Phase gate:** D-06 four-dimension UAT on x64 + Win32 audio smoke, no warning-flood, before `/gsd-verify-work`.

### Wave 0 Gaps
- [ ] No automated audio test harness exists — UAT is human-only by nature (acknowledged in D-06). No Wave-0 test files to author; the build/grep/file-exists gates above are the automatable surface.
- [ ] Boot-log census helper (count "Unable to create a valid sample id" + "redist missing") would make the warning-flood check semi-automatable — optional Wave-0 nicety, not required.

## Security Domain

> `security_enforcement` not found in `.planning/config.json` context as explicitly false; this phase is a native desktop-game audio middleware swap with no network/auth/input-validation surface. ASVS categories are largely N/A.

### Applicable ASVS Categories
| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | n/a — no auth in audio subsystem |
| V3 Session Management | no | n/a |
| V4 Access Control | no | n/a |
| V5 Input Validation | partial | Sound file images are parsed by Miles (AIL_WAV_info/AIL_file_type) from game TRE data — trusted game assets, not user input. No new validation surface. |
| V6 Cryptography | no | n/a |

### Known Threat Patterns for native C++ / external binary SDK
| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Untrusted DLL load (mss64.dll from CWD) | Tampering / Elevation | Standard for this engine — DLLs ship in the controlled `stage/` install; `AIL_set_redist_directory` uses a fixed relative path. No mitigation change vs 7.2e. |
| Malformed audio asset → decoder crash | DoS | Game assets are from signed TRE archives; same trust boundary as 7.2e. No new exposure. |
| Vendored proprietary binary provenance | — | 9.3b is a RAD/SIGMA preservation build for legacy maintenance (per SDK README) — same licensing posture as the existing 7.2e binaries already in the repo. |

## Sources

### Primary (HIGH confidence)
- `D:/Code/milesss-v9.3b/win/sdk/include/mss.h` — full AIL_* signature diff vs 7.2e (room_type, file callbacks, open_digital_driver macro, all constants)
- `src/external/3rd/library/miles-7.2e/include/mss.h` — 7.2e baseline signatures
- `src/engine/client/library/clientAudio/src/win32/Audio.cpp` — live call-site inventory, install(), codec probe, room_type sites, Phase-33 stubs (line-verified)
- `src/engine/client/library/clientAudio/src/win32/SoundObject3d.cpp:32` — x64 listener gate
- `src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj` — link blocks (Win32/x64), lib dirs, PostBuild staging
- `src/engine/client/library/clientAudio/build/win32/clientAudio.vcxproj` — include path
- `D:/Code/milesss-v9.3b/` redist trees (`find` inventory, both platforms) + README (version banner)
- `.gitignore` (`*.lib` line 14, `stage/*` line 92) + `git ls-files` (force-add evidence)

### Secondary (MEDIUM confidence)
- `.planning/STATE.md`, `.planning/ROADMAP.md`, `.planning/REQUIREMENTS.md` — phase scope, sequencing, success criteria
- Project memory `project_miles_9_3b_local_sdk`, `project_audio_fixed_missing_miles_redist` — SDK provenance + the missing-redist failure signature
- Phase-24 commit `c0ff0119a` (PORT-01) — the in-repo-redist staging model

### Tertiary (LOW confidence)
- bus_index=0 semantics (Miles 9.x multi-bus mixer) — inferred from mss.h bus API declarations (AIL_bus_* take `S32 bus_index`); not cross-checked against Miles 9.x docs (doc/ folder is a placeholder). Flagged A1.

## Metadata

**Confidence breakdown:**
- API delta (the core task): HIGH — both headers diffed directly; only room_type changed for client-used symbols.
- Stub-removal line ranges: HIGH — verified against live files this session.
- Provider/redist set: HIGH — every file located by `find` in the actual SDK; the `.m3d` gap confirmed by absence.
- vcxproj/staging wiring: HIGH — read the actual Win32 + x64 link blocks and PostBuild events.
- bus_index value + header C++20 compat: MEDIUM — A1/A4 deferred to UAT/first-build.

**Research date:** 2026-06-18
**Valid until:** ~30 days (stable — local SDK and repo are fixed; the only volatility is line-number drift if Audio.cpp is edited before planning)
