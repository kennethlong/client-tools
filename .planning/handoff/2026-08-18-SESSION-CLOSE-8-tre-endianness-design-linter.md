# SESSION CLOSE 8 (2026-08-18) — endianness CR round-tripped · builder DESIGN delivered · tre-lint SHIPPED + all installs baselined

One session (2026-08-17 day → 08-18), one arc: the ⭐ CLOSE-7 opener ("where does the TRE
builder live?") got answered, and the answer spawned three shipped deliverables plus a
same-day cross-repo fix cycle. Read this file; CLOSE-7 remains the record for the PR/merge
state (unchanged this session). Trackers current: this file, the toolkit handoff dir (five
new provider docs), CONSULT-77/78 research sets, memory topic files.

## 0. TL;DR state

1. **Builder home SETTLED**: the TRE linter/builder belongs to SWG-Toolkit (their maintainer
   confirmation in writing, their handback §6). Full design DELIVERED to them, reviewed
   twice before handoff. They implement via their GSD planning; we are the format authority
   + fixture provider.
2. **The toolkit's TRE writer was broken and is now FIXED** (their `1ecb559`) — our
   CONSULT-77 CR proved their forward-ASCII version model wrong at runtime; they adopted
   items 1–4 same day, falsified-in-reverse.
3. **`tools/tre-lint/` EXISTS and WORKS** — zero-dep TS linter, 8 install baselines in
   `tools/tre-lint/baselines/*.json`, multiple real wild finds, built as the toolkit's
   `@swg/compose` B1 seed (handed via PROVIDER-NOTE).
4. **v0006 record layout SETTLED read-side on real bytes**: REORDER model (pad falsified
   0.0% on all 46 populated Restoration TOCs); compressor code 1 = Restoration-dialect zlib.
5. ⚠ **NOTHING from this session is committed** in either repo (tools/tre-lint/, all
   .planning docs, toolkit-side handoff docs — their session commits their own).

## 1. CONSULT-77 — the endianness verification round (morning)

**Claim tested**: toolkit treated forward-ASCII "0004"/"0005"/"0006" as versions distinct
from "5000"/"6000". **Verdict: byte-order artifact.** The version field is a
big-endian-composed uint32 tag serialized LE, same as the `EERT` magic.

- **Census**: 881 `.tre`/`.toc` files, 8 installs — 0 forward-`TREE`; only `EERT`+{`5000`,
  `6000`} and `' COT'`+{`1000`,`2000`}. `6000` = MAJORITY of retail v3.0 (137/209) —
  `.toc`-membered blob containers, `numberOfFiles==0`, plain zlib, engine never validates
  their headers (`TreeFile_SearchNode.cpp:822-835`).
- **Engine map** (Codex): SearchTree ctor accepts {TAG_0004, TAG_0005} else FATAL
  (`:448-449`, `:505-514`); SearchTOC accepts TAG_0001 only; `validate()` dead code;
  TreeFileBuilder emits exactly `EERT5000` (`TreeFileBuilder.cpp:777-778`); NO 32-byte TOC
  record anywhere in stock.
- **Runtime probes (the decisive additions — Opus caught that M0 flipped BOTH fields)**:
  Q2 = byte-copy of the M0-accepted `treexp_a.tre` with only bytes 4..7 → `"0005"` ⇒
  **FATAL ~4 s, exit 0x80000003, on OUR engine AND Sais's**; control (unflipped, same
  priority-12 mount) boots to world. Q3 = retail `EERT6000` on a `searchTree_` line ⇒ FATAL
  on stock, **BOOTS on Sais's engine** — his tree added `case TAG_0006:` with stride-32
  support and a field-REORDERED record map (his squash `3ab047315b`; no decryption anywhere
  in his sharedFile — he can enumerate Restoration, not read payloads).
- **Genealogy** (Sonnet): Utinni held the identical wrong belief (D-06b), falsified it
  in-phase ("the live client is 100% EERT5000"), corrected its enum, left stale
  doc-comments. Masking mechanism in both codebases: self-generated fixtures validated by
  the same parser that wrote them.
- **Blast radius** (Cursor): production deploy (`packPatch.ts:116`) already passed `'5000'`
  explicitly — a working workaround around a wrong model; `repackTre` default silently
  converted retail headers (turned out latent — no production callers).
- Artifacts: `.planning/research/CONSULT-77-*` (evidence pack, results, 2 CLI consults;
  Sonnet/Opus reports folded into the CR).

## 2. The cross-repo fix cycle (all five docs in `SWG-Toolkit/.planning/handoff/`, indexed there)

1. `2026-08-17-PROVIDER-CHANGE-REQUEST-tre-version-endianness.md` — the CR.
2. Their `…TOOLKIT-HANDBACK-…-adopted.md` — items 1–4 landed (`1ecb559`), verified against
   real bytes before adopting, falsified-in-reverse (writer reverted ⇒ 3 tests red incl. a
   byte-for-byte vs a real archive). Found: **a test asserting the defect** ("will NOT load
   in the live Infinity client" encoded as an expectation), a forward-bytes binary fixture,
   a duplicate version table the compiler exposed. Their correction to us: `repackTre` had
   no production callers (latent, not live). v0006 writes stay refused. Confirmed: the
   linter/builder is THEIRS.
3. `…PROVIDER-REPLY-probe-artifacts-and-4000-census.md` — probe artifacts GRANTED (paths +
   regeneration recipe + full gate procedure), "4000" answer (ZERO in census),
   102-skeleton corroboration (their `protocol_droid.skt` find = our catalogued class;
   repairer + middle-sibling clamp warning).
4. `…PROVIDER-DESIGN-tre-linter-set-builder.md` — see §3.
5. `…PROVIDER-NOTE-tre-lint-seed-and-v0006-verdict.md` — see §4.

## 3. The builder/linter DESIGN (afternoon) — delivered, twice-reviewed

**Kenny's scope call (recorded verbatim in the design):** build the FULL engine core now —
recipes included — behind a headless CLI; stage the GUI; new UI components later against the
working core; **where a feature fits an existing UI element, wire it in v1.**

Shape: new pure-Node `packages/compose` → `@swg/compose`, CLI `tresmith`
(lint/compose/verify/explain, always-dry-run), strictly no renderer/Electron imports
(renderer services WRAP compose, never reverse). Module map ports CONSULT-76 (recipe/fold/
cache/lint/repair/emit-packed/emit-loose/verify/manifest). Lint registry pattern-INSPIRED by
their `policyRuleset.ts` but with its own contracts (ruleId, error/warn/info — their policy
merges by classId with ban/advisory; do not conflate). v1 UI wiring: `DataPanel`/
`IffStructureTree`/`VfsTree` badges (NOT `InspectorStack`), prerequisite = propagate
`parseIff().defects` (currently stripped at `TreVfsBrowser.tsx:386-390`); compose-backed
deploy behind a settings-file field; recipe/report UI = CLI-only + INVISIBLE.

**Review round (CONSULT-78) — both reports in `.planning/research/`, all findings applied:**
- Cursor binding check (11 corrections): `assertSweep()` exists but is NEVER CALLED (design
  now instructs wiring it); `@swg/contracts` has no built `dist/` (CLI needs build step or
  alias); no `bin` precedent in the workspace; only `crc32.ts`/`pathSafety.ts` are
  move-ready pure; harness has a test-only `buildSyntheticToc()` (B0 seed); `VfsTree`
  already shows winner archives.
- Fresh-Fable adversarial (11 findings). **F1 BLOCKER — a `.toc` set CANNOT tombstone**
  (`SearchTOC::exists` hardcodes `deleted=false`, `:1010-1015`; zero-length `.toc` entry =
  plain not-found): removals require a tombstone-carrier `.tre` on its own `searchTree`
  line above the searchTOC; Stage-2 realizability abort + B0 zero-length-`.toc` probe +
  named running-engine acceptance added. **F2 — Stage-8 verify must be CONTENT-hash level**
  (native `.toc` parser is a PORT of `tocReader.ts` — shared genealogy; member-read path
  has NO payload CRC) + the `.toc` re-emit-`sku0_client.toc`-byte-identical gate. F3
  member-name resolution contract (CWD→`TOCTreePath`, release-FATAL on miss; cfg fragment
  must emit `TOCTreePath`). F5 restored `cache.ts`. F11 build order SPLIT: B3a compose over
  the engine-proven LOOSE path first, B3b packed+`.toc` writer separately (one variable per
  stage). Survived refutation: full-recipe-in-v1 is NOT dead code; '0005' members in a
  `.toc` fine; searchTOC 24-byte record layout (with the fileNameLength-not-offset subtlety
  made explicit).
- The superseded draft (`.planning/research/TRE-BUILDER-DESIGN-draft.md`) is kept for the
  reviews' section references.

**Owed by us to the toolkit (open):** cohort class table seed + engine-special-files rows
(baked shader-cache includes — the M0 cache-disable finding); 102-pair fixture vendoring
location/sign-off = **Kenny's call** (GR-derived bytes into their git).

## 4. tre-lint — built, validated, baselined (evening)

`tools/tre-lint/` — 4 erasable-TS modules (`format.ts`, `iff.ts`, `lint.ts`, `cli.ts`) +
README + `baselines/`. Zero deps; runs as `node tools/tre-lint/src/cli.ts <target>` (Node
type stripping; machine has v24). CRC self-test at startup. Flags: `--deep` (payload inflate
+ IFF size-fit walk), `--filter`, `--json` (deterministic baseline artifact), `--baseline`
(exit 2 + named delta on any NEW non-info finding = file-granularity `V_full \ V_base`),
`--quiet`. Per-rule cap 25/archive in findings with rollup — EXCEPT set-degenerate-shadow,
where ALL pairs go to JSON (console capped at 15/rule) so baseline diffing sees new pairs.

**Validation:** stock SWGEmu 53 archives ZERO findings (no false positives);
v3.0 213 archives 0 errors + **129,101 `.toc` entries cross-checked vs self-indexed
members, ALL coherent**; ILM_visuals `--deep --filter .skt` reproduces the corrupt-skeleton
class **exactly 102/102, each +16 bytes**; probe pair gates correctly (treexp_a clean /
q2_fwdver errors); baseline self-diff exit 0, planted defect exit 2.

**Settled on real bytes:** v0006 32-byte record = **REORDER** (`crc,length,offset,0,0,
fileNameOffset@20,compressor@24,compressedLength@28`) — pad model falsified 0.0% on all 46
populated Restoration TOCs (dual-model arbitration per archive, CRC-of-name as arbiter;
sub-100% reorder scores = the volume-split `_01_a/b/c` sharing one TOC). **Compressor 1 =
Restoration-dialect zlib** (215k+ entries; linter reports info in v0006, warn elsewhere).
Restoration payloads never decrypted (standing rule) — bounds only.

**Wild finds (all in baselines):** Beyond's `beyond_patch_01.tre` obfuscated TOC found
unprompted (the third 6000 profile); ⭐ **a REAL in-archive 32-bit CRC collision** in
`infinity_custom_01.tre` (`…boss_nass…ready.ans` vs `…speederbike_deed.iff`, both
0xf2ead531) — living fixture for the dup-CRC rule; `patch_00` stubs `terrain/tutorial.trn`
to 990 B (SOE retiring the tutorial — historic content-kill); retail's 176 B
`arc170_cockpit_1_emis.dds` stub is UN-stubbed by ILM with 349 KB (ILM ADDING content).

**Stub/degenerate-shadow rules (Kenny's ask):** `set-degenerate-shadow` (warn, order-free,
names both copies; smallest ≤ max(512 B, 5% of biggest), biggest ≥ 4 KiB, smallest > 0;
searchTOC-blob copies MERGED into the census — the TOC-layer blind spot closed) ⇒ **582
pairs in v3.0, 90 with the stub side in ILM** = the preference-kill landmine catalog
enumerated mechanically. Plus `tre-stub-sized-texture` (info) and `--deep`
`tre-stub-texture` (DDS dims ≤16×16). ⚠ Honest limit (in README + note): catches
SIZE-degenerate stubs only; the density-ZEROED datatables are content-degenerate at similar
size ⇒ `@swg/compose` B4 cohort/content-rule territory.

Baselines (8): swgemu-stock, swgsource-v3.0, client-dx11, restoration, beyond, infinity,
stardust, ilm-visuals-skt-deep. v3.0 profile: 0 errors / 583 warns (582 degen + 1
`.toc`-zero-length) / 509 infos.

## 5. Gotchas learned this session (each cost real time)

- **Client cfg-on-command-line is POST-`--` ONLY** (`SetupSharedFoundation.cpp:204`):
  `SwgClient_r.exe -- @probe.cfg`. `@file` load failures are `IGNORE_RETURN`-silent — **a
  booted client does NOT prove the probe mounted**; always run the reject case as control.
  (Cost one false-negative Q2 run; Kenny then adopted the booted probe client as a live
  session — never kill a client you didn't verifiably start, even one YOU launched, once
  Kenny takes it over.)
- Early-FATAL probes (TreeFile::install stage) ARE valid from an agent shell — the FATAL
  fires pre-DirectInput; reject = exit 0x80000003 in ~4 s with zero report-log growth
  (log sink attaches later); accept = "Star Wars Galaxies" window alive.
- .NET static file APIs (`[System.IO.File]::Open`) do NOT follow PowerShell `Set-Location`
  — absolute paths always.
- Baseline artifacts must contain ALL findings of census rules (cap console, not JSON) or
  `--baseline` regression detection silently loses coverage — first version had a 58 MB
  Restoration JSON from an uncapped warn flood, second had a capped-but-blind census; the
  shipped shape caps per-rule structural findings at 25 w/ rollup and exempts the census.
- `gh`/PS pipe exit-255 cosmetic issue struck again (`Select-Object -First N` closing the
  pipe) — output is fine, check content not exit code.

## 6. Next-session board (priority order)

1. **Sais review of PR #2/#3** — unchanged from CLOSE-7; still the top external wait. Then
   the joint SWGSource upstream conversation.
2. **Toolkit implements the design** (their GSD planning; their repo is mid-phase-05.6
   wave 10 — expect the compose phase after). Watch their handoff dir for the phase plan;
   our review of it is implicitly expected. Deliver when asked (or proactively):
   **cohort class-table seed + engine-special-files lint rows**; Kenny to rule on
   **102-pair fixture vendoring** into their git.
3. **A-side owes (unchanged from CLOSE-7):** the c_ambient-class gl11 fix (our renderer
   washes stock interiors the same way B's did pre-c54 — "black walls full ambient" same
   compensation class), then RE-BASELINE every ILM "brightening" claim on the fixed
   renderer. (Kenny's closing joke this session — "past you told me ILM broke the cantina
   lighting" — is the compensation-stack lesson; the ILM re-habilitation data from tre-lint
   makes the re-baseline concrete.)
4. **Commits owed when Kenny says go**: `tools/tre-lint/` (+ README + baselines decision —
   baselines are machine-local, maybe gitignore), `.planning/` docs (CONSULT-77/78 sets,
   handoffs, README edits). Toolkit-side docs are committed by their session.
5. ILM sweep proper (the saga) waits for `tresmith` (their B-stages) — recipe-not-bytes
   rides B4. The 90-stub census + 102-skeleton corpus are its ready inputs.
6. Parked probe artifacts stay parked (stage-B-x64: q2_fwdver.tre, client-q2/q3/q2ctl.cfg,
   treexp set; stage-x64: cfg copies) — they are the toolkit's granted vendoring source
   AND our re-run kit.

## 7. Where everything lives

- Tool: `tools/tre-lint/` (src, README, baselines/).
- Research: `.planning/research/CONSULT-77-*` (endianness round),
  `CONSULT-78-*` (design reviews), `TRE-BUILDER-DESIGN-draft.md` (superseded draft).
- Cross-repo: `D:/Code/SWG-Toolkit/.planning/handoff/2026-08-17-PROVIDER-{CHANGE-REQUEST,
  REPLY,DESIGN,NOTE}-*` + their `TOOLKIT-HANDBACK` + their index (updated).
- Memory: `project_ilm_tre_cleanup_saga` topic file carries the durable facts (mirror
  model, v0006 REORDER verdict, toc-can't-tombstone, probe how-to, tre-lint state).
