# 2026-06-11: build //hlsl PSRC overrides for the ps.1.1 asm emissive
# pixel shaders (emismap.psh, emis_full.psh, a_emis.psh) -- the texren
# recipe: original IFF with the PSRC chunk swapped to a //hlsl reauthor,
# PEXE preserved byte-for-byte so D3D9 is unaffected.
#
# IFF layout (u32 BIG-endian sizes; FORM size counts the 4-char type +
# children): FORM PSHP { FORM 0000 { PSRC <text> , PEXE <bytecode> } }
# Chunks are WALKED (never find(b'PEXE') -- it can match comment text).
import struct, os

SRC_DIR = r"D:\Code\swg-main\serverdata\pixel_program"
OUT_DIR = r"D:\Code\swg-client-v2\stage\override\pixel_program"

HLSL = {}

# emismap.psh: mul r0, t0, v0 ; lrp r0.rgb, t0.a, t0, r0 ; +mov r0.a, c[alphaFadeOpacity]
# = lit texture with the emissive-masked unlit texture lerped back on top
# (mask = MAIN alpha). Mirrors the official a_emismap_ps11.psh //hlsl.
HLSL["emismap.psh"] = """//hlsl ps_1_1

#include "pixel_program/include/pixel_shader_constants.inc"
#include "pixel_program/include/functions.inc"

sampler diffuseMap : register(s0);

float4 main
(
	in float4 vertexDiffuse   : COLOR0,
	in float2 tcs_MAIN	: TEXCOORD0
)
: COLOR
{
	float4 result;

	float4 sample = tex2D(diffuseMap, tcs_MAIN);
	float3 litTexture = sample.rgb * vertexDiffuse.rgb;

	// lerp unlit texture back on top of lighting with the emissive mask
	result.rgb = lerp(litTexture, sample.rgb, sample.a);
	result.a = alphaFadeOpacity;

	return result;
}
"""

# emis_full.psh: mov r0.rgb, t0 ; +mul r0.a, t0.a, c[alphaFadeOpacity]
HLSL["emis_full.psh"] = """//hlsl ps_1_1

#include "pixel_program/include/pixel_shader_constants.inc"
#include "pixel_program/include/functions.inc"

sampler diffuseMap : register(s0);

float4 main
(
	in float2 tcs_MAIN	: TEXCOORD0
)
: COLOR
{
	float4 result;

	float4 sample = tex2D(diffuseMap, tcs_MAIN);
	result.rgb = sample.rgb;
	result.a = sample.a * alphaFadeOpacity;

	return result;
}
"""

# a_emis.psh: mov r0.rgb, t0 ; +mov r0.a, c[alphaFadeOpacity]
HLSL["a_emis.psh"] = """//hlsl ps_1_1

#include "pixel_program/include/pixel_shader_constants.inc"
#include "pixel_program/include/functions.inc"

sampler diffuseMap : register(s0);

float4 main
(
	in float2 tcs_MAIN	: TEXCOORD0
)
: COLOR
{
	float4 result;

	result.rgb = tex2D(diffuseMap, tcs_MAIN).rgb;
	result.a = alphaFadeOpacity;

	return result;
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
