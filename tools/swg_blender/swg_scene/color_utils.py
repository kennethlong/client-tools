"""Vertex color packing helpers — Phase 8.4."""

from __future__ import annotations


def rgba_float_to_packed(r: float, g: float, b: float, a: float = 1.0) -> int:
    """Pack Blender RGBA floats into SWG A8R8G8B8 uint32."""
    ri = max(0, min(255, int(round(r * 255.0))))
    gi = max(0, min(255, int(round(g * 255.0))))
    bi = max(0, min(255, int(round(b * 255.0))))
    ai = max(0, min(255, int(round(a * 255.0))))
    return (ai << 24) | (ri << 16) | (gi << 8) | bi


def packed_to_rgba_float(color: int) -> tuple[float, float, float, float]:
    ai = (color >> 24) & 0xFF
    ri = (color >> 16) & 0xFF
    gi = (color >> 8) & 0xFF
    bi = color & 0xFF
    return ri / 255.0, gi / 255.0, bi / 255.0, ai / 255.0


def colors_are_default(colors0: list[int]) -> bool:
    if not colors0:
        return True
    return all(color in (0, 0xFFFFFFFF) for color in colors0)