# CONSULT-77 RESULTS — TRE version-field endianness verified; CR filed to SWG-Toolkit (2026-08-17)

Question: is the toolkit's `TreVersion` taxonomy (forward-ASCII "0004"/"0005"/"0006" as
distinct versions from "5000"/"6000") correct, or a byte-order artifact?

**Verdict: byte-order artifact, now runtime-proven on two engines.** The version field is a
big-endian-composed uint32 tag serialized little-endian, exactly like the `EERT` magic.
On-disk `"5000"` IS `TAG_0005` (retail stock), `"6000"` IS `TAG_0006`. Deliverable:
`D:/Code/SWG-Toolkit/.planning/handoff/2026-08-17-PROVIDER-CHANGE-REQUEST-tre-version-endianness.md`
(the full CR: defect family, fix list by blast radius, test-gap analysis, open questions).

## Method + artifacts (this dir)

- `CONSULT-77-EVIDENCE-tre-version-bytes.md` — neutral evidence pack (881-file scan E1,
  engine excerpts E2, M0 probe E3, toolkit taxonomy E4).
- Crew (4 parallel, non-overlapping): `CONSULT-77-codex-engine-header-paths.{md,out}` (engine
  read/write map), `CONSULT-77-cursor-toolkit-version-blast-radius.{md,out}` (toolkit
  inventory — his tables are the blast-radius spec), Sonnet ecosystem/genealogy report and
  Opus formal adjudication + E5 re-measurement (both delivered as agent results, key content
  folded into the CR).

## Key facts established

1. 881 files scanned (8 installs): 0 forward-`TREE`; `.tre` = `EERT`+{`5000`×316, `6000`×553};
   `.toc` = `' COT'`+{`1000`, `2000`(Restoration)}. `6000` is the MAJORITY of retail v3.0
   (137/209) — `.toc`-membered blob containers, `numberOfFiles==0`, plain zlib, engine never
   validates their headers (TreeFile_SearchNode.cpp:822-835).
2. Engine accept-sets: SearchTree ctor = {TAG_0004, TAG_0005} else FATAL (:448-449, :505-514);
   SearchTOC = TAG_0001 only; `validate()` dead code (0004-only). Stock TreeFileBuilder
   emits exactly `EERT5000` (TreeFileBuilder.cpp:777-778).
3. **Runtime probes (today)**: Q2 = M0-accepted treexp_a.tre with only bytes 4..7 flipped to
   `"0005"` → FATAL ~4s exit 0x80000003 on OUR exe AND Sais's stage-B exe; control
   (unflipped, same priority-12 mount) boots to world. Q3 = retail `EERT6000` on a
   `searchTree_` line → FATAL ~3s on ours; BOOTS on Sais's engine — **his tree added
   `case TAG_0006:` with stride-32 support and a field-REORDERED 32-byte record map**
   (crc,length,offset,unk0,unk0,fileNameOffset,compressor,compressedLength; zlib code noted
   as 1) — contradicts the toolkit's 24+8-trailing-pad model; answers Opus Q4 pending
   verification vs SwgRestoration_00.tre.
4. Launch gotcha (cost one false-negative run): ConfigFile only receives the POST-`--`
   command line (SetupSharedFoundation.cpp:204) — `SwgClient_r.exe -- @probe.cfg`, and
   `@file` load failures are IGNORE_RETURN-silent. A booted client ≠ probe mounted.
5. Genealogy (Sonnet): Utinni held the identical wrong belief (D-06b "5000 = unverified
   sibling of encrypted 6000"), falsified it in-phase ("the live client is 100% EERT5000",
   07-02-SUMMARY:39), corrected TreVersion.cs, left stale doc-comments. Toolkit taxonomy =
   Utinni's pre-correction state. Masking mechanism in both: self-generated fixtures
   validated by the same parser that wrote them.
6. Toolkit deploy path (`packPatch.ts:116`) already passes `'5000'` explicitly — live deploys
   work; the model was wrong around a working workaround. `repackTre` default converts
   retail headers to the rejected spelling.

## Probe artifacts (parked for re-runs)

- stage-B-x64: `q2_fwdver.tre`, `client-q2.cfg`, `client-q3.cfg`, `client-q2ctl.cfg`
  (+ M0's treexp set). stage-x64: `client-q2.cfg`, `client-q3.cfg` copies.
- No cfg swaps were made; live cfgs untouched (probes ride `-- @file.cfg` on top).
