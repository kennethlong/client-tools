# Session checkpoint — Utinni Bucket B-2 + Bucket A/A-2/A-3 + 2 crash diagnoses (2026-06-29 CONTEXT-CLEAR)

**Why this file:** clearing context. Authoritative "where we are now / what's next" snapshot for the
2026-06-28 → 06-29 Utinni provider work + crash hunts. Read this first, then the linked per-wave
handbacks for whichever thread you pick up. Everything below is **committed + pushed to
`origin/master`** (clean tree, 0/0 vs origin; one untracked `stage/override/.../ksk_all_spaceterminal.dds`
is the user's, intentionally left).

---

## TL;DR

- **Utinni provider contract is at `ENGINE_HOOKPOINTS_VERSION` = 12, 113 names.** Staged
  `SwgClient_r.exe` (Release/Win32, 2026-06-28 ~20:56) carries it; `GetEngineHookPoints` exports; advertise
  TU is 32-bit-only. All waves pushed.
- **THE one open blocker is consumer-side (maintainer's court):** rebuild `UtinniCore.dll` against **v12**
  so `hkSetTarget` binds the advertised `network::getObjectById` instead of a stale hardcoded RVA. That is
  the *only* thing gating the target-change editor.
- **Spaceport zone-in crash = PARKED** (rare intermittent, the `"SSHT"`/StaticShaderTemplate heap-corruption
  flake family; NOT the contract). Logged under task #6.

---

## Commit chain this session (newest first, all pushed)

| commit | what |
|---|---|
| `f78df545a` | todo: spaceport zone-in crash "Sighting 2" parked under the async StaticShaderTemplate todo |
| `88d54b7a5` | docs: A-3 `network::getObjectById` handback + README |
| `901398013` | **feat: Utinni A-3 v12** — `network::getObjectById` (unblocks target-change) |
| `e6d66495c` | docs: A-2.1 systemMessageManager crashfix handback + README |
| `1693f8099` | **fix: Utinni A-2.1 v11** — REVERT `systemMessageManager::receiveMessage` (world-load crash) |
| `08ab5709c` | docs: Bucket A-2 v10 world-pick handback + README |
| `cc1933001` | **feat: Utinni A-2 v10** — world-pick (`cuiHud::getTarget` + `cuiHud::g_instance`) |
| `a53a52fc8` | docs: Bucket A v9 handback + README |
| `ef073dca2` | **feat: Utinni Bucket A v9** — 6 per-editor real-entry rows (ledger §2.A) |
| `bbea5890b` | docs: Bucket B-2 v8 handback + README |
| `33c2a7081` | **feat: Utinni Bucket B-2 v8** — live `.cef` RE-PLAY (`particlePreview::replayClientEffect`) + the `[utinni.retrigger]` instrumentation |

Per-wave detail is in the matching `.planning/handoff/2026-06-28-*HANDBACK.md` files (all indexed in the README).

---

## Contract — where it stands (v12 / 113 names)

Files: `src/game/client/application/SwgClient/src/shared/engine_hookpoints.{h,inc}` (shared) +
`.../win32/engine_advertise.cpp` (table + thunks + `ensureDynamicRowsFilled` + export).

**v12 re-sync sha256 (LF working-copy bytes — copy the working tree, not a CRLF checkout):**
- `engine_hookpoints.h`   `61586631d0883f380004a370c3e8b7dd4aada31625338a01b956c060eb90699c`
- `engine_hookpoints.inc` `c68d55c72652e6fddcc7acd58994fe5355ba9809012f5f04530ab4e7981dcc83`

### Net new endpoints this session
- **B-2:** `particlePreview::replayClientEffect` → free fn `utinni_replayClientEffect` (re-plays a `.cef`
  fresh on `Game::getPlayer()`; the transient muzzle/hit case `restart()` can't cover). Maintainer-confirmed.
- **A (6):** `cuiRadialMenuManager::update`, `cuiMenu::infoTypesFindDefaultCursor` (namespace free fn),
  `creatureObject::setTarget`→`CreatureObject::setLookAtTarget` (MI real-entry via a `CreatureObject.cpp`
  accessor — header too heavy for the exe TU), `messageQueue::appendMessage` + `appendMessageData` (the two
  overloads). `systemMessageManager::receiveMessage` was here too but was REVERTED (see A-2.1).
- **A-2 (2):** `cuiHud::getTarget` → `__fastcall` thunk over `SwgCuiHud::getLastSelectedObject()` (MI, CALLED)
  + `cuiHud::g_instance` → `SwgCuiHudFactory::findMediatorForCurrentHud` (live ground/space hud accessor).
- **A-2.1 (−1, revert):** `systemMessageManager::receiveMessage` REMOVED — it was mapped to the static
  `receiveSystemMessage`, but the consumer's `hkReceiveMessage` targets the 2-arg
  `MessageDispatch::Receiver::receiveMessage(emitter,message)`; the wrong-& crashed world-load. Real receiver
  is the file-local `Listener::receiveMessage` → un-advertisable → OMIT.
- **A-3 (1):** `network::getObjectById` → static `NetworkIdManager::getObjectById(const NetworkId&)`
  (the id→Object* resolver `hkSetTarget` needs).

### OMIT/SKIP ledger (in `engine_advertise.cpp`, don't re-attempt)
- OMIT ctors `cuiChatWindow::ctor` (funnel `createNewWindow` already advertised) + `cuiLoginScreen::ctor`
  (no funnel, un-addressable). OMIT absent `cuiManager::findObjectUnderCursor` (world-pick value comes from
  the A-2 `cuiHud` rows instead).
- SKIP virtual (consumer vtable-resolves): `cuiHud::actionPerformAction`, `cuiHud::update`,
  `cuiLoginScreen::activate`, `debugCamera::alter` (`FreeCamera::alter`, virtual at every level).

### Discipline (unchanged)
NAME ADD/REMOVE → +1 version per wave + re-sync `.h`/`.inc` byte-identical into `D:/Code/Utinni/UtinniCore/swg/`
+ sha256-verify. Constant `(void*)&sym`/`&thunk` rows need NO `dyn[]`; any runtime-CALL-computed row needs the
`{name,0}` placeholder **and** a `dyn[]` entry. 32-bit-only TU; Release/Win32, build **serially** (gl05/gl07
postbuild `d3dcompiler_47.dll` copy races under `/m`), delete the exe to relink, grep 0 unresolved, dumpbin
the undecorated export, X-macro count `static_assert`. **Do NOT touch `D:/Code/Utinni`.** Live inject-smoke is
maintainer-side.

---

## OPEN — priority order

1. **Maintainer-side (their court): rebuild `UtinniCore.dll` on v12.** Re-sync the v12 `.h`/`.inc` (sha256s
   above) AND rebuild the consumer so `hkSetTarget` calls `network::getObjectById` (resolves to
   `0x018f7070`) instead of the stale hardcoded RVA. This fixes the **target-change crash** (dump `135512`,
   below). Until then, selecting a target in-world crashes (`SwgCuiHud::update`→`setLookAtTarget` detour
   fires every time the reticle picks a target). Smoke: re-inject (113/113, v12), select a target → `onTarget`
   gets a non-null `Object*`, no crash.
2. **Still flagged for typedef-verify during smoke** (MISMATCH/detour rows): `creatureObject::setTarget`
   (→`setLookAtTarget` — the mapping is correct; alts `setIntendedTarget`/`setLookAtAndIntendedTarget` if the
   consumer wanted a different target), `messageQueue::appendMessage[Data]` (INPUT-path detours). If any fires
   a wrong-signature detour → symbolize the `.mdmp`, OMIT if mismatched (the A-2.1 playbook).
3. **Next Utinni waves available** (ledger `2026-06-26-utinni-provider-outstanding-editor-unlock.md` §2.C–F):
   virtual vtable rows (consumer-side; provider thunk optional), mid-function cooperative toggles
   (modal-chat setter already shipped as `config::setModalChat`; offline-scene flag; debug-cam passthrough),
   `Utinni_RegisterOnSceneReady` callback, crash-log-dir setter (low value).

---

## Crash hunts this session

### A) Target-change crash — ROOT-CAUSED, fixed by A-3 + needs the v12 consumer rebuild
Dump `stage/SwgClient_r.exe-unknown.0-20260629135512.mdmp` symbolized to: `SwgCuiHud::update`
(line 1435 `player->setLookAtTarget(...)`) → consumer `hkSetTarget` detour → `call eax` where `eax` =
`0x00b30160` (mid-`xmlParseChunk`, garbage). My advertised symbols are correct and uninvolved
(`getObjectById`=`0x018f7070`, `setLookAtTarget`=`0x01298c60`); the consumer was calling a **stale hardcoded
id-resolver RVA** because `UtinniCore.dll` predates v12. **Provider fix shipped (A-3 v12); consumer rebuild
pending** (item #1).

### B) Tatooine spaceport zone-in crash — PARKED (rare intermittent, not the contract)
Dump `stage/SwgClient_r.exe-unknown.0-20260629152448.txt` (**.txt only — no mdmp**): primary exception
`e06d7363` (C++ throw) ~1s into loading `ilm_extract/shader/spaceport_*_cs8_tato.sht`
(`SwitchTextureShaderTemplate`/`StaticShaderTemplate` path). The `0xC0000005 @ +0xC850B4` was a **nested AV
inside the crash handler** (`MyUnhandledExceptionFilter+0x2e4`) while writing the breadcrumb → truncated
`.txt`, no `.mdmp`. Almost certainly the same `"SSHT"` StaticShaderTemplate free-list corruption as task #6,
C++-throw flavor. **Could NOT reproduce: 2× clean under cdb + 20+ scene changes under Utinni.** Parked under
`.planning/todos/pending/2026-06-28-meshappearance-async-load-heap-corruption-sibling.md` (Sighting 2).

---

## Diagnostics gotchas re-confirmed this session (don't relearn)

- **Utinni will NOT inject while a debugger is attached** (`IsDebuggerPresent`/`CheckRemoteDebuggerPresent`).
  So cdb-attach and Utinni-injected are mutually exclusive — you can't pre-attach cdb to catch a
  Utinni-only crash. Use the **app's own crash handler mdmp** (passive capture: it writes a full
  `SwgClient_r.exe-unknown.0-*.mdmp` on most crashes — that's how `135512` was symbolized), or light
  page-heap (a process-image flag, not a debugger → Utinni still injects) to force a heap-corruption flake.
- **The SWG crash-`.txt` breadcrumb is NOT the stack** — it lists last-loaded assets and the handler frames.
  The truth is the `.mdmp`: `cdb.exe -z <mdmp> -y stage -lines -c ".ecxr; kb 80; q"`. cdb (x86) is at
  `C:\Program Files (x86)\Windows Kits\10\Debuggers\x86\cdb.exe`; PDBs are in `stage/`.
- **`REPORT_LOG` visibility:** `stage/client.cfg` has a `[SharedLog]` block (`logReportLogs=1` +
  `logTarget0=file:SwgClient_report.log`) routing `REPORT_LOG` to `stage/SwgClient_report.log`. The
  `[utinni.retrigger]` / `[utinni.replay]` diagnostic markers land there. (`logReportLogs` defaults to false.)
- **Build serially.** `/m` races gl05/gl07 postbuild `d3dcompiler_47.dll` copies → `MSB3073`.
- Memories added this session: `project_utinni_advertise_freefn_toplevel_const_mangling` (LNK2001 top-level
  const param), `project_utinni_receivemessage_wrong_amp_crash` (receiveMessage = Receiver virtual, not a static).
