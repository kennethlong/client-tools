"""Resolve creature (SMAT) dependency graphs for TRE project round-trip."""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path

from swg_pipeline.swg_main_paths import serverdata_dir, serverdata_file
from swg_scene.mesh_lod import load_mesh_lod
from swg_scene.mesh_skeletal import load_skeletal_mesh
from swg_scene.skeletal_appearance import load_skeletal_appearance


def normalize_tre_relpath(relpath: str) -> str:
    return relpath.replace("\\", "/").lstrip("/")


def serverdata_roots_for(sat_path: Path, workspace: Path | None = None) -> list[Path]:
    """Search order for TreeFile-relative paths."""
    roots: list[Path] = []
    if workspace is not None:
        roots.append(workspace)
    sat_path = sat_path.resolve()
    if sat_path.parent.name == "appearance":
        roots.append(sat_path.parent.parent)
    sd = serverdata_dir()
    if sd is not None:
        roots.append(sd)
    return roots


def tre_relpath_for_file(path: Path, roots: list[Path]) -> str:
    resolved = path.resolve()
    for root in roots:
        try:
            return normalize_tre_relpath(str(resolved.relative_to(root.resolve())))
        except ValueError:
            continue
    if resolved.parent.name == "appearance":
        return normalize_tre_relpath(f"appearance/{resolved.name}")
    return normalize_tre_relpath(resolved.name)


def resolve_on_disk(relpath: str, roots: list[Path]) -> Path | None:
    norm = normalize_tre_relpath(relpath)
    for root in roots:
        candidate = root / norm
        if candidate.is_file():
            return candidate
    return serverdata_file(norm)


@dataclass
class CreatureGraph:
    root_sat: str
    mesh_generator_paths: list[str] = field(default_factory=list)
    mesh_lod_paths: list[str] = field(default_factory=list)
    mesh_paths: list[str] = field(default_factory=list)
    skeleton_paths: list[str] = field(default_factory=list)
    lat_paths: list[str] = field(default_factory=list)
    shader_paths: list[str] = field(default_factory=list)

    def to_dict(self) -> dict[str, list[str] | str]:
        return {
            "root_sat": self.root_sat,
            "mesh_generator_paths": list(self.mesh_generator_paths),
            "mesh_lod_paths": list(self.mesh_lod_paths),
            "mesh_paths": list(self.mesh_paths),
            "skeleton_paths": list(self.skeleton_paths),
            "lat_paths": list(self.lat_paths),
            "shader_paths": list(self.shader_paths),
        }

    def all_asset_relpaths(self) -> list[str]:
        seen: set[str] = set()
        ordered: list[str] = []
        for group in (
            [self.root_sat],
            self.mesh_generator_paths,
            self.mesh_lod_paths,
            self.mesh_paths,
            self.skeleton_paths,
            self.lat_paths,
            self.shader_paths,
        ):
            for rel in group:
                norm = normalize_tre_relpath(rel)
                if norm not in seen:
                    seen.add(norm)
                    ordered.append(norm)
        return ordered


def resolve_creature_graph(
    sat_path: str | Path,
    *,
    workspace: Path | None = None,
    collect_shaders: bool = True,
) -> CreatureGraph:
    """
    Walk SAT -> LMG -> MGN and collect TreeFile-relative dependencies.

    ``sat_path`` may be under a TRE workspace (``.../appearance/foo.sat``) or
    swg-main ``serverdata``.
    """
    sat_file = Path(sat_path)
    if not sat_file.is_file():
        raise FileNotFoundError(sat_file)

    sat = load_skeletal_appearance(sat_file)
    roots = serverdata_roots_for(sat_file, workspace)
    root_sat = tre_relpath_for_file(sat_file, roots)
    mesh_lod_paths: list[str] = []
    mesh_paths: list[str] = []
    shader_paths: list[str] = []
    seen_shaders: set[str] = set()

    for lmg_rel in sat.mesh_generator_paths:
        norm_lmg = normalize_tre_relpath(lmg_rel)
        mesh_lod_paths.append(norm_lmg)
        lmg_file = resolve_on_disk(norm_lmg, roots)
        if lmg_file is None:
            continue
        lmg = load_mesh_lod(lmg_file)
        for mgn_rel in lmg.level_paths:
            norm_mgn = normalize_tre_relpath(mgn_rel)
            mesh_paths.append(norm_mgn)
            if not collect_shaders:
                continue
            mgn_file = resolve_on_disk(norm_mgn, roots)
            if mgn_file is None:
                continue
            scene = load_skeletal_mesh(mgn_file)
            for mesh in scene.meshes:
                shader = normalize_tre_relpath(mesh.material.shader_relpath or "")
                if shader and shader not in seen_shaders:
                    seen_shaders.add(shader)
                    shader_paths.append(shader)

    skeleton_paths = [
        normalize_tre_relpath(b.skeleton_relpath) for b in sat.skeleton_bindings
    ]
    lat_paths = [normalize_tre_relpath(v) for _, v in sat.skeleton_lat_pairs]

    return CreatureGraph(
        root_sat=root_sat,
        mesh_generator_paths=[normalize_tre_relpath(p) for p in sat.mesh_generator_paths],
        mesh_lod_paths=mesh_lod_paths,
        mesh_paths=mesh_paths,
        skeleton_paths=skeleton_paths,
        lat_paths=lat_paths,
        shader_paths=shader_paths,
    )


def _pick_lod0_mgn(level_paths: list[str]) -> str | None:
    for rel in level_paths:
        norm = normalize_tre_relpath(rel)
        if "_l0" in norm.lower() or norm.lower().endswith("l0.mgn"):
            return norm
    if level_paths:
        return normalize_tre_relpath(level_paths[0])
    return None


def creature_parts(graph: CreatureGraph, workspace: Path) -> list[dict[str, object]]:
    """One Blender part per SAT mesh generator (.lmg), default mesh = LOD0."""
    from swg_scene.mesh_lod import load_mesh_lod

    roots = serverdata_roots_for(workspace / graph.root_sat, workspace)
    parts: list[dict[str, object]] = []
    for index, lmg_rel in enumerate(graph.mesh_generator_paths):
        norm_lmg = normalize_tre_relpath(lmg_rel)
        lod_mgn_relpaths: list[str] = []
        lmg_file = resolve_on_disk(norm_lmg, roots)
        if lmg_file is not None:
            lod_mgn_relpaths = [
                normalize_tre_relpath(p) for p in load_mesh_lod(lmg_file).level_paths
            ]
        lod0 = _pick_lod0_mgn(lod_mgn_relpaths) if lod_mgn_relpaths else None
        parts.append(
            {
                "part_index": index,
                "lmg_relpath": norm_lmg,
                "lod0_mgn_relpath": lod0,
                "lod_mgn_relpaths": lod_mgn_relpaths,
                "object_name": Path(norm_lmg).stem,
            }
        )
    return parts
