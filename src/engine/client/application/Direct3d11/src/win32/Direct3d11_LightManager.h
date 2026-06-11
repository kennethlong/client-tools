// ======================================================================
//
// Direct3d11_LightManager.h
// Phase 11 D3D11 renderer plugin -- per-frame light list -> cbuffer.
// Plan 11-06 (Wave 6).
//
// Replaces D3D9's SetLight / SetVertexShaderConstants register-file
// lighting plumbing with a single LightingCB cbuffer populated per
// frame via Direct3d11_ConstantBuffer.
//
// The engine's light-list iteration (Light::Type filter, ambient
// accumulation, dot-product weighting per RESEARCH §LightManager) is
// preserved verbatim from the D3D9 plugin; only the OUTPUT path differs.
//
// Per CONTEXT D-13: ComPtr ownership only; no FFP fixed-light state.
//
// ======================================================================

#ifndef INCLUDED_Direct3d11_LightManager_H
#define INCLUDED_Direct3d11_LightManager_H

// ======================================================================

#include <DirectXMath.h>

#include <vector>

#include "sharedFoundation/StlForwardDeclaration.h"

class Light;

// ======================================================================

// Light cbuffer struct -- mirrors HLSL cbuffer LightingCB { ... };
// Layout MUST stay 16-byte aligned per RESEARCH Pitfall 2.
//
// Fields:
//   ambientColor       -- accumulated ambient component (T_ambient lights)
//   directionalDir     -- first T_parallel direction in world space (xyz)
//                         + intensity in w
//   directionalColor   -- corresponding diffuse color in rgb + 1.0 in a
//   pointLightPos[8]   -- up to 8 T_point world-space positions in xyz
//                         + range in w
//   pointLightColor[8] -- corresponding diffuse color in rgb + intensity in a
//   pointLightCount    -- number of active point lights
//   pad                -- alignment padding to 16 bytes
//
// Total: 16 + 16 + 16 + (16 * 8) + (16 * 8) + 16 = 320 bytes (multiple of 16).
struct Direct3d11_LightingCB
{
	DirectX::XMFLOAT4 ambientColor;
	DirectX::XMFLOAT4 directionalDir;
	DirectX::XMFLOAT4 directionalColor;
	DirectX::XMFLOAT4 pointLightPos[8];
	DirectX::XMFLOAT4 pointLightColor[8];
	DirectX::XMFLOAT4 pointLightCountPad;     // x = count, yzw padding
};
static_assert(sizeof(Direct3d11_LightingCB) == 320,         "Direct3d11_LightingCB size mismatch");
static_assert((sizeof(Direct3d11_LightingCB) % 16) == 0,    "Direct3d11_LightingCB not 16-byte aligned");

// ======================================================================

// Plan 17-08 (GAP-4) Producer A: per-frame snapshot of the primary
// directional light's dot3 PS constants, mirroring the D3D9
// Direct3d9_LightManager PixelDot3Data (5 float4 @ PSCR_dot3LightDirection).
// Captured in WORLD space here (per frame); StaticShaderData::apply
// transforms localDirectionWorld into object-local space per draw (the
// asset PS lights against the object-space normal_o) and writes c0..c4
// into the b0 SwgVertexConstants shadow. `.w` channels carry the same
// alpha-fade/bloom/spec-power packing the D3D9 path uses, EXCEPT
// localDirection.w (materialSpecularPower) which is filled per-draw from
// the material at the write site.
struct Direct3d11_PixelDot3State
{
	DirectX::XMFLOAT4 localDirectionWorld;        // xyz = world light dir (NOT yet object-local); w unused here
	DirectX::XMFLOAT4 diffuseColor;
	DirectX::XMFLOAT4 specularColor;
	DirectX::XMFLOAT4 tangentMinusDiffuseColor;
	DirectX::XMFLOAT4 tangentMinusBackColor;
	// Plan 17-08 (GAP-5): extra fields the VS vertex-lighting path needs.
	// Direct3d11_StateCache::composeSlot0Shadow fills VertexSlot0CB.lightData
	// (ambient @ c16, dot3 @ c40-c43, parallelSpecular @ c17-c19) +
	// extendedLightData (@ c60-c63) from these so the VS emits a real COLOR0
	// (vertexDiffuse) instead of NaN -> fixes the dark-body residual.
	DirectX::XMFLOAT4 ambient;                     // accumulated T_ambient (lightData[0] @ c16)
	DirectX::XMFLOAT4 backColor;                   // raw hemispheric back  (extendedLightData[0] @ c60)
	DirectX::XMFLOAT4 tangentColor;                // raw hemispheric tangent (extendedLightData[1] @ c61)
	bool              valid;                       // false until a directional light is seen this frame

	// Black-walls fix (2026-06-08): the DIFFUSE parallel lights the world VS reads at
	// lightData.parallel[0..1] (c20-23). D3D9 selectLights routes the sun (highest specular)
	// into parallelSpecular[0]/dot3 and the next two directionals BY DIFFUSE INTENSITY into
	// parallel[0],[1]; the asset VS's MAIN diffuse term sums parallel[0]+parallel[1] (the
	// fill + bounce lights), NOT the sun. D3D11 captured only the first T_parallel -> walls lit
	// by fill/bounce went dark. Mirror D3D9: capture up to 2 diffuse-sorted directionals here.
	DirectX::XMFLOAT4 parallelDirectionWorld[2];   // world dir of parallel[0..1] (negated at the fill site)
	DirectX::XMFLOAT4 parallelDiffuseColor[2];     // diffuse color of parallel[0..1]
	int               parallelCount;               // 0..2 valid entries in the two arrays above

	// Point-light VS lane (2026-06-11, blue-NPC-face fix): the world VS's main diffuse
	// term ALSO sums pointSpecular[0] (lightData[8-11] @ c24-27) + point[0..3]
	// (lightData[12-23] @ c28-39) -- five point-light terms D3D11 never fed (only the
	// k0=1 NaN guards, StateCache:444-448). Interior scenes (cantina) are lit by warm
	// POINT fixtures; with these zero, v1 = ambient+fills only (cool blue) while D3D9's
	// v1 includes the warm points -> pink skin. Capture35 draw 516: c24-c39 all zero,
	// v1 B/R=1.39 on a near-white face albedo = the blue face. Mirror
	// Direct3d9_LightManager::selectLights' point cascade (:442-463) +
	// applyLights_vertexShader's packing (:652-720). Unused slots carry attenuation
	// (1,0,0,0) -- the divide-guard (1/dp4(att,(1,d,d2,0))) -- and zero colors/position.
	// Member initializers cover the window before the first setLights call.
	DirectX::XMFLOAT4 pointSpecPosition    { 0.0f, 0.0f, 0.0f, 0.0f };   // lightData[8]  @ c24 (world, w=1 when valid)
	DirectX::XMFLOAT4 pointSpecDiffuse     { 0.0f, 0.0f, 0.0f, 0.0f };   // lightData[9]  @ c25
	DirectX::XMFLOAT4 pointSpecAttenuation { 1.0f, 0.0f, 0.0f, 0.0f };   // lightData[10] @ c26 (k0,k1,k2,0)
	DirectX::XMFLOAT4 pointSpecSpecular    { 0.0f, 0.0f, 0.0f, 0.0f };   // lightData[11] @ c27
	DirectX::XMFLOAT4 pointPosition[4]     { {0.0f,0.0f,0.0f,0.0f}, {0.0f,0.0f,0.0f,0.0f}, {0.0f,0.0f,0.0f,0.0f}, {0.0f,0.0f,0.0f,0.0f} };   // lightData[12/15/18/21] @ c28/31/34/37
	DirectX::XMFLOAT4 pointDiffuse[4]      { {0.0f,0.0f,0.0f,0.0f}, {0.0f,0.0f,0.0f,0.0f}, {0.0f,0.0f,0.0f,0.0f}, {0.0f,0.0f,0.0f,0.0f} };   // lightData[13/16/19/22] @ c29/32/35/38
	DirectX::XMFLOAT4 pointAttenuation[4]  { {1.0f,0.0f,0.0f,0.0f}, {1.0f,0.0f,0.0f,0.0f}, {1.0f,0.0f,0.0f,0.0f}, {1.0f,0.0f,0.0f,0.0f} };   // lightData[14/17/20/23] @ c30/33/36/39
};

// ======================================================================

class Direct3d11_LightManager
{
public:

	static void install();
	static void remove();

	// Plan 17-08 (GAP-4) Producer A: the current frame's primary-directional
	// dot3 snapshot for the b0 SwgVertexConstants asset-PS lighting path.
	static Direct3d11_PixelDot3State const &getPixelDot3State();

	// Gl_api::setLights slot body.
	// Iterates the engine's light list, accumulates ambient + directional
	// + up to 8 point lights, packs into Direct3d11_LightingCB, then
	// flushes via Direct3d11_ConstantBuffer::updateVS(3, &cb, ...) +
	// updatePS(3, &cb, ...) and binds both stages so the LightingCB
	// is reachable from both VS and PS HLSL.
	//
	// LightingCB lives at cbuffer slot 3 (slots 0/1/2 are taken by
	// PerFrame/PerObject/PerMaterial from Plan 11-05).
	static void setLights(stdvector<Light const *>::fwd const &lightList);
};

// ======================================================================

#endif
