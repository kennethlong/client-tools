"""Resolve swg-main serverdata paths (sibling repo to swg-client-v2)."""

from __future__ import annotations

import os
from pathlib import Path

_CLIENT_ROOT = Path(__file__).resolve().parents[3]
_DEFAULT_SWG_MAIN = _CLIENT_ROOT.parent / "swg-main"
_GOLDEN_DIR = Path(__file__).resolve().parents[1] / "tests" / "golden"


def swg_main_root() -> Path | None:
    env = os.environ.get("SWG_MAIN")
    if env:
        p = Path(env)
        return p if p.is_dir() else None
    return _DEFAULT_SWG_MAIN if _DEFAULT_SWG_MAIN.is_dir() else None


def serverdata_dir() -> Path | None:
    root = swg_main_root()
    if not root:
        return None
    d = root / "serverdata"
    return d if d.is_dir() else None


def golden_fixture(name: str) -> Path | None:
    """Offline copy under tests/golden/ (preferred for pytest)."""
    p = _GOLDEN_DIR / name
    return p if p.is_file() else None


def appearance_skeleton(name: str) -> Path | None:
    golden = golden_fixture(name)
    if golden:
        return golden
    sd = serverdata_dir()
    if not sd:
        return None
    p = sd / "appearance" / "skeleton" / name
    return p if p.is_file() else None


def appearance_component(name: str) -> Path | None:
    """Resolve ``appearance/component/foo.cmp`` under serverdata or golden."""
    golden = golden_fixture(name)
    if golden:
        return golden
    sd = serverdata_dir()
    if not sd:
        return None
    rel = name.replace("\\", "/").lstrip("/")
    if not rel.startswith("appearance/"):
        rel = f"appearance/component/{rel}"
    p = sd / rel
    return p if p.is_file() else None


def appearance_pob(name: str) -> Path | None:
    """Resolve ``appearance/foo.pob`` under serverdata or golden."""
    golden = golden_fixture(name)
    if golden:
        return golden
    sd = serverdata_dir()
    if not sd:
        return None
    rel = name.replace("\\", "/").lstrip("/")
    if not rel.startswith("appearance/"):
        rel = f"appearance/{rel}"
    if not rel.endswith(".pob"):
        rel = f"{rel}.pob"
    p = sd / rel
    return p if p.is_file() else None


def appearance_apt(name: str) -> Path | None:
    """Resolve ``appearance/foo.apt`` under serverdata or golden."""
    golden = golden_fixture(name)
    if golden:
        return golden
    sd = serverdata_dir()
    if not sd:
        return None
    rel = name.replace("\\", "/").lstrip("/")
    if not rel.startswith("appearance/"):
        rel = f"appearance/{rel}"
    if not rel.endswith(".apt"):
        rel = f"{rel}.apt"
    p = sd / rel
    return p if p.is_file() else None


def appearance_collision_floor(name: str) -> Path | None:
    """Resolve ``appearance/collision/foo.flr`` under serverdata or golden."""
    golden = golden_fixture(name)
    if golden:
        return golden
    sd = serverdata_dir()
    if not sd:
        return None
    rel = name.replace("\\", "/").lstrip("/")
    if not rel.startswith("appearance/"):
        rel = f"appearance/collision/{rel}"
    p = sd / rel
    return p if p.is_file() else None


def appearance_lod(name: str) -> Path | None:
    """Resolve ``appearance/lod/foo.lod`` under serverdata or golden."""
    golden = golden_fixture(name)
    if golden:
        return golden
    sd = serverdata_dir()
    if not sd:
        return None
    rel = name.replace("\\", "/").lstrip("/")
    if not rel.startswith("appearance/"):
        rel = f"appearance/lod/{rel}"
    p = sd / rel
    return p if p.is_file() else None


def appearance_mesh(name: str) -> Path | None:
    golden = golden_fixture(name)
    if golden:
        return golden
    sd = serverdata_dir()
    if not sd:
        return None
    p = sd / "appearance" / "mesh" / name
    return p if p.is_file() else None


def appearance_animation(name: str) -> Path | None:
    golden = golden_fixture(name)
    if golden:
        return golden
    sd = serverdata_dir()
    if not sd:
        return None
    p = sd / "appearance" / "animation" / name
    return p if p.is_file() else None


def shader_template(relpath: str) -> Path | None:
    """Resolve `shader/foo.sht` (or `foo.sht`) under serverdata."""
    sd = serverdata_dir()
    if not sd:
        return None
    rel = relpath.replace("\\", "/").lstrip("/")
    if not rel.startswith("shader/"):
        rel = f"shader/{rel}"
    p = sd / rel
    return p if p.is_file() else None


def serverdata_file(relpath: str) -> Path | None:
    """Resolve a TreeFile-relative path under ``serverdata/``."""
    sd = serverdata_dir()
    if not sd:
        return None
    rel = relpath.replace("\\", "/").lstrip("/")
    candidate = sd / rel
    return candidate if candidate.is_file() else None
