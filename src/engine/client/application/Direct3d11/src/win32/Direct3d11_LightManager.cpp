// ======================================================================
//
// Direct3d11_LightManager.cpp
// Phase 11 D3D11 renderer plugin -- per-frame light list -> cbuffer.
// Plan 11-06 (Wave 6).
//
// Engine light-list iteration logic preserved from Direct3d9_LightManager
// in spirit (Light::Type filter, ambient accumulation, point-light
// position+range packing). The OUTPUT is the only D3D11 divergence:
// SetVertexShaderConstants (D3D9 register file) -> cbuffer update +
// bind via Direct3d11_ConstantBuffer.
//
// ======================================================================

#include "FirstDirect3d11.h"
#include "Direct3d11_LightManager.h"

#include "Direct3d11_ConstantBuffer.h"

#include "clientGraphics/Light.h"

#include "sharedMath/VectorArgb.h"
#include "sharedMath/Vector.h"
#include "sharedObject/Object.h"

#include <vector>

// ======================================================================

using DirectX::XMFLOAT4;

namespace Direct3d11_LightManagerNamespace
{
	// Reserved cbuffer slot used by VS + PS for the LightingCB (slots
	// 0/1/2 are PerFrame/PerObject/PerMaterial from Plan 11-05).
	constexpr int kLightingCBSlot = 3;

	// Plan 17-08 (GAP-4) Producer A: per-frame dot3 snapshot.
	Direct3d11_PixelDot3State s_pixelDot3State = {};
}
using namespace Direct3d11_LightManagerNamespace;

// ======================================================================

void Direct3d11_LightManager::install()
{
	// No process-wide state to initialize. setLights populates the
	// cbuffer on demand. Provided for symmetry with the other
	// install()/remove() pairs and to give Direct3d11.cpp a single
	// place to wire enable/disable if the slot acquires real state.
}

// ----------------------------------------------------------------------

void Direct3d11_LightManager::remove()
{
}

// ----------------------------------------------------------------------

void Direct3d11_LightManager::setLights(stdvector<Light const *>::fwd const &lightList)
{
	Direct3d11_LightingCB cb = {};

	// Defaults: zero ambient, no directional, no point lights.
	cb.ambientColor          = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	cb.directionalDir        = XMFLOAT4(0.0f, -1.0f, 0.0f, 0.0f);
	cb.directionalColor      = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	cb.pointLightCountPad    = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

	int pointLightCount = 0;
	bool directionalAssigned = false;

	// Plan 17-08 (GAP-4) Producer A: invalidate last frame's dot3 snapshot;
	// re-captured below when the primary directional light is assigned.
	s_pixelDot3State.valid = false;

	for (Light const *light : lightList)
	{
		if (!light)
			continue;

		VectorArgb const &color = light->getScaledDiffuseColor();
		float const intensity = light->getScaledDiffuseIntensity();
		switch (light->getType())
		{
			case Light::T_ambient:
				// Accumulate ambient contribution.
				cb.ambientColor.x += color.r * intensity;
				cb.ambientColor.y += color.g * intensity;
				cb.ambientColor.z += color.b * intensity;
				cb.ambientColor.w  = 1.0f;
				break;

			case Light::T_parallel:
				// First directional only; subsequent ignored (matches
				// SM5.0 simple-shader convention; multi-directional
				// support lands when shaders surface the need).
				if (!directionalAssigned)
				{
					Vector const dir = light->getObjectFrameK_w();
					cb.directionalDir   = XMFLOAT4(dir.x, dir.y, dir.z, intensity);
					cb.directionalColor = XMFLOAT4(color.r, color.g, color.b, 1.0f);
					directionalAssigned = true;

					// Plan 17-08 (GAP-4) Producer A: capture this light's dot3 PS
					// constants in WORLD space (mirrors Direct3d9_LightManager's
					// PixelDot3Data assembly at :592-601 / :823-830 +
					// _vsps_setExtendedLightData at :732-760). The world->object-local
					// rotation of localDirectionWorld + the materialSpecularPower in
					// .w happen per-draw at the b0 write site (StaticShaderData::apply).
					VectorArgb const &diffuse = light->getScaledDiffuseColor();
					VectorArgb const &specular = light->getScaledSpecularColor();
					VectorArgb const &back    = light->getScaledDiffuseBackColor();
					VectorArgb const &tangent = light->getScaledDiffuseTangentColor();

					s_pixelDot3State.localDirectionWorld = XMFLOAT4(dir.x, dir.y, dir.z, 0.0f);
					s_pixelDot3State.diffuseColor        = XMFLOAT4(diffuse.r, diffuse.g, diffuse.b, diffuse.a);
					s_pixelDot3State.specularColor       = XMFLOAT4(specular.r, specular.g, specular.b, specular.a);
					s_pixelDot3State.tangentMinusDiffuseColor = XMFLOAT4(
						tangent.r - diffuse.r, tangent.g - diffuse.g, tangent.b - diffuse.b, tangent.a - diffuse.a);
					s_pixelDot3State.tangentMinusBackColor    = XMFLOAT4(
						tangent.r - back.r, tangent.g - back.g, tangent.b - back.b, tangent.a - back.a);
					// Plan 17-08 (GAP-5): raw hemispheric colors for the VS extendedLightData.
					s_pixelDot3State.backColor    = XMFLOAT4(back.r, back.g, back.b, back.a);
					s_pixelDot3State.tangentColor = XMFLOAT4(tangent.r, tangent.g, tangent.b, tangent.a);
					s_pixelDot3State.valid = true;
				}
				break;

			case Light::T_point:
			case Light::T_point_multicell:
				if (pointLightCount < 8)
				{
					Vector const pos = light->getPosition_w();
					cb.pointLightPos[pointLightCount]   = XMFLOAT4(pos.x, pos.y, pos.z, light->getRange());
					cb.pointLightColor[pointLightCount] = XMFLOAT4(color.r, color.g, color.b, intensity);
					++pointLightCount;
				}
				break;

			case Light::T_spot:
				// Spot-light support deferred -- Plan 11-07 smoke surfaces
				// whether target scenes use spot lights (cantina sconces
				// might). For now treated as point light if room remains.
				if (pointLightCount < 8)
				{
					Vector const pos = light->getPosition_w();
					cb.pointLightPos[pointLightCount]   = XMFLOAT4(pos.x, pos.y, pos.z, light->getRange());
					cb.pointLightColor[pointLightCount] = XMFLOAT4(color.r, color.g, color.b, intensity);
					++pointLightCount;
				}
				break;

			case Light::T_OBSOLETE_parallelPoint:
			default:
				break;
		}
	}

	cb.pointLightCountPad.x = static_cast<float>(pointLightCount);

	// Plan 17-08 (GAP-5): record the frame's accumulated ambient for the VS
	// vertex-lighting fill (lightData[0] @ c16). Set unconditionally so the
	// body keeps an ambient floor even when no directional light is present.
	s_pixelDot3State.ambient = cb.ambientColor;

	// Flush + bind on both VS and PS stages -- shader passes may sample
	// lighting on either side.
	Direct3d11_ConstantBuffer::updateVS(kLightingCBSlot, &cb, sizeof(cb));
	Direct3d11_ConstantBuffer::bindVS(kLightingCBSlot);
	Direct3d11_ConstantBuffer::updatePS(kLightingCBSlot, &cb, sizeof(cb));
	Direct3d11_ConstantBuffer::bindPS(kLightingCBSlot);
}

// ----------------------------------------------------------------------

Direct3d11_PixelDot3State const &Direct3d11_LightManager::getPixelDot3State()
{
	return s_pixelDot3State;
}

// ======================================================================
