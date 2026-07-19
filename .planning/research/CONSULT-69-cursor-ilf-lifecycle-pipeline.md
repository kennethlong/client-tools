You are reading a from-source SWG NGE client (repo root = current directory; engine at
src/engine, game at src/game). Read .planning/research/CONSULT-69-ilf-object-identity-EVIDENCE.md
first — treat its contents as GIVEN.

TASK (precise file:line code trace — you are the detail reader; no speculation, only what
the code does):

PART A — interior-layout object LIFECYCLE:
1. Trace the full lifecycle of a ClientInteriorLayoutManager-created object: creation
   (ClientInteriorLayoutManager.cpp), ownership (addClientOnlyInteriorLayoutObject — where
   does TangibleObject store these, who iterates the list), and DESTRUCTION — every path
   that deletes these objects (cell unload/removeFromWorld, building despawn, zone change,
   TangibleObject dtor). Exact file:line for each delete site.
2. From that: what is the safe lifetime window for a raw Object* to one of these held by an
   in-process consumer that re-reads it every frame on the game thread? What events
   invalidate it mid-session WITHOUT a zone change (LOD? detail level change? cell
   visibility flicker? the resume-cursor throttling)?
3. Is there any existing per-object identity (index, name, position key) stable across a
   destroy/respawn cycle of the same cell?

PART B — the id-less SELECTION pipeline:
With CuiPreferences::allowTargetAnything == true, trace a mouse click on an
interior-layout chair (no NetworkId) end-to-end through SwgCuiHud (src/game/client/library/
swgClientUserInterface/src/shared/page/SwgCuiHud.cpp) and whatever it calls:
1. Per-frame hover: findAllTargettableObjects → testFindObject → m_lastSelectedObject
   assignment (~:1433-1441). Does the id-less chair ARRIVE in m_lastSelectedObject?
2. The CLICK/selection action: what does clicking do with m_lastSelectedObject /
   foundObject — where does it call setLookAtTarget / CuiRadialMenuManager / anything else,
   and at exactly which point does an id of 0 make the operation a no-op or early-out?
3. SwgCuiHud::getLastSelectedObject (the advertised read) — when is m_lastSelectedObject
   updated vs cleared; does it persist while the cursor moves off the object; is it cleared
   when the id-keyed target set fails?
DELIVER: the exact dead-end line(s) where id-less selection stops working, and a verdict:
can an in-process consumer get the chair's Object* today purely via
allowTargetAnything + getLastSelectedObject (yes/no + conditions)?
