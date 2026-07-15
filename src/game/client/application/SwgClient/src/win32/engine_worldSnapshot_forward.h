// ======================================================================
//
// engine_worldSnapshot_forward.h -- exe-local declarations of the Goal B
// Wave-1 snapshot-editor READ shims (hookpoints v17; rev-3 freeze 2026-07-15).
//
// The shims are extern "C" __cdecl and DEFINED in clientGame
// WorldSnapshot.cpp -- the TU that owns the file-scope ms_reader singleton
// and its bookkeeping (the sysmsg/lookAtTarget shim pattern at TU scale).
// engine_advertise.cpp includes this header to take constant &fn addresses.
//
// UtinniWsNodeInfo (the wsGetNodeInfo POD-out) is defined in the shared
// contract header engine_hookpoints.h; an incomplete declaration suffices
// here (the advertiser only takes the function address).
//
// ======================================================================

#ifndef INCLUDED_engine_worldSnapshot_forward_H
#define INCLUDED_engine_worldSnapshot_forward_H

struct UtinniWsNodeInfo;

extern "C" int     __cdecl utinni_wsGetNodeCount(void);
extern "C" __int64 __cdecl utinni_wsGetTopNodeIdAt(int index);
extern "C" int     __cdecl utinni_wsGetChildCount(__int64 networkIdInt);
extern "C" __int64 __cdecl utinni_wsGetChildIdAt(__int64 networkIdInt, int index);
extern "C" int     __cdecl utinni_wsGetNodeInfo(__int64 networkIdInt, UtinniWsNodeInfo* out);
extern "C" int     __cdecl utinni_wsGetNodeTemplateName(__int64 networkIdInt, char* buf, int cap);
extern "C" int     __cdecl utinni_wsGetGeneration(void);

#endif // INCLUDED_engine_worldSnapshot_forward_H
