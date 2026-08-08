# 2026-08-08 — SESSION CLOSE: §6 queue closed+verified · gl05 VB-lock CURED · upstream offering PARKED · fifth-.ws-variant correction

**READ FIRST.** Covers 2026-08-07 evening → 2026-08-08 morning, written before a restart. This
session CONTINUES from [2026-08-07-SESSION-CLOSE-2-ws-drain-queue-closed.md](2026-08-07-SESSION-CLOSE-2-ws-drain-queue-closed.md)
(which documents the §6 fixes themselves) — read that for the code detail; THIS file carries
everything after it: the live verification, the gl05 close-out, the upstream-offering plan (and
its parking), and the consumer exchange of 08-08 morning.

## 0. STATE — everything committed and pushed

`master` == `origin/master` == **`745d99dce`**, working tree clean. Contract **v33/160 unchanged**,
ord-82 unchanged, nothing owed to the consumer. Both platforms staged from `b47718cbc`'s tree
(5-target Release 0/0 both).

```
745d99dce  docs: upstream-offering plan -- written, decided, PARKED until toolkit editor MVP
bc235ed91  chore(gitignore): ignore the toolkit's live editor data under stage/override/
f40b37284  docs: session close 2 -- ws delete drain + $6 queue closed
b47718cbc  fix(WorldSnapshot): stream-out drain; wrong-class guards; detailLevelChanged   <-- the code
37c1e421f  (previous session close)
```

⚠ **The SWG-Toolkit repo has OUR uncommitted files awaiting THEIR session** (normal pattern —
their session commits provider files): `2026-08-07-PROVIDER-HANDBACK-ws-drain-and-wrongclass-hardening.md`,
`2026-08-08-PROVIDER-NOTE-fifth-ws-variant-and-precedence.md`, plus their `README.md` index edits.

## 1. The §6 fixes are ALL LIVE-VERIFIED (single launch, 08-07 evening)

Kenny ran one Win32 gl11 editor-scene session (cloning facility → deep desert → return):

- **Drain (6.4):** 1,800 objects streamed out across 734 `[ws.drain]` lines; `refused=` steady at
  6–9 (server-owned, correctly retried, zero teardown); `loaded` recovered 240→313 on return;
  **zero `createObject FAILED` after any drain line** (the failure signature — never seen).
  All 46 FAILED lines that session were login-phase supersedes, all BEFORE the first drain line.
- **.ilf refusal (6.3):** planted `draft_schematic` row in `edit_1106500.ilf` fired **exactly one**
  `WRONG CLASS … [class Object]` WARNING; building loaded fine minus the row; no storm. Test file
  byte-restored (5,775 B / 46 nodes, walk-verified). ⚠ Scoping facts that cost the first attempt:
  both Mos Eisley cloning facilities are REBOUND to toolkit .ilfs (nearest stock-file consumer is
  5 km out), and draft_schematic templates are **TOC-only** (0 per-tre, 3,899 in sku0 TOC).
- Consumer handback mirrored to the toolkit; key consumer-visible point: **borrowed `Object*` can
  now be deleted by mere travel** — their pointer-invalidation must extend beyond scene events.

## 2. gl05 815ms VB-lock stall — **CLOSED AS CURED** (second instrumented null)

Kenny ran the deliberate gl05 session (rasterMajor=5, `logDynamicBufferLockMs=5` re-armed,
5-point setup audit passed + probe symbol grep-verified in the staged DLL): **ZERO VBLOCK/IBLOCK**
with skeletal content streaming on a live server; zero gameplay stalls beyond the known
104–113 ms login-burst class + benign exit teardown. Cured by the 07-18 `shaderCachePreload`;
DISCARD-wrap suspect exonerated by absence. **Cfg restored: `rasterMajor=11` (toolkit-safe),
probe key stripped (code stays in the DLL, re-armable), closure recorded in the cfg comment.**
Bonus: first gl05 exposure of the drain — 210 lines, cross-renderer clean. Also disarmed
`logCellAtPosition` (→0, acceptance recorded). The 08-01 handoff's "NOT CLOSED" tail is struck in
this README.

## 3. ⏸ UPSTREAM OFFERING — planned, decided, PARKED (the big strategic item)

Full plan at [`.planning/research/UPSTREAM-OFFERING-plan.md`](../research/UPSTREAM-OFFERING-plan.md);
memory `project_upstream_offering_parked_until_editor_mvp` mirrors the triggers. Decisions, all
Kenny's, 08-07/08-08:

- **⏸ PARKED until the SWG-Toolkit editor MVP completes** — the editor is THE attention-getter
  (with modernization/x64/D3D11/perf as pillars); pitching before it demos wastes the first
  impression. **Re-arm trigger: watch the toolkit's handoff README for their editor MVP.**
- **Vehicle: talk to the owners first** (waves are the MENU, not PRs-in-flight).
- **Five merge waves:** ① toolchain/stability → ② x64 → ③ perf → ④ gl11+data manifest →
  ⑤ editor surface+ilm recipe. Pitch order ≠ merge order; **editor leads the pitch**.
- **⚠ CLAIM DISCIPLINE: NEVER claim "first x64"** — Legends AND Restoration both have x64
  clients (Kenny). Ours = x64 for the OPEN lineage, D3DX-free (Restoration's x64 kept D3DX).
  The two nobody-else-has claims are **the editor and D3D11**.
- **ilm_extract: recipe-not-bytes** (775 MB Legends-derived — never distribute; ship extraction
  recipe+tooling; JTL stays in scope, provenance stays clean).
- Still open at re-arm: **Miles SDK licensing check** for the `external/3rd` additions.
- Measured basis in the plan §1: +54k/−449k over 2,722 files vs the v2 base, maintained against
  upstream's moving master (mergeable by construction).
- On re-arm, §6 of the plan executes: per-wave catalogue mining (handoff archive + git log),
  tre-compare data manifest for the 94-file override payload, editor demo script, prospectus
  draft for Kenny's edit pass.

## 4. Consumer exchange 08-08 morning — the FIFTH `.ws` variant correction

Their `98321f7` superseded their own `.ws`-layout block: **four distinct `tatooine.ws` variants
ship in the TREs** (18,515/17,724/17,719/16,514 nodes — good census), three byte-layout
corrections (**all confirmed against `Node::save`**: `+48` = `portalLayoutCrc`; quaternion at
`+16` w-first; radius varies), BUT their inference *"15,808/1,178 came from a different client
install"* is **FALSE** — answered in `2026-08-08-PROVIDER-NOTE-fifth-ws-variant-and-precedence.md`:

- **`stage/override/snapshot/tatooine.ws` measured 08-08: 1,400,609 B / 15,808 nodes / 9,140
  top-level / OTNL 1,181 / 346 portalCrc≠0** — the FIFTH variant = their own `saveFiltered`
  output, and it **outranks all four TRE variants** (`searchPath_00_10`) — it is what actually
  loads in every live session.
- Smaller-than-stock = the documented saveFiltered buildout-node exclusion (07-31 close-out).
- **OTNL 1,178→1,181 = exactly three novel-template interns since 07-31** — independently
  corroborates the intern-is-permanent contract from the 08-07 handback §4.
- Id-comparability flip: their live ids are STABLE (override wins); the four-TRE precedence
  question bites only when the override save is absent.

## 5. Utinni `swgptr` change request — dispositioned WON'T FIX

Their 08-07 CR (Utinni binds two advertised rows with 32-bit `swgptr` where our signatures take
`int64`; x86 `__cdecl` misalignment analysis) is **correct on the ABI** — but **Utinni is retired
and stays frozen** (Kenny: the toolkit has exceeded Utinni's functionality; it will never be
un-retired). No runtime exposure (both rows inert). The guard against future copy-the-broken-shape
is their own `rva_table.cpp` ⚠ comment + the CR file itself. Disposition recorded in the provider
note §5 + their README entry.

## 6. Config / repo state snapshot

- `stage/client.cfg`: `rasterMajor=11` · `logDynamicBufferLockMs` REMOVED (comment records the
  cure) · `logCellAtPosition=0` · `streamOutSnapshotObjects` in NO cfg (default ON is intended) ·
  `singlePlayerStartLocation=3480/3/-4870` both cfgs (KEEP) · `portalCullProbe=true` still armed
  (strip decision never made — carried) · BOM-clean verified after every edit.
- `.gitignore` (`bc235ed91`): `stage/override/{interiorlayout,object,snapshot}/` ignored — the
  toolkit's live editor data can never be swept into a provider commit; **the provider repo holds
  NO backup of those bytes** (their persistence is the only copy); the tracked 94-file
  shader/texture override payload is UNAFFECTED.
- Memory updated this session: drain fix + VB-lock closure in the perf-arc memory;
  new `project_upstream_offering_parked_until_editor_mvp`.

## 7. Open board for next session

- **Waiting on:** the toolkit's editor MVP (= the upstream re-arm trigger AND their exterior
  `.ws`-node editing phase, which will likely generate the next contract requests — their
  Phase 05.3 wave 1 already bound `moveObject`/`removeObject`/`addNodeAt` per the Utinni CR).
- **Watch items (passive):** `createObject FAILED CEC_objectAlreadyExists` after a `[ws.drain]`
  line (never seen, both renderers); drain `std::find` over `ms_loadedList` if a stall sample
  ever lands in `WorldSnapshot::update` during travel; NV driver-threading soak.
- **Discretionary backlog:** 873 ms NV-driver `WaitForSingleObject` in `drawIndexedTriangleList`
  (Direct3d9.cpp:4469, one-off, uninvestigated); ~560 ms GroundScene-ctor frame (Kenny:
  deprioritized, hidden by loading screen); ilm_extract preference sweep (now scoped to the
  upstream RECIPE deliverable); portal probe suite strip decision (`portalCullProbe` +
  STUCK0/CELLSTATE/KILLDETAIL, soaked a month).
- **Nothing queued, nothing owed.** The §6 queue is empty and every 08-06→08-08 fix is
  live-verified on both sides.
