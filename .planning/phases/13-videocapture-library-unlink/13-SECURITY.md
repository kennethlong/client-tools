---
phase: 13
slug: videocapture-library-unlink
status: verified
threats_open: 0
asvs_level: 1
created: 2026-05-26
---

# Phase 13 — Security

> Per-phase security contract: threat register, accepted risks, and audit trail.
> Register authored at plan time (all 3 PLAN.md files carry `<threat_model>` blocks).
> All threats CLOSED — mitigations independently confirmed during execution + by the
> Phase 13 verifier (see Audit Trail). No auditor re-run needed (threats_open: 0,
> register authored at plan time → short-circuit per secure-phase Step 3).

---

## Trust Boundaries

| Boundary | Description | Data Crossing |
|----------|-------------|---------------|
| build → linked binary | Removed link inputs must not leave a referenced-but-unprovided symbol that `/FORCE` masks into the shipped EXE | linker symbol resolution (a masked unresolved-external in per-frame `draw()` code = latent runtime fault) |
| 3rd-party static libs → client | Removing PICTools/PICVideo (`picn20*.lib`) + capture libs reduces unmaintained binary surface | static lib link inputs |
| dead source ↔ live source | `#if 0` capture residue lives in files (Audio.cpp/.h) that also hold live Miles `AIL_*` playback — over-deletion could break live audio | C++ source / compiled symbols |
| vestigial config ↔ build graph | `.rsp` / editor-aux `.vcxproj` refs are inert; an over-broad token delete could strip a live dep shared in the same list | build-input token lists |
| referenced path → deleted tree | Deleting the vendored tree while any inline lib/include path still points at it orphans the path (LNK1181) | filesystem / linker search paths |

---

## Threat Register

| Threat ID | Category | Component | Disposition | Mitigation | Status |
|-----------|----------|-----------|-------------|------------|--------|
| T-13-01 | DoS / Tampering | SwgClient EXE link under `/FORCE` (pre- and post-tree-delete) | mitigate | ATOMIC caller+wrapper+lib removal in ONE commit (bb2e101b8) + D-06 link-log grep gate (`/VERBOSE` marker present, `unresolved external symbol`==0, `LNK1181`/`cannot open input file`==0) on Debug (blocking-primary) AND Release, re-run after the tree delete; runtime backstop = dual-renderer boot smoke | closed |
| T-13-02 | Tampering | SwgClient.vcxproj `<Link>` lists + `.rsp`/editor `.vcxproj` interleaved tokens + Audio.cpp dead-vs-live boundary | mitigate | Capture-only 15-token allow-list + explicit do-not-touch list (binkw32/bink, vivox*, zlib, dpvs, crypto/989crypt); D-02a guard (touch only dead `AudioCapture` methods, never live `AIL_*`); targeted negative controls | closed |
| T-13-03 | Tampering | Audio.h unguarded `namespace AudioCapture` fwd-decl + live audio interface | accept | LOW risk: fwd-decl only typed the dead `startAudioCapture` param; grep-confirmed zero live callers; clientAudio compiles clean under the build gate. (Residue was in fact fully removed, so the risk is moot.) | closed |
| T-13-04 | Tampering | vendored-tree delete blast radius (RESEARCH A3) | accept | LOW risk: grep-confirmed the `.vcxproj`+`.rsp` consumer set is complete; after Plans 01+02 nothing references the tree. Pre-delete grep asserted 0 external refs; negative control asserts binkw32/vivox remain | closed |

*Status: open · closed*
*Disposition: mitigate (implementation required) · accept (documented risk) · transfer (third-party)*

### Mitigation Evidence (verified this session)

- **T-13-01 — CLOSED (mitigated).** D-06 link gate, fresh post-tree-delete logs: `link-debug.log` **2138** Searching / **0** unresolved / **0** LNK1181 (blocking-primary); `link-release.log` **2104 / 0 / 0** (confirmation). Removal landed atomically (bb2e101b8). Dual-renderer boot human-confirmed (D3D9 + D3D11 both reached character select → Tatooine, no capture-related FATAL). Independently re-verified in 13-VERIFICATION.md (criteria #1/#2 by codebase grep+log, #3 by user).
- **T-13-02 — CLOSED (mitigated).** Negative controls confirmed in the specific files: `binkw32.lib` ×3 + `vivox` ×6 retained in SwgClient.vcxproj; live `AIL_*` playback intact in Audio.cpp (162 call sites; `AIL_start_sample` present); removed recorder method names (`getAudioCaptureConfig`/`startAudioCapture`/`stopAudioCapture`) == 0; zlib/dpvs/crypto preserved. Code review WR-01 (two orphaned `Audio.cpp` file-statics left by the removal) was caught and fixed (78d7373ff), tightening this boundary.
- **T-13-03 — CLOSED (accepted + moot).** Repo-wide grep for `AudioCapture` == 0; clientAudio recompiled clean under the Debug/Release build gate — the fwd-decl removal demonstrably did not affect live playback.
- **T-13-04 — CLOSED (accepted).** Pre-delete grep (incl. `.vcxproj`) confirmed 0 references outside the tree; post-delete repo-wide grep == 0; build + dual-renderer boot clean. Blast radius was as predicted (A3 LOW).

---

## Accepted Risks Log

| Risk ID | Threat Ref | Rationale | Accepted By | Date |
|---------|------------|-----------|-------------|------|
| AR-13-01 | T-13-03 | Removing the dead `namespace AudioCapture` fwd-decl in Audio.h cannot affect live audio — it only typed a dead method's param; zero live callers (grep-confirmed) and clientAudio compiles clean. Residue fully removed. | Kenny Long | 2026-05-26 |
| AR-13-02 | T-13-04 | Vendored `videocapture/` tree delete: RESEARCH A3 LOW — the consumer set (`.vcxproj`+`.rsp`) is complete and was removed by Plans 01/02; pre-delete grep proved 0 external refs; post-delete grep == 0 + build/boot clean. | Kenny Long | 2026-05-26 |

---

## Security Audit Trail

| Audit Date | Threats Total | Closed | Open | Run By |
|------------|---------------|--------|------|--------|
| 2026-05-26 | 4 | 4 (2 mitigated, 2 accepted) | 0 | /gsd:secure-phase (orchestrator; mitigations cross-confirmed by 13-VERIFICATION.md) |

---

## Sign-Off

- [x] All threats have a disposition (mitigate / accept / transfer)
- [x] Accepted risks documented in Accepted Risks Log
- [x] `threats_open: 0` confirmed
- [x] `status: verified` set in frontmatter

**Approval:** verified 2026-05-26
