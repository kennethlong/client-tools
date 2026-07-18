# 2026-07-18 session close — Goal B Waves 1-3 COMPLETE (v19/140), all pushed, all gates green

**READ FIRST after restart.** Context-clear checkpoint for the whole Goal B snapshot-editor arc
(2026-07-13 consult → 2026-07-18 Wave-3 gate closure). `origin/master` = `dfb9639d4`. Working tree
clean except the long-parked CONSULT-56/57/66 `.out` files (intentional, unchanged).

## 1. Where the arc stands

The Utinni snapshot editor — the last SWGEmu-only editor — is fully unlocked on the advertised
client, provider-side DONE:

| Wave | Contract | Landed | Status |
|------|----------|--------|--------|
| 1 — read/browse | v16→17, 128 names | `fb32e1c64` | consumer live-smoke PASSED (5449/5449 naboo) |
| 2 — live mutation | v17→18, 133 | `85877bae4` + fixes below | consumer smoke PASSED (add/remove/duplicate/radius/undo/redo) |
| 3 — persistence + riders | v18→19, **140** | `fd06a2ad6` + `25c8c8f35` | provider gates green incl. LIVE save (result=0 both planets, lossless round-trip); **consumer bind+smoke = the open item** |

Wave-2/3 same-day debug rounds (all resolved, each with its own handoff):
- **id-mint refusal** → TOC-layer snapshots carry authored server-range ids (the NGE collection
  items) → seed hardening `d7dba07a6`, field-verified. Diagnostics (`[editor.ws]` lines) + the
  allocator discriminator are PERMANENT standing tools.
- **occupancy guard** → bidirectional (contents walk + parent-cell sweep) `835ad389c`; the
  blind-spot≡blast-radius symmetry explains why the pre-fix delete was provably harmless.
- **buildout provenance** → the save self-test caught the id tripwire refusing every
  buildout-planet save (positive v2 objids are NORMAL, TOC-indexed tables exist for regular
  planets) → identity-keyed provenance `25c8c8f35`; save enum code 5 RESERVED; the Wave-2
  wsAddNodeAt buildout-set refusal formally amended (flagged in the Wave-3 handback §6).
- **positionAndRotationChanged row request** → NO ROW: the advertised `setTransform_o2w` already
  fires the notify internally; the consumer's no-op guard is the permanent correct config.

## 2. Standing tools / keys (all default-off, all in-tree permanently)

- `[editor.ws]` refusal/OK diagnostics on every mutation + save shim (SwgClient_report.log).
- `wsAllocateIdRange` discriminator (seed/walk/collision-predicate dump on refusal).
- `[ClientGame/WorldSnapshot] wsSelfTestSaveOnLoad=1` — one REAL save at parse completion, typed
  result logged. Used twice today; caught a shipped bug on first flight. Strip after use; delete
  any `stage/override/snapshot/*.ws` artifacts it writes (they shadow TOC copies).
- Wave-1 probes: `[ClientGraphics] logTextureCreates` (TEXCREATE) + the stall stack sampler remain
  the perf-arc standing tools (unrelated to Goal B, still armed).

## 3. AWAITING (consumer side — nothing pending ours)

1. Utinni v19 re-sync (sha256s in the Wave-3 HANDBACK §4) + bind + their smoke: the
   Save→edit→Save→Unload→Reload survival cycle, delete-from-inside re-test (expect `-1
   OCCUPIED (parent-cell)` — their §3a repro predated the fix), target-gate + gizmo passes.
2. They must read the Wave-3 HANDBACK **§6 addendum** (identity provenance, enum code 5 reserved,
   wsAddNodeAt amendment) and the positionchanged ANSWER before binding.
3. Pre-approved for the NEXT version bump, whenever occasioned: `cuiRadialMenuManager::clear`
   (public static, .h:47). No v20 minted for it alone.
4. Then arc close-out (probe/diag review is NOT needed — Goal B diagnostics are permanent by
   consumer request).

## 4. Lessons that must survive the context clear

- **TOC blind spot (bit TWICE today):** per-tre index scans see NOTHING indexed by
  `skuN_client.toc` — snapshots AND buildout tables. Memory written
  (`reference_toc_layer_blind_spot_tre_scans`): a per-tre absence claim is evidence of nothing
  until the TOC layer is walked (`tre_reader.read_search_toc_*`).
- **Exercise the real path in the gate.** The Wave-2 gate never ran a real add → the id-mint bug
  shipped. The Wave-3 self-test key ran a real save → caught the tripwire bug pre-handback. The
  consumer codified this in their §5 mechanics; keep honoring it.
- **Crossed wires are real:** consumer request docs can predate my latest handoffs (twice today —
  provenance rider + occupancy §4D). On any incoming doc, check its claims against the newest
  handoffs BEFORE implementing what it asks.
- **PS 5.1 traps this session:** embedded double quotes in `git commit -m @'...'@` here-strings
  break native arg passing (use `git commit -F <file>`); `2>&1`/stderr exit-255 quirk on
  clean-running git commands persists — read the output, not the exit code.
- Contract conventions now settled: contract names mirror ENGINE names; frozen enums are
  append-only with codes retireable-to-RESERVED; provenance is identity-keyed, never id-keyed.

## 5. Carried backlog (untouched today, from the 07-13 handoff)

1. D3D11 perf arc: GroundScene-ctor mega frame (~560ms pre-screen), gl05 bytecode-cache sibling
   (two convicted classes), preloadSomeAssets single-item overshoot, driver-threading soak call
   (Kenny) → ConfigDirect3d11 default flip, probe strip pass (PortalCullProbe chatty; TEXCREATE +
   sampler + editor.ws are standing, NOT strip candidates).
2. Utinni consumer-side carries: real-door trigger brittleness, ilm-extract audit, Debug-config
   5-target refresh.
3. Parked CONSULT-56/57/66 `.out` files in `.planning/research/` (still untracked, still fine).
