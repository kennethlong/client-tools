//===================================================================
//
// EnvironmentBlockManager.h
// asommers
//
// copyright 2002, sony online entertainment
// 
//===================================================================

#ifndef INCLUDED_EnvironmentBlockManager_H
#define INCLUDED_EnvironmentBlockManager_H

//===================================================================

class DataTable;
class EnvironmentBlock;
class EnvironmentGroup;

//===================================================================

class EnvironmentBlockManager
{
public:

	EnvironmentBlockManager (const EnvironmentGroup* environmentGroup, const char* fileName);
	virtual ~EnvironmentBlockManager ();

	const EnvironmentBlock* getEnvironmentBlock (int familyId, int weatherIndex) const;
	const EnvironmentBlock* getDefaultEnvironmentBlock () const;

private:

	void load (const EnvironmentGroup* environmentGroup, const char* fileName);
	void realizeRow (int key, int row) const;

private:

	EnvironmentBlockManager ();
	EnvironmentBlockManager (const EnvironmentBlockManager&);
	EnvironmentBlockManager& operator= (const EnvironmentBlockManager&);

private:

	typedef stdmap<int, EnvironmentBlock*>::fwd EnvironmentBlockMap;
	EnvironmentBlockMap* const m_environmentBlockMap;
	EnvironmentBlock* const    m_defaultEnvironmentBlock;

	// Blocks realize lazily on first getEnvironmentBlock hit: EnvironmentBlock::setData
	// synchronously fetches sky/cloud shaders, environment textures, and decodes the
	// color-ramp image per row, which cost ~0.7s inside the GroundScene ctor when every
	// row was realized up front (stall-loop31674, 2026-07-04). The DataTable is retained
	// so deferred rows can be read later; only weather/family states actually visited
	// ever pay their load cost.
	DataTable*                 m_dataTable;
	typedef stdmap<int, int>::fwd PendingRowMap;
	PendingRowMap* const       m_pendingRowMap;
};

//===================================================================

#endif
