---
phase: 30-tre-compare-tool-frontend-spa
plan: 03
subsystem: ui
tags: [react, typescript, tanstack-query, tanstack-virtual, shadcn, sheet, virtualized-tree, master-detail, frozen-contract]

# Dependency graph
requires:
  - phase: 30-01
    provides: "the Vite/React/TS/Tailwind/shadcn web project, the 8 shadcn primitives + TanStack libs, the dev-proxy/prod-same-origin relative-fetch seam, and the SpaStaticFiles mount that serves web/build/ from the single 127.0.0.1 FastAPI process"
  - phase: 30-02
    provides: "the pure data layer — types.ts (frozen contract), tree.ts (buildFolderTree/flattenVisible), status.ts (the honesty-distinction badge maps), api.ts (typed relative-path client + ApiError)"
provides:
  - "tools/tre-compare/web/src/App.tsx — the D-01 linked master-detail layout shell + shared compare state (leftCfg/rightCfg/committed-pair/scopeArchive/selectedPath); /compare/set + /compare/files via keyed TanStack Query"
  - "InstallPicker — fetchInstalls populates both pickers; Compare disabled until both chosen; 'No installations configured.' empty state"
  - "SetDeltaStrip — collapsible (collapsed by default), setStatusBadge (SET map), fault rows never dropped, archive-row click sets the tree cross-filter scope"
  - "SummaryStats — status_counts via the file badge map + per-side node_errors/rejected/tombstoned"
  - "FileTree — virtualized (useVirtualizer, 28px fixed, getItemKey by path) over flattenVisible; buildFolderTree memoized once per rows; collapsed-to-top default; folder roll-up badge counts; in-browser status filter + name/path search + Hide-identical (empty-folder suppression) + set-delta cross-filter"
  - "DetailPanel — shadcn Sheet (side=right, ~440px) auto-firing /file/detail keyed on [left_cfg,right_cfg,path] (stale-discard); verdict via verdictBadge (green = content only post-hash); sizes/winner/shadowed/hash; 404/400 ApiError copy; the CRC labeled-absence note (no numeric value)"
  - "StatusBadge — the shared semantic-variant -> shadcn Badge mapper (dark-zinc palette) + the honesty tooltip on metadata-only matches"
affects: [tre-compare-frontend, TRE-05, SC#4]

# Tech tracking
tech-stack:
  added: []  # no new deps — all libs installed in 30-01; this is the UI assembly over 30-02
  patterns:
    - "Committed-pair pattern: picker selection (leftCfg/rightCfg) is distinct from the committed compare pair (set on Compare click) so results don't churn while re-picking; queries keyed + enabled on the pair"
    - "Single semantic-badge mapper (StatusBadge) so the honesty distinction (≈ metadata neutral vs = content green) is rendered identically across tree/strip/summary/detail and cannot drift"
    - "Keyed-query stale-discard: /file/detail keyed on [left_cfg,right_cfg,path] (Pitfall 4) so rapid selection never shows a stale prior file"
    - "build-once / flatten-per-interaction tree (RESEARCH Pattern 1): useMemo(buildFolderTree,[rows]); accept predicate in useMemo; flattenVisible -> useVirtualizer @ 28px fixed"

key-files:
  created:
    - tools/tre-compare/web/src/components/InstallPicker.tsx
    - tools/tre-compare/web/src/components/SetDeltaStrip.tsx
    - tools/tre-compare/web/src/components/SummaryStats.tsx
    - tools/tre-compare/web/src/components/StatusBadge.tsx
    - tools/tre-compare/web/src/components/FileTree.tsx
    - tools/tre-compare/web/src/components/DetailPanel.tsx
  modified:
    - tools/tre-compare/web/src/App.tsx
    - tools/tre-compare/web/src/main.tsx
    - tools/tre-compare/.gitignore

key-decisions:
  - "StatusBadge added (not in files_modified list) as the single semantic-variant->shadcn-Badge mapper — shadcn Badge ships default/secondary/destructive/outline/ghost/link, NOT the semantic success/warning/neutral/tombstone from status.ts; mapping in one place keeps the honesty distinction structural across all five consumers"
  - "Committed-pair separate from picker selection — Compare is a manual trigger (no auto-fetch on pick) so the heavy /compare/files only runs on the explicit primary action and results are stable while re-picking"
  - "Set-delta -> tree cross-filter implemented as a best-effort PATH-stem scope (archive basename minus .tre/.toc, substring-matched against row.path) because the frozen /compare/files rows carry NO per-file archive field (only drill-in exposes winning_archive). This is the only archive association derivable in-browser without a backend change the frozen scope forbids."
  - "Native <select> for the install/status pickers (not a shadcn Select primitive — not in the installed set) — keyboard/SR-accessible, dense, zero extra wiring"

requirements-completed: []  # TRE-05 / SC#4 acceptance is the human-verify gate (Task 3) — NOT yet human-confirmed; see Checkpoint Status

# Metrics
duration: ~25min (autonomous tasks); SC#4 human gate pending
completed: 2026-06-15 (Tasks 1-2; Task 3 checkpoint-pending)
---

# Phase 30 Plan 03: Frontend Master-Detail SPA Summary

**The deliverable UI: the D-01 linked master-detail SPA over the 30-02 data layer — install pickers -> collapsible set-delta strip (cross-filter) -> virtualized merged file-tree (28px fixed, roll-up badges, in-browser filter/search/hide-identical) -> auto-verdict detail Sheet with the honesty distinction + CRC labeled-absence — building clean and proven end-to-end against two REAL installs at the API level. The final human-eyes-on-browser SC#4 gate is pending.**

> CHECKPOINT-PENDING: Tasks 1 and 2 are complete, committed, and build/test green. Task 3 is a `checkpoint:human-verify` gate (auto_advance=false) that requires a human to open the served SPA in a real browser and confirm the visual/interactive end-to-end. The backend is fully prepared and proven (all four routes return 200 on a real 26,513-row install diff); only the in-browser visual confirmation remains.

## Performance

- **Tasks:** 3 (2 autonomous build tasks done; 1 human-verify checkpoint pending)
- **Files created:** 6 components; **modified:** App.tsx, main.tsx, .gitignore
- **Build:** `npm run build` exits 0; `npm run test -- --run` 23/23 green (no regression to the 30-02 suite)

## Accomplishments
- Assembled the full D-01 master-detail SPA: `main.tsx` wraps `App` in `QueryClientProvider` + `TooltipProvider`; `App.tsx` owns the shared compare state and the keyed `/compare/set` + `/compare/files` queries; the layout flows pickers -> collapsible set-delta -> summary -> virtualized tree -> detail Sheet (linked lenses, not tabs).
- Built the virtualized focal pane (`FileTree`): `useMemo(buildFolderTree,[rows])` once, `flattenVisible` -> `useVirtualizer` at the LOCKED 28px fixed height with `getItemKey` by path + `overscan:12`; collapsed-to-top default; folder roll-up badge counts; in-browser status filter, name/path search, and Hide-identical (empty-folder suppression delegated to `flattenVisible`), plus the set-delta cross-filter scope.
- Built the auto-verdict detail Sheet (`DetailPanel`): shadcn `Sheet` (side=right, ~440px) auto-firing `/file/detail` keyed on `[left_cfg,right_cfg,path]` (stale-discard, Pitfall 4); sizes/winner/shadowed/per-side hash; verdict via `verdictBadge` (green `= content` only post-xxhash); 404/400 ApiError copy; the CRC labeled-absence note with NO numeric value.
- Encoded the honesty distinction structurally in `StatusBadge` (the single semantic->shadcn mapper) — `identical-by-metadata` renders neutral/dashed with a "not content-verified" tooltip; the post-hash `content-confirmed-identical` is the only solid green.
- Prepared + proved the SC#4 run at the API level: launched `python -m tre_compare` on 127.0.0.1:8765, all four routes 200 on a REAL two-install pair (SWGSource + SWGLegends `swgsource_3.0.tre`), `/compare/files` returning **26,513 rows** (a real large dataset for the virtualizer) and `/file/detail` returning a real xxhash + `content-confirmed-identical` verdict.

## Task Commits

1. **Task 1: app shell + install picker + set-delta strip + summary stats** - `3754b2a73` (feat)
2. **Task 2: virtualized file-tree + detail Sheet (auto-verdict, CRC labeled-absence)** - `362e2e576` (feat)
3. **Task 3: SC#4 end-to-end human-verify** - CHECKPOINT PENDING (no code; verification gate)

## Files Created/Modified
- `src/main.tsx` - QueryClientProvider (retry:false) + TooltipProvider wrap (keyed-detail dedup, Pitfall 4)
- `src/App.tsx` - D-01 shell + shared compare state; committed-pair pattern; keyed /compare/{set,files}; empty/loading/error states
- `src/components/InstallPicker.tsx` - fetchInstalls -> two pickers; Compare gated on both sides; 'No installations configured.' empty state
- `src/components/SetDeltaStrip.tsx` - collapsible (collapsed default); setStatusBadge; fault rows never dropped; archive-row click -> cross-filter scope + clear affordance
- `src/components/SummaryStats.tsx` - status_counts (file badge map) + per-side node_errors/rejected/tombstoned
- `src/components/StatusBadge.tsx` - semantic-variant -> shadcn Badge (dark-zinc palette) + honesty tooltip
- `src/components/FileTree.tsx` - virtualized tree (28px fixed, getItemKey path, overscan 12); buildFolderTree memoized once; collapsed default; roll-up badges; filter/search/hide-identical/cross-filter
- `src/components/DetailPanel.tsx` - Sheet (right, ~440px); auto /file/detail keyed; verdict badge; CRC labeled-absence; 404/400 copy
- `tools/tre-compare/.gitignore` - ignore local `verify-*.cfg` run artifacts

## Decisions Made
See `key-decisions` in frontmatter. Most consequential: `StatusBadge` as the single semantic->shadcn-Badge mapper (the honesty distinction cannot drift across the five consumers), and the set-delta cross-filter being a best-effort path-stem scope because the frozen `/compare/files` rows have no per-file archive field.

## Deviations from Plan

### Auto-added supporting artifact

**1. [Rule 2 - Missing critical functionality] Added StatusBadge.tsx (not in files_modified)**
- **Found during:** Task 1
- **Issue:** status.ts emits a semantic `BadgeVariant` (success/destructive/warning/neutral/tombstone) but the installed shadcn Badge ships only default/secondary/destructive/outline/ghost/link. Every component that shows a status (tree, strip, summary, detail) needs the same semantic->shadcn mapping + the honesty affordance — duplicating it five times would let the HARD-RULE honesty distinction drift.
- **Fix:** One `StatusBadge` component owns the variant->Badge+className map and the metadata-only "not content-verified" tooltip; all five consumers render through it.
- **Files added:** tools/tre-compare/web/src/components/StatusBadge.tsx
- **Committed in:** `3754b2a73`

### Constraint accommodation (frozen contract)

**2. [Rule 3 - Blocking] Set-delta cross-filter is a best-effort path-stem scope (no per-file archive field)**
- **Found during:** Task 2
- **Issue:** D-01 / RESEARCH say clicking a set-delta archive row "scopes the file-tree to that archive's files," but the frozen `/compare/files` rows carry NO per-file archive association (only `/file/detail` exposes `winning_archive`). A contract-faithful per-file archive filter is impossible without a backend field the frozen scope forbids (the same field-gap class as the path-CRC, anticipated by the UI-SPEC).
- **Fix:** The cross-filter scopes the tree by substring-matching the archive basename stem (minus `.tre`/`.toc`, lowercased) against `row.path` — the only archive association derivable in-browser. Documented in code + here; not a silent backend edit.
- **Files modified:** tools/tre-compare/web/src/components/FileTree.tsx (scopeToken + accept)
- **Committed in:** `362e2e576`

## Deferred Issues (out of this plan's scope — Phase-29 backend behaviors)

During the Task-3 verification prep, the API probe surfaced two Phase-29 backend behaviors. **Both are in frozen Phase-29 files (`cache.py` / `scanner.py`), NOT in this plan's files_modified, and Task 3 writes no code — so they are out of scope and logged here, not fixed:**

1. **`/compare/files` returns 500 (not a fault-degrade) when a `searchTree` archive is missing.** `/compare/set` correctly degrades a missing/corrupt archive to a `fault` row, but `/compare/files` raises `FileNotFoundError` in `cache._identity` -> bare 500. The UI-SPEC's "backend never 500s; it returns faults" expectation does not hold for `/compare/files` against a missing archive. Worth a Phase-29 fix (wrap the file-level scan in the same fault-degrade).
2. **Relative `searchTree` paths are not anchored to the cfg directory.** `scanner.py` uses the cfg value verbatim as `abspath`; a bare relative archive name (`searchTree_00_8=swgsource_3.0.tre`, as both shipped install cfgs use) only resolves from the install dir, so the tool's server (CWD `tools/tre-compare/`) FileNotFounds. Worked around for the run with absolute-path `verify-*.cfg` wrappers (gitignored). A Phase-29 fix would anchor relative search paths to `os.path.dirname(cfg_path)`.

These do not block the SPA itself — the UI renders fault rows and error envelopes correctly; they are install-cfg/backend-resolution issues that affect which install pairs run clean.

## Known Stubs
None. All six components render live data from the 30-02 client over the frozen routes — no hardcoded/empty data paths, no placeholder text that flows to render. (The pre-existing `lib/placeholder.test.ts` from 30-01/02 is unrelated and out of this plan's scope.)

## Checkpoint Status (Task 3 — human-verify, BLOCKING)

**Automated verification (executor-performed, all GREEN):**
- `npm run build` exits 0; `npm run test -- --run` 23/23 green.
- grep: no `dangerouslySetInnerHTML` anywhere (T-30-03-01).
- grep: `estimateSize: () => 28` present in FileTree; `getItemKey` by path; `buildFolderTree` in `useMemo` keyed on rows.
- grep: "Path-CRC — not a content signal." present in DetailPanel; no numeric crc read from the response.
- Server: `python -m tre_compare` bound `127.0.0.1:8765`; GET / -> 200 text/html; /installs -> both installs; /compare/set, /compare/files (26,513 rows), /file/detail all -> 200 on the REAL SWGSource+SWGLegends pair; /file/detail returned a real xxhash + `content-confirmed-identical` verdict.

**Remaining (genuine human gate — cannot be self-satisfied):** a human must open the served SPA in a real browser and visually confirm the virtualized tree scrolls without hang on the 26k-row data, the badge/honesty colors render, the Sheet slides over, and filter/search/hide-identical/cross-filter all behave. See the PLAN-COMPLETE checkpoint block for the exact step-by-step. Server is left running on 127.0.0.1:8765 for the human.

## Self-Check: PASSED

All 6 created components + the 2 modified source files exist on disk; both task commits (`3754b2a73`, `362e2e576`) are present in git history; build + 23-test suite green; the served SPA + all four API routes verified 200 on a real install pair.

---
*Phase: 30-tre-compare-tool-frontend-spa*
*Tasks 1-2 completed 2026-06-15; Task 3 human-verify checkpoint pending.*
