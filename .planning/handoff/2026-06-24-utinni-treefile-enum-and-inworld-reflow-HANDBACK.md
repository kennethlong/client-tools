# Provider Handback — TreeFile enumeration + in-world CUI reflow (swg-client-v2 → Utinni)

**From:** swg-client-v2 (provider) · **To:** Utinni (consumer) · **Date:** 2026-06-24
**Re:** `2026-06-24-utinni-treefile-enum-and-inworld-reflow.md`. Both asks DONE + build-gated.
**Contract version:** `ENGINE_HOOKPOINTS_VERSION` **4 → 5** (Ask A is one NAME ADD; Ask B is behavior-only).
**Not committed yet** (awaiting maintainer). Did NOT touch `D:/Code/Utinni`.

---

## 0. TL;DR

- **Ask A** — added `TreeFile::enumerateFiles(cb, ctx)` (walks `ms_searchNodes`, yields every
  SearchTree/SearchTOC filename as a **real string** via callback) + advertised `treeFile::enumerateFiles`.
  Version 4→5, 98 names.
- **Ask B** — **provider-detected (Option P)**. Behavior-only fix: the in-world CUI reflow now keys off the
  **back-buffer** size (`Graphics::getFrameBufferMaxWidth/Height`) instead of the *current render-target*
  size, so it tracks back down to the embed panel after the fullscreen restyle. No contract change.
- **Staged `SwgClient_r.exe` is now contract v5 (98 names)** — see §3 re: your ~40/95 resolution note.

**Re-sync** (byte-identical into `D:/Code/Utinni/UtinniCore/swg/`, sha256-verify):
```
engine_hookpoints.h    sha256 3343b0cd76580a50d68ab52dab2129ff1d1b13e95f99bf0e213de27ea8b3e3e6
engine_hookpoints.inc  sha256 8a001fd2079216c2831ec19adc56029c105fc6cf9458dabebcefee86fabe05e3
```

---

## 1. Ask A — `treeFile::enumerateFiles` (NAME ADD)

### Signature & semantics (matches your requested shape exactly)
```cpp
// TreeFile.h (public static). __cdecl, no pThis (TreeFile is all-static).
static void TreeFile::enumerateFiles(void (*callback)(const char *fileName, void *context), void *context);
```
- Invokes `callback` **once per filename** across **all registered `SearchTree` + `SearchTOC` nodes** — the
  union the client can `open()`. Filenames are **real engine-relative strings** (e.g. `"terrain/tatooine.trn"`),
  read from each node's TOC name block (`m_fileNames + entry.fileNameOffset`). Confirmed recoverable (not
  CRC-only): the CRC is just the binary-search key; the plaintext name table is live in memory.
- `SearchPath` (loose dir), `SearchAbsolute`, `SearchCache` are **intentionally skipped** — they carry no flat
  TOC name table (a loose dir would need a live filesystem walk). The TRE/TOC union is what the editors need.
- **Order/dedup:** unspecified (priority order of `ms_searchNodes`; a filename present in multiple trees is
  yielded multiple times). You group + dedup your side, as planned.
- **No engine-side container** is built — `cb` is invoked directly during the walk, per your preference.
- **⚠️ Re-entrancy:** the callback runs **while `TreeFile::ms_criticalSection` is held** (the walk is locked,
  matching the `debugReportPaths` idiom, to protect the `ms_searchNodes` traversal). Your callback **must not
  re-enter `TreeFile`** (no `open`/`exists`/`addSearchTree`/… from inside `cb`). Your Repository append is
  allocation-only → safe. If you ever need to call back into TreeFile, snapshot the names in `cb` and process
  after `enumerateFiles` returns.

### Utinni-side typedef (your doc, confirmed correct)
```cpp
void(__cdecl*)(void(__cdecl* cb)(const char*, void*), void* ctx)
```

### Implementation (provider side, for your review)
- `TreeFile::enumerateFiles` (static driver, `TreeFile.cpp`) locks `ms_criticalSection`, iterates
  `ms_searchNodes`, `dynamic_cast`s each node to `SearchTree`/`SearchTOC` (the established `getSearchPath`
  RTTI idiom), and calls a per-node `enumerateFiles` member.
- `SearchTree::enumerateFiles` / `SearchTOC::enumerateFiles` (members in `TreeFile_SearchNode.cpp`, because the
  name arrays are private): loop `[0, m_numberOfFiles)`, `cb(m_fileNames + m_tableOfContents[i].fileNameOffset, ctx)`.
  (Null-guarded; `m_numberOfFiles` is `int` on SearchTree, `uint32` on SearchTOC.)
- Advertised row: `{ "treeFile::enumerateFiles", (void *)&TreeFile::enumerateFiles }`.

---

## 2. Ask B — in-world reflow: **PROVIDER-DETECTED (Option P)**

**Decision: provider-detected.** No contract change. (Consumer-driven escape hatch confirmed available — see §2b.)

### Root cause (refined from the request's framing)
The RNDR-04 CUI reflow in `CuiManager::render` keyed off `Graphics::getCurrentRenderTargetWidth/Height`. That
getter returns **whatever render target is currently bound** — and it bounces to a **fullscreen-sized scene /
post-fx texture RT** mid-frame. On world entry the client does the window-level fullscreen restyle; your
watchdog resizes the window back to the panel, but the scene RT stays fullscreen, so the CUI latched the
fullscreen res → minimap/toolbar/chat clipped.

### The fix (behavior-only, ~2 lines + comment, exe-side)
`CuiManager::render`'s size-compare now reads **`Graphics::getFrameBufferMaxWidth/Height`** — the **back-buffer**
size, which is set *only* in `Graphics::resize` and which your embed-resize poll (`Graphics::beginScene`, gl11)
already keeps synced to `GetClientRect(window)` every frame. So after the watchdog re-sizes the window back to
the panel, the back-buffer (and now the CUI) track the panel. It **equals** the current-RT dims in normal
play/standalone, so startup/maximize and the D3D9 path are unaffected (no-op when nothing changed).

> Note: this is cleaner than putting `GetClientRect(Os::getWindow())` directly in `CuiManager.cpp` (a
> cross-platform `shared/` TU) — `getFrameBufferMaxWidth/Height` is the already-public back-buffer accessor and
> needs no new Win32 dependency. Same authoritative source as your poll, one indirection removed.

The `beginScene` poll (which keeps `ms_frameBufferMax` == window client rect) is unchanged and runs **before**
`CuiManager::render` each frame, so the back-buffer is already corrected to the panel when the CUI reads it.

### 2b. Consumer-driven escape hatch — confirmed available
If provider-detected ever proves insufficient through some restyle path, both rows you'd drive from the watchdog
are advertised with **valid, real-function addresses**:
- `cuiManager::setSize → &CuiManager::setSize` (static, re-lays-out the UI root: `SetSize` + `ForcePackChildren`).
- `graphics::resize → &Graphics::resize` (static).
You can call `setSize(panelW, panelH)` (+ `resize`) on every re-assert. But with Option P you shouldn't need to —
and driving both from two sources of truth was the thing to avoid.

---

## 3. Re: your "~40 of 95 names resolve" heads-up

The staged `SwgClient_r.exe` is now **contract version 5, 98 names** (built 2026-06-24 22:29, this batch). If
your smoke saw ~40/95, it was running against a **pre-full-catalog** staged exe. With this restage the full v5
set is live. If the full set still doesn't resolve after re-injecting against this exe, that's a real bug to
chase (the table is dynamically initialized — a couple of rows use function-call initializers like
`pmfRealEntry`/`utinni_chatWindowCreateNewWindowEntry`, all resolved post-startup before you read the table) —
flag it and I'll trace. `utinni_verifyNoNullNoDup()` is green (98 rows == 98 `.inc` names, no nulls, no dups).

---

## 4. Build gate + files changed

- **Release/Win32**, canonical 5-target, `/nodeReuse:false`, forced relink. Exit 0; **0** `unresolved external
  symbol`; **0** errors. `dumpbin /exports` → undecorated **`GetEngineHookPoints`** present. Restaged with
  matching PDB (exe+pdb 22:29). Contract self-check: 98 table rows == 98 `.inc` names, no dups/nulls.
- Live inject-smoke on `SwgClient_r.exe` is the maintainer's step (Ask A: editor scene/object lists populate;
  Ask B: in-world minimap/toolbar/chat no longer clipped).

**Files (provider side):**
- `TreeFile.h` / `TreeFile.cpp` — `enumerateFiles` static + driver.
- `TreeFile_SearchNode.h` / `TreeFile_SearchNode.cpp` — per-node `SearchTree`/`SearchTOC::enumerateFiles`.
- `CuiManager.cpp` — reflow keys off `getFrameBufferMax*` (Ask B).
- `engine_advertise.cpp` — `treeFile::enumerateFiles` row.
- `engine_hookpoints.inc` / `engine_hookpoints.h` — +1 name, version 4→5.
