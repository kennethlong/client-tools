# TASK (Codex — binary forensics on the Restoration install)

Read `D:\Code\swg-client-v2\.planning\research\CONSULT-35v-AXIOMS.md` first (LOCKED facts + a BANNED
falsified framing — honor both). L2 already establishes Restoration STATICALLY imports mss64 by name.

**Your single question:** Beyond the static name import, what — if anything — does the Restoration x64
client do DIFFERENTLY around Miles streaming that we'd need to mirror to get idle login-screen music?

Investigate `D:\SWG Restoration` (and its `x64\` subdir). You may run read-only tools (dumpbin is at
`C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\dumpbin.exe`;
strings/findstr ok):
1. Does Restoration's x64 `SwgClient_r.exe` ALSO dynamically load Miles (any `LoadLibrary("mss64")` /
   `GetProcAddress` for `AIL_*`), or is it purely the static name import? (`dumpbin /imports` for
   kernel32 LoadLibrary/GetProcAddress is weak signal; look for `mss64` literal strings + AIL_* name
   strings in the exe that would feed GetProcAddress.)
2. Restoration's `x64\miles\` provider set: list it, get each file's build timestamp (dumpbin /headers
   on the .asi/.flt PE files → "time date stamp"). Are they a SELF-CONSISTENT 9.3v set (timestamps near
   the 2015-05-18 core), or mixed? This matters for L5 (a mixed set is the leading crash suspect).
3. Any Restoration config/registry/launch-arg that selects audio streaming mode, a redist dir, or a
   Miles preference (search `D:\SWG Restoration` for `.cfg`, `AIL_set_preference`-style tokens, "stream",
   "miles" in config files / the exe strings).
4. Does Restoration ship a Miles **import lib / SDK** anywhere in its tree (`*.lib`, `mss*.h`)? (We don't
   need one per L3, but if present it dates their Miles build.)

Report concrete findings with the command output that backs each. Flag anything that would explain why
Restoration's 9.3v streams at idle and ours (9.3b) does not. Do not propose relinking (banned).
