# Evidence: streamed music silent at idle screen (Miles 9.3b), treat as GIVEN

A 2003-era SW Galaxies client (engine in `src/engine/client/library/clientAudio/src/win32/Audio.cpp`)
was ported from **Miles Sound System 7.2e → 9.3b** (x64). Vendored headers at
`src/external/3rd/library/miles-9.3b/include/mss.h`; `mss64.dll` is MSS 9.3.21.0 (2015).

Do NOT re-derive these; they are measured facts (cdb breakpoints + the engine's own logging):

1. **Cached samples work.** UI sounds load the whole file into memory via
   `AIL_set_named_sample_file(handle, ext, buffer, size, 0)` then `AIL_allocate_sample_handle` +
   `AIL_start_sample`. They are AUDIBLE immediately at the login/character-select screen. So the
   digital driver + mixer output is alive.

2. **Streamed music is SILENT at the idle login screen**, but the SAME track becomes audible later,
   "halfway through the planet loading screen" (a heavy file-I/O phase). The track is
   `music/mus_title_lp.mp3` opened with `AIL_open_stream(dig, filename, 0)`.

3. cdb breakpoints on the live process at the login screen show, for that stream, IN ORDER:
   - `AIL_open_stream(...)` -> returns a NON-NULL HSTREAM
   - the engine's Miles file-read callback fires **exactly once** (8192 bytes) DURING the open
   - `AIL_auto_service_stream(stream, 1)` is called  (we added this)
   - `AIL_start_stream(stream)` is called
   - **then NOTHING**: for the entire time sitting at the login screen, the file-read callback never
     fires again -> the stream is never fed more data -> silent.

4. Stream data is read via Miles file callbacks set once at init:
   `AIL_set_file_callbacks(open, close, seek, read)`. The callback signatures match the 9.3b typedefs
   exactly (`MSS_FILE` == `char`; seek/read/close widths match). Cached samples do NOT use these
   callbacks (in-memory buffer); only streams do.

5. The app's only stream pump is `AIL_serve()`. It was historically wired ONLY to file I/O
   (`AbstractFile::setAudioServe(serve)`), i.e. serviced incidentally during file reads. We ALSO now
   call `serve()` every frame from `Audio::alter()` (which the main loop calls every frame regardless
   of scene). `Audio::serve()` gates on `Os::isMainThread() && s_digitalDevice2d &&
   s_prioritizedPlayingSounds.size()>0 && timer`, then calls `AIL_serve()` ~20Hz.

6. Despite (5), the read callback STILL does not fire after start at the idle screen (music still
   silent). Under 7.2e the same engine code played login music fine.

## The question
Why is a started, non-null, auto-service-enabled HSTREAM never serviced (no further file reads, no
audio) at an idle screen under MSS 9.3, while cached samples play — and what is the correct MSS-9.x
call/preference/threading setup to make `AIL_open_stream` playback work?

## Hard constraints
- MSS 9.3.21 x64, classic synchronous file callbacks (`AIL_set_file_callbacks`, not the async set).
- We can add MSS calls / preferences / a service thread in `Audio.cpp` install or the stream path.
