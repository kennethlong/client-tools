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
#include "Direct3d11_LightManager.h"   // Plan 17-08 (GAP-5): VS vertex-lighting fill in composeSlot0Shadow
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
#include <d3d11sdklayers.h>   // Iter-4: ID3D11InfoQueue::AddApplicationMessage routing for first-dynamic-PS diagnostic

#include <cstdint>
#include <cstring>
#include <set>            // Plan 17-07: per-(VS, PS, path) bind-attribution dedupe set
#include <unordered_map>
#include <utility>        // Plan 17-07: std::pair / std::tuple key for the dedupe set
#include <vector>

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

	// VB tracking (single-stream path).
	ID3D11Buffer *  ms_currentVB         = nullptr;
	UINT            ms_currentVBStride   = 0;
	UINT            ms_currentVBOffset   = 0;
	int             ms_currentVBVertexCount = 0;
	VertexBufferFormat ms_currentVBFormat;
	bool            ms_currentVBValid    = false;

	// Plan 11-09.7: multi-stream VertexBufferVector tracking. Mutually
	// exclusive with the single-stream state above -- setVertexBuffer
	// clears the vector flag and setVertexBufferVector clears the
	// single-stream state. resolveShaders + applyPreDrawState branch
	// on ms_currentVBVectorActive to pick the input-layout cache lookup
	// + IASetVertexBuffers call shape.
	enum { kMaxVBStreams = 2 };   // matches Direct3d11_VertexBufferVectorData::MAX_VERTEX_BUFFERS
	bool                          ms_currentVBVectorActive               = false;
	int                           ms_currentVBVectorStreamCount          = 0;
	ID3D11Buffer *                ms_currentVBVectorBuffers[kMaxVBStreams] = {};
	UINT                          ms_currentVBVectorStrides[kMaxVBStreams] = {};
	UINT                          ms_currentVBVectorOffsets[kMaxVBStreams] = {};
	VertexBufferFormat const *    ms_currentVBVectorFormats[kMaxVBStreams] = {};

	// IB tracking.
	ID3D11Buffer *  ms_currentIB         = nullptr;
	UINT            ms_currentIBOffset   = 0;
	int             ms_currentIBIndexCount = 0;
	DXGI_FORMAT     ms_currentIBFormat   = DXGI_FORMAT_R16_UINT;
	bool            ms_currentIBValid    = false;

	// Shader tracking.
	Direct3d11_VertexShaderData const *           ms_currentVSData = nullptr;
	uint32                                        ms_currentVSKey  = 0;   // Plan 17-09: per-pass texcoord-set key for the current VS variant
	float                                         ms_specularPower = 32.0f;   // Plan 17-09 (GAP-5): materialSpecularPower for lightData[25].w (mirror D3D9 ms_specularPower default 32)
	Direct3d11_PixelShaderProgramData const *     ms_currentPSData = nullptr;
	ID3D11InputLayout *                           ms_currentInputLayout = nullptr;
	bool                                          ms_geometryRebindNeeded = true;

	// Detail-blend fix 2026-06-11: PS slot-1 cbuffer shadow (alpha-test +
	// textureFactor + fogColor share Direct3d11_PSAlphaTestCB) + the active
	// pass's FFP texture-stage combiner descriptor for the cascade PS
	// generator. Shadow fields start at -1 sentinels (impossible real
	// values) so the FIRST setter call always mismatches and uploads -- the
	// GPU buffer is primed to ZEROS at install, so a shadow that started at
	// the "right" default would dirty-skip the first upload and leave zeros
	// live.
	Direct3d11_PSAlphaTestCB     ms_psSlot1CB = { DirectX::XMFLOAT4(-1.0f, -1.0f, 0.0f, 0.0f),
	                                              DirectX::XMFLOAT4(-1.0f, -1.0f, -1.0f, -1.0f),
	                                              DirectX::XMFLOAT4(-1.0f, -1.0f, -1.0f, -1.0f) };
	Direct3d11_FfpCombinerDesc   ms_ffpCombiner = {};
	bool                         ms_ffpCombinerValid = false;

	// Fog fix 2026-06-11: scene fog state (from the engine's setFog slot) +
	// the active pass's fog MODE (from StaticShaderData::apply, engine
	// ShaderImplementationPass::FogMode: 0=Normal scene color, 1=Black,
	// 2=White -- mirrors D3D9's per-pass D3DRS_FOGCOLOR resolve at
	// Direct3d9_StaticShaderData.cpp:932-948).
	bool              ms_sceneFogEnabled = false;
	DirectX::XMFLOAT4 ms_sceneFogColor   = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	int               ms_psFogMode       = 0;

	// Recompute the b1 fogColor from scene state + pass mode; upload on
	// change. Called from setFog (scene change) and setPsFogMode (per-pass).
	void refreshPsFogColor()
	{
		DirectX::XMFLOAT4 c;
		if (ms_psFogMode == 1)      c = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);   // FM_Black
		else if (ms_psFogMode == 2) c = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);   // FM_White
		else                        c = ms_sceneFogColor;                            // FM_Normal
		c.w = ms_sceneFogEnabled ? 1.0f : 0.0f;
		if (ms_psSlot1CB.fogColor.x == c.x && ms_psSlot1CB.fogColor.y == c.y
			&& ms_psSlot1CB.fogColor.z == c.z && ms_psSlot1CB.fogColor.w == c.w)
			return;
		ms_psSlot1CB.fogColor = c;
		Direct3d11_ConstantBuffer::updatePS(1, &ms_psSlot1CB, sizeof(ms_psSlot1CB));
		Direct3d11_ConstantBuffer::bindPS(1);
	}

	// ------------------------------------------------------------------
	// Frame counters / metrics (Direct3d11_Metrics consumes these in Task 3).

	int ms_drawCallCount      = 0;
	int ms_stateObjectCreates = 0;
	int ms_skippedDrawsNullVS = 0;
	int ms_skippedDrawsNullLayout = 0;

	// Plan 11-09.14: lifetime count of applyPreDrawState flushes where
	// ms_boundSRV[0] != nullptr at the PSSetShaderResources call. Pairs
	// with the StaticShaderData-side per-pass counters to attribute id=353
	// residue ("PS expects SRV at slot 0, but none is bound") to either
	// (a) genuine slot-0-less passes, or (b) StaticShaderData-bypass paths
	// (CODEX Q6 residual-gap survey).
	int ms_drawsWithSRV0Bound = 0;

	// Plan 11-09.15 (CODEX Bucket A): reusable static fan-to-list index
	// buffer + 6 lifetime counters. D3D11 has no native TRIANGLEFAN topology;
	// drawTriangleFan + drawPartialTriangleFan bind this IB after
	// applyPreDrawState and issue DrawIndexed with the canonical fan-to-list
	// pattern (0, 1, 2, 0, 2, 3, ..., 0, N-2, N-1). Pre-Plan-11-09.15 the
	// Plan 11-06 STUB submitted fan vertex data with TRIANGLELIST topology;
	// 4-vert quads (SkyBox6Sided face, sun, moon, planet, sprite, beam,
	// marker, post-FX, Bink video) drew as upper-right triangle only.
	//
	// CODEX Q2: R16_UINT, initial capacity 1024 verts, doubling growth,
	// hard cap 65535. ms_triangleFanMaxVertices surfaces actual max at
	// shutdown so capacity sizing can be tuned post-soak.
	constexpr int kTriangleFanIBInitialCapacityVerts = 1024;
	constexpr int kTriangleFanIBMaxCapacityVerts     = 65535;
	ComPtr<ID3D11Buffer> ms_triangleFanIB;
	int ms_triangleFanIBCapacityVerts = 0;

	// Plan 11-09.15 Iter-27: reusable quad-list IB mirror of the fan-list
	// IB infrastructure. CODEX + Cursor convergent consult on the
	// engine-renders-only-postfx-blit symptom identified drawQuadList as
	// the missing implementation that silently dropped every UI textured
	// quad. D3D9 reference: Direct3d9.cpp:4277 uses ms_quadListIndexBuffer
	// with pattern (4N+0, 4N+1, 4N+2, 4N+0, 4N+2, 4N+3) per quad.
	// R16_UINT -> cap is 65535 / 4 = 16383 quads.
	constexpr int kQuadListIBInitialCapacityQuads = 256;   // 1024 verts -- room for splash + mid-UI
	constexpr int kQuadListIBMaxCapacityQuads     = 16383; // R16_UINT cap; 16383 * 4 = 65532 verts
	ComPtr<ID3D11Buffer> ms_quadListIB;
	int ms_quadListIBCapacityQuads = 0;

	int ms_drawTriangleFanCalls               = 0;
	int ms_drawPartialTriangleFanCalls        = 0;
	int ms_triangleFanIBRebuilds              = 0;
	int ms_triangleFanMaxVertices             = 0;

	// Plan 11-09.15 Iter-27: drawQuadList counters parallel to fan counters.
	int ms_drawQuadListCalls                  = 0;
	int ms_quadListIBRebuilds                 = 0;
	int ms_quadListMaxQuads                   = 0;
	// Plan 11-09.15 (CODEX Q3 DEFER + INSTRUMENT): indexed-fan variants still
	// broken (TRIANGLELIST topology applied to fan-shaped indexed data). Real
	// fix needs CPU-side index shadows in Direct3d11_StaticIndexBufferData at
	// lock/unlock first; that is Plan 11-09.16+ scope. These counters tell
	// smoke whether 11-09.16 is required (non-zero -> engine actively uses
	// indexed fans per ShaderPrimitiveSetTemplate.cpp:673 SPSPT_indexedTriangleFan).
	int ms_drawIndexedTriangleFanCalls        = 0;
	int ms_drawPartialIndexedTriangleFanCalls = 0;

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

	// Plan 17-08 (GAP-4) Producer A: the raw orthonormal object->world
	// Transform (NOT scale-composed like s_cachedWorld) so we can reuse the
	// engine's own Transform::rotate_p2l to map a world light direction into
	// object-local space EXACTLY as Direct3d9_LightManager does
	// (ms_objectToWorldTransform.rotate_p2l). Default-constructed = identity;
	// if a draw never set an object transform, rotate_p2l is a pass-through
	// (lighting stays non-black, just world-aligned -- refine on boot).
	Transform  s_cachedObjectToWorldTransform;
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

		// Plan 11-09.15 Iter-38B: TRANSPOSE before storing.
		//
		// Iter-38A2 captured V/P/W and demonstrated empirically that the
		// shader bytecode does row-vector left-multiply (`pos_row * M`) per
		// Cursor's disassembly, while the engine's source matrices are in
		// column-vector convention (translation in last *column*: W has
		// translation in W._14/_24/_34, not in row 3). For
		// `(M * pos_col)` semantics to come out of `(pos_row * stored)`, the
		// stored matrix must be the transpose of M.
		//
		// Verified by hand-walking sample #26 (terrain, identity W,
		// in-world): vertex 10 units in front of camera gives clip.w ~ +9.97
		// under (M * pos_col) (correct) but clip.w ~ -4.4e7 under
		// (pos_row * M) (catastrophic radial-fan symptom). Transposing M
		// makes (pos_row * M_T) = (M * pos_col).
		DirectX::XMMATRIX wvp_t = DirectX::XMMatrixTranspose(wvp);
		DirectX::XMMATRIX W_t   = DirectX::XMMatrixTranspose(W);
		DirectX::XMStoreFloat4x4(&s_slot0Shadow.objectWorldCameraProjectionMatrix, wvp_t);
		DirectX::XMStoreFloat4x4(&s_slot0Shadow.objectWorldMatrix,                 W_t);

		// c8 = cameraPosition_w. Shader declares as float3, but our struct
		// member is XMFLOAT4 -- the float w component (s_cachedCameraPos.w)
		// is ignored by the shader per HLSL float3-from-float4 conversion
		// rules. c9 + c10 stay at whatever value other setters put there
		// (c9 = viewportData, c10 = fog -- preserved from setFog calls).
		s_slot0Shadow.c8_to_c10[0] = s_cachedCameraPos;

		// Plan 17-08 (GAP-5): fill the VS vertex-lighting region so the VS emits a
		// real COLOR0 (vertexDiffuse) / COLOR1 (vertexSpecular) instead of NaN ->
		// fixes the dark-body residual after GAP-4. Mirrors D3D9
		// applyLights_vertexShader's lightData upload (Direct3d9_LightManager.cpp:
		// 530-628 / 723 + the dot3 path :768-846). Object-local dir + camera pos
		// are derived from the cached object->world transform exactly as D3D9 does
		// (ms_objectToWorldTransform.rotate_p2l / rotateTranslate_p2l). We fill BOTH
		// parallelSpecular[0] (c17-c19, WORLD dir) AND dot3 (c40-c43, OBJECT-LOCAL)
		// so whichever the VS reads is populated. extendedLightData (c60-c63) carries
		// the hemispheric colors. VSCR layout: lightData[28]@c16, ext[8]@c60 (see
		// Direct3d9_VertexShaderConstantRegisters.h + the VertexSlot0CB static_asserts).
		{
			Direct3d11_PixelDot3State const &d3 = Direct3d11_LightManager::getPixelDot3State();

			// ambient @ lightData[0] (c16) -- the real accumulated ambient (always, even with no
			// directional light). Mirrors D3D9 applyLights_vertexShader's lightData.ambient upload.
			s_slot0Shadow.lightData[0] = d3.ambient;

			if (d3.valid)
			{
				Vector const worldDir(
					d3.localDirectionWorld.x, d3.localDirectionWorld.y, d3.localDirectionWorld.z);
				Vector const localDir = s_cachedObjectToWorldTransform.rotate_p2l(worldDir);
				Vector const camWorld(s_cachedCameraPos.x, s_cachedCameraPos.y, s_cachedCameraPos.z);
				Vector const localCam = s_cachedObjectToWorldTransform.rotateTranslate_p2l(camWorld);

				// parallelSpecular[0] @ lightData[1..3] (c17-c19): WORLD dir (negated), colors.
				s_slot0Shadow.lightData[1] = DirectX::XMFLOAT4(-worldDir.x, -worldDir.y, -worldDir.z, 0.0f);
				s_slot0Shadow.lightData[2] = d3.diffuseColor;
				s_slot0Shadow.lightData[3] = d3.specularColor;

				// dot3 @ lightData[24..27] (c40-c43): OBJECT-LOCAL (localCameraPosition,
				// localDirection=-rotate_p2l(dir), diffuseColor, specularColor).
				// Plan 17-09 (GAP-5 specular-power fix): lightData[25].w carries
				// materialSpecularPower (mirror D3D9 applyLights_vertexShader:577
				// `dot3.localDirection.w = getSpecularPower()`). The pre-17-09 stub
				// left .w = 0.0f, but calculateDiffuseSpecularLighting's pow() lowers
				// to exp2(power * log2(N.H)); with power=0 AND N.H<=0 (back/perp
				// vertices, common on arms) that is exp2(0 * -INF) = exp2(NaN) = NaN,
				// poisoning COLOR0/COLOR1 -> the purple/green bump arms. 32 = D3D9's
				// default specular power; the real per-material value is fed below.
				s_slot0Shadow.lightData[24] = DirectX::XMFLOAT4(localCam.x, localCam.y, localCam.z, 1.0f);
				s_slot0Shadow.lightData[25] = DirectX::XMFLOAT4(-localDir.x, -localDir.y, -localDir.z, ms_specularPower);
				s_slot0Shadow.lightData[26] = d3.diffuseColor;
				s_slot0Shadow.lightData[27] = d3.specularColor;

				// extendedLightData @ [0..3] (c60-c63): hemispheric back/tangent + deltas.
				s_slot0Shadow.extendedLightData[0] = d3.backColor;
				s_slot0Shadow.extendedLightData[1] = d3.tangentColor;
				s_slot0Shadow.extendedLightData[2] = d3.tangentMinusBackColor;
				s_slot0Shadow.extendedLightData[3] = d3.tangentMinusDiffuseColor;
			}

			// parallel[0..1] @ lightData[4..7] (c20-c23): THE BLACK-WALLS FIX (2026-06-08).
			// The world VS's MAIN diffuse term sums lightData.parallel[0] (c20-21) + parallel[1]
			// (c22-23) -- the FILL + BOUNCE directionals, NOT the sun (the sun is in
			// parallelSpecular[0]/dot3). GAP-5 never filled parallel[] -> the VS diffuse was 0 ->
			// surfaces lit by fill/bounce went black. Feed the diffuse-sorted directionals captured
			// in setLights (direction negated to toward-light, mirroring D3D9 applyLights_vertexShader
			// :638-647 `parallel.direction = -getObjectFrameK_w()`). Independent of d3.valid (the
			// sun): a scene can have fill/bounce without the sun winning the dot3 path.
			for (int pi = 0; pi < 2; ++pi)
			{
				DirectX::XMFLOAT4 const &pd = d3.parallelDirectionWorld[pi];
				s_slot0Shadow.lightData[4 + pi * 2] = DirectX::XMFLOAT4(-pd.x, -pd.y, -pd.z, 0.0f);
				s_slot0Shadow.lightData[5 + pi * 2] = d3.parallelDiffuseColor[pi];
			}
		}

		// Point-light VS lane (2026-06-11, blue-NPC-face fix; supersedes the Plan 17-09
		// k0=1-only stub): feed the five point-light terms the world VS sums into its
		// main diffuse (v1) -- pointSpecular[0] @ lightData[8-11] (c24-27) + point[0..3]
		// @ lightData[12-23] (c28-39, PointSpecularData=4 regs, PointData=3 regs).
		// Previously ONLY the unused-slot k0=1 NaN guards were written ("we never fill
		// point lights"), so interior scenes lit by warm POINT fixtures (cantina) lost
		// all point terms: v1 = ambient+fills (cool blue), and a near-white face albedo
		// showed it raw -> the blue NPC face (D3D9 includes the points -> pink).
		// Positions are WORLD space (the VS subtracts them from the world-space vertex,
		// instr 15/54/66/78 of the asset VS) -- direct copy, no transform. The snapshot
		// carries the (1,0,0,0) attenuation divide-guard for unused slots, preserving
		// the Plan 17-09 NaN fix (1/dp4 >= 1, colors zero).
		{
			Direct3d11_PixelDot3State const &d3p = Direct3d11_LightManager::getPixelDot3State();
			s_slot0Shadow.lightData[8]  = d3p.pointSpecPosition;
			s_slot0Shadow.lightData[9]  = d3p.pointSpecDiffuse;
			s_slot0Shadow.lightData[10] = d3p.pointSpecAttenuation;
			s_slot0Shadow.lightData[11] = d3p.pointSpecSpecular;
			for (int pi = 0; pi < 4; ++pi)
			{
				s_slot0Shadow.lightData[12 + pi * 3] = d3p.pointPosition[pi];
				s_slot0Shadow.lightData[13 + pi * 3] = d3p.pointDiffuse[pi];
				s_slot0Shadow.lightData[14 + pi * 3] = d3p.pointAttenuation[pi];
			}
		}

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

		// Plan 11-09.15 Iter-35: log every setVSConstantsRaw call for the
		// first 100 to see if the engine pushes WVP/W matrices directly via
		// this register API. Iter-34 cbuffer dump showed c0..c7 always
		// identity -- if the engine pushes matrices via THIS path, but the
		// composeSlot0Shadow path overwrites them on next per-object setter
		// call (clobbering them back to composed-from-shadow values), we'd
		// see the symptom we have. Dump register, count, and first 4 floats.
		{
			static int s_iter35Count = 0;
			if (s_iter35Count < 100)
			{
				++s_iter35Count;
				if (ID3D11InfoQueue *iq35 = Direct3d11_Device::getInfoQueue())
				{
					float const *f = static_cast<float const *>(data);
					char buf35[256];
					_snprintf_s(buf35, sizeof(buf35), _TRUNCATE,
						"Plan 11-09.15 Iter-35 setVSConstantsRaw#%d reg=%d count=%d "
						"first4=[%.3f %.3f %.3f %.3f]",
						s_iter35Count, registerIndex, count,
						f[0], f[1], f[2], f[3]);
					iq35->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, buf35);
				}
			}
		}
	}

	// Plan 11-09 Iter-2.7 Fix C: lazy upload of the slot 0 shadow once per
	// applyPreDrawState. Replaces Plan 11-08's first-draw race guard +
	// setObjectToWorldTransformAndScale-canonical-upload pattern. If no
	// setter has touched the shadow since the last flush, this is a no-op.
	void flushSlot0IfDirty()
	{
		if (!s_slot0Dirty)
			return;
		// Plan 11-09.15 Iter-9 diagnostic: sample the c9 viewportData slot
		// at flush time so we can confirm the value actually uploaded to
		// the GPU. c9 is at byte offset 144 of s_slot0Shadow.
		//
		// Plan 11-09.15 Iter-34: ALSO dump c0..c7 (WVP at c0..c3 +
		// objectWorldMatrix at c4..c7). World VSes (a_specmap_bump_*,
		// a_simple_*, etc.) consume these matrices to transform local-space
		// verts to clip space. The Iter-26/33B WORLD logger showed all
		// world draws collapse to a radial fan = bad WVP math = stale or
		// wrong matrix in the cbuffer at upload time. Dumping c0..c7 for
		// the first 20 flushes (covers first few frames of char-select)
		// reveals what we're actually uploading.
		{
			static int s_flushCount = 0;
			++s_flushCount;
			bool const early  = (s_flushCount <= 5);
			bool const sample = (s_flushCount > 5) && ((s_flushCount % 1000) == 0);
			if (early || sample)
			{
				float const *c9 = reinterpret_cast<float const *>(
					reinterpret_cast<uint8_t const *>(&s_slot0Shadow) + 144);
				char buf[256];
				_snprintf_s(buf, sizeof(buf), _TRUNCATE,
					"Plan 11-09.15 Iter-9 flushSlot0#%d c9=(%.6f, %.6f, %.6f, %.6f) "
					"shadowSize=%zu",
					s_flushCount, c9[0], c9[1], c9[2], c9[3], sizeof(s_slot0Shadow));
				if (ID3D11InfoQueue *iq = Direct3d11_Device::getInfoQueue())
					iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, buf);
			}
			// Iter-34: dump c0..c7 (WVP + worldMatrix) per flush, first 20
			// + every 200th. Each matrix is 16 floats; write 4 floats per
			// line to keep buffer size reasonable.
			//
			// Plan 11-09.15 Iter-37A: also dump the current bound VS
			// filename and active static-shader template name. Iter-36D2
			// confirmed in-world SBSSP transforms are healthy, so the 41%
			// identity-W flushes seen at Iter-34 must come from a different
			// render family. Logging VS+template per flush lets us correlate
			// identity-W cbuffer entries to the responsible engine path
			// (batch skeletal, terrain, particles, attachments, etc.) so we
			// know which prepareToDraw to instrument next. ms_currentVSData
			// is updated when the shader binds (well before applyPreDrawState
			// calls flushSlot0IfDirty), so it reflects THIS draw's VS.
			static int s_iter34Count = 0;
			bool const i34_early  = (s_iter34Count < 20);
			bool const i34_sample = (s_iter34Count >= 20) && (s_flushCount % 200 == 0);
			if (i34_early || i34_sample)
			{
				++s_iter34Count;
				float const *c0 = reinterpret_cast<float const *>(&s_slot0Shadow);
				char const *vsFn34 = "<none>";
				if (ms_currentVSData)
				{
					ShaderImplementationPassVertexShader const *eng34 =
						ms_currentVSData->getEngineShader();
					if (eng34) vsFn34 = eng34->getFilename();
				}
				char const * const sht34 =
					Direct3d11_StaticShaderData::getActiveStaticShaderName();
				if (ID3D11InfoQueue *iq34 = Direct3d11_Device::getInfoQueue())
				{
					char buf34[1024];
					_snprintf_s(buf34, sizeof(buf34), _TRUNCATE,
						"Plan 11-09.15 Iter-34 slot0#%d (flush#%d) sht='%s' VS='%s' | "
						"WVP c0=[%.3f %.3f %.3f %.3f] c1=[%.3f %.3f %.3f %.3f] "
						"c2=[%.3f %.3f %.3f %.3f] c3=[%.3f %.3f %.3f %.3f] | "
						"W c4=[%.3f %.3f %.3f %.3f] c5=[%.3f %.3f %.3f %.3f] "
						"c6=[%.3f %.3f %.3f %.3f] c7=[%.3f %.3f %.3f %.3f]",
						s_iter34Count, s_flushCount, sht34, vsFn34,
						c0[ 0], c0[ 1], c0[ 2], c0[ 3],
						c0[ 4], c0[ 5], c0[ 6], c0[ 7],
						c0[ 8], c0[ 9], c0[10], c0[11],
						c0[12], c0[13], c0[14], c0[15],
						c0[16], c0[17], c0[18], c0[19],
						c0[20], c0[21], c0[22], c0[23],
						c0[24], c0[25], c0[26], c0[27],
						c0[28], c0[29], c0[30], c0[31]);
					iq34->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, buf34);
				}
			}

			// Plan 11-09.15 Iter-38A: WVP composition + multiply-convention
			// diagnostic. Both CODEX and Cursor closed the row/column-major
			// packing hypothesis (D3DCOMPILE_PACK_MATRIX_ROW_MAJOR + row_major
			// bytecode confirmed). Stale-CSO cache hypothesis ruled out by
			// smoke after stage/stage/shader-cache purge (same broken visual).
			// Remaining candidates: WVP composition order, or V/P input bug.
			//
			// Dumps per sampled flush:
			//   - V, P, W as separate matrices (so D3D9 baseline can be diffed)
			//   - WVP_current = (P*V)*W (current composeSlot0Shadow output)
			//   - WVP_alt     = (W*V)*P (alternate composition)
			//   - For a canonical head-height test point (0, 1.7, 0, 1):
			//     * clip_PVW_col: WVP_current applied as column-vector right-mult
			//     * clip_PVW_row: WVP_current applied as row-vector left-mult
			//     * clip_WVP_col: WVP_alt     applied as column-vector right-mult
			//     * clip_WVP_row: WVP_alt     applied as row-vector left-mult
			//
			// The interpretation that produces clip.w POSITIVE for an in-front
			// vertex tells us the correct composition + convention pair. Iter-37A
			// post-smoke math showed clip.w going catastrophically negative under
			// (P*V*W column-vector) for the player at head height; this iter
			// directly empirically tests the alternatives.
			//
			// Sampling: ONLY when V is non-identity (= world rendering phase,
			// not UI/billboard phase). Iter-38A-take-1 sampled by flush-count
			// modulo and ALL 372 captures landed on identity V/P/W -- the
			// engine resets V/P to identity during UI/billboard draws (which
			// use XYZRHW pre-transformed verts), and every 500th flush
			// systematically aligned with UI/billboard bursts. Iter-34 (which
			// reads s_slot0Shadow at different flushes) saw non-identity WVP
			// from world draws between those bursts. So the gate has to be
			// content-based: only sample when V indicates a real world
			// camera. Take the first 8 such samples, then every 200th
			// thereafter, capped at 30 total.
			{
				static int s_iter38aCount  = 0;
				static int s_iter38aSeen   = 0;
				// "World render phase" predicate: V is non-identity (camera
				// has been set). Cheap byte-compare against the identity init.
				bool const v_is_identity =
					s_cachedView._11 == 1.0f && s_cachedView._12 == 0.0f &&
					s_cachedView._13 == 0.0f && s_cachedView._14 == 0.0f &&
					s_cachedView._21 == 0.0f && s_cachedView._22 == 1.0f &&
					s_cachedView._44 == 1.0f;
				bool const worldPhase = !v_is_identity;
				if (worldPhase) ++s_iter38aSeen;
				bool const i38_early  = worldPhase && (s_iter38aCount < 8);
				bool const i38_sample = worldPhase && (s_iter38aCount < 30)
					&& (s_iter38aCount >= 8) && (s_iter38aSeen % 200 == 0);
				if (i38_early || i38_sample)
				{
					++s_iter38aCount;
					if (ID3D11InfoQueue *iq38 = Direct3d11_Device::getInfoQueue())
					{
						char const *vsFn38 = "<none>";
						if (ms_currentVSData)
						{
							ShaderImplementationPassVertexShader const *eng38 =
								ms_currentVSData->getEngineShader();
							if (eng38) vsFn38 = eng38->getFilename();
						}
						char const * const sht38 =
							Direct3d11_StaticShaderData::getActiveStaticShaderName();

						// Header.
						char hdr[256];
						_snprintf_s(hdr, sizeof(hdr), _TRUNCATE,
							"Plan 11-09.15 Iter-38A sample#%d (flush#%d) sht='%s' VS='%s'",
							s_iter38aCount, s_flushCount, sht38, vsFn38);
						iq38->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, hdr);

						// Build alt composition.
						DirectX::XMMATRIX Vm  = DirectX::XMLoadFloat4x4(&s_cachedView);
						DirectX::XMMATRIX Pm  = DirectX::XMLoadFloat4x4(&s_cachedProj);
						DirectX::XMMATRIX Wm  = DirectX::XMLoadFloat4x4(&s_cachedWorld);
						DirectX::XMMATRIX cur = DirectX::XMMatrixMultiply(
							DirectX::XMMatrixMultiply(Pm, Vm), Wm);          // (P*V)*W
						DirectX::XMMATRIX alt = DirectX::XMMatrixMultiply(
							DirectX::XMMatrixMultiply(Wm, Vm), Pm);          // (W*V)*P
						DirectX::XMFLOAT4X4 curM, altM;
						DirectX::XMStoreFloat4x4(&curM, cur);
						DirectX::XMStoreFloat4x4(&altM, alt);

						auto dumpMatrix = [&](char const *label,
							DirectX::XMFLOAT4X4 const &M)
						{
							char buf[512];
							_snprintf_s(buf, sizeof(buf), _TRUNCATE,
								"Plan 11-09.15 Iter-38A   %s "
								"r0=[%.3f %.3f %.3f %.3f] r1=[%.3f %.3f %.3f %.3f] "
								"r2=[%.3f %.3f %.3f %.3f] r3=[%.3f %.3f %.3f %.3f]",
								label,
								M._11, M._12, M._13, M._14,
								M._21, M._22, M._23, M._24,
								M._31, M._32, M._33, M._34,
								M._41, M._42, M._43, M._44);
							iq38->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, buf);
						};

						dumpMatrix("V        ", s_cachedView);
						dumpMatrix("P        ", s_cachedProj);
						dumpMatrix("W        ", s_cachedWorld);
						dumpMatrix("WVP(cur) ", curM);
						dumpMatrix("WVP(alt) ", altM);

						// Compute 4 clip vectors for test point (head-height
						// vertex above model origin = vertex you'd expect to
						// be in front of camera & visible).
						float const px = 0.0f, py = 1.7f, pz = 0.0f, pw = 1.0f;
						auto clipColMul = [&](DirectX::XMFLOAT4X4 const &M)
						{
							// clip[i] = sum_j M[i][j] * pos[j]  (column-vector right-mult)
							return DirectX::XMFLOAT4(
								M._11*px + M._12*py + M._13*pz + M._14*pw,
								M._21*px + M._22*py + M._23*pz + M._24*pw,
								M._31*px + M._32*py + M._33*pz + M._34*pw,
								M._41*px + M._42*py + M._43*pz + M._44*pw);
						};
						auto clipRowMul = [&](DirectX::XMFLOAT4X4 const &M)
						{
							// clip[j] = sum_i pos[i] * M[i][j]  (row-vector left-mult)
							return DirectX::XMFLOAT4(
								px*M._11 + py*M._21 + pz*M._31 + pw*M._41,
								px*M._12 + py*M._22 + pz*M._32 + pw*M._42,
								px*M._13 + py*M._23 + pz*M._33 + pw*M._43,
								px*M._14 + py*M._24 + pz*M._34 + pw*M._44);
						};
						DirectX::XMFLOAT4 clipPVWcol = clipColMul(curM);
						DirectX::XMFLOAT4 clipPVWrow = clipRowMul(curM);
						DirectX::XMFLOAT4 clipWVPcol = clipColMul(altM);
						DirectX::XMFLOAT4 clipWVProw = clipRowMul(altM);

						auto dumpClip = [&](char const *label,
							DirectX::XMFLOAT4 const &c)
						{
							char buf[256];
							_snprintf_s(buf, sizeof(buf), _TRUNCATE,
								"Plan 11-09.15 Iter-38A   %s = [%.4f %.4f %.4f %.4f]  w_sign=%s",
								label, c.x, c.y, c.z, c.w,
								(c.w > 0.0f ? "POSITIVE" : "negative"));
							iq38->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, buf);
						};

						char tpHdr[128];
						_snprintf_s(tpHdr, sizeof(tpHdr), _TRUNCATE,
							"Plan 11-09.15 Iter-38A   testpt = (%.3f, %.3f, %.3f, %.3f)",
							px, py, pz, pw);
						iq38->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, tpHdr);
						dumpClip("clip_PVW_col (current,    column-vec right-mul)", clipPVWcol);
						dumpClip("clip_PVW_row (current,    row-vec    left-mul )", clipPVWrow);
						dumpClip("clip_WVP_col (W*V*P alt,  column-vec right-mul)", clipWVPcol);
						dumpClip("clip_WVP_row (W*V*P alt,  row-vec    left-mul )", clipWVProw);
					}
				}
			}
		}
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

		// Plan 11-09.15 Iter-32A: ENABLE alpha-blend by default (SrcAlpha /
		// InvSrcAlpha — the standard "over" composite). Iter-31B chain
		// proved the font atlas IS correctly loaded as DXT5 with proper
		// alpha-mask glyphs (white RGB + varying alpha) -- the bug is that
		// Direct3d11_ShaderImplementationData::apply() is a documented
		// no-op (TextureData.cpp:91-120 confirms), so the engine's per-pass
		// m_alphaBlendEnable=true on `uicanvas_filtered.sht` never reaches
		// D3D11. Without blending, alpha-mask glyphs render as solid color
		// quads (the Iter-29A symptom). Globally enabling alpha-blend is
		// the smoke test before Iter-32B wires it properly per-pass via
		// Direct3d11_StaticShaderData::apply.
		//
		// Safety: opaque draws output alpha=1, so SrcAlpha=1 + InvSrcAlpha=0
		// yields the source pixel unchanged (no blending effect). UI assets
		// with proper vColor.a and texture alpha get correct compositing.
		//
		// Plan 11-09.15 Iter-39A TEST: flipping BlendEnable=TRUE -> FALSE
		// to confirm the Iter-32A safety assumption was wrong. Post-Iter-38B
		// (transpose fix), world geometry renders correctly but appears
		// washed/translucent -- world textures must NOT be outputting
		// alpha=1 as Iter-32A assumed. With BlendEnable=FALSE the world
		// should render opaque correctly, UI fonts will regress to
		// invisible (Iter-29A symptom), and the char-select preview avatar
		// should become visible (no longer multiplied to ~zero by texture
		// alpha). Iter-39B will then implement per-pass blend state in
		// Direct3d11_ShaderImplementationData::apply() so the engine's
		// per-shader m_alphaBlendEnable reaches D3D11 and UI works again.
		ms_bsDesc.AlphaToCoverageEnable = FALSE;
		ms_bsDesc.IndependentBlendEnable = FALSE;
		for (auto &rt : ms_bsDesc.RenderTarget)
		{
			rt.BlendEnable           = FALSE;   // Iter-39A test (was TRUE in Iter-32A)
			rt.SrcBlend              = D3D11_BLEND_SRC_ALPHA;
			rt.DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
			rt.BlendOp               = D3D11_BLEND_OP_ADD;
			rt.SrcBlendAlpha         = D3D11_BLEND_ONE;
			rt.DestBlendAlpha        = D3D11_BLEND_INV_SRC_ALPHA;
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

	// Plan 11-09.15 (CODEX Bucket A): lazy-rebuild reusable fan-to-list IB.
	// Returns true if ms_triangleFanIB can serve a fan with `vertCount`
	// vertices; false on cap-overflow or CreateBuffer failure (caller skips
	// the draw). CODEX Q2: R16_UINT; cap 65535; doubling growth from initial
	// capacity 1024.
	bool ensureTriangleFanIB(int vertCount)
	{
		if (vertCount > kTriangleFanIBMaxCapacityVerts)
		{
			DEBUG_FATAL(true, ("Direct3d11_StateCache::ensureTriangleFanIB: requested %d verts > R16_UINT cap %d; Plan 11-09.15 scope CODEX Q2 hard guard.\n",
				vertCount, kTriangleFanIBMaxCapacityVerts));
			DEBUG_REPORT_LOG_PRINT(true,
				("Direct3d11_StateCache::ensureTriangleFanIB: requested %d verts > R16_UINT cap %d; skipping draw.\n",
				vertCount, kTriangleFanIBMaxCapacityVerts));
			return false;
		}

		if (ms_triangleFanIB && vertCount <= ms_triangleFanIBCapacityVerts)
			return true;  // cache hit

		// Compute new capacity (doubling growth from current or initial, up to cap).
		int newCapacity = ms_triangleFanIBCapacityVerts > 0
			? ms_triangleFanIBCapacityVerts
			: kTriangleFanIBInitialCapacityVerts;
		while (newCapacity < vertCount)
		{
			newCapacity *= 2;
			if (newCapacity > kTriangleFanIBMaxCapacityVerts)
				newCapacity = kTriangleFanIBMaxCapacityVerts;
		}

		int const indexCount = (newCapacity - 2) * 3;
		std::vector<uint16_t> indices;
		indices.reserve(static_cast<size_t>(indexCount));
		for (int i = 1; i <= newCapacity - 2; ++i)
		{
			indices.push_back(0);
			indices.push_back(static_cast<uint16_t>(i));
			indices.push_back(static_cast<uint16_t>(i + 1));
		}

		D3D11_BUFFER_DESC bd = {};
		bd.ByteWidth      = static_cast<UINT>(indexCount * sizeof(uint16_t));
		bd.Usage          = D3D11_USAGE_IMMUTABLE;
		bd.BindFlags      = D3D11_BIND_INDEX_BUFFER;
		bd.CPUAccessFlags = 0;
		bd.MiscFlags      = 0;

		D3D11_SUBRESOURCE_DATA srd = {};
		srd.pSysMem          = indices.data();
		srd.SysMemPitch      = 0;
		srd.SysMemSlicePitch = 0;

		ComPtr<ID3D11Buffer> newIB;
		HRESULT const hr = Direct3d11_Device::getDevice()->CreateBuffer(&bd, &srd, newIB.GetAddressOf());
		if (FAILED(hr))
		{
			DEBUG_REPORT_LOG_PRINT(true,
				("Direct3d11_StateCache::ensureTriangleFanIB: CreateBuffer failed (cap=%d, %d indices): %s\n",
				newCapacity, indexCount, Direct3d11Namespace::hresultString(hr)));
			// Leave ms_triangleFanIB at previous (possibly null) value; next
			// call retries. Don't FATAL -- a transient CreateBuffer failure
			// shouldn't kill the client, just drop the fan draw.
			return false;
		}

		ms_triangleFanIB = newIB;
		ms_triangleFanIBCapacityVerts = newCapacity;
		++ms_triangleFanIBRebuilds;
		return true;
	}

	// Plan 11-09.15 Iter-27: reusable quad-list IB; lazy doubling growth.
	// Mirrors ensureTriangleFanIB. Quad N indices pattern:
	//   (4N+0, 4N+1, 4N+2, 4N+0, 4N+2, 4N+3) -- 6 indices per quad,
	// matching D3D9's per-quad expansion at Direct3d9.cpp:4277.
	bool ensureQuadListIB(int numQuads)
	{
		if (numQuads > kQuadListIBMaxCapacityQuads)
		{
			DEBUG_FATAL(true, ("Direct3d11_StateCache::ensureQuadListIB: requested %d quads > R16_UINT cap %d.\n",
				numQuads, kQuadListIBMaxCapacityQuads));
			DEBUG_REPORT_LOG_PRINT(true,
				("Direct3d11_StateCache::ensureQuadListIB: requested %d quads > R16_UINT cap %d; skipping draw.\n",
				numQuads, kQuadListIBMaxCapacityQuads));
			return false;
		}

		if (ms_quadListIB && numQuads <= ms_quadListIBCapacityQuads)
			return true;  // cache hit

		int newCapacity = ms_quadListIBCapacityQuads > 0
			? ms_quadListIBCapacityQuads
			: kQuadListIBInitialCapacityQuads;
		while (newCapacity < numQuads)
		{
			newCapacity *= 2;
			if (newCapacity > kQuadListIBMaxCapacityQuads)
				newCapacity = kQuadListIBMaxCapacityQuads;
		}

		int const indexCount = newCapacity * 6;
		std::vector<uint16_t> indices;
		indices.reserve(static_cast<size_t>(indexCount));
		for (int q = 0; q < newCapacity; ++q)
		{
			uint16_t const base = static_cast<uint16_t>(q * 4);
			indices.push_back(static_cast<uint16_t>(base + 0));
			indices.push_back(static_cast<uint16_t>(base + 1));
			indices.push_back(static_cast<uint16_t>(base + 2));
			indices.push_back(static_cast<uint16_t>(base + 0));
			indices.push_back(static_cast<uint16_t>(base + 2));
			indices.push_back(static_cast<uint16_t>(base + 3));
		}

		D3D11_BUFFER_DESC bd = {};
		bd.ByteWidth      = static_cast<UINT>(indexCount * sizeof(uint16_t));
		bd.Usage          = D3D11_USAGE_IMMUTABLE;
		bd.BindFlags      = D3D11_BIND_INDEX_BUFFER;
		bd.CPUAccessFlags = 0;
		bd.MiscFlags      = 0;

		D3D11_SUBRESOURCE_DATA srd = {};
		srd.pSysMem          = indices.data();
		srd.SysMemPitch      = 0;
		srd.SysMemSlicePitch = 0;

		ComPtr<ID3D11Buffer> newIB;
		HRESULT const hr = Direct3d11_Device::getDevice()->CreateBuffer(&bd, &srd, newIB.GetAddressOf());
		if (FAILED(hr))
		{
			DEBUG_REPORT_LOG_PRINT(true,
				("Direct3d11_StateCache::ensureQuadListIB: CreateBuffer failed (cap=%d, %d indices): %s\n",
				newCapacity, indexCount, Direct3d11Namespace::hresultString(hr)));
			return false;
		}

		ms_quadListIB = newIB;
		ms_quadListIBCapacityQuads = newCapacity;
		++ms_quadListIBRebuilds;
		return true;
	}

	// ------------------------------------------------------------------
	// Plan 11-09.13 Iter-4: dispatch through the per-VS dynamic PS cache.
	// getOrCompilePSForVS lazily generates a PS whose input struct
	// mirrors the VS's reflected output struct register-for-register
	// (D3D11 stage linkage matches by semantic AND hardware register
	// position; Iter-3's static Variant T was register-position-limited
	// and produced 65K id=343 errors per session for VSes whose TEXCOORD0
	// wasn't at o0). On null return (compile-failure tombstone or empty
	// reflection data), fall back to Variant M magenta so geometry still
	// surfaces visibly.
	//
	// Plan 11-09.14 (CODEX correction #2): the previous Iter-4 first-dynamic-
	// PS-bind diagnostic was REMOVED here. PSGetShaderResources/PSGetSamplers
	// inside selectFallbackPSForVS read DEVICE state, but
	// PSSetShaderResources/PSSetSamplers don't run until later in
	// applyPreDrawState's flush block (StateCache.cpp:672-673). So the
	// diagnostic always saw the previous draw's state, not the state for the
	// upcoming draw -- it reported sampler0=NULL on the very first per-VS
	// dynamic-PS bind even though the install-time bootstrap pre-fills
	// ms_boundSampler[] with ms_defaultSampler.Get(). Replaced by the
	// lifetime counters logged at Direct3d11_StaticShaderData::remove() +
	// Direct3d11_StateCache::remove() shutdown.

	ID3D11PixelShader *selectFallbackPSForVS(Direct3d11_VertexShaderData const *vsData, uint32 textureCoordinateSetKey)
	{
		ID3D11PixelShader *variantM = Direct3d11_PixelShaderProgramData::getFallbackPS();
		if (!vsData)
			return variantM;

		// Walls fix 2026-06-11: ms_boundSRV[0] is the SHADOW state for the
		// UPCOMING draw (StaticShaderData::apply wrote it before the draw
		// call reached applyPreDrawState; the flush below at step 7 uploads
		// it). Note this is the shadow, NOT a PSGetShaderResources device
		// read -- the Plan 11-09.14 staleness caveat above applied to device
		// reads only. Textured draws get the sampling variant; untextured
		// draws get the vertex-color variant (FFP: no texture -> diffuse).
		//
		// Detail-blend fix 2026-06-11: forward the active pass's FFP combiner
		// descriptor (set/cleared per-pass by StaticShaderData::apply) so FFP
		// stage-based passes get the full multi-texture cascade PS.
		ID3D11PixelShader *perVS = Direct3d11_PixelShaderProgramData::getOrCompilePSForVS(
			vsData, textureCoordinateSetKey, ms_boundSRV[0] != nullptr,
			ms_ffpCombinerValid ? &ms_ffpCombiner : nullptr);
		if (!perVS)
			return variantM;   // tombstone -> magenta safety net

		return perVS;
	}

	// ------------------------------------------------------------------
	// Plan 17-07 (Round-4 MEDIUM 'success metric inflation'): bind-path
	// attribution log emitted at the PSSetShader call site, AFTER psToBind is
	// decided. Distinct from the COMPATIBLE/INCOMPATIBLE validator logs -- this
	// proves WHICH lane (native asset PS / rewritten asset PS / dynamic fallback)
	// actually delivered each bind. Plan 17-05 Task 4 greps `asset-PS bound=`
	// (>= 8) vs `fallback-PS bound=` (<= 1) on Kenny's POST-gap boot. Deduped per
	// (VS*, PS*, path) triple so the log doesn't flood per draw; dual-route
	// (DEBUG_REPORT_LOG_PRINT + ID3D11InfoQueue file sink) mirroring 17-02/17-04.
	void emitPlan1707BindAttribution(
		Direct3d11_VertexShaderData       const *vsData,
		Direct3d11_PixelShaderProgramData const *psData,
		ID3D11PixelShader                       *psToBind,
		char const                              *bindPath)
	{
		typedef std::pair<void const *, void const *> VsPsKey;
		// Encode the path in the key's third dimension by partitioning into two
		// dedupe sets keyed on (VS, PS) -- one for asset binds, one for fallback.
		static std::set<VsPsKey> s_assetLogged;
		static std::set<VsPsKey> s_fallbackLogged;

		bool const isFallback = (bindPath && std::strcmp(bindPath, "fallback") == 0);
		VsPsKey const key(static_cast<void const *>(vsData), static_cast<void const *>(psToBind));
		std::set<VsPsKey> &logged = isFallback ? s_fallbackLogged : s_assetLogged;
		if (logged.find(key) != logged.end())
			return;
		logged.insert(key);

		char const *shaderName = psData ? psData->getFileName() : "?";

		char msg[512];
		if (isFallback)
			_snprintf_s(msg, sizeof(msg), _TRUNCATE,
				"Plan 17-07 fallback-PS bound= ptr=0x%p vs=0x%p shader='%s'",
				static_cast<void const *>(psToBind), static_cast<void const *>(vsData), shaderName);
		else
			_snprintf_s(msg, sizeof(msg), _TRUNCATE,
				"Plan 17-07 asset-PS bound= ptr=0x%p vs=0x%p ps=0x%p (path=%s) shader='%s'",
				static_cast<void const *>(psToBind), static_cast<void const *>(vsData),
				static_cast<void const *>(psData), bindPath ? bindPath : "?", shaderName);

		if (ID3D11InfoQueue *iq = Direct3d11_Device::getInfoQueue())
			iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, msg);
		DEBUG_REPORT_LOG_PRINT(true, ("%s\n", msg));
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

		// Plan 17-09: resolve the per-key VS variant ONCE (cheap cached lookup
		// after first compile) and pass it EXPLICITLY into layout creation -- the
		// layout cache never reads the ambient key.
		Direct3d11_VertexShaderData::Variant const &vsVariant =
			ms_currentVSData->getVariant(ms_currentVSKey);

		// Plan 11-09.7: multi-stream path takes precedence -- the engine
		// calls either setVertexBuffer OR setVertexBufferVector, and they
		// clear each other's state.
		if (ms_currentVBVectorActive)
		{
			ms_currentInputLayout = Direct3d11_VertexDeclarationMap::getOrCreateMultiStream(
				ms_currentVBVectorFormats,
				ms_currentVBVectorStreamCount,
				vsVariant);
			return vsVariant.d3dVS.Get() != nullptr;
		}

		if (!ms_currentVBValid)
			return vsVariant.d3dVS.Get() != nullptr;

		ms_currentInputLayout = Direct3d11_VertexDeclarationMap::getOrCreate(
			ms_currentVBFormat, vsVariant);
		return vsVariant.d3dVS.Get() != nullptr;
	}

	// ------------------------------------------------------------------
	// Apply pending bindings to the GPU pipeline before issuing the draw.
	// Order mirrors RESEARCH "Pre-draw setup" enumeration in 11-06-PLAN.md.

	// Phase 19 cell-shell diagnostic: thousands of draws/session are skipped
	// at the VS stage (skippedNullVS=12k-26k) with d3dVS null ("//asm VS or
	// compile failure"). Cell walls/floor are suspected to be among them ->
	// clear-only. Log, once per unique shader template, which shaders get
	// VS-skipped so we can name the cell-shell culprit. REVERT (set to 0).
#define P19_SKIP_AUDIT 1
#if P19_SKIP_AUDIT
	void p19LogSkipOnce(char const *reason)
	{
		static std::set<std::string> s_logged;
		char const *sn = Direct3d11_StaticShaderData::getActiveStaticShaderName();
		std::string key = std::string(reason) + "|" + (sn ? sn : "(null)");
		if (!s_logged.insert(key).second)
			return;
		char buf[320];
		_snprintf_s(buf, sizeof(buf), _TRUNCATE,
			"[P19SKIP] draw skipped (%s) shader='%s'", reason, sn ? sn : "(null)");
		if (ID3D11InfoQueue *iq = Direct3d11_Device::getInfoQueue())
			iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_WARNING, buf);
	}
#endif

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
#if P19_SKIP_AUDIT
			p19LogSkipOnce("resolveShaders-fail");
#endif
			return false;
		}
		ID3D11VertexShader *vs = ms_currentVSData->getVariant(ms_currentVSKey).d3dVS.Get();   // Plan 17-09: per-key variant
		if (!vs)
		{
			++ms_skippedDrawsNullVS;
#if P19_SKIP_AUDIT
			p19LogSkipOnce("VS-null-asm-or-compilefail");
#endif
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

		// 3. VB -- Plan 11-09.7 multi-stream path takes precedence.
		if (ms_currentVBVectorActive && ms_currentVBVectorStreamCount > 0)
		{
			ctx->IASetVertexBuffers(
				0,
				static_cast<UINT>(ms_currentVBVectorStreamCount),
				ms_currentVBVectorBuffers,
				ms_currentVBVectorStrides,
				ms_currentVBVectorOffsets);
		}
		else if (ms_currentVBValid && ms_currentVB)
		{
			ID3D11Buffer *vbs[1] = { ms_currentVB };
			UINT strides[1]      = { ms_currentVBStride };
			UINT offsets[1]      = { ms_currentVBOffset };
			ctx->IASetVertexBuffers(0, 1, vbs, strides, offsets);
		}

		// Plan 11-09.8: phantom zero buffer at slot 15. Backs reflection-
		// driven phantom-element InputSlot=15 entries that
		// VertexBufferDescriptorMap::augmentWithPhantomElements appended
		// for VS-declared inputs the VBFormat doesn't cover (vs_4_0
		// signature strictness). Stride=0 means every vertex reads the
		// same zero value. Cheaper to bind unconditionally than to gate
		// per-VS (CODEX 11-09.8 consult Q4); D3D11 is fine with unused-
		// but-bound slots.
		{
			ID3D11Buffer *phantomVB    = Direct3d11_Device::getPhantomZeroBuffer();
			UINT          phantomStride = 0;
			UINT          phantomOffset = 0;
			if (phantomVB)
				ctx->IASetVertexBuffers(15, 1, &phantomVB, &phantomStride, &phantomOffset);
		}

		// 4. IB (only for indexed draws -- caller passes topology that
		// implies indexed; we always bind if valid to avoid driver state).
		if (ms_currentIBValid && ms_currentIB)
			ctx->IASetIndexBuffer(ms_currentIB, ms_currentIBFormat, ms_currentIBOffset);

		// 5. VS + PS (Plan 11-09 Iter-2 / 11-09.13 Iter-3 / 17-04 Task 1:
		// VS<->PS signature pair validation gates the asset-PS bind so D3D11
		// register-position-strict linkage is honored. When incompatible,
		// route through selectFallbackPSForVS whose buildHlslForVSOutputs-built
		// PS mirrors the VS output signature register-for-register).
		ctx->VSSetShader(vs, nullptr, 0);

		// Plan 17-04 Task 1: validate the (VS, PS) pair signature compatibility
		// before letting the Plan 17-02-bound asset PS win. Pre-17-04 this gate
		// was implicit (any non-null ms_currentPSData->getPixelShader() won
		// unconditionally), which produced id=343 x 24K per char-select boot
		// because asset PS input register positions didn't match the VS output
		// the recompile lane assigned independently. isCompatibleWithVS returns
		// true for the safe-bind path (matched semantics + registers + types +
		// mask-subset) and false on any mismatch (route through the VS-matched
		// per-VS PS instead). See Direct3d11_PixelShaderProgramData::
		// isCompatibleWithVS for the validation contract + per-pair logging.
		// Plan 17-07 (HIGH-2 + Round-5 item 10): bind priority is
		// native-asset-PS > rewritten-cached-PS > dynamic-fallback-PS.
		ID3D11PixelShader *psToBind = nullptr;
		char const *bindPath = "none";
		// Phase 19: when the VS is the generic //asm FALLBACK (isAssembly), it
		// emits only SV_Position + TEXCOORD0 + const-white COLOR0 -- NOT the
		// asm-computed varyings the asset PS expects. Skip the native/rewritten
		// asset-PS lanes entirely and route straight to selectFallbackPSForVS,
		// whose buildHlslForVSOutputs PS mirrors the fallback VS output sig
		// (textured-passthrough). Binding an asset PS here would read undeclared
		// varyings -> wrong/magenta. See Direct3d11_VertexShaderData::isAssembly.
		bool const vsIsFallback = ms_currentVSData && ms_currentVSData->isAssembly();
		if (vsIsFallback)
		{
			// fall through to the fallback-PS selection block below
		}
		else if (ms_currentPSData && ms_currentPSData->getPixelShader()
			&& Direct3d11_PixelShaderProgramData::isCompatibleWithVS(ms_currentVSData, ms_currentVSKey, ms_currentPSData))
		{
			// 1. Native asset PS validated by the unchanged ctor-time validator.
			psToBind = ms_currentPSData->getPixelShader();
			bindPath = "native";
		}
		else if (ms_currentPSData)
		{
			// 2. Plan 17-07: native asset PS was INCOMPATIBLE (register-position
			// mismatch). Try the per-VS rewritten cache (the retained //hlsl PSRC
			// main() parameter list reordered into the bound VS's output-register
			// layout) BEFORE the dynamic fallback. Validate the rewritten PS via
			// its OWN per-VS reflected inputs (HIGH-6) so the native ctor-time
			// m_reflectedPSInputs cache doesn't false-reject it.
			std::pair<ID3D11PixelShader *, std::vector<Direct3d11_ReflectedPSInputSig> const *> const rewritten =
				ms_currentPSData->tryGetOrBuildRewrittenPSForVS(ms_currentVSData, ms_currentVSKey);
			if (rewritten.first && rewritten.second
				&& Direct3d11_PixelShaderProgramData::isCompatibleWithVS_withExplicitPSInputs(
					ms_currentVSData, ms_currentVSKey, *rewritten.second, static_cast<void const *>(rewritten.first)))
			{
				psToBind = rewritten.first;
				bindPath = "rewritten";
			}
		}
		if (!psToBind)
		{
			// 3. Plan 11-09.13 Iter-3 / 17-04 Task 1 safety net: VS-matched per-VS
			// PS (Variant T textured-passthrough) when the bound VS's reflected
			// output signature is stage-linkage compatible, else Variant M
			// (magenta). Owns the residual 0-1/9 pair the asset lane can't satisfy.
			if (ID3D11PixelShader *fallback = selectFallbackPSForVS(ms_currentVSData, ms_currentVSKey))
			{
				psToBind = fallback;
				bindPath = "fallback";
			}
		}
		if (psToBind)
		{
			ctx->PSSetShader(psToBind, nullptr, 0);
			// Plan 17-07 (Round-4 MEDIUM): prove WHICH lane delivered the bind --
			// `asset-PS bound=` (path=native|rewritten) vs `fallback-PS bound=`.
			// One-shot per (VS, PS, path); Plan 17-05 Task 4 greps these counts.
			emitPlan1707BindAttribution(ms_currentVSData, ms_currentPSData, psToBind, bindPath);
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
		Direct3d11_ConstantBuffer::bindPS(1);   // fog fix 2026-06-11: alphaTest/textureFactor/fogColor -- generated PSes AND the 17-07 asset wrapper read b1 now
		Direct3d11_ConstantBuffer::bindPS(2);

		// 7. SRVs + samplers (Pitfall 4 split)
		ctx->PSSetShaderResources(0, kMaxSRVs, ms_boundSRV);
		ctx->PSSetSamplers(0, kMaxSamplers, ms_boundSampler);
		if (ms_boundSRV[0])
			++ms_drawsWithSRV0Bound;

		// Plan 11-09.15 Iter-26: log ALL draws (transformed + non-transformed)
		// for the first applyPreDrawState invocations. Iter-25 only
		// captured isTransformed() draws and saw nothing but the post-FX
		// blit. The splash UI must be using non-transformed VBFormat
		// (clip-space verts) -- this iter captures every draw so we see
		// the full picture: topology, VS, isTransformed flag, SRV0, RTV.
		//
		// Plan 11-09.15 Iter-33: extended with shader template name and
		// bumped cap from 200 -> 500 to cover the world-content draw
		// routing investigation. UI rendering is resolved (Iter-32A); the
		// next frontier is in-world geometry showing skewed textures, which
		// flows through drawIndexedTriangleList / drawTriangleList / etc.
		// (NOT drawQuadList, so Iter-29B's UI-focused logger doesn't catch
		// it). All world draws still go through applyPreDrawState here, so
		// enhancing this single logger covers every draw path.
		//
		// Plan 11-09.15 Iter-33B: split cap into two counters -- one for
		// xform=1 (UI; XYZRHW pre-projected verts) and one for xform=0
		// (world content; goes through full WVP). The first 200-cap run
		// showed all xform=1 (UI), so world content draws AFTER the boot
		// UI sequence. Separate counters guarantee world content captures
		// regardless of when in the session it fires.
		//
		// Plan 11-09.15 Iter-37A: WORLD sampling switched from "first 500
		// only" to "first 500 + every 50th call thereafter, cap 2000". The
		// flat 500 cap exhausted entirely during char-select preview (all
		// 500 entries were Sullustan armor/clothing shaders), leaving zero
		// visibility into in-world VS/template diversity. Same pattern as
		// Iter-36D2 SBSSP logger which successfully captured both the
		// preview burst and post-preview in-world transforms.
		{
			static int s_iter26CountUI       = 0;   // xform=1, logged
			static int s_iter26CountWorld    = 0;   // xform=0, logged
			static int s_iter26CallsWorld    = 0;   // xform=0, total seen (Iter-37A sampling)
			bool const isXformed = ms_currentVBFormat.isTransformed();
			if (!isXformed) ++s_iter26CallsWorld;
			bool const worldEarly  = (!isXformed) && (s_iter26CountWorld < 500);
			bool const worldSample = (!isXformed) && (s_iter26CountWorld < 2000)
				&& (s_iter26CallsWorld % 50 == 0);
			bool const shouldLog =
				(isXformed && s_iter26CountUI < 500) ||
				worldEarly || worldSample;
			if (shouldLog)
			{
				if (isXformed) ++s_iter26CountUI; else ++s_iter26CountWorld;
				int const drawNum = isXformed ? s_iter26CountUI : s_iter26CountWorld;
				char const * const drawTag = isXformed ? "UI" : "WORLD";
				if (ID3D11InfoQueue *iq26 = Direct3d11_Device::getInfoQueue())
				{
					char const *vsFn = "<none>";
					if (ms_currentVSData)
					{
						ShaderImplementationPassVertexShader const *eng = ms_currentVSData->getEngineShader();
						if (eng) vsFn = eng->getFilename();
					}
					char const * const sht = Direct3d11_StaticShaderData::getActiveStaticShaderName();
					ID3D11RenderTargetView *cRTV26 = nullptr;
					ctx->OMGetRenderTargets(1, &cRTV26, nullptr);
					char buf26[512];
					_snprintf_s(buf26, sizeof(buf26), _TRUNCATE,
						"Plan 11-09.15 Iter-26/33B %s#%d topo=%d xform=%d sht='%s' VS='%s' "
						"SRV0=0x%p RTV=0x%p vertCount=%d",
						drawTag, drawNum, static_cast<int>(topology),
						isXformed ? 1 : 0,
						sht, vsFn,
						static_cast<void *>(ms_boundSRV[0]),
						static_cast<void *>(cRTV26),
						ms_currentVBVertexCount);
					iq26->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, buf26);
					if (cRTV26) cRTV26->Release();
				}
			}
		}

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
	// Phase 19 DIAGNOSTIC (P19_MAXLOD0): clamp to mip 0 only. Tests whether the
	// world corruption lives in the low mips (bad content OR not-yet-uploaded
	// streaming transient). Corruption gone -> mip chain implicated. REVERT.
// RESULT 2026-05-31: MaxLOD=0 = NO CHANGE -> mip chain EXONERATED (it's a flat
// per-frame cbuffer-garbage color, not a texture). Reverted.
#define P19_MAXLOD0 0
#if P19_MAXLOD0
	ssDesc.MaxLOD         = 0.0f;
#endif

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
	ms_drawsWithSRV0Bound = 0;  // Plan 11-09.14

	// Plan 11-09.15: reset fan counters + pre-allocate the fan IB at the
	// initial capacity so the first world-load drawTriangleFan doesn't stall
	// on CreateBuffer.
	ms_drawTriangleFanCalls               = 0;
	ms_drawPartialTriangleFanCalls        = 0;
	ms_triangleFanIBRebuilds              = 0;
	ms_triangleFanMaxVertices             = 0;
	ms_drawIndexedTriangleFanCalls        = 0;
	ms_drawPartialIndexedTriangleFanCalls = 0;
	ms_triangleFanIBCapacityVerts         = 0;
	ms_triangleFanIB.Reset();
	(void)ensureTriangleFanIB(kTriangleFanIBInitialCapacityVerts);

	// Plan 11-09.15 Iter-27: reset quad-list counters + pre-allocate the
	// quad-list IB at the initial capacity so the first splash draw
	// doesn't stall on CreateBuffer.
	ms_drawQuadListCalls                  = 0;
	ms_quadListIBRebuilds                 = 0;
	ms_quadListMaxQuads                   = 0;
	ms_quadListIBCapacityQuads            = 0;
	ms_quadListIB.Reset();
	(void)ensureQuadListIB(kQuadListIBInitialCapacityQuads);

	// Plan 11-09.15 Iter-2 follow-up: dump the first 12 indices of the fan IB
	// (covers 4 fan-list triangles) to verify the (0,i,i+1) construction
	// produces what we expect. Sanity check for the "radiating from world
	// point" artifact -- if the IB content is wrong, the fan triangulation
	// itself is broken. Emits via InfoQueue so it lands in stage/d3d11-debug.log.
	{
		// Re-derive the expected first-12 from our generator pattern. If
		// CreateBuffer cleanly consumed the std::vector<uint16_t> source, the
		// device IB should match these values exactly (we don't read back
		// from D3D11 -- that's an additional Map+CopySubresource step and
		// USAGE_IMMUTABLE doesn't allow it anyway).
		char buf[256];
		_snprintf_s(buf, sizeof(buf), _TRUNCATE,
			"Plan 11-09.15 Iter-2 fan IB at install cap=%d expected first-12 indices: "
			"(0,1,2) (0,2,3) (0,3,4) (0,4,5) -- if fan-rendered geometry shows "
			"radiating-from-apex pattern, the apex shared = vertex 0 of each fan VB.",
			ms_triangleFanIBCapacityVerts);
		if (ID3D11InfoQueue *iq = Direct3d11_Device::getInfoQueue())
			iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, buf);
	}
}

// ----------------------------------------------------------------------

void Direct3d11_StateCache::remove()
{
	// Plan 11-09.15 Iter-1 follow-up: also emit through InfoQueue so the
	// shutdown stats land in stage/d3d11-debug.log (DEBUG_REPORT_LOG_PRINT
	// is invisible from GUI-exe launches per Plan 11-09.13 Iter-3 finding).
	char shutdownBuf[768];
	_snprintf_s(shutdownBuf, sizeof(shutdownBuf), _TRUNCATE,
		"Direct3d11_StateCache: shutdown stats stateObjectCreates=%d skippedNullVS=%d skippedNullLayout=%d drawsWithSRV0Bound=%d "
		"drawTriangleFanCalls=%d drawPartialTriangleFanCalls=%d triangleFanIBRebuilds=%d triangleFanMaxVertices=%d "
		"drawIndexedTriangleFanCalls=%d drawPartialIndexedTriangleFanCalls=%d "
		"drawQuadListCalls=%d quadListIBRebuilds=%d quadListMaxQuads=%d",
		ms_stateObjectCreates, ms_skippedDrawsNullVS, ms_skippedDrawsNullLayout, ms_drawsWithSRV0Bound,
		ms_drawTriangleFanCalls, ms_drawPartialTriangleFanCalls, ms_triangleFanIBRebuilds, ms_triangleFanMaxVertices,
		ms_drawIndexedTriangleFanCalls, ms_drawPartialIndexedTriangleFanCalls,
		ms_drawQuadListCalls, ms_quadListIBRebuilds, ms_quadListMaxQuads);
	if (ID3D11InfoQueue *iq = Direct3d11_Device::getInfoQueue())
		iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, shutdownBuf);
	DEBUG_REPORT_LOG_PRINT(true, ("%s\n", shutdownBuf));

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
	ms_currentVSKey  = 0;   // Plan 17-09
	ms_currentPSData = nullptr;
	ms_currentVBValid = ms_currentIBValid = false;
	// Plan 11-09.7: clear multi-stream state too.
	ms_currentVBVectorActive = false;
	ms_currentVBVectorStreamCount = 0;
	for (int i = 0; i < kMaxVBStreams; ++i)
	{
		ms_currentVBVectorBuffers[i] = nullptr;
		ms_currentVBVectorStrides[i] = 0;
		ms_currentVBVectorOffsets[i] = 0;
		ms_currentVBVectorFormats[i] = nullptr;
	}
	ms_defaultSampler.Reset();

	// Plan 11-09.15: release fan IB.
	ms_triangleFanIB.Reset();
	ms_triangleFanIBCapacityVerts = 0;

	// Plan 11-09.15 Iter-27: release quad-list IB.
	ms_quadListIB.Reset();
	ms_quadListIBCapacityQuads = 0;

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
	// Plan 18-02 — half-pixel load-screen seam fix (D-01 CODEX+Cursor consult,
	// UNANIMOUS; .planning/research/CONSULT-18-half-texel-seam-SYNTHESIS.md). The
	// engine bakes CuiManager::ms_pixelOffset (-0.5) into every 2D XYZRHW vertex
	// (fingerprint (-0.5,-0.5)...(1023.5,767.5)). Those baked positions aligned under
	// D3D9's pre-D3D10 -0.5 texel rasterizer rule, but misalign under D3D11 center-
	// sampling -> the load-screen centerline seam plus a global half-pixel shift on
	// all 2D UI. D3D9 carried this compensation in the rasterizer; D3D11 must fold it
	// into this shared clip-map constant ONCE (D-02: central, not per-draw fudge).
	// Confirmed sign+magnitude: .z += 1/width, .w -= 1/height. A baked half-pixel maps
	// to 1/w in NDC because the c9 scale term is 2/w (half of that scale is one over
	// width); a smaller term would only cancel a quarter pixel and leave the seam.
	// Resolution-independent — derived from the live viewport width/height, no
	// hardcoded pixel constants. gl11-only: the D3D9 reference path is byte-for-byte
	// unchanged (D-04).
	float const halfPixelClipX = 1.0f / static_cast<float>(width);
	float const halfPixelClipY = 1.0f / static_cast<float>(height);
	float const viewportData[4] = {
		 2.0f / static_cast<float>(width),
		-2.0f / static_cast<float>(height),
		-1.0f - xOffset + halfPixelClipX,
		 1.0f + yOffset - halfPixelClipY
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

	// Plan 11-09.15 Iter-35: log first 30 setWorldToCameraTransform calls.
	// Iter-34 cbuffer dump showed c0..c7 always identity -- if THIS setter
	// fires regularly with non-identity input, the bug is in the upload
	// pipeline. If it never fires (or always with identity), the engine
	// uses a different mechanism.
	{
		static int s_iter35WtcCount = 0;
		if (s_iter35WtcCount < 30)
		{
			++s_iter35WtcCount;
			if (ID3D11InfoQueue *iq35 = Direct3d11_Device::getInfoQueue())
			{
				char buf35[384];
				_snprintf_s(buf35, sizeof(buf35), _TRUNCATE,
					"Plan 11-09.15 Iter-35 setWorldToCameraTransform#%d "
					"row0=[%.3f %.3f %.3f %.3f] row1=[%.3f %.3f %.3f %.3f] "
					"row2=[%.3f %.3f %.3f %.3f] camPos=[%.3f %.3f %.3f]",
					s_iter35WtcCount,
					static_cast<float>(m[0][0]), static_cast<float>(m[0][1]),
					static_cast<float>(m[0][2]), static_cast<float>(m[0][3]),
					static_cast<float>(m[1][0]), static_cast<float>(m[1][1]),
					static_cast<float>(m[1][2]), static_cast<float>(m[1][3]),
					static_cast<float>(m[2][0]), static_cast<float>(m[2][1]),
					static_cast<float>(m[2][2]), static_cast<float>(m[2][3]),
					cameraPosition.x, cameraPosition.y, cameraPosition.z);
				iq35->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, buf35);
			}
		}
	}

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

	// Plan 11-09.15 Iter-35: log first 30 setProjectionMatrix calls.
	{
		static int s_iter35ProjCount = 0;
		if (s_iter35ProjCount < 30)
		{
			++s_iter35ProjCount;
			if (ID3D11InfoQueue *iq35 = Direct3d11_Device::getInfoQueue())
			{
				float const *p = reinterpret_cast<float const *>(projectionMatrix.matrix);
				char buf35[384];
				_snprintf_s(buf35, sizeof(buf35), _TRUNCATE,
					"Plan 11-09.15 Iter-35 setProjectionMatrix#%d "
					"row0=[%.3f %.3f %.3f %.3f] row1=[%.3f %.3f %.3f %.3f] "
					"row2=[%.3f %.3f %.3f %.3f] row3=[%.3f %.3f %.3f %.3f]",
					s_iter35ProjCount,
					p[ 0], p[ 1], p[ 2], p[ 3],
					p[ 4], p[ 5], p[ 6], p[ 7],
					p[ 8], p[ 9], p[10], p[11],
					p[12], p[13], p[14], p[15]);
				iq35->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, buf35);
			}
		}
	}
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
	// CONSULT-39 (2026-06-08): same b0 zero-tail clobber as setAlphaFadeOpacity -- drop the apply()
	// cache so the dot3 block is re-uploaded on the next setStaticShader.
	Direct3d11_StaticShaderData::invalidateApplyCache();

	// Fog fix 2026-06-11: feed the VS fog constant c10 = {0, 0, density,
	// density^2} -- EXACT D3D9 parity (Direct3d9.cpp:3390-3391, VSCR_fog=10).
	// The reauthored //hlsl VSes compute the per-vertex EXP2 fog factor
	// 1/exp(d^2 * fog.w) (functions.inc calculateFog); pre-fix c10 was NEVER
	// written ("zero for now") so every VS emitted factor 1.0 = no fog.
	// Zeroed when disabled -> factor stays 1.0 (D3D9 just flips FOGENABLE;
	// the PS-side enable gate in b1 fogColor.w covers that as well).
	float const d = enabled ? static_cast<float>(density) : 0.0f;
	s_slot0Shadow.c8_to_c10[2] = XMFLOAT4(0.0f, 0.0f, d, d * d);
	s_slot0Dirty = true;

	// Fog fix 2026-06-11: PS-side scene fog snapshot + b1 fogColor refresh
	// (the per-pass MODE resolve lives in setPsFogMode).
	ms_sceneFogEnabled = enabled;
	ms_sceneFogColor   = XMFLOAT4(r, g, b, 1.0f);
	refreshPsFogColor();
}

// ----------------------------------------------------------------------

void Direct3d11_StateCache::setPsFogMode(int fogMode)
{
	// Fog fix 2026-06-11: per-pass fog COLOR mode (engine FogMode enum:
	// 0=Normal scene color, 1=Black, 2=White). Mirrors D3D9's per-pass
	// D3DRS_FOGCOLOR switch at Direct3d9_StaticShaderData.cpp:932-948.
	// Called per-pass from StaticShaderData::apply.
	if (ms_psFogMode == fogMode)
		return;
	ms_psFogMode = fogMode;
	refreshPsFogColor();
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
	// Plan 11-09.15 Iter-42 (corrected): scale ROWS of m, NOT columns.
	// Pre-Iter-42 the code scaled only diagonals (m[0][0]*sx, m[1][1]*sy,
	// m[2][2]*sz) which is wrong for any non-uniformly-scaled rotated
	// object. My first Iter-42 attempt scaled columns (each m[*][i] by
	// scale[i]); the Mos Eisley smoke 2026-05-23 showed that direction was
	// wrong (no improvement on stretched ribbons, head, trim).
	//
	// The D3D9 sibling at Direct3d9.cpp:3155-3168 is the ground truth:
	// it scales ROW i by scale[i] (every column of row 0 by scale.x, every
	// column of row 1 by scale.y, etc.), leaving the m[i][3] translation
	// untouched. The engine's m[i][3] = translation is the LAST ROW of the
	// math matrix (storage transposed relative to math); row-scaling on
	// stored m = column-scaling on math M = post-multiplying M by diag(scale)
	// = standard object-space scale-then-rotate-then-translate composition.
	//
	// Iter-38B's transpose-at-upload (composeSlot0Shadow) handles the
	// stored-vs-math packing for the cbuffer; this site just needs to
	// match what D3D9 produces.
	// Plan 17-08 (GAP-4) Producer A: cache the raw orthonormal transform for
	// world->object-local light direction mapping (reuses Transform::rotate_p2l).
	s_cachedObjectToWorldTransform = objectToWorld;

	auto const &m = objectToWorld.getMatrix();    // real[3][4]
	s_cachedWorld = XMFLOAT4X4(
		static_cast<float>(m[0][0]) * scale.x, static_cast<float>(m[0][1]) * scale.x, static_cast<float>(m[0][2]) * scale.x, static_cast<float>(m[0][3]),
		static_cast<float>(m[1][0]) * scale.y, static_cast<float>(m[1][1]) * scale.y, static_cast<float>(m[1][2]) * scale.y, static_cast<float>(m[1][3]),
		static_cast<float>(m[2][0]) * scale.z, static_cast<float>(m[2][1]) * scale.z, static_cast<float>(m[2][2]) * scale.z, static_cast<float>(m[2][3]),
		0.0f, 0.0f, 0.0f, 1.0f);

	composeSlot0Shadow();
	s_anyMatrixWritten = true;   // Kept for back-compat; Fix C uses s_slot0Dirty instead

	// Plan 11-09.15 Iter-35: log first 50 setObjectToWorldTransformAndScale
	// calls. This is THE per-mesh setter -- engine calls it before each
	// world mesh draw. If we see N draws on the Iter-26/33B WORLD log but
	// only M < N calls here, the engine is using a different path for
	// (N - M) draws.
	{
		static int s_iter35OwtCount = 0;
		if (s_iter35OwtCount < 50)
		{
			++s_iter35OwtCount;
			if (ID3D11InfoQueue *iq35 = Direct3d11_Device::getInfoQueue())
			{
				char buf35[384];
				_snprintf_s(buf35, sizeof(buf35), _TRUNCATE,
					"Plan 11-09.15 Iter-35 setObjectToWorldTransformAndScale#%d "
					"row0=[%.3f %.3f %.3f %.3f] row1=[%.3f %.3f %.3f %.3f] "
					"row2=[%.3f %.3f %.3f %.3f] scale=[%.3f %.3f %.3f]",
					s_iter35OwtCount,
					static_cast<float>(m[0][0]), static_cast<float>(m[0][1]),
					static_cast<float>(m[0][2]), static_cast<float>(m[0][3]),
					static_cast<float>(m[1][0]), static_cast<float>(m[1][1]),
					static_cast<float>(m[1][2]), static_cast<float>(m[1][3]),
					static_cast<float>(m[2][0]), static_cast<float>(m[2][1]),
					static_cast<float>(m[2][2]), static_cast<float>(m[2][3]),
					scale.x, scale.y, scale.z);
				iq35->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, buf35);
			}
		}
	}
}

// ----------------------------------------------------------------------

Vector Direct3d11_StateCache::worldDirectionToObjectLocal(Vector const &worldDir)
{
	// Mirrors Direct3d9_LightManager::applyLights_vertexShader_dot3:
	//   ms_objectToWorldTransform.rotate_p2l(direction)
	return s_cachedObjectToWorldTransform.rotate_p2l(worldDir);
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

// CONSULT-40 (2026-06-08): D3D9-parity alpha-fade state (mirrors D3D9 ms_alphaFadeOpacity).
namespace { bool s_alphaFadeEnabled = false; float s_alphaFadeOpacity = 1.0f; }

void Direct3d11_StateCache::setAlphaFadeOpacity(bool enabled, float opacity)
{
	// CONSULT-40 (2026-06-08): RECORD the engine's alpha-fade state; the dot3 b0 packing in
	// Direct3d11_StaticShaderData::apply reads it back (getAlphaFadeEnabled/Opacity) and writes
	// c1.a = enabled?1:0 and c2.a = opacity -- exactly D3D9 applyLights_vertexShader (:582,584).
	//
	// REMOVED the old 16-byte `updatePS(0, fade, 16)`: it pushed the opacity into b0 *c0*, a slot
	// the asset dot3 PS never reads for alpha (it reads c1.a/c2.a). Worse, c0 holds the dot3
	// LIGHT DIRECTION + specular power, so this write CLOBBERED it every fade call (one source of
	// the P19/black-walls churn -- the CONSULT-39 invalidateApplyCache was papering over it). The
	// genuine fade opacity was therefore dropped and the light's own specular.a (~0.6) leaked into
	// c2.a instead, forcing every dot3-shaded surface (faces, bump walls) into the PS fade branch
	// -> translucent. Now the real fade flows through the c1.a/c2.a packing and c0 is left intact.
	s_alphaFadeEnabled = enabled;
	s_alphaFadeOpacity = opacity;
	// Force the next setStaticShader to be an apply() cache MISS so the per-draw dot3 staging fill
	// (StaticShaderData.cpp:962-968) re-runs and picks up the new c1.a/c2.a fade values.
	Direct3d11_StaticShaderData::invalidateApplyCache();
}

bool  Direct3d11_StateCache::getAlphaFadeEnabled() { return s_alphaFadeEnabled; }
float Direct3d11_StateCache::getAlphaFadeOpacity() { return s_alphaFadeOpacity; }

// ----------------------------------------------------------------------

void Direct3d11_StateCache::setAlphaBlendEnable(bool enabled)
{
	// Plan 11-09.15 Iter-39B: mirror Direct3d9::setAlphaBlendEnable. Mutates
	// the cached BlendState descriptor's RT0.BlendEnable and marks the
	// blend cache entry dirty so the next applyPreDrawState picks up a
	// (possibly cached) ID3D11BlendState matching the new descriptor.
	//
	// Default at install() is FALSE post-Iter-39A. UI canvas shaders
	// (uicanvas_filtered.sht etc.), particle billboards, and glow sprites
	// flip this to TRUE via their per-pass apply() call. Opaque world
	// shaders leave it FALSE (or explicitly set FALSE if they were last-set
	// after a UI draw).
	D3D11_BLEND_DESC &desc = Direct3d11_StateCacheNamespace::ms_bsDesc;
	BOOL const newVal = enabled ? TRUE : FALSE;
	if (desc.RenderTarget[0].BlendEnable != newVal)
	{
		desc.RenderTarget[0].BlendEnable = newVal;
		Direct3d11_StateCacheNamespace::ms_bsDirty = true;
	}
}

// ----------------------------------------------------------------------

void Direct3d11_StateCache::setDepthEnable(bool enabled)
{
	// Plan 11-09.15 Iter-44A: per-pass z-enable. Mirrors D3D9
	// RSB(D3DRS_ZENABLE, m_zEnable). Writes ms_dssDesc.DepthEnable + marks
	// the dss cache dirty. Cache lookup at applyPreDrawState picks up the
	// matching (cached) ID3D11DepthStencilState.
	D3D11_DEPTH_STENCIL_DESC &desc = Direct3d11_StateCacheNamespace::ms_dssDesc;
	BOOL const newVal = enabled ? TRUE : FALSE;
	if (desc.DepthEnable != newVal)
	{
		desc.DepthEnable = newVal;
		Direct3d11_StateCacheNamespace::ms_dssDirty = true;
	}
}

// ----------------------------------------------------------------------

void Direct3d11_StateCache::setDepthWriteEnable(bool writeEnabled)
{
	// Plan 11-09.15 Iter-44A: per-pass z-write. Mirrors D3D9
	// RSB(D3DRS_ZWRITEENABLE, m_zWrite). D3D11 has no boolean write enable
	// per se; the mask is D3D11_DEPTH_WRITE_MASK_ALL (write) or
	// D3D11_DEPTH_WRITE_MASK_ZERO (no write). One-bit toggle matches D3D9.
	D3D11_DEPTH_STENCIL_DESC &desc = Direct3d11_StateCacheNamespace::ms_dssDesc;
	D3D11_DEPTH_WRITE_MASK const newMask = writeEnabled
		? D3D11_DEPTH_WRITE_MASK_ALL
		: D3D11_DEPTH_WRITE_MASK_ZERO;
	if (desc.DepthWriteMask != newMask)
	{
		desc.DepthWriteMask = newMask;
		Direct3d11_StateCacheNamespace::ms_dssDirty = true;
	}
}

// ----------------------------------------------------------------------

void Direct3d11_StateCache::setDepthCompareFunc(D3D11_COMPARISON_FUNC func)
{
	// Plan 11-09.15 Iter-44A: per-pass z-compare. Mirrors D3D9
	// RSM(D3DRS_ZFUNC, m_zCompare, Compare). Engine-enum-to-D3D11 mapping
	// lives at caller (StaticShaderData::apply); see CODEX caveat about
	// the C_GreaterOrEqual <-> C_NotEqual swap in D3D9's table that we
	// preserve for asset parity.
	D3D11_DEPTH_STENCIL_DESC &desc = Direct3d11_StateCacheNamespace::ms_dssDesc;
	if (desc.DepthFunc != func)
	{
		desc.DepthFunc = func;
		Direct3d11_StateCacheNamespace::ms_dssDirty = true;
	}
}

// ----------------------------------------------------------------------

void Direct3d11_StateCache::setAlphaTest(bool enabled, float reference)
{
	// Plan 11-09.15 Iter-44B: per-pass alpha-test. Push enable+ref into PS
	// cbuffer slot 1; the dynamic-generated PS reads it and conditionally
	// clip()s. Dirty-skip when already-current to avoid per-draw cbuffer
	// updates for repeating values.
	//
	// Detail-blend fix 2026-06-11: slot 1 now shares Direct3d11_PSAlphaTestCB
	// with textureFactor -- writes go through the ms_psSlot1CB shadow so each
	// setter uploads the full struct without clobbering the other's state
	// (WRITE_DISCARD full-write discipline).
	float const newEnable = enabled ? 1.0f : 0.0f;
	if (ms_psSlot1CB.alphaTest.x == newEnable && ms_psSlot1CB.alphaTest.y == reference)
		return;
	ms_psSlot1CB.alphaTest.x = newEnable;
	ms_psSlot1CB.alphaTest.y = reference;

	Direct3d11_ConstantBuffer::updatePS(1, &ms_psSlot1CB, sizeof(ms_psSlot1CB));
	Direct3d11_ConstantBuffer::bindPS(1);
}

// ----------------------------------------------------------------------

void Direct3d11_StateCache::setPsTextureFactor(float r, float g, float b, float a)
{
	// Detail-blend fix 2026-06-11: per-pass TFACTOR for the FFP combiner-
	// cascade generated PS. Same shadow + dirty-skip model as setAlphaTest.
	if (ms_psSlot1CB.textureFactor.x == r && ms_psSlot1CB.textureFactor.y == g
		&& ms_psSlot1CB.textureFactor.z == b && ms_psSlot1CB.textureFactor.w == a)
		return;
	ms_psSlot1CB.textureFactor = DirectX::XMFLOAT4(r, g, b, a);

	Direct3d11_ConstantBuffer::updatePS(1, &ms_psSlot1CB, sizeof(ms_psSlot1CB));
	Direct3d11_ConstantBuffer::bindPS(1);
}

// ----------------------------------------------------------------------

void Direct3d11_StateCache::setFfpCombiner(Direct3d11_FfpCombinerDesc const *desc)
{
	// Detail-blend fix 2026-06-11: copy (descriptor storage is rebuilt on
	// graphics-data restore) or clear the active pass's combiner cascade.
	if (desc)
	{
		ms_ffpCombiner      = *desc;
		ms_ffpCombinerValid = true;
	}
	else
		ms_ffpCombinerValid = false;
}

// ----------------------------------------------------------------------

void Direct3d11_StateCache::setColorWriteEnable(uint8 writeMask)
{
	// Plan 11-09.15 Iter-44A: per-pass color-write mask. Mirrors D3D9
	// RSB-equivalent push of D3DRS_COLORWRITEENABLE. Engine m_writeEnable
	// bits are bitwise-identical to D3D11 D3D11_COLOR_WRITE_ENABLE_* so we
	// pass through unchanged. Mask is per-RT in D3D11; we write RT[0] only
	// (single-RT engine model).
	D3D11_BLEND_DESC &desc = Direct3d11_StateCacheNamespace::ms_bsDesc;
	UINT8 const newMask = static_cast<UINT8>(writeMask & 0x0F);
	if (desc.RenderTarget[0].RenderTargetWriteMask != newMask)
	{
		desc.RenderTarget[0].RenderTargetWriteMask = newMask;
		Direct3d11_StateCacheNamespace::ms_bsDirty = true;
	}
}

// ----------------------------------------------------------------------

void Direct3d11_StateCache::setAlphaBlendFactors(D3D11_BLEND srcBlend,
                                                 D3D11_BLEND destBlend,
                                                 D3D11_BLEND_OP blendOp)
{
	// Plan 11-09.15 Iter-43: per-pass blend FACTORS. See header for the full
	// rationale. Writes to RT0 only -- the engine assumes one render target
	// for color writes (mirrors the D3D9 single-RT model). Mirrors the alpha
	// side to the same factors / op as the color side; the D3D9 sibling uses
	// D3DRS_SRCBLEND / DESTBLEND / BLENDOP which are color-side-only, with
	// alpha implicitly tracking color (D3D9 default). D3D11 requires both
	// sides set explicitly when independent-alpha-blend is OFF (which is our
	// case -- D3D11_BLEND_DESC::IndependentBlendEnable defaults to FALSE).
	// Blend-factors re-land (2026-06-12): the alpha side may NOT use the
	// _COLOR factor variants (D3D11 CreateBlendState rejects them with
	// E_INVALIDARG). Convert to the equivalent _ALPHA variant for the alpha
	// side -- matches D3D9's implicit behavior where the alpha channel
	// tracks the color factors' alpha component.
	struct Local
	{
		static D3D11_BLEND alphaSide(D3D11_BLEND b)
		{
			switch (b)
			{
				case D3D11_BLEND_SRC_COLOR:      return D3D11_BLEND_SRC_ALPHA;
				case D3D11_BLEND_INV_SRC_COLOR:  return D3D11_BLEND_INV_SRC_ALPHA;
				case D3D11_BLEND_DEST_COLOR:     return D3D11_BLEND_DEST_ALPHA;
				case D3D11_BLEND_INV_DEST_COLOR: return D3D11_BLEND_INV_DEST_ALPHA;
				default:                         return b;
			}
		}
	};
	D3D11_BLEND const srcBlendAlpha  = Local::alphaSide(srcBlend);
	D3D11_BLEND const destBlendAlpha = Local::alphaSide(destBlend);

	D3D11_BLEND_DESC &desc = Direct3d11_StateCacheNamespace::ms_bsDesc;
	D3D11_RENDER_TARGET_BLEND_DESC &rt = desc.RenderTarget[0];
	bool changed = false;
	if (rt.SrcBlend != srcBlend)              { rt.SrcBlend = srcBlend;             changed = true; }
	if (rt.DestBlend != destBlend)            { rt.DestBlend = destBlend;           changed = true; }
	if (rt.BlendOp != blendOp)                { rt.BlendOp = blendOp;               changed = true; }
	if (rt.SrcBlendAlpha != srcBlendAlpha)    { rt.SrcBlendAlpha = srcBlendAlpha;   changed = true; }
	if (rt.DestBlendAlpha != destBlendAlpha)  { rt.DestBlendAlpha = destBlendAlpha; changed = true; }
	if (rt.BlendOpAlpha != blendOp)           { rt.BlendOpAlpha = blendOp;          changed = true; }
	if (changed)
		Direct3d11_StateCacheNamespace::ms_bsDirty = true;
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
// Plan 11-09.14: per-pass PS resource binding setters. Direct3d11_StaticShaderData::
// apply() routes the per-pass diffuse SRV + sampler through these
// (CODEX Q1: never call PSSet* directly from StaticShaderData -- preserves
// the lazy-bind model where applyPreDrawState's single flush writes all
// 16 slots per draw).

void Direct3d11_StateCache::setPixelShaderResource(int slot, ID3D11ShaderResourceView *srv)
{
	if (slot < 0 || slot >= kMaxSRVs)
		return;
	ms_boundSRV[slot] = srv;
}

void Direct3d11_StateCache::setPixelShaderSampler(int slot, D3D11_SAMPLER_DESC const &desc)
{
	if (slot < 0 || slot >= kMaxSamplers)
		return;
	// Hash-cached via the existing FNV-1a-keyed SSCache (Plan 11-06).
	// Zero-initialised desc fields are caller's responsibility -- padding
	// bytes will drift the cache key otherwise.
	ID3D11SamplerState *ss = getOrCreateSampler(desc);
	ms_boundSampler[slot] = ss ? ss : ms_defaultSampler.Get();
}

void Direct3d11_StateCache::setPixelShaderSampler(int slot, ID3D11SamplerState *sampler)
{
	if (slot < 0 || slot >= kMaxSamplers)
		return;
	// Null -> default sampler keeps the slot in a defined, sane state so
	// the next draw's flush doesn't push a null pointer at PSSetSamplers
	// (PSSetSamplers tolerates null but the bound shader then reads garbage
	// per the D3D11 spec; CODEX Q4 says "default sampler is fine" for the
	// "no texture" path).
	ms_boundSampler[slot] = sampler ? sampler : ms_defaultSampler.Get();
}

// ----------------------------------------------------------------------

void Direct3d11_StateCache::setVertexBuffer(HardwareVertexBuffer const &vb)
{
	// Plan 11-09.7: deactivate multi-stream state (mutual exclusion with
	// the single-stream path below). resolveShaders + applyPreDrawState
	// branch on ms_currentVBVectorActive.
	ms_currentVBVectorActive = false;
	ms_currentVBVectorStreamCount = 0;

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
		// Plan 11-09.12 Iter-1: Direct3d11_DynamicVertexBufferData::getOffset()
		// returns the slice's VERTEX index (Direct3d11_DynamicVertexBufferData.cpp
		// :234 `m_offset = ms_used / vertexSize`), but IASetVertexBuffers expects
		// the offset argument in BYTES. The multi-stream path
		// (Direct3d11_VertexBufferVectorData.cpp:145) correctly multiplies by
		// stride; the single-stream path here was missing the conversion. Pre-fix
		// the D3D11 debug-layer fired 152,869 id=366 ERRORs per ~30s session
		// ("Vertex Buffer Offset (N) at the input vertex slot 0 is not aligned
		// properly. The current Input Layout imposes an alignment of (4)" with
		// N = raw vertex index, mostly non-multiple-of-4). Mirrors the D3D9
		// reference architecture intent (D3D9 keeps byteOffset=0 and uses
		// BaseVertexIndex on the draw call -- a separate option here, but
		// matching the multi-stream pattern is the minimum-blast-radius fix).
		ms_currentVBOffset       = static_cast<UINT>(data->getOffset()) * ms_currentVBStride;
		ms_currentVBVertexCount  = data->getNumberOfVertices();
		ms_currentVBFormat       = dvb->getFormat();
		ms_currentVBValid        = (ms_currentVB != nullptr);
	}

	ms_geometryRebindNeeded = true;
}

// ----------------------------------------------------------------------
// Plan 11-09.7: multi-stream VertexBufferVector bind path. Called by
// Direct3d11_VertexBufferVectorData::bind, which is the slot wired to
// Gl_api::setVertexBufferVector. The wrapper class iterates the
// engine-side VBVector, extracts per-stream ID3D11Buffer + offset +
// stride + format, and passes them in. resolveShaders + applyPreDrawState
// branch on ms_currentVBVectorActive.

void Direct3d11_StateCache::setVertexBufferVectorBindState(
	int streamCount,
	ID3D11Buffer * const *buffers,
	UINT const *strides,
	UINT const *offsets,
	VertexBufferFormat const * const *formats,
	int sliceFirstVertex,
	int sliceVertexCount)
{
	// Clear single-stream state (mutual exclusion with the vector path).
	ms_currentVB = nullptr;
	ms_currentVBStride = 0;
	ms_currentVBOffset = 0;
	ms_currentVBValid = false;

	DEBUG_FATAL(streamCount < 0 || streamCount > kMaxVBStreams,
		("Direct3d11_StateCache::setVertexBufferVectorBindState: streamCount=%d outside [0,%d]",
		streamCount, kMaxVBStreams));

	ms_currentVBVectorActive      = (streamCount > 0);
	ms_currentVBVectorStreamCount = streamCount;
	for (int i = 0; i < kMaxVBStreams; ++i)
	{
		ms_currentVBVectorBuffers[i] = (i < streamCount && buffers) ? buffers[i] : nullptr;
		ms_currentVBVectorStrides[i] = (i < streamCount && strides) ? strides[i] : 0;
		ms_currentVBVectorOffsets[i] = (i < streamCount && offsets) ? offsets[i] : 0;
		ms_currentVBVectorFormats[i] = (i < streamCount && formats) ? formats[i] : nullptr;
	}

	// Mirror Direct3d9.cpp:3773 / 3824 slice tracking. Engine draw calls
	// use these to compute the BaseVertexLocation for indexed draws +
	// vertex-count argument for non-indexed.
	ms_currentVBVertexCount = sliceVertexCount;
	(void)sliceFirstVertex; // single-stream slice offset; multi-stream uses streamOffsets[]

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

void Direct3d11_StateCache::setCurrentVSData(Direct3d11_VertexShaderData const *vs, uint32 textureCoordinateSetKey)
{
	// Plan 11-09 Iter-1: Direct3d11_StaticShaderData::apply calls this
	// once per draw with the per-pass VS pointer. Trip ms_geometryRebindNeeded
	// so the next applyPreDrawState reselects the input layout against the
	// new VS bytecode signature (Pitfall 6 territory).
	//
	// Plan 17-09: the per-pass texcoord-set key rides alongside the VS pointer.
	// The SAME VS asset can bind under different keys across passes -- each key
	// is a distinct compiled variant (distinct bytecode/signature), so a key
	// change must also rebind the layout. resolveShaders / applyPreDrawState
	// read ms_currentVSKey explicitly to resolve the right variant.
	if (Direct3d11_StateCacheNamespace::ms_currentVSData != vs
		|| Direct3d11_StateCacheNamespace::ms_currentVSKey != textureCoordinateSetKey)
	{
		Direct3d11_StateCacheNamespace::ms_currentVSData = vs;
		Direct3d11_StateCacheNamespace::ms_currentVSKey  = textureCoordinateSetKey;
		Direct3d11_StateCacheNamespace::ms_geometryRebindNeeded = true;
	}
}

// ----------------------------------------------------------------------

void Direct3d11_StateCache::setSpecularPower(float power)
{
	// Plan 17-09 (GAP-5): mirror Direct3d9_StateCache::setSpecularPower. The VS
	// object-space specular reads materialSpecularPower from lightData[25].w
	// (dot3.localDirection.w). Store it (composeSlot0Shadow uses it) AND patch the
	// live shadow in place so the next flush is correct regardless of whether
	// composeSlot0Shadow re-runs this draw. Reject 0 / NaN / wild values (they make
	// pow() produce NaN) -- fall back to the D3D9 default of 32.
	float const safe = (power > 0.0f && power < 1.0e6f) ? power : 32.0f;
	Direct3d11_StateCacheNamespace::ms_specularPower = safe;
	// c15.x (material[4].x) is materialSpecularPower -- the register the VS object-
	// space specular actually reads (proven by disasm: `mul r4.w, r4.w, cb0[15].x`).
	// composeSlot0Shadow never fills material[], so without this it is 0 -> at a
	// perpendicular vertex (nDotH==0) the unclamped `log2(0)*0 = -INF*0 = NaN` is
	// selected by the >=0 movc mask -> NaN COLOR0/COLOR1 -> purple/green arms.
	Direct3d11_StateCacheNamespace::s_slot0Shadow.material[4].x = safe;
	// Also mirror D3D9's dot3.localDirection.w (c41.w) for the PS dot3 path.
	Direct3d11_StateCacheNamespace::s_slot0Shadow.lightData[25].w = safe;
	Direct3d11_StateCacheNamespace::s_slot0Dirty = true;
}

// ----------------------------------------------------------------------

void Direct3d11_StateCache::setVertexMaterialColors(
	DirectX::XMFLOAT4 const &diffuse,
	DirectX::XMFLOAT4 const &specular,
	DirectX::XMFLOAT4 const &emissive)
{
	// Plan 17-09 (GAP-5 bump-arm audit): mirror Direct3d9_StaticShaderData.cpp:862
	// `setVertexShaderConstants(VSCR_material, &m_material, 5)` (VSCR_material=11).
	// The VS material struct of Direct3d11_VertexSlot0CB is material[5] @ c11..c15:
	//   material[0] = c11 diffuse, material[1] = c12 ambient, material[2] = c13
	//   specular, material[3] = c14 emissive, material[4].x = c15 specularPower.
	// composeSlot0Shadow never fills c11..c14 and setSpecularPower only fills
	// material[4].x, so the asset VS read cb0[14] (emissive, the diffuse SEED) and
	// cb0[13] (specular tint) as ZERO -> dark diffuse seed + always-black VS
	// specular, diverging from D3D9. We patch the live shadow in place (like
	// setSpecularPower) so the next flush carries them regardless of whether
	// composeSlot0Shadow re-runs. material[1] (c12 ambient) is left untouched:
	// PerPassMaterial carries no ambient and no asset VS in the char-select set
	// reads c12 (the diffuse path seeds from emissive, not material ambient).
	// material[4].x (power) is owned by setSpecularPower -- not touched here.
	Direct3d11_StateCacheNamespace::s_slot0Shadow.material[0] = diffuse;
	Direct3d11_StateCacheNamespace::s_slot0Shadow.material[2] = specular;
	Direct3d11_StateCacheNamespace::s_slot0Shadow.material[3] = emissive;
	Direct3d11_StateCacheNamespace::s_slot0Dirty = true;
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
	// Plan 11-09.15 (CODEX Bucket A): D3D11 has no native TRIANGLEFAN
	// topology. Bind the reusable fan-to-list IB and issue DrawIndexed
	// with the canonical (0, i, i+1) pattern that mirrors D3D9's native
	// D3DPT_TRIANGLEFAN expansion. Mirror of D3D9 drawQuadList shape at
	// Direct3d9.cpp:4277. Plan 11-09.14 SRV0 binding wired textures to the
	// rendered half of every fan-drawn quad (SkyBox face, sprite, beam,
	// sun, moon, planet, post-FX, Bink video), unmasking the half-render
	// artifact that pre-Plan-11-09.15 was invisible under magenta-fallback.
	int const vertCount = ms_currentVBVertexCount;
	if (vertCount < 3)
		return;  // need >= 3 verts for >= 1 triangle
	int const triCount = vertCount - 2;

	++ms_drawTriangleFanCalls;
	if (vertCount > ms_triangleFanMaxVertices)
		ms_triangleFanMaxVertices = vertCount;

	if (!ensureTriangleFanIB(vertCount))
		return;  // > 65535 cap or CreateBuffer failure -> skip draw

	if (!applyPreDrawState(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST))
		return;

	// Plan 11-09.15 Iter-3 diagnostic: sample fan-draw state at intervals
	// to attribute the "radiating from world point" artifact. Iter-2's
	// "first 5" capture showed all from startup-frame intro/loading (one
	// fan per frame in first 5 frames -> all offset=0 because of frame-
	// boundary discards). Iter-3 captures:
	//   * The first 5 fans (intro/loading state)
	//   * Then every 500th fan after that (mid-game samples)
	// Both bucket batches surface as separate InfoQueue lines.
	{
		static int s_diagFanCallsTotal = 0;
		++s_diagFanCallsTotal;
		bool const earlyBucket = (s_diagFanCallsTotal <= 5);
		bool const sampleBucket = (s_diagFanCallsTotal > 5) && ((s_diagFanCallsTotal % 500) == 0);
		if (earlyBucket || sampleBucket)
		{
			char buf[384];
			_snprintf_s(buf, sizeof(buf), _TRUNCATE,
				"Plan 11-09.15 Iter-3 fan-draw#%d state: vertCount=%d triCount=%d "
				"VB=0x%p stride=%u offset=%u VBValid=%d "
				"IB=0x%p IBValid=%d IBFormat=%d IBOffset=%u IBIndexCount=%d "
				"VBVectorActive=%d VBVectorStreamCount=%d",
				s_diagFanCallsTotal, vertCount, triCount,
				(void *)ms_currentVB, ms_currentVBStride, ms_currentVBOffset, ms_currentVBValid ? 1 : 0,
				(void *)ms_currentIB, ms_currentIBValid ? 1 : 0,
				static_cast<int>(ms_currentIBFormat), ms_currentIBOffset, ms_currentIBIndexCount,
				ms_currentVBVectorActive ? 1 : 0, ms_currentVBVectorStreamCount);
			if (ID3D11InfoQueue *iq = Direct3d11_Device::getInfoQueue())
				iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, buf);
		}
	}

	// Plan 11-09.15 Iter-5 (CODEX consult follow-up): for transformed-vert
	// (XYZRHW) callers, dump VS metadata so we can disambiguate between
	// "right 2D VS bound, math broken somewhere downstream" vs "wrong 3D VS
	// accidentally bound for transformed callers." CODEX recommended a
	// narrow Path A fix targeting the 2D shader compile path, but only
	// after we confirm which scenario applies. Sampled at the same cadence
	// as the Iter-3 fan-draw-state diagnostic (first 5 + every 500th).
	if (ms_currentVBFormat.isTransformed() && ms_currentVSData)
	{
		static int s_diagTransformedFanCalls = 0;
		++s_diagTransformedFanCalls;
		bool const earlyBucket = (s_diagTransformedFanCalls <= 5);
		bool const sampleBucket = (s_diagTransformedFanCalls > 5) && ((s_diagTransformedFanCalls % 500) == 0);
		if (earlyBucket || sampleBucket)
		{
			ID3D11InfoQueue *iq = Direct3d11_Device::getInfoQueue();
			if (iq)
			{
				ShaderImplementationPassVertexShader const *eng = ms_currentVSData->getEngineShader();
				char const *vsFilename = (eng ? eng->getFilename() : "<no engine shader>");
				uint64_t const vsHash = ms_currentVSData->getVariant(ms_currentVSKey).bytecodeHash;   // Plan 17-09

				// Header line: which transformed-vert fan call this is +
				// which VS is bound.
				{
					char buf[256];
					_snprintf_s(buf, sizeof(buf), _TRUNCATE,
						"Plan 11-09.15 Iter-5 transformed-fan#%d VS='%s' bytecodeHash=0x%016llX vertCount=%d",
						s_diagTransformedFanCalls, vsFilename,
						static_cast<unsigned long long>(vsHash), vertCount);
					iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, buf);
				}

				// VS source-text first ~180 chars (signature decl is at the
				// top of every .vsh body; that's what we need to see).
				if (eng && eng->m_text && eng->m_textLength > 0)
				{
					int const snippetLen = (eng->m_textLength < 180) ? eng->m_textLength : 180;
					char snippet[224];
					int outIdx = 0;
					for (int i = 0; i < snippetLen && outIdx < static_cast<int>(sizeof(snippet)) - 1; ++i)
					{
						char const c = eng->m_text[i];
						snippet[outIdx++] = (c == '\n' || c == '\r' || c == '\t') ? ' ' : c;
					}
					snippet[outIdx] = '\0';
					char buf[384];
					_snprintf_s(buf, sizeof(buf), _TRUNCATE,
						"Plan 11-09.15 Iter-5 transformed-fan#%d VS-source-head: %s",
						s_diagTransformedFanCalls, snippet);
					iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, buf);
				}

				// Reflected VS inputs (Plan 11-09.8 captured these).
				{
					auto const &inputs = ms_currentVSData->getVariant(ms_currentVSKey).reflectedInputs;   // Plan 17-09
					char buf[384];
					int outIdx = _snprintf_s(buf, sizeof(buf), _TRUNCATE,
						"Plan 11-09.15 Iter-5 transformed-fan#%d VS-inputs(%zu):",
						s_diagTransformedFanCalls, inputs.size());
					for (size_t k = 0; k < inputs.size() && outIdx > 0 && outIdx < static_cast<int>(sizeof(buf)) - 32; ++k)
					{
						int n = _snprintf_s(buf + outIdx, sizeof(buf) - outIdx, _TRUNCATE,
							" [%s%u mask=0x%X type=%d]",
							inputs[k].SemanticName, inputs[k].SemanticIndex,
							inputs[k].ComponentMask, static_cast<int>(inputs[k].ComponentType));
						if (n < 0) break;
						outIdx += n;
					}
					iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, buf);
				}

				// Reflected VS outputs (Plan 11-09.13 Iter-3 captured these).
				{
					auto const &outputs = ms_currentVSData->getVariant(ms_currentVSKey).reflectedOutputs;   // Plan 17-09
					char buf[384];
					int outIdx = _snprintf_s(buf, sizeof(buf), _TRUNCATE,
						"Plan 11-09.15 Iter-5 transformed-fan#%d VS-outputs(%zu):",
						s_diagTransformedFanCalls, outputs.size());
					for (size_t k = 0; k < outputs.size() && outIdx > 0 && outIdx < static_cast<int>(sizeof(buf)) - 32; ++k)
					{
						int n = _snprintf_s(buf + outIdx, sizeof(buf) - outIdx, _TRUNCATE,
							" [%s%u r=o%u mask=0x%X rwmask=0x%X]",
							outputs[k].SemanticName, outputs[k].SemanticIndex,
							outputs[k].Register, outputs[k].ComponentMask, outputs[k].ReadWriteMask);
						if (n < 0) break;
						outIdx += n;
					}
					iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, buf);
				}

				// Plan 11-09.15 Iter-6: one-shot FULL VS source dump per unique
				// bytecode hash. Iter-5 confirmed the right 2D VS is bound
				// (2d_texture.vsh + ui_radar.vsh) -- we now need to see the
				// VS body to determine WHY the position math is wrong. Reading
				// the HLSL is the cheapest way to tell whether the VS uses
				// viewportData/c9 vs mul(pos, worldViewProjection). Chunked
				// across multiple InfoQueue messages because typical .vsh
				// bodies run 300-1200 chars. Newlines/tabs flattened to
				// spaces for log readability.
				{
					static uint64_t s_iter6SeenHashes[8] = {0};
					static int      s_iter6SeenCount     = 0;
					bool alreadySeen = false;
					for (int k = 0; k < s_iter6SeenCount; ++k)
						if (s_iter6SeenHashes[k] == vsHash) { alreadySeen = true; break; }
					if (!alreadySeen && s_iter6SeenCount < 8 && eng && eng->m_text && eng->m_textLength > 0)
					{
						s_iter6SeenHashes[s_iter6SeenCount++] = vsHash;
						int const totalLen   = eng->m_textLength;
						int const kChunkSize = 240;
						int const chunkCount = (totalLen + kChunkSize - 1) / kChunkSize;
						for (int ci = 0; ci < chunkCount && ci < 8; ++ci)
						{
							int const startByte = ci * kChunkSize;
							int const chunkLen  = (totalLen - startByte < kChunkSize) ? (totalLen - startByte) : kChunkSize;
							char chunkBuf[256];
							int chunkOut = 0;
							for (int i = 0; i < chunkLen && chunkOut < static_cast<int>(sizeof(chunkBuf)) - 1; ++i)
							{
								char const c = eng->m_text[startByte + i];
								chunkBuf[chunkOut++] = (c == '\n' || c == '\r' || c == '\t') ? ' ' : c;
							}
							chunkBuf[chunkOut] = '\0';
							char buf[384];
							_snprintf_s(buf, sizeof(buf), _TRUNCATE,
								"Plan 11-09.15 Iter-6 VS-full hash=0x%016llX file='%s' chunk%d/%d: %s",
								static_cast<unsigned long long>(vsHash), vsFilename,
								ci, chunkCount, chunkBuf);
							iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, buf);
						}
					}
				}
			}
		}
	}

	// Plan 11-09.15 Iter-15: GPU staging readback of the bound SRV0 texture.
	// Iter-14 UV viz showed gradients on splash -> UVs reach PS correctly
	// -> H6 (PS register linkage drift) is dead. CODEX + Cursor both rank
	// H3 (texture content is zero on GPU) as the top suspect. This block
	// runs once per session at the first transformed-vert fan draw with a
	// bound SRV0: dump the texture's metadata, copy mip 0 to a STAGING
	// resource, Map+Read center bytes, log byte hex + nonzero-byte counts.
	// If center bytes are all zero and totalNonZero across the row is 0,
	// H3 is confirmed -- the texture exists but its GPU memory was never
	// uploaded with real pixel data.
	if (ms_currentVBFormat.isTransformed() && ms_boundSRV[0])
	{
		// Plan 11-09.15 Iter-16: log the SRV pointer + VS filename for
		// the first 10 transformed-vert fan draws. Tells us whether SRV0
		// is stable (same wrong texture for every draw -> something is
		// pre-binding the RT and nobody overwrites) or thrashing
		// (different textures per draw -> the binding logic is wrong
		// for this specific shader/draw class).
		static int s_iter16Count = 0;
		if (s_iter16Count < 50)
		{
			++s_iter16Count;
			if (ID3D11InfoQueue *iqHdr = Direct3d11_Device::getInfoQueue())
			{
				char const *vsFn = "<none>";
				if (ms_currentVSData)
				{
					ShaderImplementationPassVertexShader const *eng = ms_currentVSData->getEngineShader();
					if (eng) vsFn = eng->getFilename();
				}

				// Plan 11-09.15 Iter-23: also read the ACTUAL bound RT via
				// OMGetRenderTargets. If RT differs from the backbuffer at
				// draw time, UI is rendering to a different target (e.g.,
				// primaryBuffer per PostProcessingEffectsManager). Use that
				// to confirm where UI draws are landing.
				ID3D11RenderTargetView *currentRTV = nullptr;
				ID3D11DeviceContext *ctxRT = Direct3d11_Device::getContext();
				if (ctxRT)
					ctxRT->OMGetRenderTargets(1, &currentRTV, nullptr);
				UINT rtWidth = 0, rtHeight = 0;
				DXGI_FORMAT rtFmt = DXGI_FORMAT_UNKNOWN;
				if (currentRTV)
				{
					Microsoft::WRL::ComPtr<ID3D11Resource> rtResource;
					currentRTV->GetResource(rtResource.GetAddressOf());
					Microsoft::WRL::ComPtr<ID3D11Texture2D> rtTex;
					if (rtResource && SUCCEEDED(rtResource.As(&rtTex)) && rtTex)
					{
						D3D11_TEXTURE2D_DESC rtDesc = {};
						rtTex->GetDesc(&rtDesc);
						rtWidth = rtDesc.Width;
						rtHeight = rtDesc.Height;
						rtFmt = rtDesc.Format;
					}
				}

				char hdr[384];
				_snprintf_s(hdr, sizeof(hdr), _TRUNCATE,
					"Plan 11-09.15 Iter-16/23 sample#%d VS='%s' SRV0_ptr=0x%p "
					"boundRTV=0x%p rtW=%u rtH=%u rtFmt=%d",
					s_iter16Count, vsFn, static_cast<void *>(ms_boundSRV[0]),
					static_cast<void *>(currentRTV), rtWidth, rtHeight,
					static_cast<int>(rtFmt));
				iqHdr->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, hdr);

				if (currentRTV)
					currentRTV->Release();
			}
		}

		static bool s_iter15Done = false;
		if (!s_iter15Done)
		{
			s_iter15Done = true;
			ID3D11InfoQueue * const iq = Direct3d11_Device::getInfoQueue();
			ID3D11Device * const device = Direct3d11_Device::getDevice();
			ID3D11DeviceContext * const context = Direct3d11_Device::getContext();
			if (iq && device && context)
			{
				Microsoft::WRL::ComPtr<ID3D11Resource> resource;
				ms_boundSRV[0]->GetResource(resource.GetAddressOf());
				Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2d;
				if (resource && SUCCEEDED(resource.As(&tex2d)) && tex2d)
				{
					D3D11_TEXTURE2D_DESC texDesc = {};
					tex2d->GetDesc(&texDesc);
					{
						char buf[384];
						_snprintf_s(buf, sizeof(buf), _TRUNCATE,
							"Plan 11-09.15 Iter-15 SRV0 texture: W=%u H=%u Mips=%u "
							"Format=%d Usage=%d BindFlags=0x%X CPUAccess=0x%X SampleCount=%u",
							texDesc.Width, texDesc.Height, texDesc.MipLevels,
							static_cast<int>(texDesc.Format),
							static_cast<int>(texDesc.Usage), texDesc.BindFlags,
							texDesc.CPUAccessFlags, texDesc.SampleDesc.Count);
						iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, buf);
					}

					D3D11_TEXTURE2D_DESC stagingDesc = texDesc;
					stagingDesc.Usage          = D3D11_USAGE_STAGING;
					stagingDesc.BindFlags      = 0;
					stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
					stagingDesc.MipLevels      = 1;
					stagingDesc.ArraySize      = 1;
					stagingDesc.MiscFlags      = 0;

					Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
					HRESULT hr = device->CreateTexture2D(&stagingDesc, nullptr, staging.GetAddressOf());
					if (SUCCEEDED(hr) && staging)
					{
						context->CopySubresourceRegion(staging.Get(), 0, 0, 0, 0,
							resource.Get(), 0, nullptr);
						D3D11_MAPPED_SUBRESOURCE mapped = {};
						hr = context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
						if (SUCCEEDED(hr) && mapped.pData)
						{
							UINT const cx = texDesc.Width  / 2;
							UINT const cy = texDesc.Height / 2;
							uint8_t const *centerRow = static_cast<uint8_t const *>(mapped.pData) + cy * mapped.RowPitch;
							uint8_t centerBytes[16] = {};
							int centerNonZero = 0;
							UINT const bppGuess = (texDesc.Format == DXGI_FORMAT_BC1_UNORM ||
							                       texDesc.Format == DXGI_FORMAT_BC1_UNORM_SRGB) ? 2 : 4;
							UINT const centerByteOffset = cx * bppGuess;
							for (int k = 0; k < 16 && centerByteOffset + k < mapped.RowPitch; ++k)
							{
								centerBytes[k] = centerRow[centerByteOffset + k];
								if (centerBytes[k] != 0) ++centerNonZero;
							}
							int totalNonZero = 0;
							for (UINT y = 0; y < texDesc.Height && totalNonZero < 100; ++y)
							{
								uint8_t const *r = static_cast<uint8_t const *>(mapped.pData) + y * mapped.RowPitch;
								for (UINT b = 0; b < mapped.RowPitch && totalNonZero < 100; ++b)
									if (r[b] != 0) ++totalNonZero;
							}
							char buf[512];
							_snprintf_s(buf, sizeof(buf), _TRUNCATE,
								"Plan 11-09.15 Iter-15 SRV0 readback center(%u,%u) bppGuess=%u "
								"bytes=[%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X] "
								"centerNonZero=%d/16 totalNonZeroFirst100=%d RowPitch=%u",
								cx, cy, bppGuess,
								centerBytes[0], centerBytes[1], centerBytes[2], centerBytes[3],
								centerBytes[4], centerBytes[5], centerBytes[6], centerBytes[7],
								centerBytes[8], centerBytes[9], centerBytes[10], centerBytes[11],
								centerBytes[12], centerBytes[13], centerBytes[14], centerBytes[15],
								centerNonZero, totalNonZero, mapped.RowPitch);
							iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, buf);
							context->Unmap(staging.Get(), 0);
						}
						else
						{
							char errBuf[160];
							_snprintf_s(errBuf, sizeof(errBuf), _TRUNCATE,
								"Plan 11-09.15 Iter-15 staging Map failed hr=0x%08X",
								static_cast<unsigned int>(hr));
							iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_WARNING, errBuf);
						}
					}
					else
					{
						char errBuf[160];
						_snprintf_s(errBuf, sizeof(errBuf), _TRUNCATE,
							"Plan 11-09.15 Iter-15 staging CreateTexture2D failed hr=0x%08X format=%d",
							static_cast<unsigned int>(hr), static_cast<int>(texDesc.Format));
						iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_WARNING, errBuf);
					}
				}
			}
		}
	}

	ID3D11DeviceContext *ctx = Direct3d11_Device::getContext();

	// Plan 11-09.15 Iter-4 (Test B): fan-IB BYPASS test for radial-pattern
	// attribution. When kBypassFanIBForTestB is true, render the fan VB as
	// TRIANGLELIST topology (verts 0,1,2 -> tri 1; verts 3+ dropped) without
	// binding our fan-list IB. This reproduces the pre-Plan-11-09.15
	// half-screen behavior. Visual result interprets:
	//   * Half-screen returns -> Plan 11-09.15 IS the regression vehicle
	//     (the radial pattern was introduced by fan-list expansion).
	//   * Radial pattern persists -> the bug is UPSTREAM of fan expansion;
	//     Plan 11-09.15 merely unmasked it by fixing the half-render that
	//     was hiding it.
	// Disambiguates H4 in the .continue-here.md hypothesis matrix.
	constexpr bool kBypassFanIBForTestB = false;
	if (kBypassFanIBForTestB)
	{
		ctx->Draw(static_cast<UINT>(vertCount), 0);
		return;
	}

	ctx->IASetIndexBuffer(
		ms_triangleFanIB.Get(),
		DXGI_FORMAT_R16_UINT,
		0);

	ctx->DrawIndexed(
		static_cast<UINT>(triCount * 3),
		0,
		0);

	// Plan 11-09.15 Iter-1 follow-up: defensive IB rebind after our DrawIndexed.
	// CODEX Q4 said "next indexed draw rebinds via applyPreDrawState's existing
	// guard" so this should be redundant -- Kenny's Iter-2 smoke confirmed the
	// rebind didn't fix the radiating pattern (i.e. the leak hypothesis was
	// wrong; the bug is something else). Keep the rebind as defensive
	// hygiene; harmless ~50ns/call.
	if (ms_currentIBValid && ms_currentIB)
		ctx->IASetIndexBuffer(ms_currentIB, ms_currentIBFormat, ms_currentIBOffset);
}

void Direct3d11_StateCache::drawQuadList()
{
	// Plan 11-09.15 Iter-27: implementation of the stubbed drawQuadList.
	// CODEX + Cursor convergent consult on the
	// engine-renders-only-postfx-blit symptom identified this stub as the
	// root cause of "splash UI never reaches D3D11 plugin": every UI
	// textured quad goes through CuiLayerRenderer::flushRenderQueueQuads
	// -> Graphics::drawQuadList -> the stub here -> silent drop.
	//
	// Mirrors Plan 11-09.15's fan-list IB pattern (drawTriangleFan):
	// reusable quad-list IB with pattern (4N+0, 4N+1, 4N+2, 4N+0, 4N+2,
	// 4N+3) per quad. Bind IB, applyPreDrawState(TRIANGLELIST), issue
	// DrawIndexed(numQuads * 6, 0, 0). Defensive IB rebind matches the
	// fan path.
	//
	// D3D9 reference: Direct3d9.cpp:4277 uses ms_quadListIndexBuffer
	// with identical winding + same R16 cap.
	int const vertCount = ms_currentVBVertexCount;
	if (vertCount < 4)
		return;
	int const numQuads = vertCount / 4;
	if (numQuads <= 0)
		return;

	++ms_drawQuadListCalls;
	if (numQuads > ms_quadListMaxQuads)
		ms_quadListMaxQuads = numQuads;

	if (!ensureQuadListIB(numQuads))
		return;  // > R16_UINT cap or CreateBuffer failure -- skip draw

	if (!applyPreDrawState(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST))
		return;

	ID3D11DeviceContext * const ctx = Direct3d11_Device::getContext();
	ctx->IASetIndexBuffer(
		ms_quadListIB.Get(),
		DXGI_FORMAT_R16_UINT,
		0);

	// Plan 11-09.15 Iter-29B: routing diagnostic. Log shader template
	// name + VS filename + SRV0 + vertCount per drawQuadList call for
	// the first 50 calls. CODEX+Cursor consult recommended this to
	// localize whether font draws hit shader/uicanvas_filtered.sht (the
	// path Iter-28 modulate fixes) or some other template entirely.
	// Iter-29A1/A2/A3 PS-shape smoke (`.a` mask, `.r` mask, raw `tex`)
	// all failed to produce letter shapes -- which rules out the PS-
	// shape hypothesis and points to a routing or texture-binding bug.
	{
		static int s_iter29bCount = 0;
		if (s_iter29bCount < 50)
		{
			++s_iter29bCount;
			if (ID3D11InfoQueue *iq29b = Direct3d11_Device::getInfoQueue())
			{
				char const * const tmpl = Direct3d11_StaticShaderData::getActiveStaticShaderName();
				char const *vsFn = "<none>";
				if (ms_currentVSData)
				{
					ShaderImplementationPassVertexShader const *eng = ms_currentVSData->getEngineShader();
					if (eng) vsFn = eng->getFilename();
				}
				char buf29b[512];
				_snprintf_s(buf29b, sizeof(buf29b), _TRUNCATE,
					"Plan 11-09.15 Iter-29B quad#%d sht='%s' VS='%s' "
					"SRV0=0x%p vertCount=%d numQuads=%d",
					s_iter29bCount, tmpl, vsFn,
					static_cast<void *>(ms_boundSRV[0]),
					ms_currentVBVertexCount, numQuads);
				iq29b->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, buf29b);
			}
		}
	}

	// Plan 11-09.15 Iter-29D: Iter-29C readback REMOVED (caused first-frame
	// crash at planet-select on Kenny's smoke 2026-05-22 20:36; only beginScene#1
	// reached the d3d11-debug.log before crash, even though UpTime showed 22s
	// of asset loading). Suspected cause: CopySubresourceRegion + Map of the
	// font atlas SRV from inside drawQuadList interfered with the active draw
	// stream OR called GetResource on a stale ms_boundSRV[0] raw pointer.
	// Iter-29B routing diagnostic above is preserved (it WORKED the previous
	// run -- we got 50 lines). A safer readback variant (Iter-30 candidate)
	// would defer the dump to AFTER Present, capture the atlas SRV by
	// AddRef'ing it during drawQuadList for later use, and run in a
	// CallbackEnd-equivalent slot rather than mid-draw.

	ctx->DrawIndexed(
		static_cast<UINT>(numQuads * 6),
		0,
		0);

	// Defensive IB rebind matches the fan-list path: if a downstream
	// caller assumes its prior setIndexBuffer is still bound on the
	// device, restore the StateCache shadow's IB.
	if (ms_currentIBValid && ms_currentIB)
		ctx->IASetIndexBuffer(ms_currentIB, ms_currentIBFormat, ms_currentIBOffset);
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
	// Plan 11-09.15 (CODEX Q3 DEFER + INSTRUMENT): indexed-fan variants
	// are still broken (TRIANGLELIST topology applied to fan-shaped indexed
	// data). A real fix needs CPU-side index shadows in
	// Direct3d11_StaticIndexBufferData::lock/unlock first --
	// Direct3d11_StaticIndexBufferData.cpp:96 currently allocates a zeroed
	// CPU buffer without reading GPU/engine indices back. Per CODEX Q3
	// caveat, source search doesn't prove indexed fans are unused
	// (ShaderPrimitiveSetTemplate.cpp:673 CAN dispatch
	// SPSPT_indexedTriangleFan from asset data), so the diagnostic +
	// counter pair tells smoke whether Plan 11-09.16 is required.
	++ms_drawIndexedTriangleFanCalls;
	static bool s_warnedOnce = false;
	if (!s_warnedOnce)
	{
		s_warnedOnce = true;
		// Plan 11-09.15 Iter-1 follow-up: DEBUG_REPORT_LOG_PRINT is invisible
		// from GUI-exe launches per Plan 11-09.13 Iter-3 finding; route through
		// ID3D11InfoQueue::AddApplicationMessage so the warning lands in
		// stage/d3d11-debug.log.
		char const *msg = "Plan 11-09.15: Direct3d11_StateCache::drawIndexedTriangleFan called -- "
			"STILL BROKEN (TRIANGLELIST topology applied to fan-shaped indexed data). "
			"Plan 11-09.16 territory; needs CPU-side index shadows in "
			"Direct3d11_StaticIndexBufferData::lock/unlock first per CODEX 11-09.15 Q3.";
		if (ID3D11InfoQueue *iq = Direct3d11_Device::getInfoQueue())
			iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_WARNING, msg);
		DEBUG_REPORT_LOG_PRINT(true, ("%s\n", msg));
	}

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
	// Plan 11-09.15 (CODEX Bucket A): partial-fan variant. Uses the same
	// reusable fan IB; BaseVertexLocation = startVertex shifts the (0,i,i+1)
	// pattern to (startVertex, startVertex+i, startVertex+i+1).
	if (primitiveCount < 1)
		return;
	int const vertCount = primitiveCount + 2;

	++ms_drawPartialTriangleFanCalls;
	if (vertCount > ms_triangleFanMaxVertices)
		ms_triangleFanMaxVertices = vertCount;

	if (!ensureTriangleFanIB(vertCount))
		return;

	if (!applyPreDrawState(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST))
		return;

	ID3D11DeviceContext *ctx = Direct3d11_Device::getContext();
	ctx->IASetIndexBuffer(
		ms_triangleFanIB.Get(),
		DXGI_FORMAT_R16_UINT,
		0);

	ctx->DrawIndexed(
		static_cast<UINT>(primitiveCount * 3),
		0,
		static_cast<INT>(startVertex));

	// Plan 11-09.15 Iter-1 follow-up: defensive IB rebind (see drawTriangleFan).
	if (ms_currentIBValid && ms_currentIB)
		ctx->IASetIndexBuffer(ms_currentIB, ms_currentIBFormat, ms_currentIBOffset);
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
	// Plan 11-09.15 (CODEX Q3 DEFER + INSTRUMENT): see drawIndexedTriangleFan
	// for the deferral rationale. Same CPU-side-index-shadow gap.
	++ms_drawPartialIndexedTriangleFanCalls;
	static bool s_warnedOnce = false;
	if (!s_warnedOnce)
	{
		s_warnedOnce = true;
		// Plan 11-09.15 Iter-1 follow-up: route through InfoQueue (see
		// drawIndexedTriangleFan).
		char const *msg = "Plan 11-09.15: Direct3d11_StateCache::drawPartialIndexedTriangleFan called -- "
			"STILL BROKEN (TRIANGLELIST topology applied to fan-shaped indexed data). "
			"Plan 11-09.16 territory per CODEX 11-09.15 Q3.";
		if (ID3D11InfoQueue *iq = Direct3d11_Device::getInfoQueue())
			iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_WARNING, msg);
		DEBUG_REPORT_LOG_PRINT(true, ("%s\n", msg));
	}

	if (!applyPreDrawState(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST))
		return;
	Direct3d11_Device::getContext()->DrawIndexed(
		static_cast<UINT>(primitiveCount + 2),
		static_cast<UINT>(startIndex),
		static_cast<INT>(baseIndex));
}

// ======================================================================
