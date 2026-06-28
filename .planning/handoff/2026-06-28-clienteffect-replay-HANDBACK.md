# Utinni Bucket B-2 HANDBACK — live `.cef` RE-PLAY (`particlePreview::replayClientEffect`)

**Status:** DONE, build-gated, committed + pushed to `origin/master`. **Awaiting maintainer inject-smoke.**
**Date:** 2026-06-28
**Commit:** `33c2a7081` (`feat(enginehook): Utinni Bucket B-2 v8 -- live .cef RE-PLAY`)
**Contract:** `ENGINE_HOOKPOINTS_VERSION` **7 → 8, 105 names** (+1 NAME ADD).

---

## Why this wave (the Bucket B diagnostic result)

Bucket B's `particlePreview::retrigger` (restart live `ParticleEffectAppearance` instances) **resolves
and fires** — confirmed end-to-end. But the file-backed `[utinni.retrigger]` instrumentation
(added this session; `REPORT_LOG`, surfaced via the `[SharedLog] logReportLogs=1` sink → grep
`SwgClient_report.log`) proved **`m_particleSystems` is EMPTY for the real editing targets**:

- A Naboo fountain is a **static world-snapshot `ParticleEffectAppearance`** — never routed through
  `ClientEffectManager`, so `restart()` has nothing to walk.
- Transient `.cef` effects (muzzle/hit/explosion) are **gone before the editor saves** — the effect
  finished, so there is no live instance to restart.

⇒ The high-value preview primitive for **`.cef` authoring** is **RE-PLAY** (spawn fresh, with the
edit), not restart — exactly the path flagged in the Bucket B handback
("re-play via the public `ClientEffectManager::playClientEffect`").

---

## What shipped

### New name
`particlePreview::replayClientEffect` → constant `(void *)&utinni_replayClientEffect` (NOT a `dyn[]` row).

### Exact signature
```cpp
bool utinni_replayClientEffect(char const * clientEffectName);
```
- `__cdecl`, returns `playClientEffect`'s `bool`.
- Defined (32-bit only, same `#if !defined(_WIN64)` guard as the retrigger) in
  `clientGame/.../clientEffect/ClientEffectManager.cpp`.
- Exe-local forward declaration in `.../win32/utinni_clientEffect_forward.h` (included only by
  `engine_advertise.cpp`).

### Play-target choice
**ON the local player at its origin** — `Game::getPlayer()` →
`ClientEffectManager::playClientEffect(CrcLowerString(name), player, CrcLowerString::empty)`
(no hardpoint). Rationale: simplest, and targeting the player **skips the entire stealth/visibility
filter** in `playLabeledClientEffect` (it is gated on `creatureObject != Game::getPlayer()`), so a
preview can never be silently dropped. Bails `false` (no crash) if there is no player / no scene.

### Refresh semantics (honest about the boundary)
- **Transient (the targeted case):** a finished `.cef` has already released its `ClientEffectTemplate`
  → refcount 0 → `ClientEffectTemplateList::stopTracking` evicted it. So the play is a guaranteed
  **cache-MISS** → `fetch` reloads the `.cef` from disk → `load*()` re-fetches every referenced
  `.prt`/appearance and sound template via `AppearanceTemplateList::fetch` / `SoundTemplateList::fetch`
  → **edit visible.**
- **Still-cached (sustained):** there is **no clean public way** to force-reload it —
  `ClientEffectTemplate::load()` *appends* (never clears), and `stopTracking()` is private. A
  currently-sustained `.cef` being edited stays on Bucket B's `restart()` path / the editor's
  scene-change reload tier — **out of scope here** (same boundary as the static-snapshot exclusion).
- The `fetch()`+`release()` in the function is an **existence check + the `templateRefreshed` log
  signal** (and, for the uncached case, the reload trigger); the actual play does its own `fetch`.

### Instrumentation (file-backed; how the maintainer verifies without a debugger)
`REPORT_LOG`, prefix `[utinni.replay]`, into `stage/SwgClient_report.log` (the `[SharedLog]` sink):
- `BEGIN name="…" target=player(0x…) templateRefreshed=yes|no` — or `-> NO PLAYER (no scene)` on bail.
- `END name="…" played=yes|no`.

This commit **also** lands the sibling `[utinni.retrigger]` instrumentation in the same file (counts
`m_particleSystems`, logs each `ParticleEffectAppearance` candidate's `getAppearanceTemplateName()` +
match/restart tallies) — the diagnostic that motivated this wave.

---

## Build gate (passed)

- **Release/Win32**, `$env:MSBUILD`, `/nodeReuse:false`, exe deleted to force relink.
- **0** `unresolved external symbol`, **0** LNK1120/LNK1181, **0** `error C…`.
- Undecorated export confirmed: `GetEngineHookPoints = _GetEngineHookPoints`.
- X-macro count gate (`UTINNI_REQUIRED_COUNT` static_assert): **105 == 105** (table rows == `.inc` names).
- Staged `SwgClient_r.exe` (28,456,448 bytes, 2026-06-28 ~16:19). Advertise TU is **32-bit only**.

### LNK gotcha resolved (note for the next free-fn row)
First link failed `LNK2001` on `utinni_replayClientEffect`: the standalone definition's parameter was
`char const * const` → MSVC mangled the **top-level const** into the symbol (`…YA_N`**`Q`**`BD@Z`),
while the forward-header declaration is non-const (`…YA_N`**`P`**`BD@Z`). The retrigger dodges this
because it is a **friend** of `ClientEffectManager`, so its (non-const) declaration is visible in the
TU and reconciles the mangling. Fix: parameter is plain `char const *` (no top-level const) so the
definition matches the declaration. **Rule for future non-friend advertise free fns: keep the
definition's top-level param cv-qualifiers identical to the forward decl.**

---

## Re-sync (maintainer)

NAME ADD → re-copy these two **byte-identical (LF working-copy bytes — copy the working tree, not a
CRLF checkout)** into `D:/Code/Utinni/UtinniCore/swg/` and sha256-verify:

| file | sha256 (LF) |
|---|---|
| `engine_hookpoints.h`   | `9cc2a3455d860d473ccdcadc72dba6cc8ef06da292efdc7696bbe821a20d009e` |
| `engine_hookpoints.inc` | `e1330bab9e511c08312f6133c1db07fcb57fdc44816d752455622f4dd9a78aa3` |

(`engine_advertise.cpp` is provider-side only — not re-synced.) The new row is a constant
`(void*)&fn` — **no** `ensureDynamicRowsFilled` / `dyn[]` entry needed.

---

## Maintainer inject-smoke steps

1. Inject into the staged `SwgClient_r.exe`; confirm `utinni_init` still resolves the full table
   (now **105/105**) and `GetEngineHookPoints` reports version **8**.
2. Load a world (so `Game::getPlayer()` is non-null).
3. Open a `.cef` in the ClientEffect editor → edit (e.g. swap/scale a referenced particle) → **save**.
4. Trigger **Preview** (consumer marshals `replayClientEffect(name)` via
   `GameCallbacks.AddMainLoopCall`, game-thread, once).
5. **Expected:** the effect plays **fresh at the player** with the edit visible; and in
   `stage/SwgClient_report.log`:
   ```
   [utinni.replay] BEGIN name="appearance/…/foo.cef" target=player(0x…) templateRefreshed=yes
   [utinni.replay] END name="appearance/…/foo.cef" played=yes
   ```
   `played=no` + `templateRefreshed=no` ⇒ bad/again-cached name or missing file (check the `name`
   string format the editor sends vs. the `.cef` TreeFile path). `NO PLAYER` ⇒ no scene loaded.

> Live inject-smoke is **maintainer-side**. Provider claim is scoped to: contract populated (105/105),
> built + staged + exported, the function compiles/links and logs as specified. **Do not** touch
> `D:/Code/Utinni`.

---

## Out of scope (unchanged)

Walking **static world-snapshot `ParticleEffectAppearance`** instances (the Naboo fountain) — that is
a separate, larger scene-wide enumeration; those placements stay on the editor's scene-change reload
tier.

## Files touched (commit `33c2a7081`)

- `src/engine/client/library/clientGame/src/shared/clientEffect/ClientEffectManager.cpp` — `utinni_replayClientEffect` + `ClientEffectTemplate.h` include + the `[utinni.retrigger]` instrumentation.
- `src/game/client/application/SwgClient/src/win32/utinni_clientEffect_forward.h` — replay forward decl.
- `src/game/client/application/SwgClient/src/win32/engine_advertise.cpp` — `particlePreview::replayClientEffect` constant row.
- `src/game/client/application/SwgClient/src/shared/engine_hookpoints.h` — version 7→8 + note.
- `src/game/client/application/SwgClient/src/shared/engine_hookpoints.inc` — `+ENGINE_HOOKPOINT(particlePreview, replayClientEffect)`.
