# Architecture Research

**Domain:** Standalone local web app — cross-installation TRE archive diff tool for the whitengold SWG client (v2.3 Hardening milestone)
**Researched:** 2026-06-12
**Confidence:** HIGH (engine semantics + format verified from source; existing reusable parser confirmed on disk)

> Scope note: this file covers ONLY the one new component in v2.3 — the **web-based TRE compare tool**. The other six hardening items (DPVS config-gate, instrumentation removal, machine portability, Options FATAL, D3DCompile port, corner-snap fix) live inside the existing C++ client and need no architecture research. (Supersedes the stale v2.2 asset-PS-pipeline ARCHITECTURE.md previously at this path.)

---

## Decisive finding up front

**A complete, tested, version-aware Python TRE/TOC parser already exists** at
`D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py` (471 lines). It already handles:

- `.tre` archives (`TAG_TREE`, versions `0004`/`0005` 24-byte TOC, plus extended `6000`/`0006`/`5000` 32-byte TOC)
- `SearchTOC` master indexes (`TAG_TOC`/`0001`) — **this is exactly what `client.cfg` uses** (`searchTOC_00_0=...sku0_client.toc`)
- `COT2000` master indexes (SwgRestoration/SWGSource variant)
- zlib block decompression, per-entry CRC/offset/length, name-block resolution

This single fact drives the whole recommendation: **the backend is Python, and it reuses `tre_reader.py` rather than re-implementing the format.** Re-implementing in JS/TS/Rust would re-derive a solved, tested problem and lose the COT2000/extended-version coverage. The only piece the parser does NOT yet expose is the search-path *merge/resolution* layer — that's the genuinely new code, and it must mirror the engine, documented below.

---

## Engine search-path semantics (verified from source — this IS the spec)

Source: `src/engine/shared/library/sharedFile/src/shared/TreeFile.cpp` and `TreeFile_SearchNode.cpp`.

The virtual-tree merge must reproduce this exactly or the diff lies about what the client actually loads.

### How the node list is built (`TreeFile::install`, TreeFile.cpp:84-191)

1. Config keys are read from the `[SharedFile]` section of `client.cfg`. `maxSearchPriority` (default 20; our cfg uses 12) bounds the priority loop.
2. For each `priority` from `0 .. maxPriority`, and for each multi-value index, four node kinds are added:
   - `searchPath<sku>%d` → `SearchPath` (a loose directory on disk)
   - `searchTree<sku>%d` → `SearchTree` (a single `.tre`)
   - `searchTOC<sku>%d`  → `SearchTOC` (a master index naming many `.tre`s)
   - (plus `searchAbsolute` and `searchCache` appended after — irrelevant to asset diffing)
3. The optional `_NN_` sku infix (e.g. `searchTOC_00_0`) is for multi-sku accounts; the **trailing integer is the priority**. So `searchTOC_00_0` = priority 0, `searchTree_00_8` = priority 8, `searchPath_00_10` = priority 10.

### The resolution rule (THE critical semantic)

- Nodes are stored **sorted by priority, HIGHEST first** (`searchNodePriorityOrder`: `a->getPriority() > b->getPriority()`, TreeFile.cpp:285-288). New same-priority nodes insert **after** the last equal-priority node (`std::lower_bound`, TreeFile.cpp:299-308) → **stable, insertion-order within a priority.**
- `TreeFile::find` / `TreeFile::open` walk the node list **front-to-back (highest priority → lowest)** and return the **FIRST node that contains the file** (TreeFile.cpp:437-461, 661-765). **First hit wins. Higher priority shadows lower.**
- Therefore: a file present in `searchPath_00_10` (priority 10) **overrides** the same logical path inside `searchTOC_00_0` (priority 0). This is exactly the `stage/override/` shader-override mechanism the project already relies on. **The compare tool's "effective file" for any path = the entry from the highest-priority node that contains it.**

### Filename normalization (`fixUpFileName`, TreeFile.cpp:511-617) — must replicate

Before any lookup the engine canonicalizes the path:
- strip leading `\`, `/`, `./`, `../`
- backslashes → forward slashes
- **lowercase everything** (`tolower`)
- collapse repeated slashes

The merge key MUST be this canonical form, or two installs that differ only in case/slashes will show phantom diffs. `tre_reader.py` already lowercases for its `needle` comparison in `read_tre_payload`; the merge layer must apply the same rule to every key.

### Lookup mechanics inside an archive (`SearchTree::localExists`, :360-408)

- Each TOC is sorted by `crc` then by case-insensitive filename; lookup is a **binary search on `Crc::calculate(fileName)`**, tie-broken by `_stricmp`.
- **CRC is non-standard.** `Crc::calculate` (sharedFoundation/Crc.cpp) uses polynomial `0x04C11DB7` applied **MSB-first / non-reflected**, init `0xFFFFFFFF`, final XOR `0xFFFFFFFF` — i.e. CRC-32/MPEG-2 lineage, **NOT** the reflected zlib/PNG CRC-32. The tool does **not** need to recompute CRCs for normal diffing (the stored per-entry `crc` is read straight out of the TOC and is a free pre-hash), but anyone who synthesizes/validates a CRC must use this variant. Flag for any contributor who reaches for `binascii.crc32`.
- A "deleted" entry has `length == 0` (`SearchTree`) — a tombstone that *stops the search* (sets `deleted=true`, aborts the `find` loop). The merge must treat a length-0 entry as "this path is intentionally removed at this priority," shadowing lower-priority copies. This is a real diff signal, not a no-op.

---

## Standard Architecture

A local single-user diagnostic tool. The right shape is the **smallest thing that works**: a single Python process serving a static SPA, with all heavy lifting (parse + merge + diff) done server-side because the TRE data lives on the local filesystem and the parser is Python.

### System Overview

```
┌──────────────────────────────────────────────────────────────────┐
│  Browser (localhost)  —  thin SPA                                  │
│  ┌────────────┐ ┌──────────────┐ ┌──────────────┐ ┌────────────┐ │
│  │ Install    │ │ Archive-set  │ │ Virtual-tree │ │ File-detail│ │
│  │ picker     │ │ delta view   │ │ diff (search │ │ /hex/text  │ │
│  │ (2 cfgs)   │ │ (TRE × TRE)  │ │ + filter)    │ │ compare    │ │
│  └─────┬──────┘ └──────┬───────┘ └──────┬───────┘ └─────┬──────┘ │
└────────┼───────────────┼────────────────┼───────────────┼────────┘
         │  HTTP/JSON (localhost only)     │               │
┌────────┴───────────────┴────────────────┴───────────────┴────────┐
│  Python backend (FastAPI/Flask, single process)                   │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │ Web API layer  (REST + paginated/lazy file listing)         │ │
│  ├─────────────────────────────────────────────────────────────┤ │
│  │ Diff engine    (set-delta + per-file added/removed/changed) │ │
│  ├──────────────────────────┬──────────────────────────────────┤ │
│  │ Virtual-tree builder      │  Installation scanner            │ │
│  │ (priority merge = engine  │  (parse client.cfg [SharedFile], │ │
│  │  first-hit-wins rule)     │   enumerate searchTOC/Tree/Path) │ │
│  ├──────────────────────────┴──────────────────────────────────┤ │
│  │ TRE/TOC parser  ==  REUSE swg_pipeline/tre_reader.py         │ │
│  ├─────────────────────────────────────────────────────────────┤ │
│  │ Index cache    (on-disk sqlite/JSON keyed by file mtime+size)│ │
│  └─────────────────────────────────────────────────────────────┘ │
└───────────────────────────────────────────────────────────────────┘
         │ reads (seek-read / mmap, never full-load)
┌────────┴──────────────────────────────────────────────────────────┐
│  Two SWG installations on disk (each: client.cfg + *.toc + *.tre)  │
└───────────────────────────────────────────────────────────────────┘
```

### Component Responsibilities

| Component | Responsibility | Typical Implementation |
|-----------|----------------|------------------------|
| **TRE/TOC parser** | Read `.tre`, `SearchTOC`, `COT2000`; yield per-entry (path, crc, length, offset, compressor, compressed_length, source-tre) | **Reuse `swg_pipeline/tre_reader.py` as-is** — do not rewrite |
| **Installation scanner** | Parse a `client.cfg` `[SharedFile]` section, expand `searchTOC/Tree/Path_<sku>_<prio>` keys, resolve to a priority-ordered list of archive/dir nodes | New: ~150-line cfg reader (duplicate-key INI → hand-parse the section, not stdlib `configparser`) |
| **Virtual-tree builder** | Apply the engine's first-hit-wins priority merge → ONE map: canonical-path → winning entry (+ shadowed list) | New: the core "mirror the client" code; deterministic, pure |
| **Diff engine** | (a) set-level: which archives exist in A vs B; (b) file-level: added / removed / changed / unchanged across the two merged trees | New: dict-vs-dict over merged maps; change = size delta or content-hash mismatch |
| **Index cache** | Persist parsed TOC indices keyed by `(abspath, mtime, size)` so re-runs are instant; persist content hashes lazily | New: sqlite or JSON sidecar in an app cache dir |
| **Web API** | Serve merged-tree/diff JSON with pagination + lazy file-content fetch | FastAPI (preferred) or Flask |
| **Frontend SPA** | Install picker, set-delta table, filterable virtual-tree diff, on-demand file-detail compare | Vite + a light framework (React/Svelte) or vanilla — single-user, no SSR |

---

## Recommended Project Structure

### Where it lives — recommendation: **`D:/Code/swg-client-v2/tools/tre-compare/`** (in-repo)

| Option | Pros | Cons | Verdict |
|--------|------|------|---------|
| **In swg-client-v2 `tools/tre-compare/`** | Co-located with the authoritative `TreeFile.cpp` semantics it mirrors; the project's own `stage/override` use-case is the first consumer; one repo to clone; CLAUDE.md/memory context already here | Adds a Python+JS sub-tree to a C++ repo; needs a clear "not built by swg.sln" boundary | **RECOMMENDED** |
| In `swg-blender-plugin` (where the parser is) | `tre_reader.py` already there → zero import friction | That repo is a Blender asset pipeline; a web app is off-mission; the *consumer* (whitengold client diffs) lives in swg-client-v2; couples release cadence to Blender tooling | Reject |
| New standalone repo | Clean separation; independent CI | Splits the parser source-of-truth from its only reuse site; more clone/setup overhead for a hobby tool; loses co-location with the engine spec | Reject (over-engineering for a single-user diagnostic) |

**Reuse mechanism without forking the parser:** either `pip install -e D:/Code/swg-blender-plugin` (editable path-dependency, `from swg_pipeline.tre_reader import ...`) **or vendor just `tre_reader.py` + `tre_decrypt.py`** (pure `struct`/`zlib`, no Blender imports) into `tools/tre-compare/backend/tre/`. **Prefer vendoring:** it pins the format reader, keeps the tool self-contained, avoids dragging a Blender Python env, and the files are small and stable.

```
swg-client-v2/
└── tools/
    └── tre-compare/                 # NOT part of swg.sln; standalone
        ├── README.md                # run instructions; "not built by MSBuild"
        ├── pyproject.toml           # backend deps (fastapi, uvicorn); pinned
        ├── backend/
        │   ├── tre/                 # vendored pure-Python format readers
        │   │   ├── tre_reader.py    #   copied from swg-blender-plugin (no Blender deps)
        │   │   └── tre_decrypt.py   #   "
        │   ├── scanner.py           # parse client.cfg [SharedFile] -> node list
        │   ├── vtree.py             # engine first-hit-wins merge  (THE mirror code)
        │   ├── diff.py              # set-delta + file-level diff
        │   ├── cache.py             # sqlite index cache (mtime+size keyed)
        │   ├── api.py               # FastAPI routes
        │   └── app.py               # entrypoint: serve API + static frontend
        ├── frontend/                # Vite SPA (built to backend/static/)
        │   ├── src/
        │   │   ├── views/           # InstallPicker, SetDelta, TreeDiff, FileDetail
        │   │   ├── api.ts           # fetch wrappers
        │   │   └── main.ts
        │   └── vite.config.ts
        └── tests/
            ├── test_scanner.py      # cfg priority parsing  (use stage/client.cfg!)
            ├── test_vtree.py        # first-hit-wins, tombstone (length==0), case-fold
            └── test_diff.py
```

### Structure Rationale

- **`backend/tre/` vendored, not imported from the Blender repo:** the tool runs without a Blender Python env; the readers are dependency-free.
- **`vtree.py` is its own module with its own tests:** the one place where correctness == "matches the C++ client." Its tests assert against `stage/client.cfg`'s real layout (priority-10 override shadows priority-0 TOC).
- **Frontend built into `backend/static/`:** one process serves everything on `localhost`; no CORS, no separate dev server in production use.

---

## Architectural Patterns

### Pattern 1: Mirror-the-engine merge (single source of truth = `TreeFile.cpp`)

**What:** `vtree.py` reproduces `TreeFile::install` ordering + `TreeFile::find` first-hit-wins. Build a list of `(priority, insertion_index, node)`, sort by `(-priority, insertion_index)`, then fold entries: first canonical-path wins; a length-0 entry is a tombstone that finalizes the path as "removed."
**When to use:** Always — this is the tool's reason to exist over a naive "list all files in all TREs."
**Trade-offs:** Must track the engine if it ever changes (low risk — frozen archive code). Cite `TreeFile.cpp:437-461` in the docstring so the link is explicit.

```python
# canonical key per engine fixUpFileName: lowercase, '/'-slashes, no leading ./ or //
def canon(p: str) -> str:
    p = p.replace("\\", "/").lstrip("/").lower()
    while "//" in p: p = p.replace("//", "/")
    return p

# nodes already sorted highest-priority-first, stable within priority
def build_vtree(nodes):
    winning = {}                       # canon path -> entry
    for node in nodes:                 # high prio -> low prio
        for e in node.entries():
            k = canon(e.path)
            if k in winning:           # already shadowed by higher priority
                continue
            if e.length == 0:          # tombstone: removed at this priority
                winning[k] = TOMBSTONE
                continue
            winning[k] = e
    return {k: v for k, v in winning.items() if v is not TOMBSTONE}
```

### Pattern 2: Two-tier diff — cheap metadata first, content hash only on demand

**What:** Compare merged trees by **(presence, length, stored TOC crc)** first. CRC comes free from the TOC — use it as the first-line "changed?" signal. Only files that are present-in-both-but-you-want-byte-certainty get a real content hash / decompress.
**When to use:** Always at this scale (thousands of files × hundreds of archives).
**Trade-offs:** The stored CRC is over the *uncompressed logical content* — a strong but not cryptographic equality signal; collisions are astronomically unlikely for "did this asset change" diagnostics. For absolute certainty on a specific file, the file-detail view decompresses both sides and does a real byte/hash compare on click.

### Pattern 3: Eager index, lazy payload

**What:** On "compare," eagerly parse all TOCs of both installs (cheap — header + TOC tables only, no payload decompression) and build both merged trees + the diff summary. Decompress actual file *bytes* only when the user opens a file-detail view.
**When to use:** The load-time vs interactivity sweet spot for hundreds of archives.
**Trade-offs:** First compare of an un-cached install pays the TOC-parse cost once; cached thereafter.

---

## Data Flow

### Compare flow (two installations)

```
[User selects install A cfg + install B cfg]
        ↓
scanner.parse_cfg(A)  →  node list A (priority-ordered)   ┐  (eager)
scanner.parse_cfg(B)  →  node list B                       │
        ↓                                                  │
for each archive node:  cache.get_or_parse(tre/toc)        │  (eager, cached by mtime+size)
        ↓                                                  │
vtree.build(A) , vtree.build(B)   → merged maps            │
        ↓                                                  │
diff.compare(mergedA, mergedB):                            │
   set-delta:  archives only-in-A / only-in-B / common     │
   file-delta: added / removed / changed(len|crc) / same   ┘
        ↓
API returns summary + paginated file rows (NOT payloads)
        ↓
[User clicks a 'changed' file]
        ↓
api.file_detail(path):  decompress entry from A and from B   (lazy, on demand)
   → return size/crc + text-or-hex preview + optional byte diff
```

### What is eager vs lazy vs cached

| Work | When | Where stored |
|------|------|--------------|
| Parse `client.cfg` `[SharedFile]` | eager (per compare) | in-memory |
| Parse TOC/TRE headers + TOC tables (path, crc, len, offset) | eager | **on-disk index cache**, keyed `(abspath, mtime, size)` |
| Build merged virtual tree | eager (fast in-memory dict fold) | in-memory (optionally memoized per install signature) |
| Set-level + metadata file-level diff | eager | in-memory / returned JSON |
| **Decompress a file's actual bytes** | **lazy** (on file-detail open) | optional small LRU; not persisted |
| Content hash of a specific file (byte-exact confirm) | lazy (only when metadata ambiguous or user asks) | cache row, written on first compute |

### State management (frontend)

```
[Compare request: {cfgA, cfgB}]
        ↓
[summary store]  ← set-delta + counts
        ↓ (subscribe)
[TreeDiff view] —(filter/paginate)→ GET /diff/files?status=changed&page=N
        ↓ (row click)
[FileDetail view] → GET /file?path=...&side=both   (lazy fetch)
```

---

## Scaling Considerations

This is a **single-user local tool**; "scale" = dataset size (hundreds of archives, tens-of-thousands of paths), not concurrent users.

| Scale | Architecture adjustments |
|-------|--------------------------|
| 1 install pair, ~hundreds of TREs, ~tens of thousands of paths | In-memory dicts are fine; sub-second after cache warm. No DB strictly required, but sqlite index cache makes re-runs instant. |
| Very large master TOCs (100k+ entries) | Stream TOC parse (already cheap — no payload); keep merged map as `dict[str, entry]`; paginate file lists in the API (don't ship 50k rows to the browser at once). |
| Repeated compares across sessions | On-disk index cache keyed by `(path, mtime, size)` avoids re-parsing unchanged archives — the single biggest win. |

### Scaling priorities

1. **First bottleneck — re-parsing TOCs every run.** Fix: persistent index cache keyed by mtime+size (skip re-parse when unchanged).
2. **Second bottleneck — shipping the full file list to the browser.** Fix: server-side filter + pagination; the SPA requests only the current page/status bucket.
3. **Non-bottleneck (do NOT pre-optimize): payload decompression.** It's lazy/on-click; never decompress all files up front.

---

## Anti-Patterns

### Anti-Pattern 1: Re-implementing the TRE format in JS/TS/Rust

**What people do:** Write a fresh binary parser in the frontend stack "to keep it one language."
**Why it's wrong:** `tre_reader.py` already correctly handles `0004/0005/6000/0006/5000`, COT2000, **and** SearchTOC — the variant our `client.cfg` actually uses. Re-deriving offsets/compressor flags is error-prone (the COT2000 `fileNameOffset`-is-actually-a-length quirk at TreeFile_SearchNode.cpp:687-699 is a known trap) and throws away tested code.
**Do this instead:** Vendor the Python readers; backend is Python.

### Anti-Pattern 2: Naive "union of all files in all TREs" diff

**What people do:** List every entry across every archive and diff the raw sets.
**Why it's wrong:** Ignores priority shadowing. A file overridden by a higher-priority `searchPath` (the `stage/override` case) or tombstoned by a length-0 entry would be mis-reported. The diff would not reflect **what the client actually loads.**
**Do this instead:** Diff the **merged virtual trees** (first-hit-wins), and optionally surface shadowed copies as a secondary "also present in (lower-priority) X" annotation.

### Anti-Pattern 3: Trusting `configparser` for `client.cfg`

**What people do:** `configparser.read(cfg)` to get `[SharedFile]`.
**Why it's wrong:** The cfg has **duplicate keys differing only by index/priority suffix** (`searchTOC_00_0`, `searchTOC_01_1`, …); stdlib `configparser` mangles duplicate keys, and the priority is embedded in the key name and must be parsed out.
**Do this instead:** Hand-parse the `[SharedFile]` section, regex `search(TOC|Tree|Path|Cache|Absolute)(?:_\d+)?_(\d+)` to extract kind + priority, honor `maxSearchPriority`.

### Anti-Pattern 4: Using zlib/`binascii.crc32` for path CRCs

**What people do:** Assume SWG path CRCs are standard CRC-32.
**Why it's wrong:** The engine's `Crc::calculate` is MSB-first, non-reflected (poly `0x04C11DB7`, init/xorout `0xFFFFFFFF`) — CRC-32/MPEG-2 lineage, not zlib's reflected variant.
**Do this instead:** For diffing, just read the stored per-entry `crc` from the TOC (no recompute). If a CRC must ever be synthesized, implement the MSB-first variant explicitly and unit-test it against a known TOC entry.

---

## Integration Points

### External / sibling-repo

| Source | Integration pattern | Notes |
|--------|---------------------|-------|
| `swg-blender-plugin/swg_pipeline/tre_reader.py` (+ `tre_decrypt.py`) | **Vendor** (copy) into `backend/tre/`; or `pip install -e` the sibling repo | Readers are pure `struct`/`zlib`, no Blender deps → safe to vendor. Record the source-of-truth path + a version note in README. |
| `src/engine/.../sharedFile/TreeFile.cpp` (+ `TreeFile_SearchNode.cpp`, `Crc.cpp`) | **Spec reference, not code** | Merge / normalization / CRC semantics are copied from here; cite file:line in `vtree.py`/`scanner.py` docstrings. |
| A SWG install's `client.cfg` `[SharedFile]` | Read-only file parse | Use `stage/client.cfg` (real: `maxSearchPriority=12`, four `searchTOC`, two `searchTree`, one `searchPath` override) as the canonical test fixture. |
| The `.tre`/`.toc` archives themselves | Read-only, seek/`mmap` — **never full-load** | A master `.toc` names many `.tre`s by relative name; the scanner must resolve those relative to the `.toc`'s directory (mirrors the `SearchTOC` ctor's `TOCTreePath` + empty-path probe, TreeFile_SearchNode.cpp:616-656). |

### Internal boundaries

| Boundary | Communication | Notes |
|----------|---------------|-------|
| scanner ↔ vtree | scanner yields a priority-ordered node list; vtree consumes it | Keep scanner output a plain dataclass list so vtree is pure/testable |
| vtree ↔ diff | two merged `dict[path,entry]` maps in | diff is dict-vs-dict; no I/O |
| parser ↔ cache | cache wraps `read_*_entries` with mtime+size memoization | parser stays oblivious to caching |
| backend ↔ frontend | localhost REST/JSON; paginated file lists; lazy file-detail | no auth, no CORS (same origin), bind `127.0.0.1` only |

---

## Suggested Build Order (dependency-driven)

1. **Vendor + smoke-test the parser** (`backend/tre/`). Confirm it reads `stage/client.cfg`'s referenced `sku0_client.toc` and a `.tre`. *Unblocks everything; lowest risk (code already works).*
2. **Installation scanner** (`scanner.py`) — parse `client.cfg [SharedFile]` into a priority-ordered node list; resolve `.toc`→`.tre` relative paths. *Test against the real `stage/client.cfg`.*
3. **Virtual-tree builder** (`vtree.py`) — the engine first-hit-wins merge + tombstones + canonicalization. *The correctness keystone; unit-test priority shadowing (override @10 beats TOC @0) and length-0 tombstones BEFORE any UI.*
4. **Index cache** (`cache.py`) — mtime+size keyed persistence around the parser. *Optional for first run, but cheap and makes step 5+ iterate fast.*
5. **Diff engine** (`diff.py`) — set-delta + metadata file-level diff over two merged maps. *Pure; fully testable headless.*
6. **Web API** (`api.py`/`app.py`) — expose summary + paginated file rows + lazy file-detail; serve static frontend. *Now there's something to render.*
7. **Frontend SPA** — install picker → set-delta table → filterable tree-diff → file-detail compare. *Build views in the order the data flow produces them.*
8. **File-detail content compare** (lazy decompress + text/hex/byte-diff) — last, since it's the only part needing payload decompression.

Rationale: steps 1-5 are pure, headless, and unit-testable against the real `stage/client.cfg` + its archives; the entire "mirror the client" correctness story is locked before any web/UI code exists. Web + UI (6-8) are then a thin presentation layer over proven logic.

---

## Sources

- `src/engine/shared/library/sharedFile/src/shared/TreeFile.cpp` — `install` (priority loop, cfg keys), `find`/`open` (first-hit-wins), `fixUpFileName` (canonicalization), `searchNodePriorityOrder` — **HIGH** (authoritative engine source, read in full)
- `src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.cpp` + `.h` — `SearchTree`/`SearchTOC` binary layout, `localExists` (CRC binary search), length-0 tombstone, COT2000 fileNameOffset-as-length quirk — **HIGH** (read in full)
- `src/engine/shared/library/sharedFoundation/src/shared/Crc.cpp` — CRC-32/MPEG-2 variant (poly `0x04C11DB7`, MSB-first, init/xorout `0xFFFFFFFF`) — **HIGH**
- `stage/client.cfg` / `stage/client_d.cfg` — real `[SharedFile]` layout: `maxSearchPriority=12`, 4×`searchTOC`, 2×`searchTree`, `searchPath` override @ priority 10/11 — **HIGH**
- `D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py` — existing tested version-aware TRE/TOC/COT2000/SearchTOC parser (the reuse target) — **HIGH** (read in full, on disk)
- `.planning/PROJECT.md` — milestone scope, tool purpose (SWGSource-vs-whitengold space-asset diff first use case) — **HIGH**

---
*Architecture research for: web-based TRE compare tool (v2.3 Hardening, swg-client-v2)*
*Researched: 2026-06-12*
