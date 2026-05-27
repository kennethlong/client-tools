"""Batch export CLI (MayaExporter-style flags) — Phase 14."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from swg_pipeline.export_bundle import (
    export_phase7_validation_bundle,
    export_static_bundle_from_mesh_file,
    export_skeletal_bundle_from_files,
)
from swg_pipeline.export_manifest import PIPELINE_VERSION
from swg_pipeline.pack_pipeline import PackError, pack_bundle


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Batch SWG asset export (static / skeletal / pack)"
    )
    parser.add_argument("-silent", action="store_true", help="only print errors")
    parser.add_argument("-outputdir", "--output-dir", type=Path, dest="output_dir")
    parser.add_argument("-author", default="", help="author string for export manifest")
    parser.add_argument("-node", help="source asset path (mesh .msh or skeletal .mgn)")
    parser.add_argument(
        "--notes",
        default="",
        help="freeform export notes stored in manifest",
    )
    sub = parser.add_subparsers(dest="command")

    static_p = sub.add_parser("static", help="export static mesh bundle")
    static_p.add_argument("--mesh", type=Path)
    static_p.add_argument("--shader-reference", type=Path)

    skel_p = sub.add_parser("skeletal", help="export skeletal bundle")
    skel_p.add_argument("--mgn", type=Path)
    skel_p.add_argument("--skt", type=Path)
    skel_p.add_argument("--animation", type=Path, action="append", default=[])

    sub.add_parser("phase7", help="export Phase 7 validation bundle")
    pack_p = sub.add_parser("pack", help="pack bundle rsp -> tre")
    pack_p.add_argument("--bundle-dir", type=Path)
    pack_p.add_argument("--rebuild-rsp", action="store_true")

    args = parser.parse_args(argv)
    output_dir = args.output_dir
    author = args.author or ""
    silent = args.silent

    try:
        if args.command == "pack":
            bundle = args.bundle_dir or output_dir
            if bundle is None:
                parser.error("pack requires --bundle-dir or -outputdir")
            result = pack_bundle(bundle, rebuild_rsp=args.rebuild_rsp)
            summary = {
                "pipeline_version": PIPELINE_VERSION,
                "tre_files": [str(p) for p in result.tre_paths],
            }
            if not silent:
                print(json.dumps(summary, indent=2))
            return 0

        if args.command == "phase7":
            if output_dir is None:
                parser.error("phase7 requires -outputdir")
            result = export_phase7_validation_bundle(output_dir)
            if not silent:
                print(json.dumps(result.as_dict(), indent=2))
            return 0

        if args.command == "skeletal":
            mgn = args.mgn or (Path(args.node) if args.node else None)
            skt = args.skt
            if mgn is None or skt is None or output_dir is None:
                parser.error("skeletal requires --mgn, --skt, and -outputdir")
            result = export_skeletal_bundle_from_files(
                mgn,
                skt,
                output_dir,
                animation_sources=args.animation or None,
            )
            if author and result.manifest_path:
                data = json.loads(result.manifest_path.read_text(encoding="utf-8"))
                data["author"] = author
                data["notes"] = args.notes
                result.manifest_path.write_text(json.dumps(data, indent=2), encoding="utf-8")
            if not silent:
                print(json.dumps(result.as_dict(), indent=2))
            return 0

        mesh = args.mesh or (Path(args.node) if args.node else None)
        if args.command == "static" or mesh is not None:
            if mesh is None or output_dir is None:
                parser.error("static requires --mesh or -node and -outputdir")
            result = export_static_bundle_from_mesh_file(mesh, output_dir)
            if author and result.manifest_path:
                data = json.loads(result.manifest_path.read_text(encoding="utf-8"))
                data["author"] = author
                data["notes"] = args.notes
                result.manifest_path.write_text(json.dumps(data, indent=2), encoding="utf-8")
            if not silent:
                print(json.dumps(result.as_dict(), indent=2))
            return 0

        parser.print_help()
        return 2
    except (OSError, ValueError, FileNotFoundError, PackError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
