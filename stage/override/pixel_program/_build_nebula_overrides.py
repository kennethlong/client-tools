# 2026-06-26: //hlsl PSRC overrides for the ps.1.1 asm nebula-gas pixel
# shaders (e_nebula_emisadd.psh, a_modulate.psh). Same recipe as
# _build_emis_overrides.py: original IFF with the PSRC chunk swapped to a
# //hlsl reauthor, PEXE preserved byte-for-byte so D3D9 is unaffected. These
# were never reauthored -> gl11 asset-PS pipeline can't compile ps.1.1 asm ->
# magenta fallback PS on the space nebula cloud (screenShot0407/0408,
# shader/pt_nebulae_gas_4_2.sht -> effect/e_nebula_emisadd.eft -> this PS).
# Originals sourced from the loaded TREs (patch_11_00 / ILM_visuals) -- they are
# NOT in swg-main serverdata, so we read them via the tre_reader.
import struct, os, sys, glob
sys.path.insert(0, 'D:/Code/swg-blender-plugin')
from swg_pipeline import tre_reader as tr

DATA = r"D:/Code/SWGSource Client v3.0"
OUT_DIR = r"D:/Code/swg-client-v2/stage/override/pixel_program"

HLSL = {}

# e_nebula_emisadd.psh ps.1.1:
#   tex t0 / mov r0,t0 / mul r0.rgb,t0,v0 / mul r0,r0,v0.a
#   => rgb = tex.rgb * vtxColor.rgb * vtxColor.a ; a = tex.a * vtxColor.a
HLSL["e_nebula_emisadd.psh"] = """//hlsl ps_1_1

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

	// ps.1.1: mov r0,t0 / mul r0.rgb,t0,v0 / mul r0,r0,v0.a
	result.rgb = sample.rgb * vertexDiffuse.rgb * vertexDiffuse.a;
	result.a   = sample.a * vertexDiffuse.a;

	return result;
}
"""

# a_modulate.psh ps.1.1:
#   tex t0 / mul r0,t0,v0 / mul r0.a,r0.a,c[alphaFadeOpacity]
#   => rgb = tex.rgb * vtxColor.rgb ; a = tex.a * vtxColor.a * alphaFadeOpacity
HLSL["a_modulate.psh"] = """//hlsl ps_1_1

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

	// ps.1.1: mul r0,t0,v0 / mul r0.a,r0.a,c[alphaFadeOpacity]
	result.rgb = sample.rgb * vertexDiffuse.rgb;
	result.a   = sample.a * vertexDiffuse.a * alphaFadeOpacity;

	return result;
}
"""


def parse_chunks(data, pos, end):
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


def norm(n):
    return n.replace(chr(92), '/').lower()

# locate originals in the TREs
loc = {}
for t in sorted(glob.glob(os.path.join(DATA, "*.tre"))):
    try:
        for n in tr.list_tre(t):
            loc.setdefault(norm(n), (t, n))
    except Exception:
        pass

for name, hlsl in HLSL.items():
    key = "pixel_program/" + name
    raw = tr.read_tre_payload(*loc[key])

    top = parse_chunks(raw, 0, len(raw))
    assert top[0][0] == b"FORM" and raw[top[0][1]:top[0][1] + 4] == b"PSHP", name
    lvl1 = parse_chunks(raw, top[0][1] + 4, top[0][1] + top[0][2])
    assert lvl1[0][0] == b"FORM" and raw[lvl1[0][1]:lvl1[0][1] + 4] == b"0000", name
    lvl2 = parse_chunks(raw, lvl1[0][1] + 4, lvl1[0][1] + lvl1[0][2])
    psrc_old = pexe = None
    for tag, off, size in lvl2:
        if tag == b"PSRC": psrc_old = raw[off:off + size]
        elif tag == b"PEXE": pexe = raw[off:off + size]
    assert psrc_old is not None and pexe is not None, (name, lvl2)

    text = hlsl.replace("\r\n", "\n").encode("ascii")
    if psrc_old.endswith(b"\x00") and not text.endswith(b"\x00"):
        text += b"\x00"

    new = form(b"PSHP", [form(b"0000", [chunk(b"PSRC", text), chunk(b"PEXE", pexe)])])
    open(os.path.join(OUT_DIR, name), "wb").write(new)

    # round-trip verify PEXE preserved
    c0 = parse_chunks(new, 0, len(new))
    c1 = parse_chunks(new, c0[0][1] + 4, c0[0][1] + c0[0][2])
    c2 = parse_chunks(new, c1[0][1] + 4, c1[0][1] + c1[0][2])
    pexe2 = [new[o:o + s] for t, o, s in c2 if t == b"PEXE"][0]
    assert pexe2 == pexe, name
    print("%s: ok  psrc %d->%d bytes, pexe %d preserved" % (name, len(psrc_old), len(text), len(pexe)))
