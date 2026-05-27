"""Bundle -> rsp -> .tre production pipeline (Phase 14)."""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass, field
from pathlib import Path

from swg_pipeline.export_manifest import append_tre_to_manifest, utc_now_iso
from swg_pipeline.rsp_builder import build_rsp_from_roots
from swg_pipeline.tre_reader import list_tre, read_tre_header
from swg_pipeline.tre_tools import find_tool, pack_tre


@dataclass
class PackResult:
    rsp_path: Path
    tre_path: Path
    file_count: int = 0
    version: str = ""
    ok: bool = True
    message: str = ""


@dataclass
class BundlePackResult:
    bundle_dir: Path
    rsp_paths: list[Path] = field(default_factory=list)
    tre_paths: list[Path] = field(default_factory=list)
    packs: list[PackResult] = field(default_factory=list)


class PackError(RuntimeError):
    pass


def _tre_name_for_rsp(rsp_path: Path) -> str:
    stem = rsp_path.stem
    if stem.endswith(".rsp"):
        stem = stem[:-4]
    return f"{stem}.tre"


def pack_rsp_file(
    rsp_path: str | Path,
    out_tre: str | Path,
    *,
    no_toc_compress: bool = False,
    verify: bool = True,
) -> PackResult:
    rsp = Path(rsp_path)
    out = Path(out_tre)
    if not rsp.is_file():
        raise PackError(f"rsp not found: {rsp}")
    if find_tool("TreeFileBuilder") is None:
        raise PackError("TreeFileBuilder.exe not found (build swg.sln or add to PATH)")

    out.parent.mkdir(parents=True, exist_ok=True)
    proc = pack_tre(rsp, out, no_toc_compress=no_toc_compress)
    if proc.returncode != 0:
        err = (proc.stderr or proc.stdout or "").strip()
        raise PackError(f"TreeFileBuilder failed ({proc.returncode}): {err}")

    file_count = 0
    version = ""
    if verify and out.is_file():
        header = read_tre_header(out)
        version = header.version
        file_count = header.number_of_files

    return PackResult(
        rsp_path=rsp,
        tre_path=out,
        file_count=file_count,
        version=version,
        ok=True,
        message="packed",
    )


def pack_bundle(
    bundle_dir: str | Path,
    *,
    tre_dir: str | Path | None = None,
    rebuild_rsp: bool = False,
    rsp_names: list[str] | None = None,
    no_toc_compress: bool = False,
    update_manifest: bool = True,
) -> BundlePackResult:
    root = Path(bundle_dir).resolve()
    if not root.is_dir():
        raise PackError(f"bundle directory not found: {root}")

    rsp_dir = root / "rsp"
    if rebuild_rsp or not rsp_dir.is_dir() or not any(rsp_dir.glob("*.rsp")):
        rsp_paths = build_rsp_from_roots([root], rsp_dir)
    else:
        rsp_paths = sorted(rsp_dir.glob("*.rsp"))

    if rsp_names:
        wanted = {n if n.endswith(".rsp") else f"{n}.rsp" for n in rsp_names}
        rsp_paths = [p for p in rsp_paths if p.name in wanted]
        if not rsp_paths:
            raise PackError(f"no matching rsp files in {rsp_dir}: {sorted(wanted)}")

    out_tre_dir = Path(tre_dir) if tre_dir else root / "tre"
    out_tre_dir.mkdir(parents=True, exist_ok=True)

    packs: list[PackResult] = []
    tre_paths: list[Path] = []
    for rsp in rsp_paths:
        tre_out = out_tre_dir / _tre_name_for_rsp(rsp)
        result = pack_rsp_file(rsp, tre_out, no_toc_compress=no_toc_compress)
        packs.append(result)
        tre_paths.append(tre_out)

    if update_manifest:
        manifest = root / "swg_export_manifest.json"
        if not manifest.is_file():
            manifest = root / "swg_phase7_manifest.json"
        append_tre_to_manifest(manifest, tre_paths)

    return BundlePackResult(
        bundle_dir=root,
        rsp_paths=list(rsp_paths),
        tre_paths=tre_paths,
        packs=packs,
    )


def verify_tre_listing(tre_path: Path) -> list[str]:
    return list_tre(tre_path)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Pack export bundles: rsp manifests -> .tre via TreeFileBuilder"
    )
    parser.add_argument("bundle_dir", type=Path, help="bundle root (serverdata layout)")
    parser.add_argument(
        "--tre-dir",
        type=Path,
        help="output directory for .tre files (default: <bundle>/tre)",
    )
    parser.add_argument(
        "--rebuild-rsp",
        action="store_true",
        help="regenerate rsp/*.rsp from bundle tree before packing",
    )
    parser.add_argument(
        "--rsp",
        action="append",
        metavar="NAME",
        help="pack only these rsp basenames (e.g. data_compressed_other)",
    )
    parser.add_argument("--no-toc-compression", action="store_true")
    parser.add_argument("--json", action="store_true")
    parser.add_argument(
        "--list",
        metavar="TRE",
        type=Path,
        help="list contents of a .tre (python reader, no extractor required)",
    )
    args = parser.parse_args(argv)

    if args.list:
        try:
            paths = verify_tre_listing(args.list)
        except (OSError, ValueError) as exc:
            print(f"ERROR: {exc}", file=sys.stderr)
            return 1
        for p in paths:
            print(p)
        return 0

    try:
        result = pack_bundle(
            args.bundle_dir,
            tre_dir=args.tre_dir,
            rebuild_rsp=args.rebuild_rsp,
            rsp_names=args.rsp,
            no_toc_compress=args.no_toc_compression,
        )
    except PackError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    summary = {
        "bundle_dir": str(result.bundle_dir),
        "packed_at": utc_now_iso(),
        "rsp_files": [str(p) for p in result.rsp_paths],
        "tre_files": [str(p) for p in result.tre_paths],
        "packs": [
            {
                "rsp": str(p.rsp_path),
                "tre": str(p.tre_path),
                "files": p.file_count,
                "version": p.version,
            }
            for p in result.packs
        ],
    }
    if args.json:
        print(json.dumps(summary, indent=2))
    else:
        for pack in result.packs:
            print(f"{pack.tre_path} ({pack.file_count} files, {pack.version})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
