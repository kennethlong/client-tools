# Research task: map the skinning-mode & LOD decision inputs (read-only, no fixes)

You are auditing this repository's client engine. Do NOT propose fixes or change any files. Produce a precise written map.

In `src/engine/client/library/clientSkeletalAnimation` and `src/engine/client/library/clientObject`, trace and document, per-frame per-object:

1. How `SkeletalAppearance2` selects its **LOD** each frame — the exact function(s), the inputs, and the math/thresholds.
2. What decides whether a skeletal mesh renders via **`SoftwareBlendSkeletalShaderPrimitive`** (software blend skinning) versus any hardware-skin or no-skin path — the exact decision point and every input to it.

Enumerate **every input** that feeds those two decisions. Explicitly confirm or deny each of these as an input, with the file:line where it is (or is not) consulted:
- camera distance to the object
- on-screen / projected size (screen-diameter fraction)
- whether the object is the **current target**, **under the reticle/cursor**, or **moused-over**
- whether the object is in the view frustum / near the **center of view** / facing the camera
- fade state, hologram/cloak state
- frame-to-frame **hysteresis** (does a chosen LOD or skinning mode persist / resist dropping back once raised?)
- anything else you find

Deliverable: a call-graph from the per-frame render entry point down to the LOD pick and the skinning-mode pick, plus a bullet list of decision inputs with file:line citations. Do not diagnose any bug; just map the logic faithfully.
