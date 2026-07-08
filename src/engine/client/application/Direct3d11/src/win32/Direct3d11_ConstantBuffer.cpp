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

#include "ConfigDirect3d11.h"
#include "Direct3d11.h"   // FATAL_DX_HR
#include "Direct3d11_Device.h"
#include "Direct3d11_LightManager.h"   // Iter-3a primeDefaults: Direct3d11_LightingCB struct identity

#include <cstring>
#include <d3d11_1.h>

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

int Direct3d11_ConstantBuffer::ms_frameVsUpdates[Direct3d11_ConstantBuffer::kNumSlots];
int Direct3d11_ConstantBuffer::ms_framePsUpdates[Direct3d11_ConstantBuffer::kNumSlots];
int Direct3d11_ConstantBuffer::ms_frameVsDiscards;
int Direct3d11_ConstantBuffer::ms_framePsDiscards;

// ----------------------------------------------------------------------
// CONSULT-58 per-frame churn census.

void Direct3d11_ConstantBuffer::beginFrame()
{
	for (int i = 0; i < kNumSlots; ++i)
	{
		ms_frameVsUpdates[i] = 0;
		ms_framePsUpdates[i] = 0;
	}
	ms_frameVsDiscards = 0;
	ms_framePsDiscards = 0;
}

void Direct3d11_ConstantBuffer::getFrameCensus(int (&vsUpdates)[kNumSlots], int (&psUpdates)[kNumSlots])
{
	for (int i = 0; i < kNumSlots; ++i)
	{
		vsUpdates[i] = ms_frameVsUpdates[i];
		psUpdates[i] = ms_framePsUpdates[i];
	}
}

void Direct3d11_ConstantBuffer::getFrameDiscardCensus(int &vsDiscards, int &psDiscards)
{
	vsDiscards = ms_frameVsDiscards;
	psDiscards = ms_framePsDiscards;
}

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

	// ------------------------------------------------------------------
	//
	// CONSULT-67 NO_OVERWRITE cbuffer ring (the last structural per-draw
	// D3D9-ism in the gl11 hot path -- same churn class as the CONSULT-58
	// dynamic VB/IB ring, which collapsed hitches 19x).
	//
	// Legacy model: one small ID3D11Buffer per stage x slot, replaced via
	// Map(WRITE_DISCARD) on EVERY update -> a driver-side buffer rename
	// essentially per draw (vsB0 carries the per-object world matrix;
	// census: median 39 renames/frame on that slot alone).
	//
	// Ring model: ONE large dynamic buffer per stage. Each update appends a
	// fixed 1280-byte entry via Map(WRITE_NO_OVERWRITE) and binds the slot
	// to that entry's window via XSSetConstantBuffers1 (D3D11.1 constant
	// offsets). The buffer renames only when the ring wraps (Map DISCARD),
	// every ~8-15 frames at census rates -- not per draw.
	//
	// Requirements probed at install (any miss -> legacy path unchanged):
	//   - ID3D11DeviceContext1 (D3D11.1 runtime, Win8+),
	//   - D3D11_FEATURE_D3D11_OPTIONS.ConstantBufferOffsetting,
	//   - .MapNoOverwriteOnDynamicConstantBuffer (11.0 FORBIDS NO_OVERWRITE
	//     maps on cbuffers; this cap lifts that),
	//   - the 1 MB CreateBuffer itself (the >64KB cbuffer cap is lifted by
	//     the 11.1 runtime; window per bind stays <= 4096 constants).
	//   - config kill switch: [Direct3d11] constantBufferRing=false.
	//
	// P19_CBUF_ZERO_TAIL semantics are PRESERVED: every slot keeps a CPU
	// shadow of its full zero-padded 1280-byte entry; updates memset the
	// shadow then memcpy the payload, and the full entry is uploaded, so
	// registers past sizeBytes read 0 exactly as on the legacy path
	// (through c79 -- the bound window is 80 constants).
	//
	// The shadows also solve the wrap hazard: a mid-frame DISCARD renames
	// the ring under the OTHER slots' live offset windows (their bindings
	// would silently read the fresh allocation's garbage). On wrap we
	// rewrite ALL slots' shadows into the fresh allocation and rebind every
	// slot, so no stale window survives a rename.
	//
	// Single-threaded device-context contract (same as the rest of this
	// file): no locking.

	int const kStageVS = 0;
	int const kStagePS = 1;

	bool                          s_ringActive;
	ComPtr<ID3D11DeviceContext1>  s_context1;

	struct RingStage
	{
		ComPtr<ID3D11Buffer> ring;
		int                  used;                                                          // byte cursor (multiple of kRingEntryBytes)
		int                  slotOffset[Direct3d11_ConstantBuffer::kNumSlots];              // byte offset of each slot's live entry, -1 = none
		unsigned char        shadow[Direct3d11_ConstantBuffer::kNumSlots][Direct3d11_ConstantBuffer::kRingEntryBytes];
	};
	RingStage s_ringStage[2];

	void ringBind(int stage, int slot)
	{
		RingStage &rs = s_ringStage[stage];
		DEBUG_FATAL(rs.slotOffset[slot] < 0,
			("Direct3d11_ConstantBuffer ringBind: stage %d slot %d bound before first update", stage, slot));

		ID3D11Buffer *buffers[1]      = { rs.ring.Get() };
		UINT const    firstConstant[1] = { static_cast<UINT>(rs.slotOffset[slot] / 16) };
		UINT const    numConstants[1]  = { static_cast<UINT>(Direct3d11_ConstantBuffer::kRingEntryConstants) };

		// NOTE: the documented same-buffer/offset-only-change dropped-bind
		// quirk applies to DEFERRED contexts on the Windows 8 runtime; we
		// bind exclusively on the immediate context. If a stale-constants
		// anomaly ever implicates this, constantBufferRing=false is the
		// escape hatch.
		if (stage == kStageVS)
			s_context1->VSSetConstantBuffers1(static_cast<UINT>(slot), 1, buffers, firstConstant, numConstants);
		else
			s_context1->PSSetConstantBuffers1(static_cast<UINT>(slot), 1, buffers, firstConstant, numConstants);
	}

	// Append the (already-refreshed) shadow of `slot` to the stage's ring.
	// Fast path = one NO_OVERWRITE map. Wrap path = one DISCARD map that
	// rewrites every live slot's shadow into the fresh allocation + rebinds.
	void ringAppend(int stage, int slot, int &discardCounter)
	{
		RingStage &rs = s_ringStage[stage];

		if (rs.used + Direct3d11_ConstantBuffer::kRingEntryBytes <= Direct3d11_ConstantBuffer::kRingBytes)
		{
			D3D11_MAPPED_SUBRESOURCE mapped = {};
			HRESULT const hr = Direct3d11_Device::getContext()->Map(
				rs.ring.Get(), 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mapped);
			FATAL_DX_HR("Direct3d11_ConstantBuffer ringAppend: Map(NO_OVERWRITE) failed: %s", hr);

			std::memcpy(static_cast<unsigned char *>(mapped.pData) + rs.used,
				rs.shadow[slot], Direct3d11_ConstantBuffer::kRingEntryBytes);
			Direct3d11_Device::getContext()->Unmap(rs.ring.Get(), 0);

			rs.slotOffset[slot] = rs.used;
			rs.used += Direct3d11_ConstantBuffer::kRingEntryBytes;

			ringBind(stage, slot);
			return;
		}

		// Wrap: rename the ring, rewrite ALL live slots at the front.
		++discardCounter;

		D3D11_MAPPED_SUBRESOURCE mapped = {};
		HRESULT const hr = Direct3d11_Device::getContext()->Map(
			rs.ring.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		FATAL_DX_HR("Direct3d11_ConstantBuffer ringAppend: Map(WRITE_DISCARD) failed: %s", hr);

		rs.used = 0;
		for (int s = 0; s < Direct3d11_ConstantBuffer::kNumSlots; ++s)
		{
			if (rs.slotOffset[s] < 0 && s != slot)
				continue;
			std::memcpy(static_cast<unsigned char *>(mapped.pData) + rs.used,
				rs.shadow[s], Direct3d11_ConstantBuffer::kRingEntryBytes);
			rs.slotOffset[s] = rs.used;
			rs.used += Direct3d11_ConstantBuffer::kRingEntryBytes;
		}
		Direct3d11_Device::getContext()->Unmap(rs.ring.Get(), 0);

		for (int s = 0; s < Direct3d11_ConstantBuffer::kNumSlots; ++s)
			if (rs.slotOffset[s] >= 0)
				ringBind(stage, s);
	}

	void ringUpdate(int stage, int slot, void const *data, size_t sizeBytes, int &discardCounter)
	{
		RingStage &rs = s_ringStage[stage];

		// Full-entry shadow refresh: zero tail (P19_CBUF_ZERO_TAIL
		// semantics through c79) + payload.
		std::memset(rs.shadow[slot], 0, Direct3d11_ConstantBuffer::kRingEntryBytes);
		std::memcpy(rs.shadow[slot], data, sizeBytes);

		ringAppend(stage, slot, discardCounter);
	}

	bool tryInstallRing()
	{
		if (!ConfigDirect3d11::getConstantBufferRing())
		{
			DEBUG_REPORT_LOG_PRINT(true, ("Direct3d11_ConstantBuffer: ring disabled by config ([Direct3d11] constantBufferRing=false); legacy per-slot path\n"));
			return false;
		}

		HRESULT hr = Direct3d11_Device::getContext()->QueryInterface(IID_PPV_ARGS(s_context1.GetAddressOf()));
		if (FAILED(hr) || !s_context1)
		{
			s_context1.Reset();
			REPORT_LOG_PRINT(true, ("Direct3d11_ConstantBuffer: no ID3D11DeviceContext1 (hr=0x%08X); legacy per-slot path\n", static_cast<unsigned>(hr)));
			return false;
		}

		D3D11_FEATURE_DATA_D3D11_OPTIONS options = {};
		hr = Direct3d11_Device::getDevice()->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS, &options, sizeof(options));
		if (FAILED(hr) || !options.ConstantBufferOffsetting || !options.MapNoOverwriteOnDynamicConstantBuffer)
		{
			s_context1.Reset();
			REPORT_LOG_PRINT(true, ("Direct3d11_ConstantBuffer: D3D11.1 cbuffer caps missing (hr=0x%08X offsetting=%d noOverwriteMap=%d); legacy per-slot path\n",
				static_cast<unsigned>(hr), options.ConstantBufferOffsetting ? 1 : 0, options.MapNoOverwriteOnDynamicConstantBuffer ? 1 : 0));
			return false;
		}

		for (int stage = 0; stage < 2; ++stage)
		{
			RingStage &rs = s_ringStage[stage];

			D3D11_BUFFER_DESC desc = {};
			desc.ByteWidth      = Direct3d11_ConstantBuffer::kRingBytes;
			desc.Usage          = D3D11_USAGE_DYNAMIC;
			desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

			hr = Direct3d11_Device::getDevice()->CreateBuffer(&desc, nullptr, rs.ring.GetAddressOf());
			if (FAILED(hr))
			{
				s_ringStage[kStageVS].ring.Reset();
				s_ringStage[kStagePS].ring.Reset();
				s_context1.Reset();
				REPORT_LOG_PRINT(true, ("Direct3d11_ConstantBuffer: %d-byte ring CreateBuffer failed (hr=0x%08X); legacy per-slot path\n",
					Direct3d11_ConstantBuffer::kRingBytes, static_cast<unsigned>(hr)));
				return false;
			}

			rs.used = 0;
			for (int s = 0; s < Direct3d11_ConstantBuffer::kNumSlots; ++s)
				rs.slotOffset[s] = -1;
			std::memset(rs.shadow, 0, sizeof(rs.shadow));
		}

		return true;
	}
}
using namespace Direct3d11_ConstantBufferNamespace;

// ----------------------------------------------------------------------

bool Direct3d11_ConstantBuffer::isRingActive()
{
	return s_ringActive;
}

// ======================================================================

void Direct3d11_ConstantBuffer::install()
{
	NOT_NULL(Direct3d11_Device::getDevice());

	// CONSULT-67: prefer the NO_OVERWRITE ring; any missing requirement
	// falls back to the legacy per-slot buffers below.
	s_ringActive = tryInstallRing();
	if (s_ringActive)
	{
		REPORT_LOG_PRINT(true,
			("Direct3d11_ConstantBuffer: NO_OVERWRITE ring active (2 stages x %d bytes; %d-byte entries, %d-constant windows via XSSetConstantBuffers1)\n",
			kRingBytes, kRingEntryBytes, kRingEntryConstants));
	}
	else
	{
		for (int i = 0; i < kNumSlots; ++i)
		{
			createOneSlot(ms_vsBuffers[i], "vs", i);
			createOneSlot(ms_psBuffers[i], "ps", i);
		}

		DEBUG_REPORT_LOG_PRINT(true,
			("Direct3d11_ConstantBuffer: installed %d VS slots + %d PS slots (each %d bytes; D3D11_USAGE_DYNAMIC)\n",
			kNumSlots, kNumSlots, kMaxCBufferBytes));
	}

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

	s_ringActive = false;
	for (int stage = 0; stage < 2; ++stage)
	{
		s_ringStage[stage].ring.Reset();
		s_ringStage[stage].used = 0;
		for (int s = 0; s < kNumSlots; ++s)
			s_ringStage[stage].slotOffset[s] = -1;
	}
	s_context1.Reset();
}

// ----------------------------------------------------------------------

void Direct3d11_ConstantBuffer::updateVS(int slot, void const *data, size_t sizeBytes)
{
	DEBUG_FATAL(slot < 0 || slot >= kNumSlots,
		("Direct3d11_ConstantBuffer::updateVS: slot %d out of range [0,%d)", slot, kNumSlots));
	DEBUG_FATAL(sizeBytes > static_cast<size_t>(kMaxCBufferBytes),
		("Direct3d11_ConstantBuffer::updateVS: size %zu exceeds slot capacity %d", sizeBytes, kMaxCBufferBytes));
	NOT_NULL(data);

	++ms_frameVsUpdates[slot];   // CONSULT-58 churn census

	if (s_ringActive)
	{
		// CONSULT-67: append to the ring + rebind the slot's window (the
		// data moved, so update implies bind on this path).
		ringUpdate(kStageVS, slot, data, sizeBytes, ms_frameVsDiscards);
		return;
	}

	NOT_NULL(ms_vsBuffers[slot].Get());

	++ms_frameVsDiscards;   // legacy path: every update is a rename

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

	++ms_framePsUpdates[slot];   // CONSULT-58 churn census

	if (s_ringActive)
	{
		// CONSULT-67: append to the ring + rebind the slot's window.
		ringUpdate(kStagePS, slot, data, sizeBytes, ms_framePsDiscards);
		return;
	}

	NOT_NULL(ms_psBuffers[slot].Get());

	++ms_framePsDiscards;   // legacy path: every update is a rename

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

	if (s_ringActive)
	{
		ringBind(kStageVS, slot);
		return;
	}

	NOT_NULL(ms_vsBuffers[slot].Get());

	ID3D11Buffer *buffers[1] = { ms_vsBuffers[slot].Get() };
	Direct3d11_Device::getContext()->VSSetConstantBuffers(static_cast<UINT>(slot), 1, buffers);
}

// ----------------------------------------------------------------------

void Direct3d11_ConstantBuffer::bindPS(int slot)
{
	DEBUG_FATAL(slot < 0 || slot >= kNumSlots,
		("Direct3d11_ConstantBuffer::bindPS: slot %d out of range [0,%d)", slot, kNumSlots));

	if (s_ringActive)
	{
		ringBind(kStagePS, slot);
		return;
	}

	NOT_NULL(ms_psBuffers[slot].Get());

	ID3D11Buffer *buffers[1] = { ms_psBuffers[slot].Get() };
	Direct3d11_Device::getContext()->PSSetConstantBuffers(static_cast<UINT>(slot), 1, buffers);
}

// ======================================================================
