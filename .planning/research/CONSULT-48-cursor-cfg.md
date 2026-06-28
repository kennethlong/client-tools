First read `D:/Code/swg-client-v2/.planning/research/CONSULT-48-EVIDENCE.md` (factual context; GIVEN).

YOUR ANGLE (cfg/manifest detail — you are the most detailed file reader): determine whether our
client TRE/TOC/searchTree configuration is COMPLETE for JTL space, by reading the actual config
artifacts in the repo and comparing to what the client expects.

Read and analyze:
1. `stage/client.cfg` (staged) AND the source template `src/cmake/config/client.cfg.in` (the
   `[SharedFile]` searchTOC/searchTree/searchPath block). List exactly which TREs/TOCs are wired.
2. Any repo docs/notes about the TRE/TOC/searchTree setup, the ILM content pack, or JTL/space install
   (grep .planning, docs, README, the cmake config dir, AGENTS.md for "ILM", "searchTree", "TOC",
   "JTL", "space", "client.cfg").
3. Reason about the 9 unwired TREs from the evidence (ILM_animation/maps/music/sound/ui/visuals,
   gu8, hotfix_56). Given ILM_visuals carries ship LODs / station / nebula / planetoid visuals and
   ILM_ui carries the space HUD UI: SHOULD these be wired (searchTree entries, or listed in a TOC)?
   At what priority relative to swgsource_3.0.tre (8) / the sku TOCs (0-3)? Is "ILM" a required base
   layer or an optional enhancement, from what the repo config implies?

Deliver concisely: (a) is the cfg manifest missing required TRE wiring (which TREs, what priority);
(b) the exact cfg lines you'd add to wire the ILM_* (and gu8?) family if required; (c) the single
decisive test. Quote concrete config lines; no padding.
