// ======================================================================
//
// ByteOrder.cpp
// copyright (c) 2001 Sony Online Entertainment
//
// ======================================================================

#include "sharedFoundation/FirstSharedFoundation.h"
#include "sharedFoundation/ByteOrder.h"

#include <intrin.h>

// ======================================================================

// Phase 31 (BITS-01, B-GAP-1): the original implementations were
// __declspec(naked) x86 inline-asm `bswap` trampolines, which are illegal
// targeting x64 (C4235). Ported to the _byteswap_* compiler intrinsics, which
// compile on BOTH x86 and x64 (no #ifdef fork) and emit the same `bswap` /
// `rol r,8` the asm did. The 16-bit ntohs/htons originally did a 32-bit bswap
// + shr 16, which is exactly a 16-bit byte swap of the low word -- _byteswap_ushort.

ulong ntohl(ulong netLong)
{
	return _byteswap_ulong(netLong);
}

ulong htonl(ulong hostLong)
{
	return _byteswap_ulong(hostLong);
}

ushort ntohs(ushort netShort)
{
	return _byteswap_ushort(netShort);
}

ushort htons(ushort hostShort)
{
	return _byteswap_ushort(hostShort);
}

// ======================================================================
