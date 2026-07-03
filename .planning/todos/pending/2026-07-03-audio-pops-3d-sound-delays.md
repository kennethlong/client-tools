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
