# Phase 16: v2.1 Tech-Debt Cleanup - Research

**Researched:** 2026-05-26
**Domain:** C++ SWG client port — dead-code/dead-token removal (MSBuild/MSVC v145, Win32), no behavior change
**Confidence:** HIGH (every claim verified against the live tree with file:line + grep counts)

## Summary

This is a small, deterministic, no-behavior-change cleanup. The job of this research was NOT to
re-decide scope (CONTEXT.md D-01..D-08 already lock it) but to **confirm or correct each dead-code /
dead-token claim against the live tree**. **All eight decisions are CONFIRMED correct** — every
CONTEXT.md claim held up under verification. There are zero "WRONG claim" surprises; the highest-value
nuance is a *scoping* observation on Target 3a (the dead block is actually wider than the literal
`~:1170-1189` range, but D-06 deliberately scopes the removal narrowly — keep the diff small and honor
the lock).

The three edit targets reduce to: (1) remove **one** token `989crypt.lib` from a single inline
`<AdditionalDependencies>` line in `SwgGodClient.vcxproj:99` (Debug config only); (2) Target 2 is a
verified **no-op** (all 4 editor vcxproj already 0 `lcdui`); (3) two source edits in SwgClient-linked
libs — the dead `finalUrl` block (`SwgCuiHudAction.cpp:1170-1189`) and the orphaned voice-volume API
(`CuiPreferences.cpp:397-398` + `:3460-3484`, `CuiPreferences.h:553-557`), the latter with
**ZERO external callers and no save/load hook** confirmed repo-wide.

**Primary recommendation:** Plan three independent atomic-commit tasks (SwgGodClient vcxproj token /
HudAction.cpp / CuiPreferences.cpp+.h) plus one folded Target-2 verify step, all gated by the proven
Phase 12–15 discipline: per-token `rg` == 0, then SwgClient Debug+Release link-grep `unresolved external
symbol` == 0 (Optimized EXEMPT), then one rasterMajor=11 boot to char-select.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| SwgGodClient link-dep hygiene (Target 1) | Build config (vcxproj) | — | Out-of-`/t:SwgClient`-closure tool; grep-verified only, never built (Qt MSB8066) |
| Editor LIBPATH hygiene (Target 2) | Build config (vcxproj) | — | Verify-only; already swept by 15-04 |
| Dead UI source removal (Target 3a/3b) | Client UI libs (`swgClientUserInterface`, `clientUserInterface`) | SwgClient link closure | Both libs link into SwgClient → covered by link-grep + boot |

There is no runtime/server/DB tier in scope — this is a pure build-config + dead-source cleanup on the
Win32 client.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01:** Unlink, do NOT restore — remove dead `989crypt.lib` token from `SwgGodClient.vcxproj`; never re-add deleted `stationapi/` lib.
- **D-02:** Sweep, not surgical — audit SwgGodClient `AdditionalDependencies` across all 3 configs (Debug/Release/Optimized) for any token whose backing lib was deleted in v2.1 (Phases 12–15): `stationapi`/`989crypt`, `lcdui`, `VideoCapture`, `vivox*`, `libMozilla`/`xpcom`/`xul`. KEEP-list guard: soePlatform tokens (`Base.lib`, `ChatAPI.lib`, `ChatMono.lib`, `CommodityAPI.lib`, `monapi.lib`, `Network.lib`, `rdp.lib`, `TcpLibrary.lib`) are preserved; only `989crypt.lib` (stationapi) is dead in that cluster. Do not over-remove.
- **D-03:** Grep-only verification for SwgGodClient — cannot be built (Qt MSB8066, out of `/t:SwgClient`); verify via `grep == 0`. Do NOT attempt to build SwgGodClient.
- **D-04:** Target 2 is a verify-only no-op — all 4 editor vcxproj already contain 0 `lcdui` refs (swept by 15-04). Record `lcdui-editor-LIBPATH == 0`; no edit required.
- **D-05:** No doc-staleness work — leave 12-VERIFICATION.md stale score line + 13-SUMMARY.md empty frontmatter out of scope.
- **D-06:** Full removal, both files. `SwgCuiHudAction.cpp` (~:1170-1189): delete entire dead `finalUrl` block (incl. `if (finalUrl.length() > 2048)` truncation branch) — dead because trailing `ShellExecute(...)` at :1189 is commented out. `CuiPreferences`: remove full speaker/mic volume API — 2 statics (`.cpp:397-398`) + 4 accessors (`.cpp:3460-3484`) + 4 header decls (`.h:553-557`). Zero external callers confirmed.
- **D-07:** CR-01 ordinal lesson does NOT apply — these are plain `float` statics + accessors, not positional enum/datatable rows. Delete outright; no placeholders.
- **D-08:** Link-grep + single char-select smoke. SwgClient Debug+Release link-grep `unresolved external symbol` == 0 (Optimized EXEMPT — DEF-14-01 SAFESEH LNK1281). Boot once to char-select under rasterMajor=11 (D3D11). No-behavior-change phase — one boot, not the full dual-renderer matrix.

### Claude's Discretion
- Commit granularity — three targets are independent (SwgGodClient.vcxproj, HudAction.cpp, CuiPreferences.cpp/.h non-overlapping); plan cleanest atomic-commit split.
- Whether Target-2 verify step is its own plan step or folded into the build/verification gate.

### Deferred Ideas (OUT OF SCOPE)
- AR-15-01 future-TCG-revival re-evaluation (future v2.x).
- `stage/client_d.cfg` accumulated-test-settings cleanup (coupled to v2.2 visual-parity close).
- Nyquist finalisation for Phases 12 & 13 (do via `/gsd:validate-phase 12`+`13`, not a code phase).
- Audit doc-staleness items (12-VERIFICATION.md stale score line; 13-SUMMARY.md empty `requirements_completed`) — left alone per D-05.
</user_constraints>

<phase_requirements>
## Phase Requirements

No formal REQ-IDs — this is post-hoc tech-debt closure. Traces to `.planning/v2.1-MILESTONE-AUDIT.md`
`tech_debt` frontmatter (the definition of done).

| Source item (audit tech_debt) | Phase target | Research Support |
|-------------------------------|--------------|------------------|
| Phase 12: "SwgGodClient Debug AdditionalDependencies still lists 989crypt.lib but the file was deleted with stationapi/ — would LNK1181 if built" | Target 1 (D-01/02/03) | CONFIRMED: token at `SwgGodClient.vcxproj:99` (Debug only, count=1); `stationapi/` dir ABSENT; `989crypt.lib` ABSENT on disk; soePlatform KEEP-list all PRESENT |
| Phase 12: "Inert lcdui\lib /LIBPATH segments remain in 4 editor vcxproj" | Target 2 (D-04) | CONFIRMED no-op: all 4 editor vcxproj `lcdui` count = 0 (swept by 15-04) |
| Phase 14: "Dead statics ms_speakerVolume/ms_micVolume remain in CuiPreferences.cpp after voice-UI deletion" | Target 3b (D-06) | CONFIRMED: statics `.cpp:397-398`, accessors `.cpp:3460-3484`, decls `.h:553-557`; ZERO external callers repo-wide; no save/load hook |
| Phase 15: "Dead-store finalUrl in SwgCuiHudAction.cpp (~:1170-1189)" | Target 3a (D-06) | CONFIRMED dead: `finalUrl` consumed only by commented-out `//ShellExecute(...)` at :1189; no other consumer |
</phase_requirements>

## Verification Results — D-01..D-08 Status Board

> The core deliverable. Each decision marked CONFIRMED (claim held) or CORRECTED (claim wrong — flag).

| Decision | Status | Evidence |
|----------|--------|----------|
| **D-01** unlink 989crypt | ✅ CONFIRMED | Token present exactly **1×** at `SwgGodClient.vcxproj:99` (Debug `<AdditionalDependencies>`). `stationapi/` dir ABSENT; `989crypt.lib` ABSENT anywhere on disk → genuine LNK1181-if-built. |
| **D-02** sweep all configs + KEEP-list | ✅ CONFIRMED | Dead-token sweep across the file: `989crypt`=1, `stationapi`=0, `lcdui`=0, `VideoCapture`=0, `vivox`=0, `libMozilla`=0, `xpcom`=0, `xul`=0, `VChatAPI`=0. Only `989crypt.lib` is dead. All 9 soePlatform KEEP libs PRESENT on disk in `soePlatform/libs/Win32-Debug`. |
| **D-03** grep-only verify | ✅ CONFIRMED | `989crypt` is in vcxproj only (no `.rsp`, no `.filters`); single inline edit point; build-impossible tool → grep==0 is the correct gate. |
| **D-04** Target 2 no-op | ✅ CONFIRMED | All 4 editor vcxproj: `lcdui` count = **0** each. No edit required. |
| **D-05** no doc work | ✅ N/A | Out of scope; not researched per decision. |
| **D-06** full removal both files | ✅ CONFIRMED (one scoping nuance — see Pitfall 1) | finalUrl: 5 occurrences (:1173/1175/1177/1182/1189), only consumer is commented `//ShellExecute` at :1189. Voice API: statics + 4 accessors + 4 decls exactly where stated; ZERO external callers; no save/load hook. |
| **D-07** no ordinal placeholders | ✅ CONFIRMED | `ms_speakerVolume`/`ms_micVolume` are plain `float` statics (`.cpp:397-398`, init `0.5f`); accessors are flat get/set. Not a positional enum/datatable. CR-01 irrelevant. |
| **D-08** link-grep + 1 boot | ✅ CONFIRMED feasible | SwgClient.vcxproj links BOTH `clientUserInterface.lib` and `swgClientUserInterface.lib` → D-06 edits are in-closure. `stage/client_d.cfg:37` already has `rasterMajor=11` → boot smoke ready. |

**No CONTEXT.md claim was found WRONG.** The only thing the planner must decide consciously is the
Target-3a removal *scope* (Pitfall 1 below) — and the locked D-06 text already points to the narrow choice.

## Target-by-Target Evidence

### Target 1 — SwgGodClient dead deps (D-01/02/03)

**File:** `src/game/client/application/SwgGodClient/build/win32/SwgGodClient.vcxproj`

Three configs exist (Debug / Optimized / Release). Their `<Link><AdditionalDependencies>` differ sharply:

| Config | Line | soePlatform cluster present? | `989crypt.lib`? | Dep-list shape |
|--------|------|------------------------------|-----------------|----------------|
| Debug | 99 | YES (`Base;ChatAPI;ChatMono;CommodityAPI;dbgutil;monapi;Network;rdp;TcpLibrary`) | **YES** (`...TcpLibrary.lib;989crypt.lib;tinyxmld.lib...`) | Full ~200-token list |
| Optimized | 143 | NO | NO | Short ~19-token list (no soePlatform, no 989crypt) |
| Release | 185 | NO | NO | Short ~19-token list (no soePlatform, no 989crypt) |

**The edit is a single-token deletion on line 99 only:** remove `989crypt.lib;` from the run
`...rdp.lib;TcpLibrary.lib;989crypt.lib;tinyxmld.lib;tinyxmld_STL.lib...`. Optimized/Release need no edit.

**Backing-file disk truth (verified):**
- `src/external/3rd/library/stationapi/` → **ABSENT**
- `989crypt.lib` anywhere under `src/external/` → **ABSENT**
- `soePlatform/libs/Win32-Debug/` → present; contains `Base.lib, ChatAPI.lib, ChatMono.lib, CommodityAPI.lib, Network.lib, TcpLibrary.lib, dbgutil.lib, monapi.lib, rdp.lib` — **all 9 KEEP-list tokens backed.** (Also present but unreferenced: `Base_vchat.lib, NetworkSupport.lib, VChatAPI.lib` — irrelevant, no token references them.)
- LIBPATH: Debug config references `soePlatform\libs\Win32-Debug` (intact). Optimized/Release do NOT reference soePlatform at all.

**`.rsp` note (build-graph gotcha):** SwgGodClient has 16 `.rsp` files including `libraries_d.rsp`, but
`libraries_d.rsp` is **empty** and the vcxproj does **not** `@`-reference any `.rsp`. `989crypt` is NOT in
any `.rsp`. The inline `<AdditionalDependencies>` is the sole authoritative location → single-point edit,
no `.rsp` touch needed. Matches memory `project_decruft_removal_build_graph_gotchas` (".rsp vestigial,
edit inline vcxproj").

### Target 2 — editor lcdui LIBPATH (D-04, verify-only)

| Editor vcxproj | `lcdui` count |
|----------------|---------------|
| `AnimationEditor/build/win32/AnimationEditor.vcxproj` | **0** |
| `ClientEffectEditor/build/win32/ClientEffectEditor.vcxproj` | **0** |
| `LightningEditor/build/win32/LightningEditor.vcxproj` | **0** |
| `ParticleEditor/build/win32/ParticleEditor.vcxproj` | **0** |

D-04 holds: **verify-only, no edit.** Fold into the verification gate as `lcdui-editor-LIBPATH == 0`.

### Target 3a — SwgCuiHudAction.cpp dead `finalUrl` block (D-06)

**File:** `src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiHudAction.cpp`

`finalUrl` symbol map (5 hits, all inside the `CuiActions::service` branch):
- `:1173` `std::string finalUrl = HttpGetEncoder::getUrl(baseUrl, httpParams);`
- `:1175` `if (finalUrl.length() > 2048)`
- `:1177` `unsigned diff = finalUrl.length() - 2048;`
- `:1182` `finalUrl = HttpGetEncoder::getUrl(baseUrl, httpParams);`
- `:1189` `//ShellExecute(NULL, "open", finalUrl.c_str(), NULL, "", SW_SHOW);` ← **commented-out, sole consumer**

**Confirmed dead:** `finalUrl` has no consumer except the commented `ShellExecute`. `baseUrl` (`:1172`) is
local and used only to build `finalUrl`. No reference to `finalUrl` after `:1189`.

**Precise dead-block bounds (the narrow D-06 reading):** lines **1170–1189** inclusive — from the
`// create the final URL` comment (`:1170`) through the commented `//ShellExecute` (`:1189`). This block
contains the `if (finalUrl.length() > 2048)` truncation branch (`:1175-1188`) including an early
`return false;` at `:1186` (also dead once the block goes). The closing `}` at `:1190` belongs to the
enclosing `CuiActions::service` branch — keep it.

### Target 3b — CuiPreferences voice-volume API (D-06)

**Statics** (`CuiPreferences.cpp:397-398`):
```cpp
float ms_speakerVolume = 0.5f;
float ms_micVolume = 0.5f;
```
**Accessors** (`CuiPreferences.cpp:3460-3484`): `setSpeakerVolume` (:3460), `getSpeakerVolume` (:3467),
`setMicVolume` (:3474), `getMicVolume` (:3481) — four flat get/set bodies, each separated by `//---`
comment dividers.
**Declarations** (`CuiPreferences.h:553-557`): the 4 `static` declarations.

**Caller audit (repo-wide, `src/**`, `*.cpp/*.h/*.hpp/*.inl`):** every single hit for
`getSpeakerVolume`, `setSpeakerVolume`, `getMicVolume`, `setMicVolume`, `ms_speakerVolume`,
`ms_micVolume` resolves to **CuiPreferences.cpp / CuiPreferences.h itself** (the def/decl/body lines
above). **ZERO external callers.** D-06's "zero callers" claim CONFIRMED.

**Save/load/register hook check:** repo-wide grep for `speakerVolume`/`micVolume`/`SpeakerVolume`/
`MicVolume` outside CuiPreferences → **0 hits.** No UserSettings persistence, no config-table loader, no
`register`/`fetch` of these prefs. The statics are pure default-initialized orphans → clean full-API
removal with no orphaned load/save hook to chase.

## Build-Graph Constraints (carry into every task)

| Constraint | Source | Plan implication |
|------------|--------|------------------|
| `/FORCE` masks unresolved externals | memory `project_decruft_removal_build_graph_gotchas` | Verify removals by grepping link log for `unresolved external symbol` == 0, NOT by msbuild exit 0 |
| msbuild must be full-path + `/nodeReuse:false`; delete target exe to force relink; run from PowerShell not Git Bash | same memory | Bake into the build-gate task command |
| Debug exe reads `stage/client_d.cfg` (NOT `client.cfg`) | memory `feedback_debug_exe_reads_client_d_cfg` | rasterMajor=11 already set at `client_d.cfg:37` — boot smoke ready |
| Optimized config LNK1281 SAFESEH pre-existing (DEF-14-01) | memory `project_optimized_config_safeseh_pre_existing` | EXEMPT from link gate; validate via Debug+Release link-grep only |
| SwgGodClient pre-broken on Qt (MSB8066), out of `/t:SwgClient` closure | CONTEXT D-03 + audit | NEVER build SwgGodClient; grep-only verify its vcxproj edit |
| Both edited libs are in SwgClient link closure | `SwgClient.vcxproj` links `clientUserInterface.lib` + `swgClientUserInterface.lib` (verified) | D-06 source edits ARE covered by the link-grep + boot gate |

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Confirming a token/symbol is gone | A manual "looks removed" eyeball | `rg <token> <path>` → assert 0 hits | The Phase 12–15 grep-zero gate is the project's proven, deterministic removal-correctness signal |
| Confirming the link is clean | `msbuild` exit code | grep the link log for `unresolved external symbol` == 0 | `/FORCE` masks unresolved externals — exit 0 lies (memory-documented trap) |
| Validating no init regression | Full dual-renderer matrix | One rasterMajor=11 boot to char-select (D-08) | No-behavior-change phase; one boot suffices per the locked verification bar |

**Key insight:** this domain has NO unit-test harness; correctness is established by grep-zero + link-grep
+ a human boot. Reuse that discipline verbatim — do not invent a new validation mechanism.

## Common Pitfalls

### Pitfall 1: Over-removing the Target-3a block (scope creep beyond D-06)
**What goes wrong:** A literal-minded executor notices that the *entire* `httpParams` accumulation in the
`CuiActions::service` branch (`SwgCuiHudAction.cpp:1081-1189`) exists only to feed the dead `finalUrl`, and
deletes all ~108 lines (the `httpParams[...]` writes, the `Game::getPlayerPath` call, etc.).
**Why it happens:** D-06 says "remove the whole dead computation, not just the assignment" — which *can* be
read maximally. But the live `if (params.length() == 0)` confirm-box logic at `:1074-1079` is still live UI,
and the broader `httpParams` block, while technically also dead-store, is OUTSIDE the locked
`~:1170-1189` range CONTEXT.md names.
**How to avoid:** Honor the locked narrow scope — delete **only lines 1170–1189** (the `// create the final
URL` comment through the commented `//ShellExecute`). Leave the `httpParams` accumulation (1081-1169) and
the confirm-box logic intact. Minimal diff; matches the literal D-06 bounds; avoids touching live code.
**Warning signs:** Diff touches `Game::getPlayerPath`, `CuiLoginManager`, or the `s_confirmCsBrowserSpawn`
message-box lines — STOP, that's out of scope.

### Pitfall 2: Removing a soePlatform KEEP-list token by mistake (Target 1)
**What goes wrong:** During the D-02 "sweep" an executor over-reaches and strips `Base.lib`/`ChatAPI.lib`/etc.
along with `989crypt.lib` because they're adjacent in the same Debug dep run.
**Why it happens:** The 9 KEEP tokens sit immediately before `989crypt.lib` on line 99 (`...rdp.lib;
TcpLibrary.lib;989crypt.lib;...`).
**How to avoid:** Remove the single token `989crypt.lib;` ONLY. All 9 soePlatform libs are backed on disk
(verified) — removing them would create real LNK1181s if the tool were ever fixed. Post-edit grep must show
the KEEP tokens still present.
**Warning signs:** `rg "Base.lib|ChatAPI.lib|TcpLibrary.lib" SwgGodClient.vcxproj` returns fewer hits than before.

### Pitfall 3: Editing the wrong cfg / wrong renderer for the boot smoke
**What goes wrong:** Setting `rasterMajor` in `client.cfg` (Release) instead of `client_d.cfg` (Debug),
or booting the Release exe when the link gate ran Debug.
**Why it happens:** Two cfg files exist; the Debug exe silently ignores `client.cfg`.
**How to avoid:** `client_d.cfg:37` already has `rasterMajor=11`. Boot `SwgClient_d.exe`. No cfg edit needed.
**Warning signs:** Renderer comes up as D3D9 (rasterMajor=5) when you expected D3D11.

### Pitfall 4: Trusting msbuild exit 0 for the link gate
**What goes wrong:** `/FORCE` (`<ForceFileOutput>Enabled</ForceFileOutput>` is set in the Debug link, line 107)
lets the link "succeed" while masking unresolved externals.
**How to avoid:** Grep the actual link log for `unresolved external symbol` == 0. Delete the target exe first
to force a real relink.

## Code Examples

### Target 1 — the single-token edit (SwgGodClient.vcxproj:99, Debug only)
```text
// BEFORE (excerpt of the long AdditionalDependencies run):
...rdp.lib;TcpLibrary.lib;989crypt.lib;tinyxmld.lib;tinyxmld_STL.lib;ws2_32.lib...
// AFTER:
...rdp.lib;TcpLibrary.lib;tinyxmld.lib;tinyxmld_STL.lib;ws2_32.lib...
```
Optimized (line 143) and Release (line 185) dep lists do NOT contain `989crypt.lib` — no edit there.

### Target 3a — the dead block to delete (SwgCuiHudAction.cpp:1170-1189)
```cpp
		// create the final URL                                    // :1170  ← delete from here

		std::string baseUrl = ConfigClientGame::getCsTrackingBaseUrl();
		std::string finalUrl = HttpGetEncoder::getUrl(baseUrl, httpParams);

		if (finalUrl.length() > 2048)
		{
			unsigned diff = finalUrl.length() - 2048;
			if (httpParams["Charchat"].length() > diff)
			{
				httpParams["Charchat"] = httpParams["Charchat"].substr(diff);
				finalUrl = HttpGetEncoder::getUrl(baseUrl, httpParams);
			}
			else
			{
				return false;
			}
		}
		//ShellExecute(NULL, "open", finalUrl.c_str(), NULL, "", SW_SHOW);  // :1189  ← delete to here
	}                                                                       // :1190  ← KEEP (branch close)
```

### Target 3b — the voice-volume API to delete
```cpp
// CuiPreferences.cpp:397-398  — statics
float ms_speakerVolume = 0.5f;
float ms_micVolume = 0.5f;

// CuiPreferences.cpp:3460-3484 — 4 accessor bodies (+ surrounding //--- dividers)
void  CuiPreferences::setSpeakerVolume(float volume) { ms_speakerVolume = volume; }
float CuiPreferences::getSpeakerVolume()             { return ms_speakerVolume; }
void  CuiPreferences::setMicVolume(float volume)     { ms_micVolume = volume; }
float CuiPreferences::getMicVolume()                 { return ms_micVolume; }

// CuiPreferences.h:553-557 — 4 declarations
static float getSpeakerVolume();
static void  setSpeakerVolume(float volume);
static float getMicVolume();
static void  setMicVolume(float volume);
```

## Runtime State Inventory

> This is a code/config dead-token removal, not a rename/migration. There are no stored keys, no live
> service config, no OS-registered names, no secrets, and no installed-package artifacts keyed to the
> removed symbols. Each category checked explicitly:

| Category | Items Found | Action Required |
|----------|-------------|------------------|
| Stored data | None — voice-volume statics (`ms_speakerVolume`/`ms_micVolume`) are NOT persisted (repo-wide grep for save/load hook == 0). No DB/datastore in scope. | none |
| Live service config | None — no external service references `989crypt`, `finalUrl`, or voice-volume. `stage/client_d.cfg` has a stale `voiceChatEnabled=false` key (audit item) but that is OUT of scope per D-05/deferred. | none (cfg cleanup deferred) |
| OS-registered state | None — no Task Scheduler / service registration touches these symbols. | none |
| Secrets/env vars | None — no secret key or env var references the removed tokens/symbols. | none |
| Build artifacts | `989crypt.lib` backing file already ABSENT on disk; no compiled artifact regenerates it. SwgGodClient is never built (Qt MSB8066), so no stale obj/exe to reconcile. SwgClient relinks from the edited `clientUserInterface`/`swgClientUserInterface` objs — the link gate covers this. | delete target exe before relink (per build-graph gotcha) |

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| MSBuild (VS v145 toolset) | D-08 Debug+Release link gate | Assumed ✓ (used through Phases 11–15) | v145 / VS 2022-class | none — required |
| `rg` (ripgrep) | grep-zero gates | ✓ (used this research) | — | `grep -r` (slower) |
| SwgClient_d.exe + gl0X_d.dll in `stage/` | D-08 boot smoke | Produced by build (Koogie post-build copy lands them per memory `feedback_rebuild_swgclient_when_clientGame_changes`) | — | none |
| Live SWGSource VM / server | D-08 boot to char-select | Operator-provided (human boot gate) | — | none — manual gate |

**Missing dependencies with no fallback:** none identified. The link toolchain and stage layout are the
same proven set used through Phase 15.

## Validation Architecture

> Nyquist enabled. No-behavior-change cleanup — validation is framed around **removal-correctness +
> absence-of-residue + absence-of-regression**, the established Phase 12–15 pattern. There is no C++
> unit-test harness in this tree (confirmed: no `test/`/`tests/` dirs under `src`).

### Test Framework
| Property | Value |
|----------|-------|
| Framework | None (C++ engine cleanup; no unit-test harness in tree) |
| Config file | none — validation is `rg` grep-zero + MSBuild link-log grep + one manual boot |
| Quick run command | per-token `rg`: `rg -i "989crypt" src` → 0; `rg "finalUrl" .../SwgCuiHudAction.cpp` → 0; `rg "ms_speakerVolume\|ms_micVolume\|getSpeakerVolume\|setSpeakerVolume\|getMicVolume\|setMicVolume" src` → 0; per-editor `rg -i lcdui` → 0 |
| Full suite command | MSBuild SwgClient **Debug + Release** (full VS path, `/nodeReuse:false`, from PowerShell; delete `SwgClient_d.exe`/`SwgClient_r.exe` first), then grep each link log for `unresolved external symbol` (== 0). **Optimized EXEMPT** (DEF-14-01). |

### Phase Requirements → Test Map
| Target | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| T1 (989crypt) | dead token gone, KEEP-list survives | grep-zero | `rg -i "989crypt" src` → 0; `rg "Base.lib\|ChatAPI.lib\|TcpLibrary.lib" SwgGodClient.vcxproj` still present | ✅ (rg) |
| T2 (lcdui editors) | LIBPATH residue == 0 | grep-zero | per-editor `rg -i lcdui *.vcxproj` → 0 (already 0) | ✅ (rg) |
| T3a (finalUrl) | dead block gone | grep-zero | `rg "finalUrl" .../SwgCuiHudAction.cpp` → 0 | ✅ (rg) |
| T3b (voice API) | full API gone, no callers | grep-zero | `rg "ms_speakerVolume\|ms_micVolume\|[gs]etSpeakerVolume\|[gs]etMicVolume" src` → 0 | ✅ (rg) |
| T3a+T3b (link) | SwgClient links clean | link-grep | MSBuild Debug+Release; link log `unresolved external symbol` == 0 (Optimized EXEMPT) | ✅ (msbuild) |
| All | no init regression | manual boot | `stage/client_d.cfg` rasterMajor=11 (already set); launch `SwgClient_d.exe`; reach char-select | ✅ (manual) |

### Sampling Rate
- **Per task commit:** per-token `rg` == 0 for the symbol that task removed (seconds).
- **Per wave/merge:** SwgClient Debug+Release link-grep `unresolved external symbol` == 0 (minutes).
- **Phase gate:** all grep-zero gates green + Debug+Release link clean + ONE rasterMajor=11 boot to
  char-select human-confirmed (D-08). NOT the full dual-renderer matrix.

### Wave 0 Gaps
- None — no test infrastructure to scaffold. The grep-zero + link-grep + boot gates are the project's
  established validation primitives (Phase 12–15 precedent in 15-VALIDATION.md). Framework install: N/A.

## Security Domain

> `security_enforcement: true`, ASVS level 1. This is a **dead-code removal with no new attack surface** —
> it only *reduces* surface. No new input handling, no new I/O, no new auth/session/crypto.

### Applicable ASVS Categories
| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | No auth code touched |
| V3 Session Management | no | No session code touched |
| V4 Access Control | no | No access-control code touched |
| V5 Input Validation | no | The removed `finalUrl` block *built* a URL from player/session data but its only sink (`ShellExecute`) is already commented out — removal eliminates a latent untrusted-URL-launch path, a net security improvement |
| V6 Cryptography | no | `989crypt.lib` is a *dead link token* (backing lib already deleted); removing the token changes no crypto behavior. `crypto.lib` (a different, live token) is untouched. |

### Known Threat Patterns for this change
| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Latent untrusted-URL `ShellExecute` launch (the dead `finalUrl` → already-commented sink) | Elevation/Tampering | Already neutralized (sink commented); this phase deletes the dead computation entirely — surface reduced to zero |
| `/FORCE`-masked unresolved symbol shipping a broken binary | Tampering (integrity) | Link-grep `unresolved external symbol` == 0 (not exit 0) — the D-08 gate |
| Over-removal of a live KEEP-list dep (would break SwgClient or SwgGodClient if rebuilt) | Denial of Service (build) | KEEP-list guard verified backed on disk; remove only `989crypt.lib` |

No threats opened. Net effect on security posture: neutral-to-positive (removes a dead untrusted-URL path).

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | MSBuild VS v145 toolchain is installed/available on the build machine (not re-probed this session) | Environment Availability | LOW — used continuously through Phases 11–15; if absent the link gate can't run, but that's an operator setup issue, not a plan defect |
| A2 | The live SWGSource VM/server is reachable for the D-08 char-select boot | Validation / Environment | LOW — same human boot gate used at Phase 15 close (commit 16fd3ac4c); operator-provided |

Everything else in this research is `[VERIFIED: codebase grep/Read]` — file:line and counts cited inline.

## Open Questions (RESOLVED)

1. **Target-3a removal scope — narrow (1170-1189) vs. wide (1081-1189)?** — **RESOLVED: narrow bounds (1170-1189) per D-06; implemented in 16-02 Task 1.**
   - What we know: The whole `httpParams` accumulation (1081-1189) is technically dead-store (feeds only the
     dead `finalUrl`). The live confirm-box logic (1074-1079) is separate and must stay.
   - What's unclear: D-06 names `~:1170-1189` but also says "remove the whole dead computation."
   - Recommendation: Honor the **narrow** locked bounds (1170-1189). It is the literal D-06 range, keeps the
     diff minimal, and avoids touching `Game::getPlayerPath`/`CuiLoginManager` live calls. If the planner
     wants the wider sweep, that should be an explicit decision — not an executor judgment call.

## Sources

### Primary (HIGH confidence — all VERIFIED this session via Read/Grep/Bash on the live tree)
- `src/game/client/application/SwgGodClient/build/win32/SwgGodClient.vcxproj` — dep-list lines 99/143/185; soePlatform LIBPATH; `.rsp` vestigial check
- `src/external/3rd/library/` — `stationapi/` absent, `989crypt.lib` absent, `soePlatform/libs/Win32-Debug/` KEEP-list present
- `src/engine/client/application/{AnimationEditor,ClientEffectEditor,LightningEditor,ParticleEditor}/build/win32/*.vcxproj` — lcdui count 0×4
- `src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiHudAction.cpp` — finalUrl block :1170-1189
- `src/engine/client/library/clientUserInterface/src/shared/core/CuiPreferences.{cpp,h}` — statics/accessors/decls + repo-wide caller audit (0 external)
- `src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj` — links clientUserInterface + swgClientUserInterface (in-closure)
- `stage/client_d.cfg:37` — rasterMajor=11 set
- `.planning/v2.1-MILESTONE-AUDIT.md`, `.planning/phases/16-.../16-CONTEXT.md`, `.planning/phases/15-.../15-VALIDATION.md`

### Secondary (MEDIUM)
- `AGENTS.md` — project constraints (boot gate, minimize diff scope)
- Memories: `project_decruft_removal_build_graph_gotchas`, `feedback_debug_exe_reads_client_d_cfg`, `project_optimized_config_safeseh_pre_existing`, `project_rastermajor_values`, `feedback_rebuild_swgclient_when_clientGame_changes`

## Metadata

**Confidence breakdown:**
- Target 1 (989crypt + sweep + KEEP-list): HIGH — token count, disk presence/absence, and LIBPATH all directly verified
- Target 2 (lcdui editors): HIGH — 0 count confirmed on all 4 files
- Target 3a (finalUrl): HIGH — symbol map + sole-consumer-is-commented confirmed; scope nuance flagged
- Target 3b (voice API): HIGH — locations confirmed; ZERO external callers + no save/load hook confirmed repo-wide
- Build graph / validation: HIGH — link closure + cfg state verified; gates reuse proven Phase 15 pattern

**Research date:** 2026-05-26
**Valid until:** 2026-06-25 (stable; the only volatility is the live tree, which won't drift unless another branch edits these exact files before planning)
