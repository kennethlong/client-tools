# SESSION CLOSE 9 (2026-08-18) — ARCHIVE POINT: both PRs MERGED, active dev moved to swg-source-x64-dx11

**This is the final session-close of swg-client-v2 as the working codebase.** The repo is
now the frozen archive (kept on disk + pushed to kennethlong/client-tools). Active
development continues in **`D:\Code\swg-source-x64-dx11`** (fresh clone of the merged
Galaxies-Reborn repo), which has its own handoff chain starting at
`.planning/handoff/2026-08-18-BOOTSTRAP-transition-from-v2.md` (local-only there).

## 1. The trigger — the merge landed

Sais merged everything on 2026-08-18 (~12:21 UTC), **silently — zero comments, zero review
remarks** on any PR:

- **PR #2** — the 54-commit deliverable ("The A/B fix queue: strict data, crash fixes,
  DX11+D3D9 render parity, and stock-dataset self-sufficiency"), merge `8a9021322`.
- **PR #3** — "The toolkit advertise surface: GetEngineHookPoints v35/165 + the Gl_api
  overlay callbacks", merge `c9378aed4`.
- He then created and merged his own **#4/#5** branch-sweep merges; master tip `6a62e4792`.

The covert-mode arc, the one-giant-PR mandate, the stock-acceptance bar — all closed by
full acceptance. Next joint step: the SWGSource upstream conversation.

## 2. Transition executed (the CLOSE-8-era plan, item by item)

1. **Claude memory migrated**: all 103 files (incl. the new
   `project_codebase_b_working_repo` anchor + merge-state updates to the Sais and
   transition memories) copied to `C:\Users\kenne\.claude\projects\D--Code-swg-source-x64-dx11\memory\`.
   Nothing pruned — the anchor memory explains which entries describe archive mechanics.
2. **Personal layer ported to B**: `AGENTS.md` (B-adapted manual: Build-Client.ps1 flow,
   renderer/cfg safety, RenderDoc, key paths, conventions), `CLAUDE.md` (phone-a-friend
   crew, CONSULT numbering continues from 78), `.claude/` (GSD install, hooks,
   settings.json with `$env:MSBUILD`, settings.local.json), `.mcp.json`. All added to B's
   `.git/info/exclude` (machine-local; AGENTS.md = candidate docs contribution later).
3. **B bootstrap handoff written** (open board inherited there).
4. **This file** = the archive point.

## 3. Map of what stays in this repo (the archive holds these)

- `.planning/handoff/` — the full session-close chain (CLOSE-1..9).
- `.planning/research/` — CONSULT-1..78 (74 = A/B findings master doc, 76 = TRE builder
  M0 + design, 77 = endianness round, 78 = design reviews), UPSTREAM-OFFERING-plan
  (parked), TRE-BUILDER-DESIGN-draft.
- `tools/tre-compare/` and `tools/tre-lint/` (committed `52ea9f182` with all 8 baselines —
  baselines decided IN git as the regression reference; note the repo-global `.gitignore`
  `package.json` rule required a force-add of the tool's package.json).
- `docs/FORK-CHANGES.md`, `.planning/SAIS-PR-QUEUE.md` (post-merge TRE cleanup scope),
  capture evidence references; probe artifacts stay parked in stage-B-x64/stage-x64
  (toolkit's granted vendoring source + the re-run kit).
- `stage/` and `stage-x64/` remain the A-side runnables (gitignored, machine-local).

## 4. Still-open items (tracked forward in B's bootstrap handoff)

SWGSource upstream conversation · toolkit compose phase (we owe cohort seed rows +
Kenny's 102-pair fixture-vendoring ruling) · c_ambient-class gl11 fix + ILM re-baseline
(now B-repo work) · ILM sweep awaiting `tresmith`. (The archive commits landed same day:
`52ea9f182` tre-lint + `414f67362` docs.)
