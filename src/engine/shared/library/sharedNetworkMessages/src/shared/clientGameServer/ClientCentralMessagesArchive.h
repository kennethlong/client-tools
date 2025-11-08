// ClientCentralMessages.h
// copyright 2001 Verant Interactive
// Author: Justin Randall

#ifndef	_INCLUDED_ClientCentralMessagesArchive_H
#define	_INCLUDED_ClientCentralMessagesArchive_H

//-----------------------------------------------------------------------

#include "Archive/ByteStream.h"

struct EnumerateCharacterId_Chardata;

namespace Archive
{
	void get(ReadIterator& source, EnumerateCharacterId_Chardata& c);
	void put(ByteStream& target, EnumerateCharacterId_Chardata const& c);
}

#endif	// _INCLUDED_ClientCentralMessagesArchive_H
