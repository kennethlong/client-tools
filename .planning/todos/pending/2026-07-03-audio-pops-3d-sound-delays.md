---
created: 2026-07-03
title: Audio pops + slight delays on 3D sounds
area: client audio / Miles
status: backlog
priority: low-medium (noticeable, not blocking)
references:
  - memory: project_login_music_miles_integration_not_version (the earlier 3D SFX late/dropped
    arc was marked resolved 2026-07-03 — this is either a residual or a new, lesser regression)
  - memory: project_miles_93b_x64_3d_mixer_bug_use_93v
---

Kenny (2026-07-03, during the cantina fog-regression session, Win32 Release): audible **pops**
plus **slight delays on 3D sounds**. Filed as an observation — not yet triaged.

Context worth checking when picked up:
- The earlier "3D SFX late/dropped" open item was declared complete 2026-07-03; this may be a
  residual tail of that fix, or new.
- Suspect list from prior audio arcs: sample-handle pressure (32 handles,
  `s_requestedMaxNumberOfSamples`), `stage/miles/` provider set integrity, and the
  once-per-second sample-count probe plan in the login-music memory.
- Same session context: rebuilt exe (885b190a0 lineage) + Texture/TreeFile lock changes — if
  the delays correlate with zone-in bursts, consider lock-contention on the audio IO thread's
  TreeFile::open path (it now snapshots under ms_criticalSection) before blaming Miles.

2026-07-04 PM UPDATE 3 (CONSULT-61 — ROOT CAUSE + fix landed, follow-ups filed): the
file-callback layer was the arc's root cause — handle 0 == Miles' "unopened" sentinel
(the whole title-music shim was scaffolding over it; its one-slot state corrupted
multi-stream audio) + zero synchronization on s_fileMap/counter/shim across main +
Miles IO threads. FIXED (handles from 1, shim off, critical section). REMAINING follow-ups:
(1) **volume-snap pops** — 9.3b AIL_set_sample_volume_levels has NO ramp; Sound2d sets
volume per frame and the .snd volume-wander snaps at interpolationRate=0; VOLSTEP probe
lines convict/acquit next session; fix = engine-side volume lerp (max step/frame).
(2) **late one-shot starts** (door/footsteps) — cold synchronous first-touch load inside
playSound (TreeFile read + zlib + redundant AIL duration-probe, Audio.cpp:1640-1650) and
event templates freed between one-shots (footsteps re-decompress per step); fix wave =
keep-alive LRU template pin + interior event-sound pre-warm + header-computed duration;
maxSampleCount is a RED HERRING (exhaustion drops, never delays).
Full record: .planning/research/CONSULT-61-audio-popping-SYNTHESIS.md.

2026-07-04 PM UPDATE 2 (CONSULT-60 audio arc): **the load-phase crackle/skip regression was
OUR setLargePreMixBuffer(1024)** — on the live RadSoundSystem backend, SS_serve computes
`DesiredRemainder = ringTotal(256ms default) - mixAhead`; 1024 made it NEGATIVE so the fill
loop force-overwrote unplayed audio every timer tick (genericdig.cpp:124-148). FIXED:
`DIG_DS_FRAGMENT_CNT=2048` set BEFORE device open (ring TotalMs is read at open,
genericdig.cpp:339) + large mix-ahead 1500ms (< ring), normal stays 16. Also
`[ClientAudio] streamBufferBytes` (default 0 = stock 1s) + `audioDiagLog` per-stream
starvation edge-log to stage/audio-diag.log — armed in client.cfg for the next session.
**IN-GAME crackles/pops remain this todo's original (pre-existing) class** — if the diag
shows no STARVED edges in-game while pops occur, the cause is sample-side (handle pressure /
start clicks), not streaming.

2026-07-04 UPDATE (stall-watchdog conviction, CONSULT-59): **TreeFile lock contention REFUTED**
— in every whole-process stall dump the Miles threads are idle in WaitForSingleObject, never in
engine code. The load-phase music glitches/pops were pure MAIN-THREAD STARVATION (terrain
preload 3-4.5s + async-loader callback bursts, both fixed in the CONSULT-59 wave). Residual
startup music glitch = WorldSnapshot::load ~2.5s parse + 0.6-0.9s string-table pump frames
(see 2026-07-04-worldsnapshot-parse-startup-freeze.md). Settled-play census is clean (p99 17ms,
0 frames >80ms) — if 3D-sound skips persist in SETTLED play after the snapshot fix, THEN this
todo's Miles-side suspects (sample-handle pressure, provider set) are back on the table.
