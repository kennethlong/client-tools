---
phase: 24-dpvs-config-gate-machine-portability
reviewed: 2026-06-12T00:00:00Z
depth: standard
files_reviewed: 9
files_reviewed_list:
  - src/engine/client/library/clientGraphics/src/shared/ConfigClientGraphics.h
  - src/engine/client/library/clientGraphics/src/shared/ConfigClientGraphics.cpp
  - src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp
  - src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp
  - src/engine/client/library/clientAudio/src/win32/Audio.cpp
  - src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj
  - tools/setup/setup-client.ps1
  - tools/setup/client.cfg.template
  - .gitignore
findings:
  critical: 0
  warning: 4
  info: 5
  total: 9
resolved:
  fixed: 2
  wontfix: 2
status: issues_found
---

# Phase 24: Code Review Report

**Reviewed:** 2026-06-12
**Depth:** standard
**Files Reviewed:** 9
**Status:** issues_found

## Summary

Reviewed the phase-24 DPVS config-gate + machine-portability changes: the tri-state
`occlusionMode` knob (enum/parse/getter + the config-gated occlusion bit in
`RenderWorld::drawScene`), the two D-07 engine-default flips (multistream VB default,
D3D11 Bloom no-op), the one-shot Miles codec-absence warning + flood-suppression flag in
`Audio.cpp`, the Miles-redist postbuild repoint with `.m3d`-guard repair, and the
PowerShell cfg generator + template.

No critical defects (no injection, no null-deref, no auth/data-loss). The occlusion gate
is correctly null-guarded and the enum default (`DOM_off = 0`) is consistent across the
header comment, the parse fallback, and the getter. The findings below are real
correctness/portability gaps that should be addressed but are not ship-blockers.

## Warnings

### WR-01: Miles-redist postbuild repair guard misses a missing/stale `mssmp3.asi`

**Resolution:** fixed (commit abbcdcd81) — added a `mssmp3.asi` existence guard alongside the existing `.m3d` guards in BOTH the Debug and Release `<PostBuildEvent>` blocks, so a stale stage missing the mp3 decoder is now repaired.

**File:** `src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj:134-135` (Debug), `:242-243` (Release)
**Issue:** The repair path only fires when `Msseax.m3d` OR `msssoft.m3d` is absent:
```
if not exist "...\stage\miles\Msseax.m3d"  xcopy ... redist  stage\miles\
if not exist "...\stage\miles\msssoft.m3d" xcopy ... redist  stage\miles\
```
But `Audio::install`'s startup probe (Audio.cpp:1295) requires FOUR files — `mssmp3.asi`,
`mssogg.asi`, `Msseax.m3d`, `msssoft.m3d`. A stale stage that already has both `.m3d`
providers but is missing/outdated `mssmp3.asi` (the mp3 decoder) or `mssogg.asi` will pass
BOTH guards, so the postbuild skips the repair and the runtime probe then warns "silent
music + world audio." The repair logic and the runtime contract are checking different
file sets. The guard rationale (".m3d most likely absent") does not cover the documented
"has mssmp3.asi but lacks reconciled bytes" stale case in reverse.
**Fix:** Guard on the codec the runtime actually decodes with, or (simplest) gate on a
sentinel that covers the full required set. E.g. add the asi check, or always refresh:
```
if not exist "...\stage\miles\mssmp3.asi" xcopy /E /I /Y "...\miles-7.2e\redist" "...\stage\miles\"
if not exist "...\stage\miles\Msseax.m3d" xcopy /E /I /Y "...\miles-7.2e\redist" "...\stage\miles\"
```
Since `xcopy /Y` is an idempotent overwrite (per the comment), gating on the first
required file — or simply running it unconditionally — is safer than the partial `.m3d`-only set.

### WR-02: Optimized configuration has no postbuild — `_o.exe` runs against an unstaged/stale `stage/miles`

**Resolution:** wontfix — the Optimized config is a known-broken link target (pre-existing LNK1281 SAFESEH image-gen failure documented in project history); it never produces a runnable exe, so a postbuild there is dead weight. Pre-existing broken config, out of scope.

**File:** `src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj:138-183`
**Issue:** The `Optimized|Win32` `ItemDefinitionGroup` closes at line 183 with NO
`<PostBuildEvent>`. Only Debug (:123) and Release (:231) stage the exe/pdb and repair
`stage/miles`. A fresh clone built Optimized-only (or first-built Optimized) gets no
`stage/miles` population and no exe copy, reproducing exactly the "silent music + warning
flood" portability failure this phase set out to eliminate. The phase goal is "a fresh
clone now stages working audio with no external dependency" — that guarantee does not hold
for the Optimized config.
**Fix:** Add the same exe/pdb copy + Miles-redist repair `<PostBuildEvent>` to the
`Optimized|Win32` group (staging `$(ProjectName)_o.exe` / `_o.pdb`), or document that
Optimized is not a supported standalone-clone target.

### WR-03: `setup-client.ps1` Miles repair is warn-only and points devs at a postbuild that may not run for their config

**Resolution:** wontfix — by design (D-11). Staging is deliberately owned by the SwgClient postbuild, not the setup generator; copying the redist from the script would duplicate the staging responsibility.

**File:** `tools/setup/setup-client.ps1:118-130`
**Issue:** The cfg generator validates `stage/miles` and, on a miss, only prints a WARNING
("Build SwgClient -- the postbuild stages stage/miles"). Combined with WR-02, a dev who
builds the Optimized config will be told the postbuild repairs miles when it does not,
leaving them with the silent-audio state the setup script was meant to flag. The script
has the vendored redist path available (`src/external/3rd/library/miles-7.2e/redist`) and
could copy it directly rather than deferring to a build step.
**Fix:** Either copy the redist directly from the script when files are missing (the script
already computes `$repoRoot`), or make the warning conditional on the redist source
actually existing and accurate about which configs stage it.

### WR-04: `screenShot_impl` quality clamp is dead/unreachable — out-of-range quality is not clamped

**Resolution:** fixed (commit 91f37b243) — removed the unreachable `if (q < 0)` branch and kept the `> 100` upper clamp. Scoped narrower than the suggested fix: the `quality <= 0` => libjpeg-default behavior is preserved so externally-observable output for valid quality values is unchanged.

**File:** `src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp:541-547`
**Issue:** (Touched file under review.) The clamp is gated inside `if (quality > 0)`, then
re-checks `if (q < 0) q = 0;` which can never be true (already `> 0`), and `if (q > 100)`
which can fire — but a caller passing `quality <= 0` skips the whole block and calls
`jpeg_set_quality` never (libjpeg keeps its `jpeg_set_defaults` value). The `q < 0` branch
is dead code and the structure is confusing; a negative quality silently means "default"
while a >100 quality is clamped — inconsistent handling.
**Fix:** Clamp unconditionally and drop the dead branch:
```cpp
int q = quality;
if (q < 1)   q = 1;     // libjpeg quality is 1..100
if (q > 100) q = 100;
jpeg_set_quality(&cinfo, q, TRUE);
```
This is pre-existing-adjacent and low-impact (screenshots only), but it is in a file under
review and the dead `if (q < 0)` is a genuine logic defect.

## Info

### IN-01: `treRootQuoted` is computed but the bare `@TRE_ROOT@` token uses it — verify it is actually consumed

**File:** `tools/setup/setup-client.ps1:75,93`
**Issue:** `$treRootQuoted` (line 75) is used at line 93 (`$c.Replace('@TRE_ROOT@', $treRootQuoted)`), so it is consumed — but the ordering comment at line 91 ("substitute the longer @TRE_ROOT_*@ tokens BEFORE the bare @TRE_ROOT@") relies on `@TRE_ROOT_TOC0@` etc. NOT being a prefix collision. They are not (`@TRE_ROOT@` is a prefix of `@TRE_ROOT_TOC0@`), and `.Replace` of the bare token AFTER the longer ones is the correct order, so this is fine. Noting for confirmation: the hashtable iteration order of `$tocPaths.Keys` is irrelevant here because none of the long tokens are prefixes of each other.
**Fix:** None required; consider a brief inline note that the long tokens are mutually non-overlapping so hashtable iteration order is safe.

### IN-02: `occlusionMode` parse does redundant `!= NULL` checks after a non-null-defaulted getKeyString

**File:** `src/engine/client/library/clientGraphics/src/shared/ConfigClientGraphics.cpp:144-150`
**Issue:** `ConfigFile::getKeyString(..., "off")` supplies a non-null default, so
`occlusionModeStr` is effectively never NULL; the two `occlusionModeStr != NULL &&` guards
are defensive but redundant. Harmless and arguably good hygiene (T-24-01 safe-fallback
intent), just noting it is belt-and-suspenders.
**Fix:** Optional — keep as-is for defensiveness, or drop the null checks since the default is non-null.

### IN-03: `DOM_auto` comment says "outdoor on / interior off" but keys on `isWorldCell()` only

**File:** `src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp:922-923`
**Issue:** The auto branch sets the occlusion bit when `ms_cameraCell->isWorldCell()`. The
world cell is the single outdoor/top-level cell, so "in the world cell" == outdoor and any
POB interior cell is non-world == off. This matches the documented intent. Confirmed
`isWorldCell()` exists (CellProperty.h:105). No bug — flagging only because the mapping
"world cell == outdoor" is a semantic assumption a future reader should not have to
re-derive.
**Fix:** None; the existing comment already explains it.

### IN-04: Flood-suppression flag is a plain global; relies on single-threaded audio install ordering

**File:** `src/engine/client/library/clientAudio/src/win32/Audio.cpp:101,1312,1564`
**Issue:** `s_milesCodecRedistAvailable` is a non-atomic global set once in `Audio::install`
(:1312) and read later in `cacheSample` (:1564 via `DEBUG_WARNING`). Audio install runs on
the main thread before sample loading, and the read site is a `DEBUG_WARNING` (Debug-only,
no Release flood path), so there is no real race in practice. Noting the lifetime/threading
assumption for completeness; it is sound given the engine's single-threaded install.
**Fix:** None required given current call ordering.

### IN-05: `setBloomEnabled_impl` one-shot log flag is `_DEBUG`-only static — fine, but Release gives no signal

**File:** `src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp:322-332`
**Issue:** The no-op correctly prevents the prior FATAL and logs once in Debug. In Release
the function is a pure empty no-op (the `#ifdef _DEBUG` log is compiled out), so a Release
user who enables Bloom gets silent no-op with zero indication Bloom is unimplemented. This
matches the D-07 intent (redundant cfg key) and `setPointSize_impl` precedent, so it is
acceptable; noting only that there is no Release-visible trace.
**Fix:** None required; behavior is intentional per D-07.

---

_Reviewed: 2026-06-12_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
