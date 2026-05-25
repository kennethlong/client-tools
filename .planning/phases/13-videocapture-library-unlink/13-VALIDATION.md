---
phase: 13
slug: videocapture-library-unlink
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-05-25
---

# Phase 13 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.
> This is a dead-code **removal** phase in a C++ game client — there is no unit-test
> harness. Validation is **build-gate + link-output grep + manual dual-renderer boot**.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | None (C++ game client; no unit-test harness) |
| **Config file** | none |
| **Quick run command** | `rg -i "videocapture\|AudioCapture" --glob "*.rsp" --glob "*.{cpp,h,inl}"` → 0 hits (excluding `.planning/`) |
| **Full suite command** | Build SwgClient Debug + Release via `swg.sln`, grep each link log for `unresolved external symbol` (== 0), then dual-renderer boot to character select |
| **Estimated runtime** | ~build time (single-config link ~minutes); boot gate is manual |

---

## Sampling Rate

- **After every task commit:** `.vcxproj` link/include-list grep (capture tokens removed) + source grep
- **After every plan wave:** Build SwgClient Debug; grep link log for `unresolved external symbol` (== 0)
- **Before phase verification:** Debug AND Release both link-clean (link-log grep), full repo grep == 0 for the `.rsp`/source/include scope
- **Max feedback latency:** one Debug link cycle

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 13-XX-XX | TBD | TBD | DECRUFT-04 (crit #1) | — | Removing debug-recorder console-command path reduces input surface | grep gate | `rg -i "videocapture\|AudioCapture" --glob "*.rsp" --glob "*.{cpp,h,inl}"` → 0 | ✅ (rg available) | ⬜ pending |
| 13-XX-XX | TBD | TBD | DECRUFT-04 (crit #2) | T-13-01 | No silent unresolved-external under `/FORCE` | build + link-grep | Build SwgClient Debug + Release; grep each link log for `unresolved external symbol` → 0 (NOT exit code) | ❌ W0 (link-log capture step) | ⬜ pending |
| 13-XX-XX | TBD | TBD | DECRUFT-04 (crit #3) | — | Client boots clean post-removal under both renderers | manual boot gate | Launch `SwgClient_d.exe` (reads `client_d.cfg`) with `rasterMajor=5` then `=11` against the VM | ❌ manual | ⬜ pending |

*Task IDs finalized by the planner. Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] **Link-log capture step** — build SwgClient with link output redirected so the `/VERBOSE` link text can be grepped for `unresolved external symbol` (the `/FORCE` false-pass guard from D-06). Must be authored for **both** Debug and Release; Phase 12 did this ad-hoc — formalize the command.
- [ ] **Grep-scope decision** — lock whether the criterion-#1 zero-reference grep includes `.vcxproj` inline refs (D-04 "repo-wide" intent) vs. only `.rsp`/source/include (literal criterion #1). See RESEARCH Open Question 1.
- [ ] No test files / framework install needed — this is a build+boot validated phase.

*If none: "Existing infrastructure covers all phase requirements."*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Boots to character select under D3D9 | DECRUFT-04 (crit #3) | Requires live SWGSource VM + GPU + human observation | Set `rasterMajor=5` in `client_d.cfg`; launch `SwgClient_d.exe`; confirm reach character select |
| Boots to character select under D3D11 | DECRUFT-04 (crit #3) | Requires live SWGSource VM + GPU + human observation | Set `rasterMajor=11` in `client_d.cfg`; launch `SwgClient_d.exe`; confirm reach character select |

*Build + link-grep gates are automatable; the boot gate is human-run, consistent with prior phases.*

---

## Validation Sign-Off

- [ ] All tasks have an automated grep/build verify or a documented manual boot gate
- [ ] Sampling continuity: every removal task has a same-commit grep verify; no 3 consecutive tasks without an automated check
- [ ] Wave 0 covers the link-log capture step (the `/FORCE` guard)
- [ ] No watch-mode flags
- [ ] Link-grep gate runs on Debug AND Release link logs
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
