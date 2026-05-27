"""Skeletal appearance template (SMAT) load + export."""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path

from swg_iff.reader import IffReader, IffError
from swg_iff.tags import (
    TAG_0001,
    TAG_0002,
    TAG_0003,
    TAG_APAG,
    TAG_INFO,
    TAG_LATX,
    TAG_LDTB,
    TAG_MSGN,
    TAG_SFSK,
    TAG_SKTI,
    TAG_SMAT,
    tag_to_str,
)
from swg_iff.writer import (
    make_chunk,
    make_chunk_int16,
    make_chunk_int32,
    make_chunk_int8,
    make_chunk_string,
    make_chunk_uint8,
    make_form,
)


@dataclass
class SwgSkeletonBinding:
    skeleton_relpath: str
    attachment_transform: str = ""


@dataclass
class SwgSkeletalAppearance:
    source_path: str = ""
    version: str = "0003"
    mesh_generator_paths: list[str] = field(default_factory=list)
    skeleton_bindings: list[SwgSkeletonBinding] = field(default_factory=list)
    create_animation_controller: bool = True
    animation_state_graph_path: str = ""
    skeleton_lat_pairs: list[tuple[str, str]] = field(default_factory=list)
    must_use_soft_skinning: bool | None = None
    always_play_action_generator: bool | None = None
    trailing_blocks: list[bytes] = field(default_factory=list)


def load_skeletal_appearance(path: str | Path) -> SwgSkeletalAppearance:
    p = Path(path)
    reader = IffReader.from_file(p)
    sat = _load_from_reader(reader)
    sat.source_path = str(p)
    return sat


def load_skeletal_appearance_from_reader(reader: IffReader) -> SwgSkeletalAppearance:
    return _load_from_reader(reader)


def _load_from_reader(reader: IffReader) -> SwgSkeletalAppearance:
    root = reader.current_name()
    if root != TAG_SMAT:
        raise IffError(f"expected SMAT root, got {tag_to_str(root)!r}")
    reader.enter_form(TAG_SMAT)
    try:
        version_tag = reader.current_name()
        version = tag_to_str(version_tag)
        if version_tag == TAG_0001:
            sat = _load_0001(reader)
        elif version_tag == TAG_0002:
            sat = _load_0002(reader)
        elif version_tag == TAG_0003:
            sat = _load_0003(reader)
        else:
            raise IffError(f"unsupported SMAT version {version!r}")
        sat.version = version
        return sat
    finally:
        reader.exit_form(TAG_SMAT)


def _load_msgn(reader: IffReader, mesh_count: int) -> list[str]:
    reader.enter_chunk(TAG_MSGN)
    try:
        return [reader.read_string() for _ in range(mesh_count)]
    finally:
        reader.exit_chunk(TAG_MSGN)


def _load_skti(reader: IffReader, skel_count: int) -> list[SwgSkeletonBinding]:
    reader.enter_chunk(TAG_SKTI)
    try:
        bindings: list[SwgSkeletonBinding] = []
        for _ in range(skel_count):
            skeleton = reader.read_string()
            attachment = reader.read_string()
            bindings.append(
                SwgSkeletonBinding(
                    skeleton_relpath=skeleton,
                    attachment_transform=attachment,
                )
            )
        return bindings
    finally:
        reader.exit_chunk(TAG_SKTI)


def _load_0001(reader: IffReader) -> SwgSkeletalAppearance:
    reader.enter_form(TAG_0001)
    try:
        reader.enter_chunk(TAG_INFO)
        try:
            mesh_count = reader.read_int32()
            skel_count = reader.read_int32()
            reader.read_string()
            if mesh_count < 0 or skel_count < 0:
                raise IffError("invalid SMAT 0001 counts")
        finally:
            reader.exit_chunk(TAG_INFO)
        mesh_paths = _load_msgn(reader, mesh_count)
        bindings = _load_skti(reader, skel_count)
        return SwgSkeletalAppearance(
            mesh_generator_paths=mesh_paths,
            skeleton_bindings=bindings,
            create_animation_controller=False,
        )
    finally:
        reader.exit_form(TAG_0001)


def _load_0002(reader: IffReader) -> SwgSkeletalAppearance:
    reader.enter_form(TAG_0002)
    try:
        reader.enter_chunk(TAG_INFO)
        try:
            mesh_count = reader.read_int32()
            skel_count = reader.read_int32()
            asg_path = reader.read_string()
            if mesh_count < 0 or skel_count < 0:
                raise IffError("invalid SMAT 0002 counts")
        finally:
            reader.exit_chunk(TAG_INFO)
        mesh_paths = _load_msgn(reader, mesh_count)
        bindings = _load_skti(reader, skel_count)
        return SwgSkeletalAppearance(
            mesh_generator_paths=mesh_paths,
            skeleton_bindings=bindings,
            create_animation_controller=bool(asg_path),
            animation_state_graph_path=asg_path,
        )
    finally:
        reader.exit_form(TAG_0002)


def _load_0003(reader: IffReader) -> SwgSkeletalAppearance:
    reader.enter_form(TAG_0003)
    try:
        reader.enter_chunk(TAG_INFO)
        try:
            mesh_count = reader.read_int32()
            skel_count = reader.read_int32()
            create_controller = reader.read_int8() != 0
            if mesh_count < 0 or skel_count < 0:
                raise IffError("invalid SMAT 0003 counts")
        finally:
            reader.exit_chunk(TAG_INFO)

        mesh_paths = _load_msgn(reader, mesh_count)
        bindings = _load_skti(reader, skel_count)

        lat_pairs: list[tuple[str, str]] = []
        if _peek_chunk(reader, TAG_LATX):
            reader.enter_chunk(TAG_LATX)
            try:
                count = reader.read_int16()
                if count < 0:
                    raise IffError(f"invalid LATX entry count {count}")
                for _ in range(count):
                    lat_pairs.append((reader.read_string(), reader.read_string()))
            finally:
                reader.exit_chunk(TAG_LATX)

        trailing: list[bytes] = []
        must_use_soft_skinning: bool | None = None
        always_play_action_generator: bool | None = None

        while not reader.at_end_of_form():
            block = reader._peek_block()
            if block.is_form and block.form_type == TAG_LDTB:
                trailing.append(reader.read_raw_block())
                continue
            if not block.is_form and block.tag == TAG_SFSK:
                reader.enter_chunk(TAG_SFSK)
                try:
                    must_use_soft_skinning = reader.read_uint8() != 0
                finally:
                    reader.exit_chunk(TAG_SFSK)
                continue
            if not block.is_form and block.tag == TAG_APAG:
                reader.enter_chunk(TAG_APAG)
                try:
                    always_play_action_generator = reader.read_uint8() != 0
                finally:
                    reader.exit_chunk(TAG_APAG)
                continue
            trailing.append(reader.read_raw_block())

        return SwgSkeletalAppearance(
            mesh_generator_paths=mesh_paths,
            skeleton_bindings=bindings,
            create_animation_controller=create_controller,
            skeleton_lat_pairs=lat_pairs,
            must_use_soft_skinning=must_use_soft_skinning,
            always_play_action_generator=always_play_action_generator,
            trailing_blocks=trailing,
        )
    finally:
        reader.exit_form(TAG_0003)


def _peek_chunk(reader: IffReader, tag: int) -> bool:
    block = reader._peek_block()
    return (not block.is_form) and block.tag == tag


def write_skeletal_appearance(sat: SwgSkeletalAppearance) -> bytes:
    version = sat.version or "0003"
    if version == "0001":
        body = _write_0001(sat)
    elif version == "0002":
        body = _write_0002(sat)
    elif version == "0003":
        body = _write_0003(sat)
    else:
        raise ValueError(f"unsupported SMAT version {version!r}")
    return make_form("SMAT", body)


def write_skeletal_appearance_file(path: str | Path, sat: SwgSkeletalAppearance) -> Path:
    p = Path(path)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_bytes(write_skeletal_appearance(sat))
    return p


def _write_0001(sat: SwgSkeletalAppearance) -> bytes:
    info_payload = (
        make_chunk_int32(len(sat.mesh_generator_paths))
        + make_chunk_int32(len(sat.skeleton_bindings))
        + make_chunk_string("")
    )
    return make_form(
        "0001",
        make_chunk("INFO", info_payload)
        + _write_msgn(sat.mesh_generator_paths)
        + _write_skti(sat.skeleton_bindings),
    )


def _write_0002(sat: SwgSkeletalAppearance) -> bytes:
    info_payload = (
        make_chunk_int32(len(sat.mesh_generator_paths))
        + make_chunk_int32(len(sat.skeleton_bindings))
        + make_chunk_string(sat.animation_state_graph_path)
    )
    return make_form(
        "0002",
        make_chunk("INFO", info_payload)
        + _write_msgn(sat.mesh_generator_paths)
        + _write_skti(sat.skeleton_bindings),
    )


def _write_0003(sat: SwgSkeletalAppearance) -> bytes:
    info_payload = (
        make_chunk_int32(len(sat.mesh_generator_paths))
        + make_chunk_int32(len(sat.skeleton_bindings))
        + make_chunk_int8(1 if sat.create_animation_controller else 0)
    )
    body = (
        make_chunk("INFO", info_payload)
        + _write_msgn(sat.mesh_generator_paths)
        + _write_skti(sat.skeleton_bindings)
    )
    if sat.skeleton_lat_pairs:
        lat_payload = make_chunk_int16(len(sat.skeleton_lat_pairs))
        for key, value in sat.skeleton_lat_pairs:
            lat_payload += make_chunk_string(key) + make_chunk_string(value)
        body += make_chunk("LATX", lat_payload)
    for block in sat.trailing_blocks:
        body += block
    if sat.must_use_soft_skinning is not None:
        body += make_chunk("SFSK", make_chunk_uint8(1 if sat.must_use_soft_skinning else 0))
    if sat.always_play_action_generator is not None:
        body += make_chunk("APAG", make_chunk_uint8(1 if sat.always_play_action_generator else 0))
    return make_form("0003", body)


def _write_msgn(paths: list[str]) -> bytes:
    payload = b"".join(make_chunk_string(p) for p in paths)
    return make_chunk("MSGN", payload)


def _write_skti(bindings: list[SwgSkeletonBinding]) -> bytes:
    payload = b""
    for binding in bindings:
        payload += make_chunk_string(binding.skeleton_relpath)
        payload += make_chunk_string(binding.attachment_transform)
    return make_chunk("SKTI", payload)


def make_skeletal_appearance_for_bundle(
    *,
    mesh_relpaths: list[str],
    skeleton_bindings: list[SwgSkeletonBinding],
    create_animation_controller: bool = False,
    skeleton_lat_pairs: list[tuple[str, str]] | None = None,
) -> SwgSkeletalAppearance:
    """Build a SMAT 0003 referencing bundle-relative serverdata paths."""
    norm_mesh = [p.replace("\\", "/") for p in mesh_relpaths]
    norm_bindings = [
        SwgSkeletonBinding(
            skeleton_relpath=b.skeleton_relpath.replace("\\", "/"),
            attachment_transform=b.attachment_transform,
        )
        for b in skeleton_bindings
    ]
    lat = [(k.replace("\\", "/"), v.replace("\\", "/")) for k, v in (skeleton_lat_pairs or [])]
    return SwgSkeletalAppearance(
        version="0003",
        mesh_generator_paths=norm_mesh,
        skeleton_bindings=norm_bindings,
        create_animation_controller=create_animation_controller,
        skeleton_lat_pairs=lat,
    )
