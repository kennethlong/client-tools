---
phase: 14
reviewers: [codex, cursor]
reviewed_at: 2026-05-26T14:31:20Z
plans_reviewed: [14-01-PLAN.md, 14-02-PLAN.md, 14-03-PLAN.md]
note: claude skipped (self — review run from inside Claude Code, skipped for independence)
---

# Cross-AI Plan Review — Phase 14: Voice Chat (Vivox) Source Removal

Two independent external models reviewed the three plans against the live tree.
Both converged on the same two top concerns and each surfaced one additional HIGH
the other missed. The internal gsd-plan-checker passed (it verifies plan
structure + decision/requirement coverage); these external reviews add *live-tree
completeness* grounding the checker does not perform.

---

## Codex Review

**Summary**

The plans are directionally strong and mostly match the correct removal shape: this is treated as an atomic source-removal/link-dewire phase, not a feature change, and the `/FORCE` link-grep + dual-renderer boot gates are the right success criteria. However, I would not approve as-is. My read-only grep found at least two material completeness gaps: `VOICE_KICK`/`voice_kick` survives in `CuiMenuInfoTypes`, and several editor `.vcxproj` files appear to carry inline `vivoxSharedWrapper` refs, while 14-02 only cleans their `.rsp` files plus `SwgGodClient.vcxproj`. There is also an ordering problem inside 14-01: Task 1 strips `CuiPreferences` voice APIs while `SwgCuiOptVoice.cpp` still exists until Task 2, creating an unbuildable intermediate task state.

**Strengths**

- Correctly recognizes the closed symbol chain: callers -> `CuiVoiceChatManager` -> Vivox/VChat libs + voice network messages.
- Correctly rejects no-op stubs; grep-zero requires real deletion and call-site removal.
- Includes the important baseline misses from research: `SwgCuiHudWindowManager`, mediator types, actions, shared voice network messages, `SwgGodClient` inline refs, and `soePlatform/VChatAPI`.
- Keeps `soePlatform/libs` while removing `Base_vchat.lib` / `VChatAPI.lib` tokens, which avoids breaking non-voice SOE platform libs.
- Uses the right validation posture for this repo: Debug + Release link logs must grep zero `unresolved external symbol`, and boot must be confirmed under D3D9 and D3D11.

**Concerns**

- **HIGH:** `VOICE_KICK` / `voice_kick` is missed. Current refs include `CuiMenuInfoTypes.h:167` and `CuiMenuInfoTypes.cpp:309`. The plans remove `VOICE_INVITE` only, but `SwgCuiVoiceActiveSpeakers.cpp` uses `VOICE_KICK`, and deleting that page leaves the menu id orphaned in surviving code. If the goal is whole voice-subsystem removal, this should be deleted and added to grep gates.

- **HIGH:** 14-02 appears to miss inline editor `.vcxproj` Vivox refs beyond `SwgGodClient`. I found inline `vivoxSharedWrapper_release.lib` / `vivoxSharedWrapper\lib` refs in editor projects such as `AnimationEditor.vcxproj`, `ClientEffectEditor.vcxproj`, `LightningEditor.vcxproj`, `NpcEditor.vcxproj`, `ParticleEditor.vcxproj`, and `SwooshEditor.vcxproj`. Cleaning only editor `.rsp` files will not satisfy a repo-wide `rg -i vivox src/` gate.

- **MEDIUM:** 14-01 Task 1 is not buildable as a standalone task. It strips `CuiPreferences::getVoice*` APIs before Task 2 deletes `SwgCuiOptVoice.cpp`, which still calls those APIs. If executors validate or commit by task, the tree breaks between Task 1 and Task 2.

- **MEDIUM:** The grep gates are inconsistent. Some gates search `voiceKick` but not `VOICE_KICK` or `voice_kick`; some include `VChatAPI` only for selected files, while 14-03 repo-wide gate searches only `vivox`. Decide whether the phase gate is literal Vivox-only or whole voice/VChat residue cleanup, then make all gates reflect that.

- **MEDIUM:** `soePlatform/copy-libs.bat` still references `VChatAPI` paths outside the deleted `VChatAPI/` tree. This is probably non-build-load-bearing, but it conflicts with any "VChatAPI grep-zero" interpretation.

- **LOW:** Task 2 verifies only a few sentinel deleted files. Given the deletion list is large, this should verify every planned deleted file is absent.

- **LOW:** `stage/client_d.cfg` is modified for boot testing. The plan should say whether to restore the original `rasterMajor` after the D3D11/D3D9 checks or intentionally leave the last-tested value.

**Suggestions**

- Add explicit removal of `Cui::MenuInfoTypes::VOICE_KICK` and `MAKE_ID(VOICE_KICK, none, voice_kick, 0)`, and add `VOICE_KICK|voice_kick|toggleVoiceActiveSpeakers` to all relevant grep-zero gates.

- Extend 14-02 to purge inline Vivox refs from all eight editor `.vcxproj` files, not only `SwgGodClient.vcxproj`, or explicitly narrow criterion #1 to exclude editor project files. The cleaner path is to purge them.

- Collapse 14-01 Tasks 1-3 into one truly atomic edit/build unit, or reorder so no task boundary is expected to compile until all source deletion + project deletion + link dewire lands. At minimum, label Task 1/2 as non-buildable substeps inside one atomic task.

- Make final gates explicit and canonical:
  - `rg -i "vivox" src` == 0
  - `rg "CuiVoiceChat|SwgCuiVoice|VoiceChat|VOICE_INVITE|VOICE_KICK|voice_invite|voice_kick|voiceInvite|voiceKick|WS_VoiceFlyBar|WS_VoiceActiveSpeakers|toggleVoiceFlyBar|toggleVoiceActiveSpeakers|ms_voice|getVoice|setVoice|CommandParserVoice" src` == 0
  - optionally `rg -i "VChatAPI|Base_vchat|libsndfile" src/**/*.vcxproj src/**/*.rsp src/external/3rd/library/soePlatform` with documented allowed/removed paths.

- Add a pre-delete check in 14-03 for editor `.vcxproj` refs too, not just `SwgClient.vcxproj`.

**Risk Assessment**

Overall risk: **MEDIUM-HIGH** as written. The architectural approach is sound, and the plans understand the major link-resolution risks, but the missed `VOICE_KICK` residue and editor inline `.vcxproj` refs are likely to fail grep-zero, and the 14-01 task split creates an avoidable unbuildable intermediate state. With those fixes, the risk drops to **MEDIUM**, mostly due to the size of the live UI dewire and reliance on manual dual-renderer boot validation.

---

## Cursor Review

# Phase 14 Plan Review: Vivox Voice-Chat Source Removal

## 1. Summary

The three-plan wave structure (14-01 atomic source/link surgery, 14-02 residue purge, 14-03 vendored-tree delete + milestone gate) is **directionally excellent** and closely mirrors the proven Phase 13 pattern. Research quality is high: symbol-resolution analysis, `/FORCE` link-grep gates, inline-`.vcxproj`-over-`.rsp` emphasis, and D-10 `VChatAPI/` scope correction are all sound.

However, verified against the live tree, the plans have **three execution-critical gaps** that would block success criteria or produce a mid-plan broken tree: (1) **wrong canonical paths** for several header edits (public re-include shims vs `src/shared/core/` originals), (2) **14-02 misses inline `vivox` refs in seven editor `.vcxproj` files** (only `.rsp` + `SwgGodClient.vcxproj` are covered), and (3) **14-01 Task 1→2 ordering violates D-02a** by stripping `CuiPreferences` voice APIs while voice `.cpp` files still compile. Fix those before execution and this phase is **MEDIUM risk** and achievable; as written it is **HIGH risk of a false-pass or compile break**.

## 2. Strengths

- **Correct mental model for a removal phase:** Treats Vivox as a closed symbol chain (`callers → CuiVoiceChatManager → SwgVivox/libs → voicechat messages`) and mandates atomic deletion, not stubs — appropriate for DECRUFT-05 criterion #2 grep-zero.
- **Phase 12/13 lessons internalized:** Inline `.vcxproj` as authoritative link wiring, vestigial `.rsp` as cosmetic, `/FORCE` false-pass, `soePlatform\libs` directory preservation, and dual-renderer boot gate are all explicitly wired in.
- **Research baseline miss recovery:** `SwgCuiHudWindowManager`, `SwgCuiMediatorTypes`, `SwgCuiActions`, `sharedNetworkMessages/voicechat/`, and `SwgGodClient` inline refs are captured — the CONTEXT baseline alone would have failed.
- **D-10 resolution is correct:** Deleting `soePlatform/VChatAPI/` source (~115 `vivox` literals) while keeping `soePlatform/libs/` prebuilts and `ChatAPI2/` matches criterion #1 without breaking community chat.
- **Wave sequencing is right:** Vendored-tree delete in Wave 2 after include-path purge prevents Pitfall 3; `depends_on: [14-01, 14-02]` on 14-03 is correct.
- **Parallel 14-01 / 14-02:** Zero file overlap — safe to run concurrently.
- **Validation architecture:** Per-task grep gates + Debug/Release link-grep + human boot checkpoint maps cleanly to the five roadmap success criteria.
- **Over-reach discipline on live middleware:** Miles/Bink untouched; `libsndfile-1.lib` removal backed by zero non-link-line consumers in source.

## 3. Concerns

| Severity | Concern |
|----------|---------|
| **HIGH** | **Wrong header paths in 14-01 Task 1.** Several edits target `include/public/` shim headers that only `#include "../../src/shared/..."`. The real symbols live in canonical files: `src/.../src/shared/core/CuiPreferences.h` (voice getters at :538–554), `CuiMenuInfoTypes.h` (`VOICE_INVITE` at :165), `SwgCuiMediatorTypes.h` (`WS_VoiceFlyBar` at :104–105), `SwgCuiActions.h` (`toggleVoiceFlyBar` at :69–70). Editing shims alone leaves criterion #2/#3 failing while Task 1 grep verify falsely passes. |
| **HIGH** | **14-02 incomplete for criterion #1.** Seven editor projects carry **inline** `vivoxSharedWrapper_release.lib` + `vivoxSharedWrapper\lib` in `.vcxproj` (AnimationEditor, ClientEffectEditor, LightningEditor, NpcEditor, ParticleEditor, SwooshEditor, Viewer — 6 configs each). Plan 14-02 purges their `.rsp` files and `SwgGodClient.vcxproj` only. Repo-wide `rg -i "vivox" src/` in 14-03 would still hit ~42 lines. |
| **HIGH** | **14-01 Task 1→2 breaks D-02a.** Task 1 strips `CuiPreferences` voice getters/setters (`getVoiceChatEnabled`, etc.) while Task 2 still leaves `CuiVoiceChatManager.cpp`, `SwgCuiOptVoice.cpp`, and other voice sources compiling — those files call the removed APIs (confirmed at `CuiVoiceChatManager.cpp:305+`). Intermediate state is unbuildable. Task 1 verify is grep-only on edited caller files, so it won't catch this. |
| **MEDIUM** | **Task-level ordering within 14-01 contradicts "atomic coherent change."** Objective says one commit / one coherent unit, but Task 1 (dewire), Task 2 (delete), Task 3 (unlink + build) creates three checkpoints where only Task 3 builds. An executor pausing after Task 1 gets a broken tree with no compile gate. |
| **MEDIUM** | **14-01 Task 1 verify scope is too narrow.** Automated verify greps `include/public/...` paths for several headers but omits the canonical `src/shared/core/` files where symbols actually live — same root cause as the path bug, but also means verify won't catch incomplete edits even if paths were fixed partially. |
| **MEDIUM** | **Optimized config not gated.** `SwgClient.vcxproj` has Debug / Optimized / Release; Task 3 builds Debug + Release only. If Optimized is used in practice, vivox token removal there is planned but unverified. |
| **LOW** | **`soePlatform/copy-libs.bat` still references deleted `VChatAPI/` paths** after 14-03. No `vivox` literal — won't fail criterion #1 — but stale maint script. |
| **LOW** | **Prebuilt `VChatAPI.lib` / `Base_vchat.lib` remain on disk** in `soePlatform/libs/`. Criterion #1 targets `vivox`/`Vivox` in source/include, not `.lib` filenames; acceptable, but document as intentional residual. |
| **LOW** | **RESEARCH.md Open Questions §Q1 still contains stale "leave soePlatform alone" text** contradicted by D-10. Plans corrected it; research doc could mislead future readers. |

## 4. Suggestions

- **Fix canonical paths in 14-01 Task 1** — replace all `include/public/...` edit targets with their `src/shared/core/` (or `src/shared/page/`) counterparts for: `CuiPreferences.h`, `CuiMenuInfoTypes.h`, `SwgCuiMediatorTypes.h`, `SwgCuiActions.h`. Keep public shims untouched (they auto-follow).
- **Extend 14-02 Task 2** to purge inline vivox tokens from all seven editor `.vcxproj` files (same token list as SwgGodClient: deps + lib dirs, all configs). Update verify to `rg -i "vivox" src/**/build/win32/*.vcxproj` for editor projects, not just `.rsp`.
- **Collapse 14-01 into one atomic task** (or reorder): dewire callers + strip prefs + delete all voice sources + remove ClCompile entries + unlink SwgClient/engine includes **before any intermediate verify**, then build-gate once. If keeping three tasks, move CuiPreferences strip from Task 1 to Task 2 (same commit, same MSBuild invocation).
- **Strengthen Task 1 verify** to grep canonical source paths under `src/shared/core/` and `src/shared/page/`, not public shims.
- **Add a Wave 1 merge gate** before 14-03: single command `rg -i "vivox" src/` == 0 covering `.vcxproj`, `.rsp`, and source — catches 14-02 editor `.vcxproj` gap early.
- **Optional:** build Optimized config once in Task 3 for parity with the three `.vcxproj` config blocks being edited.
- **Optional:** note in 14-03 that `stage/client_d.cfg` comments mentioning "Vivox" are out of criterion #1 scope (`src/` only) — or clean them for hygiene.

## 5. Risk Assessment

**Overall: MEDIUM-HIGH (as written) → MEDIUM (after path/order/14-02 fixes)**

**Justification:**

- **Link-resolution risk is LOW** once executed atomically: research correctly identifies `CuiVoiceChatManager.cpp` as the sole `SwgVivox` consumer; removing source + callers + link tokens together should yield 0 unresolved externals, and the `/FORCE` grep gate is the right control.
- **Boot/runtime risk is LOW:** Voice is off-by-default, no Vivox backend on SWGSource VM; D-06a orphaned-TRE no-op reasoning is sound; dual-renderer checkpoint is appropriate.
- **Over-reach risk is LOW:** `soePlatform/libs` preservation, ChatAPI2 untouched, libsndfile scoped correctly — no evidence of live non-voice consumers being targeted.
- **Plan execution risk is HIGH today** due to wrong file paths, 14-02 `.vcxproj` gap, and Task 1/2 ordering — these don't reflect misunderstanding of the subsystem, they're fixable plan defects that would cause compile failure or criterion #1 failure at the 14-03 gate.

**Success criteria forecast (as written vs fixed):**

| Criterion | As written | After suggested fixes |
|-----------|------------|----------------------|
| #1 Zero `vivox`/`Vivox` | **FAIL** (editor `.vcxproj` holdouts) | PASS |
| #2 Symbol deletion | **FAIL** (wrong header paths) | PASS |
| #3 Prefs strip | **FAIL** (wrong `CuiPreferences.h`) | PASS |
| #4 Debug+Release build | **PASS** if 14-01 completed atomically in one session | PASS |
| #5 Dual-renderer boot | **PASS** (checkpoint well-specified) | PASS |

The research and wave architecture are strong enough to execute confidently once the path enumeration, editor `.vcxproj` purge, and intra-14-01 atomicity issues are corrected.

---

## Consensus Summary

Both reviewers rate the **architecture and research as strong** and the **plans as not-yet-executable as written** (codex: MEDIUM-HIGH; cursor: MEDIUM-HIGH → MEDIUM after fixes). The two top concerns are **independently corroborated** by both models.

### Agreed Strengths (2+ reviewers)
- Correct removal mental model — closed symbol chain (`callers → CuiVoiceChatManager → Vivox/VChat libs + voicechat messages`), atomic deletion not stubs.
- Phase 12/13 lessons internalized — inline `.vcxproj` authoritative, `/FORCE` link-grep gate, `soePlatform/libs` preserved, dual-renderer boot gate.
- Research baseline-miss recovery captured (SwgCuiHudWindowManager, mediator types, actions, voicechat network messages, SwgGodClient inline).
- D-10 resolution correct — delete `soePlatform/VChatAPI/` source, keep `libs/` prebuilts + `ChatAPI2/`.
- Right validation posture — Debug+Release link-grep for `unresolved external symbol`, manual dual-renderer boot.
- Over-reach discipline — Miles/Bink untouched, `libsndfile-1.lib` correctly scoped, no live non-voice consumer targeted.

### Agreed Concerns (2+ reviewers — HIGHEST PRIORITY)
1. **[HIGH — both] 14-02 misses INLINE editor `.vcxproj` vivox refs.** 6–7 editor projects (AnimationEditor, ClientEffectEditor, LightningEditor, NpcEditor, ParticleEditor, SwooshEditor; cursor also counts Viewer) carry inline `vivoxSharedWrapper_release.lib` + `vivoxSharedWrapper\lib`. 14-02 cleans only their `.rsp` + `SwgGodClient.vcxproj`. The repo-wide `rg -i "vivox" src/` gate in 14-03 still hits ~42 lines → **criterion #1 FAILS**.
2. **[HIGH/MED — both] 14-01 Task 1→2 ordering produces an unbuildable intermediate (D-02a violation).** Task 1 strips `CuiPreferences::getVoice*` while Task 2 still leaves voice `.cpp` (e.g. `CuiVoiceChatManager.cpp`, `SwgCuiOptVoice.cpp`) compiling and calling those removed APIs. Task 1's grep-only verify won't catch it. Fix: collapse 14-01 into one atomic build-gated unit, or move the prefs strip into the same task as the source deletion.
3. **[MED — both] Grep-gate / verify-scope inconsistency.** Gates search `voiceKick` but not `VOICE_KICK`/`voice_kick`; verify greps narrow (public shims / sentinel files). Canonicalize the gate symbol set across all three plans.
4. **[LOW — both] `soePlatform/copy-libs.bat`** still references deleted `VChatAPI/` paths (stale maint script; no `vivox` literal so won't fail criterion #1).
5. **[LOW — both] `stage/client_d.cfg` rasterMajor** restore-vs-leave after the boot gate is unspecified.

### Divergent Views (single reviewer — but concrete and verifiable, treat as high-value)
- **[HIGH — codex only] `VOICE_KICK` / `voice_kick` residue missed.** `CuiMenuInfoTypes.h:167` + `CuiMenuInfoTypes.cpp:309`; `SwgCuiVoiceActiveSpeakers.cpp` uses `VOICE_KICK`. Plans remove only `VOICE_INVITE` → orphaned menu id + grep-zero failure. **High-value, easily verified — should be folded in.**
- **[HIGH — cursor only] Wrong header edit paths.** 14-01 Task 1 targets `include/public/` shim headers that only re-include the canonical `src/shared/core/` (or `src/shared/page/`) originals; editing shims leaves criteria #2/#3 failing while the grep verify *falsely passes*. Canonical lines named: `CuiPreferences.h:538–554`, `CuiMenuInfoTypes.h:165`, `SwgCuiMediatorTypes.h:104–105`, `SwgCuiActions.h:69–70`. **High-value — verify against the tree.**
- **[MED — cursor only] Optimized config not gated** — only Debug+Release built; SwgClient.vcxproj has a third (Optimized) config.
- **[LOW — cursor only] RESEARCH.md Q1 stale text** — already corrected this session (Q1 marked RESOLVED by D-10).

### Recommended Action
Both reviewers' top-two concerns are corroborated and, combined with the two single-reviewer HIGHs, all point at concrete, verifiable plan defects (not architectural disagreement). Recommend a `--reviews` replanning pass to: (a) extend 14-02 to purge inline vivox from all editor `.vcxproj`, (b) verify + fix canonical header edit paths in 14-01 and add `VOICE_KICK`/`voice_kick`, (c) re-order/collapse 14-01 so every task boundary is buildable, and (d) canonicalize the grep-gate symbol set across all three plans (incl. a Wave-1 merge gate before 14-03).
