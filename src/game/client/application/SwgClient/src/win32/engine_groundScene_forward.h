// ======================================================================
//
// engine_groundScene_forward.h -- exe-local declarations for the PRIVATE
// GroundScene methods advertised to Utinni (Phase 38-01, EPA-05; 38-05 split).
//
// GroundScene::{init,update,handleInputMapUpdate,handleInputMapEvent} are PRIVATE
// [GroundScene.h:173,103,170,194], so they cannot be named from engine_advertise.cpp
// (C2248). The matching helpers are DEFINED in GroundScene.cpp (the class's own TU,
// which has member access); this header only DECLARES them so engine_advertise.cpp
// can reference them. TWO MECHANISMS after 38-05:
//   * CALLED/forwarder rows (init, handleInputMapUpdate): __fastcall call-through
//     forwarders -- Utinni invokes them, they forward.
//   * DETOURED rows (update, handleInputMapEvent): NO forwarder. They are advertised
//     by their REAL engine code entry via the engine_groundScene*RealEntry() address
//     providers below (a detour on a call-through forwarder is silently dead -- the
//     Utinni review finding). delta==0 verified inside each accessor.
//
// CALLING CONVENTION (forwarder rows): GroundScene is MULTIPLE-INHERITANCE
// (NetworkScene : public Scene, public MessageDispatch::Receiver) -> a raw PMF
// is inflated; each forwarder is __fastcall(GroundScene* /*ECX*/, int /*EDX*/,
// args) == __thiscall (dummy EDX; MSVC v145 forbids __thiscall on a free
// function, C3865). Utinni-side typedef per forwarder:
//   void(__thiscall*)(GroundScene*, const char*, CreatureObject*, float)   // init
//   void(__thiscall*)(GroundScene*)                                        // handleInputMapUpdate
// For the DETOURED rows, the address is the engine's own __thiscall entry --
//   void(__thiscall*)(GroundScene*, float)        // update (real entry)
//   void(__thiscall*)(GroundScene*, IoEvent*)     // handleInputMapEvent (real entry)
//
// EXE-LOCAL: this header lives under SwgClient/src/win32 and is included ONLY by
// engine_advertise.cpp. It MUST NOT be pulled by any gl0X plugin TU -- the
// forwarder DEFINITIONS in GroundScene.cpp do not change the GroundScene struct
// ABI, but keeping this declaration header exe-local avoids any shared-header
// ABI cascade (AGENTS.md). Forward-declares its argument types -- no heavy
// engine headers are included here.
//
// 32-bit-only: the forwarder definitions are #if !defined(_WIN64) in
// GroundScene.cpp, matching the whole Win32-only advertise body.
// ======================================================================

#ifndef INCLUDED_engine_groundScene_forward_H
#define INCLUDED_engine_groundScene_forward_H

// ======================================================================

class GroundScene;
class CreatureObject;
class MessageQueue;   // free-cam wave (v13): return type of engine_groundSceneGetDebugPortalCameraMessageQueue
struct IoEvent;   // IoEvent is a struct in-tree (sharedIoWin) -- match the tag to silence C4099

// ----------------------------------------------------------------------
// extern declarations of the four __fastcall groundScene private-method
// forwarders defined in GroundScene.cpp.
// ----------------------------------------------------------------------
void __fastcall engine_groundSceneInit(GroundScene * pThis, int /*edx*/,
	const char * terrainFilename, CreatureObject * player, float timeInSeconds);
void __fastcall engine_groundSceneHandleInputMapUpdate(GroundScene * pThis, int /*edx*/);
// (38-05) the update / handleInputMapEvent CALL-THROUGH forwarder decls were REMOVED
// -- those two rows are DETOURED and now advertise the REAL engine entry via the
// real-entry accessors below.

// ----------------------------------------------------------------------
// 38-05 (address-correctness): real-entry accessors for the two DETOURED private
// methods (update, handleInputMapEvent). Defined in GroundScene.cpp (friend access
// to take the PRIVATE PMF), they return the REAL engine code entry (delta==0
// verified) the engine's own call path reaches -- the address Utinni must DETOUR.
// The call-through __fastcall forwarders above are correct ONLY for CALLED rows; a
// detour placed on a forwarder is silently dead (the engine never calls it). These
// take no args -- they are pure address providers, not call-throughs.
// ----------------------------------------------------------------------
void * engine_groundSceneUpdateRealEntry();
void * engine_groundSceneHandleInputMapEventRealEntry();

// ----------------------------------------------------------------------
// FREE-CAM wave (v13): CALLED accessor for the debug-portal-camera input MessageQueue.
// m_debugPortalCameraInputMap is PRIVATE [GroundScene.h:111], so this friend forwarder is
// DEFINED in GroundScene.cpp (member access + InputMap already included there) and only
// DECLARED here. Returns m_debugPortalCameraInputMap->getMessageQueue() -- the MQ the
// consumer previously read at the hardcoded InputMap+0xC. __fastcall(GroundScene* /*ECX*/,
// int /*EDX*/) == __thiscall (GroundScene is MI -> dummy EDX). Utinni-side typedef:
//   MessageQueue*(__thiscall*)(GroundScene*)
// ----------------------------------------------------------------------
MessageQueue * __fastcall engine_groundSceneGetDebugPortalCameraMessageQueue(GroundScene * pThis, int /*edx*/);

// ======================================================================

#endif
