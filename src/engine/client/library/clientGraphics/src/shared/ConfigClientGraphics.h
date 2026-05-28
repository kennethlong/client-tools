// ======================================================================
//
// ConfigClientGraphics.h
// Portions copyright 1999 Bootprint Entertainment
// Portions copyright 2001-2002 Sony Online Entertainment
// All Rights Reserved.
//
// ======================================================================

#ifndef INCLUDED_ConfigClientGraphics_H
#define INCLUDED_ConfigClientGraphics_H

// ======================================================================

class ConfigClientGraphics
{
public:

	struct Defaults
	{
		int   rasterMajor;
		bool  windowed;
		bool  skipInitialClearViewport;
		int   screenWidth;
		int   screenHeight;
		int   colorBufferBitDepth;
		int   alphaBufferBitDepth;
		int   zBufferBitDepth;
		int   stencilBufferBitDepth;
	};

public:

	static void           install(const Defaults &defaults);

	static int            getRasterMajor();

	static int            getScreenWidth();
	static int            getScreenHeight();

	static bool           getWindowed();
	static bool           getSkipInitialClearViewport();
	static bool           getBorderlessWindow();

	static int            getColorBufferBitDepth();
	static int            getAlphaBufferBitDepth();
	static int            getZBufferBitDepth();
	static int            getStencilBufferBitDepth();

	static bool           getValidateShaderImplementations();
	static bool           getDisableMultiStreamVertexBuffers();

	static bool           getLogBadCustomizationData();

	static float          getDpvsMinimumObjectWidth();
	static float          getDpvsMinimumObjectHeight();
	static float          getDpvsMinimumObjectOpacity();
	static float          getDpvsImageScale();

	static bool           getUseHardwareMouseCursor();
	static bool           getHardwareMouseCursorUseOriginalAlpha();
	static bool           getConstrainMouseCursorToWindow();

	static bool           getEnableLightScaling();

	static int            getDiscardHighestMipMapLevels();
	static int            getDiscardHighestNormalMipMapLevels();

	static bool           getLoadAllAssetsRegardlessOfShaderCapability();

	static bool           getLoadGpa();

	// Phase 17 (Plan 17-01) / D-01/D-03: gate flag for the PSRC language census
	// emitted from ShaderImplementationPassPixelShaderProgram::load_0000. KEPT
	// (not throwaway) so Phase 20 can re-run the census on an open-world boot;
	// default false so the D3D9 path is behaviorally untouched when off.
	static bool           getPsrcCensus();

private:

	ConfigClientGraphics();
	ConfigClientGraphics(const ConfigClientGraphics &);
	ConfigClientGraphics &operator =(const ConfigClientGraphics &);
};

// ======================================================================

#endif
