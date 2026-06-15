// ======================================================================
//
// SseMath.cpp
// Copyright 2002 Sony Online Entertainment, Inc.
// All Rights Reserved.
//
// ======================================================================

#include "sharedMath/FirstSharedMath.h"
#include "sharedMath/SseMath.h"

#include "sharedMath/Transform.h"
#include "sharedMath/Vector.h"

#include <intrin.h>     // __cpuid
#include <xmmintrin.h>  // _mm_* SSE intrinsics

#ifdef _DEBUG
#include <cmath>        // fabsf for the numeric-equivalence oracle
#endif

// ======================================================================
//
// BITS-01 / D-05: this module was 13 inline-asm SSE blocks across 5 functions
// (a CPUID feature check + 4 matrix-transform routines). x64 forbids inline
// asm (always error C4235), so it is rewritten as register-faithful _mm_*
// intrinsics. Intrinsics compile on BOTH x86 and x64 -- there is NO #ifdef
// fork (D-05); the committed 32-bit build inherits the same code.
//
// ALIGNMENT (review #2): the `Transform` argument is an embedded plain member
// with no alignas(16); a movaps / _mm_load_ps on it would FAULT at runtime on
// x64 (a fault the compile-only harness is structurally blind to). All loads
// from the matrix/vector pointer arguments therefore use _mm_loadu_ps
// (UNALIGNED). The old global non-reentrant staging array (sse-scratch[5][4])
// is RETIRED in favor of stack locals (review #2).
//
// USAGE-AUDIT (D-05 / Pitfall 4): all of these are LIVE -- canDoSseMath drives
// Transform::install's selector (Transform.cpp:76-78) and s_useSSE in the
// skinning hot path (SoftwareBlendSkeletalShaderPrimitive.cpp:365/1569/1592).
// Nothing is dead; nothing is deleted.
//
// ======================================================================

namespace
{
	// Horizontal sum of all 4 lanes of a __m128 (lane0+lane1+lane2+lane3).
	inline float hsum4(__m128 v)
	{
		// {x,y,z,w} + {y,x,w,z} = {x+y, y+x, z+w, w+z}
		__m128 shuf = _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 3, 0, 1));
		__m128 sums = _mm_add_ps(v, shuf);
		// + {z+w, ., ., x+y} -> lane0 = x+y+z+w
		shuf = _mm_movehl_ps(shuf, sums);
		sums = _mm_add_ss(sums, shuf);
		return _mm_cvtss_f32(sums);
	}

	// Horizontal sum of only the low 3 lanes (lane0+lane1+lane2); lane3 ignored.
	inline float hsum3(__m128 v)
	{
		float tmp[4];
		_mm_storeu_ps(tmp, v);
		return tmp[0] + tmp[1] + tmp[2];
	}

	// Load the 3 rows of a 3x4 row-major Transform from an UNALIGNED pointer
	// (review #2 -- never movaps an embedded Transform).
	inline void loadMatrixRows(const Transform &transform, __m128 &row0, __m128 &row1, __m128 &row2)
	{
		const float *m = reinterpret_cast<const float *>(&transform);
		row0 = _mm_loadu_ps(m + 0);   // a1 a2 a3 a4
		row1 = _mm_loadu_ps(m + 4);   // b1 b2 b3 b4
		row2 = _mm_loadu_ps(m + 8);   // c1 c2 c3 c4
	}
}

// ======================================================================
/**
 * Retrieve whether the hardware can do SSE math.
 *
 * @return  true if SSE math processing is available; false otherwise.
 */

bool SseMath::canDoSseMath()
{
	//-- Query CPUID leaf 1. EDX bit 25 = SSE, bit 24 = FXSR (FXSAVE/FXRSTOR).
	//   On x64 SSE2 is architecturally guaranteed so this is effectively always
	//   true there -- that is fine; the function stays (it drives the install
	//   selector + s_useSSE branch and must not be deleted -- Pitfall 4).
	int cpuInfo[4] = { 0, 0, 0, 0 };
	__cpuid(cpuInfo, 1);

	const unsigned int featureBits = static_cast<unsigned int>(cpuInfo[3]);   // EDX

	const bool cpuHasSse         = ((featureBits & 0x02000000u) != 0);   // bit 25
	const bool cpuHasSaveRestore = ((featureBits & 0x01000000u) != 0);   // bit 24

	return cpuHasSse && cpuHasSaveRestore;
}

// ----------------------------------------------------------------------

Vector SseMath::rotateTranslateScale_l2p(const Transform &transform, const Vector &vector, float scale)
{
	__m128 row0, row1, row2;
	loadMatrixRows(transform, row0, row1, row2);

	// Source w = 1.0 -> the translate column (a4/b4/c4) participates.
	const __m128 src = _mm_set_ps(1.0f, vector.z, vector.y, vector.x);   // {x,y,z,1}
	const __m128 s   = _mm_set1_ps(scale);

	// Per row: (src * row) * scale, then sum ALL 4 lanes (translate scaled too).
	const __m128 r0 = _mm_mul_ps(_mm_mul_ps(src, row0), s);
	const __m128 r1 = _mm_mul_ps(_mm_mul_ps(src, row1), s);
	const __m128 r2 = _mm_mul_ps(_mm_mul_ps(src, row2), s);

	return Vector(hsum4(r0), hsum4(r1), hsum4(r2));
}

// ----------------------------------------------------------------------

Vector SseMath::rotateScale_l2p(const Transform &transform, const Vector &vector, float scale)
{
	__m128 row0, row1, row2;
	loadMatrixRows(transform, row0, row1, row2);

	// Source w = 0.0 -> NO translate. Sum only 3 lanes (faithful to the asm,
	// which set the source w-lane to 0 and summed lanes [0..2]).
	const __m128 src = _mm_set_ps(0.0f, vector.z, vector.y, vector.x);   // {x,y,z,0}
	const __m128 s   = _mm_set1_ps(scale);

	const __m128 r0 = _mm_mul_ps(_mm_mul_ps(src, row0), s);
	const __m128 r1 = _mm_mul_ps(_mm_mul_ps(src, row1), s);
	const __m128 r2 = _mm_mul_ps(_mm_mul_ps(src, row2), s);

	return Vector(hsum3(r0), hsum3(r1), hsum3(r2));
}

// ----------------------------------------------------------------------

void SseMath::skinPositionNormal_l2p(const Transform &transform, const Vector &sourcePosition, const Vector &sourceNormal, float scale, Vector &destPosition, Vector &destNormal)
{
	__m128 row0, row1, row2;
	loadMatrixRows(transform, row0, row1, row2);

	const __m128 s = _mm_set1_ps(scale);

	//-- POSITION: w = 1.0, sum all 4 lanes (translate column included).
	{
		const __m128 src = _mm_set_ps(1.0f, sourcePosition.z, sourcePosition.y, sourcePosition.x);
		const __m128 p0 = _mm_mul_ps(_mm_mul_ps(src, row0), s);
		const __m128 p1 = _mm_mul_ps(_mm_mul_ps(src, row1), s);
		const __m128 p2 = _mm_mul_ps(_mm_mul_ps(src, row2), s);
		destPosition.x = hsum4(p0);
		destPosition.y = hsum4(p1);
		destPosition.z = hsum4(p2);
	}

	//-- NORMAL: the asm loads w = 1.0 BUT sums only 3 lanes -> the translate
	//   column is dropped. Reproduce that asymmetry exactly (review #2).
	{
		const __m128 src = _mm_set_ps(1.0f, sourceNormal.z, sourceNormal.y, sourceNormal.x);
		const __m128 n0 = _mm_mul_ps(_mm_mul_ps(src, row0), s);
		const __m128 n1 = _mm_mul_ps(_mm_mul_ps(src, row1), s);
		const __m128 n2 = _mm_mul_ps(_mm_mul_ps(src, row2), s);
		destNormal.x = hsum3(n0);
		destNormal.y = hsum3(n1);
		destNormal.z = hsum3(n2);
	}
}

// ----------------------------------------------------------------------

void SseMath::skinPositionNormalAdd_l2p(const Transform &transform, const Vector &sourcePosition, const Vector &sourceNormal, float scale, Vector &destPosition, Vector &destNormal)
{
	__m128 row0, row1, row2;
	loadMatrixRows(transform, row0, row1, row2);

	const __m128 s = _mm_set1_ps(scale);

	//-- POSITION: w = 1.0, sum all 4 lanes, ACCUMULATE into out.
	{
		const __m128 src = _mm_set_ps(1.0f, sourcePosition.z, sourcePosition.y, sourcePosition.x);
		const __m128 p0 = _mm_mul_ps(_mm_mul_ps(src, row0), s);
		const __m128 p1 = _mm_mul_ps(_mm_mul_ps(src, row1), s);
		const __m128 p2 = _mm_mul_ps(_mm_mul_ps(src, row2), s);
		destPosition.x += hsum4(p0);
		destPosition.y += hsum4(p1);
		destPosition.z += hsum4(p2);
	}

	//-- NORMAL: w = 1.0 loaded, sum only 3 lanes (translate dropped), ACCUMULATE.
	{
		const __m128 src = _mm_set_ps(1.0f, sourceNormal.z, sourceNormal.y, sourceNormal.x);
		const __m128 n0 = _mm_mul_ps(_mm_mul_ps(src, row0), s);
		const __m128 n1 = _mm_mul_ps(_mm_mul_ps(src, row1), s);
		const __m128 n2 = _mm_mul_ps(_mm_mul_ps(src, row2), s);
		destNormal.x += hsum3(n0);
		destNormal.y += hsum3(n1);
		destNormal.z += hsum3(n2);
	}
}

// ======================================================================
//
// _DEBUG numeric-equivalence oracle (review #2 / D-05).
//
// Boot smoke + the compile-only harness BOTH miss a transposed lane / wrong
// w-lane / wrong lane-count. This static-init self-test runs each of the 4
// *_l2p routines on a fixed input vector + matrix and compares the intrinsic
// result against the SCALAR reference (Transform::rotateTranslate_l2p /
// rotate_l2p) within an epsilon, DEBUG_FATAL on mismatch -- so a lane error is
// caught deterministically at startup. It deliberately exercises the verified
// asymmetries: rotateTranslateScale 4-lane vs rotateScale 3-lane, and the skin
// position-4-lane vs normal-3-lane split.
//
// ======================================================================

#ifdef _DEBUG
namespace
{
	inline bool nearlyEqual(float a, float b) { return fabsf(a - b) < 1e-4f; }

	struct SseMathSelfTest
	{
		SseMathSelfTest()
		{
			// Fixed, non-degenerate affine transform (rotation-ish + translation).
			// `matrix` is private; write the 12 row-major floats through the same
			// reinterpret-cast view loadMatrixRows() reads (Transform is a single
			// matrix_t member -> standard layout).
			Transform xf(Transform::IF_none);
			float *m = reinterpret_cast<float *>(&xf);
			m[0]  = 0.36f;  m[1]  = -0.48f; m[2]  = 0.80f; m[3]  =  1.50f;
			m[4]  = 0.80f;  m[5]  =  0.60f; m[6]  = 0.00f; m[7]  = -2.25f;
			m[8]  = -0.48f; m[9]  =  0.64f; m[10] = 0.60f; m[11] =  0.75f;

			const Vector v(1.30f, -2.10f, 0.70f);
			const Vector n(0.50f, 0.50f, -0.70710678f);
			const float  scale = 1.75f;

			//-- rotateTranslateScale_l2p: w=1, 4 lanes (translate included, scaled).
			{
				const Vector got = rotateTranslateScale_l2p(xf, v, scale);
				const Vector ref = xf.rotateTranslate_l2p(v) * scale;
				DEBUG_FATAL(!nearlyEqual(got.x, ref.x) || !nearlyEqual(got.y, ref.y) || !nearlyEqual(got.z, ref.z),
					("SseMath oracle: rotateTranslateScale_l2p mismatch (got %g,%g,%g ref %g,%g,%g)",
						got.x, got.y, got.z, ref.x, ref.y, ref.z));
			}

			//-- rotateScale_l2p: w=0, 3 lanes (NO translate).
			{
				const Vector got = rotateScale_l2p(xf, v, scale);
				const Vector ref = xf.rotate_l2p(v) * scale;
				DEBUG_FATAL(!nearlyEqual(got.x, ref.x) || !nearlyEqual(got.y, ref.y) || !nearlyEqual(got.z, ref.z),
					("SseMath oracle: rotateScale_l2p mismatch (got %g,%g,%g ref %g,%g,%g)",
						got.x, got.y, got.z, ref.x, ref.y, ref.z));
			}

			//-- skinPositionNormal_l2p: position w=1 4-lane, normal w=1 3-lane.
			{
				Vector gotP, gotN;
				skinPositionNormal_l2p(xf, v, n, scale, gotP, gotN);
				const Vector refP = xf.rotateTranslate_l2p(v) * scale;
				const Vector refN = xf.rotate_l2p(n) * scale;
				DEBUG_FATAL(!nearlyEqual(gotP.x, refP.x) || !nearlyEqual(gotP.y, refP.y) || !nearlyEqual(gotP.z, refP.z),
					("SseMath oracle: skinPositionNormal_l2p POSITION mismatch"));
				DEBUG_FATAL(!nearlyEqual(gotN.x, refN.x) || !nearlyEqual(gotN.y, refN.y) || !nearlyEqual(gotN.z, refN.z),
					("SseMath oracle: skinPositionNormal_l2p NORMAL mismatch"));
			}

			//-- skinPositionNormalAdd_l2p: same, accumulating into a known base.
			{
				Vector gotP(10.0f, 20.0f, 30.0f);
				Vector gotN(1.0f, 2.0f, 3.0f);
				skinPositionNormalAdd_l2p(xf, v, n, scale, gotP, gotN);
				const Vector refP = Vector(10.0f, 20.0f, 30.0f) + xf.rotateTranslate_l2p(v) * scale;
				const Vector refN = Vector(1.0f, 2.0f, 3.0f) + xf.rotate_l2p(n) * scale;
				DEBUG_FATAL(!nearlyEqual(gotP.x, refP.x) || !nearlyEqual(gotP.y, refP.y) || !nearlyEqual(gotP.z, refP.z),
					("SseMath oracle: skinPositionNormalAdd_l2p POSITION mismatch"));
				DEBUG_FATAL(!nearlyEqual(gotN.x, refN.x) || !nearlyEqual(gotN.y, refN.y) || !nearlyEqual(gotN.z, refN.z),
					("SseMath oracle: skinPositionNormalAdd_l2p NORMAL mismatch"));
			}
		}
	};

	SseMathSelfTest s_sseMathSelfTest;
}
#endif

// ======================================================================
