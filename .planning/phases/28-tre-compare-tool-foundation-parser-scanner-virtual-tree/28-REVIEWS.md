---
phase: 28
reviewers: [codex, cursor]
reviewed_at: 2026-06-14T18:35:16Z
review_round: 2
plans_reviewed: [28-01-PLAN.md, 28-02-PLAN.md, 28-03-PLAN.md, 28-04-PLAN.md]
self_skipped: claude (running inside Claude Code — skipped for independence)
unavailable: [gemini, coderabbit, opencode, qwen]
note: "Round 2 — re-review of the plans revised after the round-1 cross-AI pass (commit 847cb7e35). Round-1 REVIEWS.md is preserved in git history at 6106b7bb4."
---

# Cross-AI Plan Review — Phase 28 (Round 2)

Re-review of the four revised plans (parser/scanner/virtual-tree foundation) after the
round-1 revisions landed. Two independent external reviewers (OpenAI Codex, Cursor Agent);
Claude skipped for independence (run from inside Claude Code); Gemini/CodeRabbit/OpenCode/Qwen
not installed. Both reviewers rate the set **MEDIUM** overall risk: architecture, vendoring,
and the corrected single-pass tombstone model are solid; remaining risk is concentrated in
execution details (SearchTOC handling, SC#3 wording, same-priority node ordering).

## Codex Review

## Summary

The revised plan set is strong overall: it correctly isolates the package, keeps Phase 28 scoped to parser/scanner/virtual-tree, and directly targets the main correctness trap: per-node-type tombstone behavior. The biggest remaining risks are not in the high-level architecture but in execution details: SearchTOC/COT2000 hostile-size preflighting is under-specified, SC#3 is explicitly narrowed from the roadmap wording, and Plan 03's virtual-tree API/pseudocode has a few ambiguities that can produce either implementation drift or tests that pass the wrong thing.

## Strengths

- Clear D-01 isolation boundary: own `pyproject.toml`, lockfile, package-local `.gitignore`, no engine imports.
- Good correction on tombstones: tree length-0 only tombstones when first encountered; toc length-0/offset-0 is skip-only.
- Single descending pass is the right model for first-match-wins.
- Plan 04 adds the important inverse tombstone test: lower-priority tree tombstone must not erase a higher-priority real winner.
- Synthetic fixtures are the right default corpus; the real install is correctly env-gated.
- Parser vendoring is conservative: provenance plus one import rewrite, no rewrite during foundation work.
- `node_errors` is a good addition; malformed archive behavior needs to be observable for Phase 29.

## Concerns

- **HIGH — Plan 04 narrows SC#3 without changing the controlling requirement.**
  Plan 04 says real-cfg tombstone proof is delegated to synthetic tests, but the roadmap success criterion says "unit tests against the real `stage/client.cfg` proving correct override shadowing and tombstone behavior." That is a real requirement mismatch. The plan documents the interpretation, but it does not resolve it. Either the roadmap/SC must be amended, or the plan should not claim SC#3 is literally met.

- **HIGH — Plan 03 hardening preflights `.tre` only, not SearchTOC/COT2000.**
  Plan 03 Task 2 says SearchTOC/COT2000 readers are covered by try/except and "a separate offset-arithmetic preflight is not required." That is weak. If the vendored parser reads size-declared blocks before raising, a hostile `.toc` can still force large reads/allocations. The same `file_size` bound discipline should be applied to SearchTOC and COT2000 headers before calling `read_search_toc_entries` / `read_cot2000_entries`.

- **MEDIUM — `fix_up_file_name` is doing two jobs with conflicting semantics.**
  Plan 03 says `fix_up_file_name` mirrors `TreeFile.cpp:511 exactly`, then adds traversal rejection. The safer design is to keep `fix_up_file_name()` strictly engine-faithful and add a separate `is_safe_virtual_key()` / `canonicalize_entry_key()` wrapper. Otherwise tests may accidentally assert non-engine behavior on the function named after the engine routine.

- **MEDIUM — SearchPath symlink/reparse handling is not sufficient unless directory descent is pruned.**
  Plan 03 says skip symlinks/reparse points "for each entry," but with `os.walk`, reparse directories must be removed from `dirnames` before descent. Checking after traversal is too late for junction loops or escapes. This should be explicit: mutate `dirnames[:]` to remove links/reparse dirs before sorting/yielding.

- **MEDIUM — Shadow recording semantics are inconsistent.**
  Plan 03 pseudocode appends any later node to `shadowed` when `key in claimed`, including lower-priority tree tombstones and toc absent entries. Later text says append "ONLY when a later REAL copy is seen," while Plan 04's inverse tombstone test allows the lower length-0 node in `shadowed`. Pick one. I recommend `shadowed` mean "later real visible candidate only," and record skipped tombstones/absent entries elsewhere only if needed.

- **MEDIUM — Scanner intentionally omits legacy key grammar.**
  Plan 03 documents that `searchPath10` style keys are deferred. That may be acceptable for this repo's current cfg, but it is a gap against "engine-faithful" scanner language. If this tool is meant to compare arbitrary SWG installs, support both `_NN_` and bare-priority forms now; it is cheap and removes a future footgun.

- **MEDIUM — TOCTreePath is treated as informational in Phase 28, but that can make TOC virtual entries misleading.**
  The virtual tree can list entries from master indexes without opening payload `.tre`s, but `winner_node.abspath` alone may not identify the actual containing archive for a TOC entry. `MergedEntry` likely needs source metadata beyond `winner_node`: at least entry `tre_file`, `tree_index`, offset, and node kind. Otherwise Phase 29 may need a data-model refactor.

- **LOW — Plan 01's uv build backend version may be brittle.**
  It pins `uv_build>=0.11.21,<0.12` while the environment says `uv 0.11.7`. That may still resolve from PyPI, but the plan should simply verify the generated backend works under the installed uv, or avoid an exact future-ish lower bound unless confirmed.

- **LOW — Acceptance criteria rely heavily on grep.**
  Grep gates are useful, but several prove presence rather than behavior: tombstone branch grep, bounds-preflight grep, `crc` grep. Plan 04 covers much of this, but Plan 03 should run at least a tiny in-memory/synthetic smoke if fixtures are simple enough, or defer all behavioral claims until Plan 04 wording.

## Suggestions

1. Amend either the roadmap SC#3 or Plan 04's success criteria. Suggested wording: "real `stage/client.cfg` proves scanner precedence and override shadowing; tombstone behavior is proven by portable synthetic archives."

2. Add master-index bounds preflight in Plan 03:
   - SearchTOC: validate declared TOC/name/tree-name block sizes fit within file size before typed reader calls.
   - COT2000: validate magic/header, tree-name block, TOC block, and name block ranges fit within file size.

3. Split canonicalization APIs:
   - `fix_up_file_name(name)`: exact engine transform only.
   - `safe_virtual_key(name) -> str | None`: calls fixup, rejects residual `..`, drive-relative paths, UNC/absolute forms, empty keys if undesired.

4. Make `MergedEntry` carry entry-level source details:
   - `source_path` or archive path, `entry_path`, `offset`, `length`, `compressed_length`, optional `tre_file/tree_index` for TOC-derived entries.
   - This prevents Phase 29 from reworking the model to locate payloads.

5. Define `shadowed` precisely and test that definition. Prefer "later real candidates shadowed by the winner," excluding toc absent entries and tree tombstones.

6. Support legacy bare search keys now:
   - `search(Tree|TOC|Path)(\d+)` with `sku=None` or `sku=-1`.
   - Keep `_NN_` tests as the primary current cfg case.

7. In `os.walk`, prune unsafe dirs before descent:
   - inspect each `dirnames` entry;
   - remove symlink/reparse dirs from `dirnames[:]`;
   - record them in `node_errors`;
   - then sort remaining dirs/files.

## Risk Assessment

**Overall risk: MEDIUM.** The architecture and test strategy are mostly sound, and the corrected single-pass tombstone model is the right direction. The risk is concentrated in two places: the plan currently claims full SC#3 despite narrowing the real-cfg tombstone proof, and the hostile-input hardening is incomplete for `.toc` master indexes. Fix those before execution and the phase risk drops materially.

---

## Cursor Review

## Summary

The revised four-plan set is materially stronger than a typical first draft: isolation boundaries are clear, the per-node-type tombstone distinction is finally correct in prose and pseudocode, the inverse lower-priority tombstone case is named and tested, COT2000 round-trip is mandatory, and merge-layer hardening (`node_errors`, bounds preflight, deterministic `searchPath` walk) is well placed. The plans are executable and likely to produce a useful Phase-28 library. The remaining risks are not in scaffolding or vendoring — they sit in **engine ordering fidelity** (a gap the first review largely missed while praising "stable cfg insertion order"), **fixture complexity for SearchTOC**, and a few deliberate divergences (interior `..` rejection, lazy vs eager `searchPath`) that should be documented as known approximations rather than implied as bit-identical engine behavior.

---

## Strengths

- **D-01/D-02 are concrete and verifiable.** Plan 28-01's zero-runtime-dep `pyproject`, package-local `.gitignore`, committed `uv.lock`, and `integration` marker give Phase 29 a clean base without engine coupling.
- **Tombstone semantics are finally aligned with the engine.** The revised Plan 28-03 single descending pass — `key in claimed or key in tombstoned` *before* the tree `length==0` branch, no `claimed.pop` — matches `TreeFile.cpp:456`'s `!deleted` walk and `SearchTree::localExists` / `SearchTOC::localExists` behavior in `TreeFile_SearchNode.cpp`.
- **Inverse tombstone test closes the highest-confidence round-1 bug.** `test_searchtree_lower_priority_length0_does_not_tombstone_winner` (Plan 28-04) is exactly the case a bulk merge gets wrong.
- **Vendor strategy is disciplined.** Plan 28-02's "verbatim + provenance header + line-251 import rewrite" matches the actual source at `D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py` (confirmed: `TreEntry`/`Cot2000Entry`/`SearchTocEntry` already expose `length` and `compressed_length`; import at ~line 251 is the only `swg_pipeline` reference).
- **Hardening is at the right layer.** Bounds preflight before `read_tre_entries`, per-node try/except → `node_errors`, and regression tests in Plan 28-04 address the DoS seam the vendored parser intentionally leaves open (D-03).
- **SC#3 integration narrowing is honest.** Delegating real-archive tombstone proof to synthetics while proving precedence + override on `stage/client.cfg` is the right call; forging length-0 entries in production `.tre`s would violate read-only posture.
- **Wave ordering is sound.** 28-01 → parser → merge/scanner → tests is the correct dependency chain; Plan 28-03 explicitly deferring behavioral proof to 28-04 is acceptable if executors treat 28-04 as blocking.

---

## Concerns

### HIGH — Same-priority node order does **not** mirror the engine (first review missed this)

**Plan 28-03 Task 1** claims stable sort preserves cfg insertion order within a priority tie, "mirroring the C++ 'insert after the last priority match'" behavior.

That is **only true among nodes of the same kind registered in cfg index order**. The engine does **not** register nodes in cfg file order. In `TreeFile.cpp:118–148`, for each `priority` from `0..maxSearchPriority` it always adds, in order:

1. all `searchPath` entries at that priority
2. all `searchTree` entries
3. all `searchTOC` entries

So at priority *P*, engine order is **path → tree → toc**, regardless of how lines appear in `client.cfg`.

**Impact:** Whitengold's live cfg uses unique priorities (10, 8, 7, 3–0), so the integration test passes anyway. But Plan 28-04's `test_stable_tie_order` and any synthetic fixture with two node **kinds** at the same priority can pass while being **engine-wrong**. This directly challenges TRE-01's "engine-faithful" precedence claim and is the biggest correctness gap still open after round 1.

### HIGH — Plan 28-04 `build_search_toc` is underspecified relative to parser complexity

Plan 28-04 gives a detailed byte layout for `build_cot2000` (magic, header, tree-name block, TOC stride, `fn_len` walk), but `build_search_toc` is mostly "minimal VALID SearchTOC" without the same field-level spec.

The vendored `read_search_toc_entries` (`tre_reader.py:368–407`) uses a **two-phase** TOC/name layout: TOC rows carry an `fn_field` used to walk the name block (`fn_offset += fn_field + 1`), then names are read from a **separate compressed name block** after the TOC slice. A naive "header + flat entries + names" builder will fail round-trip or tempt executors to weaken tests.

**Risk:** SC#1 SearchTOC proof fails on first execution, or tests get "fixed" to match a wrong builder.

### MEDIUM — Deliberate `fix_up_file_name` hardening diverges from engine semantics

Plan 28-03 adds post-canonicalization rejection of interior `..` segments and drive/UNC absolutes into `rejected`. The engine's `fixUpFileName` (`TreeFile.cpp:511–601`) only strips **leading** `../`; it does not reject `a/../../x`.

For a compare tool, skipping hostile paths is reasonable — but it is **not engine-faithful**. A file the client would resolve might be absent from the virtual tree, producing false "missing/changed" signals in Phase 29. This should be an explicit documented tradeoff, not folded into "mirrors TreeFile.cpp exactly."

### MEDIUM — `searchPath` eager walk is a static snapshot, not lazy `exists()`

Plan 28-03 correctly resolves Open-Q1 with deterministic `os.walk`. That is the right choice for a manifest-style compare tool, but it is still an approximation:

- Engine: `SearchPath::exists` checks one path at lookup time (`TreeFile_SearchNode.cpp:109–113`).
- Plan: enumerates all files up front; case collisions on Windows collapse via lowercase keys.

Document as **intentional static model**; do not claim lazy equivalence.

### MEDIUM — Engine nodes omitted from scanner: `searchAbsolute`, `searchCache`, preloads

`TreeFile.cpp:151–181` always adds `searchAbsolute` and `searchCache` (default priority = highest configured + 1) and optional preloaded cache files. None appear in Plan 28-03's scanner grammar or `stage/client.cfg` (verified: no keys present).

For typical whitengold cfgs this may be harmless, but TRE-01 says "engine-faithful" scanning — the virtual tree can never reflect files served only via those nodes. At minimum, document as out-of-scope with a Phase-29 note if real installs use them.

### MEDIUM — Plan 28-03 ships no behavioral tests; entire correctness gate is Plan 28-04

Tasks marked `tdd="true"` but verification is literally `MISSING — Plan 04 provides…`. If fixture builders slip, you get ~two plans of unverified merge logic. Acceptable structurally, but **overall phase risk stays elevated until 28-04 lands**.

### MEDIUM — Bounds preflight is necessary but incomplete

Plan 28-03's `toc_offset + size_of_toc + size_of_name_block <= file_size` catches the worst header lie before slicing. Gaps remain:

- No cap on `number_of_files * stride` vs `size_of_toc` before allocation inside `read_tre_entries`.
- zlib expansion of TOC/name blocks is still unbounded (acceptable for Phase 28 metadata-only path, but threat register should say "partial," not "mitigate" fully).

### MEDIUM — Windows / PowerShell execution friction

Plan 28-01/28-04 verification uses `TRE_COMPARE_INTEGRATION=1 uv run pytest …` (Unix env syntax). The stated environment is PowerShell on Windows. Executors need `$env:TRE_COMPARE_INTEGRATION=1` or `cross-env` — otherwise the "opt-in integration passes" gate may falsely fail in the intended dev environment.

### LOW — Legacy cfg key forms deferred but undersold

Plan 28-03 documents `searchPath10` (no `_NN_`) as deferred. Fine for whitengold, but RESEARCH/Plan 28-03 still describe scanner output as "matching engine precedence" without the caveat that **only the `_NN_` grammar** is implemented.

### LOW — SC#3 "real cfg … tombstone" wording vs delivered proof

Plan 28-04's hybrid interpretation (real cfg → precedence + override; tombstones → synthetic) is reasonable and documented. Orchestrator should accept the narrowing explicitly in ROADMAP/SC#3 text so Phase 28 verification isn't challenged retroactively.

---

## Tombstone / merge-loop deep dive (Plan 28-03 Task 2)

**Verdict: the revised pseudocode is engine-faithful for distinct-priority cases and for the critical inverse case**, assuming node order matches the engine.

| Scenario | Engine `find()` | Revised merge loop |
|----------|-----------------|-------------------|
| High tree real, low tree length-0 | High node wins; low tombstone never consulted | Low hits `key in claimed` guard → skip ✓ |
| High tree length-0, low tree real | High tombstone sets `deleted`; search stops | High tombstones first; low hits `key in tombstoned` ✓ |
| High toc length-0/offset-0, low real | toc absent; low wins | toc `continue` without tombstone; low claims ✓ |
| Low tree length-0 after high real | Engine never sees low | Guard prevents re-tombstone ✓ |

**Ordering cases still at risk:**

1. **Same priority, different kinds** (path vs tree vs toc) — engine type order ≠ cfg order (**HIGH**, above).
2. **Same priority, same kind, two real copies** — engine index order ≈ cfg repeated-key order ✓ (if scanner preserves repeat order).
3. **Same priority, same kind, real then tombstone** — engine: first registered node wins per-path lookup; bulk merge must iterate entries in an order equivalent to "which node would `find()` hit first?" For a single `.tre`, only one entry per path; cross-node same-priority requires **engine registration order**, not cfg line order.
4. **Equal-priority tree-tombstone-before-tree-real in cfg but engine registers tree nodes in index order** — if both are `searchTree_00_P` repeats, cfg/index order likely matches; if kinds differ, it breaks.

**Recommendation:** replace "stable cfg insertion order" with an explicit **`sort_nodes_engine_faithful()`** that sorts by `(-priority, kind_order, cfg_index)` where `kind_order = {path:0, tree:1, toc:2}` and `cfg_index` is discovery order among same kind/priority.

---

## Synthetic fixtures feasibility (Plan 28-04 Task 1)

| Fixture | Feasibility | Notes |
|---------|-------------|-------|
| `build_tre` all five tags | **Feasible** | Extended 8-byte pad for `6000`/`0006` matches `toc_entry_stride()`; `5000` stays 24-byte — correctly specified. |
| `build_cot2000` | **Feasible** | Plan layout matches `read_cot2000_entries` name-block walk and size check at line 328. |
| `build_search_toc` | **Risky as written** | Parser uses compressed blocks + `fn_field` stride in TOC rows; plan lacks parallel detail to COT2000 section. |
| `write_synthetic_cfg` | **Feasible** | Straightforward if archive paths are relative/temp. |

**Additional fixture gap:** no test that `.tre` TOC/name blocks with **compression flags set** round-trip (retail archives often use zlib). Uncompressed builders prove layout understanding but not the decompression path the merge actually exercises via `read_tre_entries`.

---

## D-01 extractability

No material leaks found in the plan set:

- Engine imports forbidden and grep-gated.
- Integration test's absolute paths are gated and skip-by-default (accepted tradeoff).
- Vendoring path correctly points at `swg-blender-plugin`, not stale `swg-tools`.

**Minor:** committed `test_integration.py` embeds `D:/Code/swg-client-v2/stage/client.cfg` — fine under D-07, but extractors should treat it as dev-only baggage.

---

## Suggestions

1. **Fix node ordering (Plan 28-03 Task 1):** After parsing, sort by `(-priority, kind_rank, seq)` with `kind_rank = {"path": 0, "tree": 1, "toc": 2}` and `seq` = cfg encounter index within `(priority, kind)`. Replace `test_stable_tie_order` with `test_same_priority_kind_order_path_before_tree_before_toc`.

2. **Expand `build_search_toc` spec (Plan 28-04 Task 1):** Mirror the COT2000 detail level — document `SEARCH_TOC_HEADER_FMT`, tree-name block after header, TOC row `fn_field` semantics, compressed name block placement, and add a one-liner round-trip acceptance like COT2000's.

3. **Document approximation boundary in `virtual_tree.py` module docstring:** eager `searchPath` walk, interior-`..` rejection, omitted `searchAbsolute`/`searchCache`.

4. **Add same-priority cross-kind merge test (Plan 28-04):** e.g. priority-5 `searchTOC` real + `searchTree` real for same path — assert tree wins (engine order), even if cfg lists toc first.

5. **Tighten bounds preflight (Plan 28-03):** Also require `number_of_files * stride <= size_of_toc` (and cap `number_of_files` to a sane max) before calling `read_tre_entries`.

6. **PowerShell-friendly verify lines:** Document `$env:TRE_COMPARE_INTEGRATION=1; uv run pytest -m integration` in Plan 28-04 Task 3 verification.

7. **Optional same-priority tombstone pair:** Plan 28-04 lists same-priority real-then-tombstone as "optional if cheap" — worth adding once kind-order is fixed; it catches ordering regressions.

8. **Orchestrator sign-off:** Explicitly amend SC#3 success criterion text to match the hybrid real/synthetic tombstone proof split.

---

## Risk Assessment

**Overall risk: MEDIUM**

**Justification:** Scaffold, vendoring, and the core tombstone algorithm (after round-1 fixes) are solid; the phase will likely boot and run green tests. Remaining MEDIUM-HIGH execution risk concentrates in (a) **wrong same-priority cross-kind ordering** — subtle, untested, and incorrectly described as a strength in round 1, (b) **SearchTOC fixture/builder fragility**, and (c) **Plan 28-03 having no behavioral tests until 28-04**. None of these are plan-killers, but without the kind-order fix the tool can pass its own suite while disagreeing with the client on realistic cfgs that reuse a priority across path/tree/toc — exactly the override patterns Phase 29's diff will depend on.

*Reviewer stance: independent second pass; verified claims against `TreeFile.cpp`, `TreeFile_SearchNode.cpp`, `stage/client.cfg`, and `swg-blender-plugin/swg_pipeline/tre_reader.py` in-repo/on-disk.*

---

## Consensus Summary

Both reviewers independently rate the revised plan set **MEDIUM** overall risk and agree the
foundation is sound: D-01 isolation is concrete and grep-verifiable, the vendoring strategy is
disciplined, and — critically — the round-1 merge-loop / per-node-type tombstone fix is now
engine-faithful (Cursor verified the four distinct-priority + inverse cases against
`TreeFile.cpp`/`TreeFile_SearchNode.cpp`). The remaining risk is in execution detail, not
architecture.

### Agreed Strengths (raised by both)

- **D-01/D-02 extractability** — own pyproject/lockfile/.gitignore, zero engine imports, grep-gated; vendoring correctly points at `swg-blender-plugin` (not the stale `swg-tools`).
- **Corrected single-descending-pass tombstone model** — `key in claimed or key in tombstoned` guard *before* the tree length-0 branch, `claimed.pop` deleted; the right model for first-match-wins.
- **Inverse tombstone test** (`test_searchtree_lower_priority_length0_does_not_tombstone_winner`) closes the highest-confidence round-1 bug.
- **`node_errors` diagnostics + bounds-preflight at the merge boundary** are well-placed for the DoS seam the vendored parser leaves open.
- **Synthetic-default / env-gated-real test split** is the right corpus strategy.

### Agreed Concerns (raised by both — highest priority)

1. **SC#3 wording mismatch (Codex HIGH / Cursor LOW).** Plan 04 narrows the ROADMAP SC#3 "real `stage/client.cfg` … tombstone" clause to synthetic proof but does not amend the controlling requirement. **Both demand an explicit ROADMAP/SC#3 amendment** (suggested wording: "real cfg proves precedence + override shadowing; tombstone behavior proven by portable synthetic archives") so Phase-28 verification isn't challenged retroactively. *This is the single most-agreed action item.*

2. **SearchTOC / master-index handling is the weak spot (Codex HIGH / Cursor HIGH).** Two angles on the same area:
   - **Codex:** the bounds-preflight covers `.tre` only — apply the same `file_size`-bound discipline to SearchTOC + COT2000 headers before the typed readers run (hostile `.toc` can still force large reads).
   - **Cursor:** `build_search_toc` is underspecified vs `build_cot2000` — the parser uses a two-phase TOC + `fn_field`-walked separate name block; a naive builder fails round-trip or tempts executors to weaken the SC#1 SearchTOC test.

3. **`fix_up_file_name` conflates two jobs (both MEDIUM).** The engine routine only strips *leading* `../`; the plan folds interior-`..`/absolute rejection into the function named after the engine. **Both recommend splitting** into a verbatim `fix_up_file_name()` + a separate `safe_virtual_key()`/`is_safe_virtual_key()` wrapper, and documenting the rejection as a deliberate, non-engine-faithful tradeoff.

4. **Legacy bare-priority key grammar deferred (Codex MEDIUM / Cursor LOW).** Only the `_NN_` form is implemented; the bare `searchPath10` form is engine-real. Both note it undercuts the "engine-faithful" language; Codex pushes to support both now (cheap), Cursor accepts deferral if explicitly caveated.

### Divergent / Single-Reviewer Views (worth investigating)

- **[Cursor, HIGH — standout finding] Same-priority cross-kind node ordering.** The engine registers nodes per-priority in fixed kind order **path → tree → toc**, NOT cfg line order (`TreeFile.cpp:118–148`). The plan's "stable sort preserves cfg insertion order" is only correct for same-*kind* ties. Live whitengold cfg uses unique priorities so the integration test passes, but any cfg reusing a priority across kinds would diverge from the client — and round 1 praised this as a strength. **Fix:** sort by `(-priority, kind_rank={path:0,tree:1,toc:2}, cfg_seq)` and add a `test_same_priority_kind_order_path_before_tree_before_toc`. *Codex did not catch this; it deserves explicit adjudication before execution.*
- **[Cursor, MEDIUM] `searchAbsolute`/`searchCache`/preload nodes omitted** from the scanner grammar (`TreeFile.cpp:151–181`). Absent from the live cfg, but document as out-of-scope.
- **[Cursor, MEDIUM] PowerShell env-var friction** — verify lines use Unix `VAR=1 cmd`; the dev env is PowerShell (`$env:VAR=1; cmd`). Practical: the integration gate could falsely fail in the intended environment.
- **[Cursor, MEDIUM] Compression-flag fixture gap** — no fixture exercises a zlib-compressed TOC/name block, so the decompression path `read_tre_entries` actually uses is unproven.
- **[Codex, MEDIUM] Shadow-recording semantics inconsistent** between Plan 03 pseudocode ("any later node when `key in claimed`"), Plan 03 prose ("only a later REAL copy"), and Plan 04's inverse test (allows the lower length-0 node in `shadowed`). Pick one definition and test it.
- **[Codex, MEDIUM] `MergedEntry` lacks entry-level source metadata** (`tre_file`, `tree_index`, offset) — `winner_node.abspath` alone may not identify the containing archive for a TOC-derived entry, risking a Phase-29 data-model refactor.
- **[Codex, MEDIUM] `os.walk` dir pruning** — symlink/reparse dirs must be removed from `dirnames[:]` *before* descent (not skipped per-entry after) to avoid junction loops/escapes. (Sharpens Cursor's MEDIUM on the same walk.)
- **[Codex, LOW] `uv_build>=0.11.21,<0.12` lower bound** vs installed `uv 0.11.7` — verify the generated backend resolves/works under the installed uv.
- **[Codex, LOW] grep-heavy acceptance criteria** prove presence not behavior (esp. Plan 03, whose behavioral proof is fully deferred to Plan 04).

### Recommended pre-execution actions (synthesis)

Highest-leverage fixes both reviews point toward, before `/gsd:execute-phase`:
1. **Amend ROADMAP SC#3** to the hybrid wording (the one item both flag and the only requirement-level mismatch).
2. **Adjudicate + likely fix the same-priority kind-order rule** (Cursor HIGH) — `(-priority, kind_rank, cfg_seq)` sort + a cross-kind ordering test; or, if accepted as out-of-scope for unique-priority cfgs, document it as a known approximation.
3. **Strengthen SearchTOC handling** — field-level `build_search_toc` spec (Cursor) + master-index bounds preflight for `.toc`/COT2000 (Codex).
4. **Split `fix_up_file_name` from the safety wrapper** and document the traversal-rejection as a deliberate non-engine divergence.
5. **Pin down `shadowed` semantics** and make Plan 04 assert exactly that definition.

To incorporate: `/gsd:plan-phase 28 --reviews`
