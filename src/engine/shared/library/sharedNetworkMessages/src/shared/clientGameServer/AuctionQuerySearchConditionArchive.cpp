// ======================================================================
//
// AuctionQuerySearchConditionArchive.cpp
//
// Copyright 2002 Sony Online Entertainment
//
// ======================================================================

#include "unicodeArchive/UnicodeArchive.h"
#include "sharedFoundation/NetworkIdArchive.h"
#include "sharedNetworkMessages/FirstSharedNetworkMessages.h"
#include "AuctionQueryTypes.h"
#include "sharedNetworkMessages/AuctionQuerySearchConditionArchive.h"

// ======================================================================

namespace Archive
{
	void get(ReadIterator & source, SearchCondition & target)
	{
		get(source, target.attributeNameCrc);
		get(source, target.requiredAttribute);

		int8 temp;
		get(source, temp);
		target.comparison = static_cast<SearchConditionComparison>(temp);

		if (target.comparison == SCC_int)
		{
			get(source, target.intMin);
			get(source, target.intMax);
		}
		else if (target.comparison == SCC_float)
		{
			get(source, target.floatMin);
			get(source, target.floatMax);
		}
		else if ((target.comparison == SCC_string_equal) ||
		         (target.comparison == SCC_string_not_equal) ||
		         (target.comparison == SCC_string_contain) ||
		         (target.comparison == SCC_string_not_contain))
		{
			get(source, target.stringValue);
		}
		else // error
		{
			target.comparison = SCC_int;
		}
	}

	void put(ByteStream & target, const SearchCondition & source)
	{
		put(target, source.attributeNameCrc);
		put(target, source.requiredAttribute);
		put(target, static_cast<int8>(source.comparison));

		if (source.comparison == SCC_int)
		{
			put(target, source.intMin);
			put(target, source.intMax);
		}
		else if (source.comparison == SCC_float)
		{
			put(target, source.floatMin);
			put(target, source.floatMax);
		}
		else if ((source.comparison == SCC_string_equal) ||
		         (source.comparison == SCC_string_not_equal) ||
		         (source.comparison == SCC_string_contain) ||
		         (source.comparison == SCC_string_not_contain))
		{
			put(target, source.stringValue);
		}
	}
}
