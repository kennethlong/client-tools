---
created: 2026-05-14
title: SafeCast null dynamic_cast on world load — investigate with /gsd-debug
area: phase-9-followthrough
next_action: /gsd-debug
files:
  - D:/Code/swg-client-v2/src/engine/shared/library/sharedFoundation/src/shared/SafeCast.h
  - D:/Code/swg-client-v2/stage/SwgClient_d.exe (May 10 19:32 build)
  - D:/Code/swg-client-v2/stage/client_d.cfg
  - D:/Code/swg-client/src/engine/shared/library/sharedFoundation/src/shared/Fatal.h (NOT_NULL definition for reference)
---

## Symptom

Running `D:/Code/swg-client-v2/stage/SwgClient_d.exe` against the SWGSource VM
(`192.168.1.200:44453`) with character "Little Bigman (swg)" on Tatooine:

1. Reach character-select screen — character preview shows **naked character**
   (no clothing/armor visible — the loadout system silently produced nothing).
2. Click "Enter World" → CRT assert dialog fires:
   ```
   Assertion failed!
   File: SafeCast.h
   Line: 29
   Expression: (t) != nullptr
   ```
3. Click **Ignore**. Second identical assert dialog fires.
4. Click **Ignore** again. Client recovers and finishes world load — gameplay works.

Two distinct call sites are tripping `safe_cast` to return null. Both are
recoverable (downstream callers tolerate null); v1 (`D:/Code/swg-client/build-v145/bin/Debug/`)
does NOT pop this dialog with the same character + same TRE data + same VM.

## Root cause analysis (preliminary)

The difference is in `SafeCast.h` between the two trees — **Koogie's tree
patched the debug-mode null check**:

**v1 (`D:/Code/swg-client/...sharedFoundation/src/shared/SafeCast.h`):**
```cpp
T t = dynamic_cast<T>(u);
NOT_NULL(t);                 // expands to FATAL(!t, ...) in debug per Fatal.h:76
return t;
```

**v2 (Koogie patched the same file):**
```cpp
#include <assert.h>          // ← added
...
T t = dynamic_cast<T>(u);
assert((t) != nullptr);      // ← changed from NOT_NULL
return t;
```

Both are nominally fatal in debug. The behavior difference:
- v1's `FATAL` macro routes through SOE's debug system — its dialog has a
  `Continue` button that's easy to dismiss without noticing
- v2's CRT `assert` pops the standard Win32 Abort/Retry/Ignore dialog

So the **underlying null dynamic_cast almost certainly happens in v1 too**;
either the v1 SOE FATAL dialog has been getting silently dismissed, OR the
v1 TU compiles with `DEBUG_LEVEL == DEBUG_LEVEL_RELEASE` (line 18 of SafeCast.h)
which collapses `safe_cast` to `static_cast` with no check.

## Why this almost certainly is NOT

- **Not a race condition.** Deterministic — same character + same TRE data
  trips the same two call sites every launch.
- **Not the LOEQ IFF guard.** The `iff.exitChunk(true)` graceful-traversal
  guard at `CuiCharacterLoadoutManager.cpp:162` IS present in v2 (commit
  `460f4540d`, port-forward of whitengold `dd78832c4`). The LOEQ chunk
  parses without FATALing.
- **Not stale DLLs / DPVS work.** The staged `SwgClient_d.exe` is dated
  May 10 19:32 — BEFORE any of the Phase 10 Wave 0-3 DPVS commits landed.
  Same EXE that PASSed Tatooine zone-in on 2026-05-10 (evidence:
  `log-replan3-02.txt`, 7.2 MB, zero FATAL).
- **Not config drift.** `client_d.cfg` byte-identical between v1 and v2
  trees; same TRE paths (`D:/Code/SWGSource Client v3.0/`); same loadout
  source file.

## What this likely IS

A different SWGSource compat gap surfacing through a different code path —
something on the loadout/equipment template chain that returns the wrong
dynamic type for the client's `safe_cast`. The "naked character" tells us
the cast target is in the clothing/wearable/equip object hierarchy.
Candidates from CONTEXT.md D-18 "bisect-first" compat-guard list that were
NOT ported because Tatooine PASSed without them:
- ContrailData cast guard
- NebulaManagerClient null guard
- POB missing-files guard

Any of those (or a new one) could be returning a placeholder that
`dynamic_cast<TangibleObject*>` or similar can't downcast.

## Constraint — don't modify Koogie's SafeCast.h change

User preference: **do not edit Koogie's `assert(t != nullptr)` patch in
`SafeCast.h`.** Koogie added it deliberately as part of the
`MSVC-CPP20-Upgrade` branch — likely to surface latent type-mismatch bugs
that were silently swallowed by SOE's older FATAL macro. Reverting it
would lose the diagnostic value.

The fix should target the **real null-cast call sites**, not the assertion.

## Next action

Run `/gsd-debug` to:
1. Capture both call stacks (break on `SafeCast.h:29`, hit Ignore once to
   continue, capture the second stack too)
2. Identify the source/destination types of each failed dynamic_cast
3. Determine whether each is a real SWGSource compat gap (port a new guard)
   or a different class of bug (object initialization order, template
   wiring, etc.)
4. Cross-reference candidates against CONTEXT.md D-18 unported guards
   (ContrailData, NebulaManagerClient, POB)

## Workaround for now

Press **Ignore** twice on the dialogs. Client recovers and the rest of
gameplay works fine. The naked-character visual persists at character
select but the in-world character renders correctly (per May 10 minidump
evidence: armor_marauder_s01_belt_f.sat appearance loaded during the
deferred 11-min crash session).

## Additional signal — warning volume

User reports **35,194 warnings on world load** in v2 (2026-05-14). For
comparison, STATE.md "deferred items" notes the historical baseline as
"156k DEBUG_WARNINGs at exit (mostly per-frame nebula lightning)" — that
was an at-exit cumulative count, not world-load. The 35k figure at
world-load is a much earlier, more concentrated signal and could be
related to the null-cast issue (warnings often precede the dynamic_cast
that returns null — failed template loads, missing IFF chunks, missing
appearance files, etc. all log warnings).

When `/gsd-debug` runs, grep the warning stream for clusters:
- "appearance" / ".sat" / ".mgn" — character/clothing visuals
- "ObjectTemplate" / "shared_" — template chain failures
- "IFF" / "chunk" — SWGSource format mismatches
- "safe_cast" / "dynamic_cast" — direct hits if logged

If a single warning class dominates the 35k count, that's a strong lead
on the root cause regardless of the SafeCast call stacks.

## Related deferred long-tails (do not conflate)

These are tracked elsewhere — list them so a debug session doesn't get
sidetracked:
- ExceptionHandler crash after ~11 min of in-world play (separate
  `/gsd-debug` candidate, noted in STATE.md "deferred long-tail")
- First-launch login flakiness (back-out + retry login twice — not related)
- Pre-existing Koogie post-build MSB3073 (unrelated to runtime)
