"""Hand-written ``[SharedFile]`` scanner for SWG ``client.cfg`` (Phase-28).

Parses the engine's ``searchTOC`` / ``searchTree`` / ``searchPath`` keys out of a
``client.cfg`` ``[SharedFile]`` section and yields an **engine-faithful**, priority-
ordered :class:`SearchNode` list that the virtual-tree merge consumes.

Why a hand parser (RESEARCH §Architecture Pattern 1, §Common Pitfalls 4-5):
    stdlib ``configparser`` cannot handle this grammar. The engine reads each
    ``searchXXX_NN_P`` key via a ``for (index; ...; ++index)`` loop, so the SAME key
    name may legally appear multiple times in one cfg (each occurrence is a distinct
    search node). ``configparser`` raises ``DuplicateOptionError`` (strict) or silently
    keeps the last value. We therefore scan the file line-by-line.

Engine-faithful ordering rule (review finding #1, VERIFIED vs TreeFile.cpp:118-148,
:285, :299-305 this session):
    The C++ ``searchNodePriorityOrder`` comparator orders by ``priority`` ONLY
    (``a > b`` → higher first). The FINAL node order is the REGISTRATION order: the
    init loop runs, for each priority 0..max, in a FIXED KIND ORDER — (1) all
    searchPath keys, (2) all searchTree keys, (3) all searchTOC keys — and
    ``addSearchNode`` inserts via ``std::lower_bound`` with that strict ``>`` comparator
    ("inserted after the last priority match"), preserving registration order within an
    equal-priority run. So for ANY (priority, kind) tie group the order is::

        (-priority, kind_rank, cfg_seq)   with KIND_RANK = {"path": 0, "tree": 1, "toc": 2}

    where ``cfg_seq`` is the cfg discovery index, used ONLY to break ties WITHIN the
    same (priority, kind). A plain ``-priority`` stable sort (the round-1 draft) is WRONG
    for same-priority CROSS-KIND ties — it would order them by cfg line order, diverging
    from the client.

D-08: the cfg path is a REQUIRED parameter; the live client.cfg location is never
hardcoded (a real install cfg uses machine-specific absolute paths like
``D:/Code/SWGSource Client v3.0/``).
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from pathlib import Path

# Engine per-priority registration order (review finding #1). Within one priority the
# engine registers searchPath first, then searchTree, then searchTOC.
KIND_RANK = {"path": 0, "tree": 1, "toc": 2}

# Two key grammars the engine emits (KEY-NAME GRAMMAR, TreeFile.cpp:113-115, :123):
#   sprintf(skuText, "_%02d_", sku); sprintf(buffer, "searchTOC%s%d", skuText, priority);
# When skuBits != 0 the middle is "_NN_" (NN = SKU number); when skuBits == 0 (legacy /
# no sku flagged) skuText is the EMPTY string, so the engine ALSO emits the bare-priority
# form `searchTOC10` etc. We parse BOTH (review finding #6). Priority is the FINAL
# trailing integer either way (Pitfall 5).
_RE_SKU_KEY = re.compile(r"^search(TOC|Tree|Path)_(\d+)_(\d+)$", re.IGNORECASE)
_RE_BARE_KEY = re.compile(r"^search(TOC|Tree|Path)(\d+)$", re.IGNORECASE)


@dataclass(frozen=True)
class SearchNode:
    """One ``searchXXX`` entry from ``[SharedFile]``.

    kind     -- "tree" | "toc" | "path"
    priority -- the trailing-integer priority (HIGHER is searched FIRST)
    abspath  -- the (quote-stripped) cfg value (a path; NOT validated for existence here)
    raw_key  -- the full original key name, retained for provenance
    sku      -- parsed NN segment (informational; SKU gating is ignored, RESEARCH Open-Q2);
                -1 sentinel for bare-priority legacy keys (no `_NN_` segment)
    cfg_seq  -- discovery index in the cfg, used ONLY to break ties WITHIN the same
                (priority, kind) group (matches same-kind repeated-key registration order)
    """

    kind: str
    priority: int
    abspath: str
    raw_key: str
    sku: int
    cfg_seq: int


@dataclass(frozen=True)
class ScanResult:
    """Result of parsing a ``[SharedFile]`` section.

    nodes               -- engine-faithful sorted SearchNode list (-priority, kind_rank, cfg_seq)
    max_search_priority -- the cfg's ``maxSearchPriority`` scalar (provenance; the engine's
                           priority-loop upper bound). The scanner emits only the keys present;
                           it does NOT synthesize missing-priority nodes.
    toc_tree_path       -- the cfg's ``TOCTreePath`` scalar (trailing slash kept). This is the
                           dir prefix the ENGINE prepends to `.tre` names listed INSIDE a `.toc`
                           when it later opens those payload `.tre`s. CONFIRMED against the
                           vendored parser: read_search_toc_entries / read_cot2000_entries resolve
                           each entry's archive from the master index's OWN embedded tree-file
                           name block + per-entry tree_index — TOCTreePath is NOT consulted to
                           read `.toc` ENTRIES. Phase 28 reads TOC metadata only (Assumption A3),
                           so this is informational here; Phase 29 must thread it if it ever
                           resolves `.toc`-referenced payload `.tre`s.
    """

    nodes: list[SearchNode] = field(default_factory=list)
    max_search_priority: int | None = None
    toc_tree_path: str | None = None


def _strip_quotes(value: str) -> str:
    """Strip a single surrounding pair of double quotes, if both ends are ``"``."""
    v = value.strip()
    if len(v) >= 2 and v[0] == '"' and v[-1] == '"':
        return v[1:-1]
    return v


def parse_shared_file(cfg_path: str | Path) -> ScanResult:
    """Hand-parse the ``[SharedFile]`` section of *cfg_path* into a :class:`ScanResult`.

    *cfg_path* is REQUIRED (D-08) — there is no default cfg location.

    The returned ``nodes`` list is sorted ENGINE-FAITHFULLY by
    ``(-priority, KIND_RANK[kind], cfg_seq)`` (review finding #1). The scanner is a pure
    cfg -> node-list transform: it does NOT validate that the abspaths exist (that is the
    merge's job and the integration test's concern).
    """
    text = Path(cfg_path).read_text(encoding="utf-8", errors="replace")

    in_section = False
    parsed_nodes: list[SearchNode] = []
    max_search_priority: int | None = None
    toc_tree_path: str | None = None
    cfg_seq = 0  # discovery index, bumped per appended search node

    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line:
            continue
        if line[0] == "#":  # comment
            continue
        if line.startswith("[") and line.endswith("]"):  # section header toggles state
            in_section = line[1:-1].strip().lower() == "sharedfile"
            continue
        if not in_section:
            continue
        if "=" not in line:
            continue

        key, _, value = line.partition("=")
        key = key.strip()
        value = _strip_quotes(value)

        if key.lower() == "maxsearchpriority":
            try:
                max_search_priority = int(value.strip())
            except ValueError:
                pass  # malformed scalar — ignore (Pitfall: do not crash on bad lines)
            continue
        if key.lower() == "toctreepath":
            toc_tree_path = value  # keep trailing slash
            continue

        m = _RE_SKU_KEY.match(key)
        if m:
            kind = m.group(1).lower()
            sku = int(m.group(2))
            priority = int(m.group(3))  # FINAL trailing integer (Pitfall 5)
            parsed_nodes.append(
                SearchNode(kind=kind, priority=priority, abspath=value, raw_key=key, sku=sku, cfg_seq=cfg_seq)
            )
            cfg_seq += 1
            continue

        m = _RE_BARE_KEY.match(key)
        if m:
            # Bare-priority legacy form (empty skuText, TreeFile.cpp:123), review finding #6.
            kind = m.group(1).lower()
            priority = int(m.group(2))  # trailing integer
            parsed_nodes.append(
                SearchNode(kind=kind, priority=priority, abspath=value, raw_key=key, sku=-1, cfg_seq=cfg_seq)
            )
            cfg_seq += 1
            continue
        # Any other key inside [SharedFile] is ignored (not a search node, not a scalar).

    # ENGINE-FAITHFUL SORT (review finding #1) — NOT a plain stable-by-priority sort.
    nodes = sorted(parsed_nodes, key=lambda n: (-n.priority, KIND_RANK[n.kind], n.cfg_seq))

    return ScanResult(
        nodes=nodes,
        max_search_priority=max_search_priority,
        toc_tree_path=toc_tree_path,
    )
