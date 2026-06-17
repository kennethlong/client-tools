# 33-01 Task 2 : x64 import-lib provenance

x64 MSVC import `.lib`s generated from the **SWG Restoration x64 DLL exports**
(`D:\SWG Restoration\x64\`), for the three LIFT third-party deps that have no
in-tree x64 source build. Each was produced by:

```
dumpbin /exports <Restoration-dll>            -> <name>.exports.txt
# parse the NAME column -> EXPORTS .def
lib /def:<name>.def /machine:x64 /out:<dest>  -> <name>-x64.lib
dumpbin /headers <lib> | findstr machine      -> 8664 (x64)
```

Generated 2026-06-17 with the VS18 BuildTools x64 toolchain
(`...\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\{dumpbin,lib}.exe`).

| Dep | Source DLL (Restoration x64) | Generated import lib | Exports | dumpbin machine |
|-----|------------------------------|----------------------|---------|-----------------|
| libxml2 | `libxml2.dll` | `src/external/3rd/library/libxml2-2.6.7.win32/lib/libxml2-x64.lib` | 1675 | **8664 (x64)** |
| pcre    | `pcre.dll`    | `src/external/3rd/library/pcre/4.1/win32/lib/pcre-x64.lib` | 30 | **8664 (x64)** |
| jpeg    | `jpeg62.dll`  | `src/external/3rd/library/libjpeg/lib/jpeg62-x64.lib` | 107 | **8664 (x64)** |

Filenames are DISTINCT from the shipped x86 inputs (`libxml2-win32-debug.lib` /
`libxml2-win32-release.lib`, `libpcre.a`, `libjpeg.lib`) so the 32-bit link is
untouched. Plan 05 owns the x64 `AdditionalDependencies` name swap.

## pcre export-name spot-check (reviews fix #7b, Sonnet HIGH) : PASS

The live 32-bit link uses the MinGW `libpcre.a` (undecorated `pcre_*`). The risk
was that Restoration's MSVC `pcre.dll` might export DECORATED names, yielding a
machine-x64 lib whose symbols don't resolve -> LNK2019 only at Plan 05's full
link. **Result: NO mismatch.** Restoration's `pcre.dll` exports the
engine-referenced symbols UNDECORATED:

Engine call sites (`grep -rho "pcre_[a-z_]*" src/engine/shared/library/sharedRegex/`):
`pcre_free`, `pcre_malloc` (the function-pointer globals wired in
`SetupSharedRegex.cpp`); the broader engine also references `pcre_compile`,
`pcre_exec`.

All four resolve against `pcre.dll`'s undecorated exports (see
`pcre.exports.txt`: ord 3 `pcre_compile`, ord 9 `pcre_exec`, ord 10 `pcre_free`,
ord 24 `pcre_malloc`). The generated `pcre-x64.lib` therefore links clean for
Plan 05; the MinGW-`.a`-vs-MSVC-DLL decoration trap does not bite this codebase.

## Provenance files

- `<name>.exports.txt` : raw `dumpbin /exports` of the Restoration DLL
- `<name>.def`         : the `EXPORTS` def derived from the NAME column (lib input)

The intermediate `.exp` side-products from `lib /def` were discarded (not link inputs).
