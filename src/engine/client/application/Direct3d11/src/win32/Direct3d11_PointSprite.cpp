// ======================================================================
//
// Direct3d11_PointSprite.cpp
//
// See the header for the design rationale. The one non-obvious constraint
// repeated here because it lives next to the shader text: the interpolant
// struct below must match the compiled stars vertex-shader output
// signature EXACTLY (semantic names, order, and therefore registers).
// The live stars.vsh (//hlsl, patch_10.tre) outputs
//   POSITION0 (-> SV_Position under SM4), COLOR0 (float4), FOG (float)
// and the capture-verified vs_4_0 signature is o0=SV_Position, o1=COLOR0,
// o2.x=FOG. The GS body only touches position; everything else passes
// through untouched.
//
// ======================================================================

#include "FirstDirect3d11.h"
#include "Direct3d11_PointSprite.h"

#include "ConfigDirect3d11.h"
#include "Direct3d11_Device.h"

#include <cstring>
#include <d3d11.h>
#include <d3dcompiler.h>

// ======================================================================

namespace Direct3d11_PointSpriteNamespace
{
	// The expander. Self-contained source: no #includes, no macros -- the
	// corpus rewrite pipeline (Direct3d11_HlslRewrite) never sees this
	// text, and `point` stays the gs_4_0 primitive-type keyword.
	//
	// Corner order (-x,-y) (-x,+y) (+x,-y) (+x,+y) is the strip winding
	// that reads as front-facing under the scene's cull mode (live-verified
	// in the Galaxies-Reborn port this is adapted from).
	char const cms_shaderSource[] =
		"cbuffer PointSpriteConstants : register(b0)\n"
		"{\n"
		"	float4 pointSpriteExtent;\n"   // xy = clip-space half-extent; zw unused
		"};\n"
		"\n"
		"struct StarVertex\n"
		"{\n"
		"	float4 position : SV_Position;\n"
		"	float4 diffuse  : COLOR0;\n"
		"	float  fog      : FOG;\n"
		"};\n"
		"\n"
		"[maxvertexcount(4)]\n"
		"void main(point StarVertex input[1], inout TriangleStream<StarVertex> stream)\n"
		"{\n"
		"	StarVertex source = input[0];\n"
		"\n"
		// Multiplying by w cancels the perspective divide, so the quad is a
		// constant pixel size at any depth (D3D9 POINTSCALEENABLE-off).
		"	float2 extent = pointSpriteExtent.xy * source.position.w;\n"
		"\n"
		"	StarVertex corner = source;\n"
		"	corner.position = float4(source.position.x - extent.x, source.position.y - extent.y, source.position.z, source.position.w);\n"
		"	stream.Append(corner);\n"
		"	corner.position = float4(source.position.x - extent.x, source.position.y + extent.y, source.position.z, source.position.w);\n"
		"	stream.Append(corner);\n"
		"	corner.position = float4(source.position.x + extent.x, source.position.y - extent.y, source.position.z, source.position.w);\n"
		"	stream.Append(corner);\n"
		"	corner.position = float4(source.position.x + extent.x, source.position.y + extent.y, source.position.z, source.position.w);\n"
		"	stream.Append(corner);\n"
		"\n"
		"	stream.RestartStrip();\n"
		"}\n";

	ID3D11GeometryShader *ms_shader;
	ID3D11Buffer *ms_constants;

	bool  ms_enabled;
	float ms_size        = 1.0f;
	float ms_sizeMinimum = 1.0f;
	float ms_sizeMaximum = 64.0f;

	// Last-uploaded clip-space extents, so a draw that changes nothing
	// writes nothing.
	float ms_uploadedX = -1.0f;
	float ms_uploadedY = -1.0f;

	bool ms_bound;
	bool ms_reportedScale;
}
using namespace Direct3d11_PointSpriteNamespace;

// ======================================================================

void Direct3d11_PointSprite::install()
{
	if (!ConfigDirect3d11::getPointSprites())
		return;

	ID3D11Device *const device = Direct3d11_Device::getDevice();
	if (!device)
		return;

	// Compiled once at install, not on first use -- the star field draws in
	// the first frame of a space scene.
	ID3DBlob *bytecode = NULL;
	ID3DBlob *errors = NULL;
	HRESULT hresult = D3DCompile(cms_shaderSource, strlen(cms_shaderSource), "Direct3d11_PointSprite",
		NULL, NULL, "main", "gs_4_0", 0, 0, &bytecode, &errors);

	if (FAILED(hresult) || !bytecode)
	{
		// Point sprites stay off: single-pixel points rather than nothing.
		WARNING(true, ("Direct3d11: the point sprite geometry shader did not compile (%s); point sprites will render as single pixels.",
			errors ? static_cast<char const *>(errors->GetBufferPointer()) : "no compiler output"));
		if (errors)
			errors->Release();
		if (bytecode)
			bytecode->Release();
		return;
	}
	if (errors)
		errors->Release();

	hresult = device->CreateGeometryShader(bytecode->GetBufferPointer(), bytecode->GetBufferSize(), NULL, &ms_shader);
	bytecode->Release();

	if (FAILED(hresult) || !ms_shader)
	{
		WARNING(true, ("Direct3d11: the point sprite geometry shader could not be created (hr=0x%08x).", static_cast<unsigned int>(hresult)));
		ms_shader = NULL;
		return;
	}

	D3D11_BUFFER_DESC description;
	memset(&description, 0, sizeof(description));
	description.ByteWidth = 16;
	description.Usage = D3D11_USAGE_DYNAMIC;
	description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	hresult = device->CreateBuffer(&description, NULL, &ms_constants);
	if (FAILED(hresult) || !ms_constants)
	{
		WARNING(true, ("Direct3d11: the point sprite constant buffer could not be created (hr=0x%08x).", static_cast<unsigned int>(hresult)));
		ms_shader->Release();
		ms_shader = NULL;
		ms_constants = NULL;
		return;
	}
}

// ----------------------------------------------------------------------

void Direct3d11_PointSprite::remove()
{
	if (ms_constants)
	{
		ms_constants->Release();
		ms_constants = NULL;
	}

	if (ms_shader)
	{
		ms_shader->Release();
		ms_shader = NULL;
	}

	ms_bound = false;
}

// ======================================================================

void Direct3d11_PointSprite::setEnabled(bool enabled)
{
	ms_enabled = enabled;
}

// ----------------------------------------------------------------------

void Direct3d11_PointSprite::setSize(float size)
{
	ms_size = size;
}

// ----------------------------------------------------------------------

void Direct3d11_PointSprite::setSizeMinimum(float size)
{
	ms_sizeMinimum = size;
}

// ----------------------------------------------------------------------

void Direct3d11_PointSprite::setSizeMaximum(float size)
{
	ms_sizeMaximum = size;
}

// ----------------------------------------------------------------------

void Direct3d11_PointSprite::setScaleEnabled(bool enabled)
{
	// Distance attenuation has no engine caller; recorded-and-reported,
	// not implemented -- with nothing asking for it there is no behaviour
	// to reproduce and no way to check a guess.
	if (enabled && !ms_reportedScale)
	{
		ms_reportedScale = true;
		WARNING(true, ("Direct3d11: point size distance attenuation was switched on; it is not implemented and point sprites stay a constant pixel size."));
	}
}

// ----------------------------------------------------------------------

void Direct3d11_PointSprite::setScaleFactor(float a, float b, float c)
{
	UNREF(a);
	UNREF(b);
	UNREF(c);
}

// ======================================================================

void Direct3d11_PointSprite::apply(bool isPointList)
{
	bool const want = isPointList && ms_enabled && ms_shader && ms_constants;

	if (!want)
	{
		// Unbinding matters more than binding: a geometry shader left bound
		// would expand the next triangle list's vertices into quads.
		if (ms_bound)
		{
			ms_bound = false;
			ID3D11DeviceContext *const context = Direct3d11_Device::getContext();
			if (context)
				context->GSSetShader(NULL, NULL, 0);
		}
		return;
	}

	ID3D11DeviceContext *const context = Direct3d11_Device::getContext();
	if (!context)
		return;

	// The half-extent in clip space, from the CURRENT viewport (D3D9 point
	// size is in render-target pixels, and the star field draws into the
	// screen-sized scene RT). NDC spans -1..1, so half of a size-pixel quad
	// is size/dimension.
	UINT viewportCount = 1;
	D3D11_VIEWPORT viewport;
	memset(&viewport, 0, sizeof(viewport));
	context->RSGetViewports(&viewportCount, &viewport);
	if (viewportCount == 0 || viewport.Width <= 0.0f || viewport.Height <= 0.0f)
		return;

	float size = ms_size;
	if (size < ms_sizeMinimum)
		size = ms_sizeMinimum;
	if (size > ms_sizeMaximum)
		size = ms_sizeMaximum;

	float const extentX = size / viewport.Width;
	float const extentY = size / viewport.Height;

	if (extentX != ms_uploadedX || extentY != ms_uploadedY)
	{
		D3D11_MAPPED_SUBRESOURCE mapped;
		memset(&mapped, 0, sizeof(mapped));

		if (SUCCEEDED(context->Map(ms_constants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			float const values[4] = {extentX, extentY, 0.0f, 0.0f};
			memcpy(mapped.pData, values, sizeof(values));
			context->Unmap(ms_constants, 0);

			ms_uploadedX = extentX;
			ms_uploadedY = extentY;
		}
	}

	// Bind unconditionally on every point-list draw (one per frame in
	// practice): survives any external context state clear without
	// tracking it.
	ms_bound = true;
	context->GSSetShader(ms_shader, NULL, 0);
	context->GSSetConstantBuffers(0, 1, &ms_constants);
}

// ======================================================================
