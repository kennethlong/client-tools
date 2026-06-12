//hlsl vs_2_0

// ======================================================================
// Phase 19 //hlsl reauthoring of //asm tfcl_2uv.vsh (tfcl + a second
// detail UV set). asm: oT0=MAIN, oT1=DETA. Diffuse math identical to tfcl.
// The PS-fallback only samples t0, so the DETA (detail) stage isn't blended
// yet -- base texture * vertex lighting is correct; detail blend is a
// follow-up that needs a multi-texture PS-fallback. See tfcl.vsh preamble.
// ======================================================================

#define textureCoordinateSetMAIN	textureCoordinateSet0
#define textureCoordinateSetDETA	textureCoordinateSet1
#define DECLARE_textureCoordinateSets	\
	float2 textureCoordinateSet0 : TEXCOORD0 : register(v7); \
	float2 textureCoordinateSet1 : TEXCOORD1 : register(v8);

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
	float2  tcs_DETA : TEXCOORD1;
};

OutputVertex main(InputVertex inputVertex)
{
	OutputVertex outputVertex;

	outputVertex.position = transform3d(inputVertex.position);
	outputVertex.fog      = calculateFog(inputVertex.position);
	outputVertex.tcs_MAIN = inputVertex.textureCoordinateSetMAIN;
	outputVertex.tcs_DETA = inputVertex.textureCoordinateSetDETA;

	outputVertex.diffuse  = saturate(inputVertex.color0 + calculateDiffuseLighting(false, inputVertex.position, inputVertex.normal_o.xyz));

	return outputVertex;
}
