# Session handoff — Utinni integration + teardown/texture crash fixes (2026-06-23)

For the next session (a new Utinni-project task). Repo `D:/Code/swg-client-v2`, branch `master`.
Build/run/cfg conventions: `AGENTS.md`. Machine-specific (codex/cursor consult CLIs): `CLAUDE.md`.

## TL;DR state
- **Utinni `GetEngineHookPoints` contract: VERSION 3, 94 endpoints** (`utinni_engine_hookpoints.{h,inc}`).
  Phase 37 (MVP) + Phase 38 (full coverage) COMPLETE + maintainer-smoke-approved. The 4 detoured endpoints
  advertise REAL engine entries (38-05). Provider stays Utinni-agnostic.
- **This session also fixed 3 distinct teardown crashes + 1 texture crash** found in real minidumps. The
  ROOT teardown fix (38-09) is confirmed working (clean shutdown, no crash).

## CRITICAL operating rules (also in memory)
- **NEVER write to `D:/Code/Utinni`.** The consumer dev manages his own repo and pulls our committed
  contract himself. Writing there broke his build once this session. Hand off via committed `swg-client-v2`
  HEAD + a handback doc stating version + name count. (memory `project_utinni_contract_sync_volatile`)
- **After GSD plan/execute, verify `.planning/ROADMAP.md` wasn't truncated** (`wc -l` ~180; `grep -c '^| [0-9]'`).
  The planner truncated it once this session. (memory `project_gsd_planner_truncates_roadmap`)
- Build: `$env:MSBUILD` (VS18/v145), PowerShell, `/nodeReuse:false`, serial; grep log for 0
  `unresolved external symbol` (/FORCE masks them); native MSBuild, no tests — validate by build+dumpbin+boot.

## GIT STATE — 5 unpushed commits on master (behind: 0)
```
1cfef979b fix(38-09): CuiMediator::garbageCollect re-entrancy guard — ROOT of the shutdown UAF
799660f88 fix(38-08): guard SwgCuiHudWindowManagerGround::handlePerformDeactivate m_callback null-deref
9f706d321 docs(38-07): SUMMARY + handback for Tatooine texture-crash guard
a43c43f9a fix(38-07): Release-safe DDS validation + gl11 desc-reject for Tatooine texture-create crash
73f14bb3d docs(handoff): embed-resize root cause (client scene viewport/RT) + world-load crash symbolized  [Utinni-dev commit]
```
NOT yet pushed to origin. (Maintainer's call whether to push.) Large pre-existing untracked WIP in
`.planning/` (CONSULT-* research) is unrelated — do NOT sweep it into commits; stage exact paths.

## Crash-fix status (this session, all gap-closures under Phase 38)
1. **38-09 — THE ROOT teardown fix (CONFIRMED working).** `CuiMediator::garbageCollect` got a re-entrancy
   guard (`static s_collecting`) + erase-before-deactivate + restart-on-mutation. Root (measured from a Debug
   `FATAL CuiWorkspace.cpp:335 "Can't removeMediator … during m_iteratingEnabledStates"`): a re-entrant
   `garbageCollect` (outer GC → `CuiWorkspace::updateMediatorEnabledStates` → `SwgCuiAllTargets::removeUnusedStatusPages`
   → nested GC → `removeMediator` mid-iteration) → Release use-after-free (`eip=0` null-vtable call). Post-fix:
   shutdown clean, NO new crash dumps after the 13:34 build. Crew-reviewed (4 AIs, `CONSULT-48-*`).
   Supersedes the per-site guards 38-06 (`m_objectCallbackVector`) + 38-08 (`m_callback`) which remain as
   harmless defense-in-depth.
   - OPTIONAL confirm: a fresh **Debug** shutdown should NOT hit the `CuiWorkspace:335` FATAL anymore.
2. **38-07 — Tatooine async texture-create crash (staged, AWAITING a live trip).** NVIDIA driver null in
   `nvwgf2um` during `CreateTexture2D` from an async `MeshAppearanceTemplate`→`StaticShaderTemplate` load.
   Guard: `Texture::load` made Release-safe (malformed/short DDS or insane dims → fall back to default texture
   + WARNING naming the asset); `Direct3d11_TextureData` rejects+names an invalid desc. Crew-reviewed
   (`CONSULT-47-*`). NOT yet confirmed firing — keep doing **cold Tatooine zone-ins with Utinni injected**
   (`stage/` Win32, since Utinni is 32-bit only); the WARNING names the culprit if it trips. The prior
   `d1f92ab1f` ".mgn hardening" was on a DISJOINT path and is unrelated to this crash.
3. Forensics + verdicts: `.planning/research/crash-20260623-nvwgf2um-FORENSICS.md`,
   `CONSULT-47-SYNTHESIS.md`, `CONSULT-48-SYNTHESIS.md`, `CONSULT-48-EVIDENCE-mediator-uaf.md`.

## Utinni integration — key docs the next task likely needs
- `.planning/handoff/2026-06-22-utinni-provider-coverage-handback.md` — Phase-38 provider handback (94 eps,
  EPA-08 DX11 evidence that the provider is clean, the consumer worklist).
- `.planning/handoff/2026-06-23-utinni-detour-entries-fixed.md` — 38-05 real-entry fix for the 4 detoured eps.
- `.planning/handoff/2026-06-23-utinni-provider-followup-2.md` — wndProc→WM_SIZE→ResizeBuffers path CONFIRMED
  on our side (the embed-resize fix is the consumer binding `client::wndProc`, already advertised v2).
- `73f14bb3d` handoff (just committed by the Utinni dev): **embed-resize root cause (client scene viewport/RT)
  + world-load crash symbolized** — READ THIS FIRST for the new task; it's the most recent Utinni input.
- Contract source of truth: `src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.{h,inc}`
  + `src/.../win32/utinni_advertise.cpp`. Out-of-scope (consumer-side, per the handback): virtual vtable
  resolution, the `tryInstall` device+context poll guard, dropping `installable()` skips, mid-function patches.

## Phase/ROADMAP state
Phases 37 + 38 = Complete in ROADMAP (post-v3.0 standalone phases). 38 has gap-closures 38-05..38-09 (commits;
ROADMAP/STATE reflect them). No active GSD phase pending.
