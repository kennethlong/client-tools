First read `D:/Code/swg-client-v2/.planning/research/CONSULT-49-EVIDENCE.md` (factual context; GIVEN).

YOUR ANGLE (fine-grained lifecycle reader — you are the most detailed code reader): walk the EXACT
sequence of the space environment + hyperspace objects from zone-load to in-space, line by line, and
determine whether the planets end up `setActive(false)` with nothing restoring them.

Read in detail and report (file:line):
1. `SpaceEnvironment` (clientTerrain/.../appearance/SpaceEnvironment.cpp) full lifecycle: ctor (what's
   added to `m_objectsToDisableForHyperspace` vs `m_objectVector` vs the camera-follow vector), dtor,
   `render()`, `alter()`, and EVERY method that touches object active-state. Are celestials (suns) also
   disabled, or only distant-appearance planets? Are the disabled objects ever re-added or recreated?
2. `HyperspaceIoWin.cpp` full flow: the stages (Cockpit/ThirdPerson), `finish()`, `~HyperspaceIoWin`,
   and `disableOtherObjectsThisFrame` (line ~335) — is THAT a per-frame disable that self-restores when
   the IoWin closes (so world SHIPS come back), while `disableEnvironmentForHyperspace` is a one-shot
   that does NOT? Distinguish the two mechanisms precisely.
3. The operator sees: planets gone, ships gone, starfield + markers remain. Map each missing thing to
   exactly which disable mechanism (and whether each has a restore). Does the evidence cleanly explain
   BOTH planets AND ships, or only planets?
4. If there's a restore the main agent missed, quote it. If not, identify the single cleanest insertion
   point for the reactivation.

Deliver concisely: (a) planets — confirmed deactivated-without-restore? (b) ships — same mechanism or
different/separate? (c) exact fix location, or the missed restore. Quote concrete lines; no padding.
