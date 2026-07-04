You are consulting on in-game audio failures in this repo (D:\Code\swg-client-v2).
Read the shared evidence first — measured ground truth, treat as GIVEN:

  .planning/research/CONSULT-61-audio-popping-EVIDENCE.md

YOUR ANGLE (byte-level Miles SDK async-stream internals; the SDK source is at
D:\Code\milesss-v9.3b\win\sdk\src\sdk — other consultants own our engine callback
layer, the late-sample-start class, and lateral analysis):

1. DEAD STREAM MECHANICS: In mssstrm.cpp + milesasync.h/.cpp, enumerate every way a
   stream's first chunk can PERMANENTLY fail to reach the sample, i.e.
   `load_buffer_into_Miles` never fires `AIL_load_sample_buffer` (measured:
   SAMPLE::starved never cleared for 40s on mus_theme_tatooine.mp3 while sibling
   streams thrived): async read never completes (lost/error), `asyncs_loaded[i]`
   stuck != 2, `s->error` set (who sets it, is it silent?), `playcontrol`,
   `primeleft` never satisfied (prime = bufsize*(CHUNKS-1) with our 1MB pool =
   ~896KB — trace `start_IOs_if_we_can` + handle_asyncs: does priming issue all 8
   chunk reads at once? what if ONE errors?). File:line for each path, and which are
   silent (no error surfaced).

2. WHAT WOULD RECOVER IT: For each death path — does ANYTHING retry (the 16Hz
   background timer keeps calling load_buffer_into_Miles — what condition would it
   need)? Is there an API-visible signal we could poll to detect + restart a dead
   stream (AIL_stream_status stays 4 per the log — useless; stream->error? position
   not advancing?)?

3. POPS IN A HEALTHY STREAM: mus_figrin_dan_song_2.mp3 played with starved=0 but
   audible pops. With a 1MB pool = 8 x 128KB chunks of an ASI-decoded .mp3, examine
   the chunk-boundary path: reset_ASI / AIL_request_EOB_ASI_reset usage
   (mssstrm.cpp:149-155), when EOB resets are requested for big chunks vs small, loop
   handling for looping streams, and whether large chunks change decode continuity vs
   Miles' stock 1-second/8 chunks. Could 128KB chunks CAUSE periodic pops (predict
   cadence: 128KB of ~16-32KB/s mp3 = one boundary every 4-8s — does that match "a
   pop every few seconds"?). Also check AIL_set_stream_loop_block / loop wraps.

4. PRIME/START LATENCY: With stream_mem=1MB, primeamount ~= 7/8MB before
   AIL_start_sample fires (mssstrm.cpp:162-166, :792-793). Confirm, and compute what
   that does to STREAM start latency (music/ambience fade-ins arriving late) at
   realistic read speeds through a contended file layer. Is there a way to cap
   primeamount independently of pool size (a field? a preference?), so a big pool
   does not delay starts?

Deliverable: findings with file:line per question, then a table: death paths (which
silent), pop mechanisms ranked with predicted cadence, and the recommended
stream_mem/prime configuration.
