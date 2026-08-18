# CONSULT-77 EVIDENCE PACK — TRE header magic/version bytes (neutral, treat as given)

Measured + code-read facts only. No interpretation. All measurements 2026-08-17 on this machine.

## E1 — Header scan of every .tre/.toc across all client installs on disk

Scan method: first 8 bytes of each file, rendered as ASCII (non-printable as \xNN).
881 files total. Complete tally (count, root, extension, [bytes 0-3][bytes 4-7], example):

```
    4  D:\Code\Galaxies-Reborn\_client_dx11  .toc  [ COT][1000]   e.g. sku0_client.toc
   72  D:\Code\Galaxies-Reborn\_client_dx11  .tre  [EERT][5000]   e.g. bottom.tre
  137  D:\Code\Galaxies-Reborn\_client_dx11  .tre  [EERT][6000]   e.g. hotfix_56_client_00.tre
    4  D:\Code\SWGSource Client v3.0         .toc  [ COT][1000]   e.g. sku0_client.toc
   72  D:\Code\SWGSource Client v3.0         .tre  [EERT][5000]   e.g. bottom.tre
  137  D:\Code\SWGSource Client v3.0         .tre  [EERT][6000]   e.g. hotfix_56_client_00.tre
    2  D:\Sample-TRE-Files                   .toc  [ COT][2000]   e.g. SwgRestoration.toc
   92  D:\Sample-TRE-Files                   .tre  [EERT][6000]   e.g. SwgRestoration_00.tre
   23  D:\Stardust TREs                      .tre  [EERT][5000]   e.g. mtg_patch_001_appearance_01.tre
    4  D:\SWG Beyond                         .toc  [ COT][1000]   e.g. sku0_client.toc
   69  D:\SWG Beyond                         .tre  [EERT][5000]   e.g. beyond_client_base.tre
  138  D:\SWG Beyond                         .tre  [EERT][6000]   e.g. beyond_patch_01.tre
   27  D:\SWG Infinity                       .tre  [EERT][5000]   e.g. bottom.tre
    1  D:\SWG Restoration                    .toc  [ COT][2000]   e.g. SwgRestoration.toc
   46  D:\SWG Restoration                    .tre  [EERT][6000]   e.g. SwgRestoration_00.tre
   53  D:\SWGEmu-Client                      .tre  [EERT][5000]   e.g. bottom.tre
```

Facts as given:
- 0 of 881 files begin with forward ASCII `TREE`.
- Observed version-field byte strings: only `5000` and `6000` (.tre); `1000` and `2000` (.toc).
- `[EERT][6000]` archives appear in the RETAIL-lineage installs (SWGSource v3.0, _client_dx11,
  SWG Beyond — the hotfix_NN/patch archives), not only in SWG Restoration.
- `D:\Code\SWGSource Client v3.0` is the exact TRE set the swg-client-v2 stock client mounts
  and plays from daily (both renderers).

## E2 — Engine source excerpts (swg-client-v2, the SOE-lineage client)

Tag construction (`sharedFoundation/src/shared/Tag.h`):
```
93: #define TAG_B(a,b,c,d) 0x ## a ## b ## c ## d
94: #define TAG_A(a,b,c,d) static_cast<Tag>(TAG_B(a,b,c,d))
95: #define TAG(a,b,c,d)   TAG_A(TAG_DIGIT_ ## a, TAG_DIGIT_ ## b, TAG_DIGIT_ ## c, TAG_DIGIT_ ## d)
263: const Tag TAG_0004 = TAG(0,0,0,4);   // = 0x30303034
264: const Tag TAG_0005 = TAG(0,0,0,5);   // = 0x30303035
265: const Tag TAG_0006 = TAG(0,0,0,6);   // = 0x30303036
```

`sharedFile/src/shared/TreeFile_SearchNode.cpp`:
```
39:  const Tag TAG_TREE = TAG(T,R,E,E);   // = 0x54524545
396-397:  Header header; file->read(0, &header, sizeof(header), ...)   // raw struct read
407: if (header.token != TAG_TREE) return false;          // [validate() path]
411: if (header.version < TAG_0004 || header.version > TAG_0004) return false;   // [validate() path]
437-439:  raw header read; DEBUG_FATAL(header.token != TAG_TREE, ...)  // [SearchTree ctor]
446-449:  switch (m_version) { case TAG_0004: case TAG_0005: { ... }   // [SearchTree ctor accepted versions]
```

`TreeFileBuilder/src/shared/TreeFileBuilder.cpp` (the stock offline packer):
```
32:  const Tag TAG_TREE = TAG(T,R,E,E);
777: header.token   = TAG_TREE;
778: header.version = TAG_0005;
```
The Header struct is written to disk as a raw struct (fwrite-style), fields native x86
little-endian uint32.

CONSULT-76 §1.1 finding (verified previously): `validate()` (the :407-411 path) has zero
callers in src/ — dead code. The ctor path (:437-449) is the live mount path.

## E3 — Runtime probe (CONSULT-76 M0, 2026-08-16, results in CONSULT-76-M0-RESULTS.md)

- Two hand-written archives whose first 8 bytes were `EERT` + `5000` were accepted end-to-end
  by the running swg-client-v2 client (mounted via searchTree_XX_Y, entries served, override
  visible, zero-length entry acted as a runtime tombstone).
- A variant whose first 8 bytes were forward `TREE` + `0005` was REJECTED by the reader.
- MD5 trailer block: archive without it was accepted (optional to the client).

## E4 — SWG-Toolkit native-core current state (packages/native-core/modules/core/tre/)

`TreVersion.h` (excerpt, current):
```
enum class TreVersion : uint8_t {
    V0004,  // "0004" — Infinity/SWGEmu early format
    V0005,  // "0005" — Infinity/SWGEmu/Stardust primary format
    V0006,  // "0006" — SWG Restoration; READABLE (not encrypted, not enumerate-only)
    V5000,  // "5000" — some legacy format
    V6000,  // "6000" — SWG Restoration encrypted; ENUMERATE-ONLY (payloads never read)
};
// parseVersionString(): memcmp of the 4 bytes at offset 4..7 against forward ASCII
// "0004"/"0005"/"0006"/"5000"/"6000"; throws on anything else.
// isEnumerateOnly(): true only for V6000.
// recordStride(): 24 for all but V6000 (32).
```

`TreBuilder.cpp:149-163` (write path): writes the 4 bytes `EERT`, then a FORWARD ASCII
version string per enum arm: V0004→"0004", V0005→"0005", V0006→"0006", V5000→"5000",
V6000→"6000". Default build target documented as V0005.

Toolkit read-path provenance note (their docs): "bottom.tre ver '5000', 808 records, stride 24;
SwgRestoration_00.tre ver '6000', 334 records, stride 32 — proven byte-exact." Their 04.3
work additionally established: retail-lineage `[EERT][6000]` container archives have
numberOfFiles=0 internal TOCs and are indexed by the master `.toc`; payloads read by offset;
"plain zlib for SWG-Source, encrypted only for Restoration" (per-payload classify).
