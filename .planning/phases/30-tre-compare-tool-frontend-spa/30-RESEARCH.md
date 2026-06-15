# Phase 30: TRE Compare Tool — Frontend SPA - Research

**Researched:** 2026-06-15
**Domain:** React SPA (Vite + shadcn/ui + TanStack Virtual) over a frozen FastAPI localhost contract
**Confidence:** HIGH (stack versions, FastAPI static-serve, virtualization pattern verified; contract read directly from source)

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01 — Screen layout:** Single-page, master-detail, **linked**. Install pickers (left/right) on top;
  archive set-delta as a **collapsible summary strip**; merged file-tree center; per-file detail in a
  **right side-panel that slides over**. Clicking a set-delta archive row **scopes the file-tree to that
  archive's files** (cross-filter). Set-delta and file-tree are linked lenses on the same compare, not tabs.
- **D-02 — Serve / launch model:** FastAPI **static-mounts the built SPA bundle**. `npm run build` (Vite)
  emits a static bundle FastAPI serves at **one URL**; `python -m tre_compare` launches the whole app as a
  **single localhost process** (SC#4). Vite dev server (proxying API) is dev-only. Shipped artifact = one
  command, one port. Planner decides where `dist/` lives and whether the build is committed or built on demand.
  `__main__` already binds `127.0.0.1` only — preserve that.
- **D-03 — File-tree presentation:** Nested collapsible folder tree with roll-up counts. Folders show
  added/removed/changed roll-up badge counts; collapsed to top level by default. Rendered via **TanStack
  Virtual over the flattened set of currently-visible rows** (proven 10k–100k approach — no hang). Full flat
  diff array arrives in one `/compare/files` response (D-08); all tree-building, filtering, search, and
  hide-identical happen **in-browser**.
- **D-04 — Detail panel + content hash:** **Auto content-verdict on file select.** Selecting a file
  auto-calls `/file/detail`; panel shows sizes (`len`/`clen` both sides), **winning archive**, **shadowed
  copies**, the **path-CRC explicitly labeled "path-CRC — not content"**, and the **content verdict**
  (`content-confirmed-identical` / `content-changed` / `content-indeterminate`) from the on-demand xxhash.
  Results are server-cache-memoized so re-selecting is instant.
  - **Honesty distinction (carried from Phase 29 D-06) MUST be visible:** `identical-by-metadata` (matched
    `(len, clen)`, NOT hash-verified) must be **visually distinct** from `content-confirmed-identical`
    (drill-in xxhash matched). Never present the cheap metadata match as a content guarantee. The TOC `crc`
    is a path-CRC — never used or shown as a change signal.

### Claude's Discretion
- shadcn component selection, exact color/badge palette, dark/light theme, table/tree library details, and
  **TanStack Query vs plain fetch** are planner/implementer choices — keep the UI consistent and dense
  (single-user diagnostic tool, not a consumer product).
- Empty/loading/error states for each route (404 from `/file/detail`, a corrupt-archive `fault` row from
  `/compare/set`) — render gracefully; the backend never 500s on bad data.

### Deferred Ideas (OUT OF SCOPE)
- TRE management / extract / repack / IFF-aware structural diff — TREM-01..03, future milestone.
- Comparing **encrypted Restoration payloads by content** — out of scope (payloads encrypted; tool reads
  index/metadata only). First real run is **SWGSource-vs-whitengold** (both open-zlib).
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| TRE-05 | React/Vite/shadcn virtualized tree-diff SPA: install picker, set-delta table, badges, filter, search, per-file detail panel; runs as a single localhost server and answers the SWGSource-vs-whitengold space-asset diff. | Standard Stack (Vite 8 + React 19 + shadcn 4 + TanStack Virtual 3.14); Pattern 1 (flat-array → folder-tree → flatten-visible); Pattern 2 (FastAPI StaticFiles mount + dev proxy); Pattern 3 (project layout preserving D-01 extractability); the Route → UI State Map (exact field names); the Honesty-distinction section (D-06/D-04 UI rendering). |
</phase_requirements>

## Summary

Phase 30 is a **pure frontend** over a fully-frozen FastAPI contract (read directly from `api.py`/`diff.py` —
the four routes and their exact JSON shapes are documented below). There is **no backend work**: the only
backend touch the SPA needs is the static mount of the built bundle (D-02), which is an additive change to the
app factory / `__main__`, not a contract change. The genuine implementation risks are all client-side:
(1) building a virtualized folder tree from a flat 10k–100k-row array without hanging, (2) wiring the
FastAPI static-serve + Vite dev-proxy so prod and dev both work, (3) keeping the Vite project inside
`tools/tre-compare/` without entangling the engine MSBuild graph or the Python `dist/` directory, and
(4) faithfully rendering the **honest tri-state / content-verdict distinction** that is Kenny's flagged #1
correctness pitfall.

The stack is settled and current (June 2026): **Vite 8 + React 19 + TypeScript 6 + Tailwind 4 + shadcn CLI 4 +
TanStack Virtual 3.14**. shadcn on Vite/Tailwind-4 uses the `@tailwindcss/vite` plugin and `@/*` path aliases;
React 19 is the shadcn default. For four simple localhost POST/GET routes with no auth and no CORS in prod,
**plain `fetch` wrapped in a tiny typed client is sufficient** — but I recommend **TanStack Query** because the
two heavy responses (`/compare/files`, `/file/detail`) benefit materially from its built-in request
deduplication, caching, and loading/error state machine (the discretionary empty/loading/error states D-04
asks for become declarative). It is one small dependency and removes a class of hand-rolled state bugs.

**Primary recommendation:** Scaffold `tools/tre-compare/web/` (Vite + React 19 + TS + Tailwind 4 + shadcn) as a
sibling of `src/`; build to a **non-`dist/` output dir** (e.g. `web/build/` — `dist/` is already gitignored as
the Python build output, a name collision); mount that dir via a custom `SpaStaticFiles(html=True)` at `/`
**registered last** in `create_app`; build the visible-tree from the flat `/compare/files` array with a
**fixed-height TanStack Virtualizer** over a memoized flattened-visible-rows array; render the honesty
distinction with **distinct badge styling for `identical-by-metadata` vs `content-confirmed-identical`** and an
always-visible "path-CRC — not content" label on the CRC field.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Install picker list | API (`GET /installs`) | Browser (render) | Config-driven list lives server-side (tomllib); UI only renders/selects. |
| Set-delta computation | API (`POST /compare/set`) | — | Frozen engine owns the diff; UI never recomputes. |
| Full file-level diff | API (`POST /compare/files`) | — | One-shot full payload (D-08); UI never paginates server-side. |
| Tree-building from flat rows | **Browser** | — | D-03 explicitly assigns tree-build, filter, search, hide-identical to the client. |
| Filter / search / hide-identical | **Browser** | — | In-memory over the already-fetched flat array; no round-trips. |
| Virtualized rendering | **Browser** | — | TanStack Virtual over flattened-visible rows. |
| Content hash / verdict | API (`POST /file/detail`) | Browser (render + auto-trigger) | xxhash + memo are server-side (D-04); UI auto-calls on select and renders verdict. |
| Static bundle serving | **API (FastAPI StaticFiles)** | — | D-02 single-process, one-port serve. |
| Dev-time API proxy | Vite dev server | — | Dev-only; not in the shipped artifact. |

## Standard Stack

All versions verified against the npm registry on 2026-06-15.

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| `vite` | 8.0.16 | Build tool + dev server | The default React build tool; emits the static bundle D-02 mounts. `[VERIFIED: npm view vite version]` |
| `react` / `react-dom` | 19.2.7 | UI runtime | shadcn's default target; current stable. `[VERIFIED: npm registry]` |
| `typescript` | 6.0.3 | Types | Standard; shadcn path aliases assume TS. `[VERIFIED: npm registry]` |
| `@vitejs/plugin-react` | 6.0.2 | React Fast Refresh + JSX in Vite | Standard React-in-Vite plugin. `[VERIFIED: npm registry]` |
| `tailwindcss` | 4.3.1 | Styling | shadcn 4's styling layer; Tailwind 4 uses the Vite plugin, not PostCSS config. `[VERIFIED: npm registry]` |
| `@tailwindcss/vite` | (ships with tailwind 4) | Tailwind-4 Vite integration | Tailwind 4 dropped `tailwind.config.js`/PostCSS in favor of this plugin + `@import "tailwindcss"`. `[CITED: ui.shadcn.com/docs/installation/vite]` |
| `@tanstack/react-virtual` | 3.14.2 | Row virtualization | D-03 names it; the proven approach for 10k–100k rows. `[VERIFIED: npm registry]` |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| shadcn CLI | 4.11.0 | Component scaffolding (`npx shadcn@latest init/add`) | Init + add components (button, badge, table, dialog/sheet, input, scroll-area). Not a runtime dep — it copies source into your repo. `[VERIFIED: npm view shadcn version]` |
| `@tanstack/react-query` | 5.101.0 | Server-state caching for the 4 routes | **Recommended** — dedupes/caches the two heavy responses, gives declarative loading/error states (D-04). `[VERIFIED: npm registry]` |
| `clsx` | 2.1.1 | Conditional classNames | shadcn `cn()` helper dependency. `[VERIFIED: npm registry]` |
| `tailwind-merge` | 3.6.0 | Merge Tailwind classes | shadcn `cn()` helper dependency. `[VERIFIED: npm registry]` |
| `class-variance-authority` | 0.7.1 | Variant styling (badge variants) | shadcn component dependency; useful for status-badge variants. `[VERIFIED: npm registry]` |
| `lucide-react` | 1.18.0 | Icons (chevrons for tree expand, status icons) | shadcn default icon set. `[VERIFIED: npm registry]` |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| TanStack Query | Plain `fetch` + `useState`/`useEffect` | Plain fetch is fine for 4 routes and zero added deps; but you hand-roll loading/error/dedup state for `/compare/files` (expensive) and `/file/detail` (auto-fired on every select). Query makes the discretionary D-04 loading/error states declarative. **Recommend Query**; plain fetch is an acceptable fallback if the planner wants zero extra deps. |
| `@tanstack/react-virtual` for the tree | A dedicated tree-view lib (react-arborist etc.) | A tree lib bundles its own virtualizer + expand state, but D-03 explicitly locks TanStack Virtual over a flattened-visible model, and the flatten-yourself approach is more transparent and faster for this scale. **Stay with TanStack Virtual.** |
| shadcn Table for the tree | Hand-rolled `<div>` rows | The tree is NOT a table — virtualized absolute-positioned rows are simpler than fighting `<table>` semantics inside a scroll container. Use shadcn Table for the **set-delta strip** (small, fixed) and `<div>` rows for the **virtualized tree**. |
| shadcn `Dialog` for detail panel | shadcn `Sheet` (slide-over) | D-01 says the detail panel "slides over" from the right → **`Sheet`** (side="right") is the literal match. `[CITED: ui.shadcn.com — Sheet]` |

**Installation (web project — run from `tools/tre-compare/web/`):**
```bash
npm create vite@latest . -- --template react-ts
npm install
npm install tailwindcss @tailwindcss/vite
npm install -D @types/node
# wire @tailwindcss/vite into vite.config.ts and @/* aliases into tsconfig*.json (see Pattern 2)
npx shadcn@latest init
npx shadcn@latest add button badge table sheet input scroll-area separator tooltip
npm install @tanstack/react-virtual @tanstack/react-query   # query optional per discretion
```

**Version verification (already run 2026-06-15):** `npm view <pkg> version` confirmed every version above
against the live registry. Installed backend: **FastAPI 0.137.0 / Starlette 1.3.1** (`uv run python -c
"import fastapi, starlette"`), so the StaticFiles API below is current for the shipped backend.

## Architecture Patterns

### System Architecture Diagram

```
                          ┌─────────────────── ONE localhost process (127.0.0.1:8765) ──────────────────┐
  Browser                 │  FastAPI app (create_app)                                                    │
  ───────                 │                                                                              │
  GET /            ───────┼──▶ SpaStaticFiles(html=True) mounted at "/"  ── serves ──▶ web/build/index.html + assets
  GET /assets/*    ───────┼──▶ (same mount, hashed asset files)                                          │
                          │                                                                              │
  GET  /installs   ───────┼──▶ installs()  ──▶ load_installs(tomllib)  ──▶ [{name, cfg_path}]            │
  POST /compare/set ──────┼──▶ compare_set ──▶ diff_archive_set        ──▶ {rows:[set-delta]}            │
  POST /compare/files ────┼──▶ compare_files ─▶ diff_virtual_trees     ──▶ {rows:[FULL flat], summary}   │
  POST /file/detail ──────┼──▶ file_detail   ─▶ drill_in + memoized xxhash ─▶ {winner, shadowed, verdict}│
                          └──────────────────────────────────────────────────────────────────────────────┘

  ── In-browser pipeline (D-03, all client-side; NO server round-trips after /compare/files) ──
  flat rows[] (≤100k)  ──▶  buildFolderTree()  ──▶  nested folder nodes + rolled-up {added,removed,changed} counts
        │                                                   │
        │  user: filter status / search text /              │  user: expand/collapse folder
        │        hide-identical / archive cross-filter      ▼
        └──────────────────────────────▶  flattenVisible(tree, expandedSet, filters)  ──▶  visibleRows[]
                                                            │
                                                            ▼
                                          TanStack Virtualizer (fixed rowHeight)  ──▶  ~30 DOM rows rendered
                                                            │
                                          click row ──▶ setSelectedPath ──▶ auto POST /file/detail ──▶ Sheet panel
```

### Recommended Project Structure (Pattern 3 — preserves D-01 extractability)
```
tools/tre-compare/                  # the isolated uv package — DROP-IN extractable (D-01)
├── pyproject.toml                  # Python package (unchanged)
├── uv.lock
├── installs.toml(.example)
├── src/tre_compare/                # backend (FROZEN contract)
│   ├── api.py  diff.py  config.py  __main__.py  ...
│   └── web_static.py   # NEW (only backend addition): SpaStaticFiles class + mount helper
├── web/                            # NEW Vite project — sibling of src/, its OWN package.json
│   ├── package.json  vite.config.ts  tsconfig*.json  components.json
│   ├── index.html
│   ├── src/                        # React app (App.tsx, components/, lib/api.ts, lib/tree.ts)
│   └── build/                      # Vite output  (RENAMED from dist/ — see landmine below)
└── tests/
```

- The `web/` dir has **zero coupling to the engine MSBuild graph** — it is npm/Vite only, mirroring how
  `src/` is uv/Python only. Both share the directory but not the build graph. This preserves the D-01
  invariant: `cp -r tools/tre-compare /somewhere-else` still works (it carries both `pyproject.toml` and
  `web/package.json`).
- **`dist/` name collision (landmine, see Common Pitfalls):** `tools/tre-compare/.gitignore` already ignores
  `dist/` as the **Python build artifact**. Do NOT let Vite default its output to a `dist/` that collides;
  set Vite `build.outDir` to `build/` (or `web/dist/` — a *nested* path, not the top-level `dist/`). Mount
  whatever dir you pick. Recommend `web/build/`.

### Pattern 1: Flat diff array → virtualized folder tree (THE #1 risk — D-03)

The `/compare/files` response is a **flat array** of `{path, status, left, right, qualifier?}` rows where
`path` is a forward-slash canonical virtual path (engine `fix_up_file_name`: lowercase, `/`-separated). Build
the tree **once** per response, then re-flatten the *visible* set on every expand/collapse/filter change.

```typescript
// Source: synthesized from TanStack Virtual docs (github.com/tanstack/virtual) + D-03 flattened-visible model.
// lib/tree.ts

type Status = "added" | "removed" | "changed" | "identical-by-metadata" | "tombstoned";
type DiffRow = { path: string; status: Status; left: SideMeta | null; right: SideMeta | null; qualifier?: string[] };
type SideMeta = { len: number; clen: number };

type FolderNode = {
  kind: "folder";
  name: string;            // segment name
  path: string;            // full folder path (stable key)
  children: Map<string, TreeNode>;
  rollup: { added: number; removed: number; changed: number; identical: number; total: number };
};
type FileNode = { kind: "file"; name: string; row: DiffRow };
type TreeNode = FolderNode | FileNode;

// Build the nested tree ONCE from the flat array. O(N * avgDepth).
export function buildFolderTree(rows: DiffRow[]): FolderNode {
  const root: FolderNode = { kind: "folder", name: "", path: "", children: new Map(),
    rollup: { added: 0, removed: 0, changed: 0, identical: 0, total: 0 } };
  for (const row of rows) {
    const segs = row.path.split("/");
    let node = root;
    const fileName = segs.pop()!;
    let acc = "";
    for (const seg of segs) {
      acc = acc ? `${acc}/${seg}` : seg;
      let child = node.children.get(seg) as FolderNode | undefined;
      if (!child) {
        child = { kind: "folder", name: seg, path: acc, children: new Map(),
          rollup: { added: 0, removed: 0, changed: 0, identical: 0, total: 0 } };
        node.children.set(seg, child);
      }
      node = child;
    }
    node.children.set(fileName, { kind: "file", name: fileName, row });
  }
  computeRollup(root);   // single post-order pass to sum descendant counts into every folder
  return root;
}

// Re-flatten ONLY the visible rows whenever expandedSet or filters change. O(visible).
export function flattenVisible(
  root: FolderNode, expanded: Set<string>, accept: (row: DiffRow) => boolean,
): VisibleRow[] {
  const out: VisibleRow[] = [];
  const walk = (node: FolderNode, depth: number) => {
    const folders = [...node.children.values()].filter(c => c.kind === "folder") as FolderNode[];
    const files   = [...node.children.values()].filter(c => c.kind === "file")   as FileNode[];
    for (const f of folders) {
      // optional: skip folders whose rollup is entirely filtered out (hide-identical → hide empty folders)
      out.push({ kind: "folder", depth, path: f.path, name: f.name, rollup: f.rollup,
                 expanded: expanded.has(f.path) });
      if (expanded.has(f.path)) walk(f, depth + 1);
    }
    for (const file of files) if (accept(file.row))
      out.push({ kind: "file", depth, path: file.row.path, name: file.name, row: file.row });
  };
  walk(root, 0);
  return out;
}
```

```typescript
// Source: github.com/tanstack/virtual docs — useVirtualizer + production checklist.
// components/FileTree.tsx  (fixed-height rows = simplest + fastest at this scale)
const parentRef = useRef<HTMLDivElement>(null);
const visible = useMemo(() => flattenVisible(tree, expanded, acceptFn), [tree, expanded, acceptFn]);
const virtualizer = useVirtualizer({
  count: visible.length,
  getScrollElement: () => parentRef.current,
  estimateSize: () => 28,                 // FIXED row height (px) — no measureElement, no ResizeObserver
  overscan: 12,
  getItemKey: (i) => visible[i].path,     // stable key — survives expand/collapse re-flatten
});
// scroll container: <div ref={parentRef} style={{height: "100%", overflow: "auto"}}>
//   <div style={{height: virtualizer.getTotalSize(), position: "relative"}}>
//     {virtualizer.getVirtualItems().map(vi => <Row .../>)} with translateY(vi.start)
```

**What:** Build the nested tree once; keep `expanded: Set<string>`; recompute `visible` (a flat array of only
the rows the user can currently see) with `useMemo`; feed `visible.length` to TanStack Virtual.
**When to use:** Always for D-03. This is the locked approach.
**Why it scales:** `buildFolderTree` runs once per `/compare/files` (O(N)); `flattenVisible` runs per
interaction but only walks **expanded** branches (O(visible), typically hundreds even with 100k total); the
virtualizer renders ~30 DOM nodes regardless of total. The 100k array stays in memory but is never fully
rendered.

### Pattern 2: FastAPI StaticFiles SPA mount + Vite dev proxy (D-02)

**VERIFIED landmine:** Starlette `StaticFiles(html=True)` serves `index.html` for **directory** requests and
serves a `404.html` if present, but it returns a plain **404 for unknown paths** — it does NOT fall back to
`index.html` for arbitrary routes. `[CITED: starlette.io/staticfiles — "Static files will respond with 404 …
for requests which do not match"; html mode "Automatically loads index.html for directories"]`

This SPA is a **single page with no client-side router routes**, so a bare `StaticFiles(directory=..., html=True)`
mounted at `/` is technically sufficient (the only navigable URL is `/`). But mount it **after** all API routes,
and prefer the small custom subclass below so a future deep-link / accidental path still serves the app instead
of a raw 404:

```python
# Source: starlette.io/staticfiles + common SPA-fallback pattern (crccheck.com/blog/serving-spas-from-starlette).
# tre_compare/web_static.py  — the ONLY backend addition for Phase 30.
from pathlib import Path
from starlette.exceptions import HTTPException
from starlette.responses import Response
from starlette.staticfiles import StaticFiles

class SpaStaticFiles(StaticFiles):
    """StaticFiles that falls back to index.html on 404 so the SPA renders for any non-asset path."""
    async def get_response(self, path: str, scope) -> Response:
        try:
            return await super().get_response(path, scope)
        except HTTPException as exc:
            if exc.status_code == 404:
                return await super().get_response("index.html", scope)
            raise
```

```python
# In create_app(...), AFTER all four API routes are registered (mount order matters — see pitfall):
from .web_static import SpaStaticFiles
WEB_DIR = Path(__file__).resolve().parent.parent.parent / "web" / "build"
if WEB_DIR.is_dir():                      # only mount when a build exists (dev runs API-only)
    app.mount("/", SpaStaticFiles(directory=WEB_DIR, html=True), name="spa")
```

**Vite dev proxy (dev-only — `web/vite.config.ts`):**
```typescript
// Source: vitejs.dev server.proxy. Dev server proxies API calls to the FastAPI port (no CORS needed).
import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import tailwindcss from "@tailwindcss/vite";
import path from "node:path";

export default defineConfig({
  plugins: [react(), tailwindcss()],
  resolve: { alias: { "@": path.resolve(__dirname, "./src") } },  // shadcn @/* alias
  build: { outDir: "build", emptyOutDir: true },                  // NOT "dist" (collision — see pitfall)
  server: {
    proxy: {
      "/compare": "http://127.0.0.1:8765",
      "/file":    "http://127.0.0.1:8765",
      "/installs":"http://127.0.0.1:8765",
    },
  },
});
```

With this, the frontend uses **same-origin relative paths** (`fetch("/compare/files", …)`) in both dev (proxied)
and prod (same FastAPI origin) — **no CORS, no base-URL config**. Dev = run `python -m tre_compare` (API on 8765)
+ `npm run dev` (Vite on 5173 proxying to 8765). Prod = `npm run build` then `python -m tre_compare` serves
everything on 8765.

**Build committed vs built-on-demand (planner decides — tradeoff):**
- *Committed `web/build/`:* `python -m tre_compare` works on a fresh clone with no Node toolchain. Cost: a
  large generated bundle in git history; must rebuild+commit on every UI change. Conflicts with the existing
  `.gitignore dist/` pattern philosophy (build artifacts are ignored).
- *Built on demand (recommend):* `.gitignore web/build/`; document `npm run build` as a setup step (Node 24
  is present on this machine — verified). Cleaner git, but `python -m tre_compare` fails with a friendly
  message if `web/build/` is absent (the `if WEB_DIR.is_dir()` guard above degrades to API-only — add a log
  line "no built SPA found, run `npm run build` in web/"). **Recommend built-on-demand** to match the repo's
  existing ignore-artifacts convention and keep git lean; the planner may override per the D-02 note.

### Pattern 3: Route → UI State map (the EXACT frozen field names)

Read from `api.py` + `diff.py`. The UI must consume these names verbatim.

**`GET /installs` →** `[{ "name": str, "cfg_path": str }]` — feed both left/right pickers. Empty list = empty
picker (not an error). The picker sends the chosen `cfg_path` strings as `left_cfg`/`right_cfg` in every POST.

**`POST /compare/set` {left_cfg, right_cfg} →** `{ "rows": [ … ] }`. Each row (from `diff_archive_set`):
| Field | Type | Present when | Notes |
|-------|------|--------------|-------|
| `basename` | str | always | archive filename (the match key, with `kind`) |
| `kind` | `"tree" \| "toc"` | always | tree↔toc basename collision = two rows |
| `status` | `"added" \| "removed" \| "changed" \| "identical" \| "fault"` | always | **set-level statuses differ from file-level** |
| `size_delta` | int | status changed/identical | `right.size - left.size` |
| `entry_count_delta` | int | status changed/identical | |
| `version_left` / `version_right` | str \| null | per side present | TREE version tag / master-index kind |
| `fault_left` / `fault_right` | str | status fault | corrupt-archive message — **render as a fault row, never drop** |

**`POST /compare/files` {left_cfg, right_cfg} →** `{ "rows": [...], "summary": {...} }` (FULL, no pagination).
Each row (from `diff_virtual_trees`):
| Field | Type | Notes |
|-------|------|-------|
| `path` | str | canonical virtual path (lowercased, `/`-sep) — **the tree key** |
| `status` | `"added" \| "removed" \| "changed" \| "identical-by-metadata" \| "tombstoned"` | **tri-state +; `identical-by-metadata` is NOT hash-verified** |
| `left` | `{len:int, clen:int}` \| null | null = absent on left |
| `right` | `{len:int, clen:int}` \| null | null = absent on right |
| `qualifier` | string[] (optional) | any of `tombstoned_left/right`, `rejected_left/right`, `error_left/right` — surface as small flags |

`summary`:
```json
{ "left":  {"node_errors": int, "rejected": int, "tombstoned": int},
  "right": {"node_errors": int, "rejected": int, "tombstoned": int},
  "status_counts": { "<status>": int, ... } }    // drives the summary-stats panel (SC#2)
```
`status_counts` keys are exactly the file-level status strings above. The summary panel renders these counts
plus the per-side `node_errors`/`rejected`/`tombstoned` (so the user knows the diff saw malformed/skipped data).

**`POST /file/detail` {left_cfg, right_cfg, path} →** (200) or **404** `{detail:{error:"not found",path}}` /
**400** `{detail:{error:"rejected path",path}}`. 200 body:
| Field | Type | Notes |
|-------|------|-------|
| `path` | str | canonical key echoed back |
| `status` | `"ok"` | |
| `winning_archive_left` / `winning_archive_right` | str \| null | the **winning archive** basename per side (SC#3) |
| `left` / `right` | `{len:int, clen:int}` \| null | winner sizes per side |
| `shadowed_left` / `shadowed_right` | `[{archive:str, kind:str, priority:int}]` | the **shadowed copies** (SC#3) |
| `hash_left` / `hash_right` | `{hexdigest:str\|null, error:str\|null, source_archive:str}` \| null | xxh3_64; `error` non-null = degraded read → indeterminate |
| `verdict` | `"content-confirmed-identical" \| "content-changed" \| "content-indeterminate"` \| null | null when one side absent (added/removed) |

**Field-gap flag (planning, NOT a backend edit):** SC#3 / D-04 say the detail panel shows "**CRC display**" /
"the **path-CRC** explicitly labeled". **The four routes do NOT expose any `crc` field today.** `diff.py`
deliberately never emits the TOC `crc` (D-06: it's a path-CRC, never a change signal), `_node_brief` exposes
only `{archive, kind, priority}`, and `_merged_meta` exposes only `{len, clen}`. So **the UI cannot render a
path-CRC value without a backend change**, which the FROZEN contract forbids. This is a genuine planning
decision — three honest options for the planner:
  1. **Render no CRC value** and instead show an explanatory note ("path-CRC is intentionally not a change
     signal — see content verdict below"). Satisfies the *honesty* intent of D-04 fully; satisfies SC#3's
     "CRC display" only as a labeled *absence*. **Lowest-risk, contract-preserving — recommend.**
  2. **Treat the path itself as the path-CRC subject** — label the displayed virtual `path` as "engine path
     (basis of the path-CRC), not a content signal." No new field needed; honest.
  3. **Flag for a contract amendment** — if Kenny wants the literal numeric path-CRC shown, that requires
     adding a field to `_node_brief`/detail, which is OUT of this phase's frozen scope and must be raised
     explicitly, not silently edited. **Do not do this silently.**
  The planner must pick one and record it as a decision; do not let a task quietly add a backend field.
  `[VERIFIED: read of api.py serializers + diff.py — no crc field emitted anywhere]`

### Anti-Patterns to Avoid
- **Rebuilding the whole tree on every keystroke/expand.** Build `buildFolderTree` ONCE per `/compare/files`
  response (memoize on the response); only `flattenVisible` re-runs on interaction.
- **Rendering the full flat array in any list/table.** Even off-screen, mapping 100k rows to elements hangs.
  Only the virtualizer's `getVirtualItems()` (~30) become DOM.
- **Using `measureElement`/dynamic heights for the tree.** Rows are uniform; fixed `estimateSize` avoids
  ResizeObserver churn and layout thrash at 100k scale. Reserve dynamic measurement for the detail panel if
  ever needed (it isn't — the Sheet isn't virtualized).
- **Mounting StaticFiles at `/` BEFORE the API routes.** A `/` mount is greedy; if registered first it can
  shadow `/compare/*`. Register all four API routes first, mount the SPA last.
- **Adding a backend field to show the CRC.** Frozen contract — see the field-gap flag above.
- **Hardcoding `http://127.0.0.1:8765` in fetch calls.** Use relative paths so dev-proxy and prod-same-origin
  both work with one code path.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Row virtualization | A custom windowing/scroll calculator | `@tanstack/react-virtual` (D-03) | Edge cases: variable scroll, overscan, scroll restoration, total-size math. Locked choice. |
| Server-state / request lifecycle | `useEffect` + `useState` fetch soup for 4 routes | TanStack Query (recommended) or one tiny typed `fetch` wrapper | Dedup, caching, loading/error states (D-04) for free; avoids stale-response races on rapid file selection. |
| Component primitives (badge, sheet, table, tooltip) | Bespoke CSS components | shadcn `add` (copies source into repo) | Accessible, themeable, consistent; you own the source so still extractable. |
| className merging | String concatenation with conditionals | `cn()` = `clsx` + `tailwind-merge` (shadcn ships it) | Correct Tailwind conflict resolution for variant badges. |
| Static-serve + SPA fallback | A custom file-reading route | `StaticFiles` / `SpaStaticFiles` subclass | Starlette handles range requests, etags, mime types; you only add the 404→index.html fallback. |

**Key insight:** Everything risky in this phase is the *data plumbing* (flat→tree→flatten and the field
mapping), not the widgets. Lean on shadcn + TanStack for the widgets/virtualization and spend the effort on
the tree-build/flatten correctness and the honesty rendering.

## The Honesty Distinction in UI Terms (D-06 / D-04 — Kenny's #1 pitfall)

There are **two different "identical" concepts** and they must look different on screen:

| Concept | Source | Meaning | Render |
|---------|--------|---------|--------|
| `identical-by-metadata` | `/compare/files` row `status` | `(len, clen)` matched — **content NOT verified** (could be a false-identical) | Muted/neutral badge (e.g. grey "≈ metadata") **with a subtle "not content-verified" affordance** (tooltip or dim icon). Never a confident green check. |
| `content-confirmed-identical` | `/file/detail` `verdict` (after xxhash) | decompressed bytes hashed equal both sides | **Strong/green** badge ("= content"); only here is a confident "identical" earned. |
| `content-changed` | `/file/detail` `verdict` | hashes differ | "≠ content" badge. |
| `content-indeterminate` | `/file/detail` `verdict` | a side's read degraded (`hash.error` non-null) | Warning/amber badge ("? content — read error"); show the `error` string. |
| path-CRC | (NOT in contract — see field-gap flag) | TOC path checksum — **never** a content signal | If shown at all, label explicitly **"path-CRC — not content"** and visually separate from the verdict. Per the field-gap flag, the value isn't exposed; render the *label/explanation*, not a misleading number. |

**Implementation rules:**
- A file-tree row showing `identical-by-metadata` must NOT use the same styling as a drill-in
  `content-confirmed-identical`. Reserve the confident "identical" visual exclusively for the post-hash verdict.
- When a user selects an `identical-by-metadata` file and the auto `/file/detail` returns
  `content-confirmed-identical`, it is legitimate to upgrade the badge in the panel — but the tree row's status
  came from cheap metadata and should keep its honest "metadata-only" framing unless you also persist the
  per-file verdict client-side after drill-in.
- Put the "path-CRC — not content" label adjacent to wherever CRC/checksum language appears so it can never be
  mistaken for the verdict.

## Common Pitfalls

### Pitfall 1: `dist/` name collision with the Python build artifact
**What goes wrong:** Vite defaults `build.outDir` to `dist/`; `tools/tre-compare/.gitignore` already ignores
`dist/` as the **uv/Python** build output. A Vite `dist/` either gets silently gitignored (so a committed
bundle never lands) or collides conceptually with the Python dist.
**Why it happens:** Two different toolchains share the directory.
**How to avoid:** Set Vite `build.outDir: "build"` (mount `web/build/`), or use a nested `web/dist/` and adjust
ignore rules deliberately. Recommend `web/build/`.
**Warning signs:** `git status` shows nothing after `npm run build`; or a `dist/` with mixed Python+JS content.

### Pitfall 2: Greedy `/` mount shadows the API
**What goes wrong:** SPA served fine but `/compare/files` returns the SPA HTML or 404.
**Why it happens:** `app.mount("/", …)` registered before the `@app.post` routes.
**How to avoid:** Register all four routes in `create_app` first, mount SPA last (the example does this).
**Warning signs:** API calls return `text/html` instead of JSON.

### Pitfall 3: Treating set-level and file-level statuses as the same enum
**What goes wrong:** Badge logic breaks because `/compare/set` uses `identical` while `/compare/files` uses
`identical-by-metadata`, and set adds `fault`/`removed`/`added`/`changed` while files add `tombstoned`.
**Why it happens:** Similar names, different sources (`diff_archive_set` vs `diff_virtual_trees`).
**How to avoid:** Two separate TypeScript union types (`SetStatus`, `FileStatus`); separate badge maps.
**Warning signs:** A file row rendered with set-level styling or vice versa.

### Pitfall 4: Stale `/file/detail` response on rapid selection
**What goes wrong:** User clicks file A then B quickly; A's slower response lands last and overwrites B's panel.
**Why it happens:** Hand-rolled fetch with no request keying.
**How to avoid:** TanStack Query keyed on `[left_cfg, right_cfg, path]` (it discards stale), or an abort/seq
guard in a plain-fetch wrapper. This is the strongest argument for Query here.
**Warning signs:** Detail panel shows data for the previously-selected file.

### Pitfall 5: Rebuilding the tree on every render
**What goes wrong:** UI hangs on filter/search/expand at 100k rows.
**Why it happens:** `buildFolderTree` called inside render instead of memoized on the response.
**How to avoid:** `useMemo(() => buildFolderTree(rows), [rows])`; only `flattenVisible` recomputes on
interaction.
**Warning signs:** Typing in the search box lags; expanding a folder freezes.

### Pitfall 6: Filter (hide-identical) leaving empty folders
**What goes wrong:** "Hide identical" leaves folders that now contain no visible files.
**Why it happens:** `flattenVisible` emits the folder before discovering all its children were filtered out.
**How to avoid:** Use the folder `rollup` counts to decide whether a folder has any rows passing the current
filter before emitting it (or do a child-presence check). The rollup makes this O(1) per folder.
**Warning signs:** Empty expandable folders with zero rows under them.

## Code Examples

### A tiny typed fetch wrapper (if plain-fetch chosen over Query)
```typescript
// lib/api.ts — relative paths => dev-proxy + prod-same-origin both work, no CORS, no base URL.
export async function postJson<T>(url: string, body: unknown): Promise<T> {
  const res = await fetch(url, {
    method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(body),
  });
  if (!res.ok) {
    const detail = await res.json().catch(() => ({}));
    throw new ApiError(res.status, detail);   // 400 rejected / 404 not_found from /file/detail
  }
  return res.json() as Promise<T>;
}
export const fetchInstalls = () => fetch("/installs").then(r => r.json());
export const compareSet   = (b: PairReq) => postJson<{rows: SetRow[]}>("/compare/set", b);
export const compareFiles = (b: PairReq) => postJson<FilesResponse>("/compare/files", b);
export const fileDetail   = (b: PairReq & {path: string}) => postJson<DetailResponse>("/file/detail", b);
```

### shadcn Sheet as the slide-over detail panel (D-01)
```tsx
// components/DetailPanel.tsx
<Sheet open={!!selectedPath} onOpenChange={(o) => !o && setSelectedPath(null)}>
  <SheetContent side="right" className="w-[440px]">
    {/* winning_archive_left/right, left/right {len,clen}, shadowed_*[], verdict badge,
        "path-CRC — not content" label, hash_*.error if indeterminate */}
  </SheetContent>
</Sheet>
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Tailwind `tailwind.config.js` + PostCSS | `@tailwindcss/vite` plugin + `@import "tailwindcss"` | Tailwind v4 | No PostCSS config; shadcn-vite docs use the plugin. |
| React 18 default in shadcn | React 19 default | shadcn 4 / React 19 stable | Use React 19; shadcn components target it. |
| Create-React-App | Vite | long settled | CRA is dead; Vite is the standard. |
| `react-window` / `react-virtualized` | `@tanstack/react-virtual` | TanStack Virtual 3.x | Headless, hook-based, framework-agnostic core; D-03's locked choice. |

**Deprecated/outdated:** CRA (dead); Tailwind PostCSS config in v4 (replaced by the Vite plugin); `html=True`
as a presumed SPA-fallback (it is NOT — verified above).

## Runtime State Inventory

Not applicable — Phase 30 is a greenfield frontend addition, not a rename/refactor/migration. No stored data,
live-service config, OS-registered state, secrets, or build artifacts carry a renamed string. (The only
new persistent artifact is the optional `web/build/` bundle, addressed under Pattern 2.)

## Validation Architecture

`workflow.nyquist_validation` is `true` in config — section included.

### Test Framework
| Property | Value |
|----------|-------|
| Backend framework | pytest (existing) — `uv run pytest` from `tools/tre-compare/`; `httpx` TestClient for routes |
| Frontend framework | **Vitest + React Testing Library** (Vite-native; recommended) — none exists yet → Wave 0 |
| Frontend config file | `web/vite.config.ts` (Vitest reads the Vite config) — Wave 0 |
| Quick run command (backend) | `uv run pytest -m "not integration" -x` |
| Quick run command (frontend) | `cd web && npm run test -- --run` (Vitest) |
| Full suite | backend `uv run pytest` (+ env-gated integration) AND `cd web && npm run test -- --run` + `npm run build` |

The strongest, most decoupled validation seam is the **pure functions** `buildFolderTree` / `flattenVisible` /
`computeRollup` and the **status→badge / verdict→badge mapping** — unit-test these in Vitest with synthetic
flat-row arrays (no DOM, no server). The contract field-mapping types can be validated by a small fixture that
mirrors the documented JSON shapes. A build-succeeds smoke (`npm run build` exits 0) plus a backend route smoke
(TestClient already exists in `tests/test_api.py`) covers the integration seam without a real browser.

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| TRE-05 | `buildFolderTree` builds correct nesting + rollup counts from a flat array | unit | `cd web && npm run test -- --run src/lib/tree.test.ts` | ❌ Wave 0 |
| TRE-05 | `flattenVisible` honors expand state + hide-identical filter + empty-folder suppression | unit | `cd web && npm run test -- --run src/lib/tree.test.ts` | ❌ Wave 0 |
| TRE-05 | file-status → badge map distinguishes `identical-by-metadata` from `content-confirmed-identical` | unit | `cd web && npm run test -- --run src/lib/status.test.ts` | ❌ Wave 0 |
| TRE-05 | SPA bundle builds clean | smoke | `cd web && npm run build` | ❌ Wave 0 |
| TRE-05 | FastAPI mounts the built SPA + still serves the 4 routes (mount-order) | integration | `uv run pytest tests/test_web_static.py -x` | ❌ Wave 0 |
| TRE-05 | SWGSource-vs-whitengold real diff renders end-to-end (SC#4) | manual | manual run: `npm run build` → `python -m tre_compare` → browser | n/a (manual) |

### Sampling Rate
- **Per task commit:** `cd web && npm run test -- --run` (fast Vitest unit) + `npm run build` when touching build config.
- **Per wave merge:** full frontend Vitest + `npm run build` + backend `uv run pytest -m "not integration"`.
- **Phase gate:** full suite green + the manual SWGSource-vs-whitengold end-to-end run before `/gsd-verify-work`.

### Wave 0 Gaps
- [ ] `web/` Vite project scaffold (package.json, vite.config.ts, tsconfig*, components.json) + `npm install`
- [ ] Vitest + `@testing-library/react` install (`npm install -D vitest @testing-library/react jsdom`)
- [ ] `web/src/lib/tree.test.ts` — covers TRE-05 tree-build/flatten
- [ ] `web/src/lib/status.test.ts` — covers TRE-05 honesty-distinction badge mapping
- [ ] `tools/tre-compare/tests/test_web_static.py` — covers SPA mount + mount-order (the only new backend test)
- [ ] `tre_compare/web_static.py` — the SpaStaticFiles helper (only new backend source)

## Security Domain

`security_enforcement: true`, `security_asvs_level: 1`. This is a **localhost-only, single-user, read-only,
no-auth diagnostic tool** (`127.0.0.1` bind is the single security control, T-29-03-03, already in place). Most
ASVS categories are N/A by posture; the relevant ones are input handling and the new static-serve surface.

### Applicable ASVS Categories
| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | Single-user localhost; no auth surface (by design, D-01/T-29). |
| V3 Session Management | no | No sessions/cookies. |
| V4 Access Control | partial | The 127.0.0.1 bind is the access control; the backend opens request-controlled cfg paths (validated to 400 in `_validate_cfg`). The frontend adds **no new** filesystem surface. |
| V5 Input Validation | yes | Frontend: treat all route responses as untrusted-shaped; render `fault`/`error`/`qualifier` defensively (backend never 500s but can return faults). React escapes text by default — **never** `dangerouslySetInnerHTML` for `path`/`error`/`fault` strings (they come from filenames/archive data). |
| V6 Cryptography | no | xxhash is a non-cryptographic content checksum used only for equality; correct usage, not a crypto control. Don't relabel it "secure." |
| V12/V14 (config/build) | partial | Don't introduce a second open port; keep the single 127.0.0.1 bind. Don't widen StaticFiles to serve outside `web/build/`. |

### Known Threat Patterns for this stack
| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| XSS via rendering attacker-controlled path/filename/error strings | Tampering / Elevation | React default text-escaping; no `dangerouslySetInnerHTML`; no `eval`. |
| Path traversal via the static mount | Information disclosure | `StaticFiles(directory=web/build)` confines to that dir; the SPA fallback only ever returns `index.html` from that dir — do not join request paths to the filesystem yourself. |
| Accidentally binding all interfaces / second port | Exposure | Keep the single `127.0.0.1` bind; no new listener for the static serve (same app, same port). |
| Supply-chain (npm deps) | Tampering | Pin via `package-lock.json`; the deps recommended are mainstream (Vite/React/TanStack/shadcn). |

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | The flat `/compare/files` payload for the SWGSource-vs-whitengold pair is in the few-MB / ≤100k-row range (per D-08's stated estimate), so the full-payload + client-virtualization model holds without server pagination. | Pattern 1 | If a real pair yields far more, the in-browser tree-build could be slow on first render — but D-08 explicitly chose and accepted this model; revisit only if measured slow. |
| A2 | The SPA needs no client-side router (single page, master-detail in one view per D-01), so `StaticFiles(html=True)` + the SpaStaticFiles fallback fully covers routing. | Pattern 2 | If a router with real URL routes is later added, the fallback subclass already handles it — low risk. |
| A3 | Built-on-demand (gitignored `web/build/`) is preferable to committing the bundle, matching the repo's ignore-artifacts convention. | Pattern 2 | Planner may prefer committing for zero-Node fresh-clone runs; explicitly flagged as a planner decision, not silent. |
| A4 | Vitest is the right frontend test runner (Vite-native). | Validation Architecture | Low — Vitest is the standard for Vite projects; Jest would also work but adds config friction. |

## Open Questions (RESOLVED)

> All three resolved during the UI-SPEC → PATTERNS → plan pass; resolutions locked in the Phase 30 plans.
> 1. **RESOLVED → Option 1** (labeled-absence note, no numeric path-CRC; no silent backend field add) — 30-02/30-03.
> 2. **RESOLVED → build-on-demand (A3)**, `web/build/` gitignored — 30-01.
> 3. **RESOLVED → TanStack Query** (keyed-detail dedup, Pitfall 4) — 30-03.

1. **CRC display vs frozen contract (the field-gap).**
   - What we know: D-04/SC#3 ask for a "path-CRC — not content" display; the four routes expose **no `crc`
     field** (verified in `api.py`/`diff.py`).
   - What's unclear: whether Kenny wants the literal numeric path-CRC shown (needs a contract amendment) or is
     satisfied by an honest *labeled-absence / explanation*.
   - Recommendation: ship Option 1 (explanatory "path-CRC is intentionally not a change signal" note, no
     numeric value) to honor both D-04's honesty intent and the frozen-contract rule; raise to Kenny in
     planning/discuss if the literal value is wanted (that becomes a separate, explicitly-scoped backend task).

2. **Commit vs build-on-demand for `web/build/`.** Recommendation: build-on-demand (A3). Planner to lock per D-02.

3. **TanStack Query vs plain fetch.** Recommendation: Query (Pitfall 4 — stale-response races on rapid file
   selection are the deciding factor). Planner's discretion per CONTEXT.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| Node.js | Vite build / dev | ✓ | v24.15.0 | — |
| npm | package install / scripts | ✓ | 11.12.1 | — |
| Python 3.11+ | backend (existing) | ✓ | (uv-managed) | — |
| uv | backend venv/run (existing) | ✓ | (existing) | — |
| FastAPI / Starlette | static mount API | ✓ | 0.137.0 / 1.3.1 | — |

**Missing dependencies with no fallback:** None.
**Missing dependencies with fallback:** None — full toolchain present on this machine.

## Sources

### Primary (HIGH confidence)
- `tools/tre-compare/src/tre_compare/api.py`, `diff.py`, `config.py`, `__main__.py` — the FROZEN contract, read directly (exact field names, statuses, route shapes, serializers).
- `tools/tre-compare/installs.toml.example`, `pyproject.toml`, `.gitignore` — picker shape, deps, `dist/` ignore collision.
- Context7 `/tanstack/virtual` — `measureElement`, dynamic-vs-fixed sizing, production checklist.
- Context7 `/fastapi/fastapi` — StaticFiles mount API.
- starlette.io/staticfiles — `html=True` behavior (verified: 404 for unknown paths, index.html for directories only).
- npm registry (`npm view <pkg> version`, 2026-06-15) — all stack versions.
- `uv run python -c "import fastapi, starlette"` — installed backend versions.

### Secondary (MEDIUM confidence)
- ui.shadcn.com/docs/installation/vite — shadcn + Vite + Tailwind 4 init flow.
- crccheck.com/blog/serving-spas-from-starlette + davidmuraya.com — SpaStaticFiles `get_response` fallback pattern (cross-checked against Starlette source behavior).

### Tertiary (LOW confidence)
- None relied upon for decisions.

## Metadata

**Confidence breakdown:**
- Frozen contract / field mapping: HIGH — read directly from source.
- Standard stack + versions: HIGH — npm-registry verified 2026-06-15.
- FastAPI static-serve / SPA fallback: HIGH — Starlette behavior verified (the `html=True` non-fallback is the key correction).
- Virtualized tree pattern: HIGH — TanStack docs + locked D-03 model; the flat→tree→flatten pattern is well-established.
- CRC field-gap: HIGH that the field is absent; the resolution is an Open Question for Kenny.

**Research date:** 2026-06-15
**Valid until:** ~2026-07-15 (stable stack; re-verify versions if planning slips a month — Vite/React/Tailwind move fast).
