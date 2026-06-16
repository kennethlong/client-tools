// ======================================================================
//
// RegexServices.cpp
// Copyright 2003 Sony Online Entertainment, Inc.
// All Rights Reserved.
//
// ======================================================================

#include "sharedRegex/FirstSharedRegex.h"
#include "sharedRegex/RegexServices.h"

#include <intrin.h>

// ======================================================================

static void * __cdecl localAllocate(size_t size, uint32 owner, bool array, bool leakTest)
{
	return MemoryManager::allocate(size, owner, array, leakTest);
}

// ----------------------------------------------------------------------
// Phase 31 (BITS-01, B-GAP-1): the original regexAllocate was a
// __declspec(naked) x86 trampoline (illegal on x64, C4235). It forwarded the
// allocation to MemoryManager via localAllocate, passing the CALLER's return
// address as the leak-tracking `owner` (the original read `[ebp+4]` -- the
// return address into allocateMemory's caller). The naked frame is replaced by
// a normal function that reads the same owner via the _ReturnAddress() intrinsic
// at the public entry point, so the leak owner remains the real call site rather
// than this library's internals. (owner is the existing 32-bit MemoryManager
// owner contract -- the A1-DBGHELP-RIP-class output-width residue is Phase 33.)

void *RegexServices::allocateMemory(size_t byteCount)
{
	uint32 const owner = static_cast<uint32>(reinterpret_cast<uintptr_t>(_ReturnAddress()));
	return localAllocate(byteCount, owner, false, true);
}

// ----------------------------------------------------------------------

void RegexServices::freeMemory(void *pointer)
{
	MemoryManager::free(pointer, false);
}

// ======================================================================
