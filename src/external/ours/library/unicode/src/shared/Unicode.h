// ======================================================================
// Unicode.h
// copyright (c) 2001 Sony Online Entertainment
//
// jwatson
//
// Basic Unicode primitives and string handling/manipulating functions
// ======================================================================

#ifndef INCLUDED_Unicode_H
#define INCLUDED_Unicode_H

#if WIN32
#pragma warning (disable:4710)
#pragma warning (disable:4786)
// stl warning func not inlined
#pragma warning (disable:4514)
#endif

#include <string>

//-----------------------------------------------------------------

namespace Unicode
{
	using unicode_char_t = wchar_t; // UTF-16 on Windows, 2-bytes
	using String = std::wstring;

	/**
	* NarrowString is a ascii string.
	*/

	typedef std::string                        NarrowString;

	// Unicode whitespace/endline tables must also use wchar_t
	inline constexpr unicode_char_t whitespace[] = {
		L' ', L'\n', L'\r', L'\t', 0x3000, 0
	};

	inline constexpr unicode_char_t endlines[] = { L'\r', L'\n', 0 };

	inline constexpr char ascii_whitespace[] = { ' ', '\n', '\r', '\t', 0 };
	inline constexpr char ascii_endlines[] = { '\r', '\n', 0 };

	inline const String emptyString;
}

//-----------------------------------------------------------------

#endif

