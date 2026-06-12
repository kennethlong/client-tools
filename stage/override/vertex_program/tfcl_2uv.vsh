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

// ----------------------------------------------------------------------
// 2026-06-12 hemispheric-sun fix -- see tfcl.vsh for the full preamble.
// Sun (parallelSpecular[0]) lit hemispherically via cExtLtData c60-63
// (asm diffuse.inc); retail calculateDiffuseLighting's plain max(N.L,0)
// blackened away-facing slopes on D3D11.

float4 extLight_backColor           : register(c60);
float4 extLight_tangentColor        : register(c61);
float4 extLight_tangentMinusBack    : register(c62);
float4 extLight_tangentMinusDiffuse : register(c63);

float4 calculateDiffuseLightingHemi(float4 vertexPosition_o, float3 vertexNormal_o)
{
	float4 result = material.emissiveColor;

	float3 normal_w = normalize(mul(vertexNormal_o, (float3x3)objectWorldMatrix));

#if VERTEX_SHADER_VERSION >= 20
	if (light_parallelSpecular_0_enabled)
#endif
	{
		float d = dot(normal_w, lightData.parallelSpecular[0].direction_w);
		result += extLight_tangentColor
		        - extLight_tangentMinusDiffuse * max(d, 0.0)
		        + extLight_tangentMinusBack    * min(d, 0.0);
	}

#if VERTEX_SHADER_VERSION >= 20
	if (light_parallel_0_enabled)
#endif
		result += calculateDiffuseParallelLight(lightData.parallel[0], vertexNormal_o);

#if VERTEX_SHADER_VERSION >= 20
	if (light_parallel_1_enabled)
#endif
		result += calculateDiffuseParallelLight(lightData.parallel[1], vertexNormal_o);

#if VERTEX_SHADER_VERSION >= 20
	if (light_pointSpecular_0_enabled)
#endif
		result += calculateDiffusePointLight((PointLight)lightData.pointSpecular[0], vertexPosition_o, vertexNormal_o);

#if VERTEX_SHADER_VERSION >= 20
	if (light_point_0_enabled)
#endif
		result += calculateDiffusePointLight(lightData.point[0], vertexPosition_o, vertexNormal_o);

#if VERTEX_SHADER_VERSION >= 20
	if (light_point_1_enabled)
#endif
		result += calculateDiffusePointLight(lightData.point[1], vertexPosition_o, vertexNormal_o);

#if VERTEX_SHADER_VERSION >= 20
	if (light_point_2_enabled)
#endif
		result += calculateDiffusePointLight(lightData.point[2], vertexPosition_o, vertexNormal_o);

#if VERTEX_SHADER_VERSION >= 20
	if (light_point_3_enabled)
#endif
		result += calculateDiffusePointLight(lightData.point[3], vertexPosition_o, vertexNormal_o);

	return result;
}


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

	outputVertex.diffuse  = saturate(inputVertex.color0 + calculateDiffuseLightingHemi(inputVertex.position, inputVertex.normal_o.xyz));

	return outputVertex;
}
