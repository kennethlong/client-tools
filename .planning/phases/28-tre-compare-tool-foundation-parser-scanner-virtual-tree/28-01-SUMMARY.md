---
phase: 28-tre-compare-tool-foundation-parser-scanner-virtual-tree
plan: 01
subsystem: tooling
tags: [python, uv, pytest, packaging, tre-compare, scaffold]

# Dependency graph
requires: []
provides:
  - "Isolated uv library scaffold at tools/tre-compare/ (src layout, zero runtime deps, ZERO engine imports — D-01)"
  - "pyproject.toml with requires-python >=3.11, dependencies=[], pytest dev dep, registered 'integration' marker"
  - "Committed uv.lock re-resolved under the 3.11 floor (portable to 3.11/3.12 contributors)"
  - "Package-local .gitignore (.venv/, __pycache__, dist/, etc.) — repo root has no Python ignores"
  - "Empty parser/ subpackage init ready for Plan 02 vendoring (D-03)"
  - "pytest test root (conftest.py + collectable smoke test) — Wave-0 validation infra live"
affects: [28-02-vendor-parser, 28-03-scanner-virtual-tree, 28-04-fixtures-integration, phase-29-diff-api, phase-30-web-ui]

# Tech tracking
tech-stack:
  added: ["uv 0.11.7 (venv/lock/run/build)", "pytest 9.1.0 (dev-only)", "uv_build backend"]
  patterns: ["isolated copy-paste-extractable Python package (D-01)", "zero-runtime-dep stdlib-only discipline", "committed uv.lock for reproducibility (D-05)", "env-gated pytest 'integration' marker (D-07 infra)"]

key-files:
  created:
    - tools/tre-compare/pyproject.toml
    - tools/tre-compare/uv.lock
    - tools/tre-compare/.python-version
    - tools/tre-compare/README.md
    - tools/tre-compare/.gitignore
    - tools/tre-compare/src/tre_compare/__init__.py
    - tools/tre-compare/src/tre_compare/py.typed
    - tools/tre-compare/src/tre_compare/parser/__init__.py
    - tools/tre-compare/tests/conftest.py
    - tools/tre-compare/tests/test_scaffold.py
  modified: []

key-decisions:
  - "Pinned .python-version + requires-python to the 3.11 floor (not the dev machine's 3.14) so a fresh clone / 3.11-3.12 contributor resolves cleanly; re-resolved uv.lock under 3.11 for lock portability (review LOW finding)"
  - "Kept [build-system].requires at uv_build>=0.11.7,<0.12.0 (the bound uv init generated for the installed uv 0.11.7) — no forward-pin; verified the backend RESOLVES via `uv build` exit 0 (review finding #11)"
  - "Registered the 'integration' pytest marker now (Plan 01) so Plan 04's env-gated D-07 test emits no PytestUnknownMarkWarning"

patterns-established:
  - "Isolated uv library (D-01): own pyproject.toml + uv.lock + venv, zero engine imports, copy-paste extractable"
  - "Zero-runtime-dep guarantee: dependencies=[]; pytest is the ONLY (dev) dependency; vendored parser will be stdlib-only"
  - "Package-local .gitignore prevents .venv/dist leaking into the repo (repo root has no Python entries — Pitfall 6)"

requirements-completed: [TRE-01]

# Metrics
duration: ~8min
completed: 2026-06-14
---

# Phase 28 Plan 01: TRE Compare Tool Scaffold Summary

**Isolated, copy-paste-extractable uv library at `tools/tre-compare/` with zero runtime deps, pytest 9.1.0 dev dep, committed 3.11-portable uv.lock, registered `integration` marker, empty parser/ subpackage, and a green collectable pytest suite — the Wave-0 foundation for Plans 02-04.**

## Performance

- **Duration:** ~8 min
- **Started:** 2026-06-14T19:15Z (approx)
- **Completed:** 2026-06-14T19:23Z
- **Tasks:** 3
- **Files created:** 10

## Accomplishments
- `uv init --lib tools/tre-compare` scaffold (src layout, `py.typed`, `uv_build` backend) with **zero runtime dependencies** (D-01 extractability) and pytest 9.1.0 as the only dev dep.
- `requires-python` and `.python-version` lowered to the **3.11 floor** (uv init wrote 3.14, the dev machine's interpreter); `uv.lock` **re-resolved under 3.11** so a fresh clone / CI / a 3.11-3.12 contributor resolves cleanly while 3.12-3.14 still work.
- `[build-system].requires` kept at `uv_build>=0.11.7,<0.12.0` (the bound uv 0.11.7 generated) — no forward-pin — and **proven to resolve** via `uv build` exit 0 (review finding #11).
- Registered the `integration` pytest marker (D-07 infra) so Plan 04's env-gated real-install test will not warn.
- Package-local `.gitignore` (`.venv/`, `__pycache__/`, `*.pyc`, `.pytest_cache/`, `*.egg-info/`, `dist/`) — verified effective via `git check-ignore`.
- Empty `parser/` subpackage ready for Plan 02 vendoring; `tre_reader.py`/`tre_decrypt.py` intentionally NOT added (Plan 02's task).
- pytest test root: `conftest.py` collection root + a collectable smoke test → `uv run pytest -m "not integration"` exits 0, 1 passed, no marker warnings.

## Task Commits

Each task was committed atomically:

1. **Task 1: Scaffold isolated uv library + zero-dep pyproject + lockfile** - `ef582ae73` (feat)
2. **Task 2: Package-local .gitignore + parser subpackage skeleton** - `4f102935a` (feat)
3. **Task 3: pytest test root (conftest + collectable smoke test)** - `959266632` (test)

**Plan metadata:** (this SUMMARY + STATE/ROADMAP/REQUIREMENTS) committed separately.

## Files Created/Modified
- `tools/tre-compare/pyproject.toml` - uv project config: name tre-compare, requires-python >=3.11, dependencies=[], pytest dev dep, `[tool.pytest.ini_options]` with the `integration` marker + `testpaths`.
- `tools/tre-compare/uv.lock` - committed lockfile (pytest 9.1.0 + transitive deps), re-resolved under the 3.11 pin for portability.
- `tools/tre-compare/.python-version` - pinned to `3.11`.
- `tools/tre-compare/README.md` - package purpose, 3.11 floor, uv usage incl. PowerShell env-var form for the integration gate.
- `tools/tre-compare/.gitignore` - Python ignores incl. `dist/` (the `uv build` verify artifact).
- `tools/tre-compare/src/tre_compare/__init__.py`, `py.typed` - package init + typing marker.
- `tools/tre-compare/src/tre_compare/parser/__init__.py` - empty parser subpackage (Plan 02 placeholder).
- `tools/tre-compare/tests/conftest.py` - collection root + Plan-04 fixture placeholder.
- `tools/tre-compare/tests/test_scaffold.py` - 1 passing import test.

## Decisions Made
- **3.11 floor pin (review LOW finding):** uv init pinned `.python-version`/`requires-python` to the dev machine's 3.14; lowered both to the 3.11 floor and re-resolved `uv.lock` under 3.11 (uv downloaded cpython-3.11.15 to resolve) so the committed lock is portable. `requires-python = ">=3.11"` keeps 3.12-3.14 working.
- **Build-backend bound (review finding #11):** kept the uv-init-generated `uv_build>=0.11.7,<0.12.0` rather than the RESEARCH-doc example `>=0.11.21` (which would be a forward-pin under the installed uv 0.11.7). Verified resolution with a `uv build` (exit 0, built sdist + wheel).
- **Marker registered early:** the `integration` marker lives in `pyproject.toml` from Plan 01 so the Plan 04 gated test does not emit `PytestUnknownMarkWarning`.

## Deviations from Plan

None - plan executed exactly as written. (The plan explicitly anticipated the uv-init 3.14 pin and instructed lowering to 3.11, and explicitly anticipated keeping the build-backend bound at the installed uv version — both handled as specified, not as deviations.)

## Issues Encountered

- **uv hardlink warning** (`Failed to hardlink files; falling back to full copy`) — benign; cache and target are on the same drive but uv fell back to copy. No effect on correctness; venv is gitignored.
- **CRLF normalization warnings** on `git add` (`LF will be replaced by CRLF`) — expected on Windows for the newly created text files; cosmetic.

## Acceptance Criteria Note

The plan's anti-forward-pin grep `uv_build>=0\.11\.(2[1-9]|[3-9])` technically matches the kept bound `uv_build>=0.11.7` (because `7` falls in the `[3-9]` class). This is a slight over-breadth in the check pattern, **not** a forward-pin: `0.11.7` is exactly the installed uv version, which is the criterion's actual intent ("bound NOT raised above the installed uv_build"). Backend resolution confirmed independently by `uv build` exit 0.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- **Plan 02 (vendor parser)** is unblocked: the empty `src/tre_compare/parser/__init__.py` is in place to receive the vendored `tre_reader.py` + `tre_decrypt.py` (D-03) with the single line-251 import rewrite.
- **Plan 03/04** can build the scanner/virtual-tree and the synthetic fixtures against this `tests/` root and the registered `integration` marker.
- Toolchain proven: `uv sync` / `uv run pytest` / `uv build` all green under the installed uv 0.11.7.

## Self-Check: PASSED

All 10 created files verified present; all 3 task commits verified in git log (see below).

---
*Phase: 28-tre-compare-tool-foundation-parser-scanner-virtual-tree*
*Completed: 2026-06-14*
