# CONSULT-62 task (Cursor) — Miles 9.3b internals byte-map: the serve path and its failure modes

FIRST read `.planning/research/CONSULT-62-audio-residual-crackle-EVIDENCE.md` in this repo — its facts are LOCKED ground truth.

Your angle: the vendored Miles source at `D:\Code\milesss-v9.3b` (read-only). Produce a precise mechanism map with file:line for every claim:

a. `rrThreadSetPriority(..., rrThreadOneHigherThanCurrent)` — what actual Win32 thread priority results, and what calling it twice does.
b. `rrThreadSetCPUCore(&s_TimerThread, 1)` — hard affinity (SetThreadAffinityMask) or soft/ideal-processor hint? What happens when logical core 1 is busy.
c. `rrSemaphoreDecrementOrWait(&s_CloseSem, 1)` — the underlying wait primitive and its timeout granularity semantics.
d. Which public AIL_* entry points take `s_MilesMutex` (InMilesMutex/AIL_lock_mutex) and can hold it longer than ~10ms — walk AIL_open_stream, AIL_set_named_sample_file, sample start/allocate, ASI(MP3) codec init/seek, stream service. Identify the SLOWEST work that can occur under that mutex (disk? decode? memory?).
e. SS_serve per-wake behavior (genericdig.cpp): how much audio it mixes per call at 16ms and 64ms DIG_DS_MIX_FRAGMENT_CNT with a 256ms ring; where MP3 stream decode happens (which thread, under which lock); and EXACTLY what is audible when serves are delayed or skipped (silence gap? stale ring replay? torn buffer?) — trace the fill/wrap arithmetic.
f. Stream servicing cadence (mssstrm.cpp service_stream / background processing): how stream chunk refills interleave with SS_serve on the single MSSTimer thread, and what happens to the mix when a refill or decode runs long within one API_timer() pass.

Finish with: the precise condition(s) most likely to produce (a) a 1-2s burst of minor crackle while many sounds/streams are being STARTED (zone-in), and (b) a rare isolated crackle when the main thread takes a 20-100ms hitch (D3D9 shader compile) — given AIL_MM_PERIOD=1 and the ThreadProc skip-on-mutex-timeout logic quoted in the evidence file.
