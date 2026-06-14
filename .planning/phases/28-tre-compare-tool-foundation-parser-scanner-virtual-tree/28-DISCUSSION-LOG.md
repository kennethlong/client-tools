# Phase 28: TRE Compare Tool — Foundation (Parser + Scanner + Virtual Tree) - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-06-14
**Phase:** 28-tre-compare-tool-foundation-parser-scanner-virtual-tree
**Areas discussed:** Extractability, Parser reuse, Toolchain, Test fixtures

---

## Extractability — how self-contained should `tools/tre-compare/` be?

| Option | Description | Selected |
|--------|-------------|----------|
| Fully isolated package | Own pyproject.toml + lockfile + venv, ZERO engine imports; whole dir copy-pasteable to a new repo; tests use committed fixtures | ✓ |
| Isolated but reads parent cfg | Self-contained code but defaults to reading ../../stage/client.cfg | |
| Loose script set | Plain scripts sharing PATH Python, no package boundary | |

**User's choice:** Fully isolated package
**Notes:** Directly enforces the standing constraint (Kenny 2026-06-12) that the TRE tool be standalone and movable to another repo, separate from the engine build graph.

---

## Parser reuse — how to reuse the parser from swg-blender-plugin?

| Option | Description | Selected |
|--------|-------------|----------|
| Vendor copy & own | Copy tre_reader.py + tre_decrypt.py in, record provenance, diverge freely; no live link | ✓ |
| Git submodule | Submodule swg-blender-plugin — couples to its layout, breaks extraction | |
| pip install -e path | Editable install — requires plugin present on every machine | |

**User's choice:** Vendor copy & own
**Notes:** Aligns with the roadmap's "vendored" wording and the extractability constraint. Scout correction: parser source is `D:/Code/swg-blender-plugin/swg_pipeline/` (a prior memory note claiming `D:/Code/swg-tools` is stale — that path does not exist).

---

## Toolchain — Python dependency manager for the fresh project

| Option | Description | Selected |
|--------|-------------|----------|
| uv | uv for venv + locking + running; pytest for tests | ✓ |
| venv + pip + requirements.txt | Stdlib venv, pip, pinned requirements | |
| Poetry | Poetry for deps + packaging | |

**User's choice:** uv
**Notes:** Fast single tool; clean foundation for Phase 29 (FastAPI/sqlite/xxhash) and Phase 30 frontend tooling.

---

## Test fixtures — SC#3 wants tests against the real client.cfg, which has machine-specific absolute paths

| Option | Description | Selected |
|--------|-------------|----------|
| Synthetic fixtures + opt-in real | Committed byte-built mini archives + synthetic cfg for portable unit tests; ONE optional env-gated integration test against the real install | ✓ |
| Real install only | Tests point at stage/client.cfg + D:/Code/SWGSource — non-portable | |
| Synthetic only | Committed synthetic fixtures only — never exercises real data | |

**User's choice:** Synthetic fixtures + opt-in real
**Notes:** Keeps the default test run machine-independent (preserving extractability) while still satisfying SC#3's "real stage/client.cfg" intent via the gated integration test.

---

## Claude's Discretion

- Internal subpackage layout, module/dataclass names (subject to the isolation constraint).
- Synthetic-fixture construction approach (programmatic builder vs committed blobs).

## Deferred Ideas

- Diff engine + FastAPI + sqlite cache → Phase 29.
- Web SPA (TanStack Virtual tree, filters, detail panel) → Phase 30.
- TRE management (extract/repack/IFF-aware diff, TREM-01..03) → later increment; architecture must not preclude.
- Cantina corner-snap todo and x64-port todo: keyword-matched only, reviewed and not folded.
