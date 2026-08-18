# DESIGN — TRE Linter + Set Builder (`@swg/compose`) for SWG-Toolkit

> ⚠ **SUPERSEDED DRAFT (kept for review-trail section references).** The FINAL design —
> with all CONSULT-78 corrections applied (Cursor binding check + Fable adversarial review,
> incl. the F1 toc-set-cannot-tombstone blocker and the F2 content-level Stage-8 rework) —
> is `D:/Code/SWG-Toolkit/.planning/handoff/2026-08-17-PROVIDER-DESIGN-tre-linter-set-builder.md`.

**From:** swg-client-v2 maintainer session (provider) · 2026-08-17
**To:** SWG-Toolkit session, as input to normal GSD phase planning (per
`todos/pending/tre-linter-and-toc-set-builder.md`: "run it through the normal phase planning,
not implement from this file" — same rule applies to this document).
**Scope decided by the maintainer (owns both repos):** build the FULL engine core now —
including the recipe/composition layer — behind a headless CLI; stage the GUI. New UI surfaces
come later against the working core through the normal sketch→UI-SPEC→gate process; where a
feature fits an EXISTING UI element, wire it in v1.
**Design inputs:** CONSULT-76 synthesis (compose semantics, cut-lines, CI properties),
CONSULT-76 M0 + CONSULT-77 (format ground truth, now landed in native-core `1ecb559`), the
08-17 CR/handback/reply exchange, and a code-read of the toolkit as of `c356e8b`.

---

## 1. What this is

Two products in one engine, from the maintainer's direction ("a TRE file linter and builder
that can combine TRE files into a TOC TRE set"), generalized by the recipe layer:

- **Linter** — walk archives / mounted sets / composed outputs and report structural,
  consistency, and cohort defects WITHOUT refusing to load (the tolerate-and-report posture
  `parseIff` adopted in `3476b11`; its `result.defects[]` records are a linter input).
- **Builder** — compose an output TRE set from ordered inputs (loose dirs, existing archives,
  recipe-declared layers), emitting packed `.tre` archives + a master `.toc` index, a loose
  overlay, or both — gated so nothing structurally invalid or set-inconsistent ships silently.

The recipe layer makes the input declarative (a shareable TOML file with pinned layer sources)
instead of an ad-hoc ordered list; the engine treats "explicit file list" as a degenerate
recipe, so both entry styles share one pipeline.

## 2. Package shape and placement

**New workspace package `packages/compose` → `@swg/compose`**, pure Node TS:

- Depends on `@swg/native-core` (direct `require`, same as the renderer's Path-B pattern in
  `packPatch.ts:31-36` — native tests already prove native-core loads in plain Node) and
  `@swg/contracts` (new types land there, following `contracts/src/policy.ts` precedent).
- **MUST NOT import from `@swg/renderer` or Electron.** This is the deliberate divergence from
  the current "logic lives in renderer services" pattern, and it is load-bearing twice over:
  (a) the headless CLI (`bin: tresmith`) must run under plain `node` for CI and for
  standalone/extractable use; (b) renderer services then WRAP compose functions (thin
  adapters, like `packPatch.ts` wraps `buildTre`), never the reverse.
- Existing renderer services that already do compose-adjacent work stay where they are and are
  REUSED via the wrap direction where possible; where a service is pure (no store imports),
  consider MOVING it into `@swg/compose` and re-exporting (candidates on inspection:
  `crc32.ts`, `pathSafety.ts`, parts of `tocReader.ts`, `clientSearchOrder.ts`). Judgment
  call per file during planning — do not force it.

CLI surface (v1): `tresmith lint <target>`, `tresmith compose <recipe|--files ...>`,
`tresmith verify <recipe>` (drift check), `tresmith explain <virtualPath>` (winner + shadow
chain + provenance). Always-dry-run: `compose` prints the plan and requires `--write`.

## 3. Module map (CONSULT-76 modules → toolkit reality)

| Module (in `@swg/compose/src/`) | Responsibility | What already exists / notes |
|---|---|---|
| `recipe.ts` | TOML parse/validate, exact pins only (reject range syntax with "not yet"), waivers, must-exist smoke list, variant selection (variant = pre-authored sub-tree selection, NEVER a transform) | New. Schema = CONSULT-76 §3.3 (R1/R2/R8 decisions). `[output]` gains `form = "toc-set"` as the toolkit-native target alongside `"loose-override"` |
| `layers.ts` | Source resolve (local path / git URL), digest, ingest+index, intra-layer collision rules | New. Path normalization MUST reuse ONE implementation — native `fixUpFileName` via a small native-core export or a TS port pinned by fixtures against `TreeFile.cpp:511-601` (C-1/C-2: one canonicalization, at ingest) |
| `fold.ts` | The compose fold: winners / removed / provenance / shadow chains, materialized total order, deterministic manifest + `manifest_digest` | Shadow-chain logic exists in native `TreMount::resolveChain` / `vfsEntriesColumnar` — reuse for VERIFICATION, but the fold itself is TS over the ingest index (it must handle not-yet-written outputs and removals, which no mount of existing archives can represent) |
| `lint/` | Rule registry + rules (see §4) | Pattern-copy `policyRuleset.ts`/`policyEvaluator.ts` — bundled ruleset in contracts + per-project override file, merged by rule id; adding a rule = data + one registration, never evaluator changes |
| `repair.ts` | IFF size-clamp repair as content-addressed derivations | Vendored port of the field-proven repairer (see §5). `parseIff`'s tolerate-and-report already computes the clamp on READ; repair = write-back of the honest sizes |
| `emit-packed.ts` | Packed emit: N `.tre` archives + master `.toc` | `buildTre`/`repackTre` exist (post-`1ecb559`). **The `.toc` WRITER is new** (§6) |
| `emit-loose.ts` | Loose overlay + journal + prune + cfg fragment | Largely exists: `looseOverrideDeploy.ts`, `cfgActivator.ts`, `clientSearchOrder.ts`. Wrap/reuse; add the emit journal so prune never deletes user files |
| `verify.ts` | Stage-8 round-trip: mount the EMITTED output (native `TreMount` + real `.toc` parse) and assert winners/removed match the fold's manifest — fatal, no bypass | Native mount = the verifier here is a DIFFERENT code path from the fold (TS fold vs C++ mount), which preserves meaningful redundancy inside the toolkit. The fully independent audit (`tre-compare`, Python) stays on the provider side as an external CI cross-check — never vendored, per the self-referential-fixture lesson |
| `manifest.ts` | Canonical JSON, digest vocabulary, report rendering (`compose-report.json` + text) | New. No `Date.now()` inside digested content; timestamps live outside the digest |
| `cli.ts` | `tresmith` entry | New. Exit codes 0 (clean) / 1 (warns) / 2 (errors) / 3 (structural failure) / 4 (internal) |

Determinism contract (CI-enforced): same recipe + same resolved sources ⇒ byte-identical
manifest and byte-identical packed output across two runs and two machines (build-twice test;
the writer half already has this guarantee in native-core).

## 4. Linter architecture

**Rule registry, data-driven, on the policy pattern** (`policyRuleset.ts` precedent —
bundled defaults + optional per-project override, merge by id, unknown profile ⇒ no bans):

- **Severity vocabulary: `error` / `warn` / `info`** (one vocabulary everywhere — R5).
- **Rule classes, v1:**
  1. *Structural (per file)*: IFF size-fit walk — consume `parseIff` `result.defects[]`;
     engine-parity by construction (their `Iff.cpp:386` posture decision).
  2. *Archive-level*: header sanity vs the corrected version model; TOC sorted by (crc,
     stricmp); name-block offsets in range; `compressor` ∈ {0, 2}; CRC-of-name mismatches;
     duplicate CRC with different names (the 32-bit collision case).
  3. *Set-level*: `.toc` entries resolving nowhere (the `_client_dx11` prefix-bug class);
     duplicate paths across archives (reported as shadow info, not error); tombstones that
     delete nothing; degenerate shadows (stub-texture/zeroed-datatable landmine catalog —
     each catalog entry becomes a rule with its historical failure mode in the message).
  4. *Cohort (the marquee class)*: **the skt/lmg/lod/sat pairing rule ships in v1** — backed
     by the invisible-NPCs field incident; message names the exact missing paired files.
     General witness-predicate machinery (COMPLETE over touched cohorts, content-hash
     witnesses) ships as the engine behind it; additional cohort classes are data additions.
  5. *Per-format sanity (cheap ones only)*: skeleton chunk-sizes vs bone count (the
     `protocol_droid.skt` proof pattern).
- **Baseline regression gating** (`V_full \ V_base`): lint of a composed set gates ONLY on
  violations not present in the base alone — stock defects report as `info: inherited`.
  Without this, every real install drowns the panel; with it, "what did MY layers break" is
  the default question answered.
- **Waivers**: content-pinned (sha256 multiset), reason-required, self-expiring on content
  change; TOML in the recipe (or a sibling file for lint-without-recipe), display-only in UI.
- **The 102-file ILM corpus is the linter's acceptance fixture set** (real corrupt bytes +
  known-good repaired pairs; see the 08-17 PROVIDER-REPLY for paths).

## 5. Repairer vendoring (R9 discipline)

Port the field-proven bottom-up clamp repairer (provider side, Python) into `repair.ts` (or a
native-core helper if profiling demands). **Acceptance test: reproduce all 102 known ILM
repairs byte-identically** against the verified outputs in
`Galaxies-Reborn/stage-B-override/appearance/skeleton/`. The naive clamp-to-parent-end formula
is WRONG for middle siblings (it swallows following chunks) — the bottom-up
siblings-sum-to-parent-end walk is the load-bearing part; do not re-derive from the validator.
Repair runs as a content-addressed derivation (input-hash → output-hash memo), is idempotent,
and never widens a size field. v1 exposure: `tresmith lint --repair-into <dir>` + the compose
pipeline's Stage 4; findings stay report-only unless repair is explicitly requested.

## 6. The `.toc` writer — the one new format deliverable, with its own M0

"Combine TRE files into a TOC TRE set" requires emitting the master `.toc`, which no tool in
either repo writes today. Format ground truth is strong on the READ side: engine
`TreeFile_SearchNode.h:331-345` (header: `' COT'` token + `TAG_0001` + two uint8 compressors +
counts/sizes), `:774-865` (parse), `:1010-1113` (lookup + member-archive payload reads by
absolute offset), and the toolkit's own `tocReader.ts` (validated against real
`sku0_client.toc` bytes). Writer requirements derived from the reader: 24-byte searchTOC
records (`uint8,uint8,uint16,5×uint32` — a DIFFERENT 24-byte layout from the `.tre` TOC),
CRC-sorted, tree-file name block + per-entry treeFileIndex, `TAG_0001` version, mirrored
byte order throughout, zlib-or-raw blocks per the two compressor bytes.

**Decisive experiment before the writer is trusted (the M0 pattern, budget: one afternoon):**
hand-write a minimal `.toc` indexing one or two container `.tre` files (one entry stored, one
zlib, one path shadowing a stock asset), mount via `searchTOC_XX_Y` in a probe cfg, and use
the FATAL-vs-boot + override-visibility detectors. The probe harness from the 08-17
PROVIDER-REPLY (launch convention, exit-code detector, kill discipline, skipIf-missing lane)
applies unchanged. ⚠ Note from the census: a `.toc`-membered container may be version `'0006'`
with `numberOfFiles==0` — but emitted containers should be ordinary self-indexed `'0005'`
archives that ALSO appear in the `.toc`, which the engine accepts (retail mounts both kinds);
the probe should confirm a `'0005'` container works as a `.toc` member (predicted yes — the
member-open path never reads the container header at all, `TreeFile_SearchNode.cpp:822-835`).

**Explicitly deferred (unchanged from the CR round):** v0006 WRITES stay refused until the
32-byte record layout is settled against real Restoration bytes (pad model vs the GR
field-reorder map — first task if v0006 write support is ever wanted, likely never needed for
emit); no `COT2000`; no Restoration payload handling beyond enumerate; no `"4000"` emission.

## 7. Compose pipeline (CONSULT-76 §3.4, unchanged structure)

0 Resolve & pin → 1 Ingest & index → 2 Fold (abort if a tombstone is required under loose
emit) → 3 Structural validate → 4 Repair (opt-in, content-addressed) → 5 Re-validate (nothing
invalid is emitted; `--allow-invalid <path>` is the only escape, recorded in the manifest) →
6 Consistency lint (cohorts, references, baseline, waivers) → 7 Emit plan & write
(admissibility + journal-or-refuse on a dirty output dir + prune) → 8 Verify emitted
(mount-and-compare vs manifest; fatal).

Gates G1–G4 and the CI property set (P-DET, P-ASSOC, P-IDEM, P-BASE, P-ROUND, P-COHORT,
P-BASELINE, P-WAIVER, P-PRUNE, P-PRIORITY, P-REPAIR) carry over verbatim from the synthesis;
P-COHORT (the historical skeleton-without-LOD recipe must fail naming the exact missing
files) and P-PRIORITY are the first fixtures written. Emit admissibility uses the
CONSULT-76-M0-confirmed comparator: **priority is pure numeric across node kinds** — emitted
nodes just need a higher number than every base mount; assert it, don't assume it.

## 8. Verification & harness integration

- Every new format fixture registers in `fixtureRegistry.ts` with a mandatory `loaderSource`
  citation (existing sweep enforces it). New entries: searchTOC record (cite
  `TreeFile_SearchNode.h:349-360` + real `sku0_client.toc` bytes), recipe/manifest canonical
  forms, the 102-file repair pairs (fixtures-real lane), the probe archive pair
  (`treexp_a.tre` / `q2_fwdver.tre` — vendoring already granted).
- **Running-client acceptance lane** (skipIf-missing, like `fixtures-real`): the
  FATAL-vs-boot gate from the PROVIDER-REPLY, run for (a) default `buildTre` output, (b) the
  first `.toc` emit, (c) composed-set smoke (boot gate = character select, the standing
  project bar).
- Provider-side standing audit (not in the toolkit): `tre-compare` diff of packed-vs-loose
  emits of the same recipe = zero content differences; runs on our side against release
  candidates.

## 9. UI in v1 — wire into EXISTING elements only

Per the maintainer: no new sketched surfaces in v1; anything needing a new surface waits.

| Feature | Existing element | v1 wiring |
|---|---|---|
| Lint findings on a mounted archive/set | The Assets/VFS browser + Inspect tab stack (`TreVfsBrowser`, `InspectorStack`) | Per-entry defect badge from `parseIff.defects[]` + archive-level lint summary line; counts only, details in the inspector card |
| Shadow/override/tombstone provenance | Already rendered (`isOverride`/`isTombstone`/`shadowCount` columns; `resolveChain` exposed) | Add "winner archive" to the existing chip's tooltip where absent; no new panel |
| Version/format facts | `MountedArchivesList` version chip (now `v5000`/`v6000` post-`1ecb559`) | Unchanged; enumerate-only chip already per-payload |
| Compose/deploy | Existing stage → save version → deploy → revert flow (`changesetService`, `stageSnapshot`, `packPatch`, deploy panels) | `packPatch` path gains the option to emit via `@swg/compose` (same staging entries in, packed set + `.toc` out) — the deploy UI itself does not change |
| Recipe runs, reports, waiver browsing, layer stack UI | **NONE — deliberately CLI-only in v1.** | Declare CLI-only features INVISIBLE in the GUI (no half-rendered controls — the 05.6 bare-controls lesson); the UI-SPEC for the future Compose panel is a named follow-up with its own sketch cycle |

## 10. Suggested build order (input to GSD phase planning, not a plan)

| Stage | Deliverable | Acceptance |
|---|---|---|
| B0 | `.toc`-writer decisive probe (hand-written, §6) | Client mounts the hand-built `.toc` set; override visible; FATAL-vs-boot lane green |
| B1 | `@swg/compose` skeleton + `lint` v1 (structural + archive + set rules, registry on the policy pattern, report + exit codes) | 102-file corpus fully flagged; registry sweep green; `tresmith lint` runs headless |
| B2 | Repairer port | 102/102 byte-identical vs stage-B-override |
| B3 | Fold + manifest + explicit-file-list compose + packed emit + `.toc` writer + Stage-8 verify | P-DET/P-ROUND/P-PRIORITY; composed set boots to character select |
| B4 | Recipe layer (TOML, pins, variants, waivers) + baseline gating + cohort rule (skt/lmg/lod) | P-COHORT names the exact missing files; P-BASELINE; two-layer recipe (base + one bugfix layer) composes and boots |
| B5 | Loose-overlay emit path unification + cfg fragment + `verify` drift command; v1 UI wiring (§9) | Packed-vs-loose zero-diff (provider audit); UI badges live on a real mount |

Rationale for order: B0 de-risks the only unconfirmed format; lint-first (B1) ships standalone
value before any composition exists (the "linter with a builder attached" framing), and B2's
corpus makes B1 honest.

## 11. Open questions going into planning

1. `.toc` writer probe outcomes (B0) — settles searchTOC writer spec the way M0 settled the
   `.tre` writer.
2. Where the repairer port lives (TS vs native) — decide on B2 profiling, not upfront.
3. Which pure renderer services migrate into `@swg/compose` vs get wrapped — per-file call
   during planning.
4. Recipe `source` = git URL in v1 or local-path only first? (Pins are exact either way;
   git adds fetch/auth machinery — acceptable to defer git to a fast-follow.)
5. Cohort class table initial contents beyond skt/lmg/lod/sat — mine the landmine catalog;
   provider will supply the seed table.
