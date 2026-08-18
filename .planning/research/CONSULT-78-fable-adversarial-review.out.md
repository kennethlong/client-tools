# CONSULT-78 — fresh-Fable adversarial review of TRE-BUILDER-DESIGN-draft (2026-08-17)

Delivered as an agent result; persisted verbatim. All findings were applied to the final
design (`SWG-Toolkit/.planning/handoff/2026-08-17-PROVIDER-DESIGN-tre-linter-set-builder.md`).

Spot-checks performed against: engine `TreeFile_SearchNode.{h,cpp}` (SearchTOC
header/parse/lookup/open paths), `TreeFile.cpp` (addSearchTOC), toolkit
`packages/native-core/modules/core/tre/TreMount.{h,cpp}`, `tre_binding.cpp`
(mountTreMountWithToc), `packages/renderer/src/services/tocReader.ts`,
`packages/native-core/test/tre-toc-mount.test.ts`.

## 1. Verdict summary

The single biggest risk: **the design's flagship output form — the toc-set — cannot express
removals, and the design doesn't know it.** `SearchTOC::exists` hardcodes `deleted = false`
(TreeFile_SearchNode.cpp:1013) and `localExists` returns plain not-found for `length == 0`
(:991-994), so a zero-length entry in a `.toc` set does not stop the search — the shadowed
stock file wins at lower priority. Tombstones work only through `searchTree`-mounted `.tre`
nodes. The draft's Stage-2 guard ("abort if a tombstone is required under **loose** emit")
inherits the synthesis's loose-vs-packed dichotomy, but the draft changed the primary target
to toc-set without re-deriving which side the toc-set falls on. It falls on the *loose* side.
Nothing in B0–B5 acceptance would catch it (Stage-8 checks winner *names*; the running-client
lane is skipIf-missing). Close behind: Stage-8 verify is weaker than claimed on exactly the
axis (new-format byte correctness) where the toolkit was just burned.

## 2. Findings

- **F1 — BLOCKER — toc-set emit cannot tombstone; Stage-2 guard names the wrong emit mode.**
  Fix: extend Stage-2 realizability (tombstones realizable only with a `searchTree`-mounted
  carrier `.tre` above the base; toc-set-only + removals ⇒ abort or auto-add carrier + cfg
  line with stated priority relation); B0 probe: zero-length entry IN a `.toc` over an
  observable stock path — confirm it does NOT delete; restore the packed-tombstone
  running-engine acceptance the draft dropped.
- **F2 — MAJOR — "TS fold + native mount = redundancy" is true for composition semantics,
  false for the NEW format; verify checks the cheap thing.** Native `TocExternalEntry` is
  annotated as a port of `tocReader.ts` (TreMount.h:45-58) — writer and verifier share one
  genealogy; and Stage-8 compares winner NAMES while the member-read path has no payload CRC
  (:1107-1116) — offset/length bugs ship silently. Fix: content-hash extraction through the
  native mount; `.toc` re-emit byte-parity vs real `sku0_client.toc`; running-client skips =
  visible CI statuses, required for release tags.
- **F3 — MAJOR — the .toc member-name resolution contract is absent from the writer spec.**
  Engine resolves member names against CWD then each `[SharedFile] TOCTreePath`; miss = hard
  release FATAL (:798-841, :835). Fix: bare filenames + cfg fragment emitting `TOCTreePath`;
  lint rule "member unresolvable"; B0 mounts from a non-CWD dir.
- **F4 — MAJOR — .toc↔member offset coherence: stale-but-in-bounds offsets read wrong bytes
  silently (no payload CRC).** Fix: cross-check `.toc` entries vs self-indexed members'
  internal TOCs; bounds+spot-inflate blob members.
- **F5 — MAJOR — the cache/incrementality module vanished in the Python→TS translation while
  its behaviors survived as promises.** ~250k-entry lint with no persistence story. Fix:
  restore `cache.ts` (identity `(abspath, mtime_ns, size)`, verdict memos, derivation store);
  perf budget + worker decision at B1 with a measurement.
- **F6 — MAJOR — B1/B2/B3 acceptances are exactly the tests CI will silently skip** (102
  pairs live in a mutable staging dir outside the repo; boot gates are machine-bound;
  packed-vs-loose audit is provider-side). Fix: snapshot the 102 pairs into an immutable
  digest-manifested fixture archive (maintainer sign-off for vendoring GR-derived bytes);
  every skip a distinct visible CI status; name machine-bound sign-offs.
- **F7 — MINOR — Windows emit-path guards (C-16–C-20: MAX_PATH, reserved device names, case
  collisions) fell out of the draft.** Restored + fixtures.
- **F8 — MINOR — engine-feature-disabling shadows missing from the landmine catalog** (baked
  shader-cache include class from M0's bonus finding). Added as the "engine-special files"
  category.
- **F9 — MINOR — packed tombstone realization had no named acceptance in B0–B5** (quiet
  under-delivery of "full engine"). Named acceptance added.
- **F10 — MINOR — the packPatch compose option is a smuggled control.** Fixed: selected by a
  project-settings field (sibling-file pattern), explicitly not a visible control; winner-
  tooltip line re-scoped to extending the existing detail panel.
- **F11 — SEQUENCING — split B3**: B3a fold+manifest+LOOSE emit+verify (composition semantics
  proven over the engine-proven loose path, zero new format code) → B3b packed emit + `.toc`
  writer (failures unambiguously format). B2 parallel. Adopted.

## 3. Refutations attempted that FAILED (survived — signal)

- **Full recipe layer in v1 = dead code without UI** — survived: the CLI itself and the
  provider's ILM recipe-not-bytes plan are day-one consumers; P-DET exercises the digests.
  Deferring git-URL sources cuts acquisition, not engine.
- **`'0005'` self-indexed container as `.toc` member** — survived; no failure mechanism
  (member-open never reads the header, :822-835; payload reads by absolute offset). B0 stays
  as the runtime double-check.
- **24-byte searchTOC record layout** — survived, with the writer-hostile subtlety confirmed
  correctly captured read-side: on-disk `fileNameOffset` stores filename LENGTH, rebuilt to
  cumulative offsets (:869-881; tocReader.ts:292-321); name block in TOC order. Made explicit
  in the writer spec.
- **"Priority is pure numeric across node kinds"** — survived (M0-measured; design says
  assert-don't-assume).
- **Fold-must-be-TS** — survived (must represent unwritten outputs + removals); noted that the
  mandated shared canonicalization is one more reason Stage-8 independence is
  composition-level only.

Net: failures concentrated where the draft DIVERGED from the synthesis without re-deriving
consequences — the toc-set-as-primary flip (F1/F3/F4/F9), the Python→TS translation (F5), and
the verify-independence story (F2).
