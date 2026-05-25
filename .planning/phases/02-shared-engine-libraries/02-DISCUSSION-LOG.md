# Phase 2: Shared Engine Libraries — Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-05-03
**Phase:** 2-Shared Engine Libraries
**Areas discussed:** swg-main divergence policy, server-only file exclusion, time_t probe mechanism

---

## swg-main Divergence Policy

| Option | Description | Selected |
|--------|-------------|----------|
| Researcher pre-diffs all 22 libs first | Research phase produces complete divergence map before planning | ✓ |
| Planner investigates at authoring time | Divergences handled per-lib during planning; risk of mid-plan stalls | |

**Whitengold file not in swg-main:**

| Option | Selected |
|--------|----------|
| Include it — whitengold is authoritative | ✓ |
| Flag for manual review | |

**swg-main file not in whitengold:**

| Option | Selected |
|--------|----------|
| Skip silently — added post-fork to swg-main | ✓ |
| Treat as a blocker | |

**Notes:** 1:1 adoption means swg-main is the dep-graph template; whitengold source dirs are the file-list authority. Consistent with the "whitengold is the archive" mental model.

---

## Server-Only File Exclusion

| Option | Description | Selected |
|--------|-------------|----------|
| Use original .vcproj file list | VS 2005 project already lists client-safe files | ✓ |
| Include all, fix compile errors | Pragmatic but creates a fix-errors loop | |
| Researcher pre-filters by header inspection | Thorough but time-consuming | |

**Scope of .vcproj policy:**

| Option | Selected |
|--------|----------|
| Only network message libs — others use whitengold source dir | ✓ |
| All 22 libs use .vcproj as source list | |

**Notes:** sharedNetworkMessages is the known risk. Applying .vcproj extraction selectively keeps research scope narrow while addressing the only lib where client/server mixing is a real concern.

---

## time_t Probe Mechanism

| Option | Description | Selected |
|--------|-------------|----------|
| configure_file — generate probe .cpp into build tree | One template, configured per lib; no source edits | ✓ |
| CHECK_TYPE_SIZE in CMakeLists | Configure-time check only; won't catch toolset regressions | |
| New .cpp per lib in source tree | Works but adds 22 new files to engine source dirs | |

**Probe coverage:**

| Option | Selected |
|--------|----------|
| One per tier — sharedDebug / sharedFoundation / sharedNetwork | ✓ |
| All 22 libs | |

**Notes:** Three probes cover the three distinct compile contexts. The template lives at `src/cmake/stubs/time_t_probe.cpp.in` — consistent with the stubs directory established for Phase 3's XPCOM stub.

---

## Claude's Discretion

None — all decisions were made by the user from presented options.

## Deferred Ideas

None — discussion stayed within phase scope.
