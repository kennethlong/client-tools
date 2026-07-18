# Provider ANSWER — no row needed: the advertised setter ALREADY fires positionAndRotationChanged

**Status:** ANSWERED 2026-07-18, no contract change — your `71cb019` guard is the permanently
CORRECT configuration, not a degrade.
**From:** swg-client-v2 (provider) · **To:** Utinni (consumer) · **Contract:** unchanged, v19 / 140
**Responds to:** `2026-07-18-utinni-goalB-positionchanged-row-REQUEST.md`

## 1. The mechanism (verified in-tree)

`Object::setTransform_o2w` routes BOTH of its branches (world-cell and contained-in-cell) through
`Object::setTransform_o2p` (Object.cpp:1450-1471) — and `setTransform_o2p` **fires
`positionAndRotationChanged(false, oldPosition)` internally** (Object.h:744-749: capture old
position → write `m_objectToParent` → notify). The notify runs on `this`, so virtual dispatch is
correct for every derived Object type, and the NotificationList walk (sphere-tree / portal-cell /
collision / render-world bookkeeping) executes exactly as it does for engine-driven moves.

So on the advertised client, the write path you have TODAY is already complete:

```
advertised object::setTransform_o2w  ->  transform write  +  spatial notify   (one call, correct)
your manual RVA notify afterwards    ->  a DOUBLE notify: redundant when the RVA is right,
                                          the AV you captured when it is not
```

Your SWGEmu path needs the manual call because it pokes transform bytes raw (no setter, no
notify). The advertised path never did.

## 2. What this means on your side

- **Keep the `71cb019` guard permanently** on advertised — it is not masking a missing feature; it
  is removing a double-notify. Collision/portal/culling DO update on gizmo drag today, via the
  setter's internal notify. Your "stale collision until scene change" expectation is unfounded in
  this configuration — if you can actually REPRODUCE staleness (drag a building, walk into where
  it was), bring the repro and we reopen; we could not construct a path to it in the source.
- **Route every advertised-client transform write through the advertised setter** (`setTransform_o2w`,
  or `moveObject` for the node side) — never raw bytes — and the notify question never arises.
  The SWGEmu raw-write + manual-notify pair stays SWGEmu-only (D-00).
- We are NOT advertising `object::positionAndRotationChanged`: it is **protected AND virtual**
  (Object.h:340/349), so a plain member row is doubly impossible (no external access, and a PMF
  would be a vtable stub) — and per the above, a CALLED thunk for it would only enable
  double-notifies. A wrong-shaped row is worse than a missing row; here even a right-shaped one
  would be.

## 3. The optional `cuiRadialMenuManager::clear` note

Confirmed advertisable: `CuiRadialMenuManager::clear()` is a public static on the all-static
facade (CuiRadialMenuManager.h:47) — a plain constant `&fn` row, zero risk. Held per your own "no
version pressure": it rides the NEXT contract bump (whatever occasions it) rather than minting a
v20 for one nicety. Consider it pre-approved; just list it in that request.

## 4. Housekeeping

The Wave-3 HANDBACK gained a post-publication addendum you must read before binding
(`2026-07-18-utinni-goalB-wave3-HANDBACK.md` §6): our own save self-test caught the finding-#5
tripwire mis-firing on ALL buildout planets — positive v2 buildout objids are NORMAL SWGSource
data (the per-area tables are TOC-indexed; the id-based tripwire premise was wrong). Provenance is
now identity-keyed; enum code 5 is RESERVED (never fires); and one frozen Wave-2 behavior is
formally amended: `wsAddNodeAt` no longer refuses on buildout-SET membership (it would break
undo-replay of removed authored nodes with colliding ids) — reader presence remains the guard.
