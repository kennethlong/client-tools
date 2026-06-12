# Feature Research

**Domain:** Web-based archive/asset comparison tool for SWG `.tre` installations
**Researched:** 2026-06-12
**Confidence:** HIGH (TRE format verified from the engine source itself; community-tool landscape and diff-UX patterns verified via web survey)

> Supersedes the prior v2.2 Visual Parity FEATURES.md (archived content was renderer-porting research; see `milestones/v2.2-*` for that milestone's record).

## Scope note

This file covers ONLY the **TRE compare tool** (the one new user-facing capability in v2.3 Hardening). The other v2.3 items (DPVS config-gate, instrumentation removal, machine portability, Options-window FATAL, D3DCompile port, cantina corner-snap fix) are internal client fixes with no feature-research need and are excluded per the milestone brief.

The tool compares two SWG installations at two levels:
1. **Set level** — which `.tre` archives exist in each install; size/version/file-count deltas.
2. **File level** — the merged virtual file tree each install presents after the search-path overlay resolves (later-priority archive wins per filename), with per-file added/removed/changed status.

First real use case: diagnosing the parked space-scene graphics-artifact diff (SWGSource Client v3.0 vs whitengold's data set).

---

## Ground truth: how a TRE actually resolves (from the engine, not guessed)

Verified directly in `src/engine/shared/library/sharedFile/src/shared/TreeFile.cpp` and `TreeFile_SearchNode.{h,cpp}` — this is the authoritative spec the tool must mirror, not a third-party reverse-engineering.

**Archive header** (`SearchTree::Header`, 32 bytes, read from offset 0):

| Field | Type | Meaning |
|-------|------|---------|
| `token` | Tag | `'TREE'` magic (`TAG_TREE`) — validate this first |
| `version` | Tag | only `TAG_0004` accepted by `validate()`; `0005` also handled in the ctor switch |
| `numberOfFiles` | uint32 | TOC entry count |
| `tocOffset` | uint32 | byte offset of the TOC block |
| `tocCompressor` | uint32 | compressor code for the TOC block |
| `sizeOfTOC` | uint32 | compressed TOC size |
| `blockCompressor` | uint32 | compressor code for the name block |
| `sizeOfNameBlock` / `uncompSizeOfNameBlock` | uint32 | filename-table sizes |

**Per-file TOC entry** (`TableOfContentsEntry`): `crc`, `length` (uncompressed), `offset`, `compressor`, `compressedLength`, `fileNameOffset`. Filenames live in a separate (zlib-compressed) name block indexed by `fileNameOffset`.

**Compression:** zlib (`ZlibCompressor`); `isCompressed()` gates per-block; `CT_deprecated` is a fatal/unsupported code. The TOC and name block are themselves zlib-compressed; individual file records may be stored or zlib-compressed per their `compressor` field.

**Two load-bearing consequences for the tool:**
- **Per-file CRC is in the TOC.** Content-identity comparison is *free* — you do NOT need to decompress every record to tell "changed" from "identical". CRC + uncompressed `length` is the cheap, correct change signal. (Confidence: HIGH — `crc` field read straight from the struct.)
- **Search-path resolution is priority-ordered, later-wins-per-filename.** `TreeFile::install` adds `searchPath%d` / `searchTree%d` / `searchTOC%d` by numeric priority from `client.cfg`; the merged virtual tree is the union of all archives, with the highest-priority archive supplying each filename. The tool's file-level view is meaningless unless it replicates this exact overlay order — that IS the feature.

---

## Competitor landscape (surveyed, not invented)

| Tool | Tech / era | Does | Falls short |
|------|-----------|------|-------------|
| **TRE Explorer** (MTGUli / ilikenwf fork) | C# WinForms, archived Aug 2022 | TOC/TRE view, extract, edit, repack; aggregates older tools (TRE Archiver, InsideSWG, Kadoo, STF Edit, IFF Reader) | Single-archive-centric, dated WinForms UI, no install-vs-install diff, no merged-tree-with-overlay-resolution, no comparison concept at all |
| **Swg.Explorer** (wverkley) | C# / MonoGame, mid-late-2000s look | **Multi-archive combined view**, IFF + STF preview, mesh→Collada export, STF→CSV | Combined VIEW only (no diff/compare), no per-file change status, no two-install comparison, no search/filter across archives, desktop-only |
| **swg_tre** (Rust crate) | library, current | read + write TRE programmatically | Library, not an app — useful as a parser-reference cross-check, no UI |
| **treLib / SWGEmu tre tools** | C++/scripts | extract/pack for server data prep | CLI/build-pipeline oriented, no comparison UI |
| **WPS Toolchain IFF Inspector / SWG IFF Viewer** (VS Code ext) | modern dark webview | clean chunk-hierarchy IFF inspection with offsets | IFF-file-level only — no archive set/diff layer; *but proves the "modern clean UI" bar the user wants and shows it's achievable in a webview* |

**The gap the user named ("modern looking unlike most of these tools") is real:** every TRE-archive tool is either an archived WinForms app or a 2000s-MonoGame desktop viewer, and **none of them do install-vs-install comparison at all.** The comparison capability is genuinely new for the SWG ecosystem; the modern web UI is the second differentiator. The only modern-looking SWG tooling (WPS IFF viewer) operates one level below (inside a single IFF), not at the archive-set level.

---

## Feature Landscape

### Table Stakes (Users Expect These)

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| **TRE archive parser (header + TOC + name block)** | Nothing works without reading the format | MEDIUM | Mirror `TreeFile_SearchNode` exactly: `'TREE'` magic, version `0004/0005`, zlib-decompress TOC + name block. swg_tre (Rust) and the C++ engine source are both reference parsers. Do NOT decompress record payloads at this stage. |
| **Set-level archive list per install** | User explicitly asked "which TRE archives exist in each installation" | LOW | Enumerate `.tre` files; the priority order comes from parsing each install's `client.cfg` `searchTree%d`/`searchPath%d` entries, not directory order |
| **Set-level delta (added / removed / in-common)** | Core ask — "size/version deltas" between the two sets | LOW | Archive-name set difference + per-archive size + `numberOfFiles` + version |
| **Merged virtual file tree per install (overlay resolution)** | User's exact words: archives "overlay in search-path order, later wins per-file" | MEDIUM | The differentiator-grade feature most tools lack; table stakes *here* because it's the named requirement. Resolution must match `client.cfg` priority semantics |
| **File-level diff: added / removed / changed status** | The whole point of a compare tool | MEDIUM | "Changed" = CRC differs OR uncompressed length differs (both already in TOC — no payload decompression). "Added/removed" = filename present on only one side |
| **Tree view with per-node status badges** | Universal diff-tool expectation (WinMerge, Beyond Compare, every git UI) | MEDIUM | Folder-path tree (`.tre` filenames are slash-pathed virtual paths); roll child status up to parent folders (folder = "has changes" if any descendant changed) |
| **Filter by status (only changed / added / removed / hide identical)** | Beyond Compare & WinMerge both default-hide identical files; mandatory with tens of thousands of files | LOW | Client-side filter over the diff result |
| **Summary stats (X added, Y removed, Z changed, N identical)** | Users expect an at-a-glance verdict before drilling | LOW | Header bar; cheap from the diff pass |
| **Per-file detail (winning archive, size, CRC, compressor)** | Needed to answer "why did this file change" — overlay-winner identity | LOW | Show the resolved winning archive + the shadowed losers; directly serves the space-artifact diagnosis |
| **Search / filter file list by name or path** | Tens of thousands of entries; can't scroll | LOW | Substring + glob; Swg.Explorer notably *lacks* this and it's a known pain point |
| **Modern, clean UI** | Explicit user requirement; the named reason for building new | LOW-MEDIUM | The bar is "not WinForms / not 2000s MonoGame". A standard modern web stack clears it trivially; the WPS IFF viewer proves a clean dark webview is the expected aesthetic |

### Differentiators (Competitive Advantage)

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| **Install-vs-install comparison at all** | No existing SWG tool does this — it is the entire reason to build | (covered above) | Category-defining; everything else is presentation |
| **Two-level drill (set → merged file tree)** | "Which archive set differs" then "which files within" in one tool | MEDIUM | The two requested levels as linked views; click a set-level delta to filter the file tree to that archive |
| **Overlay-shadowing visualisation** | Show file X exists in 3 archives but archive-N wins; reveals when a later install *masks* a file differently | MEDIUM | Exactly the failure mode behind the space-artifact bug — a later archive in one install overriding a file the other resolves differently. High diagnostic value, unique |
| **Virtualised tree rendering** | A retail SWG merged tree is tens of thousands of files; naive DOM dies | MEDIUM | Virtual scroll / windowing separates "modern" from "hangs on real data". Near-table-stakes at real input sizes |
| **CRC/size-only fast diff (no payload decompression)** | Diff a whole install pair in seconds, not minutes | LOW | Free because CRC is in the TOC. Default mode; full-content diff is the opt-in escalation |
| **Permalink / shareable diff result** | "Here's the exact file that differs" in a bug thread | LOW-MEDIUM | Serialize install pair + filter + selected path into URL state |
| **Hex view of an individual file's bytes** | When CRC differs, see *how* | MEDIUM | Decompress the single selected record (cheap — one record). Useful for binary asset diagnosis |
| **IFF-aware structural preview (chunk tree)** | Most SWG assets are IFF; a nested FORM/chunk tree beats raw hex for "what changed semantically" | HIGH | WPS/VS-Code IFF viewers prove demand + feasibility. High effort; defer past MVP. A *structural IFF diff* would be best-in-class but is clearly v2+ |

### Anti-Features (Commonly Requested, Often Problematic)

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| **In-tool TRE editing / repacking** | TRE Explorer & Swg.Explorer do it, so users assume parity | Doubles format risk (write path), turns a read-only diagnostic into a modding tool, invites "corrupted my install" bugs; out of scope for the stated purpose | Stay read-only; single-file extract is the most you should offer, and even that is optional |
| **Full byte-level content diff of every file by default** | "Show me all the differences" | Requires decompressing every record in both installs (tens of thousands × 2); slow, memory-heavy, and the CRC already answers added/removed/changed | CRC/size fast-diff by default; decompress-and-diff only the single opened file |
| **Asset rendering (mesh/texture preview, Collada export)** | Swg.Explorer does it; feels expected for an SWG tool | Pulls in the whole asset pipeline (mesh/shader/texture decode) — months of work orthogonal to "compare two installs"; that's the client's job | Leave to Swg.Explorer; the compare tool answers "which files differ", not "what does this asset look like" |
| **Desktop-native rewrite (Electron/native) for "performance"** | Belief that web can't handle big trees | The data sizes (TOC metadata, not payloads) are modest; virtualised web rendering handles them, and "web app" is an explicit requirement | Virtual scroll + Web Worker for the parse/diff keeps the UI responsive in-browser |
| **Live filesystem watching / auto-rescan** | "Re-diff when I patch" | Adds filesystem-access + state complexity for a one-shot diagnostic workflow | Manual "re-run compare" button; investigate-then-done, not continuous |
| **N-way compare (3+ installs)** | "Why stop at two" | Multiplies the diff matrix and UI complexity; the use case is strictly pairwise (SWGSource vs whitengold) | Two-install compare only; revisit if a concrete 3-install need appears |
| **`searchAbsolute` / loose-file overlays in v1** | The engine supports loose-file search paths above TREs | Real but a corner case for the diagnostic use case (both installs are TRE-driven); adds a second resolution mechanism | TRE-archive overlay only for MVP; flag loose-path support as a known v1.x gap |

---

## Feature Dependencies

```
TRE archive parser (header + TOC + name block, zlib)
    └──required by──> Set-level archive list
    └──required by──> Merged virtual file tree (overlay resolution)
                          └──requires──> client.cfg search-priority parse
                          └──required by──> File-level diff (added/removed/changed)
                                                └──required by──> Tree view + status badges
                                                                      └──enhanced by──> Status filter
                                                                      └──enhanced by──> Summary stats
                                                                      └──enhanced by──> Overlay-shadowing view

File-level diff ──enables──> Hex view (decompress single record)
                                  └──enhanced by──> IFF-aware preview (v2+)

Virtualised tree rendering ──enhances──> Tree view  (mandatory at real data sizes)
```

### Dependency Notes

- **Everything depends on the parser.** It's the one irreducible piece; get the `'TREE'`/version/TOC/name-block decode exactly right against the engine source (cross-check with the swg_tre Rust crate). Confidence HIGH that the spec is fully known.
- **Merged tree requires the client.cfg priority parse.** The overlay order is *config-driven*, not directory-driven. Reading `searchTree%d`/`searchPath%d` priorities from each install's `client.cfg` is a prerequisite for any correct file-level diff — without it the "later wins" resolution is wrong and the file-level view is misleading.
- **File-level diff is cheap because CRC lives in the TOC.** This dependency *removes* a dependency: diff does NOT require a decompress pass. Only the optional hex/IFF view does.
- **Virtual scroll is a hard dependency of "usable", not a nice-to-have.** Real merged trees are large enough that non-virtualised rendering is the difference between "modern tool" and "browser tab hangs".

---

## MVP Definition

### Launch With (v1)

Minimum to satisfy the named requirement and serve the space-artifact diagnosis:

- [ ] **TRE parser** (header + zlib TOC + name block) — irreducible foundation
- [ ] **client.cfg search-priority parse** — required for correct overlay order
- [ ] **Set-level archive list + delta** (added/removed/size/version/file-count) — directly requested
- [ ] **Merged virtual file tree per install with overlay resolution** — directly requested, the hard part
- [ ] **File-level diff** (added/removed/changed via CRC+size) — the core value
- [ ] **Tree view with status badges + folder roll-up** — table-stakes presentation
- [ ] **Status filter + name/path search + summary stats** — usable at real data sizes
- [ ] **Per-file detail panel** (winning archive, shadowed archives, size, CRC, compressor) — the diagnostic payoff
- [ ] **Modern clean web UI + virtualised tree** — the explicit reason for building

### Add After Validation (v1.x)

- [ ] **Hex view of a single selected file** — trigger: a CRC diff that needs byte-level inspection
- [ ] **Single-file extract / download** — trigger: wanting the actual bytes outside the tool
- [ ] **Permalink / shareable diff state** — trigger: collaboration / bug-thread sharing
- [ ] **Loose-file (`searchAbsolute`) overlay support** — trigger: an install that overlays loose files above TREs

### Future Consideration (v2+)

- [ ] **IFF-aware chunk-tree preview** — defer: high effort; existing IFF viewers cover it standalone
- [ ] **Structural IFF diff (chunk-tree diff of two file versions)** — defer: best-in-class but large; only after IFF preview exists
- [ ] **N-way (3+ install) compare** — defer: no current use case beyond pairwise

---

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| TRE parser (header/TOC/name block) | HIGH | MEDIUM | P1 |
| client.cfg priority parse | HIGH | LOW | P1 |
| Set-level archive list + delta | HIGH | LOW | P1 |
| Merged virtual tree + overlay resolution | HIGH | MEDIUM | P1 |
| File-level diff (CRC/size) | HIGH | MEDIUM | P1 |
| Tree view + status badges + folder roll-up | HIGH | MEDIUM | P1 |
| Status filter + search + summary stats | HIGH | LOW | P1 |
| Per-file detail (overlay winner/shadowed) | HIGH | LOW | P1 |
| Virtualised tree rendering | HIGH | MEDIUM | P1 |
| Modern clean UI | HIGH | LOW-MEDIUM | P1 |
| Hex view (single file) | MEDIUM | MEDIUM | P2 |
| Single-file extract | MEDIUM | LOW | P2 |
| Permalink / shareable state | MEDIUM | LOW-MEDIUM | P2 |
| Loose-file overlay support | LOW-MEDIUM | MEDIUM | P2 |
| IFF chunk-tree preview | MEDIUM | HIGH | P3 |
| Structural IFF diff | HIGH | HIGH | P3 |
| N-way compare | LOW | HIGH | P3 |

**Priority key:** P1 = launch · P2 = add when possible · P3 = future.

---

## Competitor Feature Analysis

| Feature | TRE Explorer | Swg.Explorer | Our Approach |
|---------|--------------|--------------|--------------|
| Read TRE archive | Yes | Yes | Yes (parser mirrors engine + swg_tre) |
| Multi-archive merged VIEW | Partial | Yes (combined view) | Yes — but as a *resolved overlay*, not a flat union |
| Install-vs-install COMPARE | No | No | **Yes — the differentiator** |
| Per-file added/removed/changed | No | No | **Yes (CRC/size, no decompress)** |
| Overlay-shadowing (winner vs masked) | No | No | **Yes — serves the space-artifact diagnosis** |
| Search across files | No | No | **Yes** |
| File preview (IFF/STF/mesh) | Yes | Yes | Defer (P3) / leave to existing tools |
| Edit / repack | Yes | No | **No (anti-feature — read-only)** |
| Modern UI | No (WinForms) | No (2000s MonoGame) | **Yes (web, virtualised) — the explicit ask** |
| Platform | Windows desktop | Windows desktop | **Web app** |

---

## Dependencies on existing assets

- **TRE parser reference:** the *authoritative* spec is THIS repo's engine — `src/engine/shared/library/sharedFile/src/shared/TreeFile.cpp` and `TreeFile_SearchNode.{h,cpp}` (header struct, TOC entry, zlib decode, search-priority install). Cross-check with the `swg_tre` Rust crate and Swg.Explorer's C# reader. **Note:** the PROJECT.md pointer to `D:/Code/swg-tools` (swg-blender tre extract/repack) is stale — that path does not exist on disk; the live tre tooling reference is `D:/Code/swg-blender-plugin` plus the engine source above. (Confidence: HIGH for the engine source — read directly; the swg-tools path was confirmed absent.)
- **Real input data for the first use case:** two installations on disk — `D:/Code/SWGSource Client v3.0` (confirmed present) and the whitengold/retail data set. Both supply `.tre` archives + a `client.cfg` defining search priority.
- **Standalone, outside the boot invariant:** PROJECT.md is explicit that the TRE tool is a standalone web app, NOT bound by the "bootable to character select under rasterMajor 5 and 11" invariant. It can use any modern web stack independent of the C++ client.

---

## Sources

- **Engine source (authoritative, HIGH):** `src/engine/shared/library/sharedFile/src/shared/TreeFile.cpp`, `TreeFile_SearchNode.h`, `TreeFile_SearchNode.cpp` (this repo) — TRE header/TOC/CRC/zlib/search-priority semantics read directly.
- [Swg.Explorer (wverkley)](https://github.com/wverkley/Swg.Explorer) — combined-view TRE viewer, IFF/STF preview, Collada export (C#/MonoGame).
- [TRE Explorer (MTGUli)](https://github.com/MTGUli/TREExplorer) / [ilikenwf fork](https://github.com/ilikenwf/TREExplorer) — archived WinForms modding tool (TOC view/extract/edit/repack).
- [swg_tre (Rust crate)](https://lib.rs/crates/swg_tre) — TRE read/write library; parser cross-reference + compressor-code confirmation.
- [Beyond Compare folder-compare filtering docs](https://documentation.help/BeyondCompare4/dir_filtering_the_view.html) — show-orphans / hide-identical / structure filters (status-filter UX pattern).
- [WinMerge folder-compare manual](https://manual.winmerge.org/en/Compare_dirs.html) — expandable tree, show-identical toggle, recursive tree view (tree-badge UX pattern).
- [SWG IFF Viewer (VS Code, WPS)](https://marketplace.visualstudio.com/items?itemName=WastedPotentialStudiosLLC.swg-iff-viewer) + [WPS Toolchain dev tools](https://swgnb.com/dev-tools/) — modern dark webview IFF chunk-tree inspection (proves the "modern UI" bar + the deferred IFF-preview feasibility).
- [EA IFF 85 standard](https://wiki.amigaos.net/wiki/EA_IFF_85_Standard_for_Interchange_Format_Files) — FORM/nested-chunk structure underlying SWG `.iff` assets (for the deferred IFF-preview tier).

---
*Feature research for: web-based TRE compare tool (v2.3 Hardening)*
*Researched: 2026-06-12*
