---
phase: 29
slug: tre-compare-tool-diff-engine-api
status: draft
nyquist_compliant: true
wave_0_complete: false
created: 2026-06-14
---

# Phase 29 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | pytest 9.1.0 (Phase-28 dev dep) + httpx (TestClient) |
| **Config file** | `tools/tre-compare/pyproject.toml` (`integration` marker registered) |
| **Quick run command** | `uv run pytest -m "not integration"` |
| **Full suite command** | `uv run pytest` (integration gated by `TRE_COMPARE_LEFT_CFG`/`TRE_COMPARE_RIGHT_CFG` env vars) |
| **Estimated runtime** | ~5–15 seconds (synthetic); integration skips cleanly when env unset |

---

## Sampling Rate

- **After every task commit:** Run `uv run pytest -m "not integration"`
- **After every plan wave:** Run `uv run pytest -m "not integration"`
- **Before `/gsd:verify-work`:** Full suite green; integration test run once with the real SWGSource-vs-whitengold cfg pair (SC#4)
- **Max feedback latency:** 15 seconds

---

## Per-Task Verification Map

*Planner fills this map from the final plan task IDs. Source from RESEARCH.md §"Validation Architecture" (line 500) — synthetic byte-built fixtures mirror the Phase-28 harness; the env-gated integration test covers SC#4.*

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| P01-T1 deps+lock | 29-01 | 1 | D-11/D-12 | — | bare uvicorn (lean wheel surface) | smoke | `uv run python -c "import fastapi,uvicorn,xxhash"` | ❌ W0 | ⬜ pending |
| P01-T2 fixtures+test_diff (RED) | 29-01 | 1 | TRE-02/03/04 | — | false-identical + toc-payload fixtures | unit | `uv run pytest tests/test_diff.py -q` | ❌ W0 | ⬜ pending |
| P01-T3 diff.py impl | 29-01 | 1 | TRE-02/03/04 | T-29-01-01/02/03 | no crc signal; decompressed-byte xxhash; degrade-not-raise | unit | `uv run pytest tests/test_diff.py -q` | ❌ W0 | ⬜ pending |
| P02-T1 test_cache (RED) | 29-02 | 2 | TRE-02/03/04 (D-10) | — | parity vs Phase-28 builder | unit | `uv run pytest tests/test_cache.py -q` | ❌ W0 | ⬜ pending |
| P02-T2 cache.py impl | 29-02 | 2 | TRE-02/03/04 (D-10) | T-29-02-01/02/03/04 | implicit (mtime,size) invalidation; WAL+Lock; fault isolation | unit | `uv run pytest tests/test_cache.py -q` | ❌ W0 | ⬜ pending |
| P03-T1 config.py | 29-03 | 3 | D-02 | T-29-03-04 | absent config → empty list (no crash) | unit | `uv run pytest -m "not integration" -q` | ❌ W0 | ⬜ pending |
| P03-T2 api.py+__main__+test_api | 29-03 | 3 | TRE-02/03/04 | T-29-03-01/02/03 | 400 envelope; node_errors not 500; bind 127.0.0.1 | unit | `uv run pytest tests/test_api.py -q` | ❌ W0 | ⬜ pending |
| P03-T3 integration (SC#4) | 29-03 | 3 | TRE-02/03/04 (SC#4) | T-29-03-01 | env-gated; skips clean by default | integration | `TRE_COMPARE_INTEGRATION=1 uv run pytest -m integration -k pair -q` | ⚠️ extend | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tools/tre-compare/tests/test_diff.py` — set-level + file-level tri-state diff over synthetic VirtualTrees (TRE-02/03)
- [ ] `tools/tre-compare/tests/test_cache.py` — sqlite (abspath,mtime,size) keying + invalidation + `build_virtual_tree_cached` parity vs Phase-28 builder
- [ ] `tools/tre-compare/tests/test_api.py` — FastAPI TestClient over the four routes; lean per-row JSON shape; error envelope
- [ ] `tools/tre-compare/tests/conftest.py` — extend the Phase-28 byte-built fixture builders for two-install pairs
- [ ] Env-gated integration test parameterized via `TRE_COMPARE_LEFT_CFG`/`TRE_COMPARE_RIGHT_CFG` (SC#4)

*Framework already installed (Phase 28). httpx added as dev dep for TestClient.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Drill-in xxhash resolves a real false-identical | TRE-04 | Requires the real SWGSource-vs-whitengold pair; Kenny supplies the two cfg paths | Run integration test with both env vars set; confirm an `identical-by-metadata` entry upgrades to content-changed/confirmed |

*Most phase behaviors have automated verification via synthetic fixtures.*

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 15s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
