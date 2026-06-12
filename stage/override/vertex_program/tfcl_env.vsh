//hlsl vs_2_0

// ======================================================================
// Phase 19 //hlsl reauthoring of //asm tfcl_env.vsh (tfcl + environment
// reflection on oT1, used by c_envmask glass/metal). Diffuse math identical
// to tfcl. The asm env_t1.inc emits oT1 = reflect of the world viewer about
// the normal for a cube/sphere env lookup -- but the current PS-fallback
// ignores TEXCOORD1 and has no env sampler, so the reflection is DEFERRED
// (would render identically). When an env-aware PS-fallback exists, add:
//   float3 viewer_w = calculateViewerDirection_w(position.xyz);
//   oT1 = normal_o*(2*dot(normal_o,viewer_w)) - viewer_w;
// Until then this draws correctly-lit base texture (glass loses reflection,
// same as the generic fallback did). See tfcl.vsh preamble.
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

	outputVertex.diffuse  = saturate(inputVertex.color0 + calculateDiffuseLighting(false, inputVertex.position, inputVertex.normal_o.xyz));

	return outputVertex;
}
