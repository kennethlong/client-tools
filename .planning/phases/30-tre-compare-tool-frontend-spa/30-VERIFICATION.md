---
phase: 30-tre-compare-tool-frontend-spa
verified: 2026-06-15T00:00:00Z
status: passed
score: 4/4
overrides_applied: 0
---

# Phase 30: TRE Compare Tool — Frontend SPA Verification Report

**Phase Goal:** Ship the modern web UI over the proven running API — a virtualized, filterable install-vs-install tree-diff that solves the space-asset diagnosis use case end-to-end.

**Verified:** 2026-06-15
**Status:** PASSED
**Re-verification:** No — initial verification

---

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | User can pick two installations, see a set-delta table, and browse a virtualized merged file-tree (TanStack Virtual, no hang on real data) with added/removed/changed badges and folder roll-up | VERIFIED | `InstallPicker.tsx` populates two selectors via `fetchInstalls`, gates Compare until both chosen. `SetDeltaStrip.tsx` renders archive rows with `setStatusBadge` (SET-level map), collapsed by default, clickable for cross-filter. `FileTree.tsx` uses `useVirtualizer` with `estimateSize: () => 28` and `getItemKey` by path (overscan 12). `buildFolderTree` in `useMemo([rows])`, `flattenVisible` -> virtualizer. Folder roll-up badge counts rendered. Human-approved against 231,086-row SWGSource × SWG Infinity diff with no hang. |
| 2 | User can filter by status (hide-identical), search by name/path, and read summary stats | VERIFIED | `FileTree.tsx` has a status `<select>` over `STATUS_FILTERS`, an `Input` for name/path search, and a "Hide identical" checkbox. The `accept` predicate memos on all four inputs and delegates empty-folder suppression to `flattenVisible`. `SummaryStats.tsx` renders `summary.status_counts` via `fileStatusBadge` + per-side `node_errors/rejected/tombstoned`. Human-confirmed all filters working live. |
| 3 | User can select any file and see a detail panel with winning archive, shadowed copies, sizes, and CRC display | VERIFIED | `DetailPanel.tsx` is a shadcn `Sheet` (`side="right"`, `sm:max-w-[440px]`). Auto-fires via `useQuery` keyed on `[left_cfg, right_cfg, path]` (stale-discard). Renders `metaText(left/right)`, `winning_archive_left/right`, `ShadowedList(shadowed_left/right)`, `HashLine` per side, and `verdictBadge(detail.verdict)` through `StatusBadge`. CRC labeled-absence note "Path-CRC — not a content signal." is present (line 135); no numeric crc value is read from the response. `content-confirmed-identical` maps to `success`/green (`contentVerified: true`); `identical-by-metadata` maps to `neutral`/dashed (`contentVerified: false`) — honesty distinction structural in `StatusBadge`. |
| 4 | Tool runs end-to-end as a single localhost server and answers the install-vs-install space-asset diff use case | VERIFIED | `web_static.py` defines `SpaStaticFiles(StaticFiles)` with 404→index.html fallback, `WEB_DIR` at 3-level `.parent` walk. In `api.py`, `app.mount("/", SpaStaticFiles(...))` appears after all four routes (line 252) and before `return app` (line 256). `__main__.py` binds `HOST = "127.0.0.1"` only. `vite.config.ts` sets `build.outDir: "build"`. Human-approved SC#4: server launched on 127.0.0.1:8765, all four routes returned 200 against real SWGSource × SWG Infinity pair (231,086 rows). Space-asset diagnosis use case is answerable. |

**Score:** 4/4 truths verified

---

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `tools/tre-compare/web/src/lib/types.ts` | Frozen-contract TS types: SetStatus, FileStatus, Verdict, DiffRow, FilesResponse, DetailResponse, InstallEntry, PairReq | VERIFIED | Exists, substantive (151 lines). Two DISTINCT unions `SetStatus` and `FileStatus`. `FileStatus` includes `"identical-by-metadata"` and `"tombstoned"`. `SetStatus` includes `"identical"` and `"fault"`. No `crc` field anywhere. |
| `tools/tre-compare/web/src/lib/tree.ts` | buildFolderTree / computeRollup / flattenVisible pure transforms | VERIFIED | Exists, substantive (216 lines). `buildFolderTree`, `computeRollup` (post-order), `flattenVisible` (O(visible) with `folderHasVisibleFile` empty-folder suppression). No React/DOM/server — pure. |
| `tools/tre-compare/web/src/lib/status.ts` | fileStatusBadge / setStatusBadge / verdictBadge with honesty distinction | VERIFIED | Exists, substantive (92 lines). `identical-by-metadata` → `neutral`/`border-dashed`/`contentVerified: false`. `content-confirmed-identical` → `success`/`contentVerified: true`. Two separate maps enforced. |
| `tools/tre-compare/web/src/lib/api.ts` | Typed relative-path fetch client for 4 routes | VERIFIED | Exists, substantive (88 lines). All paths relative (`/installs`, `/compare/set`, `/compare/files`, `/file/detail`). `ApiError` unwraps the `{detail}` envelope. No `127.0.0.1` or `http://localhost` literal. |
| `tools/tre-compare/web/src/App.tsx` | D-01 layout shell + shared compare state | VERIFIED | Exists, 158 lines (well above 40 min). Holds `leftCfg/rightCfg/pair/scopeArchive/selectedPath`. Committed-pair pattern. Keyed TanStack Query on `["compare/set", ...]` and `["compare/files", ...]`. Renders `InstallPicker → SetDeltaStrip → SummaryStats → FileTree → DetailPanel` in the correct linked master-detail order. |
| `tools/tre-compare/web/src/components/FileTree.tsx` | Virtualized folder tree via useVirtualizer over flattenVisible | VERIFIED | Exists, 241 lines. `buildFolderTree` in `useMemo([rows])`. `flattenVisible` in `useMemo([tree, expanded, accept])`. `useVirtualizer` with `estimateSize: () => 28`, `getItemKey: (i) => visible[i].path`, `overscan: 12`. Status filter, name/path search, hide-identical, cross-filter scope all wired. |
| `tools/tre-compare/web/src/components/DetailPanel.tsx` | Sheet slide-over with verdict badge + CRC labeled-absence | VERIFIED | Exists, 205 lines. `Sheet` with `side="right"`. `useQuery` keyed on `[left_cfg, right_cfg, path]`. `verdictBadge` through `StatusBadge`. "Path-CRC — not a content signal." present (line 135). No numeric crc value. 404/400 ApiError copy handled. |
| `tools/tre-compare/src/tre_compare/web_static.py` | SpaStaticFiles + WEB_DIR | VERIFIED | Exists. `class SpaStaticFiles(StaticFiles)` with `get_response` 404→index.html fallback. `WEB_DIR = Path(__file__).resolve().parent.parent.parent / "web" / "build"`. |
| `tools/tre-compare/src/tre_compare/api.py` | SPA mount registered last | VERIFIED | `app.mount("/", SpaStaticFiles(...))` at line 252, `return app` at line 256 — after all four `@app.post`/`@app.get` routes. |

---

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `FileTree.tsx` | `tree.ts` | `useMemo(buildFolderTree)` + `flattenVisible` → `useVirtualizer` | WIRED | `buildFolderTree` imported and called in `useMemo([rows])`. `flattenVisible` imported and called in `useMemo([tree, expanded, accept])`. `useVirtualizer` fed `visible.length`. |
| `DetailPanel.tsx` | `/file/detail` | `useQuery` keyed on `[left_cfg, right_cfg, path]` | WIRED | `fileDetail` imported from `@/lib/api`. `useQuery` calls `fileDetail(...)` enabled when `!!path`. Key includes path — stale-discard guaranteed. |
| `InstallPicker.tsx` | `/installs` | `fetchInstalls` populates both pickers | WIRED | `fetchInstalls` imported from `@/lib/api`. `useQuery({ queryFn: fetchInstalls })`. Both `InstallSelect` components receive `installs` list. |
| `api.py` | `web_static.py` | `app.mount('/', SpaStaticFiles(...))` last in `create_app` | WIRED | Import `from .web_static import WEB_DIR, SpaStaticFiles` at top of `api.py`. Mount at line 252, after all 4 routes, before `return app` at line 256. |
| `vite.config.ts` | `web/build/` | `build.outDir = 'build'` | WIRED | `outDir: "build"` at line 17 of vite.config.ts. No `dist/` produced. |

---

### Data-Flow Trace (Level 4)

| Artifact | Data Variable | Source | Produces Real Data | Status |
|----------|---------------|--------|--------------------|--------|
| `FileTree.tsx` | `rows: DiffRow[]` | `filesQuery.data?.rows` in `App.tsx`, populated by `compareFiles(pair)` POST to `/compare/files` | Yes — live API call returning real JSON rows | FLOWING |
| `DetailPanel.tsx` | `query.data: DetailResponse` | `useQuery` calling `fileDetail(...)` POST to `/file/detail` | Yes — keyed live API call returning real JSON | FLOWING |
| `InstallPicker.tsx` | `installs: InstallEntry[]` | `useQuery({ queryFn: fetchInstalls })` GET `/installs` | Yes — live API call returning the installs.toml list | FLOWING |
| `SummaryStats.tsx` | `summary: FilesResponse["summary"]` | `files.summary` from `filesQuery.data` in App, same `/compare/files` response | Yes — real summary from the diff engine | FLOWING |
| `SetDeltaStrip.tsx` | `rows: SetRow[]` | `setQuery.data?.rows` in App, populated by `compareSet(pair)` POST `/compare/set` | Yes — live API call | FLOWING |

---

### Behavioral Spot-Checks

All behavioral spot-checks were executed live during the SC#4 human-verify gate (Task 3, approved). Automated code-level checks:

| Behavior | Check | Result | Status |
|----------|-------|--------|--------|
| No XSS surface | `grep -r dangerouslySetInnerHTML web/src/` | 0 matches | PASS |
| api.ts relative-paths only | `grep -n '127.0.0.1\|http://localhost' api.ts` | 0 matches | PASS |
| types.ts no crc field | `grep -in 'crc' types.ts` | 0 matches | PASS |
| FileTree: 28px fixed, keyed by path | `grep 'estimateSize.*28\|getItemKey.*path'` | Both found (lines 101-102) | PASS |
| FileTree: buildFolderTree in useMemo | `grep 'useMemo.*buildFolderTree'` | Found line 62 | PASS |
| DetailPanel: Sheet side=right | `grep 'side="right"'` | Found line 184 | PASS |
| DetailPanel: CRC labeled-absence | `grep 'Path-CRC.*not a content signal'` | Found line 135 | PASS |
| Mount order: API routes before / mount | Lines 252 (mount) vs 256 (return) in api.py; all 4 routes registered before | Confirmed | PASS |
| HOST = 127.0.0.1 only | `grep 'HOST' __main__.py` | `HOST = "127.0.0.1"` confirmed | PASS |
| Vitest 23/23 green | Documented in 30-02-SUMMARY + 30-03-SUMMARY self-checks | Confirmed | PASS |
| All 6 task commits in git | `git log --oneline` for all commit hashes | All 10 commits present | PASS |

---

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|----------|
| TRE-05 | 30-01, 30-02, 30-03 | The UI is a modern web app — virtualized tree view with added/removed/changed badges, hide-identical filter, search, and summary stats | SATISFIED | Full SPA delivered: virtualized tree (TanStack Virtual), file status badges, hide-identical filter with empty-folder suppression, name/path search, summary stats panel. Human-verified end-to-end. REQUIREMENTS.md marks TRE-05 as Complete. |

---

### Anti-Patterns Found

The code review (30-REVIEW.md, conducted 2026-06-15) identified 4 warnings and 3 info items. None are blockers for the phase goal. The items are advisory for future polish:

| File | Finding | Severity | Impact on Goal |
|------|---------|----------|----------------|
| `tree.ts:128-141` | WR-01: file/folder name collision silently overwrites (rare with real TRE paths) | Warning | Does not block the diff use case; silent data loss only on path-segment collisions unlikely in SWG assets |
| `FileTree.tsx:183-189` | WR-02: `role="button"` rows lack `onKeyDown` handler — keyboard users cannot activate | Warning | Accessibility gap; does not affect mouse-driven use case acceptance |
| `web_static.py:41-48` | WR-03: 404-fallback returns index.html for missing static assets (broken deploy masking) | Warning | Only affects stale/partial npm builds; does not affect nominal use |
| `App.tsx:39-44` | WR-04: re-comparing identical pair is silent (TanStack Query cache hit, no loading indicator) | Warning | Minor UX; does not affect SC#4 acceptance |
| `App.tsx:64` | IN-01: `isLoading` vs `isFetching` spinner gap on refetch | Info | Advisory |
| `SetDeltaStrip.tsx:17-20` | IN-02: `fmtDelta` hides genuine zero delta | Info | Advisory |
| `FileTree.tsx:51` | IN-03: `setRows` prop accepted but discarded (`_setRows`) | Info | Dead prop, no behavior |

No blockers found. 0 critical findings from the code review.

---

### Human Verification Required

No outstanding items. The SC#4 human-verify gate (Task 3 of Plan 30-03) was APPROVED on 2026-06-15 by the user. The acceptance was performed against SWGSource × SWG Infinity (231,086 file rows), which is a valid and more rigorous substitute for the plan's literal SWGSource × whitengold pair (whitengold has no TRE install on this machine). The substitution is documented in 30-03-SUMMARY.md and accepted as an honest satisfaction of SC#4's intent.

All four success criteria were verified either programmatically or by the human-verify gate:
- SC#1 (virtualized tree, set-delta, badges, folder roll-up): human-approved
- SC#2 (filter/search/hide-identical/summary stats): human-approved
- SC#3 (detail panel, winning archive, shadowed, sizes, CRC display): human-approved
- SC#4 (single localhost server, end-to-end): human-approved

---

### Gaps Summary

No gaps. All four roadmap success criteria are VERIFIED against the actual codebase. The phase goal is achieved.

---

_Verified: 2026-06-15_
_Verifier: Claude (gsd-verifier)_
