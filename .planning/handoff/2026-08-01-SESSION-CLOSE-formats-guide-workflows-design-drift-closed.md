# 2026-08-01 — SESSION CLOSE: formats guide + guided-workflows design shipped to toolkit; size drift closed; model-D consumer-closed

**Repo state:** `origin/master` = `fa346b421` (all pushed — the 3 model-D commits 2026-07-31, then the two research docs + index).
**Uncommitted (docs/planning only, no code):** this file + handoff README edits, `2026-07-31-ws-size-drift-CLOSED.md`, the 07-19 `getobjecttransformo2p-REQUEST` handoff, CONSULT-56/57/66 `.out` research files, and `stage/override/{interiorlayout,object,snapshot}/` (the toolkit's live model-D edit artifacts — decide keep-local vs commit; they are toolkit test output, lean local-only).

## 1. Model-D arc: CLOSED consumer-side (their doc 2026-07-31 11:40)

Full loop proven live end-to-end: hover → Arm (v25 `getContainingBuildingId`) → gizmo → Persist
(edited `.ilf` + derived template) → `wsSetNodeTemplateName` OK → `wsSaveSnapshot` → byte-verified
→ reload holds the edit. Same-session double-edit accumulation proven; their per-template
stock-ilf mirror shipped. Both 07-30 AWAITINGs flipped in the handoff README. Contract stays
**v25/147**. Toolkit-side leftovers (theirs): agent `-1` result-mapping fix, editor-scene verify
pass, debug-trace gating.

## 2. .ws size drift: CLOSED, no code change (`2026-07-31-ws-size-drift-CLOSED.md`)

The "~20KB per load-save cycle" was a ONE-TIME inflation on first save from stock: 325 buildout
template names interned into the OTNL (tatooine has **68 TOC-resolved v2 buildout tables** — the
idmint-era "no tatooine buildout" claim was the per-tre-scan blind spot again) + the 1 derived
toolkit name; `saveFiltered` writes the whole table by design; nodes byte-identical; crc dedupe
keys match across load/intern so cycles are idempotent (field-proven: 7/30 vs 7/31 saves differ
by exactly the 41-byte derived name). Optional future polish: OTNL garbage-collect in
saveFiltered — deliberately not done (save path is live-verified). Memory + toolkit mirror updated.

## 3. NEW: the asset formats & modding guide (`docs/research/asset-formats-and-modding-guide.md`)

The comprehensive reference for SWG-Toolkit product work, verified 2026-08-01 against engine
source + a 193,475-file census of the live sku0 TOC (six parallel research agents; prior docs
surveyed and corrected, not trusted). Contents: all 48 extensions with loader authority +
write-path status; IFF/TRE/searchPath mechanics; composition chains (6 mermaid diagrams);
world→scene breakdown (scene IS the terrain file; 4 content layers; POB-CRC and CSTB identity
gates); the original SOE pipeline (incl. god-mode-vs-SwgGodClient distinction); per-format
modern editing matrix; composite-edit recipes (§9, model-D as the interior example); **§10 SOE
editor inventory + prioritization** (P1 TemplateCompiler+CSTB → P2 buildout lift → P3 .cdf →
P4 FX suite → P5 god-client UX organs → P6 terrain/Turf) + **§10.3 toolkit-vs-field capability
matrix and lift-and-shift shortlist**; §11 pitfalls crib; §12 corrections to older docs.
Key discoveries: engine libraries already round-trip 11 formats (wrap, don't rewrite);
`SwgGodClient_o.exe` EXISTS in the `D:\Code\swg-client\exe\win32` era drop (revival verdict:
don't — Perforce strip is ~1 day but Qt 3.3.4 is a multi-week wall; run the era exe once as UX
archaeology); `.qst` is authoring-only XML (emit `.tab`+CSTB directly); CSTB generators are lost
Perl — must self-emit.

## 4. NEW: guided-workflows + AI layer design (`docs/research/toolkit-guided-workflows-and-ai-layer.md`)

Kenny's product direction: identify common mods → make the flows guided wizards → optionally let
a user-supplied AI subscription DRIVE the wizard, stopping where a human must act. Design landed
(Kenny-directed framing): a **supplemental Guided Workflow layer on top of the toolkit's planned
Phase-8 MCP server** — AI-01/AI-02 and `docs/09-ai-mcp/ai-and-mcp-integration.md` verified and
kept unchanged. Stack: services → MCP (as planned) → declarative flow engine (typed steps,
validation gates, **confirm boundaries shared by human and AI**, render hints per their settled
overlay-vs-app boundary rule) → four consumers: UI wizard (zero AI), external MCP agents
(Claude Code/Cursor/Copilot — the multi-provider answer with no provider SDKs; `workflow.confirm`
routes approval through the toolkit UI so agents can't self-approve), optional embedded
Tool Runner agent (BYO Anthropic key via safeStorage or detected `ant auth login` OAuth profile —
**subscription-token licensing check still open**), and AI-02 spot assists.
First real demand data gathered (web agents): MTG census (#1 download is SIE the TOOL; 340-thread
request backlog; texture/UI mods the biggest walked paths), **per-server policy is first-class**
(Legends sanctions modding via launcher Mod Manager but bans terrain/footsteps/collision/
animation/interiors; Restoration bans client mods) → policy checker is a built-in gate + a W1
"packaging + policy" wizard. Wizard order: W1 texture-reskin + packaging/policy → W2 UI-theme,
appearance-swap, sound → W3 decoration (=sketch 021 retrofit), new-item (needs P1), creature →
W4 building/buildout/server-scaffolding. Housing-decoration FILE mods refuted as a category
(the live editor is the answer). Claude-api skill facts captured in-session (Tool Runner vs
Agent SDK vs Managed Agents; OAuth profile resolution) — reload the skill before writing AI code.

## 5. Handoff package delivered to SWG-Toolkit (their `.planning/handoff/`)

Three files: `2026-08-01-PROVIDER-HANDOFF-modding-research-docs.md` (provenance + adoption
guidance + the **sequencing recommendation: workflow engine directly after their Phase 5.1,
BEFORE Phase 6 Blender Bridge** — adjacency to sketch-021, demand center, DCC audience already
served by the standalone plugin, dependency inversion into Phase 8, low new-surface cost, policy
timeliness; re-order not cancellation) + full copies of both docs as `PROVIDER-DOC-*` /
`PROVIDER-NOTE-*`. **AWAITING: toolkit adoption + their roadmap-reorder call.**

## 6. Carried-forward backlog (untouched this session)

- **Perf thread (parked since 07-19):** gl05 `logDynamicBufferLockMs` probe still ARMED in
  `stage/client.cfg`; 815ms skeletal first-draw VB-lock stall cold-boot repro pending (suspect:
  DISCARD wrap on the 2MB ring). Driver-threading soak call; probe/diag strip pass.
- Optional: one era-exe session (`SwgGodClient_o.exe` vs dev server) as P2/P5 UX archaeology.
- Toolkit-side open items noted in the design doc: OAuth licensing check, maintained per-server
  policy data file, community personas survey.

## 7. Session gotchas worth keeping

- Grep tool output can DISPLAY backslash artifacts that aren't in the file — Read exact lines
  before "fixing" (the mermaid `<br\>` false alarm; the real cause of gray boxes = viewer
  without mermaid support; repo convention offers HTML twins with CDN mermaid).
- The TOC blind spot claimed its third victim (buildout tables). Any "what does the client load"
  claim MUST walk `read_search_toc_entries`.
- Agent research reports include premise corrections — trust them (no WaterShaderTemplate,
  `.mkr`=MKAT appearance, `.pln`=planet billboard, `.mp/.mpb` never existed).
