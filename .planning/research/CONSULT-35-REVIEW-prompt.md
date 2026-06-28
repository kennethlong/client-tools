# Cross-AI Plan Review Request — Phase 35: Miles 9.3b Audio Port

You are an independent reviewer of implementation plans for an established C++ game-engine
codebase (Star Wars Galaxies client, "swg-client-v2"). This phase is part of a v3.0 milestone
porting the 32-bit client to x64. Provide structured, skeptical, technically-specific feedback.
Treat the facts below as given (they were verified against the live SDK + source this session);
focus your energy on plan quality, gaps, ordering, and risk — not on re-deriving the API delta.

## Project Context (brief)

- Pure MSBuild build (`src/build/win32/swg.sln`), VS18 / toolset v145, C++20. No CMake.
- `clientAudio` is a static lib; `SwgClient.vcxproj` is the final link.
- `/FORCE` link options (ForceFileOutput, /SAFESEH:NO) are active on the SwgClient link — they
  DOWNGRADE unresolved-external (LNK2019/2001) to warnings and still emit a binary. So "exit 0"
  is NOT proof of a clean link; the gate is grepping the build log for `unresolved external
  symbol` == 0. `LNK1181 cannot open input file` is a hard error (not masked).
- `*.lib` is gitignored (bare `*.lib`); `stage/*` is gitignored. Vendored libs need `git add -f`.
- Redist staging uses a sentinel-guarded PostBuild xcopy from in-repo-tracked bytes (Phase-24 model).
- Win32 Debug exe reads `client_d.cfg`; UTF-8 BOM on a `.cfg` crashes the Release client at boot.
- Boot gate: the client must remain bootable to character select.

## The change, in one line

Swap audio middleware from x86-only Miles 7.2e to the locally-cloned Miles 9.3b SDK on BOTH
platforms, undo the Phase-33 x64 Miles-disable scaffolding, and stage the 9.3b provider set so
in-game audio works on x64 (and is re-verified on Win32).

## Locked Decisions (from CONTEXT.md — do not challenge these, review the plans against them)

- **D-01:** Upgrade BOTH platforms to 9.3b (not just x64); retire 7.2e + unversioned `miles/` as the
  active dependency. The 7.2e→9.3b edits move into the SHARED (non-`_M_X64`) code path; Win32 must be
  re-verified for audio.
- **D-02:** Vendor both `mss32.lib` (Win32) and `mss64.lib` (x64); the same 9.3b header compiles for both.
- **D-03:** Delete the three Phase-33 x64 Miles disables (the `#if _M_X64` AIL_* macro-stub block,
  the `install()` x64 early-return that sets `s_disableMiles=true`, and the `SoundObject3d` listener
  gate). No x64-specific disable survives.
- **D-04 (KEEP):** Leave the legacy `[ClientAudio] disableMiles` cfg short-circuit intact — it is
  retail config, NOT Phase-33 scaffolding. Default false → Miles on.
- **D-05:** Full 7.2e-parity provider set staged per platform (mp3/ogg decoders PLUS the EAX reverb +
  DS3D 3D filters + bink ASI), gathered from three SDK redist trees.
- **D-06:** Manual UAT, all four dimensions on x64 (+ Win32 smoke): music, 2D UI SFX, 3D positional,
  reverb/room-type. Pass bar: all four audible, NO warning-flood lag. Silent-or-flood = fail.

## Requirements addressed

- **AUDIO-01:** Client links vendored Miles 9.3b SDK; clientAudio call sites ported 7.2e→9.x (incl.
  the `AIL_room_type` signature edit); SDK vendored into `src/external/3rd/library/miles-9.3b/`.
- **AUDIO-02:** x64 Miles redist + provider set staged next to the x64 exe; in-game audio works
  (music, 2D UI, 3D positional, reverb/room-type) without half-dead-audio / warning-flood.

## Key verified facts (treat as given)

- The only breaking API change the client touches: the room_type family gained an `S32 bus_index` arg.
  GETTER `AIL_room_type(dig)` → `AIL_room_type(dig, 0)` (bus_index TRAILING, 1 site).
  SETTER `AIL_set_room_type(dig, room_type)` → `AIL_set_room_type(dig, 0, room_type)` (bus_index
  MIDDLE, 26 sites). `0` = master mixer bus (assumption A1, deferred to UAT #4).
- 9.3b ships NO `.m3d` 3D-provider modules (7.2e did). The codec-presence probe hardcodes the 7.2e
  `.m3d` filenames, so on a 9.3b stage it always reports "redist missing" and silently suppresses
  audio. The probe filename list must be repointed to the 9.3b `.flt`/`.asi`/`.dll` set (platform-aware).
- File-callback typedefs are byte-identical (UINTa already correct from Phase 33) — no callback work.
- `AIL_open_digital_driver` became a macro but is source-compatible at the 4-arg call site.

---

## Plans to Review (4 plans, 3 waves)

### Wave 1
- **35-01** (autonomous): Vendor the 9.3b SDK tree (mirror miles-7.2e: include/ + lib/win/ + redist/
  + redist64/), force-add both libs past the `*.lib` gitignore, repoint clientAudio.vcxproj include
  path miles/→miles-9.3b across 5 configs, then COMPILE-ONLY smoke the static lib on both platforms
  to isolate header-compat risk (A4) before any API edit. Vendors rrcore.h (mss.h includes it). No .m3d.
- **35-02** (autonomous): Source port to the two TUs that call Miles (Audio.cpp, SoundObject3d.cpp):
  port room_type (1 getter + 26 setters), repoint the codec probe off .m3d to the platform-aware
  9.3b set, and remove the three Phase-33 x64 disables — while KEEPING the legacy disableMiles cfg.
  The codec-probe `#if _M_X64` is the ONE intentional surviving _M_X64. Note: this plan does NOT
  link standalone; the link gate is in plan 03.

### Wave 2
- **35-03** (autonomous, depends 35-01+35-02): Repoint SwgClient.vcxproj Miles lib dir to
  miles-9.3b on both platforms, ADD mss64.lib to the x64 link (Phase-33 had dropped it), repoint the
  Win32 PostBuild redist source + change to a 9.3b-only sentinel, ADD an x64 PostBuild block staging
  redist64 → stage-x64/miles. Clear stale stages, build+link both platforms, gate on `unresolved
  external symbol` == 0 + LNK1181 == 0, and file-exists-check the staged providers.

### Wave 3
- **35-04** (NOT autonomous, depends 35-03, human checkpoint): Automated pre-flight boot + log census
  (no "redist missing", no warning-flood; confirm disableMiles false), then the blocking human D-06
  four-dimension UAT on x64 + a Win32 audio smoke; record result in 35-HUMAN-UAT.md.

(The full task bodies, acceptance criteria with exact grep counts, threat models, and verification
sections are in the PLAN files; the executor reads PATTERNS.md §1-6 for exact line ranges + tokens.)

## Review Instructions

For each plan and the phase as a whole, provide:

1. **Summary** — one-paragraph assessment of plan quality.
2. **Strengths** — what is well-designed (bullets).
3. **Concerns** — issues, gaps, risks, each tagged severity HIGH / MEDIUM / LOW. Be specific:
   cite the plan number and the exact mechanism. Examples of what to scrutinize:
   - Missing edge cases or error handling (e.g. the bus_index=0 assumption A1 — is UAT enough?)
   - Dependency-ordering issues across the 3 waves
   - The /FORCE-masking link gate — is "unresolved external symbol == 0" sufficient, or could a
     symbol resolve to the WRONG lib / a stale obj?
   - The sentinel-guarded staging — does the 9.3b-only sentinel actually repair a stale 7.2e stage?
     Are there race/ordering or stale-state holes the plan misses?
   - The grep-count acceptance criteria (e.g. setter == 26, _M_X64 == 2) — are they brittle? What
     if line drift or an extra legitimate `_M_X64` breaks the exact-count gate?
   - Editor vcxprojs also link mss32.lib from the old miles/ tree — does leaving them unrepointed
     create a latent break, even if editors are "pre-broken on Qt"?
   - Scope creep or over-engineering; security; performance.
   - Whether the plans actually achieve AUDIO-01 + AUDIO-02 and the boot gate.
4. **Suggestions** — specific, actionable improvements.
5. **Risk Assessment** — overall LOW / MEDIUM / HIGH with justification.

Output in markdown.
