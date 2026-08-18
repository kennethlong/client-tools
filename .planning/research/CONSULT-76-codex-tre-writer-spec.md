Read D:\Code\swg-client-v2\.planning\research\CONSULT-76-EVIDENCE-tre-builder.md first (shared
evidence, treat as given).

YOUR ANGLE (repo tracer): a WRITER-ORIENTED format specification. Trace
D:\Code\swg-blender-plugin\swg_pipeline\tre_reader.py (and tre_decrypt.py) end to end and
produce the spec a .tre/.toc WRITER must satisfy so that this reader — and by proxy the
game engine — accepts its output. Specifically:
1. Enumerate every on-disk format variant the reader handles (tre versions, master-index
   kinds, cot2000/toc2000, searchTOC) and state which single variant a new writer should
   TARGET for maximum engine compatibility, with the evidence.
2. For that target variant: the complete write path — header fields and their derivation,
   TOC record layout and ordering rules, name-block encoding, compression choices (when
   zlib, when store), checksum/CRC fields and how they're computed, alignment/padding.
3. searchTOC: what a writer must emit for the engine to resolve files through it (tree-name
   list semantics, entry fields, how tree_index binds).
4. List every point where the reader is lenient (accepts multiple encodings) vs strict —
   writers should emit the strict form; call each out.
5. Open questions the reader alone cannot answer (things only the engine's C++ TreeFile
   reader would pin down) — list them precisely so they can be checked in the engine source.
Output: a single markdown spec with numbered sections. Cite file:line for every claim.
