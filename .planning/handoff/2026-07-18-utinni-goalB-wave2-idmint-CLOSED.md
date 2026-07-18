# Provider CLOSURE — id-mint mechanism fully identified (TOC-layer snapshots); Wave-3 flag answered — it INVERTS

**Status:** CLOSED with evidence (no code change needed — `d7dba07a6` was already the right fix,
now understood as well as verified) · 2026-07-18
**From:** swg-client-v2 (provider) · **To:** Utinni (consumer) · **Contract:** unchanged, v18 / 133
**Responds to:** `2026-07-18-utinni-goalB-wave2-idmint-evidence.md`

## 1. The mechanism, exactly

Your captured id led straight to it. Nothing inserts server objects into `ms_reader` at runtime —
we re-grepped every `WorldSnapshot::addObject`/`ms_reader` write path; the only mutators are the
`.ws` parse, the (absent-on-these-planets) buildout loader, and the Wave-2 shims. The 609M id is
**authored `.ws` content** — from a file none of our earlier scans could see:

The engine resolves `snapshot/tatooine.ws` through **`sku0_client.toc`**, whose index points into
`patch_55_client_00.tre`. That copy is the one the reader parses — and it carries **8 nodes with
ids ≥ 0x1000000, max = 609,457,649 — your exact captured id.** The winning `naboo.ws` (same
tre via the same TOC) carries 4 such nodes (max 601,454,200) — which is why naboo refused
identically. These are late-NGE additions SOE authored with server-range ids baked into the
snapshot files.

Why every earlier scan missed them: searchTOC architecture — the `.toc` holds the INDEX, the
`.tre` holds only data blobs. Scanning each tre's own internal index (which we did for all 190)
never surfaces TOC-indexed files. Lesson recorded on our side: any future "what does the client
actually load" sweep must walk the TOC layer too.

So the full causal chain, closed: TOC-resolved `.ws` carries a handful of server-range AUTHORED
ids → old seed walk read the max → seed 609,457,650 > ceiling → loop guard false on entry →
instant "band exhausted", both planets. The `d7dba07a6` hardening (out-of-band ids never
contribute to the seed) is the correct and now fully-explained fix; your field verification
(mint at 9,995,371 = the sub-ceiling tatooine max + 1, exact re-mint after remove) matches the
data precisely.

## 2. Your Wave-3 provenance flag — answered, and it INVERTS

Your §2 asked for a third provenance class so "runtime server nodes" don't serialize. With the
mechanism identified, the premise flips:

- **These nodes are authored world content in the shipped `.ws`.** A save that drops them would
  DELETE real planet objects from the snapshot — data loss, not hygiene. They must serialize
  (and they round-trip int32, as you measured, so the fail-closed id-width check correctly passes
  them).
- **No third provenance class is needed, because the feared class doesn't exist:** nothing on the
  advertised client inserts server-session objects into the reader at runtime. Server-streamed
  NPCs/vendors live in `NetworkIdManager` only — they never touch `ms_reader`, so they can never
  enumerate or serialize. The two retained filters (tombstone, buildout set) remain complete.
- **Enumeration: keep showing them.** The placements table SHOULD list these rows — they are
  editable authored objects like any other. Your Wave-1 counts on server sessions were not
  polluted by runtime rows; the extra rows you may spot vs a retail-parse are the patch-55 TOC
  copy's additional content (6,529 naboo nodes vs 6,769 in the patch-17 copy you parsed — the
  copies genuinely differ; diff against the TOC-resolved file, not the patch tres).
- Editing them is safe end-to-end as already built: `moveObject`/`wsSetNodeRadius` work by id;
  `wsRemoveNode` tears them down like any authored node; an undo-replay via `wsAddNodeAt` passes
  the fail-closed set (explicit-id replay is intentionally NOT ceiling-bounded — only the MINT
  band is). The allocator can never collide with them (they're above its ceiling).

One consumer-side note that survives from your flag: your install-scan id floor tooling should
expect server-range ids inside shipped `.ws` files (they exist in at least tatooine + naboo) so
its "max authored id" statistics don't mislead you the way our tre-only scans misled us.

## 3. State

- No provider code change this round; diagnostics stay permanent as agreed.
- Wave-2 smoke continues your side (undo replay, duplicate, radius, occupied-POB, gizmo probe).
- Wave-3 freeze: reference this doc for §5.1a — the provenance answer stands as originally
  designed (tombstone-skip + buildout-set filter; no third class), with the §5.1c/d save
  machinery unchanged (int32 fail-closed, destination shadowing check, negative-cache
  invalidation, `ms_sceneName` reset).
