Read these files in this repo (read-only):
- .planning/research/CONSULT-35-MUSIC-EVIDENCE.md  (measured facts -- treat as GIVEN, do not re-derive)
- src/engine/client/library/clientAudio/src/win32/Audio.cpp  (the engine audio layer)
- src/external/3rd/library/miles-9.3b/include/mss.h  (the MSS 9.3 API we link against)

YOUR ANGLE (trace/call-graph): Determine exactly how MSS 9.3 services an HSTREAM created by
AIL_open_stream. Specifically:
- Does AIL_serve() actually pump HSTREAM data in MSS 9.3, or does AIL_serve() only handle timers/
  samples while streams require AIL_service_stream() and/or a dedicated background service thread?
- What is the canonical MSS 9.x AIL_open_stream -> audible-playback init sequence (digital driver
  open flags/preferences, any required AIL_service_stream priming, AIL_start_stream, auto-service)?
- Given the evidence (open returns non-null, start called, auto_service(stream,1) called, yet the
  file READ callback never fires again after the single read during open at an idle screen), name the
  SPECIFIC missing call or preference, with exact MSS function names and mss.h line references.

Be concrete and surgical. The fix must be an MSS call/preference we add in Audio.cpp.
