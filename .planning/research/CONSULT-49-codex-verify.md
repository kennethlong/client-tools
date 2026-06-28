First read `.planning/research/CONSULT-49-EVIDENCE.md` in this repo (factual context; treat as GIVEN).

YOUR ANGLE (rigorous code verification — you are the repo tracer): VERIFY OR REFUTE the main agent's
claim that the hyperspace path deactivates the space-environment planets and NEVER reactivates them.
Be adversarial — try to find a reactivation the main agent missed.

Trace and report (file:line):
1. The full hyperspace lifecycle: who creates/runs `HyperspaceIoWin`, when `deactivateSpaceEnvironmentObjects`
   is called, and EVERY place the space environment objects could be set active again. Search beyond
   grep for "setActive(true)" — consider: is the `ClientSpaceTerrainAppearance`/`SpaceEnvironment`
   DESTROYED + RECREATED on arrival in the destination zone (which would re-create the planets ACTIVE,
   making the disable moot)? Does `Game::setScene`/zone-load rebuild the terrain appearance?
2. Is `deactivateSpaceEnvironmentObjects` even reached for a STATION-LAUNCH into space (the operator's
   case: ground station -> space), vs only for an in-space hyperspace JUMP between zones? What triggers
   HyperspaceIoWin, and does a launch go through it?
3. Is `SpaceEnvironment.cpp` / `HyperspaceIoWin.cpp` STOCK retail/SWGSource source or fork-modified
   here? Look for fork markers, diffs from the original SOE structure, or comments indicating edits.
4. If there genuinely is no reactivation: where is the CORRECT place to add `setActive(true)` on
   `m_objectsToDisableForHyperspace` (the dtor? a finish()? a per-frame re-enable)?

Deliver concisely: (a) is the no-reactivation claim CORRECT or is there a missed path (name it);
(b) is a station-launch even affected; (c) fix-code-where vs not-a-bug. Cite concrete file:line; no padding.
