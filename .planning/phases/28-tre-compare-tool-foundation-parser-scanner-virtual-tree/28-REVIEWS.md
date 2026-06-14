---
phase: 28
reviewers: [codex, cursor]
reviewed_at: 2026-06-14T17:35:58Z
plans_reviewed: [28-01-PLAN.md, 28-02-PLAN.md, 28-03-PLAN.md, 28-04-PLAN.md]
self_skipped: claude (running inside Claude Code — skipped for independence)
unavailable: [gemini, coderabbit, opencode, qwen]
---

# Cross-AI Plan Review — Phase 28 (TRE Compare Tool — Foundation)

Two independent external reviewers (OpenAI Codex, Cursor Agent) reviewed the four-plan set.
Claude was skipped for independence (this review ran inside Claude Code). Gemini, CodeRabbit,
OpenCode, and Qwen are not installed on this machine.

Both reviewers converged hard on the same top-3 findings — the merge-loop tombstone ordering
bug, the missing inverse tombstone test, and the COT2000 SC#1 hedge. That agreement makes
those the highest-confidence action items.

---

## Codex Review

## Summary

The plan set is directionally strong and correctly identifies the highest-risk semantics: isolated packaging, repeated cfg keys, priority order, first-hit-wins, `fixUpFileName`, and the tree-vs-toc tombstone split. The main problem is that several success criteria are deferred to tests in 28-04 while the implementation in 28-03 is underspecified enough that a plausible implementation can pass smoke tests and still be engine-wrong. The biggest risks are merge-loop ordering around tombstones, incomplete COT2000 coverage, parser/fixture mismatch from vendoring code the planner has not fully characterized, and `searchPath` eager walking behaving unlike the engine in edge cases.

## Strengths

- Strong isolation boundary: separate `pyproject`, lockfile, venv, no engine imports, no build graph coupling. This directly supports D-01/D-02.
- Correctly rejects `configparser`; repeated `search*` keys are a real cfg grammar requirement.
- Priority sort is explicitly higher-first and stable, matching the engine's "priority desc, insertion order within tie" behavior.
- The per-node-type tombstone distinction is explicitly called out and named in tests, which is good because it is the easiest correctness nuance to flatten accidentally.
- Deferring full API/UI work keeps Phase 28 focused on backend semantics.
- Synthetic byte-built fixtures are the right portability move; real install tests are correctly env/marker gated.
- Security posture is at least acknowledged: malformed archives skipped, path traversal canonicalization, no default real-install dependency.

## Concerns

- **HIGH: Tombstone merge algorithm is underspecified and easy to implement wrong.**
  The plan says tree `length==0` "add to tombstoned + skip (lower copies hit the guard)", but does not explicitly say "only if the path has not already been claimed." In engine search order, a lower-priority tree tombstone must not erase a higher-priority real file already found. Same for a later same-priority tombstone after an earlier same-priority real entry. The virtual-tree loop must behave like lookup order, not like a global postpass.

- **HIGH: High-priority tree tombstone must remove lower-priority real copies without relying on `claimed.pop` ambiguity.**
  If implementation ever claims lower entries first or mutates `entries` after seeing tombstones, it can contradict first-hit-wins. The safest model is single descending pass: if path already in `entries` or `tombstoned`, skip; if tree tombstone first encountered, put in `tombstoned`; if real first encountered, put in `entries`.

- **HIGH: SearchTOC tombstone/absent behavior must also respect prior claims.**
  For TOC `length==0` or `offset==0`, "absent here, keep searching" means skip only when encountered before a lower real copy. But if a higher real copy was already claimed, the lower TOC absent entry must not be recorded as shadowing or affecting anything. The plan should make that explicit.

- **HIGH: COT2000 coverage is not acceptable if Plan 04 only documents detector coverage.**
  SC#1 says COT2000/SearchTOC are read without rewriting. A hedge like "if builder is impractical, assert detector path" does not prove parsing works. That leaves a stated success criterion unverified.

- **HIGH: Plan 02's "copy verbatim plus exactly two edits" may conflict with the desired public API.**
  The plan requires dataclasses exposing both `length` and `compressed_length`. If the vendored parser does not already expose exactly that snake_case surface, adding aliases/wrappers is more than the allowed edits. This should be resolved before execution, not discovered in 28-04.

- **HIGH: Synthetic fixtures may not match parser arithmetic.**
  The fixture plan uses expected struct formats and strides, but admits the planner has not fully read the vendored parser's arithmetic. That is a direct risk to 28-04: tests could either fail for fixture bugs or, worse, be adjusted to the fixture instead of validating the parser.

- **MEDIUM: Parser hardening is deferred too far.**
  Plan 02 accepts malformed-count/OOM risk and says Plan 03 merge layer mitigates it. But Plan 03 still calls parser entry readers. If those entry readers allocate or seek based on attacker-controlled counts before returning, wrapping exceptions in the merge layer is too late.

- **MEDIUM: `searchPath` eager `os.walk` is a semantic approximation, not engine-faithful.**
  The engine resolves paths lazily at lookup time. Eager walking builds a snapshot, can be expensive, can hit permissions/races, and can introduce case-collision behavior that depends on filesystem traversal order. This may be fine for a compare tool, but it should be documented as a deliberate approximation with tests for deterministic ordering.

- **MEDIUM: Windows path security is under-specified.**
  Rejecting residual `..` and drive/UNC absolute paths after canonicalization is good, but `fixUpFileName` strips leading `../` by design. That means hostile archive paths like `../../foo` become `foo`; maybe engine-faithful, but security-sensitive. The plan should separate "engine canonical name" from "archive path rejected for suspicious source spelling."

- **MEDIUM: `os.walk` symlink/junction escape guard is weak.**
  `relpath` not starting with `..` does not fully handle Windows reparse points, junctions, or case-normalized path escapes. If following symlinks is disabled, say so and test it; if junctions are possible, skip or resolve them deliberately.

- **MEDIUM: Scanner grammar may be too narrow.**
  Regex only matches lowercase-style `searchTOC|Tree|Path`. If engine cfg keys are case-insensitive or allow whitespace around section names/keys differently, tests against one `stage/client.cfg` will not prove general correctness. Also inline comments and semicolon comments are not mentioned.

- **MEDIUM: `maxSearchPriority` and `TOCTreePath` are parsed but not validated against engine behavior.**
  The plan captures them, but does not say whether they affect ordering, node inclusion, or TOC path resolution. If `TOCTreePath` is needed to locate `.toc` entries or master index files, Plan 03 may be incomplete.

- **MEDIUM: "Malformed archive node skipped" can hide correctness failures.**
  Skipping bad archives is reasonable for robustness, but tests need to assert diagnostics include which node failed and why. Otherwise a fixture/parser mismatch can silently produce a partial tree.

- **LOW: Python 3.14.4 with `requires-python >=3.11` may create lock portability noise.**
  If `uv.lock` is generated under 3.14, verify it does not implicitly exclude 3.11/3.12 users. For a copy-paste-extractable tool, this matters.

- **LOW: Scaffold with `uv_build` may be unnecessary coupling to uv internals.**
  It is acceptable because D-05 chooses uv, but if copy-paste portability matters, confirm the build backend is captured and installable from a fresh environment.

## Suggestions

- Specify the virtual merge loop as executable pseudocode:

  ```python
  for node in nodes_desc_stable:
      for e in iter_node_entries(node):
          key = fix_up_file_name(e.path)

          if key in entries or key in tombstoned:
              record_shadow_or_skip(...)
              continue

          if node.kind == "tree" and e.length == 0:
              tombstoned.add(key)
              continue

          if node.kind == "toc" and (e.length == 0 or e.offset == 0):
              continue

          entries[key] = MergedEntry(...)
  ```

  The critical invariant: only the first encountered record for a canonical path can decide visibility.

- Add explicit tests for lower-priority tombstones:
  - higher tree real, lower tree tombstone -> real remains.
  - higher tree tombstone, lower tree real -> absent globally.
  - same priority earlier real, later tombstone -> real remains.
  - same priority earlier tombstone, later real -> absent globally.

- Remove the COT2000 hedge. Either build a real minimal COT2000 fixture matching the parser or mark SC#1 not satisfied. Detector-only coverage is not enough.

- Before accepting Plan 02, inspect the actual vendored parser surface and update the plan if wrappers are needed. Do not pretend "verbatim + two edits" is compatible with API reshaping unless the source already matches.

- Move minimal parser bounds tests into 28-02 or 28-03. At least cover absurd file counts, negative/overflow offsets, and short headers before the virtual-tree layer depends on parser safety.

- Make `searchPath` deterministic:
  - sort directory names and filenames before yielding.
  - do not follow symlinks by default.
  - record skipped unreadable paths.
  - add a case-collision policy for Windows, since canonical keys lowercase names.

- Clarify `TOCTreePath` semantics before 28-03. If TOC discovery depends on it, treating `.toc` nodes as directly readable paths may be incomplete.

- Add acceptance greps for isolation beyond imports: no references to `D:/Code/swg-client-v2`, no relative imports outside `tre_compare`, no engine `sys.path` mutation, no build files touched outside `tools/tre-compare`.

- Make diagnostics part of `VirtualTree`: skipped malformed nodes should include node path, kind, exception type, and message. Silent skip is too opaque for a compare tool.

## Risk Assessment

**Overall risk: MEDIUM-HIGH.** The phase is scoped correctly and the split into four waves is sensible, but the correctness burden is concentrated in 28-03/28-04 and several hard requirements are not nailed down before implementation. The biggest risk is not scaffolding or isolation; it is producing a virtual tree that looks plausible but differs from engine lookup semantics in tombstone ordering, TOC absence handling, or eager `searchPath` behavior. COT2000 is the clearest outright gap: detector-only testing would fail the stated success criteria.

---

## Cursor Review

## 1. Summary

The four-plan wave structure is coherent and well-researched: isolation (D-01/D-02), vendoring (D-03/D-04), engine semantics from `TreeFile.cpp` / `TreeFile_SearchNode.cpp`, and the per-node-type tombstone distinction are largely correct and testable in principle. The biggest adversarial finding is a **merge-loop ordering bug** in Plan 28-03: processing *all* entries from *all* archives is not the same as the engine's per-request `find()` walk, so a **lower-priority SearchTree length-0 can incorrectly global-tombstone a path already won at higher priority** — and the planned test suite does not cover that inverse case. Secondary gaps: SC#3's integration test promises real-cfg tombstone/override proof but only checks node ordering; COT2000 byte-round-trip is explicitly optional; fixture builders must match vendored parser arithmetic (especially 32-byte TOC stride padding) without a verified spec in the plan; parser DoS hardening is deferred while Plan 02's must-haves claim bounded reads.

## 2. Strengths

- **Isolation architecture is tight.** Zero runtime deps, uv lockfile, package-local `.gitignore`, provenance headers, single import rewrite, and grep-based acceptance gates are concrete and enforce D-01/D-02 extractability.
- **Engine semantics research is strong.** Priority sort (`higher number first`, stable ties), `fixUpFileName` steps, `!deleted` loop guard, and the SearchTree vs SearchTOC tombstone split are verified against `TreeFile_SearchNode.cpp:397-400` (tree sets `deleted=true`) and `:809-817` (toc returns false, never sets deleted).
- **Per-node-type tombstone is named, branched, and test-mapped.** Plan 04 requires `test_searchtoc_length0_does_NOT_shadow` and `test_searchtree_length0_tombstone_removes_globally` — the right split for Pitfall #1.
- **Scanner design matches real cfg.** Hand parser (not `configparser`), repeated keys, quote strip, cfg path parameter, and the live `stage/client.cfg` block (path@10 → tree@8/7 → toc@3..0) align with `TreeFile.cpp:117-143`.
- **searchPath eager `os.walk` is a defensible static-model choice.** Open-Q1 is resolved with documentation and `test_searchpath_override_shadows_lower`; required for a compare manifest, even though it diverges from lazy engine `exists()`.
- **Wave ordering is logical.** Scaffold → vendor → implement → prove; deferring behavioral tests to 28-04 is acceptable if the phase gate is 04, not 03.
- **Threat model is unusually honest.** Path-traversal rejection beyond engine `fixUpFileName`, per-node skip on malformed archives, and integration path containment are thought through.

## 3. Concerns

### Tombstone / merge correctness

- **HIGH — Merge pseudocode can mis-tombstone on lower-priority tree length-0.** Plan 28-03 processes the tree tombstone branch *before* checking `key in claimed`, and includes `claimed.pop(key, None)`. Engine `find()` stops at the first hit along priority order; it never consults a lower-priority tree tombstone if a higher node already serves the file. In a bulk merge, a lower-priority `.tre` length-0 entry for a path already claimed at priority 8 would still run the tombstone branch and remove the winner. The prose says lower-priority tombstones must not un-claim winners; the pseudocode contradicts it.
- **HIGH — Missing negative tombstone test.** `test_searchtree_length0_tombstone_removes_globally` only covers *high-priority* tree tombstone shadowing lower real copies. There is no test for: high-priority real entry + lower-priority tree length-0 → entry must **remain** and must **not** be in `tombstoned`.
- **MEDIUM — `claimed.pop` comment is self-contradictory.** Comment says "also remove an already-claimed lower? NO" but `pop` removes whatever is claimed, including a higher-priority winner if encountered later. Executor confusion risk.

### SC#3 / integration coverage

- **HIGH — Integration test does not prove SC#3 tombstone or override on real cfg.** Success criterion #3 requires unit tests against real `stage/client.cfg` proving override shadowing and tombstone behavior. Plan 28-04's integration test only asserts `nodes[0]` is path@10, `nodes[-1]` is toc@0, and `len(vt.entries) > 0`, with a **comment stub** for override shadowing and **zero** tombstone assertion. Synthetic tests satisfy D-06; they do not satisfy the "real stage/client.cfg" tombstone/override clause unless integration is strengthened or SC#3 is reinterpreted.
- **MEDIUM — D-07 "exactly one" test is a scan smoke, not a behavioral proof.** Acceptable for portability, but the phase success criteria oversell what the gated test delivers.

### COT2000 / parser coverage

- **MEDIUM — COT2000 round-trip is hedged, not guaranteed.** Plan 28-04: "if a COT2000 builder is impractical… assert the detector path and document." SC#1 and Plan 28-02 must-haves require reading COT2000 without rewrite. `detect_master_index_kind` + `read_cot2000_entries` exist in the vendored parser, but **detector-only ≠ reading**. Whitengold's live cfg uses SearchTOC `.toc` files; COT2000 gaps may not bite immediately but fail SC#1 literally.
- **MEDIUM — Extended TRE versions (6000/0006/5000) fixture risk.** Parser uses `TOC_ENTRY_FMT` (24 bytes) with `toc_entry_stride` 32 for extended tags — extra 8 bytes per entry are stride padding. Plan 28-04's struct block does not explicitly require zero-padding those 8 bytes per entry. `test_parse_each_version_tag` may fail on first execution if builders only pack 24-byte rows.
- **MEDIUM — Plan 28-02 must-haves vs threat model conflict.** Must-have claims "bounded reads" (T-28-02-01 mitigate); threat register says hardening is deferred to Plan 03 merge wrapper. Vendored `read_tre_entries` slices `data[toc_offset:toc_offset+size_of_toc]` and decompresses without pre-checking against `len(data)` — hostile headers can cause large reads/OOM. Plan 03's `try/except` around node enumeration catches `TreFormatError`/`struct.error`/`OSError`, not `MemoryError` or hang in zlib bomb paths (payload path deferred, but TOC/name-block zlib is still used in merge).

### searchPath / scanner edge cases

- **MEDIUM — Eager `os.walk` semantic divergence needs more edge-case tests.** No tests for: missing override dir (should yield nothing, not fail), empty override dir, nested subdirs, or Windows path casing (`fix_up_file_name` lowercases keys; `os.walk` paths may not match on case-insensitive FS). Documented in docstring but untested.
- **LOW — Scanner regex requires `_NN_` SKU segment.** Engine legacy keys use `searchPath10` when `skuText` is empty (`TreeFile.cpp:123`). Live `stage/client.cfg` uses `_00_` form — fine for this project, but not fully grammar-faithful.
- **LOW — `maxSearchPriority` captured but unused.** Engine uses it to bound the priority loop (`TreeFile.cpp:118`). Scanner only emits configured keys — probably OK for whitengold, but missing keys at "gaps" in priority 0..12 are not modeled.

### Process / dependency risks

- **MEDIUM — Plan 28-03 marked `tdd="true"` with no automated behavioral verify.** Implementation ships in wave 3; correctness proof waits for wave 4. A bad merge in 03 is invisible until 04 — acceptable only if phase completion is gated on 04, not 03.
- **LOW — Python 3.14.4 on dev machine.** `requires-python >=3.11` is fine; `.python-version` from `uv init` may pin 3.14 and surprise contributors on 3.11/3.12 CI unless documented.
- **LOW — CONTEXT.md still states blanket "length-0 tombstone removes lower copy"** without per-node-type nuance — downstream agents reading CONTEXT before RESEARCH could regress.

## 4. Suggestions

1. **Fix merge loop ordering for tree tombstones.** Before tombstoning, require `key not in claimed` (or only tombstone when the tombstone node's priority is strictly higher than `claimed[key].winner_node.priority`). Remove `claimed.pop` entirely; rely on `tombstoned` set + `if key in tombstoned: continue` for lower nodes. Match engine: tombstone only affects search when that node is reached *before* a winner is found.

2. **Add `test_searchtree_lower_priority_length0_does_not_tombstone_winner`.** High-priority `.tre` with real `foo/x.dds`, low-priority `.tre` with length-0 same path → assert winner remains, path not in `tombstoned`, lower node in `shadowed` or ignored.

3. **Strengthen integration test or narrow SC#3 wording.** If real-install proof is required: when `stage/override` exists, pick a known override file and assert `winner_node.kind == "path"` and priority 10. Tombstone on real archives is harder (needs a known length-0 entry); either document that tombstone proof is synthetic-only or add a targeted assertion against a known asset in `disable_wayfar_dearic_snow.tre` if present.

4. **Make COT2000 builder mandatory or downgrade SC#1.** Minimal `build_cot2000()` with magic ` COT2000`, one tre name, one entry, and round-trip via `read_cot2000_entries` — parser layout is in `tre_reader.py:264-332`. If skipped, SC#1 should say "SearchTOC + COT2000 detect/route" not "read without rewrite."

5. **Document extended TOC fixture layout explicitly in 28-04 Task 1.** Per entry: `struct.pack(TOC_ENTRY_FMT, ...)` + `b"\x00" * 8` when `toc_entry_stride(version) == 32`. Add a builder self-test that parametrizes all five tags before the full suite.

6. **Add parser bounds preflight in virtual_tree (or thin wrapper).** Before calling `read_tre_entries`, check `toc_offset + size_of_toc + size_of_name_block <= file_size` from header read — satisfies T-28-02-01 at the merge boundary without rewriting vendored parser.

7. **Record per-node parse errors on `VirtualTree`.** Plan 03 mentions "per-node error list" in threat text but `VirtualTree` only has `rejected`. Surface `node_errors: list[tuple[SearchNode, str]]` so malformed-archive skips are observable in tests and Phase 29 UI.

8. **Add searchPath edge tests in 28-04.** `test_searchpath_missing_dir_yields_nothing`, `test_searchpath_empty_dir`, optional `test_searchpath_nested_subdirs`.

9. **Clarify Plan 28-02 must-haves.** Change "bounded reads" to "no *additional* unbounded reads on vendor; merge-layer bounds in 03."

## 5. Risk Assessment

**Overall: MEDIUM-HIGH**

| Area | Level | Justification |
|------|-------|----------------|
| Tombstone merge correctness | **HIGH** | Pseudocode bug + missing inverse test; core SC#3 nuance |
| SC#3 / integration alignment | **HIGH** | Real-cfg test undershoots stated success criteria |
| Fixture/parser alignment | **MEDIUM** | Byte builders depend on unread-at-plan-time arithmetic; extended stride padding unspecified |
| COT2000 coverage | **MEDIUM** | Explicit escape hatch vs SC#1 |
| Isolation / vendoring | **LOW** | Well-specified, grep-verifiable |
| searchPath eager walk | **LOW-MEDIUM** | Intentional, tested synthetically; minor edge gaps |
| Security (DoS) | **MEDIUM** | Acknowledged, partially mitigated; TOC zlib still on merge path |

**Verdict:** The plan set is **above average** for research depth and engine fidelity, and will likely produce a usable Phase 28 foundation **if** the tombstone merge ordering is fixed before or during 28-03 execution and 28-04 adds the missing negative tombstone test. Without those fixes, the tool can silently produce wrong virtual trees on realistic archive combinations (override + disable TRE + sku TOCs), which is exactly the failure mode the phase exists to prevent.

**Per-node-type tombstone (direct answer):** SearchTOC skip-only semantics are **correctly specified and testable**. SearchTree global-remove is **correct for high-priority tombstones** but **not correctly implemented** for the bulk-merge model unless lower-priority length-0 entries are gated on `key not in claimed`. The `claimed.pop` line is the red flag.

**Dependency ordering:** 01→02→03→04 is sound; the critical gate must remain **28-04 green**, not 28-03 smoke imports alone.

---

## Consensus Summary

Both reviewers rate the set **MEDIUM-HIGH risk**: well-researched, isolation/vendoring solid,
but the correctness burden concentrates in 28-03/28-04 and a real merge bug is latent in the
plan's own pseudocode.

### Agreed Strengths (2+ reviewers)
- **Isolation boundary is tight and grep-verifiable** — zero runtime deps, lockfile, package-local `.gitignore`, provenance headers, single import rewrite (D-01/D-02). (codex, cursor)
- **Engine-semantics research is strong** — higher-priority-first stable sort, `fixUpFileName` steps, `!deleted` guard, the tree-vs-toc tombstone split, all traced to `TreeFile*.cpp`. (codex, cursor)
- **Per-node-type tombstone is named and test-mapped** — the right split for the #1 nuance. (codex, cursor)
- **Hand parser (not configparser) + repeated-key handling + cfg-path parameter** matches the real cfg grammar. (codex, cursor)
- **Synthetic byte-built corpus + env-gated real-install test** is the right portability posture. (codex, cursor)

### Agreed Concerns (2+ reviewers — highest priority)
1. **[HIGH] Merge-loop tombstone ordering bug.** The 28-03 pseudocode runs the tree `length==0` branch (and a `claimed.pop(key, None)`) *before* the `key in claimed` check — so a **lower-priority** SearchTree length-0 can wrongly global-tombstone a path already won at higher priority. The prose says the opposite of what the pseudocode does. Both reviewers call `claimed.pop` the red flag and both prescribe the same fix: single descending pass, gate tombstoning on `key not in claimed and key not in tombstoned`, drop `claimed.pop`. (codex HIGH×2, cursor HIGH + MEDIUM)
2. **[HIGH] Missing inverse/negative tombstone test.** 28-04 only tests high-priority tree tombstone removing lower copies; there is NO test for high real + lower length-0 → must remain. This is exactly the case the bug above would break. Both reviewers give the explicit test matrix. (codex, cursor)
3. **[HIGH/MEDIUM] COT2000 hedge contradicts SC#1.** 28-04's "if a COT2000 builder is impractical, assert the detector path and document" makes detector-only coverage possible, but SC#1 + 28-02 must-haves require *reading* COT2000 without rewrite. Detector ≠ read. Remove the hedge (build a minimal COT2000 fixture — cursor even cites the parser layout `tre_reader.py:264-332`) or formally narrow SC#1. (codex, cursor)
4. **[HIGH/MEDIUM] Integration test undershoots SC#3.** SC#3 wants real-`stage/client.cfg` proof of override shadowing AND tombstone; the gated test only asserts node ordering + `len(entries)>0` with a comment stub. Either strengthen it (assert a known override file's winner is the path node) or reinterpret SC#3 as synthetic-proven. (cursor HIGH; codex notes the same undersell via the "malformed skip can hide failures" + diagnostics thread)
5. **[MEDIUM] Fixture/parser arithmetic mismatch risk.** Builders in 28-04 are specified from the layout block, but the planner has not fully read the vendored parser's exact offset/stride arithmetic — risk that tests fail for fixture bugs or get bent to match the fixture instead of validating the parser. Cursor adds the concrete 32-byte extended-stride padding gap. (codex, cursor)
6. **[MEDIUM] Parser DoS hardening deferred too far.** 28-02 must-haves claim "bounded reads" but push hardening to 28-03's merge `try/except`, which is too late if `read_tre_entries` allocates/seeks on attacker-controlled counts first (and won't catch `MemoryError`/zlib bombs on the TOC/name-block path still used in merge). Add a bounds preflight at the merge boundary. (codex, cursor)
7. **[MEDIUM] searchPath eager `os.walk` edge cases under-tested.** Deliberate static-model divergence from the lazy engine is fine, but needs deterministic ordering (sort), no-follow-symlinks default, and tests for missing/empty/nested dirs + Windows case-collision. (codex, cursor)

### Divergent / Unique Views (worth a look, one reviewer each)
- **Codex only:** Plan 02's "verbatim + two edits" may conflict with the required dual-length snake_case API surface — resolve by inspecting the actual vendored surface *before* execution, not in 28-04. Also: Windows reparse-point/junction escape beyond `relpath`; `uv_build` backend portability from a fresh env; lock portability under 3.14; add isolation greps for absolute repo paths / sys.path mutation / build-file touches.
- **Cursor only:** `VirtualTree` should expose `node_errors` (Plan 03's threat text mentions a per-node error list but the dataclass only has `rejected`); scanner regex requires `_NN_` SKU and won't match engine legacy `searchPath10` (empty skuText) form; `maxSearchPriority`/`TOCTreePath` captured but their engine roles (loop bound / `.toc` path resolution) not modeled — `TOCTreePath` may be needed to *locate* `.toc` entries; CONTEXT.md still carries the blanket "length-0 removes lower copy" statement that contradicts the per-node nuance (downstream-regression trap).

### Recommended action before/during execution
The two HIGH consensus items are cheap to fix in-plan and high-value:
- **28-03:** rewrite the merge pseudocode to the single-descending-pass form (gate tombstone on `key not in claimed`, delete `claimed.pop`, drop the contradictory comment).
- **28-04:** add the inverse tombstone test + make the COT2000 fixture mandatory (or renegotiate SC#1 wording) + strengthen the integration assertion or explicitly scope SC#3 to synthetic proof.
- **28-02:** soften the "bounded reads" must-have wording and/or pull a minimal bounds preflight forward.

To incorporate this feedback into planning:
  `/gsd:plan-phase 28 --reviews`
