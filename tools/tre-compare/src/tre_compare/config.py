"""Installs picker config loader for tre-compare (Phase-29 Plan 03 / D-02).

A SMALL, stateless loader — NOT a registry. ``GET /installs`` reads a tool-local
``installs.toml`` (a stdlib ``tomllib`` ``[[installs]]`` array-of-tables of
``name`` / ``cfg_path``) and returns a frozen :class:`InstallEntry` list the SPA's
install picker offers. The real ``installs.toml`` carries machine-specific absolute
install paths and is GITIGNORED unconditionally (REVIEWS also-agreed — a real
acceptance criterion); only ``installs.toml.example`` is tracked.

Tolerance mirrors the scanner's ``parse_shared_file`` (pure file -> frozen-dataclass,
tolerant of malformed rows): a MISSING config file returns ``[]`` (an empty picker, not
an error) and a malformed / field-missing entry is SKIPPED rather than crashing the
whole load. Stdlib only (``tomllib`` + ``pathlib``); NO ``fastapi`` import.
"""

from __future__ import annotations

import tomllib
from dataclasses import dataclass
from pathlib import Path

# Tool-local default config: tools/tre-compare/installs.toml  (config.py is at
# tools/tre-compare/src/tre_compare/config.py -> parents: tre_compare, src, tre-compare).
DEFAULT_INSTALLS_PATH = Path(__file__).resolve().parent.parent.parent / "installs.toml"


@dataclass(frozen=True)
class InstallEntry:
    """One picker entry: a human ``name`` + the ``cfg_path`` its client.cfg lives at."""

    name: str
    cfg_path: str


def load_installs(config_path: str | Path | None = None) -> list[InstallEntry]:
    """Load the ``[[installs]]`` array from *config_path* into :class:`InstallEntry` rows.

    *config_path* defaults to :data:`DEFAULT_INSTALLS_PATH` (the tool-local
    ``installs.toml``). A non-existent path returns ``[]`` (empty picker, not an error).
    Each ``[[installs]]`` table needs a string ``name`` AND ``cfg_path``; a row missing
    either (or with a non-string value) is SKIPPED rather than crashing the load. A
    malformed TOML document also degrades to ``[]`` (the picker simply has no entries).
    """
    path = Path(config_path) if config_path is not None else DEFAULT_INSTALLS_PATH
    if not path.is_file():
        return []
    try:
        with open(path, "rb") as fh:
            data = tomllib.load(fh)
    except (tomllib.TOMLDecodeError, OSError):
        return []

    out: list[InstallEntry] = []
    for row in data.get("installs", []) or []:
        if not isinstance(row, dict):
            continue
        name = row.get("name")
        cfg_path = row.get("cfg_path")
        if not isinstance(name, str) or not isinstance(cfg_path, str):
            continue  # skip a malformed / field-missing entry (scanner-style tolerance)
        if not name or not cfg_path:
            continue
        out.append(InstallEntry(name=name, cfg_path=cfg_path))
    return out
