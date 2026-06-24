# EngineHook rename — provider-side handback (swg-client-v2)

**Date:** 2026-06-23 · **Status:** DONE on the provider side, build-gated. **Not committed** (awaiting maintainer's usual stage-explicit close-out).

Paired with the consumer request in Utinni HEAD `005787e` ("queue provider request for the paired
EngineHook rename"). This is the provider half: drop the `Utinni` branding from the shared
hookpoint contract so any app can advertise/resolve a client this way.

## What changed (source-cosmetic only — binary contract UNCHANGED)

**File renames (`git mv`):**
- `…/SwgClient/src/shared/utinni_engine_hookpoints.h`   → `engine_hookpoints.h`
- `…/SwgClient/src/shared/utinni_engine_hookpoints.inc` → `engine_hookpoints.inc`
- `…/SwgClient/src/win32/utinni_advertise.cpp`          → `engine_advertise.cpp`

**Token map applied (whole-token):**
- `UtinniEngineHookPoint`  → `EngineHookPoint`   (covers `…Points`)
- `UTINNI_HOOKPOINT`       → `ENGINE_HOOKPOINT`  (covers `UTINNI_HOOKPOINTS_VERSION` → `ENGINE_HOOKPOINTS_VERSION`)
- `UtinniDx11HookPoints`   → `EngineDx11HookPoints`   (Direct3d11.cpp local struct; layout unchanged)
- `utinni_engine_hookpoints` → `engine_hookpoints`    (filenames, `#include`s, header guard `INCLUDED_engine_hookpoints_H`)
- `utinni_advertise.cpp`   → `engine_advertise.cpp`   (doc-comment references to the renamed TU, in
  `GroundScene.cpp`, `Os.cpp`, `ClientMain.cpp/.h`, `utinni_clientShims_forward.h`,
  `utinni_groundScene_forward.h`, and the `.h/.inc/.cpp` self-references)

**Project file:** `SwgClient.vcxproj` `ClCompile`/`ClInclude` entries repointed to the new filenames
(BOM + CRLF preserved). No `.filters` file exists for this project.

**Kept as-is (intentional):**
- Export names `GetEngineHookPoints` (exe) and `GetHookPoints` (gl11) — unchanged.
- `ENGINE_HOOKPOINTS_VERSION` = **3** — **no version bump** (pure rename; no name add/remove, no
  address move).
- Non-contract, exe-local names: the `utinni_*` thunk/forwarder functions
  (`utinni_loadOverrideConfig`, `utinni_groundScene*`, `utinni_osWindowProc`,
  `utinni_installConfigFileOverride`, `utinni_verifyNoNullNoDup`, `utinni_strEq`, …), the
  `utinni_clientShims_forward.h` / `utinni_groundScene_forward.h` headers, and the local enum
  `UTINNI_REQUIRED_COUNT`. These are provider-internal, not part of the shared contract — left
  untouched per the "keep your non-contract names" instruction.

## Build gate — PASS (Release / Win32)

- Canonical 5-target build (`Direct3d11;Direct3d9;Direct3d9_ffp;Direct3d9_vsps;SwgClient`),
  `/nodeReuse:false`, forced relink (deleted `SwgClient_r.exe` first). Exit 0.
- **0** `unresolved external symbol` in the build log (checked — `/FORCE` would have masked them).
- `dumpbin /exports`:
  - `stage/SwgClient_r.exe` → `GetEngineHookPoints` (undecorated) ✓
  - `stage/gl11_r.dll`      → `GetHookPoints` (undecorated) + `GetApi` ✓
- Restaged with matching PDBs (exe+pdb 23:21, gl11 dll+pdb 23:20).
- Compile-time table self-check (`s_engineHookPoints` count == `.inc` required-set count) compiled
  clean → 95 rows, version 3, unchanged.

## ⚠️ Heads-up for the maintainer: the `.h/.inc` are NOT byte-identical to Utinni HEAD — by design

The request said to copy Utinni HEAD's `engine_hookpoints.h/.inc` (`304b5a8`) **verbatim** because the
doc-comment was "already genericized." **That is not the actual state of the Utinni tree.** Ground
truth from `D:/Code/Utinni` (read-only — we did not touch it):

- `304b5a8` ("refactor(harness): genericize … (EngineHook)") is a **pure `git mv`** —
  `2 files changed, 0 insertions(+), 0 deletions(-)`. It renamed the files but left the **content
  unchanged**.
- At Utinni HEAD, `UtinniCore/swg/engine_hookpoints.{h,inc}` **still carry the old tokens**
  (`UtinniEngineHookPoint`, `UTINNI_HOOKPOINT`, `UTINNI_HOOKPOINTS_VERSION`, the
  `INCLUDED_utinni_engine_hookpoints_H` guard), and the consumer
  (`endpoints.cpp/.h`, `endpoints_bindings.cpp`) still references `UtinniEngineHookPoints` /
  `UTINNI_HOOKPOINT`.

So copying Utinni's current files verbatim would have **retained** the `UTINNI_` branding — the
opposite of the rename. **Our stance: we don't think the `UTINNI_` tokens should be retained.** We
applied the full token genericization on our (canonical, source-of-truth) side instead. This means:

- Right now the two repos' `.h/.inc` **diverge** (ours = `ENGINE_*`, Utinni's = `UTINNI_*`). The
  **binary contract is identical** (same POD layout `{version,count,entries}` / `{name,addr}`, same
  `GetEngineHookPoints` export), so **nothing breaks at runtime** — your renamed consumer keeps
  working against the current staged `SwgClient_r.exe` exactly as noted.
- To restore byte-identical lockstep, **re-copy our `engine_hookpoints.h/.inc` into
  `UtinniCore/swg/`** and apply the same token map to the Utinni-side consumer
  (`endpoints*.{h,cpp}`). The exact, reproducible transform set we used (apply verbatim to get the
  same bytes):
  ```
  s/utinni_engine_hookpoints/engine_hookpoints/g
  s/UtinniEngineHookPoint/EngineHookPoint/g     # covers …Points
  s/UTINNI_HOOKPOINT/ENGINE_HOOKPOINT/g         # covers UTINNI_HOOKPOINTS_VERSION
  s/utinni_advertise\.cpp/engine_advertise.cpp/g   # doc-comment TU ref only
  ```
  (LF line endings; the prose "Utinni" words in the doc-comments are intentionally left — the map is
  identifier-scoped, not a blanket de-brand.)

## Notes / constraints honored

- Did **not** write to `D:/Code/Utinni` (read-only inspection only).
- No version bump.
- Not committed — staged set is the 3 renamed files + `Direct3d11.cpp` + `SwgClient.vcxproj` +
  comment-only touches in `GroundScene.cpp` / `Os.cpp` / `ClientMain.cpp/.h` /
  `utinni_*_forward.h`. Stage explicit paths at close-out.
