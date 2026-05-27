"""Generate TreeFileRspBuilder-style .rsp manifests from loose serverdata trees."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

# Bucket order mirrors TreeFileRspBuilder.cpp (suffix match, then catch-all).
_BUCKET_RULES: list[tuple[str, str]] = [
    (".mp3", "music"),
    (".wav", "sample"),
    (".dds", "texture"),
    (".ans", "animation"),
    (".mgn", "mesh_skeletal"),
    (".msh", "mesh_static"),
]

RSP_FILENAMES: dict[str, str] = {
    "music": "data_uncompressed_music.rsp",
    "sample": "data_uncompressed_sample.rsp",
    "texture": "data_compressed_texture.rsp",
    "animation": "data_compressed_animation.rsp",
    "mesh_static": "data_compressed_mesh_static.rsp",
    "mesh_skeletal": "data_compressed_mesh_skeletal.rsp",
    "other": "data_compressed_other.rsp",
}


def classify_path(relpath: str) -> str:
    lower = relpath.replace("\\", "/").lower()
    for suffix, bucket in _BUCKET_RULES:
        if lower.endswith(suffix):
            return bucket
    return "other"


def iter_tree_files(root: Path) -> list[tuple[str, Path]]:
    root = root.resolve()
    return [
        (p.relative_to(root).as_posix(), p.resolve())
        for p in sorted(root.rglob("*"))
        if p.is_file()
    ]


def build_rsp_maps(search_roots: list[Path]) -> dict[str, dict[str, str]]:
    """Map bucket name -> {treefile_path: explicit_path}. First root wins per path."""
    maps: dict[str, dict[str, str]] = {name: {} for name in RSP_FILENAMES}
    for root in search_roots:
        if not root.is_dir():
            continue
        for rel, abspath in iter_tree_files(root):
            bucket = classify_path(rel)
            bucket_map = maps[bucket]
            if rel not in bucket_map:
                bucket_map[rel] = str(abspath)
    return maps


def format_rsp_line(treefile_path: str, explicit_path: str | Path) -> str:
    explicit = str(explicit_path).replace("\\", "/")
    return f"{treefile_path} @ {explicit}\n"


def write_rsp_file(path: Path, entries: dict[str, str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = [format_rsp_line(k, v) for k, v in sorted(entries.items())]
    path.write_text("".join(lines), encoding="utf-8")


def write_rsp_maps(maps: dict[str, dict[str, str]], output_dir: Path) -> list[Path]:
    output_dir = Path(output_dir)
    written: list[Path] = []
    for bucket, filename in RSP_FILENAMES.items():
        entries = maps.get(bucket) or {}
        if not entries:
            continue
        out = output_dir / filename
        write_rsp_file(out, entries)
        written.append(out)
    return written


def build_rsp_from_roots(search_roots: list[Path], output_dir: Path) -> list[Path]:
    maps = build_rsp_maps(search_roots)
    return write_rsp_maps(maps, output_dir)


def client_search_path_snippet(
    search_paths: list[Path],
    *,
    priority: int = 0,
    sku: int | None = None,
) -> list[str]:
    """Build SharedFile lines for TreeFile loose-path overrides.

    SWGSource / multi-SKU cfgs use ``searchPath_00_9=...`` (higher priority
    than ``searchTree_00_8``). Legacy cfgs use ``searchPath0=...``.
    """
    lines: list[str] = []
    for path in search_paths:
        resolved = Path(path).resolve()
        if sku is not None:
            lines.append(f"searchPath_{sku:02d}_{priority}={resolved}")
        else:
            lines.append(f"searchPath{priority}={resolved}")
    return lines


def write_client_cfg_fragment(
    search_paths: list[Path],
    output: Path,
    *,
    priority: int = 0,
    sku: int | None = None,
) -> Path:
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    body = "\n".join(
        client_search_path_snippet(search_paths, priority=priority, sku=sku)
    )
    output.write_text(body + "\n", encoding="utf-8")
    return output


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Build TreeFileRspBuilder-style .rsp files from loose directories"
    )
    parser.add_argument(
        "roots",
        nargs="+",
        type=Path,
        help="search roots (serverdata-style trees); earlier roots win on duplicates",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        required=True,
        help="directory for data_*.rsp files",
    )
    parser.add_argument(
        "--client-cfg",
        type=Path,
        help="optional path to write searchPathN= lines for client_d.cfg",
    )
    parser.add_argument(
        "--search-path-index",
        type=int,
        default=0,
        help="starting index for searchPathN keys (default 0)",
    )
    parser.add_argument("--json", action="store_true", help="print summary JSON to stdout")
    args = parser.parse_args(argv)

    try:
        written = build_rsp_from_roots(args.roots, args.output_dir)
        cfg_path = None
        if args.client_cfg:
            cfg_path = write_client_cfg_fragment(
                args.roots,
                args.client_cfg,
                priority=args.search_path_index,
            )
    except OSError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    summary = {
        "output_dir": str(args.output_dir),
        "rsp_files": [str(p) for p in written],
        "client_cfg": str(cfg_path) if cfg_path else None,
    }
    if args.json:
        print(json.dumps(summary, indent=2))
    else:
        for p in written:
            print(p)
        if cfg_path:
            print(cfg_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())