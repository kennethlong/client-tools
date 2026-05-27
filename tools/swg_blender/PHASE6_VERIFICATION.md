# Phase 6 verification (2026-05-26)

## Automated

```powershell
cd tools/swg_blender
python -m pytest tests/ -q
```

**Result:** 84 passed

## TRE listing (retail 0005)

```powershell
python -m swg_pipeline.tre_list -l tests/golden/retail_mini_0005.tre
```

**Result:** 2 paths (`texture/a.dds`, `shader/b.sht`), `toc_stride=24`

## TRE listing (Restoration 6000)

```powershell
python -m swg_pipeline.tre_list -l D:\Sample-TRE-Files\SwgRestoration_06.tre --head 5
```

**Result:** 54 files, `version='6000'`, `toc_stride=32`

## Master index (COT2000)

```powershell
python -m swg_pipeline.tre_list --master D:\Sample-TRE-Files\SwgRestoration.toc --head 3
```

**Result:** `kind=cot2000`, 213086 indexed paths, 45 archives

## Master index (SearchTOC synthetic)

Covered by `tests/test_tre_versions.py::test_build_search_toc_roundtrip`.

## API surface

| Function | Purpose |
| --- | --- |
| `read_tre_header` / `read_tre_entries` | Per-.tre with version-aware TOC stride |
| `read_tre_payload` | Retail zlib decompress |
| `detect_master_index_kind` | `cot2000` vs `search_toc` |
| `parse_master_index` | Unified summary dict |
| `read_search_toc_*` | Retail master `.toc` |
| `read_cot2000_*` | SwgRestoration master `.toc` |

## Known limits

- Restoration v6000 payloads may not zlib-decompress at rest (use `TreeFileExtractor`).
- Tags `0006` / `5000` raise `UnsupportedTreVersionError` until fixtures exist.