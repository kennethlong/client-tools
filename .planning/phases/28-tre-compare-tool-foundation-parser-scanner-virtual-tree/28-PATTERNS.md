# Phase 28: TRE Compare Tool — Foundation (Parser + Scanner + Virtual Tree) - Pattern Map

**Mapped:** 2026-06-14
**Files analyzed:** 13 (scaffold + 4 source modules + 6 test files + 2 config)
**Analogs found:** 3 strong / 13 (this is a greenfield, fully-isolated package — most files have NO in-repo analog by design)

## Orientation (read before using this map)

This phase is a **brand-new, fully isolated Python package** at `tools/tre-compare/` with a HARD
constraint of ZERO imports from the engine repo (D-01) and no coupling to the engine build graph
(D-02). The directory must be copy-paste extractable. That constraint shapes every analog choice:

- **PRIMARY source-of-truth (vendor-copy, then own):** `D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py`
  + `tre_decrypt.py`. These are copied IN and owned (D-03). Their structure is the template for the
  vendored `parser/` subpackage. They are NOT engine-repo files, so vendoring them does NOT violate
  D-01. [VERIFIED this session: both stdlib-only; API matches RESEARCH §Code Examples exactly.]
- **SEMANTIC reference to MIRROR in Python (NOT a code-copy analog):**
  `src/engine/shared/library/sharedFile/src/shared/TreeFile.cpp` and `TreeFile_SearchNode.cpp`.
  These define precedence ordering, per-node-type tombstone rules, and `fixUpFileName`. RESEARCH
  already quoted the exact load-bearing logic (Patterns 1–4). The Python files reimplement these
  semantics — they do NOT import or link engine code. **Do not copy C++ structure; copy behavior.**
- **In-repo Python precedent (one file):** `tools/dpvs-profile/analysis.py` — a stdlib-only,
  argparse, docstring-with-decision-ID-trail module. Useful as a *house-style* analog for module
  docstrings and stdlib discipline, NOT for domain structure. The new package is a `src/`-layout
  uv library, not a loose script, so this is a weak/partial analog only.

**Net:** the genuinely-new code (scanner, virtual-tree builder, fixtures) has no in-repo analog.
Its reference is the RESEARCH-quoted C++ semantics + the vendored parser's data model.

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|-------------------|------|-----------|----------------|---------------|
| `tools/tre-compare/pyproject.toml` | config | — | (none in repo; uv-generated) | no analog |
| `tools/tre-compare/uv.lock` | config | — | (none; uv-generated) | no analog |
| `tools/tre-compare/.gitignore` | config | — | (none; repo .gitignore has no Python entries) | no analog |
| `tools/tre-compare/README.md` | config/docs | — | (none) | no analog |
| `tools/tre-compare/src/tre_compare/__init__.py` | package init | — | swg_pipeline package layout | partial |
| `.../parser/tre_reader.py` | parser (vendored) | file-I/O / transform | `swg_pipeline/tre_reader.py` (the source) | vendor-copy (exact) |
| `.../parser/tre_decrypt.py` | parser (vendored) | transform | `swg_pipeline/tre_decrypt.py` (the source) | vendor-copy (exact) |
| `.../parser/__init__.py` | package init | — | swg_pipeline layout | partial |
| `.../scanner.py` | service/parser | transform (cfg→nodes) | `TreeFile.cpp` install loop (semantic mirror) | semantic-mirror, no code analog |
| `.../virtual_tree.py` | service | transform / merge | `TreeFile.cpp` + `TreeFile_SearchNode.cpp` (semantic mirror) | semantic-mirror, no code analog |
| `tests/conftest.py` | test (fixtures) | file-I/O (byte-build) | parser struct layouts (RESEARCH §Code Examples) | no analog (new) |
| `tests/test_parser.py` | test | request-response | `swg_pipeline/tre_list.py` (consumer usage) | partial |
| `tests/test_scanner.py` | test | request-response | (none) | no analog (new) |
| `tests/test_virtual_tree.py` | test | request-response | C++ behaviors (Patterns 1–4) | semantic-mirror, no code analog |
| `tests/test_integration.py` | test (gated) | file-I/O | `swg_pipeline/sample_tre.py` env-gate (`SWG_SAMPLE_TRE_DIR`) | partial |

## Pattern Assignments

### `.../parser/tre_reader.py` + `.../parser/tre_decrypt.py` (vendored parser)

**Source (vendor-copy, then own — D-03):** `D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py`
and `.../tre_decrypt.py`. Provenance commit `f803f58782e675e85844960fe868b0849405249a` (master,
2026-06-14). [VERIFIED this session.]

**Copy verbatim, with exactly TWO edits per file:**

1. **Provenance header** (prepend to each file) — RESEARCH §Pattern 5:
```python
# Vendored 2026-06-14 from swg-blender-plugin
#   source: D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py
#   commit: f803f58782e675e85844960fe868b0849405249a (master)
# Copied per Phase-28 D-03; owned and may diverge. No live link back.
```

2. **Rewrite the one cross-package import** at `tre_reader.py:251` (the ONLY mandatory code edit).
   [VERIFIED this session: line 251 is `from swg_pipeline.tre_decrypt import try_read_tre_payload`,
   an in-function import inside `read_tre_payload`]:
```python
# BEFORE (line 251, inside read_tre_payload):
                from swg_pipeline.tre_decrypt import try_read_tre_payload
# AFTER (point at the vendored sibling module):
                from .tre_decrypt import try_read_tre_payload
```

**Imports pattern to preserve (stdlib-only — keeps zero-runtime-dep guarantee)** [VERIFIED
`tre_reader.py:10-17`]:
```python
from __future__ import annotations
import shutil
import struct
import zlib
from dataclasses import dataclass
from enum import Enum
from pathlib import Path
```
`tre_decrypt.py` imports only `zlib`, `dataclasses.dataclass`, `from __future__ import annotations`.
Do NOT add any third-party import — that would break D-01 extractability.

**Public API the rest of the package imports** (do not rename on vendor; downstream Phase 29 expects
these) [VERIFIED `tre_reader.py` def lines this session]:
```python
read_tre_header(path)         -> TreHeader            # :159
read_tre_entries(path)        -> list[TreEntry]       # :195
list_tre(path)                -> list[str]            # :232
read_tre_payload(path, name)  -> bytes                # :236 (drill-in; Phase 29 only)
detect_master_index_kind(path)-> MasterIndexKind      # :148
parse_master_index(path)      -> dict                 # :454 (auto-routes COT2000 vs SearchTOC)
read_cot2000_entries(path)    -> list[Cot2000Entry]   # :295
read_search_toc_entries(path) -> list[SearchTocEntry] # :368
toc_entry_stride(version)     -> int                  # :138 (24 retail/5000, 32 extended)
# Exceptions: UnsupportedTreVersionError, TreFormatError
# SUPPORTED_TRE_VERSIONS = {"0004","0005","6000","0006","5000"}
```

**Entry dataclass shape (frozen) — both `length` AND `compressed_length` present** [VERIFIED
`tre_reader.py:72-105`; preserve verbatim for Phase-29 changed-detection forward-compat]:
```python
@dataclass(frozen=True)
class TreEntry:        path; crc; length; offset; compressor; compressed_length
@dataclass(frozen=True)
class Cot2000Entry:    path; tre_file; tree_index; crc; offset; length; compressor; compressed_length
@dataclass(frozen=True)
class SearchTocEntry:  path; tre_file; tree_index; crc; offset; length; compressor; compressed_length
```
**Flag for planner:** attribute is snake_case `compressed_length`, NOT camelCase `compressedLength`
as CONTEXT §specifics wrote. Phase-29 code must use the snake_case name.

---

### `.../scanner.py` (service/parser — cfg → priority-ordered node list)

**No in-repo Python code analog.** Semantic reference: the C++ `TreeFile::install` key-build loop
in `TreeFile.cpp` (RESEARCH-quoted). House-style analog: `tools/dpvs-profile/analysis.py`
(stdlib-only + argparse + decision-ID docstring). Do NOT import or mirror engine C++ code.

**Key grammar to parse** (hand-written; NOT `configparser` — Pitfall 4) [VERIFIED live
`stage/client.cfg` §`[SharedFile]`, RESEARCH §Code Examples]:
```ini
[SharedFile]
	maxSearchPriority=12
	TOCTreePath="D:/Code/SWGSource Client v3.0/"
	searchTOC_00_0="D:/Code/SWGSource Client v3.0/sku0_client.toc"
	searchTree_00_7="D:/Code/SWGSource Client v3.0/disable_wayfar_dearic_snow.tre"
	searchPath_00_10="D:/Code/swg-client-v2/stage/override"
```

**Parsing rules to honor** (each VERIFIED in RESEARCH):
- Section header `[SharedFile]`; TAB-indented lines; `#` comment lines; values may be double-quoted
  with embedded spaces — strip surrounding quotes. (Pitfall 4)
- Key shape: `search{TOC|Tree|Path}_<NN>_<P>` where `NN` = SKU number (`sprintf("_%02d_", sku)`)
  and `P` = priority. Parse priority from the FINAL `_<int>` suffix; retain the full key for
  provenance. SKU gating out of scope (live cfg is all SKU 00). (Pitfall 5, Open Q2)
- Repeated/indexed keys are legal — the same key name can appear multiple times (engine reads via
  `getKeyString(..., index, ...)` loop). This is WHY `configparser` fails. (Pitfall 4)
- `maxSearchPriority` and `TOCTreePath` are scalar settings; `TOCTreePath` is the dir prefix
  prepended to `.tre` names found INSIDE `.toc` files (must keep trailing slash).
- **D-08:** cfg path is an argument/parameter — do NOT hardcode `stage/client.cfg`.

**Output: priority-ordered node descriptors** — `kind ∈ {tree, toc, path}`, `priority:int`,
`abspath:str`. **Sort HIGHER priority number FIRST** with a STABLE sort (Python's sort is stable,
preserving config insertion order within a tie) — mirrors `searchNodePriorityOrder`
[VERIFIED `TreeFile.cpp:285`, Pattern 1 + Pitfall 2]:
```python
# Engine: bool searchNodePriorityOrder(a, b) { return a->getPriority() > b->getPriority(); }
# Python mirror:
nodes = sorted(parsed_nodes, key=lambda n: -n.priority)   # stable → ties keep cfg order
```
Expected order for the live cfg: `path(10) → tree(8) → tree(7) → toc(3) → toc(2) → toc(1) → toc(0)`.

---

### `.../virtual_tree.py` (service — priority merge + tombstone + canonicalization)

**No in-repo Python code analog.** Semantic reference: `TreeFile.cpp` (`find()` first-match-wins,
`fixUpFileName`) + `TreeFile_SearchNode.cpp` (per-node-type tombstone). All quoted in RESEARCH
Patterns 2–4. Reimplement behavior in pure Python; do NOT import engine code.

**`fix_up_file_name` — port verbatim, run BEFORE any path compare/merge** [VERIFIED
`TreeFile.cpp:511-601`, Pattern 3]:
```python
def fix_up_file_name(name: str) -> str:
    f = name
    while f[:1] in ("\\", "/"):           # 1. strip leading slashes
        f = f[1:]
    while f[:2] in ("./", ".\\"):         # 2. strip leading ./
        f = f[2:]
    while f[:3] in ("../", "..\\"):       # 3. strip leading ../
        f = f[3:]
    out: list[str] = []
    prev_slash = False
    for c in f:                           # 4. \->/ , lowercase, 5. collapse repeats
        if c in ("\\", "/"):
            cur_slash = True; ch = "/"
        else:
            cur_slash = False; ch = c.lower()
        if not cur_slash or not prev_slash:
            out.append(ch); prev_slash = cur_slash
    return "".join(out)
```
Test vectors (Pitfall 3): `\\foo\\\\Bar/baz.dds` → `foo/bar/baz.dds`; `./a/b` → `a/b`;
`../x` → `x`; mixed-case → lowercase.

**Merge loop — first-match-wins, key on canonical PATH (not CRC)** [VERIFIED `TreeFile.cpp:437-461`,
Pattern 2 + anti-pattern "Re-deriving CRC"]:
```
for node in priority_order:           # higher priority first
    for entry in node.entries:        # tree/toc nodes: parser entries; path nodes: os.walk (see below)
        key = fix_up_file_name(entry.path)
        if key already claimed or tombstoned: skip          # 1st (highest) wins
        else: record winner = (node, entry); append lower copies to shadowed list
```
**The merge key is `fix_up_file_name(path)` — NOT the TOC `crc` field** (crc is a path-CRC for the
engine's binary search, not a content hash). Synthetic fixtures may use placeholder CRCs.

**Tombstone — PER-NODE-TYPE, NOT a blanket rule** (the single most important correctness nuance;
CONTEXT under-specified it) [VERIFIED `TreeFile_SearchNode.cpp:397` (tree) and `:807,:831` (toc),
Pattern 4 + Pitfall 1]:
- **tree node, `length == 0` → TRUE tombstone:** sets `deleted`, search STOPS, file removed
  globally (lower-priority copies never served). This is the override-delete mechanism.
- **toc node, `length == 0` (or `offset == 0`) → NOT a tombstone:** "absent here, keep searching";
  does NOT shadow a real lower-priority file. SearchTOC always leaves `deleted = false`.
- **path node (dir):** no tombstone concept — filesystem existence only.

```python
# WRONG (blanket): if entry.length == 0: tombstone(key)
# RIGHT (per-node-type):
if node.kind == "tree" and entry.length == 0:
    tombstone[key] = True          # remove globally; later nodes skip this key
    continue
if node.kind == "toc" and (entry.length == 0 or entry.offset == 0):
    continue                       # just absent here; do NOT tombstone
```

**searchPath (loose dir) handling — eager walk (Open Question 1, planner-confirm).** The engine
answers `SearchPath::exists()` lazily via the filesystem [VERIFIED `TreeFile_SearchNode.cpp:109`].
A static virtual tree needs concrete files, so RESEARCH recommends `os.walk` the dir and inject each
file at the node's (highest) priority, applying `fix_up_file_name` to the dir-relative path. This is
the ONE place the static model adds behavior the lazy engine doesn't — flag in the plan and unit-test
it (synthetic override dir beating a base TRE).

**Output:** `{ canonical_path -> WinningEntry(+shadowed copies list) }` — a clean importable object
Phase 29's diff engine consumes without refactor.

---

### `tests/conftest.py` + `tests/test_*.py` (test fixtures + suites)

**No in-repo pytest analog** (no `conftest.py` or `pyproject.toml` exists anywhere in the repo —
[VERIFIED: Glob returned none]). House-style reference for stdlib discipline + decision-ID docstrings:
`tools/dpvs-profile/analysis.py`. Consumer-usage reference for the parser API:
`swg_pipeline/tre_list.py`. Env-gate reference for the integration test: `swg_pipeline/sample_tre.py`
uses `SWG_SAMPLE_TRE_DIR`.

**Byte-built synthetic fixtures (D-06; programmatic builder recommended)** — exact struct layouts to
encode [VERIFIED `tre_reader.py` formats cross-checked vs C++ structs, RESEARCH §Code Examples]:
```python
# TRE header (36 bytes): STANDARD_HEADER_FMT = "<4s4s7I"
#   token('TREE'), version('0004'..), numberOfFiles, tocOffset, tocCompressor,
#   sizeOfTOC, blockCompressor, sizeOfNameBlock, uncompSizeOfNameBlock
# TRE TOC entry (24 bytes retail / 32 extended): TOC_ENTRY_FMT = "<Iiiiii"
#   crc:u32, length:i32, offset:i32, compressor:i32, compressedLength:i32, fileNameOffset:i32
# SearchTOC header: SEARCH_TOC_HEADER_FMT = "<4s4sBBBB6I"
#   token('TOC '), version('0001'), tocCompressor:u8, nameBlockCompressor:u8, 2x unused:u8,
#   numberOfFiles, sizeOfTOC, sizeOfNameBlock, uncompSizeOfNameBlock,
#   numberOfTreeFiles, sizeOfTreeFileNameBlock
```
Smallest faithful TRE = header(36) + N×24-byte TOC entries + null-terminated name block. Payloads
may be empty and CRCs may be placeholders — the Phase-28 merge reads TOC metadata and keys on the
path STRING, never the payload or CRC (Assumption A3). Entries need not be CRC-sorted for the merge.

**Synthetic corpus MUST exercise (D-06):** each supported version tag (0004/0005/6000/0006/5000);
override shadowing (first-hit-wins across priorities); SearchTree length-0 tombstone removal;
SearchTOC length-0 NON-tombstone (lower copy still served — the Pitfall-1 guard test);
`fix_up_file_name` canonicalization.

**Integration test (D-07) — env/marker-gated, default-OFF:**
```python
# pyproject.toml: [tool.pytest.ini_options] markers = ["integration: real-install test (env-gated)"]
import os, pytest
@pytest.mark.integration
@pytest.mark.skipif(not os.environ.get("TRE_COMPARE_INTEGRATION"),
                    reason="set TRE_COMPARE_INTEGRATION=1 + real install present")
def test_real_client_cfg(): ...   # scans stage/client.cfg → D:/Code/SWGSource Client v3.0/
```
Run modes: `uv run pytest -m "not integration"` (default/CI), `uv run pytest -m integration` (opt-in).

---

### `pyproject.toml`, `uv.lock`, `.gitignore`, `__init__.py` files (scaffold)

**No in-repo analog** (greenfield uv package). Generate via `uv init --lib tools/tre-compare`, then
`uv add --dev pytest` [CITED: docs.astral.sh/uv]. Generated `pyproject.toml` shape (RESEARCH):
```toml
[project]
name = "tre-compare"
version = "0.1.0"
requires-python = ">=3.11"
dependencies = []                  # ZERO runtime deps in Phase 28 (D-01 extractability)
[build-system]
requires = ["uv_build>=0.11.21,<0.12"]
build-backend = "uv_build"
[tool.pytest.ini_options]
markers = ["integration: real-install test (env-gated)"]
```
`tools/tre-compare/.gitignore` MUST be added (repo `.gitignore` has NO Python entries — Pitfall 6):
`.venv/`, `__pycache__/`, `*.pyc`. Commit `uv.lock` (reproducibility, not an artifact).

## Shared Patterns

### Stdlib-only / zero-runtime-dep discipline (D-01 extractability)
**Source:** vendored `tre_reader.py`/`tre_decrypt.py` (stdlib-only) + house precedent
`tools/dpvs-profile/analysis.py` (also stdlib-only).
**Apply to:** every source module in the package. No third-party runtime import in Phase 28
(`xxhash`/`fastapi`/`sqlite` arrive in Phase 29). pytest is the ONLY dev dep.

### Module docstring with decision-ID trail
**Source:** `tools/dpvs-profile/analysis.py:1-21` — opens with a docstring that cites the locking
CONTEXT decision IDs (D-04, D-09…).
**Apply to:** `scanner.py`, `virtual_tree.py`, `conftest.py` — cite the D-NN decisions and the
RESEARCH pattern numbers each module realizes (e.g. virtual_tree.py → "D-03/Pattern 2,3,4").

### Engine-faithful semantics (mirror, never import)
**Source (semantic only):** `TreeFile.cpp` Patterns 1–3, `TreeFile_SearchNode.cpp` Pattern 4 —
all quoted in `28-RESEARCH.md`.
**Apply to:** `scanner.py` (priority sort) and `virtual_tree.py` (first-match-wins, fixUpFileName,
per-node-type tombstone). These are the correctness contract. Add an explicit unit test per pattern.
**Do NOT** `#include`, import, or code-copy the C++ — reimplement behavior in pure Python.

### Vendoring with provenance (D-03)
**Source:** RESEARCH §Pattern 5 header template + provenance commit `f803f587…`.
**Apply to:** both vendored parser files. Plus the single mandatory import rewrite at
`tre_reader.py:251` (`swg_pipeline.tre_decrypt` → `.tre_decrypt`).

## No Analog Found

Files with no close in-repo match — planner should use the vendored parser data model and the
RESEARCH-quoted C++ semantics as the reference instead (this is expected: the package is isolated
by design, D-01):

| File | Role | Data Flow | Reference to use instead |
|------|------|-----------|--------------------------|
| `scanner.py` | service/parser | transform | C++ `TreeFile::install` key loop (RESEARCH Patterns 1, Pitfalls 4/5); live `stage/client.cfg` grammar |
| `virtual_tree.py` | service | merge | C++ `find()`/`fixUpFileName`/tombstone (RESEARCH Patterns 2–4) |
| `tests/conftest.py` | test fixtures | byte-build | Parser struct layouts (RESEARCH §Code Examples / `tre_reader.py` formats) |
| `tests/test_scanner.py` | test | request-response | scanner output contract above |
| `tests/test_virtual_tree.py` | test | request-response | C++ behaviors (Patterns 1–4) |
| `pyproject.toml` / `uv.lock` / `.gitignore` | config | — | `uv init --lib` output + RESEARCH scaffold block |

## Metadata

**Analog search scope:** `tools/**/*.py`, `**/pyproject.toml`, `**/conftest.py` (repo); vendored
source `D:/Code/swg-blender-plugin/swg_pipeline/`; engine semantics in
`src/engine/shared/library/sharedFile/src/shared/` (read via RESEARCH, spot-verified this session).
**Files scanned this session:** `tre_reader.py` (header + def index + line 251 import),
`tools/dpvs-profile/analysis.py` (docstring/style); rest leveraged from verified `28-RESEARCH.md`.
**In-repo Python analogs found:** 1 weak house-style (`dpvs-profile/analysis.py`); 0 pytest/uv analogs.
**Pattern extraction date:** 2026-06-14
