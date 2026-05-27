"""Static shader template (`.sht` / SSHT) copy and patch helpers."""

from __future__ import annotations

import argparse
import shutil
import sys
from dataclasses import dataclass, field
from pathlib import Path

from swg_iff.reader import IffError, IffReader
from swg_iff.tags import TAG_0001, TAG_DATA, TAG_NAME, TAG_SSHT, tag_from_str
from swg_iff.writer import (
    make_chunk,
    make_chunk_int32,
    make_chunk_string,
    make_form,
    make_chunk_bool8,
)
from swg_pipeline.validate import validate_shader_template


@dataclass
class ShaderTextureSlot:
    tag: str
    path: str
    wrap_u: int = 0
    wrap_v: int = 0
    wrap_w: int = 1
    filter_mip: int = 1
    filter_min: int = 1
    filter_mag: int = 1


@dataclass
class ShaderStub:
    mats_raw: bytes = b""
    textures: list[ShaderTextureSlot] = field(default_factory=list)
    tcss_raw: bytes = b""
    tfns_raw: bytes = b""
    arvs_raw: bytes = b""
    effect_path: str = "effect\\a_specmap.eft"

    @classmethod
    def from_file(cls, path: str | Path) -> ShaderStub:
        data = Path(path).read_bytes()
        return cls.from_bytes(data)

    @classmethod
    def from_bytes(cls, data: bytes) -> ShaderStub:
        reader = IffReader(data)
        reader.enter_form(TAG_SSHT)
        try:
            reader.enter_form(tag_from_str("0000"))
            try:
                stub = cls()
                while not reader.at_end_of_form():
                    block = reader._peek_block()
                    if block.is_form and block.form_type_str.strip() == "MATS":
                        stub.mats_raw = reader.read_raw_block()
                    elif block.is_form and block.form_type_str.strip() == "TXMS":
                        stub.textures = _read_textures(reader)
                    elif block.is_form and block.form_type_str.strip() == "TCSS":
                        stub.tcss_raw = reader.read_raw_block()
                    elif block.is_form and block.form_type_str.strip() == "TFNS":
                        stub.tfns_raw = reader.read_raw_block()
                    elif block.is_form and block.form_type_str.strip() == "ARVS":
                        stub.arvs_raw = reader.read_raw_block()
                    elif not block.is_form and block.tag_str.strip() == "NAME":
                        reader.enter_chunk(TAG_NAME)
                        stub.effect_path = reader.read_string()
                        reader.exit_chunk()
                    else:
                        reader.skip_block()
                if not stub.textures:
                    raise IffError("SSHT missing TXMS texture block")
                return stub
            finally:
                reader.exit_form(tag_from_str("0000"))
        finally:
            reader.exit_form(TAG_SSHT)

    def apply_overrides(
        self,
        *,
        texture_overrides: dict[str, str] | None = None,
        effect: str | None = None,
    ) -> ShaderStub:
        out = ShaderStub(
            mats_raw=self.mats_raw,
            textures=[ShaderTextureSlot(**slot.__dict__) for slot in self.textures],
            tcss_raw=self.tcss_raw,
            tfns_raw=self.tfns_raw,
            arvs_raw=self.arvs_raw,
            effect_path=self.effect_path,
        )
        if texture_overrides:
            by_tag = {slot.tag: slot for slot in out.textures}
            for tag, path in texture_overrides.items():
                key = tag.upper()
                if key in by_tag:
                    by_tag[key].path = path
                else:
                    raise ValueError(f"reference shader has no texture slot {key!r}")
        if effect is not None:
            out.effect_path = effect
        return out

    def to_bytes(self) -> bytes:
        body = (
            self.mats_raw
            + _write_textures(self.textures)
            + self.tcss_raw
            + self.tfns_raw
            + self.arvs_raw
            + make_chunk("NAME", make_chunk_string(self.effect_path))
        )
        return make_form("SSHT", make_form("0000", body))


def _read_textures(reader: IffReader) -> list[ShaderTextureSlot]:
    textures: list[ShaderTextureSlot] = []
    reader.enter_form(tag_from_str("TXMS"))
    try:
        while not reader.at_end_of_form():
            block = reader._peek_block()
            if not (block.is_form and block.form_type_str.strip() == "TXM"):
                reader.skip_block()
                continue
            reader.enter_form(block.form_type)
            try:
                reader.enter_form(TAG_0001)
                try:
                    reader.enter_chunk(TAG_DATA)
                    tag_int = reader.read_int32()
                    tag = _tag_int_to_str(tag_int)
                    _placeholder = reader.read_uint8()
                    wrap_u = reader.read_uint8()
                    wrap_v = reader.read_uint8()
                    wrap_w = reader.read_uint8()
                    filter_mip = reader.read_uint8()
                    filter_min = reader.read_uint8()
                    filter_mag = reader.read_uint8()
                    reader.exit_chunk(TAG_DATA)

                    reader.enter_chunk(TAG_NAME)
                    path = reader.read_string()
                    reader.exit_chunk(TAG_NAME)

                    textures.append(
                        ShaderTextureSlot(
                            tag=tag,
                            path=path,
                            wrap_u=wrap_u,
                            wrap_v=wrap_v,
                            wrap_w=wrap_w,
                            filter_mip=filter_mip,
                            filter_min=filter_min,
                            filter_mag=filter_mag,
                        )
                    )
                finally:
                    reader.exit_form(TAG_0001)
            finally:
                reader.exit_form(block.form_type)
        return textures
    finally:
        reader.exit_form(tag_from_str("TXMS"))


def _write_textures(textures: list[ShaderTextureSlot]) -> bytes:
    slots = b"".join(_write_texture_slot(slot) for slot in textures)
    return make_form("TXMS", slots)


def _write_texture_slot(slot: ShaderTextureSlot) -> bytes:
    data = (
        make_chunk_int32(_tag_str_to_int(slot.tag))
        + make_chunk_bool8(False)
        + bytes(
            [
                slot.wrap_u & 0xFF,
                slot.wrap_v & 0xFF,
                slot.wrap_w & 0xFF,
                slot.filter_mip & 0xFF,
                slot.filter_min & 0xFF,
                slot.filter_mag & 0xFF,
            ]
        )
    )
    inner = make_chunk("DATA", data) + make_chunk("NAME", make_chunk_string(slot.path))
    return make_form("TXM ", make_form("0001", inner))


def _tag_int_to_str(value: int) -> str:
    raw = value.to_bytes(4, byteorder="big", signed=False)
    return raw.decode("ascii", errors="replace").rstrip("\x00")


def _tag_str_to_int(tag: str) -> int:
    padded = (tag.upper() + "\x00\x00\x00\x00")[:4]
    return int.from_bytes(padded.encode("ascii"), byteorder="big", signed=False)


def copy_shader_stub(
    reference: str | Path,
    output: str | Path,
    *,
    texture_overrides: dict[str, str] | None = None,
    effect: str | None = None,
    validate: bool = True,
) -> Path:
    """
    Copy a retail/static `.sht` and optionally patch texture paths or effect name.

    ``texture_overrides`` keys are slot tags like ``MAIN`` or ``SPEC``.
    """
    ref = Path(reference)
    out = Path(output)
    if not ref.is_file():
        raise FileNotFoundError(f"reference shader not found: {ref}")

    if not texture_overrides and effect is None:
        out.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(ref, out)
    else:
        stub = ShaderStub.from_file(ref).apply_overrides(
            texture_overrides=texture_overrides,
            effect=effect,
        )
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_bytes(stub.to_bytes())

    if validate:
        result = validate_shader_template(out)
        if not result.ok:
            raise ValueError("; ".join(result.errors))
    return out


def copy_shader_stub_for_mesh(
    mesh_shader_relpath: str,
    *,
    reference: str | Path,
    output_dir: str | Path,
    texture_overrides: dict[str, str] | None = None,
    effect: str | None = None,
) -> Path:
    """Write ``shader/foo.sht`` under ``output_dir`` mirroring serverdata layout."""
    rel = mesh_shader_relpath.replace("\\", "/").lstrip("/")
    out = Path(output_dir) / rel
    return copy_shader_stub(
        reference,
        out,
        texture_overrides=texture_overrides,
        effect=effect,
    )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Copy/patch SWG static shader template (.sht)"
    )
    parser.add_argument("--reference", type=Path, required=True, help="source .sht")
    parser.add_argument("--output", type=Path, required=True, help="destination .sht")
    parser.add_argument(
        "--main-texture",
        help="override MAIN texture path (e.g. texture/my_prop.dds)",
    )
    parser.add_argument(
        "--spec-texture",
        help="override SPEC texture path",
    )
    parser.add_argument("--effect", help="override effect path")
    args = parser.parse_args(argv)

    overrides: dict[str, str] = {}
    if args.main_texture:
        overrides["MAIN"] = args.main_texture
    if args.spec_texture:
        overrides["SPEC"] = args.spec_texture

    try:
        copy_shader_stub(
            args.reference,
            args.output,
            texture_overrides=overrides or None,
            effect=args.effect,
        )
    except (OSError, ValueError, IffError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    print(f"{args.output}: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())