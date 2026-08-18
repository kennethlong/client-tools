# SESSION CLOSE 7 (2026-08-17 early) — BOTH PRs OPEN · stock acceptance closed · TRE-builder M0 PASSED

The biggest single session of the collaboration. Read this file, then SESSION-CLOSE-6 (same
night, earlier half) only if a detail is missing here. Trackers current: SAIS-PR-QUEUE.md,
memory topic files, CONSULT-76 research set.

## ⭐ NEXT SESSION OPENS WITH (Kenny, at close): where does the TRE builder tool LIVE?
Kenny's opening position: **inside SWG-Toolkit, as a TRE builder with a built-in LINTER.**
Bring to that discussion:
- The standing constraint (memory `project_tre_editor_standalone_extractable`): TRE tooling
  must be standalone/extractable — reconcile with a toolkit home (a toolkit HOST around an
  extractable core library is the likely shape; tools/tre-compare is the precedent: standalone
  uv package, zero engine imports).
- The "linter" framing maps beautifully onto the crew design: Opus's validation gates
  (IFF size-fit walk, cohort consistency, dangling refs, regression-vs-baseline) ARE a linter
  — they can ship as a standalone lint pass long before the composer exists.
- SWG-Toolkit is the consumers' repo (their C# editor + our advertised contract); the builder
  seed is Python. Integration surface / language / who owns releases = the actual decision.
- All design inputs ready: CONSULT-76-SYNTHESIS-tre-builder.md (v1 cut-line, build order),
  CONSULT-76-M0-RESULTS.md (confirmed format spec), CONSULT-76-M0-build_treexp.py (working
  writer seed), Codex writer spec + Cursor byte maps + Sonnet recipe/ecosystem + Opus
  composition semantics (all in .planning/research/).

## State of the world (all pushed, all gated)
1. **Deliverable PR = GitHub #2** on Galaxies-Reborn/swg-source-x64-dx11 (branch
   strict-data-defaults, 54 commits, OPEN — GitHub refused reopening old #1 after the
   historical force-push; #1 carries a superseded-by comment). Body LEADS with "It runs the
   stock dataset, as shipped" + compensation-removal notes. COVERT MODE OVER; awaiting Sais.
2. **Advertise PR = GitHub #3** (branch toolkit-advertise, 2 commits, stacked on #2, OPEN):
   `da91bbaa1` Gl_api v35 tail slots (his gl11 implements frame/resize callbacks natively in
   Direct3d11_SwapChain; D3D9 accept-and-ignore; sizeof(Gl_api) changed → matched 6-binary
   set ONLY, deployed to stage-B-x64) + `67b109785` the full advertise surface (v35/165).
   Gates ALL GREEN: build clean, hookpoints-probe PASS v35/165/0 nulls, **Kenny boot smoke
   PASSED on DX11 AND D3D9**. Remaining: Sais review + toolkit-side x64 consumer work.
3. **Stock acceptance CLOSED both renderers** (the joint-upstream bar): c53 `5232bb632`
   embedded corpus (stock ships 224 asm programs; 221 embedded, includes INLINED — stock TREs
   shadow modules/*.inc with ASM copies) + c54 `8a9021322` c_ambient stock cell semantics
   (RenderDoc Capture240 conviction: corpus added white ambient over the bake → every interior
   washed; bake IS the lighting). Verified: login, char select, cantina-matches-DX9, NPCs,
   Theed water, space+nebulas, on gl11 AND gl05.
4. **TRE-builder M0 decisive experiment PASSED** (see the ⭐ section; full table in
   CONSULT-76-M0-RESULTS.md). Keys: on-disk magic EERT/5000 (BE tags written LE); engine CRC
   replicated + verified vs stock TOCs; 24B TOC sorted (crc, stricmp); MD5 tail OPTIONAL;
   zero-length entry = REAL runtime tombstone (silenced the login theme); **priority
   comparator = pure numeric across node types** (tre@11 beat loose@9). Bonus: unreadable
   baked-cache includes DISABLE his compiled-shader cache (composed sets ⇒ expect re-bake).
   Verification channel that finally worked: the shader-substitution log line as a binary
   detector (two visual-target attempts missed: pre-login logos configured out; pedestal
   textures unused by this client's screens).

## Cfg / install state
- stage-B-x64 + stage-x64 live cfgs RESTORED (BOM-verified). Parked for re-runs:
  client-stock.cfg (both dirs), client-treexp.cfg, treexp_a/b.tre, treexp_loose/,
  client-live.cfg.bak-stockrun/.bak-treexp backups.
- stage-B-x64 binaries = the PR #3 matched set (superset of #2; NEVER mix with pre-#3 DLLs).

## Open threads (priority order)
1. Sais review of #2/#3; then the joint SWGSource upstream conversation (the unified client
   IS the deliverable — strategic frame in memory; never claim "first x64").
2. TRE builder home discussion (⭐ above) → then v1 per the synthesis build order.
3. A-side (swg-client-v2) owes: the c_ambient-class fix (our gl11 washes stock interiors the
   same way — "black walls full ambient" is the same compensation class), then RE-BASELINE
   every ILM "brightening" claim on the fixed renderer (rule recorded in the ILM saga memory).
4. Fix-queue candidates catalogued during the #3 port (his tree): Os.cpp x64 keyCode
   extraction (REAL Release input bug) + menu/ShellExecute pointer-width, ClientMain
   timeBeginPeriod(1) + bootTrace, DebugHelp minidump-reserve, CreatureObject naked-NPC
   wearables retry, ClientWorld logCellAtPosition probe.
5. At PR-merge time: the codebase-B transition checklist (memory
   `project_postmerge_transition_to_codebase_b` — migrate path-keyed Claude memory, port
   gitignored personal layer, archive-point handoff; v2 kept frozen as archive).
6. Passive: ambientBoost knob is INOPERATIVE (flagged in #2 body — reimplement-as-macro
   question for Sais); corpus-in-client-assets regeneration recommendation.

## Gotchas re-learned tonight
- The tool JSON layer eats one backslash level in heredoc'd Python — `\n` in a tool call
  arrives as a real newline inside string literals (bit us twice: SwapChain REPORT_LOG,
  ExitChain marker). Build such strings from chr() or extract from source files.
- gh pr reopen is PERMANENTLY refused after any post-close force-push of the head branch —
  open a fresh PR, comment the old one.
- Report-log run slicing: metrics lines mark run ENDS, not starts — slice between the last
  two end markers.
- PS 5.1 + gh piping: `| Select-Object -First N` closes the pipe and makes gh exit 255 —
  cosmetic, but check the actual edit landed.
