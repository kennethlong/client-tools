"""CompressedQuaternion expand/compress (sharedMath/CompressedQuaternion.cpp)."""

from __future__ import annotations

import math

Quaternion = tuple[float, float, float, float]

_CS_X_SHIFT = 21
_CS_Y_SHIFT = 10
_CS_VALUE_MASK_ELEVEN = 0x3FF
_CS_SIGN_BIT_ELEVEN = 0x400
_CS_VALUE_MASK_TEN = 0x1FF
_CS_SIGN_BIT_TEN = 0x200

# format_id, base_shift_count (base_count = 1 << shift)
_FORMAT_PRECISION = (
    (0xFE, 0),
    (0xFC, 1),
    (0xF8, 2),
    (0xF0, 3),
    (0xE0, 4),
    (0xC0, 5),
    (0x80, 6),
)

_compress_factor_eleven: list[float] = []
_expand_factor_eleven: list[float] = []
_compress_factor_ten: list[float] = []
_expand_factor_ten: list[float] = []
_format_base_value: dict[int, float] = {}
_format_precision_index: dict[int, int] = {}
_installed = False


def _calculate_range(base_shift_count: int) -> float:
    base_count = 1 << base_shift_count
    return 4.0 / float(base_count + 1)


def _install() -> None:
    global _installed
    if _installed:
        return
    for base_shift_count in range(len(_FORMAT_PRECISION)):
        half_range = 0.5 * _calculate_range(base_shift_count)
        _compress_factor_eleven.append(0x3FF / half_range)
        _expand_factor_eleven.append(half_range / float(0x3FF))
        _compress_factor_ten.append(0x1FF / half_range)
        _expand_factor_ten.append(half_range / float(0x1FF))
    for format_id, base_shift_count in _FORMAT_PRECISION:
        base_count = 1 << base_shift_count
        base_separation = 2.0 / float(base_count + 1)
        for i in range(base_count):
            format_index = format_id | i
            _format_base_value[format_index] = -1.0 + (i + 1) * base_separation
            _format_precision_index[format_index] = base_shift_count
    _installed = True


def _expand_eleven(compressed_value: int, fmt: int) -> float:
    _install()
    base_value = _format_base_value[fmt]
    precision = _format_precision_index[fmt]
    if compressed_value & _CS_SIGN_BIT_ELEVEN:
        return base_value - float(compressed_value & _CS_VALUE_MASK_ELEVEN) * _expand_factor_eleven[precision]
    return base_value + float(compressed_value & _CS_VALUE_MASK_ELEVEN) * _expand_factor_eleven[precision]


def _expand_ten(compressed_value: int, fmt: int) -> float:
    _install()
    base_value = _format_base_value[fmt]
    precision = _format_precision_index[fmt]
    if compressed_value & _CS_SIGN_BIT_TEN:
        return base_value - float(compressed_value & _CS_VALUE_MASK_TEN) * _expand_factor_ten[precision]
    return base_value + float(compressed_value & _CS_VALUE_MASK_TEN) * _expand_factor_ten[precision]


def expand_quaternion(data: int, x_format: int, y_format: int, z_format: int) -> Quaternion:
    """Return (x, y, z, w) matching engine Quaternion component order used in IFF."""
    _install()
    x = _expand_eleven(data >> _CS_X_SHIFT, x_format)
    y = _expand_eleven(data >> _CS_Y_SHIFT, y_format)
    z = _expand_ten(data, z_format)
    w = math.sqrt(max(0.0, 1.0 - (x * x + y * y + z * z)))
    return (x, y, z, w)


def _compress_eleven(uncompressed_value: float, fmt: int) -> int:
    _install()
    base_value = _format_base_value[fmt]
    precision = _format_precision_index[fmt]
    factor = _compress_factor_eleven[precision]
    if uncompressed_value >= base_value:
        raw_value = int(factor * max(0.0, uncompressed_value - base_value))
        return min(_CS_VALUE_MASK_ELEVEN, raw_value)
    raw_value = int(factor * max(0.0, base_value - uncompressed_value))
    return _CS_SIGN_BIT_ELEVEN | min(_CS_VALUE_MASK_ELEVEN, raw_value)


def _compress_ten(uncompressed_value: float, fmt: int) -> int:
    _install()
    base_value = _format_base_value[fmt]
    precision = _format_precision_index[fmt]
    factor = _compress_factor_ten[precision]
    if uncompressed_value >= base_value:
        raw_value = int(factor * max(0.0, uncompressed_value - base_value))
        return min(_CS_VALUE_MASK_TEN, raw_value)
    raw_value = int(factor * max(0.0, base_value - uncompressed_value))
    return _CS_SIGN_BIT_TEN | min(_CS_VALUE_MASK_TEN, raw_value)


def _find_closest_base(base_shift_count: int, midpoint: float) -> tuple[int, float]:
    base_count = 1 << base_shift_count
    base_separation = 2.0 / float(base_count + 1)
    closest_base_distance = float("inf")
    closest_base_value = 0.0
    closest_base_index = 0
    for test_base_index in range(base_count):
        test_base_value = -1.0 + (test_base_index + 1) * base_separation
        test_distance = abs(test_base_value - midpoint)
        if test_distance < closest_base_distance:
            closest_base_distance = test_distance
            closest_base_value = test_base_value
            closest_base_index = test_base_index
        else:
            break
    return closest_base_index, closest_base_value


def _find_base_shift_count_covering_range(range_value: float) -> int:
    for base_shift_count in range(len(_FORMAT_PRECISION) - 1, -1, -1):
        if _calculate_range(base_shift_count) >= range_value:
            return base_shift_count
    raise ValueError(f"failed to find encoding for range {range_value}")


def _find_format_for_range(base_shift_count: int, min_value: float, max_value: float) -> int | None:
    range_value = max_value - min_value
    midpoint = min_value + 0.5 * range_value
    base_index, base_value = _find_closest_base(base_shift_count, midpoint)
    format_half_range = 0.5 * _calculate_range(base_shift_count)
    if min_value < (base_value - format_half_range) or max_value > (base_value + format_half_range):
        return None
    format_id, _ = _FORMAT_PRECISION[base_shift_count]
    return format_id | base_index


def get_optimal_compression_format(min_value: float, max_value: float) -> int:
    if min_value > max_value:
        raise ValueError("min_value > max_value")
    base_shift_count = _find_base_shift_count_covering_range(max_value - min_value)
    while base_shift_count >= 0:
        fmt = _find_format_for_range(base_shift_count, min_value, max_value)
        if fmt is not None:
            return fmt
        base_shift_count -= 1
    raise ValueError(f"failed to find encoding for range [{min_value}, {max_value}]")


def get_optimal_compression_format_for_rotations(rotations: list[Quaternion]) -> tuple[int, int, int]:
    if not rotations:
        least_precise = _FORMAT_PRECISION[0][0]
        return least_precise, least_precise, least_precise

    min_x = min_y = min_z = float("inf")
    max_x = max_y = max_z = float("-inf")
    for x, y, z, w in rotations:
        if w < 0.0:
            x, y, z = -x, -y, -z
        min_x = min(min_x, x)
        max_x = max(max_x, x)
        min_y = min(min_y, y)
        max_y = max(max_y, y)
        min_z = min(min_z, z)
        max_z = max(max_z, z)

    return (
        get_optimal_compression_format(min_x, max_x),
        get_optimal_compression_format(min_y, max_y),
        get_optimal_compression_format(min_z, max_z),
    )


def compress_quaternion(
    x: float,
    y: float,
    z: float,
    w: float,
    x_format: int,
    y_format: int,
    z_format: int,
) -> int:
    if w < 0.0:
        w = -w
        x = -x
        y = -y
        z = -z
    x_packed = _compress_eleven(x, x_format)
    y_packed = _compress_eleven(y, y_format)
    z_packed = _compress_ten(z, z_format)
    return (x_packed << _CS_X_SHIFT) | (y_packed << _CS_Y_SHIFT) | z_packed


def compress_rotations(
    rotations: list[Quaternion],
    x_format: int,
    y_format: int,
    z_format: int,
) -> list[int]:
    return [compress_quaternion(x, y, z, w, x_format, y_format, z_format) for x, y, z, w in rotations]
