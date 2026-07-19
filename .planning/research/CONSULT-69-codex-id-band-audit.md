You are auditing a from-source SWG NGE client (repo root = current directory; engine at
src/engine, game at src/game). Read .planning/research/CONSULT-69-ilf-object-identity-EVIDENCE.md
first — treat its contents as GIVEN.

TASK (repo call-graph audit, file:line evidence for every claim):

Question: if the client minted NetworkIds for client-only interior-layout objects from a
RESERVED band (candidate: negative int64 values; alternative: a high positive band above
int32), what would break?

Trace ALL client-side consumers of NetworkId VALUES and enumerate every place that assumes
sign, range, or int32-ness:

1. NetworkIdManager (registration, lookup, getAllObjects walkers) — any assumptions?
2. CachedNetworkId / Watcher resolution paths.
3. Uplink paths: what messages carry the lookAt/intended target id to the server in a live
   session (CreatureObject setLookAtTarget sync, controller messages, radial menu request
   messages, /target command)? What id values would leak upstream if a reserved-band id
   became the lookAt target?
4. The cluster-id masking in NetworkId (cms_clusterIdMask, getValueWithoutClusterId,
   getValueWithClusterId) — do negative or high-band values collide with the cluster-id bit
   packing? This is a KEY question — give the exact bit layout.
5. Any int32 truncation of NetworkId values (casts, printf %d, serialization) reachable from
   client-only objects.
6. The WorldSnapshot editor's id allocator seeding (WorldSnapshot.cpp, wsAllocateIdRange /
   wsConfigureIdAllocator area) — confirm reserved-band ids would be excluded or harmless.
7. ClientObject/TangibleObject code paths keyed on getNetworkId().isValid() that would change
   behavior if a formerly-invalid object suddenly had a valid id (auto-follow, look-at,
   examine, chat, datatables, object attributes manager fetching attributes from the SERVER
   by id, etc.). List each behavioral flip you can find.

DELIVER: a risk table (consumer, file:line, what it assumes, verdict SAFE/RISK/BLOCKER for
negative-band and for high-positive-band separately) + a one-paragraph overall verdict on
whether reserved-band id minting is viable, and which band is safer.
