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
> The executor runs on **Windows PowerShell** — the locked commands below are PowerShell
> `Select-String` forms; bash `rg`/`grep` are cross-references only.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | None (C++ game client; no unit-test harness) |
| **Config file** | none |
| **Quick run command** | `(Get-ChildItem -Recurse -Include *.rsp,*.cpp,*.h,*.inl src \| Select-String -Pattern 'videocapture\|VideoCapture\|AudioCapture').Count -eq 0` (excluding `.planning/`) |
| **Full suite command** | Build SwgClient Debug + Release via `swg.sln` (locked command below), apply the 3-condition Select-String gate to each link log, then dual-renderer boot to character select |
| **Estimated runtime** | ~build time (single-config link ~minutes); boot gate is manual |

---

## Locked Link-Log Capture + Gate (the /FORCE false-pass guard, D-06)

> Authored in Plan 13-01 Task 1; recorded verbatim in 13-01-SUMMARY.md; reused by Plan 13-03 Task 2.

**Build + capture (PowerShell, from repo root):**
```
msbuild src\build\win32\swg.sln /t:SwgClient /p:Configuration=Debug   /p:Platform=Win32 /verbosity:detailed 2>&1 | Out-File -Encoding utf8 link-debug.log
msbuild src\build\win32\swg.sln /t:SwgClient /p:Configuration=Release /p:Platform=Win32 /verbosity:detailed 2>&1 | Out-File -Encoding utf8 link-release.log
```

**Stale-obj mitigation (Pitfall 5) — run BEFORE each verification link:**
```
Remove-Item -ErrorAction SilentlyContinue src\compile\win32\clientGraphics\Debug\SwgVideoCapture.obj, src\compile\win32\clientGraphics\Release\SwgVideoCapture.obj
```

**3-condition gate (all on the SAME log file; NOT MSBuild exit code):**
```
# (a) /VERBOSE captured — guards against a false-green empty log
(Select-String -Path link-<cfg>.log -Pattern 'Searching .*\.lib').Count -gt 0
# (b) zero unresolved external
(Select-String -Path link-<cfg>.log -Pattern 'unresolved external symbol').Count -eq 0
# (c) zero missing-lib (deleted-lib-still-referenced; distinct from unresolved)
(Select-String -Path link-<cfg>.log -Pattern 'LNK1181|cannot open input file').Count -eq 0
```

**Primary/secondary:** DEBUG is the **blocking-primary** signal (live capture symbols compile only
under `#if PRODUCTION==0`, i.e. Debug). RELEASE is **required confirmation** (it can link clean even
with an incomplete Debug removal). Bash cross-ref: `grep -c "unresolved external symbol" link-debug.log` → 0;
`grep -Ec "LNK1181|cannot open input file" link-debug.log` → 0; `grep -c "Searching .*\.lib" link-debug.log` → >0.

---

## Sampling Rate

- **After every task commit:** `.vcxproj` link/include-list grep (capture tokens removed) + source grep
- **After every plan wave:** Build SwgClient Debug; apply the 3-condition gate to link-debug.log
- **Before phase verification:** Debug AND Release both pass the 3-condition gate; full repo grep == 0 for `.rsp`/source/include **and `.vcxproj`**
- **Max feedback latency:** one Debug link cycle

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 13-01-T1 | 13-01 | 1 (W0) | DECRUFT-04 (crit #2) | T-13-01 | Lock the /FORCE-guard link-log capture before relying on it | build smoke + log-capture | `(Select-String -Path link-debug.log -Pattern 'Searching .*\.lib').Count -gt 0` (proves /VERBOSE captured) | ❌ W0 (link-log capture step — this task authors it) | ⬜ pending |
| 13-01-T2 | 13-01 | 1 | DECRUFT-04 (crit #2) | T-13-01, T-13-02 | Atomic caller+wrapper+lib removal; no silent unresolved-external/LNK1181 under `/FORCE` | source grep + build + 3-condition link-grep | Debug (primary): `'Searching .*\.lib'>0 -and 'unresolved external symbol'==0 -and 'LNK1181\|cannot open input file'==0` on link-debug.log; same on link-release.log (confirmation) | ✅ (Select-String available) | ⬜ pending |
| 13-02-T1 | 13-02 | 1 | DECRUFT-04 (crit #1) | T-13-02, T-13-03 | Remove dead recorder source residue; live AIL_* playback untouched | source grep + targeted controls | `(Select-String <3 source areas> -Pattern 'videoCapture\|VideoCapture\|AudioCapture\|SwgAudioCapture').Count -eq 0`; removed fn names == 0; a known live `AIL_*` call present | ✅ | ⬜ pending |
| 13-02-T2 | 13-02 | 1 | DECRUFT-04 (crit #1) | T-13-02 | Drop videocapture include path (build-inert) | grep gate | `(Select-String -Path clientGame.vcxproj,clientAudio.vcxproj,<3 includePaths.rsp> -Pattern 'videocapture').Count -eq 0` | ✅ | ⬜ pending |
| 13-02-T3 | 13-02 | 1 | DECRUFT-04 (crit #1) | T-13-02 | Purge vestigial .rsp + editor/aux .vcxproj refs (LOCAL acceptance; enumerated + reconciled) | enumerate + grep gate (excl. vendored tree) | LOCAL: `rg -li "videocapture\|VideoCapture\|AudioCapture" --glob "*.rsp" --glob "!src/external/3rd/library/videocapture/**" src` → 0; same over this plan's 12 owned .vcxproj → 0 | ✅ (rg + Select-String available) | ⬜ pending |
| 13-03-T1 | 13-03 | 2 | DECRUFT-04 (crit #1, D-04) | T-13-01, T-13-04 | Delete vendored tree last; post-Wave-1-merge full-repo grep INCLUDING .vcxproj | dir-delete + repo-wide grep (incl .vcxproj) | `(Test-Path src\external\3rd\library\videocapture) -eq $false -and (Get-ChildItem -Recurse -Include *.rsp,*.cpp,*.h,*.inl,*.vcxproj src \| Select-String -Pattern 'videocapture\|VideoCapture\|videoCapture\|AudioCapture').Count -eq 0`; + the 4 build-path .vcxproj clean | ✅ | ⬜ pending |
| 13-03-T2 | 13-03 | 2 | DECRUFT-04 (crit #2) | T-13-01 | Re-run Debug (primary) + Release (confirmation) link gate after tree delete | build + 3-condition link-grep | Debug + Release each: `'Searching .*\.lib'>0 -and 'unresolved external symbol'==0 -and 'LNK1181\|cannot open input file'==0` on link-<cfg>.log | ✅ | ⬜ pending |
| 13-03-T3 | 13-03 | 2 | DECRUFT-04 (crit #3, D-05) | — | Client boots clean post-removal under both renderers | manual boot gate | Launch `SwgClient_d.exe` (reads `client_d.cfg`) with `rasterMajor=5` then `=11` against the VM; record per-renderer evidence | ❌ manual | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] **Link-log capture step (LOCKED in Plan 13-01 Task 1)** — build SwgClient with link output
  redirected so the `/VERBOSE` link text can be grepped for `unresolved external symbol` AND `LNK1181`
  (the `/FORCE` false-pass guard from D-06). Authored for **both** Debug and Release using the LOCKED
  PowerShell command in §"Locked Link-Log Capture + Gate" above; recorded verbatim in 13-01-SUMMARY.md
  for Plan 13-03 reuse. Phase 12 did this ad-hoc — now formalized.
- [x] **Grep-scope decision** — RESOLVED (per the cross-AI reviews): the FINAL criterion-#1 gate is
  repo-wide INCLUDING `.vcxproj` (D-04 "repo-wide" intent), owned by Plan 13-03 Task 1. Wave-1 plans
  carry LOCAL acceptance over their own files only; the cross-cutting all-`.vcxproj`/`.rsp` check is the
  post-Wave-1-merge gate in Plan 13-03.
- [ ] No test files / framework install needed — this is a build+boot validated phase.

*(`wave_0_complete` flag stays `false` here; flip it at execution time when 13-01 Task 1 lands.)*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Boots to character select under D3D9 | DECRUFT-04 (crit #3) | Requires live SWGSource VM + GPU + human observation | Set `rasterMajor=5` in `client_d.cfg`; launch `SwgClient_d.exe`; confirm reach character select; record evidence (config path, exe, result, any FATAL) |
| Boots to character select under D3D11 | DECRUFT-04 (crit #3) | Requires live SWGSource VM + GPU + human observation | Set `rasterMajor=11` in `client_d.cfg`; launch `SwgClient_d.exe`; confirm reach character select; record evidence |

*Build + link-grep gates are automatable; the boot gate is human-run, consistent with prior phases.*

---

## Validation Sign-Off

- [ ] All tasks have an automated grep/build verify or a documented manual boot gate
- [ ] Sampling continuity: every removal task has a same-commit grep verify; no 3 consecutive tasks without an automated check
- [ ] Wave 0 covers the link-log capture step (the `/FORCE` guard) — LOCKED in 13-01 Task 1
- [ ] No watch-mode flags
- [ ] Link-grep gate runs on Debug (blocking-primary) AND Release (confirmation) link logs, with the /VERBOSE marker + unresolved-external + LNK1181 checks
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
