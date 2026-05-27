"""Coordinate conversion between SWG engine space and Blender.

MayaExporter flips X when writing (MayaUtility.cpp): engine_x = -maya_x.
Scale is ignored in Maya export.

Static mesh import applies a +90 deg CCW object rotation about X in
``swg_blender.import_static`` (see ``IMPORT_ROTATION_EULER``).
"""

from __future__ import annotations


def engine_to_blender_position(x: float, y: float, z: float) -> tuple[float, float, float]:
    return (-x, y, z)


def engine_to_blender_vector(x: float, y: float, z: float) -> tuple[float, float, float]:
    return (-x, y, z)


def blender_to_engine_position(x: float, y: float, z: float) -> tuple[float, float, float]:
    return (-x, y, z)


def blender_to_engine_vector(x: float, y: float, z: float) -> tuple[float, float, float]:
    return (-x, y, z)