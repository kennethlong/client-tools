"""Building TRE project import/export (M17)."""

from __future__ import annotations

import json
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from swg_pipeline.building_graph import (
    BuildingGraph,
    building_cells,
    resolve_building_graph,
)
from swg_pipeline.creature_graph import normalize_tre_relpath
from swg_pipeline.export_manifest import PIPELINE_VERSION, utc_now_iso
from swg_pipeline.texture_bundle import copy_textures_from_shader_file
from swg_pipeline.tre_project import (
    PROJECT_MANIFEST,
    MaterializedAsset,
    TreProjectError,
    load_project_manifest,
    materialize_asset,
)


@dataclass
class ImportBuildingResult:
    workspace: Path
    manifest_path: Path
    graph: BuildingGraph
    materialized: list[MaterializedAsset] = field(default_factory=list)
    missing: list[str] = field(default_factory=list)
    ok: bool = True

    def as_dict(self) -> dict[str, Any]:
        return {
            "ok": self.ok,
            "workspace": str(self.workspace),
            "manifest_path": str(self.manifest_path),
            "root_pob": self.graph.root_pob,
            "missing": list(self.missing),
            "materialized_count": sum(1 for m in self.materialized if m.path),
        }


@dataclass
class ExportBuildingResult:
    workspace: Path
    manifest_path: Path
    exported_msh: list[str] = field(default_factory=list)
    ok: bool = True

    def as_dict(self) -> dict[str, Any]:
        return {
            "ok": self.ok,
            "workspace": str(self.workspace),
            "exported_msh": list(self.exported_msh),
        }


def write_building_manifest(
    workspace: Path,
    *,
    graph: BuildingGraph,
    source_tre: str | None,
    materialized: list[MaterializedAsset],
    missing: list[str],
    cells: list[dict[str, Any]],
) -> Path:
    manifest_path = workspace / PROJECT_MANIFEST
    sources = {m.relpath: m.source for m in materialized if m.path is not None}
    payload = {
        "pipeline_version": PIPELINE_VERSION,
        "created_at": utc_now_iso(),
        "project_type": "building",
        "source_tre": source_tre or "",
        "root_pob": graph.root_pob,
        "graph": graph.to_dict(),
        "cells": cells,
        "materialized": sources,
        "missing": missing,
    }
    manifest_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    return manifest_path


def import_building_project(
    workspace: str | Path,
    pob_relpath: str,
    *,
    tre_path: str | Path | None = None,
    copy_textures: bool = False,
) -> ImportBuildingResult:
    from swg_pipeline.tre_project import _tre_index

    workspace = Path(workspace).resolve()
    workspace.mkdir(parents=True, exist_ok=True)
    tre = Path(tre_path).resolve() if tre_path else None
    if tre is not None and not tre.is_file():
        raise TreProjectError(f"TRE not found: {tre}")

    pob_norm = normalize_tre_relpath(pob_relpath)
    tre_index = _tre_index(tre)

    pob_result = materialize_asset(pob_norm, workspace, tre_path=tre, tre_index=tre_index)
    if pob_result.path is None:
        raise TreProjectError(
            f"cannot resolve POB {pob_norm!r} from TRE or serverdata"
        )

    graph = resolve_building_graph(pob_result.path, workspace=workspace)
    materialized: list[MaterializedAsset] = [pob_result]
    missing: list[str] = []

    for rel in graph.all_asset_relpaths():
        if rel == pob_norm:
            continue
        result = materialize_asset(rel, workspace, tre_path=tre, tre_index=tre_index)
        materialized.append(result)
        if result.path is None:
            missing.append(rel)

    if copy_textures:
        for rel in graph.shader_paths:
            shader_path = workspace / rel
            if shader_path.is_file():
                copy_textures_from_shader_file(shader_path, workspace)

    cells = building_cells(graph, workspace)
    manifest_path = write_building_manifest(
        workspace,
        graph=graph,
        source_tre=str(tre) if tre else None,
        materialized=materialized,
        missing=missing,
        cells=cells,
    )

    return ImportBuildingResult(
        workspace=workspace,
        manifest_path=manifest_path,
        graph=graph,
        materialized=materialized,
        missing=missing,
        ok=not missing,
    )


def _resolve_blender_exe() -> str:
    import os
    import shutil
    from glob import glob

    env = os.environ.get("BLENDER_EXE", "").strip()
    if env and Path(env).is_file():
        return env
    found = shutil.which("blender") or shutil.which("blender.exe")
    if found:
        return found
    matches = sorted(
        glob(r"C:\Program Files\Blender Foundation\Blender *\blender.exe"),
        reverse=True,
    )
    if matches:
        return matches[0]
    raise TreProjectError(
        "Blender not found: set BLENDER_EXE or install Blender on PATH"
    )


def _parse_blender_json(stdout: str) -> dict[str, Any]:
    for line in reversed(stdout.strip().splitlines()):
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            return json.loads(line)
        except json.JSONDecodeError:
            continue
    raise TreProjectError("no JSON result line in Blender stdout")


def run_blender_import_building(workspace: Path) -> subprocess.CompletedProcess[str]:
    script = Path(__file__).resolve().parent / "tre_project_bpy_building.py"
    cmd = [
        _resolve_blender_exe(),
        "--background",
        "--python",
        str(script),
        "--",
        "--workspace",
        str(workspace.resolve()),
    ]
    return subprocess.run(cmd, capture_output=True, text=True)


def run_blender_export_building(workspace: Path) -> subprocess.CompletedProcess[str]:
    script = Path(__file__).resolve().parent / "tre_project_bpy_building.py"
    cmd = [
        _resolve_blender_exe(),
        "--background",
        "--python",
        str(script),
        "--",
        "--workspace",
        str(workspace.resolve()),
        "--export",
    ]
    return subprocess.run(cmd, capture_output=True, text=True)


def rewrite_building_pob(
    workspace: str | Path,
    manifest: dict[str, Any] | None = None,
    *,
    portal_geometry_raw: list[bytes] | None = None,
) -> Path:
    """Rewrite POB CELS from manifest; optional PRTS IDTL blocks; preserve PGRF/CRC."""
    from swg_scene.pob_cells import (
        _decode_cell_raw,
        cell_fields_unchanged,
        summary_from_manifest_cell,
        write_cell_bytes,
    )
    from swg_scene.portal_property import load_portal_property, write_portal_property

    workspace = Path(workspace).resolve()
    if manifest is None:
        manifest = load_project_manifest(workspace)
    pob_path = workspace / str(manifest["root_pob"]).replace("\\", "/")
    if not pob_path.is_file():
        raise TreProjectError(f"POB not found in workspace: {pob_path}")

    spec = load_portal_property(pob_path)
    if portal_geometry_raw is not None:
        if len(portal_geometry_raw) != spec.portal_count:
            raise TreProjectError(
                f"portal blocks {len(portal_geometry_raw)} != POB portal_count {spec.portal_count}"
            )
        spec.portal_geometry_raw = list(portal_geometry_raw)
    decoded = [_decode_cell_raw(raw) for raw in spec.cells_raw]
    manifest_cells = list(manifest.get("cells") or [])
    if len(manifest_cells) != len(decoded):
        raise TreProjectError(
            f"manifest cells {len(manifest_cells)} != POB cells {len(decoded)}"
        )

    new_cells_raw: list[bytes] = []
    for entry, original in zip(manifest_cells, decoded, strict=True):
        if cell_fields_unchanged(entry, original):
            new_cells_raw.append(original.cell_raw)
            continue
        portal_count = int(entry.get("portal_count", original.portal_count))
        summary = summary_from_manifest_cell(
            entry,
            portal_count=portal_count,
            extra_blocks=original.extra_blocks,
        )
        new_cells_raw.append(write_cell_bytes(summary))

    spec.cells_raw = new_cells_raw
    write_portal_property(pob_path, spec)
    return pob_path


def run_blender_building_roundtrip(
    workspace: Path, *, ignore_blend_targets: bool = False
) -> subprocess.CompletedProcess[str]:
    script = Path(__file__).resolve().parent / "tre_project_bpy_building.py"
    cmd = [
        _resolve_blender_exe(),
        "--background",
        "--python",
        str(script),
        "--",
        "--workspace",
        str(workspace.resolve()),
        "--roundtrip",
    ]
    if ignore_blend_targets:
        cmd.append("--ignore-blend-targets")
    return subprocess.run(cmd, capture_output=True, text=True)


def export_building_project(
    workspace: str | Path,
    *,
    use_blender: bool = False,
    rebuild_rsp: bool = False,
) -> ExportBuildingResult:
    workspace = Path(workspace).resolve()
    manifest = load_project_manifest(workspace)
    if manifest.get("project_type") != "building":
        raise TreProjectError("workspace manifest is not a building project")

    exported_msh: list[str] = []
    blender_exports: dict[str, Any] = {"skipped": True}

    if use_blender:
        proc = run_blender_export_building(workspace)
        if proc.returncode != 0:
            err = (proc.stderr or proc.stdout or "").strip()
            raise TreProjectError(f"Blender building export failed: {err}")
        blender_exports = _parse_blender_json(proc.stdout or "")
        if not blender_exports.get("ok"):
            raise TreProjectError(blender_exports.get("error", "export failed"))
        exported_msh = list(blender_exports.get("exported_msh") or [])

    manifest_path = workspace / PROJECT_MANIFEST
    manifest["last_blender_export"] = blender_exports
    if use_blender:
        manifest = load_project_manifest(workspace)
    else:
        rewrite_building_pob(workspace, manifest)
    manifest["last_pob_rewrite"] = str(manifest["root_pob"])
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")

    if rebuild_rsp:
        from swg_pipeline.pack_pipeline import pack_bundle

        pack_bundle(workspace, rebuild_rsp=True, update_manifest=True)

    return ExportBuildingResult(
        workspace=workspace,
        manifest_path=manifest_path,
        exported_msh=exported_msh,
        ok=True,
    )
