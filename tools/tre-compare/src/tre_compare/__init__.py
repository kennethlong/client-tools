"""tre-compare — standalone, extractable SWG TRE/TOC compare-tool foundation (Phase 28).

Re-exports the public surface so downstream (Phase-29 diff/API) imports stay clean and
refactor-free:
  * parser  -- vendored TRE/TOC/COT2000 reader (Plan 02)
  * scanner -- hand-written ``[SharedFile]`` parser -> engine-faithful SearchNode list (Plan 03)
  * virtual_tree -- first-hit-wins merge + per-node-type tombstone + searchPath walk (Plan 03)
"""

from .scanner import ScanResult, SearchNode, parse_shared_file
from .virtual_tree import (
    MergedEntry,
    VirtualTree,
    build_virtual_tree,
    fix_up_file_name,
    safe_virtual_key,
)

__all__ = [
    "ScanResult",
    "SearchNode",
    "parse_shared_file",
    "MergedEntry",
    "VirtualTree",
    "build_virtual_tree",
    "fix_up_file_name",
    "safe_virtual_key",
]
