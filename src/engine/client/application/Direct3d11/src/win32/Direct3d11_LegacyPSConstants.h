// ======================================================================
//
// Direct3d11_LegacyPSConstants.h
// Phase 17 Plan 17-08 (GAP-4) -- persistent legacy PS constant shadow
// for the asset pixel shader's `SwgVertexConstants` cbuffer @ register b0.
//
// WHY THIS EXISTS
// ---------------
// Plan 17-07 closed GAP-3: the recompiled asset PSes now bind 9/9 with
// correct interpolators. But they render BLACK because they read their
// light/material constants from a reflected cbuffer `SwgVertexConstants`
// at b0 (totalSize 400) that NO producer fills -- so allDiffuseLight = 0.
//
// Direct3d11_HlslRewrite.cpp:795-805 emits that cbuffer at b0 and its own
// comment defers the constant push to a future "Plan 11-08" that would
// "mirror the legacy c-register layout into a single shadow buffer
// targeted at slot b0." THIS MODULE IS THAT SHADOW.
//
// The c-register -> byte-offset map is the in-repo authoritative enum
// Direct3d9_PixelShaderConstantRegisters.h (byte offset = index * 16).
// No TRE .inc extraction is needed (dual-AI consult CONVERGED -- see
// .planning/research/CONSULT-17-07-gap4-cbuffer-reconcile*).
//
// The shadow is fed by MULTIPLE PRODUCERS mirroring the D3D9
// setPixelShaderConstants(PSCR_*) call sites:
//   Producer A -- Direct3d11_LightManager : dot3 light data @ c0..c4
//   Producer B -- Direct3d11_StaticShaderData::apply : textureFactor/
//                 materialSpecularColor @ c5..c7 (RMW from this shadow)
//   Producer C -- Direct3d11.cpp setPixelShaderUserConstants : user @ c8+
//
// gl11-only. The D3D9 (gl05) renderer is byte-for-byte unchanged; the
// PSCR_* indices below are compile-time COPIES of the D3D9 enum (NOT a
// cross-plugin include), kept in lock-step by the shared WARNING banner.
//
// ======================================================================

#ifndef INCLUDED_Direct3d11_LegacyPSConstants_H
#define INCLUDED_Direct3d11_LegacyPSConstants_H

// ======================================================================

#include <DirectXMath.h>

// ======================================================================

// c-register indices, mirrored from
// src/engine/client/application/Direct3d9/src/win32/Direct3d9_PixelShaderConstantRegisters.h
//
// ** These values are ENCODED INTO THE PIXEL SHADER DATA FILES. ** They
// must match the D3D9 enum exactly or every asset PS reads the wrong slot.
// byte offset into the b0 shadow = index * 16.
namespace Direct3d11_LegacyPSConstantRegisters
{
	constexpr int PSCR_dot3LightDirection               = 0;   // .xyz = object-local light dir, .w = materialSpecularPower
	constexpr int PSCR_dot3LightDiffuseColor            = 1;
	constexpr int PSCR_dot3LightSpecularColor           = 2;
	constexpr int PSCR_dot3LightTangentMinusDiffuseColor = 3;
	constexpr int PSCR_dot3LightTangentMinusBackColor   = 4;
	constexpr int PSCR_textureFactor                    = 5;
	constexpr int PSCR_textureFactor2                   = 6;
	constexpr int PSCR_materialSpecularColor            = 7;
	constexpr int PSCR_userConstant                     = 8;   // first USER constant; user[i] -> c(8+i)
	constexpr int PSCR_MAX                              = 9;
}

// ======================================================================

// The b0 SwgVertexConstants shadow is 400 bytes = 25 float4 registers
// (c0..c24): c0..c8 named (see enum), c8..c24 = userConstants[17].
class Direct3d11_LegacyPSConstants
{
public:

	static constexpr int   kRegisterCount = 25;            // 25 float4 = 400 bytes
	static constexpr size_t kShadowBytes   = kRegisterCount * sizeof(DirectX::XMFLOAT4);

	// Base pointer to the 400-byte shadow (read for RMW staging in Producer B).
	static void const *getShadow();

	// Write one float4 at c-register cIndex (bounds-checked; out-of-range is a no-op).
	static void writeFloat4(int cIndex, DirectX::XMFLOAT4 const &value);

	// Write numFloat4s consecutive float4 starting at cStartIndex (bounds-clamped).
	static void writeRange(int cStartIndex, void const *src, int numFloat4s);

	// Copy a freshly-merged 400-byte staging buffer BACK into the shadow so
	// Producer B's per-pass material patches persist for later passes.
	static void writeShadowBack(void const *staging400);

	// Direct upload of the shadow to PS slot b0 (Producer-independent flush
	// fallback). The normal upload path is StaticShaderData::apply's
	// updatePS(0); this is for any draw path that bypasses it.
	static void flushToB0();

	// One-shot boot instrumentation: GAP4 b0 c0/c1/c7/c8 + path tag.
	static void logShadow(char const *path);
};

// ======================================================================

#endif
