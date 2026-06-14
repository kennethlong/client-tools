# Vendored 2026-06-14 from swg-blender-plugin
#   source: D:/Code/swg-blender-plugin/swg_pipeline/tre_decrypt.py
#   commit: f803f58782e675e85844960fe868b0849405249a (master)
# Copied per Phase-28 D-03; owned and may diverge. No live link back.
"""TRE payload helpers — Restoration v6000 decrypt research (Phase 14)."""

from __future__ import annotations

import zlib
from dataclasses import dataclass


@dataclass(frozen=True)
class PayloadReadResult:
    ok: bool
    data: bytes
    method: str
    note: str = ""


def try_read_tre_payload(raw: bytes, compressor: int, *, version: str = "") -> PayloadReadResult:
    """Try zlib decompress; Restoration v6000 may need TreeFileExtractor."""
    if not compressor:
        return PayloadReadResult(True, raw, "stored")
    try:
        return PayloadReadResult(True, zlib.decompress(raw), "zlib")
    except zlib.error:
        return PayloadReadResult(
            False,
            raw,
            "encrypted_or_unknown",
            note=(
                f"TRE {version!r} payload did not zlib-decompress (compressor={compressor}). "
                "Use TreeFileExtractor.exe for Restoration archives."
            ),
        )


RESTORATION_VERSIONS = frozenset({"6000", "0006", "5000"})
