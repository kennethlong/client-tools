"""Wrap TreeFileExtractor / TreeFileBuilder and provide a unified CLI (Phase 14)."""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

_DEFAULT_EXE_DIRS = [
    Path("exe/win32"),
    Path("src/engine/shared/application/TreeFileExtractor/build/win32/Release"),
    Path("src/engine/shared/application/TreeFileBuilder/build/win32/Release"),
]

SWG_ROOT = Path(__file__).resolve().parents[3]


class TreToolError(RuntimeError):
    pass


def find_tool(name: str, exe_dirs: list[Path] | None = None) -> Path | None:
    candidates = exe_dirs or _DEFAULT_EXE_DIRS
    for d in candidates:
        p = (SWG_ROOT / d / f"{name}.exe") if not d.is_absolute() else d / f"{name}.exe"
        if p.is_file():
            return p
    found = shutil.which(name)
    return Path(found) if found else None


def extract_tre(
    tre_path: str | Path,
    out_dir: str | Path,
    tool: Path | None = None,
) -> subprocess.CompletedProcess:
    exe = tool or find_tool("TreeFileExtractor")
    if not exe:
        raise TreToolError("TreeFileExtractor.exe not found.")
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    return subprocess.run([str(exe), "-e", str(tre_path), str(out_dir)], capture_output=True, text=True)


def list_tre_exe(tre_path: str | Path, tool: Path | None = None) -> subprocess.CompletedProcess:
    exe = tool or find_tool("TreeFileExtractor")
    if not exe:
        raise TreToolError("TreeFileExtractor.exe not found.")
    return subprocess.run([str(exe), "-l", str(tre_path)], capture_output=True, text=True)


def pack_tre(
    rsp_path: str | Path,
    out_tre: str | Path,
    tool: Path | None = None,
    no_toc_compress: bool = False,
) -> subprocess.CompletedProcess:
    exe = tool or find_tool("TreeFileBuilder")
    if not exe:
        raise TreToolError("TreeFileBuilder.exe not found.")
    cmd = [str(exe), "-r", str(rsp_path)]
    if no_toc_compress:
        cmd.append("--no-toc-compression")
    cmd.append(str(out_tre))
    return subprocess.run(cmd, capture_output=True, text=True)


def _cmd_pack(args: argparse.Namespace) -> int:
    from swg_pipeline.pack_pipeline import pack_rsp_file

    try:
        result = pack_rsp_file(
            args.rsp,
            args.output,
            no_toc_compress=args.no_toc_compress,
            verify=not args.no_verify,
        )
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    payload = {
        "rsp": str(result.rsp_path),
        "tre": str(result.tre_path),
        "files": result.file_count,
        "version": result.version,
    }
    if args.json:
        print(json.dumps(payload, indent=2))
    else:
        print(result.tre_path)
    return 0


def _cmd_extract(args: argparse.Namespace) -> int:
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


def _cmd_list(args: argparse.Namespace) -> int:
    if args.python_reader:
        from swg_pipeline.tre_reader import list_tre

        try:
            for path in list_tre(args.tre):
                print(path)
        except (OSError, ValueError) as exc:
            print(f"ERROR: {exc}", file=sys.stderr)
            return 1
        return 0
    try:
        proc = list_tre_exe(args.tre)
    except TreToolError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    if proc.returncode != 0:
        print((proc.stderr or proc.stdout or "list failed").strip(), file=sys.stderr)
        return proc.returncode
    if proc.stdout and not args.silent:
        sys.stdout.write(proc.stdout)
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="SWG TRE pack/extract/list tools")
    parser.add_argument("-silent", action="store_true", help="suppress non-error stdout")
    sub = parser.add_subparsers(dest="command", required=True)

    pack_p = sub.add_parser("pack", help="pack one .rsp into a .tre (TreeFileBuilder)")
    pack_p.add_argument("--rsp", type=Path, required=True)
    pack_p.add_argument("--output", type=Path, required=True, help="output .tre path")
    pack_p.add_argument("--no-toc-compression", action="store_true")
    pack_p.add_argument("--no-verify", action="store_true", help="skip post-pack tre_reader check")
    pack_p.add_argument("--json", action="store_true")

    ext_p = sub.add_parser("extract", help="extract .tre to loose files (TreeFileExtractor)")
    ext_p.add_argument("--tre", type=Path, required=True)
    ext_p.add_argument("--out", type=Path, required=True)

    list_p = sub.add_parser("list", help="list paths inside a .tre")
    list_p.add_argument("--tre", type=Path, required=True)
    list_p.add_argument(
        "--python-reader",
        action="store_true",
        help="use swg_pipeline.tre_reader (retail zlib; no extractor required)",
    )

    args = parser.parse_args(argv)
    if args.command == "pack":
        return _cmd_pack(args)
    if args.command == "extract":
        return _cmd_extract(args)
    if args.command == "list":
        return _cmd_list(args)
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
