---
created: 2026-06-12
resolves_phase: 28
tags: [tre-tool, architecture, repo-structure]
---

# TRE editor: build in its own directory, completely separate

Kenny (2026-06-12, during Phase 24 planning): the TRE editor/tool (Phases 28–30)
should be built in its own directory and **completely separate** from the engine
build — own solution/project, no coupling into the SwgClient build graph — so it
can be moved into another repository at some point.

Implications for Phases 28–30 planning:
- Own top-level directory (e.g. `tools/tre-editor/` or a sibling dir), own
  build files; not wired into the engine .sln/vcxproj dependency graph.
- Minimize/eliminate engine-library dependencies; anything shared should be
  vendored or copied, not linked across the repo boundary.
- Precedent: `swg_blender` was moved out to `D:/Code/swg-tools` — same
  extraction path is anticipated here.
