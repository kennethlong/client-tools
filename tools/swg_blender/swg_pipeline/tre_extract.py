"""CLI alias for TreeFileExtractor wrapper (Phase 14)."""

from __future__ import annotations

import argparse
import sys

from swg_pipeline.tre_tools import TreToolError, extract_tre, main as tre_tools_main


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Extract a .tre archive to loose files")
    parser.add_argument("--tre", type=str, required=True)
    parser.add_argument("--out", type=str, required=True)
    parser.add_argument("-silent", action="store_true")
    args, rest = parser.parse_known_args(argv)
    if rest and rest[0] == "extract":
        rest = rest[1:]
    if rest:
        return tre_tools_main(rest)
    try:
        proc = extract_tre(args.tre, args.out)
    except TreToolError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    if proc.returncode != 0:
        print((proc.stderr or proc.stdout or "extract failed").strip(), file=sys.stderr)
        return proc.returncode
    if not args.silent:
        print(args.out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
