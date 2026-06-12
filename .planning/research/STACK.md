# Stack Research — Web-based TRE Compare Tool (v2.3 Hardening)

**Domain:** Local web-based TRE archive compare/diff tool (single-user, Windows 11, hobby)
**Researched:** 2026-06-12
**Confidence:** HIGH

> Scope note: this file covers ONLY the v2.3 new capability — the TRE compare web tool. The C++/MSBuild
> client and its D3D9/D3D11 renderers need no stack research (existing validated toolchain). The prior
> v2.2 renderer-stack research is archived as `STACK-v2.2-d3d11-shader-pipeline.md`.

## TL;DR Recommendation

**Python 3.12 + FastAPI + Uvicorn backend, React 19 + Vite 8 + TypeScript + Tailwind v4 + shadcn/ui frontend, shipped as a localhost server you open in a browser.**

The single decisive factor: a mature, version-aware TRE parser **already exists in Python** at
`D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py` (~470 lines; handles 5 version tags —
`0004/0005/6000/0006/5000` — plus COT2000 and SearchTOC master indexes, zlib decompression, and
filename-block walking; backed by golden-file tests). Reimplementing that version matrix in
Go/Rust/TypeScript is the largest single risk and effort sink in the entire tool, and it buys nothing
for a tool that scans a few thousand files once per run on a local SSD. **Reuse the parser; wrap it in
a thin API; spend the effort budget on the UI.**

This is **not** a desktop-shell (Tauri/Electron) candidate — see "What NOT to Use." A plain localhost
FastAPI server with a Vite-built SPA is simpler, needs no packaging/signing/WebView toolchain, and uses
the natural local-data model: the server reads the disk, the browser just renders JSON.

> NOTE ON THE PROJECT'S MEMORY: the sibling parser repo is referenced in `PROJECT.md` as `D:/Code/swg-tools`,
> but it actually lives at **`D:/Code/swg-blender-plugin/swg_pipeline/`** (verified this session;
> `D:/Code/swg-tools` does not exist). Use the real path.

## Recommended Stack

### Core Technologies

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| **Python** | 3.12.x | Backend runtime + TRE parsing | The existing `tre_reader.py` reference is Python; reuse beats reimplementation. 3.12 is stable, fast, fully supported on Win11. (Parser is pure-stdlib so 3.13/3.14 also work; 3.12 is the safe floor.) |
| **FastAPI** | 0.136.x | HTTP/JSON API layer | Minimal-ceremony typed endpoints, automatic OpenAPI docs, async file streaming, trivial to serve a built SPA as static files. Latest 0.136.3 (2026-05-23). |
| **Uvicorn** | 0.34.x | ASGI server | Standard FastAPI dev+prod server; `uvicorn app:app` binds `127.0.0.1` and you're done. `--reload` in dev. |
| **React** | 19.2.x | Frontend UI framework | Latest stable 19.2.7 (2026-06-01). Broadest component-lib support for tree/diff UIs; React Compiler removes most manual memoization (helps a virtualized 10k-row file tree stay snappy). |
| **Vite** | 8.0.x | Frontend build/dev tooling | Latest 8.0.9 (2026-04-20). Instant HMR, one-command React+TS scaffold, Rust-based toolchain; zero-config SPA that proxies `/api` to Python in dev. |
| **TypeScript** | 5.6+ | Frontend type safety | Catches shape drift between API DTOs and UI; pairs with FastAPI's OpenAPI to optionally codegen client types. |

### Supporting Libraries

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| **Tailwind CSS** | v4.x | Utility-first styling | Always — the "clean modern UI" requirement. shadcn/ui CLI initializes v4 projects natively (OKLCH theme, `@theme` directive, no `tailwind.config.js`). |
| **shadcn/ui** | latest (CLI) | Copy-in component primitives | Always — accessible, themed Table, Tabs, Command-palette, ScrollArea, Badge components you own as source (no runtime dep lock-in). Updated for Tailwind v4 + React 19 (`data-slot`). |
| **TanStack Virtual** | v3.x | Row virtualization | Required for the file-level diff view — the merged virtual tree can be 10k–100k+ entries; render only visible rows or the tree janks. |
| **TanStack Query** | v5.x | Server-state fetching/cache | Recommended — manages the scan-A / scan-B / diff async lifecycle, loading/error states, and caches scan results so re-diffing is instant. |
| **xxhash** (Python) | latest | Fast content hashing | Recommended over `hashlib.sha256` for change-detection: xxh3 is ~10x faster; you need change-detection, not cryptographic strength. Used only on the deep-diff path (see fast path below). |
| **orjson** (Python) | latest | Fast JSON serialization | Optional — large diff payloads (tens of thousands of entries) serialize much faster than stdlib `json`; FastAPI supports `ORJSONResponse`. |

### Development Tools

| Tool | Purpose | Notes |
|------|---------|-------|
| **uv** (Astral) | Python env + deps | Fast modern pip/venv replacement; `uv venv` + `uv pip install`. Cleaner than Conda for one tool. |
| **pnpm** (or npm) | Frontend package manager | pnpm is faster/disk-cheaper; npm is fine. Either works with Vite. |
| **Ruff** | Python lint/format | Single fast tool. |
| **Vite proxy** | Dev-time API routing | `server.proxy: { '/api': 'http://127.0.0.1:8000' }` so the SPA dev server forwards API calls — no CORS dance in dev. |

## Architecture Shape: "Web-based but local-filesystem"

**Recommended shape — localhost server + browser (NOT a desktop shell):**

```
┌─────────────────┐         ┌──────────────────────────┐
│  Browser tab    │  HTTP   │  FastAPI / Uvicorn       │
│  (Vite SPA,     │ ──────► │  127.0.0.1:8000          │
│   React+shadcn) │ ◄────── │   ├─ /api/scan?path=...  │
└─────────────────┘  JSON   │   ├─ /api/diff           │
                            │   └─ serves built SPA    │
                            │  tre_reader.py (REUSED)  │
                            │   reads C:\...\*.tre      │
                            └──────────────────────────┘
```

- The **Python server** does all filesystem work (enumerate `.tre` archives, read TOCs, hash payloads,
  build the merged virtual tree). The browser never touches the disk — it renders JSON. This sidesteps
  the browser sandbox's filesystem restrictions entirely, which is the whole reason "web-based local
  tool" is usually awkward.
- **Production run** = `uvicorn app:app` + `vite build` output served as static files from the same
  FastAPI app on one port. Open `http://127.0.0.1:8000`. One process, no installer.
- **Dev run** = Uvicorn on :8000, Vite dev server on :5173 with a proxy to :8000. HMR on the UI,
  `--reload` on the API.
- A trivial launcher (`.bat` or `python -m`) can `start` the browser at the URL once the server binds.
  That's the entire "desktop integration" a single-user local tool needs.

**Why a server, not just static files in the browser:** the browser File System Access API is
Chromium-only, needs per-folder user-gesture grants, can't efficiently random-access read inside large
`.tre` archives, and can't background-hash thousands of entries — you'd fight the sandbox the whole way.
A local server is the standard friction-free answer for local-data tools (cf. Jupyter, Streamlit,
ComfyUI — all localhost servers).

## TRE Parser Reuse — the Integration Point

**Do NOT reimplement TRE parsing.** `D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py` already
provides, version-aware:

- `read_tre_header()` / `read_tre_entries()` — per-archive TOC + filename-block parse (`0004/0005`
  24-byte stride, `6000/0006` 32-byte stride).
- `read_search_toc_entries()` / `read_cot2000_entries()` — the two master-index layouts that map a
  merged virtual tree across many `.tre` files (exactly the "search-path-order resolution, later archive
  wins" model the tool needs).
- **`TreEntry.crc` — every TOC entry already carries SOE's per-file CRC.** For set-level and file-level
  "changed?" detection, compare `(length, crc)` straight from the TOCs **without decompressing payloads
  at all** in the fast path; only zlib-decompress + xxhash when CRCs collide or you want byte-exact
  confirmation. This makes "diff two whole installations" cheap (TOC reads are tiny).
- `read_tre_payload()` — on-demand zlib decompress for the deep-diff path.

**Integration recommendation:** `pip install -e D:/Code/swg-blender-plugin` (editable local install)
so `import swg_pipeline.tre_reader` works and fixes to the version matrix propagate — single source of
truth. Vendoring a copy of `tre_reader.py` (+ `tre_decrypt.py`) is the more self-contained alternative;
editable-install is preferred since both repos are local and single-developer.

The tool's own code is then thin: walk a SWG install dir for `.tre` files in `searchPath` order, build
a `{logical_path: winning_entry}` map per installation (later archive wins), set-diff the archive lists,
file-diff the two merged maps by `(length, crc)` → emit added/removed/changed JSON.

## Installation

```bash
# --- Backend (Python) ---
uv venv && . .venv/Scripts/activate            # or: python -m venv .venv
uv pip install "fastapi==0.136.*" "uvicorn[standard]==0.34.*" xxhash orjson
uv pip install -e D:/Code/swg-blender-plugin   # reuse swg_pipeline.tre_reader (single source of truth)

# --- Frontend (Node) ---
pnpm create vite@latest web -- --template react-ts   # Vite 8 + React 19 + TS
cd web
pnpm add -D tailwindcss@4 @tailwindcss/vite
pnpm add @tanstack/react-virtual @tanstack/react-query
pnpm dlx shadcn@latest init                          # Tailwind v4 + React 19 components
pnpm dlx shadcn@latest add table tabs scroll-area badge command
```

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative |
|-------------|-------------|-------------------------|
| **Python + FastAPI** | **Go** | If the parser did NOT already exist and you wanted one static `.exe`, no runtime. Go's stdlib (`hash`, `compress/zlib`, `net/http`) is great for this and ships one binary — but you'd reimplement the whole 5-version TRE matrix, the tool's hardest part. Not worth it here. |
| **Python + FastAPI** | **Rust (axum/actix)** | If hashing throughput on millions of files were the bottleneck. It isn't — the TOC `(length, crc)` fast path avoids decompression, and a few thousand files on an SSD is sub-second in Python. Rust would also mean a full parser rewrite. |
| **Python + FastAPI** | **Node/TypeScript backend** | If you wanted one language across the stack. Still means porting the TRE parser to TS (the version-matrix logic is the cost) and Node's CPU-bound hashing isn't faster than Python+xxhash. The unified-language win doesn't pay for the rewrite. |
| **React + Vite** | **SvelteKit** | Smallest bundle / least boilerplate for a personal tool. A fine choice; React is recommended only because shadcn/ui + TanStack Virtual/Query give the richest *off-the-shelf* table/tree/diff components. Pick Svelte if you already prefer it. |
| **React + Vite** | **FastAPI + Jinja2 + htmx** | Zero frontend build step; viable and low-ceremony when the UI is mostly tables. Rejected as primary because "modern clean diff/tree UI with virtualization" benefits from a real component framework. |
| **localhost server** | **Tauri 2** | If this ever became a *distributable* app for non-developers (single signed installer, native window). For a single-developer local tool it adds a Rust toolchain + WebView + packaging for no benefit. |

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| **Electron** | 100s of MB bundle (ships Chromium + Node), heavy RAM, packaging/signing/auto-update machinery — all pure overhead for a single-user localhost tool. | localhost FastAPI + browser |
| **Tauri 2 (this milestone)** | Adds a Rust toolchain, WebView capability/permission config, and an installer build just to wrap a UI you can open in a browser you already have. Justified only if distribution to non-devs becomes a goal. | localhost FastAPI + browser |
| **Reimplementing the TRE parser** (Go/Rust/TS) | The 5-version TOC matrix + COT2000/SearchTOC master indexes + zlib + filename-block walking is ~470 lines of carefully version-aware Python that already works and is golden-file tested. Rewriting it is the project's biggest risk for zero functional gain. | Reuse `swg_pipeline.tre_reader` |
| **Browser File System Access API** (static SPA, no server) | Chromium-only, per-folder gesture grants, no efficient random-access into large archives, can't background-hash thousands of files. Fights the sandbox. | Server-side filesystem access |
| **`hashlib.sha256` for change-detection** | Cryptographic strength is unnecessary and ~10x slower; you're detecting changes, not defending against adversaries. | Compare TOC `(length, crc)` first; xxh3 only for byte-exact confirmation |
| **Conda** for env management | Heavyweight for one small tool; slow solves on Windows. | `uv` (or plain `venv`) |

## Stack Patterns by Variant

**If you want the absolute minimum surface area (acceptable UI):**
- Drop React/Vite; serve Jinja2 templates from FastAPI with htmx for partial updates.
- Because: no Node toolchain at all, one language; the diff/tree views are mostly tables, and htmx
  handles "scan / expand node / load diff" with server-rendered fragments. Trade-off: less polished
  virtualized scrolling. Keep React as primary if the "modern clean UI" bar is high.

**If this tool ever ships to non-developer SWG community users:**
- Wrap the same React SPA + a bundled Python (PyInstaller), or move the backend into Tauri 2. Tauri's
  scoped-folder filesystem permissions fit a "pick two install dirs" UX well.
- Because: distribution to non-devs wants a single signed installer, not "run uvicorn then open a
  browser." Out of scope for v2.3 (single-developer, local).

**If the dataset grows to millions of files and hashing dominates:**
- Parallelize hashing with a `ProcessPoolExecutor` (sidesteps the GIL for CPU-bound zlib+hash); keep
  the TOC-CRC fast path. Only then consider a Go/Rust hashing core called from Python.
- Because: real SWG installs are ~thousands of archives / low-hundreds-of-thousands of entries —
  comfortably within single-process Python with the CRC fast path. Don't pre-optimize.

## Version Compatibility

| Package A | Compatible With | Notes |
|-----------|-----------------|-------|
| FastAPI 0.136.x | Python 3.12 / Uvicorn 0.34.x | Use `uvicorn[standard]` for http-tools/websocket extras. |
| React 19.2.x | Vite 8.0.x / TypeScript 5.6+ | `@vitejs/plugin-react` (or `-swc`) supports React 19. |
| Tailwind v4 | shadcn/ui (latest CLI) / Vite 8 | shadcn CLI initializes v4 natively; use `@tailwindcss/vite` plugin; v4 config moves to CSS `@theme` (no `tailwind.config.js`). |
| shadcn/ui (latest) | React 19 / Tailwind v4 | Components updated for both (`data-slot`, OKLCH, `forwardRef` removed per React 19). |
| `swg_pipeline.tre_reader` | Python 3.12 / 3.13 / 3.14 | Pure-stdlib (`struct`, `zlib`, `pathlib`); no version constraint. Golden-tested against a `0005` TRE. |
| TanStack Virtual v3 / Query v5 | React 19 | Both support React 19. |

## Sources

- `D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py` — HIGH: the existing version-aware TRE parser
  (read directly this session); confirms reuse is viable and that the per-entry CRC in the TOC enables a
  decompression-free diff fast path.
- `D:/Code/swg-blender-plugin/docs/research/tre-project-roundtrip-SPEC.md` — HIGH: confirms the Python
  pipeline is the established TRE tooling line.
- [FastAPI release notes / PyPI](https://fastapi.tiangolo.com/release-notes/) — HIGH: latest 0.136.3 (2026-05-23).
- [Vite releases](https://vite.dev/releases) — HIGH: latest 8.0.9 (2026-04-20).
- [React 19.2 blog / GitHub releases](https://react.dev/blog/2025/10/01/react-19-2) — HIGH: latest 19.2.7 (2026-06-01).
- [shadcn/ui Tailwind v4 docs](https://ui.shadcn.com/docs/tailwind-v4) — HIGH: confirms Tailwind v4 + React 19 support.
- [Electron vs Tauri 2026 guide](https://www.pkgpulse.com/guides/electron-vs-tauri-2026) — MEDIUM: bundle-size/permission-model comparison informing the "no desktop shell" decision.

---
*Stack research for: local web-based TRE archive compare/diff tool (v2.3 Hardening)*
*Researched: 2026-06-12*
