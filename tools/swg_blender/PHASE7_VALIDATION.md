# Phase 7 — Client validation (M8)

**Status:** ✅ **Complete** — in-game validation run 2026-05-26 (tester sign-off).

**Human testers (non-Blender / non-SWG experts):** follow **[HUMAN_E2E_TEST_GUIDE.md](HUMAN_E2E_TEST_GUIDE.md)** — step-by-step preflight, cfg mount, visual pass/fail tables, and result template.

## Generate the dev bundle

```powershell
cd D:\Code\swg-client-v2\tools\swg_blender
python -m swg_pipeline.export_bundle phase7
# optional: copy MAIN textures from swg-main
python -m swg_pipeline.export_bundle phase7 --copy-textures
```

Default output: `D:\swg_dev_bundle\`

| File | Purpose |
| --- | --- |
| `appearance/mesh/frn_all_bed_sm_s1_l0.msh` | Static prop test |
| `appearance/mesh/abyssin_m_arms_l0.mgn` | Skeletal mesh test |
| `appearance/skeleton/all_b.skt` | Humanoid skeleton |
| `appearance/animation/all_b_cbt_pistol_run_ready.ans` | Sample animation |
| `shader/*.sht` | Stub shaders for both assets |
| `rsp/data_*.rsp` | TreeFileBuilder manifests |
| `client_search_paths.cfg` | Paste into client cfg |
| `PHASE7_SPAWN_NOTES.md` | Per-asset spawn guidance |
| `swg_phase7_manifest.json` | Machine-readable summary |

## Mount in client

1. Open your active cfg (e.g. `client_d.cfg` next to `SwgClient.exe`).
2. Under `[SharedFile]`, set highest-priority search path:

```ini
searchPath0=D:\swg_dev_bundle
```

3. Restart the client fully.

## Validation checklist

Record results here after your session:

| Check | Static bed | Skeletal arms | Notes |
| --- | --- | --- | --- |
| Asset resolves (no log errors) | ☑ | ☑ | |
| Visible geometry (not pink) | ☑ | ☑ | |
| Scale / orientation OK | ☑ | ☑ | |
| Shader / MAIN texture OK | ☑ | ☑ | |
| Animation plays (skeletal only) | n/a | ☐ optional | |

**Client build tested:** koogie-msvc-cpp20-base Debug client  
**Date:** 2026-05-26  
**Tester:** User sign-off (session)

## Automated preflight (no client required)

```powershell
python -m pytest tests/test_export_bundle.py tests/test_phase7_bundle.py -q
python -m swg_pipeline.validate --mesh D:\swg_dev_bundle\appearance\mesh\frn_all_bed_sm_s1_l0.msh
```

## If something fails

| Symptom | Likely fix |
| --- | --- |
| Pink mesh | `.sht` not on search path; check `shader/` in bundle |
| Invisible mesh | Object template still points at old mesh path |
| Wrong rotation | Re-import with stock addon (do not bake +90° X into verts) |
| Skeletal spaghetti | SKT/MGN path mismatch; verify `all_b.skt` resolves |
| Anim silent | `.ans` path or skeleton mismatch |

## Phase 7 exit criteria

- [x] Static prop visible in client (M2 L proof)
- [x] Skeletal mesh on humanoid visible (M4 L proof)
- [ ] Optional: test animation plays on spawned creature
- [x] Results recorded in this file

**M8 client validation gate:** closed for static + skeletal mesh load. Optional anim left open.
