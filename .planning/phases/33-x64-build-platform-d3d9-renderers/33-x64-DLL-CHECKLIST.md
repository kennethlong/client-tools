# 33-x64 Staged-DLL Checklist (X64-02 supply-chain provenance)

Every x64 DLL that must sit in `stage-x64/` (next to the x64 `SwgClient_*.exe`)
**before the first x64 boot**, with provenance + an x64-machine verification
command per the Security Domain **V14** supply-chain control (threat T-33-01:
third-party prebuilt binaries from a non-first-party source enter the client's
load path).

Verify each DLL is genuinely x64 with:

```
dumpbin /headers <dll> | findstr machine    ->  must read  8664 machine (x64)
```

(`dumpbin.exe` ships beside `$env:MSBUILD` under
`...\VC\Tools\MSVC\<ver>\bin\Hostx64\x64\`.)

## Staged x64 DLLs

| DLL | Provenance | Size (bytes) | SHA256 | dumpbin machine |
|-----|------------|--------------|--------|-----------------|
| `gl05_d.dll` | **built from source** (gl05 / Direct3d9, Plan 05) | — | — | 8664 (post-build) |
| `gl06_d.dll` | **built from source** (gl06 / Direct3d9_ffp, Plan 05) | — | — | 8664 (post-build) |
| `gl07_d.dll` | **built from source** (gl07 / Direct3d9_vsps, Plan 05) | — | — | 8664 (post-build) |
| `SwgClient_d.exe` | **built from source** (Plan 05) | — | — | 8664 (post-build) |
| `dpvs.dll` | **built from source** (dpvs.vcxproj x64, Plan 04). Restoration `D:\SWG Restoration\x64\dpvs.dll` (424448 B) is the fallback if the from-source x64 build is deferred. | — | — | 8664 (post-build) |
| `binkw64.dll` | **LIFTED** from `D:\SWG Restoration\x64\binkw64.dll` (Bink uses a bare `LoadLibrary` name; no x64 source) | 282624 | `036201ec4226f788c6acf2f2e7ca6a9f87987db129aa4eb687a4fe9a66af18a4` | 8664 (x64) |
| `libxml2.dll` | **LIFTED** from `D:\SWG Restoration\x64\libxml2.dll` (paired with the generated `libxml2-x64.lib` import lib, 33-01 Task 2) | 1315128 | `d72d231ee867fdfef88813fe7a5e7ae3bbee8d73e47d75b6dc60801e7e207357` | 8664 (x64) |
| `pcre.dll` | **LIFTED** from `D:\SWG Restoration\x64\pcre.dll` (paired with the generated `pcre-x64.lib`; export-name spot-check PASS, 33-01 Task 2) | 147456 | `58e002361c4c8c14678311848cbe3c9ccb51f232c7d4084c355fa8f07b906e30` | 8664 (x64) |
| `jpeg62.dll` | **LIFTED** from `D:\SWG Restoration\x64\jpeg62.dll` (paired with the generated `jpeg62-x64.lib`) | 604160 | `6cfefdec532fc1c607dab268e8731d45a1b91b95d8863cf9fd7793a7e87f3f81` | 8664 (x64) |
| `d3dcompiler_47.dll` (x64) | **SYSTEM/SDK** x64 copy. Verified present at `C:\Windows\System32\d3dcompiler_47.dll` (4669440 B, SHA256 `2a8bc296c24647e0f5b6bb269e1cd2a0d77256a219ba9fafc6688e968532fc3b`, machine **8664**). The shipped 32-bit tree's master postbuild already copies a `d3dcompiler_47` (Phase 32) — the x64 boot needs the x64 copy. | 4669440 | `2a8bc296c24647e0f5b6bb269e1cd2a0d77256a219ba9fafc6688e968532fc3b` | 8664 (x64) |

> **WoW64 redirector note (reviews fix #7a):** a **32-bit** MSBuild/PowerShell
> copying `d3dcompiler_47.dll` from `C:\Windows\System32` is silently redirected
> by the WoW64 file-system redirector to `C:\Windows\SysWOW64` (the **x86** copy).
> To obtain the genuine x64 DLL from a 32-bit context, copy from
> `C:\Windows\Sysnative\d3dcompiler_47.dll` (the Sysnative alias bypasses the
> redirector), **or** run the copy from a native-x64 shell. Always
> `dumpbin /headers <copied dll> | findstr machine` -> `8664` to confirm. (The
> SHA256 above was captured from a 64-bit Git-Bash shell, where `System32`
> resolves to the real x64 copy.)

## Deferred / NOT staged this phase

- **`mss64.dll` (Miles x64 audio)** is **deferred to Phase 35** (AUDIO-01/02).
  The engine is on Miles 7.2e and the only x64 Miles is 9.x (9.3b SDK at
  `D:/Code/milesss-v9.3b/`); the port is real code work, not a relink. The x64
  client boots **audio-degraded** without it, which is acceptable for X64-02
  (boot-to-character-select). Do **NOT** lift Restoration's `mss64.dll` this phase
  (it is the 9.x API; the client calls the 7.2e API until Phase 35 ports the call
  sites).

## X64-04 N/A dispositions (icu / discord-rpc)

X64-04's wording mentions resolving **icu** and **discord-rpc** x64 libs. They
ship in Restoration's `x64/` directory (`icudt62.dll`, `libicuuc.dll`,
`discord-rpc.dll`) because **Restoration added those features** — they are **NOT
dependencies of THIS client**. Recorded **satisfied-by-N/A** with exact-token /
path / vendor evidence (reviews fix #10c — deliberately NOT the false-positive
`grep -rl "icu|discord"`, which matches ordinary words like "particular"):

| Check | Command | Result |
|-------|---------|--------|
| (a) include-directive | `grep -rIn -e '#include.*icu' -e 'discord' -e 'discord-rpc' src/` | **0** — no client source includes/references either |
| (b) vendored-source | `ls src/external/3rd/library/ \| grep -iE '^(icu\|discord)'` | **0** — neither is vendored in-tree |
| (c) link-input | `grep -riE 'icu[a-z0-9]*\.lib\|discord[-_]?rpc' src/build src/game src/engine --include=*.vcxproj` | **0** — no `.vcxproj` links them |

User-confirmed **2026-06-17**. **No sourcing or build work** is required for icu
or discord-rpc; X64-04 is provably satisfied-by-N/A for those two. Do not stage
`icudt62.dll` / `libicuuc.dll` / `discord-rpc.dll` for this client.

## Provenance recording (V14)

The LIFTED DLLs (binkw64 / libxml2 / pcre / jpeg62) are the same vetted binaries
the Restoration client ships. Where a from-source x64 build exists it is
preferred (zlib / dpvs / ui / udplibrary build from source in Plan 04; the gl0X
renderers + SwgClient in Plan 05) — the lifts are limited to deps with no
practical x64 source build. Re-verify the SHA256 + `dumpbin machine` of each
lifted DLL whenever `stage-x64/` is repopulated.
