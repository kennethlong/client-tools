# Phase 16: v2.1 Tech-Debt Cleanup - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-05-26
**Phase:** 16-v2-1-tech-debt-cleanup
**Areas discussed:** 989crypt.lib fix, Phase scope, Residue depth, Verification bar

---

## 989crypt.lib fix

| Option | Description | Selected |
|--------|-------------|----------|
| Unlink, surgical, grep-verify | Remove only the 989crypt.lib token from SwgGodClient Debug deps; grep-verify; don't build the Qt-broken tool | |
| Unlink + sweep dangling tokens | Also audit the full dep list (all 3 configs) for OTHER tokens pointing at libs deleted during v2.1, and remove those too | ✓ |
| Restore 989crypt.lib | Re-add the deleted stationapi lib (contradicts Decruft) | |

**User's choice:** Unlink + sweep dangling tokens
**Notes:** Verification stays grep-only (SwgGodClient is pre-broken on Qt + out of `/t:SwgClient` closure — can't link-test). Sweep is bounded by the v2.1-deleted-lib set (stationapi/989crypt, lcdui, VideoCapture, vivox, libMozilla); soePlatform/libs tokens (Base/ChatAPI/etc.) are KEEP-listed and preserved.

---

## Phase scope

| Option | Description | Selected |
|--------|-------------|----------|
| Verify Target 2 no-op; code residue only | Record lcdui-editor-LIBPATH==0 as a verification step; fix code residue + 989crypt; leave docs alone | ✓ |
| Also fold in doc-staleness fixes | Additionally correct 12-VERIFICATION score line + populate 13-SUMMARY frontmatter | |
| Keep Target 2 as an explicit active item | Treat Target 2 as a re-verify item even though already 0; no doc work | |

**User's choice:** Verify Target 2 no-op; code residue only
**Notes:** Scout found Target 2 (lcdui editor LIBPATH) already at 0 hits in all 4 editor vcxproj — swept by Phase 15's 15-04 "A1 lcdui P12-residue cleanup" after the audit. Doc-staleness items explicitly left out of scope.

---

## Residue depth

| Option | Description | Selected |
|--------|-------------|----------|
| Full removal | Delete the whole dead finalUrl block (HudAction :1170-1189) + full volume API (2 statics + 4 methods + 4 header decls, zero callers) | ✓ |
| Minimal — dead values only | Remove only the statics + dead-store assignment; keep accessor shells/declarations | |
| You decide per-file | Claude picks cleanest cut per file | |

**User's choice:** Full removal
**Notes:** finalUrl is dead because its consuming ShellExecute is commented out. Volume API has zero external callers (scout-confirmed). CR-01 ordinal-placeholder lesson does NOT apply — plain float statics, not a positional enum/datatable row.

---

## Verification bar

| Option | Description | Selected |
|--------|-------------|----------|
| Link-grep + single char-select smoke | SwgClient Debug+Release link-grep==0 (Optimized exempt) + boot once at rasterMajor=11 | ✓ |
| Full dual-renderer boot gate | Boot under BOTH rasterMajor=5 and =11, matching every prior Decruft step | |
| Link-grep only, no boot | Dead-code/token removal; link-grep==0 sufficient, no boot | |

**User's choice:** Link-grep + single char-select smoke
**Notes:** No-behavior-change phase; source edits are in SwgClient-linked libs so one boot confirms no regression. Optimized config EXEMPT per DEF-14-01 (pre-existing SAFESEH LNK1281).

---

## Claude's Discretion

- Commit granularity / atomic-commit split across the 3 non-overlapping target files.
- Whether the Target-2 verify step is its own plan step or folded into the build gate.

## Deferred Ideas

- Audit doc-staleness fixes (12-VERIFICATION score line; 13-SUMMARY frontmatter) — out of scope per D-05.
- Nyquist finalisation for Phases 12 & 13 (`/gsd:validate-phase 12` + `13`).
- `stage/client_d.cfg` accumulated-test-settings cleanup (v2.2-coupled).
- AR-15-01 future-TCG-revival re-evaluation (future v2.x).
- Reviewed-not-folded todos: cantina corner-snap engine quirk; SWGSource-vs-whitengold TRE asset diff (both off-domain / v2.2-era).
