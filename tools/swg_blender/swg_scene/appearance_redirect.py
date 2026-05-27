"""Appearance template redirect (.apt) — Phase 13."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from swg_iff.reader import IffError, IffReader
from swg_iff.tags import TAG_0000, TAG_APT, TAG_NAME
from swg_iff.writer import make_chunk, make_chunk_string, make_form


@dataclass
class SwgAppearanceRedirect:
    target_path: str


def load_appearance_redirect(path: str | Path) -> SwgAppearanceRedirect:
    return load_appearance_redirect_bytes(Path(path).read_bytes())


def load_appearance_redirect_bytes(data: bytes) -> SwgAppearanceRedirect:
    reader = IffReader(data)
    reader.enter_form(TAG_APT)
    try:
        reader.enter_form(TAG_0000)
        try:
            reader.enter_chunk(TAG_NAME)
            try:
                target = reader.read_string()
            finally:
                reader.exit_chunk(TAG_NAME)
            if not target:
                raise IffError("APT NAME chunk is empty")
            return SwgAppearanceRedirect(target_path=target)
        finally:
            reader.exit_form(TAG_0000)
    finally:
        reader.exit_form(TAG_APT)


def write_appearance_redirect_bytes(spec: SwgAppearanceRedirect) -> bytes:
    body = make_chunk("NAME", make_chunk_string(spec.target_path))
    return make_form("APT", make_form("0000", body))


def write_appearance_redirect(path: str | Path, spec: SwgAppearanceRedirect) -> None:
    Path(path).write_bytes(write_appearance_redirect_bytes(spec))
