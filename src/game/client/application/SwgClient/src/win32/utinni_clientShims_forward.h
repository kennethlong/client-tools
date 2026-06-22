// ======================================================================
//
// utinni_clientShims_forward.h -- exe-local forwarder declarations for the
// two client::* external-linkage shims advertised to Utinni (Phase 38-02,
// EPA-06).
//
//   client::wndProc       -> utinni_osWindowProc   (DEFINED in Os.cpp; an
//                            external __stdcall/CALLBACK shim over the PRIVATE
//                            Os::WindowProc [Os.h:138]). The exe cannot name the
//                            private static, and Os.h is in sharedFoundation/
//                            src/win32 (not a public include dir), so the shim
//                            lives in Os.cpp and the exe sees only this decl.
//
//   client::writeMiniDump -> utinni_writeMiniDump  (DEFINED in DebugHelp.cpp; an
//                            external shim over DebugHelp::writeMiniDump
//                            [DebugHelp.h:36], whose win32 header is not on the
//                            exe include path). The exe sees only this decl.
//
// CALLING CONVENTION: utinni_osWindowProc is __stdcall (CALLBACK) -- a real
// window-proc convention, preserved exactly (NOT the __fastcall/__thiscall
// member-call emulation). utinni_writeMiniDump is a plain free function.
// Utinni-side typedefs:
//   LRESULT(__stdcall*)(HWND,UINT,WPARAM,LPARAM)        // client::wndProc
//   bool(*)(char const*, PEXCEPTION_POINTERS)           // client::writeMiniDump
//
// EXE-LOCAL: this header lives under SwgClient/src/win32 and is included ONLY by
// utinni_advertise.cpp. It MUST NOT be pulled by any gl0X plugin TU -- the shim
// DEFINITIONS in Os.cpp / DebugHelp.cpp do not change any struct ABI, but keeping
// this declaration header exe-local avoids any shared-header ABI cascade
// (AGENTS.md).
//
// 32-bit-only: the shim definitions are #if !defined(_WIN64) in Os.cpp /
// DebugHelp.cpp, matching the whole Win32-only advertise body. <windows.h>
// supplies HWND / UINT / WPARAM / LPARAM / LRESULT / CALLBACK and the
// PEXCEPTION_POINTERS type used in the writeMiniDump signature.
// ======================================================================

#ifndef INCLUDED_utinni_clientShims_forward_H
#define INCLUDED_utinni_clientShims_forward_H

// ======================================================================

#include <windows.h>   // HWND/UINT/WPARAM/LPARAM/LRESULT/CALLBACK + PEXCEPTION_POINTERS

// ----------------------------------------------------------------------
// extern declarations of the two client::* shims defined in their respective
// engine TUs (Os.cpp / DebugHelp.cpp).
// ----------------------------------------------------------------------
extern LRESULT CALLBACK utinni_osWindowProc(HWND, UINT, WPARAM, LPARAM);
extern bool utinni_writeMiniDump(char const *, PEXCEPTION_POINTERS);

// ======================================================================

#endif
