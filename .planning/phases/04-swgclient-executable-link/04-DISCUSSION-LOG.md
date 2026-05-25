# Phase 4: SwgClient Executable Link — Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-05-04
**Phase:** 04-swgclient-executable-link
**Areas discussed:** Link line exclusions, Post-build DLL staging, STLPort exe linkage (D-10)

---

## Link Line Exclusions

### Q1 — CRT library exclusion mechanism

| Option | Description | Selected |
|--------|-------------|----------|
| Rely on CMAKE_MSVC_RUNTIME_LIBRARY (Recommended) | CMP0091 NEW + global setting already emits correct /NODEFAULTLIB flags | ✓ |
| Reproduce full ignoreLibraries.rsp explicitly | Verbatim /NODEFAULTLIB via target_link_options — more explicit, but fights CMake CRT management | |
| You decide | Claude picks based on Phase 1-3 precedents | |

**User's choice:** Rely on CMAKE_MSVC_RUNTIME_LIBRARY
**Notes:** Phase 1 already locked CMP0091 NEW + global MultiThreaded generator expression; consistent continuation.

---

### Q2 — Mozilla XPCOM libs in link line

| Option | Description | Selected |
|--------|-------------|----------|
| Simply omit them (Recommended) | Stub generates no XPCOM references; nothing to link | ✓ |
| Add /NODEFAULTLIB exclusions | Explicit linker ignore — defensive but unnecessary per Phase 3 dumpbin gate | |
| You decide | Claude picks based on D-07 confirmation | |

**User's choice:** Simply omit them
**Notes:** Phase 3 SC gate (D-07 dumpbin check) already confirmed zero XPCOM symbols; omission is the safe path.

---

## Post-build DLL Staging

### Q1 — Staging destination

| Option | Description | Selected |
|--------|-------------|----------|
| Alongside the exe in build output dir (Recommended) | bin/Debug/ and bin/Release/ — double-click just works | ✓ |
| Separate runtime/ directory | Fixed path regardless of config — requires PATH manipulation to run | |
| You decide | Claude picks for minimum Phase 6 friction | |

**User's choice:** Alongside the exe in build output dir
**Notes:** Standard game-engine pattern; minimises friction for Phase 6 launch debugging.

---

### Q2 — Staging mechanism

| Option | Description | Selected |
|--------|-------------|----------|
| add_custom_command POST_BUILD (Recommended) | cmake -E copy + cmake -E copy_directory; fires automatically on link | ✓ |
| cmake install() rules | DESTDIR-based; requires manual cmake --install | |
| You decide | Claude picks the auto-firing approach | |

**User's choice:** add_custom_command POST_BUILD
**Notes:** Must fire automatically; install() rules are opt-in.

---

### Q3 — client.cfg in Phase 4

| Option | Description | Selected |
|--------|-------------|----------|
| Leave client.cfg entirely to Phase 6 | Clean phase boundary | |
| Drop a minimal placeholder client.cfg now (Recommended) | Commented-out searchPath stubs; allows testing exe launch before Phase 6 | ✓ |
| You decide | Claude picks based on ROADMAP phase boundary | |

**User's choice:** Drop a minimal placeholder client.cfg now
**Notes:** Low cost; lets the exe launch without a fatal config error; Phase 6 still owns full asset wiring.

---

## STLPort Exe Linkage (D-10)

### Q1 — When to add STLPort runtime linkage

| Option | Description | Selected |
|--------|-------------|----------|
| Link eagerly from the start (Recommended) | Pre-empts LNK storms; matches swg-main pattern | ✓ |
| Only add if LNK errors appear | Reactive approach; likely to fail first attempt | |
| You decide | Claude picks based on D-10 risk profile | |

**User's choice:** Link eagerly from the start
**Notes:** D-10 explicitly called out executable-link LNK storms as a known risk; proactive linking is safer.

---

### Q2 — How to express STLPort linkage

| Option | Description | Selected |
|--------|-------------|----------|
| Direct target_link_libraries with generator expression (Recommended) | $<$<CONFIG:Debug>:stlportstlx_vc71_debug.lib> / $<$<CONFIG:Release>:stlportstlx_vc71.lib>; no new INTERFACE target | ✓ |
| Promote STLPort::Headers INTERFACE target to STLPort::Static | Cleaner API but adds scope beyond Phase 4 exe | |
| You decide | Claude picks the minimal-change approach | |

**User's choice:** Direct target_link_libraries with generator expression
**Notes:** Minimal scope; exe is the only consumer; no reason to add INTERFACE complexity.

---

### Q3 — LNK storm fallback

| Option | Description | Selected |
|--------|-------------|----------|
| TheSuperHackers rebuild (Locked Phase 1 decision) | Already named in PROJECT.md Key Decisions; stays within Phase 4 | ✓ |
| Defer LNK storms to a new Phase 4.x insertion | More structured but slower | |
| You decide | Claude picks based on PROJECT.md | |

**User's choice:** TheSuperHackers rebuild (Locked Phase 1 decision)
**Notes:** Confirms the Phase 1 spike decision remains the plan; no new process overhead.

---

## Claude's Discretion

None — user selected options for all questions.

## Deferred Ideas

- `swgClientAutomation` CMakeLists — not in SwgClient.vcproj; deferred to tools-build phase
- `swgClientQtWidgets` CMakeLists — Qt tool lib, not in SwgClient.vcproj; deferred
- Full `client.cfg` with real TRE search paths — Phase 6 (LAUNCH-01)
- `/WX` warnings-as-errors re-enable — post-v1 per FLAGS-02
