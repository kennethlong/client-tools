# 2026-07-03 session — zone-in heap corruption FIXED (3 threading fixes) + Utinni sysmsg v15 (utf8 shim + ABI audit)

**Status: BOTH ARCS DONE, committed + pushed. `master` = `bd653dccf`.**
Chain this session: `d33f34e2d` (v14 sysmsg row, 07-02 evening) → `f344d1035` (threading fixes) →
`bd653dccf` (v15 rev-2 shim). Staged Win32 Release exe (09:30) carries everything; all gl0X_r
plugins rebuilt in the same lineage (Shader.h shared-header cascade honored).

---

## Arc 1 — Tatooine zone-in crash → heap corruption from unguarded cross-thread containers (`f344d1035`)

**Crash (07:39, Win32 Release gl11, ~30s in):** c0000005 on the ClientTerrain worker in
`std::map<Tag,Material>::find` ← `StaticShader::getMaterial` ← `setupMaterial` ←
`ShaderCache::createBlendedShader` ← `ClientChunk::create` ← `threadRoutine`. The live
template-owned MaterialMap was overwritten with **loader path-string text** ("…ader file…" bytes as
the map head pointer). A **nested AV in the crash handler** (strlen on a garbage
`CrashReportInformation::getEntry`, `SetupSharedFoundation.cpp:133`) killed the process before the
mdmp write — hence stage/ had only a truncated `.txt`.

**Forensics (new technique, memorized as `crash-dump-forensics-wer-localdumps`):** our handler wrote
no mdmp, but **WER LocalDumps** had `AppData\Local\CrashDumps\SwgClient_r.exe.<PID>.dmp` (PID from
Event Log 1000, which also gives the ASLR-immune fault-offset RVA). The crash-`.txt` `=addr` is a raw
**ASLR** VA — never symbolize against 0x400000 (base was 0x4b0000 this run). Original exception
context recovered off the faulting thread's stack (`s -d <esp> … <origEip>` → CONTEXT at hit−0xB8 →
`.cxr` → full stack).

**Root cause (CONSULT-56 — all four crew + own audit, files in `.planning/research/CONSULT-56-*`,
untracked; the codex `.out` is 1.3MB, do NOT commit):** a stale/racing `delete` on an unguarded
cross-thread container poisons the MemoryManager freelist → a live object's block is handed out
again → a `"<type> file: <path>"` breadcrumb string lands on top of it. Same defect class as the
TreeFile::SearchCache fix (`9c03f53c5`). Three same-shape fixes landed:

1. **`ShaderCache::nodeList` lock** (`ClientProceduralTerrainAppearance_ShaderCache.{h,cpp}`) —
   bare ArrayList mutated by terrain thread (findCachedShader/add) vs main (alter + sync LOD
   createChunk fallback); ArrayList::add reallocs (new[]/copy/delete[]). `Mutex.h` was already
   included but no lock member ever added — an abandoned fix, now completed:
   `mutable RecursiveMutex m_nodeListLock` + `Guard` in all 5 nodeList methods (recursive because
   createBlendedShader re-enters via findCachedShader). Lock order: always precedes
   ShaderTemplateList's CS — no inversion.
2. **`CrashReportInformation` rewrite** (`sharedFoundation/.../CrashReportInformation.cpp`) — the
   two bare std::vectors were written by EVERY loading thread (FileStreamer logs a breadcrumb per
   loose-file open — hot with our ilm_extract/override dirs). Now fixed-capacity never-reallocated
   tables (1024 static / 256 dynamic), mutex-serialized writers, **lock-free null-checked
   `getEntry`** → the crash-handler walk is memory-safe by construction and future crashes WILL
   write their mdmp. (This answered "do we still need handler hardening?" — no separate change
   needed.)
3. **`Shader::m_users` interlocked** (`clientGraphics/.../Shader.{h,cpp}`) — plain-int
   `++/--/delete this` raced between terrain (fetchModifiable clones) and main (render
   PrimitiveNode fetch/release, alter). Now `mutable volatile long` + `_Interlocked*`; fetch()
   moved out-of-line. Layout-identical (int→long) but Shader.h is plugin-consumed → all plugins
   rebuilt.

**Ruled out (don't re-investigate):** orderly refcount UAF of family shaders (preloaded BEFORE the
terrain thread exists; released only after thread join); **`ShaderCache::destroyShader` is DEAD
CODE** (zero callers → node referenceCount never drops → alter()'s release branch never fires —
blended shaders slow-leak, retail-era, separate cleanup item); OOB indexing (bounded by
construction); **MemoryBlockManager** — its `shared` ctor flag only pools allocators;
`allocate`/`free` take the global CS **unconditionally** — do NOT "fix" that flag.

**Verified:** clean 5-target Win32 Release (0 unresolved), Kenny zoned into Tatooine clean.
**Watch:** crash was intermittent — confidence accrues per zone-in. A **regression would present as
a zone-in HANG** (lock misuse), not a crash. If a crash recurs, the handler now writes a proper
mdmp — no WER scavenging needed.

**Follow-up audit backlog (same class, unfixed, low priority):**
- `ShaderEffect` / `ShaderImplementation` / `Video` have the identical plain-int `m_users` pattern —
  check cross-thread exposure before touching.
- `TreeFile::open` iterates `ms_searchNodes` unlocked cross-thread (Sonnet flag — same shape as the
  fixed SearchCache).
- Wire `destroyShader` from chunk/ShaderSet teardown so alter()'s 5s timeout works (fixes the
  blended-shader leak).
- Sonnet also noted `Direct3d11_StaticShaderData::construct()` is a second terrain-thread
  `getMaterial` reader — fine now that the corruptors are locked; relevant only if new map races appear.

## Arc 2 — Utinni sysmsg send: v14 crashed live smoke → v15 extern-C utf8 shim (`bd653dccf`)

**v14 post-mortem (consumer's error, not ours):** the direct-`&fn` row passed
`const Unicode::String&` across the advertised boundary. Consumer `swg::WString` models the 2002
SWGEmu 3-pointer layout (12B); our v145 `Unicode::String` is an SSO basic_string (24B) → engine read
`_Mysize` past the consumer object → WRITE-AV.

**v15 delivered (rev-2 request → `.planning/handoff/2026-07-03-utinni-sysmsg-send-rev2.md`):**
- NAME REPLACE (not re-point): `systemMessageManager::sendMessage` REMOVED,
  `systemMessageManager::sendMessageUtf8` ADDED → `extern "C" void __cdecl
  utinni_sendFakeSystemMessage(const char* utf8Msg, bool chatBoxOnly)` in `engine_advertise.cpp` —
  null-checked, widens via `Unicode::narrowToWide` (per-byte; ASCII editor text per rev-2), calls
  `sendFakeSystemMessage`. Game-thread-only contract.
- `ENGINE_HOOKPOINTS_VERSION` **14→15**, still **120 names**; `.h`/`.inc` re-copied into
  `D:/Code/Utinni/UtinniCore/swg/`, sha256-verified: `.inc` `6c612aff…`, `.h` `8f9e15d6…`.
- **ABI RULE now canon (both repos):** only primitives and pointers cross the advertised boundary on
  CALLED endpoints; class-type params/returns need an extern-C shim. DETOURED rows unaffected.

**Companion ABI audit** (flags baked into the row comments in `engine_advertise.cpp`; shims on
request): `cuiChatWindow::writeToAllTabs` + `writeToCurrentTab` (Unicode::String& — their §3,
consumer-blocked, latent), **`consoleHelper::sendInput` (Unicode::String& — NEW audit find, NOT in
their §3 — flag to the maintainer: hard-block consumer-side like the chat writes)**,
`worldSnapshot::addObject` (CrcString const& + std::string const&),
`objectTemplate::get{AppearanceFilename,PortalLayoutFilename,ClientDataFile}` (RETURN our
std::string by const&). Checked safe: NetworkId/Vector/Transform/CmdInfo rows (POD/layout-stable).

**OPEN (consumer side, nothing pending ours):** maintainer rebinds on v15 + live smoke — typed
broadcast text appears in chat feed; `.trn` save shows "[Utinni] saved … — terrain reloaded".
SWGEmu D3D9 smoke unchanged (send path there untouched).

## Loose ends
- `.planning/research/CONSULT-56-*` untracked by design (1.3MB codex .out); the small `.md`
  evidence/task files can ride a future commit if wanted.
- Memories written this session: `project_terrain_zonein_heap_corruption_unguarded_containers`,
  `reference_crash_dump_forensics_wer_localdumps`; MEMORY.md compacted 19.8→11.8KB (no entries lost).
