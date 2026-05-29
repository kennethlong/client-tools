"""Resolve building (POB) dependency graphs for TRE project round-trip."""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path

from swg_pipeline.creature_graph import (
    normalize_tre_relpath,
    resolve_on_disk,
    serverdata_roots_for,
    tre_relpath_for_file,
)
from swg_scene.appearance_redirect import load_appearance_redirect
from swg_scene.component_appearance import load_component_appearance
from swg_scene.detail_appearance import load_detail_appearance
from swg_scene.mesh_static import load_static_mesh
from swg_scene.pob_cells import SwgPortalCellSummary, load_portal_cells


@dataclass
class BuildingGraph:
    root_pob: str
    appearance_paths: list[str] = field(default_factory=list)
    floor_paths: list[str] = field(default_factory=list)
    mesh_paths: list[str] = field(default_factory=list)
    shader_paths: list[str] = field(default_factory=list)
    portal_count: int = 0
    cell_count: int = 0

    def to_dict(self) -> dict[str, object]:
        return {
            "root_pob": self.root_pob,
            "appearance_paths": list(self.appearance_paths),
            "floor_paths": list(self.floor_paths),
            "mesh_paths": list(self.mesh_paths),
            "shader_paths": list(self.shader_paths),
            "portal_count": self.portal_count,
            "cell_count": self.cell_count,
        }

    def all_asset_relpaths(self) -> list[str]:
        seen: set[str] = set()
        ordered: list[str] = []
        for group in (
            [self.root_pob],
            self.appearance_paths,
            self.floor_paths,
            self.mesh_paths,
            self.shader_paths,
        ):
            for rel in group:
                norm = normalize_tre_relpath(rel)
                if norm not in seen:
                    seen.add(norm)
                    ordered.append(norm)
        return ordered


def _collect_mesh_paths(
    relpath: str,
    roots: list[Path],
    *,
    seen: set[str],
    mesh_paths: list[str],
    appearance_paths: list[str],
    collect_shaders: bool,
    shader_paths: list[str],
    seen_shaders: set[str],
) -> None:
    norm = normalize_tre_relpath(relpath)
    if not norm:
        return

    path = resolve_on_disk(norm, roots)
    if path is None and not norm.startswith("appearance/"):
        path = resolve_on_disk(f"appearance/{norm}", roots)
    if path is None:
        return

    tre_rel = tre_relpath_for_file(path, roots)
    if tre_rel in seen:
        return
    seen.add(tre_rel)
    if tre_rel not in appearance_paths:
        appearance_paths.append(tre_rel)

    ext = path.suffix.lower()
    if ext == ".msh":
        if tre_rel not in mesh_paths:
            mesh_paths.append(tre_rel)
        if collect_shaders:
            scene = load_static_mesh(path)
            for mesh in scene.meshes:
                shader = normalize_tre_relpath(mesh.material.shader_relpath or "")
                if shader and shader not in seen_shaders:
                    seen_shaders.add(shader)
                    shader_paths.append(shader)
        return

    if ext == ".apt":
        target = load_appearance_redirect(path).target_path
        _collect_mesh_paths(
            target,
            roots,
            seen=seen,
            mesh_paths=mesh_paths,
            appearance_paths=appearance_paths,
            collect_shaders=collect_shaders,
            shader_paths=shader_paths,
            seen_shaders=seen_shaders,
        )
        return

    if ext == ".lod":
        dtla = load_detail_appearance(path)
        for level in dtla.levels:
            if level.appearance_path:
                _collect_mesh_paths(
                    level.appearance_path,
                    roots,
                    seen=seen,
                    mesh_paths=mesh_paths,
                    appearance_paths=appearance_paths,
                    collect_shaders=collect_shaders,
                    shader_paths=shader_paths,
                    seen_shaders=seen_shaders,
                )
        return

    if ext == ".cmp":
        cmp = load_component_appearance(path)
        for part in cmp.parts:
            _collect_mesh_paths(
                part.appearance_path,
                roots,
                seen=seen,
                mesh_paths=mesh_paths,
                appearance_paths=appearance_paths,
                collect_shaders=collect_shaders,
                shader_paths=shader_paths,
                seen_shaders=seen_shaders,
            )


def resolve_building_graph(
    pob_path: str | Path,
    *,
    workspace: Path | None = None,
    collect_shaders: bool = True,
) -> BuildingGraph:
    pob_file = Path(pob_path)
    if not pob_file.is_file():
        raise FileNotFoundError(pob_file)

    roots = serverdata_roots_for(pob_file, workspace)
    root_pob = tre_relpath_for_file(pob_file, roots)
    cells = load_portal_cells(pob_file)

    appearance_paths: list[str] = []
    floor_paths: list[str] = []
    mesh_paths: list[str] = []
    shader_paths: list[str] = []
    seen: set[str] = set()
    seen_shaders: set[str] = set()

    for cell in cells:
        if cell.appearance_relpath:
            _collect_mesh_paths(
                cell.appearance_relpath,
                roots,
                seen=seen,
                mesh_paths=mesh_paths,
                appearance_paths=appearance_paths,
                collect_shaders=collect_shaders,
                shader_paths=shader_paths,
                seen_shaders=seen_shaders,
            )
        if cell.floor_relpath:
            norm = normalize_tre_relpath(cell.floor_relpath)
            if norm and norm not in floor_paths:
                floor_paths.append(norm)

    from swg_scene.portal_property import load_portal_property

    spec = load_portal_property(pob_file)
    return BuildingGraph(
        root_pob=root_pob,
        appearance_paths=appearance_paths,
        floor_paths=floor_paths,
        mesh_paths=mesh_paths,
        shader_paths=shader_paths,
        portal_count=spec.portal_count,
        cell_count=spec.cell_count,
    )


def building_cells(
    graph: BuildingGraph,
    workspace: Path,
    *,
    pob_path: Path | None = None,
) -> list[dict[str, object]]:
    """Manifest entries per POB cell with resolved editable mesh paths."""
    if pob_path is None:
        pob_path = resolve_on_disk(graph.root_pob, serverdata_roots_for(workspace / graph.root_pob, workspace))
    if pob_path is None:
        return []

    roots = serverdata_roots_for(pob_path, workspace)
    summaries = load_portal_cells(pob_path)
    cells: list[dict[str, object]] = []

    for index, summary in enumerate(summaries):
        mesh_relpaths: list[str] = []
        seen_mesh: set[str] = set()
        seen_app: set[str] = set()
        if summary.appearance_relpath:
            before = len(mesh_relpaths)
            _collect_mesh_paths(
                summary.appearance_relpath,
                roots,
                seen=seen_app,
                mesh_paths=mesh_relpaths,
                appearance_paths=[],
                collect_shaders=False,
                shader_paths=[],
                seen_shaders=set(),
            )
            if not mesh_relpaths and before == 0:
                mesh_relpaths = []

        primary_mesh = mesh_relpaths[0] if mesh_relpaths else None
        cells.append(
            {
                "cell_index": index,
                "name": summary.name,
                "appearance_relpath": normalize_tre_relpath(summary.appearance_relpath),
                "floor_relpath": normalize_tre_relpath(summary.floor_relpath or ""),
                "mesh_relpaths": mesh_relpaths,
                "primary_mesh_relpath": primary_mesh,
                "can_see_parent": summary.can_see_parent,
                "portal_count": summary.portal_count,
            }
        )
    return cells
