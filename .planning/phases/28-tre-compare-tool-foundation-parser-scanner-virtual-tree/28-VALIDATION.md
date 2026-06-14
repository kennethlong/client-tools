---
phase: 28
slug: tre-compare-tool-foundation-parser-scanner-virtual-tree
status: planned
nyquist_compliant: true
wave_0_complete: true
created: 2026-06-14
revised: 2026-06-14
---

# Phase 28 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.
> Revised 2026-06-14 (round-2 cross-AI review incorporation): added tests for engine-faithful
> same-priority cross-kind ordering (#1), the safe_virtual_key split (#3), the `.toc`/COT2000
> bounds preflight (#2a), the bare-priority scanner grammar (#6), and the zlib-compressed-block
> fixture (#10).
> Revised 2026-06-14 (user decision on SC#3 — FULFILLED, not narrowed): added the cfg-driven
> END-TO-END tombstone test (`test_tombstone_end_to_end_via_cfg`) that drives the real
> `parse_shared_file → build_virtual_tree` pipeline over purpose-built genuine on-disk TRE/TOC
> archives wired through a live-modeled `client.cfg`. ROADMAP SC#3 text is UNCHANGED.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | pytest (resolved + pinned in `tools/tre-compare/uv.lock` at scaffold — Plan 01) |
| **Config file** | `tools/tre-compare/pyproject.toml` `[tool.pytest.ini_options]` (registers the `integration` marker; created in Plan 01) |
| **Quick run command** | `cd tools/tre-compare && uv run pytest -m "not integration" -q` |
| **Full suite command** | `cd tools/tre-compare && uv run pytest -q` (synthetic) / opt-in (PowerShell): `$env:TRE_COMPARE_INTEGRATION=1; uv run pytest -m integration -q` (POSIX: `TRE_COMPARE_INTEGRATION=1 uv run pytest -m integration -q`) |
| **Estimated runtime** | ~3–8 seconds (tiny byte-built fixtures; no network, no payload decompression beyond a tiny zlib round-trip) |

---

## Sampling Rate

- **After every task commit:** Run `cd tools/tre-compare && uv run pytest -m "not integration" -q`
- **After every plan wave:** Run the same quick command (full synthetic suite)
- **Before `/gsd-verify-work`:** Synthetic suite green; integration test green-or-clean-skip on this machine (env-gated, manual; PowerShell `$env:` syntax — review finding #9)
- **Max feedback latency:** ~10 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 28-01-01 | 01 | 1 | TRE-01 | T-28-01-01 | pinned lockfile, zero runtime deps, build backend resolves under uv 0.11.7 (#11) | unit/smoke | `cd tools/tre-compare && uv run python -c "import tre_compare" && uv build` | ❌ W0 | ⬜ pending |
| 28-01-02 | 01 | 1 | TRE-01 | T-28-01-02 | .gitignore prevents .venv/dist commit | file check | `test -f tools/tre-compare/.gitignore` | ❌ W0 | ⬜ pending |
| 28-01-03 | 01 | 1 | TRE-01 | — | marker registered, suite collects | unit | `cd tools/tre-compare && uv run pytest -m "not integration" -q` | ❌ W0 | ⬜ pending |
| 28-02-01 | 02 | 2 | TRE-01 | T-28-02-03 | provenance pin, zero swg_pipeline residue | smoke | `cd tools/tre-compare && uv run python -c "from tre_compare.parser import tre_reader, tre_decrypt"` | ❌ W0 | ⬜ pending |
| 28-02-02 | 02 | 2 | TRE-01 | T-28-02-01 | both length fields, all 5 tags | unit | `cd tools/tre-compare && uv run pytest tests/test_parser.py -x -q` | ❌ W0 | ⬜ pending |
| 28-03-01 | 03 | 3 | TRE-01 | T-28-03-03 | hand-parse (no configparser), engine-faithful (-priority, kind_rank, cfg_seq) sort (#1), bare-priority grammar (#6), cfg-path param | smoke | `cd tools/tre-compare && uv run python -c "from tre_compare.scanner import parse_shared_file, SearchNode"` | ❌ W0 | ⬜ pending |
| 28-03-02 | 03 | 3 | TRE-01 | T-28-03-01 / T-28-03-02 / T-28-03-04 | fix_up_file_name verbatim + safe_virtual_key split (#3) + per-node tombstone + shadowed=REAL-only (#4) + .tre & .toc bounds preflight (#6/#2a) + reparse-dir-prune walk (#5/#7) + malformed-node skip | smoke | `cd tools/tre-compare && uv run python -c "from tre_compare.virtual_tree import fix_up_file_name, safe_virtual_key, build_virtual_tree"` | ❌ W0 | ⬜ pending |
| 28-04-01 | 04 | 4 | TRE-01 | T-28-04-03 | byte-built TRE/SearchTOC(two-phase)/COT2000 + compressed-block fixtures round-trip parser; cfg writer lays out tombstone-vs-real node pairs | unit | `cd tools/tre-compare && uv run python -c "import sys; sys.path.insert(0,'tests'); import _fixtures"` | ✅ exists | ✅ green |
| 28-04-02 | 04 | 4 | TRE-01 | T-28-04-01 | BOTH+inverse tombstone cases, same-priority cross-kind tree-wins (#1), searchPath override, traversal/malformed/.tre+.toc-preflight guards | unit | `cd tools/tre-compare && uv run pytest -m "not integration" -q` | ✅ exists | ✅ green |
| 28-04-04 | 04 | 4 | TRE-01 / SC#3 | T-28-04-01 | cfg-driven END-TO-END tombstone proof: `test_tombstone_end_to_end_via_cfg` drives real `parse_shared_file → build_virtual_tree` over purpose-built genuine on-disk TRE/TOC archives wired through a live-modeled `client.cfg` (tree length-0 → global tombstone; toc length-0/offset-0 → no shadow). SC#3 met LITERALLY (user decision 2026-06-14); D-03 read-only not violated (authoring a test `.tre` ≠ mutating an installed one) | unit | `cd tools/tre-compare && uv run pytest tests/test_virtual_tree.py -k tombstone_end_to_end -q` | ✅ exists | ✅ green |
| 28-04-03 | 04 | 4 | TRE-01 | T-28-04-02 | gated, default-off, clean-skip when absent (PowerShell `$env:` syntax, #9); proves override-shadowing/precedence on the REAL cfg | integration (gated) | `cd tools/tre-compare && $env:TRE_COMPARE_INTEGRATION=1; uv run pytest -m integration -q` (POSIX: `TRE_COMPARE_INTEGRATION=1 uv run pytest -m integration -q`) | ✅ exists | ✅ green |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

All test infrastructure is greenfield (no pytest/uv harness exists in the repo). The
infrastructure is created within this phase's own plans (it IS the deliverable):

- [ ] `tools/tre-compare/pyproject.toml` `[tool.pytest.ini_options]` + `integration` marker — **Plan 01**
- [ ] `tools/tre-compare/.gitignore` (Python ignores + dist/) + uv venv/lockfile — **Plan 01**
- [ ] `tools/tre-compare/tests/conftest.py` (collection root; fixtures filled in Plan 04) — **Plan 01 (stub) / Plan 04 (fixtures)**
- [ ] `tools/tre-compare/tests/_fixtures.py` — programmatic TRE/SearchTOC(two-phase)/COT2000 byte-builders (+ compress option) + synthetic cfg (lays out tombstone-vs-real node pairs) — **Plan 04**
- [ ] `tools/tre-compare/tests/test_parser.py` — **Plan 02 (shape) + Plan 04 (per-tag + compressed-block + COT2000/SearchTOC round-trip)**
- [ ] `tools/tre-compare/tests/test_scanner.py` — engine-faithful kind-order + bare-priority + repeated-key + cfg-param — **Plan 04**
- [ ] `tools/tre-compare/tests/test_virtual_tree.py` (BOTH+inverse tombstone, same-priority cross-kind, safe_virtual_key split, searchPath override/edges, traversal, .tre+.toc bounds preflight, AND the cfg-driven END-TO-END tombstone test) — **Plan 04**
- [ ] `tools/tre-compare/tests/test_integration.py` — env/marker-gated real-install (D-07) — **Plan 04**

*`wave_0_complete` flips to true once Plan 01 lands the scaffold + marker + test root.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Real-install scan against the actual SWGSource v3.0 archives | TRE-01 / SC#3 | Requires machine-specific `D:/Code/SWGSource Client v3.0/` install + `stage/override` present; absent on CI/fresh-clone | PowerShell: `cd tools/tre-compare; $env:TRE_COMPARE_INTEGRATION=1; uv run pytest -m integration -q` (POSIX: `TRE_COMPARE_INTEGRATION=1 uv run pytest -m integration -q`) — passes if install present, clean-skips otherwise (the automated-but-env-gated D-07 test; "manual" only in that the operator opts in) |

*Otherwise: all phase behaviors have automated synthetic verification — including the cfg-driven
end-to-end tombstone proof, which is fully synthetic + machine-independent (no real install needed).*

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or Wave 0 dependencies (Plan 03's two impl tasks carry MISSING→Plan-04 behavioral suite + a smoke-import automated check; no 3-task gap)
- [x] Sampling continuity: no 3 consecutive tasks without automated verify (every task has a smoke/unit command)
- [x] Wave 0 covers all MISSING references (Plan 01 stands up the harness; Plan 04 fills behavioral suites)
- [x] No watch-mode flags
- [x] Feedback latency < 10s
- [x] `nyquist_compliant: true` set in frontmatter
- [x] Round-2 review tests added to the map: same-priority cross-kind (#1), safe_virtual_key split (#3), `.toc`/COT2000 bounds preflight (#2a), bare-priority grammar (#6), compressed-block fixture (#10)
- [x] SC#3 fulfilled-not-narrowed (user decision 2026-06-14): cfg-driven end-to-end tombstone test (28-04-04) added; ROADMAP SC#3 text unchanged

**Approval:** approved 2026-06-14 (revised for round-2 review incorporation 2026-06-14; revised for SC#3 user decision 2026-06-14)
