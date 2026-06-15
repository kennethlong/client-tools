# Phase 31: 64-bit Correctness Foundation - Pattern Map

**Mapped:** 2026-06-15
**Files analyzed:** 16 touched files (15 modified in-place + 1 new uncommitted scratch props) across 5 pattern families
**Analogs found:** 16 / 16 (every touched file maps to an in-tree analog; the scratch `.props` maps to a structural `.vcxproj` analog since the tree has zero `.props` files)

> This phase is **source surgery on a still-32-bit tree** — almost everything is an in-place edit, not a new file. The "analog" question is therefore mostly **house-style**: when the executor rewrites a `__asm` block to an intrinsic, fixes a `(int)ptr` truncation, or adds a `static_assert`, which existing in-tree code shows the convention to copy? That is what this map answers. The `__asm` inventory and truncation/serialization defect lists are already verified in `31-RESEARCH.md` — do NOT re-derive them; this doc only supplies the analog/excerpt mapping.

---

## File Classification

| Touched file | Role | Front | Closest Analog | Match Quality |
|--------------|------|-------|----------------|---------------|
| `sharedFoundation/src/win32/FloatingPointUnit.cpp` | foundation/config | BITS-01 (D-04) | dpvs `_controlfp` usage (3rd-party) + MS-cited pattern in RESEARCH §Pattern 1 | role-match (no first-party `_control87` analog) |
| `sharedMath/src/win32/SseMath.cpp` | math/transform | BITS-01 (D-05) | **no in-tree `_mm_*` analog** — RESEARCH §Pattern 2 is the spec | no analog (house-style = new) |
| `sharedMath/src/shared/Transform.cpp` (`sse_xf_matrix_3x4`) | math/transform | BITS-01 (D-05) | self-referential to SseMath rewrite | sibling-match |
| `sharedCollision/src/shared/core/CollisionUtils.cpp` | math/utility | BITS-01 misc | `sqrtf` (CRT) — RESEARCH §Pattern 3 | trivial-match |
| `sharedFoundation/src/shared/Fatal.cpp` | foundation/diagnostics | BITS-01 misc | miles `MSSBreakPoint()` → `__debugbreak()` (3rd-party) | role-match |
| `sharedFoundation/src/shared/Clock.cpp` | foundation/diagnostics | BITS-01 misc | same `__debugbreak()` analog as Fatal.cpp | role-match |
| `sharedDebug/src/win32/DebugHelp.cpp` | diagnostics/stack-walk | BITS-01 (bitness-fork) | RESEARCH §Pattern 4 (`RtlCaptureContext`); only `#if _M_X64` fork in phase | no analog (OS-API) |
| `sharedDebug/src/win32/ProfilerTimer.cpp` (`readTimeStampCounter`) | diagnostics/profiling | BITS-01 misc | `__rdtsc()` intrinsic — RESEARCH §Pattern 3 | trivial-match |
| `clientGame/src/shared/HTTPpost/VeCritsec.hpp` | concurrency/spinlock | BITS-01 misc | `_interlockedbittestandset` intrinsic — RESEARCH §Pattern 3 | trivial-match |
| `clientGraphics/src/shared/StaticShader.cpp` | graphics/sort-key | BITS-02 | **`sharedDebug/src/shared/LeakFinder.h:81`** (canonical `reinterpret_cast<uintptr_t>`) | exact |
| `sharedMemoryManager/src/shared/MemoryManager.cpp` | allocator/ptr-math | BITS-02 | `LeakFinder.h:81` (ptr→int) + `ptrdiff_t` for diffs | exact / role-match |
| `sharedFoundation/src/win32/Os.cpp` (`ShellExecute`) | OS-boundary | BITS-02 | `LeakFinder.h:81` pattern, widened to `INT_PTR`/`DWORD_PTR` | exact (type-variant) |
| Direct3d11/Direct3d9 `*VertexBufferData` / `*ShaderData` | graphics-plugin/buffer | BITS-02 | `LeakFinder.h:81`; **note existing `static_cast<int>(reinterpret_cast<uintptr_t>(...))` re-truncation sites** | exact (+ defect to fix) |
| `external/ours/library/archive/src/shared/Archive.h` (`std::map` get/put) | serialization/wire | BITS-03 (D-07) | **in-file** `std::vector` get/put fixed-4-byte count + `unsigned int` overload | exact (same file) |
| `sharedImage/src/shared/TargaFormat.cpp` (pragma-pack) | image/disk-layout | BITS-03 (D-08) | **`Direct3d11_ConstantBuffer.h:47-66`** `static_assert(sizeof==N)` + `offsetof` constellation | exact |
| `sharedGame/.../AssetCustomizationManager.cpp` (pragma-pack) | game/disk-layout | BITS-03 (D-08) | same `Direct3d11_ConstantBuffer.h` static_assert analog | exact |
| **NEW** `src/build/win32/x64-scratch/x64-compile.props` (uncommitted) | build-config | D-01 harness | **`sharedMath/build/win32/sharedMath.vcxproj`** `ItemDefinitionGroup/ClCompile` block | structural-match |

---

## Pattern Assignments

### BITS-02 truncation — THE canonical analog: `LeakFinder.h:81`

**Analog:** `src/engine/shared/library/sharedDebug/src/shared/LeakFinder.h` (lines 79-83)

This is the single most-reused analog in the phase. RESEARCH names it "the **template to copy**" for every pointer→integer site (StaticShader sort-key, MemoryManager ptr-math/logging, Os::ShellExecute). It is the one place in the in-scope tree that already does the width-correct thing:

```cpp
// sharedDebug/src/shared/LeakFinder.h:79-83  [VERIFIED]
struct ptr_hash {
    size_t operator()(void* p) const noexcept {
        return std::hash<uintptr_t>()(reinterpret_cast<uintptr_t>(p));
    }
};
```

**Apply to (per RESEARCH §"BITS-02 Truncation-Defect Sample"):**
- `clientGraphics/src/shared/StaticShader.cpp:289` — the defect is `return reinterpret_cast<int>(&getStaticShaderTemplate());` inside `int getShaderTemplateSortKey() const`. Width-correct fix mirrors LeakFinder: cast through `uintptr_t`/`intptr_t`. **Caveat:** the function's return type is `int`; preserving the signature while keeping the key unique on x64 needs either a return-type widen or a hash (planner's discretion — D-06 toolbox says "pointer stored/hashed as integer → `uintptr_t`").
- `sharedMemoryManager/.../MemoryManager.cpp:405` (ptr-diff via int → `ptrdiff_t`), `:1329,1426,1753` (`%08x` logging → `uintptr_t` + `%p`/`%zx`).
- `sharedFoundation/src/win32/Os.cpp:1579` (HINSTANCE→int → `INT_PTR`/`DWORD_PTR`).

**ANTI-PATTERN already in the tree (fix, don't copy):** the Direct3d11 plugin has `static_cast<int>(reinterpret_cast<uintptr_t>(...))` — width-correct cast immediately **re-truncated to `int`**:
```cpp
// Direct3d11_StaticVertexBufferData.cpp:167  [VERIFIED — re-truncation defect]
return static_cast<int>(reinterpret_cast<uintptr_t>(m_d3dBuffer.Get()));
// Direct3d11_DynamicVertexBufferData.cpp:372  — same shape
```
These are exactly the `(int)(intptr_t)ptr` re-truncation the RESEARCH anti-pattern warns against (the `uintptr_t` is correct, the outer `static_cast<int>` defeats it). The Direct3d9 plugin's `(DWORD)(uintptr_t)` casts in `Direct3d9.cpp:137-203` are diagnostic-log-only (`%08X`) and are the same class.

---

### BITS-03 serialization — Archive `std::map` size_t pin

**Analog:** `src/external/ours/library/archive/src/shared/Archive.h` — **in-file**, the surrounding fixed-width overloads are the pattern to match.

The hazard sites (verified):
```cpp
// Archive.h:191-204  get(std::map<>)  — HAZARD: raw size_t count
template<typename Key, typename Value> inline void get(ReadIterator & source, std::map<Key, Value> & target)
{
    size_t numKeys;            // <-- 4 bytes on Win32, 8 on x64; NO 64-bit overload exists
    get(source, numKeys);
    ...
}
// Archive.h:380-389  put(std::map<>)  — HAZARD twin
template<typename Key, typename Value> inline void put(ByteStream & target, const std::map<Key, Value> & source)
{
    size_t numKeys = source.size();   // <-- same width hazard
    put(target, numKeys);
    ...
}
```

The **in-file analog showing the correct house-style** (how every other container count is already pinned to a fixed 4 bytes — copy this shape):
```cpp
// Archive.h:130-141  get(std::vector<>)  — the correct fixed-width count idiom
template<typename A> inline void get(ReadIterator & source, std::vector<A>& target)
{
    signed int length = 0;
    source.get(&length, 4);    // <-- explicit 4-byte read, bitness-invariant
    ...
}
// Archive.h:368-371  put(std::vector<>)  — write twin
signed int length = source.size();
target.put(&length, 4);
```
And the scalar overload the pinned `uint32_t` will bind to on both bitnesses:
```cpp
// Archive.h:47-50  — the 4-byte unsigned overload (what uint32_t resolves to)
inline void get(ReadIterator & source, unsigned int & target) { source.get(&target, 4); }
```

**Fix (RESEARCH §Pattern 5):** replace `size_t numKeys` with `uint32_t numKeys` (read) / `const uint32_t numKeys = static_cast<uint32_t>(source.size())` (write). Binds the existing 4-byte `unsigned int` overload on both bitnesses; wire format stays byte-identical to the shipped 32-bit server. **Do NOT add an 8-byte overload** (would change the wire format — RESEARCH anti-pattern).

> Note: `include/Archive/Archive.h` is a 2-line forwarder to `src/shared/Archive.h` — edit the `src/shared/` copy.

---

### BITS-03 layout guards — `static_assert(sizeof==N)` + `offsetof`

**Analog:** `src/engine/client/application/Direct3d11/src/win32/Direct3d11_ConstantBuffer.h` (lines 41-66, and the `offsetof` constellation at 240-249).

This is **the established house convention** for size/layout guards in this tree (added during Phase 11 D3D11 work) — match its exact form: a `static_assert(sizeof(X) == N, "message")` immediately after the struct, message naming the struct + reason. RESEARCH §Pattern 6 mirrors it.

```cpp
// Direct3d11_ConstantBuffer.h:41-48  [VERIFIED — the convention to copy]
struct Direct3d11_PerFrameCB { ... };
static_assert(sizeof(Direct3d11_PerFrameCB) == 96,         "Direct3d11_PerFrameCB size mismatch");
static_assert((sizeof(Direct3d11_PerFrameCB) % 16) == 0,   "Direct3d11_PerFrameCB not 16-byte aligned");
```
```cpp
// Direct3d11_ConstantBuffer.h:243-244  — offsetof field-drift guard
static_assert(offsetof(Direct3d11_VertexSlot0CB, objectWorldCameraProjectionMatrix) == 0,  "c0 boundary");
static_assert(offsetof(Direct3d11_VertexSlot0CB, objectWorldMatrix)                 == 64, "c4 boundary");
```
(`Direct3d11_LightManager.h:58-59` and `Direct3d11_LegacyPSConstants.cpp:33-34` are additional in-tree instances of the same convention.)

**Apply to** the two `#pragma pack(push,1)` structs that are the only packed structs in the engine tree (verified layout-safe — fixed-width fields, no pointer/size_t):
```cpp
// sharedImage/src/shared/TargaFormat.cpp:64-85  — TargaHeader (uint8/uint16) + TargaFooter (uint32×2 + char[N])
#pragma pack(push, 1)
struct TargaHeader { uint8 m_idLength; ... uint8 m_imageDescriptor; } PACKING_END_STRUCT;   // ~18 bytes
struct TargaFooter { uint32 m_extensionAreaOffset; ... char m_signature[N]; } PACKING_END_STRUCT;  // ~26 bytes
#pragma pack(pop)
```
- `sharedGame/.../AssetCustomizationManager.cpp:47-98` — `IntRange`/etc.

> **Baseline the literal `N` from the compiler, not from RESEARCH** (Assumption A1 / Open-Q1): the 18/26 are TGA-spec values but the executor must confirm the actual 32-bit `sizeof` and bake *that* literal in. Adding `static_assert`/`offsetof` does NOT change layout → safe in shared headers (no ABI cascade).

---

### BITS-01 intrinsic ports — house-style

There is **almost no first-party intrinsic code** in the in-scope tree to copy from (the only `_controlfp`/`__debugbreak` uses are in 3rd-party `external/3rd/`: `dpvs`, `miles`). So for BITS-01 the analog is the **RESEARCH §Patterns 1-4 spec** (MS-cited), not a sibling file. The executor matches the spec; there is no pre-existing house dialect to conform to. The sites, verified at the exact lines:

| Site (verified) | Current form | Target |
|------|------|------|
| `FloatingPointUnit.cpp:119,128` | `__asm fnstcw` / `__asm fldcw` | `_control87` (RESEARCH §Pattern 1; **guard out `_MCW_PC` on x64** — assertion, not no-op) |
| `SseMath.cpp:34` (`canDoSseMath`) | `__asm { mov eax,1; cpuid; mov featureBits,edx }` (lines 57-72) | `__cpuid`/`__cpuidex` → read EDX bit 25 (SSE) / bit 24 (FXSR) |
| `SseMath.cpp:97+` (4 math fns, 13 `__asm{}` total) | inline SSE `movaps`/`mulps`/`shufps` | `_mm_load_ps`/`_mm_mul_ps`/`_mm_set1_ps`/`_mm_shuffle_ps`/`_mm_add_ps` (RESEARCH §Pattern 2) — **live, do not delete** |
| `Transform.cpp:275` (`sse_xf_matrix_3x4`) | `__declspec(naked)` SSE 3×4 matmul | normal `_mm_*` fn (drop `naked`); register-faithful — peer of SseMath, not "misc" |
| `CollisionUtils.cpp:427-429` | `__asm fld t; fsqrt; fstp t` | `t = sqrtf(t);` |
| `Fatal.cpp:174`, `Clock.cpp:270` | `__asm int 3` | `__debugbreak()` |
| `ProfilerTimer.cpp:29-36` | `__declspec(naked) { rdtsc; ret; }` | `return __rdtsc();` (drop `naked`) |
| `VeCritsec.hpp:43-48` | `__asm { mov esi,[p_i_lock]; lock bts dword ptr[esi],0; jnc Locked }` | `_interlockedbittestandset((long*)&m_iLock, 0)` — **only the `#ifdef _WIN32` MSVC branch**; the `#else` GCC `__asm__` branch is untouched |
| `DebugHelp.cpp:503-511` | EIP/ESP/EBP register grab | `RtlCaptureContext` + AMD64 fields under `#if defined(_M_X64)` — **the only bitness-fork in the phase** |

> `MemoryManager.cpp:2074` `__asm int 3` is **already commented out** → no BITS-01 work there (it has BITS-02 ptr-math work, separate). Per RESEARCH the SSE path is **live** (`Transform::install` selects it at Transform.cpp:76-78; `SoftwareBlendSkeletalShaderPrimitive.cpp:365,1569,1592` branch on `s_useSSE = SseMath::canDoSseMath()`) — rewrite, do not delete (Pitfall 4).

---

### NEW scratch harness `.props` — structural analog

**Analog:** `src/engine/shared/library/sharedMath/build/win32/sharedMath.vcxproj` — the `ItemDefinitionGroup` / `ClCompile` block (Debug|Win32, lines 69-94).

There are **zero `.props` files anywhere in the tree** (verified) — every lib carries its compile settings inline in its `.vcxproj`. So the scratch `x64-compile.props` (D-01, uncommitted) has no `.props` analog; its structural source is a representative lib `.vcxproj`'s `ClCompile` settings, which the props must mirror so the scratch x64 compile sees the same defines/includes/standard as the committed 32-bit build:

```xml
<!-- sharedMath.vcxproj:69-94  Debug|Win32 ClCompile — the settings the scratch x64 props must replicate -->
<ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Debug|Win32'">
  <ClCompile>
    <AdditionalIncludeDirectories>...sharedDebug\include\public;...sharedFoundation\include\public;...;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
    <PreprocessorDefinitions>WIN32;_DEBUG;_MBCS;DEBUG_LEVEL=2;_CRT_SECURE_NO_DEPRECATE=1;_USE_32BIT_TIME_T=1;_LIB;%(PreprocessorDefinitions)</PreprocessorDefinitions>
    <RuntimeTypeInfo>true</RuntimeTypeInfo>
    <WarningLevel>Level4</WarningLevel>
    <TreatWarningAsError>false</TreatWarningAsError>     <!-- scratch flips to /we4311 /we4312 /we4244 only -->
    <LanguageStandard>stdcpp20</LanguageStandard>
  </ClCompile>
</ItemDefinitionGroup>
```

**Key items to carry into the scratch props/driver** (RESEARCH §"Scratch x64 Compile-Only Harness"):
- `_USE_32BIT_TIME_T=1` — **critical**: without it `time_t` widens to 8 bytes on x64 and breaks the BITS-03 size asserts. Present in all 3 configs of every lib `.vcxproj`; the harness MUST carry it.
- `_CRT_SECURE_NO_DEPRECATE=1`, `WIN32`, `_DEBUG`, `DEBUG_LEVEL=2`, `_LIB`, `_MBCS`; `LanguageStandard=stdcpp20`; `ConfigurationType=StaticLibrary` (compile, no app link).
- `PLATFORM_WIN32` — drives the `#pragma pack` in TargaFormat.cpp:60 and `PACKING_END_STRUCT`; RESEARCH lists it in the harness define set (note: not in this particular `.vcxproj`'s define line — confirm per-lib, but the harness should define it for parity with the pack macros).
- Per-lib `AdditionalIncludeDirectories` — read out of each in-scope lib's own `.vcxproj` (the long relative list), since they differ per lib.
- Scratch-only deltas vs. this analog: target x64 `cl.exe` (`Hostx64\x64`), `/c` only, `/Y-` (no PCH), `/we4311 /we4312 /we4244`.

> Lib `.vcxproj` files live in per-lib `build/win32/` dirs (e.g. `sharedMath/build/win32/sharedMath.vcxproj`), NOT in `src/build/win32/` (which holds only `swg.sln` + `_all_*.vcxproj` aggregators). Use any in-scope lib's `build/win32/*.vcxproj` as the per-lib settings source.

---

## Shared Patterns

### Pointer-as-integer (BITS-02, applies to all of clientGraphics / sharedMemoryManager / sharedFoundation / both renderer plugins)
**Source:** `sharedDebug/src/shared/LeakFinder.h:79-83` (`reinterpret_cast<uintptr_t>(p)`).
**Toolbox (D-06):** stored/hashed ptr → `uintptr_t`; signed diff → `intptr_t`/`ptrdiff_t`; Win32 boundary → `DWORD_PTR`/`INT_PTR`/`LONG_PTR`; buffer size → `size_t`. **Never** `(int)(intptr_t)ptr` (the Direct3d11_*VertexBufferData re-truncation), **never** `#pragma warning(disable)`.

### Fixed-width wire count (BITS-03, applies to Archive serializers)
**Source:** `Archive.h:130-141 / 368-371` — `signed int length; get(&length,4)` / `put(&length,4)`. Pin counts to a fixed 4 bytes (`uint32_t`), bind the existing `unsigned int` 4-byte overload. Never widen the serializer.

### Layout guard (BITS-03/D-08, applies to every packed/serialized struct touched)
**Source:** `Direct3d11_ConstantBuffer.h:47-66, 240-249` — `static_assert(sizeof(X)==N, "...")` + `static_assert(offsetof(X,f)==K, "...")`. Layout-safe in shared headers (no ABI cascade). Baseline `N` from the compiler.

### Validation / regression (all fronts, D-08)
**Source:** existing dual-renderer boot smoke. `client_d.cfg` `rasterMajor=5` then `=11`, Debug exe, boot to char-select (exercises TRE/IFF load + login handshake). **Edit `.cfg` with Edit/Write/bash, never PS `Set-Content`** (BOM crashes Release boot). Re-validate 32-bit after edits; grep build log for `unresolved external symbol` = 0 (exit 0 ≠ clean link under `/FORCE`).

---

## No Analog Found

Files where no in-tree first-party analog exists — the executor follows the cited RESEARCH spec / MS docs, not a sibling file:

| File / site | Front | Why no analog | Use instead |
|------|------|------|------|
| `SseMath.cpp` math routines, `Transform.cpp` `sse_xf_matrix_3x4` | BITS-01 | No first-party `_mm_*` intrinsic code anywhere in the in-scope tree | RESEARCH §Pattern 2 (register-faithful translation, MS-cited) |
| `FloatingPointUnit.cpp` `_control87` port | BITS-01 | Only `_controlfp` use is 3rd-party (dpvs); none in our libs | RESEARCH §Pattern 1 + MS `_control87` cite; guard `_MCW_PC` on x64 |
| `DebugHelp.cpp` x64 stack-walk | BITS-01 | OS-API, inherently bitness-specific; no in-tree `RtlCaptureContext` use | RESEARCH §Pattern 4; `#if defined(_M_X64)` fork (the only fork) |
| `x64-compile.props` (new) | D-01 | Zero `.props` files in the tree | structural `.vcxproj` `ClCompile` block (sharedMath.vcxproj:69-94) |

---

## Metadata

**Analog search scope:** `src/engine/{shared,client}/library/**`, `src/engine/client/application/Direct3d{9,11}/**`, `src/external/ours/library/archive/**`, `src/game/shared/**`, `src/build/win32/**`, per-lib `build/win32/*.vcxproj`.
**Files read for excerpts:** LeakFinder.h, FloatingPointUnit.cpp, SseMath.cpp, ProfilerTimer.cpp, VeCritsec.hpp, Transform.cpp (via grep), Archive.h (src/shared), Direct3d11_ConstantBuffer.h, TargaFormat.cpp, StaticShader.cpp (via grep), sharedMath.vcxproj.
**Verified-not-found:** `.props` files (0 in tree); first-party `_mm_*`/`__cpuid`/`__rdtsc`/`_control87`/`__debugbreak` intrinsic usage (only 3rd-party).
**Pattern extraction date:** 2026-06-15
