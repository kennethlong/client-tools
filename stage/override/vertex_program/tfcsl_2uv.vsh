//hlsl vs_2_0

// ======================================================================
// Phase 19 //hlsl reauthoring of //asm tfcsl_2uv.vsh. asm duplicates MAIN to
// both oT0 and oT1 (a multi-stage modulate of the same UV). Lighting math
// identical to tfcsl. PS-fallback samples only t0; the second stage isn't
// modulated yet (multi-texture PS-fallback follow-up). See tfcsl.vsh preamble.
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
	float4  position  : POSITION0;
	float4  diffuse   : COLOR0;
	float4  specular  : COLOR1;
	float   fog       : FOG;
	float2  tcs_MAIN0 : TEXCOORD0;
	float2  tcs_MAIN1 : TEXCOORD1;
};

OutputVertex main(InputVertex inputVertex)
{
	OutputVertex outputVertex;

	outputVertex.position  = transform3d(inputVertex.position);
	outputVertex.fog       = calculateFog(inputVertex.position);
	outputVertex.tcs_MAIN0 = inputVertex.textureCoordinateSetMAIN;
	outputVertex.tcs_MAIN1 = inputVertex.textureCoordinateSetMAIN;   // asm: oT1 = MAIN (duplicate)

	DiffuseSpecular ds = calculateDiffuseSpecularLighting(false, inputVertex.position, inputVertex.normal_o.xyz);
	outputVertex.diffuse  = saturate(inputVertex.color0 + ds.diffuse);
	outputVertex.specular = saturate(ds.specular * material.specularColor);

	return outputVertex;
}
