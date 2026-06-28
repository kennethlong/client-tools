# TASK (Opus — ABI/runtime spec reasoning)

Read `CONSULT-35v-AXIOMS.md` first (LOCKED facts + a BANNED falsified framing — honor both).

**Your single question:** Given that a name-bound runtime swap to 9.3v resolves all 61 of our imports
(L3), what is the most likely root cause of the **c0000005 in `RIB_register_interface` during
`AIL_startup`** (L5) when our exe loads a clean 9.3v core + 9.3v providers — given that Restoration's
OWN exe loads the identical 9.3v DLL set without crashing?

Produce a RANKED list of candidate root causes, each with (a) the mechanism, (b) why it would hit our
exe but not Restoration's, (c) a cheap discriminating test to confirm/refute. Consider at least:
- Core↔provider RIB-version skew (a 9.3b provider leaking into `stage-x64\miles\` during the swap; an
  extra/missing provider; `binkawin64.asi` version mismatch).
- Our exe being x64 **Debug** (debug CRT / heap / iterator-debug) vs Restoration **Release** — does Miles
  RIB registration touch anything CRT/heap-sensitive at startup?
- `msvcr71.dll` — Restoration ships it next to both exes; does Miles 9.3v depend on a specific CRT that
  must be co-resident? (Restoration ships msvcr71 in x64\ too.)
- Provider load ORDER / a provider that fails to self-register and returns a bad interface pointer that
  `RIB_register_interface` then dereferences.
- Whether `AIL_set_redist_directory("miles")` (relative) resolves differently for our cwd vs Restoration's.

End with: the cleanest VALIDATED adoption path if 9.3v is the right fix — i.e. exactly what to stage and
in what order to make our Debug exe load 9.3v without the RIB crash, and what to check at each step.
Concrete, testable steps only. No relinking/import-lib proposals (banned).
