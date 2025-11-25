// ======================================================================
//
// AuctionQuerySearchConditionArchive.h
//
// Copyright 2002 Sony Online Entertainment
//
// ======================================================================

#ifndef	_AuctionQuerySearchConditionArchive_H
#define	_AuctionQuerySearchConditionArchive_H

//-----------------------------------------------------------------------

class SearchCondition;

namespace Archive
{
	class ReadIterator;
	class ByteStream;

	// forward declare before AutoList
	void get(ReadIterator& source, SearchCondition& target);
	void put(ByteStream& target, const SearchCondition& source);
}

// ======================================================================

#endif // _AuctionQueryHeadersMessage_H
