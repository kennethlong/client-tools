# TASK (Cursor — detailed binary + doc reader)

Read `D:\Code\swg-client-v2\.planning\research\CONSULT-35v-AXIOMS.md` first (LOCKED facts + a BANNED
falsified framing — honor both).

**Your single question:** Is there a documented or binary-evidenced DIFFERENCE between Miles **9.3b**
and **9.3v** in HOW A STREAM (`AIL_open_stream`) is FED at an IDLE / low-I/O application state? I.e.
could a 9.3b→9.3v point-release change plausibly explain why our streamed title music stays silent at
the login screen but plays once heavy zone-load I/O begins?

Evidence sources (read, cite file:line / exact strings):
1. Decompiled Miles 9.3b help: `D:\Code\swg-client-v2\.planning\research\miles-chm\` — find the pages on
   `AIL_open_stream`, `AIL_service_stream`, `AIL_serve`, auto-servicing, the stream "secondary thread",
   `AIL_set_file_callbacks`, and how/when the stream's read callback is driven. Quote what the docs say
   about WHO services a stream when the app is idle and whether a background thread does it automatically.
2. Binary diff of the two DLLs:
   - `D:\SWG Restoration\x64\mss64.dll` (9.3v) vs
   - `D:\Code\swg-client-v2\src\external\3rd\library\miles-9.3b\redist64\mss64.dll` (9.3b).
   Compare embedded version strings, build banners, any "stream"/"async"/"thread"/"serve" literals, and
   the export tables (already dumped to `CONSULT-35v-exports-9.3v.txt` / `-9.3b.txt`). Use whatever
   read-only tooling you have (strings, findstr, a hex scan). Surface any string/symbol that hints at a
   changed streaming/auto-service/threading code path.
3. If accessible: any RAD/Miles changelog text already on disk under `.planning/research/` or the chm.

Report: (a) what the docs say drives an idle stream, (b) any concrete 9.3v-vs-9.3b binary delta touching
streaming/threading, (c) a calibrated verdict — does the EVIDENCE support "9.3v changes idle stream
servicing" or is it neutral/against. Cite everything. Do not speculate beyond the evidence.
