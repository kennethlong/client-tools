Read D:/Code/swg-client-v2/.planning/research/CONSULT-66-portal-sigB-return-EVIDENCE.md
and D:/Code/swg-client-v2/.planning/research/CONSULT-66-raw-trace.txt first. Treat the
"LOCKED ground truth" section as measured fact.

TASK (repo call-graph trace — your specialty):

Trace the per-frame path from the engine to the dPVS portal traversal on this
codebase (x64, gl11 client):
RenderWorld (src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp)
-> camera/dPVS camera update -> Umbra/dPVS visibility query
(src/external/3rd/library/dpvs/implementation/sources/, esp.
dpvsVisibilityQuery_Traverse.cpp, dpvsVisibilityQuery_Test*, dpvsDatabase*).

Enumerate EVERY code path/early-out through which a visibility query starting
from a NON-world camera cell can complete having tested ZERO portals (the
exported reject/tested counters from swgDpvsGetPortalRejects() all zero) while
still resolving the camera's own cell visible. For each: (a) file:line, (b)
what state triggers it, (c) whether that state can PERSIST across many frames
of normal camera movement (the observed collapse lasted 16+ seconds), (d) what
would clear it.

Pay attention to how the camera's dPVS cell is assigned and what happens if
the camera object's cell association is stale/invalid, and to what the
dirty-node / database-update state machine does when counters show
dbUpd:0 dbRem:0 dbAdd:0.

Output: the enumerated list, then the single most timeline-consistent path,
then ONE probe line or experiment that would confirm/refute it on the next
sighting.
