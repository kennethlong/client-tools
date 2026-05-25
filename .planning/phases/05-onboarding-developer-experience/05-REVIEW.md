---
phase: 05-onboarding-developer-experience
reviewed: 2026-05-05T00:00:00Z
depth: standard
files_reviewed: 4
files_reviewed_list:
  - README.md
  - docs/build.md
  - docs/recon/08-phase6-preflight.md
  - src/cmake/win32/FindSTLPort.cmake
findings:
  critical: 1
  warning: 3
  info: 2
  total: 6
status: issues_found
---

# Phase 5: Code Review Report

**Reviewed:** 2026-05-05
**Depth:** standard
**Files Reviewed:** 4
**Status:** issues_found

## Summary

Phase 5 deliverables are four files: two developer-facing docs (`README.md`,
`docs/build.md`), one research/planning document (`docs/recon/08-phase6-preflight.md`),
and one updated CMake module (`src/cmake/win32/FindSTLPort.cmake`). The FindSTLPort.cmake
is the most complex artifact — it stages STLPort headers, copies MSVC and UCRT system
headers into the build tree, and surgically patches five STLPort source files via
CMake `string(REPLACE)` to bridge the gap between the vc71-era library and MSVC 2022.
The patch logic is idempotent and the sequencing is correct.

One blocker is present: both `README.md` and `docs/build.md` document the wrong
config key name for the TRE asset search path (`searchPath_0` instead of `searchPath0`),
which would cause silent asset-loading failure if a developer follows the documentation
literally. Three additional warnings cover a hardcoded Windows Kits path that breaks
on non-default SDK installs, a missing diagnostic when the UCRT directory cannot be
located, and a reconfigure-time performance issue from unguarded `file(COPY)` calls.
Two info items cover documentation clarity.

---

## Critical Issues

### CR-01: Wrong config key name for TRE search path in README and build guide

**File:** `README.md:43`, `docs/build.md:176-177`
**Issue:** Both documents instruct the developer to set `searchPath_0` (with an
underscore) in `client.cfg`. The actual config key read by the game is `searchPath0`
(no underscore), as confirmed by every shipped config file in `exe/win32/common.cfg`
(`searchPath0=`, `searchPath1=`, `searchPath2=`) and by all `exe/win32/*.cfg` editor
configs. The SWG config parser treats `searchPath_0` as an unrecognised key and silently
ignores it, causing the client to start with an empty tree-file search path and find no
game assets. The developer will see a running (non-crashing) client that cannot load any
world data — a confusing failure mode with no error message.

**Fix:**
```ini
# README.md line 43 — change:
[SharedFile] searchPath_0=<path-to-your-SWG-install>
# to:
[SharedFile] searchPath0=<path-to-your-SWG-install>

# docs/build.md lines 175-177 — change:
[SharedFile]
searchPath_0=C:/SWG
searchPath_1=C:/SWG/patch
# to:
[SharedFile]
searchPath0=C:/SWG
searchPath1=C:/SWG/patch
```

---

## Warnings

### WR-01: Hardcoded Windows Kits path breaks on non-default SDK installations

**File:** `src/cmake/win32/FindSTLPort.cmake:6`
**Issue:** The UCRT include directory is constructed with a hardcoded prefix:
```cmake
set(_UCRT_INCLUDE_DIR "C:/Program Files (x86)/Windows Kits/10/Include/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/ucrt")
```
This path fails silently on machines where:
- The Windows SDK was installed to a non-default drive or directory (common in CI
  environments and shared build servers)
- The developer uses an arm64 Windows host (where the native SDK may be under
  `C:/Program Files/Windows Kits/` rather than `(x86)`)
- The `CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION` variable is empty (e.g., early
  configure pass before the VS generator fully initialises it)

When `_UCRT_INCLUDE_DIR` does not exist, the `if(EXISTS ...)` guard on line 16 silently
skips the UCRT header copy and the `stdio.h` redirect write (line 35). The staged include
directory then lacks the UCRT headers, and the cstdio patch applied at line 45 will have
been applied to a file that may still include an unredirected `stdio.h`. No warning is
emitted — the developer gets a cryptic compile error later instead of a clear configure-time
message.

**Fix:**
```cmake
# Replace the hardcoded prefix with a registry query (available on Windows):
if(NOT _UCRT_INCLUDE_DIR)
    get_filename_component(_WIN_KITS_ROOT
        "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot10]"
        ABSOLUTE)
    set(_UCRT_INCLUDE_DIR "${_WIN_KITS_ROOT}/Include/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/ucrt")
endif()

# Add a diagnostic when the directory still doesn't exist after the lookup:
if(NOT EXISTS "${_UCRT_INCLUDE_DIR}")
    message(WARNING "FindSTLPort: UCRT include directory not found at '${_UCRT_INCLUDE_DIR}'. "
                    "STLPort stdio.h redirect will not be staged. Build may fail with "
                    "C2732 or double-inclusion errors.")
endif()
```

---

### WR-02: Silent UCRT miss — no diagnostic when staged stdio.h redirect is skipped

**File:** `src/cmake/win32/FindSTLPort.cmake:35-40`
**Issue:** The `stdio.h` redirect (the patch that prevents double-inclusion of
`stdio.h` between the staged UCRT copy and the real UCRT) is written inside an
`if(EXISTS "${_UCRT_INCLUDE_DIR}/stdio.h")` block (line 35). If the UCRT directory
is missing (see WR-01), both the UCRT header copy and the stdio.h redirect are
skipped with no message. The developer proceeds to build and encounters C2732 linkage
errors or `#pragma once` double-inclusion warnings at compile time, with no CMake
configure-time indication of what went wrong. The existing code for the stlport/
copy (line 8) demonstrates the pattern of guarding operations with `if(EXISTS ...)`
but that pattern is not paired with a `message(WARNING ...)` fallback.

**Fix:** Add a `message(WARNING ...)` in the `else` branch of any `if(EXISTS ...)` guard
that controls a staging operation critical to the build. At minimum:
```cmake
if(EXISTS "${_UCRT_INCLUDE_DIR}")
    file(COPY "${_UCRT_INCLUDE_DIR}/" DESTINATION "${STLPORT_STAGE_ROOT}/include")
else()
    message(WARNING "FindSTLPort: UCRT include dir '${_UCRT_INCLUDE_DIR}' not found — "
                    "staged STLPort headers may be incomplete.")
endif()
```

---

### WR-03: Unguarded MSVC and UCRT file(COPY) runs on every reconfigure

**File:** `src/cmake/win32/FindSTLPort.cmake:12-18`
**Issue:** The `stlport/` tree copy is correctly guarded (`if(NOT EXISTS ...)` on
line 8), but the two `file(COPY)` operations for MSVC and UCRT headers have no
equivalent guard:
```cmake
if(EXISTS "${_MSVC_INCLUDE_DIR}")
    file(COPY "${_MSVC_INCLUDE_DIR}/" DESTINATION "${STLPORT_STAGE_ROOT}/include")  # line 13
endif()
if(EXISTS "${_UCRT_INCLUDE_DIR}")
    file(COPY "${_UCRT_INCLUDE_DIR}/" DESTINATION "${STLPORT_STAGE_ROOT}/include")  # line 17
endif()
```
On every incremental `cmake ..` (reconfigure without deleting the build tree), CMake
re-copies the entire MSVC and UCRT header trees into the staged `include/` directory.
On a typical VS 2022 install these directories are several hundred files; copying them
every configure adds multiple seconds to what should be a near-instant reconfigure. The
`typeinfo.h` write and `stdio.h` write at lines 24 and 37 overwrite any previously
staged versions immediately after, so correctness is not affected, but the unnecessary
bulk copy precedes them.

**Fix:**
```cmake
if(EXISTS "${_MSVC_INCLUDE_DIR}" AND NOT EXISTS "${STLPORT_STAGE_ROOT}/include/vcruntime.h")
    file(COPY "${_MSVC_INCLUDE_DIR}/" DESTINATION "${STLPORT_STAGE_ROOT}/include")
endif()
if(EXISTS "${_UCRT_INCLUDE_DIR}" AND NOT EXISTS "${STLPORT_STAGE_ROOT}/include/stdio.h")
    file(COPY "${_UCRT_INCLUDE_DIR}/" DESTINATION "${STLPORT_STAGE_ROOT}/include")
endif()
```
Note: use a sentinel file that is known to exist in each source tree (`vcruntime.h` for
MSVC, `stdio.h` for UCRT) to gate the copy, matching the pattern used for the
`stlport/` copy on line 8.

---

## Info

### IN-01: Duplicate [ClientGame] section in sample config may confuse readers

**File:** `docs/build.md:179-188`
**Issue:** The sample `client.cfg` block shows two separate `[ClientGame]` sections:
```ini
[ClientGame]
loginServerAddress0=127.0.0.1
loginServerPort0=44453

[Station]
gameFeatures=15

[ClientGame]
freeChaseCameraMaximumDistance=75.0
```
The SWG config parser does merge duplicate section headers (as shown in
`exe/win32/common.cfg` which uses the same pattern), so this is not a functional bug.
However, a developer unfamiliar with SWG's config format may think the second
`[ClientGame]` block overrides or is an error. A brief inline comment would prevent
confusion.

**Fix:** Add a comment above the second `[ClientGame]`:
```ini
# Multiple [ClientGame] blocks are merged by the parser — both are valid here.
[ClientGame]
freeChaseCameraMaximumDistance=75.0
```

---

### IN-02: _FILE_I_preincr/_postincr stubs use unused FILE* parameter — future /WX risk

**File:** `src/cmake/win32/FindSTLPort.cmake:74` (injected into `_stdio_file.h`)
**Issue:** The four character-reference stub functions injected into the patched
`_stdio_file.h` accept a `FILE*` parameter that is never used:
```cpp
inline char& _FILE_I_preincr(FILE *__f) { static char c; return c; }
inline char& _FILE_I_postincr(FILE *__f) { static char c; return c; }
inline char& _FILE_I_predecr(FILE *__f) { static char c; return c; }
inline char& _FILE_I_postdecr(FILE *__f) { static char c; return c; }
```
Under `/W4` these would emit C4100 (unreferenced formal parameter). The current
build intentionally omits `/WX`, so this does not cause a build failure today.
However, when `/WX` is enabled in a future milestone, these stubs (injected into
vendored STLPort source) will produce errors that are non-trivial to suppress
(they are in a generated file, not a source file, so `#pragma warning(disable:4100)`
would need to be part of the injected text).

**Fix:** Silence the unused parameter by casting to void inside the stub, incorporated
into the `string(REPLACE)` content in FindSTLPort.cmake:
```cpp
inline char& _FILE_I_preincr(FILE *__f) { (void)__f; static char c; return c; }
```
Apply the same pattern to all four `FILE*`-accepting stubs.

---

_Reviewed: 2026-05-05_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
