"""TRE creature project: materialize workspace and Blender import (M16)."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
from glob import glob
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from swg_pipeline.creature_graph import (
    CreatureGraph,
    creature_parts,
    normalize_tre_relpath,
    resolve_creature_graph,
)
from swg_pipeline.export_manifest import PIPELINE_VERSION, utc_now_iso
from swg_pipeline.swg_main_paths import serverdata_file
from swg_pipeline.texture_bundle import copy_textures_from_shader_file
from swg_pipeline.tre_reader import TreFormatError, list_tre, read_tre_payload

PROJECT_MANIFEST = "swg_tre_project.json"


class TreProjectError(RuntimeError):
    pass


@dataclass
class MaterializedAsset:
    relpath: str
    source: str
    path: Path | None = None


@dataclass
class ImportCreatureResult:
    workspace: Path
    manifest_path: Path
    graph: CreatureGraph
    materialized: list[MaterializedAsset] = field(default_factory=list)
    missing: list[str] = field(default_factory=list)
    ok: bool = True

    def as_dict(self) -> dict[str, Any]:
        return {
            "ok": self.ok,
            "workspace": str(self.workspace),
            "manifest": str(self.manifest_path),
            "root_sat": self.graph.root_sat,
            "missing": list(self.missing),
            "materialized_count": sum(1 for m in self.materialized if m.path),
        }


def _tre_index(tre_path: Path | None) -> set[str]:
    if tre_path is None or not tre_path.is_file():
        return set()
    try:
        return {p.replace("\\", "/").lower() for p in list_tre(tre_path)}
    except Exception:
        return set()


def materialize_asset(
    relpath: str,
    workspace: Path,
    *,
    tre_path: Path | None = None,
    tre_index: set[str] | None = None,
) -> MaterializedAsset:
    norm = normalize_tre_relpath(relpath)
    dest = workspace / norm
    if dest.is_file():
        return MaterializedAsset(norm, "workspace", dest)

    index = tre_index if tre_index is not None else _tre_index(tre_path)
    if tre_path is not None and norm.lower() in index:
        try:
            payload = read_tre_payload(tre_path, norm)
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_bytes(payload)
            return MaterializedAsset(norm, "tre", dest)
        except (KeyError, TreFormatError, OSError):
            pass

    src = serverdata_file(norm)
    if src is not None and src.is_file():
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dest)
        return MaterializedAsset(norm, "serverdata", dest)

    return MaterializedAsset(norm, "missing", None)


def write_project_manifest(
    workspace: Path,
    *,
    graph: CreatureGraph,
    source_tre: str | None,
    materialized: list[MaterializedAsset],
    missing: list[str],
    parts: list[dict[str, Any]],
) -> Path:
    manifest_path = workspace / PROJECT_MANIFEST
    sources = {m.relpath: m.source for m in materialized if m.path is not None}
    payload = {
        "pipeline_version": PIPELINE_VERSION,
        "created_at": utc_now_iso(),
        "project_type": "creature",
        "source_tre": source_tre or "",
        "root_sat": graph.root_sat,
        "graph": graph.to_dict(),
        "parts": parts,
        "materialized": sources,
        "missing": missing,
    }
    manifest_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    return manifest_path


def load_project_manifest(workspace: Path) -> dict[str, Any]:
    path = workspace / PROJECT_MANIFEST
    if not path.is_file():
        raise TreProjectError(f"missing {PROJECT_MANIFEST} in {workspace}")
    return json.loads(path.read_text(encoding="utf-8"))


def import_creature_project(
    workspace: str | Path,
    sat_relpath: str,
    *,
    tre_path: str | Path | None = None,
    copy_textures: bool = False,
) -> ImportCreatureResult:
    """Materialize all creature graph assets under ``workspace``."""
    workspace = Path(workspace).resolve()
    workspace.mkdir(parents=True, exist_ok=True)
    tre = Path(tre_path).resolve() if tre_path else None
    if tre is not None and not tre.is_file():
        raise TreProjectError(f"TRE not found: {tre}")

    sat_norm = normalize_tre_relpath(sat_relpath)
    tre_index = _tre_index(tre)

    sat_result = materialize_asset(
        sat_norm, workspace, tre_path=tre, tre_index=tre_index
    )
    if sat_result.path is None:
        raise TreProjectError(
            f"cannot resolve SAT {sat_norm!r} from TRE or serverdata"
        )

    graph = resolve_creature_graph(sat_result.path, workspace=workspace)
    materialized: list[MaterializedAsset] = [sat_result]
    missing: list[str] = []

    for rel in graph.all_asset_relpaths():
        if rel == sat_norm:
            continue
        result = materialize_asset(rel, workspace, tre_path=tre, tre_index=tre_index)
        materialized.append(result)
        if result.path is None:
            missing.append(rel)

    if copy_textures:
        for rel in graph.shader_paths:
            shader_path = workspace / rel
            if shader_path.is_file():
                copy_textures_from_shader_file(shader_path, workspace)

    parts = creature_parts(graph, workspace)
    manifest_path = write_project_manifest(
        workspace,
        graph=graph,
        source_tre=str(tre) if tre else None,
        materialized=materialized,
        missing=missing,
        parts=parts,
    )

    return ImportCreatureResult(
        workspace=workspace,
        manifest_path=manifest_path,
        graph=graph,
        materialized=materialized,
        missing=missing,
        ok=not missing,
    )


@dataclass
class ExportCreatureResult:
    workspace: Path
    manifest_path: Path
    exported_mgn: list[str] = field(default_factory=list)
    skeleton_relpath: str | None = None
    ok: bool = True

    def as_dict(self) -> dict[str, Any]:
        return {
            "ok": self.ok,
            "workspace": str(self.workspace),
            "exported_mgn": list(self.exported_mgn),
            "skeleton_relpath": self.skeleton_relpath,
        }


def rebuild_creature_appearance_files(
    workspace: Path, graph: CreatureGraph
) -> None:
    """Rewrite SAT/LMG from workspace (paths unchanged)."""
    from swg_scene.mesh_lod import load_mesh_lod, write_mesh_lod_file
    from swg_scene.skeletal_appearance import (
        load_skeletal_appearance,
        write_skeletal_appearance_file,
    )

    sat_path = workspace / graph.root_sat
    if sat_path.is_file():
        sat = load_skeletal_appearance(sat_path)
        write_skeletal_appearance_file(sat_path, sat)

    for lmg_rel in graph.mesh_generator_paths:
        lmg_path = workspace / normalize_tre_relpath(lmg_rel)
        if lmg_path.is_file():
            write_mesh_lod_file(lmg_path, load_mesh_lod(lmg_path))


def _touch_manifest_export(workspace: Path, *, blender_exports: dict[str, Any]) -> Path:
    manifest = load_project_manifest(workspace)
    manifest["exported_at"] = utc_now_iso()
    manifest["last_blender_export"] = blender_exports
    out = workspace / PROJECT_MANIFEST
    out.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    return out


def _export_creature_via_blender(
    workspace: Path, *, ignore_blend_targets: bool = False
) -> dict[str, Any]:
    """Export from the active Blender session, or spawn headless Blender."""
    try:
        import bpy  # noqa: F401
    except ImportError:
        proc = run_blender_export(
            workspace, ignore_blend_targets=ignore_blend_targets
        )
        if proc.returncode != 0:
            err = (proc.stderr or proc.stdout or "").strip()
            raise TreProjectError(f"Blender export failed: {err}")
        if not proc.stdout.strip():
            return {"ok": False, "exported_mgn": []}
        return json.loads(proc.stdout.strip().splitlines()[-1])

    from swg_pipeline.tre_project_bpy import export_creature_from_workspace

    return export_creature_from_workspace(
        workspace, ignore_blend_targets=ignore_blend_targets
    )


def export_creature_project(
    workspace: str | Path,
    *,
    use_blender: bool = False,
    ignore_blend_targets: bool = False,
    rebuild_rsp: bool = False,
) -> ExportCreatureResult:
    """Export Blender parts (optional) and refresh SAT/LMG on disk."""
    workspace = Path(workspace).resolve()
    manifest = load_project_manifest(workspace)
    graph = resolve_creature_graph(
        workspace / manifest["root_sat"], workspace=workspace
    )

    blender_exports: dict[str, Any] = {"skipped": True}
    exported_mgn: list[str] = []
    skeleton_relpath: str | None = None

    if use_blender:
        proc = run_blender_creature_roundtrip(
            workspace, ignore_blend_targets=ignore_blend_targets
        )
        if proc.returncode != 0:
            err = (proc.stderr or proc.stdout or "").strip()
            raise TreProjectError(f"Blender round-trip failed: {err}")
        line = proc.stdout.strip().splitlines()[-1] if proc.stdout.strip() else "{}"
        blender_exports = json.loads(line)
        if not blender_exports.get("ok"):
            raise TreProjectError(blender_exports.get("error", "export failed"))
        exported_mgn = list(blender_exports.get("exported_mgn") or [])
        skeleton_relpath = blender_exports.get("skeleton_relpath")

    rebuild_creature_appearance_files(workspace, graph)
    manifest_path = _touch_manifest_export(
        workspace, blender_exports=blender_exports
    )

    if rebuild_rsp:
        from swg_pipeline.pack_pipeline import pack_bundle

        pack_bundle(workspace, rebuild_rsp=True, update_manifest=True)

    return ExportCreatureResult(
        workspace=workspace,
        manifest_path=manifest_path,
        exported_mgn=exported_mgn,
        skeleton_relpath=skeleton_relpath,
        ok=True,
    )


def _resolve_blender_exe() -> str:
    env = os.environ.get("BLENDER_EXE", "").strip()
    if env and Path(env).is_file():
        return env
    found = shutil.which("blender") or shutil.which("blender.exe")
    if found:
        return found
    matches = sorted(
        glob(r"C:\Program Files\Blender Foundation\Blender *\blender.exe"),
        reverse=True,
    )
    if matches:
        return matches[0]
    raise TreProjectError(
        "Blender not found: set BLENDER_EXE or install Blender on PATH"
    )




def run_blender_creature_roundtrip(
    workspace: Path, *, ignore_blend_targets: bool = False
) -> subprocess.CompletedProcess:
    script = Path(__file__).resolve().parent / "tre_project_bpy.py"
    cmd = [
        _resolve_blender_exe(),
        "-b",
        "--python",
        str(script),
        "--",
        "--workspace",
        str(workspace.resolve()),
        "--roundtrip",
    ]
    if ignore_blend_targets:
        cmd.append("--ignore-blend-targets")
    return subprocess.run(cmd, capture_output=True, text=True)


def run_blender_import(workspace: str | Path) -> subprocess.CompletedProcess:
    script = Path(__file__).resolve().parent / "tre_project_bpy.py"
    workspace = Path(workspace).resolve()
    cmd = [
        _resolve_blender_exe(),
        "-b",
        "--python",
        str(script),
        "--",
        "--workspace",
        str(workspace),
    ]
    return subprocess.run(cmd, capture_output=True, text=True)


def run_blender_export(
    workspace: str | Path, *, ignore_blend_targets: bool = False
) -> subprocess.CompletedProcess:
    script = Path(__file__).resolve().parent / "tre_project_bpy.py"
    workspace = Path(workspace).resolve()
    cmd = [
        _resolve_blender_exe(),
        "-b",
        "--python",
        str(script),
        "--",
        "--workspace",
        str(workspace),
        "--export",
    ]
    if ignore_blend_targets:
        cmd.append("--ignore-blend-targets")
    return subprocess.run(cmd, capture_output=True, text=True)


def _cmd_import_creature(args: argparse.Namespace) -> int:
    try:
        result = import_creature_project(
            args.workspace,
            args.sat,
            tre_path=args.tre,
            copy_textures=args.copy_textures,
        )
    except TreProjectError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    if args.json:
        print(json.dumps(result.as_dict(), indent=2))
    else:
        print(result.manifest_path)
        if result.missing:
            print(f"WARNING: missing {len(result.missing)} assets:", file=sys.stderr)
            for rel in result.missing[:20]:
                print(f"  {rel}", file=sys.stderr)

    if args.blender:
        proc = run_blender_import(result.workspace)
        if proc.returncode != 0:
            err = (proc.stderr or proc.stdout or "").strip()
            print(f"Blender import failed: {err}", file=sys.stderr)
            return proc.returncode or 1
        if proc.stdout.strip():
            print(proc.stdout.strip())

    return 0 if result.ok or not args.strict else 1


def _cmd_export_creature(args: argparse.Namespace) -> int:
    try:
        result = export_creature_project(
            args.workspace,
            use_blender=args.blender,
            ignore_blend_targets=args.ignore_blend_targets,
            rebuild_rsp=args.rebuild_rsp,
        )
    except (TreProjectError, json.JSONDecodeError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    if args.json:
        print(json.dumps(result.as_dict(), indent=2))
    else:
        print(result.manifest_path)
        if result.exported_mgn:
            print(f"exported {len(result.exported_mgn)} mesh(es)")

    return 0




def _cmd_import_building(args: argparse.Namespace) -> int:
    from swg_pipeline.tre_project_building import import_building_project

    try:
        result = import_building_project(
            args.workspace,
            args.pob,
            tre_path=args.tre,
            copy_textures=args.copy_textures,
        )
    except TreProjectError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    if args.json:
        print(json.dumps(result.as_dict(), indent=2))
    else:
        print(result.manifest_path)
        if result.missing:
            print(f"WARNING: missing {len(result.missing)} assets", file=sys.stderr)
            for rel in result.missing[:20]:
                print(f"  {rel}", file=sys.stderr)

    if args.blender:
        from swg_pipeline.tre_project_building import run_blender_import_building

        proc = run_blender_import_building(result.workspace)
        if proc.returncode != 0:
            err = (proc.stderr or proc.stdout or "").strip()
            print(f"Blender import failed: {err}", file=sys.stderr)
            return proc.returncode
        if proc.stdout.strip():
            print(proc.stdout.strip())

    return 1 if (args.strict and result.missing) else 0


def _cmd_export_building(args: argparse.Namespace) -> int:
    from swg_pipeline.tre_project_building import export_building_project

    try:
        result = export_building_project(
            args.workspace,
            use_blender=args.blender,
            rebuild_rsp=args.rebuild_rsp,
        )
    except TreProjectError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    if args.json:
        print(json.dumps(result.as_dict(), indent=2))
    else:
        print(result.manifest_path)
        if result.exported_msh:
            print(f"exported {len(result.exported_msh)} .msh files")
    return 0


def _cmd_pack(args: argparse.Namespace) -> int:
    from swg_pipeline.pack_pipeline import pack_bundle

    try:
        pack_bundle(
            args.workspace,
            tre_dir=args.tre_dir,
            rebuild_rsp=args.rebuild_rsp,
        )
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    print(args.workspace)
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="SWG TRE project: creature (SAT) and building (POB) workspaces"
    )
    sub = parser.add_subparsers(dest="command", required=True)

    imp = sub.add_parser(
        "import-creature",
        help="extract creature DAG into a serverdata workspace",
    )
    imp.add_argument("--workspace", type=Path, required=True)
    imp.add_argument(
        "--sat",
        required=True,
        help="TreeFile path e.g. appearance/abyssin_m.sat",
    )
    imp.add_argument("--tre", type=Path, help="source .tre archive")
    imp.add_argument(
        "--copy-textures",
        action="store_true",
        help="copy DDS from shaders after materialize",
    )
    imp.add_argument(
        "--blender",
        action="store_true",
        help="run headless Blender import after materialize",
    )
    imp.add_argument(
        "--strict",
        action="store_true",
        help="exit 1 if any graph asset is missing",
    )
    imp.add_argument("--json", action="store_true")
    imp.set_defaults(func=_cmd_import_creature)

    exp = sub.add_parser(
        "export-creature",
        help="export Blender creature parts to workspace TreeFile paths",
    )
    exp.add_argument("--workspace", type=Path, required=True)
    exp.add_argument(
        "--blender",
        action="store_true",
        help="run headless Blender export from SWG_Creature_* collection",
    )
    exp.add_argument(
        "--ignore-blend-targets",
        action="store_true",
        help="skip blend shape export on .mgn",
    )
    exp.add_argument(
        "--rebuild-rsp",
        action="store_true",
        help="rebuild rsp and refresh manifest after export",
    )
    exp.add_argument("--json", action="store_true")
    exp.set_defaults(func=_cmd_export_creature)

    impb = sub.add_parser(
        "import-building",
        help="extract building POB graph into a serverdata workspace",
    )
    impb.add_argument("--workspace", type=Path, required=True)
    impb.add_argument(
        "--pob",
        required=True,
        help="TreeFile path e.g. appearance/echo_base_pob.pob",
    )
    impb.add_argument("--tre", type=Path)
    impb.add_argument("--copy-textures", action="store_true")
    impb.add_argument("--blender", action="store_true")
    impb.add_argument("--strict", action="store_true")
    impb.add_argument("--json", action="store_true")
    impb.set_defaults(func=_cmd_import_building)

    expb = sub.add_parser(
        "export-building",
        help="export Blender building meshes to workspace TreeFile paths",
    )
    expb.add_argument("--workspace", type=Path, required=True)
    expb.add_argument("--blender", action="store_true")
    expb.add_argument("--rebuild-rsp", action="store_true")
    expb.add_argument("--json", action="store_true")
    expb.set_defaults(func=_cmd_export_building)

    pack = sub.add_parser("pack", help="rsp + TreeFileBuilder for workspace")
    pack.add_argument("--workspace", type=Path, required=True)
    pack.add_argument("--tre-dir", type=Path)
    pack.add_argument("--rebuild-rsp", action="store_true")
    pack.set_defaults(func=_cmd_pack)

    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
