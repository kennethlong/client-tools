# TODO — MeshAppearanceTemplate async-load heap corruption (sibling of d1f92ab1f)

**Status:** OPEN · known-sibling follow-up · band-aid in place (retry usually clears it)
**Filed:** 2026-06-28 · **Severity:** intermittent c0000005 on world/zone-in (cold cache), heap corruption
**Related:** memory `project_intermittent_tatooine_crash_087a`; the FIXED skeletal sibling `d1f92ab1f`.

## What

Intermittent `c0000005` on load-into-world, surfacing in the allocator as a **free-list node whose
link pointer was overwritten with `0x54485353` = ASCII `"SSHT"`** (the `StaticShaderTemplate` IFF FORM
tag). I.e. shader-template IFF data landed in already-freed memory; the next `new` faceplants.

Minidump stack (`stage/SwgClient_r.exe-unknown.0-20260628194822.{txt,mdmp}`):

```
MemoryManagerNamespace::removeFromFreeList+0xe      <- crash (block link == "SSHT")
MemoryManager::allocate(size=8) / operator new
StaticShaderTemplate::load_texture_0001             [StaticShaderTemplate.cpp:764]
StaticShaderTemplate::load_0000 / ::load / ::create
ShaderTemplateList::fetch(Iff)
ShaderPrimitiveSetTemplate::load_sps_0001           [ShaderPrimitiveSetTemplate.cpp:1551]
MeshAppearanceTemplate::load_0005                   [MeshAppearanceTemplate.cpp:402]
MeshAppearanceTemplate::asynchronousLoadCallback    <- THE async loader (clientObject)
AsynchronousLoader::processCallbacks
Game::runGameLoopOnce  <- (UtinniCore!hkMainLoop on stack -- Utinni was injected)
```

## Why this is NOT "already fixed"

`d1f92ab1f` (2026-06-19) hardened the **`SkeletalMeshGenerator`** async path
(`clientSkeletalAnimation`) — the armor_marauder-belt `.mgn` race. **This crash is its cousin in a
different loader:** `MeshAppearanceTemplate::asynchronousLoadCallback` (`clientObject`) → mesh `.msh`
→ `ShaderPrimitiveSetTemplate` → `StaticShaderTemplate` texture. Same bug *class* (async-load UAF/
overrun, cold-cache), parallel code path, untouched by that commit. So "we thought we nailed it" was
half-right — we nailed the skeletal one.

Aggravator this instance: **Utinni injected** (`hkMainLoop` detour adds load/timing pressure). Worth
an A/B (inject vs not) to confirm whether the consumer widens the window or it's purely engine-side.

## How to kill it (playbook already exists from d1f92ab1f)

1. **Repro harness (reuse):** `tools/setup/zonein-loop.ps1` + `tools/setup/EmptyStandbyList.exe`
   (cold-cache forcing) + the catch scripts `tools/setup/zonein-crash-catch.cdb` /
   `zonein-loop-catch.cdb`.
2. **Catch the corrupting write:** full page-heap (`gflags /p /enable SwgClient_r.exe /full`) so it
   faults *at the write* that stamps `"SSHT"` past a buffer / into a freed block — culprit on the
   stack instead of the victim (the technique that cold-nailed the gl11 IB UAF this session).
3. **Harden** `MeshAppearanceTemplate::asynchronousLoadCallback` / `load_0005` + the
   `ShaderPrimitiveSetTemplate`/`StaticShaderTemplate` fetch under it with the same pattern
   d1f92ab1f applied to the skeletal path (detach-queue-before-iterate, fetch/release bookends,
   release-safe deref guards, no erase-during-iteration, no stale-pointer reuse across a reentrant
   create()).

Full skeletal-fix context + accepted limitations: `.planning/handoff/gl11-x64-tatooine-zonein-crash.md`.

---

## Sighting 2 — Tatooine spaceport zone-in, C++-throw flavor (2026-06-29)

Another intermittent zone-in crash, almost certainly the SAME root (StaticShaderTemplate /
`"SSHT"` corruption), surfacing as a **C++ throw** rather than the free-list AV:

- Dump `stage/SwgClient_r.exe-unknown.0-20260629152448.txt` (**`.txt` only — no `.mdmp`**): primary
  exception `e06d7363` (MSVC `_CxxThrowException`) thrown ~1s into loading a Tatooine spaceport,
  while pulling `ilm_extract/shader/spaceport_*_cs8_tato.sht` + `intr_tech_*_cs8_tato.sht`
  (`SwitchTextureShaderTemplate` / `StaticShaderTemplate` load path).
- The `0xC0000005 READ 0x4A2550A5 @ +0xC850B4` reported alongside it was a **nested AV inside the
  crash handler** (`SetupSharedFoundationNamespace::MyUnhandledExceptionFilter+0x2e4`) while it was
  writing the breadcrumb report — which is why the `.txt` is truncated (no footer) and **no `.mdmp`
  was produced**. So the handler itself faults on a bad pointer when it crashes mid-shader-load.

### Repro attempts (2026-06-29) — could NOT reproduce
- 2× clean zone-ins under **cdb** (no Utinni — and **Utinni refuses to inject while a debugger is
  attached**: `IsDebuggerPresent`/`CheckRemoteDebuggerPresent`, so cdb-attach and Utinni-injected are
  mutually exclusive; can't catch it with a pre-attached debugger).
- **20+ scene changes under Utinni, no debugger → no crash.**
- Conclusion: rare intermittent (the documented "retry usually succeeds" flake), NOT a deterministic
  regression, and NOT the Utinni contract waves (A/A-2/A-3, B/B-2 are sound). PARKED 2026-06-29.

### Decision / plan
- **Passive capture:** when it next crashes naturally, the client's own handler usually writes a full
  `mdmp` (e.g. the clean `135512.mdmp`) — symbolize that. If it again produces `.txt`-only, the handler
  nested-AV is itself worth hardening (it dereferences a bad asset-name pointer while building the report).
- **Active trap (when prioritized):** light page-heap on `SwgClient_r.exe` (a process-image flag, NOT a
  debugger → Utinni still injects) + `EmptyStandbyList` cold cache + a zonein loop, to force the
  StaticShaderTemplate corruption deterministic and trap the corrupter. Light (not full) to avoid the
  32-bit 2 GB OOM.
