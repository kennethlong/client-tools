// ======================================================================
//
// Direct3d11_HlslRewrite.cpp
// Phase 11 D3D11 renderer plugin -- shared HLSL textual rewrite utility.
//
// Plan 11-07 Iteration 7 -- see Direct3d11_HlslRewrite.h preamble for the
// full design rationale (two-call-site lifetime semantics, the three
// rewrite rules A/B/C, semantic-loss caveat, cache-invalidation strategy).
//
// Implementation summary:
//   Single-pass scanner walks the source byte-by-byte. At each position
//   it tries (in order):
//     1. Rule A: SM4+ reserved-keyword whole-word match (Iter-4 origin).
//        On match -- emit the keyword followed by the `_id` suffix; output
//        grows by 3 bytes per match. Left + right boundary checks ensure
//        `endpoint` / `pointSize` / `linear` / etc. don't trigger.
//     2. Rule B: `:\s*[bcstu]\d+\b` -- legacy struct-member register-
//        binding shortcut (Iter-7 origin; Iter-8 expanded register-type
//        set from `c` only to all 5 D3D11 HLSL register types b/c/s/t/u).
//        On match -- emit the same number of spaces; output size unchanged
//        from this rule.
//     3. Rule C: `:\s*register\s*\(\s*[bcstu]\d+\s*\)` -- explicit
//        register() form (Iter-7 origin; Iter-8 expanded the same way).
//        On match -- emit the same number of spaces; output size unchanged
//        from this rule.
//   No match -- emit the single byte unchanged + advance one position.
//
// Order matters: Rule C is tried BEFORE Rule B because Rule C is a
// superset / more-specific match starting at the same `:` position.
// (Both rules start at `:`; Rule C's pattern is longer when it matches,
// so we try the longer one first to avoid Rule B greedily eating the
// `:` of a Rule-C-eligible site.)
//
// Two-pass strategy:
//   Pass 1 (computeRewrittenLength) -- walks the buffer once and returns
//     the total output size in bytes. Required because the include-handler
//     entry point needs to size the destination heap allocation BEFORE
//     emitting any bytes; the main-source entry point uses vector::resize
//     against the same size for symmetry + same emit code path.
//   Pass 2 (emitRewritten) -- walks the same buffer a second time and
//     emits the rewritten bytes into the caller's destination. Identical
//     control-flow shape to Pass 1; the difference is "+= 1 to output
//     size" vs. "write 1 byte to dst[outPos++]".
//   The two passes are guaranteed to agree on every match's location and
//   length because they read the same input and use identical match
//   predicates -- if they ever diverge, DEBUG_FATAL guards in emitRewritten
//   catch the corruption.
//
// Column preservation: Rules B/C replace match-length bytes with the same
// number of spaces. This preserves the source's column offsets so any
// subsequent D3DCompile error message that prints `(line,col)` still
// points at the same logical location in the original source. Line
// numbers are preserved automatically because the rewrite never adds or
// removes newlines.
//
// Memory: the include-handler entry point delete[]'s the original buffer
// when it allocates a new one; the main-source entry point only reads
// the input (caller owns it). Both entry points produce a `std::size_t`
// output-size value the caller passes to D3DCompile as the
// SrcDataSize / Bytes parameter.
//
// ======================================================================

#include "FirstDirect3d11.h"
#include "Direct3d11_HlslRewrite.h"

#include "sharedDebug/DebugFlags.h"

#include <cstddef>
#include <cstdio>   // THROWAWAY Iter-8 -- diagnostic dump (revert in Iter-9)
#include <cstring>

// ======================================================================

namespace
{
	// ------------------------------------------------------------------
	// Rule A: SM4+ reserved keywords renamed to `<keyword>_id`.
	// ------------------------------------------------------------------

	struct ReservedKeyword
	{
		char const *word;
		std::size_t length;
	};

	// SM4+ HLSL lexer reserves these identifiers as keywords for
	// geometry-shader primitive-type-binding syntax. SWG's D3D9-era
	// HLSL uses them as ordinary identifiers; the lexer rejects them
	// with X3000 "syntax error: unexpected token '<keyword>'" before
	// any profile-specific feature gating runs.
	constexpr ReservedKeyword kReservedKeywords[] = {
		{ "point",       5 },  // GS primitive type + sampler filter literal
		{ "line",        4 },  // GS primitive type
		{ "triangle",    8 },  // GS primitive type
		{ "lineadj",     7 },  // GS primitive type (line w/ adjacency)
		{ "triangleadj", 11 }, // GS primitive type (triangle w/ adjacency)
	};

	constexpr std::size_t kReservedKeywordCount =
		sizeof(kReservedKeywords) / sizeof(kReservedKeywords[0]);

	// Replacement suffix appended after each Rule-A match.
	constexpr char const  kReplacementSuffix[]     = "_id";
	constexpr std::size_t kReplacementSuffixLength = 3;

	// HLSL identifier character class (same as C): [A-Za-z0-9_]. A
	// whole-word match for Rule A requires the previous char (if any) to
	// be OUT of this class AND the following char (if any) to be OUT of
	// this class.
	inline bool isIdentChar(unsigned char c)
	{
		return (c >= 'a' && c <= 'z')
			|| (c >= 'A' && c <= 'Z')
			|| (c >= '0' && c <= '9')
			|| (c == '_');
	}

	inline bool isAsciiDigit(unsigned char c)
	{
		return (c >= '0' && c <= '9');
	}

	// Whitespace per HLSL's lexer (used to scan `\s*` between tokens in
	// Rules B/C). HLSL treats space, tab, and the line-end characters
	// equivalently between tokens; we follow the same set.
	inline bool isHlslSpace(unsigned char c)
	{
		return (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n');
	}

	// D3D11 HLSL register-type letter (used by Rules B/C). The full set
	// per MSDN / HLSL reference:
	//   `b#` -- constant buffer (cbuffer) register
	//   `t#` -- texture / SRV register
	//   `s#` -- sampler register
	//   `u#` -- UAV (unordered access view) register (D3D11 only)
	//   `c#` -- single constant register (D3D9 legacy + D3D11 cbuffer-
	//           member packoffset offset)
	//
	// Iter-7 originally hardcoded just `c` here because the X3202 error
	// site visible at the time (`2d.vsh(8,44-45)`) was speculatively
	// thought to be a `: c<N>` binding. Iter-7's smoke produced the
	// identical X3202 at the identical site, meaning the actual letter
	// at line 8 col 44 was NOT `c`. Iter-8 expands the set to all 5
	// canonical D3D11 register-type letters so a `: s0` (sampler) /
	// `: t0` (texture/SRV) / `: b0` (cbuffer) / `: u0` (UAV) binding
	// on a struct member also gets stripped. HLSL is case-sensitive;
	// these letters are lowercase by HLSL convention -- uppercase user
	// semantics like `: COLOR0` / `: SV_POSITION` are not affected.
	inline bool isHlslRegisterTypeLetter(unsigned char c)
	{
		return (c == 'b') || (c == 'c') || (c == 's') || (c == 't') || (c == 'u');
	}

	// ------------------------------------------------------------------
	// Match helpers for Rules A/B/C. Each returns 0 on no-match, else
	// the number of input bytes the match consumes (i.e., how far to
	// advance the scanner). On match, callers know how many output bytes
	// to produce based on the rule:
	//   Rule A -- emit `keyword + "_id"` (out = match length + 3).
	//   Rule B -- emit match-length spaces (out = match length).
	//   Rule C -- emit match-length spaces (out = match length).
	// ------------------------------------------------------------------

	// Rule A: returns (keyword index, match length) on match else (0,0).
	// Matched keyword index is stored in `outKwIndex` for emitRewritten
	// to know which keyword string to copy.
	std::size_t tryMatchRuleA(
		unsigned char const *src,
		std::size_t          length,
		std::size_t          pos,
		std::size_t         &outKwIndex)
	{
		// Left boundary: start-of-buffer OR preceding char is non-ident.
		bool const leftBoundaryOk =
			(pos == 0) || !isIdentChar(src[pos - 1]);
		if (!leftBoundaryOk)
			return 0;

		for (std::size_t k = 0; k < kReservedKeywordCount; ++k)
		{
			ReservedKeyword const &kw = kReservedKeywords[k];
			if (pos + kw.length > length)
				continue;
			if (std::memcmp(src + pos, kw.word, kw.length) != 0)
				continue;

			// Right boundary: end-of-buffer OR following char is
			// non-ident.
			std::size_t const tail = pos + kw.length;
			bool const rightBoundaryOk =
				(tail == length) || !isIdentChar(src[tail]);
			if (!rightBoundaryOk)
				continue;

			outKwIndex = k;
			return kw.length;
		}
		return 0;
	}

	// Rule C: `:\s*register\s*\(\s*[bcstu]\d+\s*\)`
	// Tried BEFORE Rule B (both start at `:`; Rule C is the more-specific
	// longer match). Returns total match length (from `:` through the
	// closing `)`) on match else 0.
	//
	// Implementation walks the input strictly forward, never backtracking,
	// because the pattern is purely sequential: `:`, then optional
	// whitespace, then the literal `register` token, etc.
	//
	// Iter-8: the single `c` register-type letter is now any of the 5
	// canonical D3D11 register-type letters `[bcstu]` -- see
	// isHlslRegisterTypeLetter() above for the rationale.
	std::size_t tryMatchRuleC(
		unsigned char const *src,
		std::size_t          length,
		std::size_t          pos)
	{
		if (pos >= length || src[pos] != ':')
			return 0;

		std::size_t i = pos + 1;

		// `\s*`
		while (i < length && isHlslSpace(src[i]))
			++i;

		// Literal `register` (8 chars, lowercase, whole-word).
		constexpr char const kRegister[] = "register";
		constexpr std::size_t kRegisterLen = 8;
		if (i + kRegisterLen > length)
			return 0;
		if (std::memcmp(src + i, kRegister, kRegisterLen) != 0)
			return 0;
		// Right boundary on the literal `register` keyword: next char
		// must be non-ident (else this is some identifier like
		// `register_42` we should NOT match).
		std::size_t const afterRegister = i + kRegisterLen;
		if (afterRegister < length && isIdentChar(src[afterRegister]))
			return 0;
		i = afterRegister;

		// `\s*`
		while (i < length && isHlslSpace(src[i]))
			++i;

		// Literal `(`
		if (i >= length || src[i] != '(')
			return 0;
		++i;

		// `\s*`
		while (i < length && isHlslSpace(src[i]))
			++i;

		// `[bcstu]` (single register-type letter, lowercase). Iter-8
		// expanded set from just `c`.
		if (i >= length || !isHlslRegisterTypeLetter(src[i]))
			return 0;
		++i;

		// `\d+` (one or more digits)
		std::size_t const digitStart = i;
		while (i < length && isAsciiDigit(src[i]))
			++i;
		if (i == digitStart)
			return 0;

		// `\s*`
		while (i < length && isHlslSpace(src[i]))
			++i;

		// Literal `)`
		if (i >= length || src[i] != ')')
			return 0;
		++i;

		return i - pos;
	}

	// Rule B: `:\s*[bcstu]\d+\b`
	// Tried AFTER Rule C at the same `:` site so the longer, more-specific
	// register() form wins when present. Returns total match length on
	// match else 0.
	//
	// Iter-8: the single `c` register-type letter is now any of the 5
	// canonical D3D11 register-type letters `[bcstu]` -- see
	// isHlslRegisterTypeLetter() above for the rationale.
	std::size_t tryMatchRuleB(
		unsigned char const *src,
		std::size_t          length,
		std::size_t          pos)
	{
		if (pos >= length || src[pos] != ':')
			return 0;

		std::size_t i = pos + 1;

		// `\s*`
		while (i < length && isHlslSpace(src[i]))
			++i;

		// `[bcstu]` (single register-type letter, lowercase). Iter-8
		// expanded set from just `c`.
		if (i >= length || !isHlslRegisterTypeLetter(src[i]))
			return 0;
		++i;

		// `\d+` -- one or more digits.
		std::size_t const digitStart = i;
		while (i < length && isAsciiDigit(src[i]))
			++i;
		if (i == digitStart)
			return 0;

		// `\b` -- right boundary: end-of-buffer OR next char is non-ident.
		// This stops `: c4foo` from matching (the `c4` is a substring of
		// the identifier `c4foo`, not a register binding) and stops
		// `: c12_thing` from matching for the same reason.
		if (i < length && isIdentChar(src[i]))
			return 0;

		return i - pos;
	}

	// ------------------------------------------------------------------
	// Two-pass core: compute output size, then emit bytes.
	// ------------------------------------------------------------------

	std::size_t computeRewrittenLength(
		unsigned char const *src,
		std::size_t          length,
		std::size_t         &outRewriteCount)
	{
		outRewriteCount = 0;
		if (!src || length == 0)
			return 0;

		std::size_t out = 0;
		std::size_t i   = 0;
		while (i < length)
		{
			// Rule A -- reserved-keyword rename (output = match + 3).
			std::size_t kwIndex = 0;
			std::size_t matchA = tryMatchRuleA(src, length, i, kwIndex);
			if (matchA != 0)
			{
				out += matchA + kReplacementSuffixLength;
				i   += matchA;
				++outRewriteCount;
				continue;
			}

			// Rule C -- explicit register() form (output = match, all
			// spaces). Tried before Rule B because both start at `:`
			// and Rule C is the more-specific longer match.
			std::size_t matchC = tryMatchRuleC(src, length, i);
			if (matchC != 0)
			{
				out += matchC;
				i   += matchC;
				++outRewriteCount;
				continue;
			}

			// Rule B -- shortcut `:\s*c\d+\b` (output = match, all spaces).
			std::size_t matchB = tryMatchRuleB(src, length, i);
			if (matchB != 0)
			{
				out += matchB;
				i   += matchB;
				++outRewriteCount;
				continue;
			}

			// No rule matched -- copy single byte through.
			out += 1;
			i   += 1;
		}

		return out;
	}

	void emitRewritten(
		unsigned char const *src,
		std::size_t          length,
		unsigned char       *dst,
		std::size_t          dstCapacity)
	{
		std::size_t outPos = 0;
		std::size_t i      = 0;
		while (i < length)
		{
			// Rule A -- emit keyword + "_id" suffix.
			std::size_t kwIndex = 0;
			std::size_t matchA = tryMatchRuleA(src, length, i, kwIndex);
			if (matchA != 0)
			{
				ReservedKeyword const &kw = kReservedKeywords[kwIndex];
				DEBUG_FATAL(outPos + kw.length > dstCapacity,
					("Direct3d11_HlslRewrite: emit overflow Rule A keyword"
					 " '%s' at src[%zu], outPos=%zu/%zu",
					 kw.word, i, outPos, dstCapacity));
				std::memcpy(dst + outPos, kw.word, kw.length);
				outPos += kw.length;

				DEBUG_FATAL(outPos + kReplacementSuffixLength > dstCapacity,
					("Direct3d11_HlslRewrite: emit overflow Rule A suffix"
					 " '%s' at src[%zu], outPos=%zu/%zu",
					 kw.word, i, outPos, dstCapacity));
				std::memcpy(dst + outPos, kReplacementSuffix, kReplacementSuffixLength);
				outPos += kReplacementSuffixLength;

				i += matchA;
				continue;
			}

			// Rule C -- emit match-length spaces.
			std::size_t matchC = tryMatchRuleC(src, length, i);
			if (matchC != 0)
			{
				DEBUG_FATAL(outPos + matchC > dstCapacity,
					("Direct3d11_HlslRewrite: emit overflow Rule C at"
					 " src[%zu] (len=%zu), outPos=%zu/%zu",
					 i, matchC, outPos, dstCapacity));
				std::memset(dst + outPos, ' ', matchC);
				outPos += matchC;
				i      += matchC;
				continue;
			}

			// Rule B -- emit match-length spaces.
			std::size_t matchB = tryMatchRuleB(src, length, i);
			if (matchB != 0)
			{
				DEBUG_FATAL(outPos + matchB > dstCapacity,
					("Direct3d11_HlslRewrite: emit overflow Rule B at"
					 " src[%zu] (len=%zu), outPos=%zu/%zu",
					 i, matchB, outPos, dstCapacity));
				std::memset(dst + outPos, ' ', matchB);
				outPos += matchB;
				i      += matchB;
				continue;
			}

			// No rule matched -- pass byte through.
			DEBUG_FATAL(outPos + 1 > dstCapacity,
				("Direct3d11_HlslRewrite: emit overflow passthrough byte"
				 " src[%zu]=0x%02x, outPos=%zu/%zu",
				 i, src[i], outPos, dstCapacity));
			dst[outPos] = src[i];
			++outPos;
			++i;
		}

		DEBUG_FATAL(outPos != dstCapacity,
			("Direct3d11_HlslRewrite: emit length mismatch outPos=%zu"
			 " expected=%zu (Pass1/Pass2 divergence)",
			 outPos, dstCapacity));
	}
}

// ======================================================================

unsigned char *Direct3d11_HlslRewrite::applyToIncludeBuffer(
	unsigned char *inBuffer,
	std::size_t    inLength,
	std::size_t   &outLength,
	char const    *diagName)
{
	if (!inBuffer || inLength == 0)
	{
		outLength = inLength;
		return inBuffer;
	}

	std::size_t rewriteCount = 0;
	std::size_t const rewrittenLen = computeRewrittenLength(
		inBuffer, inLength, rewriteCount);

	if (rewriteCount == 0)
	{
		// Fast path -- no occurrences of any rule. Pass the buffer
		// through; no allocation.
		DEBUG_FATAL(rewrittenLen != inLength,
			("Direct3d11_HlslRewrite: include-handler fast-path size"
			 " mismatch rewrittenLen=%zu inLength=%zu",
			 rewrittenLen, inLength));
		outLength = inLength;
		return inBuffer;
	}

	// Allocate a fresh buffer + emit.
	unsigned char *outBuffer = new unsigned char[rewrittenLen];
	emitRewritten(inBuffer, inLength, outBuffer, rewrittenLen);

	// Discard the original (we own it via the include-handler contract).
	delete[] inBuffer;

	DEBUG_REPORT_LOG_PRINT(true,
		("Direct3d11_HlslRewrite: include '%s' rewritten"
		 " (%zu rewrite%s; input=%zu bytes -> output=%zu bytes; delta=%+zd)\n",
		 diagName ? diagName : "<unnamed>",
		 rewriteCount, (rewriteCount == 1) ? "" : "s",
		 inLength, rewrittenLen,
		 static_cast<ptrdiff_t>(rewrittenLen) - static_cast<ptrdiff_t>(inLength)));

	outLength = rewrittenLen;
	return outBuffer;
}

// ----------------------------------------------------------------------

void Direct3d11_HlslRewrite::applyToMainSource(
	char const        *src,
	std::size_t        length,
	std::vector<char> &outVec,
	char const        *diagName)
{
	outVec.clear();

	if (!src || length == 0)
		return;

	// ------------------------------------------------------------------
	// THROWAWAY Iter-8 diagnostic dump -- REVERT IN ITER-9.
	//
	// One-shot: on the very first call to applyToMainSource, write the
	// raw INPUT source bytes (pre-rewrite) to a fixed path under
	// `stage/`. Purpose: Iter-4..Iter-7 have repeatedly FATAL'd at
	// `2d.vsh(8,44-45) error X3202: location semantics cannot be
	// specified on members`, but the actual line-8 contents of `2d.vsh`
	// are not visible to the dev team because the source ships inside
	// a TRE archive (the on-disk `stage/vertex_program/` doesn't even
	// exist -- the path is a synthetic D3DCompile construct from the
	// displayName argument).
	//
	// This dump captures whatever source the engine handed us so the
	// next iteration can read the actual file content (in particular
	// line 8 column 44-45) and design the correct rewrite rule against
	// concrete data instead of speculation.
	//
	// The first call is whatever shader loads first. Today that is the
	// VS for `shader/2d_vertexcolor.sht` -> `vertex_program/2d.vsh` per
	// the Iter-7 crash dump tail (`ShaderTemplate_Iff:` line). If a
	// different first shader surfaces later, dump still captures
	// whatever it is -- the file content alone is enough to decode.
	//
	// One-shot is intentional: dumping every shader spams the filesystem
	// and obscures the data point we actually need. A static bool gate
	// is the simplest possible implementation.
	//
	// File path: `stage/shader-debug-first-source.txt` (gitignored under
	// the existing `stage/` rule).
	//
	// Mirrors the Plan 11-01 THROWAWAY D-04 pattern: diagnostic
	// instrumentation that lands as part of an iteration to unblock the
	// next iteration's design, and gets reverted as the first commit of
	// that next iteration. Iter-9 should remove this entire block plus
	// the `#include <cstdio>` line and revert all related preamble
	// comments.
	{
		static bool dumped = false;
		if (!dumped)
		{
			dumped = true;
			FILE *fp = std::fopen(
				"D:/Code/swg-client-v2/stage/shader-debug-first-source.txt",
				"wb");
			if (fp)
			{
				std::fwrite(src, 1, length, fp);
				std::fclose(fp);
				DEBUG_REPORT_LOG_PRINT(true,
					("Direct3d11_HlslRewrite (THROWAWAY Iter-8): dumped"
					 " first applyToMainSource input '%s' (%zu bytes)"
					 " to stage/shader-debug-first-source.txt\n",
					 diagName ? diagName : "<unnamed>", length));
			}
		}
	}
	// END THROWAWAY Iter-8 diagnostic dump.
	// ------------------------------------------------------------------

	unsigned char const *srcU =
		reinterpret_cast<unsigned char const *>(src);

	std::size_t rewriteCount = 0;
	std::size_t const rewrittenLen = computeRewrittenLength(
		srcU, length, rewriteCount);

	outVec.resize(rewrittenLen);
	if (rewrittenLen == 0)
		return;

	emitRewritten(
		srcU,
		length,
		reinterpret_cast<unsigned char *>(outVec.data()),
		rewrittenLen);

	if (rewriteCount != 0)
	{
		DEBUG_REPORT_LOG_PRINT(true,
			("Direct3d11_HlslRewrite: main-source '%s' rewritten"
			 " (%zu rewrite%s; input=%zu bytes -> output=%zu bytes; delta=%+zd)\n",
			 diagName ? diagName : "<unnamed>",
			 rewriteCount, (rewriteCount == 1) ? "" : "s",
			 length, rewrittenLen,
			 static_cast<ptrdiff_t>(rewrittenLen) - static_cast<ptrdiff_t>(length)));
	}
}

// ======================================================================
