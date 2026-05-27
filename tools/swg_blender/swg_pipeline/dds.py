"""PNG / RGBA ↔ DDS helpers for SWG texture paths (Phase 8.7).

SWG clients accept standard DirectDraw Surface files. When ``texconv`` is not
on PATH we write uncompressed ``A8R8G8B8`` DDS (always valid). Install
DirectXTex ``texconv.exe`` for DXT1/DXT5 output matching retail assets.
"""

from __future__ import annotations

import shutil
import struct
import subprocess
import sys
from dataclasses import dataclass
from enum import Enum
from pathlib import Path

try:
    from PIL import Image
except ImportError:  # pragma: no cover - optional at runtime
    Image = None  # type: ignore[misc, assignment]

DDS_MAGIC = 0x20534444  # "DDS "

DDSD_CAPS = 0x00000001
DDSD_HEIGHT = 0x00000002
DDSD_WIDTH = 0x00000004
DDSD_PITCH = 0x00000008
DDSD_PIXELFORMAT = 0x00001000
DDSD_LINEARSIZE = 0x00080000

DDPF_ALPHAPIXELS = 0x00000001
DDPF_FOURCC = 0x00000004
DDPF_RGB = 0x00000040

DDSCAPS_TEXTURE = 0x00001000


class DdsFormat(str, Enum):
    A8R8G8B8 = "a8r8g8b8"
    DXT1 = "dxt1"
    DXT5 = "dxt5"


@dataclass(frozen=True)
class DdsInfo:
    width: int
    height: int
    four_cc: str | None
    rgb_bit_count: int
    is_compressed: bool
    payload_offset: int


def _four_cc(code: bytes) -> int:
    if len(code) != 4:
        raise ValueError("four_cc must be 4 bytes")
    return struct.unpack("<I", code)[0]


def _make_pixel_format(
    *,
    flags: int,
    four_cc: int = 0,
    rgb_bit_count: int = 0,
    r_mask: int = 0,
    g_mask: int = 0,
    b_mask: int = 0,
    a_mask: int = 0,
) -> bytes:
    return struct.pack(
        "<8I",
        32,
        flags,
        four_cc,
        rgb_bit_count,
        r_mask,
        g_mask,
        b_mask,
        a_mask,
    )


def read_dds_info(path: str | Path) -> DdsInfo:
    data = Path(path).read_bytes()
    if len(data) < 128 or struct.unpack_from("<I", data, 0)[0] != DDS_MAGIC:
        raise ValueError(f"not a DDS file: {path}")

    height, width = struct.unpack_from("<II", data, 12)
    pf_flags, four_cc, rgb_bit_count = struct.unpack_from("<III", data, 76)
    is_compressed = bool(pf_flags & DDPF_FOURCC)
    code = struct.pack("<I", four_cc).decode("ascii", errors="replace").rstrip("\x00")
    return DdsInfo(
        width=width,
        height=height,
        four_cc=code if is_compressed else None,
        rgb_bit_count=rgb_bit_count,
        is_compressed=is_compressed,
        payload_offset=128,
    )


def _write_dds_header(
    *,
    width: int,
    height: int,
    pixel_format: bytes,
    flags: int,
    pitch_or_linear: int,
) -> bytes:
    header = bytearray(124)
    struct.pack_into("<I", header, 0, 124)
    struct.pack_into("<I", header, 4, flags)
    struct.pack_into("<I", header, 8, height)
    struct.pack_into("<I", header, 12, width)
    struct.pack_into("<I", header, 16, pitch_or_linear)
    struct.pack_into("<I", header, 20, 0)  # depth
    struct.pack_into("<I", header, 24, 0)  # mip count
    header[76:108] = pixel_format
    struct.pack_into("<I", header, 108, DDSCAPS_TEXTURE)
    return bytes(header)


def write_dds_a8r8g8b8(rgba: bytes, width: int, height: int) -> bytes:
    if len(rgba) != width * height * 4:
        raise ValueError("rgba byte length does not match width*height*4")

    # SWG Dds.h DDSPF_A8R8G8B8 masks (BGRA in memory → named A8R8G8B8).
    pf = _make_pixel_format(
        flags=DDPF_RGB | DDPF_ALPHAPIXELS,
        rgb_bit_count=32,
        r_mask=0x00FF0000,
        g_mask=0x0000FF00,
        b_mask=0x000000FF,
        a_mask=0xFF000000,
    )
    pitch = width * 4
    header = _write_dds_header(
        width=width,
        height=height,
        pixel_format=pf,
        flags=DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_PITCH,
        pitch_or_linear=pitch,
    )
    # Pillow RGBA → BGRA for this mask layout.
    pixels = bytearray(len(rgba))
    for i in range(0, len(rgba), 4):
        r, g, b, a = rgba[i : i + 4]
        pixels[i] = b
        pixels[i + 1] = g
        pixels[i + 2] = r
        pixels[i + 3] = a
    return struct.pack("<I", DDS_MAGIC) + header + bytes(pixels)


def _require_pillow() -> None:
    if Image is None:
        raise RuntimeError("Pillow is required for PNG→DDS conversion (pip install Pillow)")


def load_rgba_image(path: str | Path) -> tuple[bytes, int, int]:
    _require_pillow()
    img = Image.open(path).convert("RGBA")
    width, height = img.size
    return img.tobytes(), width, height


def png_to_dds_bytes(
    png_path: str | Path,
    *,
    fmt: DdsFormat = DdsFormat.DXT5,
    allow_texconv: bool = True,
) -> bytes:
    png_path = Path(png_path)
    if allow_texconv and fmt in (DdsFormat.DXT1, DdsFormat.DXT5):
        texconv = find_texconv()
        if texconv:
            return _convert_via_texconv(texconv, png_path, fmt)
    rgba, width, height = load_rgba_image(png_path)
    if fmt != DdsFormat.A8R8G8B8:
        # Caller asked for DXT but texconv is unavailable — uncompressed fallback.
        pass
    return write_dds_a8r8g8b8(rgba, width, height)


def png_to_dds_file(
    png_path: str | Path,
    dds_path: str | Path,
    *,
    fmt: DdsFormat = DdsFormat.DXT5,
    allow_texconv: bool = True,
) -> DdsInfo:
    out = Path(dds_path)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(png_to_dds_bytes(png_path, fmt=fmt, allow_texconv=allow_texconv))
    return read_dds_info(out)


def find_texconv() -> Path | None:
    found = shutil.which("texconv")
    if found:
        return Path(found)
    for candidate in (
        Path(r"C:\Program Files (x86)\DirectXTex\texconv.exe"),
        Path(r"C:\Program Files\DirectXTex\texconv.exe"),
    ):
        if candidate.is_file():
            return candidate
    return None


def _convert_via_texconv(texconv: Path, png_path: Path, fmt: DdsFormat) -> bytes:
    flag = "BC1_UNORM" if fmt == DdsFormat.DXT1 else "BC3_UNORM"
    import tempfile

    with tempfile.TemporaryDirectory(prefix="swg_dds_") as tmp:
        tmp_path = Path(tmp)
        cmd = [
            str(texconv),
            "-y",
            "-f",
            flag,
            "-o",
            str(tmp_path),
            str(png_path),
        ]
        proc = subprocess.run(cmd, capture_output=True, text=True)
        if proc.returncode != 0:
            raise RuntimeError(
                f"texconv failed ({proc.returncode}): {proc.stderr or proc.stdout}"
            )
        produced = tmp_path / f"{png_path.stem}.dds"
        if not produced.is_file():
            matches = list(tmp_path.glob("*.dds"))
            if len(matches) != 1:
                raise RuntimeError(f"texconv did not produce a .dds in {tmp_path}")
            produced = matches[0]
        return produced.read_bytes()


def install_texture_for_bundle(
    treefile_relpath: str,
    output_dir: Path,
    *,
    png_source: str | Path | None = None,
    fmt: DdsFormat = DdsFormat.DXT5,
) -> Path | None:
    """Copy or convert a texture into ``output_dir`` at ``treefile_relpath``.

    Resolution order:
    1. Explicit ``png_source`` → convert to ``treefile_relpath`` (.dds)
    2. Existing file under ``SWG_MAIN/serverdata`` at ``treefile_relpath``
    3. ``treefile_relpath`` with ``.png`` extension on serverdata → convert
    """
    from swg_pipeline.swg_main_paths import serverdata_file

    rel = treefile_relpath.replace("\\", "/").lstrip("/")
    dest = output_dir / rel
    dest.parent.mkdir(parents=True, exist_ok=True)

    if png_source is not None:
        png_to_dds_file(png_source, dest, fmt=fmt)
        return dest

    copied = serverdata_file(rel)
    if copied is not None:
        if not dest.exists():
            shutil.copyfile(copied, dest)
        return dest

    if rel.lower().endswith(".dds"):
        png_rel = rel[:-4] + ".png"
        png_file = serverdata_file(png_rel)
        if png_file is not None:
            png_to_dds_file(png_file, dest, fmt=fmt)
            return dest
    return None


def main(argv: list[str] | None = None) -> int:
    import argparse

    parser = argparse.ArgumentParser(description="Convert PNG to SWG-compatible DDS")
    parser.add_argument("input", type=Path, help="source .png (or .dds to inspect)")
    parser.add_argument("output", type=Path, nargs="?", help="destination .dds")
    parser.add_argument(
        "--format",
        choices=[f.value for f in DdsFormat],
        default=DdsFormat.DXT5.value,
        help="target format (DXT requires texconv on PATH)",
    )
    parser.add_argument("--info", action="store_true", help="print DDS header and exit")
    args = parser.parse_args(argv)

    if args.info or (args.input.suffix.lower() == ".dds" and args.output is None):
        info = read_dds_info(args.input)
        print(
            f"{args.input}: {info.width}x{info.height} "
            f"four_cc={info.four_cc!r} compressed={info.is_compressed}"
        )
        return 0

    if args.output is None:
        parser.error("output path required unless --info on a .dds")

    fmt = DdsFormat(args.format)
    png_to_dds_file(args.input, args.output, fmt=fmt)
    info = read_dds_info(args.output)
    print(f"Wrote {args.output} ({info.width}x{info.height}, four_cc={info.four_cc!r})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
