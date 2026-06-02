// ======================================================================
//
// Direct3d11_ConstantBuffer.cpp
// Phase 11 D3D11 renderer plugin -- ID3D11Buffer-backed constant buffer
// wrapper implementation. Plan 11-05 (Wave 5 -- shader layer).
//
// Replaces D3D9 register-file model:
//   Direct3d9_StateCache.cpp:526 -- ms_device->SetVertexShaderConstantF
//   Direct3d9_StateCache.cpp:561 -- ms_device->SetPixelShaderConstantF
//
// USAGE = D3D11_USAGE_DYNAMIC + D3D11_BIND_CONSTANT_BUFFER +
// D3D11_CPU_ACCESS_WRITE. Update via Map(D3D11_MAP_WRITE_DISCARD) ->
// memcpy -> Unmap (RESEARCH §Pattern 2 lines 236-269).
//
// Per CONTEXT D-13: NO D3DPOOL_MANAGED, NO OnLostDevice, NO OnResetDevice.
//
// ======================================================================

#include "FirstDirect3d11.h"
#include "Direct3d11_ConstantBuffer.h"

#include "Direct3d11.h"   // FATAL_DX_HR
#include "Direct3d11_Device.h"
#include "Direct3d11_LightManager.h"   // Iter-3a primeDefaults: Direct3d11_LightingCB struct identity

#include <cstring>

// Phase 19 world-corruption fix/diagnostic. WRITE_DISCARD returns UNDEFINED
// memory for the whole cbuffer; updateVS/updatePS write only sizeBytes, so a
// shader reading past sizeBytes samples per-frame driver garbage -> flat
// cycling red/yellow/teal surfaces on world geometry. Zero the tail to make
// unwritten registers read 0. If this kills the corruption it confirms the
// partial-cbuffer-undefined-read; the proper fix is then to always upload the
// full register payload the bound shader expects.
// 2026-06-01: crash under RenderDoc was the DEBUG client's D3D11 debug layer +
// RenderDoc on a 32-bit process, NOT this memset (Release client captured fine
// 50x last night). Color fix stays ON; capture on the Release client.
#define P19_CBUF_ZERO_TAIL 1

// ======================================================================

using Microsoft::WRL::ComPtr;

// ======================================================================

ComPtr<ID3D11Buffer> Direct3d11_ConstantBuffer::ms_vsBuffers[Direct3d11_ConstantBuffer::kNumSlots];
ComPtr<ID3D11Buffer> Direct3d11_ConstantBuffer::ms_psBuffers[Direct3d11_ConstantBuffer::kNumSlots];

// ======================================================================

namespace Direct3d11_ConstantBufferNamespace
{
	void createOneSlot(ComPtr<ID3D11Buffer> &out, char const *label, int slot)
	{
		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth      = Direct3d11_ConstantBuffer::kMaxCBufferBytes;
		desc.Usage          = D3D11_USAGE_DYNAMIC;
		desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags      = 0;
		desc.StructureByteStride = 0;

		HRESULT hr = Direct3d11_Device::getDevice()->CreateBuffer(&desc, nullptr, out.GetAddressOf());
		FATAL_DX_HR("Direct3d11_ConstantBuffer::install: CreateBuffer failed: %s", hr);

		UNREF(label);
		UNREF(slot);
	}
}
using namespace Direct3d11_ConstantBufferNamespace;

// ======================================================================

void Direct3d11_ConstantBuffer::install()
{
	NOT_NULL(Direct3d11_Device::getDevice());

	for (int i = 0; i < kNumSlots; ++i)
	{
		createOneSlot(ms_vsBuffers[i], "vs", i);
		createOneSlot(ms_psBuffers[i], "ps", i);
	}

	DEBUG_REPORT_LOG_PRINT(true,
		("Direct3d11_ConstantBuffer: installed %d VS slots + %d PS slots (each %d bytes; D3D11_USAGE_DYNAMIC)\n",
		kNumSlots, kNumSlots, kMaxCBufferBytes));

	// Plan 11-08 Iter-3a: prime every slot with identity / zero defaults
	// before any draw call has a chance to read uninitialized cbuffer
	// memory. See primeDefaults header comment for the full Iter-18 BSOD
	// root-cause-item-#6 rationale (Map(WRITE_DISCARD) yields UNDEFINED
	// bytes, not zeros).
	primeDefaults();
}

// ----------------------------------------------------------------------

void Direct3d11_ConstantBuffer::primeDefaults()
{
	NOT_NULL(Direct3d11_Device::getDevice());
	NOT_NULL(Direct3d11_Device::getContext());

	// Single zero-fill buffer reused for slots that don't have a
	// dedicated struct shape. Allocated on stack; kMaxCBufferBytes is
	// 1152 -- well under any reasonable stack-frame budget.
	unsigned char zero[kMaxCBufferBytes] = {};

	// -- VS slot 0: full Direct3d11_VertexSlot0CB shape with identity
	// matrices for WVP + World; remaining packoffset regions stay
	// zeroed by the {}-init. lightData[0].x = 0 is already covered by
	// zero-init (it's the implicit numLights = 0 guard against
	// shader-side for-loop bombs reading uninitialized counters).
	Direct3d11_VertexSlot0CB slot0 = {};
	DirectX::XMStoreFloat4x4(&slot0.objectWorldCameraProjectionMatrix, DirectX::XMMatrixIdentity());
	DirectX::XMStoreFloat4x4(&slot0.objectWorldMatrix,                 DirectX::XMMatrixIdentity());
	updateVS(0, &slot0, sizeof(slot0));

	// -- VS slots 1, 2: zero-fill the FULL kMaxCBufferBytes payload.
	// These slots are not actively used by Plan 11-08's per-draw path
	// (Task 3b consolidates VS state into slot 0). Belt-and-suspenders
	// against any engine path that binds them between Iter-3a landing
	// and the Task 3b setter rewrite.
	updateVS(1, zero, sizeof(zero));
	updateVS(2, zero, sizeof(zero));

	// -- VS slot 3: Plan 11-06 LightManager's reserved slot
	// (Direct3d11_LightingCB at kLightingCBSlot = 3). Direct3d11_LightManager::install
	// (LightManager.cpp:42-48) is intentionally empty -- it primes via
	// setLights only when the engine first pushes a light list, which
	// happens AFTER first-draw. Iter-3a Rule-1 deviation from the plan
	// (which assumed LightManager primed at install): primeDefaults
	// FILLS slot 3 with a zeroed payload so a first-draw read of
	// LightingCB returns defined zeros (zero ambient, no directional,
	// no point lights, count = 0) instead of garbage. When setLights
	// fires later it overwrites cleanly. Without this prime the
	// CODEX-sixth-hypothesis garbage applies to slot 3 too.
	updateVS(3, zero, sizeof(zero));

	// -- PS slots 0..3: zero-fill the FULL payload each. PS-side reads
	// of zero produce defined-dark visuals rather than NaN, matching
	// Iter-18 must-have #3 (initial-state guarantee covers EVERY slot
	// the engine binds, not just VS slot 0).
	for (int s = 0; s < kNumSlots; ++s)
		updatePS(s, zero, sizeof(zero));

	DEBUG_REPORT_LOG_PRINT(true,
		("Direct3d11_ConstantBuffer::primeDefaults: VS slot 0 = identity-matrix Direct3d11_VertexSlot0CB (1152B); "
		 "VS slots 1/2/3 + PS slots 0..3 = full %d-byte zero-fill. Iter-18 BSOD safety net.\n",
		 kMaxCBufferBytes));
}

// ----------------------------------------------------------------------

void Direct3d11_ConstantBuffer::remove()
{
	for (int i = 0; i < kNumSlots; ++i)
	{
		ms_vsBuffers[i].Reset();
		ms_psBuffers[i].Reset();
	}
}

// ----------------------------------------------------------------------

void Direct3d11_ConstantBuffer::updateVS(int slot, void const *data, size_t sizeBytes)
{
	DEBUG_FATAL(slot < 0 || slot >= kNumSlots,
		("Direct3d11_ConstantBuffer::updateVS: slot %d out of range [0,%d)", slot, kNumSlots));
	DEBUG_FATAL(sizeBytes > static_cast<size_t>(kMaxCBufferBytes),
		("Direct3d11_ConstantBuffer::updateVS: size %zu exceeds slot capacity %d", sizeBytes, kMaxCBufferBytes));
	NOT_NULL(data);
	NOT_NULL(ms_vsBuffers[slot].Get());

	D3D11_MAPPED_SUBRESOURCE mapped = {};
	HRESULT hr = Direct3d11_Device::getContext()->Map(
		ms_vsBuffers[slot].Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	FATAL_DX_HR("Direct3d11_ConstantBuffer::updateVS: Map failed: %s", hr);

#if P19_CBUF_ZERO_TAIL
	// Phase 19 fix/diagnostic: WRITE_DISCARD returns UNDEFINED driver memory for
	// the WHOLE buffer; callers write only sizeBytes, so a shader whose cbuffer
	// layout reads past sizeBytes samples garbage that VARIES PER FRAME (flat
	// cycling saturated colors). Zero the undefined tail so unwritten registers
	// read as 0 instead of per-frame garbage.
	std::memset(mapped.pData, 0, static_cast<size_t>(kMaxCBufferBytes));
#endif
	std::memcpy(mapped.pData, data, sizeBytes);

	Direct3d11_Device::getContext()->Unmap(ms_vsBuffers[slot].Get(), 0);
}

// ----------------------------------------------------------------------

void Direct3d11_ConstantBuffer::updatePS(int slot, void const *data, size_t sizeBytes)
{
	DEBUG_FATAL(slot < 0 || slot >= kNumSlots,
		("Direct3d11_ConstantBuffer::updatePS: slot %d out of range [0,%d)", slot, kNumSlots));
	DEBUG_FATAL(sizeBytes > static_cast<size_t>(kMaxCBufferBytes),
		("Direct3d11_ConstantBuffer::updatePS: size %zu exceeds slot capacity %d", sizeBytes, kMaxCBufferBytes));
	NOT_NULL(data);
	NOT_NULL(ms_psBuffers[slot].Get());

	D3D11_MAPPED_SUBRESOURCE mapped = {};
	HRESULT hr = Direct3d11_Device::getContext()->Map(
		ms_psBuffers[slot].Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	FATAL_DX_HR("Direct3d11_ConstantBuffer::updatePS: Map failed: %s", hr);

#if P19_CBUF_ZERO_TAIL
	// Phase 19 fix/diagnostic (see updateVS): zero the WRITE_DISCARD undefined
	// tail so a PS reading past sizeBytes gets 0, not per-frame garbage (the
	// flat cycling red/yellow/teal surfaces).
	std::memset(mapped.pData, 0, static_cast<size_t>(kMaxCBufferBytes));
#endif
	std::memcpy(mapped.pData, data, sizeBytes);

	Direct3d11_Device::getContext()->Unmap(ms_psBuffers[slot].Get(), 0);
}

// ----------------------------------------------------------------------

void Direct3d11_ConstantBuffer::bindVS(int slot)
{
	DEBUG_FATAL(slot < 0 || slot >= kNumSlots,
		("Direct3d11_ConstantBuffer::bindVS: slot %d out of range [0,%d)", slot, kNumSlots));
	NOT_NULL(ms_vsBuffers[slot].Get());

	ID3D11Buffer *buffers[1] = { ms_vsBuffers[slot].Get() };
	Direct3d11_Device::getContext()->VSSetConstantBuffers(static_cast<UINT>(slot), 1, buffers);
}

// ----------------------------------------------------------------------

void Direct3d11_ConstantBuffer::bindPS(int slot)
{
	DEBUG_FATAL(slot < 0 || slot >= kNumSlots,
		("Direct3d11_ConstantBuffer::bindPS: slot %d out of range [0,%d)", slot, kNumSlots));
	NOT_NULL(ms_psBuffers[slot].Get());

	ID3D11Buffer *buffers[1] = { ms_psBuffers[slot].Get() };
	Direct3d11_Device::getContext()->PSSetConstantBuffers(static_cast<UINT>(slot), 1, buffers);
}

// ======================================================================
