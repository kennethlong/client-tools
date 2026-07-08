TASK — precise file:line state trace in this repo (D:/Code/swg-client-v2). Do NOT summarize the evidence files; perform the trace and answer the three questions.

CONTEXT (measured facts, do not re-derive): In a live x64 gl11 session, the camera entered an interior cell (foyer1) of a Mos Eisley POB. One second later a door closed the cell's world-facing portal pair (Portal::setClosed on BOTH directions, foyer1->world and world->foyer1, probe shows dpvs=yes on both). On the very next probe line the dPVS visibility query collapsed: portals=0, visCells=1 (own cell only), tested=0, ALL dPVS reject counters zero — and the state persisted 16+ seconds of normal camera movement (everything beyond every portal rendered as skybox). In a healthy earlier session the SAME door pair closed while the camera was in the WORLD cell and traversal stayed healthy (tested:2). Full probe extracts: .planning/research/CONSULT-66-raw-trace.txt; background: .planning/research/CONSULT-66-portal-sigB-return-EVIDENCE.md.

TRACE REQUIRED, with file:line at each hop: what Portal::setClosed(true) does end-to-end in this codebase — sharedObject CellProperty/PortalProperty -> clientGraphics RenderWorld dPVS hooks -> vendored dPVS state (src/external/3rd/library/dpvs/implementation/sources/): which dPVS object gets ENABLED=false, what cell/database state is dirtied or NOT dirtied, and what the next visibility query does when the CAMERA'S OWN cell has one freshly disabled portal among several enabled ones.

ANSWER:
1. Can disabling one portal of the camera's own cell make the traversal test ZERO portals of that cell (not merely skip the disabled one)? Through what exact state?
2. Both directions of the pair are disabled in the same frame — any pathway where the paired disable leaves the cell's portal list, bounds, or dirty/database state inconsistent (note the collapse line shows dbUpd:0 dbRem:0 dbAdd:0)?
3. What is mechanically different when the camera sits in the SMALLER cell of the pair at close time (sick case) versus the world cell (healthy case)?

FINISH with: the single most timeline-consistent mechanism, and ONE probe line or experiment to discriminate it on the next sighting.
