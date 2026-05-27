"""Building export helpers — assemble POB sidecars (Phase 13)."""

from __future__ import annotations

from pathlib import Path

from swg_scene.appearance_redirect import (
    SwgAppearanceRedirect,
    write_appearance_redirect,
)
from swg_scene.portal_property import SwgPortalProperty, write_portal_property


def export_pob(path: str | Path, spec: SwgPortalProperty) -> Path:
    """Write a portal property (.pob) file."""
    out = Path(path)
    write_portal_property(out, spec)
    return out


def export_apt(path: str | Path, target_appearance: str) -> Path:
    """Write an appearance redirect (.apt) pointing at ``target_appearance``."""
    out = Path(path)
    write_appearance_redirect(out, SwgAppearanceRedirect(target_path=target_appearance))
    return out
