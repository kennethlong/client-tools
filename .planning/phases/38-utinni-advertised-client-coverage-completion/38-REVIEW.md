---
phase: 38-utinni-advertised-client-coverage-completion
reviewed: 2026-06-22T00:00:00Z
depth: standard
files_reviewed: 10
files_reviewed_list:
  - src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp
  - src/game/client/application/SwgClient/src/win32/utinni_groundScene_forward.h
  - src/game/client/application/SwgClient/src/win32/utinni_clientShims_forward.h
  - src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.h
  - src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.inc
  - src/engine/client/library/clientGame/src/shared/scene/GroundScene.cpp
  - src/engine/client/library/clientGame/src/shared/scene/GroundScene.h
  - src/engine/shared/library/sharedFoundation/src/win32/Os.cpp
  - src/engine/shared/library/sharedFoundation/src/win32/Os.h
  - src/engine/shared/library/sharedDebug/src/win32/DebugHelp.cpp
findings:
  critical: 0
  warning: 5
  info: 3
  total: 8
status: issues_found
---

# Phase 38: Code Review Report

**Reviewed:** 2026-06-22T00:00:00Z
**Depth:** standard
**Files Reviewed:** 10
**Status:** issues_found

## Summary

Phase 38 grows the exported `GetEngineHookPoints()` name->pointer table (consumed by
the external Utinni injector that detours each advertised address) and adds the
supporting MI-class thunks, private-method forwarders, win32 shims, and friend
declarations.

The HIGH-VALUE axis -- ABI / calling-convention correctness -- is **clean**. Every
`__fastcall(pThis /*ECX*/, int /*edx*/, args...)` thunk emulates `__thiscall`
correctly (dummy EDX present, post-EDX args match the real member signature). The
`client::wndProc` shim preserves `__stdcall`/CALLBACK exactly. The placement-new
ctor thunks return the constructed pointer. The friend declarations in `GroundScene.h`
and `Os.h` are ABI-neutral (no new data member, no virtual, no layout change) and
correctly guarded `#if !defined(_WIN64)`. The in-TU forwarders call their intended
private targets and have external linkage reachable from `utinni_advertise.cpp`. The
coverage lockstep holds: `.inc` count == table rowcount (94 == 94, the `static_assert`
will pass), `UTINNI_HOOKPOINTS_VERSION == 2`, no null/dup rows, the runtime name-set
check is the zero-missing gate, and every static/free-function/ctor-thunk/MI-method
row resolves to a real, non-virtual target. No row resolves to a vtable stub, and no
row crashes the injector at first detour.

The defects found are NOT crash-class. The dominant issue is a cluster of `object::*`
PMF rows that advertise the address of **header-`inline`** member functions. `&inline`
compiles and yields a valid (COMDAT, address-taken) pointer -- so there is no first-
detour crash and no null -- but a detour placed on that address will NOT intercept the
many inlined call sites throughout the engine. This directly violates the contract's
own documented invariant ("an inline has no ODR address" -> OMIT, the rule under which
`Object::move_o` was correctly excluded), and the row comments mis-cite the
declaration line rather than flag the inline definition.

## Warnings

### WR-01: Five `object::*` rows advertise the address of header-`inline` functions (Pitfall 2 violation)

**File:** `src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp:365,367,371,372,373`
**Issue:** These rows take `&` of member functions that are defined `inline` in
`Object.h`, not out-of-line in `Object.cpp`:
- `object::getObjectTemplate` (`:365`) -> `inline ... Object::getObjectTemplate() const` [Object.h:484]
- `object::getNetworkId` (`:367`) -> `inline const NetworkId &Object::getNetworkId() const` [Object.h:1345]
- `object::getPosition_w` (`:371`) -> `inline const Vector Object::getPosition_w() const` [Object.h:691]
- `object::setPosition_w` (`:372`) -> `inline void Object::setPosition_w(...)` [Object.h:729]
- `object::getAppearance` (`:373`) -> `inline Appearance *Object::getAppearance()` [Object.h:592] (both overloads are inline: 580/592)

Verified: none of these have an out-of-line definition in `Object.cpp` (only
`getParentCell`/`getTransform_o2w`/`setTransform_o2w`/`setAppearance`/`move_p`/
`getObjectTemplateName`/`getObjectType` are out-of-line there, and those rows are
fine). `&inlineFn` is well-formed (MSVC emits an address-taken COMDAT copy, so the
build does NOT break and the runtime null-check passes), but the COMDAT body is not
the code that executes at inlined call sites. An injector detouring these addresses
silently fails to intercept most calls -- exactly the failure mode the contract's own
`move_o` OMIT was written to avoid (".cpp" Pitfall 2: "an inline has no ODR address").
The row comments compound the problem by citing the *declaration* line (e.g.
`getNetworkId ... [Object.h:162]`) and not flagging the inline definition, so the
inline status is invisible to a future maintainer.
**Fix:** Treat these the same way the private GroundScene methods were treated --
route each through an out-of-line forwarder so the advertised address is the real
executed body, OR OMIT them and remove their names from the `.inc` required set
(a contract change, version bump). Example out-of-line forwarder (compiled in a `.cpp`,
declared in an exe-local forward header), mirroring the existing groundScene pattern:
```cpp
// in some sharedObject .cpp TU (where Object members are visible), #if !defined(_WIN64)
const NetworkId & __fastcall utinni_objectGetNetworkId(Object * pThis, int /*edx*/)
{
    return pThis->getNetworkId();   // forces a real, detourable out-of-line body
}
```
At minimum, if the rows are kept as-is for the "call" use case, annotate each comment
`INLINE in header -- detour will NOT intercept inlined call sites` so the limitation
is explicit.

### WR-02: `graphics::*` / `worldSnapshot::*` / `audio::*` static rows are not verified non-`inline`

**File:** `src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp:280-294,384-389,402-403`
**Issue:** The same inline-address hazard as WR-01 applies to any advertised *static*
facade function that happens to be defined `inline` in its header. The row comments
assert each is a static at a given header line but do not establish that the symbol is
out-of-line. For facade entry points (`Graphics::*`, `WorldSnapshot::*`, `Audio::*`)
the risk is lower than the `Object` getters (these are typically called cross-TU and
less aggressively inlined), but the same detour-misses-inlined-sites failure exists if
any one is header-`inline`. This was confirmed for the `Object` cluster (WR-01) and
should be confirmed -- not assumed -- for the static rows.
**Fix:** For each advertised static/PMF row, confirm an out-of-line definition exists
in the corresponding `.cpp` (grep `ClassName::method` in the impl TU; confirm it is NOT
`inline ClassName::method` in the header). Where a row is header-`inline`, apply the
WR-01 forwarder/OMIT remedy. Add the verification to the row comment so it does not
silently regress on a future header edit.

### WR-03: `game::g_runningFlags` advertises a `bool isOver()` under a "flags" contract name (semantic type mismatch)

**File:** `src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp:277`
**Issue:** The row maps the contract name `game::g_runningFlags` to `&Game::isOver`
(a `static bool isOver()`), tagged "ACCESSOR ... call-not-read". A consumer that reads
`g_runningFlags` as SWGEmu does (a multi-bit running-state word) will instead invoke a
zero-arg function returning a single `bool`. If the injector's typedef for this slot is
a data pointer (read) rather than `bool(__cdecl*)()` (call), it dereferences a function
entry point as an int -- garbage, and a detour-vs-read intent mismatch. The "call-not-
read" note acknowledges this but the burden is silently pushed onto the consumer, and
the contract name still implies a readable flags global.
**Fix:** Confirm with the Utinni side that this slot is consumed as a `bool()` call,
not a data read; if there is any doubt, rename the contract slot to reflect the accessor
semantics (e.g. `game::isOver`) so the name cannot be mistaken for a readable global, and
bump the version. Same caution applies to the other accessor-as-global rows
(`graphics::g_renderTargetWidth/Height` -> getters, `graphics::g_frameNumber` -> getter,
`cuiManager::g_instance`/`cuiIo::g_instance` -> `getIoWin`).

### WR-04: `config::loadConfigFileString` contract name maps to `ConfigFile::loadFile` (path-arg vs string-buffer semantic mismatch)

**File:** `src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp:255`
**Issue:** The contract slot `config::loadConfigFileString` (spec name `loadFromString`)
is bound to `&ConfigFile::loadFile`, which is `static bool loadFile(char const*)` -- a
function that takes a **filename/path** and loads from disk, NOT a function that parses a
config **string buffer**. The adjacent row `config::loadConfigFileBuffer` already binds
the real buffer parser `ConfigFile::loadFromBuffer(char const*, int)`. If the injector
calls `loadConfigFileString(someConfigText)` expecting in-memory parsing, it instead
hands the config text to `loadFile()` as a path -> file-not-found / no-op, a silent
behavioral defect (not a crash). The MISMATCH comment documents the name swap but not the
semantic divergence (path loader vs string parser).
**Fix:** Confirm the injector's intended semantics for `loadConfigFileString`. If it
means "parse a config string", this row should map to `loadFromBuffer` (with a length
thunk) or be OMITTED, not to the path-based `loadFile`. If the injector truly wants the
path loader, rename the slot to avoid the "String" implication. Either way, annotate the
row with the path-vs-buffer distinction.

### WR-05: `report::print` maps to `Report::puts`, dropping the printf-style contract

**File:** `src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp:405`
**Issue:** The contract slot `report::print` (spec `Report::print`) is bound to
`&Report::puts` -- `static void puts(const char*)`, a single fixed-string sink with no
format processing. The comment notes "printf is variadic" as the reason, but a consumer
expecting `print(fmt, ...)` semantics that passes a format string containing `%s`/`%d`
to `puts` will get the raw unformatted string (best case) -- and if the injector's
typedef is variadic `void(__cdecl*)(const char*, ...)` while the target is the
non-variadic `puts(const char*)`, the extra stack args are the caller's responsibility
under `__cdecl` so it will not corrupt, but the format directives are silently ignored.
This is a behavioral/contract mismatch, not a crash.
**Fix:** Confirm the injector typedef for this slot. If it is a plain
`void(const char*)` logger, `puts` is fine -- rename the slot or note "no format
processing" in the comment. If it is variadic printf-style, advertise a thunk that
forwards to the engine's actual variadic report path (or OMIT and flag for the handback)
rather than silently binding the non-formatting `puts`.

## Info

### IN-01: Row-comment header line numbers drift from the actual headers

**File:** `src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp` (many rows)
**Issue:** Several `[Header.h:NN]` citations are stale relative to the current headers
(e.g. `object::getTransform_o2w` cites `[Object.h:243]` but the declaration is at 234;
`game::*` rows cite a `Game.h` line numbering that does not match the actual
`src/shared/core/Game.h`). The bindings themselves are correct; only the cited line
numbers drift. This is cosmetic but, combined with WR-01, means the comments cannot be
trusted to convey inline/line facts.
**Fix:** Regenerate the cited line numbers, or drop the `:NN` suffix and cite only the
header, so the comments do not rot on every upstream header edit.

### IN-02: `utinni_strEq` re-implements `strcmp`-equality by hand

**File:** `src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp:498-510`
**Issue:** A hand-rolled string-equality helper duplicates `std::strcmp(a,b)==0` (with
null-guards). The implementation is correct (verified: pointer-eq fast path, null
handling, prefix case all behave), but it is avoidable surface area in a TU that already
includes the C++ standard library.
**Fix:** Use `(a && b && std::strcmp(a, b) == 0) || a == b` (or keep the helper but note
why a raw `strcmp` was avoided -- e.g. null tolerance). Low priority.

### IN-03: Several inline `.cpp` SKIP/OMIT comments cite header lines that have drifted

**File:** `src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp:416-453`
**Issue:** The VIRTUAL-SKIP and OMIT ledger cites header lines (e.g. `Object.h:168`,
`Object.h:1216`, `Camera.h:210`) some of which are slightly off the current headers.
The classifications (virtual / inline / protected) are correct where spot-checked
(`setParentCell` virtual, `move_o` inline, `realIntersect` virtual), so the decisions
stand; only the line citations drift.
**Fix:** Same as IN-01 -- refresh or drop the `:NN` citations.

---

_Reviewed: 2026-06-22T00:00:00Z_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
