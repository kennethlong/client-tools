# Phase 26: Instrumentation Removal + Options-Window FATAL - Context

**Gathered:** 2026-06-13
**Status:** Ready for planning
**Source:** Inline capture (decisions taken with Kenny 2026-06-13; discuss-phase skipped for a well-understood cleanup phase)

<domain>
## Phase Boundary

Two independent cleanup items, both bounded by the core boot invariant.

1. **HARD-03 (PARTIAL this phase) — strip ONLY the D-15 DPVS profiling instrumentation.** The
   Phase 23 DPVS verdict (shipped as the Phase 24 config-gate) supersedes the profiling purpose
   of the D-15 instrumentation, so it is now dead weight and is removed.
2. **HARD-04 — fix the Options-window FATAL** (`checkShowToolbarCommandCooldownTimer` `.ui`-data
   mismatch, introduced by feature commit `d1b3c0eaf`). Pre-existing, OUTSIDE any prior milestone
   diff; opening the Options window currently FATALs.

**Explicitly OUT of scope this phase:** removal of the CORNERSNAP `_DEBUG` instrumentation
(see decision below). HARD-03 therefore completes only partially in Phase 26; the CORNERSNAP
half is deferred until the door snap is actually resolved.
</domain>

<decisions>
## Implementation Decisions

### CORNERSNAP instrumentation — KEEP (do NOT strip this phase)
- **Locked decision (Kenny, 2026-06-13):** Leave the CORNERSNAP `_DEBUG` probes in place.
- **Why:** They are the acceptance harness for the cantina **door snap**, which Phase 25 did
  NOT resolve — the re-entrancy guard fix was built and REVERTED (mechanism falsified), the
  interior snap is resolved-by-config only, and the residual door snap is root-caused to a
  32-bit-build/codegen float transient and PARKED for x64 / HARD-05. Stripping the harness now
  would leave us blind when the door snap is revisited.
- **Sites (do NOT touch):** `src/engine/shared/library/sharedCollision/src/shared/core/CollisionResolve.cpp`,
  `src/engine/shared/library/sharedObject/src/shared/portal/CellProperty.cpp`.
- **Revisit:** strip CORNERSNAP as part of the x64 port / HARD-05 work, once the door snap is
  actually fixed and verified against these probes.

### D-15 DPVS instrumentation — STRIP atomically
- Remove the dedicated profiling module and every reference to it in one atomic change so the
  tree never has a dangling symbol.
- **Known sites (planner/researcher to confirm exhaustively):**
  - `src/engine/client/library/clientGraphics/src/shared/DpvsProfileInstrumentation.cpp` (module impl)
  - `src/engine/client/library/clientGraphics/include/public/clientGraphics/DpvsProfileInstrumentation.h` (module header)
  - `src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp` + `RenderWorld.h` (call sites)
  - `src/engine/client/library/clientGame/src/shared/core/Game.cpp` (lifecycle / install)
  - `src/engine/client/library/clientGraphics/src/win32/SetupClientGraphics.cpp` (setup wiring)
  - `src/engine/client/library/clientUserInterface/src/shared/core/CuiIoWin.cpp` (any debug-key toggle)
  - `src/game/client/library/swgClientUserInterface/src/shared/parser/SwgCuiCommandParserDefault.cpp` (any debug command)
  - `src/engine/client/library/clientGraphics/build/win32/clientGraphics.vcxproj` (remove the two compiled files from the project)
- The `.vcxproj` edit must be inline (the `.rsp` is vestigial — see decruft gotchas).

### HARD-04 — Options-window FATAL
- Root cause is a `.ui`-data vs code mismatch in `checkShowToolbarCommandCooldownTimer`
  (`src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiOptUi.cpp`), traced to
  feature commit `d1b3c0eaf`. Fix is "fix the caller/data, not a diagnostic patch" — make the code
  tolerant of (or correct against) the actual shipped `.ui` widget set so opening Options no longer
  FATALs.

### Sequencing & atomicity
- HARD-03 (D-15) and HARD-04 are independent in code surface; either can land first. Each is its
  own atomic commit.
</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Roadmap / state
- `.planning/ROADMAP.md` — Phase 26 section (HARD-03, HARD-04 definitions + sequencing constraints)
- `.planning/REQUIREMENTS.md` — HARD-03, HARD-04 acceptance text
- `.planning/STATE.md` — Phase 25 outcome (door snap parked → CORNERSNAP-keep rationale), boot invariant, /FORCE false-pass blocker

### Source surfaces
- D-15 sites listed under "D-15 DPVS instrumentation — STRIP" above
- CORNERSNAP sites listed under "CORNERSNAP — KEEP" above (read so the executor knows what NOT to touch)
- `src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiOptUi.cpp` (Options FATAL)
</canonical_refs>

<specifics>
## Specific Ideas / Constraints

- **Boot invariant (every client-touching task):** client must stay bootable to character select
  under BOTH `rasterMajor=5` (D3D9) and `rasterMajor=11` (D3D11). Debug exe reads `client_d.cfg`;
  Release exe reads `client.cfg` — set rasterMajor in the right file per smoke. Claude owns the
  cfg toggle on renderer switches.
- **/FORCE false-pass guard:** SwgClient links under `/FORCE`, which downgrades unresolved
  externals to WARNINGS and still emits a binary with exit 0. After the atomic D-15 removal, grep
  the link output for `unresolved external symbol` (must be 0) — do NOT trust exit code alone.
- **Shared-header ABI cascade:** `DpvsProfileInstrumentation.h` lives in clientGraphics public
  include. If any plugin (gl05/gl07/gl11) includes it transitively, rebuild ALL plugin .vcxprojs
  after removal or risk a stale-DLL ABI crash. Verify whether the header is plugin-visible before
  declaring done.
- **Build mechanics (decruft gotchas):** edit the `.vcxproj` inline (`.rsp` vestigial); run msbuild
  via `$env:MSBUILD` full path with `/nodeReuse:false`; from PowerShell, not Git Bash; delete the
  target exe/dll to force a relink when validating "0 unresolved".
- Validate D-15 removal under BOTH Debug and Release link-grep (Optimized config has a pre-existing
  SAFESEH LNK1281 that is NOT a regression — do not chase it).
</specifics>

<deferred>
## Deferred Ideas

- **CORNERSNAP instrumentation removal** — deferred to the x64 port / HARD-05 work, after the door
  snap is fixed and verified against these probes. This is the second half of HARD-03.
</deferred>

---

*Phase: 26-instrumentation-removal-options-window-fatal*
*Context gathered: 2026-06-13 via inline capture*
