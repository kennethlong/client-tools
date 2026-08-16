# CONSULT-75 — evidence pack (treat every item as GIVEN; do not re-derive)

A Star Wars Galaxies client (C++/MSVC, x64). Its audio backend is a shim that implements the
Miles Sound System C API (`AIL_*`) on top of JUCE 8. Two files matter:

- `src/engine/client/library/clientAudio/src/win32/Audio.cpp` — ENGINE side, calls `AIL_*`
- `src/engine/client/library/clientAudio/src/win32/JuceMiles.cpp` — the JUCE shim (~1560 lines)

Repo root: `D:/Code/Galaxies-Reborn/swg-source-x64-dx11`, branch `strict-data-defaults`.

## Measured facts

1. `Audio.cpp` classifies each 2D sound. If `sampleSize > Audio::getMaxCached2dSampleSize()` it
   calls `AIL_open_stream(driver, path, 0)` and stores a `SampleStream` with
   `m_status = Audio::PS_notStarted`. Otherwise it calls `AIL_allocate_sample_handle` +
   `AIL_set_named_sample_file`.
2. In the shim, `AIL_open_stream` currently: reads the whole file, allocates a sample handle,
   calls `AIL_set_named_sample_file`, which calls `decodeAudio()` — a FULL synchronous decode of
   the entire asset into `SampleState::pcm` (a `juce::AudioBuffer<float>`) — then returns HSTREAM.
3. This works correctly. Its cost, measured on the game thread:
   - `music/mus_theme_tatooine.mp3`  262.6 s audio, 4.20 MB -> 454.2 ms
   - `music/mus_title_lp.mp3`        115.8 s audio, 1.85 MB -> 202.7 ms
   - `sample/amb_desert_lp.wav`       12.8 s audio, 1.13 MB ->   1.6 ms
   WAV decodes are ~0 ms; only MP3 music is expensive. Stall-watchdog stacks confirm the hitch is
   `Audio::alter -> Audio::startSample -> AudioNamespace::createSampleId ->
   AIL_set_named_sample_file -> AIL_open_stream -> decodeAudio -> juce::MP3Decoder::...`
4. `AIL_start_stream(s)` -> `AIL_start_sample(sampleHandle)` -> `startSample(SampleState&)`.
   `startSample` currently REFUSES if `pcm.getNumSamples()==0 || totalFrames==0`.
5. `AIL_stream_status(s)` -> `AIL_sample_status(sampleHandle)` -> returns `sample.status`.
6. The mixer (`mixSample`) skips any sample where
   `status != SMP_PLAYING || totalFrames == 0 || pcm.getNumSamples() == 0`.
7. The JUCE audio device callback (`DriverState::audioDeviceIOCallbackWithContext`) takes the
   global `s_mutex` with **`std::try_to_lock`** and, if it cannot acquire it, clears the output
   and RETURNS (silence for that block).
8. The mixer reads `pcm` by random access with a fractional cursor and linear interpolation, and
   supports arbitrary loop start/end derived from encoded byte offsets, plus seeking
   (`AIL_set_sample_ms_position`).

## The two failed attempts (both produced NO MUSIC; WAV ambients kept working)

Attempt A: `AIL_open_stream` copies the encoded bytes and hands the decode to a detached
`std::thread`; on completion it locks `s_mutex`, re-looks-up the handle, compares a per-decode
token, then moves the decoded buffer in. While pending, `status` was left at `SMP_DONE`.
Result: no music.

Attempt B: identical, plus `startSample` on a pending decode records `startWhenDecoded = true`,
sets `status = SMP_PLAYING` immediately, and returns true; the completion path applies the buffer
and then calls `startSample` for real. Result: still no music. WAV ambients unaffected.

Reverting to the synchronous decode restores music exactly.

## The question

What does the ENGINE require between `AIL_open_stream` returning and music becoming audible, such
that a stream whose PCM arrives ~200-450 ms late is silently abandoned? Name the specific
mechanism and the evidence for it.
