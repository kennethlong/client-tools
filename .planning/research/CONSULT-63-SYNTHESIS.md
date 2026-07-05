# CONSULT-63 SYNTHESIS — Miles version delta + crackle-storm mechanism (2026-07-04)

Five consultants (Codex source-trace, Cursor header-delta, Sonnet version-strategy,
Opus mechanism, Fable blind-adversarial), all fed the neutral evidence pack
(CONSULT-63-miles-version-delta-EVIDENCE.md). Outputs in the sibling .out files.

## Version matrix (ground truth, measured)

| | Compiled/linked against | Runtime DLL | Notes |
|---|---|---|---|
| Retail SWG | Miles 7.x API | **mss32.dll 7.2a** | engine audio code written for this era |
| Our Win32 | miles-9.3b header+lib | **mss32.dll 9.3b** | consistent, no header/DLL mismatch |
| Our x64 | miles-9.3b header+lib | **mss64.dll 9.3v** | filters report 9.3g (mixed-vintage bundle) |

In-repo rollback asset: complete 7.2e SDK (header + Mss32.lib + full redist incl.
mssmp3.asi) at src/external/3rd/library/miles-7.2e/. Stale `miles/` (7.2a header) is
referenced by NO build config.

## CLOSED: the dead stream-EOS callback (Kenny's "maybe the end is handled differently" — YES)

Structural in 9.3b, confirmed independently by Codex (source) and Cursor (header+source):

- Non-preloaded streams: AIL_register_stream_callback only stores stream->callback;
  delivery requires stream->docallback, which is set ONLY in load_buffer_into_Miles()
  on an explicit zero-length terminal buffer (mssstrm.cpp:172-176), then delivered by a
  16 Hz background timer gated on autostreaming==2 && sample != SMP_PLAYING
  (mssstrm.cpp:211-229).
- ASI/MP3 natural decode exhaustion marks the SAMPLE SMP_DONE directly in the mixer
  (wavefile.cpp:11230-11234) — a path that never sets docallback for non-preloaded
  streams. The 7.2-era EOB-driven stream service was REMOVED in 9.3b (commented out,
  mssstrm.cpp:814).
- Sonnet's changelog dig: Miles 8.0g = "all new IO and streaming system" rewrite — the
  version where this class of behavior changed; multiple AIL_stream_status fixes through
  9.0c postdate 7.2e.

VERDICT: our SMP_DONE poll (landed c994f74e8) is the CORRECT mechanism, not a workaround.
The callback registration stays harmless. Item CLOSED.

## STORM: convergence-from-divergence on one mechanism family

All four analytical consultants independently landed on: **underrun of the in-game
mix-ahead cushion (16ms) during the ~1s stream-prime window after a track transition.**
They split only on WHERE the overrun originates:

- **Variant A (Codex #1, Opus M1 ~60%): mixer-thread ASI decode burst.** Stream prime
  fills COMPRESSED bytes only — the new stream starts with an empty PCM reservoir, so
  the first ~1s of SS_fill passes carries extra MP3 decode; a one-shot MP3 started in
  that window forces the SAME pass to also init+first-decode a new ASI stage
  (wavefile.cpp:12023-12047) → pass exceeds fragment budget → underrun crackle.
  Explains: idle-clean, +8s-clean, ~1s duration (reservoir build), and 0ms app calls
  (cost is on the MSSTimer thread, structurally invisible to app-side probes).
- **Variant B (Fable): main-thread AIL_lock holds.** TreeFile I/O inside the stream-open
  path + one-shot codec init run under the batch-wide Miles lock hold; mixer skips its
  pass on a failed 0+10ms mutex wait. Fable's instrumentation attack: our "0ms" call
  timings are GetTickCount-floor blind below ~15.6ms and NOTHING measured the lock-HOLD
  duration.
- **Version framing (Cursor):** retail 7.2a stock = 8ms×96 ≈ 768ms ring / 64ms mix-ahead,
  3-chunk streams; 9.3b stock = 1ms×256 = 256ms ring / 48ms mix-ahead, 8-chunk async
  streams. The retail client had ~3x the cushion around every transition; the engine's
  buffering intuition (comments still say "one second") predates the 8.0g rewrite.

Eliminations audit (Fable): the "mix-ahead eliminated" item only tested the LOAD premix
(192ms) and a 32ms in-game raise — the in-game cushion at the storm moment was never
raised into the 100-200ms band, so the underrun family was never actually eliminated.
The STARVED "slot-reuse artifact" dismissal is circular (same log line either way).

Refuted as primary: Miles-mutex contention as a standalone cause (per-sound lock cycling
made it WORSE — opposite prediction); slot-reuse corruption; IO/refill starvation.

## Decisive next wave (one diagnostic build discriminates everything)

1. **QPC lock-hold probe:** QueryPerformanceCounter timer around Audio::lock()/unLock()
   (batch start path, Audio.cpp:2773/2790) + QPC-based call timings; log holds >8ms.
   Storm-coincident long holds → Variant B; silence → Variant A. Retires the 0-15ms
   instrument blindness permanently. Config-gated, ship-safe.
2. **In-game mix-ahead DIAGNOSTIC raise** (config-gated override, 100-200ms, one session
   only — round-6 proved >16ms is a shippable-latency non-starter): storm vanishes →
   engine-schedulable underrun confirmed; persists → Miles-internal corruption confirmed
   against the strongest counter-account.
3. **PCM door-sound swap** (optional, isolates Variant A's decode-burst specifically):
   re-encode the door one-shot as PCM/ADPCM in a loose override; storm gone = ASI
   first-decode convicted.
4. **Escalation (if needed): instrumented mss32.dll** from D:\Code\milesss-v9.3b —
   log SS_fill per-pass wall time + dig->asi_times[] + active-sample count, and the
   SS_serve dropout/re-prime counter (mssdig.cpp:1851). One session adjudicates A vs B
   vs Miles-internal.

## x64 test sequencing (Sonnet's confound catch)

The cantina door is NAMED in commit 984afc073 as a 3D sound the 9.3v swap fixed — an
x64 "storm gone" with that trigger is not clean evidence. Sequence:
(1) confirm door SFX 2D vs 3D authoring; (2) shipped x64 (9.3v) repro; (3) pure DLL swap
to 9.3b redist64 (stage-x64/_miles93b_bak exists) — same exe, isolates 9.3b-vs-9.3v
independent of bitness; (4) repeat with a known 2D trigger.

## Rollback option (32-bit → 7.2e): FEASIBLE, on file

All 77 AIL_* symbols used by clientAudio exist in the 7.2e header; file-callback
typedefs byte-identical. Work: include-path + lib-path edits in clientAudio/SwgClient
vcxprojs (Win32 configs only) + stage a 7.2e redist set + AUDIT every AIL_set/get_
preference call (pref IDs are renumbered across versions — DIG_3D_MUTE_AT_MAX 46↔11
with flipped default is the proven exhibit; compile-time symbols handle it IF built
against the matching header). Reserve lever — decide after the diagnostic wave.

## Side flags (Fable)

- endOfSample2dCallBack walks engine maps on the Miles thread while main-thread release
  paths may erase outside Audio::lock — UAF-class, needs an audit pass (CONSULT-56 class).
- Active cfg verified clean: streamBufferBytes=0, titleMusicStreamFix not re-armed.

---

# OUTCOME (07-04/05 same night — arc CLOSED)

1. **A/B conviction (x64, same exe, DLL swap only): 9.3b = storm, 9.3v = no storm.**
   The storm was a Miles 9.3b DLL defect. Engine exonerated; Fable's engine-side
   account and the underrun-family mechanism analyses are moot for the storm itself
   (kept above as the record of what the evidence could not exclude pre-A/B).
2. **No 32-bit 9.3v exists** (machine-wide scan; Restoration's own 32-bit ships 7.2a)
   → Win32 rollback executed.
3. **7.2e rollback detour:** the in-repo miles-7.2e SDK redist mss32.dll is a DUD —
   standalone-harness-proven to fail MP3 ASI provider discovery with ANY provider set
   (retail 7.2a core works with ANY set, including 7.2e's providers). Do not stage it.
4. **7.2e bring-up found + fixed a 20-year engine bug:** getSampleTime() leaked its
   throwaway duration-probe sample handle on every set_named_sample_file failure
   (release was success-gated) — drained all 64 driver handles at boot = total
   silence. Invisible on 9.3x where the bind never failed.
5. **END STATE:** Win32 runs the vendored retail **7.2a** runtime
   (src/external/3rd/library/miles-7.2a/redist, postbuild-staged), compiled/linked
   against the 7.2e SDK (MilesCompat shim bridges room_type bus_index; premix setters
   RAD-stock on 7.x — 8ms fragments). x64 runs **9.3v** (9.3b header/lib).
   **Kenny verified 3×: music plays, door-at-transition storm GONE on Win32-7.2a.**
   Combined with x64-9.3v clean: storm eliminated on both platforms.
