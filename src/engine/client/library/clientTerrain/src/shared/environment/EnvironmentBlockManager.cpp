//===================================================================
//
// EnvironmentBlockManager.cpp
// asommers
//
// copyright 2002, sony online entertainment
// 
//===================================================================

#include "clientTerrain/FirstClientTerrain.h"
#include "clientTerrain/EnvironmentBlockManager.h"

#include "clientTerrain/EnvironmentBlock.h"
#include "sharedFile/Iff.h"
#include "sharedFoundation/PointerDeleter.h"
#include "sharedTerrain/EnvironmentGroup.h"
#include "sharedUtility/DataTable.h"

#include <algorithm>
#include <map>

//===================================================================

namespace EnvironmentBlockManagerNamespace
{
	enum ColumnData
	{
		CD_name,
		CD_weatherIndex,
		CD_gradientSkyTextureName,
		CD_cloudLayerBottomShaderTemplateName,
		CD_cloudLayerBottomShaderSize,
		CD_cloudLayerBottomSpeed,
		CD_cloudLayerTopShaderTemplateName,
		CD_cloudLayerTopShaderSize,
		CD_cloudLayerTopSpeed,
		CD_colorRampFileName,
		CD_shadowsEnabled,
		CD_fogEnabled,
		CD_minimumFogDensity,
		CD_maximumFogDensity,
		CD_cameraAppearanceTemplateName,
		CD_dayEnvironmentTextureName,
		CD_nightEnvironmentTextureName,
		CD_day1AmbientSoundTemplateName,
		CD_day2AmbientSoundTemplateName,
		CD_night1AmbientSoundTemplateName,
		CD_night2AmbientSoundTemplateName,
		CD_firstMusicSoundTemplateName,
		CD_sunriseMusicSoundTemplateName,
		CD_sunsetMusicSoundTemplateName,
		CD_windSpeedScale
	};
}

using namespace EnvironmentBlockManagerNamespace;

//===================================================================
// PUBLIC EnvironmentBlockManager
//===================================================================

EnvironmentBlockManager::EnvironmentBlockManager (const EnvironmentGroup* const environmentGroup, const char* const fileName) :
	m_environmentBlockMap (new EnvironmentBlockMap),
	m_defaultEnvironmentBlock (new EnvironmentBlock ()),
	m_dataTable (NULL),
	m_pendingRowMap (new PendingRowMap)
{
	load (environmentGroup, fileName);

	//--
	EnvironmentBlockData data;
	data.m_name = "_default";
	m_defaultEnvironmentBlock->setData (data);
}

//-------------------------------------------------------------------

EnvironmentBlockManager::~EnvironmentBlockManager ()
{
	std::for_each (m_environmentBlockMap->begin (), m_environmentBlockMap->end (), PointerDeleterPairSecond ());
	delete m_environmentBlockMap;

	delete m_defaultEnvironmentBlock;

	delete m_pendingRowMap;
	delete m_dataTable;
}

//-------------------------------------------------------------------

const EnvironmentBlock* EnvironmentBlockManager::getEnvironmentBlock (const int familyId, const int weatherIndex) const
{
	const int key = familyId << 16 | weatherIndex;
	EnvironmentBlockMap::iterator iter = m_environmentBlockMap->find (key);
	if (iter != m_environmentBlockMap->end ())
		return iter->second;

	//-- lazy realization: the key was parsed at load time but its assets not yet fetched
	PendingRowMap::iterator pendingIter = m_pendingRowMap->find (key);
	if (pendingIter != m_pendingRowMap->end ())
	{
		const int row = pendingIter->second;
		m_pendingRowMap->erase (pendingIter);
		realizeRow (key, row);

		iter = m_environmentBlockMap->find (key);
		if (iter != m_environmentBlockMap->end ())
			return iter->second;
	}

	//-- if not found, return environment block 0
	if (weatherIndex != 0)
		return getEnvironmentBlock (familyId, 0);

	return m_defaultEnvironmentBlock;
}

//-------------------------------------------------------------------

const EnvironmentBlock* EnvironmentBlockManager::getDefaultEnvironmentBlock () const
{
	return m_defaultEnvironmentBlock;
}

//===================================================================
// PRIVATE EnvironmentBlockManager
//===================================================================

void EnvironmentBlockManager::load (const EnvironmentGroup* const environmentGroup, const char* fileName)
{
	Iff iff;
	if (iff.open (fileName, true))
	{
		//-- parse the table but defer per-row asset realization (setData) to first use;
		//   the DataTable is retained for the manager's lifetime so deferred rows stay readable
		m_dataTable = new DataTable;
		m_dataTable->load (iff);

		const int numberOfRows = m_dataTable->getNumRows ();
		int row;
		for (row = 0; row < numberOfRows; ++row)
		{
			const char* const name         = m_dataTable->getStringValue (CD_name, row);
			const int         weatherIndex = m_dataTable->getIntValue    (CD_weatherIndex, row);

			if (environmentGroup->hasFamily (name))
			{
				const int familyId = environmentGroup->getFamilyId (name);
				const int key      = familyId << 16 | weatherIndex;
				IGNORE_RETURN (m_pendingRowMap->insert (std::make_pair (key, row)));
			}
			else
				DEBUG_WARNING (true, ("EnvironmentBlockManager::load: environment block file %s specifies family %s not found within terrain file", fileName, name));
		}
	}
}

//-------------------------------------------------------------------

void EnvironmentBlockManager::realizeRow (const int key, const int row) const
{
	NOT_NULL (m_dataTable);

	EnvironmentBlockData data;
	data.m_name                               = m_dataTable->getStringValue (CD_name, row);
	data.m_familyId                           = key >> 16;
	data.m_weatherIndex                       = m_dataTable->getIntValue    (CD_weatherIndex, row);
	data.m_gradientSkyTextureName             = m_dataTable->getStringValue (CD_gradientSkyTextureName, row);
	data.m_cloudLayerBottomShaderTemplateName = m_dataTable->getStringValue (CD_cloudLayerBottomShaderTemplateName, row);
	data.m_cloudLayerBottomShaderSize         = m_dataTable->getFloatValue  (CD_cloudLayerBottomShaderSize, row);
	data.m_cloudLayerBottomSpeed              = m_dataTable->getFloatValue  (CD_cloudLayerBottomSpeed, row);
	data.m_cloudLayerTopShaderTemplateName    = m_dataTable->getStringValue (CD_cloudLayerTopShaderTemplateName, row);
	data.m_cloudLayerTopShaderSize            = m_dataTable->getFloatValue  (CD_cloudLayerTopShaderSize, row);
	data.m_cloudLayerTopSpeed                 = m_dataTable->getFloatValue  (CD_cloudLayerTopSpeed, row);
	data.m_colorRampFileName                  = m_dataTable->getStringValue (CD_colorRampFileName, row);
	data.m_shadowsEnabled                     = m_dataTable->getIntValue    (CD_shadowsEnabled, row) != 0;
	data.m_fogEnabled                         = m_dataTable->getIntValue    (CD_fogEnabled, row) != 0;
	data.m_minimumFogDensity                  = m_dataTable->getFloatValue  (CD_minimumFogDensity, row);
	data.m_maximumFogDensity                  = m_dataTable->getFloatValue  (CD_maximumFogDensity, row);
	data.m_cameraAppearanceTemplateName       = m_dataTable->getStringValue (CD_cameraAppearanceTemplateName, row);
	data.m_dayEnvironmentTextureName          = m_dataTable->getStringValue (CD_dayEnvironmentTextureName, row);
	data.m_nightEnvironmentTextureName        = m_dataTable->getStringValue (CD_nightEnvironmentTextureName, row);
	data.m_day1AmbientSoundTemplateName       = m_dataTable->getStringValue (CD_day1AmbientSoundTemplateName, row);
	data.m_day2AmbientSoundTemplateName       = m_dataTable->getStringValue (CD_day2AmbientSoundTemplateName, row);
	data.m_night1AmbientSoundTemplateName     = m_dataTable->getStringValue (CD_night1AmbientSoundTemplateName, row);
	data.m_night2AmbientSoundTemplateName     = m_dataTable->getStringValue (CD_night2AmbientSoundTemplateName, row);
	data.m_firstMusicSoundTemplateName        = m_dataTable->getStringValue (CD_firstMusicSoundTemplateName, row);
	data.m_sunriseMusicSoundTemplateName      = m_dataTable->getStringValue (CD_sunriseMusicSoundTemplateName, row);
	data.m_sunsetMusicSoundTemplateName       = m_dataTable->getStringValue (CD_sunsetMusicSoundTemplateName, row);
	data.m_windSpeedScale                     = m_dataTable->getFloatValue  (CD_windSpeedScale, row);

	EnvironmentBlock* const environmentBlock = new EnvironmentBlock ();
	environmentBlock->setData (data);

	IGNORE_RETURN (m_environmentBlockMap->insert (std::make_pair (key, environmentBlock)));
}

//===================================================================
