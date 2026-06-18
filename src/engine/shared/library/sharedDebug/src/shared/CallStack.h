// ======================================================================
//
// CallStack.h
// asommers
//
// Copyright 2004, Sony Online Entertainment
// All Rights Reserved
//
// ======================================================================

#ifndef INCLUDED_CallStack_H
#define INCLUDED_CallStack_H

#include <cstdint>   // PHASE-33 (A1-DBGHELP-RIP): uintptr_t for the call-stack entry type

// ======================================================================

class CallStack
{
public:


public:

	CallStack();
	~CallStack();

	bool operator<(const CallStack &o) const;

	void sample();

	void debugPrint() const;
	void debugLog() const;

private:

	enum
	{
		//-- This is to avoid memory allocations and needing to free the memory
		S_callStack = 32
	};

private:

	// PHASE-33 (A1-DBGHELP-RIP): uintptr_t entries so the full 64-bit Rip survives
	// on x64 (DebugHelp::getCallStack now writes uintptr_t). operator< memcmp uses
	// sizeof(m_callStack), so it scales with the wider element automatically.
	uintptr_t m_callStack[S_callStack];
};

// ======================================================================

#endif
