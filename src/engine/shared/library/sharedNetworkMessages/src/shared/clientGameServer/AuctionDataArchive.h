// ======================================================================
//
// AuctionDataArchive.h
//
// Copyright 2002 Sony Online Entertainment
//
// ======================================================================

#ifndef	_AuctionDataArchive_H
#define	_AuctionDataArchive_H

namespace Auction
{
	struct PalettizedItemDataHeader;
}

namespace Archive
{
	class ReadIterator;
	class ByteStream;

	void get(ReadIterator& source, Auction::PalettizedItemDataHeader& target);
	void put(ByteStream& target, Auction::PalettizedItemDataHeader const& source);
}

#endif // _AuctionDataArchive_H

