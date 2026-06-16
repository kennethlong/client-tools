// ======================================================================
//
// RenderWorld_Services.cpp
// Copyright 2000-01, Sony Online Entertainment Inc.
// All Rights Reserved.
//
// ======================================================================

#include "clientGraphics/FirstClientGraphics.h"
#include "clientGraphics/RenderWorldServices.h"

#include "sharedFoundation/ExitChain.h"
#include "sharedSynchronization/Mutex.h"

#include <intrin.h>

// ======================================================================

RenderWorldServices::RenderWorldServices()
: DPVS::LibraryDefs::Services(),
	m_mutex(new Mutex())
{
}

// ----------------------------------------------------------------------

RenderWorldServices::~RenderWorldServices()
{
	delete m_mutex;
}

// ----------------------------------------------------------------------

void RenderWorldServices::error(const char * message)
{
	UNREF(message);
//	if (!ExitChain::isFataling())
//		FATAL(true, ("DPVS error: %s", message));
}

// ----------------------------------------------------------------------

static void * __cdecl localAllocate(size_t size, uint32 owner, bool array, bool leakTest)
{
	return MemoryManager::allocate(size, owner, array, leakTest);
}

// ----------------------------------------------------------------------
// Phase 31 (BITS-01, B-GAP-1): dpvsAllocate was a __declspec(naked) x86
// trampoline (illegal on x64, C4235) forwarding to MemoryManager with the
// CALLER's return address as the leak `owner`. Replaced by a normal function
// that captures the same owner via _ReturnAddress() at the public entry point.

void *RenderWorldServices::allocateMemory(size_t bytes)
{
	uint32 const owner = static_cast<uint32>(reinterpret_cast<uintptr_t>(_ReturnAddress()));
	return localAllocate(bytes, owner, false, true);
}

// ----------------------------------------------------------------------

void RenderWorldServices::releaseMemory(void * ptr)
{
	MemoryManager::free(ptr, false);
}

// ----------------------------------------------------------------------

void RenderWorldServices::enterMutex()
{
	m_mutex->enter();
}

// ----------------------------------------------------------------------

void RenderWorldServices::leaveMutex()
{
	m_mutex->leave();
}

// ======================================================================
