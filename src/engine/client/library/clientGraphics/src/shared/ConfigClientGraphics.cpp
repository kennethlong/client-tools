// ======================================================================
//
// ConfigClientGraphics.cpp
// Portions copyright 1999 Bootprint Entertainment
// Portions copyright 2001-2002 Sony Online Entertainment
// All Rights Reserved.
//
// ======================================================================

#include "clientGraphics/FirstClientGraphics.h"
#include "clientGraphics/ConfigClientGraphics.h"

#include "sharedFoundation/ConfigFile.h"
#include "clientGraphics/Graphics.def"

// ======================================================================

namespace ConfigClientGraphicsNamespace
{
	int   ms_rasterMajor;

	int   ms_screenWidth;
	int   ms_screenHeight;

	bool  ms_windowed;
	bool  ms_skipInitialClearViewport;
	bool  ms_borderlessWindow;

	int   ms_colorBufferBitDepth;
	int   ms_alphaBufferBitDepth;
	int   ms_zBufferBitDepth;
	int   ms_stencilBufferBitDepth;

	bool  ms_validateShaderImplementations;
	bool  ms_disableMultiStreamVertexBuffers;
	bool  ms_screenShotBackBuffer;
	bool  ms_nPatchTest;

	bool  ms_logBadCustomizationData;

	float ms_dpvsMinimumObjectWidth;
	float ms_dpvsMinimumObjectHeight;
	float ms_dpvsMinimumObjectOpacity;
	float ms_dpvsImageScale;

	bool  ms_useHardwareMouseCursor;
	bool  ms_hardwareMouseCursorUseOriginalAlpha;
	bool  ms_constrainMouseCursorToWindow;

	bool  ms_enableLightScaling;

	int   ms_discardHighestMipMapLevels;
	int   ms_discardHighestNormalMipMapLevels;

	bool  ms_loadAllAssetsRegardlessOfShaderCapability;

	bool  ms_loadGpa;

	// Phase 17 (Plan 17-01): gate flag for the PSRC language census emitted from
	// ShaderImplementationPassPixelShaderProgram::load_0000. [ClientGraphics]
	// section (NOT [Direct3d11] — clientGraphics cannot link the plugin's
	// ConfigDirect3d11). Default false; D3D9 behavior unchanged when off.
	bool  ms_psrcCensus;

	// Phase 24 (Plan 24-01) / HARD-01: cached tri-state DPVS occlusion knob, parsed
	// once at install from [ClientGraphics/Dpvs] occlusionMode. Default DOM_off (D-02).
	ConfigClientGraphics::DpvsOcclusionMode ms_dpvsOcclusionMode;
}

using namespace ConfigClientGraphicsNamespace;

// ======================================================================

#define KEY_INT(a,b)     (ms_ ## a = ConfigFile::getKeyInt("ClientGraphics", #a, b))
#define KEY_FLOAT(a,b)   (ms_ ## a = ConfigFile::getKeyFloat("ClientGraphics", #a, b))
#define KEY_BOOL(a,b)    (ms_ ## a = ConfigFile::getKeyBool("ClientGraphics", #a, b))

// @todo remove these when the options have been migrated
#define KEY_SF_INT(a,b)     (ms_ ## a = ConfigFile::getKeyInt("ClientGraphics", #a, ConfigFile::getKeyInt("SharedFoundation", #a, b)))
#define KEY_SF_BOOL(a,b)    (ms_ ## a = ConfigFile::getKeyBool("ClientGraphics", #a, ConfigFile::getKeyBool("SharedFoundation", #a, b)))

// ======================================================================
// Determine the configuration information
//
// Remarks:
//
//   This routine inspects the ConfigFile class to set some variables for rapid access
//   by the rest of the engine.

void ConfigClientGraphics::install (const Defaults &defaults)
{
	KEY_INT(rasterMajor,                          defaults.rasterMajor);

	KEY_SF_INT(screenWidth,                       defaults.screenWidth);
	KEY_SF_INT(screenHeight,                      defaults.screenHeight);

	KEY_SF_BOOL(windowed,                         defaults.windowed);
	KEY_BOOL(skipInitialClearViewport,            defaults.skipInitialClearViewport);
	KEY_BOOL(borderlessWindow,                    false);

	KEY_INT(colorBufferBitDepth,                  defaults.colorBufferBitDepth);
	KEY_INT(alphaBufferBitDepth,                  defaults.alphaBufferBitDepth);
	KEY_INT(zBufferBitDepth,                      defaults.zBufferBitDepth);
	KEY_INT(stencilBufferBitDepth,                defaults.stencilBufferBitDepth);

	KEY_BOOL(validateShaderImplementations,       true);
	KEY_BOOL(screenShotBackBuffer,                false);
	// Phase 24 (Plan 24-01) / D-07: default flipped from true to false so multi-stream
	// skinned VBs are the engine default, making the disableMultiStreamVertexBuffers=false
	// cfg override key redundant (deleted in plan 24-03).
	KEY_BOOL(disableMultiStreamVertexBuffers,     false);
	KEY_BOOL(nPatchTest,                          false);

	KEY_BOOL(logBadCustomizationData,             false);

	KEY_FLOAT(dpvsMinimumObjectWidth,             8.0f);
	KEY_FLOAT(dpvsMinimumObjectHeight,            8.0f);
	KEY_FLOAT(dpvsMinimumObjectOpacity,           1.0f);
	KEY_FLOAT(dpvsImageScale,                     0.5f);

	KEY_BOOL(useHardwareMouseCursor,              true);
	KEY_BOOL(hardwareMouseCursorUseOriginalAlpha, false);
	KEY_BOOL(constrainMouseCursorToWindow,        true);

	KEY_BOOL(enableLightScaling,                  true);

	KEY_INT(discardHighestMipMapLevels,           0);
	KEY_INT(discardHighestNormalMipMapLevels,     0);

	KEY_BOOL(loadAllAssetsRegardlessOfShaderCapability, false);

	KEY_BOOL(loadGpa,                             false);

	// Phase 17 (Plan 17-01): default false so D3D9 behavior is unchanged when off.
	KEY_BOOL(psrcCensus,                          false);

	// Phase 24 (Plan 24-01) / HARD-01: tri-state DPVS occlusion knob. This is a
	// STRING in the [ClientGraphics/Dpvs] section (NOT a bool in [ClientGraphics]),
	// so it is read directly via getKeyString rather than the KEY_BOOL macro. Parse
	// the value to the cached enum with case-insensitive compares; any unrecognized
	// or empty value falls back to DOM_off (D-02 default off; T-24-01 safe fallback,
	// no array indexing on the raw string).
	{
		char const * const occlusionModeStr = ConfigFile::getKeyString("ClientGraphics/Dpvs", "occlusionMode", "off");
		if (occlusionModeStr != NULL && _stricmp(occlusionModeStr, "auto") == 0)
			ms_dpvsOcclusionMode = ConfigClientGraphics::DOM_auto;
		else if (occlusionModeStr != NULL && _stricmp(occlusionModeStr, "on") == 0)
			ms_dpvsOcclusionMode = ConfigClientGraphics::DOM_on;
		else
			ms_dpvsOcclusionMode = ConfigClientGraphics::DOM_off; // D-02: default off
	}
}

// ----------------------------------------------------------------------
/**
 * Return the initial raster major number for the game.
 * 
 * @return The initial value of the raster major number for the graphics subsystem
 */

int ConfigClientGraphics::getRasterMajor()
{
	return ms_rasterMajor;
}

// ----------------------------------------------------------------------
/**
 * Return the initial screen width for the game.
 * 
 * @return The initial value of the screen width for the graphics subsystem
 */

int ConfigClientGraphics::getScreenWidth()
{
	return ms_screenWidth;
}

// ----------------------------------------------------------------------
/**
 * Return the initial screen height for the game.
 * 
 * @return The initial value of the screen height for the graphics subsystem
 */

int ConfigClientGraphics::getScreenHeight()
{
	return ms_screenHeight;
}

// ----------------------------------------------------------------------
/**
 * return the initial windowed state for the game.
 * 
 * @return The initial value of the windowed flag for the graphics subsystem
 */

bool ConfigClientGraphics::getWindowed()
{
	return ms_windowed;
}

//----------------------------------------------------------------------

bool ConfigClientGraphics::getBorderlessWindow()
{
	return ms_borderlessWindow;
}

// ----------------------------------------------------------------------
/**
 * Return the initial screen color depth (in bits) for the game.
 * 
 * @return The initial value of the color depth (in bits) for the graphics subsystem
 */

int ConfigClientGraphics::getColorBufferBitDepth()
{
	return ms_colorBufferBitDepth;
}

// ----------------------------------------------------------------------
/**
 * Return the initial screen alpha depth (in bits) for the game.
 * 
 * @return The initial value of the alpha depth (in bits) for the graphics subsystem
 */

int ConfigClientGraphics::getAlphaBufferBitDepth()
{
	return ms_alphaBufferBitDepth;
}

// ----------------------------------------------------------------------
/**
 * Return the initial z buffer depth (in bits) for the game.
 * 
 * @return The initial value of the z buffer depth (in bits) for the graphics subsystem
 */

int ConfigClientGraphics::getZBufferBitDepth()
{
	return ms_zBufferBitDepth;
}

// ----------------------------------------------------------------------
/**
 * Return the initial screen depth (in bits) for the game.
 * 
 * @return The initial value of the screen depth (in bits) for the graphics subsystem
 */

int ConfigClientGraphics::getStencilBufferBitDepth()
{
	return ms_stencilBufferBitDepth;
}

// ----------------------------------------------------------------------

bool ConfigClientGraphics::getValidateShaderImplementations()
{
	return ms_validateShaderImplementations;
}

// ----------------------------------------------------------------------

bool ConfigClientGraphics::getDisableMultiStreamVertexBuffers()
{
	return ms_disableMultiStreamVertexBuffers;
}

// ----------------------------------------------------------------------

bool ConfigClientGraphics::getLogBadCustomizationData()
{
	return ms_logBadCustomizationData;
}

// ----------------------------------------------------------------------

float ConfigClientGraphics::getDpvsMinimumObjectWidth()
{
	return ms_dpvsMinimumObjectWidth;
}

// ----------------------------------------------------------------------

float ConfigClientGraphics::getDpvsMinimumObjectHeight()
{
	return ms_dpvsMinimumObjectHeight;
}

// ----------------------------------------------------------------------

float ConfigClientGraphics::getDpvsMinimumObjectOpacity()
{
	return ms_dpvsMinimumObjectOpacity;
}

// ----------------------------------------------------------------------

float ConfigClientGraphics::getDpvsImageScale()
{
	return ms_dpvsImageScale;
}

// ----------------------------------------------------------------------

bool ConfigClientGraphics::getUseHardwareMouseCursor()
{
	return ms_useHardwareMouseCursor;
}

// ----------------------------------------------------------------------

bool ConfigClientGraphics::getHardwareMouseCursorUseOriginalAlpha()
{
	return ms_hardwareMouseCursorUseOriginalAlpha;
}

// ----------------------------------------------------------------------

bool ConfigClientGraphics::getConstrainMouseCursorToWindow()
{
	return ms_constrainMouseCursorToWindow;
}

// ----------------------------------------------------------------------

bool ConfigClientGraphics::getEnableLightScaling()
{
	return ms_enableLightScaling;
}

// ----------------------------------------------------------------------

int ConfigClientGraphics::getDiscardHighestMipMapLevels()
{
	return ms_discardHighestMipMapLevels;
}

// ----------------------------------------------------------------------

int ConfigClientGraphics::getDiscardHighestNormalMipMapLevels()
{
	return ms_discardHighestNormalMipMapLevels;
}

// ----------------------------------------------------------------------

bool ConfigClientGraphics::getLoadAllAssetsRegardlessOfShaderCapability()
{
	return ms_loadAllAssetsRegardlessOfShaderCapability;
}

// ----------------------------------------------------------------------

bool ConfigClientGraphics::getLoadGpa()
{
	return ms_loadGpa;
}

// ----------------------------------------------------------------------

bool ConfigClientGraphics::getSkipInitialClearViewport()
{
	return ms_skipInitialClearViewport;
}

// ----------------------------------------------------------------------

bool ConfigClientGraphics::getPsrcCensus()
{
	return ms_psrcCensus;
}

// ----------------------------------------------------------------------

// Phase 24 (Plan 24-01) / HARD-01: return the cached tri-state DPVS occlusion knob
// (D-01 knob; D-02 default off). Consumed per-frame by RenderWorld::drawScene.
ConfigClientGraphics::DpvsOcclusionMode ConfigClientGraphics::getDpvsOcclusionMode()
{
	return ms_dpvsOcclusionMode;
}

// ======================================================================
