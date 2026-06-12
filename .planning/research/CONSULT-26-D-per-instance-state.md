# Research task: inventory per-instance render state for a skinned appearance (read-only, no fixes)

You are auditing this repository. Do NOT propose fixes or change files. Produce a written inventory.

Scenario: one skeletal appearance (e.g. `appearance/jawa_m.sat`) is instantiated several times in a single scene. Enumerate **all per-instance state** that flows into the vertex/draw path for `SoftwareBlendSkeletalShaderPrimitive` (in `src/engine/client/library/clientSkeletalAnimation`). For each piece of state, give:

- what it is (field / object / buffer) and the owning class, with file:line
- its **lifetime**: when it is created, when it is rebuilt, when it is freed
- whether it is **shared across instances** or **unique per instance**
- what could make one instance's value differ from another identical instance's value **at draw time**

Cover at minimum: the dynamic vertex buffer and its bound offset, the system vertex buffer, the index buffer, the skeleton / transform / bone-matrix array, the selected LOD index, and any lazily-built or cached resources (and what triggers a rebuild).

Then: of everything above, which items are **written in one pass and consumed (drawn) in a later pass** within the same frame (i.e. a deferred producer→consumer gap), and what — if anything — guarantees the produced value survives unmodified until the consuming draw?

Deliverable: a table (state | owner+file:line | lifetime | shared-or-per-instance | how it could diverge), followed by the deferred producer→consumer list. Map faithfully; do not diagnose a specific bug.
