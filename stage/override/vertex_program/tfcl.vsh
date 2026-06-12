//hlsl vs_2_0

// ======================================================================
// Phase 19 //hlsl reauthoring of the legacy //asm tfcl.vsh
// (Transform + Fog + Cell color + diffuse Lighting), the vs_2_0 fallback
// tier of effects like c_alpha / c_emismap. D3D11 can't compile D3D9 vertex
// asm; the generic VS fallback drew these flat/over-bright (const-white
// COLOR0). This restores correct per-vertex Gouraud lighting by mirroring the
// asm module math (modules/{transform,fog,c_ambient,diffuse}.inc) with the
// existing //hlsl helpers in include/functions.inc.
//
// asm mapping: r7 = vColor0 (c_ambient) ; diffuse.inc adds hemispheric
// parallelSpecular[0] + parallel[0..1] + point[0..3] + emissive ; oD0 = r7.
// => oD0 = vColor0 + calculateDiffuseLighting(dot3=false, ...).  The paired
// tfcl-tier PS (a_simple.psh etc.) is ps.1.1 `mul r0.rgb, t0, v0` = texture *
// COLOR0; D3D11 routes it through the PS-fallback which emits the identical
// t0.Sample * COLOR0, so authoring the VS alone is the complete fix.
// saturate() matches the D3D9 ps_1_1 color-register [0,1] clamp.
// ======================================================================

#define textureCoordinateSetMAIN	textureCoordinateSet0
#define DECLARE_textureCoordinateSets	\
	float2 textureCoordinateSet0 : TEXCOORD0 : register(v7);

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
