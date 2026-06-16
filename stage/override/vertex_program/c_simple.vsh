//hlsl vs_2_0

// ======================================================================
// 2026-06-11 //hlsl reauthoring of the legacy //asm c_simple.vsh -- the
// vertex shader behind effect c_simple / c_emismap (cantina bar counter,
// overhead-light canopy, many simple lit props). The //asm source is
// BYTE-IDENTICAL to tfcl.vsh (same defines, same module list:
// transform + lighting_fog_setup + fog + c_ambient + diffuse, oT0=MAIN),
// so this is the proven tfcl //hlsl reauthor under the c_simple asset
// name. Pre-fix D3D11 routed it to the generic //asm fallback VS
// (const-white COLOR0, no lighting, NO FOG) -> the bar/canopy rendered
// full-bright unfogged (Capture37 ev5737: VS outputs o2=1,1,1,1).
//
// asm mapping: r7 = vColor0 (c_ambient) ; diffuse.inc adds hemispheric
// parallelSpecular[0] + parallel[0..1] + point[0..3] + emissive ; oD0 = r7.
// => oD0 = vColor0 + calculateDiffuseLighting(dot3=false, ...).  The paired
// PS tier is ps.1.1 texture * COLOR0; D3D11 routes it through the PS
// fallback / FFP cascade which consumes COLOR0 + FOG, so authoring the VS
// alone is the complete fix. saturate() matches the D3D9 ps_1_1 clamp.
// ======================================================================

#define textureCoordinateSetMAIN	textureCoordinateSet0
#define DECLARE_textureCoordinateSets	\
	float2 textureCoordinateSet0 : TEXCOORD0;

#include "vertex_program/include/vertex_shader_constants.inc"
#include "vertex_program/include/functions.inc"

struct InputVertex
{
	float4  position : POSITION0 : register(v0);
	float4  normal_o : NORMAL0   : register(v3);
	float4  color0   : COLOR0    : register(v5);
	DECLARE_textureCoordinateSets
};

struct OutputVertex
{
	float4  position : POSITION0;
	float4  diffuse  : COLOR0;
	float   fog      : FOG;
	float2  tcs_MAIN : TEXCOORD0;
};

OutputVertex main(InputVertex inputVertex)
{
	OutputVertex outputVertex;

	outputVertex.position = transform3d(inputVertex.position);
	outputVertex.fog      = calculateFog(inputVertex.position);
	outputVertex.tcs_MAIN = inputVertex.textureCoordinateSetMAIN;

	// cell/ambient term = vertex color (vColor0); add full diffuse lighting
	// (hemispheric + parallel + point + emissive). dot3=false -> hemispheric on.
	outputVertex.diffuse  = saturate(inputVertex.color0 + calculateDiffuseLighting(false, inputVertex.position, inputVertex.normal_o.xyz));

	return outputVertex;
}
