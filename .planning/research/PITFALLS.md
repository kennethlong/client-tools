# Pitfalls Research

**Domain:** v2.3 Hardening — web-based SWG TRE compare tool + in-client hardening fixes (DPVS config-gate, instrumentation removal, machine portability, re-entrancy guard, D3DXCompileShader→D3DCompile port)
**Researched:** 2026-06-12
**Confidence:** HIGH for the TRE-format and client-hardening pitfalls (derived from the in-tree authoritative parser source + the existing handoff/memory records); MEDIUM for the cross-distribution TRE-variant claims (community-distribution variance is real but under-documented publicly); HIGH for the D3DCompile API-difference pitfalls (MS docs + in-tree D3D9 source).

> **Authoritative reference for the TRE tool:** the engine's own parser is the spec. Read these before writing any TRE reader:
> - `src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.h` — `SearchTree::Header` / `SearchTree::TableOfContentsEntry` / `SearchTOC::Header` structs (the on-disk layout)
> - `src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.cpp` — the actual read/decompress/binary-search logic
> - `src/engine/shared/library/sharedFile/src/shared/TreeFile.cpp` — `fixUpFileName` (path normalization), `searchNodePriorityOrder` (precedence), `installSearches` (cfg key parsing)
> - `D:/Code/swg-tools` swg-blender tre extract/repack — a second, independently-written reader to cross-check against.

## Critical Pitfalls

### Pitfall 1: Hardcoding the TRE version and rejecting valid archives (or accepting garbage)

**What goes wrong:**
The reader assumes one TRE layout and either FATALs on a legitimate archive from a different SWG distribution, or silently mis-parses one whose header it doesn't actually understand.

**Why it happens:**
The SWG `TREE` format is versioned in the header's second field (`Tag version`). The in-tree client validates `header.version < TAG_0004 || header.version > TAG_0004` in `SearchTree::validate` (only `'0004'`) **but the constructor accepts both `TAG_0004` and `TAG_0005`** (`TreeFile_SearchNode.cpp:278-279`) and FATALs on anything else. There is also a completely separate `TOC` container format (`TAG3(T,O,C)`, version `TAG_0001`) with a *different* header struct and a *different* `TableOfContentsEntry` layout (`SearchTOC::Header` / `SearchTOC::TableOfContentsEntry`). Community distributions (SWGEmu, SWGSource, retail NGE, pre-CU) were repacked by different tools over ~20 years and ship a mix of `0004`/`0005` TREEs and occasional `.toc`+TRE-set bundles. A reader that hardcodes one struct silently reads the wrong fields.

**The two on-disk layouts (from the in-tree structs — these ARE the spec):**
- **TREE v0004/v0005 `TableOfContentsEntry`** (24 bytes, all 32-bit): `crc, length, offset, compressor, compressedLength, fileNameOffset`. Header has `tocOffset, tocCompressor, sizeOfTOC, blockCompressor, sizeOfNameBlock, uncompSizeOfNameBlock`.
- **TOC v0001 `TableOfContentsEntry`** (different!): `uint8 compressor, uint8 unused, uint16 treeFileIndex, uint32 crc, uint32 fileNameOffset, uint32 offset, uint32 length, uint32 compressedLength`. AND in the `.toc` the on-disk `fileNameOffset` field actually stores a **name length**, post-processed into a running offset after load (`TreeFile_SearchNode.cpp:687-699`). Getting this wrong shifts every filename.

**How to avoid:**
Branch on `(token, version)` exactly like the engine: dispatch `TREE/0004`, `TREE/0005`, `TOC/0001` to distinct parsers; on an unknown `(token,version)` surface a clean "unsupported TRE variant vX in <file>" diagnostic, never a crash and never a best-effort guess. Validate `numberOfFiles`, `sizeOfTOC`, and name-block sizes against the actual file length before allocating.

**Warning signs:**
Filenames come out shifted/garbled by a constant; CRCs don't match the names; one distribution's archives parse and another's produce nonsense; an allocation size read from the header is implausibly large (multi-GB) — that's a wrong-struct read, not a huge archive.

**Phase to address:** TRE-tool foundation phase (format reader / model layer), before any UI.

---

### Pitfall 2: Getting search-path precedence backwards in the merged virtual tree

**What goes wrong:**
The merged-virtual-tree diff resolves each logical file to the *wrong* physical archive, so "added/removed/changed" verdicts are inverted or attributed to the wrong TRE. The whole tool's core value (a trustworthy diff) is then quietly wrong.

**Why it happens:**
SWG's override semantics are non-obvious and easy to invert. The engine's rule (`TreeFile.cpp`):
- Search nodes are ordered by `searchNodePriorityOrder`: **`a->getPriority() > b->getPriority()`** — i.e. **higher priority number is searched FIRST** (it's at the front of the list).
- The config keys are `searchTree<priority>`, `searchPath<priority>`, `searchTOC<priority>` (optionally with a `_NN_` SKU infix); `installSearches` walks `priority` 0..`maxSearchPriority` (default 20). Within the same priority, **same-priority nodes are inserted AFTER existing matches** (`std::lower_bound` + the "insert after last priority match" comment) — so among equal priorities, *config/parse order wins ties*, first-listed resolves first.
- `TreeFile::open` walks the list front-to-back and returns the **first** node that has the (non-deleted) file. So the *winner* is the highest-priority-number node, then earliest-listed at that priority.

A naive implementation that sorts ascending, or that treats "later searchTree entry overrides earlier," or that ignores the priority number entirely and just uses archive filename order, will resolve backwards.

**How to avoid:**
Model resolution as: stable-sort nodes by `(priority DESC, parseOrder ASC)`, then first-match-wins, exactly mirroring `addSearchNode` + `open`. Unit-test against a known retail `client.cfg`'s actual `searchTree*`/`searchPath*` ordering and confirm the tool's "winner" for a file matches what `TreeFileExtractor`/the client actually loads.

**Warning signs:**
Patch/override TREs (which are *supposed* to win) show as "shadowed"; the base data TRE shows as the live source for files a patch clearly replaces; flipping two archives in the input changes nothing (means you're ignoring precedence) or flips everything (means you inverted it).

**Phase to address:** TRE-tool merged-virtual-tree phase.

---

### Pitfall 3: Reading the wrong searchPath set — inventing your own instead of parsing `client.cfg`

**What goes wrong:**
The tool globs `*.tre` from the install directory in alphabetical/filesystem order and calls that the search set. The real client only loads the archives named by `searchTree*`/`searchPath*`/`searchTOC*` keys in `[SharedFile]`, in the priority order above — and SKU-gated keys (`searchTree_00_0`, `_01_`, …) are conditionally included based on account SKU bits. A directory glob includes archives the client never loads and omits the priority semantics entirely.

**Why it happens:**
It's far easier to enumerate the directory than to parse the SWG `.cfg` format (INI-ish but with repeated keys, `key=value` accumulation by index, `[Section]`s, and `@import`/`@include`-style chaining in some configs). The repeated-key accumulation (`getKeyString(section, key, index, default)`) is the part people miss — multiple `searchTree0=...` lines all coexist.

**How to avoid:**
Parse the actual `client.cfg` (and any chained configs) `[SharedFile]` section: collect all `searchPath<P>[_SKU_]`, `searchTree<P>[_SKU_]`, `searchTOC<P>[_SKU_]` keys with their repeated values, reconstruct the priority-ordered node list. Offer a directory-glob mode only as an explicit fallback clearly labeled "all archives on disk (not load order)". Treat the `.cfg` as the source of truth for *which* archives and *in what order*.

**Warning signs:**
The tool's archive list differs from `TreeFile`'s reportTreeFilePaths debug dump; SKU/locale archives appear that the user's account never loads; two installs with identical files but different `.cfg`s diff as "identical" (you ignored the cfg).

**Phase to address:** TRE-tool config-ingestion phase (precedes virtual-tree).

---

### Pitfall 4: Case-sensitivity and path-normalization mismatch in the virtual tree

**What goes wrong:**
The same logical asset appears as two different entries (`appearance/Mesh.msh` vs `appearance/mesh.msh`, or `texture\font\x` vs `texture/font/x`), so the diff reports spurious add/remove pairs, and CRC-based lookups miss.

**Why it happens:**
SWG internal paths are **case-insensitive and slash-normalized**, but the *stored* name-block bytes preserve whatever case the packer used. The engine canonicalizes every lookup through `fixUpFileName` (`TreeFile.cpp:511`): lowercases all chars, converts `\`→`/`, strips leading `\`,`/`,`./`,`../`, and collapses repeated slashes. The TOC binary search then compares with `_stricmp` after a CRC match. A tool that compares raw stored names byte-for-byte will treat case/slash variants as distinct files; one that lowercases for comparison but displays the raw name is correct.

**How to avoid:**
Apply the exact `fixUpFileName` normalization (lowercase, `\`→`/`, strip `./`,`../`,leading-slashes, collapse `//`) to produce the *key* for virtual-tree matching and for CRC computation, but retain the original-case stored name for display. Match the engine's `_stricmp` tie-break semantics. Do NOT assume the name block is unique or sorted by name — it's sorted by CRC, then name.

**Warning signs:**
Diffs full of "renamed" pairs that differ only in case/slashes; CRC verification fails on files that clearly exist; the same asset listed twice in a single archive's tree view.

**Phase to address:** TRE-tool virtual-tree phase (the normalization layer is shared with Pitfall 2).

---

### Pitfall 5: Eager full-archive hashing — memory blowup and minutes-long diffs

**What goes wrong:**
Comparing two installs (~30+ archives × tens of thousands of records each) by decompressing and MD5-hashing every file eagerly turns a "should be seconds" set-delta into a multi-minute, multi-GB-RAM operation, and can OOM the browser/Node process.

**Why it happens:**
The "compare two installs" framing tempts a content-hash-everything approach. But the TRE TOC **already carries per-entry metadata that answers most diff questions without decompressing anything**: each `TableOfContentsEntry` has `crc` (a CRC of the *path*, not the content — see Pitfall 6), `length` (uncompressed), `offset`, `compressor`, `compressedLength`. Two same-named entries with identical `length` + `compressedLength` + identical compressed bytes are almost certainly identical; content hashing is only needed to disambiguate the rare collision or to *prove* equality. Decompressing 30 archives × 50k files to MD5 each is the brute-force path.

**How to avoid:**
Tiered comparison: (1) set-level archive delta from the TOC alone (present/absent, size deltas) — no decompression; (2) file-level "changed" detection from `(length, compressedLength)` and a cheap hash of the *compressed* bytes (skip the zlib expand); (3) only fall back to decompress-and-content-hash for entries that are ambiguous or that the user explicitly drills into. Stream from the archive (seek to `offset`, read `compressedLength`) rather than loading whole TREs into memory. Cap concurrency.

**Warning signs:**
Comparing two installs spins for minutes; RAM climbs to GB; the tool reads every archive fully before showing anything; progress is all-or-nothing instead of incremental.

**Phase to address:** TRE-tool diff-engine phase (design the tiering up front; retrofitting laziness is painful).

---

### Pitfall 6: Misusing the TOC `crc` field as a content checksum

**What goes wrong:**
The tool reports files as "unchanged" because their TOC `crc` matches, when in fact the *contents* differ — or it tries to verify file integrity with `crc` and is confused when it never matches the data.

**Why it happens:**
The `crc` field in `TableOfContentsEntry` is `Crc::calculate(fileName)` — a CRC of the **normalized path string**, used as the primary key for the binary search (`localExists`: compare `crc`, then `_stricmp` the name on collision). It is NOT a checksum of the file's bytes. Two archives can have the same path-CRC for the same filename while the file contents differ entirely (that's the whole point of an override TRE). The format carries `length`/`compressedLength` but **no content checksum/MD5 field at all**.

**How to avoid:**
Treat `crc` strictly as a path-identity key (and a fast equality pre-filter for the binary search). For genuine content-equality, compare `(length, compressedLength)` first, then the actual (compressed or decompressed) bytes. Document clearly in the UI that "changed" is content-based, not crc-based.

**Warning signs:**
Files with obviously different sizes report "same crc → unchanged"; integrity checks using `crc` always fail against the data bytes; an override TRE's replacement files show as identical to the base.

**Phase to address:** TRE-tool diff-engine phase.

---

### Pitfall 7: Windows file-locking the live install while the game is running

**What goes wrong:**
The tool opens TREs without share-read, or holds exclusive handles, and either fails to open archives the running client has mapped, or blocks the client / gets blocked. The TRE tool's first real use case (SWGSource-vs-whitengold space-asset diff) is exactly the scenario where someone has the client running.

**Why it happens:**
Default file-open modes on Windows can request exclusive access; a long-lived read handle held across the whole diff keeps the file busy. The client opens its TREs read-mostly but a careless writer-mode or non-shared open from the tool collides.

**How to avoid:**
Open every TRE **read-only with FILE_SHARE_READ|FILE_SHARE_WRITE** (Node: plain `'r'` read streams are fine; if using native handles, set share flags explicitly). Never open for write. Read-seek-close per entry instead of holding handles open for the whole session where practical. Treat the install dirs as strictly read-only inputs.

**Warning signs:**
"File in use / access denied" when the client is running; the client stutters or fails an async load while the tool runs; the tool works only when the game is closed.

**Phase to address:** TRE-tool foundation phase (file-access layer).

---

### Pitfall 8: Re-entrancy guard that swallows legitimate same-frame portal transitions

**What goes wrong:**
The cantina corner-snap "fix" adds a guard in `CellProperty::Notification::positionChanged` that suppresses *any* second cell transition in a frame — but legitimately traversing a doorway can require a genuine A→B transition in the same frame (e.g. fast movement crossing a thin portal cell). Over-suppression leaves the avatar/camera in the wrong cell, causing worse artifacts (clipping through walls, wrong-cell render/cull) than the snap it replaced.

**Why it happens:**
The mechanism (already root-caused, memory `project_cantina_corner_snap_engine_quirk` + `.planning/todos/pending/2026-05-15-cantina-corner-snap-engine-improvement.md`) is specifically a **re-entrant A→B→A reversal ping-pong** (3–5 transitions/frame, each the exact reversal of the prior), NOT "more than one transition is bad." A blanket "one transition per object per frame" guard cannot distinguish the pathological reversal from a valid multi-cell traverse. The `dueToParent` early-out already exists and does NOT catch the re-entry — naively widening it risks suppressing parent-propagated updates that other systems depend on.

**How to avoid:**
Guard the **reversal/oscillation specifically**: detect that this same-frame re-entry would move back through the portal it just crossed (A→B then B→A with the same portal/old-new reversed) and keep the *first* result, rather than capping transition count. Verify with the committed `_DEBUG` `CORNERSNAP-PORTAL/RESOLVE/CELLJUMP` instrumentation (a9b419daf) — ping-pong frames gone AND legitimate door traversals still land in the right cell. Test fast doorway run-throughs and the camera-boom-straddling-portal case separately (camera churn may want its own hysteresis, not the same guard).

**Warning signs:**
After the fix: avatar ends up in the wrong room after running through a door; camera renders the wrong cell; objects flicker between cells; the instrumentation shows transitions being dropped that aren't reversals. Don't ship without the dual check (snap gone + traversal correct).

**Phase to address:** Cantina corner-snap phase. Re-use the existing instrumentation as the acceptance harness; do not re-derive the mechanism.

---

### Pitfall 9: D3DXCompileShader→D3DCompile port silently changes shader behavior

**What goes wrong:**
The Fix B port (replace `D3DXCompileShader`/`D3DXAssembleShader` with `D3DCompile`) compiles fine but produces subtly different bytecode/behavior, OR fails to handle the **assembly** path at all, regressing the D3D9 visual parity that v2.2 just achieved.

**Why it happens:**
`d3dx9shader` and `d3dcompiler` are *similar but not drop-in*:
- **Macro struct:** `D3DXMACRO` → `D3D_SHADER_MACRO` (same shape, different type — every `ms_defines` array and the include handler signatures change).
- **Include interface:** `LPD3DXINCLUDE` (`ID3DXInclude`, `D3DXINCLUDE_TYPE`) → `ID3DInclude` (`D3D_INCLUDE_TYPE`). The existing `IncludeHandler` (`Direct3d9_VertexShaderData.cpp:144`) with its `../../`-strip hack and TreeFile-backed `Open`/`Close` must be reimplemented against `ID3DInclude`.
- **Flags:** `D3DXSHADER_*` → `D3DCOMPILE_*`. The numeric values are NOT all identical and the set differs; copying the old flag DWORD verbatim binds wrong options (mirrors the v2.2 "copied D3D9 register indices" trap). Re-derive intent (debug/skip-opt/etc.), don't copy bits.
- **Output blob:** `LPD3DXBUFFER` → `ID3DBlob` (different `Release` discipline).
- **No assembly path:** `D3DCompile` compiles **HLSL only**. The current code calls `D3DXAssembleShader` at `Direct3d9_VertexShaderData.cpp:567` for the `.vsh`/assembly path. `d3dcompiler` exposes `D3DAssemble`/`D3DDisassemble`, but `D3DAssemble` is sparsely documented and not a clean equivalent. **Any asm-input shader path must be handled deliberately** — either route it through `D3DAssemble`, or confirm those assets are also available as HLSL, or keep the D3DX assemble path for asm-only inputs. Dropping it silently breaks every asm shader (null VS → skipped draws).
- The whole point of Fix B is that `D3DCompile` is immune to the modern-toolchain D3DX FP-cleanup fault (the 0xC0000090 crash the Phase-19 SEH guard works around). Once D3DCompile is in for the *compile* path, the SEH guard becomes dead for that path — but **do not remove it until the assemble path is also off D3DX**, or you reintroduce the fault on asm shaders.

**How to avoid:**
Port the compile path first, keep `D3DXAssembleShader` (still SEH-guarded) until its replacement is proven, and **A/B the rendered output** against the D3D9 baseline (the v2.2 reference-sequence-first method) for the specific shaders that previously faulted AND a broad scene. Map flags by intent, not by value. Reimplement the include handler against `ID3DInclude`. Link `d3dcompiler_47` and confirm the redist is staged (see Pitfall 11).

**Warning signs:**
Shaders compile but lighting/UV/fog look subtly off vs the gl05 baseline; asm-input shaders fail to load (null VS) → draws skipped; `D3DCompile` returns errors on `#include` (include handler not wired); the SEH guard never fires anymore but a different crash appears on an asm shader.

**Phase to address:** D3DCompile-port phase. Invariant: client still boots to character select under `rasterMajor=5` AND `=11` (PROJECT.md core invariant), with D3D9 visual parity intact.

---

### Pitfall 10: Removing instrumentation that something else still references

**What goes wrong:**
Stripping the D-15 DPVS instrumentation or CORNERSNAP `_DEBUG` probes breaks the build (orphaned callers), silently leaves a registered debug flag dangling, or removes the very instrumentation a sibling fix (corner-snap) is being verified with — sequencing the removal too early.

**Why it happens:**
Dead code in this engine is still *linked* via callers, config-flag registration, and `.vcxproj` build-graph references — not just the symbol. The D-15 DPVS instrumentation spans `DpvsProfileInstrumentation.{h,cpp}`, `RenderWorld.cpp`, `ConfigClientGraphics.cpp`, and `clientGraphics.vcxproj`. The CORNERSNAP probes live in `CellProperty.cpp` + `CollisionResolve.cpp` and are wired to the `.planning/todos` capture workflow that the corner-snap fix (Pitfall 8) uses for acceptance. And the engine links under `/FORCE`, which downgrades unresolved externals to warnings — MSBuild exit 0 is NOT proof of a clean removal (the recurring v2.1 Decruft lesson).

**How to avoid:**
Strip the symbol AND its callers AND its config-flag registration AND the build-graph reference **atomically**. Gate on `unresolved external symbol` == 0 in the link output, not just MSBuild exit 0. Boot under BOTH `rasterMajor=5` and `=11`. **Sequence instrumentation removal AFTER the corner-snap fix that uses the CORNERSNAP probes** — don't strip the harness before the fix it verifies is done.

**Warning signs:**
Link warnings about unresolved externals (masked by `/FORCE`); a `DebugFlags::registerFlag` left without its variable; the corner-snap fix can't be verified because its instrumentation is gone; a `_DEBUG`-only block referencing a now-deleted symbol breaks the Debug config only.

**Phase to address:** Instrumentation-removal phase (sequenced after corner-snap fix).

---

### Pitfall 11: Config-gate default flips and de-hardcoded paths silently change shipped behavior / break a fresh machine

**What goes wrong:**
Two related traps: (a) the DPVS config-gate changes the *default* shipped behavior (occlusion outdoor-on/indoor-off, auto = occlusion bit only outside POB cells) and one renderer regresses because the change wasn't boot-tested under both; (b) de-hardcoding `stage/override` + `stage/miles` paths "works on my box" but a fresh checkout/copy can't resolve them (and `stage/miles/` is famously NOT in git and NOT copied by postbuild — memory `project_audio_fixed_missing_miles_redist`), so audio half-dies or shaders fall to fallback on another machine.

**Why it happens:**
"It's just a default" / "it's just a path" understates that both are observable behavior changes to the shipped client. The DPVS verdict was *revised* precisely because both Phase-10 verdicts flipped under D3D11 — the auto-mode logic must actually key on "outside POB cell," and the two renderers have different driver-overhead profiles. The path work touches a redist (`stage/miles/`) that isn't tracked or auto-copied, so a portability change that assumes it's present passes on the author's machine and fails elsewhere.

**How to avoid:**
Treat config-gate defaults as behavior changes: boot-verify under `rasterMajor=5` AND `=11`, confirm the auto-mode actually gates occlusion on POB-cell membership, and that neither renderer regresses. For paths: resolve relative to a discoverable root (exe dir / a documented env var or cfg key), test from a fresh copy on a non-dev machine, and document the `stage/miles/` redist requirement explicitly (it must still be present even after de-hardcoding the path). Clean up `stage/client_d.cfg`'s accumulated test settings as part of this, not separately.

**Warning signs:**
DPVS change boots one renderer fine, the other regresses/over-culls; auto-mode culls occluders indoors (POB) or fails to outdoors; a fresh copy boots silent (Miles redist missing) or magenta/fallback shaders (override path unresolved); absolute author paths left in cfg/stage.

**Phase to address:** DPVS config-gate phase + machine-portability phase.

---

## Technical Debt Patterns

Shortcuts that seem reasonable but create long-term problems.

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Directory-glob `*.tre` instead of parsing `client.cfg` search keys | Skips cfg parsing | Wrong archive set + no precedence → diffs subtly wrong (P3) | Only as an explicit, labeled "all-on-disk" fallback mode |
| Decompress + MD5 everything eagerly | Simple, obviously-correct equality | Minutes-long diffs, GB RAM, OOM (P5) | Tiny test corpora only; never for the real 30-archive case |
| Hardcode TREE v0005 struct only | Fastest reader to write | FATALs/mis-parses other distributions' archives (P1) | Never — branch on (token,version) from day one |
| Reuse the old `D3DXSHADER_*` flag DWORD verbatim in D3DCompile | No flag re-research | Wrong compile options, subtle shader regressions (P9) | Never — re-derive by intent |
| Blanket "one cell transition per frame" guard | One-line corner-snap fix | Breaks legitimate door traversals (P8) | Never — guard the reversal specifically |
| Remove the D3DX SEH guard the moment D3DCompile compiles | Cleaner diff | Reintroduces 0xC0000090 on asm-input shaders still on D3DXAssemble | Only after the assemble path is also off D3DX |
| Flip a config-gate default without dual-renderer boot test | "It's just a default" | Silent behavior change; one renderer regresses (P11) | Never — gate changes are behavior changes |
| De-hardcode a path but assume the redist is present | Path looks portable | Fresh machine boots silent/fallback (`stage/miles/`) (P11) | Never — document + test the redist requirement |

## Integration Gotchas

Common mistakes when connecting to external services / formats.

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| SWG `client.cfg` parsing | Treating it as plain INI; one value per key | Repeated keys accumulate by index (`searchTree0=` multiple times); honor `[SharedFile]` section + SKU-infix keys + priority ordering |
| zlib decompression of TOC/name-block | Assuming the whole TRE is zlib | Compression is **per-section** and **per-entry**: `tocCompressor`/`blockCompressor` in the header gate the TOC and name block independently; each file entry has its own `compressor`. `CT_none=0, CT_deprecated=1 (FATAL if seen), CT_zlib=2`; `isCompressed()` = `!= CT_none` |
| Name block | Reading fixed-width or assuming sorted-by-name | Null-terminated strings concatenated; entries reference them via `fileNameOffset`; TOC sorted by **CRC then name**, not name alone. For `.toc` variant, on-disk `fileNameOffset` is a *length* to be running-summed post-load |
| `d3dcompiler` include handling | Passing a `D3DX`-style include object | `ID3DInclude` (not `ID3DXInclude`); or `D3D_COMPILE_STANDARD_FILE_INCLUDE` — but SWG needs the TreeFile-backed custom handler, so reimplement against `ID3DInclude` |
| Win32 file open on a live install | Exclusive/default share mode | `FILE_SHARE_READ|FILE_SHARE_WRITE`, read-only, never hold handles for the whole session |

## Performance Traps

Patterns that work at small scale but fail as usage grows.

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| Decompress-and-hash every file for diff | Multi-minute compares, GB RAM | Tier on TOC metadata first (size/compressedLength), content-hash only ambiguous entries | ~30 archives × tens of thousands of records (the stated real case) |
| Load whole TREs into memory | RAM = sum of all archive sizes | Seek to `offset`, read `compressedLength` per entry; stream | Multi-GB install sets |
| O(n²) virtual-tree merge (linear scan per file across archives) | Diff time quadratic in file count | Build a normalized-path → entry map (hash) once per archive, then merge keys | 50k+ files × 30 archives |
| Synchronous/blocking decompression on the UI thread | Frozen web UI during compare | Stream results incrementally; worker thread/process; cap concurrency | Any real compare |
| Re-reading/re-parsing TOCs on every UI interaction | Sluggish drill-down | Parse TOCs once into an in-memory model; cache | Interactive use |

## Security Mistakes

Domain-specific issues (this is a local-only Windows tool reading a proprietary binary format).

| Mistake | Risk | Prevention |
|---------|------|------------|
| Trusting header size fields to allocate | Malformed/hostile TRE → huge alloc or OOB read (the community-distribution "malformed archive" case) | Validate `numberOfFiles`, `sizeOfTOC`, name-block sizes, and every `offset+compressedLength` against actual file length before allocating/reading |
| Path-traversal on extract/export | A crafted entry name (`..\..\system32\...`) writes outside the export dir | Apply `fixUpFileName`'s `../`/`..\`/leading-slash stripping to *output* paths too; sandbox the export root |
| zlib decompression bomb | Tiny compressed entry claims huge `length` → memory exhaustion on expand | Cap decompressed size to `length` from the TOC and sanity-bound it; stream-expand with a ceiling |
| Web server binds to 0.0.0.0 | Local tool exposed to the network | Bind to `127.0.0.1` only; it "runs locally on Windows 11" — keep it local |

## UX Pitfalls

Common user-experience mistakes for a diff/compare tool.

| Pitfall | User Impact | Better Approach |
|---------|-------------|-----------------|
| No progress / all-or-nothing diff | User stares at a frozen page on a 30-archive compare | Incremental streaming: show set-level deltas immediately, file-level as it computes |
| Presenting raw stored names (mixed case/slashes) | Confusing "duplicate" entries that are the same asset | Display normalized paths (or original-case but de-duped by normalized key) |
| Conflating "in a different archive" with "changed" | False positives when a file just moved between TREs in the search order | Distinguish "moved/shadowed" (precedence) from "content changed" |
| No indication of which archive *wins* | User can't tell what the client actually loads | Show the resolved winner per file + the shadowed copies |

## "Looks Done But Isn't" Checklist

Things that appear complete but are missing critical pieces.

- [ ] **TRE reader:** Often missing the **TOC (`.toc`) container variant** — verify it parses a `.toc`+TRE-set bundle, not just standalone TREEs (different struct, `fileNameOffset`=length quirk).
- [ ] **TRE reader:** Often missing **per-entry/per-section compression branching** — verify uncompressed AND zlib entries both extract; verify `CT_deprecated` is rejected cleanly not crashed.
- [ ] **Virtual tree:** Often missing **case/slash normalization** — verify `Mesh.MSH` and `mesh\msh` collapse to one entry (P4).
- [ ] **Virtual tree:** Often missing **correct precedence direction** — verify against a real `client.cfg` that the highest-priority override TRE wins (P2/P3).
- [ ] **Diff:** Often missing **the size pre-filter** — verify it does NOT decompress unchanged files (watch time/RAM on the real 30-archive case).
- [ ] **Corner-snap fix:** Often missing **the legitimate-traversal regression test** — verify fast door run-throughs land in the correct cell, not just that the snap is gone (P8).
- [ ] **D3DCompile port:** Often missing **the assembly-input path** — verify asm-input shaders still load (else null VS → skipped draws), and the SEH guard isn't removed while D3DXAssemble remains (P9).
- [ ] **D3DCompile port:** Often missing **`d3dcompiler_47.dll` staging** — verify the redist is in `stage/` and gl05 boots (P9/P11).
- [ ] **Instrumentation removal:** Often missing **the things that REFERENCE the instrumentation** — verify removal doesn't break callers, config flags, or the build, and that the corner-snap fix is done first (P10).
- [ ] **Machine portability:** Often missing **a non-dev-machine test** — verify de-hardcoded `stage/override` + `stage/miles` resolve from a fresh copy, and that `stage/miles/` redist is still present (P11).

## Recovery Strategies

When pitfalls occur despite prevention, how to recover.

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| Wrong precedence direction (P2/P3) | LOW | Centralize resolution in one comparator; flip + re-test against `client.cfg`; all diffs recompute |
| Eager hashing perf (P5) | MEDIUM | Retrofit TOC-metadata pre-filter; add streaming; usually a diff-engine rewrite, not a UI change |
| Wrong TRE struct (P1) | MEDIUM | Re-derive structs from the in-tree headers (authoritative); add (token,version) dispatch |
| Over-suppressing portal transitions (P8) | LOW–MEDIUM | Narrow guard to reversal-only using committed instrumentation; re-test door traversals |
| D3DCompile behavioral regression (P9) | MEDIUM–HIGH | A/B vs gl05 baseline; revert to SEH-guarded D3DX path (still present) per-shader-path while fixing |
| Removed referenced instrumentation (P10) | LOW | Build break catches most; revert the removal, strip caller+probe atomically |
| Config-gate/path regression on another machine (P11) | LOW–MEDIUM | Re-test fresh copy under both renderers; restore redist; resolve paths relative to discoverable root |

## Pitfall-to-Phase Mapping

How roadmap phases should address these pitfalls.

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| P1 TRE version/variant hardcoding | TRE reader foundation | Parses retail + SWGEmu + SWGSource samples; unknown variant → clean error not crash |
| P3 Inventing the search set | TRE config-ingestion | Tool's archive list matches `client.cfg` `[SharedFile]` keys + order |
| P2 Precedence backwards | TRE virtual-tree | Override TRE wins for replaced files vs a real cfg |
| P4 Case/slash mismatch | TRE virtual-tree | `fixUpFileName`-normalized keys; no spurious case-only renames |
| P5/P6 Eager hashing / crc misuse | TRE diff-engine | 30-archive compare in seconds; "changed" content-based; size pre-filter active |
| P7 File locking | TRE foundation (file layer) | Compare runs with the client open |
| Malformed-archive safety | TRE reader foundation | Garbage/fuzz TRE → bounded error, no OOB/OOM |
| P8 Re-entrancy guard | Cantina corner-snap | Snap gone AND door traversals land correctly (committed instrumentation) |
| P9 D3DCompile port | D3DCompile port | gl05 boots to char-select; D3D9 visual parity A/B holds; asm path handled |
| P10 Instrumentation removal | Instrumentation removal (after corner-snap) | Debug+Release link clean (0 unresolved); both renderers boot; no orphaned callers/flags |
| P11 Config-gate default flips | DPVS config-gate | Dual-renderer (5 + 11) boot; auto-mode = occlusion only outside POB cells, measured |
| P11 Machine portability | Portability/cfg cleanup | Fresh-copy/non-dev-box boot; redist present; no absolute author paths in cfg/stage |

## Sources

- **`src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.{h,cpp}`** — authoritative on-disk TRE/TOC struct layout, version handling, zlib branching, CRC binary search (in-tree, HIGH).
- **`src/engine/shared/library/sharedFile/src/shared/TreeFile.cpp`** — `fixUpFileName` normalization, `searchNodePriorityOrder` precedence, `installSearches` cfg-key parsing (in-tree, HIGH).
- **`src/engine/shared/application/TreeFileExtractor/src/shared/TreeFileExtractor.cpp`** — reference traversal/extract loop (in-tree, HIGH).
- **`src/engine/client/application/Direct3d9/src/win32/Direct3d9_VertexShaderData.cpp`** — the SEH-guarded `D3DXCompileShader`/`D3DXAssembleShader` (Fix B target) + include handler (in-tree, HIGH).
- **Microsoft Learn — `D3DCompile` (d3dcompiler.h)** — `D3D_SHADER_MACRO`, `ID3DInclude`, `D3DCOMPILE_*` flags, `d3dcompiler_47.dll` (HIGH): https://learn.microsoft.com/en-us/windows/win32/api/d3dcompiler/nf-d3dcompiler-d3dcompile
- **Memory `project_cantina_corner_snap_engine_quirk`** + `.planning/todos/pending/2026-05-15-cantina-corner-snap-engine-improvement.md` — root-caused re-entrant ping-pong mechanism + committed instrumentation (project record, HIGH).
- **Memory `project_decruft_removal_build_graph_gotchas`** — `/FORCE` masking, atomic caller+symbol removal, link-grep gate (project record, HIGH).
- **Memory `project_audio_fixed_missing_miles_redist`** — `stage/miles/` not in git / not copied by postbuild; relevant to machine-portability de-hardcoding (project record, HIGH).
- **SWGANH Wiki TRE breakdown / Swg.Explorer / swg_tre (Rust)** — community corroboration that multiple TRE versions + zlib + path-CRC exist across distributions (community, MEDIUM; in-tree source supersedes for exact layout): http://wiki.swganh.org/index.php/TRE:TRE_Breakdown , https://github.com/wverkley/Swg.Explorer , https://lib.rs/crates/swg_tre

---
*Pitfalls research for: v2.3 Hardening (TRE compare web tool + in-client hardening)*
*Researched: 2026-06-12*
