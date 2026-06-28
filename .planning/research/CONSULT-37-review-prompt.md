# Cross-AI Plan Review Request — swg-client-v2 Phase 37

You are independently reviewing implementation plans for a software phase in the repo at
`D:/Code/swg-client-v2`. You have READ access to the repo — open any file you need to verify a claim.
Be a skeptical, adversarial reviewer: your job is to find what is wrong, missing, or risky, not to
praise. A plan that survives your review is more robust.

## Project context (what this client is)

`swg-client-v2` is a from-source build of the Star Wars Galaxies game client (MSBuild, MSVC toolset
v145 / VS2026, C++20). Phase 37 is a **standalone post-v3.0 integration** with an external tool called
**Utinni** (a 32-bit in-process ImGui modding overlay that is injected into the live client). 32-bit
ONLY (x64 is explicitly out of scope).

### The problem Phase 37 solves
Utinni today reaches ~230 engine functions on the SWGEmu Pre-CU client by **hardcoded RVAs** (literal
addresses baked into Utinni). Those literals match the SWGEmu binary byte-for-byte but match NOTHING in
this from-source `SwgClient_r.exe`, so injecting Utinni crashes on the very first detour
(`0xC0000005 … target=0x00401000`, the `config::loadOverrideConfig` hook).

### The deliverable
Add ONE new undecorated exported C function to the SwgClient exe —
`extern "C" __declspec(dllexport) const UtinniEngineHookPoints* __cdecl GetEngineHookPoints()` — that
returns a `{name, void* addr}` table of engine functions/globals, each row populated at COMPILE TIME by
`&EngineSymbol`. Utinni resolves hook points by name instead of by hardcoded RVA. The client stays
fully Utinni-agnostic (pure read-only getter, inert when not injected). This mirrors an
already-shipped graphics-side twin: `gl11_r.dll!GetHookPoints` at
`src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp:856-888`.

### Requirements addressed
- **EPA-02** — advertised-path discovery / first-detour crash fixed (the `config::loadOverrideConfig`
  row, mapped to a thunk wrapping the exe's `installConfigFileOverride()` — NOT `ConfigFile::loadFromBuffer`).
- **EPA-03** — DX11 overlay kickoff resolves off the contract (the `graphics::*` rows).
- **EPA-04** — graceful degradation + a coverage gate (zero-missing / no-duplicate on the required name set).

## Repo facts you should treat as established (verify if you doubt them)
- `__declspec(dllexport)` on an `extern "C" __cdecl` function ALONE forces an undecorated export — no
  `.def`, no `/EXPORT` pragma (proven by the shipped gl11 twin; `dumpbin` shows `GetHookPoints = _GetHookPoints`).
- SwgClient links under `/FORCE` (`ForceFileOutput Enabled`), so a typo'd / unresolved `&Class::method`
  becomes a LINK WARNING, not an error — the gate is `grep "unresolved external symbol" == 0`, NOT exit code.
- All game libs (clientGame, sharedObject, clientUserInterface, sharedCommandParser, …) are already in
  the exe link closure; the include path already has every `include/public` dir + `src/shared` + `src/win32`.
- 3 Win32 flavors: Debug (_d), Optimized (_o), Release (_r). PCH is `FirstSwgClient.h` (Use mode).
- Touching a shared header consumed by the renderer plugins (gl05/06/07/11) causes a stale-DLL ABI
  crash cascade — the plan keeps the new header exe-local to avoid this.

## The C++ "landmines" the plan must handle per-row (do NOT bulk `&fn`)
- **virtual** member → `&Class::virtualMethod` yields a vtable-dispatch thunk under MSVC, NOT the real
  code address. Plan's rule: SKIP (Utinni resolves off the live vtable), document, do not advertise.
- **constructor** → `&Class::Class` is ill-formed; needs a free-function thunk.
- **private** member → not addressable externally; needs friend/accessor or skip.
- **overloaded** → needs explicit `static_cast<Ret(*)(Args)>(&fn)`.
- **pointer-to-member-function** (non-static, non-virtual) → converted to `void*` via a `union` helper
  guarded by `static_assert(sizeof(PMF)==sizeof(void*))`.
- **private-static "globals"** → advertise an accessor address, not the private member; else omit.

## The plans to review (read them in full from disk)
- `.planning/phases/37-utinni-engine-entry-point-advertisement-getenginehookpoints/37-01-PLAN.md`
  — P0 contract spike: shared header + `.inc` X-macro, the undecorated export, PMF→void* helper, the
  `installConfigFileOverride` exposure, the coverage self-check scaffold, ~3-5 representative rows,
  build + dumpbin + boot gate.
- `.../37-02-PLAN.md` — P1 MVP catalog (~70 endpoints), MISMATCH corrections, landmine handling,
  full 5-target build, dual-renderer boot.
- `.../37-03-PLAN.md` — P2 full catalog (~230 total), globals, ctor thunks, virtual skips, §8
  mid-function-patch triage, 3-flavor build.

## Supporting artifacts (read as needed)
- `.../37-RESEARCH.md` (findings, symbol resolution table, landmine analysis)
- `.../37-PATTERNS.md` (in-tree analogs)
- `.../37-VALIDATION.md` (validation strategy — build/link/dumpbin/static_assert driven; no unit-test framework)
- `.planning/handoff/2026-06-20-utinni-engine-entrypoint-advertisement-spec.md` (the canonical external contract spec from Utinni)
- The shipped analog to mirror: `src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp:856-888`

## Your review — output in markdown with these sections

1. **Summary** — one-paragraph assessment of the 3-plan set.
2. **Strengths** — what is well-designed (bullets).
3. **Concerns** — issues / gaps / risks, each tagged severity **HIGH / MEDIUM / LOW**. Be specific:
   cite the plan + task + the exact claim or symbol you doubt.
4. **Suggestions** — concrete, actionable improvements.
5. **Risk Assessment** — overall LOW / MEDIUM / HIGH with justification.

Focus especially on:
- **Correctness of the C++ mechanism**: Is the `union`-based PMF→void* conversion actually well-defined
  /reliable under MSVC v145 32-bit for the cases used? Does the `static_assert` coverage bridge (table
  row count == `.inc` count) actually catch the drift it claims to? Can a virtual-as-`&fn` bug slip past it?
- **Symbol-resolution risk**: the plan asserts many engine symbols and `file:line` (`Game::install` @
  Game.h:94, `CommandParser::addSubCommand` @ CommandParser.h:149, the 8 MISMATCH corrections, etc.).
  Spot-check whether these symbols actually exist with the claimed signature/static-ness/virtual-ness in
  this repo's source. A WRONG `&` is worse than a missing row.
- **The undecorated-export claim** under 32-bit MSVC from an EXE (not a DLL): will `__declspec(dllexport)`
  alone really produce `GetEngineHookPoints` undecorated, or is the `/EXPORT` pragma fallback actually needed?
- **Coverage-gate logic**: the PATTERNS file shows a `static_assert(... - 1 == UTINNI_REQUIRED_COUNT ...)`
  with a `-1`, while 37-01 shows it without. Is the count bridge correct, or off-by-one / brittle?
- **Dependency ordering & sequencing** across 37-01 → 37-02 → 37-03; scope creep / over-engineering;
  anything that could break the boot gate or the renderer plugins.
- **Verifiability gap**: the real acceptance (first-detour crash gone, overlay renders) is OUT OF REPO
  (needs live Utinni injection). Is what's left actually a meaningful gate, or is the phase declaring
  victory on by-construction checks that could still ship a wrong table?
