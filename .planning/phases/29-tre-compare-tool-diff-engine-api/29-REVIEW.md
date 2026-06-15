---
phase: 29-tre-compare-tool-diff-engine-api
reviewed: 2026-06-14T00:00:00Z
depth: standard
files_reviewed: 11
files_reviewed_list:
  - tools/tre-compare/src/tre_compare/diff.py
  - tools/tre-compare/src/tre_compare/cache.py
  - tools/tre-compare/src/tre_compare/api.py
  - tools/tre-compare/src/tre_compare/config.py
  - tools/tre-compare/src/tre_compare/__main__.py
  - tools/tre-compare/pyproject.toml
  - tools/tre-compare/tests/test_diff.py
  - tools/tre-compare/tests/test_cache.py
  - tools/tre-compare/tests/test_api.py
  - tools/tre-compare/tests/test_integration.py
  - tools/tre-compare/tests/conftest.py
findings:
  critical: 1
  warning: 3
  info: 4
  total: 8
status: issues_found
---

# Phase 29: Code Review Report

**Reviewed:** 2026-06-14
**Depth:** standard
**Files Reviewed:** 11
**Status:** issues_found

## Summary

Reviewed the Phase-29 tre-compare diff engine (`diff.py`), sqlite cache (`cache.py`), FastAPI
surface (`api.py` / `__main__.py`), installs-config loader (`config.py`), `pyproject.toml`, and
the five test modules. The code is generally careful: all sqlite is **fully parameterized** (no
string-built SQL anywhere — no SQL injection), the never-raise hash contract is correctly
implemented via `_HASH_FAULTS = (*_NODE_FAULTS, KeyError)`, the merge re-implementation in
`build_virtual_tree_cached` is a faithful copy of `build_virtual_tree`, the localhost-only bind
(`127.0.0.1`) is genuinely enforced in `__main__.py` (uvicorn never sees an all-interfaces
address), and `safe_virtual_key` correctly rejects interior `..` / UNC / drive-letter keys before
any filesystem join on the user-supplied drill `path`.

However, the review found one **BLOCKER**: a path-traversal in the TOC payload resolver that uses
an **archive-content-controlled** filename (`entry.tre_file`) in an `os.path.join`, which escapes
the intended `toc_tree_path` root and lets a request hash an arbitrary file on disk. The
localhost-only posture bounds blast radius to the operator's own machine, but the data driving the
traversal is the *parsed archive* (which can come from an untrusted install being diagnosed), so it
is not self-only. Three WARNINGs cover error-handling gaps and a dead optimization that re-parses a
193k-entry master index on every drill-in.

## Critical Issues

### CR-01: Path traversal via archive-controlled `tre_file` in the TOC payload resolver

**File:** `tools/tre-compare/src/tre_compare/diff.py:345` (and the identical resolve in
`tools/tre-compare/src/tre_compare/api.py:122`)

**Issue:** `_payload_tre_for_toc` builds the payload `.tre` path with

```python
payload_tre = os.path.join(toc_tree_path or "", e.tre_file)
```

`e.tre_file` is **content read out of the master-index archive** (`read_search_toc_entries` /
`read_cot2000_entries` → `tre_reader.py`), not a vetted path. `os.path.join` has the well-known
foot-gun that an **absolute** right-hand component *discards* the left: if a crafted (or simply
corrupt) `.toc` carries `tre_file = "C:/Windows/win.ini"` or `tre_file = "../../../../secret"`, the
join yields an arbitrary filesystem path, and `hash_virtual_file` (diff.py:374) then opens and
hashes that file via `read_tre_payload`. The drill-in endpoint (`POST /file/detail`) reaches this
for any toc-served winner. The threat note T-29-01-03 claims "no caller path component enters the
join," which is true — but the *archive* component does, and the install being diffed is exactly
the untrusted input this tool exists to inspect.

The `vpath`/loose-path branch (diff.py:377) is safe because `vpath` is the
`safe_virtual_key`-canonicalized key; only this `tre_file` branch is unguarded.

**Fix:** Reject any `tre_file` that is absolute or escapes `toc_tree_path`, and confirm the
resolved path stays within the configured tree root before opening:

```python
import os.path

base = os.path.abspath(toc_tree_path or "")
candidate = os.path.abspath(os.path.join(base, e.tre_file))
# os.path.commonpath raises on mixed drives (Windows) -> treat as escape
try:
    if os.path.commonpath([base, candidate]) != base:
        raise KeyError(vpath)  # degrades to a DriveHashResult error, never opens
except ValueError:
    raise KeyError(vpath)
payload_tre = candidate
```

Alternatively, reject up front: `if os.path.isabs(e.tre_file) or ".." in e.tre_file.replace("\\","/").split("/"): raise KeyError(vpath)`. Either keeps the never-raise contract (the
`KeyError` is caught by `_HASH_FAULTS` and degrades to a `DriveHashResult` error). Add a test with a
forged `.toc` whose `tre_file` is `../../escape.tre` asserting `hash.error` is set and no file
outside the root is read.

## Warnings

### WR-01: `get_or_compute_hash` only catches `OSError`, not `sqlite3.Error` → 500 path

**File:** `tools/tre-compare/src/tre_compare/api.py:129-143`

**Issue:** The memo GET/PUT are wrapped only in `except OSError`:

```python
try:
    cached = cache.get_file_hash(memo_node, vpath)
except OSError:
    return hash_virtual_file(node, vpath, toc_tree_path)
...
try:
    cache.put_file_hash(memo_node, vpath, result.hexdigest)
except OSError:
    pass
```

`cache.get_file_hash`/`put_file_hash` run `self._conn.execute(...)`, which can raise
`sqlite3.OperationalError` (e.g. `database is locked` past `busy_timeout`, or `disk I/O error`) or
`sqlite3.ProgrammingError` (recursive cursor use on the shared connection under the threadpool —
see WR-02). None of those are subclasses of `OSError`, so they escape `get_or_compute_hash`,
escape the route, and surface as an unhandled 500 — defeating the "drill-in never 500s" intent
(`_validate_cfg`, the fault-row path, etc. all target never-500). The `_identity` `os.stat` inside
those methods *is* an `OSError`, so the OSError catch is partially load-bearing, but the sqlite call
itself is not covered.

**Fix:** Broaden to cover the database errors:

```python
except (OSError, sqlite3.Error):
    return hash_virtual_file(node, vpath, toc_tree_path)
...
except (OSError, sqlite3.Error):
    pass
```

(`import sqlite3` at module top in `api.py`, or catch `Exception` narrowly with a comment.) A memo
failure must degrade to a fresh compute, never a 500.

### WR-02: Shared `sqlite3.Connection` read on the threadpool without a lock

**File:** `tools/tre-compare/src/tre_compare/cache.py:139-153, 268-279` (reads) vs the
write `self._lock` at 235, 286

**Issue:** `Cache` opens one connection with `check_same_thread=False` (cache.py:122) and the
FastAPI threadpool shares that single `Cache` across worker threads (per the Plan-02/03 design).
All *writes* are serialized by `self._lock`, but the *reads* — `_has_meta`, `_select_entries`
(`archive_entries` HIT path, called without the lock at cache.py:208-209), and `get_file_hash`
(api.py drill path) — call `self._conn.execute(...)` with **no lock**. `check_same_thread=False`
only disables Python's same-thread *assertion*; it does not make a single `sqlite3.Connection`
safe for genuinely concurrent use. Two threads driving `execute` on the same connection object can
raise `sqlite3.ProgrammingError` ("Recursive use of cursors not allowed" / "Cursor needed to be
reset") or interleave a read against an in-flight write. The `test_concurrent_miss` test only
exercises two threads racing the *write* path (which is lock-guarded) and does not stress
concurrent reads, so the gap is unproven-safe, not proven-safe.

**Fix:** Either (a) guard every `self._conn.execute` read under `self._lock` too (simplest, the
cache is not a throughput bottleneck for a single-user tool), or (b) switch to a
`threading.local()` connection-per-thread (each thread opens its own `sqlite3.connect` to the same
WAL file — the canonical sqlite threadpool pattern), or (c) gate concurrency out with
`uvicorn.run(..., workers=1)` plus a documented "routes are sync, run on the threadpool" caveat and
serialize all DB access through one lock. Given WR-01, broadening the route-level catch is the
seatbelt; this WR is the root cause.

### WR-03: Pitfall-7 cached `tre_file` is dead — drill-in re-parses the full master index every call

**File:** `tools/tre-compare/src/tre_compare/cache.py:155-168, 213-223` (writes `tre_file`) vs
`tools/tre-compare/src/tre_compare/api.py:122` / `diff.py:330-347` (the drill path)

**Issue:** The cache goes to real effort to compute and store a per-row `tre_file` payload name
(`_toc_tre_files`, the `archive_entry.tre_file` column), and the module docstring (cache.py:10-11)
states this exists "so a drill-in resolves the payload `.tre` without re-parsing a 193k-entry
`.toc` (Pitfall 7)." But **nothing on the drill path reads it.** `get_or_compute_hash`
(api.py:122) calls `_diff._payload_tre_for_toc`, which (diff.py:338-347) re-runs
`detect_master_index_kind` + `read_search_toc_entries`/`read_cot2000_entries` over the **entire**
master index and linear-scans for `fix_up_file_name(e.path) == vpath` on *every* drill-in. The
stored `tre_file` column is therefore dead data, and the stated Pitfall-7 optimization is not
realized — exactly the 193k-entry re-parse it claims to avoid happens on each `/file/detail`.

This is correctness-of-design / dead-code rather than a crash, but it is a real defect: the column
is written-never-read, and a drill-in on a large real install (the SC#4 north-star pair) pays the
full master-index parse it was designed to skip.

**Fix:** Either (a) add a `Cache` method that resolves the payload `.tre` from the cached
`archive_entry.tre_file` (`SELECT tre_file FROM archive_entry WHERE abspath=? AND ... AND vpath=?`)
and have `get_or_compute_hash` use it, falling back to `_payload_tre_for_toc` only on a cache miss;
or (b) if the re-parse is acceptable for a single-user tool, drop the `tre_file` column and
`_toc_tre_files` entirely and delete the Pitfall-7 docstring claim so the code matches the
behavior. Do not leave a written-never-read column with a docstring asserting an optimization that
does not exist.

## Info

### IN-01: `_validate_cfg` permits opening any readable file as a cfg

**File:** `tools/tre-compare/src/tre_compare/api.py:73-84`

**Issue:** `_validate_cfg` checks only `is_file()` + `os.access(R_OK)`; the request body's
`left_cfg`/`right_cfg` can be any readable path on the host (`C:/Windows/system.ini`, etc.).
`parse_shared_file` then reads it whole. This is arbitrary-file-open-and-parse by request. It is
**accepted** under the locked localhost-only single-user posture (D-01 / T-29-03-03), and the cfg
parser only extracts `[SharedFile]` keys, so disclosure is limited. Noting it because the binding
in `__main__.py` is the *only* control; if a future increment ever relaxes the bind (or serves the
Phase-30 SPA from a non-loopback address), this becomes a real SSRF/LFI. Consider, at minimum, a
`# SECURITY:` comment at `_validate_cfg` tying it to the localhost-bind invariant so the coupling is
not lost.

### IN-02: cfg `read_text` has no size cap

**File:** `tools/tre-compare/src/tre_compare/scanner.py:120` (Phase-28 code; reached via the
Phase-29 routes)

**Issue:** `Path(cfg_path).read_text(...)` reads a request-controlled cfg whole into memory with no
cap. Already documented as accepted in `tests/conftest.py:25-28` (Opus LOW). Listed here only for
completeness; no action required under the current posture.

### IN-03: memo-HIT `source_archive` is recomputed, not stored

**File:** `tools/tre-compare/src/tre_compare/api.py:119-135`

**Issue:** On a `file_hash` HIT, `get_or_compute_hash` returns a synthetic `DriveHashResult` whose
`source_archive` it recomputed by re-resolving the payload `.tre` (the same re-parse as WR-03). The
value is correct; it is just derived rather than persisted. If WR-03 is fixed by reading the cached
`tre_file`, `source_archive` falls out for free. Cosmetic.

### IN-04: `test_keyerror_degrade` monkeypatches the reader to *always* raise

**File:** `tools/tre-compare/tests/test_diff.py:196-208`

**Issue:** The test forces `read_tre_payload` to raise `KeyError` unconditionally to prove the
never-raise degrade. That validates the catch, but it is a broad stub (any call raises), so it does
not distinguish "name-miss KeyError" from "the read got that far." It is a valid test of the
never-raise contract; consider also a fixture-driven case (a real `.tre` missing the looked-up
entry) so the degrade is proven against genuine archive bytes, not only a blanket monkeypatch.
Not blocking.

---

_Reviewed: 2026-06-14_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
