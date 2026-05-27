"""Default paths for D:/Sample-TRE-Files test fixture."""

from __future__ import annotations

import hashlib
import json
import os
import shutil
from datetime import datetime, timezone
from pathlib import Path

DEFAULT_SAMPLE_TRE_DIR = Path(r"D:\Sample-TRE-Files")
PLAYGROUND_SUBDIR = "playground"
MANIFEST_NAME = ".playground_manifest.json"


def sample_tre_dir() -> Path | None:
    """Canonical read-only reference tree (original .tre / .toc)."""
    env = os.environ.get("SWG_SAMPLE_TRE_DIR")
    if env:
        p = Path(env)
        return p if p.is_dir() else None
    return DEFAULT_SAMPLE_TRE_DIR if DEFAULT_SAMPLE_TRE_DIR.is_dir() else None


def playground_dir() -> Path | None:
    """Writable copy under sample_tre_dir/playground/ for pack/write experiments."""
    root = sample_tre_dir()
    if not root:
        return None
    return root / PLAYGROUND_SUBDIR


def _archive_files(root: Path) -> list[Path]:
    return sorted(p for p in root.iterdir() if p.is_file() and p.suffix.lower() in {".tre", ".toc"})


def _file_fingerprint(path: Path) -> dict:
    st = path.stat()
    return {"name": path.name, "size": st.st_size, "mtime_ns": st.st_mtime_ns}


def playground_manifest(root: Path | None = None) -> dict | None:
    pg = (root or playground_dir())
    if not pg:
        return None
    manifest_path = pg / MANIFEST_NAME
    if not manifest_path.is_file():
        return None
    return json.loads(manifest_path.read_text(encoding="utf-8"))


def playground_is_current() -> bool:
    root = sample_tre_dir()
    pg = playground_dir()
    if not root or not pg or not pg.is_dir():
        return False
    manifest = playground_manifest(pg)
    if not manifest:
        return False
    src_fps = {_file_fingerprint(p)["name"]: _file_fingerprint(p) for p in _archive_files(root)}
    recorded = {e["name"]: e for e in manifest.get("files", [])}
    if set(src_fps) != set(recorded):
        return False
    return all(src_fps[n] == recorded[n] for n in src_fps)


def ensure_playground_copy(force: bool = False) -> Path:
    """
    Copy all .tre/.toc from sample_tre_dir into playground/.

    Skips if playground already matches originals unless force=True.
    Returns playground directory path.
    """
    root = sample_tre_dir()
    if not root:
        raise FileNotFoundError("Sample TRE directory not found (set SWG_SAMPLE_TRE_DIR)")
    pg = root / PLAYGROUND_SUBDIR
    sources = _archive_files(root)
    if not sources:
        raise FileNotFoundError(f"No .tre/.toc files in {root}")

    if not force and playground_is_current():
        return pg

    if pg.exists() and force:
        shutil.rmtree(pg)
    pg.mkdir(parents=True, exist_ok=True)

    copied = []
    for src in sources:
        dest = pg / src.name
        shutil.copy2(src, dest)
        copied.append(_file_fingerprint(src))

    manifest = {
        "source_dir": str(root),
        "created_at": datetime.now(timezone.utc).isoformat(),
        "files": copied,
        "digest": hashlib.sha256(
            json.dumps(copied, sort_keys=True).encode("utf-8")
        ).hexdigest(),
    }
    (pg / MANIFEST_NAME).write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    return pg


def master_toc(use_playground: bool = False) -> Path | None:
    d = playground_dir() if use_playground else sample_tre_dir()
    if not d or (use_playground and not d.is_dir()):
        if use_playground:
            d = sample_tre_dir()
        else:
            return None
    if not d:
        return None
    for name in ("SwgRestoration.toc", "swg.toc", "default.toc"):
        p = d / name
        if p.is_file():
            return p
    return None


def tre_files(use_playground: bool = False) -> list[Path]:
    d = playground_dir() if use_playground else sample_tre_dir()
    if use_playground and (not d or not d.is_dir()):
        d = sample_tre_dir()
    if not d:
        return []
    return sorted(d.glob("*.tre"))


def smallest_tre(max_bytes: int = 5_000_000, use_playground: bool = False) -> Path | None:
    candidates = [p for p in tre_files(use_playground=use_playground) if p.stat().st_size <= max_bytes]
    if not candidates:
        return None
    return min(candidates, key=lambda p: p.stat().st_size)