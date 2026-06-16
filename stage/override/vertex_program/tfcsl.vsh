//hlsl vs_2_0

// ======================================================================
// Phase 19 //hlsl reauthoring of //asm tfcsl.vsh
// (Transform + Fog + Cell color + diffuse/Specular Lighting), the vs_2_0
// fallback tier. Mirrors modules/diffuse_specular.inc:
//   oD0 = vColor0 + diffuse(hemispheric+parallel+point) + emissive
//   oD1 = (sum light.specularColor * lit.z) * material.specularColor
// => calculateDiffuseSpecularLighting(dot3=false,...) returns {diffuse,
//    specular}; diffuse already includes emissive, specular is the per-light
//    sum pre-material; multiply specular by material.specularColor here.
//
// NOTE: the PS-fallback (t0.Sample * COLOR0) does NOT add COLOR1 (specular)
// yet, so specular highlights aren't visible until a spec-aware PS-fallback
// exists. Matte interior surfaces (the common tfcsl case) are unaffected --
// diffuse lighting is the dominant term. saturate() matches ps_1_1 clamp.
// See tfcl.vsh preamble for the asm->hlsl rationale.
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
