# Phase 30: TRE Compare Tool — Frontend SPA - Context

**Gathered:** 2026-06-15
**Status:** Ready for planning

<domain>
## Phase Boundary

Ship the **React + Vite + shadcn web UI** over the already-running, frozen tre-compare
FastAPI surface (Phase 29). The SPA delivers TRE-05: an install picker, an archive-level
set-delta table, a virtualized merged file-tree with added/removed/changed badges and
folder roll-up, in-browser filter/search/hide-identical, summary stats, and a per-file
detail panel (winning archive, shadowed copies, sizes, CRC, content verdict). The end-to-end
acceptance use case is the **SWGSource-vs-whitengold space-asset diff** (folded todo).

**Builds on (Phase 29, proven + verified):** the four stateless routes exposed by
`tre_compare.api.create_app(cache, installs_path)`:
- `POST /compare/set`  `{left_cfg, right_cfg}` → `{rows: [...]}` (archive set-delta)
- `POST /compare/files` `{left_cfg, right_cfg}` → `{rows, summary}` (FULL file-level diff, no pagination)
- `POST /file/detail`  `{left_cfg, right_cfg, path}` → drill-in detail (404 not_found / 400 rejected)
- `GET  /installs` → `[{name, cfg_path}]` (config-driven picker list)

**The backend contract is FROZEN — this phase is UI only.** No new routes, no backend
refactor (Phase 29 SUMMARY: "create_app + diff importable for Phase 30 with zero backend
refactor"). If the UI needs data the routes don't expose, that is a planning flag, not a
silent backend edit.

**Not in this phase:** no TRE write / extract / repack / IFF-aware diff (TREM-01..03,
deferred to a future milestone). v2.3 is **read-only compare**.
</domain>

<decisions>
## Implementation Decisions

### Screen layout (D-01)
- **D-01:** **Single-page, master-detail, linked.** Install pickers (left/right) on top;
  the archive **set-delta as a collapsible summary strip**; the **merged file-tree center**;
  the **per-file detail in a right side-panel** that slides over. Clicking an archive row in
  the set-delta **scopes the file-tree to that archive's files** (cross-filter). Set-delta and
  file-tree are linked lenses on the same compare, not separate tabs. *(Kenny selected.)*

### Serve / launch model (D-02)
- **D-02:** **FastAPI static-mounts the built SPA bundle.** `npm run build` (Vite) emits a
  static bundle that FastAPI serves at **one URL**; `python -m tre_compare` launches the whole
  app as a **single localhost process** (satisfies SC#4 "single localhost server"). The Vite
  dev server (proxying API calls to FastAPI) is used **only during development**. The shipped
  artifact is one command, one port. *(Kenny selected.)*
  - Planner: decide where the built bundle lives (e.g. `tools/tre-compare/web/dist/` mounted
    via `StaticFiles`) and whether the build is committed or built on demand. `__main__` already
    binds `127.0.0.1` only (T-29-03-03) — preserve that.

### File-tree presentation (D-03)
- **D-03:** **Nested collapsible folder tree with roll-up counts.** Folders show
  added/removed/changed roll-up badge counts; collapsed to top level by default. Rendered via
  **TanStack Virtual over the flattened set of currently-visible rows** (the proven approach for
  10k–100k entries on localhost — no hang). Matches SC#1 "folder roll-up" literally. The full
  flat diff array arrives in one `/compare/files` response (D-08 from Phase 29); all
  tree-building, filtering, search, and hide-identical happen **in-browser**. *(Kenny selected.)*

### Detail panel + content hash (D-04)
- **D-04:** **Auto content-verdict on file select.** Selecting a file auto-calls
  `/file/detail`; the panel shows sizes (`len`/`clen` both sides), the **winning archive**,
  **shadowed copies**, the **path-CRC explicitly labeled "path-CRC — not content"**, and the
  **content verdict** (`content-confirmed-identical` / `content-changed` /
  `content-indeterminate`) derived from the on-demand xxhash. Results are cache-memoized
  server-side so re-selecting a file is instant. *(Kenny selected.)*
  - **Honesty distinction (carried from Phase 29 D-06) MUST be visible in the UI:**
    `identical-by-metadata` (matched `(len, clen)`, NOT hash-verified) must be visually
    distinct from `content-confirmed-identical` (drill-in xxhash matched). Never present the
    cheap metadata match as a content guarantee. The TOC `crc` is a path-CRC — never used or
    shown as a change signal.

### Claude's Discretion
- shadcn component selection, exact color/badge palette, dark/light theme, table/tree library
  details, and TanStack Query vs plain fetch are planner/implementer choices — keep the UI
  consistent and dense (single-user diagnostic tool, not a consumer product).
- Empty/loading/error states for each route (e.g. 404 from `/file/detail`, a corrupt-archive
  `fault` row from `/compare/set`) — render gracefully; the backend never 500s on bad data.

### Folded Todos
- **`2026-05-15-swgsource-vs-whitengold-tre-asset-diff`** — "Diff SWGSource (Restoration /
  community) vs whitengold codebase + TRE files for space-scene graphics artifacts." This is
  the literal **SC#4 acceptance use case** for Phase 30: the shipped tool must answer this diff
  end-to-end. It defines the real-data smoke test for verification, not new scope. (NOTE:
  Restoration TRE payloads are encrypted — see encryption catalog; the realistic first run is
  SWGSource-vs-whitengold, both open-zlib.)

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Frozen backend contract (the data the SPA consumes)
- `tools/tre-compare/src/tre_compare/api.py` — `create_app` + the four routes and their exact
  JSON response shapes (`_merged_meta`, `_node_brief`, `_hash_brief` serializers). THE contract.
- `tools/tre-compare/src/tre_compare/diff.py` — `diff_archive_set`, `diff_virtual_trees`
  (row + summary schema), `drill_in` (winner/shadowed/verdict semantics).
- `tools/tre-compare/src/tre_compare/config.py` — `load_installs` / `installs.toml` shape behind `/installs`.
- `tools/tre-compare/installs.toml.example` — the picker config format (real `installs.toml` is gitignored).

### Phase 29 decisions that constrain the UI
- `.planning/phases/29-tre-compare-tool-diff-engine-api/29-CONTEXT.md` — D-06 (tri-state +
  honest `identical-by-metadata`), D-08 (full-payload, client-virtualized), D-09 (lean rows),
  D-07 (drill-in hash semantics), CRC-is-path-CRC warning.
- `.planning/phases/29-tre-compare-tool-diff-engine-api/29-VERIFICATION.md` — what's proven working.

### Phase / roadmap anchors
- `.planning/ROADMAP.md` §"Phase 30" — goal, 4 success criteria, UI hint.
- `.planning/REQUIREMENTS.md` — TRE-05.
- `.planning/todos/pending/2026-05-15-swgsource-vs-whitengold-tre-asset-diff.md` — folded acceptance use case.
- `tools/tre-compare/README.md` — how to run the backend.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- The entire Phase 28–29 Python backend is done and verified. `create_app()` is importable;
  the SPA is a pure client of its four routes.
- `tools/tre-compare/` is an isolated `uv` package — the `web/` frontend should sit alongside
  (its own `package.json` / Vite project) without entangling the engine MSBuild graph (D-01
  extractability invariant from Phase 28).

### Established Patterns
- Localhost-only bind (`127.0.0.1`, T-29-03-03) — no auth, single-user.
- Full-payload + in-browser virtualization (D-08) — the SPA owns filter/search/tree-build;
  the server does no pagination.
- Backend never 500s on bad data (fault rows, node_errors in summary) — the UI must surface
  these, not assume clean input.

### Integration Points
- Dev: Vite dev server proxies `/compare/*`, `/file/*`, `/installs` to the FastAPI port.
- Prod: FastAPI `StaticFiles` mount serves the built bundle from the same origin (no CORS).

</code_context>

<specifics>
## Specific Ideas

- The tool is a single-user, dense, localhost diagnostic — bias toward information density and
  speed over polish. Dark theme is fine.
- The path-CRC must be shown but unmistakably labeled as NOT a content signal — Kenny has
  repeatedly flagged the path-CRC-vs-content-hash trap as the tool's #1 correctness pitfall.

</specifics>

<deferred>
## Deferred Ideas

- TRE management / extract / repack / IFF-aware structural diff — TREM-01..03, future milestone.
- Comparing encrypted Restoration payloads by content — out of scope (payloads encrypted; the
  tool reads its index/metadata only). First real run is SWGSource-vs-whitengold (both open-zlib).

### Reviewed Todos (not folded)
- `2026-05-15-cantina-corner-snap-engine-improvement` — false keyword match (rendering/collision); belongs to the client-hardening stream (Phase 25, done), not the TRE tool.
- `2026-05-31-port-d3d9-shader-compile-to-d3dcompile` — false keyword match; HARD-05 / Phase 27 (done/deferred to x64).
- `2026-06-13-64bit-x64-port` — false keyword match; future-milestone platform work, unrelated to the SPA.

</deferred>

---

*Phase: 30-tre-compare-tool-frontend-spa*
*Context gathered: 2026-06-15*
