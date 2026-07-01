// ======================================================================
//
// engine_creatureObject_forward.h -- exe-local declaration for the Bucket A
// (2026-06-28) creatureObject::setTarget real-entry address provider.
//
// engine_advertise.cpp cannot #include clientGame/CreatureObject.h: that header
// transitively pulls sharedSkillSystem/SkillObjectArchive.h, whose include dir is
// NOT on the SwgClient (exe) project's include path (it is on clientGame's). So the
// PMF real-entry of CreatureObject::setLookAtTarget is computed by an out-of-line
// accessor compiled in CreatureObject.cpp (the class's own TU, where the header
// builds), exactly like the engine_groundScene*RealEntry() accessors live in
// GroundScene.cpp. This header only DECLARES it.
//
// WHY setLookAtTarget: there is NO CreatureObject::setTarget in this tree; the
// "current target" setter is the PUBLIC non-virtual setLookAtTarget(const NetworkId&)
// [CreatureObject.h:311] (m_lookAtTarget = "this creature's current target"). The
// contract name stays creatureObject::setTarget (the consumer's lookup key); this is
// the same MISMATCH-name mapping used elsewhere in the table.
//
// REAL ENTRY (detoured row): CreatureObject is MULTIPLE-INHERITANCE
// (TangibleObject : public ClientObject, public CallbackReceiver), so &CreatureObject::
// setLookAtTarget is an inflated PMF { void* pfn; int delta; }. setLookAtTarget is an
// OWN method of the most-derived class -> primary base at offset 0 -> delta==0 and pfn
// is the real engine entry the engine's own call path reaches (the address Utinni must
// DETOUR). The accessor hard-gates delta==0 and returns nullptr otherwise, so
// engine_verifyNoNullNoDup() fails loudly rather than advertising a wrong entry.
//
// EXE-LOCAL: included ONLY by engine_advertise.cpp. setLookAtTarget is PUBLIC, so the
// accessor needs no friend grant and CreatureObject.h is UNCHANGED (no shared-header
// ABI cascade). 32-bit-only: the definition is #if !defined(_WIN64) in
// CreatureObject.cpp, matching the whole advertise body.
// ======================================================================

#ifndef INCLUDED_engine_creatureObject_forward_H
#define INCLUDED_engine_creatureObject_forward_H

// Returns the REAL engine code entry of CreatureObject::setLookAtTarget (delta==0
// verified), the address Utinni detours for the creatureObject::setTarget row.
// Defined in CreatureObject.cpp.
void * engine_creatureSetTargetRealEntry();

#endif
