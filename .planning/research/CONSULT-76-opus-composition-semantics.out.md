# CONSULT-76 — Composition engine semantics for the layered TRE-set builder

**Consultant angle:** formal composition semantics. Deliberately silent on binary format
layouts, byte-level table structures, and recipe/ecosystem UX — those are other consultants'
lanes. Where I must name a format or a recipe field, I name only the *interface* the
composition engine needs from it and mark it `[FORMAT-LANE]` / `[UX-LANE]`.

**Ground truth consulted (beyond the evidence pack):** the already-shipped, engine-faithful
merge in `D:\Code\swg-client-v2\tools\tre-compare\src\tre_compare\virtual_tree.py`
(`fix_up_file_name`, `safe_virtual_key`, first-hit-wins descending pass, per-node-type
tombstone semantics) and its cache layer in `.../cache.py` (identity `(abspath, mtime_ns,
size)`, sqlite memo tables). **The builder should not invent a second canonicalization or a
second merge; it should reuse these and invert the direction.** That reuse is the single
highest-leverage decision in this document.

---

## 0. The two merges — do not conflate them

There are **two distinct merge semantics** in this system and every bug in a tool like this
comes from mixing them up:

| | **Compose merge** (the builder) | **Runtime merge** (the engine) |
|---|---|---|
| Input | recipe-ordered *layers* | cfg-ordered *search nodes* |
| Order | **base first, later layers WIN** (ascending precedence) | **highest priority first, FIRST HIT WINS** (descending) |
| Output | one flat authoritative map `path → content` | the client's view |
| Implemented by | this spec | `build_virtual_tree()` (already exists) |

The compose merge is **authoritative**: it decides what the player must see. The runtime
merge is a **realization detail** of how the emitted artifacts are mounted. Stage 8 (§3.9)
closes the loop by re-running the *runtime* merge over the emitted artifacts and asserting it
reproduces the *compose* result. That round-trip gate is what makes the whole design safe,
and it is available essentially for free because the reader already exists.

**Naming rule for implementers:** never use the word "priority" for both. Compose uses
`rank` (integer, higher = wins, assigned by recipe position). Runtime uses `priority`
(engine cfg semantics). A code review should reject any function that takes both without
distinct names.

---

## 1. Core model

### 1.1 Path space

Let `P` be the set of **canonical paths**. Canonicalization is *not* a builder invention:

```
canon(raw) = safe_virtual_key(raw)          # -> str | None
           = harden(fix_up_file_name(raw))
```

`fix_up_file_name` is the verbatim engine port (strip leading slashes / `./` / `../`,
backslash→slash, lowercase, collapse repeated slashes). `safe_virtual_key` adds the
non-engine hardening (reject residual interior `..`, drive-letter, UNC, empty).

**Decision C-1 — the builder adopts `safe_virtual_key` as its canonical key function,
unchanged.** Rationale: the composition key space must be a *subset* of the engine's key
space or the tool can produce a set whose winners the engine resolves differently. The
hardening only ever *drops* keys; a dropped key is reported as a rejected source entry, never
silently emitted. Divergence between builder canon and engine canon is the one class of bug
that Stage 8 would catch only probabilistically, so we eliminate it by construction.

**Decision C-2 — canonicalization happens exactly once, at layer ingest (Stage 1).** Every
downstream stage, cache key, cohort rule, reference edge, manifest row and report line uses
canonical paths only. Raw source paths survive only in the provenance record, for reporting.

### 1.2 Cells

A composed map is `M : P ⇀ Blob`. A *cell* is what a layer says about one path:

```
Cell ::= Content(blob_hash, attrs)      -- this layer provides these bytes
       | Tombstone                      -- this layer removes the path
       | Derivation(transform, params)  -- this layer rewrites whatever is currently there
```

`attrs` is a small, closed, deterministic record: `{source_ref, exec_bit?: no, declared_kind}`.
It deliberately contains **no timestamps and no filesystem metadata** (§1.6 D-3).

`Derivation` is **specified now, not implemented in v1** (§5.6). It exists in the algebra so
v1 does not paint itself into a corner.

### 1.3 Layers as endomorphisms — the actual core model

The naive model ("a layer is a partial map; merge is `dict.update`") cannot express tombstones
or deltas without special cases. Use instead:

> **A layer is a function from composed maps to composed maps.**

```
⟦L⟧ : (P ⇀ Blob) → (P ⇀ Blob)

⟦L⟧(M) = M', where for each p ∈ P:
    L(p) = undefined        ⇒  M'(p) = M(p)                        (pass through)
    L(p) = Content(b, a)    ⇒  M'(p) = b                           (override)
    L(p) = Tombstone        ⇒  M'(p) = ⊥ (p ∉ dom M')              (remove)
    L(p) = Derivation(t, k) ⇒  M'(p) = t(M(p), k)  [error if p ∉ dom M]   (patch)
```

Composition of layers is **function composition**:

```
compose([L₁ … Lₙ]) = ⟦Lₙ⟧ ∘ … ∘ ⟦L₁⟧   applied to  M₀ = ∅
```

Properties (all directly testable):

- **Associativity** — function composition is associative, so `(L₁▷L₂)▷L₃ = L₁▷(L₂▷L₃)`.
  Grouping layers into a *layer bundle* (an ILM bundle = lighting + sound) is therefore sound
  and changes nothing. This is what makes bundles safe.
- **Identity** — the empty layer is the identity; `(Layers, ▷, ∅)` is a **monoid**, not a
  commutative one.
- **Non-commutativity** — deliberate; order is meaning. Therefore recipe order must be
  *total and explicit* (§1.4).
- **Not idempotent in general** once `Derivation` exists (`t` may not be idempotent);
  with only `Content`/`Tombstone`, `⟦L⟧ ∘ ⟦L⟧ = ⟦L⟧` holds and should be a property test.
- **Absorption** — `Content` and `Tombstone` are *absorbing*: the result does not depend on
  `M(p)`. Only `Derivation` reads the accumulator. This is exactly why v1 (Content+Tombstone
  only) admits the cheap per-path incremental model in §4, and why `Derivation` costs more.

For v1 (no `Derivation`), `⟦L⟧` degenerates to the familiar last-writer-wins override, and the
whole fold can be computed **path-locally**:

```
winner(p) = the highest-rank layer L with p ∈ dom(L)
M(p)      = ⊥ if that cell is Tombstone, else its blob
```

**Decision C-3 — v1 implements the path-local form but through the `Cell` ADT and the fold
signature above**, so adding `Derivation` later is a new arm in one `match`, not a rewrite.

### 1.4 Recipe order

**Decision C-4 — the recipe declares a total order as an explicit list.** Layer *rank* = index
in that list; base is rank 0. If the recipe additionally supports dependency edges
(`requires`, `after`) `[UX-LANE]`, the engine must:

1. topologically sort;
2. break every tie by **declaration order**, then by layer id (byte-lexicographic ASCII);
3. **materialize the resolved total order into the output manifest** as an explicit list.

A recomposition must reuse the *materialized* order from the manifest when the recipe is
unchanged. Rationale: a toposort that is only "a valid order" makes composition
non-deterministic across tool versions the moment the sort implementation changes. The
materialized order is the contract; the toposort is a convenience for authoring.

Cyclic `after` edges = **abort**, report the cycle as an ordered path.

### 1.5 What composition emits

Composition produces a **ComposedSet**, which is more than the map:

```
ComposedSet = {
  order      : [LayerId]                      # the materialized total order
  winners    : {canon_path -> WinnerRecord}
  removed    : {canon_path -> LayerId}        # tombstoned, with who did it
  provenance : {canon_path -> Provenance}
  diagnostics: [Diagnostic]                   # sorted, deterministic
}

WinnerRecord = { layer: LayerId, content_hash: Hash, size: int,
                 derivations: [DerivationId] }   # e.g. ["iff-clamp-repair@1"]

Provenance   = { layer, source_ref, raw_path, source_content_hash,
                 shadowed_by_rank: [(LayerId, Hash)] }   # every lower contributor, in order
```

**Decision C-5 — provenance records every shadowed contributor, not just the winner.** It
costs one tuple per override and it is what makes the consistency reports (§2), the "redundant
layer" lint (§5.5) and post-hoc forensics ("which layer killed the fog?") possible at all.
The known landmine catalogue is exactly a query over this structure.

### 1.6 Determinism — the requirements, enumerated

**Definition.** Composition is deterministic iff: same recipe + same pinned source content +
same tool version ⇒ **byte-identical ComposedSet manifest** and **byte-identical emitted
payloads**.

Note the manifest, not the directory: filesystem mtimes are not content and must be excluded
from the definition (§D-3). The testable artifact is `manifest_digest = H(canonical manifest
bytes)`.

| ID | Requirement | Hazard it kills | Mitigation |
|---|---|---|---|
| D-1 | Total, materialized layer order | toposort drift | §1.4 |
| D-2 | All iteration over `P` is sorted by canonical path, byte-lexicographic | Python dict / `os.walk` order | sort at every emit + report boundary; `os.walk` results already sorted in `_walk_search_path` |
| D-3 | No wall-clock, no mtime, no uid, no locale, no cwd, no absolute host paths in any hashed or emitted artifact | "works on my machine" digests | manifest schema forbids them; emitted archive timestamp fields (if any) are pinned to a constant `[FORMAT-LANE]` |
| D-4 | Compressor identity is pinned and recorded | zlib version/level changing bytes across machines | record `{codec, level, impl_version}` in the manifest; **v1 loose-dir emit sidesteps this entirely** |
| D-5 | Repairs are pure functions of input bytes + params, versioned by id | silent repair-algorithm drift | `DerivationId = "name@version"`, participates in hashing (§4.2) |
| D-6 | Parallelism only in pure per-file work; all merges/reductions are ordered | thread-race ordering in diagnostics | workers return `(path, result)`, reduce in sorted order |
| D-7 | Source pinning: every layer resolves to a content digest, verified before compose | "same recipe" against a mutated install | Stage 0 (§3.1) |
| D-8 | Case collapse is deterministic and collisions abort | two source files differing only in case | §5.3 |
| D-9 | Floating point never participates | — | trivially satisfied; state it so no one adds a heuristic score |

**Test obligation P-DET:** compose twice in one process, compose once in a fresh process with
`PYTHONHASHSEED` randomized, compose on a second machine; all three `manifest_digest` values
must be equal. Run in CI.

---

## 2. The consistency-constraint system

### 2.1 The problem, stated precisely

The field failure — replacing `.skt` skeletons without their paired `.lmg`/`.lod`/`.sat`
made NPCs invisible beyond ~10m — is **not** a per-file property. Each file was individually
well-formed. The failure is a property of a *set* of paths whose winners came from different
layers. The constraint system must therefore be defined over **cohorts**, not files.

### 2.2 Cohorts

A **paired-asset class** `K` is a declaration:

```
class:
  id: "creature-appearance-set"
  members:                       # each = a role with a path pattern + key extraction
    - role: skeleton    pattern: "appearance/skeleton/{key}.skt"       required: true
    - role: lodmesh     pattern: "appearance/lod/{key}.lmg"            required: false
    - role: mesh        pattern: "appearance/mesh/{key}_l*.msh"        required: false
    - role: sat         pattern: "appearance/{key}.sat"                required: false
  strength: same-origin           # same-origin | same-origin-or-compatible | advisory
  compat: null                    # checker id, only for -or-compatible
```

Formally a class induces a partial **cohort key function** `Kᵢ : P ⇀ CohortKey` (pattern match
with a `{key}` capture). A **cohort** is `C(k) = { p ∈ dom(M) ∪ removed : Kᵢ(p) = k }` — note
it includes tombstoned paths, because "one layer deleted the mesh and another kept the
skeleton" is exactly the failure mode.

`[FORMAT-LANE]` owns the actual pattern table (which extensions truly pair). The composition
engine only requires that a class is expressible as *(pattern → key capture, role, required)*.
That interface is the deliverable here.

### 2.3 The consistency predicate — the central definition

Naive rule ("all members must come from the same layer") is **wrong**: it fires on the very
common case where two layers ship byte-identical copies of a member, and it fires on a layer
that legitimately ships the full set while an unrelated later layer re-ships one identical
file. Use content, not identity:

> **CONSISTENT(C)** ⟺ ∃ a layer `L` in the recipe such that for every `p ∈ C`:
> `winner_content(p) ≡ L(p)`
>
> where `≡` means: both are `Content` with equal content hash, **or** both are absent
> (`p ∉ dom(L)` and `p` tombstoned/absent in the composed set).

In words: **the composed cohort must be byte-reproducible by some single layer acting alone.**
That single sentence subsumes:

- pure same-layer wins → consistent (that layer is the witness);
- byte-identical duplicate across layers → consistent (either layer is a witness);
- base untouched → consistent (base is the witness);
- **skeleton from ILM + mesh from base → INCONSISTENT** (no single layer reproduces the pair) —
  precisely the field failure, caught;
- a layer that ships a skeleton and *nothing else* while base has the meshes → inconsistent,
  caught, with the fix ("layer must carry `X`, `Y`") derivable from the witness search.

**Decision C-6 — CONSISTENT as defined above is the v1 constraint.** It requires only content
hashes, which the engine already computes for other reasons, and it needs no semantic
understanding of the file formats.

`strength` modulates the verdict, not the predicate:

| strength | not CONSISTENT ⇒ |
|---|---|
| `same-origin` | **ERROR, abort compose** |
| `same-origin-or-compatible` | run `compat` checker on the winner set; pass ⇒ warn+annotate, fail ⇒ ERROR |
| `advisory` | WARN only, recorded in the manifest |

Additionally, per-class **COMPLETE(C)**: every `required: true` role has a resolvable member.
A cohort that lost its skeleton entirely (tombstoned by a layer, no replacement) fails
COMPLETE ⇒ ERROR. COMPLETE is checked *only for cohorts the recipe touched* (see §2.5) — stock
data is full of incomplete cohorts and failing on those makes the tool unusable.

### 2.4 Detection algorithm

Runs in Stage 6, after repair, over the composed set. Linear, single pass, no format parsing:

```
for each class Ki:
    buckets = {}
    for p in sorted(winners.keys() | removed.keys()):
        k = Ki(p);  if k is None: continue
        buckets.setdefault(k, []).append(p)

    for k, members in sorted(buckets.items()):
        touched = any(provenance[p].layer != BASE for p in members if p in winners) \
                  or any(p in removed for p in members)
        if not touched: continue                        # untouched stock cohort: skip (§2.5)

        witnesses = [L for L in order if reproduces(L, members)]   # the ∃ search
        if not witnesses:
            emit Violation(kind="cohort-split", class=Ki.id, key=k, members=...)
        if not complete(Ki, members):
            emit Violation(kind="cohort-incomplete", ...)
```

`reproduces(L, members)` is `all(cell_matches(L.cell(p), composed_state(p)) for p in members)`.
Cost: `O(|touched cohorts| × |layers| × |members|)`; layers are ~10 and members ~4, so this is
noise next to I/O. The `touched` filter keeps it proportional to the diff, not the install.

`witnesses` is retained on success too — the *set* of witnesses is a useful diagnostic
("this cohort is reproducible by base and by ILM ⇒ ILM's copy is redundant").

### 2.5 The baseline rule (why this system does not drown in false positives)

**Decision C-7 — every consistency gate is a REGRESSION gate, not an absolute gate.**

Compute violations twice:

- `V_base` = violations of the base-only compose (`order = [base]`),
- `V_full` = violations of the full compose.

**Gate on `V_full \ V_base` (matched by stable violation id, §2.8).** Pre-existing stock
breakage is reported at INFO with an explicit "inherited" flag and never blocks. Anything the
recipe *introduced* blocks per its strength.

`V_base` is a pure function of the pinned base ⇒ compute it once and cache it forever under
the base digest (§4). This is cheap and it is the difference between a tool that ships and a
tool that cries wolf on retail data.

### 2.6 Cross-file reference consistency

A **referencer** is a per-format plugin `refs(blob) → [(target_raw_path, kind)]`. `[FORMAT-LANE]`
owns which formats have referencers and how to parse them. The composition engine requires
only this signature, plus a `kind` from a closed set:

| kind | meaning | dangling ⇒ |
|---|---|---|
| `hard` | engine will fail/void the asset if missing | ERROR (subject to §2.5 baseline) |
| `soft` | engine degrades gracefully (optional LOD, optional sound) | WARN |
| `weak` | string that *may* be a path (heuristic extraction) | INFO only, never blocks |

Rules:

- **R-1 — cross-layer references are ALLOWED by default.** A file from layer *ILM* referencing
  a path served by *base* is the normal, intended case; forbidding it would forbid layering.
  Only cohort classes (§2.2) constrain cross-layer coupling, and they do so by *declaration*,
  not by inference.
- **R-2 — a dangling `hard` reference is an error iff it is new** (`∈ V_full \ V_base`). New =
  either the referencing file is contributed by a non-base layer, or its target was removed /
  changed such that the reference no longer resolves.
- **R-3 — reference *targets are resolved against the composed set*, including tombstones.**
  A path removed by a tombstone is not a valid target. This is the only way a tombstone's blast
  radius is visible before runtime.
- **R-4 — reference resolution uses `canon()`.** Assets reference each other with mixed case
  and backslashes; anything else produces phantom danglers.
- **R-5 — "version-coupled" references are a cohort class, not a reference rule.** If format
  analysis finds that A embeds a *structural assumption* about B (bone counts, index tables),
  that pair is declared as a paired-asset class with `same-origin`, and the ordinary cohort
  machinery handles it. Do not add a second mechanism.
- **R-6 — reference extraction is opt-in per class of file and runs only on
  contributed-or-affected files** (§4.4), never on the whole install, or the tool's runtime
  becomes dominated by parsing.

### 2.7 Waivers

`same-origin` violations must be escapable, or a legitimate hand-verified mix is impossible.

**Decision C-8 — waivers are content-pinned and self-expiring:**

```
waivers:
  - violation: "cohort-split:creature-appearance-set:rodian_male"
    pins: ["sha256:ab12…", "sha256:cd34…"]     # the exact winner hashes waived
    reason: "verified in-game 2026-08-16; ILM skt is bone-compatible with stock lmg"
```

A waiver applies **only** if the violation id matches **and** the multiset of winner content
hashes equals `pins`. Any content change on either side re-arms the error. A waiver without a
non-empty `reason` is a recipe validation error. Waivers are recorded in the output manifest,
so a shipped set carries the list of things someone chose to override — that is the audit trail.

### 2.8 The failure report — required contents

Machine-readable JSON (`compose-report.json`) plus a rendered text view; **the JSON is the
contract**, the text is generated from it.

Every violation record MUST contain:

1. `id` — **stable violation id**: `"{kind}:{class_or_rule}:{cohort_key_or_path}"`, canonical
   and stable across runs (this is what waivers and the baseline diff key on). Never include
   a hash or a count in the id, or waivers break on every content change *by construction*
   rather than by intent.
2. `kind`, `rule_id`, `severity` (`error|warn|info`), `inherited: bool` (from §2.5).
3. `cohort_key` / `path`.
4. **Member table**, one row per member, sorted by path:
   `{path, state: present|removed|missing, winner_layer, content_hash,
     why_won: "rank N overrides {lower layers}", contributed_by: [layers]}`.
5. `witness_search` — for each layer, *why it failed to be a witness*: the first member path
   where it diverged and how (`absent-in-layer` / `hash-mismatch` / `present-but-removed`).
   This is the field that turns a report into a fix.
6. `suggested_fix` — derived mechanically from (5): the layer with the fewest divergences plus
   the exact list of paths it would need to also carry (or tombstone) to become a witness.
7. `provenance_excerpt` — the shadow chain for each member.
8. `waiver_hint` — the exact YAML block a user would paste (id + current pins), so waiving is
   copy-paste, not archaeology.

Report-level requirements: violations sorted by `(severity, kind, id)`; counts summarized;
`manifest_digest` of the attempted compose included; **the report is written even on abort**
(that is the whole point). Exit code distinguishes `0` clean, `1` warnings only (if
`--strict` then also non-zero), `2` consistency errors, `3` structural/repair failure, `4`
recipe/source resolution failure.

---

## 3. The pipeline — stages, invariants, abort-vs-warn

Each stage states the invariant it **guarantees to the next**. A stage may only assume its
predecessors' invariants; violating that is how "the repairer sees uncanonicalized paths" bugs
happen.

### 3.0 Stage table

| # | Stage | Guarantees to next | Abort conditions |
|---|---|---|---|
| 0 | Resolve & pin | every layer has a verified source digest | missing source, digest mismatch, cycle in order, unknown class/rule id |
| 1 | Ingest & index | per-layer `dom(L)` in canonical keys; no content read yet | intra-layer canonical collision (§5.3); layer id collision |
| 2 | Fold | `ComposedSet.winners/removed/provenance` complete and total | tombstone unrealizable in target emit mode (§5.1) |
| 3 | Structural validate | every winner blob parses as a well-formed container **or** is flagged repairable/opaque | never aborts on its own — defers to Stage 4/5 |
| 4 | Repair | repaired blobs exist as derivations with new hashes | repair transform raised / produced non-fixpoint |
| 5 | Re-validate | **every emitted blob is structurally valid** | any blob still invalid after repair |
| 6 | Consistency | cohorts + references satisfied modulo baseline & waivers | any non-inherited, non-waived `error` |
| 7 | Emit plan & write | output tree matches `winners` exactly; stale files pruned | emit-priority inadmissible (§3.8); path too long; write failure |
| 8 | Verify emitted | engine-faithful re-read reproduces `winners` | any mismatch — **always fatal** |

### 3.1 Stage 0 — Resolve & pin

Turn the recipe into a fully pinned plan: each layer → a concrete source (install dir, archive
set, extracted tree) → a **layer digest**. Layer digest = Merkle root over sorted
`(canon_path, source_content_hash)` (§4.1). For archive-backed layers, the index comes from the
existing per-archive sqlite cache keyed on `(abspath, mtime_ns, size)`; the *layer digest* is
computed from content hashes, not mtimes, so it is portable.

Validate the recipe here: unknown class ids, unknown rule ids, waivers referencing nonexistent
rules, cycles. All are aborts, all reported together (do not fail on the first).

### 3.2 Stage 1 — Ingest & index

Build `dom(L)` and the raw→canon mapping. **No blob bytes are read** beyond what hashing
requires (and hashing is memoized). Reuse `iter_node_entries` for archive layers and the
hardened `_walk_search_path` for directory layers — including its symlink/reparse pruning,
which the builder needs for exactly the same reasons the comparer did.

Intra-layer duplicate canonical keys: see §5.3 (abort for directory layers, engine-faithful
first-hit + WARN for archive layers).

### 3.3 Stage 2 — Fold

The §1.3 fold. Pure, in-memory, path-local. Output: `winners`, `removed`, `provenance`.
**No content is read here either** — winners carry hashes, and blobs are fetched lazily by
later stages. On a 100k-file set this stage should be milliseconds.

### 3.4 Stages 3–5 — validate → repair → re-validate

**Placement decision C-9 — validation and repair are modelled as a per-file *derivation*, not
as a pipeline pass over the whole set.**

```
repaired_blob = derive("iff-clamp-repair@1", source_blob)
```

Because the derivation is a pure function of the input bytes, it is:

- **content-addressed and cacheable forever** — repairing the same 102 ILM files across ten
  recipes costs one run, ever (§4.2);
- **independent of the merge**, so it can be computed before, during or after the fold without
  changing the result;
- **executed lazily, for winners only** — never waste work repairing a file that loses.

The *stages* 3/4/5 are therefore the **scheduling** of that derivation over
`winners`, plus the gate:

- **Stage 3** classifies each winner blob: `valid` / `repairable(reason)` / `invalid(reason)` /
  `opaque` (no validator for this format — the common case; opaque is not a failure).
- **Stage 4** applies the clamp-to-parent-extent + recursive-validate repair to every
  `repairable`. Requirements on the transform:
  - **idempotent**: `t(t(x)) = t(x)` — assert it in the derivation (cheap: re-validate output);
  - **fixpoint-terminating**: bounded iteration count, abort if not converged;
  - **never widens**: repair may only *shrink* declared sizes to fit the parent extent, never
    grow them, and never touch payload bytes. A repair that would move payload bytes is
    out of scope and must abort, not guess.
- **Stage 5** re-validates. Invariant handed forward: **everything that will be emitted is
  structurally valid.** A still-invalid file is `ERROR` — with one escape hatch,
  `--allow-invalid <path>` (recorded in the manifest), because the alternative is that one
  unrepairable stock oddity blocks an entire set.

**Warn-not-abort at this layer:** `opaque` files; files that were repaired (WARN with the
byte-level delta summary — a repair is a modification of third-party data and must be visible);
declared-size mismatches that are *smaller* than payload (a different corruption class than the
known one — WARN and mark `invalid` unless a rule says otherwise).

**Landmine / preference-kill classification is NOT part of validation.** A 240-byte stub
texture is perfectly valid IFF; it is a *policy* signal. It belongs in Stage 6 as a lint rule
whose severity comes from the landmine catalogue, and whose default is WARN with a very loud
message when the stub *shadows* a real base asset (that pattern —
`layer overrides base with a degenerate-size file` — is mechanically detectable from
provenance + size, without any catalogue at all, and should be a built-in lint).

### 3.5 Stage 6 — Consistency

§2. Requires Stage 5's invariant because reference extraction parses blobs, and parsing a
corrupt-header file yields garbage edges. That ordering is the reason repair precedes
consistency, and it should be documented in the code.

### 3.6 Stage 7 — Emit plan

Two modes:

- **FULL** — emit every winner; the output is a self-contained set.
- **OVERLAY** — emit only winners whose provenance ≠ base; the pinned base install stays
  mounted underneath. This is the mode the recipe-not-bytes constraint wants, and the mode
  that keeps output tiny.

**View-preservation theorem for OVERLAY.** Skipping a base-provenance winner `p` is safe iff
the base node still serves `p` with identical content and no node between the overlay and the
base serves `p`. With exactly two nodes (overlay above base) this holds by construction.
It **fails** if the target cfg mounts anything between them — hence:

### 3.7 Emit-priority admissibility (the trap)

Engine read priority is: **loose searchPaths > `searchTree_XX_Y` tres > searchTOC-resolved**.
Therefore *a packed `.tre` overlay cannot override a loose base file*, no matter what
priority number it is given.

**Decision C-10 — Stage 7 parses the target cfg with the existing scanner and asserts
`min(engine_priority(emitted nodes)) > max(engine_priority(all base/other nodes))` in the
engine's own ordering (including the node-kind tiebreak `path < tree < toc`).** If the
assertion fails, abort with the specific offending node and the concrete remedy (switch emit
mode to loose, or raise `searchTree_XX_Y`). A naive implementation will skip this and produce
an overlay that silently does nothing for a subset of paths — the most expensive possible
failure, because it looks like it worked.

**Decision C-11 — v1 emits a FLATTENED set (winners only), never one archive per layer.**
Per-layer archives recreate the ordering problem at runtime, put the authority back in the
cfg, and make Stage 8 a much weaker check. A `--layered-archives` mode may exist later; it
must emit in exact rank order and still pass Stage 8.

**Emit journal.** The output directory is a *managed* tree with a journal
(`.compose-journal.json`: `path → emitted_content_hash`, plus the previous `manifest_digest`).
Emitting must **prune** every journaled path absent from the new plan. Without pruning, a
stale override file from a previous recipe outranks everything and poisons the run — this is
the same class of bug as the emit-priority trap and is just as invisible. If the journal is
missing but the directory is non-empty, refuse (`--force-clean` to wipe).

### 3.8 Stage 8 — Verify emitted (the round-trip gate)

Construct the runtime node list that the *target cfg* will produce (base + emitted overlay),
run **`build_virtual_tree()` / `build_virtual_tree_cached()` unchanged**, and assert:

- for every `p ∈ winners`: the engine-resolved winner's content hash equals
  `winners[p].content_hash`;
- for every `p ∈ removed`: `p` is absent from `VirtualTree.entries`;
- `VirtualTree.rejected` contains nothing we intended to emit;
- `VirtualTree.node_errors` is empty for our emitted nodes.

Any mismatch is **fatal and unconditional** — no flag disables it. This single stage converts
every wrong assumption about engine precedence, canonicalization, tombstones and priority into
a build-time failure instead of an in-game mystery. It is also nearly free to build, because
the reader already exists and is already engine-verified.

---

## 4. Incremental rebuild

### 4.1 Hash vocabulary

```
H          = BLAKE3 or SHA-256 for durable/manifest hashes; xxh3_64 permitted ONLY for
             in-process memoization (matching the existing cache's algo field).
             Decision C-12: manifest and waiver pins use a cryptographic hash — waivers are
             a security-relevant pin and xxh3 collisions there are unacceptable.

content_hash(blob)   = H(bytes)
derivation_hash(d)   = H("drv/1", tool_version, transform_id, canonical_params, input_hash)
layer_digest(L)      = H("layer/1", sorted[(canon_path, cell_tag, content_hash|"")])
recipe_digest(R)     = H("recipe/1", materialized_order, per-layer layer_digest,
                          class table digest, waiver list digest, policy flags)
winner_hash(p)       = the final (post-derivation) content hash of p
compose_digest       = H("compose/1", recipe_digest, sorted[(p, winner_hash(p))],
                          sorted[removed], tool_version)
manifest_digest      = H(canonical JSON of the manifest)
```

Every hash input begins with a **domain-separation tag** (`"layer/1"` etc.). Bump the tag
version whenever the hashed structure changes; that is the global cache-invalidation lever.

### 4.2 Cache tables

| Cache | Key | Value | Invalidated by |
|---|---|---|---|
| **Source index** | `(abspath, mtime_ns, size)` — reuse the existing `archive_meta`/`archive_entry` schema | entry rows | file identity change |
| **Content hash memo** | `(abspath, mtime_ns, size, vpath, algo)` — the existing `file_hash` table | hexdigest | file identity change |
| **Derivation store** | `derivation_hash` | repaired blob + validation verdict | tool_version / transform version bump |
| **Cohort verdict memo** | `H(class_id, cohort_key, sorted[(p, state, winner_hash)])` | verdict + witnesses | any member's winner hash or state |
| **Reference edge memo** | `winner_hash(p)` (edges depend only on the blob) | `[(canon target, kind)]` | that file's content |
| **Reference verdict memo** | `(winner_hash(p), witness_vector)` — see §4.4 | verdict | any consulted target's presence flipping |
| **Baseline violations** | `layer_digest(base) + class-table digest` | `V_base` | base or class table change |
| **Emit journal** | output dir | `path → emitted hash`, prev `manifest_digest` | every emit |

The existing sqlite cache already implements rows 1–2 with the right identity semantics
(integer `mtime_ns`, WAL, write lock, `INSERT OR IGNORE` for concurrent misses). **Reuse that
module rather than writing a second cache.**

### 4.3 What a one-layer change actually invalidates

Change layer `Lⱼ` (content or membership). Then:

1. `layer_digest(Lⱼ)` changes ⇒ `recipe_digest` changes. **This must NOT invalidate everything.**
   Achieve that by never keying per-file work on `recipe_digest` — per-file caches key on
   *content*, only the top-level `compose_digest` keys on the recipe.
2. Recompute the fold. It is path-local and cheap; **always recompute it in full** rather than
   trying to patch it — it is milliseconds and a wrong incremental fold is a nightmare bug.
3. Diff `winners_old` vs `winners_new` → `Δ = {added, removed, hash-changed}`.
4. Derivations: only paths in `Δ` can need new derivations, and even those hit the derivation
   store if the same bytes were repaired before.
5. Cohorts: recompute only cohorts whose member set intersects `Δ` (index `path → cohort keys`
   once, at class-table load).
6. References: recompute edges only for `Δ`; recompute *verdicts* per §4.4.
7. Emit: write only `Δ.added ∪ Δ.changed`; delete `Δ.removed` per the journal.

**Non-goal:** invalidating "downstream layers". There is no such thing — the fold is
path-local in v1 (§1.3, absorption), so a change in `Lⱼ` can only affect paths in
`dom(Lⱼ) ∪ dom(Lⱼ_old)`. Say this out loud in the code, because the instinct to re-run
everything above rank *j* is strong and wrong. (It becomes *right* only when `Derivation`
cells land — see §5.6.)

### 4.4 Incremental validation of set-dependent checks (the hard part)

Reference checks are not path-local: whether `A`'s reference dangles depends on whether `B`
exists *anywhere* in the composed set. Naive incrementality here is unsound.

**Decision C-13 — record a witness vector.** When checking file `p`, log every target presence
the check consulted: `witness = sorted[(target_path, present: bool)]`. Cache the verdict under
`(winner_hash(p), H(witness))`. On recompose, replay the witness against the new composed set:
if every recorded `(target, present)` still holds, the cached verdict is valid; otherwise
recheck. This is the standard discovered-dependency technique from build systems, it is sound,
and it is cheap (the witness is a handful of paths per file).

The same technique covers COMPLETE checks and the landmine lints.

### 4.5 Equivalence obligation

**Property P-INC: `compose_incremental(state, R) == compose_clean(R)`**, compared on
`manifest_digest`. Ship a `--verify-incremental` mode that runs both and diffs, and run it in
CI on a scripted sequence of layer mutations (toggle a layer, edit one file, delete one file,
rename one file, change class table, add a waiver). Any incremental cache design that cannot
be checked this way should not ship; this check is what lets you trust §4.4.

---

## 5. Edge cases — rule or explicit out-of-scope

### 5.1 Deletions / tombstones

**In scope for v1 as a model concept; realizability is emit-mode dependent — and that must be
checked at Stage 2, not discovered at Stage 7.**

- Semantics: `Tombstone` sets the cell absent; a **higher-rank layer may resurrect** the path
  with `Content` (the fold handles this with no special case).
- Realization: with a read-only base you cannot unlink a file from someone else's archive. The
  options are:
  - **Loose-dir overlay (v1 default): a delete is NOT expressible.** A loose file cannot mask
    a lower file into absence.
  - **Packed `.tre` overlay: expressible** — the engine treats a **length-0 entry in a `.tre` as
    a global remove** (verified in `virtual_tree.py`, per-node-type tombstone semantics; note
    the contrast: a length-0/offset-0 entry in a `.toc` means "absent here, keep searching" and
    is **not** a tombstone — never emit a delete through a TOC).
  - **Full rebuild of the base set: trivially expressible** (just omit the path) but conflicts
    with recipe-not-bytes for redistribution and is a heavier operation.
- **Rule C-14:** Stage 2 checks `removed ≠ ∅ ⇒ emit_mode supports tombstones`; if not, **abort**
  naming the paths and the required mode. Do not silently drop deletes, and do not emulate a
  delete.
- **Rule C-15 — a degenerate replacement is not a delete.** Overriding a texture with a
  240-byte stub is `Content`, tracked as a replacement, and it trips the degenerate-shadow lint
  (§3.4). Conflating the two would make the landmine catalogue unexpressible.

### 5.2 Renames

**Out of scope as a first-class operation in v1.** Model a rename as `Tombstone(old) +
Content(new)`, which the algebra already expresses.

Rationale: a first-class rename would have to rewrite *references* inside other files to keep
the set coherent — that is a content-rewriting transform (a `Derivation`), which v1 does not
implement (§5.6). Meanwhile the reference checker (§2.6) already catches the consequence:
every file still pointing at `old` produces a *new* dangling `hard` reference and blocks the
build with an exact list. That is the correct v1 behaviour: refuse, and tell the author which
files their rename broke. Revisit when `Derivation` lands.

### 5.3 Case and path-separator normalization

- **Rule C-16:** the canonical key is `safe_virtual_key` (lowercased, forward slashes,
  collapsed) — decision C-1.
- **Rule C-17 — intra-layer canonical collision:**
  - *directory-backed layer*: two files colliding under `canon` (`Foo/Bar.SKT` and
    `foo/bar.skt`, or `a//b` and `a/b`) ⇒ **ABORT**, listing both raw paths. Which one wins
    would depend on filesystem enumeration order ⇒ non-deterministic ⇒ violates D-8. There is
    no defensible automatic choice.
  - *archive-backed layer*: duplicates inside one archive are resolved **engine-faithfully
    (first entry wins)** and reported as WARN. Aborting here would reject real stock archives;
    the engine's own rule is deterministic, so determinism is preserved.
- **Rule C-18 — cross-layer collision is not a collision.** It is an override; that is the
  feature.
- **Rule C-19 — emit always writes the canonical (lowercase, forward-slash) name.** On NTFS
  (case-insensitive, case-preserving) writing the original casing would create a file whose
  name differs from the key the engine will lowercase-match — harmless in practice, corrosive
  in diffs and journals. One name, everywhere.
- **Rule C-20 — path length:** loose emit must check `len(output_root + '/' + path)` against the
  platform limit and abort with the offending path (and suggest a shorter output root or the
  packed emitter, which is immune). Deep `appearance/...` trees plus a deep output root will hit
  this on Windows.
- Reserved Windows device names (`con`, `aux`, `nul`, `prn`, `com1…`) as a path segment: abort
  with the path and recommend packed emit. Rare, but it is a hard OS failure with a confusing
  error if unhandled.

### 5.4 Identical-content collisions

**Never an error.** By §2.3, byte-identical members cannot break a cohort (the lower layer
remains a witness). Two positive uses:

- **Redundant-override lint (INFO):** winner content hash equals the shadowed content hash ⇒
  this layer entry changes nothing. Aggregated per layer.
- **Empty-effective-layer lint (WARN):** *every* entry of a layer is a redundant override ⇒ the
  layer is a no-op against this base (usually means it was authored against a different patch
  level). This is a genuinely valuable early signal that the base pin is wrong, and it costs
  nothing to compute.

### 5.5 Layer identity and other degenerate recipes

- Duplicate layer id in one recipe ⇒ **abort** (ids key the manifest, provenance and waivers).
- The *same source* mounted twice under different ids ⇒ allowed, WARN (legal, occasionally
  intended, usually a mistake).
- A layer with an empty domain ⇒ WARN (`layer resolved to 0 files` — almost always a wrong path).
- A layer whose domain is disjoint from base and from all other layers ⇒ fine, INFO (pure
  additive content, e.g. JTL).

### 5.6 Delta / patch layers

**Out of scope for v1 — implemented as `Cell::Derivation`, specified but not built.**

Rationale, in order of weight:

1. **Authoring has no stable diff format for these binaries.** A byte-delta against a specific
   base is a *bytes* artifact in disguise and it silently violates recipe-not-bytes unless the
   delta is semantic (e.g. "set this datatable column to X"), which requires per-format writers
   that do not exist yet (`[FORMAT-LANE]`: no TRE/TOC writer, no IFF editor).
2. **It breaks path-locality** (§1.3 absorption). Once a cell reads the accumulator, a change
   in layer *j* can affect the result of layer *k>j* on the same path, so §4.3's "only
   `dom(Lⱼ)` is affected" claim weakens to "only paths in `dom(Lⱼ)` *and their derivation
   chains*". Implementable (hash the chain: `derivation_hash` already takes `input_hash`), but
   it is a second incremental-correctness surface.
3. **It complicates the consistency predicate.** §2.3's witness search asks "can a single layer
   reproduce this cohort?"; with derivations the honest question becomes "can a single
   *prefix-closed chain* reproduce it?", and the reports get much harder to read.

What v1 must do to stay open: keep `Cell` a three-arm ADT with `Derivation` present and
`unimplemented!()`, keep `derivation_hash` taking `input_hash`, and keep the fold written as a
left fold over cells rather than a `dict.update`. That is a few lines of discipline now versus
a rewrite later.

### 5.7 Miscellaneous rules worth pinning

- **Zero-byte source files are real files, not tombstones.** Tombstones are declared in the
  recipe/layer manifest only — never inferred from size, never from a magic filename like
  `foo.skt.deleted`. Inference is ambiguous against stock data and un-auditable.
- **Symlinks / junctions / reparse points in directory layers are skipped and recorded**, reusing
  the existing `_is_reparse_or_link` hardening. Escape-safe by construction.
- **Directory layers are snapshotted at Stage 1** — like the comparer's eager walk, the builder's
  view is a static manifest, not a live directory. Concurrent edits during compose are not
  observed; the layer digest pins what was seen, and Stage 8 re-reads what was written.
- **Empty directories are never emitted.** They carry no engine meaning and pollute journals.
- **The composed set must be non-empty** and must contain the paths named in a small,
  configurable `must_exist` smoke list (a handful of well-known engine-critical paths) — a cheap
  guard against a catastrophically mis-pinned base. WARN by default, ERROR under `--strict`.

---

## 6. Implementation sketch

### 6.1 Types

```python
LayerId   = str
CanonPath = str
Hash      = str                    # "blake3:<hex>"

class Cell:                        # tagged union
    Content(source_ref: str, raw_path: str, content_hash: Hash, size: int)
    Tombstone(reason: str | None)
    Derivation(transform: str, params: Mapping)      # v1: unimplemented

@dataclass(frozen=True)
class Layer:
    id: LayerId
    rank: int
    digest: Hash
    cells: Mapping[CanonPath, Cell]          # dom(L)

@dataclass(frozen=True)
class WinnerRecord:
    layer: LayerId
    content_hash: Hash                        # post-derivation
    source_hash: Hash                         # pre-derivation
    size: int
    derivations: tuple[str, ...]

@dataclass(frozen=True)
class ComposedSet:
    order: tuple[LayerId, ...]
    winners: Mapping[CanonPath, WinnerRecord]
    removed: Mapping[CanonPath, LayerId]
    provenance: Mapping[CanonPath, Provenance]
    diagnostics: tuple[Diagnostic, ...]        # sorted
```

### 6.2 The fold

```python
def fold(layers: Sequence[Layer]) -> ComposedSet:
    winners: dict[CanonPath, WinnerRecord] = {}
    removed:  dict[CanonPath, LayerId] = {}
    shadow:   dict[CanonPath, list[tuple[LayerId, Hash]]] = defaultdict(list)

    for L in layers:                              # ASCENDING rank: later WINS
        for p in sorted(L.cells):                 # D-2
            cell = L.cells[p]
            prev = winners.pop(p, None)
            if prev is not None:
                shadow[p].append((prev.layer, prev.content_hash))
            elif p in removed:
                shadow[p].append((removed.pop(p), TOMBSTONE_HASH))

            match cell:
                case Content(_, _, h, size):
                    winners[p] = WinnerRecord(L.id, h, h, size, ())
                case Tombstone():
                    removed[p] = L.id
                case Derivation():
                    raise NotImplementedError("delta layers are v1 out-of-scope (§5.6)")
    ...
```

Note the direction: **ascending rank, later wins** — the inverse of `build_virtual_tree`'s
descending first-hit-wins. Both are correct in their own frame (§0). A one-line comment at
each loop head stating which frame it is in will save someone a day.

### 6.3 Module layout (mirrors the existing `tre_compare` conventions)

```
tools/tre-builder/src/tre_builder/
    recipe.py        # parse + validate + materialize order        [UX-LANE owns the schema]
    layers.py        # Stage 0/1: resolve, index, layer_digest
    fold.py          # Stage 2: the endomorphism fold (§6.2)
    derive.py        # Stages 3-5: validate/repair as content-addressed derivations
    classes.py       # cohort class table + Ki path->key indexing
    consistency.py   # Stage 6: cohorts, references, baseline diff, waivers
    emit.py          # Stage 7: emit plan, admissibility, journal, prune
    verify.py        # Stage 8: round-trip via tre_compare.virtual_tree
    manifest.py      # canonical JSON, digests, report rendering
    cache.py         # reuse/extend tre_compare.cache schema
```

`verify.py` should **import `tre_compare`** rather than vendoring it. One canonicalization,
one runtime merge, one source of truth.

---

## 7. Property/test obligations (the acceptance set for the engine)

| ID | Property |
|---|---|
| P-DET | Two processes / two machines ⇒ equal `manifest_digest` (§1.6) |
| P-ASSOC | `compose([A,B,C]) == compose([bundle(A,B), C])` |
| P-IDEM | `⟦L⟧∘⟦L⟧ == ⟦L⟧` for Content/Tombstone-only layers |
| P-BASE | `compose([base]) ` emits nothing in OVERLAY mode and is byte-identical to base in FULL mode |
| P-INC | incremental == clean, over a scripted mutation sequence (§4.5) |
| P-ROUND | Stage 8 passes for every fixture, incl. a tombstone fixture under packed emit |
| P-COHORT | The historical skeleton-without-LOD recipe **fails** with a report naming the missing `.lmg`/`.lod`/`.sat` paths and a suggested fix |
| P-IDENTICAL | Byte-identical duplicate across layers does **not** trip the cohort gate |
| P-BASELINE | Pre-existing stock dangling refs do not block; an introduced one does |
| P-WAIVER | Waiver applies; then mutate one waived file ⇒ error re-arms |
| P-PRUNE | Recompose with a layer removed ⇒ its files are deleted from the output tree |
| P-PRIORITY | A packed overlay against a loose base is **rejected** at Stage 7 (§3.7) |
| P-REPAIR | All 102 known-corrupt files repair, are idempotent, and re-validate |

P-COHORT and P-PRIORITY are the two that justify the whole design; make them the first
fixtures written.

---

## 8. Decisions requiring the owner (not resolvable from the evidence pack)

1. **Default emit mode.** OVERLAY (loose dir + cfg lines) is the natural v1 given "no TRE
   writer yet", but it **cannot express deletions** (§5.1). If any target layer needs a
   deletion, the packed emitter moves from "v2 nice-to-have" to "v1 blocker".
2. **Where the `strictData`-style escape hatch sits.** This spec assumes strict-by-default with
   per-violation waivers; a single global `--permissive` is easy to add and easy to abuse.
   Recommendation: no global bypass for cohort errors; a global bypass only for the
   `--allow-invalid` structural case, always recorded in the manifest.
3. **Whether `V_base` (baseline violations) is shipped with the tool** as a pinned artifact per
   known retail patch level, or computed on first run. Computing is simpler; shipping makes
   "your base install is not what you think it is" detectable.
4. **Hash choice** — BLAKE3 (fast, needs a dep) vs SHA-256 (stdlib, slower). Everything else in
   the ecosystem here is deliberately stdlib-only; SHA-256 keeps that property and the hashing
   is I/O-bound anyway. Recommend SHA-256 for durable hashes, keep `xxh3_64` for the existing
   in-process memo.
