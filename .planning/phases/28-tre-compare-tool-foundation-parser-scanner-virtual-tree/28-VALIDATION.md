---
phase: 28
slug: tre-compare-tool-foundation-parser-scanner-virtual-tree
status: planned
nyquist_compliant: true
wave_0_complete: false
created: 2026-06-14
---

# Phase 28 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | pytest (resolved + pinned in `tools/tre-compare/uv.lock` at scaffold — Plan 01) |
| **Config file** | `tools/tre-compare/pyproject.toml` `[tool.pytest.ini_options]` (registers the `integration` marker; created in Plan 01) |
| **Quick run command** | `cd tools/tre-compare && uv run pytest -m "not integration" -q` |
| **Full suite command** | `cd tools/tre-compare && uv run pytest -q` (synthetic) / opt-in: `TRE_COMPARE_INTEGRATION=1 uv run pytest -m integration -q` |
| **Estimated runtime** | ~3–8 seconds (tiny byte-built fixtures; no network, no payload decompression) |

---

## Sampling Rate

- **After every task commit:** Run `cd tools/tre-compare && uv run pytest -m "not integration" -q`
- **After every plan wave:** Run the same quick command (full synthetic suite)
- **Before `/gsd-verify-work`:** Synthetic suite green; integration test green-or-clean-skip on this machine (env-gated, manual)
- **Max feedback latency:** ~10 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 28-01-01 | 01 | 1 | TRE-01 | T-28-01-01 | pinned lockfile, zero runtime deps | unit/smoke | `cd tools/tre-compare && uv run python -c "import tre_compare"` | ❌ W0 | ⬜ pending |
| 28-01-02 | 01 | 1 | TRE-01 | T-28-01-02 | .gitignore prevents .venv commit | file check | `test -f tools/tre-compare/.gitignore` | ❌ W0 | ⬜ pending |
| 28-01-03 | 01 | 1 | TRE-01 | — | marker registered, suite collects | unit | `cd tools/tre-compare && uv run pytest -m "not integration" -q` | ❌ W0 | ⬜ pending |
| 28-02-01 | 02 | 2 | TRE-01 | T-28-02-03 | provenance pin, zero swg_pipeline residue | smoke | `cd tools/tre-compare && uv run python -c "from tre_compare.parser import tre_reader, tre_decrypt"` | ❌ W0 | ⬜ pending |
| 28-02-02 | 02 | 2 | TRE-01 | T-28-02-01 | both length fields, all 5 tags | unit | `cd tools/tre-compare && uv run pytest tests/test_parser.py -x -q` | ❌ W0 | ⬜ pending |
| 28-03-01 | 03 | 3 | TRE-01 | T-28-03-03 | hand-parse (no configparser), cfg-path param | smoke | `cd tools/tre-compare && uv run python -c "from tre_compare.scanner import parse_shared_file, SearchNode"` | ❌ W0 | ⬜ pending |
| 28-03-02 | 03 | 3 | TRE-01 | T-28-03-01 / T-28-03-02 | fixUpFileName + per-node tombstone + path-traversal reject + malformed-node skip | smoke | `cd tools/tre-compare && uv run python -c "from tre_compare.virtual_tree import fix_up_file_name, build_virtual_tree"` | ❌ W0 | ⬜ pending |
| 28-04-01 | 04 | 4 | TRE-01 | T-28-04-03 | byte-built fixtures round-trip parser | unit | `cd tools/tre-compare && uv run python -c "import sys; sys.path.insert(0,'tests'); import _fixtures"` | ❌ W0 | ⬜ pending |
| 28-04-02 | 04 | 4 | TRE-01 | T-28-04-01 | BOTH tombstone cases + searchPath override + traversal/malformed guards | unit | `cd tools/tre-compare && uv run pytest -m "not integration" -q` | ❌ W0 | ⬜ pending |
| 28-04-03 | 04 | 4 | TRE-01 | T-28-04-02 | gated, default-off, clean-skip when absent | integration (gated) | `cd tools/tre-compare && TRE_COMPARE_INTEGRATION=1 uv run pytest -m integration -q` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

All test infrastructure is greenfield (no pytest/uv harness exists in the repo). The
infrastructure is created within this phase's own plans (it IS the deliverable):

- [ ] `tools/tre-compare/pyproject.toml` `[tool.pytest.ini_options]` + `integration` marker — **Plan 01**
- [ ] `tools/tre-compare/.gitignore` (Python ignores) + uv venv/lockfile — **Plan 01**
- [ ] `tools/tre-compare/tests/conftest.py` (collection root; fixtures filled in Plan 04) — **Plan 01 (stub) / Plan 04 (fixtures)**
- [ ] `tools/tre-compare/tests/_fixtures.py` — programmatic TRE/SearchTOC byte-builders + synthetic cfg — **Plan 04**
- [ ] `tools/tre-compare/tests/test_parser.py` — **Plan 02 (shape) + Plan 04 (per-tag round-trip)**
- [ ] `tools/tre-compare/tests/test_scanner.py` — **Plan 04**
- [ ] `tools/tre-compare/tests/test_virtual_tree.py` (incl. BOTH tombstone tests, searchPath override, traversal) — **Plan 04**
- [ ] `tools/tre-compare/tests/test_integration.py` — env/marker-gated real-install (D-07) — **Plan 04**

*`wave_0_complete` flips to true once Plan 01 lands the scaffold + marker + test root.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Real-install scan against the actual SWGSource v3.0 archives | TRE-01 / SC#3 | Requires machine-specific `D:/Code/SWGSource Client v3.0/` install + `stage/override` present; absent on CI/fresh-clone | `cd tools/tre-compare && TRE_COMPARE_INTEGRATION=1 uv run pytest -m integration -q` — passes if install present, clean-skips otherwise (this is the automated-but-env-gated D-07 test; "manual" only in that the operator opts in) |

*Otherwise: all phase behaviors have automated synthetic verification.*

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or Wave 0 dependencies (Plan 03's two impl tasks carry MISSING→Plan-04 behavioral suite + a smoke-import automated check; no 3-task gap)
- [x] Sampling continuity: no 3 consecutive tasks without automated verify (every task has a smoke/unit command)
- [x] Wave 0 covers all MISSING references (Plan 01 stands up the harness; Plan 04 fills behavioral suites)
- [x] No watch-mode flags
- [x] Feedback latency < 10s
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** approved 2026-06-14
