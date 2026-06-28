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
// parallelSpecular[0] + parallel[0..1] + point_id[0..3] + emissive ; oD0 = r7.
// => oD0 = vColor0 + calculateDiffuseLighting(dot3=false, ...).  The paired
// tfcl-tier PS (a_simple.psh etc.) is ps.1.1 `mul r0.rgb, t0, v0` = texture *
// COLOR0; D3D11 routes it through the PS-fallback which emits the identical
// t0.Sample * COLOR0, so authoring the VS alone is the complete fix.
// saturate() matches the D3D9 ps_1_1 color-register [0,1] clamp.
// ======================================================================

#define textureCoordinateSetMAIN	textureCoordinateSet0
#define DECLARE_textureCoordinateSets	\
	float2 textureCoordinateSet0 : TEXCOORD0;

#include "vertex_program/include/vertex_shader_constants.inc"
#include "vertex_program/include/functions.inc"

// ----------------------------------------------------------------------
// 2026-06-12 hemispheric-sun fix (Mos Eisley plaza A/B screenShot0365/0366):
// the asm diffuse.inc lights the sun (parallelSpecular[0]) HEMISPHERICALLY
// via cExtLtData c60-63 -- facing=diffuseColor, terminator=tangentColor,
// away=backColor -- but retail //hlsl calculateDiffuseLighting() is plain
// max(N.L,0)*diffuse (its dot3 arg changes nothing), so away-facing terrain
// slopes went BLACK on D3D11. composeSlot0Shadow already feeds c60-63
// (zeroed when sunless). Non-sun terms below mirror functions.inc exactly.

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
		result += calculateDiffusePointLight(lightData.point_id[0], vertexPosition_o, vertexNormal_o);

#if VERTEX_SHADER_VERSION >= 20
	if (light_point_1_enabled)
#endif
		result += calculateDiffusePointLight(lightData.point_id[1], vertexPosition_o, vertexNormal_o);

#if VERTEX_SHADER_VERSION >= 20
	if (light_point_2_enabled)
#endif
		result += calculateDiffusePointLight(lightData.point_id[2], vertexPosition_o, vertexNormal_o);

#if VERTEX_SHADER_VERSION >= 20
	if (light_point_3_enabled)
#endif
		result += calculateDiffusePointLight(lightData.point_id[3], vertexPosition_o, vertexNormal_o);

	return result;
}

struct InputVertex
{
	float4  position : POSITION0               ;
	float4  normal_o : NORMAL0                 ;
	float4  color0   : COLOR0                  ;
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
	// (hemispheric sun + parallel + point_id + emissive -- see Hemi block above).
	outputVertex.diffuse  = saturate(inputVertex.color0 + calculateDiffuseLightingHemi(inputVertex.position, inputVertex.normal_o.xyz));

	return outputVertex;
}
