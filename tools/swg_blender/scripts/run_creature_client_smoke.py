"""Creature TRE workspace -> pack -> client mount instructions (M16 smoke)."""
from __future__ import annotations

import argparse
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

DEFAULT_SAT = "appearance/abyssin_m.sat"


def main() -> int:
    parser = argparse.ArgumentParser(description="Creature workspace + client pack smoke")
    parser.add_argument("--workspace", type=Path, help="existing workspace (else temp)")
    parser.add_argument("--sat", default=DEFAULT_SAT)
    parser.add_argument("--blender", action="store_true")
    parser.add_argument("--pack", action="store_true", help="run pack_bundle if tools exist")
    args = parser.parse_args()

    from swg_pipeline.tre_project import export_creature_project, import_creature_project

    ws = args.workspace or Path(tempfile.mkdtemp(prefix="swg_creature_smoke_"))
    print("workspace:", ws)
    imp = import_creature_project(ws, args.sat)
    print("materialized:", len(imp.materialized), "missing:", len(imp.missing))
    if imp.missing:
        return 1
    if args.blender:
        exp = export_creature_project(ws, use_blender=True)
        print("exported mgn:", len(exp.exported_mgn or []))
    if args.pack:
        try:
            from swg_pipeline.pack_pipeline import pack_bundle
            pack_bundle(ws, rebuild_rsp=True, update_manifest=True)
            print("pack ok (see workspace rsp/ and *.tre)")
        except Exception as exc:
            print("pack skipped:", exc)
    notes = ws / "CLIENT_MOUNT.txt"
    notes.write_text(
        "\n".join([
            "Mount this workspace in client_d.cfg [SharedFile]:",
            f"searchPath0={ws}",
            "",
            "Restart client. Spawn or template using paths under appearance/ from abyssin_m.sat.",
            "See tools/swg_blender/CLIENT_SPAWN_CHECKLIST.md",
        ]),
        encoding="utf-8",
    )
    print("wrote", notes)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())