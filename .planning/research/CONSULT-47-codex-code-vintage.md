First, read `.planning/research/CONSULT-47-EVIDENCE.md` in this repo — it is the factual context
(treat as GIVEN; do not re-derive).

YOUR ANGLE (code vintage — you are the repo tracer): Using ONLY internal evidence in this source
tree (`D:/Code/swg-client-v2`), determine what SWG client PUBLISH / VERSION / ERA this CODE
corresponds to, and whether that is NEWER or OLDER than a "base JTL" (pre-Chapters) client.

Investigate:
1. Version / publish / era markers in the source: version strings, build tags, `#define`s,
   capability/feature flags, datatable schema versions, ClientMachine/GameFeature bits, any
   changelog or publish identifiers. Where does this tree sit on the SWG timeline?
2. The space/JTL feature level the CODE implements. Specifically the contract `SwgCuiHudSpace`
   requires (the 7 hard CodeData fetches — esp. `buttonEnterSpace` "atmospheric flight" and
   `missileLockOnYou`) and `NebulaManagerClient`'s lightning-appearance requirement. Do these tie to
   a specific known SWG publish/chapter?
3. Is "atmospheric flight" / `enterSpaceButton` a first-class, fully wired feature in THIS code
   (implying a Chapters-era / post-base-JTL client), or vestigial?

Deliver, concisely: (a) which SWG publish/era this CODE most likely is, with file:line citations;
(b) is the code newer or older than a base-JTL UI dataset; (c) the single most decisive test that
would disambiguate "real SWGSource JTL gap" vs "operator paired mismatched source/data versions".
Do not pad. Cite concrete evidence.
