# CONSULT-76 M0 — the decisive experiment: RESULTS (2026-08-16 night)

Two hand-written `TREE/0005` archives + a conflicting loose file, mounted over the pure
stock v3.0 dataset in stage-B (his exe, gl11), one boot. Script:
`CONSULT-76-M0-build_treexp.py` (also generates the archives; CRC self-verifies against 25
real stock TOC entries before writing). Field verification: Kenny boot (audible tombstone),
report-log forensics for everything else (the shader-substitution channel: an HLSL override
of a stock asm program makes the "substituted with the embedded corpus translation" line
VANISH for that name — binary, unmissable, with 45 untouched asm programs as control group).

## Verdicts — every §1.5 open item retired

| Question | Verdict | Evidence |
| --- | --- | --- |
| Writer byte format (header/TOC/name block/sort/CRC) | **CONFIRMED** | Both archives mount without the corruption FATAL; engine binary-search FINDS our entries (tombstone honored + both overrides served) — lookup success requires correct CRC, sort, name block, and name form together |
| Header tag byte order | **`EERT` + `5000` on disk** — TAG_TREE/TAG_0005 are big-endian tag VALUES written little-endian. Writing literal `TREE`/`0005` is rejected. (Spec docs corrected; the reader's "5000 = Restoration variant" note is a misread — 5000 IS retail 0005.) | reader rejection + `xxd` of stock patch_08.tre |
| Stored payload read | **CONFIRMED** | tre_a's HLSL `ui.psh` compiled as ps_4_0, substitution line absent |
| Compressed payload read (compressor=2, zlib) | **CONFIRMED** | tre_b's `a_modulate2x.psh` decompressed + compiled; corpus's normal X3206 warnings prove content intact |
| MD5 trailing block optionality | **OPTIONAL** (mount AND read) | tre_b has no MD5 block and served its payload |
| Zero-length tombstone at runtime | **REAL** | `sound/music_main_title.snd` len=0 → silent login theme (Kenny's ear); world music unaffected |
| Loose-vs-tree priority comparator | **PURE NUMERIC priority, node type irrelevant** | tre at priority 11 beat a loose searchPath copy at priority 9 for the same path; the loose mount itself was live (manifestFiles=1 probe line) |
| Include-through-archive | **WORKS** | `asm_constants.inc` "served from the search path, 861 bytes" = exactly our tre's payload via the D3D11 include handler → TreeFile |

## Bonus field finding
Mounting content whose BAKED shader-cache includes are unreadable in the current mount
DISABLES the compiled-shader cache ("the cache is disabled because every program that
included it would compile from different text than was baked") — a cache-coherence guard.
Builder consequence: composed sets change shader-cache warmth; a compose should expect (and
may want to trigger) a re-bake.

## Consequences for the roadmap
- **M5 (TRE writer) is now confirmed spec, not speculative** — the ~60-line writer in the M0
  script is the seed (strict form: payloads-after-header, uncompressed 24B TOC sorted by
  (crc, stricmp), raw name block with sizeOfNameBlock == uncompSizeOfNameBlock, EERT/5000).
- **Stage-7 admissibility is simple**: emitted nodes just need a higher priority NUMBER than
  every base mount — no node-type special-casing.
- **Tombstones are packed-emit-only** (as Opus specified): loose emit cannot express them;
  the .tre path can, and it works.
- Cleanup state: experiment artifacts left in stage-B-x64 (treexp_a.tre, treexp_b.tre,
  treexp_loose/, client-treexp.cfg) for re-runs; live client.cfg RESTORED (verified BOM-clean,
  zero treexp lines).
