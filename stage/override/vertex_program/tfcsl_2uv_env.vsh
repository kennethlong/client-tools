//hlsl vs_2_0

// ======================================================================
// Phase 19 //hlsl reauthoring of //asm tfcsl_2uv_env.vsh. asm: oT0=MAIN then
// env_t1.inc -> oT1 = reflection vector. Lighting math identical to tfcsl.
// As with tfcl_env, the env reflection (oT1) is DEFERRED until an env-aware
// PS-fallback exists (PS-fallback ignores TEXCOORD1, no env sampler). Draws
// correctly-lit base texture. See tfcsl.vsh + tfcl_env.vsh preambles.
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
	float4  specular : COLOR1;
	float   fog      : FOG;
	float2  tcs_MAIN : TEXCOORD0;
};

OutputVertex main(InputVertex inputVertex)
{
	OutputVertex outputVertex;

	outputVertex.position = transform3d(inputVertex.position);
	outputVertex.fog      = calculateFog(inputVertex.position);
	outputVertex.tcs_MAIN = inputVertex.textureCoordinateSetMAIN;

	DiffuseSpecular ds = calculateDiffuseSpecularLighting(false, inputVertex.position, inputVertex.normal_o.xyz);
	outputVertex.diffuse  = saturate(inputVertex.color0 + ds.diffuse);
	outputVertex.specular = saturate(ds.specular * material.specularColor);

	return outputVertex;
}
