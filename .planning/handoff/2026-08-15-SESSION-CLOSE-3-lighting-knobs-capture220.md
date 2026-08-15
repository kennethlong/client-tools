# 2026-08-15 — SESSION CLOSE 3: lighting compensations knobbed, Capture220 conviction, his branch at 21

**READ FIRST (latest session-close).** Continues SESSION-CLOSE-2 (same day). This session:
the queued our-side phantom fix landed, and Kenny's live cantina reports drove a two-commit
lighting arc on his branch (c20/c21) that closed the character-brightness residual —
capture-convicted, Kenny-verified "close enough for now." **Live tracker:
[.planning/SAIS-PR-QUEUE.md](../SAIS-PR-QUEUE.md) · knobs ledger:
[.planning/SAIS-KNOBS.md](../SAIS-KNOBS.md)** (new this session — feeds the PR #1 body).

## State

- **Our repo:** `master` local commits pushed-state per close-out below. New commits:
  `9283264c9` gl11 phantom COLOR-reads-white mirror (32-byte phantom buffer zeros+white,
  COLOR elements at AlignedByteOffset 16 — mirror of his c17; all four gl11 configs built,
  staged both platforms) · `4a45b895a` + `876bcad11` + `2833ba045` docs (knobs ledger + tracker).
- **His repo:** `strict-data-defaults` @ `bf4aef663`, **21 commits**, pushed, tree clean.
  c20 `52ba6fc36` ShaderSource lighting patches → opt-in knobs · c21 `bf4aef663` synthesized
  hemisphere → opt-in knob. ⚠ **c20/c21 SHAs are post message-reword force-push** (PowerShell
  here-string doubled apostrophes; content identical; c17's `''` typos left as-is — its SHA
  is referenced everywhere).
- **stage-B-x64:** exe = branch-tip (unchanged by c20/c21 — plugin-only), gl11_r.dll = c21
  build, hand-restaged. His cfg has NO strictData key (strict default) and no lighting knobs
  (stock look).

## The lighting arc (Kenny's two cantina reports → c20/c21)

1. **"Still rendering the fog"** — dispositioned CORRECT, not residual: retail's smokey haze;
   the ILM/Legends landmine turned it OFF; our merged `interior.iff` (8cd8c2d82) at priority
   12 deliberately restores it (live-verified 08-14). If no-fog is ever preferred → TRE
   cleanup manifest preference, not a defect. Do not re-report.
2. **"People look a little bright"** → **c20**: his engine runtime-patches shader source
   (`Direct3d11_ShaderSource`) with two eye-tuned DX9-x64 compensations — c_ambient
   `mov→add` (scene ambient onto baked color) and a `max(ambient+diffuse, 0.85)` floor
   injected into every stock `//hlsl` program (the dot3/skinned character family). Both now
   opt-in: `[Direct3d11] ambientBoost` (re-enabled add is COLOR-only, **alpha pinned** — his
   shape drove COLOR.a to 1.85) + `diffuseFloorPercent` (85 = his tuning). Cache-safe by
   construction (compile key hashes post-patch text; includes hashed as served).
3. **"Much better… still a touch brighter, like a shadow layer is missing" + Capture220
   pair** → **c21**: capture diff showed world at parity (floor 0.95×, wall 1.02× — c20
   pixel-verified) but characters ~1.9×. Same robe fragment, same interpolated vertex
   lighting (v1=0.24 both) → delta in PS dot3 constants → his
   `Direct3d9→11_LightManager::setExtendedLightData` **synthesizes** tangent=0.65×/back=0.30×
   key-light diffuse for lights authored WITHOUT hemispheres → +65% key light on every
   character unconditionally (shade side never falls = "missing shadows"). Stock ref 0.19,
   his 0.41. Gate: `[Direct3d11] synthesizeHemisphericLight` (default false).
   **Kenny verified: "looks really close, close enough for now" — formal side-by-side render
   test DEFERRED TO POST-MERGE.**

## Guardrails recorded (do-not-refix)

- **tangentColor "flat add" in his 4 diffuse*.inc — REFUTED as compensation**: it is the BASE
  TERM of the stock hemispheric ramp (dot=1→diffuse, 0→tangent, −1→back; constants from
  `Direct3d9_LightManager::setExtendedLightData`). Do NOT strip it.
- **0.3 ambient floor**: Sais removed it from D3D11 HIMSELF (`a98867e9d`, 07-27) — his call,
  not ours to knob. Measured impact (his own commit): authored interior ambient 0.125–0.135
  more than doubled to 0.30; his "matches what SOE shipped" comment is contradicted by that
  same measurement.
- **His D3D9 tree still carries the FULL ungated stack** (0.3 floor :562, 0.65/0.30 synthesis
  :789, ShaderSource patch family origin) and went live when c2 made his x64 D3D9 load.
  **Disposition CONFIRMED by Kenny: leave untouched — his D3D9 line is replaced wholesale by
  our gl05/06/07 ("D3D9 is ours"). No knob mirroring.** Also: his gl05 is NOT a clean visual
  reference (this stack); ours is.

## Capture-diff method notes (banked)

- Capture220 pair: ours `stage/Capture220.rdc` (Win32 gl11), his `stage-B-x64/Capture220.rdc`.
- renderdoc-cli `pixel` reports only the LAST passing writer — both clients' final writers are
  fullscreen passes (ours: gamma pre-Present; his: scene→backbuffer blit), so bound `-e` to
  the last WORLD draw to reach real geometry writers. MCP `diff_draws` aligner finds zero
  matches across the two clients (different draw naming) — use the raw per-capture rows
  (eventId/triangles/shaderHash) from its JSON instead.
- Conviction chain that worked: region-average brightness (PIL over exported PNGs) →
  pixel history for the writer eid → `debug pixel` for PS inputs → same-inputs+same-code ⇒
  constants ⇒ read the LightManager source as ground truth for what gets uploaded.

## Open board

- PR #1 reopen: refresh body for c16–c21 **+ fold in SAIS-KNOBS.md** (the "nothing deleted,
  only parked behind keys" story) → Sais review → merge.
- Post-merge: side-by-side render test (Kenny's call, deferred); TRE cleanup / ILM
  preference-kill audit (scope in SAIS-PR-QUEUE.md); corpus/data unification + ILM mount
  conversations with Sais; D3D9 swap wave (retires his D3D9 compensation stack).
- Parked engine queue unchanged (dPVS portal fixes, wearables retry, screenshot key,
  WorldSnapshot narrowing).
- Our-side perf backlog unchanged (873ms NV wait, ctor frame).
