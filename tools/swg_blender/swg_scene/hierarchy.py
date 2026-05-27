"""Maya hierarchy naming conventions for Blender export (Phase 11.8)."""

from __future__ import annotations

import re
from dataclasses import dataclass, field

# Mirrors MayaHierarchy.cpp decode table (prefix -> node type).
NODE_PREFIXES: tuple[tuple[str, str], ...] = (
    ("mesh", "msh"),
    ("c", "cmp"),
    ("l", "lod"),
    ("radar", "lod"),
    ("test", "lod"),
    ("write", "lod"),
    ("collision", "collision"),
    ("floor", "floor"),
    ("extent", "extent"),
    ("sphere", "extent"),
    ("cube", "extent"),
    ("cylinder", "extent"),
    ("psphere", "extent"),
    ("pcube", "extent"),
    ("pcylinder", "extent"),
    ("pob", "pob"),
    ("building", "pob"),
    ("portals", "pls"),
    ("portal", "prt"),
    ("cells", "cel"),
)


@dataclass
class HierarchyNodeInfo:
    node_type: str
    index: int | None = None
    raw_name: str = ""


@dataclass
class HierarchyIssue:
    path: str
    message: str


@dataclass
class HierarchyReport:
    ok: bool
    issues: list[HierarchyIssue] = field(default_factory=list)

    def error(self, path: str, message: str) -> None:
        self.ok = False
        self.issues.append(HierarchyIssue(path, message))


_INDEX_RE = re.compile(r"(\d+)$")


def decode_export_name(name: str) -> HierarchyNodeInfo | None:
    """Decode Maya-style export node name (e.g. mesh0, c1, l2, radar)."""
    base = name.strip().lower()
    if not base or base in ("exportroot", "donotuse"):
        return None
    for prefix, node_type in NODE_PREFIXES:
        if base == prefix:
            return HierarchyNodeInfo(node_type=node_type, index=None, raw_name=name)
        if base.startswith(prefix):
            suffix = base[len(prefix) :]
            if suffix.isdigit():
                return HierarchyNodeInfo(
                    node_type=node_type,
                    index=int(suffix),
                    raw_name=name,
                )
    return None


def validate_hierarchy_names(names: list[str], *, root: str = "") -> HierarchyReport:
    """Lint a list of child empty/collection names for export conventions."""
    report = HierarchyReport(ok=True)
    seen_types: dict[str, set[int | None]] = {}
    for name in names:
        info = decode_export_name(name)
        path = f"{root}/{name}" if root else name
        if info is None:
            report.error(path, "unrecognized export node name")
            continue
        bucket = seen_types.setdefault(info.node_type, set())
        if info.index in bucket:
            report.error(path, f"duplicate {info.node_type} index {info.index}")
        bucket.add(info.index)
    return report
