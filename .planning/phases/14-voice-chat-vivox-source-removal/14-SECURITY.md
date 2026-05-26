---
phase: 14
slug: voice-chat-vivox-source-removal
status: verified
threats_open: 0
asvs_level: 1
created: 2026-05-26
---

# Phase 14 — Security

> Per-phase security contract: threat register, accepted risks, and audit trail.
> Phase 14 is a dead-code removal (Vivox voice-chat subsystem). Voice was off-by-default
> and there is no Vivox backend on the SWGSource VM — net effect is a reduction in attack surface.

---

## Trust Boundaries

| Boundary | Description | Data Crossing |
|----------|-------------|---------------|
| inbound network → `sharedNetworkMessages` deserializers | Removing the 3 `voicechat/*` `Archive::ReadIterator` parsers REDUCES inbound attack surface; no new boundary added | voice channel/account network payloads (now rejected/ignored) |
| source/build state → shipped binary | A `/FORCE`-masked unresolved external could ship a silently-broken binary | linker symbol-resolution integrity |
| on-disk vendored source → repo grep-zero | Deleting the last vivox/Vivox source literals (3 vendored trees) | third-party SDK source (removed) |
| client ↔ SWGSource VM (192.168.1.200) | Inbound object-menu / command messages at boot; voice handlers removed → unhandled = engine-ignored | radial/command dispatch (graceful no-op) |

---

## Threat Register

| Threat ID | Category | Component | Disposition | Mitigation | Status |
|-----------|----------|-----------|-------------|------------|--------|
| T-14-01 | Tampering (integrity) | de-wire live non-voice caller `SwgCuiHudWindowManager.cpp:318-319` (getVoiceShowFlybar→WS_VoiceFlyBar) | mitigate | De-wired in 14-01 (`eb9b68987`); `rg getVoiceShowFlybar\|WS_VoiceFlyBar` → 0; build compiled | closed |
| T-14-02 | Tampering (silent broken binary) | `/FORCE` masks unresolved externals in SwgClient link | mitigate | D-09 link-grep: Debug+Optimized+Release `unresolved external symbol` == 0 (not exit-0) | closed |
| T-14-03 | DoS (availability) | removing a shared lib token a non-voice consumer needs (`libsndfile`, `soePlatform\libs`) | mitigate | `soePlatform/libs/Win32-{Debug,Release}/Base.lib` preserved; only voice tokens removed; A2 grep confirmed libsndfile has no non-voice consumer | closed |
| T-14-04 | DoS (orphaned dispatch) | inbound voice network message after handler removed | accept | Engine default-ignores unbound messages; voice off-by-default; no Vivox backend on the VM | closed |
| T-14-05 | Tampering (over-removal) | removing more than the `vivoxSharedWrapper` token from an editor `.vcxproj` AdditionalDependencies | mitigate | `xpcom.lib` survives 3× in each of the 7 editor `.vcxproj`; word-diff audit: 0 tokens added | closed |
| T-14-06 | DoS (availability) | removing the `soePlatform\libs` dir from `SwgGodClient.vcxproj` | accept | SwgGodClient + 7 editors are pre-broken non-build-targets — cannot break the SwgClient build/boot | closed |
| T-14-07 | Tampering (false grep-zero) | a `vivox` ref left in an unedited editor `.vcxproj` | mitigate | All 8 editor `.vcxproj` (`rg -i vivox`) == 0; backstopped by 14-03 repo-wide gate | closed |
| T-14-08 | Tampering (wrong tree delete) | deleting `soePlatform/libs` (or other subdir) instead of only `VChatAPI/` | mitigate | `soePlatform/libs/` + `ChatAPI2/` present; only `vivox/`, `vivoxSharedWrapper/`, `soePlatform/VChatAPI/` deleted | closed |
| T-14-09 | Tampering (silent broken binary) | `/FORCE` masks an unresolved external after the tree delete | mitigate | 14-03 re-captured Debug+Release link-grep == 0 AFTER the delete; dual-renderer boot gate PASS | closed |
| T-14-10 | DoS (availability) | deleting a vendored tree BEFORE the include-path purge landed | mitigate | `depends_on: [14-01,14-02]`; Wave-1 merge gate (`rg -i vivox src` matched only the 3 doomed trees) ran before any delete | closed |
| T-14-11 | DoS (orphaned dispatch) | orphaned compiled TRE radial/command/page assets after handlers removed | accept | D-06a graceful no-op + boot gate backstop. **NB:** code review (CR-01) found the related enum-ordinal shift was NOT a graceful no-op — surviving radial items remapped to wrong datatable rows — FIXED in `1bfeff6b3` (ordinal-preserving placeholders); D-06a corrected | closed |
| T-14-12 | Tampering / supply-chain | dormant third-party voice SDK with latent CVEs left on disk | mitigate | `vivox/`, `vivoxSharedWrapper/`, `soePlatform/VChatAPI/` deleted (`rg -i vivox src` == 0) — removes dormant SDK source (net security positive) | closed |
| T-14-13 | Tampering (false grep via shim) | editing the public-shim header instead of the canonical `src/shared/core` file | mitigate | Canonical `CuiPreferences.{cpp,h}` / `CuiMenuInfoTypes.h` / `SwgCuiMediatorTypes.h` / `SwgCuiActions.h` verified emptied of voice symbols; compile is the backstop | closed |

*Status: open · closed*
*Disposition: mitigate (implementation required) · accept (documented risk) · transfer (third-party)*

---

## Accepted Risks Log

| Risk ID | Threat Ref | Rationale | Accepted By | Date |
|---------|------------|-----------|-------------|------|
| AR-14-01 | T-14-04 | Inbound voice network messages have no registered handler after the manager delete → engine default = ignore (not a crash); voice off-by-default and no Vivox backend on the SWGSource VM | plan threat model (14-01) | 2026-05-26 |
| AR-14-02 | T-14-06 | `SwgGodClient` + the 7 editors are pre-broken non-build-targets (Qt); a wrong edit there cannot break the SwgClient build or boot | plan threat model (14-02) | 2026-05-26 |
| AR-14-03 | T-14-11 | Orphaned retail-TRE radial/command/page rows no-op gracefully (no in-repo datatable to scrub); boot gate is the backstop. Validity restored only after the CR-01 fix (`1bfeff6b3`) corrected the surviving-member ordinal shift | plan threat model (14-03) + CR-01 fix | 2026-05-26 |

*Documented intentional residual (not a threat): prebuilt `VChatAPI.lib` / `Base_vchat.lib` remain in `soePlatform/libs/Win32-{Debug,Release}/` by filename — criterion #1 targets vivox/Vivox source/include literals, not `.lib` filenames.*

---

## Security Audit Trail

| Audit Date | Threats Total | Closed | Open | Run By |
|------------|---------------|--------|------|--------|
| 2026-05-26 | 13 | 13 | 0 | gsd-security-auditor (sonnet) + orchestrator |

---

## Sign-Off

- [x] All threats have a disposition (mitigate / accept / transfer)
- [x] Accepted risks documented in Accepted Risks Log
- [x] `threats_open: 0` confirmed
- [x] `status: verified` set in frontmatter

**Approval:** verified 2026-05-26
