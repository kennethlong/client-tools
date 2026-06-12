//hlsl vs_2_0

// ======================================================================
// 2026-06-11 //hlsl reauthoring of the legacy //asm envmask_specmap_c.vsh
// -- the vertex shader behind effect c_envmask (cantina cistern machine,
// stage floor, env-masked metal props). //asm module list: transform +
// lighting_fog_setup + fog + c_ambient + diffuse_specular, then
// oT0 = MAIN uv, oT1 = view vector reflected around the normal (env
// lookup), oT2 = MAIN uv (mask). Pre-fix D3D11 routed it to the generic
// //asm fallback VS (const-white COLOR0, no lighting, NO FOG) -> rendered
// full-bright unfogged.
//
// Modeled on the verified tfcsl reauthor (calculateDiffuseSpecularLighting
// + material.specularColor modulate). The reflection vector is computed in
// WORLD space (matching the //hlsl helper convention -- functions.inc does
// all lighting with world-space camera/light data); the asm computed it
// from its object-space r11, but the env term is a secondary contribution
// masked by the env-mask stage, and the D3D11 FFP-cascade PS currently
// samples env stages approximately anyway.
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
	float4  specular : COLOR1;
	float   fog      : FOG;
	float2  tcs_MAIN : TEXCOORD0;
	float3  tcs_ENV  : TEXCOORD1;
	float2  tcs_MASK : TEXCOORD2;
};

OutputVertex main(InputVertex inputVertex)
{
	OutputVertex outputVertex;

	outputVertex.position = transform3d(inputVertex.position);
	outputVertex.fog      = calculateFog(inputVertex.position);
	outputVertex.tcs_MAIN = inputVertex.textureCoordinateSetMAIN;
	outputVertex.tcs_MASK = inputVertex.textureCoordinateSetMAIN;

	DiffuseSpecular ds = calculateDiffuseSpecularLighting(false, inputVertex.position, inputVertex.normal_o.xyz);
	outputVertex.diffuse  = saturate(inputVertex.color0 + ds.diffuse);
	outputVertex.specular = saturate(ds.specular * material.specularColor);

	// env_t1.inc: view vector reflected around the normal (world space).
	// NOTE: the RUNTIME vertex_shader_constants.inc names the camera
	// constant `cameraPosition_w` (verified via the reflected cbuffer dump
	// stage/stage/vs-disasm-all.txt, offset 128) -- the vsh-extract copy of
	// the constants header is an older revision that calls it
	// `cameraPosition`; using that name FATALed the client (X3004,
	// 2026-06-11 crash dumps 235325/235359).
	float3 position_w = (float3)mul(inputVertex.position, objectWorldMatrix);
	float3 viewer_w   = normalize((float3)cameraPosition_w - position_w);
	float3 normal_w   = normalize((float3)mul(float4(inputVertex.normal_o.xyz, 0.0), objectWorldMatrix));
	outputVertex.tcs_ENV = normal_w * (2.0 * dot(normal_w, viewer_w)) - viewer_w;

	return outputVertex;
}
