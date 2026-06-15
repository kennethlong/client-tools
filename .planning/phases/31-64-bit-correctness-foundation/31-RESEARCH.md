# Phase 31: 64-bit Correctness Foundation - Research

**Researched:** 2026-06-15
**Domain:** x64 source correctness on a still-32-bit MSVC tree (inline-asm removal, pointer/int truncation, LLP64 serialization width)
**Confidence:** HIGH (all named sites verified against the source tree; MSVC API behavior confirmed against learn.microsoft.com)

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

- **D-01:** Stand up a **throwaway, uncommitted scratch `Debug|x64` *compile-only* config** as the compiler-driven worklist generator. It COMPILES the in-scope engine libs (no link — so no x64 third-party libs needed) and lets `cl.exe` emit the real C4311/C4312/C4244 + inline-asm errors. **It must NOT touch committed `swg.sln`/`.vcxproj`** — the committed x64 platform-add stays Phase 33 scope.
- **D-02:** **Phase-31 acceptance bar** = all in-scope build-path engine libs COMPILE x64-clean in the scratch config. Anything that genuinely cannot be validated without platform/link infra is documented as a **residual worklist handed forward to Phase 33** — not forced clean here.
- **D-03:** Scope = **SwgClient boot path + the 4 renderer plugins (gl05/gl06/gl07/gl11) + every engine lib they link**. **Editor apps are explicitly OUT** (ShaderBuilder, Headless, DllExport, AnimationEditor/ParticleEditor/SwgGodClient, …). `DllExport.cpp` (91 `__asm int 3` sites) is editor-side → OUT.
- **D-04:** **`FloatingPointUnit.cpp`** — port faithfully to `_controlfp`/`_control87`. Rounding-mode + exception-masking map cleanly to SSE/MXCSR. **Precision control (P_24/P_53/P_64) has NO SSE equivalent** — `setPrecision()` becomes a **documented x64 no-op** + **VERIFY-01 (door-snap) watch-item**.
- **D-05:** **`SseMath.cpp`** (13 `__asm{}` SSE blocks) — **faithful `_mm_*` intrinsic translation**, preserving register-level semantics. Intrinsics compile on BOTH x86 and x64 → **no `#ifdef` fork**. **Usage-audit each routine first**; rewrite live ones, delete/stub dead.
- **D-06:** Enforce **`/we4311 /we4312 /we4244` in the scratch x64 harness ONLY**. Truncation FIXES land in shared source (32-bit inherits them); the committed 32-bit `.vcxproj` flags stay as-is. Fixes use width-correct types (`uintptr_t`/`intptr_t`/`DWORD_PTR`/`size_t`) — never cast-to-silence.
- **D-07:** **Targeted audit depth.** Audit only the types that change width on Windows **LLP64**: **pointer-derived fields, `size_t`, `ptrdiff_t`, `time_t`** (note `_USE_32BIT_TIME_T=1`) wherever in `#pragma pack` structs, hardcoded `sizeof()` reads, `memcpy`/blob serialization, or `Archive` put/get. **`long` stays 32-bit on LLP64** — NOT a mover. **Skip** the already-explicit fixed-width serialization.
- **D-08:** **Regression proof = dual-renderer boot smoke + compile-time guards.** Boot-to-character-select under both renderers exercises TRE/IFF load + login handshake. **PLUS `static_assert(sizeof(X)==N)` / `offsetof` guards** on each serialized/packed struct touched, baselined to current 32-bit sizes.

### Claude's Discretion
- Per-phase task sequencing (asm → truncation → layout, or interleaved).
- Exact structure of the residual-worklist artifact handed to Phase 33.
- Mechanical choice of width-correct replacement type per truncation site (`uintptr_t`/`intptr_t`/`DWORD_PTR`/`size_t`) — convention: fix with the correct width-preserving type, never cast-to-silence.

### Deferred Ideas (OUT OF SCOPE)
- Committed `/we4311 /we4312` as a permanent tree-wide guardrail — deferred to Phase 33+.
- Full serialization sweep / golden round-trip fixtures — deferred behind targeted audit + static_assert guards.
- Editor-app x64 cleanup — out of scope.
- Committed x64 platform add, third-party x64 relink, D3DX removal — Phases 32–33.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| BITS-01 | Replace x87 FPU-control inline asm with `_controlfp`/`_control87`; tree-wide `__asm` sweep made x64-clean | Verified site inventory below (FloatingPointUnit, SseMath ×13, Transform naked-SSE, CollisionUtils fsqrt, Clock/Fatal int3, DebugHelp context-capture, ProfilerTimer rdtsc, VeCritsec lock-bts). MemoryManager site is already commented out. Per-site intrinsic mapping table provided. |
| BITS-02 | Resolve pointer/int truncation (C4311/C4312/C4244 as errors); no `(int)pointer` survivors in build path | Confirmed real defects: `StaticShader::getShaderTemplateSortKey` (`reinterpret_cast<int>(ptr)`), MemoryManager ptr-diff via `int`, `Os::ShellExecute` HINSTANCE→int. Compiler-driven worklist method + replacement-type toolbox documented. |
| BITS-03 | Audit/correct struct packing / hardcoded sizeof / serialization width for IFF/TRE + network; no x64 data-layout regression | Verified: IFF reader is fixed-width by design (`int8/16/32`, `Tag`=uint32) → SAFE. Engine `int32`/`uint32` typedef to `long` → 32-bit on LLP64 → SAFE. **Real hazard = `Archive` `std::map` get/put using raw `size_t`** (no 64-bit overload exists). `#pragma pack` structs (TargaHeader/Footer, AssetCustomizationManager) are fixed-width → need static_assert guards but are layout-safe. |
</phase_requirements>

## Summary

Phase 31 is **source surgery on a still-32-bit tree** to make the SwgClient build path compile x64-clean, without adding the x64 platform (that's Phase 33). I verified every site CONTEXT.md names by reading the source, and the tree largely confirms the locked decisions — with three corrections worth flagging to the planner.

The single most important methodological truth: **the compiler is the authoritative worklist generator, not grep.** Truncation defects (`reinterpret_cast<int>(ptr)`, the `%08x`-of-a-pointer idiom) are scattered and inconsistent across the build path; an x64 compile with `/we4311 /we4312 /we4244` enumerates them deterministically while a manual grep misses casts hidden behind macros and templates. The `__asm` sweep is more grep-tractable (29 files tree-wide; the in-scope subset is small and fully inventoried below), but the truncation and serialization fronts need the scratch x64 compile to be trustworthy. C4235 (`__asm` not allowed on x64) is **always an error**, so the scratch compile surfaces every surviving inline-asm site for free.

Three corrections to the CONTEXT.md framing (all detailed below): (1) **`setPrecision()` on x64 cannot call `_controlfp(_,_MCW_PC)`** — that path *raises an assertion + invokes the invalid-parameter handler*, it is not a silent no-op; the port must guard the precision write out entirely, and `__control87_2` is a hard compiler error on x64. (2) **Transform.cpp's `__asm` is a `__declspec(naked)` SSE matrix-multiply function** (`sse_xf_matrix_3x4`), not a small inline snippet — it is the *live* SSE path when the CPU supports SSE, and naked-asm functions are doubly x64-illegal; this is a bigger BITS-01 item than "misc sweep survivor" implies. (3) The "global `/wd4244` suppression" referenced in CONTEXT.md **does not exist as a suppression** in the in-scope `.vcxproj`s — C4244 simply fires as a non-fatal WarningLevel4 warning (`TreatWarningAsError=false`); the substance of D-06 still holds (don't change committed 32-bit flags), but there is no `/wd4244` line to "leave untouched."

**Primary recommendation:** Stand up the scratch `Debug|x64` compile-only harness as a copied/uncommitted `.props` + per-lib invocation (detailed in the harness section), sequence the work asm → truncation → layout, and gate the whole phase on the existing dual-renderer boot smoke plus `static_assert` size guards on the handful of packed/serialized structs actually touched.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| FPU control-word config | sharedFoundation (engine core) | — | `FloatingPointUnit` is process-wide FP setup; lives in the foundation lib, called at `install()`. |
| SSE matrix/skin math | sharedMath + clientSkeletalAnimation | — | `SseMath` + `Transform` SSE path drive skeletal skinning (hot path) + matrix concat; both engine libs. |
| Stack-walk / crash diagnostics | sharedDebug | — | `DebugHelp::getCallStack` captures register context for symbolized backtrace. |
| Profiling timestamp | sharedDebug | — | `ProfilerTimer` rdtsc cycle counter. |
| Spinlock (HTTP post) | clientGame (HTTPpost) | — | `VeCritsec` user-space spinlock for the HTTP worker. |
| Allocator bookkeeping | sharedMemoryManager | — | Pointer arithmetic + guard-band math; truncation-sensitive. |
| IFF/TRE asset parsing | sharedFile | — | `Iff` reader; fixed-width by design — x64-safe. |
| Wire serialization | external/ours/library/archive | sharedNetworkMessages | `Archive::get/put` ByteStream; the `std::map` size_t path is the LLP64 hazard. |
| Renderer plugins | Direct3d9 / Direct3d11 (gl05/06/07/11) | clientGraphics | Vertex/index-buffer data classes hold pointer→int casts (truncation). |

## Standard Stack

This phase adds **no new libraries**. It replaces inline assembly with MSVC compiler intrinsics (already available in the toolchain) and adds compile-time `static_assert`s (C++11, available under the project's `stdcpp20`).

### Core (intrinsic headers / CRT functions — already in the toolchain)
| Facility | Header | Purpose | Why Standard |
|----------|--------|---------|--------------|
| `_control87` / `_controlfp` | `<float.h>` | FPU control word (rounding, exception mask) | Portable CRT replacement for `fnstcw`/`fldcw`; works x86+x64 [CITED: learn.microsoft.com/.../control87-controlfp-control87-2] |
| `_mm_*` SSE intrinsics | `<xmmintrin.h>` | `movaps`/`mulps`/`shufps`/`addps` etc. | MS-blessed replacement for x64-illegal SSE inline asm; compiles both bitnesses [CITED: learn.microsoft.com x64 intrinsics] |
| `__rdtsc` | `<intrin.h>` | Read time-stamp counter | Direct intrinsic for the `rdtsc` instruction [VERIFIED: web search — "available as compiler intrinsics that work in x64"] |
| `_InterlockedBitTestAndSet` | `<intrin.h>` | `lock bts` atomic bit-test-and-set | Direct intrinsic for the spinlock asm [VERIFIED: web search] |
| `__debugbreak()` | `<intrin.h>` | `int 3` breakpoint trap | Portable replacement for `__asm int 3` |
| `sqrtf` / `_mm_sqrt_ss` | `<cmath>` / `<xmmintrin.h>` | scalar sqrt | Replacement for `fld/fsqrt/fstp` x87 block |
| `RtlCaptureContext` | `<windows.h>` (winnt) | Capture CONTEXT (Rip/Rsp/Rbp) | x64-correct replacement for the EIP/ESP/EBP register-grab asm |
| `static_assert` / `offsetof` | (language) / `<cstddef>` | Compile-time layout guards | D-08 regression proof; adding them does NOT change layout (safe in shared headers) |

### Supporting
| Facility | Header | Purpose | When to Use |
|----------|--------|---------|-------------|
| `uintptr_t` / `intptr_t` | `<cstdint>` | width-correct pointer-as-integer | Truncation fixes where a pointer is stored/hashed/diffed as an integer |
| `ptrdiff_t` | `<cstddef>` | pointer subtraction result | MemoryManager pointer-difference math |
| `DWORD_PTR` | `<windows.h>` | Win32 pointer-width integer | Win32 API boundaries (e.g. `SetWindowLongPtr`, `ShellExecute` return) |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `_mm_*` intrinsics for SseMath | Drop SSE, let MSVC auto-vectorize scalar C++ | D-05 locks faithful intrinsic translation; auto-vec needs a perf sanity check and changes numerical behavior — out of scope. |
| Separate MASM `.asm` for x64 | (rejected) | MS lists MASM as an option for x64 asm, but intrinsics are the locked choice (D-05) and avoid a per-bitness build fork. |
| `_controlfp` | `_control87` | Use `_control87` for the exception-mask path to preserve DENORMAL parity (the two differ only on the DENORMAL mask — see API note below). Rounding via either. |

**Installation:** None. All facilities ship with the VS18/v145 toolchain already in use.

**Version verification:** N/A — no package installs. Toolchain is the committed VS18/v145 (`PlatformToolset=v145`), confirmed in `sharedFoundation.vcxproj`.

## Architecture Patterns

### System Architecture Diagram

```
                         [ Phase 31 work = source edits on the 32-bit tree ]
                                          |
        +---------------------------------+----------------------------------+
        |                                 |                                  |
   BITS-01 asm sweep              BITS-02 truncation                  BITS-03 layout/width
   (grep + compiler)              (compiler-driven)                   (targeted audit)
        |                                 |                                  |
   replace __asm with             /we4311 /we4312 /we4244            audit size_t/ptr/time_t
   intrinsics/_controlfp          in scratch x64 harness             in Archive map + #pragma pack
        |                                 |                                  |
        +----------------+----------------+----------------+-----------------+
                         |                                 |
                  [ scratch Debug|x64 compile-only harness ]   <-- D-01 signal generator
                  cl.exe /c each in-scope lib, no link            (uncommitted; copies props)
                         |
                  emits C4235 (asm) + C4311/4312/4244 (trunc) as ERRORS
                         |
                  ====>  WORKLIST  ====>  fixes land in SHARED .cpp/.h
                         |
        +----------------+-----------------------------------+
        |                                                    |
   committed 32-bit build inherits fixes              residual worklist
   (flags unchanged)                                  (third-party-dependent code)
        |                                                    |
   DUAL-RENDERER BOOT SMOKE (rasterMajor=5 + =11)      hand-off --> Phase 33
   + static_assert size guards = D-08 regression proof
```

### Recommended Project Structure (where the scratch harness lives — uncommitted)
```
src/build/win32/
├── swg.sln                  # COMMITTED — do NOT touch (no x64 platform; verified 0 x64 entries)
├── x64-scratch/             # UNCOMMITTED, gitignored — the D-01 harness
│   ├── x64-compile.props    # shared props: /we4311 /we4312 /we4244, /arch:SSE2 implied, x64 target
│   └── compile-all.ps1      # iterate in-scope libs, cl /c each TU, collect errors
```
> The harness must be ignored (add to a local exclude or `.gitignore` line) and never appear in a commit. Its only output is the error worklist; it is a signal generator, not a deliverable (CONTEXT.md "Specific Ideas").

### Pattern 1: FPU control-word port (BITS-01, D-04)
**What:** Replace `__asm fnstcw`/`__asm fldcw` with `_control87`; guard out the precision write on x64.
**When to use:** `FloatingPointUnit::getControlWord` / `setControlWord` / `setPrecision`.
**Example:**
```cpp
// Source: learn.microsoft.com/.../control87-controlfp-control87-2 [CITED]
// getControlWord(): read both via _control87(0,0) — mask 0 = "just read"
WORD FloatingPointUnit::getControlWord(void)
{
    // _control87 returns the FP control state; only the SSE2/MXCSR word is
    // affected on x64. Bit layout for _MCW_RC / _MCW_EM matches the engine's
    // hand-rolled masks (ROUND_*, EXCEPTION_*); _MCW_PC bits are inert on x64.
    return static_cast<WORD>(_control87(0, 0));
}

void FloatingPointUnit::setControlWord(WORD controlWord)
{
    // Apply rounding + exception-mask bits. Use _control87 (not _controlfp) so the
    // DENORMAL exception mask is honored the same way the x87 code intended.
    _control87(controlWord, _MCW_RC | _MCW_EM);   // do NOT include _MCW_PC on x64
}
```
> **CRITICAL CORRECTION to D-04:** `setPrecision()` must NOT pass `_MCW_PC` to `_controlfp`/`_control87` on x64 — doing so *raises an assertion and invokes the invalid-parameter handler* (it is not a silent no-op). The port should record `precision` for state-query parity but skip the FPU write of the precision bits when targeting x64. `__control87_2` is a hard **compiler error** on x64, so it cannot be used to set the x87 word separately. [CITED: learn.microsoft.com — "On the ARM, ARM64, and x64 platforms, changing the infinity mode or the floating-point precision isn't supported. If the precision control mask is used on the x64 platform, the function raises an assertion, and the invalid parameter handler is invoked."]

### Pattern 2: SSE inline-asm → `_mm_*` intrinsics (BITS-01, D-05)
**What:** Translate each `__asm{}` SSE block register-faithfully to `__m128` intrinsics.
**When to use:** `SseMath.cpp` (5 functions, 13 `__asm{}` blocks) + `Transform.cpp` naked `sse_xf_matrix_3x4`.
**Example (the load-matrix + transform-multiply core, applies to all 4 math routines):**
```cpp
// Source: pattern derived from the verified asm in SseMath.cpp:101-159 [VERIFIED: source read]
// Original: movaps xmm0,[ebx+0]; mulps xmm3,xmm0; ... ; horizontal-add the lanes
Vector SseMath::rotateTranslateScale_l2p(const Transform &t, const Vector &v, float scale)
{
    const float *m = reinterpret_cast<const float*>(&t);   // Transform is 3x4 row-major, 16B-aligned rows
    __m128 row0 = _mm_load_ps(m + 0);   // movaps xmm0,[ebx+0]
    __m128 row1 = _mm_load_ps(m + 4);   // movaps xmm1,[ebx+16]
    __m128 row2 = _mm_load_ps(m + 8);   // movaps xmm2,[ebx+32]

    __m128 src  = _mm_set_ps(1.0f, v.z, v.y, v.x);          // sseVariable[0] = {x,y,z,1}
    __m128 s    = _mm_set1_ps(scale);                       // movss+shufps+movlhps broadcast

    __m128 r0 = _mm_mul_ps(_mm_mul_ps(src, row0), s);       // a*sx*scale ...
    __m128 r1 = _mm_mul_ps(_mm_mul_ps(src, row1), s);
    __m128 r2 = _mm_mul_ps(_mm_mul_ps(src, row2), s);

    // horizontal add of each row's 4 lanes (the original summed sseVariable[2..4][0..3])
    return Vector(hsum4(r0), hsum4(r1), hsum4(r2));
}
```
> `hsum4` = a small helper doing the lane sum (`_mm_hadd_ps` ×2, or shuffle+add). The `rotateScale_l2p` variant sets the 4th source lane to `0.0f` and sums only 3 lanes; `skinPositionNormal[Add]_l2p` apply the same multiply to a position (w=1) and a normal (w=1) and write/accumulate into out params. **Audit usage first (D-05):** SSE is live — `Transform::install` selects `sse_xf_matrix_3x4` when `SseMath::canDoSseMath()` is true (Transform.cpp:76-78), and `SoftwareBlendSkeletalShaderPrimitive.cpp:1569/1592` branches on `s_useSSE`. Do NOT delete as dead.

### Pattern 3: scalar/instruction intrinsics (BITS-01 misc sweep)
**What:** One-line instruction replacements.
**Example:**
```cpp
// __asm int 3;                  -> __debugbreak();                 [Fatal.cpp:174, Clock.cpp:270]
// __asm{ rdtsc; ret; } (naked)  -> return __rdtsc();               [ProfilerTimer.cpp:29-36]
// __asm fld t; fsqrt; fstp t;   -> t = sqrtf(t);                   [CollisionUtils.cpp:427-429]
// __asm{ lock bts [esi],0; ... }-> _interlockedbittestandset(...)  [VeCritsec.hpp:43-48]
```

### Pattern 4: x64-correct stack-walk context capture (BITS-01)
**What:** Replace the EIP/ESP/EBP register grab with `RtlCaptureContext` and walk with the AMD64 machine type.
**Example:**
```cpp
// Source: DebugHelp.cpp:494-525 [VERIFIED: source read] — current 32-bit form uses
//   __asm { call GetEIP; GetEIP: pop eax; mov context.Eip,eax; mov context.Esp,esp; mov context.Ebp,ebp }
//   then stackWalk64(IMAGE_FILE_MACHINE_I386, ...).
// x64 form (intrinsic-based, compiles both bitnesses if guarded):
CONTEXT context;
RtlCaptureContext(&context);
// x64: AddrPC=Rip, AddrStack=Rsp, AddrFrame=Rbp; machine = IMAGE_FILE_MACHINE_AMD64
```
> This is the heaviest misc-sweep port: the surrounding `STACKFRAME64` fields (`AddrPC/AddrStack/AddrFrame`) and the `stackWalk64` machine constant are hard-wired to `IMAGE_FILE_MACHINE_I386` and `Eip/Esp/Ebp`. The register names differ on x64 (`Rip/Rsp/Rbp`). For Phase 31 (still compiling 32-bit) the minimum is to make the file *compile* x64-clean — a `#if defined(_M_X64)` branch using `RtlCaptureContext` + AMD64 fields, with the 32-bit path unchanged, is the faithful approach. This is the one BITS-01 site where a bitness branch is unavoidable (the asm is bitness-specific by nature).

### Pattern 5: LLP64 serialization width guard (BITS-03, D-07)
**What:** Force the `Archive` `std::map` count to a fixed 32-bit width so the wire format is bitness-invariant.
**Example:**
```cpp
// Source: Archive.h:191-204, 380-389 [VERIFIED: source read]
// Current: size_t numKeys; get(source, numKeys);  /  size_t numKeys = source.size(); put(target, numKeys);
// On 32-bit, size_t -> the get(unsigned int) overload (4 bytes). On x64, size_t is 8 bytes
// and NO 64-bit Archive overload exists -> overload-resolution/width hazard + wire-format change risk.
// FIX (width-stable, wire-identical to 32-bit):
uint32_t numKeys;                 // read
get(source, numKeys);             // binds the 4-byte overload on both bitnesses
...
const uint32_t numKeys = static_cast<uint32_t>(source.size());   // write
put(target, numKeys);
```

### Pattern 6: compile-time layout guard (BITS-03, D-08)
```cpp
// Source: D-08 regression-proof pattern. Adding static_assert does NOT change layout (safe in shared headers).
static_assert(sizeof(TargaHeader) == 18, "TargaFormat: TGA header must stay 18 bytes on disk");
static_assert(sizeof(TargaFooter) == 26, "TargaFormat: TGA footer must stay 26 bytes on disk");
static_assert(offsetof(TargaHeader, m_width) == 12, "TargaHeader field drift");
```

### Anti-Patterns to Avoid
- **Cast-to-silence:** `(int)ptr` → `(uintptr_t)ptr` is correct; `#pragma warning(disable:4311)` or `(int)(intptr_t)ptr` re-truncates and defeats the point (D-06).
- **Adding a 64-bit `Archive` overload that reads 8 bytes:** would change the wire format and break interop with the 32-bit server. The fix is to pin the count to 32-bit (Pattern 5), NOT to widen the serializer.
- **Changing a packed-struct field width to "fix" x64:** TargaHeader/AssetCustomization fields are fixed-width already; changing them WOULD trip the shared-header ABI cascade (rebuild all plugin DLLs). `static_assert` is the safe guard; field edits are not needed and not allowed casually.
- **Calling `_controlfp(_, _MCW_PC)` on x64:** asserts + invokes the invalid-parameter handler (NOT a no-op). Guard it out.
- **Treating `long`/`int32`/`uint32` as a width mover:** they stay 32-bit on Windows LLP64. Do not "fix" them.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| FPU control word access | inline `fnstcw`/`fldcw` (x64-illegal) | `_control87`/`_controlfp` | CRT handles x87+SSE2/MXCSR; x64-illegal asm won't compile (C4235) |
| Atomic bit-test-and-set | `lock bts` asm | `_interlockedbittestandset` | Intrinsic; correct memory semantics, x64-clean |
| Cycle counter | naked `rdtsc` | `__rdtsc()` | Naked + asm both x64-illegal; intrinsic is direct |
| Breakpoint trap | `__asm int 3` | `__debugbreak()` | Portable; identical trap |
| Register/stack context | EIP/ESP/EBP asm grab | `RtlCaptureContext` | Register names + sizes differ on x64; OS API is correct |
| SSE matrix/vector math | hand-written SSE asm | `_mm_*` intrinsics | Same codegen, compiles both bitnesses, optimizer-visible |
| Pointer-as-integer | `(int)ptr` / `reinterpret_cast<int>` | `uintptr_t`/`intptr_t`/`DWORD_PTR` | Width-correct on LLP64; no high-bit loss |
| Serialized container count | raw `size_t` over the wire | pinned `uint32_t` | Keeps wire format bitness-invariant |

**Key insight:** Every hand-rolled item here was a 2002-era micro-optimization or platform hack. Modern MSVC intrinsics generate equivalent (often better) code, compile on both bitnesses with no `#ifdef` fork (except the inherently-bitness-specific stack-walk), and are the only legal path on x64.

## Runtime State Inventory

> Phase 31 is **source surgery only** — no stored data, live-service config, OS-registered state, or build-artifact renames. It edits `.cpp`/`.h` in place. There is no rename/migration component.

| Category | Items Found | Action Required |
|----------|-------------|------------------|
| Stored data | None — no DB keys, collection names, or persisted IDs are touched. The wire/disk *formats* must stay byte-identical (that's the BITS-03 constraint, enforced by static_assert + boot smoke). | None |
| Live service config | None — verified. No n8n/Datadog/Tailscale state involved. | None |
| OS-registered state | None — verified. No Task Scheduler / pm2 / service registrations. | None |
| Secrets/env vars | None — verified. `_USE_32BIT_TIME_T=1` is a compile-flag (build define), not a secret; it is a serialization-width lever for the `time_t` audit (D-07), not state to migrate. | None (audit-only) |
| Build artifacts | The scratch x64 harness produces `.obj` files (compile-only) — throwaway, must be gitignored. The committed 32-bit `stage/` binaries are rebuilt + redeployed by the existing postbuild after source edits. | Keep scratch obj dir out of git; rebuild 32-bit for the boot smoke |

## Common Pitfalls

### Pitfall 1: `_controlfp(_, _MCW_PC)` assertion on x64
**What goes wrong:** Naively porting `setPrecision()` to `_controlfp(precisionBits, _MCW_PC)` triggers an assertion + invalid-parameter handler on x64 (not a no-op).
**Why it happens:** x64 has no FPU precision-control concept (SSE doubles are always full precision).
**How to avoid:** Record the `precision` member for state-query parity but skip the FPU write of precision bits on x64. Never use `__control87_2` (compiler error on x64).
**Warning signs:** Assertion at startup / `FloatingPointUnit::install`; invalid-parameter handler invoked. Also a VERIFY-01 watch-item — the lost 24-bit precision-force is theorized as *why* the door-snap clears on x64.

### Pitfall 2: Shared-header ABI cascade from a layout edit
**What goes wrong:** Editing a field in a struct in a shared header (consumed by gl05/06/07/11) silently breaks stale plugin DLL ABI → deterministic boot crash.
**Why it happens:** Plugins link against the struct layout at their own build time; an EXE/plugin layout mismatch reads garbage.
**How to avoid:** Adding `static_assert`/`offsetof` does NOT change layout — safe. If a serialized struct field is ever actually altered (should not be needed — they're fixed-width), rebuild ALL plugin `.vcxproj`s (gl05/06/07/11), not just SwgClient. [CITED: AGENTS.md §Build, memory `project_shared_header_abi_cascade_trap`]
**Warning signs:** `gl11_d.dll` older than `SwgClient_d.exe` after a shared-header touch; boot crash post-edit.

### Pitfall 3: `/FORCE` link masks unresolved externals
**What goes wrong:** SwgClient links under `/FORCE`; unresolved externals become warnings and a binary is still emitted with exit 0.
**Why it happens:** Koogie's link config downgrades LNK2019/2001.
**How to avoid:** Grep the build log for `unresolved external symbol` (must be 0); MSBuild exit 0 is NOT proof of a clean link. Relevant when re-validating the 32-bit build after edits (delete the exe to force a relink). [CITED: AGENTS.md §Build]
**Warning signs:** A function you renamed/removed still "links" but crashes at call time.

### Pitfall 4: Mistaking the SSE path for dead code
**What goes wrong:** Deleting `SseMath`/`sse_xf_matrix_3x4` as "obsolete" breaks skeletal skinning + matrix concat on SSE-capable CPUs (i.e. all of them).
**Why it happens:** The SSE path is gated behind `canDoSseMath()` at install time, so a casual reader sees a scalar fallback and assumes SSE is vestigial.
**How to avoid:** D-05 usage-audit confirms it IS live (Transform.cpp:76-78 install selector; SoftwareBlendSkeletalShaderPrimitive.cpp:1569/1592 runtime branch). Rewrite, don't delete.
**Warning signs:** Skinned characters render wrong / matrix math regresses after "cleanup."

### Pitfall 5: BOM corruption of `.cfg` during boot-smoke prep
**What goes wrong:** Editing `stage/client.cfg` with PowerShell `Set-Content`/`Out-File` adds a UTF-8 BOM → Release client crashes at boot (Debug masks it).
**Why it happens:** PS 5.1 prepends a BOM; `ConfigFile::Section::getName` ACs on it.
**How to avoid:** Use Edit/Write tools or bash (BOM-safe) to set `rasterMajor` in `client_d.cfg` for the smoke. [CITED: AGENTS.md §Config safety, memory `reference_client_cfg_bom_release_crash`]
**Warning signs:** `SwgClient_r.exe` won't start, 26-byte `(null)-unknown.*.txt` dump.

## Code Examples

(All verified patterns appear in Architecture Patterns 1–6 above, sourced to exact file:line from the tree.)

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| x87 `fnstcw`/`fldcw` inline asm | `_control87`/`_controlfp` | x64 (MSVC dropped inline asm) | Mandatory; x87 precision control gone on x64 |
| SSE inline `__asm{}` | `_mm_*` intrinsics | x64 | Mandatory; intrinsics compile both bitnesses |
| `naked` + `rdtsc` | `__rdtsc()` | x64 | `__declspec(naked)` with asm is x64-illegal |
| EIP/ESP/EBP register grab | `RtlCaptureContext` | x64 | Register set + sizes changed (Rip/Rsp/Rbp, 64-bit) |
| `(int)pointer` | `uintptr_t`/`intptr_t` | LP64/LLP64 era | Pointer no longer fits in `int`/`long` |

**Deprecated/outdated:**
- `__asm` (MSVC inline assembly): unsupported on x64; `__asm` use is always **error C4235** under x64 compile. [VERIFIED: web search + learn.microsoft.com C4235]
- `__control87_2`: hard compiler error on x64.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | TargaHeader is 18 bytes / TargaFooter 26 bytes on the wire (the static_assert baseline values) | Pattern 6 | Low — the planner/executor must compute the actual 32-bit `sizeof` at build time and baseline the assert to *that*; the numbers here are the standard TGA spec values and should match but must be confirmed by the compiler, not trusted from this doc. |
| A2 | The `Archive` `std::map` size_t path is the *only* raw-`size_t`-over-the-wire serializer in the in-scope build path | BITS-03 | Medium — the scratch x64 compile will surface any other `get(source, <size_t>)`/`put(target, <size_t>)` call as an overload-resolution failure (no 64-bit overload). The compile is the backstop; this audit found only the map path by reading Archive.h, but did not exhaustively grep every `Archive::get` caller. |
| A3 | No third-party header in the in-scope link set forces a layout change that the scratch compile-only harness can't see (because compile-only skips link) | D-01 harness | Low-Medium — compile-only catches source-level width defects; anything that only manifests at link/runtime against an x64 third-party lib is by design deferred to Phase 33 (D-02 residual worklist). |

## Open Questions (RESOLVED)

> All three questions below carry an inline `Recommendation:` that resolves them; the plans implement those resolutions (31-05 reads the live 32-bit `sizeof` rather than trusting doc literals; the scratch x64 compile is the authoritative serialization-width backstop; DebugHelp is scoped to compile-clean via a `#if defined(_M_X64)` branch with runtime x64-backtrace validation deferred to Phase 33/36). No open ambiguity remains for planning.

1. **Exact `static_assert` baseline sizes for packed structs.**
   - What we know: TargaHeader/TargaFooter and AssetCustomizationManager's `IntRange`/etc. are fixed-width (no pointer/size_t members) → layout-stable across bitness.
   - What's unclear: the precise byte counts to bake into the asserts.
   - Recommendation: have the executor emit `static_assert(sizeof(X)==sizeof(X))`-style by first compiling a one-off `printf(sizeof)` or reading the 32-bit `sizeof` from a quick compile, then hard-code the literal. Trust the compiler, not this doc (A1).

2. **Whether any sharedNetworkMessages serializer uses `size_t`/pointer width beyond the Archive map path.**
   - What we know: IFF and the vector/set/deque Archive paths are fixed-width (`signed int length; get(&length,4)`). The map path uses raw `size_t`.
   - What's unclear: whether any hand-rolled message struct does a `memcpy`-blob of a struct containing a pointer/size_t (none found in the spot-checks, but not exhaustive).
   - Recommendation: the scratch x64 compile is the exhaustive backstop — a raw-`size_t` `Archive::get` call won't resolve to a 64-bit overload (none exists) and will surface as an error. Treat the compile output as the authoritative serialization-width worklist.

3. **DebugHelp stack-walk: minimum viable Phase-31 scope.**
   - What we know: the asm is inherently bitness-specific (Eip/Esp/Ebp vs Rip/Rsp/Rbp; I386 vs AMD64 machine type).
   - What's unclear: whether to fully implement the x64 walk now or just make it compile (the x64 walk only matters once Phase 33 produces an x64 binary).
   - Recommendation: make it *compile* x64-clean via a `#if defined(_M_X64)` branch using `RtlCaptureContext` + AMD64 fields; full runtime validation of the x64 backtrace can ride into Phase 33/36. This is the one place a bitness branch is justified (the others are no-`#ifdef` per D-05).

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| MSBuild (VS18/v145) | scratch x64 compile + 32-bit rebuild | ✓ | v145 (`$env:MSBUILD`) | — |
| `cl.exe` x64 cross-compiler | scratch `Debug\|x64` compile-only | ✓ (ships with VS18 BuildTools) | matches v145 | — |
| Intrinsic headers (`<intrin.h>`,`<xmmintrin.h>`,`<float.h>`) | all intrinsic replacements | ✓ | toolchain | — |
| Dual-renderer boot smoke (existing) | D-08 regression proof | ✓ | in-place | — |
| x64 third-party libs (dpvs/bink/pcre/…) | NOT needed (compile-only, no link) | ✗ | — | by design deferred to Phase 33 (D-02) |

**Missing dependencies with no fallback:** None for Phase 31 (compile-only deliberately sidesteps the x64 third-party gap).

**Missing dependencies with fallback:** x64 third-party libs are absent but intentionally not required — D-01's compile-only harness needs no link step; their resolution is Phase 33 (X64-04).

## Validation Architecture

> nyquist_validation is enabled (config `workflow.nyquist_validation: true`). This phase is C++ source surgery with no unit-test framework in the tree — validation is **compiler-driven + boot-smoke + compile-time asserts**, not a pytest/jest suite. The "test framework" is the toolchain itself.

### Test Framework
| Property | Value |
|----------|-------|
| Framework | MSVC compiler (cl.exe) as the assertion engine — no xUnit harness exists in this repo |
| Config file | scratch `x64-scratch/x64-compile.props` (uncommitted) for the x64 worklist; committed `.vcxproj`s for the 32-bit rebuild |
| Quick run command | `cl /c /we4311 /we4312 /we4244 <TU>` (scratch x64) per touched translation unit |
| Full suite command | `$env:MSBUILD swg.sln /t:Direct3d11;Direct3d9;Direct3d9_ffp;Direct3d9_vsps;SwgClient /p:Configuration=Debug /p:Platform=Win32 /nodeReuse:false` then dual-renderer boot smoke |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| BITS-01 | No x64-illegal `__asm` survivors in build path | compile (x64 scratch) | scratch x64 `cl /c` of each in-scope lib → grep log for C4235 (must be 0) | ✅ harness = Wave 0 |
| BITS-01 | FPU/SSE/asm replacements preserve 32-bit behavior | smoke | 32-bit build + dual-renderer boot to char-select | ✅ existing |
| BITS-02 | No pointer/int truncation survivors | compile (x64 scratch) | scratch x64 `cl /c /we4311 /we4312 /we4244` → grep log for C4311/C4312/C4244 (must be 0) | ✅ harness = Wave 0 |
| BITS-03 | Packed/serialized struct sizes unchanged | compile-time assert | 32-bit build compiles with the new `static_assert(sizeof==N)` green | ✅ asserts added in-phase |
| BITS-03 | Assets load + saved data + network parse | smoke | boot to char-select (exercises TRE/IFF load + login handshake) under both renderers | ✅ existing |
| SC#4 | 32-bit boots both renderers after edits | smoke | `rasterMajor=5` then `=11` in `client_d.cfg`, Debug exe, boot to char-select | ✅ existing |

### Sampling Rate
- **Per task commit:** scratch x64 `cl /c` of the touched TU(s) → 0 C4235/C4311/C4312/C4244; 32-bit build of the owning lib stays clean.
- **Per wave merge:** full canonical 5-target 32-bit build (`/nodeReuse:false`), link-grep `unresolved external symbol` = 0.
- **Phase gate:** all in-scope libs compile x64-clean in the scratch harness (residual deferrals documented for Phase 33) + dual-renderer boot smoke green + all new `static_assert`s pass.

### Wave 0 Gaps
- [ ] `src/build/win32/x64-scratch/x64-compile.props` — the D-01 scratch x64 compile-only config (uncommitted, gitignored). Sets x64 target, `/we4311 /we4312 /we4244`, `PLATFORM_WIN32` + `_USE_32BIT_TIME_T=1` preprocessor parity, in-scope include dirs.
- [ ] `src/build/win32/x64-scratch/compile-all.ps1` — iterate the in-scope lib TU list, `cl /c` each, collect C4235/C4311/C4312/C4244 into the worklist.
- [ ] In-scope lib/TU manifest — the explicit list of `.cpp` to feed the harness (see Build-Graph Scope below).
- [ ] No xUnit framework needed/added — the compiler + boot smoke + static_assert are the validation surface for C++ source-correctness work.

## Build-Graph Scope (D-03 — in-scope libs to feed the scratch harness)

**In scope (SwgClient boot path + gl05/06/07/11 + the engine libs they link):**
- `sharedFoundation` (FloatingPointUnit, Os, Fatal, Clock) — **has BITS-01 + BITS-02 sites**
- `sharedMath` (SseMath ×13, Transform naked-SSE) — **has BITS-01 (the hard chunk)**
- `sharedCollision` (CollisionUtils fsqrt) — **has BITS-01**
- `sharedMemoryManager` (MemoryManager ptr math) — **has BITS-02; its `__asm int 3` is already commented out → no BITS-01 work**
- `sharedDebug` (DebugHelp stack-walk, ProfilerTimer rdtsc, LeakFinder) — **has BITS-01 + BITS-02; LeakFinder already uses the correct `uintptr_t` pattern**
- `sharedFile` (Iff) — BITS-03 audit; **fixed-width by design → SAFE, add static_asserts only if a packed struct is touched**
- `sharedImage` (TargaFormat `#pragma pack`) — BITS-03 static_assert candidate (layout-safe)
- `sharedGame` (AssetCustomizationManager `#pragma pack`) — BITS-03 static_assert candidate (layout-safe)
- `clientGame` (VeCritsec HTTPpost spinlock) — **has BITS-01**
- `clientGraphics` (StaticShader sort-key, ShaderEffect, RenderWorld) — **has BITS-02**
- `clientSkeletalAnimation` (SoftwareBlendSkeletalShaderPrimitive — consumes SseMath) — BITS-02 + SSE consumer
- `external/ours/library/archive` (Archive.h map size_t) — **has BITS-03 (the real serialization hazard)**
- renderer plugins `Direct3d9` (gl05/06/07) + `Direct3d11` (gl11) — **have BITS-02** (StaticVertexBufferData / DynamicVertexBufferData / StaticShaderData pointer→int casts)
- transitive engine libs they link (sharedNetwork, sharedNetworkMessages, sharedUtility, sharedTerrain, sharedObject, etc.) — feed to the harness; the compiler surfaces any survivors.

**Explicitly OUT (D-03):**
- `DllExport.cpp` (91 `__asm int 3`), `ShaderBuilder` (TextureView/StageView/ShaderBuilderDoc), `Headless`, AnimationEditor/ParticleEditor/SwgGodClient, TerrainEditor, TextureBuilder, MayaExporter, Viewer, WorldSnapshotViewer, Miff, Turf — editor/tool apps, MSB8066-pre-broken, not the boot invariant.
- `external/3rd/library/*` third-party asm (dpvs ×~26, miles, atlmfc, soePlatform AES) — third-party, x64 resolution is Phase 33 (X64-04); not our source to port.
- `external/ours/application/LagOMatic` — tool, OUT.

## `__asm` Survivor Inventory (verified by reading the tree)

> Tree-wide grep found **169 `__asm` occurrences across 29 files**. After excluding third-party (`external/3rd/*`) and editor apps (D-03), the **in-scope** survivors are:

| # | File:line | Category | What the asm does | x64 replacement | Live? |
|---|-----------|----------|-------------------|-----------------|-------|
| 1 | `sharedFoundation/src/win32/FloatingPointUnit.cpp:119` | x87 FPU-control (D-04) | `fnstcw controlWord` — read FP control word | `_control87(0,0)` (read) | yes |
| 2 | `sharedFoundation/src/win32/FloatingPointUnit.cpp:128` | x87 FPU-control (D-04) | `fldcw controlWord` — write FP control word | `_control87(cw, _MCW_RC \| _MCW_EM)`; skip `_MCW_PC` on x64 | yes |
| 3–15 | `sharedMath/src/win32/SseMath.cpp` (13 `__asm{}` blocks across 5 fns: `canDoSseMath`, `rotateTranslateScale_l2p`, `rotateScale_l2p`, `skinPositionNormal_l2p`, `skinPositionNormalAdd_l2p`) | real SSE (D-05) | CPUID feature check + 4 matrix-transform-multiply-scale routines (load 3×4 matrix, broadcast scale, `mulps`, horizontal-sum) | `__cpuid`/`__cpuidex` for the feature check; `_mm_load_ps`/`_mm_mul_ps`/`_mm_set1_ps`/`_mm_shuffle_ps`/`_mm_add_ps` for the math | **yes** — gated behind `canDoSseMath()`, drives skeletal skinning + matrix concat |
| 16 | `sharedMath/src/shared/Transform.cpp:280` | real SSE — **`__declspec(naked)`** (D-05, under-stated in CONTEXT) | `sse_xf_matrix_3x4` — full 3×4 matrix concatenation in naked SSE asm | rewrite as a normal `_mm_*` function (drop `naked`); register-faithful | **yes** — `Transform::install` selects it when SSE available (line 76-78) |
| 17 | `sharedCollision/src/shared/core/CollisionUtils.cpp:427-429` | x87 (misc sweep) | `fld t; fsqrt; fstp t` — scalar sqrt in `faster_normalize` | `t = sqrtf(t);` (or `_mm_sqrt_ss`) | yes (inline normalize) |
| 18 | `sharedFoundation/src/shared/Fatal.cpp:174` | `int 3` (misc sweep) | breakpoint trap on fatal | `__debugbreak();` | yes (`_WIN32` guard) |
| 19 | `sharedFoundation/src/shared/Clock.cpp:270` | `int 3` (misc sweep) | breakpoint on frame-time anomaly | `__debugbreak();` | debug-report path |
| 20 | `sharedMemoryManager/src/shared/MemoryManager.cpp:2074` | `int 3` (misc sweep) | **ALREADY COMMENTED OUT** (`// __asm int 3;`) | none — no work | n/a |
| 21 | `sharedDebug/src/win32/DebugHelp.cpp:503-511` | register grab (misc sweep) | capture Eip/Esp/Ebp into CONTEXT for stack walk | `RtlCaptureContext` + AMD64 fields under `#if _M_X64` | yes (crash backtrace) |
| 22 | `sharedDebug/src/win32/ProfilerTimer.cpp:29-36` | **`__declspec(naked)`** rdtsc (misc sweep) | `rdtsc; ret` cycle counter | `return __rdtsc();` (drop naked) | yes (profiler) |
| 23 | `clientGame/src/shared/HTTPpost/VeCritsec.hpp:43-48` | `lock bts` spinlock (misc sweep) | atomic test-and-set lock acquire | `_interlockedbittestandset((long*)&m_iLock, 0)` | yes (HTTP worker) — note the `#else` branch already has a GCC `__asm__` variant; the `#ifdef _WIN32` MSVC branch is the one to port |

> **Correction to CONTEXT.md note:** D-05 says "13 real `__asm{}` SSE blocks" in SseMath — confirmed (13 `__asm` blocks), but they live in **5 functions**, one of which (`canDoSseMath`) is a CPUID check, not math; the 4 math routines are the faithful-translation targets. Additionally, **Transform.cpp's site (#16) is a `__declspec(naked)` SSE function** — a 14th SSE asm body that is the *primary* matrix-multiply path, arguably harder than any single SseMath block; the planner should scope it as a peer of the SseMath work, not a trivial "misc sweep" line.

## BITS-02 Truncation-Defect Sample (representative, NOT exhaustive — compiler is authoritative)

| File:line | Defect | Class | Fix |
|-----------|--------|-------|-----|
| `clientGraphics/src/shared/StaticShader.cpp:289` | `return reinterpret_cast<int>(&getStaticShaderTemplate());` — pointer→int sort key | C4311 | `intptr_t` return, or hash the pointer; sort key must stay unique on x64 |
| `sharedMemoryManager/.../MemoryManager.cpp:405` | `reinterpret_cast<int>(next) - reinterpret_cast<int>(this)` — ptr diff via int | C4311 | subtract pointers (`ptrdiff_t`) or cast via `intptr_t` |
| `sharedMemoryManager/.../MemoryManager.cpp:1329,1426,1753` | `reinterpret_cast<int>(memory)` for `%08x` logging / block math | C4311 | `uintptr_t` + `%p`/`%zx` |
| `sharedFoundation/src/win32/Os.cpp:1579` | `reinterpret_cast<int>(ShellExecute(...))` — HINSTANCE→int | C4311 | `intptr_t`/`INT_PTR` |
| `sharedDebug/.../LeakFinder.h:81` | (reference: already correct) `reinterpret_cast<uintptr_t>(p)` | — | this is the **template to copy** |

**The toolbox (D-06 discretion):**
- pointer stored/hashed as integer → `uintptr_t`
- signed pointer math / difference → `intptr_t` or `ptrdiff_t`
- Win32 API boundary (window longs, ShellExecute) → `DWORD_PTR` / `INT_PTR` / `LONG_PTR`
- container/buffer size → `size_t`
- **never** `(int)(intptr_t)ptr` (re-truncates) and **never** `#pragma warning(disable)`.

## BITS-03 Serialization/Layout Audit (verified)

**SAFE by design (skip per D-07):**
- `Iff` reader: every primitive read is fixed-width — `read_int8/16/32`, `read_uint8/16/32`, `read_float`, `Tag`=uint32 via `memcpy(&t,...,sizeof(Tag))`, `isizeof(Tag)`/`isizeof(uint32)`. No `read_int64`, no pointer/size_t in the wire path. [VERIFIED: Iff.h:195-224, Iff.cpp:537-555]
- Engine fundamental types: `int32`/`uint32` typedef to `signed long`/`unsigned long` → **32-bit on Windows LLP64** (long doesn't widen). So all IFF/network fields declared as `int32`/`uint32` stay 4 bytes. [VERIFIED: FoundationTypesWin32.h:21-28]
- `Archive` vector/set/deque: `signed int length; source.get(&length, 4)` — hardcoded 4 bytes. [VERIFIED: Archive.h:130-174]
- `Archive` `long`/`unsigned long` get/put: read/write exactly 4 bytes (`source.get(&target, 4)`) — matches LLP64 `long`=32-bit. [VERIFIED: Archive.h:33-43, 232-242]

**HAZARD (the real BITS-03 work):**
- `Archive` `std::map` get/put uses raw **`size_t numKeys`** (Archive.h:193, 382). On 32-bit `size_t`→`unsigned int` overload (4 bytes); on x64 `size_t` is 8 bytes with **no 64-bit Archive overload existing** → overload-resolution failure / wire-width change. **Fix: pin to `uint32_t`** (Pattern 5) — keeps the wire format identical to 32-bit. This is the one serializer that changes width on x64.

**`#pragma pack` structs (only 2 in the engine tree — both layout-safe, need static_assert guards per D-08):**
- `sharedImage/.../TargaFormat.cpp:61-88` — `TargaHeader` (uint8/uint16 fields, ~18 bytes), `TargaFooter` (uint32×2 + char[N], ~26 bytes). Read via `fileInterface->read(&header, sizeof(header))`. Fixed-width → x64-safe; add `static_assert(sizeof==N)`. [VERIFIED: source read]
- `sharedGame/.../AssetCustomizationManager.cpp:47-98` — `IntRange`/etc. (uint16/int fields). Fixed-width → x64-safe; add `static_assert`. [VERIFIED: source read]

> `time_t` (D-07): governed by the existing `_USE_32BIT_TIME_T=1` build define (verified in `sharedFoundation.vcxproj` PreprocessorDefinitions, all 3 configs). With it set, `time_t` stays 32-bit even on x64 — so any `time_t` serialized as 4 bytes stays consistent. The scratch x64 harness MUST carry `_USE_32BIT_TIME_T=1` to preserve this parity, else `time_t` widens to 8 bytes and breaks size asserts. **Carry the flag in the harness props.**

## Scratch x64 Compile-Only Harness — concrete HOW (D-01)

**Goal:** Make `cl.exe` emit C4235 (asm) + C4311/C4312/C4244 (truncation, as errors) for the in-scope libs targeting x64, **without** touching committed `swg.sln`/`.vcxproj` and **without** linking (so no x64 third-party libs needed).

**Cleanest approach (recommended):** an uncommitted PowerShell driver + a shared props snippet, invoking the x64 `cl.exe` per translation unit with `/c` (compile-only). This avoids generating any committed project artifact.

Key requirements the harness must replicate from the committed 32-bit build:
- **Target x64:** invoke the `Hostx64\x64` (or `Hostx86\x64`) `cl.exe` — sets `_M_X64`, makes `__asm` → C4235.
- **`/c` only** — no `/link`, no `.lib` output dependency → third-party x64 libs irrelevant.
- **Warnings-as-errors (D-06):** `/we4311 /we4312 /we4244`.
- **Language + defines parity:** `/std:c++20`, `/D PLATFORM_WIN32 /D WIN32 /D _USE_32BIT_TIME_T=1 /D _CRT_SECURE_NO_DEPRECATE=1 /D _DEBUG /D DEBUG_LEVEL=2 /D _LIB`.
- **Include dirs:** mirror each lib's `AdditionalIncludeDirectories` (long list per `.vcxproj`); easiest to read them out of the existing `.vcxproj` per lib.
- **PCH:** disable PCH for the scratch compile (`/Y-`) to avoid PCH-mismatch noise — each TU compiles standalone.

Sketch (illustrative — planner turns into discrete tasks):
```powershell
# src/build/win32/x64-scratch/compile-all.ps1   (UNCOMMITTED, gitignored)
$cl = "<VS18>\VC\Tools\MSVC\<ver>\bin\Hostx64\x64\cl.exe"
$common = '/c /std:c++20 /EHsc /Y- /we4311 /we4312 /we4244',
          '/D PLATFORM_WIN32 /D WIN32 /D _USE_32BIT_TIME_T=1 /D _CRT_SECURE_NO_DEPRECATE=1 /D _DEBUG /D DEBUG_LEVEL=2 /D _LIB'
foreach ($tu in $inScopeTranslationUnits) {
    & $cl $common $tu.Includes $tu.Path 2>&1 | Tee-Object -Append worklist.log
}
Select-String 'error C4235|error C4311|error C4312|error C4244' worklist.log   # = the worklist
```

**Alternative (heavier):** copy `swg.sln` + the in-scope `.vcxproj`s into `x64-scratch/`, add an `x64` ProjectConfiguration to the *copies* only, set the warning flags + `ConfigurationType=StaticLibrary` (compile, no link of the app), and build with MSBuild `/p:Platform=x64`. More faithful to the real build (gets exact include/PCH handling) but generates more files to keep uncommitted. The PowerShell-driver approach is cleaner for "signal generator, not deliverable" (CONTEXT.md).

**Keeping it off the committed tree:** put everything under `src/build/win32/x64-scratch/`, add that path to a local git exclude (or a gitignored line). Verify before any commit that `git status` shows no x64-scratch files and `swg.sln` is unmodified (grep it for `x64` = 0, as confirmed today).

**Residual worklist (D-02):** anything the compile-only harness *cannot* prove (e.g. a TU that won't compile without an x64 third-party header, or runtime-only x64 behavior) is written into a residual artifact (planner's discretion on format) and handed to Phase 33. Phase 31's bar is "in-scope libs compile x64-clean in the scratch config," not "100% of the build path."

## Project Constraints (from AGENTS.md / CLAUDE.md)

- **Builds run from PowerShell** (not Git Bash — MSYS mangles `/t:` `/p:`), using `$env:MSBUILD` (VS18/v145). Always `/nodeReuse:false`.
- **Canonical 5-target build:** `Direct3d11;Direct3d9;Direct3d9_ffp;Direct3d9_vsps;SwgClient`; postbuild auto-copies `gl0X_d.dll` + `SwgClient_*.exe` to `stage/`.
- **`/FORCE` link masks unresolved externals** → grep build log for `unresolved external symbol` (must be 0); exit 0 ≠ clean link. Delete the exe to force a relink.
- **Shared-header ABI cascade:** touching a public struct layout in a shared header breaks stale plugin DLLs → boot crash; rebuild ALL plugin `.vcxproj`s (gl05/06/07/11). `static_assert` additions are layout-safe.
- **`.cfg` BOM trap:** never write `stage/*.cfg` with PS `Set-Content`/`Out-File` (BOM crashes Release boot); use Edit/Write/bash. Debug exe reads `client_d.cfg`; set `rasterMajor` there for the smoke.
- **Validation bar = SwgClient clean + dual-renderer boot**, NOT a green full-solution build (editor apps are MSB8066-pre-broken and OUT).
- **Don't modify Koogie's source-level changes** without strong reason.
- **Commit only when asked; push to `origin` only, never `koogie`.**

## Sources

### Primary (HIGH confidence)
- Source tree (verified by reading): `FloatingPointUnit.cpp`, `SseMath.cpp`, `Transform.cpp`, `CollisionUtils.cpp`, `Fatal.cpp`, `Clock.cpp`, `MemoryManager.cpp`, `DebugHelp.cpp`, `ProfilerTimer.cpp`, `VeCritsec.hpp`, `Iff.h`/`Iff.cpp`, `Archive.h`/`ByteStream.h`, `FoundationTypesWin32.h`, `FirstPlatform.h`/`PlatformGlue.h`, `TargaFormat.cpp`, `AssetCustomizationManager.cpp`, `StaticShader.cpp`, `Os.cpp`, `LeakFinder.h`, `sharedFoundation.vcxproj`, `swg.sln`.
- [CITED: learn.microsoft.com/en-us/cpp/c-runtime-library/reference/control87-controlfp-control87-2] — `_control87`/`_controlfp`/`__control87_2` x64 behavior: `_MCW_RC` + `_MCW_EM` supported on x64 (MXCSR), `_MCW_PC` + `_MCW_IC` NOT supported (assertion + invalid-parameter handler), `__control87_2` is a compiler error on x64; `_control87` vs `_controlfp` differ on DENORMAL mask.

### Secondary (MEDIUM confidence)
- [VERIFIED: WebSearch + learn.microsoft.com C4235] — `__asm` is always **error C4235** on x64; recommended replacements are MASM or compiler intrinsics; `__rdtsc`/`_InterlockedBitTestAndSet` available as x64 intrinsics.

### Tertiary (LOW confidence)
- None — all load-bearing claims are tool-verified or cited.

## Metadata

**Confidence breakdown:**
- Standard stack (intrinsics/CRT): HIGH — MS docs cited; all facilities ship with the toolchain.
- `__asm` inventory: HIGH — every in-scope site read at file:line; MemoryManager site confirmed already-commented-out; Transform naked-SSE site confirmed live.
- BITS-02 truncation: HIGH (method) / MEDIUM (completeness) — representative defects verified; the compiler-driven worklist is the authoritative enumerator (by design — A2/Open-Q2).
- BITS-03 serialization: HIGH — IFF fixed-width confirmed, `int32`=`long`=32-bit-on-LLP64 confirmed, Archive map size_t hazard confirmed, the 2 `#pragma pack` structs confirmed layout-safe.
- FPU port semantics (D-04): HIGH — MS docs explicitly state the `_MCW_PC` x64 assertion behavior (correcting the "silent no-op" framing).

**Research date:** 2026-06-15
**Valid until:** ~2026-07-15 (stable — toolchain + MS CRT behavior + this source tree change slowly; re-verify only if the toolchain version or the committed tree shifts materially).
