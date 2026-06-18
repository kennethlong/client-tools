// ======================================================================
//
// DebugHelp.h
// copyright 2000 Verant Interactive
//
// ======================================================================

#ifndef DEBUG_HELP_H
#define DEBUG_HELP_H

#include <cstdint>   // PHASE-33 (A1-DBGHELP-RIP): uintptr_t for the full-width call-stack entry type

// ======================================================================

typedef unsigned long uint32;

// ======================================================================

class DebugHelp
{
public:

	static void install();
	static void remove();

	static bool loadSymbolsForDll(const char *name);

	// PHASE-33 (A1-DBGHELP-RIP, reviews fix #10a): the call-stack entry type is
	// `uintptr_t` so the captured instruction pointer is NOT truncated on x64
	// (an x64 crash handler must carry the full 64-bit RIP). On 32-bit `uintptr_t`
	// is 4 bytes == the prior `uint32`, so the 32-bit ABI/behavior is byte-unchanged.
	static void getCallStack(uintptr_t *callStack, int sizeOfCallStack);
	static void reportCallStack(int const maxStackDepth = 4);
	static bool lookupAddress(uintptr_t address, char *libName, char *fileName, int fileNameLength, int &line);

	static bool writeMiniDump(char const *miniDumpFileName=0, PEXCEPTION_POINTERS exceptionPointers=0);
};

// ======================================================================

#endif
