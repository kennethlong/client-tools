# Phase 8: Dead Code Removal — Track B - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-05-07
**Phase:** 8-Dead Code Removal — Track B
**Areas discussed:** Qt GUI tools strategy, Linux-only tool handling, Wave grouping into plans, Tools beyond CLEAN-06's named list

---

## Qt GUI Tools Strategy

| Option | Description | Selected |
|--------|-------------|----------|
| Find Qt + wire it | Add find_package(Qt5 Widgets). Tools build if Qt installed; skip gracefully if not. | ✓ (with Qt3 clarification) |
| Guard + defer build | Author CMakeLists.txt but wrap add_executable in if(QT_FOUND). File exists but build deferred. | |
| Scope Qt tools out of Phase 8 | Only wire non-Qt tools. Qt integration deferred to its own phase. | |

**User's choice:** Find Qt + wire it

**Follow-up — Qt Version:**

| Option | Description | Selected |
|--------|-------------|----------|
| Vendored Qt 3.3.4 | Write FindQt3.cmake to src/external/3rd/library/qt/3.3.4/. Consistent with vendored SDK pattern. | ✓ |
| System Qt 5/6 | Requires system install + .ui format conversion. Qt5 dropped Qt3 support. | |

**User's choice:** Vendored Qt 3.3.4
**Notes:** User noted: "put a note to upgrade to latest in a future version". Qt3 .ui files (version="3.3") confirmed in ParticleEditor. Vendored Qt3.3.4 has moc.exe, uic.exe, headers, and prebuilt libs.

---

## Linux-Only Tool Handling

| Option | Description | Selected |
|--------|-------------|----------|
| CMakeLists.txt scaffold with if(UNIX) guard | Author CMakeLists.txt that compiles shared/ on all platforms, linux/ on UNIX only. File exists but no Win32 exe. | |
| Skip them | Linux-only CLI tools out of scope for Windows CMake build. | ✓ |

**User's choice:** Skip for now
**Notes:** User noted: "make a note to build these on a linux server later in future version". Affected tools: ArmorExporterTool, DataTableTool, WeaponExporterTool, CoreWeaponExporterTool (confirmed linux-only by directory survey).

---

## Wave Grouping Into Plans

| Option | Description | Selected |
|--------|-------------|----------|
| By source tier (4 waves) | Wave 1: engine/shared CLI. Wave 2: engine/client GUI (Qt). Wave 3: game/* apps. Wave 4: external/ours + boot verify. | ✓ |
| Non-Qt then Qt (2 waves) | Wave 1: all non-Qt tools. Wave 2: all Qt tools. | |
| One big plan | All ~31 CMakeLists.txt in a single plan. | |

**User's choice:** By source tier (Recommended)
**Notes:** ROADMAP.md already suggested "wave-grouped by tool tier and dependency"; user confirmed this approach.

---

## Tools Beyond CLEAN-06's Named List

| Option | Description | Selected |
|--------|-------------|----------|
| CLEAN-06 list only | Wire only the ~35 named tools. Unlisted tools explicitly out of scope. | |
| Easy wins too | Wire unlisted tools with simple source + no exotic SDK deps. | |
| All non-Linux tools including Maya | Wire everything with Windows source, including MayaExporter with Maya SDK. | ✓ |

**User's choice:** All non-Linux tools including Maya

**Follow-up — Maya SDK Version:**

| Option | Description | Selected |
|--------|-------------|----------|
| Maya 7 | Most complete vendored SDK. Most exporter source appears to target Maya 7 era. | |
| All four (multi-target) | FindMaya.cmake checks all four versions. More flexible but complex. | |
| Maya 8 (latest vendored) | Maya 8 SDK present. Most current option. | ✓ |

**User's choice:** Latest vendored (Maya 8)

**Follow-up — game/server/application scope:**

| Option | Description | Selected |
|--------|-------------|----------|
| CLEAN-06 editors + easy wins | Wire 5 named editors, check extras for win32 source. Skip SwgGameServer/SwgDatabaseServer (Linux). | |
| Everything non-Linux | Check all game/server/application/ for win32 source, wire anything that has it. | ✓ |

**User's choice:** All non-Linux + note remaining Linux ones for future version.

---

## Claude's Discretion

- If Maya 8 headers cause build errors in MayaExporter, fall back to Maya 7 SDK (planner's discretion, no need to ask user).
- Discovery rule for tool scope: tool is in scope if `src/win32/` exists OR `src/shared/` contains a `main()` entry point.

## Deferred Ideas

- **Qt upgrade to latest Qt5/Qt6:** User explicitly requested a note to upgrade Qt from 3.3.4 to latest in a future version. Requires .ui format conversion.
- **Linux tool builds:** DataTableTool, ArmorExporterTool, WeaponExporterTool, CoreWeaponExporterTool, SwgGameServer, SwgDatabaseServer — defer to future Linux-capable phase.
