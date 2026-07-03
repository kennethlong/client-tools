---
created: 2026-07-03
title: Audit stage/ilm_extract for more Legends preference-changes shadowing retail data
area: client data / TRE layering / ilm_extract
status: backlog
priority: medium (one confirmed hit with wide blast radius; unknown residue)
references:
  - the confirmed hit: interior fog OFF in ~124 interiors via ILM's datatables/interior/interior.iff
    (fixed 2026-07-03 by a merged override at stage/override/datatables/interior/interior.iff)
  - memory: project_jtl_space_ship_data_ilm_exclusive (why ilm_extract exists)
  - CONSULT-51 (the de-stub extraction that created stage/ilm_extract, 2026-06-27)
  - comparison tooling: D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py + the DataTable
    IFF parser/merger written 2026-07-03 (in the session transcript; re-derivable — parse
    FORM DTII/0001 COLS/TYPE/ROWS; strings null-terminated inline, ints/floats 4 bytes)
  - tools/tre-compare/ — the standalone TRE compare tool is purpose-built for exactly this

## The risk class (proven 2026-07-03)

The CONSULT-51 de-stub kept ILM_visuals.tre's "genuine" entries: exclusives AND **"real updates"**
— entries that differ from base. The "real updates" bucket is exactly where SWG Legends'
gameplay/aesthetic preference-changes hide: their interior.iff was retail's table with
`Fog Enabled` flipped 1→0 in ~124 interiors (cantinas, Jabba's palace, temples, bunkers) + one
ambient-sound swap. It silently shadowed retail at priority 5 for six days.

## The audit

For every file in stage/ilm_extract that ALSO exists in the retail patch chain (patch_16_00 et al
— latest patch wins in the TOC), diff ILM vs retail-effective:
1. Enumerate overlaps: ilm_extract file list ∩ union of TOC-served names (tre_reader.list_tre
   over the sku0 TOC's tre set; the effective copy is the one TreeFile would serve WITHOUT
   ilm_extract wired).
2. Known non-space suspects to check FIRST: `datatables/cutscenes/cutscenes.iff`,
   `datatables/missiles.iff` (space-adjacent but gameplay-tuning-prone). Then the overlapping
   `shader/`, `texture/`, `effect/`, `clienteffect/`, `palette/`, `terrain/` entries (visual
   deltas — a Legends re-tint/re-author would shadow retail ground visuals).
3. For byte-different overlaps: classify KEEP (needed for ILM space content to work),
   MERGE (retail values + ILM-only rows, the interior.iff treatment), or DROP from ilm_extract
   (pure preference change).
4. Verification bar: cantina + a known bunker/temple look retail-correct; space still loads with
   ship visuals intact (the reason ilm_extract exists).

## Done when
Every ilm_extract-vs-retail overlap is classified with a decision, preference-changes are
neutralized (merged or dropped), and the layering hazard is documented in the memory that
recommends wiring ILM.
