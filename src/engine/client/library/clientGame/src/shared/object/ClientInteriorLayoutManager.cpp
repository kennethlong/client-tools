// ======================================================================
//
// ClientInteriorLayoutManager.cpp
// Copyright 2004, Sony Online Entertainment
//
// ======================================================================

#include "clientGame/FirstClientGame.h"
#include "clientGame/ClientInteriorLayoutManager.h"

#include "clientGame/ClientBuildingObjectTemplate.h"   // v32 refresh: reloadInteriorLayout (the layout is cached on the TEMPLATE)
#include "sharedObject/NetworkIdManager.h"         // v32 refresh: resolve the building by id
#include "clientGame/TangibleObject.h"
#include "clientGraphics/RenderWorld.h"
#include "sharedDebug/InstallTimer.h"
#include "sharedDebug/DebugFlags.h"
#include "sharedFoundation/ConfigFile.h"
#include "sharedFoundation/ExitChain.h"
#include "sharedFoundation/TemporaryCrcString.h"
#include "sharedObject/CellProperty.h"
#include "sharedObject/ObjectTemplateList.h"
#include "sharedObject/ObjectWatcherList.h"
#include "sharedObject/PortalProperty.h"
#include "sharedUtility/InteriorLayoutReaderWriter.h"

#include <typeinfo>   // createInteriorLayoutObject names the wrong class it refused

// ======================================================================

namespace ClientInteriorLayoutManagerNamespace
{
	bool ms_disableLazyInteriorLayoutCreation;
	bool ms_logApplyInteriorLayoutCreates;

	bool ms_disableInteriorLayouts;

	// CONSULT-46 fix #2: max interior-layout objects created per frame. 0 = unlimited
	// (byte-identical original behavior). >0 spreads a cell's create burst across frames to
	// smooth the zone-in stutter. Resume cursor lives on CellProperty (no dangling pointers).
	int ms_maxInteriorCreatesPerFrame;

	void remove();

	ClientObject *createInteriorLayoutObject(CrcString const &objectTemplateName, char const *buildingTemplateName, char const *layoutFileName);
}

using namespace ClientInteriorLayoutManagerNamespace;

// ======================================================================

void ClientInteriorLayoutManager::install(bool const disableLazyInteriorLayoutCreation)
{
	InstallTimer const installTimer("ClientInteriorLayoutManager::install");

	ms_disableLazyInteriorLayoutCreation = ConfigFile::getKeyBool("ClientGame/ClientInteriorLayoutManager", "disableLazyInteriorLayoutCreation", disableLazyInteriorLayoutCreation);

	ms_disableInteriorLayouts = !ConfigFile::getKeyBool("ClientGame", "useInteriorLayoutFiles", true);

	// CONSULT-46 fix #2: per-frame interior-layout create budget (0 = unlimited = original).
	ms_maxInteriorCreatesPerFrame = ConfigFile::getKeyInt("ClientGame/ClientInteriorLayoutManager", "maxInteriorCreatesPerFrame", 0);

	DebugFlags::registerFlag(ms_logApplyInteriorLayoutCreates, "ClientGame/ClientInteriorLayoutManager", "logApplyInteriorLayoutCreates");

	ExitChain::add(remove, "ClientInteriorLayoutManagerNamespace::remove");
}

// ----------------------------------------------------------------------

void ClientInteriorLayoutManagerNamespace::remove()
{
	DebugFlags::unregisterFlag(ms_logApplyInteriorLayoutCreates);
}

// ----------------------------------------------------------------------
/**
 * Create one interior-layout object, REFUSING anything that is not a ClientObject.
 *
 * 2026-08-06. Both create sites in this file used to read
 *
 *     ClientObject * const o = safe_cast<ClientObject *>(ObjectTemplateList::createObject(name));
 *     if (o) { ...use it... } else DEBUG_WARNING(true, ("...Object will be skipped."));
 *
 * which is unsound in Release on BOTH halves:
 *
 *   * safe_cast is a bare static_cast in Release (SafeCast.h:16-18 -- it only dynamic_casts and
 *     asserts in Debug), and ObjectTemplateList::createObject returns whatever the template's own
 *     createObject() built. Any template class that does not override it gets the BASE
 *     implementation, `new Object(this, NetworkId::cms_invalid)` (ObjectTemplate.cpp:155-158) --
 *     a plain Object, not a ClientObject. So a row naming a template of the wrong CLASS yields a
 *     non-null, wrongly-typed pointer that sails through the null check, and the next virtual call
 *     -- endBaselines(), addToWorld() -- dispatches through a vtable slot read from whatever the
 *     Object layout happens to hold at that offset.
 *   * the only diagnostic was DEBUG_WARNING, which is NOP in Release (Fatal.h:50-52). The one
 *     build where this crashes is the one build that says nothing about it.
 *
 * Not hypothetical: .ilf rows are authored data, and SWG-Toolkit hit exactly this class of
 * mismatch from the placement side on 2026-08-04 (a substring filter admitted
 * object/draft_schematic/furniture/* -- crafting schematics handed over as world props; the AV
 * was an indirect call through a pointer read out of string data). A bad row that reaches an .ilf
 * is strictly worse than a bad placement: it crashes on EVERY subsequent load of that building,
 * for anyone who has the file.
 *
 * asClientObject() is the Release-correct discriminator -- virtual, 0 in the base
 * (Object.cpp:2628-2631) -- and is already what update() below uses on the portal owner. On either
 * failure we WARN (compiled in, unlike DEBUG_WARNING; routes to the report log via
 * Fatal.cpp InternalWarning) and return 0, so the caller skips the row -- which is the behaviour
 * the original diagnostic already promised.
 *
 * The wrongly-classed Object is OURS to dispose of: nothing else has a reference to it yet. delete
 * is safe here because ~Object handles the never-added case (`if (m_inWorld)` / `if
 * (m_attachedToObject)`, Object.cpp:846-868) and both callers run from the alter phase
 * (GroundScene::update via IOET_Update), outside the setDisallowObjectDelete window that wraps
 * IoWinManager::draw (Game.cpp:1690-1704).
 *
 * Neither half of this is novel: SwgCuiQuestJournal already takes createObject's Object * and
 * narrows it with asClientObject() (:1118-1123), and disposes of the result with a plain delete
 * (:388, :1105). This function just puts that idiom where the .ilf path always needed it.
 */
ClientObject *ClientInteriorLayoutManagerNamespace::createInteriorLayoutObject(CrcString const &objectTemplateName, char const *const buildingTemplateName, char const *const layoutFileName)
{
	Object * const object = ObjectTemplateList::createObject(objectTemplateName);
	if (!object)
	{
		WARNING(true, ("Object template %s specified building layout %s which specified invalid interior object template name %s.  Object will be skipped.", buildingTemplateName, layoutFileName, objectTemplateName.getString()));
		return 0;
	}

	ClientObject * const clientObject = object->asClientObject();
	if (!clientObject)
	{
		WARNING(true, ("Object template %s specified building layout %s which specified interior object template name %s of the WRONG CLASS: it created a [%s], which is not a ClientObject.  Object will be skipped.", buildingTemplateName, layoutFileName, objectTemplateName.getString(), typeid(*object).name()));
		delete object;
		return 0;
	}

	return clientObject;
}

// ----------------------------------------------------------------------

void ClientInteriorLayoutManager::update()
{
	if (ms_disableLazyInteriorLayoutCreation || ms_disableInteriorLayouts)
		return;

	// CONSULT-46 fix #2: per-frame create budget shared across ALL cells this frame
	// (ms_maxInteriorCreatesPerFrame; 0 = unlimited = original behavior). When spent we stop and
	// resume next frame from each cell's persisted created-count cursor (lives on CellProperty;
	// reset by removeFromWorld on unload -> no dangling pointer, no missing/duplicate objects).
	bool const throttled = (ms_maxInteriorCreatesPerFrame > 0);
	int        remainingBudget = ms_maxInteriorCreatesPerFrame;

	//-- find all visible cells
	RenderWorld::CellPropertyList const & cellPropertyList = RenderWorld::getVisibleCells();
	for (size_t i = 0; i < cellPropertyList.size(); ++i)
	{
		if (throttled && remainingBudget <= 0)
			break;   // budget spent this frame; unapplied cells stay eligible -> resume next frame

		CellProperty const * const cellProperty = cellPropertyList[i];
		if (!cellProperty || cellProperty == CellProperty::getWorldCellProperty())
			continue;

		//-- Verify that the cell is in the world
		if (!cellProperty->getOwner().isInWorld())
			continue;

		//-- If we have already applied the interior layout, skip this cell
		if (cellProperty->getAppliedInteriorLayout())
			continue;

		//-- Get the portal property's owner
		PortalProperty const * const portalProperty = cellProperty->getPortalProperty();
		if (!portalProperty)
		{
			cellProperty->setAppliedInteriorLayout();   // nothing to create -> latch done (orig behavior)
			continue;
		}

		Object & owner = const_cast<Object &>(portalProperty->getOwner());

		//-- Only tangible objects can have interior layouts
		TangibleObject * const tangibleObject = owner.asClientObject() ? owner.asClientObject()->asTangibleObject() : 0;
		if (!tangibleObject)
		{
			cellProperty->setAppliedInteriorLayout();
			continue;
		}

		InteriorLayoutReaderWriter const * const interiorLayout = tangibleObject->getInteriorLayout();
		if (!interiorLayout)
		{
			cellProperty->setAppliedInteriorLayout();
			continue;
		}

		TemporaryCrcString const cellName(cellProperty->getCellName(), true);
		int const numberOfObjects = interiorLayout->getNumberOfObjects(cellName);

		//-- Resume at the persisted cursor (0 first time; advanced if a prior frame ran out of
		//   budget mid-cell; reset to 0 by CellProperty::removeFromWorld on unload -> no dup/dangle).
		int objectIndex = cellProperty->getInteriorLayoutCreatedCount();
		for (; objectIndex < numberOfObjects; ++objectIndex)
		{
			if (throttled && remainingBudget <= 0)
				break;

			CrcString const & objectTemplateName = interiorLayout->getObjectTemplateName(cellName, objectIndex);
			Transform const & transform_o2p = interiorLayout->getTransform_o2p(cellName, objectIndex);

			//-- Create the object (class-checked; refuses + reports a wrong-class template rather
			//   than static_cast-ing it into a crash -- see createInteriorLayoutObject)
			ClientObject * const interiorObject = createInteriorLayoutObject(objectTemplateName, tangibleObject->getObjectTemplateName(), interiorLayout->getFileName().getString());
			if (interiorObject)
			{
				DEBUG_REPORT_LOG(ms_logApplyInteriorLayoutCreates, ("ilf created [%s]\n", objectTemplateName.getString()));

				tangibleObject->addClientOnlyInteriorLayoutObject(interiorObject);

				interiorObject->setParentCell(const_cast<CellProperty *>(cellProperty));
				CellProperty::setPortalTransitionsEnabled(false);
					interiorObject->setTransform_o2p(transform_o2p);
				CellProperty::setPortalTransitionsEnabled(true);

				RenderWorld::addObjectNotifications(*interiorObject);

				interiorObject->endBaselines();
				interiorObject->addToWorld();
			}

			if (throttled)
				--remainingBudget;
		}

		//-- Persist progress; latch "applied" ONLY when the whole cell is complete.
		cellProperty->setInteriorLayoutCreatedCount(objectIndex);
		if (objectIndex >= numberOfObjects)
			cellProperty->setAppliedInteriorLayout();
	}
}

// ----------------------------------------------------------------------

void ClientInteriorLayoutManager::applyInteriorLayout(TangibleObject * const tangibleObject, InteriorLayoutReaderWriter const * const interiorLayout, char const * const fileName)
{
	UNREF(fileName);

	if (!ms_disableLazyInteriorLayoutCreation || ms_disableInteriorLayouts)
		return;

	if (!tangibleObject)
		return;

	if (!interiorLayout)
		return;

	//-- Are there any objects in the interior layout file?
	int const totalNumberOfObjects = interiorLayout->getNumberOfObjects();
	if (totalNumberOfObjects == 0)
		return;

	//-- Does the tangible object have a priority?
	PortalProperty * const portalProperty = tangibleObject->getPortalProperty();
	if (!portalProperty)
		return;

	//-- Add the objects in the interior layout file to the object
	for (int i = 0; i < interiorLayout->getNumberOfCellNames(); ++i)
	{
		CrcString const & cellName = interiorLayout->getCellName(i);

		CellProperty * const cellProperty = portalProperty->getCell(cellName.getString());
		if (cellProperty)
		{
			int const numberOfObjects = interiorLayout->getNumberOfObjects(cellName);
			for (int j = 0; j < numberOfObjects; ++j)
			{
				CrcString const & objectTemplateName = interiorLayout->getObjectTemplateName(cellName, j);
				Transform const & transform_o2p = interiorLayout->getTransform_o2p(cellName, j);

				//-- Create the object (class-checked -- see createInteriorLayoutObject)
				ClientObject * const object = createInteriorLayoutObject(objectTemplateName, tangibleObject->getObjectTemplateName(), fileName);
				if (object)
				{
					DEBUG_REPORT_LOG(ms_logApplyInteriorLayoutCreates, ("ilf created [%s]\n", objectTemplateName.getString()));

					tangibleObject->addClientOnlyInteriorLayoutObject(object);

					object->setParentCell(cellProperty);
					CellProperty::setPortalTransitionsEnabled(false);
						object->setTransform_o2p(transform_o2p);
					CellProperty::setPortalTransitionsEnabled(true);

					RenderWorld::addObjectNotifications(*object);

					object->endBaselines();
					object->addToWorld();
				}
			}
		}
		else
			DEBUG_WARNING(true, ("Object template %s specified building layout %s which specified invalid cell name %s.  Object will be skipped.", tangibleObject->getObjectTemplateName(), fileName, cellName.getString()));
	}
}

// ======================================================================

// ======================================================================
// v32: per-building interior REFRESH -- re-apply a changed .ilf to ONE building
// without a scene reload.
//
// WHY THIS EXISTS: the alternative was the toolkit's full "reload current scene",
// which tears down and rebuilds the whole snapshot. That is a sledgehammer for a
// moved chair, and after the unload guard it leaves occupied buildings showing
// their PRE-EDIT on-disk state (a kept root collides with the re-parsed node and
// loses its sphere handle). This path retires that residual: nothing is torn down,
// so nothing is kept, so nothing goes stale.
//
// THREE THINGS MUST HAPPEN, and missing any one is a silent no-op:
//   1. the building's client-only interior objects are deleted -- and ONLY those.
//      NOT "every client-cached object in the cells": the consumer's unpersisted
//      placements are wsAddObject-minted snapshot nodes living in the same cells,
//      with no on-disk copy, and sweeping them would look exactly like the editor
//      discarding a modder's work.
//   2. the TEMPLATE's cached InteriorLayoutReaderWriter is reloaded. The layout is
//      cached on ClientBuildingObjectTemplate, not on the object, so steps 1+3
//      alone would faithfully rebuild the OLD .ilf -- the exact failure mode that
//      makes the whole feature useless.
//   3. each cell's applied-latch is cleared AND its resume cursor reset. The latch
//      alone would still resume at the old cursor and create nothing.
//
// Re-creation itself is left to the existing budgeted update(), so a large cantina
// spreads across frames under maxInteriorCreatesPerFrame instead of hitching --
// which is the other reason this beats a reload.
//
// 1 = refreshed · 0 = no such object / not a POB / no layout · -1 = layout reload failed.
// ======================================================================

#if !defined(_WIN64)   // the advertise surface is 32-bit only (engine_advertise.cpp:94) -- no x64 export surface, so this shim would be a dead symbol there
extern "C" int __cdecl engine_refreshInteriorLayout (__int64 buildingNetworkId)
{
	Object * const object = NetworkIdManager::getObjectById (NetworkId (static_cast<NetworkId::NetworkIdType> (buildingNetworkId)));
	if (!object)
		return 0;

	ClientObject * const clientObject = object->asClientObject ();
	TangibleObject * const tangibleObject = clientObject ? clientObject->asTangibleObject () : 0;
	if (!tangibleObject)
		return 0;

	PortalProperty * const portalProperty = tangibleObject->getPortalProperty ();
	if (!portalProperty)
		return 0;

	//-- (2) reload the TEMPLATE's cached layout first, so the re-create in (3) reads the new file
	const ClientBuildingObjectTemplate * const buildingTemplate = dynamic_cast<const ClientBuildingObjectTemplate *> (tangibleObject->getObjectTemplate ());
	if (!buildingTemplate)
		return 0;

	if (!buildingTemplate->reloadInteriorLayout ())
		return -1;

	//-- (1) delete ONLY this building's interior-layout objects
	int const deleted = tangibleObject->clearClientOnlyInteriorLayoutObjects ();

	//-- (3) re-arm every cell so the budgeted update() re-creates from the reloaded layout
	int cellsArmed = 0;
	int const numberOfCells = portalProperty->getNumberOfCells ();
	for (int i = 0; i < numberOfCells; ++i)
	{
		CellProperty const * const cellProperty = portalProperty->getCell (i);
		if (!cellProperty)
			continue;

		cellProperty->clearAppliedInteriorLayout ();
		cellProperty->setInteriorLayoutCreatedCount (0);
		++cellsArmed;
	}

	REPORT_LOG (true, ("[editor.ilf] refreshInteriorLayout OK id=%I64d deleted=%d cellsArmed=%d\n", buildingNetworkId, deleted, cellsArmed));

	return 1;
}
#endif // !defined(_WIN64)

// ======================================================================
