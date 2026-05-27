"""Normal map bake helpers (Phase 8.6) — port of NormalMapUtility + TriangleRasterizer."""

from __future__ import annotations

import math
import struct
from dataclasses import dataclass
from typing import Iterable, Sequence

BIT_UNDER_ONE = 1.0 - 1.0 / 65536.0
def range_compress_normal_float(value: float) -> int:
    rc = value * 0.5 + 0.5
    byte_max = 256.0 * BIT_UNDER_ONE
    rci = int(math.floor(rc * byte_max))
    if rci < 0 or rci >= 256:
        raise ValueError(f"normal byte out of range: {rci}")
    return rci


def pack_bgr_normal(normal: Sequence[float]) -> tuple[int, int, int]:
    return (
        range_compress_normal_float(normal[2]),
        range_compress_normal_float(normal[1]),
        range_compress_normal_float(normal[0]),
    )


def unpack_bgr_normal(pixel: Sequence[int]) -> tuple[float, float, float]:
    nx = pixel[2] / 255.0 * 2.0 - 1.0
    ny = pixel[1] / 255.0 * 2.0 - 1.0
    nz = pixel[0] / 255.0 * 2.0 - 1.0
    length = math.sqrt(nx * nx + ny * ny + nz * nz) or 1.0
    return nx / length, ny / length, nz / length


def _best_slope(x1: float, y1: float, x2: float, y2: float, x3: float, y3: float) -> float:
    x_sum = x1 + x2 + x3
    y_sum = y1 + y2 + y3
    product_of_sums = x_sum * y_sum
    sum_of_products = x1 * y1 + x2 * y2 + x3 * y3
    sum_of_x_squares = x1 * x1 + x2 * x2 + x3 * x3
    sum_of_xs_squared = x_sum * x_sum
    denom = sum_of_x_squares - sum_of_xs_squared
    if denom == 0:
        return float("inf")
    return (sum_of_products - product_of_sums) / denom


def process_scratch_height_map(
    height_map: Sequence[int],
    width: int,
    height: int,
    *,
    scale: float = 1.0,
    wrap_u: bool = False,
    wrap_v: bool = False,
) -> list[tuple[float, float, float]]:
    """Convert an 8-bit height field to tangent-space normals (Maya scratch path)."""
    actual_scale = scale / 32.0
    normals: list[tuple[float, float, float]] = [(0.0, 0.0, 1.0)] * (width * height)
    for y in range(height):
        if wrap_v:
            yp1 = 0 if y + 1 == height else y + 1
            ym1 = height - 1 if y == 0 else y - 1
        else:
            yp1 = height - 1 if y + 1 == height else y + 1
            ym1 = 0 if y == 0 else y - 1
        for x in range(width):
            if wrap_u:
                xp1 = 0 if x + 1 == width else x + 1
                xm1 = width - 1 if x == 0 else x - 1
            else:
                xp1 = width - 1 if x + 1 == width else x + 1
                xm1 = 0 if x == 0 else x - 1
            z = actual_scale * float(height_map[y * width + x])
            north_z = actual_scale * float(height_map[yp1 * width + x])
            south_z = actual_scale * float(height_map[ym1 * width + x])
            east_z = actual_scale * float(height_map[y * width + xp1])
            west_z = actual_scale * float(height_map[y * width + xm1])
            m = _best_slope(-1.0, south_z, 0.0, z, 1.0, north_z)
            y_nx, y_ny, y_nz = 0.0, -m, 1.0
            y_len = math.sqrt(y_nx * y_nx + y_ny * y_ny + y_nz * y_nz) or 1.0
            y_nx, y_ny, y_nz = y_nx / y_len, y_ny / y_len, y_nz / y_len
            m = _best_slope(-1.0, west_z, 0.0, z, 1.0, east_z)
            x_nx, x_ny, x_nz = -m, 0.0, 1.0
            x_len = math.sqrt(x_nx * x_nx + x_ny * x_ny + x_nz * x_nz) or 1.0
            x_nx, x_ny, x_nz = x_nx / x_len, x_ny / x_len, x_nz / x_len
            nx = x_nx + y_nx
            ny = x_ny + y_ny
            nz = x_nz + y_nz
            n_len = math.sqrt(nx * nx + ny * ny + nz * nz) or 1.0
            normals[y * width + x] = (nx / n_len, ny / n_len, nz / n_len)
    return normals


def scratch_height_to_bgr(
    height_map: Sequence[int],
    width: int,
    height: int,
    *,
    scale: float = 1.0,
    wrap_u: bool = False,
    wrap_v: bool = False,
) -> bytes:
    normals = process_scratch_height_map(
        height_map, width, height, scale=scale, wrap_u=wrap_u, wrap_v=wrap_v
    )
    out = bytearray(width * height * 3)
    for index, normal in enumerate(normals):
        b, g, r = pack_bgr_normal(normal)
        offset = index * 3
        out[offset] = b
        out[offset + 1] = g
        out[offset + 2] = r
    return bytes(out)


def _normalize(vec: Sequence[float]) -> tuple[float, float, float]:
    x, y, z = vec
    length = math.sqrt(x * x + y * y + z * z) or 1.0
    return x / length, y / length, z / length


def _barycentric(px: float, py: float, ax: float, ay: float, bx: float, by: float, cx: float, cy: float) -> tuple[float, float, float] | None:
    v0x, v0y = cx - ax, cy - ay
    v1x, v1y = bx - ax, by - ay
    v2x, v2y = px - ax, py - ay
    dot00 = v0x * v0x + v0y * v0y
    dot01 = v0x * v1x + v0y * v1y
    dot02 = v0x * v2x + v0y * v2y
    dot11 = v1x * v1x + v1y * v1y
    dot12 = v1x * v2x + v1y * v2y
    denom = dot00 * dot11 - dot01 * dot01
    if abs(denom) < 1e-12:
        return None
    inv = 1.0 / denom
    u = (dot11 * dot02 - dot01 * dot12) * inv
    v = (dot00 * dot12 - dot01 * dot02) * inv
    if u < 0.0 or v < 0.0 or (u + v) > 1.0:
        return None
    return 1.0 - u - v, v, u


@dataclass
class _PixelHit:
    normal: tuple[float, float, float]
    count: int = 0


def bake_mesh_normal_map(
    positions: Sequence[Sequence[float]],
    normals: Sequence[Sequence[float]],
    uvs: Sequence[Sequence[float]],
    indices: Sequence[int],
    *,
    width: int,
    height: int,
    uv_set: int = 0,
) -> bytes:
    """Rasterize mesh normals into a BGR888 tangent-space normal map."""
    if width <= 0 or height <= 0:
        raise ValueError("width and height must be positive")
    if not indices:
        return bytes(width * height * 3)
    if uv_set >= len(uvs) or not uvs[uv_set]:
        raise ValueError(f"uv set {uv_set} missing")

    layer = uvs[uv_set]
    hits = [_PixelHit(normal=(0.0, 0.0, 1.0), count=0) for _ in range(width * height)]

    for tri in range(0, len(indices), 3):
        i0, i1, i2 = indices[tri], indices[tri + 1], indices[tri + 2]
        uv0 = layer[i0]
        uv1 = layer[i1]
        uv2 = layer[i2]
        ax, ay = uv0[0] * width, uv0[1] * height
        bx, by = uv1[0] * width, uv1[1] * height
        cx, cy = uv2[0] * width, uv2[1] * height
        n0 = _normalize(normals[i0])
        n1 = _normalize(normals[i1])
        n2 = _normalize(normals[i2])
        min_x = max(0, int(math.floor(min(ax, bx, cx))))
        max_x = min(width - 1, int(math.floor(max(ax, bx, cx))))
        min_y = max(0, int(math.floor(min(ay, by, cy))))
        max_y = min(height - 1, int(math.floor(max(ay, by, cy))))
        for py in range(min_y, max_y + 1):
            for px in range(min_x, max_x + 1):
                sample_x = px + 0.5
                sample_y = py + 0.5
                bary = _barycentric(sample_x, sample_y, ax, ay, bx, by, cx, cy)
                if bary is None:
                    continue
                w0, w1, w2 = bary
                nx = n0[0] * w0 + n1[0] * w1 + n2[0] * w2
                ny = n0[1] * w0 + n1[1] * w1 + n2[1] * w2
                nz = n0[2] * w0 + n1[2] * w1 + n2[2] * w2
                normal = _normalize((nx, ny, nz))
                offset = py * width + px
                hit = hits[offset]
                if hit.count:
                    hit.normal = _normalize(
                        (
                            hit.normal[0] + normal[0],
                            hit.normal[1] + normal[1],
                            hit.normal[2] + normal[2],
                        )
                    )
                else:
                    hit.normal = normal
                hit.count += 1

    out = bytearray(width * height * 3)
    for index, hit in enumerate(hits):
        b, g, r = pack_bgr_normal(hit.normal)
        offset = index * 3
        out[offset] = b
        out[offset + 1] = g
        out[offset + 2] = r
    return bytes(out)


def bake_swgmesh_normal_map(mesh, *, width: int = 256, height: int = 256, uv_set: int = 0) -> bytes:
    return bake_mesh_normal_map(
        mesh.positions,
        mesh.normals,
        mesh.uvs,
        mesh.indices,
        width=width,
        height=height,
        uv_set=uv_set,
    )


def write_bgr_png(path, width: int, height: int, bgr: bytes) -> None:
    from swg_pipeline.dds import _require_pillow

    _require_pillow()
    from PIL import Image

    if len(bgr) != width * height * 3:
        raise ValueError("bgr byte length does not match width*height*3")
    rgb = bytearray(len(bgr))
    for i in range(0, len(bgr), 3):
        rgb[i] = bgr[i + 2]
        rgb[i + 1] = bgr[i + 1]
        rgb[i + 2] = bgr[i]
    image = Image.frombytes("RGB", (width, height), bytes(rgb))
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    image.save(path)


def bake_swgmesh_normal_png(mesh, path, *, width: int = 256, height: int = 256, uv_set: int = 0) -> Path:
    bgr = bake_swgmesh_normal_map(mesh, width=width, height=height, uv_set=uv_set)
    write_bgr_png(path, width, height, bgr)
    return Path(path)
