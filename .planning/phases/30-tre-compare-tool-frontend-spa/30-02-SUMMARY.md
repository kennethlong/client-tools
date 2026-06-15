---
phase: 30-tre-compare-tool-frontend-spa
plan: 02
subsystem: ui
tags: [typescript, vitest, react, tanstack-virtual, frozen-contract, pure-functions, tdd]

# Dependency graph
requires:
  - phase: 30-01
    provides: "Vite 8 + React 19 + TS 6 + Tailwind 4 + shadcn web project at tools/tre-compare/web/ with a wired Vitest harness, the @ alias, and the dev-proxy/prod-same-origin relative-fetch seam"
  - phase: 29-tre-compare-tool-diff-engine-api
    provides: "the FROZEN four-route FastAPI contract (diff.py status enums + api.py serializers) this data layer transcribes verbatim into types.ts"
provides:
  - "tools/tre-compare/web/src/lib/types.ts — frozen-contract TS types: SetStatus + FileStatus (two DISTINCT unions), Verdict, SideMeta, DiffRow, FilesResponse, SetRow, NodeBrief, HashBrief, DetailResponse, InstallEntry, PairReq, FileDetailReq — NO path-checksum member"
  - "tools/tre-compare/web/src/lib/tree.ts — pure buildFolderTree / computeRollup / flattenVisible transforms (flat array -> virtualized folder tree, RESEARCH Pattern 1) with empty-folder suppression via rollup (Pitfall 6)"
  - "tools/tre-compare/web/src/lib/status.ts — fileStatusBadge / setStatusBadge / verdictBadge descriptor maps enforcing the identical-by-metadata vs content-confirmed-identical honesty distinction (D-04/D-06 HARD RULE)"
  - "tools/tre-compare/web/src/lib/api.ts — relative-path typed fetch client (fetchInstalls/compareSet/compareFiles/fileDetail) + ApiError surfacing the {detail:{error,path/cfg}} envelope"
  - "23-test Vitest unit suite (10 tree + 12 status + 1 carried placeholder) proving the two highest-risk seams before any UI renders"
affects: [30-03, tre-compare-frontend, wave-2-ui]

# Tech tracking
tech-stack:
  added: []  # no new deps — all libs were installed in 30-01; this is the pure data layer
  patterns:
    - "Frozen-contract transcription: TS unions/interfaces mirror diff.py enum literals + api.py serializer field names verbatim; two SEPARATE status unions (SetStatus vs FileStatus) never one (Pitfall 3)"
    - "Pure-function validation seam: tree-build/flatten + status->badge maps are framework-free (no React/DOM/server) so they TDD in Vitest decoupled from any UI"
    - "Build-once / flatten-per-interaction tree model (RESEARCH Pattern 1) with O(visible) re-flatten + rollup-driven empty-folder suppression"
    - "Honesty distinction encoded in the type+badge layer: solid-green reserved exclusively for the post-xxhash verdict; metadata match is neutral + contentVerified:false"
    - "Relative-path fetch client (no hardcoded host) so dev-proxy + prod-same-origin share one code path; ApiError unwraps the FastAPI {detail} envelope"

key-files:
  created:
    - tools/tre-compare/web/src/lib/types.ts
    - tools/tre-compare/web/src/lib/tree.ts
    - tools/tre-compare/web/src/lib/tree.test.ts
    - tools/tre-compare/web/src/lib/status.ts
    - tools/tre-compare/web/src/lib/status.test.ts
    - tools/tre-compare/web/src/lib/api.ts
  modified: []

key-decisions:
  - "Rollup `identical` bucket counts BOTH identical-by-metadata AND tombstoned file statuses — so hide-identical suppresses a folder whose entire subtree is non-changing (Pitfall 6) and the bucket means 'not a real add/remove/change'"
  - "BadgeDescriptor is a small string descriptor {label, variant, className?, contentVerified?} rather than JSX — keeps status.ts pure + unit-testable; the component maps `variant` onto a shadcn Badge variant in 30-03"
  - "BadgeVariant adds a `tombstone` (violet/neutral outline) variant distinct from `neutral` so a deliberate engine removal never reads as a plain set-level identical"
  - "Field-gap (D-04 Option 1) honored: NO path-checksum member in types.ts; the labeled-absence note is a 30-03 component concern, not a type"
  - "empty-folder suppression uses a short-circuiting folderHasVisibleFile subtree walk (returns on the first accepted descendant) rather than precomputed filtered-rollups — correct under an arbitrary `accept` predicate, still cheap"

patterns-established:
  - "Two-separate-status-unions + two-separate-badge-maps (SetStatus/FileStatus) — the structural guard against Pitfall 3 for every Wave-2 component"
  - "Pure lib/ transform + co-located *.test.ts RED->GREEN Vitest seam — the decoupled validation pattern 30-03 components build on"

requirements-completed: [TRE-05]

# Metrics
duration: 6min
completed: 2026-06-15
---

# Phase 30 Plan 02: Frontend Data Layer Summary

**The pure, framework-free Wave-2 data layer — frozen-contract TS types (two distinct status unions, no path-checksum field), the flat-array→virtualized-folder-tree transforms with empty-folder suppression, the identical-by-metadata vs content-confirmed-identical honesty-distinction badge maps, and a relative-path typed fetch client — all proven under a 23-test Vitest suite.**

## Performance

- **Duration:** 6 min
- **Started:** 2026-06-15T15:14:28Z
- **Completed:** 2026-06-15T15:20:30Z
- **Tasks:** 2 (both TDD: RED + GREEN each)
- **Files modified/created:** 6 (3 source + 1 test for Task 1; 2 source + 1 test for Task 2)

## Accomplishments
- Transcribed the FROZEN Phase-29 contract into `types.ts` verbatim: `SetStatus` and `FileStatus` as two DISTINCT unions (Pitfall 3), `Verdict`, `DiffRow`/`FilesResponse`/`SetRow`/`NodeBrief`/`HashBrief`/`DetailResponse`/`InstallEntry`/`PairReq` — with NO path-checksum member (D-04 Option 1, contract-preserving).
- Implemented the #1-risk pure transforms in `tree.ts` per RESEARCH Pattern 1 (`buildFolderTree` build-once, `computeRollup` single post-order pass, `flattenVisible` O(visible)) including the empty-folder suppression so hide-identical never leaves empty expandable folders (Pitfall 6) — 10 Vitest cases green.
- Encoded Kenny's #1 correctness pitfall in `status.ts`: `fileStatusBadge`/`setStatusBadge`/`verdictBadge` with the HARD RULE that `identical-by-metadata` (neutral `≈ metadata`, `contentVerified:false`) is visually distinct from `content-confirmed-identical` (solid green `= content`) — two separate maps for the two enums — 12 Vitest cases green.
- Wrote the relative-path typed `api.ts` client (no hardcoded host) whose `ApiError` surfaces the `{detail:{error,path/cfg}}` envelope on a non-ok 400/404.

## Task Commits

Each task was committed atomically (TDD RED -> GREEN):

1. **Task 1 (RED): failing tree.test.ts** - `9c32f5fd0` (test)
2. **Task 1 (GREEN): types.ts + tree.ts** - `d52a6df78` (feat)
3. **Task 2 (RED): failing status.test.ts** - `3bb08b864` (test)
4. **Task 2 (GREEN): status.ts + api.ts** - `00e44fc04` (feat)

**Plan metadata:** (this commit) `docs(30-02): complete data-layer plan`

_REFACTOR phase skipped both tasks — the GREEN implementations were clean; no behavior-preserving cleanup warranted._

## Files Created/Modified
- `tools/tre-compare/web/src/lib/types.ts` - Frozen-contract types; `SetStatus`/`FileStatus` distinct unions, `Verdict`, the four-route response/request interfaces; no path-checksum field
- `tools/tre-compare/web/src/lib/tree.ts` - `buildFolderTree`/`computeRollup`/`flattenVisible` + the `FolderNode`/`FileNode`/`VisibleRow`/`Rollup` node model; empty-folder suppression
- `tools/tre-compare/web/src/lib/tree.test.ts` - 10 cases: nesting, rollup counts, collapsed-default, expand-walk depth, stable keys, hide-identical drop + nested empty-folder suppression
- `tools/tre-compare/web/src/lib/status.ts` - `fileStatusBadge`/`setStatusBadge`/`verdictBadge` -> `BadgeDescriptor`; the honesty distinction; `BadgeVariant` palette incl. `tombstone`
- `tools/tre-compare/web/src/lib/status.test.ts` - 12 cases incl. the HARD-RULE distinct-styling assertion and the two-maps-don't-collide assertion
- `tools/tre-compare/web/src/lib/api.ts` - `fetchInstalls`/`compareSet`/`compareFiles`/`fileDetail` relative-path client + `ApiError` envelope unwrap

## Decisions Made
See `key-decisions` in frontmatter. Most consequential: the rollup `identical` bucket counts both `identical-by-metadata` and `tombstoned` (so a folder of only-non-changing files is suppressible), and `BadgeDescriptor` is a pure string descriptor (not JSX) so the badge maps stay framework-free and unit-testable — 30-03 maps `variant` onto a shadcn Badge.

## Deviations from Plan

Two trivial in-scope authoring corrections, both blocking-the-task (Rule 3), both inside the same commits — neither is a contract or behavior change:

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Discriminated-union narrowing in tree.test.ts broke `tsc -b`**
- **Found during:** Task 1 (first `npm run build`)
- **Issue:** A test asserted `aRow?.expanded` on a `VisibleRow` (union of folder|file); `expanded` exists only on the folder variant, so `tsc -b` failed TS2339 (the GREEN tests still ran green under Vitest, but `npm run build` is an acceptance gate).
- **Fix:** Narrowed by `kind` first (`if (aRow?.kind === "folder") expect(aRow.expanded)...`) — the idiomatic discriminated-union access.
- **Files modified:** tools/tre-compare/web/src/lib/tree.test.ts
- **Verification:** `npm run build` exits 0; 10 tree tests still green.
- **Committed in:** `d52a6df78` (Task 1 GREEN commit)

**2. [Rule 3 - Blocking] Acceptance greps matched explanatory comments**
- **Found during:** Task 1 (`crc` grep) and Task 2 (`127.0.0.1` grep)
- **Issue:** The acceptance criteria grep `types.ts` for `crc` and `api.ts` for `127.0.0.1` (must be empty). Both files initially carried those tokens ONLY in explanatory comments (documenting the field-gap / the dev-proxy target) — no actual field or hardcoded host. To satisfy the literal grep unambiguously, the comments were reworded ("path checksum" / "the dev FastAPI port").
- **Fix:** Reworded the two comments; no code/field change.
- **Files modified:** tools/tre-compare/web/src/lib/types.ts, tools/tre-compare/web/src/lib/api.ts
- **Verification:** `grep -in crc src/lib/types.ts` empty; `grep -inE '127\.0\.0\.1|http://localhost' src/lib/api.ts` empty; build still green.
- **Committed in:** `d52a6df78` (types.ts) + `00e44fc04` (api.ts)

---

**Total deviations:** 2 auto-fixed (both Rule 3 - blocking authoring/grep frictions)
**Impact on plan:** No contract change, no behavior change, no scope creep. The delivered types/transforms/maps/client match the plan + frozen contract exactly.

## Issues Encountered
None beyond the two trivial deviations above. RED failed correctly for both tasks (unresolved imports), GREEN passed first try, and the full suite + build stayed green.

## TDD Gate Compliance
Both tasks followed RED -> GREEN with visible gate commits: `test(...)` then `feat(...)` for each (`9c32f5fd0`->`d52a6df78`, `3bb08b864`->`00e44fc04`). REFACTOR was unnecessary (clean GREEN). Plan-level `type: tdd` gate sequence satisfied.

## User Setup Required
None - no external service configuration required. The data layer is pure TS; `npm run test`/`npm run build` use the Node 24 + npm 11 already present.

## Next Phase Readiness
- **Plan 30-03 (UI: pickers, set-delta strip, virtualized tree, detail Sheet) is fully unblocked.** It imports `types.ts` (no further contract exploration), `tree.ts` (memoize `buildFolderTree` on the `/compare/files` response, feed `flattenVisible` to a fixed-28px TanStack Virtualizer), `status.ts` (map `variant` onto shadcn Badge variants — note the HARD RULE), and `api.ts` (TanStack Query keyed on `[left_cfg,right_cfg,path]` over these typed calls, Pitfall 4).
- The honesty distinction and the two-separate-status-unions guard are now structural — a 30-03 component cannot accidentally render a metadata match as a content guarantee or share one status map.
- No blockers. Frozen Phase-29 contract consumed unchanged; no backend touch in this plan.

## Self-Check: PASSED

All 6 created files exist on disk; all 4 task commits (`9c32f5fd0`, `d52a6df78`, `3bb08b864`, `00e44fc04`) present in git history; full Vitest suite 23/23 green; `npm run build` exits 0.

---
*Phase: 30-tre-compare-tool-frontend-spa*
*Completed: 2026-06-15*
