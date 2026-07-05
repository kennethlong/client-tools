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

## RenderDoc capture-and-diff evidence (2026-07-05 — Kenny caught TWO broken/good pairs)

`stage-x64\Capture100-103.rdc` (gl11/x64, cantina):
- Pair A: 100 broken (portal shows wrong wall) = **249 draws / 26,650 tris**;
  101 good (slightly different angle) = **291 draws / 32,842 tris** → 42 draws MISSING.
- Pair B: 102 broken (sky through portal) = **74 draws / 9,882 tris**;
  103 good = **234 draws / 47,959 tris** → 160 draws MISSING.
- In BOTH pairs every shared draw is byte-equal (diff status `equal`); the broken
  frames are missing whole blocks of DrawIndexed calls. **The cell geometry is never
  SUBMITTED — culled engine-side by the portal/cell visibility traversal.** Renderer
  fully exonerated (matches the cross-renderer/cross-bitness sightings).
- Symptom mechanics: a small camera angle/position change flips the visibility test
  back to correct ⇒ a boundary/degenerate case in the portal clip test (camera near
  a portal plane?) in the shared traversal — same math family the gl05-32bit
  FPU_PRESERVE fix touched, now misbehaving (rarely) even on SSE/x64.

## Next step when picked up
Trace the engine's portal-visibility walk (CellProperty/PortalProperty clip-plane
test feeding ClientWorld render submission) for the camera-near-portal-plane
degenerate case; candidate probe = log cell ids + portal clip results when the
visible-cell SET changes while the camera moved < epsilon. The four captures are
the ground truth for any hypothesis (broken frames enumerate exactly WHICH draws
vanish). Likely a CONSULT-64 crew round with the captures + this evidence as the pack.

## Priority
Cosmetic, self-clearing, intermittent — parked behind the audio arc close-out and
the TreeFile loose-searchPath negative-cache work. UPGRADED candidate: with a
RenderDoc-verified mechanism and cross-platform repro it is now cheap to pick up.
