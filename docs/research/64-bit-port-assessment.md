# 64-bit Port Assessment

> Scope estimate for converting the SWG client tree from 32-bit Win32 compile
> and runtime to native 64-bit Windows.

## Executive summary

This is a large port, not a project-file toggle.

The current tree is deliberately 32-bit at three layers:

1. Build metadata: the checked-in Visual Studio projects expose only `Win32`
   configurations and the renderer plug-ins explicitly link as `MachineX86`.
2. Third-party binary stack: the client depends on many vendored Win32-era
   import libraries and DLLs. Several have no x64 binary in the repo.
3. Runtime assumptions: source code contains many pointer-to-`int`/`DWORD`
   casts, `SetWindowLong`/`GetWindowLong` usage, x86 inline assembly, and a
   custom memory manager that stores and prints addresses as 32-bit integers.

Practical estimate:

| Goal | Size | Why |
| --- | --- | --- |
| x64 compile of a reduced/stubbed client | Large, weeks | Requires x64 project/CMake plumbing, replacing or stubbing x86-only middleware, and fixing compiler errors from pointer truncation. |
| x64 boot to login/character select | Very large, 1-3 months | Adds runtime validation of memory manager, graphics plug-in ABI, asset loaders, UI, audio stubs/replacements, and crash/debug tooling. |
| x64 feature-complete runtime | Major port, multi-month | Requires real x64 replacements for audio/video/voice/browser/occlusion pieces or deliberate feature removals, plus broad gameplay/client soak testing. |

The most pragmatic path is an x64 branch with a deliberately smaller feature set:
Direct3D11 only, browser disabled, voice disabled, Bink optional/stubbed, video
capture disabled, and DPVS either rebuilt from source or replaced with a no-op
frustum-only path until correctness is proven.

## Evidence from docs

Relevant existing docs:

- `README.md` says the original solution is Visual Studio based and notes that
  SWG applications are only compatible with Win32.
- `docs/research/swgclient-build.md` records the original client configurations
  as `Debug|Win32`, `Optimized|Win32`, and `Release|Win32`; it also notes
  `_USE_32BIT_TIME_T=1` and that the client never targeted 64-bit.
- `docs/build.md` describes the modern build work and lists the vendored SDK
  landscape: Miles, DPVS, Bink, Vivox, Mozilla, STLPort, libxml2, PCRE, and zlib
  are all staged from Win32-era vendor paths.
- `docs/research/runtime-middleware.md` is the most useful dependency triage.
  It shows most client middleware is contained, but not necessarily available
  as x64 binaries.

Important nuance: `docs/build.md` describes a CMake + Visual Studio 2022 port,
but this checkout currently does not appear to contain active top-level CMake
files. A scan only found a baseline `CMakeLists.txt` under `.planning/`. The
assessment below therefore treats the checked-in `.vcxproj`/`.sln` files as the
active build metadata visible in this working tree.

## Build-system work

The visible project files are under `build/win32` directories. Representative
examples:

- `src/build/win32/swg.sln`
- `src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj`
- `src/engine/client/application/Direct3d9/build/win32/Direct3d9.vcxproj`
- `src/engine/client/application/Direct3d11/build/win32/Direct3d11.vcxproj`
- `src/engine/shared/library/sharedFoundation/build/win32/sharedFoundation.vcxproj`

The client and shared library projects declare only Win32 configurations. The
D3D9 and D3D11 plug-ins also set:

```xml
<TargetEnvironment>Win32</TargetEnvironment>
<TargetMachine>MachineX86</TargetMachine>
```

That means the first build task is not "change the solution platform." It is:

1. Add `x64` configurations across the client dependency graph.
2. Change output directories so x86 and x64 objects/libs never collide.
3. Split x86 and x64 third-party library search paths.
4. Replace `WIN32` as an architecture signal with clearer defines:
   - `WIN32` or `_WIN32` means Windows API.
   - `_WIN64` / architecture-specific project config means pointer size.
5. Decide whether the modern build system is the source of truth. If CMake is
   restored, x64 should be added there first; duplicating this manually across
   dozens of `.vcxproj` files is brittle.

Expected size: medium-large. Project generation is mechanical, but the moment
x64 is selected the linker will expose the real third-party binary blockers.

## Third-party binary blockers

These are the highest-confidence blockers from the docs and project files.

| Dependency | Current state | x64 implication |
| --- | --- | --- |
| Miles Sound System | `Mss32.lib` plus `Mss32.dll` | Need x64 Miles SDK/runtime, or replace/stub audio. |
| Bink | `bink/lib/win32`, `binkw32.dll`, late-bound | Need x64 Bink or disable cinematics. Because it is late-bound, a stub is low-risk. |
| DPVS | `dpvs/lib/win32-x86`, full source also vendored | Best candidate to rebuild from source, but source includes x86 assembly paths. Could be stubbed temporarily. |
| Vivox | Win32 SDK DLLs and wrapper libs | Easiest to disable. The runtime docs say only 5 client files call through the wrapper. |
| Mozilla XPCOM | Old Win32 XPCOM DLL stack | Keep disabled/stubbed. Do not try to port this as part of x64. |
| STLPort 4.5.3 | prebuilt vc6/vc7/vc71 Win32 libs | Either continue the shim work with x64-compatible replacements, or migrate to MSVC STL in a separate effort. |
| PCRE/libxml2/zlib | old Win32 static libs | Replace with source-built x64 versions. Low semantic risk, but must preserve allocator hooks and ABI boundaries. |
| DirectX 9 extras | old SDK import libs and D3DX/DXErr pieces | Prefer D3D11 path for x64. D3D9 can be deferred or dropped. |
| Video capture / PICTools | Win32 dev-only stack | Disable for x64. Not needed for production client boot. |

The good news is that many integrations are contained. The bad news is that
"contained" still means every linked binary must be x64 or removed from the x64
link. A single x86 `.lib` in the graph makes the x64 link impossible.

## Source-level 64-bit hazards

### Pointer truncation

There are many examples of pointers being forced into 32-bit integer storage.
Some are debug-print only; others affect identity, UI item data, hashes, or
Windows message payloads.

Examples found during this assessment:

- `sharedMemoryManager/src/shared/MemoryManager.cpp`
  - `reinterpret_cast<int>(next) - reinterpret_cast<int>(this)`
  - logging addresses with `%08x` after `reinterpret_cast<int>(memory)`
  - computing memory addresses into an `int`
- `sharedStatusWindow/src/win32/StatusWindow.cpp`
  - stores `this` with `SetWindowLong(..., GWL_USERDATA, reinterpret_cast<LONG>(this))`
  - reads it back with `GetWindowLong`
- `sharedFoundation/src/win32/Os.cpp`
  - casts `HMENU` to `UINT` for `InsertMenu` / `AppendMenu`
  - casts `ShellExecute` result to `int`
  - uses a 32-bit `RaiseException` thread-name payload pattern
- `clientGraphics/src/shared/StaticShader.cpp`
  - returns `reinterpret_cast<int>(&getStaticShaderTemplate())`
- `clientGraphics/src/shared/RenderWorld.cpp`
  - logs DPVS object pointers as `int`
- `Direct3d9` and `Direct3d11` plug-ins
  - several buffer/resource debug IDs return `int` derived from COM pointers.

Fix pattern:

- Use `uintptr_t` / `intptr_t` for integer storage of pointers.
- Use `LONG_PTR`, `DWORD_PTR`, `UINT_PTR`, `WPARAM`, `LPARAM` for Win32 payloads.
- Replace `SetWindowLong`/`GetWindowLong` with `SetWindowLongPtr`/`GetWindowLongPtr`
  when pointer values are involved.
- Update printf formats for pointers to `%p` or use typed formatting helpers.
- Separate true persistent 32-bit asset IDs from incidental memory addresses.

Expected size: large but tractable. This is a broad audit across engine, client,
tools, and some third-party code.

### x86 inline assembly

A scan found 173 `__asm`/`_asm` hits under the source areas checked. The
load-bearing one is DPVS:

- `src/external/3rd/library/dpvs/implementation/sources/dpvsX86.cpp`

MSVC x64 does not support inline assembly. For DPVS this means either:

1. Compile a non-x86 code path if the vendored source has one.
2. Port the assembly to intrinsics or plain C++.
3. Stub/replace DPVS for the first x64 milestone.

There are also isolated debug/no-op assembly cases, such as `_asm nop` in the
UI library. Those are trivial.

### Memory manager assumptions

The custom memory manager is a high-risk area. It does block arithmetic and
diagnostics with 32-bit casts. Even if the compiler accepts a patched version,
this subsystem needs runtime validation under x64 because allocator metadata
corruption will show up as unrelated client crashes later.

Specific checks needed:

- Block header layout and alignment with 8-byte pointers.
- Guard band offsets.
- Free-list pointer math.
- Debug leak reporting.
- Any cap or target-size code that assumes 32-bit address space pressure.

For the first x64 milestone, consider using the CRT allocator behind the same
public hooks, then re-enable the custom manager after the client boots.

### Serialization and file formats

The SWG asset/network formats use explicit-width types in many places (`uint32`,
`int32`, `NetworkId` as 64-bit game identity, IFF chunk payloads). That is good.
But an x64 port must enforce a rule:

Do not serialize `size_t`, raw pointers, STL container internals, or native
`long`/`time_t` values unless the file format explicitly says so.

The global `_USE_32BIT_TIME_T=1` define is a warning sign. It was used for
compatibility with old STLPort and/or original data assumptions. Removing it
must be audited against archive formats, logs, database/network messages, and
any code that exchanges timestamps with servers.

## Runtime risk by subsystem

| Subsystem | x64 risk | Notes |
| --- | --- | --- |
| Foundation/debug/memory | High | Pointer-size assumptions and custom allocator. Must stabilize first. |
| Graphics abstraction | Medium | Engine abstraction helps, but plug-in ABI and resource debug IDs need cleanup. |
| D3D11 plug-in | Medium | Best renderer candidate for x64. Still has `MachineX86` project settings today. |
| D3D9 plug-in | High | Old D3DX/DXErr/D3D9 SDK path. Defer unless x64 D3D9 is a hard requirement. |
| Audio | High | Miles x86 dependency. Replace or stub. |
| Video/cinematics | Low-medium | Bink is late-bound, so stubbing is practical. Real playback needs x64 Bink or replacement. |
| Voice | Low if disabled, high if preserved | Vivox is contained but binary-only in this tree. |
| Browser | Low if disabled, very high if preserved | Keep the existing Mozilla stub approach. |
| UI | Medium | Pointer item-data and window proc subclassing patterns need Win64 API fixes. |
| Asset loading | Medium | Mostly explicit-width formats, but needs soak testing for `size_t` and pointer-derived IDs. |
| Network/gameplay | Medium | Network IDs are already 64-bit game IDs; risk is native type leakage, not conceptual ID width. |

## Suggested migration plan

### Phase 0: Decide the target runtime

Recommended first target:

- Windows x64 only.
- D3D11 only.
- Browser disabled.
- Voice disabled.
- Video capture disabled.
- Bink disabled or late-bound optional.
- DPVS initially rebuilt or stubbed behind `RenderWorld`.

This keeps the first milestone focused on architecture correctness rather than
preserving every legacy integration.

### Phase 1: Add x64 build plumbing

- Restore or create the active build generator if CMake is intended to be the
  source of truth.
- Add x64 configurations and output directories.
- Ensure every internal static library builds as x64 before linking `SwgClient`.
- Add a hard failure if an x86 library path is used in an x64 build.

Exit criterion: foundation/shared/client libraries compile in x64 with all
external middleware disabled or replaced by stubs.

### Phase 2: Replace x86-only link blockers

- Stub libMozilla, Vivox, Bink, video capture.
- Replace PCRE/libxml2/zlib with source-built x64 libraries.
- Decide DPVS strategy: rebuild from source without x86 asm, or temporary no-op.
- Stub Miles audio or integrate a modern x64 audio backend.

Exit criterion: `SwgClient_x64.exe` links.

### Phase 3: Fix pointer-size correctness

- Compile with high warning levels for pointer truncation.
- Sweep all `reinterpret_cast<int>`, `reinterpret_cast<DWORD>`,
  `SetWindowLong`, `GetWindowLong`, `GWL_USERDATA`, `DWL_USER`, and `%08x`
  pointer logging sites.
- Use `uintptr_t` and Win64-safe APIs.
- Add focused runtime asserts around the memory manager and Win32 callback paths.

Exit criterion: client starts, creates a window, and reaches early setup without
allocator or window-message crashes.

### Phase 4: Boot and render

- Bring up D3D11 x64.
- Validate plug-in loading and exported entry points.
- Load TRE assets.
- Reach login/character select.
- Enter a simple scene with audio/browser/voice/cinematics still disabled.

Exit criterion: repeatable login and world-load smoke test.

### Phase 5: Restore optional features selectively

Bring features back only when they have x64-compatible dependencies:

- Audio backend.
- Cinematic playback.
- Occlusion culling.
- Voice.
- Browser, only if there is a modern replacement.

## Recommended answer to "how big is it?"

If the goal is "prove this code can be native x64," expect a serious but bounded
engineering project. The likely path is weeks of build and source cleanup plus
stubs for old middleware.

If the goal is "ship the current client feature-complete as x64," expect a
multi-month port. The blockers are not only compiler errors; they include
unavailable x64 middleware, a custom allocator with 32-bit address assumptions,
old Win32 API patterns, and runtime validation across graphics, UI, assets, and
networked gameplay.

The lowest-risk first milestone is:

> x64 `SwgClient` links and boots to a window using D3D11, with Mozilla, Vivox,
> Bink, video capture, and possibly DPVS/audio disabled.

That milestone would answer whether the core engine can survive pointer widening
before spending time on replacing every legacy SDK.
