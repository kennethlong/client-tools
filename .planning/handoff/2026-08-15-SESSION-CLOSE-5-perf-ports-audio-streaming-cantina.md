# 2026-08-15 — SESSION CLOSE 5: A/B validated, perf→B ports, JUCE streaming fix, cantina forensics

**READ FIRST (latest session-close; same day as CLOSE-4 — the marathon evening).** Four arcs:
the cold-singles A/B ran and DECIDED, a perf+smoothness port wave landed in Sais's tree, the
JUCE audio stall was root-caused and FIXED (third attempt, CONSULT-75 crew round), and the
"cantina band too quiet" mystery was chased through three data eras to a resolution of
*factory slider defaults*. Trackers: [SAIS-PR-QUEUE.md](../SAIS-PR-QUEUE.md) (has the new
ISSUE STACK + STOCK-DATASET ACCEPTANCE sections) · CONSULT-75 files in `../research/`.

## State

- **Our repo `master`: pushed through `845d5f8a7`; ONE local commit** `f29986864` (VOLSET seed
  logging) + this docs set uncommitted at close. Tree otherwise clean.
- **Sais branch `strict-data-defaults` @ `5d01cb63a`, pushed.** ~38 commits. stage-B-x64
  staged with that build (exe hash starts `69E3…`? — no: FINAL staged = the 5d01cb63a-equivalent
  build `D426…`-successor; the LAST staged exe is the streaming-fix build. gl11 unchanged
  `C2539CF3…` all day).
- **A staged**: 21:19 exe (VOLSET seed + shader inventory + sampler pre-warm + shared-read
  stall log). Matched set with 17:51 gl11 (inventory commit rebuilt both).
- **cfg state**: A curated (ilm_extract + override restored; `freeChaseCameraInteriorMaximumZoom=0`
  NEW — Kenny killed the interior zoom cap, retail feel). B: ILM_sound/ILM_music **UNMOUNTED**
  (Kenny: Legends preferences out), bake `false` (cache in use, 198 blobs), watchdog armed,
  budgets live. Both stage dirs have `client-stock.cfg` acceptance variants (UNRUN on B).

## Arc 1 — cold-singles A/B: DECIDED, fix shipped

- **[treefile.probe] never reached the log** — ExitChain LIFO ordering ran the log teardown
  before TreeFile::remove. Fixed `d55ae6d10` (probe emission = own ExitChain entry, prio 100).
- **A/B numbers**: manifest OFF realProbes **78,623** / negCache 88,328 (52.9% avoided) → ON
  realProbes **1,083** / manifestSkips 166,686 (**99.4%** avoided, −98.6% syscalls). Zero
  missing-file warning deltas. **SHIPPED.**
- Wall-time: stalls unchanged in magnitude — probes were gone but the same ~700 ms remained as
  real I/O under `SpacePreloadedAssetManager::load` → **the banked async-loader manifest-miss
  design is RETIRED** (wrong path — that stall never touches AsynchronousLoader::add).
- **Stall sampler pre-warm** `845d5f8a7`: symbolization cold-start (689 ms PDB load) ran INSIDE
  the first stall → longest stall of every session got 1 sample. Pre-warmed on the watchdog
  thread; also made the worst stall SHORTER (721→533 ms — the profiler was inflating what it
  measured). Also: stall log now `_fsopen/_SH_DENYWR` (readable live) — BOTH trees.
- Duplicate texture creates (~430 ms/session, 420-460 repeats, font atlas ×5) — OPEN, untouched.

## Arc 2 — perf + smoothness ports A→B (Kenny mandate: "any relevant improvements", ONE giant PR; advertise = PR #2)

Landed on his branch (all built clean, staged, pushed):
`3937fe234` floor-seam graze (door-snap FLOOR half — Kenny verified fixed) · `352a68488`
async-loader budget · `c23498847` interior-layout spread · `261522e5f` WorldSnapshot drain
budget · `620ce5d2b` stall watchdog+sampler · `78c9697fe` TreeFileFactory buffering ·
`4856bcf4f` Texture one-read · `0da2d4b7c` budgeted terrain preload · `2ef1a907e` shared-read
stall log · `5d01cb63a` audio streaming (arc 3).

⛔ **CAMERA: NOTHING from A goes to B** (Kenny final): his snap-back is CORRECT; our smoothing
(pull-in rate limit + convergence + interior zoom cap) is what lets the camera exit walls.
Zoom cap was ported then REVERTED same hour; also disabled in A via cfg. Stock client
reproduces the terminal-oscillation → 20-year-old behaviour, NOT ours to fix.
**STILL TO PORT**: phased WorldSnapshot load (CONSULT-60 — hard hand-port, his file 1467 vs
our 3272 lines) → then the music-fade non-blocking change (GATED behind it — its safety
argument depends on the budgeted load work). dPVS portal fixes still blocked on the
cellLoaded-parenting conflict; Kenny SAW foyer see-through in B → needed eventually.

## Arc 3 — JUCE audio: stall root-caused, streaming fix landed (CONSULT-75)

- Watchdog convicted the cantina-door hitch: `AIL_open_stream` decoded ENTIRE MP3s inline on
  the game thread (130 ms band track, 454 ms zone theme, re-paid per restart).
- **Attempts A/B (async decode + deferred start) FAILED** — engine snapshots sample METADATA at
  open (`Audio.cpp:3031` reads playbackRate ONCE; `Sound2d.cpp:397` re-applies it EVERY frame).
  Pending rate 0 → mixer 0.0625× floor → title music 4 octaves down = Kenny's "truck".
- **CONSULT-75** (Codex engine-trace + Cursor state-table + Sonnet lateral): engine NEVER
  abandons a slow stream; shim has no pending state; only startSample enters PLAYING;
  AIL_start_sample discards failures. Files in `.planning/research/CONSULT-75-*`.
- **Fix `5d01cb63a` — header-first**: metadata correct at open (header parse, ~1 ms), full
  zeroed buffer never resized, first second decoded inline, worker fills chunks behind the
  cursor, fillToken guards release/reassign. Opens now 0.1–1.4 ms; door-stall class GONE from
  the watchdog; login/zone/band all correct. `audio-decode.log` = live instrument (shared-read).
- **JUCE hardening still open**: crackling (mixer/callback — Kenny's original report, untouched),
  no decoded-sample reuse (61× re-decodes of one WAV observed — cheap but wasteful),
  ~26-93 MB float PCM resident per long track (real block-streaming = mix-loop rewrite, later),
  `AudioFormatManager` cross-thread createReaderFor (undocumented safety — watch for audio crashes).

## Arc 4 — shaders: corpus verdict + bake + inventory

- **`shader-asm-fallback.txt`** (new, A `98da87786`): Release-visible worklist of //asm VS that
  used the generic fallback. Curated run: **3** (cloudlayer, gradient_sky, tf). Stock run:
  **12** (= those 3 + exactly our 9 hand-authored overrides). **His corpus covers 12/12** (96
  VS vs our 9) → **adopt his corpus wholesale; ours is a strict subset with gaps**; our
  override layer becomes redundant once his mounts. Fallback stays as thin safety net.
- **His shader cache works**: bake run wrote 198 blobs + manifest; warm run = 50 ms compile
  total (was 3435 ms), peak frame 0.0 ms D3DCompile. Bake = config step reviewers won't know →
  document or default it (tracker).
- **PR blocker stands**: his repo ships ZERO shader files; corpus + asm2hlsl generator live
  outside it. Ship generator (recipe-not-bytes) per tracker. Stock-acceptance cfgs written both
  sides; **B's stock run (refusal count) still UNRUN** — prediction on record: 12 refusals.

## Arc 5 — cantina forensics (the fun one)

- **Epic-music-everywhere = Legends feeding the dormant NGE dynamic-music system**: stock
  GameMusicManager tables reference 122 combat/reveal/victory tracks that the shelf dataset
  doesn't ship; `ILM_music.tre` supplies them → system wakes up. UNMOUNTED from B (with
  ILM_sound) — Kenny: Legends preferences out, "ILM = content, not opinions".
- **Band-too-quiet: RESOLVED = mixing console.** Chased through: template volumes (1.0 in
  pre-CU/SWGEmu-D:-drive, retail-final-C:, v3.0 — byte-identical across 20 years), duck chain
  (bg=1.000 measured), sliders (CATVOL), source mastering (band mp3 −20.4 dB vs ambience
  −34.3 dB — band is HOT). Actual cause: Kenny's master was floored to escape loud effects +
  **SOE factory defaults** (music 0.75, playerMusic 0.5, ambient/effects 1.0, Audio.cpp:6001).
  Sliders re-mixed → band audible. Banked: saner distribution defaults tweak.
- Retail-final install (`C:/Program Files (x86)/StarWarsGalaxies`, 128 tres: 63×T5000+59×T6000
  +6×T4000) and pre-CU set (`D:/SWGEmu-Client/SWGEmu`, 53 tres) both catalogued as reference
  data; T6000 direct-mount is FATAL — TOC surgery needed if ever booting them.

## Late-evening addendum (post-close-draft, ~21:45-22:10)

- **Dropout metric landed** (`2f7fe8f9b` + unthrottled/timestamped `0fb0f71af`): the device
  callback plays SILENCE for any block where try_to_lock loses; every miss is now counted and
  logged per-event with timestamps. THE instrument for the crackle complaint — Kenny's
  "tiny pops most wouldn't notice" are now measurable (his ear was right all along).
- **Login crackle burst FIXED** (`21816bc66`): ~98 cached sample decodes each held s_mutex for
  the whole decode → ~60 missed blocks (~18%) in the first 3 s of every launch. Decode moved
  OUTSIDE the lock (pure decodeEncoded + locked applyDecoded — nothing decodes under s_mutex
  anywhere now). Measured 60 → 2 misses; ear-confirmed.
- **The drip remains**: ~1 miss / 5-10 s in play (0.13% of blocks) from per-frame lock traffic
  (volume/rate sets per sound per frame + AIL_serve walks). DESIGNED FIX queued: atomic float
  stores for the hot setters (no lock) + repeat-last-block instead of silence on residual
  misses; command-queue redesign is the long-term "adoption done properly" version. Audibility
  is load- and masking-dependent (a miss during near-silence is inaudible) — but A-clean is
  the bar and it is now VERIFIABLE (lockMisses ≈ 0 + ear sign-off).
- **dPVS see-through captured in B** (screenshot in tracker entry): foyer portal showing raw
  sky/terrain, Mos Eisley 3457,5,-4844. The parked 6-fix port is now field-evidenced; the
  cellLoaded-parenting conflict investigation is its entry ticket.
- Sais branch closes the night at `0fb0f71af` (~42 commits), all pushed.

## ISSUE STACK (tracker has detail)

1. **Login-position regression** (Kenny report): spawned at starport instead of cantina.
   TWO CONFOUNDS FIRST: multi-client last-writer-wins (A+B share the character) and the
   return-from-space starport mechanic. Test: single-client relog.
2. Ship-flyby loudness / distribution default mix (taste knob, banked).
3. Duplicate texture creates (~430 ms/session, stable across runs).
4. VOLSET seed probe armed in A — next login prints the band's full volume arithmetic
   (band resolved, but the probe validates the chain).

## Open board (next session, in order)

1. B stock-dataset acceptance run (`client-stock.cfg` swap) → refusal count vs prediction 12.
2. Phased WorldSnapshot load port → then music-fade port (gated).
3. JUCE crackling (mixer/callback audit — the remaining approved-adoption blocker).
4. PR #1 body refresh (branch grew ~16 commits today: c23→~38).
5. Login-position discriminating test (single-client).
6. Push A when Kenny says (1 commit + docs).
