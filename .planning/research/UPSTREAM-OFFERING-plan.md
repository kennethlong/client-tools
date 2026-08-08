# Upstream offering plan — presenting the change set back to SWG-Source

**Started 2026-08-07 (evening). ⏸ PARKED 2026-08-08 (Kenny's call): DO NOT approach SWGSource
until the SWG-Toolkit editor MVP is complete.** The editor is THE key to getting their attention
— it is the one thing nobody else in the ecosystem has — with modernization, x64, D3D11 and
performance as the supporting pillars. A pitch led by a demo-able in-client world editor lands
categorically harder than a pitch led by fixes, so the offering waits for it. **Re-arm trigger:
toolkit editor MVP done** (their side; exterior `.ws`-node editing is their next phase — watch
their handoff README). Everything below is current as of parking and ready to execute when
re-armed.

Goal: offer our comprehensive change set back to the SWGSource owners, with the advantages framed
in terms meaningful to THEM (server-community operators who distribute a client to players), plus
a sorted, provenance-clean presentation of the data/TRE-side changes. This file is the working
plan; the outward-facing prospectus gets drafted from it.

---

## 1. The change set, measured (v2 base `479d35df3` → `bc235ed91`, 2026-05-09 → 2026-08-07)

- **~380 engine/code commits** (of 1,017 total ours; the rest are planning/docs), **2,722 files**,
  **+53,903 / −448,771 lines** under `src/`.
- The −424k concentrates in `external/3rd/library` = **D3DX removal + dead-library decruft +
  Miles re-vendor** — the tree got dramatically SMALLER and more auditable, not bigger.
- **+57 new files = the entire Direct3d11 renderer** (`gl11`), the single biggest addition.
- Two-way flow already proven: upstream PR #18 (entertainer captcha) merged INTO this trunk;
  `master` is a live upstream-integration branch that syncs FROM SWG-Source — **our diff is
  maintained against their moving master, not a stale fork point.** That is itself a selling point:
  the offering is mergeable by construction.

## 2. The advantages, framed for SWGSource owners

Ordered by what a server community operator actually feels. Each theme gets a one-line
"why you care" that leads the eventual prospectus section.

### 2.1 x64 parity for the SWGSource tree itself (Phase 33)
*Why they care: Legends and Restoration both field x64 clients — SWGSource's own tree doesn't.
Every community building on SWGSource is stuck at the 32-bit 2GB ceiling (the #1 veteran-server
crash class: long sessions, city density, mod-heavy installs) while the walled gardens moved on.
This offering closes that gap for EVERYONE downstream of SWG-Source, and does it D3DX-free
(Restoration's x64 still carries D3DX).*
**CLAIM DISCIPLINE (Kenny, 2026-08-08): we are NOT the first x64 SWG client — Legends and
Restoration both have one. Never claim "first x64." The x64 claims that ARE ours: x64 for the
open SWGSource lineage, D3DX-free, mergeable.** Also: `/LARGEADDRESSAWARE` for the 32-bit Debug
config, and the x64 build is proven NOT impacted by the known 32-bit memory-leak class. Full
dual-platform staging discipline (Win32 + x64 built and verified side-by-side all summer).

### 2.2 A native Direct3D 11 renderer (gl11) at visual parity
*Why they care: the 2004 D3D9 path runs on modern Windows through emulation-era driver code;
every new NVIDIA/AMD driver release is a fresh risk they can't fix. A native D3D11 backend is
the future-proofing story — and it's RenderDoc-debuggable, so render bugs become diagnosable.*
Parity catalogue is deep and documented: FFP combiner cascade, multi-texture slot remap,
dot3/alpha-fade register packing, depth buffer, additive-UI premultiply, gamma pre-Present,
minimap, embed-resize, device-loss/alt-tab, skinning fixes. Renderer selection stays the stock
`rasterMajor` mechanism — D3D9 (gl05/06/07) remains fully supported and actively maintained.

### 2.3 Modern toolchain, zero legacy-SDK dependencies
*Why they care: distribution. D3DX removal means no d3dx9_xx.dll / DirectX-web-setup step for
players; C++20/VS-current means the code builds on toolchains people actually have in 2026.*
Builds clean on VS 2026 (v145) — this completes and extends the Koogie MSVC-CPP20 base both
projects share lineage with. Shader pipeline moved from D3DX to D3DCompile with on-disk bytecode
caches (both renderers). Pure MSBuild — no build-system divergence to negotiate.

### 2.4 Player-visible performance work (all config-gated, all kill-switched)
*Why they care: zone-in hitches and audio dropouts are the complaints their players file.*
The catalogue, each item root-caused and live-verified: 3.1s world-snapshot parse → phased
budgeted load; 300ms create bursts → 6ms/frame time budget; 1.2s char-select first-draw →
shader-cache RAM preload (both renderers); 500-800ms string-table stalls → whole-file buffering;
1s blocking title-music fade → non-blocking; texture warm-up gated behind the loading screen;
searchPath negative cache; cbuffer NO_OVERWRITE ring (865× driver-rename reduction on D3D11);
**the snapshot delete drain restored after being a no-op for the code's entire life** (unbounded
snapshot memory growth, fixed 2026-08-07). Every change carries a config kill switch — an
operator can revert any single behavior with a cfg line, no rebuild.

### 2.5 Crash fixes for 20-year-old engine defects
*Why they care: these are THE long-tail crash reports every SWG server community carries.*
Erase-without-reseat map-iterator UB (combat kill crash); heap corruption from unguarded
cross-thread containers (terrain zone-in AV); unlocked SearchCache map; two use-after-frees;
dPVS portal see-through (6 root-caused fixes); `safe_cast` Release-soundness class (wrong-class
template → vtable garbage call — fixed at every create site with virtual-discriminator
narrowing); Miles handle-0 and a 20-year `getSampleTime` handle leak; an NV driver thread race.
Each has a recorded failure signature and verification history — these are documented fixes,
not drive-by patches.

### 2.6 Audio correctness on modern Miles runtimes
*Why they care: "sound randomly dies / UI rollovers go silent" is a recurring community report.*
The full Miles version matrix was resolved by A/B conviction (7.2a Win32 / 9.3v x64; 9.3b storm
defect and the 7.2e dud identified so nobody re-walks that minefield), EOS polling replaces the
structurally dead callback, duck handoff ramped. NOTE for the offering: Miles redists are
license-bound and NOT in git — upstream vendors their own, and our matrix doc tells them
exactly which versions are safe. (The SDK header/lib additions under `external/3rd` need a
licensing check before any public PR — flagged §5.)

### 2.7 A diagnosability layer the codebase never had
*Why they care: when a player reports a hitch or a crash, their maintainers currently have
almost nothing. This change set ships the instruments.*
Audio-safe stall stack sampler (suspends only the stalled thread, symbolizes a snapshot);
stall watchdog with dump budget; Release-surviving probe families (`[ws.drain]`, `[editor.ws]`,
`[cellAtPos]`, portal-cull, texture-create, VB-lock — all default-quiet, cfg-armed); crash
forensics playbook. This is the "we can support what we ship" argument.

### 2.8 The in-client world-editor surface (the ecosystem play)
*Why they care: creator tooling retains communities. This is infrastructure their modders
could build on, not just fixes.*
A stable, versioned engine-advertise contract (v33, 160 names) exposing world-snapshot editing,
interior-layout (.ilf) editing, ray-pick/placement primitives, and scene control — powering a
live external editor (SWG-Toolkit) that decorates buildings, persists .ws/.ilf edits, and
round-trips byte-verified saves. The engine-side defects found and fixed on the way (lossy
reload, interior-NPC deletion cascade, proximity-index re-arm, loadScene teardown) benefit
every client even if upstream never adopts the contract itself. Offerable as an optional,
`_WIN64`-aware, export-gated module.

## 3. Track 2 — the data/TRE changes (Kenny's flagged dangler)

The code offering is clean; the data side needs SORTING before any of it is presented. Current
inventory and disposition:

| Layer | Contents | Upstream disposition |
| --- | --- | --- |
| `stage/override/` TRACKED payload (94 files) | 11 pixel + 9 vertex programs (//hlsl reauthors of //asm assets, PSRC-swapped .psh for texren/emissive/nebula/ui_radar), 72 textures (nebula skybox family + detail maps), `planet_tatooine.pln`, `datatables/interior/interior.iff` (the merged interior-fog fix) | **OFFERABLE** — these are our authored/derived works needed by gl11. Needs a per-file manifest: what changed vs stock, why, which renderer needs it. Delivery shape decision: loose-override recipe (searchPath layering, as we run) vs baked patch .tre |
| `stage/ilm_extract/` (775 MB, untracked) | Legends/ILM-extracted layer — JTL ship data is ILM-exclusive; also carries Legends PREFERENCE changes (the interior-fog audit is still open) | **⚠ PROVENANCE BLOCKER — cannot be offered as-is.** Options: (a) document it as an operator-sourced requirement with our extraction recipe, (b) audit which subset is retail-derivable and offer only that, (c) scope JTL support out of wave 1. Decision needed |
| `stage/override/{interiorlayout,object,snapshot}/` (untracked, gitignored) | SWG-Toolkit live editor data | **NOT offered** — consumer-owned working state, not content |
| cfg deltas | searchPath layering, `gameFeatures=33297`, budgets/probes | **OFFERABLE as documentation** — the annotated cfg is itself a deliverable (every key carries its why) |
| Tooling | `tools/tre-compare` (standalone byte-diff vs installs), the TRE parser in swg-blender-plugin, renderdoc-mcp (external repo) | **OFFERABLE** — tre-compare is exactly the tool that generates the Track-2 manifest AND lets upstream verify our data claims independently |

**The sort-out work item:** run tre-compare against a stock SWGSource install to produce the
authoritative "every byte we changed and why" manifest for the tracked payload; sweep
`ilm_extract` for the preference-change audit (the old open todo — now load-bearing for the
provenance decision).

## 4. Integration-risk framing (their real first question: "what does merging this cost us?")

- **Config-gated everything**: the kill-switch inventory is long and deliberate — any single
  behavior reverts with a cfg line. Default behavior without a cfg = stock-plus-fixes.
- **Same build system** (MSBuild, their solution layout), **same renderer-selection mechanism**,
  **no server-side changes required** — this is a client-only offering that speaks stock
  protocol (everything was developed against a live stock server).
- **Maintained against their moving master** — upstream syncs merge in continuously; the diff
  is current, not archaeological.
- **Verification culture**: boot gate discipline, dual-renderer/dual-platform gates,
  live-verification records in the commit history, probes that survive into Release.
- Honest liabilities to disclose up front: editor tools (Qt .ui) are pre-broken in both trees;
  the advertise surface is Win32-export-only today; Miles licensing (§2.6); the change set
  assumes the staged-directory layout for shader caches.

## 5. Decisions (called 2026-08-07 evening with Kenny)

1. **Vehicle: DECIDED — talk to the owners first**, prospectus in hand, before any code moves.
   The waves below are the MENU the conversation presents, not PRs-in-flight; merge order is
   negotiated after they bite.
2. **Wave curation: DECIDED — five curated waves** (revised per Kenny: x64 pulled OUT of wave ①
   into its own wave — it is the extensibility platform and a large coherent review, not a
   stability bullet; bundling it would poison wave ①'s "small diffs, easy yes" promise):
   - **① Toolchain + stability** — VS-2026/C++20, D3DX removal/D3DCompile, crash fixes, audio
     correctness, diagnosability layer. Small diffs, zero features, teaches our conventions;
     prerequisite for ② (x64 compiles on this toolchain).
   - **② x64 platform** — THE extensibility story, standalone: 2GB ceiling gone, modern
     middleware/library linking viable, the platform the next decade of community work rides on.
     Big but coherent review (pointer-width correctness, _WIN64 guards, per-platform staging,
     Miles 9.3v, dpvs static). **PITCH ORDER ≠ MERGE ORDER** — merge order is the wave order;
     the conversation is led by the pitch hierarchy in §5.7.
   - **③ Performance catalogue** — independent, cherry-pickable, every item kill-switched.
   - **④ gl11 renderer + data manifest** — biggest addition; first wave with a data dependency
     (the 94-file override payload + its tre-compare manifest).
   - **⑤ Editor surface + ilm recipe** — ecosystem play, fully optional.
3. **ilm_extract provenance: DECIDED — recipe, not bytes.** Never distribute the layer; offer
   the extraction recipe + tooling so operators source it from their own installs. JTL stays in
   scope, provenance stays clean.
4. **Miles SDK files in `external/3rd`**: licensing check STILL OPEN — before anything public.
5. **Toolkit positioning: DECIDED 2026-08-08 — the editor is CENTRAL to the pitch**, not a
   side-story. It is the differentiator; see §5.7.
6. **CLAIM CHECK: RESOLVED 2026-08-08 (Kenny)** — Legends AND Restoration both have x64 clients;
   we are NOT first and never claim it. **Neither has D3D11** — that claim IS uniquely ours (as
   is the editor). x64 framing = "x64 parity for the open SWGSource lineage, D3DX-free"
   (Restoration's x64 kept D3DX).
7. **Pitch hierarchy: DECIDED 2026-08-08 (supersedes the earlier x64-leads note)** — the
   conversation opens with **the editor** (a live in-client world editor: decorate a building,
   persist it, reload it — demo-able, unique in the ecosystem), backed by the four pillars:
   **modernization** (toolchain/D3DX-free), **x64**, **D3D11**, **performance**. The editor gets
   attention; the pillars prove the engineering underneath it is real and mergeable. The two
   NOBODY-ELSE-HAS-THIS claims are the editor and D3D11; x64 and modernization are
   parity-with-the-walled-gardens claims, which land differently but land well (they make the
   open lineage competitive).
8. **Timing: DECIDED 2026-08-08 — GATED ON THE TOOLKIT EDITOR MVP.** No outreach, no outward
   drafting until the editor can carry a live demo. Rationale: pitching the differentiator
   before it demos would spend the first impression on promises.

## 6. Work items WHEN RE-ARMED (trigger: toolkit editor MVP complete)

Nothing here starts until the gate in the header opens. Order of execution then:

1. Mine the full per-WAVE change catalogue with commit refs + verification evidence per item
   (source: the handoff archive + `git log` — the raw material is unusually well recorded).
2. Run the tre-compare manifest for the Track-2 tracked payload; ilm_extract preference sweep
   (scoped to supporting the RECIPE deliverable, not a distribution audit).
3. Draft the outward prospectus — **editor-led** (§5.7), waves as the merge menu, one page of
   framing + the catalogue as appendix — for Kenny's edit pass before anything leaves the
   building. Respect the claim discipline in §2.1/§5.6.
4. The one still-open call to fold in when made: Miles SDK licensing (§5.4).
5. Prepare the demo asset: the editor MVP demo IS the pitch's first five minutes — coordinate
   with the toolkit on a repeatable demo script (decorate → persist → reload → show the .ws/.ilf
   bytes) before the conversation is scheduled.
