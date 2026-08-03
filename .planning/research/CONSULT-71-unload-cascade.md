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
