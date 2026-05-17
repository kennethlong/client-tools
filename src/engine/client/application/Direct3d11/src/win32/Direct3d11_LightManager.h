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

class Direct3d11_LightManager
{
public:

	static void install();
	static void remove();

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
