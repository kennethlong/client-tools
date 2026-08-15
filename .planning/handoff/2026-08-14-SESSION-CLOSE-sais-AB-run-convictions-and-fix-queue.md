# 2026-08-14 — SESSION CLOSE: Sais A/B run, convictions live-verified, fix queue armed

**READ FIRST (latest session-close).** The whole Sais (Galaxies-Reborn) collaboration arc:
CONSULT-74 consult round → adversarial close → live dual-client A/B → three convictions,
two falsifications, four retractions → merge agreement #1 → fix queue. Detail lives in
`.planning/research/CONSULT-74-*.md` (EVIDENCE / SYNTHESIS / ADVERSARIAL-CLOSE / AB-RUN-PROTOCOL /
**AB-RUN-FINDINGS** = the master findings doc); pics in `CONSULT-74-pics/`. **The task board
(TaskList, #1–#11) is the live queue** — this file is context, not the todo list.

## State

- **Our repo:** `master` == `origin/master` == `f5f14c3f6`, tree clean except A/B staging.
  ⚠ `stage-x64/client.cfg` currently points at HIS dataset (`_client_dx11`, his 65-root layering
  + priority-9 loose ui layer added after the buttonEnterSpace FATAL). **Restore with
  `cp stage-x64/client.cfg.swgsource-bak stage-x64/client.cfg` before normal dev.**
- **Sais's tree:** `D:/Code/Galaxies-Reborn/client-tools` (full history, 14 branches, remote
  `sais` in our repo). Packaged squash + staged runnable client: `stage-B-x64/` (exe+DLLs rebuilt
  by us — his D3D9 plugins relinked but STILL Gl_api-mismatched 864v896, PRODUCTION fix pending).
  His A/B overrides: `stage-B-override/` = his asm2hlsl corpus (extracted from
  `origin/x64-dx11-vanilla` — his packaged repo SHIPS WITHOUT IT) + our fixes staged live:
  `datatables/interior/interior.iff` (fog), patched `c_ambient.inc` (his original =
  `.sais-orig`, ours = `.consult74-patched`, ACTIVE = ours), `texture/nebula*` (60 files).
- **Discord state:** Kenny sent the findings + cantina pics + "how much do you want me to just
  fix" — **WAITING ON SAIS'S REPLY** (sorts fix-quietly vs walk-through). Agreed already:
  **D3D9 is ours** ("go for it — i stopped messing with 9"). Plan on yes: branch on his repo,
  one fix per PR, corpus changes through HIS asm2hlsl generator. Sais owes: vanilla TRE set.

## The convictions (all instruction-level, mechanism in AB-RUN-FINDINGS)

1. **Interior whitewash** = his `c_ambient.inc` `r7 = vColor0 + ambient` (compensation for his
   broken-era data; vestigial — baked colors load fine now). **LIVE-VERIFIED**: one-line revert
   + fog file = his client renders the cantina at parity (wall 0.84→0.17). Plan: don't delete —
   **parameterize as `[Direct3d11] ambientBoost=0.0`** (Kenny's idea; color-only, PIN ALPHA —
   measured COLOR.a=1.85 inflation is a latent hazard). Roofline "lights" = same boost clipping
   trim (probed 1.0-pinned), not light objects.
2. **Missing fog + missing nebulas on his client** = ILM raw-mount landmines, BOTH classes of our
   shareable doc live-verified: class-3 preference change (interior.iff fog-off, 124 rows) and
   class-1 stub shadowing (zero-alpha textures over base → nebulas invisible). He has NEVER seen
   interior fog or nebulas on x64. Both fixed live via loose overrides.
3. **Sky/nebula oversaturation** = his emissive/additive conversion family summing >1 and
   clipping (horizon composite measured 1.12/α=2.0; nebulas same class). Task #5 = review as a
   generator RULE, using our solved e_nebula/premultiply arc as reference.

## Falsified / retracted (don't re-derive)

- c_ambient does NOT cause his naked char select (A/B'd). Naked = **stock async load race**,
  char-select path, no retry; we win it by a mile (preloads, negative cache, TOC indexing),
  he loses it (65 linear mounts, no shader cache, JUCE whole-file decode). Fix = retry like his
  in-world `verifyWornItems`; queued, not urgent for us.
- Tent/floating-parts/missing-canvas = MY mid-frame export artifact. **RULE: always take the
  last-draw eid from `renderdoc-cli draws`** before export/pixel-history (goto-probing round
  numbers silently misses the tail). Also: exported RTs carry engine alpha — flatten to RGB
  (PIL convert) or viewers composite them wrong.
- The A/B captures were CROSS-dataset (ours-on-his, his-on-ours) — accidentally STRONGER
  (each renderer correct/wrong on the OTHER's data), but the 80.5% diff number carries that
  asterisk. His `_client_dx11` derives from SWGSource v3.0 anyway.
- His device-removed handling is ALSO fatal-by-design (adversarial close E1); write-mask is an
  R↔B swap LOW not "any partial mask"; TextureFormatInfo short-table is STOCK-inherited.

## Kenny directives (fresh, 2026-08-14 late)

- **Task #11 FIRST on any shared branch: revert his FATAL-softening sweep** (~63 sites +
  NetworkHandler swallow + DataTable defaults + sentinel + CustomizationData truncation).
  Strict by default; at most one explicit strictData opt-out. Live justification: identical
  missing-ui-layer gap — ours FATAL'd actionably, his sailed silently.
- Fix his no-focus-on-startup (queue item; his Game.cpp splash/focus rework suspect).
- Space ambient sound stopped on BOTH clients (task #10f) — ours may be OUR crossfade-defer bug,
  his may be JUCE EOS; separate mechanisms, don't conflate.

## Resume point

1. Sais replies → task #11 then #7 queue (branch mechanics: collaborator vs fork).
2. Agent-runnable now: #5 (emissive/additive rule review), #8 (B→A adoptions — top item
   `e961a57c5` CustomizableShader fix OBSERVED FIRING in our log), #6 design (after reply).
3. Kenny-runnable: #3 (timed zone-in), #4 (his own-install baseline cell), #10 pile.
4. Restore our cfg from `.swgsource-bak` when A/B work pauses.

Memory updated: `project_sais_collaboration_ab_run` (new). The narrative for SWGSource: both
renderers sound; his defects = compensations for a data layer he couldn't see was broken (no
x64 D3D9 reference, raw-ILM mounts); ours match retail because of the reference-diff discipline;
combination thesis proven live in one day — each side had exactly the piece the other lacked.
