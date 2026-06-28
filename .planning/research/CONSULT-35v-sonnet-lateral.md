# TASK (Sonnet — lateral / out-of-the-box)

Read `D:\Code\swg-client-v2\.planning\research\CONSULT-35v-AXIOMS.md` first (LOCKED facts + a BANNED
falsified framing — honor both). Also read
`.planning\phases\35-miles-9-3b-audio-port\35-04-LOGIN-MUSIC-FOLLOWUP.md` (the proven mechanism + every
fix already tried and reverted — do NOT re-propose those).

**Your job:** Challenge the lead itself. Everyone assumes "9.3v fixes idle streamed music." Is that the
right tree to bark up? The 35-04 doc PROVES the failure mechanism is version-independent on its face:
`AIL_service_stream` drove our sync read callback ZERO times at idle; the stream got only the single
8 KB read `AIL_open_stream` does at open, so it starves; it "kicks in" only when heavy zone-load I/O
begins.

Generate lateral hypotheses for **why a Miles stream feeds during heavy I/O but starves at idle**, that
do NOT depend on the 9.3b-vs-9.3v version difference, plus a cheap test for each. Consider e.g.:
- Is our per-frame service / `Audio::alter`/`serve` loop even RUNNING at the login screen (pre-mainloop
  / different update pump than in-game)? If the login screen doesn't tick the audio service, NO Miles
  version would feed the stream — and zone-load "kick in" is just the mainloop finally ticking.
- Thread priority / timer: Miles' auto-service secondary thread needs CPU; at idle the login screen may
  spin a tight UI loop or sleep oddly, starving Miles' timer thread; heavy I/O changes scheduling.
- Does the title music actually reach `AIL_start_stream` at the login screen, or is playback deferred
  until a later state transition (i.e. it's a SEQUENCING bug, not a servicing bug)?
- The cached/buffered in-memory path works for everything else — is the title theme the ONLY thing on
  the stream path by accident (a size threshold), and would nudging that threshold be a smaller, safer
  fix than a Miles version swap (note: 35-04 fix #3 tried forcing the cached path and CRASHED on null
  sampleRawData — read why, and whether a DIFFERENT cached-path entry avoids that).

Rank your hypotheses by (likelihood × cheapness-to-test). For the top 2, give the exact probe (which
log line / cdb breakpoint / counter to watch) that would confirm or kill it WITHOUT needing 9.3v at all.
The goal is to make sure we don't burn effort adopting 9.3v if the real bug is elsewhere.
