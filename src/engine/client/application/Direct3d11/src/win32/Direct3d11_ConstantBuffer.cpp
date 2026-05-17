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

#include <cstring>

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
