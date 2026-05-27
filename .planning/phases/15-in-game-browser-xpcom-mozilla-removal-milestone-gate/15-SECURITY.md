---
phase: 15
slug: in-game-browser-xpcom-mozilla-removal-milestone-gate
status: verified
threats_open: 0
asvs_level: 1
created: 2026-05-26
---

# Phase 15 — Security

> Per-phase security contract: threat register, accepted risks, and audit trail.
>
> Phase 15 is a **removal/decruft** phase — it deletes the vendored XPCOM/Mozilla
> in-game browser engine and its bridge, then runs the dual-renderer boot gate that
> closes the v2.1 Decruft milestone. The threat surface is dominated by **supply-chain
> reduction** (removing a dormant, unpatched browser engine — net-positive) and
> **removal-integrity** (not shipping a silently-broken binary, not over-deleting a
> live dependency). No new runtime attack surface is introduced.

---

## Trust Boundaries

| Boundary | Description | Data Crossing |
|----------|-------------|---------------|
| client → rendered HTML/URL (WebBrowser widget) | **Removed.** Deleting `SwgCuiWebBrowserWidget` + `setURL`/`createWebBrowserPage` eliminates an HTML/URL-rendering surface and its XPCOM parser. Net-positive; no new boundary added. | Arbitrary HTML/URL (eliminated) |
| TCG middleware → URL navigation | **Severed (D-04).** The `navigateProc`→`setURL` tie let TCG-supplied URLs drive the browser; severing it removes the open-redirect-shaped latent path. | TCG-supplied URLs (eliminated) |
| source/build state → shipped binary | A `/FORCE`-masked unresolved external could ship a silently-broken binary (integrity). | Compiled symbols / link closure |
| on-disk .rsp / .vcxproj refs → repo grep-zero | Removing the last Mozilla-family tokens from .rsp + editor .vcxproj (criterion #1). Editors are not shipped; .rsp are vestigial — no runtime boundary. | Build config tokens |
| on-disk vendored XPCOM SDK → repo grep-zero | Deleting the last XPCOM/Gecko engine source (1,866 files incl. `.xpt`) from the tree. Supply-chain reduction. | Vendored engine source |
| swg.sln build graph → shipped binary | A dangling project GUID or `/FORCE`-masked unresolved external could break the build graph or ship a broken binary (integrity). | Project GUID / dependency edges |
| full removed-subsystem set → repo grep-zero | Milestone-wide residue boundary (P12–P15); a missed token means a removal was incomplete. | Removed-subsystem tokens |
| UITypeID enum ordinal → runtime RTTI focus routing | The D-02 ordinal-shift surface — only the running client exercises it; link + grep cannot. | RTTI type identity / focus routing |
| client ↔ SWGSource VM | Inbound network at boot; browser handlers removed → unhandled = ignored. No new boundary. | Inbound game protocol |

---

## Threat Register

| Threat ID | Category | Component | Disposition | Mitigation | Status |
|-----------|----------|-----------|-------------|------------|--------|
| T-15-01 | Tampering / supply-chain | Vendored XPCOM/Gecko browser engine + bridge reachable in the binary (latent CVEs) | mitigate | **Headline security win.** 3 `SwgCuiWebBrowser*` consumers deleted; `libMozilla.lib`/XPCOM libs unlinked from SwgClient (15-01); vendored `libMozilla/` tree (1,866 files) deleted (15-03). Verified: tree DELETED, `nsIWebBrowser`/`nspr4`/`plc4`/`profdirserviceprovider` → 0, `CuiWebBrowser*`/`UIWebBrowserWidget` → 0. (VERIFICATION truths 1,2,6) | closed |
| T-15-02 | Spoofing / open-redirect (latent) | Dead TCG URL-navigation path (`navigateProc` → `SwgCuiWebBrowserManager::setURL`) reachable via the TCG callback | mitigate | D-04: `navigateProc`/`navigateWithPostDataProc` defns + their `libEverQuestTCG` registration severed entirely; no proc remains that a TCG-supplied URL can drive. Verified: `SwgCuiTcgControl.h:93` browser clause dropped; no navigate-proc/registration refs in SwgCui. **Residual future-revival sliver recorded as AR-15-01.** (VERIFICATION truth 4) | closed |
| T-15-03 | Tampering (silent broken binary) | `/FORCE` + `/VERBOSE` downgrades a leftover unresolved external to a warning and still emits a binary at exit 0 | mitigate | D-13: Debug AND Release link logs greped for `unresolved external symbol` == 0 — never trusts MSBuild exit 0. Optimized exempt (pre-existing DEF-14-01 SAFESEH, not a removal regression). Recorded across 15-01/15-03/15-04 SUMMARYs; structurally corroborated (G15Lcd stub symbols defined, 5 phase commits present). (VERIFICATION truth 8) | closed |
| T-15-04 | Tampering (ordinal-shift regression) | Deleting `TUIWebBrowser` mid-enum shifts the ordinals of the ~30 `*Style` entries after it (the CR-01 shape) | mitigate | RESEARCH D-02 proves the `UITypeID` ordinal is NEVER serialized (string-keyed `.ui` loader) — the shift is internally consistent across recompile, unlike CR-01. Verified: `TUIWebBrowser` → 0 in UITypeID.h, no placeholder, all 5 IsA focus sites clean. **Empirical backstop:** the 15-04 dual-renderer boot gate (HUD/radial/IME focus exercise) was HUMAN-verified + user-APPROVED (commit 16fd3ac4c). *(15-01 dispositioned this `accept`; the 15-04 boot-gate backstop landed, so it is recorded `mitigate`/closed.)* (VERIFICATION truths 3,9) | closed |
| T-15-05 | Denial of Service (over-deletion) | De-wiring a sibling list/table `*Browser` UI (`SwgCuiCommandBrowser`/`SwgCuiMissionBrowser`) or a non-browser TCG member | mitigate | Scope guard: touched ONLY the `SwgCuiWebBrowserManager` include + the createWebBrowserPage/setURL block. Verified: sibling Browser UIs intact (11 refs in SwgCuiHudAction.cpp; CommandBrowser/MissionBrowser sources present); G15Lcd stubs kept. (VERIFICATION truth 5) | closed |
| T-15-06 | Denial of Service (over-deletion) | Removing a non-Mozilla token a live dep needs — `989crypt.lib` (SwgGodClient stationapi dep) or an unrelated lib | mitigate | Scope guard: removed ONLY the Mozilla family (nspr4/plc4/profdirserviceprovider_s/xpcom/xul/libMozilla.lib + dirs). Verified: `989crypt.lib` present in SwgGodClient.vcxproj; Qt/ui tokens survive. (VERIFICATION truth 7) | closed |
| T-15-07 | Tampering (false grep-zero — backslash mis-parse) | sed backslash-escaping on a `.vcxproj` path purge mangles an adjacent token (MSYS `3rd` back-reference trap) | mitigate | Substring+delimiter match `[^;<>]*TOKEN[^;<>]*;` mandated; per-editor source-assertion grep catches any mangled/surviving token. Verified: 7 editors + .rsp + SwgGodClient Mozilla tokens all → 0, no mangled tokens. (VERIFICATION truth 1) | closed |
| T-15-08 | Tampering (dangling sln GUID) | Dropping the Project block but leaving the 9 `ProjectDependency` back-refs → swg.sln references a non-existent GUID → broken build graph | mitigate | Pitfall-4 11-location list (incl. the SwgClient build-order edge); criterion #1b greps GUID + literal `libMozilla` to 0 across the whole sln. Verified: `C6C1E14A-…-5DC4`/`libMozilla` → 0 in swg.sln; SwgClient project intact (sln well-formed). (VERIFICATION truth 1) | closed |
| T-15-09 | Denial of Service (premature delete) | Deleting the vendored tree BEFORE 15-01's include-dir purge + 15-02's .rsp/editor purge → a stale `libMozilla\include` AdditionalIncludeDirectories dangles | mitigate | `depends_on: [15-01, 15-02]` enforces Wave-2 ordering; the Wave-1 merge gate re-greps repo-wide before deleting. Verified: Mozilla include config → 0, tree deleted, build link-clean. (VERIFICATION truths 1,6) | closed |
| T-15-10 | Tampering (false grep-zero — KEEP-list mishandling) | The milestone sweep false-fails on KEEP-listed residue, or false-passes via over-broad exclusion hiding a real holdout | mitigate | KEEP-list exact + path-scoped (exclude ChatAPI2/ + libs/); every surviving trackIR/lcdui hit confirmed a removal-comment or label; the XPCOM family required LITERALLY 0 (no KEEP FPs). Verified: milestone P12–P15 sweep == 0/KEEP-listed. (VERIFICATION truth 7) | closed |
| T-15-11 | Denial of Service (over-deletion in A1 cleanup) | The lcdui A1 cleanup removes a live token or deletes `SwgCuiG15Lcd.cpp/.h` (whose removal-comment is KEEP-listed) | mitigate | A1 scope STRICTLY the inert lcdui include-path strings + SwgCuiG15Lcd .vcxproj entries (the .cpp/.h stay). Verified: `SwgCuiG15Lcd.cpp` on disk; ClCompile+ClInclude kept (2); `989crypt.lib` survives; lcdui in config → 0. (VERIFICATION truth 7) | closed |

*Status: open · closed*
*Disposition: mitigate (implementation required) · accept (documented risk) · transfer (third-party)*

---

## Accepted Risks Log

| Risk ID | Threat Ref | Rationale | Accepted By | Date |
|---------|------------|-----------|-------------|------|
| AR-15-01 | T-15-02 | The "safe to unregister the TCG navigate callbacks entirely" verdict is **DEPLOYMENT-SCOPED**: it holds because `SWGTCG.dll` is absent on the SWGSource VM and `callbackTable` zero-inits + the setters are pure assignment, so the now-null navigate members are never read. If `SWGTCG.dll` is ever restored AND TCG is launched, the absent navigate callbacks are unverified against the DLL's internals — at that point prefer the D-04a fallback (registered logging-no-op procs). TCG revival is **out of scope for Phase 15** (a future v2.x concern); recorded so a future TCG-revival phase re-evaluates before relinking. Both cross-AI reviewers flagged this caveat. | Kenny Long (phase owner) | 2026-05-26 |

*Accepted risks do not resurface in future audit runs.*

---

## Security Audit Trail

| Audit Date | Threats Total | Closed | Open | Run By |
|------------|---------------|--------|------|--------|
| 2026-05-26 | 11 | 11 | 0 | Claude (gsd-secure-phase, State B from artifacts) |

Method: 11 unique threats (T-15-01 … T-15-11) extracted from the `<threat_model>` blocks of 15-01/02/03/04-PLAN.md (no `## Threat Flags` in any SUMMARY). Each mitigation classified against 15-VERIFICATION.md (independent live-tree verification, 9/9 truths PASS) and corroborated by orchestrator-run independent grep spot-checks of the headline removal claims (vendored tree deleted, swg.sln GUID=0, XPCOM/WebBrowser symbols=0, TUIWebBrowser enum=0, 989crypt.lib + sibling Browser UIs + G15Lcd stubs surviving). `threats_open: 0` — no auditor sub-agent spawned (workflow short-circuits the user gate + auditor when classification yields 0 open threats).

---

## Sign-Off

- [x] All threats have a disposition (mitigate / accept / transfer)
- [x] Accepted risks documented in Accepted Risks Log
- [x] `threats_open: 0` confirmed
- [x] `status: verified` set in frontmatter

**Approval:** verified 2026-05-26
