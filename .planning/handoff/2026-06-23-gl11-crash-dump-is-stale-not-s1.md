# gl11 startup-crash dump triage — VERDICT: stale June-2 dump, NOT a §1 resize regression

**Date:** 2026-06-23
**Status:** HANDBACK — forensic triage of `stage\release-gl11-crash.dmp`.
**Audience:** the maintainer (live-smoke owner) + Utinni dev.
**Source of truth:** this repo. Did NOT write to `D:/Code/Utinni`. No code/contract change made.

## TL;DR

`stage\release-gl11-crash.dmp` is a **June 2** crash of the **June 2** `gl11_r.dll` — it predates the
§1 resize change (June 23) by three weeks. **My §1 code is not in the crashing binary, so this dump
cannot be a §1 regression.** Nothing to fix in §1. The likely real issue: the maintainer's runtime
loaded a **stale gl11_r.dll** (or an old dump was grabbed by mistake). Re-smoke the *actual* §1 binary
and capture a fresh dump — it will symbolize cleanly with the already-staged matching PDB.

## Evidence (build-independent, from the dump itself + dumpbin)

| | PE timestamp | PDB GUID | PDB age |
|---|---|---|---|
| **Dump's `gl11_r.dll`** (read from dump memory) | `6A1F5583` = **Jun 2 17:13** | `{3F037E9B-F5A2-4AD1-B433-5FA447D4A16A}` | **0xa = 10** |
| **My §1 `gl11_r.dll`** (stage + compile, both) | `6A3AFC1F` = **Jun 23 16:35** | `{3F037E9B-…-7D4A16A}` (same lineage) | **131** |

- **Dump file mtime:** `2026-06-02 19:01:51` (matches the maintainer's "19:01" — but the DATE is June 2,
  not June 23).
- Same PDB GUID but **age 10 vs 131** = the same `gl11_r.pdb` advanced by ~121 incremental links
  between June 2 and June 23. The dump needs the **age-10** PDB state, which incremental linking
  overwrote in place long ago → **this dump is now unsymbolizable by anyone** (my age-131 PDB, the
  maintainer's stage PDB — all "mismatched pdb" against age-10). cdb confirmed: `gl11_r` and
  `SwgClient_r` both fell back to `export symbols` (every function name printed by cdb/the dev is
  nearest-export garbage, including the "GetApi+…", "ShaderImplementationPassPixelShaderProgram…",
  "StaticIndexBufferGraphicsData…" names — ignore them).

## The crash signature (build-independent — for whoever chases the June-2 bug; likely already fixed)

Fault: `gl11_r+0x28f60  mov edi,[eax+0x44]` with `eax=1` → read of `0x45` → AV. Surrounding disasm:

```
mov  eax,[ecx+10h]      ; eax = container->begin   (= 1, garbage)
mov  ecx,[ecx+14h]      ; ecx = container->end
cmp  eax,ecx            ; while (begin != end)
je   <skip-if-empty>
mov  edi,[eax+44h]      ; edi = element->[0x44]     <-- AV: begin==1
test edi,edi
```

This is a **container/vector iteration** (`begin`=[obj+0x10], `end`=[obj+0x14]) reading
`element->[0x44]`, where the container's `begin` pointer is a garbage `1` (a bool/flag value where a
pointer is expected). It is in **June-2 gl11 code**, reached from gl11+0x27611 ← an exe frame. Without
the age-10 PDB it can't be named. **It is NOT the §1 resize path** — §1's only loops iterate
`ms_deviceLost/RestoredCallbacks` by index and *call* the entries (no `element->[0x44]` read), and §1
isn't in this binary anyway. This may already be fixed in the 3 weeks since (e.g. a since-resolved
texture/teardown crash); treat as historical unless it reproduces on a current build.

## Why §1 is not implicated

1. The crashing binary is June 2; §1 landed June 23. The bytes aren't there.
2. The fault signature (container-iterate, `element->[0x44]`, `begin==1`) does not match any §1 code
   (`Graphics::displayModeChanged`→`Graphics::resize`, `resize_impl`, `resizeBackBuffer`).
3. §1 built clean Release+Debug and only touches the device-lost/restored fan-out + backbuffer resize.

## PDB drift — diagnosis + fix

- **Not a staging bug for the current build:** `stage\gl11_r.dll` (age 131) and `stage\gl11_r.pdb`
  (16:35, same link) **match** — a fresh crash on the *current* §1 binary symbolizes cleanly. Same for
  `SwgClient_r.exe`/`.pdb`. The postbuild already stages the matching PDBs.
- **The drift is incremental linking:** Release links are incremental (the age went 10→131), so the
  PDB GUID stays constant while the age bumps and the PDB is mutated in place. Any dump older than the
  current build can no longer be symbolized once a later incremental link overwrites its age.
- **Recommended hygiene (maintainer's call — build-speed tradeoff):** set
  `<LinkIncremental>false</LinkIncremental>` for `Release|Win32` on `Direct3d11` + `SwgClient` so each
  build gets a **unique PDB GUID** (no age aliasing; a stale PDB cleanly fails to match by GUID rather
  than "almost matching"). Pair with the existing PDB staging (already in place) and, ideally, archive
  the `gl11_r.pdb`/`SwgClient_r.pdb` next to each dump you keep. I did **not** apply this unilaterally —
  it slows every Release link team-wide; say the word and I'll set it + rebuild.

## Action items for the maintainer

1. **Confirm the runtime is loading the June-23 build, not a stale gl11.** This dump proves a **June-2**
   `gl11_r.dll` was injected. At runtime, verify the loaded `gl11_r.dll` is `6A3AFC1F` / Jun 23 16:35
   (e.g. check the module timestamp, or `stat stage\gl11_r.dll`). If Utinni/your launcher is pulling a
   `gl11_r.dll` from somewhere other than the fresh `stage\`, point it at the 16:35 build.
2. **Re-smoke the actual §1 binary** (`stage\SwgClient_r.exe` 16:35 + `stage\gl11_r.dll` 16:35, age
   131). If it crashes, capture a **fresh** dump — keep the matching `stage\gl11_r.pdb` +
   `SwgClient_r.pdb` with it. I'll symbolize it instantly (my compile-dir PDBs match this build).
3. **Retire the stale dump** to avoid re-confusion: `stage\release-gl11-crash.dmp` is the June-2
   artifact (I left it untouched — it's yours). Rename/delete it.
4. If the embed-resize crop is *also* still present in the re-smoke, that's another sign the runtime is
   on stale code (§1 fixes the crop) — check #1 first.

## What I did NOT do

- No §1 revert / no code change (no regression evidenced).
- No rebuild (current staged binaries already match their PDBs).
- No write to `D:/Code/Utinni`. No contract change (still version 3 / 94 endpoints).
