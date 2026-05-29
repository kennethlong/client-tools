"""Materialize workspace (optional) and run creature side-by-side compare in Blender."""

from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

DEFAULT_SAT = "appearance/abyssin_m.sat"
DEFAULT_IO_SCENE = Path(r"D:\Code\io_scene_swg_msh\io_scene_swg")
BPY_SCRIPT = Path(__file__).resolve().parent / "compare_creature_io_scene_bpy.py"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare swg_blender vs io_scene_swg_msh creature import in Blender"
    )
    parser.add_argument("--workspace", type=Path, help="Materialized creature workspace")
    parser.add_argument("--sat", default=DEFAULT_SAT)
    parser.add_argument("--materialize", action="store_true", help="Extract SAT graph into workspace")
    parser.add_argument("--io-scene-dir", type=Path, default=DEFAULT_IO_SCENE)
    parser.add_argument("--offset", type=float, default=2.5)
    parser.add_argument(
        "--in-blender",
        action="store_true",
        help="Print bpy command for the open Blender session (MCP/manual)",
    )
    parser.add_argument(
        "--factory-reset",
        action="store_true",
        help="Pass --factory-reset to bpy script (default: purge compare collections only)",
    )
    parser.add_argument("--io-skt", action="store_true", help="Import io_scene skeleton")
    parser.add_argument("--swg-only", action="store_true")
    parser.add_argument("--io-only", action="store_true")
    parser.add_argument(
        "--launch-blender",
        action="store_true",
        help="Spawn a separate Blender process (NOT your MCP session). Prefer Scripting/MCP.",
    )
    parser.add_argument(
        "--background",
        action="store_true",
        help="With --launch-blender: use -b (no GUI).",
    )
    parser.add_argument(
        "--io-materials",
        action="store_true",
        help="Pass --io-materials to bpy script (shader/DDS; can crash)",
    )
    parser.add_argument(
        "--install-io-addon",
        action="store_true",
        help="Pass --install-io-addon to bpy script",
    )
    args = parser.parse_args()

    ws = args.workspace
    if args.materialize or ws is None:
        from swg_pipeline.tre_project import import_creature_project

        ws = ws or Path(tempfile.mkdtemp(prefix="swg_creature_compare_"))
        print("workspace:", ws)
        imp = import_creature_project(ws, args.sat)
        print("materialized:", len(imp.materialized), "missing:", len(imp.missing))
        if imp.missing:
            return 1

    if not (ws / "swg_tre_project.json").is_file():
        print("error: workspace has no swg_tre_project.json", file=sys.stderr)
        return 1

    if not args.io_scene_dir.is_dir():
        print("error: io_scene add-on not found:", args.io_scene_dir, file=sys.stderr)
        return 1

    bpy_argv = [
        "--workspace",
        str(ws.resolve()),
        "--sat",
        args.sat,
        "--io-scene-dir",
        str(args.io_scene_dir.resolve()),
        "--offset",
        str(args.offset),
    ]
    if args.factory_reset:
        bpy_argv.append("--factory-reset")
    if args.io_skt:
        bpy_argv.append("--io-skt")
    if args.swg_only:
        bpy_argv.append("--swg-only")
    if args.io_only:
        bpy_argv.append("--io-only")
    if args.io_materials:
        bpy_argv.append("--io-materials")
    if args.install_io_addon:
        bpy_argv.append("--install-io-addon")

    if args.in_blender or not args.launch_blender:
        print("Run inside your open Blender (Scripting or MCP) — does NOT spawn a new window:")
        print("  import sys, types, runpy")
        print("  if 'audioop' not in sys.modules:")
        print("      m = types.ModuleType('audioop'); m.cross = lambda *a, **k: b''")
        print("      sys.modules['audioop'] = m")
        print(f"  script = r'{BPY_SCRIPT}'")
        print("  sys.argv = [script, '--', " + ", ".join(repr(a) for a in bpy_argv) + "]")
        print("  runpy.run_path(script, run_name='__main__')")
        if not args.launch_blender:
            return 0

    from swg_pipeline.tre_project import _resolve_blender_exe

    cmd = [_resolve_blender_exe()]
    if args.background:
        cmd.append("-b")
    cmd.extend(["--python", str(BPY_SCRIPT), "--", *bpy_argv])
    mode = "background" if args.background else "GUI subprocess"
    print(f"running ({mode}):", " ".join(cmd[:5]), "...")
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.stdout:
        print(proc.stdout)
    if proc.stderr:
        print(proc.stderr, file=sys.stderr)
    return proc.returncode


if __name__ == "__main__":
    raise SystemExit(main())