First, read `D:/Code/swg-client-v2/.planning/research/CONSULT-47-EVIDENCE.md` — it is the factual
context (treat as GIVEN; do not re-derive).

Two extracted DATA artifacts are provided as loose text files (the two copies of the space-HUD UI,
decompressed from the TREs):
- `.planning/research/CONSULT-47-data-ILM_ui--ui_hud_space.inc`
- `.planning/research/CONSULT-47-data-swgsource_3.0--ui_hud_space.inc`

YOUR ANGLE (data vintage — you are the most detailed file reader): determine the VINTAGE and
INTERNAL CONSISTENCY of the client DATA, and whether it matches the client code's expectations.

Investigate:
1. Diff the two `ui_hud_space.inc` copies in fine detail. Enumerate every widget / CodeData entry
   that differs. Which copy is newer / more complete? Quote the differing regions (line-level).
2. Hunt for any version / build / date / author markers inside either file or its structure.
3. Reconcile these specific oddities and say what they imply about how the data was assembled:
   - ILM_ui has the CodeData KEY `buttonEnterSpace='buttons.enterSpaceButton'` but NO
     `enterSpaceButton` widget in the page tree.
   - swgsource_3.0 has neither the key nor the widget.
   - BOTH have the `missileLockOnYou` + `missileLockOnYouDeformer` WIDGETS but NO CodeData mapping
     for them.
   Is this DATA internally self-consistent (one authored vintage), or a partial / frankenstein mix
   of vintages? Is ILM_ui a genuinely newer space-HUD that swgsource_3.0's override regresses?

Deliver, concisely: (a) which copy is newer and by how much; (b) whether the DATA looks like a
complete single-version JTL UI set or a mixed/incomplete one; (c) the single most decisive test to
disambiguate "real SWGSource JTL gap" vs "operator paired mismatched source/data versions". Quote
concrete lines; do not pad.
