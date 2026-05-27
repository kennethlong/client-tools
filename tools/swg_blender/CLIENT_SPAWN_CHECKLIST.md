# SWG client spawn validation checklist

Use this after exporting a test bundle from Blender or `swg_pipeline.export_bundle`.

**Non-expert human E2E:** use **[HUMAN_E2E_TEST_GUIDE.md](HUMAN_E2E_TEST_GUIDE.md)** first (plain language, pass/fail tables). This file is the technical companion.

## 1. Bundle layout

Confirm the export folder mirrors serverdata paths:

- `appearance/mesh/<name>.msh`
- `shader/<name>.sht` (when a reference shader was resolved)
- optional `texture/**/*.dds` (when copy-textures is enabled)
- `rsp/data_*.rsp` (TreeFileRspBuilder-style manifest for packing)
- `client_search_paths.cfg` (snippet for `client_d.cfg`)
- `swg_export_manifest.json` (machine-readable summary)

## 2. Mount loose assets

Add the bundle root to the client filesystem search list (highest priority wins):

```ini
[SharedFile]
searchPath0=D:\path\to\your\bundle
```

Copy lines from `client_search_paths.cfg` or the manifest `client_test_notes` field.

Restart the client after editing `client_d.cfg` (or your active cfg).

## 3. In-game validation

| Check | Pass criteria |
| --- | --- |
| Mesh loads | Object using the mesh template appears; no pink shader / missing mesh log spam |
| Scale / orientation | Prop matches expected floor contact and facing (import uses +90° X on object) |
| Shader | `.sht` resolves; MAIN texture visible (retail paths may load from stock TREs) |
| Animation (if applicable) | `.ans` plays on skeleton; CKAT round-trip assets use default `ckat` export |

## 4. Packing for distribution (optional)

When building a `.tre` with `TreeFileBuilder.exe`:

1. Run `python -m swg_pipeline.rsp_builder <bundle_root> --output-dir publish/rsp`
2. Point `TreeFileRspBuilder` / builder config `searchPathN` entries at the same roots used for the rsp scan
3. Feed generated `data_compressed_mesh_static.rsp` (etc.) to the builder workflow documented in `docs/data-pipeline.html`

## 5. Common failures

- **Pink mesh** — shader path wrong or `.sht` not on search path; check `shader/` in bundle
- **Invisible mesh** — template still references old appearance path; update object template or mesh relpath
- **Wrong orientation** — do not bake +90° X into vertices; re-import with stock `import_static.py`
- **pytest "null bytes"** — Python file saved as UTF-16 by Cursor `Write` on Windows; rewrite as UTF-8 (see `AGENTS.md` → *Windows UTF-16 corruption*). Use `StrReplace` or PowerShell `WriteAllText`, not `Write`.

## CLI quick reference

```powershell
cd tools/swg_blender

# Phase 7 — combined static + skeletal dev bundle (recommended)
python -m swg_pipeline.export_bundle phase7
python -m swg_pipeline.export_bundle phase7 --output-dir D:\swg_dev_bundle

# Static only
python -m swg_pipeline.export_bundle static --mesh tests/golden/frn_all_bed_sm_s1_l0.msh --output-dir D:\swg_dev_bundle_static

# Legacy static (still supported)
python -m swg_pipeline.export_bundle --mesh tests/golden/frn_all_bed_sm_s1_l0.msh --output-dir D:\swg_dev_bundle

python -m swg_pipeline.rsp_builder D:\swg_dev_bundle --output-dir D:\swg_dev_bundle\rsp
```

See `PHASE7_VALIDATION.md` for the full M8 client checklist.