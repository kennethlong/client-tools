# Phase 15: In-Game Browser (XPCOM/Mozilla) Removal & Milestone Gate - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-05-26
**Phase:** 15-in-game-browser-xpcom-mozilla-removal-milestone-gate
**Areas discussed:** UITypeID enum surgery, TCG browser caller, Milestone gate depth

---

## Gray Area Selection

Presented as multiSelect (removal mechanics already locked by the Phase 12–14 precedent):
UITypeID enum surgery / TCG browser caller / Milestone gate depth.

**User selected:** all three.

---

## UITypeID enum surgery

Scout finding presented: `ui` library is **name-keyed** (`GetTypeName`/`TypeName`,
`UIStandardLoader<T>`); `UITypeID` is in-memory RTTI for `IsA()`, not a serialized TRE row
index — structurally different from the CR-01 case where the enum ordinal *was* the
`radial_menu.iff` row index.

| Option | Description | Selected |
|--------|-------------|----------|
| Verify-then-clean-delete | Researcher confirms no UITypeID ordinal is serialized to TRE/.ui, then delete TUIWebBrowser + all IsA clauses; placeholder fallback only if int-persistence found | ✓ |
| Ordinal-preserving placeholder | Reserve the slot with a RESERVED enumerator regardless (mirrors CR-01 fix); maximally safe but leaves a vestigial dead enumerator | |
| Clean-delete, no verify | Just delete mid-enum and fix call sites; ignores the CR-01 lesson | |

**User's choice:** Verify-then-clean-delete (Recommended).
**Notes:** Evidence-backed (name-keyed loader). The mandatory research step is confirming no
`UITypeID` integer is ever persisted to disk/TRE; the hard fallback (D-02a) is the
ordinal-preserving placeholder if any int-persistence is found. The link gate and char-select
boot gate cannot catch an ordinal-shift regression, so this verification is non-negotiable.

---

## TCG browser caller

Scout finding presented: `SwgCuiTcgControl`/`SwgCuiTcgWindow` are built on `libEverQuestTCG`
(separate live middleware), NOT libMozilla. Browser ties are only (1) two `__stdcall` navigate
callbacks in `SwgCuiTcgManager` that open `SwgCuiWebBrowserManager`, and (2)
`SwgCuiTcgControl::IsA` claiming `TUIWebBrowser`.

| Option | Description | Selected |
|--------|-------------|----------|
| Gut callback bodies, keep registration | Strip the browser calls, leave navigate procs as logging no-ops still registered with libEverQuestTCG; drop the TUIWebBrowser IsA clause | |
| Also unregister the callbacks | Beyond gutting bodies, remove the libEverQuestTCG callback registrations entirely so no dead procs remain; cleaner grep-zero but touches the lib's API surface | ✓ |
| You decide | Let researcher/planner pick based on whether libEverQuestTCG requires the callbacks | |

**User's choice:** Also unregister the callbacks.
**Notes:** Go cleaner — no orphan procs. Research caveat captured as D-04a: confirm
`libEverQuestTCG` tolerates absent/null navigate callbacks before unregistering; if the lib
requires them registered, fall back to registered logging-no-op procs rather than risk a
null-callback crash in live middleware. libEverQuestTCG and all TCG card-rendering infra stay.

---

## Milestone gate depth

DECRUFT-07 closes v2.1; roadmap criterion #4 specifies a "milestone-wide" gate after the full
DECRUFT-01..06 removal set.

| Option | Description | Selected |
|--------|-------------|----------|
| Full sweep + clean rebuild | All-6 residue re-grep + clean-from-scratch rebuild (wipe compile/ + stage binaries) Debug+Release link-grep 0 + dual-renderer boot; catches stale-object/incremental-link masking | |
| Full residue sweep, incremental build | Same all-6 residue re-grep + Debug+Release link-grep 0 + dual-renderer boot, but normal incremental build (no full wipe) | ✓ |
| Per-phase boot only | Just the standard dual-renderer boot after XPCOM removal; no milestone-wide sweep | |

**User's choice:** Full residue sweep, incremental build.
**Notes:** Re-grep all 6 removed subsystems (trackIR/stationapi/SwgClientSetup/lcdui/
videocapture/vivox/xpcom) repo-wide == 0, excluding the known KEEP-listed false positives
(soePlatform/ChatAPI2 community-chat getVoice*, soePlatform/libs prebuilt VChatAPI libs).
Debug+Release link-grep 0 unresolved + dual-renderer boot. Trust MSBuild incremental dependency
tracking — no full clean-from-scratch wipe. Captured as D-12.

---

## Claude's Discretion

- Exact edit order, plan/wave breakdown, and commit granularity within the removal — provided
  every coherent step keeps the tree buildable and the link gate green.
- Whether the milestone residue sweep is its own plan/wave or folded into the final boot-gate plan.

## Deferred Ideas

- Bink video codec / Miles audio — OUT of v2.1 scope (active middleware).
- `libEverQuestTCG` (TCG card game) removal — dormant, plausible future decruft target, but NOT
  in the v2.1 requirement set; Phase 15 severs only the browser tie.
- DEF-14-01 — SwgClient Optimized config LNK1281 SAFESEH defect (pre-existing, removal-unrelated).
- v2.2 Visual Parity (asset PS pipeline) — next milestone.
- Reviewed todos (not folded): SWGSource-vs-whitengold TRE asset diff (0.6, → v2.2); cantina
  corner-snap engine quirk (0.2, pre-existing, not Decruft).
