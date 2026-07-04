You are consulting on in-game audio failures in this repo (D:\Code\swg-client-v2).
Read the shared evidence first — measured ground truth, treat as GIVEN:

  .planning/research/CONSULT-61-audio-popping-EVIDENCE.md

YOUR ANGLE (repo tracing of OUR Miles file-callback layer — other consultants own the
Miles SDK async internals, the late-sample-start class, and lateral pop mechanisms):

Target: src/engine/client/library/clientAudio/src/win32/Audio.cpp — the
fileOpenCallBack / fileCloseCallBack / fileSeekCallBack / fileReadCallBack
implementations and the title-music-fix state machine around them
(s_lastAudioOpenName, s_streamResumeOffset, s_substitutedHandleMap,
s_titleMusicStreamFix — roughly :3990-4150, plus wherever those statics are declared
and reset).

1. MULTI-STREAM CORRECTNESS: Walk the state machine for the measured zone-in sequence:
   title stream closes; then mus_theme_tatooine.mp3, amb_desert_lp.wav, and shortly
   after two cantina ambiences + mus_figrin_dan_song_2.mp3 open CONCURRENTLY, each
   issuing chunk reads from Miles' async IO thread through these callbacks. The
   single-slot globals were built for ONE title stream. Enumerate exactly what happens
   with interleaved open("realname A"), open("realname B"), open("") re-opens, reads,
   seeks, closes from multiple streams: can stream A's re-open be served stream B's
   name/offset? Can a read return data from the wrong file or wrong offset? Give the
   precise interleaving that leaves ONE stream's first chunk read never completing or
   permanently wrong (the measured mus_theme_tatooine death) with file:line.

2. THREADING: Which threads run these callbacks (main thread for cached-sample loads,
   Miles async IO thread for stream chunk reads, Miles timer thread?)? Is the fix
   state accessed from multiple threads without synchronization (torn strings /
   racing offset updates)? File:line.

3. SEEK/READ CONTRACT: Miles streams SEEK (chunk reads are positioned). Does
   fileSeekCallBack + the s_streamResumeOffset sequential-read tracking honor seeks
   correctly for MULTIPLE concurrent handles, or does the resume-offset logic assume
   one sequential reader? What does a wrong offset produce downstream (garbage
   compressed data → decoder glitch = POP hypothesis)?

4. SCOPE CHECK: The empty-name re-open shim exists because Miles re-opens the CURRENT
   stream with an empty name. Confirm from the vendored SDK (search AIL file callback
   open calls in mssstrm.cpp/milesasync) WHEN Miles does empty-name re-opens — per
   stream open? per chunk read? only for certain paths? — so we know how often the
   shim actually fires with multiple streams.

Deliverable: findings with file:line per question, then a verdict: is the one-slot
callback state the mus_theme_tatooine killer and/or the pop source, and the
minimal-diff correct shape (per-handle state map keyed on the return of
fileOpenCallBack? per-thread? something else).
