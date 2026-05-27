# Agent instructions (swg-client-v2)

Guidance for AI agents working in this repository.

## Session startup

Before substantial work, restore context from handoffs:

1. Read [.planning/handoff/README.md](.planning/handoff/README.md) for the active handoff index.
2. Read every handoff file listed there (newest / most relevant first).
3. If the task involves GSD milestone work, also read [.planning/STATE.md](.planning/STATE.md) and [.planning/PROJECT.md](.planning/PROJECT.md).

Handoffs live in **`.planning/handoff/`** — one markdown file per workstream, updated when context would otherwise be lost.

## Project constraints

- **Boot gate:** Changes should leave the client bootable to character select unless the task explicitly says otherwise.
- **GSD planning:** Milestone state, phases, and requirements are under `.planning/`.
- **Commits:** Only when the user asks; do not push unless asked.

## Workstreams (pointers)

| Area | Where to look |
| --- | --- |
| MSVC / Decruft milestone | `.planning/STATE.md`, `.planning/ROADMAP.md` |
| SWG Blender IFF import | `.planning/handoff/swg-blender-import.md`, `tools/swg_blender/`, `.cursor/skills/swg-blender-pipeline/` |
| MayaExporter / asset parity | `docs/research/maya-exporter-parity-checklist.md` |
| Engine / client code | `src/engine/` |

## Tooling notes

- GitHub CLI (`gh`) for issues and PRs when requested.
- Prefer matching existing code style and minimizing diff scope.

## Gotchas

### Windows UTF-16 corruption (Python files)

**We do not want UTF-16 for Python source.** All `.py` files must be **UTF-8** (with or without BOM). Python on Windows reads UTF-8 by default; UTF-16-LE files often fail at import with:

```text
SyntaxError: source code string cannot contain null bytes
```

#### What we know

| Observation | Detail |
| --- | --- |
| **Trigger** | Cursor agent **`Write`** on Windows, especially when creating or fully overwriting files under `tools/swg_blender/**/*.py` |
| **Symptom** | File saved as UTF-16-LE (or UTF-8 BOM + wrong wide-char path) — ASCII looks fine in some viewers but bytes contain `\x00` between characters |
| **Not intentional** | Nothing in this repo requires UTF-16 Python; it is a tooling encoding bug/quirk, not a project convention |
| **Likely cause** | Cursor Write path on Windows may use a Unicode API that defaults to UTF-16. Treat **`Write` on `.py` and handoff `.md` as unsafe** |
| **Safe alternative** | **`StrReplace`** on an already-valid UTF-8 file has been reliable for edits. For new files or full rewrites, use PowerShell `WriteAllText` or Python `write_text(..., encoding='utf-8')` via shell |

#### Rules for agents

1. **Do not use Cursor `Write`** on `tools/swg_blender/**/*.py` or `.planning/handoff/*.md`.
2. **Prefer `StrReplace`** for incremental edits to existing UTF-8 Python files.
3. **New files / full rewrites** — write via PowerShell or Python (see above).
4. **After editing** `tools/swg_blender/`, run pytest — corruption usually shows up at collection time.

#### Detect corruption

```powershell
python -c "from pathlib import Path; root=Path(r'tools/swg_blender');
[print(p) for p in root.rglob('*.py') if b'\x00' in p.read_bytes()]"
```

#### Repair

Rewrite the file as UTF-8 using one of the safe write methods above. Do not commit UTF-16 `.py` files.

See also: `tools/swg_blender/CLIENT_SPAWN_CHECKLIST.md`, `.cursor/skills/swg-blender-pipeline/SKILL.md`.

- **Blender 4.x:** `Mesh.use_auto_smooth` was removed — `import_static.py` uses try/except + per-poly `use_smooth`.

- **VTXA tag:** four chars `VTXA`, not `VTX ` — see `tools/swg_blender/swg_iff/tags.py`.

- **SWG import orientation:** X-flip lives in `coords.py`; +90° X rotation is applied on the **object** in `import_static.py` (`IMPORT_ROTATION_EULER`). Do not bake rotation into vertex coordinates — it did not match the validated manual workflow.