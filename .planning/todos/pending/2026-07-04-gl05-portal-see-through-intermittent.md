# gl05 intermittent portal see-through (cantina entrance, Mos Eisley)

**Filed:** 2026-07-04 evening (Kenny, 2 sightings same day)

## Symptom
Standing at/in the Mos Eisley cantina entrance, a wall/portal face does not render —
raw terrain + sky visible through the building (screenshot
`stage/screenshots/screenShot0430.jpg`, 17:40:14). Seen twice on 2026-07-04, both on
**gl05/D3D9** (`rasterMajor=5`). **Does NOT persist** — walking out/in or relogging
clears it. Second sighting ~18:2x session.

## What's ruled out (log forensics, first sighting session)
- NO asset failure logged: zero mesh/appearance/texture/portal errors in
  SwgClient_report.log for the session (only the standing chiss/zam/dewback noise).
- Not streaming pop-in: screenshot is 30s after login; watchdog quiet at that moment.
- Not a stale DLL: staged gl05_r.dll (7/3) contains the 2026-06-20
  `D3DCREATE_FPU_PRESERVE` fix (the historical deterministic gl05-32bit
  see-through-cantina bug — same symptom family, but that one was deterministic at
  one spot and is verified fixed).

## Leads for when this is picked up
- Historical family: portal-visibility math (DPVS) glitch class — known intermittent
  quirks (memory: project_phase23_dpvs_verdict_option_alpha_revised; CORNERSNAP
  probes kept). Intermittent + self-clearing + renderer-specific-so-far points at
  order-of-operations in cell/portal visibility rather than data.
- Not yet seen on gl11 — but only 2 data points, both gl05 sessions. First step:
  try to catch it on gl11 to split renderer-mechanical vs shared portal logic.
- No repro recipe yet; both sightings were shortly after zone-in at the cantina.
- If it becomes deterministic: gl11 RenderDoc capture at the spot (RenderDoc cannot
  capture D3D9) + the CONSULT-56-era portal-visibility knowledge.

## Priority
Cosmetic, self-clearing, intermittent — parked behind the audio arc close-out and
the TreeFile loose-searchPath negative-cache work.
