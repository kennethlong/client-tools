# tre-compare

A **standalone, copy-paste-extractable** Python package for scanning SWG TRE/TOC
archive sets and merging them into an engine-faithful virtual file tree.

This is the headless foundation (Phase 28) for a TRE compare/diff tool. It has
**zero runtime dependencies** and **zero imports from any engine repo** (decision
D-01) — the entire `tools/tre-compare/` directory can be dropped into a fresh repo
and run as-is.

## What it does

- **Parser** (`tre_compare.parser`, vendored) — reads all TREE format variants
  (0004/0005/6000/0006/5000), retail SearchTOC (TAG_TOC/0001), and Restoration
  COT2000 master indexes. Stdlib-only.
- **Scanner** — hand-parses `client.cfg` `[SharedFile]` repeated/indexed keys into a
  priority-ordered search-node list (higher priority number searched first).
- **Virtual-tree builder** — merges archives with first-hit-wins precedence,
  per-node-type length-0 tombstone handling, and `fixUpFileName` canonicalization,
  mirroring the engine's `TreeFile.cpp` semantics.

The diff engine, FastAPI surface, and web UI live in later phases (29, 30).

## Requirements

- **Python 3.11+** — `requires-python = ">=3.11"`. `.python-version` is pinned to
  the `3.11` floor so a fresh clone / CI / a 3.11–3.12 contributor resolves cleanly;
  3.12, 3.13, and 3.14 all work too.
- **uv** for venv + dependency locking + running (decision D-05). The committed
  `uv.lock` pins the resolved toolchain.

## Usage

```bash
# from tools/tre-compare/
uv sync                              # create the managed venv from uv.lock
uv run pytest -m "not integration"   # default test run (machine-independent)
uv run pytest -m integration         # opt-in real-install test (env-gated; see below)
```

The optional integration test is gated behind an env var and is off by default.
On PowerShell:

```powershell
$env:TRE_COMPARE_INTEGRATION=1; uv run pytest -m integration
```
