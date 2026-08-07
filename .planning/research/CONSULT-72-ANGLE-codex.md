
# YOUR ANGLE (Codex) — the population call graph

Trace, exhaustively and by file:line, **every path by which an Object created from the world
snapshot ends up inside `ms_tangibleSphereTree`**.

Specifically:

1. From `WorldSnapshot` creating a node's Object, to that Object being inserted into
   `ms_tangibleSphereTree`. Name every function in the chain and every early return, guard,
   predicate or flag along it that could cause the insert not to happen.
2. Enumerate ALL insertion sites into `ms_tangibleSphereTree` (not just the snapshot one), and what
   drives each.
3. `ClientWorld` maintains more than one sphere tree. Identify all of them, and state exactly what
   decides WHICH tree a given object goes into. Could a portalized building end up in a tree other
   than the tangible one? Under what condition?
4. State what removes objects from the tree, and whether any removal path leaves an object in a
   state where a later re-add would be refused.

Deliver a call graph with citations, then a list of every gate on it. Do not rank them by
likelihood; I want the complete gate list, not your favourite.
