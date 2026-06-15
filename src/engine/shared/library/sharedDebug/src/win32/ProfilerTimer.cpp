// ======================================================================
//
// ProfilerTimer.cpp
// copyright (c) 2001 Sony Online Entertainment
//
// ======================================================================

#include "shareddebug/FirstSharedDebug.h"
#include "shareddebug/ProfilerTimer.h"

#include "shareddebug/DebugFlags.h"
#include "sharedFoundation/WindowsWrapper.h"

#include <intrin.h>

// ======================================================================

namespace ProfilerTimerNamespace
{
	ProfilerTimer::Type ms_qpcFrequency;
	float               ms_floatQpcFrequency;
	__int64             ms_rdtsc;
	__int64             ms_qpc;
	bool                ms_useRdtsc;

}
using namespace ProfilerTimerNamespace;

// ======================================================================

// BITS-01 (Phase 31): the original function was a naked __stdcall wrapper whose
// inline-asm body issued the rdtsc instruction — x64-illegal (C4235), and the
// naked attribute is unsupported on x64.
// Replaced with the __rdtsc() intrinsic. __rdtsc() returns `unsigned __int64`;
// the `static_cast<__int64>` KEEPS this function's existing `__int64` return type
// so every caller's signature is intact and no implicit C4244 narrowing fires.
static __int64 readTimeStampCounter()
{
	return static_cast<__int64>(__rdtsc());
}

// ======================================================================

void ProfilerTimer::install()
{
	IGNORE_RETURN(QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER *>(&ms_qpc)));
	ms_rdtsc = readTimeStampCounter();

	IGNORE_RETURN(QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER *>(&ms_qpcFrequency)));
	ms_floatQpcFrequency = static_cast<float>(ms_qpcFrequency);

	DebugFlags::registerFlag(ms_useRdtsc, "SharedDebug/Profiler", "useRdtsc");
}

// ----------------------------------------------------------------------

void ProfilerTimer::getTime(Type &time)
{
	if (ms_useRdtsc)
		time = readTimeStampCounter();
	else
		IGNORE_RETURN(QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER *>(&time)));	
}

// ----------------------------------------------------------------------

void ProfilerTimer::getCalibratedTime(Type &time, Type &frequency)
{
	if (ms_useRdtsc)
	{
		__int64 qpc;
		IGNORE_RETURN(QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER *>(&qpc)));
		__int64 rdtsc = readTimeStampCounter();

		float const t = static_cast<float>(qpc - ms_qpc) / ms_floatQpcFrequency;
		frequency = static_cast<__int64>(static_cast<float>(rdtsc - ms_rdtsc) / t);

		time = rdtsc;
		ms_qpc = qpc;
		ms_rdtsc = time;
	}
	else
	{
		IGNORE_RETURN(QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER *>(&time)));
		frequency = ms_qpcFrequency;
	}
}

void ProfilerTimer::getFrequency(Type &frequency)
{
	frequency = ms_qpcFrequency;
}

// ======================================================================
