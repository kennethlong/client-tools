First read `.planning/research/CONSULT-48-EVIDENCE.md` in this repo (factual context; treat as GIVEN).

YOUR ANGLE (client load-path tracer — you are the repo tracer): using the SOURCE TREE, determine why
a ship renders as the DEFAULT placeholder and the space environment renders broken when the JTL data
is present, and whether the cfg's TRE/TOC manifest is complete for JTL space.

Trace and report (file:line):
1. The load path: ship object template -> appearance resolution (AppearanceTemplateList / TreeFile).
   What happens when an appearance file is NOT found on the searchPath? Confirm it yields the default
   placeholder render object (the "missing model" the operator sees).
2. The space ENVIRONMENT / zone setup: how the client loads a space zone's celestials, nebula,
   lighting, skybox, asteroid fields, and the space-zone CONFIG datatable. What data files does it
   require, and what's the failure mode when they're absent/unwired (vs FATAL)?
3. Any in-repo cfg TEMPLATE (e.g. src/cmake/config/client.cfg.in) — is its TOC/searchTree manifest
   complete? Does anything in the repo (code, cfg, docs, build) reference the ILM_* TREs, gu8.tre, or
   a canonical client TRE list? Is the ILM pack expected to be loaded?
4. Given the 9 unwired TREs (esp. ILM_visuals with ship LODs / station / nebula / planetoid assets),
   is "unwired ILM_* = missing ship/space visuals" consistent with the engine's asset-resolution
   behavior?

Deliver concisely: (a) does unwired-ILM_* explain the ship default-model + broken space; (b) is our
cfg's TRE manifest incomplete for JTL; (c) the single decisive test. Cite concrete evidence; no padding.
