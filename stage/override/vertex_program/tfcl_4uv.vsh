//hlsl vs_2_0

// ======================================================================
// Phase 19 //hlsl reauthoring of //asm tfcl_4uv.vsh (tfcl + 4 UV sets for
// 2-blend dirt/dodge effects). asm: oT0=DTLA, oT1=DTLB, oT2=MASK, oT3=DIRT
// (declared in that order -> drives the texcoord-set key). Diffuse math
// identical to tfcl. PS-fallback samples only t0 (DTLA); the DTLB/MASK/DIRT
// blend needs a multi-texture PS-fallback (follow-up). Base texture * vertex
// lighting is correct. See tfcl.vsh preamble.
// ======================================================================

#define textureCoordinateSetDTLA	textureCoordinateSet0
#define textureCoordinateSetDTLB	textureCoordinateSet1
#define textureCoordinateSetMASK	textureCoordinateSet2
#define textureCoordinateSetDIRT	textureCoordinateSet3
#define DECLARE_textureCoordinateSets	\
	float2 textureCoordinateSet0 : TEXCOORD0; \
	float2 textureCoordinateSet1 : TEXCOORD1; \
	float2 textureCoordinateSet2 : TEXCOORD2; \
	float2 textureCoordinateSet3 : TEXCOORD3;

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
	float2  tcs_DTLA : TEXCOORD0;
	float2  tcs_DTLB : TEXCOORD1;
	float2  tcs_MASK : TEXCOORD2;
	float2  tcs_DIRT : TEXCOORD3;
};

OutputVertex main(InputVertex inputVertex)
{
	OutputVertex outputVertex;

	outputVertex.position = transform3d(inputVertex.position);
	outputVertex.fog      = calculateFog(inputVertex.position);
	outputVertex.tcs_DTLA = inputVertex.textureCoordinateSetDTLA;
	outputVertex.tcs_DTLB = inputVertex.textureCoordinateSetDTLB;
	outputVertex.tcs_MASK = inputVertex.textureCoordinateSetMASK;
	outputVertex.tcs_DIRT = inputVertex.textureCoordinateSetDIRT;

	outputVertex.diffuse  = saturate(inputVertex.color0 + calculateDiffuseLightingHemi(inputVertex.position, inputVertex.normal_o.xyz));

	return outputVertex;
}
