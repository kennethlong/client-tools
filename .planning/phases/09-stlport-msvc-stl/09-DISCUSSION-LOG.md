# Phase 9: STLPort → MSVC STL - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-05-08 (original) + 2026-05-08 (replan after spike findings)
**Phase:** 09-stlport-msvc-stl
**Mode:** discuss (default), invoked twice — original `/gsd:discuss-phase 9`, then `/gsd:discuss-phase 9 --replan` after spikes 001a/001b/001c
**Areas discussed (replan only):** Fate of 13 landed commits, Wave structure for big-bang, StlCompat.h lifecycle, Build-broken interval & boot-gate cadence

---

## Original 2026-05-08 Session (preserved verbatim from prior log)

### Context Carry-Forward

Pulled from prior phases without re-asking:

- v1: STLPort vc71/MSVC 2022 ABI compat shim (`stlport_vc143_compat.cpp`) and `/FORCE:MULTIPLE` linker flag. Both targeted for removal in Phase 9.
- Phase 7 CLEAN-04: XPCOM removed → `/Zc:wchar_t-` constraint lifted (was the original blocker for STL-03).
- Phase 8 deferral: 15 MFC tools unbuildable while STLPort is on the include path. Phase 9 unblocks them as a side-effect.
- Memory record: STLPort `basic_filebuf::_M_setup_codecvt` vtable crash and `mbstate_t` ABI mismatch — both eliminated by this phase.

User explicitly requested **diligence and additional verifications** before discussion started, motivated by another AI's warning about "invisible bugs" from STL swaps.

### Area 1 — Migration Cadence

**Question:** Per-library, hybrid 2-tier, or big-bang whole-tree?

| Option | Description | Selected |
|--------|-------------|----------|
| Hybrid | shared engine libs as Wave 1, then engine/client + game libs as Wave 2 | ✓ |
| Per-library | one library at a time | |
| Big-bang | whole-tree in one shot | |

**Outcome → CONTEXT.md decision D-01 (later RETIRED by spike findings):** Three-wave structure. Wave 1 = engine/shared/library STLPort removal + boot. Wave 2 = engine/client + game STLPort removal + MFC canary builds + boot. Wave 3 = wchar_t flip + final verification.

### Area 2 — hash_map Sweep Approach

**Question:** Direct rename, compat header alias, or stdext::hash_map?

| Option | Description | Selected |
|--------|-------------|----------|
| Compat header | aliases `std::hash_map` to `std::unordered_map` | ✓ |
| Direct rename per file | full hasher/allocator updates per call-site | |
| `stdext::hash_map` | MSVC deprecated extension | |

**Outcome → CONTEXT.md decision D-02:** Author `sharedFoundation/StlCompat.h`. Sweep is a mechanical `#include` change. After waves boot clean, a separate cleanup commit migrates each call-site to standard names and deletes the compat header.

### Area 3 — wchar_t Flip Timing (STL-03)

**Question:** Same wave as STLPort removal, or separate wave after?

| Option | Description | Selected |
|--------|-------------|----------|
| Separate wave AFTER STLPort gone | bisect cleanly between STL ABI shift and wchar_t bit-width shift | ✓ |
| Same wave as STLPort removal | one fewer wave but ambiguous bisect surface | |

**Outcome → CONTEXT.md decision D-03:** STL-03 lives in its own dedicated wave (originally Wave 3, renumbered Wave 4 after replan).

### Area 4 — Verification Gate Set

**Question:** Which gate categories to bake in? (multiSelect)

| Option | Description | Selected |
|--------|-------------|----------|
| Code/symbol verification | grep BEFORE-STL.txt at each gate | ✓ |
| Canary build verification | MFC tools (Turf, Viewer) compile clean | ✓ |
| ABI/serialization verification | hash determinism + round-trip persist | ✓ |
| Runtime/perf verification | frame time + leak count | ✓ |

**Outcome → CONTEXT.md decisions D-04 + D-05:** All four verification categories enabled. Gates baked into both per-wave and final positions. MFC canaries (Turf + Viewer) re-enabled.

---

## Replan 2026-05-08 Session

Triggered by `/gsd:discuss-phase 9 --replan` after the 09-01 execution attempt halted mid-wave (architectural finding documented in `.continue-here.md`) and three spikes ran:

- 001a (per-target include scope) — INVALIDATED ✗
- 001b (coexistence guards) — INVALIDATED ✗ (and dangerous: silent miscompile)
- 001c (big-bang audit) — VALIDATED ✓

The combined spike finding is that STLPort's `#define std _STL` is undefeatable per-TU. Big-bang is the only viable architecture. The original D-01 (hybrid 3-wave) is retired.

### Area 1 — Fate of the 13 Landed Commits (df4bd21d3..831304d35)

**Question:** What do you want to do with the 13 landed Wave-1 commits?

| Option | Description | Selected |
|--------|-------------|----------|
| Keep them — replan starts at HEAD | Treat the 13 commits as Wave-1 done. Replan begins at Wave-2 client-source sweep. Preserves the per-group bisect granularity and the 8 documented portability fixes. Matches 001c's recommendation. | ✓ |
| Revert to 35b872357 and replan from scratch | `git reset --hard 35b872357`. Replan rewrites Wave-1 too. Single clean PR shape with new wave numbering. Loses per-group bisect history. | |
| Squash the 13 into one commit, then continue | Rewrite into one "Phase 9 Wave 1: engine/shared STLPort sweep" commit, then start Wave-2. Cleaner history but loses group-by-group bisect granularity. | |

**User's choice:** Keep them — replan starts at HEAD
**Outcome → CONTEXT.md decision R-01:** All 13 commits preserved. Wave 1 is "DONE" in the new wave structure. Replan begins at Wave 2.

### Area 2 — Wave Structure for Big-Bang

**Question:** How should the wave structure handle the idiom-portability fixes (Wave 5 territory)?

| Option | Description | Selected |
|--------|-------------|----------|
| 5 distinct waves — idiom fixes as their own wave | Wave 5 carved out for 5–15 fix-as-you-go portability edits. Each fix lands as a single atomic commit. Matches 09-01's `831304d35` pattern. Bisect-friendly: "did this break in the sweep wave or the fix wave?" | ✓ |
| 4 waves — fold idiom fixes into surfacing wave | Idiom fixes are inline patches within Wave 2 or Wave 4 commits, not their own wave. Fewer wave boundaries; less clean bisect for "what fixed this idiom." | |
| 5 waves but unicode last | Push unicode flip to Wave 5 (after build is fully green on STL). Idiom fixes fold into surfacing wave. Clean "unicode is purely additive" story but inverts the unicode-isolation rationale of D-03. | |

**User's choice:** 5 distinct waves — idiom fixes as their own wave
**Outcome → CONTEXT.md decision R-02:** 5-wave structure: (1) DONE — Wave 1 already on main, (2) client source sweep, (3) CMake link cleanup + MFC canaries, (4) unicode flip, (5) idiom fixes.

### Area 3 — StlCompat.h Lifecycle

**Question:** How should StlCompat.h be lifecycled?

| Option | Description | Selected |
|--------|-------------|----------|
| Keep as transitional alias, drop in cleanup commit | StlCompat.h continues to alias `std::hash_map`. Wave 2 sweep is 1-line per file. After Wave 5 boots green, dedicated cleanup commit migrates call-sites and deletes the header. Best bisect granularity. Matches what's already authored in `df4bd21d3`. | ✓ |
| Fold call-site migration into Wave 2 sweep | Every Wave-2 file edit changes BOTH the `#include` AND every `std::hash_map<...>` reference in that file. StlCompat.h still gets authored but is deleted at end of Wave 2. Coarser bisect. | |
| Skip StlCompat.h entirely — direct migration only | Revert `df4bd21d3`, do direct `std::hash_map → std::unordered_map` per call-site with no alias bridge. Effectively requires reverting 13 commits — declined in Area 1. | |

**User's choice:** Keep as transitional alias, drop in cleanup commit
**Outcome → CONTEXT.md decision R-03:** D-02 unchanged. StlCompat.h is the transitional alias; cleanup commit fires after Wave 5.

### Area 4 — Build-Broken Interval & Boot-Gate Cadence

**Question:** What commit/boot cadence should Waves 2–5 use?

| Option | Description | Selected |
|--------|-------------|----------|
| Per-group atomic + boot at wave end | Each library / CMake target / portability fix is its own commit. Tree may not compile mid-wave. Boot test fires once at end of each wave (5 boot tests total). Best bisect granularity. Consistent with 13 already-landed commits. | ✓ |
| Per-wave atomic — each wave is one commit | Squash each wave into a single commit. 5 commits total for the replan. Cleaner main-branch history but loses per-library bisect granularity. Inconsistent with the 13 already-landed per-group commits. | |
| Compile-green-every-commit | Order commits topologically so the tree always compiles. Highest verification cost (~40+ build+boot cycles). Likely impossible because std-namespace hijack is all-or-nothing within a TU's `#include` graph. | |

**User's choice:** Per-group atomic + boot at wave end
**Outcome → CONTEXT.md decision R-04:** Per-group atomic commits. Tree may not compile intra-wave. 5 boot tests total (one per wave end). Per-commit boot tests explicitly NOT required.

---

## Claude's Discretion

- **Wave 2 ordering within the ~19 client-tier files** — planner decides per-target sequencing (engine/client vs game/client vs ui first). User indicated no preference; mechanical sweep with consistent commit naming (e.g., `feat(09-02): clientGame — hash_map → StlCompat.h (group 3.1)`) is fine.
- **Wave 3 commit ordering** — whether to remove tier-parent CMake STLPort blocks before per-target link refs, and where the FindSTLPort.cmake / stlport453/ deletion lands within the wave. User indicated planner discretion as long as the wave-end boot gate fires after the full subtree delete.
- **Wave 5 trigger** — whether Wave 5 starts as soon as the Wave 4 boot succeeds, or whether the cleanup commit (call-site migration + StlCompat.h delete) gates between Wave 5 idiom fixes and the cleanup. CONTEXT.md positions cleanup AFTER Wave 5 for cleanest bisect; planner may revisit if Wave 5 surfaces idioms that interact with the call-site migration.
- **Round-trip persist target (D-06)** — researcher to pick a single concrete data class (`NetworkIdManager` recommended). User indicated no preference between the candidates.

## Deferred Ideas

(Carried forward from original 2026-05-08 + new from replan.)

- **Boost subset upgrade to a newer version** — D-07 keeps existing 4-header subset.
- **`/WX` warnings-as-errors re-enablement** — v3+ track.
- **Server-tier STLPort removal (33 sites)** — out of scope per CLAUDE.md "server is read-only contract." Future client-port milestone if needed.
- **C++20 / C++23 upgrade** — out of scope for Phase 9.
- **Re-enable remaining 13 MFC tools** — Phase 12.x follow-up after Wave 3 canary success.
- **Re-enable Qt-batch tools (Phase 12.1)** — independent follow-up phase.
- **STLPort directory removal as separate commit within Wave 3** — bisectable subtree delete.
- **(NEW from replan) Re-runnable spike audits** — `bash .planning/spikes/001c-big-bang-audit/audit.sh` and `list-files.sh` give re-runnable inventories. If HEAD moves between replan and execution, planner re-runs them rather than trusting cached counts.

---

## 2026-05-10 Session — Replan-3 (Option D adoption)

**Mode:** discuss (default), invoked as `/gsd:discuss-phase 9` after Probe E/G (2026-05-09) empirically invalidated D-08 (v145 + 2016 SWGSource DLL runtime contract) and the four-corner test (also 2026-05-09) empirically validated Option D (Koogie tree as v2 base + whitengold compat-guard port).

### Pre-discussion state

- Replan-2 D-08..D-12 binding decisions REVOKED. Boot gate against v145 EXE + 2016 DLLs FATALs at graphics init.
- Koogie's tree (`MSVC-CPP20-Upgrade` branch) builds clean under VS 2026/v145, boots to char-select, FATALs at world-load on character-loadout IFF chunk-size mismatch.
- Whitengold v1 has the `exitChunk(true)` graceful guard at `CuiCharacterLoadoutManager.cpp:162` plus 3 other SWGSource compat guards.
- v2 repo at `D:/Code/swg-client-v2/` on `koogie-msvc-cpp20-base` already has merge anchor `479d35df3` (--no-ff merge of `koogie/MSVC-CPP20-Upgrade`).
- 4 areas presented to the user; selected "Update it (replan-3)" path.

### Area 1 — Plan granularity

**Question:** How granular should Phase 9 replan-3 plans be?

| Option | Description | Selected |
|--------|-------------|----------|
| Two plans | 09-01 merge anchor + char-select gate; 09-02 compat-guard port + Tatooine + closeout | ✓ |
| Single plan | One 09-01 covering the full arc with two boot gates inside | |
| Per-guard plans | 09-01 merge, 09-02 IFF, 09-03 POB, 09-04 ContrailData+Nebula, 09-05 Tatooine+closeout | |

**Outcome → CONTEXT.md D-17.** Two plans: 09-01 anchors the merged build at char-select (mirrors Koogie-standalone empirical state); 09-02 forward-ports compat guards, fires Tatooine gate, writes closeout.

### Area 2 — `.planning/` location plan

**Question:** Where does `.planning/` live now that code work moves to swg-client-v2/?

| Option | Description | Selected |
|--------|-------------|----------|
| After Phase 9 closes | Keep `.planning/` in whitengold through 09-02 closeout; then copy top-level files into v2 | ✓ |
| After 09-01 merge anchor | Move mid-flight after 09-01 passes; 09-02 runs from v2 | |
| Move now | Step 0 of replan-3 is the copy; both 09-01 and 09-02 originate from v2 | |
| Mirror both | Maintain `.planning/` in both repos during Phase 9 | |

**Outcome → CONTEXT.md D-16.** Stay in whitengold through closeout. Copy `PROJECT.md`, `ROADMAP.md`, `REQUIREMENTS.md`, `STATE.md` (only those four top-level files) into `swg-client-v2/.planning/` as the final step of 09-02. Per-phase dirs `phases/01-*..09-*` stay in whitengold as research history. Phase 10 originates cold in v2.

### Area 3 — Compat-guard port scope

**Question:** Which whitengold v1 SWGSource compat guards get forward-ported in Phase 9?

| Option | Description | Selected |
|--------|-------------|----------|
| Bisect-first | IFF guard mandatory; ContrailData / Nebula / POB on-demand only if Tatooine surfaces FATAL | ✓ |
| Exhaustive | Port IFF + POB + ContrailData + Nebula in one sweep before Tatooine test | |
| Surgical | IFF guard only; defer POB/ContrailData/Nebula to a future phase regardless | |

**Outcome → CONTEXT.md D-18.** IFF guard at `CuiCharacterLoadoutManager.cpp:162` is the only mandatory port. The other three are candidate-set entries (whitengold commits `89bce3a43`, `fcce67cf1`, `fac49a4c3`) ported only if Tatooine zone-in surfaces them. Rationale: Koogie's tree may have absorbed equivalent fixes from SWG-Source/master independently; pre-porting risks duplicate-fix conflicts. Bisect-first lets the test pick the minimum required set.

### Area 4 — Upstream PR cadence

**Question:** When and how do compat-guard PRs land against SWG-Source/master?

| Option | Description | Selected |
|--------|-------------|----------|
| Land locally, batch PR after Tatooine PASS | Each guard as thematic commit on v2; PRs against SWG-Source/master open per-guard with empirical evidence | ✓ |
| Per-guard PRs as authored | Open each PR immediately after the guard commit, before Tatooine gate | |
| Defer upstream PRs entirely | Land guards on v2 only; revisit upstream contribution in a later phase | |

**Outcome → CONTEXT.md D-19.** Land each guard as a thematic commit on `swg-client-v2/koogie-msvc-cpp20-base`. Open per-guard PRs ONLY AFTER the full guard set passes Tatooine zone-in together end-to-end. Each PR references its Tatooine-PASS evidence inline. 30-day outreach protocol (per memory `project_swg_source_upstreaming.md`) applies post-PR-open, which is post-Phase-9 followthrough.

### Sequencing Follow-up — `.planning/` move timing

**Question:** When in Phase 9 does the `.planning/` move execute?

| Option | Description | Selected |
|--------|-------------|----------|
| After Phase 9 closes | Last step of 09-02 SUMMARY (copy top-level files into v2 after Tatooine PASS) | ✓ |
| After 09-01 merge anchor | Move mid-flight; 09-02 runs from v2 | |
| Move now (Step 0) | Both 09-01 and 09-02 author from v2 from the start | |
| Mirror both during Phase 9 | Maintain in both repos through Phase 9 | |

**Outcome → CONTEXT.md D-16 (sequencing fold-in).** Move executes as final task of 09-02 closeout, after Tatooine PASS recorded and STATE.md updated. No mid-flight migration risk.

## Claude's Discretion (replan-3)

- **09-01 staging procedure** — exact runtime DLL set to copy into `swg-client-v2/stage/` matches the 2026-05-09 Koogie-standalone test setup recorded in `.continue-here-replan3.md`. Planner enumerates the specific files (`binkw32.dll`, `Mss32.dll`, etc.) per memory `project_vm_server_setup.md` plus Koogie-built artifacts.
- **09-02 commit shape** — each compat-guard commit on v2 should reference whitengold's original commit hash in the body (e.g., `Port-forward of whitengold dd78832c4 fix(06): tolerate extra ITEM chunk fields in character loadout IFF.`) so the v2 PR description can cite the empirical provenance to SWG-Source maintainers.
- **STATE.md milestone math** — under D-14, STL-01..04 close mechanically on 09-01 (merge anchor includes them). STL-05 closes on 09-02 Tatooine PASS. STATE.md `completed_phases` count increments by 1 at 09-02 closeout, not at 09-01 char-select gate.
- **Replan-2 artifacts under `archive/replan-2/` and `baseline/`** — stay in place as research history per `.continue-here-replan3.md` recommendation ("KEEP them — per-commit shape is good research history"). Not deleted, not moved.

## Deferred Ideas (replan-3)

- **Koogie's `x64bit-Upgrade` branch** — separate branch from `MSVC-CPP20-Upgrade`; a future v3 or v4 milestone could merge it after v2 stabilises (per memory `project_v4_linux_vulkan.md` and `project_v2_scope.md`).
- **CMake re-authoring on top of v2** — whitengold's CMake graph is research history under Option D. If a future milestone wants to ship CMake authoring on the v2 tree, that becomes its own phase. Not Phase 9 scope.
- **Memory re-keying** — auto-memory is currently keyed to `D--Code-swg-client` (whitengold). Whether to mirror or re-key memory to `D--Code-swg-client-v2` is a post-Phase-9 concern; for now, both directories share the same memory pool via the whitengold key.
- **Server-tier STLPort removal (33 sites)** — still out of scope; Koogie's tree handles client-side only, mirrors whitengold's "server is read-only contract."
