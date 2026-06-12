# 2026-06-11: build the //hlsl PSRC override for pixel_program/ui_radar.psh
# (the mini-map radar PS). Original is ps.1.1 asm:
#     tex t0
#     tex t1
#     mul r0.rgb, t0, c[textureFactor]
#     + mov r0.a, t1.a
# t0 = MAIN (terrain radar map, panned/zoomed UVs in TEXCOORD0)
# t1 = ALPH (texture/ui_radar_mask.dds circular mask, static 0..1 UVs in
#      TEXCOORD1 -- see CuiWidgetGroundRadar::Render normal_uvs)
# The mask ALPHA is what makes the radar ROUND. ps.1.1 cannot compile on
# D3D11 -> tombstone -> fallback PS samples only srv0 and drops the mask
# -> square mini-map (open since Phase 11/12).
# Same texren recipe as _build_emis_overrides.py: PSRC swapped to //hlsl,
# PEXE preserved byte-for-byte so D3D9 is unaffected.
# ui_radar.vsh needs NO override: it is already //hlsl and forwards both
# texcoord sets.
import struct, os

SRC_DIR = r"D:\Code\swg-main\serverdata\pixel_program"
OUT_DIR = r"D:\Code\swg-client-v2\stage\override\pixel_program"

HLSL = {}

HLSL["ui_radar.psh"] = """//hlsl ps_2_0

#include "pixel_program/include/pixel_shader_constants.inc"

sampler radarMap : register(s0);
sampler circleMask : register(s1);

float4 main
(
	in float2 tcs_MAIN	: TEXCOORD0,
	in float2 tcs_ALPH	: TEXCOORD1
)
: COLOR
{
	float4 map = tex2D(radarMap, tcs_MAIN);
	float4 mask = tex2D(circleMask, tcs_ALPH);
	return float4(map.rgb * textureFactor.rgb, mask.a);
}
"""


def parse_chunks(data, pos, end):
    """Yield (tag, payload_offset, payload_size) walking sibling chunks."""
    out = []
    while pos + 8 <= end:
        tag = data[pos:pos + 4]
        size = struct.unpack(">I", data[pos + 4:pos + 8])[0]
        out.append((tag, pos + 8, size))
        pos += 8 + size
    return out


def chunk(tag, payload):
    return tag + struct.pack(">I", len(payload)) + payload


def form(formtype, children):
    payload = formtype + b"".join(children)
    return b"FORM" + struct.pack(">I", len(payload)) + payload


for name, hlsl in HLSL.items():
    src = os.path.join(SRC_DIR, name)
    raw = open(src, "rb").read()

    top = parse_chunks(raw, 0, len(raw))
    assert top[0][0] == b"FORM", name
    t_off, t_size = top[0][1], top[0][2]
    assert raw[t_off:t_off + 4] == b"PSHP", name

    lvl1 = parse_chunks(raw, t_off + 4, t_off + t_size)
    assert lvl1[0][0] == b"FORM", name
    v_off, v_size = lvl1[0][1], lvl1[0][2]
    version = raw[v_off:v_off + 4]
    assert version == b"0000", (name, version)

    lvl2 = parse_chunks(raw, v_off + 4, v_off + v_size)
    psrc_old = None
    pexe = None
    for tag, off, size in lvl2:
        if tag == b"PSRC":
            psrc_old = raw[off:off + size]
        elif tag == b"PEXE":
            pexe = raw[off:off + size]
    assert psrc_old is not None and pexe is not None, (name, lvl2)

    # Preserve the original PSRC termination convention (trailing NUL?).
    text = hlsl.replace("\r\n", "\n").encode("ascii")
    if psrc_old.endswith(b"\x00") and not text.endswith(b"\x00"):
        text += b"\x00"

    new = form(b"PSHP", [form(b"0000", [chunk(b"PSRC", text), chunk(b"PEXE", pexe)])])

    out = os.path.join(OUT_DIR, name)
    open(out, "wb").write(new)

    # Verify round-trip: re-parse + confirm PEXE byte-identical.
    chk = parse_chunks(new, 0, len(new))
    c1 = parse_chunks(new, chk[0][1] + 4, chk[0][1] + chk[0][2])
    c2 = parse_chunks(new, c1[0][1] + 4, c1[0][1] + c1[0][2])
    pexe2 = [new[o:o + s] for t, o, s in c2 if t == b"PEXE"][0]
    psrc2 = [new[o:o + s] for t, o, s in c2 if t == b"PSRC"][0]
    assert pexe2 == pexe, name
    print("%s: ok  psrc %d->%d bytes, pexe %d bytes preserved" % (name, len(psrc_old), len(psrc2), len(pexe)))
