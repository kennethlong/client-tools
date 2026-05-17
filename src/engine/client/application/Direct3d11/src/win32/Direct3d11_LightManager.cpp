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

	// Flush + bind on both VS and PS stages -- shader passes may sample
	// lighting on either side.
	Direct3d11_ConstantBuffer::updateVS(kLightingCBSlot, &cb, sizeof(cb));
	Direct3d11_ConstantBuffer::bindVS(kLightingCBSlot);
	Direct3d11_ConstantBuffer::updatePS(kLightingCBSlot, &cb, sizeof(cb));
	Direct3d11_ConstantBuffer::bindPS(kLightingCBSlot);
}

// ======================================================================
