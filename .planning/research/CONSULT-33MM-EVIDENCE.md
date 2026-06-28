# CONSULT-33MM — x64 MemoryManager boot crash (NEUTRAL EVIDENCE — treat as LOCKED facts)

Do NOT re-derive these; they are measured ground-truth. Find the root cause of the crash from
your assigned angle. File: `src/engine/shared/library/sharedMemoryManager/src/shared/MemoryManager.cpp`.

## Measured facts (LOCKED)
- The 32-bit (x86) build of this client boots fine. This is the FIRST x64 build. Bug is x64-only.
- Crash: `c0000005` AV on the FIRST `MemoryManager::free` during C++ static init (a
  `std::vector<void*>::push_back` reallocation freeing its old buffer -> `operator delete`
  (OsNewDel.cpp:76) -> `MemoryManager::free`).
- Faulting instruction (Release build): `test byte ptr [rbx+10h],1` = `Block::isFree()` reading
  `m_free` at offset 0x10. **rbx = 0x0 (NULL).** The crash is `block->getPrevious()->isFree()` at
  MemoryManager.cpp:1557 — i.e. the block being freed has `m_previous == NULL`. Only the prefix
  sentinel block legitimately has `m_previous == NULL`.
- cdb-measured x64 type sizes: `sizeof(Block) == sizeof(AllocatedBlock) == 0x18 (24)`. Block layout
  x64: `m_previous`@0, `m_next`@8, `m_free` bitfield @0x10.
- Constants (MemoryManager.cpp:209-215):
  - `cms_blockSize = (sizeof(Block)+15)&~15`            -> x64 = 32
  - `cms_allocatedBlockSize = (sizeof(AllocatedBlock)+15)&~15` -> x64 = 32
  - `cms_freeBlockSize = (sizeof(FreeBlock)+15)&~15`    -> x64 = ?? (FreeBlock : public Block adds
    4 pointers: m_smallerFreeBlock/m_sameFreeBlock/m_largerFreeBlock/m_parentFreeBlock)
  - `cms_guardBandSize = DO_GUARDS ? 16 : 0` (Release: DO_GUARDS off -> 0)
- These code regions read as correct/platform-agnostic (verified): the `SystemAllocation` ctor
  block linking (:313-331), the allocate/split linking (:1251-1262), the Block get/set Next/Previous
  accessors (no pointer truncation), the user-pointer round-trip (alloc `:1329` memory = best +
  cms_allocatedBlockSize + cms_guardBandSize; free `:1453` reverses it identically).
- Block-list (m_previous/m_next) is separate from the FreeBlock free-list TREE
  (m_smallerFreeBlock etc., managed by addToFreeList/removeFromFreeList/searchFreeList).

## The question
Why does an allocated-then-freed block have `m_previous == NULL` on x64 but not x86? Identify the
exact line(s) and the x64-vs-x86 divergence. Propose the minimal fix.
