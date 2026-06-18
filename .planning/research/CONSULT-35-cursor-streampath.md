Read these files in this repo (read-only):
- .planning/research/CONSULT-35-MUSIC-EVIDENCE.md  (measured facts -- treat as GIVEN, do not re-derive)
- src/engine/client/library/clientAudio/src/win32/Audio.cpp
- src/external/3rd/library/miles-9.3b/include/mss.h

YOUR ANGLE (detailed byte-level code read): Walk the EXACT stream code path in Audio.cpp from
AIL_open_stream through AIL_start_stream and the per-frame serve, line by line. Compare against the
MSS 9.3 stream API surface in mss.h (AIL_open_stream, AIL_service_stream, AIL_auto_service_stream,
AIL_start_stream, AIL_stream_status, AIL_set_stream_*, plus any digital-driver preference enums like
DIG_* that affect streaming/threading). Find the concrete discrepancy: a missing/mis-ordered call, a
required preference not set, a parameter that changed meaning between 7.2e and 9.3b (e.g. the
stream_mem arg to AIL_open_stream), or a needed AIL_service_stream(stream,1) prime. Cite exact lines
in Audio.cpp and mss.h. Propose the minimal concrete code edit.
