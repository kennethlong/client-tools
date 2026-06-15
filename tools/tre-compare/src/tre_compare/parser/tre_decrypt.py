# Vendored 2026-06-14 from swg-blender-plugin
#   source: D:/Code/swg-blender-plugin/swg_pipeline/tre_decrypt.py
#   commit: f803f58782e675e85844960fe868b0849405249a (master)
# Copied per Phase-28 D-03; owned and may diverge. No live link back.
"""TRE payload helpers — zlib decompress + detection of non-zlib (encrypted) payloads.

NOTE: Restoration's TRE payloads are encrypted with a PROPRIETARY scheme. As of 2026-06-15 this
project does NOT support decrypting them and will not implement decryption — we only detect and
flag encrypted payloads, never read their contents. Open installs (SWGSource / SWGEmu / Infinity)
use standard zlib and read normally.
"""

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
    """Try zlib decompress; flag non-zlib payloads as encrypted (unsupported, never decrypted)."""
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
                "Restoration's encryption is proprietary and is NOT supported by this project "
                "(decryption intentionally not implemented, 2026-06-15)."
            ),
        )


RESTORATION_VERSIONS = frozenset({"6000", "0006", "5000"})
