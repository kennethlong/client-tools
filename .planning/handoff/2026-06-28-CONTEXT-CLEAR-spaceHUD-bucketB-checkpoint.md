# Session checkpoint — space-HUD fix + Utinni Bucket B + async sibling (2026-06-28 CONTEXT-CLEAR)

**Why this file:** clearing context. This is the authoritative "where we are now / what's next" snapshot
for the work done 2026-06-28. It ties together three threads and points at the detailed docs (it does
NOT duplicate them). Read this first, then the linked handoffs for whichever thread you pick up.

Everything below is **committed + pushed to `origin/master`** (clean tree, 0/0 vs origin).

---

## TL;DR — three threads, all landed or logged

1. **gl11 space-HUD "cyan square" — FIXED + committed.** The reticle/radar gauge backgrounds rendered
   as solid translucent cyan blocks in gl11 (thin arcs in gl05). Root: the additive `ui_shader_add.sht`
   (`SrcColor/One`) blends a STRAIGHT-alpha UI texture, so masked texels (alpha=0, RGB present) get
   added → solid fill. FIX: premultiply the generated PS output (`col.rgb *= col.a`) for additive
   (One-dest) SrcColor passes only (gated by a new PS-cbuffer `alphaTest.z` flag). Live-confirmed via a
   position-matched gl05-vs-gl11 A/B. Commit `44d54da26` (also carried the prior zone-in device-flag +
   IB-ComPtr fixes). Memory: `project_d3d11_additive_ui_premultiply_parity`.
2. **Utinni provider Bucket B (Effects editor live preview) — DONE + handback, awaiting maintainer
   smoke.** `ENGINE_HOOKPOINTS_VERSION` **6 → 7, 104 names**. Commits `db3ca5895` (code) + `a334cb2eb`
   (handback/STATE). See **[2026-06-26-effects-render-retrigger-HANDBACK.md](2026-06-26-effects-render-retrigger-HANDBACK.md)**.
3. **MeshAppearanceTemplate async-load heap corruption — LOGGED (not fixed).** A crash during Bucket B
   testing turned out to be a pre-existing async-load sibling, NOT a Bucket B regression. Logged as a
   todo + task #6. See `.planning/todos/pending/2026-06-28-meshappearance-async-load-heap-corruption-sibling.md`.

---

## Commit chain this session (newest first, all pushed)

| commit | what |
|---|---|
| `b73bb4765` | todo: log the MeshAppearanceTemplate async-load heap-corruption sibling of d1f92ab1f |
| `a334cb2eb` | docs: Bucket B HANDBACK + commit the outstanding-editor-unlock ledger + reopen STATE |
| `db3ca5895` | **feat(enginehook): Utinni Bucket B v7** — render rows + particle retrigger |
| `53529d30d` | chore(gitignore): ignore `stage-deferral-fix/` + `stage-revert-baseline/` (722 MB binary baselines) |
| `8e663685f` | docs(planning): snapshot accumulated `.planning/` + tracked `stage/override/` (1060 files) |
| `ce21ebd3b` | fix(config): correct `gameFeatures` 15→33297 + retire DPVS key in tracked cfg template |
| `44d54da26` | **fix(d3d11): land gl11 JTL space rendering** (HUD premultiply + zone-in stability); pushed w/ 3 prior |

---

## Build / run state (current)

- **Staged `SwgClient_r.exe`** = v7 (Release/Win32, built 2026-06-28 ~13:57), 0 unresolved, undecorated
  `GetEngineHookPoints` export confirmed. The advertise TU is **32-bit only**.
- **Staged `gl11_r.dll`** carries the space fixes (device flag `PREVENT_INTERNAL_THREADING_OPTIMIZATIONS`,
  `ms_currentIB` ComPtr UAF fix, the additive premultiply). `stage/client.cfg`: `rasterMajor=11`,
  `gameFeatures=33297`, BOM-clean.
- Build recipe unchanged (AGENTS.md): `$env:MSBUILD`, Release/Win32, `/nodeReuse:false`, delete the exe
  to force relink, grep log for `unresolved external symbol` (0), dumpbin for undecorated export.

---

## Utinni provider contract — where it stands

- **v7, 104 names.** Files: `src/game/client/application/SwgClient/src/shared/engine_hookpoints.{h,inc}`
  (shared contract) + `.../src/win32/engine_advertise.cpp` (table + thunks + `ensureDynamicRowsFilled` +
  export). New this wave: a friend free fn `utinni_retriggerClientEffect` in
  `clientGame/.../ClientEffectManager.cpp` + the exe-local `utinni_clientEffect_forward.h`.
- **v7 re-sync sha256 (LF working-copy bytes — copy the working tree, not a CRLF-normalized checkout):**
  - `engine_hookpoints.h`   `5f0e48acc711554eff7a55180ba083d35c2e8317d8076edd8edf0ee149a47ae9`
  - `engine_hookpoints.inc` `79c733976faadd2e565f6b029bed7634e93a3879b435e3f313afc035f193b558`
- **Discipline (unchanged):** NAME ADD → +1 version per wave + re-sync `.h`/`.inc` byte-identical into
  `D:/Code/Utinni/UtinniCore/swg/` + sha256-verify. Any new function-call row → `{name,0}` placeholder
  **AND** a `dyn[]` entry in `ensureDynamicRowsFilled` (else null on the consumer's pre-resume read);
  constant `(void*)&Symbol` rows are fine as-is. Do **not** touch `D:/Code/Utinni`. Live inject-smoke is
  maintainer-side — never claim an editor "works."

### OPEN — Utinni (priority order, one wave per maintainer smoke; buckets ship independently)

The live ledger of what's left is **[2026-06-26-utinni-provider-outstanding-editor-unlock.md](2026-06-26-utinni-provider-outstanding-editor-unlock.md)** (§4 priority). Per the §3 rule, partial waves are
safe in any order — you do NOT need the Bucket B smoke to start Bucket A.

1. **Maintainer-side (their court):** Bucket B live inject-smoke (load world → Effects editor →
   edit+save a `.prt` with live instances → "Preview in client" lights up + restarts). Re-sync to v7.
2. **Bucket A** (§2.A — per-editor real-entry detour rows): `cuiChatWindow::ctor` (chat), `debugCamera::alter`
   (free-cam), `cuiManager::findObjectUnderCursor` + `cuiHud::*` + `creatureObject::setTarget`
   (world-pick/HUD), `systemMessageManager::receiveMessage` + `messageQueue::appendMessage*` (sys-msg/input),
   `cuiRadialMenuManager::update`, `cuiLoginScreen::*`. Each: real-entry (`pmfRealEntry`/MI-thunk/friend
   accessor) or SKIP-if-virtual. The user flagged **chat + free-cam as highest-touch** — start there.
3. **C** (virtual vtable — consumer-side, provider thunk optional) → **D** (mid-function joint toggles:
   modal-chat setter, offline-scene flag, debug-cam passthrough) → **E** (WS-5 `Utinni_RegisterOnSceneReady`
   callback) → **F** (crash-log dir setter). All in the ledger §2.C–F.

**Playbook that worked for Bucket B (reuse it):** parallel `general-purpose` agents to map each
subsystem's real symbols + classify (static `&fn` / non-virtual single-inheritance `bit_cast` PMF /
MI real-entry / virtual SKIP / ctor funnel-or-omit), then advertise the advertisable, OMIT/SKIP the rest
with documented reasons + offered alternatives ("a wrong & is worse than a missing row"), bump version,
build-gate, write a HANDBACK with sha256s + maintainer smoke steps.

---

## Open backlog (tasks; not Utinni)

- **#6 — MeshAppearanceTemplate async-load heap corruption** (this session's crash). Pre-existing sibling
  of `d1f92ab1f` (which hardened the SkeletalMeshGenerator/`clientSkeletalAnimation` path; this is the
  `MeshAppearanceTemplate`/`clientObject` cousin → `StaticShaderTemplate` texture load → free-list `"SSHT"`
  stomp). Band-aid: retry. Real fix: reuse `tools/setup/zonein-loop.ps1` + `EmptyStandbyList` cold-cache +
  full page-heap to trap the write, then harden the mesh-appearance async callback. Todo +
  `project_intermittent_tatooine_crash_087a` memory updated.
- **#1 — Option A: gl05↔gl11 dual-renderer state-dump diff harness** (proactive parity net — would have
  caught the premultiply bug). **#2 — Option B: static D3D9→D3D11 state-surface parity audit** (cheap
  seed for A). Both are user-approved future work; design in the task descriptions.

---

## Gotchas re-confirmed this session (don't relearn)

- **Dirty-repo commits:** stage explicit paths, never `git add -A` blind — this repo has 722 MB of binary
  scratch dirs (now gitignored) + ~200 cross-workstream WIP files. (`feedback_gate_closeout_commits_on_dirty_repo`)
- **Mixed line endings:** several engine `.cpp` are CRLF; the contract `.{h,inc}` are LF. Multi-line Edit
  matches fail on CRLF — check EOL first (cat -A) or use a scoped Python rewrite.
- **`utinni_verifyNoNullNoDup()` runs at boot** (`ClientMain.cpp:285`) even without Utinni — so a bad
  `dyn[]` PMF would crash at boot, not world-load. (It passed; the world-load crash is the async sibling.)
- **Friend grants access, not scope:** a friend free function must still qualify members
  (`ClientEffectManager::m_particleSystems`), not name them bare.
