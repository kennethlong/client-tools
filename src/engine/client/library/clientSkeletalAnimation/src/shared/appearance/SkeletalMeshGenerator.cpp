// ======================================================================
//
// SkeletalMeshGenerator.cpp
// Copyright 2001, 2002 Sony Online Entertainment Inc.
// All Rights Reserved.
// 
// ======================================================================

#include "clientSkeletalAnimation/FirstClientSkeletalAnimation.h"
#include "clientSkeletalAnimation/SkeletalMeshGenerator.h"

#include "clientGraphics/ConfigClientGraphics.h"
#include "clientSkeletalAnimation/SkeletalMeshGeneratorTemplate.h"
#include "clientTextureRenderer/TextureRendererTemplate.h"
#include "sharedFoundation/CrcLowerString.h"
#include "sharedFoundation/ExitChain.h"
#include "sharedFoundation/MemoryBlockManager.h"
#include "sharedObject/CustomizationData.h"
#include "sharedObject/RangedIntCustomizationVariable.h"

#include <string>
#include <vector>

// ======================================================================

bool                              SkeletalMeshGenerator::ms_installed;
MemoryBlockManager *SkeletalMeshGenerator::ms_memoryBlockManager;

// ======================================================================
/**
 * Retrieve a reference to the SkeletalMeshGeneratorTemplate instance
 * associated with this SkeletalMeshGenerator instance.
 *
 * @return  the SkeletalMeshGeneratorTemplate instance associated with
 *          this instance.
 */

inline const SkeletalMeshGeneratorTemplate &SkeletalMeshGenerator::getSkeletalMeshGeneratorTemplate() const
{
	return *NON_NULL(safe_cast<const SkeletalMeshGeneratorTemplate*>(&getMeshGeneratorTemplate()));
}

// ======================================================================

void SkeletalMeshGenerator::install()
{
	DEBUG_FATAL(ms_installed, ("SkeletalMeshGenerator already installed.\n"));

	ms_memoryBlockManager = new MemoryBlockManager("SkeletalMeshGenerator", true, sizeof(SkeletalMeshGenerator), 0, 0, 0);
	ms_installed          = true;

	ExitChain::add(remove, "SkeletalMeshGenerator");
}

// ----------------------------------------------------------------------

void *SkeletalMeshGenerator::operator new(size_t size)
{
	DEBUG_FATAL(!ms_installed, ("SkeletalMeshGenerator not installed.\n"));
	DEBUG_FATAL(size != sizeof(SkeletalMeshGenerator), ("size mismatch, derived classes must not use this operator new"));
	UNREF(size);

	return ms_memoryBlockManager->allocate();
}

// ----------------------------------------------------------------------

void SkeletalMeshGenerator::operator delete(void *data)
{
	DEBUG_FATAL(!ms_installed, ("SkeletalMeshGenerator not installed.\n"));

	if (data)
		ms_memoryBlockManager->free(data);
}

// ======================================================================

int SkeletalMeshGenerator::getOcclusionLayer() const
{
	if (!m_isCreated)   // HARDEN I3: release-safe guard (was DEBUG_FATAL-only)
	{
		REPORT_LOG(true, ("[MGN-PROBE] I3-DEREF-AVERTED getOcclusionLayer on UNCREATED [%s]\n", getSkeletalMeshGeneratorTemplate().getName().getString()));
		return -1;   // CONSULT-39 (Cursor): template default is -1; 0 mis-orders composite layers
	}
	return getSkeletalMeshGeneratorTemplate().getOcclusionLayer();
}

// ----------------------------------------------------------------------

void SkeletalMeshGenerator::applySkeletonModifications(Skeleton &skeleton) const
{
	// HARDEN I3 (CONSULT-37): in Release the old DEBUG_FATAL was compiled out, so a call on an unloaded
	// template fell straight through to deref m_blendValues (NULL) / empty template internals = AV. Guard
	// release-safe: log the would-have-fired condition and skip (the appearance re-renders once loaded).
	if (!m_isCreated)
	{
		REPORT_LOG(true, ("[MGN-PROBE] I3-DEREF-AVERTED applySkeletonModifications on UNCREATED [%s]\n", getSkeletalMeshGeneratorTemplate().getName().getString()));
		return;
	}
	getSkeletalMeshGeneratorTemplate().applySkeletonModifications(m_blendValues, skeleton);
}

// ----------------------------------------------------------------------

void SkeletalMeshGenerator::addShaderPrimitives(Appearance &appearance, int lodIndex, CustomizationData *customizationData, const TransformNameMap &transformNameMap, const OcclusionZoneSet &zonesCurrentlyOccluded, OcclusionZoneSet &zonesOccludedByThisLayer, ShaderPrimitiveVector &shaderPrimitives) const
{
	// HARDEN I3 (CONSULT-37): prime AV site during the early zone-in shader-primitive build. In Release the
	// DEBUG_FATAL was compiled out -> deref of an unloaded template. Guard release-safe: log + skip (no
	// shader primitives contributed until the template finishes loading; the appearance rebuilds then).
	if (!m_isCreated)
	{
		REPORT_LOG(true, ("[MGN-PROBE] I3-DEREF-AVERTED addShaderPrimitives on UNCREATED [%s] lod=%d\n", getSkeletalMeshGeneratorTemplate().getName().getString(), lodIndex));
		return;
	}
	getSkeletalMeshGeneratorTemplate().addShaderPrimitives(appearance, lodIndex, customizationData, m_blendValues, transformNameMap, zonesCurrentlyOccluded, zonesOccludedByThisLayer, shaderPrimitives);
}

// ----------------------------------------------------------------------

void SkeletalMeshGenerator::setCustomizationData(CustomizationData *customizationData)
{
	//-- check for assignment of same CustomizationData instance.
	if (customizationData == m_customizationData)
		return;

	//-- release modification callback set on old CustomizationData
	if (m_customizationData)
	{
		m_customizationData->deregisterModificationListener(handleCustomizationModificationStatic, this);
		m_customizationData->release();
	}

	//-- assign new customization data
	m_customizationData = customizationData;

	//-- attach modification callback on new CustomizationData
	if (m_customizationData)
	{
		m_customizationData->registerModificationListener(handleCustomizationModificationStatic, this);

		//-- handle any customizations as any variable may be new
		handleCustomizationModification(*m_customizationData);

		//-- fetch reference
		m_customizationData->fetch();
	}
}

// ----------------------------------------------------------------------

Appearance *SkeletalMeshGenerator::createAppearance() const
{
	if (!m_isCreated)   // HARDEN I3: release-safe guard (was DEBUG_FATAL-only)
	{
		REPORT_LOG(true, ("[MGN-PROBE] I3-DEREF-AVERTED createAppearance on UNCREATED [%s]\n", getSkeletalMeshGeneratorTemplate().getName().getString()));
		return 0;
	}
	return getSkeletalMeshGeneratorTemplate().createAppearance();
}

// ----------------------------------------------------------------------

void SkeletalMeshGenerator::addCustomizationVariables(CustomizationData &customizationData) const
{
	if (!m_isCreated)   // HARDEN I3: release-safe guard (was DEBUG_FATAL-only)
	{
		REPORT_LOG(true, ("[MGN-PROBE] I3-DEREF-AVERTED addCustomizationVariables on UNCREATED [%s]\n", getSkeletalMeshGeneratorTemplate().getName().getString()));
		return;
	}
	getSkeletalMeshGeneratorTemplate().addCustomizationVariables(customizationData);
}

// ----------------------------------------------------------------------

bool SkeletalMeshGenerator::isReadyForUse() const
{
	//-- CONSULT-39: ready iff THIS instance has been created (not merely the template loaded -- the I6
	//   fixup window has isLoaded()==true while a queued instance is not yet created).
	if (m_isCreated)
		return true;

	//-- Permanent-failure escape (CONSULT-40): if the template gave up loading, report ready so the
	//   readiness gate stops the not-ready MRU-pin LIVELOCK in unloadUnusedResources (which would reset the
	//   timer every frame on a never-ready generator). The deref guards still skip on !m_isCreated, so we
	//   render nothing rather than crash. This does NOT force eviction (that's gated on a non-empty shader-
	//   primitive vector); the generator simply persists, invisible, until the NPC despawns -- the correct
	//   outcome for a genuinely missing/corrupt asset after the bounded retries are exhausted.
	return getSkeletalMeshGeneratorTemplate().isLoadPermanentlyFailed();
}

// ----------------------------------------------------------------------

int SkeletalMeshGenerator::getReferencedSkeletonTemplateCount() const
{
	if (!m_isCreated)   // HARDEN I3: release-safe guard (was DEBUG_FATAL-only)
	{
		REPORT_LOG(true, ("[MGN-PROBE] I3-DEREF-AVERTED getReferencedSkeletonTemplateCount on UNCREATED [%s]\n", getSkeletalMeshGeneratorTemplate().getName().getString()));
		return 0;   // no referenced skeletons until loaded -> callers won't index getReferencedSkeletonTemplateName
	}
	return getSkeletalMeshGeneratorTemplate().getReferencedSkeletonTemplateCount();
}

// ----------------------------------------------------------------------

const CrcLowerString &SkeletalMeshGenerator::getReferencedSkeletonTemplateName(int index) const
{
	if (!m_isCreated)   // HARDEN I3: release-safe guard (was DEBUG_FATAL-only)
	{
		REPORT_LOG(true, ("[MGN-PROBE] I3-DEREF-AVERTED getReferencedSkeletonTemplateName on UNCREATED [%s]\n", getSkeletalMeshGeneratorTemplate().getName().getString()));
		static CrcLowerString const s_empty("");   // safe sentinel; count-guard above means this is normally unreachable
		return s_empty;
	}
	return getSkeletalMeshGeneratorTemplate().getReferencedSkeletonTemplateName(index);
}

// ======================================================================

void SkeletalMeshGenerator::remove()
{
	DEBUG_FATAL(!ms_installed, ("SkeletalMeshGenerator not installed"));

	delete ms_memoryBlockManager;
	ms_memoryBlockManager = 0;
}

// ----------------------------------------------------------------------
/**
 * Handle reception of CustomizationData modifications and dispatch
 * to associated SkeletalMeshGenerator instance.
 *
 * @param customizationData  the CustomizationData instance modified.
 * @param context            a casted version of the SkeletalMeshGenerator
 *                           instance pointer.
 */

void SkeletalMeshGenerator::handleCustomizationModificationStatic(const CustomizationData &customizationData, const void *context)
{
	NOT_NULL(context);

	//-- convert context to SkeletalMeshGenerator instance
	SkeletalMeshGenerator *const skeletalMeshGenerator = const_cast<SkeletalMeshGenerator*>(static_cast<const SkeletalMeshGenerator*>(context));

	//-- call the instance handler
	skeletalMeshGenerator->handleCustomizationModification(customizationData);
}

// ======================================================================

SkeletalMeshGenerator::SkeletalMeshGenerator(const SkeletalMeshGeneratorTemplate *meshGeneratorTemplate) :
	MeshGenerator(meshGeneratorTemplate),
	m_customizationData(0),
	m_blendValues(0),
	m_isCreated(false)
{
	//-- get the number of blend shape variables from the template
	NOT_NULL(meshGeneratorTemplate);
	if (meshGeneratorTemplate->isLoaded())
		create();
}

// ----------------------------------------------------------------------

SkeletalMeshGenerator::~SkeletalMeshGenerator()
{
	SkeletalMeshGeneratorTemplate const &mgTemplate = getSkeletalMeshGeneratorTemplate();
	if (!m_isCreated)
	{
		// CONSULT-39: a not-yet-created instance is exactly one still queued in the template's
		// m_uninitializedMeshGenerators (key on m_isCreated, not isLoaded() -- isLoaded() can be true mid
		// fixup while this instance is still queued). Self-remove so the fixup loop never touches us.
		// (Template can't be freed first: SMG holds a refcount on it.) NULL-list case is guarded inside.
		REPORT_LOG(true, ("[MGN-PROBE] RANK1-DESTROY-PENDING ~SkeletalMeshGenerator while uncreated [%s]\n", mgTemplate.getName().getString()));
		mgTemplate.removeAsynchronouslyLoadedMeshGenerator(this);
	}

	delete m_blendValues;

	//-- release modification callback
	if (m_customizationData)
	{
		m_customizationData->deregisterModificationListener(handleCustomizationModificationStatic, this);
		m_customizationData->release();
		m_customizationData = 0;
	}
}

// ----------------------------------------------------------------------
/**
 * Called after the associated SkeletalMeshGeneratorTemplate has finished
 * loading.
 *
 * Implements all code that needs to run after the associated
 * SkeletalMeshGeneratorTemplate has finished loading.  This function
 * is called from the constructor if this instance is created from an already-loaded
 * template; otherwise, it is called by the template when it is done loading.
 */

void SkeletalMeshGenerator::create()
{
	int const blendVariableCount = getSkeletalMeshGeneratorTemplate().getBlendVariableCount();

	if (blendVariableCount > 0)
	{
		// @todo Initialize to mesh generator defaults.  Currently this initializes all
		//       to zero.  This fix will allow the viewer to see default values set by artists.
		m_blendValues = new IntVector(static_cast<IntVector::size_type>(blendVariableCount));
	}

	if (m_customizationData)
	{
		// Update customization data now that we're setup.
		handleCustomizationModification(*m_customizationData);
	}

	//-- CONSULT-39: mark this instance created (LAST, after all state is set) -> the readiness predicate
	//   for isReadyForUse()/the deref guards flips true exactly here.
	m_isCreated = true;

	//-- CONSULT-39 (heal A3b): an already-spawned appearance that requested this generator while the
	//   template was still async-loading cleared/skipped its build (not ready). Now that we're created,
	//   nudge the owning appearance dirty (via the registered meshGeneratorModifiedCallback) so its next
	//   render rebuilds with our contribution. signalModified() only marks dirty -- rebuild is deferred to
	//   render, so this is not a reentrant rebuild.
	signalModified();
}

// ----------------------------------------------------------------------
/**
 * Perform activities necessary when the associated CustomizationData
 * is modified.
 *
 * This function should only be called by handleCustomizationModificationStatic.
 *
 * This function will signal all MeshGenerator modification handlers if
 * and only if a customization variable influencing this SkeletalMeshGenetor 
 * instance is modified from its current value.
 *
 * @param customizationData  this is here for debug purposes --- it should
 *                           be the same as this instance's CustomizationData
 *                           variable.  If not, a DEBUG_FATAL will occur.
 */

void SkeletalMeshGenerator::handleCustomizationModification(const CustomizationData &customizationData)
{
	if (!m_blendValues)
	{
		// Nothing to do, this isn't yet loaded or has nothing to customize.
		return;
	}

	//-- sanity check: the given customizationData should be the same as our internal m_customizationData.
	DEBUG_FATAL(&customizationData != m_customizationData, ("SkeletalMeshGenerator instance is notified of a CustomizationData change not associated with this SkeletalMeshGenerator instance"));
	UNREF(customizationData);
	NOT_NULL(m_customizationData);

	//-- find values for each of the MeshGeneratorTemplate's variable names
	const SkeletalMeshGeneratorTemplate &mgTemplate = getSkeletalMeshGeneratorTemplate();
	bool  modified = false;

	const int variableCount = mgTemplate.getBlendVariableCount();
	DEBUG_FATAL(variableCount != static_cast<int>(m_blendValues->size()), ("out of sync variable data storage"));

	for (int i = 0; i < variableCount; ++i)
	{
		//-- get variable name
		const std::string &variableName = mgTemplate.getBlendVariableName(i);

		//-- get RangedIntCustomizationVariable for the given variable name
		const RangedIntCustomizationVariable *const variable = safe_cast<const RangedIntCustomizationVariable*>(m_customizationData->findConstVariable(variableName));
		if (!variable)
		{
			WARNING(ConfigClientGraphics::getLogBadCustomizationData(), ("SkeletalMeshGenerator [%s]: no customization data variable for mesh blend variable [%s].", mgTemplate.getName().getString(), variableName.c_str()));
			continue;
		}

		//-- assign new value if different from old value
		const int newValue = variable->getValue();
		int &localVariable = (*m_blendValues)[static_cast<size_t>(i)];

		if (localVariable != newValue)
		{
			// assign new value
			localVariable = newValue;

			// keep track that this instance has been modified
			modified = true;
		}
	}

	//-- if the instance has any variables that were modified, inform
	//   the MeshGenerator so it can tell any interested entities about
	//   the change.
	if (modified)
		signalModified();
}

// ======================================================================
