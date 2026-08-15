# PROVIDER-HANDBACK — x64 `GetEngineHookPoints`: SHIPPED, all six asks answered

**Date:** 2026-08-15
**From:** `swg-client-v2` (provider)
**To:** SWG-Toolkit (consumer)
**Answers:** [2026-08-15-CHANGE-REQUEST-x64-hookpoint-contract.md](2026-08-15-CHANGE-REQUEST-x64-hookpoint-contract.md)
**Status:** the x64 surface is **BUILT, STAGED, AND TABLE-VERIFIED TODAY** — not planned.
Provider commits: `372e7aa42` (the port), `e292a3478` (probe harness), `971805d5c` (v34 wave).
**Contract: v33 → v34, 160 → 162 names** (two ADDs for your asks #4/#5; zero renames, zero
removals, zero signature changes).

---

## Headline

Both staged exes (`stage\SwgClient_r.exe` Win32, `stage-x64\SwgClient_r.exe` x64) export
`GetEngineHookPoints` and advertise the **identical 162-name table from one source**. Verified by
a boot-free probe that reads the table exactly the way your agent does (image load +
`GetProcAddress` by name + call — the pre-CRT early-read path):

```
[Win32] version=34 count=162 nulls=0 dups=0 uniqueNames=162   PASS
[x64]   version=34 count=162 nulls=0 dups=0 uniqueNames=162   PASS
```

Zero nulls on x64 also proves every MI real-entry extraction (your DETOUR targets) held
`delta==0` under the x64 PMF layout.

---

## Ask 1 — row names byte-identical on x64: **YES, by construction**

One `engine_hookpoints.inc`, one table, both arches. No renames, ever, in this wave; v34 is
additive-only (the two new rows below). The probe's name-set-equality check (both directions vs
the `.inc`) passed on both exes, so "same names" is measured, not asserted.

One non-name caveat: the x64 export's **ordinal happens to land at 82** like Win32 (same export
set, same alphabetical ordering). That is a coincidence of the export table, **not contract** —
keep binding by name, never by ordinal.

## Ask 2 — exact struct layout, verbatim from `engine_hookpoints.h`

```cpp
struct EngineHookPoint
{
	const char * name;   // stable contract identity, e.g. "config::loadOverrideConfig"
	void *       addr;   // &EngineSymbol (or thunk) -- borrowed, process-lifetime
};

struct EngineHookPoints
{
	unsigned int                 version;   // == ENGINE_HOOKPOINTS_VERSION at build time
	unsigned int                 count;     // number of rows in entries[]
	const EngineHookPoint * entries;  // static array of `count` rows
};
```

- **No `#pragma pack` anywhere** in the contract header — natural alignment on both arches.
- `version` / `count` **stay 32-bit** (`unsigned int`) on x64.
- `EngineHookPoints`: `version` @0, `count` @4, `entries` @**8 on BOTH arches** (Win32: no
  padding, sizeof 12; x64: pointer naturally 8-aligned after two uints, sizeof 16).
- `EngineHookPoint`: two pointers — Win32 `name` @0 / `addr` @4, sizeof/stride **8**; x64
  `name` @0 / `addr` @8, sizeof/stride **16**.
- No sentinel row; `count` is authoritative (unchanged from what you walk today).

## Ask 3 — calling conventions, row by row

General rule first: **x64 has one calling convention; every `__cdecl` / `__stdcall` /
`__thiscall` / `__fastcall` annotation is accepted-and-ignored.** Your existing `__thiscall`
typedefs work **unchanged** on x64 (`this` in RCX). Provider-side detail that makes that true by
construction: the Win32 `__fastcall(pThis, dummy-EDX, ...)` emulation thunks take **no dummy on
x64** — so the same `__thiscall` member-shaped typedef matches both arches.

| Row | Answer |
|---|---|
| `camera::getProjectionMatrix` | **Confirmed identical**: `extern "C" int __cdecl (float* out16)` — copy-out GlMatrix4x4 (row-major 4×4), 1 ok / 0 no camera. |
| `camera::getTransformO2W` | **Confirmed identical**: copy-out `float[12]` row-major 3×4, position column 3; int return. |
| `object::getTransformO2P` | **Confirmed identical**: same 3×4 layout and int return; borrowed `Object*` first arg. |
| `worldSnapshot::moveObject` | **Explicit YES.** `static void WorldSnapshot::moveObject(int64, Transform const&)`. `Transform`'s **sole** data member is `real matrix[3][4]` (`Transform.h:52` `typedef real matrix_t[3][4];` / `:126` `matrix_t matrix;`) — 48 bytes of float, row-major, position in column 3, **no vtable, no other members** — so `Transform const&` is a pointer to exactly that on both arches. |
| `object::getNetworkId` | **Confirmed**: still returns `const NetworkId&` — a pointer-sized **reference** to the object's member on both arches, never a by-value 64-bit id. (`NetworkId` wraps an `__int64` value; read it through the reference as you do today.) PMF row unchanged; `__thiscall` typedef fine on x64. |
| `cellProperty::getCellName` | **Confirmed identical**: copy-out, returns needed length **including NUL**, 0 = null input / no name; world cell returns `"world"`. |

## Ask 4 — `object::setScale`: **ADDED (v34)**

`{ "object::setScale", pmfToVoid(&Object::setScale) }` — non-virtual, out-of-line
(`Object.h:228` / `Object.cpp:2205`), advertised on **both arches**. Typedef shape:
`void (__thiscall*)(Object*, const Vector&)` — `Vector` is a 3-float POD passed by `const&`,
the exact `object::setPosition_w` / `object::move_p` boundary shape you already bind. Your
D-09 `nullptr` seed resolves on the next sync.

## Ask 5 — identifying an advertised x64 build: **BOTH mechanisms provided**

1. **Version bumped 33 → 34** (name-add policy; both arches carry 34 from the one source — the
   bump signals the dual-arch contract era, and your soft-warning handling means the x86 path is
   untouched).
2. **New row `client::advertisedArchBits`** — `extern "C" int __cdecl (void)` returning the
   provider pointer width in bits (**32 / 64**, a compile-time per-arch constant; pre-CRT-safe
   like the rest of the fill). This is the assert you asked for: your x64 agent can require
   `advertisedArchBits == 64` from the **table itself** rather than inferring from the PE
   header. Your fail-closed design for the export-absent x64 case is right, and we endorse it —
   there is no legacy x64 client, and this row is deliberately impossible to skew (it is
   `sizeof(void*) * 8` compiled into the advertising image).

## Ask 6 — x86 advertised build unchanged: **YES**

Same source, additive-only: 2 new names, version 34, **no removals, no renames, no signature
changes, no semantic changes** to any existing row. The Win32 export remains ord-82 (its RVA
moves with each relink, as always — you never depended on it). The probe passed on the staged
Win32 exe with the full 162-name set.

---

## Extras you did not ask for but want

- **Detour targets on x64.** The DETOURED rows (`groundScene::update`,
  `groundScene::handleInputMapEvent`, `cuiChatWindow::enableTextInput`,
  `cuiChatWindow::chatEnterHandler`, `cuiChatWindow::createNewWindow`,
  `creatureObject::setTarget`) advertise the **real engine code entry on x64 too** (MI-PMF
  `delta==0` verified — zero nulls in the probe). What we can NOT vouch for is your detour
  engine: **verify DetourXS's x64 trampoline support early** — x64 prologue patching is a
  different problem (RIP-relative instructions, ≥14-byte absolute jumps) and it is the one
  piece of this integration wholly on your side of the boundary.
- **The pre-CRT early-read guarantee holds on x64.** The table fill is still pure address
  arithmetic (no heap, no TLS, no CRT, no imports); our probe literally loads the exe without
  resolving imports and calls the export — the same envelope as your remote thread reading
  before `_initterm`.
- **A boot-free contract gate you can run yourself**: provider repo
  `tools/hookpoints-probe/Probe-HookPoints.ps1`. No args probes both staged exes; `-ExePath`
  probes one; arch-mismatched probes re-dispatch to the matching PowerShell host. Checks:
  export resolves by name, version matches the header, count and name set match the `.inc`
  exactly, zero nulls, zero dups. Exit 0 = PASS. Negative-tested (an exe without the export
  fails with exit 1). Run it against any staged build **before** binding.
- **`EngineWsNodeInfo` is bit-identical on x64** — the frozen 80-byte layout (all offsets, the
  `__int64` at 8) holds under x64 alignment; the `offsetof` static_asserts in
  `WorldSnapshot.cpp` compile on both arches as the standing proof. Size-first protocol
  unchanged.
- Your `LiveState` truncated-pointer item (`focusToken`/`guardAddr` as `uint32_t`) — noted and
  agreed it is yours; an arch-versioned channel is the right call. Nothing provider-side reads
  that memory.

## What is NOT verified (honesty section)

- No live x64 **injection** has ever run — the probe validates the table and the read path, not
  row CALLS (those need an installed engine) and not detours. Suggested acceptance ladder for
  your first x64 session: bind + assert `advertisedArchBits==64` → CALL read-only rows
  (`game::getPlayer`, `game::getSceneId`) → called thunk rows (`groundScene::getCurrentCamera`)
  → copy-out accessors → detours last.
- The x64 client itself boots and plays (that is the shipping v3.0-milestone build), but no
  boot has run with a consumer attached on x64.
