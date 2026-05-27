# Phase 16: v2.1 Tech-Debt Cleanup - Context

**Gathered:** 2026-05-26
**Status:** Ready for planning

<domain>
## Phase Boundary

Close the **non-blocking tech debt** surfaced by the v2.1 milestone audit
(`.planning/v2.1-MILESTONE-AUDIT.md`) so v2.1 completes clean. This is a
**cosmetic/correctness cleanup — no product behavior changes**. Three targets:

1. **SwgGodClient latent `989crypt.lib` LNK1181** — the token lingers in
   `SwgGodClient.vcxproj` `AdditionalDependencies` but the backing lib was
   deleted with `stationapi/` in Phase 12. SwgGodClient is **pre-broken on Qt
   (MSB8066)** and **out of the `/t:SwgClient` build closure**, so it never
   blocked a DECRUFT gate — but the dead token (and any siblings) should go.
2. **Inert `lcdui\lib` `/LIBPATH` segments** in 4 editor vcxproj
   (AnimationEditor / ClientEffectEditor / LightningEditor / ParticleEditor).
   **Already clean in the current tree (0 hits)** — see D-04; this is now a
   verify-only item, not an edit.
3. **Cosmetic source residue** — the dead-store `finalUrl` block in
   `SwgCuiHudAction.cpp` (~:1170-1189, browser CS-action de-wire leftover) and
   the dead `ms_speakerVolume`/`ms_micVolume` voice-volume API in
   `CuiPreferences.cpp` (orphaned by the Phase 14 Vivox voice-UI delete).

**Invariant (preserved):** SwgClient still builds clean (Debug+Release
link-grep `unresolved external symbol` == 0) and boots to character select
after the cleanup.

**Out of scope (deferred, NOT this phase):**
- AR-15-01 future-TCG-revival re-evaluation (future v2.x)
- `stage/client_d.cfg` accumulated-test-settings cleanup (coupled to v2.2 visual-parity close)
- Nyquist-coverage finalisation for Phases 12 & 13 (do via `/gsd:validate-phase 12` + `13`, not a code phase)
- Audit doc-staleness items — 12-VERIFICATION.md stale score line + 13-SUMMARY.md empty `requirements_completed` frontmatter (explicitly left alone per D-05)

</domain>

<decisions>
## Implementation Decisions

### 989crypt.lib resolution (Target 1)
- **D-01:** **Unlink, do NOT restore.** Remove the dead `989crypt.lib` token
  from `SwgGodClient.vcxproj` — never re-add the deleted `stationapi/` lib
  (restoring would contradict the Decruft milestone).
- **D-02:** **Sweep, not surgical.** Don't stop at `989crypt.lib` — audit
  SwgGodClient's full `AdditionalDependencies` across **all 3 configs**
  (Debug / Release / Optimized) for **any other token whose backing lib was
  deleted during v2.1 (Phases 12–15)**: `stationapi`/`989crypt`, `lcdui`,
  `VideoCapture`, `vivox*`/`vivoxSharedWrapper`, `libMozilla`/`xpcom`/`xul`.
  Remove every dangling one.
  - **KEEP-list guard:** the soePlatform tokens present in the SwgGodClient
    dep list (`Base.lib`, `ChatAPI.lib`, `ChatMono.lib`, `CommodityAPI.lib`,
    `monapi.lib`, `Network.lib`, `rdp.lib`, `TcpLibrary.lib`) are **preserved**
    — `soePlatform/libs/` survived Phase 14 (the `Win32-Debug`/`Win32-Release`
    `/LIBPATH` is intact). Only `989crypt.lib` (stationapi) is known-dead in
    that cluster. Do not over-remove.
- **D-03:** **Grep-only verification for SwgGodClient.** The tool cannot be
  built (Qt MSB8066, out of `/t:SwgClient` closure), so the fix is verified by
  `grep == 0` for the removed tokens across the 3 configs — **do not attempt to
  build SwgGodClient**.

### Phase scope (Target 2 + doc-staleness)
- **D-04:** **Target 2 is a verify-only no-op.** All 4 editor vcxproj
  (AnimationEditor / ClientEffectEditor / LightningEditor / ParticleEditor) already
  contain **0** `lcdui` references — the segments were swept by Phase 15's "A1
  lcdui P12-residue cleanup" (15-04), which landed after the 2026-05-26 audit
  that spawned this phase. Record `lcdui-editor-LIBPATH == 0` as a confirmed
  verification step; **no edit required**.
- **D-05:** **No doc-staleness work.** Leave the audit's doc-only items
  (12-VERIFICATION.md stale "14/15 pending" score line; 13-SUMMARY.md empty
  `requirements_completed` frontmatter) **out of scope** — they don't fit this
  phase's stated code-cleanup goal and carry no build/behavior impact.

### Source-residue removal depth (Target 3)
- **D-06:** **Full removal, both files.**
  - **`SwgCuiHudAction.cpp`** (~:1170-1189): delete the entire dead `finalUrl`
    block — it's dead because the trailing `ShellExecute(...)` at :1189 is
    commented out, so `finalUrl` is computed (incl. the `if (finalUrl.length()
    > 2048)` truncation branch) and never consumed. Remove the whole dead
    computation, not just the assignment.
  - **`CuiPreferences`**: remove the **full** speaker/mic volume API — the 2
    statics `ms_speakerVolume`/`ms_micVolume` (`.cpp:397-398`) **plus** all 4
    accessor methods `get/setSpeakerVolume`, `get/setMicVolume`
    (`.cpp:3460-3484`) **plus** their 4 header declarations (`.h:553-557`).
    Scout confirmed **zero external callers** of these accessors — clean
    full-API removal.
- **D-07:** **CR-01 ordinal lesson does NOT apply here.** The Phase 14 CR-01
  rule (deletions from positional enums/tables mirroring retail-TRE rows need
  ordinal-preserving placeholders) is **irrelevant** to D-06 — these are plain
  `float` statics + accessors, not a positional enum/datatable row. Do not
  introduce placeholders; delete outright.

### Verification bar
- **D-08:** **Link-grep + single char-select smoke** (not the full
  dual-renderer matrix). The source-residue edits (D-06) live in
  SwgClient-linked libs (`clientUserInterface`, `swgClientUserInterface`), so:
  1. SwgClient **Debug + Release** link-grep `unresolved external symbol` == 0
     (**Optimized config EXEMPT** — pre-existing SAFESEH LNK1281 per DEF-14-01).
  2. Boot **once** to character select under `rasterMajor=11` (D3D11).
  This is a no-behavior-change phase; one boot confirms no link/init regression
  without re-running the full `rasterMajor=5` AND `=11` matrix used for the
  prior (riskier) Decruft removals.

### Claude's Discretion
- Commit granularity — the three targets are independent (SwgGodClient vcxproj,
  HudAction.cpp, CuiPreferences.cpp/.h are non-overlapping files); plan as the
  cleanest atomic-commit split.
- Whether the Target-2 verify step is its own plan step or folded into the
  build/verification gate.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase source / authority
- `.planning/v2.1-MILESTONE-AUDIT.md` — **the source of this phase.** The
  `tech_debt` frontmatter list enumerates every item; §"Tech Debt Summary"
  gives the headline set. MUST read — it is the definition of done for Phase 16.
- `.planning/ROADMAP.md` §"Phase 16: v2.1 Tech-Debt Cleanup" — the locked phase
  goal + explicit out-of-scope list.
- `.planning/PROJECT.md` §"Current Milestone: v2.1 — Decruft" + Evolution
  footer — v2.1 is functionally COMPLETE (DECRUFT-01..07); Phase 16 is post-hoc
  cleanup so the milestone closes clean.

### Target files (the actual residue)
- `src/game/client/application/SwgGodClient/build/win32/SwgGodClient.vcxproj` —
  `989crypt.lib` token at `<AdditionalDependencies>` (line ~99, Debug `<Link>`);
  sweep all 3 configs (Target 1).
- `src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiHudAction.cpp`
  — dead `finalUrl` block ~:1170-1189 (Target 3a).
- `src/engine/client/library/clientUserInterface/src/shared/core/CuiPreferences.cpp`
  — `ms_speakerVolume`/`ms_micVolume` statics (:397-398) + accessor defs
  (:3460-3484) (Target 3b).
- `src/engine/client/library/clientUserInterface/src/shared/core/CuiPreferences.h`
  — accessor declarations (:553-557) (Target 3b).
- 4 editor vcxproj under `src/engine/client/application/{AnimationEditor,
  ClientEffectEditor,LightningEditor,ParticleEditor}/build/win32/*.vcxproj` —
  Target 2 verify-only (already 0 `lcdui` hits).

### Cross-phase lessons that constrain this phase
- Memory `project_decruft_removal_build_graph_gotchas` — `/FORCE` masks
  unresolved externals (grep for the symbol, not exit 0); msbuild full-path +
  `/nodeReuse:false`; delete exe to force relink; run from PowerShell.
- Memory `feedback_debug_exe_reads_client_d_cfg` — set `rasterMajor=11` in
  `stage/client_d.cfg` (NOT `client.cfg`) for the Debug-exe boot smoke.
- Memory `project_optimized_config_safeseh_pre_existing` (DEF-14-01) — Optimized
  config LNK1281 SAFESEH is pre-existing; EXEMPT from the link gate; validate
  removals via Debug+Release only.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets / Established Patterns
- **Per-target atomic-commit + grep-gate pattern** from Phases 12–15 applies
  directly: edit → repo-wide `rg` for the removed token == 0 → Debug+Release
  link-grep == 0. This phase is a smaller instance of the same discipline.
- **soePlatform `/LIBPATH` is KEEP-listed** (Phase 14): `Base.lib`/`ChatAPI.lib`/etc.
  in SwgGodClient deps are valid — only `989crypt.lib` (stationapi) is dead in
  that cluster. The sweep (D-02) must distinguish stationapi-dead from
  soePlatform-alive.

### Integration Points
- Source edits (D-06) are in `clientUserInterface` + `swgClientUserInterface`,
  both linked into `SwgClient` → covered by the D-08 link-grep + boot smoke.
- vcxproj edits (D-01/D-02 SwgGodClient; D-04 editors) are **out of the
  `/t:SwgClient` closure** → grep-verified only, do not build those projects.

### Known false-positive guard
- `989crypt.lib` was noted as "token present" in the integration check's
  KEEP-list observation — that observation was just recording the dead token's
  presence, NOT marking it as a KEEP asset. It IS the thing to remove (D-01).

</code_context>

<specifics>
## Specific Ideas

- The `finalUrl` block is dead specifically because its consuming
  `ShellExecute(...)` line is commented out (`//ShellExecute(... finalUrl ...)`
  at SwgCuiHudAction.cpp:1189) — a browser CS-action de-wire leftover from the
  Phase 15 XPCOM removal. Removing the block is removing now-unreachable
  computation, not changing any live behavior.
- The volume API has zero callers because the Phase 14 Vivox voice-UI delete
  removed every consumer; the accessors + statics were left orphaned.

</specifics>

<deferred>
## Deferred Ideas

- **Audit doc-staleness fixes** (12-VERIFICATION.md stale score line;
  13-SUMMARY.md empty frontmatter) — reviewed, kept out of scope per D-05.
  Cheap doc-only follow-ups if the operator wants them later; no build impact.
- **Nyquist finalisation for Phases 12 & 13** — `status: draft` VALIDATION.md;
  flip via `/gsd:validate-phase 12` + `13` (doc pass, not a code phase).
- **`stage/client_d.cfg` accumulated-test-settings cleanup** — coupled to the
  v2.2 visual-parity close (memory `project_stage_client_d_cfg_cleanup_todo`).
- **AR-15-01 future-TCG-revival re-evaluation** — accepted risk; a future TCG
  phase must re-evaluate the navigate-callback severance before relinking.

### Reviewed Todos (not folded)
- `2026-05-15-cantina-corner-snap-engine-improvement.md` — rendering/collision
  engine quirk; off-domain for a dead-code cleanup. Known pre-existing SOE
  engine quirk (memory `project_cantina_corner_snap_engine_quirk`). Not folded.
- `2026-05-15-swgsource-vs-whitengold-tre-asset-diff.md` — TRE asset / space
  graphics research; v2.2-era research scope, off-domain. Not folded.

</deferred>

---

*Phase: 16-v2-1-tech-debt-cleanup*
*Context gathered: 2026-05-26*
