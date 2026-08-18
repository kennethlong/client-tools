Read D:\Code\swg-client-v2\.planning\research\CONSULT-76-EVIDENCE-tre-builder.md first (shared
evidence, treat as given).

YOUR ANGLE (byte-map tables): exhaustive field-level tables, cited file:line, from
D:\Code\swg-blender-plugin\swg_pipeline\tre_reader.py and tre_decrypt.py:
1. TRE archive: header table (offset, size, type, endianness, meaning, valid values per
   version), TOC entry table, name-block layout, compression flag semantics, and the
   decrypt scheme(s) in tre_decrypt.py (which archives, what algorithm, key handling).
2. Master index / cot2000 / toc2000: same table treatment.
3. searchTOC: header table incl. the tree-file-name block, entry table
   (SEARCH_TOC_ENTRY_FMT decoded field by field), compression of the TOC block itself.
4. IFF container: FORM/chunk grammar as tables (tag, BE size, payload, nesting), the
   size-consistency invariants a validator checks, and the exact repair rule for the
   oversized-FORM corruption class described in the evidence pack.
5. A "gotchas for a writer" list: every place byte order, inclusive/exclusive sizes,
   NUL-termination, or path-separator/case conventions could bite.
Do not editorialize on architecture — the tables ARE the deliverable. Cite everything.
