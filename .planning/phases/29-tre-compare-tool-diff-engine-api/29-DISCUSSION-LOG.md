# Phase 29: TRE Compare Tool — Diff Engine + API - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-06-14
**Phase:** 29-TRE Compare Tool — Diff Engine + API
**Areas discussed:** API shape & request model, Set-level diff semantics, File-level diff at scale, Cache & new dependencies

---

## API shape & request model

| Option | Description | Selected |
|--------|-------------|----------|
| Stateless POST-a-pair | POST {cfgA,cfgB} → diff; no install registry; sqlite cache is only persistence | ✓ (Claude's call) |
| Register installs, compare by id | POST /installs → id; GET /compare?left=id&right=id; cleaner URLs, lifecycle state | |
| You decide | Delegate to Claude | ✓ (user choice) |

**User's choice:** "You decide" → locked Stateless POST-a-pair (D-01) + config-driven `GET /installs` picker feed (D-02).
**Notes:** Best fit for a single-user localhost diagnosis tool; avoids server-side lifecycle state.

---

## Set-level diff semantics — archive matching

| Option | Description | Selected |
|--------|-------------|----------|
| By basename | Match on filename only, ignoring directory; only-in-A=removed, only-in-B=added | ✓ |
| By search-node identity | Match on (kind, priority, basename) from the cfg node list; stricter, can over-split | |
| You decide | Delegate to Claude | |

**User's choice:** By basename (D-04).
**Notes:** Abspaths differ across install roots; basename is the stable identity.

---

## Set-level diff semantics — "changed" criteria

| Option | Description | Selected |
|--------|-------------|----------|
| Any of size / count / version | Changed if file size OR entry-count OR TREE version tag differs; all shown | ✓ |
| On-disk size only | Changed if byte size differs; count/version info-only | |
| You decide | Delegate to Claude | |

**User's choice:** Any of size / count / version (D-05).
**Notes:** Cheap header/stat signal (no content hashing); surfaces WHY in the row.

---

## File-level diff at scale — status taxonomy

| Option | Description | Selected |
|--------|-------------|----------|
| Tri-state changed | added/removed/changed/identical-by-metadata; drill-in xxhash upgrades metadata-identical | ✓ (Claude's call) |
| Binary changed/identical | added/removed/changed/identical; hash is detail-only, doesn't change badge | |
| You decide | Delegate to Claude | ✓ (user choice) |

**User's choice:** "You decide" → locked Tri-state (D-06).
**Notes:** Honest about false-identicals (length,clen match but content differs); maps to SC#3 drill-in.

---

## File-level diff at scale — delivery

| Option | Description | Selected |
|--------|-------------|----------|
| Full payload, client-virtualized | Whole flat diff in one JSON; filter/search/hide-identical in browser | ✓ (Claude's call) |
| Server-side paginated/filtered | filter+search+offset/limit; page+counts per request | |
| You decide | Delegate to Claude | ✓ (user choice) |

**User's choice:** "You decide" → locked Full payload, client-virtualized (D-08) + lean per-row schema (D-09).
**Notes:** Matches Phase 30's TanStack Virtual plan; one round-trip; ~few MB at 100k lean rows on localhost.

---

## Cache & new dependencies — cache scope

| Option | Description | Selected |
|--------|-------------|----------|
| Per-archive parsed TOC | Cache parsed entries keyed by (abspath,mtime,size); merge/diff re-run from cache; + memoized file_hash table | ✓ (Claude's call) |
| Per-pair diff result | Cache whole diff for an (A,B) pair; coarser, not reusable across pairings | |
| You decide | Delegate to Claude | ✓ (user choice) |

**User's choice:** "You decide" → locked Per-archive parsed TOC + memoized file_hash (D-10).
**Notes:** Granular, reusable across any install pairing.

---

## Cache & new dependencies — xxhash source

| Option | Description | Selected |
|--------|-------------|----------|
| xxhash PyPI (C-ext) | Add `xxhash` to pyproject, pin in uv.lock; fast C extension | ✓ (Claude's call) |
| stdlib hashlib (blake2b) | Built-in, zero extra dep; deviates from SC#3 'xxhash' wording | |
| You decide | Delegate to Claude | ✓ (user choice) |

**User's choice:** "You decide" → locked xxhash PyPI C-ext (D-11).
**Notes:** SC#3 names xxhash; FastAPI already adds a dep; uv.lock keeps fresh-clone extraction reproducible (D-01).

---

## Claude's Discretion

- API model (D-01/D-02/D-03) — user delegated; locked stateless POST-a-pair + config-driven picker.
- File status taxonomy (D-06) — user delegated; locked tri-state.
- File delivery model (D-08/D-09) — user delegated; locked full-payload client-virtualized.
- Cache scope (D-10) — user delegated; locked per-archive parsed TOC + memoized hashes.
- xxhash source (D-11) — user delegated; locked xxhash PyPI C-ext.
- Endpoint paths/verbs, Pydantic field names, error-envelope, localhost binding/security, sqlite
  schema details, cache file location, decompressed-vs-compressed hash bytes — left to planner/executor.

## Deferred Ideas

- React/Vite/shadcn virtualized SPA — Phase 30 (TRE-05).
- Server-side pagination/filtering — not built (chose full-payload + client virtualization).
- Stateful install registry/lifecycle — not built (chose stateless + config-driven picker).
- TRE management (extract/repack/IFF-aware diff) — TREM-01..03, later increment.
- Phase-28 approximation-boundary items (lazy searchPath, searchAbsolute/searchCache) — revisit
  only if the real SWGSource/whitengold cfgs exercise them.
