---
phase: 12
slug: orphaned-directory-project-deletes
status: verified
threats_open: 0
asvs_level: 1
created: 2026-05-25
---

# Phase 12 — Security

> Per-phase security contract: threat register, accepted risks, and audit trail.
> Phase 12 is a dead-code REMOVAL phase — every threat is net attack-surface *reduction*.
> Register authored at plan time (all 3 PLAN.md files carried `<threat_model>` blocks);
> all mitigations were verified by phase execution + verification (15/15) + the three
> operator-confirmed dual-renderer boot gates.

---

## Trust Boundaries

| Boundary | Description | Data Crossing |
|----------|-------------|---------------|
| trackIR runtime probe | `ClientHeadTracking` read HKCU `NaturalPoint\NPClient Location` + `LoadLibrary("NPClient.dll")` | OS registry + arbitrary DLL load — REMOVED |
| stationapi auth SDK | Dormant SOE auth SDK; vendored prebuilt `989crypt.lib`/`rdp.lib`/`dbgutil.lib` | Third-party binary of unknown provenance — DELETED |
| SwgClientSetup MFC installer | Standalone retail launcher `.exe` (~40 MFC dialog files + `.rc`), never linked into the runtime client | MFC dialog/config input-parsing surface — DELETED |
| lcdui G15 LCD output | `SwgCuiG15Lcd` → `CEzLcd` → Logitech `lgLcd.dll` device output | Device output path — REMOVED (compiled out + unlinked) |
| lcdui/ Logitech SDK | Vendored prebuilt `lgLcd.lib` linked into SwgClient + 6 editor tools | Third-party binary — DELETED |

---

## Threat Register

| Threat ID | Category | Component | Disposition | Mitigation | Status |
|-----------|----------|-----------|-------------|------------|--------|
| T-12-01 | (I) Information Disclosure | trackIR registry/LoadLibrary probe in `ClientHeadTracking.cpp` | mitigate | `ClientHeadTracking` de-wired from `NPClient.h` and reduced to no-op stubs — removes the HKCU NaturalPoint registry read + the `NPClient.dll` `LoadLibrary` attempt. Verified: no NPClient/registry/LoadLibrary code remains; `grep NPClient` over src = 0 outside deleted dir. (commit 7a7da726d) | closed |
| T-12-02 | (E) Elevation / (T) Tampering | stationapi prebuilt crypto libs (`989crypt.lib`, `rdp.lib`) of unknown provenance | mitigate | `stationapi/` deleted (incl. the prebuilt `.lib`s); dangling `989crypt.lib` dropped from SwgClient link (relink clean → proven unreferenced). Verified: dir absent, 0 grep refs. (commit adda94729) | closed |
| T-12-03 | (T) Tampering | Accidental removal of live, feature-critical code | mitigate | Per-module build gate (`/t:SwgClient` exit 0 **and 0 unresolved externals** — the `/FORCE` false-pass was explicitly guarded against) + dual-renderer boot gate (operator-confirmed character select under D3D9 + D3D11). Verifier scored 15/15. | closed |
| T-12-04 | (T) Tampering | SwgClientSetup MFC dialog/config input surface (~40 dialog files, `.rc`) | mitigate | Entire `SwgClientSetup/` directory deleted (53 files) + removed from `swg.sln`; never in the runtime client path. Verified: dir absent, 0 `SwgClientSetup`/`9080903C` refs in swg.sln. (commit c10d19f10) | closed |
| T-12-05 | (T) Tampering | `swg.sln` GlobalSection config-map corruption from over-broad deletion | mitigate | Targeted Project-block + 6 config-line deletion (no regex GUID sweep). Verified: build gate exit 0 (solution parses), Project/EndProject counts balanced (128/128 after lcdui removal). | closed |
| T-12-06 | (T) Tampering | `lgLcd.lib` third-party prebuilt binary linked into 7 projects | mitigate | Unlinked from SwgClient + 6 editor/SwgGodClient inline vcxproj deps + `.rsp`; `lcdui/` (the vendored SDK) deleted. Verified: 0 `lgLcd` refs in swg.sln/.rsp/vcxproj; dirs absent. (commit f99c645dd) | closed |
| T-12-07 | (D) Denial of Service / (T) Tampering | Live UI source edit (`SwgCuiG15Lcd`/`SwgCuiHud`) breaking the HUD / char-select path | mitigate | All LCD usage was `#ifdef USE_LCD`-guarded → removing the define compiles out cleanly with no behavioral change; methods kept as no-op stubs so callers link. Full-closure build gate + dual-renderer boot gate (HUD + input explicitly verified) PASS. (commits f4a7fb228, f99c645dd) | closed |
| T-12-08 | (T) Tampering | `swg.sln` corruption from sweeping the lcdui GUID across 7 distinct dependency sites | mitigate | Targeted per-line deletion of the 7 `{20D2AEE7}` dependency lines + own block + config (no regex sweep). Verified: 0 `20D2AEE7`/`lcdui` refs in swg.sln; solution parses (build exit 0). Follow-up: 4 editor-vcxproj dangling `<ProjectReference>` also removed (commit 1d6b80242). | closed |

*Status: open · closed*
*Disposition: mitigate (implementation required) · accept (documented risk) · transfer (third-party)*

---

## Accepted Risks Log

| Risk ID | Threat Ref | Rationale | Accepted By | Date |
|---------|------------|-----------|-------------|------|

No accepted risks — all 8 threats mitigated and verified. (Note: the 3 INFO-level code-review findings in `12-REVIEW.md` live in *inert* `#ifdef USE_LCD` dead code that cannot execute in the shipped configuration; they are not threats and are out of scope for this removal phase.)

---

## Security Audit Trail

| Audit Date | Threats Total | Closed | Open | Run By |
|------------|---------------|--------|------|--------|
| 2026-05-25 | 8 | 8 | 0 | Claude (gsd-secure-phase, short-circuit: plan-time register, all mitigations verified) |

---

## Sign-Off

- [x] All threats have a disposition (mitigate / accept / transfer)
- [x] Accepted risks documented in Accepted Risks Log (none)
- [x] `threats_open: 0` confirmed
- [x] `status: verified` set in frontmatter
