// ======================================================================
//
// Direct3d11_StateCache.h
// Phase 11 D3D11 renderer plugin -- central state-object cache + binding
// tracker + draw-call dispatch.
//
// Plan 11-06 (Wave 6).
//
// What lives here:
//   * Hash-cached immutable state objects (ID3D11RasterizerState,
//     ID3D11BlendState, ID3D11DepthStencilState, ID3D11SamplerState)
//   * Currently-bound shader / VB / IB / SRV / sampler tracking
//   * applyPreDrawState() helper -- pushes pending bindings before each
//     draw call (lazy bind on draw, not on setter)
//   * The Gl_api draw* slot bodies (Draw / DrawIndexed dispatch)
//   * State-setter slot bodies (setFillMode / setCullMode / setFog / ...)
//
// Per RESEARCH Pitfall 4 (SRV/sampler split): ms_boundSRV and
// ms_boundSampler are tracked INDEPENDENTLY. D3D9's SetTexture coupled
// resource + sampler; D3D11 splits them. setGlobalTexture binds the SRV
// only; sampler state is bound via setShader / per-shader sampler config.
//
// Per RESEARCH Pitfall 5 (cbuffer flush on draw): draw dispatch invokes
// Direct3d11_ConstantBuffer flushers so that per-object / per-material
// shadows from Plan 11-05 setters land on the GPU at draw time.
//
// Per CONTEXT D-13: no D3DPOOL_MANAGED / OnLostDevice / OnResetDevice.
//
// ======================================================================

#ifndef INCLUDED_Direct3d11_StateCache_H
#define INCLUDED_Direct3d11_StateCache_H

// ======================================================================

#include <d3d11.h>
#include <DirectXMath.h>   // Plan 17-09 (GAP-5): DirectX::XMFLOAT4 in setVertexMaterialColors

#include "clientGraphics/Graphics.def"
#include "sharedFoundation/Tag.h"

class Direct3d11_PixelShaderProgramData;
class Direct3d11_VertexShaderData;
struct Direct3d11_FfpCombinerDesc;   // detail-blend fix 2026-06-11
class StaticShader;
class Texture;
class HardwareVertexBuffer;
class HardwareIndexBuffer;
class VertexBufferFormat;
class PackedArgb;
class Transform;
class Vector;

// ======================================================================

class Direct3d11_StateCache
{
public:

	static void install();
	static void remove();
	static void beginFrame();
	static void endFrame();

	// ------------------------------------------------------------------
	// Slot-setter bodies (called from Direct3d11.cpp slot bindings).

	static void setFillMode(GlFillMode mode);
	static void setCullMode(GlCullMode mode);
	static void setAntialiasEnabled(bool enabled);

	static void setViewport(int x, int y, int width, int height, real minZ, real maxZ);
	static void setScissorRect(bool enabled, int x, int y, int width, int height);
	static void setWorldToCameraTransform(Transform const &objectToWorld, Vector const &cameraPosition);
	static void setProjectionMatrix(GlMatrix4x4 const &projectionMatrix);
	static void setFog(bool enabled, real density, PackedArgb const &color);

	// Fog fix 2026-06-11: per-pass fog COLOR mode for the generated-PS fog
	// lerp (engine ShaderImplementationPass::FogMode passed as int:
	// 0=Normal scene color, 1=Black, 2=White). Mirrors D3D9's per-pass
	// D3DRS_FOGCOLOR resolve (Direct3d9_StaticShaderData.cpp:932-948).
	// Called per-pass from Direct3d11_StaticShaderData::apply.
	static void setPsFogMode(int fogMode);
	static void setObjectToWorldTransformAndScale(Transform const &objectToWorld, Vector const &scale);

	// Plan 17-08 (GAP-4) Producer A: map a WORLD-space direction into the
	// current object's local frame using the cached object->world transform
	// (Transform::rotate_p2l). Used by Direct3d11_StaticShaderData::apply to
	// orient the dot3 light direction against the asset PS's object-space
	// normal_o before writing the b0 SwgVertexConstants shadow.
	static Vector worldDirectionToObjectLocal(Vector const &worldDir);

	static void setTextureTransform(int stage, bool enabled, int dimension, bool projected, real const *transform);
	static void setAlphaFadeOpacity(bool enabled, float opacity);

	// CONSULT-40 (2026-06-08): D3D9-parity alpha-fade state. The dot3 asset PS reads its
	// output-alpha control from b0 c1.a (alphaFadeOpacityEnabled) and c2.a (alphaFadeOpacity)
	// -- see Direct3d9_LightManager.cpp:582,584. setAlphaFadeOpacity records enabled/opacity
	// here; Direct3d11_StaticShaderData::apply patches the per-draw dot3 c1.a/c2.a from these
	// getters (NOT the light's own diffuse/specular alpha, which previously leaked in and forced
	// every dot3 surface into the fade branch -> translucent faces). Mirrors D3D9 ms_alphaFadeOpacity.
	static bool  getAlphaFadeEnabled();
	static float getAlphaFadeOpacity();

	// Plan 11-09.15 Iter-39B: per-pass alpha-blend control. Called from
	// Direct3d11_ShaderImplementationData::apply() to honor the engine's
	// per-shader m_alphaBlendEnable (UI canvas / particles / glow billboards
	// need TRUE; opaque world content needs FALSE). Mirrors D3D9 sibling
	// Direct3d9::setAlphaBlendEnable. Default in install() is FALSE
	// (Iter-39A flipped from Iter-32A's TRUE); shaders that need blending
	// flip via this setter.
	static void setAlphaBlendEnable(bool enabled);

	// Plan 11-09.15 Iter-43: per-pass alpha-blend FACTORS. Iter-39C wired
	// the BlendEnable boolean per-pass but left SrcBlend/DestBlend/BlendOp
	// hard-coded to the install() default (SrcAlpha/InvSrcAlpha/Add -- the
	// standard "over" composite). UI shaders use that default, but particles
	// + glow billboards use additive blend (One/One/Add) and a few effects
	// use other modes. Pre-Iter-43 every blended draw collapsed to "over"
	// blending, which against typical particle textures (alpha=1.0 because
	// authoring expected additive) renders as a solid opaque tinted quad
	// instead of a soft glow. Mirrors the D3D9 path's RSM(D3DRS_SRCBLEND/
	// DESTBLEND/BLENDOP, ...) per-pass writes in Direct3d9_ShaderImplementation
	// Data.cpp:259-261. Takes D3D11_BLEND / D3D11_BLEND_OP enums; the engine-
	// enum mapping lives at the caller (StaticShaderData::apply).
	//
	// Iter-43 was REVERTED at the call site after Mos Eisley smoke regressed.
	// CODEX + Cursor consult diagnosed the regression: blend factors alone
	// run ahead of per-pass depth (44A) + alpha test (44B) + real PS (Phase
	// 12). Re-land in Iter-44C after A+B close the prerequisite gaps.
	static void setAlphaBlendFactors(D3D11_BLEND srcBlend,
	                                 D3D11_BLEND destBlend,
	                                 D3D11_BLEND_OP blendOp);

	// Plan 11-09.15 Iter-44A: per-pass depth-stencil state. D3D9 sibling
	// pushes ZENABLE / ZWRITEENABLE / ZFUNC per-pass via RSB / RSM
	// (Direct3d9_ShaderImplementationData.cpp:255-257); D3D11 was using only
	// the install() defaults (DepthEnable=TRUE, WriteMask=ALL, Func=LESS_EQUAL)
	// for every draw. That's wrong for skeletal multi-pass shaders -- the
	// back-of-head opaque pass writes depth, the eye/decal pass with
	// m_zWrite=false expects to not write depth and the eye pass with
	// m_zCompare=Always passes regardless of depth. The Mos Eisley smoke
	// surfaced the symptom: when the player avatar faced away, the eye
	// texture rendered visibly through the back of the head. Wiring per-pass
	// depth from engPass->m_zEnable / m_zWrite / m_zCompare closes that.
	// Takes D3D11 enums directly; engine-enum-to-D3D11 mapping lives at the
	// caller (StaticShaderData::apply).
	static void setDepthEnable(bool enabled);
	static void setDepthWriteEnable(bool writeEnabled);
	static void setDepthCompareFunc(D3D11_COMPARISON_FUNC func);

	// Plan 11-09.15 Iter-44A: per-pass color-write mask. D3D9 sibling pushes
	// COLORWRITEENABLE per-pass (Direct3d9_ShaderImplementationData.cpp). The
	// engine's m_writeEnable is a uint8 bitmask with D3D9 D3DCOLORWRITEENABLE_*
	// flags (RED=1, GREEN=2, BLUE=4, ALPHA=8) which are bitwise-identical to
	// D3D11_COLOR_WRITE_ENABLE_* so the value passes through unchanged.
	static void setColorWriteEnable(uint8 writeMask);

	// Plan 11-09.15 Iter-44B: per-pass alpha-test. D3D11 has no FFP alpha-test
	// state; the engine's m_alphaTestEnable + reference value gets pushed into
	// PS cbuffer slot 1 (Direct3d11_PSAlphaTestCB) where the dynamic-generated
	// PS reads it and conditionally clip()s. Reference is 0..1 (normalized
	// from engine's uint8 via /255.0f). Caller (StaticShaderData::apply)
	// resolves the engine's TAG-based reference to a uint8 before normalizing.
	static void setAlphaTest(bool enabled, float reference);

	// CONSULT-53 (2026-06-28): per-pass premultiply-alpha flag (PS cbuffer slot 1
	// alphaTest.z). The generated PS does `col.rgb *= col.a` when set, so additive
	// (One-dest, SrcColor) glow passes with straight-alpha textures don't leak
	// masked-region RGB into the additive sum (mirrors D3D9's premultiplied feed).
	// Caller (StaticShaderData::apply) sets it for additive passes only.
	static void setAlphaPremultiply(bool enabled);

	// Detail-blend fix 2026-06-11: per-pass texture factor for the FFP
	// combiner-cascade generated PS (TA_textureFactor = D3D9 TFACTOR).
	// Shares the b1 cbuffer with alpha-test (Direct3d11_PSAlphaTestCB).
	// Caller (StaticShaderData::apply) passes the pass's resolved
	// textureFactor, or white when the pass declares none (D3D9 default
	// TFACTOR is 0xFFFFFFFF).
	static void setPsTextureFactor(float r, float g, float b, float a);

	// Detail-blend fix 2026-06-11: per-pass FFP texture-stage combiner
	// descriptor for the cascade PS generator (see Direct3d11_FfpCombinerDesc
	// in Direct3d11_PixelShaderProgramData.h). StaticShaderData::apply pushes
	// the active pass's descriptor for FFP stage-based passes and null for
	// asset-PS passes (clears -- prevents a stale cascade leaking onto the
	// next fallback-PS draw). The descriptor is COPIED into shadow state;
	// the caller's storage may be rebuilt on graphics-data restore.
	static void setFfpCombiner(Direct3d11_FfpCombinerDesc const *desc);

	// Pitfall 4: SRV binding only -- sampler is bound independently.
	static void setGlobalTexture(Tag textureTag, Texture const &texture);
	static void releaseAllGlobalTextures();

	// Plan 11-09.14: per-pass PS resource binding via the shadow arrays.
	// Direct3d11_StaticShaderData::apply() routes the per-pass diffuse
	// texture + sampler through these setters rather than calling
	// PSSet*ShaderResources / PSSetSamplers directly. The setters write
	// into ms_boundSRV[]/ms_boundSampler[]; applyPreDrawState flushes the
	// full shadow with one PSSetShaderResources + PSSetSamplers per draw
	// (Pitfall 4 lazy-bind model preserved).
	//
	// Slot must be in [0, kMaxSRVs / kMaxSamplers). Out-of-range silently
	// ignored to avoid undefined behaviour. SRV null is a valid value
	// (explicit unbind). Sampler null reverts the slot to the default
	// sampler (avoids the unbound-sampler hazard CODEX flagged in the
	// Plan 11-09.13 Iter-4 stale-state diagnostic).
	static void setPixelShaderResource(int slot, ID3D11ShaderResourceView *srv);
	static void setPixelShaderSampler(int slot, D3D11_SAMPLER_DESC const &desc);
	static void setPixelShaderSampler(int slot, ID3D11SamplerState *sampler);

	// Geometry binding.
	static void setVertexBuffer(HardwareVertexBuffer const &vb);
	static void setIndexBuffer(HardwareIndexBuffer const &ib);

	// Plan 11-09.7: multi-stream VertexBufferVector bind. Called by
	// Direct3d11_VertexBufferVectorData::bind after iterating the engine
	// VBVector + extracting per-stream ID3D11Buffer / stride / offset /
	// format. Mutually exclusive with setVertexBuffer (each clears the
	// other's state). resolveShaders + applyPreDrawState branch on the
	// internal ms_currentVBVectorActive flag set here.
	static void setVertexBufferVectorBindState(
		int streamCount,
		ID3D11Buffer * const *buffers,
		UINT const *strides,
		UINT const *offsets,
		VertexBufferFormat const * const *formats,
		int sliceFirstVertex,
		int sliceVertexCount);

	// Shader binding (drives input-layout reselect + VS/PS swap).
	static void setBadVertexShaderStaticShader(StaticShader const *shader);
	static void setStaticShader(StaticShader const &shader, int pass);

	// Plan 11-09 Iter-1: per-pass VS + PS pointer wiring. Called by
	// Direct3d11_StaticShaderData::apply once per draw. Assigning a non-null
	// pointer marks ms_geometryRebindNeeded so the next applyPreDrawState
	// reselects the input layout against the new VS signature.
	static void setCurrentVSData(Direct3d11_VertexShaderData const *vs, uint32 textureCoordinateSetKey);   // Plan 17-09: per-key variant
	static void setCurrentPSData(Direct3d11_PixelShaderProgramData const *ps);

	// Plan 17-09 (GAP-5): materialSpecularPower for the VS object-space specular
	// (lightData[25].w). Mirror Direct3d9_StateCache::setSpecularPower; fed per-pass
	// from StaticShaderData::apply. A 0 power makes pow() emit NaN -> dark/wrong bump.
	static void setSpecularPower(float power);

	// Plan 17-09 (GAP-5 bump-arm audit): feed the VS material color struct
	// (c11 diffuse / c13 specular / c14 emissive) of Direct3d11_VertexSlot0CB.
	// Mirrors Direct3d9_StaticShaderData.cpp:862 `setVertexShaderConstants(
	// VSCR_material, &m_material, 5)`. The asset VS reads cb0[14] (emissive) as
	// the diffuse accumulator SEED and cb0[13] (specular) as the specular output
	// tint (proven by the a_specmap_bump_vs20 disasm). composeSlot0Shadow only
	// fed material[4].x (power) -> these stayed 0, so the VS specular output was
	// always black and the diffuse seed dropped the material emissive vs D3D9.
	// Called per-pass from StaticShaderData::apply next to setSpecularPower.
	static void setVertexMaterialColors(
		DirectX::XMFLOAT4 const &diffuse,
		DirectX::XMFLOAT4 const &specular,
		DirectX::XMFLOAT4 const &emissive);

	// Plan 11-09 Iter-2.7 Fix C: raw register-index write into the VS slot 0
	// cbuffer shadow. Mirrors D3D9's Direct3d9_StateCache::setVertexShaderConstants
	// API. registerIndex is the D3D9 c-register number; data points to
	// `count` XMFLOAT4 (16 byte) entries. Bounds-checked against the
	// Direct3d11_VertexSlot0CB capacity (72 registers); out-of-range calls
	// are silently dropped with a one-time probe log.
	// Called from Direct3d11.cpp's setVertexShaderUserConstants_impl with
	// registerIndex = VCSR_userConstant0 + index = 52 + index.
	static void setVSConstants(int registerIndex, void const *data, int count);

	// ------------------------------------------------------------------
	// Draw-call dispatch -- the Gl_api draw* slot bodies.
	// Non-partial slots use the engine's currently-set VB/IB ranges
	// implicitly; partial slots take explicit ranges.

	static void drawPointList();
	static void drawLineList();
	static void drawLineStrip();
	static void drawTriangleList();
	static void drawTriangleStrip();
	static void drawTriangleFan();
	static void drawQuadList();

	static void drawIndexedPointList();
	static void drawIndexedLineList();
	static void drawIndexedLineStrip();
	static void drawIndexedTriangleList();
	static void drawIndexedTriangleStrip();
	static void drawIndexedTriangleFan();

	static void drawPartialPointList(int startVertex, int primitiveCount);
	static void drawPartialLineList(int startVertex, int primitiveCount);
	static void drawPartialLineStrip(int startVertex, int primitiveCount);
	static void drawPartialTriangleList(int startVertex, int primitiveCount);
	static void drawPartialTriangleStrip(int startVertex, int primitiveCount);
	static void drawPartialTriangleFan(int startVertex, int primitiveCount);

	static void drawPartialIndexedPointList(int baseIndex, int minimumVertexIndex, int numberOfVertices, int startIndex, int primitiveCount);
	static void drawPartialIndexedLineList(int baseIndex, int minimumVertexIndex, int numberOfVertices, int startIndex, int primitiveCount);
	static void drawPartialIndexedLineStrip(int baseIndex, int minimumVertexIndex, int numberOfVertices, int startIndex, int primitiveCount);
	static void drawPartialIndexedTriangleList(int baseIndex, int minimumVertexIndex, int numberOfVertices, int startIndex, int primitiveCount);
	static void drawPartialIndexedTriangleStrip(int baseIndex, int minimumVertexIndex, int numberOfVertices, int startIndex, int primitiveCount);
	static void drawPartialIndexedTriangleFan(int baseIndex, int minimumVertexIndex, int numberOfVertices, int startIndex, int primitiveCount);
};

// ======================================================================

#endif
