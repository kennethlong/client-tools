# CONSULT-76 shared evidence pack — TRE-set builder (treat as given; do not re-derive)

GOAL: a configuration-driven tool that BUILDS a playable Star Wars Galaxies TRE data set
from layers.

- Base layer: a pristine retail install (the original game's files; patch level to be
  verified at project start).
- Opt-in layers (initial set, extensible): SWG-Source data bugfixes · JTL (space) content ·
  ILM lighting enhancements · ILM sound enhancements · community-authored layers.
- CONSTRAINTS (hard):
  * Recipe-not-bytes: the tool distributes compose instructions + provenance, never
    third-party bytes the user doesn't already own.
  * Layer consistency ENFORCED: paired asset classes must move together (field-proven
    failure: replacing skeletons (.skt) without their paired mesh-LOD files (.lmg/.lod/.sat)
    made NPCs invisible beyond ~10m). A layer touching one member of a paired class must
    carry the set, or composition fails loudly.
  * Corrupt IFF headers repaired at compose time. Known corruption class: FORM size fields
    declared LARGER than the actual payload (a third-party LOD-strip tool wrote these).
    A bottom-up clamp-to-parent-extent + recursive validate pass exists and has repaired
    102 such files with zero failures.
  * Output must load in the stock engine: either packed .tre archives (+ optionally a
    searchTOC) or a loose override directory + cfg searchPath lines. Engine read priority:
    loose searchPaths outrank searchTree_XX_Y tres, which outrank searchTOC-resolved files.
- EXISTING TOOLING (on the shelf, working):
  * Version-aware TRE/TOC READER: D:\Code\swg-blender-plugin\swg_pipeline\tre_reader.py
    (+ tre_decrypt.py) — parses .tre archives, master indexes, cot2000/toc2000 variants,
    and searchTOC files (read_search_toc_entries etc.), zlib decompression.
  * tools/tre-compare (in D:\Code\swg-client-v2) — standalone same-path content-diff of
    archives, zero engine imports, uv package.
  * IFF validator/repairer pattern (recursive size-fit walk; every FORM/chunk size must fit
    its parent, siblings sum to parent end).
  * A landmine catalog distinguishing genuine content vs "preference-kills" (e.g., 240-byte
    stub textures at stock paths, density-zeroed datatables) vs genuine corruption.
- MISSING piece: a TRE/TOC WRITER. A v1 could emit a loose override dir + cfg lines instead
  of packing archives.
