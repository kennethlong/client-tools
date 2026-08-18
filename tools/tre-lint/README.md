# tre-lint — standalone SWG TRE / searchTOC set linter

Zero-dependency TypeScript, runs directly under Node ≥22.12 (type stripping — no build step,
no npm install):

```
node tools/tre-lint/src/cli.ts <file.tre|file.toc|directory> [...targets]
     [--deep] [--filter <substring>] [--json <out.json>] [--baseline <old.json>] [--quiet]
```

Directory targets lint every `*.tre`/`*.toc` inside (non-recursive) and then run the
set-level pass: `.toc` member resolution (TOCTreePath approximation = the same directory),
shadow census, and `.toc`↔member payload-coordinate coherence.

Built 2026-08-17 as the **working seed + test baseline for SWG-Toolkit `@swg/compose` B1**
(see `SWG-Toolkit/.planning/handoff/2026-08-17-PROVIDER-DESIGN-tre-linter-set-builder.md`).
Format ground truth: engine source (`TreeFile_SearchNode.{h,cpp}`, `Tag.h`, `Crc.cpp`),
`tre_reader.py`, and the CONSULT-76/77 verification rounds (mirrored-tag model, M0 runtime
probes, 881-file census). Severity vocabulary (`error`/`warn`/`info`) and rule granularity
follow the PROVIDER-DESIGN §4 contracts. Tolerate-and-report throughout — nothing refuses to
parse past a finding unless the bytes make continuing meaningless.

## Modes

- **Default:** header + TOC + name-block structural rules, CRC verification of every entry
  name, sort-order check, payload bounds, duplicate path/CRC detection, tombstone census,
  version-model checks (incl. the forward-ASCII "0005" endianness defect class, with the
  runtime-proof citation in the message).
- **`--deep`:** additionally inflates every zlib payload (length cross-check) and runs the
  IFF size-fit walk (`iff-over-declared` / `-under-declared` / `-truncated` /
  `-trailing-bytes`) on payloads that start with `FORM`. `--filter s` restricts deep work to
  virtual paths containing `s`.
- **Degenerate-shadow / stub detection (the ILM preference-kill class):**
  `set-degenerate-shadow` (warn, set-level) flags any path whose copies diverge so hard in
  size that one is a stub of another (smallest ≤ max(512 B, 5% of biggest), biggest ≥ 4 KiB,
  smallest > 0) — order-free, names both archives, states the hazard conditionally ("blanks
  the real content if it mounts at higher priority"). The census MERGES searchTOC-indexed
  copies living in blob containers (the TOC-layer blind spot), deduped against self-indexed
  members. Every pair goes into the JSON (console prints cap at 15/rule) so `--baseline`
  catches any NEW pair. Companion archive-level rules: `tre-stub-sized-texture` (info,
  ≤512 B `.dds`/`.tga`) and, under `--deep`, `tre-stub-texture` (warn, real DDS header
  dimensions ≤16×16 — the 8×8 class).
  ⚠ Honest limit: this catches SIZE-degenerate stubs. Content-degenerate shadows of similar
  size (e.g. the ILM density-ZEROED nebula datatables) need format-aware rules — that is
  `@swg/compose` B4 cohort/content territory, not this tool.
- **`--json out.json`:** deterministic machine-readable report — the BASELINE artifact.
- **`--baseline old.json`:** exit 2 + a named delta if any non-info finding exists that the
  baseline lacks; exit 0 otherwise. This is the `V_full \ V_base` regression posture from the
  design, at file granularity.

Per-rule findings are capped at 25 per archive with a `-rollup` count line (a 215k-warning
archive produces a readable report, not a 58 MB one).

## v0006 (on-disk "6000") populated-TOC record-model arbitration

For every populated 32-byte-stride TOC the linter decodes each record under BOTH candidate
models — PAD (24-byte layout + 8 trailing zero bytes) and REORDER (unknowns at [12..19],
compressor/compressedLength at [24..31], the Galaxies-Reborn engine field map) — and scores
them per entry on zero-pair placement, compressor sanity, payload bounds, and CRC-of-name
match. The verdict lands in the report (`v0006Model`).

**Result on real bytes (2026-08-17, all 46 populated Restoration archives): REORDER wins
unanimously; PAD scores 0.0% everywhere. The pad model is falsified.** Every entry carries
`compressor=1` — confirming code 1 as the Restoration-dialect zlib id (stock uses 2); the
linter reports that as `info` in v0006 archives, `warn` (CT_deprecated) elsewhere.
Sub-100% REORDER scores track the volume-split archives (`_01_a/b/c` share one TOC whose
entries point into sibling volumes — bounds fail per volume, as expected).
Restoration payloads are never decrypted (standing rule) — bounds checks only.

## Baselines shipped in `baselines/` (2026-08-17, this machine's installs)

| Install | Archives | E / W | Notable |
| --- | --- | --- | --- |
| SWGEmu stock | 53 | 0 / 0 | The clean corpus — zero non-info findings |
| SWGSource v3.0 | 213 | 0 / 583 | 129,101 `.toc` entries cross-checked vs self-indexed members: ALL coherent; warns = **582 degenerate-shadow pairs** (90 with the stub side in ILM archives — every `nebula2_*`/`nebula_dant_*`/`nebula_endr_*` 240 B skybox stub vs its 786 KB–1 MB real counterpart, i.e. the landmine catalog enumerated mechanically) + 1 zero-length-`.toc`-entries warn. The non-ILM pairs are 15 years of REAL retail content-kills and reversals: `patch_00` stubs `terrain/tutorial.trn` to 990 B (SOE retiring the old tutorial), and retail's 176 B `arc170_cockpit_1_emis.dds` stub is UN-stubbed by ILM with 349 KB of real content (ILM adding content — direction-agnostic rule catching a reverse case) |
| `_client_dx11` | 213 | 0 / 583 | Same profile as v3.0 |
| Restoration | 47 | 0 / 0 | v0006 model verdict above; `.toc` is v'0002' (fork; enumerate-only note) |
| SWG Beyond | 211 | 1 / 1 | The 1 error = `beyond_patch_01.tre` obfuscated TOC (`tocCompressor=2`, not zlib) — the known third-profile archive, found by the tool unprompted |
| Infinity | 27 | 0 / 1 | ⭐ The warn is a REAL in-archive 32-bit CRC collision (`…boss_nass…ready.ans` vs `…speederbike_deed.iff`, both 0xf2ead531 in `infinity_custom_01.tre`) — live proof of the duplicate-CRC rule's reason to exist |
| Stardust | 23 | 0 / 0 | Clean |
| ILM_visuals `--deep --filter .skt` | 1 | 102 / 0 | Exactly the catalogued 102-skeleton class, each over-declared by exactly 16 bytes — zero misses, zero false positives |

## Acceptance evidence

- CRC self-test at startup (forward CRC-32 vs a verified stock TOC value) — the tool refuses
  to run if the table is wrong.
- Known-good/known-bad pair: `treexp_a.tre` (M0 client-accepted) lints clean;
  `q2_fwdver.tre` (client-FATAL, runtime-proven) flags `tre-version-forward`.
- The 102-skeleton corpus reproduces exactly under `--deep`.
- Baseline-diff: self-comparison exits 0; a bad archive against a clean baseline exits 2
  naming the regression.

## Handover notes (SWG-Toolkit)

- Erasable-TS only (runs under Node type stripping; also compiles under plain `tsc`).
- Three modules: `format.ts` (parsers + CRC + tag model), `iff.ts` (size-fit walker — VALIDATE
  only; do NOT derive a repairer from it, see the design's R9 note), `lint.ts` (rules + set
  pass), `cli.ts` (entry, report, baseline diff).
- Maps onto `@swg/compose` B1: `lint/` rules land in the registry structure, `format.ts`
  merges with/cross-checks native-core, the baselines become `fixtures-real`-lane test
  inputs, and the JSON reports are ready-made expected-output fixtures.
- The v0006 REORDER verdict + compressor=1 finding settle the design's open questions §10.1
  (read-side) — v0006 WRITES stay refused regardless.
