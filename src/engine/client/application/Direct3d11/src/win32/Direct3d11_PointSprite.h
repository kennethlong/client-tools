// ======================================================================
//
// Direct3d11_PointSprite.h
//
// D3D9 point sprites (D3DRS_POINTSPRITEENABLE / D3DRS_POINTSIZE), emulated
// with a geometry shader that expands each point into a screen-aligned
// quad. D3D10+ removed the fixed-function mechanism entirely -- a point
// primitive is exactly one pixel and there is no size state -- so without
// this every point sprite renders as a single hardware pixel. This closes
// the Plan 11-09.10 deferral (the no-op setPointSize/setPointSpriteEnable
// slots) that left the space star field near-invisible; convicted by the
// 2026-08-15 gl05-vs-gl11 A/B (150 vs 1,218 bright star pixels, gl05
// matching the dense stock look).
//
// One hand-written geometry shader covers the entire feature because
// exactly one thing in the client draws point sprites: StarAppearance
// (shader/stars.sht, a fixed two-pixel size). Its vertex program outputs
// {SV_Position, COLOR0, FOG} and nothing else, and its pixel program
// samples no texture, so the expander needs no texture coordinates. The
// interpolant struct in the shader source is written to that signature
// EXACTLY -- a geometry shader sits between the vertex and pixel stages
// and must match the vertex output AND satisfy the pixel input, register
// for register; any other vertex program enabling point sprites would fail
// stage linkage (draws rejected, nothing rendered) rather than render
// wrongly.
//
// The half-extent is (pixels / viewportDimension) * w; multiplying by w
// cancels the perspective divide so the quad is a constant pixel size at
// any depth, which is D3D9's behaviour with POINTSCALEENABLE off -- the
// only mode the engine ever uses. Distance attenuation
// (setPointScaleEnable/Factor) has no engine caller; the state is recorded
// and reported by name if ever enabled, not implemented.
//
// Design adapted from the Galaxies-Reborn D3D11 port's
// Direct3d11_PointSprite (swg-source-x64-dx11), which live-verified the
// GS-expansion approach; the implementation here is rebuilt against this
// plugin's shader framework (per-shader natural signatures, no shared
// interpolant block) and draw path (applyPreDrawState chokepoint).
//
// Kill switch: [Direct3d11] pointSprites=false (ConfigDirect3d11) skips
// install entirely -- point lists then draw as 1-pixel points, the
// pre-fix behaviour.
//
// ======================================================================

#ifndef INCLUDED_Direct3d11_PointSprite_H
#define INCLUDED_Direct3d11_PointSprite_H

// ======================================================================

class Direct3d11_PointSprite
{
public:
	static void install();
	static void remove();

	// The D3D9 render states, recorded here. Wired to the Gl_api
	// setPointSize / setPointSpriteEnable slots.
	static void setEnabled(bool enabled);
	static void setSize(float size);

	// States with no engine caller. Recorded so they are not silently
	// lost; scale (distance attenuation) is reported once if ever enabled.
	static void setSizeMinimum(float size);
	static void setSizeMaximum(float size);
	static void setScaleEnabled(bool enabled);
	static void setScaleFactor(float a, float b, float c);

	// Bind the expander and push the size for a point-list draw, or unbind
	// it for anything else -- a geometry shader left bound would expand the
	// next triangle list's vertices into quads. Called from
	// applyPreDrawState, the single chokepoint every draw passes through.
	static void apply(bool isPointList);

private:
	Direct3d11_PointSprite();
	Direct3d11_PointSprite(Direct3d11_PointSprite const &);
	Direct3d11_PointSprite &operator=(Direct3d11_PointSprite const &);
};

// ======================================================================

#endif
