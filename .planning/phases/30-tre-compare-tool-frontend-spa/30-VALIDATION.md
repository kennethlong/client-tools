---
phase: 30
slug: tre-compare-tool-frontend-spa
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-06-15
---

# Phase 30 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.
> Derived from 30-RESEARCH.md §"Validation Architecture".

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Frontend framework** | Vitest + React Testing Library (Vite-native; none exists yet → Wave 0) |
| **Backend framework** | pytest (existing) — `uv run pytest` from `tools/tre-compare/`; `httpx` TestClient for routes |
| **Config file** | `tools/tre-compare/web/vite.config.ts` (Vitest reads the Vite config) — Wave 0 |
| **Quick run command (frontend)** | `cd tools/tre-compare/web && npm run test -- --run` |
| **Quick run command (backend)** | `uv run pytest -m "not integration" -x` (from `tools/tre-compare/`) |
| **Full suite command** | `cd tools/tre-compare/web && npm run test -- --run && npm run build` AND `uv run pytest` |
| **Estimated runtime** | ~30–60 seconds (Vitest unit fast; `npm run build` dominates) |

---

## Sampling Rate

- **After every task commit:** `cd tools/tre-compare/web && npm run test -- --run` (fast Vitest unit) + `npm run build` when touching build/serve config.
- **After every plan wave:** full frontend Vitest + `npm run build` + backend `uv run pytest -m "not integration"`.
- **Before `/gsd:verify-work`:** full suite green + the manual SWGSource-vs-whitengold end-to-end run.
- **Max feedback latency:** ~60 seconds.

---

## Per-Task Verification Map

| Req | Behavior | Test Type | Automated Command | File Exists | Status |
|-----|----------|-----------|-------------------|-------------|--------|
| TRE-05 | `buildFolderTree` builds correct nesting + roll-up counts from a flat array | unit | `cd tools/tre-compare/web && npm run test -- --run src/lib/tree.test.ts` | ❌ W0 | ⬜ pending |
| TRE-05 | `flattenVisible` honors expand state + hide-identical filter + empty-folder suppression | unit | `cd tools/tre-compare/web && npm run test -- --run src/lib/tree.test.ts` | ❌ W0 | ⬜ pending |
| TRE-05 | file-status → badge map distinguishes `identical-by-metadata` from `content-confirmed-identical` (and set vs file enums are separate maps) | unit | `cd tools/tre-compare/web && npm run test -- --run src/lib/status.test.ts` | ❌ W0 | ⬜ pending |
| TRE-05 | SPA bundle builds clean (`build.outDir: web/build`, not `dist/`) | smoke | `cd tools/tre-compare/web && npm run build` | ❌ W0 | ⬜ pending |
| TRE-05 | FastAPI mounts the built SPA AND still serves the 4 routes (mount-order: API routes before `/` SPA mount) | integration | `uv run pytest tests/test_web_static.py -x` | ❌ W0 | ⬜ pending |
| TRE-05 | SWGSource-vs-whitengold real diff renders end-to-end (SC#4) | manual | `npm run build` → `python -m tre_compare` → browser | n/a | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tools/tre-compare/web/` Vite project scaffold (package.json, vite.config.ts, tsconfig*, components.json) + `npm install`
- [ ] Vitest + `@testing-library/react` + jsdom install (`npm install -D vitest @testing-library/react jsdom`)
- [ ] `tools/tre-compare/web/src/lib/tree.test.ts` — covers TRE-05 tree-build/flatten/roll-up
- [ ] `tools/tre-compare/web/src/lib/status.test.ts` — covers TRE-05 honesty-distinction badge mapping (separate set vs file enums)
- [ ] `tools/tre-compare/tests/test_web_static.py` — covers SPA mount + mount-order (the only new backend test)
- [ ] `tre_compare/web_static.py` — the SpaStaticFiles helper (only new backend source)

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Full SWGSource-vs-whitengold space-asset diff renders end-to-end with virtualized tree, no hang on real data (SC#4) | TRE-05 | Requires a real browser + two real installs + visual judgement of the rendered tree/badges/detail panel | `cd tools/tre-compare/web && npm run build` → `python -m tre_compare` (binds 127.0.0.1) → open the printed URL → pick SWGSource + whitengold installs → confirm set-delta, virtualized tree, filter/search/hide-identical, and per-file detail all work |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references (the 6 gaps above)
- [ ] No watch-mode flags (`--run` forces Vitest single-shot)
- [ ] Feedback latency < 60s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
