"""Dump IFF structure to text (ViewIff-lite)."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from swg_iff.reader import IffReader, root_form_type, validate_iff
from swg_iff.tags import KNOWN_ROOT_FORMS, TAG_FORM, tag_to_str


def _describe_root(form_type: int) -> str:
    name = KNOWN_ROOT_FORMS.get(form_type)
    return f"  ({name})" if name else ""


def dump_tree(data: bytes, path: str = "<memory>") -> str:
    lines: list[str] = []
    root = root_form_type(data)
    if root is not None:
        lines.append(f"ROOT FORM {tag_to_str(root)!r}{_describe_root(root)}")
    elif not validate_iff(data):
        lines.append("WARNING: structural validation failed")

    def walk(reader: IffReader, indent: int) -> None:
        prefix = "  " * indent
        while not reader.at_end_of_form():
            block = reader._peek_block()
            if block.is_form:
                ft = tag_to_str(block.form_type or 0)
                lines.append(f"{prefix}FORM {ft!r}  len={block.length}")
                reader.enter_form()
                walk(reader, indent + 1)
                reader.exit_form()
            else:
                lines.append(f"{prefix}CHUNK {block.tag_str!r}  len={block.length}")
                reader.enter_chunk()
                reader.exit_chunk()

    reader = IffReader(data, path)
    walk(reader, 0)
    return "\n".join(lines)


def dump_file(path: Path) -> str:
    data = path.read_bytes()
    header = [f"File: {path}", f"Size: {len(data)} bytes", f"Valid: {validate_iff(data)}"]
    return "\n".join(header) + "\n\n" + dump_tree(data, str(path))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Dump SWG IFF chunk tree")
    parser.add_argument("files", nargs="+", help="IFF file paths")
    args = parser.parse_args(argv)
    for i, f in enumerate(args.files):
        if i:
            print()
        print(dump_file(Path(f)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
