// ======================================================================
//
// Direct3d11_LegacyPSConstants.cpp
// Phase 17 Plan 17-08 (GAP-4) -- legacy PS constant shadow @ b0.
// See Direct3d11_LegacyPSConstants.h for the WHY.
//
// ======================================================================

#include "FirstDirect3d11.h"
#include "Direct3d11_LegacyPSConstants.h"

#include "Direct3d11_ConstantBuffer.h"

#include "sharedDebug/DebugFlags.h"

#include <cstring>

// ======================================================================

using DirectX::XMFLOAT4;

namespace Direct3d11_LegacyPSConstantsNamespace
{
	// The asset PS reads SwgVertexConstants @ b0 (the rewriter emits it
	// there -- Direct3d11_HlslRewrite.cpp:809). Slot 0 is shared with
	// StaticShaderData::apply's b0 upload; the shadow is the single source
	// of truth that all three producers merge into.
	constexpr int kB0Slot = 0;

	// 400-byte shadow, zero-initialised once at static init.
	XMFLOAT4 s_shadow[Direct3d11_LegacyPSConstants::kRegisterCount] = {};

	static_assert(sizeof(s_shadow) == 400, "legacy PS b0 shadow must be 400 bytes (25 float4)");
	static_assert(Direct3d11_LegacyPSConstants::kShadowBytes == 400, "kShadowBytes mismatch");
}
using namespace Direct3d11_LegacyPSConstantsNamespace;

// ======================================================================

void const *Direct3d11_LegacyPSConstants::getShadow()
{
	return s_shadow;
}

// ----------------------------------------------------------------------

void Direct3d11_LegacyPSConstants::writeFloat4(int cIndex, XMFLOAT4 const &value)
{
	if (cIndex < 0 || cIndex >= kRegisterCount)
		return;
	s_shadow[cIndex] = value;
}

// ----------------------------------------------------------------------

void Direct3d11_LegacyPSConstants::writeRange(int cStartIndex, void const *src, int numFloat4s)
{
	if (!src || numFloat4s <= 0 || cStartIndex < 0)
		return;

	// Clamp so a malformed count can never overrun the 400-byte shadow.
	int count = numFloat4s;
	if (cStartIndex + count > kRegisterCount)
		count = kRegisterCount - cStartIndex;
	if (count <= 0)
		return;

	std::memcpy(&s_shadow[cStartIndex], src, static_cast<size_t>(count) * sizeof(XMFLOAT4));
}

// ----------------------------------------------------------------------

void Direct3d11_LegacyPSConstants::writeShadowBack(void const *staging400)
{
	if (!staging400)
		return;
	std::memcpy(s_shadow, staging400, kShadowBytes);
}

// ----------------------------------------------------------------------

void Direct3d11_LegacyPSConstants::flushToB0()
{
	Direct3d11_ConstantBuffer::updatePS(kB0Slot, s_shadow, kShadowBytes);
}

// ----------------------------------------------------------------------

void Direct3d11_LegacyPSConstants::logShadow(char const *path)
{
	using namespace Direct3d11_LegacyPSConstantRegisters;

	XMFLOAT4 const &c0 = s_shadow[PSCR_dot3LightDirection];
	XMFLOAT4 const &c1 = s_shadow[PSCR_dot3LightDiffuseColor];
	XMFLOAT4 const &c7 = s_shadow[PSCR_materialSpecularColor];
	XMFLOAT4 const &c8 = s_shadow[PSCR_userConstant];

	DEBUG_REPORT_LOG_PRINT(true, (
		"GAP4 b0: c0.xyz=(%.3f,%.3f,%.3f) c0.w=specPower(%.3f) "
		"c1.rgb=diffuse(%.3f,%.3f,%.3f) c7=matSpec(%.3f,%.3f,%.3f) "
		"c8=user0(%.3f,%.3f,%.3f,%.3f) path=%s\n",
		c0.x, c0.y, c0.z, c0.w,
		c1.x, c1.y, c1.z,
		c7.x, c7.y, c7.z,
		c8.x, c8.y, c8.z, c8.w,
		path ? path : "(null)"));
}

// ======================================================================
