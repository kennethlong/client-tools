// ======================================================================
//
// utinni_engine_hookpoints.inc -- canonical X-macro name list for the
// Utinni engine entry-point advertisement contract. SHARED VERBATIM with
// D:/Code/Utinni (re-copied at each catalog wave so the two repos cannot
// drift). This file is the single source of truth for the set of advertised
// endpoint NAMES.
//
// Format: UTINNI_HOOKPOINT(group, name) -- the contract name is "group::name".
// The includer #defines UTINNI_HOOKPOINT before #include-ing this file and
// #undef's it after, expanding it to (a) the table rows on the provider side,
// (b) the required-name set + the coverage gate on the consumer side, and
// (c) the s_requiredNames[] / count self-check in utinni_advertise.cpp.
//
// 37-01 spike: the 3 representative rows below prove the full mechanism --
// a crash-fixer thunk (config), a static fn (game), and a non-virtual member
// PMF (commandParser). 37-02/37-03 EXTEND this file with the MVP / full
// catalog. The table BODY in utinni_advertise.cpp stays hand-written (rows
// are heterogeneous: static fn / bit_cast PMF / ctor thunk / accessor) and
// the coverage self-check bridges this name list to that body.
//
// See .planning/handoff/2026-06-20-utinni-engine-entrypoint-advertisement-spec.md.
// ======================================================================

UTINNI_HOOKPOINT(config,        loadOverrideConfig)
UTINNI_HOOKPOINT(game,          install)
UTINNI_HOOKPOINT(commandParser, addSubCommand)
