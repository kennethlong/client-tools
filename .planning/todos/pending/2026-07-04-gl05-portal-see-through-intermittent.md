# Intermittent portal see-through (cantina entrance, Mos Eisley) — RENDERER- AND BITNESS-AGNOSTIC

**Filed:** 2026-07-04 evening (Kenny, 2 sightings same day, both gl05/Win32)
**UPDATE 2026-07-05:** 3rd sighting on **gl11/x64** (rasterMajor=11, the 7:30 AM
leak-fix build) — answers this todo's "first step" question: NOT renderer-mechanical,
NOT bitness. The suspect space collapses to SHARED engine portal/cell-visibility
logic (or data): the todo's DPVS/order-of-operations lead is now the primary one.

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
- ~~Not yet seen on gl11~~ **ANSWERED 07-05: seen on gl11/x64** → renderer- and
  bitness-agnostic → shared portal-visibility logic (DPVS cell/portal traversal) or
  portal data, NOT the renderer plugins. The FPU_PRESERVE family (precision-driven
  portal math divergence) is worth a second look now that it's cross-platform:
  something upstream may still feed marginal values into the portal clip test.
- No repro recipe yet; sightings cluster shortly after zone-in at the cantina.
- Since it reproduces on gl11: next sighting, grab a RenderDoc capture at the spot
  (F12; RenderDoc cannot capture D3D9) — capture-and-diff against a healthy frame
  of the same view is THE diagnostic. Also worth arming: a one-line probe in the
  portal-visibility walk logging cell/portal ids when a portal test flips frame-over-
  frame at a static camera (catches the transient without a screenshot).

## Priority
Cosmetic, self-clearing, intermittent — parked behind the audio arc close-out and
the TreeFile loose-searchPath negative-cache work.
