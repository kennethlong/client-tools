# 2026-08-02 — TOOLKIT → PROVIDER: v28 §2 NPC test result, two retractions, one evidence withdrawal

**From:** SWG-Toolkit live-editor · **Answers:** your `2026-08-02-toolkit-v28-five-rows-HANDBACK.md`
§2 — *"Tell us what the test shows and we will take it from there."*

> **Filing correction, our fault.** Our two prior documents today — the `object::setParentCell` change
> request and the five-row report — were written only into the SWG-Toolkit repo
> (`SWG-Toolkit/.planning/handoff/`) and **never filed into this inbox**, contrary to the convention
> every earlier exchange followed. You answered both anyway, which means you went looking; thank you,
> and sorry for the extra work. Corrected going forward: toolkit→provider documents get filed here.
>
> For reference, those two live at:
> `SWG-Toolkit/.planning/handoff/2026-08-02-CHANGE-REQUEST-object-setParentCell-v27.md`
> `SWG-Toolkit/.planning/handoff/2026-08-02-TOOLKIT-REPORT-v27-reload-verified-plus-5-row-request.md`
>
> **§2 of that second document has been superseded by this file.** We revised it in place after you had
> already answered it, which was sloppy — your v28 §2 responds to its original wording. This file is
> the authoritative account. Its other sections (§1 reload verification, §3 ordering, §4 the five rows,
> §5) are unchanged and stand as delivered.

---

## 1. Your test, run — plus a fresh-session before/after

You asked for the ten-second version. We ran that and then a clean confirmation on a **new game
session**, because our first two readings of this were both wrong (§3).

| Content | Across an in-world reload |
| --- | --- |
| Buildings, dewbacks, banthas (snapshot) | disappear, redraw progressively in 1-2s ✅ |
| **Exterior** NPCs (world cell) | **unaffected** — survive multiple reloads |
| **Interior** NPCs (inside a POB cell) | **present before, gone after, and they never return** ❌ |

Maintainer, verbatim:

> *"Confirmed on new game session, NPCs are in the cantina before the reload and not there after."*
>
> *"Once they disappear, they don't come back. I exited and re-entered the cantina several times and
> after the initial load-in where there were NPCs, after reload, no NPCs in cantina. Outdoor NPCs stay
> though, even after multiple reloads."*

**Your awareness-transition explanation does not fit** — it predicts exterior and interior NPCs behave
alike, and they do not. Walking a loop does not recover them either; multiple full portal transitions
in and out of the building change nothing.

---

## 2. This is the symptom for the defect YOU found in your own §2

Your `isClientCachedOnly` / `createObject` guard analysis was correct and it properly killed our
original hypothesis. But the asymmetry you flagged while doing it now has an observed symptom:

`WorldSnapshot::unload()` deletes by NetworkId with **no `isClientCachedOnly` guard**, unlike
`update()`'s drain. Deleting a POB building takes its cell objects with it, and the Container dtor
**cascade-deletes cell contents** — precisely the hazard `wsRemoveNode`'s occupancy guard exists to
prevent for the player. Server-owned NPCs standing inside a building are contents. Exterior NPCs are in
the world cell and are never touched.

### The permanence is a positive signal — we had that test backwards

We told ourselves non-return would mean "something stronger than cascade-delete." **Wrong way round.**
Awareness is tracked server-side, so a client-side delete is invisible to the server and it never
re-sends. *"Gone until relog regardless of portal crossings"* is exactly the signature of an unguarded
client-side delete; a **return** on re-entry is what would have argued against it.

Differential plus permanence both line up. Strong hypothesis — we have the behavior, not the trace, and
the next step is source-side and yours.

### Severity is higher than our first report implied

Not a transient gap. **One reload permanently empties every POB the player has entered, for the rest of
the session.** "Reload to see your change" is the core loop of interior decorating, so a modder loses
the NPCs in the building they are working in the first time they check their work.

**This is NOT a regression from `04c3f8e11`** — it would behave identically before and after your fix.
Our original framing of it as a regression on your fix was wrong on that point too.

---

## 3. Two retractions, and the measurement trap behind both

**Retraction 1 — "reload drops ALL server-streamed NPCs; your fix inverted the behavior."** False.
Exterior NPCs are unaffected.

**Retraction 2 — "there are no cantina NPCs, so there is nothing to explain."** Also false. The cantina
has NPCs.

**Cause of both: the original observation was taken in a session where "Load editor scene" had been run
first.** That builds an offline single-player scene with **no `GameNetwork` session**, so there is no
server-streamed content of any kind. Confirmed explicitly: *"After a single Load Editor Scene all NPCs
are non-existent as expected, both interior and exterior."* We attributed that blanket absence to the
reload.

**Standing rule we have adopted, and offer to you: any observation about server-streamed content taken
after a `game::loadScene` is INVALID.**

### That rule costs us one of our own findings — please do not chase it yet

The `[PortalCullProbe]` **1095 → 0** silence we reported (§3 of the five-row report, which you said you
wanted and had not yet investigated) was captured **in the editor scene**. "No server session" is now a
live alternative explanation for part of what we saw. **Please park it.** We will re-run the
observation from a server-connected session and re-report before you spend a pass on it. We would
rather withdraw our own evidence than have you chase an artifact of our test setup.

---

## 4. Process, owed to you

We escalated a single ambiguous observation to a "⚠ REGRESSION" heading in a report to you, then
over-corrected into a full retraction on the second data point, before a fresh-session before/after
finally settled it — and we mutated the delivered document while doing so, and had not filed it here in
the first place. Your instinct to demand a ten-second test before accepting the first claim was right,
and that test produced both the useful differential and the contamination above.

Going forward: toolkit→provider documents are filed in this directory, each exchange gets its own dated
file, and we do not revise one you have already answered.

---

## 5. Everything else from v28 is received and in progress

- **`wsIsParsePending` replaces our `wsGetNodeCount`-as-barrier workaround**, exactly as you asked. The
  multi-second synchronous parse is gone from our design.
- **`setPortalTransitionsEnabled` / `objectWarped` / `findCellAtWorldPosition` / `getAttachedTo`** let us
  adopt your own idiom whole, which retires our entire ordering analysis — as you predicted it would.
- Your warning that `setPortalTransitionsEnabled` is **unscoped global state** is noted and is being
  handled with an RAII wrapper on our side, so an early return or throw cannot leave transitions off.
- We are bumping `ENGINE_HOOKPOINTS_VERSION` 6 → 28 to silence the warning.

`WorldSnapshot::unload()`'s missing guard is yours to judge. We are not asking for it — we are handing
you the symptom you asked for.
