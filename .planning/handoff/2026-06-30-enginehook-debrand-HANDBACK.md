# EngineHook de-brand — provider identifiers + logs scrubbed of "Utinni"

**DONE 2026-06-30, committed + pushed `9cee259c0`. No maintainer re-sync required (contract unchanged).**

## Intent

The engine-hookpoint advertisement is a **generic layer** — a name→pointer table any injected
modding overlay can read via `GetEngineHookPoints()`. It is **not tied to Utinni**; Utinni is just
the first (currently only) consumer. This commit continues de-coupling our in-tree code from that one
consumer's name so the layer reads as engine infrastructure, not a Utinni feature.

The shared **contract** was already de-branded in the [2026-06-23 rename](2026-06-23-enginehook-rename-provider-handback.md)
(`Engine*`/`ENGINE_`, `GetEngineHookPoints`). What remained was **exe-local** branding: shim
identifiers, runtime log tags, and one leftover diagnostic probe. This commit scrubs those.

## What changed (no behavior change, contract untouched)

- **~30 shim identifiers** `utinni_*` → `engine_*`: `engine_verifyNoNullNoDup`, the
  groundScene / chatWindow / creatureObject / clientEffect / clientShims forwarders, the freecam
  accessors. All exe-local — the consumer never references these symbols (it reads the table by
  name string, not by our shim identifier).
- **5 forward headers** `utinni_*_forward.h` → `engine_*_forward.h` (+ include-guards + includes).
- **Log tags**: `[utinni.retrigger]`/`[utinni.replay]` → `[effect.retrigger]`/`[effect.replay]`;
  `DEBUG_FATAL("utinni: …")` → `"engine: …"`; `engine_verifyNoNullNoDup` log prefixes follow the
  identifier rename.
- **Removed** the Direct3d9 `[Utinni Probe]` entirely — `namespace UtinniProbe` + `DumpD3D9Vtable`
  + the `CreateDevice` call site + the `winver.h`/`version.lib` pull-in. It was a one-shot leftover
  that dumped the d3d9 vtable to `%TEMP%\d3d9_vtable_probe.txt` **every boot**. gl05/06/07 rebuilt
  for its removal.
- **Contract `.h`/`.inc`**: de-branded descriptive comment prose only. **KEPT** the factual
  `D:/Code/Utinni` sync path + `UtinniCore.dll` name (name set unchanged → comment-only).

## Contract status — UNCHANGED

- `ENGINE_HOOKPOINTS_VERSION` still **13 / 119 names**. Name set, version, and the
  `GetEngineHookPoints` export are identical to the [freecam v13 handback](2026-06-29-utinni-freecam-accessors-HANDBACK.md).
- **The maintainer does NOT need to re-sync from this commit.** The changes are exe-local identifiers
  + comment-only edits on the shared `.h`/`.inc`. The v13 sha256s in the freecam handback still
  govern the binary contract. (Current on-disk `.h`/`.inc` sha256 after the comment-prose edit:
  `engine_hookpoints.h` = `1f09fff7658909cc7d78a1ecdf0bd127348913e8fa674364cd4b540f0d08072b`,
  `engine_hookpoints.inc` = `a4fb865cd562ca3757a75d5fe6649aba077a7f670cce45735aecc36037973b68` —
  informational; contract bytes/names unchanged.)

## Gate

Release/Win32 5-target (`Direct3d11;Direct3d9;Direct3d9_ffp;Direct3d9_vsps;SwgClient`), **0
unresolved external symbols**, `GetEngineHookPoints` undecorated export intact, X-macro
`static_assert(119)` holds, exe + all 4 gl0X plugins restaged. Staged: `stage/SwgClient_r.exe`
(Jun 30 19:16) + `gl05/06/07_r.dll` (Jun 30 19:05).

## Remaining for full genericity (NOT blockers — optional cleanup)

The commit deliberately kept prose that names the consumer as an "accurate reference." If the goal is
a fully consumer-agnostic layer, these are the residuals:

1. **`UTINNI_REQUIRED_COUNT` enum identifier** (`engine_advertise.cpp:741` def + `:746` static_assert
   use) — a real **code** identifier the lowercase `utinni_*`→`engine_*` rename missed (it's
   uppercase). Cleanest quick win; rename to `ENGINE_REQUIRED_COUNT` (or fold into the X-macro count).
   This is the one residual that contradicts "all utinni identifiers scrubbed."
2. **Comment prose still naming Utinni**: `engine_advertise.cpp` (~40 lines of design rationale that
   reference Utinni's typedefs / detour behavior / RVA splits), `ClientMain.cpp:115/130/279`,
   `ClientMain.h:16/27`, `SwgCuiChatWindow.cpp:3354`. These are *accurate* (they explain why a row is
   shaped for the consumer's ABI), so they were kept. To fully generalize, reword to "the injected
   consumer" / "the modding overlay" and move Utinni-specific typedef notes into the handoff trail.

Both are cosmetic — they don't affect the binary, the contract, or any other consumer. Left for a
follow-up if/when full de-branding is prioritized.

## Follow-the-trail

- Prior contract de-brand: [2026-06-23-enginehook-rename-provider-handback.md](2026-06-23-enginehook-rename-provider-handback.md).
- Current contract (v13/119) + maintainer's one open action (rebuild `UtinniCore.dll` on v13):
  [2026-06-29-utinni-freecam-accessors-HANDBACK.md](2026-06-29-utinni-freecam-accessors-HANDBACK.md).
- Cross-request Utinni provider snapshot: [2026-06-29-CONTEXT-CLEAR-utinni-bucketA-checkpoint.md](2026-06-29-CONTEXT-CLEAR-utinni-bucketA-checkpoint.md).
