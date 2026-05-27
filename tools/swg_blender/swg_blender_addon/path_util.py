"""Ensure tools/swg_blender is importable inside Blender."""

from __future__ import annotations

import os
import sys
from pathlib import Path

_DEFAULT_ROOT = Path(__file__).resolve().parents[1]


def default_pipeline_root() -> Path:
    return _DEFAULT_ROOT


def pipeline_root() -> Path:
    env = os.environ.get("SWG_BLENDER_ROOT", "").strip()
    if env:
        return Path(env)
    try:
        import bpy

        addon = bpy.context.preferences.addons.get("swg_blender_addon")
        if addon is not None:
            custom = getattr(addon.preferences, "pipeline_root", "").strip()
            if custom:
                return Path(custom)
    except Exception:
        pass
    return _DEFAULT_ROOT


def ensure_pipeline_on_path() -> Path:
    root = pipeline_root().resolve()
    root_str = str(root)
    if root_str not in sys.path:
        sys.path.insert(0, root_str)
    return root