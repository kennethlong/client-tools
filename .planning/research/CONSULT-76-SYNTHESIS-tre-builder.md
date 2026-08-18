# CONSULT-76 SYNTHESIS — configuration-driven TRE-set builder

Adversarial synthesis over four consultant outputs + the shared evidence pack. Written by a
fresh Fable 5 with no session context, re-derived from the documents alone, with three claims
settled directly against the engine C++ (`sharedFile/TreeFile*.cpp`) and the repo during
synthesis (marked **[VERIFIED HERE]**).

Inputs:
- Evidence pack: `.planning/research/CONSULT-76-EVIDENCE-tre-builder.md`
- Codex (writer-oriented TRE/TOC format spec): `CONSULT-76-codex-tre-writer-spec.out.md`
- Cursor (byte-level format tables): `CONSULT-76-cursor-byte-maps.out`
- Sonnet (recipe/layer ecosystem + UX): `CONSULT-76-sonnet-recipe-ecosystem.out.md`
- Opus (formal composition semantics): `CONSULT-76-opus-composition-semantics.out.md`

---

## 1. REFUTE — weakest load-bearing claim per consultant

### 1.1 Codex — the `SearchTree::validate()` 0004-only gate

**Claim (Codex §1.6, §6.1):** `SearchTree::validate()` accepts only `TAG_0004`, while the
builder writes `0005`; "if any caller gates loading through `validate()`, `0005` may fail
there." If true, the entire choice of `TREE/0005` as writer target is at risk.

**Verdict: refuted as a risk — [VERIFIED HERE].** `SearchTree::validate()` is reachable only
via `TreeFile::validateSearchTree()` (TreeFile.cpp:499), and a repo-wide grep of `src/` finds
**zero callers** of `validateSearchTree` — it is dead code in the client load path. The
constructor path (`TreeFile_SearchNode.cpp:446-449`) accepts `0004` and `0005`. Writing
`TREE/0005` is safe for the stock client. (Caveat: server-side or external tools outside this
tree could call it; irrelevant to the client target.)

Codex's remaining open questions that stay open (see §1.5): MD5-block optionality, and
whether the engine's stored-payload read path tolerates `compressedLength = 0` exactly as
TreeFileBuilder writes it. Both fold into the decisive experiment (§4). Everything else in
the Codex spec is doubly sourced (Python reader + engine C++ + stock TreeFileBuilder) and
cross-checks cleanly against Cursor's byte maps — the two format documents **converge from
independent derivations** on header layout, TOC entry format, name-block semantics,
searchTOC's length-not-offset fifth field, and sort requirements. That convergence is the
strongest signal in the round.

### 1.2 Cursor — reader-derived tables presented as writer contract, and a re-derived repair formula

Two weak load-bearing claims:

**(a) "`uncomp_size_of_name_block` … not enforced on read for `.tre`" (header table, §1.2).**
True of the *Python reader only*. The engine allocates and decompresses against this field
(Codex §2.14 cites `TreeFile_SearchNode.cpp:452-484`). A writer treating any "not enforced"
row in Cursor's tables as slack will produce archives the Python tools accept and the client
rejects or overruns. **Ruling:** Cursor's tables are byte-accurate *reader* maps — excellent
as layout reference — but the *writer contract* is Codex's "strict writer form" (his §5),
never Cursor's leniency notes. Same applies to "negative int32 offsets not rejected" (gotcha
28) and "any nonzero compressor triggers zlib" (gotcha 5): writer emits only 0/2, only
non-negative, period.

**(b) The derived IFF clamp formula (§4.6): "if `pos + 8 + block_length > end`, set
`block_length := end - pos - 8`".** Cursor states "No repair_iff / clamp implementation
exists in `tre_reader.py`, `tre_decrypt.py`, or `swg_iff/reader.py`" and then *derives* a
repair rule from the validator. Two problems. First, the evidence pack says a repair pass
**exists and has repaired 102 files with zero failures** — it lives outside the modules
Cursor scanned; the correct move is to locate and vendor the field-proven implementation, not
re-derive one. Second, the derived formula is **subtly wrong in the general case**: clamping
an oversized block to the parent's remaining extent is only correct for the *last* child; an
oversized *middle* sibling clamped to parent end would swallow its following siblings. The
proven repairer is described as bottom-up with a siblings-sum-to-parent-end invariant — its
actual clamp rule must be treated as authoritative and diffed against any reimplementation.
**Ruling: refuted as an implementation source; accepted as a validator spec.** Acceptance
test: reimplementation (or vendored code) must reproduce the 102 known repairs byte-identically
(they are already verified in `stage-B-override`).

### 1.3 Sonnet — a v1 recipe schema that breaks his own reproducibility guarantee

**Claim (implicit across §1/§4):** v1 ships semver *ranges* (`^2.0`), a `registry:` resolution
scheme, and per-layer `[layer.options]` knobs — while §4 promises "a given recipe.toml + its
resolved lock state + base fingerprint must produce byte-identical output" and the lockfile is
explicitly **deferred to v2** (§1, "Why not a lockfile").

**Verdict: internally inconsistent as scoped; the ecosystem design survives, the v1 scoping
does not.** Ranges without a persisted lock mean two users composing the same recipe a week
apart get different layer versions — the exact "diff 40,000 files on Discord" failure the
reproducibility section forbids. It also collides head-on with Opus D-7 (every layer resolves
to a verified content digest before compose). Resolution in §2.3 below: **v1 = exact pins
only** (a git URL + tag/SHA or local path *is* the lock); ranges and registry arrive together
with the lockfile split, later.

Second refutation, same document: `[layer.options]` values like `interior-fog = "off"` and
`intensity = "reduced"` are compose-time *content transforms* — exactly Opus's `Derivation`
cell, which Opus excludes from v1 for three strong reasons (§5.6: no per-format writers, breaks
path-locality, complicates the witness predicate), and which the format lane cannot support
(no IFF editor exists). **v1 options must be realized as variant selection** — an option value
selects which pre-authored file subset inside the layer package is mounted (a sub-layer), never
a transform. Sonnet's `derived-from-user-install` provenance class is likewise a *declared
category* in v1 (used by the smuggling CI check) but no v1 layer may actually require a
compose-time generation step.

Minor factual slip, not load-bearing: "TOML preserves array order, unlike a YAML map" — YAML
sequences preserve order too; the TOML choice stands on its real merits (comments, no
whitespace coercion footguns, cfg-file familiarity).

What must be kept from Sonnet, verbatim: `provenance.toml` with the closed origin vocabulary
(the one mechanically-enforceable teeth behind recipe-not-bytes), the always-dry-run UX, the
error text that names the historical failure mode, and `verify` as a standing drift command.

### 1.4 Opus — the tombstone semantics claim, verified against a Python model instead of the engine

**Claim (§5.1):** "the engine treats a length-0 entry in a `.tre` as a global remove
(verified in `virtual_tree.py` …)". This is load-bearing for C-14, P-ROUND's tombstone
fixture, the emit-mode decision matrix, and §8.1 (whether the packed emitter is a v1
blocker). But it was verified against `tre_compare`'s *Python model* of the engine, not the
engine — a circularity: if the model is wrong, Stage 8 verifies the emit against the same
wrong model and passes.

**Verdict: claim CONFIRMED — [VERIFIED HERE], against the C++.**
`TreeFile::SearchTree::localExists` (TreeFile_SearchNode.cpp:565-571): on CRC/name match with
`m_tableOfContents[mid].length == 0` it sets `deleted = true` and returns false. Every
TreeFile search loop (`exists` :554-557, `getFileSize` :595-601, `open` :815-818) iterates
`for (…; !deleted && …)` — **a zero-length TRE entry stops the search across ALL
lower-priority nodes**: a true global tombstone. And `SearchTOC::exists`
(TreeFile_SearchNode.cpp:1010-1014) hardcodes `deleted = false` — a searchTOC can *never*
tombstone, confirming Opus's TRE-vs-TOC contrast exactly. The circularity is now broken; the
Python model matches the engine on this point.

Opus's remaining weakest spot after that: the Stage-7 admissibility comparator ("including
the node-kind tiebreak `path < tree < toc`", C-10). The evidence pack states the kind
ordering as given, but the *exact comparator* — whether a `searchTree_XX_Y` with a high
priority number can interleave above a low-priority loose `searchPath`, and how equal
priorities tie-break — must be read out of `TreeFile::addSearchPath` /
`addSearchTree` / `addSearchTOC` insertion logic before `emit.py` asserts anything. Listed in
§1.5.

Also **[VERIFIED HERE]**: the modules Opus's "single highest-leverage decision" leans on all
exist with the claimed names — `tools/tre-compare/src/tre_compare/virtual_tree.py`
(`safe_virtual_key`, `fix_up_file_name`, `build_virtual_tree`), `cache.py`, plus tests. The
reuse decision is sound and available.

### 1.5 Claims that must be verified against the C++ TreeFile reader before implementation

Settled during this synthesis:
- ~~`SearchTree::validate()` gates loading of `0005`~~ — **no callers; dead code** (§1.1).
- ~~Zero-length TRE entry = global tombstone; TOC cannot tombstone~~ — **confirmed** (§1.4).

Still open — verify by code-read and/or by the §4 probe before the corresponding module is coded:

1. **Node ordering comparator** (`TreeFile::addSearchPath/addSearchTree/addSearchTOC`): does
   node *kind* trump the numeric priority, or do priorities interleave across kinds? Gates
   Opus C-10 (emit admissibility) — the "silently does nothing" failure class.
2. **Stored-payload read path**: engine behavior when `compressor = 0` and
   `compressedLength = 0` (TreeFileBuilder's convention) — confirm the read uses `length`,
   not `compressedLength` (`TreeFile_SearchNode.cpp:~695-704`).
3. **MD5 trailing block**: required by the client, or tool-only? (Header has no field pointing
   at it; expected ignorable — but confirm by probe, and write it anyway for byte-parity with
   stock tooling.)
4. **`uncompSizeOfNameBlock` exactness**: engine allocation behavior if it disagrees with the
   actual decompressed size (writer must be exact regardless; know the failure mode).
5. **Duplicate CRC handling** in the engine binary search (`:958-976`) — required to define
   writer behavior on same-CRC-different-name collisions (rare but real with a 32-bit CRC over
   a 100k-path namespace).
6. **Loose `SearchPath::exists`** `deleted` semantics — confirm loose files can never set
   `deleted` (expected; confirms "loose emit cannot express deletion", Opus §5.1).

---

## 2. RECONCILE — silent disagreements, decided

| # | Disagreement | Between | Decision | Rationale |
|---|---|---|---|---|
| R1 | **Version pinning in v1**: semver ranges + registry (Sonnet) vs verified content digest per layer before compose (Opus D-7) | Sonnet/Opus | **Exact pins only in v1** (git URL+SHA/tag or local path). Ranges+registry+lockfile arrive together, post-v1. | Ranges without a lock break the reproducibility guarantee both consultants require. |
| R2 | **Layer options / parameterized layers**: `[layer.options]` transforms (Sonnet) vs `Derivation` cells out of v1 (Opus §5.6) | Sonnet/Opus | Options = **variant selection** of pre-authored sub-trees inside the layer package; no compose-time transforms in v1. `Derivation` stays a stubbed ADT arm. | No per-format writers exist; transforms break path-local incrementality; variant selection expresses fog-off/intensity today. |
| R3 | **Base-install verification**: "never a hard fail" (Sonnet §3) vs Stage-0 digest-mismatch abort (Opus D-7/3.1) | Sonnet/Opus | **Split by trust boundary**: layer-package digest mismatch = hard abort (supply chain); base-install fingerprint mismatch = loud WARN, compose proceeds, manifest records `base_verified: false` and reproducibility claims are void. | The user owns the base bytes; the ecosystem's real attack surface is the layer channel. Both consultants are right about different objects. |
| R4 | **Cohort/pairing declaration ownership**: per-layer `[[provides]]` completeness self-declaration (Sonnet) vs global class table + computed witness predicate (Opus §2.2-2.3) | Sonnet/Opus | **Global class table ships with the builder** (format lane owns the patterns); Opus's CONSISTENT predicate is the sole authority. Layer `provides` blocks are optional documentation that improve error text, never inputs to the verdict. | Self-declared completeness is exactly the honor system the tool exists to replace; the witness predicate computes it from content hashes. |
| R5 | **Severity vocabularies**: `hard`/`soft` conflicts (Sonnet) vs `error`/`warn`/`info` + strength `same-origin`/`advisory` (Opus) | Sonnet/Opus | One vocabulary: **error / warn / info** everywhere (report, exit codes, conflicts). `hard`→error, `soft`→warn; class `strength` maps onto it per Opus's table. | Two severity systems in one report is how warning fatigue starts. |
| R6 | **Ordering mechanism**: array order + `after` hints (Sonnet) vs toposort + declaration-order tiebreak + **materialized order in manifest** (Opus C-4) | Sonnet/Opus | Adopt Opus C-4 wholesale; Sonnet's array order *is* the declaration order the tiebreak uses; `after` edges validated, cycles abort. | A merely-valid toposort is non-deterministic across tool versions; the materialized list is the contract. |
| R7 | **Derived-output cache key**: keyed by base fingerprint (Sonnet §5 idea 3) vs content-addressed `derivation_hash(input_hash, transform@version, params)` (Opus C-9/4.2) | Sonnet/Opus | **Content-addressed** (Opus). Base-fingerprint keying is subsumed: the input hash comes from the base file's bytes. | Finer-grained, survives base re-verification, dedupes across recipes. |
| R8 | **Waiver/config syntax**: YAML blocks (Opus §2.7 examples) vs TOML recipe (Sonnet) | Opus/Sonnet | **TOML everywhere**, waivers included, in the recipe file. Opus's waiver *semantics* (content-pinned, self-expiring, reason-required) unchanged. | One hand-authored format; Opus's YAML was illustrative, not argued. |
| R9 | **Repair implementation status**: "exists, 102 files, zero failures" (evidence pack) vs "no implementation exists in repo" + derived clamp formula (Cursor) | Evidence/Cursor | The proven repairer exists outside the modules Cursor scanned. **Locate and vendor it**; any reimplementation must reproduce the 102 repairs byte-identically. Cursor's derived formula is rejected as-is (§1.2b). | Field-proven code with a 102/102 record beats a formula derived from a validator. |
| R10 | **Writer target risk**: `0005` may fail `validate()` (Codex) vs stock builder writes `0005` | Codex/internal | Target **`TREE/0005`, 24-byte entries, retail layout**. The `validate()` concern is dead code (§1.1). | Verified: zero callers of `validateSearchTree` in `src/`. |
| R11 | **Where the TRE writer sits in the roadmap**: whole-doc focus now (Codex) vs "packed becomes a v1 blocker iff any layer needs deletion" (Opus §8.1) vs loose-first (evidence pack, Sonnet default) | Codex/Opus/Sonnet | **v1 emits loose overlay; writer is milestone 5**, immediately after v1 gates. Standing check: Stage 2 aborts if any recipe tombstones under loose emit (Opus C-14). None of the initial layers (bugfixes/JTL/ILM lighting/ILM sound) require deletion — exclusion of preference-kills is achieved by *not layering them in* over a pristine base, not by deleting. | Ship the compose/consistency value early; the writer's spec is now probe-confirmed (§4) so milestone 5 is de-risked, not speculative. |
| R12 | **Tool naming/layout**: `tresmith` (Sonnet) vs `tools/tre-builder/src/tre_builder/` (Opus §6.3) | Sonnet/Opus | Repo path `tools/tre-builder/`, package `tre_builder`, CLI name free to be `tresmith`. | Path convention mirrors `tools/tre-compare`; branding is cosmetic. |
| R13 | **What Cursor's leniency notes mean for the writer** | Cursor/Codex | Writer contract = Codex §5 strict form; Cursor tables are layout reference only. | §1.2a. |
| R14 | **Registry in v1** | Sonnet/scope | `source` accepts local path and git URL in v1; `registry:` scheme is *reserved syntax*, unimplemented. | Keeps recipes forward-compatible without building registry infrastructure nobody needs for 4-5 first-party layers. |

---

## 3. SYNTHESIZE — consolidated v1 design brief

### 3.1 Scope cut-line

**v1 ships:**
- TOML recipe (§3.3), exact pins, local-path/git sources, waivers, must-exist smoke list.
- Layer packages: `layer.toml` + `provenance.toml` (closed origin vocabulary) + `data/`
  tree; variant subdirectories for options (R2).
- Compose engine: canonical ingest (reusing `safe_virtual_key`), endomorphism fold over the
  three-arm `Cell` ADT (`Derivation` stubbed), provenance with shadow chains, materialized
  order, deterministic manifest + `manifest_digest`.
- IFF validate + repair as content-addressed derivations, using the vendored field-proven
  clamp repairer (R9).
- Consistency: cohort classes (global table), CONSISTENT witness predicate, COMPLETE on
  touched cohorts, hard/soft reference checks with witness-vector memoization, **baseline
  regression gating** (`V_full \ V_base`), content-pinned waivers, degenerate-shadow lint
  (landmine catalog as lint severities).
- Emit: **loose overlay (OVERLAY mode)** with emit journal + prune, cfg-fragment emission,
  emit-priority admissibility assert (pending the §1.5.1 comparator read), canonical
  lowercase names, Windows path-length/device-name guards.
- Stage-8 round-trip verify via imported `tre_compare.virtual_tree` — fatal, no bypass.
- Reports: `compose-report.json` as contract + rendered text; exit codes 0/1/2/3/4;
  always-dry-run UX with confirm; `verify` standing drift command.
- SHA-256 for durable hashes (stdlib-only stays true); xxh3 only where the existing cache
  already uses it.

**v1 defers:** TRE/TOC writer + packed emit + tombstone realization (milestone 5, spec
already probe-confirmed); registry + semver ranges + lockfile split (arrive together);
`Derivation` transforms / delta layers / renames; provenance CI + two-tier registry;
per-layer archives (`--layered-archives`); COT2000 anything (read-only knowledge, never a
write target).

### 3.2 Modules (`tools/tre-builder/src/tre_builder/`)

| Module | Responsibility |
|---|---|
| `recipe.py` | TOML parse/validate, pin resolution, materialize total order, waiver parsing |
| `layers.py` | Stage 0-1: source resolve + digest, ingest/index via `iter_node_entries` / hardened `_walk_search_path`, intra-layer collision rules (C-17) |
| `fold.py` | Stage 2: the fold (§6.2 of Opus), winners/removed/provenance/shadow |
| `derive.py` | Stages 3-5: structural classify, vendored IFF repair as derivation, re-validate gate |
| `classes.py` | Global cohort class table, `Ki` path→key indexing |
| `consistency.py` | Stage 6: witness predicate, COMPLETE, references, baseline diff, waivers, lints |
| `emit.py` | Stage 7: emit plan, admissibility assert, journal, prune, cfg fragment |
| `verify.py` | Stage 8: round-trip via **imported** `tre_compare` (never vendored) |
| `manifest.py` | canonical JSON, digest vocabulary, report rendering |
| `cache.py` | reuse/extend `tre_compare.cache` sqlite schema (identity `(abspath, mtime_ns, size)`), plus derivation store + verdict memos |

### 3.3 Recipe schema decision

Sonnet's TOML shape, amended per R1/R2/R8:
- `[recipe]` name/version/schema; `[base]` kind + patch-level + `verify = "manifest"|"hash"|"none"`
  (warn-only per R3).
- `[[layer]]`: `id`, `source` (path or git URL; `registry:` reserved), `version` = **exact pin
  only** (v1 rejects range syntax with a "not yet" error, not silence), optional `after`,
  optional `variant` selections (replacing `options` transforms).
- `[[waiver]]`: Opus C-8 semantics in TOML — `violation` id + `pins` (sha256 multiset) +
  mandatory `reason`.
- `[output]`: `form = "loose-override"` (only v1 value; `packed-tre` values parse-reserved),
  `target`, `cfg-fragment`.
- Manifest doubles as the lock: it records materialized order, per-layer digests, base
  fingerprint + verified flag, tool version, class-table digest.

### 3.4 Compose pipeline (adopted from Opus §3, unchanged in structure)

0 Resolve & pin → 1 Ingest & index → 2 Fold (+ C-14 tombstone-realizability abort) →
3 Structural validate → 4 Repair (vendored clamp, idempotent, never-widens) → 5 Re-validate
(everything emitted is structurally valid) → 6 Consistency (cohorts/references, baseline,
waivers, lints) → 7 Emit plan & write (admissibility, journal, prune) → 8 Verify emitted
(engine-faithful round-trip; fatal, unconditional).

### 3.5 Validation gates

- **G1 (Stage 5):** no structurally invalid blob is emitted; `--allow-invalid <path>` is the
  only escape, recorded in the manifest.
- **G2 (Stage 6):** no non-inherited, non-waived error-severity violation. Baseline rule: gate
  on `V_full \ V_base` only. No global bypass for cohort errors.
- **G3 (Stage 7):** emit-priority admissibility + journal-or-refuse on a dirty output dir.
- **G4 (Stage 8):** round-trip equality with `winners`/`removed` — fatal, no flag.
- **CI properties:** P-DET, P-ASSOC, P-IDEM, P-BASE, P-INC, P-ROUND, P-COHORT, P-IDENTICAL,
  P-BASELINE, P-WAIVER, P-PRUNE, P-PRIORITY, P-REPAIR (Opus §7). P-COHORT and P-PRIORITY are
  the first fixtures written.

### 3.6 Build order — first five milestones

| M | Deliverable | Acceptance test |
|---|---|---|
| **M0 (probe, pre-code)** | The §4 decisive experiment | Stock client boots with the hand-built `.tre` mounted; observations logged for items §1.5.1-6 |
| **M1** | `recipe.py` + `layers.py` + `fold.py` + `manifest.py` on `tre_compare` foundations | P-DET (two processes, `PYTHONHASHSEED` randomized, second machine ⇒ equal `manifest_digest`), P-ASSOC, P-BASE on synthetic fixtures |
| **M2** | `emit.py` + `verify.py`: loose overlay, journal+prune, cfg fragment, admissibility, Stage 8 | P-ROUND, P-PRUNE, P-PRIORITY fixtures pass; a two-layer recipe (base + JTL) composes and the client **boots to character select** off the emitted overlay (project boot gate) |
| **M3** | `derive.py`: validate/repair as content-addressed derivations, vendored repairer | P-REPAIR: all 102 known-corrupt ILM files repair idempotently and **byte-match the already-field-verified `stage-B-override` outputs** |
| **M4** | `classes.py` + `consistency.py`: cohorts, baseline, waivers, references, lints, report | P-COHORT (the historical skeleton-without-LOD recipe fails naming the exact missing `.lmg`/`.lod`/`.sat` and a suggested fix), P-IDENTICAL, P-BASELINE, P-WAIVER |
| **M5** | TRE `TREE/0005` writer (+ optional `TOC/0001`), packed emit, tombstone realization | Pack a composed set: client boots from it; `tre-compare` diff packed-vs-loose emit = zero content differences; tombstone fixture verifiably removes a stock path through the running engine (P-ROUND packed variant) |

### 3.7 Standing rules inherited without change

Opus C-1/C-2 (one canonicalization, at ingest), C-5 (shadow-chain provenance), C-7 (every
gate is a regression gate), C-13 (witness vectors for set-dependent checks), C-14/C-15
(tombstones declared, never inferred; degenerate replacement ≠ delete), C-16-C-20 (case,
collisions, canonical emit names, path limits), §4.3 ("no such thing as downstream-layer
invalidation" — say it in the code), §4.5 (`--verify-incremental` in CI). Sonnet's UX
contract: always-dry-run, historical-failure-mode strings in cohort errors, copy-paste
waiver hints, `zzz_` default packed naming.

---

## 4. THE ONE DECISIVE EXPERIMENT

**Hand-write one minimal `.tre` with a ~100-line throwaway script (byte maps are already on
the shelf) and boot the stock client against it.** Contents:

1. an **override** of one known-loaded asset (small texture or datatable) with a visibly
   distinguishable payload — one entry stored (`compressor=0, compressedLength=0`), one
   zlib-compressed;
2. a **zero-length entry** over a stock path whose absence is observable (verifiable in-game
   or via a TreeFile log probe);
3. header/TOC/name-block per Codex strict form (`TREE/0005`, 24-byte entries, CRC-then-stricmp
   sort, lowercase slash paths); build **two variants**, with and without the trailing MD5
   block;
4. mount via `searchTree_XX_Y` in `client_d.cfg`, and also drop a *conflicting loose file* for
   one overridden path under a low-priority `searchPath`.

One boot session then retires, in a single capture: writer byte-format correctness (header,
sort, CRC, name block, stored-vs-compressed field conventions — §1.5.2), MD5-block
optionality (§1.5.3), `0005` acceptance end-to-end (confirming the dead-code finding §1.1 at
runtime), **zero-length tombstone behavior through the real engine** (already confirmed in
source, §1.4 — this is the runtime double-check), and the **loose-vs-tree priority
comparator** (§1.5.1) that gates Stage-7 admissibility. Cost: an afternoon, zero engine
changes, fully reversible cfg edit. It converts milestone M5 from "speculative format work"
into "confirmed spec", settles Opus §8.1's packed-emitter question with running-engine
evidence, and de-risks the single most expensive failure class in the design — the overlay
that silently does nothing.
