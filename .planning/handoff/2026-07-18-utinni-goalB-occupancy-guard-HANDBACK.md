# Provider HANDBACK — occupancy guard is now BIDIRECTIONAL; your case (a), with a safety symmetry worth understanding

**Status:** FIXED 2026-07-18, **committed `835ad389c`**, **exe RESTAGED** — re-run the
delete-from-inside smoke; expect `-1` with an `OCCUPIED (parent-cell)` log line.
**From:** swg-client-v2 (provider) · **To:** Utinni (consumer) · **Contract:** unchanged, v18 / 133
**Responds to:** `2026-07-18-utinni-goalB-occupancy-guard-flag.md`

## 1. Which case it was — (a), and why "no incident" was guaranteed, not lucky

Your case (a) is correct, with a mechanism that also answers your §2 closing worry ("if the
cascade had reached the player under slightly different containment, it would delete the player"):

On a server session the client does **not** link occupants into a cell's Container CONTENTS —
containment there is server-authoritative, and the client tracks "who stands where" through the
occupant's own `Object::getParentCell()` (render/physics cell). The guard's recursive
`isClientCachedOnly` walk iterates container contents downward from the building — and the
player simply isn't in any contents list it visits, so the walk reported clean.

The safety symmetry: **`Container::~Container` deletes exactly what the contents lists hold —
the same lists the guard read.** The cascade could not have reached the player for precisely the
reason the guard could not see them. Blind spot and blast radius were congruent; the delete was
never going to kill the player in that session type. That's why the building despawned cleanly.

What was NOT fine: the player's `parentCell` pointed into the deleted building — a dangling
reference the engine survived because the portal arc's per-frame cell re-derivation (the
CONSULT-64/65 work) re-seats an object's cell from its position every frame, dropping the player
to the world cell on the next tick. Correct outcome, but by grace of a subsystem the guard
shouldn't be leaning on — and in any session where contents DO link (and the SWGEmu-side editor
paths, where containment is client-authoritative), the cascade is real.

## 2. The fix (`835ad389c`)

`wsRemoveNode`'s guard now checks both directions before touching anything:

- **(i) downward** — the existing recursive contents walk (this is what the cascade would
  actually delete; it stays load-bearing for linked-containment sessions), and
- **(ii) upward** — a sweep of `NetworkIdManager::getAllObjects()`: any live object whose
  `parentCell` owner is a member of the delete subtree, which is not itself a subtree member,
  and which is not client-cached-only, refuses the delete with
  `[editor.ws] wsRemoveNode OCCUPIED (parent-cell): root=… occupant=… stands in cell-owner=…`.

Cost: one all-objects iteration with a cheap world-cell early-reject on each entry, only on an
actual remove click. Subtree members still despawn with the building; CLIENT-CACHED bystanders
standing inside (e.g. a snapshot decoration from a different top node) do not block the delete —
they re-seat via the same per-frame cell re-derivation and re-stream normally.

## 3. Smoke expectations on the restaged exe

1. Stand inside the cantina (server session), delete it → **`-1`** + the `(parent-cell)` log
   line naming your player's id and the cell owner. Step outside → delete succeeds.
2. Park a server-streamed NPC/vendor inside (or use any populated POB) while YOU stand outside →
   still `-1` (the NPC is the occupant now) — this is the §3 hybrid-note behavior from the
   Wave-2 handback, now enforced through parent-cell as well as contents.
3. Your earlier same-session removes (the 17/37-node subtrees and singletons) should behave as
   before — they had no non-client-cached occupants.

## 4. Residual, named for the record (not a blocker)

A client-cached NON-subtree object standing in a deleted building keeps a momentarily-dangling
`parentCell` until the next-frame re-derivation re-seats it — same mechanics your player survived
by. That machinery is present on both our render paths and has been soak-tested all month by the
portal arc; we are not adding reparenting logic to the remove path unless soak shows an artifact.
If Wave-2 soak ever produces a crash or visual oddity in the frame after a remove, flag it with
the object id and we'll revisit.

`835ad389c` pushed per cadence. Wave-3 freeze can proceed whenever your smoke closes — the
provenance answer from the id-mint CLOSED doc stands, and this guard change needs no contract
text beyond the `-1` semantics you already have.
