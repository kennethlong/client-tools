---
phase: 30-tre-compare-tool-frontend-spa
plan: 01
subsystem: ui
tags: [vite, react, typescript, tailwind, shadcn, vitest, tanstack, fastapi, staticfiles, spa]

# Dependency graph
requires:
  - phase: 29-tre-compare-tool-diff-engine-api
    provides: "create_app(cache, installs_path) factory + four frozen JSON routes (/compare/set, /compare/files, /file/detail, /installs) the SPA consumes; __main__ 127.0.0.1 bind"
provides:
  - "tools/tre-compare/web/ — first frontend in the repo: Vite 8 + React 19 + TS 6 + Tailwind 4 + shadcn (zinc/dark), builds to web/build/ (NOT dist/)"
  - "Vitest + RTL + jsdom test harness (Wave-0 frontend seam) wired via vite.config test block; npm run test single-shot green"
  - "8 shadcn primitives (button badge table sheet input scroll-area separator tooltip) + @tanstack/react-virtual + @tanstack/react-query installed for Wave 2"
  - "web_static.SpaStaticFiles + a guarded app.mount('/') registered LAST in create_app — single 127.0.0.1 process serves the built SPA + the 4 API routes (D-02)"
  - "create_app gains an optional web_dir param (defaults to web/build) so the mount is testable without a real npm build"
affects: [30-02, 30-03, tre-compare-frontend, wave-2-data-layer, wave-2-ui]

# Tech tracking
tech-stack:
  added: [vite@8, react@19, typescript@6, tailwindcss@4, "@tailwindcss/vite", shadcn, vitest@4, "@testing-library/react", jsdom, "@tanstack/react-virtual", "@tanstack/react-query", lucide-react, "radix-ui", tw-animate-css]
  patterns: ["Vite SPA as a sibling of src/ inside an isolated uv package (zero coupling to the engine MSBuild graph; D-01 extractability preserved)", "FastAPI SpaStaticFiles 404->index.html mounted LAST so greedy / never shadows the API (T-30-01-03)", "build.outDir=build to dodge the gitignored Python dist/ collision", "relative-path fetch + dev proxy so dev-proxied and prod-same-origin share one code path"]

key-files:
  created:
    - tools/tre-compare/web/vite.config.ts
    - tools/tre-compare/web/components.json
    - tools/tre-compare/web/src/App.tsx
    - tools/tre-compare/web/src/lib/utils.ts
    - tools/tre-compare/web/src/lib/placeholder.test.ts
    - tools/tre-compare/web/src/components/ui/ (8 shadcn primitives)
    - tools/tre-compare/src/tre_compare/web_static.py
    - tools/tre-compare/tests/test_web_static.py
  modified:
    - tools/tre-compare/src/tre_compare/api.py
    - tools/tre-compare/.gitignore
    - .gitignore (root — negate global package.json/lib/ ignores for the web project)

key-decisions:
  - "Realigned shadcn init OFF its interactive 'Nova' preset back to the locked zinc base + plain new-york style + dark-mode default (UI-SPEC §Design System) — the CLI had no --base-color flag and defaulted to the Nova/neutral/Geist preset"
  - "build.outDir = 'build' (never dist/) — dist/ is the gitignored Python build artifact; web/build/ is built-on-demand and gitignored (D-02/A3)"
  - "Removed TS 6 deprecated baseUrl; @/* path aliases resolve relative to tsconfig in TS 6 without it"
  - "Negated the root .gitignore global package.json / package-lock.json / lib/ ignores ONLY for tools/tre-compare/web — this is the one legitimate Node project in an otherwise C++/Python repo"

patterns-established:
  - "SpaStaticFiles(StaticFiles) 404->index.html fallback, mounted at / LAST after all API routes (Pitfall 2 / T-30-01-03)"
  - "create_app(web_dir=...) injection lets tests forge a synthetic web/build without npm — same shape as the cache/installs_path injection"

requirements-completed: [TRE-05]

# Metrics
duration: 12min
completed: 2026-06-15
---

# Phase 30 Plan 01: Frontend SPA Scaffold + SPA Static Mount Summary

**First frontend in the repo — a Vite 8 / React 19 / Tailwind 4 / shadcn (zinc, dark) SPA at `tools/tre-compare/web/` that builds to `web/build/`, a wired Vitest harness, and a `SpaStaticFiles` mount that serves the built bundle from the existing single-127.0.0.1 FastAPI process.**

## Performance

- **Duration:** 12 min
- **Started:** 2026-06-15T14:57:04Z
- **Completed:** 2026-06-15T15:09:15Z
- **Tasks:** 3 (Task 3 was TDD: RED + GREEN)
- **Files modified/created:** ~40 (scaffold + 8 shadcn components + 2 backend files + 1 test + 3 gitignores)

## Accomplishments
- Scaffolded the repo's first Vite + React 19 + TS 6 + Tailwind 4 + shadcn project as a sibling of `src/`, building to `web/build/` (no `dist/` collision), with a dev proxy + `@` alias and a minimal "TRE Compare" placeholder (real layout is Wave 2 / plan 30-03).
- Wired the Vitest + React Testing Library + jsdom harness (Wave-0 frontend seam) — `npm run test -- --run` is single-shot green — and pre-installed the 8 shadcn primitives + TanStack Virtual/Query that Wave 2 needs.
- Added the ONLY backend touch of the whole phase: `web_static.SpaStaticFiles` (404->index.html fallback) mounted LAST in `create_app`, guarded on the build existing, with a new injectable `web_dir` param; verified end-to-end against the REAL built bundle (GET / -> 200 text/html "TRE Compare") and that `/compare/files` still returns JSON (mount-order regression).

## Task Commits

1. **Task 1: Scaffold Vite + React + Tailwind 4 + shadcn** - `544359067` (feat)
2. **Task 2: Wire Vitest + shadcn primitives + TanStack libs + placeholder test** - `e8b5154b0` (test)
3. **Task 3 (TDD RED): failing SPA mount + mount-order regression test** - `626b9a073` (test)
4. **Task 3 (TDD GREEN): SpaStaticFiles mount in create_app** - `dd4e1da2b` (feat)

_REFACTOR phase skipped — the GREEN implementation was already clean (no behavior-preserving cleanup warranted)._

## Files Created/Modified
- `tools/tre-compare/web/vite.config.ts` - react + tailwindcss plugins, `@` alias, `build.outDir=build`, dev proxy (`/compare` `/file` `/installs` -> 127.0.0.1:8765), Vitest `test` block (jsdom + globals)
- `tools/tre-compare/web/components.json` - shadcn config realigned to zinc base + new-york style
- `tools/tre-compare/web/src/index.css` - canonical shadcn zinc theme (`:root` light + `.dark` dark), Tailwind 4 `@theme inline` token bridge; Nova preset imports/Geist font stripped
- `tools/tre-compare/web/tsconfig.json` + `tsconfig.app.json` - `@/*` path alias (no baseUrl)
- `tools/tre-compare/web/index.html` - `class="dark"` (dark default), `<title>TRE Compare</title>`
- `tools/tre-compare/web/src/App.tsx` - minimal "TRE Compare" placeholder
- `tools/tre-compare/web/src/lib/utils.ts` - shadcn `cn()` helper
- `tools/tre-compare/web/src/lib/placeholder.test.ts` - Wave-0 Vitest harness proof (Wave 2 replaces)
- `tools/tre-compare/web/src/components/ui/*` - button, badge, table, sheet, input, scroll-area, separator, tooltip
- `tools/tre-compare/src/tre_compare/web_static.py` - `SpaStaticFiles` + `WEB_DIR` (3-level `.parent` walk mirroring config.py)
- `tools/tre-compare/src/tre_compare/api.py` - `create_app(web_dir=...)` + guarded SPA mount registered LAST
- `tools/tre-compare/tests/test_web_static.py` - 6 tests: SPA index serve, mount-order (2 routes), 404->index fallback, real-asset serve, no-build-dir degrade
- `tools/tre-compare/.gitignore` - `web/build/` + `web/node_modules/` (built-on-demand, D-02/A3)
- `.gitignore` (root) - negations so the web project's `package.json`/`package-lock.json`/`src/lib/` are tracked

## Decisions Made
- See `key-decisions` in frontmatter. The most consequential: the shadcn CLI's interactive init applied its "Nova" preset (neutral base, Geist font, extra registry imports) instead of the locked zinc/plain/dark contract; I rewrote `components.json` + `index.css` to the canonical zinc theme to honor the UI-SPEC rather than ship the off-contract preset.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] shadcn init had no `--base-color` flag; defaulted to the off-contract "Nova" preset**
- **Found during:** Task 1
- **Issue:** The new shadcn CLI 4 init has no `--base-color` option and, run non-interactively with `--template vite -b radix`, applied the "Nova" preset (baseColor neutral, Geist variable font, `shadcn/tailwind.css` + `@fontsource-variable/geist` imports, `shadcn` added as a runtime dep). The UI-SPEC locks **zinc base, plain init, dark-mode default**.
- **Fix:** Rewrote `components.json` to `style: new-york` + `baseColor: zinc`; rewrote `src/index.css` to the canonical shadcn zinc `:root`/`.dark` token theme (kept `tw-animate-css`, dropped the Geist font + Nova-only imports); `npm uninstall @fontsource-variable/geist shadcn`; set `class="dark"` on `<html>` for dark default.
- **Files modified:** components.json, src/index.css, index.html, package.json/package-lock.json
- **Verification:** `npm run build` green; theme matches the UI-SPEC zinc/dark contract.
- **Committed in:** `544359067` (Task 1 commit)

**2. [Rule 3 - Blocking] TypeScript 6 deprecates `baseUrl` (build error TS5101)**
- **Found during:** Task 1 (first `npm run build`)
- **Issue:** Adding shadcn's `baseUrl: "."` to tsconfig broke `tsc -b` with TS5101 (baseUrl deprecated, stops functioning in TS 7).
- **Fix:** Removed `baseUrl` from both tsconfig files; TS 6 resolves `@/*` paths relative to the tsconfig location without it.
- **Files modified:** tsconfig.json, tsconfig.app.json
- **Verification:** `npm run build` exits 0; `@` alias resolves in build + Vite.
- **Committed in:** `544359067` (Task 1 commit)

**3. [Rule 3 - Blocking] Root .gitignore globally ignores package.json / package-lock.json / lib/**
- **Found during:** Task 1 (staging)
- **Issue:** The root `.gitignore` ignores `package.json`, `package-lock.json` (stray-npm-tooling guard for this C++/Python repo) and `lib/` (engine build artifact) — all three caught the web project's required files (`files_modified` lists package.json + package-lock.json; `src/lib/utils.ts` is shadcn's `cn()`).
- **Fix:** Added negation rules (`!tools/tre-compare/web/package.json`, `!.../package-lock.json`, `!.../src/lib/` + `/**`) — scoped ONLY to the one legitimate Node project.
- **Files modified:** .gitignore (root)
- **Verification:** `git check-ignore` returns non-zero (no longer ignored) for all three; files committed in Task 1.
- **Committed in:** `544359067` (Task 1 commit)

---

**Total deviations:** 3 auto-fixed (all Rule 3 - blocking tool/environment frictions; no architectural change)
**Impact on plan:** All three were toolchain/environment frictions blocking the planned scaffold; the resulting project matches the plan + UI-SPEC contract exactly. No scope creep, no contract changes.

## Issues Encountered
- The shadcn CLI's non-interactive behavior preset-drift (handled as deviation 1). No other problems; the backend `web_static` mount worked first try (GREEN), and the full non-integration suite stayed green (80 passed, 2 deselected — the 72 prior tests + 6 new + 2 others, no regression).

## Known Stubs
Both are **intentional Wave-0 placeholders explicitly scoped to Wave 2** by the plan, not unresolved gaps:
- `tools/tre-compare/web/src/App.tsx` - minimal "TRE Compare" heading; the real master-detail layout (pickers, set-delta strip, virtualized tree, detail Sheet) is plan 30-03.
- `tools/tre-compare/web/src/lib/placeholder.test.ts` - trivial `1+1` test proving the runner is wired; plan 30-03 replaces it with `tree.test.ts` / `status.test.ts`.

## User Setup Required
None - no external service configuration required. (Frontend builds on demand via `npm run build`; Node 24 + npm 11 already present on this machine.)

## Next Phase Readiness
- **Wave 2 (plans 30-02/30-03) is unblocked:** the buildable project, the dev-proxy/prod-same-origin fetch path, the green Vitest seam, the 8 shadcn primitives + TanStack libs, and a correct SPA static-mount (mount-order regression locked) all exist. Wave 2 can build the data layer (`lib/api.ts`, `lib/tree.ts`) and UI on this foundation and run RED->GREEN against the wired Vitest harness.
- No blockers. The frozen Phase-29 four-route contract is consumed unchanged; the 127.0.0.1 bind is untouched.

## Self-Check: PASSED

All 10 spot-checked created files exist on disk; all 4 task commits (`544359067`, `e8b5154b0`, `626b9a073`, `dd4e1da2b`) are present in git history.

---
*Phase: 30-tre-compare-tool-frontend-spa*
*Completed: 2026-06-15*
