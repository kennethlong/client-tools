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

// Plan 11-09.15 Iter-44B: per-pass alpha-test parameters for dynamic PS
// clip(). D3D11 has no fixed-function alpha test (D3D9 D3DRS_ALPHATEST*
// was removed); the test must live in the pixel shader as a clip() call.
// We push the engine's m_alphaTestEnable + reference value into PS cbuffer
// slot 1 (previously unused) and the dynamic-generated PS reads it and
// emits clip(sampledAlpha - ref) when enabled. Only the dynamic fallback
// PS uses this; real authored PSes (when present) are expected to do
// their own alpha test.
//
// Compare function is NOT plumbed here -- the dominant case in SWG is
// GreaterOrEqual (keep alpha >= ref, discard otherwise), so we emit
// clip(alpha - ref) which has that semantic. If a future asset needs
// other compare modes we can plumb a func index here.
struct Direct3d11_PSAlphaTestCB
{
	DirectX::XMFLOAT4 alphaTest;   // x = enable (0/1), y = reference (0..1), z/w = pad
};
static_assert(sizeof(Direct3d11_PSAlphaTestCB) == 16,               "Direct3d11_PSAlphaTestCB size mismatch");

// ======================================================================

class Direct3d11_ConstantBuffer
{
public:

	static int const kNumSlots = 4;

	// Maximum cbuffer size we provision (bytes). D3D11 cbuffer max is 4096
	// float4s = 65536 bytes; our slot capacity is set by the shader's actual
	// register usage.
	//
	// Plan 11-08 Task 2.5 retrofit (CODEX Q3a + Q3b): expand from Plan 11-05's
	// 1024 bytes to 1152 bytes (72 registers). The shader's cbuffer span ends
	// at register c67 (ExtendedLightData = 8 registers at c60..c67 per CODEX
	// Q3a's HLSL struct-row-alignment analysis); minimum required is
	// (67+1)*16 = 1088 bytes. 1152 = 72 registers gives 4 registers of safety
	// pad above the 1088 requirement (room for a trailing pad block in
	// Direct3d11_VertexSlot0CB and any future +-1 register drift). Plan 11-05's
	// original 1024 figure was an unverified "comfortable headroom" assumption
	// that under-provisioned c60..c67 by 64 bytes; the Iter-18 BSOD path
	// confirmed shader reads past the provisioned region produce UNDEFINED
	// values under Map(WRITE_DISCARD) (CODEX sixth hypothesis).
	static int const kMaxCBufferBytes = 1152;

	static void install();
	static void remove();

	// Plan 11-08 Iter-3a: fill every VS/PS slot with sane defaults via
	// Map(WRITE_DISCARD) -> memcpy a full kMaxCBufferBytes payload. The
	// FULL-FILL discipline is mandatory per the Iter-18 BSOD root-cause
	// item #6 (CODEX sixth hypothesis): Map(WRITE_DISCARD) does NOT zero
	// unwritten bytes -- unwritten bytes contain ARBITRARY GARBAGE from
	// prior frames. Without this prime, the first-draw race produces
	// undefined shader reads of c8..c71 in slot 0 + every byte in slots
	// 1..3 + every byte in PS slots 0..3, which is exactly the symptom
	// that took the OS down in Plan 11-07 Iter-18 (TDR escalation from
	// a NaN cascade in the shader). Called from install() after the
	// per-slot CreateBuffer loop; safe to call again at runtime when a
	// soft-reset path is added (no soft-reset path exists yet).
	static void primeDefaults();

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
//
// Direct3d11_VertexSlot0CB
//
// Plan 11-08 Task 2.5 + Task 3a: full slot-0 cbuffer for the VS, sized to
// match the shader's packoffset declarations in vertex_shader_constants.inc.
// Layout locked at planning time per CODEX Q3a ratification + the Iter-13B
// shader IR dump (stage/shader-iter13b-inc-0-output.txt). Per-field
// static_assert constellation below enforces every packoffset boundary at
// compile time so any future drift -- C++ side OR HLSL side -- surfaces as
// a build error, never a runtime NaN cascade / BSOD.
//
// Memory layout (all offsets in bytes, mapped to HLSL c-register slots):
//
//   c0..c3   offset    0   objectWorldCameraProjectionMatrix (WVP)    64B
//   c4..c7   offset   64   objectWorldMatrix (World)                  64B
//   c8..c10  offset  128   fog region                                 48B
//   c11..c15 offset  176   material                                   80B
//   c16..c43 offset  256   LightData (28 registers)                  448B
//   c44..c59 offset  704   gap (16 registers, reserved)              256B
//   c60..c67 offset  960   ExtendedLightData (8 registers)           128B
//   c68..c71 offset 1088   trailing pad (4 registers, safety)         64B
//                          ---
//                          total = 1152 bytes = 72 registers
//
// Iter-18 BSOD root-cause-set item #6 (CODEX sixth hypothesis): Map with
// WRITE_DISCARD does NOT zero unwritten bytes per Microsoft documentation
// -- unwritten bytes contain ARBITRARY GARBAGE. Every cbuffer slot upload
// must therefore write the FULL struct, no partial writes. Task 3a's
// primeDefaults landing fills every byte of this struct with identity /
// zero defaults at install() time. Task 3b's per-frame setters memcpy the
// full struct before every updateVS(0, ...) call.

struct Direct3d11_VertexSlot0CB
{
	DirectX::XMFLOAT4X4 objectWorldCameraProjectionMatrix;   // c0..c3   (WVP)         offset 0
	DirectX::XMFLOAT4X4 objectWorldMatrix;                   // c4..c7   (World)       offset 64
	DirectX::XMFLOAT4   c8_to_c10[3];                        // c8..c10  (fog region)  offset 128
	DirectX::XMFLOAT4   material[5];                         // c11..c15 (Material)    offset 176
	DirectX::XMFLOAT4   lightData[28];                       // c16..c43 (LightData)   offset 256
	DirectX::XMFLOAT4   c44_to_c59[16];                      // c44..c59 (gap)         offset 704
	DirectX::XMFLOAT4   extendedLightData[8];                // c60..c67 (ExtLight)    offset 960
	DirectX::XMFLOAT4   trailing_pad[4];                     // c68..c71 (slot pad)    offset 1088
};

static_assert(sizeof(Direct3d11_VertexSlot0CB) == 1152,                                          "Direct3d11_VertexSlot0CB size = 1152 = 72 registers (CODEX Q3a/Q3b ratified)");
static_assert((sizeof(Direct3d11_VertexSlot0CB) % 16) == 0,                                      "Direct3d11_VertexSlot0CB not 16-byte aligned");
static_assert(sizeof(Direct3d11_VertexSlot0CB) <= Direct3d11_ConstantBuffer::kMaxCBufferBytes,   "Direct3d11_VertexSlot0CB exceeds slot capacity");
static_assert(offsetof(Direct3d11_VertexSlot0CB, objectWorldCameraProjectionMatrix) == 0,        "c0 boundary");
static_assert(offsetof(Direct3d11_VertexSlot0CB, objectWorldMatrix)                 == 64,       "c4 boundary");
static_assert(offsetof(Direct3d11_VertexSlot0CB, c8_to_c10)                         == 128,      "c8 boundary");
static_assert(offsetof(Direct3d11_VertexSlot0CB, material)                          == 176,      "c11 boundary");
static_assert(offsetof(Direct3d11_VertexSlot0CB, lightData)                         == 256,      "c16 boundary");
static_assert(offsetof(Direct3d11_VertexSlot0CB, c44_to_c59)                        == 704,      "c44 boundary");
static_assert(offsetof(Direct3d11_VertexSlot0CB, extendedLightData)                 == 960,      "c60 boundary");

// ======================================================================

#endif
