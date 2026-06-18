---
phase: 35
slug: miles-9-3b-audio-port
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-06-18
---

# Phase 35 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.
>
> **Nature of this phase:** native MSBuild C++ engine-audio subsystem swap. There is **no automated
> test harness for `clientAudio`** in the repo, and audio output is inherently runtime/manual. The
> automatable surface is therefore **build-time gates** (link 0-unresolved, grep counts, file-exists)
> plus a **boot-log census**; the four audio dimensions (D-06) are human UAT by nature.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | None automated for audio (no clientAudio test harness in repo). Build + grep + file-exists gates substitute. |
| **Config file** | n/a |
| **Quick run command** | `& $env:MSBUILD src\build\win32\swg.sln /t:SwgClient /p:Platform=x64 /p:Configuration=Debug /nodeReuse:false` then grep log for `unresolved external symbol` (must be 0) |
| **Full suite command** | Canonical 5-target build (both platforms) + boot staged client + run D-06 manual UAT checklist |
| **Estimated runtime** | Build ~minutes; UAT ~10 min manual |

---

## Sampling Rate

- **After every task commit:** Build the touched target (clientAudio compile, then SwgClient link) + grep the log for `unresolved external symbol` (==0) and `LNK1181` (absent).
- **After every plan wave:** Full 5-target build BOTH platforms + boot smoke to character select.
- **Before `/gsd:verify-work`:** D-06 four-dimension UAT on x64 + a Win32 audio smoke, no warning-flood, all build/grep/file-exists gates green.
- **Max feedback latency:** build-gate seconds-to-minutes; UAT manual.

---

## Per-Task Verification Map

> Build-time / grep / file-exists gates are the **automatable** proof surface. Runtime audio is manual (D-06).

| Req | Behavior | Test Type | Automated Command / Gate | Manual? | Status |
|-----|----------|-----------|--------------------------|---------|--------|
| AUDIO-01 | x64 client links `mss64.lib`, 0 unresolved AIL_* | build-time | x64 build log: `unresolved external symbol` count == 0 AND no `LNK1181` | — | ⬜ pending |
| AUDIO-01 | Win32 client links `mss32.lib` (D-01 regression) | build-time | Win32 build log: same grep | — | ⬜ pending |
| AUDIO-01 | Phase-33 x64 stubs removed (D-03) | build-time | macro block (Audio.cpp:250-310), install x64 disable (1306-1318 + #else/#endif), SoundObject3d.cpp:32-36 gone; `_M_X64` count drops to only the codec-probe `#if` | — | ⬜ pending |
| AUDIO-01 | Legacy `disableMiles` cfg KEPT (D-04) | build-time | Audio.cpp:1298-1304 short-circuit still present | — | ⬜ pending |
| AUDIO-01 | room_type bus_index edit applied | build-time | `AIL_set_room_type(... , 0, ...)` middle-arg present at 26 sites; getter `AIL_room_type(dig, 0)` present | — | ⬜ pending |
| AUDIO-02 | Codec probe ported (no false ".m3d missing") | build-time + runtime-log | probe at Audio.cpp:~1399 no longer hardcodes `Msseax.m3d`/`msssoft.m3d`; boot log has NO "redist missing" warning | ⚠️ log check | ⬜ pending |
| AUDIO-02 | x64 provider set staged | build-time | `stage-x64/miles/` has `mss64.dll` + `mss64mp3.asi` + `mss64ogg.asi` + `mss64dsp.flt` + EAX/DS3D `.flt` + `binkawin64.asi` | — | ⬜ pending |
| AUDIO-02 | Win32 provider set staged | build-time | `stage/miles/` has `mss32.dll` + `mssmp3.asi` + `mssogg.asi` + filters + `binkawin.asi` | — | ⬜ pending |
| AUDIO-02 #1 | Music plays (char-select / login) | manual UAT | — | ❌ manual | ⬜ pending |
| AUDIO-02 #2 | 2D UI rollover + click SFX audible | manual UAT | — | ❌ manual | ⬜ pending |
| AUDIO-02 #3 | 3D positional ambient localizes (cantina) | manual UAT | — | ❌ manual | ⬜ pending |
| AUDIO-02 #4 | Reverb / room-type audible on cell entry | manual UAT | — | ❌ manual | ⬜ pending |
| AUDIO-02 | No warning-flood lag | manual UAT + log census | boot/play log NOT exploding the "Unable to create a valid sample id" signature | ⚠️ log census | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- No automated audio test harness exists — UAT is human-only by nature (acknowledged in D-06). **No Wave-0 test files to author.**
- The automatable surface is the build/grep/file-exists gates above (no new infra needed — uses the existing MSBuild + log-grep workflow from AGENTS.md).
- *(Optional nicety, not required)* a boot-log census helper counting `"Unable to create a valid sample id"` + `"redist missing"` would make the warning-flood check semi-automatable.

*Existing build infrastructure covers all automatable phase gates.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Music plays | AUDIO-02 | No audio capture/assert harness | Boot x64 client → char-select/login → music audible |
| 2D UI SFX | AUDIO-02 | runtime audio | Hover + click UI elements → rollover/click SFX audible (the "UI-works-but-silent" signature must NOT recur) |
| 3D positional ambient | AUDIO-02 | runtime spatial audio | Enter cantina interior → 3D ambient localizes by listener position |
| Reverb / room-type | AUDIO-02 | runtime DSP (validates bus_index=0, open A1) | Cross a cell boundary → audible reverb/room-type change |
| No warning-flood lag | AUDIO-02 | perf/log signature | Play session shows no flood of Miles warnings / frame-lag (the missing-`stage/miles` failure mode) |

---

## Validation Sign-Off

- [ ] All tasks have a build-time gate OR are flagged manual-UAT (no automated harness possible for audio)
- [ ] Sampling continuity: every code task has a build/grep gate; audio behavior tasks flagged manual
- [ ] Wave 0 covers all MISSING references (n/a — no test infra to author)
- [ ] No watch-mode flags
- [ ] D-06 four-dimension UAT defined with concrete scenes
- [ ] `nyquist_compliant: true` set once planner maps each code task to its build/grep gate

**Approval:** pending
