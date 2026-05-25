# Phase 3: Client Engine Libraries (SDK-Heavy Tier) — Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-05-03
**Phase:** 3-Client Engine Libraries (SDK-Heavy Tier)
**Areas discussed:** DPVS linkage strategy, XPCOM stub placement, clientGame phase boundary, STLPort first-use triage plan

---

## DPVS Linkage Strategy

| Option | Description | Selected |
|--------|-------------|----------|
| Prebuilt `.lib` | Use `lib/win32-x86/` MSVC 7.1 prebuilt; simpler, CRT match risk | ✓ |
| Build from vendored source | Compile DPVS under VS 2022; avoids CRT mismatch, risks compile failures | |

**User's choice:** Deferred to Claude
**Notes:** No-source-edit constraint means VS 2022 compile failures in old DPVS code cannot be fixed in this milestone. Prebuilt `/MT` lib is the pragmatic path. LNK2005 at Phase 4 link is the named escalation trigger for a source-build fallback.

---

## XPCOM Stub Placement

| Option | Description | Selected |
|--------|-------------|----------|
| `src/cmake/stubs/libMozilla_stub.cpp` | New file in CMake-only stubs dir; replaces src/win32 sources | ✓ |
| `configure_file` template | Generate stub at build time; no checked-in .cpp | |
| Exclude XPCOM .cpp files | Skip inclusion in CMakeLists; may leave unresolved symbols | |

**User's choice:** Deferred to Claude
**Notes:** New file approach is explicit and auditable. `src/cmake/stubs/` parallels `src/cmake/win32/` — consistent with the Phase 1 CMake-only directory pattern. Planner must scan `clientUserInterface` includes to enumerate stub entry points before writing the file.

---

## clientGame Phase Boundary

| Option | Description | Selected |
|--------|-------------|----------|
| clientGame in Phase 3 (per ROADMAP) | Phase 3 authors all 13 client lib CMakeLists; Phase 4 is pure link step | ✓ |
| clientGame in Phase 4 | Defer integrator CMakeLists to the link phase | |

**User's choice:** Deferred to Claude
**Notes:** ROADMAP is the authority. Confirmed as-designed. Phase 4 = executable link only (`add_executable` + `target_link_libraries`).

---

## STLPort First-Use Triage Plan

| Option | Description | Selected |
|--------|-------------|----------|
| clientDirectInput first within Tier 7 | Prove DX9 headers with smaller dinput.h surface before d3d9.h | ✓ |
| clientGraphics first | Full DX9 surface immediately; maximum early signal but maximum risk | |
| Dedicated canary target | Separate minimal CMakeLists before full authoring | |

**User's choice:** Deferred to Claude
**Notes:** LNK2005 from STLPort is a Phase 4 concern (executable link), not Phase 3 (static lib build). Phase 3 triage focuses on DX9 header ordering and C4530 suppression. STLPort `.lib` link steps are explicitly excluded from Phase 3 targets.

---

## Claude's Discretion

All four areas were deferred to Claude by the user. Decisions D-01 through D-15 in CONTEXT.md represent Claude's choices, made with explicit rationale. Planner should treat them as locked unless research surfaces a hard technical blocker.

## Deferred Ideas

None — discussion stayed within phase scope.
