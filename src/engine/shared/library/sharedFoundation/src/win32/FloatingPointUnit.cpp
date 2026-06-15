// ======================================================================
//
// FloatingPointUnit.cpp
// copyright 1999 Bootprint Entertainment
// copyright 2001 Sony Online Entertainment
//
// ======================================================================

#include "sharedFoundation/FirstSharedFoundation.h"
#include "sharedFoundation/FloatingPointUnit.h"

#include "sharedFoundation/ConfigSharedFoundation.h"

#include <float.h>

// ======================================================================

int                          FloatingPointUnit::updateNumber;
ushort                       FloatingPointUnit::status;
FloatingPointUnit::Precision FloatingPointUnit::precision;
FloatingPointUnit::Rounding  FloatingPointUnit::rounding;
bool                         FloatingPointUnit::exceptionEnabled[E_max];

// ======================================================================

const WORD PRECISION_MASK        = BINARY4(0000,0011,0000,0000);
const WORD PRECISION_24          = BINARY4(0000,0000,0000,0000);
const WORD PRECISION_53          = BINARY4(0000,0010,0000,0000);
const WORD PRECISION_64          = BINARY4(0000,0011,0000,0000);

const WORD ROUND_MASK            = BINARY4(0000,1100,0000,0000);
const WORD ROUND_NEAREST         = BINARY4(0000,0000,0000,0000);
const WORD ROUND_CHOP            = BINARY4(0000,1100,0000,0000);
const WORD ROUND_DOWN            = BINARY4(0000,0100,0000,0000);
const WORD ROUND_UP              = BINARY4(0000,1000,0000,0000);

const WORD EXCEPTION_PRECISION   = BINARY4(0000,0000,0010,0000);
const WORD EXCEPTION_UNDERFLOW   = BINARY4(0000,0000,0001,0000);
const WORD EXCEPTION_OVERFLOW    = BINARY4(0000,0000,0000,1000);
const WORD EXCEPTION_ZERO_DIVIDE = BINARY4(0000,0000,0000,0100);
const WORD EXCEPTION_DENORMAL    = BINARY4(0000,0000,0000,0010);
const WORD EXCEPTION_INVALID     = BINARY4(0000,0000,0000,0001);
const WORD EXCEPTION_ALL         = BINARY4(0000,0000,0011,1111);

// ======================================================================

#ifdef _DEBUG
namespace { void verifyControlWordRoundTrip(); }
#endif

// ======================================================================

void FloatingPointUnit::install(void)
{
	precision = P_24;
	rounding  = R_roundToNearestOrEven;
	memset(exceptionEnabled, 0, sizeof(exceptionEnabled));

	// preserve all other bits
	status  = getControlWord();
	status &= ~(PRECISION_MASK | ROUND_MASK | EXCEPTION_ALL);

	// set to single precision, rounding, and all exceptions masked
	status |= PRECISION_24 | ROUND_NEAREST | EXCEPTION_ALL;

	// check the config platform flags to see if we should enable some exceptions
	if (ConfigSharedFoundation::getFpuExceptionPrecision())
	{
		exceptionEnabled[E_precision] = true;
		status &= ~EXCEPTION_PRECISION;
	}

	if (ConfigSharedFoundation::getFpuExceptionUnderflow())
	{
		exceptionEnabled[E_underflow] = true;
		status &= ~EXCEPTION_UNDERFLOW;
	}

	if (ConfigSharedFoundation::getFpuExceptionOverflow())
	{
		exceptionEnabled[E_overflow] = true;
		status &= ~EXCEPTION_OVERFLOW;
	}

	if (ConfigSharedFoundation::getFpuExceptionZeroDivide())
	{
		exceptionEnabled[E_zeroDivide] = true;
		status &= ~EXCEPTION_ZERO_DIVIDE;
	}

	if (ConfigSharedFoundation::getFpuExceptionDenormal())
	{
		exceptionEnabled[E_denormal] = true;
		status &= ~EXCEPTION_DENORMAL;
	}

	if (ConfigSharedFoundation::getFpuExceptionInvalid())
	{
		exceptionEnabled[E_invalid] = true;
		status &= ~EXCEPTION_INVALID;
	}

	setControlWord(status);

#ifdef _DEBUG
	// Prove the x87<->_MCW_* boundary translation is lossless (review #1a).
	verifyControlWordRoundTrip();
#endif
}

// ----------------------------------------------------------------------

void FloatingPointUnit::update(void)
{
	WORD currentStatus = getControlWord();

	if (currentStatus != status)
	{
//		DEBUG_REPORT_LOG_PRINT(true, ("FPU: update=%d, in mode=%04x, should be in mode=%04x\n", updateNumber, static_cast<int>(currentStatus), static_cast<int>(status)));
		setControlWord(status);
	}

	++updateNumber;
}

// ----------------------------------------------------------------------

// ----------------------------------------------------------------------
//
// x87-hardware-layout <-> MSVC abstract _MCW_* translation (strategy A).
//
// CRITICAL (review #1a): every function in this module builds/tests `status`
// in RAW x87 HARDWARE control-word layout (the ROUND_*/PRECISION_*/EXCEPTION_*
// constants at the top of this file: ROUND_MASK=0x0C00, PRECISION_MASK=0x0300,
// EXCEPTION_ALL=0x003F). The CRT FP API (_controlfp_s/_control87) traffics in
// the MSVC ABSTRACT _MCW_*/_RC_*/_PC_*/_EM_* encoding, whose bit POSITIONS are
// DIFFERENT (e.g. _MCW_PC = 0x00030000, _MCW_EM = 0x0008001F). Handing the
// x87-layout `status` straight to a CRT call would set the wrong state and
// break update()'s `currentStatus != status` compare. So the whole module
// stays in x87 layout and we translate ONLY here, at the get/set boundary.
//
// Polarity note: x87 exception-mask bit SET = exception MASKED (disabled); the
// CRT _EM_* convention is identical (set = masked) -- do NOT invert.
//
namespace
{
	// Translate an x87-hardware-layout control WORD into the CRT abstract
	// _RC_*/_PC_*/_EM_* fields (the value you hand to _controlfp_s).
	unsigned int x87LayoutToMcw(FloatingPointUnit::WORD x87)
	{
		unsigned int mcw = 0;

		// --- rounding (x87 ROUND_MASK 0x0C00 -> _MCW_RC 0x0300) ---
		switch (x87 & ROUND_MASK)
		{
			case ROUND_NEAREST: mcw |= _RC_NEAR;  break;
			case ROUND_DOWN:    mcw |= _RC_DOWN;  break;
			case ROUND_UP:      mcw |= _RC_UP;    break;
			case ROUND_CHOP:    mcw |= _RC_CHOP;  break;
			default: break;
		}

		// --- precision (x87 PRECISION_MASK 0x0300 -> _MCW_PC 0x00030000) ---
		switch (x87 & PRECISION_MASK)
		{
			case PRECISION_24: mcw |= _PC_24;  break;
			case PRECISION_53: mcw |= _PC_53;  break;
			case PRECISION_64: mcw |= _PC_64;  break;
			default: break;
		}

		// --- exception masks (x87 0x003F -> _MCW_EM 0x0008001F), per-bit ---
		if (x87 & EXCEPTION_INVALID)     mcw |= _EM_INVALID;
		if (x87 & EXCEPTION_DENORMAL)    mcw |= _EM_DENORMAL;
		if (x87 & EXCEPTION_ZERO_DIVIDE) mcw |= _EM_ZERODIVIDE;
		if (x87 & EXCEPTION_OVERFLOW)    mcw |= _EM_OVERFLOW;
		if (x87 & EXCEPTION_UNDERFLOW)   mcw |= _EM_UNDERFLOW;
		if (x87 & EXCEPTION_PRECISION)   mcw |= _EM_INEXACT;

		return mcw;
	}

	// Translate the CRT abstract _MCW_* control value BACK into an
	// x87-hardware-layout WORD -- so getControlWord() returns bits that are
	// identical to what the old `fnstcw` inline asm returned and every
	// `status & MASK` test (and update()'s compare) keeps working unchanged.
	FloatingPointUnit::WORD mcwToX87Layout(unsigned int mcw)
	{
		FloatingPointUnit::WORD x87 = 0;

		switch (mcw & _MCW_RC)
		{
			case _RC_NEAR: x87 |= ROUND_NEAREST; break;
			case _RC_DOWN: x87 |= ROUND_DOWN;    break;
			case _RC_UP:   x87 |= ROUND_UP;      break;
			case _RC_CHOP: x87 |= ROUND_CHOP;    break;
			default: break;
		}

		switch (mcw & _MCW_PC)
		{
			case _PC_24: x87 |= PRECISION_24; break;
			case _PC_53: x87 |= PRECISION_53; break;
			case _PC_64: x87 |= PRECISION_64; break;
			default: break;
		}

		if (mcw & _EM_INVALID)     x87 |= EXCEPTION_INVALID;
		if (mcw & _EM_DENORMAL)    x87 |= EXCEPTION_DENORMAL;
		if (mcw & _EM_ZERODIVIDE)  x87 |= EXCEPTION_ZERO_DIVIDE;
		if (mcw & _EM_OVERFLOW)    x87 |= EXCEPTION_OVERFLOW;
		if (mcw & _EM_UNDERFLOW)   x87 |= EXCEPTION_UNDERFLOW;
		if (mcw & _EM_INEXACT)     x87 |= EXCEPTION_PRECISION;

		return x87;
	}
}

WORD FloatingPointUnit::getControlWord(void)
{
	// Read the CRT control word and translate the abstract _MCW_* bits back
	// into the x87-hardware layout this module traffics in (review #1a). The
	// returned WORD is bit-identical to the old `fnstcw` inline-asm result for the
	// ROUND/PRECISION/EXCEPTION fields, so update()'s compare is preserved.
	unsigned int current = 0;
	_controlfp_s(&current, 0, 0);   // mask 0 == read only
	return mcwToX87Layout(current);
}

// ----------------------------------------------------------------------

void FloatingPointUnit::setControlWord(WORD controlWord)
{
	const unsigned int mcw = x87LayoutToMcw(controlWord);

	// 32-bit-precision-retention NAMED DECISION (CONTEXT D-04 review #1b, LOCKED):
	// precision control (P_24) is RETAINED on the shipped 32-bit build -- the
	// door-snap causal theory (VERIFY-01) depends on the x87 24-bit-precision
	// force, so we deliberately keep writing it on 32-bit. It is a NO-OP only on
	// x64: x64 has no x87 precision control and passing _MCW_PC to the CRT on
	// x64 raises the invalid-parameter handler (Pitfall 1 -- NOT a silent no-op),
	// so the precision field is omitted from the x64 hardware write. `status`
	// still RECORDS the precision bits on both bitnesses, so the state-query API
	// stays consistent; only the x64 HARDWARE write drops precision. This is the
	// VERIFY-01 (door-snap) x64 watch-item. Never use __control87_2 (x64 error).
	unsigned int dummy = 0;
#if defined(_M_X64)
	_controlfp_s(&dummy, mcw, _MCW_RC | _MCW_EM);
#else
	_controlfp_s(&dummy, mcw, _MCW_RC | _MCW_PC | _MCW_EM);
#endif
}

// ----------------------------------------------------------------------

#ifdef _DEBUG
namespace
{
	// Round-trip self-check (review #1a -- a grep count is NOT proof): prove the
	// x87<->_MCW_* boundary translation is lossless. Capture the live control
	// word, write it back through setControlWord(), re-read it, and DEBUG_FATAL
	// if the ROUND/EXCEPTION fields (and, on 32-bit, the PRECISION field too)
	// do not round-trip byte-identically. Runs once at install (see install()).
	void verifyControlWordRoundTrip()
	{
		const FloatingPointUnit::WORD before = FloatingPointUnit::getControlWord();
		FloatingPointUnit::setControlWord(before);
		const FloatingPointUnit::WORD after = FloatingPointUnit::getControlWord();

#if defined(_M_X64)
		// x64 omits the precision hardware write, so compare ROUND+EXCEPTION only.
		const FloatingPointUnit::WORD compareMask =
			static_cast<FloatingPointUnit::WORD>(ROUND_MASK | EXCEPTION_ALL);
#else
		// 32-bit retains precision -- precision must round-trip too.
		const FloatingPointUnit::WORD compareMask =
			static_cast<FloatingPointUnit::WORD>(PRECISION_MASK | ROUND_MASK | EXCEPTION_ALL);
#endif
		DEBUG_FATAL((before & compareMask) != (after & compareMask),
			("FloatingPointUnit: x87<->_MCW_* control-word round-trip is LOSSY (before=%04x after=%04x mask=%04x)",
				static_cast<int>(before), static_cast<int>(after), static_cast<int>(compareMask)));
	}
}
#endif

// ----------------------------------------------------------------------

void FloatingPointUnit::setPrecision(Precision newPrecision)
{
	WORD bits = 0;

	switch (precision)
	{
		case P_24:
			bits = PRECISION_24;
			break;

		case P_53:
			bits = PRECISION_53;
			break;

		case P_64:
			bits = PRECISION_64;
			break;

		case P_max:
		default:
			DEBUG_FATAL(true, ("bad case"));
	}

	// record the current state
	precision = newPrecision;

	// set the proper bit pattern
	status &= ~PRECISION_MASK;
	status |= bits;

	// slam it into the FPU
	setControlWord(status);
}

// ----------------------------------------------------------------------

void FloatingPointUnit::setRounding(Rounding newRounding)
{
	WORD bits = 0;

	switch (newRounding)
	{
		case R_roundToNearestOrEven:
			bits = ROUND_NEAREST;
			break;

		case R_chop:
			bits = ROUND_CHOP;
			break;

		case R_roundDown:
			bits = ROUND_DOWN;
			break;

		case R_roundUp:
			bits = ROUND_UP;
			break;

		case R_max:
		default:
			DEBUG_FATAL(true, ("bad case"));
	}

	// record the current state
	rounding = newRounding;

	// set the proper bit pattern
	status &= ~ROUND_MASK;
	status |= bits;

	// slam it into the FPU
	setControlWord(status);
}

// ----------------------------------------------------------------------

void FloatingPointUnit::setExceptionEnabled(Exception exception, bool enabled)
{
	WORD bits = 0;

	switch (exception)
	{
		case E_precision:
			bits = EXCEPTION_PRECISION;
			break;

		case E_underflow:
			bits = EXCEPTION_UNDERFLOW;
			break;

		case E_overflow:
			bits = EXCEPTION_OVERFLOW;
			break;

		case E_zeroDivide:
			bits = EXCEPTION_ZERO_DIVIDE;
			break;

		case E_denormal:
			bits = EXCEPTION_DENORMAL;
			break;

		case E_invalid:
			bits = EXCEPTION_INVALID;
			break;

		case E_max:
		default:
			DEBUG_FATAL(true, ("bad case"));
	}

	// record the current state
	exceptionEnabled[exception] = enabled;

	// twiddle the bit appropriately.  these bits masks, so set the bit to disable the exception, clear the bit to enable it.
	if (enabled)
		status &= ~bits;
	else
		status |= bits;

	// slam it into the FPU
	setControlWord(status);
}

// ======================================================================
