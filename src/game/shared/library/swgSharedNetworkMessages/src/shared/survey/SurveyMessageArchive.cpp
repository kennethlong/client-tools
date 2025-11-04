#include "SurveyMessageArchive.h"

#include "Archive/ByteStream.h"

// ======================================================================

namespace Archive
{
	void put(ByteStream& target, const Survey_DataItem& data)
	{
		put(target, data.m_location);
		put(target, data.m_efficiency);
	}

	void get(ReadIterator& source, Survey_DataItem& data)
	{
		get(source, data.m_location);
		get(source, data.m_efficiency);
	}
}

// ======================================================================