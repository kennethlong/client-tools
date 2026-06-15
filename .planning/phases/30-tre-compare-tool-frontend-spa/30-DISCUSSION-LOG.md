# Phase 30: TRE Compare Tool — Frontend SPA - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-06-15
**Phase:** 30-tre-compare-tool-frontend-spa
**Areas discussed:** Screen layout, Serve/launch model, Tree presentation, Detail panel + hash

---

## Screen layout

| Option | Description | Selected |
|--------|-------------|----------|
| One page, linked | Pickers on top, set-delta collapsible strip, file-tree center, detail in right side-panel; clicking an archive scopes the tree | ✓ |
| One page, independent | Both visible, no cross-filtering | |
| Tabbed | Separate Set Delta / File Diff tabs, detail as modal/drawer | |

**User's choice:** One page, linked (→ D-01)
**Notes:** Set-delta and file-tree are linked lenses; archive selection cross-filters the file-tree.

---

## Serve/launch model

| Option | Description | Selected |
|--------|-------------|----------|
| FastAPI static-mounts SPA | `npm run build` → static bundle served by FastAPI at one URL; `python -m tre_compare` = one process; Vite dev-proxy only in dev | ✓ |
| Two processes always | Vite + FastAPI separate even for normal use | |

**User's choice:** FastAPI static-mounts SPA (→ D-02)
**Notes:** Satisfies SC#4 "single localhost server"; preserve the 127.0.0.1-only bind.

---

## Tree presentation

| Option | Description | Selected |
|--------|-------------|----------|
| Nested folder tree | Collapsible folders with roll-up counts, collapsed to top by default, TanStack Virtual over flattened visible rows | ✓ |
| Flat list + group toggle | Flat virtualized list with optional group-by-folder | |
| Flat + search only | Flat list, no nesting, navigate via search | |

**User's choice:** Nested folder tree (→ D-03)
**Notes:** Matches SC#1 "folder roll-up" literally; full payload arrives once, all tree/filter/search in-browser.

---

## Detail panel + hash

| Option | Description | Selected |
|--------|-------------|----------|
| Auto on select | Selecting a file auto-calls /file/detail (xxhash both sides), shows verdict + path-CRC labeled "not content"; cached | ✓ |
| On-demand verify button | Metadata immediately; "Verify content" button triggers hash | |

**User's choice:** Auto on select (→ D-04)
**Notes:** path-CRC must be visibly labeled NOT a content signal; `identical-by-metadata` must look distinct from `content-confirmed-identical`.

---

## Claude's Discretion

- shadcn component selection, badge/color palette, theme (dark OK), TanStack Query vs fetch, exact bundle-mount path, build-committed-vs-on-demand.
- Empty/loading/error states for each route (404 from /file/detail, corrupt-archive fault rows).

## Deferred Ideas

- TRE management / extract / repack / IFF-aware diff — TREM-01..03, future milestone.
- Content-comparing encrypted Restoration payloads — out of scope (payloads encrypted); first real run is SWGSource-vs-whitengold.
- Reviewed-but-not-folded todos (false keyword matches): cantina corner-snap (Phase 25), d3dcompile port (Phase 27), x64 port (future) — all client-hardening/platform, not the TRE tool.
