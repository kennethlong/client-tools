# Phase 14: Voice Chat (Vivox) Source Removal - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-05-26
**Phase:** 14-voice-chat-vivox-source-removal
**Areas discussed:** Removal scope, Live-caller model, CuiPreferences keys, Vendored trees + UI data

---

## Removal scope

| Option | Description | Selected |
|--------|-------------|----------|
| Whole subsystem | Remove every Vivox/voice-chat file the researcher can enumerate (3 named + ActiveSpeakers/_TableModel, FlyBarMessageQueue, OptVoice, CommandParserVoice, VoiceChatGlue.h, public headers). Mirrors Phase 13 D-02. | ✓ |
| Named-3 strictly | Delete only the 3 files DECRUFT-05 names; leaves the rest referencing deleted symbols — will not build. | |

**User's choice:** Whole subsystem
**Notes:** Confirms the DECRUFT-05 named-3 is a subset; the named-3 alone is non-compiling. Researcher must re-grep to enumerate the full file list (scouted baseline ≠ guaranteed complete).

---

## Live-caller model

| Option | Description | Selected |
|--------|-------------|----------|
| Delete at each call site | Edit each surviving caller (CuiManagerManager lifecycle, CuiRadialMenuManager VOICE_INVITE, CommandCppFuncs voice funcs, ClientCommandQueue + CustomerServiceManager CS-report) to remove voice lines. No stub. Satisfies criterion #2 grep-zero. | ✓ |
| No-op CuiVoiceChatManager stub | Keep an empty inline no-op class so callers compile unchanged (Phase 12 lcdui pattern). Symbol survives → fails criterion #2. | |

**User's choice:** Delete at each call site
**Notes:** Extends Phase 13's closed-chain delete to *live* callers. Verified during scout: callers live in surfaces that stay (UI manager, radial menu, command tables, CS report), so each call site is edited in place rather than the whole chain deleted.

---

## CuiPreferences keys

| Option | Description | Selected |
|--------|-------------|----------|
| Full strip | Remove all 6 ms_voice* bools, REGISTER_OPTION_USER lines, getters/setters, the CuiVoiceChatManager::setVoice*(false) default-disable block, and the include. | ✓ |
| Keep deprecated no-op getters | Retain getVoiceChatEnabled()-style getters returning false for safety. Unnecessary + leaves 'Voice' symbols → fails criteria #1/#3. | |

**User's choice:** Full strip
**Notes:** Pre-verified safe — grep found zero non-voice callers of the voice getters/setters; REGISTER_OPTION_USER → CurrentUserOptionManager, which silently ignores unregistered persisted keys, so stale options.cfg lines are inert. Researcher confirms unregistered-key tolerance; boot gate is the backstop.

---

## Vendored trees + UI data — Q1 (trees)

| Option | Description | Selected |
|--------|-------------|----------|
| Delete trees + purge dangling | Delete external/3rd/library/vivox/ + vivoxSharedWrapper/ trees (mirror Phase 13 D-03); purge absent-on-disk swgClientVivox include refs + 8 editor .rsp vivox refs (D-04 carry-forward). | ✓ |
| Unlink only, keep trees | Remove build refs but leave vendored dirs on disk; vendored headers contain 'vivox' → fails criterion #1 grep-zero. | |

**User's choice:** Delete trees + purge dangling

---

## Vendored trees + UI data — Q2 (UI / data side)

| Option | Description | Selected |
|--------|-------------|----------|
| Source registrations only | Delete source-side hooks (mediator factory, command-table funcs, radial enum); treat TRE .ui/datatable assets as out-of-reach inert data. | |
| Also chase datatable/.ui data | Attempt to scrub voice rows from command/radial datatables and .ui page defs. | ✓ |

**User's choice:** Also chase datatable/.ui data → **refined after follow-up**
**Notes:** User overrode the recommendation to dismiss the data side as out-of-reach. Follow-up scout found there is **no `dsrc/`/`data/` datatable tree in this repo** — so "chase the data" reconciles to chasing **in-repo source-level registrations**: `CommandTable::addCppFunction("voiceInvite"/"voiceKick")`, `CuiMenuInfoTypes` VOICE_INVITE enum + `MAKE_ID(voice_invite)`, and the `SwgCuiMediatorFactorySetup` ctor lines. User confirmed this refined scope ("Chase all in-repo registrations"). The compiled `.ui` pages + command/radial datatable rows live in the retail TRE install (outside this repo, not redistributed) and are accepted as inert — the engine no-ops on orphaned asset rows.

---

## Claude's Discretion

- Exact edit order, plan/wave breakdown, and commit granularity within the removal — provided every coherent step leaves the tree buildable and the link gate green. Phase 13's wave shape is a reasonable template.

## Deferred Ideas

- In-Game Browser (XPCOM/Mozilla) removal — Phase 15 (DECRUFT-06).
- Bink video codec / Miles audio playback — explicitly OUT of v2.1 scope (active middleware); do not conflate with voice removal.
- Reviewed-not-folded todos: "SWGSource vs whitengold TRE space-scene diff" (v2.2 visual work) and "Cantina corner-snap engine quirk" (rendering, pre-existing) — both out of voice-removal scope.
