"""Export manifest JSON (Maya .log replacement) — Phase 14."""

from __future__ import annotations

import json
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

# Matches legacy MayaExporter-style tooling version stamp.
PIPELINE_VERSION = "3.009"


def utc_now_iso() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat()


def build_export_manifest(
    *,
    bundle_type: str,
    output_dir: str | Path,
    assets: dict[str, Any],
    author: str = "",
    notes: str = "",
    tre_files: list[str] | None = None,
    rsp_files: list[str] | None = None,
    extra: dict[str, Any] | None = None,
) -> dict[str, Any]:
    manifest: dict[str, Any] = {
        "pipeline_version": PIPELINE_VERSION,
        "exported_at": utc_now_iso(),
        "bundle_type": bundle_type,
        "output_dir": str(Path(output_dir)),
        "author": author,
        "notes": notes,
        "assets": assets,
    }
    if rsp_files:
        manifest["rsp_files"] = rsp_files
    if tre_files:
        manifest["tre_files"] = tre_files
    if extra:
        manifest.update(extra)
    return manifest


def write_export_manifest(path: str | Path, manifest: dict[str, Any]) -> Path:
    out = Path(path)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    return out


def apply_manifest_author_notes(
    manifest_path: str | Path,
    *,
    author: str = "",
    notes: str = "",
) -> None:
    path = Path(manifest_path)
    if not path.is_file():
        return
    data = json.loads(path.read_text(encoding="utf-8"))
    if author:
        data["author"] = author
    if notes:
        data["notes"] = notes
    write_export_manifest(path, data)


def append_tre_to_manifest(manifest_path: Path, tre_paths: list[Path]) -> None:
    if not manifest_path.is_file():
        return
    data = json.loads(manifest_path.read_text(encoding="utf-8"))
    existing = list(data.get("tre_files") or [])
    for p in tre_paths:
        rel = str(p)
        if rel not in existing:
            existing.append(rel)
    data["tre_files"] = existing
    data["packed_at"] = utc_now_iso()
    write_export_manifest(manifest_path, data)
