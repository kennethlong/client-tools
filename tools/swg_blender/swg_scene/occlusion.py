"""SKMG occlusion zone chunks (OZN, FOZC, OZC, ZTO) load + export."""

from __future__ import annotations

from dataclasses import dataclass, field

from swg_iff.reader import IffReader
from swg_iff.tags import TAG_FOZC, TAG_OITL, TAG_OZN, TAG_OZC, TAG_ZTO
from swg_iff.writer import make_chunk, make_chunk_int16, make_chunk_int32, make_chunk_string


@dataclass
class SwgOcclusionData:
    zone_names: list[str] = field(default_factory=list)
    zone_combinations: list[list[int]] = field(default_factory=list)
    fully_occluded_zone_combination: list[int] | None = None
    zones_this_occludes: list[int] = field(default_factory=list)
    occlusion_layer: int = -1


def load_occlusion_from_reader(reader: IffReader, *, counts: tuple[int, int, int, int]) -> SwgOcclusionData:
    """Load OZN/FOZC/OZC/ZTO immediately after BLTS (before PSDT)."""
    zone_count, combination_count, zones_this_occludes_count, occlusion_layer = counts
    data = SwgOcclusionData(occlusion_layer=occlusion_layer)

    if zone_count > 0:
        reader.enter_chunk(TAG_OZN)
        data.zone_names = [reader.read_string() for _ in range(zone_count)]
        reader.exit_chunk(TAG_OZN)

    block = reader._peek_block()
    if not block.is_form and block.tag == TAG_FOZC:
        reader.enter_chunk(TAG_FOZC)
        fozc_count = reader.read_uint16()
        data.fully_occluded_zone_combination = [reader.read_int16() for _ in range(fozc_count)]
        reader.exit_chunk(TAG_FOZC)

    if combination_count > 0:
        reader.enter_chunk(TAG_OZC)
        for _ in range(combination_count):
            combo_size = reader.read_int16()
            data.zone_combinations.append([reader.read_int16() for _ in range(combo_size)])
        reader.exit_chunk(TAG_OZC)

    if zones_this_occludes_count > 0:
        reader.enter_chunk(TAG_ZTO)
        data.zones_this_occludes = [reader.read_int16() for _ in range(zones_this_occludes_count)]
        reader.exit_chunk(TAG_ZTO)

    return data


def write_occlusion_chunks(data: SwgOcclusionData) -> bytes:
    """Write OZN/FOZC/OZC/ZTO chunks in Maya/engine order."""
    payload = b""

    if data.zone_names:
        payload += make_chunk(
            "OZN",
            b"".join(make_chunk_string(name) for name in data.zone_names),
        )

    if data.fully_occluded_zone_combination is not None:
        fozc = make_chunk_int16(len(data.fully_occluded_zone_combination))
        fozc += b"".join(make_chunk_int16(i) for i in data.fully_occluded_zone_combination)
        payload += make_chunk("FOZC", fozc)

    if data.zone_combinations:
        ozc = b""
        for combo in data.zone_combinations:
            ozc += make_chunk_int16(len(combo))
            ozc += b"".join(make_chunk_int16(i) for i in combo)
        payload += make_chunk("OZC", ozc)

    if data.zones_this_occludes:
        zto = b"".join(make_chunk_int16(i) for i in data.zones_this_occludes)
        payload += make_chunk("ZTO", zto)

    return payload


def occlusion_counts(data: SwgOcclusionData) -> tuple[int, int, int, int]:
    return (
        len(data.zone_names),
        len(data.zone_combinations),
        len(data.zones_this_occludes),
        data.occlusion_layer,
    )


def write_oitl_chunk(triangles: list[tuple[int, int, int, int]]) -> bytes:
    """Write OITL: int16 zone combo + three int32 shader vertex indices per triangle."""
    payload = make_chunk_int32(len(triangles))
    for zone_index, i0, i1, i2 in triangles:
        payload += make_chunk_int16(zone_index)
        payload += make_chunk_int32(i0)
        payload += make_chunk_int32(i1)
        payload += make_chunk_int32(i2)
    return make_chunk("OITL", payload)


def load_oitl_triangles(reader: IffReader) -> list[tuple[int, int, int, int]]:
    reader.enter_chunk(TAG_OITL)
    tri_count = reader.read_int32()
    triangles: list[tuple[int, int, int, int]] = []
    for _ in range(tri_count):
        zone_index = reader.read_int16()
        i0 = reader.read_int32()
        i1 = reader.read_int32()
        i2 = reader.read_int32()
        triangles.append((zone_index, i0, i1, i2))
    reader.exit_chunk(TAG_OITL)
    return triangles
