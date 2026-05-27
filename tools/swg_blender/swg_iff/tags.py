"""SWG IFF tag constants — mirrors sharedFoundation/Tag.h and engine loaders."""

from __future__ import annotations

import struct
from typing import Union

Tag = int


def tag(a: str, b: str, c: str, d: str) -> Tag:
    """Build TAG(a,b,c,d) like the C++ macro (four ASCII chars, big-endian uint32)."""
    return struct.unpack(">I", (a + b + c + d).encode("ascii"))[0]


def tag3(a: str, b: str, c: str) -> Tag:
    """TAG3 — fourth byte is space."""
    return tag(a, b, c, " ")


def tag_to_str(value: Tag) -> str:
    """Convert tag to 4-char string (non-printable → '?')."""
    raw = struct.pack(">I", value & 0xFFFFFFFF)
    chars = []
    for b in raw:
        chars.append(chr(b) if 32 <= b < 127 else "?")
    return "".join(chars)


def tag_from_str(s: str) -> Tag:
    """Convert up-to-4-char string to tag (padded with spaces)."""
    s = (s + "    ")[:4]
    return struct.unpack(">I", s.encode("ascii"))[0]


# Generic (Tag.h)
TAG_FORM = tag("F", "O", "R", "M")
TAG_INFO = tag("I", "N", "F", "O")
TAG_DATA = tag("D", "A", "T", "A")
TAG_NAME = tag("N", "A", "M", "E")
TAG_0000 = tag("0", "0", "0", "0")
TAG_0001 = tag("0", "0", "0", "1")
TAG_0002 = tag("0", "0", "0", "2")
TAG_0003 = tag("0", "0", "0", "3")
TAG_0004 = tag("0", "0", "0", "4")
TAG_0005 = tag("0", "0", "0", "5")
TAG_0006 = tag("0", "0", "0", "6")
TAG_0008 = tag("0", "0", "0", "8")

# Collision / floor / extents (Phase 12)
TAG_FLOR = tag("F", "L", "O", "R")
TAG_VERT = tag("V", "E", "R", "T")
TAG_TRIS = tag("T", "R", "I", "S")
TAG_BTRE = tag("B", "T", "R", "E")
TAG_BEDG = tag("B", "E", "D", "G")
TAG_PGRF = tag("P", "G", "R", "F")
TAG_IDTL = tag("I", "D", "T", "L")
TAG_CMSH = tag("C", "M", "S", "H")
TAG_EXBX = tag("E", "X", "B", "X")
TAG_EXSP = tag("E", "X", "S", "P")
TAG_XCYL = tag("X", "C", "Y", "L")
TAG_NULL = tag("N", "U", "L", "L")
TAG_BOX = tag("B", "O", "X", " ")
TAG_CYLN = tag("C", "Y", "L", "N")

# Portal property / building (Phase 13)
TAG_PRTO = tag("P", "R", "T", "O")
TAG_PRTS = tag("P", "R", "T", "S")
TAG_CELS = tag("C", "E", "L", "S")
TAG_PRTL = tag("P", "R", "T", "L")
TAG_CELL = tag("C", "E", "L", "L")
TAG_CRC = tag3("C", "R", "C")
TAG_APT = tag3("A", "P", "T")
TAG_LGHT = tag("L", "G", "H", "T")

# Appearances
TAG_APPR = tag("A", "P", "P", "R")
TAG_MESH = tag("M", "E", "S", "H")
TAG_SMAT = tag("S", "M", "A", "T")
TAG_MSGN = tag("M", "S", "G", "N")
TAG_SKTI = tag("S", "K", "T", "I")
TAG_LATX = tag("L", "A", "T", "X")
TAG_SFSK = tag("S", "F", "S", "K")
TAG_APAG = tag("A", "P", "A", "G")
TAG_LDTB = tag("L", "D", "T", "B")
TAG_CMPA = tag("C", "M", "P", "A")
TAG_PART = tag("P", "A", "R", "T")
TAG_DTLA = tag("D", "T", "L", "A")
TAG_CHLD = tag("C", "H", "L", "D")
TAG_RADR = tag("R", "A", "D", "R")
TAG_PIVT = tag("P", "I", "V", "T")
TAG_TEST = tag("T", "E", "S", "T")
TAG_WRIT = tag("W", "R", "I", "T")
TAG_CNTR = tag("C", "N", "T", "R")
TAG_RADI = tag("R", "A", "D", "I")
TAG_SPHR = tag("S", "P", "H", "R")

# Shader primitive set
TAG_SPS = tag3("S", "P", "S")
TAG_CNT = tag3("C", "N", "T")
TAG_INDX = tag("I", "N", "D", "X")
TAG_SIDX = tag("S", "I", "D", "X")

# Vertex buffer
TAG_VTXA = tag("V", "T", "X", "A")

# Shaders
TAG_SSHT = tag("S", "S", "H", "T")
TAG_CSHD = tag("C", "S", "H", "D")
TAG_SWTS = tag("S", "W", "T", "S")
TAG_OPST = tag("O", "P", "S", "T")
TAG_CUST = tag("C", "U", "S", "T")
TAG_TFAC = tag("T", "F", "A", "C")
TAG_TXTR = tag("T", "X", "T", "R")
TAG_TEXT = tag("T", "E", "X", "T")
TAG_PAL = tag3("P", "A", "L")
TAG_TX1D = tag("T", "X", "1", "D")
TAG_DRTS = tag("D", "R", "T", "S")
TAG_DPPT = tag("D", "P", "P", "T")
TAG_DRFT = tag("D", "R", "F", "T")
TAG_MAIN = tag("M", "A", "I", "N")
TAG_NRML = tag("N", "R", "M", "L")

# Skeletal mesh generator
TAG_SKMG = tag("S", "K", "M", "G")
TAG_SKTM = tag("S", "K", "T", "M")
TAG_MLOD = tag("M", "L", "O", "D")
TAG_SLOD = tag("S", "L", "O", "D")
TAG_KFAT = tag("K", "F", "A", "T")
TAG_XFNM = tag("X", "F", "N", "M")
TAG_POSN = tag("P", "O", "S", "N")
TAG_TWHD = tag("T", "W", "H", "D")
TAG_TWDT = tag("T", "W", "D", "T")
TAG_NORM = tag("N", "O", "R", "M")
TAG_DOT3 = tag("D", "O", "T", "3")
TAG_PSDT = tag("P", "S", "D", "T")
TAG_PIDX = tag("P", "I", "D", "X")
TAG_NIDX = tag("N", "I", "D", "X")
TAG_TXCI = tag("T", "X", "C", "I")
TAG_TCSF = tag("T", "C", "S", "F")
TAG_TCSD = tag("T", "C", "S", "D")
TAG_PRIM = tag("P", "R", "I", "M")
TAG_ITL = tag("I", "T", "L", " ")
TAG_OITL = tag("O", "I", "T", "L")
TAG_VDCL = tag("V", "D", "C", "L")
TAG_OZN = tag("O", "Z", "N", " ")
TAG_BLT = tag3("B", "L", "T")
TAG_BLTS = tag("B", "L", "T", "S")
TAG_HPTS = tag("H", "P", "T", "S")
TAG_STAT = tag("S", "T", "A", "T")
TAG_DYN = tag3("D", "Y", "N")

TAG_FOZC = tag("F", "O", "Z", "C")
TAG_OZC = tag("O", "Z", "C", " ")
TAG_ZTO = tag("Z", "T", "O", " ")

# Skeleton template
TAG_PRNT = tag("P", "R", "N", "T")
TAG_RPRE = tag("R", "P", "R", "E")
TAG_RPST = tag("R", "P", "S", "T")
TAG_BPTR = tag("B", "P", "T", "R")
TAG_BPMJ = tag("B", "P", "M", "J")
TAG_BPRO = tag("B", "P", "R", "O")
TAG_JROR = tag("J", "R", "O", "R")


# Keyframe skeletal animation (KFAT)
TAG_XFRM = tag("X", "F", "R", "M")
TAG_XFIN = tag("X", "F", "I", "N")
TAG_AROT = tag("A", "R", "O", "T")
TAG_ATRN = tag("A", "T", "R", "N")
TAG_QCHN = tag("Q", "C", "H", "N")
TAG_CHNL = tag("C", "H", "N", "L")
TAG_SROT = tag("S", "R", "O", "T")
TAG_STRN = tag("S", "T", "R", "N")
TAG_MESG = tag("M", "E", "S", "G")
TAG_MSGS = tag("M", "S", "G", "S")
TAG_LOCR = tag("L", "O", "C", "R")
TAG_LOCT = tag("L", "O", "C", "T")
TAG_CKAT = tag("C", "K", "A", "T")

# Tree archive
TAG_TREE = tag("T", "R", "E", "E")
TAG_TOC = tag3("T", "O", "C")

KNOWN_ROOT_FORMS: dict[Tag, str] = {
    TAG_MESH: "MeshAppearance",
    TAG_SMAT: "SkeletalAppearance",
    TAG_CMPA: "ComponentAppearance",
    TAG_DTLA: "DetailAppearance",
    TAG_SSHT: "StaticShaderTemplate",
    TAG_SKMG: "SkeletalMeshGenerator",
    TAG_KFAT: "KeyframeSkeletalAnimation",
    TAG_APPR: "AppearanceTemplate",
}
