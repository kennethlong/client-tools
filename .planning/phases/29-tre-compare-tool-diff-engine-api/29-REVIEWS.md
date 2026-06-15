---
phase: 29
reviewers: [codex, cursor, sonnet, opus]
reviewed_at: 2026-06-14
plans_reviewed: [29-01-PLAN.md, 29-02-PLAN.md, 29-03-PLAN.md]
self_skipped: claude (running inside Claude Code — reviewed by the other four for independence)
---

# Cross-AI Plan Review — Phase 29 (TRE Compare: Diff Engine + sqlite Cache + FastAPI)

Four independent reviewers on non-overlapping angles (per the project crew protocol):
**Cursor** = detailed code reader · **Codex** = repo tracer / call-graph · **fresh Sonnet** =
lateral / blind-spots · **fresh Opus** = spec/correctness reasoning. The two agents additionally
read the live Phase-28 source to verify the plans' restated interfaces against ground truth.

**Headline:** all four converge on **MEDIUM / MEDIUM-HIGH** risk. The architecture, decision-honoring,
and purity discipline are strong and the happy-path SC#1–SC#4 will pass — but the plans **assert more
correctness than their acceptance criteria prove**. Four findings reached cross-reviewer consensus and
two reviewers found code-verified bugs that will 500 the drill-in. See **Consensus Summary** at the end
for the prioritized action list.

---

## Codex Review (repo tracer)

**Summary** — Executed mostly as written, the three plans should deliver the broad SC#1-SC#4 shape. The
main risk is not missing scope, but **semantic drift between layers**: Plan 02 duplicates the Phase-28
merge, Plan 03 may not actually use cache-backed drill-in hash memoization, and several edge cases
around tombstones, rejected paths, parse failures, and TOC payload resolution are under-specified.
Viable but not yet tight enough to execute without targeted tests and API/cache contract clarifications.

**Concerns (HIGH):**
- **Plan 02 duplicates the virtual-tree merge without a strong enough parity matrix.** One parity test
  over `synthetic_install` is not enough. The dangerous cases are the rare ones: same-priority
  path/tree/toc ordering, lower-priority tree tombstone after a visible winner, higher-priority tree
  tombstone before later real copies, TOC absent entries, shadow ordering, rejected unsafe keys, node
  parse failures, `path` nodes mixed with archive nodes.
- **Plan 02 `archive_entries(abspath)` loses node-kind context.** Cache parsing should be
  `archive_entries(node)` or `archive_entries(abspath, kind)` — `abspath` alone forces accidental
  format detection and risks drift from the Phase-28 `SearchNode.kind` source of truth.
- **Plan 03 claims drill-in hash memoization but Plan 01 `drill_in` has no cache dependency.** No
  interface is specified for passing the cache into drill-in or replacing hash calls with cache lookup.
  As written, the API may rehash every drill-in, failing the SC#4 memoization intent.
- **TOC payload resolution is under-specified and possibly unsafe.** The threat model assumes
  `entry.tre_file` is bare, but the parser reads archive-controlled values. Explicitly reject
  absolute/`..`/drive/UNC/separator `tre_file`, or require the resolved path to stay under `toc_tree_path`.

**Concerns (MEDIUM):** `(abspath,mtime,size)` invalidation acceptable but not acknowledged — prefer
`stat().st_mtime_ns` over float `mtime REAL`; single shared sqlite connection across the threadpool is
fragile (lock *all* access or one-connection-per-op; set `busy_timeout`); archive-parse-failure
semantics absent from the cache parity interface (`archive_entries` has no place to return node_errors);
tombstoned/rejected paths only summarized not diffed (state it explicitly); `drill_in` not-found behavior
missing (diff.py is pure/not HTTP-aware — needs a domain result Plan 03 maps to 404/400); basename
match can collapse duplicates (dict silently overwrites); Plan 01 adds web deps before web code.

**Concerns (LOW):** `uvicorn>=0.49`/`fastapi>=0.137` floors must be validated by `uv lock` on the
executing date; API version `0.2.0` vs phase v2.3 (clarify); `installs.toml` should be a must-have
gitignore, not "if created."

**Per-plan:** 29-01 needs sharper contracts (tombstone/rejected non-rows stated; `drill_in`
canonicalizes via `fix_up_file_name`+`safe_virtual_key`; structured hash result `{hexdigest, error,
source_archive}` not nullable string + ad-hoc `hash_error`; malicious `tre_file="../x.tre"` test).
**29-02 is the highest-risk plan** — change cache API to `archive_entries(node, node_errors=None)` or
return `(rows, errors)`; key with `mtime_ns`; add the 7-case parity matrix; add `PRAGMA busy_timeout`.
29-03 — API-layer helper does `get_file_hash`→`hash_virtual_file` on miss→`put_file_hash` (keeps
sqlite out of diff.py); add 404 (unknown path) + 400 (unsafe path) tests; assert `/compare/files` has
no `hash` keys anywhere; assert malformed archive → `summary.node_errors>0` + HTTP 200; decide test
override of module-level cache/config paths or TestClient tests leak state.

**Risk: MEDIUM-HIGH.** "Would not execute Plan 02/03 without tightening those contracts first." With the
parity matrix + interface fixes → MEDIUM.

---

## Cursor Review (detailed code reader)

**Summary** — Coherent three-wave structure that mostly honors D-01–D-12; delivers the common case.
**But several semantic gaps, integration holes, and under-tested duplication risks mean SC#1–SC#4 are
not fully guaranteed** — especially tombstone/rejection semantics, TOC hash cache keying, cache↔drill-in
wiring, path-winner hashing, and the A4 parity surface. "Good skeleton, insufficient edge-case coverage
and one plan-boundary bug (cache memo not specified in diff layer)."

**Concerns (HIGH):**
- **Tombstone-only paths invisible in file diff.** `diff_virtual_trees` unions only `vt.entries` keys.
  Paths in `tombstoned` on both sides produce no row; one-sided tombstone vs missing entry appears as
  added/removed, not "tombstoned." Summary counts but UI cannot correlate per-path. Violates the spirit
  of SC#2 observability for a core Phase-28 concept.
- **`file_hash` cache key does not cover TOC payload identity.** Key is `(winner.abspath, mtime, size,
  vpath)` where `winner` is the `.toc` node, but bytes come from `join(toc_tree_path, tre_file)` — a
  different file. If the payload `.tre` changes without the `.toc` mtime/size changing, drill-in returns
  a stale/wrong hash. RESEARCH Pitfall 7 fixes lookup, not invalidation.
- **Cache memoization for drill-in is unspecified at the boundary.** Plan 01's `drill_in` calls
  `hash_virtual_file` directly (no Cache param). Plan 03 says "memoized via the cache" but never defines
  the wrapper. Implementer will guess; easy to ship uncached re-parses of 193k-entry TOCs.
- **Path-winner drill-in likely broken for real override files.** `open(os.path.join(node.abspath,
  vpath))` uses the canonical `vpath`, but `MergedEntry` doesn't store the raw walk-relative path.
  Override dirs use original casing/layout. May work on case-insensitive Windows; fragile and untested,
  and the real cfg has a priority-10 `searchPath` so override winners are on the SC#3 hot path.

**Concerns (MEDIUM):** A4 parity test too narrow (asserts `entries`/`tombstoned`/`rejected` only — NOT
`shadowed` contents/order, `node_errors`, or `follow_search_path=False`); basename dict collision (last
node wins silently); `stat_archive` lacks `_NODE_FAULTS` wrapper → one corrupt archive 500s
`/compare/set`, contradicting "node_errors never 500"; left/right status naming counterintuitive
(document "left = baseline"); module-level `Cache` vs `tmp_cache` fixture not wired (state pollution);
SC#4 integration runs compare once, never asserts a HIT.

**Concerns (LOW):** `(mtime,size)` edge cases (same-size in-place edit, subsecond mtime, `touch`);
`installs.toml` gitignore not in any task; full 200k-row JSON has no size guard.

**Risk: MEDIUM-HIGH.** Core happy-path diff LOW; tombstone/rejection/parse-failure semantics
MEDIUM-HIGH; cache parity MEDIUM-HIGH; cache correctness (payload hash key + mtime) HIGH. "Do not treat
SC#1–SC#4 as closed without addressing tombstone diff semantics, TOC/payload hash cache keying, explicit
drill-in↔cache wiring, path-winner hashing, and expanded A4 parity coverage."

---

## Sonnet Review (lateral / blind-spots)

**Summary** — Well-structured, largely honors the locked decisions; TDD/wave-ordering/fault-isolation
inheritance/separation-of-concerns are solid. But several concrete gaps could cause **silent
correctness failures the written tests would NOT catch**: three unhandled diff edge cases
(tombstoned/rejected/node_error), at least three merge subtleties the cache re-expression can silently
diverge on, an NTFS-mtime cache-key trap, and a FastAPI/Cache concurrency flaw.

**Concerns (HIGH):**
- **Tombstoned-one-side → false "added"/"removed", masking the real cause.** A tombstone ("engine
  explicitly removed this") is semantically different from "file never registered"; the diff collapses
  them. Tombstoned-both-sides is silently ignored entirely. No row-level provision; Phase 30 can't
  distinguish without a drill-in that doesn't exist for one-sided entries.
- **`safe_virtual_key`→None rejection on one side → false added/removed row.** Present in `vtL.entries`
  but rejected on the right (lands in `vtR.rejected`) classifies as "removed." Untested.
- **node_error on one side silently converts all files of the failed archive into false added/removed.**
  *The most dangerous case for a diagnostic tool* — user sees "1000 files added on the right" when the
  left archive actually failed to parse. Summary carries the count; rows give no indication.
- **Concurrent MISS race in the cache → PRIMARY KEY violation → 500.** The MISS path is `SELECT` (miss)
  then `INSERT`; the write Lock guards only the write, not the read-then-write. Two threads (even one
  browser making two parallel requests) can both miss and race to `INSERT` the same PK. Plain `INSERT`
  (not `INSERT OR IGNORE/REPLACE`) → constraint violation surfaced as a 500.

**Concerns (MEDIUM):** the three cache-merge subtleties (shadow-append stores the *node* not the entry
and depends on exact `is_toc_absent`=`length==0 or offset==0` column reads; toc-absent-vs-tombstone
branches on `node.kind` which `archive_entry` does NOT store — only `archive_meta` does, so a wrong join
flips behavior; `path` nodes bypass the cache and the plan never states it); `file_hash` memo keyed on
the `.toc` not the payload `.tre` (stale hash on drill-in — same finding as Cursor); no scan-result /
virtual-tree caching across requests (O(N) rebuild per call, undocumented); `/file/detail` not-found
behavior unspecified/untested; NTFS float-mtime fine but FAT32/network-share 1–2s resolution defeats the
key (SWG installs live on external drives).

**Concerns (LOW):** default `.sqlite` path resolved via cwd not `__file__` (wrong-dir launch → empty
cache every run); set-level `path`-node exclusion is a blind spot for override-only-on-one-side
installs (per-spec, but undocumented); `GET /installs` absent-config returns 200 `[]` indistinguishable
from a misconfigured deploy; Phase-28 integration test still hardcodes `stage/client.cfg` and is NOT the
SC#4 proof.

**Suggestions:** add an optional `qualifier: tombstoned_left|rejected_left|error_left` to lean rows
(backwards-compatible, not a hash so doesn't violate D-09); three new `test_diff.py` cases; fix the
`file_hash` key to the payload `.tre`; make MISS+INSERT atomic under the lock or use `INSERT OR
IGNORE/REPLACE`; resolve the default cache path via `__file__`.

**Risk: MEDIUM-HIGH.** Silent misclassification (tombstone/rejected/node_error) is a *correctness*
failure for a diagnostic tool, not cosmetic; TOC hash memo stale; concurrent-MISS race 500.

---

## Opus Review (spec / correctness reasoning — code-verified against Phase-28 source)

**Summary** — Happy-path SC#1–SC#4 very likely delivered; architecture clean and unusually
well-grounded in the real Phase-28 API. **However**, several SC acceptance criteria prove *correlates*
rather than the thing itself, the tri-state diff has an under-specified interaction with
tombstoned/rejected/node_errors that no test forces, the A4 duplication carries real divergence risk a
single synthetic parity test can't retire, and the drill-in payload path has a concrete
`KeyError`-escapes-`_NODE_FAULTS` hole. Risk **MEDIUM**: nothing architecturally wrong, but the proofs
are softer than the spec claims.

**Concerns (HIGH):**
- **`KeyError` from `read_tre_payload` is NOT in `_NODE_FAULTS` and escapes the degrade wrapper.**
  Code-verified: `read_tre_payload` (tre_reader.py:265) raises bare `KeyError(logical_name)` on a name
  miss, and `_NODE_FAULTS` = `(TreFormatError, UnsupportedTreVersionError, struct.error, OSError,
  MemoryError, zlib.error)` — no `KeyError`. The plan says "wrap the hash read in `_NODE_FAULTS` … and
  DEGRADE." A vpath/logical-name mismatch therefore **raises through the handler and 500s the
  drill-in**, violating T-29-01-02 ("never raise") and the API "never 500" promise. *Fix:* catch
  `(*_NODE_FAULTS, KeyError)`.
- **Canonical-key vs raw-entry-path mismatch on the tree hash path silently hashes the wrong/no file.**
  Code-verified: the merge key is `safe_virtual_key`→`fix_up_file_name` (strips leading `./`/`../`,
  collapses slashes, lowercases). Drill-in passes that *canonical* `vpath` into
  `read_tre_payload(node.abspath, vpath)`, which matches against the *raw* `entry.path` using only
  `.replace("\\","/").lower()` — it does **not** strip leading dots or collapse slashes. Any entry with
  a leading `./`, `../`, or doubled slash is reachable under the merged key but unreachable by
  `read_tre_payload` → `KeyError` → (per the bug above) 500. The toc path is symmetric (Pitfall 4) and
  fine; the **tree path is asymmetric** and untested. *Fix:* resolve the tree winner like the toc —
  iterate `read_tre_entries`, match `fix_up_file_name(e.path)==vpath`, read by raw `e.path`.

**Concerns (MEDIUM):** SC#4 "re-runs instant" never proven — only "rows match" (the HIT test has no
instrumentation that the parse was skipped; spy `read_tre_entries`/`iter_node_entries` and assert
call-count==1 across two calls); D-05 set-level entry-count collision can false-"identical" and toc
count is the master-index count (acceptable as cheap signal but undocumented — surface as
"identical-by-header" so the UI doesn't overclaim); `diff_virtual_trees` is *total* over
{present/absent}² but the *meaning* of absent (tombstoned vs rejected vs parse-failed vs genuinely
missing) is collapsed with no row-level test; `(abspath,mtime,size)` false-HIT modes are real and
unmentioned — **mtime-preserving writes (`robocopy /COPY:T`, `touch -r`, backup restore) + same size
defeat the key on the exact use case the tool exists for** → offer `--no-cache`/cache-bust + document.

**Concerns (LOW):** WAL+Lock correct but reads unguarded + MISS-under-lock serializes writers for a
193k-entry parse (note it; use fresh cursors); scanner `Path(...).read_text()` on a request-controlled
cfg has no size cap (DoS-on-yourself); the Task-1 `import uvicorn` verify passes even WITH the heavy
extra so it doesn't prove "bare" — grep `uv.lock` for no `uvicorn[standard]`/`uvloop`; `/file/detail`
verdict undefined when one hash degrades — add `content-indeterminate`.

**Strengths called out:** genuine API grounding (all assumed fields exist; `TreEntry` correctly has no
`tre_file`); purity invariant enforced by grep not just stated; crc-avoidance tested negatively; the
false-identical test actually proves SC#3's core value.

**Risk: MEDIUM.** Driven by the single-fixture A4 parity (doesn't even assert `shadowed`/`rejected`
order), SC#4 proven only by row-equality, the `KeyError`+tree-path-asymmetry 500 pair, and the
tombstoned/rejected/parse-failed collapse. All cheaply fixable within the plan structure; suggestions
1–4 → LOW.

---

## Consensus Summary

### Agreed strengths (2+ reviewers)
- Clean 3-wave layering (pure `diff.py` → cache → thin FastAPI); wave ordering 01→02→03 correct.
- `diff.py` purity (no fastapi/sqlite3) is **mechanically grep-tested**, not just stated — protects the
  Phase-30/TREM headless-import guarantee.
- crc-avoidance locked negatively (D-06); decompressed-byte xxhash on drill-in only (SC#3) is correct
  and the false-identical test actually proves it.
- Fault-isolation (`_NODE_FAULTS` + preflights) correctly inherited for the cache MISS parse.
- Plans are genuinely grounded in the real Phase-28 API (Opus code-verified every assumed field).
- Localhost-only bind + env-gated integration test (machine-independent default run).

### Agreed concerns (priority — consensus first)

**P1 — Tombstoned / rejected / node_error paths silently become misleading added/removed rows
(HIGH; ALL FOUR reviewers).** This is the strongest consensus and the most serious for a *diagnostic*
tool: a parse failure or one-sided tombstone is collapsed into "N files added/removed," sending the user
down a false trail. No row-level test forces the behavior. **Fix:** add an optional row `qualifier`
(`tombstoned_left|rejected_left|error_left` + right variants — backwards-compatible, not a hash so D-09
holds), surface it, and add three `test_diff.py` cases pinning the row-level appearance of each.

**P2 — Cache parity (A4) retired by a single happy-path fixture that doesn't even assert
`shadowed`/`rejected` ordering (HIGH; ALL FOUR).** `build_virtual_tree_cached` re-implements the ~40-line
merge from sqlite rows. The current parity test asserts `entries`/`tombstoned`/`rejected` only. **Fix:**
assert `shadowed` tuples element-equal and `rejected` *list* order; add cases for tree-tombstone-then-real-winner,
lower-priority-tombstone-doesn't-evict, toc-absent, multi-shadow (3-way), a `path` node, rejected key,
and a malformed-archive node_error; prefer property-based over randomized node orderings. Also: pass
`SearchNode.kind` to the cache (`archive_entries(node)` not `(abspath)`) — `kind` lives only in
`archive_meta`, and a wrong join flips tombstone↔absent semantics (Codex + Sonnet).

**P3 — Drill-in ↔ cache hash memoization boundary is undefined, AND the `file_hash` key is bound to
the wrong file (HIGH; memo-gap: Cursor+Codex · wrong-key: Cursor+Sonnet+Opus).** Plan 01 `drill_in` takes
no cache; Plan 03 says "memoized via the cache" with no interface → likely ships uncached 193k-entry
re-parses. Worse, the `file_hash` PK uses the `.toc`'s `(abspath,mtime,size)` while the bytes come from
the payload `.tre` — a changed payload with an unchanged `.toc` returns a **stale hash**. **Fix:** an
API-layer helper `get_file_hash`→`hash_virtual_file`(on miss)→`put_file_hash` (keeps sqlite out of
diff.py), keyed on the **payload `.tre`'s** identity for toc-served files; add a test that mutates the
payload without touching the `.toc`.

**P4 — `(abspath, mtime, size)` invalidation has real false-HIT modes, unmentioned (MEDIUM; ALL FOUR).**
mtime-preserving copies/restores + equal size defeat the key on the tool's exact purpose; FAT32/network
shares have 1–2s resolution. **Fix:** use `st_mtime_ns` (Codex+Opus), document the residual, and offer a
`--no-cache`/cache-bust affordance.

**P5 — SC#4 "instant re-run" is proven only by row-equality, never by a measured parse-skip (MEDIUM;
Cursor+Opus).** **Fix:** spy `iter_node_entries`/`read_tre_entries` and assert call-count==1 across two
`archive_entries` calls (and a second compare in the integration test asserting a HIT).

**Also agreed (MEDIUM/LOW):** basename collision silently overwrites in the set diff (Cursor+Codex —
use `(basename,kind)` or emit a collision diagnostic); `drill_in` not-found behavior unspecified
(Codex+Sonnet+Opus — define a domain result Plan 03 maps to 404/400; add the test); module-level
`Cache`/config not test-overridable → state leak (Cursor+Codex+Sonnet — `create_app(cache=None)` or an
env var, wire `tmp_cache`); `installs.toml` must be gitignored, not "if created" (Cursor+Codex);
set-level `stat_archive` needs the `_NODE_FAULTS` wrapper or a corrupt archive 500s `/compare/set`
(Cursor, reinforced by Sonnet/Opus's "never 500" theme).

### Divergent / unique findings worth investigating
- **Opus (code-verified, unique, critical):** `KeyError` from `read_tre_payload` escapes `_NODE_FAULTS`
  → 500 on drill-in; and the **tree-path canonical-vs-raw asymmetry** that triggers exactly that
  `KeyError` on leading-dot/doubled-slash entry names. Catch `(*_NODE_FAULTS, KeyError)` and resolve the
  tree winner symmetrically with the toc path. *No other reviewer caught these — they read the actual
  payload-resolution and canonicalization code.*
- **Sonnet (unique, concrete):** concurrent-MISS race on plain `INSERT` → PK violation → 500 (even from
  two parallel requests in one browser). Use `INSERT OR IGNORE/REPLACE` or make MISS-detect+INSERT
  atomic under the lock.
- **Codex/Opus:** add `PRAGMA busy_timeout`; guard *all* connection access or use per-op connections,
  not only writes.
- **Codex:** structured hash result `{hexdigest, error, source_archive}` instead of nullable string +
  ad-hoc `hash_error`. **Opus:** add a `content-indeterminate` verdict when one side's hash degrades.
- **Sonnet:** resolve the default `.sqlite` path via `__file__`, not cwd.
- **Codex:** validate the `fastapi>=0.137`/`uvicorn>=0.49` floors actually resolve via `uv lock` on the
  executing date (and grep the lock to prove `uvicorn` is bare, since `import uvicorn` passes either way).

### Verdict
**Consensus risk: MEDIUM-HIGH as written → MEDIUM after P1–P3.** No reviewer found an architectural
flaw; all found that the plans prove less than they claim. The plans are executable, but **Plans 02/03
should be tightened before execution** on: the row-level tombstone/rejected/error qualifier (P1), the
expanded parity matrix + `kind`-aware cache API (P2), the drill-in↔cache wiring + payload-keyed hash
memo (P3), and the two Opus-verified drill-in 500 bugs (catch `KeyError`, symmetric tree-path
resolution). These are all cheap fixes within the existing structure.

---

To incorporate this feedback into the plans:

  `/gsd:plan-phase 29 --reviews`
