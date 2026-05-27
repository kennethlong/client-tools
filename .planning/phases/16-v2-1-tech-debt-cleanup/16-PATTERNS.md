# Phase 16: v2.1 Tech-Debt Cleanup - Pattern Map

**Mapped:** 2026-05-26
**Files analyzed:** 3 modified (+ 4 verify-only, 0 new)
**Analogs found:** 3 / 3 (each modified file maps to an exact removal-precedent commit)

> **This is a DELETION phase — zero new files.** The conventional "map new file to
> analog" job does not apply. Instead each MODIFIED file maps to its closest
> **removal-precedent commit** from Phases 12–15, so the planner mirrors the exact
> removal discipline already proven in this repo: **atomic per-target commit →
> repo-wide `rg` for the removed token == 0 → SwgClient Debug+Release link-grep
> `unresolved external symbol` == 0.** All upstream scope (D-01..D-08) is locked and
> CONFIRMED in 16-RESEARCH.md; nothing here re-decides scope.

## File Classification

| Modified File | Removal Type | Compiles Into | In SwgClient Link Closure? | Closest Precedent | Match Quality |
|---------------|-------------|---------------|----------------------------|-------------------|---------------|
| `SwgGodClient.vcxproj:99` (Debug `<AdditionalDependencies>`) | build-config dead-token | (SwgGodClient.exe — **never built**, Qt MSB8066) | NO — out of `/t:SwgClient` closure | `4bc512b45` (14-02 vivox/VChat token purge, **same file**) | exact (same file + same construct) |
| `SwgCuiHudAction.cpp:1170-1189` (dead `finalUrl` block) | dead source block | `swgClientUserInterface.lib` | **YES** | `e5a76be9f` (13-02 delete dead `#if 0` capture source residue) | exact (dead-block deletion in UI lib) |
| `CuiPreferences.cpp:397-398 / :3460-3484` + `.h:553-557` (voice-volume API) | full dead API (statics + accessors + decls, 0 callers) | `clientUserInterface.lib` | **YES** | `78d7373ff` (13 remove orphaned AudioCapture file-statics, WR-01) | exact (orphaned-state-after-feature-delete) |

**Verify-only (no edit — D-04):** 4 editor vcxproj (`AnimationEditor`, `ClientEffectEditor`, `LightningEditor`, `ParticleEditor`) — already 0 `lcdui` hits (swept by `d1dcb8447` / 15-04). Precedent for the *verify-only* posture: `1d6b80242` (12-03 lcdui ProjectReference removal) was the original lcdui-in-editor-vcxproj fix; 15-04 finished the LIBPATH residue. This phase only re-asserts `lcdui-editor-LIBPATH == 0`.

## Link-Closure Confirmation (from RESEARCH.md, re-verified)

- `clientUserInterface.vcxproj` → `ConfigurationType=StaticLibrary`, output `$(ProjectName).lib` = **`clientUserInterface.lib`**. Holds `CuiPreferences.cpp/.h` (Target 3b).
- `swgClientUserInterface.vcxproj` → static lib **`swgClientUserInterface.lib`**. Holds `SwgCuiHudAction.cpp` (Target 3a).
- `SwgClient.vcxproj` references **both** `clientUserInterface.lib` and `swgClientUserInterface.lib` (lines 103/158/204 — all 3 configs). → Both Target-3 edits ARE in the link-grep + boot gate.
- `SwgGodClient.vcxproj` Target-1 edit is **out of closure** → grep-only verify, never build (D-03).

---

## Pattern Assignments

### `SwgGodClient.vcxproj` — Target 1, build-config dead-token (D-01/02/03)

**Precedent commit:** `4bc512b45` — *"chore(14-02): purge inline vivox/VChat/libsndfile from 8 editor .vcxproj + 16 editor .rsp"* — touched **this same file** (`SwgGodClient.vcxproj`, 12 lines), removing dead lib tokens from a long inline `<AdditionalDependencies>` run while explicitly preserving the soePlatform KEEP-list. This is the exact same operation Target 1 needs.

**Removal pattern to mirror (single-token delete on the dep run):**
```text
// 4bc512b45 BEFORE: ...xpcom.lib;xul.lib;vivoxSharedWrapper_release.lib;%(AdditionalDependencies)
// 4bc512b45 AFTER:  ...xpcom.lib;xul.lib;%(AdditionalDependencies)
```
Target 16 mirror (the only edit, line 99 Debug config ONLY):
```text
// BEFORE: ...rdp.lib;TcpLibrary.lib;989crypt.lib;tinyxmld.lib;tinyxmld_STL.lib...
// AFTER:  ...rdp.lib;TcpLibrary.lib;tinyxmld.lib;tinyxmld_STL.lib...
```

**KEEP-list discipline (carried verbatim from 4bc512b45's commit body — "KEEP soePlatform\libs + Base.lib/ChatAPI.lib (Pitfall 4)"):** the 9 soePlatform tokens `Base.lib;ChatAPI.lib;ChatMono.lib;CommodityAPI.lib;dbgutil.lib;monapi.lib;Network.lib;rdp.lib;TcpLibrary.lib` sit immediately before `989crypt.lib` on line 99 — **remove the single token `989crypt.lib;` only.** All 9 are backed on disk in `soePlatform/libs/Win32-Debug` (RESEARCH.md verified).

> **DO-NOT-TOUCH adjacency trap:** `crypto.lib` appears separately *earlier* on line 99 (a different, live token — RESEARCH.md Security §V6). It is NOT `989crypt`. Only `989crypt.lib` is dead.

**Optimized (line 143) / Release (line 185):** no `989crypt`, no soePlatform — **no edit** (RESEARCH.md). Don't touch.

**`.rsp` note:** `989crypt` is in the inline vcxproj only — no `.rsp`, no `.filters` reference (RESEARCH.md). Single-point edit; matches memory `project_decruft_removal_build_graph_gotchas` (".rsp vestigial — edit inline vcxproj").

**Verification gate (grep-only — D-03, NEVER build SwgGodClient):**
```text
rg -i "989crypt|stationapi" src/game/client/application/SwgGodClient/build/win32/SwgGodClient.vcxproj   → 0
rg -i "lcdui|VideoCapture|vivox|libMozilla|xpcom|xul|VChatAPI" <same file>                              → 0  (D-02 sweep, already 0)
rg "Base.lib|ChatAPI.lib|TcpLibrary.lib" <same file>                                                    → still PRESENT (KEEP-list survives, Pitfall 2)
```

---

### `SwgCuiHudAction.cpp` — Target 3a, dead source block (D-06)

**Precedent commit:** `e5a76be9f` — *"refactor(13-02): delete dead #if 0 capture source residue (D-02/D-02a)"* — deleted dead source blocks across 5 client UI/game files (191 deletions) that were unreachable because their callers/sinks were de-wired in a prior phase. Same shape: a block left computing values that nothing consumes after a feature de-wire.

**Removal pattern:** delete the contiguous dead computation block, leave the enclosing scope's structural braces intact. Here:
- Delete **lines 1170–1189 inclusive** — from the `// create the final URL` comment (`:1170`) through the commented-out `//ShellExecute(...)` (`:1189`), including the `if (finalUrl.length() > 2048)` truncation branch and its `return false;` at `:1186`.
- **KEEP** the closing `}` at `:1190` (it closes the enclosing `CuiActions::service` branch).

**Scope guard (RESEARCH.md Pitfall 1 — the one conscious decision):** honor the **narrow** D-06 bounds (1170-1189). Do NOT extend into the `httpParams` accumulation (1081-1169), `Game::getPlayerPath`, `CuiLoginManager`, or the `s_confirmCsBrowserSpawn` confirm-box logic (1074-1079) — those are out of the locked range. **Warning sign:** if the diff touches any of those, STOP.

**Verification gate:**
```text
rg "finalUrl" src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiHudAction.cpp   → 0
```
Then the shared link-grep gate (below) — covered because `swgClientUserInterface.lib` is in the SwgClient closure.

---

### `CuiPreferences.cpp` + `CuiPreferences.h` — Target 3b, full dead API (D-06/D-07)

**Precedent commit:** `78d7373ff` — *"refactor(13): remove orphaned AudioCapture file-statics (code-review WR-01)"* — **near-identical structural analog.** Its commit body describes Target 3b's exact situation: *"s_audioFilterProvider + s_audioFilter were read/written exclusively by the #if 0 AudioCapture functions removed in 13-02; now dead unguarded namespace state. Deleted both."* — orphaned statics whose only readers/writers were removed by a prior feature delete (there: VideoCapture; here: Phase 14 Vivox voice-UI delete). It even names the verification verbatim: *"Incremental Debug rebuild re-confirmed the link gate: 2138 Searching / 0 unresolved external / 0 LNK1181."*

**Removal pattern (delete statics + their entire accessor API + their decls — all in one atomic commit):**
```cpp
// CuiPreferences.cpp:397-398  — statics (delete)
float ms_speakerVolume = 0.5f;
float ms_micVolume = 0.5f;

// CuiPreferences.cpp:3460-3484 — 4 accessor bodies + surrounding //--- dividers (delete)
void  CuiPreferences::setSpeakerVolume(float volume) { ms_speakerVolume = volume; }
float CuiPreferences::getSpeakerVolume()             { return ms_speakerVolume; }
void  CuiPreferences::setMicVolume(float volume)     { ms_micVolume = volume; }
float CuiPreferences::getMicVolume()                 { return ms_micVolume; }

// CuiPreferences.h:553-557 — 4 declarations (delete)
static float getSpeakerVolume();
static void  setSpeakerVolume(float volume);
static float getMicVolume();
static void  setMicVolume(float volume);
```

**Zero-caller / no-placeholder discipline:** RESEARCH.md confirmed **0 external callers** repo-wide and **no save/load hook** — clean full-API removal. D-07: these are plain `float` statics + flat get/set, NOT a positional enum/datatable row → the Phase 14 CR-01 ordinal-placeholder rule does **NOT** apply. Delete outright; no placeholders.

**Verification gate:**
```text
rg "ms_speakerVolume|ms_micVolume|[gs]etSpeakerVolume|[gs]etMicVolume" src                              → 0
rg "speakerVolume|micVolume|SpeakerVolume|MicVolume" src  (outside CuiPreferences — save/load-hook check) → 0
```
Then the shared link-grep gate — covered because `clientUserInterface.lib` is in the SwgClient closure.

---

## Shared Patterns

### Removal discipline (THE central pattern — Phases 12–15 → mirror verbatim)

**Source precedents:** `bb2e101b8` (13-01 atomic unlink), `e5a76be9f` (13-02 dead-source delete), `78d7373ff` (13 orphaned-static delete), `4bc512b45` (14-02 vcxproj token purge), `d1dcb8447` (15-04 residue sweep).
**Apply to:** all 3 modified targets.

The proven per-target loop:
1. **Atomic per-target commit.** The 3 targets are non-overlapping files (SwgGodClient.vcxproj / SwgCuiHudAction.cpp / CuiPreferences.cpp+.h) → one commit per target (planner's discretion per CONTEXT.md). Precedent commits are each single-concern (`chore(NN-..)` / `refactor(NN-..)`).
2. **Per-token `rg` == 0** for the symbol that commit removed (the per-target gates above). The repo's deterministic removal-correctness signal — RESEARCH.md "Don't Hand-Roll": never eyeball "looks removed," always `rg → 0`.
3. **SwgClient Debug+Release link-grep** (shared gate below) after the source edits land.

### Link-grep gate (NOT msbuild exit code)

**Source:** memory `project_decruft_removal_build_graph_gotchas`; precedent `78d7373ff` ("0 unresolved external / 0 LNK1181").
**Apply to:** Target 3a + 3b (the in-closure source edits). NOT Target 1 (out of closure, grep-only).

```text
# Build SwgClient Debug + Release with full VS msbuild path, /nodeReuse:false, from PowerShell.
# Delete SwgClient_d.exe / SwgClient_r.exe FIRST to force a real relink.
# Then grep each link log:
"unresolved external symbol"   → 0     (Debug)
"unresolved external symbol"   → 0     (Release)
```
- **`/FORCE` trap (RESEARCH.md Pitfall 4):** `<ForceFileOutput>Enabled</ForceFileOutput>` is set in the Debug link (line 107) — msbuild can exit 0 while masking unresolved externals. Grep the LOG for the symbol, never trust exit code.
- **Optimized config EXEMPT** (DEF-14-01 / memory `project_optimized_config_safeseh_pre_existing`) — pre-existing SAFESEH LNK1281, validate via Debug+Release only.

### Boot smoke (single, not the full matrix — D-08)

**Source:** memory `feedback_debug_exe_reads_client_d_cfg`; precedent Phase 15 close `16fd3ac4c`.
**Apply to:** phase gate (once, after all edits land).
```text
# stage/client_d.cfg:37 already has rasterMajor=11 (D3D11) — no cfg edit needed.
# Launch SwgClient_d.exe (Debug exe reads client_d.cfg, NOT client.cfg).
# Reach character select. ONE boot — no-behavior-change phase; not the rasterMajor=5 AND =11 matrix.
```

### Verify-only assertion (Target 2 — D-04)

**Source:** `1d6b80242` (12-03 original lcdui editor-vcxproj fix) + `d1dcb8447` (15-04 finished the LIBPATH residue).
**Apply to:** the 4 editor vcxproj.
```text
rg -i "lcdui" <each of 4 editor .vcxproj>   → 0   (already 0; record as confirmed, no edit)
```
Fold into the verification gate per CONTEXT.md discretion — it's an assertion, not a change.

## No Analog Found

None. Every modified file has an exact same-type removal precedent in Phase 12–15 history. (This phase is explicitly a smaller instance of the established Decruft removal discipline — RESEARCH.md "reuse that discipline verbatim — do not invent a new validation mechanism.")

## Metadata

**Analog search scope:** `git log --all` Phases 12–15 cleanup commits; `SwgGodClient/AnimationEditor/ClientEffectEditor/LightningEditor/ParticleEditor` build/win32; `clientUserInterface` + `swgClientUserInterface` + `SwgClient` vcxproj.
**Precedent commits cited:** `1d6b80242`, `4bc512b45`, `e5a76be9f`, `78d7373ff`, `d1dcb8447`, `16fd3ac4c`.
**Pattern extraction date:** 2026-05-26
