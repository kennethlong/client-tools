# Phase 37 deferred items (out-of-scope discoveries)

## 37-02

- **SwgClient Optimized (`_o.exe`) config fails at LINK with `LNK1281: Unable to
  generate SAFESEH image`** — PRE-EXISTING, unrelated to the hookpoint catalog.
  Confirmed isolation: the Optimized build log has `0 unresolved external symbol`
  and `0 LNK2019/2001` (none of the 41 MVP `&` rows fail to resolve); the ONLY
  error is the SAFESEH image-generation failure. This matches the known
  `project_optimized_config_safeseh_pre_existing` memory ("validate removals via
  Debug+Release link-grep; don't re-investigate"). The spec §5 wants the export on
  all 3 flavors, but `_o` cannot link for a reason that predates this phase. Debug
  (`_d`) and Release (`_r`) both link clean and export `GetEngineHookPoints`
  undecorated (dumpbin-confirmed). Do NOT attempt to fix SAFESEH as part of 37-02.
