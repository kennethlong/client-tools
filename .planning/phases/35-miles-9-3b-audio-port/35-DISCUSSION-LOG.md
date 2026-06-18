# Phase 35: Miles 9.3b Audio Port - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-06-18
**Phase:** 35-miles-9-3b-audio-port
**Areas discussed:** 32-bit Miles policy, x64 enable strategy, Provider/redist set, Audio acceptance test

---

## 32-bit Miles policy (SC#4 fork)

| Option | Description | Selected |
|--------|-------------|----------|
| x64-only swap; Win32 stays 7.2e | Per-platform lib; zero regression risk to shipped 32-bit client | |
| Upgrade both platforms to 9.3b | Single lib version everywhere; retires 7.2e; re-validates 32-bit audio | ✓ |

**User's choice:** Upgrade both platforms to 9.3b (chose the more ambitious path over the recommended low-risk default).
**Notes:** Consequence — the API port moves into the shared (non-`_M_X64`) code path; SC#4 is met via "swap applied consistently + Win32 audio still works," not via "7.2e unregressed."

---

## x64 enable strategy

| Option | Description | Selected |
|--------|-------------|----------|
| Remove #if _M_X64 stubs; keep disableMiles cfg fallback | Real AIL_* on x64; keep cfg off-switch | |
| Fully delete all disable paths (always-on) | Strip stubs AND the toggle | ✓ (then refined) |

**User's choice:** Fully delete all disable paths — **then clarified mid-write: "leave the legacy short circuit."**
**Notes:** Final resolution (D-03/D-04): delete the **Phase-33 x64-specific** disable (macro stubs, SoundObject3d gate, install() x64 early-return) but **KEEP** the legacy retail `disableMiles` `[ClientAudio]` cfg short-circuit (`Audio.cpp:1300`). The legacy flag was distinct from the Phase-33 scaffolding; it stays as the supported per-platform off-switch.

---

## Provider / redist set

| Option | Description | Selected |
|--------|-------------|----------|
| Full 7.2e parity set | mss64.dll + mp3/ogg + dsp + eax/ds3d + dolby/srs + binkawin64 | ✓ |
| Minimal functional set | mss64.dll + mp3/ogg + dsp + binkawin64 only | |

**User's choice:** Full 7.2e parity set (Recommended).
**Notes:** Reverb/room-type is an explicit success criterion → EAX/DS3D filters required. Both platforms now need their respective 9.3b provider set (D-01 consequence).

---

## Audio acceptance test (AUDIO-02 bar)

| Option | Description | Selected |
|--------|-------------|----------|
| Scene checklist: music + UI clicks + 3D ambient + reverb | Full 4-dimension manual UAT, no warning-flood | ✓ |
| Smoke only | Any audio + no warning-flood at boot | |
| You decide | Planner derives UAT from the 4 named dimensions | |

**User's choice:** Scene checklist (Recommended).
**Notes:** "UI-rollovers-work-but-silent" is an explicit fail signature (half-dead Miles); warning-flood lag = missing-redist fail signature.

## Claude's Discretion

- `miles-9.3b/` internal vendoring layout (mirror `miles-7.2e/` convention vs SDK native tree).
- Whether to `git rm` dormant `miles-7.2e` + unversioned `miles/` now or defer to cleanup.

## Deferred Ideas

- Physical removal of dormant `miles-7.2e` / `miles/` trees — optional cleanup follow-up.
- New 9.3b-only audio features (sound banks, extra codecs, Miles Sound Studio tooling) — out of scope.
