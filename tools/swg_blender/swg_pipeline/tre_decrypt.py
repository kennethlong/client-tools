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
