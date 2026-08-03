# Evidence pack — treat all of this as GIVEN, do not re-derive

Codebase: D:\Code\swg-client-v2 (SWG client, C++, MSBuild).

## Established by source trace (facts, not hypotheses)

1. `WorldSnapshot::unload()` (clientGame/.../WorldSnapshot.cpp) iterates every snapshot node and does:
     Object* o = NetworkIdManager::getObjectById(NetworkId(node->getNetworkIdInt()));
     if (o) delete o;
   with NO guard of any kind.

2. `WorldSnapshot::update()`'s delete drain, by contrast, IS guarded:
     if (ContainerInterface::isClientCachedOnly(*safe_cast<ClientObject*>(object)))
         { object->removeFromWorld(); delete object; }

3. Deleting a POB building cascades:
   ~Object(building) -> "destroy children" loop: attached objects where isChildObject() are `delete`d
     -> cells are child objects, so ~Object(cell) runs
       -> "destroy all the properties" loop `delete p`s each Property
         -> `class CellProperty : public Container`
           -> ~Container iterates m_contents and `delete`s every contained Object.

4. OBSERVED LIVE (field report, treat as ground truth): an in-world snapshot reload
   (wsUnloadSnapshot + load) permanently removes NPCs that are INSIDE a POB. Exterior
   NPCs (world cell) are unaffected across many reloads. The interior NPCs never return,
   including across repeated portal transitions in and out of the building. Only a relog
   restores them.

5. These interior NPCs are SERVER-owned (server-streamed), not client-cached snapshot content.
   A client-side delete is invisible to the server, so the server never re-sends them.

6. A prior investigation in this codebase established that on a server session the PLAYER
   object is NOT linked into cell Container contents (the client tracks the player's cell via
   Object::getParentCell instead). A guard elsewhere therefore had to be made bidirectional:
   a downward contents walk PLUS an upward sweep for objects whose parentCell owner is in the
   subtree being deleted.

7. `WorldSnapshot::unload()` is called on BOTH a real scene/zone change AND an in-place
   editor "reload current scene". On a real zone change all server objects are torn down by
   the network path anyway; on an in-place reload the GameNetwork session persists.

## Your task

Answer ONLY the question in your assigned angle below. Be concrete, cite file:line where you can,
and say explicitly when you are uncertain. Do not propose a fix outside your angle.

## YOUR ANGLE (Cursor — detailed trace route, file:line). Answer ONLY these three routes.

Facts 1-7 above are LOCKED. Do not re-confirm them; I already have that. This is a read-only
tracing task, do NOT propose a fix.

### ROUTE A
How does a SERVER-STREAMED object end up inside a cell's `Container::m_contents` on the CLIENT?
Start at the network message that creates a streamed object carrying a containedBy cell reference,
and follow it step by step to the exact line that inserts into `m_contents` — or prove that nothing
ever does. Name every function and file:line on the path.

### ROUTE B
Trace the same for the PLAYER object on a server session. Fact 6 says the player is NOT linked into
cell contents, and the client instead tracks its cell via `Object::getParentCell`. Find the EXACT
divergence point between Route A and Route B: the specific branch, flag, or call that puts an NPC
into `m_contents` but not the player. **If Routes A and B do NOT actually diverge, say so plainly**
— that would contradict fact 6, and I would rather know.

### ROUTE C
Confirm or refute that a POB building's CELLS are reached by the "destroy children" loop in
`Object::~Object`. That loop only `delete`s attached objects where `isChildObject()` is true, and
merely DETACHES the rest with a WARNING_STRICT_FATAL. So: are cell objects attached to their
building with the child-object flag TRUE? Find where a POB's cells are created and attached
(file:line) and what that attach call passes for the asChildObject argument.

### BOTTOM LINE (one paragraph)
At the moment `WorldSnapshot::unload()` deletes a POB building on a LIVE SERVER SESSION, is
`m_contents` of that building's cells populated with server-owned object ids — YES, NO, or
UNPROVEN FROM SOURCE? Cite the lines that decide it.
