# Code Conventions — Extended Reference

Extended sections omitted from `CLAUDE.md` for brevity. The essentials (Naming Patterns, Code Style, Import Organization, Error Handling) are summarised there; this file expands the sections only needed occasionally.

**Source:** `.planning/codebase/CONVENTIONS.md` | **Analysis Date:** 2026-05-03

---

## Logging

Framework: custom macros in the `sharedDebug` library.

**Macros:**
- `REPORT(condition, flags, printf_args)` — conditional log with flags
- `REPORT_LOG(condition, printf_args)` — logs to file only
- `REPORT_PRINT(condition, printf_args)` — prints to console only
- `REPORT_LOG_PRINT(condition, printf_args)` — logs and prints
- `DEBUG_REPORT*` macros — debug-only versions (optimised out in Release)

**Log category flags (`Report::RF_*`):**
| Flag | Meaning |
|------|---------|
| `Report::RF_log` | file logging |
| `Report::RF_print` | console output |
| `Report::RF_warning` | warning flag |
| `Report::RF_fatal` | fatal flag |
| `Report::RF_console` | console routing |
| `Report::RF_dialog` | dialog box |

**Configuration:**
- `[SharedDebug] installTimer=true` in `client.cfg` / `server.cfg` enables per-phase profiling
- Log callbacks: `Report::bindLogCallback()`, `Report::bindWarningCallback()`, `Report::bindFatalCallback()`

---

## Comments

**When to comment:**
- File header: always include copyright, author if known, brief purpose
- Complex algorithms: explain intent, not mechanics
- Workarounds/hacks: explain why (historical context)
- Non-obvious design decisions: annotate intent

**Format:**
- Single-line: `// comment`
- Multi-line: `/* ... */` or stacked `//` per line
- File headers: `// ====... ====` box format

No JSDoc/TSDoc — code predates documentation generators.

---

## Function Design

**Size guidelines (observed, not enforced):**
- Small utilities: <50 lines
- Setup functions: 50–200 lines (sequential `install()` calls)
- Game loop functions: 100+ lines (multiple update phases)
- Critical path: inline functions preferred in header-only libs

**Parameters:**
- `const` on right of pointer/reference: `int * const ptr`
- Pointer parameters used instead of references
- Output parameters as non-const pointers (no parameter bundles): `void setFlags(int flags)` not `void setFlags(const int &flags)`

**Return values:**
- Explicit return types, no `auto`
- `void` for side-effect operations
- `bool` for success/failure
- Pointer return for object ownership (null = failure): `AbstractFile * const abstractFile = TreeFile::open(...)`

---

## Module Design

**Setup pattern:** Component libraries use explicit setup classes — `class SetupSharedFoundation`, `class SetupClientGraphics`. Public headers go in `include/public/<ComponentName>/`; internal headers stay in `src/` (not installed).

**Singleton pattern:**
```cpp
// Defined in src/external/ours/library/singleton/src/shared/Singleton.h
template<class ValueType> class Singleton { ... };

class MySingleton : public Singleton<MySingleton> { ... };
MySingleton::getInstance()  // auto-installs on first call
// or: MySingleton::install() / MySingleton::remove() for explicit lifetime
```

**Barrel includes:** `First*` headers serve as precompiled-header barrels. `FirstSwgClient.h` pulls in `FirstSharedFoundation.h` and all major subsystems. `#include "FirstSwgClient.h"` must be the first line in every `.cpp`.

**Namespaces:** Minimal — most code in the global namespace. File-local scope uses anonymous namespaces or named local namespaces:
```cpp
namespace ClientMainNamespace {
    void installConfigFileOverride() { ... }
}
```
No `using namespace std;` anywhere (STLPort era).

`DLLEXPORT` macro used for DLL boundaries (Win32-specific, not used in static builds).

---

## Static Analysis & Portability

**Key compiler flags:**

| Flag | Purpose |
|------|---------|
| `/Zc:wchar_t-` | `wchar_t = unsigned short` (XPCOM ABI compatibility) |
| `/MBCS` | Multi-byte character set (not Unicode) |
| `WarnAsError="TRUE"` | Strict warnings-as-errors |
| `/MT` (Release) / `/MTd` (Debug) | Static CRT linkage |
| `_USE_32BIT_TIME_T=1` | Pin 32-bit `time_t` for STLPort 4.5.3 |
| `-DPRODUCTION=(0\|1)` | 0=dev (debug menu, profiler), 1=retail |
| `-DDEBUG_LEVEL=(0\|1\|2)` | 0=release, 1=opt-debug, 2=debug |
| `/Ob1` | Function-level inlining only |

**Platform guards:**
- Windows (primary): `#include <dinput.h>`, WinAPI calls
- Linux (secondary): `#ifdef _LINUX` or `#if TARGET_OS == OS_LINUX`
- Conditional builds: `#if PRODUCTION == 0` / `#if PRODUCTION == 1`

---

*Convention analysis: 2026-05-03*
