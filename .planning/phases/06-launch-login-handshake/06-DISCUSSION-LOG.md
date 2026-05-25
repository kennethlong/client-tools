# Phase 6: Launch + Login Handshake — Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-05-05
**Phase:** 6-Launch + Login Handshake
**Areas discussed:** client.cfg template, Boot milestone strategy, Boot failure triage, NetworkVersionId pre-audit

---

## client.cfg template

### Q1 — Template location in repo

| Option | Description | Selected |
|--------|-------------|----------|
| `src/cmake/config/` | Versioned template; POST_BUILD copies to bin/ | ✓ |
| `exe/win32/` companion | Alongside original per-planet configs | |
| Repo root | Manual copy step required | |

**User's choice:** `src/cmake/config/` (CMake configure_file approach — one source of truth)

---

### Q2 — Machine-specific TRE path handling

| Option | Description | Selected |
|--------|-------------|----------|
| CMake configure_file with `-DSWG_INSTALL_DIR` | Variable substituted at configure time | ✓ |
| Placeholder paths + EDIT-THIS comments | User edits staged file manually | |
| You decide | Let planner pick | |

**User's choice:** `configure_file` with `-DSWG_INSTALL_DIR`

---

### Q3 — VM connectivity

| Option | Description | Selected |
|--------|-------------|----------|
| Loopback (127.0.0.1) | VM on same machine | ✓ |
| Different host / VM IP | Additional CMake variable needed | |

**User's choice:** Loopback — `loginServerAddress0=127.0.0.1`, `loginServerPort0=44453`

---

### Q4 — TRE file list scope

| Option | Description | Selected |
|--------|-------------|----------|
| Full retail set | Copy TRE list from existing configs; safe, complete | ✓ |
| Minimal boot set only | Research minimum required TREs | |

**User's choice:** Full retail set
**Notes:** User has an installed SWGSource Client image at `D:\Code\SWGSource Client v3.0\` with a working `client.cfg` at that directory root. Researcher must read that file to get the exact TRE list and any additional settings already tuned for this server/VM.

---

## Boot milestone strategy

### Q1 — Plan structure

| Option | Description | Selected |
|--------|-------------|----------|
| Two explicit checkpoints | MVB-1 first, then MVB-3 | ✓ |
| One end-to-end pass to MVB-3 | Single attempt to character select | |

**User's choice:** Two explicit checkpoints

---

### Q2 — Server startup documentation

| Option | Description | Selected |
|--------|-------------|----------|
| Include server startup checklist | Document which swg-main processes to start | |
| VM startup is out of scope | User has SWGSource VM startup instructions | ✓ |

**User's choice:** Out of scope — user has SWGSource VM startup instructions already

---

### Q3 — MVB-1 success criterion

| Option | Description | Selected |
|--------|-------------|----------|
| Visual + log marker | Login UI visible AND specific installTimer log line | ✓ |
| Visual confirmation only | Login UI on screen with frames — sufficient | |

**User's choice:** Visual confirmation + specific installTimer log marker

---

## Boot failure triage

### Q1 — Primary diagnostic approach

| Option | Description | Selected |
|--------|-------------|----------|
| VS 2022 debugger attached from launch | First-chance exceptions; immediate source location | |
| Log-first (installTimer output) | Standalone run, check Logs/ directory | |
| Hybrid: log-first, then debugger on failures | Standalone pass → debugger only when needed | ✓ |

**User's choice:** Hybrid approach

---

### Q2 — Early crash fallback

| Option | Description | Selected |
|--------|-------------|----------|
| Windows Event Log + mini-dump in bin/Debug/ | sharedFoundation writes minidump on exception | ✓ |
| Attach WinDbg / VS debugger immediately | Skip log analysis for early crashes | |
| You decide | Let planner pick | |

**User's choice:** Windows Event Log + mini-dump

---

### Q3 — Triage runbook

| Option | Description | Selected |
|--------|-------------|----------|
| Include a triage runbook | Document 3-4 failure modes with log signatures and fixes | ✓ |
| Ad-hoc only | Debug what comes up without prescribed steps | |

**User's choice:** Include triage runbook (missing DLL, TRE not found, D3D init failure)

---

## NetworkVersionId pre-audit

### Q1 — Pre-audit vs. runtime discovery

| Option | Description | Selected |
|--------|-------------|----------|
| Pre-audit in source before first run | Researcher checks both whitengold + swg-main | ✓ |
| Discover at runtime | See if LoginIncorrectClientId appears; address then | |

**User's choice:** Pre-audit

---

### Q2 — Fix strategy if mismatch found

| Option | Description | Selected |
|--------|-------------|----------|
| Override via CMake compile definition | Inject correct value without touching source | |
| Accept as a minimal source edit | One-liner fix like FreeCamera.cpp; document in build.md | ✓ |
| Defer if mismatched | Raise finding and decide then | |

**User's choice:** Accept as minimal source edit (FreeCamera.cpp precedent)

---

## Claude's Discretion

None — user made explicit choices on all presented options.

## Deferred Ideas

None — discussion stayed within Phase 6 scope.
