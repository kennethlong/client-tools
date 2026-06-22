---
phase: 38
slug: utinni-advertised-client-coverage-completion
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-06-22
---

# Phase 38 — Validation Strategy

> Per-phase validation contract. This is a native engine exe with **no unit-test harness** —
> validation is the **build-link gate** + `dumpbin /exports` + the compiled-in coverage self-check
> (`utinni_verifyNoNullNoDup()`), and the **maintainer's live inject-smoke** (out of repo). Source:
> `38-RESEARCH.md` §Validation Architecture.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | none (native exe; no unit-test harness) — validation = build gate + the in-binary `utinni_verifyNoNullNoDup()` self-check |
| **Config file** | none |
| **Quick run command** | `dumpbin /exports stage/SwgClient_r.exe \| grep GetEngineHookPoints` (undecorated export) |
| **Full suite command** | canonical 5-target build Debug+Release (grep log: 0 `unresolved external symbol`) + boot smoke gl05/gl11 |
| **Estimated runtime** | ~5 min /t:SwgClient single-config; ~60 min full 5-target Optimized |

---

## Sampling Rate

- **Per task commit:** Debug build of `/t:SwgClient` (or the 5-target if a shared `.cpp` changed) — grep build log for **0 `unresolved external symbol`** (SwgClient links under /FORCE — exit 0 ≠ clean link).
- **Per wave merge:** full 5-target Debug + Release; `dumpbin /exports` on `_d` and `_r` (undecorated `GetEngineHookPoints`); boot gl05 to char-select + gl11 window-up.
- **Before phase gate:** all of the above green + `.h`/`.inc` re-synced to `D:/Code/Utinni` + EPA-08 handback doc written.
- **Max feedback latency:** ~5 min (single-config Debug link grep).

---

## Phase Requirements → Validation Map

| Req ID | Behavior | Validation Type | Command / Method | Exists? |
|--------|----------|-----------------|------------------|---------|
| EPA-05 | groundScene + chatWindow MI thunks resolve non-null | build + self-check | Debug build 0 unresolved; `utinni_verifyNoNullNoDup()` no-assert on Debug boot | ✅ self-check at `utinni_advertise.cpp` |
| EPA-06 | client/config shims + WR-05 resolve non-null | build + self-check | same; confirm `consoleHelper::sendInput` + `client::wndProc` rows non-null | ✅ |
| EPA-07 | remaining adds + version bump + gate in lockstep | compile-time + runtime | `static_assert(rowcount == .inc count)` green; runtime name-set-equality | ✅ |
| EPA-08 | DX11 evidence handback | documentation | dated handoff doc transcribing the LOCKED dumpbin + create/destroy ordering (CONTEXT.md D-01 / DX11 finding) | N/A (doc) |

---

## Wave 0 Gaps (infrastructure to stand up before/with first wave)

- [ ] `GroundScene.cpp` in-TU forwarder(s) (exe-reachable, NOT plugin-shared) for the **private** methods (`init/update/handleInputMapUpdate/handleInputMapEvent`) — these cannot be reached by a free thunk in `utinni_advertise.cpp` (C2248). New narrow forwarder + extern decl, the `utinni_installConfigFileOverride` precedent.
- [ ] `Os.cpp` / `DebugHelp.cpp` shim forward-declarations reachable from `utinni_advertise.cpp` (extern decl or tiny header) for `client::wndProc` (Os::WindowProc private + `__stdcall`) and `client::writeMiniDump`.
- [ ] No test-framework install needed (none exists; the compiled-in self-check IS the gate).

---

## Validation Bar (definition of "validated" for this phase)

1. Debug **and** Release 5-target builds link with **0 `unresolved external symbol`** (grep the log; `_o`/Optimized SAFESEH failure is PRE-EXISTING and not a gate per AGENTS.md).
2. `dumpbin /exports` shows `GetEngineHookPoints` undecorated on `_d` and `_r`.
3. Compile-time `static_assert` (table rowcount == `.inc` required-set count) holds after the version bump; runtime `utinni_verifyNoNullNoDup()` passes (no null / no dup / name-set equality) on Debug boot to char-select.
4. `.h` + `.inc` re-synced byte-identical into `D:/Code/Utinni/UtinniCore/swg/`; `UTINNI_HOOKPOINTS_VERSION` bumped 1→2.
5. EPA-08 handoff-back doc written (dumpbin + create/destroy ordering evidence + consumer-side items).
6. **Maintainer runs the live inject-smoke** (out of repo): Utinni injection → no `0xC0000005`, endpoints resolved by name. This phase produces the staged build + evidence and hands it back.
