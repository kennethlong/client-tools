# Phase 30: TRE Compare Tool — Frontend SPA - Pattern Map

**Mapped:** 2026-06-15
**Files analyzed:** 14 new (13 frontend, 1 new backend source) + 2 modified backend + 1 new backend test
**Analogs found:** 3 / 17 (only the backend-touch files have in-repo analogs; the React SPA is greenfield)

## Headline finding

**No React / Vite / TypeScript / frontend analog exists anywhere in this repository.** `web/` is
absent, there are no `.tsx`/`.jsx` files, and the only `package.json`s are repo-root tooling and
`node_modules` (Express/MCP SDK chaff, unrelated to this SPA). This phase scaffolds the first
frontend in the repo.

**Therefore the integration anchor for ALL frontend files is the FROZEN FastAPI contract**, read
directly from source this pass:
- `tools/tre-compare/src/tre_compare/api.py` — the four routes + the three serializers
  (`_merged_meta`, `_node_brief`, `_hash_brief`) that define every JSON field the SPA consumes.
- `tools/tre-compare/src/tre_compare/diff.py` — the status/verdict enum literals (verified below).
- `tools/tre-compare/src/tre_compare/config.py` — the `/installs` `{name, cfg_path}` shape.

The planner must treat the api.py serializers + the verified enum lists below as the type spec for
`web/src/lib/api.ts`. There is no React file to copy a pattern *from*; copy the **field names and
enum literals** from the backend instead.

Only three files in this phase have a real in-repo analog (all backend-side): `web_static.py`,
`__main__.py` (modified), and `test_web_static.py`. Those are mapped concretely below.

## File Classification

### Frontend (greenfield — anchor = API contract, no code analog)

| New File | Role | Data Flow | Closest Analog | Match Quality |
|----------|------|-----------|----------------|---------------|
| `web/package.json` | config | — | (none — `tools/tre-compare/pyproject.toml` is the *conceptual* sibling manifest) | no analog |
| `web/vite.config.ts` | config | — | none (RESEARCH Pattern 2 is the spec) | no analog |
| `web/tsconfig*.json`, `web/components.json` | config | — | none | no analog |
| `web/index.html` | config | — | none | no analog |
| `web/src/lib/api.ts` | utility (typed fetch client) | request-response | **`api.py` serializers (contract)** | contract-anchor |
| `web/src/lib/types.ts` | model (TS unions/interfaces) | — | **`diff.py` enums + `api.py` serializers (contract)** | contract-anchor |
| `web/src/lib/tree.ts` | utility (pure transform) | transform | **RESEARCH Pattern 1 (provided code)** | spec-provided |
| `web/src/lib/status.ts` | utility (status→badge map) | transform | **UI-SPEC badge palette + `diff.py` enums** | contract-anchor |
| `web/src/App.tsx` | component (layout shell) | event-driven | none (UI-SPEC §Interaction Contract is the spec) | no analog |
| `web/src/components/InstallPicker.tsx` | component | request-response | `GET /installs` contract | contract-anchor |
| `web/src/components/SetDeltaStrip.tsx` | component | request-response | `POST /compare/set` contract | contract-anchor |
| `web/src/components/FileTree.tsx` | component (virtualized) | transform/event-driven | **RESEARCH Pattern 1 (provided code)** | spec-provided |
| `web/src/components/DetailPanel.tsx` | component (Sheet) | request-response | `POST /file/detail` contract | contract-anchor |
| `web/src/components/SummaryStats.tsx` | component | transform | `compare/files` `summary` contract | contract-anchor |
| `web/src/lib/tree.test.ts`, `status.test.ts` | test (Vitest) | — | none in-repo (pytest is Python) | no analog |

### Backend touch (real in-repo analogs exist)

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|-------------------|------|-----------|----------------|---------------|
| `src/tre_compare/web_static.py` (NEW) | middleware/route (static mount) | file-I/O | `src/tre_compare/config.py` (small stateless stdlib module + the `api.py` mount site) | role-match |
| `src/tre_compare/__main__.py` (MODIFIED) | config/entrypoint | — | **itself** (preserve `127.0.0.1` bind) | exact |
| `src/tre_compare/api.py` (MODIFIED — mount line only) | route registration | — | **itself** (`create_app`) | exact |
| `tests/test_web_static.py` (NEW) | test | — | **`tests/test_api.py` + `conftest.py`** | exact |

## Pattern Assignments

### Frontend type/contract files — anchor on api.py, not a React analog

#### `web/src/lib/types.ts` (model — contract-anchor)

There is no TS analog. The planner must transcribe these **verified** enum literals and serializer
shapes from the backend. **Set-level and file-level statuses are DIFFERENT enums — two unions, never
one** (RESEARCH Pitfall 3; UI-SPEC §Color).

**Set-level statuses** — `diff.py` lines 196–212 emit exactly:
```
"fault" | "added" | "removed" | "changed" | "identical"
```
(confirmed against `test_api.py:24`: `_VALID_SET_STATUSES = {"added","removed","changed","identical","fault"}`)

**File-level statuses** — `diff.py` lines 278–289 emit exactly:
```
"identical-by-metadata" | "changed" | "removed" | "added" | "tombstoned"
```
(confirmed against `test_api.py:25`: `_VALID_FILE_STATUSES = {"added","removed","changed","identical-by-metadata","tombstoned"}`)

**Detail verdict** — `api.py` lines 208–217 + 422:
```
"content-confirmed-identical" | "content-changed" | "content-indeterminate" | null
```

**Serializer field names — transcribe verbatim** (`api.py` lines 243–260):
```python
# _merged_meta  (api.py:243-246)  -> {"len": int, "clen": int} | null
# _node_brief   (api.py:249-254)  -> {"archive": str, "kind": "tree"|"toc", "priority": int}
# _hash_brief   (api.py:257-260)  -> {"hexdigest": str|null, "error": str|null, "source_archive": str} | null
```

`/file/detail` 200 body (`api.py:219-231`), TS interface to mirror exactly:
```
path, status:"ok", winning_archive_left|right: str|null,
left|right: {len,clen}|null, shadowed_left|right: _node_brief[],
hash_left|right: _hash_brief|null, verdict
```

`/compare/files` row + summary — `summary` shape from `diff.py:315-320`:
```
summary.left|right = {node_errors:int, rejected:int, tombstoned:int}
summary.status_counts = { "<file-status>": int }
```

`qualifier` (optional row field, `test_api.py:87-94`): `string[]` of
`tombstoned_left/right | rejected_left/right | error_left/right`.

#### `web/src/lib/api.ts` (utility — contract-anchor)

No in-repo analog. The relative-path typed-fetch wrapper is fully specified in RESEARCH §Code
Examples (lines 538–555). Key rules from the contract:
- Relative paths only (`fetch("/compare/files")`) — dev-proxy + prod-same-origin share one code path.
- 400 envelope is `{detail:{error, cfg}}` (`api.py:83` / `test_api.py:163`); `/file/detail` 404 is
  `{detail:{error:"not found", path}}` (`api.py:187`), 400 is `{detail:{error:"rejected path", path}}`
  (`api.py:189`). The wrapper must surface `detail` on non-`ok`.

#### `web/src/lib/tree.ts` + `web/src/components/FileTree.tsx` (transform — spec-provided)

No in-repo analog; the **complete reference implementation is in RESEARCH Pattern 1**
(`30-RESEARCH.md` lines 203–298: `buildFolderTree`, `flattenVisible`, `computeRollup`, and the
`useVirtualizer` wiring). Locked constraints to carry verbatim:
- Build tree ONCE per `/compare/files` response (`useMemo` on `rows`); only `flattenVisible` re-runs.
- Fixed `estimateSize: () => 28` (UI-SPEC §Spacing locks 28px — do NOT round to 24/32).
- `getItemKey: (i) => visible[i].path` (stable across re-flatten).
- Hide-identical must suppress now-empty folders via `rollup` counts (RESEARCH Pitfall 6).

#### `web/src/lib/status.ts` (transform — contract-anchor)

No analog. Spec = UI-SPEC §"Status / Verdict Badge Palette" (lines 113–125) keyed on the verified
enums above. **HARD RULE** (UI-SPEC line 127 / CONTEXT D-04): `identical-by-metadata` (neutral/grey
"≈ metadata") must render visually distinct from `content-confirmed-identical` (solid green
"= content"). Two separate badge maps for the two enums.

### Backend touch files — real in-repo analogs

#### `web_static.py` (NEW — middleware/static-mount, file-I/O)

**Analog:** `src/tre_compare/config.py` — both are small, stateless, single-purpose stdlib modules
that live beside the engine. config.py's `DEFAULT_INSTALLS_PATH` resolution pattern is the one to
mirror for locating `web/build/`.

**Path-resolution pattern to copy** (`config.py:22-24`):
```python
# config.py is at src/tre_compare/config.py -> parents: tre_compare, src, tre-compare
DEFAULT_INSTALLS_PATH = Path(__file__).resolve().parent.parent.parent / "installs.toml"
```
RESEARCH Pattern 2 (lines 333) uses the identical 3-level `.parent` walk to reach `web/build/`:
```python
WEB_DIR = Path(__file__).resolve().parent.parent.parent / "web" / "build"
```
This is consistent with config.py's convention — copy that resolution shape exactly. The
`SpaStaticFiles(StaticFiles)` subclass body is provided in RESEARCH Pattern 2 (lines 319–328).

#### `__main__.py` (MODIFIED — preserve the bind, exact analog = itself)

**Analog:** itself. The localhost bind is a security invariant (T-29-03-03). Lines 16–23:
```python
HOST = "127.0.0.1"   # Localhost ONLY — MUST NOT bind all-interfaces
PORT = 8765
def main() -> None:
    app = create_app()
    uvicorn.run(app, host=HOST, port=PORT)
```
Any change here must NOT widen the host. The SPA serve is additive in `create_app`, not a new
listener (RESEARCH Security: "no second open port").

#### `api.py` (MODIFIED — mount line only, exact analog = itself)

**Analog:** the existing `create_app` (`api.py:149-237`). The SPA mount is **one block appended
AFTER all four routes are registered and before `return app`** (RESEARCH Pattern 2 lines 331–335;
Pitfall 2: a `/` mount is greedy — registering it before the routes shadows `/compare/*`). The
existing route-registration order inside `create_app` is the pattern; insert the mount at line ~236,
immediately before `return app`.

#### `tests/test_web_static.py` (NEW — exact analog = test_api.py)

**Analog:** `tests/test_api.py` + `tests/conftest.py` — copy this structure directly.

**TestClient + injected-cache fixture pattern** (`conftest.py:56-65`):
```python
@pytest.fixture
def api_client(tmp_cache, tmp_installs):
    from fastapi.testclient import TestClient
    from tre_compare.api import create_app
    app = create_app(cache=tmp_cache, installs_path=tmp_installs)
    with TestClient(app) as client:
        yield client
```
**Route-shape assertion style to copy** (`test_api.py:70-84`) and the **mount-order regression** the
new test must add: assert `/compare/files` still returns JSON (not `text/html`) AND `GET /` returns
the SPA `index.html` when `web/build/` exists. Use a `tmp_path` `web/build/index.html` fixture so the
test does not depend on a real `npm run build`.

## Shared Patterns

### Localhost-only bind (security invariant — applies to every serve path)
**Source:** `__main__.py:16-23` (`HOST = "127.0.0.1"`), documented in `api.py:25-26`.
**Apply to:** `web_static.py`, the `api.py` mount, `__main__.py`. Never introduce a second
listener or widen the host. The 127.0.0.1 bind is the single access control (T-29-03-03).

### Stateless app factory + injected cache (no module-level singletons)
**Source:** `api.py:149-160` (`create_app(cache=None, installs_path=None)`).
**Apply to:** the SPA mount (added inside `create_app`, not at module scope) and every new test
(inject `tmp_cache` + `tmp_installs` per `conftest.py`). A module-level Cache/app leaks state across
TestClient tests — the factory pattern is mandatory (`api.py:1-6` rationale).

### Tolerant degradation — backend never 500s; UI must surface faults
**Source:** `api.py:167-169` (corrupt archive → `fault` row), `config.py:35-64` (missing/malformed
config → `[]`), `diff.py:315-320` (`node_errors`/`rejected`/`tombstoned` in summary).
**Apply to:** every frontend component. Render `fault` rows (`SetDeltaStrip`), empty-picker on `[]`
(`InstallPicker`), `node_errors`/`rejected` counts (`SummaryStats`), and `hash.error` →
`content-indeterminate` (`DetailPanel`). UI-SPEC §Copywriting Contract has the exact strings.

### `dist/` name collision (build-config landmine)
**Source:** `tools/tre-compare/.gitignore` line `dist/` (verified — it is the **Python** build
artifact ignore). Vite defaults `build.outDir` to `dist/`, which would be silently gitignored.
**Apply to:** `web/vite.config.ts` — set `build: { outDir: "build" }` (RESEARCH Pattern 2 line 349;
Pitfall 1). Mount `web/build/`, not `web/dist/`.

### No `dangerouslySetInnerHTML` for backend strings (XSS posture)
**Source:** RESEARCH §Security V5. `path`, `error`, `fault`, archive names come from filesystem /
archive data. React's default text-escaping is the control.
**Apply to:** every component rendering `path`, `basename`, `fault_left/right`, `hash.error`.

## Field-Gap Flag (carry into planning — NOT a backend edit)

**The four frozen routes expose NO `crc` field** — verified this pass: `_node_brief` (`api.py:249-254`)
emits only `{archive, kind, priority}`; `_merged_meta` (`api.py:243-246`) emits only `{len, clen}`;
`diff.py` deliberately never emits the TOC path-CRC. CONTEXT D-04 / SC#3 ask for a "path-CRC — not
content" display. The UI therefore **cannot render a numeric path-CRC without a contract change the
frozen scope forbids.** RESEARCH Open Question 1 + UI-SPEC §"CRC display — field-gap flag" recommend
Option 1: render the explanatory labeled-absence note, no numeric value. The planner must pick and
record this; a task must NOT silently add a backend field.

## No Analog Found

Files with no in-repo code analog — planner uses the cited spec instead:

| File(s) | Role | Reason / spec to use instead |
|---------|------|------------------------------|
| All `web/` config (`package.json`, `vite.config.ts`, `tsconfig*`, `components.json`, `index.html`) | config | First frontend in repo. Use RESEARCH §Standard Stack install block (lines 131–141) + Pattern 2 vite.config (lines 339–357). |
| `web/src/App.tsx` | component | No layout analog. Use UI-SPEC §Visual Hierarchy + §Interaction Contract (D-01 linked-lens layout). |
| `web/src/lib/api.ts` | utility | No fetch-client analog. Anchor on `api.py` routes; code in RESEARCH lines 538–555. |
| `web/src/lib/types.ts` | model | No TS analog. Transcribe enums (`diff.py`) + serializers (`api.py`) — verified above. |
| `web/src/lib/tree.ts`, `components/FileTree.tsx` | transform/component | No virtualization analog. RESEARCH Pattern 1 is the provided implementation. |
| `web/src/lib/status.ts` | transform | No badge-map analog. UI-SPEC badge palette + verified enums. |
| `web/src/components/{InstallPicker,SetDeltaStrip,DetailPanel,SummaryStats}.tsx` | component | No React analog. Each anchors on its route's contract (api.py serializers) + UI-SPEC copywriting. |
| `web/src/lib/*.test.ts` | test | Vitest has no in-repo analog (existing tests are pytest/Python). RESEARCH §Validation names the pure-function seams to cover. |

## Metadata

**Analog search scope:** `tools/tre-compare/` (full backend + tests), repo-wide `**/*.{tsx,jsx}`,
repo-wide `**/package.json`, `.gitignore`, `installs.toml.example`.
**Files scanned (read this pass):** `api.py`, `__main__.py`, `config.py`, `tests/test_api.py`,
`tests/conftest.py`, `diff.py` (enum lines via Grep), `.gitignore`, `installs.toml.example`.
**Frontend analogs in repo:** zero (confirmed greenfield).
**Pattern extraction date:** 2026-06-15
