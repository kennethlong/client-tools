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

#include "clientGraphics/Graphics.def"
#include "sharedFoundation/Tag.h"

class Direct3d11_PixelShaderProgramData;
class Direct3d11_VertexShaderData;
class StaticShader;
class Texture;
class HardwareVertexBuffer;
class HardwareIndexBuffer;
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
	static void setObjectToWorldTransformAndScale(Transform const &objectToWorld, Vector const &scale);
	static void setTextureTransform(int stage, bool enabled, int dimension, bool projected, real const *transform);
	static void setAlphaFadeOpacity(bool enabled, float opacity);

	// Pitfall 4: SRV binding only -- sampler is bound independently.
	static void setGlobalTexture(Tag textureTag, Texture const &texture);
	static void releaseAllGlobalTextures();

	// Geometry binding.
	static void setVertexBuffer(HardwareVertexBuffer const &vb);
	static void setIndexBuffer(HardwareIndexBuffer const &ib);

	// Shader binding (drives input-layout reselect + VS/PS swap).
	static void setBadVertexShaderStaticShader(StaticShader const *shader);
	static void setStaticShader(StaticShader const &shader, int pass);

	// Plan 11-09 Iter-1: per-pass VS + PS pointer wiring. Called by
	// Direct3d11_StaticShaderData::apply once per draw. Assigning a non-null
	// pointer marks ms_geometryRebindNeeded so the next applyPreDrawState
	// reselects the input layout against the new VS signature.
	static void setCurrentVSData(Direct3d11_VertexShaderData const *vs);
	static void setCurrentPSData(Direct3d11_PixelShaderProgramData const *ps);

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
