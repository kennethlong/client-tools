// ======================================================================
//
// SseMath.h
// Copyright 2002 Sony Online Entertainment, Inc.
// All Rights Reserved.
//
// ======================================================================

#ifndef INCLUDED_SseMath_H
#define INCLUDED_SseMath_H

// ======================================================================

#include <xmmintrin.h>   // _mm_prefetch (SseMath::prefetch)

// ======================================================================

class Transform;
class Vector;

// ======================================================================
#define MXCSR_FLUSH_TO_ZERO        (1<<15)
#define MXCSR_PRECISION_MASK       (1<<12)
#define MXCSR_UNDERFLOW_MASK       (1<<11)
#define MXCSR_OVERFLOW_MASK        (1<<10)
#define MXCSR_DIVIDE_BY_ZERO_MASK  (1<< 9)
#define MXCSR_DENORMAL_MASK        (1<< 8)

class SseMath
{
public:

	static bool    canDoSseMath();

	static Vector  rotateTranslateScale_l2p(const Transform &transform, const Vector &vector, float scale);
	static Vector  rotateScale_l2p(const Transform &transform, const Vector &vector, float scale);

	static void    skinPositionNormal_l2p(const Transform &transform, const Vector &position, const Vector &normal, float scale, Vector &destPosition, Vector &destNormal);
	static void    skinPositionNormalAdd_l2p(const Transform &transform, const Vector &position, const Vector &normal, float scale, Vector &destPosition, Vector &destNormal);
	static void prefetch(void const * const sourceData, size_t const objectSize);
};

// ----------------------------------------------------------------------

inline void SseMath::prefetch(void const * const sourceData, size_t const objectSize)
{
	// BITS-01 / D-05: the old MSVC branch was an inline-asm prefetchnta block
	// (x64-illegal -> C4235). Ported to the _mm_prefetch intrinsic,
	// which compiles on BOTH x86 and x64 (no #ifdef fork). _MM_HINT_NTA matches
	// the original prefetchnta (non-temporal) hint. The asm prefetched the byte at
	// (sourceData + objectSize); reproduce that address faithfully.
	_mm_prefetch(reinterpret_cast<char const *>(sourceData) + objectSize, _MM_HINT_NTA);
}

// ======================================================================

#endif
