You are consulting on a game-audio investigation. Read the evidence pack first:
.planning/research/CONSULT-63-miles-version-delta-EVIDENCE.md
Everything in it is measured ground truth — treat as given.

This repo vendors two Miles Sound System headers side by side:
  src/external/3rd/library/miles-7.2e/include/mss.h   (the retail-era API, 2008)
  src/external/3rd/library/miles-9.3b/include/mss.h   (what we compile+link+run today)

Your task — a precise API/semantics DELTA MAP between the two headers, with line
references in both files, focused on what the engine actually uses:

1. Stream API: HSTREAM/stream struct visibility, AIL_open_stream, AIL_start_stream,
   AIL_close_stream, AIL_pause_stream, AIL_stream_status (status/state enum VALUES —
   did SMP_DONE's numeric value or meaning change?), AIL_set_stream_loop_count,
   AIL_register_stream_callback and its callback typedef (signature AND calling
   convention), AIL_service_stream / any periodic-service requirement notes.
2. Sample API: AIL_allocate_sample_handle, AIL_init_sample (arg changes!),
   AIL_set_named_sample_file, AIL_start_sample, AIL_set_sample_volume* (7.2 used
   AIL_set_sample_volume; 9.3 uses volume_levels — semantic differences),
   AIL_register_EOS_callback / AIL_register_EOB_callback deltas.
3. Digital driver + file I/O: AIL_open_digital_driver vs 7.2-era waveOut open path,
   DIG_* preference/env values (which existed in 7.2e vs new/removed in 9.3b),
   AIL_set_file_callbacks / async file callback deltas.
4. Then cross-check EVERY AIL_* call made in
   src/engine/client/library/clientAudio/src/win32/Audio.cpp and Sound2d.cpp:
   list each call whose semantics, defaults, units, or required companion calls
   changed between 7.2e and 9.3b. Flag any call pattern that was correct against
   7.2e but is wrong or incomplete against 9.3b — especially anything relevant to
   end-of-stream callback delivery.

Report: per-area delta tables with file:line for both headers, then a short list of
"stale 7.2-era assumptions in this engine's Miles usage", ranked by relevance to the
evidence pack's locked anomaly (stream EOS callback never fires) and storm symptom.
Mechanism analysis only; do not propose fixes.
