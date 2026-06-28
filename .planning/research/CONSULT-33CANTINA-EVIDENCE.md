# CONSULT-33CANTINA — x64 cantina-transition crash (NEUTRAL EVIDENCE — treat as LOCKED facts)

Do NOT re-derive these; find the root cause from your assigned angle. This is the FIRST x64 build of
this SWG client (32-bit is the working reference). The x64 client now boots + plays; this crash hit
while the player ran in and out of the Mos Eisley cantina (cell/zone transition with heavy object
load/unload). Build: x64 Debug.

## Measured facts (LOCKED — from the automated crash dump)
- Exception code **0xe06d7363** = a thrown C++ exception (MSVC `throw`), UNHANDLED -> crash. NOT an
  access violation (those are 0xc0000005). So some code executed `throw` and nothing caught it.
- No minidump (.mdmp) was written (the in-proc minidump path is known-broken), so there is NO stack
  trace available from this crash. You must reason from code + the context below.
- Crash-context fields captured at the moment of death:
  - `ObjectTemplate_Constructor: object/mobile/shared_dressed_commoner_naboo_human_female_02.iff`
    (an NPC commoner was being CONSTRUCTED)
  - `~Object: template=object/static/structure/tatooine/shared_debris_tatt_drum_dented_1.iff`
    (a static debris object was being DESTRUCTED)
  - `SkeletalMeshGeneratorTemplate: appearance/mesh/shirt_s26_m_l1.mgn` (a wearable shirt .mgn was loading)
  - `ShaderTemplate_Iff: shader/shirt_s26_hcs16.sht`
  - `SkeletalAnimationTemplate: appearance/animation/hum_m_face_squint.ans`
  - `Terrain: terrain/tatooine.trn`, Player near the Mos Eisley cantina.
  - `BytesAllocated: ~565 MB` (healthy on x64 -- this is NOT an out-of-memory death).
  - `UpTime: 76` seconds, `MainLoop: 4558`.
- This is a cell/zone TRANSITION moment: objects (NPC + its wearable .mgn) being created while others
  (debris) are destroyed -- i.e. concurrent/async asset load + teardown.

## Candidate context (do NOT assume; one of several angles)
- The codebase has a documented intermittent crash class: "async armor/belt `.mgn` load race; retry
  usually succeeds" on Tatooine. This MIGHT be the same intermittent async-`.mgn`-load race (pre-existing,
  not x64-specific), OR a genuine x64 regression. Resolve which.
- The codebase also has a documented "erase-then-++ map-iterator UB" crash class (STL container walk).

## The question
What `throw` site fires 0xe06d7363 during async skeletal-mesh (`.mgn`) load + object teardown on a cell
transition? Is it x64-specific (a real regression) or the pre-existing intermittent async-load race?
Identify the throwing code path (file:line of the `throw` and what triggers it) and the minimal fix or
the proof it is the benign intermittent race.

Relevant trees: `src/engine/client/library/clientSkeletalAnimation/` (SkeletalMeshGenerator/.mgn load),
`src/engine/shared/library/sharedObject/` (Object/container teardown), the async loader
(`sharedFile`/`clientGame` async/loader), `src/engine/client/library/clientGame/` (CellObject/PortalProperty
cell transition).
