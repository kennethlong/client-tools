**Out of scope (consumer-side / deferred — note in handoff-back, do not implement here):** virtual-method resolution off the live vtable (`object::{addToWorld,removeFromWorld,setParentCell}`, `cui::io::processEvent`); Utinni's `directx11.cpp tryInstall()` poll-until-device+context guard + the three-pointer diagnostic log; dropping Utinni's per-subsystem `installable()`/group skips; the mid-function-patch features (Issue #11 chat routing, UI cascade, debug-cam suppress, `.trn`-name NOP) — each needs a joint cooperative-API decision with Utinni.

**Plans:** 4 plans (sequential — every plan edits `utinni_advertise.cpp` + the `.inc`, so they serialize; worktrees disabled project-wide)
- [ ] 38-01-PLAN.md — groundScene coverage: 3 public MI thunks + ctor thunk + 4 private in-TU forwarders; draw SKIP, g_instance OMIT (EPA-05)
- [ ] 38-02-PLAN.md — client/config external-linkage shims (wndProc/writeMiniDump) + CuiPreferences setModalChat/getModalChat + commit parked WR-05; writeCrashLog/setupStartDataInstall NONEXISTENT-OMIT (EPA-06)
- [ ] 38-03-PLAN.md — chatWindow MI thunks (3 clean + chatEnterHandler, ctor DEFER) + Bucket-4 confirm-or-OMIT + UTINNI_HOOKPOINTS_VERSION 1→2 (EPA-05, EPA-07)
- [ ] 38-04-PLAN.md — contract re-sync byte-exact to D:/Code/Utinni + EPA-08 DX11-evidence handback doc + final 5-target Debug+Release build + dumpbin (EPA-07, EPA-08)
