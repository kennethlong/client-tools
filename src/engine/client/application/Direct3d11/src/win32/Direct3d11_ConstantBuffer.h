// ======================================================================
//
// Direct3d11_ConstantBuffer.h
// Phase 11 D3D11 renderer plugin -- ID3D11Buffer-backed constant buffer
// wrapper. Replaces D3D9 register-file SetVertexShaderConstantF /
// SetPixelShaderConstantF (Direct3d9_StateCache.cpp:526,561).
//
// Plan 11-05 (Wave 5 -- shader layer).
//
// Per RESEARCH Pitfall 2 (cbuffer 16-byte alignment): every cbuffer struct
// on the C++ side mirrors the HLSL cbuffer declaration exactly using
// DirectX::XMFLOAT4X4 / XMFLOAT4 (or explicit float4 padding); each struct
// has a static_assert(sizeof(...) % 16 == 0) gate.
//
// Per RESEARCH §Pattern 2: per-frame slot 0 / per-object slot 1 / per-material
// slot 2 convention. We provision kNumSlots = 4 for VS and PS (room for one
// extra per-shader-program user-constant slot when Plan 11-06 wires real draws).
//
// Per CONTEXT D-13: D3D11_USAGE_DYNAMIC + D3D11_CPU_ACCESS_WRITE; NO
// D3DPOOL_MANAGED / OnLostDevice / OnResetDevice.
//
// ======================================================================

#ifndef INCLUDED_Direct3d11_ConstantBuffer_H
#define INCLUDED_Direct3d11_ConstantBuffer_H

// ======================================================================

#include <cstddef>
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

// ======================================================================

// Common cbuffer struct definitions -- C++ mirrors of HLSL cbuffer layouts.
// Every struct must be a multiple of 16 bytes; use XMFLOAT4X4 / XMFLOAT4
// (or explicit float4 padding) per RESEARCH Pitfall 2. The static_asserts
// catch alignment regressions at compile time.

struct Direct3d11_PerFrameCB
{
	DirectX::XMFLOAT4X4 viewProj;        // 64 bytes
	DirectX::XMFLOAT4   cameraPos_pad;   // 16 bytes (xyz = pos, w = padding)
	DirectX::XMFLOAT4   fogColor_density; // 16 bytes (rgb = color, a = density)
};
static_assert(sizeof(Direct3d11_PerFrameCB) == 96,                  "Direct3d11_PerFrameCB size mismatch");
static_assert((sizeof(Direct3d11_PerFrameCB) % 16) == 0,            "Direct3d11_PerFrameCB not 16-byte aligned");

struct Direct3d11_PerObjectCB
{
	DirectX::XMFLOAT4X4 world;           // 64 bytes
	DirectX::XMFLOAT4   userConstants[8]; // 128 bytes -- D3D9 VCSR_userConstant0..7 analog
};
static_assert(sizeof(Direct3d11_PerObjectCB) == 192,                "Direct3d11_PerObjectCB size mismatch");
static_assert((sizeof(Direct3d11_PerObjectCB) % 16) == 0,           "Direct3d11_PerObjectCB not 16-byte aligned");

struct Direct3d11_PerMaterialCB
{
	DirectX::XMFLOAT4   materialDiffuse;
	DirectX::XMFLOAT4   materialSpecular;
	DirectX::XMFLOAT4   materialEmissive;
	DirectX::XMFLOAT4   userConstants[4]; // PS-side analog of D3D9 PSCR_userConstant
};
static_assert(sizeof(Direct3d11_PerMaterialCB) == 112,              "Direct3d11_PerMaterialCB size mismatch");
static_assert((sizeof(Direct3d11_PerMaterialCB) % 16) == 0,         "Direct3d11_PerMaterialCB not 16-byte aligned");

// ======================================================================

class Direct3d11_ConstantBuffer
{
public:

	static int const kNumSlots = 4;

	// Maximum cbuffer size we provision (bytes). Large enough for any of
	// the above structs; D3D11 cbuffer max is 4096 float4s = 65536 bytes.
	// 1024 bytes is comfortable headroom for SWG shader content.
	static int const kMaxCBufferBytes = 1024;

	static void install();
	static void remove();

	// CPU-side update: Map(WRITE_DISCARD) -> memcpy -> Unmap. Per
	// RESEARCH §Pattern 2 the WRITE_DISCARD mode ensures GPU/CPU
	// non-aliasing for whole-buffer replacement (which is the only
	// pattern we use for cbuffers).
	static void updateVS(int slot, void const *data, size_t sizeBytes);
	static void updatePS(int slot, void const *data, size_t sizeBytes);

	// Bind cbuffer to the GPU VS/PS slot (called from state cache when
	// shader changes -- Plan 11-06 territory; provided here so the API
	// is complete and self-tests).
	static void bindVS(int slot);
	static void bindPS(int slot);

private:

	static Microsoft::WRL::ComPtr<ID3D11Buffer> ms_vsBuffers[kNumSlots];
	static Microsoft::WRL::ComPtr<ID3D11Buffer> ms_psBuffers[kNumSlots];
};

// ======================================================================

#endif
