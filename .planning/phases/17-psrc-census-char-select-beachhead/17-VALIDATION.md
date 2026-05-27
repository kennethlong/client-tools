---
phase: 17
slug: psrc-census-char-select-beachhead
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-05-27
---

# Phase 17 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | none — renderer change has no unit-test harness; validation is build-boot-capture + screenshot A/B + debug-log assertions (per 17-RESEARCH.md Validation Architecture) |
| **Config file** | none |
| **Quick run command** | build SwgClient (Debug) via `src/build/win32/swg.sln`, link-grep for 0 unresolved externals |
| **Full suite command** | boot `rasterMajor=5` then `rasterMajor=11` to char-select; capture matched A/B screenshots; assert `id=342==0 && id=343==0` in `stage/d3d11-debug.log` |
| **Estimated runtime** | ~minutes (manual boot + capture) |

---

## Sampling Rate

- **After every task commit:** Debug+Release SwgClient link gate (0 unresolved externals); D3D9-path-touch tasks additionally boot-gate `rasterMajor=5`.
- **After every plan wave:** dual-renderer boot to char-select (`=5` and `=11`); census/recompile waves capture matched A/B screenshots.
- **Before `/gsd:verify-work`:** both renderers boot clean; `id=342/343==0`; committed matched A/B pair exists for each CHAR-0x claim.
- **Max feedback latency:** one build+boot cycle.

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 17-01-01 | 01 | 1 | CHAR-01/02/03 (census) | T-17-01 / — | PSRC chunk read bounds-checked / null-terminated; compile failure → magenta tombstone, never crash | manual+log | build + char-select boot → census log written | ❌ W0 | ⬜ pending |

*(Populated in full by the planner / nyquist auditor against the final PLAN.md task list. Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky)*

---

## Wave 0 Requirements

- [ ] Confirm `stage/d3d11-debug.log` emission path + `id=342`/`id=343` counters are live (RESEARCH.md A4).
- [ ] Confirm matched-screenshot capture workflow + COMPARISON-style dir/naming convention (RESEARCH.md A5, mirror `docs/research/phase12-baseline/COMPARISON.md`).
- [ ] RenderDoc availability for the D-08 single-stream-vs-multistream mesh A/B (RESEARCH.md notes it is NOT installed — install or fallback; does NOT block CHAR-01/02/03).

*No code-test framework to install — renderer validation is boot/capture/log-based.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Skin/clothing diffuse correct under D3D11 | CHAR-01 | Visual parity — no pixel-test harness | Boot `=5` and `=11` to char-select default pose; capture matched pair; eyeball-diff; commit pair |
| Eyes correct (palette color, seated, occluded) | CHAR-02 | Visual + depth; fresh A/B required (Iter-44A already wired depth) | Capture fresh A/B; confirm not gray, seated in face, not visible through back of head |
| Head/face multi-stage composite correct | CHAR-03 | Visual parity, multi-sampler | Capture matched pair of `sul_*_head.sht`/`sul_eye.sht` regions; eyeball-diff |
| Single-stream vs multi-stream skinning confirmed | (gates CHAR-03 attribution) | RenderDoc mesh-viewer inspection | RenderDoc capture at char-select; inspect mesh VB streams A/B |

---

## Validation Sign-Off

- [ ] Every CHAR-0x requirement has a committed matched A/B screenshot pair before the claim is marked done
- [ ] Sampling continuity: dual-renderer boot gate on every shared `clientGraphics` edit
- [ ] Wave 0 covers debug-log + capture-convention + RenderDoc prerequisites
- [ ] No watch-mode flags
- [ ] `id=342==0 && id=343==0` asserted at phase close
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
