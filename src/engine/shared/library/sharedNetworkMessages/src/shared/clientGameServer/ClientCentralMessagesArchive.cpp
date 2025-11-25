// ClientCentralMessagesArchive.h
// copyright 2001 Verant Interactive
// Author: Justin Randall

//-----------------------------------------------------------------------

#include "ClientCentralMessagesTypes.h"

#include "sharedNetworkMessages/FirstSharedNetworkMessages.h"

namespace Archive
{
	inline void get(ReadIterator& source, EnumerateCharacterId_Chardata& c)
	{
		get(source, c.m_name);
		get(source, c.m_objectTemplateId);
		get(source, c.m_networkId);
		get(source, c.m_clusterId);
		get(source, c.m_characterType);
	}

	inline void put(ByteStream& target, EnumerateCharacterId_Chardata const& c)
	{
		put(target, c.m_name);
		put(target, c.m_objectTemplateId);
		put(target, c.m_networkId);
		put(target, c.m_clusterId);
		put(target, c.m_characterType);
	}
}