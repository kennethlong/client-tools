# Phase 28: TRE Compare Tool — Foundation (Parser + Scanner + Virtual Tree) - Context

**Gathered:** 2026-06-14
**Status:** Ready for planning

<domain>
## Phase Boundary

Stand up the **headless, fully unit-tested Python backend foundation** for the TRE
compare tool at `tools/tre-compare/`. Three pieces only:

1. **Parser** — a vendored TRE/TOC reader that handles all TREE format variants
   (0004/0005/6000/0006/5000) plus retail SearchTOC (TAG_TOC/0001) and Restoration
   COT2000 master indexes, without a rewrite.
2. **Scanner** — a hand-written `client.cfg` `[SharedFile]` parser that yields a
   priority-ordered node list matching the engine's `searchTree` / `searchTOC` /
   `searchPath` precedence (repeated keys; NOT stdlib `configparser`).
3. **Virtual-tree builder** — merges archives with first-hit-wins precedence
   (higher priority number searched first), length-0 tombstone handling, and
   `fixUpFileName` canonicalization, proven by unit tests.

**Not in this phase:** no diff engine, no FastAPI surface (Phase 29), no web UI
(Phase 30). Library-first; the API wraps this proven core in 29.
</domain>

<decisions>
## Implementation Decisions

### Project structure & extractability
- **D-01:** `tools/tre-compare/` is a **fully isolated Python package** — its own
  `pyproject.toml` + lockfile, its own venv, and **ZERO imports from the engine
  repo**. The entire directory must be copy-paste-able to a new repo and run as-is.
  This is a hard constraint (Kenny 2026-06-12: standalone/extractable, separate from
  the engine build graph, movable to another repo).
- **D-02:** No coupling to the engine's CMake/MSBuild build graph. The tool builds
  and tests independently of any engine target.

### Parser reuse
- **D-03:** **Vendor-copy and own.** Copy `tre_reader.py` + `tre_decrypt.py` into
  `tools/tre-compare/` (e.g. under a `tre_core/` or `parser/` subpackage), record
  provenance (source path + the swg-blender-plugin commit hash) in a file header,
  then own and diverge freely. No git submodule, no editable install, no live link
  back to swg-blender-plugin — that would break clean extraction.
- **D-04:** **Source-of-truth correction:** the parser lives at
  `D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py` (+ `tre_decrypt.py`).
  An earlier memory note claiming the repo moved to `D:/Code/swg-tools` is STALE —
  that path does not exist. Vendor from swg-blender-plugin.

### Toolchain
- **D-05:** Use **uv** for venv + dependency locking + running, `pytest` for tests.
  Single fast tool; sets a clean foundation for Phase 29's FastAPI/sqlite/xxhash
  additions and Phase 30's frontend tooling.

### Test-data strategy
- **D-06:** **Synthetic fixtures + opt-in real.** Commit tiny, byte-built mini
  TRE/TOC archives + a synthetic `client.cfg` as the portable unit-test corpus
  (runs anywhere, including CI and after extraction). The synthetic corpus MUST
  exercise: each supported version tag, override shadowing (first-hit-wins across
  priorities), and length-0 tombstone removal, plus `fixUpFileName` canonicalization.
- **D-07:** Add exactly **one optional integration test** against the real install
  (`stage/client.cfg` → `D:/Code/SWGSource Client v3.0/...`), **gated** behind an
  env var / pytest marker so the default test run stays machine-independent. This
  satisfies SC#3's "real `stage/client.cfg`" intent without breaking extractability.
- **D-08:** The scanner must NOT hardcode `stage/client.cfg`; cfg path is an
  argument/parameter (a real install cfg uses machine-specific absolute paths like
  `D:/Code/SWGSource Client v3.0/`).

### Claude's Discretion
- Internal subpackage layout, module names, and dataclass shapes — planner/executor
  decide, subject to the isolation constraint (D-01).
- Exact byte-construction approach for synthetic fixtures (programmatic builder vs
  committed binary blobs) — whichever keeps the corpus small and readable.

### Folded Todos
- **`2026-05-15-swgsource-vs-whitengold-tre-asset-diff.md`** — the driving use case:
  diagnose space-scene graphics artifacts by diffing SWGSource (Restoration) vs
  whitengold TRE sets. This is the north-star scenario the whole 28→29→30 stream
  must answer; Phase 28 lays the parser/scanner/virtual-tree groundwork for it.
- **`2026-06-12-tre-editor-standalone-separate-directory.md`** — the standalone/
  extractable constraint, encoded as D-01/D-02 above.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Roadmap & requirements
- `.planning/ROADMAP.md` §"Phase 28" — goal + the 3 success criteria (the spec for this phase)
- `.planning/REQUIREMENTS.md` — TRE-01 (Phase 28 requirement); TRE-02..05 (downstream); TREM-01..03 + the "architecture must not preclude" constraint
- `.planning/PROJECT.md` — v2.3 Hardening milestone goal (TRE tool is the standalone web-app stream)

### Parser source (vendor from here)
- `D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py` — version-aware TRE/TOC/COT2000 reader (TREE 0004/0005/6000/0006/5000, TAG_TOC/0001, COT2000)
- `D:/Code/swg-blender-plugin/swg_pipeline/tre_decrypt.py` — decrypt/deobfuscation helper

### Engine semantics to mirror (read for faithful precedence + canonicalization)
- `src/engine/shared/library/sharedFile/src/shared/TreeFile.cpp` — `searchTree`/`searchTOC`/`searchPath` precedence, first-match-wins, `fixUpFileName` normalization, tombstone (length-0) handling
- `stage/client.cfg` §`[SharedFile]` — real-world key shapes: `maxSearchPriority`, `TOCTreePath`, `searchTOC_NN_P`, `searchTree_NN_P`, `searchPath_NN_P` (repeated indexed keys with embedded priority)

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `tre_reader.py`: already supports the full version matrix and both master-index
  formats (retail SearchTOC + Restoration COT2000) — vendor as-is, no rewrite.
- `stage/client.cfg`: a real, representative `[SharedFile]` block (sku0–sku3 TOCs,
  two override `.tre`s at priorities 7/8, a loose `searchPath` override at 10,
  `maxSearchPriority=12`) — the model the synthetic fixture cfg should imitate.
- `tools/` already hosts standalone helper exes (`TreeFileExtractor.exe`,
  `TreeFileBuilder.exe`, etc.) — precedent for tool-dir residency, but the new tool
  is a self-isolated Python package, not another loose exe.

### Established Patterns
- The `searchXXX_NN_P` key convention encodes priority in the key name; **higher
  priority number is searched FIRST** (override dir at 10 beats TREs at 7/8).
- **Tombstone is PER-NODE-TYPE — NOT a blanket "length-0 removes a lower copy" rule**
  (the #1 correctness nuance the phase hinges on; see RESEARCH §Patterns and
  `TreeFile_SearchNode.cpp`). A length-0 entry in a **SearchTree (`.tre`)** node
  sets `deleted=true` and the engine's `!deleted` find-guard STOPS the search →
  the file is **removed globally** (override-delete). A length-0 OR offset-0 entry
  in a **SearchTOC (`.toc`)** node returns "not here" but leaves `deleted=false` →
  it is **absent-here-keep-searching** and does **NOT** shadow a real lower-priority
  copy. SearchPath (loose dir) nodes have no tombstone concept (filesystem existence
  only). Implementers MUST branch on node kind; treating length-0 as a uniform
  "remove lower copy" is the regression this phase exists to prevent.
- `fixUpFileName` canonicalization must run before path comparison/merge.

### Integration Points
- This phase produces a pure library; Phase 29 will import the virtual-tree builder
  behind a FastAPI surface + sqlite index cache. Keep the public surface clean and
  importable so 29 needs no refactor.

</code_context>

<specifics>
## Specific Ideas

- "Changed" detection is explicitly via `(length, compressedLength)` and on-demand
  real hashing (xxhash) — NOT the TOC `crc` field (it's a path-CRC). This lands in
  Phase 29, but the parser/virtual-tree data model in Phase 28 must expose
  `length` + `compressedLength` per entry so 29 can diff without re-parsing.
- Target dataset for eventual end-to-end validation: the SWGSource-vs-whitengold
  install pair, space-asset diagnosis.

</specifics>

<deferred>
## Deferred Ideas

- **Diff engine + FastAPI + sqlite cache** — Phase 29 (TRE-02/03/04).
- **Web SPA (TanStack Virtual tree, filters, detail panel)** — Phase 30 (TRE-05).
- **TRE management (extract / repack / IFF-aware diff)** — TREM-01..03, a later
  increment; v2.3 is read-only compare. Architecture must not preclude these
  (REQUIREMENTS.md constraint) but they are NOT built now.

### Reviewed Todos (not folded)
- `2026-05-15-cantina-corner-snap-engine-improvement.md` — keyword match only;
  belongs to the engine/collision stream (Phase 25), not the TRE tool.
- `2026-06-13-64bit-x64-port.md` — keyword match only; separate future milestone.

</deferred>

---

*Phase: 28-TRE Compare Tool — Foundation (Parser + Scanner + Virtual Tree)*
*Context gathered: 2026-06-14*
*Revised 2026-06-14 (cross-AI review, finding #9): corrected the blanket "length-0 tombstone removes lower copy" Established-Patterns line to the per-node-type distinction (tree=global-remove vs toc=skip-only) to prevent a downstream-regression trap.*
