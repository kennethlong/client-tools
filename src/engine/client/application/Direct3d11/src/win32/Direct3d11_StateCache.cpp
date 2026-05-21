// ======================================================================
//
// Direct3d11_StateCache.cpp
// Phase 11 D3D11 renderer plugin -- state-object cache + binding tracker
// + draw-call dispatch + state-setter slot bodies. Plan 11-06 (Wave 6).
//
// Architecture notes:
//   * RS / BS / DSS / sampler state objects are immutable in D3D11. We
//     cache them by descriptor-hash (FNV-1a 64-bit over the struct bytes)
//     so identical state combinations share one object. Cache survives
//     for the lifetime of the device.
//   * Engine state-setter slots (setFillMode / setCullMode / setFog /
//     setAlphaFadeOpacity / ...) mutate a per-frame "current" descriptor
//     struct; the actual state object is fetched / created at draw time
//     in applyPreDrawState.
//   * Pitfall 4: SRV bindings (ms_boundSRV[16]) and sampler bindings
//     (ms_boundSampler[16]) tracked INDEPENDENTLY. setGlobalTexture only
//     touches the SRV; default samplers come from install-time defaults.
//   * Pitfall 5: cbuffer flush per draw via Direct3d11_ConstantBuffer.
//   * PS NULL fallback: if the currently-set shader's m_d3dPS is null
//     (stock-asset D3D9 PEXE bytecode case from Plan 11-05),
//     applyPreDrawState DOES NOT call PSSetShader -- D3D11 leaves
//     whatever PS was last bound (default pass-through during init).
//     Documented in Plan 11-05 SUMMARY; visual impact surfaces in Plan 11-07.
//
// ======================================================================

#include "FirstDirect3d11.h"
#include "Direct3d11_StateCache.h"

#include "Direct3d11.h"
#include "Direct3d11_ConstantBuffer.h"
#include "Direct3d11_Device.h"
#include "Direct3d11_DynamicIndexBufferData.h"
#include "Direct3d11_DynamicVertexBufferData.h"
#include "Direct3d11_PixelShaderProgramData.h"
#include "Direct3d11_StaticIndexBufferData.h"
#include "Direct3d11_StaticShaderData.h"
#include "Direct3d11_StaticVertexBufferData.h"
#include "Direct3d11_TextureData.h"
#include "Direct3d11_VertexBufferDescriptorMap.h"
#include "Direct3d11_VertexDeclarationMap.h"
#include "Direct3d11_VertexShaderData.h"

#include "clientGraphics/DynamicIndexBuffer.h"
#include "clientGraphics/DynamicVertexBuffer.h"
#include "clientGraphics/Graphics.def"
#include "clientGraphics/HardwareIndexBuffer.h"
#include "clientGraphics/HardwareVertexBuffer.h"
#include "clientGraphics/ShaderImplementation.h"
#include "clientGraphics/StaticIndexBuffer.h"
#include "clientGraphics/StaticShader.h"
#include "clientGraphics/StaticVertexBuffer.h"
#include "clientGraphics/VertexBufferFormat.h"

#include "sharedFoundation/Tag.h"
#include "sharedMath/PackedArgb.h"
#include "sharedMath/Transform.h"
#include "sharedMath/Vector.h"

#include <DirectXMath.h>

#include <cstring>
#include <unordered_map>

// ======================================================================

using Microsoft::WRL::ComPtr;
using DirectX::XMFLOAT4;
using DirectX::XMFLOAT4X4;

namespace Direct3d11_StateCacheNamespace
{
	// ------------------------------------------------------------------
	// Maximum slot counts (D3D11 hard limits per pipeline stage).

	constexpr int kMaxSRVs     = 16;
	constexpr int kMaxSamplers = 16;
	constexpr int kMaxRenderTargets = 1;

	// ------------------------------------------------------------------
	// FNV-1a 64-bit hash helper -- reused for all state-object caches.

	uint64_t fnv1a64(void const *data, size_t length)
	{
		uint64_t hash = 0xcbf29ce484222325ull;
		uint8_t const *p = static_cast<uint8_t const *>(data);
		for (size_t i = 0; i < length; ++i)
		{
			hash ^= p[i];
			hash *= 0x100000001b3ull;
		}
		return hash;
	}

	// ------------------------------------------------------------------
	// Hashed state-object caches.

	typedef std::unordered_map<uint64_t, ComPtr<ID3D11RasterizerState>>   RSCache;
	typedef std::unordered_map<uint64_t, ComPtr<ID3D11BlendState>>        BSCache;
	typedef std::unordered_map<uint64_t, ComPtr<ID3D11DepthStencilState>> DSSCache;
	typedef std::unordered_map<uint64_t, ComPtr<ID3D11SamplerState>>      SSCache;

	RSCache  *ms_rsCache  = nullptr;
	BSCache  *ms_bsCache  = nullptr;
	DSSCache *ms_dssCache = nullptr;
	SSCache  *ms_ssCache  = nullptr;

	// ------------------------------------------------------------------
	// Pending descriptors -- mutated by state-setter slots; resolved to
	// state objects on draw via applyPreDrawState.

	D3D11_RASTERIZER_DESC    ms_rsDesc     = {};
	D3D11_BLEND_DESC         ms_bsDesc     = {};
	D3D11_DEPTH_STENCIL_DESC ms_dssDesc    = {};
	bool                     ms_rsDirty    = true;
	bool                     ms_bsDirty    = true;
	bool                     ms_dssDirty   = true;

	// Default sampler state -- single descriptor reused across slots
	// unless a shader-specific sampler is bound. Plan 11-06 ships only
	// the default linear / wrap path; per-stage sampler overrides land
	// in a later plan if the engine surfaces them.
	ComPtr<ID3D11SamplerState> ms_defaultSampler;

	// ------------------------------------------------------------------
	// Currently-bound resource tracking.

	ID3D11ShaderResourceView * ms_boundSRV[kMaxSRVs]         = {};
	ID3D11SamplerState       * ms_boundSampler[kMaxSamplers] = {};

	// VB tracking (single stream for Phase 11; multi-stream deferred).
	ID3D11Buffer *  ms_currentVB         = nullptr;
	UINT            ms_currentVBStride   = 0;
	UINT            ms_currentVBOffset   = 0;
	int             ms_currentVBVertexCount = 0;
	VertexBufferFormat ms_currentVBFormat;
	bool            ms_currentVBValid    = false;

	// IB tracking.
	ID3D11Buffer *  ms_currentIB         = nullptr;
	UINT            ms_currentIBOffset   = 0;
	int             ms_currentIBIndexCount = 0;
	DXGI_FORMAT     ms_currentIBFormat   = DXGI_FORMAT_R16_UINT;
	bool            ms_currentIBValid    = false;

	// Shader tracking.
	Direct3d11_VertexShaderData const *           ms_currentVSData = nullptr;
	Direct3d11_PixelShaderProgramData const *     ms_currentPSData = nullptr;
	ID3D11InputLayout *                           ms_currentInputLayout = nullptr;
	bool                                          ms_geometryRebindNeeded = true;

	// ------------------------------------------------------------------
	// Frame counters / metrics (Direct3d11_Metrics consumes these in Task 3).

	int ms_drawCallCount      = 0;
	int ms_stateObjectCreates = 0;
	int ms_skippedDrawsNullVS = 0;
	int ms_skippedDrawsNullLayout = 0;

	// ------------------------------------------------------------------
	// Plan 11-08 Task 3b: matrix-shadow file-scope statics. The three
	// per-frame transform setters (setWorldToCameraTransform /
	// setProjectionMatrix / setObjectToWorldTransformAndScale) write
	// ONLY to these shadows -- they do NOT touch the GPU cbuffer. The
	// per-object setter (setObjectToWorldTransformAndScale) is the
	// canonical upload site: it composes WVP = (P * V) * W from the
	// current shadows and uploads the FULL Direct3d11_VertexSlot0CB
	// (1152 bytes) to slot 0 via composeSlot0Shadow().
	//
	// Iter-18 BSOD root-cause analysis (the never-committed minimum-WVP
	// attempt that BSOD'd the OS via GPU TDR escalation) enumerated 5
	// must-haves; CODEX peer review added a 6th. This block satisfies
	// must-haves #1 (verified D3D9 composition order via Iter-1 source
	// read), #4 (initial-state guarantee via identity-matrix defaults
	// here + primeDefaults at install), #5 (per-draw flush+bind via
	// applyPreDrawState's existing bindVS(0)), #7 (full-fill mandatory:
	// every composeSlot0Shadow() call uploads the entire 1152-byte
	// struct -- partial writes FORBIDDEN per CODEX 6th hypothesis).
	//
	// Identity-initialized at file scope so the first applyPreDrawState
	// call (before any engine setter has fired) finds safe defaults
	// regardless of primeDefaults's success. s_anyMatrixWritten is the
	// belt-and-suspenders sentinel for the first-draw race guard.

	XMFLOAT4X4 s_cachedView  = XMFLOAT4X4(1.0f, 0.0f, 0.0f, 0.0f,
	                                      0.0f, 1.0f, 0.0f, 0.0f,
	                                      0.0f, 0.0f, 1.0f, 0.0f,
	                                      0.0f, 0.0f, 0.0f, 1.0f);
	XMFLOAT4X4 s_cachedProj  = XMFLOAT4X4(1.0f, 0.0f, 0.0f, 0.0f,
	                                      0.0f, 1.0f, 0.0f, 0.0f,
	                                      0.0f, 0.0f, 1.0f, 0.0f,
	                                      0.0f, 0.0f, 0.0f, 1.0f);
	XMFLOAT4X4 s_cachedWorld = XMFLOAT4X4(1.0f, 0.0f, 0.0f, 0.0f,
	                                      0.0f, 1.0f, 0.0f, 0.0f,
	                                      0.0f, 0.0f, 1.0f, 0.0f,
	                                      0.0f, 0.0f, 0.0f, 1.0f);
	XMFLOAT4   s_cachedCameraPos = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	bool       s_anyMatrixWritten = false;

	// Plan 11-09 Iter-2.7 Fix C: namespace-scope shadow of the full VS slot 0
	// cbuffer (1152 bytes, Direct3d11_VertexSlot0CB layout). Replaces Plan
	// 11-05's broken function-local-static shadow in Direct3d11.cpp's
	// setVertexShaderUserConstants_impl AND Plan 11-08's first-draw race
	// guard upload pattern. All slot 0 mutations (matrix setters + user
	// constants via setVSConstants) target this shadow + mark dirty; the
	// shadow is uploaded ONCE per applyPreDrawState (lazy flush per CODEX
	// Round 3 guidance: avoids dozens of Map(WRITE_DISCARD) per frame).
	//
	// Coherence with GPU: install() runs composeSlot0Shadow + flushes once
	// so shadow and GPU state both reflect identity matrices + zero defaults
	// (matches primeDefaults' install-time GPU upload).
	Direct3d11_VertexSlot0CB s_slot0Shadow = {};
	bool                     s_slot0Dirty = false;

	// Compose WVP from current shadows + upload full Direct3d11_VertexSlot0CB
	// to slot 0. CODEX Q1 ratified order: wtp = P * V; wvp = wtp * W = P*V*W.
	// Direct port of D3D9 plugin's draw-time site at Direct3d9.cpp:4034
	// (D3DXMatrixMultiply(matrices+0, &ms_cachedWorldToProjectionMatrix,
	// &ms_cachedObjectToWorldMatrix) where ms_cachedWorldToProjectionMatrix
	// = P*V per Direct3d9.cpp:3291,3359). Per CODEX peer review,
	// XMMatrixMultiply(A, B) == D3DXMatrixMultiply(&out, &A, &B) with the
	// same row-major + row-vector + pre-multiplication semantics, so the
	// port is byte-for-byte equivalent.
	//
	// Per the cbuffer struct layout (Iter-2.5, verified against shader IR
	// dump stage/shader-iter13b-inc-0-output.txt lines 97-103):
	//   c0..c3   = objectWorldCameraProjectionMatrix (WVP)
	//   c4..c7   = objectWorldMatrix (World)
	//   c8       = cameraPosition_w (float3, world-space camera pos)
	//   c9       = viewportData     (float4, zero for now -- Task 4 if needed)
	//   c10      = fog              (float4, zero for now -- Task 4 if needed)
	//   c11..c71 = material / lightData / gap / extendedLightData / pad,
	//             all zero by {}-init (numLights = lightData[0].x = 0 is
	//             the shader-side loop-bomb guard per Iter-18 must-have #2).
	// Plan 11-09 Iter-2.7 Fix C: composes WVP from cached matrices into the
	// slot 0 shadow's c0..c3 + c4..c7 + c8 regions; marks shadow dirty so
	// the next applyPreDrawState flushes. Other shadow regions (c9..c71)
	// are preserved -- user constants pushed via setVSConstants land at
	// c52..c59 and lighting/material/etc. live elsewhere in slot 0.
	// Renamed from composeAndUploadSlot0 since the upload is now deferred
	// to flushSlot0IfDirty (called once per draw).
	void composeSlot0Shadow()
	{
		DirectX::XMMATRIX V   = DirectX::XMLoadFloat4x4(&s_cachedView);
		DirectX::XMMATRIX P   = DirectX::XMLoadFloat4x4(&s_cachedProj);
		DirectX::XMMATRIX W   = DirectX::XMLoadFloat4x4(&s_cachedWorld);
		DirectX::XMMATRIX wtp = DirectX::XMMatrixMultiply(P, V);     // P * V (CODEX Q1)
		DirectX::XMMATRIX wvp = DirectX::XMMatrixMultiply(wtp, W);   // (P * V) * W

		DirectX::XMStoreFloat4x4(&s_slot0Shadow.objectWorldCameraProjectionMatrix, wvp);
		DirectX::XMStoreFloat4x4(&s_slot0Shadow.objectWorldMatrix,                 W);

		// c8 = cameraPosition_w. Shader declares as float3, but our struct
		// member is XMFLOAT4 -- the float w component (s_cachedCameraPos.w)
		// is ignored by the shader per HLSL float3-from-float4 conversion
		// rules. c9 + c10 stay at whatever value other setters put there
		// (c9 = viewportData, c10 = fog -- preserved from setFog calls).
		s_slot0Shadow.c8_to_c10[0] = s_cachedCameraPos;

		// Plan 11-09 Iter-2.7 Fix C: defer GPU upload to flushSlot0IfDirty.
		// Only mark dirty here -- per-setter Map(WRITE_DISCARD) was a CODEX-
		// flagged anti-pattern (dozens of full-fill uploads per frame).
		s_slot0Dirty = true;
	}

	// Plan 11-09 Iter-2.7 Fix C: write arbitrary float4 values into the slot 0
	// shadow by D3D9-style register index. Mirrors Direct3d9_StateCache::
	// setVertexShaderConstants(register, data, count). Used by
	// setVertexShaderUserConstants_impl (Direct3d11.cpp) to land user constants
	// at VCSR_userConstant0..7 = registers 52..59 (verified against
	// Direct3d9_VertexShaderConstantRegisters.h:37).
	void setVSConstantsRaw(int registerIndex, void const *data, int count)
	{
		if (registerIndex < 0 || count <= 0)
			return;
		// VertexSlot0CB is 72 registers (c0..c71); reject writes that would
		// overflow. Engine push beyond this would require growing the struct
		// (or extending Plan 11-08's c68..c71 pad region).
		if (registerIndex + count > 72)
			return;
		uint8_t * const bytes = reinterpret_cast<uint8_t *>(&s_slot0Shadow);
		std::memcpy(bytes + registerIndex * 16, data, static_cast<size_t>(count) * 16);
		s_slot0Dirty = true;
	}

	// Plan 11-09 Iter-2.7 Fix C: lazy upload of the slot 0 shadow once per
	// applyPreDrawState. Replaces Plan 11-08's first-draw race guard +
	// setObjectToWorldTransformAndScale-canonical-upload pattern. If no
	// setter has touched the shadow since the last flush, this is a no-op.
	void flushSlot0IfDirty()
	{
		if (!s_slot0Dirty)
			return;
		Direct3d11_ConstantBuffer::updateVS(0, &s_slot0Shadow, sizeof(s_slot0Shadow));
		s_slot0Dirty = false;
	}

	// ------------------------------------------------------------------
	// Defaults applied at install time -- mirror Direct3d9_StateCache::install
	// (lines 52-100) for the engine-visible "starting state" the engine
	// expects on first frame.

	void initDefaultDescs()
	{
		// Rasterizer
		ms_rsDesc.FillMode              = D3D11_FILL_SOLID;
		ms_rsDesc.CullMode              = D3D11_CULL_BACK;       // D3DCULL_CCW analog
		ms_rsDesc.FrontCounterClockwise = FALSE;
		ms_rsDesc.DepthBias             = 0;
		ms_rsDesc.DepthBiasClamp        = 0.0f;
		ms_rsDesc.SlopeScaledDepthBias  = 0.0f;
		ms_rsDesc.DepthClipEnable       = TRUE;
		ms_rsDesc.ScissorEnable         = FALSE;
		ms_rsDesc.MultisampleEnable     = FALSE;
		ms_rsDesc.AntialiasedLineEnable = FALSE;

		// Blend (all targets off by default per D3D9 D3DRS_ALPHABLENDENABLE=FALSE)
		ms_bsDesc.AlphaToCoverageEnable = FALSE;
		ms_bsDesc.IndependentBlendEnable = FALSE;
		for (auto &rt : ms_bsDesc.RenderTarget)
		{
			rt.BlendEnable           = FALSE;
			rt.SrcBlend              = D3D11_BLEND_ONE;
			rt.DestBlend             = D3D11_BLEND_ZERO;
			rt.BlendOp               = D3D11_BLEND_OP_ADD;
			rt.SrcBlendAlpha         = D3D11_BLEND_ONE;
			rt.DestBlendAlpha        = D3D11_BLEND_ZERO;
			rt.BlendOpAlpha          = D3D11_BLEND_OP_ADD;
			rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		}

		// Depth-stencil (D3D9 D3DRS_ZENABLE=TRUE, ZFUNC=LESSEQUAL, ZWRITEENABLE=TRUE)
		// Plan 11-09 Iter-2.3 diagnostic ran with DepthEnable=FALSE; magenta
		// did NOT surface (PSInvocations still 0). Depth ELIMINATED as a
		// suspect. Iter-2.4 reverts depth + adds state probes (viewport /
		// scissor / IL / topology) to bisect topology vs WVP.
		ms_dssDesc.DepthEnable    = TRUE;
		ms_dssDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		ms_dssDesc.DepthFunc      = D3D11_COMPARISON_LESS_EQUAL;
		ms_dssDesc.StencilEnable  = FALSE;
		ms_dssDesc.StencilReadMask  = D3D11_DEFAULT_STENCIL_READ_MASK;
		ms_dssDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
		ms_dssDesc.FrontFace.StencilFailOp      = D3D11_STENCIL_OP_KEEP;
		ms_dssDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		ms_dssDesc.FrontFace.StencilPassOp      = D3D11_STENCIL_OP_KEEP;
		ms_dssDesc.FrontFace.StencilFunc        = D3D11_COMPARISON_ALWAYS;
		ms_dssDesc.BackFace = ms_dssDesc.FrontFace;

		ms_rsDirty = ms_bsDirty = ms_dssDirty = true;
	}

	// ------------------------------------------------------------------
	// Lookup-or-create helpers.

	ID3D11RasterizerState *getOrCreateRS(D3D11_RASTERIZER_DESC const &desc)
	{
		uint64_t const key = fnv1a64(&desc, sizeof(desc));
		auto it = ms_rsCache->find(key);
		if (it != ms_rsCache->end())
			return it->second.Get();

		ComPtr<ID3D11RasterizerState> rs;
		HRESULT const hr = Direct3d11_Device::getDevice()->CreateRasterizerState(&desc, rs.GetAddressOf());
		if (FAILED(hr))
		{
			DEBUG_REPORT_LOG_PRINT(true, ("Direct3d11_StateCache::CreateRasterizerState failed: %s\n",
				Direct3d11Namespace::hresultString(hr)));
			return nullptr;
		}
		++ms_stateObjectCreates;
		(*ms_rsCache)[key] = rs;
		return rs.Get();
	}

	ID3D11BlendState *getOrCreateBS(D3D11_BLEND_DESC const &desc)
	{
		uint64_t const key = fnv1a64(&desc, sizeof(desc));
		auto it = ms_bsCache->find(key);
		if (it != ms_bsCache->end())
			return it->second.Get();

		ComPtr<ID3D11BlendState> bs;
		HRESULT const hr = Direct3d11_Device::getDevice()->CreateBlendState(&desc, bs.GetAddressOf());
		if (FAILED(hr))
		{
			DEBUG_REPORT_LOG_PRINT(true, ("Direct3d11_StateCache::CreateBlendState failed: %s\n",
				Direct3d11Namespace::hresultString(hr)));
			return nullptr;
		}
		++ms_stateObjectCreates;
		(*ms_bsCache)[key] = bs;
		return bs.Get();
	}

	ID3D11DepthStencilState *getOrCreateDSS(D3D11_DEPTH_STENCIL_DESC const &desc)
	{
		uint64_t const key = fnv1a64(&desc, sizeof(desc));
		auto it = ms_dssCache->find(key);
		if (it != ms_dssCache->end())
			return it->second.Get();

		ComPtr<ID3D11DepthStencilState> dss;
		HRESULT const hr = Direct3d11_Device::getDevice()->CreateDepthStencilState(&desc, dss.GetAddressOf());
		if (FAILED(hr))
		{
			DEBUG_REPORT_LOG_PRINT(true, ("Direct3d11_StateCache::CreateDepthStencilState failed: %s\n",
				Direct3d11Namespace::hresultString(hr)));
			return nullptr;
		}
		++ms_stateObjectCreates;
		(*ms_dssCache)[key] = dss;
		return dss.Get();
	}

	ID3D11SamplerState *getOrCreateSampler(D3D11_SAMPLER_DESC const &desc)
	{
		uint64_t const key = fnv1a64(&desc, sizeof(desc));
		auto it = ms_ssCache->find(key);
		if (it != ms_ssCache->end())
			return it->second.Get();

		ComPtr<ID3D11SamplerState> ss;
		HRESULT const hr = Direct3d11_Device::getDevice()->CreateSamplerState(&desc, ss.GetAddressOf());
		if (FAILED(hr))
		{
			DEBUG_REPORT_LOG_PRINT(true, ("Direct3d11_StateCache::CreateSamplerState failed: %s\n",
				Direct3d11Namespace::hresultString(hr)));
			return nullptr;
		}
		++ms_stateObjectCreates;
		(*ms_ssCache)[key] = ss;
		return ss.Get();
	}

	// ------------------------------------------------------------------
	// Resolve the VS / PS / input-layout from the currently-bound static
	// shader's pass data. Sets ms_currentVSData / ms_currentPSData /
	// ms_currentInputLayout. Returns true if at least the VS is valid
	// (input layout may still be nullptr if format mismatch -- caller
	// then skips the draw).

	bool resolveShaders()
	{
		if (!ms_currentVSData)
			return false;

		if (!ms_currentVBValid)
			return ms_currentVSData->getVertexShader() != nullptr;

		ms_currentInputLayout = Direct3d11_VertexDeclarationMap::getOrCreate(
			ms_currentVBFormat, ms_currentVSData);
		return ms_currentVSData->getVertexShader() != nullptr;
	}

	// ------------------------------------------------------------------
	// Apply pending bindings to the GPU pipeline before issuing the draw.
	// Order mirrors RESEARCH "Pre-draw setup" enumeration in 11-06-PLAN.md.

	bool applyPreDrawState(D3D11_PRIMITIVE_TOPOLOGY topology)
	{
		ID3D11DeviceContext *ctx = Direct3d11_Device::getContext();
		if (!ctx)
			return false;

		// Plan 11-09 Iter-2.7 Fix C: lazy upload of slot 0 shadow once per
		// draw. Replaces Plan 11-08's first-draw race guard pattern. Any
		// matrix setter OR setVSConstants call between draws marks shadow
		// dirty; this flush uploads the full 1152 bytes (Plan 11-08 full-
		// fill mandate) and clears the dirty flag.
		flushSlot0IfDirty();

		// 1. Shaders
		if (!resolveShaders())
		{
			++ms_skippedDrawsNullVS;
			return false;
		}
		ID3D11VertexShader *vs = ms_currentVSData->getVertexShader();
		if (!vs)
		{
			++ms_skippedDrawsNullVS;
			return false;       // //asm VS or compile failure -- skip
		}

		// 2. Input layout (depends on VBFormat + VS bytecode -- Pitfall 6)
		if (!ms_currentInputLayout)
		{
			++ms_skippedDrawsNullLayout;
			return false;
		}

		ctx->IASetInputLayout(ms_currentInputLayout);
		ctx->IASetPrimitiveTopology(topology);

		// 3. VB
		if (ms_currentVBValid && ms_currentVB)
		{
			ID3D11Buffer *vbs[1] = { ms_currentVB };
			UINT strides[1]      = { ms_currentVBStride };
			UINT offsets[1]      = { ms_currentVBOffset };
			ctx->IASetVertexBuffers(0, 1, vbs, strides, offsets);
		}

		// 4. IB (only for indexed draws -- caller passes topology that
		// implies indexed; we always bind if valid to avoid driver state).
		if (ms_currentIBValid && ms_currentIB)
			ctx->IASetIndexBuffer(ms_currentIB, ms_currentIBFormat, ms_currentIBOffset);

		// 5. VS + PS (Plan 11-09 Iter-2: magenta fallback when asset PS is null).
		ctx->VSSetShader(vs, nullptr, 0);
		if (ms_currentPSData && ms_currentPSData->getPixelShader())
		{
			ctx->PSSetShader(ms_currentPSData->getPixelShader(), nullptr, 0);
		}
		else if (ID3D11PixelShader *fallback = Direct3d11_PixelShaderProgramData::getFallbackPS())
		{
			// Plan 11-09 Iter-2: bind the install-time magenta pass-through
			// PS so geometry surfaces visibly. Without this, draws rasterize
			// but produce no fragment writes (D3D11 has no implicit FFP
			// pass-through PS) -- the Plan 11-05 PEXE-caveat invisible-
			// geometry failure mode. Magenta is intentional debug color;
			// real per-shader PS lands when Phase 12 re-authors the .psh
			// assets to DXBC-format SM4+ bytecode.
			ctx->PSSetShader(fallback, nullptr, 0);
		}
		else
		{
			// Defensive: should never fire post-Iter-2 since install() FATALs
			// on fallback compile failure. Leave previous PS bound if it does.
		}

		// 6. cbuffer flush (Pitfall 5). The shadow buffers maintained by
		// Direct3d11.cpp's setVertexShaderUserConstants_impl /
		// setPixelShaderUserConstants_impl land on the GPU here. The
		// ConstantBuffer API binds the cbuffer to slot 1 / slot 2 per
		// Plan 11-05 convention; updateVS/updatePS callers (the per-frame
		// transform setters in this file) update slot 0.
		Direct3d11_ConstantBuffer::bindVS(0);
		Direct3d11_ConstantBuffer::bindVS(1);
		Direct3d11_ConstantBuffer::bindPS(0);
		Direct3d11_ConstantBuffer::bindPS(2);

		// 7. SRVs + samplers (Pitfall 4 split)
		ctx->PSSetShaderResources(0, kMaxSRVs, ms_boundSRV);
		ctx->PSSetSamplers(0, kMaxSamplers, ms_boundSampler);

		// 8. RS / BS / DSS state objects (lazy reselect on dirty).
		if (ms_rsDirty)
		{
			ID3D11RasterizerState *rs = getOrCreateRS(ms_rsDesc);
			if (rs) ctx->RSSetState(rs);
			ms_rsDirty = false;
		}
		if (ms_bsDirty)
		{
			ID3D11BlendState *bs = getOrCreateBS(ms_bsDesc);
			if (bs)
			{
				float const factor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
				ctx->OMSetBlendState(bs, factor, 0xffffffffu);
			}
			ms_bsDirty = false;
		}
		if (ms_dssDirty)
		{
			ID3D11DepthStencilState *dss = getOrCreateDSS(ms_dssDesc);
			if (dss) ctx->OMSetDepthStencilState(dss, 0);
			ms_dssDirty = false;
		}

		++ms_drawCallCount;

		// Plan 11-09 Iter-2.7f (CODEX Round 6): mark frame as having draw
		// activity. clearViewport's escape hatch checks this to decide
		// whether a primary-backbuffer clear should be deferred to next
		// beginScene (avoiding the D3D11 flip-model wipe-after-draw hazard).
		Direct3d11_Device::setFrameHasDrawActivity();

		return true;
	}
}
using namespace Direct3d11_StateCacheNamespace;

// ======================================================================

void Direct3d11_StateCache::install()
{
	DEBUG_FATAL(ms_rsCache, ("Direct3d11_StateCache already installed"));
	ms_rsCache  = new RSCache;
	ms_bsCache  = new BSCache;
	ms_dssCache = new DSSCache;
	ms_ssCache  = new SSCache;

	initDefaultDescs();

	// Default sampler: linear filter, wrap addressing -- matches most
	// engine-side D3DSAMP_* defaults (TEXMAGFILTER=LINEAR, MINFILTER=LINEAR,
	// MIPFILTER=LINEAR, ADDRESSU/V/W=WRAP).
	D3D11_SAMPLER_DESC ssDesc = {};
	ssDesc.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	ssDesc.AddressU       = D3D11_TEXTURE_ADDRESS_WRAP;
	ssDesc.AddressV       = D3D11_TEXTURE_ADDRESS_WRAP;
	ssDesc.AddressW       = D3D11_TEXTURE_ADDRESS_WRAP;
	ssDesc.MipLODBias     = 0.0f;
	ssDesc.MaxAnisotropy  = 1;
	ssDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	ssDesc.MinLOD         = 0.0f;
	ssDesc.MaxLOD         = D3D11_FLOAT32_MAX;

	HRESULT const hr = Direct3d11_Device::getDevice()->CreateSamplerState(&ssDesc, ms_defaultSampler.GetAddressOf());
	FATAL_DX_HR("Direct3d11_StateCache::install: CreateSamplerState (default) failed: %s", hr);

	// Plan 11-09 Iter-2.7 Fix C: populate slot 0 shadow with identity matrices
	// so it matches primeDefaults' install-time GPU upload. Subsequent setter
	// calls mutate the shadow + mark dirty; flushSlot0IfDirty uploads at draw
	// time. Without this, the first flush would overwrite primeDefaults'
	// identity matrices with the zero-initialized shadow contents.
	composeSlot0Shadow();   // composes from s_cached* (identity at file scope)
	s_slot0Dirty = false;   // shadow now matches GPU (post-primeDefaults); skip the redundant upload

	uint64_t const ssKey = fnv1a64(&ssDesc, sizeof(ssDesc));
	(*ms_ssCache)[ssKey] = ms_defaultSampler;
	++ms_stateObjectCreates;

	// Pre-fill bound-sampler array with the default so untouched stages
	// produce sensible sampling.
	for (int i = 0; i < kMaxSamplers; ++i)
		ms_boundSampler[i] = ms_defaultSampler.Get();
	for (int i = 0; i < kMaxSRVs; ++i)
		ms_boundSRV[i] = nullptr;

	ms_drawCallCount = ms_stateObjectCreates = 0;
	ms_skippedDrawsNullVS = ms_skippedDrawsNullLayout = 0;
}

// ----------------------------------------------------------------------

void Direct3d11_StateCache::remove()
{
	DEBUG_REPORT_LOG_PRINT(true,
		("Direct3d11_StateCache: shutdown stats stateObjectCreates=%d skippedNullVS=%d skippedNullLayout=%d\n",
		ms_stateObjectCreates, ms_skippedDrawsNullVS, ms_skippedDrawsNullLayout));

	// Drop tracked-bind pointers BEFORE releasing the caches so we don't
	// hold dangling raw pointers into the cache maps.
	for (int i = 0; i < kMaxSRVs; ++i)
		ms_boundSRV[i] = nullptr;
	for (int i = 0; i < kMaxSamplers; ++i)
		ms_boundSampler[i] = nullptr;
	ms_currentVB = nullptr;
	ms_currentIB = nullptr;
	ms_currentInputLayout = nullptr;
	ms_currentVSData = nullptr;
	ms_currentPSData = nullptr;
	ms_currentVBValid = ms_currentIBValid = false;
	ms_defaultSampler.Reset();

	delete ms_rsCache;
	delete ms_bsCache;
	delete ms_dssCache;
	delete ms_ssCache;
	ms_rsCache  = nullptr;
	ms_bsCache  = nullptr;
	ms_dssCache = nullptr;
	ms_ssCache  = nullptr;
}

// ----------------------------------------------------------------------

void Direct3d11_StateCache::beginFrame()
{
	ms_drawCallCount = 0;
	ms_skippedDrawsNullVS = 0;
	ms_skippedDrawsNullLayout = 0;
}

// ----------------------------------------------------------------------

void Direct3d11_StateCache::endFrame()
{
	// Reserved for Plan 11-07 frame-timing publish (counters consumed by
	// Direct3d11_Metrics).
}

// ======================================================================
// Slot-setter bodies
// ======================================================================

void Direct3d11_StateCache::setFillMode(GlFillMode mode)
{
	D3D11_FILL_MODE const m = (mode == GFM_wire) ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
	if (ms_rsDesc.FillMode != m)
	{
		ms_rsDesc.FillMode = m;
		ms_rsDirty = true;
	}
}

// ----------------------------------------------------------------------

void Direct3d11_StateCache::setCullMode(GlCullMode mode)
{
	D3D11_CULL_MODE m = D3D11_CULL_NONE;
	switch (mode)
	{
		case GCM_none:                m = D3D11_CULL_NONE;  break;
		case GCM_clockwise:           m = D3D11_CULL_FRONT; break;
		case GCM_counterClockwise:    m = D3D11_CULL_BACK;  break;
	}
	if (ms_rsDesc.CullMode != m)
	{
		ms_rsDesc.CullMode = m;
		ms_rsDirty = true;
	}
}

// ----------------------------------------------------------------------

void Direct3d11_StateCache::setAntialiasEnabled(bool enabled)
{
	BOOL const b = enabled ? TRUE : FALSE;
	if (ms_rsDesc.AntialiasedLineEnable != b || ms_rsDesc.MultisampleEnable != b)
	{
		ms_rsDesc.AntialiasedLineEnable = b;
		ms_rsDesc.MultisampleEnable     = b;
		ms_rsDirty = true;
	}
}

// ----------------------------------------------------------------------

void Direct3d11_StateCache::setViewport(int x, int y, int width, int height, real minZ, real maxZ)
{
	ID3D11DeviceContext *ctx = Direct3d11_Device::getContext();
	if (!ctx)
		return;
	D3D11_VIEWPORT vp = {};
	vp.TopLeftX = static_cast<float>(x);
	vp.TopLeftY = static_cast<float>(y);
	vp.Width    = static_cast<float>(width);
	vp.Height   = static_cast<float>(height);
	vp.MinDepth = static_cast<float>(minZ);
	vp.MaxDepth = static_cast<float>(maxZ);
	ctx->RSSetViewports(1, &vp);

	// Plan 11-09 Iter-2.7b (CODEX Round 4): write VSCR_viewportData at c9
	// of the slot 0 cbuffer. This is the renderer-internal constant the
	// engine's UI/HUD vertex shader reads to convert screen-space (already-
	// transformed) vertex positions into clip space. Pre-Fix, c9 stayed at
	// install-time zero, so the UI VS multiplied screen-space x/y by zero
	// -> all UI verts collapsed to a single clip-space point -> CPrimitives
	// counted them but PSInvocations stayed 0 (degenerate triangles).
	// Formula verbatim from Direct3d9.cpp:3240-3243 (VSPS path).
	float const xOffset = (static_cast<float>(x) * 2.0f) / static_cast<float>(width);
	float const yOffset = (static_cast<float>(y) * 2.0f) / static_cast<float>(height);
	float const viewportData[4] = {
		 2.0f / static_cast<float>(width),
		-2.0f / static_cast<float>(height),
		-1.0f - xOffset,
		 1.0f + yOffset
	};
	constexpr int kVSCR_viewportData = 9;
	setVSConstants(kVSCR_viewportData, viewportData, 1);

	UNREF(minZ);
	UNREF(maxZ);
}

// ----------------------------------------------------------------------

void Direct3d11_StateCache::setScissorRect(bool enabled, int x, int y, int width, int height)
{
	ID3D11DeviceContext *ctx = Direct3d11_Device::getContext();
	if (!ctx)
		return;

	if (ms_rsDesc.ScissorEnable != (enabled ? TRUE : FALSE))
	{
		ms_rsDesc.ScissorEnable = enabled ? TRUE : FALSE;
		ms_rsDirty = true;
	}

	if (enabled)
	{
		D3D11_RECT r;
		r.left   = x;
		r.top    = y;
		r.right  = x + width;
		r.bottom = y + height;
		ctx->RSSetScissorRects(1, &r);
	}
}

// ----------------------------------------------------------------------

void Direct3d11_StateCache::setWorldToCameraTransform(Transform const &objectToWorld, Vector const &cameraPosition)
{
	// Plan 11-08 Task 3b: shadow-only. The cbuffer upload happens in
	// setObjectToWorldTransformAndScale (the canonical per-object site
	// where the full WVP is composed). Pre-Plan-11-08 this site wrote
	// Direct3d11_PerFrameCB to slot 0 (96 bytes) -- which under the new
	// Direct3d11_VertexSlot0CB layout (1152 bytes) would be a partial
	// write leaving 1056 bytes UNDEFINED via Map(WRITE_DISCARD), exactly
	// the Iter-18 BSOD root cause #6. Replaced with shadow assignment;
	// see composeAndUploadSlot0 helper in this file's namespace block.
	auto const &m = objectToWorld.getMatrix();    // real[3][4]; 4th row is implicit (0,0,0,1)
	s_cachedView = XMFLOAT4X4(
		static_cast<float>(m[0][0]), static_cast<float>(m[0][1]), static_cast<float>(m[0][2]), static_cast<float>(m[0][3]),
		static_cast<float>(m[1][0]), static_cast<float>(m[1][1]), static_cast<float>(m[1][2]), static_cast<float>(m[1][3]),
		static_cast<float>(m[2][0]), static_cast<float>(m[2][1]), static_cast<float>(m[2][2]), static_cast<float>(m[2][3]),
		0.0f, 0.0f, 0.0f, 1.0f);
	s_cachedCameraPos = XMFLOAT4(cameraPosition.x, cameraPosition.y, cameraPosition.z, 0.0f);

	// Plan 11-09 Iter-2.7 Fix C: re-compose WVP into shadow so the View
	// change propagates to GPU on next applyPreDrawState. Pre-Fix-C
	// (Plan 11-08 Task 3b) this was shadow-only because setObjectToWorld-
	// TransformAndScale was the canonical upload site; but the engine
	// doesn't always call setObjectToWorld... (Iter-2.6 smoke proved),
	// so View/Proj changes would never reach GPU. Now every setter
	// triggers the compose + dirty path.
	composeSlot0Shadow();
}

// ----------------------------------------------------------------------

void Direct3d11_StateCache::setProjectionMatrix(GlMatrix4x4 const &projectionMatrix)
{
	// Plan 11-08 Task 3b: shadow-only. Pre-Plan-11-08 this site wrote
	// Direct3d11_PerObjectCB to slot 1 (192 bytes), clobbering the world
	// matrix that setObjectToWorldTransformAndScale also wrote to slot 1.
	// The shader's c4..c7 (objectWorldMatrix) was therefore overwritten
	// by the projection matrix every frame -- a structural bug. Under
	// the Plan 11-08 layout slot 1 is unused by the per-frame pipeline;
	// projection lives in the WVP composition uploaded to slot 0 via
	// composeSlot0Shadow().
	std::memcpy(&s_cachedProj, projectionMatrix.matrix, sizeof(float) * 16);

	// Plan 11-09 Iter-2.7 Fix C: re-compose + mark dirty. See note in
	// setWorldToCameraTransform above.
	composeSlot0Shadow();
}

// ----------------------------------------------------------------------

void Direct3d11_StateCache::setFog(bool enabled, real density, PackedArgb const &color)
{
	// Stash in the PerFrame cbuffer (slot 0). The view-camera setter
	// above writes the same cbuffer; the actual VS/PS read fog from it.
	// For Plan 11-06 we keep the fields wired but don't compose view+fog
	// in a single update (cbuffer update is whole-record discard);
	// instead, fog is written as a separate updateVS to slot 3 (reserved).
	float const a = enabled ? static_cast<float>(density) : 0.0f;
	float const r = static_cast<float>(color.getR()) * (1.0f / 255.0f);
	float const g = static_cast<float>(color.getG()) * (1.0f / 255.0f);
	float const b = static_cast<float>(color.getB()) * (1.0f / 255.0f);
	XMFLOAT4 fog(r, g, b, a);
	Direct3d11_ConstantBuffer::updatePS(0, &fog, sizeof(fog));
}

// ----------------------------------------------------------------------

void Direct3d11_StateCache::setObjectToWorldTransformAndScale(Transform const &objectToWorld, Vector const &scale)
{
	// Plan 11-08 Task 3b: the canonical per-object upload site. Shadows
	// the world matrix (with uniform per-axis scale composed into basis
	// columns -- same logic as the pre-Plan-11-08 version), then composes
	// the full WVP from the three shadows and uploads the entire
	// Direct3d11_VertexSlot0CB (1152 bytes) to slot 0. See
	// composeAndUploadSlot0 in this file's namespace block for the WVP
	// composition order (CODEX Q1 ratified: wtp = P * V; wvp = wtp * W).
	// Pre-Plan-11-08 this site wrote Direct3d11_PerObjectCB to slot 1
	// (clobbered by setProjectionMatrix), with no WVP composition; the
	// shader's c0..c3 was therefore reading uninitialized memory --
	// exactly the Iter-18 BSOD root cause.
	auto const &m = objectToWorld.getMatrix();    // real[3][4]
	s_cachedWorld = XMFLOAT4X4(
		static_cast<float>(m[0][0]) * scale.x, static_cast<float>(m[0][1]),           static_cast<float>(m[0][2]),           static_cast<float>(m[0][3]),
		static_cast<float>(m[1][0]),           static_cast<float>(m[1][1]) * scale.y, static_cast<float>(m[1][2]),           static_cast<float>(m[1][3]),
		static_cast<float>(m[2][0]),           static_cast<float>(m[2][1]),           static_cast<float>(m[2][2]) * scale.z, static_cast<float>(m[2][3]),
		0.0f, 0.0f, 0.0f, 1.0f);

	composeSlot0Shadow();
	s_anyMatrixWritten = true;   // Kept for back-compat; Fix C uses s_slot0Dirty instead
}

// ----------------------------------------------------------------------

void Direct3d11_StateCache::setTextureTransform(int /*stage*/, bool /*enabled*/, int /*dimension*/, bool /*projected*/, real const * /*transform*/)
{
	// D3D9 used D3DTSS_TEXTURETRANSFORMFLAGS + per-stage matrix. D3D11 has
	// no FFP texture-coordinate pipeline; texture transforms live in the
	// shader. Plan 11-06 records the slot as no-op; engine assets that
	// rely on per-stage texture transforms surface in Plan 11-07 smoke.
}

// ----------------------------------------------------------------------

void Direct3d11_StateCache::setAlphaFadeOpacity(bool /*enabled*/, float opacity)
{
	// Push opacity into the per-material cbuffer alpha channel. Plan 11-07
	// smoke will surface whether the engine shaders read it from
	// userConstants[0].a or expect a dedicated slot.
	XMFLOAT4 fade(opacity, opacity, opacity, opacity);
	Direct3d11_ConstantBuffer::updatePS(0, &fade, sizeof(fade));
}

// ----------------------------------------------------------------------

void Direct3d11_StateCache::setGlobalTexture(Tag textureTag, Texture const &texture)
{
	// Engine binds named global textures (e.g. environment maps, screen
	// captures) via tags. We route the tag's slot index into the SRV
	// array; the actual slot allocation is a global-texture lookup
	// inside Direct3d11_TextureData. Pitfall 4 enforcement: SRV only;
	// sampler stays the default unless a shader sets its own.
	Direct3d11_TextureData::setGlobalTexture(textureTag, texture);
	Direct3d11_TextureData const *const *data = Direct3d11_TextureData::getGlobalTexture(textureTag);
	if (!data || !*data)
		return;
	// Naive slot assignment by low 4 bits of tag -- conflicts surface in
	// Plan 11-07 smoke. The engine has its own tag-to-stage convention
	// (FFP era) that we'll map explicitly once smoke reveals which tags
	// are actually used at runtime.
	int const slot = static_cast<int>(textureTag & 0xF) % kMaxSRVs;
	ms_boundSRV[slot] = (*data)->getShaderResourceView();
}

// ----------------------------------------------------------------------

void Direct3d11_StateCache::releaseAllGlobalTextures()
{
	Direct3d11_TextureData::releaseAllGlobalTextures();
	for (int i = 0; i < kMaxSRVs; ++i)
		ms_boundSRV[i] = nullptr;
}

// ----------------------------------------------------------------------

void Direct3d11_StateCache::setVertexBuffer(HardwareVertexBuffer const &vb)
{
	ms_currentVBValid = false;
	ms_currentVB = nullptr;
	ms_currentVBStride = 0;
	ms_currentVBOffset = 0;
	ms_currentVBVertexCount = 0;

	if (vb.getType() == HardwareVertexBuffer::T_static)
	{
		StaticVertexBuffer const *svb = safe_cast<StaticVertexBuffer const *>(&vb);
		Direct3d11_StaticVertexBufferData const *data =
			safe_cast<Direct3d11_StaticVertexBufferData const *>(svb->m_graphicsData);
		if (!data)
			return;
		ms_currentVB             = data->getVertexBuffer();
		ms_currentVBStride       = static_cast<UINT>(data->getVertexSize());
		ms_currentVBOffset       = 0;
		ms_currentVBVertexCount  = svb->getNumberOfVertices();
		ms_currentVBFormat       = svb->getFormat();
		ms_currentVBValid        = (ms_currentVB != nullptr);
	}
	else
	{
		DynamicVertexBuffer const *dvb = safe_cast<DynamicVertexBuffer const *>(&vb);
		Direct3d11_DynamicVertexBufferData const *data =
			safe_cast<Direct3d11_DynamicVertexBufferData const *>(dvb->m_graphicsData);
		if (!data)
			return;
		ms_currentVB             = Direct3d11_DynamicVertexBufferData::getSharedRingBuffer();
		ms_currentVBStride       = static_cast<UINT>(data->getVertexSize());
		ms_currentVBOffset       = static_cast<UINT>(data->getOffset());
		ms_currentVBVertexCount  = data->getNumberOfVertices();
		ms_currentVBFormat       = dvb->getFormat();
		ms_currentVBValid        = (ms_currentVB != nullptr);
	}

	ms_geometryRebindNeeded = true;
}

// ----------------------------------------------------------------------

void Direct3d11_StateCache::setIndexBuffer(HardwareIndexBuffer const &ib)
{
	ms_currentIBValid = false;
	ms_currentIB = nullptr;
	ms_currentIBOffset = 0;
	ms_currentIBIndexCount = 0;

	if (ib.getType() == HardwareIndexBuffer::T_static)
	{
		StaticIndexBuffer const *sib = safe_cast<StaticIndexBuffer const *>(&ib);
		Direct3d11_StaticIndexBufferData const *data =
			safe_cast<Direct3d11_StaticIndexBufferData const *>(sib->m_graphicsData);
		if (!data)
			return;
		ms_currentIB             = data->getIndexBuffer();
		ms_currentIBOffset       = 0;
		ms_currentIBIndexCount   = sib->getNumberOfIndices();
		ms_currentIBFormat       = DXGI_FORMAT_R16_UINT;
		ms_currentIBValid        = (ms_currentIB != nullptr);
	}
	else
	{
		DynamicIndexBuffer const *dib = safe_cast<DynamicIndexBuffer const *>(&ib);
		Direct3d11_DynamicIndexBufferData const *data =
			safe_cast<Direct3d11_DynamicIndexBufferData const *>(dib->m_graphicsData);
		if (!data)
			return;
		ms_currentIB             = Direct3d11_DynamicIndexBufferData::getSharedRingBuffer();
		ms_currentIBOffset       = static_cast<UINT>(data->getOffset() * sizeof(Index));
		ms_currentIBIndexCount   = data->getNumberOfIndices();
		ms_currentIBFormat       = DXGI_FORMAT_R16_UINT;
		ms_currentIBValid        = (ms_currentIB != nullptr);
	}
}

// ----------------------------------------------------------------------

void Direct3d11_StateCache::setBadVertexShaderStaticShader(StaticShader const * /*shader*/)
{
	// Diagnostic-only in D3D9 plugin. Plan 11-06 records the slot bound
	// but does not consume it -- Plan 11-07 smoke surfaces whether the
	// engine relies on this for debugging.
}

// ----------------------------------------------------------------------

void Direct3d11_StateCache::setStaticShader(StaticShader const &shader, int pass)
{
	// Plan 11-09 Iter-1: the canonical port of Direct3d9.cpp:3044-3072. Cast
	// shader.m_graphicsData -> Direct3d11_StaticShaderData const * and call
	// apply(pass); StaticShaderData::apply wires ms_currentVSData /
	// ms_currentPSData via the new setCurrentVSData/setCurrentPSData
	// setters below.
	//
	// Plan 11-06's original "no-op record" comment is OBSOLETE -- treating
	// this slot as a no-op was the root cause of Plan 11-08's post-Task-3b
	// "zero draws reach the GPU" diagnostic (setStaticShader never wired
	// ms_currentVSData, so applyPreDrawState's resolveShaders always
	// short-circuited). Confirmed by CODEX post-implementation consult.
	//
	// shader.m_graphicsData was populated by Graphics::createStaticShader
	// GraphicsData -> Direct3d11::createStaticShaderGraphicsData (Plan 11-06)
	// -> new Direct3d11_StaticShaderData(shader). It is non-null for every
	// StaticShader the engine asks us to bind.
	Direct3d11_StaticShaderData const * d3dShader =
		static_cast<Direct3d11_StaticShaderData const *>(shader.m_graphicsData);
	NOT_NULL(d3dShader);
	IGNORE_RETURN(d3dShader->apply(pass));
}

// ----------------------------------------------------------------------

void Direct3d11_StateCache::setCurrentVSData(Direct3d11_VertexShaderData const *vs)
{
	// Plan 11-09 Iter-1: Direct3d11_StaticShaderData::apply calls this
	// once per draw with the per-pass VS pointer. Trip ms_geometryRebindNeeded
	// so the next applyPreDrawState reselects the input layout against the
	// new VS bytecode signature (Pitfall 6 territory).
	if (Direct3d11_StateCacheNamespace::ms_currentVSData != vs)
	{
		Direct3d11_StateCacheNamespace::ms_currentVSData = vs;
		Direct3d11_StateCacheNamespace::ms_geometryRebindNeeded = true;
	}
}

// ----------------------------------------------------------------------

void Direct3d11_StateCache::setCurrentPSData(Direct3d11_PixelShaderProgramData const *ps)
{
	// Plan 11-09 Iter-1: per-pass PS pointer wiring. May be null per Plan
	// 11-05 PEXE caveat -- applyPreDrawState's PS-NULL guard at the
	// PSSetShader call site handles that gracefully (leave previous PS bound).
	// Iter-2 will replace the leave-previous-PS-bound path with the magenta
	// fallback PS so geometry surfaces visibly.
	Direct3d11_StateCacheNamespace::ms_currentPSData = ps;
}

// ----------------------------------------------------------------------

void Direct3d11_StateCache::setVSConstants(int registerIndex, void const *data, int count)
{
	// Plan 11-09 Iter-2.7 Fix C: public thunk to the namespace-scope helper.
	// Routes engine register pushes (D3D9 SetVertexShaderConstantF analog)
	// into the slot 0 shadow.
	Direct3d11_StateCacheNamespace::setVSConstantsRaw(registerIndex, data, count);
}

// ======================================================================
// Draw-call dispatch
// ======================================================================

void Direct3d11_StateCache::drawPointList()
{
	if (!applyPreDrawState(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST))
		return;
	Direct3d11_Device::getContext()->Draw(static_cast<UINT>(ms_currentVBVertexCount), 0);
}

void Direct3d11_StateCache::drawLineList()
{
	if (!applyPreDrawState(D3D11_PRIMITIVE_TOPOLOGY_LINELIST))
		return;
	Direct3d11_Device::getContext()->Draw(static_cast<UINT>(ms_currentVBVertexCount), 0);
}

void Direct3d11_StateCache::drawLineStrip()
{
	if (!applyPreDrawState(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP))
		return;
	Direct3d11_Device::getContext()->Draw(static_cast<UINT>(ms_currentVBVertexCount), 0);
}

void Direct3d11_StateCache::drawTriangleList()
{
	if (!applyPreDrawState(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST))
		return;
	Direct3d11_Device::getContext()->Draw(static_cast<UINT>(ms_currentVBVertexCount), 0);
}

void Direct3d11_StateCache::drawTriangleStrip()
{
	if (!applyPreDrawState(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP))
		return;
	Direct3d11_Device::getContext()->Draw(static_cast<UINT>(ms_currentVBVertexCount), 0);
}

void Direct3d11_StateCache::drawTriangleFan()
{
	// D3D11 has no native TRIANGLEFAN. Emit a single draw with TRIANGLELIST
	// topology; the engine's triangle-fan asset would normally be
	// triangulated during VB build (D3D9 used D3DPT_TRIANGLEFAN directly).
	// Plan 11-07 smoke will reveal whether any subsystem actually feeds
	// us un-triangulated fans; if so, server-side re-emit lands as a
	// Rule-1 deviation in Plan 11-07.
	if (!applyPreDrawState(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST))
		return;
	Direct3d11_Device::getContext()->Draw(static_cast<UINT>(ms_currentVBVertexCount), 0);
}

void Direct3d11_StateCache::drawQuadList()
{
	// D3D9 plugin triangulates quads at draw time via a static quad-index
	// buffer. Plan 11-06 emits a STUB-style log; Plan 11-07 smoke will
	// surface whether any subsystem uses drawQuadList (UI overlays
	// historically do; particle sprite renderer may).
	DEBUG_REPORT_LOG_PRINT(true, ("Direct3d11_StateCache::drawQuadList: TODO Plan 11-07\n"));
}

void Direct3d11_StateCache::drawIndexedPointList()
{
	if (!applyPreDrawState(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST))
		return;
	Direct3d11_Device::getContext()->DrawIndexed(static_cast<UINT>(ms_currentIBIndexCount), 0, 0);
}

void Direct3d11_StateCache::drawIndexedLineList()
{
	if (!applyPreDrawState(D3D11_PRIMITIVE_TOPOLOGY_LINELIST))
		return;
	Direct3d11_Device::getContext()->DrawIndexed(static_cast<UINT>(ms_currentIBIndexCount), 0, 0);
}

void Direct3d11_StateCache::drawIndexedLineStrip()
{
	if (!applyPreDrawState(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP))
		return;
	Direct3d11_Device::getContext()->DrawIndexed(static_cast<UINT>(ms_currentIBIndexCount), 0, 0);
}

void Direct3d11_StateCache::drawIndexedTriangleList()
{
	if (!applyPreDrawState(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST))
		return;
	Direct3d11_Device::getContext()->DrawIndexed(static_cast<UINT>(ms_currentIBIndexCount), 0, 0);
}

void Direct3d11_StateCache::drawIndexedTriangleStrip()
{
	if (!applyPreDrawState(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP))
		return;
	Direct3d11_Device::getContext()->DrawIndexed(static_cast<UINT>(ms_currentIBIndexCount), 0, 0);
}

void Direct3d11_StateCache::drawIndexedTriangleFan()
{
	// As with drawTriangleFan, D3D11 has no native indexed-triangle-fan.
	// Emit as TRIANGLELIST; Plan 11-07 surfaces fix needs.
	if (!applyPreDrawState(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST))
		return;
	Direct3d11_Device::getContext()->DrawIndexed(static_cast<UINT>(ms_currentIBIndexCount), 0, 0);
}

// ------------------------------------------------------------------
// Partial draw variants (engine supplies explicit ranges).

void Direct3d11_StateCache::drawPartialPointList(int startVertex, int primitiveCount)
{
	if (!applyPreDrawState(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST))
		return;
	Direct3d11_Device::getContext()->Draw(static_cast<UINT>(primitiveCount), static_cast<UINT>(startVertex));
}

void Direct3d11_StateCache::drawPartialLineList(int startVertex, int primitiveCount)
{
	if (!applyPreDrawState(D3D11_PRIMITIVE_TOPOLOGY_LINELIST))
		return;
	Direct3d11_Device::getContext()->Draw(static_cast<UINT>(primitiveCount * 2), static_cast<UINT>(startVertex));
}

void Direct3d11_StateCache::drawPartialLineStrip(int startVertex, int primitiveCount)
{
	if (!applyPreDrawState(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP))
		return;
	Direct3d11_Device::getContext()->Draw(static_cast<UINT>(primitiveCount + 1), static_cast<UINT>(startVertex));
}

void Direct3d11_StateCache::drawPartialTriangleList(int startVertex, int primitiveCount)
{
	if (!applyPreDrawState(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST))
		return;
	Direct3d11_Device::getContext()->Draw(static_cast<UINT>(primitiveCount * 3), static_cast<UINT>(startVertex));
}

void Direct3d11_StateCache::drawPartialTriangleStrip(int startVertex, int primitiveCount)
{
	if (!applyPreDrawState(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP))
		return;
	Direct3d11_Device::getContext()->Draw(static_cast<UINT>(primitiveCount + 2), static_cast<UINT>(startVertex));
}

void Direct3d11_StateCache::drawPartialTriangleFan(int startVertex, int primitiveCount)
{
	if (!applyPreDrawState(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST))
		return;
	Direct3d11_Device::getContext()->Draw(static_cast<UINT>(primitiveCount + 2), static_cast<UINT>(startVertex));
}

// ------------------------------------------------------------------
// Partial indexed variants.

void Direct3d11_StateCache::drawPartialIndexedPointList(int baseIndex, int /*minimumVertexIndex*/, int /*numberOfVertices*/, int startIndex, int primitiveCount)
{
	if (!applyPreDrawState(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST))
		return;
	Direct3d11_Device::getContext()->DrawIndexed(
		static_cast<UINT>(primitiveCount),
		static_cast<UINT>(startIndex),
		static_cast<INT>(baseIndex));
}

void Direct3d11_StateCache::drawPartialIndexedLineList(int baseIndex, int /*minimumVertexIndex*/, int /*numberOfVertices*/, int startIndex, int primitiveCount)
{
	if (!applyPreDrawState(D3D11_PRIMITIVE_TOPOLOGY_LINELIST))
		return;
	Direct3d11_Device::getContext()->DrawIndexed(
		static_cast<UINT>(primitiveCount * 2),
		static_cast<UINT>(startIndex),
		static_cast<INT>(baseIndex));
}

void Direct3d11_StateCache::drawPartialIndexedLineStrip(int baseIndex, int /*minimumVertexIndex*/, int /*numberOfVertices*/, int startIndex, int primitiveCount)
{
	if (!applyPreDrawState(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP))
		return;
	Direct3d11_Device::getContext()->DrawIndexed(
		static_cast<UINT>(primitiveCount + 1),
		static_cast<UINT>(startIndex),
		static_cast<INT>(baseIndex));
}

void Direct3d11_StateCache::drawPartialIndexedTriangleList(int baseIndex, int /*minimumVertexIndex*/, int /*numberOfVertices*/, int startIndex, int primitiveCount)
{
	if (!applyPreDrawState(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST))
		return;
	Direct3d11_Device::getContext()->DrawIndexed(
		static_cast<UINT>(primitiveCount * 3),
		static_cast<UINT>(startIndex),
		static_cast<INT>(baseIndex));
}

void Direct3d11_StateCache::drawPartialIndexedTriangleStrip(int baseIndex, int /*minimumVertexIndex*/, int /*numberOfVertices*/, int startIndex, int primitiveCount)
{
	if (!applyPreDrawState(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP))
		return;
	Direct3d11_Device::getContext()->DrawIndexed(
		static_cast<UINT>(primitiveCount + 2),
		static_cast<UINT>(startIndex),
		static_cast<INT>(baseIndex));
}

void Direct3d11_StateCache::drawPartialIndexedTriangleFan(int baseIndex, int /*minimumVertexIndex*/, int /*numberOfVertices*/, int startIndex, int primitiveCount)
{
	if (!applyPreDrawState(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST))
		return;
	Direct3d11_Device::getContext()->DrawIndexed(
		static_cast<UINT>(primitiveCount + 2),
		static_cast<UINT>(startIndex),
		static_cast<INT>(baseIndex));
}

// ======================================================================
