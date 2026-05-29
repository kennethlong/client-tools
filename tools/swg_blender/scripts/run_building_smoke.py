"""Fast building workspace smoke (default: blacksun_transport_player, 21 cells)."""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

DEFAULT_POB = "appearance/blacksun_transport_player.pob"


def main() -> int:
    parser = argparse.ArgumentParser(description="Building import + POB rewrite smoke")
    parser.add_argument("--pob", default=DEFAULT_POB, help="POB TreeFile relpath")
    parser.add_argument("--workspace", type=Path, help="reuse workspace (else temp)")
    parser.add_argument("--blender", action="store_true")
    args = parser.parse_args()

    import tempfile
    from swg_pipeline.tre_project_building import import_building_project, rewrite_building_pob

    ws = args.workspace or Path(tempfile.mkdtemp(prefix="swg_bld_smoke_"))
    print("workspace:", ws)
    print("pob:", args.pob)
    imp = import_building_project(ws, args.pob)
    print("materialized:", imp.as_dict().get("materialized_count"), "missing:", len(imp.missing))
    if imp.missing:
        return 1
    from swg_pipeline.tre_project import load_project_manifest

    manifest = load_project_manifest(ws)
    cells = manifest.get("cells") or []
    print("cells:", len(cells))
    rewrite_building_pob(ws, manifest)
    print("pob rewrite ok")

    if args.blender:
        from swg_pipeline.tre_project_building import run_blender_building_roundtrip, _resolve_blender_exe

        print("blender:", _resolve_blender_exe())
        proc = run_blender_building_roundtrip(ws)
        if proc.returncode != 0:
            print((proc.stderr or proc.stdout or "")[-1500:], file=sys.stderr)
            return proc.returncode
        print("blender roundtrip ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())