// ======================================================================
//
// utinni_groundScene_forward.h -- exe-local forwarder declarations for the
// four PRIVATE GroundScene methods advertised to Utinni (Phase 38-01, EPA-05).
//
// GroundScene::{init,update,handleInputMapUpdate,handleInputMapEvent} are
// PRIVATE [GroundScene.h:173,77,170,168], so they cannot be named from
// utinni_advertise.cpp (C2248). The matching __fastcall forwarders are DEFINED
// in GroundScene.cpp (the class's own TU, which has member access); this header
// only DECLARES them so utinni_advertise.cpp can take their addresses.
//
// CALLING CONVENTION: GroundScene is MULTIPLE-INHERITANCE
// (NetworkScene : public Scene, public MessageDispatch::Receiver) -> a raw PMF
// is inflated; each forwarder is __fastcall(GroundScene* /*ECX*/, int /*EDX*/,
// args) == __thiscall (dummy EDX; MSVC v145 forbids __thiscall on a free
// function, C3865). Utinni-side typedef per forwarder:
//   void(__thiscall*)(GroundScene*, const char*, CreatureObject*, float)   // init
//   void(__thiscall*)(GroundScene*, float)                                 // update
//   void(__thiscall*)(GroundScene*)                                        // handleInputMapUpdate
//   void(__thiscall*)(GroundScene*, IoEvent*)                              // handleInputMapEvent
//
// EXE-LOCAL: this header lives under SwgClient/src/win32 and is included ONLY by
// utinni_advertise.cpp. It MUST NOT be pulled by any gl0X plugin TU -- the
// forwarder DEFINITIONS in GroundScene.cpp do not change the GroundScene struct
// ABI, but keeping this declaration header exe-local avoids any shared-header
// ABI cascade (AGENTS.md). Forward-declares its argument types -- no heavy
// engine headers are included here.
//
// 32-bit-only: the forwarder definitions are #if !defined(_WIN64) in
// GroundScene.cpp, matching the whole Win32-only advertise body.
// ======================================================================

#ifndef INCLUDED_utinni_groundScene_forward_H
#define INCLUDED_utinni_groundScene_forward_H

// ======================================================================

class GroundScene;
class CreatureObject;
class IoEvent;

// ----------------------------------------------------------------------
// extern declarations of the four __fastcall groundScene private-method
// forwarders defined in GroundScene.cpp.
// ----------------------------------------------------------------------
void __fastcall utinni_groundSceneInit(GroundScene * pThis, int /*edx*/,
	const char * terrainFilename, CreatureObject * player, float timeInSeconds);
void __fastcall utinni_groundSceneUpdate(GroundScene * pThis, int /*edx*/, float elapsedTime);
void __fastcall utinni_groundSceneHandleInputMapUpdate(GroundScene * pThis, int /*edx*/);
void __fastcall utinni_groundSceneHandleInputMapEvent(GroundScene * pThis, int /*edx*/, IoEvent * event);

// ======================================================================

#endif
