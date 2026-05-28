---
phase: 17
slug: psrc-census-char-select-beachhead
status: planned
nyquist_compliant: true
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
| **Quick run command** | build SwgClient (Debug) via `src/build/win32/swg.sln` (full-path msbuild, `/nodeReuse:false`, PowerShell; delete `stage/SwgClient_d.exe` first), link-grep for 0 `unresolved external symbol` |
| **Full suite command** | boot `rasterMajor=5` then `rasterMajor=11` to char-select; capture matched A/B screenshots; assert `id=342==0 && id=343==0` in `stage/d3d11-debug.log` |
| **Estimated runtime** | ~minutes (manual boot + capture) |

---

## Sampling Rate

- **After every task commit:** Debug SwgClient link gate (0 unresolved externals); shared-`clientGraphics`-touch tasks (Plan 01) additionally boot-gate `rasterMajor=5` AND `=11`.
- **After every plan wave:** dual-renderer boot to char-select (`=5` and `=11`); recompile/reflection waves capture matched A/B screenshots.
- **Before `/gsd:verify-work`:** both renderers boot clean; `id=342/343==0`; committed matched A/B pair exists for each CHAR-0x claim; HLSL:asm ratio + skinning-stream recorded.
- **Max feedback latency:** one build+boot cycle.

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 17-01-01 | 01 | 1 | CHAR-01/02/03 (retain+flag) | — | new member is decl-only; no read yet | build/link | Debug link → 0 `unresolved external symbol` | ✅ | ⬜ pending |
| 17-01-02 | 01 | 1 | CHAR-01/02/03 (retain+census) | T-17-01 / T-17-03 | PSRC chunk read via getChunkLengthLeft + null-terminate; free in dtor+reload; D3D9 byte-for-byte except storing text | build+dual boot+log | Debug link 0 unresolved + boot `=5` and `=11` no crash + `grep -c "id=342"`/`"id=343"` == 0 | ✅ | ⬜ pending |
| 17-01-03 | 01 | 1 | (gate) census ratio | T-17-02 | flag default-off; read-only diagnostic | manual+log | boot `=11` w/ psrcCensus=true → census log → HLSL:asm ratio recorded in COMPARISON.md | ❌ artifact | ⬜ pending (checkpoint) |
| 17-02-01 | 02 | 2 | CHAR-01 | T-17-04 / T-17-05 | compile failure → magenta tombstone (no crash); bump cache version | build+boot+log | Debug link 0 unresolved + boot `=11` no crash + `id=342`/`id=343` == 0 | ✅ | ⬜ pending |
| 17-02-02 | 02 | 2 | CHAR-01 | T-17-06 | m_exe never to CreatePixelShader; D3D9 unregressed | screenshot A/B | committed matched `=5`/`=11` pair; single-stage control matches D3D9 | ❌ pair | ⬜ pending (checkpoint) |
| 17-03-01 | 03 | 3 | CHAR-02/03 (D-04) | T-17-07 / T-17-08 | D3DReflect non-fatal; full-struct updatePS; no PSSet* direct; no register folklore | build+boot+log | Debug link 0 unresolved + boot `=11` no crash + `id=342`/`id=343` == 0 | ✅ | ⬜ pending |
| 17-03-02 | 03 | 3 | (confirm) skinning | — | confirmation only; no code change | RenderDoc A/B (D-08) | mesh-viewer capture; record single vs multi-stream in COMPARISON.md | ❌ RenderDoc | ⬜ pending (checkpoint) |
| 17-03-03 | 03 | 3 | CHAR-02, CHAR-03 | T-17-09 | D3D9 unregressed; blend factors deferred; SRVs _UNORM | screenshot A/B (committed pairs, D-07) | committed matched eye + head pairs match D3D9; fresh eye A/B verifies Iter-44A depth | ❌ pairs | ⬜ pending (checkpoint) |

*(Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky. "File Exists" = the automated gate channel/artifact is already present.)*

---

## Wave 0 Requirements

- [ ] `stage/d3d11-debug.log` emission path + `id=342`/`id=343` counters confirmed live (RESEARCH A4 — log channel verified present).
- [ ] char-select COMPARISON dir + `{anchor}_{renderer}_{shot}.jpg` naming convention created (Plan 01 Task 3; mirror `docs/research/phase12-baseline/COMPARISON.md`).
- [ ] RenderDoc install (or engine-diagnostics fallback) for the D-08 single-stream-vs-multistream mesh A/B (NOT installed per RESEARCH A5; does NOT block CHAR-01/02/03 — Plan 03 Task 2 / user_setup in Plan 01).
- [ ] PSRC-retain member (`m_psrcText`/`m_psrcLen`) + shared `psrcCensus` flag landed (Plan 01 Tasks 1-2).

*No code-test framework to install — renderer validation is boot/capture/log-based.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| HLSL:asm ratio recorded | (gate) | Requires a real char-select boot against packed TREs | Boot `=11` with psrcCensus=true; aggregate census log; record ratio + per-shader table in COMPARISON.md (Plan 01 T3) |
| Skin/clothing diffuse correct under D3D11 | CHAR-01 | Visual parity — no pixel-test harness | Boot `=5` and `=11` to char-select default pose; capture matched single-stage-control pair; eyeball-diff; commit pair (Plan 02 T2) |
| Eyes correct (palette color, seated, occluded) | CHAR-02 | Visual + depth; fresh A/B required (Iter-44A already wired depth) | Capture fresh A/B; confirm not gray, seated in face, not visible through back of head; commit pair (Plan 03 T3) |
| Head/face multi-stage composite correct | CHAR-03 | Visual parity, multi-sampler | Capture matched pair of `sul_*_head.sht`/`sul_eye.sht` regions; eyeball-diff; commit pair (Plan 03 T3) |
| Single-stream vs multi-stream skinning confirmed | (gates CHAR-03 attribution) | RenderDoc mesh-viewer inspection | RenderDoc capture at char-select; inspect mesh VB streams A/B; record (Plan 03 T2) |

---

## Validation Sign-Off

- [ ] Every CHAR-0x requirement has a committed matched A/B screenshot pair before the claim is marked done (D-07)
- [ ] Sampling continuity: dual-renderer boot gate on every shared `clientGraphics` edit (Plan 01)
- [ ] Wave 0 covers debug-log + capture-convention + RenderDoc prerequisites
- [ ] No watch-mode flags
- [ ] `id=342==0 && id=343==0` asserted at phase close
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
