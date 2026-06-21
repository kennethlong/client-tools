---
phase: 37-utinni-engine-entry-point-advertisement-getenginehookpoints
reviewed: 2026-06-21T00:00:00Z
depth: standard
files_reviewed: 6
files_reviewed_list:
  - src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.h
  - src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.inc
  - src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp
  - src/game/client/application/SwgClient/src/win32/ClientMain.cpp
  - src/game/client/application/SwgClient/src/win32/ClientMain.h
  - src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj
findings:
  critical: 0
  warning: 5
  info: 3
  total: 8
status: issues_found
---

# Phase 37: Code Review Report

**Reviewed:** 2026-06-21
**Depth:** standard
**Files Reviewed:** 6
**Status:** issues_found

## Summary

Reviewed the `GetEngineHookPoints()` advertisement implementation (provider TU
`utinni_advertise.cpp`, the shared `.h`/`.inc` contract, the `ClientMain.cpp`
shim + boot call site, and the vcxproj Win32-scoping).

The high-risk axes called out in the domain context all hold up under tracing:

- **PMF inflation / calling convention:** Every class whose member PMF is
  `bit_cast` is single- or zero-inheritance — verified `Object` (no base),
  `Camera : public Object` (single), `CuiIoWin : public IoWin` (single),
  `CuiConsoleHelper : public UIEventCallback` (single), `CommandParser` (no
  base), `BaseExtent` (no base), `SharedObjectTemplate : public ObjectTemplate`
  (single). None inflate, so `static_assert(sizeof(PMF)==sizeof(void*))` passes
  and the guard remains a live regression net. The MI classes (GroundScene,
  chatWindow) are correctly OMITTED, not bit_cast.
- **Virtual rows:** All advertised `Object`/`Camera`/`BaseExtent`/`CommandParser`
  members traced as confirmed **non-virtual** (the virtual siblings —
  `addToWorld`, `removeFromWorld`, `setParentCell`, `BaseExtent::realIntersect`,
  `CuiIoWin::processEvent` — are correctly skipped). No `&Class::virtualMethod`
  vtable-thunk landmine present.
- **Overload casts:** `Game::setScene`, `Graphics::present`/`presentWindow`,
  `Object::getAppearance`, `ObjectTemplate::createObject`, `Camera::setViewport`,
  `Camera::reverseProjectInViewportSpace`, `BaseExtent::intersect` all resolve to
  the cited overloads (signatures verified against headers).
- **Coverage gate:** `.inc` (78 names) and the table (78 rows) match exactly —
  zero missing, zero extra, zero duplicate names. The compile-time count
  `static_assert` and the runtime `utinni_verifyNoNullNoDup()` set-equality check
  are internally consistent and will pass.
- **Export surface:** `extern "C" __declspec(dllexport) ... __cdecl` yields the
  undecorated `GetEngineHookPoints` name (matches the gl11 twin pattern). The
  whole body is `#if !defined(_WIN64)`-guarded AND the vcxproj excludes the TU
  from x64 (`Condition="'$(Platform)'=='Win32'"`); the boot call site is
  `#if PRODUCTION == 0 && !defined(_WIN64)`-gated. x64 cannot export the symbol
  or reference the undefined self-check.

No BLOCKER-class defects (no UB, no ABI/CC mistake, no null/dup row, no missing
guard). The findings below are accuracy/robustness WARNINGs and minor INFO items.

## Warnings

### WR-01: Five advertised `Object`/`Camera` rows are INLINE-defined but commented "non-inline" — wrong address handed to the injector

**File:** `src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp:221-222, 217, 215, 223, 247`
**Issue:** The plan's stated landmine rule is "INLINE -> OMITTED (no ODR-emitted
address, Pitfall 2)". But several rows that ARE inline-defined in `Object.h` /
`Camera.h` are advertised anyway, with comments that assert the opposite:

- `object::getPosition_w` (`Object.h:691` `inline const Vector Object::getPosition_w() const`) — comment says `const Vector getPosition_w() const`, no inline note.
- `object::setPosition_w` (`Object.h:729` `inline void Object::setPosition_w(...)`).
- `object::getNetworkId` (`Object.h:1345` `inline const NetworkId &Object::getNetworkId() const`).
- `object::getObjectTemplate` (`Object.h:484` inline).
- `object::getAppearance` non-const overload (`Object.h:592` inline).
- `camera::reverseProjectInViewportSpace` (`Camera.h:439` inline) — comment explicitly (and wrongly) labels it "OVERLOADED non-inline".

Functionally these still LINK and CALL correctly: taking `&Object::getPosition_w`
forces MSVC to emit an out-of-line copy, so `pmfToVoid` returns a valid callable
pointer. The defect is twofold: (1) the comments are factually false, eroding the
"address verified against header" trust the contract leans on; (2) the injector
receives the address of a per-TU emitted thunk, NOT a canonical engine RVA — if
Utinni ever cross-checks the address against a SWGEmu/retail RVA (the stated
purpose of several rows, e.g. `extent::intersect` "collapses the RVA split"), an
inline-emitted address will not match that RVA. For a pure call-through this is
fine; for an RVA-equivalence consumer it is a latent surprise.
**Fix:** Either correct the comments to state these are inline (address is an
emitted thunk, call-through-only — not RVA-stable), or, if RVA stability matters
for these rows, OMIT them per the same Pitfall-2 rule the other inline symbols
were dropped under. Minimum action: fix the false "non-inline" labels on
`getPosition_w`/`setPosition_w`/`reverseProjectInViewportSpace`.

### WR-02: `s_engineHookPoints[]` is dynamically (not constant) initialized — table is zero-filled until this TU's static init runs

**File:** `src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp:126-288, 290-295`
**Issue:** The header advertises the table as a "process-lifetime static" that is
"correct by construction." But every `pmfToVoid(...)` row calls a non-`constexpr`
inline (`std::bit_cast` through a runtime template), so the aggregate
`s_engineHookPoints[]` is **dynamically initialized**, not constant-initialized.
Mixed with the `(void*)&staticFn` rows, the whole array becomes a dynamic-init
object. Before this TU's dynamic initializers run, every `pmfToVoid` row holds 0.
If `GetEngineHookPoints()` (or `utinni_verifyNoNullNoDup()`) were ever reached
from another TU's static initializer (static-init-order fiasco), it would observe
NULL PMF rows and either mis-advertise or trip the null-addr branch. In the
current flow the injector calls post-boot and the self-check runs inside
`ClientMain` (well after CRT init), so this is safe TODAY — but the "correct by
construction / survives every rebuild" framing in the header overstates the
guarantee and hides the init-order dependency.
**Fix:** Make `pmfToVoid` `constexpr` (C++20 `std::bit_cast` is constexpr; the
template can be `constexpr` if not constrained by the `static_assert` placement —
it is fine) so the PMF rows become constant-initialized and the array is
statically initialized. If `constexpr` is not achievable, add a comment at the
table declaration documenting the dynamic-init dependency and that
`GetEngineHookPoints` must never be called from static-init context.

### WR-03: `utinni_verifyNoNullNoDup()` duplicate check is name-only; two rows alias the same address with no integrity check

**File:** `src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp:182, 190, 360-371`
**Issue:** `cuiManager::g_instance` and `cuiIo::g_instance` both resolve to
`&CuiManager::getIoWin` (same address, two contract names). This is intentional,
but the self-check only detects duplicate NAMES, never duplicate or
cross-suspicious ADDRESSES, and never validates that a row's address actually
falls inside the module image. A future copy/paste that points two semantically
different rows at the same wrong symbol (the classic advertisement-table bug)
would pass all three self-check phases silently. The self-check is presented
(EPA-04) as "the zero-missing gate," which implies more integrity coverage than
it provides.
**Fix:** Either document explicitly that address-aliasing is unverified by design
(accept the limitation), or extend phase (a) to also flag when two DIFFERENT
contract names share an identical non-accessor address (allow a small known
allowlist for the deliberate `getIoWin` alias).

### WR-04: `utinni_installConfigFileOverride()` swallows partial-read / null-data failure paths inherited from the crash-fixer

**File:** `src/game/client/application/SwgClient/src/win32/ClientMain.cpp:98-109, 122-125`
**Issue:** The advertised `config::loadOverrideConfig` thunk forwards to
`ClientMainNamespace::installConfigFileOverride()`. That function calls
`abstractFile->readEntireFileAndClose()` and feeds `data` straight to
`ConfigFile::loadFromBuffer(reinterpret_cast<char const*>(data), length)` with no
null check on `data`. If `readEntireFileAndClose()` returns null (read failure on
a present-but-unreadable `misc/override.cfg`), `loadFromBuffer` is handed a null
pointer with a non-zero `length`, and `delete [] data` on null is benign but the
buffer-loader may deref. This is pre-existing engine code, not introduced by this
phase — but Phase 37 now exposes this path as a PUBLICLY DETOURABLE entry point
(the single most important EPA-02 row), so an injector that calls it on demand can
hit the unguarded path far more often than the once-at-boot original caller.
**Fix:** Add `if (data)` around the `loadFromBuffer` call (and treat
`length <= 0` as a no-op). Per AGENTS.md "fix the caller/data, not the diagnostic
patch," guard at the `installConfigFileOverride` body, not by rewriting
`loadFromBuffer`.

### WR-05: `consoleHelper::sendInput` advertises a 3-arg member whose 2nd required arg is an internal recursion-guard container — likely unusable by the injector's typedef

**File:** `src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp:193`
**Issue:** The row maps the contract name `consoleHelper::sendInput` to
`&CuiConsoleHelper::processInput`, whose real signature is
`bool processInput(const Unicode::String &, stdset<Unicode::String>::fwd & recursionCheckStack, bool addToHistory = true)`
(`CuiConsoleHelper.h:76`). The second parameter is a caller-supplied recursion
tracking set, not an optional/defaulted argument. Utinni's `sendInput` typedef
almost certainly expects `(this, const String&)` or `(this, const String&, bool)`
— it has no way to know it must construct and pass a `stdset<Unicode::String>`.
Calling through this PMF with the wrong arg count is a stack/ABI mismatch at the
first detour (the exact failure class the phase set out to avoid). The PMF
address itself is valid (compiles, non-virtual, single-inheritance), so the
self-check passes — the danger is purely at the consumer call boundary.
**Fix:** Prefer advertising `CuiConsoleHelper::processCurrentInput(bool)` or
`processInput` wrapped in a `__fastcall(pThis, edx, const Unicode::String&, bool)`
thunk that allocates the recursion set internally (same pattern as the
`commandParser` ctor thunks) so Utinni's simple typedef is ABI-correct. At
minimum, document the required 3-arg signature in the row comment so the Utinni
side does not assume a 2-arg shape.

## Info

### IN-01: `utinni_strEq` reimplements `strcmp`-equality by hand

**File:** `src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp:329-341`
**Issue:** A hand-rolled byte loop duplicates `strcmp(a,b)==0` (with a null-safe
guard). It is correct, but it is dead-simple duplication of stdlib behavior in a
TU that already pulls in the CRT. The only added value is the null-tolerance.
**Fix:** Optional — `return (a && b) ? (strcmp(a,b)==0) : (a==b);`. Not worth a
change if you prefer zero CRT dependency in the contract path.

### IN-02: Self-check duplicate-name scan is O(n^2); fine at 78 rows, scales poorly as the catalog grows

**File:** `src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp:360-371, 376-410`
**Issue:** Three nested O(n*m) scans run on every Debug boot. At 78 rows this is
trivial (~6k comparisons), but the catalog is explicitly slated to grow across
waves. Not a correctness issue (and performance is out of v1 scope) — noted only
because it runs unconditionally on the boot path under `PRODUCTION==0`.
**Fix:** None required now. If the table reaches hundreds of rows, sort once and
do a linear adjacent-duplicate pass.

### IN-03: Header comment claims "every address taken at compile time by &EngineSymbol" — not true for the thunk and ctor rows

**File:** `src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.h:11-12`; `utinni_advertise.cpp:8-10`
**Issue:** Both file banners state every address is `&EngineSymbol` "taken at
compile time ... correct by construction and survives every rebuild." Several
rows are deliberately NOT direct engine-symbol addresses — they are local thunks
(`utinni_loadOverrideConfig`, `utinni_commandParserCtor1/2`) and accessor
forwards (`Game::isOver`, `Graphics::getCurrentRenderTargetWidth`, etc.). The
claim is aspirational rather than literal; combined with WR-02's dynamic-init
reality it slightly oversells the "correct by construction" guarantee.
**Fix:** Soften the banner to "each address is either `&EngineSymbol` or a
local thunk/accessor wrapping it" to match the actual table.

---

_Reviewed: 2026-06-21_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
