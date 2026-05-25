# Phase 5: Onboarding + Developer Experience - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in 05-CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-05-05
**Phase:** 5-Onboarding Developer Experience
**Areas discussed:** README scope + placement, Clean-clone verification, Phase 6 pre-flight depth

---

## README Scope + Placement

### Where should the primary build guide live?

| Option | Description | Selected |
|--------|-------------|----------|
| Root README.md | GitHub convention — first thing a visitor sees; short and focused | |
| docs/build.md (new page) | Keeps root tidy; root links to docs/ | |
| Both — terse root README + detailed docs/build.md | Root = one-screener; docs/build.md = full walkthrough | ✓ |

**User's choice:** Both — terse root README + detailed docs/build.md

---

### What should the root README cover beyond the cmake commands?

| Option | Description | Selected |
|--------|-------------|----------|
| Prereqs + commands only | VS 2022, Win11 SDK, CMake 3.27+, Git then the two commands; links to docs/ | |
| Prereqs + commands + project intro | Add 2-3 sentence "What is this?" paragraph before build steps | ✓ |
| Full onboarding narrative | Prereqs + what + why + build + runtime config outline + links | |

**User's choice:** Prereqs + commands + project intro

---

### What makes docs/build.md different from the root README?

| Option | Description | Selected |
|--------|-------------|----------|
| SDK notes + known build warnings | STLPort vc71 path, stlport_vc143_compat shim, /WX off, two source edits, post-build staging layout | |
| Step-by-step with troubleshooting | Walk each cmake step with expected output, common error messages and their fixes | |
| Architecture overview + build guide combined | 70-lib dependency graph, wave order, SDK landscape alongside build steps | ✓ |

**User's choice:** Architecture overview + build guide combined

---

### Does docs/build.md also need an HTML counterpart?

| Option | Description | Selected |
|--------|-------------|----------|
| Markdown only — no HTML | Living build guide; easier to maintain; distinct from static recon docs | ✓ |
| Both .md and .html | Consistent with existing docs/ site structure | |
| Let Claude decide | Based on docs/ site structure | |

**User's choice:** Markdown only — no HTML

---

## Clean-Clone Verification

### How do you want to satisfy DEV-01's clean-clone proof?

| Option | Description | Selected |
|--------|-------------|----------|
| Delete build/ + reconfigure locally | rm -rf build/, full cmake configure + build from scratch; fast, eliminates cached state | ✓ |
| Fresh VM / second machine | Clean Windows 11 VM with only VS 2022 + CMake + Git; definitive but 2-4h setup overhead | |
| Trust Phase 4 + document | Phase 4 proved it builds (ARTIF-01 gate); document commands without re-running | |

**User's choice:** Delete build/ + reconfigure locally (Recommended)

---

### When Phase 5 runs the reconfigure, should it also verify the Release build?

| Option | Description | Selected |
|--------|-------------|----------|
| Debug only | Phase 4 already verified Release; re-proving Debug sufficient for DEV-01 | |
| Debug + Release both | Run both configs on clean reconfigure to confirm neither regressed | ✓ |
| Let Claude decide | Based on Phase 4 results | |

**User's choice:** Debug + Release both

---

### What should the plan record as 'verification passed' for DEV-01?

| Option | Description | Selected |
|--------|-------------|----------|
| Exe produced + no error exit | cmake --build exits 0, exe exists; same bar as Phase 4 ARTIF-01 | |
| Exe produced + dumpbin gates re-run | Zero MSVCR* imports, zero xpcom imports after clean rebuild | ✓ |
| Exe produced + launch sanity | Double-click exe + confirm no missing DLL popup; Phase 4 SC-5 bar | |

**User's choice:** Exe produced + dumpbin gates re-run

---

## Phase 6 Pre-Flight Depth

### How deep should the sharedNetworkMessages diff go (P2.01)?

| Option | Description | Selected |
|--------|-------------|----------|
| Key divergences only | Files added/removed/renamed; note messages affecting login flow; no full line-by-line diff | ✓ |
| Full line-by-line diff, written up | Recursive diff, every changed file with context; thorough but overkill | |

**User's choice:** Key divergences only (Recommended)

---

### What format should P2.01 and P2.03 findings be recorded in?

| Option | Description | Selected |
|--------|-------------|----------|
| New doc: docs/recon/08-phase6-preflight.md | Continues recon/ numbering convention; single place Phase 6 planner reads | ✓ |
| Inline in Phase 5 CONTEXT.md | Findings travel with phase artifacts; less navigable | |
| Two separate files per task | docs/recon/p2-01-*.md + docs/recon/p2-03-*.md; more granular | |

**User's choice:** New doc: docs/recon/08-phase6-preflight.md (Recommended)

---

### For P2.03 (swg-main auth bypass), how deeply should we read the LoginServer source?

| Option | Description | Selected |
|--------|-------------|----------|
| Just the bypass mechanism | validateStationKey=0 wiring + what LoginServer does instead; exact client.cfg entry | ✓ |
| Full login flow trace | Trace full LoginClientId → EnumerateCharacterId handshake; overlaps Phase 6 planning | |
| Config only — no source reading | Just note validateStationKey=0 from loginServer.cfg; minimal effort | |

**User's choice:** Just the bypass mechanism (Recommended)

---

## Claude's Discretion

- Git tag name: user confirmed "push this and tag it" but left naming to Claude. Suggested: `v1-build-verified` or `phase5-complete`.

## Deferred Ideas

- CI / GitHub Actions automation — deferred to post-v1 (solo hobby project, manual builds sufficient)
- `/WX` warnings-as-errors re-enable — deferred to post-v1 per FLAGS-02
- Full LoginClientId → EnumerateCharacterId handshake trace — Phase 6 scope, not P2.03
- Push to remote: user requested "push this and tag it" — fold into Phase 5 plan as a final step after DEV-01 gate passes; planner should include a git push + git tag step
