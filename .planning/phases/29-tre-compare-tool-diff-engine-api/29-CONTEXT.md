# Phase 29: TRE Compare Tool — Diff Engine + API - Context

**Gathered:** 2026-06-14
**Status:** Ready for planning

<domain>
## Phase Boundary

Build the **set-level + file-level diff engine** and the **FastAPI surface** over the
proven Phase-28 virtual tree, plus a **sqlite index cache**, so Phase 30's web UI works
against real data from day one. Delivers TRE-02 (set-level compare), TRE-03 (file-level
compare of merged virtual trees), TRE-04 (drill-in: metadata, winning archive, shadowed
copies, on-demand content hash).

**Builds on (Phase 28, proven + unit-tested):** `tre_compare.parser` (vendored version-aware
TRE/TOC/COT2000 reader), `tre_compare.scanner` (`parse_shared_file` → `ScanResult`/`SearchNode`),
`tre_compare.virtual_tree` (`build_virtual_tree` → `VirtualTree`/`MergedEntry` with first-hit-wins
precedence, per-node-type tombstones, `fix_up_file_name`/`safe_virtual_key`). The API wraps this
core; no refactor of Phase-28 code should be needed.

**Not in this phase:** no React/Vite/shadcn SPA (Phase 30, TRE-05); no TRE management /
extract / repack / IFF-aware diff (TREM-01..03, deferred). v2.3 is **read-only compare**.
</domain>

<decisions>
## Implementation Decisions

### API shape & request model
- **D-01:** **Stateless `POST`-a-pair core contract.** Each compare sends the two install
  `client.cfg` paths in the request body and returns the diff; there is **no stateful
  server-side install registry / lifecycle**. The sqlite cache (keyed by `(abspath, mtime,
  size)`) is the only persistence. Best fit for a single-user localhost diagnosis tool.
  *(Kenny delegated — "you decide"; locked.)*
- **D-02:** Provide a lightweight **`GET /installs`** that returns a **config-driven** list
  of known install cfg paths (read from a small tool-local config, NOT a stateful registry)
  to populate Phase 30's install picker. Keeps the picker fed without lifecycle state.
- **D-03:** Endpoint surface (planner may refine exact paths/verbs, keep the shape):
  - `POST /compare/set`   `{ left_cfg, right_cfg }` → set-delta (added/removed/changed archives)
  - `POST /compare/files` `{ left_cfg, right_cfg }` → full file-level diff (see D-08/D-09)
  - `POST /file/detail`   `{ left_cfg, right_cfg, path }` → drill-in (see D-07) incl. on-demand hash
  - `GET  /installs` → config-driven picker list

### Set-level diff semantics (TRE-02)
- **D-04:** **Match archives by basename** (filename only, ignoring directory) — abspaths
  differ across install roots, basename is the stable identity. only-in-A = **removed**,
  only-in-B = **added**, present-in-both = **changed candidate**. *(Kenny selected.)*
- **D-05:** An archive is **`changed`** if **any of**: on-disk file **size**, **entry-count**,
  or **TREE version tag** differs. All three deltas are surfaced in the row (so the user sees
  WHY it changed), even when only one drives the status. This is the cheap, no-content-hashing
  signal SC#1 calls for (header/stat reads only). *(Kenny selected.)*

### File-level diff semantics & scale (TRE-03)
- **D-06:** **Tri-state file status.** Per merged-tree entry: `added` / `removed` / `changed`
  / `identical-by-metadata`, where **`identical-by-metadata`** explicitly means `(length,
  compressedLength)` match but content is **NOT** hash-verified. `changed` = `(length, clen)`
  differ. This is honest about the cheap signal (two files can match on both yet differ in
  content — a false-identical). Drill-in xxhash (D-07) **upgrades** an `identical-by-metadata`
  entry to **content-confirmed-identical** or **content-changed**. *(Kenny delegated; locked.)*
  **NEVER** use the TOC `crc` field for change detection — it is a path-CRC, not a content hash.
- **D-07:** **Drill-in** returns: file metadata (`length`, `compressedLength`, winning
  `SearchNode`), the **winning archive**, the **shadowed copies** (from `MergedEntry.shadowed`,
  which already excludes toc-absent + tree-tombstones), and an **on-demand real content hash
  (xxhash)** computed **only on drill-in** for BOTH sides — never eager full-archive hashing
  (SC#3). The drill-in hash compare is what resolves false-identicals.
- **D-08:** **Full payload, client-virtualized delivery.** `/compare/files` returns the
  **entire flat diff array in one JSON response** plus a summary count block; Phase 30
  virtualizes rendering (TanStack Virtual) and does filter / search / hide-identical
  **in-browser**. 100k lean rows is a few MB — fine on localhost; one round-trip; matches
  Phase 30's stated plan. No server-side pagination. *(Kenny delegated; locked.)*
- **D-09:** Keep per-row JSON **lean** (e.g. `{ path, status, left:{len,clen}, right:{len,clen} }`)
  so the full-payload model stays small enough for 100k entries. Hashes are NOT in the bulk
  payload — only in the drill-in detail response (D-07).

### Cache & new dependencies
- **D-10:** **sqlite cache stores per-archive parsed TOC entries**, keyed by `(abspath, mtime,
  size)` (SC#4). Re-runs skip re-parsing every `.tre`/`.toc`; the virtual-tree merge + diff
  re-run cheaply from cached entries (granular, reusable across ANY install pairing — not tied
  to a specific pair). Also memoize **drill-in xxhash results** in a `file_hash` table keyed by
  `(abspath, mtime, size, vpath)`. NOT a coarse per-pair diff blob. *(Kenny delegated; locked.)*
- **D-11:** **xxhash via the `xxhash` PyPI package (C extension)**, added to `pyproject.toml`
  and pinned in `uv.lock`. SC#3 names xxhash explicitly; FastAPI already establishes a real
  dependency story in this phase; `uv.lock` makes the binary wheel reproducible on a fresh
  clone, preserving D-01 extractability. *(Kenny delegated; locked.)*
- **D-12:** New runtime deps for Phase 29: **FastAPI + an ASGI server (uvicorn)** + **xxhash**.
  **sqlite is stdlib** (no dep). These MUST be pinned in `uv.lock`; the directory stays
  copy-paste-extractable per the carried-forward D-01 (below). No engine imports.

### Carried forward from Phase 28 (locked — do NOT re-litigate)
- **Standalone / extractable (Phase-28 D-01/D-02):** `tools/tre-compare/` is a fully isolated
  Python package — own `pyproject.toml` + `uv.lock` + venv, **zero engine imports**, copy-paste
  movable to another repo, NOT coupled to the engine CMake/MSBuild graph. Phase 29's new deps
  must preserve this. (Hard constraint; Kenny 2026-06-12.)
- **Toolchain (Phase-28 D-05):** uv for venv/lock/run, pytest for tests.
- **"Changed" signal source:** `(length, compressedLength)` + on-demand xxhash, **never** TOC
  `crc`. The Phase-28 data model already exposes `length` + `compressed_length` per `MergedEntry`.

### Claude's Discretion
- Exact endpoint paths/verbs, Pydantic model field names, error-envelope shape, and
  localhost-only binding/security posture — planner/executor decide (keep D-01..D-12 intent).
- sqlite schema details, cache file location (within the tool dir, gitignored), and whether
  the merged-tree build is memoized in-process vs re-run-from-cache each request.
- Whether the on-demand content hash reads decompressed vs stored-compressed bytes — pick the
  one that makes false-identical detection correct (likely decompressed/full extracted bytes).
- Test strategy mirrors Phase 28: synthetic fixtures (default, machine-independent) + one
  env-gated integration test against the real SWGSource-vs-whitengold install pair (SC#4).

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Roadmap & requirements
- `.planning/ROADMAP.md` §"Phase 29" — goal + the 4 success criteria (the spec for this phase)
- `.planning/REQUIREMENTS.md` — TRE-02 / TRE-03 / TRE-04 (this phase); TRE-05 (Phase 30);
  TREM-01..03 + the "architecture must not preclude management" constraint
- `.planning/PROJECT.md` — v2.3 Hardening milestone (TRE tool = standalone web-app stream)

### Phase-28 foundation (the library this phase wraps — read before any API/diff work)
- `.planning/phases/28-tre-compare-tool-foundation-parser-scanner-virtual-tree/28-CONTEXT.md`
  — locked decisions D-01..D-08 (standalone/extractable, vendored parser, uv, test strategy)
- `tools/tre-compare/src/tre_compare/virtual_tree.py` — `build_virtual_tree`, `VirtualTree`,
  `MergedEntry` (`canonical_path`, `winner_node`, `length`, `compressed_length`, `shadowed`),
  `fix_up_file_name`, `safe_virtual_key`; the **APPROXIMATION BOUNDARY** docstring lists
  Phase-28 out-of-scope items (static searchPath snapshot, omitted searchAbsolute/searchCache)
  that **Phase 29 must revisit if real installs exercise them**
- `tools/tre-compare/src/tre_compare/scanner.py` — `parse_shared_file` → `ScanResult` /
  `SearchNode` (`kind`, `priority`, `abspath`, `raw_key`, `sku`, `cfg_seq`)
- `tools/tre-compare/src/tre_compare/parser/tre_reader.py` — entry fields incl. `length`,
  `compressed_length`, `offset` (the diff signal source); version tag detection
- `tools/tre-compare/README.md` + `tools/tre-compare/pyproject.toml` — current zero-dep
  baseline + uv/pytest/integration-marker conventions to extend

### Engine semantics to stay faithful to
- `src/engine/shared/library/sharedFile/src/shared/TreeFile.cpp` — search precedence,
  first-match-wins, `fixUpFileName`, tombstone handling (already ported in Phase 28; reference
  if drill-in/shadowed semantics need confirming)
- `stage/client.cfg` §`[SharedFile]` — a real install cfg shape for the integration test

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `build_virtual_tree(scan)` returns a `VirtualTree` whose `entries: dict[str, MergedEntry]`
  is exactly the per-install merged map the **file-level diff** consumes — diff = compare two
  `VirtualTree.entries` dicts by canonical key. `MergedEntry.shadowed` already gives the
  drill-in "shadowed copies" list. `length`/`compressed_length` are the change signal.
- `parse_shared_file(cfg)` → `ScanResult.nodes` gives the per-install archive list for the
  **set-level diff** (basename-match the `SearchNode.abspath` basenames; size via `os.path.getsize`,
  version via the parser's header read, entry-count via the parsed TOC).
- Phase-28 test harness (synthetic byte-built fixtures + env-gated integration marker) is the
  template for Phase-29 diff/API/cache tests.

### Established Patterns
- Per-archive parse is already isolated and bounds-checked (`iter_node_entries`,
  `_preflight_tre`/`_preflight_toc`, `node_errors`) — the cache layer wraps this without
  changing fault-isolation behavior.
- `VirtualTree.node_errors` / `rejected` / `tombstoned` are observable — the API should surface
  these (or counts) so the UI can flag malformed/skipped archives rather than silently dropping.

### Integration Points
- Phase 29 imports the Phase-28 library behind FastAPI + sqlite; **keep the public surface
  clean and importable** so Phase 30 needs no backend refactor — Phase 30 only adds the SPA
  (and, per Phase-30 SC#4, a single localhost server that may also serve the static frontend).

</code_context>

<specifics>
## Specific Ideas

- **North-star scenario (folded in Phase 28, drives validation here):** diff the **SWGSource
  (Restoration) vs whitengold** install pair to diagnose space-scene graphics artifacts — the
  API must return correct diff JSON for this real pair (SC#4). This is the acceptance scenario.
- The cheap diff must stay cheap: header/stat reads + cached parsed TOC entries only — **eager
  full-archive content hashing is explicitly forbidden**; xxhash runs only on drill-in.
- Tri-state status (D-06) exists specifically so Phase 30 badges don't lie about false-identicals;
  drill-in is the path that converts `identical-by-metadata` into a content-verified verdict.

</specifics>

<deferred>
## Deferred Ideas

- **React/Vite/shadcn virtualized tree-diff SPA** (install picker, set-delta table, badges,
  hide-identical filter, search, per-file detail panel) — **Phase 30 (TRE-05)**.
- **Server-side pagination/filtering** of the file diff — explicitly NOT built (D-08 chose
  full-payload + client virtualization); revisit only if real payloads prove too large.
- **Stateful install registry / lifecycle** — explicitly NOT built (D-01 chose stateless +
  config-driven picker); a future increment if multi-user/persistent installs are ever wanted.
- **TRE management (extract / repack / IFF-aware content diff)** — TREM-01..03, a later
  increment beyond v2.3's read-only compare. Architecture must not preclude these
  (REQUIREMENTS.md constraint) but they are NOT built now.
- **Phase-28 approximation-boundary items** (lazy searchPath re-scan, `searchAbsolute` /
  `searchCache` / preloaded-cache nodes) — revisit ONLY if the real SWGSource/whitengold cfgs
  exercise them; otherwise carry the Phase-28 static-snapshot behavior forward.

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 29-TRE Compare Tool — Diff Engine + API*
*Context gathered: 2026-06-14*
