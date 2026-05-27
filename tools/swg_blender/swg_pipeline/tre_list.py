"""CLI: python -m swg_pipeline.tre_list [-l tre] [--toc] [--master PATH]"""

from __future__ import annotations

import argparse
import sys

from swg_pipeline.sample_tre import master_toc, sample_tre_dir, smallest_tre, tre_files
from swg_pipeline.tre_reader import (
    MasterIndexKind,
    UnsupportedTreVersionError,
    detect_master_index_kind,
    parse_master_index,
    parse_toc2000,
    read_tre_entries,
    read_tre_header,
    toc_entry_stride,
)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="List SOE retail or SwgRestoration TRE / master TOC contents"
    )
    parser.add_argument("-l", "--list-tre", metavar="PATH", help="List files inside one .tre")
    parser.add_argument("--toc", action="store_true", help="Summarize sample COT2000 master .toc")
    parser.add_argument(
        "--master",
        metavar="PATH",
        help="Summarize master index (auto-detect COT2000 vs SearchTOC)",
    )
    parser.add_argument("--sample-dir", action="store_true", help="Print sample TRE directory")
    parser.add_argument("--head", type=int, default=30, help="Max lines to print (default 30)")
    args = parser.parse_args(argv)

    if args.sample_dir:
        d = sample_tre_dir()
        print(d or "(not found — set SWG_SAMPLE_TRE_DIR)")
        return 0

    if args.master:
        try:
            kind = detect_master_index_kind(args.master)
            info = parse_master_index(args.master)
        except (OSError, ValueError) as exc:
            print(f"ERROR: {exc}", file=sys.stderr)
            return 1
        print(f"Master index: {info['path']}")
        print(f"  kind={kind.value}")
        print(f"  TRE archives: {len(info['tre_files'])}")
        for name in info["tre_files"][: args.head]:
            print(f"    {name}")
        if len(info["tre_files"]) > args.head:
            print(f"    ... ({len(info['tre_files']) - args.head} more archives)")
        print(f"  Indexed paths: {info['indexed_path_count']}")
        print("  Sample paths:")
        for p in info["indexed_paths_sample"]:
            print(f"    {p}")
        return 0

    if args.toc:
        toc = master_toc()
        if not toc:
            print("No master .toc in sample dir", file=sys.stderr)
            return 1
        info = parse_toc2000(toc)
        print(f"TOC: {info['path']}")
        print(f"  kind={info.get('kind', MasterIndexKind.COT2000.value)}")
        print(f"  TRE archives: {len(info['tre_files'])}")
        for name in info["tre_files"]:
            print(f"    {name}")
        print(f"  Indexed paths: {info['indexed_path_count']}")
        print("  Sample paths:")
        for p in info["indexed_paths_sample"]:
            print(f"    {p}")
        return 0

    tre_path = args.list_tre
    if not tre_path:
        small = smallest_tre()
        if not small:
            print("No .tre in sample dir", file=sys.stderr)
            return 1
        tre_path = str(small)

    try:
        header = read_tre_header(tre_path)
        entries = read_tre_entries(tre_path)
    except UnsupportedTreVersionError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    except (OSError, ValueError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    stride = toc_entry_stride(header.version)
    print(f"TRE: {header.path.name}")
    print(
        f"  version={header.version!r} files={header.number_of_files} "
        f"toc_stride={stride} toc_offset={header.toc_offset}"
    )
    print("  entries:")
    for entry in entries[: args.head]:
        comp = f" zlib:{entry.compressed_length}" if entry.compressor else ""
        print(f"    {entry.path}  size={entry.length} off={entry.offset}{comp}")
    if len(entries) > args.head:
        print(f"    ... ({len(entries) - args.head} more)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
