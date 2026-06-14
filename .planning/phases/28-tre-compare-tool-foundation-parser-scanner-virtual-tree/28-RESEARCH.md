# Phase 28: TRE Compare Tool — Foundation (Parser + Scanner + Virtual Tree) - Research

**Researched:** 2026-06-14
**Domain:** Python library packaging (uv/pytest) + SWG TRE/TOC binary-archive parsing + faithful mirror of engine `TreeFile.cpp` search-path precedence semantics
**Confidence:** HIGH (every load-bearing claim verified against the actual source files in this session)

## Summary

Phase 28 stands up a headless, fully unit-tested, **standalone** Python package at
`tools/tre-compare/` with exactly three pieces: a vendored TRE/TOC parser, a hand-written
`client.cfg` `[SharedFile]` scanner, and an engine-faithful virtual-tree merge builder. No
diff engine, no API, no UI.

The good news from investigation: the parser to vendor
(`D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py` + `tre_decrypt.py`) is **stdlib-only**
(`struct`, `zlib`, `dataclasses`, `enum`, `pathlib`, `shutil`) — vendoring is genuinely clean,
`pyproject.toml` needs **zero runtime dependencies**, and the package is trivially extractable
per D-01. The only edit required on vendor is rewriting one in-function import
(`from swg_pipeline.tre_decrypt import ...` at `tre_reader.py:251`) to the new package path.
The parser already exposes per-entry `length` **and** `compressed_length` on all three entry
dataclasses, satisfying the Phase-29 forward-compat requirement.

The engine semantics were read in full and are unambiguous. Priority ordering, first-match-wins,
`fixUpFileName` canonicalization, and tombstone (length-0) handling are all captured below with
exact code quotes. **One critical subtlety the CONTEXT did not surface:** the length-0 tombstone
"removes lower-priority copies" behavior is **only** true for `SearchTree` nodes — `SearchTOC`
nodes treat length-0 as merely "not here, keep searching" and do **not** shadow. The virtual-tree
builder must mirror this per-node-type difference, not apply a blanket rule.

`uv 0.11.7` and `Python 3.14.4` are both already installed on this machine.

**Primary recommendation:** `uv init --lib` a package at `tools/tre-compare/` (src layout, zero
runtime deps, pytest as the only dev dep); vendor the two parser files verbatim + fix the one
relative import + add a provenance header; hand-write the scanner against the exact
`searchXXX_NN_P` key grammar (NN = SKU number, P = priority); build the virtual tree mirroring
`TreeFile.cpp`'s `searchNodePriorityOrder` (higher number first), `find()` first-match-wins loop,
`fixUpFileName`, and the **SearchTree-only** length-0 tombstone; prove it with programmatically
byte-built synthetic fixtures + one env-gated integration test against `stage/client.cfg`.

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01:** `tools/tre-compare/` is a **fully isolated Python package** — its own `pyproject.toml`
  + lockfile, its own venv, and **ZERO imports from the engine repo**. The entire directory must
  be copy-paste-able to a new repo and run as-is. Hard constraint (Kenny 2026-06-12).
- **D-02:** No coupling to the engine's CMake/MSBuild build graph. The tool builds and tests
  independently of any engine target.
- **D-03:** **Vendor-copy and own.** Copy `tre_reader.py` + `tre_decrypt.py` into `tools/tre-compare/`
  (e.g. under a `tre_core/` or `parser/` subpackage), record provenance (source path + the
  swg-blender-plugin commit hash) in a file header, then own and diverge freely. No git submodule,
  no editable install, no live link back to swg-blender-plugin.
- **D-04:** **Source-of-truth correction:** the parser lives at
  `D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py` (+ `tre_decrypt.py`). The memory note
  claiming the repo moved to `D:/Code/swg-tools` is STALE — that path does not exist. Vendor from
  swg-blender-plugin.
- **D-05:** Use **uv** for venv + dependency locking + running, `pytest` for tests.
- **D-06:** **Synthetic fixtures + opt-in real.** Commit tiny, byte-built mini TRE/TOC archives +
  a synthetic `client.cfg` as the portable unit-test corpus. The synthetic corpus MUST exercise:
  each supported version tag, override shadowing (first-hit-wins across priorities), length-0
  tombstone removal, and `fixUpFileName` canonicalization.
- **D-07:** Add exactly **one optional integration test** against the real install
  (`stage/client.cfg` → `D:/Code/SWGSource Client v3.0/...`), **gated** behind an env var / pytest
  marker so the default test run stays machine-independent.
- **D-08:** The scanner must NOT hardcode `stage/client.cfg`; cfg path is an argument/parameter.

### Claude's Discretion
- Internal subpackage layout, module names, and dataclass shapes — planner/executor decide,
  subject to the isolation constraint (D-01).
- Exact byte-construction approach for synthetic fixtures (programmatic builder vs committed binary
  blobs) — whichever keeps the corpus small and readable.
  *(Research recommendation below: programmatic builder. See Architecture Patterns.)*

### Deferred Ideas (OUT OF SCOPE)
- **Diff engine + FastAPI + sqlite cache** — Phase 29 (TRE-02/03/04).
- **Web SPA (TanStack Virtual tree, filters, detail panel)** — Phase 30 (TRE-05).
- **TRE management (extract / repack / IFF-aware diff)** — TREM-01..03, later increment.
  Architecture must not preclude these but they are NOT built now.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| TRE-01 | User can point the tool at two SWG installations and scan their TRE sets — archives discovered plus each install's cfg search-path order parsed (engine-faithful: `searchTree`/`searchTOC`/`searchPath` priorities) | Parser API (Standard Stack §Parser); scanner cfg-key grammar (Architecture §Scanner) reproduces the exact C++ `TreeFile::install` key-build loop; virtual-tree builder (Architecture §Virtual Tree) mirrors `searchNodePriorityOrder` + `find()` first-match-wins + `fixUpFileName` + SearchTree tombstone. The "two installations" surface in TRE-01 is delivered by making the scanner accept a cfg path argument (D-08) so it runs against either install's cfg independently; the *compare* of the two is Phase 29. |

**Phase-28 scope of TRE-01:** the foundation must *scan* and *merge-into-a-virtual-tree* one
install correctly and faithfully. The two-install *diff* is Phase 29. The architecture must expose
the parsed node list and merged tree as clean importable objects so 29 needs no refactor.
</phase_requirements>

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Parse `.tre` / `.toc` binary archives | Parser subpackage (vendored) | — | Pure binary I/O; already written, stdlib-only |
| Decrypt/deobfuscate Restoration payloads | Parser subpackage (`tre_decrypt`) | — | Helper to parser; not needed for TOC-only metadata reads in Phase 28 |
| Parse `client.cfg [SharedFile]` repeated keys | Scanner subpackage (new) | — | Stdlib `configparser` cannot handle repeated keys; hand-write |
| Order nodes by priority + merge to virtual tree | Virtual-tree builder (new) | Parser (reads each node's entries) | Mirrors `TreeFile.cpp` precedence; the engine-faithful core |
| Canonicalize file paths (`fixUpFileName`) | Virtual-tree builder (shared util) | — | Must run before any path comparison/merge |
| Test corpus (synthetic archives + cfg) | Test fixtures | — | Byte-built; portable; runs after extraction |

This is a single-tier (pure Python library) project. The "tiers" above are subpackage boundaries,
not deployment tiers. There is no browser/server/DB split in Phase 28.

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Python | `>=3.11` (3.14.4 installed) | Runtime | [VERIFIED: `python --version` → 3.14.4]. Parser uses `from __future__ import annotations` + PEP 604 `str | Path` unions + frozen dataclasses; 3.11+ is safe. uv `--lib` default pins `requires-python = ">=3.11"`. |
| uv | 0.11.7 | venv + lock + run + build | [VERIFIED: `uv --version` → 0.11.7]. D-05 mandate. Single fast tool; `uv.lock` is committed for reproducibility. |
| pytest | latest (resolve via `uv add --dev pytest`) | Test runner | [VERIFIED: D-05 mandate]. Only dev dependency needed in Phase 28. |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| (none) | — | — | **Phase 28 has zero runtime dependencies.** The vendored parser is stdlib-only [VERIFIED: grep of imports below]. `xxhash`, `fastapi`, `sqlite` etc. arrive in Phase 29 — do NOT add them now. |

**Verified import inventory of the files to vendor** [VERIFIED: Grep of source]:
- `tre_reader.py`: `shutil`, `struct`, `zlib`, `dataclasses.dataclass`, `enum.Enum`, `pathlib.Path`, `from __future__ import annotations` — **all stdlib**. Plus one *in-function* import at line 251: `from swg_pipeline.tre_decrypt import try_read_tre_payload` — **this is the one line that must be rewritten** to the vendored package path (e.g. `from .tre_decrypt import try_read_tre_payload` or the chosen subpackage path).
- `tre_decrypt.py`: `zlib`, `dataclasses.dataclass`, `from __future__ import annotations` — **all stdlib**.

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `uv init --lib` | `uv init --package` / manual `pyproject.toml` | `--lib` *implies* `--package` and generates the correct src layout + `py.typed` + `uv_build` backend [CITED: docs.astral.sh/uv]. Use `--lib`. |
| hand-written cfg scanner | stdlib `configparser` | `configparser` collapses repeated keys (`searchTOC_00_0`, `searchTOC_01_1`...) — actually those keys are *distinct* strings so configparser would NOT collapse them, BUT the engine reads each key with a repeating *index* (`getKeyString(..., index, ...)`) meaning a SINGLE key name can legally appear multiple times in a cfg. configparser raises `DuplicateOptionError` on that. Hand-write per D-06/CONTEXT. (See Pitfall 4 for the precise reason.) |
| programmatic fixture builder | committed binary blobs | Both allowed by D's discretion. **Recommend programmatic** — readable, diff-able, self-documenting, and forces the team to encode the exact byte layout (which doubles as parser documentation). |

**Installation / scaffold:**
```bash
# From repo root
uv init --lib tools/tre-compare      # creates src layout, pyproject.toml, .python-version, py.typed
cd tools/tre-compare
uv add --dev pytest                  # only dev dependency
# vendor the two parser files into the chosen subpackage, fix the one import, add provenance header
uv run pytest                        # run the suite
```

**Version verification performed this session:**
- `uv 0.11.7` [VERIFIED: `uv --version`, build 2026-04-15]
- `Python 3.14.4` [VERIFIED: `python --version`]
- pytest version intentionally NOT pinned here — resolve latest at scaffold time via `uv add --dev pytest`; uv writes the resolved version into `uv.lock`.

## Architecture Patterns

### System Architecture Diagram

```
                          client.cfg (path passed in — D-08)
                                   │
                                   ▼
                     ┌──────────────────────────┐
                     │   SCANNER (hand-written)  │   parses [SharedFile]:
                     │   parse_shared_file()     │   maxSearchPriority, TOCTreePath,
                     └──────────────────────────┘   searchTOC_NN_P / searchTree_NN_P /
                                   │                 searchPath_NN_P  (repeated indexed keys)
                                   ▼
                   priority-ordered list of SearchNode descriptors
                   (kind ∈ {tree, toc, path}, priority:int, abspath:str)
                   sorted: HIGHER priority number FIRST   (mirrors searchNodePriorityOrder)
                                   │
                  ┌────────────────┴─────────────────┐
                  ▼ (tree / toc nodes)                ▼ (path nodes)
       ┌──────────────────────┐            (filesystem dir; enumerated lazily —
       │  PARSER (vendored)   │             not in Phase-28 merge unless a dir walk
       │  read_tre_entries()  │             is in scope; see Open Question 1)
       │  parse_master_index()│
       └──────────────────────┘
                  │  per-entry: path, crc, length, compressed_length, offset, compressor
                  ▼
       ┌─────────────────────────────────────────────┐
       │  VIRTUAL-TREE BUILDER (new)                  │
       │  for each node in priority order:            │
       │    for each entry:                           │
       │      key = fixUpFileName(entry.path)         │  ← canonicalize FIRST
       │      if key already claimed: skip (1st wins) │  ← first-match-wins
       │      if SearchTree entry.length == 0:        │  ← tombstone (SearchTree ONLY)
       │          mark key tombstoned → shadow lower  │
       │      else: record winner = this node         │
       └─────────────────────────────────────────────┘
                  │
                  ▼
       merged virtual tree: { canonical_path -> WinningEntry(+shadowed copies list) }
                  │
                  └──► consumed by Phase 29 diff engine (imports this object)
```

### Recommended Project Structure
```
tools/tre-compare/
├── .python-version            # written by uv init
├── .gitignore                 # add: .venv/  __pycache__/  *.pyc  (repo .gitignore has none)
├── README.md
├── pyproject.toml             # zero runtime deps; pytest under [dependency-groups]/dev
├── uv.lock                    # committed
├── src/
│   └── tre_compare/
│       ├── __init__.py
│       ├── py.typed
│       ├── parser/            # vendored, owned (D-03)
│       │   ├── __init__.py
│       │   ├── tre_reader.py  # + provenance header; fix line-251 import
│       │   └── tre_decrypt.py # + provenance header
│       ├── scanner.py         # hand-written [SharedFile] parser (D-08: takes cfg path)
│       └── virtual_tree.py    # fixUpFileName + priority merge + tombstone
└── tests/
    ├── conftest.py            # shared fixtures (synthetic corpus builders)
    ├── fixtures/              # OR build programmatically in conftest (recommended)
    │   └── synthetic_client.cfg
    ├── test_parser.py
    ├── test_scanner.py
    ├── test_virtual_tree.py
    └── test_integration.py    # D-07 env/marker-gated against stage/client.cfg
```
*(Exact module names are Claude's discretion per CONTEXT; this is a recommendation.)*

### Pattern 1: Engine-faithful priority ordering (HIGHER number = searched FIRST)
**What:** Nodes are sorted descending by priority; ties keep insertion (config) order.
**Verified against C++** [VERIFIED: TreeFile.cpp:285-308]:
```cpp
// Source: src/engine/shared/library/sharedFile/src/shared/TreeFile.cpp:285
bool TreeFile::searchNodePriorityOrder(const SearchNode *a, const SearchNode *b)
{
    return a->getPriority() > b->getPriority();   // strictly greater → HIGHER first
}
// addSearchNode uses std::lower_bound with this comparator; "New nodes with the
// same priority as old nodes will be inserted after the last priority match."
```
So in the live `stage/client.cfg`: `searchPath` at priority **10** is searched BEFORE the
`.tre`s at 7/8, which are searched before the `.toc`s at 0–3. CONTEXT's claim is **CONFIRMED**.
**Python mirror:** `sorted(nodes, key=lambda n: -n.priority)` with a *stable* sort (Python's sort
is stable) preserves config insertion order within a priority tie — matching the C++ "insert after
last match" behavior.

### Pattern 2: First-match-wins lookup
**Verified** [VERIFIED: TreeFile.cpp:437-461]:
```cpp
// Source: TreeFile.cpp:456 — iterate nodes in priority order, return FIRST hit
for (SearchNodes::iterator i = ms_searchNodes.begin(); !deleted && i != iEnd; ++i)
    if ((*i)->exists(fileName, deleted))
        return *i;        // highest-priority node that has the file wins
```
**Python mirror:** when building the merged tree, the first node (in priority order) to claim a
canonical path is the winner; later nodes' copies are "shadowed."

### Pattern 3: `fixUpFileName` canonicalization (exact transform)
**Verified** [VERIFIED: TreeFile.cpp:511-601]. The EXACT transform, in order:
1. Strip ALL leading `\` or `/`.
2. Strip leading `./` or `.\` sequences (repeatedly).
3. Strip leading `../` or `..\` sequences (repeatedly).
4. For the remainder: convert every `\` to `/`; `tolower()` every non-slash character.
5. Collapse repeated consecutive slashes to a single `/`.
6. (No trailing-slash strip in `fixUpFileName` itself — that happens only in `SearchPath` ctor.)
```cpp
// Source: TreeFile.cpp:569 (the core loop)
for (; *f; ++f) {
    const char c = *f;
    if (c == '\\' || c == '/') { currentIsSlash = true;  *output = '/'; }
    else                       { currentIsSlash = false; *output = static_cast<char>(tolower(c)); }
    if (!currentIsSlash || !previousIsSlash) { ++output; previousIsSlash = currentIsSlash; }
}
```
**Python mirror (faithful):**
```python
def fix_up_file_name(name: str) -> str:
    f = name
    while f[:1] in ("\\", "/"):           # strip leading slashes
        f = f[1:]
    while f[:2] in ("./", ".\\"):         # strip leading ./
        f = f[2:]
    while f[:3] in ("../", "..\\"):       # strip leading ../
        f = f[3:]
    out: list[str] = []
    prev_slash = False
    for c in f:
        if c in ("\\", "/"):
            cur_slash = True; ch = "/"
        else:
            cur_slash = False; ch = c.lower()
        if not cur_slash or not prev_slash:   # collapse repeated slashes
            out.append(ch); prev_slash = cur_slash
    return "".join(out)
```
**Note on case-folding:** C++ `tolower()` is locale-/ASCII-oriented. SWG asset paths are ASCII;
Python `str.lower()` is Unicode-aware but identical for ASCII. Use `.lower()` — safe for the
ASCII asset namespace. (If paranoid, restrict to `c.lower() if c.isascii() else c`, but the asset
set is ASCII so plain `.lower()` is faithful.)

### Pattern 4: Tombstone (length-0) handling — **PER-NODE-TYPE, NOT BLANKET**
This is the most important correctness nuance and the CONTEXT under-specifies it.

**SearchTree (`.tre`): length-0 IS a true tombstone** [VERIFIED: TreeFile_SearchNode.cpp:395-407]:
```cpp
// Source: TreeFile_SearchNode.cpp:397
if (found) {
    if (m_tableOfContents[mid].length == 0) {
        deleted = true;     // ← sets the OUT param
        return false;       // ← "not found here"
    }
    ...
}
```
Combined with `find()`'s loop guard `!deleted` (TreeFile.cpp:456): once a SearchTree reports a
length-0 entry for a path, `deleted` becomes true and **the entire search stops** — lower-priority
nodes are never consulted. The file is effectively *removed*. This is the override-delete mechanism.

**SearchTOC (`.toc`): length-0 does NOT tombstone** [VERIFIED: TreeFile_SearchNode.cpp:807-833]:
```cpp
// Source: TreeFile_SearchNode.cpp:807
if (found) {
    if (m_tableOfContents[mid].length == 0)  return false;          // just "not here"
    else if (m_tableOfContents[mid].offset == 0)  return false;     // invalid pointer → "not here"
    ...
}
// and SearchTOC::exists ALWAYS does:  deleted = false;  (line 831)
```
SearchTOC never sets `deleted`, so a length-0 (or offset-0) entry in a `.toc` is treated as
"absent in this node, keep searching lower priority" — it does **not** shadow lower copies.

**SearchPath (dir): no tombstone concept** — filesystem existence only.

**Python mirror requirement:** the virtual-tree builder must track `deleted`/tombstone state and
honor it **only for tree-kind nodes**. A length-0 entry in a tree node removes the path globally
(stop, file is deleted); a length-0/offset-0 entry in a toc node is simply skipped. **Do not apply
a single blanket "length==0 removes the file" rule** — that would wrongly let a `.toc` length-0
shadow a real lower-priority file.

### Pattern 5: Vendoring with provenance (D-03)
Add a header to each vendored file:
```python
# Vendored 2026-06-14 from swg-blender-plugin
#   source: D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py
#   commit: f803f58782e675e85844960fe868b0849405249a (master)
# Copied per Phase-28 D-03; owned and may diverge. No live link back.
```
[VERIFIED: `git -C D:/Code/swg-blender-plugin log -1` → commit `f803f58782e675e85844960fe868b0849405249a`, dated 2026-06-14, branch master.]

### Anti-Patterns to Avoid
- **Using `configparser` for `[SharedFile]`:** breaks on legitimately-repeated key names (see Pitfall 4). D-06 says hand-write; do it.
- **Blanket length-0 tombstone:** wrong for `.toc` nodes (Pattern 4). Per-node-type only.
- **Re-deriving CRC for path matching:** the merge key is the canonicalized *path string*, not the TOC `crc` (which is a path-CRC used for the engine's binary search; CONTEXT §specifics + TRE-04 both warn it is NOT a content hash). Match on `fix_up_file_name(path)`.
- **Importing anything from the engine repo or from `swg_pipeline`:** violates D-01/D-03. After vendoring, the `swg_pipeline.tre_decrypt` import at `tre_reader.py:251` MUST be rewritten to the local package path or the package is not extractable.
- **Hardcoding `stage/client.cfg`:** violates D-08; cfg path is a parameter.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| TRE/TOC binary parsing | A fresh `struct.unpack` reader | The vendored `tre_reader.py` | Already handles 5 TRE version tags + COT2000 + SearchTOC, with zlib payload + name-block-offset arithmetic. Rewriting risks subtle offset bugs. (D-03 mandates vendoring.) |
| zlib payload decompress + Restoration fallback | Custom decompressor | Vendored `read_tre_payload` / `tre_decrypt.try_read_tre_payload` | Restoration v6000/0006/5000 payloads may be obfuscated; the helper already routes the fallback. (Not needed for Phase-28 metadata-only reads, but present if drill-in is added.) |
| venv + lock + run | pip + requirements.txt + manual venv | uv (`uv init/add/run/lock`) | D-05; faster, single lockfile, reproducible. |
| Reading repeated cfg keys | — | A small hand-written line parser | This one you DO hand-roll (per D-06) because no stdlib option handles SWG's repeated-key + indexed-key grammar. ~30 lines. |

**Key insight:** Phase 28's only genuinely-new code is the scanner (~30–50 lines) and the
virtual-tree builder (~80–120 lines). The parser is a vendored, proven, stdlib-only asset. Keep
the new surface minimal and faithful; the value is *correctness vs. the C++*, not volume.

## Runtime State Inventory

> Phase 28 is greenfield (a brand-new package) — not a rename/refactor/migration. This section is
> included only to record the one cross-boundary fact that behaves like migration state.

| Category | Items Found | Action Required |
|----------|-------------|------------------|
| Stored data | None — no datastore in Phase 28 (sqlite cache is Phase 29). | None. |
| Live service config | The real install paths in `stage/client.cfg` point at `D:/Code/SWGSource Client v3.0/` (machine-specific absolute paths). | The D-07 integration test must skip cleanly when that path is absent. Default test run must not depend on it (D-08). |
| OS-registered state | None. | None. |
| Secrets/env vars | The D-07 integration gate needs an env var (recommend `TRE_COMPARE_INTEGRATION=1` or a pytest marker `@pytest.mark.integration`). Precedent: the source `swg_pipeline/sample_tre.py` uses `SWG_SAMPLE_TRE_DIR`. | Define the gate env-var name in the plan; default-off. |
| Build artifacts | The vendored `tre_reader.py:251` import (`from swg_pipeline.tre_decrypt ...`) is a stale cross-package reference once copied. | **Code edit** — rewrite to the local package path. This is the single mandatory edit on vendor. |

**The canonical question (rename-style):** after copying the two parser files, the only thing
still referencing the old package is that one in-function import line. Fix it and the package is
fully extractable.

## Common Pitfalls

### Pitfall 1: Applying length-0 tombstone uniformly across node types
**What goes wrong:** A `.toc` master index legitimately contains length-0/offset-0 entries that
mean "absent here," not "deleted everywhere." Treating them as tombstones wrongly hides real files
that exist in lower-priority archives.
**Why it happens:** CONTEXT phrases the tombstone rule generically ("length-0 entry shadows/removes
a lower-priority file"). The C++ only does global-removal for `SearchTree`.
**How to avoid:** Mirror Pattern 4 exactly — tombstone semantics for tree-kind nodes; plain skip
for toc-kind nodes.
**Warning signs:** A synthetic test where a `.toc` length-0 entry incorrectly removes a file that
a lower-priority `.tre` provides.

### Pitfall 2: Wrong priority direction
**What goes wrong:** Sorting ascending makes the override dir (priority 10) lose to the base TREs.
**Why it happens:** "Priority" intuitively suggests lower-number-first in many systems.
**How to avoid:** `a->getPriority() > b->getPriority()` — HIGHER number searched FIRST [VERIFIED:
TreeFile.cpp:287]. Sort descending. Add an explicit test: override at 10 beats base at 7/8.

### Pitfall 3: `fixUpFileName` collapse + leading-strip order
**What goes wrong:** Forgetting to strip leading `./` / `../` / slashes, or not collapsing repeated
slashes, yields keys that don't match the engine's and produces false "added/removed" diffs in
Phase 29.
**Why it happens:** It's a fiddly multi-step transform.
**How to avoid:** Port Pattern 3 verbatim; test `\\foo\\\\Bar/baz.dds` → `foo/bar/baz.dds`,
`./a/b` → `a/b`, `../x` → `x`, mixed-case → lowercase.

### Pitfall 4: Using `configparser` for `[SharedFile]`
**What goes wrong:** `configparser` raises `DuplicateOptionError` (strict mode) or silently keeps
last value when a key name repeats. The engine reads each `searchXXX_NN_P` via an *index* loop
(`getKeyString("SharedFile", buffer, index, NULL)` with `++index`), so the SAME key name can
legally appear multiple times in one cfg, each occurrence a distinct search node.
[VERIFIED: TreeFile.cpp:126, 136, 146 — the `for (int index = 0; ... ++index)` pattern.]
**Why it happens:** configparser looks like the obvious tool.
**How to avoid:** Hand-write a line scanner (D-06). Also: the live cfg uses TAB indentation and
quoted values with spaces (`TOCTreePath="D:/Code/SWGSource Client v3.0/"`); strip surrounding
quotes and honor `#` comment lines. [VERIFIED: actual `stage/client.cfg` content below.]

### Pitfall 5: Misreading the `NN` in `searchTOC_NN_P`
**What goes wrong:** Treating `NN` as a free index counter; it is actually the **SKU number**.
**Why it happens:** It looks like an enumerator.
**How to avoid:** [VERIFIED: TreeFile.cpp:113-115] `sprintf(skuText, "_%02d_", sku)` builds the
`_NN_` middle from the SKU number, and `sprintf(buffer, "searchTOC%s%d", skuText, priority)`
appends the priority. So `searchTOC_00_0` = SKU 00, priority 0; `searchTree_00_7` = SKU 00,
priority 7. For the scanner you can parse the trailing `_<priority>` as the priority and treat the
whole base as one node descriptor; the SKU segment matters only if you later model multi-SKU
accounts (out of scope for Phase 28 — the live cfg is all SKU 00). Recommend: parse priority from
the final `_<int>` suffix; retain the full key for provenance.

### Pitfall 6: `.gitignore` gap commits the venv
**What goes wrong:** The repo `.gitignore` has NO Python entries [VERIFIED: grep]. `uv` creates
`.venv/` and `__pycache__/`; committing them bloats the repo and breaks extractability.
**How to avoid:** Add a `tools/tre-compare/.gitignore` with `.venv/ __pycache__/ *.pyc`. Commit
`uv.lock` (it's reproducibility, not an artifact).

## Code Examples

### Parser public API (vendored — what Phase 28 imports)
[VERIFIED: `tre_reader.py` definitions + `swg_pipeline/tre_list.py` consumer usage]
```python
# Source: D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py
read_tre_header(path)        -> TreHeader          # token, version, number_of_files, offsets...
read_tre_entries(path)       -> list[TreEntry]     # per-file metadata
list_tre(path)               -> list[str]          # just the paths
read_tre_payload(path, name) -> bytes              # decompressed bytes (drill-in; Phase 29)

detect_master_index_kind(path) -> MasterIndexKind  # COT2000 | SEARCH_TOC
parse_master_index(path)     -> dict               # auto-routes COT2000 vs SearchTOC
read_cot2000_entries(path)   -> list[Cot2000Entry]
read_search_toc_entries(path)-> list[SearchTocEntry]
toc_entry_stride(version)    -> int                # 24 (retail/5000) or 32 (extended)

# Exceptions:  UnsupportedTreVersionError, TreFormatError
# Supported tags: SUPPORTED_TRE_VERSIONS = {"0004","0005","6000","0006","5000"}
#                 master indexes: COT2000 magic " COT2000" ; SearchTOC = TAG_TOC + TAG_0001
```

### Per-entry fields — `length` AND `compressed_length` BOTH present (Phase-29 forward-compat)
[VERIFIED: `tre_reader.py:72-105`]
```python
@dataclass(frozen=True)
class TreEntry:        path; crc; length; offset; compressor; compressed_length
@dataclass(frozen=True)
class Cot2000Entry:    path; tre_file; tree_index; crc; offset; length; compressor; compressed_length
@dataclass(frozen=True)
class SearchTocEntry:  path; tre_file; tree_index; crc; offset; length; compressor; compressed_length
```
**DISCREPANCY (minor, flag for planner):** CONTEXT §specifics writes the field as
`compressedLength` (camelCase). The actual Python attribute is **`compressed_length`** (snake_case).
All three entry types expose both `length` and `compressed_length` — the Phase-29 "changed
detection via `(length, compressedLength)`" requirement is satisfiable without re-parse, but code
in 29 must use the snake_case names (or the Phase-28 data model can re-expose them under whatever
names it chooses).

### The exact live `[SharedFile]` block (the fixture model)
[VERIFIED: actual `D:/Code/swg-client-v2/stage/client.cfg`]
```ini
[SharedFile]
	maxSearchPriority=12
	# TOCTreePath: directory prefix prepended to .tre filenames listed inside .toc files.
	# Must have trailing slash. Quoted to survive ConfigFile space-splitting on paths with spaces.
	TOCTreePath="D:/Code/SWGSource Client v3.0/"
	searchTOC_00_0="D:/Code/SWGSource Client v3.0/sku0_client.toc"
	searchTOC_01_1="D:/Code/SWGSource Client v3.0/sku1_client.toc"
	searchTOC_02_2="D:/Code/SWGSource Client v3.0/sku2_client.toc"
	searchTOC_03_3="D:/Code/SWGSource Client v3.0/sku3_client.toc"
	searchTree_00_7="D:/Code/SWGSource Client v3.0/disable_wayfar_dearic_snow.tre"
	searchTree_00_8="D:/Code/SWGSource Client v3.0/swgsource_3.0.tre"
	# Phase 19: loose override dir (priority 10 > TREs 7/8) for reauthored //hlsl
	searchPath_00_10="D:/Code/swg-client-v2/stage/override"
```
Notes for the scanner: TAB-indented; values double-quoted (strip quotes); `#` comments; the
priority is the final `_<int>` of the key; the resulting search order is **path(10) → tree(8) →
tree(7) → toc(3) → toc(2) → toc(1) → toc(0)** (higher first). The four `searchTOC` entries each
have priority equal to their SKU index here (0–3), so they sort in that order.

### TOC entry binary layouts (for byte-building synthetic fixtures)
[VERIFIED: `tre_reader.py` struct formats cross-checked against C++ `TableOfContentsEntry` structs]

**TRE TOC entry** (`TOC_ENTRY_FMT = "<Iiiiii"`, 24 bytes retail / 32 extended via stride):
`crc:u32, length:i32, offset:i32, compressor:i32, compressedLength:i32, fileNameOffset:i32`
— matches C++ `SearchTree::TableOfContentsEntry { uint32 crc; int length; int offset; int compressor; int compressedLength; int fileNameOffset; }` [VERIFIED: TreeFile_SearchNode.h:189-197].

**TRE header** (`STANDARD_HEADER_FMT = "<4s4s7I"`, 36 bytes):
`token('TREE'), version('0004'..), numberOfFiles, tocOffset, tocCompressor, sizeOfTOC, blockCompressor, sizeOfNameBlock, uncompSizeOfNameBlock` [VERIFIED: tre_reader.py:28-29 vs TreeFile_SearchNode.h:174-185].

**SearchTOC header** (`SEARCH_TOC_HEADER_FMT = "<4s4sBBBB6I"`):
`token('TOC '), version('0001'), tocCompressor:u8, nameBlockCompressor:u8, unused:u8, unused:u8, numberOfFiles, sizeOfTOC, sizeOfNameBlock, uncompSizeOfNameBlock, numberOfTreeFiles, sizeOfTreeFileNameBlock` [VERIFIED: tre_reader.py:36 vs TreeFile_SearchNode.h:262-276].

These layouts are sufficient to byte-build minimal valid archives. Smallest faithful synthetic
TRE = header(36) + N×24-byte TOC entries + null-terminated name block (+ optional payload bytes;
for metadata-only tests payloads can be empty/zero-length since the merge reads TOC, not payload).
**Note:** entries inside a real TRE are sorted by CRC (the engine does a binary search on
`Crc::calculate(fileName)`), but the Phase-28 virtual-tree builder iterates ALL entries and keys
on the *path string*, so CRC sort order is irrelevant to the merge — synthetic fixtures need not
compute real CRCs unless a future test exercises the parser's own lookup path. Set a placeholder
CRC; the merge does not consult it.

### uv library scaffold + test invocation
[CITED: docs.astral.sh/uv/concepts/projects/init]
```bash
uv init --lib tools/tre-compare    # → .python-version, README.md, pyproject.toml,
                                   #    src/tre_compare/{__init__.py, py.typed}
uv add --dev pytest                # dev-only dep; resolved version pinned in uv.lock
uv run pytest                      # run suite in the managed venv
uv run pytest -m "not integration" # default CI run (skip D-07 real-install test)
uv run pytest -m integration       # opt-in real-install test (with env/marker gate)
```
Generated `pyproject.toml` shape:
```toml
[project]
name = "tre-compare"
version = "0.1.0"
requires-python = ">=3.11"
dependencies = []                  # ZERO runtime deps in Phase 28
[build-system]
requires = ["uv_build>=0.11.21,<0.12"]
build-backend = "uv_build"
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| pip + venv + requirements.txt | uv (init/add/run/lock) | uv GA'd 2024–2025; standard by 2026 | Single tool, fast, lockfile reproducibility; D-05 mandate. |
| `configparser` for any `.ini`-ish file | hand-written for repeated/indexed keys | n/a | SWG cfg grammar is not standard INI; configparser is wrong tool here. |

**Deprecated/outdated:**
- The memory note pointing the parser at `D:/Code/swg-tools` — **STALE** (D-04). Path does not exist. Vendor from `D:/Code/swg-blender-plugin/swg_pipeline/`. [VERIFIED: directory listing of swg_pipeline succeeded; swg-tools not referenced.]

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | Python `>=3.11` is an acceptable floor (uv `--lib` default); 3.14.4 is installed | Standard Stack | LOW — parser uses only 3.10+ syntax; if CI pins an older Python, bump `requires-python`. Verified locally but CI env unknown. |
| A2 | `.lower()` faithfully reproduces C++ `tolower()` for SWG asset paths | Pattern 3 | LOW — SWG asset namespace is ASCII; identical. Only matters if a path contains non-ASCII bytes (none observed). |
| A3 | Phase-28 merge reads TOC metadata only, never payloads, so synthetic fixtures can have empty/placeholder payloads and placeholder CRCs | Code Examples | LOW — confirmed the merge keys on path string, not CRC/payload. If a test exercises `read_tre_payload`, real zlib payloads + matching offsets would be needed. |
| A4 | `searchPath` (loose dir) nodes are NOT walked into individual files during the Phase-28 merge (the merge composes archive TOCs; a dir node contributes by filesystem presence) | Architecture diagram / Open Q1 | MEDIUM — see Open Question 1. The engine treats SearchPath as a node that answers `exists()` per-file via the filesystem; whether Phase-28's virtual tree should enumerate the override dir's files into the merged tree is a design choice the planner must make. |

## Open Questions

1. **Should the virtual-tree builder enumerate files inside `searchPath` (loose dir) nodes?**
   - What we know: The engine's `SearchPath` answers `exists(file)` by checking the filesystem
     (`FileStreamer::exists(absPath)`); it does not pre-enumerate. [VERIFIED: TreeFile_SearchNode.cpp:109-114]
   - What's unclear: A *static* virtual-tree (for compare/diff) needs a concrete file list. To
     include override-dir files in the merged tree, the builder must `os.walk` the dir and inject
     each file at the dir node's priority (highest, so they win). The engine does this lazily;
     the tool must do it eagerly.
   - Recommendation: **Yes, walk searchPath dirs** and inject their files at the node priority,
     applying `fix_up_file_name` to the dir-relative path. This is required for TRE-01's "scan
     their TRE sets ... search-path order parsed" to reflect override files. Make it a unit-tested
     behavior (synthetic override dir beating a base TRE). Flag for the planner to confirm scope —
     it is the one place the static model must *add* behavior the lazy engine doesn't.

2. **Multi-SKU modeling.** The live cfg is all SKU 00. The engine loops SKUs via `skuBits`
   (TreeFile.cpp:106). Phase-28 scope is single-install scan; recommend ignoring SKU bitmask
   gating (treat all `searchXXX_NN_P` keys as active) and just parse priority. Confirm this is
   acceptable (it is for the SWGSource/whitengold use case, which uses SKU 00).

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| Python | Everything | ✓ | 3.14.4 | — |
| uv | scaffold/lock/run/test (D-05) | ✓ | 0.11.7 | — |
| pytest | tests | resolve via `uv add --dev pytest` | latest@scaffold | — |
| `stage/client.cfg` | D-07 integration test only | ✓ (present in repo) | — | Test skips if absent (env/marker gate) |
| `D:/Code/SWGSource Client v3.0/` (real TREs) | D-07 integration test only | NOT checked this session | — | Test skips if path missing; default run is synthetic-only |
| swg-blender-plugin source | one-time vendoring (D-03) | ✓ | commit f803f587 | — |

**Missing dependencies with no fallback:** none.
**Missing with fallback:** the real install dir for D-07 is machine-specific — the integration
test must skip gracefully when absent (this is exactly why D-07 is env/marker-gated and D-08
parameterizes the cfg path).

## Validation Architecture

> nyquist_validation: `.planning/config.json` not inspected for an explicit `false`; treating as
> enabled. Phase 28 is "fully unit-tested" by its own charter, so validation IS the deliverable.

### Test Framework
| Property | Value |
|----------|-------|
| Framework | pytest (resolved + pinned in `uv.lock` at scaffold) |
| Config file | `tools/tre-compare/pyproject.toml` `[tool.pytest.ini_options]` (register the `integration` marker here) — none exists yet → Wave 0 |
| Quick run command | `uv run pytest -m "not integration"` (from `tools/tre-compare/`) |
| Full suite command | `uv run pytest` (includes integration when gated env var set) |

### Phase Requirements → Test Map
| Req | Behavior | Test Type | Automated Command | File Exists? |
|-----|----------|-----------|-------------------|-------------|
| TRE-01 | Parser reads each supported TRE version tag (0004/0005/6000/0006/5000) | unit | `uv run pytest tests/test_parser.py -x` | ❌ Wave 0 |
| TRE-01 | Parser reads COT2000 + SearchTOC master indexes | unit | `uv run pytest tests/test_parser.py -x` | ❌ Wave 0 |
| TRE-01 | Scanner parses `[SharedFile]` repeated/indexed keys → priority-ordered node list | unit | `uv run pytest tests/test_scanner.py -x` | ❌ Wave 0 |
| TRE-01 | Scanner orders nodes HIGHER-priority-first (override 10 before tree 7/8 before toc 0–3) | unit | `uv run pytest tests/test_scanner.py -x` | ❌ Wave 0 |
| TRE-01 | `fix_up_file_name` matches C++ (leading-strip, slash-normalize, lowercase, collapse) | unit | `uv run pytest tests/test_virtual_tree.py -x` | ❌ Wave 0 |
| TRE-01 | First-match-wins: higher-priority node shadows lower for same path | unit | `uv run pytest tests/test_virtual_tree.py -x` | ❌ Wave 0 |
| TRE-01 | SearchTree length-0 tombstone removes file globally | unit | `uv run pytest tests/test_virtual_tree.py -x` | ❌ Wave 0 |
| TRE-01 | SearchTOC length-0 does NOT tombstone (file still served from lower priority) | unit | `uv run pytest tests/test_virtual_tree.py -x` | ❌ Wave 0 |
| TRE-01 | Real `stage/client.cfg` scans + merges without error (override/tombstone correct) | integration (gated) | `TRE_COMPARE_INTEGRATION=1 uv run pytest -m integration` | ❌ Wave 0 |

### Sampling Rate
- **Per task commit:** `uv run pytest -m "not integration"`
- **Per wave merge:** `uv run pytest -m "not integration"` (full synthetic suite)
- **Phase gate:** synthetic suite green; integration test green on this machine (manual, env-gated).

### Wave 0 Gaps
- [ ] `tools/tre-compare/` scaffold via `uv init --lib` (+ `uv add --dev pytest`)
- [ ] `tests/conftest.py` — synthetic TRE/TOC byte-builder fixtures + synthetic `client.cfg`
- [ ] `tests/test_parser.py`, `tests/test_scanner.py`, `tests/test_virtual_tree.py`
- [ ] `tests/test_integration.py` — env/marker-gated against `stage/client.cfg` (D-07)
- [ ] `[tool.pytest.ini_options] markers = ["integration: real-install test (env-gated)"]` in `pyproject.toml`
- [ ] `tools/tre-compare/.gitignore` (repo `.gitignore` has no Python entries)

## Project Constraints (from CLAUDE.md)

No `./CLAUDE.md` exists at the repo root [VERIFIED: Read returned "File does not exist"]. No
project-level coding directives to enforce beyond the CONTEXT decisions. The MEMORY note
"TRE editor (Phases 28-30) must be standalone/extractable" is already encoded as D-01/D-02.

## Sources

### Primary (HIGH confidence)
- `D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py` — full read; public API, entry dataclasses, struct layouts, version matrix, the line-251 cross-package import.
- `D:/Code/swg-blender-plugin/swg_pipeline/tre_decrypt.py` — full read; stdlib-only, payload fallback API.
- `D:/Code/swg-blender-plugin/swg_pipeline/tre_list.py` — consumer, confirms public API usage.
- `src/engine/shared/library/sharedFile/src/shared/TreeFile.cpp` — full read; `searchNodePriorityOrder` (285), `addSearchNode` lower_bound (299), `find` first-match-wins (437), `fixUpFileName` (511), install key-build loop (102-149).
- `src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.cpp` — full read; SearchTree tombstone (397), SearchTOC non-tombstone (807), SearchPath filesystem exists (109).
- `src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.h` — TableOfContentsEntry struct layouts (189, 281), header structs (174, 262).
- `D:/Code/swg-client-v2/stage/client.cfg` `[SharedFile]` — actual key shapes/values.
- `git -C D:/Code/swg-blender-plugin log -1` — provenance commit `f803f58782e675e85844960fe868b0849405249a`.
- `uv --version` (0.11.7), `python --version` (3.14.4) — local environment.

### Secondary (MEDIUM confidence)
- docs.astral.sh/uv/concepts/projects/init — `uv init --lib` layout + pytest dev-dep + `uv run pytest` (WebFetch).

### Tertiary (LOW confidence)
- (none — all load-bearing claims verified against primary sources this session.)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — uv/python verified locally; parser imports grepped; zero-dep confirmed.
- Architecture / engine semantics: HIGH — every precedence/tombstone/canonicalization claim quoted from the actual C++ this session, including the SearchTree-vs-SearchTOC tombstone distinction CONTEXT missed.
- cfg grammar: HIGH — read the live `[SharedFile]` block and the C++ that produces the key names.
- Pitfalls: HIGH — each derived from a verified source fact.
- Test-data byte layouts: HIGH — Python struct formats cross-checked against C++ structs.

**Research date:** 2026-06-14
**Valid until:** ~2026-07-14 (stable; the only fast-moving item is uv/pytest versions, pinned by lockfile at scaffold time)
