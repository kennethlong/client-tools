"""Extended SWG shader templates: CSHD, SWTS, OPST (Phase 10.2-10.4)."""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path

from swg_iff.reader import IffError, IffReader, root_form_type
from swg_iff.tags import (
    TAG_0000,
    TAG_0001,
    TAG_CSHD,
    TAG_CUST,
    TAG_DATA,
    TAG_DRTS,
    TAG_DRFT,
    TAG_DPPT,
    TAG_INFO,
    TAG_NAME,
    TAG_OPST,
    TAG_PAL,
    TAG_SSHT,
    TAG_SWTS,
    TAG_TFAC,
    TAG_TEXT,
    TAG_TX1D,
    TAG_TXTR,
    tag_from_str,
    tag_to_str,
)
from swg_iff.writer import (
    make_chunk,
    make_chunk_float,
    make_chunk_int16,
    make_chunk_int32,
    make_chunk_int8,
    make_chunk_string,
    make_chunk_uint32,
    make_form,
)
from swg_pipeline.shader_builder import ShaderBuildSpec, build_shader_bytes
from swg_pipeline.shader_import import load_shader_build_spec, spec_from_shader_stub
from swg_pipeline.shader_stub import ShaderStub, _tag_int_to_str, _tag_str_to_int


@dataclass
class PaletteCustomization:
    variable_name: str
    is_private: bool
    texture_tag: str
    palette_path: str
    default_index: int


@dataclass
class SwappableTextureCustomization:
    texture_tag: str
    variable_name: str
    is_private: bool
    default_index: int
    texture_paths: list[str] = field(default_factory=list)


@dataclass
class CshdSpec:
    base: ShaderBuildSpec
    palettes: list[PaletteCustomization] = field(default_factory=list)
    swappable: list[SwappableTextureCustomization] = field(default_factory=list)
    version: str = "0001"


@dataclass
class SwtsSpec:
    base_shader_path: str
    order: str
    time_min: float
    time_max: float
    frames: list[tuple[str, str]] = field(default_factory=list)


@dataclass
class OpstSpec:
    base_shader_path: str


def shader_root_type(data: bytes) -> str | None:
    root = root_form_type(data)
    return tag_to_str(root) if root is not None else None


def load_cshd_spec(path: str | Path) -> CshdSpec:
    return load_cshd_bytes(Path(path).read_bytes())


def load_cshd_bytes(data: bytes) -> CshdSpec:
    reader = IffReader(data)
    reader.enter_form(TAG_CSHD)
    try:
        version = reader.current_name()
        version_str = tag_to_str(version)
        if version_str not in ("0000", "0001"):
            raise IffError(f"unsupported CSHD version {version_str!r}")
        reader.enter_form(version)
        try:
            base = _load_embedded_ssht(reader)
            palettes: list[PaletteCustomization] = []
            swappable: list[SwappableTextureCustomization] = []
            while not reader.at_end_of_form():
                block = reader._peek_block()
                if block.is_form and block.form_type_str.strip() == "TFAC":
                    palettes.extend(_read_tfac(reader))
                elif block.is_form and block.form_type_str.strip() == "TXTR":
                    swappable.extend(_read_txtr(reader))
                else:
                    reader.skip_block()
            return CshdSpec(
                base=base,
                palettes=palettes,
                swappable=swappable,
                version=version_str,
            )
        finally:
            reader.exit_form(version)
    finally:
        reader.exit_form(TAG_CSHD)


def _load_embedded_ssht(reader: IffReader) -> ShaderBuildSpec:
    block = reader._peek_block()
    if not (block.is_form and block.form_type_str.strip() == "SSHT"):
        raise IffError("CSHD missing embedded SSHT form")
    raw = reader.read_raw_block()
    return spec_from_shader_stub(ShaderStub.from_bytes(raw))


def _read_tfac(reader: IffReader) -> list[PaletteCustomization]:
    palettes: list[PaletteCustomization] = []
    reader.enter_form(TAG_TFAC)
    try:
        while not reader.at_end_of_form():
            block = reader._peek_block()
            if block.is_form or block.tag_str.strip() != "PAL":
                reader.skip_block()
                continue
            reader.enter_chunk(TAG_PAL)
            try:
                variable_name = reader.read_string()
                is_private = reader.read_int8() != 0
                texture_tag = _tag_int_to_str(reader.read_uint32())
                palette_path = reader.read_string()
                default_index = reader.read_int32()
                palettes.append(
                    PaletteCustomization(
                        variable_name=variable_name,
                        is_private=is_private,
                        texture_tag=texture_tag,
                        palette_path=palette_path,
                        default_index=default_index,
                    )
                )
            finally:
                reader.exit_chunk(TAG_PAL)
        return palettes
    finally:
        reader.exit_form(TAG_TFAC)


def _read_txtr(reader: IffReader) -> list[SwappableTextureCustomization]:
    result: list[SwappableTextureCustomization] = []
    reader.enter_form(TAG_TXTR)
    try:
        texture_paths: list[str] = []
        if not reader.at_end_of_form():
            block = reader._peek_block()
            if block.is_form or block.tag_str.strip() != "DATA":
                raise IffError("TXTR missing DATA chunk")
            reader.enter_chunk(TAG_DATA)
            try:
                count = reader.read_int16()
                for _ in range(count):
                    texture_paths.append(reader.read_string())
            finally:
                reader.exit_chunk(TAG_DATA)

        if reader.at_end_of_form():
            return result

        block = reader._peek_block()
        if not (block.is_form and block.form_type_str.strip() == "CUST"):
            return result

        reader.enter_form(TAG_CUST)
        try:
            index = 0
            while not reader.at_end_of_form():
                block = reader._peek_block()
                if block.is_form or block.tag_str.strip() != "TX1D":
                    reader.skip_block()
                    continue
                reader.enter_chunk(TAG_TX1D)
                try:
                    texture_tag = _tag_int_to_str(reader.read_uint32())
                    first_index = reader.read_int16()
                    path_count = reader.read_int16()
                    variable_name = reader.read_string()
                    is_private = reader.read_int8() != 0
                    default_index = reader.read_int16()
                    paths = texture_paths[first_index : first_index + path_count]
                    result.append(
                        SwappableTextureCustomization(
                            texture_tag=texture_tag,
                            variable_name=variable_name,
                            is_private=is_private,
                            default_index=default_index,
                            texture_paths=list(paths),
                        )
                    )
                    index += path_count
                finally:
                    reader.exit_chunk(TAG_TX1D)
            return result
        finally:
            reader.exit_form(TAG_CUST)
    finally:
        reader.exit_form(TAG_TXTR)


def build_cshd_bytes(spec: CshdSpec) -> bytes:
    body = build_shader_bytes(spec.base)
    if spec.swappable:
        body += _write_txtr(spec.swappable)
    if spec.palettes:
        body += _write_tfac(spec.palettes)
    version = spec.version if spec.version in ("0000", "0001") else "0001"
    return make_form("CSHD", make_form(version, body))


def _write_tfac(palettes: list[PaletteCustomization]) -> bytes:
    chunks = b""
    for entry in palettes:
        payload = (
            make_chunk_string(entry.variable_name)
            + make_chunk_int8(1 if entry.is_private else 0)
            + make_chunk_uint32(_tag_str_to_int(entry.texture_tag))
            + make_chunk_string(entry.palette_path)
            + make_chunk_int32(entry.default_index)
        )
        chunks += make_chunk("PAL ", payload)
    return make_form("TFAC", chunks)


def _write_txtr(swappable: list[SwappableTextureCustomization]) -> bytes:
    texture_paths: list[str] = []
    cust_chunks = b""
    first_index = 0
    for entry in swappable:
        cust_chunks += make_chunk(
            "TX1D",
            make_chunk_uint32(_tag_str_to_int(entry.texture_tag))
            + make_chunk_int16(first_index)
            + make_chunk_int16(len(entry.texture_paths))
            + make_chunk_string(entry.variable_name)
            + make_chunk_int8(1 if entry.is_private else 0)
            + make_chunk_int16(entry.default_index),
        )
        texture_paths.extend(entry.texture_paths)
        first_index += len(entry.texture_paths)

    data_payload = make_chunk_int16(len(texture_paths))
    for path in texture_paths:
        data_payload += make_chunk_string(path)
    data_chunk = make_chunk("DATA", data_payload)
    return make_form("TXTR", data_chunk + make_form("CUST", cust_chunks))


def load_swts_spec(path: str | Path) -> SwtsSpec:
    return load_swts_bytes(Path(path).read_bytes())


def load_swts_bytes(data: bytes) -> SwtsSpec:
    reader = IffReader(data)
    reader.enter_form(TAG_SWTS)
    try:
        version = reader.current_name()
        if tag_to_str(version) != "0000":
            raise IffError("unsupported SWTS version")
        reader.enter_form(TAG_0000)
        try:
            base_shader_path = ""
            order = "DRTS"
            time_min = 0.0
            time_max = 0.0
            frames: list[tuple[str, str]] = []
            while not reader.at_end_of_form():
                block = reader._peek_block()
                if not block.is_form and block.tag_str.strip() == "NAME":
                    reader.enter_chunk(TAG_NAME)
                    try:
                        base_shader_path = reader.read_string()
                    finally:
                        reader.exit_chunk(TAG_NAME)
                elif block.is_form and block.form_type_str.strip() in (
                    "DRTS",
                    "DPPT",
                    "DRFT",
                ):
                    order = block.form_type_str.strip()
                    reader.enter_form(tag_from_str(order))
                    try:
                        reader.enter_chunk(TAG_0000)
                        try:
                            _frame_count = reader.read_int32()
                            time_min = reader.read_float()
                            time_max = reader.read_float()
                        finally:
                            reader.exit_chunk(TAG_0000)
                    finally:
                        reader.exit_form(tag_from_str(order))
                elif not block.is_form and block.tag_str.strip() == "TEXT":
                    reader.enter_chunk(TAG_TEXT)
                    try:
                        tag = _tag_int_to_str(reader.read_int32())
                        path = reader.read_string()
                        frames.append((tag, path))
                    finally:
                        reader.exit_chunk(TAG_TEXT)
                else:
                    reader.skip_block()
            return SwtsSpec(
                base_shader_path=base_shader_path,
                order=order,
                time_min=time_min,
                time_max=time_max,
                frames=frames,
            )
        finally:
            reader.exit_form(TAG_0000)
    finally:
        reader.exit_form(TAG_SWTS)


def build_swts_bytes(spec: SwtsSpec) -> bytes:
    order = spec.order.upper()
    if order not in ("DRTS", "DPPT", "DRFT"):
        order = "DRTS"
    switcher = make_form(
        order,
        make_chunk(
            "0000",
            make_chunk_int32(len(spec.frames))
            + make_chunk_float(spec.time_min)
            + make_chunk_float(spec.time_max),
        ),
    )
    text_chunks = b"".join(
        make_chunk(
            "TEXT",
            make_chunk_int32(_tag_str_to_int(tag))
            + make_chunk_string(path),
        )
        for tag, path in spec.frames
    )
    body = (
        make_chunk("NAME", make_chunk_string(spec.base_shader_path))
        + switcher
        + text_chunks
    )
    return make_form("SWTS", make_form("0000", body))


def load_opst_spec(path: str | Path) -> OpstSpec:
    return load_opst_bytes(Path(path).read_bytes())


def load_opst_bytes(data: bytes) -> OpstSpec:
    reader = IffReader(data)
    reader.enter_form(TAG_OPST)
    try:
        version = reader.current_name()
        if tag_to_str(version) != "0000":
            raise IffError("unsupported OPST version")
        reader.enter_form(TAG_0000)
        try:
            reader.enter_chunk(TAG_INFO)
            try:
                base_shader_path = reader.read_string()
            finally:
                reader.exit_chunk(TAG_INFO)
            return OpstSpec(base_shader_path=base_shader_path)
        finally:
            reader.exit_form(TAG_0000)
    finally:
        reader.exit_form(TAG_OPST)


def build_opst_bytes(spec: OpstSpec) -> bytes:
    body = make_chunk("INFO", make_chunk_string(spec.base_shader_path))
    return make_form("OPST", make_form("0000", body))


def load_shader_spec(path: str | Path):
    data = Path(path).read_bytes()
    root = shader_root_type(data)
    if root == "SSHT":
        return load_shader_build_spec(path)
    if root == "CSHD":
        return load_cshd_spec(path)
    if root == "SWTS":
        return load_swts_spec(path)
    if root == "OPST":
        return load_opst_spec(path)
    raise IffError(f"unsupported shader root form {root!r}")


def build_shader_template_bytes(spec) -> bytes:
    if isinstance(spec, ShaderBuildSpec):
        return build_shader_bytes(spec)
    if isinstance(spec, CshdSpec):
        return build_cshd_bytes(spec)
    if isinstance(spec, SwtsSpec):
        return build_swts_bytes(spec)
    if isinstance(spec, OpstSpec):
        return build_opst_bytes(spec)
    raise TypeError(f"unsupported shader spec type: {type(spec)!r}")


def write_shader_template(
    spec,
    output: str | Path,
    *,
    validate: bool = True,
) -> Path:
    from swg_pipeline.validate import validate_shader_file

    out = Path(output)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(build_shader_template_bytes(spec))
    if validate:
        result = validate_shader_file(out)
        if not result.ok:
            raise ValueError("; ".join(result.errors))
    return out
