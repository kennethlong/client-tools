"""CLI: init or verify Sample-TRE-Files/playground/ mirror."""

from __future__ import annotations

import argparse
import sys

from swg_pipeline.sample_tre import (
    ensure_playground_copy,
    playground_dir,
    playground_is_current,
    playground_manifest,
    sample_tre_dir,
)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Manage playground/ copy of Sample-TRE-Files for safe TRE experiments"
    )
    parser.add_argument(
        "command",
        choices=("init", "status", "path"),
        help="init=copy archives, status=check manifest, path=print playground dir",
    )
    parser.add_argument("--force", action="store_true", help="Re-copy even if playground looks current")
    args = parser.parse_args(argv)

    root = sample_tre_dir()
    if not root:
        print("Sample TRE dir not found (set SWG_SAMPLE_TRE_DIR)", file=sys.stderr)
        return 1

    if args.command == "path":
        pg = playground_dir()
        print(pg or root / "playground")
        return 0

    if args.command == "status":
        pg = playground_dir()
        print(f"reference: {root}")
        print(f"playground: {pg}")
        if not pg or not pg.is_dir():
            print("status: missing — run: python -m swg_pipeline.tre_playground init")
            return 2
        manifest = playground_manifest(pg)
        current = playground_is_current()
        print(f"status: {'current' if current else 'stale or incomplete'}")
        if manifest:
            print(f"  created: {manifest.get('created_at')}")
            print(f"  files: {len(manifest.get('files', []))}")
        return 0 if current else 2

    pg = ensure_playground_copy(force=args.force)
    print(f"playground ready: {pg}")
    print(f"  files: {len(list(pg.glob('*.tre')))} .tre + .toc")
    print("Write/pack tests should target playground/ only; keep parent dir as reference.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())