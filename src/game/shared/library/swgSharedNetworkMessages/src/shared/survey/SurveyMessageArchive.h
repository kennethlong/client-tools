#ifndef INCLUDED_SurveyMessageArchive_H
#define INCLUDED_SurveyMessageArchive_H

#include "sharedMathArchive/VectorArchive.h

namespace Archive
{
	class ByteStream;
	class ReadIterator;

	void put(ByteStream& target, const Survey_DataItem& data);
	void get(ReadIterator& source, Survey_DataItem& data);
}

#endif