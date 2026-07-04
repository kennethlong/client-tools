# CONSULT-62 EVIDENCE — residual audio crackle (zone-in 1-2s both renderers; rare in-game on D3D9 only)

Treat everything below as measured ground truth ("LOCKED"). Do not re-derive or dispute it.

## Platform
- SWG client fork, Win32 x86 Release, Windows 11 Home 26200. CPU: Intel i5-12400F — 6 physical P-cores, 12 logical (SMT), NO E-cores.
- Audio: Miles Sound System 9.3b, vendored SOURCE at `D:\Code\milesss-v9.3b` (readable). The active output backend is `win\sdk\src\sdk\genericdig.cpp` (RadSoundSystem); the DirectSound path in `mssdig.cpp` is dead code in this build.
- Output format: 22050 Hz, 16-bit, stereo (engine hardcodes getFrequency()=22050).
- Engine audio layer: `src/engine/client/library/clientAudio/src/win32/Audio.cpp`.

## Mixer service architecture (from the vendored Miles source)
- Output ring: TotalMs = DIG_DS_FRAGMENT_SIZE(1ms) x DIG_DS_FRAGMENT_CNT, read once at device open (genericdig.cpp:339). This build uses the DEFAULT 256ms ring.
- Mix-ahead: DIG_DS_MIX_FRAGMENT_CNT. In-game = 16 (ms). During scene load = 64, restored at load end. (Set via Audio::setLargePreMixBuffer/setNormalPreMixBuffer.)
- SS_serve (the mixer fill) is registered as an AIL timer (genericdig.cpp:746) and driven by a dedicated thread "MSSTimer":
  - Created at genericmss.cpp:740-743: `rrThreadCreate(&s_TimerThread, ThreadProc, 80*1024, 0, thread_flags, "MSSTimer");` followed by `rrThreadSetPriority(&s_TimerThread, rrThreadOneHigherThanCurrent);` called TWICE (743-744 in source; both identical).
  - genericmss.cpp:747: `rrThreadSetCPUCore(&s_TimerThread, 1);` — pinned to logical core 1.
  - AIL_MM_PERIOD preference = DEFAULT_AMP = 1 (mss.h:762-763; the comment says "5 msec" but the value is 1).
- ThreadProc main loop, VERBATIM (genericmss.cpp:454-484):

```c
for(;;)
{
   if ( rrSemaphoreDecrementOrWait(&s_CloseSem, (U32)AIL_preference[AIL_MM_PERIOD]) )
     break;

   if (rrMutexLockTimeout(&s_MilesMutex, 0) == 0 )
   {
     if ( rrMutexLockTimeout(&s_MilesMutex, 10) )
       goto gotit;
   }
   else
   {
    gotit:
     API_timer();          // runs all registered timers incl. SS_serve
     rrMutexUnlock(&s_MilesMutex);
   }
}
```
  (Note the control flow: if the 0ms try fails AND the 10ms retry fails, the wake does NOTHING — that service period is skipped. `InMilesMutex()`/`AIL_lock_mutex()` take the same `s_MilesMutex` from app threads.)
- Miles async file IO runs on a separate Miles-owned IO thread (milesasync.cpp) using engine-registered file callbacks (TreeFile-backed). The engine's callback lock covers only its handle-map state; all file I/O in the callbacks runs unlocked (fixed 2026-07-04).
- Streams: AIL_open_stream mem=0 → ~1s of compressed data in 8 chunks; music is 22kHz MP3 streams.

## Already fixed and VERIFIED this arc (treat as given; do NOT propose these)
1. timeBeginPeriod(1) at process start (was: nothing but a timeBeginPeriod(10) in a UI lib). This produced a LARGE improvement: char-select crackle GONE both renderers, D3D9 in-game went from "much worse, frequent crackle" to "a couple of minor crackles".
2. Miles file-handle 0 never issued (Miles treats handle 0 as "unopened"); legacy one-slot re-open shim off.
3. Engine file-callback lock narrowed to map ops only (no disk I/O under it).
4. Ring/premix at stock (256ms ring; 16ms in-game / 64ms load mix-ahead).
5. Main-thread load stalls largely eliminated: phased WorldSnapshot parse (40ms budget), budgeted terrain preload, async-loader callback budget 6ms/frame, loading-pump slices 50ms, lazy environment-block realization. A per-frame stall watchdog (100ms threshold) writes minidumps; during the zone-in window there remains a burst of ~100-900ms main-thread stalls (first-draw GPU resource creates and one deliberate 1s Audio::alter+Sleep(5) music-fade pump loop in the SCENE_CHANGED handler). Mid-play, frames are ~10-30ms with rare 100-150ms one-offs.
6. Fresh audio streams are healthy (probe logs starvation edges + >0.05 one-frame volume steps + 500ms summaries into stage/audio-diag.log).

## Current symptom matrix (build of 2026-07-04 2:18 PM, user-verified)
- Zone-in (loading screen -> world): ~1-2 seconds of MINOR crackle on BOTH D3D9 (gl05) and D3D11 (gl11) clients.
- In-game after settling: D3D11 clean; D3D9 has occasional single minor crackles ("much better" than before the timer fix).
- Char select: clean on both.
- Renderer difference facts: gl11 has a VS bytecode disk cache (no in-game compiles); Win32 gl05 compiles shaders through D3DX synchronously on the MAIN thread when new shaders appear in-game. The NVIDIA D3D11 driver stack raises OS timer resolution as a side effect; D3D9 does not (this was the pre-fix gl05-much-worse mechanism, now equalized by fix #1).
- During the zone-in crackle window the engine: starts many sounds (planet theme MP3 stream, 1-2 ambient WAV loop streams, one-shot UI/object samples), creates GPU resources on first draw, runs loader threads (disk + zlib inflate) on multiple cores, and pumps Audio::alter from a Sleep(5) loop for ~1s.

## Open question for this round
What mechanism(s) explain (a) the 1-2s renderer-agnostic minor crackle at zone-in and (b) the rare D3D9-only in-game crackles, given the fixes above — and what is the cheapest decisive probe or fix for each?
